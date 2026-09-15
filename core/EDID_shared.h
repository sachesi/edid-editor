/***************************************************************
 * Name:      EDID_shared.h
 * Purpose:   EDID shared declarations
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2021-2025
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_SHARED_H
#define EDID_SHARED_H 1

#include "wxcompat.h"

#include "wxcompat.h"


#include "def_types.h"
#include "id_flags.h"
#include "CEA_ET.h"

#include "rcode/rcode.h"

#include "guilog.h"

#define EDI_FIELD_SZ sizeof(edi_field_t)
enum {
   CEA_DBCHDR_FCNT = 2,
   CEA_ETHDR_FCNT  = 3
};

enum { //handler operating modes
   OP_READ  = 0x01,
   OP_WRSTR = 0x02,
   OP_WRINT = 0x04
};

enum { //EDID blocks
   EDI_BLK_SIZE = 128,
   EDI_BASE_IDX = 0,
   EDI_EXT0_IDX,
   EDI_EXT1_IDX,
   EDI_EXT2_IDX
};

typedef u8_t ediblk_t[EDI_BLK_SIZE];

typedef struct __attribute__ ((packed)) { //edi_s
   edid_t   base;
   ediblk_t ext0;
   ediblk_t ext1;
   ediblk_t ext2;
} edi_t;

typedef union __attribute__ ((packed)) { //edi_buff_u
   u8_t     buff[4* sizeof(edi_t)];
   ediblk_t blk[4];
   edi_t    edi;
} edi_buf_t;

class  EDID_cl;
struct edi_dynfld_s;
//field handler fn ptr
typedef rcode (EDID_cl::*field_fn)(u32_t, wxc_String&, u32_t&, edi_dynfld_s*);

typedef struct edi_field_s {
         field_fn  handlerfn;
         u32_t     vmap_idx; //value selector index, see vmap.h
         u32_t     offs;     //offset in data struct - generic/multi-instance functions
         u32_t     shift;    //offset in bits
         u32_t     fld_sz;   //len in bytes/bits -> depends on flags
         u32_t     flags;
         u32_t     minv;
         u32_t     maxv;
   const char     *name;
   const char     *desc;     //if NULL, use name field string
} edi_field_t;

typedef struct edi_dynfld_s { //__attribute__ ((packed))
   edi_field_t  field;
   u8_t        *base ; //data pointer (instance) - generic handlers
} edi_dynfld_t;

//EDID_cl::getValDesc() modes
typedef enum {
   VD_FULL, //return name + description
   VD_NAME, //name only
   VD_DESC, //description only
} vd_mode;

class edi_grp_cl;
typedef edi_grp_cl* (*psub_ctor)();

//Field for EDID Root group descriptor
typedef struct edi_subg_dsc_s {
   psub_ctor          s_ctor;
   u32_t              inst_cnt; //instance count, (-1) == until data end
} edi_subg_dsc_t;
//EDID Root group descriptor:
typedef struct edi_rootgp_dsc_s {
   const char           *CodN;
   const char           *Name;
   const char           *Desc;
   const char           *PadDsc;
         u32_t           type_id;
         u32_t           grp_offs;
         u32_t           grp_arsz;
   const edi_subg_dsc_t *grp_ar;
} edi_rootgp_dsc_t;

//Field for flat group descriptor
typedef struct gpfld_dsc_s {
         u32_t        flags;
         u32_t        dat_sz;
         i32_t        inst_cnt; //instance count, (-1) == until data end
         u32_t        fcount;
   const edi_field_t *fields;
} gpfld_dsc_t;
//DBC Flat group descriptor:
typedef struct dbc_flatgp_dsc_s {
   const char         *CodN;
   const char         *Name;
   const char         *Desc;
         u32_t         type_id;
         u32_t         flags;
         u32_t         min_len;
         u32_t         max_len;
         u32_t         max_fld;
         u32_t         hdr_fcnt; //CEA_DBCHDR_FCNT | CEA_ETHDR_FCNT
         u32_t         hdr_sz;   //sizeof(bhdr_t) | //sizeof(ethdr_t)
         u32_t         fld_arsz;
   const gpfld_dsc_t  *fld_ar;
} dbc_flatgp_dsc_t;

//Sub group descriptor
typedef struct subgrp_dsc_s {
   psub_ctor          s_ctor;
   const char        *CodN;
   const char        *Name;
   const char        *Desc;
         u32_t        type_id;
         u32_t        min_len;
         u32_t        fcount;   //0: sub-group init generates fields array
         i32_t        inst_cnt; //instance count, (-1) == until data end
   const edi_field_t *fields;
} dbc_subg_dsc_t;
//DBC Root group descriptor
typedef struct dbc_root_dsc_s {
   const char           *CodN;
   const char           *Name;
   const char           *Desc;
         u32_t           type_id;
         u32_t           flags;
         u32_t           min_len;
         u32_t           max_len;
         u32_t           hdr_fcnt; //CEA_DBCHDR_FCNT || CEA_ETHDR_FCNT
         u32_t           hdr_sz;   //sizeof(bhdr_t) || sizeof(ethdr_t)
         u32_t           ahf_sz;   //data size of additional header fields
         u32_t           ahf_cnt;  //additional root grp fields (exluding header)
   const edi_field_t    *ah_flds;  //ahf_cnt == 0, ah_flds == NULL -> DBC hdr only
         u32_t           grp_arsz;
   const dbc_subg_dsc_t *grp_ar;
} dbc_root_dsc_t;

WX_DECLARE_OBJARRAY(edi_dynfld_t*, wxArGrpField);

#include "grpar.h"

class edi_grp_cl : public wxTreeItemData {
   protected:

      u8_t    inst_data[128]; //local copy of one EDID block/group

      u32_t   dat_sz;   //total instance data size
      u32_t   hdr_sz;   //hdr data size in inst_data: SpawnInstance() uses this when number of sub-groups is variable
      u32_t   ahf_sz;   //root group: size used for additional header fields
      u32_t   subg_sz;  //subgroup init: free space left for subgroup
      gtid_t  type_id;
      u32_t   abs_offs;
      u32_t   rel_offs;

      u32_t        grp_idx;    //index within the array
      GroupAr_cl  *grp_ar;     //array containing *this* group
      edi_grp_cl  *parent_grp; //NULL if not a sub-group

      //dynamic field array, depends on data layout
      u32_t        dyn_fcnt;
      edi_field_t *dyn_fldar;

      void        clear_fields();
      edi_grp_cl* base_clone(rcode& rcd, edi_grp_cl* grp, u32_t orflags);

      rcode       base_DBC_Init_FlatGrp(const u8_t* inst, const dbc_flatgp_dsc_t *pgdsc, u32_t orflags, edi_grp_cl* parent);
      rcode       base_DBC_Init_GrpFields(const gpfld_dsc_t *pgfld, edi_field_t **pp_fld, u32_t *pdlen, u32_t *poffs);

      rcode       base_DBC_Init_RootGrp(const u8_t* inst, const dbc_root_dsc_t *pGDsc, u32_t orflags, edi_grp_cl* parent);

                  //IFDB: InfoFrame Data Block: common init fn for all sub-groups
      rcode       IFDB_Init_SubGrp(const u8_t* inst, const dbc_subg_dsc_t* pSGDsc, u32_t orflags, edi_grp_cl* parent);

   public:
      wxArGrpField  FieldsAr;

      wxc_String       CodeName;
      wxc_String       GroupName;
      wxc_String       GroupDesc;

      virtual void   getGrpName(EDID_cl& /*EDID*/, wxc_String& gp_name) {gp_name = GroupName;};

      inline  void   CopyInstData (const u8_t *pinst, u32_t datsz);
      virtual void   SpawnInstance(u8_t *pinst); //copy local data back to EDID buffer
              rcode  AssembleGroup(); //copy sub-group data to parent' group local data buffer

      inline  gtid_t getTypeID    () {return type_id;};
      inline  u32_t  getAbsOffs   () {return abs_offs;};
      inline  u32_t  getRelOffs   () {return rel_offs;};
      virtual u32_t  getDataSize  () {return (hdr_sz == 0) ? dat_sz : (hdr_sz + ahf_sz);};
      inline  u32_t  getHeaderSize() {return hdr_sz;};
      inline  u32_t  getAHF_Size  () {return ahf_sz;};
      virtual u32_t  getTotalSize () {return getDataSize();};
      inline  u8_t*  getInstPtr   () {return inst_data;};
      inline  u32_t  getFreeSubgSZ() {return subg_sz;};

      inline  GroupAr_cl* getParentAr   () {return grp_ar ;};
      inline  u32_t       getParentArIdx() {return grp_idx;};

      inline  void   setTypeID  (gtid_t tid) {type_id  = tid ;};
      inline  void   setAbsOffs (u32_t offs) {abs_offs = offs;};
      inline  void   setRelOffs (u32_t offs) {rel_offs = offs;};
      inline  void   setIndex   (u32_t idx ) {grp_idx  = idx;};
      inline  void   setArray   (GroupAr_cl *p_ar) {grp_ar = p_ar;};
      inline  void   setArrayIdx(GroupAr_cl *p_ar, u32_t idx) {grp_ar = p_ar; grp_idx = idx;};
      virtual void   setParentAr(GroupAr_cl *) {return;};
      virtual void   setDataSize(u32_t dsz) {dat_sz = dsz;};

      virtual rcode  init            (const u8_t* inst, u32_t orflags, edi_grp_cl* parent) =0;
      virtual rcode  ForcedGrpRefresh() {rcode rcd; rcd.value=RCD_OK; return rcd;};
      virtual u32_t  getSubGrpCount  () {return 0;};

      inline  void        setParentGrp(edi_grp_cl *parent) {parent_grp = parent;};
      inline  edi_grp_cl* getParentGrp() {return parent_grp;};
      virtual edi_grp_cl* getSubGroup (u32_t ) {return NULL;};
      virtual GroupAr_cl* getSubGrpAr () {return NULL;};

      virtual void        delete_subg () {return;};

      virtual rcode Append_UNK_DAT(const u8_t* /*inst*/, u32_t /*dlen*/, u32_t /*orflags*/,
                                   u32_t /*abs_offs*/, u32_t /*rel_offs*/,
                                   edi_grp_cl* /*parent_grp*/)
                                  {rcode rcd; rcd.value=RCD_OK; return rcd;};

      virtual edi_grp_cl* Clone(rcode&, u32_t) {return NULL;};



      rcode   init_fields(const edi_field_t* field_ar, const u8_t* inst, u32_t fcount,
                          bool b_append=false,
                          const char *pname=NULL, const char *pdesc=NULL, const char *pcodn=NULL,
                          i32_t offs=0);

      //NOTE: def. subg_id.t32 == T_SUB_GRP: needed by SubGrpAr_cl::Paste()
      edi_grp_cl() : dat_sz(0), hdr_sz(0), ahf_sz(0), subg_sz(0), abs_offs(0), rel_offs(0),
                     grp_idx(0), grp_ar(NULL), parent_grp(NULL), dyn_fcnt(0), dyn_fldar(NULL)
                   { memset(inst_data, 0, sizeof(inst_data));
                     type_id.t32 = ID_INVALID; };

      virtual ~edi_grp_cl() {
         if (dyn_fldar != NULL) { free(dyn_fldar); };
         clear_fields();
         FieldsAr.Clear();
      };
};

//copy EDID buffer data to a local buffer
void edi_grp_cl::CopyInstData(const u8_t *pinst, u32_t datsz) {
   memcpy(inst_data, pinst, datsz);
   dat_sz = datsz;
}

class dbc_grp_cl : public edi_grp_cl {
   protected:
      SubGrpAr_cl  subgroups;



   public:
      void setDataSize(u32_t blklen) {
         bhdr_t *phdr;

         dat_sz  = blklen;
         blklen -= sizeof(bhdr_t); //excluded from DBC block length

         phdr = reinterpret_cast<bhdr_t*> (inst_data);
         phdr->tag.blk_len = blklen;
      };

      u32_t getTotalSize();


      rcode Append_UNK_DAT(const u8_t* inst, u32_t dlen, u32_t orflags,
                           u32_t abs_offs, u32_t rel_offs, edi_grp_cl* parent_grp);

      inline void setParentAr(GroupAr_cl *parent_ar) {
         subgroups.setParentArray(parent_ar);
      };

      inline SubGrpAr_cl* getSubGrpAr   ()          {return &subgroups;};

      inline edi_grp_cl*  getSubGroup   (u32_t idx) {return subgroups.Item(idx);};
      inline u32_t        getSubGrpCount()          {return subgroups.GetCount();};
             void         delete_subg   ();
};

//base EDID: alt. descriptor with sub-groups
class edi_adsc_cl : public dbc_grp_cl {
   protected:

      rcode  base_AltDesc_Init_RootGrp(const u8_t* inst, const edi_rootgp_dsc_t *pGDsc,
                                       u32_t orflags, edi_grp_cl* parent);

   public:

      u32_t  getTotalSize()          {return dat_sz;};
      void   setDataSize (u32_t dsz) {dat_sz =  dsz;};
};


#define __EDID_HDL_ARGS u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field

class EDID_cl {
   private:
      edi_buf_t    EDID_buff;

      guilog_cl   *pGLog;
      wxc_String     tmps;
      wxc_String     tmps2;
      u32_t        num_valid_blocks;

   protected:
      static const wxc_String val_unit_name [];
      static const wxc_String val_type_name [];
      static const wxc_String prop_flag_name[];

      inline u16_t rdWord16_LE(u8_t* pword);
      inline void  wrWord16_LE(u8_t* pword, u16_t v16);

      inline u32_t rdWord24_LE(u8_t* pbt);
      inline void  wrWord24_LE(u8_t* pbt, u32_t v32);

      //Common handlers: helpers
      inline u32_t calcGroupOffs (void *inst) {return (reinterpret_cast <u8_t*> (inst) - EDID_buff.buff);};
      inline u8_t* getInstancePtr(edi_dynfld_t* p_field) {return p_field->base;};
      inline u8_t* getValPtr     (edi_dynfld_t* p_field);

      rcode getStrUint  (wxc_String& sval, int base, u32_t minv, u32_t maxv, ulong& val);
      rcode getStrFloat (wxc_String& sval, float minv, float maxv, float& val);
      rcode rdByteStr   (wxc_String& sval, u8_t* pstrb, u32_t slen);
      rcode rdByteStrLE (wxc_String& sval, u8_t* pstrb, u32_t slen);
      rcode wrByteStr   (wxc_String& sval, u8_t* pstrb, u32_t slen);
      rcode wrByteStrLE (wxc_String& sval, u8_t* pstrb, u32_t slen);

   public:
      enum group_template {
         CEA_AUDIO_LPCM,
         CEA_AUDIO_EXTENDED,
         CEA_VIDEO,
         CEA_TIMING,
         DISPLAYID_DATA,
      };
      wxc_String       gp_name; //global for xxx::getGrpName()

      bool           b_RD_Ignore;
      bool           b_ERR_Ignore;
      bool           b_GrpNameDynamic;

      GroupAr_cl*    BlkGroupsAr[4];
      EDID_GrpAr_cl  EDI_BaseGrpAr;
      CEA_GrpAr_cl   EDI_Ext0GrpAr;
      CEA_GrpAr_cl   EDI_Ext1GrpAr;
      CEA_GrpAr_cl   EDI_Ext2GrpAr;

      inline  edi_buf_t* getEDID() {return &EDID_buff;};
      inline  void       SetGuiLogPtr(guilog_cl *p_glog) {pGLog = p_glog;};
      inline  u32_t      getNumValidBlocks() {return num_valid_blocks;};
      inline  void       ForceNumValidBlocks(u32_t nblk) {num_valid_blocks = (nblk%5);}; //max 4

              u32_t      EDID_Get_num_DTD();
              void       CEA_Set_DTD_Offset(u8_t *pbuf, GroupAr_cl *p_grp_ar);

      u32_t genChksum(u32_t block);
      bool  VerifyChksum(u32_t block);
      void  Clear();

      rcode ParseEDID_Base(u32_t& n_extblk);
      rcode ParseAltDtor  (u8_t *pinst, edi_grp_cl** pp_grp, i32_t offs = -1);
      rcode ParseEDID_CEA (u32_t block = EDI_EXT0_IDX);
      rcode ParseEDID_DisplayID(u32_t block);
      rcode ParseCEA_DBC  (u8_t *pinst, GroupAr_cl& groups);
      static rcode ParseDBC_TAG(u8_t *pinst, edi_grp_cl** pp_grp);
      //Blocks the data declares: the base block count, or the HDMI Forum EDID
      //Extension Override count when the first CTA-861 block starts with one.
      static u32_t DeclaredBlocks(const u8_t* data, size_t size);
      rcode CreateGroup   (group_template which, u8_t displayid_version,
                           edi_grp_cl** pp_grp);
      //After a successful write to field, build a new group from the current
      //data when the write changed the group type or layout. Returns NULL when
      //no rebuild is needed or possible; *target receives the group to replace,
      //which is group itself or, for F_INIT sub-group fields, its parent.
      edi_grp_cl* RebuildGroup(edi_grp_cl* group, edi_dynfld_t* field,
                               bool type_changed, edi_grp_cl** target,
                               rcode& result);
      //Detach target and put replacement at its position. Fails, leaving
      //target in place, when the replacement does not fit in the block.
      static bool ReplaceGroup(edi_grp_cl* target, edi_grp_cl* replacement);
      static void InsertGroupAt(GroupAr_cl* array, u32_t index,
                                edi_grp_cl* group, edi_grp_cl* parent);
      rcode AssembleEDID  ();

      //field properties
      void  getValUnitName     (wxc_String& sval, const u32_t flags);
      void  getValTypeName     (wxc_String& sval, const u32_t flags);
      void  getValFlagsAsString(wxc_String& sval, const u32_t flags);
      void  getValDesc         (wxc_String &sval, edi_dynfld_t* p_field, u32_t val, vd_mode mode=VD_FULL);

      //Common handlers
      rcode FldPadStr (__EDID_HDL_ARGS);
      rcode ByteStr   (__EDID_HDL_ARGS);
      rcode Word16    (__EDID_HDL_ARGS);
      rcode Word24    (__EDID_HDL_ARGS);
      rcode ByteVal   (__EDID_HDL_ARGS);
      rcode BitF8Val  (__EDID_HDL_ARGS);
      rcode BitF16Val (__EDID_HDL_ARGS);
      rcode BitVal    (__EDID_HDL_ARGS);
      rcode Gamma     (__EDID_HDL_ARGS);
      //BED handlers
      rcode MfcId     (__EDID_HDL_ARGS);
      rcode ProdSN    (__EDID_HDL_ARGS);
      rcode ProdWk    (__EDID_HDL_ARGS);
      rcode ProdYr    (__EDID_HDL_ARGS);
      //input type : no dedicated handlers
      //basic display descriptor (old) : no dedicated handlers
      //Supported features
      rcode SPF_vsig  (__EDID_HDL_ARGS);
      //Chromacity coords
      rcode ChrXY_getWriteVal(u32_t op, wxc_String& sval, u32_t& ival);
      rcode CHredX    (__EDID_HDL_ARGS);
      rcode CHredY    (__EDID_HDL_ARGS);
      rcode CHgrnX    (__EDID_HDL_ARGS);
      rcode CHgrnY    (__EDID_HDL_ARGS);
      rcode CHbluX    (__EDID_HDL_ARGS);
      rcode CHbluY    (__EDID_HDL_ARGS);
      rcode CHwhtX    (__EDID_HDL_ARGS);
      rcode CHwhtY    (__EDID_HDL_ARGS);
      //Resolution map : no dedicated handlers
      //STI: Std Timing Descriptor
      rcode STI_Xres  (__EDID_HDL_ARGS);
      rcode STI_Vref  (__EDID_HDL_ARGS);
      void  STI_DBN   (EDID_cl& EDID, wxc_String& dyngp_name, wxArGrpField& FieldsAr);
      //CVT3: VESA-CVT 3-byte Timing Code
      rcode CVT3_Yres (__EDID_HDL_ARGS);
      //Alt Descriptor type handler
      rcode ALT_DType (__EDID_HDL_ARGS);
      //DTD: Detailed Timing Descriptor
      rcode DTD_PixClk(__EDID_HDL_ARGS);
      rcode DTD_HApix (__EDID_HDL_ARGS);
      rcode DTD_HBpix (__EDID_HDL_ARGS);
      rcode DTD_VAlin (__EDID_HDL_ARGS);
      rcode DTD_VBlin (__EDID_HDL_ARGS);
      rcode DTD_HOsync(__EDID_HDL_ARGS);
      rcode DTD_VOsync(__EDID_HDL_ARGS);
      rcode DTD_VsyncW(__EDID_HDL_ARGS);
      rcode DTD_HsyncW(__EDID_HDL_ARGS);
      rcode DTD_Hsize (__EDID_HDL_ARGS);
      rcode DTD_Vsize (__EDID_HDL_ARGS);
      //MRL handlers
      rcode MRL_Freq     (__EDID_HDL_ARGS);
      rcode MRL_02_GTFM  (__EDID_HDL_ARGS);
      rcode MRL_MaxPixClk(__EDID_HDL_ARGS);
      rcode MRL_04_PixClk(__EDID_HDL_ARGS);
      rcode MRL_04_HAlin (__EDID_HDL_ARGS);
      //WPD handlers
      rcode WPD_pad      (__EDID_HDL_ARGS);
      //DCM: Display Color Management Data
      rcode DCM_coef16   (__EDID_HDL_ARGS);
      //CEA:DBC Block Header
      rcode CEA_DBC_Len  (__EDID_HDL_ARGS);
      rcode CEA_DBC_Tag  (__EDID_HDL_ARGS);
      rcode CEA_DBC_ExTag(__EDID_HDL_ARGS);
      //CEA:VDB
      u32_t CEA_VDB_SVD_decode(u32_t vic, u32_t &native);
      rcode SVD_VIC      (__EDID_HDL_ARGS);
      rcode SVD_Native   (__EDID_HDL_ARGS);
      //CEA:ADB:SAD
      rcode SAD_LPCM_MC  (__EDID_HDL_ARGS);
      rcode SAD_BitRate  (__EDID_HDL_ARGS);
      //CEA:VSD
      rcode VSD_ltncy    (__EDID_HDL_ARGS);
      rcode VSD_MaxTMDS  (__EDID_HDL_ARGS);
      rcode HF_VRRmax    (__EDID_HDL_ARGS);
      //CEA:HDRS, AMD VSD: luminance codes
      rcode Luminance    (__EDID_HDL_ARGS);
      rcode HDRS_MinLum  (__EDID_HDL_ARGS);
      rcode AMD_MinLum   (__EDID_HDL_ARGS);
      rcode MinLuminance (u32_t op, wxc_String& sval, u32_t& ival, u8_t* inst, u32_t max_code);
      //CEA-ET: VDDD
      rcode VDDD_IF_MaxF   (__EDID_HDL_ARGS);
      rcode VDDD_HVpix_cnt (__EDID_HDL_ARGS);
      rcode VDDD_AspRatio  (__EDID_HDL_ARGS);
      rcode VDDD_HVpx_pitch(__EDID_HDL_ARGS);
      rcode VDDD_AudioDelay(__EDID_HDL_ARGS);
      //CEA-ET: RMCD, SLDB: normalized distances
      rcode RMCD_NormV(__EDID_HDL_ARGS);
      //CEA-ET: VFDB
      u32_t CEA_VFPD_SVR_decode(u32_t svr, u32_t &ndtd);
      //CEA-ET: T7VTDB
      rcode T7VTB_PixClk  (__EDID_HDL_ARGS);
      rcode DisplayID_PixelClock(__EDID_HDL_ARGS);
      rcode DisplayID_ValuePlusOne16(__EDID_HDL_ARGS);
      rcode DisplayID_ValuePlusOne15(__EDID_HDL_ARGS);
      rcode DisplayID_PixelClockKHz(__EDID_HDL_ARGS);
      rcode DisplayID_MaxRefresh(__EDID_HDL_ARGS);

      EDID_cl() : num_valid_blocks(0), b_RD_Ignore(false),
                  b_ERR_Ignore(false), b_GrpNameDynamic(true)
      {
         BlkGroupsAr[0] = &EDI_BaseGrpAr;
         BlkGroupsAr[1] = &EDI_Ext0GrpAr;
         BlkGroupsAr[2] = &EDI_Ext1GrpAr;
         BlkGroupsAr[3] = &EDI_Ext2GrpAr;
      };

      ~EDID_cl() {
         Clear();
      };
};
#undef __EDID_HDL_ARGS

// rdWordXX_LE(), wrWordXX_LE:
// The functions are used independently of platform endianness,
// allowing to operate unaligned pointers.
u16_t EDID_cl::rdWord16_LE(u8_t* pbt) {
   u16_t  v16;
   u16_t  u8v;

   u8v   = *pbt; //L-byte
   v16   = u8v;
   pbt  ++ ;
   u8v   = *pbt; //H-byte
   u8v <<= 8;
   v16  |= u8v;

   return v16;
};

void EDID_cl::wrWord16_LE(u8_t* pbt, u16_t v16) {

   *pbt   = v16;
    pbt  ++ ;
    v16 >>= 8;
   *pbt   = v16;
};

u32_t EDID_cl::rdWord24_LE(u8_t* pbt) {
   u32_t  v32;

   v32   = pbt[2];
   v32 <<= 8;
   v32  |= pbt[1];
   v32 <<= 8;
   v32  |= pbt[0];

   return v32;
};

void EDID_cl::wrWord24_LE(u8_t* pbt, u32_t v32) {

   *pbt   = v32;

    pbt  ++ ;
    v32 >>= 8;
   *pbt   = v32;

    pbt  ++ ;
    v32 >>= 8;
   *pbt   = v32;
};


u8_t* EDID_cl::getValPtr(edi_dynfld_t* p_field) {
   u8_t*  ptr;

   ptr  = getInstancePtr(p_field);
   ptr += p_field->field.offs;
   return ptr;
};

//STI: Std Timing Information Descriptor
class dmt_std2_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new dmt_std2_cl(), flags); };
      void        getGrpName(EDID_cl& EDID, wxc_String& gp_name);

      static edi_grp_cl* group_new() {return new dmt_std2_cl();};
};
//CVT3 VESA-CVT 3-byte Timing Code
class dmt_cvt3_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new dmt_cvt3_cl(), flags); };
      void        getGrpName(EDID_cl& EDID, wxc_String& gp_name);

      static edi_grp_cl* group_new() {return new dmt_cvt3_cl();};
};

//UNK-DAT Unknown Data bytes, subgroup.
class cea_unkdat_cl : public edi_grp_cl {
   private:
      static const char  CodN[];
      static const char  Name[];
      static const char  Desc[];

   public:
      rcode       init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {return base_clone(rcd, new cea_unkdat_cl(), flags); };
};

#endif /* EDID_SHARED_H */
