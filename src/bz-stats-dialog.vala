/* bz-stats-dialog.vala
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

[GtkTemplate (ui = "/io/github/kolunmi/Bazaar/bz-stats-dialog.ui")]
public sealed class Bz.StatsDialog : Adw.BreakpointBin {
    private const int DISTROS_DIALOG_WIDTH = 700;
    private const int DIALOG_HEADER_HEIGHT = 47;
    private const int DISTROS_DIALOG_MIN_HEIGHT = 300;
    private const int DISTROS_DIALOG_MAX_HEIGHT = 750;

    [GtkChild]
    private unowned Adw.ViewStack stack;
    [GtkChild]
    private unowned Gtk.ScrolledWindow distros_scroll;

    private int previous_width = -1;
    private int previous_height = -1;

    public GLib.ListModel? model { get; set; }
    public GLib.ListModel? country_model { get; set; }
    public GLib.ListModel? distro_model { get; set; }
    public int total_downloads { get; set; }

    public StatsDialog () {}

    class construct {
        typeof (Bz.DataGraph).ensure ();
        typeof (Bz.WorldMap).ensure ();
        typeof (DistroChart).ensure ();
    }

    construct {
        stack.notify["visible-child-name"].connect (on_visible_page_changed);
    }

    private void on_visible_page_changed () {
        var dialog = get_ancestor (typeof (Dialog)) as Dialog;
        if (dialog == null)
            return;

        if (stack.visible_child_name == "distros") {
            previous_width = dialog.content_width;
            previous_height = dialog.content_height;
            dialog.resize_to (
                DISTROS_DIALOG_WIDTH,
                Dialog.measure_height (distros_scroll, DISTROS_DIALOG_WIDTH, DIALOG_HEADER_HEIGHT, DISTROS_DIALOG_MIN_HEIGHT, DISTROS_DIALOG_MAX_HEIGHT)
            );
        } else if (previous_width > 0) {
            dialog.resize_to (previous_width, previous_height);
            previous_width = -1;
        }
    }

    [GtkCallback]
    private bool model_has_items (GLib.ListModel? points) {
        return points != null && points.get_n_items () > 0;
    }

    [GtkCallback]
    private string format_installs (int value) {
        return value > 0 ? "%'d".printf (value) : "---";
    }

    [GtkCallback]
    private string format_last_week (GLib.ListModel? points) {
        if (points == null)
            return "---";

        double sum = 0;
        uint n = points.get_n_items ();
        for (uint i = n > 7 ? n - 7 : 0; i < n; i++)
            sum += ((Bz.DataPoint) points.get_item (i)).dependent;
        return "%'.0f".printf (sum);
    }

    [GtkCallback]
    private string format_distros_caption (GLib.ListModel? points) {
        int64 total = 0;
        int64 not_reported = 0;
        for (uint i = 0; points != null && i < points.get_n_items (); i++) {
            var point = (DistroDataPoint) points.get_item (i);
            total += point.installs;
            if (point.is_unreported)
                not_reported = point.installs;
        }

        var unreported = total > 0 ? (double) not_reported / total * 100 : 0;
        if (Math.round (unreported) <= 0)
            return _("Last 30 days");
        /* Translators: Shown below the install statistics, for example: Last 30 days, 42% of installs don't yet report a distro */
        return _("Last 30 days, %.0f%% of installs don't yet report a distro").printf (unreported);
    }
}
