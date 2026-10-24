/* transaction-types.vala
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

public enum Bz.BackendNotificationKind {
    ERROR,
    TELL_INCOMING,
    REPLACE_ENTRY,
    INVALIDATE_REMOTES,
    REMOTE_SYNC_START,
    REMOTE_SYNC_FINISH,
    INSTALL_DONE,
    UPDATE_DONE,
    REMOVE_DONE,
    EXTERNAL_CHANGE
}

public sealed class Bz.BackendNotification : GLib.Object {
    public Bz.BackendNotificationKind kind { get; set; }
    public string? error { get; set; }
    public int n_incoming { get; set; }
    public Bz.Entry? entry { get; set; }
    public string? version { get; set; }
    public string? remote_name { get; set; }
    public string? generic_id { get; set; }
    public string? unique_id { get; set; }
    public bool was_rebased { get; set; }
}

public sealed class Bz.BackendTransactionOpPayload : GLib.Object {
    public string? name { get; set; }
    public Bz.Entry? entry { get; set; }
    public uint64 download_size { get; set; }
    public uint64 installed_size { get; set; }
}

public sealed class Bz.BackendTransactionOpProgressPayload : GLib.Object {
    public Bz.BackendTransactionOpPayload? op { get; set; }
    public string? status { get; set; }
    public bool is_estimating { get; set; }
    public double progress { get; set; }
    public double total_progress { get; set; }
    public uint64 bytes_transferred { get; set; }
    public uint64 start_time { get; set; }
}

public sealed class Bz.TransactionTask : GLib.Object {
    public Bz.BackendTransactionOpPayload? op { get; set; }
    public Bz.BackendTransactionOpProgressPayload? last_progress { get; set; }
    public string? error { get; set; }
}

public enum Bz.TransactionEntryKind {
    INSTALL,
    UPDATE,
    REMOVAL
}

public enum Bz.TransactionEntryStatus {
    QUEUED,
    ONGOING,
    DONE,
    CANCELLED
}

public sealed class Bz.TransactionEntryTracker : GLib.Object {
    public Bz.Entry? entry { get; set; }
    public GLib.ListModel? current_ops { get; set; }
    public GLib.ListModel? finished_ops { get; set; }
    public Bz.TransactionEntryKind kind { get; set; }
    public Bz.TransactionEntryStatus status { get; set; }
    public double progress { get; set; }
    public bool pending { get; set; }
    public bool active { get; set; }
}

public sealed class Bz.TransactionDialogResult : GLib.Object {
    public Bz.Entry? selected_entry { get; set; }
    public bool delete_user_data { get; set; }
    public bool confirmed { get; set; }
}

public sealed class Bz.BulkInstallDialogResult : GLib.Object {
    public GLib.ListModel? entries { get; set; }
    public bool confirmed { get; set; }
}

public sealed class Bz.UpdatePermissionInfo : GLib.Object {
    public Bz.Entry? entry { get; set; }
    public Bz.AppPermissions? additional_permissions { get; set; }
}

public sealed class Bz.UpdateHistoryDataPoint : GLib.Object {
    public Bz.EntryGroup? group { get; set; }
    public string? old_version { get; set; }
    public string? new_version { get; set; }
    public string? title { get; set; }
    public string? id { get; set; }
}

public sealed class Bz.FlatpakBundleResult : GLib.Object {
    public Bz.FlatpakEntry? entry { get; set; }
    public Bz.FlatpakRepo? runtime_repo { get; set; }
}

public sealed class Bz.FlatpakRepo : GLib.Object {
    public string? name { get; set; }
    public string? title { get; set; }
    public string? url { get; set; }
    public string? homepage { get; set; }
    public string? comment { get; set; }
    public string? description { get; set; }
    public string? icon { get; set; }
    public string? gpg_key { get; set; }
    public string? default_branch { get; set; }
    public string? filter { get; set; }
    public bool is_user { get; set; }
    public bool disabled { get; set; }
    public bool noenumerate { get; set; }
    public bool gpg_verify { get; set; }
}

public sealed class Bz.Repository : GLib.Object {
    public bool is_user { get; set; }
    public bool is_beta { get; set; }
    public bool noenumerate { get; set; }
    public string? title { get; set; }
    public string? url { get; set; }
    public string? name { get; set; }
}

public sealed class Bz.SizeResult : GLib.Object {
    public uint64 user_data_size { get; set; }
    public uint64 cache_size { get; set; }
}
