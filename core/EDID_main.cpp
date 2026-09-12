/***************************************************************
 * Name:      EDID_main.cpp
 * Purpose:   EDID main functions and common field handlers
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2014-2025
 * License:   GPLv3+
 **************************************************************/

#include "debug.h"
#include "rcdunits.h"
#ifndef idEDID_MAIN
   #error "EDID_base.cpp: missing unit ID"
#endif
#define RCD_UNIT idEDID_MAIN
#include "rcode/rcode.h"

#include "wxedid_rcd_scope.h"

RCD_AUTOGEN_DEFINE_UNIT

#include <stddef.h>

#include "vmap.h"
#include "EDID_class.h"
#include "CEA_class.h"
#include "CEA_ET_class.h"


const wxc_String EDID_cl::prop_flag_name[] = {
   "RD",  //F_RD
   "NU",  //F_NU
   "GD",  //F_GD
   "BN",  //F_DN
   "VS",  //F_VS
   "FR",  //F_FR
   "FR",  //F_INIT, same as Forced Refresh
// "RS",  //F_RS: internal checks: reserved field
// "NI",  //F_NI: internal checks: not an integer, not displayed
};

const wxc_String EDID_cl::val_type_name[] = {
   "Bit",    //F_BIT
   "BitF",   //F_BFD
   "Byte",   //F_BTE
   "Int",    //F_INT
   "Float",  //F_FLT
   "Hex",    //F_HEX
   "String", //F_STR
   "LE"      //F_LE
};

const wxc_String EDID_cl::val_unit_name[] = {
   "pix", //F_PIX
   "mm",  //F_MM
   "cm",  //F_CM
   "dm",  //F_DM
   "Hz",  //F_HZ
   "kHz", //F_KHZ
   "MHz", //F_MHZ
   "ms",  //F_MLS
   "%"    //F_PCT
};

void EDID_cl::getValUnitName(wxc_String& sval, const u32_t flags) {
   u32_t flg;

   flg = (flags >> F_UN_SFT) & F_UN_MSK;

   if (flg == 0) {
      sval.Empty(); //sval = "--"; -> leave it to caller
      return;
   }

   for (u32_t it=0; it<F_UN_CNT; it++) {
      if (flg & 0x1) {
         sval = val_unit_name[it];
         break;
      }
      flg >>= 1;
   }

   return;
}

void EDID_cl::getValTypeName(wxc_String& sval, const u32_t flags) {
   u32_t flg;

   flg = (flags >> F_TP_SFT) & F_TP_MSK;

   if (flg == 0) {
      sval = "--";
      return;
   }

   sval.Empty();
   for (u32_t it=0; it<F_TP_CNT; it++) {
      if (flg & 0x1) {
         if (sval.Len() > 0) sval << "|";
         sval << val_type_name[it];
      }
      flg >>= 1;
   }
   return;
}

void EDID_cl::getValFlagsAsString(wxc_String& sval, const u32_t flags) {
   u32_t flg;
   u32_t itf;

   flg = (flags >> F_PR_SFT) & F_PR_MSK;

   if (flg == 0) {
      sval = "--";
      return;
   }

   sval.Empty();
   itf = 0;

   for (; itf<F_PR_CNT; itf++) {

      if (flg & 0x1) {
         if (sval.Len() > 0) sval << "|";
         sval << prop_flag_name[itf];
      }
      flg >>= 1;
   }
   return;
}

void EDID_cl::getValDesc(wxc_String &sval, edi_dynfld_t* p_field, u32_t val, vd_mode mode) {
   const char        *cstr;
   const vmap_ent_t  *m_ent;

   vmap_type  vtype;

   if (0 == p_field->field.vmap_idx) {
      sval.Empty(); //no value description
      return;
   }

   vtype = (F_VSVM & p_field->field.flags) ? VMAP_VAL : VMAP_MID;

   m_ent = vmap_GetVmapEntry(p_field->field.vmap_idx, vtype, val);

   if (NULL == m_ent) {
      sval = "<reserved>";
      return;
   }

   if (VD_DESC == mode) {
      cstr = m_ent->desc;
      sval = wxc_String::FromAscii(cstr);
      return;
   }

   cstr = m_ent->name;
   sval = wxc_String::FromAscii(cstr);

   if (mode != VD_FULL) return;

   //include description
   cstr  = m_ent->desc;
   sval  << " ";
   sval  << wxc_String::FromAscii(cstr);
}

rcode EDID_cl::ParseDBC_TAG(u8_t *pinst, edi_grp_cl** pp_grp) {
   rcode       retU;
   ethdr_t     ethdr;
   int         tagcode;
   edi_grp_cl *pgrp = NULL;

   RCD_SET_OK(retU);

   ethdr.w16 = reinterpret_cast<u16_t*> (pinst)[0];
   tagcode   = ethdr.ehdr.hdr.tag.tag_code;

   switch (tagcode) {
      case DBC_T_ADB: //1: Audio Data Block
         pgrp = new cea_adb_cl;
         break;
      case DBC_T_VDB: //2: Video Data Block
         pgrp = new cea_vdb_cl;
         break;
      case DBC_T_VSD: //3: Vendor Specific Data Block
         pgrp = new cea_vsd_cl;
         break;
      case DBC_T_SAB: //4: Speaker Allocation Data Block
         pgrp = new cea_sab_cl;
         break;
      case DBC_T_VTC: //5: VESA Display Transfer Characteristic Data Block (gamma)
         pgrp = new cea_vdtc_cl;
         break;
      case DBC_T_EXT: //7: Extended Tag Codes
         {
            u32_t  etag;
            etag = ethdr.ehdr.extag;

            switch (etag) {
               case DBC_ET_VCDB: //0: Video Capability Data Block
                  pgrp = new cea_vcdb_cl;
                  break;
               case DBC_ET_VSVD: //1: Vendor-Specific Video Data Block
                  pgrp = new cea_vsvd_cl;
                  break;
               case DBC_ET_VDDD: //2: VESA Display Device Data Block
                  pgrp = new cea_vddd_cl;
                  break;
            // case DBC_ET_RSV3: //3: Reserved for VESA Video Data Block
            //    pgrp = NULL;
            //    break;
            // case DBC_ET_RSV4: //4: Reserved for HDMI Video Data Block
            //    pgrp = NULL;
            //    break;
               case DBC_ET_CLDB: //5: Colorimetry Data Block
                  pgrp = new cea_cldb_cl;
                  break;
               case DBC_ET_HDRS: //6: HDR Static Metadata Data Block
                  pgrp = new cea_hdrs_cl;
                  break;
               case DBC_ET_HDRD: //7: HDR Dynamic Metadata Data Block
                  pgrp = new cea_hdrd_cl;
                  break;
               case DBC_ET_VFPD: //13: Video Format Preference Data Block
                  pgrp = new cea_vfpd_cl;
                  break;
               case DBC_ET_Y42V: //14: YCBCR 4:2:0 Video Data Block
                  pgrp = new cea_y42v_cl;
                  break;
               case DBC_ET_Y42C: //15: YCBCR 4:2:0 Capability Map Data Block
                  pgrp = new cea_y42c_cl;
                  break;
            // case DBC_ET_RS16: //16: Reserved for CTA Miscellaneous Audio Fields
            //    pgrp = NULL;
            //    break;
               case DBC_ET_VSAD: //17: Vendor-Specific Audio Data Block
                  pgrp = new cea_vsad_cl;
                  break;
               case DBC_ET_HADB: //18: HDMI Audio Data Block
                  pgrp = new cea_hadb_cl;
                  break;
               case DBC_ET_RMCD: //19: Room Configuration Data Block
                  pgrp = new cea_rmcd_cl;
                  break;
               case DBC_ET_SLDB: //20: Speaker Location Data Block
                  pgrp = new cea_sldb_cl;
                  break;
               case DBC_ET_IFDB: //32  InfoFrame Data Block
                  pgrp = new cea_ifdb_cl;
                  break;
               case DBC_ET_T7VTB: //34 DisplayID Type 7 Video Timing Data Block
                  pgrp = new cea_t7vtb_cl;
                  break;
               case DBC_ET_T8VTB: //35 DisplayID Type 8 Video Timing Data Block
                  pgrp = new cea_t8vtb_cl;
                  break;
               case DBC_ET_T10VTB: //42 DisplayID Type 10 Video Timing Data Block
                  pgrp = new cea_t10vtb_cl;
                  break;
            // case DBC_ET_HEOVR: //120 HDMI Forum EDID Extension Override Data Block
            //    pgrp = NULL;
            //    break;
            // case DBC_ET_HSCDB: //121 HDMI Forum Sink Capability Data Block
            //    pgrp = NULL;
            //    break;
               default:
                  //CTA-861-H: reserved Extended Tag Codes:
                  //3, 4, 8-12, 18, 21-31, 33, 36..41, 43..119, 122..255
                  //UNK-ET: Unknown Data Block (Extended Tag Code)
                  pgrp = new cea_unket_cl;
                  wxedid_RCD_SET_FAULT_VMSG(retU,
                                            "[E!] CTA-861 DBC: unknown Extended Tag Code=%u",
                                            etag);
            }
         }
         break;
      default:
         //CTA-861-G: reserved Tag Codes: 0,6
         //UNK-TC: Unknown Data Block (Tag Code)
         pgrp = new cea_unktc_cl;
         wxedid_RCD_SET_FAULT_VMSG(retU,
                                   "[E!] CTA-861 DBC: unknown Tag Code=%u", tagcode);
   }

  *pp_grp = pgrp;
   return retU;
}

rcode EDID_cl::ParseCEA_DBC(u8_t *pinst) {
   rcode       retU;
   rcode       retU2;
   u32_t       orflags;
   u32_t       grp_sz;
   edi_grp_cl *pgrp = NULL;

   RCD_SET_OK(retU2);

   retU2 = ParseDBC_TAG(pinst, &pgrp);

   if (pgrp == NULL) {RCD_RETURN_FAULT(retU); }

   pgrp->setAbsOffs(calcGroupOffs(pinst));
   pgrp->setRelOffs(pgrp->getAbsOffs() % sizeof(edid_t));

   orflags = 0;
   if (b_ERR_Ignore) {
      orflags = T_MODE_EDIT;
   }

   retU = pgrp->init(pinst, orflags, NULL);
   if (!RCD_IS_OK(retU)) {
      //the fault message is logged, but ignored
      if (retU.detail.rcode > RCD_FVMSG) {
         delete pgrp;
         return retU;
      }
   }

   grp_sz = pgrp->getTotalSize();
   pGLog->slog.Printf("[%zu] offs %u: \"%s\", size %u",
                      EDI_Ext0GrpAr.GetCount(), pgrp->getRelOffs(),
                      pgrp->CodeName.c_str(), grp_sz );
   pGLog->DoLog();

   pgrp->setParentAr(&EDI_Ext0GrpAr);
   EDI_Ext0GrpAr.Append(pgrp);

   if (! RCD_IS_OK(retU2)) {return retU2;}
   if (! RCD_IS_OK(retU )) {return retU ;}


   RCD_RETURN_OK(retU);
}

rcode EDID_cl::ParseEDID_CEA() {
   rcode       retU;
   edi_grp_cl *pgrp;
   u8_t       *p8_dtd;
   u8_t       *pend;
   u8_t       *pinst;
   cea_hdr_t  *cea_hdr;
   u32_t       dtd_offs;
   i32_t       num_dtd;
   i32_t       blk0_dtd;

   EDI_Ext0GrpAr.Empty();

   u8_t *pext = EDID_buff.edi.ext0;

   pGLog->DoLog("CEA-861:");

   // CEA/CTA-861 header
   pgrp = new cea_hdr_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);

   retU = pgrp->init(pext, 0, NULL);
   if (!RCD_IS_OK(retU)) return retU;

   pgrp->setAbsOffs(calcGroupOffs(pext));
   EDI_Ext0GrpAr.Append(pgrp);

   cea_hdr  = reinterpret_cast <cea_hdr_t*> (pext);
   dtd_offs = cea_hdr->dtd_offs;
   num_dtd  = cea_hdr->info_blk.num_dtd;
   blk0_dtd = EDID_Get_num_DTD();

   if (num_dtd > blk0_dtd) {
      num_dtd -= blk0_dtd; //native DTDs present in CEA
   } else {
      num_dtd = 0; //no native DTDs present in CEA (but non-native can be included)
   }

   pGLog->DoLog("[0] offs 0: \"CHD\", size 4");

   //No DTD, no DBC
   if (dtd_offs == 0) {num_valid_blocks++ ; RCD_RETURN_OK(retU);}

   if ((dtd_offs >= 1) && (3 >= dtd_offs)) {
      wxedid_RCD_SET_FAULT_VMSG(retU, "[E!] CTA-861 Header: invalid DTD offset=%u", dtd_offs);
      return retU; //can't be ignored
   }

   //(dtd_offs > (EDI_BLK_SIZE - sizeof(dsctor_u) -3))

   p8_dtd = (pext + dtd_offs);
   pend   = (pext + EDI_BLK_SIZE);
   pinst  =  pext + sizeof(cea_hdr_t);

   //DBC
   if (dtd_offs > 0x04) {
      ethdr_t  ethdr;
      u32_t    blklen;

      do {
         //Parse Data Block Collection (DBC)
         ethdr.w16 = reinterpret_cast<u16_t*> (pinst)[0];
         blklen    = ethdr.ehdr.hdr.tag.blk_len;
         blklen   += sizeof(bhdr_t);

         if (p8_dtd < (pinst + blklen)) {
            wxedid_RCD_SET_FAULT_VMSG(retU,
                 "[E!] CTA-861: Collision: DTD offset=%u and DBC@offset=%u, len=%u",
                 dtd_offs, (u32_t) (calcGroupOffs(pinst) % sizeof(edid_t)), blklen);
            if (! b_ERR_Ignore) return retU;
            pGLog->PrintRcode(retU);
            RCD_SET_OK(retU);
         }

         retU = ParseCEA_DBC(pinst);
         if (! RCD_IS_OK(retU)) {
            if (! b_ERR_Ignore) return retU;
            RCD_SET_OK(retU);
         }

         pinst += blklen;
         if (pinst >= p8_dtd) break;

      } while (pinst < pend);

      if (pinst != p8_dtd) {
         wxedid_RCD_SET_FAULT_VMSG(retU,
              "[E!] CTA-861: DTD offset=%u != DBC_end=%u",
              dtd_offs, (u32_t) (calcGroupOffs(pinst) % sizeof(edid_t)) );
         if (! b_ERR_Ignore) return retU;
         pGLog->PrintRcode(retU);
         RCD_SET_OK(retU);
      }
   }

   {  //DTD
      dtd_t      *pdtd;
      edi_grp_cl *pgrp;
      i32_t       space_left;
      i32_t       max_dtd;
      u32_t       offs;

      //search DTDs after DBC end, not at declared DTD offset
      pdtd        = reinterpret_cast<dtd_t*> (pinst);
      space_left  = (pend - pinst);
      space_left -- ; //last byte contains checksum
      max_dtd     = space_left / sizeof(dtd_t); //18

      //mandatory DTDs for *native* mode
      if (num_dtd > 0) {
         if (max_dtd < num_dtd) {
            wxedid_RCD_SET_FAULT_VMSG(retU,
                 "[E!] CTA-861: insufficient space for declared number of native DTDs: %u, max: %u",
                 num_dtd, max_dtd);
            if (! b_ERR_Ignore) return retU;
            pGLog->PrintRcode(retU);
            num_dtd = max_dtd;
            RCD_SET_OK(retU);
         }

         for (i32_t itd=0; itd<num_dtd; itd++) {

            if (pdtd->pix_clk == 0) {
               wxedid_RCD_SET_FAULT_VMSG(retU,
                    "[E!] CTA-861: missing mandatory DTD @ offset %u",
                    (u32_t) (reinterpret_cast <u8_t*> (pdtd) - EDID_buff.buff) );
               if (! b_ERR_Ignore) return retU;
               pGLog->PrintRcode(retU);
               RCD_SET_OK(retU);
               break;
            }

            pgrp = new dtd_cl;
            if (pgrp == NULL) {
               RCD_SET_FAULT(retU);
               pGLog->PrintRcode(retU);
               return retU;
            }

            pgrp->init(reinterpret_cast <u8_t*> (pdtd), 0, NULL);

            offs = (reinterpret_cast <u8_t*> (pdtd) - EDID_buff.buff);

            //append the group
            pgrp->setAbsOffs(offs);                  //offset in buffer
            pgrp->setRelOffs(offs % sizeof(edid_t)); //offset in extension block

            pGLog->slog.Printf("[%zu] offs %u: \"DTD\", size 18",
                      EDI_Ext0GrpAr.GetCount(), pgrp->getRelOffs() );
            pGLog->DoLog();

            EDI_Ext0GrpAr.Append(pgrp);

            pdtd       ++ ;
            max_dtd    -- ;
            space_left -= sizeof(dtd_t);
         }
      }

      //Check for additional DTDs (non-mandatory)
      for (i32_t itd=0; itd<max_dtd; itd++) {
         if (pdtd->pix_clk == 0) break; //not a DTD

         pgrp = new dtd_cl;
         if (pgrp == NULL) {
            RCD_SET_FAULT(retU);
            pGLog->PrintRcode(retU);
            return retU;
         }

         pgrp->init(reinterpret_cast <u8_t*> (pdtd), 0, NULL);

         offs = (reinterpret_cast <u8_t*> (pdtd) - EDID_buff.buff);

         //append the group
         pgrp->setAbsOffs(offs);
         pgrp->setRelOffs(offs % sizeof(edid_t));

         pGLog->slog.Printf("[%zu] offs %u: \"DTD\", size 18",
                            EDI_Ext0GrpAr.GetCount(), pgrp->getRelOffs() );
         pGLog->DoLog();

         EDI_Ext0GrpAr.Append(pgrp);

         pdtd       ++ ;
         space_left -= sizeof(dtd_t);
      }

      //Check padding bytes
      p8_dtd = reinterpret_cast <u8_t*> (pdtd);

      pGLog->slog.Printf("[%zu] offs %zu: [free space]: %u bytes",
                         EDI_Ext0GrpAr.GetCount(), (p8_dtd - pext), space_left );
      pGLog->DoLog();

      for (i32_t itb=0; itb<space_left; itb++) {
         if (*p8_dtd != 0) {
            wxedid_RCD_SET_FAULT_VMSG(retU,
                                      "[E!] CTA-861: padding byte != 0 @ offset %u",
                                      (u32_t) (p8_dtd - EDID_buff.buff) );
            if (! b_ERR_Ignore) return retU;
            pGLog->PrintRcode(retU);
            RCD_SET_OK(retU);
         }

         p8_dtd ++ ;
      }
   }

   num_valid_blocks++ ;

   //free/used used bytes in the block
   EDI_Ext0GrpAr.CalcDataSZ();

   RCD_RETURN_OK(retU);
}

rcode EDID_cl::ParseEDID_Base(u32_t& n_extblk) {
   rcode       retU;
   edi_grp_cl *pgrp;
   dsctor_u   *pdsc;
   u32_t       hdr0, hdr1;

   EDI_BaseGrpAr.Empty();
   num_valid_blocks = 0;

   //check header
   hdr0 = EDID_buff.edi.base.hdr.hdr_w32[0];
   hdr1 = EDID_buff.edi.base.hdr.hdr_w32[1];
   _BE_SWAP32(hdr0);
   _BE_SWAP32(hdr1);

   if ((hdr0 != 0xFFFFFF00) || (hdr1 != 0x00FFFFFF)) {
      wxedid_RCD_SET_FAULT_VMSG(retU,
                                "[E!] EDID block0: invalid header=0x%08X_%08X",
                                hdr0, hdr1);
      if (! b_ERR_Ignore) return retU;
      pGLog->PrintRcode(retU);
   }
   //BED: Base EDID data
   pgrp = new edibase_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init(reinterpret_cast <u8_t*> (&EDID_buff.edi.base.hdr), 0, NULL);
   if (!RCD_IS_OK(retU)) return retU;
   EDI_BaseGrpAr.Append(pgrp);
   //VID: Video Input Descriptor
   pgrp = new vindsc_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init(reinterpret_cast <u8_t*> (&EDID_buff.edi.base.vinput_dsc), 0, NULL);
   if (!RCD_IS_OK(retU)) return retU;
   EDI_BaseGrpAr.Append(pgrp);
   //BDD: basic display descriptior
   pgrp = new bddcs_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init( reinterpret_cast <u8_t*> (&EDID_buff.edi.base.bdd), 0, NULL);
   if (!RCD_IS_OK(retU)) return retU;
   EDI_BaseGrpAr.Append(pgrp);
   //SPF: Supported features class
   pgrp = new spft_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init(reinterpret_cast <u8_t*> (&EDID_buff.edi.base.features), 0, NULL);
   if (!RCD_IS_OK(retU)) return retU;
   EDI_BaseGrpAr.Append(pgrp);
   //CXY: CIE Chromacity coords class
   pgrp = new chromxy_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init(reinterpret_cast <u8_t*> (&EDID_buff.edi.base.chromxy), 0, NULL);
   if (!RCD_IS_OK(retU)) return retU;
   EDI_BaseGrpAr.Append(pgrp);
   //Resolution map class
   pgrp = new resmap_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init(reinterpret_cast <u8_t*> (&EDID_buff.edi.base.res_map), 0, NULL);
   if (!RCD_IS_OK(retU)) return retU;
   EDI_BaseGrpAr.Append(pgrp);
   //STI: Std Timing Information descriptors
   {
      dmt_std2_t *pstdt = &EDID_buff.edi.base.std_timg0;
      for (u32_t itd=0; itd<8; itd++) {
         u32_t  abs_offs;

         pgrp = new dmt_std2_cl;
         if (pgrp == NULL) RCD_RETURN_FAULT(retU);

         abs_offs = calcGroupOffs(pstdt);
         pgrp->setAbsOffs(abs_offs);
         pgrp->setRelOffs(abs_offs);
         pgrp->init(reinterpret_cast <u8_t*> (pstdt), T_P_HOLDER, NULL);
         pgrp->setParentAr(&EDI_BaseGrpAr); //needed for Paste() >> UpdateAbsOffs()
         EDI_BaseGrpAr.Append(pgrp);
         pstdt += 1;
      }
   }
   //DTD/MRL/WPT...

   pGLog->DoLog("BASE EDID: descriptors:");

   pdsc = &EDID_buff.edi.base.descriptor0;
   for (u32_t itd=0; itd<4; itd++) {
      u32_t  offs;

      retU = ParseAltDtor(reinterpret_cast <u8_t*> (pdsc), &pgrp);
      if (pgrp == NULL) return retU;

      //offset in buffer
      offs = calcGroupOffs(pdsc);
      pgrp->setAbsOffs(offs);
      pgrp->setRelOffs(offs);

      retU = pgrp->init(reinterpret_cast <u8_t*> (pdsc), 0, NULL);
      if (! RCD_IS_OK(retU)) {
         delete pgrp;
         return retU;
      }

      pgrp->setParentAr(&EDI_BaseGrpAr);
      EDI_BaseGrpAr.Append(pgrp);
      pdsc += 1;

      pGLog->slog.Printf("[%u] offs %u: \"%s\", size %u",
                         itd, offs, pgrp->CodeName.c_str(), pgrp->getDataSize());
      pGLog->DoLog();
   }

   num_valid_blocks = 1;
   n_extblk         = EDID_buff.edi.base.num_extblk;

   //free/used bytes in the block
   EDI_BaseGrpAr.CalcDataSZ();

   RCD_RETURN_OK(retU);
}

rcode EDID_cl::ParseAltDtor(u8_t *pinst, edi_grp_cl** pp_grp, i32_t offs) {
   rcode       retU;
   u32_t       dsctype;
   edi_grp_cl *pgrp;
   dsctor_u   *pdsc;

   pdsc = reinterpret_cast <dsctor_u*> (pinst);

   //DTD
   if (pdsc->dtd.pix_clk != 0) {
      RCD_SET_OK(retU);

      pgrp = new dtd_cl;
      if (pgrp == NULL) {RCD_SET_FAULT(retU); goto _err; }

      goto _exit;
   }

   dsctype = pdsc->unk.hdr.dsc_type;

   //not a DTD: types 0xFA...0xFF
   switch (dsctype) {
      case 0xF7: //ET3: Estabilished Timings 3 Descriptor
         pgrp = new et3_cl;
         if (pgrp == NULL) {RCD_SET_FAULT(retU); goto _err; }
         break;
      case 0xF8: //CT3: VESA-CVT 3-byte Video Timing Codes
         pgrp = new ct3_cl;
         if (pgrp == NULL) {RCD_SET_FAULT(retU); goto _err; }
         break;
      case 0xF9: //DCM: Display Color Management Data
         pgrp = new dcm_cl;
         if (pgrp == NULL) {RCD_SET_FAULT(retU); goto _err; }
         break;
      case 0xFA: //AST: Additional Standard Timings identifiers
         pgrp = new ast_cl;
         if (pgrp == NULL) {RCD_SET_FAULT(retU); goto _err; }
         break;
      case 0xFB: //WPD: White Point Descriptor
         pgrp = new wpt_cl;
         if (pgrp == NULL) {RCD_SET_FAULT(retU); goto _err; }
         break;
      case 0xFD: //MRL: Monitor Range Limits
         pgrp = new mrl_cl;
         if (pgrp == NULL) {RCD_SET_FAULT(retU); goto _err; }
         break;
      case 0xFE: //UTX: Unspecified Text
      case 0xFC: //MND: Monitor Name Descriptor
      case 0xFF: //MSN: Monitor Serial Number Descriptor
         pgrp = new txtd_cl;
         if (pgrp == NULL) {RCD_SET_FAULT(retU); goto _err; }
         break;
      default:
         //UNK: Unknown Descriptor (fallback)
         pgrp = new unk_cl;
         if (pgrp == NULL) {RCD_SET_FAULT(retU); goto _err; }

         //offs > 0: called by BlkTreeChangeGrpType(): pinst is a local data buffer
         if (offs < 0) offs = calcGroupOffs(pdsc);
         wxedid_RCD_SET_FAULT_VMSG(retU,
                                   "[E!] Unknown alt. descriptor type=0x%02X, abs. offset=%u",
                                   dsctype, offs);
         break;
   }

_err:
_exit:
   *pp_grp = pgrp;
   return retU;
}

bool EDID_cl::VerifyChksum(u32_t block) {
   if (block > EDI_EXT2_IDX) return false;

   u32_t csum = 0;
   u8_t *pblk = EDID_buff.blk[block];

   for (u32_t itb=0; itb<EDI_BLK_SIZE; itb++) {
      csum += pblk[itb];
   }
   return ((csum & 0xFF) == 0);
}

u32_t EDID_cl::genChksum(u32_t block) {
   if (block > EDI_EXT2_IDX) return 0;

   u32_t csum = 0;
   u8_t *pblk = EDID_buff.blk[block];

   for (u32_t itb=0; itb<(EDI_BLK_SIZE-1); itb++) {
      csum += pblk[itb];
   }
   csum = (0x100 - (csum & 0xFF));
   pblk[EDI_BLK_SIZE-1] = csum;
   return csum;
}

void EDID_cl::Clear() {
   EDI_BaseGrpAr.Clear();
   EDI_Ext0GrpAr.Clear();
   EDI_Ext1GrpAr.Clear();
   EDI_Ext2GrpAr.Clear();
   num_valid_blocks = 0;
   memset(EDID_buff.buff, 0, sizeof(edi_buf_t) );
}

u32_t EDID_cl::EDID_Get_num_DTD() {
   edi_grp_cl *pgrp;
   u32_t       idx;
   u32_t       grp_cnt;
   u32_t       num_dtd = 0;

   grp_cnt = EDI_BaseGrpAr.GetCount();
   idx     = 14; //first alt. descriptor

   for (; idx<grp_cnt; ++idx) {
      gtid_t  tid;

      pgrp = EDI_BaseGrpAr[idx];
      tid  = pgrp->getTypeID();

      if (ID_DTD == tid.base_id) {
         num_dtd ++ ;
      }
   }

   return num_dtd;
}

void EDID_cl::CEA_Set_DTD_Offset(u8_t *pbuf, GroupAr_cl *p_grp_ar) {
   edi_grp_cl *pgrp;
   cea_hdr_t  *cea_hdr;
   u32_t       num_dtd;
   u32_t       idx;
   u32_t       grp_cnt;
   u32_t       dtd_offs = 0;

   cea_hdr = reinterpret_cast<cea_hdr_t*> (pbuf);
   grp_cnt = p_grp_ar->GetCount();
   num_dtd = 0;
   idx     = 1; //skip CEA header

   for (; idx<grp_cnt; ++idx) {
      gtid_t  tid;
      pgrp  = p_grp_ar->Item(idx);
      tid   = pgrp->getTypeID();

      if (ID_DTD == (tid.t32 & ID_EDID_MASK)) {
         if (num_dtd == 0) {
            dtd_offs = pgrp->getRelOffs();
         }
         num_dtd ++ ;
      }
   }

   if (num_dtd > 0) goto done;
   if (grp_cnt < 1) dtd_offs = 0; //No DBC and no DTD

done:
   cea_hdr->dtd_offs = dtd_offs;

   {
      u32_t num_dbc;

      num_dbc  = grp_cnt;
      num_dbc -= num_dtd;
      num_dbc -= 1; //hdr

      pGLog->slog.Printf("CEA_Set_DTD_Offset(): num_dbc=%u, num_dtd=%u, dtd_offs=%u",
                         num_dbc, num_dtd, dtd_offs);
      pGLog->DoLog();
   }
}

rcode EDID_cl::AssembleEDID() {
   edi_grp_cl  *pgrp;
   GroupAr_cl  *p_grp_ar;

   u8_t   *pbuf;
   u32_t   block;
   i32_t   blk_sz;
   u32_t   offs;
   u32_t   idx_grp;
   u32_t   sg_idx;
   u32_t   n_grp;
   rcode   retU;

   for (block=0; block<num_valid_blocks; ++block) {
      pbuf     = EDID_buff.blk[block];
      p_grp_ar = BlkGroupsAr  [block];
      blk_sz   = sizeof(edid_t);
      blk_sz  -= 1; //checksum byte
      offs     = 0;

      if (block == 0) blk_sz -= 1; //edid_t.num_extblk

      n_grp = p_grp_ar->GetCount();

      if ((block > EDI_BASE_IDX) && (n_grp == 0)) {
         pGLog->slog.Printf("[i] Preserving unsupported extension block %u", block);
         pGLog->DoLog();
         continue;
      }

      if (n_grp == 0) {
         RCD_RETURN_FAULT_MSG(retU, "[E!] Invalid number of extension blocks");
      }

      pGLog->slog.Printf("AssembleEDID(): block %u/%u, groups %u", block, num_valid_blocks, n_grp);
      pGLog->DoLog();

      for (idx_grp=0; idx_grp<n_grp; ++idx_grp) {
         u32_t       n_subg;
         u32_t       dat_sz;
         gtid_t      gtid;
         edi_grp_cl *p_subg;

         if (blk_sz <= 0) { goto err; }

         pgrp   = p_grp_ar->Item(idx_grp);
         dat_sz = pgrp->getDataSize();
         n_subg = pgrp->getSubGrpCount();

         pGLog->slog.Printf("[%u] offs %u: \"%s\", size %u",
                            idx_grp, offs, pgrp->CodeName.c_str(), pgrp->getTotalSize());
         if (n_subg != 0) {
            pGLog->slog << ", sub-groups: " << n_subg;
         }
         pGLog->DoLog();

         if (block == EDI_EXT0_IDX) {
            gtid = pgrp->getTypeID();
            if (gtid.base_id != 0) {
               if (ID_DTD != gtid.base_id) {
                  pGLog->slog.Printf("[E!] Invalid group for CTA-861 block: '%s: %s', "
                                     "offs: %u",
                                     pgrp->CodeName.c_str(), pgrp->GroupName.c_str(),
                                     pgrp->getRelOffs());
                  pGLog->DoLog();
                  RCD_RETURN_FAULT(retU);
               }
            }
         }

         pgrp->SpawnInstance(&pbuf[offs]);
         offs   += dat_sz;
         blk_sz -= dat_sz;

         for (sg_idx=0; sg_idx<n_subg; ++sg_idx) {

            if (blk_sz <= 0) { goto err; }

            p_subg = pgrp->getSubGroup(sg_idx);
            dat_sz = p_subg->getDataSize();

            pGLog->slog.Printf("   [%u] offs %u: \"%s\", size %u",
                               sg_idx, offs, p_subg->CodeName.c_str(), dat_sz);
            pGLog->DoLog();

            p_subg->SpawnInstance(&pbuf[offs]);
            offs   += dat_sz;
            blk_sz -= dat_sz;
         }
      }

      //base EDID: consistency check
      if ((block == 0) && (blk_sz > 0)) {
         unk_t *unk;
         //check if size is multiple of desc. size
         if ((blk_sz % sizeof(unk_t)) != 0) {
            RCD_RETURN_FAULT_MSG(retU, "[E!] Assebling FAILED, internal error");
         } else
         if (! b_ERR_Ignore) {
            wxedid_RCD_RETURN_FAULT_VMSG(retU,
                   "[E!] Base EDID block: missing descriptor(s) @offset=%u", offs);
         }
         //clear remaing buf. space, then inject VOID descriptor(s), type 0x10
         memset(&pbuf[offs], 0, blk_sz);

         unk = reinterpret_cast <unk_t*> (&pbuf[offs]);
         do {
            pGLog->slog.Printf("[i] Descriptor type 0x10 (VOID) inserted, offs %u", offs);
            pGLog->DoLog();

            unk->hdr.dsc_type = 0x10;

            offs   += sizeof(unk_t);
            blk_sz -= sizeof(unk_t);
            unk     = reinterpret_cast <unk_t*> (&pbuf[offs]);

         } while (blk_sz > 0);
      }

      //CEA: clear unused bytes
      if (block == 1) {

         pGLog->slog.Printf("[%u] offs: %u [free space]: %u bytes", idx_grp, offs, blk_sz );
         pGLog->DoLog();

         while (blk_sz > 0) {
            pbuf[offs] = 0;
            offs   ++ ;
            blk_sz -- ;
         }

         //Update DTD offset
         CEA_Set_DTD_Offset(pbuf, p_grp_ar);
      }
   }

   RCD_RETURN_OK(retU);

err:
   wxedid_RCD_SET_FAULT_VMSG(retU, "[E!] AssembleEDID(): Block[%u] size limit reached: [idx=%u] \'%s\', sub-group idx=%u",
                             block, idx_grp, (const char*) pgrp->GroupName.c_str(), sg_idx);
   return retU;
}

//Common handlers
rcode EDID_cl::BitVal(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;
   u8_t   bmask;

   inst = getValPtr(p_field);

   if ((p_field->field.flags & F_BIT) == 0) RCD_RETURN_FAULT(retU);

   bmask = (1 << p_field->field.shift);

   if (op == OP_READ) {
      ival = ((inst[0] & bmask) >> p_field->field.shift);
      sval << ival;
      RCD_SET_OK(retU);
   } else { //write
      ulong val = 0;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         //fixed min and max are used -> this is a bit value
         retU = getStrUint(sval, 10, 0, 1, val);
         if (! RCD_IS_OK(retU)) return retU;
      } else if (op == OP_WRINT) {
         if ((p_field->field.flags & F_NI) != 0) RCD_RETURN_FAULT(retU);
         val = ival;
         RCD_SET_OK(retU);
      } else {
         RCD_RETURN_FAULT(retU); //wrong op code
      }
      val = (val << p_field->field.shift);
      inst[0] &= ~bmask;
      inst[0] |= (val & bmask);
   }
   return retU;
}

rcode EDID_cl::BitF8Val(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   ulong   bmask;
   u8_t   *inst;

   inst = getValPtr(p_field);

   if ((p_field->field.flags & F_BFD) == 0) RCD_RETURN_FAULT(retU);
   if ((p_field->field.fld_sz + p_field->field.shift) > 8) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      bmask = (0xFF >> (8 - p_field->field.fld_sz));
      ival  = ((inst[0] >> p_field->field.shift) & bmask);

      if (p_field->field.flags & F_INT) {
         sval.Empty(); sval << ival;
         RCD_SET_OK(retU);
      } else
      if (p_field->field.flags & F_HEX) {
         sval.Printf("0x%02X", ival);
         RCD_SET_OK(retU);
      } else {
         ulong tmpv = ival;
         uint  itb;
         char  chbit[12]; chbit[11] = 0;

         //read by bit
         for (itb=0; itb<p_field->field.fld_sz; itb++) {
            chbit[10-itb] = 0x30+(tmpv & 0x01); //to ASCII
            tmpv >>= 1;
         }
         tmps = wxc_String::FromAscii(&chbit[(11-itb)]);
         sval = "0b";
         sval << tmps;
         RCD_SET_OK(retU);
      }

   } else {
      ulong tmpv = 0;
      int   base;
      RCD_SET_FAULT(retU);

      bmask = ((0xFF >> (8 - p_field->field.fld_sz)) << p_field->field.shift);

      if (op == OP_WRSTR) {

         if (p_field->field.flags & F_INT) {
            base = 10;
         } else if (p_field->field.flags & F_HEX) {
            base = 16;
         } else {
            base = 2;
         }
         retU = getStrUint(sval, base, p_field->field.minv, p_field->field.maxv, tmpv);
         if (! RCD_IS_OK(retU)) return retU;

      } else
      if (op == OP_WRINT) {
         if ((p_field->field.flags & F_NI) != 0) RCD_RETURN_FAULT(retU);
         tmpv = ival;
         RCD_SET_OK(retU);
      } else {
         RCD_RETURN_FAULT(retU); //wrong op code
      }
      tmpv = ((tmpv << p_field->field.shift) & bmask);
      inst[0] &= ~bmask;
      inst[0] |= tmpv;
   }
   return retU;
}

rcode EDID_cl::BitF16Val(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   ulong   bmask;
   u8_t   *inst;
   u16_t   w16;

   inst = getValPtr(p_field);

   if ((p_field->field.flags & F_BFD) == 0) RCD_RETURN_FAULT(retU);
   if ((p_field->field.fld_sz + p_field->field.shift) > 16) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      w16 = rdWord16_LE(inst);

      bmask = (0xFFFF >> (16 - p_field->field.fld_sz));
      ival  = ((w16 >> p_field->field.shift) & bmask);

      if (p_field->field.flags & F_INT) {
         sval.Empty(); sval << ival;
         RCD_SET_OK(retU);
      } else
      if (p_field->field.flags & F_HEX) {
         sval.Printf("0x%04X", ival);
         RCD_SET_OK(retU);
      } else {
         ulong tmpv = ival;
         uint  itb;
         char  chbit[20]; chbit[19] = 0;

         //read by bit
         for (itb=0; itb<p_field->field.fld_sz; itb++) {
            chbit[10-itb] = 0x30+(tmpv & 0x01); //to ASCII
            tmpv >>= 1;
         }
         tmps = wxc_String::FromAscii(&chbit[(19-itb)]);
         sval = "0b";
         sval << tmps;
         RCD_SET_OK(retU);
      }

   } else {
      ulong tmpv = 0;
      int   base;
      RCD_SET_FAULT(retU);
      ulong bmask = ((0xFFFF >> (16 - p_field->field.fld_sz)) << p_field->field.shift);

      if (op == OP_WRSTR) {

         if (p_field->field.flags & F_INT) {
            base = 10;
         } else if (p_field->field.flags & F_HEX) {
            base = 16;
         } else {
            base = 2;
         }
         retU = getStrUint(sval, base, p_field->field.minv, p_field->field.maxv, tmpv);
         if (! RCD_IS_OK(retU)) return retU;

         ival = tmpv;
      } else
      if (op == OP_WRINT) {
         if ((p_field->field.flags & F_NI) != 0) RCD_RETURN_FAULT(retU);
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      ival = ((ival << p_field->field.shift) & bmask);

      w16  = rdWord16_LE(inst);
      _BE_SWAP16(w16);

      w16 &= ~bmask;
      w16 |= ival;

      wrWord16_LE(inst, w16);
   }
   return retU;
}

rcode EDID_cl::ByteVal(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   inst = getValPtr(p_field);

   if ((p_field->field.flags & (F_BIT|F_STR)) != 0) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) { //read
      ival = inst[0];
      if (p_field->field.flags & F_INT) {
         sval.Empty(); sval << ival;
         RCD_SET_OK(retU);
      } else
      if (p_field->field.flags & F_HEX) {
         sval.Printf("0x%02X", ival);
         RCD_SET_OK(retU);
      } else {
         RCD_SET_FAULT(retU);
      }

   } else { //write
      ulong val = 0;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         if (p_field->field.flags & F_INT) {
            retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, val);
         }
         if (p_field->field.flags & F_HEX) {
            if (sval.SubString(0, 1) != "0x") RCD_RETURN_FAULT(retU);
            retU = getStrUint(sval, 16, p_field->field.minv, p_field->field.maxv, val);
         }
         if (! RCD_IS_OK(retU)) return retU;
      } else
      if (op == OP_WRINT) {
         if ((p_field->field.flags & F_NI) != 0) RCD_RETURN_FAULT(retU);
         val = ival;
         RCD_SET_OK(retU);
      } else {
         RCD_RETURN_FAULT(retU); //wrong op code
      }
      inst[0] = val;
   }
   return retU;
}

rcode EDID_cl::FldPadStr(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u32_t  itb;
   u8_t  *inst;

   inst = getValPtr(p_field);

   u32_t maxl = p_field->field.fld_sz;
   if (maxl > 31) RCD_RETURN_FAULT(retU);

   char cbuff[32];

   if (op == OP_READ) {
      //max maxl chars allowed
      for (itb=0; itb<maxl; itb++) {
         if (0x0A == inst[itb]) break;
         cbuff[itb] = inst[itb];
      }
      cbuff[itb] = 0;
      sval = wxc_String::From8BitData(cbuff);
      RCD_SET_OK(retU);

   } else {
      if (op == OP_WRINT) RCD_RETURN_FAULT(retU);
      if (sval.Len() > maxl) RCD_RETURN_FAULT(retU);

      memcpy(cbuff, sval.To8BitData(), sval.Len());

      for (itb=0; itb<sval.Len(); itb++) {
         char chr = cbuff[itb];
         if (chr == 0) break;
         inst[itb] = chr;
      }
      //padding
      if (itb < maxl) inst[itb++] = 0x0A; //LF
      for (; itb<maxl; itb++) {
         inst[itb] = 0x20; //SP
      }
      RCD_SET_OK(retU);
   }
   ival = 0;
   return retU;
}

rcode EDID_cl::getStrUint(wxc_String& sval, int base, u32_t minv, u32_t maxv, ulong& val) {
   rcode retU;
   ulong tmpv;

   RCD_SET_OK(retU);

   //check conversion base
   if (base != 10) { // expect '0b' or '0x' prefix
      u32_t slen = sval.Len();
      if (slen < 3) RCD_RETURN_FAULT(retU);

      tmps2 = sval.SubString(0, 1);
      switch (base) {
         case 16:
            if (tmps2.Cmp("0x") != 0) RCD_RETURN_FAULT(retU);
            break;
         case 2:
            if (tmps2.Cmp("0b") != 0) RCD_RETURN_FAULT(retU);
            break;
         default:
            RCD_RETURN_FAULT(retU);
      }

      if (! (sval.SubString(2, slen)).ToULong(&tmpv, base)) {
         RCD_RETURN_FAULT(retU);
      }
   } else {
      if (! sval.ToULong(&tmpv, base)) {
         RCD_RETURN_FAULT(retU);
      }
   }

   if (tmpv > maxv) {
      RCD_SET_FAULT(retU);
   }
   if (tmpv < minv) {
      RCD_SET_FAULT(retU);
   }
   val = tmpv;
   return retU;
}

rcode EDID_cl::getStrFloat(wxc_String& sval, float minv, float maxv, float& val) {
   rcode   retU;
   double  dval;

   RCD_SET_OK(retU);

   if (! sval.ToDouble(&dval)) {
      RCD_RETURN_FAULT(retU);
   }

   val = (float) dval;
   if (val > maxv) {
      RCD_SET_FAULT(retU);
   }
   if (val < minv) {
      RCD_SET_FAULT(retU);
   }
   return retU;
}

rcode EDID_cl::Gamma(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   uint    tmpv;
   float   fval;
   void*   inst = NULL;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      tmpv = ( (reinterpret_cast <u8_t*> (inst))[0] + 100);
      ival = tmpv;
      fval = tmpv;
      sval.Printf("%.02f", (fval/100.0));
   } else {
      if (op == OP_WRINT) RCD_RETURN_FAULT(retU);

      retU = getStrFloat(sval, 1.0, 3.54, fval);
      if (! RCD_IS_OK(retU)) return retU;

      tmpv = (fval*100.0);
      *(reinterpret_cast <u8_t*> (inst)) = (tmpv - 100);
   }

   RCD_RETURN_OK(retU);
}

rcode EDID_cl::rdByteStr(wxc_String& sval, u8_t* pstrb, u32_t slen) {
   rcode  retU;
   if (pstrb == NULL) RCD_RETURN_FAULT(retU);

   sval = "0x";
   for (u32_t itb=0; itb<slen; itb++) {
      tmps.Printf("%02X", pstrb[itb]);
      sval << tmps;
   }

   RCD_RETURN_OK(retU);
}

rcode EDID_cl::rdByteStrLE(wxc_String& sval, u8_t* pstrb, u32_t slen) {
   rcode  retU;
   if (pstrb == NULL) RCD_RETURN_FAULT(retU);

   sval = "0x";
   for (ssize_t itb=(slen-1); itb>=0; itb--) {
      tmps.Printf("%02X", pstrb[itb]);
      sval << tmps;
   }

   RCD_RETURN_OK(retU);
}

rcode EDID_cl::wrByteStr(wxc_String& sval, u8_t* pstrb, u32_t slen) {
   rcode  retU;
   if (pstrb == NULL) RCD_RETURN_FAULT(retU);

   //required prefix
   if (sval.SubString(0, 1).Cmp("0x") != 0) RCD_RETURN_FAULT(retU);

   u32_t clen = sval.Len();
   if ((clen & 0x1) != 0  ) RCD_RETURN_FAULT(retU);
   if (((clen-2)/2) > slen) RCD_RETURN_FAULT(retU);

   clen --;
   u32_t itc  = 2;
   ulong tmpv = 0;
   for (u32_t itb=0; itb<slen; itb++) {
      if (itc >= clen) break;
      if (! sval.SubString(itc, itc+1).ToULong(&tmpv, 16)) {
         RCD_RETURN_FAULT(retU);
      }
      pstrb[itb] = static_cast <u8_t> (tmpv);
      itc+=2;
   }

   RCD_RETURN_OK(retU);
}

rcode EDID_cl::wrByteStrLE(wxc_String& sval, u8_t* pstrb, u32_t slen) {
   rcode  retU;

   if (pstrb == NULL) RCD_RETURN_FAULT(retU);

   //required prefix
   if (sval.SubString(0, 1).Cmp("0x") != 0) {
      RCD_RETURN_FAULT(retU);
   }

   u32_t itc = sval.Len();
   if (itc < 4) { //1 byte minimum: 0xBB
      RCD_RETURN_FAULT(retU);}
   if ((itc & 0x1) != 0) { //incomplete byte
      RCD_RETURN_FAULT(retU);}
   itc = (itc-2)/2;
   if (itc > slen) { //too long
      RCD_RETURN_FAULT(retU);}

   i32_t itb = (slen-1);
   ulong tmpv  = 0;
   if (itc < slen) {
      do {
         pstrb[itb--] = 0;
         slen-- ;
      } while (itc < slen);
   }

   itc = 2;
   for (; itb>=0; itb--) {
      if (! sval.SubString(itc, itc+1).ToULong(&tmpv, 16)) {
         RCD_RETURN_FAULT(retU);
      }
      pstrb[itb] = static_cast <u8_t> (tmpv);
      itc += 2;
   }

   RCD_RETURN_OK(retU);
}

rcode EDID_cl::ByteStr(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   RCD_SET_OK(retU);

   inst = getValPtr(p_field);

   if ((p_field->field.flags & F_STR) == 0) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      if ((p_field->field.flags & F_LE) == 0) {
         retU = rdByteStr(sval, inst, p_field->field.fld_sz);
      } else {
         retU = rdByteStrLE(sval, inst, p_field->field.fld_sz);
      }
   } else {
      if (op == OP_WRINT) RCD_RETURN_FAULT(retU);

      if ((p_field->field.flags & F_LE) == 0) {
         retU = wrByteStr(sval, inst, p_field->field.fld_sz);
      } else {
         retU = wrByteStrLE(sval, inst, p_field->field.fld_sz);
      }
   }
   ival = 0;
   return retU;
}

rcode EDID_cl::Word16(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   static const char fmt_dec[] = "%u";
   static const char fmt_hex[] = "0x%04X";

   rcode       retU;
   u32_t       flags;
   const char *fmt;
   u8_t       *p_w16;
   u16_t       w16;

   RCD_SET_FAULT(retU);

   p_w16 = getValPtr(p_field);
   flags = p_field->field.flags;

   if (op == OP_READ) {
      w16 = rdWord16_LE(p_w16);
      //swap forced
      if (F_LE & flags) w16 = __bswap_16(w16);

      ival = w16;
      fmt  = (F_HEX & flags) ? fmt_hex : fmt_dec;
      sval.Printf(fmt, ival);
      RCD_SET_OK(retU);

   } else {

      if (op == OP_WRSTR) {
         ulong tmpv = 0;
         int   base;

         base = (F_HEX & flags) ? 16 : 10;
         retU = getStrUint(sval, base, p_field->field.minv, p_field->field.maxv, tmpv);
         w16  = (u16_t) tmpv;

      } else if (op == OP_WRINT) {
         w16  = (u16_t) ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      if (F_LE & flags) w16 = __bswap_16(w16);
      wrWord16_LE(p_w16, w16);
   }
   return retU;
}

rcode EDID_cl::Word24(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   static const char fmt_dec[] = "%u";
   static const char fmt_hex[] = "0x%06X";

   rcode       retU;
   u32_t       flags;
   const char *fmt;
   u8_t       *p_w32;
   u32_t       w32;

   RCD_SET_FAULT(retU);

   p_w32 = getValPtr(p_field);
   flags = p_field->field.flags;

   if (op == OP_READ) {
      w32 = rdWord24_LE(p_w32);
      //byte swap forced
      if (F_LE & flags) {
         w32   = __bswap_32(w32);
         w32 >>= 8;
      }

      ival = w32;
      fmt  = (F_HEX & flags) ? fmt_hex : fmt_dec;
      sval.Printf(fmt, ival);
      RCD_SET_OK(retU);

   } else {

      if (op == OP_WRSTR) {
         ulong tmpv = 0;
         int   base;

         base = (F_HEX & flags) ? 16 : 10;
         retU = getStrUint(sval, base, p_field->field.minv, p_field->field.maxv, tmpv);
         w32  = (u32_t) tmpv;

      } else if (op == OP_WRINT) {
         w32  = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      if (F_LE & flags) {
         w32   = __bswap_32(w32);
         w32 >>= 8;
      }
      wrWord24_LE(p_w32, w32);
   }
   return retU;
}
