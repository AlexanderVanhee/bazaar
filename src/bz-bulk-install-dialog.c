/* bz-bulk-install-dialog.c
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

#include <bzvala.h>
#include "config.h"

#include <glib/gi18n.h>

#include "bz-bulk-install-dialog.h"
#include "bz-entry-group.h"
#include "bz-transaction-list-dialog.h"
#include "env.h"
#include "error.h"
#include "util.h"

static DexFuture *
bulk_install_dialog_fiber (GtkWidget  *parent,
                           GListModel *groups)
{
  g_autoptr (GError) local_error               = NULL;
  g_autoptr (BzBulkInstallDialogResult) result = NULL;
  g_autoptr (GPtrArray) resolved_entries       = NULL;
  g_autoptr (GListStore) entries_store         = NULL;
  AdwDialog       *dialog                      = NULL;
  g_autofree char *dialog_response             = NULL;
  g_autofree char *heading                     = NULL;
  guint            n_groups                    = 0;
  gboolean         confirmed                   = FALSE;

  result           = bz_bulk_install_dialog_result_new ();
  resolved_entries = g_ptr_array_new_with_free_func (g_object_unref);

  if (groups == NULL)
    {
      bz_bulk_install_dialog_result_set_confirmed (result, FALSE);
      return dex_future_new_for_object (result);
    }

  n_groups = g_list_model_get_n_items (groups);

  for (guint i = 0; i < n_groups; i++)
    {
      g_autoptr (BzEntryGroup) group = NULL;
      g_autoptr (GListStore) store   = NULL;
      g_autoptr (BzEntry) entry      = NULL;

      group = g_list_model_get_item (groups, i);

      if (bz_entry_group_get_removable (group) > 0)
        continue;

      store = dex_await_object (bz_entry_group_dup_all_into_store (group), &local_error);
      if (store == NULL || g_list_model_get_n_items (G_LIST_MODEL (store)) == 0)
        continue;

      entry = g_list_model_get_item (G_LIST_MODEL (store), 0);
      if (entry == NULL)
        continue;

      if (bz_entry_is_installed (entry) || bz_entry_is_holding (entry))
        continue;

      g_ptr_array_add (resolved_entries, g_object_ref (entry));
    }

  if (resolved_entries->len == 0)
    {
      g_autoptr (AdwDialog) info_alert = NULL;

      info_alert = g_object_ref_sink (adw_alert_dialog_new (
          _ ("All apps are already installed"), NULL));

      adw_alert_dialog_add_response (ADW_ALERT_DIALOG (info_alert), "ok", _ ("_OK"));
      adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (info_alert), "ok");
      adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (info_alert), "ok");

      adw_dialog_present (info_alert, parent);

      dex_await (bz_make_alert_dialog_future (ADW_ALERT_DIALOG (info_alert)), NULL);

      bz_bulk_install_dialog_result_set_confirmed (result, FALSE);
      return dex_future_new_for_object (result);
    }

  entries_store = g_list_store_new (BZ_TYPE_ENTRY);
  for (guint i = 0; i < resolved_entries->len; i++)
    g_list_store_append (entries_store, g_ptr_array_index (resolved_entries, i));

  heading = g_strdup_printf (ngettext ("Install %u App?",
                                       "Install %u Apps?",
                                       resolved_entries->len),
                             resolved_entries->len);

  dialog = bz_transaction_list_dialog_new (
      G_LIST_MODEL (entries_store),
      heading,
      _ ("The following will be installed. Additional shared components may also be installed"),
      _ ("%d addons will be installed."),
      _ ("Additionally, addons will be installed."),
      _ ("_Cancel"),
      _ ("_Install All"));

  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "confirm");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

  adw_dialog_present (dialog, parent);

  dialog_response = dex_await_string (
      bz_make_alert_dialog_future (ADW_ALERT_DIALOG (dialog)),
      &local_error);

  if (dialog_response == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  confirmed = bz_transaction_list_dialog_was_confirmed (
      BZ_TRANSACTION_LIST_DIALOG (dialog));
  bz_bulk_install_dialog_result_set_confirmed (result, confirmed);
  if (confirmed)
    {
      g_autoptr (GListStore) store = NULL;

      store = g_list_store_new (BZ_TYPE_ENTRY);
      for (guint i = 0; i < resolved_entries->len; i++)
        {
          BzEntry *entry = g_ptr_array_index (resolved_entries, i);
          g_list_store_append (store, entry);
        }

      bz_bulk_install_dialog_result_set_entries (result, G_LIST_MODEL (store));
    }
  return dex_future_new_for_object (result);
}

DexFuture *
bz_bulk_install_dialog_show (GtkWidget  *parent,
                             GListModel *groups)
{
  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);
  g_return_val_if_fail (G_IS_LIST_MODEL (groups), NULL);

  return dex_scheduler_spawnv (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      G_CALLBACK (bulk_install_dialog_fiber),
      2,
      GTK_TYPE_WIDGET, parent,
      G_TYPE_LIST_MODEL, groups);
}
