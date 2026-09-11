app_sources = [
  'main.cpp',
  'window.cpp',
]

app_deps = [
  gtk_dep,
  adw_dep,
]

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
