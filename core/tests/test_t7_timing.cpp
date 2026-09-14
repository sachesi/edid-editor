/***************************************************************
 * Name:      test_t7_timing.cpp
 * Purpose:   CTA Type VII timing parse, edit, and reassembly
 * License:   GPLv3+
 **************************************************************/

#include <stdio.h>
#include <string.h>

#include "wxcompat.h"
#include "guilog.h"
#include "EDID_class.h"

static int failures = 0;

static void check(bool ok, const char* what) {
   printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
   if (! ok) failures++;
}

static edi_grp_cl* find_t7(GroupAr_cl& groups) {
   for (u32_t idx=0; idx<groups.GetCount(); idx++) {
      edi_grp_cl* group = groups.Item(idx);
      if (group->CodeName == "T7VTB") return group;
   }
   return NULL;
}

static edi_dynfld_t* find_field(edi_grp_cl* group, const char* name) {
   if (group == NULL) return NULL;
   for (u32_t idx=0; idx<group->FieldsAr.GetCount(); idx++) {
      edi_dynfld_t* field = group->FieldsAr.Item(idx);
      if ((field->field.name != NULL) &&
          (strcmp(field->field.name, name) == 0)) return field;
   }
   return NULL;
}

static bool read_field(EDID_cl& EDID, edi_dynfld_t* field,
                       wxc_String& text, u32_t& value) {
   if (field == NULL) return false;
   text.Empty();
   value = 0;
   return RCD_IS_OK((EDID.*field->field.handlerfn)(OP_READ, text, value, field));
}

int main(int argc, char* argv[]) {
   if (argc != 2) {
      fprintf(stderr, "usage: %s <edid.bin>\n", argv[0]);
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
   check(bytes_read == 256, "fixture contains two EDID blocks");
   check(extension_count == 1, "fixture declares one extension block");

   result = EDID.ParseEDID_CEA();
   check(RCD_IS_OK(result), "parse CTA extension");
   edi_grp_cl* timing = find_t7(*EDID.BlkGroupsAr[EDI_EXT0_IDX]);
   check(timing != NULL, "Type VII timing block found");

   edi_dynfld_t* horizontal = find_field(timing, "H-Active pix");
   edi_dynfld_t* vertical = find_field(timing, "V-Active lin");
   edi_dynfld_t* pixel_clock = find_field(timing, "Pixel clock");
   wxc_String text;
   u32_t value = 0;
   check(read_field(EDID, horizontal, text, value) && (value == 2560),
         "horizontal active decodes as 2560 pixels");
   check(read_field(EDID, vertical, text, value) && (value == 1440),
         "vertical active decodes as 1440 lines");
   check(read_field(EDID, pixel_clock, text, value) &&
         (value == 241500) && (text == "241.500"),
         "pixel clock decodes as 241.500 MHz");

   value = 2559;
   result = (EDID.*horizontal->field.handlerfn)(OP_WRINT, text, value, horizontal);
   check(RCD_IS_OK(result), "horizontal active accepts an edit");
   result = EDID.AssembleEDID();
   check(RCD_IS_OK(result), "assemble edited EDID");
   EDID.genChksum(EDI_EXT0_IDX);
   check(buffer->blk[EDI_EXT0_IDX][11] == 0xFF &&
         buffer->blk[EDI_EXT0_IDX][12] == 0x09,
         "2559-pixel active width is encoded at the expected bytes");
   check(EDID.VerifyChksum(EDI_EXT0_IDX), "CTA extension checksum is valid");

   EDID.BlkGroupsAr[EDI_EXT0_IDX]->Clear();
   result = EDID.ParseEDID_CEA();
   check(RCD_IS_OK(result), "reparse assembled CTA extension");
   timing = find_t7(*EDID.BlkGroupsAr[EDI_EXT0_IDX]);
   horizontal = find_field(timing, "H-Active pix");
   check(read_field(EDID, horizontal, text, value) && (value == 2559),
         "edited width survives reparse");

   printf("%s\n", failures == 0 ? "ALL OK" : "FAILED");
   return failures == 0 ? 0 : 1;
}
