/* bz-world-map.vala
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

public sealed class Bz.WorldMap : Gtk.Widget {
    private const double MAP_WIDTH = 1000;
    private const double MAP_HEIGHT = 500;
    private const double OPACITY_MULTIPLIER = 2;
    private const int CARD_OFFSET = 10;

    private static string[] isos;
    private static string[] names;
    private static Gsk.Path[] paths;

    public GLib.ListModel? model {
        set {
            downloads = new uint[isos.length];
            max_downloads = 0;
            var order = new GLib.List<int> ();
            uint n = value?.get_n_items () ?? 0;
            for (uint i = 0; i < n; i++) {
                var point = (Bz.CountryDataPoint) value.get_item (i);
                for (int k = 0; k < isos.length; k++) {
                    if (isos[k] == point.country_code) {
                        downloads[k] = point.downloads;
                        max_downloads = uint.max (max_downloads, point.downloads);
                        order.prepend (k);
                    }
                }
            }

            order.sort_with_data ((a, b) => (int) (downloads[b] - downloads[a]));
            string[] top = {};
            for (unowned GLib.List<int>? l = order; l != null && top.length < 5; l = l.next)
                top += ngettext ("%s, %'u install", "%s, %'u installs", downloads[l.data])
                           .printf (names[l.data], downloads[l.data]);
            // Translators: accessible description of the world map, like. "Most installs from Germany, 1,200 installs, France, 800 installs"
            update_property (Gtk.AccessibleProperty.DESCRIPTION,
                             top.length > 0 ? _("Most installs from %s").printf (string.joinv (", ", top)) : "");

            set_hovered (-1);
            queue_draw ();
        }
    }

    private uint[] downloads = new uint[isos.length];
    private uint max_downloads;
    private int hovered = -1;
    private double pointer_x;
    private double pointer_y;
    private double scale = 1;
    private double offset_x;
    private double offset_y;

    private Gtk.Box card;
    private Gtk.Label name_label;
    private Gtk.Label count_label;
    private Gtk.Label suffix_label;

    static construct {
        set_css_name ("worldmap");
        set_accessible_role (Gtk.AccessibleRole.IMG);
        load ();
    }

    private static void load () {
        Bytes bytes;
        try {
            bytes = GLib.resources_lookup_data ("/io/github/kolunmi/Bazaar/countries.gvariant",
                                                GLib.ResourceLookupFlags.NONE);
        } catch (Error e) {
            assert_not_reached ();
        }
        var data = new Variant.from_bytes (new VariantType ("a(ssa{ss}s)"), bytes, true);
        foreach (var country in data) {
            isos += country.get_child_value (1).get_string ();
            names += translated_name (country.get_child_value (2), country.get_child_value (0).get_string ());
            paths += Gsk.Path.parse (country.get_child_value (3).get_string ());
        }
    }

    construct {
        focusable = true;
        name_label = new Gtk.Label (null) { xalign = 0 };
        name_label.add_css_class ("heading");

        count_label = new Gtk.Label (null) { xalign = 0 };
        count_label.add_css_class ("monospace");

        suffix_label = new Gtk.Label (null) { xalign = 0 };
        suffix_label.add_css_class ("dim-label");

        var count_box = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 6);
        count_box.append (count_label);
        count_box.append (suffix_label);

        card = new Gtk.Box (Gtk.Orientation.VERTICAL, 4) { can_target = false };
        card.add_css_class ("floating-tooltip");
        card.add_css_class ("card");
        card.append (name_label);
        card.append (count_box);
        card.update_state (Gtk.AccessibleState.HIDDEN, true);
        card.set_child_visible (false);
        card.set_parent (this);

        var motion = new Gtk.EventControllerMotion ();
        motion.motion.connect (on_motion);
        motion.leave.connect (() => set_hovered (-1));
        add_controller (motion);

        var drag = new Gtk.GestureDrag () { touch_only = true };
        drag.drag_begin.connect (on_motion);
        drag.drag_update.connect ((dx, dy) => {
            double x, y;
            drag.get_start_point (out x, out y);
            on_motion (x + dx, y + dy);
        });
        add_controller (drag);

        Adw.StyleManager.get_default ().notify["accent-color"].connect (queue_draw);
    }
    public override void dispose () {
        card.unparent ();
        base.dispose ();
    }

    public override void size_allocate (int width, int height, int baseline) {
        scale = double.min (width / MAP_WIDTH, height / MAP_HEIGHT);
        offset_x = (width - MAP_WIDTH * scale) / 2;
        offset_y = (height - MAP_HEIGHT * scale) / 2;

        Gtk.Requisition size;
        card.get_preferred_size (null, out size);
        double x = pointer_x > width / 2.0 ? pointer_x - size.width - CARD_OFFSET : pointer_x + CARD_OFFSET;
        double y = pointer_y + CARD_OFFSET;
        x = x.clamp (0, double.max (0, width - size.width));
        y = y.clamp (0, double.max (0, height - size.height));
        card.allocate (size.width, size.height, -1,
                       new Gsk.Transform ().translate ({ (float) x, (float) y }));
    }

    public override void snapshot (Gtk.Snapshot s) {
        var style = Adw.StyleManager.get_default ();
        var accent = style.get_accent_color_rgba ();
        var border = get_color ();
        border.alpha = 0.3f;
        var stroke = new Gsk.Stroke (0.5f);

        s.save ();
        s.translate ({ (float) offset_x, (float) offset_y });
        s.scale ((float) scale, (float) scale);
        for (int i = 0; i < paths.length; i++) {
            uint d = downloads[i];
            if (d > 0) {
                var fill = accent;
                fill.alpha = (float) (d / (double) max_downloads * OPACITY_MULTIPLIER).clamp (0.1, 1.0);
                s.append_fill (paths[i], Gsk.FillRule.EVEN_ODD, fill);
            }
            s.append_stroke (paths[i], stroke, border);
        }

        if (hovered >= 0)
            s.append_stroke (paths[hovered], new Gsk.Stroke (1.5f), style.accent_color.to_standalone_rgba (style.dark));
        s.restore ();

        if (hovered >= 0)
            snapshot_child (card, s);
    }

    private void on_motion (double x, double y) {
        pointer_x = x;
        pointer_y = y;
        set_hovered (country_at (x, y));
        queue_allocate ();
    }

    private void set_hovered (int index) {
        if (index == hovered)
            return;
        hovered = index;
        card.set_child_visible (index >= 0);
        if (index >= 0) {
            uint count = downloads[index];
            name_label.label = names[index];
            count_label.label = "%'u".printf (count);
            // Translators: As in, "1 Install" / "100 Installs"
            suffix_label.label = ngettext ("Install", "Installs", count);
        }
        queue_draw ();
    }

    private int country_at (double x, double y) {
        Graphene.Point point = { (float) ((x - offset_x) / scale), (float) ((y - offset_y) / scale) };
        for (int i = 0; i < paths.length; i++)
            if (paths[i].in_fill (point, Gsk.FillRule.EVEN_ODD))
                return i;
        return -1;
    }

    private static string translated_name (Variant translations, string fallback) {
        foreach (unowned string language in Intl.get_language_names ()) {
            var name = translations.lookup_value (language, VariantType.STRING);
            if (name != null)
                return name.get_string ();
        }
        return fallback;
    }
}
