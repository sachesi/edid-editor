# Using EDID Editor

## Opening EDID data

Open reads an EDID file: 128 bytes for the base block, and 128 more for each extension.
Files ending in `.hex` or `.txt` are read as hexadecimal text, in the layouts printed by
`edid-decode`, `xrandr --verbose` or Export Hex; Import Hex reads any file this way.
Files can also be dropped on the window or given on the command line, and the start
page and Open Recent list the files opened or saved recently.

Open from Display lists the connected displays whose EDID is available under
`/sys/class/drm`, named after the monitor. The EDID opens as a read-only document named
after its connector; the display itself is never written to, so Save As asks for a
file.

EDID data that breaks the standard is refused by default. Open Anyway, offered when a
file fails to open, or Ignore EDID Errors in the main menu open it regardless. Notices
from opening a file, such as a corrected block count, are listed under Notes on the
Overview, and EDID Log in the main menu shows every message.

## The overview

An opened EDID starts on the Overview: the monitor name, manufacturer and date, the
input, screen size, timings, refresh ranges, HDR and color support, audio formats and
extension blocks. It is read from the groups, so it follows unsaved edits.

Under Modes, the Overview lists every detailed timing with its place, starring the
preferred ones and noting the one Linux uses by default. Selecting a mode opens it, and
its star makes it the preferred mode.

Compare with File and Compare with Display list the fields whose values differ between
the current EDID and another one, by block and group, and the groups only one of them
has. Groups are paired by their code in order; checksums are left out.

## Editing

The sidebar lists the blocks and their groups; Ctrl+F searches them, and a group that
holds a match opens. The fields of the selected group are shown as cards with their
plain names and units. Values with named meanings are chosen from a list, single bits
are switches, and the help button of a field shows its full description. The Bytes view
marks the bytes of the last field focused or edited. Reserved fields that hold zero are
hidden; Show Reserved Fields lists them.

Fields derived from other data, such as lengths and offsets, are read-only. Edit
Read-Only Fields unlocks them. Changing a field that decides the layout of its group,
such as a data block tag or length, rebuilds the group from its data; a length that no
longer fits is refused.

Detailed timings open in a visual editor with the pixel clock in MHz, durations in µs,
and the resulting line and refresh rates, along with the image size, interlacing and
sync polarities, which the ModeLine includes. Its diagram draws the whole frame to
scale: the active image, and around it the blanking, labelled with the size of its
sync, back porch and sync offset. Timings of the same resolution can look different there because
their blanking differs. Typing a refresh rate sets the pixel clock that gives it with
the current blanking; the rate shown afterwards is the one the clock reaches in steps
of its unit.

## The preferred mode

Every system takes the first detailed timing of the base block as the display's
preferred mode; a DisplayID timing can also be flagged preferred. Make Preferred, in the
context menu of a timing, or the star on its Timing page or on the Overview, makes a
timing the preferred one without losing any other: the timing and the first one change
places, converted between their formats, and competing DisplayID flags are cleared. The
first detailed timing holds a pixel clock of up to 655.35 MHz, so a faster DisplayID
timing is flagged preferred instead, and a dialog says which mode Linux then uses. Undo
takes the whole change back. Remove Preferred Flag, or the star of a flagged timing,
clears a DisplayID flag; the first detailed timing is preferred by its place, so another
timing has to be made preferred to replace it. The Overview names the mode Linux uses as
the preferred timing, and the first detailed timing too when that is another one.

To add a mode, such as 150 Hz next to 144 Hz, choose Add → Detailed Timing with a group
of a CTA-861 block selected. The new timing is a copy of the selected timing, or of the
first detailed timing when another group is selected, and typing the new rate in
Vertical refresh sets its pixel clock.

## Changing groups

CTA-861 and DisplayID data blocks can be added, duplicated, moved and deleted from the
sidebar, its context menu or the keyboard. The audio templates start with a valid LPCM
or extended audio layout. Undo and Redo cover these changes as well as field edits.

## Saving

Save assembles the blocks and recomputes their checksums. Data the editor does not
understand is kept, but a CTA-861 extension is written with a correct detailed timing
offset and with any non-zero padding cleared. Export Hex writes the EDID as hexadecimal
text, and Save Report writes every group, field, value and unit followed by the raw
data.

## Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| Ctrl+O | Open |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save As |
| Ctrl+Z | Undo |
| Ctrl+Shift+Z | Redo |
| Ctrl+F | Search the groups; Escape clears the search |
| Ctrl+? | Keyboard shortcuts |

While the group list has focus:

| Shortcut | Action |
| --- | --- |
| Ctrl+D | Duplicate the group |
| Delete | Delete the group |
| Alt+Up, Alt+Down | Move the group |
| Shift+F10, Menu | Open the context menu |
