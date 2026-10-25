/* bz-safety-dialog.c
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

#include "bz-app-permissions.h"
#include "bz-context-row.h"
#include "bz-entry.h"
#include "bz-lozenge.h"
#include "bz-safety-dialog.h"
#include <bzvala.h>
#include "safety-calculator.h"
#include "template-callbacks.h"

struct _BzSafetyDialog
{
  AdwBin parent_instance;

  BzEntry *entry;

  gboolean has_sandbox_escape;
  gboolean owns_dialog;

  BzLozenge   *lozenge;
  GtkListBox  *permissions_list;
  GtkStack    *stack;
  GtkBox      *global_box;
  GtkLabel    *arbitrary_description;
};

G_DEFINE_FINAL_TYPE (BzSafetyDialog, bz_safety_dialog, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_ENTRY,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

static void update_permissions_list (BzSafetyDialog *self);
static void animate_to_page (BzSafetyDialog *self, guint page_index);

static void
bz_safety_dialog_dispose (GObject *object)
{
  BzSafetyDialog *self = BZ_SAFETY_DIALOG (object);

  g_clear_object (&self->entry);

  G_OBJECT_CLASS (bz_safety_dialog_parent_class)->dispose (object);
}

static void
bz_safety_dialog_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzSafetyDialog *self = BZ_SAFETY_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_value_set_object (value, self->entry);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_safety_dialog_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzSafetyDialog *self;

  self = BZ_SAFETY_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_clear_object (&self->entry);
      self->entry = g_value_dup_object (value);
      update_permissions_list (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
get_target_size (BzSafetyDialog *self,
                 guint           page_index,
                 int            *out_width,
                 int            *out_height)
{
  if (page_index == 0)
    {
      *out_width  = 450;
      *out_height = bz_dialog_measure_height (GTK_WIDGET (self->global_box), *out_width, 48, 100, 600);
    }
  else
    {
      *out_width  = 640;
      *out_height = 576;
    }
}

static void
animate_to_page (BzSafetyDialog *self,
                 guint           page_index)
{
  GtkWidget *dialog        = NULL;
  int        target_width  = 0;
  int        target_height = 0;

  if (!self->owns_dialog)
    return;

  dialog = gtk_widget_get_ancestor (GTK_WIDGET (self), BZ_TYPE_DIALOG);
  if (dialog == NULL)
    return;

  get_target_size (self, page_index, &target_width, &target_height);
  bz_dialog_resize_to (BZ_DIALOG (dialog), target_width, target_height);
}

static void
on_dialog_map (BzSafetyDialog *self)
{
  guint page_index = self->has_sandbox_escape ? 0 : 1;

  if (page_index == 1)
    gtk_stack_set_visible_child_full (self->stack, "permissions", GTK_STACK_TRANSITION_TYPE_NONE);

  animate_to_page (self, page_index);
}

static void
next_page (BzSafetyDialog *self,
           GtkButton      *button)
{
  gtk_accessible_announce (GTK_ACCESSIBLE (self),
                           bz_lozenge_get_title (self->lozenge),
                           GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);

  gtk_stack_set_visible_child_name (self->stack, "permissions");
  animate_to_page (self, 1);
}

static void
bz_safety_dialog_class_init (BzSafetyDialogClass *klass)
{
  GObjectClass   *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_safety_dialog_dispose;
  object_class->get_property = bz_safety_dialog_get_property;
  object_class->set_property = bz_safety_dialog_set_property;

  props[PROP_ENTRY] =
      g_param_spec_object ("entry",
                           NULL, NULL,
                           BZ_TYPE_ENTRY,
                           G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_LOZENGE);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/io/github/kolunmi/Bazaar/bz-safety-dialog.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);

  gtk_widget_class_bind_template_child (widget_class, BzSafetyDialog, lozenge);
  gtk_widget_class_bind_template_child (widget_class, BzSafetyDialog, permissions_list);
  gtk_widget_class_bind_template_child (widget_class, BzSafetyDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, BzSafetyDialog, global_box);
  gtk_widget_class_bind_template_child (widget_class, BzSafetyDialog, arbitrary_description);
  gtk_widget_class_bind_template_callback (widget_class, next_page);
}

static void
bz_safety_dialog_init (BzSafetyDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  g_signal_connect_swapped (self, "map", G_CALLBACK (on_dialog_map), self);
}

AdwDialog *
bz_safety_dialog_new (BzEntry *entry)
{
  BzSafetyDialog  *widget      = NULL;
  AdwDialog       *dialog      = NULL;
  g_autofree char *description = NULL;

  widget              = g_object_new (BZ_TYPE_SAFETY_DIALOG, NULL);
  widget->owns_dialog = TRUE;

  dialog = ADW_DIALOG (bz_dialog_new ());

  adw_dialog_set_title (dialog, _ ("Safety"));
  adw_dialog_set_content_height (dialog, 576);
  adw_dialog_set_content_width (dialog, 640);
  adw_dialog_set_child (dialog, GTK_WIDGET (widget));

  g_object_set (widget, "entry", entry, NULL);

  if (widget->has_sandbox_escape)
    description = g_strdup_printf ("%s. %s",
                                   _ ("Arbitrary Permissions"),
                                   gtk_label_get_text (widget->arbitrary_description));
  else
    description = g_strdup (bz_lozenge_get_title (widget->lozenge));

  gtk_accessible_update_property (GTK_ACCESSIBLE (dialog),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, description,
                                  -1);

  return dialog;
}

AdwNavigationPage *
bz_safety_dialog_page_new (BzEntry *entry)
{
  BzSafetyDialog    *widget = NULL;
  AdwNavigationPage *page   = NULL;

  widget = g_object_new (BZ_TYPE_SAFETY_DIALOG, "entry", entry, NULL);
  page   = adw_navigation_page_new (GTK_WIDGET (widget), _ ("Safety"));
  adw_navigation_page_set_tag (page, "safety");

  return page;
}

static void
update_permissions_list (BzSafetyDialog *self)
{
  const char      *icon_names[2];
  const char      *app_name                = NULL;
  g_autofree char *title_text              = NULL;
  BzImportance     importance              = BZ_IMPORTANCE_UNIMPORTANT;
  GtkWidget       *child                   = NULL;
  g_autoptr (GListModel) model             = NULL;
  guint n_items                            = 0;
  g_autoptr (BzAppPermissions) permissions = NULL;
  BzAppPermissionsFlags perm_flags         = BZ_APP_PERMISSIONS_FLAGS_NONE;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->permissions_list))) != NULL)
    gtk_list_box_remove (self->permissions_list, child);

  if (self->entry == NULL)
    return;

  g_object_get (self->entry, "permissions", &permissions, NULL);
  if (permissions != NULL)
    perm_flags = bz_app_permissions_get_flags (permissions);
  self->has_sandbox_escape = (perm_flags & BZ_APP_PERMISSIONS_FLAGS_ESCAPE_SANDBOX) != 0;

  app_name   = bz_entry_get_title (self->entry);
  model      = bz_safety_calculator_analyze_entry (self->entry);
  importance = bz_safety_calculator_calculate_rating (self->entry);
  n_items    = g_list_model_get_n_items (model);

  for (gint level = BZ_IMPORTANCE_IMPORTANT; level >= BZ_IMPORTANCE_UNIMPORTANT; level--)
    {
      for (gint j = 0; j < n_items; j++)
        {
          g_autoptr (BzSafetyRow) row_data = NULL;
          AdwActionRow *row                = NULL;
          BzImportance  row_importance     = 0;

          row_data       = g_list_model_get_item (model, j);
          row_importance = bz_safety_row_get_importance (row_data);
          if (row_importance != level)
            continue;
          row = bz_context_row_new (bz_safety_row_get_icon_name (row_data),
                                    bz_safety_row_get_importance (row_data),
                                    bz_safety_row_get_title (row_data),
                                    bz_safety_row_get_subtitle (row_data));
          gtk_list_box_append (self->permissions_list, GTK_WIDGET (row));
        }
    }

  switch (importance)
    {
    case BZ_IMPORTANCE_UNIMPORTANT:
      icon_names[0] = "app-safety-ok-symbolic";
      icon_names[1] = NULL;
      title_text    = g_strdup_printf (_ ("%s is Safe"), app_name);
      break;
    case BZ_IMPORTANCE_NEUTRAL:
      icon_names[0] = "app-safety-ok-symbolic";
      icon_names[1] = NULL;
      title_text    = g_strdup_printf (_ ("%s has no Unsafe Permissions"), app_name);
      break;
    case BZ_IMPORTANCE_INFORMATION:
      icon_names[0] = "app-safety-ok-symbolic";
      icon_names[1] = NULL;
      title_text    = g_strdup_printf (_ ("%s is Probably Safe"), app_name);
      break;
    case BZ_IMPORTANCE_WARNING:
      icon_names[0] = "app-safety-unknown-symbolic";
      icon_names[1] = NULL;
      title_text    = g_strdup_printf (_ ("%s is Possibly Unsafe"), app_name);
      break;
    case BZ_IMPORTANCE_IMPORTANT:
      icon_names[0] = "app-safety-unsafe-symbolic";
      icon_names[1] = NULL;
      title_text    = g_strdup_printf (_ ("%s is Unsafe"), app_name);
      break;
    default:
      g_assert_not_reached ();
    }

  bz_lozenge_set_icon_names (self->lozenge, icon_names);
  bz_lozenge_set_title (self->lozenge, title_text);
  bz_lozenge_set_importance (self->lozenge, importance);
}
