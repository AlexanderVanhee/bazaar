/* bz-permission-entry-row.c
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

#include <glib/gi18n.h>

#include "bz-permission-entry-row.h"

enum
{
  SIGNAL_CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

struct _BzPermissionEntryRow
{
  AdwExpanderRow parent_instance;

  GPtrArray *entries;
};

G_DEFINE_FINAL_TYPE (BzPermissionEntryRow, bz_permission_entry_row, ADW_TYPE_EXPANDER_ROW)

static void
update_expandability (BzPermissionEntryRow *self)
{
  if (self->entries->len == 0)
    {
      adw_expander_row_set_expanded (ADW_EXPANDER_ROW (self), FALSE);
      adw_expander_row_set_enable_expansion (ADW_EXPANDER_ROW (self), FALSE);
    }
  else
    {
      adw_expander_row_set_enable_expansion (ADW_EXPANDER_ROW (self), TRUE);
    }
}

static void
remove_cb (GtkButton            *button,
           BzPermissionEntryRow *self)
{
  GtkWidget *row = NULL;

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), ADW_TYPE_ENTRY_ROW);

  if (row != NULL)
    {
      g_ptr_array_remove (self->entries, row);
      adw_expander_row_remove (ADW_EXPANDER_ROW (self), row);
      update_expandability (self);
      g_signal_emit (self, signals[SIGNAL_CHANGED], 0);
    }
}

static void
focus_leave_cb (GtkEventControllerFocus *controller,
                BzPermissionEntryRow    *self)
{
  g_signal_emit (self, signals[SIGNAL_CHANGED], 0);
}

static AdwEntryRow *
add_entry (BzPermissionEntryRow *self,
           const char           *text)
{
  AdwEntryRow             *row        = NULL;
  GtkButton               *button     = NULL;
  GtkEventControllerFocus *focus_ctrl = NULL;

  row = ADW_ENTRY_ROW (adw_entry_row_new ());

  if (text != NULL)
    gtk_editable_set_text (GTK_EDITABLE (row), text);

  button = GTK_BUTTON (gtk_button_new_from_icon_name ("cross-small-circle-outline-symbolic"));
  gtk_widget_add_css_class (GTK_WIDGET (button), "error");
  gtk_widget_set_valign (GTK_WIDGET (button), GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (GTK_WIDGET (button), "flat");
  adw_entry_row_add_suffix (row, GTK_WIDGET (button));

  g_signal_connect (button, "clicked",
                    G_CALLBACK (remove_cb), self);

  focus_ctrl = GTK_EVENT_CONTROLLER_FOCUS (gtk_event_controller_focus_new ());
  g_signal_connect (focus_ctrl, "leave",
                    G_CALLBACK (focus_leave_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (row),
                             GTK_EVENT_CONTROLLER (focus_ctrl));

  adw_expander_row_add_row (ADW_EXPANDER_ROW (self), GTK_WIDGET (row));
  // this cursed line sets first -> second -> first widget to invisible to properly hide the title...
  gtk_widget_set_visible (gtk_widget_get_next_sibling (gtk_widget_get_first_child (gtk_widget_get_next_sibling (gtk_widget_get_first_child (gtk_widget_get_first_child (GTK_WIDGET (row)))))), FALSE);

  g_ptr_array_add (self->entries, row);
  update_expandability (self);

  return row;
}

static void
add_cb (GtkButton            *button,
        BzPermissionEntryRow *self)
{
  AdwEntryRow *row = add_entry (self, NULL);
  adw_expander_row_set_expanded (ADW_EXPANDER_ROW (self), TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (row));
}

static void
bz_permission_entry_row_dispose (GObject *object)
{
  BzPermissionEntryRow *self = BZ_PERMISSION_ENTRY_ROW (object);

  g_clear_pointer (&self->entries, g_ptr_array_unref);

  G_OBJECT_CLASS (bz_permission_entry_row_parent_class)->dispose (object);
}

static void
bz_permission_entry_row_class_init (BzPermissionEntryRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = bz_permission_entry_row_dispose;

  signals[SIGNAL_CHANGED] =
      g_signal_new ("changed",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL, NULL,
                    G_TYPE_NONE, 0);
}

static void
bz_permission_entry_row_init (BzPermissionEntryRow *self)
{
  GtkButton *add_button = NULL;

  self->entries = g_ptr_array_new ();

  add_button = GTK_BUTTON (gtk_button_new_from_icon_name ("list-add-symbolic"));
  gtk_widget_set_valign (GTK_WIDGET (add_button), GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (GTK_WIDGET (add_button), "flat");
  adw_expander_row_add_suffix (ADW_EXPANDER_ROW (self), GTK_WIDGET (add_button));

  g_signal_connect (add_button, "clicked",
                    G_CALLBACK (add_cb), self);

  adw_expander_row_set_enable_expansion (ADW_EXPANDER_ROW (self), FALSE);
}

GtkWidget *
bz_permission_entry_row_new (const char *title,
                             const char *subtitle)
{
  BzPermissionEntryRow *self = NULL;

  self = g_object_new (BZ_TYPE_PERMISSION_ENTRY_ROW, NULL);

  if (title != NULL)
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title);

  if (subtitle != NULL)
    adw_expander_row_set_subtitle (ADW_EXPANDER_ROW (self), subtitle);

  return GTK_WIDGET (self);
}

GStrv
bz_permission_entry_row_get_values (BzPermissionEntryRow *self)
{
  GPtrArray *arr = NULL;

  g_return_val_if_fail (BZ_IS_PERMISSION_ENTRY_ROW (self), NULL);

  arr = g_ptr_array_new ();

  for (guint i = 0; i < self->entries->len; i++)
    {
      AdwEntryRow *row  = g_ptr_array_index (self->entries, i);
      const char  *text = gtk_editable_get_text (GTK_EDITABLE (row));

      if (text != NULL && text[0] != '\0')
        g_ptr_array_add (arr, g_strdup (text));
    }

  g_ptr_array_add (arr, NULL);

  return (GStrv) g_ptr_array_free (arr, FALSE);
}

void
bz_permission_entry_row_set_values (BzPermissionEntryRow *self,
                                    const char *const    *values)
{
  g_return_if_fail (BZ_IS_PERMISSION_ENTRY_ROW (self));

  for (guint i = self->entries->len; i > 0; i--)
    {
      GtkWidget *row = g_ptr_array_index (self->entries, i - 1);
      adw_expander_row_remove (ADW_EXPANDER_ROW (self), row);
    }

  g_ptr_array_set_size (self->entries, 0);

  if (values != NULL)
    {
      for (guint i = 0; values[i] != NULL; i++)
        add_entry (self, values[i]);
    }

  update_expandability (self);
}
