/* bz-truck-paintable.c
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

#include "bz-truck-paintable.h"
#include <adwaita.h>
#include <librsvg/rsvg.h>
#include <math.h>
#include <string.h>

static const char *TRUCK_SVG_PATH = "/io/github/kolunmi/Bazaar/assets/io.github.kolumni.Bazaar.Truck.svg";
static const char *GREEN_COLORS[] = { "#0F402A", "#2EC27E", "#8FF0A4" };
static const double SOURCE_R = 0x2E / 255.0;
static const double SOURCE_G = 0xC2 / 255.0;
static const double SOURCE_B = 0x7E / 255.0;

typedef struct
{
  double h;
  double s;
  double v;
} HSV;

struct _BzTruckPaintable
{
  GObject parent_instance;

  RsvgHandle *handle;
  gulong      accent_signal_id;
};

static void bz_truck_paintable_paintable_init (GdkPaintableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (BzTruckPaintable, bz_truck_paintable, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, bz_truck_paintable_paintable_init))

static void
rgb_to_hsv (double  r,
            double  g,
            double  b,
            HSV    *hsv)
{
  double max;
  double min;
  double delta;

  max = fmax (fmax (r, g), b);
  min = fmin (fmin (r, g), b);
  delta = max - min;

  hsv->v = max;
  hsv->s = (max == 0) ? 0 : delta / max;

  if (delta == 0)
    {
      hsv->h = 0;
    }
  else if (max == r)
    {
      hsv->h = fmod ((g - b) / delta + (g < b ? 6 : 0), 6) / 6.0;
    }
  else if (max == g)
    {
      hsv->h = ((b - r) / delta + 2) / 6.0;
    }
  else
    {
      hsv->h = ((r - g) / delta + 4) / 6.0;
    }
}

static void
hsv_to_rgb (HSV    *hsv,
            double *r,
            double *g,
            double *b)
{
  double h;
  double s;
  double v;
  int i;
  double f;
  double p;
  double q;
  double t;

  h = hsv->h * 6.0;
  s = hsv->s;
  v = hsv->v;

  i = (int) floor (h);
  f = h - i;
  p = v * (1 - s);
  q = v * (1 - f * s);
  t = v * (1 - (1 - f) * s);

  switch (i % 6)
    {
    case 0:
      *r = v;
      *g = t;
      *b = p;
      break;
    case 1:
      *r = q;
      *g = v;
      *b = p;
      break;
    case 2:
      *r = p;
      *g = v;
      *b = t;
      break;
    case 3:
      *r = p;
      *g = q;
      *b = v;
      break;
    case 4:
      *r = t;
      *g = p;
      *b = v;
      break;
    case 5:
      *r = v;
      *g = p;
      *b = q;
      break;
    default:
      *r = 0;
      *g = 0;
      *b = 0;
      break;
    }
}

static char *
recolor_svg (const char *svg_content,
             const char *target_hex)
{
  unsigned int hex_val;
  double target_r;
  double target_g;
  double target_b;
  HSV target_hsv;
  gboolean is_blueish;
  char *new_svg;
  HSV source_hsv;
  double hue_shift;
  int i;

  hex_val = strtol (target_hex + 1, NULL, 16);
  target_r = ((hex_val >> 16) & 0xFF) / 255.0;
  target_g = ((hex_val >> 8) & 0xFF) / 255.0;
  target_b = (hex_val & 0xFF) / 255.0;

  rgb_to_hsv (target_r, target_g, target_b, &target_hsv);

  is_blueish = (0.55 <= target_hsv.h && target_hsv.h <= 0.60) &&
               (0.40 <= target_hsv.s && target_hsv.s <= 1.0) &&
               (0.50 <= target_hsv.v && target_hsv.v <= 1.0);

  if (is_blueish)
    return g_strdup (svg_content);

  new_svg = g_strdup (svg_content);

  rgb_to_hsv (SOURCE_R, SOURCE_G, SOURCE_B, &source_hsv);

  hue_shift = target_hsv.h - source_hsv.h;

  for (i = 0; i < 3; i++)
    {
      unsigned int color_val;
      int r;
      int g;
      int b;
      HSV color_hsv;
      double new_r;
      double new_g;
      double new_b;
      char new_color[8];
      char *pos;

      color_val = strtol (GREEN_COLORS[i] + 1, NULL, 16);
      r = (color_val >> 16) & 0xFF;
      g = (color_val >> 8) & 0xFF;
      b = color_val & 0xFF;

      rgb_to_hsv (r / 255.0, g / 255.0, b / 255.0, &color_hsv);

      color_hsv.h = fmod (color_hsv.h + hue_shift + 1.0, 1.0);
      color_hsv.s = fmin (1.0, fmax (0.0, color_hsv.s * (target_hsv.s / 0.76)));
      color_hsv.v = fmin (1.0, fmax (0.0, color_hsv.v * (target_hsv.v / 0.76)));

      hsv_to_rgb (&color_hsv, &new_r, &new_g, &new_b);

      snprintf (new_color, 8, "#%02X%02X%02X",
                (int) (new_r * 255), (int) (new_g * 255), (int) (new_b * 255));

      pos = new_svg;
      while ((pos = strcasestr (pos, GREEN_COLORS[i])) != NULL)
        {
          memcpy (pos, new_color, 7);
          pos += 7;
        }
    }

  return new_svg;
}

static void
update_svg_from_accent (BzTruckPaintable *self)
{
  AdwStyleManager *style_manager;
  GdkRGBA *accent;
  char hex[8];
  GError *error;
  GBytes *bytes;
  gsize length;
  const gchar *svg_data;
  char *recolored;

  style_manager = adw_style_manager_get_default ();
  accent = adw_style_manager_get_accent_color_rgba (style_manager);

  snprintf (hex, 8, "#%02X%02X%02X",
            (int) (accent->red * 255),
            (int) (accent->green * 255),
            (int) (accent->blue * 255));

  error = NULL;
  bytes = g_resources_lookup_data (TRUCK_SVG_PATH,
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   &error);

  if (!bytes)
    {
      g_error_free (error);
      return;
    }

  svg_data = g_bytes_get_data (bytes, &length);

  recolored = recolor_svg (svg_data, hex);
  g_bytes_unref (bytes);

  if (self->handle)
    g_object_unref (self->handle);

  self->handle = rsvg_handle_new_from_data ((guint8 *) recolored, strlen (recolored), &error);
  g_free (recolored);

  if (self->handle)
    gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static void
on_accent_color_changed (AdwStyleManager *style_manager,
                         GParamSpec      *pspec,
                         gpointer         user_data)
{
  BzTruckPaintable *self;

  self = BZ_TRUCK_PAINTABLE (user_data);
  update_svg_from_accent (self);
}

static void
bz_truck_paintable_snapshot (GdkPaintable *paintable,
                             GdkSnapshot  *snapshot,
                             double        width,
                             double        height)
{
  BzTruckPaintable *self;
  graphene_rect_t bounds;
  cairo_t *cr;
  double svg_width;
  double svg_height;
  double scale_x;
  double scale_y;
  double scale;
  RsvgRectangle viewport;

  self = BZ_TRUCK_PAINTABLE (paintable);

  if (!self->handle)
    return;

  bounds = GRAPHENE_RECT_INIT (0, 0, width, height);
  cr = gtk_snapshot_append_cairo (GTK_SNAPSHOT (snapshot), &bounds);

  rsvg_handle_get_intrinsic_size_in_pixels (self->handle, &svg_width, &svg_height);

  scale_x = width / svg_width;
  scale_y = height / svg_height;
  scale = fmin (scale_x, scale_y);

  cairo_scale (cr, scale, scale);

  viewport.x = 0;
  viewport.y = 0;
  viewport.width = svg_width;
  viewport.height = svg_height;
  rsvg_handle_render_document (self->handle, cr, &viewport, NULL);

  cairo_destroy (cr);
}

static int
bz_truck_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  BzTruckPaintable *self;
  double width;
  double height;

  self = BZ_TRUCK_PAINTABLE (paintable);

  if (!self->handle)
    return 0;

  rsvg_handle_get_intrinsic_size_in_pixels (self->handle, &width, &height);
  return (int) width;
}

static int
bz_truck_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  BzTruckPaintable *self;
  double width;
  double height;

  self = BZ_TRUCK_PAINTABLE (paintable);

  if (!self->handle)
    return 0;

  rsvg_handle_get_intrinsic_size_in_pixels (self->handle, &width, &height);
  return (int) height;
}

static double
bz_truck_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  BzTruckPaintable *self;
  double width;
  double height;

  self = BZ_TRUCK_PAINTABLE (paintable);

  if (!self->handle)
    return 0.0;

  rsvg_handle_get_intrinsic_size_in_pixels (self->handle, &width, &height);

  if (height == 0)
    return 0.0;

  return width / height;
}

static void
bz_truck_paintable_dispose (GObject *object)
{
  BzTruckPaintable *self;
  AdwStyleManager *style_manager;

  self = BZ_TRUCK_PAINTABLE (object);

  if (self->accent_signal_id)
    {
      style_manager = adw_style_manager_get_default ();
      g_signal_handler_disconnect (style_manager, self->accent_signal_id);
      self->accent_signal_id = 0;
    }

  g_clear_object (&self->handle);

  G_OBJECT_CLASS (bz_truck_paintable_parent_class)->dispose (object);
}

static void
bz_truck_paintable_class_init (BzTruckPaintableClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = bz_truck_paintable_dispose;
}

static void
bz_truck_paintable_init (BzTruckPaintable *self)
{
  AdwStyleManager *style_manager;

  self->handle = NULL;
  self->accent_signal_id = 0;

  style_manager = adw_style_manager_get_default ();
  self->accent_signal_id = g_signal_connect (style_manager,
                                             "notify::accent-color",
                                             G_CALLBACK (on_accent_color_changed),
                                             self);

  update_svg_from_accent (self);
}

static void
bz_truck_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = bz_truck_paintable_snapshot;
  iface->get_intrinsic_width = bz_truck_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = bz_truck_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = bz_truck_paintable_get_intrinsic_aspect_ratio;
}

BzTruckPaintable *
bz_truck_paintable_new (void)
{
  return g_object_new (BZ_TYPE_TRUCK_PAINTABLE, NULL);
}
