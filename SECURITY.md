# Security

Please report a vulnerability privately rather than in a public issue: through
[a private advisory](https://github.com/sachesi/edid-editor/security/advisories/new) on
GitHub, or by mail to sachesi <xsachesi@pm.me>. Say what you found, how to reproduce it
and which version you ran; a fix is worked out with you before anything is published.

Only the latest release gets fixes.

## What counts

EDID Editor parses data that anyone may have written: EDID files, hexadecimal text, and
the EDID of a connected display. The parser is C++, so the parts where a mistake matters
most are:

- Reading, editing or saving an EDID that makes the application read or write memory
  outside its buffers, however malformed the data is, including with Ignore EDID Errors
  turned on.
- Anything that writes to a display or to a file other than the one the user chose to
  save to. Connected displays are only read, from `/sys/class/drm`.

An EDID that is decoded wrong, refused although it follows the standard, or that makes
the application stop with an error message is a bug; please file it as an ordinary
issue.
