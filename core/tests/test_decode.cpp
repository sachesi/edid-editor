/***************************************************************
 * Name:      test_decode.cpp
 * Purpose:   value decoding: standard timing codes, range limits,
 *            native SVDs, labels, HDR luminance
 * Copyright: sachesi (C) 2026
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

static wxc_String text_of(EDID_cl& EDID, edi_grp_cl* group, const char* name) {
   wxc_String text;
   u32_t value = 0;
   edi_dynfld_t* field = find_field(group, name);
   if (field != NULL) (EDID.*field->field.handlerfn)(OP_READ, text, value, field);
   return text;
}

static bool write_text(EDID_cl& EDID, edi_grp_cl* group, const char* name, const char* text) {
   edi_dynfld_t* field = find_field(group, name);
   if (field == NULL) return false;
   wxc_String value = text;
   u32_t unused = 0;
   return RCD_IS_OK((EDID.*field->field.handlerfn)(OP_WRSTR, value, unused, field));
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
   //range limits of a 360 Hz panel: V max +255, H max and min +255
   static const u8_t range_limits[] = {
      0x00, 0x00, 0x00, 0xFD, 0x0E, 60, 105, 212, 212, 0x61, 0x01,
      0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
   };
   std::memcpy(&buffer->blk[0][90], range_limits, sizeof(range_limits));
   buffer->blk[1][5] = 0x90;  //VIC 16, native
   buffer->blk[1][6] = 193;   //8-bit VIC
   //HDR static metadata block before the DTD
   static const u8_t hdr_static[] = {0xE6, 0x06, 0x07, 0x01, 99, 93, 14};
   std::memmove(&buffer->blk[1][16 + sizeof(hdr_static)], &buffer->blk[1][16], 18);
   std::memcpy(&buffer->blk[1][16], hdr_static, sizeof(hdr_static));
   buffer->blk[1][2] = 16 + sizeof(hdr_static);
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

   //the signal format is a color type before EDID 1.4 and for analog input,
   //and the color encodings of a digital input from 1.4, as edid-decode says
   edi_grp_cl* features = find_group(base, "SPF", 0);
   edi_grp_cl* base_group = find_group(base, "BED", 0);
   check(label_of(EDID, features, "vsig_format") == wxc_String("Non-RGB color"),
         "EDID 1.3 signal format is a color type");
   write(EDID, base_group, "edid_rev", 4);
   check(label_of(EDID, features, "vsig_format") == wxc_String("RGB 4:4:4, YCbCr 4:2:2"),
         "EDID 1.4 digital signal format is a color encoding");
   write(EDID, input_group, "Input Type", 0);
   check(label_of(EDID, features, "vsig_format") == wxc_String("Non-RGB color"),
         "EDID 1.4 analog signal format is a color type");
   write(EDID, input_group, "Input Type", 1);
   write(EDID, base_group, "edid_rev", 3);

   //range limits above 255 Hz and kHz, values as printed by edid-decode
   edi_grp_cl* range = find_group(base, "MRL", 0);
   check((value_of(EDID, range, "min_Vfreq") == 60) &&
         (value_of(EDID, range, "max_Vfreq") == 360) &&
         (value_of(EDID, range, "min_Hfreq") == 467) &&
         (value_of(EDID, range, "max_Hfreq") == 467), "range limit offsets decode");
   check(write(EDID, range, "max_Vfreq", 200) && write(EDID, range, "min_Hfreq", 100) &&
         (value_of(EDID, range, "rate_offsets") == 0x08) &&
         (value_of(EDID, range, "max_Hfreq") == 467),
         "rates up to 255 clear their offset flags");
   check(! write(EDID, range, "min_Vfreq", 300) && write(EDID, range, "min_Hfreq", 400) &&
         ! write(EDID, range, "max_Hfreq", 250) && ! write(EDID, range, "max_Vfreq", 511) &&
         (value_of(EDID, range, "rate_offsets") == 0x0C),
         "a minimum rate above 255 needs a maximum rate above 255");
   check(RCD_IS_OK(EDID.AssembleEDID()) && (buffer->blk[0][94] == 0x0C) &&
         (buffer->blk[0][95] == 60) && (buffer->blk[0][96] == 200) &&
         (buffer->blk[0][97] == 145) && (buffer->blk[0][98] == 212),
         "range limits and offsets are written back");

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

   //HDR luminance in cd/m^2, values as printed by edid-decode
   edi_grp_cl* hdr = find_group(cta, "HDRS", 0);
   check((text_of(EDID, hdr, "max_lum") == wxc_String("426.86")) &&
         (text_of(EDID, hdr, "avg_lum") == wxc_String("374.83")) &&
         (text_of(EDID, hdr, "min_lum") == wxc_String("0.0129")),
         "HDR luminance decodes to cd/m^2");
   wxc_String unit;
   edi_dynfld_t* max_lum = find_field(hdr, "max_lum");
   if (max_lum != NULL) EDID.getValUnitName(unit, max_lum->field.flags);
   check(unit == wxc_String("cd/m²"), "luminance has a unit");
   check(write_text(EDID, hdr, "max_lum", "400") && (value_of(EDID, hdr, "max_lum") == 96) &&
         (text_of(EDID, hdr, "max_lum") == wxc_String("400.00")),
         "maximum luminance is stored as the nearest code");
   check(text_of(EDID, hdr, "min_lum") == wxc_String("0.0121"),
         "minimum luminance follows the maximum");
   check(write_text(EDID, hdr, "min_lum", "0.05") && (value_of(EDID, hdr, "min_lum") == 29),
         "minimum luminance is stored as the nearest code");
   check(! write_text(EDID, hdr, "max_lum", "20") && ! write_text(EDID, hdr, "min_lum", "5") &&
         (value_of(EDID, hdr, "max_lum") == 96) && (value_of(EDID, hdr, "min_lum") == 29),
         "luminance outside the code range is refused");
   check(RCD_IS_OK(EDID.AssembleEDID()) && (buffer->blk[1][20] == 96) &&
         (buffer->blk[1][22] == 29), "luminance codes are written back");

   std::printf("---\n%s\n", failures == 0 ? "ALL OK" : "FAILURES PRESENT");
   return failures == 0 ? 0 : 1;
}
