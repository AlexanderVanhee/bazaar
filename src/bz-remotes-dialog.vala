/* bz-remotes-dialog.vala
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

public class Bz.RemoteRow : Adw.ActionRow {
    public signal void remove_requested ();

    public RemoteRow (Bz.Repository repository, uint n_apps, bool removable) {
        var remove_label = _("Remove %s").printf (repository.title);

        title = GLib.Markup.escape_text (repository.title);
        MatchInfo match_info;
        var domain = repository.url ?? "";
        if (/^(?:[a-zA-Z][a-zA-Z0-9+.-]*:\/\/)?([^?#]+?)(?:\/repo)?\/?$/.match (domain, 0, out match_info))
            domain = match_info.fetch (1);
        subtitle = GLib.Markup.escape_text (domain);
        subtitle_lines = 1;

        var count_label = new Gtk.Label (
            n_apps == 0 ? _("Unused") : ngettext ("%u app installed", "%u apps installed", n_apps).printf (n_apps)
        ) {
            valign = Gtk.Align.CENTER,
        };
        count_label.add_css_class ("numeric");
        count_label.add_css_class ("dim-label");

        var remove_button = new Gtk.Button.from_icon_name ("user-trash-symbolic") {
            valign = Gtk.Align.CENTER,
            tooltip_text = remove_label,
            sensitive = removable,
        };
        remove_button.add_css_class ("flat");
        remove_button.update_property (Gtk.AccessibleProperty.LABEL, remove_label);
        remove_button.clicked.connect (() => remove_requested ());

        var suffix_box = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 12) {
            valign = Gtk.Align.CENTER,
        };
        suffix_box.append (count_label);
        suffix_box.append (remove_button);
        add_suffix (suffix_box);

        activatable = removable;
        activated.connect (() => remove_requested ());
    }
}

[GtkTemplate (ui = "/io/github/kolunmi/Bazaar/bz-remote-app-tile.ui")]
public class Bz.RemoteAppTile : Gtk.Box {
    public Bz.EntryGroup group { get; construct; }

    [GtkChild]
    private unowned Gtk.Label subtitle_label;

    public RemoteAppTile (Bz.EntryGroup group) {
        Object (group: group);

        subtitle_label.label = GLib.format_size (group.installed_size);
        subtitle_label.visible = group.installed_size > 0;
    }
}

[GtkTemplate (ui = "/io/github/kolunmi/Bazaar/bz-remotes-dialog.ui")]
public class Bz.RemotesDialog : Bz.Dialog {
    private const int LIST_MIN_HEIGHT = 300;
    private const int LIST_MAX_HEIGHT = 600;
    private const int EMPTY_HEIGHT = 400;
    private const int CONFIRM_MIN_HEIGHT = 300;
    private const int CONFIRM_MAX_HEIGHT = 700;

    [GtkChild]
    private unowned Adw.NavigationView navigation_view;
    [GtkChild]
    private unowned Adw.HeaderBar list_header_bar;
    [GtkChild]
    private unowned Gtk.Stack list_stack;
    [GtkChild]
    private unowned Adw.PreferencesGroup system_group;
    [GtkChild]
    private unowned Adw.PreferencesGroup user_group;
    [GtkChild]
    private unowned Adw.NavigationPage confirm_page;
    [GtkChild]
    private unowned Gtk.Label confirm_title;
    [GtkChild]
    private unowned Gtk.Label confirm_body;
    [GtkChild]
    private unowned Adw.PreferencesGroup apps_group;
    [GtkChild]
    private unowned Gtk.Box apps_box;

    private GLib.HashTable<string, GLib.GenericArray<string>> apps_by_remote =
        new GLib.HashTable<string, GLib.GenericArray<string>> (str_hash, str_equal);
    private Bz.Repository? confirm_repo = null;

    public RemotesDialog () {
        map.connect (animate_to_size);
        navigation_view.notify["visible-page-tag"].connect (() => {
            Idle.add_full (Priority.DEFAULT_IDLE, () => {
                animate_to_size ();
                return Source.REMOVE;
            });
        });

        var installed_set = Bz.StateInfo.get_default ().installed_set;

        installed_set.foreach ((unique_id, version) => {
            var key = Bz.flatpak_unique_id_dup_remote_key (unique_id);
            if (key == null || !unique_id.has_prefix (key + "app/"))
                return;

            var list = apps_by_remote.lookup (key);
            if (list == null) {
                list = new GLib.GenericArray<string> ();
                apps_by_remote.insert (key, list);
            }
            list.add (unique_id);
        });

        var repositories = Bz.StateInfo.get_default ().repositories;
        uint n_repos = repositories != null ? repositories.get_n_items () : 0;

        system_group.visible = false;
        user_group.visible = false;

        foreach (bool flathub_pass in new bool[] { true, false }) {
            for (uint i = 0; i < n_repos; i++) {
                var repo = (Bz.Repository) repositories.get_item (i);
                if (repo.noenumerate || (repo.name == "flathub") != flathub_pass)
                    continue;

                var group = repo.is_user ? user_group : system_group;
                group.add (create_row (repo));
                group.visible = true;
            }
        }

        list_stack.visible_child_name = system_group.visible || user_group.visible ? "list" : "empty";
    }

    public static bool should_show (GLib.ListModel? repositories) {
        if (repositories == null)
            return false;

        uint n_shown = 0;
        for (uint i = 0; i < repositories.get_n_items (); i++) {
            if (!((Bz.Repository) repositories.get_item (i)).noenumerate)
                n_shown++;
        }
        return n_shown > 1;
    }

    private Gtk.Widget create_row (Bz.Repository repo) {
        var state = Bz.StateInfo.get_default ();
        var apps = apps_by_remote.lookup (Bz.flatpak_remote_key (repo.name, repo.is_user));
        bool removable = (repo.is_user || repo.name != "flathub") && !state.busy && !state.syncing;

        var row = new Bz.RemoteRow (repo, apps != null ? apps.length : 0, removable);
        row.remove_requested.connect (() => {
            if (apps != null && apps.length > 0)
                show_confirm (repo, apps);
            else
                present_confirm_alert (repo);
        });
        return row;
    }

    private void present_confirm_alert (Bz.Repository repo) {
        var alert = new Adw.AlertDialog (
            _("Remove “%s”?").printf (repo.title),
            repo.is_user
                ? _("You will no longer be able to install apps from this user remote.")
                : _("You will no longer be able to install apps from this system remote.")
        );
        alert.add_response ("cancel", _("_Cancel"));
        alert.add_response ("remove", _("_Remove"));
        alert.set_response_appearance ("remove", Adw.ResponseAppearance.DESTRUCTIVE);
        alert.default_response = "cancel";
        alert.close_response = "cancel";
        alert.response.connect ((response) => {
            if (response == "remove")
                remove_remote (repo);
        });
        alert.present (this);
    }

    private void show_confirm (Bz.Repository repo, GLib.GenericArray<string> apps) {
        confirm_repo = repo;
        confirm_title.label = _("Remove “%s”?").printf (repo.title);
        apps_group.title = ngettext ("%u App to Remove", "%u Apps to Remove", apps.length).printf (apps.length);

        if (repo.is_user) {
            confirm_body.label = ngettext ("The app below was installed from this remote and will be removed from your account along with it.",
                                           "These apps were installed from this remote and will be removed from your account along with it.",
                                           apps.length);
        } else {
            confirm_body.label = ngettext ("The app below was installed from this remote and will be removed for all users along with it.",
                                           "These apps were installed from this remote and will be removed for all users along with it.",
                                           apps.length);
        }

        Gtk.Widget? child;
        while ((child = apps_box.get_first_child ()) != null)
            apps_box.remove (child);

        var groups = new GLib.HashTable<string, Bz.EntryGroup> (str_hash, str_equal);
        var installed_groups = Bz.StateInfo.get_default ().all_installed_entry_groups;
        for (uint i = 0; i < installed_groups.get_n_items (); i++) {
            var group = (Bz.EntryGroup) installed_groups.get_item (i);
            groups.insert (group.id, group);
        }

        foreach (var unique_id in apps) {
            var app_id = unique_id.split ("/")[1];
            apps_box.append (new Bz.RemoteAppTile (groups.lookup (app_id)));
        }

        navigation_view.push_by_tag ("confirm");
    }

    private void animate_to_size () {
        int target_height;

        switch (navigation_view.visible_page_tag) {
        case "list":
            if (list_stack.visible_child_name == "empty") {
                target_height = EMPTY_HEIGHT;
                break;
            }
            int header_nat;
            list_header_bar.measure (Gtk.Orientation.VERTICAL, content_width, null, out header_nat, null, null);
            target_height = Bz.Dialog.measure_height (system_group.get_parent (), content_width, header_nat, LIST_MIN_HEIGHT, LIST_MAX_HEIGHT);
            break;
        case "confirm":
            target_height = Bz.Dialog.measure_height (confirm_page, content_width, 0, CONFIRM_MIN_HEIGHT, CONFIRM_MAX_HEIGHT);
            break;
        default:
            return;
        }

        resize_to (content_width, target_height);
    }

    [GtkCallback]
    private void on_confirm_clicked () {
        if (confirm_repo != null)
            remove_remote (confirm_repo);
    }

    private void remove_remote (Bz.Repository repo) {
        close ();
        GLib.Application.get_default ().activate_action ("remove-repository",
                                                         new Variant ("(sb)", repo.name, repo.is_user));
    }
}
