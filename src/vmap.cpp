/***************************************************************
 * Name:      vmap.cpp
 * Purpose:   value->description maps and value selector menus
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2024-2025
 * License:   GPLv3+
 **************************************************************/

#include "debug.h"
#include "rcdunits.h"
#ifndef idVMAP
   #error "vmap.cpp: missing unit ID"
#endif
#define RCD_UNIT idVMAP
#include "rcode/rcode.h"

#include "wxedid_rcd_scope.h"

RCD_AUTOGEN_DEFINE_UNIT

#include "vmap.h"
#include "id_flags.h"

#include <wx/string.h>

extern const vfmt_t dmt_table[]; //vid_fmt.cpp

extern sm_vmap AltDescType_map;  //EDID_dsc.cpp
extern sm_vmap MRL_ext_map;      //EDID_dsc.cpp
extern sm_vmap VID_input_map;    //EDID_base.cpp
extern sm_vmap STI_asp_ratio;    //EDID_base.cpp
extern sm_vmap CVT3_asp_ratio;   //EDID_base.cpp
extern sm_vmap CVT3_pref_vref;   //EDID_base.cpp
extern sm_vmap STD2_vidfmt_map;  //vid_fmt.cpp
extern sm_vmap SVD_vidfmt_map;   //vid_fmt.cpp
extern sm_vmap DBC_Tag_map;      //CEA.cpp
extern sm_vmap DBC_ExtTag_map;   //CEA.cpp
extern sm_vmap ADB_AFC_map;      //CEA.cpp
extern sm_vmap ADB_ACE_TC_map;   //CEA.cpp
extern sm_vmap SPKLD_IDX_map;    //CEA_ET_class.cpp
extern sm_vmap T7_AspRatio_map;  //CEA_ET_class.cpp
extern sm_vmap T7_3Dsupp_map;    //CEA_ET_class.cpp

//selector @idx zero is empty - idx==0 means VS_NO_SELECTOR
vmap_selector_t vmap_sel[] = {
   {             NULL,              NULL, NULL},
   {&VID_input_map   , &VID_input_map   , NULL},
   {&MRL_ext_map     , &MRL_ext_map     , NULL},
   {&AltDescType_map , &AltDescType_map , NULL},
   {&STI_asp_ratio   , &STI_asp_ratio   , NULL},
   {&CVT3_asp_ratio  , &CVT3_asp_ratio  , NULL},
   {&CVT3_pref_vref  , &CVT3_pref_vref  , NULL},
   {             NULL,              NULL, NULL}, //DMT-ID 1-byte codes, created from dmt_table[]
   {             NULL,              NULL, NULL}, //DMT-STD2 2-byte codes, ...
   {             NULL,              NULL, NULL}, //DMT-CVT 3-byte codes, ...
   {&SVD_vidfmt_map  , &SVD_vidfmt_map  , NULL},
   {&DBC_Tag_map     , &DBC_Tag_map     , NULL},
   {&DBC_ExtTag_map  , &DBC_ExtTag_map  , NULL},
   {&ADB_AFC_map     , &ADB_AFC_map     , NULL},
   {&ADB_ACE_TC_map  , &ADB_ACE_TC_map  , NULL},
   {&SPKLD_IDX_map   , &SPKLD_IDX_map   , NULL},
   {&T7_AspRatio_map , &T7_AspRatio_map , NULL},
   {&T7_3Dsupp_map   , &T7_3Dsupp_map   , NULL}
};

//value formats for menus and for vmap_GetValueAsString()
static const char fmt_dec [] = "%u";
static const char fmt_hex1[] = "0x%02X";
static const char fmt_hex2[] = "0x%04X";
static const char fmt_hex3[] = "0x%06X";


static const char*
get_value_fmt(u32_t idx) {
   const char* fmt;

   //specialized value formats
   switch (idx) {
      case VS_ALT_DSC_TYPE:
      case VS_MRL_EXT:
      case VS_DMT1_VIDFMT:
      case VS_SPKLD_IDX:
         fmt = fmt_hex1;
         break;
      case VS_STD2_VIDFMT:
         fmt = fmt_hex2;
         break;
      case VS_CVT3_VIDFMT:
         fmt = fmt_hex3;
         break;
      default:
         fmt = fmt_dec;
   }

   return fmt;
}

static rcode
init_selector(vmap_selector_t *psel, u32_t idx, u32_t flags) {
   rcode       retU;
   int         itm_id;
   const char* fmt;
   wxString    tmps;
   wxString    w_fmt;

   sm_vmap           *vmap;
   sm_vmap::iterator  itv;

   psel->selector = new wxMenu();
   if (psel->selector == NULL) {
      RCD_RETURN_FAULT(retU);
   }

   vmap = psel->mid_map;
   fmt  = get_value_fmt(idx);

   w_fmt.Printf("[%s] ", fmt);

   for(itv = vmap->begin(); itv != vmap->end(); ++itv) {
      int         tmpv;
      const char* p_str;

      itm_id            = itv->first;
      vmap_ent_t& m_ent = itv->second;

      if (F_VSVM & flags) {
         tmpv = m_ent.val; //item val stored in vmap
      } else {
         tmpv = itm_id;    //item val stored in item_id
      }

      tmps.Printf(w_fmt, tmpv);

      p_str = m_ent.name;
      tmps << wxString::FromAscii(p_str);

      p_str = m_ent.desc;
      if (p_str != NULL) {
         tmps << " ";
         tmps << wxString::FromAscii(p_str);
      }

      psel->selector->Append( itm_id, tmps);
   }

   RCD_RETURN_OK(retU);
}

sm_vmap*
vmap_GetVmap(u32_t idx, vmap_type vm_type) {
   bool             valid;
   vmap_selector_t *psel;
   sm_vmap         *vmap;

   valid  = (idx >= VS_START );
   valid &= (idx  < VS_NUM_OF);

   if (! valid) {
      return NULL; //not an error
   }

   psel = &vmap_sel[idx]; //NULL for uninitialized vmaps
   vmap = psel->map_ar[vm_type];

   return vmap;
}

const vmap_ent_t*
vmap_GetVmapEntry(u32_t vmap_idx, vmap_type vm_type, u32_t key_val) {
   sm_vmap           *vmap;
   sm_vmap::iterator  itv;
   vmap_ent_t        *m_ent;

   vmap = vmap_GetVmap(vmap_idx, vm_type);
   if (NULL == vmap) {
      return NULL;
   }

   itv = vmap->find(key_val);
   if (itv == vmap->end()) {
      return NULL;
   }

   m_ent = &vmap->at(key_val);

   return m_ent;
}

rcode
vmap_GetValueAsString(u32_t vs_idx, vmap_type vm_type, u32_t key_val, u32_t flags, wxString& sval) {
   rcode              retU;
   sm_vmap           *vmap;
   sm_vmap::iterator  itv;
   int                tmpv;
   const char        *fmt;

   vmap = vmap_GetVmap(vs_idx, vm_type);
   if (NULL == vmap) {
      RCD_RETURN_FAULT(retU);
   }

   itv  = vmap->find(key_val);
   if (itv == vmap->end()) {
      sval.Empty();
      RCD_RETURN_FALSE(retU);
   }
   vmap_ent_t& m_ent = vmap->at(key_val);

   fmt  = get_value_fmt(vs_idx);

   if (F_VSVM & flags) {
      tmpv = m_ent.val; //item val stored in vmap
   } else {
      tmpv = itv->first; //item val stored in item_id
   }

   sval.Printf(fmt, tmpv);

   RCD_RETURN_TRUE(retU);
}

wxMenu*
vmap_GetSelector(u32_t idx, u32_t flags, rcode& retU) {
   bool             valid;
   vmap_selector_t *psel;

   RCD_SET_OK(retU);

   valid  = (idx >= VS_START );
   valid &= (idx  < VS_NUM_OF);

   if (! valid) {
      return NULL; //not an error
   }

   psel = &vmap_sel[idx];

   if (psel->selector != NULL) return psel->selector;

   //deferred init of selector menus (on access)
   retU = init_selector(psel, idx, flags);
   if (! RCD_IS_OK(retU)) return NULL;

   return psel->selector;
}

void
vmap_DestroySelectors() {
   u32_t  its;

   vmap_selector_t *psel;

   for (its=VS_START; its<VS_NUM_OF; ++its) {
      psel = &vmap_sel[its];

      if (psel->selector != NULL) delete psel->selector;
   }
}

static rcode
init_DMT_vmap(u32_t idx, u32_t voffs) {
   rcode          retU;
   sm_vmap       *mid_map;
   sm_vmap       *val_map;
   vmap_ent_t     m_ent;
   const vfmt_t  *v_ent;
   u32_t          idxv;
   u32_t         *pval;

   m_ent.desc = NULL;

   mid_map = new sm_vmap;
   vmap_sel[idx].mid_map = mid_map;
   if (NULL == mid_map) RCD_RETURN_FAULT(retU);

   val_map = new sm_vmap;
   vmap_sel[idx].val_map = val_map;
   if (NULL == val_map) RCD_RETURN_FAULT(retU);

   idxv  = 0;
   v_ent = &dmt_table[0];

   while (v_ent->name != NULL) {
      pval       = (u32_t*) &((u8_t*) v_ent)[voffs];
      m_ent.val  = *pval;
      m_ent.name = v_ent->name;
      v_ent ++ ;
      if (0 == m_ent.val) continue;
      mid_map->emplace(idxv     , m_ent);
      val_map->emplace(m_ent.val, m_ent);
      idxv  ++ ;
   }

   RCD_RETURN_OK(retU);
}

rcode
vmap_InitDMT_Vmaps() {
   rcode          retU;

   //DMT-ID 1-byte codes
   retU = init_DMT_vmap(VS_DMT1_VIDFMT, offsetof(vfmt_t, DMT_ID));
   if (! RCD_IS_OK(retU)) RCD_RETURN_FAULT(retU);

   //DMT-STD2 2-byte codes
   retU = init_DMT_vmap(VS_STD2_VIDFMT, offsetof(vfmt_t, STD2  ));
   if (! RCD_IS_OK(retU)) RCD_RETURN_FAULT(retU);

   //DMT-CVT 3-byte codes
   retU = init_DMT_vmap(VS_CVT3_VIDFMT, offsetof(vfmt_t, CVT3  ));
   if (! RCD_IS_OK(retU)) RCD_RETURN_FAULT(retU);

   RCD_RETURN_OK(retU);
}

void
vmap_DeleteDMT_Vmaps() {
   sm_vmap  *vmap;

   vmap = vmap_sel[VS_DMT1_VIDFMT].mid_map;
   if (vmap != NULL) delete vmap;
   vmap = vmap_sel[VS_DMT1_VIDFMT].val_map;
   if (vmap != NULL) delete vmap;

   vmap = vmap_sel[VS_STD2_VIDFMT].mid_map;
   if (vmap != NULL) delete vmap;
   vmap = vmap_sel[VS_STD2_VIDFMT].val_map;
   if (vmap != NULL) delete vmap;

   vmap = vmap_sel[VS_CVT3_VIDFMT].mid_map;
   if (vmap != NULL) delete vmap;
   vmap = vmap_sel[VS_CVT3_VIDFMT].val_map;
   if (vmap != NULL) delete vmap;
}

