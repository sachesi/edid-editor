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
