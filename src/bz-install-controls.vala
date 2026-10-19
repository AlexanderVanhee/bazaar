/*
 * bz-install-controls.vala
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

[GtkTemplate (ui = "/io/github/kolunmi/Bazaar/bz-install-controls.ui")]
public class Bz.InstallControls : Gtk.Widget {
    private const float RING_WIDTH = 5.0f;
    private const float RING_PADDING = 4.0f;

    private enum InstallState {
        INACTIVE,
        PENDING,
        FRACTION,
        CANCELLING,
    }

    public bool wide { get; set; default = true; }

    private Bz.EntryGroup? _entry_group;
    public Bz.EntryGroup? entry_group {
        get { return _entry_group; }
        set {
            _entry_group = value;
            update_install_label ();
            update_tracker ();
        }
    }

    public Bz.StateInfo state {
        get { return Bz.StateInfo.get_default (); }
    }

    private ListModel all_trackers;

    private Bz.TransactionEntryTracker? _tracker;
    public Bz.TransactionEntryTracker? tracker {
        get { return _tracker; }
        private set {
            if (_tracker == value)
                return;
            if (_tracker != null)
                _tracker.notify.disconnect (update_install_state);
            _tracker = value;
            if (_tracker != null)
                _tracker.notify.connect (update_install_state);
            update_install_state ();
        }
    }

    public bool in_progress { get; private set; }

    public signal void update (ListModel entries);

    [GtkChild]
    private unowned Gtk.Stack stack;
    [GtkChild]
    private unowned Gtk.Button install_button;
    [GtkChild]
    private unowned Gtk.Stack install_label_stack;
    [GtkChild]
    private unowned Gtk.Widget pending_ring;
    [GtkChild]
    private unowned Gtk.Widget progress_ring;

    private InstallState install_state = InstallState.INACTIVE;
    private string? pride_class;

    private Graphene.Rect ring_rect;
    private double progress_eased;
    private int64 start_time;
    private uint tick_id;

    private Adw.SpringAnimation progress_anim;
    private Adw.SpringAnimation pending_fade;
    private Adw.SpringAnimation progress_fade;

    construct {
        progress_anim = new Adw.SpringAnimation (this, 0.0, 0.0, new Adw.SpringParams (1.1, 0.1, 10.0),
                                                 progress_anim_target (this));
        pending_fade = new Adw.SpringAnimation (this, 0.0, 0.0, new Adw.SpringParams (1.0, 0.1, 10.0),
                                                new Adw.PropertyAnimationTarget (pending_ring, "opacity"));
        progress_fade = new Adw.SpringAnimation (this, 0.0, 0.0, new Adw.SpringParams (1.0, 0.1, 10.0),
                                                 new Adw.PropertyAnimationTarget (progress_ring, "opacity"));

        stack.notify["visible-child"].connect (queue_allocate);

        all_trackers = state.transaction_manager.all_trackers;
        all_trackers.items_changed.connect (update_tracker);
        state.settings.changed["global-progress-bar-theme"].connect (update_pride_class);
        update_pride_class ();
    }

    private static bool redraw_tick (Gtk.Widget widget, Gdk.FrameClock clock) {
        widget.queue_draw ();
        return Source.CONTINUE;
    }

    private static Adw.AnimationTarget progress_anim_target (Bz.InstallControls self) {
        unowned var controls = self;
        return new Adw.CallbackAnimationTarget ((value) => {
            controls.progress_eased = value;
            controls.queue_draw ();
        });
    }

    protected override void dispose () {
        dispose_template (typeof (Bz.InstallControls));

        base.dispose ();
    }

    public void focus_button () {
        Idle.add_full (Priority.DEFAULT_IDLE, () => {
            stack.visible_child.child_focus (Gtk.DirectionType.TAB_FORWARD);
            return Source.REMOVE;
        });
    }

    protected override void measure (Gtk.Orientation orientation, int for_size,
                                     out int minimum, out int natural,
                                     out int minimum_baseline, out int natural_baseline) {
        stack.measure (orientation, for_size, out minimum, out natural, out minimum_baseline, out natural_baseline);
    }

    protected override void size_allocate (int width, int height, int baseline) {
        stack.allocate (width, height, baseline, null);

        Graphene.Rect bounds;
        if (!install_button.compute_bounds (this, out bounds))
            bounds = {};
        ring_rect = bounds.inset_r (-RING_PADDING, -RING_PADDING);

        foreach (var ring in new Gtk.Widget[] { pending_ring, progress_ring }) {
            Gtk.Requisition min, nat;
            ring.get_preferred_size (out min, out nat);
            ring.allocate ((int) ring_rect.size.width, (int) ring_rect.size.height, -1,
                           new Gsk.Transform ().translate (ring_rect.origin));
        }
    }

    protected override void snapshot (Gtk.Snapshot snapshot) {
        snapshot_child (stack, snapshot);

        if (!install_button.should_layout ())
            return;

        double elapsed = (get_monotonic_time () - start_time) / (double) TimeSpan.SECOND;
        draw_ring (snapshot, pending_ring, elapsed / 3.0, 0.2);
        draw_ring (snapshot, progress_ring, 0.0, 0.05 + progress_eased.clamp (0.0, 1.0) * 0.95);
    }

    private void draw_ring (Gtk.Snapshot snapshot, Gtk.Widget ring, double start, double span) {
        if (ring.opacity <= 0.0)
            return;

        Gsk.RoundedRect outline = {};
        outline.init_from_rect (ring_rect, ring_rect.size.height / 2.0f);
        var builder = new Gsk.PathBuilder ();
        builder.add_rounded_rect (outline);
        var path = builder.to_path ();

        float length = new Gsk.PathMeasure (path).get_length ();
        float dash = (float) span * length;
        float origin = ring_rect.size.width / 2.0f - ring_rect.size.height / 2.0f;

        var stroke = new Gsk.Stroke (RING_WIDTH);
        stroke.set_line_cap (Gsk.LineCap.BUTT);
        if (dash < length) {
            stroke.set_dash ({ dash, length - dash });
            stroke.set_dash_offset (length - (float) Math.fmod (origin + start * length, length));
        }

        snapshot.push_stroke (path, stroke);
        snapshot_child (ring, snapshot);
        snapshot.pop ();
    }

    private void update_tracker () {
        string? group_id = _entry_group?.id;
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

    private void update_install_state () {
        var progress = _tracker?.progress ?? 0.0;
        if (progress != progress_anim.value_to)
            animate (progress_anim, progress_eased, progress);

        var new_state = compute_install_state ();
        if (new_state == install_state)
            return;

        install_state = new_state;
        in_progress = new_state != InstallState.INACTIVE;

        install_label_stack.visible_child_name = new_state == InstallState.INACTIVE ? "install"
                                               : new_state == InstallState.CANCELLING ? "cancelling"
                                               : "cancel";
        install_button.sensitive = new_state != InstallState.CANCELLING;
        if (new_state == InstallState.INACTIVE)
            install_button.add_css_class ("suggested-action");
        else
            install_button.remove_css_class ("suggested-action");

        retarget_rings ();
    }

    private InstallState compute_install_state () {
        if (_tracker == null)
            return InstallState.INACTIVE;
        if ((_tracker.active || _tracker.pending) && _tracker.status == Bz.TransactionEntryStatus.CANCELLED)
            return InstallState.CANCELLING;
        if (_tracker.pending || _tracker.status == Bz.TransactionEntryStatus.QUEUED)
            return InstallState.PENDING;
        if (!_tracker.active || _tracker.status == Bz.TransactionEntryStatus.DONE)
            return InstallState.INACTIVE;
        return InstallState.FRACTION;
    }

    private void retarget_rings () {
        bool pending = install_state == InstallState.PENDING;
        bool fraction = install_state == InstallState.FRACTION;

        if ((pending || fraction) && pending_ring.opacity <= 0.0 && progress_ring.opacity <= 0.0) {
            animate (progress_anim, 0.0, progress_anim.value_to);
            start_time = get_monotonic_time ();
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
        animation.spring_params = new Adw.SpringParams (1.0, 0.1, shown ? 10.0 : 100.0);
        animate (animation, ring.opacity, shown ? 1.0 : 0.0);
    }

    private static void animate (Adw.SpringAnimation animation, double from, double to) {
        animation.value_from = from;
        animation.value_to = to;
        animation.play ();
    }

    private void update_install_label () {
        string? title = _entry_group?.title;
        string? developer = _entry_group?.developer;

        if (title == null)
            return;

        var label = developer != null ? _("Install %s from %s").printf (title, developer) : _("Install %s").printf (title);
        install_button.update_property (Gtk.AccessibleProperty.LABEL, label, -1);
    }

    private void update_pride_class () {
        var id = state.settings.get_string ("global-progress-bar-theme");
        var css_class = id == "accent-color" ? @"$id-theme" : @"$id-horizontal-theme";

        foreach (var ring in new Gtk.Widget[] { pending_ring, progress_ring }) {
            if (pride_class != null)
                ring.remove_css_class (pride_class);
            ring.add_css_class (css_class);
        }
        pride_class = css_class;
    }

    private ListStore? find_matching_updates (ListModel? updates) {
        string? group_id = _entry_group?.id;
        var store = new ListStore (typeof (Bz.Entry));
        uint n_items = updates?.get_n_items () ?? 0;

        for (uint i = 0; i < n_items && group_id != null; i++) {
            var entry = ((Bz.UpdatePermissionInfo) updates.get_item (i)).entry;
            if (entry?.id == group_id)
                store.append (entry);
        }

        return store.get_n_items () > 0 ? store : null;
    }

    [GtkCallback]
    private string get_visible_page (int installable, int removable, ListModel? updates, bool in_progress) {
        if (in_progress)
            return "install";

        if (removable <= 0)
            return installable > 0 ? "install" : "empty";

        var update_signal = Signal.lookup ("update", typeof (Bz.InstallControls));
        if (!Signal.has_handler_pending (this, update_signal, 0, false))
            return "open";

        return find_matching_updates (updates) != null ? "update" : "open";
    }

    [GtkCallback]
    private bool is_not_empty_page (string? page) {
        return page != null && page != "empty";
    }

    [GtkCallback]
    private bool is_not_blocked (ListModel? blocked, Bz.EntryGroup? group) {
        uint n_items = blocked?.get_n_items () ?? 0;

        for (uint i = 0; i < n_items && group?.id != null; i++) {
            if (((Gtk.StringObject) blocked.get_item (i)).string.contains (group.id))
                return false;
        }
        return true;
    }

    [GtkCallback]
    private Gtk.Align page_halign (bool wide) {
        return wide ? Gtk.Align.END : Gtk.Align.FILL;
    }

    [GtkCallback]
    private int choose (bool wide, int wide_value, int narrow_value) {
        return wide ? wide_value : narrow_value;
    }

    [GtkCallback]
    private void on_install_clicked () {
        if (_entry_group == null)
            return;

        if (install_state == InstallState.INACTIVE)
            activate_action ("window.install-group", "(sb)", _entry_group.id, true);
        else
            activate_action ("window.cancel-group", "s", _entry_group.id);
    }

    [GtkCallback]
    private void on_remove_clicked () {
        if (_entry_group != null)
            activate_action ("window.remove-group", "(sb)", _entry_group.id, true);
    }

    [GtkCallback]
    private void on_open_clicked () {
        if (_entry_group != null)
            activate_action ("window.launch-group", "s", _entry_group.id);
    }

    [GtkCallback]
    private void on_update_clicked () {
        var store = find_matching_updates (state.available_updates);
        if (store != null)
            update (store);
    }
}
