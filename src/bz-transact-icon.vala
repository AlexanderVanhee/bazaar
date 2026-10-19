/*
 * bz-transact-icon.vala
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

public class Bz.TransactIcon : Gtk.Widget {
    private const float RING_WIDTH = 3.0f;

    private Bz.EntryGroup? _group;
    public Bz.EntryGroup? group {
        get { return _group; }
        set {
            _group = value;
            update_tracker ();
        }
    }

    public Gdk.Paintable? paintable {
        owned get { return icon.paintable; }
        set {
            if (value != null)
                icon.paintable = value;
            else
                icon.icon_name = "application-x-executable";
        }
    }

    private Bz.TransactionEntryTracker? _tracker;
    private Bz.TransactionEntryTracker? tracker {
        set {
            if (_tracker == value)
                return;
            if (_tracker != null)
                _tracker.notify.disconnect (update_rings);
            _tracker = value;
            if (_tracker != null)
                _tracker.notify.connect (update_rings);
            update_rings ();
        }
    }

    private Gtk.Image icon;
    private Gtk.Widget? pending_ring;
    private Gtk.Widget? progress_ring;

    private ListModel all_trackers;
    private string? pride_class;

    private double progress_eased;
    private int64 start_time;
    private uint tick_id;

    private Adw.SpringAnimation? progress_anim;
    private Adw.SpringAnimation? pending_fade;
    private Adw.SpringAnimation? progress_fade;

    static construct {
        set_layout_manager_type (typeof (Gtk.BinLayout));
    }

    construct {
        icon = new Gtk.Image.from_icon_name ("application-x-executable");
        icon.add_css_class ("icon-dropshadow");
        icon.set_parent (this);
        bind_property ("height-request", icon, "pixel-size", BindingFlags.SYNC_CREATE);

        all_trackers = Bz.StateInfo.get_default ().transaction_manager.all_trackers;
        all_trackers.items_changed.connect (update_tracker);
    }

    private void ensure_rings () {
        if (pending_ring != null)
            return;

        pending_ring = new_ring ();
        progress_ring = new_ring ();

        progress_anim = new Adw.SpringAnimation (this, 0.0, 0.0, new Adw.SpringParams (1.1, 0.1, 10.0),
                                                 progress_anim_target (this));
        pending_fade = new Adw.SpringAnimation (this, 0.0, 0.0, new Adw.SpringParams (1.0, 0.1, 5.0),
                                                new Adw.PropertyAnimationTarget (pending_ring, "opacity"));
        progress_fade = new Adw.SpringAnimation (this, 0.0, 0.0, new Adw.SpringParams (1.0, 0.1, 5.0),
                                                 new Adw.PropertyAnimationTarget (progress_ring, "opacity"));

        Bz.StateInfo.get_default ().settings.changed["global-progress-bar-theme"].connect (update_pride_class);
        update_pride_class ();
    }

    private Gtk.Widget new_ring () {
        var ring = new Adw.Bin () {
            can_target = false,
            opacity = 0.0,
        };
        ring.add_css_class ("transact-icon-flag");
        ring.set_parent (this);
        return ring;
    }

    private static bool redraw_tick (Gtk.Widget widget, Gdk.FrameClock clock) {
        widget.queue_draw ();
        return Source.CONTINUE;
    }

    private static Adw.AnimationTarget progress_anim_target (Bz.TransactIcon self) {
        unowned var transact_icon = self;
        return new Adw.CallbackAnimationTarget ((value) => {
            transact_icon.progress_eased = value;
            transact_icon.queue_draw ();
        });
    }

    protected override void dispose () {
        for (var child = get_first_child (); child != null; child = get_first_child ())
            child.unparent ();

        base.dispose ();
    }

    protected override void snapshot (Gtk.Snapshot snapshot) {
        if (pending_ring == null) {
            snapshot_child (icon, snapshot);
            return;
        }

        float width = get_width ();
        float height = get_height ();
        float icon_scale = 1.0f - 0.4f * (float) (pending_ring.opacity + progress_ring.opacity).clamp (0.0, 1.0);

        snapshot.save ();
        snapshot.translate ({ width / 2.0f, height / 2.0f });
        snapshot.scale (icon_scale, icon_scale);
        snapshot.translate ({ -width / 2.0f, -height / 2.0f });
        snapshot_child (icon, snapshot);
        snapshot.restore ();

        double elapsed = (get_monotonic_time () - start_time) / (double) TimeSpan.SECOND;
        draw_ring (snapshot, pending_ring, elapsed / 2.0, 0.2);
        draw_ring (snapshot, progress_ring, 0.0, 0.05 + progress_eased.clamp (0.0, 1.0) * 0.95);
    }

    private void draw_ring (Gtk.Snapshot snapshot, Gtk.Widget ring, double start, double span) {
        if (ring.opacity <= 0.0)
            return;

        float radius = float.min (get_width (), get_height ()) / 2.0f - RING_WIDTH / 2.0f;
        var builder = new Gsk.PathBuilder ();
        builder.add_circle ({ get_width () / 2.0f, get_height () / 2.0f }, radius);

        float length = 2.0f * (float) Math.PI * radius;
        float dash = (float) span * length;

        var stroke = new Gsk.Stroke (RING_WIDTH);
        stroke.set_line_cap (Gsk.LineCap.ROUND);
        if (dash < length) {
            stroke.set_dash ({ dash, length - dash });
            stroke.set_dash_offset (length - (float) Math.fmod ((start + 0.75) * length, length));
        }

        snapshot.push_stroke (builder.to_path (), stroke);
        snapshot_child (ring, snapshot);
        snapshot.pop ();
    }

    private void update_tracker () {
        string? group_id = _group?.id;
        uint n_items = all_trackers.get_n_items ();

        for (uint i = 0; i < n_items && group_id != null; i++) {
            var candidate = (Bz.TransactionEntryTracker) all_trackers.get_item (i);
            if (candidate.entry?.id == group_id) {
                tracker = candidate;
                return;
            }
        }
        tracker = null;
    }

    private void update_rings () {
        if (_tracker == null && pending_ring == null)
            return;
        ensure_rings ();

        bool pending = false;
        bool fraction = false;

        if (_tracker != null && _tracker.status != Bz.TransactionEntryStatus.CANCELLED) {
            pending = _tracker.pending || _tracker.status == Bz.TransactionEntryStatus.QUEUED;
            fraction = !pending && _tracker.active && _tracker.status != Bz.TransactionEntryStatus.DONE;
        }

        var progress = _tracker?.progress ?? 0.0;
        if ((pending || fraction) && pending_ring.opacity <= 0.0 && progress_ring.opacity <= 0.0) {
            animate (progress_anim, 0.0, progress);
            start_time = get_monotonic_time ();
        } else if (progress != progress_anim.value_to) {
            animate (progress_anim, progress_eased, progress);
        }

        fade (pending_fade, pending_ring, pending);
        fade (progress_fade, progress_ring, fraction);

        if (pending && tick_id == 0) {
            tick_id = add_tick_callback (redraw_tick);
        } else if (!pending && tick_id != 0) {
            remove_tick_callback (tick_id);
            tick_id = 0;
        }
    }

    private static void fade (Adw.SpringAnimation animation, Gtk.Widget ring, bool shown) {
        double to = shown ? 1.0 : 0.0;
        if (to != animation.value_to)
            animate (animation, ring.opacity, to);
    }

    private static void animate (Adw.SpringAnimation animation, double from, double to) {
        animation.value_from = from;
        animation.value_to = to;
        animation.play ();
    }

    private void update_pride_class () {
        var css_class = Bz.StateInfo.get_default ().settings.get_string ("global-progress-bar-theme") + "-theme";

        foreach (var ring in new Gtk.Widget[] { pending_ring, progress_ring }) {
            if (pride_class != null)
                ring.remove_css_class (pride_class);
            ring.add_css_class (css_class);
        }
        pride_class = css_class;
    }
}
