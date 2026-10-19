/* misc-types.vala
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

public sealed class Bz.AgeRatingAttribute : GLib.Object {
    public Bz.Importance importance { get; set; }
    public string? id { get; set; }
    public string? title { get; set; }
    public string? description { get; set; }
    public string? icon_name { get; set; }
}

public sealed class Bz.SafetyRow : GLib.Object {
    public Bz.Importance importance { get; set; }
    public string? title { get; set; }
    public string? subtitle { get; set; }
    public string? icon_name { get; set; }
}

public sealed class Bz.FinishedSearchQuery : GLib.Object {
    public string? interpreted_query { get; set; }
    public GLib.GenericArray<Bz.SearchResult>? results { get; set; }
    public uint n_results { get; set; }
    public double elapsed { get; set; }
}

public sealed class Bz.SearchResult : GLib.Object {
    public Bz.EntryGroup? group { get; set; }
    public uint original_index { get; set; }
    public double score { get; set; }
    public string? title_markup { get; set; }
    public Bz.StateInfo? state { get; set; }
}

public sealed class Bz.FlathubAuthProvider : GLib.Object {
    public string? method { get; set; }
    public string? name { get; set; }
}

public sealed class Bz.HashTableObject : GLib.Object {
    public GLib.HashTable<string, void*>? hash_table { get; set; }
}

public sealed class Bz.Release : GLib.Object {
    public string? description { get; set; }
    public uint64 timestamp { get; set; }
    public string? url { get; set; }
    public string? version { get; set; }
}

public sealed class Bz.Url : GLib.Object {
    public string? id { get; set; }
    public string? url { get; set; }
}

public sealed class Bz.VerificationStatus : GLib.Object {
    public bool verified { get; set; }
    public string? method { get; set; }
    public string? website { get; set; }
    public string? login_name { get; set; }
    public string? login_provider { get; set; }
    public string? timestamp { get; set; }
    public bool login_is_organization { get; set; }
}
