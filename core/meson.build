edid_core_sources = [
  'EDID_base.cpp',
  'EDID_dsc.cpp',
  'EDID_main.cpp',
  'CEA.cpp',
  'CEA_ET.cpp',
  'grpar.cpp',
  'vmap.cpp',
  'vid_fmt.cpp',
  'guilog.cpp',
  'wxedid_rcd_scope.cpp',
]

edid_core_lib = static_library(
  'edid-core',
  edid_core_sources,
  include_directories : [inc_core, include_directories('.')],
  install : false,
)

#headless smoke test: parse an EDID binary and dump the group tree
test_parse_dump = executable(
  'test-parse-dump',
  'tests/test_parse_dump.cpp',
  link_with : edid_core_lib,
  include_directories : [inc_core, include_directories('.')],
  install : false,
)

#headless edit-path test: write fields, assemble, checksum, verify
test_edit_roundtrip = executable(
  'test-edit-roundtrip',
  'tests/test_edit_roundtrip.cpp',
  link_with : edid_core_lib,
  include_directories : [inc_core, include_directories('.')],
  install : false,
)

test_reload_ownership = executable(
  'test-reload-ownership',
  'tests/test_reload_ownership.cpp',
  link_with : edid_core_lib,
  include_directories : [inc_core, include_directories('.')],
  install : false,
)

#synthetic EDID sample data (generated, checked in as build tests below)
sample_data = custom_target(
  'sample-edid-data',
  output : ['sample_base.bin', 'sample_cea.bin', 'sample_cea_displayid.bin'],
  command : [find_program('python3'), files('tests/gen_sample_edid.py'), '@OUTDIR@'],
  build_by_default : true,
)

test('parse-dump', test_parse_dump, args : [sample_data[1]])
test('edit-roundtrip', test_edit_roundtrip, args : [sample_data[1]])
test('edit-roundtrip-raw-extension', test_edit_roundtrip, args : [sample_data[2]])
test('reload-ownership', test_reload_ownership, args : [sample_data[1]])
