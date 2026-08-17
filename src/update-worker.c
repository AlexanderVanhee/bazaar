/* update-worker.c
 *
 * Copyright 2026 Alexander Vanhee
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "BAZAAR::UPDATE-WORKER"

#include "config.h"

#include <flatpak/flatpak.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "bz-update-history-data-point.h"
#include "update-worker.h"

#define DAILY_WINDOW_START_HOUR    6
#define DAILY_WINDOW_SPREAD_HOURS  6
#define STALE_CHECK_THRESHOLD_DAYS 7
#define STALE_NOTIFY_THROTTLE_DAYS 1

static gboolean network_is_restricted (void);
static gboolean power_saver_is_enabled (void);
static gboolean due_for_daily_check (GSettings *settings);
static gboolean timestamp_more_than_days_ago (GSettings  *settings,
                                              const char *key,
                                              guint       days);
static void     maybe_notify_stale (GSettings *settings);
static gboolean should_skip_extension_ref (FlatpakInstalledRef *iref);
static gboolean on_operation_error (FlatpakTransaction          *transaction,
                                    FlatpakTransactionOperation *operation,
                                    GError                      *error,
                                    int                          details,
                                    gpointer                     user_data);
static gboolean update_installation (FlatpakInstallation *installation,
                                     GPtrArray           *history_out);
static void     send_portal_notification (const char *id,
                                          const char *title,
                                          const char *body,
                                          const char *action,
                                          GVariant   *action_target);
static void     send_update_notification (GPtrArray *history);

int
run_update_worker (int   argc,
                   char *argv[])
{
  g_autoptr (GSettings) settings              = NULL;
  g_autoptr (GError) local_error              = NULL;
  g_autoptr (FlatpakInstallation) system_inst = NULL;
  g_autoptr (GPtrArray) history               = NULL;
  gboolean auto_update                        = FALSE;
  gboolean auto_update_notifications          = FALSE;
  gboolean had_error                          = FALSE;
  g_autoptr (GDateTime) now                   = NULL;

  settings                  = g_settings_new (APPLICATION_ID);
  auto_update               = g_settings_get_boolean (settings, "auto-update");
  auto_update_notifications = g_settings_get_boolean (settings, "auto-update-notifications");

  if (!auto_update)
    {
      g_debug ("Skipping update check: auto-update is disabled\n");
      return EXIT_SUCCESS;
    }

  if (!due_for_daily_check (settings))
    return EXIT_SUCCESS;

  if (network_is_restricted ())
    {
      g_debug ("Skipping update check: no network or network is metered\n");
      maybe_notify_stale (settings);
      return EXIT_SUCCESS;
    }

  if (power_saver_is_enabled ())
    {
      g_debug ("Skipping update check: power saver is enabled\n");
      maybe_notify_stale (settings);
      return EXIT_SUCCESS;
    }

  history = g_ptr_array_new_with_free_func (g_object_unref);

  system_inst = flatpak_installation_new_system (NULL, &local_error);
  if (system_inst == NULL)
    {
      g_warning ("Failed to open system installation: %s", local_error->message);
      g_clear_error (&local_error);
      had_error = TRUE;
    }
  else
    {
      flatpak_installation_set_no_interaction (system_inst, TRUE);
      if (!update_installation (system_inst, history))
        had_error = TRUE;
    }

#ifndef SANDBOXED_LIBFLATPAK
  {
    g_autoptr (FlatpakInstallation) user_inst = NULL;

    user_inst = flatpak_installation_new_user (NULL, &local_error);
    if (user_inst == NULL)
      {
        g_warning ("Failed to open user installation: %s", local_error->message);
        g_clear_error (&local_error);
        had_error = TRUE;
      }
    else
      {
        flatpak_installation_set_no_interaction (user_inst, TRUE);
        if (!update_installation (user_inst, history))
          had_error = TRUE;
      }
  }
#endif

  if (had_error)
    {
      g_debug ("Update process encountered errors\n");
      maybe_notify_stale (settings);
      return EXIT_SUCCESS;
    }

  now = g_date_time_new_now_local ();
  g_settings_set_int64 (settings, "last-update-check", g_date_time_to_unix (now));

  if (history->len == 0)
    {
      g_print ("No apps needed auto updating\n");
      return EXIT_SUCCESS;
    }

  if (auto_update_notifications)
    send_update_notification (history);

  return EXIT_SUCCESS;
}

/*
 * Updates try to run once per day at a randomized hour between 6:00 and
 * 11:00 local time, to avoid every client updating at once. If more than a
 * day has passed since the last check then the randomization is skipped and
 * the check runs as soon as it's past 6:00.
 *
 * This logic is copied from GNOME Software.
 */
static gboolean
due_for_daily_check (GSettings *settings)
{
  gint64 last_check_unix        = 0;
  gint   hour_offset            = 0;
  g_autoptr (GDateTime) now     = NULL;
  g_autoptr (GDateTime) now_mid = NULL;
  gint      now_hour            = 0;
  gint      year = 0, month = 0, day = 0;
  GTimeSpan day_interval = 0;

  last_check_unix = g_settings_get_int64 (settings, "last-update-check");
  hour_offset     = g_settings_get_int (settings, "update-check-hour-offset");

  if (hour_offset < 0)
    {
      hour_offset = g_random_int_range (0, DAILY_WINDOW_SPREAD_HOURS);
      g_settings_set_int (settings, "update-check-hour-offset", hour_offset);
    }

  now = g_date_time_new_now_local ();

  if (last_check_unix == 0)
    return TRUE;

  {
    g_autoptr (GDateTime) last_check     = NULL;
    g_autoptr (GDateTime) last_check_mid = NULL;

    last_check = g_date_time_new_from_unix_local (last_check_unix);
    if (last_check == NULL)
      return TRUE;

    g_date_time_get_ymd (last_check, &year, &month, &day);
    last_check_mid = g_date_time_new_local (year, month, day, 0, 0, 0);

    g_date_time_get_ymd (now, &year, &month, &day);
    now_mid = g_date_time_new_local (year, month, day, 0, 0, 0);

    day_interval = g_date_time_difference (now_mid, last_check_mid);
  }

  if (day_interval < G_TIME_SPAN_DAY)
    {
      g_debug ("Skipping update check: already checked today\n");
      return FALSE;
    }

  now_hour = g_date_time_get_hour (now);

  if (day_interval < 2 * G_TIME_SPAN_DAY &&
      now_hour < DAILY_WINDOW_START_HOUR + hour_offset)
    {
      g_debug ("Skipping update check: too early in the day\n");
      return FALSE;
    }

  if (day_interval >= 2 * G_TIME_SPAN_DAY &&
      now_hour < DAILY_WINDOW_START_HOUR)
    {
      g_debug ("Skipping update check: too early in the day\n");
      return FALSE;
    }

  return TRUE;
}

static gboolean
timestamp_more_than_days_ago (GSettings  *settings,
                              const char *key,
                              guint       days)
{
  gint64 ts                  = 0;
  g_autoptr (GDateTime) then = NULL;
  g_autoptr (GDateTime) now  = NULL;

  ts = g_settings_get_int64 (settings, key);

  if (ts == 0)
    return TRUE;

  then = g_date_time_new_from_unix_local (ts);
  if (then == NULL)
    return TRUE;

  now = g_date_time_new_now_local ();

  return g_date_time_difference (now, then) / G_TIME_SPAN_DAY >= days;
}

static void
maybe_notify_stale (GSettings *settings)
{
  if (!timestamp_more_than_days_ago (settings, "last-update-check", STALE_CHECK_THRESHOLD_DAYS))
    return;

  if (!timestamp_more_than_days_ago (settings, "last-stale-notification", STALE_NOTIFY_THROTTLE_DAYS))
    return;

  send_portal_notification ("bazaar-update-stale",
                            _ ("Updates Are Out of Date"),
                            _ ("Please check for available updates"),
                            NULL, NULL);

  g_settings_set_int64 (settings, "last-stale-notification", g_date_time_to_unix (g_date_time_new_now_local ()));
}

static gboolean
network_is_restricted (void)
{
  GNetworkMonitor *monitor = NULL;

  monitor = g_network_monitor_get_default ();
  if (monitor == NULL)
    return FALSE;

  return g_network_monitor_get_network_metered (monitor);
}

static gboolean
power_saver_is_enabled (void)
{
  g_autoptr (GPowerProfileMonitor) monitor = NULL;

  monitor = g_power_profile_monitor_dup_default ();
  if (monitor == NULL)
    return FALSE;

  return g_power_profile_monitor_get_power_saver_enabled (monitor);
}

static gboolean
should_skip_extension_ref (FlatpakInstalledRef *iref)
{
  const char *ref_name = NULL;

  ref_name = flatpak_ref_get_name (FLATPAK_REF (iref));

  return g_str_has_suffix (ref_name, ".Locale") ||
         g_str_has_suffix (ref_name, ".Debug") ||
         g_str_has_suffix (ref_name, ".Sources");
}

static gboolean
on_operation_error (FlatpakTransaction          *transaction,
                    FlatpakTransactionOperation *operation,
                    GError                      *error,
                    int                          details,
                    gpointer                     user_data)
{
  const char *ref = NULL;

  ref = flatpak_transaction_operation_get_ref (operation);

  g_warning ("Update failed for %s: %s", ref, error->message);

  return TRUE;
}

static gboolean
update_installation (FlatpakInstallation *installation,
                     GPtrArray           *history_out)
{
  g_autoptr (GError) local_error             = NULL;
  g_autoptr (GPtrArray) update_refs          = NULL;
  g_autoptr (FlatpakTransaction) transaction = NULL;
  g_autoptr (GPtrArray) pending              = NULL;
  gboolean added_any                         = FALSE;
  gboolean ran                               = FALSE;
  gboolean success                           = TRUE;

  update_refs = flatpak_installation_list_installed_refs_for_update (
      installation, NULL, &local_error);
  if (update_refs == NULL)
    {
      g_warning ("Failed to list updates: %s", local_error->message);
      return FALSE;
    }

  if (update_refs->len == 0)
    return TRUE;

  transaction = flatpak_transaction_new_for_installation (installation, NULL, &local_error);
  if (transaction == NULL)
    {
      g_warning ("Failed to create transaction: %s", local_error->message);
      return FALSE;
    }

  g_signal_connect (transaction, "operation-error", G_CALLBACK (on_operation_error), NULL);

  pending = g_ptr_array_new ();

  for (guint i = 0; i < update_refs->len; i++)
    {
      FlatpakInstalledRef *iref    = NULL;
      g_autofree char     *ref_fmt = NULL;
      const char          *title   = NULL;
      const char          *id      = NULL;
      const char          *old_ver = NULL;

      iref = g_ptr_array_index (update_refs, i);
      if (should_skip_extension_ref (iref))
        continue;

      ref_fmt = flatpak_ref_format_ref (FLATPAK_REF (iref));

      id    = flatpak_ref_get_name (FLATPAK_REF (iref));
      title = flatpak_installed_ref_get_appdata_name (iref);
      if (title == NULL)
        title = id;
      old_ver = flatpak_installed_ref_get_appdata_version (iref);

      if (flatpak_transaction_add_update (transaction, ref_fmt, NULL, NULL, &local_error))
        {
          BzUpdateHistoryDataPoint *point = NULL;

          added_any = TRUE;

          point = g_object_new (BZ_TYPE_UPDATE_HISTORY_DATA_POINT,
                                "id", id,
                                "title", title,
                                "old-version", old_ver != NULL ? old_ver : "",
                                "new-version", old_ver != NULL ? old_ver : "",
                                NULL);

          g_ptr_array_add (history_out, point);
          g_ptr_array_add (pending, iref);
          g_ptr_array_add (pending, point);
        }
      else
        {
          g_warning ("Failed to queue update for %s: %s", ref_fmt, local_error->message);
          g_clear_error (&local_error);
          success = FALSE;
        }
    }

  if (!added_any)
    return success;

  ran = flatpak_transaction_run (transaction, NULL, &local_error);
  if (!ran && local_error != NULL)
    {
      g_warning ("Transaction did not complete cleanly: %s", local_error->message);
      success = FALSE;
    }

  for (guint i = 0; i < pending->len; i += 2)
    {
      FlatpakInstalledRef      *iref              = NULL;
      BzUpdateHistoryDataPoint *point             = NULL;
      g_autoptr (FlatpakInstalledRef) updated_ref = NULL;
      g_autoptr (GError) resolve_error            = NULL;
      const char *new_ver                         = NULL;

      iref  = g_ptr_array_index (pending, i);
      point = g_ptr_array_index (pending, i + 1);

      updated_ref = flatpak_installation_get_installed_ref (
          installation,
          flatpak_ref_get_kind (FLATPAK_REF (iref)),
          flatpak_ref_get_name (FLATPAK_REF (iref)),
          flatpak_ref_get_arch (FLATPAK_REF (iref)),
          flatpak_ref_get_branch (FLATPAK_REF (iref)),
          NULL, &resolve_error);

      if (updated_ref != NULL)
        {
          new_ver = flatpak_installed_ref_get_appdata_version (updated_ref);
          bz_update_history_data_point_set_new_version (point, new_ver != NULL ? new_ver : "");
        }
    }

  return success;
}

static void
send_portal_notification (const char *id,
                          const char *title,
                          const char *body,
                          const char *action,
                          GVariant   *action_target)
{
  g_autoptr (GDBusConnection) conn = NULL;
  g_autoptr (GError) error         = NULL;
  GVariantBuilder notification     = { 0 };

  conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (conn == NULL)
    {
      g_warning ("Unable to reach session bus for notification: %s",
                 error->message);
      return;
    }

  g_variant_builder_init (&notification, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&notification, "{sv}", "title", g_variant_new_string (title));
  g_variant_builder_add (&notification, "{sv}", "body", g_variant_new_string (body));
  g_variant_builder_add (&notification, "{sv}", "priority", g_variant_new_string ("normal"));

  if (action != NULL)
    {
      GVariantBuilder buttons = { 0 };
      GVariantBuilder button  = { 0 };

      g_variant_builder_init (&button, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_add (&button, "{sv}", "label", g_variant_new_string (_ ("Details")));
      g_variant_builder_add (&button, "{sv}", "action", g_variant_new_string (action));
      if (action_target != NULL)
        g_variant_builder_add (&button, "{sv}", "target", action_target);

      g_variant_builder_init (&buttons, G_VARIANT_TYPE ("aa{sv}"));
      g_variant_builder_add (&buttons, "a{sv}", &button);

      g_variant_builder_add (&notification, "{sv}", "buttons", g_variant_builder_end (&buttons));
    }

  g_dbus_connection_call_sync (
      conn, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.Notification", "AddNotification",
      g_variant_new ("(sa{sv})", id, &notification),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

  if (error != NULL)
    g_warning ("Failed to send notification: %s", error->message);
}

static void
send_update_notification (GPtrArray *history)
{
  g_autoptr (GString) body   = NULL;
  g_autofree char *summary   = NULL;
  guint            n_updated = history->len;
  GVariantBuilder  builder   = { 0 };
  guint            i         = 0;

  if (n_updated == 0)
    return;

  body    = NULL;
  summary = NULL;
  i       = 0;

  body = g_string_new (NULL);

  summary = g_strdup_printf (
      ngettext ("%u App Updated", "%u Apps Updated", n_updated), n_updated);

  if (n_updated == 1)
    g_string_append_printf (body, _ ("%s has been updated."),
                            bz_update_history_data_point_get_title (g_ptr_array_index (history, 0)));
  else if (n_updated == 2)
    g_string_append_printf (body, _ ("%s and %s have been updated."),
                            bz_update_history_data_point_get_title (g_ptr_array_index (history, 0)),
                            bz_update_history_data_point_get_title (g_ptr_array_index (history, 1)));
  else if (n_updated >= 3)
    g_string_append_printf (body, _ ("Includes %s, %s and %s."),
                            bz_update_history_data_point_get_title (g_ptr_array_index (history, 0)),
                            bz_update_history_data_point_get_title (g_ptr_array_index (history, 1)),
                            bz_update_history_data_point_get_title (g_ptr_array_index (history, 2)));

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sss)"));
  for (i = 0; i < history->len; i++)
    {
      BzUpdateHistoryDataPoint *point   = NULL;
      const char               *id      = NULL;
      const char               *old_ver = NULL;
      const char               *new_ver = NULL;

      point   = g_ptr_array_index (history, i);
      id      = bz_update_history_data_point_get_id (point);
      old_ver = bz_update_history_data_point_get_old_version (point);
      new_ver = bz_update_history_data_point_get_new_version (point);

      g_variant_builder_add (&builder, "(sss)",
                             id != NULL ? id : "",
                             old_ver != NULL ? old_ver : "",
                             new_ver != NULL ? new_ver : "");
    }

  send_portal_notification ("bazaar-update", summary, body->str,
                            "app.show-update-history",
                            g_variant_builder_end (&builder));
}
