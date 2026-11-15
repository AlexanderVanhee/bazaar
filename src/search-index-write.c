/* search-index-write.c
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

#include "config.h"

#define G_LOG_DOMAIN  "BAZAAR::SEARCH-INDEX-WRITE"
#define BAZAAR_MODULE "search-index-writer"

#include "bz-entry-group.h"
#include "env.h"
#include "search-index-write.h"

#define INDEX_MAGIC         "BZSI"
#define INDEX_VERSION       1
#define DESCRIPTION_MAX_LEN 200
#define WRITE_DELAY_SECONDS 3

static DexFuture  *write_index_fiber (GListModel *groups, const char *out_path, DexFuture *wait_for);
static void        put_uint32 (GByteArray *buf, guint32 value);
static void        put_string (GByteArray *buf, const char *str, gsize max_len);
static gboolean    entry_group_is_eligible (BzEntryGroup *group);
static const char *entry_group_icon_path (BzEntryGroup *group);

DexFuture *
bz_write_search_index (GListModel *groups,
                       const char *out_path,
                       DexFuture  *wait_for)
{
  dex_return_error_if_fail (G_IS_LIST_MODEL (groups));
  dex_return_error_if_fail (out_path != NULL);
  dex_return_error_if_fail (DEX_IS_FUTURE (wait_for));

  return dex_scheduler_spawnv (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      G_CALLBACK (write_index_fiber),
      3,
      G_TYPE_LIST_MODEL, groups,
      G_TYPE_STRING, out_path,
      DEX_TYPE_FUTURE, wait_for);
}

static DexFuture *
write_index_fiber (GListModel *groups,
                   const char *out_path,
                   DexFuture  *wait_for)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GByteArray) buf     = NULL;
  g_autoptr (GBytes) bytes       = NULL;
  g_autoptr (GFile) out_file     = NULL;
  guint   n_groups               = 0;
  guint32 count                  = 0;

  dex_await (dex_ref (wait_for), NULL);
  dex_await (dex_timeout_new_seconds (WRITE_DELAY_SECONDS), NULL);

  buf = g_byte_array_new ();
  g_byte_array_append (buf, (const guint8 *) INDEX_MAGIC, 4);
  put_uint32 (buf, INDEX_VERSION);
  put_uint32 (buf, 0);

  n_groups = g_list_model_get_n_items (groups);
  for (guint i = 0; i < n_groups; i++)
    {
      g_autoptr (BzEntryGroup) group = g_list_model_get_item (groups, i);

      if (!entry_group_is_eligible (group))
        continue;

      put_string (buf, bz_entry_group_get_id (group), 0);
      put_string (buf, bz_entry_group_get_title (group), 0);
      put_string (buf, bz_entry_group_get_developer (group), 0);
      put_string (buf, bz_entry_group_get_description (group), DESCRIPTION_MAX_LEN);
      put_string (buf, bz_entry_group_get_search_tokens (group), 0);
      put_string (buf, entry_group_icon_path (group), 0);
      count++;
    }

  count = GUINT32_TO_LE (count);
  memcpy (buf->data + 8, &count, sizeof (count));

  bytes    = g_byte_array_free_to_bytes (g_steal_pointer (&buf));
  out_file = g_file_new_for_path (out_path);
  if (!dex_await (
          dex_file_replace_contents_bytes (
              out_file, bytes, NULL, FALSE,
              G_FILE_CREATE_REPLACE_DESTINATION),
          &local_error))
    {
      g_warning ("Failed to write search index to %s: %s", out_path, local_error->message);
      return dex_future_new_for_error (g_steal_pointer (&local_error));
    }

  return dex_future_new_true ();
}

static void
put_uint32 (GByteArray *buf,
            guint32     value)
{
  value = GUINT32_TO_LE (value);
  g_byte_array_append (buf, (const guint8 *) &value, sizeof (value));
}

static void
put_string (GByteArray *buf,
            const char *str,
            gsize       max_len)
{
  gsize len = str != NULL ? strlen (str) : 0;

  if (max_len > 0)
    len = MIN (len, max_len);

  put_uint32 (buf, len);
  g_byte_array_append (buf, (const guint8 *) str, len);
}

static gboolean
entry_group_is_eligible (BzEntryGroup *group)
{
  return bz_entry_group_is_searchable (group) &&
         !bz_entry_group_is_addon (group) &&
         bz_entry_group_get_removable (group) == 0 &&
         bz_entry_group_get_eol (group) == NULL;
}

static const char *
entry_group_icon_path (BzEntryGroup *group)
{
  GIcon *icon = NULL;

  icon = bz_entry_group_get_mini_icon (group);
  if (icon == NULL || !G_IS_FILE_ICON (icon))
    return NULL;

  return g_file_peek_path (g_file_icon_get_file (G_FILE_ICON (icon)));
}
