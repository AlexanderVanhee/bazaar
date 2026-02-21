/* bz-rich-app-tile.c
 *
 * Copyright 2025 Adam Masciola, Alexander Vanhee
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

#include "bz-rich-app-tile.h"
#include "bz-entry.h"
#include "bz-rounded-picture.h"
#include "bz-themed-entry-group-rect.h"
#include "bz-util.h"

struct _BzRichAppTile
{
  BzListTile    parent_instance;
  BzEntryGroup *group;

  GtkWidget *picture_box;
};

G_DEFINE_FINAL_TYPE (BzRichAppTile, bz_rich_app_tile, BZ_TYPE_LIST_TILE);

enum
{
  PROP_0,
  PROP_GROUP,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_INSTALL_CLICKED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
bz_rich_app_tile_dispose (GObject *object)
{
  BzRichAppTile *self = BZ_RICH_APP_TILE (object);

  g_clear_object (&self->group);

  G_OBJECT_CLASS (bz_rich_app_tile_parent_class)->dispose (object);
}

static void
bz_rich_app_tile_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzRichAppTile *self = BZ_RICH_APP_TILE (object);
  switch (prop_id)
    {
    case PROP_GROUP:
      g_value_set_object (value, bz_rich_app_tile_get_group (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_rich_app_tile_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzRichAppTile *self = BZ_RICH_APP_TILE (object);
  switch (prop_id)
    {
    case PROP_GROUP:
      bz_rich_app_tile_set_group (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static gboolean
is_zero (gpointer object,
         int      value)
{
  return value == 0;
}

static gboolean
logical_and (gpointer object,
             gboolean value1,
             gboolean value2)
{
  return value1 && value2;
}

static void
install_button_clicked_cb (BzRichAppTile *self,
                           GtkButton     *button)
{
  g_signal_emit (self, signals[SIGNAL_INSTALL_CLICKED], 0);
}

static void
bz_rich_app_tile_class_init (BzRichAppTileClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_rich_app_tile_set_property;
  object_class->get_property = bz_rich_app_tile_get_property;
  object_class->dispose      = bz_rich_app_tile_dispose;

  props[PROP_GROUP] =
      g_param_spec_object (
          "group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_INSTALL_CLICKED] =
      g_signal_new (
          "install-clicked",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          NULL,
          G_TYPE_NONE, 0);

  g_type_ensure (BZ_TYPE_LIST_TILE);
  g_type_ensure (BZ_TYPE_ROUNDED_PICTURE);
  g_type_ensure (BZ_TYPE_THEMED_ENTRY_GROUP_RECT);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-rich-app-tile.ui");
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
  gtk_widget_class_bind_template_callback (widget_class, logical_and);
  gtk_widget_class_bind_template_callback (widget_class, install_button_clicked_cb);
  gtk_widget_class_bind_template_child (widget_class, BzRichAppTile, picture_box);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_BUTTON);
}

static void
bz_rich_app_tile_init (BzRichAppTile *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_rich_app_tile_new (void)
{
  return g_object_new (BZ_TYPE_RICH_APP_TILE, NULL);
}

BzEntryGroup *
bz_rich_app_tile_get_group (BzRichAppTile *self)
{
  g_return_val_if_fail (BZ_IS_RICH_APP_TILE (self), NULL);
  return self->group;
}

void
bz_rich_app_tile_set_group (BzRichAppTile *self,
                            BzEntryGroup  *group)
{
  g_return_if_fail (BZ_IS_RICH_APP_TILE (self));

  g_clear_object (&self->group);

  if (group != NULL)
    self->group = g_object_ref (group);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_GROUP]);
}

