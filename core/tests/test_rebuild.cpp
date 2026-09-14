/***************************************************************
 * Name:      test_rebuild.cpp
 * Purpose:   group rebuilds after type and layout field changes
 * License:   GPLv3+
 **************************************************************/

#include <cstdio>
#include <cstring>

#include "wxcompat.h"
#include "wxedid_rcd_scope.h"
#include "guilog.h"
#include "vmap.h"
#include "EDID_class.h"

static int failures = 0;

static void check(bool ok, const char* what) {
   std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
   if (! ok) failures++;
}

static bool load_edid(EDID_cl& EDID, const char* path) {
   FILE* input = std::fopen(path, "rb");
   if (input == NULL) return false;
   EDID.Clear();
   size_t size = std::fread(EDID.getEDID(), 1, sizeof(edi_t), input);
   std::fclose(input);
   if ((size < sizeof(ediblk_t)) || ((size % sizeof(ediblk_t)) != 0)) return false;
   u32_t extensions = 0;
   if (! RCD_IS_OK(EDID.ParseEDID_Base(extensions))) return false;
   if ((extensions > 0) && ! RCD_IS_OK(EDID.ParseEDID_CEA())) return false;
   EDID.ForceNumValidBlocks(extensions + 1);
   return true;
}

static bool reload(EDID_cl& EDID, bool ignore_errors = false) {
   EDID.b_ERR_Ignore = ignore_errors;
   if (! RCD_IS_OK(EDID.AssembleEDID())) return false;
   for (u32_t block=0; block<EDID.getNumValidBlocks(); block++) EDID.genChksum(block);
   edi_buf_t data;
   std::memcpy(&data, EDID.getEDID(), sizeof(data));
   u32_t blocks = EDID.getNumValidBlocks();
   EDID.Clear();
   std::memcpy(EDID.getEDID(), &data, blocks * sizeof(ediblk_t));
   u32_t extensions = 0;
   if (! RCD_IS_OK(EDID.ParseEDID_Base(extensions))) return false;
   if ((extensions > 0) && ! RCD_IS_OK(EDID.ParseEDID_CEA())) return false;
   EDID.ForceNumValidBlocks(extensions + 1);
   EDID.b_ERR_Ignore = false;
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

static edi_grp_cl* find_group(GroupAr_cl& groups, u32_t type) {
   for (u32_t idx=0; idx<groups.GetCount(); idx++) {
      if ((groups.Item(idx)->getTypeID().t32 & ID_PARENT_MASK) == type)
         return groups.Item(idx);
   }
   return NULL;
}

//write a value, then rebuild and swap the group as the editor does
static edi_grp_cl* edit(EDID_cl& EDID, edi_grp_cl* group, const char* name,
                        u32_t value, bool* replaced) {
   *replaced = false;
   edi_dynfld_t* field = find_field(group, name);
   if (field == NULL) return NULL;
   wxc_String text;
   rcode written = (EDID.*field->field.handlerfn)(OP_WRINT, text, value, field);
   if (! RCD_IS_OK(written)) return NULL;
   edi_grp_cl* target = NULL;
   rcode result;
   edi_grp_cl* rebuilt = EDID.RebuildGroup(group, field, RCD_IS_TRUE(written),
                                           &target, result);
   if (rebuilt == NULL) return group;
   if (! EDID.ReplaceGroup(target, rebuilt)) {
      delete rebuilt;
      return group;
   }
   *replaced = true;
   delete target;
   return rebuilt;
}

int main(int argc, char* argv[]) {
   if (argc != 2) return 2;
   EDID_cl EDID;
   guilog_cl log;
   log.SetSink([](const char*, void*) {}, NULL);
   EDID.SetGuiLogPtr(&log);
   check(load_edid(EDID, argv[1]), "load CTA-861 fixture");
   GroupAr_cl& cta = *EDID.BlkGroupsAr[EDI_EXT0_IDX];
   bool replaced = false;

   //Tag Code: a video block becomes an audio block
   edi_grp_cl* video = find_group(cta, ID_VDB);
   u32_t video_index = (video != NULL) ? video->getParentArIdx() : 0;
   edi_grp_cl* audio = (video != NULL) ? edit(EDID, video, "Tag Code", DBC_T_ADB, &replaced) : NULL;
   check(replaced && (audio != NULL) &&
         ((audio->getTypeID().t32 & ID_PARENT_MASK) == ID_ADB) &&
         (cta.Item(video_index) == audio), "tag change rebuilds the group as its new type");
   //video codes are not valid audio descriptors: check structure only
   check(reload(EDID, true) && (cta.GetCount() > video_index) &&
         ((cta.Item(video_index)->getTypeID().t32 & ID_PARENT_MASK) == ID_ADB),
         "retyped group assembles and parses as its new type");

   //Blk length: shortening a vendor block drops its trailing fields
   check(load_edid(EDID, argv[1]), "reload CTA-861 fixture");
   edi_grp_cl* vendor = find_group(cta, ID_VSD);
   u32_t vendor_index = (vendor != NULL) ? vendor->getParentArIdx() : 0;
   u32_t vendor_size = (vendor != NULL) ? vendor->getTotalSize() : 0;
   vendor = (vendor != NULL) ? edit(EDID, vendor, "Blk length", 5, &replaced) : NULL;
   check(replaced && (vendor != NULL) && (vendor->getTotalSize() == 6) &&
         (vendor_size == 8), "length change rebuilds the group at its new size");
   check(reload(EDID) && (cta.GetCount() > vendor_index) &&
         (cta.Item(vendor_index)->getTotalSize() == 6),
         "shortened block assembles and parses at its new size");

   //Blk length: a block that no longer fits is left unchanged
   check(load_edid(EDID, argv[1]), "reload CTA-861 fixture");
   vendor = find_group(cta, ID_VSD);
   vendor_index = vendor->getParentArIdx();
   while (cta.getFreeSpace() >= 24) {
      rcode result;
      edi_grp_cl* copy = vendor->Clone(result, T_MODE_EDIT);
      cta.InsertDn(vendor_index, copy);
   }
   vendor = edit(EDID, vendor, "Blk length", 30, &replaced);
   check(! replaced && (cta.getFreeSpace() >= 0),
         "length change that does not fit keeps the block");

   //F_FR: switching the input type regenerates the video input fields
   check(load_edid(EDID, argv[1]), "reload CTA-861 fixture");
   edi_grp_cl* input = find_group(EDID.EDI_BaseGrpAr, ID_VID);
   bool digital = (input != NULL) && (find_field(input, "Color Depth") != NULL);
   input = (input != NULL) ? edit(EDID, input, "Input Type", 0, &replaced) : NULL;
   check(digital && replaced && (input != NULL) &&
         (find_field(input, "Color Depth") == NULL) &&
         (find_field(input, "vsync") != NULL),
         "input type change regenerates the field layout");

   //F_FR without a layout change: the timing stays the same group
   edi_grp_cl* timing = find_group(EDID.EDI_BaseGrpAr, ID_DTD);
   edi_grp_cl* same = (timing != NULL) ? edit(EDID, timing, "Pixel clock", 2600, &replaced) : NULL;
   check((timing != NULL) && ! replaced && (same == timing),
         "value change without a layout change keeps the group");

   //descriptor type: a monitor name becomes unspecified text
   edi_grp_cl* name = find_group(EDID.EDI_BaseGrpAr, ID_MND);
   name = (name != NULL) ? edit(EDID, name, "desc_type", 0xFE, &replaced) : NULL;
   check(replaced && (name != NULL) && (name->CodeName == wxc_String("UTX")),
         "descriptor type change rebuilds the descriptor");
   check(reload(EDID) && (find_group(EDID.EDI_BaseGrpAr, ID_UTX) != NULL),
         "retyped descriptor assembles and parses as its new type");

   std::printf("---\n%s\n", failures == 0 ? "ALL OK" : "FAILURES PRESENT");
   return failures == 0 ? 0 : 1;
}
