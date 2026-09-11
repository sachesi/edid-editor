/***************************************************************
 * Name:      vmap.h
 * Purpose:   value->description maps and value selector menus
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2024-2025
 * License:   GPLv3+
 **************************************************************/

#ifndef VMAP_H
#define VMAP_H 1

#include <map>
#include <wx/menu.h>
#include <wx/string.h>

#include "rcode/rcode.h"
#include "def_types.h"

typedef struct { //vfmt_s
    u32_t       DMT_ID;
    u32_t       STD2;
    u32_t       CVT3;
    const char *name;
} vfmt_t;

//value map : single entry
typedef struct { // vname_map_t
   u32_t        val ;  //item value != item id, field has F_VSVM flag set
   const char  *name;  //value name
   const char  *desc;  //additional description or NULL
} vmap_ent_t;

//vmap type
typedef std::map<u32_t, vmap_ent_t> sm_vmap;

typedef struct {
   union {
         sm_vmap  *map_ar[2];
      struct {
         sm_vmap  *mid_map; //menu id map
         sm_vmap  *val_map; //value map
      };
   };
   wxMenu   *selector;
} vmap_selector_t;

typedef enum {
   VMAP_MID, //value == menu item id
   VMAP_VAL
} vmap_type;

enum vmap_selector_idx {
   VS_NO_SELECTOR,
   VS_START,
   VS_VID_INPUT = VS_START,
   VS_MRL_EXT,
   VS_ALT_DSC_TYPE,
   VS_STD2_ASP_RATIO,
   VS_CVT3_ASP_RATIO,
   VS_CVT3_PREF_VREF,
   VS_DMT1_VIDFMT,
   VS_STD2_VIDFMT,
   VS_CVT3_VIDFMT,
   VS_SVD_VIDFMT,
   VS_CEA_TAG,
   VS_CEA_ETAG,
   VS_ADB_AFC,
   VS_ADB_ACE_TC,
   VS_SPKLD_IDX,
   VS_T7_ASP_RATIO,
   VS_T710_3D_SUPP,
   VS_NUM_OF
};

sm_vmap*          vmap_GetVmap     (u32_t idx, vmap_type vm_type);
const vmap_ent_t* vmap_GetVmapEntry(u32_t vmap_idx, vmap_type vm_type, u32_t key_val);

rcode             vmap_GetValueAsString(u32_t vs_idx, vmap_type vm_type, u32_t key_val, u32_t flags, wxString& sval);

wxMenu*           vmap_GetSelector (u32_t idx, u32_t flags, rcode& retU);
void              vmap_DestroySelectors();

rcode             vmap_InitDMT_Vmaps();
void              vmap_DeleteDMT_Vmaps();

#endif /* VMAP_H */
