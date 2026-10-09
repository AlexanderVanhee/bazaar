/* bz-screenshot-page.vala
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

[GtkTemplate (ui = "/io/github/kolunmi/Bazaar/bz-screenshot-page.ui")]
public class Bz.ScreenshotPage : Adw.Bin {
    private const uint MOUSE_BACK_BUTTON = 8;

    private const double SPRING_DAMPING_RATIO = 1.05;
    private const double SPRING_MASS = 1.0;
    private const double SPRING_STIFFNESS = 1000.0;
    private const uint FADE_DURATION_MS = 350;

    [GtkChild]
    private unowned Adw.Carousel carousel;
    [GtkChild]
    private unowned Adw.ToastOverlay toast_overlay;

    private ListModel? captions = null;

    private bool reduce_motion = false;
    private bool closing = false;

    private unowned Gtk.Widget? _source_widget = null;
    private Gdk.Paintable? source_image = null;
    private Graphene.Rect source_bounds_at_map;
    private Adw.Animation? animation = null;
    private double animation_progress = 0.0;

    public ListModel? screenshots { get; construct; }
    public uint initial_index { get; construct; }

    private uint _current_index = 0;
    public uint current_index {
        get { return _current_index; }
    }

    [CCode (notify = false)]
    public string? current_caption {
        get {
            if (captions == null)
                return null;

            var caption = captions.get_item (_current_index) as Gtk.StringObject;
            return caption?.string;
        }
    }

    public bool is_zoomed { get; private set; default = false; }

    public Gtk.Widget? source_widget {
        get { return _source_widget; }
        set {
            if (_source_widget == value)
                return;

            if (_source_widget != null)
                _source_widget.opacity = 1.0;

            _source_widget = value;
            source_image = null;

            if (get_mapped () && !reduce_motion && _source_widget != null)
                capture_source ();
        }
    }

    public ScreenshotPage (ListModel screenshots, ListModel? captions,
                           uint initial_index, Gtk.Widget? source_widget) {
        Object (screenshots: screenshots, initial_index: initial_index);

        this.source_widget = source_widget;
        set_captions (captions);
    }

    static construct {
        install_action ("screenshot.previous", null, (w, a, p) => ((ScreenshotPage) w).step (-1));
        install_action ("screenshot.next", null, (w, a, p) => ((ScreenshotPage) w).step (1));
        install_action ("screenshot.zoom-in", null, (w, a, p) => ((ScreenshotPage) w).get_current_zoom ()?.zoom_in ());
        install_action ("screenshot.zoom-out", null, (w, a, p) => ((ScreenshotPage) w).get_current_zoom ()?.zoom_out ());
        install_action ("screenshot.zoom-reset", null, (w, a, p) => ((ScreenshotPage) w).get_current_zoom ()?.reset ());
        install_action ("screenshot.copy", null, (w, a, p) => ((ScreenshotPage) w).copy_clicked ());
    }

    construct {
        populate_carousel ();

        if (initial_index < carousel.n_pages)
            _current_index = initial_index;

        var zoom = get_current_zoom ();
        if (zoom != null)
            zoom.notify["zoom-level"].connect (update_is_zoomed);

        update_is_zoomed ();

        var swipe = new Gtk.GestureSwipe () {
            propagation_phase = Gtk.PropagationPhase.CAPTURE,
            touch_only = true,
        };
        swipe.swipe.connect ((vx, vy) => {
            if (Math.fabs (vy) > 500.0)
                back_clicked ();
        });
        add_controller (swipe);

        var click = new Gtk.GestureClick () {
            button = MOUSE_BACK_BUTTON,
            propagation_phase = Gtk.PropagationPhase.CAPTURE,
        };
        click.pressed.connect ((n, x, y) => back_clicked ());
        add_controller (click);

        var scroll = new Gtk.EventControllerScroll (Gtk.EventControllerScrollFlags.VERTICAL);
        scroll.scroll.connect (on_scroll);
        add_controller (scroll);
    }

    public void set_captions (ListModel? captions) {
        this.captions = captions;
        notify_property ("current-caption");
    }

    public void close () {
        back_clicked ();
    }

    public bool is_closing () {
        return closing;
    }

    public override void map () {
        base.map ();

        child_focus (Gtk.DirectionType.TAB_FORWARD);

        closing = false;
        reduce_motion = get_settings ().gtk_interface_reduced_motion != Gtk.ReducedMotion.NO_PREFERENCE;
        source_image = null;
        animation_progress = reduce_motion ? 1.0 : 0.0;

        if (reduce_motion) {
            opacity = 0.0;
        } else if (source_widget != null) {
            capture_source ();
            set_image_hidden (true);
        }

        add_tick_callback ((w, clock) => {
            if (carousel.get_width () == 0)
                return Source.CONTINUE;

            carousel.scroll_to (carousel.get_nth_page (_current_index), false);

            if (!closing)
                start_animation (1.0);

            return Source.REMOVE;
        });
    }

    private void capture_source () {
        source_image = new Gtk.WidgetPaintable (_source_widget).get_current_image ();

        if (!_source_widget.compute_bounds (this, out source_bounds_at_map))
            source_bounds_at_map = Graphene.Rect ().init (0, 0, get_width (), get_height ());

        _source_widget.opacity = 0.0;
    }

    public override void unmap () {
        stop_animation ();

        if (source_widget != null)
            source_widget.opacity = 1.0;

        set_image_hidden (false);

        base.unmap ();
    }

    [GtkCallback]
    private void back_clicked () {
        if (closing)
            return;

        if (source_widget == null) {
            remove_from_overlay ();
            return;
        }

        closing = true;
        can_target = false;

        if (source_image != null)
            set_image_hidden (true);

        start_animation (0.0);
    }

    private void start_animation (double to) {
        stop_animation ();

        if (reduce_motion) {
            animation = new Adw.TimedAnimation (this, opacity, to, FADE_DURATION_MS,
                                                new Adw.PropertyAnimationTarget (this, "opacity"));
        } else {
            var params = new Adw.SpringParams (SPRING_DAMPING_RATIO, SPRING_MASS, SPRING_STIFFNESS);
            animation = new Adw.SpringAnimation (this, animation_progress, to, params,
                                                 new Adw.CallbackAnimationTarget (on_progress)) {
                clamp = true,
                epsilon = 0.0001,
                initial_velocity = -1.2,
            };
        }

        animation.done.connect (on_animation_done);
        animation.play ();
    }

    private void stop_animation () {
        if (animation == null)
            return;

        animation.done.disconnect (on_animation_done);
        animation.pause ();
        animation = null;
    }

    private void on_progress (double value) {
        animation_progress = value;
        queue_draw ();
    }

    private void on_animation_done () {
        if (closing)
            remove_from_overlay ();
        else
            set_image_hidden (false);
    }

    private void remove_from_overlay () {
        (get_parent () as Gtk.Overlay)?.remove_overlay (this);
    }

    public override void snapshot (Gtk.Snapshot snapshot) {
        if (source_image == null || animation_progress >= 1.0) {
            base.snapshot (snapshot);
            return;
        }

        float p = (float) animation_progress;
        var page = Graphene.Rect ().init (0, 0, get_width (), get_height ());

        Graphene.Rect from;
        if (!source_widget.compute_bounds (this, out from))
            from = source_bounds_at_map;

        Graphene.Rect viewport;
        if (!get_current_scrolled_window ().compute_bounds (this, out viewport))
            viewport = page;

        var rect = from.interpolate (compute_image_bounds (), p);
        float w = rect.size.width;
        float h = rect.size.height;

        snapshot.push_opacity (p);
        base.snapshot (snapshot);
        snapshot.pop ();

        snapshot.push_clip (page.interpolate (viewport, p));
        snapshot.save ();
        snapshot.translate ({ rect.origin.x, rect.origin.y });

        snapshot.push_cross_fade (p);
        source_image.snapshot (snapshot, w, h);
        snapshot.pop ();
        get_current_screenshot ().paintable.snapshot (snapshot, w, h);
        snapshot.pop ();

        snapshot.restore ();
        snapshot.pop ();
    }

    private Graphene.Rect compute_image_bounds () {
        var zoom = get_current_zoom ();

        Graphene.Rect bounds = {};
        if (zoom == null || !zoom.compute_child_bounds (this, out bounds))
            return Graphene.Rect ().init (0, 0, get_width (), get_height ());

        double ratio = ((Bz.Screenshot) zoom.child).paintable.get_intrinsic_aspect_ratio ();
        float w = bounds.size.width;
        float h = bounds.size.height;
        if (ratio <= 0.0 || h <= 0.0f)
            return bounds;

        float fit_w = float.min (w, (float) (h * ratio));
        float fit_h = float.min (h, (float) (w / ratio));

        bounds.inset ((w - fit_w) / 2.0f, (h - fit_h) / 2.0f);
        return bounds;
    }

    [GtkCallback]
    private void on_carousel_position_changed (Object object, ParamSpec pspec) {
        uint new_index = (uint) Math.round (carousel.position);
        if (new_index == _current_index || new_index >= carousel.n_pages)
            return;

        var old_zoom = get_current_zoom ();
        if (old_zoom != null) {
            old_zoom.notify["zoom-level"].disconnect (update_is_zoomed);
            old_zoom.reset ();
        }
        get_current_scrolled_window ()?.set_policy (Gtk.PolicyType.NEVER, Gtk.PolicyType.NEVER);

        _current_index = new_index;

        var new_zoom = get_current_zoom ();
        assert (new_zoom != null);
        new_zoom.notify["zoom-level"].connect (update_is_zoomed);

        update_is_zoomed ();
        notify_property ("current-index");
        notify_property ("current-caption");
    }

    private void update_is_zoomed () {
        var zoom = get_current_zoom ();
        var screenshot = zoom?.child as Bz.Screenshot;

        if (screenshot != null)
            screenshot.filter = zoom.zoom_level <= 4.5
                ? Gsk.ScalingFilter.TRILINEAR : Gsk.ScalingFilter.NEAREST;

        is_zoomed = zoom != null && zoom.is_transformed ();

        var policy = is_zoomed ? Gtk.PolicyType.AUTOMATIC : Gtk.PolicyType.NEVER;
        get_current_scrolled_window ()?.set_policy (policy, policy);
    }

    private void step (int delta) {
        int n_pages = (int) carousel.n_pages;
        if (n_pages == 0)
            return;

        int index = ((int) _current_index + delta + n_pages) % n_pages;
        carousel.scroll_to (carousel.get_nth_page (index), true);
    }

    private void copy_clicked () {
        var async_texture = get_current_screenshot ()?.paintable as Bz.AsyncTexture;
        var texture = async_texture?.dup_texture ();
        if (texture == null)
            return;

        Gdk.Display.get_default ().get_clipboard ().set_texture (texture);

        toast_overlay.add_toast (new Adw.Toast (_("Copied!")) { timeout = 1 });
    }

    private void set_image_hidden (bool hidden) {
        var screenshot = get_current_screenshot ();
        if (screenshot != null)
            screenshot.opacity = hidden ? 0.0 : 1.0;
    }

    private Gtk.ScrolledWindow? get_current_scrolled_window () {
        if (_current_index >= carousel.n_pages)
            return null;

        return carousel.get_nth_page (_current_index) as Gtk.ScrolledWindow;
    }

    private Bz.Zoom? get_current_zoom () {
        return get_current_scrolled_window ()?.child as Bz.Zoom;
    }

    private Bz.Screenshot? get_current_screenshot () {
        return get_current_zoom ()?.child as Bz.Screenshot;
    }

    private void populate_carousel () {
        if (screenshots == null)
            return;

        uint n_items = screenshots.get_n_items ();

        for (uint i = 0; i < n_items; i++) {
            var async_texture = screenshots.get_item (i) as Bz.AsyncTexture;
            if (async_texture == null)
                continue;

            var screenshot = new Bz.Screenshot () {
                paintable = async_texture,
                rounded_corners = false,
                margin_top = 25,
                margin_bottom = 25,
                margin_start = 25,
                margin_end = 25,
            };

            carousel.append (new Gtk.ScrolledWindow () {
                hexpand = true,
                vexpand = true,
                hscrollbar_policy = Gtk.PolicyType.NEVER,
                vscrollbar_policy = Gtk.PolicyType.NEVER,
                child = new Bz.Zoom () { child = screenshot },
            });
        }
    }

    [GtkCallback]
    private bool has_multiple_screenshots (ListModel? screenshots) {
        return screenshots != null && screenshots.get_n_items () > 1;
    }

    [GtkCallback]
    private bool is_valid_string (string? value) {
        return value != null && value != "";
    }

    private bool on_scroll (Gtk.EventControllerScroll controller, double dx, double dy) {
        var device = controller.get_current_event_device ();
        if (device != null && device.source == Gdk.InputSource.TOUCHPAD && Math.fabs (dy) > 2.0)
            back_clicked ();

        return Gdk.EVENT_PROPAGATE;
    }
}
