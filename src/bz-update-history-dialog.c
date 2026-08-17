/* bz-update-history-dialog.c
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

#include <glib/gi18n.h>

#include "bz-entry-group.h"
#include "bz-entry.h"
#include "bz-releases-list.h"
#include "bz-result.h"
#include "bz-update-history-data-point.h"
#include "bz-update-history-dialog.h"
#include "env.h"
#include "template-callbacks.h"
#include "util.h"

struct _BzUpdateHistoryDialog
{
  AdwDialog parent_instance;

  /* Template widgets */
  AdwNavigationView *nav_view;
  GtkListView       *list_view;
  AdwNavigationPage *releases_page;
  GtkStack          *releases_stack;
  AdwBin            *releases_bin;
};

G_DEFINE_FINAL_TYPE (BzUpdateHistoryDialog, bz_update_history_dialog, ADW_TYPE_DIALOG)

static DexFuture *
load_releases_fiber (BzWeakRef     *wr,
                     BzEntryGroup  *group,
                     GtkStringList *installed);

static char *
format_version_change (gpointer    object,
                       const char *old_version,
                       const char *new_version)
{
  if (new_version == NULL)
    return g_strdup ("");

  if (old_version != NULL && g_strcmp0 (old_version, new_version) != 0)
    return g_strdup_printf ("%s → %s", old_version, new_version);

  return g_strdup (new_version);
}

static void
row_activated_cb (BzUpdateHistoryDialog *self,
                  guint                  position,
                  GtkListView           *list_view)
{
  g_autoptr (BzUpdateHistoryDataPoint) point = NULL;
  g_autoptr (BzWeakRef) wr                   = NULL;
  g_autoptr (GtkStringList) installed        = NULL;
  GtkSelectionModel *model                   = NULL;
  BzEntryGroup      *group                   = NULL;
  const char        *new_version             = NULL;

  g_return_if_fail (BZ_IS_UPDATE_HISTORY_DIALOG (self));

  model = gtk_list_view_get_model (self->list_view);
  if (model == NULL)
    return;

  point = g_list_model_get_item (G_LIST_MODEL (model), position);
  if (point == NULL)
    return;

  group       = bz_update_history_data_point_get_group (point);
  new_version = bz_update_history_data_point_get_new_version (point);

  if (group != NULL)
    {
      const char *title = NULL;

      title = bz_entry_group_get_title (group);
      adw_navigation_page_set_title (self->releases_page,
                                     title != NULL ? title : _ ("Release Notes"));
    }

  adw_bin_set_child (self->releases_bin, NULL);
  gtk_stack_set_visible_child_name (self->releases_stack, "empty");
  adw_navigation_view_push_by_tag (self->nav_view, "releases");

  if (group == NULL)
    return;

  wr        = bz_weak_ref_new (self);
  installed = gtk_string_list_new (NULL);

  if (new_version != NULL)
    gtk_string_list_append (installed, new_version);

  dex_future_disown (dex_scheduler_spawnv (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      G_CALLBACK (load_releases_fiber),
      3,
      BZ_TYPE_WEAK_REF, wr,
      BZ_TYPE_ENTRY_GROUP, group,
      GTK_TYPE_STRING_LIST, installed));
}

static void
bz_update_history_dialog_class_init (BzUpdateHistoryDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (BZ_TYPE_UPDATE_HISTORY_DATA_POINT);
  g_type_ensure (BZ_TYPE_ENTRY_GROUP);
  g_type_ensure (BZ_TYPE_ENTRY);
  g_type_ensure (BZ_TYPE_RESULT);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/io/github/kolunmi/Bazaar/bz-update-history-dialog.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);
  gtk_widget_class_bind_template_child (widget_class, BzUpdateHistoryDialog, nav_view);
  gtk_widget_class_bind_template_child (widget_class, BzUpdateHistoryDialog, list_view);
  gtk_widget_class_bind_template_child (widget_class, BzUpdateHistoryDialog, releases_page);
  gtk_widget_class_bind_template_child (widget_class, BzUpdateHistoryDialog, releases_stack);
  gtk_widget_class_bind_template_child (widget_class, BzUpdateHistoryDialog, releases_bin);

  gtk_widget_class_bind_template_callback (widget_class, format_version_change);
  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);
}

static void
bz_update_history_dialog_init (BzUpdateHistoryDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwDialog *
bz_update_history_dialog_new (GListModel *model)
{
  BzUpdateHistoryDialog *self = NULL;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);

  self = g_object_new (BZ_TYPE_UPDATE_HISTORY_DIALOG, NULL);

  if (model != NULL)
    {
      g_autoptr (GtkNoSelection) selection = NULL;

      selection = gtk_no_selection_new (g_object_ref (model));
      gtk_list_view_set_model (self->list_view, GTK_SELECTION_MODEL (selection));
    }

  return ADW_DIALOG (self);
}

static DexFuture *
load_releases_fiber (BzWeakRef     *wr,
                     BzEntryGroup  *group,
                     GtkStringList *installed)
{
  g_autoptr (BzUpdateHistoryDialog) self = NULL;
  g_autoptr (GError) local_error         = NULL;
  g_autoptr (BzResult) result            = NULL;
  g_autoptr (BzEntry) entry              = NULL;
  g_autoptr (GListModel) history         = NULL;
  GtkWidget *list                        = NULL;
  guint      n_items                     = 0;

  bz_weak_get_or_return_reject (self, &wr->ref);

  result = bz_entry_group_dup_ui_entry (group);
  if (result == NULL)
    return dex_future_new_false ();

  entry = dex_await_object (bz_result_dup_future (result), &local_error);
  if (entry == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  g_object_get (entry, "version-history", &history, NULL);
  if (history != NULL)
    n_items = g_list_model_get_n_items (history);

  if (n_items > 0)
    {
      list = bz_releases_list_new (history, G_LIST_MODEL (installed));
      adw_bin_set_child (self->releases_bin, list);
    }

  gtk_stack_set_visible_child_name (self->releases_stack,
                                    n_items > 0 ? "content" : "empty");

  return dex_future_new_true ();
}
