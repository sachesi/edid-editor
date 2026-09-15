/***************************************************************
 * Name:      main.cpp
 * Purpose:   command line interface to the EDID core
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <strings.h>
#include <vector>

#include "wxcompat.h"
#include "wxedid_rcd_scope.h"
#include "guilog.h"
#include "vmap.h"
#include "EDID_class.h"
#include "EDID_compare.h"
#include "EDID_display.h"
#include "EDID_document.h"
#include "EDID_names.h"
#include "EDID_summary.h"
#include "EDID_text.h"
#include "EDID_timing.h"

namespace {

const char* const usage_text =
"Usage: edid-editor-cli COMMAND [OPTIONS] ...\n"
"\n"
"Reading:\n"
"  info FILE                     overview of the display\n"
"  report FILE                   every group, field, value and unit, and the raw data\n"
"  groups FILE                   groups with the addresses the other commands take\n"
"  fields FILE GROUP             fields of a group: value, unit and notes\n"
"  describe FILE GROUP FIELD     a field's description, range and named values\n"
"  get FILE GROUP FIELD          the value of a field\n"
"  diff FILE1 FILE2              fields that differ; exit status 1 when any do\n"
"  displays                      connected displays and the files their EDID is read from\n"
"\n"
"Changing (the result goes to -o OUTPUT, or back to FILE with --in-place):\n"
"  set FILE GROUP FIELD=VALUE... write fields, rebuilding groups whose layout changes\n"
"  add FILE BLOCK KIND           add audio-lpcm, audio-extended, video or timing to a\n"
"                                CTA-861 block, or displayid to a DisplayID block; a\n"
"                                timing copies the first one, or GROUP after timing\n"
"  duplicate FILE GROUP          copy a group after itself\n"
"  delete FILE GROUP             remove a group\n"
"  move FILE GROUP up|down       move a group within its block\n"
"  prefer FILE GROUP             make a detailed timing the preferred one, keeping\n"
"                                every other timing\n"
"  fix-checksums FILE            recompute the checksum of every block\n"
"  convert FILE                  the same bytes as binary or hexadecimal text\n"
"\n"
"Shell completion:\n"
"  complete [WORD...] CURRENT    candidates for CURRENT after WORD..., one to a\n"
"                                line; exit status 3 asks for file names\n"
"\n"
"FILE is binary EDID or hexadecimal text, as printed by edid-decode or\n"
"xrandr --verbose; - reads standard input. A connected display is read from\n"
"its edid file, see displays. OUTPUT is written as hexadecimal text when it\n"
"ends in .hex or .txt or is -, standard output, and as binary otherwise.\n"
"\n"
"GROUP is an offset as listed by groups (0x036), a group code (DTD) when only\n"
"one group has it, the nth group with a code (DTD:2), or both (DTD@0x036).\n"
"FIELD is a field name as listed by fields, matched without case, spaces, '-'\n"
"and '_' (pixelclock), name:N for the nth field of that name, or #N for the\n"
"nth field. VALUE is text as the field shows it, a named value from describe,\n"
"or on/off for single bits. Detailed timings also have a Refresh field in Hz;\n"
"setting it sets the pixel clock, keeping the blanking.\n"
"\n"
"Options:\n"
"  -o, --output OUTPUT    where a changed EDID is written\n"
"  -i, --in-place         write a changed EDID back to FILE\n"
"      --ignore-errors    open EDID data that breaks the standard\n"
"      --edit-read-only   allow writing fields derived from other data\n"
"  -q, --quiet            leave out notices about the data\n"
"      --json             print info, groups, fields, diff and displays as JSON\n"
"  -h, --help             show this help\n"
"      --version          show the version\n";

struct options {
   std::vector<std::string> args;
   std::string output;
   bool in_place = false;
   bool ignore_errors = false;
   bool edit_read_only = false;
   bool quiet = false;
   bool json = false;
};

options opts;

void log_sink(const char* msg, void*) {
   if (0 == strncmp(msg, "[E!] ", 5)) {
      std::fprintf(stderr, "error: %s\n", msg + 5);
   } else if ((0 == strncmp(msg, "[i] ", 4)) && ! opts.quiet) {
      std::fprintf(stderr, "note: %s\n", msg + 4);
   }
   //other lines trace the parser
}

[[noreturn]] void fail(const std::string& message, int status = 1) {
   std::fprintf(stderr, "error: %s\n", message.c_str());
   std::exit(status);
}

[[noreturn]] void usage_error(const std::string& message) {
   std::fprintf(stderr, "error: %s\nRun edid-editor-cli --help for usage.\n",
                message.c_str());
   std::exit(2);
}

std::string rcode_text(rcode result) {
   char text[1024];
   wxedid_RCD_GET_MSG(result, text, sizeof(text));
   std::string message = text;
   //messages carry the log prefix of the core
   if (0 == message.compare(0, 5, "[E!] ")) message.erase(0, 5);
   return message;
}

bool ends_with_text_suffix(const std::string& path) {
   if (path.size() < 4) return false;
   std::string suffix = path.substr(path.size() - 4);
   return (0 == strcasecmp(suffix.c_str(), ".hex")) ||
          (0 == strcasecmp(suffix.c_str(), ".txt"));
}

//binary EDID starts with a fixed header; anything else is read as hex text
std::vector<u8_t> read_input(const std::string& path) {
   FILE* in = (path == "-") ? stdin : std::fopen(path.c_str(), "rb");
   if (in == NULL) fail("couldn’t open " + path + ": " + std::strerror(errno));
   std::vector<u8_t> data;
   u8_t chunk[4096];
   size_t count;
   while ((count = std::fread(chunk, 1, sizeof(chunk), in)) > 0) {
      data.insert(data.end(), chunk, chunk + count);
      if (data.size() > 65536) fail(path + " is too large to be EDID data");
   }
   bool failed = std::ferror(in) != 0;
   int error = errno;
   if (in != stdin) std::fclose(in);
   if (failed) fail("couldn’t read " + path + ": " + std::strerror(error));

   static const u8_t header[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
   if ((data.size() >= sizeof(header)) && (0 == memcmp(data.data(), header, sizeof(header)))) {
      return data;
   }
   std::vector<u8_t> bytes;
   std::string problem;
   if (edid_hex_decode(reinterpret_cast<const char*>(data.data()), data.size(),
                       bytes, problem)) {
      return bytes;
   }
   //neither: leave it to the parser to say what is wrong
   if (data.size() <= sizeof(edi_t)) return data;
   fail("couldn’t read " + path + " as EDID data: " + problem);
}

void write_output(const std::string& path, const u8_t* data, size_t size) {
   if (path == "-") {
      std::fputs(edid_hex_encode(data, size).c_str(), stdout);
      return;
   }
   if (0 == path.compare(0, 5, "/sys/")) {
      fail("connected displays are only read; write the EDID to a file");
   }
   std::string contents = ends_with_text_suffix(path)
      ? edid_hex_encode(data, size)
      : std::string(reinterpret_cast<const char*>(data), size);
   //a new file replaces the old one only once it is complete
   std::string temp = path + ".tmp-edid-editor";
   FILE* out = std::fopen(temp.c_str(), "wb");
   if (out == NULL) fail("couldn’t write " + path + ": " + std::strerror(errno));
   size_t written = std::fwrite(contents.data(), 1, contents.size(), out);
   int close_rc = std::fclose(out);
   if ((written != contents.size()) || (close_rc != 0) ||
       (std::rename(temp.c_str(), path.c_str()) != 0)) {
      int error = errno;
      std::remove(temp.c_str());
      fail("couldn’t write " + path + ": " + std::strerror(error));
   }
}

//what a change did; standard error when the EDID itself goes to standard output
FILE* report_stream() {
   return (opts.output == "-") ? stderr : stdout;
}

std::string output_path(const std::string& input) {
   if (opts.in_place && ! opts.output.empty()) usage_error("use either --output or --in-place");
   if (opts.in_place) {
      if (input == "-") usage_error("--in-place needs a file, not standard input");
      return input;
   }
   if (opts.output.empty()) usage_error("say where the result goes: -o OUTPUT or --in-place");
   return opts.output;
}

struct document {
   EDID_cl   EDID;
   guilog_cl log;
   std::string path;
};

void open_document(document& doc, const std::string& path) {
   doc.path = path;
   doc.log.SetSink(log_sink, NULL);
   doc.EDID.SetGuiLogPtr(&doc.log);
   doc.EDID.b_ERR_Ignore = opts.ignore_errors;
   doc.EDID.b_RD_Ignore = opts.edit_read_only;
   std::vector<u8_t> bytes = read_input(path);
   const char* name = (path == "-") ? "standard input" : path.c_str();
   edid_load_result loaded = edid_load(doc.EDID, bytes.data(), bytes.size(), name, doc.log);
   if (! loaded.opened) {
      if (loaded.can_retry && ! opts.ignore_errors) {
         std::fprintf(stderr, "To open it anyway, add --ignore-errors.\n");
      }
      std::exit(1);
   }
   if (loaded.extension_failed && ! opts.quiet) {
      std::fprintf(stderr, "note: extension blocks that failed are kept as data\n");
   }
}

void save_document(document& doc) {
   std::string path = output_path(doc.path);
   if (! edid_prepare_output(doc.EDID, doc.log)) std::exit(1);
   write_output(path, doc.EDID.getEDID()->buff,
                doc.EDID.getNumValidBlocks() * sizeof(ediblk_t));
}

std::string block_title(EDID_cl& EDID, u32_t block) {
   if (block == 0) return "Block 0: Base EDID";
   u8_t tag = EDID.getEDID()->blk[block][0];
   char text[64];
   if (EDID.BlkGroupsAr[block]->GetCount() == 0) {
      snprintf(text, sizeof(text), "Block %u: tag 0x%02X, kept as data", block, tag);
   } else {
      snprintf(text, sizeof(text), "Block %u: %s", block,
               (tag == 0x70) ? "DisplayID" : "CTA-861");
   }
   return text;
}

std::string group_name(EDID_cl& EDID, edi_grp_cl* group) {
   return edid_group_display_name(group, EDID);
}

struct listed_group {
   edi_grp_cl* group;
   u32_t       block;
   u32_t       depth;
};

void collect(std::vector<listed_group>& list, edi_grp_cl* group, u32_t block, u32_t depth) {
   list.push_back({group, block, depth});
   for (u32_t idx=0; idx<group->getSubGrpCount(); idx++) {
      edi_grp_cl* sub = group->getSubGroup(idx);
      if (sub != NULL) collect(list, sub, block, depth + 1);
   }
}

std::vector<listed_group> all_groups(EDID_cl& EDID) {
   std::vector<listed_group> list;
   for (u32_t block=0; block<EDID.getNumValidBlocks(); block++) {
      GroupAr_cl* array = EDID.BlkGroupsAr[block];
      for (u32_t idx=0; idx<array->GetCount(); idx++) {
         collect(list, array->Item(idx), block, 0);
      }
   }
   return list;
}

std::string address(edi_grp_cl* group) {
   char text[32];
   snprintf(text, sizeof(text), "%s@0x%03X", group->CodeName.c_str(), group->getAbsOffs());
   return text;
}

bool parse_number(const std::string& text, int base, u32_t& value) {
   if (text.empty()) return false;
   char* end = NULL;
   errno = 0;
   unsigned long parsed = std::strtoul(text.c_str(), &end, base);
   if ((errno != 0) || (*end != 0) || (parsed > 0xFFFFFFFFUL)) return false;
   value = static_cast<u32_t>(parsed);
   return true;
}

listed_group find_group(EDID_cl& EDID, const std::string& spec) {
   std::string code = spec;
   std::string offset_text;
   u32_t nth = 0;
   size_t at = spec.find('@');
   if (at != std::string::npos) {
      code = spec.substr(0, at);
      offset_text = spec.substr(at + 1);
   } else if ((spec.size() > 2) && (spec[0] == '0') && ((spec[1] == 'x') || (spec[1] == 'X'))) {
      code.clear();
      offset_text = spec;
   } else {
      size_t colon = spec.find(':');
      if (colon != std::string::npos) {
         code = spec.substr(0, colon);
         if (! parse_number(spec.substr(colon + 1), 10, nth) || (nth == 0)) {
            usage_error("in " + spec + ", the number after ':' counts from 1");
         }
      }
   }
   u32_t offset = 0;
   if (! offset_text.empty() && ! parse_number(offset_text, 16, offset)) {
      usage_error(offset_text + " is not an offset such as 0x036");
   }

   std::vector<listed_group> matches;
   for (const listed_group& entry : all_groups(EDID)) {
      if (! code.empty() && (0 != strcasecmp(code.c_str(), entry.group->CodeName.c_str()))) {
         continue;
      }
      if (! offset_text.empty() && (entry.group->getAbsOffs() != offset)) continue;
      matches.push_back(entry);
   }
   if (nth > 0) {
      if (nth > matches.size()) {
         fail("there are " + std::to_string(matches.size()) + " groups with the code " + code);
      }
      return matches[nth - 1];
   }
   if (matches.empty()) fail("no group matches " + spec + "; see the groups command");
   if (matches.size() > 1) {
      std::string list;
      for (const listed_group& entry : matches) list += " " + address(entry.group);
      fail(spec + " matches several groups:" + list);
   }
   return matches[0];
}

bool field_is_reserved(const edi_field_t& f) {
   const char* name = (f.name != NULL) ? f.name : "";
   if ((0 == strncasecmp(name, "rsvd", 4)) || (0 == strncasecmp(name, "resvd", 5)) ||
       (0 == strncasecmp(name, "reserved", 8))) return true;
   return (0 == strncasecmp(name, "res", 3)) && (name[3] >= '0') && (name[3] <= '9');
}

bool field_has_selector(const edi_field_t& f) {
   return ((f.flags & F_VS) != 0) && (f.vmap_idx != VS_NO_SELECTOR);
}

//field names are matched without case, spaces, '-' and '_'
std::string name_key(const char* name) {
   std::string key;
   for (const char* chr = name; *chr != 0; chr++) {
      if ((*chr == ' ') || (*chr == '-') || (*chr == '_')) continue;
      key += static_cast<char>(((*chr >= 'A') && (*chr <= 'Z')) ? *chr - 'A' + 'a' : *chr);
   }
   return key;
}

edi_dynfld_t* find_field(edi_grp_cl* group, const std::string& spec) {
   u32_t count = group->FieldsAr.GetCount();
   if (! spec.empty() && (spec[0] == '#')) {
      u32_t index = 0;
      if (! parse_number(spec.substr(1), 10, index) || (index == 0) || (index > count)) {
         fail("the group has fields #1 to #" + std::to_string(count));
      }
      return group->FieldsAr.Item(index - 1);
   }
   auto named = [&](const std::string& name) {
      std::vector<edi_dynfld_t*> found;
      for (u32_t idx=0; idx<count; idx++) {
         edi_dynfld_t* field = group->FieldsAr.Item(idx);
         if ((field->field.name != NULL) &&
             ((name_key(field->field.name) == name_key(name.c_str())) ||
              (name_key(edid_field_display_name(field->field.name).c_str()) ==
               name_key(name.c_str())))) {
            found.push_back(field);
         }
      }
      return found;
   };
   //a name may itself end in :N, as "YCbCr 4:2:2" does
   std::string name = spec;
   u32_t nth = 0;
   std::vector<edi_dynfld_t*> matches = named(name);
   size_t colon = spec.rfind(':');
   if (matches.empty() && (colon != std::string::npos) &&
       parse_number(spec.substr(colon + 1), 10, nth)) {
      name = spec.substr(0, colon);
      matches = named(name);
   }
   if (matches.empty()) fail("the group has no field " + name + "; see the fields command");
   if (nth > 0) {
      if (nth > matches.size()) {
         fail("the group has " + std::to_string(matches.size()) + " fields named " + name);
      }
      return matches[nth - 1];
   }
   if (matches.size() > 1) {
      fail("the group has " + std::to_string(matches.size()) + " fields named " + name +
           "; use " + name + ":N or #N");
   }
   return matches[0];
}

struct field_value {
   std::string text;  //as the field shows it
   std::string label; //the name of a named value
   u32_t       value = 0;
   bool        ok = false;
};

field_value read_field(EDID_cl& EDID, edi_dynfld_t* field) {
   field_value result;
   wxc_String text;
   rcode ret = (EDID.*field->field.handlerfn)(OP_READ, text, result.value, field);
   result.ok = RCD_IS_OK(ret);
   result.text = result.ok ? text.std_str() : "<" + rcode_text(ret) + ">";
   if (result.ok && field_has_selector(field->field)) {
      wxc_String label;
      EDID.getValDesc(label, field, result.value, VD_NAME);
      result.label = label.std_str();
   }
   return result;
}

std::string shown(const field_value& value) {
   return value.label.empty() ? value.text : value.text + " (" + value.label + ")";
}

//the unit, unless the field name already ends with it, as in "Horizontal active pixels"
std::string unit_of(EDID_cl& EDID, const edi_field_t& f) {
   wxc_String unit;
   EDID.getValUnitName(unit, f.flags);
   std::string name = edid_field_display_name((f.name != NULL) ? f.name : "");
   for (const char* suffix : {" pix", " lines", " px"}) {
      size_t length = strlen(suffix);
      if ((name.size() > length) && (0 == name.compare(name.size() - length, length, suffix))) {
         return "";
      }
   }
   if (! unit.IsEmpty() && (name.size() > unit.Len()) &&
       (0 == name.compare(name.size() - unit.Len() - 1, unit.Len() + 1, " " + unit.std_str()))) {
      return "";
   }
   return (unit == wxc_String("pix")) ? "px" : unit.std_str();
}

//The refresh rate of a detailed timing is a field of the command line only:
//it follows from the pixel clock and the totals, and setting it changes the
//pixel clock, as the timing editor does.
bool is_refresh(edi_grp_cl* group, const std::string& spec) {
   std::string key = name_key(spec.c_str());
   if ((key != "refresh") && (key != "refreshrate") && (key != "verticalrefresh")) return false;
   //a field of the name, as standard timings have, comes first
   for (u32_t idx=0; idx<group->FieldsAr.GetCount(); idx++) {
      const char* name = group->FieldsAr.Item(idx)->field.name;
      if ((name != NULL) && ((name_key(name) == key) ||
                             (name_key(edid_field_display_name(name).c_str()) == key))) {
         return false;
      }
   }
   return true;
}

struct timing_state {
   edid_timing_layout layout;
   u32_t  values[TIMING_FIELD_COUNT];
   double htotal;
   double vtotal;
   double refresh;
};

bool read_timing(EDID_cl& EDID, edi_grp_cl* group, timing_state& timing) {
   if (! edid_timing_layout_of(group, timing.layout)) return false;
   for (int idx=0; idx<TIMING_FIELD_COUNT; idx++) {
      timing.values[idx] = 0;
      if (timing.layout.fields[idx] < 0) continue;
      field_value value = read_field(EDID, group->FieldsAr.Item(timing.layout.fields[idx]));
      if (! value.ok) return false;
      timing.values[idx] = value.value;
   }
   timing.htotal = static_cast<double>(timing.values[TIMING_HACTIVE]) +
                   timing.values[TIMING_HBLANK];
   timing.vtotal = static_cast<double>(timing.values[TIMING_VACTIVE]) +
                   timing.values[TIMING_VBLANK];
   if ((timing.htotal <= 0.0) || (timing.vtotal <= 0.0)) return false;
   timing.refresh = timing.values[TIMING_PIXCLK] * timing.layout.pixel_hz /
                    (timing.htotal * timing.vtotal);
   return true;
}

std::string hz_text(double hz) {
   char text[32];
   snprintf(text, sizeof(text), "%.2f", hz);
   return text;
}

timing_state need_timing(EDID_cl& EDID, edi_grp_cl* group) {
   timing_state timing;
   if (! read_timing(EDID, group, timing)) {
      fail(address(group) + " is not a detailed timing; only those have a refresh rate");
   }
   return timing;
}

//-------- JSON

std::string json_string(const std::string& text) {
   std::string quoted = "\"";
   for (unsigned char chr : text) {
      if ((chr == '"') || (chr == '\\')) {
         quoted += '\\';
         quoted += static_cast<char>(chr);
      } else if (chr < 0x20) {
         char escape[8];
         snprintf(escape, sizeof(escape), "\\u%04X", chr);
         quoted += escape;
      } else {
         quoted += static_cast<char>(chr);
      }
   }
   return quoted + "\"";
}

//members are added in order; values that are already JSON go in with raw()
struct json_object {
   std::string members;

   json_object& raw(const char* key, const std::string& json) {
      if (! members.empty()) members += ", ";
      members += json_string(key) + ": " + json;
      return *this;
   }
   json_object& text(const char* key, const std::string& value) {
      return raw(key, json_string(value));
   }
   json_object& text_or_null(const char* key, const std::string& value) {
      return raw(key, value.empty() ? "null" : json_string(value));
   }
   json_object& number(const char* key, u32_t value) {
      return raw(key, std::to_string(value));
   }
   json_object& flag(const char* key, bool value) {
      return raw(key, value ? "true" : "false");
   }
   std::string str() const { return "{" + members + "}"; }
};

//an array with one element to a line
std::string json_array(const std::vector<std::string>& elements) {
   if (elements.empty()) return "[]";
   std::string array = "[";
   for (size_t idx=0; idx<elements.size(); idx++) {
      array += ((idx == 0) ? "\n  " : ",\n  ") + elements[idx];
   }
   return array + "\n]";
}

void print_json(const std::string& json) {
   std::printf("%s\n", json.c_str());
}

//-------- commands

void need_args(size_t count, const char* synopsis) {
   if (opts.args.size() != count) usage_error(std::string("usage: edid-editor-cli ") + synopsis);
}

int cmd_info() {
   need_args(2, "info FILE");
   document doc;
   open_document(doc, opts.args[1]);
   std::vector<edid_summary_item> items = edid_summary(doc.EDID);
   if (opts.json) {
      std::vector<std::string> elements;
      for (const edid_summary_item& item : items) {
         elements.push_back(json_object().text("section", item.section)
                            .text("label", item.label).text("value", item.value).str());
      }
      print_json(json_array(elements));
      return 0;
   }
   std::string section;
   size_t width = 0;
   for (const edid_summary_item& item : items) width = std::max(width, item.label.size());
   for (const edid_summary_item& item : items) {
      if (item.section != section) {
         if (! section.empty()) std::printf("\n");
         section = item.section;
         std::printf("%s\n", section.c_str());
      }
      std::printf("  %-*s  %s\n", static_cast<int>(width), item.label.c_str(),
                  item.value.c_str());
   }
   return 0;
}

int cmd_report() {
   need_args(2, "report FILE");
   document doc;
   open_document(doc, opts.args[1]);
   std::fputs(edid_text_report(doc.EDID, doc.path.c_str(), EDID_EDITOR_VERSION).c_str(),
              stdout);
   return 0;
}

int cmd_groups() {
   need_args(2, "groups FILE");
   document doc;
   open_document(doc, opts.args[1]);
   std::vector<listed_group> list = all_groups(doc.EDID);
   if (opts.json) {
      std::vector<std::string> elements;
      for (const listed_group& entry : list) {
         elements.push_back(json_object().text("address", address(entry.group))
                            .number("block", entry.block)
                            .number("offset", entry.group->getAbsOffs())
                            .text("code", entry.group->CodeName.c_str())
                            .text("name", group_name(doc.EDID, entry.group))
                            .number("depth", entry.depth).str());
      }
      print_json(json_array(elements));
      return 0;
   }
   for (u32_t idx=0; idx<doc.EDID.getNumValidBlocks(); idx++) {
      if (idx > 0) std::printf("\n");
      std::printf("%s\n", block_title(doc.EDID, idx).c_str());
      for (const listed_group& entry : list) {
         if (entry.block != idx) continue;
         std::printf("  0x%03X  %-*s%-9s %s\n", entry.group->getAbsOffs(),
                     static_cast<int>(entry.depth * 2), "",
                     entry.group->CodeName.c_str(),
                     group_name(doc.EDID, entry.group).c_str());
      }
   }
   return 0;
}

int cmd_fields() {
   need_args(3, "fields FILE GROUP");
   document doc;
   open_document(doc, opts.args[1]);
   listed_group found = find_group(doc.EDID, opts.args[2]);
   edi_grp_cl* group = found.group;
   u32_t count = group->FieldsAr.GetCount();
   timing_state timing;
   bool has_refresh = read_timing(doc.EDID, group, timing);
   if (opts.json) {
      std::vector<std::string> elements;
      for (u32_t idx=0; idx<count; idx++) {
         edi_dynfld_t* field = group->FieldsAr.Item(idx);
         const edi_field_t& f = field->field;
         field_value value = read_field(doc.EDID, field);
         json_object element;
         element.number("index", idx + 1).text("name", edid_field_display_name(f.name))
                .text("core_name", (f.name != NULL) ? f.name : "");
         if (value.ok) {
            element.text("value", value.text).number("raw", value.value)
                   .text_or_null("value_name", value.label);
         } else {
            element.raw("value", "null").text("error", value.text.substr(1, value.text.size() - 2));
         }
         element.text("unit", unit_of(doc.EDID, f))
                .flag("read_only", (f.flags & F_RD) != 0)
                .flag("reserved", field_is_reserved(f))
                .flag("not_used", (f.flags & F_NU) != 0)
                .flag("named_values", field_has_selector(f));
         elements.push_back(element.str());
      }
      json_object object;
      object.text("address", address(group)).text("name", group_name(doc.EDID, group))
            .number("block", found.block)
            .raw("refresh", has_refresh ? hz_text(timing.refresh) : "null")
            .raw("fields", json_array(elements));
      print_json(object.str());
      return 0;
   }
   std::printf("%s  %s, block %u\n", address(group).c_str(),
               group_name(doc.EDID, group).c_str(), found.block);
   size_t name_width = 4;
   size_t value_width = 5;
   std::vector<field_value> values;
   for (u32_t idx=0; idx<count; idx++) {
      edi_dynfld_t* field = group->FieldsAr.Item(idx);
      values.push_back(read_field(doc.EDID, field));
      name_width = std::max(name_width, edid_field_display_name(field->field.name).size());
      value_width = std::max(value_width, shown(values.back()).size());
   }
   value_width = std::min<size_t>(value_width, 40);
   for (u32_t idx=0; idx<count; idx++) {
      const edi_field_t& f = group->FieldsAr.Item(idx)->field;
      std::string notes;
      std::string unit = unit_of(doc.EDID, f);
      if ((f.flags & F_RD) != 0) notes += " read-only";
      if (field_is_reserved(f)) notes += " reserved";
      if ((f.flags & F_NU) != 0) notes += " not-used";
      if (field_has_selector(f)) notes += " named-values";
      std::printf("  #%-3u %-*s  %-*s  %-8s%s\n", idx + 1,
                  static_cast<int>(name_width), edid_field_display_name(f.name).c_str(),
                  static_cast<int>(value_width), shown(values[idx]).c_str(),
                  unit.c_str(), notes.c_str());
   }
   if (has_refresh) {
      std::printf("       %-*s  %-*s  %-8s%s\n", static_cast<int>(name_width), "Refresh",
                  static_cast<int>(value_width), hz_text(timing.refresh).c_str(), "Hz",
                  " derived: setting it changes the pixel clock");
   }
   return 0;
}

int cmd_describe() {
   need_args(4, "describe FILE GROUP FIELD");
   document doc;
   open_document(doc, opts.args[1]);
   edi_grp_cl* group = find_group(doc.EDID, opts.args[2]).group;
   if (is_refresh(group, opts.args[3])) {
      timing_state timing = need_timing(doc.EDID, group);
      std::printf("%s Refresh\n", address(group).c_str());
      std::printf("  value:  %s\n", hz_text(timing.refresh).c_str());
      std::printf("  unit:   Hz\n");
      std::printf("\nThe vertical refresh rate: the pixel clock divided by the horizontal and\n"
                  "vertical totals. Setting it sets the pixel clock that comes closest to it\n"
                  "with the current totals, in steps of the pixel clock's unit; the blanking\n"
                  "stays as it is.\n");
      return 0;
   }
   edi_dynfld_t* field = find_field(group, opts.args[3]);
   const edi_field_t& f = field->field;
   std::string plain = edid_field_display_name(f.name);
   std::printf("%s %s", address(group).c_str(), plain.c_str());
   if (plain != f.name) std::printf(" (%s)", f.name);
   std::printf("\n");
   std::printf("  value:  %s\n", shown(read_field(doc.EDID, field)).c_str());
   std::string unit = unit_of(doc.EDID, f);
   if (! unit.empty()) std::printf("  unit:   %s\n", unit.c_str());
   if ((f.maxv > f.minv) && ((f.flags & F_FLT) == 0)) {
      std::printf("  range:  %u to %u\n", f.minv, f.maxv);
   }
   long start = (field->base + f.offs) - group->getInstPtr();
   if (start >= 0) {
      u32_t first = group->getAbsOffs() + static_cast<u32_t>(start);
      u32_t count = ((f.flags & (F_BIT | F_BFD)) != 0) ? 1 : std::max<u32_t>(1, f.fld_sz);
      if ((f.flags & F_BIT) != 0) {
         std::printf("  bytes:  bit %u of byte 0x%03X\n", f.shift, first);
      } else if (count == 1) {
         std::printf("  bytes:  0x%03X\n", first);
      } else {
         std::printf("  bytes:  0x%03X to 0x%03X\n", first, first + count - 1);
      }
   }
   if ((f.flags & F_RD) != 0) {
      std::printf("  derived from other data: writing it needs --edit-read-only\n");
   }
   if ((f.desc != NULL) && (f.desc[0] != 0)) std::printf("\n%s\n", f.desc);
   if (field_has_selector(f)) {
      sm_vmap* vmap = vmap_GetVmap(f.vmap_idx, VMAP_MID);
      if (vmap != NULL) {
         std::printf("\nNamed values:\n");
         for (auto& entry : *vmap) {
            u32_t value = ((f.flags & F_VSVM) != 0) ? entry.second.val : entry.first;
            std::printf("  %-6u %s\n", value, entry.second.name);
         }
      }
   }
   return 0;
}

int cmd_get() {
   need_args(4, "get FILE GROUP FIELD");
   document doc;
   open_document(doc, opts.args[1]);
   edi_grp_cl* group = find_group(doc.EDID, opts.args[2]).group;
   if (is_refresh(group, opts.args[3])) {
      std::printf("%s\n", hz_text(need_timing(doc.EDID, group).refresh).c_str());
      return 0;
   }
   field_value value = read_field(doc.EDID, find_field(group, opts.args[3]));
   if (! value.ok) fail(value.text);
   std::printf("%s\n", value.text.c_str());
   return 0;
}

//the pixel clock that gives a refresh rate with the current totals
void write_refresh(document& doc, edi_grp_cl* group, const std::string& text) {
   timing_state timing = need_timing(doc.EDID, group);
   char* end = NULL;
   double target = std::strtod(text.c_str(), &end);
   if (text.empty() || (*end != 0) || !(target > 0.0)) {
      fail(address(group) + " Refresh: " + text + " is not a rate in Hz");
   }
   edi_dynfld_t* field = group->FieldsAr.Item(timing.layout.fields[TIMING_PIXCLK]);
   double maximum = (timing.layout.clock_max > 0.0) ? timing.layout.clock_max
                                                    : field->field.maxv;
   double clock = edid_timing_clock_for(timing.layout, target, timing.htotal, timing.vtotal);
   if (clock > maximum) {
      fail(address(group) + " Refresh: " + text + " Hz needs a pixel clock above the "
           "largest this timing can hold");
   }
   field_value before = read_field(doc.EDID, field);
   u32_t value = static_cast<u32_t>(clock);
   wxc_String unused;
   rcode result = (doc.EDID.*field->field.handlerfn)(OP_WRINT, unused, value, field);
   if (! RCD_IS_OK(result)) fail(address(group) + " Refresh: " + rcode_text(result));
   timing_state after = need_timing(doc.EDID, group);
   std::fprintf(report_stream(), "%s Refresh: %s -> %s Hz (Pixel clock: %s -> %s)\n",
                address(group).c_str(), hz_text(timing.refresh).c_str(),
                hz_text(after.refresh).c_str(), before.text.c_str(),
                read_field(doc.EDID, field).text.c_str());
}

//write one field the way the editor does, rebuilding the group when its
//type or layout follows from the field
void write_field(document& doc, const std::string& group_spec, const std::string& field_spec,
                 const std::string& text) {
   edi_grp_cl* group = find_group(doc.EDID, group_spec).group;
   if (is_refresh(group, field_spec)) {
      write_refresh(doc, group, text);
      return;
   }
   edi_dynfld_t* field = find_field(group, field_spec);
   const edi_field_t& f = field->field;
   std::string where = address(group) + " " + edid_field_display_name(f.name);
   if (((f.flags & F_RD) != 0) && ! opts.edit_read_only) {
      fail(where + " is derived from other data; add --edit-read-only to change it");
   }
   field_value before = read_field(doc.EDID, field);

   rcode result;
   bool written = false;
   sm_vmap* vmap = field_has_selector(f) ? vmap_GetVmap(f.vmap_idx, VMAP_MID) : NULL;
   if (vmap != NULL) {
      for (auto& entry : *vmap) {
         if (0 != strcasecmp(entry.second.name, text.c_str())) continue;
         u32_t value = ((f.flags & F_VSVM) != 0) ? entry.second.val : entry.first;
         wxc_String unused;
         result = (doc.EDID.*f.handlerfn)(OP_WRINT, unused, value, field);
         written = true;
         break;
      }
   }
   //bit fields shown in binary, as 0b11, also take a decimal number
   u32_t number = 0;
   if (! written && ((f.flags & F_BFD) != 0) && ((f.flags & F_INT) == 0) &&
       (text.find_first_not_of("0123456789") == std::string::npos) &&
       parse_number(text, 10, number)) {
      if ((number < f.minv) || (number > f.maxv)) {
         fail(where + ": " + text + " is not a valid value (" + std::to_string(f.minv) +
              " to " + std::to_string(f.maxv) + ")");
      }
      wxc_String unused;
      result = (doc.EDID.*f.handlerfn)(OP_WRINT, unused, number, field);
      written = true;
   }
   if (! written) {
      std::string value = text;
      if ((f.flags & F_BIT) != 0) {
         if ((0 == strcasecmp(text.c_str(), "on")) || (0 == strcasecmp(text.c_str(), "true")) ||
             (0 == strcasecmp(text.c_str(), "yes"))) value = "1";
         if ((0 == strcasecmp(text.c_str(), "off")) || (0 == strcasecmp(text.c_str(), "false")) ||
             (0 == strcasecmp(text.c_str(), "no"))) value = "0";
      }
      wxc_String sval(value.c_str());
      u32_t unused = 0;
      result = (doc.EDID.*f.handlerfn)(OP_WRSTR, sval, unused, field);
   }
   if (! RCD_IS_OK(result)) {
      //only volatile-message faults carry text; others name a source location
      std::string reason = (result.detail.rcode == RCD_FVMSG) ? rcode_text(result)
                                                              : "not a valid value";
      if ((result.detail.rcode != RCD_FVMSG) && (f.maxv > f.minv) && ((f.flags & F_FLT) == 0)) {
         reason += " (" + std::to_string(f.minv) + " to " + std::to_string(f.maxv) + ")";
      }
      fail(where + ": " + text + " is " + reason);
   }

   field_value after = read_field(doc.EDID, field);
   std::fprintf(report_stream(), "%s: %s -> %s\n", where.c_str(), shown(before).c_str(),
                shown(after).c_str());

   bool type_changed = RCD_IS_TRUE(result);
   if (! type_changed && ((f.flags & (F_FR | F_INIT)) == 0)) return;
   edi_grp_cl* target = NULL;
   rcode rebuild_result;
   edi_grp_cl* rebuilt = doc.EDID.RebuildGroup(group, field, type_changed, &target,
                                               rebuild_result);
   if ((rebuilt == NULL) && ! RCD_IS_OK(rebuild_result)) {
      fail(where + ": this change is not possible here");
   }
   if (rebuilt == NULL) return;
   if (! EDID_cl::ReplaceGroup(target, rebuilt)) {
      delete rebuilt;
      fail(where + ": this change does not fit in the block");
   }
   std::fprintf(report_stream(), "%s rebuilt as %s %s\n", address(target).c_str(),
                address(rebuilt).c_str(), group_name(doc.EDID, rebuilt).c_str());
   delete target;
}

int cmd_set() {
   if (opts.args.size() < 4) usage_error("usage: edid-editor-cli set FILE GROUP FIELD=VALUE...");
   document doc;
   open_document(doc, opts.args[1]);
   output_path(doc.path);
   for (size_t idx=3; idx<opts.args.size(); idx++) {
      size_t equals = opts.args[idx].find('=');
      if ((equals == std::string::npos) || (equals == 0)) {
         usage_error(opts.args[idx] + " is not FIELD=VALUE");
      }
   }
   for (size_t idx=3; idx<opts.args.size(); idx++) {
      const std::string& assignment = opts.args[idx];
      size_t equals = assignment.find('=');
      write_field(doc, opts.args[2], assignment.substr(0, equals),
                  assignment.substr(equals + 1));
   }
   save_document(doc);
   return 0;
}

GroupAr_cl* block_array(document& doc, const std::string& spec, u8_t& tag) {
   u32_t block = 0;
   if (! parse_number(spec, 10, block) || (block >= doc.EDID.getNumValidBlocks())) {
      fail("there is no block " + spec + "; see the groups command");
   }
   GroupAr_cl* array = doc.EDID.BlkGroupsAr[block];
   tag = (block == 0) ? 0 : doc.EDID.getEDID()->blk[block][0];
   if ((block == 0) || (array->GetCount() == 0)) {
      fail("groups can be added to CTA-861 and DisplayID blocks only");
   }
   return array;
}

int cmd_add() {
   bool with_source = (opts.args.size() == 5) && (opts.args[3] == "timing");
   if (! with_source) need_args(4, "add FILE BLOCK KIND, or add FILE BLOCK timing [GROUP]");
   document doc;
   open_document(doc, opts.args[1]);
   output_path(doc.path);
   u8_t tag = 0;
   GroupAr_cl* array = block_array(doc, opts.args[2], tag);
   const std::string& kind = opts.args[3];
   edi_grp_cl* group = NULL;
   rcode result;
   if (kind == "displayid") {
      if (tag != 0x70) fail("a DisplayID data block goes into a DisplayID block");
      u8_t version = array->Item(0)->getInstPtr()[1];
      result = doc.EDID.CreateGroup(EDID_cl::DISPLAYID_DATA, version, &group);
   } else {
      EDID_cl::group_template which;
      if (kind == "audio-lpcm") which = EDID_cl::CEA_AUDIO_LPCM;
      else if (kind == "audio-extended") which = EDID_cl::CEA_AUDIO_EXTENDED;
      else if (kind == "video") which = EDID_cl::CEA_VIDEO;
      else if (kind == "timing") which = EDID_cl::CEA_TIMING;
      else usage_error("KIND is audio-lpcm, audio-extended, video, timing or displayid");
      if (tag != 0x02) fail(kind + " goes into a CTA-861 block");
      result = doc.EDID.CreateGroup(which, 0, &group);
   }
   if (! RCD_IS_OK(result)) fail(rcode_text(result));
   //a new timing starts as a copy of the one given, or of the first
   if (kind == "timing") {
      edi_grp_cl* source = with_source ? find_group(doc.EDID, opts.args[4]).group
                                       : edid_first_timing(doc.EDID);
      if ((source != NULL) && ! edid_timing_copy(doc.EDID, source, group)) {
         delete group;
         fail(address(source) + " doesn't fit a detailed timing of a CTA-861 block");
      }
   }
   if (! edid_insert_group(array, group)) {
      delete group;
      fail("the group does not fit in block " + opts.args[2]);
   }
   std::fprintf(report_stream(), "added %s %s\n", address(group).c_str(),
                group_name(doc.EDID, group).c_str());
   save_document(doc);
   return 0;
}

int cmd_duplicate() {
   need_args(3, "duplicate FILE GROUP");
   document doc;
   open_document(doc, opts.args[1]);
   output_path(doc.path);
   edi_grp_cl* group = find_group(doc.EDID, opts.args[2]).group;
   GroupAr_cl* array = group->getParentAr();
   gtid_t type = group->getTypeID();
   if ((array == NULL) || type.t_gp_fixed || type.t_no_copy) {
      fail(address(group) + " can't be duplicated");
   }
   rcode result;
   edi_grp_cl* copy = group->Clone(result, T_MODE_EDIT);
   if ((copy == NULL) || ! RCD_IS_OK(result) ||
       ! array->CanInsertDn(group->getParentArIdx(), copy)) {
      delete copy;
      fail(address(group) + " can't be duplicated in the available space");
   }
   array->InsertDn(group->getParentArIdx(), copy);
   std::fprintf(report_stream(), "added %s %s\n", address(copy).c_str(),
                group_name(doc.EDID, copy).c_str());
   save_document(doc);
   return 0;
}

int cmd_delete() {
   need_args(3, "delete FILE GROUP");
   document doc;
   open_document(doc, opts.args[1]);
   output_path(doc.path);
   edi_grp_cl* group = find_group(doc.EDID, opts.args[2]).group;
   GroupAr_cl* array = group->getParentAr();
   if ((array == NULL) || ! array->CanDelete(group->getParentArIdx())) {
      fail(address(group) + " can't be deleted");
   }
   std::string removed = address(group) + " " + group_name(doc.EDID, group);
   if (array->Cut(group->getParentArIdx()) != group) fail(address(group) + " couldn’t be deleted");
   delete group;
   std::fprintf(report_stream(), "deleted %s\n", removed.c_str());
   save_document(doc);
   return 0;
}

int cmd_move() {
   need_args(4, "move FILE GROUP up|down");
   document doc;
   open_document(doc, opts.args[1]);
   output_path(doc.path);
   bool up = (opts.args[3] == "up");
   if (! up && (opts.args[3] != "down")) usage_error("a group moves up or down");
   edi_grp_cl* group = find_group(doc.EDID, opts.args[2]).group;
   GroupAr_cl* array = group->getParentAr();
   u32_t index = (array != NULL) ? group->getParentArIdx() : 0;
   if ((array == NULL) || ! (up ? array->CanMoveUp(index) : array->CanMoveDn(index))) {
      fail(address(group) + " can't move " + opts.args[3]);
   }
   std::string before = address(group);
   if (up) array->MoveUp(index); else array->MoveDn(index);
   std::fprintf(report_stream(), "moved %s to %s\n", before.c_str(), address(group).c_str());
   save_document(doc);
   return 0;
}

int cmd_prefer() {
   need_args(3, "prefer FILE GROUP");
   document doc;
   open_document(doc, opts.args[1]);
   output_path(doc.path);
   edi_grp_cl* group = find_group(doc.EDID, opts.args[2]).group;
   std::vector<edid_data_change> changes;
   std::string message;
   if (! edid_plan_preferred(doc.EDID, group, changes, message)) fail(address(group) + ": " + message);
   edid_apply_changes(changes, true);
   std::fprintf(report_stream(), "%s\n", message.c_str());
   save_document(doc);
   return 0;
}

int cmd_diff() {
   need_args(3, "diff FILE1 FILE2");
   document one;
   document two;
   open_document(one, opts.args[1]);
   open_document(two, opts.args[2]);
   std::vector<edid_difference> found = edid_compare(one.EDID, two.EDID);
   if (opts.json) {
      //a whole group present on one side only has no field, and no value on the other
      std::vector<std::string> elements;
      for (const edid_difference& difference : found) {
         elements.push_back(json_object().text("place", difference.place)
                            .text_or_null("field", difference.field)
                            .text_or_null("left", difference.left)
                            .text_or_null("right", difference.right).str());
      }
      print_json(json_array(elements));
      return found.empty() ? 0 : 1;
   }
   std::string place;
   for (const edid_difference& difference : found) {
      if (difference.place != place) {
         place = difference.place;
         std::printf("%s\n", place.c_str());
      }
      if (difference.field.empty()) {
         std::printf("  only in %s\n", difference.left.empty() ? opts.args[2].c_str()
                                                              : opts.args[1].c_str());
      } else {
         std::printf("  %s: %s -> %s\n", difference.field.c_str(),
                     difference.left.empty() ? "(none)" : difference.left.c_str(),
                     difference.right.empty() ? "(none)" : difference.right.c_str());
      }
   }
   return found.empty() ? 0 : 1;
}

int cmd_displays() {
   need_args(1, "displays");
   std::vector<edid_display> displays = edid_connected_displays("/sys/class/drm");
   if (opts.json) {
      std::vector<std::string> elements;
      for (const edid_display& display : displays) {
         elements.push_back(json_object().text("connector", display.connector)
                            .text("name", display.name).text("path", display.path).str());
      }
      print_json(json_array(elements));
      return displays.empty() ? 1 : 0;
   }
   if (displays.empty()) {
      std::fprintf(stderr, "No connected display has EDID data under /sys/class/drm.\n");
      return 1;
   }
   for (const edid_display& display : displays) {
      std::printf("%-16s %-24s %s\n", display.connector.c_str(), display.name.c_str(),
                  display.path.c_str());
   }
   return 0;
}

//these work on the bytes, so they also serve data the parser refuses
int cmd_fix_checksums() {
   need_args(2, "fix-checksums FILE");
   std::string path = output_path(opts.args[1]);
   std::vector<u8_t> bytes = read_input(opts.args[1]);
   if ((bytes.empty()) || (bytes.size() % sizeof(ediblk_t) != 0) ||
       (bytes.size() > sizeof(edi_t))) {
      fail("EDID data must contain 1 to 4 complete 128-byte blocks");
   }
   for (size_t block=0; block<bytes.size() / sizeof(ediblk_t); block++) {
      u8_t* data = bytes.data() + (block * sizeof(ediblk_t));
      u8_t sum = 0;
      for (size_t idx=0; idx<sizeof(ediblk_t) - 1; idx++) sum += data[idx];
      u8_t checksum = static_cast<u8_t>(0x100 - sum);
      if (data[sizeof(ediblk_t) - 1] != checksum) {
         std::fprintf(report_stream(), "block %zu: checksum 0x%02X -> 0x%02X\n", block,
                      data[sizeof(ediblk_t) - 1], checksum);
         data[sizeof(ediblk_t) - 1] = checksum;
      }
   }
   write_output(path, bytes.data(), bytes.size());
   return 0;
}

int cmd_convert() {
   need_args(2, "convert FILE");
   std::string path = output_path(opts.args[1]);
   std::vector<u8_t> bytes = read_input(opts.args[1]);
   write_output(path, bytes.data(), bytes.size());
   return 0;
}

int cmd_complete();

//the arguments of a command after COMMAND: F a file, G a group, N a field,
//A any number of FIELD=VALUE, B a block, K a kind of group, M up or down
const struct command {
   const char* name;
   int (*run)();
   const char* arguments;
   const char* summary;
} commands[] = {
   {"info", cmd_info, "F", "overview of the display"},
   {"report", cmd_report, "F", "every group, field, value and unit"},
   {"groups", cmd_groups, "F", "groups and their addresses"},
   {"fields", cmd_fields, "FG", "fields of a group"},
   {"describe", cmd_describe, "FGN", "a field's description, range and named values"},
   {"get", cmd_get, "FGN", "the value of a field"},
   {"diff", cmd_diff, "FF", "fields that differ"},
   {"displays", cmd_displays, "", "connected displays"},
   {"set", cmd_set, "FGA", "write fields"},
   {"add", cmd_add, "FBKG", "add a group to a block"},
   {"duplicate", cmd_duplicate, "FG", "copy a group after itself"},
   {"delete", cmd_delete, "FG", "remove a group"},
   {"move", cmd_move, "FGM", "move a group within its block"},
   {"prefer", cmd_prefer, "FG", "make a detailed timing the preferred one"},
   {"fix-checksums", cmd_fix_checksums, "F", "recompute the checksum of every block"},
   {"convert", cmd_convert, "F", "the same bytes as binary or hexadecimal text"},
   {"complete", cmd_complete, "", ""},
};

//a candidate, with a description after a tab for the shells that show one
void candidate(const std::string& word, std::string description = "") {
   std::replace(description.begin(), description.end(), '\t', ' ');
   std::replace(description.begin(), description.end(), '\n', ' ');
   if (description.empty()) {
      std::printf("%s\n", word.c_str());
   } else {
      std::printf("%s\t%s\n", word.c_str(), description.c_str());
   }
}

//a field name as the command line takes it, without spaces
std::string field_word(const char* name) {
   std::string word;
   for (char chr : edid_field_display_name(name)) {
      word += (chr == ' ') ? '-' : static_cast<char>(((chr >= 'A') && (chr <= 'Z'))
                                                     ? chr - 'A' + 'a' : chr);
   }
   return word;
}

void complete_groups(EDID_cl& EDID) {
   std::vector<listed_group> list = all_groups(EDID);
   for (const listed_group& entry : list) {
      char offset[16];
      snprintf(offset, sizeof(offset), "0x%03X", entry.group->getAbsOffs());
      candidate(offset, std::string(entry.group->CodeName.c_str()) + " " +
                        group_name(EDID, entry.group));
   }
   std::vector<std::string> codes;
   for (const listed_group& entry : list) {
      const std::string code = entry.group->CodeName.c_str();
      if (std::find(codes.begin(), codes.end(), code) != codes.end()) continue;
      codes.push_back(code);
      size_t nth = 0;
      size_t count = std::count_if(list.begin(), list.end(), [&](const listed_group& other) {
         return code == other.group->CodeName.c_str();
      });
      for (const listed_group& other : list) {
         if (code != other.group->CodeName.c_str()) continue;
         nth++;
         candidate((count == 1) ? code : code + ":" + std::to_string(nth),
                   group_name(EDID, other.group));
      }
   }
}

void complete_fields(EDID_cl& EDID, edi_grp_cl* group, const char* suffix) {
   std::vector<std::string> words;
   for (u32_t idx=0; idx<group->FieldsAr.GetCount(); idx++) {
      const char* name = group->FieldsAr.Item(idx)->field.name;
      words.push_back(field_word((name != NULL) ? name : ""));
   }
   for (size_t idx=0; idx<words.size(); idx++) {
      if (words[idx].empty()) continue;
      std::string word = words[idx];
      if (std::count(words.begin(), words.end(), word) > 1) {
         word += ":" + std::to_string(std::count(words.begin(), words.begin() + idx + 1, word));
      }
      edi_dynfld_t* field = group->FieldsAr.Item(idx);
      std::string unit = unit_of(EDID, field->field);
      candidate(word + suffix, shown(read_field(EDID, field)) + (unit.empty() ? "" : " " + unit));
   }
   timing_state timing;
   if (read_timing(EDID, group, timing)) {
      candidate(std::string("refresh") + suffix, hz_text(timing.refresh) + " Hz");
   }
}

void complete_values(EDID_cl& EDID, edi_grp_cl* group, const std::string& field_spec,
                     const std::string& prefix) {
   if (is_refresh(group, field_spec)) return;
   edi_dynfld_t* field = find_field(group, field_spec);
   //reading a field can choose its named values, as for the signal format
   read_field(EDID, field);
   const edi_field_t& f = field->field;
   sm_vmap* vmap = field_has_selector(f) ? vmap_GetVmap(f.vmap_idx, VMAP_MID) : NULL;
   if (vmap != NULL) {
      for (auto& entry : *vmap) candidate(prefix + entry.second.name);
   } else if ((f.flags & F_BIT) != 0) {
      candidate(prefix + "on");
      candidate(prefix + "off");
   }
}

//Candidates for the last word, CURRENT, from the words before it, one to a
//line; exit status 3 asks for file names instead. Options are left to the
//shell, which removes them from the words.
int cmd_complete() {
   if (opts.args.size() < 2) usage_error("usage: edid-editor-cli complete [WORD...] CURRENT");
   std::vector<std::string> words(opts.args.begin() + 1, opts.args.end() - 1);
   const std::string& current = opts.args.back();
   if (words.empty()) {
      for (const command& entry : commands) {
         if (entry.run != cmd_complete) candidate(entry.name, entry.summary);
      }
      return 0;
   }
   const command* found = NULL;
   for (const command& entry : commands) {
      if (words[0] == entry.name) found = &entry;
   }
   if ((found == NULL) || (found->run == cmd_complete)) return 0;
   size_t count = strlen(found->arguments);
   size_t position = words.size() - 1;
   char kind = 0;
   if (position < count) {
      kind = found->arguments[position];
   } else if ((count > 0) && (found->arguments[count - 1] == 'A')) {
      kind = 'A';
   }
   if (kind == 0) return 0;
   if (kind == 'F') return 3;
   if (kind == 'M') {
      candidate("up");
      candidate("down");
      return 0;
   }

   //the rest is read from the file, whatever state it is in
   opts.quiet = true;
   opts.ignore_errors = true;
   document doc;
   open_document(doc, words[1]);
   if (kind == 'G') {
      complete_groups(doc.EDID);
   } else if (kind == 'B') {
      for (u32_t block=1; block<doc.EDID.getNumValidBlocks(); block++) {
         if (doc.EDID.BlkGroupsAr[block]->GetCount() == 0) continue;
         std::string title = block_title(doc.EDID, block);
         candidate(std::to_string(block), title.substr(title.find(": ") + 2));
      }
   } else if (kind == 'K') {
      u32_t block = 0;
      if (! parse_number(words[2], 10, block) || (block == 0) ||
          (block >= doc.EDID.getNumValidBlocks())) return 0;
      if (doc.EDID.getEDID()->blk[block][0] == 0x70) {
         candidate("displayid", "DisplayID data block");
      } else {
         candidate("audio-lpcm", "LPCM audio block");
         candidate("audio-extended", "extended audio block");
         candidate("video", "video block");
         candidate("timing", "detailed timing");
      }
   } else {
      edi_grp_cl* group = find_group(doc.EDID, words[2]).group;
      size_t equals = current.find('=');
      if (kind == 'N') {
         complete_fields(doc.EDID, group, "");
      } else if (equals == std::string::npos) {
         complete_fields(doc.EDID, group, "=");
      } else {
         complete_values(doc.EDID, group, current.substr(0, equals),
                         current.substr(0, equals + 1));
      }
   }
   return 0;
}

} //namespace

int main(int argc, char* argv[]) {
   for (int idx=1; idx<argc; idx++) {
      std::string arg = argv[idx];
      if (! opts.args.empty() && (opts.args[0] == "complete")) {
         //the words to complete may look like options
         opts.args.push_back(arg);
      } else if ((arg == "-h") || (arg == "--help")) {
         std::fputs(usage_text, stdout);
         return 0;
      } else if (arg == "--version") {
         std::printf("edid-editor-cli %s\n", EDID_EDITOR_VERSION);
         return 0;
      } else if ((arg == "-o") || (arg == "--output")) {
         if (++idx >= argc) usage_error(arg + " needs a file");
         opts.output = argv[idx];
      } else if (0 == arg.compare(0, 9, "--output=")) {
         opts.output = arg.substr(9);
      } else if ((arg == "-i") || (arg == "--in-place")) {
         opts.in_place = true;
      } else if (arg == "--ignore-errors") {
         opts.ignore_errors = true;
      } else if (arg == "--edit-read-only") {
         opts.edit_read_only = true;
      } else if ((arg == "-q") || (arg == "--quiet")) {
         opts.quiet = true;
      } else if (arg == "--json") {
         opts.json = true;
      } else if ((arg.size() > 1) && (arg[0] == '-') && (arg != "-")) {
         usage_error("unknown option " + arg);
      } else {
         opts.args.push_back(arg);
      }
   }
   if (opts.args.empty()) {
      std::fputs(usage_text, stderr);
      return 2;
   }

   for (const command& entry : commands) {
      if (opts.args[0] != entry.name) continue;
      std::string name = " " + opts.args[0] + " ";
      if (opts.json && (NULL == strstr(" info groups fields diff displays ", name.c_str()))) {
         usage_error("--json works with info, groups, fields, diff and displays");
      }
      return entry.run();
   }
   usage_error("unknown command " + opts.args[0]);
}
