/***************************************************************
 * Name:      test_structure_edit.cpp
 * Purpose:   structural group editing and safe template tests
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

static bool load_edid(EDID_cl& EDID, const char* path) {
   FILE* input = std::fopen(path, "rb");
   if (input == NULL) return false;
   std::memset(EDID.getEDID(), 0, sizeof(edi_buf_t));
   size_t size = std::fread(EDID.getEDID(), 1, sizeof(edi_t), input);
   std::fclose(input);
   if ((size < sizeof(ediblk_t)) || ((size % sizeof(ediblk_t)) != 0)) return false;
   u32_t extensions = 0;
   rcode result = EDID.ParseEDID_Base(extensions);
   if (! RCD_IS_OK(result)) return false;
   for (u32_t block=1; block<=extensions; block++) {
      u8_t tag = EDID.getEDID()->blk[block][0];
      result = (tag == 0x02) ? EDID.ParseEDID_CEA() :
               (tag == 0x70) ? EDID.ParseEDID_DisplayID(block) : result;
      if (((tag == 0x02) || (tag == 0x70)) && ! RCD_IS_OK(result)) return false;
   }
   return true;
}

static edi_grp_cl* find_group(GroupAr_cl& groups, u32_t type) {
   for (u32_t idx=0; idx<groups.GetCount(); idx++) {
      if ((groups.Item(idx)->getTypeID().t32 & ID_PARENT_MASK) == type)
         return groups.Item(idx);
   }
   return NULL;
}

int main(int argc, char* argv[]) {
   if ((argc < 2) || (argc > 4)) return 2;
   EDID_cl EDID;
   guilog_cl log;
   EDID.SetGuiLogPtr(&log);
   check(load_edid(EDID, argv[1]), "load CTA and DisplayID fixture");

   const EDID_cl::group_template templates[] = {
      EDID_cl::CEA_AUDIO_LPCM, EDID_cl::CEA_AUDIO_EXTENDED,
      EDID_cl::CEA_VIDEO, EDID_cl::CEA_TIMING, EDID_cl::DISPLAYID_DATA,
   };
   for (EDID_cl::group_template which : templates) {
      edi_grp_cl* group = NULL;
      rcode result = EDID.CreateGroup(which, 0x12, &group);
      check(RCD_IS_OK(result) && (group != NULL), "safe group template constructs");
      if ((which == EDID_cl::CEA_AUDIO_LPCM) ||
          (which == EDID_cl::CEA_AUDIO_EXTENDED)) {
         check((group != NULL) && (group->getSubGrpCount() == 1),
               "audio template contains one valid SAD");
      }
      delete group;
   }

   GroupAr_cl& cta = *EDID.BlkGroupsAr[EDI_EXT0_IDX];
   edi_grp_cl* source = find_group(cta, ID_VDB);
   rcode result;
   edi_grp_cl* copy = (source != NULL) ? source->Clone(result, T_MODE_EDIT) : NULL;
   check((copy != NULL) && RCD_IS_OK(result), "CTA data block duplicates safely");
   u32_t original_cta_count = cta.GetCount();
   u32_t source_index = (source != NULL) ? source->getParentArIdx() : 0;
   check((copy != NULL) && cta.CanInsertDn(source_index, copy),
         "CTA duplicate fits after its source");
   if ((copy != NULL) && cta.CanInsertDn(source_index, copy)) {
      cta.InsertDn(source_index, copy);
      check(cta.GetCount() == original_cta_count + 1, "CTA duplicate inserts");
      u32_t copy_index = copy->getParentArIdx();
      edi_grp_cl* removed = cta.Cut(copy_index);
      check((removed == copy) && (cta.GetCount() == original_cta_count),
            "CTA duplicate removes without corrupting ownership");
      delete removed;
   } else {
      delete copy;
   }

   GroupAr_cl& displayid = *EDID.BlkGroupsAr[EDI_EXT1_IDX];
   edi_grp_cl* data = find_group(displayid, ID_DISPLAYID_DB);
   copy = (data != NULL) ? data->Clone(result, T_MODE_EDIT) : NULL;
   check((copy != NULL) && RCD_IS_OK(result), "DisplayID data block duplicates safely");
   u32_t payload_before = displayid.Item(0)->getInstPtr()[2];
   u32_t padding_before = displayid.Item(displayid.GetCount() - 1)->getTotalSize();
   if ((copy != NULL) && displayid.CanInsertDn(data->getParentArIdx(), copy)) {
      displayid.InsertDn(data->getParentArIdx(), copy);
      check(displayid.Item(0)->getInstPtr()[2] == payload_before,
            "DisplayID insertion preserves payload boundary");
      edi_grp_cl* padding = displayid.Item(displayid.GetCount() - 1);
      check(padding->getTotalSize() + copy->getTotalSize() == padding_before,
            "DisplayID insertion consumes zero padding");
      edi_grp_cl* removed = displayid.Cut(copy->getParentArIdx());
      check((removed == copy) &&
            (displayid.Item(displayid.GetCount() - 1)->getTotalSize() == padding_before),
            "DisplayID removal restores zero padding");
      delete removed;
   } else {
      check(false, "DisplayID duplicate fits in padding");
      delete copy;
   }

   result = EDID.AssembleEDID();
   check(RCD_IS_OK(result), "structurally edited document assembles");
   for (u32_t block=0; block<EDID.getNumValidBlocks(); block++) {
      EDID.genChksum(block);
      check(EDID.VerifyChksum(block), "assembled block checksum is valid");
   }

   if (argc >= 3) {
      EDID.Clear();
      check(load_edid(EDID, argv[2]), "load compact DisplayID fixture");
      GroupAr_cl& compact = *EDID.BlkGroupsAr[EDI_EXT0_IDX];
      edi_grp_cl* compact_data = find_group(compact, ID_DISPLAYID_DB);
      copy = (compact_data != NULL) ? compact_data->Clone(result, T_MODE_EDIT) : NULL;
      u32_t compact_payload = compact.Item(0)->getInstPtr()[2];
      if ((copy != NULL) && compact.CanInsertDn(compact_data->getParentArIdx(), copy)) {
         compact.InsertDn(compact_data->getParentArIdx(), copy);
         check(compact.Item(0)->getInstPtr()[2] ==
               compact_payload + copy->getTotalSize(),
               "DisplayID insertion grows an unpadded payload");
         edi_grp_cl* removed = compact.Cut(copy->getParentArIdx());
         check(compact.Item(0)->getInstPtr()[2] == compact_payload,
               "DisplayID removal restores an unpadded payload length");
         delete removed;
      } else {
         check(false, "DisplayID duplicate fits unused extension space");
         delete copy;
      }
   }

   if (argc == 4) {
      EDID.Clear();
      check(load_edid(EDID, argv[3]), "load short-padding DisplayID fixture");
      GroupAr_cl& padded = *EDID.BlkGroupsAr[EDI_EXT0_IDX];
      edi_grp_cl* padded_data = find_group(padded, ID_DISPLAYID_DB);
      edi_grp_cl* padding = padded.Item(padded.GetCount() - 1);
      bool has_padding = (padding->getTypeID().t32 & ID_PARENT_MASK) ==
                         ID_DISPLAYID_PADDING;
      check(has_padding, "short DisplayID padding is parsed");
      u32_t padded_payload = padded.Item(0)->getInstPtr()[2];
      u32_t padding_size = has_padding ? padding->getTotalSize() : 0;
      copy = (padded_data != NULL) ? padded_data->Clone(result, T_MODE_EDIT) : NULL;
      if (has_padding && (copy != NULL) && (copy->getTotalSize() > padding_size) &&
          padded.CanInsertDn(padded_data->getParentArIdx(), copy)) {
         padded.InsertDn(padded_data->getParentArIdx(), copy);
         check(padded.Item(0)->getInstPtr()[2] ==
               padded_payload - padding_size + copy->getTotalSize(),
               "DisplayID insertion consumes short padding and grows payload");
         check((padded.Item(padded.GetCount() - 1)->getTypeID().t32 &
                ID_PARENT_MASK) != ID_DISPLAYID_PADDING,
               "exhausted DisplayID padding is removed");
         result = EDID.AssembleEDID();
         check(RCD_IS_OK(result), "short-padding insertion assembles");
      } else {
         check(false, "DisplayID duplicate exceeds short padding but fits block");
         delete copy;
      }
   }

   std::printf("---\n%s\n", failures == 0 ? "ALL OK" : "FAILURES PRESENT");
   return failures == 0 ? 0 : 1;
}
