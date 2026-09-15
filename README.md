# EDID Editor

EDID Editor inspects and edits EDID, the data a display sends to describe itself: its
name, size, timings, refresh ranges, color and HDR support, and audio formats. It is
written in C++ with GTK 4 and libadwaita, and is based on
[wxEDID](https://sourceforge.net/projects/wxedid/) by Tomasz Pawlak.

It reads the EDID base block, CTA-861 extensions and DisplayID 1.x and 2.x blocks, from
binary files, from hexadecimal text as printed by `edid-decode` or `xrandr --verbose`, or
from a connected display through `/sys/class/drm`. The display itself is never written
to. An overview sums up the whole EDID, every field can be edited with its value named
and its bytes marked, detailed timings have a visual editor, data blocks can be added,
duplicated, moved and deleted, and two EDIDs can be compared field by field. Saving
recomputes the checksums and keeps data the editor does not understand.

`edid-editor-cli` does the same from a terminal, without a display: it lists and reads
every field, sets fields, adds, duplicates, moves and deletes groups, and compares and
converts EDIDs. It prints JSON for scripts, and comes with a manual page and completions
for bash, zsh and fish that read groups and fields from the file being edited.

## Building and installing

    just build
    sudo just install        # or: just prefix=$HOME/.local install
    just gui=disabled build  # only edid-editor-cli, without GTK
    just uninstall           # with sudo if it was installed with sudo

    just run [FILE]          # a debug build, uninstalled
    builddir/cli/edid-editor-cli --help

`just` runs Meson; `meson setup`, `meson compile` and `meson install` work as well.
Building needs Meson and a C++17 compiler, and for the application gettext and the
development packages for GTK 4 and libadwaita 1.8 or newer; it is developed against GTK
4.22 and libadwaita 1.9. Without them, or with `gui=disabled` (`-Dgui=disabled` for
Meson), only `edid-editor-cli` is built. The tests of the interface also need Weston,
Xwayland, xdotool, the session bus and the Python bindings for AT-SPI, and are skipped
when those are missing.

## Documentation

- [Using EDID Editor](docs/usage.md), including keyboard shortcuts
- [The command line](docs/cli.md)
- [Known issues](docs/known-issues.md) with the standards and the editor
- [Contributing](CONTRIBUTING.md), including where things are in the code, and
  [reporting a vulnerability](SECURITY.md)

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). The EDID parser in `core/` comes from wxEDID,
Copyright (C) 2014-2025 Tomasz Pawlak, and has been modified for EDID Editor since
2026; the changes are in the history of this repository, which starts from the wxEDID
0.0.33 sources. The rcode library in `core/rcode/`, also by Tomasz Pawlak, is
LGPL-3.0-or-later, see [COPYING.LESSER](core/rcode/COPYING.LESSER). The EDIDs in
`core/tests/corpus/` come from the Linux Hardware EDID repository under CC BY 4.0, see
its [README](core/tests/corpus/README).
