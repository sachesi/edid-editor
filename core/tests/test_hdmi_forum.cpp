/***************************************************************
 * Name:      test_hdmi_forum.cpp
 * Purpose:   vendor-specific and HDMI Forum data block decoding
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
   u32_t extensions = 0;
   if (! RCD_IS_OK(EDID.ParseEDID_Base(extensions))) return false;
   if (! RCD_IS_OK(EDID.ParseEDID_CEA())) return false;
   EDID.ForceNumValidBlocks(extensions + 1);
   return true;
}

static edi_dynfld_t* find_field(edi_grp_cl* group, const char* name) {
   for (u32_t idx=0; idx<group->FieldsAr.GetCount(); idx++) {
      edi_dynfld_t* field = group->FieldsAr.Item(idx);
      if ((field->field.name != NULL) && (0 == std::strcmp(field->field.name, name)))
         return field;
   }
   return NULL;
}

static bool value_is(EDID_cl& EDID, edi_grp_cl* group, const char* name, u32_t expected) {
   edi_dynfld_t* field = (group != NULL) ? find_field(group, name) : NULL;
   if (field == NULL) return false;
   wxc_String text;
   u32_t value = 0;
   (EDID.*field->field.handlerfn)(OP_READ, text, value, field);
   return value == expected;
}

//the first vendor block whose IEEE OUI reads as text
static edi_grp_cl* vendor_block(EDID_cl& EDID, const char* oui) {
   GroupAr_cl& cta = *EDID.BlkGroupsAr[EDI_EXT0_IDX];
   for (u32_t idx=0; idx<cta.GetCount(); idx++) {
      edi_grp_cl* group = cta.Item(idx);
      if ((group->getTypeID().t32 & ID_PARENT_MASK) != ID_VSD) continue;
      edi_dynfld_t* field = find_field(group, "IEEE-OUI");
      wxc_String text;
      u32_t value = 0;
      if (field != NULL) (EDID.*field->field.handlerfn)(OP_READ, text, value, field);
      if (text == wxc_String(oui)) return group;
   }
   return NULL;
}

static edi_grp_cl* find_group(EDID_cl& EDID, u32_t type) {
   GroupAr_cl& cta = *EDID.BlkGroupsAr[EDI_EXT0_IDX];
   for (u32_t idx=0; idx<cta.GetCount(); idx++) {
      if ((cta.Item(idx)->getTypeID().t32 & ID_PARENT_MASK) == type) return cta.Item(idx);
   }
   return NULL;
}

int main(int argc, char* argv[]) {
   if (argc != 4) return 2;
   EDID_cl EDID;
   guilog_cl log;
   log.SetSink([](const char*, void*) {}, NULL);
   EDID.SetGuiLogPtr(&log);

   //expected values are those printed by edid-decode for the same data
   check(load_hex(EDID, argv[1]), "load HDMI 2.1 television");
   edi_grp_cl* forum = vendor_block(EDID, "0xC45DD8");
   check((forum != NULL) && (forum->GroupName == wxc_String("HDMI Forum Vendor Specific Data Block")),
         "HDMI Forum OUI selects the HDMI Forum layout");
   check(value_is(EDID, forum, "Version", 1) && value_is(EDID, forum, "Max_TMDS", 600) &&
         value_is(EDID, forum, "SCDC_Present", 1) && value_is(EDID, forum, "Max_FRL_Rate", 6) &&
         value_is(EDID, forum, "UHD_VIC", 1) && value_is(EDID, forum, "ALLM", 1),
         "HDMI Forum rate and feature fields decode");
   check(value_is(EDID, forum, "VRRmin", 48) && value_is(EDID, forum, "VRRmax", 144),
         "HDMI Forum variable refresh range decodes");
   check(value_is(EDID, forum, "DSC_1p2", 1) && value_is(EDID, forum, "DSC_MaxSlices", 4) &&
         value_is(EDID, forum, "DSC_Max_FRL_Rate", 3) &&
         value_is(EDID, forum, "DSC_TotalChunkKBytes", 5),
         "HDMI Forum compression fields decode");
   edi_grp_cl* hdmi = vendor_block(EDID, "0x000C03");
   check((hdmi != NULL) && (find_field(hdmi, "src phy") != NULL),
         "HDMI Licensing OUI keeps the HDMI 1.4 layout");
   check(value_is(EDID, find_group(EDID, ID_HFEEODB), "EEODB_count", 3),
         "EDID Extension Override block decodes");

   edi_dynfld_t* vrr = (forum != NULL) ? find_field(forum, "VRRmax") : NULL;
   wxc_String text;
   u32_t value = 1000;
   check((vrr != NULL) && RCD_IS_OK((EDID.*vrr->field.handlerfn)(OP_WRINT, text, value, vrr)) &&
         value_is(EDID, forum, "VRRmax", 1000) && value_is(EDID, forum, "VRRmin", 48),
         "VRRmax writes its upper bits without touching VRRmin");

   check(load_hex(EDID, argv[2]), "load second HDMI 2.1 television");
   forum = vendor_block(EDID, "0xC45DD8");
   check(value_is(EDID, forum, "LTE_340Mcsc_Scramble", 1) && value_is(EDID, forum, "ALLM", 1),
         "second HDMI Forum block decodes its own flags");
   edi_grp_cl* other = vendor_block(EDID, "0x1ABBFB");
   check((other != NULL) && (other->GroupName == wxc_String("Vendor Specific Data Block")) &&
         (find_field(other, "src phy") == NULL) && (find_field(other, "byte") != NULL),
         "other vendors show their payload as data bytes");

   //changing the OUI switches the layout once the group is rebuilt
   hdmi = vendor_block(EDID, "0x000C03");
   edi_dynfld_t* oui = (hdmi != NULL) ? find_field(hdmi, "IEEE-OUI") : NULL;
   wxc_String forum_oui("0xC45DD8");
   rcode written;
   if (oui != NULL) written = (EDID.*oui->field.handlerfn)(OP_WRSTR, forum_oui, value, oui);
   edi_grp_cl* target = NULL;
   rcode result;
   edi_grp_cl* rebuilt = ((oui != NULL) && RCD_IS_OK(written))
      ? EDID.RebuildGroup(hdmi, oui, RCD_IS_TRUE(written), &target, result) : NULL;
   check((rebuilt != NULL) && (find_field(rebuilt, "Max_TMDS") != NULL) &&
         (find_field(rebuilt, "src phy") == NULL),
         "OUI change rebuilds the block with the HDMI Forum layout");
   delete rebuilt;

   //the override block counts a second CTA-861 extension
   FILE* input = std::fopen(argv[3], "rb");
   EDID.Clear();
   size_t size = (input != NULL) ? std::fread(EDID.getEDID(), 1, sizeof(edi_t), input) : 0;
   if (input != NULL) std::fclose(input);
   u32_t blocks = EDID_cl::DeclaredBlocks(EDID.getEDID()->buff, size);
   check((blocks == 3) && (EDID.getEDID()->edi.base.num_extblk == 1),
         "override block count replaces the base block count");
   u32_t extensions = 0;
   bool parsed = RCD_IS_OK(EDID.ParseEDID_Base(extensions)) &&
                 RCD_IS_OK(EDID.ParseEDID_CEA(1)) && RCD_IS_OK(EDID.ParseEDID_CEA(2));
   GroupAr_cl& second = *EDID.BlkGroupsAr[EDI_EXT1_IDX];
   check(parsed && (EDID.getNumValidBlocks() == 3) && (second.GetCount() == 3) &&
         ((second.Item(1)->getTypeID().t32 & ID_PARENT_MASK) == ID_ADB) &&
         ((second.Item(2)->getTypeID().t32 & ID_PARENT_MASK) == ID_VSD),
         "second CTA-861 extension parses its data blocks");

   std::printf("---\n%s\n", failures == 0 ? "ALL OK" : "FAILURES PRESENT");
   return failures == 0 ? 0 : 1;
}
