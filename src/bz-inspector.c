/* bz-inspector.c
 *
 * Copyright 2025 Adam Masciola
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

#define G_LOG_DOMAIN "BAZAAR::INSPECTOR"

#include "config.h"

#include <bzvala.h>
#include <json-glib/json-glib.h>

#include "bz-application.h"
#include "bz-entry-inspector.h"
#include "bz-inspector.h"
#include "bz-serializable.h"
#include "bz-window.h"
#include "dex-gtk-utils.h"
#include "env.h"
#include "template-callbacks.h"

struct _BzInspector
{
  AdwBin parent_instance;

  GBinding  *debug_mode_binding;
  GBinding  *disable_blocklists_binding;
  GtkWindow *preview_window;

  GtkCheckButton     *debug_mode_check;
  GtkCheckButton     *disable_blocklists_check;
  GtkEditable        *serialize_all_entries_path_entry;
  GtkCheckButton     *serialize_all_entries_open_externally;
  GtkCheckButton     *serialize_all_entries_open_externally_prefer_terminal;
  GtkEditable        *serialize_all_entries_open_externally_alt_program_entry;
  GtkButton          *serialize_all_entries_btn;
  GtkProgressBar     *serialize_all_entries_progress;
  GtkEditable        *search_entry;
  GtkFilterListModel *filter_model;
  GtkSingleSelection *groups_selection;
};

G_DEFINE_FINAL_TYPE (BzInspector, bz_inspector, ADW_TYPE_BIN);

enum
{
  PROP_0,

  PROP_STATE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static DexFuture *
serialize_all_entries_fiber (BzInspector *self);

static gboolean
filter_func (BzEntryGroup *group,
             BzInspector  *self);

static void
bz_inspector_dispose (GObject *object)
{
  BzInspector *self = BZ_INSPECTOR (object);

  g_clear_object (&self->debug_mode_binding);
  g_clear_object (&self->disable_blocklists_binding);
  if (self->preview_window != NULL)
    gtk_window_close (self->preview_window);
  g_clear_object (&self->preview_window);

  G_OBJECT_CLASS (bz_inspector_parent_class)->dispose (object);
}

static void
bz_inspector_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, bz_state_info_get_default ());
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
serialize_all_entries_cb (BzInspector *self,
                          GtkButton   *button)
{
  dex_future_disown (
      dex_scheduler_spawn (
          dex_scheduler_get_default (),
          bz_get_dex_stack_size (),
          (DexFiberFunc) serialize_all_entries_fiber,
          g_object_ref (self), g_object_unref));
}

static void
preview_changed (BzInspector    *self,
                 GParamSpec     *pspec,
                 GtkCheckButton *button)
{
  if (gtk_check_button_get_active (button))
    {
      BzWindow     *window   = NULL;
      BzEntryGroup *selected = NULL;

      g_assert (self->preview_window == NULL);

      window = bz_window_new (bz_state_info_get_default ());
      gtk_window_set_default_size (GTK_WINDOW (window), 750, 750);
      gtk_window_present (GTK_WINDOW (window));

      selected = gtk_single_selection_get_selected_item (self->groups_selection);
      if (selected != NULL)
        bz_window_show_group (window, selected);

      self->preview_window = (GtkWindow *) g_object_ref_sink (window);
    }
  else
    {
      if (self->preview_window != NULL)
        gtk_window_close (self->preview_window);
      g_clear_object (&self->preview_window);
    }
}

static void
selected_group_changed (BzInspector        *self,
                        GParamSpec         *pspec,
                        GtkSingleSelection *selection)
{
  BzEntryGroup *group = NULL;

  if (self->preview_window == NULL ||
      !gtk_widget_get_mapped (GTK_WIDGET (self->preview_window)))
    return;

  group = gtk_single_selection_get_selected_item (self->groups_selection);
  if (group != NULL)
    bz_window_show_group (BZ_WINDOW (self->preview_window), group);
}

static void
entry_changed (BzInspector *self,
               GtkEditable *editable)
{
  GtkFilter *filter = NULL;

  filter = gtk_filter_list_model_get_filter (self->filter_model);
  gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static void
decache_and_inspect_cb (GtkListItem *list_item,
                        GtkButton   *button)
{
  GtkStringObject *item       = NULL;
  BzInspector     *self       = NULL;
  g_autoptr (BzResult) result = NULL;

  item = gtk_list_item_get_item (list_item);

  self = BZ_INSPECTOR (gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_INSPECTOR));
  g_assert (self != NULL);

  result = bz_application_map_factory_convert_one (
      bz_state_info_get_entry_factory (bz_state_info_get_default ()),
      g_object_ref (item));
  if (result != NULL)
    {
      BzEntryInspector *inspector = NULL;

      inspector = bz_entry_inspector_new ();
      bz_entry_inspector_set_result (inspector, result);

      gtk_window_present (GTK_WINDOW (inspector));
    }
}

static void
copy_unique_id_cb (GtkListItem *list_item,
                   GtkButton   *button)
{
  GtkStringObject *item      = NULL;
  const char      *text      = NULL;
  GdkClipboard    *clipboard = NULL;

  item = gtk_list_item_get_item (list_item);
  text = gtk_string_object_get_string (item);

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  gdk_clipboard_set_text (clipboard, text);
}

static void
open_file_externally_cb (GtkListItem *list_item,
                         GtkButton   *button)
{
  GtkStringObject *string = NULL;
  const char      *path   = NULL;

  string = gtk_list_item_get_item (list_item);
  path   = gtk_string_object_get_string (string);

#if defined(DEVELOPMENT_BUILD) && defined(SANDBOXED_LIBFLATPAK)
  {
    const char *argv[] = { "flatpak-spawn", "--host", "xdg-open", NULL, NULL };

    argv[3] = path;
    g_spawn_async (NULL, (char **) argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
  }
#else
  {
    g_autofree char *uri                = NULL;
    g_autoptr (GtkUriLauncher) launcher = NULL;

    uri      = g_strdup_printf ("file://%s", path);
    launcher = gtk_uri_launcher_new (uri);

    gtk_uri_launcher_launch (launcher, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (button))), NULL, NULL, NULL);
  }
#endif
}

static void
bz_inspector_class_init (BzInspectorClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = bz_inspector_get_property;
  object_class->dispose      = bz_inspector_dispose;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-inspector.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);

  gtk_widget_class_bind_template_child (widget_class, BzInspector, debug_mode_check);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, disable_blocklists_check);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, serialize_all_entries_path_entry);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, serialize_all_entries_open_externally);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, serialize_all_entries_open_externally_prefer_terminal);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, serialize_all_entries_open_externally_alt_program_entry);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, serialize_all_entries_btn);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, serialize_all_entries_progress);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, search_entry);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, filter_model);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, groups_selection);
  gtk_widget_class_bind_template_callback (widget_class, serialize_all_entries_cb);
  gtk_widget_class_bind_template_callback (widget_class, preview_changed);
  gtk_widget_class_bind_template_callback (widget_class, selected_group_changed);
  gtk_widget_class_bind_template_callback (widget_class, decache_and_inspect_cb);
  gtk_widget_class_bind_template_callback (widget_class, copy_unique_id_cb);
  gtk_widget_class_bind_template_callback (widget_class, open_file_externally_cb);
  gtk_widget_class_bind_template_callback (widget_class, entry_changed);
}

static void
on_map (BzInspector *self,
        GtkWidget   *widget)
{
  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}

static void
bz_inspector_init (BzInspector *self)
{
  GtkCustomFilter *filter                       = NULL;
  g_autofree char *serialize_all_entries_output = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  filter = gtk_custom_filter_new ((GtkCustomFilterFunc) filter_func, self, NULL);
  gtk_filter_list_model_set_filter (self->filter_model, GTK_FILTER (filter));

  g_signal_connect_swapped (self, "map", G_CALLBACK (on_map), self);

  serialize_all_entries_output = g_build_filename (
      g_get_home_dir (),
      "BAZAAR_SERIALIZED_ENTRIES.TXT",
      NULL);
  gtk_editable_set_text (
      self->serialize_all_entries_path_entry,
      serialize_all_entries_output);

  self->debug_mode_binding = g_object_bind_property (
      bz_state_info_get_default (), "debug-mode",
      self->debug_mode_check, "active",
      G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  self->disable_blocklists_binding = g_object_bind_property (
      bz_state_info_get_default (), "disable-blocklists",
      self->disable_blocklists_check, "active",
      G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

BzInspector *
bz_inspector_new (void)
{
  return g_object_new (BZ_TYPE_INSPECTOR, NULL);
}

static DexFuture *
serialize_all_entries_fiber (BzInspector *self)
{
  g_autoptr (GError) local_error                     = NULL;
  g_autoptr (BzEntryCacheManager) cache              = NULL;
  const char      *path                              = NULL;
  g_autofree char *path_clone                        = NULL;
  gboolean         open_externally                   = FALSE;
  gboolean         open_externally_prefer_terminal   = FALSE;
  const char      *open_externally_alt_program       = NULL;
  g_autofree char *open_externally_alt_program_clone = NULL;
  g_autoptr (GFile) initial_file                     = NULL;
  g_autoptr (GtkFileDialog) file_dialog              = NULL;
  g_autoptr (GFile) file                             = NULL;
  g_autoptr (GHashTable) cached_set                  = NULL;
  guint cached_set_size                              = 0;
  g_autoptr (GFileOutputStream) output               = NULL;
  GHashTableIter iter                                = { 0 };

  cache = bz_state_info_get_cache_manager (bz_state_info_get_default ());
  if (cache == NULL)
    return dex_future_new_false ();
  g_object_ref (cache);

  path = gtk_editable_get_text (self->serialize_all_entries_path_entry);
  if (path != NULL && *path != '\0')
    {
      path_clone   = g_strdup (path);
      initial_file = g_file_new_for_path (path_clone);
    }

  open_externally                 = gtk_check_button_get_active (self->serialize_all_entries_open_externally);
  open_externally_prefer_terminal = gtk_check_button_get_active (self->serialize_all_entries_open_externally_prefer_terminal);

  open_externally_alt_program = gtk_editable_get_text (self->serialize_all_entries_open_externally_alt_program_entry);
  if (open_externally_alt_program != NULL && *open_externally_alt_program != '\0')
    open_externally_alt_program_clone = g_strdup (open_externally_alt_program);

  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_open_externally), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_open_externally_prefer_terminal), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_open_externally_alt_program_entry), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_btn), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->serialize_all_entries_progress), TRUE);
  gtk_progress_bar_set_fraction (self->serialize_all_entries_progress, 0.0);

  file_dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (file_dialog, "Choose a file to serialize json into");
  gtk_file_dialog_set_initial_file (file_dialog, initial_file);

  file = dex_await_object (
      bz_file_dialog_save (file_dialog, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)))),
      &local_error);
  if (file == NULL)
    goto err;

  cached_set = dex_await_boxed (
      bz_entry_cache_manager_enumerate_disk (
          bz_state_info_get_cache_manager (bz_state_info_get_default ())),
      &local_error);
  if (cached_set == NULL)
    goto err;

  cached_set_size = g_hash_table_size (cached_set);

  output = dex_await_object (
      dex_file_replace (
          file, NULL, TRUE,
          G_FILE_CREATE_REPLACE_DESTINATION,
          G_PRIORITY_DEFAULT),
      &local_error);
  if (output == NULL)
    goto err;

#define WRITE_STRING(_string)         \
  G_STMT_START                        \
  {                                   \
    dex_await (                       \
        dex_output_stream_write (     \
            G_OUTPUT_STREAM (output), \
            (_string),                \
            strlen ((_string)),       \
            G_PRIORITY_DEFAULT),      \
        &local_error);                \
    if (local_error != NULL)          \
      goto err;                       \
  }                                   \
  G_STMT_END

  WRITE_STRING ("[\n");

  g_hash_table_iter_init (&iter, cached_set);
  for (guint i = 1;; i++)
    {
      char *checksum                      = NULL;
      g_autoptr (BzEntry) entry           = NULL;
      g_autoptr (GVariantBuilder) builder = NULL;
      g_autoptr (GVariant) variant        = NULL;
      g_autoptr (JsonNode) node           = NULL;
      g_autoptr (JsonGenerator) generator = NULL;
      gsize            length             = 0;
      g_autofree char *string             = NULL;

      if (!g_hash_table_iter_next (
              &iter, (gpointer *) &checksum, NULL))
        break;

      if (i > 1)
        WRITE_STRING (",\n\n");

      entry = dex_await_object (
          bz_entry_cache_manager_get_by_checksum (
              cache, checksum),
          &local_error);
      if (entry == NULL)
        goto err;

      builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);
      bz_serializable_serialize (BZ_SERIALIZABLE (entry), builder);
      variant = g_variant_builder_end (builder);

      node = json_gvariant_serialize (variant);

      generator = json_generator_new ();
      json_generator_set_pretty (generator, TRUE);
      json_generator_set_root (generator, node);

      string = json_generator_to_data (generator, &length);
      dex_await (
          dex_output_stream_write (
              G_OUTPUT_STREAM (output),
              string,
              length,
              G_PRIORITY_DEFAULT),
          &local_error);
      if (local_error != NULL)
        goto err;

      gtk_progress_bar_set_fraction (
          self->serialize_all_entries_progress,
          (double) i / (double) cached_set_size);
    }

  WRITE_STRING ("\n]\n");

  dex_await (
      dex_output_stream_close (
          G_OUTPUT_STREAM (output),
          G_PRIORITY_DEFAULT),
      &local_error);
  if (local_error != NULL)
    goto err;

  if (open_externally)
    {
      if (open_externally_prefer_terminal)
        {
          g_autofree char *shell_snippet = NULL;

          const char *argv[] = {
#if defined(SANDBOXED_LIBFLATPAK)
            "flatpak-spawn",
            "--host",
#endif
            "/bin/sh",
            "-c",
            NULL,
            NULL,
          };

          shell_snippet = g_strdup_printf (
              "xdg-terminal-exec \"/bin/sh -c '%s %s'\" || "
              "ptyxis --new-window -x \"/bin/sh -c '%s %s'\" || "
              "xdg-open \"file://%s\"",
              open_externally_alt_program_clone != NULL ? open_externally_alt_program_clone : "$EDITOR",
              path_clone,
              open_externally_alt_program_clone != NULL ? open_externally_alt_program_clone : "$EDITOR",
              path_clone,
              path_clone);

#if defined(SANDBOXED_LIBFLATPAK)
          argv[4] = shell_snippet;
#else
          argv[2] = shell_snippet;
#endif

          g_spawn_async (NULL, (char **) argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
        }
      else
        {
#if defined(SANDBOXED_LIBFLATPAK)
          const char *argv[] = {
            "flatpak-spawn",
            "--host",
            "xdg-open",
            path_clone,
            NULL,
          };

          g_spawn_async (NULL, (char **) argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
#else
          g_autofree char *uri                = NULL;
          g_autoptr (GtkUriLauncher) launcher = NULL;

          uri      = g_strdup_printf ("file://%s", path_clone);
          launcher = gtk_uri_launcher_new (uri);

          gtk_uri_launcher_launch (launcher, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))), NULL, NULL, NULL);
#endif
        }
    }

  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_open_externally), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_open_externally_prefer_terminal), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_open_externally_alt_program_entry), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_btn), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->serialize_all_entries_progress), FALSE);
  return dex_future_new_true ();

err:
  if (local_error != NULL)
    g_warning ("Failed to serialize: %s", local_error->message);
  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_open_externally), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_open_externally_prefer_terminal), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_open_externally_alt_program_entry), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->serialize_all_entries_btn), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->serialize_all_entries_progress), FALSE);
  return dex_future_new_for_error (g_steal_pointer (&local_error));
}

static gboolean
filter_func (BzEntryGroup *group,
             BzInspector  *self)
{
  const char *text  = NULL;
  const char *id    = NULL;
  const char *title = NULL;

  text  = gtk_editable_get_text (self->search_entry);
  id    = bz_entry_group_get_id (group);
  title = bz_entry_group_get_title (group);

  if (text == NULL || *text == '\0')
    return TRUE;

  if (strcasestr (id, text) != NULL)
    return TRUE;
  if (strcasestr (title, text) != NULL)
    return TRUE;

  return FALSE;
}

struct _BzInspectorPage
{
  AdwBin parent_instance;
};

G_DEFINE_FINAL_TYPE (BzInspectorPage, bz_inspector_page, ADW_TYPE_BIN);

enum
{
  PAGE_PROP_0,

  PAGE_PROP_TITLE,
  PAGE_PROP_OBJECT,

  PAGE_LAST_PROP
};

static void
bz_inspector_page_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  if (prop_id == PAGE_PROP_TITLE)
    g_value_set_string (value, "Bazaar");
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
bz_inspector_page_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  if (prop_id != PAGE_PROP_OBJECT)
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
bz_inspector_page_map (GtkWidget *widget)
{
  if (adw_bin_get_child (ADW_BIN (widget)) == NULL)
    {
      g_autoptr (GtkCssProvider) provider = NULL;
      BzInspector *inspector              = NULL;

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/io/github/kolunmi/Bazaar/inspector.css");
      gtk_style_context_add_provider_for_display (
          gtk_widget_get_display (widget),
          GTK_STYLE_PROVIDER (provider),
          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      gtk_icon_theme_add_resource_path (
          gtk_icon_theme_get_for_display (gtk_widget_get_display (widget)),
          "/io/github/kolunmi/Bazaar/icons");

      inspector = bz_inspector_new ();
      adw_bin_set_child (ADW_BIN (widget), GTK_WIDGET (inspector));
    }

  GTK_WIDGET_CLASS (bz_inspector_page_parent_class)->map (widget);
}

static void
bz_inspector_page_class_init (BzInspectorPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_inspector_page_set_property;
  object_class->get_property = bz_inspector_page_get_property;
  widget_class->map          = bz_inspector_page_map;

  g_object_class_install_property (
      object_class, PAGE_PROP_TITLE,
      g_param_spec_string ("title", NULL, NULL, NULL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (
      object_class, PAGE_PROP_OBJECT,
      g_param_spec_object ("object", NULL, NULL, G_TYPE_OBJECT,
                           G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}

static void
bz_inspector_page_init (BzInspectorPage *self)
{
}

void
bz_inspector_page_register (void)
{
  if (g_io_extension_point_lookup ("gtk-inspector-page") != NULL)
    g_io_extension_point_implement ("gtk-inspector-page", BZ_TYPE_INSPECTOR_PAGE, "bazaar", 10);
}

/* End of bz-inspector.c */
