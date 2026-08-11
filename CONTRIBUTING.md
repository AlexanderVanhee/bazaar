# Contributing Guide

Welcome! We are glad that you are here! 💖

If you are here to contribute translations, please refer to the [Damned Lies
Module](https://l10n.gnome.org/module/bazaar/).

For code, first make sure to read the [style rules](/CODESTYLE.md).

The easiest way to run Bazaar from source is to use the
[foundry](https://gitlab.gnome.org/GNOME/foundry) cli:

```sh
foundry run
```

Alternatively, you can do:

```sh
just build-flatpak
```

Or without flatpak, if your system permits it:

```sh
meson setup build --prefix=/usr --libdir=/usr/lib64
ninja -C build
sudo ninja -C build install
bazaar
```

You will need the following dependencies installed, along with a C compiler, meson, and ninja:
| Dep Name                                                          | `pkg-config` Name | Min Version | Justification                                       |
|-------------------------------------------------------------------|-------------------|-------------|-----------------------------------------------------|
| [gtk4](https://gitlab.gnome.org/GNOME/gtk/)                       | `gtk4`            | `4.22.1`    | GUI                                                 |
| [libadwaita](https://gitlab.gnome.org/GNOME/libadwaita)           | `libadwaita-1`    | `1.8`       | GNOME styling                                       |
| [libdex](https://gitlab.gnome.org/GNOME/libdex)                   | `libdex-1`        | `1.0`       | Async helpers                                       |
| [flatpak](https://github.com/flatpak/flatpak)                     | `flatpak`         | `1.9`       | Flatpak installation management                     |
| [appstream](https://github.com/ximion/appstream)                  | `appstream`       | `1.0`       | Interpret application metadata                      |
| [xmlb](https://github.com/hughsie/libxmlb)                        | `xmlb`            | `0.3.4`     | Handle binary xml appstream bundles/Parse plain xml |
| [glycin](https://gitlab.gnome.org/GNOME/glycin)                   | `glycin-2`        | `2.0`       | Decode image URIs                                   |
| [glycin-gtk4](https://gitlab.gnome.org/GNOME/glycin)              | `glycin-gtk4-2`   | `2.0`       | Convert glycin frames to texture representations    |
| [libyaml](https://github.com/yaml/libyaml)                        | `yaml-0.1`        | `0.2.5`     | Parse YAML configs                                  |
| [libsoup](https://gitlab.gnome.org/GNOME/libsoup)                 | `libsoup-3.0`     | `3.6.0`     | HTTP operations                                     |
| [json-glib](https://gitlab.gnome.org/GNOME/json-glib)             | `json-glib-1.0`   | `1.10.0`    | Parse some HTTP replies                             |
| [md4c](https://github.com/mity/md4c)                              | `md4c`            | `0.5.1`     | Parse markdown (.md)                                |
| [gtksourceview](https://gitlab.gnome.org/GNOME/gtksourceview)     | `gtksourceview-5` | `5.17`      | Render markdown code blocks                         |
| [webkitgtk](https://webkitgtk.org/)                               | `webkitgtk-6.0`   | `2.50.2`    | Render web views                                    |
| [libsecret](https://gitlab.gnome.org/GNOME/libsecret)             | `libsecret-1`     | `0.20`      | Store Flathub account information                   |
| [libproxy](https://github.com/libproxy/libproxy)                  | `libproxy-1.0`    | `0.5`       | Parse proxies for networking operations             |
| [malcontent](https://gitlab.freedesktop.org/pwithnall/malcontent) | `malcontent-0`    | `0.12.0`    | Adhere to system parental controls settings         |
| [libsystemd](https://github.com/systemd/systemd)                  | `libsystemd'`     | `245`       | Utilities for bazaar-daemon                         |

There is a [script](/scripts/install-deps/fedora-rawhide.sh) for installing all
of these on fedora.

Right now there is no hard rules on commit messages. Just be descriptive.

If anything is confusing here, please do not be afraid to open an issue. We are
more than happy to answer questions.
