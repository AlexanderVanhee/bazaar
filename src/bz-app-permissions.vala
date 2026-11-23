/* bz-app-permissions.vala
 *
 * Copyright 2026 Alexander Vanhee
 * Copyright (C) 2022 Red Hat <www.redhat.com>
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

[Compact]
public class Bz.FilesystemPath {
    public enum Type {
        SYSTEM_ROOT,
        HOME_SUBDIR,
        HOST_OS,
        HOST_ETC,
        XDG_DESKTOP,
        XDG_DOCUMENTS,
        XDG_MUSIC,
        XDG_PICTURES,
        XDG_PUBLIC_SHARE,
        XDG_VIDEOS,
        XDG_TEMPLATES,
        XDG_CACHE,
        XDG_CONFIG,
        XDG_DATA,
        XDG_RUN,
        CUSTOM,
    }

    public Type type;
    public string? subpath;

    public FilesystemPath (Type type, string? subpath) {
        this.type = type;
        this.subpath = subpath;
    }

    public string to_display_string () {
        switch (type) {
        case SYSTEM_ROOT:
            return _("System folder %s").printf (subpath ?? "/");
        case HOME_SUBDIR:
            return _("Home subfolder %s").printf (subpath);
        case HOST_OS:
            return _("Host system folders");
        case HOST_ETC:
            return _("Host system configuration from /etc");
        case XDG_DESKTOP:
            return describe (_("Desktop folder"), _("Desktop subfolder %s"));
        case XDG_DOCUMENTS:
            return describe (_("Documents folder"), _("Documents subfolder %s"));
        case XDG_MUSIC:
            return describe (_("Music folder"), _("Music subfolder %s"));
        case XDG_PICTURES:
            return describe (_("Pictures folder"), _("Pictures subfolder %s"));
        case XDG_PUBLIC_SHARE:
            return describe (_("Public Share folder"), _("Public Share subfolder %s"));
        case XDG_VIDEOS:
            return describe (_("Videos folder"), _("Videos subfolder %s"));
        case XDG_TEMPLATES:
            return describe (_("Templates folder"), _("Templates subfolder %s"));
        case XDG_CACHE:
            return describe (_("User cache folder"), _("User cache subfolder %s"));
        case XDG_CONFIG:
            return describe (_("User configuration folder"), _("User configuration subfolder %s"));
        case XDG_DATA:
            return describe (_("User data folder"), _("User data subfolder %s"));
        case XDG_RUN:
            return describe (_("User runtime folder"), _("User runtime subfolder %s"));
        case CUSTOM:
            return _("Filesystem access to %s").printf (subpath);
        default:
            return _("Unknown filesystem path");
        }
    }

    string describe (string folder, string subfolder_format) {
        return subpath != null ? subfolder_format.printf (subpath) : folder;
    }

    public unowned string to_icon_name () {
        switch (type) {
        case HOST_OS:
            return "computer-symbolic";
        case HOST_ETC:
        case XDG_CONFIG:
            return "emblem-system-symbolic";
        case XDG_DESKTOP:
            return "user-desktop-symbolic";
        case XDG_DOCUMENTS:
            return "folder-documents-symbolic";
        case XDG_MUSIC:
            return "folder-music-symbolic";
        case XDG_PICTURES:
            return "folder-pictures-symbolic";
        case XDG_PUBLIC_SHARE:
            return "folder-publicshare-symbolic";
        case XDG_VIDEOS:
            return "folder-videos-symbolic";
        default:
            return "folder-symbolic";
        }
    }

    internal static int compare (FilesystemPath a, FilesystemPath b) {
        return a.type != b.type ? (int) a.type - (int) b.type : strcmp (a.subpath, b.subpath);
    }
}

[Compact]
public class Bz.BusPolicy {
    public enum Permission {
        UNKNOWN,
        NONE,
        SEE,
        TALK,
        OWN,
    }

    public GLib.BusType bus_type;
    public string bus_name;
    public Permission permission;

    public BusPolicy (GLib.BusType bus_type, string bus_name, Permission permission) {
        this.bus_type = bus_type;
        this.bus_name = bus_name;
        this.permission = permission;
    }

    internal static int compare (BusPolicy a, BusPolicy b) {
        return a.bus_type != b.bus_type ? (int) a.bus_type - (int) b.bus_type : strcmp (a.bus_name, b.bus_name);
    }
}

public sealed class Bz.AppPermissions : GLib.Object {
    [Flags]
    public enum Flags {
        NONE = 0,
        NETWORK = 1 << 0,
        SYSTEM_BUS = 1 << 1,
        SESSION_BUS = 1 << 2,
        DEVICES = 1 << 3,
        HOME_FULL = 1 << 4,
        HOME_READ = 1 << 5,
        FILESYSTEM_FULL = 1 << 6,
        FILESYSTEM_READ = 1 << 7,
        DOWNLOADS_FULL = 1 << 8,
        DOWNLOADS_READ = 1 << 9,
        SETTINGS = 1 << 10,
        X11 = 1 << 11,
        ESCAPE_SANDBOX = 1 << 12,
        FILESYSTEM_OTHER = 1 << 13,
        BUS_POLICY_OTHER = 1 << 14,
        AUDIO_DEVICES = 1 << 15,
        INPUT_DEVICES = 1 << 16,
        SYSTEM_DEVICES = 1 << 17,
        SCREEN = 1 << 18,
        DEVEL = 1 << 19,
        BLUETOOTH = 1 << 20,
        CANBUS = 1 << 21,
        SSH_AUTH = 1 << 22,
        PRINTING = 1 << 23,
        SMART_CARDS = 1 << 24,
    }

    public Flags flags { get; private set; }
    public GLib.GenericArray<FilesystemPath> filesystem_read { get; default = new GLib.GenericArray<FilesystemPath> (); }
    public GLib.GenericArray<FilesystemPath> filesystem_full { get; default = new GLib.GenericArray<FilesystemPath> (); }
    public GLib.GenericArray<BusPolicy> bus_policies { get; default = new GLib.GenericArray<BusPolicy> (); }

    void sort () {
        filesystem_read.sort (FilesystemPath.compare);
        filesystem_full.sort (FilesystemPath.compare);
        bus_policies.sort (BusPolicy.compare);
    }

    public bool is_empty () {
        return flags == NONE && filesystem_read.length == 0 && filesystem_full.length == 0 && bus_policies.length == 0;
    }

    public AppPermissions diff (AppPermissions other) {
        var result = new AppPermissions ();

        result.flags = other.flags & ~flags;

        foreach (unowned FilesystemPath path in other.filesystem_read) {
            if (find_path (filesystem_read, path.type, path.subpath) < 0)
                result.add_filesystem_read (path.type, path.subpath);
        }
        foreach (unowned FilesystemPath path in other.filesystem_full) {
            if (find_path (filesystem_full, path.type, path.subpath) < 0)
                result.add_filesystem_full (path.type, path.subpath);
        }

        for (int i = 0, j = 0; j < other.bus_policies.length;) {
            unowned BusPolicy theirs = other.bus_policies[j];
            int comparison = i < bus_policies.length ? BusPolicy.compare (bus_policies[i], theirs) : 1;

            if (comparison < 0) {
                i++;
            } else if (comparison == 0 && theirs.permission <= bus_policies[i].permission) {
                i++;
                j++;
            } else {
                result.bus_policies.add (new BusPolicy (theirs.bus_type, theirs.bus_name, theirs.permission));
                j++;
            }
        }

        result.sort ();
        return result;
    }

    public static AppPermissions new_from_metadata (GLib.KeyFile keyfile) throws GLib.Error {
        var permissions = new AppPermissions ();
        var flags = Flags.NONE;
        string? app_id = null;

        try {
            app_id = keyfile.get_value ("Application", "name");
        } catch (GLib.KeyFileError e) {}

        string[] sockets = get_string_list (keyfile, "sockets");
        foreach (unowned string socket in sockets)
            flags |= socket_flags (socket, sockets, app_id);
        foreach (unowned string device in get_string_list (keyfile, "devices"))
            flags |= device_flags (device, app_id);
        foreach (unowned string feature in get_string_list (keyfile, "features"))
            flags |= feature_flags (feature, app_id);

        if ("network" in get_string_list (keyfile, "shared"))
            flags |= NETWORK;

        flags |= permissions.parse_filesystems (get_string_list (keyfile, "filesystems"));

        if (!(SYSTEM_BUS in flags) && !(SESSION_BUS in flags)) {
            flags |= permissions.parse_bus_policies (keyfile, GLib.BusType.SESSION, "Session Bus Policy", app_id);
            flags |= permissions.parse_bus_policies (keyfile, GLib.BusType.SYSTEM, "System Bus Policy", app_id);
        }

        permissions.flags = flags;
        permissions.sort ();

        return permissions;
    }

    public void serialize (GLib.VariantBuilder builder) {
        if (flags != NONE) {
            var flags_class = (GLib.FlagsClass) typeof (Flags).class_ref ();
            string[] nicks = {};

            foreach (unowned GLib.FlagsValue value in flags_class.values) {
                if ((flags & value.value) != 0)
                    nicks += value.value_nick;
            }
            builder.add ("{sv}", "permissions-flags", new GLib.Variant.strv (nicks));
        }

        if (filesystem_read.length > 0)
            builder.add ("{sv}", "permissions-filesystem-read", serialize_paths (filesystem_read));
        if (filesystem_full.length > 0)
            builder.add ("{sv}", "permissions-filesystem-full", serialize_paths (filesystem_full));

        if (bus_policies.length > 0) {
            var enum_class = (GLib.EnumClass) typeof (BusPolicy.Permission).class_ref ();
            var policies = new GLib.VariantBuilder (new GLib.VariantType ("a(iss)"));

            foreach (unowned BusPolicy policy in bus_policies)
                policies.add ("(iss)", (int32) policy.bus_type, policy.bus_name, enum_class.get_value (policy.permission).value_nick);
            builder.add ("{sv}", "permissions-bus-policies", policies.end ());
        }
    }

    public void deserialize (GLib.Variant data) {
        var dict = new GLib.VariantDict (data);

        var nicks = dict.lookup_value ("permissions-flags", GLib.VariantType.STRING_ARRAY);
        if (nicks != null) {
            var flags_class = (GLib.FlagsClass) typeof (Flags).class_ref ();

            flags = NONE;
            foreach (unowned string nick in nicks.get_strv ()) {
                unowned GLib.FlagsValue? flag = flags_class.get_value_by_nick (nick);
                if (flag != null)
                    flags |= flag.value;
            }
        }

        deserialize_paths (dict.lookup_value ("permissions-filesystem-read", new GLib.VariantType ("a(ss)")), filesystem_read);
        deserialize_paths (dict.lookup_value ("permissions-filesystem-full", new GLib.VariantType ("a(ss)")), filesystem_full);

        var policies = dict.lookup_value ("permissions-bus-policies", new GLib.VariantType ("a(iss)"));
        if (policies != null) {
            var enum_class = (GLib.EnumClass) typeof (BusPolicy.Permission).class_ref ();

            foreach (var policy in policies) {
                int32 bus_type;
                string bus_name, permission_nick;

                policy.get ("(iss)", out bus_type, out bus_name, out permission_nick);
                unowned GLib.EnumValue? permission = enum_class.get_value_by_nick (permission_nick);
                if (permission != null)
                    bus_policies.add (new BusPolicy ((GLib.BusType) bus_type, bus_name, (BusPolicy.Permission) permission.value));
            }
        }

        sort ();
    }

    void add_filesystem_read (FilesystemPath.Type type, string? subpath) {
        if (find_path (filesystem_read, type, subpath) < 0 && find_path (filesystem_full, type, subpath) < 0)
            filesystem_read.add (new FilesystemPath (type, subpath));
    }

    void add_filesystem_full (FilesystemPath.Type type, string? subpath) {
        if (find_path (filesystem_full, type, subpath) >= 0)
            return;

        filesystem_full.add (new FilesystemPath (type, subpath));

        int read_index = find_path (filesystem_read, type, subpath);
        if (read_index >= 0)
            filesystem_read.remove_index (read_index);
    }

    Flags parse_filesystems (string[] filesystems) {
        var flags = Flags.NONE;

        foreach (unowned string filesystem in filesystems) {
            switch (filesystem) {
            case "home":
            case "home:rw":
            case "~":
            case "~:rw":
                flags |= HOME_FULL;
                continue;
            case "home:ro":
            case "~:ro":
                flags |= HOME_READ;
                continue;
            case "host":
            case "host:rw":
            case "xdg-run/gvfsd":
                flags |= FILESYSTEM_FULL;
                continue;
            case "host:ro":
                flags |= FILESYSTEM_READ;
                continue;
            case "xdg-download":
            case "xdg-download:rw":
                flags |= DOWNLOADS_FULL;
                continue;
            case "xdg-download:ro":
                flags |= DOWNLOADS_READ;
                continue;
            case "xdg-data/flatpak/overrides:create":
                flags |= ESCAPE_SANDBOX;
                continue;
            case "xdg-run/pipewire-0":
            case "xdg-run/pipewire-0:rw":
            case "xdg-run/pipewire-0:ro":
                flags |= SCREEN | AUDIO_DEVICES;
                continue;
            case "xdg-config/kdeglobals:ro":
                continue;
            }

            flags |= FILESYSTEM_OTHER;
            if (filesystem == "")
                continue;

            bool is_readonly = filesystem.has_suffix (":ro");
            int colon = filesystem.last_index_of_char (':');
            string path = colon >= 0 ? filesystem.substring (0, colon) : filesystem;
            string? subpath = null;
            var type = path_type_for (path);

            if (type == CUSTOM) {
                subpath = path;
            } else if (type == SYSTEM_ROOT) {
                subpath = path.length > 1 ? path.substring (1) : null;
            } else {
                int slash = path.index_of_char ('/');
                if (slash > 0)
                    subpath = path.substring (slash + 1);
            }

            if (is_readonly)
                add_filesystem_read (type, subpath);
            else
                add_filesystem_full (type, subpath);
        }

        if (HOME_FULL in flags)
            flags &= ~Flags.HOME_READ;
        if (FILESYSTEM_FULL in flags)
            flags &= ~Flags.FILESYSTEM_READ;
        if (DOWNLOADS_FULL in flags)
            flags &= ~Flags.DOWNLOADS_READ;

        return flags;
    }

    Flags parse_bus_policies (GLib.KeyFile keyfile, GLib.BusType bus_type, string group, string? app_id) {
        var flags = Flags.NONE;
        var enum_class = (GLib.EnumClass) typeof (BusPolicy.Permission).class_ref ();
        string? app_id_non_devel = app_id != null && app_id.has_suffix (".Devel")
                                       ? app_id.substring (0, app_id.length - ".Devel".length)
                                       : null;
        string[] names;

        try {
            names = keyfile.get_keys (group);
        } catch (GLib.KeyFileError e) {
            return flags;
        }

        foreach (unowned string name in names) {
            unowned GLib.EnumValue? policy = null;

            try {
                policy = enum_class.get_value_by_nick (keyfile.get_string (group, name));
            } catch (GLib.KeyFileError e) {}

            if (policy == null || policy.value == BusPolicy.Permission.UNKNOWN)
                continue;

            if (bus_type == GLib.BusType.SESSION &&
                ((app_id != null && (is_own_bus_name (name, app_id) ||
                                     name == "org.freedesktop.DBus" ||
                                     name.has_prefix ("org.freedesktop.portal."))) ||
                 (app_id_non_devel != null && is_own_bus_name (name, app_id_non_devel))))
                continue;

            var known = Flags.NONE;
            if (bus_type == GLib.BusType.SESSION && policy.value >= BusPolicy.Permission.TALK) {
                if (name == "ca.desrt.dconf")
                    known = SETTINGS;
                else if (name == "org.freedesktop.Flatpak" || name == "org.freedesktop.impl.portal.PermissionStore")
                    known = ESCAPE_SANDBOX;
                else if (name.has_prefix ("org.gtk.vfs."))
                    known = FILESYSTEM_FULL;
            }

            if (known != NONE) {
                flags |= known;
            } else {
                flags |= BUS_POLICY_OTHER;
                add_bus_policy (bus_type, name, (BusPolicy.Permission) policy.value);
            }
        }

        return flags;
    }

    void add_bus_policy (GLib.BusType bus_type, string bus_name, BusPolicy.Permission permission) {
        foreach (unowned BusPolicy policy in bus_policies) {
            if (policy.bus_type == bus_type && policy.bus_name == bus_name) {
                policy.permission = (BusPolicy.Permission) int.max (policy.permission, permission);
                return;
            }
        }

        if (permission != NONE)
            bus_policies.add (new BusPolicy (bus_type, bus_name, permission));
    }

    static FilesystemPath.Type path_type_for (string path) {
        if (path.has_prefix ("/"))
            return SYSTEM_ROOT;
        if (path.has_prefix ("home/") || path.has_prefix ("~/"))
            return HOME_SUBDIR;

        var enum_class = (GLib.EnumClass) typeof (FilesystemPath.Type).class_ref ();
        for (int type = FilesystemPath.Type.HOST_OS; type <= FilesystemPath.Type.XDG_RUN; type++) {
            if (path.has_prefix (enum_class.get_value (type).value_nick))
                return (FilesystemPath.Type) type;
        }

        return CUSTOM;
    }

    static Flags socket_flags (string socket, string[] sockets, string? app_id) {
        switch (socket) {
        case "system-bus":
            return SYSTEM_BUS | ESCAPE_SANDBOX;
        case "session-bus":
            return SESSION_BUS | ESCAPE_SANDBOX;
        case "x11":
            return "fallback-x11" in sockets ? Flags.NONE : Flags.X11;
        case "fallback-x11":
            return "wayland" in sockets ? Flags.NONE : Flags.X11;
        case "wayland":
            return NONE;
        case "pulseaudio":
            return AUDIO_DEVICES;
        case "cups":
            return PRINTING;
        case "pcsc":
            return SMART_CARDS;
        case "ssh-auth":
            return SSH_AUTH;
        case "inherit-wayland-socket":
        case "gpg-agent":
            return ESCAPE_SANDBOX;
        default:
            debug ("Unrecognised Context.sockets value ‘%s’ for app %s", socket, app_id);
            return ESCAPE_SANDBOX;
        }
    }

    static Flags device_flags (string device, string? app_id) {
        switch (device) {
        case "all":
        case "usb":
            return DEVICES;
        case "input":
            return INPUT_DEVICES;
        case "shm":
        case "kvm":
            return SYSTEM_DEVICES;
        case "dri":
            return NONE;
        default:
            if (device.has_prefix ("!"))
                return NONE;
            debug ("Unrecognised Context.devices value ‘%s’ for app %s", device, app_id);
            return SYSTEM_DEVICES;
        }
    }

    static Flags feature_flags (string feature, string? app_id) {
        switch (feature) {
        case "devel":
            return DEVEL;
        case "bluetooth":
            return BLUETOOTH;
        case "canbus":
            return CANBUS;
        case "multiarch":
        case "per-app-dev-shm":
            return NONE;
        default:
            if (feature.has_prefix ("!"))
                return NONE;
            debug ("Unrecognised Context.features value ‘%s’ for app %s", feature, app_id);
            return ESCAPE_SANDBOX;
        }
    }

    static bool is_own_bus_name (string name, string app_id) {
        return name == app_id || name.has_prefix (app_id + ".") || name == "org.mpris.MediaPlayer2." + app_id;
    }

    static string[] get_string_list (GLib.KeyFile keyfile, string key) {
        try {
            return keyfile.get_string_list ("Context", key);
        } catch (GLib.KeyFileError e) {
            return {};
        }
    }

    static int find_path (GLib.GenericArray<FilesystemPath> paths, FilesystemPath.Type type, string? subpath) {
        for (int i = 0; i < paths.length; i++) {
            if (paths[i].type == type && paths[i].subpath == subpath)
                return i;
        }
        return -1;
    }

    static GLib.Variant serialize_paths (GLib.GenericArray<FilesystemPath> paths) {
        var enum_class = (GLib.EnumClass) typeof (FilesystemPath.Type).class_ref ();
        var builder = new GLib.VariantBuilder (new GLib.VariantType ("a(ss)"));

        foreach (unowned FilesystemPath path in paths)
            builder.add ("(ss)", enum_class.get_value (path.type).value_nick, path.subpath ?? "");
        return builder.end ();
    }

    static void deserialize_paths (GLib.Variant? paths, GLib.GenericArray<FilesystemPath> into) {
        if (paths == null)
            return;

        var enum_class = (GLib.EnumClass) typeof (FilesystemPath.Type).class_ref ();
        foreach (var path in paths) {
            string type_nick, subpath;

            path.get ("(ss)", out type_nick, out subpath);
            unowned GLib.EnumValue? type = enum_class.get_value_by_nick (type_nick);
            if (type != null)
                into.add (new FilesystemPath ((FilesystemPath.Type) type.value, subpath != "" ? subpath : null));
        }
    }
}
