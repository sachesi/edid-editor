/***************************************************************
 * Name:      test_text.cpp
 * Purpose:   hexadecimal import/export and text report tests
 * Copyright: sachesi (C) 2026
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

static bool decode(const char* text, std::vector<u8_t>& bytes, std::string& error) {
   return edid_hex_decode(text, std::strlen(text), bytes, error);
}

static bool contains(const std::string& text, const char* part) {
   return text.find(part) != std::string::npos;
}

int main(int argc, char* argv[]) {
   if (argc != 2) return 2;

   std::vector<u8_t> bytes;
   std::string error;
   check(decode("00 ff FF 7a\n", bytes, error) && (bytes.size() == 4) &&
         (bytes[1] == 0xff) && (bytes[3] == 0x7a), "space-separated bytes decode");
   check(decode("{0x00, 0xFF,\n 0x10};", bytes, error) && (bytes.size() == 3) &&
         (bytes[2] == 0x10), "C array bytes decode");
   check(decode("\t\t00ffffffffffff00\n\t\t10ac\n", bytes, error) &&
         (bytes.size() == 10) && (bytes[8] == 0x10), "packed xrandr rows decode");
   check(decode("0000: 00 ff\n0002: 01\n", bytes, error) && (bytes.size() == 3) &&
         (bytes[2] == 0x01), "offset labels are skipped");
   check(! decode("00 ff\nzz 00\n", bytes, error) && contains(error, "line 2") &&
         contains(error, "zz"), "invalid token reports its line");
   check(! decode("00 fff\n", bytes, error), "odd digit count is rejected");
   check(! decode(" \n", bytes, error) && contains(error, "no hexadecimal"),
         "empty text is rejected");
   std::string oversized;
   for (int idx=0; idx<(int) sizeof(edi_buf_t) + 1; idx++) oversized += "00 ";
   check(! decode(oversized.c_str(), bytes, error) && contains(error, "more than"),
         "more than four blocks is rejected");

   FILE* input = std::fopen(argv[1], "rb");
   check(input != NULL, "open fixture");
   if (input == NULL) return 1;
   EDID_cl EDID;
   guilog_cl log;
   EDID.SetGuiLogPtr(&log);
   std::memset(EDID.getEDID(), 0, sizeof(edi_buf_t));
   size_t size = std::fread(EDID.getEDID(), 1, sizeof(edi_buf_t), input);
   std::fclose(input);

   std::string hex = edid_hex_encode(EDID.getEDID()->buff, size);
   check(hex.compare(0, 16, "00FFFFFFFFFFFF00") == 0, "export starts with the header");
   check(contains(hex, "\n\n"), "export separates 128-byte blocks");
   check(decode(hex.c_str(), bytes, error) && (bytes.size() == size) &&
         (0 == std::memcmp(bytes.data(), EDID.getEDID()->buff, size)),
         "exported text imports to identical bytes");

   u32_t extensions = 0;
   rcode result = EDID.ParseEDID_Base(extensions);
   bool parsed = RCD_IS_OK(result);
   for (u32_t block=1; parsed && (block<=extensions); block++) {
      u8_t tag = EDID.getEDID()->blk[block][0];
      if (tag == 0x02) result = EDID.ParseEDID_CEA();
      else if (tag == 0x70) result = EDID.ParseEDID_DisplayID(block);
      parsed = RCD_IS_OK(result);
   }
   EDID.ForceNumValidBlocks(extensions + 1);
   check(parsed, "parse fixture");

   std::string report = edid_text_report(EDID, "sample.bin", "9.9.9");
   check(contains(report, "EDID Editor 9.9.9"), "report names the version");
   check(contains(report, "Source: sample.bin"), "report names the source");
   check(contains(report, "EDID block [0]: Base EDID"), "report lists the base block");
   check(contains(report, "EDID block [1]: CTA-861 extension"),
         "report lists the CTA-861 block");
   check(contains(report, "EDID block [2]: DisplayID extension"),
         "report lists the DisplayID block");
   check(contains(report, "BED: "), "report lists base groups");
   check(contains(report, "DID-T1: "), "report lists DisplayID sub-groups");
   check(contains(report, "prod_id"), "report lists field names");
   check(contains(report, "----| Raw data |----\n\n00FFFFFFFFFFFF00"),
         "report ends with raw hex data");

   std::printf("---\n%s\n", failures == 0 ? "ALL OK" : "FAILURES PRESENT");
   return failures == 0 ? 0 : 1;
}
