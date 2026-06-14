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

#include <json-glib/json-glib.h>
#include <libsecret/secret.h>
#include <libsoup/soup.h>

#include "bz-async-texture.h"
#include "bz-auth-state.h"
#include "bz-global-net.h"

#define SECRET_SCHEMA_NAME "io.github.kolunmi.Bazaar.FlathubAuth"
#define SECRET_LABEL       "Flathub Authentication"
#define TOKEN_EXPIRY_BUFFER_SECONDS 60
#define OIDC_TOKEN_URL "http://localhost:8000/api/v2/oidc/token"
#define OIDC_CLIENT_ID "bazaar"
#define OIDC_CLIENT_SECRET "test-client-secret"

struct _BzAuthState
{
  GObject parent_instance;

  char      *name;
  char      *access_token;
  char      *refresh_token;
  char      *profile_icon_url;
  gint64     access_token_expiry;
  gboolean   loading;

  BzAsyncTexture *paintable;
};

G_DEFINE_FINAL_TYPE (BzAuthState, bz_auth_state, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_NAME,
  PROP_PROFILE_ICON_URL,
  PROP_AUTHENTICATED,
  PROP_PAINTABLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

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

static void
save_to_secrets (BzAuthState *self)
{
  g_autoptr (GHashTable) attributes   = NULL;
  g_autoptr (GVariantBuilder) builder = NULL;
  g_autoptr (GVariant) variant        = NULL;
  g_autofree char *serialized         = NULL;
  g_autoptr (GError) error            = NULL;

  if (self->loading)
    return;

  attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_hash_table_insert (attributes, g_strdup ("service"), g_strdup ("flathub"));

  builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);

  if (self->name != NULL)
    g_variant_builder_add (builder, "{sv}", "name", g_variant_new_string (self->name));
  if (self->access_token != NULL)
    g_variant_builder_add (builder, "{sv}", "access-token", g_variant_new_string (self->access_token));
  if (self->refresh_token != NULL)
    g_variant_builder_add (builder, "{sv}", "refresh-token", g_variant_new_string (self->refresh_token));
  g_variant_builder_add (builder, "{sv}", "access-token-expiry", g_variant_new_int64 (self->access_token_expiry));
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
      &error);

  if (error != NULL)
    g_warning ("Failed to save authentication to secrets: %s", error->message);
}

static void
load_from_secrets (BzAuthState *self)
{
  g_autoptr (GHashTable) attributes = NULL;
  g_autoptr (GError) error          = NULL;
  g_autofree char *secret           = NULL;

  attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_hash_table_insert (attributes, g_strdup ("service"), g_strdup ("flathub"));

  secret = secret_password_lookupv_sync (
      get_secret_schema (),
      attributes,
      NULL,
      &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, SECRET_ERROR, SECRET_ERROR_NO_SUCH_OBJECT))
        g_warning ("Failed to load authentication from secrets: %s", error->message);
      return;
    }

  if (secret != NULL)
    {
      g_autoptr (GVariant) variant  = NULL;
      g_autoptr (GVariantIter) iter = NULL;
      g_autoptr (GError) parse_error = NULL;

      variant = g_variant_parse (G_VARIANT_TYPE_VARDICT, secret, NULL, NULL, &parse_error);
      if (parse_error != NULL)
        {
          g_warning ("Failed to parse secret: %s", parse_error->message);
          return;
        }

      if (variant != NULL)
        {
          iter = g_variant_iter_new (variant);
          for (;;)
            {
              g_autofree char *key       = NULL;
              g_autoptr (GVariant) value = NULL;

              if (!g_variant_iter_next (iter, "{sv}", &key, &value))
                break;

              if (g_strcmp0 (key, "name") == 0)
                {
                  g_clear_pointer (&self->name, g_free);
                  self->name = g_variant_dup_string (value, NULL);
                }
              else if (g_strcmp0 (key, "access-token") == 0)
                {
                  g_clear_pointer (&self->access_token, g_free);
                  self->access_token = g_variant_dup_string (value, NULL);
                }
              else if (g_strcmp0 (key, "refresh-token") == 0)
                {
                  g_clear_pointer (&self->refresh_token, g_free);
                  self->refresh_token = g_variant_dup_string (value, NULL);
                }
              else if (g_strcmp0 (key, "access-token-expiry") == 0)
                {
                  self->access_token_expiry = g_variant_get_int64 (value);
                }
              else if (g_strcmp0 (key, "profile-icon-url") == 0)
                {
                  g_clear_pointer (&self->profile_icon_url, g_free);
                  self->profile_icon_url = g_variant_dup_string (value, NULL);

                  g_clear_object (&self->paintable);
                  if (self->profile_icon_url != NULL && self->profile_icon_url[0] != '\0')
                    {
                      g_autoptr (GFile) file = g_file_new_for_uri (self->profile_icon_url);
                      self->paintable        = bz_async_texture_new (file, NULL);
                    }
                }
            }
        }
    }
}

static void
clear_secrets (BzAuthState *self)
{
  g_autoptr (GHashTable) attributes = NULL;
  g_autoptr (GError) local_error    = NULL;

  attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  g_hash_table_replace (attributes, g_strdup ("service"), g_strdup ("flathub"));

  if (!secret_password_clearv_sync (get_secret_schema (), attributes, NULL, &local_error))
    g_warning ("Failed to clear auth values from secrets: %s", local_error->message);
}

static DexFuture *
do_token_refresh (BzAuthState *self)
{
  g_autoptr (SoupSession) session = NULL;
  g_autoptr (SoupMessage) msg     = NULL;
  g_autofree char *body           = NULL;
  g_autoptr (GBytes) body_bytes   = NULL;
  g_autoptr (GBytes) response     = NULL;
  g_autoptr (GError) error        = NULL;
  g_autoptr (JsonParser) parser   = NULL;
  JsonNode   *root                = NULL;
  JsonObject *obj                 = NULL;
  const char *new_access_token    = NULL;
  const char *new_refresh_token   = NULL;
  gint64      expires_in          = 3600;

  body = g_strdup_printf (
      "grant_type=refresh_token"
      "&refresh_token=%s"
      "&client_id=%s"
      "&client_secret=%s",
      self->refresh_token,
      OIDC_CLIENT_ID,
      OIDC_CLIENT_SECRET);

  session = soup_session_new ();
  msg     = soup_message_new ("POST", OIDC_TOKEN_URL);
  soup_message_headers_append (soup_message_get_request_headers (msg),
                               "Content-Type", "application/x-www-form-urlencoded");
  body_bytes = g_bytes_new (body, strlen (body));
  soup_message_set_request_body_from_bytes (msg, "application/x-www-form-urlencoded", body_bytes);

  response = soup_session_send_and_read (session, msg, NULL, &error);
  if (response == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser,
                                   g_bytes_get_data (response, NULL),
                                   g_bytes_get_size (response),
                                   &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  root = json_parser_get_root (parser);
  obj  = json_node_get_object (root);

  if (json_object_has_member (obj, "error"))
    {
      const char *err = json_object_get_string_member (obj, "error");
      return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_FAILED, "Token refresh failed: %s", err);
    }

  new_access_token  = json_object_get_string_member (obj, "access_token");
  new_refresh_token = json_object_get_string_member (obj, "refresh_token");

  if (json_object_has_member (obj, "expires_in"))
    expires_in = json_object_get_int_member (obj, "expires_in");

  if (new_access_token == NULL)
    return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_FAILED, "No access token in refresh response");

  g_clear_pointer (&self->access_token, g_free);
  self->access_token = g_strdup (new_access_token);
  self->access_token_expiry = g_get_real_time () / G_USEC_PER_SEC + expires_in;

  if (new_refresh_token != NULL)
    {
      g_clear_pointer (&self->refresh_token, g_free);
      self->refresh_token = g_strdup (new_refresh_token);
    }

  save_to_secrets (self);

  return dex_future_new_take_string (g_strdup (new_access_token));
}

static DexFuture *
refresh_fiber (BzAuthState *self)
{
  return do_token_refresh (self);
}

static void
bz_auth_state_dispose (GObject *object)
{
  BzAuthState *self = BZ_AUTH_STATE (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->access_token, g_free);
  g_clear_pointer (&self->refresh_token, g_free);
  g_clear_pointer (&self->profile_icon_url, g_free);
  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (bz_auth_state_parent_class)->dispose (object);
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
  object_class->get_property = bz_auth_state_get_property;

  properties[PROP_NAME] =
      g_param_spec_string (
          "name",
          NULL, NULL, NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PROFILE_ICON_URL] =
      g_param_spec_string (
          "profile-icon-url",
          NULL, NULL, NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_AUTHENTICATED] =
      g_param_spec_boolean (
          "authenticated",
          NULL, NULL, FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PAINTABLE] =
      g_param_spec_object (
          "paintable",
          NULL, NULL,
          GDK_TYPE_PAINTABLE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
bz_auth_state_init (BzAuthState *self)
{
  self->loading = TRUE;
  load_from_secrets (self);
  self->loading = FALSE;
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
bz_auth_state_get_profile_icon_url (BzAuthState *self)
{
  g_return_val_if_fail (BZ_IS_AUTH_STATE (self), NULL);
  return self->profile_icon_url;
}

gboolean
bz_auth_state_is_authenticated (BzAuthState *self)
{
  g_return_val_if_fail (BZ_IS_AUTH_STATE (self), FALSE);
  return self->refresh_token != NULL && self->refresh_token[0] != '\0';
}

GdkPaintable *
bz_auth_state_get_paintable (BzAuthState *self)
{
  g_return_val_if_fail (BZ_IS_AUTH_STATE (self), NULL);
  return GDK_PAINTABLE (self->paintable);
}

DexFuture *
bz_auth_state_get_access_token (BzAuthState *self)
{
  gint64 now = 0;

  g_return_val_if_fail (BZ_IS_AUTH_STATE (self), dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_FAILED, "Invalid auth state"));

  if (!bz_auth_state_is_authenticated (self))
    return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "Not authenticated");

  now = g_get_real_time () / G_USEC_PER_SEC;

  if (self->access_token != NULL &&
      self->access_token_expiry > now + TOKEN_EXPIRY_BUFFER_SECONDS)
    return dex_future_new_take_string (g_strdup (self->access_token));

  if (self->refresh_token == NULL)
    return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "No refresh token");

  return dex_scheduler_spawn (
      dex_scheduler_get_default (),
      0,
      (DexFiberFunc) refresh_fiber,
      g_object_ref (self),
      g_object_unref);
}

void
bz_auth_state_set_authenticated (BzAuthState *self,
                                 const char  *name,
                                 const char  *access_token,
                                 gint64       expires_in_seconds,
                                 const char  *refresh_token,
                                 const char  *profile_icon_url)
{
  gboolean was_authenticated = FALSE;
  gboolean name_changed      = FALSE;
  gboolean token_changed     = FALSE;
  gboolean icon_changed      = FALSE;

  g_return_if_fail (BZ_IS_AUTH_STATE (self));

  was_authenticated = bz_auth_state_is_authenticated (self);

  if (g_strcmp0 (self->name, name) != 0)
    {
      g_clear_pointer (&self->name, g_free);
      self->name   = g_strdup (name);
      name_changed = TRUE;
    }

  if (g_strcmp0 (self->access_token, access_token) != 0)
    {
      g_clear_pointer (&self->access_token, g_free);
      self->access_token = g_strdup (access_token);
      token_changed      = TRUE;
    }

  if (access_token != NULL && expires_in_seconds > 0)
    self->access_token_expiry = g_get_real_time () / G_USEC_PER_SEC + expires_in_seconds;
  else
    self->access_token_expiry = 0;

  if (g_strcmp0 (self->refresh_token, refresh_token) != 0)
    {
      g_clear_pointer (&self->refresh_token, g_free);
      self->refresh_token = g_strdup (refresh_token);
      token_changed       = TRUE;
    }

  if (g_strcmp0 (self->profile_icon_url, profile_icon_url) != 0)
    {
      g_clear_pointer (&self->profile_icon_url, g_free);
      self->profile_icon_url = g_strdup (profile_icon_url);
      icon_changed           = TRUE;

      g_clear_object (&self->paintable);
      if (profile_icon_url != NULL && profile_icon_url[0] != '\0')
        {
          g_autoptr (GFile) file = g_file_new_for_uri (profile_icon_url);
          self->paintable        = bz_async_texture_new (file, NULL);
        }
    }

  if (name_changed)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
  if (token_changed)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTHENTICATED]);
  if (icon_changed)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROFILE_ICON_URL]);

  if (!!was_authenticated != !!bz_auth_state_is_authenticated (self))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_AUTHENTICATED]);

  save_to_secrets (self);
}

void
bz_auth_state_clear (BzAuthState *self)
{
  g_return_if_fail (BZ_IS_AUTH_STATE (self));

  clear_secrets (self);
  bz_auth_state_set_authenticated (self, NULL, NULL, 0, NULL, NULL);
}
