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
    public Bz.Repository repository { get; private set; }
    public GLib.GenericArray<string>? apps { get; private set; }

    public signal void remove_requested ();

    public RemoteRow (Bz.Repository repository, GLib.GenericArray<string>? apps, bool removable) {
        this.repository = repository;
        this.apps = apps;

        var remove_label = _("Remove %s").printf (repository.title);

        title = GLib.Markup.escape_text (repository.title);
        MatchInfo match_info;
        var domain = repository.url ?? "";
        if (/^(?:[a-zA-Z][a-zA-Z0-9+.-]*:\/\/)?([^?#]+?)(?:\/repo)?\/?$/.match (domain, 0, out match_info))
            domain = match_info.fetch (1);
        subtitle = GLib.Markup.escape_text (domain);
        subtitle_lines = 1;

        var count_label = new Gtk.Label (
            apps == null ? _("Unused") : ngettext ("%u app installed", "%u apps installed", apps.length).printf (apps.length)
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
    private const int CONFIRM_MIN_HEIGHT = 300;
    private const int CONFIRM_MAX_HEIGHT = 700;

    [GtkChild]
    private unowned Adw.NavigationView navigation_view;
    [GtkChild]
    private unowned Adw.PreferencesGroup system_group;
    [GtkChild]
    private unowned Adw.PreferencesGroup user_group;
    [GtkChild]
    private unowned Gtk.Label confirm_title;
    [GtkChild]
    private unowned Gtk.Label confirm_body;
    [GtkChild]
    private unowned Adw.PreferencesGroup apps_group;
    [GtkChild]
    private unowned Gtk.Box apps_box;

    private Bz.Repository? confirm_repo = null;

    public RemotesDialog () {
        map.connect (() => update_size (true));
        navigation_view.notify["visible-page-tag"].connect (() => queue_update_size (true));

        var apps_by_remote = new GLib.HashTable<string, GLib.GenericArray<string>> (str_hash, str_equal);
        var installed_set = Bz.StateInfo.get_default ().installed_set;

        foreach (unowned string unique_id in installed_set.get_keys ()) {
            var key = Bz.flatpak_unique_id_dup_remote_key (unique_id);
            if (key == null || !unique_id.has_prefix (key + "app/"))
                continue;

            var list = apps_by_remote.lookup (key);
            if (list == null) {
                list = new GLib.GenericArray<string> ();
                apps_by_remote.insert (key, list);
            }
            list.add (unique_id);
        }

        var repositories = Bz.StateInfo.get_default ().repositories;
        uint n_repos = repositories != null ? repositories.get_n_items () : 0;

        foreach (bool flathub_pass in new bool[] { true, false }) {
            for (uint i = 0; i < n_repos; i++) {
                var repo = (Bz.Repository) repositories.get_item (i);
                if (repo.noenumerate || (repo.name == "flathub") != flathub_pass)
                    continue;

                var group = repo.is_user ? user_group : system_group;
                group.add (create_row (repo, apps_by_remote.lookup (Bz.flatpak_remote_key (repo.name, repo.is_user))));
                group.visible = true;
            }
        }
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

    private Gtk.Widget create_row (Bz.Repository repo, GLib.GenericArray<string>? apps) {
        var state = Bz.StateInfo.get_default ();
        bool removable = (repo.is_user || repo.name != "flathub") && !state.busy && !state.syncing;

        var row = new Bz.RemoteRow (repo, apps, removable);
        row.remove_requested.connect (on_remove_requested);
        return row;
    }

    private void on_remove_requested (Bz.RemoteRow row) {
        if (row.apps != null)
            show_confirm (row.repository, row.apps);
        else
            present_confirm_alert (row.repository);
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

        var factory = Bz.StateInfo.get_default ().application_factory;
        foreach (var unique_id in apps) {
            var app_id = new Gtk.StringObject (unique_id.split ("/")[1]);
            apps_box.append (new Bz.RemoteAppTile ((Bz.EntryGroup) factory.convert_one (app_id)));
        }

        navigation_view.push_by_tag ("confirm");
    }

    [GtkCallback]
    private void on_breakpoint_changed () {
        queue_update_size (false);
    }

    private void queue_update_size (bool animated) {
        Idle.add_full (Priority.DEFAULT_IDLE, () => {
            update_size (animated);
            return Source.REMOVE;
        });
    }

    private void update_size (bool animated) {
        int width = get_width () > 0 ? get_width () : content_width;
        bool is_list = navigation_view.visible_page_tag == "list";
        int target_height = Bz.Dialog.measure_height (navigation_view.visible_page, width, 0,
                                                      is_list ? LIST_MIN_HEIGHT : CONFIRM_MIN_HEIGHT,
                                                      is_list ? LIST_MAX_HEIGHT : CONFIRM_MAX_HEIGHT);

        if (animated)
            resize_to (content_width, target_height);
        else
            resize_to_instant (content_width, target_height);
    }

    [GtkCallback]
    private void on_confirm_clicked () {
        remove_remote (confirm_repo);
    }

    private void remove_remote (Bz.Repository repo) {
        close ();
        GLib.Application.get_default ().activate_action ("remove-repository",
                                                         new Variant ("(sb)", repo.name, repo.is_user));
    }
}
