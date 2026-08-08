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
                                     GHashTable          *titles_out);
static void     send_portal_notification (const char *id,
                                          const char *title,
                                          const char *body);
static void     send_update_notification (GHashTable *titles);

int
run_update_worker (int   argc,
                   char *argv[])
{
  g_autoptr (GSettings) settings              = NULL;
  g_autoptr (GError) local_error              = NULL;
  g_autoptr (FlatpakInstallation) system_inst = NULL;
  g_autoptr (GHashTable) titles               = NULL;
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

  titles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

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
      if (!update_installation (system_inst, titles))
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
        if (!update_installation (user_inst, titles))
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

  if (g_hash_table_size (titles) == 0)
    {
      g_print ("No apps needed auto updating\n");
      return EXIT_SUCCESS;
    }

  if (auto_update_notifications)
    send_update_notification (titles);

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
                            _ ("Please check for available updates"));

  g_settings_set_int64 (settings, "last-stale-notification", g_date_time_to_unix (g_date_time_new_now_local ()));
}

static gboolean
network_is_restricted (void)
{
  GNetworkMonitor *monitor = NULL;

  monitor = g_network_monitor_get_default ();
  if (monitor == NULL)
    return FALSE;

  return !g_network_monitor_get_network_available (monitor) ||
         g_network_monitor_get_network_metered (monitor);
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
                     GHashTable          *titles_out)
{
  g_autoptr (GError) local_error             = NULL;
  g_autoptr (GPtrArray) update_refs          = NULL;
  g_autoptr (FlatpakTransaction) transaction = NULL;
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

  for (guint i = 0; i < update_refs->len; i++)
    {
      FlatpakInstalledRef *iref    = NULL;
      g_autofree char     *ref_fmt = NULL;
      const char          *title   = NULL;

      iref = g_ptr_array_index (update_refs, i);
      if (should_skip_extension_ref (iref))
        continue;

      ref_fmt = flatpak_ref_format_ref (FLATPAK_REF (iref));

      title = flatpak_installed_ref_get_appdata_name (iref);
      if (title == NULL)
        title = flatpak_ref_get_name (FLATPAK_REF (iref));

      g_hash_table_replace (titles_out, g_strdup (ref_fmt), g_strdup (title));

      if (flatpak_transaction_add_update (transaction, ref_fmt, NULL, NULL, &local_error))
        added_any = TRUE;
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

  return success;
}

static void
send_portal_notification (const char *id,
                          const char *title,
                          const char *body)
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

  g_dbus_connection_call_sync (
      conn, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.Notification", "AddNotification",
      g_variant_new ("(sa{sv})", id, &notification),
      NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

  if (error != NULL)
    g_warning ("Failed to send notification: %s", error->message);
}

static void
send_update_notification (GHashTable *titles)
{
  g_autoptr (GString) body      = NULL;
  g_autoptr (GPtrArray) updated = NULL;
  g_autofree char *summary      = NULL;
  GHashTableIter   iter         = { 0 };
  gpointer         ref_key      = NULL;
  gpointer         title_val    = NULL;
  guint            n_updated    = 0;

  body    = g_string_new (NULL);
  updated = g_ptr_array_new ();

  g_hash_table_iter_init (&iter, titles);
  while (g_hash_table_iter_next (&iter, &ref_key, &title_val))
    g_ptr_array_add (updated, title_val);

  n_updated = updated->len;
  if (n_updated == 0)
    return;

  summary = g_strdup_printf (
      ngettext ("%u App Updated", "%u Apps Updated", n_updated), n_updated);

  if (n_updated == 1)
    g_string_append_printf (body, _ ("%s has been updated."),
                            (const char *) g_ptr_array_index (updated, 0));
  else if (n_updated == 2)
    g_string_append_printf (body, _ ("%s and %s have been updated."),
                            (const char *) g_ptr_array_index (updated, 0),
                            (const char *) g_ptr_array_index (updated, 1));
  else if (n_updated >= 3)
    g_string_append_printf (body, _ ("Includes %s, %s and %s."),
                            (const char *) g_ptr_array_index (updated, 0),
                            (const char *) g_ptr_array_index (updated, 1),
                            (const char *) g_ptr_array_index (updated, 2));

  send_portal_notification ("bazaar-update", summary, body->str);
}
