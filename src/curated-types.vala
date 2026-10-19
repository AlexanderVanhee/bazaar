/* curated-types.vala
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

public sealed class Bz.CuratedAppidsInfo : GLib.Object {
    public GLib.ListModel? list { get; set; }
}

public sealed class Bz.CuratedArticle : GLib.Object {
    public string? title { get; set; }
    public string? subtitle { get; set; }
    public string? image { get; set; }
    public string? uri { get; set; }
}

public sealed class Bz.CuratedArticlesInfo : GLib.Object {
    public GLib.ListModel? list { get; set; }
    public string? uri { get; set; }
    public float aspect_ratio { get; set; }
}

public sealed class Bz.CuratedBanner : GLib.Object {
    public Bz.CuratedImageInfo? image { get; set; }
    public string? light_color { get; set; }
    public string? dark_color { get; set; }
    public int height { get; set; }
}

public sealed class Bz.CuratedFeaturedCarousel : GLib.Object {
    public Bz.CuratedAppidsInfo? appids { get; set; }
    public bool disable_auto_rotate { get; set; }
}

public sealed class Bz.CuratedImageInfo : GLib.Object {
    public string? uri { get; set; }
    public string? alt { get; set; }
    public string? light_uri { get; set; }
    public string? dark_uri { get; set; }
    public bool can_shrink { get; set; }
    public Gtk.ContentFit fit { get; set; }
    public Gtk.Align halign { get; set; }
}

public sealed class Bz.CuratedMarkdownInfo : GLib.Object {
    public string? @string { get; set; }
}

public sealed class Bz.CuratedRow : GLib.Object {
    public Bz.CuratedSection? section { get; set; }
    public Bz.CuratedFeaturedCarousel? featured_carousel { get; set; }
    public Bz.CuratedBanner? banner { get; set; }
    public Bz.CuratedArticlesInfo? articles { get; set; }
}

public sealed class Bz.CuratedSection : GLib.Object {
    public string? title { get; set; }
    public Bz.CuratedMarkdownInfo? subtitle { get; set; }
    public Bz.CuratedAppidsInfo? appids { get; set; }
    public string? list_type { get; set; }
    public bool enable_bulk_install { get; set; }
    public int overflow_count { get; set; }
}

public sealed class Bz.RootCuratedConfig : GLib.Object {
    public GLib.ListModel? templates { get; set; }
    public GLib.ListModel? rows { get; set; }
}

public sealed class Bz.FlathubCuratedSelection : GLib.Object {
    public string? theme_key { get; set; }
    public string? slot { get; set; }
    public Gtk.StringList? apps { get; set; }
    public Bz.ApplicationMapFactory? map_factory { get; set; }
}

public sealed class Bz.FlathubSubCategory : GLib.Object {
    public string? name { get; set; }
    public string? id { get; set; }
}
