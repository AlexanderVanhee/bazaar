/*
 * bz-flathub-category-section.vala
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

[GtkTemplate (ui = "/io/github/kolunmi/Bazaar/bz-flathub-category-section.ui")]
public class Bz.FlathubCategorySection : Gtk.Box {
    // Shared by all sections so that lazy sections fill one per frame
    private static int64 lazy_fill_frame = -1;

    [GtkChild]
    private unowned Gtk.Label section_title;
    [GtkChild]
    private unowned Bz.DynamicListView section_list;
    [GtkChild]
    private unowned Gtk.Button more_button;

    private Gtk.SliceListModel? slice_model;
    private bool lazy_filled = false;
    private ulong lazy_fill_id = 0;

    private Bz.FlathubCategory? _category;
    public Bz.FlathubCategory? category {
        get { return _category; }
        set {
            if (_category == value)
                return;
            _category = value;
            slice_model = null;

            if (_category != null) {
                section_title.label = _category.get_display_name ();
                more_button.label = _category.get_more_of_name ();
                update_model ();
            }
        }
    }

    private bool _compact = false;
    public bool compact {
        get { return _compact; }
        set {
            if (_compact == value)
                return;
            _compact = value;
            update_model ();
        }
    }

    private uint _min_items = 0;
    public uint min_items {
        get { return _min_items; }
        set {
            if (_min_items == value)
                return;
            _min_items = value;
            update_model ();
        }
    }

    private uint _max_items = 12;
    public uint max_items {
        get { return _max_items; }
        set {
            if (_max_items == value)
                return;
            _max_items = value;
            update_model ();
        }
    }

    public uint max_children_per_line { get; set; default = 4; }
    public bool has_title { get; set; default = true; }
    public bool show_more { get; set; default = true; }
    public bool lazy { get; set; default = false; }

    private void update_model () {
        if (_category == null)
            return;

        uint max = uint.max (_compact ? 6 : _max_items, _min_items);

        if (slice_model != null) {
            slice_model.size = max;
            return;
        }

        if (lazy && !lazy_filled && lazy_fill_id == 0) {
            var clock = (get_root () as Gtk.Widget)?.get_frame_clock ();
            if (clock != null)
                lazy_fill_id = clock.after_paint.connect (lazy_fill_after_paint);
        }
        if (lazy_fill_id != 0)
            max = 0;

        slice_model = new Gtk.SliceListModel (null, 0, max);
        _category.bind_property ("applications", slice_model, "model", BindingFlags.SYNC_CREATE);
        section_list.model = slice_model;
    }

    private void lazy_fill_after_paint (Gdk.FrameClock clock) {
        clock.request_phase (Gdk.FrameClockPhase.AFTER_PAINT);

        int64 frame = clock.get_frame_counter ();
        if (frame == lazy_fill_frame)
            return;
        lazy_fill_frame = frame;

        clock.disconnect (lazy_fill_id);
        lazy_fill_id = 0;
        lazy_filled = true;
        update_model ();
    }

    [GtkCallback]
    private void on_more_button_clicked (Gtk.Button button) {
        var nav_view = get_ancestor (typeof (Adw.NavigationView)) as Adw.NavigationView;
        if (_category == null || nav_view == null || get_ancestor (typeof (Bz.FlathubPage)) == null)
            return;

        var apps_page = Bz.AppsPage.new_from_category (_category);
        if (apps_page != null)
            nav_view.push (apps_page);
    }

    [GtkCallback]
    private int get_spacing (bool compact) {
        return compact ? 9 : 11;
    }

    [GtkCallback]
    private bool invert_boolean (bool value) {
        return !value;
    }

    [GtkCallback]
    private bool is_null (Object? value) {
        return value == null;
    }
}
