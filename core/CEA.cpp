/***************************************************************
 * Name:      CEA.cpp
 * Purpose:   EDID CEA-861 extension
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2014-2025
 * License:   GPLv3+
 **************************************************************/

#include "debug.h"
#include "rcdunits.h"
#ifndef idCEA
   #error "CEA.cpp: missing unit ID"
#endif
#define RCD_UNIT idCEA
#include "rcode/rcode.h"

#include "wxedid_rcd_scope.h"

RCD_AUTOGEN_DEFINE_UNIT

#include <stddef.h>
#include <cmath>

#include "CEA.h"
#include "CEA_class.h"
#include "vmap.h"

//VDB: Video Data Block : video modes
extern sm_vmap SVD_vidfmt_map;

//CHD: CEA Header
const char  cea_hdr_cl::CodN[] = "CHD";
const char  cea_hdr_cl::Name[] = "CEA-861 header";
const char  cea_hdr_cl::Desc[] = "CEA/CTA-861 header";

const edi_field_t cea_hdr_cl::fields[] = {
   {&EDID_cl::ByteVal, 0, offsetof(cea_hdr_t, ext_tag), 0, 1, F_BTE|F_HEX|F_RD, 0, 0xFF, "Extension tag",
   "Extension type: 0x02 for CEA-861" },
   {&EDID_cl::ByteVal, 0, offsetof(cea_hdr_t, rev), 0, 1, F_BTE|F_INT|F_RD, 0, 0xFF, "Revision",
   "Extension revision" },
   {&EDID_cl::ByteVal, 0, offsetof(cea_hdr_t, dtd_offs), 0, 1, F_BTE|F_HEX|F_RD, 0, 0xFF, "DTD offset",
   "Offset to first DTD:\n"
   "dtd_offs = 0 -> No DBC data and no DTD sections\n"
   "dtd_offs = 4 -> No DBC data, DTD sections available\n"
   "dtd_offs > 4 -> Both DBC data and DTD sections available\n"
    },
   {&EDID_cl::BitF8Val, 0, offsetof(cea_hdr_t, info_blk), 0, 4, F_BFD|F_INT|F_RD, 0, 0x0F,
   "num_dtd",
   "Sum of *native* DTDs included in block 0 and in CEA extension.\n\n"
   "num_dtd==0 means that native format can't be expressed in 18-byte DTD and in such case "
   "first VDB.SVD should be considered as native/preferred video format\n\n"
   "NOTE#1: there can be additional non-native DTDs present at the end of the CTA-861 block.\n"
   "NOTE#2: CTA-861 rev.1 the 3rd byte in header is reserved (0x00), what implies that 1st DTD "
   "in block 0 describes preferred video format."
    },
   {&EDID_cl::BitVal, 0, offsetof(cea_hdr_t, info_blk), 4, 1, F_BIT, 0, 1, "YCbCr 4:2:2",
   "1 = supported." },
   {&EDID_cl::BitVal, 0, offsetof(cea_hdr_t, info_blk), 5, 1, F_BIT, 0, 1, "YCbCr 4:4:4",
   "1 = supported." },
   {&EDID_cl::BitVal, 0, offsetof(cea_hdr_t, info_blk), 6, 1, F_BIT, 0, 1, "Basic Audio",
   "1 = supports basic audio." },
   {&EDID_cl::BitVal, 0, offsetof(cea_hdr_t, info_blk), 7, 1, F_BIT, 0, 1, "Underscan",
   "1 = supports underscan." },
   {&EDID_cl::ByteVal, 0, offsetof(cea_hdr_t, info_blk)+1, 0, 1, F_BTE|F_HEX|F_RD, 0, 0xFF, "checksum",
   "Block checksum:\n [sum_of_bytes(0-127) mod 256] + checkum_field_val must be equal to zero.\n\n"
   "NOTE: this byte is physically located at the end of the extension block (offset 127)"}
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode cea_hdr_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   static const u32_t fcount = 9;

   rcode  retU;
   u32_t  ext_tag;
   u32_t  rev;

   ext_tag = reinterpret_cast <const cea_hdr_t*> (inst)->ext_tag;
   rev     = reinterpret_cast <const cea_hdr_t*> (inst)->rev;

   if (ext_tag != 0x02) { //CEA tag
      wxedid_RCD_RETURN_FAULT_VMSG(retU, "[E!] CEA/CTA-861: unknown header tag=0x%02X", ext_tag);
   }

   if ((rev > 3) || (rev < 1)) { //CEA revision
      wxedid_RCD_RETURN_FAULT_VMSG(retU, "[E!] CEA/CTA-861: unknown revision=%u", rev);
   }

   parent_grp  = parent;
   type_id.t32 = ID_CHD | T_GRP_FIXED;

   CopyInstData(inst, sizeof(cea_hdr_t));
   hdr_sz              = dat_sz;
   inst_data[dat_sz++] = inst[127]; //chksum

   retU = init_fields(&fields[0], inst_data, fcount, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

void cea_hdr_cl::SpawnInstance(u8_t *pinst) {
   u32_t   dsz;

   dsz    = dat_sz;
   dsz   -= 1; //chksum

   memcpy(pinst, inst_data, dsz);

   pinst[127] = inst_data[dsz];
}

//CEA bad block length msg
extern const char ERR_GRP_LEN_MSG[];
       const char ERR_GRP_LEN_MSG[] = "[E!] %s: invalid block length";

//CEA header field descriptions
extern const char CEA_BlkHdr_dscF0[];
extern const char CEA_BlkHdr_dscF1[];
extern const char CEA_BlkHdr_dscF2[];
extern const edi_field_t CEA_BlkHdr_fields[];

extern const edi_field_t unknown_byte_fld;
extern void insert_unk_byte(edi_field_t *p_fld, u32_t len, u32_t s_offs);

const char CEA_BlkHdr_dscF0[] =
"Total number of bytes in the block excluding 1st header byte.\n"
"If Block Tag_Code = 7 then the header takes additional byte for Extended Tag Code value.\n"
"The 2nd header ic counted as 1st payload byte.";
const char CEA_BlkHdr_dscF1[] =
"Block Type, Tag Code: \n"
"0: reserved\n"
"1: ADB: Audio Data Block\n"
"2: VDB: Video Data Block\n"
"3: VSD: Vendor Specific Data Block\n"
"4: SAB: Speaker Allocation Data Block\n"
"5: VDT: VESA Display Transfer Characteristic Data Block\n"
"6: reserved\n"
"7: EXT: Use Extended Tag\n";
const char CEA_BlkHdr_dscF2[] =
"Extended Tag Codes:\n"
"00: VCDB: Video Capability Data Block\n"
"01: VSVD: Vendor-Specific Video Data Block\n"
"02: VDDD: VESA Display Device Data Block\n"
"03: ! Reserved for VESA Video Data Block\n"
"04: ! Reserved for HDMI Video Data Block\n"
"05: CLDB: Colorimetry Data Block\n"
"06: HDRS: HDR Static Metadata Data Block\n"
"07: HDRD: HDR Dynamic Metadata Data Block\n"
"08..12 ! Reserved for video-related blocks\n"
"13: VFPD: Video Format Preference Data Block\n"
"14: Y42V: YCBCR 4:2:0 Video Data Block\n"
"15: Y42C: YCBCR 4:2:0 Capability Map Data Block\n"
"16: ! Reserved for CTA Miscellaneous Audio Fields\n"
"17: VSAD: Vendor-Specific Audio Data Block\n"
"18: HADB: HDMI Audio Data Block\n"
"19: RMCD: Room Configuration Data Block\n"
"20: SLDB: Speaker Location Data Block\n"
"21..31 ! Reserved for audio-related blocks\n"
"32: IFDB: InfoFrame Data Block\n"
"33: ! Reserved\n"
"34: T7VTB: DisplayID Type 7 Video Timing Data Block\n"
"34: T8VTB: DisplayID Type 8 Video Timing Data Block\n"
"36..41 ! Reserved\n"
"42: T10VTB: DisplayID Type 10 Video Timing Data Block\n"
"43..119 ! Reserved\n"
"120: HEOVR: HDMI Forum EDID Extension Override Data Block\n"
"121: HSCDB: HDMI Forum Sink Capability Data Block\n"
"122..127 ! Reserved for HDMI\n"
"128..255 ! Reserved\n";

//CEA-DBC Tag Code selector
sm_vmap DBC_Tag_map = {
  //0: ! Reserved
   {1, {0, "ADB: Audio Data Block", NULL}},
   {2, {0, "VDB: Video Data Block", NULL}},
   {3, {0, "VSD: Vendor Specific Data Block"   , NULL}},
   {4, {0, "SAB: Speaker Allocation Data Block", NULL}},
   {5, {0, "VDTC: VESA Display Transfer Characteristic Data Block", NULL}},
  //6: ! Reserved
   {7, {0, "EXT: Extended Tag Code", NULL}},
};

//CEA-DBC Extended Tag Code selector
sm_vmap DBC_ExtTag_map = {
   {  0, {0, "VCDB: Video Capability Data Block"     , NULL}},
   {  1, {0, "VSVD: Vendor-Specific Video Data Block", NULL}},
   {  2, {0, "VDDD: VESA Display Device Data Block"  , NULL}},
 //   3  ! Reserved for VESA Video Data Block
 //   4  ! Reserved for HDMI Video Data Block
   {  5, {0, "CLDB: Colorimetry Data Block"         , NULL}},
   {  6, {0, "HDRS: HDR Static Metadata Data Block" , NULL}},
   {  7, {0, "HDRD: HDR Dynamic Metadata Data Block", NULL}},
 //   8..12 ! Reserved for video-related blocks
   { 13, {0, "VFPD: Video Format Preference Data Block"   , NULL}},
   { 14, {0, "Y42V: YCBCR 4:2:0 Video Data Block"         , NULL}},
   { 15, {0, "Y42C: YCBCR 4:2:0 Capability Map Data Block", NULL}},
 //  16  ! Reserved for CTA Miscellaneous Audio Fields
   { 17, {0, "VSAD: Vendor-Specific Audio Data Block", NULL}},
   { 18, {0, "HADB: HDMI Audio Data Block"           , NULL}},
   { 19, {0, "RMCD: Room Configuration Data Block"   , NULL}},
   { 20, {0, "SLDB: Speaker Location Data Block"     , NULL}},
 //  21..31 ! Reserved for audio-related blocks
   { 32, {0, "IFDB: InfoFrame Data Block", NULL}},
 //  33  ! Reserved
   { 34, {0, "T7VTB: DisplayID Type 7 Video Timing Data Block" , NULL}},
   { 35, {0, "T8VTB: DisplayID Type 8 Video Timing Data Block", NULL}},
 //  36..41 ! Reserved
   { 42, {0, "T10VTB: DisplayID Type 10 Video Timing Data Block", NULL}},
 //  43..119 ! Reserved
   {120, {0, "HEOVR: HDMI Forum EDID Extension Override Data Block", NULL}},
   {121, {0, "HSCDB: HDMI Forum Sink Capability Data Block", NULL}},
 // 122..127 ! Reserved for HDMI
 // 128..255 ! Reserved
};


//CEA-DBC Header fields:
const edi_field_t CEA_BlkHdr_fields[] = {
   {&EDID_cl::CEA_DBC_Len, 0, 0, 0, 5, F_BFD|F_INT|F_RD|F_FR, 0, 0x1F, "Blk length",
    CEA_BlkHdr_dscF0 },
   {&EDID_cl::CEA_DBC_Tag, VS_CEA_TAG, 0, 5, 3, F_BFD|F_INT|F_RD|F_FR|F_VS, 0, 0x07, "Tag Code",
    CEA_BlkHdr_dscF1 },
   //Extended Tag Code field is included conditionally, depending on Tag Code
   {&EDID_cl::CEA_DBC_ExTag, VS_CEA_ETAG, 1, 0, 1, F_BTE|F_INT|F_RD|F_FR|F_VS, 0, 0xFF, "Ext Tag Code",
    CEA_BlkHdr_dscF2 }
};

//unknown/invalid byte: (offset has to be modified accordingly)
const edi_field_t unknown_byte_fld =
   {&EDID_cl::ByteVal, 0, 0, 0, 1, F_BTE|F_HEX|F_RD, 0, 0xFF, "byte", "data byte" };

rcode EDID_cl::CEA_DBC_Tag(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;

   if (op == OP_READ) {
      retU = BitF8Val(op, sval, ival, p_field);

   } else {
      u32_t   blkTag;
      bhdr_t *phdr;

      phdr   = reinterpret_cast<bhdr_t*> ( getValPtr(p_field) );
      blkTag = phdr->tag.tag_code;
      retU   = BitF8Val(op, sval, ival, p_field);

      if (RCD_IS_OK(retU)) {
         if (blkTag != (u32_t) phdr->tag.tag_code) RCD_RETURN_TRUE(retU);
         //True means block type changed -> the BlockTree needs to be updated.
      }
   }
   return retU;
}

rcode EDID_cl::CEA_DBC_ExTag(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   if (op == OP_READ) {
      retU = ByteVal(op, sval, ival, p_field);
   } else {
      u8_t   blkExTag;
      u8_t  *ptag;

      ptag     = getValPtr(p_field);
      blkExTag = *ptag;
      retU     = ByteVal(op, sval, ival, p_field);

      if (RCD_IS_OK(retU)) {
         if (blkExTag != *ptag) RCD_RETURN_TRUE(retU);
      }
   }
   return retU;
}

rcode EDID_cl::CEA_DBC_Len(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   bhdr_t *phdr;

   phdr = reinterpret_cast<bhdr_t*> ( getValPtr(p_field) );

   if (op == OP_READ) {
      retU = BitF8Val(op, sval, ival, p_field);
   } else {
      u32_t  blklen;

      blklen = phdr->tag.blk_len;
      retU   = BitF8Val(op, sval, ival, p_field);

      if (RCD_IS_OK(retU)) {
         if (blklen != (u32_t) phdr->tag.blk_len) RCD_RETURN_TRUE(retU);
      }
   }
   return retU;
}

//fallback: insert "unknown byte" field: display unknown data instead of error
void insert_unk_byte(edi_field_t *p_fld, u32_t len, u32_t s_offs) {
   for (u32_t cnt=0; cnt<len; ++cnt) {

      memcpy( p_fld, &unknown_byte_fld, EDI_FIELD_SZ );

      p_fld->offs = s_offs;

      s_offs ++ ;
      p_fld  ++ ;
   }
}


//ADB: Audio Data Block
//ADB: AFC : Audio Format Code : selector
sm_vmap ADB_AFC_map = {
  // 0: ! Reserved
   { 1, {0, "LPCM"        , NULL}},
   { 2, {0, "AC-3"        , NULL}},
   { 3, {0, "MPEG1"       , "(Layers 1 or 2)"}},
   { 4, {0, "MP3"         , NULL}},
   { 5, {0, "MPEG2"       , NULL}},
   { 6, {0, "AAC LC"      , NULL}},
   { 7, {0, "DTS"         , NULL}},
   { 8, {0, "ATRAC"       , NULL}},
   { 9, {0, "1Bit Audio"  , NULL}},
   {10, {0, "AC3 Enhanced", NULL}},
   {11, {0, "DTS-HD/UHD"  , NULL}},
   {12, {0, "MAT"         , "DVD Forum MLP"}},
   {13, {0, "DST Audio"   , NULL}},
   {14, {0, "WMA Pro"     , "(Microsoft)"}},
   {15, {0, "ACE"         , "(Audio Coding Extension)"}}
};
//ADB: ACE : Audio Coding Extension Type Code : selector
sm_vmap ADB_ACE_TC_map = {
  // 0..3: ! Reserved
   { 4, {0, "MPEG-4 HE AAC"         , NULL}},
   { 5, {0, "MPEG-4 HE AAC v2"      , NULL}},
   { 6, {0, "MPEG-4 AAC LC"         , NULL}},
 //{ 7, {0, "DRA"                   , NULL}}, //no SAD structure defined: UNK
   { 8, {0, "MPEG-4 HE AAC Surround", NULL}},
  // 9: ! Reserved
   {10, {0, "MPEG-4 AAC LC Surround", NULL}},
   {11, {0, "MPEG-H 3D Audio"       , NULL}},
   {12, {0, "AC-4"                  , NULL}},
   {13, {0, "L-PCM 3D Audio"        , NULL}},
  //14..31: ! Reserved
};

const char  cea_adb_cl::Desc[] =
"Audio Data Block contains one or more 3-byte "
"Short Audio Descriptors (SADs). Each SAD defines audio format, channel number, "
"and bitrate/resolution capabilities of the display";

const dbc_root_dsc_t cea_adb_cl::ADB_grp = {
   .CodN     = "ADB",
   .Name     = "Audio Data Block",
   .Desc     = Desc,
   .type_id  = ID_ADB | ID_SAD,
   .flags    = 0,
   .min_len  = sizeof(sad_t),
   .max_len  = 31,
   .hdr_fcnt = CEA_DBCHDR_FCNT,
   .hdr_sz   = sizeof(bhdr_t),
   .ahf_sz   = 0,
   .ahf_cnt  = 0,
   .ah_flds  = NULL,
   .grp_arsz = 1,
   .grp_ar   = &cea_sad_cl::SAD_subg
};

rcode cea_adb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   retU = base_DBC_Init_RootGrp(inst, &ADB_grp, orflags, parent);
   return retU;
}

//ADB: Audio Data Block ->
//SAD: Short Audio Descriptor
const char  cea_sad_cl::Desc[] =
"SAD describes audio format, channel number, "
"and sample bitrate/resolution capabilities of the display";

static const char freqSuppDsc[] =
"SAD byte 1, bit flag =1: sampling frequency is supported.";
static const char sadAFC_Desc[] =
"Audio Format Codes:\n"
"00: reserved\n"
"01: LPCM (Linear Pulse Code Modulation)\n"
"02: AC-3\n"
"03: MPEG1 (Layers 1 or 2)\n"
"04: MP3\n"
"05: MPEG2\n"
"06: AAC LC\n"
"07: DTS\n"
"08: ATRAC\n"
"09: 1 Bit Audio\n"
"10: AC3 Enhanced\n"
"11: DTS-HD/UHD\n"
"12: MAT: DVD Forum MLP\n"
"13: DST Audio\n"
"14: Microsoft WMA Pro\n"
"15: Audio Coding Extension Type Code is used (ACE)";
//const u32_t cea_sad_cl::fcount = 16;

static const char sadACE_Desc[] =
"00..03: ! Reserved\n"
"04 MPEG-4 HE AAC\n"
"05 MPEG-4 HE AAC v2\n"
"06 MPEG-4 AAC LC\n"
"07 DRA  no SAD structure defined: UNK\n"
"08 MPEG-4 HE AAC Surround\n"
"09: Reserved\n"
"10 MPEG-4 AAC LC Surround\n"
"11 MPEG-H 3D Audio\n"
"12 AC-4\n"
"13 L-PCM 3D Audio\n"
"14..31: Reserved";

/* SAD (Short Audio Descriptor) uses different data layouts, depending
   on Audio Format Code (AFC) and Audio Coding Extension Type Code (ACE).
   All the possible data layouts are defined here, and the final SAD is
   constructed by cea_sad_cl::gen_data_layout(), which is also setting the
   cea_sad_cl::fcount variable.
*/

const dbc_subg_dsc_t cea_sad_cl::SAD_subg = {
   .s_ctor   = &cea_sad_cl::group_new,
   .CodN     = "SAD",
   .Name     = "Short Audio Descriptor",
   .Desc     = Desc,
   .type_id  = ID_SAD | T_SUB_GRP,
   .min_len  = sizeof(sad_t),
   .fcount   = 0,
   .inst_cnt = -1,
   .fields   = {}
};

//SAD byte 0: AFC = 1...14
const edi_field_t cea_sad_cl::byte0_afc1_14[] {
   {&EDID_cl::BitF8Val, 0, offsetof(sad_t, sad0), 0, 3, F_BFD|F_INT, 0, 8, "num_chn",
   "Bitfield of 3 bits: number of channels minus 1 -> f.e. 0 = 1 channel; 7 = 8 channels." },
   {&EDID_cl::BitF8Val, VS_ADB_AFC, offsetof(sad_t, sad0), 3, 4, F_BFD|F_INT|F_VS|F_DN|F_FR, 0, 0x0F,
   "AFC", sadAFC_Desc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad0), 7, 1, F_BIT|F_RD, 0, 1, "rsvd0_7",
   "reserved (0)" }
};
//SAD byte 0: AFC = 15 && ACE = 11 (MPEG-H 3D Audio)
const edi_field_t cea_sad_cl::byte0_afc15_ace11[] {
   {&EDID_cl::BitF8Val, 0, offsetof(sad_t, sad0), 0, 3, F_BFD|F_INT, 0, 8, "MPEGH_3DA",
   "MPEG-H 3D Audio Level:\n"
   " 0b000: (0) unspecified\n 0b001..0b101 (1..5) level 1..5\n 0b110, 0b111 (6,7) reserved" },
   {&EDID_cl::BitF8Val, VS_ADB_AFC, offsetof(sad_t, sad0), 3, 4, F_BFD|F_INT|F_VS|F_DN|F_FR, 0, 0x0F,
   "AFC", sadAFC_Desc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad0), 7, 1, F_BIT|F_RD, 0, 1, "rsvd0_7",
   "reserved (0)" }
};
//SAD byte 0: AFC = 15 && ACE = 12 (AC-4)
const edi_field_t cea_sad_cl::byte0_afc15_ace12[] {
   {&EDID_cl::BitF8Val, 0, offsetof(sad_t, sad0), 0, 3, F_BFD|F_INT, 0, 8, "rsvd0_02",
   "reserved (0)" },
   {&EDID_cl::BitF8Val, VS_ADB_AFC, offsetof(sad_t, sad0), 3, 4, F_BFD|F_INT|F_VS|F_DN|F_FR, 0, 0x0F,
   "AFC", sadAFC_Desc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad0), 7, 1, F_BIT|F_RD, 0, 1, "rsvd0_7",
   "reserved (0)" }
};
//SAD byte 0: AFC = 15 && ACE = 13 (L-PCM 3D Audio)
const edi_field_t cea_sad_cl::byte0_afc15_ace13[] {
   {&EDID_cl::SAD_LPCM_MC, 0, offsetof(sad_t, sad0), 0, 3, F_BFD|F_INT, 0, 32, "num_chn",
   "Set of 5 bits (scattered - not a bitfield), number of channels minus 1 - i.e. 0 -> 1 channel; "
   "7 -> 8 channels.\n"
   "Used only with L-PCM 3D Audio." },
   {&EDID_cl::BitF8Val, VS_ADB_AFC, offsetof(sad_t, sad0), 3, 4, F_BFD|F_INT|F_VS|F_DN|F_FR, 0, 0x0F,
   "AFC", sadAFC_Desc },
};

//SAD byte 1: AFC = 1...14 || AFC = 15 && ACE = 11 (MPEG-H 3D Audio)
const edi_field_t cea_sad_cl::byte1_afc1_14_ace11[] = {
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 0, 1, F_BIT, 0, 1, "sf_32kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 1, 1, F_BIT, 0, 1, "sf_44.1kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 2, 1, F_BIT, 0, 1, "sf_48kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 3, 1, F_BIT, 0, 1, "sf_88.2kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 4, 1, F_BIT, 0, 1, "sf_96kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 5, 1, F_BIT, 0, 1, "sf_176.4kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 6, 1, F_BIT, 0, 1, "sf_192kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 7, 1, F_BIT|F_RD, 0, 1, "rsvd1_7",
   "reserved (0)" }
};
//SAD byte 1: AFC = 15 && ACE = 4,5,6,8,10
const edi_field_t cea_sad_cl::byte1_afc15_ace456810[] = {
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 0, 1, F_BIT, 0, 1, "sf_32kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 1, 1, F_BIT, 0, 1, "sf_44.1kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 2, 1, F_BIT, 0, 1, "sf_48kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 3, 1, F_BIT, 0, 1, "sf_88.2kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 4, 1, F_BIT, 0, 1, "sf_96kHz",
    freqSuppDsc },
   {&EDID_cl::BitF8Val, 0, offsetof(sad_t, sad1), 5, 3, F_BFD|F_RD, 0, 1, "rsvd1_57",
   "reserved (0)" },
};
//SAD byte 1: //AFC = 15 && ACE = 12 (AC-4)
const edi_field_t cea_sad_cl::byte1_afc15_ace12_fsmp[] = {
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 0, 1, F_BIT|F_RD, 0, 1, "rsvd1_0",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 1, 1, F_BIT, 0, 1, "sf_44.1kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 2, 1, F_BIT, 0, 1, "sf_48kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 3, 1, F_BIT|F_RD, 0, 1, "rsvd1_3",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 4, 1, F_BIT, 0, 1, "sf_96kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 5, 1, F_BIT|F_RD, 0, 1, "rsvd1_5",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 6, 1, F_BIT, 0, 1, "sf_192kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 7, 1, F_BIT|F_RD, 0, 1, "rsvd1_7",
   "reserved (0)" }
};
//SAD byte 1: AFC = 15 && ACE = 13 (L-PCM 3D Audio)
const edi_field_t cea_sad_cl::byte1_afc15_ace13[] = {
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 0, 1, F_BIT, 0, 1, "sf_32kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 1, 1, F_BIT, 0, 1, "sf_44.1kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 2, 1, F_BIT, 0, 1, "sf_48kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 3, 1, F_BIT, 0, 1, "sf_88.2kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 4, 1, F_BIT, 0, 1, "sf_96kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 5, 1, F_BIT, 0, 1, "sf_176.4kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 6, 1, F_BIT, 0, 1, "sf_192kHz",
    freqSuppDsc },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad1), 7, 1, F_BIT|F_RD, 0, 1, "MC4",
   "This bit is part of 'num_chn' value for L-PCM 3D Audio, do not change it directly!." }
};

//SAD byte 2: AFC = 1: LPCM
const edi_field_t cea_sad_cl::byte2_afc1[] = {
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 0, 1, F_BIT, 0, 1, "sample16b",
   "LPCM sample size in bits: 1=supported." },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 1, 1, F_BIT, 0, 1, "sample20b",
   "LPCM sample size in bits: 1=supported." },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 2, 1, F_BIT, 0, 1, "sample24b",
   "LPCM sample size in bits: 1=supported." },
   {&EDID_cl::BitF8Val, 0, offsetof(sad_t, sad2), 3, 5, F_BFD|F_RD, 0, 0, "rsvd2_37",
   "reserved (0)" }
};
//SAD byte 2: AFC = 2...8 : bitrate
const edi_field_t cea_sad_cl::byte2_afc2_8[] = {
   {&EDID_cl::SAD_BitRate, 0, offsetof(sad_t, sad2), 0, 1, F_BTE|F_INT|F_KHZ, 0, (0xFF*8), "Max bitrate",
   "Maximum bitrate, stored value is the bitrate divided by 8kHz" }
};
//SAD byte 2: AFC = 9...13: AFC dependent value
const edi_field_t cea_sad_cl::byte2_afc9_13[] = {
   {&EDID_cl::ByteVal, 0, offsetof(sad_t, sad2), 0, 1, F_BTE|F_HEX, 0, 0xFF, "byte2",
   "AFC dependent value, not used with AFC 9..13" }
};
//SAD byte 2: AFC = 14: WMA-Pro
const edi_field_t cea_sad_cl::byte2_afc14[] = {
   {&EDID_cl::BitF8Val, 0, offsetof(sad_t, sad2), 0, 3, F_BFD|F_RD, 0, 0, "profile",
   "WMA-Pro profile." },
   {&EDID_cl::BitF8Val, 0, offsetof(sad_t, sad2), 3, 5, F_BFD|F_RD, 0, 0, "rsvd2_37",
   "reserved (0)" }
};
//SAD byte 2: AFC = 15, ACE = 4, 5, 6
const edi_field_t cea_sad_cl::byte2_afc15_ace456[] = {
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 0, 1, F_BIT|F_RD, 0, 1, "SysH_22_2",
   "ITU-R BS.2051 [139] System H 22.2 multichannel sound\n1: supported\n\n"
   "NOTE: in CTE-861 revisions prior to H this bit is reserved." },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 1, 1, F_BIT, 0, 1, "960_TL",
   "Total frame length 960 samples" },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 2, 1, F_BIT, 0, 1, "1024_TL",
   "Total frame length 1024 samples" },
   {&EDID_cl::BitF8Val, VS_ADB_ACE_TC, offsetof(sad_t, sad2), 3, 5, F_BFD|F_INT|F_DN|F_FR|F_VS, 1, 13,
   "ACE_TC", sadACE_Desc }
};
//SAD byte 2: AFC = 15, ACE = 8 or 10
const edi_field_t cea_sad_cl::byte2_afc15_ace8_10[] = {
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 0, 1, F_BIT, 0, 1, "MPS_L",
   "MPEG Surround" },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 1, 1, F_BIT, 0, 1, "960_TL",
   "Total frame length 960 samples" },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 2, 1, F_BIT, 0, 1, "1024_TL",
   "Total frame length 1024 samples" },
   {&EDID_cl::BitF8Val, VS_ADB_ACE_TC, offsetof(sad_t, sad2), 3, 5, F_BFD|F_INT|F_DN|F_FR|F_VS, 1, 13,
   "ACE_TC", sadACE_Desc }
};
//SAD byte 2: AFC = 15, ACE = 11 (MPEG-H 3D Audio)
const edi_field_t cea_sad_cl::byte2_afc15_ace11[] = {
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 0, 1, F_BIT, 0, 1, "LCP",
   "MPEG-H 3D Audio Low Complexity Profile\n1: supported" },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 1, 1, F_BIT, 0, 1, "BP",
   "MPEG-H 3D Audio Baseline Profile\n1: supported" },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 2, 1, F_BIT|F_RD, 0, 1, "rsvd2_2",
   "reserved (0)" },
   {&EDID_cl::BitF8Val, VS_ADB_ACE_TC, offsetof(sad_t, sad2), 3, 5, F_BFD|F_INT|F_DN|F_FR|F_VS, 1, 13,
   "ACE_TC", sadACE_Desc }
};
//SAD byte 2: AFC = 15, ACE = 12 (AC-4)
const edi_field_t cea_sad_cl::byte2_afc15_ace12[] = {
   {&EDID_cl::BitF8Val, 0, offsetof(sad_t, sad2), 0, 3, F_BFD, 0, 0, "AFC_dep_val",
   "Audio Format Code dependent value" },
   {&EDID_cl::BitF8Val, VS_ADB_ACE_TC, offsetof(sad_t, sad2), 3, 5, F_BFD|F_INT|F_DN|F_FR|F_VS, 1, 13,
   "ACE_TC", sadACE_Desc }
};
//SAD byte 2: AFC = 15, ACE = 13 (L-PCM 3D Audio)
const edi_field_t cea_sad_cl::byte2_afc15_ace13[] = {
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 0, 1, F_BIT, 0, 1, "s16bit",
   "16bit sample" },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 1, 1, F_BIT, 0, 1, "s20bit",
   "20bit sample" },
   {&EDID_cl::BitVal, 0, offsetof(sad_t, sad2), 2, 1, F_BIT, 0, 1, "s24bit",
   "24bit sample" },
   {&EDID_cl::BitF8Val, VS_ADB_ACE_TC, offsetof(sad_t, sad2), 3, 5, F_BFD|F_INT|F_DN|F_FR|F_VS, 1, 13,
   "ACE_TC", sadACE_Desc }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode cea_sad_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;
   rcode retU2;

   parent_grp  = parent;
   type_id.t32 = ID_SAD | T_SUB_GRP;
   AFC         = 0;

   //pre-alloc buffer for array of fields
   dyn_fldar = (edi_field_t*) malloc( 31 * EDI_FIELD_SZ );
   if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);

   CopyInstData(inst, sizeof(sad_t));

   retU = gen_data_layout(inst_data);

   retU2 = init_fields(dyn_fldar, inst_data, dyn_fcnt, false,
                       SAD_subg.Name, SAD_subg.Desc, SAD_subg.CodN);

   if (! RCD_IS_OK(retU )) return retU;
   if (! RCD_IS_OK(retU2)) return retU2;

   if (AFC == 15) { //move ACE field to pos. 3 (right after AFC)
      u32_t          idx;
      edi_dynfld_t  *pfld;

      idx  = FieldsAr.GetCount() -1;
      pfld = FieldsAr.Detach(idx);
      FieldsAr.Insert(pfld, 2);
   }

   RCD_RETURN_OK(retU);
}
#pragma GCC diagnostic warning "-Wunused-parameter"

rcode cea_sad_cl::ForcedGrpRefresh() {
   rcode retU;
   rcode retU2;

   //clear fields
   memset( (void*) dyn_fldar, 0, ( 31 * EDI_FIELD_SZ ) );

   AFC = 0;
   //on error, "unknown data" layout is generated, init_fields is always called.
   retU  = gen_data_layout(inst_data);

   retU2 = init_fields(dyn_fldar, inst_data, dyn_fcnt);

   if (! RCD_IS_OK(retU )) return retU;
   if (! RCD_IS_OK(retU2)) return retU2;

   if (AFC == 15) { //move ACE field to pos. 3 (right after AFC)
      u32_t          idx;
      edi_dynfld_t  *pfld;

      idx  = FieldsAr.GetCount() -1;
      pfld = FieldsAr.Detach(idx);
      FieldsAr.Insert(pfld, 2);
   }

   RCD_RETURN_OK(retU);
}

rcode cea_sad_cl::data_byte0() {
   rcode  retU;

   if (AFC == 0 ) {
      RCD_RETURN_FAULT_MSG(retU, "[E!] SAD: invalid Audio Format Code (AFC)");
   }
   if (AFC < 15) { //AFC = 1...14
      memcpy( p_fld, byte0_afc1_14, (byte0_afc1_14_fcnt * EDI_FIELD_SZ) );
      p_fld    += byte0_afc1_14_fcnt;
      dyn_fcnt  = byte0_afc1_14_fcnt;
   } else
   {  //AFC = 15 && ACE = 4-6,8,10: same as for AFC = 1..14
      if ((ACE >= 4 && 6 >= ACE) || (ACE == 8) || (ACE == 10)) {
         memcpy( p_fld, byte0_afc1_14, (byte0_afc1_14_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte0_afc1_14_fcnt;
         dyn_fcnt  = byte0_afc1_14_fcnt;

      } else
      //AFC = 15 && ACE = 11,12 (MPEG-H 3D Audio, AC-4)
      if (ACE == 11) {
         memcpy( p_fld, byte0_afc15_ace11, (byte0_afc15_ace11_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte0_afc15_ace11_fcnt;
         dyn_fcnt  = byte0_afc15_ace11_fcnt;

      } else
      if (ACE == 12) {
         memcpy( p_fld, byte0_afc15_ace12, (byte0_afc15_ace12_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte0_afc15_ace12_fcnt;
         dyn_fcnt  = byte0_afc15_ace12_fcnt;

      } else
      // AFC == 15, ACE = 13 (L-PCM 3D Audio)
      if (ACE == 13) {
         memcpy( p_fld, byte0_afc15_ace13, (byte0_afc15_ace13_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte0_afc15_ace13_fcnt;
         dyn_fcnt  = byte0_afc15_ace13_fcnt;
      } else {
      // AFC == 15, ACE > 13 || ACE = 9 || ACE = 7 || ACE < 4 : unknown
         RCD_RETURN_FAULT_MSG(retU, "[E!] SAD: invalid Audio Coding Extension Type Code (ACE)");
      }
   }
   RCD_RETURN_OK(retU);
}

rcode cea_sad_cl::data_byte1() {
   rcode  retU;

   //AFC = 1...14 || AFC = 15 && ACE = 11 (MPEG-H 3D Audio)
   if ( (AFC < 15) || ( AFC == 15 && ACE == 11 ) ) {
      memcpy( p_fld, byte1_afc1_14_ace11, (byte1_afc1_14_ace11_fcnt * EDI_FIELD_SZ) );
      p_fld    += byte1_afc1_14_ace11_fcnt;
      dyn_fcnt += byte1_afc1_14_ace11_fcnt;
   } else {
      //AFC == 15
      if ( (ACE >= 4 && 6 >= ACE) || ACE==8 || ACE==10 ) { //ACE = 4,5,6,8,10
         memcpy( p_fld, byte1_afc15_ace456810, (byte1_afc15_ace456810_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte1_afc15_ace456810_fcnt;
         dyn_fcnt += byte1_afc15_ace456810_fcnt;
      } else
      if ( ACE==12 ) { //ACE = 12 (AC-4)
         memcpy( p_fld, byte1_afc15_ace12_fsmp, (byte1_afc15_ace12_fsmp_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte1_afc15_ace12_fsmp_fcnt;
         dyn_fcnt += byte1_afc15_ace12_fsmp_fcnt;
      } else
      if ( ACE==13 ) { //ACE = 13 (L-PCM 3D Audio)
         memcpy( p_fld, byte1_afc15_ace13, (byte1_afc15_ace13_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte1_afc15_ace13_fcnt;
         dyn_fcnt += byte1_afc15_ace13_fcnt;
      } else {
      // AFC == 15, ACE > 13 || ACE = 9 || ACE = 7 || ACE < 4 :
         RCD_RETURN_FAULT_MSG(retU, "[E!] SAD: invalid Audio Coding Extension Type Code (ACE)");
      }
   }
   RCD_RETURN_OK(retU);
}

rcode cea_sad_cl::data_byte2() {
   rcode  retU;

   if (AFC == 1 ) { //AFC = 1: LPCM
      memcpy( p_fld, byte2_afc1, (byte2_afc1_fcnt * EDI_FIELD_SZ) );
      p_fld    += byte2_afc1_fcnt;
      dyn_fcnt += byte2_afc1_fcnt;
      RCD_RETURN_OK(retU);
   } else
   if (AFC >= 2 && 8 >= AFC ) { //AFC = 2...8 : bitrate
      memcpy( p_fld, byte2_afc2_8, (byte2_afc2_8_fcnt * EDI_FIELD_SZ) );
      p_fld    += byte2_afc2_8_fcnt;
      dyn_fcnt += byte2_afc2_8_fcnt;
      RCD_RETURN_OK(retU);
   } else
   if (AFC >= 9 && 13 >= AFC ) { //AFC = 9...13: AFC dependent value
      memcpy( p_fld, byte2_afc9_13, (byte2_afc9_13_fcnt * EDI_FIELD_SZ) );
      p_fld    += byte2_afc9_13_fcnt;
      dyn_fcnt += byte2_afc9_13_fcnt;
      RCD_RETURN_OK(retU);
   } else
   if (AFC == 14) { //AFC = 14: WMA-Pro
      memcpy( p_fld, byte2_afc14, (byte2_afc14_fcnt * EDI_FIELD_SZ) );
      p_fld    += byte2_afc14_fcnt;
      dyn_fcnt += byte2_afc14_fcnt;
      RCD_RETURN_OK(retU);
   } else
   if (AFC == 15) {
      if ( ACE >= 4 && 6 >= ACE ) { //AFC = 15, ACE = 4, 5, 6
         memcpy( p_fld, byte2_afc15_ace456, (byte2_afc15_ace456_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte2_afc15_ace456_fcnt;
         dyn_fcnt += byte2_afc15_ace456_fcnt;
         RCD_RETURN_OK(retU);
      } else
      if ( ACE==8 || ACE==10 ) { //AFC = 15, ACE = 8 or 10
         memcpy( p_fld, byte2_afc15_ace8_10, (byte2_afc15_ace8_10_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte2_afc15_ace8_10_fcnt;
         dyn_fcnt += byte2_afc15_ace8_10_fcnt;
         RCD_RETURN_OK(retU);
      } else
      if ( ACE==11 ) { //AFC = 15, ACE = 11 (MPEG-H 3D Audio)
         memcpy( p_fld, byte2_afc15_ace11, (byte2_afc15_ace11_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte2_afc15_ace11_fcnt;
         dyn_fcnt += byte2_afc15_ace11_fcnt;
         RCD_RETURN_OK(retU);
      } else
      if ( ACE==12 ) { //AFC = 15, ACE = 12 (AC-4)
         memcpy( p_fld, byte2_afc15_ace12, (byte2_afc15_ace12_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte2_afc15_ace12_fcnt;
         dyn_fcnt += byte2_afc15_ace12_fcnt;
         RCD_RETURN_OK(retU);
      } else
      if ( ACE==13 ) { //AFC = 15, ACE = 13 (L-PCM 3D Audio)
         memcpy( p_fld, byte2_afc15_ace13, (byte2_afc15_ace13_fcnt * EDI_FIELD_SZ) );
         p_fld    += byte2_afc15_ace13_fcnt;
         dyn_fcnt += byte2_afc15_ace13_fcnt;
         RCD_RETURN_OK(retU);
      }
      // AFC == 15, ACE > 13 || ACE = 9 || ACE = 7 || ACE < 4 : unknown
      RCD_RETURN_FAULT_MSG(retU, "[E!] SAD: invalid Audio Coding Extension Type Code (ACE)");
   }
   RCD_RETURN_OK(retU);
}

rcode cea_sad_cl::gen_data_layout(const u8_t* inst) {
   rcode  retU;
   u32_t  ulen;
   u32_t  uoffs;

   p_fld    = dyn_fldar;
   dyn_fcnt = 0;

   AFC = reinterpret_cast <const sad0_t*> (inst  )->afc1_14.audio_fmt;
   ACE = reinterpret_cast <const sad2_t*> (inst+2)->afc15_ace.ace_456.ace_tc;

   //SAD byte 0
   retU = data_byte0();
   if (! RCD_IS_OK(retU)) {
      ulen  = 3;
      uoffs = 0;
      goto _err;
   }
   //SAD byte 1
   retU = data_byte1();
   if (! RCD_IS_OK(retU)) {
      ulen  = 2;
      uoffs = 1;
      goto _err;
   }
   //SAD byte 2
   retU = data_byte2();
   if (! RCD_IS_OK(retU)) {
      ulen  = 1;
      uoffs = 2;
      goto _err;
   }
   return retU;

_err:
   //invalid/unknown AFC or ACE
   insert_unk_byte(p_fld, ulen, uoffs);
   dyn_fcnt += ulen;
   return retU;
}

void cea_sad_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   const vmap_ent_t *m_ent;
   sad_t            *psad;
   u32_t             mAFC;
   u32_t             mACE;

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   psad = reinterpret_cast<sad_t*> (inst_data);
   mAFC = psad->sad0.afc1_14.audio_fmt;
   mACE = psad->sad2.afc15_ace.ace_456.ace_tc;

   if (mAFC != 15) {
      m_ent   = vmap_GetVmapEntry(VS_ADB_AFC, VMAP_MID, mAFC);
      if (m_ent != NULL) {
         gp_name = m_ent->name;
         return;
      }
      gp_name  = "AFC";
      gp_name << mAFC;
      return;
   }

   m_ent = vmap_GetVmapEntry(VS_ADB_ACE_TC, VMAP_MID, mACE);
   if (m_ent != NULL) {
         gp_name = m_ent->name;
         return;
   }

   gp_name  = "AFC15::ACE";
   gp_name << mACE;

}

//ADB: Audio Data Block: handlers
//ADB:SAD byte 1, AFC=2..8, Max bitrate
rcode EDID_cl::SAD_BitRate(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   sad2_t *sad2;

   sad2 = reinterpret_cast<sad2_t*> ( getValPtr(p_field) );

   if (op == OP_READ) {
      ival   = sad2->afc2_8_bitrate8k;
      ival <<= 3; //*8kHz
      sval <<  ival;
      RCD_SET_OK(retU);
   } else {
      ulong  utmp;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, utmp);
         if (! RCD_IS_OK(retU)) return retU;
      } else if (op == OP_WRINT) {
         utmp = ival;
         RCD_SET_OK(retU);
      } else {
         RCD_RETURN_FAULT(retU); //wrong op code
      }
      {
         uint  tmpv;
         tmpv   = utmp;
         tmpv >>= 3; // div by 8kHz
         tmpv  &= 0xFF;
         sad2->afc2_8_bitrate8k = tmpv;
      }
   }
   return retU;
}
//ADB:SAD byte0 & 1, AFC = 15 && ACE = 13 (L-PCM 3D Audio): number of channels, bits MC4..MC0
rcode EDID_cl::SAD_LPCM_MC(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      uint  tmpv;
      tmpv   = ((sad0_t*)  inst   )->afc15_ace13.MC3;
      ival   = ((sad1_t*) (inst+1))->fsmp_afc15_ace13.MC4;
      ival <<= 1;
      ival  |= tmpv; //bits MC4,MC3
      tmpv   = inst[0] & 0x07; //bits MC2 .. MC0
      ival <<= 3;
      ival  |= tmpv; //bits MC4 .. MC0

      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong  utmp;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, utmp);
         if (! RCD_IS_OK(retU)) return retU;
      } else if (op == OP_WRINT) {
         utmp = ival;
         RCD_SET_OK(retU);
      } else {
         RCD_RETURN_FAULT(retU); //wrong op code
      }
      {
         uint  tmpv;
         utmp   &= 0xFF;

         tmpv    = utmp >> 3; //bit MC3
         tmpv  <<= 7;
         ival    = inst[0]; //SAD byte 0
         ival   &= 0x78;    //clr M3, MC2-0
         ival   |= tmpv;    //bit MC3

         tmpv    = utmp & 0x07; //bits MC2 .. MC0
         ival   |= tmpv;
         inst[0] = ival; //write bits M3, MC2-0

         tmpv    = utmp >> 4; //bit MC4
         ((sad1_t*) inst+1)->fsmp_afc15_ace13.MC4 = tmpv;
      }
   }
   return retU;
}


//VDB: Video Data Block
const char  cea_vdb_cl::Desc[] =
"Video Data Block contains one or more 1-byte "
"Short Video Descriptors (SVD). Each SVD contains index number from supported "
"resolutions set defined in CEA/EIA standard, and an optional \"native\" resolution flag";

const dbc_root_dsc_t cea_vdb_cl::VDB_grp = {
   .CodN     = "VDB",
   .Name     = "Video Data Block",
   .Desc     = Desc,
   .type_id  = ID_VDB | ID_SVD,
   .flags    = 0,
   .min_len  = sizeof(svd_t),
   .max_len  = 31,
   .hdr_fcnt = CEA_DBCHDR_FCNT,
   .hdr_sz   = sizeof(bhdr_t),
   .ahf_sz   = 0,
   .ahf_cnt  = 0,
   .ah_flds  = NULL,
   .grp_arsz = 1,
   .grp_ar   = &cea_svd_cl::SVD_subg
};

rcode cea_vdb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   retU = base_DBC_Init_RootGrp(inst, &VDB_grp, orflags, parent);
   return retU;
}

//VDB: Video Data Block ->
//SVD: Short Video Descriptor
const char  cea_svd_cl::Desc[] =
"Short Video Descriptor:\n"
"contains index number of supported resolution defined in CEA/CTA-861 standard, "
"and an optional \"native\" resolution flag";

const dbc_subg_dsc_t cea_svd_cl::SVD_subg = {
   .s_ctor   = &cea_svd_cl::group_new,
   .CodN     = "SVD",
   .Name     = "Short Video Descriptor",
   .Desc     = Desc,
   .type_id  = ID_SVD | T_SUB_GRP,
   .min_len  = sizeof(svd_t),
   .fcount   = 2,
   .inst_cnt = -1,
   .fields   = cea_svd_cl::fld_dsc
};

const edi_field_t cea_svd_cl::fld_dsc[] = {
   {&EDID_cl::SVD_VIC, VS_SVD_VIDFMT, 0, 0, 1, F_BTE|F_INT|F_VS|F_FR|F_DN, 0, 0xFF, "VIC",
   "Video ID Code: an index referencing the Table 3 in CEA/CTA-861, containing standard screen resolutions. "
   "For CEA-861-F and above, the descriptor byte can take 8bits, interpretation:\n"
   "0\t\t: reserved\n"
   "1-127\t\t: 7bit VIC,\n"
   "128\t\t: reserved\n"
   "129-192\t: 7bit VIC, modes 1-64 with \"native\" bit set to 1 (128+1..128+64)\n"
   "193-219\t: 8bit VIC, no \"native\" bit\n"
   "220-255\t: reserved\n\n"
   "This field shows the VIC without the \"native\" bit." },
   {&EDID_cl::SVD_Native, 0, 0, 7, 1, F_BIT|F_FR|F_DN, 0, 1, "Native",
   "\"native\" mode flag\n\n"
   "Only VICs 1-64 can be marked as native. See the VIC field description." }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode cea_svd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   parent_grp  = parent;
   type_id.t32 = SVD_subg.type_id;

   CopyInstData(inst, sizeof(svd_t));

   retU = init_fields(&fld_dsc[0], inst_data, SVD_subg.fcount, false,
                      SVD_subg.Name, SVD_subg.Desc, SVD_subg.CodN);

   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

void cea_svd_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   u32_t         ival;
   u32_t         natv;
   edi_dynfld_t *p_field;

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   p_field = FieldsAr.Item(0);
   ival    = EDID.CEA_VDB_SVD_decode(inst_data[0], natv);

   EDID.getValDesc(gp_name, p_field, ival, VD_DESC);

   if (natv) {
      gp_name << " [Native]"; //Native flag
   }
}

u32_t EDID_cl::CEA_VDB_SVD_decode(u32_t vic, u32_t &native) {
   u32_t natv = 0;
   u32_t mode = (vic & 0xFF);

   switch (vic) {
      case 0 ... 128: //0, 128 reserved, 1..127 -> VIC code
         break;
      case 129 ... 192:
         natv = 1;
         mode = (vic & 0x7F);
         break;
      default: //193 ... 255: svd_vidfmt.h: max VIC is 219, natv=0, 220..255 are reserved in G spec.
         break;
   }

   native = natv;
   return mode;
}

//VIC without the native bit: a write keeps the native bit when the new VIC
//can carry it (1-64)
rcode EDID_cl::SVD_VIC(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;
   u32_t  natv;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      ival = CEA_VDB_SVD_decode(inst[0], natv);
      sval << ival;
      RCD_RETURN_OK(retU);
   }

   ulong tmpv;
   if (op == OP_WRSTR) {
      retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      if (! RCD_IS_OK(retU)) return retU;
   } else if (op == OP_WRINT) {
      tmpv = ival;
   } else {
      RCD_RETURN_FAULT(retU); //wrong op code
   }
   if ((tmpv > 0xFF) || ((tmpv >= 128) && (tmpv <= 192))) {
      RCD_RETURN_FAULT_MSG(retU, "[E!] SVD: VICs 128-192 are not valid, "
                                 "use the Native field to mark VICs 1-64 as native");
   }
   CEA_VDB_SVD_decode(inst[0], natv);
   if (natv && (tmpv >= 1) && (tmpv <= 64)) tmpv |= 0x80;
   inst[0] = (tmpv & 0xFF);
   RCD_RETURN_OK(retU);
}

rcode EDID_cl::SVD_Native(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;
   u32_t  natv;
   u32_t  vic;

   inst = getValPtr(p_field);
   vic  = CEA_VDB_SVD_decode(inst[0], natv);

   if (op == OP_READ) {
      ival = natv;
      sval << ival;
      RCD_RETURN_OK(retU);
   }

   ulong tmpv;
   if (op == OP_WRSTR) {
      retU = getStrUint(sval, 10, 0, 1, tmpv);
      if (! RCD_IS_OK(retU)) return retU;
   } else if (op == OP_WRINT) {
      if (ival > 1) RCD_RETURN_FAULT(retU);
      tmpv = ival;
   } else {
      RCD_RETURN_FAULT(retU); //wrong op code
   }
   if ((tmpv != 0) && ((vic < 1) || (vic > 64))) {
      RCD_RETURN_FAULT_MSG(retU, "[E!] SVD: only VICs 1-64 can be marked as native");
   }
   inst[0] = (vic | ((tmpv != 0) ? 0x80 : 0)) & 0xFF;
   RCD_RETURN_OK(retU);
}

//VSD: Vendor Specific Data Block: handlers
rcode EDID_cl::VSD_ltncy(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      ival   = inst[0];
      if (ival > 0) {
         ival  -= 1;
         ival <<= 1;
      } else {
         //value not provided
         p_field->field.flags |= F_NU;
      }

      //ival = (inst[0] - 1) << 1; //(val-1)*2
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong  tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
         if (! RCD_IS_OK(retU)) return retU;
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      } else {
         RCD_RETURN_FAULT(retU); //wrong op code
      }

      tmpv >>= 1; //(val/2)+1
      tmpv ++ ;
      inst[0] = (tmpv & 0xFF);
      p_field->field.flags &= ~F_NU;
   }
   return retU;
}

rcode EDID_cl::VSD_MaxTMDS(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      ival = (inst[0] * 5);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong       tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
         if (! RCD_IS_OK(retU)) return retU;
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      } else {
         RCD_RETURN_FAULT(retU); //wrong op code
      }

      tmpv = (tmpv / 5);
      inst[0] = (tmpv & 0xFF);
   }
   return retU;
}

//HF: HDMI Forum VSDB and SCDB: VRRmax spans bits 6-7 of the first byte and
//all of the second byte
rcode EDID_cl::HF_VRRmax(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      ival = ((inst[0] & 0xC0) << 2) | inst[1];
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong  tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
         if (! RCD_IS_OK(retU)) return retU;
      } else if (op == OP_WRINT) {
         if (ival > p_field->field.maxv) RCD_RETURN_FAULT(retU);
         tmpv = ival;
         RCD_SET_OK(retU);
      } else {
         RCD_RETURN_FAULT(retU); //wrong op code
      }

      inst[0] = (inst[0] & 0x3F) | ((tmpv >> 2) & 0xC0);
      inst[1] = (tmpv & 0xFF);
   }
   return retU;
}

//Luminance codes (CTA-861.3, AMD FreeSync): the maximum is 50 * 2^(code/32)
//cd/m^2, the minimum is relative to a maximum: max * (code/255)^2 / 100.
//The integer value is the stored code.
static double max_luminance(u32_t code) {
   return 50.0 * std::pow(2.0, code / 32.0);
}

rcode EDID_cl::Luminance(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      ival = inst[0];
      sval.Printf("%.2f", max_luminance(ival));
      RCD_RETURN_OK(retU);
   }

   long code;
   if (op == OP_WRSTR) {
      float value;
      retU = getStrFloat(sval, 50.0, max_luminance(255) + 0.005, value);
      if (! RCD_IS_OK(retU)) return retU;
      code = std::lround(32.0 * std::log2(value / 50.0));
   } else if (op == OP_WRINT) {
      code = ival;
   } else {
      RCD_RETURN_FAULT(retU); //wrong op code
   }
   if ((code < 0) || (code > 255)) RCD_RETURN_FAULT(retU);
   inst[0] = code;
   RCD_RETURN_OK(retU);
}

rcode EDID_cl::MinLuminance(u32_t op, wxc_String& sval, u32_t& ival, u8_t* inst,
                            u32_t max_code) {
   rcode  retU;
   double maxv = max_luminance(max_code);

   if (op == OP_READ) {
      ival = inst[0];
      sval.Printf("%.4f", maxv * std::pow(ival / 255.0, 2) / 100.0);
      RCD_RETURN_OK(retU);
   }

   long code;
   if (op == OP_WRSTR) {
      float value;
      retU = getStrFloat(sval, 0.0, (maxv / 100.0) + 0.00005, value);
      if (! RCD_IS_OK(retU)) return retU;
      code = std::lround(255.0 * std::sqrt(value * 100.0 / maxv));
   } else if (op == OP_WRINT) {
      code = ival;
   } else {
      RCD_RETURN_FAULT(retU); //wrong op code
   }
   if ((code < 0) || (code > 255)) RCD_RETURN_FAULT(retU);
   inst[0] = code;
   RCD_RETURN_OK(retU);
}

//HDRS: the maximum luminance is 2 bytes before the minimum
rcode EDID_cl::HDRS_MinLum(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   u8_t* inst = getValPtr(p_field);
   return MinLuminance(op, sval, ival, inst, inst[-2]);
}

//AMD VSD: the maximum luminance is the byte before the minimum
rcode EDID_cl::AMD_MinLum(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   u8_t* inst = getValPtr(p_field);
   return MinLuminance(op, sval, ival, inst, inst[-1]);
}

//HDMI Forum payload, shared by the HF-VSDB (VSD with OUI C4-5D-D8) and the
//HF-SCDB (Extended Tag Code 121): both place it at byte 4 of the block.
static const char HF_FRL_Desc[] =
"Maximum Fixed Rate Link:\n0= not supported\n1= 3 Gbps on 3 lanes\n"
"2= 3 and 6 Gbps on 3 lanes\n3= as 2, 6 Gbps on 4 lanes\n"
"4= as 3, 8 Gbps on 4 lanes\n5= as 4, 10 Gbps on 4 lanes\n"
"6= as 5, 12 Gbps on 4 lanes\nother values are reserved";

extern const edi_field_t HF_version_fld[];
extern const edi_field_t HF_tmds_fld[];
extern const edi_field_t HF_scdc_fld[];
extern const edi_field_t HF_frl_fld[];
extern const edi_field_t HF_vrr_mode_fld[];
extern const edi_field_t HF_vrr_fld[];
extern const edi_field_t HF_dsc_fld[];
extern const edi_field_t HF_dsc_frl_fld[];
extern const edi_field_t HF_dsc_chunk_fld[];

const edi_field_t HF_version_fld[] = {
   {&EDID_cl::ByteVal, 0, 4, 0, 1, F_BTE|F_INT, 0, 0xFF, "Version",
   "HDMI Forum block version (1)." }
};

const edi_field_t HF_tmds_fld[] = {
   {&EDID_cl::VSD_MaxTMDS, 0, 5, 0, 1, F_INT|F_MHZ, 0, 1275, "Max_TMDS",
   "Maximum TMDS Character Rate / 5 MHz; 0 means 340 MHz or less." }
};

const edi_field_t HF_scdc_fld[] = {
   {&EDID_cl::BitVal, 0, 6, 0, 1, F_BIT, 0, 1, "3D_OSD_Disparity",
   "Supports 3D OSD Disparity signaling." },
   {&EDID_cl::BitVal, 0, 6, 1, 1, F_BIT, 0, 1, "3D_Dual_View",
   "Supports 3D Dual View signaling." },
   {&EDID_cl::BitVal, 0, 6, 2, 1, F_BIT, 0, 1, "3D_Independent_View",
   "Supports 3D Independent View signaling." },
   {&EDID_cl::BitVal, 0, 6, 3, 1, F_BIT, 0, 1, "LTE_340Mcsc_Scramble",
   "Supports scrambling for character rates of 340 Mcsc or less." },
   {&EDID_cl::BitVal, 0, 6, 4, 1, F_BIT, 0, 1, "CCBPCI",
   "Supports Color Content Bits Per Component Indication." },
   {&EDID_cl::BitVal, 0, 6, 5, 1, F_BIT, 0, 1, "Cable_Status",
   "Supports Cable Status." },
   {&EDID_cl::BitVal, 0, 6, 6, 1, F_BIT, 0, 1, "RR_Capable",
   "SCDC Read Request capable." },
   {&EDID_cl::BitVal, 0, 6, 7, 1, F_BIT, 0, 1, "SCDC_Present",
   "Status and Control Data Channel present." }
};

const edi_field_t HF_frl_fld[] = {
   {&EDID_cl::BitVal, 0, 7, 0, 1, F_BIT, 0, 1, "DC_30bit_420",
   "Supports 10 bits per component Deep Color 4:2:0 encoding." },
   {&EDID_cl::BitVal, 0, 7, 1, 1, F_BIT, 0, 1, "DC_36bit_420",
   "Supports 12 bits per component Deep Color 4:2:0 encoding." },
   {&EDID_cl::BitVal, 0, 7, 2, 1, F_BIT, 0, 1, "DC_48bit_420",
   "Supports 16 bits per component Deep Color 4:2:0 encoding." },
   {&EDID_cl::BitVal, 0, 7, 3, 1, F_BIT, 0, 1, "UHD_VIC",
   "Supports UHD VICs signaled through HDMI VICs." },
   {&EDID_cl::BitF8Val, 0, 7, 4, 4, F_BFD|F_INT, 0, 15, "Max_FRL_Rate", HF_FRL_Desc }
};

const edi_field_t HF_vrr_mode_fld[] = {
   {&EDID_cl::BitVal, 0, 8, 0, 1, F_BIT, 0, 1, "FAPA_start_location",
   "Supports a FAPA in blanking after the first active video line." },
   {&EDID_cl::BitVal, 0, 8, 1, 1, F_BIT, 0, 1, "ALLM",
   "Supports Auto Low-Latency Mode." },
   {&EDID_cl::BitVal, 0, 8, 2, 1, F_BIT, 0, 1, "FVA",
   "Supports Fast Vactive." },
   {&EDID_cl::BitVal, 0, 8, 3, 1, F_BIT, 0, 1, "CNMVRR",
   "Supports negative Mvrr values." },
   {&EDID_cl::BitVal, 0, 8, 4, 1, F_BIT, 0, 1, "CinemaVRR",
   "Supports media rates below VRRmin; deprecated, must be 0." },
   {&EDID_cl::BitVal, 0, 8, 5, 1, F_BIT, 0, 1, "M_delta",
   "Supports Mdelta." },
   {&EDID_cl::BitVal, 0, 8, 6, 1, F_BIT, 0, 1, "QMS",
   "Supports Quick Media Switching." },
   {&EDID_cl::BitVal, 0, 8, 7, 1, F_BIT, 0, 1, "FAPA_End_Extended",
   "Supports FAPA End Extended." }
};

const edi_field_t HF_vrr_fld[] = {
   {&EDID_cl::BitF8Val, 0, 9, 0, 6, F_BFD|F_INT|F_HZ, 0, 63, "VRRmin",
   "Minimum Variable Refresh Rate; 0 when VRR is not supported." },
   {&EDID_cl::HF_VRRmax, 0, 9, 0, 2, F_INT|F_HZ, 0, 1023, "VRRmax",
   "Maximum Variable Refresh Rate; 0 when not specified." }
};

const edi_field_t HF_dsc_fld[] = {
   {&EDID_cl::BitVal, 0, 11, 0, 1, F_BIT, 0, 1, "DSC_10bpc",
   "Supports 10 bpc compressed video transport." },
   {&EDID_cl::BitVal, 0, 11, 1, 1, F_BIT, 0, 1, "DSC_12bpc",
   "Supports 12 bpc compressed video transport." },
   {&EDID_cl::BitVal, 0, 11, 2, 1, F_BIT, 0, 1, "DSC_16bpc",
   "Supports 16 bpc compressed video transport." },
   {&EDID_cl::BitVal, 0, 11, 3, 1, F_BIT, 0, 1, "DSC_All_bpp",
   "Supports compressed video transport at any valid 1/16th bit bpp." },
   {&EDID_cl::BitVal, 0, 11, 4, 1, F_BIT, 0, 1, "QMS_TFRmin",
   "Supports QMS TFRmin." },
   {&EDID_cl::BitVal, 0, 11, 5, 1, F_BIT, 0, 1, "QMS_TFRmax",
   "Supports QMS TFRmax." },
   {&EDID_cl::BitVal, 0, 11, 6, 1, F_BIT, 0, 1, "DSC_Native_420",
   "Supports compressed video transport for 4:2:0 encoding." },
   {&EDID_cl::BitVal, 0, 11, 7, 1, F_BIT, 0, 1, "DSC_1p2",
   "Supports VESA DSC 1.2a compression." }
};

const edi_field_t HF_dsc_frl_fld[] = {
   {&EDID_cl::BitF8Val, 0, 12, 0, 4, F_BFD|F_INT, 0, 15, "DSC_MaxSlices",
   "DSC Max Slices:\n0= not supported\n1= 1 slice, 2= 2 slices, 3= 4 slices "
   "and 4= 8 slices at 340 MHz per slice\n5= 8 slices and 6= 12 slices at 400 MHz\n"
   "7= 12 slices at 600 MHz" },
   {&EDID_cl::BitF8Val, 0, 12, 4, 4, F_BFD|F_INT, 0, 15, "DSC_Max_FRL_Rate", HF_FRL_Desc }
};

const edi_field_t HF_dsc_chunk_fld[] = {
   {&EDID_cl::BitF8Val, 0, 13, 0, 6, F_BFD|F_INT, 0, 63, "DSC_TotalChunkKBytes",
   "Maximum bytes in a line of chunks: 1024 * (1 + value)." },
   {&EDID_cl::BitF8Val, 0, 13, 6, 2, F_BFD|F_INT|F_RD, 0, 3, "reserved",
   "reserved (0)" }
};

//VSD: Vendor Specific Data Block
const char  cea_vsd_cl::Desc[] =
"Vendor Specific Data Block is required to contain the following fields:\n"
"1. Vendor's IEEE-OUI 24-bit LE registration number\n"
"2. Source Physical Address (16bit LE). The source physical address provides the CEC physical address for upstream CEC devices.\n"
"The rest of fields can be anything the vendor considers worthy of inclusion in the VSD.\n"
"Known IEEE-OUI codes:\n"
"- 00-0C-03 \"HDMI Licensing, LLC\" -> provides HDMI 1.4 payload\n"
"- C4-5D-D8 \"HDMI Forum\" -> provides HDMI 2.0 payload\n"
"- 00-D0-46 \"DOLBY LABORATORIES, INC.\" -> provides Dolby Vision payload\n"
"- 90-84-8b \"HDR10+ Technologies, LLC\" -> provides HDR10+ payload as part of HDMI 2.1 Amendment A1 standard\n\n"
"wxEDID decodes the 00-0C-03, C4-5D-D8, and 00-00-1A (AMD) payloads; the payload of other\n"
"vendors is shown as data bytes.\n";

const edi_field_t cea_vsd_cl::hdr_fld_dsc[] = {
   {&EDID_cl::ByteStr, 0, offsetof(vsd_hdmi14_t, ieee_id)+1, 0, 3, F_STR|F_HEX|F_LE|F_RD|F_FR, 0, 0xFFFFFF, "IEEE-OUI",
   "IEEE Registration Id LE" },
   {&EDID_cl::Word16, 0, offsetof(vsd_hdmi14_t, src_phy)+1, 0, 2, F_HEX|F_RD, 0, 0xFFFF, "src phy",
   "Source Physical Address (section 8.7 of HDMI 1.3a)." }
};

const edi_field_t cea_vsd_cl::sink_feat_fld_dsc[] = {
   {&EDID_cl::BitVal, 0, offsetof(vsd_hdmi14_t, sink_feat)+1, 0, 1, F_BIT, 0, 1, "DVI_dual",
   "DVI Dual Link Operation support." },
   {&EDID_cl::BitVal, 0, offsetof(vsd_hdmi14_t, sink_feat)+1, 1, 1, F_BIT, 0, 1, "reserved",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(vsd_hdmi14_t, sink_feat)+1, 2, 1, F_BIT, 0, 1, "reserved",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(vsd_hdmi14_t, sink_feat)+1, 3, 1, F_BIT, 0, 1, "DC_Y444",
   "4:4:4 support in deep color modes." },
   {&EDID_cl::BitVal, 0, offsetof(vsd_hdmi14_t, sink_feat)+1, 4, 1, F_BIT, 0, 1, "DC_30bit",
   "10-bit-per-channel deep color support." },
   {&EDID_cl::BitVal, 0, offsetof(vsd_hdmi14_t, sink_feat)+1, 5, 1, F_BIT, 0, 1, "DC_36bit",
   "12-bit-per-channel deep color support." },
   {&EDID_cl::BitVal, 0, offsetof(vsd_hdmi14_t, sink_feat)+1, 6, 1, F_BIT, 0, 1, "DC_48bit",
   "16-bit-per-channel deep color support." },
   {&EDID_cl::BitVal, 0, offsetof(vsd_hdmi14_t, sink_feat)+1, 7, 1, F_BIT, 0, 1, "Supports_AI",
   "Supports_AI (needs info from ACP or ISRC packets)." }
};

const edi_field_t cea_vsd_cl::max_tmds_fld_dsc[] = {
   {&EDID_cl::VSD_MaxTMDS, 0, offsetof(vsd_hdmi14_t, max_tmds)+1, 0, 1, F_INT|F_MHZ, 0, 0xFF, "Max_TMDS",
   "Optional: If non-zero: Max_TMDS_Frequency / 5mhz." },
};

const edi_field_t cea_vsd_cl::latency_fld_dsc[] = {
   {&EDID_cl::BitF8Val, 0, offsetof(vsd_hdmi14_t, ltncy_hdr)+1, 0, 6, F_BFD, 0, 0, "reserved",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(vsd_hdmi14_t, ltncy_hdr)+1, 6, 1, F_BIT, 0, 0, "i_latency",
   "If set, 4 interlaced latency fields are present, must be 0 if latency_f (bit 7) is 0" },
   {&EDID_cl::BitVal, 0, offsetof(vsd_hdmi14_t, ltncy_hdr)+1, 7, 1, F_BIT, 0, 0, "latency_f",
   "If set, latency fields are present" }
};

const edi_field_t cea_vsd_cl::av_latency_fld_dsc[] = {
   {&EDID_cl::VSD_ltncy, 0, offsetof(vsd_hdmi14_t, vid_lat)+1, 0, 1, F_INT|F_MLS|F_FR, 0, 500, "Video Latency",
   "Optional: Video Latency value=1+ms/2 with a max of 251 meaning 500ms, 2ms granularity." },
   {&EDID_cl::VSD_ltncy, 0, offsetof(vsd_hdmi14_t, aud_lat)+1, 0, 1, F_INT|F_MLS|F_FR, 0, 500, "Audio Latency",
   "Optional: Audio Latency value=1+ms/2 with a max of 251 meaning 500ms, 2ms granularity." },
   {&EDID_cl::VSD_ltncy, 0, offsetof(vsd_hdmi14_t, vid_ilat)+1, 0, 1, F_INT|F_MLS|F_FR, 0, 500, "Video iLatency",
   "Optional: Interlaced Video Latency value=1+ms/2 with a max of 251 meaning 500ms, 2ms granularity." },
   {&EDID_cl::VSD_ltncy, 0, offsetof(vsd_hdmi14_t, aud_ilat)+1, 0, 1, F_INT|F_MLS|F_FR, 0, 500, "Audio iLatency",
   "Optional: Interlaced Audio Latency value=1+ms/2 with a max of 251 meaning 500ms, 2ms granularity." }
};

const gpfld_dsc_t cea_vsd_cl::sub_fld_grp[] = {
   { //vsd_hdr_idphy
      .flags    = T_FLEX_LAYOUT,
      .dat_sz   = 5,
      .inst_cnt = 1,
      .fcount   = 2,
      .fields   = cea_vsd_cl::hdr_fld_dsc
   },
   { //vsd_sink_feat
      .flags    = 0,
      .dat_sz   = 1,
      .inst_cnt = 1,
      .fcount   = 8,
      .fields   = cea_vsd_cl::sink_feat_fld_dsc
   },
   { //vsd_max_tmds
      .flags    = 0,
      .dat_sz   = 1,
      .inst_cnt = 1,
      .fcount   = 1,
      .fields   = cea_vsd_cl::max_tmds_fld_dsc
   },
   { //vsd_latency
      .flags    = 0,
      .dat_sz   = 1,
      .inst_cnt = 1,
      .fcount   = 3,
      .fields   = cea_vsd_cl::latency_fld_dsc
   },
   { //vsd_av_latency
      .flags    = T_FLEX_LAYOUT|T_FLEX_LEN,
      .dat_sz   = 4,
      .inst_cnt = 1,
      .fcount   = 4,
      .fields   = cea_vsd_cl::av_latency_fld_dsc
   }
};

const gpfld_dsc_t cea_vsd_cl::hf_fld_grp[] = {
   { .flags = T_FLEX_LAYOUT, .dat_sz = 3, .inst_cnt = 1, .fcount = 1,
     .fields = cea_vsd_cl::hdr_fld_dsc },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 1, .fields = HF_version_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 1, .fields = HF_tmds_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 8, .fields = HF_scdc_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 5, .fields = HF_frl_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 8, .fields = HF_vrr_mode_fld },
   { .flags = 0, .dat_sz = 2, .inst_cnt = 1, .fcount = 2, .fields = HF_vrr_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 8, .fields = HF_dsc_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 2, .fields = HF_dsc_frl_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 2, .fields = HF_dsc_chunk_fld }
};

//AMD (OUI 00-00-1A): FreeSync range and HDR data; the field meanings follow
//edid-decode, several flag bits are undocumented
static const edi_field_t AMD_head_fld[] = {
   {&EDID_cl::ByteVal, 0, 4, 0, 1, F_BTE|F_INT|F_FR, 0, 0xFF, "Version",
   "AMD block version; version 3 moves the maximum refresh rate to 10 bits." },
   {&EDID_cl::ByteVal, 0, 5, 0, 1, F_BTE|F_HEX, 0, 0xFF, "Feature_Caps",
   "Feature capabilities:\nbit1= HDR fields valid\nbit2= global backlight control\n"
   "bit3= local dimming\nbit6= FreeSync Panel Replay\nother bits are undocumented" },
   {&EDID_cl::ByteVal, 0, 6, 0, 1, F_BTE|F_INT|F_HZ, 0, 0xFF, "Min_Refresh",
   "Minimum FreeSync refresh rate." }
};

static const edi_field_t AMD_max_fld[] = {
   {&EDID_cl::ByteVal, 0, 7, 0, 1, F_BTE|F_INT|F_HZ, 0, 0xFF, "Max_Refresh",
   "Maximum FreeSync refresh rate." }
};

static const edi_field_t AMD_max_legacy_fld[] = {
   {&EDID_cl::ByteVal, 0, 7, 0, 1, F_BTE|F_INT|F_HZ, 0, 0xFF, "Max_Refresh_8bit",
   "Maximum refresh rate used before version 3." }
};

static const edi_field_t AMD_flags_fld[] = {
   {&EDID_cl::ByteVal, 0, 8, 0, 1, F_BTE|F_HEX, 0, 0xFF, "Flags_1x",
   "FreeSync 1.x flags; any of bits 1, 2, 5, 6, 7 means the range is switched via MCCS." }
};

static const edi_field_t AMD_hdr_fld[] = {
   {&EDID_cl::ByteVal, 0, 9, 0, 1, F_BTE|F_HEX, 0, 0xFF, "Flags_2x",
   "FreeSync 2.x flags:\nbit2= PQ EOTF\nbits6-7: 1= Mini LED, 2= OLED\n"
   "other bits are undocumented" },
   {&EDID_cl::Luminance, 0, 10, 0, 1, F_FLT|F_CDM2|F_FR, 0, 0xFF, "Max_Luminance",
   "Maximum luminance: 50 * 2^(code/32) cd/m^2.\n"
   "A new value is rounded to the nearest code." },
   {&EDID_cl::AMD_MinLum, 0, 11, 0, 1, F_FLT|F_CDM2, 0, 0xFF, "Min_Luminance",
   "Minimum luminance: Max_Luminance * (code/255)^2 / 100 cd/m^2.\n"
   "A new value is rounded to the nearest code." },
   {&EDID_cl::Luminance, 0, 12, 0, 1, F_FLT|F_CDM2|F_FR, 0, 0xFF, "Max_Luminance_2",
   "Maximum luminance without local dimming, or at minimum backlight: "
   "50 * 2^(code/32) cd/m^2." },
   {&EDID_cl::AMD_MinLum, 0, 13, 0, 1, F_FLT|F_CDM2, 0, 0xFF, "Min_Luminance_2",
   "Minimum luminance without local dimming, or at minimum backlight: "
   "Max_Luminance_2 * (code/255)^2 / 100 cd/m^2." }
};

static const edi_field_t AMD_max3_fld[] = {
   {&EDID_cl::DisplayID_MaxRefresh, 0, 14, 0, 2, F_INT|F_HZ, 0, 1023, "Max_Refresh",
   "Maximum FreeSync refresh rate: low byte, then bits 0-1 of the next byte." }
};

const gpfld_dsc_t cea_vsd_cl::amd_fld_grp[] = {
   { .flags = T_FLEX_LAYOUT, .dat_sz = 3, .inst_cnt = 1, .fcount = 1,
     .fields = cea_vsd_cl::hdr_fld_dsc },
   { .flags = 0, .dat_sz = 3, .inst_cnt = 1, .fcount = 3, .fields = AMD_head_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 1, .fields = AMD_max_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 1, .fields = AMD_flags_fld },
   { .flags = 0, .dat_sz = 5, .inst_cnt = 1, .fcount = 5, .fields = AMD_hdr_fld }
};

const gpfld_dsc_t cea_vsd_cl::amd3_fld_grp[] = {
   { .flags = T_FLEX_LAYOUT, .dat_sz = 3, .inst_cnt = 1, .fcount = 1,
     .fields = cea_vsd_cl::hdr_fld_dsc },
   { .flags = 0, .dat_sz = 3, .inst_cnt = 1, .fcount = 3, .fields = AMD_head_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 1, .fields = AMD_max_legacy_fld },
   { .flags = 0, .dat_sz = 1, .inst_cnt = 1, .fcount = 1, .fields = AMD_flags_fld },
   { .flags = 0, .dat_sz = 5, .inst_cnt = 1, .fcount = 5, .fields = AMD_hdr_fld },
   { .flags = 0, .dat_sz = 2, .inst_cnt = 1, .fcount = 1, .fields = AMD_max3_fld }
};

const dbc_flatgp_dsc_t cea_vsd_cl::AMD_VSD_grp = {
   .CodN     = "VSD",
   .Name     = "AMD Vendor Specific Data Block",
   .Desc     = Desc,
   .type_id  = ID_VSD,
   .flags    = T_FLEX_LAYOUT,
   .min_len  = 3,
   .max_len  = 31,
   .max_fld  = CEA_DBCHDR_FCNT + 11 + 31,
   .hdr_fcnt = CEA_DBCHDR_FCNT,
   .hdr_sz   = sizeof(bhdr_t),
   .fld_arsz = 5,
   .fld_ar   = cea_vsd_cl::amd_fld_grp
};

const dbc_flatgp_dsc_t cea_vsd_cl::AMD3_VSD_grp = {
   .CodN     = "VSD",
   .Name     = "AMD Vendor Specific Data Block",
   .Desc     = Desc,
   .type_id  = ID_VSD,
   .flags    = T_FLEX_LAYOUT,
   .min_len  = 3,
   .max_len  = 31,
   .max_fld  = CEA_DBCHDR_FCNT + 12 + 31,
   .hdr_fcnt = CEA_DBCHDR_FCNT,
   .hdr_sz   = sizeof(bhdr_t),
   .fld_arsz = 6,
   .fld_ar   = cea_vsd_cl::amd3_fld_grp
};

const gpfld_dsc_t cea_vsd_cl::vendor_fld_grp[] = {
   { .flags = T_FLEX_LAYOUT, .dat_sz = 3, .inst_cnt = 1, .fcount = 1,
     .fields = cea_vsd_cl::hdr_fld_dsc }
};

const dbc_flatgp_dsc_t cea_vsd_cl::HF_VSD_grp = {
   .CodN     = "VSD",
   .Name     = "HDMI Forum Vendor Specific Data Block",
   .Desc     = Desc,
   .type_id  = ID_VSD,
   .flags    = T_FLEX_LAYOUT,
   .min_len  = 3,
   .max_len  = 31,
   .max_fld  = CEA_DBCHDR_FCNT + 38 + 31,
   .hdr_fcnt = CEA_DBCHDR_FCNT,
   .hdr_sz   = sizeof(bhdr_t),
   .fld_arsz = 10,
   .fld_ar   = cea_vsd_cl::hf_fld_grp
};

const dbc_flatgp_dsc_t cea_vsd_cl::Vendor_VSD_grp = {
   .CodN     = "VSD",
   .Name     = "Vendor Specific Data Block",
   .Desc     = Desc,
   .type_id  = ID_VSD,
   .flags    = T_FLEX_LAYOUT,
   .min_len  = 3,
   .max_len  = 31,
   .max_fld  = CEA_DBCHDR_FCNT + 1 + 31,
   .hdr_fcnt = CEA_DBCHDR_FCNT,
   .hdr_sz   = sizeof(bhdr_t),
   .fld_arsz = 1,
   .fld_ar   = cea_vsd_cl::vendor_fld_grp
};

const dbc_flatgp_dsc_t cea_vsd_cl::VSD_grp = {
   .CodN     = "VSD",
   .Name     = "HDMI Vendor Specific Data Block",
   .Desc     = Desc,
   .type_id  = ID_VSD,
   .flags    = T_FLEX_LAYOUT,
   .min_len  = 3,
   .max_len  = 31,
   .max_fld  = (32 - sizeof(vsd_hdmi14_t) - sizeof(bhdr_t) + 18),
   .hdr_fcnt = CEA_DBCHDR_FCNT,
   .hdr_sz   = sizeof(bhdr_t),
   .fld_arsz = 5,
   .fld_ar   = cea_vsd_cl::sub_fld_grp
};

rcode cea_vsd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;
   const dbc_flatgp_dsc_t* layout = &VSD_grp;

   //the IEEE OUI (little endian) selects the payload layout
   if (reinterpret_cast <const bhdr_t*> (inst)->tag.blk_len >= 3) {
      u32_t oui = inst[1] | (inst[2] << 8) | (inst[3] << 16);
      if (oui == 0xC45DD8) {
         layout = &HF_VSD_grp;
      } else if (oui == 0x00001A) {
         bool version3 = (reinterpret_cast <const bhdr_t*> (inst)->tag.blk_len >= 4) &&
                         (inst[4] >= 3);
         layout = version3 ? &AMD3_VSD_grp : &AMD_VSD_grp;
      } else if (oui != 0x000C03) {
         layout = &Vendor_VSD_grp;
      }
   }
   retU = base_DBC_Init_FlatGrp(inst, layout, orflags, parent);
   return retU;
}

//SAB: Speaker Allocation Data Block
//NOTE: the structure of SAB is basically an SPM: Speaker Presence Mask
//      The Desc and fields are shared with RMCD: Room Configuration Data Block: SPM

extern const char SAB_SPM_CodN[];
extern const char SAB_SPM_Name[];
extern const char SAB_SPM_Desc[];

const char SAB_SPM_CodN[] = "SPM";
const char SAB_SPM_Name[] = "Speaker Presence Mask";
const char SAB_SPM_Desc[] =
"Speaker Presence Mask (3 bytes).\n"
"Contains information about which speakers are present in the display device.\n"
"Byte0:\n"
"FL_FR\t\t: Front Left/Right\n"
"LFE1\t\t: LFE Low Frequency Effects 1 (subwoofer1)\n"
"FC\t\t\t: Front Center\n"
"BL_BR\t\t: Back Left/Right\n"
"BC\t\t\t: Back Center\n"
"FLC_FRC\t\t: Front Left/Right of Center\n"
"bit6\t\t: reserved in CTA-861-H, was RLC/RRC: Rear Left/Right of Center\n"
"FLW_FRW\t: Front Left/Right Wide\n"
"Byte1:\n"
"TpFL_TpFR\t: Top Front Left/Right\n"
"TpC\t\t\t: Top Center\n"
"TpFC\t\t: Top Front Center\n"
"LS_RS\t\t: Left/Right Surround\n"
"LFE2\t\t: LFE Low Frequency Effects 2 (subwoofer2)\n"
"TpBC\t\t: Top Back Center\n"
"SiL_SiR\t\t: Side Left/Right\n"
"TpSiL_TpSiR\t: Top Side Left/Right\n"
"Byte2:\n"
"TpBL_TpBR\t: Top Back Left/Right\n"
"BtFC\t\t: Bottom Front Center\n"
"BtFL_BtFR\t: Bottom Front Left/Right\n"
"bit3:\t\t reserved in CTA-861-H, was TpLS/TpRS: Top L/R Surround, HADB: LSd/RSd: L/R Surround direct\n"
"bits4-7\t\t: reserved\n";

//NOTE: The SAB fields definitions are shared with
//      RMCD: Room Configuration Data Block: SPM and
//      HADB: HDMI Audio Data Block
extern const gpfld_dsc_t SAB_SPM_fields;
extern const edi_field_t SAB_SPM_fld_dsc[];

const gpfld_dsc_t SAB_SPM_fields = {
   .flags    = 0,
   .dat_sz   = sizeof(sab_t),
   .inst_cnt = 1,
   .fcount   = 24,
   .fields   = SAB_SPM_fld_dsc
};

const edi_field_t SAB_SPM_fld_dsc[] = {
   //SAB byte 0
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm0)+1, 0, 1, F_BIT, 0, 1, "FL_FR",
   "Front Left/Right " },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm0)+1, 1, 1, F_BIT, 0, 1, "LFE1",
   "LFE Low Frequency Effects 1 (subwoofer1)" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm0)+1, 2, 1, F_BIT, 0, 1, "FC",
   "Front Center" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm0)+1, 3, 1, F_BIT, 0, 1, "BL_BR",
   "Back Left/Right" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm0)+1, 4, 1, F_BIT, 0, 1, "BC",
   "Back Center" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm0)+1, 5, 1, F_BIT, 0, 1, "FLC_FRC",
   "Front Left/Right of Center" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm0)+1, 6, 1, F_BIT, 0, 1, "rsvd0_6",
   "bit6: reserved in CTA-861-H, was RLC/RRC: Rear Left/Right of Center\n" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm0)+1, 7, 1, F_BIT, 0, 1, "FLW_FRW",
   "Front Left/Right Wide" },
   //SAB byte 1
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm1)+1, 0, 1, F_BIT, 0, 1, "TpFL_TpFR",
   "Top Front Left/Right" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm1)+1, 1, 1, F_BIT, 0, 1, "TpC",
   "Top Center" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm1)+1, 2, 1, F_BIT, 0, 1, "TpFC",
   "Top Front Center" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm1)+1, 3, 1, F_BIT, 0, 1, "LS_RS",
   "Left/Right Surround" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm1)+1, 4, 1, F_BIT, 0, 1, "LFE2",
   "LFE Low Frequency Effects 2 (subwoofer2)" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm1)+1, 5, 1, F_BIT, 0, 1, "TpBC",
   "Top Back Center" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm1)+1, 6, 1, F_BIT, 0, 1, "SiL_SiR",
   "Side Left/Right" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm1)+1, 7, 1, F_BIT, 0, 1, "TpSiL_TpSiR",
   "Top Side Left/Right" },
   //SAB byte 2
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm2)+1, 0, 1, F_BIT, 0, 1, "TpBL_TpBR",
   "Top Back Left/Right" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm2)+1, 1, 1, F_BIT, 0, 1, "BtFC",
   "Bottom Front Center" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm2)+1, 2, 1, F_BIT, 0, 1, "BtFL_BtFR",
   "Bottom Front Left/Right" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm2)+1, 3, 1, F_BIT, 0, 1, "rsvd2_3",
   "byte2.bit3: reserved in CTA-861-H, was TpLS/TpRS: Top Left/Right Surround,\n"
   "HADB: (HDMI Audio Data Block): LSd/RSd: Left/Right Surround direct" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm2)+1, 4, 1, F_BIT|F_RD, 0, 1, "rsvd2_4",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm2)+1, 5, 1, F_BIT|F_RD, 0, 1, "rsvd2_5",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm2)+1, 6, 1, F_BIT|F_RD, 0, 1, "rsvd2_6",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(sab_t, spm2)+1, 7, 1, F_BIT|F_RD, 0, 1, "rsvd2_7",
   "reserved (0)" },
// HADB: ACAT fields in byte 3:
   {&EDID_cl::BitF8Val, 0, offsetof(sab_t, spm2)+2, 0, 4, F_BFD, 0, 3, "res3_04",
   "reserved (0)" },
   {&EDID_cl::BitF8Val, 0, offsetof(sab_t, spm2)+2, 4, 4, F_BFD|F_INT, 0, 3, "ACAT",
   "Audio Channel Allocation Type:\ndefines which speakers should be allocated\n\n"
   "0b0001: (1) ITU-R BS.2159-4, Type B 10.2 channels:\n"
   "FL/FR, LFE1, FC, BL/BR, TpFL/TpFR, LS/RS, LFE2, TpBC\n\n"
   "0b0010: (2) SMPTE 2036-2, 22.2 channels:\n"
   "all speakers except: FLW/FRW, LS/LR, TpLS/TpRS, LSd/RSd\n\n"
   "0b0011: (3) IEC 62574 v1.0, 30.2 channels\n"
   "all speakers should be allocated\n\n"
   "Other values are reserved\n" }
};

const dbc_flatgp_dsc_t cea_sab_cl::SAB_grp = {
   .CodN     = "SAB",
   .Name     = "Speaker Allocation Block",
   .Desc     = SAB_SPM_Desc,
   .type_id  = ID_SAB,
   .flags    = 0,
   .min_len  = sizeof(sab_t),
   .max_len  = sizeof(sab_t),
   .max_fld  = (32 - sizeof(sab_t) - sizeof(bhdr_t) + 24),
   .hdr_fcnt = CEA_DBCHDR_FCNT,
   .hdr_sz   = sizeof(bhdr_t),
   .fld_arsz = 1,
   .fld_ar   = &SAB_SPM_fields
};

rcode cea_sab_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   retU = base_DBC_Init_FlatGrp(inst, &SAB_grp, orflags, parent);
   return retU;
}

//VDTC: VESA Display Transfer Characteristic Data Block (gamma)
const char  cea_vdtc_cl::Desc[] =
"Gamma value for the display device.\n"
"The format is exactly the same as in EDID block 0, byte 23";

const gpfld_dsc_t cea_vdtc_cl::fields[] = {
   {
      .flags    = 0,
      .dat_sz   = sizeof(vtc_t),
      .inst_cnt = 1,
      .fcount   = 1,
      .fields   = cea_vdtc_cl::fld_dsc
   }
};

const edi_field_t cea_vdtc_cl::fld_dsc[] = {
   {&EDID_cl::Gamma, 0, offsetof(vtc_t, gamma)+1, 0, 1, F_FLT|F_NI, 0, 255, "gamma",
   "Byte value = (gamma*100)-100 (range 1.00–3.54)" }
};

const dbc_flatgp_dsc_t cea_vdtc_cl::VDTC_grp = {
   .CodN     = "VDTC",
   .Name     = "VESA Display Transfer Characteristic",
   .Desc     = Desc,
   .type_id  = ID_VDTC,
   .flags    = 0,
   .min_len  = sizeof(vtc_t),
   .max_len  = sizeof(vtc_t),
   .max_fld  = (31 + CEA_DBCHDR_FCNT),
   .hdr_fcnt = CEA_DBCHDR_FCNT,
   .hdr_sz   = sizeof(bhdr_t),
   .fld_arsz = 1,
   .fld_ar   = cea_vdtc_cl::fields
};

rcode cea_vdtc_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   retU = base_DBC_Init_FlatGrp(inst, &VDTC_grp, orflags, parent);
   return retU;
}

//UNK-TC: Unknown Data Block (Tag Code)
const dbc_flatgp_dsc_t cea_unktc_cl::UNK_TC_grp = {
   .CodN     = "UNK-TC",
   .Name     = "Unknown Data Block",
   .Desc     = "Unknown Tag Code",
   .type_id  = ID_CEA_UTC,
   .flags    = 0,
   .min_len  = 0,
   .max_len  = 31,
   .max_fld  = (31 + CEA_DBCHDR_FCNT),
   .hdr_fcnt = CEA_DBCHDR_FCNT,
   .hdr_sz   = sizeof(bhdr_t),
   .fld_arsz = 0,
   .fld_ar   = NULL
};

rcode cea_unktc_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   retU = base_DBC_Init_FlatGrp(inst, &UNK_TC_grp, orflags, parent);
   return retU;
}

//UNK-DAT Unknown data bytes, subgroup.
const char  cea_unkdat_cl::CodN[] = "UNK-DAT";
const char  cea_unkdat_cl::Name[] = "Unknown data bytes";
const char  cea_unkdat_cl::Desc[] = "Unknown data bytes";

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode cea_unkdat_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode        retU;
   u32_t        dlen;

   //variable size: dat_sz must be set before init() !!
   dlen        = dat_sz;
   parent_grp  = parent;
   type_id.t32 = ID_CEA_UDAT | T_SUB_GRP | T_NO_MOVE; //unknown data sub-group + parent group ID

   CopyInstData(inst, dlen);

   //pre-alloc buffer for array of fields: hdr_fcnt + dlen
   dyn_fldar = (edi_field_t*) malloc( dlen * EDI_FIELD_SZ );
   if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);

   dyn_fcnt  = dlen;

   //payload data interpreted as unknown
   insert_unk_byte(dyn_fldar, dyn_fcnt, 0);

   retU = init_fields(&dyn_fldar[0], inst_data, dyn_fcnt, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

//Universal Init function for DBC blocks with subgroups
rcode edi_grp_cl::base_DBC_Init_RootGrp(const u8_t* inst, const dbc_root_dsc_t *pGDsc, u32_t orflags, edi_grp_cl* parent) {
   rcode        retU, retU2;
   u32_t        dlen;
   u32_t        dlen2;
   u32_t        ahf_count;
   u32_t        offs;
   u32_t        buflen;
   u32_t        gr_idx;
   u32_t        gr_inst;
   gtid_t       sub_id;
   const u8_t  *pgrp_inst;
   bool         b_edit_mode;
   GroupAr_cl  *subgrp_ar;

   //NOTE: All the fault messages are logged, but ignored -> edi_grp_cl::base_clone()

   RCD_SET_OK(retU);
   RCD_SET_OK(retU2);
   b_edit_mode = (T_MODE_EDIT & orflags);

   dlen  = reinterpret_cast <const bhdr_t*> (inst)->tag.blk_len;
   dlen2 = dlen;
   if (dlen < pGDsc->min_len) {
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, pGDsc->CodN);
      if (! b_edit_mode) return retU2;
   }

   subgrp_ar   = getSubGrpAr();
   parent_grp  = parent;
   type_id.t32 = pGDsc->type_id;
   pgrp_inst   = inst;
   hdr_sz      = pGDsc->hdr_sz;
   ahf_sz      = pGDsc->ahf_sz;
   ahf_count   = pGDsc->ahf_cnt;

   dlen   += sizeof(bhdr_t); //+1 DBC header
   buflen  = (b_edit_mode) ? 32 : dlen;
   memcpy(inst_data, inst, buflen);
   dat_sz  = dlen;
   offs    = hdr_sz;

   if (dlen >= hdr_sz) {
      dlen  -= hdr_sz;
   } else {
      dlen  -= 1;
   }

   if (dlen < pGDsc->ahf_sz) {
      ahf_count = 0;
      pgrp_inst = &inst[offs]; //offs for UNK-DAT
      goto _blk_len_err;
   }

   dlen      -= pGDsc->ahf_sz;
   offs      += pGDsc->ahf_sz;
   pgrp_inst += offs;

   sub_id      = type_id; //some sub-grps need to know parent's subg_id, e.g. T8VTDB, T10VTDB
   sub_id.t32 |= orflags;
   sub_id.t_md_edit = 0;
   sub_id.t_sub_gp  = 1;

   for (gr_idx=0; gr_idx<pGDsc->grp_arsz; ++gr_idx) {
      const dbc_subg_dsc_t *pSubGDsc;

      edi_grp_cl *pgrp;
      u32_t       dsz;
      u32_t       min_sz;

      pSubGDsc = &pGDsc->grp_ar[gr_idx];
      min_sz   = pSubGDsc->min_len;
      //invalid grp desc >> infinite loop if (min_sz==0) && (inst_cnt==-1)
      if (0 == min_sz) RCD_RETURN_FAULT(retU);

      //num of sub-group instances to spawn, if inst_cnt==(-1) -> until data end
      for (gr_inst=0; gr_inst<(u32_t)pSubGDsc->inst_cnt; ++gr_inst) {

         if (dlen < pSubGDsc->min_len) {
            if (dlen > 0) {
               //no space left for next sub-group
               wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, pGDsc->CodN);
            }
            goto payload;
         }

         subg_sz = dlen; //free space left for subgroup->init()
         pgrp    = pSubGDsc->s_ctor();
         if (NULL == pgrp) RCD_RETURN_FAULT(retU);

         retU2 = pgrp->init(pgrp_inst, sub_id.t32, this);
         if (! RCD_IS_OK(retU2)) {
            if (retU2.detail.rcode > RCD_FVMSG) {
               wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, pGDsc->CodN);
               delete pgrp;
               goto payload;
            }
            //else VMSG from sub-group
         }

         pgrp->setRelOffs(offs);
         pgrp->setAbsOffs(offs + abs_offs);
         subgrp_ar->Append(pgrp);

         dsz        = pgrp->getTotalSize();
         offs      += dsz;
         pgrp_inst += dsz;
         dlen      -= dsz;
      }
   }

payload:
   if (dlen2 > pGDsc->max_len) {
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, pGDsc->CodN);
   }

_blk_len_err:
   //unspecified payload: interpreted as unknown sub-group.
   if (dlen > 0) {
      orflags = type_id.t32 | T_NO_MOVE;
      retU    = Append_UNK_DAT(pgrp_inst, dlen, orflags, (offs + abs_offs), offs, this);
      if ( (! RCD_IS_OK(retU)) && !b_edit_mode ) return retU;
   }

   subgrp_ar->CalcDataSZ(this);

   retU = init_fields(&CEA_BlkHdr_fields[0], inst_data, pGDsc->hdr_fcnt, false,
                      pGDsc->Name, pGDsc->Desc, pGDsc->CodN);
   if (! RCD_IS_OK(retU)) return retU;

   if (ahf_count != 0) {
      bool append_md = true;
      //append and init additional fields in root group header
      retU = init_fields(pGDsc->ah_flds, inst_data, ahf_count, append_md);
   }

   if (RCD_IS_OK(retU)) return retU2;
   return retU;
}

//Universal Init function for DBC blocks (no sub-groups)
rcode edi_grp_cl::base_DBC_Init_FlatGrp(const u8_t* inst, const dbc_flatgp_dsc_t *pGDsc, u32_t orflags, edi_grp_cl* parent) {
   rcode        retU, retU2;
   u32_t        dlen;
   u32_t        dlen2;
   u32_t        offs;
   u32_t        fcnt;
   u32_t        buflen;
   u32_t        fldgr_idx;
   u32_t        gr_inst;
   edi_field_t *p_fld;
   bool         b_edit_mode;

   //NOTE: edi_grp_cl::base_clone(): All the fault messages are logged, but ignored.

   RCD_SET_OK(retU);
   RCD_SET_OK(retU2);
   b_edit_mode  = (0 != (T_MODE_EDIT & orflags));

   dlen  = reinterpret_cast <const bhdr_t*> (inst)->tag.blk_len;
   dlen2 = dlen;
   if (dlen < pGDsc->min_len) {
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, pGDsc->CodN);
      if (! b_edit_mode) return retU2;
   }

   parent_grp  = parent;
   type_id.t32 = pGDsc->type_id;

   dlen   += sizeof(bhdr_t); //+1 DBC header
   buflen  = (b_edit_mode) ? 32 : dlen;
   memcpy(inst_data, inst, buflen); //Local data copy
   dat_sz  = dlen;
   if (dlen >= pGDsc->hdr_sz) {
      dlen  -= pGDsc->hdr_sz;
   } else {
      dlen  -= 1;
   }

   //max fields:
   dyn_fldar = (edi_field_t*) malloc( pGDsc->max_fld * EDI_FIELD_SZ );
   if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);
   dyn_fcnt  = pGDsc->hdr_fcnt;
   p_fld     = dyn_fldar;

   //insert DBC header: pGDsc->hdr_sz defines header type
   memcpy( p_fld, CEA_BlkHdr_fields, (pGDsc->hdr_fcnt * EDI_FIELD_SZ) );
   p_fld += pGDsc->hdr_fcnt;
   offs   = pGDsc->hdr_sz;

   for (fldgr_idx=0; fldgr_idx<pGDsc->fld_arsz; ++fldgr_idx) {
      const gpfld_dsc_t *pgFld;
      u32_t dsz;

      pgFld = &pGDsc->fld_ar[fldgr_idx];

      if ( (T_FLEX_LAYOUT & pgFld->flags) != 0) {
         retU2 = base_DBC_Init_GrpFields(pgFld, &p_fld, &dlen, &offs);
         if (! RCD_IS_OK(retU2)) {
            wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, pGDsc->CodN);
            goto _payload;
         }
         if (RCD_IS_TRUE(retU2)) {
            continue;
         }
      }

      fcnt = pgFld->fcount;
      dsz  = pgFld->dat_sz;

      //num of sub-group instances to spawn, if inst_cnt==(-1) -> until data end
      for (gr_inst=0; gr_inst<(u32_t)pgFld->inst_cnt; ++gr_inst) {
         bool  b_flex;

         if (dlen < dsz) {
            b_flex  = ((T_FLEX_LAYOUT & pGDsc->flags)  != 0);
            b_flex |= (                  pgFld->inst_cnt < 0);
            if (! b_flex) {
               wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, pGDsc->CodN);
            }
            goto _payload; //fixed blk size mismatch -> unknown data
         }

         b_flex = ( (T_FLEX_OFFS & pgFld->flags) != 0);

         memcpy( p_fld, pgFld->fields, (fcnt * EDI_FIELD_SZ) );

         if (b_flex) { //update field offsets: all fields have to be of size ==1
            edi_field_t *p_dstf;

            p_dstf = p_fld;
            for (u32_t fidx=0; fidx<fcnt; ++fidx) {
               p_dstf->offs = (offs + fidx);
               p_dstf      ++ ;
            }
         }

         p_fld    += fcnt;
         dyn_fcnt += fcnt;
         dlen     -= dsz;
         offs     += dsz;
      }
   }

_payload:
   if (dlen2 > pGDsc->max_len) {
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, pGDsc->CodN);
   }

   //unspecified payload: interpreted as unknown bytes.
   if (dlen > 0) {
      fcnt = (dyn_fcnt + dlen);

      if (fcnt > pGDsc->max_fld) {
         edi_field_t* t_buf;

         //This can happen due to EDID error or in b_edit_mode,
         //if user enters invalid block length
         t_buf = (edi_field_t*) realloc(dyn_fldar, (fcnt * EDI_FIELD_SZ));
         if (NULL == t_buf) RCD_RETURN_FAULT(retU);

         dyn_fldar = t_buf;
         p_fld     = dyn_fldar;
         p_fld    += dyn_fcnt; //append unk. bytes after last valid field
      }

      dyn_fcnt = fcnt;

      insert_unk_byte(p_fld, dlen, offs);
   }
   retU = init_fields(&dyn_fldar[0], inst_data, dyn_fcnt, false,
                      pGDsc->Name, pGDsc->Desc, pGDsc->CodN);

   if (RCD_IS_OK(retU)) return retU2;
   return retU;
}

rcode edi_grp_cl::base_DBC_Init_GrpFields(const gpfld_dsc_t *pgFld, edi_field_t **pp_fld, u32_t *pdlen, u32_t *poffs) {
   rcode  retU;
   u32_t  dlen   = *pdlen;
   u32_t  offs   = *poffs;
   u32_t  nfld   = 0;
   u32_t  grlen  = 0;
   bool   b_offs = ( (T_FLEX_OFFS & pgFld->flags) != 0);

         edi_field_t *p_fld  = *pp_fld;
   const edi_field_t *p_srcf = &pgFld->fields[0];

   for (; nfld<pgFld->fcount; ++nfld) {
      u32_t  fldlen;

      fldlen  = p_srcf->fld_sz;

      if (dlen < fldlen) break;

      memcpy( p_fld, p_srcf, EDI_FIELD_SZ );
      if (b_offs) { //update fields offsets:
         p_fld->offs = offs;
      }
      dlen  -= fldlen;
      grlen += fldlen;
      offs  += fldlen;

      p_srcf ++ ;
      p_fld  ++ ;
   }

   RCD_SET_TRUE(retU);

   if (grlen != pgFld->dat_sz) {
      bool  b_flex;
      b_flex  = ((T_FLEX_LEN & pgFld->flags) != 0);
      if (! b_flex) {
         RCD_SET_FAULT(retU); //Bad length
      }
   }

   *pp_fld   = p_fld;
   *poffs    = offs;
   *pdlen    = dlen;
   dyn_fcnt += nfld;

   return retU;
}

