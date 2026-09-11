/***************************************************************
 * Name:      id_flag.h
 * Purpose:   Global block/groups IDs and field flags
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2024-2025
 * License:   GPLv3+
 **************************************************************/

#ifndef ID_FLAGS_H
#define ID_FLAGS_H 1

enum { //block and groups IDs & flags
   ID_INVALID     = 0x00000000,

   //EDID base:
   ID_BED         = 0x00000001,
   ID_VID         = 0x00000002,
   ID_BDD         = 0x00000003,
   ID_SPF         = 0x00000004,
   ID_CXY         = 0x00000005,
   ID_ETM         = 0x00000006,
   ID_DMT2        = 0x00010000,
   ID_CVT3        = 0x00020000,
   //EDID base: descriptors
   ID_UNK         = 0x00000007,
   ID_MRL         = 0x00000008,
   ID_MSN         = 0x00000009,
   ID_MND         = 0x0000000A,
   ID_WPD         = 0x0000000B,
   ID_AST         = 0x0000000C,
   ID_UTX         = 0x0000000D,
   ID_DCM         = 0x0000000E,
   ID_CT3         = 0x0000000F,
   ID_ET3         = 0x00000010,
   ID_DTD         = 0x00000011, //last ID: can be inserted into CEA
   ID_EDID_MASK   = 0x0000001F, //base EDID type mask
   //CEA-DBC:
   ID_CHD         = 0x00000020,
   ID_ADB         = 0x00000040,
   ID_SAD         = 0x00030000,
   ID_VDB         = 0x00000060,
   ID_SVD         = 0x00040000,
   ID_VSD         = 0x00000080,
   ID_SAB         = 0x000000A0,
   ID_VDTC        = 0x000000C0,
   ID_CEA_UTC     = 0x000000E0, //unknown Tag Code
   ID_CEA_MASK    = 0x000000E0, //mask for Tag Code type ID
   //CEA-DBC-ET:
   ID_VCDB        = 0x00000100,
   ID_VSVD        = 0x00000200,
   ID_VSAD        = 0x00000300,
   ID_HADB        = 0x00000400,
   ID_SAB3D       = 0x00050000,
   ID_VDDD        = 0x00000500,
   ID_VDDD_IPF    = 0x00060000,
   ID_VDDD_CPT    = 0x00070000,
   ID_VDDD_AUD    = 0x00080000,
   ID_VDDD_DPR    = 0x00090000,
   ID_VDDD_CXY    = 0x000A0000,
   ID_RSV4        = 0x00000600,
   ID_CLDB        = 0x00000700,
   ID_HDRS        = 0x00000800,
   ID_HDRD        = 0x00000900,
   ID_HDRD_HMTD   = 0x000B0000,
   ID_VFPD        = 0x00000A00,
   ID_VFPD_SVR    = 0x000C0000,
   ID_Y42V        = 0x00000B00,
   ID_Y42C        = 0x00000C00,
   ID_RMCD        = 0x00000D00,
   ID_RMCD_SPM    = 0x000D0000,
   ID_RMCD_SPKD   = 0x000E0000,
   ID_RMCD_DSPC   = 0x000F0000,
   ID_SLDB        = 0x00000E00,
   ID_SLDB_SPKLD  = 0x00100000,
   ID_IFDB        = 0x00000F00,
   ID_IFDB_IFPD   = 0x00110000,
   ID_IFDB_SIFD   = 0x00120000,
   ID_IFDB_VSD    = 0x00130000,
   ID_T7VTDB      = 0x00001000,
   ID_T8VTDB      = 0x00001100,
   ID_T8VTC_T0    = 0x00140000, //TSC=0, 1-byte VTC
/* ID_T8VTC_T1    = 0x00010000, TSC=1: ID_DMT2 */
   ID_T10VTDB     = 0x00001200,
   ID_T10VTD_M0   = 0x00150000, //T10_M=0, 6-byte VTD
   ID_T10VTD_M1   = 0x00160000, //T10_M=1, (6+1)-byte VTD
   ID_CEA_UETC    = 0x0000FF00, //unknown Extended Tag Code
   ID_CEA_UDAT    = 0x007F0000, //unknown data sub-group
   ID_CEA_ET_MASK = 0x0000FF00, //mask for Extended Tag type ID

   ID_SUBGRP_MASK = 0x007F0000, //mask for subgroups
   ID_PARENT_MASK = (ID_EDID_MASK|ID_CEA_MASK|ID_CEA_ET_MASK), //mask for parent groups
   //Group init flags:
   T_FLEX_LAYOUT  = 0x00800000, // use field data lengths
   T_FLEX_LEN     = 0x01000000, // !T_FLEX_LEN -> fail if group length is too small
   T_FLEX_OFFS    = 0x02000000, // update field's offsets
   T_FLAG_MASK    = 0x03800000,
   //Group type:
   T_MODE_EDIT    = 0x04000000, //alternative behaviour in <re>init()
   T_NO_COPY      = 0x08000000, //can't copy: sub-group needs data from parent group
   T_NO_MOVE      = 0x10000000, //can't be moved
   T_P_HOLDER     = 0x20000000, //place holder grp: can be moved/copied/pasted but not cut or deleted
   T_GRP_FIXED    = 0x40000000, //fixed group: can't be moved, copied, pasted, cut or deleted.
   T_SUB_GRP      = 0x80000000, //sub-group

   T_TYPE_MASK    = 0xFC000000
};

typedef union __attribute__ ((__transparent_union__)) gtid_u {
   u32_t  t32;
   struct __attribute__ ((packed)) {
      uint base_id       :5;
      uint cea_id        :3;

      uint cea_et_id     :8;

      uint subg_id       :7;
      uint t_flex_layout :1;

      uint t_flex_len    :1;
      uint t_flex_offs   :1;
      uint t_md_edit     :1;
      uint t_no_copy     :1;

      uint t_no_move     :1;
      uint t_p_holder    :1;
      uint t_gp_fixed    :1;
      uint t_sub_gp      :1;
   };
} gtid_t;

enum { //EDID field flags
   //byte0: property flags
   F_PR_CNT = 8,
   F_PR_SFT = 0,
   F_PR_MSK = 0xFFF,
   F_RD     = 0x00000001, //read-only
   F_NU     = 0x00000002, //field not used i.e. it is marked as unused in the EDID
   F_GD     = 0x00000004, //display group description for this field
   F_DN     = 0x00000008, //dynamic block name: created from field value
   F_VS     = 0x00000010, //value selector menu available
   F_FR     = 0x00000020, //force group refresh after changing a field with this flag set
   F_INIT   = 0x00000040, //group requires re-initialization on field change
   F_RS     = 0x00000080, //field value can't be provided as int (for write)
   F_NI     = 0x00000100, //field value can't be provided as int (for write)
   //byte1: field type
   F_TP_CNT = 8,
   F_TP_SFT = 12,
   F_TP_MSK = 0xFF,
   F_BIT    = 0x00001000, //single bit
   F_BFD    = 0x00002000, //bitfield
   F_BTE    = 0x00004000, //byte
   F_INT    = 0x00008000, //int
   F_FLT    = 0x00010000, //float
   F_HEX    = 0x00020000, //display as hex
   F_STR    = 0x00040000, //text/byte string
   F_LE     = 0x00080000, //Little Endian byte order : reverse the byte order in byte strings
   //byte2, byte3.bit0: val unit
   F_UN_CNT = 9,
   F_UN_SFT = 20,
   F_UN_MSK = 0x1FF,
   F_PIX    = 0x00100000, //pixels
   F_MM     = 0x00200000, //
   F_CM     = 0x00400000, //
   F_DM     = 0x00800000, //decimeters
   F_HZ     = 0x01000000, //
   F_KHZ    = 0x02000000, //
   F_MHZ    = 0x04000000, //
   F_MLS    = 0x08000000, //milliseconds
   F_PCT    = 0x10000000, //percent
   //byte3.bit1..7: other flags
   F_VSVM   = 0x20000000  //value selector: value stored in vmap_ent_t, not in item id
};

#endif /* ID_FLAGS_H */
