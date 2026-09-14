project('edid-editor', 'cpp', 'c',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++17',
    'warning_level=2',
    'b_ndebug=if-release',
  ],
)

gtk_dep = dependency('gtk4', version : '>=4.10')
adw_dep = dependency('libadwaita-1', version : '>=1.5')

inc_core = include_directories('core')

subdir('core')
subdir('app')

gnome = import('gnome')

gnome.post_install(
  glib_compile_schemas : false,
  gtk_update_icon_cache : true,
  update_desktop_database : true,
)
