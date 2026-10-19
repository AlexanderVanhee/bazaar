/* bz-state-info.vala
 *
 * Copyright 2026 The Bazaar Contributors
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

public sealed class Bz.StateInfo : GLib.Object {
    [CCode (cname = "bz_state_info_get_default", cheader_filename = "bz-application.h")]
    public static extern unowned Bz.StateInfo get_default ();

    public GLib.ListModel? all_entries { get; set; }
    public GLib.ListModel? all_entry_groups { get; set; }
    public GLib.ListModel? filtered_entry_groups { get; set; }
    public GLib.ListModel? all_installed_entry_groups { get; set; }
    public GLib.ListModel? all_user_data_entry_groups { get; set; }
    public bool allow_manual_sync { get; set; }
    public Bz.ApplicationMapFactory? application_factory { get; set; }
    public Bz.AuthState? auth_state { get; set; }
    public GLib.ListModel? available_updates { get; set; }
    public Bz.Backend? backend { get; set; }
    public string? background_task_label { get; set; }
    public GLib.ListModel? blocklists { get; set; }
    public Bz.ContentProvider? blocklists_provider { get; set; }
    public bool busy { get; set; }
    public double busy_progress { get; set; }
    public string? busy_progress_label { get; set; }
    public string? busy_step_label { get; set; }
    public Bz.EntryCacheManager? cache_manager { get; set; }
    public bool checking_for_updates { get; set; }
    public GLib.ListModel? curated_configs { get; set; }
    public Bz.ContentProvider? curated_provider { get; set; }
    public bool debug_mode { get; set; }
    public bool disable_blocklists { get; set; }
    public bool donation_prompt_dismissed { get; set; }
    public Bz.ApplicationMapFactory? entry_factory { get; set; }
    public Bz.FlathubState? flathub { get; set; }
    public bool has_flathub { get; set; }
    public bool have_connection { get; set; }
    public bool hide_eol { get; set; }
    public Bz.InternalConfig? internal_config { get; set; }
    public Bz.MainConfig? main_config { get; set; }
    public bool metered_connection { get; set; }
    public bool recently_synced { get; set; }
    public bool online { get; set; }
    public GLib.ListModel? repositories { get; set; }
    public Bz.SearchEngine? search_engine { get; set; }
    public GLib.Settings? settings { get; set; }
    public bool show_only_flathub { get; set; }
    public bool show_only_foss { get; set; }
    public bool show_only_verified { get; set; }
    public bool syncing { get; set; }
    public Gtk.IconTheme? system_icon_theme { get; set; }
    public Bz.TransactionManager? transaction_manager { get; set; }
    public GLib.ListModel? txt_blocklists { get; set; }
    public Bz.ContentProvider? txt_blocklists_provider { get; set; }
    public Gtk.IconTheme? user_icon_theme { get; set; }
    public GLib.ListModel? parental_blocked_ids { get; set; }
    public int parental_age_rating { get; set; }
}
