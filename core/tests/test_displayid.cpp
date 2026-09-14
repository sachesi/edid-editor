/***************************************************************
 * Name:      test_displayid.cpp
 * Purpose:   DisplayID parse, edit, reassembly, and validation
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include <stdio.h>
#include <string.h>

#include "wxcompat.h"
#include "wxedid_rcd_scope.h"
#include "guilog.h"
#include "vmap.h"
#include "EDID_class.h"

static int failures = 0;

static void check(bool ok, const char* what) {
   printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
   if (! ok) failures++;
}

static edi_dynfld_t* find_field(edi_grp_cl* group, const char* name) {
   for (u32_t idx=0; idx<group->FieldsAr.GetCount(); idx++) {
      edi_dynfld_t* field = group->FieldsAr.Item(idx);
      if ((field->field.name != NULL) &&
          (strcmp(field->field.name, name) == 0)) return field;
   }
   return NULL;
}

static edi_grp_cl* find_data_block(GroupAr_cl& groups, u8_t tag) {
   for (u32_t idx=0; idx<groups.GetCount(); idx++) {
      edi_grp_cl* group = groups.Item(idx);
      if (((group->getTypeID().t32 & ID_PARENT_MASK) == ID_DISPLAYID_DB) &&
          (group->getInstPtr()[0] == tag)) return group;
   }
   return NULL;
}

static edi_grp_cl* find_group(GroupAr_cl& groups, u32_t type_id) {
   for (u32_t idx=0; idx<groups.GetCount(); idx++) {
      edi_grp_cl* group = groups.Item(idx);
      if ((group->getTypeID().t32 & ID_PARENT_MASK) == type_id) return group;
   }
   return NULL;
}

static bool displayid_checksum_ok(const u8_t* block) {
   if ((block[0] != 0x70) || (block[2] > 121)) return false;
   u32_t sum = 0;
   for (u32_t idx=1; idx<=5U + block[2]; idx++) sum += block[idx];
   return (sum & 0xff) == 0;
}

static void refresh_displayid_checksum(u8_t* block) {
   u32_t checksum_offset = 5 + block[2];
   u32_t sum = 0;
   for (u32_t idx=1; idx<checksum_offset; idx++) sum += block[idx];
   block[checksum_offset] = (u8_t) (0x100 - (sum & 0xff));
}

int main(int argc, char* argv[]) {
   if ((argc < 2) || (argc > 3)) {
      fprintf(stderr, "usage: %s <edid.bin> [--require-raw]\n", argv[0]);
      return 2;
   }
   bool require_raw = (argc == 3) && (strcmp(argv[2], "--require-raw") == 0);
   if ((argc == 3) && ! require_raw) {
      fprintf(stderr, "unknown option: %s\n", argv[2]);
      return 2;
   }

   FILE* input = fopen(argv[1], "rb");
   if (input == NULL) {
      fprintf(stderr, "cannot open %s\n", argv[1]);
      return 2;
   }

   EDID_cl EDID;
   guilog_cl log;
   EDID.SetGuiLogPtr(&log);
   edi_buf_t* buffer = EDID.getEDID();
   memset(buffer, 0, sizeof(*buffer));
   size_t bytes_read = fread(buffer, 1, sizeof(buffer->edi), input);
   fclose(input);

   u32_t extension_count = 0;
   rcode result = EDID.ParseEDID_Base(extension_count);
   check(RCD_IS_OK(result), "parse base block");
   check(bytes_read == 384, "fixture contains three EDID blocks");
   check(extension_count == 2, "fixture declares two extension blocks");

   result = EDID.ParseEDID_CEA();
   check(RCD_IS_OK(result), "parse CTA extension");
   result = EDID.ParseEDID_DisplayID(EDI_EXT1_IDX);
   check(RCD_IS_OK(result), "parse DisplayID extension");

   GroupAr_cl& groups = *EDID.BlkGroupsAr[EDI_EXT1_IDX];
   check(groups.GetCount() >= 3, "DisplayID header, data blocks, and padding found");
   edi_grp_cl* padding = find_group(groups, ID_DISPLAYID_PADDING);
   check(padding != NULL, "zero filler is represented as one padding group");
   u32_t padding_offset = (padding != NULL) ? padding->getRelOffs() : 0;

   edi_grp_cl* timing_block = find_data_block(groups, 0x03);
   check(timing_block != NULL, "Type I timing block found");
   check((timing_block != NULL) && (timing_block->getSubGrpCount() == 2),
         "two Type I timings found");

   edi_grp_cl* first_timing = (timing_block != NULL) ?
                              timing_block->getSubGroup(0) : NULL;
   edi_dynfld_t* horizontal = (first_timing != NULL) ?
                              find_field(first_timing, "Horizontal active") : NULL;
   edi_dynfld_t* pixel_clock = (first_timing != NULL) ?
                               find_field(first_timing, "Pixel clock") : NULL;
   check(horizontal != NULL, "horizontal active field found");
   check(pixel_clock != NULL, "pixel clock field found");

   wxc_String value;
   u32_t integer = 0;
   if (horizontal != NULL) {
      result = (EDID.*horizontal->field.handlerfn)(OP_READ, value, integer, horizontal);
      check(RCD_IS_OK(result) && (integer == 2560),
            "horizontal active decodes as 2560 pixels");
      integer = 1920;
      result = (EDID.*horizontal->field.handlerfn)(OP_WRINT, value, integer, horizontal);
      check(RCD_IS_OK(result), "horizontal active accepts an edit");
   }
   if (pixel_clock != NULL) {
      value.Empty();
      integer = 0;
      result = (EDID.*pixel_clock->field.handlerfn)(OP_READ, value, integer, pixel_clock);
      check(RCD_IS_OK(result) && (value == "699.50"),
            "pixel clock decodes as 699.50 MHz");
   }

   edi_grp_cl* unknown_block = find_data_block(groups, 0x30);
   edi_grp_cl* raw_payload = (unknown_block != NULL) ?
                             unknown_block->getSubGroup(0) : NULL;
   edi_dynfld_t* raw_byte = (raw_payload != NULL) ?
                            find_field(raw_payload, "Payload byte 0") : NULL;
   if (require_raw) {
      check(raw_byte != NULL, "unknown block payload is exposed for editing");
   }
   if (raw_byte != NULL) {
      integer = 0x5a;
      result = (EDID.*raw_byte->field.handlerfn)(OP_WRINT, value, integer, raw_byte);
      check(RCD_IS_OK(result), "unknown payload byte accepts an edit");
   }

   u8_t original[sizeof(ediblk_t)];
   memcpy(original, buffer->blk[EDI_EXT1_IDX], sizeof(original));
   result = EDID.AssembleEDID();
   check(RCD_IS_OK(result), "assemble edited EDID");
   EDID.genChksum(EDI_EXT1_IDX);

   const u8_t* displayid = buffer->blk[EDI_EXT1_IDX];
   check(displayid[12] == 0x7f && displayid[13] == 0x07,
         "1920-pixel active width is encoded at the expected bytes");
   if (raw_byte != NULL) {
      check(displayid[51] == 0x5a,
            "unknown payload edit is encoded at the expected byte");
   }
   check(displayid_checksum_ok(displayid), "DisplayID structure checksum is valid");
   check(EDID.VerifyChksum(EDI_EXT1_IDX), "EDID extension checksum is valid");

   u32_t structure_checksum_offset = 5 + displayid[2];
   bool only_expected_bytes_changed = true;
   for (u32_t idx=0; idx<sizeof(original); idx++) {
      if ((idx == 12) || (idx == 13) ||
          ((raw_byte != NULL) && (idx == 51)) ||
          (idx == structure_checksum_offset) || (idx == 127)) continue;
      if (original[idx] != displayid[idx]) only_expected_bytes_changed = false;
   }
   check(only_expected_bytes_changed, "reassembly changes only edited data and checksums");

   u8_t valid_extension[sizeof(ediblk_t)];
   memcpy(valid_extension, displayid, sizeof(valid_extension));

   EDID.BlkGroupsAr[EDI_EXT1_IDX]->Clear();
   result = EDID.ParseEDID_DisplayID(EDI_EXT1_IDX);
   check(RCD_IS_OK(result), "reparse assembled DisplayID extension");
   timing_block = find_data_block(*EDID.BlkGroupsAr[EDI_EXT1_IDX], 0x03);
   first_timing = (timing_block != NULL) ? timing_block->getSubGroup(0) : NULL;
   horizontal = (first_timing != NULL) ?
                find_field(first_timing, "Horizontal active") : NULL;
   if (horizontal != NULL) {
      value.Empty();
      integer = 0;
      result = (EDID.*horizontal->field.handlerfn)(OP_READ, value, integer, horizontal);
   }
   check((horizontal != NULL) && RCD_IS_OK(result) && (integer == 1920),
         "edited width survives reparse");

   memcpy(buffer->blk[EDI_EXT1_IDX], valid_extension, sizeof(valid_extension));
   buffer->blk[EDI_EXT1_IDX][2] = 122;
   result = EDID.ParseEDID_DisplayID(EDI_EXT1_IDX);
   check(! RCD_IS_OK(result) &&
         (EDID.BlkGroupsAr[EDI_EXT1_IDX]->GetCount() == 0),
         "oversized payload is rejected without partial groups");

   memcpy(buffer->blk[EDI_EXT1_IDX], valid_extension, sizeof(valid_extension));
   buffer->blk[EDI_EXT1_IDX][10] ^= 1;
   result = EDID.ParseEDID_DisplayID(EDI_EXT1_IDX);
   check(! RCD_IS_OK(result) &&
         (EDID.BlkGroupsAr[EDI_EXT1_IDX]->GetCount() == 0),
         "bad DisplayID checksum is rejected without partial groups");

   memcpy(buffer->blk[EDI_EXT1_IDX], valid_extension, sizeof(valid_extension));
   buffer->blk[EDI_EXT1_IDX][7] = 121;
   refresh_displayid_checksum(buffer->blk[EDI_EXT1_IDX]);
   result = EDID.ParseEDID_DisplayID(EDI_EXT1_IDX);
   check(! RCD_IS_OK(result) &&
         (EDID.BlkGroupsAr[EDI_EXT1_IDX]->GetCount() == 0),
         "data block beyond payload is rejected without partial groups");

   memcpy(buffer->blk[EDI_EXT1_IDX], valid_extension, sizeof(valid_extension));
   if ((padding_offset > 0) && (padding_offset + 4 < 126)) {
      buffer->blk[EDI_EXT1_IDX][padding_offset + 4] = 1;
      refresh_displayid_checksum(buffer->blk[EDI_EXT1_IDX]);
      result = EDID.ParseEDID_DisplayID(EDI_EXT1_IDX);
      check(! RCD_IS_OK(result) &&
            (EDID.BlkGroupsAr[EDI_EXT1_IDX]->GetCount() == 0),
            "non-zero filler is rejected without partial groups");
   } else {
      check(false, "input has enough padding for malformed-filler test");
   }

   printf("---\n%s\n", (failures == 0) ? "ALL OK" : "FAILURES PRESENT");
   return (failures == 0) ? 0 : 1;
}
