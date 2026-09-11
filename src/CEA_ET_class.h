/***************************************************************
 * Name:      CEA_ET_class.h
 * Purpose:   EDID::CEA-DBC-EXT descriptor classes
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2021-2025
 * License:   GPLv3+
 **************************************************************/

#ifndef CEA_ET_CLASS_H
#define CEA_ET_CLASS_H 1

#include "EDID_shared.h"

//----------------- DBC Extended Tag Codes

//VCDB: Video Capability Data Block (DBC_ET_VCDB = 0)
class cea_vcdb_cl : public edi_grp_cl {
   private:
      static const char         Desc[];
      static const edi_field_t  fld_dsc[];
      static const gpfld_dsc_t  fields;
      static const dbc_flatgp_dsc_t VCDB_grp;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_vcdb_cl(), flags); };
};
//VSVD: Vendor-Specific Video Data Block (DBC_ET_VSVD = 1)
class cea_vsvd_cl : public edi_grp_cl {
   private:
      static const char         Desc[];
      //fields shared with VSAD
      static const dbc_flatgp_dsc_t VSVD_grp;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_vsvd_cl(), flags); };
};

//VDDD: VESA Display Device Data Block (DBC_ET_VDDD = 2)
class cea_vddd_cl : public dbc_grp_cl {
   private:
      //subgroups;

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_vddd_cl(), flags); };
};
//VDDD: IFP: Interface
class vddd_iface_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

   protected:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};
//VDDD: CPT: Content Protection
class vddd_cprot_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

   protected:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};
//VDDD: AUPR: Audio properties
class vddd_audio_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

   protected:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};
//VDDD: DPR: Display properties
class vddd_disp_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

   protected:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};
//VDDD: CXY: Additional Chromaticity Coordinates
class vddd_cxy_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

   protected:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};

//CLDB: Colorimetry Data Block (DBC_ET_CLDB = 5)
class cea_cldb_cl : public edi_grp_cl {
   private:
      static const char         Desc[];
      static const edi_field_t  fld_dsc[];
      static const gpfld_dsc_t  fields;
      static const dbc_flatgp_dsc_t CLDB_grp;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_cldb_cl(), flags); };
};
//HDRS: HDR Static Metadata Data Block (DBC_ET_HDRS = 6)
class cea_hdrs_cl : public edi_grp_cl {
   private:
      static const char         Desc[];
      static const edi_field_t  fld_byte_2_3_dsc[];
      static const edi_field_t  fld_opt_byte_4_5_6_dsc[];
      static const gpfld_dsc_t  fields[];
      static const dbc_flatgp_dsc_t HDRS_grp;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_hdrs_cl(), flags); };
};

//HDRD: HDR Dynamic Metadata Data Block (DBC_ET_HDRD = 7)
class cea_hdrd_cl : public dbc_grp_cl {
   private:
      static const char         Desc[];
      static const dbc_root_dsc_t HDRD_grp;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_hdrd_cl(), flags); };
};
//HDRD: HMTD: HDR Dynamic Metadata sub-group
class hdrd_mtd_cl : public edi_grp_cl {
   private:
      static const char         Desc   [];
      static const edi_field_t  fld_dsc[];

   public:
      static const dbc_subg_dsc_t MTD_hdr;

      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
    //NOTE: no Clone(): init requires parent->getFreeSubgSZ(), parent is NULL in the copy of group

      static edi_grp_cl* group_new() {return new hdrd_mtd_cl();};
};

//VFPD: Video Format Preference Data Block (DBC_ET_VFPD = 13)
class cea_vfpd_cl : public dbc_grp_cl {
   private:
      static const dbc_root_dsc_t VFPD_grp;

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_vfpd_cl(), flags); };
};
//VFPD: Video Format Preference Data Block ->
//SVR: Short Video Reference
class cea_svr_cl : public edi_grp_cl {
   private:
      static const char         Desc   [];
      static const edi_field_t  fld_dsc[];

   public:
      static const dbc_subg_dsc_t SVR_subg;

      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_svr_cl(), flags); };

      static edi_grp_cl* group_new() {return new cea_svr_cl();};
};
//Y42V: YCBCR 4:2:0 Video Data Block (DBC_ET_Y42V = 14)
class cea_y42v_cl : public dbc_grp_cl {
   private:
      static const char         Desc[];
      static const dbc_root_dsc_t Y42V_grp;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_y42v_cl(), flags); };
};
//Y42C: YCBCR 4:2:0 Capability Map Data Block (DBC_ET_Y42C = 15)
class cea_y42c_cl : public edi_grp_cl {
   private:
      static const edi_field_t bitmap_fld;

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

      //array of strings with SVD nr
      y42c_svdn_t *svdn_ar;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_y42c_cl(), flags); };

       cea_y42c_cl() : svdn_ar(NULL) {};
      ~cea_y42c_cl() { if (svdn_ar != NULL) free(svdn_ar); };
};
//VSAD: Vendor-Specific Audio Data Block (DBC_ET_VSAD = 17)
class cea_vsad_cl : public edi_grp_cl {
   private:

   protected:
      static const char       Desc[];
      //fields shared with VSVD
      static const dbc_flatgp_dsc_t VSAD_grp;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_vsad_cl(), flags); };
};
//HADB: HDMI Audio Data Block (DBC_ET_HADB = 18)
class cea_hadb_cl : public dbc_grp_cl {
   private:
      static const char         Desc   [];
      static const edi_field_t  ah_flds[];

      static const dbc_root_dsc_t HADB_root;
      static const dbc_subg_dsc_t HADB_subg[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_hadb_cl(), flags); };
};
//HADB: HDMI Audio Data Block ->
//SAB3D: HDMI 3D Speaker Allocation Descriptor, based on SAB
class hadb_sab3d_cl : public edi_grp_cl {

   public:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new hadb_sab3d_cl(), flags); };

      static edi_grp_cl* group_new() {return new hadb_sab3d_cl();};
};

//RMCD: Room Configuration Data Block (DBC_ET_RMCD = 19)
class cea_rmcd_cl : public dbc_grp_cl {
   private:
      static const char            Desc   [];
      static const edi_field_t     ah_flds[];


      static const dbc_root_dsc_t RMCD_root;
      static const dbc_subg_dsc_t RMCD_subg[];

   public:

      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_rmcd_cl(), flags); };
};
//RMCD: SPM: Speaker Mask
class rmcd_spm_cl : public edi_grp_cl {
   private:
      //field descriptors and group description are shared with SAB

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);

      static edi_grp_cl* group_new() {return new rmcd_spm_cl();};
};
//RMCD: SPKD: Speaker Distance
class rmcd_spkd_cl : public edi_grp_cl {
   public:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

      static const edi_field_t  fld_dsc[];

      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);

      static edi_grp_cl* group_new() {return new rmcd_spkd_cl();};
};
//RMCD: DSPC: Display Coordinates
class rmcd_dspc_cl : public edi_grp_cl {
   public:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

      static const edi_field_t  fld_dsc[];

      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);

      static edi_grp_cl* group_new() {return new rmcd_dspc_cl();};
};

//SLDB: Speaker Location Data Block (DBC_ET_SLDB = 20)
class cea_sldb_cl : public dbc_grp_cl {
   private:
      static const char         Desc[];
      static const dbc_root_dsc_t SLDB_grp;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_sldb_cl(), flags); };
};
//SLDB: SPKLD: Speaker Location Descriptor
class spkld_cl : public edi_grp_cl {
   private:

   public:
      static const char           Desc   [];
      static const edi_field_t    fld_dsc[];
      static const dbc_subg_dsc_t SPKLD_subg;

      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      //NOTE: no Clone(): init requires parent->getFreeSubgSZ(), parent is NULL in the copy of group

      void   getGrpName(EDID_cl& EDID, wxString& gp_name);

      static edi_grp_cl* group_new() {return new spkld_cl();};
};

//IFDB: InfoFrame Data Block (DBC_ET_IFDB = 32)
class cea_ifdb_cl : public dbc_grp_cl {
   private:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_ifdb_cl(), flags); };
};
//IFDB: IFPD: InfoFrame Processing Descriptor
class ifdb_ifpdh_cl : public edi_grp_cl {
   private:
      static const char         Desc   [];
      static const edi_field_t  fld_dsc[];

   public:
      static const dbc_subg_dsc_t IFPD_subg;

      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};
//IFDB: SIFD: Short InfoFrame Descriptor, InfoFrame Type Code != 0x00, 0x01
class ifdb_sifd_cl : public edi_grp_cl {
   private:
      static const char         Desc[];
      static const edi_field_t  fld_dsc[];
      static const dbc_subg_dsc_t SIFD_subg;

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};
//IFDB: VSIFD: Short Vendor-Specific InfoFrame Descriptor, InfoFrame Type Code = 0x01
class ifdb_vsifd_cl : public edi_grp_cl {
   private:
      static const char         Desc[];
      static const edi_field_t  fld_dsc[];
      static const dbc_subg_dsc_t VSIFD_subg;

   public:
      rcode  init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};

//T7VTDB: DisplayID Type 7 Video Timing Data Block (DBC_ET_T7VTB = 34)
//T7VTDB fields indexes in array
enum {
   T7F_IDX_BLK_REV = 3, //ET hdr offset
   T7F_IDX_DSC_PT,
   T7F_IDX_T7_M,
   T7F_IDX_F37_RES,
   T7F_IDX_ASP_RATIO,
   T7F_IDX_T7_IL,
   T7F_IDX_3D_SUPP,
   T7F_IDX_T7_Y420,

   T7F_IDX_PIXCLK,

   T7F_IDX_HAPIX,
   T7F_IDX_HBPIX,
   T7F_IDX_HSOFFS,
   T7F_IDX_HSWIDTH,
   T7F_IDX_T7_HSP,

   T7F_IDX_VALIN,
   T7F_IDX_VBLIN,
   T7F_IDX_VSOFFS,
   T7F_IDX_VSWIDTH,
   T7F_IDX_T7_VSP
};

class cea_t7vtb_cl : public dbc_grp_cl {
   private:
      static const char         Desc   [];
      static const edi_field_t  fld_dsc[];
      static const gpfld_dsc_t  fields;

      static const dbc_flatgp_dsc_t T7VTB_grp;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_t7vtb_cl(), flags); };

      void        getGrpName(EDID_cl& EDID, wxString& gp_name);
};

//T8VTDB: DisplayID Type 8 Video Timing Data Block (DBC_ET_T8VTB = 35)
class cea_t8vtb_cl : public dbc_grp_cl {
   private:
      static const char           Desc   [];

      static const edi_field_t    ah_flds[];
      static const dbc_root_dsc_t T8VTB_root;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_t8vtb_cl(), flags); };
};
//T8VTDB: ->
//T8VTC: video timing code
class t8vtb_vtc_cl : public edi_grp_cl {

   protected:
      static const char        Desc      [];

      //block data layouts
      static const edi_field_t vtc_1_byte[];
      //for DMT STD 2-byte codes, dmt_std2_cl sub-grp is used

      const dbc_subg_dsc_t       *VTC_subg;

   public:
      static const dbc_subg_dsc_t VTC_T0_subg;
      static const dbc_subg_dsc_t VTC_T1_subg;

      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags);
      void        getGrpName(EDID_cl& EDID, wxString& gp_name);

      static edi_grp_cl* group_new() {return new t8vtb_vtc_cl();};
};
//T10VTDB: DisplayID Type 10 Video Timing Data Block (DBC_ET_T10VTB = 42)
class cea_t10vtb_cl : public dbc_grp_cl {
   private:
      static const char           Desc   [];

      static const edi_field_t    ah_flds[];
      static const dbc_root_dsc_t T10VTB_root;

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_t10vtb_cl(), flags); };
};
//T10VTDB: ->
//T10VTC: video timing code
class t10vtb_vtd_cl : public edi_grp_cl {
      friend class cea_t10vtb_cl; //access to vtc_m1 pointer

   protected:
      static const char        Desc[];

      //block data layouts
      static const edi_field_t vtd_m0_m1[];
      static const edi_field_t vtd_m0   [];
      static const edi_field_t vtd_m1   [];

      const dbc_subg_dsc_t      *VTD_subg;

   public:
      static const dbc_subg_dsc_t VTD_M0_subg;
      static const dbc_subg_dsc_t VTD_M1_subg;

      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags);
      void        getGrpName(EDID_cl& EDID, wxString& gp_name);

      static edi_grp_cl* group_new() {return new t10vtb_vtd_cl();};
};


//UNK-ET: Unknown Data Block (Extended Tag Code)
class cea_unket_cl : public edi_grp_cl {
   private:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_unket_cl(), flags); };
};


#endif /* CEA_ET_CLASS_H */
