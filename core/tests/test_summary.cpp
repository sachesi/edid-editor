/***************************************************************
 * Name:      test_summary.cpp
 * Purpose:   EDID overview items
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
#include "EDID_text.h"
#include "EDID_summary.h"

static int failures = 0;

static void check(bool ok, const char* what) {
   std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
   if (! ok) failures++;
}

static bool load(EDID_cl& EDID, const char* path) {
   FILE* input = std::fopen(path, "rb");
   if (input == NULL) return false;
   std::string data;
   char chunk[4096];
   size_t count;
   while ((count = std::fread(chunk, 1, sizeof(chunk), input)) > 0) data.append(chunk, count);
   std::fclose(input);
   std::vector<u8_t> bytes(data.begin(), data.end());
   size_t length = std::strlen(path);
   if ((length > 4) && (0 == std::strcmp(path + length - 4, ".hex"))) {
      std::string error;
      if (! edid_hex_decode(data.data(), data.size(), bytes, error)) return false;
   }
   EDID.Clear();
   std::memcpy(EDID.getEDID(), bytes.data(), bytes.size());
   u32_t extensions = 0;
   if (! RCD_IS_OK(EDID.ParseEDID_Base(extensions))) return false;
   for (u32_t block=1; block<=extensions; block++) {
      u8_t tag = EDID.getEDID()->blk[block][0];
      rcode result = (tag == 0x02) ? EDID.ParseEDID_CEA(block) : EDID.ParseEDID_DisplayID(block);
      if (! RCD_IS_OK(result)) return false;
   }
   EDID.ForceNumValidBlocks(extensions + 1);
   return true;
}

static std::string item(const std::vector<edid_summary_item>& items, const char* label) {
   for (const edid_summary_item& entry : items) {
      if (entry.label == label) return entry.value;
   }
   return "<missing>";
}

int main(int argc, char* argv[]) {
   if (argc != 3) return 2;
   EDID_cl EDID;
   guilog_cl log;
   log.SetSink([](const char*, void*) {}, NULL);
   EDID.SetGuiLogPtr(&log);

   check(load(EDID, argv[1]), "load base and CTA-861 fixture");
   std::vector<edid_summary_item> items = edid_summary(EDID);
   check(item(items, "Name") == "GTK-PORT", "monitor name");
   check((item(items, "Manufacturer") == "OXC") && (item(items, "Product code") == "0x1234") &&
         (item(items, "Serial number") == "42"), "manufacturer, product and serial number");
   check(item(items, "Manufactured") == "Week 1, 2020", "manufacture date");
   check(item(items, "EDID") == "1.3", "EDID version");
   check(item(items, "Input") == "Digital, DisplayPort", "input type");
   check(item(items, "Screen size") == "40 × 25 cm (18.6″)", "screen size and diagonal");
   check(item(items, "Preferred timing") == "640x480 @ 59.95Hz", "preferred timing");
   check(item(items, "Detailed timings") == "640x480 at 59.95 Hz; 2560x1440 at 59.95 Hz",
         "detailed timings from every block");
   check(item(items, "Block 1") == "CTA-861, revision 3", "extension blocks");
   check(item(items, "Vertical rate") == "<missing>", "absent data is left out");

   //edits show up without saving
   edi_grp_cl* base = EDID.BlkGroupsAr[EDI_BASE_IDX]->Item(0);
   for (u32_t idx=0; idx<base->FieldsAr.GetCount(); idx++) {
      edi_dynfld_t* field = base->FieldsAr.Item(idx);
      if (0 != std::strcmp(field->field.name, "prod_week")) continue;
      wxc_String text;
      u32_t week = 0xFF;
      (EDID.*field->field.handlerfn)(OP_WRINT, text, week, field);
   }
   items = edid_summary(EDID);
   check(item(items, "Manufactured") == "Model year 2020", "summary follows edits");

   //values as printed by edid-decode for the same panel
   check(load(EDID, argv[2]), "load DisplayID 2.0 panel");
   items = edid_summary(EDID);
   check(item(items, "FreeSync range") == "40–60 Hz", "FreeSync range");
   check(item(items, "DisplayID refresh range") == "40–60 Hz", "DisplayID refresh range");
   check(item(items, "Block 1") == "DisplayID 2.0", "DisplayID version");
   check(item(items, "Detailed timings") == "1920x1080 at 60.00, 40.00 Hz",
         "timings grouped by resolution, DisplayID included");

   std::printf("---\n%s\n", failures == 0 ? "ALL OK" : "FAILURES PRESENT");
   return failures == 0 ? 0 : 1;
}
