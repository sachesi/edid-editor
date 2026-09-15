#the application icon, found through the resource base path when the
#program runs from the build directory
app_resources = import('gnome').compile_resources(
  'edid-editor-resources',
  'edid-editor.gresource.xml',
  c_name : 'edid_editor',
)

app_sources = [
  'main.cpp',
  'window.cpp',
  'document.cpp',
  'fields.cpp',
  'history.cpp',
  'overview.cpp',
  'sidebar.cpp',
  'timing.cpp',
  app_resources,
]

app_deps = [
  gtk_dep,
  adw_dep,
]

version_config = configuration_data()
version_config.set_quoted('WXEDID_VERSION', meson.project_version())
configure_file(
  output : 'wxedid-config.h',
  configuration : version_config,
)

edid_editor = executable(
  'edid-editor',
  app_sources,
  include_directories : [inc_core, include_directories('../core')],
  dependencies : app_deps,
  link_with : edid_core_lib,
  install : true,
)

install_data(
  'io.github.sachesi.EdidEditor.desktop',
  install_dir : get_option('datadir') / 'applications',
)

install_data(
  'io.github.sachesi.EdidEditor.metainfo.xml',
  install_dir : get_option('datadir') / 'metainfo',
)

install_data(
  'icons/io.github.sachesi.EdidEditor.svg',
  install_dir : get_option('datadir') / 'icons/hicolor/scalable/apps',
)

install_man(files('../docs/edid-editor.1'))

desktop_file_validate = find_program('desktop-file-validate', required : false)
if desktop_file_validate.found()
  test(
    'desktop metadata',
    desktop_file_validate,
    args : files('io.github.sachesi.EdidEditor.desktop'),
    suite : 'release',
  )
endif

appstreamcli = find_program('appstreamcli', required : false)
if appstreamcli.found()
  test(
    'AppStream metadata',
    appstreamcli,
    args : ['validate', '--no-net', '--strict',
            files('io.github.sachesi.EdidEditor.metainfo.xml')],
    suite : 'release',
  )
endif

ui_test = find_program('tests/test_ui.py')
test(
  'GTK workflow',
  ui_test,
  args : [edid_editor, sample_data[3]],
  suite : 'ui',
  timeout : 900,
  is_parallel : false,
)
