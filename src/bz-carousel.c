/* bz-carousel.c
 *
 * Copyright 2025 Alexander Vanhee
 * Copyright (C) 2019 Alice Mikhaylenko <alicem@gnome.org>
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

#include "bz-carousel.h"

#include <adwaita.h>
#include <math.h>

#define SCROLL_TIMEOUT_DURATION 150

typedef struct {
  GtkWidget *widget;
  int position;
  gboolean visible;
  double size;
  double snap_point;
  gboolean adding;
  gboolean removing;

  gboolean shift_position;
  AdwAnimation *resize_animation;
} ChildInfo;

struct _BzCarousel
{
  GtkWidget parent_instance;

  GList *children;
  double distance;
  double position;
  guint spacing;
  gboolean uniform_spacing;
  GtkOrientation orientation;
  guint reveal_duration;

  double animation_source_position;
  AdwAnimation *animation;
  ChildInfo *animation_target_child;

  AdwSwipeTracker *tracker;

  gboolean allow_scroll_wheel;

  double position_shift;

  guint scroll_timeout_id;
  gboolean is_being_allocated;
};

static void bz_carousel_buildable_init (GtkBuildableIface *iface);
static void bz_carousel_swipeable_init (AdwSwipeableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (BzCarousel, bz_carousel, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, bz_carousel_buildable_init)
                               G_IMPLEMENT_INTERFACE (ADW_TYPE_SWIPEABLE, bz_carousel_swipeable_init))

static GtkBuildableIface *parent_buildable_iface;

enum {
  PROP_0,
  PROP_N_PAGES,
  PROP_POSITION,
  PROP_INTERACTIVE,
  PROP_SPACING,
  PROP_UNIFORM_SPACING,
  PROP_SCROLL_PARAMS,
  PROP_ALLOW_MOUSE_DRAG,
  PROP_ALLOW_SCROLL_WHEEL,
  PROP_ALLOW_LONG_SWIPES,
  PROP_REVEAL_DURATION,

  /* GtkOrientable */
  PROP_ORIENTATION,
  LAST_PROP = PROP_REVEAL_DURATION + 1,
};

static GParamSpec *props[LAST_PROP];

enum {
  SIGNAL_PAGE_CHANGED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];

static ChildInfo *
find_child_info (BzCarousel *self,
                 GtkWidget   *widget)
{
  GList *l;

  for (l = self->children; l; l = l->next) {
    ChildInfo *info = l->data;

    if (widget == info->widget)
      return info;
  }

  return NULL;
}

static int
find_child_index (BzCarousel *self,
                  GtkWidget   *widget,
                  gboolean     count_removing)
{
  GList *l;
  int i;

  i = 0;
  for (l = self->children; l; l = l->next) {
    ChildInfo *info = l->data;

    if (info->removing && !count_removing)
      continue;

    if (widget == info->widget)
      return i;

    i++;
  }

  return -1;
}

static GList *
get_nth_link (BzCarousel *self,
              int          n)
{

  GList *l;
  int i;

  i = n;
  for (l = self->children; l; l = l->next) {
    ChildInfo *info = l->data;

    if (info->removing)
      continue;

    if (i-- == 0)
      return l;
  }

  return NULL;
}

static ChildInfo *
get_closest_child_at (BzCarousel *self,
                      double       position,
                      gboolean     count_adding,
                      gboolean     count_removing)
{
  GList *l;
  ChildInfo *closest_child = NULL;

  for (l = self->children; l; l = l->next) {
    ChildInfo *child = l->data;

    if (child->adding && !count_adding)
      continue;

    if (child->removing && !count_removing)
      continue;

    if (!closest_child ||
        ABS (closest_child->snap_point - position) >
        ABS (child->snap_point - position))
      closest_child = child;
  }

  return closest_child;
}

static inline void
get_range (BzCarousel *self,
           double      *lower,
           double      *upper)
{
  GList *l = g_list_last (self->children);
  ChildInfo *child = l ? l->data : NULL;

  if (lower)
    *lower = 0;

  if (upper)
    *upper = MAX (0, self->position_shift + (child ? child->snap_point : 0));
}

static GtkWidget *
get_page_at_position (BzCarousel *self,
                      double       position)
{
  double lower = 0, upper = 0;
  ChildInfo *child;

  get_range (self, &lower, &upper);

  position = CLAMP (position, lower, upper);

  child = get_closest_child_at (self, position, TRUE, FALSE);

  if (!child)
    return NULL;

  return child->widget;
}

static void
update_shift_position_flag (BzCarousel *self,
                            ChildInfo   *child)
{
  ChildInfo *closest_child;
  int animating_index, closest_index;

  /* We want to still shift position when the active child is being removed */
  closest_child = get_closest_child_at (self, self->position, FALSE, TRUE);

  if (!closest_child)
    return;

  animating_index = g_list_index (self->children, child);
  closest_index = g_list_index (self->children, closest_child);

  child->shift_position = (closest_index >= animating_index);
}

static void
set_position (BzCarousel *self,
              double       position)
{
  GList *l;
  double lower = 0, upper = 0;

  get_range (self, &lower, &upper);

  position = CLAMP (position, lower, upper);

  self->position = position;
  gtk_widget_queue_allocate (GTK_WIDGET (self));

  for (l = self->children; l; l = l->next) {
    ChildInfo *child = l->data;

    if (child->adding || child->removing)
      update_shift_position_flag (self, child);
  }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_POSITION]);
}

static void
resize_animation_value_cb (double     value,
                           ChildInfo *child)
{
  BzCarousel *self = BZ_CAROUSEL (adw_animation_get_widget (child->resize_animation));
  double delta = value - child->size;

  child->size = value;

  if (child->shift_position)
    self->position_shift += delta;

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
resize_animation_done_cb (ChildInfo *child)
{
  BzCarousel *self = BZ_CAROUSEL (adw_animation_get_widget (child->resize_animation));

  g_clear_object (&child->resize_animation);

  if (child->adding)
    child->adding = FALSE;

  if (child->removing) {
    self->children = g_list_remove (self->children, child);

    g_free (child);
  }

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
animate_child_resize (BzCarousel *self,
                      ChildInfo   *child,
                      double       value,
                      guint        duration)
{
  AdwAnimationTarget *target;
  double old_size = child->size;

  update_shift_position_flag (self, child);

  if (child->resize_animation) {
    gboolean been_removing = child->removing;
    adw_animation_skip (child->resize_animation);
    /* It's because the skip finishes the animation, which triggers
       the 'done' signal, which calls resize_animation_done_cb(),
       which frees the 'child' immediately. */
    if (been_removing)
      return;
  }

  target = adw_callback_animation_target_new ((AdwAnimationTargetFunc)
                                              resize_animation_value_cb,
                                              child, NULL);
  child->resize_animation =
    adw_timed_animation_new (GTK_WIDGET (self), old_size,
                             value, duration, target);

  adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (child->resize_animation), ADW_EASE);

  g_signal_connect_swapped (child->resize_animation, "done",
                            G_CALLBACK (resize_animation_done_cb), child);

  adw_animation_play (child->resize_animation);
}

static void
scroll_animation_value_cb (double       value,
                           BzCarousel *self)
{
  set_position (self, value);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
scroll_animation_done_cb (BzCarousel *self)
{
  GtkWidget *child;
  int index;

  self->animation_source_position = 0;
  self->animation_target_child = NULL;

  child = get_page_at_position (self, self->position);
  index = find_child_index (self, child, FALSE);

  g_signal_emit (self, signals[SIGNAL_PAGE_CHANGED], 0, index);
}

static void
scroll_to (BzCarousel *self,
           GtkWidget   *widget,
           double       velocity)
{
  self->animation_target_child = find_child_info (self, widget);

  if (self->animation_target_child == NULL)
    return;

  self->animation_source_position = self->position;

  adw_spring_animation_set_value_from (ADW_SPRING_ANIMATION (self->animation),
                                       self->animation_source_position);
  adw_spring_animation_set_value_to (ADW_SPRING_ANIMATION (self->animation),
                                     self->animation_target_child->snap_point);
  adw_spring_animation_set_initial_velocity (ADW_SPRING_ANIMATION (self->animation),
                                             velocity);
  adw_animation_play (self->animation);
}

static inline double
get_closest_snap_point (BzCarousel *self)
{
  ChildInfo *closest_child =
    get_closest_child_at (self, self->position, TRUE, TRUE);

  if (!closest_child)
    return 0;

  return closest_child->snap_point;
}

static void
begin_swipe_cb (AdwSwipeTracker *tracker,
                BzCarousel     *self)
{
  adw_animation_pause (self->animation);
}

static void
update_swipe_cb (AdwSwipeTracker *tracker,
                 double           progress,
                 BzCarousel     *self)
{
  set_position (self, progress);
}

static void
end_swipe_cb (AdwSwipeTracker *tracker,
              double           velocity,
              double           to,
              BzCarousel     *self)
{
  GtkWidget *child = get_page_at_position (self, to);

  scroll_to (self, child, velocity);
}

/* Copied from GtkOrientable. Orientable widgets are supposed
 * to do this manually via a private GTK function. */
static void
set_orientable_style_classes (GtkOrientable *orientable)
{
  GtkOrientation orientation = gtk_orientable_get_orientation (orientable);
  GtkWidget *widget = GTK_WIDGET (orientable);

  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    gtk_widget_add_css_class (widget, "horizontal");
    gtk_widget_remove_css_class (widget, "vertical");
  } else {
    gtk_widget_add_css_class (widget, "vertical");
    gtk_widget_remove_css_class (widget, "horizontal");
  }
}

static void
update_orientation (BzCarousel *self)
{
  gboolean reversed =
    self->orientation == GTK_ORIENTATION_HORIZONTAL &&
    gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->tracker),
                                  self->orientation);
  adw_swipe_tracker_set_reversed (self->tracker,
                                  reversed);

  set_orientable_style_classes (GTK_ORIENTABLE (self));
}

static void
scroll_timeout_cb (BzCarousel *self)
{
  self->scroll_timeout_id = 0;
}

static gboolean
navigate_to_direction (BzCarousel            *self,
                       AdwNavigationDirection  direction)
{
  guint index;
  guint n_pages;

  n_pages = bz_carousel_get_n_pages (self);
  if (n_pages == 0)
    return FALSE;

  index = round (self->position);

  switch (direction) {
  case ADW_NAVIGATION_DIRECTION_BACK:
    if (index > 0)
      index--;
    else
      return FALSE;
    break;
  case ADW_NAVIGATION_DIRECTION_FORWARD:
    if (index < n_pages - 1)
      index++;
    else
      return FALSE;
    break;
  default:
    g_assert_not_reached();
  }

  scroll_to (self, bz_carousel_get_nth_page (self, index), 0);

  return TRUE;
}

static gboolean
scroll_cb (BzCarousel              *self,
           double                    dx,
           double                    dy,
           GtkEventControllerScroll *controller)
{
  GdkDevice *source_device;
  GdkInputSource input_source;
  int index;
  gboolean allow_vertical;
  GtkOrientation orientation;
  GtkWidget *child;

  if (!self->allow_scroll_wheel)
    return GDK_EVENT_PROPAGATE;

  if (self->scroll_timeout_id > 0)
    return GDK_EVENT_PROPAGATE;

  if (!bz_carousel_get_interactive (self))
    return GDK_EVENT_PROPAGATE;

  if (bz_carousel_get_n_pages (self) == 0)
    return GDK_EVENT_PROPAGATE;

  source_device = gtk_event_controller_get_current_event_device (GTK_EVENT_CONTROLLER (controller));
  input_source = gdk_device_get_source (source_device);
  if (input_source == GDK_SOURCE_TOUCHPAD)
    return GDK_EVENT_PROPAGATE;

  /* Mice often don't have easily accessible horizontal scrolling,
   * hence allow vertical mouse scrolling regardless of orientation */
  allow_vertical = (input_source == GDK_SOURCE_MOUSE);

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (self));
  index = 0;

  if (orientation == GTK_ORIENTATION_VERTICAL || allow_vertical) {
    if (dy > 0)
      index++;
    else if (dy < 0)
      index--;
  }

  if (orientation == GTK_ORIENTATION_HORIZONTAL && index == 0) {
    if (dx > 0)
      index++;
    else if (dx < 0)
      index--;
  }

  if (index == 0)
    return GDK_EVENT_PROPAGATE;

  child = get_page_at_position (self, self->position);

  index += find_child_index (self, child, FALSE);
  index = CLAMP (index, 0, (int) bz_carousel_get_n_pages (self) - 1);

  scroll_to (self, bz_carousel_get_nth_page (self, index), 0);

  self->scroll_timeout_id =
   g_timeout_add_once (SCROLL_TIMEOUT_DURATION,
                       (GSourceOnceFunc) scroll_timeout_cb,
                       self);

  return GDK_EVENT_STOP;
}

static gboolean
keynav_cb (BzCarousel *self,
           GVariant    *args)
{
  guint n_pages;
  gboolean is_rtl;
  AdwNavigationDirection direction;
  GtkDirectionType direction_type;

  if (!bz_carousel_get_interactive (self))
    return GDK_EVENT_PROPAGATE;

  n_pages = bz_carousel_get_n_pages (self);
  if (n_pages == 0)
    return GDK_EVENT_PROPAGATE;

  g_variant_get (args, "u", &direction_type);

  switch (direction_type) {
  case GTK_DIR_UP:
  case GTK_DIR_DOWN:
    if (self->orientation != GTK_ORIENTATION_VERTICAL)
      return GDK_EVENT_PROPAGATE;
    break;
  case GTK_DIR_LEFT:
  case GTK_DIR_RIGHT:
    if (self->orientation != GTK_ORIENTATION_HORIZONTAL)
      return GDK_EVENT_PROPAGATE;
    break;
  case GTK_DIR_TAB_BACKWARD:
  case GTK_DIR_TAB_FORWARD:
    break;
  default:
    g_assert_not_reached();
  }

  is_rtl = (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL);

  switch (direction_type) {
  case GTK_DIR_LEFT:
    direction = is_rtl ? ADW_NAVIGATION_DIRECTION_FORWARD : ADW_NAVIGATION_DIRECTION_BACK;
    break;
  case GTK_DIR_RIGHT:
    direction = is_rtl ? ADW_NAVIGATION_DIRECTION_BACK : ADW_NAVIGATION_DIRECTION_FORWARD;
    break;
  case GTK_DIR_UP:
  case GTK_DIR_TAB_BACKWARD:
    direction = ADW_NAVIGATION_DIRECTION_BACK;
    break;
  case GTK_DIR_DOWN:
  case GTK_DIR_TAB_FORWARD:
    direction = ADW_NAVIGATION_DIRECTION_FORWARD;
    break;
  default:
    g_assert_not_reached();
  }

  navigate_to_direction (self, direction);

  return GDK_EVENT_STOP;
}

static gboolean
keynav_bounds_cb (BzCarousel *self,
                  GVariant    *args)
{
  guint n_pages;
  GtkDirectionType direction;

  if (!bz_carousel_get_interactive (self))
    return GDK_EVENT_PROPAGATE;

  n_pages = bz_carousel_get_n_pages (self);
  if (n_pages == 0)
    return GDK_EVENT_PROPAGATE;

  g_variant_get (args, "u", &direction);

  switch (direction) {
  case GTK_DIR_TAB_BACKWARD:
    scroll_to (self, bz_carousel_get_nth_page (self, 0), 0);
    break;
  case GTK_DIR_TAB_FORWARD:
    scroll_to (self, bz_carousel_get_nth_page (self, n_pages - 1), 0);
    break;
  case GTK_DIR_DOWN:
  case GTK_DIR_LEFT:
  case GTK_DIR_RIGHT:
  case GTK_DIR_UP:
    return GDK_EVENT_PROPAGATE;
  default:
    g_assert_not_reached();
  }

  return GDK_EVENT_STOP;
}

static void
bz_carousel_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  BzCarousel *self = BZ_CAROUSEL (widget);
  GList *children;

  if (minimum)
    *minimum = 0;
  if (natural)
    *natural = 0;

  if (minimum_baseline)
    *minimum_baseline = -1;
  if (natural_baseline)
    *natural_baseline = -1;

  for (children = self->children; children; children = children->next) {
    ChildInfo *child_info = children->data;
    GtkWidget *child = child_info->widget;
    int child_min, child_nat;

    if (child_info->removing)
      continue;

    if (!gtk_widget_get_visible (child))
      continue;

    gtk_widget_measure (child, orientation, for_size,
                        &child_min, &child_nat, NULL, NULL);

    if (minimum)
      *minimum = MAX (*minimum, child_min);
    if (natural)
      *natural = MAX (*natural, child_nat);
  }
}

static void
bz_carousel_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  BzCarousel *self = BZ_CAROUSEL (widget);
  GList *children;
  double x, y, offset;
  gboolean is_rtl;
  double snap_point;

  if (!G_APPROX_VALUE (self->position_shift, 0, DBL_EPSILON)) {
    set_position (self, self->position + self->position_shift);
    adw_swipe_tracker_shift_position (self->tracker, self->position_shift);
    self->position_shift = 0;
  }

  int max_size = 0;
  for (children = self->children; children; children = children->next) {
    ChildInfo *child_info = children->data;
    GtkWidget *child = child_info->widget;
    int min, nat;
    int child_size;

    if (child_info->removing)
      continue;

    if (self->orientation == GTK_ORIENTATION_HORIZONTAL) {
      gtk_widget_measure (child, self->orientation,
                          height, &min, &nat, NULL, NULL);
      if (gtk_widget_get_hexpand (child))
        child_size = width;
      else
        child_size = CLAMP (nat, min, width);
    } else {
      gtk_widget_measure (child, self->orientation,
                          width, &min, &nat, NULL, NULL);
      if (gtk_widget_get_vexpand (child))
        child_size = height;
      else
        child_size = CLAMP (nat, min, height);
    }

    child_info->size = self->uniform_spacing ? 1.0 : ((double)child_size / width);

    max_size = MAX (max_size, child_size);
  }

  if (self->uniform_spacing) {
    self->distance = max_size + self->spacing;
  } else {
    self->distance = max_size;
  }

  snap_point = 0;
  for (children = self->children; children; children = children->next) {
    ChildInfo *child_info = children->data;

    if (child_info->removing)
      continue;

    child_info->snap_point = snap_point + child_info->size - 1;

    if (self->uniform_spacing) {
      snap_point += child_info->size;
    } else {
      snap_point += child_info->size;
    }

    if (child_info == self->animation_target_child)
      adw_spring_animation_set_value_to (ADW_SPRING_ANIMATION (self->animation),
                                         child_info->snap_point);
  }

  x = 0;
  y = 0;

  is_rtl = (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL);

  if (self->uniform_spacing) {
    if (self->orientation == GTK_ORIENTATION_VERTICAL)
      offset = (self->distance * self->position) - (height - max_size) / 2.0;
    else if (is_rtl)
      offset = -(self->distance * self->position) - (width - max_size) / 2.0;
    else
      offset = (self->distance * self->position) - (width - max_size) / 2.0;
  } else {
    double accumulated_distance = 0;
    int target_index = (int)round(self->position);
    int current_index = 0;

    for (children = self->children; children; children = children->next) {
      ChildInfo *child_info = children->data;

      if (child_info->removing)
        continue;

      if (current_index < target_index) {
        accumulated_distance += self->distance * child_info->size + self->spacing;
      } else if (current_index == target_index) {
        double frac = self->position - target_index;
        accumulated_distance += (self->distance * child_info->size + self->spacing) * frac;
        break;
      }

      current_index++;
    }

    if (self->orientation == GTK_ORIENTATION_VERTICAL)
      offset = accumulated_distance - (height - max_size) / 2.0;
    else if (is_rtl)
      offset = -accumulated_distance - (width - max_size) / 2.0;
    else
      offset = accumulated_distance - (width - max_size) / 2.0;
  }

  if (self->orientation == GTK_ORIENTATION_VERTICAL)
    y -= offset;
  else
    x -= offset;

  for (children = self->children; children; children = children->next) {
    ChildInfo *child_info = children->data;
    GskTransform *transform = gsk_transform_new ();
    int child_width, child_height;

    if (child_info->removing)
      continue;

    if (!gtk_widget_get_visible (child_info->widget))
      continue;

    int min, nat;
    int this_child_size;

    if (self->orientation == GTK_ORIENTATION_HORIZONTAL) {
      gtk_widget_measure (child_info->widget, self->orientation,
                          height, &min, &nat, NULL, NULL);
      if (gtk_widget_get_hexpand (child_info->widget))
        this_child_size = width;
      else
        this_child_size = CLAMP (nat, min, width);
    } else {
      gtk_widget_measure (child_info->widget, self->orientation,
                          width, &min, &nat, NULL, NULL);
      if (gtk_widget_get_vexpand (child_info->widget))
        this_child_size = height;
      else
        this_child_size = CLAMP (nat, min, height);
    }

    if (self->uniform_spacing) {
      if (self->orientation == GTK_ORIENTATION_HORIZONTAL) {
        child_width = max_size;
        child_height = height;
      } else {
        child_width = width;
        child_height = max_size;
      }
    } else {
      if (self->orientation == GTK_ORIENTATION_HORIZONTAL) {
        child_width = this_child_size;
        child_height = height;
      } else {
        child_width = width;
        child_height = this_child_size;
      }
    }

    if (self->orientation == GTK_ORIENTATION_VERTICAL) {
      child_info->position = y;
      child_info->visible = child_info->position < height &&
                            child_info->position + child_height > 0;

      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0, child_info->position));
    } else {
      child_info->position = x;
      child_info->visible = child_info->position < width &&
                            child_info->position + child_width > 0;

      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (child_info->position, 0));
    }

    gtk_widget_allocate (child_info->widget, child_width, child_height, baseline, transform);

    if (self->uniform_spacing) {
      if (self->orientation == GTK_ORIENTATION_VERTICAL) {
        y += self->distance * child_info->size;
      } else if (is_rtl) {
        x -= self->distance * child_info->size;
      } else {
        x += self->distance * child_info->size;
      }
    } else {
      if (self->orientation == GTK_ORIENTATION_VERTICAL) {
        y += this_child_size + self->spacing;
      } else if (is_rtl) {
        x -= this_child_size + self->spacing;
      } else {
        x += this_child_size + self->spacing;
      }
    }
  }

  self->is_being_allocated = FALSE;
}

static void
bz_carousel_direction_changed (GtkWidget        *widget,
                                GtkTextDirection  previous_direction)
{
  BzCarousel *self = BZ_CAROUSEL (widget);

  update_orientation (self);
}

static void
bz_carousel_constructed (GObject *object)
{
  BzCarousel *self = (BzCarousel *)object;

  update_orientation (self);

  G_OBJECT_CLASS (bz_carousel_parent_class)->constructed (object);
}

static void
bz_carousel_dispose (GObject *object)
{
  BzCarousel *self = BZ_CAROUSEL (object);

  while (self->children) {
    ChildInfo *info = self->children->data;

    bz_carousel_remove (self, info->widget);
  }

  g_clear_object (&self->tracker);
  g_clear_object (&self->animation);
  g_clear_handle_id (&self->scroll_timeout_id, g_source_remove);

  G_OBJECT_CLASS (bz_carousel_parent_class)->dispose (object);
}

static void
bz_carousel_finalize (GObject *object)
{
  BzCarousel *self = BZ_CAROUSEL (object);

  g_list_free_full (self->children, (GDestroyNotify) g_free);

  G_OBJECT_CLASS (bz_carousel_parent_class)->finalize (object);
}

static void
bz_carousel_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  BzCarousel *self = BZ_CAROUSEL (object);

  switch (prop_id) {
  case PROP_N_PAGES:
    g_value_set_uint (value, bz_carousel_get_n_pages (self));
    break;

  case PROP_POSITION:
    g_value_set_double (value, bz_carousel_get_position (self));
    break;

  case PROP_INTERACTIVE:
    g_value_set_boolean (value, bz_carousel_get_interactive (self));
    break;

  case PROP_SPACING:
    g_value_set_uint (value, bz_carousel_get_spacing (self));
    break;

  case PROP_UNIFORM_SPACING:
    g_value_set_boolean (value, bz_carousel_get_uniform_spacing (self));
    break;

  case PROP_ALLOW_MOUSE_DRAG:
    g_value_set_boolean (value, bz_carousel_get_allow_mouse_drag (self));
    break;

  case PROP_ALLOW_SCROLL_WHEEL:
    g_value_set_boolean (value, bz_carousel_get_allow_scroll_wheel (self));
    break;

  case PROP_ALLOW_LONG_SWIPES:
    g_value_set_boolean (value, bz_carousel_get_allow_long_swipes (self));
    break;

  case PROP_REVEAL_DURATION:
    g_value_set_uint (value, bz_carousel_get_reveal_duration (self));
    break;

  case PROP_ORIENTATION:
    g_value_set_enum (value, self->orientation);
    break;

  case PROP_SCROLL_PARAMS:
    g_value_set_boxed (value, bz_carousel_get_scroll_params (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
bz_carousel_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  BzCarousel *self = BZ_CAROUSEL (object);

  switch (prop_id) {
  case PROP_INTERACTIVE:
    bz_carousel_set_interactive (self, g_value_get_boolean (value));
    break;

  case PROP_SPACING:
    bz_carousel_set_spacing (self, g_value_get_uint (value));
    break;

  case PROP_UNIFORM_SPACING:
    bz_carousel_set_uniform_spacing (self, g_value_get_boolean (value));
    break;

  case PROP_SCROLL_PARAMS:
    bz_carousel_set_scroll_params (self, g_value_get_boxed (value));
    break;

  case PROP_REVEAL_DURATION:
    bz_carousel_set_reveal_duration (self, g_value_get_uint (value));
    break;

  case PROP_ALLOW_MOUSE_DRAG:
    bz_carousel_set_allow_mouse_drag (self, g_value_get_boolean (value));
    break;

  case PROP_ALLOW_SCROLL_WHEEL:
    bz_carousel_set_allow_scroll_wheel (self, g_value_get_boolean (value));
    break;

  case PROP_ALLOW_LONG_SWIPES:
    bz_carousel_set_allow_long_swipes (self, g_value_get_boolean (value));
    break;

  case PROP_ORIENTATION:
    {
      GtkOrientation orientation = g_value_get_enum (value);
      if (orientation != self->orientation) {
        self->orientation = orientation;
        update_orientation (self);
        gtk_widget_queue_resize (GTK_WIDGET (self));
        g_object_notify (G_OBJECT (self), "orientation");
      }
    }
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
bz_carousel_class_init (BzCarouselClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = bz_carousel_constructed;
  object_class->dispose = bz_carousel_dispose;
  object_class->finalize = bz_carousel_finalize;
  object_class->get_property = bz_carousel_get_property;
  object_class->set_property = bz_carousel_set_property;

  widget_class->measure = bz_carousel_measure;
  widget_class->size_allocate = bz_carousel_size_allocate;
  widget_class->direction_changed = bz_carousel_direction_changed;

  props[PROP_N_PAGES] =
    g_param_spec_uint ("n-pages", NULL, NULL,
                       0,
                       G_MAXUINT,
                       0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_POSITION] =
    g_param_spec_double ("position", NULL, NULL,
                         0,
                         G_MAXDOUBLE,
                         0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_INTERACTIVE] =
    g_param_spec_boolean ("interactive", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SPACING] =
    g_param_spec_uint ("spacing", NULL, NULL,
                       0,
                       G_MAXUINT,
                       0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_UNIFORM_SPACING] =
  g_param_spec_boolean ("uniform-spacing", NULL, NULL,
                        TRUE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SCROLL_PARAMS] =
    g_param_spec_boxed ("scroll-params", NULL, NULL,
                        ADW_TYPE_SPRING_PARAMS,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ALLOW_MOUSE_DRAG] =
    g_param_spec_boolean ("allow-mouse-drag", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ALLOW_SCROLL_WHEEL] =
    g_param_spec_boolean ("allow-scroll-wheel", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ALLOW_LONG_SWIPES] =
    g_param_spec_boolean ("allow-long-swipes", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_REVEAL_DURATION] =
    g_param_spec_uint ("reveal-duration", NULL, NULL,
                       0,
                       G_MAXUINT,
                       0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_PAGE_CHANGED] =
    g_signal_new ("page-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_UINT);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Up, GDK_NO_MODIFIER_MASK,
                                (GtkShortcutFunc) keynav_cb,
                                "u", GTK_DIR_UP);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Down, GDK_NO_MODIFIER_MASK,
                                (GtkShortcutFunc) keynav_cb,
                                "u", GTK_DIR_DOWN);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Left, GDK_NO_MODIFIER_MASK,
                                (GtkShortcutFunc) keynav_cb,
                                "u", GTK_DIR_LEFT);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Right, GDK_NO_MODIFIER_MASK,
                                (GtkShortcutFunc) keynav_cb,
                                "u", GTK_DIR_RIGHT);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Page_Up, GDK_NO_MODIFIER_MASK,
                                (GtkShortcutFunc) keynav_cb,
                                "u", GTK_DIR_TAB_BACKWARD);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Page_Down, GDK_NO_MODIFIER_MASK,
                                (GtkShortcutFunc) keynav_cb,
                                "u", GTK_DIR_TAB_FORWARD);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Home, GDK_NO_MODIFIER_MASK,
                                (GtkShortcutFunc) keynav_bounds_cb,
                                "u", GTK_DIR_TAB_BACKWARD);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_End, GDK_NO_MODIFIER_MASK,
                                (GtkShortcutFunc) keynav_bounds_cb,
                                "u", GTK_DIR_TAB_FORWARD);

  gtk_widget_class_set_css_name (widget_class, "carousel");
}

static void
bz_carousel_init (BzCarousel *self)
{
  GtkEventController *controller;
  AdwAnimationTarget *target;

  self->allow_scroll_wheel = TRUE;
  self->uniform_spacing    = TRUE;

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);

  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  self->reveal_duration = 0;

  self->tracker = adw_swipe_tracker_new (ADW_SWIPEABLE (self));
  adw_swipe_tracker_set_allow_mouse_drag (self->tracker, TRUE);

  g_signal_connect_object (self->tracker, "begin-swipe", G_CALLBACK (begin_swipe_cb), self, 0);
  g_signal_connect_object (self->tracker, "update-swipe", G_CALLBACK (update_swipe_cb), self, 0);
  g_signal_connect_object (self->tracker, "end-swipe", G_CALLBACK (end_swipe_cb), self, 0);

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
  g_signal_connect_swapped (controller, "scroll", G_CALLBACK (scroll_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  target = adw_callback_animation_target_new ((AdwAnimationTargetFunc)
                                              scroll_animation_value_cb,
                                              self, NULL);
  self->animation =
    adw_spring_animation_new (GTK_WIDGET (self), 0, 0,
                              adw_spring_params_new (1, 0.5, 500),
                              target);
  adw_spring_animation_set_clamp (ADW_SPRING_ANIMATION (self->animation), TRUE);

  g_signal_connect_swapped (self->animation, "done",
                            G_CALLBACK (scroll_animation_done_cb), self);
}

static void
bz_carousel_buildable_add_child (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  GObject      *child,
                                  const char   *type)
{
  if (GTK_IS_WIDGET (child))
    bz_carousel_append (BZ_CAROUSEL (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
bz_carousel_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = bz_carousel_buildable_add_child;
}

static double
bz_carousel_get_distance (AdwSwipeable *swipeable)
{
  BzCarousel *self = BZ_CAROUSEL (swipeable);

  return self->distance;
}

static double *
bz_carousel_get_snap_points (AdwSwipeable *swipeable,
                              int          *n_snap_points)
{
  BzCarousel *self = BZ_CAROUSEL (swipeable);
  guint i, n_pages;
  double *points;
  GList *l;

  n_pages = MAX (g_list_length (self->children), 1);
  points = g_new0 (double, n_pages);

  i = 0;
  for (l = self->children; l; l = l->next) {
    ChildInfo *info = l->data;

    points[i++] = info->snap_point;
  }

  if (n_snap_points)
    *n_snap_points = n_pages;

  return points;
}

static double
bz_carousel_get_progress (AdwSwipeable *swipeable)
{
  BzCarousel *self = BZ_CAROUSEL (swipeable);

  return bz_carousel_get_position (self);
}

static double
bz_carousel_get_cancel_progress (AdwSwipeable *swipeable)
{
  BzCarousel *self = BZ_CAROUSEL (swipeable);

  return get_closest_snap_point (self);
}

static void
bz_carousel_swipeable_init (AdwSwipeableInterface *iface)
{
  iface->get_distance = bz_carousel_get_distance;
  iface->get_snap_points = bz_carousel_get_snap_points;
  iface->get_progress = bz_carousel_get_progress;
  iface->get_cancel_progress = bz_carousel_get_cancel_progress;
}

GtkWidget *
bz_carousel_new (void)
{
  return g_object_new (BZ_TYPE_CAROUSEL, NULL);
}

void
bz_carousel_prepend (BzCarousel *self,
                      GtkWidget   *widget)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  bz_carousel_insert (self, widget, 0);
}

void
bz_carousel_append (BzCarousel *self,
                     GtkWidget   *widget)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  bz_carousel_insert (self, widget, -1);
}

void
bz_carousel_insert (BzCarousel *self,
                     GtkWidget   *widget,
                     int          position)
{
  ChildInfo *info;
  GList *next_link = NULL;

  g_return_if_fail (BZ_IS_CAROUSEL (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);
  g_return_if_fail (position >= -1);

  info = g_new0 (ChildInfo, 1);
  info->widget = widget;
  info->size = 0;
  info->adding = TRUE;

  if (position >= 0)
    next_link = get_nth_link (self, position);

  self->children = g_list_insert_before (self->children, next_link, info);

  if (next_link) {
    ChildInfo *next_sibling = next_link->data;

    gtk_widget_insert_before (widget, GTK_WIDGET (self), next_sibling->widget);
  } else {
    gtk_widget_set_parent (widget, GTK_WIDGET (self));
  }

  self->is_being_allocated = TRUE;
  gtk_widget_queue_allocate (GTK_WIDGET (self));

  animate_child_resize (self, info, 1, self->reveal_duration);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_N_PAGES]);
}

void
bz_carousel_reorder (BzCarousel *self,
                      GtkWidget   *child,
                      int          position)
{
  ChildInfo *info, *next_info = NULL;
  GList *link, *next_link;
  int old_position, n_pages;
  double closest_point, old_point, new_point;

  g_return_if_fail (BZ_IS_CAROUSEL (self));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (position >= -1);

  closest_point = get_closest_snap_point (self);

  info = find_child_info (self, child);
  link = g_list_find (self->children, info);
  old_position = g_list_position (self->children, link);

  if (position == old_position)
    return;

  old_point = info->snap_point;
  n_pages = bz_carousel_get_n_pages (self);

  if (position < 0 || position > n_pages)
    position = n_pages;

  if (old_position == n_pages - 1 && position == n_pages)
    return;

  if (position == n_pages)
    next_link = NULL;
  else if (position > old_position)
    next_link = get_nth_link (self, position + 1);
  else
    next_link = get_nth_link (self, position);

  if (next_link) {
    next_info = next_link->data;
    new_point = next_info->snap_point;

    /* Since we know position > old_position, it's not 0 so prev_info exists */
    if (position > old_position) {
      ChildInfo *prev_info = next_link->prev->data;

      new_point = prev_info->snap_point;
    }
  } else {
    GList *last_link = g_list_last (self->children);
    ChildInfo *last_info = last_link->data;

    new_point = last_info->snap_point;
  }

  self->children = g_list_remove_link (self->children, link);

  if (next_link) {
    self->children = g_list_insert_before_link (self->children, next_link, link);

    gtk_widget_insert_before (child, GTK_WIDGET (self), next_info->widget);
  } else {
    self->children = g_list_append (self->children, info);
    g_list_free (link);

    gtk_widget_insert_before (child, GTK_WIDGET (self), NULL);
  }

  if (G_APPROX_VALUE (closest_point, old_point, DBL_EPSILON))
    self->position_shift += new_point - old_point;
  else if ((G_APPROX_VALUE (old_point, closest_point, DBL_EPSILON) || old_point > closest_point) &&
           (G_APPROX_VALUE (closest_point, new_point, DBL_EPSILON) || closest_point > new_point))
    self->position_shift += info->size;
  else if ((G_APPROX_VALUE (new_point, closest_point, DBL_EPSILON) || new_point > closest_point) &&
           (G_APPROX_VALUE (closest_point, old_point, DBL_EPSILON) || closest_point > old_point))
    self->position_shift -= info->size;

  self->is_being_allocated = TRUE;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

void
bz_carousel_remove (BzCarousel *self,
                     GtkWidget   *child)
{
  ChildInfo *info;

  g_return_if_fail (BZ_IS_CAROUSEL (self));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == GTK_WIDGET (self));

  info = find_child_info (self, child);

  g_assert_nonnull (info);

  info->removing = TRUE;

  gtk_widget_unparent (child);

  info->widget = NULL;

  if (!gtk_widget_in_destruction (GTK_WIDGET (self)))
    animate_child_resize (self, info, 0, self->reveal_duration);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_N_PAGES]);
}

static void
do_scroll_to (BzCarousel *self,
              GtkWidget   *widget,
              gboolean     animate)
{
  scroll_to (self, widget, 0);

  if (!animate)
    adw_animation_skip (self->animation);
}

typedef struct {
  BzCarousel *carousel;
  GtkWidget *widget;
  gboolean animate;
} ScrollData;

static void
scroll_to_idle_cb (ScrollData *data)
{
  do_scroll_to (data->carousel, data->widget, data->animate);

  g_object_unref (data->carousel);
  g_object_unref (data->widget);
  g_free (data);
}

void
bz_carousel_scroll_to (BzCarousel *self,
                        GtkWidget   *widget,
                        gboolean     animate)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (self));

  if (self->is_being_allocated) {
    ScrollData *data;

    data = g_new (ScrollData, 1);
    data->carousel = g_object_ref (self);
    data->widget = g_object_ref (widget);
    data->animate = animate;

    g_idle_add_once ((GSourceOnceFunc) scroll_to_idle_cb, data);
    return;
  }

  do_scroll_to (self, widget, animate);
}

GtkWidget *
bz_carousel_get_nth_page (BzCarousel *self,
                           guint        n)
{
  ChildInfo *info;

  g_return_val_if_fail (BZ_IS_CAROUSEL (self), NULL);
  g_return_val_if_fail (n < bz_carousel_get_n_pages (self), NULL);

  info = get_nth_link (self, n)->data;

  return info->widget;
}

guint
bz_carousel_get_n_pages (BzCarousel *self)
{
  GList *l;
  guint n_pages;

  g_return_val_if_fail (BZ_IS_CAROUSEL (self), 0);

  n_pages = 0;
  for (l = self->children; l; l = l->next) {
    ChildInfo *child = l->data;

    if (!child->removing)
      n_pages++;
  }

  return n_pages;
}

double
bz_carousel_get_position (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), 0.0);

  return self->position;
}

gboolean
bz_carousel_get_interactive (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);

  return adw_swipe_tracker_get_enabled (self->tracker);
}

void
bz_carousel_set_interactive (BzCarousel *self,
                              gboolean     interactive)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  interactive = !!interactive;

  if (adw_swipe_tracker_get_enabled (self->tracker) == interactive)
    return;

  adw_swipe_tracker_set_enabled (self->tracker, interactive);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INTERACTIVE]);
}

guint
bz_carousel_get_spacing (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), 0);

  return self->spacing;
}

void
bz_carousel_set_spacing (BzCarousel *self,
                          guint        spacing)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  if (self->spacing == spacing)
    return;

  self->spacing = spacing;
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SPACING]);
}

gboolean
bz_carousel_get_uniform_spacing (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);

  return self->uniform_spacing;
}

void
bz_carousel_set_uniform_spacing (BzCarousel *self,
                                  gboolean    uniform_spacing)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  uniform_spacing = !!uniform_spacing;

  if (self->uniform_spacing == uniform_spacing)
    return;

  self->uniform_spacing = uniform_spacing;
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_UNIFORM_SPACING]);
}

AdwSpringParams *
bz_carousel_get_scroll_params (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), NULL);

  return adw_spring_animation_get_spring_params (ADW_SPRING_ANIMATION (self->animation));
}

void
bz_carousel_set_scroll_params (BzCarousel     *self,
                                AdwSpringParams *params)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));
  g_return_if_fail (params != NULL);

  if (bz_carousel_get_scroll_params (self) == params)
    return;

  adw_spring_animation_set_spring_params (ADW_SPRING_ANIMATION (self->animation), params);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SCROLL_PARAMS]);
}

gboolean
bz_carousel_get_allow_mouse_drag (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);

  return adw_swipe_tracker_get_allow_mouse_drag (self->tracker);
}

void
bz_carousel_set_allow_mouse_drag (BzCarousel *self,
                                   gboolean     allow_mouse_drag)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  allow_mouse_drag = !!allow_mouse_drag;

  if (bz_carousel_get_allow_mouse_drag (self) == allow_mouse_drag)
    return;

  adw_swipe_tracker_set_allow_mouse_drag (self->tracker, allow_mouse_drag);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALLOW_MOUSE_DRAG]);
}

gboolean
bz_carousel_get_allow_scroll_wheel (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);

  return self->allow_scroll_wheel;
}

void
bz_carousel_set_allow_scroll_wheel (BzCarousel *self,
                                     gboolean     allow_scroll_wheel)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  allow_scroll_wheel = !!allow_scroll_wheel;

  if (self->allow_scroll_wheel == allow_scroll_wheel)
    return;

  self->allow_scroll_wheel = allow_scroll_wheel;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALLOW_SCROLL_WHEEL]);
}

gboolean
bz_carousel_get_allow_long_swipes (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);

  return adw_swipe_tracker_get_allow_long_swipes (self->tracker);
}

void
bz_carousel_set_allow_long_swipes (BzCarousel *self,
                                    gboolean     allow_long_swipes)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  allow_long_swipes = !!allow_long_swipes;

  if (adw_swipe_tracker_get_allow_long_swipes (self->tracker) == allow_long_swipes)
    return;

  adw_swipe_tracker_set_allow_long_swipes (self->tracker, allow_long_swipes);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALLOW_LONG_SWIPES]);
}

guint
bz_carousel_get_reveal_duration (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), 0);

  return self->reveal_duration;
}

void
bz_carousel_set_reveal_duration (BzCarousel *self,
                                  guint        reveal_duration)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  if (self->reveal_duration == reveal_duration)
    return;

  self->reveal_duration = reveal_duration;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REVEAL_DURATION]);
}

