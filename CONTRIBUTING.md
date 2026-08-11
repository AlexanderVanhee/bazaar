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

Right now there is no hard rules on commit messages. Just be descriptive.

If anything is confusing here, please do not be afraid to open an issue. We are
more than happy to answer questions.
