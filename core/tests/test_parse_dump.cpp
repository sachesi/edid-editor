/***************************************************************
 * Name:      test_parse_dump.cpp
 * Purpose:   headless smoke test: parse EDID binaries with the
 *            ported core, dump group tree + field values.
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

static void log_sink(const char* msg, void* user_data) {
   FILE* out = (FILE*) user_data;
   if (out != NULL) fprintf(out, "LOG: %s\n", msg);
}

//print field values for one group
static void dump_fields(FILE* out, edi_grp_cl* pgrp, EDID_cl& EDID) {
   wxc_String sval;
   u32_t      ival = 0;
   u32_t      cnt  = pgrp->FieldsAr.GetCount();

   for (u32_t idx=0; idx<cnt; idx++) {
      edi_dynfld_t* pfld = pgrp->FieldsAr.Item(idx);

      //OP_READ via the field's own handler; sval must be reset per field
      sval.Empty();
      ival = 0;
      rcode retU = ( EDID.*pfld->field.handlerfn )(OP_READ, sval, ival, pfld);

      const char* desc = (pfld->field.desc != NULL) ? pfld->field.desc : pfld->field.name;
      fprintf(out, "  [%02u] %-28s = %s%s\n", idx, desc, sval.c_str(),
              RCD_IS_OK(retU) ? "" : "  <ERR>");
   }
}

static void dump_group(FILE* out, edi_grp_cl* pgrp, EDID_cl& EDID, int depth) {
   wxc_String gname;
   pgrp->getGrpName(EDID, gname);

   for (int i=0; i<depth; i++) fprintf(out, "  ");
   fprintf(out, "%s (offs %u, size %u, type 0x%02X)\n",
           gname.c_str(), pgrp->getAbsOffs(), pgrp->getDataSize(),
           pgrp->getTypeID().base_id);

   dump_fields(out, pgrp, EDID);

   //recurse into sub-groups
   u32_t subg_cnt = pgrp->getSubGrpCount();
   for (u32_t idx=0; idx<subg_cnt; idx++) {
      edi_grp_cl* psubg = pgrp->getSubGroup(idx);
      if (psubg != NULL) dump_group(out, psubg, EDID, depth+1);
   }
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

   EDID.SetGuiLogPtr(&GLog);
   GLog.SetSink(log_sink, stdout);

   edi_buf_t* pbuf = EDID.getEDID();
   memset(pbuf, 0, sizeof(edi_buf_t));

   size_t rd = fread(pbuf, 1, sizeof(edi_buf_t), in);
   fclose(in);

   fprintf(stdout, "== file %s: %zu bytes ==\n", argv[1], rd);

   if (! EDID.VerifyChksum(EDI_BASE_IDX)) {
      fprintf(stdout, "NOTE: base block checksum mismatch\n");
   }

   rcode retU;
   u32_t n_extblk = 0;

   retU = EDID.ParseEDID_Base(n_extblk);
   fprintf(stdout, "ParseEDID_Base: rcode=%d, ext blocks=%u\n",
           retU.detail.rcode, n_extblk);

   for (u32_t block=1; block<=n_extblk; block++) {
      if ((block == EDI_EXT0_IDX) && (pbuf->blk[block][0] == 0x02)) {
         retU = EDID.ParseEDID_CEA();
         fprintf(stdout, "ParseEDID_CEA: rcode=%d\n", retU.detail.rcode);
      } else if (pbuf->blk[block][0] == 0x70) {
         retU = EDID.ParseEDID_DisplayID(block);
         fprintf(stdout, "ParseEDID_DisplayID(%u): rcode=%d\n",
                 block, retU.detail.rcode);
      } else {
         continue;
      }
      if (! RCD_IS_OK(retU)) {
         char msg[1024];
         wxedid_RCD_GET_MSG(retU, msg, sizeof(msg));
         fprintf(stdout, "  msg: %s\n", msg);
      }
   }

   //dump base block groups
   u32_t cnt = EDID.EDI_BaseGrpAr.GetCount();
   for (u32_t idx=0; idx<cnt; idx++) {
      edi_grp_cl* pgrp = EDID.EDI_BaseGrpAr.Item(idx);
      dump_group(stdout, pgrp, EDID, 0);
   }

   //dump parsed extension groups
   for (u32_t block=1; block<=n_extblk; block++) {
      cnt = EDID.BlkGroupsAr[block]->GetCount();
      for (u32_t idx=0; idx<cnt; idx++) {
         edi_grp_cl* pgrp = EDID.BlkGroupsAr[block]->Item(idx);
         dump_group(stdout, pgrp, EDID, 0);
      }
   }

   return 0;
}
