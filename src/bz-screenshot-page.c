/* bz-screenshot-page.c
 *
 * Copyright 2025 Alexander Vanhee
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

#include "bz-screenshot-page.h"
#include "bz-screenshot.h"
#include "bz-zoom.h"
#include <glib/gi18n.h>

struct _BzScreenshotPage
{
  AdwBin parent_instance;

  AdwCarousel     *carousel;
  AdwToastOverlay *toast_overlay;

  GListModel *screenshots;
  GListModel *captions;
  guint       current_index;
  guint       initial_index;

  gboolean is_zoomed;

  GtkWidget         *source_widget;
  GdkTexture        *source_texture;
  graphene_rect_t    source_bounds_at_map;
  AdwTimedAnimation *animation;
  double             animation_progress;
  gboolean           closing;
};

G_DEFINE_FINAL_TYPE (BzScreenshotPage, bz_screenshot_page, ADW_TYPE_BIN)

static void on_zoom_level_changed (BzZoom           *zoom,
                                   GParamSpec       *pspec,
                                   BzScreenshotPage *self);

enum
{
  PROP_0,

  PROP_SCREENSHOTS,
  PROP_CURRENT_INDEX,
  PROP_CURRENT_CAPTION,
  PROP_IS_ZOOMED,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static GdkTexture *
render_widget_to_texture (GtkWidget *widget)
{
  g_autoptr (GtkWidgetPaintable) paintable = NULL;
  g_autoptr (GtkSnapshot) snapshot         = NULL;
  g_autoptr (GskRenderNode) node           = NULL;
  GtkNative  *native                       = NULL;
  GdkTexture *texture                      = NULL;

  paintable = GTK_WIDGET_PAINTABLE (gtk_widget_paintable_new (widget));
  snapshot  = gtk_snapshot_new ();

  gdk_paintable_snapshot (GDK_PAINTABLE (paintable), snapshot,
      gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (paintable)),
      gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (paintable)));

  node   = gtk_snapshot_to_node (snapshot);
  native = gtk_widget_get_native (widget);

  if (node != NULL && native != NULL)
    texture = gsk_renderer_render_texture (
        gtk_native_get_renderer (native), node, NULL);

  return texture;
}

static void
on_animation_value (AdwTimedAnimation *animation,
                    GParamSpec        *pspec,
                    BzScreenshotPage  *self)
{
  self->animation_progress = adw_animation_get_value (ADW_ANIMATION (animation));
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
on_close_animation_done (AdwAnimation     *animation,
                         BzScreenshotPage *self)
{
  GtkWidget *parent = NULL;

  if (self->source_widget != NULL)
    gtk_widget_set_opacity (self->source_widget, 1.0);

  parent = gtk_widget_get_parent (GTK_WIDGET (self));
  if (parent != NULL)
    gtk_overlay_remove_overlay (GTK_OVERLAY (parent), GTK_WIDGET (self));
}

static void
back_clicked (BzScreenshotPage *self)
{
  AdwAnimationTarget *target = NULL;
  GtkWidget          *parent = NULL;

  if (self->closing)
    return;

  if (self->source_widget != NULL)
    {
      self->closing = TRUE;

      if (self->animation != NULL)
        {
          g_signal_handlers_disconnect_by_func (self->animation, on_animation_value, self);
          g_clear_object (&self->animation);
        }

      target = adw_callback_animation_target_new (
          (AdwAnimationTargetFunc) gtk_widget_queue_draw, self, NULL);

      self->animation = ADW_TIMED_ANIMATION (
          adw_timed_animation_new (GTK_WIDGET (self), 1.0, 0.0, 250, target));

      adw_timed_animation_set_easing (self->animation, ADW_EASE_IN_QUART);

      g_signal_connect (self->animation, "notify::value", G_CALLBACK (on_animation_value), self);
      g_signal_connect (self->animation, "done", G_CALLBACK (on_close_animation_done), self);

      adw_animation_play (ADW_ANIMATION (self->animation));
    }
  else
    {
      if (self->source_widget != NULL)
        gtk_widget_set_opacity (self->source_widget, 1.0);

      parent = gtk_widget_get_parent (GTK_WIDGET (self));
      if (parent != NULL)
        gtk_overlay_remove_overlay (GTK_OVERLAY (parent), GTK_WIDGET (self));
    }
}

static void
bz_screenshot_page_map (GtkWidget *widget)
{
  BzScreenshotPage   *self   = BZ_SCREENSHOT_PAGE (widget);
  AdwAnimationTarget *target = NULL;

  GTK_WIDGET_CLASS (bz_screenshot_page_parent_class)->map (widget);

  if (self->source_widget != NULL)
    {
      self->source_texture = render_widget_to_texture (self->source_widget);

      if (!gtk_widget_compute_bounds (self->source_widget, widget, &self->source_bounds_at_map))
        graphene_rect_init (&self->source_bounds_at_map, 0, 0,
                            gtk_widget_get_width (widget), gtk_widget_get_height (widget));

      gtk_widget_set_opacity (self->source_widget, 0.0);
    }

  self->animation_progress = 0.0;
  self->closing            = FALSE;

  target = adw_callback_animation_target_new ((AdwAnimationTargetFunc) gtk_widget_queue_draw, self, NULL);
  self->animation = ADW_TIMED_ANIMATION (adw_timed_animation_new (widget, 0.0, 1.0, 300, target));
  adw_timed_animation_set_easing (self->animation, ADW_EASE_OUT_QUART);

  g_signal_connect (self->animation, "notify::value", G_CALLBACK (on_animation_value), self);

  adw_animation_play (ADW_ANIMATION (self->animation));
}

static void
bz_screenshot_page_unmap (GtkWidget *widget)
{
  BzScreenshotPage *self = BZ_SCREENSHOT_PAGE (widget);

  if (self->source_widget != NULL && !self->closing)
    gtk_widget_set_opacity (self->source_widget, 1.0);

  GTK_WIDGET_CLASS (bz_screenshot_page_parent_class)->unmap (widget);
}

static void
bz_screenshot_page_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  BzScreenshotPage *self     = BZ_SCREENSHOT_PAGE (widget);
  double            progress = self->animation_progress;
  int               width    = gtk_widget_get_width (widget);
  int               height   = gtk_widget_get_height (widget);
  float             rev;
  float             x, y, w, h;
  graphene_rect_t   lerped;

  if (self->source_texture == NULL || progress >= 1.0)
    {
      GTK_WIDGET_CLASS (bz_screenshot_page_parent_class)->snapshot (widget, snapshot);
      return;
    }

  rev = (float) (1.0 - progress);

  x = self->source_bounds_at_map.origin.x * rev + 0.0f * (float) progress;
  y = self->source_bounds_at_map.origin.y * rev + 0.0f * (float) progress;
  w = self->source_bounds_at_map.size.width * rev + (float) width * (float) progress;
  h = self->source_bounds_at_map.size.height * rev + (float) height * (float) progress;

  graphene_rect_init (&lerped, x, y, w, h);

  gtk_snapshot_push_clip (snapshot, &lerped);
  gtk_snapshot_push_cross_fade (snapshot, progress);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
  gtk_snapshot_scale (snapshot,
                      w / self->source_bounds_at_map.size.width,
                      h / self->source_bounds_at_map.size.height);
  gdk_paintable_snapshot (GDK_PAINTABLE (self->source_texture), snapshot,
                          self->source_bounds_at_map.size.width,
                          self->source_bounds_at_map.size.height);
  gtk_snapshot_restore (snapshot);

  gtk_snapshot_pop (snapshot);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
  gtk_snapshot_scale (snapshot,
                      w / (float) width,
                      h / (float) height);
  GTK_WIDGET_CLASS (bz_screenshot_page_parent_class)->snapshot (widget, snapshot);
  gtk_snapshot_restore (snapshot);

  gtk_snapshot_pop (snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
bz_screenshot_page_dispose (GObject *object)
{
  BzScreenshotPage *self = BZ_SCREENSHOT_PAGE (object);

  g_clear_object (&self->screenshots);
  g_clear_object (&self->captions);
  g_clear_object (&self->source_texture);
  g_clear_object (&self->animation);

  G_OBJECT_CLASS (bz_screenshot_page_parent_class)->dispose (object);
}

static void
bz_screenshot_page_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  BzScreenshotPage *self = BZ_SCREENSHOT_PAGE (object);

  switch (prop_id)
    {
    case PROP_SCREENSHOTS:
      g_value_set_object (value, self->screenshots);
      break;
    case PROP_CURRENT_INDEX:
      g_value_set_uint (value, self->current_index);
      break;
    case PROP_CURRENT_CAPTION:
      {
        const char *caption = bz_screenshot_page_get_current_caption (self);
        g_value_set_string (value, caption);
      }
      break;
    case PROP_IS_ZOOMED:
      g_value_set_boolean (value, self->is_zoomed);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
populate_carousel (BzScreenshotPage *self)
{
  guint n_items = 0;
  guint i       = 0;

  if (self->screenshots == NULL)
    return;

  n_items = g_list_model_get_n_items (self->screenshots);
  if (n_items == 0)
    return;

  for (guint offset = 0; offset < n_items; offset++)
    {
      g_autoptr (BzAsyncTexture) async_texture = NULL;
      GtkWidget *zoom_widget                   = NULL;
      GtkWidget *screenshot                    = NULL;

      i = (self->initial_index + offset) % n_items;

      async_texture = g_list_model_get_item (self->screenshots, i);
      if (async_texture == NULL)
        continue;

      screenshot = bz_screenshot_new ();
      bz_screenshot_set_paintable (BZ_SCREENSHOT (screenshot), GDK_PAINTABLE (async_texture));
      bz_screenshot_set_rounded_corners (BZ_SCREENSHOT (screenshot), FALSE);
      gtk_widget_set_margin_top (screenshot, 25);
      gtk_widget_set_margin_bottom (screenshot, 25);
      gtk_widget_set_margin_start (screenshot, 25);
      gtk_widget_set_margin_end (screenshot, 25);

      zoom_widget = bz_zoom_new ();
      gtk_widget_set_hexpand (zoom_widget, TRUE);
      gtk_widget_set_vexpand (zoom_widget, TRUE);
      bz_zoom_set_child (BZ_ZOOM (zoom_widget), screenshot);

      adw_carousel_append (self->carousel, zoom_widget);
    }
}

static void
update_is_zoomed (BzScreenshotPage *self)
{
  GtkWidget *page       = NULL;
  BzZoom    *zoom       = NULL;
  double     zoom_level = 1.0;
  gboolean   was_zoomed = self->is_zoomed;
  guint      n_pages    = 0;

  n_pages = adw_carousel_get_n_pages (self->carousel);
  if (self->current_index >= n_pages)
    return;

  page = adw_carousel_get_nth_page (self->carousel, self->current_index);
  if (page != NULL && BZ_IS_ZOOM (page))
    {
      GtkWidget *screenshot = NULL;

      zoom = BZ_ZOOM (page);
      g_object_get (zoom, "zoom-level", &zoom_level, NULL);

      screenshot = bz_zoom_get_child (zoom);
      if (screenshot != NULL)
        bz_screenshot_set_filter (
            BZ_SCREENSHOT (screenshot),
            zoom_level <= 4.5
                ? GSK_SCALING_FILTER_TRILINEAR
                : GSK_SCALING_FILTER_NEAREST);
    }

  self->is_zoomed = zoom != NULL && bz_zoom_is_transformed (zoom);

  if (was_zoomed != self->is_zoomed)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_ZOOMED]);
}

static void
on_zoom_level_changed (BzZoom           *zoom,
                       GParamSpec       *pspec,
                       BzScreenshotPage *self)
{
  update_is_zoomed (self);
}

static void
connect_zoom_signal (BzScreenshotPage *self,
                     GtkWidget        *page)
{
  BzZoom *zoom = NULL;

  if (page != NULL && BZ_IS_ZOOM (page))
    {
      zoom = BZ_ZOOM (page);
      g_signal_connect (zoom, "notify::zoom-level",
                        G_CALLBACK (on_zoom_level_changed), self);
    }
}

static void
bz_screenshot_page_constructed (GObject *object)
{
  BzScreenshotPage *self = BZ_SCREENSHOT_PAGE (object);
  GtkWidget        *page = NULL;

  G_OBJECT_CLASS (bz_screenshot_page_parent_class)->constructed (object);

  populate_carousel (self);

  self->current_index = 0;

  page = adw_carousel_get_nth_page (self->carousel, 0);
  if (page != NULL)
    connect_zoom_signal (self, page);

  update_is_zoomed (self);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CURRENT_CAPTION]);
}

static void
bz_screenshot_page_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BzScreenshotPage *self = BZ_SCREENSHOT_PAGE (object);

  switch (prop_id)
    {
    case PROP_SCREENSHOTS:
      g_set_object (&self->screenshots, g_value_get_object (value));
      break;
    case PROP_CURRENT_INDEX:
      self->initial_index = g_value_get_uint (value);
      break;
    case PROP_CURRENT_CAPTION:
    case PROP_IS_ZOOMED:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
zoom_in_clicked (BzScreenshotPage *self)
{
  GtkWidget *page    = NULL;
  BzZoom    *zoom    = NULL;
  guint      n_pages = 0;

  n_pages = adw_carousel_get_n_pages (self->carousel);
  if (self->current_index >= n_pages)
    return;

  page = adw_carousel_get_nth_page (self->carousel, self->current_index);
  if (page == NULL || !BZ_IS_ZOOM (page))
    return;

  zoom = BZ_ZOOM (page);
  bz_zoom_zoom_in (zoom);
}

static void
zoom_out_clicked (BzScreenshotPage *self)
{
  GtkWidget *page    = NULL;
  BzZoom    *zoom    = NULL;
  guint      n_pages = 0;

  n_pages = adw_carousel_get_n_pages (self->carousel);
  if (self->current_index >= n_pages)
    return;

  page = adw_carousel_get_nth_page (self->carousel, self->current_index);
  if (page == NULL || !BZ_IS_ZOOM (page))
    return;

  zoom = BZ_ZOOM (page);
  bz_zoom_zoom_out (zoom);
}

static void
reset_zoom_clicked (BzScreenshotPage *self)
{
  GtkWidget *page    = NULL;
  BzZoom    *zoom    = NULL;
  guint      n_pages = 0;

  n_pages = adw_carousel_get_n_pages (self->carousel);
  if (self->current_index >= n_pages)
    return;

  page = adw_carousel_get_nth_page (self->carousel, self->current_index);
  if (page == NULL || !BZ_IS_ZOOM (page))
    return;

  zoom = BZ_ZOOM (page);
  bz_zoom_reset (zoom);
}

static void
previous_clicked (BzScreenshotPage *self)
{
  guint      n_pages = 0;
  GtkWidget *page    = NULL;

  n_pages = adw_carousel_get_n_pages (self->carousel);
  if (n_pages == 0)
    return;

  if (self->current_index > 0)
    page = adw_carousel_get_nth_page (self->carousel, self->current_index - 1);
  else
    page = adw_carousel_get_nth_page (self->carousel, n_pages - 1);

  if (page != NULL)
    adw_carousel_scroll_to (self->carousel, page, TRUE);
}

static void
next_clicked (BzScreenshotPage *self)
{
  guint      n_pages = 0;
  GtkWidget *page    = NULL;

  n_pages = adw_carousel_get_n_pages (self->carousel);
  if (n_pages == 0)
    return;

  if (self->current_index < n_pages - 1)
    page = adw_carousel_get_nth_page (self->carousel, self->current_index + 1);
  else
    page = adw_carousel_get_nth_page (self->carousel, 0);

  if (page != NULL)
    adw_carousel_scroll_to (self->carousel, page, TRUE);
}

static void
on_carousel_position_changed (AdwCarousel      *carousel,
                              GParamSpec       *pspec,
                              BzScreenshotPage *self)
{
  GtkWidget *old_page = NULL;
  GtkWidget *new_page = NULL;
  BzZoom    *old_zoom = NULL;
  guint      new_index;
  guint      n_pages;

  new_index = (guint) round (adw_carousel_get_position (carousel));
  n_pages   = adw_carousel_get_n_pages (carousel);

  if (new_index == self->current_index || new_index >= n_pages)
    return;

  if (self->current_index < n_pages)
    {
      old_page = adw_carousel_get_nth_page (carousel, self->current_index);
      if (old_page != NULL && BZ_IS_ZOOM (old_page))
        {
          old_zoom = BZ_ZOOM (old_page);
          g_signal_handlers_disconnect_by_func (old_zoom, on_zoom_level_changed, self);
          bz_zoom_reset (old_zoom);
        }
    }

  self->current_index = new_index;

  if (new_index < n_pages)
    {
      new_page = adw_carousel_get_nth_page (carousel, new_index);
      connect_zoom_signal (self, new_page);
    }

  update_is_zoomed (self);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CURRENT_INDEX]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CURRENT_CAPTION]);
}

static void
copy_clicked (BzScreenshotPage *self)
{
  g_autoptr (BzAsyncTexture) async_texture = NULL;
  g_autoptr (GdkTexture) texture           = NULL;
  GdkClipboard *clipboard;
  AdwToast     *toast        = NULL;
  guint         n_items      = 0;
  guint         actual_index = 0;

  if (self->screenshots == NULL)
    return;

  n_items = g_list_model_get_n_items (self->screenshots);
  if (n_items == 0)
    return;

  actual_index = (self->initial_index + self->current_index) % n_items;

  async_texture = g_list_model_get_item (self->screenshots, actual_index);
  if (async_texture == NULL)
    return;

  texture = bz_async_texture_dup_texture (async_texture);
  if (texture == NULL)
    return;

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  gdk_clipboard_set_texture (clipboard, texture);

  toast = adw_toast_new (_ ("Copied!"));
  adw_toast_set_timeout (toast, 1);
  adw_toast_overlay_add_toast (self->toast_overlay, toast);
}

static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                BzScreenshotPage      *self)
{
  if (keyval == GDK_KEY_Left)
    {
      previous_clicked (self);
      return TRUE;
    }
  else if (keyval == GDK_KEY_Right)
    {
      next_clicked (self);
      return TRUE;
    }

  return FALSE;
}

static gboolean
has_multiple_screenshots (GObject    *object,
                          GListModel *screenshots,
                          gpointer    user_data)
{
  if (screenshots == NULL)
    return FALSE;
  return g_list_model_get_n_items (screenshots) > 1;
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static gboolean
is_valid_string (gpointer    object,
                 const char *value)
{
  return value != NULL && *value != '\0';
}

static void
bz_screenshot_page_class_init (BzScreenshotPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_screenshot_page_dispose;
  object_class->constructed  = bz_screenshot_page_constructed;
  object_class->get_property = bz_screenshot_page_get_property;
  object_class->set_property = bz_screenshot_page_set_property;

  widget_class->map      = bz_screenshot_page_map;
  widget_class->unmap    = bz_screenshot_page_unmap;
  widget_class->snapshot = bz_screenshot_page_snapshot;

  props[PROP_SCREENSHOTS] =
      g_param_spec_object (
          "screenshots",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  props[PROP_CURRENT_INDEX] =
      g_param_spec_uint (
          "current-index",
          NULL, NULL,
          0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  props[PROP_CURRENT_CAPTION] =
      g_param_spec_string (
          "current-caption",
          NULL, NULL,
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_IS_ZOOMED] =
      g_param_spec_boolean (
          "is-zoomed",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_ZOOM);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-screenshot-page.ui");
  gtk_widget_class_bind_template_child (widget_class, BzScreenshotPage, carousel);
  gtk_widget_class_bind_template_child (widget_class, BzScreenshotPage, toast_overlay);
  gtk_widget_class_bind_template_callback (widget_class, back_clicked);
  gtk_widget_class_bind_template_callback (widget_class, zoom_in_clicked);
  gtk_widget_class_bind_template_callback (widget_class, zoom_out_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_carousel_position_changed);
  gtk_widget_class_bind_template_callback (widget_class, reset_zoom_clicked);
  gtk_widget_class_bind_template_callback (widget_class, copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, previous_clicked);
  gtk_widget_class_bind_template_callback (widget_class, next_clicked);
  gtk_widget_class_bind_template_callback (widget_class, has_multiple_screenshots);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_valid_string);
}

static void
bz_screenshot_page_init (BzScreenshotPage *self)
{
  GtkEventController *key_controller = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  key_controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_phase (key_controller, GTK_PHASE_CAPTURE);
  g_signal_connect (key_controller, "key-pressed",
                    G_CALLBACK (on_key_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), key_controller);
}

const char *
bz_screenshot_page_get_current_caption (BzScreenshotPage *self)
{
  g_autoptr (GtkStringObject) caption_obj = NULL;
  guint n_items                           = 0;
  guint actual_index                      = 0;

  g_return_val_if_fail (BZ_IS_SCREENSHOT_PAGE (self), NULL);

  if (self->captions == NULL)
    return "";

  n_items = g_list_model_get_n_items (self->captions);
  if (n_items == 0)
    return "";

  actual_index = (self->initial_index + self->current_index) % n_items;

  caption_obj = g_list_model_get_item (self->captions, actual_index);
  if (caption_obj == NULL)
    return "";

  return gtk_string_object_get_string (caption_obj);
}

void
bz_screenshot_page_set_captions (BzScreenshotPage *self,
                                 GListModel       *captions)
{
  g_return_if_fail (BZ_IS_SCREENSHOT_PAGE (self));

  g_set_object (&self->captions, captions);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CURRENT_CAPTION]);
}

void
bz_screenshot_page_close (BzScreenshotPage *self)
{
  g_return_if_fail (BZ_IS_SCREENSHOT_PAGE (self));
  back_clicked (self);
}

AdwBin *
bz_screenshot_page_new (GListModel *screenshots,
                        GListModel *captions,
                        guint       initial_index,
                        GtkWidget  *source_widget)
{
  BzScreenshotPage *page = g_object_new (
      BZ_TYPE_SCREENSHOT_PAGE,
      "screenshots", screenshots,
      "current-index", initial_index,
      NULL);

  page->source_widget = source_widget;

  if (captions != NULL)
    bz_screenshot_page_set_captions (page, captions);

  return ADW_BIN (page);
}
