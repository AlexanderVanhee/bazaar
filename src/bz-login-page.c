/* bz-login-page.c
 *
 * Copyright 2025, 2026 Alexander Vanhee
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

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <libdex.h>
#include <libsoup/soup.h>
#include <sys/random.h>

#include "bz-auth-state.h"
#include "bz-env.h"
#include "bz-global-net.h"
#include "bz-login-page.h"
#include "bz-template-callbacks.h"
#include "bz-util.h"

#define OIDC_CLIENT_ID    "bazaar"
#define OIDC_REDIRECT_URI "https://usebazaar.org/callback"
#define OIDC_BASE_URL     "http://localhost:8000/api/v2"
#define OIDC_FRONTEND_URL "http://localhost:3000/api/v2"

struct _BzLoginPage
{
  AdwNavigationPage parent_instance;

  BzAuthState *auth_state;

  char *code_verifier;
  char *code_challenge;
  char *state;

  GtkStack      *main_stack;
  AdwStatusPage *error_status_page;
};

G_DEFINE_FINAL_TYPE (BzLoginPage, bz_login_page, ADW_TYPE_NAVIGATION_PAGE)

enum
{
  PROP_0,
  PROP_AUTH_STATE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

BZ_DEFINE_DATA (
    login_fiber,
    LoginFiber,
    {
      GWeakRef *self;
      char     *code;
      char     *code_verifier;
      char     *access_token;
      char     *refresh_token;
      char     *displayname;
      char     *avatar_url;
      gint64    expires_in;
    },
    BZ_RELEASE_DATA (self, bz_weak_release);
    BZ_RELEASE_DATA (code, g_free);
    BZ_RELEASE_DATA (code_verifier, g_free);
    BZ_RELEASE_DATA (access_token, g_free);
    BZ_RELEASE_DATA (refresh_token, g_free);
    BZ_RELEASE_DATA (displayname, g_free);
    BZ_RELEASE_DATA (avatar_url, g_free))

static void
show_error_take (BzLoginPage *self,
                 char        *message);

static DexFuture *
login_fiber (LoginFiberData *data);

static DexFuture *
login_fiber_finally (DexFuture      *future,
                     LoginFiberData *data);

static void
on_login_clicked (GtkButton   *button,
                  BzLoginPage *self);

static void
on_close_clicked (GtkButton   *button,
                  BzLoginPage *self);

static char *
base64url_encode (const guchar *data,
                  gsize         len)
{
  g_autofree char *b64 = g_base64_encode (data, len);
  char            *p   = NULL;
  char            *end = NULL;

  for (p = b64; *p; p++)
    {
      if (*p == '+')
        *p = '-';
      else if (*p == '/')
        *p = '_';
    }

  end = b64 + strlen (b64) - 1;
  while (end > b64 && *end == '=')
    *end-- = '\0';

  return g_steal_pointer (&b64);
}

static void
generate_pkce (BzLoginPage *self)
{
  guchar     verifier_bytes[48];
  guchar     challenge_bytes[32];
  guchar     state_bytes[16];
  GChecksum *checksum   = NULL;
  gsize      digest_len = 32;

  g_clear_pointer (&self->code_verifier, g_free);
  g_clear_pointer (&self->code_challenge, g_free);
  g_clear_pointer (&self->state, g_free);

  getrandom (verifier_bytes, sizeof (verifier_bytes), 0);
  self->code_verifier = base64url_encode (verifier_bytes, sizeof (verifier_bytes));

  checksum = g_checksum_new (G_CHECKSUM_SHA256);
  g_checksum_update (checksum, (const guchar *) self->code_verifier, strlen (self->code_verifier));
  g_checksum_get_digest (checksum, challenge_bytes, &digest_len);
  g_checksum_free (checksum);

  self->code_challenge = base64url_encode (challenge_bytes, digest_len);

  getrandom (state_bytes, sizeof (state_bytes), 0);
  self->state = base64url_encode (state_bytes, sizeof (state_bytes));
}

static void
bz_login_page_dispose (GObject *object)
{
  BzLoginPage *self = BZ_LOGIN_PAGE (object);

  g_clear_object (&self->auth_state);
  g_clear_pointer (&self->code_verifier, g_free);
  g_clear_pointer (&self->code_challenge, g_free);
  g_clear_pointer (&self->state, g_free);

  G_OBJECT_CLASS (bz_login_page_parent_class)->dispose (object);
}

static void
bz_login_page_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  BzLoginPage *self = BZ_LOGIN_PAGE (object);

  switch (prop_id)
    {
    case PROP_AUTH_STATE:
      g_value_set_object (value, self->auth_state);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_login_page_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  BzLoginPage *self = BZ_LOGIN_PAGE (object);

  switch (prop_id)
    {
    case PROP_AUTH_STATE:
      g_set_object (&self->auth_state, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static char *
format_greeting (gpointer    object,
                 const char *name)
{
  if (name == NULL || name[0] == '\0')
    return g_strdup (" ");
  return g_strdup_printf (_ ("Hello, %s!"), name);
}

static void
bz_login_page_class_init (BzLoginPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_login_page_dispose;
  object_class->get_property = bz_login_page_get_property;
  object_class->set_property = bz_login_page_set_property;

  properties[PROP_AUTH_STATE] =
      g_param_spec_object (
          "auth-state",
          NULL, NULL,
          BZ_TYPE_AUTH_STATE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/io/github/kolunmi/Bazaar/bz-login-page.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);

  gtk_widget_class_bind_template_child (widget_class, BzLoginPage, main_stack);
  gtk_widget_class_bind_template_child (widget_class, BzLoginPage, error_status_page);

  gtk_widget_class_bind_template_callback (widget_class, on_close_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_login_clicked);
  gtk_widget_class_bind_template_callback (widget_class, format_greeting);
}

static void
bz_login_page_init (BzLoginPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_stack_set_visible_child_name (self->main_stack, "start");
}

AdwNavigationPage *
bz_login_page_new (BzAuthState *auth_state)
{
  return g_object_new (BZ_TYPE_LOGIN_PAGE,
                       "auth-state", auth_state,
                       NULL);
}

void
bz_login_page_handle_uri (BzLoginPage *self,
                          const char  *uri)
{
  g_autoptr (GUri) parsed         = NULL;
  g_autoptr (GHashTable) params   = NULL;
  g_autoptr (LoginFiberData) data = NULL;
  g_autoptr (DexFuture) future    = NULL;
  const char *code                = NULL;
  const char *error               = NULL;

  g_return_if_fail (BZ_IS_LOGIN_PAGE (self));
  g_return_if_fail (uri != NULL);

  parsed = g_uri_parse (uri, G_URI_FLAGS_NONE, NULL);
  if (parsed == NULL)
    {
      show_error_take (self, g_strdup (_ ("Invalid callback URI")));
      return;
    }

  params = g_uri_parse_params (g_uri_get_query (parsed), -1, "&",
                               G_URI_PARAMS_NONE, NULL);
  if (params == NULL)
    {
      show_error_take (self, g_strdup (_ ("No parameters in callback URI")));
      return;
    }

  error = g_hash_table_lookup (params, "error");
  code  = g_hash_table_lookup (params, "code");

  if (error != NULL)
    {
      show_error_take (self, g_strdup_printf (_ ("Auth error: %s"), error));
      return;
    }

  if (code == NULL)
    {
      show_error_take (self, g_strdup (_ ("No code in callback")));
      return;
    }

  data                = login_fiber_data_new ();
  data->self          = bz_track_weak (self);
  data->code          = g_strdup (code);
  data->code_verifier = g_strdup (self->code_verifier);

  future = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) login_fiber,
      login_fiber_data_ref (data),
      login_fiber_data_unref);
  future = dex_future_finally (
      future,
      (DexFutureCallback) login_fiber_finally,
      login_fiber_data_ref (data),
      login_fiber_data_unref);
  dex_future_disown (g_steal_pointer (&future));
}

static void
show_error_take (BzLoginPage *self,
                 char        *message)
{
  g_autofree char *escaped = g_markup_escape_text (message, -1);

  adw_status_page_set_description (self->error_status_page, escaped);
  gtk_stack_set_visible_child_name (self->main_stack, "error");
  g_free (message);
}

static DexFuture *
login_fiber (LoginFiberData *data)
{
  g_autoptr (BzLoginPage) self           = NULL;
  g_autoptr (GError) error               = NULL;
  g_autoptr (SoupMessage) token_msg      = NULL;
  g_autoptr (SoupMessage) userinfo_msg   = NULL;
  g_autoptr (GBytes) token_bytes         = NULL;
  g_autoptr (GBytes) userinfo_bytes      = NULL;
  g_autoptr (JsonParser) parser          = NULL;
  g_autofree char *body                  = NULL;
  g_autofree char *auth_header           = NULL;
  g_autoptr (GBytes) body_bytes          = NULL;
  g_autoptr (GOutputStream) token_out    = NULL;
  g_autoptr (GOutputStream) userinfo_out = NULL;
  JsonNode   *root                       = NULL;
  JsonObject *obj                        = NULL;
  const char *raw                        = NULL;
  const char *access_token               = NULL;
  const char *refresh_token              = NULL;
  const char *displayname                = NULL;
  const char *avatar_url                 = NULL;
  gsize       raw_len                    = 0;

  bz_weak_get_or_return_reject (self, data->self);

  body = g_strdup_printf (
      "grant_type=authorization_code"
      "&code=%s"
      "&redirect_uri=%s"
      "&client_id=%s"
      "&client_secret=test-client-secret"
      "&code_verifier=%s",
      data->code,
      OIDC_REDIRECT_URI,
      OIDC_CLIENT_ID,
      data->code_verifier);

  body_bytes = g_bytes_new (body, strlen (body));
  token_msg  = soup_message_new ("POST", OIDC_BASE_URL "/oidc/token");
  soup_message_headers_append (soup_message_get_request_headers (token_msg),
                               "Content-Type", "application/x-www-form-urlencoded");
  soup_message_set_request_body_from_bytes (token_msg, "application/x-www-form-urlencoded", body_bytes);

  token_out = g_memory_output_stream_new_resizable ();
  dex_await (bz_send_with_global_http_session_then_splice_into (token_msg, token_out), &error);
  if (error != NULL)
    return dex_future_new_for_error (
        g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                     _ ("Token exchange failed: %s"), error->message));
  token_bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (token_out));

  raw    = g_bytes_get_data (token_bytes, &raw_len);
  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, raw, raw_len, &error))
    return dex_future_new_for_error (
        g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                     _ ("Failed to parse token response: %s"), error->message));

  root = json_parser_get_root (parser);
  if (root == NULL)
    return dex_future_new_for_error (
        g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                             _ ("Empty token response")));

  obj = json_node_get_object (root);
  if (obj == NULL)
    return dex_future_new_for_error (
        g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                             _ ("Unexpected token response format")));

  if (json_object_has_member (obj, "error"))
    {
      const char *desc = NULL;

      desc = json_object_has_member (obj, "error_description")
                 ? json_object_get_string_member (obj, "error_description")
                 : json_object_get_string_member (obj, "error");
      return dex_future_new_for_error (
          g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED, desc));
    }

  if (!json_object_has_member (obj, "access_token"))
    return dex_future_new_for_error (
        g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                             _ ("No access token in response")));

  access_token  = json_object_get_string_member (obj, "access_token");
  refresh_token = json_object_get_string_member_with_default (obj, "refresh_token", NULL);

  data->access_token  = g_strdup (access_token);
  data->refresh_token = g_strdup (refresh_token);
  data->expires_in    = json_object_has_member (obj, "expires_in")
                            ? json_object_get_int_member (obj, "expires_in")
                            : 3600;

  auth_header  = g_strdup_printf ("Bearer %s", access_token);
  userinfo_msg = soup_message_new ("GET", OIDC_BASE_URL "/oidc/userinfo");
  soup_message_headers_append (soup_message_get_request_headers (userinfo_msg),
                               "Authorization", auth_header);

  userinfo_out = g_memory_output_stream_new_resizable ();
  dex_await (bz_send_with_global_http_session_then_splice_into (userinfo_msg, userinfo_out), &error);
  if (error != NULL)
    return dex_future_new_for_error (
        g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                     _ ("Failed to fetch user info: %s"), error->message));
  userinfo_bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (userinfo_out));

  raw = g_bytes_get_data (userinfo_bytes, &raw_len);
  g_clear_object (&parser);
  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, raw, raw_len, &error))
    return dex_future_new_for_error (
        g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                     _ ("Failed to parse user info: %s"), error->message));

  root = json_parser_get_root (parser);
  if (root == NULL)
    return dex_future_new_for_error (
        g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                             _ ("Empty user info response")));

  obj = json_node_get_object (root);
  if (obj == NULL)
    return dex_future_new_for_error (
        g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                             _ ("Unexpected user info format")));

  displayname = json_object_get_string_member_with_default (obj, "name", NULL);
  if (displayname == NULL)
    displayname = json_object_get_string_member_with_default (obj, "preferred_username", "Unknown");

  avatar_url = json_object_get_string_member_with_default (obj, "picture", NULL);

  data->displayname = g_strdup (displayname);
  data->avatar_url  = g_strdup (avatar_url);

  return dex_future_new_true ();
}

static DexFuture *
login_fiber_finally (DexFuture      *future,
                     LoginFiberData *data)
{
  g_autoptr (BzLoginPage) self = NULL;
  g_autoptr (GError) error     = NULL;

  self = g_weak_ref_get (data->self);
  if (self == NULL)
    return dex_future_new_true ();

  if (!dex_await (dex_ref (future), &error))
    {
      show_error_take (self, g_strdup (error->message));
      return dex_future_new_true ();
    }

  if (self->auth_state != NULL)
    bz_auth_state_set_authenticated (self->auth_state,
                                     data->displayname, data->access_token,
                                     data->expires_in, data->refresh_token,
                                                         data->avatar_url);

  gtk_stack_set_visible_child_name (self->main_stack, "finish");
  return dex_future_new_true ();
}

static void
on_login_clicked (GtkButton   *button,
                  BzLoginPage *self)
{
  g_autofree char *url = NULL;

  generate_pkce (self);

  url = g_strdup_printf (
      OIDC_FRONTEND_URL "/oidc/authorize"
                        "?client_id=%s"
                        "&redirect_uri=%s"
                        "&response_type=code"
                        "&scope=openid%%20profile%%20email%%20offline_access"
                        "&state=%s"
                        "&code_challenge=%s"
                        "&code_challenge_method=S256",
      OIDC_CLIENT_ID,
      OIDC_REDIRECT_URI,
      self->state,
      self->code_challenge);

  gtk_stack_set_visible_child_name (self->main_stack, "waiting");

  g_app_info_launch_default_for_uri (url, NULL, NULL);
}

static void
on_close_clicked (GtkButton   *button,
                  BzLoginPage *self)
{
  GtkWidget *navigation_view = gtk_widget_get_ancestor (GTK_WIDGET (self),
                                                        ADW_TYPE_NAVIGATION_VIEW);
  if (navigation_view != NULL)
    adw_navigation_view_pop (ADW_NAVIGATION_VIEW (navigation_view));
}
