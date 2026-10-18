/*
 * bz-other-sources-list.vala
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

public class Bz.OtherSourceRow : Adw.PreferencesRow {
    private Gtk.Button download_button;
    private Bz.FlatpakEntry entry;

    public signal void download (Bz.Entry entry);

    public OtherSourceRow (Bz.FlatpakEntry entry, Bz.Repository? repository, Bz.EntryGroup entry_group) {
        activatable = false;
        this.entry = entry;

        var title_label = new Gtk.Label (repository?.title) {
            halign = Gtk.Align.START,
            xalign = 0,
        };

        title_label.add_css_class ("origin-title");

        var subtitle_label = new Gtk.Label (extract_domain (repository?.url)) {
            halign = Gtk.Align.START,
            xalign = 0,
        };
        subtitle_label.add_css_class ("caption");
        subtitle_label.add_css_class ("dim-label");

        var pill_box = new Adw.WrapBox () {
            halign = Gtk.Align.START,
            valign = Gtk.Align.CENTER,
            child_spacing = 4,
            line_spacing = 4,
        };

        if (repository != null && repository.is_user)
            pill_box.append (create_pill ("person-symbolic", _("User"), "grey"));

        if (repository != null && repository.is_beta)
            pill_box.append (create_pill ("test-symbolic", _("Beta"), "warning"));

        if (entry_group.get_unique_id_is_eol (entry.unique_id))
            pill_box.append (create_pill ("dialog-warning-symbolic", _("End-Of-Life"), "error"));

        var title_row = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 6) {
            halign = Gtk.Align.START,
        };
        title_row.append (title_label);
        title_row.append (pill_box);

        var text_box = new Gtk.Box (Gtk.Orientation.VERTICAL, 0) {
            valign = Gtk.Align.CENTER,
            hexpand = true,
            margin_bottom = 2,
        };
        text_box.append (title_row);
        text_box.append (subtitle_label);

        download_button = new Gtk.Button.from_icon_name ("folder-download-symbolic") {
            valign = Gtk.Align.CENTER,
            tooltip_text = _("Install"),
        };
        download_button.add_css_class ("flat");

        download_button.clicked.connect (on_download_clicked);

        var root_box = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 12) {
            margin_top = 8,
            margin_bottom = 8,
            margin_start = 12,
            margin_end = 12,
        };
        root_box.append (text_box);
        root_box.append (download_button);

        child = root_box;
    }

    private void on_download_clicked () {
        download (entry);
    }

    private string extract_domain (string? url) {
        MatchInfo match_info;

        if (url == null)
            return "";

        var regex = /^(?:[a-zA-Z][a-zA-Z0-9+.-]*:\/\/)?([^\/]+)/;
        if (regex.match (url, 0, out match_info))
            return match_info.fetch (1);

        return "";
    }

    private Gtk.Widget create_pill (string icon_name, string label_text, string color_class) {
        var icon = new Gtk.Image.from_icon_name (icon_name) {
            pixel_size = 12,
            margin_top = 2,
            margin_bottom = 1,
        };

        var label = new Gtk.Label (label_text);
        label.add_css_class ("caption");

        var pill = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 4) {
            valign = Gtk.Align.CENTER,
            margin_top = 2,
            margin_bottom = 2,
        };
        pill.append (icon);
        pill.append (label);
        pill.add_css_class ("origin-rounded-box");
        pill.add_css_class ("colored");
        pill.add_css_class (color_class);

        return pill;
    }
}

public class Bz.OtherSourcesList : Adw.Bin {
    private Bz.EntryGroup? _entry_group;
    public Bz.EntryGroup? entry_group {
        get { return _entry_group; }
        set {
            if (_entry_group == value)
                return;

            _entry_group = value;
            rebuild ();
        }
    }

    public signal void download (Bz.Entry entry);

    private Gtk.ListBox listbox;

    construct {
        listbox = new Gtk.ListBox () {
            selection_mode = Gtk.SelectionMode.NONE,
        };
        listbox.add_css_class ("boxed-list");
        child = listbox;
        visible = false;
    }

    private void rebuild () {
        var group = _entry_group;

        var child_widget = listbox.get_first_child ();
        while (child_widget != null) {
            var next = child_widget.get_next_sibling ();
            listbox.remove (child_widget);
            child_widget = next;
        }
        visible = false;

        if (group == null)
            return;

        var weak_self = GLib.WeakRef (this);

        var scheduler = Dex.Scheduler.get_default ();
        var fiber = scheduler.spawn (0, () => {
            return populate (weak_self, group);
        });
        fiber.disown ();
    }

    private Dex.Future? populate (GLib.WeakRef weak_self, Bz.EntryGroup group) {
        var self = weak_self.get () as Bz.OtherSourcesList;
        if (self == null)
            return null;

        GLib.ListModel store;
        try {
            store = (GLib.ListModel) group.dup_all_into_store ().await_object ();
        } catch (GLib.Error e) {
            return null;
        }

        self = weak_self.get () as Bz.OtherSourcesList;
        if (self == null || self._entry_group != group)
            return null;

        string? ui_entry_id = group.dup_ui_entry_id ();
        var repositories = Bz.StateInfo.get_default ().repositories;
        uint n_items = store.get_n_items ();
        uint n_shown = 0;

        for (uint i = 0; i < n_items; i++) {
            var entry = (Bz.Entry) store.get_item (i);

            if (ui_entry_id != null && entry.unique_id == ui_entry_id)
                continue;

            var flatpak_entry = entry as Bz.FlatpakEntry;
            if (flatpak_entry == null)
                continue;

            Bz.Repository? repo = null;
            if (repositories != null)
                repo = entry.get_repository (repositories);

            var row = new Bz.OtherSourceRow (flatpak_entry, repo, group);
            row.download.connect (self.on_row_download);

            self.listbox.append (row);
            n_shown++;
        }

        self.visible = n_shown > 0;

        return null;
    }

    private void on_row_download (Bz.Entry entry) {
        download (entry);
    }
}
