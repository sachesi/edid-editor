/***************************************************************
 * Name:      test_corpus.cpp
 * Purpose:   parse, assemble, checksum, and reparse EDID corpus
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

static bool parse_document(EDID_cl& EDID, u32_t& extensions, bool ignore_errors) {
   EDID.b_ERR_Ignore = ignore_errors;
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

//Files before "--ignore-errors" must parse strictly; files after it are
//non-conformant and must be rejected strictly but parse with errors ignored.
int main(int argc, char* argv[]) {
   if (argc < 2) return 2;
   int failures = 0;
   bool ignore_errors = false;

   for (int arg=1; arg<argc; arg++) {
      if (0 == std::strcmp(argv[arg], "--ignore-errors")) {
         ignore_errors = true;
         continue;
      }
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
      size_t size = 0;
      size_t length = std::strlen(argv[arg]);
      if ((length > 4) && (0 == std::strcmp(argv[arg] + length - 4, ".hex"))) {
         std::string text;
         char chunk[4096];
         size_t count;
         while ((count = std::fread(chunk, 1, sizeof(chunk), input)) > 0) {
            text.append(chunk, count);
         }
         std::vector<u8_t> bytes;
         std::string error;
         if (edid_hex_decode(text.data(), text.size(), bytes, error) &&
             (bytes.size() <= sizeof(edi_t))) {
            size = bytes.size();
            std::memcpy(buffer, bytes.data(), size);
         } else {
            std::printf("FAIL: decode %s: %s\n", argv[arg], error.c_str());
         }
      } else {
         size = std::fread(buffer, 1, sizeof(edi_t), input);
      }
      std::fclose(input);
      edi_buf_t input_data = {};
      std::memcpy(&input_data, buffer, size);

      u32_t extensions = 0;
      bool valid_size = (size >= sizeof(ediblk_t)) &&
                        ((size % sizeof(ediblk_t)) == 0);
      bool strict = valid_size && parse_document(EDID, extensions, false) &&
                    (size == (extensions + 1) * sizeof(ediblk_t));
      if (ignore_errors) {
         if (strict) {
            std::printf("FAIL: %s parses without ignoring errors\n", argv[arg]);
            failures++;
            continue;
         }
         EDID.Clear();
         std::memcpy(buffer, &input_data, size);
      }
      bool parsed = ignore_errors
         ? valid_size && parse_document(EDID, extensions, true) &&
           (size == (extensions + 1) * sizeof(ediblk_t))
         : strict;
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
      bool identical = assembled && (0 == std::memcmp(&assembled_data, &input_data, size));
      if (assembled && ! ignore_errors && ! identical) {
         for (size_t idx=0; idx<size; idx++) {
            if (assembled_data.buff[idx] != input_data.buff[idx]) {
               std::printf("FAIL: %s changes byte %zu from 0x%02X to 0x%02X\n", argv[arg],
                           idx, input_data.buff[idx], assembled_data.buff[idx]);
               break;
            }
         }
         assembled = false;
      }
      EDID.Clear();
      std::memcpy(buffer, &assembled_data, size);
      u32_t reparsed_extensions = 0;
      bool reparsed = assembled &&
                      parse_document(EDID, reparsed_extensions, ignore_errors) &&
                      (reparsed_extensions == extensions);

      std::printf("%s: %s (%zu bytes, %u extension%s%s)\n",
                  reparsed ? "PASS" : "FAIL", argv[arg], size, extensions,
                  extensions == 1 ? "" : "s", ignore_errors ? ", errors ignored" : "");
      if (! reparsed) failures++;
   }
   return failures == 0 ? 0 : 1;
}
