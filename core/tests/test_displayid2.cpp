/***************************************************************
 * Name:      test_displayid2.cpp
 * Purpose:   DisplayID 2.x Type VII timing and range limit blocks
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

static int failures = 0;

static void check(bool ok, const char* what) {
   std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
   if (! ok) failures++;
}

static bool parse(EDID_cl& EDID) {
   u32_t extensions = 0;
   if (! RCD_IS_OK(EDID.ParseEDID_Base(extensions))) return false;
   for (u32_t block=1; block<=extensions; block++) {
      if (EDID.getEDID()->blk[block][0] != 0x70) continue;
      if (! RCD_IS_OK(EDID.ParseEDID_DisplayID(block))) return false;
   }
   EDID.ForceNumValidBlocks(extensions + 1);
   return true;
}

static bool load_hex(EDID_cl& EDID, const char* path) {
   FILE* input = std::fopen(path, "rb");
   if (input == NULL) return false;
   std::string text;
   char chunk[4096];
   size_t count;
   while ((count = std::fread(chunk, 1, sizeof(chunk), input)) > 0) text.append(chunk, count);
   std::fclose(input);
   std::vector<u8_t> bytes;
   std::string error;
   if (! edid_hex_decode(text.data(), text.size(), bytes, error)) return false;
   EDID.Clear();
   std::memcpy(EDID.getEDID(), bytes.data(), bytes.size());
   return parse(EDID);
}

static edi_grp_cl* find_subgroup(EDID_cl& EDID, const char* code) {
   GroupAr_cl& groups = *EDID.BlkGroupsAr[EDI_EXT0_IDX];
   for (u32_t idx=0; idx<groups.GetCount(); idx++) {
      edi_grp_cl* group = groups.Item(idx);
      for (u32_t sub=0; sub<group->getSubGrpCount(); sub++) {
         if (group->getSubGroup(sub)->CodeName == wxc_String(code))
            return group->getSubGroup(sub);
      }
   }
   return NULL;
}

static edi_dynfld_t* find_field(edi_grp_cl* group, const char* name) {
   if (group == NULL) return NULL;
   for (u32_t idx=0; idx<group->FieldsAr.GetCount(); idx++) {
      edi_dynfld_t* field = group->FieldsAr.Item(idx);
      if ((field->field.name != NULL) && (0 == std::strcmp(field->field.name, name)))
         return field;
   }
   return NULL;
}

static u32_t value_of(EDID_cl& EDID, edi_grp_cl* group, const char* name) {
   edi_dynfld_t* field = find_field(group, name);
   if (field == NULL) return 0xFFFFFFFF;
   wxc_String text;
   u32_t value = 0;
   (EDID.*field->field.handlerfn)(OP_READ, text, value, field);
   return value;
}

static bool write(EDID_cl& EDID, edi_grp_cl* group, const char* name, u32_t value) {
   edi_dynfld_t* field = find_field(group, name);
   if (field == NULL) return false;
   wxc_String text;
   return RCD_IS_OK((EDID.*field->field.handlerfn)(OP_WRINT, text, value, field));
}

static bool reassemble(EDID_cl& EDID) {
   if (! RCD_IS_OK(EDID.AssembleEDID())) return false;
   for (u32_t block=0; block<EDID.getNumValidBlocks(); block++) EDID.genChksum(block);
   edi_buf_t data;
   std::memcpy(&data, EDID.getEDID(), sizeof(data));
   EDID.Clear();
   std::memcpy(EDID.getEDID(), &data, sizeof(data));
   return parse(EDID);
}

int main(int argc, char* argv[]) {
   if (argc != 2) return 2;
   EDID_cl EDID;
   guilog_cl log;
   log.SetSink([](const char*, void*) {}, NULL);
   EDID.SetGuiLogPtr(&log);

   //expected values are those printed by edid-decode for the same data
   check(load_hex(EDID, argv[1]), "load DisplayID 2.0 panel");
   edi_grp_cl* timing = find_subgroup(EDID, "DID-T7");
   wxc_String name;
   if (timing != NULL) timing->getGrpName(EDID, name);
   check((timing != NULL) && (name == wxc_String("1920x1080 @ 60.00 Hz")),
         "Type VII timing is named by its mode");
   check((value_of(EDID, timing, "Pixel clock") == 141120) &&
         (value_of(EDID, timing, "Horizontal active") == 1920) &&
         (value_of(EDID, timing, "Horizontal blanking") == 180) &&
         (value_of(EDID, timing, "Horizontal front porch") == 108) &&
         (value_of(EDID, timing, "Horizontal sync width") == 48) &&
         (value_of(EDID, timing, "Horizontal sync positive") == 1) &&
         (value_of(EDID, timing, "Vertical active") == 1080) &&
         (value_of(EDID, timing, "Vertical blanking") == 40) &&
         (value_of(EDID, timing, "Vertical front porch") == 10) &&
         (value_of(EDID, timing, "Vertical sync width") == 10) &&
         (value_of(EDID, timing, "Preferred") == 1),
         "Type VII timing fields decode");
   edi_grp_cl* range = find_subgroup(EDID, "DID-RANGE");
   check((value_of(EDID, range, "Minimum pixel clock") == 141120) &&
         (value_of(EDID, range, "Maximum pixel clock") == 141120) &&
         (value_of(EDID, range, "Minimum refresh") == 40) &&
         (value_of(EDID, range, "Maximum refresh") == 60) &&
         (value_of(EDID, range, "Seamless timing change") == 1),
         "timing range limits decode");

   check(write(EDID, timing, "Pixel clock", 169344) &&
         write(EDID, range, "Maximum refresh", 300) &&
         write(EDID, range, "Maximum pixel clock", 169344),
         "timing and range fields accept new values");
   check(reassemble(EDID), "edited DisplayID 2.0 data assembles and parses");
   timing = find_subgroup(EDID, "DID-T7");
   range = find_subgroup(EDID, "DID-RANGE");
   name.Empty();
   if (timing != NULL) timing->getGrpName(EDID, name);
   check((name == wxc_String("1920x1080 @ 72.00 Hz")) &&
         (value_of(EDID, range, "Maximum refresh") == 300) &&
         (value_of(EDID, range, "Seamless timing change") == 1) &&
         (value_of(EDID, range, "Maximum pixel clock") == 169344),
         "edited values survive reassembly");

   //CTA-861 data blocks inside DisplayID: the AMD FreeSync range
   check(load_hex(EDID, argv[1]), "reload DisplayID 2.0 panel");
   edi_grp_cl* amd = find_subgroup(EDID, "VSD");
   check((amd != NULL) && (amd->GroupName == wxc_String("AMD Vendor Specific Data Block")) &&
         (value_of(EDID, amd, "Version") == 3) && (value_of(EDID, amd, "Min_Refresh") == 40) &&
         (value_of(EDID, amd, "Max_Refresh") == 60),
         "AMD block inside DisplayID decodes its refresh range");
   edi_buf_t before;
   std::memcpy(&before, EDID.getEDID(), sizeof(before));
   edi_dynfld_t* version = find_field(amd, "Version");
   wxc_String text;
   u32_t two = 2;
   rcode written = (version != NULL)
      ? (EDID.*version->field.handlerfn)(OP_WRINT, text, two, version) : rcode();
   edi_grp_cl* target = NULL;
   rcode result;
   edi_grp_cl* rebuilt = RCD_IS_OK(written)
      ? EDID.RebuildGroup(amd, version, RCD_IS_TRUE(written), &target, result) : NULL;
   edi_grp_cl* wrapper = (amd != NULL) ? amd->getParentGrp() : NULL;
   check((rebuilt != NULL) && (find_field(rebuilt, "Max_Refresh_8bit") == NULL) &&
         EDID.ReplaceGroup(target, rebuilt) && (wrapper != NULL) &&
         (wrapper->getTotalSize() == 3 + 21),
         "version change rebuilds the AMD block inside DisplayID");
   delete target;
   check(RCD_IS_OK(EDID.AssembleEDID()), "rebuilt DisplayID data assembles");
   bool only_version = true;
   for (u32_t idx=128; idx<256; idx++) {
      if ((idx == 175) || (idx >= 254)) continue; //version, checksums
      if (EDID.getEDID()->buff[idx] != before.buff[idx]) only_version = false;
   }
   check(only_version && (EDID.getEDID()->buff[175] == 2),
         "only the version byte changes inside DisplayID");

   check(load_hex(EDID, argv[1]), "reload DisplayID 2.0 panel");
   amd = find_subgroup(EDID, "VSD");
   edi_dynfld_t* length = find_field(amd, "Blk length");
   u32_t shorter = 18;
   written = (length != NULL)
      ? (EDID.*length->field.handlerfn)(OP_WRINT, text, shorter, length) : rcode();
   rebuilt = RCD_IS_OK(written)
      ? EDID.RebuildGroup(amd, length, RCD_IS_TRUE(written), &target, result) : NULL;
   check((rebuilt == NULL) && ! RCD_IS_OK(result),
         "a size change inside DisplayID is refused");

   //a descriptor size above 20 keeps its extra byte
   check(load_hex(EDID, argv[1]), "reload DisplayID 2.0 panel");
   edi_buf_t original;
   std::memcpy(&original, EDID.getEDID(), sizeof(original));
   static const u8_t display_id[] = {
      0x70, 0x20, 24, 0x02, 0x00,
      0x22, 0x10, 21,                   //Type VII, 21-byte descriptors
      0x3f, 0x27, 0x02, 0x84, 0x7f, 0x07, 0xb3, 0x00, 0x6b, 0x80,
      0x2f, 0x00, 0x37, 0x04, 0x27, 0x00, 0x09, 0x00, 0x09, 0x00, 0x5A,
   };
   std::memset(original.blk[1], 0, sizeof(ediblk_t));
   std::memcpy(original.blk[1], display_id, sizeof(display_id));
   EDID.Clear();
   std::memcpy(EDID.getEDID(), &original, sizeof(original));
   EDID.genChksum(1);
   std::memcpy(&original, EDID.getEDID(), sizeof(original));
   check(parse(EDID), "parse 21-byte Type VII descriptor");
   timing = find_subgroup(EDID, "DID-T7");
   check((timing != NULL) && (timing->getTotalSize() == 21), "descriptor keeps its declared size");
   check(RCD_IS_OK(EDID.AssembleEDID()) &&
         (0 == std::memcmp(EDID.getEDID()->blk[1], original.blk[1], 127)),
         "extra descriptor byte is written back unchanged");

   std::printf("---\n%s\n", failures == 0 ? "ALL OK" : "FAILURES PRESENT");
   return failures == 0 ? 0 : 1;
}
