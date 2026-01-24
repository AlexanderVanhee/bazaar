/* bz-preferences-dialog.c
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

#include "bz-preferences-dialog.h"
#include <glib/gi18n.h>

typedef struct
{
  const char *id;
  const char *style_class;
  const char *tooltip;
} BarTheme;

typedef struct
{
  const char *id;
  const char *display_name;
} InstallScope;

static const BarTheme bar_themes[] = {
  {       "accent-color",       "accent-color-theme",                 N_ ("Accent Color") },
  { "pride-rainbow-flag", "pride-rainbow-flag-theme",                 N_ ("Pride Colors") },
  { "lesbian-pride-flag", "lesbian-pride-flag-theme",         N_ ("Lesbian Pride Colors") },
  {     "gay-pride-flag",     "gay-pride-flag-theme", N_ ("Male Homosexual Pride Colors") },
  {   "transgender-flag",   "transgender-flag-theme",     N_ ("Transgender Pride Colors") },
  {     "nonbinary-flag",     "nonbinary-flag-theme",       N_ ("Nonbinary Pride Colors") },
  {      "bisexual-flag",      "bisexual-flag-theme",        N_ ("Bisexual Pride Colors") },
  {       "asexual-flag",       "asexual-flag-theme",         N_ ("Asexual Pride Colors") },
  {     "pansexual-flag",     "pansexual-flag-theme",       N_ ("Pansexual Pride Colors") },
  {     "aromantic-flag",     "aromantic-flag-theme",       N_ ("Aromantic Pride Colors") },
  {   "genderfluid-flag",   "genderfluid-flag-theme",     N_ ("Genderfluid Pride Colors") },
  {    "polysexual-flag",    "polysexual-flag-theme",      N_ ("Polysexual Pride Colors") },
  {    "omnisexual-flag",    "omnisexual-flag-theme",      N_ ("Omnisexual Pride Colors") },
  {        "aroace-flag",        "aroace-flag-theme",          N_ ("Aroace Pride Colors") },
  {       "agender-flag",       "agender-flag-theme",         N_ ("Agender Pride Colors") },
  {   "genderqueer-flag",   "genderqueer-flag-theme",     N_ ("Genderqueer Pride Colors") },
  {      "intersex-flag",      "intersex-flag-theme",        N_ ("Intersex Pride Colors") },
  {    "demigender-flag",    "demigender-flag-theme",      N_ ("Demigender Pride Colors") },
  {    "biromantic-flag",    "biromantic-flag-theme",      N_ ("Biromantic Pride Colors") },
  {    "disability-flag",    "disability-flag-theme",      N_ ("Disability Pride Colors") },
  {        "femboy-flag",        "femboy-flag-theme",          N_ ("Femboy Pride Colors") },
  {      "neutrois-flag",      "neutrois-flag-theme",        N_ ("Neutrois Pride Colors") },
};

static const InstallScope install_scopes[] = {
  {   "user",    N_ ("User") },
  { "system", N_ ("System ") },
  {   "both",    N_ ("Both") },
};

struct _BzPreferencesDialog
{
  AdwPreferencesDialog parent_instance;

  BzStateInfo *state;
  GSettings   *settings;

  /* Template widgets */
  AdwComboRow  *scope_dropdown;
  AdwSwitchRow *only_foss_switch;
  AdwSwitchRow *only_flathub_switch;
  AdwSwitchRow *only_verified_switch;
  AdwSwitchRow *search_debounce_switch;
  GtkFlowBox   *flag_buttons_box;
  AdwSwitchRow *hide_eol_switch;
  AdwSwitchRow *rotate_switch;

  GtkToggleButton *flag_buttons[G_N_ELEMENTS (bar_themes)];
  gboolean         scope_dropdown_visible;
};

G_DEFINE_FINAL_TYPE (BzPreferencesDialog, bz_preferences_dialog, ADW_TYPE_PREFERENCES_DIALOG)

enum
{
  PROP_0,

  PROP_STATE,
  PROP_SCOPE_DROPDOWN_VISIBLE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void     bind_settings (BzPreferencesDialog *self);
static void     create_flag_buttons (BzPreferencesDialog *self);
static void     setup_scope_dropdown (BzPreferencesDialog *self);
static gboolean check_duplicate_repositories (BzPreferencesDialog *self);

static void
bz_preferences_dialog_dispose (GObject *object)
{
  BzPreferencesDialog *self = BZ_PREFERENCES_DIALOG (object);

  g_clear_object (&self->state);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (bz_preferences_dialog_parent_class)->dispose (object);
}

static void
flag_button_toggled (GtkToggleButton     *button,
                     BzPreferencesDialog *self)
{
  const char *theme_id = NULL;

  if (!gtk_toggle_button_get_active (button))
    return;

  theme_id = g_object_get_data (G_OBJECT (button), "theme-id");
  if (theme_id != NULL)
    {
      g_settings_set_string (self->settings, "global-progress-bar-theme", theme_id);
    }
}

static void
scope_dropdown_selected_changed (AdwComboRow         *row,
                                 GParamSpec          *pspec,
                                 BzPreferencesDialog *self)
{
  guint selected = 0;
  selected       = adw_combo_row_get_selected (row);

  if (selected < G_N_ELEMENTS (install_scopes))
    g_settings_set_string (self->settings, "scope", install_scopes[selected].id);
}

static void
install_mode_settings_changed (BzPreferencesDialog *self,
                               const char          *key,
                               GSettings           *settings)
{
  g_autofree char *mode = NULL;
  mode                  = g_settings_get_string (self->settings, "scope");

  for (guint i = 0; i < G_N_ELEMENTS (install_scopes); i++)
    {
      if (g_strcmp0 (mode, install_scopes[i].id) == 0)
        {
          adw_combo_row_set_selected (self->scope_dropdown, i);
          break;
        }
    }
}

static void
global_progress_theme_settings_changed (BzPreferencesDialog *self,
                                        const char          *key,
                                        GSettings           *settings)
{
  const char *theme = NULL;

  theme = g_settings_get_string (self->settings, "global-progress-bar-theme");

  for (guint i = 0; i < G_N_ELEMENTS (bar_themes); i++)
    {
      if (g_strcmp0 (theme, bar_themes[i].id) == 0)
        {
          gtk_toggle_button_set_active (self->flag_buttons[i], TRUE);
          break;
        }
    }
}

static void
on_rotate_switch_changed (AdwSwitchRow        *row,
                          GParamSpec          *pspec,
                          BzPreferencesDialog *self)
{
  gboolean active = FALSE;
  active          = adw_switch_row_get_active (row);
  for (guint i = 0; i < G_N_ELEMENTS (bar_themes); i++)
    {
      if (active)
        gtk_widget_add_css_class (GTK_WIDGET (self->flag_buttons[i]), "horizontal");
      else
        gtk_widget_remove_css_class (GTK_WIDGET (self->flag_buttons[i]), "horizontal");
    }
}

static void
create_flag_buttons (BzPreferencesDialog *self)
{
  GtkToggleButton *first_button = NULL;

  for (guint i = 0; i < G_N_ELEMENTS (bar_themes); i++)
    {
      GtkToggleButton *button = NULL;

      button = GTK_TOGGLE_BUTTON (gtk_toggle_button_new ());

      gtk_widget_set_tooltip_text (GTK_WIDGET (button), Q_ (bar_themes[i].tooltip));
      gtk_widget_add_css_class (GTK_WIDGET (button), "accent-button");
      gtk_widget_add_css_class (GTK_WIDGET (button), bar_themes[i].style_class);

      g_object_set_data_full (G_OBJECT (button),
                              "theme-id",
                              g_strdup (bar_themes[i].id),
                              g_free);

      if (i == 0)
        {
          first_button = button;
        }
      else
        {
          gtk_toggle_button_set_group (button, first_button);
        }

      g_signal_connect (button, "toggled",
                        G_CALLBACK (flag_button_toggled), self);

      self->flag_buttons[i] = button;
      gtk_flow_box_append (self->flag_buttons_box, GTK_WIDGET (button));
    }
}

static void
setup_scope_dropdown (BzPreferencesDialog *self)
{
  GtkStringList *model = gtk_string_list_new (NULL);

  for (guint i = 0; i < G_N_ELEMENTS (install_scopes); i++)
    gtk_string_list_append (model, install_scopes[i].display_name);

  adw_combo_row_set_model (self->scope_dropdown, G_LIST_MODEL (model));
  g_signal_connect (self->scope_dropdown, "notify::selected",
                    G_CALLBACK (scope_dropdown_selected_changed), self);
}

static gboolean
check_duplicate_repositories (BzPreferencesDialog *self)
{
  GListModel *repositories    = NULL;
  guint       n_items         = 0;
  g_autoptr (GHashTable) seen = NULL;

  repositories = bz_state_info_get_repositories (self->state);
  n_items      = g_list_model_get_n_items (repositories);
  seen         = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzRepository) repository = g_list_model_get_item (repositories, i);
      g_autofree char *name               = NULL;

      g_object_get (repository, "name", &name, NULL);

      if (!g_hash_table_add (seen, g_strdup (name)))
        return TRUE;
    }

  return FALSE;
}

static void
bind_settings (BzPreferencesDialog *self)
{
  if (self->settings == NULL)
    return;

  /* Bind all boolean settings to their respective switches */
  g_settings_bind (self->settings, "show-only-foss",
                   self->only_foss_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings, "show-only-flathub",
                   self->only_flathub_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings, "show-only-verified",
                   self->only_verified_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings, "search-debounce",
                   self->search_debounce_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings, "hide-eol",
                   self->hide_eol_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings, "rotate-flag",
                   self->rotate_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  if (adw_switch_row_get_active (self->rotate_switch))
    {
      for (guint i = 0; i < G_N_ELEMENTS (bar_themes); i++)
        {
          gtk_widget_add_css_class (GTK_WIDGET (self->flag_buttons[i]), "horizontal");
        }
    }

  setup_scope_dropdown (self);
  g_signal_connect_object (
      self->settings,
      "changed::scope",
      G_CALLBACK (install_mode_settings_changed),
      self, G_CONNECT_SWAPPED);
  install_mode_settings_changed (self, "scope", self->settings);

  g_signal_connect_object (
      self->settings,
      "changed::global-progress-bar-theme",
      G_CALLBACK (global_progress_theme_settings_changed),
      self, G_CONNECT_SWAPPED);
  global_progress_theme_settings_changed (self, "global-progress-bar-theme", self->settings);
}

static void
bz_preferences_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  BzPreferencesDialog *self = BZ_PREFERENCES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, self->state);
      break;
    case PROP_SCOPE_DROPDOWN_VISIBLE:
      g_value_set_boolean (value, self->scope_dropdown_visible);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_preferences_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_STATE:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static void
bz_preferences_dialog_class_init (BzPreferencesDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_preferences_dialog_set_property;
  object_class->get_property = bz_preferences_dialog_get_property;
  object_class->dispose      = bz_preferences_dialog_dispose;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_SCOPE_DROPDOWN_VISIBLE] =
      g_param_spec_boolean (
          "scope-dropdown-visible",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-preferences-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, scope_dropdown);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, only_foss_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, only_flathub_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, only_verified_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, search_debounce_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, flag_buttons_box);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, hide_eol_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, rotate_switch);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, on_rotate_switch_changed);
}

static void
bz_preferences_dialog_init (BzPreferencesDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  create_flag_buttons (self);
  self->scope_dropdown_visible = FALSE;
}

AdwDialog *
bz_preferences_dialog_new (BzStateInfo *state)
{
  BzPreferencesDialog *dialog = NULL;

  g_return_val_if_fail (BZ_IS_STATE_INFO (state), NULL);

  dialog        = g_object_new (BZ_TYPE_PREFERENCES_DIALOG, NULL);
  dialog->state = g_object_ref (state);
  g_object_get (state, "settings", &dialog->settings, NULL);

  dialog->scope_dropdown_visible = check_duplicate_repositories (dialog);
  g_object_notify_by_pspec (G_OBJECT (dialog), props[PROP_SCOPE_DROPDOWN_VISIBLE]);

  bind_settings (dialog);

  return ADW_DIALOG (dialog);
}
