/* bz-runtime-updates-entry.vala
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

public class Bz.RuntimeUpdatesEntry : Bz.Entry {
    public RuntimeUpdatesEntry (uint n_updates) {
        var icon = Gtk.IconTheme.get_for_display (Gdk.Display.get_default ()).lookup_icon (
            "system-component-runtime", null, 64, 1,
            Gtk.Widget.get_default_direction (), 0);

        this.set (
            "id", "runtime-updates",
            "unique-id", "runtime-updates",
            "title", ngettext ("%u Runtime Update", "%u Runtime Updates", n_updates).printf (n_updates),
            "kinds", Bz.EntryKind.RUNTIME,
            "icon-paintable", icon);
    }
}
