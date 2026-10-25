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
    private Adw.TimedAnimation width_animation;
    private Adw.TimedAnimation height_animation;
    private bool sized;

    construct {
        width_animation = new Adw.TimedAnimation (this, 0, 0, 0, new Adw.PropertyAnimationTarget (this, "content-width"));
        height_animation = new Adw.TimedAnimation (this, 0, 0, 0, new Adw.PropertyAnimationTarget (this, "content-height"));
    }

    public void resize_to (int width, int height) {
        if (!sized) {
            sized = true;
            content_width = width;
            content_height = height;
            return;
        }

        int delta = int.max ((width - content_width).abs (), (height - content_height).abs ());
        bool shrinking = width < content_width || height < content_height;
        uint duration = (uint) (delta * 0.6).clamp (200, shrinking ? 300 : 600);

        animate (width_animation, content_width, width, duration);
        animate (height_animation, content_height, height, duration);
    }

    private static void animate (Adw.TimedAnimation animation, int from, int to, uint duration) {
        animation.value_from = from;
        animation.value_to = to;
        animation.duration = duration;
        animation.play ();
    }

    public static int measure_height (Gtk.Widget widget, int for_width, int padding, int min, int max) {
        int nat;

        widget.measure (Gtk.Orientation.VERTICAL, for_width, null, out nat, null, null);
        return (nat + padding).clamp (min, max);
    }
}
