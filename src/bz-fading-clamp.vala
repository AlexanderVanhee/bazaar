/* bz-fading-clamp.vala
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

public class Bz.FadingClamp : Adw.Bin {
    private const int FADE_HEIGHT = 75;
    private const int CLAMP_LEEWAY = 100;

    private Adw.SpringAnimation animation;
    private int height_from;
    private int current_height;

    private int _max_height = 300;
    public int max_height {
        get { return _max_height; }
        set {
            if (value == _max_height)
                return;

            _max_height = value;
            height_from = current_height;
            animation.play ();
        }
    }

    public int min_max_height { get; set; default = 150; }

    public bool will_change { get; private set; }

    construct {
        layout_manager = null;
        animation = new Adw.SpringAnimation (this, 0, 1, new Adw.SpringParams (1.25, 1.0, 800.0), make_target (this));
        notify["min-max-height"].connect (queue_resize);
    }

    private static Adw.AnimationTarget make_target (Bz.FadingClamp clamp) {
        var weak_self = WeakRef (clamp);
        return new Adw.CallbackAnimationTarget (() => {
            (weak_self.get () as Gtk.Widget)?.queue_resize ();
        });
    }

    protected override Gtk.SizeRequestMode get_request_mode () {
        return Gtk.SizeRequestMode.HEIGHT_FOR_WIDTH;
    }

    protected override void measure (Gtk.Orientation orientation,
                                     int for_size,
                                     out int minimum,
                                     out int natural,
                                     out int minimum_baseline,
                                     out int natural_baseline) {
        minimum = natural = 0;
        minimum_baseline = natural_baseline = -1;

        child?.measure (orientation, for_size, out minimum, out natural, null, null);
        if (orientation == Gtk.Orientation.HORIZONTAL)
            return;

        if (for_size >= 0)
            will_change = natural > min_max_height + CLAMP_LEEWAY;

        double target = natural <= _max_height + CLAMP_LEEWAY ? natural : _max_height;
        if (animation.state == Adw.AnimationState.PLAYING)
            target = Adw.lerp (height_from, target, animation.value);

        minimum = natural = current_height = (int) Math.round (target);
    }

    protected override void size_allocate (int width, int height, int baseline) {
        int natural_height = 0;
        child?.measure (Gtk.Orientation.VERTICAL, width, null, out natural_height, null, null);

        child?.allocate (width, int.max (natural_height, height), baseline, null);
    }

    protected override void snapshot (Gtk.Snapshot snapshot) {
        int width = get_width ();
        int height = get_height ();
        if (child == null || height <= 0)
            return;

        int overflow = child.get_height () - height;
        if (overflow <= 0) {
            snapshot_child (child, snapshot);
            return;
        }

        float stop_offset = float.max (0, (float) (height - int.min (overflow, FADE_HEIGHT)) / height);
        Gsk.ColorStop[] stops = {
            { stop_offset, { 1, 1, 1, 1 } },
            { 1, { 1, 1, 1, 0 } },
        };

        snapshot.push_mask (Gsk.MaskMode.ALPHA);
        snapshot.append_linear_gradient ({{ 0, 0 }, { width, height }}, { 0, 0 }, { 0, height }, stops);
        snapshot.pop ();

        snapshot_child (child, snapshot);
        snapshot.pop ();
    }
}
