/***************************************************************
 * Name:      EDID_timing.h
 * Purpose:   the fields of detailed timings, and the rate they give
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_TIMING_H
#define EDID_TIMING_H 1

#include "EDID_class.h"

enum timing_field {
   TIMING_PIXCLK,
   TIMING_HACTIVE,
   TIMING_HBLANK,
   TIMING_VACTIVE,
   TIMING_VBLANK,
   TIMING_HOFFSET,
   TIMING_HWIDTH,
   TIMING_VOFFSET,
   TIMING_VWIDTH,
   TIMING_HBORDER,
   TIMING_VBORDER,
   TIMING_FIELD_COUNT,
};

struct edid_timing_layout {
   int    fields[TIMING_FIELD_COUNT]; //index in the group's fields, -1 when absent
   double pixel_hz;   //Hz per unit of the pixel clock value the field reads and writes
   double clock_step; //the pixel clock value changes in steps of this many units
   double clock_max;  //largest pixel clock value; 0 when the field's maximum applies
};

//The layout of a detailed timing: a DTD, a DisplayID Type I or Type VII
//timing, or a CTA-861 Type VII timing. Returns false for other groups.
bool edid_timing_layout_of(edi_grp_cl* group, edid_timing_layout& layout);

//The pixel clock value, in the units of the layout and rounded to its step,
//that comes closest to the refresh rate with the given totals.
double edid_timing_clock_for(const edid_timing_layout& layout, double refresh,
                             double htotal, double vtotal);

#endif
