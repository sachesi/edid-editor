#the command line interface needs only the core
edid_editor_cli = executable(
  'edid-editor-cli',
  'main.cpp',
  include_directories : [inc_core, include_directories('../core')],
  cpp_args : ['-DEDID_EDITOR_VERSION="@0@"'.format(meson.project_version())],
  link_with : edid_core_lib,
  install : true,
)

test(
  'command line',
  find_program('tests/test_cli.py'),
  args : [edid_editor_cli, sample_data[1], sample_data[2]],
)
