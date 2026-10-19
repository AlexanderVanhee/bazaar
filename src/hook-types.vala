/* hook-types.vala
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

public enum Bz.HookSignal {
    VIEW_APP,
    BEFORE_TRANSACTION,
    AFTER_TRANSACTION,
    ARTICLE_APP
}

public enum Bz.HookReturnStatus {
    CONTINUE,
    STOP,
    CONFIRM,
    DENY
}

public enum Bz.HookTransactionType {
    INSTALL,
    UPDATE,
    REMOVAL
}

public sealed class Bz.Hook : GLib.Object {
    public string? id { get; set; }
    public Bz.HookSignal when { get; set; }
    public GLib.ListModel? dialogs { get; set; }
    public string? shell { get; set; }
    public string? check_appid_regex { get; set; }
}

public sealed class Bz.HookDialog : GLib.Object {
    public string? id { get; set; }
    public string? title { get; set; }
    public string? body { get; set; }
    public bool body_use_markup { get; set; }
    public GLib.ListModel? options { get; set; }
    public string? default_response_id { get; set; }
}

public sealed class Bz.HookDialogOption : GLib.Object {
    public string? id { get; set; }
    public string? @string { get; set; }
    public string? style { get; set; }
}
