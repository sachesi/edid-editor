/***************************************************************
 * Name:      test_decode.cpp
 * Purpose:   value decoding: standard timing codes, native SVDs, labels
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

static bool parse(EDID_cl& EDID) {
   u32_t extensions = 0;
   if (! RCD_IS_OK(EDID.ParseEDID_Base(extensions))) return false;
   for (u32_t block=1; block<=extensions; block++) {
      if (EDID.getEDID()->blk[block][0] != 0x02) continue;
      if (! RCD_IS_OK(EDID.ParseEDID_CEA(block))) return false;
   }
   EDID.ForceNumValidBlocks(extensions + 1);
   return true;
}

//nth group with the given code name, sub-groups included
static edi_grp_cl* find_group(GroupAr_cl& groups, const char* code, u32_t nth) {
   for (u32_t idx=0; idx<groups.GetCount(); idx++) {
      edi_grp_cl* group = groups.Item(idx);
      if (group->CodeName == wxc_String(code)) {
         if (nth == 0) return group;
         nth--;
      }
      for (u32_t sub=0; sub<group->getSubGrpCount(); sub++) {
         edi_grp_cl* subgroup = group->getSubGroup(sub);
         if (subgroup->CodeName != wxc_String(code)) continue;
         if (nth == 0) return subgroup;
         nth--;
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

static wxc_String label_of(EDID_cl& EDID, edi_grp_cl* group, const char* name) {
   wxc_String label;
   edi_dynfld_t* field = find_field(group, name);
   if (field != NULL) EDID.getValDesc(label, field, value_of(EDID, group, name), VD_NAME);
   return label;
}

static wxc_String name_of(EDID_cl& EDID, edi_grp_cl* group) {
   wxc_String name;
   if (group != NULL) group->getGrpName(EDID, name);
   return name;
}

//the SVD byte as written back to the EDID buffer
static u8_t svd_byte(EDID_cl& EDID, u32_t offset) {
   if (! RCD_IS_OK(EDID.AssembleEDID())) return 0;
   return EDID.getEDID()->blk[1][offset];
}

int main(int argc, char* argv[]) {
   if (argc != 2) return 2;
   EDID_cl EDID;
   guilog_cl log;
   log.SetSink([](const char*, void*) {}, NULL);
   EDID.SetGuiLogPtr(&log);

   FILE* input = std::fopen(argv[1], "rb");
   if (input == NULL) return 2;
   std::memset(EDID.getEDID(), 0, sizeof(edi_buf_t));
   size_t size = std::fread(EDID.getEDID(), 1, sizeof(edi_t), input);
   std::fclose(input);
   check(size == 2 * sizeof(ediblk_t), "load base and CTA-861 fixture");

   edi_buf_t* buffer = EDID.getEDID();
   static const u8_t std_timings[] = {0xD1, 0xC0, 0xA9, 0xC0};
   std::memcpy(&buffer->blk[0][38], std_timings, sizeof(std_timings));
   buffer->blk[1][5] = 0x90;  //VIC 16, native
   buffer->blk[1][6] = 193;   //8-bit VIC
   EDID.genChksum(0);
   EDID.genChksum(1);
   check(parse(EDID), "parse the patched fixture");

   //standard timings resolve to VESA DMT codes without an explicit init
   GroupAr_cl& base = *EDID.BlkGroupsAr[EDI_BASE_IDX];
   edi_grp_cl* std0 = find_group(base, "STI", 0);
   edi_grp_cl* std1 = find_group(base, "STI", 1);
   check(name_of(EDID, std0) == wxc_String("1920x1080 @ 60Hz"),
         "standard timing is named by its DMT code");
   check(name_of(EDID, std1) == wxc_String("1600x900 @ 60Hz (RB)"),
         "reduced blanking DMT code is marked");
   check(label_of(EDID, std0, "DMT_2") == wxc_String("1920x1080 @ 60Hz"),
         "DMT_2 value has a label");

   //digital input labels
   edi_grp_cl* input_group = find_group(base, "VID", 0);
   check((label_of(EDID, input_group, "IF Type") == wxc_String("DisplayPort")) &&
         (label_of(EDID, input_group, "Color Depth") == wxc_String("undefined")),
         "interface type and color depth have labels");

   //native SVD: VIC without the native bit, native flag separate
   GroupAr_cl& cta = *EDID.BlkGroupsAr[EDI_EXT0_IDX];
   edi_grp_cl* native = find_group(cta, "SVD", 0);
   edi_grp_cl* wide = find_group(cta, "SVD", 1);
   check((value_of(EDID, native, "VIC") == 16) && (value_of(EDID, native, "Native") == 1) &&
         (label_of(EDID, native, "VIC") == wxc_String("1080p    16:9")) &&
         (name_of(EDID, native) == wxc_String("1920x1080p @ 59.94/60Hz [Native]")),
         "native SVD shows VIC 16 and the native flag");
   check((value_of(EDID, wide, "VIC") == 193) && (value_of(EDID, wide, "Native") == 0),
         "8-bit VIC is not native");

   check(write(EDID, native, "VIC", 4) && (svd_byte(EDID, 5) == 0x84),
         "new VIC 1-64 keeps the native bit");
   check(write(EDID, native, "VIC", 100) && (svd_byte(EDID, 5) == 100) &&
         (value_of(EDID, native, "Native") == 0),
         "VIC above 64 drops the native bit");
   check(! write(EDID, native, "VIC", 150) && (svd_byte(EDID, 5) == 100),
         "VICs 128-192 are refused");
   check(! write(EDID, native, "Native", 1) && ! write(EDID, wide, "Native", 1),
         "only VICs 1-64 can be native");
   check(write(EDID, native, "VIC", 16) && write(EDID, native, "Native", 1) &&
         (svd_byte(EDID, 5) == 0x90), "native flag sets the native bit");
   check(write(EDID, native, "Native", 0) && (svd_byte(EDID, 5) == 16),
         "native flag clears the native bit");

   std::printf("---\n%s\n", failures == 0 ? "ALL OK" : "FAILURES PRESENT");
   return failures == 0 ? 0 : 1;
}
