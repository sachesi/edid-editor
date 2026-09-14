/***************************************************************
 * Name:      test_corpus.cpp
 * Purpose:   parse, assemble, checksum, and reparse EDID corpus
 * License:   GPLv3+
 **************************************************************/

#include <cstdio>
#include <cstring>

#include "wxcompat.h"
#include "wxedid_rcd_scope.h"
#include "guilog.h"
#include "vmap.h"
#include "EDID_class.h"

static bool parse_document(EDID_cl& EDID, u32_t& extensions) {
   rcode result = EDID.ParseEDID_Base(extensions);
   if (! RCD_IS_OK(result)) return false;
   edi_buf_t* buffer = EDID.getEDID();
   for (u32_t block=1; block<=extensions; block++) {
      u8_t tag = buffer->blk[block][0];
      if ((block == EDI_EXT0_IDX) && (tag == 0x02)) {
         result = EDID.ParseEDID_CEA();
      } else if (tag == 0x70) {
         result = EDID.ParseEDID_DisplayID(block);
      } else {
         continue;
      }
      if (! RCD_IS_OK(result)) return false;
   }
   EDID.ForceNumValidBlocks(extensions + 1);
   return true;
}

int main(int argc, char* argv[]) {
   if (argc < 2) return 2;
   int failures = 0;

   for (int arg=1; arg<argc; arg++) {
      FILE* input = std::fopen(argv[arg], "rb");
      if (input == NULL) {
         std::printf("FAIL: open %s\n", argv[arg]);
         failures++;
         continue;
      }

      EDID_cl EDID;
      guilog_cl log;
      EDID.SetGuiLogPtr(&log);
      edi_buf_t* buffer = EDID.getEDID();
      std::memset(buffer, 0, sizeof(*buffer));
      size_t size = std::fread(buffer, 1, sizeof(edi_t), input);
      std::fclose(input);

      u32_t extensions = 0;
      bool valid_size = (size >= sizeof(ediblk_t)) &&
                        ((size % sizeof(ediblk_t)) == 0);
      bool parsed = valid_size && parse_document(EDID, extensions) &&
                    (size == (extensions + 1) * sizeof(ediblk_t));
      rcode result;
      if (parsed) result = EDID.AssembleEDID();
      bool assembled = parsed && RCD_IS_OK(result);
      if (assembled) {
         for (u32_t block=0; block<=extensions; block++) {
            if (EDID.BlkGroupsAr[block]->GetCount() != 0) EDID.genChksum(block);
            if (! EDID.VerifyChksum(block)) assembled = false;
         }
      }

      edi_buf_t assembled_data = {};
      if (assembled) std::memcpy(&assembled_data, buffer, size);
      EDID.Clear();
      std::memcpy(buffer, &assembled_data, size);
      u32_t reparsed_extensions = 0;
      bool reparsed = assembled && parse_document(EDID, reparsed_extensions) &&
                      (reparsed_extensions == extensions);

      std::printf("%s: %s (%zu bytes, %u extension%s)\n",
                  reparsed ? "PASS" : "FAIL", argv[arg], size, extensions,
                  extensions == 1 ? "" : "s");
      if (! reparsed) failures++;
   }
   return failures == 0 ? 0 : 1;
}
