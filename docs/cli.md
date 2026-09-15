# The command line

`edid-editor-cli` reads and changes EDID data with the same code as the editor, without
a display or GTK. `edid-editor-cli --help` lists every command and option.

## Reading

    edid-editor-cli info monitor.bin              # the overview
    edid-editor-cli report monitor.bin            # every group, field, value and unit
    edid-editor-cli displays                      # connected displays
    edid-editor-cli info /sys/class/drm/card1-DP-1/edid

A file is binary EDID or hexadecimal text in the layouts `edid-decode`, `xrandr --verbose`
and Export Hex print; the kind is recognised from the data, and `-` reads standard input.
EDID data that breaks the standard is refused unless `--ignore-errors` is given, as with
Open Anyway in the editor.

## Finding a group and a field

`groups` lists every group with its offset and code, the sub-groups indented under the
group that holds them:

    $ edid-editor-cli groups monitor.bin
    Block 0: Base EDID
      0x000  BED       XMI, prod_ID 0x27B2
      ...
      0x036  DTD       2560x1440 @ 59.95Hz
    Block 1: CTA-861
      0x080  CHD       CEA-861 header
      0x084  VDB       Video Data Block
      0x085    SVD       640x480p @ 59.94/60Hz

A group is named by its offset (`0x036`), by its code when only one group has it (`MRL`),
by the nth group with a code (`DTD:2`), or by both (`DTD@0x036`). Offsets change when
groups are added, moved or removed, so list the groups again after such a change.

`fields` lists the fields of a group; `describe` shows one field in full, with its
description, range, bytes and named values:

    $ edid-editor-cli fields monitor.bin DTD:1
    DTD@0x036  2560x1440 @ 59.95Hz, block 0
      #1   Pixel clock     241.50  MHz
      #2   H-Active pix    2560
      ...
    $ edid-editor-cli describe monitor.bin 0x036 "Pixel clock"
    $ edid-editor-cli get monitor.bin 0x036 pixelclock
    241.50

A field is named as `fields` lists it, without regard to case, spaces, `-` and `_`, so
`"Pixel clock"`, `pixel-clock` and `pixelclock` are the same field. `name:N` picks the nth
field of a name that repeats, and `#N` the nth field.

## Changing

A changed EDID is written to `-o OUTPUT`, or back to the file with `--in-place`; as
hexadecimal text when the name ends in `.hex` or `.txt`, or to standard output with
`-o -`. The new file replaces the old one only once it is complete. Checksums are
recomputed, and a connected display is never written to.

    edid-editor-cli set monitor.bin DTD:1 pixelclock=241.60 interlace=off -o new.bin
    edid-editor-cli set new.bin VID "Color depth=10 bits" --in-place
    edid-editor-cli add new.bin 1 audio-lpcm --in-place
    edid-editor-cli duplicate new.bin SVD:3 --in-place
    edid-editor-cli move new.bin ADB:2 up --in-place
    edid-editor-cli delete new.bin DTD:3 --in-place
    edid-editor-cli diff monitor.bin new.bin

`set` takes a value as the field shows it, a named value from `describe`, or `on` and
`off` for single bits; a value the field refuses stops the command before anything is
written. Fields derived from other data need `--edit-read-only`, like Edit Read-Only
Fields in the editor, and a change that alters the type or layout of a group rebuilds it,
as the editor does. `add` takes `audio-lpcm`, `audio-extended`, `video` or `timing` for a
CTA-861 block and `displayid` for a DisplayID block, and puts the group where the editor
would.

`fix-checksums` and `convert` work on the bytes alone, so they also serve data the
editor refuses: the first recomputes every block's checksum, the second writes the same
bytes as binary or text.

## Exit status

0 on success, 1 when the data can't be read, a value is refused or `diff` finds
differences, and 2 for a mistake in the command line.
