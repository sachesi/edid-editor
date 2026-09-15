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

namespace {

int field_index(edi_grp_cl* group, const char* name) {
   for (u32_t idx=0; idx<group->FieldsAr.GetCount(); idx++) {
      const char* field = group->FieldsAr.Item(idx)->field.name;
      if ((field != NULL) && (0 == strcmp(field, name))) return static_cast<int>(idx);
   }
   return -1;
}

} //namespace

bool edid_timing_layout_of(edi_grp_cl* group, edid_timing_layout& layout) {
   static const int dtd_fields[TIMING_FIELD_COUNT] = {
      DTD_IDX_PIXCLK, DTD_IDX_HAPIX, DTD_IDX_HBPIX, DTD_IDX_VALIN,
      DTD_IDX_VBLIN, DTD_IDX_HSOFFS, DTD_IDX_HSWIDTH, DTD_IDX_VSOFFS,
      DTD_IDX_VSWIDTH, DTD_IDX_HBORD, DTD_IDX_VBORD, DTD_IDX_HSIZE, DTD_IDX_VSIZE,
   };
   static const int displayid_type1_fields[TIMING_FIELD_COUNT] = {
      0, 5, 6, 10, 11, 7, 8, 12, 13, -1, -1, -1, -1,
   };
   static const int t7_fields[TIMING_FIELD_COUNT] = {
      T7F_IDX_PIXCLK, T7F_IDX_HAPIX, T7F_IDX_HBPIX, T7F_IDX_VALIN,
      T7F_IDX_VBLIN, T7F_IDX_HSOFFS, T7F_IDX_HSWIDTH, T7F_IDX_VSOFFS,
      T7F_IDX_VSWIDTH, -1, -1, -1, -1,
   };

   //single bits, found by name as their places differ
   static const char* const dtd_flags[TIMING_FLAG_COUNT] = {
      "interlace", "Hsync_type", "Vsync_type", NULL,
   };
   static const char* const displayid_flags[TIMING_FLAG_COUNT] = {
      "Interlaced", "Horizontal sync positive", "Vertical sync positive", "Preferred",
   };
   static const char* const t7_flags[TIMING_FLAG_COUNT] = {
      "T7_IL", "T7_HSP", "T7_VSP", NULL,
   };

   if (group == NULL) return false;
   const int* fields = NULL;
   const char* const* flags = NULL;
   const char* code = group->CodeName.c_str();
   layout.clock_step = 1.0;
   layout.clock_max = 0.0;
   if (0 == strcmp(code, "DTD")) {
      fields = dtd_fields;
      flags = dtd_flags;
      layout.pixel_hz = 10000.0;
   } else if (0 == strcmp(code, "DID-T1")) {
      //the field reads and writes kHz of a clock stored in 10 kHz units
      fields = displayid_type1_fields;
      flags = displayid_flags;
      layout.pixel_hz = 1000.0;
      layout.clock_step = 10.0;
      layout.clock_max = 167772160.0;
   } else if (0 == strcmp(code, "DID-T7")) {
      //Type VII shares the Type I layout
      fields = displayid_type1_fields;
      flags = displayid_flags;
      layout.pixel_hz = 1000.0;
      layout.clock_max = 16777216.0;
   } else if (0 == strcmp(code, "T7VTB")) {
      fields = t7_fields;
      flags = t7_flags;
      layout.pixel_hz = 1000.0;
   } else {
      return false;
   }
   std::copy(fields, fields + TIMING_FIELD_COUNT, layout.fields);
   for (int idx=0; idx<TIMING_FIELD_COUNT; idx++) {
      if (layout.fields[idx] >= static_cast<int>(group->FieldsAr.GetCount())) return false;
   }
   for (int idx=0; idx<TIMING_FLAG_COUNT; idx++) {
      layout.flags[idx] = (flags[idx] != NULL) ? field_index(group, flags[idx]) : -1;
   }
   return true;
}

double edid_timing_clock_for(const edid_timing_layout& layout, double refresh,
                             double htotal, double vtotal) {
   const double units = refresh * htotal * vtotal / layout.pixel_hz;
   return std::max(layout.clock_step,
                   std::round(units / layout.clock_step) * layout.clock_step);
}

//-------- timings in any format

namespace {

rcode read_field(EDID_cl& EDID, edi_dynfld_t* field, u32_t& value) {
   wxc_String text;
   return (EDID.*field->field.handlerfn)(OP_READ, text, value, field);
}

bool write_field(EDID_cl& EDID, edi_dynfld_t* field, u32_t value) {
   wxc_String text;
   return RCD_IS_OK((EDID.*field->field.handlerfn)(OP_WRINT, text, value, field));
}

bool is_dtd(edi_grp_cl* group) {
   return 0 == strcmp(group->CodeName.c_str(), "DTD");
}

std::vector<u8_t> data_of(edi_grp_cl* group) {
   const u8_t* data = group->getInstPtr();
   return std::vector<u8_t>(data, data + group->getTotalSize());
}

void set_data(edi_grp_cl* group, const std::vector<u8_t>& data) {
   memcpy(group->getInstPtr(), data.data(), data.size());
}

std::string mode_name(EDID_cl& EDID, edi_grp_cl* group) {
   edid_timing_values values;
   if (! edid_timing_read(EDID, group, values)) return group->CodeName.c_str();
   double total = static_cast<double>(values.value[TIMING_HACTIVE] + values.value[TIMING_HBLANK]) *
                  (values.value[TIMING_VACTIVE] + values.value[TIMING_VBLANK]);
   char text[64];
   snprintf(text, sizeof(text), "%ux%u @ %.2f Hz", values.value[TIMING_HACTIVE],
            values.value[TIMING_VACTIVE], (total > 0.0) ? values.pixel_hz / total : 0.0);
   return text;
}

//aspect ratio codes of DisplayID and CTA-861 Type VII timings; 8 means the
//ratio of the active sizes
u32_t aspect_code(u32_t width, u32_t height) {
   static const u32_t ratios[][2] = {
      {1, 1}, {5, 4}, {4, 3}, {15, 9}, {16, 9}, {16, 10}, {64, 27}, {256, 135},
   };
   if ((width == 0) || (height == 0)) return 8;
   for (u32_t code=0; code<8; code++) {
      double wanted = static_cast<double>(ratios[code][0]) / ratios[code][1];
      if (std::fabs((static_cast<double>(width) / height) - wanted) < wanted * 0.01) return code;
   }
   return 8;
}

//values from a timing of another format: the DTD's sync type becomes digital
//separate, where the polarity bits apply, and the aspect ratio follows the size
bool write_converted(EDID_cl& EDID, edi_grp_cl* to, edid_timing_values values,
                     bool from_dtd) {
   values.flag[TIMING_PREFERRED] = false;
   std::vector<u8_t> saved = data_of(to);
   if (from_dtd != is_dtd(to)) {
      if (values.flag[TIMING_INTERLACED]) return false; //field and frame sizes differ
      int index = is_dtd(to) ? field_index(to, "sync_type") :
                  (field_index(to, "Aspect ratio") >= 0) ? field_index(to, "Aspect ratio")
                                                         : field_index(to, "asp_ratio");
      u32_t value = is_dtd(to) ? 3 : aspect_code(values.value[TIMING_HACTIVE],
                                                 values.value[TIMING_VACTIVE]);
      if ((index >= 0) && ! write_field(EDID, to->FieldsAr.Item(index), value)) return false;
   }
   if (! edid_timing_write(EDID, to, values)) {
      set_data(to, saved);
      return false;
   }
   return true;
}

void collect_timings(std::vector<edi_grp_cl*>& list, edi_grp_cl* group) {
   edid_timing_layout layout;
   if (edid_timing_layout_of(group, layout)) list.push_back(group);
   for (u32_t idx=0; idx<group->getSubGrpCount(); idx++) {
      edi_grp_cl* sub = group->getSubGroup(idx);
      if (sub != NULL) collect_timings(list, sub);
   }
}

std::vector<edi_grp_cl*> all_timings(EDID_cl& EDID, std::vector<u32_t>* blocks = NULL) {
   std::vector<edi_grp_cl*> list;
   for (u32_t block=0; block<EDID.getNumValidBlocks(); block++) {
      GroupAr_cl* array = EDID.BlkGroupsAr[block];
      for (u32_t idx=0; idx<array->GetCount(); idx++) {
         size_t before = list.size();
         collect_timings(list, array->Item(idx));
         if (blocks != NULL) blocks->insert(blocks->end(), list.size() - before, block);
      }
   }
   return list;
}

} //namespace

bool edid_timing_read(EDID_cl& EDID, edi_grp_cl* group, edid_timing_values& values) {
   edid_timing_layout layout;
   if (! edid_timing_layout_of(group, layout)) return false;
   for (int idx=0; idx<TIMING_FIELD_COUNT; idx++) {
      values.value[idx] = 0;
      values.has[idx] = layout.fields[idx] >= 0;
      if (! values.has[idx]) continue;
      if (! RCD_IS_OK(read_field(EDID, group->FieldsAr.Item(layout.fields[idx]),
                                 values.value[idx]))) return false;
   }
   values.pixel_hz = values.value[TIMING_PIXCLK] * layout.pixel_hz;
   for (int idx=0; idx<TIMING_FLAG_COUNT; idx++) {
      u32_t bit = 0;
      if ((layout.flags[idx] >= 0) &&
          ! RCD_IS_OK(read_field(EDID, group->FieldsAr.Item(layout.flags[idx]), bit))) {
         return false;
      }
      values.flag[idx] = bit != 0;
   }
   return true;
}

bool edid_timing_write(EDID_cl& EDID, edi_grp_cl* group, const edid_timing_values& values) {
   edid_timing_layout layout;
   if (! edid_timing_layout_of(group, layout)) return false;
   std::vector<u8_t> saved = data_of(group);
   bool fits = true;
   for (int idx=0; fits && (idx<TIMING_FIELD_COUNT); idx++) {
      bool sizes = (idx == TIMING_HSIZE) || (idx == TIMING_VSIZE);
      if (layout.fields[idx] < 0) {
         //a border this format has no place for would change the timing
         fits = sizes || (values.value[idx] == 0);
         continue;
      }
      if (sizes && ! values.has[idx]) continue;
      u32_t value = values.value[idx];
      if (idx == TIMING_PIXCLK) {
         double units = values.pixel_hz / layout.pixel_hz;
         value = static_cast<u32_t>(std::lround(units / layout.clock_step) * layout.clock_step);
         double maximum = (layout.clock_max > 0.0) ? layout.clock_max
            : group->FieldsAr.Item(layout.fields[idx])->field.maxv;
         fits = (value > 0) && (value <= maximum);
         if (! fits) break;
      }
      fits = write_field(EDID, group->FieldsAr.Item(layout.fields[idx]), value);
   }
   for (int idx=0; fits && (idx<TIMING_FLAG_COUNT); idx++) {
      if (layout.flags[idx] < 0) continue;
      fits = write_field(EDID, group->FieldsAr.Item(layout.flags[idx]), values.flag[idx] ? 1 : 0);
   }
   if (! fits) set_data(group, saved);
   return fits;
}

bool edid_timing_copy(EDID_cl& EDID, edi_grp_cl* from, edi_grp_cl* to) {
   edid_timing_layout layout;
   if (! edid_timing_layout_of(to, layout)) return false;
   if (is_dtd(from) && is_dtd(to)) {
      set_data(to, data_of(from));
      return true;
   }
   edid_timing_values values;
   if (! edid_timing_read(EDID, from, values)) return false;
   return write_converted(EDID, to, values, is_dtd(from));
}

edi_grp_cl* edid_first_timing(EDID_cl& EDID) {
   GroupAr_cl* base = EDID.BlkGroupsAr[0];
   for (u32_t idx=0; idx<base->GetCount(); idx++) {
      edi_grp_cl* group = base->Item(idx);
      if (is_dtd(group) && (group->getAbsOffs() == 0x36)) return group;
   }
   return NULL;
}

std::vector<edid_mode> edid_modes(EDID_cl& EDID) {
   std::vector<edid_mode> modes;
   std::vector<u32_t> blocks;
   std::vector<edi_grp_cl*> timings = all_timings(EDID, &blocks);
   edi_grp_cl* first = edid_first_timing(EDID);
   for (size_t idx=0; idx<timings.size(); idx++) {
      edid_timing_values values;
      if (! edid_timing_read(EDID, timings[idx], values)) continue;
      edid_mode mode;
      mode.group = timings[idx];
      mode.block = blocks[idx];
      mode.width = values.value[TIMING_HACTIVE];
      mode.height = values.value[TIMING_VACTIVE];
      double total = static_cast<double>(mode.width + values.value[TIMING_HBLANK]) *
                     (mode.height + values.value[TIMING_VBLANK]);
      mode.refresh = (total > 0.0) ? values.pixel_hz / total : 0.0;
      mode.interlaced = values.flag[TIMING_INTERLACED];
      mode.preferred = (timings[idx] == first) || values.flag[TIMING_PREFERRED];
      modes.push_back(mode);
   }
   return modes;
}

size_t edid_default_mode(const std::vector<edid_mode>& modes) {
   size_t best = modes.size();
   for (size_t idx=0; idx<modes.size(); idx++) {
      if (best == modes.size()) {
         best = idx;
         continue;
      }
      const edid_mode& mode = modes[idx];
      const edid_mode& other = modes[best];
      if (mode.preferred != other.preferred) {
         if (mode.preferred) best = idx;
         continue;
      }
      u64_t area = static_cast<u64_t>(mode.width) * mode.height;
      u64_t other_area = static_cast<u64_t>(other.width) * other.height;
      if (area != other_area) {
         if (area > other_area) best = idx;
         continue;
      }
      if (mode.refresh > other.refresh + 0.0005) best = idx;
   }
   return best;
}

bool edid_plan_preferred(EDID_cl& EDID, edi_grp_cl* timing,
                         std::vector<edid_data_change>& changes, std::string& message,
                         edid_prefer_way* way) {
   changes.clear();
   edid_prefer_way unused;
   if (way == NULL) way = &unused;
   *way = PREFER_ALREADY_FIRST;
   edid_timing_layout layout;
   if ((timing == NULL) || ! edid_timing_layout_of(timing, layout)) {
      message = "only a detailed timing can be preferred";
      return false;
   }
   std::vector<edi_grp_cl*> timings = all_timings(EDID);
   std::vector<std::vector<u8_t>> saved;
   for (edi_grp_cl* group : timings) saved.push_back(data_of(group));
   auto restore = [&]() {
      for (size_t idx=0; idx<timings.size(); idx++) set_data(timings[idx], saved[idx]);
   };

   edi_grp_cl* first = edid_first_timing(EDID);
   std::string name = mode_name(EDID, timing);
   bool flagged = false;
   if (timing == first) {
      message = name + " is the first detailed timing, the one every system prefers.";
   } else {
      bool swapped = false;
      std::string first_name;
      edid_timing_values mine;
      edid_timing_values theirs;
      if (! edid_timing_read(EDID, timing, mine)) {
         message = name + " can't be read";
         return false;
      }
      if ((first != NULL) && edid_timing_read(EDID, first, theirs)) {
         first_name = mode_name(EDID, first);
         if (is_dtd(timing)) {
            std::vector<u8_t> data = data_of(timing);
            set_data(timing, data_of(first));
            set_data(first, data);
            swapped = true;
         } else {
            swapped = write_converted(EDID, first, mine, false) &&
                      write_converted(EDID, timing, theirs, true);
            if (! swapped) restore();
         }
      }
      if (swapped) {
         *way = PREFER_SWAPPED;
         message = name + " is now the first detailed timing, the one every system prefers; " +
                   first_name + " took its place.";
      } else if (layout.flags[TIMING_PREFERRED] >= 0) {
         char reason[128];
         if (mine.pixel_hz > 655350000.0) {
            snprintf(reason, sizeof(reason), "its %.2f MHz pixel clock is above the 655.35 MHz "
                     "the first detailed timing holds", mine.pixel_hz / 1000000.0);
         } else {
            snprintf(reason, sizeof(reason), "it doesn't fit the first detailed timing");
         }
         write_field(EDID, timing->FieldsAr.Item(layout.flags[TIMING_PREFERRED]), 1);
         flagged = true;
         *way = PREFER_FLAGGED;
         message = name + ": " + reason + ", so it is flagged preferred in DisplayID. " +
                   ((first != NULL) ? first_name + " stays the first detailed timing, which "
                                      "systems that read only the base block prefer."
                                    : std::string());
      } else {
         restore();
         message = name + " can't take the place of the first detailed timing, and has no "
                   "preferred flag of its own.";
         return false;
      }
   }

   //a DisplayID flag elsewhere would compete with the choice
   for (edi_grp_cl* group : timings) {
      edid_timing_layout other;
      if ((flagged && (group == timing)) || ! edid_timing_layout_of(group, other) ||
          (other.flags[TIMING_PREFERRED] < 0)) continue;
      write_field(EDID, group->FieldsAr.Item(other.flags[TIMING_PREFERRED]), 0);
   }

   //DisplayID expects one of its timings to be preferred: flag one that leaves
   //the choice as it is, when there is such a timing
   if (! flagged && (first != NULL)) {
      for (edi_grp_cl* group : timings) {
         edid_timing_layout other;
         if (! edid_timing_layout_of(group, other) || (other.flags[TIMING_PREFERRED] < 0)) continue;
         edi_dynfld_t* flag = group->FieldsAr.Item(other.flags[TIMING_PREFERRED]);
         write_field(EDID, flag, 1);
         std::vector<edid_mode> modes = edid_modes(EDID);
         size_t best = edid_default_mode(modes);
         if ((best < modes.size()) && (modes[best].group == first)) break;
         write_field(EDID, flag, 0);
      }
   }

   if (flagged) {
      std::vector<edid_mode> modes = edid_modes(EDID);
      size_t best = edid_default_mode(modes);
      if ((best < modes.size()) && (modes[best].group != timing)) {
         message += " Linux still lists " + mode_name(EDID, modes[best].group) +
                    " first, as it is larger or faster.";
      }
   }

   for (size_t idx=0; idx<timings.size(); idx++) {
      std::vector<u8_t> now = data_of(timings[idx]);
      if (now != saved[idx]) changes.push_back({timings[idx], saved[idx], now});
   }
   restore();
   return true;
}

void edid_apply_changes(const std::vector<edid_data_change>& changes, bool forward) {
   for (const edid_data_change& change : changes) {
      set_data(change.group, forward ? change.after : change.before);
   }
}
