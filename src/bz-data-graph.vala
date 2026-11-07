/* bz-data-graph.vala
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

public sealed class Bz.DataGraph : Gtk.Widget {
    private const int TICK = 6;
    private const int GAP = 6;
    private const int PADDING = 12;
    private const int PAGE_STEP = 7;
    private const int ALL = 1 << 30;
    private const double ANGLE = 35;

    public GLib.ListModel? model {
        set {
            uint n = value?.get_n_items () ?? 0;
            points = new Bz.DataPoint[n];
            for (uint i = 0; i < n; i++)
                points[i] = (Bz.DataPoint) value.get_item (i);

            peak = 0;
            for (int i = 1; i < points.length; i++)
                if (points[i].dependent > points[peak].dependent)
                    peak = i;

            if (points.length > 0) {
                // TRANSLATORS: accessible description of the installs graph, like "14 May to 31 Oct, highest on 30 Oct with 4,600"
                update_property (Gtk.AccessibleProperty.DESCRIPTION,
                                 _("%s to %s, highest on %s with %s").printf (
                                     points[0].label ?? "", points[points.length - 1].label ?? "",
                                     points[peak].label ?? "", format_number (points[peak].dependent)));
            }

            selected = -1;
            layout_dirty = true;
            update_marker ();
        }
    }

    private Bz.DataPoint[] points = {};
    private int peak;
    private double[] px = {};
    private double[] py = {};
    private Graphene.Rect plot;
    private bool rtl;
    private bool layout_dirty = true;
    private int layout_width = -1;
    private int layout_height = -1;

    private Gsk.RenderNode? axes;
    private Gdk.Texture? line_texture;
    private Gdk.Texture? fill_texture;
    private Graphene.Rect line_bounds;
    private Graphene.Rect fill_bounds;
    private double texture_scale;

    private int selected = -1;
    private double pointer_y = -1;
    private double progress = 0;
    private bool animated;
    private bool focused;

    private Gtk.Box card;
    private Gtk.Label title_label;
    private Gtk.Label installs_label;
    private Gtk.GestureDrag drag;

    static construct {
        set_css_name ("datagraph");
        set_accessible_role (Gtk.AccessibleRole.SLIDER);

        install_action ("graph.step", "(ib)", on_step);
        add_binding_action (Gdk.Key.Left, 0, "graph.step", "(ib)", -1, true);
        add_binding_action (Gdk.Key.Right, 0, "graph.step", "(ib)", 1, true);
        add_binding_action (Gdk.Key.Up, 0, "graph.step", "(ib)", 1, false);
        add_binding_action (Gdk.Key.Down, 0, "graph.step", "(ib)", -1, false);
        add_binding_action (Gdk.Key.Page_Up, 0, "graph.step", "(ib)", PAGE_STEP, false);
        add_binding_action (Gdk.Key.Page_Down, 0, "graph.step", "(ib)", -PAGE_STEP, false);
        add_binding_action (Gdk.Key.Home, 0, "graph.step", "(ib)", -ALL, false);
        add_binding_action (Gdk.Key.End, 0, "graph.step", "(ib)", ALL, false);
    }

    construct {
        focusable = true;
        cursor = new Gdk.Cursor.from_name ("crosshair", null);

        title_label = new Gtk.Label (null) { xalign = 0 };
        title_label.add_css_class ("heading");

        installs_label = new Gtk.Label (null) { xalign = 0, use_markup = true };
        installs_label.add_css_class ("dimmed");

        card = new Gtk.Box (Gtk.Orientation.VERTICAL, 4) { can_target = false };
        card.add_css_class ("card");
        card.add_css_class ("floating-tooltip");
        card.append (title_label);
        card.append (installs_label);
        card.update_state (Gtk.AccessibleState.HIDDEN, true);
        card.set_child_visible (false);
        card.set_parent (this);

        var motion = new Gtk.EventControllerMotion ();
        motion.motion.connect (on_motion);
        motion.leave.connect (deselect);
        add_controller (motion);

        drag = new Gtk.GestureDrag () { touch_only = true };
        drag.drag_begin.connect (on_motion);
        drag.drag_update.connect (on_drag_update);
        drag.drag_end.connect (deselect);
        add_controller (drag);

        var focus = new Gtk.EventControllerFocus ();
        focus.enter.connect (() => {
            focused = true;
            update_marker ();
        });
        focus.leave.connect (() => {
            focused = false;
            deselect ();
            update_marker ();
        });
        add_controller (focus);

        Adw.StyleManager.get_default ().notify["accent-color"].connect (queue_draw);
    }

    public override void dispose () {
        card.unparent ();
        base.dispose ();
    }

    private void animate_in () {
        animated = true;

        var target = new Adw.CallbackAnimationTarget ((value) => {
            bool was_done = progress >= 1;
            progress = value;
            if (was_done != (value >= 1))
                update_marker ();
            queue_draw ();
        });
        new Adw.SpringAnimation (this, 0, 1, new Adw.SpringParams (1, 1, 80), target) {
            epsilon = 0.000001,
        }.play ();
    }

    public override void css_changed (Gtk.CssStyleChange change) {
        base.css_changed (change);
        layout_dirty = true;
        queue_allocate ();
    }

    public override void direction_changed (Gtk.TextDirection previous) {
        base.direction_changed (previous);
        layout_dirty = true;
        queue_allocate ();
    }

    public override void measure (Gtk.Orientation orientation, int for_size,
                                  out int minimum, out int natural,
                                  out int minimum_baseline, out int natural_baseline) {
        var metrics = get_pango_context ().get_metrics (null, null);
        int font_height = (int) Math.ceil (metrics.get_height () / (double) Pango.SCALE);
        minimum = natural = font_height * (orientation == Gtk.Orientation.HORIZONTAL ? 12 : 8);
        minimum_baseline = natural_baseline = -1;
    }

    public override void size_allocate (int width, int height, int baseline) {
        if (layout_dirty || width != layout_width || height != layout_height)
            relayout (width, height);

        Gtk.Requisition size;
        card.get_preferred_size (null, out size);
        double x = 0, y = 0;
        int k = marked ();
        if (k >= 0 && k < px.length) {
            x = px[k] + 12;
            if (x + size.width > width)
                x = px[k] - 12 - size.width;
            y = pointer_y >= 0 ? pointer_y + 12 : py[k] - size.height / 2.0;
        }
        x = x.clamp (0, double.max (0, width - size.width));
        y = y.clamp (0, double.max (0, height - size.height));
        card.allocate (size.width, size.height, -1,
                       new Gsk.Transform ().translate ({ (float) x, (float) y }));
    }

    private void relayout (int width, int height) {
        layout_dirty = false;
        layout_width = width;
        layout_height = height;
        rtl = get_direction () == Gtk.TextDirection.RTL;
        var old_px = px;
        var old_py = py;
        axes = null;
        px = {};
        py = {};

        int n = points.length;
        if (n < 2)
            return;

        double max_y = 0, min_x = points[0].independent, max_x = min_x;
        foreach (var p in points) {
            max_y = double.max (max_y, p.dependent);
            min_x = double.min (min_x, p.independent);
            max_x = double.max (max_x, p.independent);
        }

        var layout = create_pango_layout ("0");
        var attrs = new Pango.AttrList ();
        attrs.insert (Pango.attr_scale_new (0.82));
        attrs.insert (new Pango.AttrFontFeatures ("tnum=1"));
        layout.set_attributes (attrs);
        int lw, lh;
        layout.get_pixel_size (out lw, out lh);

        int max_ticks = ((int) ((height - 2 * PADDING) / (lh * 3.0))).clamp (2, 5);
        double step = double.max (1, nice (double.max (max_y, 1) / max_ticks));
        bool thousands = step % 1000 == 0;
        int top_tick = int.max (1, (int) Math.ceil (max_y / step));

        var y_labels = new string[top_tick + 1];
        int y_label_width = 0;
        for (int i = 0; i <= top_tick; i++) {
            // Translators: K is the suffix for thousands
            y_labels[i] = i == 0 ? "0" : thousands ? _("%.*fK").printf (0, i * step / 1000) : format_number (i * step);
            layout.set_text (y_labels[i], -1);
            layout.get_pixel_size (out lw, out lh);
            y_label_width = int.max (y_label_width, lw);
        }

        DateTime[] months = {};
        int month_width = 0;
        var start = new DateTime.from_unix_utc ((int64) min_x);
        var month = new DateTime.utc (start.get_year (), start.get_month (), 1, 0, 0, 0);
        if (month.to_unix () < min_x)
            month = month.add_months (1);
        for (; month.to_unix () <= max_x; month = month.add_months (1)) {
            months += month;
            layout.set_text (month.format ("%Ob"), -1);
            layout.get_pixel_size (out lw, out lh);
            month_width = int.max (month_width, lw);
        }

        double left = Math.ceil (PADDING + y_label_width + GAP);
        double top = Math.ceil (PADDING + lh / 2.0);
        double month_plot_width = width - left - PADDING - month_width / 2.0;
        bool by_month = months.length >= 2
            && 28 * 86400 * month_plot_width / (max_x - min_x) >= month_width + 2 * GAP;

        int x_label_width = 0;
        double cos_a = Math.cos (ANGLE * Math.PI / 180), sin_a = Math.sin (ANGLE * Math.PI / 180);
        double right, below;
        if (by_month) {
            right = PADDING + month_width / 2.0;
            below = TICK + 2 + lh + PADDING;
        } else {
            foreach (var p in points) {
                layout.set_text (p.label ?? "", -1);
                layout.get_pixel_size (out lw, out lh);
                x_label_width = int.max (x_label_width, lw);
            }
            left = Math.ceil (double.max (left, PADDING + x_label_width * cos_a));
            right = PADDING + lh * sin_a;
            below = TICK + 2 + x_label_width * sin_a + lh * cos_a + PADDING;
        }

        plot = rect (left, top, Math.floor (width - left - right), Math.floor (height - top - below));
        if (plot.size.width < 10 || plot.size.height < 10)
            return;

        double bottom = plot.origin.y + plot.size.height;
        double y_max = top_tick * step;
        var ux = new double[n];
        px = new double[n];
        py = new double[n];
        for (int i = 0; i < n; i++) {
            ux[i] = to_x (points[i].independent, min_x, max_x);
            px[i] = rtl ? width - ux[i] : ux[i];
            py[i] = bottom - points[i].dependent / y_max * plot.size.height;
        }

        var fg = get_color ();
        var dim = with_alpha (fg, 0.55f);
        var grid = with_alpha (fg, 0.1f);
        var base_color = with_alpha (fg, 0.25f);
        var s = new Gtk.Snapshot ();

        fill_rect (s, base_color, plot.origin.x, bottom, plot.size.width, 1);
        for (int i = 0; i <= top_tick; i++) {
            double y = Math.round (bottom - i / (double) top_tick * plot.size.height);
            if (i > 0) {
                var line = rect (rtl ? width - plot.origin.x - plot.size.width : plot.origin.x, y, plot.size.width, 1);
                s.push_repeat (line, rect (line.origin.x, y, 7, 1));
                s.append_color (grid, rect (line.origin.x, y, 3, 1));
                s.pop ();
            }
            layout.set_text (y_labels[i], -1);
            layout.get_pixel_size (out lw, out lh);
            draw_text (s, layout, dim, plot.origin.x - GAP - lw, y - lh / 2.0, lw);
        }

        if (by_month) {
            for (int i = 0; i < months.length; i++) {
                double x = Math.round (to_x (months[i].to_unix (), min_x, max_x));
                fill_rect (s, base_color, x, bottom + 1, 1, TICK);
                layout.set_text (months[i].format ("%Ob"), -1);
                layout.get_pixel_size (out lw, out lh);
                draw_text (s, layout, dim, (x - lw / 2.0).clamp (0, width - lw), bottom + 1 + TICK + 2, lw);
            }
        } else {
            double per_point = plot.size.width / (n - 1);
            int stride = int.max (1, (int) Math.ceil (x_label_width * 2 / per_point));
            for (int i = n - 1; i >= 0; i -= stride) {
                double x = Math.round (ux[i]);
                fill_rect (s, base_color, x, bottom + 1, 1, TICK);
                layout.set_text (points[i].label ?? "", -1);
                layout.get_pixel_size (out lw, out lh);
                draw_rotated_text (s, layout, dim, x, bottom + 1 + TICK + 2, lw);
            }
        }

        axes = s.to_node ();

        if (!same (old_px, px) || !same (old_py, py))
            line_texture = null;
    }

    private double to_x (double value, double min_x, double max_x) {
        return plot.origin.x + (value - min_x) / (max_x - min_x) * plot.size.width;
    }

    public override void snapshot (Gtk.Snapshot s) {
        if (axes == null)
            return;
        if (!animated)
            animate_in ();

        var accent = Adw.StyleManager.get_default ().get_accent_color_rgba ();
        double bottom = plot.origin.y + plot.size.height;

        if (progress > 0)
            snapshot_series (s, accent);
        s.append_node (axes);

        int k = marked ();
        if (k >= 0 && k < px.length) {
            s.append_color (with_alpha (get_color (), 0.5f), rect (Math.round (px[k]), plot.origin.y, 1, py[k] - plot.origin.y));
            s.append_color (with_alpha (accent, 0.4f), rect (Math.round (px[k]), py[k], 1, bottom - py[k]));
            append_dot (s, with_alpha (accent, 0.25f), px[k], py[k], 9);
            append_dot (s, accent, px[k], py[k], 5);
            snapshot_child (card, s);
        }
    }

    private void snapshot_series (Gtk.Snapshot s, Gdk.RGBA accent) {
        double scale = get_native ()?.get_surface ()?.get_scale () ?? 1.0;
        if (line_texture == null || texture_scale != scale)
            render_series (scale);

        int n = px.length;
        double f = progress.clamp (0, 1) * (n - 1);
        int i = int.min ((int) f, n - 2);
        double fill_end = px[i] + (px[i + 1] - px[i]) * (f - i);
        double head = fill_end + (progress >= 1 ? (rtl ? -PADDING : PADDING) : 0);
        double start = px[0] + (rtl ? PADDING : -PADDING);
        double bottom = plot.origin.y + plot.size.height;

        s.push_clip (rect (double.min (start, head), 0, (head - start).abs (), bottom + 1));

        Gsk.ColorStop[] gradient = {
            { 0.0f, with_alpha (accent, 0.35f) },
            { 1.0f, with_alpha (accent, 0) },
        };
        Gsk.ColorStop[] fade = {
            { 0.0f, { 0, 0, 0, 0 } },
            { 0.03f, { 0, 0, 0, 1 } },
            { 0.9f, { 0, 0, 0, 1 } },
            { 1.0f, { 0, 0, 0, 0 } },
        };
        s.push_mask (Gsk.MaskMode.ALPHA);
        s.append_linear_gradient (fill_bounds, { (float) px[0], 0 }, { (float) fill_end, 0 }, fade);
        s.pop ();
        s.push_mask (Gsk.MaskMode.ALPHA);
        s.append_texture (fill_texture, fill_bounds);
        s.pop ();
        s.append_linear_gradient (fill_bounds, { 0, plot.origin.y }, { 0, (float) bottom }, gradient);
        s.pop ();
        s.pop ();

        s.push_mask (Gsk.MaskMode.ALPHA);
        s.append_texture (line_texture, line_bounds);
        s.pop ();
        s.append_color (accent, line_bounds);
        s.pop ();

        s.pop ();
    }

    private void render_series (double scale) {
        texture_scale = scale;
        int n = px.length;
        float bottom = plot.origin.y + plot.size.height;
        Gdk.RGBA white = { 1, 1, 1, 1 };

        var line = new Gsk.PathBuilder ();
        line.move_to ((float) px[0], (float) py[0]);
        add_smooth (line, px, py);

        var area = new Gsk.PathBuilder ();
        area.move_to ((float) px[0], bottom);
        area.line_to ((float) px[0], (float) py[0]);
        add_smooth (area, px, py);
        area.line_to ((float) px[n - 1], bottom);
        area.close ();

        var stroke = new Gsk.Stroke (2);
        stroke.set_line_cap (Gsk.LineCap.ROUND);
        stroke.set_line_join (Gsk.LineJoin.ROUND);

        var s = new Gtk.Snapshot ();
        s.append_stroke (line.to_path (), stroke, white);
        line_texture = rasterize (s.to_node (), scale, out line_bounds);

        s = new Gtk.Snapshot ();
        s.append_fill (area.to_path (), Gsk.FillRule.WINDING, white);
        fill_texture = rasterize (s.to_node (), scale, out fill_bounds);
    }

    private static void add_smooth (Gsk.PathBuilder builder, double[] xs, double[] ys) {
        int n = xs.length;
        for (int i = 0; i < n - 1; i++) {
            int a = int.max (i - 1, 0), d = int.min (i + 2, n - 1);
            builder.cubic_to ((float) (xs[i] + (xs[i + 1] - xs[a]) / 6), (float) (ys[i] + (ys[i + 1] - ys[a]) / 6),
                              (float) (xs[i + 1] - (xs[d] - xs[i]) / 6), (float) (ys[i + 1] - (ys[d] - ys[i]) / 6),
                              (float) xs[i + 1], (float) ys[i + 1]);
        }
    }

    private Gdk.Texture rasterize (Gsk.RenderNode node, double scale, out Graphene.Rect bounds) {
        var b = node.get_bounds ();
        double x0 = Math.floor (b.origin.x * scale), y0 = Math.floor (b.origin.y * scale);
        double x1 = Math.ceil ((b.origin.x + b.size.width) * scale);
        double y1 = Math.ceil ((b.origin.y + b.size.height) * scale);
        bounds = rect (x0 / scale, y0 / scale, (x1 - x0) / scale, (y1 - y0) / scale);

        var scaled = new Gsk.TransformNode (node, new Gsk.Transform ().scale ((float) scale, (float) scale));
        return get_native ().get_renderer ().render_texture (scaled, rect (x0, y0, x1 - x0, y1 - y0));
    }

    private int marked () {
        if (selected >= 0)
            return selected;
        return focused && progress >= 1 && points.length >= 2 ? points.length - 1 : -1;
    }

    private void update_marker () {
        int k = marked ();
        card.set_child_visible (k >= 0);
        if (k >= 0) {
            title_label.label = new DateTime.from_unix_utc ((int64) points[k].independent).format ("%-d %B");
            installs_label.label = "%s <span font_family=\"monospace\">%s</span>".printf (
                // TRANSLATORS: label prefix shown in graph tooltips, for example: "Installs: 42"
                Markup.escape_text (_("Installs:")), format_number (points[k].dependent));
            update_property (Gtk.AccessibleProperty.VALUE_MIN, 0.0,
                             Gtk.AccessibleProperty.VALUE_MAX, (double) (points.length - 1),
                             Gtk.AccessibleProperty.VALUE_NOW, (double) k,
                             Gtk.AccessibleProperty.VALUE_TEXT,
                             "%s, %s".printf (title_label.label, installs_label.get_text ()));
        }
        queue_allocate ();
        queue_draw ();
    }

    private void select (int index) {
        if (index == selected)
            return;
        selected = index;
        update_marker ();
    }

    private void on_motion (double x, double y) {
        pointer_y = y;
        select (index_at (x));
        queue_allocate ();
    }

    private void on_drag_update (double offset_x, double offset_y) {
        double x, y;
        drag.get_start_point (out x, out y);
        on_motion (x + offset_x, y + offset_y);
    }

    private void deselect () {
        pointer_y = -1;
        select (-1);
    }

    private static void on_step (Gtk.Widget widget, string name, Variant? parameter) {
        var self = (DataGraph) widget;
        int delta;
        bool horizontal;
        parameter.get ("(ib)", out delta, out horizontal);
        if (self.px.length < 2)
            return;

        int current = self.selected >= 0 ? self.selected : self.px.length - 1;
        self.pointer_y = -1;
        self.select ((current + (horizontal && self.rtl ? -delta : delta)).clamp (0, self.px.length - 1));
    }

    private int index_at (double x) {
        int n = px.length;
        if (n < 2 || x < double.min (px[0], px[n - 1]) - GAP || x > double.max (px[0], px[n - 1]) + GAP)
            return -1;

        int best = 0;
        for (int i = 1; i < n; i++)
            if ((px[i] - x).abs () < (px[best] - x).abs ())
                best = i;
        return best;
    }

    private void fill_rect (Gtk.Snapshot s, Gdk.RGBA color, double x, double y, double w, double h) {
        s.append_color (color, rect (rtl ? layout_width - x - w : x, y, w, h));
    }

    private void draw_text (Gtk.Snapshot s, Pango.Layout layout, Gdk.RGBA color, double x, double y, int w) {
        s.save ();
        s.translate ({ (float) (rtl ? layout_width - x - w : x), (float) y });
        s.append_layout (layout, color);
        s.restore ();
    }

    private void draw_rotated_text (Gtk.Snapshot s, Pango.Layout layout, Gdk.RGBA color, double x, double y, int w) {
        s.save ();
        s.translate ({ (float) (rtl ? layout_width - x : x), (float) y });
        s.rotate ((float) (rtl ? ANGLE : -ANGLE));
        if (!rtl)
            s.translate ({ -w, 0 });
        s.append_layout (layout, color);
        s.restore ();
    }

    private static bool same (double[] a, double[] b) {
        if (a.length != b.length)
            return false;
        for (int i = 0; i < a.length; i++)
            if (a[i] != b[i])
                return false;
        return true;
    }

    private static string format_number (double value) {
        return "%'.0f".printf (value);
    }

    private static double nice (double value) {
        double exponent = Math.pow (10, Math.floor (Math.log10 (value)));
        double f = value / exponent;
        return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * exponent;
    }

    private static Gdk.RGBA with_alpha (Gdk.RGBA color, float alpha) {
        color.alpha *= alpha;
        return color;
    }

    private static void append_dot (Gtk.Snapshot s, Gdk.RGBA color, double x, double y, double r) {
        Gsk.RoundedRect clip = {};
        clip.init_from_rect (rect (x - r, y - r, 2 * r, 2 * r), (float) r);
        s.push_rounded_clip (clip);
        s.append_color (color, clip.bounds);
        s.pop ();
    }

    private static Graphene.Rect rect (double x, double y, double w, double h) {
        Graphene.Rect r = {};
        r.init ((float) x, (float) y, (float) w, (float) h);
        return r;
    }
}
