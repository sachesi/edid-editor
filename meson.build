project('edid-editor', 'cpp', 'c',
  version : '0.3.0',
  default_options : [
    'cpp_std=c++17',
    'warning_level=2',
    'b_ndebug=if-release',
  ],
)

gtk_dep = dependency('gtk4', version : '>=4.10', required : get_option('gui'))
adw_dep = dependency('libadwaita-1', version : '>=1.8', required : get_option('gui'))

inc_core = include_directories('core')

subdir('core')
subdir('cli')

if gtk_dep.found() and adw_dep.found()
  subdir('app')
  subdir('po')

  gnome = import('gnome')

  gnome.post_install(
    glib_compile_schemas : false,
    gtk_update_icon_cache : true,
    update_desktop_database : true,
  )
endif
