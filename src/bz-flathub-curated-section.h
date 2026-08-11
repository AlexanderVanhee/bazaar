/* bz-flathub-curated-section.h
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

#pragma once

#include <adwaita.h>

#include "bz-flathub-curated-selection.h"

G_BEGIN_DECLS

#define BZ_TYPE_FLATHUB_CURATED_SECTION (bz_flathub_curated_section_get_type ())
G_DECLARE_FINAL_TYPE (BzFlathubCuratedSection, bz_flathub_curated_section, BZ, FLATHUB_CURATED_SECTION, AdwBin)

GtkWidget *
bz_flathub_curated_section_new (void);

void
bz_flathub_curated_section_set_selection (BzFlathubCuratedSection   *self,
                                          BzFlathubCuratedSelection *selection);

BzFlathubCuratedSelection *
bz_flathub_curated_section_get_selection (BzFlathubCuratedSection *self);

void
bz_flathub_curated_section_set_max_length (BzFlathubCuratedSection *self,
                                           guint                    max_length);

G_END_DECLS
