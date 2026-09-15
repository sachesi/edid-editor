/***************************************************************
 * Name:      EDID_timing.h
 * Purpose:   the fields of detailed timings, and the rate they give
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_TIMING_H
#define EDID_TIMING_H 1

#include <string>
#include <vector>

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
   TIMING_HSIZE,   //image size in mm
   TIMING_VSIZE,
   TIMING_FIELD_COUNT,
};

//single bits of a timing
enum timing_flag {
   TIMING_INTERLACED,
   TIMING_HSYNC_POSITIVE,
   TIMING_VSYNC_POSITIVE,
   TIMING_PREFERRED,   //DisplayID only; the first DTD is preferred by its place
   TIMING_FLAG_COUNT,
};

struct edid_timing_layout {
   int    fields[TIMING_FIELD_COUNT]; //index in the group's fields, -1 when absent
   int    flags[TIMING_FLAG_COUNT];   //index in the group's fields, -1 when absent
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

//A timing in plain numbers, whatever its format
struct edid_timing_values {
   double pixel_hz;
   u32_t  value[TIMING_FIELD_COUNT]; //value[TIMING_PIXCLK] is not used
   bool   flag[TIMING_FLAG_COUNT];
   bool   has[TIMING_FIELD_COUNT];   //the format has the field
};

bool edid_timing_read(EDID_cl& EDID, edi_grp_cl* group, edid_timing_values& values);

//Writes the values into a timing of any format: fields the format lacks are left
//out, as is the image size where the values have none. False, with the timing
//unchanged, when a value doesn't fit the format.
bool edid_timing_write(EDID_cl& EDID, edi_grp_cl* group, const edid_timing_values& values);

//One timing into another, in the other's format; not preferred there.
bool edid_timing_copy(EDID_cl& EDID, edi_grp_cl* from, edi_grp_cl* to);

//The DTD at the start of the base block's descriptors, if it is a timing;
//every system takes it as the preferred timing.
edi_grp_cl* edid_first_timing(EDID_cl& EDID);

struct edid_mode {
   edi_grp_cl* group;
   u32_t       block;
   u32_t       width;
   u32_t       height;
   double      refresh;
   bool        interlaced;
   bool        preferred; //the first timing, or flagged in DisplayID
};

//Every detailed timing, in the order of the EDID
std::vector<edid_mode> edid_modes(EDID_cl& EDID);

//The mode Linux lists first: preferred before others, then the largest, then
//the fastest. Returns modes.size() when there are none.
size_t edid_default_mode(const std::vector<edid_mode>& modes);

//New contents of a group's data
struct edid_data_change {
   edi_grp_cl*       group;
   std::vector<u8_t> before;
   std::vector<u8_t> after;
};

//What makes a timing the preferred one, without applying it: the timing and
//the first timing change places, converted between their formats, and DisplayID
//timings lose their preferred flag. A timing the first place can't hold is
//flagged preferred in DisplayID instead. The message says what happens.
bool edid_plan_preferred(EDID_cl& EDID, edi_grp_cl* timing,
                         std::vector<edid_data_change>& changes, std::string& message);

void edid_apply_changes(const std::vector<edid_data_change>& changes, bool forward);

#endif
