/* util.c
 *
 * Copyright 2026 Eva M
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

#include "util.h"

BzWeakRef *
bz_weak_ref_ref (BzWeakRef *wr)
{
  g_atomic_ref_count_inc (&wr->rc);
  return wr;
}

void
bz_weak_ref_unref (BzWeakRef *wr)
{
  if (!g_atomic_ref_count_dec (&wr->rc))
    return;
  g_weak_ref_clear (&wr->ref);
  g_free (wr);
}

G_DEFINE_BOXED_TYPE (
    BzWeakRef,
    bz_weak_ref,
    bz_weak_ref_ref,
    bz_weak_ref_unref);

BzWeakRef *
bz_weak_ref_new (gpointer object)
{
  BzWeakRef *wr = NULL;

  g_return_val_if_fail (object != NULL, NULL);

  wr = g_new0 (BzWeakRef, 1);
  g_atomic_ref_count_init (&wr->rc);
  g_weak_ref_init (&wr->ref, object);

  return wr;
}
