#!/usr/bin/env python3
"""Generate synthetic EDID test data.

Produces two binaries used by the headless core tests:
  sample_base.bin : EDID 1.3 base block (DTD 640x480@60, MND text descriptor)
  sample_cea.bin  : base block + CTA-861 ext (VDB, HDMI VSDB, one DTD)

Usage: gen_sample_edid.py <out_dir>
"""

import os
import struct
import sys


def chksum(block):
    block[127] = 0
    block[127] = (0x100 - (sum(block[:127]) & 0xFF)) & 0xFF
    return block


def base_block():
    b = bytearray(128)
    b[0:8] = b"\x00\xff\xff\xff\xff\xff\xff\x00"
    # manufacturer 'OXC'
    b[8:10] = struct.pack(">H", (15 << 10) | (24 << 5) | 3)
    b[10:12] = struct.pack("<H", 0x1234)   # product id
    b[12:16] = struct.pack("<I", 42)       # serial
    b[16] = 1                              # week
    b[17] = 30                             # year (2020)
    b[18] = 1                              # version
    b[19] = 3                              # revision
    b[20] = 0x80 | 0x05                    # digital input, 24bpp
    b[21] = 40                             # H size cm
    b[22] = 25                             # V size cm
    b[23] = 120                            # gamma 2.20
    b[24] = 0xB0                           # features
    b[25:35] = bytes([0xEE, 0xE4, 0xC1, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01])
    b[35:37] = b"\x20\x20"                 # established timings 640x480@60
    b[38:54] = b"\x01\x01" * 8             # std timings unused
    # DTD1: 640x480@60.00, 25.175 MHz
    b[54:72] = bytes([0x57, 0x62, 0x80, 0xA0, 0x20, 0xE0, 0x2D, 0x10, 0x20,
                      0x31, 0x00, 0x35, 0x00, 0x4C, 0x21, 0x00, 0x00, 0x1E])
    # MND: monitor name 'GTK-PORT', LF padded
    b[72:75] = b"\x00\x00\x00"
    b[75] = 0xFC
    b[76] = 0x08
    b[77:85] = b"GTK-PORT"
    b[85:90] = b"\x0a" * 5
    # remaining descriptors: VOID
    b[90:93] = b"\x00\x00\x00"
    b[93] = 0x10
    b[108:111] = b"\x00\x00\x00"
    b[111] = 0x10
    return chksum(b)


def cea_block():
    e = bytearray(128)
    e[0] = 0x02                            # CTA-861 tag
    e[1] = 0x03                            # revision 3
    p = 4
    e[p:p + 4] = bytes([0x43, 1, 2, 4])    # VDB: 3 SVDs (VIC 1,2,4)
    p += 4
    # HDMI 1.4 VSDB: OUI 00-0C-03, phys addr 1.2.3.4
    e[p:p + 8] = bytes([0x67, 0x03, 0x0C, 0x00, 0x12, 0x34, 0x28, 0x00])
    p += 8
    e[2] = p                               # DTD offset
    e[p:p + 18] = bytes([0x57, 0x62, 0x80, 0xA0, 0x20, 0xE0, 0x2D, 0x10, 0x20,
                         0x31, 0x00, 0x35, 0x00, 0x4C, 0x21, 0x00, 0x00, 0x1E])
    return chksum(e)


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    base = base_block()

    with open(os.path.join(out_dir, "sample_base.bin"), "wb") as f:
        f.write(base)

    base_cea = bytearray(base)
    base_cea[126] = 1                      # one extension block
    chksum(base_cea)

    with open(os.path.join(out_dir, "sample_cea.bin"), "wb") as f:
        f.write(bytes(base_cea) + bytes(cea_block()))

    print("wrote sample_base.bin, sample_cea.bin to", out_dir)


if __name__ == "__main__":
    main()
