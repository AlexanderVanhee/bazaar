/* bz-flatpak-mask.c
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

#define G_LOG_DOMAIN  "BAZAAR::FLATPAK"
#define BAZAAR_MODULE "flatpak"

#include "config.h"

#include "bz-env.h"
#include "bz-flatpak-mask.h"
#include "bz-flatpak-private.h"
#include "bz-util.h"

BZ_DEFINE_DATA (
    get_masked_ids,
    GetMaskedIds,
    {
      GWeakRef     *self;
      GCancellable *cancellable;
    },
    BZ_RELEASE_DATA (self, bz_weak_release);
    BZ_RELEASE_DATA (cancellable, g_object_unref))

BZ_DEFINE_DATA (
    set_mask,
    SetMask,
    {
      GWeakRef     *self;
      GCancellable *cancellable;
      char         *app_id;
      gboolean      masked;
      gboolean      user;
    },
    BZ_RELEASE_DATA (self, bz_weak_release);
    BZ_RELEASE_DATA (cancellable, g_object_unref);
    BZ_RELEASE_DATA (app_id, g_free))

static GHashTable *
read_masks_for_installation (FlatpakInstallation *installation,
                             GError             **error)
{
  g_autoptr (GFile) path         = NULL;
  g_autofree char *path_str      = NULL;
  g_autofree char *config_path   = NULL;
  g_autoptr (GKeyFile) kf        = NULL;
  g_autofree char *masked_str    = NULL;
  g_auto (GStrv) entries         = NULL;
  g_autoptr (GHashTable) set     = NULL;
  g_autoptr (GError) local_error = NULL;

  path        = flatpak_installation_get_path (installation);
  path_str    = g_file_get_path (path);
  config_path = g_build_filename (path_str, "repo", "config", NULL);

  kf = g_key_file_new ();
  if (!g_key_file_load_from_file (kf, config_path, G_KEY_FILE_NONE, error))
    return NULL;

  set = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  masked_str = g_key_file_get_string (kf, "core", "xa.masked", &local_error);
  if (masked_str == NULL)
    {
      if (g_error_matches (local_error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND) ||
          g_error_matches (local_error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND))
        return g_steal_pointer (&set);

      g_propagate_error (error, g_steal_pointer (&local_error));
      return NULL;
    }

  entries = g_strsplit (masked_str, ";", -1);
  for (guint i = 0; entries[i] != NULL; i++)
    {
      if (*entries[i] == '\0')
        continue;

      g_hash_table_add (set, g_strdup (entries[i]));
    }

  return g_steal_pointer (&set);
}

static DexFuture *
get_masked_ids_fiber (GetMaskedIdsData *data)
{
  g_autoptr (BzFlatpakInstance) self  = NULL;
  FlatpakInstallation *system         = NULL;
  FlatpakInstallation *user           = NULL;
  g_autoptr (GHashTable) system_masks = NULL;
  g_autoptr (GHashTable) user_masks   = NULL;
  g_autoptr (GHashTable) merged       = NULL;
  g_autoptr (GError) local_error      = NULL;
  GHashTableIter iter                 = { 0 };
  gpointer       key                  = NULL;

  bz_weak_get_or_return_reject (self, data->self);

  system = bz_flatpak_instance_get_system_installation (self);
  user   = bz_flatpak_instance_get_user_installation (self);

  merged = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  if (system != NULL)
    {
      system_masks = read_masks_for_installation (system, &local_error);
      if (system_masks == NULL)
        {
          g_warning ("Failed to read system masks: %s", local_error->message);
          g_clear_error (&local_error);
        }
      else
        {
          g_hash_table_iter_init (&iter, system_masks);
          while (g_hash_table_iter_next (&iter, &key, NULL))
            g_hash_table_add (merged, g_strdup (key));
        }
    }

  if (user != NULL)
    {
      user_masks = read_masks_for_installation (user, &local_error);
      if (user_masks == NULL)
        {
          g_warning ("Failed to read user masks: %s", local_error->message);
          g_clear_error (&local_error);
        }
      else
        {
          g_hash_table_iter_init (&iter, user_masks);
          while (g_hash_table_iter_next (&iter, &key, NULL))
            g_hash_table_add (merged, g_strdup (key));
        }
    }

  return dex_future_new_take_boxed (G_TYPE_HASH_TABLE, g_steal_pointer (&merged));
}

static DexFuture *
set_mask_fiber (SetMaskData *data)
{
  g_autoptr (BzFlatpakInstance) self = NULL;
  GCancellable *cancellable          = data->cancellable;
  g_autoptr (GError) local_error     = NULL;
  g_autoptr (GSubprocess) subprocess = NULL;
  gboolean result                    = FALSE;

  bz_weak_get_or_return_reject (self, data->self);

  {
    g_autoptr (GPtrArray) argv        = NULL;
    FlatpakInstallation *installation = NULL;

    argv = g_ptr_array_new_with_free_func (g_free);

#ifdef SANDBOXED_LIBFLATPAK
    g_ptr_array_add (argv, g_strdup ("flatpak-spawn"));
    g_ptr_array_add (argv, g_strdup ("--host"));
#endif

    g_ptr_array_add (argv, g_strdup ("flatpak"));
    g_ptr_array_add (argv, g_strdup ("mask"));

    installation = data->user
                       ? bz_flatpak_instance_get_user_installation (self)
                       : bz_flatpak_instance_get_system_installation (self);

    if (installation == NULL)
      return dex_future_new_reject (
          BZ_FLATPAK_ERROR,
          BZ_FLATPAK_ERROR_CANNOT_INITIALIZE,
          "Installation not available for mask operation on %s",
          data->app_id);

    {
      const char *id = flatpak_installation_get_id (installation);

      if (g_strcmp0 (id, "user") == 0)
        g_ptr_array_add (argv, g_strdup ("--user"));
      else if (g_strcmp0 (id, "default") == 0)
        g_ptr_array_add (argv, g_strdup ("--system"));
      else
        g_ptr_array_add (argv, g_strdup_printf ("--installation=%s", id));
    }

    if (!data->masked)
      g_ptr_array_add (argv, g_strdup ("--remove"));

    g_ptr_array_add (argv, g_strdup (data->app_id));
    g_ptr_array_add (argv, NULL);

    subprocess = g_subprocess_newv (
        (const char *const *) argv->pdata,
        G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
            G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &local_error);
  }

  if (subprocess == NULL)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
        "Failed to spawn flatpak mask for %s: %s",
        data->app_id, local_error->message);

  result = dex_await_boolean (
      dex_subprocess_wait_check (subprocess),
      &local_error);
  if (!result)
    {
      g_autofree char *stderr_output = NULL;

      g_subprocess_communicate_utf8 (subprocess, NULL, cancellable, NULL, &stderr_output, NULL);
      return dex_future_new_reject (
          BZ_FLATPAK_ERROR,
          BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
          "flatpak mask failed for %s: %s",
          data->app_id,
          stderr_output != NULL ? stderr_output : local_error->message);
    }

  return dex_future_new_for_boolean (TRUE);
}

DexFuture *
bz_flatpak_instance_get_masked_ids (BzFlatpakInstance *self,
                                    GCancellable      *cancellable)
{
  g_autoptr (GetMaskedIdsData) data = NULL;

  dex_return_error_if_fail (BZ_IS_FLATPAK_INSTANCE (self));
  dex_return_error_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data              = get_masked_ids_data_new ();
  data->self        = bz_track_weak (self);
  data->cancellable = bz_object_maybe_ref (cancellable);

  return dex_scheduler_spawn (
      dex_scheduler_get_thread_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) get_masked_ids_fiber,
      get_masked_ids_data_ref (data),
      get_masked_ids_data_unref);
}

DexFuture *
bz_flatpak_instance_set_mask (BzFlatpakInstance *self,
                              const char        *app_id,
                              gboolean           masked,
                              gboolean           user,
                              GCancellable      *cancellable)
{
  g_autoptr (SetMaskData) data = NULL;

  dex_return_error_if_fail (BZ_IS_FLATPAK_INSTANCE (self));
  dex_return_error_if_fail (app_id != NULL);
  dex_return_error_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data              = set_mask_data_new ();
  data->self        = bz_track_weak (self);
  data->cancellable = bz_object_maybe_ref (cancellable);
  data->app_id      = g_strdup (app_id);
  data->masked      = masked;
  data->user        = user;

  return dex_scheduler_spawn (
      dex_scheduler_get_thread_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) set_mask_fiber,
      set_mask_data_ref (data),
      set_mask_data_unref);
}
