/***************************************************************
 * Name:      vmap.cpp
 * Purpose:   value->description maps (GTK4 port: menu building moved to app)
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

#include <stddef.h>

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
extern sm_vmap VID_if_type_map;  //EDID_base.cpp
extern sm_vmap VID_depth_map;    //EDID_base.cpp
extern sm_vmap DID_stereo_map;   //DisplayID.cpp
extern sm_vmap DID_product_map;  //DisplayID.cpp
extern sm_vmap DID2_product_map; //DisplayID.cpp

//selector @idx zero is empty - idx==0 means VS_NO_SELECTOR
vmap_selector_t vmap_sel[] = {
   {             NULL,              NULL},
   {&VID_input_map   , &VID_input_map   },
   {&MRL_ext_map     , &MRL_ext_map     },
   {&AltDescType_map , &AltDescType_map },
   {&STI_asp_ratio   , &STI_asp_ratio   },
   {&CVT3_asp_ratio  , &CVT3_asp_ratio  },
   {&CVT3_pref_vref  , &CVT3_pref_vref  },
   {             NULL,              NULL}, //DMT-ID 1-byte codes, created from dmt_table[]
   {             NULL,              NULL}, //DMT-STD2 2-byte codes, ...
   {             NULL,              NULL}, //DMT-CVT 3-byte codes, ...
   {&SVD_vidfmt_map  , &SVD_vidfmt_map  },
   {&DBC_Tag_map     , &DBC_Tag_map     },
   {&DBC_ExtTag_map  , &DBC_ExtTag_map  },
   {&ADB_AFC_map     , &ADB_AFC_map     },
   {&ADB_ACE_TC_map  , &ADB_ACE_TC_map  },
   {&SPKLD_IDX_map   , &SPKLD_IDX_map   },
   {&T7_AspRatio_map , &T7_AspRatio_map },
   {&T7_3Dsupp_map   , &T7_3Dsupp_map   },
   {&VID_if_type_map , &VID_if_type_map },
   {&VID_depth_map   , &VID_depth_map   },
   {&DID_stereo_map  , &DID_stereo_map  },
   {&DID_product_map , &DID_product_map },
   {&DID2_product_map, &DID2_product_map}
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

   //DMT code maps are built from dmt_table[] on first use
   if ((NULL == vmap) && (idx >= VS_DMT1_VIDFMT) && (idx <= VS_CVT3_VIDFMT)) {
      vmap_InitDMT_Vmaps();
      vmap = psel->map_ar[vm_type];
   }

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
vmap_GetValueAsString(u32_t vs_idx, vmap_type vm_type, u32_t key_val, u32_t flags, wxc_String& sval) {
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

