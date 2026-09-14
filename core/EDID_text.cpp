/***************************************************************
 * Name:      EDID_text.cpp
 * Purpose:   EDID hexadecimal text import/export and text reports
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include <cstdio>
#include <cstring>

#include "EDID_text.h"

static bool hex_separator(char chr) {
   return (chr == ' ') || (chr == '\t') || (chr == '\r') || (chr == '\n') ||
          (chr == ',') || (chr == ';') || (chr == '{') || (chr == '}') ||
          (chr == '[') || (chr == ']') || (chr == '(') || (chr == ')');
}

static int hex_digit(char chr) {
   if ((chr >= '0') && (chr <= '9')) return chr - '0';
   if ((chr >= 'a') && (chr <= 'f')) return chr - 'a' + 10;
   if ((chr >= 'A') && (chr <= 'F')) return chr - 'A' + 10;
   return -1;
}

bool edid_hex_decode(const char* text, size_t length,
                     std::vector<u8_t>& bytes, std::string& error) {
   bytes.clear();
   error.clear();
   size_t line = 1;
   size_t pos = 0;

   while (pos < length) {
      if (text[pos] == '\n') line++;
      if (hex_separator(text[pos])) {
         pos++;
         continue;
      }

      size_t start = pos;
      while ((pos < length) && ! hex_separator(text[pos])) pos++;
      std::string token(text + start, pos - start);

      if (token.back() == ':') continue; //offset label
      std::string digits = token;
      if ((digits.size() > 2) && (digits[0] == '0') &&
          ((digits[1] == 'x') || (digits[1] == 'X'))) {
         digits.erase(0, 2);
      }

      bool valid = (digits.size() % 2) == 0;
      for (size_t idx=0; valid && (idx<digits.size()); idx++) {
         valid = hex_digit(digits[idx]) >= 0;
      }
      if (! valid) {
         if (token.size() > 24) token = token.substr(0, 24) + "…";
         char message[128];
         snprintf(message, sizeof(message),
                  "line %zu contains “%s”, which is not hexadecimal byte data",
                  line, token.c_str());
         error = message;
         bytes.clear();
         return false;
      }

      for (size_t idx=0; idx<digits.size(); idx += 2) {
         bytes.push_back(static_cast<u8_t>(
            (hex_digit(digits[idx]) << 4) | hex_digit(digits[idx + 1])));
      }
      if (bytes.size() > sizeof(edi_buf_t)) {
         char message[96];
         snprintf(message, sizeof(message),
                  "the text contains more than %zu bytes of EDID data",
                  sizeof(edi_buf_t));
         error = message;
         bytes.clear();
         return false;
      }
   }

   if (bytes.empty()) {
      error = "the text contains no hexadecimal byte data";
      return false;
   }
   return true;
}

std::string edid_hex_encode(const u8_t* data, size_t size) {
   static const char digits[] = "0123456789ABCDEF";
   std::string text;
   text.reserve(size * 2 + size / 16 + size / 128 + 1);
   for (size_t idx=0; idx<size; idx++) {
      text.push_back(digits[data[idx] >> 4]);
      text.push_back(digits[data[idx] & 0x0f]);
      if ((idx & 0x0f) == 0x0f) text.push_back('\n');
      if (((idx & 0x7f) == 0x7f) && (idx + 1 < size)) text.push_back('\n');
   }
   if ((size & 0x0f) != 0) text.push_back('\n');
   return text;
}

static void report_pad(std::string& report, size_t used, size_t width) {
   report.append((used < width) ? (width - used) : 1, ' ');
}

static void report_group(EDID_cl& EDID, edi_grp_cl* group, std::string& report,
                         const std::string& indent) {
   wxc_String name;
   group->getGrpName(EDID, name);
   char heading[64];
   snprintf(heading, sizeof(heading), "offs=%u (0x%04X): ",
            group->getAbsOffs(), group->getAbsOffs());
   report += "\n" + indent + heading;
   if (! group->CodeName.IsEmpty()) report += group->CodeName.std_str() + ": ";
   report += name.std_str() + "\n";

   for (u32_t idx=0; idx<group->FieldsAr.GetCount(); idx++) {
      edi_dynfld_t* field = group->FieldsAr.Item(idx);
      const char* field_name = (field->field.name != NULL) ? field->field.name : "";
      report += indent + "  " + field_name;
      report_pad(report, strlen(field_name), 16);

      wxc_String value;
      u32_t integer = 0;
      rcode result = (EDID.*field->field.handlerfn)(OP_READ, value, integer, field);
      if (! RCD_IS_OK(result)) {
         report += "<unreadable>\n";
         continue;
      }
      report += value.std_str();

      wxc_String detail;
      if ((field->field.flags & F_VS) != 0) {
         EDID.getValDesc(detail, field, integer, VD_NAME);
      }
      wxc_String unit;
      EDID.getValUnitName(unit, field->field.flags);
      if (! detail.IsEmpty() || ! unit.IsEmpty() ||
          ((field->field.flags & F_NU) != 0)) {
         report_pad(report, value.Len(), 9);
      }
      report += detail.std_str();
      if (! detail.IsEmpty() && ! unit.IsEmpty()) report += " ";
      report += unit.std_str();
      if ((field->field.flags & F_NU) != 0) report += "\t<not used>";
      report += "\n";
   }

   for (u32_t idx=0; idx<group->getSubGrpCount(); idx++) {
      edi_grp_cl* subgroup = group->getSubGroup(idx);
      if (subgroup != NULL) report_group(EDID, subgroup, report, indent + "  ");
   }
}

std::string edid_text_report(EDID_cl& EDID, const char* source,
                             const char* version) {
   std::string report = "EDID Editor ";
   report += version;
   report += "\nEDID structure and data\nSource: ";
   report += source;
   report += "\n";

   edi_buf_t* buffer = EDID.getEDID();
   u32_t blocks = EDID.getNumValidBlocks();
   for (u32_t block=0; block<blocks; block++) {
      GroupAr_cl* groups = EDID.BlkGroupsAr[block];
      u8_t tag = buffer->blk[block][0];
      char heading[96];
      if (block == 0) {
         snprintf(heading, sizeof(heading), "Base EDID");
      } else if (groups->GetCount() == 0) {
         snprintf(heading, sizeof(heading), "unparsed extension, tag 0x%02X", tag);
      } else {
         snprintf(heading, sizeof(heading), "%s extension",
                  (tag == 0x02) ? "CTA-861" : (tag == 0x70) ? "DisplayID" :
                                                              "unknown");
      }
      char title[128];
      snprintf(title, sizeof(title), "\n\n----| EDID block [%u]: %s |----\n",
               block, heading);
      report += title;

      if (groups->GetCount() == 0) {
         report += "\n" + edid_hex_encode(buffer->blk[block], sizeof(ediblk_t));
         continue;
      }
      for (u32_t idx=0; idx<groups->GetCount(); idx++) {
         edi_grp_cl* group = groups->Item(idx);
         if (group != NULL) report_group(EDID, group, report, "");
      }
   }

   report += "\n\n----| Raw data |----\n\n";
   report += edid_hex_encode(buffer->buff, blocks * sizeof(ediblk_t));
   report += "\n----| END |----\n";
   return report;
}
