/* bz-auth-state.c
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

#include "config.h"

#include <libdex.h>
#include <libsecret/secret.h>

#include "bz-async-texture.h"
#include "bz-auth-state.h"
#include "env.h"

#define SECRET_SCHEMA_NAME "io.github.kolunmi.Bazaar.FlathubAuth"
#define SECRET_LABEL       "Flathub Authentication"

struct _BzAuthState
{
  GObject parent_instance;

  char           *name;
  char           *token;
  char           *profile_icon_url;
  GDateTime      *token_expires;
  BzAsyncTexture *paintable;

  gboolean loading;
  gboolean changed_while_loading;
  guint    expiration_timeout_id;
};

G_DEFINE_FINAL_TYPE (BzAuthState, bz_auth_state, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_NAME,
  PROP_TOKEN,
  PROP_PROFILE_ICON_URL,
  PROP_AUTHENTICATED,
  PROP_PAINTABLE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static const SecretSchema *
get_secret_schema (void);

static gboolean
on_token_expired (BzAuthState *self);

static void
schedule_token_expiration (BzAuthState *self);

static void
save_to_secrets (BzAuthState *self);

static void
clear_secrets (BzAuthState *self);

static DexFuture *
lookup_secret_fiber (gpointer data);

static DexFuture *
lookup_secret_then (DexFuture   *future,
                    BzAuthState *self);

static void
bz_auth_state_dispose (GObject *object)
{
  BzAuthState *self = BZ_AUTH_STATE (object);

  g_clear_handle_id (&self->expiration_timeout_id, g_source_remove);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->token, g_free);
  g_clear_pointer (&self->profile_icon_url, g_free);
  g_clear_pointer (&self->token_expires, g_date_time_unref);
  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (bz_auth_state_parent_class)->dispose (object);
}

static void
bz_auth_state_finalize (GObject *object)
{
  BzAuthState *self = BZ_AUTH_STATE (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->token, g_free);
  g_clear_pointer (&self->profile_icon_url, g_free);
  g_clear_pointer (&self->token_expires, g_date_time_unref);

  G_OBJECT_CLASS (bz_auth_state_parent_class)->finalize (object);
}

static void
bz_auth_state_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  BzAuthState *self = BZ_AUTH_STATE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_TOKEN:
      g_value_set_string (value, self->token);
      break;
    case PROP_PROFILE_ICON_URL:
      g_value_set_string (value, self->profile_icon_url);
      break;
    case PROP_AUTHENTICATED:
      g_value_set_boolean (value, bz_auth_state_is_authenticated (self));
      break;
    case PROP_PAINTABLE:
      g_value_set_object (value, bz_auth_state_get_paintable (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_auth_state_class_init (BzAuthStateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = bz_auth_state_dispose;
  object_class->finalize     = bz_auth_state_finalize;
  object_class->get_property = bz_auth_state_get_property;

  props[PROP_NAME] =
      g_param_spec_string (
          "name",
          NULL, NULL,
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_TOKEN] =
      g_param_spec_string (
          "token",
          NULL, NULL,
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_PROFILE_ICON_URL] =
      g_param_spec_string (
          "profile-icon-url",
          NULL, NULL,
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_AUTHENTICATED] =
      g_param_spec_boolean (
          "authenticated",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_PAINTABLE] =
      g_param_spec_object (
          "paintable",
          NULL, NULL,
          GDK_TYPE_PAINTABLE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_auth_state_init (BzAuthState *self)
{
  g_autoptr (DexFuture) future = NULL;

  future = dex_scheduler_spawn (
      dex_thread_pool_scheduler_get_default (),
      bz_get_dex_stack_size (),
      lookup_secret_fiber,
      NULL, NULL);
  future = dex_future_then (
      future,
      (DexFutureCallback) lookup_secret_then,
      g_object_ref (self), g_object_unref);
  dex_future_disown (g_steal_pointer (&future));
}

BzAuthState *
bz_auth_state_new (void)
{
  return g_object_new (BZ_TYPE_AUTH_STATE, NULL);
}

const char *
bz_auth_state_get_name (BzAuthState *self)
{
  g_return_val_if_fail (BZ_IS_AUTH_STATE (self), NULL);
  return self->name;
}

const char *
bz_auth_state_get_token (BzAuthState *self)
{
  g_return_val_if_fail (BZ_IS_AUTH_STATE (self), NULL);
  return self->token;
}

const char *
bz_auth_state_get_profile_icon_url (BzAuthState *self)
{
  g_return_val_if_fail (BZ_IS_AUTH_STATE (self), NULL);
  return self->profile_icon_url;
}

gboolean
bz_auth_state_is_authenticated (BzAuthState *self)
{
  g_return_val_if_fail (BZ_IS_AUTH_STATE (self), FALSE);
  return self->token != NULL && self->token[0] != '\0';
}

GdkPaintable *
bz_auth_state_get_paintable (BzAuthState *self)
{
  g_return_val_if_fail (BZ_IS_AUTH_STATE (self), NULL);
  return (GdkPaintable *) self->paintable;
}

void
bz_auth_state_set_authenticated (BzAuthState *self,
                                 const char  *name,
                                 const char  *token,
                                 GDateTime   *token_expires,
                                 const char  *profile_icon_url)
{
  gboolean was_authenticated = FALSE;
  gboolean name_changed      = FALSE;
  gboolean token_changed     = FALSE;
  gboolean icon_changed      = FALSE;

  g_return_if_fail (BZ_IS_AUTH_STATE (self));

  if (!self->loading)
    self->changed_while_loading = TRUE;

  was_authenticated = bz_auth_state_is_authenticated (self);

  if (g_strcmp0 (self->name, name) != 0)
    {
      g_clear_pointer (&self->name, g_free);
      self->name   = g_strdup (name);
      name_changed = TRUE;
    }

  if (g_strcmp0 (self->token, token) != 0)
    {
      g_clear_pointer (&self->token, g_free);
      self->token   = g_strdup (token);
      token_changed = TRUE;
    }

  g_clear_pointer (&self->token_expires, g_date_time_unref);
  if (token_expires != NULL)
    self->token_expires = g_date_time_ref (token_expires);

  if (g_strcmp0 (self->profile_icon_url, profile_icon_url) != 0)
    {
      g_clear_pointer (&self->profile_icon_url, g_free);
      self->profile_icon_url = g_strdup (profile_icon_url);
      icon_changed           = TRUE;

      g_clear_object (&self->paintable);
      if (profile_icon_url != NULL && profile_icon_url[0] != '\0')
        {
          g_autoptr (GFile) file = NULL;

          file            = g_file_new_for_uri (profile_icon_url);
          self->paintable = bz_async_texture_new (file, NULL);
        }
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PAINTABLE]);
    }

  if (name_changed)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
  if (token_changed)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TOKEN]);
  if (icon_changed)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROFILE_ICON_URL]);

  if (!!was_authenticated != !!bz_auth_state_is_authenticated (self))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_AUTHENTICATED]);

  schedule_token_expiration (self);
  save_to_secrets (self);
}

void
bz_auth_state_clear (BzAuthState *self)
{
  g_return_if_fail (BZ_IS_AUTH_STATE (self));

  g_clear_handle_id (&self->expiration_timeout_id, g_source_remove);
  clear_secrets (self);
  bz_auth_state_set_authenticated (self, NULL, NULL, NULL, NULL);
}

static const SecretSchema *
get_secret_schema (void)
{
  static const SecretSchema schema = {
    SECRET_SCHEMA_NAME,
    SECRET_SCHEMA_NONE,
    {
      { "service", SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
      }
  };

  return &schema;
}

static gboolean
on_token_expired (BzAuthState *self)
{
  self->expiration_timeout_id = 0;
  bz_auth_state_clear (self);
  return G_SOURCE_REMOVE;
}

static void
schedule_token_expiration (BzAuthState *self)
{
  g_autoptr (GDateTime) now       = NULL;
  gint64 seconds_until_expiration = 0;

  if (self->expiration_timeout_id != 0)
    g_source_remove (self->expiration_timeout_id);

  self->expiration_timeout_id = 0;

  if (self->token_expires == NULL)
    return;

  now                      = g_date_time_new_now_utc ();
  seconds_until_expiration = g_date_time_difference (self->token_expires, now) / G_TIME_SPAN_SECOND;

  if (seconds_until_expiration <= 0)
    {
      bz_auth_state_clear (self);
      return;
    }

  if (seconds_until_expiration > G_MAXUINT / 1000)
    self->expiration_timeout_id = g_timeout_add_seconds (G_MAXUINT / 1000, (GSourceFunc) on_token_expired, self);
  else
    self->expiration_timeout_id = g_timeout_add_seconds (seconds_until_expiration, (GSourceFunc) on_token_expired, self);
}

static void
save_to_secrets (BzAuthState *self)
{
  g_autoptr (GHashTable) attributes   = NULL;
  g_autoptr (GError) local_error      = NULL;
  g_autoptr (GVariantBuilder) builder = NULL;
  g_autoptr (GVariant) variant        = NULL;
  g_autofree char *serialized         = NULL;

  if (self->loading)
    return;

  attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_hash_table_insert (attributes, g_strdup ("service"), g_strdup ("flathub"));

  builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);

  if (self->name != NULL)
    g_variant_builder_add (builder, "{sv}", "name", g_variant_new_string (self->name));
  if (self->token != NULL)
    g_variant_builder_add (builder, "{sv}", "token", g_variant_new_string (self->token));
  if (self->token_expires != NULL)
    {
      g_autofree char *expires = NULL;

      expires = g_date_time_format_iso8601 (self->token_expires);
      g_variant_builder_add (builder, "{sv}", "token-expires", g_variant_new_string (expires));
    }
  if (self->profile_icon_url != NULL)
    g_variant_builder_add (builder, "{sv}", "profile-icon-url", g_variant_new_string (self->profile_icon_url));

  variant    = g_variant_builder_end (builder);
  serialized = g_variant_print (variant, FALSE);

  secret_password_storev_sync (
      get_secret_schema (),
      attributes,
      NULL,
      SECRET_LABEL,
      serialized,
      NULL,
      &local_error);
  if (local_error != NULL)
    g_warning ("Failed to save authentication to secrets: %s", local_error->message);
}

static void
clear_secrets (BzAuthState *self)
{
  gboolean result                   = FALSE;
  g_autoptr (GError) local_error    = NULL;
  g_autoptr (GHashTable) attributes = NULL;

  attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_hash_table_replace (attributes, g_strdup ("service"), g_strdup ("flathub"));

  result = secret_password_clearv_sync (
      get_secret_schema (),
      attributes,
      NULL,
      &local_error);
  if (!result)
    g_warning ("Failed to clear auth values from secrets: %s", local_error->message);
}

static DexFuture *
lookup_secret_fiber (gpointer data)
{
  g_autoptr (GHashTable) attributes = NULL;
  g_autoptr (GError) local_error    = NULL;
  char *secret                      = NULL;

  attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_hash_table_insert (attributes, g_strdup ("service"), g_strdup ("flathub"));

  secret = secret_password_lookupv_sync (
      get_secret_schema (),
      attributes,
      NULL,
      &local_error);
  if (local_error != NULL)
    {
      if (!g_error_matches (local_error, SECRET_ERROR, SECRET_ERROR_NO_SUCH_OBJECT))
        g_warning ("Failed to load authentication from secrets: %s", local_error->message);
      return dex_future_new_for_error (g_steal_pointer (&local_error));
    }
  if (secret == NULL)
    return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "No stored authentication");

  return dex_future_new_take_string (secret);
}

static DexFuture *
lookup_secret_then (DexFuture   *future,
                    BzAuthState *self)
{
  g_autoptr (GError) local_error      = NULL;
  g_autoptr (GVariant) variant        = NULL;
  g_autofree char *name               = NULL;
  g_autofree char *token              = NULL;
  g_autofree char *profile_icon_url   = NULL;
  g_autoptr (GDateTime) token_expires = NULL;
  g_autoptr (GDateTime) now           = NULL;
  const char *secret                  = NULL;
  const char *expires_string          = NULL;

  if (self->changed_while_loading)
    return dex_future_new_true ();

  secret  = g_value_get_string (dex_future_get_value (future, NULL));
  variant = g_variant_parse (G_VARIANT_TYPE_VARDICT, secret, NULL, NULL, &local_error);
  if (variant == NULL)
    {
      g_warning ("Failed to parse secret: %s", local_error->message);
      return dex_future_new_for_error (g_steal_pointer (&local_error));
    }

  g_variant_lookup (variant, "name", "s", &name);
  g_variant_lookup (variant, "token", "s", &token);
  g_variant_lookup (variant, "profile-icon-url", "s", &profile_icon_url);
  if (g_variant_lookup (variant, "token-expires", "&s", &expires_string))
    token_expires = g_date_time_new_from_iso8601 (expires_string, NULL);

  self->loading = TRUE;
  now           = g_date_time_new_now_utc ();
  if (token_expires != NULL &&
      g_date_time_compare (now, token_expires) >= 0)
    bz_auth_state_clear (self);
  else
    bz_auth_state_set_authenticated (self, name, token, token_expires, profile_icon_url);
  self->loading = FALSE;

  return dex_future_new_true ();
}
