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
                                stderr=subprocess.STDOUT, timeout=55)
        stream.seek(0)
        diagnostics = stream.read()
    shutil.rmtree(runtime, ignore_errors=True)
    if result.returncode != 0:
        sys.stderr.write(diagnostics)
        raise SystemExit(result.returncode)
    rejected = ("Gtk-CRITICAL", "Adwaita-CRITICAL", "GLib-GObject-CRITICAL",
                "exceeds AdwApplicationWindow width")
    for marker in rejected:
        if marker in diagnostics:
            sys.stderr.write(diagnostics)
            raise AssertionError(f"runtime diagnostic: {marker}")


def walk(node):
    yield node
    for index in range(node.get_child_count()):
        yield from walk(node.get_child_at_index(index))


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


def named(Atspi, name, role=None):
    for node in nodes(Atspi):
        if node.get_name() == name and (role is None or node.get_role() == role):
            return node
    return None


def shortcut(keys):
    subprocess.run(["xdotool", "key", keys], check=True)
    time.sleep(0.5)


def launch_app(Atspi, app, path):
    env = os.environ.copy()
    env.update({"GDK_BACKEND": "x11", "GSK_RENDERER": "cairo", "GTK_A11Y": "atspi"})
    process = subprocess.Popen([app, path], env=env, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    wait_for(lambda: any(node.get_role() == Atspi.Role.FRAME and
                         Path(path).name in node.get_name()
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
            search = wait_for(lambda: named(Atspi, "Search groups", Atspi.Role.ENTRY),
                              "group search was not exposed")
            search.get_editable_text_iface().set_text_contents("T7VTB")
            wait_for(lambda: any("T7VTB" in node.get_name() for node in nodes(Atspi)),
                     "matching group disappeared during search")
            search.get_editable_text_iface().set_text_contents("no-such-edid-group")
            wait_for(lambda: named(Atspi, "No matching groups"),
                     "empty search state did not appear")
            search.get_editable_text_iface().set_text_contents("")

            lists = [node for node in nodes(Atspi) if node.get_role() == Atspi.Role.LIST]
            assert lists and lists[-1].get_selection_iface().select_child(21)
            time.sleep(0.5)
            spin = wait_for(lambda: named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON),
                            "timing editor did not open")
            assert int(spin.get_value_iface().get_current_value()) == 241500
            byte_button = named(Atspi, "Bytes")
            assert byte_button is not None and byte_button.get_action_iface().do_action(0)
            raw = wait_for(lambda: named(Atspi, "Selected group bytes"),
                           "byte view was not exposed")
            text_iface = raw.get_text_iface()
            raw_before = text_iface.get_text(0, text_iface.get_character_count())
            timing_button = named(Atspi, "Timing")
            assert timing_button is not None and timing_button.get_action_iface().do_action(0)
            spin = wait_for(lambda: named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON),
                            "timing editor did not reopen")
            spin.get_component_iface().grab_focus()
            for value in (241501, 241502, 241503):
                assert spin.get_value_iface().set_current_value(value)
            menu = named(Atspi, "Main menu")
            assert menu is not None and menu.get_component_iface().grab_focus()
            shortcut("ctrl+z")
            assert int(named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON)
                       .get_value_iface().get_current_value()) == 241500
            shortcut("ctrl+shift+z")
            assert int(named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON)
                       .get_value_iface().get_current_value()) == 241503

            byte_button = named(Atspi, "Bytes")
            assert byte_button is not None and byte_button.get_action_iface().do_action(0)
            raw = wait_for(lambda: named(Atspi, "Selected group bytes"),
                           "byte view was not exposed")
            text_iface = raw.get_text_iface()
            raw_text = text_iface.get_text(0, text_iface.get_character_count())
            assert "0084" in raw_text and "Hex bytes" in raw_text
            assert raw_text != raw_before

            shortcut("ctrl+s")
            wait_for(lambda: target.read_bytes() != original, "save did not write edits")
            saved = target.read_bytes()
            assert len(saved) == len(original)
            assert all(sum(saved[index:index + 128]) % 256 == 0
                       for index in range(0, len(saved), 128))

            shortcut("ctrl+shift+s")
            result = subprocess.run(["xdotool", "search", "--name", "Save EDID binary"],
                                    capture_output=True)
            assert result.returncode == 0
            shortcut("Escape")

            menu = named(Atspi, "Main menu")
            assert menu is not None and menu.get_action_iface().do_action(0)
            about = wait_for(lambda: named(Atspi, "About EDID Editor"),
                             "About menu item did not appear")
            assert about.get_action_iface().do_action(0)
            wait_for(lambda: named(Atspi, "About EDID Editor", Atspi.Role.DIALOG),
                     "About dialog did not open")
            shortcut("Escape")
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
            lists = [node for node in nodes(Atspi) if node.get_role() == Atspi.Role.LIST]
            assert lists and lists[-1].get_selection_iface().select_child(21)
            spin = wait_for(lambda: named(Atspi, "Pixel clock", Atspi.Role.SPIN_BUTTON),
                            "timing editor did not open")
            assert spin.get_value_iface().set_current_value(241501)
            shortcut("ctrl+s")
            result = subprocess.run(["xdotool", "search", "--name", "Save EDID binary"],
                                    capture_output=True)
            assert result.returncode == 0
            assert target.read_bytes() == original
        finally:
            stop_app(process)


def inside(app, fixture, scenario):
    import gi
    gi.require_version("Atspi", "2.0")
    from gi.repository import Atspi
    time.sleep(1)
    if scenario == "functional":
        functional(Atspi, app, fixture)
    elif scenario == "readonly":
        readonly(Atspi, app, fixture)
    else:
        process = launch_app(Atspi, app, fixture)
        time.sleep(1)
        stop_app(process)


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
    run_session(script, app, fixture, 360, "compact")


if __name__ == "__main__":
    main()
