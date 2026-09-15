#!/usr/bin/env python3
# Completes command lines in bash, zsh and fish with the scripts in
# cli/completions; a shell that isn't installed is skipped.
import os
import pty
import re
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

cli, scripts, sample = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), sys.argv[3]
work = Path(tempfile.mkdtemp(prefix="edid-editor-completion-"))
(work / "monitor.bin").write_bytes(Path(sample).read_bytes())
sample = str(work / "monitor.bin")
env = dict(os.environ, PATH=f"{cli.parent}{os.pathsep}{os.environ['PATH']}",
           HOME=str(work), XDG_CONFIG_HOME=str(work), XDG_DATA_HOME=str(work),
           TERM="xterm", LC_ALL="C.UTF-8")
env.pop("ZDOTDIR", None)
failures = 0

# the line typed, then what Tab should leave on it
cases = [
    ("edid-editor-cli dup", "edid-editor-cli duplicate "),
    ("edid-editor-cli --js", "edid-editor-cli --json "),
    (f"edid-editor-cli info {work}/mon", f"edid-editor-cli info {sample} "),
    (f"edid-editor-cli set {sample} -o {work}/mon", f"edid-editor-cli set {sample} -o {sample} "),
    (f"edid-editor-cli set {sample} MN", f"edid-editor-cli set {sample} MND "),
    (f"edid-editor-cli --json fields {sample} MN", f"edid-editor-cli --json fields {sample} MND "),
    (f"edid-editor-cli set {sample} DTD:1 interla", f"edid-editor-cli set {sample} DTD:1 interlaced="),
    (f"edid-editor-cli set {sample} DTD:1 interlaced=of",
     f"edid-editor-cli set {sample} DTD:1 interlaced=off "),
    (f"edid-editor-cli set {sample} VID color-depth=10",
     f"edid-editor-cli set {sample} VID color-depth=10\\ bits "),
    (f"edid-editor-cli get {sample} CHD ycbcr-4:2:", f"edid-editor-cli get {sample} CHD ycbcr-4:2:2 "),
    (f"edid-editor-cli add {sample} 1 audio-l", f"edid-editor-cli add {sample} 1 audio-lpcm "),
    (f"edid-editor-cli move {sample} VSD u", f"edid-editor-cli move {sample} VSD up "),
]


def interactive(argv, setup, line):
    """Types line and Tab into an interactive shell; Ctrl-T prints the result."""
    pid, fd = pty.fork()
    if pid == 0:
        os.execvpe(argv[0], argv, env)
    output = b""

    def read_until(pattern, timeout=15):
        nonlocal output
        end = time.time() + timeout
        while time.time() < end:
            match = re.search(pattern, output)
            if match:
                return match
            ready, _, _ = select.select([fd], [], [], 0.2)
            if ready:
                try:
                    output += os.read(fd, 65536)
                except OSError:
                    break
        return None

    try:
        os.write(fd, setup.encode() + b"\n")
        if not read_until(rb"RE ADY"):
            return None, output.decode(errors="replace")
        output = b""
        os.write(fd, line.encode() + b"\t")
        time.sleep(1.0)
        os.write(fd, b"\x14")
        match = read_until(rb"<<(.*?)>>")
        return (match.group(1).decode() if match else None), output.decode(errors="replace")
    finally:
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
        os.close(fd)


def check(shell, line, wanted, got, log=""):
    global failures
    if got == wanted:
        print(f"PASS: {shell}: {line!r}")
    else:
        failures += 1
        print(f"FAIL: {shell}: {line!r} gave {got!r}, expected {wanted!r}\n{log[-2000:]}")


def bash():
    framework = Path("/usr/share/bash-completion/bash_completion")
    if not shutil.which("bash") or not framework.exists():
        print("SKIP: bash with bash-completion is not installed")
        return
    setup = (f"PS1='$ '; source {framework}; source {scripts}/edid-editor-cli.bash; "
             "bind 'set bell-style none'; "
             "bind -x '\"\\C-t\": printf \"\\n<<%s>>\\n\" \"$READLINE_LINE\"'; echo RE' 'ADY")
    for line, wanted in cases:
        got, log = interactive(["bash", "--norc", "--noprofile", "-i"], setup, line)
        check("bash", line, wanted, got, log)


def zsh():
    if not shutil.which("zsh"):
        print("SKIP: zsh is not installed")
        return
    setup = (f"PS1='$ '; fpath=({scripts} $fpath); autoload -Uz compinit; "
             f"compinit -u -d {work}/zcompdump; unsetopt beep; "
             "show() { print -r -- $'\\n'\"<<$BUFFER>>\" }; zle -N show; bindkey '^T' show; "
             "echo RE' 'ADY")
    for line, wanted in cases:
        got, log = interactive(["zsh", "-f", "-i"], setup, line)
        check("zsh", line, wanted, got, log)


def fish():
    if not shutil.which("fish"):
        print("SKIP: fish is not installed")
        return
    for line, wanted in cases:
        # fish lists the candidates for the last word; Tab takes the only one
        result = subprocess.run(
            ["fish", "--no-config", "-c",
             f"set -p fish_complete_path {scripts}; complete -C $argv[1]", line],
            capture_output=True, env=env, text=True)
        candidates = [entry.split("\t")[0] for entry in result.stdout.splitlines()]
        head, _, last = line.rpartition(" ")
        word = wanted[len(head) + 1:].rstrip(" ").replace("\\ ", " ")
        got = wanted if candidates == [word] else f"{candidates} for {last}"
        check("fish", line, wanted, got, result.stderr)


bash()
zsh()
fish()
shutil.rmtree(work, ignore_errors=True)
sys.exit(1 if failures else 0)
