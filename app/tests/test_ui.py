#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def require_runtime():
    missing = [name for name in ("dbus-run-session", "weston", "xdotool")
               if shutil.which(name) is None]
    try:
        import gi
        gi.require_version("Atspi", "2.0")
        from gi.repository import Atspi  # noqa: F401
    except (ImportError, ValueError):
        missing.append("python3-atspi")
    if missing:
        print("SKIP: missing " + ", ".join(missing))
        raise SystemExit(77)


def run_session(script, app, fixture, width, scenario):
    runtime = tempfile.mkdtemp(prefix="wxedid-runtime-")
    os.chmod(runtime, 0o700)
    log = Path(runtime) / "weston.log"
    env = os.environ.copy()
    env["XDG_RUNTIME_DIR"] = runtime
    output = Path(runtime) / "session.log"
    command = [
        "dbus-run-session", "--", "weston", "--xwayland", "-B", "headless",
        f"--width={width}", "--height=700", "--renderer=pixman",
        "--shell=kiosk", f"--log={log}", "--", sys.executable, script,
        "--inside", app, fixture, scenario,
    ]
    with output.open("w+") as stream:
        result = subprocess.run(command, env=env, text=True, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=150)
        stream.seek(0)
        diagnostics = stream.read()
    # weston exits successfully whatever its client returns, so the scenario
    # reports success through a marker file instead.
    passed = (Path(runtime) / "scenario-passed").exists()
    shutil.rmtree(runtime, ignore_errors=True)
    if result.returncode != 0 or not passed:
        sys.stderr.write(diagnostics)
        raise SystemExit(result.returncode or 1)
    rejected = ("Gtk-CRITICAL", "Adwaita-CRITICAL", "GLib-GObject-CRITICAL",
                "exceeds AdwApplicationWindow width", "still has children left")
    for marker in rejected:
        if marker in diagnostics:
            sys.stderr.write(diagnostics)
            raise AssertionError(f"runtime diagnostic: {marker}")


def walk(node):
    # Nodes can vanish while a dialog or row closes; skip them.
    if node is None:
        return
    yield node
    try:
        count = node.get_child_count()
    except Exception:
        return
    for index in range(count):
        try:
            child = node.get_child_at_index(index)
        except Exception:
            continue
        yield from walk(child)


def wait_for(predicate, message, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.1)
    raise AssertionError(message)


def nodes(Atspi):
    return list(walk(Atspi.get_desktop(0)))


def name_of(node):
    try:
        return node.get_name() or ""
    except Exception:
        return ""


def role_of(node):
    try:
        return node.get_role()
    except Exception:
        return None


def named(Atspi, name, role=None):
    for node in nodes(Atspi):
        if name_of(node) == name and (role is None or role_of(node) == role):
            return node
    return None


def count_named_part(Atspi, text):
    return sum(text in name_of(node) for node in nodes(Atspi))


def group_count(Atspi, code):
    # The list view exposes only realized rows; filter so every match is realized.
    search = wait_for(lambda: named(Atspi, "Search groups", Atspi.Role.ENTRY),
                      "group search was not exposed")
    search.get_editable_text_iface().set_text_contents(code)
    time.sleep(0.4)
    count = sum(role_of(node) == Atspi.Role.LABEL and
                name_of(node).startswith(code + " · ") for node in nodes(Atspi))
    search.get_editable_text_iface().set_text_contents("")
    time.sleep(0.5)
    return count


def text_of(Atspi, node):
    return Atspi.Text.get_text(node, 0, Atspi.Text.get_character_count(node))


# Keyboard and pointer events cannot be injected into the headless XWayland
# session, so every step goes through accessibility actions instead.
def actionable(Atspi, name, role=None):
    for node in nodes(Atspi):
        if name_of(node) != name or (role is not None and role_of(node) != role):
            continue
        try:
            if node.get_action_iface().get_n_actions() > 0:
                return node
        except Exception:
            continue
    return None


def press_last(Atspi, name):
    # Rebuilt field cards can leave stale nodes ahead of the current ones.
    def newest():
        found = [node for node in nodes(Atspi) if name_of(node) == name]
        for node in reversed(found):
            try:
                if node.get_action_iface().get_n_actions() > 0:
                    return node
            except Exception:
                continue
        return None
    node = wait_for(newest, f"{name} was not exposed")
    assert node.get_action_iface().do_action(0), f"{name} did not activate"
    time.sleep(0.3)


def press(Atspi, name, role=None):
    node = wait_for(lambda: actionable(Atspi, name, role), f"{name} was not exposed")
    assert node.get_action_iface().do_action(0), f"{name} did not activate"
    time.sleep(0.3)


def menu_item(Atspi, name):
    # Popover menu items are labelled by a label whose text, not name,
    # carries the item title.
    roles = (Atspi.Role.MENU_ITEM, Atspi.Role.CHECK_MENU_ITEM)
    for node in nodes(Atspi):
        if role_of(node) not in roles:
            continue
        try:
            relations = node.get_relation_set()
        except Exception:
            continue
        for relation in relations:
            if relation.get_relation_type() != Atspi.RelationType.LABELLED_BY:
                continue
            for index in range(relation.get_n_targets()):
                try:
                    if text_of(Atspi, relation.get_target(index)) == name:
                        return node
                except Exception:
                    continue
    return None


def close_dialog(Atspi, title):
    dialog = wait_for(lambda: named(Atspi, title, Atspi.Role.DIALOG),
                      f"{title} dialog did not open")
    for node in walk(dialog):
        if name_of(node) == "Close" and role_of(node) == Atspi.Role.PUSH_BUTTON:
            assert node.get_action_iface().do_action(0)
            wait_for(lambda: named(Atspi, title, Atspi.Role.DIALOG) is None,
                     f"{title} dialog did not close")
            return
    raise AssertionError(f"{title} dialog has no Close button")


def activate_menu_item(Atspi, name, menu="Main menu"):
    press(Atspi, menu)
    item = wait_for(lambda: menu_item(Atspi, name), f"{name} menu item was not exposed")
    assert item.get_action_iface().do_action(0), f"{name} did not activate"
    time.sleep(0.3)


def dropdown(Atspi, current):
    for node in nodes(Atspi):
        if role_of(node) == Atspi.Role.COMBO_BOX and name_of(node) == current:
            return node
    return None


def window_exists(title):
    return subprocess.run(["xdotool", "search", "--name", title],
                          capture_output=True).returncode == 0


def filtered_rows(Atspi):
    lists = [node for node in nodes(Atspi) if role_of(node) == Atspi.Role.LIST]
    try:
        return lists[-1].get_child_count() if lists else -1
    except Exception:
        return -1


def select_group(Atspi, index):
    def attempt():
        lists = [node for node in nodes(Atspi) if role_of(node) == Atspi.Role.LIST]
        try:
            return bool(lists) and lists[-1].get_selection_iface().select_child(index)
        except Exception:
            return False
    wait_for(attempt, f"group {index} could not be selected")
    time.sleep(0.5)


def launch_app(Atspi, app, path, title=None):
    env = os.environ.copy()
    env.update({"GDK_BACKEND": "x11", "GSK_RENDERER": "cairo", "GTK_A11Y": "atspi"})
    process = subprocess.Popen([app] if path is None else [app, path], env=env, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if title is None:
        title = "EDID Editor" if path is None else Path(path).name
    wait_for(lambda: any(node.get_role() == Atspi.Role.FRAME and
                         title in node.get_name()
                         for node in nodes(Atspi)), "application window did not open")
    return process


def stop_app(process):
    process.terminate()
    try:
        stdout, stderr = process.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate(timeout=3)
    sys.stdout.write(stdout)
    sys.stderr.write(stderr)


def functional(Atspi, app, fixture):
    with tempfile.TemporaryDirectory(prefix="wxedid-ui-") as directory:
        target = Path(directory) / "editable.bin"
        shutil.copyfile(fixture, target)
        original = target.read_bytes()
        process = launch_app(Atspi, app, str(target))
        try:
            wait_for(lambda: named(Atspi, "GTK-PORT", Atspi.Role.LABEL),
                     "opening a file did not show its overview")
            assert named(Atspi, "640x480 at 59.95 Hz; 2560x1440 at 59.95 Hz",
                         Atspi.Role.LABEL), "the overview did not list the timings"
            groups = [node for node in nodes(Atspi) if role_of(node) == Atspi.Role.LIST][-1]
            assert groups.get_selection_iface().get_n_selected_children() == 0, \
                "a group stayed selected while the overview is shown"
            search = wait_for(lambda: named(Atspi, "Search groups", Atspi.Role.ENTRY),
                              "group search was not exposed")
            search.get_editable_text_iface().set_text_contents("T7VTB")
            wait_for(lambda: any("T7VTB" in name_of(node) for node in nodes(Atspi)),
                     "matching group disappeared during search")
            search.get_editable_text_iface().set_text_contents("no-such-edid-group")
            wait_for(lambda: named(Atspi, "No matching groups"),
                     "empty search state did not appear")
            search.get_editable_text_iface().set_text_contents("")
            wait_for(lambda: named(Atspi, "No matching groups") is None,
                     "clearing the search did not restore the groups")

            def switch_on(name):
                node = named(Atspi, name, Atspi.Role.SWITCH)
                return node is not None and \
                    node.get_state_set().contains(Atspi.StateType.CHECKED)
            select_group(Atspi, 15)
            press(Atspi, "Fields")
            wait_for(lambda: named(Atspi, "DTD · offset 0x036 · block 0", Atspi.Role.LABEL),
                     "the group heading did not name its code, offset, and block")
            add = [node for node in nodes(Atspi) if name_of(node) == "Add a group"]
            assert add and not any(node.get_state_set().contains(Atspi.StateType.ENABLED)
                                   for node in add), "groups can be added to the base block"
            press_last(Atspi, "About Pixel clock")
            wait_for(lambda: any("divisible by 0.25MHz" in name_of(node)
                                 for node in nodes(Atspi)),
                     "the field help did not show the full description")
            press_last(Atspi, "About Pixel clock")
            interlaced = wait_for(lambda: named(Atspi, "Interlaced", Atspi.Role.SWITCH),
                                  "a single bit was not shown as a switch")
            assert not switch_on("Interlaced")
            assert interlaced.get_action_iface().do_action(0)
            wait_for(lambda: switch_on("Interlaced"), "the switch did not turn on")
            wait_for(lambda: any(name_of(node).startswith("Modified") for node in nodes(Atspi)),
                     "turning on a bit did not change the EDID")
            activate_menu_item(Atspi, "Undo")
            wait_for(lambda: named(Atspi, "Interlaced", Atspi.Role.SWITCH) and
                     not switch_on("Interlaced"), "undo did not turn the bit off")
            # typing between valid values: 512 -> 1512 mm, no invalid step
            width = wait_for(lambda: named(Atspi, "Image width", Atspi.Role.TEXT),
                             "the image width was not editable")
            assert width.get_editable_text_iface().insert_text(0, "1", 1)
            wait_for(lambda: any(name_of(node).startswith("Modified") for node in nodes(Atspi)),
                     "typing a valid value did not mark the EDID as modified")
            activate_menu_item(Atspi, "Undo")

            def select_overview():
                for node in nodes(Atspi):
                    if role_of(node) == Atspi.Role.LIST and \
                            any(name_of(inner) == "Overview" for inner in walk(node)):
                        return node.get_selection_iface().select_child(0)
                return False
            wait_for(select_overview, "the overview could not be selected")
            wait_for(lambda: named(Atspi, "Week 1, 2020", Atspi.Role.LABEL),
                     "selecting the overview did not show it again")

            select_group(Atspi, 6)
            wait_for(lambda: named(Atspi, "7 reserved fields are hidden", Atspi.Role.LABEL),
                     "reserved fields were not hidden")
            assert named(Atspi, "Reserved0", Atspi.Role.SWITCH) is None
            press(Atspi, "Show Reserved Fields", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: named(Atspi, "Reserved0", Atspi.Role.SWITCH),
                     "showing reserved fields did not list them")
            activate_menu_item(Atspi, "Show Reserved Fields")
            wait_for(lambda: named(Atspi, "Reserved0", Atspi.Role.SWITCH) is None,
                     "reserved fields were not hidden again")

            select_group(Atspi, 21)
            wait_for(lambda: named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON),
                     "selecting the timing group did not open the timing editor")

            original_groups = group_count(Atspi, "T7VTB")
            press(Atspi, "Duplicate group (Ctrl+D)")
            wait_for(lambda: group_count(Atspi, "T7VTB") > original_groups,
                     "duplicate group did not update the sidebar")
            press(Atspi, "Delete group (Delete)")
            press(Atspi, "Delete", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: group_count(Atspi, "T7VTB") == original_groups,
                     "confirmed group deletion did not update the sidebar")
            activate_menu_item(Atspi, "Undo")
            wait_for(lambda: group_count(Atspi, "T7VTB") > original_groups,
                     "undo did not restore the deleted group")
            activate_menu_item(Atspi, "Redo")
            wait_for(lambda: group_count(Atspi, "T7VTB") == original_groups,
                     "redo did not delete the group again")
            activate_menu_item(Atspi, "Undo")
            activate_menu_item(Atspi, "Undo")
            wait_for(lambda: group_count(Atspi, "T7VTB") == original_groups,
                     "undo did not remove the duplicated group")
            assert target.read_bytes() == original

            activate_menu_item(Atspi, "Extended Audio Block", menu="Add a group")
            wait_for(lambda: group_count(Atspi, "ADB") > 0,
                     "extended audio block was not added")
            activate_menu_item(Atspi, "Undo")
            wait_for(lambda: group_count(Atspi, "ADB") == 0,
                     "undo did not remove the added audio block")
            activate_menu_item(Atspi, "Redo")
            wait_for(lambda: group_count(Atspi, "ADB") > 0,
                     "redo did not add the audio block again")
            press(Atspi, "Delete group (Delete)")
            press(Atspi, "Delete", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: group_count(Atspi, "ADB") == 0,
                     "audio block was not deleted")

            select_group(Atspi, 21)
            spin = wait_for(lambda: named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON),
                            "timing editor did not open")
            assert int(spin.get_value_iface().get_current_value()) == 241500
            press(Atspi, "Fields")
            tag = wait_for(lambda: dropdown(Atspi, "EXT: Extended Tag Code"),
                           "tag code was not shown")
            assert not tag.get_state_set().contains(Atspi.StateType.SENSITIVE)
            pixel_entry = wait_for(lambda: named(Atspi, "Pixel clock", Atspi.Role.TEXT),
                                   "field editor did not expose its label")
            original_pixel_text = text_of(Atspi, pixel_entry)
            assert original_pixel_text
            pixel_entry.get_editable_text_iface().set_text_contents("not-a-number")
            wait_for(lambda: named(Atspi, "Enter a valid value", Atspi.Role.LABEL),
                     "field-local validation detail did not appear")
            pixel_entry.get_editable_text_iface().set_text_contents(original_pixel_text)
            wait_for(lambda: named(Atspi, "Enter a valid value", Atspi.Role.LABEL) is None,
                     "field-local validation detail did not clear")
            pixel_entry.get_editable_text_iface().set_text_contents("241.51")
            wait_for(lambda: any(name_of(node).startswith("Modified") for node in nodes(Atspi)),
                     "a valid entry edit did not mark the EDID as modified")
            press(Atspi, "Bytes")
            wait_for(lambda: named(Atspi, "Pixel clock · bytes 0x087–0x089", Atspi.Role.LABEL),
                     "the bytes view did not mark the edited field")
            press(Atspi, "Fields")
            activate_menu_item(Atspi, "Undo")

            press(Atspi, "Bytes")
            raw = wait_for(lambda: named(Atspi, "Selected group bytes"),
                           "byte view was not exposed")
            raw_before = text_of(Atspi, raw)
            assert "0084" in raw_before and "Hex bytes" in raw_before
            press(Atspi, "Timing")
            spin = wait_for(lambda: named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON),
                            "timing editor did not reopen")
            assert spin.get_value_iface().set_current_value(241503)
            activate_menu_item(Atspi, "Undo")
            wait_for(lambda: int(named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON)
                                 .get_value_iface().get_current_value()) == 241500,
                     "undo did not restore the pixel clock")
            activate_menu_item(Atspi, "Redo")
            wait_for(lambda: int(named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON)
                                 .get_value_iface().get_current_value()) == 241503,
                     "redo did not reapply the pixel clock")
            press(Atspi, "Bytes")
            raw = wait_for(lambda: named(Atspi, "Selected group bytes"),
                           "byte view was not exposed")
            assert text_of(Atspi, raw) != raw_before

            press(Atspi, "Save", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: target.read_bytes() != original, "save did not write edits")
            saved = target.read_bytes()
            assert len(saved) == len(original)
            assert all(sum(saved[index:index + 128]) % 256 == 0
                       for index in range(0, len(saved), 128))

            activate_menu_item(Atspi, "Compare with File…")
            wait_for(lambda: window_exists("Compare with EDID file"),
                     "Compare with File did not open a file dialog")
            press(Atspi, "Cancel", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: not window_exists("Compare with EDID file"),
                     "the compare file dialog did not close")

            activate_menu_item(Atspi, "Save As…")
            wait_for(lambda: window_exists("Save EDID binary"),
                     "Save As did not open a file dialog")
            press(Atspi, "Cancel", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: not window_exists("Save EDID binary"),
                     "Save As dialog did not close")

            # key events cannot reach the app here; check the registered bindings
            press(Atspi, "Main menu")
            for item, binding in (("Open…", "Control+O"), ("Save As…", "Shift+Control+S"),
                                  ("Undo", "Control+Z"), ("Redo", "Shift+Control+Z"),
                                  ("Keyboard Shortcuts", "Control+?")):
                node = wait_for(lambda: menu_item(Atspi, item), f"{item} was not listed")
                assert node.get_action_iface().get_key_binding(0).endswith(binding), \
                    f"{item} is not bound to {binding}"
            press(Atspi, "Main menu")

            activate_menu_item(Atspi, "About EDID Editor")
            wait_for(lambda: named(Atspi, "EDID Editor", Atspi.Role.LABEL),
                     "About dialog did not show the application name")
            close_dialog(Atspi, "About")

            activate_menu_item(Atspi, "EDID Log")
            wait_for(lambda: named(Atspi, "EDID log"), "the EDID Log did not show the log")
            assert actionable(Atspi, "Copy Log"), "the EDID Log offers no copy button"
            close_dialog(Atspi, "EDID Log")

            activate_menu_item(Atspi, "Keyboard Shortcuts")
            wait_for(lambda: named(Atspi, "Duplicate group"),
                     "keyboard shortcuts dialog did not list group shortcuts")
            wait_for(lambda: named(Atspi, "Search groups", Atspi.Role.LABEL) or
                     any(name_of(node) == "Search groups" and role_of(node) != Atspi.Role.ENTRY
                         for node in nodes(Atspi)),
                     "keyboard shortcuts dialog did not list the search shortcut")
            close_dialog(Atspi, "Keyboard Shortcuts")

            search = named(Atspi, "Search groups", Atspi.Role.ENTRY)
            search.get_editable_text_iface().set_text_contents("MND")
            wait_for(lambda: filtered_rows(Atspi) == 2,
                     "search did not narrow the list to the monitor name")
            select_group(Atspi, 1)
            press(Atspi, "Fields")
            wait_for(lambda: named(Atspi, "Monitor name", Atspi.Role.LABEL),
                     "read-only monitor name was not shown as text")
            assert named(Atspi, "Monitor name", Atspi.Role.TEXT) is None
            activate_menu_item(Atspi, "Edit Read-Only Fields")
            wait_for(lambda: named(Atspi, "Monitor name", Atspi.Role.TEXT),
                     "read-only field did not become editable")

            search.get_editable_text_iface().set_text_contents("")
            time.sleep(0.8)
            select_group(Atspi, 21)
            activate_menu_item(Atspi, "Extended Audio Block", menu="Add a group")
            press(Atspi, "Move group up (Alt+Up)")
            press(Atspi, "Fields")
            length = wait_for(lambda: named(Atspi, "Block length", Atspi.Role.TEXT),
                              "block length was not editable")
            length.get_editable_text_iface().set_text_contents("6")
            select_group(Atspi, 1)
            before_save = target.read_bytes()
            press(Atspi, "Save", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: target.read_bytes() != before_save,
                     "saving the longer audio block did not write the file")
            saved = target.read_bytes()
            # the 7-byte audio block, then the timing block that follows it
            assert saved[128 + 4:128 + 13] == \
                b"\x26\x79\x07\x20\x00\x00\x00\xf6\x22", \
                "the longer audio block was not saved at its new size"

            def audio_bytes():
                search.get_editable_text_iface().set_text_contents("ADB")
                wait_for(lambda: filtered_rows(Atspi) == 2,
                         "search did not narrow the list to the audio block")
                select_group(Atspi, 1)
                press(Atspi, "Bytes")
                raw = wait_for(lambda: named(Atspi, "Selected group bytes"),
                               "byte view was not exposed")
                text = text_of(Atspi, raw)
                search.get_editable_text_iface().set_text_contents("")
                return text
            assert "26 79 07 20 00 00 00" in audio_bytes(), \
                "a longer audio block was not rebuilt with its new size"
            activate_menu_item(Atspi, "Undo")
            assert "23 79 07 20" in audio_bytes(), "undo did not restore the block length"
        finally:
            stop_app(process)


def readonly(Atspi, app, fixture):
    with tempfile.TemporaryDirectory(prefix="wxedid-readonly-") as directory:
        target = Path(directory) / "readonly.bin"
        shutil.copyfile(fixture, target)
        target.chmod(0o444)
        original = target.read_bytes()
        process = launch_app(Atspi, app, str(target))
        try:
            wait_for(lambda: named(Atspi,
                                   "This file is read-only. Save a copy to keep your changes."),
                     "a read-only file was not explained")
            press(Atspi, "Save As…", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: window_exists("Save EDID binary"),
                     "the read-only notice did not offer Save As")
            press(Atspi, "Cancel", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: not window_exists("Save EDID binary"),
                     "Save As dialog did not close")
            select_group(Atspi, 21)
            spin = wait_for(lambda: named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON),
                            "timing editor did not open")
            assert spin.get_value_iface().set_current_value(241501)
            press(Atspi, "Save", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: window_exists("Save EDID binary"),
                     "saving a read-only file did not ask for a new file")
            assert target.read_bytes() == original
        finally:
            stop_app(process)


def hex_import(Atspi, app, fixture):
    with tempfile.TemporaryDirectory(prefix="wxedid-hex-") as directory:
        source = Path(directory) / "display.hex"
        data = Path(fixture).read_bytes()
        rows = [" ".join(f"{byte:02x}" for byte in data[index:index + 16])
                for index in range(0, len(data), 16)]
        source.write_text("\n".join(rows) + "\n")
        original = source.read_bytes()
        process = launch_app(Atspi, app, str(source))
        try:
            wait_for(lambda: any(name_of(node).startswith("Imported")
                                 for node in nodes(Atspi)),
                     "hex import was not marked as imported")
            wait_for(lambda: group_count(Atspi, "T7VTB") > 0,
                     "imported hex did not parse the CTA-861 block")
            for item, title in (("Export Hex…", "Export EDID as hex"),
                                ("Save Report…", "Save EDID report")):
                activate_menu_item(Atspi, item)
                wait_for(lambda: window_exists(title), f"{title} dialog did not open")
                press(Atspi, "Cancel", Atspi.Role.PUSH_BUTTON)
                wait_for(lambda: not window_exists(title), f"{title} dialog did not close")
            select_group(Atspi, 21)
            spin = wait_for(lambda: named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON),
                            "timing editor did not open")
            assert spin.get_value_iface().set_current_value(241501)
            press(Atspi, "Save", Atspi.Role.PUSH_BUTTON)
            wait_for(lambda: window_exists("Save EDID binary"),
                     "saving imported hex did not ask for a binary file")
            assert source.read_bytes() == original
        finally:
            stop_app(process)


def broken(Atspi, app, fixture):
    with tempfile.TemporaryDirectory(prefix="wxedid-broken-") as directory:
        target = Path(directory) / "broken.bin"
        data = bytearray(Path(fixture).read_bytes())
        data[126] += 1
        data[127] = (-sum(data[:127])) & 0xff
        target.write_bytes(bytes(data))
        process = launch_app(Atspi, app, str(target), title="EDID Editor")
        try:
            wait_for(lambda: named(Atspi, "Open Anyway"),
                     "broken EDID did not offer to open anyway")
            assert named(Atspi, "Block 0: Base EDID") is None
            press(Atspi, "Open Anyway")
            wait_for(lambda: named(Atspi, "Block 0: Base EDID"),
                     "opening anyway did not load the EDID")
            wait_for(lambda: group_count(Atspi, "T7VTB") > 0,
                     "opening anyway did not parse the present extension")
            wait_for(lambda: any(name_of(node).startswith("Modified")
                                 for node in nodes(Atspi)),
                     "the corrected block count was not marked as a change")
            wait_for(lambda: named(Atspi, "Notes") and
                     any("the block count now matches the data" in name_of(node)
                         for node in nodes(Atspi)),
                     "the overview did not note the corrected block count")
            assert named(Atspi, "Show details") is None, "the details panel is still offered"
        finally:
            stop_app(process)


def display(Atspi, app, fixture):
    process = launch_app(Atspi, app, fixture)
    try:
        def chooser(title):
            def outcome():
                if named(Atspi, "No display data found"):
                    return "none"
                dialog = named(Atspi, title, Atspi.Role.DIALOG)
                if dialog is None:
                    return None
                for row in walk(dialog):
                    if role_of(row) != Atspi.Role.LIST_ITEM:
                        continue
                    for node in walk(row):
                        if role_of(node) == Atspi.Role.PUSH_BUTTON:
                            return node
                return None
            return wait_for(outcome, f"{title} showed neither displays nor a notice")

        activate_menu_item(Atspi, "Open from Display…")
        found = chooser("Open from Display")
        if found == "none":
            press(Atspi, "Close", Atspi.Role.PUSH_BUTTON)
            return
        assert found.get_action_iface().do_action(0)
        wait_for(lambda: any(role_of(node) == Atspi.Role.FRAME and
                             name_of(node).startswith("card") for node in nodes(Atspi)),
                 "opening a display did not name the window after its connector")
        wait_for(lambda: group_count(Atspi, "BED") == 1,
                 "the display EDID was not parsed")

        # the display compared with itself, before and after an edit
        activate_menu_item(Atspi, "Compare with Display…")
        assert chooser("Compare with Display").get_action_iface().do_action(0)
        wait_for(lambda: named(Atspi, "No differences"),
                 "comparing a display with itself found differences")
        close_dialog(Atspi, "Compare")
        select_group(Atspi, 2)
        press(Atspi, "Fields")
        # the VESA bit is also the lowest interface type bit
        vesa = wait_for(lambda: named(Atspi, "VESA compatibility", Atspi.Role.SWITCH),
                        "the input group showed no VESA compatibility switch")
        assert vesa.get_action_iface().do_action(0)
        time.sleep(0.5)
        activate_menu_item(Atspi, "Compare with Display…")
        assert chooser("Compare with Display").get_action_iface().do_action(0)
        wait_for(lambda: named(Atspi, "2 differences"),
                 "comparing an edited display did not find the change")
        wait_for(lambda: named(Atspi, "VESA compatibility"),
                 "the changed field was not named")
        close_dialog(Atspi, "Compare")
    finally:
        stop_app(process)


def two_cta(Atspi, app, fixture):
    target = str(Path(fixture).with_name("sample_cea_eeodb.bin"))
    process = launch_app(Atspi, app, target)
    try:
        search = wait_for(lambda: named(Atspi, "Search groups", Atspi.Role.ENTRY),
                          "group search was not exposed")
        search.get_editable_text_iface().set_text_contents("ADB")
        wait_for(lambda: named(Atspi, "Block 2: CTA-861"),
                 "the second CTA-861 extension was not parsed")
        search.get_editable_text_iface().set_text_contents("1280x720p")
        wait_for(lambda: any(name_of(node).startswith("1280x720p") for node in nodes(Atspi)),
                 "a search did not open the group holding the match")
    finally:
        stop_app(process)


def recent(Atspi, app, fixture):
    name = Path(fixture).name
    process = launch_app(Atspi, app, fixture)
    try:
        select_group(Atspi, 6)
        press(Atspi, "Show Reserved Fields", Atspi.Role.PUSH_BUTTON)
        wait_for(lambda: named(Atspi, "Reserved0", Atspi.Role.SWITCH),
                 "showing reserved fields did not list them")
        press(Atspi, "Close", Atspi.Role.PUSH_BUTTON)
        process.wait(timeout=5)
    finally:
        stop_app(process)

    process = launch_app(Atspi, app, None)
    try:
        def open_button():
            for row in nodes(Atspi):
                if role_of(row) != Atspi.Role.LIST_ITEM:
                    continue
                inner = list(walk(row))
                if any(name_of(node) == name for node in inner):
                    for node in inner:
                        if role_of(node) == Atspi.Role.PUSH_BUTTON:
                            return node
            return None
        button = wait_for(open_button, "the start page did not list the recent file")
        assert button.get_action_iface().do_action(0)
        wait_for(lambda: any(role_of(node) == Atspi.Role.FRAME and name in name_of(node)
                             for node in nodes(Atspi)),
                 "opening a recent file did not load it")
        select_group(Atspi, 6)
        wait_for(lambda: named(Atspi, "Reserved0", Atspi.Role.SWITCH),
                 "showing reserved fields was not remembered")
    finally:
        stop_app(process)


def narrow(Atspi, app, fixture):
    # every page must fit: a wider page makes the window report its width
    process = launch_app(Atspi, app, fixture)
    try:
        if actionable(Atspi, "Show groups"):
            press(Atspi, "Show groups")
        select_group(Atspi, 15)
        wait_for(lambda: named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON),
                 "the timing page did not open")
        time.sleep(0.5)
        press(Atspi, "Fields")
        time.sleep(0.5)
        press(Atspi, "Bytes")
        time.sleep(0.5)
    finally:
        stop_app(process)


def inside(app, fixture, scenario):
    import gi
    gi.require_version("Atspi", "2.0")
    from gi.repository import Atspi
    # keep recent files and window state inside the session
    runtime = Path(os.environ["XDG_RUNTIME_DIR"])
    for variable, folder in (("XDG_DATA_HOME", "data"), ("XDG_STATE_HOME", "state"),
                             ("XDG_CONFIG_HOME", "config")):
        (runtime / folder).mkdir(exist_ok=True)
        os.environ[variable] = str(runtime / folder)
    time.sleep(1)
    if scenario == "functional":
        functional(Atspi, app, fixture)
    elif scenario == "readonly":
        readonly(Atspi, app, fixture)
    elif scenario == "hex":
        hex_import(Atspi, app, fixture)
    elif scenario == "broken":
        broken(Atspi, app, fixture)
    elif scenario == "two-cta":
        two_cta(Atspi, app, fixture)
    elif scenario == "display":
        display(Atspi, app, fixture)
    elif scenario == "recent":
        recent(Atspi, app, fixture)
    elif scenario == "narrow":
        narrow(Atspi, app, fixture)
    else:
        process = launch_app(Atspi, app, fixture)
        time.sleep(1)
        stop_app(process)
    (Path(os.environ["XDG_RUNTIME_DIR"]) / "scenario-passed").touch()


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "--inside":
        inside(sys.argv[2], sys.argv[3], sys.argv[4])
        return
    require_runtime()
    script = str(Path(__file__).resolve())
    app = str(Path(sys.argv[1]).resolve())
    fixture = str(Path(sys.argv[2]).resolve())
    run_session(script, app, fixture, 900, "functional")
    run_session(script, app, fixture, 900, "readonly")
    run_session(script, app, fixture, 900, "hex")
    run_session(script, app, fixture, 900, "broken")
    run_session(script, app, fixture, 900, "two-cta")
    run_session(script, app, fixture, 900, "display")
    run_session(script, app, fixture, 900, "recent")
    run_session(script, app, fixture, 360, "compact")
    run_session(script, app, fixture, 360, "narrow")
    run_session(script, app, fixture, 620, "narrow")


if __name__ == "__main__":
    main()
