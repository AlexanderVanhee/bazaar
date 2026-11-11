/* bz-distro-chart.vala
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

private sealed class Bz.DistroChartRow : Gtk.Widget {
    public DistroDataPoint point;
    public double share;

    public bool active {
        get { return (get_state_flags () & (Gtk.StateFlags.PRELIGHT | Gtk.StateFlags.FOCUS_VISIBLE)) != 0; }
    }

    class construct {
        set_css_name ("distro-chart-row");
        set_accessible_role (Gtk.AccessibleRole.LIST_ITEM);
    }

    public DistroChartRow (DistroDataPoint point, double share) {
        this.point = point;
        this.share = share;
        focusable = true;
        focus_on_click = false;
        /* Translators: A row of the install statistics, for example: Fedora, 28% */
        update_property (Gtk.AccessibleProperty.LABEL, _("%s, %s").printf (point.label, DistroChart.format_share (share)), -1);
        state_flags_changed.connect (() => {
            if (get_parent () != null)
                get_parent ().queue_draw ();
        });
    }
}

public sealed class Bz.DistroChart : Gtk.Widget {
    private const int ROW_HEIGHT = 36;
    private const float BAR_HEIGHT = 22;
    private const float BAR_RADIUS = 6;
    private const int AXIS_HEIGHT = 28;
    private const double AXIS_LABEL_SPACING = 8;
    private const double ROW_PADDING = 8;
    private const double LABEL_SPACING = 16;
    private const double VALUE_SPACING = 8;
    private const double VALUE_WIDTH = 44;
    private const float GRID_ALPHA = 0.12f;
    private const float TEXT_ALPHA = 0.65f;

    private GenericArray<DistroChartRow> rows = new GenericArray<DistroChartRow> ();
    private double axis_max = 1;
    private double axis_step = 0.25;
    private double progress = 0;
    private bool animated;

    public GLib.ListModel? model { get; set; }

    class construct {
        set_css_name ("distro-chart");
        set_accessible_role (Gtk.AccessibleRole.LIST);
    }

    construct {
        var keys = new Gtk.EventControllerKey ();
        keys.key_pressed.connect (on_key_pressed);
        add_controller (keys);

        notify["model"].connect (refresh_rows);
        Adw.StyleManager.get_default ().notify["accent-color-rgba"].connect (queue_draw);
    }

    protected override void dispose () {
        Adw.StyleManager.get_default ().notify["accent-color-rgba"].disconnect (queue_draw);
        remove_rows ();
        base.dispose ();
    }

    private void animate_in () {
        animated = true;

        var target = new Adw.CallbackAnimationTarget ((value) => {
            progress = value;
            queue_draw ();
        });
        new Adw.SpringAnimation (this, 0, 1, new Adw.SpringParams (1, 1, 120), target) { epsilon = 0.00001 }.play ();
    }

    private bool on_key_pressed (uint keyval, uint keycode, Gdk.ModifierType state) {
        if (rows.length == 0)
            return Gdk.EVENT_PROPAGATE;

        switch (keyval) {
        case Gdk.Key.Home:
        case Gdk.Key.KP_Home:
            rows[0].grab_focus ();
            return Gdk.EVENT_STOP;
        case Gdk.Key.End:
        case Gdk.Key.KP_End:
            rows[rows.length - 1].grab_focus ();
            return Gdk.EVENT_STOP;
        default:
            return Gdk.EVENT_PROPAGATE;
        }
    }

    private void remove_rows () {
        foreach (var row in rows)
            row.unparent ();
        rows = new GenericArray<DistroChartRow> ();
    }

    private void refresh_rows () {
        remove_rows ();

        var points = new GenericArray<DistroDataPoint> ();
        int64 reported = 0;
        for (uint i = 0; model != null && i < model.get_n_items (); i++) {
            var point = (DistroDataPoint) model.get_item (i);
            if (!point.is_unreported) {
                points.add (point);
                reported += point.installs;
            }
        }

        var largest = 0.0;
        foreach (var point in points) {
            var share = (double) point.installs / reported;
            largest = double.max (largest, share);

            var row = new DistroChartRow (point, share);
            row.set_parent (this);
            rows.add (row);
        }

        axis_step = 0.25;
        foreach (var step in new double[] { 0.01, 0.02, 0.05, 0.1, 0.2, 0.25 }) {
            if (largest / step <= 5) {
                axis_step = step;
                break;
            }
        }
        axis_max = double.max (Math.ceil (largest / axis_step), 1) * axis_step;

        queue_resize ();
    }

    internal static string format_share (double fraction) {
        if (fraction > 0 && fraction < 0.01)
            /* Translators: Shown for values smaller than 1 percent */
            return _("<1%");
        /* Translators: A percentage, for example: 42% */
        return _("%.0f%%").printf (fraction * 100);
    }

    private double get_label_width () {
        var largest = 0;
        foreach (var row in rows) {
            int width;
            create_pango_layout (row.point.label).get_pixel_size (out width, null);
            largest = int.max (largest, width);
        }
        return largest;
    }

    protected override void measure (Gtk.Orientation orientation, int for_size,
                                     out int minimum, out int natural,
                                     out int minimum_baseline, out int natural_baseline) {
        minimum_baseline = natural_baseline = -1;
        if (orientation == Gtk.Orientation.HORIZONTAL) {
            minimum = (int) (2 * ROW_PADDING + get_label_width () + LABEL_SPACING + VALUE_WIDTH + 80);
            natural = minimum + 320;
        } else {
            minimum = natural = rows.length * ROW_HEIGHT + AXIS_HEIGHT;
        }
    }

    protected override void size_allocate (int width, int height, int baseline) {
        for (var i = 0; i < rows.length; i++) {
            Gtk.Allocation allocation = { 0, i * ROW_HEIGHT, width, ROW_HEIGHT };
            rows[i].allocate_size (allocation, -1);
        }
    }

    private static Gdk.RGBA mix (Gdk.RGBA color, float target, float amount) {
        return {
            color.red + (target - color.red) * amount,
            color.green + (target - color.green) * amount,
            color.blue + (target - color.blue) * amount,
            1
        };
    }

    private void append_text (Gtk.Snapshot snapshot, string text, double x, double y, double align, Gdk.RGBA color) {
        int text_width, text_height;
        var layout = create_pango_layout (text);
        layout.get_pixel_size (out text_width, out text_height);

        snapshot.save ();
        snapshot.translate ({
            (float) Math.round (x - text_width * align),
            (float) Math.round (y - text_height / 2.0)
        });
        snapshot.append_layout (layout, color);
        snapshot.restore ();
    }

    protected override void snapshot (Gtk.Snapshot snapshot) {
        if (!animated && rows.length > 0)
            animate_in ();

        var rtl = get_direction () == Gtk.TextDirection.RTL;
        double width = get_width ();
        var plot_start = ROW_PADDING + get_label_width () + LABEL_SPACING;
        var plot_width = width - plot_start - VALUE_WIDTH - ROW_PADDING;
        double plot_height = rows.length * ROW_HEIGHT;
        var edge_align = rtl ? 1.0 : 0.0;

        var color = get_color ();
        var dimmed = color;
        dimmed.alpha = TEXT_ALPHA;
        var grid = color;
        grid.alpha = GRID_ALPHA;
        var style = Adw.StyleManager.get_default ();
        var accent = style.get_accent_color_rgba ();
        var background = style.get_dark () ? 0.0f : 1.0f;

        foreach (var row in rows)
            snapshot_child (row, snapshot);

        int axis_label_width;
        create_pango_layout (format_share (axis_max)).get_pixel_size (out axis_label_width, null);
        var n_ticks = (int) Math.round (axis_max / axis_step);
        var label_stride = 1;
        while (label_stride < n_ticks && plot_width * label_stride / n_ticks < axis_label_width + AXIS_LABEL_SPACING)
            label_stride++;

        for (var i = 0; i <= n_ticks; i++) {
            var x = plot_start + plot_width * i / n_ticks;
            x = rtl ? width - x : x;

            snapshot.append_color (grid, {{ (float) x, 0 }, { 1, (float) plot_height }});
            if (i % label_stride == 0)
                append_text (snapshot, format_share (i * axis_step), x, plot_height + AXIS_HEIGHT / 2.0, 0.5, dimmed);
        }

        for (var i = 0; i < rows.length; i++) {
            var row = rows[i];
            double y = ROW_HEIGHT * i + ROW_HEIGHT / 2.0;
            var text = row.active ? color : dimmed;

            Gdk.RGBA bar;
            if (row.point.is_other)
                bar = mix (color, background, row.active ? 0.5f : 0.65f);
            else
                bar = row.active ? mix (accent, 1, 0.15f) : accent;

            append_text (snapshot, row.point.label, rtl ? width - ROW_PADDING : ROW_PADDING, y, edge_align, text);

            var bar_width = double.max (plot_width * row.share / axis_max, 2) * progress;
            var bar_x = rtl ? width - plot_start - bar_width : plot_start;

            var rounded = Gsk.RoundedRect ();
            rounded.init_from_rect (
                {{ (float) bar_x, (float) y - BAR_HEIGHT / 2 }, { (float) bar_width, BAR_HEIGHT }},
                float.min (BAR_RADIUS, (float) bar_width / 2)
            );
            snapshot.push_rounded_clip (rounded);
            snapshot.append_color (bar, rounded.bounds);
            snapshot.pop ();

            var value_x = plot_start + bar_width + VALUE_SPACING;
            append_text (snapshot, format_share (row.share), rtl ? width - value_x : value_x, y, edge_align, text);
        }
    }
}
