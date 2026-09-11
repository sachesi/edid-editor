/***************************************************************
 * Name:      EDID_class.h
 * Purpose:   EDID classes and field handlers
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2014-2025
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_CLASS_H
#define EDID_CLASS_H 1

#include <endian.h>
#if __BYTE_ORDER == __BIG_ENDIAN
   #define _BE_SWAP16(w16) {(w16) = __bswap_16(w16);}
   #define _BE_SWAP32(w32) {(w32) = __bswap_32(w32);}
   #define _LE_SWAP16(w16)
#else
   #define _BE_SWAP16(w16)
   #define _BE_SWAP32(w32)
   #define _LE_SWAP16(w16) {(w16) = __bswap_16(w16);}
#endif

#include "EDID_shared.h"

//----------------- EDID base block

//BED: Base EDID data
class edibase_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      void   SpawnInstance(u8_t *pinst);
      void   getGrpName(EDID_cl& EDID, wxc_String& gp_name);
};
//VID: Video Input Descriptor
class vindsc_cl : public edi_grp_cl {
   private:
      enum {
         in_analog_fcnt  = 7,
         in_digital_fcnt = 4,
         max_fcnt        = in_analog_fcnt //for pre-allocating array of fields
      };

      //block data layouts
      static const edi_field_t in_analog[];
      static const edi_field_t in_digital[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

      rcode  gen_data_layout(const u8_t* inst);

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      rcode  ForcedGrpRefresh();
      void   getGrpName(EDID_cl& EDID, wxc_String& gp_name);
};
//BDD: basic display descriptior
class bddcs_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      void   getGrpName(EDID_cl& EDID, wxc_String& gp_name);
};
//SPF: Supported features
class spft_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};
//Chromacity coords
class chromxy_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};
//ETM: Estabilished Timings Map
class resmap_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};
//STI: Std Timing Information Descriptor: EDID_shared.h
//DTD: fields indexes
enum {
   DTD_IDX_PIXCLK = 0,
   DTD_IDX_HAPIX,
   DTD_IDX_HBPIX,
   DTD_IDX_VALIN,
   DTD_IDX_VBLIN,
   DTD_IDX_HSOFFS,
   DTD_IDX_HSWIDTH,
   DTD_IDX_VSOFFS,
   DTD_IDX_VSWIDTH,
   DTD_IDX_HSIZE,
   DTD_IDX_VSIZE,
   DTD_IDX_HBORD,
   DTD_IDX_VBORD,
   DTD_IDX_IL2W,
   DTD_IDX_HSTYPE,
   DTD_IDX_VSTYPE,
   DTD_IDX_SYNCTYP,
   DTD_IDX_STEREOMD,
   DTD_IDX_ILACE
};
//DTD : detailed timing descriptor
class dtd_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new dtd_cl(), flags); };
      void        getGrpName(EDID_cl& EDID, wxc_String& gp_name);
};

//Common class for text descriptors
//MSN: Monitor Serial Number Descriptor (type 0xFF)
//UTX: UnSpecified Text (type 0xFE)
//MND: Monitor Name Descriptor (type 0xFC)
class txtd_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN_MSN[];
      static const char  Name_MSN[];
      static const char  CodN_UTX[];
      static const char  Name_UTX[];
      static const char  CodN_MND[];
      static const char  Name_MND[];

      static const char *Desc;

      static const char  fname_msn[];
      static const char  fname_utx[];
      static const char  fname_mnd[];
      static const char  dsc_msn  [];
      static const char  dsc_utx  [];
      static const char  dsc_mnd  [];

      rcode StrValid();

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new txtd_cl(), flags); };
      void        getGrpName(EDID_cl& EDID, wxc_String& gp_name);
};

//MRL: Monitor Range Limits Descriptor (type 0xFD)
class mrl_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];
      static const edi_field_t fields_GTF[];
      static const edi_field_t fields_CVT[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

      rcode  gen_data_layout(const u8_t* inst);

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new mrl_cl(), flags); };
      rcode       ForcedGrpRefresh();
};
//WPD: White Point Descriptor (type 0xFB)
class wpt_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new wpt_cl(), flags); };
};
//AST: Additional Standard Timing Identifiers (type 0xFA)
class ast_cl : public edi_adsc_cl {
   private:
      static const char Desc[];

      static const edi_rootgp_dsc_t AST_dsc;
      static const edi_subg_dsc_t   STI_grp[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new ast_cl(), flags); };
};
//DCM: Display Color Management Data (type 0xF9)
class dcm_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN  [];
      static const char  Name  [];
      static const char  Desc  [];
      static const char  VerDsc[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new dcm_cl(), flags); };
};
//CT3: VESA-CVT 3-byte Video Timing Codes (type 0xF8)
class ct3_cl : public edi_adsc_cl {
   private:
      static const edi_rootgp_dsc_t CT3_dsc;
      static const edi_subg_dsc_t   CVT3_grp[];

      static const char Desc  [];
      static const char VerDsc[]; //version field desc

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new ct3_cl(), flags); };
};
//ET3: ET3: Estabilished Timings 3 Descriptor (type 0xF7)
class et3_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN  [];
      static const char  Name  [];
      static const char  Desc  [];
      static const char  VerDsc[];
      static const char  flddsc[];
      static const char  resvd [];

      rcode gen_data_fields();

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new et3_cl(), flags); };
};
//UNK: Unknown Descriptor (type != 0xFA-0xFF)
class unk_cl : public edi_grp_cl {
   private:

      static const char  CodN_UNK [];
      static const char  Name_UNK [];
      static const char  CodN_VOID[];
      static const char  Name_VOID[];
      static const char *Desc;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new unk_cl(), flags); };
};


#endif /* EDID_CLASS_H */
