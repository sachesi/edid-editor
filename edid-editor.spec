%define _debugsource_template %{nil}
%define debug_package %{nil}

%global app_id io.github.sachesi.EdidEditor

Name:           edid-editor
# The release workflow sets Version to the tag it builds; OBS counts the Release.
Version:        0.4.0
Release:        0
Summary:        EDID editor for GNOME, with a command line interface

License:        GPL-3.0-or-later
URL:            https://github.com/sachesi/edid-editor
# Named as the Debian source package names it, which OBS builds from the same file.
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz#/%{name}_%{version}.orig.tar.gz

BuildRequires:  meson
BuildRequires:  ninja
BuildRequires:  gcc-c++
# Generates the sample EDID data the tests read.
BuildRequires:  python3
BuildRequires:  gettext-tools
BuildRequires:  desktop-file-utils
BuildRequires:  AppStream
BuildRequires:  pkgconfig(gtk4) >= 4.10
BuildRequires:  pkgconfig(libadwaita-1) >= 1.8

Requires:       libgtk-4-1 >= 4.10
Requires:       libadwaita-1-0 >= 1.8
Requires:       hicolor-icon-theme

%description
EDID Editor inspects and edits EDID, the data a display sends to describe
itself: its name, size, timings, refresh ranges, color and HDR support, and
audio formats. It reads the base block, CTA-861 extensions and DisplayID 1.x and
2.x blocks from files or from a connected display, and never writes to the
display itself. It is written with GTK 4 and libadwaita, and is based on wxEDID
by Tomasz Pawlak.

edid-editor-cli reads, edits, compares and converts EDID data from a terminal
with the same code, with a manual page and completions for bash, zsh and fish.

%prep
%autosetup -n %{name}-%{version}

%build
%meson -Dgui=enabled
%meson_build

%install
%meson_install

%check
# The ui suite needs a Wayland compositor.
%meson_test --no-suite ui

%files
%license LICENSE
%doc README.md AUTHORS docs/usage.md docs/cli.md
%{_bindir}/edid-editor
%{_bindir}/edid-editor-cli
%{_datadir}/applications/%{app_id}.desktop
%{_datadir}/metainfo/%{app_id}.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/%{app_id}.svg
%{_mandir}/man1/edid-editor.1*
%{_mandir}/man1/edid-editor-cli.1*
%{_datadir}/bash-completion/completions/edid-editor-cli
%{_datadir}/zsh/site-functions/_edid-editor-cli
# openSUSE wants every directory owned; fish is not required here.
%dir %{_datadir}/fish
%dir %{_datadir}/fish/vendor_completions.d
%{_datadir}/fish/vendor_completions.d/edid-editor-cli.fish

%changelog
