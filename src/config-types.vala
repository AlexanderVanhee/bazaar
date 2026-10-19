/* config-types.vala
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

public sealed class Bz.MainConfig : GLib.Object {
    public GLib.ListModel? txt_blocklist_paths { get; set; }
    public int blocklist_version { get; set; }
    public GLib.ListModel? yaml_blocklist_paths { get; set; }
    public int curated_config_version { get; set; }
    public GLib.ListModel? curated_config_paths { get; set; }
    public GLib.ListModel? override_eol_markings { get; set; }
    public GLib.ListModel? hooks { get; set; }
    public GLib.ListModel? search_biases { get; set; }
    public bool start_on_curated { get; set; }
    public bool hide_auto_update_options { get; set; }
}

public sealed class Bz.InternalConfig : GLib.Object {
    public GLib.ListModel? search_biases { get; set; }
}

public sealed class Bz.SearchBias : GLib.Object {
    public string? regex { get; set; }
    public string? convert_to { get; set; }
    public Bz.LinearFunction? linear_boost { get; set; }
    public Bz.ExponentialFunction? exponential_boost { get; set; }
    public GLib.ListModel? boost_appids { get; set; }
}

public sealed class Bz.LinearFunction : GLib.Object {
    public double slope { get; set; }
    public double y_intercept { get; set; }
}

public sealed class Bz.ExponentialFunction : GLib.Object {
    public double factor { get; set; }
    public double y_intercept { get; set; }
}
