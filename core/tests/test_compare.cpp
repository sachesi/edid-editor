/***************************************************************
 * Name:      test_compare.cpp
 * Purpose:   differences between two EDIDs
 * License:   GPLv3+
 **************************************************************/

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "wxcompat.h"
#include "wxedid_rcd_scope.h"
#include "guilog.h"
#include "vmap.h"
#include "EDID_class.h"
#include "EDID_compare.h"

static int failures = 0;

static void check(bool ok, const char* what) {
   std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
   if (! ok) failures++;
}

static std::vector<u8_t> read_file(const char* path) {
   std::vector<u8_t> bytes;
   FILE* input = std::fopen(path, "rb");
   if (input == NULL) return bytes;
   u8_t chunk[512];
   size_t count;
   while ((count = std::fread(chunk, 1, sizeof(chunk), input)) > 0) {
      bytes.insert(bytes.end(), chunk, chunk + count);
   }
   std::fclose(input);
   return bytes;
}

static bool load(EDID_cl& EDID, const std::vector<u8_t>& bytes) {
   std::string problem;
   return edid_parse_bytes(EDID, bytes, problem);
}

static const edid_difference* find(const std::vector<edid_difference>& found,
                                   const char* place, const char* field) {
   for (const edid_difference& entry : found) {
      if ((entry.place.find(place) != std::string::npos) && (entry.field == field)) return &entry;
   }
   return NULL;
}

int main(int argc, char* argv[]) {
   if (argc != 4) return 2;
   guilog_cl log;
   log.SetSink([](const char*, void*) {}, NULL);
   EDID_cl left;
   EDID_cl right;
   left.SetGuiLogPtr(&log);
   right.SetGuiLogPtr(&log);
   std::vector<u8_t> timing = read_file(argv[1]);
   std::vector<u8_t> video = read_file(argv[2]);
   std::vector<u8_t> base = read_file(argv[3]);

   check(load(left, timing) && load(right, timing), "load the same EDID twice");
   check(edid_compare(left, right).empty(), "identical EDIDs have no differences");

   //a changed field is named with both values
   std::vector<u8_t> changed = timing;
   changed[54] = 0xD7; //DTD pixel clock 25.18 -> 25.19 MHz
   changed[17] = 31;   //manufacture year 2021
   check(load(right, changed), "load an EDID with two changed fields");
   std::vector<edid_difference> found = edid_compare(left, right);
   const edid_difference* clock = find(found, "Block 0 · DTD", "Pixel clock");
   const edid_difference* year = find(found, "Block 0 · BED", "prod_year");
   check((clock != NULL) && (clock->left == "25.18") && (clock->right == "25.19"),
         "a changed pixel clock is reported with both values");
   check((year != NULL) && (year->left == "2020") && (year->right == "2021"),
         "a changed manufacture year is reported");
   check(found.size() == 2, "checksums are not reported");

   //different CTA-861 groups: paired by code, the rest reported as missing
   check(load(right, video), "load an EDID with other CTA-861 groups");
   found = edid_compare(left, right);
   bool only_left = false;
   bool only_right = false;
   for (const edid_difference& entry : found) {
      if (entry.field.empty() && (entry.place.find("T7VTB") != std::string::npos))
         only_left = (entry.left == "present") && entry.right.empty();
      if (entry.field.empty() && (entry.place.find("VDB") != std::string::npos))
         only_right = entry.left.empty() && (entry.right == "present");
   }
   check(only_left && only_right, "groups found in one EDID only are reported");
   check(find(found, "Block 1 · CHD", "DTD offset") != NULL,
         "fields of paired groups are compared");

   //a missing extension block
   check(load(right, base), "load a base-only EDID");
   found = edid_compare(left, right);
   const edid_difference* block = NULL;
   for (const edid_difference& entry : found) {
      if ((entry.place == "Block 1") && entry.field.empty()) block = &entry;
   }
   check((block != NULL) && (block->left == "present") && block->right.empty(),
         "an extension block found in one EDID only is reported");

   std::string problem;
   check(! edid_parse_bytes(right, std::vector<u8_t>(), problem) && ! problem.empty(),
         "empty data is refused with a reason");

   std::printf("---\n%s\n", failures == 0 ? "ALL OK" : "FAILURES PRESENT");
   return failures == 0 ? 0 : 1;
}
