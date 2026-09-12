/***************************************************************
 * Name:      test_edit_roundtrip.cpp
 * Purpose:   headless edit-path test: read field, write new value
 *            (OP_WRINT/OP_WRSTR), assemble, checksum, verify.
 * License:   GPLv3+
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wxcompat.h"
#include "wxedid_rcd_scope.h"
#include "guilog.h"
#include "vmap.h"
#include "EDID_class.h"
#include "CEA_class.h"
#include "CEA_ET_class.h"

static int failures = 0;
static bool saw_dtd_log = false;
static bool saw_vdb_log = false;

static void check(bool ok, const char* what) {
   printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
   if (! ok) failures++;
}

static void log_sink(const char* msg, void* /*user_data*/) {
   if (strstr(msg, "\"DTD\"") != NULL) saw_dtd_log = true;
   if (strstr(msg, "\"VDB\"") != NULL) saw_vdb_log = true;
}

//find first group of a given base type id in a group array
static edi_grp_cl* find_group(GroupAr_cl& ar, u32_t base_id) {
   u32_t cnt = ar.GetCount();
   for (u32_t idx=0; idx<cnt; idx++) {
      edi_grp_cl* pgrp = ar.Item(idx);
      if (pgrp->getTypeID().base_id == base_id) return pgrp;
   }
   return NULL;
}

//find field by (partial) name/description match
static edi_dynfld_t* find_field(edi_grp_cl* pgrp, const char* needle) {
   u32_t cnt = pgrp->FieldsAr.GetCount();
   for (u32_t idx=0; idx<cnt; idx++) {
      edi_dynfld_t* pfld = pgrp->FieldsAr.Item(idx);
      if ((pfld->field.name != NULL) &&
          (strstr(pfld->field.name, needle) != NULL)) return pfld;
      if ((pfld->field.desc != NULL) &&
          (strstr(pfld->field.desc, needle) != NULL)) return pfld;
   }
   return NULL;
}

int main(int argc, char* argv[]) {
   if (argc < 2) {
      fprintf(stderr, "usage: %s <edid.bin>\n", argv[0]);
      return 2;
   }

   FILE* in = fopen(argv[1], "rb");
   if (in == NULL) {
      fprintf(stderr, "cannot open %s\n", argv[1]);
      return 2;
   }

   EDID_cl   EDID;
   guilog_cl GLog;
   GLog.SetSink(log_sink, NULL);
   EDID.SetGuiLogPtr(&GLog);

   edi_buf_t* pbuf = EDID.getEDID();
   memset(pbuf, 0, sizeof(edi_buf_t));
   size_t rd = fread(pbuf, 1, sizeof(pbuf->edi), in);
   fclose(in);

   rcode retU;
   u32_t n_extblk = 0;

   retU = EDID.ParseEDID_Base(n_extblk);
   check(RCD_IS_OK(retU), "ParseEDID_Base");

   for (u32_t block=1; block<=n_extblk; block++) {
      if ((block == EDI_EXT0_IDX) && (pbuf->blk[block][0] == 0x02)) {
         retU = EDID.ParseEDID_CEA();
         check(RCD_IS_OK(retU), "ParseEDID_CEA");
      } else if (pbuf->blk[block][0] == 0x70) {
         retU = EDID.ParseEDID_DisplayID(block);
         check(RCD_IS_OK(retU), "ParseEDID_DisplayID");
      }
   }
   check(rd == (1U + n_extblk) * sizeof(ediblk_t),
         "file size matches declared extension count");
   EDID.ForceNumValidBlocks(1U + n_extblk);

   u8_t displayid_extension[sizeof(ediblk_t)] = {};
   bool has_displayid = (n_extblk > 1) &&
                        (pbuf->blk[EDI_EXT1_IDX][0] == 0x70);
   if (has_displayid) {
      memcpy(displayid_extension, pbuf->blk[EDI_EXT1_IDX],
             sizeof(displayid_extension));
   }
   check(saw_dtd_log, "parser log includes DTD group code");
   check(saw_vdb_log, "parser log includes VDB group code");

   //--- 1) numeric write: max vertical image size (BDD, writable ByteVal) ---
   edi_grp_cl*  pgrp  = find_group(EDID.EDI_BaseGrpAr, ID_BDD);
   edi_dynfld_t* pfld = (pgrp != NULL) ? find_field(pgrp, "Max vertical image size") : NULL;

   if ((pgrp != NULL) && (pfld != NULL)) {
      wxc_String sval;
      u32_t      ival;

      ival = 66;
      retU = ( EDID.*pfld->field.handlerfn )(OP_WRINT, sval, ival, pfld);
      check(RCD_IS_OK(retU), "write V-size = 66 cm (OP_WRINT)");

      sval.Empty(); ival = 0;
      ( EDID.*pfld->field.handlerfn )(OP_READ, sval, ival, pfld);
      check(ival == 66, "readback V-size == 66");
   } else {
      check(false, "locate V-size field");
   }

   //invalid floating-point input must fail without changing gamma
   pgrp  = find_group(EDID.EDI_BaseGrpAr, ID_BDD);
   pfld = (pgrp != NULL) ? find_field(pgrp, "gamma") : NULL;
   if (pfld != NULL) {
      wxc_String sval("not-a-number");
      u32_t      ival = 0;

      retU = ( EDID.*pfld->field.handlerfn )(OP_WRSTR, sval, ival, pfld);
      check(! RCD_IS_OK(retU), "invalid gamma text is rejected");

      sval.Empty();
      ( EDID.*pfld->field.handlerfn )(OP_READ, sval, ival, pfld);
      check(sval == "2.20", "invalid gamma text leaves value unchanged");
   } else {
      check(false, "locate gamma field");
   }

   //--- 2) numeric write: max horizontal image size (ByteVal) ---
   pgrp  = find_group(EDID.EDI_BaseGrpAr, ID_BDD);
   if (pgrp != NULL) {
      pfld = find_field(pgrp, "Max horizontal image size, in cm");
      if (pfld != NULL) {
         wxc_String sval;
         u32_t      ival = 55;

         retU = ( EDID.*pfld->field.handlerfn )(OP_WRINT, sval, ival, pfld);
         check(RCD_IS_OK(retU), "write H-size = 55 cm (OP_WRINT)");

         sval.Empty(); ival = 0;
         ( EDID.*pfld->field.handlerfn )(OP_READ, sval, ival, pfld);
         check(ival == 55, "readback H-size == 55");
      } else {
         check(false, "locate H-size field");
      }
   }
   //--- 3) text descriptor: field present and (upstream) read-only ---
   pgrp  = find_group(EDID.EDI_BaseGrpAr, ID_MND);
   if (pgrp != NULL) {
      pfld = find_field(pgrp, "Monitor name");
      if (pfld == NULL) pfld = find_field(pgrp, "text");
      if (pfld != NULL) {
         check(true, "locate monitor name field");
         check((pfld->field.flags & F_RD) != 0,
               "monitor name field is F_RD (read-only, per upstream)");

         //a write must be rejected for a read-only field
         wxc_String sval("PORT-TEST");
         u32_t      ival = 0;
         retU = ( EDID.*pfld->field.handlerfn )(OP_WRSTR, sval, ival, pfld);
         check(! RCD_IS_OK(retU), "OP_WRSTR on read-only field is rejected");
      } else {
         check(false, "locate name/text field in text descriptor");
      }
   } else {
      printf("SKIP: no text descriptor in sample\n");
   }

   //--- 4) assemble + checksum + verify ---
   retU = EDID.AssembleEDID();
   check(RCD_IS_OK(retU), "AssembleEDID");

   u32_t nblk = EDID.getNumValidBlocks();
   for (u32_t blk=0; blk<nblk; blk++) {
      if (EDID.BlkGroupsAr[blk]->GetCount() != 0) EDID.genChksum(blk);
   }
   for (u32_t blk=0; blk<nblk; blk++) {
      char what[64];
      snprintf(what, sizeof(what), "VerifyChksum block %u", blk);
      check(EDID.VerifyChksum(blk), what);
   }

   //--- 5) spawn: data written back to EDID buffer? ---
   //BDD.max_vsize lives at base block offset 22
   u8_t* pbase = pbuf->blk[EDI_BASE_IDX];
   check(pbase[22] == 66, "SpawnInstance: V-size byte in EDID buffer == 66");
   if (has_displayid) {
      check(memcmp(displayid_extension, pbuf->blk[EDI_EXT1_IDX],
                   sizeof(displayid_extension)) == 0,
            "unedited DisplayID extension remains byte-identical");
   }

   printf("---\n%s\n", (failures == 0) ? "ALL OK" : "FAILURES PRESENT");
   return (failures == 0) ? 0 : 1;
}
