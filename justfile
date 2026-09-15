# EDID Editor build and install tasks.
#
# `build` compiles a release build in build/; `install` only copies what is already
# built there, so the two can run as different users sharing this directory.
#
#   just build
#   sudo just install              (prefix /usr/local)
#   just prefix=$HOME/.local install
#   just gui=disabled build        (edid-editor-cli only, without GTK)
#
# Installing into DESTDIR leaves the icon and desktop caches to the package manager.

set shell := ["bash", "-euo", "pipefail", "-c"]

app_id := "io.github.sachesi.EdidEditor"
prefix := env("PREFIX", "/usr/local")
destdir := env("DESTDIR", "")
gui := env("GUI", "auto")
release := "build"
debug := "builddir"
bindir := destdir + prefix + "/bin"
datadir := destdir + prefix + "/share"
mandir := datadir + "/man/man1"

default:
    @just --list

# Release build; the application is left out with gui=disabled, or when GTK is missing.
build:
    if [ -f {{release}}/build.ninja ]; then meson configure {{release}} -Dgui={{gui}}; else meson setup {{release}} --buildtype=release -Dgui={{gui}}; fi
    meson compile -C {{release}}

# Debug build, with warnings treated as errors.
build-debug:
    if [ -f {{debug}}/build.ninja ]; then meson configure {{debug}} -Dgui={{gui}}; else meson setup {{debug}} --werror -Dgui={{gui}}; fi
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

# Install the release build, with the application when it was built. Does not build.
install:
    @test -x {{release}}/cli/edid-editor-cli || { echo "error: {{release}}/cli/edid-editor-cli missing; run 'just build' first" >&2; exit 1; }
    install -Dm755 {{release}}/cli/edid-editor-cli {{bindir}}/edid-editor-cli
    install -Dm644 docs/edid-editor-cli.1 {{mandir}}/edid-editor-cli.1
    install -Dm644 cli/completions/edid-editor-cli.bash {{datadir}}/bash-completion/completions/edid-editor-cli
    install -Dm644 cli/completions/_edid-editor-cli {{datadir}}/zsh/site-functions/_edid-editor-cli
    install -Dm644 cli/completions/edid-editor-cli.fish {{datadir}}/fish/vendor_completions.d/edid-editor-cli.fish
    if [ -x {{release}}/app/edid-editor ]; then \
        install -Dm755 {{release}}/app/edid-editor {{bindir}}/edid-editor; \
        install -Dm644 docs/edid-editor.1 {{mandir}}/edid-editor.1; \
        install -Dm644 app/{{app_id}}.desktop {{datadir}}/applications/{{app_id}}.desktop; \
        install -Dm644 app/{{app_id}}.metainfo.xml {{datadir}}/metainfo/{{app_id}}.metainfo.xml; \
        install -Dm644 app/icons/{{app_id}}.svg {{datadir}}/icons/hicolor/scalable/apps/{{app_id}}.svg; \
        if [ -z "{{destdir}}" ]; then \
            update-desktop-database -q {{datadir}}/applications || true; \
            gtk4-update-icon-cache -qtf {{datadir}}/icons/hicolor || gtk-update-icon-cache -qtf {{datadir}}/icons/hicolor || true; \
        fi; \
    fi
    @echo "installed to {{prefix}}"

uninstall:
    rm -f {{bindir}}/edid-editor {{bindir}}/edid-editor-cli
    rm -f {{mandir}}/edid-editor.1 {{mandir}}/edid-editor-cli.1
    rm -f {{datadir}}/bash-completion/completions/edid-editor-cli
    rm -f {{datadir}}/zsh/site-functions/_edid-editor-cli
    rm -f {{datadir}}/fish/vendor_completions.d/edid-editor-cli.fish
    rm -f {{datadir}}/applications/{{app_id}}.desktop {{datadir}}/metainfo/{{app_id}}.metainfo.xml
    rm -f {{datadir}}/icons/hicolor/scalable/apps/{{app_id}}.svg
    update-desktop-database -q {{datadir}}/applications || true
