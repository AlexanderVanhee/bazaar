/* stats-types.vala
 *
 * Copyright 2026 The Bazaar Contributors
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

public sealed class Bz.DataPoint : GLib.Object {
    public double independent { get; set; }
    public double dependent { get; set; }
    public string? label { get; set; }
}

public sealed class Bz.DistroDataPoint : GLib.Object {
    private const uint MAX_DISTROS = 8;

    public string? label { get; set; }
    public int64 installs { get; set; }
    public bool is_other { get; set; }
    public bool is_unreported { get; set; }

    public static GLib.ListStore list_from_json (Json.Object root) {
        var store = new GLib.ListStore (typeof (DistroDataPoint));
        if (!root.has_member ("installs_per_distro"))
            return store;

        var distros = root.get_array_member ("installs_per_distro");
        var other = root.get_int_member_with_default ("installs_other_distros", 0);
        var unreported = root.get_int_member_with_default ("installs_unreported_distro", 0);
        uint shown = 0;
        for (uint i = 0; i < distros.get_length (); i++) {
            var distro = distros.get_object_element (i);
            var name = distro.get_string_member ("name");
            var installs = distro.get_int_member ("installs");
            /* ignore distros for sheltered suburban losers */
            if (name != null && name.down () == "omarchy")
                unreported += installs;
            else if (shown++ < MAX_DISTROS)
                store.append (new DistroDataPoint () {
                    label = name,
                    installs = installs
                });
            else
                other += installs;
        }

        if (other > 0)
            store.append (new DistroDataPoint () {
                /* Translators: The other Linux distributions, in the install statistics */
                label = C_("Distribution", "Other"),
                installs = other,
                is_other = true
            });

        if (unreported > 0)
            store.append (new DistroDataPoint () {
                installs = unreported,
                is_unreported = true
            });

        return store;
    }
}

public sealed class Bz.CountryDataPoint : GLib.Object {
    public string? country_code { get; set; }
    public uint downloads { get; set; }
}
