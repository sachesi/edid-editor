edid_core_sources = [
  'EDID_base.cpp',
  'EDID_dsc.cpp',
  'EDID_main.cpp',
  'CEA.cpp',
  'CEA_ET.cpp',
  'DisplayID.cpp',
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

test_displayid = executable(
  'test-displayid',
  'tests/test_displayid.cpp',
  link_with : edid_core_lib,
  include_directories : [inc_core, include_directories('.')],
  install : false,
)

test_t7_timing = executable(
  'test-t7-timing',
  'tests/test_t7_timing.cpp',
  link_with : edid_core_lib,
  include_directories : [inc_core, include_directories('.')],
  install : false,
)

#synthetic EDID sample data (generated, checked in as build tests below)
sample_data = custom_target(
  'sample-edid-data',
  output : ['sample_base.bin', 'sample_cea.bin', 'sample_cea_displayid.bin',
            'sample_cea_t7.bin'],
  command : [find_program('python3'), files('tests/gen_sample_edid.py'), '@OUTDIR@'],
  build_by_default : true,
)

test('parse-dump', test_parse_dump, args : [sample_data[2]])
test('edit-roundtrip', test_edit_roundtrip, args : [sample_data[1]])
test('edit-roundtrip-displayid', test_edit_roundtrip, args : [sample_data[2]])
test('displayid-roundtrip', test_displayid,
     args : [sample_data[2], '--require-raw'])
test('reload-ownership', test_reload_ownership, args : [sample_data[1]])
test('t7-timing-roundtrip', test_t7_timing, args : [sample_data[3]])
