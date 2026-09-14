/***************************************************************
 * Name:      EDID_compare.cpp
 * Purpose:   field-level differences between two parsed EDIDs
 * License:   GPLv3+
 **************************************************************/

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "EDID_class.h"
#include "EDID_compare.h"

namespace {

struct compare_state {
   EDID_cl&                      left;
   EDID_cl&                      right;
   std::vector<edid_difference>& found;
};

std::string place_of(EDID_cl& EDID, edi_grp_cl* group, const std::string& block) {
   wxc_String name;
   group->getGrpName(EDID, name);
   std::string place = block + " · ";
   if (! group->CodeName.IsEmpty()) place += group->CodeName.std_str() + ": ";
   return place + name.std_str();
}

std::string value_of(EDID_cl& EDID, edi_dynfld_t* field) {
   wxc_String text;
   u32_t value = 0;
   if (! RCD_IS_OK((EDID.*field->field.handlerfn)(OP_READ, text, value, field))) {
      return "<unreadable>";
   }
   std::string shown = text.std_str();
   if (((field->field.flags & F_VS) != 0) && (field->field.vmap_idx != 0)) {
      wxc_String label;
      EDID.getValDesc(label, field, value, VD_NAME);
      if (! label.IsEmpty()) shown += " (" + label.std_str() + ")";
   }
   return shown;
}

bool is_checksum(const edi_dynfld_t* field) {
   const char* name = (field->field.name != NULL) ? field->field.name : "";
   for (const char* chr = name; *chr != 0; chr++) {
      if (0 == strncasecmp(chr, "checksum", 8)) return true;
   }
   return false;
}

void compare_groups(compare_state& state, edi_grp_cl* left, edi_grp_cl* right,
                    const std::string& block);

//pair the groups of two lists by code name, keeping their order
void compare_lists(compare_state& state, const std::vector<edi_grp_cl*>& left,
                   const std::vector<edi_grp_cl*>& right, const std::string& block) {
   size_t rows = left.size();
   size_t columns = right.size();
   std::vector<std::vector<u32_t>> common(rows + 1, std::vector<u32_t>(columns + 1, 0));
   for (size_t row=rows; row-- > 0;) {
      for (size_t column=columns; column-- > 0;) {
         common[row][column] = (left[row]->CodeName == right[column]->CodeName)
            ? common[row + 1][column + 1] + 1
            : std::max(common[row + 1][column], common[row][column + 1]);
      }
   }
   size_t row = 0;
   size_t column = 0;
   while ((row < rows) || (column < columns)) {
      if ((row < rows) && (column < columns) &&
          (left[row]->CodeName == right[column]->CodeName)) {
         compare_groups(state, left[row], right[column], block);
         row++;
         column++;
      } else if ((column >= columns) ||
                 ((row < rows) && (common[row + 1][column] >= common[row][column + 1]))) {
         state.found.push_back({place_of(state.left, left[row], block), "", "present", ""});
         row++;
      } else {
         state.found.push_back({place_of(state.right, right[column], block), "", "", "present"});
         column++;
      }
   }
}

std::vector<edi_grp_cl*> sub_groups(edi_grp_cl* group) {
   std::vector<edi_grp_cl*> list;
   for (u32_t idx=0; idx<group->getSubGrpCount(); idx++) {
      if (group->getSubGroup(idx) != NULL) list.push_back(group->getSubGroup(idx));
   }
   return list;
}

void compare_groups(compare_state& state, edi_grp_cl* left, edi_grp_cl* right,
                    const std::string& block) {
   std::string place = place_of(state.left, left, block);
   u32_t count = std::max(left->FieldsAr.GetCount(), right->FieldsAr.GetCount());
   for (u32_t idx=0; idx<count; idx++) {
      edi_dynfld_t* one = (idx < left->FieldsAr.GetCount()) ? left->FieldsAr.Item(idx) : NULL;
      edi_dynfld_t* two = (idx < right->FieldsAr.GetCount()) ? right->FieldsAr.Item(idx) : NULL;
      edi_dynfld_t* named = (one != NULL) ? one : two;
      if (is_checksum(named)) continue;
      std::string first = (one != NULL) ? value_of(state.left, one) : std::string();
      std::string second = (two != NULL) ? value_of(state.right, two) : std::string();
      bool same_field = (one != NULL) && (two != NULL) &&
                        (0 == strcmp(one->field.name, two->field.name));
      if (same_field && (first == second)) continue;
      if (! same_field && (one != NULL) && (two != NULL)) {
         //layouts differ: report both fields
         state.found.push_back({place, one->field.name, first, ""});
         state.found.push_back({place, two->field.name, "", second});
         continue;
      }
      state.found.push_back({place, named->field.name, first, second});
   }
   compare_lists(state, sub_groups(left), sub_groups(right), block);
}

std::vector<edi_grp_cl*> block_groups(EDID_cl& EDID, u32_t block) {
   std::vector<edi_grp_cl*> list;
   if (block >= EDID.getNumValidBlocks()) return list;
   GroupAr_cl* array = EDID.BlkGroupsAr[block];
   for (u32_t idx=0; idx<array->GetCount(); idx++) list.push_back(array->Item(idx));
   return list;
}

} //namespace

std::vector<edid_difference> edid_compare(EDID_cl& left, EDID_cl& right) {
   std::vector<edid_difference> found;
   compare_state state = {left, right, found};
   u32_t blocks = std::max(left.getNumValidBlocks(), right.getNumValidBlocks());
   for (u32_t block=0; block<blocks; block++) {
      std::string name = "Block " + std::to_string(block);
      bool in_left = block < left.getNumValidBlocks();
      bool in_right = block < right.getNumValidBlocks();
      u8_t left_tag = in_left ? left.getEDID()->blk[block][0] : 0;
      u8_t right_tag = in_right ? right.getEDID()->blk[block][0] : 0;
      if ((block > 0) && in_left && in_right && (left_tag != right_tag)) {
         char one[8];
         char two[8];
         snprintf(one, sizeof(one), "0x%02X", left_tag);
         snprintf(two, sizeof(two), "0x%02X", right_tag);
         found.push_back({name, "Extension tag", one, two});
         continue;
      }
      if (in_left != in_right) {
         found.push_back({name, "", in_left ? "present" : "", in_right ? "present" : ""});
         continue;
      }
      std::vector<edi_grp_cl*> one = block_groups(left, block);
      std::vector<edi_grp_cl*> two = block_groups(right, block);
      if (one.empty() && two.empty()) {
         //blocks kept as raw data
         u32_t changed = 0;
         for (u32_t idx=0; idx<sizeof(ediblk_t); idx++) {
            if (left.getEDID()->blk[block][idx] != right.getEDID()->blk[block][idx]) changed++;
         }
         if (changed > 0) {
            found.push_back({name, "Raw data", std::to_string(changed) + " bytes differ",
                             std::to_string(changed) + " bytes differ"});
         }
         continue;
      }
      compare_lists(state, one, two, name);
   }
   return found;
}

bool edid_parse_bytes(EDID_cl& EDID, const std::vector<u8_t>& bytes, std::string& problem) {
   size_t blocks = (bytes.size() + sizeof(ediblk_t) - 1) / sizeof(ediblk_t);
   if ((blocks == 0) || (blocks > (sizeof(edi_t) / sizeof(ediblk_t)))) {
      problem = "EDID data must contain 1 to 4 blocks of 128 bytes";
      return false;
   }
   EDID.Clear();
   EDID.b_ERR_Ignore = true;
   edi_buf_t* buffer = EDID.getEDID();
   std::memset(buffer, 0, sizeof(*buffer));
   std::memcpy(buffer->buff, bytes.data(), bytes.size());
   u32_t extensions = 0;
   if (! RCD_IS_OK(EDID.ParseEDID_Base(extensions))) {
      problem = "the base EDID block could not be read";
      return false;
   }
   extensions = static_cast<u32_t>(blocks - 1);
   for (u32_t block=1; block<=extensions; block++) {
      u8_t tag = buffer->blk[block][0];
      rcode result;
      if (tag == 0x02) {
         result = EDID.ParseEDID_CEA(block);
      } else if (tag == 0x70) {
         result = EDID.ParseEDID_DisplayID(block);
      } else {
         continue;
      }
      if (! RCD_IS_OK(result)) EDID.BlkGroupsAr[block]->Clear();
   }
   EDID.ForceNumValidBlocks(extensions + 1);
   return true;
}
