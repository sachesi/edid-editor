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

test(
  'shell completion',
  find_program('tests/test_completion.py'),
  args : [edid_editor_cli, meson.current_source_dir() / 'completions', sample_data[1]],
  timeout : 300,
)

install_man(files('../docs/edid-editor-cli.1'))

install_data(
  'completions/edid-editor-cli.bash',
  rename : 'edid-editor-cli',
  install_dir : get_option('datadir') / 'bash-completion' / 'completions',
)

install_data(
  'completions/_edid-editor-cli',
  install_dir : get_option('datadir') / 'zsh' / 'site-functions',
)

install_data(
  'completions/edid-editor-cli.fish',
  install_dir : get_option('datadir') / 'fish' / 'vendor_completions.d',
)
