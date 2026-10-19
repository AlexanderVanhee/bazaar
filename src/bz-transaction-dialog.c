/* bz-transaction-dialog.c
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

#include "config.h"

#include <bzvala.h>
#include <glib/gi18n.h>

#include "bz-application.h"
#include "bz-flatpak-entry.h"
#include "bz-transaction-dialog.h"
#include "env.h"
#include "error.h"
#include "safety-calculator.h"
#include "util.h"
#include "io.h"

static GListStore *
collect_selectable_entries (GListModel *store,
                            gboolean    remove)
{
  GListStore *selectable = NULL;
  guint       n_items    = 0;

  selectable = g_list_store_new (BZ_TYPE_ENTRY);
  if (store == NULL)
    return selectable;

  n_items = g_list_model_get_n_items (store);
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzEntry) entry = NULL;
      gboolean is_installed     = FALSE;
      gboolean skip             = FALSE;

      entry = g_list_model_get_item (store, i);

      if (bz_entry_is_holding (entry))
        skip = TRUE;
      else if (!remove && !bz_entry_is_reinstallable (entry))
        skip = TRUE;
      else
        {
          is_installed = bz_entry_is_installed (entry);
          skip         = (!remove && is_installed) || (remove && !is_installed);
        }

      if (!skip)
        g_list_store_append (selectable, entry);
    }

  return selectable;
}

static GPtrArray *
create_entry_radio_buttons (AdwAlertDialog *alert,
                            GListModel     *selectable,
                            gboolean        has_user_data)
{
  g_autoptr (GPtrArray) radios = NULL;
  GtkWidget *container         = NULL;
  guint      n_selectable      = 0;

  container    = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  radios       = g_ptr_array_new ();
  n_selectable = selectable != NULL ? g_list_model_get_n_items (selectable) : 0;

  if (n_selectable > 1)
    {
      GtkWidget      *listbox     = NULL;
      GtkCheckButton *dummy_radio = NULL;

      listbox = gtk_list_box_new ();
      gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);
      gtk_widget_add_css_class (listbox, "boxed-list");

      dummy_radio = GTK_CHECK_BUTTON (gtk_check_button_new ());

      for (guint i = 0; i < n_selectable; i++)
        {
          g_autoptr (BzEntry) entry     = NULL;
          BzStateInfo *state_info       = NULL;
          GListModel  *repositories     = NULL;
          g_autoptr (BzRepository) repo = NULL;
          const char *remote_title      = NULL;
          GtkWidget  *row               = NULL;
          GtkWidget  *radio             = NULL;

          entry = g_list_model_get_item (selectable, i);

          state_info   = bz_state_info_get_default ();
          repositories = bz_state_info_get_repositories (state_info);
          if (repositories != NULL)
            repo = bz_entry_get_repository (entry, repositories);
          remote_title = repo != NULL ? bz_repository_get_title (repo) : bz_entry_get_remote_repo_name (entry);

          row   = adw_action_row_new ();
          radio = gtk_check_button_new ();

          adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), remote_title != NULL ? remote_title : _ ("Unknown"));

          gtk_widget_set_valign (radio, GTK_ALIGN_CENTER);
          adw_action_row_add_prefix (ADW_ACTION_ROW (row), radio);
          adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), radio);

          gtk_check_button_set_group (GTK_CHECK_BUTTON (radio), dummy_radio);
          if (i == 0)
            gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);

          g_ptr_array_add (radios, radio);
          gtk_list_box_append (GTK_LIST_BOX (listbox), row);
        }

      gtk_box_append (GTK_BOX (container), listbox);
    }

  if (has_user_data)
    {
      GtkWidget *listbox         = NULL;
      GtkWidget *keep_data_row   = NULL;
      GtkWidget *delete_data_row = NULL;
      GtkWidget *keep_radio      = NULL;
      GtkWidget *delete_radio    = NULL;

      listbox = gtk_list_box_new ();
      gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);
      gtk_widget_add_css_class (listbox, "boxed-list");

      keep_data_row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (keep_data_row), _ ("Keep User Data"));
      adw_action_row_set_subtitle (ADW_ACTION_ROW (keep_data_row), _ ("Allow restoring personal settings &amp; content"));
      keep_radio = gtk_check_button_new ();
      gtk_widget_set_valign (keep_radio, GTK_ALIGN_CENTER);
      gtk_check_button_set_active (GTK_CHECK_BUTTON (keep_radio), TRUE);
      adw_action_row_add_prefix (ADW_ACTION_ROW (keep_data_row), keep_radio);
      adw_action_row_set_activatable_widget (ADW_ACTION_ROW (keep_data_row), keep_radio);
      gtk_list_box_append (GTK_LIST_BOX (listbox), keep_data_row);

      delete_data_row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (delete_data_row), _ ("Delete All Data"));
      adw_action_row_set_subtitle (ADW_ACTION_ROW (delete_data_row), _ ("Permanently erase user data to save space"));
      delete_radio = gtk_check_button_new ();
      gtk_widget_set_valign (delete_radio, GTK_ALIGN_CENTER);
      gtk_check_button_set_group (GTK_CHECK_BUTTON (delete_radio), GTK_CHECK_BUTTON (keep_radio));
      adw_action_row_add_prefix (ADW_ACTION_ROW (delete_data_row), delete_radio);
      adw_action_row_set_activatable_widget (ADW_ACTION_ROW (delete_data_row), delete_radio);
      gtk_list_box_append (GTK_LIST_BOX (listbox), delete_data_row);

      g_ptr_array_add (radios, keep_radio);
      g_ptr_array_add (radios, delete_radio);
      gtk_box_append (GTK_BOX (container), listbox);
    }

  if (n_selectable > 1 || has_user_data)
    adw_alert_dialog_set_extra_child (alert, container);
  else
    g_object_unref (g_object_ref_sink (container));

  return g_steal_pointer (&radios);
}

static void
configure_remove_dialog (AdwAlertDialog *alert,
                         const char     *title,
                         const char     *id,
                         gboolean        has_multiple_entries)
{
  g_autofree char *heading = NULL;
  g_autofree char *body    = NULL;

  heading = g_strdup_printf (_ ("Remove %s?"), title);

  if (has_multiple_entries)
    body = g_strdup (_ ("Select which version to remove."));
  else
    body = g_strdup_printf (_ ("It will not be possible to use %s after it is uninstalled."), title);

  adw_alert_dialog_set_heading (alert, heading);
  adw_alert_dialog_set_body (alert, body);

  adw_alert_dialog_add_responses (alert,
                                  "cancel", _ ("_Cancel"),
                                  "remove", _ ("_Remove"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (alert, "remove", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response (alert, "remove");
  adw_alert_dialog_set_close_response (alert, "cancel");
}

static void
configure_high_risk_warning_dialog (AdwAlertDialog *alert,
                                    const char     *title,
                                    BzHighRiskGroup risk_groups)
{
  g_autofree char *heading = NULL;
  g_autofree char *body    = NULL;

  heading = g_strdup_printf (_ ("“%s” is High Risk"), title);

  if (risk_groups & BZ_HIGH_RISK_GROUP_DISK)
    {
      body = g_strdup (_ ("This app has full access to your system, including all "
                          "<b>your files, browser history, saved passwords</b>, and "
                          "more. It also has access to the internet, meaning it "
                          "could send your data to outside parties.\n\n"
                          "Because the app is proprietary, it can not be audited "
                          "for what it does with these permissions."));
    }
  else if (risk_groups & BZ_HIGH_RISK_GROUP_X11)
    {
      body = g_strdup (_ ("This app uses the legacy X11 windowing system, which "
                          "allows it to <b>record all keystrokes, capture screenshots, "
                          "and monitor other applications</b>. It also has access "
                          "to the internet, meaning it could send your data to "
                          "outside parties.\n\n"
                          "Because the app is proprietary, it can not be audited "
                          "for what it does with these permissions."));
    }

  adw_alert_dialog_set_heading (alert, heading);
  adw_alert_dialog_set_body (alert, body);
  adw_alert_dialog_set_body_use_markup (alert, TRUE);
  adw_alert_dialog_set_prefer_wide_layout (alert, TRUE);

  adw_alert_dialog_add_responses (alert,
                                  "cancel", _ ("_Cancel"),
                                  "install", _ ("_Install Anyway"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (alert, "install", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response (alert, "install");
  adw_alert_dialog_set_close_response (alert, "cancel");
}

static BzHighRiskGroup
get_entry_high_risk_groups (BzEntry *entry)
{
  if (bz_entry_get_is_foss (entry))
    return BZ_HIGH_RISK_GROUP_NONE;

  return bz_safety_calculator_get_high_risk_groups (entry);
}

static DexFuture *
show_dialog_fiber (GtkWidget    *parent,
                   BzEntry      *entry,
                   BzEntryGroup *group,
                   gboolean      remove,
                   gboolean      auto_confirm)
{
  g_autoptr (GError) local_error               = NULL;
  g_autoptr (GListStore) store                 = NULL;
  g_autoptr (GListStore) selectable            = NULL;
  const char *title                            = NULL;
  const char *id                               = NULL;
  g_autoptr (AdwDialog) alert                  = NULL;
  g_autoptr (AdwDialog) risk_alert             = NULL;
  g_autoptr (GPtrArray) radios                 = NULL;
  g_autofree char *dialog_response             = NULL;
  g_autofree char *risk_response               = NULL;
  g_autoptr (BzTransactionDialogResult) result = NULL;
  g_autoptr (BzEntry) check_entry              = NULL;
  g_autoptr (BzEntry) selected_entry           = NULL;
  BzHighRiskGroup risk_groups                  = BZ_HIGH_RISK_GROUP_NONE;
  guint           n_selectable                 = 0;
  gboolean        confirmed                    = FALSE;
  gboolean        has_user_data                = FALSE;

  result = bz_transaction_dialog_result_new ();

  if (group != NULL)
    {
      title = bz_entry_group_get_title (group);
      id    = bz_entry_group_get_id (group);

      store = dex_await_object (bz_entry_group_dup_all_into_store (group), &local_error);
      if (store == NULL)
        {
          bz_show_error_for_widget (parent, _ ("Failed to load transaction dialog"), local_error->message);
          return dex_future_new_for_error (g_steal_pointer (&local_error));
        }

      if (remove)
        {
          for (guint i = g_list_model_get_n_items (G_LIST_MODEL (store)); i > 0; i--)
            {
              g_autoptr (BzEntry) candidate_entry = NULL;

              candidate_entry = g_list_model_get_item (G_LIST_MODEL (store), i - 1);
              if (!bz_entry_is_reinstallable (candidate_entry) &&
                  (!remove || !bz_entry_is_installed (candidate_entry)))
                g_list_store_remove (store, i - 1);
            }

          selectable   = collect_selectable_entries (G_LIST_MODEL (store), remove);
          n_selectable = g_list_model_get_n_items (G_LIST_MODEL (selectable));
        }

      if (g_list_model_get_n_items (G_LIST_MODEL (store)) > 0)
        check_entry = g_list_model_get_item (G_LIST_MODEL (store), 0);

      if (check_entry == NULL)
        {
          g_set_error (&local_error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                      "No entries for %s were able to be resolved",
                      id != NULL ? id : "(unknown)");
          bz_show_error_for_widget (parent, _ ("Failed to load transaction dialog"), local_error->message);
          return dex_future_new_for_error (g_steal_pointer (&local_error));
        }
    }
  else
    {
      title       = bz_entry_get_title (entry);
      id          = bz_entry_get_id (entry);
      check_entry = g_object_ref (entry);
    }

  if (remove && id != NULL)
    has_user_data = dex_await_boolean (bz_user_data_exists (id), NULL);

  if (!remove && check_entry != NULL)
    risk_groups = get_entry_high_risk_groups (check_entry);

  if (risk_groups != BZ_HIGH_RISK_GROUP_NONE)
    {
      risk_alert = g_object_ref_sink (adw_alert_dialog_new (NULL, NULL));
      configure_high_risk_warning_dialog (ADW_ALERT_DIALOG (risk_alert), title, risk_groups);

      adw_dialog_present (risk_alert, parent);
      risk_response = dex_await_string (
          bz_make_alert_dialog_future (ADW_ALERT_DIALOG (risk_alert)),
          &local_error);

      if (risk_response == NULL)
        return dex_future_new_for_error (g_steal_pointer (&local_error));

      if (g_strcmp0 (risk_response, "install") != 0)
        {
          bz_transaction_dialog_result_set_confirmed (result, FALSE);
          return dex_future_new_for_object (result);
        }
    }

  if (remove)
    {
      alert = g_object_ref_sink (adw_alert_dialog_new (NULL, NULL));
      configure_remove_dialog (ADW_ALERT_DIALOG (alert), title, id, n_selectable > 1);

      radios = create_entry_radio_buttons (ADW_ALERT_DIALOG (alert), G_LIST_MODEL (selectable), has_user_data);

      if (auto_confirm && radios->len <= 1)
        {
          dialog_response = g_strdup ("remove");
          g_ptr_array_set_size (radios, 0);
          g_clear_object (&alert);
        }
      else
        {
          adw_dialog_present (alert, parent);
          dialog_response = dex_await_string (
              bz_make_alert_dialog_future (ADW_ALERT_DIALOG (alert)),
              &local_error);
          if (dialog_response == NULL)
            return dex_future_new_for_error (g_steal_pointer (&local_error));

          if (has_user_data && radios->len >= 1)
            {
              GtkCheckButton *delete_radio = NULL;

              delete_radio = g_ptr_array_index (radios, radios->len - 1);
              bz_transaction_dialog_result_set_delete_user_data (result, gtk_check_button_get_active (delete_radio));
            }
        }

      confirmed = g_strcmp0 (dialog_response, "remove") == 0;
    }
  else
    confirmed = TRUE;

  bz_transaction_dialog_result_set_confirmed (result, confirmed);
  if (!confirmed)
    return dex_future_new_for_object (result);

  if (group != NULL)
    {
      if (selectable != NULL && n_selectable > 0)
        {
          if (n_selectable > 1)
            {
              for (guint i = 0; i < n_selectable; i++)
                {
                  GtkCheckButton *check = g_ptr_array_index (radios, i);

                  if (gtk_check_button_get_active (check))
                    {
                      selected_entry = g_list_model_get_item (G_LIST_MODEL (selectable), i);
                      break;
                    }
                }
            }

          if (selected_entry == NULL)
            selected_entry = g_list_model_get_item (G_LIST_MODEL (selectable), 0);
        }
      else
        selected_entry = check_entry != NULL ? g_object_ref (check_entry) : NULL;
    }
  else
    selected_entry = g_object_ref (entry);

  bz_transaction_dialog_result_set_selected_entry (result, selected_entry);

  return dex_future_new_for_object (result);
}

DexFuture *
bz_transaction_dialog_show (GtkWidget    *parent,
                            BzEntry      *entry,
                            BzEntryGroup *group,
                            gboolean      remove,
                            gboolean      auto_confirm)
{
  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);
  g_return_val_if_fail (entry != NULL || group != NULL, NULL);

  return dex_scheduler_spawnv (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      G_CALLBACK (show_dialog_fiber),
      5,
      GTK_TYPE_WIDGET, parent,
      BZ_TYPE_ENTRY, entry,
      BZ_TYPE_ENTRY_GROUP, group,
      G_TYPE_BOOLEAN, remove,
      G_TYPE_BOOLEAN, auto_confirm);
}
