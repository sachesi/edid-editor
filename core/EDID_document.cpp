/***************************************************************
 * Name:      EDID_document.cpp
 * Purpose:   opening, placing groups in, and writing whole EDIDs
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include <cstdio>
#include <cstring>

#include "wxedid_rcd_scope.h"
#include "EDID_document.h"

edid_load_result edid_load(EDID_cl& EDID, const u8_t* data, size_t size,
                           const char* name, guilog_cl& log) {
   edid_load_result result = {};
   bool ignore_errors = EDID.b_ERR_Ignore;
   bool partial = (size % sizeof(ediblk_t)) != 0;
   if ((size == 0) || (size > sizeof(edi_t)) || (partial && ! ignore_errors)) {
      char msg[1400];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t open %s: EDID data must contain 1 to 4 complete "
               "128-byte blocks. Choose another EDID file.", name);
      log.DoLog(msg);
      result.rejected = true;
      result.can_retry = (size > 0) && (size <= sizeof(edi_t));
      return result;
   }

   edi_buf_t loaded = {};
   memcpy(loaded.buff, data, size);
   size_t blocks = (size + sizeof(ediblk_t) - 1) / sizeof(ediblk_t);
   if (partial) {
      char msg[160];
      snprintf(msg, sizeof(msg),
               "[i] The last block is incomplete; its missing %zu bytes are read as zero",
               (blocks * sizeof(ediblk_t)) - size);
      log.DoLog(msg);
   }

   //dumps often hold only the blocks the base block counts, even when an
   //override block declares more
   u32_t declared = EDID_cl::DeclaredBlocks(loaded.buff, blocks * sizeof(ediblk_t));
   u32_t base_declared = 1U + loaded.edi.base.num_extblk;
   bool count_adjusted = false;
   if ((declared != blocks) && (base_declared == blocks)) {
      char msg[192];
      snprintf(msg, sizeof(msg),
               "[i] The EDID Extension Override block declares %u blocks; the file "
               "holds the %zu blocks the base block declares", declared, blocks);
      log.DoLog(msg);
   } else if (declared != blocks) {
      char msg[192];
      if (! ignore_errors) {
         snprintf(msg, sizeof(msg),
                  "[E!] Couldn’t open this EDID: it declares %u blocks, but the "
                  "file contains %zu. Choose a file with a matching block count.",
                  declared, blocks);
         log.DoLog(msg);
         result.rejected = true;
         result.can_retry = true;
         return result;
      }
      snprintf(msg, sizeof(msg),
               "[i] This EDID declares %u blocks, but %zu are present; the block "
               "count now matches the data", declared, blocks);
      log.DoLog(msg);
      //the count lives in the override block when there is one
      if (declared != (1U + loaded.edi.base.num_extblk)) {
         loaded.buff[sizeof(ediblk_t) + 6] = static_cast<u8_t>(blocks - 1);
      } else {
         loaded.edi.base.num_extblk = static_cast<u8_t>(blocks - 1);
      }
      count_adjusted = true;
   }

   EDID.Clear();
   edi_buf_t* pbuf = EDID.getEDID();
   memcpy(pbuf->buff, loaded.buff, blocks * sizeof(ediblk_t));

   u32_t parsed_extblk = 0;
   rcode retU = EDID.ParseEDID_Base(parsed_extblk);
   if (! RCD_IS_OK(retU)) {
      char msg[1400];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t parse %s as base EDID data. Choose a valid EDID file.",
               name);
      log.DoLog(msg);
      log.PrintRcode(retU);
      result.can_retry = true;
      return result;
   }

   parsed_extblk = static_cast<u32_t>(blocks - 1);
   bool extension_failed = false;
   for (u32_t block=1; block<=parsed_extblk; block++) {
      u8_t tag = pbuf->blk[block][0];
      bool parsed = false;
      if (tag == 0x02) {
         retU = EDID.ParseEDID_CEA(block);
         parsed = true;
      } else if (tag == 0x70) {
         retU = EDID.ParseEDID_DisplayID(block);
         parsed = true;
      }
      if (parsed && ! RCD_IS_OK(retU)) {
         log.PrintRcode(retU);
         EDID.BlkGroupsAr[block]->Clear();
         extension_failed = true;
      }
   }
   EDID.ForceNumValidBlocks(1U + parsed_extblk);

   for (u32_t block=1; block<=parsed_extblk; block++) {
      if (EDID.BlkGroupsAr[block]->GetCount() != 0) continue;
      char msg[144];
      snprintf(msg, sizeof(msg),
               "[i] Extension block %u (tag 0x%02X) preserved read-only",
               block, pbuf->blk[block][0]);
      log.DoLog(msg);
   }

   result.opened = true;
   result.extension_failed = extension_failed;
   result.partial = partial;
   result.count_adjusted = count_adjusted;
   result.can_retry = extension_failed;
   return result;
}

bool edid_insert_group(GroupAr_cl* array, edi_grp_cl* group) {
   if ((array == NULL) || (group == NULL) || (array->GetCount() == 0)) return false;
   gtid_t type = group->getTypeID();
   bool timing = (type.base_id == ID_DTD);
   u32_t last_data = 0;
   for (u32_t index=1; index<array->GetCount(); index++) {
      gtid_t candidate = array->Item(index)->getTypeID();
      if ((candidate.base_id == ID_DTD) ||
          ((candidate.t32 & ID_PARENT_MASK) == ID_DISPLAYID_PADDING)) {
         if (array->CanInsertUp(index, group)) {
            array->InsertUp(index, group);
            return true;
         }
         break;
      }
      last_data = index;
   }
   if (timing && (last_data + 1 < array->GetCount())) return false;
   if (array->CanInsertDn(last_data, group)) {
      array->InsertDn(last_data, group);
      return true;
   }
   return false;
}

bool edid_prepare_output(EDID_cl& EDID, guilog_cl& log) {
   edi_buf_t* pbuf = EDID.getEDID();
   u32_t parsed_blocks = EDID.getNumValidBlocks();
   u32_t declared_blocks = EDID_cl::DeclaredBlocks(pbuf->buff,
                                                   parsed_blocks * sizeof(ediblk_t));
   if ((declared_blocks != parsed_blocks) &&
       ((1U + pbuf->edi.base.num_extblk) != parsed_blocks)) {
      char msg[160];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t write this EDID: it declares %u blocks, but only %u "
               "were parsed. Reopen valid EDID data, then try again.",
               declared_blocks, parsed_blocks);
      log.DoLog(msg);
      return false;
   }

   rcode retU = EDID.AssembleEDID();
   if (! RCD_IS_OK(retU)) {
      char detail[1024];
      char msg[1400];
      wxedid_RCD_GET_MSG(retU, detail, sizeof(detail));
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t write this EDID: %s. Fix invalid data, then try again.",
               detail);
      log.DoLog(msg);
      return false;
   }

   //checksums: base + all parsed extension blocks
   for (u32_t blk=0; blk < EDID.getNumValidBlocks(); blk++) {
      if (EDID.BlkGroupsAr[blk]->GetCount() != 0) EDID.genChksum(blk);
   }
   return true;
}
