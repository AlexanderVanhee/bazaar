/* blocklist-types.vala
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

public sealed class Bz.Blocklist : GLib.Object {
    public int priority { get; set; }
    public GLib.ListModel? conditions { get; set; }
    public GLib.ListModel? allow { get; set; }
    public GLib.ListModel? allow_regex { get; set; }
    public GLib.ListModel? block { get; set; }
    public GLib.ListModel? block_regex { get; set; }
}

public enum Bz.BlocklistConditionPostProcessKind {
    IDENTITY,
    INVERT
}

public sealed class Bz.BlocklistCondition : GLib.Object {
    public Bz.BlocklistConditionMatchEnvvar? match_envvar { get; set; }
    public Bz.BlocklistConditionMatchLocale? match_locale { get; set; }
    public Bz.BlocklistConditionPostProcessKind post_process { get; set; }
}

public sealed class Bz.BlocklistConditionMatchEnvvar : GLib.Object {
    public string? @var { get; set; }
    public string? regex { get; set; }
}

public sealed class Bz.BlocklistConditionMatchLocale : GLib.Object {
    public string? regex { get; set; }
}

public sealed class Bz.RootBlocklist : GLib.Object {
    public GLib.ListModel? blocklists { get; set; }
}
