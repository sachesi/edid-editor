edid_core_sources = [
  'EDID_base.cpp',
  'EDID_dsc.cpp',
  'EDID_main.cpp',
  'EDID_text.cpp',
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

test_structure_edit = executable(
  'test-structure-edit',
  'tests/test_structure_edit.cpp',
  link_with : edid_core_lib,
  include_directories : [inc_core, include_directories('.')],
  install : false,
)

test_text = executable(
  'test-text',
  'tests/test_text.cpp',
  link_with : edid_core_lib,
  include_directories : [inc_core, include_directories('.')],
  install : false,
)

test_corpus = executable(
  'test-corpus',
  'tests/test_corpus.cpp',
  link_with : edid_core_lib,
  include_directories : [inc_core, include_directories('.')],
  install : false,
)

#synthetic EDID sample data (generated, checked in as build tests below)
sample_data = custom_target(
  'sample-edid-data',
  output : ['sample_base.bin', 'sample_cea.bin', 'sample_cea_displayid.bin',
            'sample_cea_t7.bin', 'sample_cea_audio.bin',
            'sample_displayid_compact.bin',
            'sample_displayid_short_padding.bin'],
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
test('structure-edit', test_structure_edit,
     args : [sample_data[2], sample_data[5], sample_data[6]])
#real-world EDIDs, see tests/corpus/README
corpus_conformant = files(
  'tests/corpus/acer-acr0450.hex',
  'tests/corpus/aoc-aoc2401.hex',
  'tests/corpus/apple-app9219.hex',
  'tests/corpus/apple-appa044.hex',
  'tests/corpus/apple-appae11.hex',
  'tests/corpus/au-optronics-auo019c.hex',
  'tests/corpus/au-optronics-auo07b2.hex',
  'tests/corpus/au-optronics-auofcb7.hex',
  'tests/corpus/dell-del430f.hex',
  'tests/corpus/nec-nec6948.hex',
  'tests/corpus/panasonic-meia296.hex',
  'tests/corpus/samsung-sam756f.hex',
  'tests/corpus/tcl-tcl0000.hex',
  'tests/corpus/xiaomi-xmd0076.hex',
  'tests/corpus/xiaomi-xmd00e1.hex',
)
corpus_nonconformant = files(
  'tests/corpus/acer-acr03e1.hex',
  'tests/corpus/apple-appae19.hex',
  'tests/corpus/chimei-innolux-cmn1540.hex',
  'tests/corpus/dell-deld082.hex',
  'tests/corpus/vizio-viz1002.hex',
)
test('corpus-roundtrip', test_corpus,
     args : [sample_data[0], sample_data[1], sample_data[2], sample_data[3],
             sample_data[4], sample_data[5], sample_data[6],
             corpus_conformant, '--ignore-errors', corpus_nonconformant])
test('hex-text-report', test_text, args : [sample_data[2]])
