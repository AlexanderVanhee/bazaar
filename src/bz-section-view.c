/* bz-section-view.c
 *
 * Copyright 2025 Adam Masciola
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

#include <bge.h>

#include "bz-application.h"
#include "bz-curated-app-tile.h"
#include "bz-curated-section.h"
#include "bz-dynamic-list-view.h"
#include "bz-entry-group.h"
#include "bz-rich-app-tile.h"
#include "bz-section-view.h"
#include "bz-window.h"

struct _BzSectionView
{
  AdwBin parent_instance;

  BzCuratedSection *section;

  /* Template widgets */
  BgeMarkdownRender *subtitle;
};

G_DEFINE_FINAL_TYPE (BzSectionView, bz_section_view, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_SECTION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_GROUP_ACTIVATED,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
tile_clicked (BzEntryGroup *group,
              GtkButton    *button);

static void
bz_section_view_dispose (GObject *object)
{
  BzSectionView *self = BZ_SECTION_VIEW (object);

  g_clear_object (&self->section);

  G_OBJECT_CLASS (bz_section_view_parent_class)->dispose (object);
}

static void
bz_section_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  BzSectionView *self = BZ_SECTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_SECTION:
      g_value_set_object (value, bz_section_view_get_section (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_section_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BzSectionView *self = BZ_SECTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_SECTION:
      bz_section_view_set_section (self, g_value_get_object (value));
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
is_null_string (gpointer    object,
               const char *value)
{
  return value == NULL;
}

static char *
get_child_type (gpointer    object,
                const char *list_type)
{
  if (list_type != NULL && g_strcmp0 (list_type, "rich") == 0)
    return g_strdup ("BzRichAppTile");
  return g_strdup ("BzCuratedAppTile");
}

static GListModel *
convert_to_groups (gpointer    object,
                   GListModel *value)
{
  BzStateInfo             *info    = NULL;
  BzApplicationMapFactory *factory = NULL;

  info    = bz_state_info_get_default ();
  factory = bz_state_info_get_application_factory (info);

  return bz_application_map_factory_generate (factory, value);
}

static void
bind_widget_cb (BzSectionView     *self,
                BzCuratedAppTile  *tile,
                BzEntryGroup      *group,
                BzDynamicListView *view)
{
  g_signal_connect_swapped (tile, "clicked", G_CALLBACK (tile_clicked), group);
}

static void
unbind_widget_cb (BzSectionView     *self,
                  BzCuratedAppTile  *tile,
                  BzEntryGroup      *group,
                  BzDynamicListView *view)
{
  g_signal_handlers_disconnect_by_func (tile, G_CALLBACK (tile_clicked), group);
}

static void
install_all_clicked (BzSectionView *self,
                     GtkButton     *button)
{
  GtkWidget               *window   = NULL;
  BzCuratedAppidsInfo     *appids   = NULL;
  GListModel              *list     = NULL;
  guint                    n_appids = 0;
  BzStateInfo             *info     = NULL;
  BzApplicationMapFactory *factory  = NULL;
  g_autoptr (GListModel) groups     = NULL;

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), BZ_TYPE_WINDOW);
  if (window == NULL)
    return;

  /* If the button is visible and the user clicked it, this must be non-null */
  appids = bz_curated_section_get_appids (self->section);
  list   = bz_curated_appids_info_get_list (appids);
  if (list == NULL)
    return;
  n_appids = g_list_model_get_n_items (list);
  if (n_appids == 0)
    return;

  /* TODO: bind state via object properties */
  info    = bz_state_info_get_default ();
  factory = bz_state_info_get_application_factory (info);

  groups = bz_application_map_factory_generate (factory, list);
  /* TODO: use signals to chain up the blueprints; it is cleaner, but more
     work... :( */
  bz_window_bulk_install (BZ_WINDOW (window), groups);
}

static GtkWidget *
markdown_bind_inline_uri (BzSectionView     *self,
                          const char        *title,
                          const char        *src,
                          BgeMarkdownRender *markdown)
{
  if (src == NULL)
    return NULL;

  if (g_str_has_prefix (src, "appstream://"))
    {
      BzStateInfo             *info    = NULL;
      BzApplicationMapFactory *factory = NULL;
      g_autoptr (BzEntryGroup) group   = NULL;

      info    = bz_state_info_get_default ();
      factory = bz_state_info_get_application_factory (info);

      group = bz_application_map_factory_convert_one (
          factory,
          gtk_string_object_new (src + strlen ("appstream://")));
      if (group != NULL)
        {
          GtkWidget *tile = NULL;

          tile = bz_rich_app_tile_new ();
          bz_rich_app_tile_set_group (BZ_RICH_APP_TILE (tile), group);

          return tile;
        }
    }

  return NULL;
}

static void
bz_section_view_class_init (BzSectionViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_section_view_dispose;
  object_class->get_property = bz_section_view_get_property;
  object_class->set_property = bz_section_view_set_property;

  props[PROP_SECTION] =
      g_param_spec_object (
          "section",
          NULL, NULL,
          BZ_TYPE_CURATED_SECTION,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_GROUP_ACTIVATED] =
      g_signal_new (
          "group-activated",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY_GROUP);
  g_signal_set_va_marshaller (
      signals[SIGNAL_GROUP_ACTIVATED],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  g_type_ensure (BZ_TYPE_CURATED_APP_TILE);
  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-section-view.ui");
  gtk_widget_class_bind_template_child (widget_class, BzSectionView, subtitle);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, is_null_string);
  gtk_widget_class_bind_template_callback (widget_class, get_child_type);
  gtk_widget_class_bind_template_callback (widget_class, convert_to_groups);
  gtk_widget_class_bind_template_callback (widget_class, bind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, install_all_clicked);
  gtk_widget_class_bind_template_callback (widget_class, markdown_bind_inline_uri);
}

static void
bz_section_view_init (BzSectionView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_section_view_new (BzCuratedSection *section)
{
  return g_object_new (
      BZ_TYPE_SECTION_VIEW,
      "section", section,
      NULL);
}

void
bz_section_view_set_section (BzSectionView    *self,
                             BzCuratedSection *section)
{
  g_return_if_fail (BZ_IS_SECTION_VIEW (self));
  g_return_if_fail (section == NULL || BZ_IS_CURATED_SECTION (section));

  g_clear_object (&self->section);

  if (section != NULL)
    self->section = g_object_ref (section);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SECTION]);
}

BzCuratedSection *
bz_section_view_get_section (BzSectionView *self)
{
  g_return_val_if_fail (BZ_IS_SECTION_VIEW (self), NULL);
  return self->section;
}

static void
tile_clicked (BzEntryGroup *group,
              GtkButton    *button)
{
  GtkWidget *self = NULL;

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_SECTION_VIEW);
  g_signal_emit (self, signals[SIGNAL_GROUP_ACTIVATED], 0, group);
}
