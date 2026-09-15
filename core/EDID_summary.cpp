/***************************************************************
 * Name:      EDID_summary.cpp
 * Purpose:   short overview of a parsed EDID
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include <cmath>
#include <cstdio>
#include <cstring>

#include "EDID_class.h"
#include "EDID_summary.h"
#include "EDID_timing.h"

namespace {

struct summary_state {
   EDID_cl&                        EDID;
   std::vector<edid_summary_item>& items;
};

void add(summary_state& state, const char* section, const char* label,
         const std::string& value) {
   if (! value.empty()) state.items.push_back({section, label, value});
}

//groups with a code name, in block order, sub-groups included
void collect(edi_grp_cl* group, const char* code, std::vector<edi_grp_cl*>& found) {
   if (group == NULL) return;
   if (group->CodeName == wxc_String(code)) found.push_back(group);
   for (u32_t idx=0; idx<group->getSubGrpCount(); idx++) {
      collect(group->getSubGroup(idx), code, found);
   }
}

std::vector<edi_grp_cl*> groups(summary_state& state, const char* code) {
   std::vector<edi_grp_cl*> found;
   for (u32_t block=0; block<state.EDID.getNumValidBlocks(); block++) {
      GroupAr_cl* array = state.EDID.BlkGroupsAr[block];
      for (u32_t idx=0; idx<array->GetCount(); idx++) collect(array->Item(idx), code, found);
   }
   return found;
}

edi_grp_cl* first(summary_state& state, const char* code) {
   std::vector<edi_grp_cl*> found = groups(state, code);
   return found.empty() ? NULL : found[0];
}

edi_dynfld_t* field(edi_grp_cl* group, const char* name) {
   if (group == NULL) return NULL;
   for (u32_t idx=0; idx<group->FieldsAr.GetCount(); idx++) {
      edi_dynfld_t* item = group->FieldsAr.Item(idx);
      if ((item->field.name != NULL) && (0 == strcmp(item->field.name, name))) return item;
   }
   return NULL;
}

bool read(summary_state& state, edi_grp_cl* group, const char* name,
          std::string* text, u32_t* value) {
   edi_dynfld_t* item = field(group, name);
   if (item == NULL) return false;
   wxc_String sval;
   u32_t ival = 0;
   if (! RCD_IS_OK((state.EDID.*item->field.handlerfn)(OP_READ, sval, ival, item))) return false;
   if (text != NULL) *text = sval.std_str();
   if (value != NULL) *value = ival;
   return true;
}

std::string text_of(summary_state& state, edi_grp_cl* group, const char* name) {
   std::string text;
   read(state, group, name, &text, NULL);
   while (! text.empty() && (text.back() == ' ')) text.pop_back();
   return text;
}

u32_t value_of(summary_state& state, edi_grp_cl* group, const char* name) {
   u32_t value = 0;
   read(state, group, name, NULL, &value);
   return value;
}

//value-list name of a field, e.g. "DisplayPort"
std::string label_of(summary_state& state, edi_grp_cl* group, const char* name) {
   edi_dynfld_t* item = field(group, name);
   if (item == NULL) return std::string();
   wxc_String label;
   state.EDID.getValDesc(label, item, value_of(state, group, name), VD_NAME);
   return label.std_str();
}

std::string group_name(summary_state& state, edi_grp_cl* group) {
   wxc_String name;
   group->getGrpName(state.EDID, name);
   return name.std_str();
}

std::string range(const std::string& low, const std::string& high, const char* unit) {
   if (low.empty() || high.empty()) return std::string();
   return ((low == high) ? low : low + "–" + high) + " " + unit;
}

void join(std::string& list, const std::string& item) {
   if (item.empty()) return;
   if (! list.empty()) list += ", ";
   list += item;
}

void display_section(summary_state& state) {
   const char* section = "Display";
   edi_grp_cl* name = first(state, "MND");
   add(state, section, "Name", text_of(state, name, "Monitor name"));
   edi_grp_cl* base = first(state, "BED");
   add(state, section, "Manufacturer", text_of(state, base, "mfc_id"));
   add(state, section, "Product code", text_of(state, base, "prod_id"));
   edi_grp_cl* serial = first(state, "MSN");
   std::string serial_text = text_of(state, serial, "Monitor SN");
   if (serial_text.empty() && (value_of(state, base, "serial") != 0)) {
      serial_text = text_of(state, base, "serial");
   }
   add(state, section, "Serial number", serial_text);

   u32_t week = value_of(state, base, "prod_week");
   std::string year = text_of(state, base, "prod_year");
   if (! year.empty()) {
      std::string made = (week == 0xFF) ? "Model year " + year :
                         ((week >= 1) && (week <= 54)) ? "Week " + std::to_string(week) + ", " + year :
                         year;
      add(state, section, "Manufactured", made);
   }
   std::string version = text_of(state, base, "edid_ver");
   std::string revision = text_of(state, base, "edid_rev");
   if (! version.empty() && ! revision.empty()) add(state, section, "EDID", version + "." + revision);
}

void video_section(summary_state& state) {
   const char* section = "Video";
   edi_grp_cl* input = first(state, "VID");
   std::string kind = label_of(state, input, "Input Type");
   if (kind == "Digital") {
      std::string interface_type = label_of(state, input, "IF Type");
      if (! interface_type.empty() && (interface_type != "undefined")) {
         kind += ", " + interface_type;
      }
      std::string depth = label_of(state, input, "Color Depth");
      if (! depth.empty() && (depth != "undefined")) kind += ", " + depth + " per color";
   }
   add(state, section, "Input", kind);

   edi_grp_cl* size = first(state, "BDD");
   u32_t width = value_of(state, size, "max_hsize");
   u32_t height = value_of(state, size, "max_vsize");
   if ((width > 0) && (height > 0)) {
      char text[64];
      double inches = std::sqrt(static_cast<double>(width * width + height * height)) / 2.54;
      snprintf(text, sizeof(text), "%u × %u cm (%.1f″)", width, height, inches);
      add(state, section, "Screen size", text);
   }

   //first detailed timing of the base block
   std::vector<edi_grp_cl*> base_timings;
   GroupAr_cl* base = state.EDID.BlkGroupsAr[EDI_BASE_IDX];
   for (u32_t idx=0; idx<base->GetCount(); idx++) collect(base->Item(idx), "DTD", base_timings);
   //the mode Linux uses, and the first timing when systems reading only the base
   //block use another
   std::vector<edid_mode> all_modes = edid_modes(state.EDID);
   size_t chosen = edid_default_mode(all_modes);
   edi_grp_cl* first = edid_first_timing(state.EDID);
   if (chosen < all_modes.size()) {
      edi_grp_cl* preferred = all_modes[chosen].group;
      add(state, section, "Preferred timing", group_name(state, preferred));
      if ((first != NULL) && (preferred != first)) {
         add(state, section, "First detailed timing", group_name(state, first));
      }
   } else if (! base_timings.empty()) {
      add(state, section, "Preferred timing", group_name(state, base_timings[0]));
   }

   //"2560x1440 @ 59.95Hz" names, rates grouped by resolution
   std::vector<std::pair<std::string, std::vector<std::string>>> modes;
   for (const char* code : {"DTD", "T7VTB", "DID-T1", "DID-T7"}) {
      for (edi_grp_cl* timing : groups(state, code)) {
         std::string name = group_name(state, timing);
         size_t at = name.find(" @ ");
         std::string resolution = (at == std::string::npos) ? name : name.substr(0, at);
         std::string rate = (at == std::string::npos) ? std::string() : name.substr(at + 3);
         if ((rate.size() > 2) && (0 == rate.compare(rate.size() - 2, 2, "Hz"))) {
            rate.resize(rate.size() - 2);
            while (! rate.empty() && (rate.back() == ' ')) rate.pop_back();
         }
         auto mode = modes.begin();
         while ((mode != modes.end()) && (mode->first != resolution)) mode++;
         if (mode == modes.end()) {
            modes.push_back({resolution, {}});
            mode = modes.end() - 1;
         }
         bool known = rate.empty();
         for (const std::string& other : mode->second) known = known || (other == rate);
         if (! known) mode->second.push_back(rate);
      }
   }
   std::string timings;
   for (const auto& mode : modes) {
      std::string text = mode.first;
      for (size_t idx=0; idx<mode.second.size(); idx++) {
         text += (idx == 0) ? " at " : ", ";
         text += mode.second[idx];
      }
      if (! mode.second.empty()) text += " Hz";
      if (! timings.empty()) timings += "; ";
      timings += text;
   }
   add(state, section, "Detailed timings", timings);

   std::vector<edi_grp_cl*> formats;
   for (edi_grp_cl* format : groups(state, "SVD")) {
      edi_grp_cl* parent = format->getParentGrp();
      if ((parent != NULL) && (parent->CodeName == wxc_String("VDB"))) formats.push_back(format);
   }
   if (! formats.empty()) {
      std::string text = std::to_string(formats.size()) +
                         ((formats.size() == 1) ? " format" : " formats");
      for (edi_grp_cl* format : formats) {
         if (value_of(state, format, "Native") != 0) {
            text += ", native " + group_name(state, format);
            std::string suffix = " [Native]";
            if ((text.size() > suffix.size()) &&
                (0 == text.compare(text.size() - suffix.size(), suffix.size(), suffix))) {
               text.resize(text.size() - suffix.size());
            }
            break;
         }
      }
      add(state, section, "CTA-861 video formats", text);
   }
}

void range_section(summary_state& state) {
   const char* section = "Refresh";
   edi_grp_cl* limits = first(state, "MRL");
   add(state, section, "Vertical rate",
       range(text_of(state, limits, "min_Vfreq"), text_of(state, limits, "max_Vfreq"), "Hz"));
   add(state, section, "Horizontal rate",
       range(text_of(state, limits, "min_Hfreq"), text_of(state, limits, "max_Hfreq"), "kHz"));
   std::string clock = text_of(state, limits, "max_PixClk");
   if (! clock.empty()) add(state, section, "Maximum pixel clock", clock + " MHz");

   for (edi_grp_cl* vendor : groups(state, "VSD")) {
      if (field(vendor, "Min_Refresh") == NULL) continue; //AMD FreeSync
      add(state, section, "FreeSync range",
          range(text_of(state, vendor, "Min_Refresh"), text_of(state, vendor, "Max_Refresh"), "Hz"));
      break;
   }
   edi_grp_cl* dynamic = first(state, "DID-RANGE");
   add(state, section, "DisplayID refresh range",
       range(text_of(state, dynamic, "Minimum refresh"),
             text_of(state, dynamic, "Maximum refresh"), "Hz"));
}

void hdr_section(summary_state& state) {
   const char* section = "HDR and color";
   edi_grp_cl* hdr = first(state, "HDRS");
   if (hdr != NULL) {
      std::string curves;
      static const char* const names[][2] = {
         {"SDR", "SDR"}, {"HDR", "HDR gamma"}, {"SMPTE", "PQ"}, {"HLG", "HLG"},
      };
      for (const auto& name : names) {
         if (value_of(state, hdr, name[0]) != 0) join(curves, name[1]);
      }
      add(state, section, "Transfer functions", curves);
      std::string luminance;
      std::string maximum = text_of(state, hdr, "max_lum");
      std::string average = text_of(state, hdr, "avg_lum");
      std::string minimum = text_of(state, hdr, "min_lum");
      if (! maximum.empty()) join(luminance, "max " + maximum);
      if (! average.empty()) join(luminance, "average " + average);
      if (! minimum.empty()) join(luminance, "min " + minimum);
      if (! luminance.empty()) add(state, section, "Luminance", luminance + " cd/m²");
   }
   edi_grp_cl* colors = first(state, "CLDB");
   if (colors != NULL) {
      std::string spaces;
      static const char* const names[][2] = {
         {"xvYCC601", "xvYCC601"}, {"xvYCC709", "xvYCC709"}, {"sYCC601", "sYCC601"},
         {"opYCC601", "opYCC601"}, {"opRGB", "opRGB"}, {"BT2020cYCC", "BT.2020 cYCC"},
         {"BT2020YCC", "BT.2020 YCC"}, {"BT2020RGB", "BT.2020 RGB"}, {"DCI-P3", "DCI-P3"},
      };
      for (const auto& name : names) {
         if (value_of(state, colors, name[0]) != 0) join(spaces, name[1]);
      }
      add(state, section, "Colorimetry", spaces);
   }
}

void audio_section(summary_state& state) {
   std::string formats;
   static const char* const rates[][2] = {
      {"sf_192kHz", "192"}, {"sf_176.4kHz", "176.4"}, {"sf_96kHz", "96"},
      {"sf_88.2kHz", "88.2"}, {"sf_48kHz", "48"}, {"sf_44.1kHz", "44.1"}, {"sf_32kHz", "32"},
   };
   for (edi_grp_cl* sad : groups(state, "SAD")) {
      std::string text = group_name(state, sad);
      if (field(sad, "num_chn") != NULL) {
         u32_t channels = value_of(state, sad, "num_chn") + 1;
         text += ", " + std::to_string(channels) + ((channels == 1) ? " channel" : " channels");
      }
      for (const auto& rate : rates) {
         if (value_of(state, sad, rate[0]) != 0) {
            text += std::string(", up to ") + rate[1] + " kHz";
            break;
         }
      }
      if (! formats.empty()) formats += "; ";
      formats += text;
   }
   add(state, "Audio", "Formats", formats);
}

void extension_section(summary_state& state) {
   edi_buf_t* buffer = state.EDID.getEDID();
   for (u32_t block=1; block<state.EDID.getNumValidBlocks(); block++) {
      GroupAr_cl* array = state.EDID.BlkGroupsAr[block];
      u8_t tag = buffer->blk[block][0];
      std::string text;
      if ((tag == 0x02) && (array->GetCount() > 0)) {
         text = "CTA-861, revision " + text_of(state, array->Item(0), "Revision");
      } else if ((tag == 0x70) && (array->GetCount() > 0)) {
         text = "DisplayID " + text_of(state, array->Item(0), "Version") + "." +
                text_of(state, array->Item(0), "Revision");
      } else {
         char unknown[48];
         snprintf(unknown, sizeof(unknown), "Tag 0x%02X, kept unchanged", tag);
         text = unknown;
      }
      std::string label = "Block " + std::to_string(block);
      state.items.push_back({"Extensions", label, text});
   }
}

} //namespace

std::vector<edid_summary_item> edid_summary(EDID_cl& EDID) {
   std::vector<edid_summary_item> items;
   summary_state state = {EDID, items};
   display_section(state);
   video_section(state);
   range_section(state);
   hdr_section(state);
   audio_section(state);
   extension_section(state);
   return items;
}
