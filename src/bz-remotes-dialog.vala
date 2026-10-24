/*
 * bz-remotes-dialog.vala
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

private string remote_title (Bz.Repository repo) {
    if (repo.title != null && repo.title != "")
        return repo.title;
    return repo.name ?? "";
}

private string remote_domain (string? url) {
    MatchInfo match_info;

    if (url == null)
        return "";

    var regex = /^(?:[a-zA-Z][a-zA-Z0-9+.-]*:\/\/)?([^?#]+?)\/?$/;
    if (regex.match (url, 0, out match_info))
        return match_info.fetch (1);

    return url;
}

private string apps_label (uint n) {
    return ngettext ("%u app", "%u apps", n).printf (n);
}

public class Bz.RemoteRow : Adw.ActionRow {
    public Bz.Repository repository { get; construct; }
    public uint n_apps { get; construct; }

    public signal void remove_requested ();

    public RemoteRow (Bz.Repository repository, uint n_apps, bool removable) {
        Object (repository: repository, n_apps: n_apps);

        title = GLib.Markup.escape_text (remote_title (repository));
        subtitle = GLib.Markup.escape_text (remote_domain (repository.url));
        subtitle_lines = 1;

        var count_label = new Gtk.Label (
            n_apps == 0 ? _("Unused") : _("%s installed").printf (apps_label (n_apps))
        ) {
            valign = Gtk.Align.CENTER,
        };
        count_label.add_css_class ("numeric");
        count_label.add_css_class ("dim-label");
        if (n_apps == 0)
            count_label.add_css_class ("remote-unused");

        var remove_button = new Gtk.Button.from_icon_name ("user-trash-symbolic") {
            valign = Gtk.Align.CENTER,
            tooltip_text = _("Remove %s").printf (remote_title (repository)),
            sensitive = removable,
        };
        remove_button.add_css_class ("flat");
        remove_button.update_property (Gtk.AccessibleProperty.LABEL,
                                       _("Remove %s").printf (remote_title (repository)));
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

[GtkTemplate (ui = "/io/github/kolunmi/Bazaar/bz-remotes-dialog.ui")]
public class Bz.RemotesDialog : Adw.Dialog {
    [GtkChild]
    private unowned Adw.NavigationView navigation_view;
    [GtkChild]
    private unowned Gtk.Stack list_stack;
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

    private GLib.HashTable<string, GLib.GenericArray<string>> apps_by_remote =
        new GLib.HashTable<string, GLib.GenericArray<string>> (str_hash, str_equal);
    private Bz.Repository? confirm_repo = null;
    private uint generation = 0;

    public RemotesDialog () {
        Object ();

        var state = Bz.StateInfo.get_default ();
        ulong repos_handler = state.notify["repositories"].connect (() => reload ());
        ulong busy_handler = state.notify["busy"].connect (() => rebuild ());
        ulong syncing_handler = state.notify["syncing"].connect (() => rebuild ());
        closed.connect (() => {
            state.disconnect (repos_handler);
            state.disconnect (busy_handler);
            state.disconnect (syncing_handler);
        });

        reload ();
    }

    private void reload () {
        uint this_generation = ++generation;
        var weak_self = GLib.WeakRef (this);

        var fiber = Dex.Scheduler.get_default ().spawn (0, () => {
            return collect_apps (weak_self, this_generation);
        });
        fiber.disown ();
    }

    private static Dex.Future? collect_apps (GLib.WeakRef weak_self, uint this_generation) {
        var by_remote = new GLib.HashTable<string, GLib.GenericArray<string>> (str_hash, str_equal);

        try {
            var installed = Bz.await_string_table (Bz.StateInfo.get_default ().backend.retrieve_install_ids (null));
            installed.foreach ((unique_id, version) => {
                var key = Bz.flatpak_unique_id_dup_remote_key (unique_id);
                if (key == null || !unique_id.has_prefix (key + "app/"))
                    return;

                var list = by_remote.lookup (key);
                if (list == null) {
                    list = new GLib.GenericArray<string> ();
                    by_remote.insert (key, list);
                }
                list.add (unique_id);
            });
        } catch (GLib.Error e) {
            warning ("Failed to enumerate installed apps: %s", e.message);
        }

        var self = weak_self.get () as Bz.RemotesDialog;
        if (self == null || self.generation != this_generation)
            return null;

        self.apps_by_remote = by_remote;
        self.rebuild ();
        return null;
    }

    private void rebuild () {
        var repositories = Bz.StateInfo.get_default ().repositories ?? new GLib.ListStore (typeof (Bz.Repository));

        uint n_system = bind_group (system_group, repositories, false);
        uint n_user = bind_group (user_group, repositories, true);

        list_stack.visible_child_name = n_system + n_user > 0 ? "list" : "empty";
    }

    private uint bind_group (Adw.PreferencesGroup group, GLib.ListModel repositories, bool is_user) {
        var filter = new Gtk.CustomFilter ((item) => {
            var repo = (Bz.Repository) item;
            return repo.is_user == is_user && !repo.noenumerate;
        });
        var sorter = new Gtk.CustomSorter ((a, b) => {
            bool a_flathub = ((Bz.Repository) a).name == "flathub";
            bool b_flathub = ((Bz.Repository) b).name == "flathub";
            return a_flathub == b_flathub ? Gtk.Ordering.EQUAL : (a_flathub ? Gtk.Ordering.SMALLER : Gtk.Ordering.LARGER);
        });
        var model = new Gtk.SortListModel (new Gtk.FilterListModel (repositories, filter), sorter);

        group.bind_model (model, (item) => create_row ((Bz.Repository) item));
        group.visible = model.get_n_items () > 0;

        return model.get_n_items ();
    }

    private Gtk.Widget create_row (Bz.Repository repo) {
        var state = Bz.StateInfo.get_default ();
        var apps = apps_by_remote.lookup (Bz.flatpak_remote_key (repo.name, repo.is_user));
        bool removable = (repo.is_user || repo.name != "flathub") && !state.busy && !state.syncing;

        var row = new Bz.RemoteRow (repo, apps != null ? apps.length : 0, removable);
        row.remove_requested.connect (() => on_remove_requested (repo));
        return row;
    }

    private void on_remove_requested (Bz.Repository repo) {
        var apps = apps_by_remote.lookup (Bz.flatpak_remote_key (repo.name, repo.is_user));

        if (apps != null && apps.length > 0)
            show_confirm (repo, apps);
        else
            present_confirm_alert (repo);
    }

    private void present_confirm_alert (Bz.Repository repo) {
        var alert = new Adw.AlertDialog (
            _("Remove “%s”?").printf (remote_title (repo)),
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
        confirm_title.label = _("Remove “%s”?").printf (remote_title (repo));
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

        var child = apps_box.get_first_child ();
        while (child != null) {
            var next = child.get_next_sibling ();
            apps_box.remove (child);
            child = next;
        }

        var weak_self = GLib.WeakRef (this);
        var ids = apps.copy (strdup);
        uint this_generation = generation;
        var fiber = Dex.Scheduler.get_default ().spawn (0, () => {
            return fill_apps (weak_self, ids, this_generation);
        });
        fiber.disown ();

        navigation_view.push_by_tag ("confirm");
    }

    private static Dex.Future? fill_apps (GLib.WeakRef weak_self, GLib.GenericArray<string> ids, uint this_generation) {
        var cache = Bz.StateInfo.get_default ().cache_manager;

        foreach (var unique_id in ids) {
            Bz.Entry? entry = null;
            try {
                entry = (Bz.Entry) cache.get (unique_id).await_object ();
            } catch (GLib.Error e) {
            }

            var self = weak_self.get () as Bz.RemotesDialog;
            if (self == null || self.generation != this_generation)
                return null;

            self.apps_box.append (create_app_card (entry, unique_id));
        }

        return null;
    }

    [GtkCallback]
    private void on_confirm_clicked () {
        if (confirm_repo != null)
            remove_remote (confirm_repo);
    }

    private static Gtk.Widget create_app_card (Bz.Entry? entry, string unique_id) {
        var parts = unique_id.split ("/");
        string app_id = parts.length > 1 ? parts[1] : unique_id;

        var icon = new Gtk.Image () {
            pixel_size = 60,
            paintable = entry?.icon_paintable,
        };
        if (icon.paintable == null)
            icon.icon_name = "application-x-executable";
        icon.add_css_class ("icon-dropshadow");

        var name_label = new Gtk.Label (entry?.title ?? app_id) {
            halign = Gtk.Align.START,
            ellipsize = Pango.EllipsizeMode.END,
        };

        var meta = new GLib.StringBuilder ();
        if (entry != null && entry.installed_version != null && entry.installed_version != "")
            meta.append (entry.installed_version);
        if (entry != null && entry.installed_size > 0) {
            if (meta.len > 0)
                meta.append (" • ");
            meta.append (GLib.format_size (entry.installed_size));
        }

        var meta_label = new Gtk.Label (meta.str) {
            halign = Gtk.Align.START,
            ellipsize = Pango.EllipsizeMode.END,
            visible = meta.len > 0,
        };
        meta_label.add_css_class ("caption");
        meta_label.add_css_class ("dim-label");

        var text_box = new Gtk.Box (Gtk.Orientation.VERTICAL, 3) {
            valign = Gtk.Align.CENTER,
            hexpand = true,
        };
        text_box.append (name_label);
        text_box.append (meta_label);

        var card = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 14) {
            height_request = 84,
        };
        card.append (icon);
        card.append (text_box);
        card.add_css_class ("card");
        card.add_css_class ("remote-app-card");

        return card;
    }

    private void remove_remote (Bz.Repository repo) {
        var app = GLib.Application.get_default () as Gtk.Application;
        if (app == null)
            return;

        close ();

        var fiber = Dex.Scheduler.get_default ().spawn (0, () => {
            return run_removal (app, repo);
        });
        fiber.disown ();
    }

    private static Dex.Future? run_removal (Gtk.Application app, Bz.Repository repo) {
        try {
            ((Bz.Application) app).remove_repository (repo.name, repo.is_user).await_boolean ();
        } catch (GLib.Error e) {
            warning ("Failed to remove remote %s: %s", repo.name, e.message);

            var alert = new Adw.AlertDialog (
                _("Could Not Remove “%s”").printf (remote_title (repo)),
                e.message
            );
            alert.add_response ("close", _("_Close"));
            alert.present (app.active_window);
        }

        return null;
    }
}
