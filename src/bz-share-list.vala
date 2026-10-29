/* bz-share-list.vala
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

private class Bz.ShareRow : Gtk.ListBoxRow {
    public string url { get; private set; }

    public ShareRow (string url, string title, string? icon_name) {
        this.url = url;
        activatable = true;

        var title_label = new Gtk.Label (title) {
            xalign = 0,
            ellipsize = Pango.EllipsizeMode.END,
        };
        title_label.add_css_class ("title");

        var subtitle = url;
        if (url.has_prefix ("http://") || url.has_prefix ("https://"))
            subtitle = url.substring (url.index_of ("://") + 3);

        var subtitle_label = new Gtk.Label (subtitle) {
            xalign = 0,
            ellipsize = Pango.EllipsizeMode.END,
        };
        subtitle_label.add_css_class ("subtitle");

        var title_box = new Gtk.Box (Gtk.Orientation.VERTICAL, 0) {
            valign = Gtk.Align.CENTER,
            hexpand = true,
        };
        title_box.add_css_class ("title");
        title_box.append (title_label);
        title_box.append (subtitle_label);

        var copy_button = new Gtk.Button.from_icon_name ("edit-copy-symbolic") {
            tooltip_text = _("Copy Link"),
            has_frame = false,
        };
        copy_button.clicked.connect (() => {
            get_clipboard ().set_text (this.url);
            ((Bz.Window) get_root ()).add_toast (new Adw.Toast (_("Copied!")) {
                timeout = 1,
            });
        });

        var separator = new Gtk.Separator (Gtk.Orientation.VERTICAL) {
            margin_top = 6,
            margin_bottom = 6,
        };
        var link_icon = new Gtk.Image.from_icon_name ("external-link-symbolic") {
            margin_start = 8,
            margin_end = 4,
        };

        var suffixes = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 4) {
            valign = Gtk.Align.CENTER,
        };
        suffixes.append (copy_button);
        suffixes.append (separator);
        suffixes.append (link_icon);

        var header = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 0) {
            valign = Gtk.Align.CENTER,
        };
        header.add_css_class ("header");
        if (icon_name != null) {
            var icon = new Gtk.Image.from_icon_name (icon_name);
            icon.add_css_class ("prefixes");
            header.append (icon);
        }
        header.append (title_box);
        header.append (suffixes);
        child = header;

        update_relation (Gtk.AccessibleRelation.LABELLED_BY, title_label, null,
                         Gtk.AccessibleRelation.DESCRIBED_BY, subtitle_label, null,
                         -1);
    }

    public static ShareRow for_url (Bz.Url item) {
        var url = item.url ?? "";

        switch (item.id) {
        case "flathub":     return new ShareRow (url, C_ ("Project URL Type", "Flathub Page"), "flathub-symbolic");
        case "homepage":    return new ShareRow (url, C_ ("Project URL Type", "Project Website"), "globe-symbolic");
        case "bugtracker":  return new ShareRow (url, C_ ("Project URL Type", "Issue Tracker"), "computer-fail-symbolic");
        case "faq":         return new ShareRow (url, C_ ("Project URL Type", "FAQ"), "help-faq-symbolic");
        case "help":        return new ShareRow (url, C_ ("Project URL Type", "Help"), "help-browser-symbolic");
        case "donation":    return new ShareRow (url, C_ ("Project URL Type", "Donate"), "heart-filled-symbolic");
        case "translate":   return new ShareRow (url, C_ ("Project URL Type", "Translate"), "translations-symbolic");
        case "contact":     return new ShareRow (url, C_ ("Project URL Type", "Contact"), "mail-send-symbolic");
        case "vcs-browser": return new ShareRow (url, C_ ("Project URL Type", "Source Code"), "code-symbolic");
        case "contribute":  return new ShareRow (url, C_ ("Project URL Type", "Contribute"), "system-users-symbolic");
        case "manifest":    return new ShareRow (url, C_ ("Project URL Type", "Manifest"), "shoe-box-symbolic");
        default:            return new ShareRow (url, url, null);
        }
    }
}

public class Bz.ShareList : Gtk.Box {
    public GLib.ListModel? urls { get; set; }
    public bool dual_column { get; set; }

    construct {
        orientation = Gtk.Orientation.VERTICAL;
        notify["urls"].connect (populate);
        notify["dual-column"].connect (populate);
    }

    private void populate () {
        for (var child = get_first_child (); child != null; child = get_first_child ())
            remove (child);

        if (urls == null)
            return;

        var n_items = urls.get_n_items ();
        if (!dual_column || n_items <= 1) {
            append (create_list_box (0, n_items));
            return;
        }

        var columns_box = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 12) {
            hexpand = true,
            homogeneous = true,
        };
        columns_box.append (create_list_box (0, (n_items + 1) / 2));
        columns_box.append (create_list_box ((n_items + 1) / 2, n_items));
        append (columns_box);
    }

    private Gtk.ListBox create_list_box (uint start, uint end) {
        var list_box = new Gtk.ListBox () {
            selection_mode = Gtk.SelectionMode.NONE,
            valign = Gtk.Align.START,
            hexpand = true,
        };
        list_box.add_css_class ("boxed-list");
        list_box.row_activated.connect ((row) => {
            row.activate_action_variant ("window.open-uri", ((Bz.ShareRow) row).url);
        });

        for (var i = start; i < end; i++)
            list_box.append (Bz.ShareRow.for_url ((Bz.Url) urls.get_item (i)));

        return list_box;
    }
}
