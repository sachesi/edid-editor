# EDID Editor build and install tasks.
#
# `build` compiles a release build in build/; `install` only copies what is already
# built there, so the two can run as different users sharing this directory.
#
#   just build
#   sudo just install              (prefix /usr/local)
#   just prefix=$HOME/.local install

set shell := ["bash", "-euo", "pipefail", "-c"]

app_id := "io.github.sachesi.EdidEditor"
prefix := env("PREFIX", "/usr/local")
destdir := env("DESTDIR", "")
release := "build"
debug := "builddir"
bindir := destdir + prefix + "/bin"
datadir := destdir + prefix + "/share"

default:
    @just --list

# Release build.
build:
    [ -f {{release}}/build.ninja ] || meson setup {{release}} --buildtype=release
    meson compile -C {{release}}

# Debug build, with warnings treated as errors.
build-debug:
    [ -f {{debug}}/build.ninja ] || meson setup {{debug}} --werror
    meson compile -C {{debug}}

# Run the debug build uninstalled: just run [FILE]
run *args: build-debug
    {{debug}}/app/edid-editor {{args}}

# Debug build, desktop entry and AppStream metadata validation.
check: build-debug
    desktop-file-validate app/{{app_id}}.desktop
    appstreamcli validate --no-net --strict app/{{app_id}}.metainfo.xml

# Tests of the core and the metadata.
test: build-debug
    meson test -C {{debug}} --no-suite ui --print-errorlogs

# Tests of the interface, in a headless Weston session; a few minutes.
test-ui: build-debug
    meson test -C {{debug}} --suite ui --print-errorlogs

# Install the release build. Does not build: run `just build` first.
install:
    @test -x {{release}}/app/edid-editor || { echo "error: {{release}}/app/edid-editor missing; run 'just build' first" >&2; exit 1; }
    install -Dm755 {{release}}/app/edid-editor {{bindir}}/edid-editor
    install -Dm644 app/{{app_id}}.desktop {{datadir}}/applications/{{app_id}}.desktop
    install -Dm644 app/{{app_id}}.metainfo.xml {{datadir}}/metainfo/{{app_id}}.metainfo.xml
    install -Dm644 app/icons/{{app_id}}.svg {{datadir}}/icons/hicolor/scalable/apps/{{app_id}}.svg
    # Caches are left to the package manager when installing into DESTDIR.
    [ -n "{{destdir}}" ] || update-desktop-database -q {{datadir}}/applications || true
    [ -n "{{destdir}}" ] || gtk4-update-icon-cache -qtf {{datadir}}/icons/hicolor || gtk-update-icon-cache -qtf {{datadir}}/icons/hicolor || true
    @echo "installed to {{prefix}}"

uninstall:
    rm -f {{bindir}}/edid-editor
    rm -f {{datadir}}/applications/{{app_id}}.desktop {{datadir}}/metainfo/{{app_id}}.metainfo.xml
    rm -f {{datadir}}/icons/hicolor/scalable/apps/{{app_id}}.svg
    update-desktop-database -q {{datadir}}/applications || true
