/***************************************************************
 * Name:      test_reload_ownership.cpp
 * Purpose:   verify owned group cleanup and repeated EDID parsing.
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
#include "CEA_class.h"
#include "CEA_ET_class.h"

static int failures = 0;
static int destroyed = 0;

static void check(bool ok, const char* what) {
   printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
   if (! ok) failures++;
}

class counted_group final : public edi_grp_cl {
   public:
      ~counted_group() override {destroyed++;};

      rcode init(const u8_t* /*inst*/, u32_t /*orflags*/,
                 edi_grp_cl* /*parent*/) override {
         rcode retU;
         retU.value = RCD_OK;
         return retU;
      };
};

int main(int argc, char* argv[]) {
   if (argc < 2) {
      fprintf(stderr, "usage: %s <edid.bin>\n", argv[0]);
      return 2;
   }

   {
      GroupAr_cl groups;
      groups.Append(new counted_group);
      groups.Append(new counted_group);
      groups.Empty();
      check(destroyed == 2, "Empty deletes owned groups");

      groups.Append(new counted_group);
   }
   check(destroyed == 3, "array destructor deletes owned groups");

   FILE* in = fopen(argv[1], "rb");
   if (in == NULL) {
      fprintf(stderr, "cannot open %s\n", argv[1]);
      return 2;
   }

   u8_t source[sizeof(edi_t)] = {};
   size_t source_size = fread(source, 1, sizeof(source), in);
   fclose(in);
   if (source_size < (2 * EDI_BLK_SIZE)) {
      fprintf(stderr, "%s does not contain a CTA extension\n", argv[1]);
      return 2;
   }

   EDID_cl EDID;
   guilog_cl GLog;
   EDID.SetGuiLogPtr(&GLog);

   u32_t base_count = 0;
   u32_t cea_count = 0;
   bool clear_was_empty = true;
   for (u32_t iteration=0; iteration<1000; iteration++) {
      EDID.Clear();
      clear_was_empty = clear_was_empty &&
                        (EDID.EDI_BaseGrpAr.GetCount() == 0) &&
                        (EDID.EDI_Ext0GrpAr.GetCount() == 0) &&
                        (EDID.EDI_Ext1GrpAr.GetCount() == 0) &&
                        (EDID.EDI_Ext2GrpAr.GetCount() == 0);
      memcpy(EDID.getEDID()->buff, source, source_size);

      u32_t extensions = 0;
      rcode retU = EDID.ParseEDID_Base(extensions);
      if (! RCD_IS_OK(retU)) {
         check(false, "repeated base parse");
         break;
      }
      if (extensions > 0) {
         retU = EDID.ParseEDID_CEA();
         if (! RCD_IS_OK(retU)) {
            check(false, "repeated CTA parse");
            break;
         }
      }

      if (iteration == 0) {
         base_count = EDID.EDI_BaseGrpAr.GetCount();
         cea_count = EDID.EDI_Ext0GrpAr.GetCount();
      } else if ((EDID.EDI_BaseGrpAr.GetCount() != base_count) ||
                 (EDID.EDI_Ext0GrpAr.GetCount() != cea_count)) {
         check(false, "stable group counts across reloads");
         break;
      }
   }

   check((base_count > 0) && (cea_count > 0),
         "1000 reloads retain stable non-empty group trees");
   check(clear_was_empty, "reload clears every parsed group tree");

   printf("---\n%s\n", (failures == 0) ? "ALL OK" : "FAILURES PRESENT");
   return (failures == 0) ? 0 : 1;
}
