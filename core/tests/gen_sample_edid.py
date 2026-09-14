#!/usr/bin/env python3
"""Generate synthetic EDID test data.

Produces eight binaries used by the headless core tests:
  sample_base.bin : EDID 1.3 base block (DTD 640x480@60, MND text descriptor)
  sample_cea.bin  : base block + CTA-861 ext (VDB, HDMI VSDB, one DTD)
  sample_cea_displayid.bin : base + CTA-861 + DisplayID extension
  sample_cea_t7.bin : base + CTA-861 Type VII detailed timing
  sample_cea_audio.bin : base + CTA-861 LPCM and extended audio descriptors
  sample_displayid_compact.bin : base + DisplayID without trailing payload padding
  sample_displayid_short_padding.bin : base + DisplayID with four padding bytes
  sample_cea_eeodb.bin : base declaring one extension + two CTA-861 blocks,
                         counted by an EDID Extension Override data block

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
    # DTD1: 640x480@60, 25.18 MHz
    b[54:72] = bytes([0xD6, 0x09, 0x80, 0xA0, 0x20, 0xE0, 0x2D, 0x10, 0x20,
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
    e[p:p + 18] = bytes([0xD6, 0x09, 0x80, 0xA0, 0x20, 0xE0, 0x2D, 0x10, 0x20,
                         0x31, 0x00, 0x35, 0x00, 0x4C, 0x21, 0x00, 0x00, 0x1E])
    return chksum(e)


def cea_t7_block():
    e = bytearray(128)
    e[0] = 0x02                            # CTA-861 tag
    e[1] = 0x03                            # revision 3
    payload = bytes([
        0xF6, 34,                          # extended block, tag 34, length 22
        0x02, 0x5C, 0xAF, 0x03,           # revision 2, 241.500 MHz
        0x04, 0x00, 0x0A, 0xA0, 0x00,     # 2560 active, 160 blank
        0x30, 0x80, 0x20, 0x00,           # H offset 48, width 32
        0xA0, 0x05, 0x29, 0x00,           # 1440 active, 41 blank
        0x03, 0x80, 0x05, 0x00,           # V offset 3, width 5
    ])
    e[4:4 + len(payload)] = payload
    e[2] = 4 + len(payload)                # first DTD / end of data blocks
    return chksum(e)


def cea_audio_block():
    e = bytearray(128)
    e[0] = 0x02
    e[1] = 0x03
    payload = bytes([
        0x26,                         # ADB, two SADs
        0x09, 0x07, 0x07,             # LPCM, 2 channels, 32/44.1/48 kHz
        0x79, 0x07, 0x20,             # AFC 15, ACE 4, 2 channels
        0x41, 16,                     # VDB, 1080p60
        0x83, 0x01, 0x00, 0x00,       # speaker allocation
    ])
    e[4:4 + len(payload)] = payload
    e[2] = 4 + len(payload)
    return chksum(e)


def displayid_block():
    e = bytearray(128)
    e[0] = 0x70                            # DisplayID extension tag
    e[1] = 0x12                            # DisplayID 1.2
    e[3] = 0x03                            # standalone display device

    # Type I Detailed Timing Data Block: 2560x1440 at 164.96 and 180 Hz.
    timing_240 = bytes([
        0x3d, 0x11, 0x01, 0x84, 0xff, 0x09, 0x9f, 0x00, 0x2f, 0x80,
        0x1f, 0x00, 0x9f, 0x05, 0x76, 0x00, 0x02, 0x00, 0x04, 0x00,
    ])
    timing_180 = bytes([
        0x53, 0x19, 0x01, 0x04, 0xff, 0x09, 0x9f, 0x00, 0x2f, 0x80,
        0x1f, 0x00, 0x9f, 0x05, 0x1e, 0x00, 0x02, 0x00, 0x04, 0x00,
    ])
    payload = bytes([0x03, 0x00, 40]) + timing_240 + timing_180

    # Unknown data blocks remain structurally parsed and byte-editable.
    payload += bytes([0x30, 0x00, 2, 0xaa, 0x55])
    # Exercise the standard zero-padding convention used by real displays.
    e[2] = 121
    e[5:5 + len(payload)] = payload

    checksum_offset = 5 + e[2]
    e[checksum_offset] = (-sum(e[1:checksum_offset])) & 0xff
    return chksum(e)


def compact_displayid_block():
    e = displayid_block()
    e[2] = 48                         # two data blocks, no zero padding
    e[54:127] = bytes(73)
    checksum_offset = 5 + e[2]
    e[checksum_offset] = (-sum(e[1:checksum_offset])) & 0xff
    return chksum(e)


def short_padding_displayid_block():
    e = compact_displayid_block()
    e[2] = 52                         # two data blocks, four padding bytes
    e[53] = 0
    checksum_offset = 5 + e[2]
    e[checksum_offset] = (-sum(e[1:checksum_offset])) & 0xff
    return chksum(e)


def cea_eeodb_block(extensions):
    e = bytearray(128)
    e[0] = 0x02
    e[1] = 0x03
    payload = bytes([
        0xE2, 0x78, extensions,       # HF-EEODB: extension count override
        0x42, 16, 4,                  # VDB: 1080p60, 720p60
    ])
    e[4:4 + len(payload)] = payload
    e[2] = 4 + len(payload)
    return chksum(e)


def cea_second_block():
    e = bytearray(128)
    e[0] = 0x02
    e[1] = 0x03
    payload = bytes([
        0x23, 0x09, 0x07, 0x07,       # ADB: LPCM, 2 channels
        0x67, 0x03, 0x0C, 0x00, 0x10, 0x00, 0x00, 0x2D,  # HDMI VSDB
    ])
    e[4:4 + len(payload)] = payload
    e[2] = 4 + len(payload)
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

    with open(os.path.join(out_dir, "sample_cea_t7.bin"), "wb") as f:
        f.write(bytes(base_cea) + bytes(cea_t7_block()))

    with open(os.path.join(out_dir, "sample_cea_audio.bin"), "wb") as f:
        f.write(bytes(base_cea) + bytes(cea_audio_block()))

    with open(os.path.join(out_dir, "sample_displayid_compact.bin"), "wb") as f:
        f.write(bytes(base_cea) + bytes(compact_displayid_block()))

    with open(os.path.join(out_dir, "sample_displayid_short_padding.bin"), "wb") as f:
        f.write(bytes(base_cea) + bytes(short_padding_displayid_block()))

    base_multi = bytearray(base)
    base_multi[126] = 2                    # two extension blocks
    chksum(base_multi)

    with open(os.path.join(out_dir, "sample_cea_displayid.bin"), "wb") as f:
        f.write(bytes(base_multi) + bytes(cea_block()) + bytes(displayid_block()))

    with open(os.path.join(out_dir, "sample_cea_eeodb.bin"), "wb") as f:
        f.write(bytes(base_cea) + bytes(cea_eeodb_block(2)) + bytes(cea_second_block()))

    print("wrote eight EDID samples to", out_dir)


if __name__ == "__main__":
    main()
