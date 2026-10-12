/* dex-gtk-utils.c
 *
 * Copyright 2026 Evelyn M
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

#include "dex-gtk-utils.h"

static void
bz_file_dialog_save_cb (GtkFileDialog *dialog,
                        GAsyncResult  *result,
                        DexPromise    *promise)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GFile) file         = NULL;

  file = gtk_file_dialog_save_finish (dialog, result, &local_error);

  if (local_error == NULL)
    dex_promise_resolve_object (promise, g_steal_pointer (&file));
  else
    dex_promise_reject (promise, g_steal_pointer (&local_error));

  dex_unref (promise);
}

DexFuture *
bz_file_dialog_save (GtkFileDialog *self,
                     GtkWindow     *parent)
{
  g_autoptr (DexPromise) promise = NULL;

  dex_return_error_if_fail (GTK_IS_FILE_DIALOG (self));
  dex_return_error_if_fail (GTK_IS_WINDOW (parent));

  promise = dex_promise_new_cancellable ();

  gtk_file_dialog_save (self,
                        parent,
                        dex_promise_get_cancellable (promise),
                        (GAsyncReadyCallback) bz_file_dialog_save_cb,
                        dex_ref (promise));

  return DEX_FUTURE (g_steal_pointer (&promise));
}
