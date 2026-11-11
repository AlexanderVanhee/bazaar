/* bz-dialog.vala
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

public class Bz.Dialog : Adw.Dialog {
    private Adw.SpringAnimation width_animation;
    private Adw.SpringAnimation height_animation;
    private bool sized;
    private int target_width;
    private int target_height;

    construct {
        width_animation = new Adw.SpringAnimation (this, 0, 0, new Adw.SpringParams (1, 1, 400), new Adw.PropertyAnimationTarget (this, "content-width"));
        height_animation = new Adw.SpringAnimation (this, 0, 0, new Adw.SpringParams (1, 1, 400), new Adw.PropertyAnimationTarget (this, "content-height"));
        width_animation.done.connect (() => content_width = target_width);
        height_animation.done.connect (() => content_height = target_height);
    }

    public void resize_to (int width, int height) {
        if (!sized) {
            sized = true;
            content_width = width;
            content_height = height;
            return;
        }

        target_width = width;
        target_height = height;

        animate (width_animation, content_width, width, get_width (), 30);
        animate (height_animation, content_height, height, get_height (), 20);
    }

    public void resize_to_instant (int width, int height) {
        width_animation.reset ();
        height_animation.reset ();

        sized = true;
        content_width = width;
        content_height = height;
    }

    private void animate (Adw.SpringAnimation animation, int from, int to, int available, int min_padding) {
        animation.value_from = clamp_to_sheet (from, available, min_padding);
        animation.value_to = clamp_to_sheet (to, available, min_padding);
        animation.epsilon = 0.000001;
        animation.play ();
    }

    private int clamp_to_sheet (int size, int available, int min_padding) {
        if (available == 0 || !has_css_class ("floating"))
            return size;

        double t = double.max (0, (available - 720) / 720.0);
        return int.min (size, available - 2 * (int) (min_padding + (120 - min_padding) * t));
    }

    public static int measure_height (Gtk.Widget widget, int for_width, int padding, int min, int max) {
        int nat;

        widget.measure (Gtk.Orientation.VERTICAL, for_width, null, out nat, null, null);
        return (nat + padding).clamp (min, max);
    }
}
