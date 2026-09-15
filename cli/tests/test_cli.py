#!/usr/bin/env python3
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

cli, cea, displayid = sys.argv[1], sys.argv[2], sys.argv[3]
work = Path(tempfile.mkdtemp(prefix="edid-editor-cli-"))
failures = 0


def run(*args, stdin=None, status=0):
    result = subprocess.run([cli, *args], input=stdin, capture_output=True)
    if result.returncode != status:
        raise AssertionError(f"{' '.join(args)}: exit status {result.returncode}, "
                             f"expected {status}\n{result.stderr.decode()}")
    return result.stdout.decode(errors="replace"), result.stderr.decode(errors="replace")


def check(what, test):
    global failures
    try:
        test()
        print(f"PASS: {what}")
    except AssertionError as error:
        failures += 1
        print(f"FAIL: {what}: {error}")


def checksums_valid(path):
    data = Path(path).read_bytes()
    return all(sum(data[index:index + 128]) % 256 == 0
               for index in range(0, len(data), 128))


def help_and_usage():
    out, _ = run("--help")
    assert "Usage: edid-editor-cli" in out
    run(status=2)
    _, err = run("frob", status=2)
    assert "unknown command frob" in err
    _, err = run("set", cea, "DTD:1", "interlace=on", status=2)
    assert "-o OUTPUT or --in-place" in err


def reading():
    out, _ = run("info", cea)
    assert "Display" in out and "GTK-PORT" in out
    out, _ = run("groups", cea)
    assert "0x036  DTD" in out and "Block 1: CTA-861" in out
    out, _ = run("fields", cea, "DTD:1")
    assert "#1   Pixel clock" in out and "#2   Horizontal active" in out
    assert "Refresh" in out and "derived: setting it changes the pixel clock" in out
    out, _ = run("get", cea, "0x036", "h-active-pix")
    assert out.strip() == "640"
    out, _ = run("get", cea, "0x036", "Horizontal active")
    assert out.strip() == "640"
    out, _ = run("describe", cea, "0x036", "horizontal-active")
    assert out.startswith("DTD@0x036 Horizontal active (H-Active pix)")
    out, _ = run("describe", cea, "0x05A", "desc_type")
    assert "Named values:" in out and "MND" in out
    out, _ = run("report", cea)
    assert "EDID block [1]: CTA-861 extension" in out
    _, err = run("get", cea, "DTD", "#1", status=1)
    assert "matches several groups: DTD@0x036 DTD@0x090" in err
    # names that look like the refresh rate of a timing, or like name:N
    assert run("get", cea, "STI:1", "refresh-rate")[0].strip().isdigit()
    assert run("get", cea, "CHD", "ycbcr-4:2:2")[0].strip() == "0"


def set_and_diff():
    target = work / "set.bin"
    out, _ = run("set", cea, "DTD:1", "pixelclock=25.20", "interlace=on", "-o", str(target))
    assert "Pixel clock: 25.18 -> 25.20" in out and "Interlaced: 0 -> 1" in out
    assert run("get", str(target), "DTD:1", "interlace")[0].strip() == "1"
    assert checksums_valid(target)
    out, _ = run("diff", cea, str(target), status=1)
    assert "Pixel clock: 25.18 -> 25.20" in out
    run("diff", cea, cea)
    same = work / "same.bin"
    run("set", cea, "DTD:1", "hactivepix=640", "-o", str(same))
    assert same.read_bytes() == Path(cea).read_bytes()


def refresh():
    rate = float(run("get", cea, "DTD:1", "refresh")[0])
    target = work / "refresh.bin"
    out, _ = run("set", cea, "DTD:1", "refresh=75", "-o", str(target))
    assert "Refresh: " + f"{rate:.2f}" + " -> 75.00 Hz" in out, out
    assert abs(float(run("get", str(target), "DTD:1", "refresh")[0]) - 75) < 0.1
    # the blanking stays; only the pixel clock changes
    out, _ = run("diff", cea, str(target), status=1)
    assert "Pixel clock:" in out and out.count(" -> ") == 1, out
    _, err = run("set", cea, "MND", "refresh=60", "-o", str(target), status=1)
    assert "not a detailed timing" in err
    _, err = run("set", cea, "DTD:1", "refresh=fast", "-o", str(target), status=1)
    assert "is not a rate in Hz" in err
    before = run("get", displayid, "DID-T1:1", "refresh")[0].strip()
    out, _ = run("set", displayid, "DID-T1:1", "refresh=144", "-o", str(target))
    assert f"Refresh: {before} -> 144.00 Hz" in out, out


def refused_writes():
    target = work / "refused.bin"
    _, err = run("set", cea, "DTD:1", "refresh=60", "oops", "-o", str(target), status=2)
    assert "oops is not FIELD=VALUE" in err and not target.exists()
    _, err = run("set", cea, "CHD", "#2=5", "-o", str(target), status=1)
    assert "--edit-read-only" in err
    _, err = run("set", cea, "DTD:1", "hactivepix=99999", "-o", str(target), status=1)
    assert "99999 is not a valid value (0 to 4095)" in err
    assert not target.exists()
    _, err = run("set", cea, "DTD:1", "interlace=on", "-o", "/sys/edid", status=1)
    assert "connected displays are only read" in err


def rebuild():
    target = work / "rebuild.bin"
    out, _ = run("set", cea, "0x05A", "desc_type=MND", "--edit-read-only",
                 "-o", str(target))
    assert "rebuilt as MND@0x05A" in out
    assert "0x05A  MND" in run("groups", str(target))[0]


def structure():
    added = work / "added.bin"
    run("add", cea, "1", "audio-lpcm", "-o", str(added))
    out, _ = run("groups", str(added))
    assert "0x090  ADB" in out and "0x094  DTD" in out
    assert checksums_valid(added)
    _, err = run("add", cea, "1", "displayid", "-o", str(added), status=1)
    assert "goes into a DisplayID block" in err

    # a new timing starts as a copy of the first, or of the timing given
    run("add", cea, "1", "timing", "-o", str(added))
    data = added.read_bytes()
    assert data[0x90:0xA2] == data[0x36:0x48], "the new timing is not the first one"
    run("add", cea, "1", "timing", "DTD@0x090", "-o", str(added))
    assert added.read_bytes()[0x90:0xA2] == Path(cea).read_bytes()[0x90:0xA2]
    _, err = run("add", displayid, "1", "timing", "DID-T1:2", "-o", str(added), status=1)
    assert "doesn't fit a detailed timing of a CTA-861 block" in err

    before = run("groups", displayid)[0].count("DID-DB")
    target = work / "displayid.bin"
    run("add", displayid, "2", "displayid", "-o", str(target))
    assert run("groups", str(target))[0].count("DID-DB") == before + 1

    target = work / "structure.bin"
    run("duplicate", cea, "SVD:1", "-o", str(target))
    assert run("groups", str(target))[0].count("SVD") == 4
    run("move", cea, "VSD", "up", "-o", str(target))
    assert "0x084  VSD" in run("groups", str(target))[0]
    run("delete", cea, "VSD", "-o", str(target))
    assert "VSD" not in run("groups", str(target))[0]


def bytes_and_files():
    text = work / "sample.hex"
    run("convert", cea, "-o", str(text))
    assert text.read_text().startswith("00FFFFFFFFFFFF00")
    binary = work / "sample.bin"
    run("convert", "-", "-o", str(binary), stdin=text.read_bytes())
    assert binary.read_bytes() == Path(cea).read_bytes()
    out, _ = run("convert", cea, "-o", "-")
    assert out == text.read_text()

    broken = bytearray(Path(cea).read_bytes())
    broken[127] ^= 0x55
    damaged = work / "damaged.bin"
    damaged.write_bytes(broken)
    out, _ = run("fix-checksums", str(damaged), "--in-place")
    assert "block 0: checksum" in out
    assert damaged.read_bytes() == Path(cea).read_bytes()

    in_place = work / "in-place.bin"
    shutil.copy(cea, in_place)
    run("set", str(in_place), "DTD:1", "interlace=on", "-i")
    assert run("get", str(in_place), "DTD:1", "interlace")[0].strip() == "1"


def preferred():
    # a timing the first place can't hold is flagged in DisplayID instead
    target = work / "prefer.bin"
    out, _ = run("prefer", displayid, "DID-T1:2", "-o", str(target))
    assert "above the 655.35 MHz" in out and "flagged preferred in DisplayID" in out, out
    assert run("get", str(target), "DID-T1:2", "preferred")[0].strip() == "1"
    assert run("get", str(target), "DID-T1:1", "preferred")[0].strip() == "0"
    assert "0x036  DTD       640x480" in run("groups", str(target))[0]
    assert checksums_valid(target)

    # one that fits changes places with the first timing, in its format
    slower = work / "slower.bin"
    # with polarities unlike the first timing's, so they must be carried over
    hsync = int(run("get", displayid, "DTD:1", "horizontal-sync-type")[0])
    vsync = int(run("get", displayid, "DTD:1", "vertical-sync-type")[0])
    run("set", displayid, "DID-T1:1", "refresh=60", f"horizontal-sync-positive={1 - hsync}",
        f"vertical-sync-positive={1 - vsync}", "-o", str(slower))
    out, _ = run("prefer", str(slower), "DID-T1:1", "-o", str(target))
    assert "is now the first detailed timing" in out, out
    groups = run("groups", str(target))[0]
    assert "0x036  DTD       2560x1440 @ 60.00Hz" in groups, groups
    assert "0x108    DID-T1    640x480 @ 59.95 Hz" in groups, groups
    for mine, theirs in (("horizontal-front-porch", "horizontal-sync-offset"),
                         ("vertical-front-porch", "vertical-sync-offset"),
                         ("vertical-sync-width", "vertical-sync-width"),
                         ("horizontal-sync-positive", "horizontal-sync-type"),
                         ("vertical-sync-positive", "vertical-sync-type")):
        assert run("get", str(slower), "DID-T1:1", mine)[0] == \
            run("get", str(target), "DTD:1", theirs)[0], mine
    # DisplayID keeps a preferred timing that doesn't compete with the choice
    assert run("get", str(target), "DID-T1:1", "preferred")[0].strip() == "1"
    assert run("get", str(target), "DID-T1:2", "preferred")[0].strip() == "0"

    # two detailed timings trade their bytes, and trading back restores the file
    raw = bytearray(Path(cea).read_bytes())
    raw[0x90 + 12] ^= 0x01                  # tell the CTA-861 timing apart
    raw[255] = (-sum(raw[128:255])) & 0xFF
    source = work / "two-dtds.bin"
    source.write_bytes(bytes(raw))
    run("prefer", str(source), "DTD@0x090", "-o", str(target))
    swapped = target.read_bytes()
    assert swapped[0x36:0x48] == bytes(raw[0x90:0xA2])
    assert swapped[0x90:0xA2] == bytes(raw[0x36:0x48])
    back = work / "back.bin"
    run("prefer", str(target), "DTD@0x090", "-o", str(back))
    assert back.read_bytes() == bytes(raw)
    out, _ = run("prefer", cea, "DTD:1", "-o", str(target))
    assert "is the first detailed timing" in out

    # off clears a DisplayID flag; the first timing stays preferred by its place
    out, _ = run("prefer", displayid, "DID-T1:1", "off", "-o", str(target))
    assert "is no longer preferred; Linux now uses 640x480 @ 59.95 Hz" in out, out
    assert run("get", str(target), "DID-T1:1", "preferred")[0].strip() == "0"
    _, err = run("prefer", displayid, "DTD:1", "off", "-o", str(target), status=1)
    assert "which is always preferred" in err
    _, err = run("prefer", displayid, "DID-T1:2", "off", "-o", str(target), status=1)
    assert "is not preferred" in err
    info = run("info", displayid)[0]
    assert "Preferred timing       2560x1440 @ 164.96 Hz" in info, info
    assert "First detailed timing  640x480 @ 59.95Hz" in info, info
    assert "First detailed timing" not in run("info", str(target))[0]
    _, err = run("prefer", cea, "MND", "-o", str(target), status=1)
    assert "only a detailed timing can be preferred" in err


def binary_bits():
    target = work / "bits.bin"
    out, _ = run("set", cea, "DTD:1", "sync-type=2", "-o", str(target))
    assert "Sync type: 0b11 -> 0b10" in out, out
    run("set", cea, "DTD:1", "sync-type=0b01", "-o", str(target))
    _, err = run("set", cea, "DTD:1", "sync-type=9", "-o", str(target), status=1)
    assert "9 is not a valid value (0 to 3)" in err


def json_output():
    items = json.loads(run("info", cea, "--json")[0])
    assert {"section": "Display", "label": "Name", "value": "GTK-PORT"} in items, items
    groups = json.loads(run("groups", "--json", cea)[0])
    assert {"address": "DTD@0x036", "block": 0, "offset": 0x36, "code": "DTD"}.items() \
        <= groups[14].items(), groups[14]
    assert any(group["depth"] == 1 for group in groups)
    fields = json.loads(run("fields", cea, "DTD:1", "--json")[0])
    assert fields["address"] == "DTD@0x036" and fields["refresh"] > 59
    assert fields["fields"][1]["name"] == "Horizontal active"
    assert fields["fields"][1]["raw"] == 640 and fields["fields"][1]["value"] == "640"
    assert json.loads(run("fields", cea, "MND", "--json")[0])["refresh"] is None
    target = work / "json.bin"
    run("set", cea, "DTD:1", "interlace=on", "-o", str(target))
    differences = json.loads(run("diff", cea, str(target), "--json", status=1)[0])
    assert differences == [{"place": differences[0]["place"], "field": "interlace",
                            "left": "0", "right": "1"}], differences
    assert json.loads(run("diff", cea, cea, "--json")[0]) == []
    _, err = run("get", cea, "DTD:1", "interlace", "--json", status=2)
    assert "--json works with" in err


def completion():
    def words(*args, status=0):
        lines = run("complete", *args, status=status)[0].splitlines()
        return [line.split("\t")[0] for line in lines]
    out = run("complete", "set", cea, "DTD:1", "")[0]
    assert "pixel-clock=\t25.18 MHz\n" in out and "refresh=\t59.95 Hz\n" in out, out
    assert "DTD:2\t640x480 @ 59.95Hz\n" in run("complete", "set", cea, "")[0]
    assert "set" in words("") and "complete" not in words("")
    assert words("info", "", status=3) == []
    assert words("diff", cea, "", status=3) == []
    groups = words("set", cea, "")
    assert {"0x036", "DTD:1", "DTD:2", "MND", "VSD"} <= set(groups), groups
    assert "DTD" not in groups
    assert words("get", cea, "DTD:1", "") [:2] == ["pixel-clock", "horizontal-active"]
    assert "refresh" in words("get", cea, "DTD:1", "")
    assert "interlaced=" in words("set", cea, "DTD:1", "--in-place", "")
    assert words("set", cea, "DTD:1", "interlaced=") == ["interlaced=on", "interlaced=off"]
    assert "desctype=MND" in words("set", cea, "0x05A", "pixel-clock=1", "desctype=")
    assert words("add", cea, "") == ["1"]
    assert words("add", cea, "1", "") == ["audio-lpcm", "audio-extended", "video", "timing"]
    assert words("add", displayid, "2", "") == ["displayid"]
    assert words("move", cea, "VSD", "") == ["up", "down"]
    assert words("displays", "") == [] and words("frob", "") == []
    # every field offered is one the other commands take
    for sample in cea, displayid:
        for group in words("get", sample, ""):
            for field in words("get", sample, group, ""):
                run("get", sample, group, field)


for name, test in [("help and usage errors", help_and_usage), ("reading", reading),
                   ("set and diff", set_and_diff), ("refresh rate", refresh),
                   ("refused writes", refused_writes),
                   ("group rebuild", rebuild), ("group structure", structure),
                   ("conversion and files", bytes_and_files), ("preferred timing", preferred),
                   ("binary bit fields", binary_bits), ("JSON", json_output),
                   ("shell completion", completion)]:
    check(name, test)
shutil.rmtree(work, ignore_errors=True)
sys.exit(1 if failures else 0)
