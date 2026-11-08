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
    public GLib.ListModel? model { get; set; }
    public GLib.ListModel? country_model { get; set; }
    public int total_downloads { get; set; }

    public StatsDialog (GLib.ListModel? model, GLib.ListModel? country_model, int total_downloads) {
        Object (model: model, country_model: country_model, total_downloads: total_downloads);
    }

    class construct {
        typeof (Bz.DataGraph).ensure ();
        typeof (Bz.WorldMap).ensure ();
    }

    [GtkCallback]
    private bool has_country_model (GLib.ListModel? countries) {
        return countries != null;
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
}
