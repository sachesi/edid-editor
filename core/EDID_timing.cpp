/***************************************************************
 * Name:      EDID_timing.cpp
 * Purpose:   the fields of detailed timings, and the rate they give
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include <algorithm>
#include <cmath>
#include <cstring>

#include "CEA_ET_class.h"
#include "EDID_timing.h"

bool edid_timing_layout_of(edi_grp_cl* group, edid_timing_layout& layout) {
   static const int dtd_fields[TIMING_FIELD_COUNT] = {
      DTD_IDX_PIXCLK, DTD_IDX_HAPIX, DTD_IDX_HBPIX, DTD_IDX_VALIN,
      DTD_IDX_VBLIN, DTD_IDX_HSOFFS, DTD_IDX_HSWIDTH, DTD_IDX_VSOFFS,
      DTD_IDX_VSWIDTH, DTD_IDX_HBORD, DTD_IDX_VBORD,
   };
   static const int displayid_type1_fields[TIMING_FIELD_COUNT] = {
      0, 5, 6, 10, 11, 7, 8, 12, 13, -1, -1,
   };
   static const int t7_fields[TIMING_FIELD_COUNT] = {
      T7F_IDX_PIXCLK, T7F_IDX_HAPIX, T7F_IDX_HBPIX, T7F_IDX_VALIN,
      T7F_IDX_VBLIN, T7F_IDX_HSOFFS, T7F_IDX_HSWIDTH, T7F_IDX_VSOFFS,
      T7F_IDX_VSWIDTH, -1, -1,
   };

   if (group == NULL) return false;
   const int* fields = NULL;
   const char* code = group->CodeName.c_str();
   layout.clock_step = 1.0;
   layout.clock_max = 0.0;
   if (0 == strcmp(code, "DTD")) {
      fields = dtd_fields;
      layout.pixel_hz = 10000.0;
   } else if (0 == strcmp(code, "DID-T1")) {
      //the field reads and writes kHz of a clock stored in 10 kHz units
      fields = displayid_type1_fields;
      layout.pixel_hz = 1000.0;
      layout.clock_step = 10.0;
      layout.clock_max = 167772160.0;
   } else if (0 == strcmp(code, "DID-T7")) {
      //Type VII shares the Type I layout
      fields = displayid_type1_fields;
      layout.pixel_hz = 1000.0;
      layout.clock_max = 16777216.0;
   } else if (0 == strcmp(code, "T7VTB")) {
      fields = t7_fields;
      layout.pixel_hz = 1000.0;
   } else {
      return false;
   }
   std::copy(fields, fields + TIMING_FIELD_COUNT, layout.fields);
   for (int idx=0; idx<TIMING_FIELD_COUNT; idx++) {
      if (layout.fields[idx] >= static_cast<int>(group->FieldsAr.GetCount())) return false;
   }
   return true;
}

double edid_timing_clock_for(const edid_timing_layout& layout, double refresh,
                             double htotal, double vtotal) {
   const double units = refresh * htotal * vtotal / layout.pixel_hz;
   return std::max(layout.clock_step,
                   std::round(units / layout.clock_step) * layout.clock_step);
}
