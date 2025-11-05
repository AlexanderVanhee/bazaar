/* bz-carousel.h
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

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_CAROUSEL (bz_carousel_get_type())

G_DECLARE_FINAL_TYPE (BzCarousel, bz_carousel, BZ, CAROUSEL, GtkWidget)

GtkWidget *bz_carousel_new (void) G_GNUC_WARN_UNUSED_RESULT;

void bz_carousel_prepend (BzCarousel *self,
                          GtkWidget  *child);

void bz_carousel_append  (BzCarousel *self,
                          GtkWidget  *child);

void bz_carousel_insert  (BzCarousel *self,
                          GtkWidget  *child,
                          int         position);

void bz_carousel_reorder (BzCarousel *self,
                          GtkWidget  *child,
                          int         position);

void bz_carousel_remove (BzCarousel *self,
                         GtkWidget  *child);

void bz_carousel_scroll_to (BzCarousel *self,
                            GtkWidget  *widget,
                            gboolean    animate);

GtkWidget *bz_carousel_get_nth_page (BzCarousel *self,
                                     guint       n);

guint bz_carousel_get_n_pages (BzCarousel *self);

double bz_carousel_get_position (BzCarousel *self);

gboolean bz_carousel_get_interactive (BzCarousel *self);

void bz_carousel_set_interactive (BzCarousel *self,
                                  gboolean    interactive);

guint bz_carousel_get_spacing (BzCarousel *self);

void bz_carousel_set_spacing (BzCarousel *self,
                               guint       spacing);

gboolean bz_carousel_get_uniform_spacing (BzCarousel *self);

void bz_carousel_set_uniform_spacing (BzCarousel *self,
                                      gboolean    uniform_spacing);

AdwSpringParams *bz_carousel_get_scroll_params (BzCarousel *self);

void bz_carousel_set_scroll_params (BzCarousel     *self,
                                    AdwSpringParams *params);

gboolean bz_carousel_get_allow_mouse_drag (BzCarousel *self);

void bz_carousel_set_allow_mouse_drag (BzCarousel *self,
                                       gboolean    allow_mouse_drag);

gboolean bz_carousel_get_allow_scroll_wheel (BzCarousel *self);

void bz_carousel_set_allow_scroll_wheel (BzCarousel *self,
                                         gboolean    allow_scroll_wheel);

gboolean bz_carousel_get_allow_long_swipes (BzCarousel *self);

void bz_carousel_set_allow_long_swipes (BzCarousel *self,
                                        gboolean    allow_long_swipes);

guint bz_carousel_get_reveal_duration (BzCarousel *self);

void bz_carousel_set_reveal_duration (BzCarousel *self,
                                      guint       reveal_duration);

G_END_DECLS
