/* bz-downgrade-page.c
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

#include "bz-application.h"
#include "bz-commit.h"
#include "bz-downgrade-page.h"
#include "bz-flatpak-entry.h"
#include "bz-flatpak-private.h"
#include "bz-release.h"
#include "bz-state-info.h"
#include "bz-util.h"

struct _BzDowngradePage
{
  AdwNavigationPage parent_instance;

  BzEntryGroup *entry_group;
  GListModel   *version_history;
  DexFuture    *fetch_future;

  char *selected_checksum;
  char *installed_checksum;

  GtkStack   *stack;
  GtkListBox *commits_box;
  GtkButton  *downgrade_button;
};

G_DEFINE_FINAL_TYPE (BzDowngradePage, bz_downgrade_page, ADW_TYPE_NAVIGATION_PAGE)

BZ_DEFINE_DATA (
    begin_fetch,
    BeginFetch,
    {
      GWeakRef      page_wr;
      BzEntryGroup *entry_group;
    },
    g_weak_ref_clear (&self->page_wr);
    BZ_RELEASE_DATA (entry_group, g_object_unref))

static char      *infer_version_for_commit (GListModel *version_history,
                                            const char *subject,
                                            guint64     timestamp);
static void       update_button_sensitivity (BzDowngradePage *self);
static void       on_radio_toggled (GtkCheckButton  *radio,
                                    BzDowngradePage *self);
static GtkWidget *create_commit_row (BzCommit        *commit,
                                     GtkWidget       *first_radio,
                                     BzDowngradePage *self,
                                     GHashTable      *version_totals,
                                     GHashTable      *version_counters);
static DexFuture *on_commit_history_done (DexFuture *future,
                                          GWeakRef  *wr);
static DexFuture *begin_fetch_fiber (BeginFetchData *data);
static void       downgrade_cb (GtkButton       *button,
                                BzDowngradePage *self);
static void       begin_fetch (BzDowngradePage *self);

static void
bz_downgrade_page_dispose (GObject *object)
{
  BzDowngradePage *self = BZ_DOWNGRADE_PAGE (object);

  dex_clear (&self->fetch_future);
  g_clear_object (&self->entry_group);
  g_clear_object (&self->version_history);
  g_clear_pointer (&self->selected_checksum, g_free);
  g_clear_pointer (&self->installed_checksum, g_free);

  G_OBJECT_CLASS (bz_downgrade_page_parent_class)->dispose (object);
}

static void
bz_downgrade_page_class_init (BzDowngradePageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bz_downgrade_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/io/github/kolunmi/Bazaar/bz-downgrade-page.ui");
  gtk_widget_class_bind_template_child (widget_class, BzDowngradePage, stack);
  gtk_widget_class_bind_template_child (widget_class, BzDowngradePage, commits_box);
  gtk_widget_class_bind_template_child (widget_class, BzDowngradePage, downgrade_button);
  gtk_widget_class_bind_template_callback (widget_class, downgrade_cb);
}

static void
bz_downgrade_page_init (BzDowngradePage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_downgrade_page_new (BzEntryGroup *entry_group,
                       GListModel   *version_history)
{
  BzDowngradePage *self = NULL;

  self = g_object_new (BZ_TYPE_DOWNGRADE_PAGE, NULL);

  if (entry_group)
    self->entry_group = g_object_ref (entry_group);

  if (version_history)
    self->version_history = g_object_ref (version_history);

  if (self->entry_group != NULL)
    begin_fetch (self);

  return GTK_WIDGET (self);
}

static char *
infer_version_for_commit (GListModel *version_history,
                          const char *subject,
                          guint64     timestamp)
{
  guint           n_items           = 0;
  guint64         best_past_delta   = G_MAXUINT64;
  guint64         best_any_delta    = G_MAXUINT64;
  g_autofree char *best_past_version = NULL;
  g_autofree char *best_any_version  = NULL;

  if (version_history == NULL)
    return NULL;

  n_items = g_list_model_get_n_items (version_history);
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzRelease) release = NULL;
      const char           *version = NULL;
      guint64               rel_ts  = 0;
      guint64               delta   = 0;

      release = g_list_model_get_item (version_history, i);
      version = bz_release_get_version (release);
      rel_ts  = bz_release_get_timestamp (release);

      if (version == NULL)
        continue;

      if (subject != NULL && strstr (subject, version) != NULL)
        return g_strdup (version);

      if (timestamp == 0 || rel_ts == 0)
        continue;

      delta = timestamp > rel_ts ? timestamp - rel_ts : rel_ts - timestamp;

      if (delta < best_any_delta)
        {
          best_any_delta   = delta;
          g_free (best_any_version);
          best_any_version = g_strdup (version);
        }

      if (rel_ts <= timestamp && delta < best_past_delta)
        {
          best_past_delta   = delta;
          g_free (best_past_version);
          best_past_version = g_strdup (version);
        }
    }

  if (best_past_version != NULL)
    return g_steal_pointer (&best_past_version);

  return g_steal_pointer (&best_any_version);
}

static void
update_button_sensitivity (BzDowngradePage *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->downgrade_button),
                            self->selected_checksum != NULL &&
                                self->installed_checksum != NULL &&
                                g_strcmp0 (self->selected_checksum, self->installed_checksum) != 0);
}

static void
on_radio_toggled (GtkCheckButton  *radio,
                  BzDowngradePage *self)
{
  if (!gtk_check_button_get_active (radio))
    return;

  g_free (self->selected_checksum);
  self->selected_checksum = g_strdup (g_object_get_data (G_OBJECT (radio), "checksum"));
  update_button_sensitivity (self);
}

static GtkWidget *
create_commit_row (BzCommit        *commit,
                   GtkWidget       *first_radio,
                   BzDowngradePage *self,
                   GHashTable      *version_totals,
                   GHashTable      *version_counters)
{
  g_autofree char *version   = NULL;
  g_autofree char *short_sum = NULL;
  g_autofree char *title_str = NULL;
  g_autofree char *date_str  = NULL;
  g_autoptr (GDateTime) dt   = NULL;
  AdwActionRow   *row        = NULL;
  GtkCheckButton *radio      = NULL;
  GtkLabel       *ver_label  = NULL;
  GtkBox         *ver_box    = NULL;
  GtkLabel       *sum_label  = NULL;
  GtkBox         *info_box   = NULL;
  GtkLabel       *date_label = NULL;
  GtkBox         *content    = NULL;

  version   = infer_version_for_commit (self->version_history,
                                        bz_commit_get_subject (commit),
                                        bz_commit_get_timestamp (commit));
  short_sum = g_strndup (bz_commit_get_checksum (commit),
                         MIN (12, strlen (bz_commit_get_checksum (commit))));

  if (version != NULL)
    {
      guint total   = 0;
      guint current = 0;

      total   = GPOINTER_TO_UINT (g_hash_table_lookup (version_totals, version));
      current = GPOINTER_TO_UINT (g_hash_table_lookup (version_counters, version));

      if (current == 0)
        current = total;

      g_hash_table_insert (version_counters, g_strdup (version), GUINT_TO_POINTER (current - 1));
      title_str = total > 1 ? g_strdup_printf ("%s (%u)", version, current) : g_strdup (version);
    }

  dt = g_date_time_new_from_unix_utc ((gint64) bz_commit_get_timestamp (commit));
  if (dt != NULL)
    date_str = g_date_time_format (dt, "%e %b %Y %H:%M");

  radio = GTK_CHECK_BUTTON (g_object_new (GTK_TYPE_CHECK_BUTTON,
                                          "active", bz_commit_get_installed (commit),
                                          "valign", GTK_ALIGN_CENTER,
                                          NULL));
  if (first_radio != NULL)
    gtk_check_button_set_group (radio, GTK_CHECK_BUTTON (first_radio));

  ver_label = GTK_LABEL (g_object_new (GTK_TYPE_LABEL,
                                       "label", title_str ? title_str : short_sum,
                                       "halign", GTK_ALIGN_START,
                                       "ellipsize", PANGO_ELLIPSIZE_END,
                                       NULL));
  gtk_widget_add_css_class (GTK_WIDGET (ver_label), "accent");
  gtk_widget_add_css_class (GTK_WIDGET (ver_label), "heading");

  ver_box = GTK_BOX (g_object_new (GTK_TYPE_BOX,
                                   "orientation", GTK_ORIENTATION_HORIZONTAL,
                                   "spacing", 6, "valign", GTK_ALIGN_CENTER,
                                   NULL));
  gtk_box_append (ver_box, GTK_WIDGET (ver_label));

  if (bz_commit_get_installed (commit))
    {
      GtkLabel *ins_label = NULL;

      ins_label = GTK_LABEL (g_object_new (GTK_TYPE_LABEL,
                                           "label", _ ("Installed"),
                                           "valign", GTK_ALIGN_CENTER,
                                           NULL));
      gtk_widget_add_css_class (GTK_WIDGET (ins_label), "lozenge");
      gtk_widget_add_css_class (GTK_WIDGET (ins_label), "small");
      gtk_box_append (ver_box, GTK_WIDGET (ins_label));
    }

  sum_label = GTK_LABEL (g_object_new (GTK_TYPE_LABEL,
                                       "label", short_sum,
                                       "halign", GTK_ALIGN_START,
                                       "ellipsize", PANGO_ELLIPSIZE_END,
                                       NULL));
  gtk_widget_add_css_class (GTK_WIDGET (sum_label), "dim-label");
  gtk_widget_add_css_class (GTK_WIDGET (sum_label), "caption");

  info_box = GTK_BOX (g_object_new (GTK_TYPE_BOX,
                                    "orientation", GTK_ORIENTATION_VERTICAL,
                                    "spacing", 2, "hexpand", TRUE,
                                    "valign", GTK_ALIGN_CENTER,
                                    NULL));
  gtk_box_append (info_box, GTK_WIDGET (ver_box));
  gtk_box_append (info_box, GTK_WIDGET (sum_label));

  date_label = GTK_LABEL (g_object_new (GTK_TYPE_LABEL,
                                        "label", date_str ? date_str : "",
                                        "valign", GTK_ALIGN_START,
                                        "halign", GTK_ALIGN_END,
                                        NULL));
  gtk_widget_add_css_class (GTK_WIDGET (date_label), "dim-label");
  gtk_widget_add_css_class (GTK_WIDGET (date_label), "caption");

  content = GTK_BOX (g_object_new (GTK_TYPE_BOX,
                                   "orientation", GTK_ORIENTATION_HORIZONTAL,
                                   "spacing", 12, "hexpand", TRUE,
                                   "valign", GTK_ALIGN_CENTER,
                                   "margin-top", 10, "margin-bottom", 10,
                                   "margin-start", 12, "margin-end", 12,
                                   NULL));
  gtk_box_append (content, GTK_WIDGET (radio));
  gtk_box_append (content, GTK_WIDGET (info_box));
  gtk_box_append (content, GTK_WIDGET (date_label));

  row = ADW_ACTION_ROW (g_object_new (ADW_TYPE_ACTION_ROW,
                                      "activatable", TRUE,
                                      "title", "",
                                      "child", content,
                                      NULL));
  adw_action_row_set_activatable_widget (row, GTK_WIDGET (radio));

  g_object_set_data_full (G_OBJECT (radio), "checksum",
                          g_strdup (bz_commit_get_checksum (commit)), g_free);
  g_signal_connect (radio, "toggled", G_CALLBACK (on_radio_toggled), self);
  g_object_set_data (G_OBJECT (row), "radio", radio);

  if (bz_commit_get_installed (commit))
    {
      g_free (self->installed_checksum);
      self->installed_checksum = g_strdup (bz_commit_get_checksum (commit));
      g_free (self->selected_checksum);
      self->selected_checksum = g_strdup (bz_commit_get_checksum (commit));
    }

  return GTK_WIDGET (row);
}

static DexFuture *
on_commit_history_done (DexFuture *future,
                        GWeakRef  *wr)
{
  g_autoptr (BzDowngradePage) self = NULL;
  g_autoptr (GError) error         = NULL;
  g_autoptr (GHashTable) totals    = NULL;
  g_autoptr (GHashTable) counters  = NULL;
  const GValue *value              = NULL;
  GListModel   *commits            = NULL;
  GtkWidget    *first_radio        = NULL;
  guint         n_commits          = 0;

  self = g_weak_ref_get (wr);
  if (self == NULL)
    return dex_future_new_true ();

  value = dex_future_get_value (future, &error);
  if (error != NULL)
    {
      gtk_stack_set_visible_child_name (self->stack, "error");
      return dex_future_new_true ();
    }

  commits   = G_LIST_MODEL (g_value_get_object (value));
  n_commits = g_list_model_get_n_items (commits);

  if (n_commits == 0)
    {
      gtk_stack_set_visible_child_name (self->stack, "empty");
      return dex_future_new_true ();
    }

  totals = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  for (guint i = 0; i < n_commits; i++)
    {
      g_autoptr (BzCommit) commit = NULL;
      g_autofree char *version    = NULL;
      guint            count      = 0;

      commit  = g_list_model_get_item (commits, i);
      version = infer_version_for_commit (self->version_history,
                                          bz_commit_get_subject (commit),
                                          bz_commit_get_timestamp (commit));

      if (version == NULL)
        continue;

      count = GPOINTER_TO_UINT (g_hash_table_lookup (totals, version)) + 1;
      g_hash_table_insert (totals, g_strdup (version), GUINT_TO_POINTER (count));
    }

  counters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  for (guint i = 0; i < n_commits; i++)
    {
      g_autoptr (BzCommit) commit = NULL;
      GtkWidget *row              = NULL;

      commit = g_list_model_get_item (commits, i);
      row    = create_commit_row (commit, first_radio, self, totals, counters);

      if (first_radio == NULL)
        first_radio = g_object_get_data (G_OBJECT (row), "radio");

      gtk_list_box_append (self->commits_box, row);
    }

  update_button_sensitivity (self);
  gtk_stack_set_visible_child_name (self->stack, "commits");
  return dex_future_new_true ();
}

static void
downgrade_cb (GtkButton       *button,
              BzDowngradePage *self)
{
  g_autoptr (BzResult) ui_result = NULL;
  g_autoptr (GVariant) param     = NULL;
  BzEntry        *ui_entry       = NULL;
  const char     *unique_id      = NULL;
  GVariantBuilder builder;

  g_return_if_fail (BZ_IS_DOWNGRADE_PAGE (self));
  g_return_if_fail (self->selected_checksum != NULL);

  ui_result = bz_entry_group_dup_ui_entry (self->entry_group);
  if (ui_result == NULL)
    return;

  ui_entry = bz_result_get_object (ui_result);
  if (ui_entry == NULL)
    return;

  unique_id = bz_entry_get_unique_id (ui_entry);
  if (unique_id == NULL)
    return;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
  g_variant_builder_add (&builder, "(ss)", unique_id, self->selected_checksum);
  param = g_variant_builder_end (&builder);

  gtk_widget_activate_action_variant (GTK_WIDGET (self), "window.update-entries", param);
  gtk_widget_activate_action (GTK_WIDGET (self), "sheet.close", NULL);
  gtk_widget_activate_action (GTK_WIDGET (self), "window.full-view-reset-focus", NULL);
}

static DexFuture *
begin_fetch_fiber (BeginFetchData *data)
{
  g_autoptr (BzDowngradePage) self     = NULL;
  g_autoptr (GError) local_error       = NULL;
  g_autoptr (GListStore) store         = NULL;
  g_autoptr (DexFuture) history_future = NULL;
  BzBackend         *backend           = NULL;
  BzFlatpakInstance *flatpak           = NULL;
  BzFlatpakEntry    *flatpak_entry     = NULL;
  FlatpakRef        *ref               = NULL;
  gboolean           user              = FALSE;
  guint              n_entries         = 0;

  self = g_weak_ref_get (&data->page_wr);
  if (self == NULL)
    return dex_future_new_true ();

  store = dex_await_object (bz_entry_group_dup_all_into_store (data->entry_group), &local_error);

  n_entries = g_list_model_get_n_items (G_LIST_MODEL (store));
  for (guint i = 0; i < n_entries; i++)
    {
      g_autoptr (BzEntry) entry = NULL;

      entry = g_list_model_get_item (G_LIST_MODEL (store), i);

      if (!bz_entry_is_installed (entry))
        continue;

      flatpak_entry = BZ_FLATPAK_ENTRY (entry);
      break;
    }

  backend = bz_state_info_get_backend (bz_state_info_get_default ());

  flatpak = BZ_FLATPAK_INSTANCE (backend);
  ref     = bz_flatpak_entry_get_ref (flatpak_entry);
  user    = bz_flatpak_entry_is_user (flatpak_entry);

  history_future = bz_flatpak_instance_get_commit_history (flatpak, ref, user, NULL);
  history_future = dex_future_then (
      g_steal_pointer (&history_future),
      (DexFutureCallback) on_commit_history_done,
      bz_track_weak (self),
      bz_weak_release);

  dex_clear (&self->fetch_future);
  self->fetch_future = g_steal_pointer (&history_future);
  dex_future_disown (dex_ref (self->fetch_future));
  return dex_future_new_true ();
}

static void
begin_fetch (BzDowngradePage *self)
{
  g_autoptr (BeginFetchData) data = NULL;
  g_autoptr (DexFuture) future    = NULL;

  data = begin_fetch_data_new ();
  g_weak_ref_init (&data->page_wr, self);
  data->entry_group = g_object_ref (self->entry_group);

  future = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      dex_get_min_stack_size (),
      (DexFiberFunc) begin_fetch_fiber,
      g_steal_pointer (&data),
      begin_fetch_data_unref);

  dex_clear (&self->fetch_future);
  self->fetch_future = g_steal_pointer (&future);
  dex_future_disown (dex_ref (self->fetch_future));
}
