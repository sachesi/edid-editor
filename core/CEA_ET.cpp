/***************************************************************
 * Name:      CEA_ET_class.cpp
 * Purpose:   CEA/CTA-861-G Extended Tag Codes
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2019-2025
 * License:   GPLv3+
 **************************************************************/

#include "debug.h"
#include "rcdunits.h"
#ifndef idCEA_ET
   #error "CEA_ET_class.cpp: missing unit ID"
#endif
#define RCD_UNIT idCEA_ET
#include "rcode/rcode.h"

#include "wxedid_rcd_scope.h"

RCD_AUTOGEN_DEFINE_UNIT

#include <stddef.h>

#include "vmap.h"
#include "CEA.h"
#include "CEA_class.h"
#include "CEA_ET_class.h"
#include "EDID_shared.h"

//CEA bad block length msg (defined in CEA.cpp)
extern const char ERR_GRP_LEN_MSG[];

//CEA header field descriptions (defined in CEA.cpp)
extern const char CEA_BlkHdr_dscF0[];
extern const char CEA_BlkHdr_dscF1[];
extern const char CEA_BlkHdr_dscF2[];
extern const edi_field_t CEA_BlkHdr_fields[];

//CEA VDB video mode map (svd_vidfmt.h)
extern sm_vmap SVD_vidfmt_map;

//unknown/invalid byte field (defined in CEA.cpp)
extern const edi_field_t unknown_byte_fld;
extern void insert_unk_byte(edi_field_t *p_fld, u32_t len, u32_t s_offs);

//SPM Speaker Presence Mask: shared description (defined in CEA.cpp)
extern const char SAB_SPM_Desc[];
//SPM Speaker Presence Mask: shared field descriptors (SAB: defined in CEA.cpp)
extern const gpfld_dsc_t SAB_SPM_fields;

//-------------------------------------------------------------------------------------------------
//CEA-DBC Extended Tag Codes

//VCDB: Video Capability Data Block (DBC_ET_VCDB = 0)
const char  cea_vcdb_cl::Desc[] =
"VCDB is used to declare display capability to overscan/underscan and the quantization range.";

const gpfld_dsc_t cea_vcdb_cl::fields = {
   .flags    = 0,
   .dat_sz   = 1,
   .inst_cnt = 1,
   .fcount   = 5,
   .fields   = cea_vcdb_cl::fld_dsc
};

const edi_field_t cea_vcdb_cl::fld_dsc[] = {
   //VCDB data
   {&EDID_cl::BitF8Val, 0, 2, 0, 2, F_BFD|F_INT, 0, 3, "S_CE01",
   "CE overscan/underscan:\n0= not supported\n1= always overscan\n"
   "2= always underscan\n3= both supported" },
   {&EDID_cl::BitF8Val, 0, 2, 2, 2, F_BFD|F_INT, 0, 3, "S_IT01",
   "IT overscan/underscan:\n0= not supported\n1= always overscan\n"
   "2= always underscan\n3= both supported" },
   {&EDID_cl::BitF8Val, 0, 2, 4, 2, F_BFD|F_INT, 0, 3, "S_PT01",
   "PT overscan/underscan:\n0= not supported\n1= always overscan\n"
   "2= always underscan\n3= both supported" },
   {&EDID_cl::BitVal, 0, 2, 6, 1, F_BIT, 0, 3, "QS",
   "Quantization Range Selectable:\n1= selectable via AVI Q (RGB only), 0= no data" },
   {&EDID_cl::BitVal, 0, 2, 7, 1, F_BIT, 0, 3, "QY",
   "Quantization Range:\n1= selectable via AVI YQ (YCC only), 0= no data" }
};

const dbc_flatgp_dsc_t cea_vcdb_cl::VCDB_grp = {
   .CodN     = "VCDB",
   .Name     = "Video Capability Data Block",
   .Desc     = Desc,
   .type_id  = ID_VCDB,
   .flags    = 0,
   .min_len  = 2,
   .max_len  = 2,
   .max_fld  = (32 - sizeof(vcdb_t) - sizeof(ethdr_t) + 5),
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .fld_arsz = 1,
   .fld_ar   = &fields
};

rcode cea_vcdb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   retU = base_DBC_Init_FlatGrp(inst, &VCDB_grp, orflags, parent);
   return retU;
}

//VSVD: Vendor-Specific Video Data Block (DBC_ET_VSVD =  1)
const char cea_vsvd_cl::Desc[] = "Vendor-Specific Video Data Block: undefined data.";

//fields shared with VSAD
static const edi_field_t fld_dsc[] = {
   //VSVD data
   {&EDID_cl::ByteStr, 0, 2, 0, 3, F_STR|F_HEX|F_LE|F_RD, 0, 0xFFFFFF, "IEEE-OUI",
   "IEEE OUI (Organizationally Unique Identifier)" }
};

static const gpfld_dsc_t VSVD_VSAD_fields = {
   .flags    = 0,
   .dat_sz   = 3,
   .inst_cnt = 1,
   .fcount   = 1,
   .fields   = fld_dsc
};

const dbc_flatgp_dsc_t cea_vsvd_cl::VSVD_grp = {
   .CodN     = "VSVD",
   .Name     = "Vendor-Specific Video Data Block",
   .Desc     = Desc,
   .type_id  = ID_VSVD,
   .flags    = 0,
   .min_len  = (sizeof(vs_vadb_t) +1),
   .max_len  = 31,
   .max_fld  = (32 - sizeof(vs_vadb_t) - sizeof(ethdr_t) + 1),
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .fld_arsz = 1,
   .fld_ar   = &VSVD_VSAD_fields
};

rcode cea_vsvd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   retU = base_DBC_Init_FlatGrp(inst, &VSVD_grp, orflags, parent);
   return retU;
}

//VDDD: VESA Display Device Data Block (DBC_ET_VDDD = 2)
//VDDD: IFP: Interface properties
const char  vddd_iface_cl::CodN[] = "IFP";
const char  vddd_iface_cl::Name[] = "Interface properties";
const char  vddd_iface_cl::Desc[] = "Interface properties";

const edi_field_t vddd_iface_cl::fields[] = {
   //if_type
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, if_type), 4, 4, F_BFD|F_INT, 0, 0xC, "if_type",
   "Interface Type Code: (num of channels/sub-type)\n"
   "0x0= analog (sub-type in 'n_lanes' field)\n"
   "0x1= LVDS (any)\n"
   "0x2= RSDS (any)\n"
   "0x3= DVI-D (1 or 2)\n"
   "0x4= DVI-I, analog section (0)\n"
   "0x5= DVI-I, digital section (1 or 2)\n"
   "0x6= HDMI-A (1)\n"
   "0x7= HDMI-B (2)\n"
   "0x8= MDDI (1 or 2)\n"
   "0x9= DisplayPort (1,2 or 4)\n"
   "0xA= IEEE-1394 (0)\n"
   "0xB= M1, analog (0)\n"
   "0xC= M1, digital (1 or 2)\n" },
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, if_type), 0, 4, F_BFD|F_INT, 0, 4, "n_lanes",
   "Number of lanes/channels provided by the interface.\n"
   "If interface type is 'analog' (0), then 'n_lanes' is interpreted as follows:\n"
   "0= 15HD/VGA (VESA EDDC std. pinout)\n"
   "1= VESA NAVI-V (15HD)\n"
   "2= VESA NAVI-D" },
   //iface standard version and release
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, if_version), 4, 4, F_BFD|F_INT, 0, 0xF, "version",
   "Interface version number" },
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, if_version), 0, 4, F_BFD|F_INT, 0, 0xF, "release",
   "Interface release number" },
   //min/max clock frequency for each interface link/channel
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, clkfrng), 2, 6, F_BFD|F_INT|F_MHZ, 0, 63, "if_fmin",
   "Min clock frequency for each interface link/channel" },
   {&EDID_cl::VDDD_IF_MaxF, 0, offsetof(vdddb_t, clkfrng), 0, 2, F_BFD|F_INT|F_MHZ, 0, 63, "if_fmax",
   "Max clock frequency for each interface link/channel" }
};

rcode vddd_iface_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 6
   };

   rcode  retU;

   parent_grp  = parent;
   type_id.t32 = ID_VDDD_IPF | orflags;

   retU = init_fields(&fields[0], inst, fcount, false, Name, Desc, CodN);
   return retU;
}

//VDDD: CPT: Content Protection
const char  vddd_cprot_cl::CodN[] = "CPT";
const char  vddd_cprot_cl::Name[] = "Content Protection";
const char  vddd_cprot_cl::Desc[] = "Content Protection";

const edi_field_t vddd_cprot_cl::fields[] = {
   //Content Protection support method
   {&EDID_cl::ByteVal, 0, offsetof(vdddb_t, cont_prot), 0, 1, F_BTE|F_INT, 0, 3, "ContProt",
   "Content Protection supported method:\n"
   "0= none supported\n"
   "1= HDCP\n"
   "2= DTCP\n"
   "3= DPCP (DisplayPort)" }
};

rcode vddd_cprot_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 1
   };

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_VDDD_CPT | orflags;

   retU = init_fields(&fields[0], inst, fcount, false, Name, Desc, CodN);
   return retU;
}

//VDDD: AUPR: Audio properties
const char  vddd_audio_cl::CodN[] = "AUPR";
const char  vddd_audio_cl::Name[] = "Audio properties";
const char  vddd_audio_cl::Desc[] = "Audio properties";

const edi_field_t vddd_audio_cl::fields[] = {
   //Audio flags
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, audioflg), 0, 5, F_BFD|F_RD, 0, 0x7, "resvd04",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(vdddb_t, audioflg), 5, 1, F_BIT, 0, 1, "audio_ovr",
   "audio input override, 1= automatically override other audio inputs" },
   {&EDID_cl::BitVal, 0, offsetof(vdddb_t, audioflg), 6, 1, F_BIT, 0, 1, "sep_achn",
   "separate audio channel inputs: not via video interface" },
   {&EDID_cl::BitVal, 0, offsetof(vdddb_t, audioflg), 7, 1, F_BIT, 0, 1, "vid_achn",
   "1= audio support on video interface" },
   //Audio delay
   {&EDID_cl::VDDD_AudioDelay, 0, offsetof(vdddb_t, audiodly), 0, 7, F_BFD|F_INT|F_MLS, 0, 254, "delay",
   "Audio delay in 2ms resolution.\n"
   "If dly> 254ms, the stored value is 254/2=127, i.e. 254ms is max,\n"
   "0= no delay compensation" },
   {&EDID_cl::BitVal, 0, offsetof(vdddb_t, audiodly), 7, 1, F_BIT, 0, 1, "dly_sign",
   "Audio delay sign: 1= '+', 0= '-'" }
};

rcode vddd_audio_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 6
   };

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_VDDD_AUD | orflags;

   retU = init_fields(&fields[0], inst, fcount, false, Name, Desc, CodN);
   return retU;
}

//VDDD: DPR: Display properties
const char  vddd_disp_cl::CodN[] = "DPR";
const char  vddd_disp_cl::Name[] = "Display properties";
const char  vddd_disp_cl::Desc[] = "Display properties";

const edi_field_t vddd_disp_cl::fields[] = {
   //H/V pixel count
   {&EDID_cl::VDDD_HVpix_cnt, 0, offsetof(vdddb_t, Hpix_cnt), 2, 6, F_INT|F_PIX, 1, 0x10000, "Hpix_cnt",
   "Count of pixels in horizontal direction: 1-65536" },
   {&EDID_cl::VDDD_HVpix_cnt, 0, offsetof(vdddb_t, Vpix_cnt), 0, 2, F_INT|F_PIX, 1, 0x10000, "Vpix_cnt",
   "Count of pixels in vertical direction: 1-65536" },
   //Aspect Ratio
   {&EDID_cl::VDDD_AspRatio, 0, offsetof(vdddb_t, aspect), 0, 1, F_FLT|F_NI, 0, 0xFF, "AspRatio",
   "Aspect ratio: stored value: 100*((long_axis/short_axis)-1)" },
   //Scan Direction, Position of "zero" pixel, Display Rotation, Orientation
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, scanrot), 0, 2, F_BFD|F_INT, 0, 3, "scan_dir",
   "Scan Direction:\n"
   "0= undefined\n"
   "1= 'fast' along long axis, 'slow' along short axis\n"
   "2= 'fast' along short axis, 'slow' along long axis\n"
   "3= reserved" },
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, scanrot), 2, 2, F_BFD|F_INT, 0, 3, "pix0_pos",
   "Position of 'zero' pixel:\n"
   "0= upper left corner\n"
   "1= upper right corner\n"
   "2= lower left corner\n"
   "3= lower right corner" },
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, scanrot), 4, 2, F_BFD|F_INT, 0, 3, "rotation",
   "Possible display rotation:\n"
   "0= not possible\n"
   "1= 90 degrees clockwise\n"
   "2= 90 degrees counterclockwise\n"
   "3= both 1 & 2 possible" },
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, scanrot), 6, 2, F_BFD|F_INT, 0, 3, "orientation",
   "Display Orientation:\n"
   "0= landscape\n"
   "1= portrait\n"
   "2= not fixed (can be rotated)\n"
   "3= undefined" },
   //sub-pixel layout code
   {&EDID_cl::ByteVal, 0, offsetof(vdddb_t, cont_prot), 0, 1, F_BTE|F_INT, 0, 0xC, "subpixlc",
   "sub-pixel layout code:\n"
   "0x0= undefined\n"
   "0x1= RGB V-stripes\n"
   "0x2= RGB H-stripes\n"
   "0x3= RGB V-stripes, primary ordering matches chromaticity info in base EDID\n"
   "0x4= RGB H-stripes, primary ordering matches chromaticity info in base EDID\n"
   "0x5= 2x2 quad: red@top-left, blue@btm-right, remaining 2pix are green\n"
   "0x6= 2x2 quad: red@btm-left, blue@top-right, remaining 2pix are green\n"
   "0x7= delta (triad)\n"
   "0x8= mosaic\n"
   "0x9= 2x2 quad: 3 RGB + 1 extra color (*)\n"
   "0xA= 3 RGB + 2 extra colors below or above the RGB set (*)\n"
   "0xB= 3 RGB + 3 extra colors below or above the RGB set (*)\n"
   "0xC= Clairvoyante, Inc. PenTile Matrix(TM)\n\n"
   "(*) Additional sub-pixel's colors should be described in Additional Chromaticity Coordinates"
   "srtucture of this block (DDDB Std, section 2.16)" },
   //horizontal/vertical pixel pitch
   {&EDID_cl::VDDD_HVpx_pitch, 0, offsetof(vdddb_t, Hpix_pitch), 0, 1, F_FLT|F_NI|F_MM, 0, 0xFF, "Hpix_pitch",
   "horizontal pixel pitch in 0.01mm (0.00 - 2.55)" },
   {&EDID_cl::VDDD_HVpx_pitch, 0, offsetof(vdddb_t, Vpix_pitch), 0, 1, F_FLT|F_NI|F_MM, 0, 0xFF, "Vpix_pitch",
   "vertical pixel pitch in 0.01mm (0.00 - 2.55)" },
   //miscellaneous display caps.
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, misccaps), 0, 3, F_BFD|F_RD, 0, 0x7, "resvd02",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(vdddb_t, misccaps), 3, 1, F_BIT, 0, 1, "deinterlacing",
   "deinterlacing, 1= possible" },
   {&EDID_cl::BitVal, 0, offsetof(vdddb_t, misccaps), 4, 1, F_BIT, 0, 1, "ovrdrive",
   "overdrive not recommended, 1= src 'should' not overdrive the signal by default" },
   {&EDID_cl::BitVal, 0, offsetof(vdddb_t, misccaps), 5, 1, F_BIT, 0, 1, "d_drive",
   "direct-drive, 0= no direct-drive" },
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, misccaps), 6, 2, F_BFD, 0, 0x7, "dither",
   "dithering, 0=no, 1=spatial, 2=temporal, 3=both" },
   //Frame rate and mode conversion
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, frm_rcnv), 0, 6, F_BFD|F_INT, 0, 0x3F, "frm_delta",
   "Frame rate delta range:\n"
   "max FPS deviation from default frame rate (+/- 63 FPS)" },
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, frm_rcnv), 6, 2, F_BFD|F_INT, 0, 3, "conv_mode",
   "available conversion mode:\n"
   "0= none\n"
   "1= single buffer\n"
   "2= double buffer\n"
   "3= other, f.e. interframe interpolation" },
   {&EDID_cl::ByteVal, 0, offsetof(vdddb_t, frm_rcnv), 1, 1, F_BTE|F_INT, 0, 0xFF, "frm_rate",
   "default frame rate (FPS)" },
   //Color bit depth
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, colbitdpth), 0, 4, F_BFD|F_INT, 0, 0xF, "cbd_dev",
   "color bit depth: display device: value=cbd-1 (1-16)" },
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, colbitdpth), 4, 4, F_BFD|F_INT, 0, 0xF, "cbd_iface",
   "color bit depth: interface: value=cbd-1 (1-16)" },
   //Display response time
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, resp_time), 0, 7, F_BFD|F_INT|F_MLS, 0, 0x7F, "resp_time",
   "Display response time in milliseconds, max 126. 127 means greater than 126, but unspecified" },
   {&EDID_cl::BitVal, 0, offsetof(vdddb_t, resp_time), 7, 1, F_BIT, 0, 1, "resp_type",
   "Display response time type:\n"
   "1=black-to-white\n"
   "0=white-to-black" },
   //Overscan
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, overscan), 0, 4, F_BFD|F_INT|F_PCT, 0, 0xF, "V_overscan",
   "Vertical overscan, 0-15%, 0= no overscan (but doesn't mean 100% image match)" },
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, overscan), 4, 4, F_BFD|F_INT|F_PCT, 0, 0xF, "H_overscan",
   "Horizontal overscan, 0-15%, 0= no overscan (but doesn't mean 100% image match)" }
};

rcode vddd_disp_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 24
   };

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_VDDD_DPR | orflags;

   retU = init_fields(&fields[0], inst, fcount, false, Name, Desc, CodN);
   return retU;
}

//VDDD: CXY: Additional Chromaticity coords
const char  vddd_cxy_cl::CodN[] = "CXY";
const char  vddd_cxy_cl::Name[] = "Additional Chromaticity coords";
const char  vddd_cxy_cl::Desc[] = "Chromaticity coords for additional primary color sub-pixels";

const edi_field_t vddd_cxy_cl::fields[] = {
   //Additional Chromaticity Coordinates
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, addchrxy)+1, 0, 2, F_BFD|F_INT, 0, 0xF, "n_colors",
   "Number of additional chromaticity coordinates for primary colors 4-6\n"
   "Standard RGB primary colors are numbered 1-3 and described in base EDID structure" },
   {&EDID_cl::BitF8Val, 0, offsetof(vdddb_t, addchrxy)+1, 2, 2, F_BFD|F_INT, 0, 0xF, "resvd32",
   "reserved (0)" },
   {&EDID_cl::CHredX, 0, offsetof(vdddb_t, addchrxy), 0, 10, F_BFD|F_FLT|F_NI, 0, 1, "col4_x", "" },
   {&EDID_cl::CHredY, 0, offsetof(vdddb_t, addchrxy), 0, 10, F_BFD|F_FLT|F_NI, 0, 1, "col4_y", "" },
   {&EDID_cl::CHgrnX, 0, offsetof(vdddb_t, addchrxy), 0, 10, F_BFD|F_FLT|F_NI, 0, 1, "col5_x", "" },
   {&EDID_cl::CHgrnY, 0, offsetof(vdddb_t, addchrxy), 0, 10, F_BFD|F_FLT|F_NI, 0, 1, "col5_y", "" },
   {&EDID_cl::CHbluX, 0, offsetof(vdddb_t, addchrxy), 0, 10, F_BFD|F_FLT|F_NI, 0, 1, "col6_x", "" },
   {&EDID_cl::CHbluY, 0, offsetof(vdddb_t, addchrxy), 0, 10, F_BFD|F_FLT|F_NI, 0, 1, "col6_y", "" }
};

rcode vddd_cxy_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 8
   };

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_VDDD_CXY | orflags;

   retU = init_fields(&fields[0], inst, fcount, false, Name, Desc, CodN);
   return retU;
}

//VDDD: VESA Display Device Data Block: base class
//NOTE: sub-groups data are not continuous within DBC.
const char  cea_vddd_cl::CodN[] = "VDDD";
const char  cea_vddd_cl::Name[] = "VESA Display Device Data Block";
const char  cea_vddd_cl::Desc[] = "Additional information about display device";

rcode cea_vddd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode       retU, retU2;
   u32_t       dlen;
   edi_grp_cl* pgrp;
   bool        b_edit_mode;

   RCD_SET_OK(retU2);

   parent_grp  = parent;
   type_id.t32 = ID_VDDD;

   CopyInstData(inst, sizeof(vdddb_t)); //32

   //create and init sub-groups: 5: IFP, CPT, AUD, DPR, CXY

   b_edit_mode  = (0 != (T_MODE_EDIT & orflags));

   dlen = reinterpret_cast <const ethdr_t*> (inst)->ehdr.hdr.tag.blk_len;
   if (dlen != 31) {
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, CodN);
      if (! b_edit_mode) return retU2;
      goto _unk;
   }

   //NOTE: vdddb_t contains the block header, so the inst ptr is the same for all sub-groups
   //      Local instance data are shared for all sub-groups.

   //IFP
   pgrp = new vddd_iface_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init(inst_data, (ID_VDDD | T_SUB_GRP | T_GRP_FIXED), this );
   if (! RCD_IS_OK(retU)) return retU;
   pgrp->setAbsOffs(abs_offs);
   subgroups.Append(pgrp);
   //CPT
   pgrp = new vddd_cprot_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init(inst_data, (ID_VDDD | T_SUB_GRP | T_GRP_FIXED), this );
   if (! RCD_IS_OK(retU)) return retU;
   pgrp->setAbsOffs(abs_offs);
   subgroups.Append(pgrp);
   //AUD
   pgrp = new vddd_audio_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init(inst_data, (ID_VDDD | T_SUB_GRP | T_GRP_FIXED), this );
   if (! RCD_IS_OK(retU)) return retU;
   pgrp->setAbsOffs(abs_offs);
   subgroups.Append(pgrp);
   //DPR
   pgrp = new vddd_disp_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init(inst_data, (ID_VDDD | T_SUB_GRP | T_GRP_FIXED), this );
   if (! RCD_IS_OK(retU)) return retU;
   pgrp->setAbsOffs(abs_offs);
   subgroups.Append(pgrp);
   //CXY
   pgrp = new vddd_cxy_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU = pgrp->init(inst_data, (ID_VDDD | T_SUB_GRP | T_GRP_FIXED), this );
   if (! RCD_IS_OK(retU)) return retU;
   pgrp->setAbsOffs(abs_offs);
   subgroups.Append(pgrp);

_unk:
   retU = init_fields(&CEA_BlkHdr_fields[0], inst_data, CEA_ETHDR_FCNT,
                      false, Name, Desc, CodN);

   if (! RCD_IS_OK(retU2)) {
      u8_t *p_inst = inst_data;

      if (dlen < sizeof(ethdr_t)) return retU2;

      p_inst += sizeof(ethdr_t);
      dlen   -= sizeof(ethdr_t);

      retU = Append_UNK_DAT(p_inst, dlen, type_id.t32, abs_offs, 0, this);
      if ( (! RCD_IS_OK(retU)) && !b_edit_mode ) return retU;
   }

   subgroups.CalcDataSZ(this);

   if (! RCD_IS_OK(retU)) return retU2;
   return retU;

}

rcode EDID_cl::VDDD_IF_MaxF(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   u8_t   *inst;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      u32_t  tmpv;
      tmpv   = reinterpret_cast<clkfrng_t*> (inst)->fmax_2msb;
      ival   = reinterpret_cast<clkfrng_t*> (inst)->fmax_8lsb;
      tmpv <<= 8;
      ival  |= tmpv; //2msb

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
         u32_t  tmpv;
         utmp &= 0x3FF;

         tmpv  = utmp >> 8; //2msb
         utmp &= 0xFF;      //8lsb

         reinterpret_cast<clkfrng_t*> (inst)->fmax_2msb = tmpv;
         reinterpret_cast<clkfrng_t*> (inst)->fmax_8lsb = utmp;
      }
   }
   return retU;
}

rcode EDID_cl::VDDD_HVpix_cnt(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   u16_t  *inst;

   inst = (u16_t*) getValPtr(p_field);

   if (op == OP_READ) {
      ival   = *inst;
      ival  += 1; //stored value is pix_count-1

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

      utmp -= 1;
      if (utmp > 0xFFFF) RCD_RETURN_FAULT(retU); //from ival
      *inst = utmp;
   }
   return retU;
}

rcode EDID_cl::VDDD_AspRatio(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      float  dval;
      ival  = *inst;
      dval  = ival;
      dval /= 100.0;
      dval += 1.0;

      if (sval.Printf("%.03f", dval) < 0) {
         RCD_SET_FAULT(retU);
      } else {
         RCD_SET_OK(retU);
      }
   } else {
      u32_t  utmp = 0;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         float  dval;
         retU = getStrFloat(sval, 1.0, 3.55, dval);
         if (! RCD_IS_OK(retU)) return retU;
         dval -= 1.0;
         dval *= 100.0;
         ival = dval;
      } else if (op == OP_WRINT) {
         ival &= 0xFF;
         utmp  = ival;
         RCD_SET_OK(retU);
      } else {
         RCD_RETURN_FAULT(retU); //wrong op code
      }

      *inst = utmp;
   }
   return retU;
}

rcode EDID_cl::VDDD_HVpx_pitch(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      float  dval;
      ival  = *inst;
      dval  = ival;
      dval /= 100.0;

      if (sval.Printf("%.03f", dval) < 0) {
         RCD_SET_FAULT(retU);
      } else {
         RCD_SET_OK(retU);
      }
   } else {
      u32_t  utmp = 0;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         float  dval;
         retU = getStrFloat(sval, 0.0, 2.55, dval);
         if (! RCD_IS_OK(retU)) return retU;
         dval *= 100.0;
         ival = dval;
      } else if (op == OP_WRINT) {
         ival &= 0xFF;
         utmp  = ival;
         RCD_SET_OK(retU);
      } else {
         RCD_RETURN_FAULT(retU); //wrong op code
      }

      *inst = utmp;
   }
   return retU;
}

rcode EDID_cl::VDDD_AudioDelay(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   u8_t   *inst;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      ival   = *inst;
      ival  &= 0x7F;
      ival <<= 1;   //2ms resolution

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

      utmp >>= 1;
      utmp  &= 0x7F;
      *inst  = utmp;
   }
   return retU;
}


//CLDB: Colorimetry Data Block (DBC_ET_CLDB = 5)
const char  cea_cldb_cl::Desc[] =
"The Colorimetry Data Block indicates support of specific extended colorimetry standards";

const gpfld_dsc_t cea_cldb_cl::fields = {
   .flags    = 0,
   .dat_sz   = sizeof(cldb_t),
   .inst_cnt = 1,
   .fcount   = 11,
   .fields   = cea_cldb_cl::fld_dsc
};

const edi_field_t cea_cldb_cl::fld_dsc[] = {
    //byte2
   {&EDID_cl::BitVal, 0, 2, 0, 1, F_BIT, 0, 1, "xvYCC601",
   "Standard Definition Colorimetry based on IEC 61966-2-4" },
   {&EDID_cl::BitVal, 0, 2, 1, 1, F_BIT, 0, 1, "xvYCC709",
   "High Definition Colorimetry based on IEC 61966-2-4" },
   {&EDID_cl::BitVal, 0, 2, 2, 1, F_BIT, 0, 1, "sYCC601",
   "Colorimetry based on IEC 61966-2-1/Amendment 1" },
   {&EDID_cl::BitVal, 0, 2, 3, 1, F_BIT, 0, 1, "opYCC601",
   "Colorimetry based on IEC 61966-2-5, Annex A" },
   {&EDID_cl::BitVal, 0, 2, 4, 1, F_BIT, 0, 1, "opRGB",
   "Colorimetry based on IEC 61966-2-5" },
   {&EDID_cl::BitVal, 0, 2, 5, 1, F_BIT, 0, 1, "BT2020cYCC",
   "Colorimetry based on ITU-R BT.2020, Y’cC’BCC’RC" },
   {&EDID_cl::BitVal, 0, 2, 6, 1, F_BIT, 0, 1, "BT2020YCC",
   "Colorimetry based on ITU-R BT.2020, Y’C’BC’R" },
   {&EDID_cl::BitVal, 0, 2, 7, 1, F_BIT, 0, 1, "BT2020RGB",
   "Colorimetry based on ITU-R BT.2020, R’G’B’" },
   //byte3
   {&EDID_cl::BitF8Val, 0, 3, 0, 4, F_BFD|F_INT|F_RD, 0, 0x07, "MD03",
    "MD0-3: Designated for future gamut-related metadata. "
    "As yet undefined, this metadata is carried in an interface-specific way." },
   {&EDID_cl::BitF8Val, 0, 3, 4, 3, F_BFD|F_INT|F_RD, 0, 0x07, "resvd46",
    "reserved (0)" },
   {&EDID_cl::BitVal, 0, 3, 7, 1, F_BIT, 0, 1, "DCI-P3",
   "Colorimetry based on DCI-P3" }
};

const dbc_flatgp_dsc_t cea_cldb_cl::CLDB_grp = {
   .CodN     = "CLDB",
   .Name     = "Colorimetry Data Block",
   .Desc     = Desc,
   .type_id  = ID_CLDB,
   .flags    = 0,
   .min_len  = (sizeof(cldb_t) +1),
   .max_len  = (sizeof(cldb_t) +1),
   .max_fld  = (32 - sizeof(cldb_t) - sizeof(ethdr_t) + 11),
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .fld_arsz = 1,
   .fld_ar   = &fields
};

rcode cea_cldb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   retU = base_DBC_Init_FlatGrp(inst, &CLDB_grp, orflags, parent);
   return retU;
}

//HDRS: HDR Static Metadata Data Block (DBC_ET_HDRS = 6)
const char  cea_hdrs_cl::Desc[] = "Indicates the HDR capabilities of the Sink.";

const edi_field_t cea_hdrs_cl::fld_byte_2_3_dsc[] = {
    //byte2
   {&EDID_cl::BitVal, 0, 2, 0, 1, F_BIT, 0, 1, "SDR",
   "Traditional gamma - SDR Luminance Range" },
   {&EDID_cl::BitVal, 0, 2, 1, 1, F_BIT, 0, 1, "HDR",
   "Traditional gamma - HDR Luminance Range" },
   {&EDID_cl::BitVal, 0, 2, 2, 1, F_BIT, 0, 1, "SMPTE",
   "SMPTE ST 2084" },
   {&EDID_cl::BitVal, 0, 2, 3, 1, F_BIT, 0, 1, "HLG",
   "Hybrid Log-Gamma (HLG) based on Recommendation ITU-R BT.2100-0" },
   {&EDID_cl::BitVal, 0, 2, 4, 1, F_BIT|F_RD, 0, 1, "ET_4",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, 2, 5, 1, F_BIT|F_RD, 0, 1, "ET_5",
   "reserved (0)" },
   {&EDID_cl::BitF8Val, 0, 2, 6, 2, F_BFD|F_RD, 0, 1, "ET6-7",
   "reserved (0)" },
   //byte3
   {&EDID_cl::BitVal, 0, 3, 0, 1, F_BIT|F_RD, 0, 1, "SM_0",
   "Static Metadata Type 1" },
   {&EDID_cl::BitF8Val, 0, 3, 1, 7, F_BFD|F_INT|F_RD, 0, 1, "SM1-7",
   "reserved (0)" }
};

const edi_field_t cea_hdrs_cl::fld_opt_byte_4_5_6_dsc[] = {
   //byte 4,5,6
   {&EDID_cl::ByteVal, 0, 4, 0, 1, F_BTE|F_INT|F_RD, 0, 0xFF, "max_lum",
   "(Optional) Desired Content Max Luminance data (8 bits)" },
   {&EDID_cl::ByteVal, 0, 5, 0, 1, F_BTE|F_INT|F_RD, 0, 0xFF, "avg_lum",
   "(Optional) Desired Content Max Frame-average Luminance data (8 bits)" },
   {&EDID_cl::ByteVal, 0, 6, 0, 1, F_BTE|F_INT|F_RD, 0, 0xFF, "min_lum",
   "(Optional) Desired Content Min Luminance data (8 bits)" }
};

const dbc_flatgp_dsc_t cea_hdrs_cl::HDRS_grp = {
   .CodN     = "HDRS",
   .Name     = "HDR Static Metadata Data Block",
   .Desc     = Desc,
   .type_id  = ID_HDRS,
   .flags    = 0,
   .min_len  = 3,
   .max_len  = (sizeof(hdrs_t) +1),
   .max_fld  = (32 - sizeof(hdrs_t) - sizeof(ethdr_t) + 12),
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .fld_arsz = 2,
   .fld_ar   = cea_hdrs_cl::fields
};

const gpfld_dsc_t cea_hdrs_cl::fields[] = {
   { //fld_byte_2_3
      .flags    = 0,
      .dat_sz   = 2,
      .inst_cnt = 1,
      .fcount   = 9,
      .fields   = cea_hdrs_cl::fld_byte_2_3_dsc
   },
   { //fld_opt_byte_4_5_6
      .flags    = T_FLEX_LAYOUT|T_FLEX_LEN,
      .dat_sz   = 3,
      .inst_cnt = 1,
      .fcount   = 3,
      .fields   = cea_hdrs_cl::fld_opt_byte_4_5_6_dsc
   }
};

rcode cea_hdrs_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   retU = base_DBC_Init_FlatGrp(inst, &HDRS_grp, orflags, parent);
   return retU;
}

//HDRD: HDR Dynamic Metadata Data Block (DBC_ET_HDRD = 7)
//HDRD: HDR Dynamic Metadata sub-group
const char  hdrd_mtd_cl::Desc[] = "HDR Dynamic Metadata";

const dbc_subg_dsc_t hdrd_mtd_cl::MTD_hdr = {
   .s_ctor   = &hdrd_mtd_cl::group_new,
   .CodN     = "HMTD",
   .Name     = "HDR Dynamic Metadata",
   .Desc     = Desc,
   .type_id  = ID_HDRD_HMTD | T_SUB_GRP | T_P_HOLDER | T_NO_COPY,
   .min_len  = sizeof(hdrd_mtd_t)-1, //min len for HDR_dmtd3 (no Support Flags byte)
   .fcount   = 4,
   .inst_cnt = -1,
   .fields   = hdrd_mtd_cl::fld_dsc
};

const edi_field_t hdrd_mtd_cl::fld_dsc[] = {
   {&EDID_cl::ByteVal, 0, offsetof(hdrd_mtd_t, mtd_len), 0, 1, F_BTE|F_INT|F_RD|F_INIT, 0, (30-1), "length",
   "Length of the metadata block\n" },
   {&EDID_cl::Word16, 0, offsetof(hdrd_mtd_t, mtd_type), 0, 2, F_HEX|F_RD|F_INIT, 0, 0xFFFF, "mtd_type",
   "Type of metadata:\n"
   "0x0000: Reserved\n"
   "0x0001: Metadata specified in Annex R\n"
   "0x0002: Supplemental Enhancement Information (SEI) messages, according to ETSI TS 103 433\n"
   "0x0003: Color Remapping Information SEI message according to ITU-T H.265\n"
   "0x0004: Metadata pecified in Annex S\n"
   "0x0005 .. 0xFFFF: Reserved" },
   {&EDID_cl::BitF8Val, 0, offsetof(hdrd_mtd_t, supp_flg), 0, 4, F_BFD|F_INT|F_RD, 0, 0xF, "mtd_ver",
   "Support Flags: HDR metadata version for metadata types 1,2,4\n"
   "For metadata type = 0x0003 this field does not exist (and also the resvd47)" },
   {&EDID_cl::BitF8Val, 0, offsetof(hdrd_mtd_t, supp_flg), 4, 4, F_BFD|F_INT|F_RD, 0, 0xF, "rsvd3_47",
   "reserved (0)" }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode hdrd_mtd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   const hdrd_mtd_t *mtdata;

   rcode  retU, retU2;
   u32_t  dlen;
   u32_t  blk_len;
   u32_t  m_type;
   u32_t  unk_offs;
   u32_t  unk_byte; //payload bytes: not interpreted

   RCD_SET_OK(retU2);

   mtdata   = reinterpret_cast<const hdrd_mtd_t*> (inst);
   blk_len  = mtdata->mtd_len;
   m_type   = mtdata->mtd_type;
   dyn_fcnt = MTD_hdr.fcount -2; //mtd_ver, resvd47
   dlen     = parent->getFreeSubgSZ();

   parent_grp  = parent;
   type_id.t32 = MTD_hdr.type_id;

   if (m_type != HDR_dmtd3) {
      if (blk_len < 4) {
         wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, MTD_hdr.CodN);
      }
   } else {
      if (blk_len != 2) {
         wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, MTD_hdr.CodN);
      }
   }

   if (blk_len < 2) {
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, MTD_hdr.CodN);
      blk_len = 2; //use trash data from local buffer
   }

   blk_len  += 1; // +metadata_length byte
   blk_len   = (blk_len <= dlen  ) ? blk_len : dlen;
   blk_len   = (blk_len <= (30-1)) ? blk_len : (30-1);

   unk_byte  = blk_len;
   unk_byte -= MTD_hdr.min_len;
   unk_offs  = MTD_hdr.min_len;

   if (blk_len > MTD_hdr.min_len) { //only HDR_dmtd3 can be represented in full
      if (m_type != HDR_dmtd3) {
         dyn_fcnt += 2; //mtd_ver, resvd47
         unk_offs += 1;
         unk_byte -= 1;

      } else { //DBC length ?> max length for HDR_dmtd3
         wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, MTD_hdr.CodN);
      }
   }

   CopyInstData(inst, blk_len);

   dyn_fldar = (edi_field_t*) malloc((dyn_fcnt + unk_byte) * EDI_FIELD_SZ);
   if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);

   {
      edi_field_t *p_fld;

      p_fld = dyn_fldar;
      memcpy( p_fld, &MTD_hdr.fields[0], (dyn_fcnt * EDI_FIELD_SZ) );
      p_fld    += dyn_fcnt;
      dyn_fcnt += unk_byte;

      insert_unk_byte(p_fld, unk_byte, unk_offs);

      retU = init_fields(dyn_fldar, inst_data, dyn_fcnt, false,
                         MTD_hdr.Name, MTD_hdr.Desc, MTD_hdr.CodN);
   }

   if (RCD_IS_OK(retU)) return retU2;
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

//HDRD: HDR Dynamic Metadata Data Block: base class
const char  cea_hdrd_cl::Desc[] = "HDR Dynamic Metadata Data Block";

const dbc_root_dsc_t cea_hdrd_cl::HDRD_grp = {
   .CodN     = "HDRD",
   .Name     = "HDR Dynamic Metadata Data Block",
   .Desc     = Desc,
   .type_id  = ID_HDRD | ID_HDRD_HMTD,
   .flags    = 0,
   .min_len  = sizeof(hdrd_mtd_t)+1,
   .max_len  = 31,
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .ahf_sz   = 0,
   .ahf_cnt  = 0,
   .ah_flds  = NULL,
   .grp_arsz = 1,
   .grp_ar   = &hdrd_mtd_cl::MTD_hdr
};

rcode cea_hdrd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   retU = base_DBC_Init_RootGrp(inst, &HDRD_grp, orflags, parent);
   return retU;
}

//VFPD: Video Format Preference Data Block (DBC_ET_VFPD = 13)
const char  cea_vfpd_cl::Desc[] =
"VFPD defines order of preference for selected video formats.\n\n"
"Preferred formats can be referenced as an index of DTD or SVD already present in the EDID or directly, as a Video ID Code (VIC)."
"The order defined here takes precedence over all other rules defined elsewhere in the standard."
"Each byte in VFPD block contains Short Video Reference (SVR) code, first SVR is the most-preferred video format.";

const dbc_root_dsc_t cea_vfpd_cl::VFPD_grp = {
   .CodN     = "VFPD",
   .Name     = "Video Format Preference Data",
   .Desc     = Desc,
   .type_id  = ID_VFPD | ID_VFPD_SVR,
   .flags    = 0,
   .min_len  = 2,
   .max_len  = 31,
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .ahf_sz   = 0,
   .ahf_cnt  = 0,
   .ah_flds  = NULL,
   .grp_arsz = 1,
   .grp_ar   = &cea_svr_cl::SVR_subg
};

rcode cea_vfpd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   retU = base_DBC_Init_RootGrp(inst, &VFPD_grp, orflags, parent);
   return retU;
}

u32_t EDID_cl::CEA_VFPD_SVR_decode(u32_t svr, u32_t& ndtd) {
   u32_t dtd  = 0;
   u32_t mode = (svr & 0xFF);

   switch (svr) {
      case 0 ... 128: //0, 128 reserved, 1..127 -> VIC code
         break;
      case 129 ... 144: //DTD 1..16
         dtd  = (mode - 128);
         break;
      case 145 ... 192: //reserved
         break;
      default: //193 ... 255: vid_fmt.cpp: max VIC is 219, natv=0, 220..255 are reserved in G spec.
         break;
   }

   ndtd = dtd;
   return mode;
}

//VFPD: Video Format Preference Data Block ->
//SVR: Short Video Reference
const char  cea_svr_cl::Desc[] =
"Short Video Reference: "
"contains index number of preferred resolution defined in CEA/CTA-861 standard "
"or a DTD index";

const dbc_subg_dsc_t cea_svr_cl::SVR_subg = {
   .s_ctor   = &cea_svr_cl::group_new,
   .CodN     = "SVR",
   .Name     = "Short Video Reference",
   .Desc     = Desc,
   .type_id  = ID_VFPD_SVR | T_SUB_GRP,
   .min_len  = 1,
   .fcount   = 1,
   .inst_cnt = -1,
   .fields   = cea_svr_cl::fld_dsc
};

const edi_field_t cea_svr_cl::fld_dsc[] = {
   //NOTE: no F_VS flag: no value selector menu
   //SVD_vidfmt_map used only in MAIN::SaveRep_SubGrps()
   {&EDID_cl::ByteVal, VS_SVD_VIDFMT, 0, 0, 1, F_BTE|F_INT, 0, 0xFF, "SVR",
   "The SVR codes are interpreted as follows:\n"
   "0\t\t: reserved\n"
   "1-127\t: VIC\n"
   "128\t\t: reserved\n"
   "129-144\t: Nth DTD block, N=SVR-128 (1..16)\n"
   "145-192\t: reserved\n"
   "193-219\t: VIC\n"
   "220-255\t: reserved" }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode cea_svr_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   parent_grp  = parent;
   type_id.t32 = SVR_subg.type_id;

   CopyInstData(inst, 1);

   retU = init_fields(&fld_dsc[0], inst_data, SVR_subg.fcount, false,
                      SVR_subg.Name, SVR_subg.Desc, SVR_subg.CodN);

   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"


//Y42V: YCBCR 4:2:0 Video Data Block (DBC_ET_Y42V = 14)
const char  cea_y42v_cl::Desc[] =
"List of Short Video Descriptors (SVD), for which the ONLY supported sampling mode is YCBCR 4:2:0 "
"(all other sampling modes are NOT supported: RGB, YCBCR 4:4:4, or YCBCR 4:2:2)\n"
"NOTE:\nBy default, SVDs in this block shall be less preferred than all regular "
"SVDs - this can be changed using Video Format Preference Data Block (VFPD)";

const dbc_root_dsc_t cea_y42v_cl::Y42V_grp = {
   .CodN     = "Y42V",
   .Name     = "YCBCR 4:2:0 Video Data Block",
   .Desc     = Desc,
   .type_id  = ID_Y42V | ID_SVD,
   .flags    = 0,
   .min_len  = sizeof(svd_t)+1,
   .max_len  = 31,
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .ahf_sz   = 0,
   .ahf_cnt  = 0,
   .ah_flds  = NULL,
   .grp_arsz = 1,
   .grp_ar   = &cea_svd_cl::SVD_subg
};

rcode cea_y42v_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   retU = base_DBC_Init_RootGrp(inst, &Y42V_grp, orflags, parent);
   return retU;
}

//Y42C: YCBCR 4:2:0 Capability Map Data Block (DBC_ET_Y42C = 15)
const char  cea_y42c_cl::CodN[] = "Y42C";
const char  cea_y42c_cl::Name[] = "YCBCR 4:2:0 Capability Map Data Block";
const char  cea_y42c_cl::Desc[] =
"Bytes in this block are forming a bitmap, in which a single bit indicates whether a format defined in VDB::SVD "
"supports YCBCR 4:2:0 sampling mode (in addition to other modes: RGB, YCBCR 4:4:4, or YCBCR 4:2:2)\n"
"0= SVD does NOT support YCBCR 4:2:0\n"
"1= SVD supports YCBCR 4:2:0 in addition to other modes\n\n"
"Bit 0 in 1st byte refers to 1st SVD in VDB, bit 1 refers to 2nd SVD, and so on, "
"and 9th SVD is referenced by bit 0 in 2nd byte.";

const edi_field_t cea_y42c_cl::bitmap_fld =
   {&EDID_cl::BitVal, 0, 0, 0, 1, F_BIT|F_GD, 0, 1, NULL, "" };

rcode cea_y42c_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode        retU, retU2;
   u32_t        dlen;
   u32_t        offs;
   u32_t        svdn;
   edi_field_t *p_fld;
   y42c_svdn_t *p_svdn;
   bool         b_edit_mode;

   RCD_SET_OK(retU2);
   b_edit_mode = (0 != (T_MODE_EDIT & orflags));

   dlen = reinterpret_cast<const ethdr_t*> (inst)->ehdr.hdr.tag.blk_len;
   if (dlen < 2) { //min 1 bitmap byte
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, CodN);
      if (! b_edit_mode) return retU2;
   }

   parent_grp  = parent;
   type_id.t32 = ID_Y42C | orflags;

   offs = dlen;
   if (dlen < 2) offs = 2;
   offs ++ ; //hdr
   CopyInstData(inst, offs);
   if (dlen >= 2) {
      dlen  -= 1;
   } else {
      dlen = 0;
   }

   //max 30 bytes * 8 bits = 240
   //+ hdr + ext_tag -> max = 243 fields.
   //buffer for array of fields is allocated dynamically

   dyn_fcnt  = (dlen << 3); //max 30 bytes * 8 bits
   dyn_fcnt += CEA_ETHDR_FCNT;

   //alloc fields array
   dyn_fldar = (edi_field_t*) malloc( dyn_fcnt * EDI_FIELD_SZ );
   if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);

   p_fld  = dyn_fldar;

   //CEA-DBC-EXT header fields
   memcpy( p_fld, CEA_BlkHdr_fields, (CEA_ETHDR_FCNT * EDI_FIELD_SZ) );

   if (dlen == 0) {
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, CodN);
      goto grp_empty; //manual editing of blk_len
   }

   //alloc array of y42c_svdn_t: dynamic field names
   svdn_ar = (y42c_svdn_t*) malloc( dyn_fcnt * sizeof(y42c_svdn_t) );
   if (NULL == svdn_ar) RCD_RETURN_FAULT(retU);

   p_fld  += CEA_ETHDR_FCNT;
   offs    = sizeof(ethdr_t); //1st bitmap byte
   p_svdn  = svdn_ar;
   svdn    = 0;

   //SVD map data: 1 field per bit
   for (u32_t itb=0; itb<dlen; ++itb) {
      for (u32_t bitn=0; bitn<8; ++bitn) {

         memcpy( p_fld, &bitmap_fld, EDI_FIELD_SZ );
         //dynamic field offset:
         p_fld->offs  = offs;
         p_fld->shift = bitn;

         //dynamic field name:
         snprintf(p_svdn->svd_num, sizeof(y42c_svdn_t), "SVD_%u", svdn);
         p_svdn->svd_num[7] = 0;
         p_fld->name        = p_svdn->svd_num;

         p_fld  += 1;
         p_svdn += 1;
         svdn   += 1;
      }
      offs += 1;
   }

grp_empty:
   retU = init_fields(&dyn_fldar[0], inst_data, dyn_fcnt, false, Name, Desc, CodN);

   if (RCD_IS_OK(retU)) return  retU2;
   return retU;
}

//VSAD: Vendor-Specific Audio Data Block (DBC_ET_VSAD = 17)
const char  cea_vsad_cl::Desc[] = "Vendor-Specific Audio Data Block: undefined data.";

const dbc_flatgp_dsc_t cea_vsad_cl::VSAD_grp = {
   .CodN     = "VSAD",
   .Name     = "Vendor-Specific Audio Data Block",
   .Desc     = Desc,
   .type_id  = ID_VSAD,
   .flags    = 0,
   .min_len  = (sizeof(vs_vadb_t) +1),
   .max_len  = 31,
   .max_fld  = (32 - sizeof(vs_vadb_t) - sizeof(ethdr_t) + 1),
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .fld_arsz = 1,
   .fld_ar   = &VSVD_VSAD_fields //fields shared with VSVD
};

rcode cea_vsad_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   retU = base_DBC_Init_FlatGrp(inst, &VSAD_grp, orflags, parent);
   return retU;
}

//HADB: HDMI Audio Data Block (DBC_ET_HADB = 18)
//NOTE: HDMI_3D_AD structure is the same as SAD (cea_sad_cl)
//NOTE: HDMI_3D_SAD structure is the same as SAB + ACAT field
const char  cea_hadb_cl::Desc[] =
"HDMI Audio Data Block";

const dbc_root_dsc_t cea_hadb_cl::HADB_root = {
   .CodN     = "HADB",
   .Name     = "HDMI Audio Data Block",
   .Desc     = Desc,
   .type_id  = ID_HADB | ID_SAD,
   .flags    = 0,
   .min_len  = sizeof(hadb_hdr_t) +1, //ETag + HADB header
   .max_len  = 31,
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .ahf_sz   = sizeof(hadb_hdr_t), //+2 for additioonal fields of HADB header
   .ahf_cnt  = 5,
   .ah_flds  = cea_hadb_cl::ah_flds,
   .grp_arsz = 2,
   .grp_ar   = HADB_subg
};

const dbc_subg_dsc_t cea_hadb_cl::HADB_subg[] = {
   { //cea_sad_cl::SAD_subg
   .s_ctor   = &cea_sad_cl::group_new,
   .CodN     = "SAD",
   .Name     = "Short Audio Descriptor",
   .Desc     = cea_sad_cl::Desc,
   .type_id  = ID_SAD | T_SUB_GRP,
   .min_len  = sizeof(sad_t),
   .fcount   = 0,
   .inst_cnt = 0, //variable: hadb_hdr_t.num_a3dsc
   .fields   = NULL
   },
   { //hadb_sab3d_cl
   .s_ctor   = &hadb_sab3d_cl::group_new,
   .CodN     = hadb_sab3d_cl::CodN,
   .Name     = hadb_sab3d_cl::Name,
   .Desc     = hadb_sab3d_cl::Desc,
   .type_id  = ID_SAB3D | T_SUB_GRP | T_NO_MOVE,
   .min_len  = sizeof(hadb_sab_t),
   .fcount   = 0,
   .inst_cnt = 1,
   .fields   = NULL
   }
};

const edi_field_t cea_hadb_cl::ah_flds[] = {
   {&EDID_cl::BitF8Val, 0, sizeof(ethdr_t), 0, 2, F_BFD|F_INT, 0, 3, "max_strm_cnt",
   "max number of audio streams in Multi-Stream Audio packets\n"
   " 0b00: (0) no MSA support,\n 0b01: (1) 2 streams\n"
   " 0b10: (2) 2 or 3 streams,\n 0b11: (3) 2,3 or 4 streams" },
   {&EDID_cl::BitVal, 0, sizeof(ethdr_t), 2, 1, F_BIT|F_INT, 0, 1, "srt_mix",
   "Support for mixing of additional streams, 1: supported" },
   {&EDID_cl::BitF8Val, 0, sizeof(ethdr_t), 3, 5, F_BFD, 0, 3, "res3_37",
   "reserved (0)" },

   {&EDID_cl::BitF8Val, 0, sizeof(ethdr_t)+1, 0, 3, F_BFD|F_INT|F_INIT, 0, 3, "num_a3dsc",
   "Number of 3D audio descriptors, (0..7), max 6" },
   {&EDID_cl::BitF8Val, 0, sizeof(ethdr_t)+1, 3, 5, F_BFD, 0, 3, "res4_37",
   "reserved (0)" },
};

rcode cea_hadb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   const hadb_hdr_t *hadb;
   dbc_subg_dsc_t   *pSAD_grp;
   dbc_root_dsc_t    HADB_tmp;
   dbc_subg_dsc_t    subg_tmp[HADB_root.grp_arsz];

   rcode  retU;
   i32_t  n_a3dsc;

   //can't use cea_sad_cl::SAD_subg directly:
   //SAD inst. count must match num_a3dsc, working on a copy:
   memcpy(&HADB_tmp, &HADB_root, sizeof(dbc_root_dsc_t));
   memcpy(&subg_tmp, &HADB_subg, sizeof(dbc_subg_dsc_t) << 1);

   pSAD_grp = &subg_tmp[0];
   hadb     = reinterpret_cast <const hadb_hdr_t*> (inst + sizeof(ethdr_t));
   n_a3dsc  = hadb->num_a3dsc;

   HADB_tmp.grp_ar    = subg_tmp;
   pSAD_grp->inst_cnt = n_a3dsc;

   retU = base_DBC_Init_RootGrp(inst, &HADB_tmp, orflags, parent);

   return retU;
}

const char  hadb_sab3d_cl::CodN[] = "SAB3D";
const char  hadb_sab3d_cl::Name[] = "Speaker Allocation Descriptor";
const char  hadb_sab3d_cl::Desc[] =
"This is essentially a SAB structure extended with additional ACAT "
"fields (Audio Channel Allocation Type).";

extern const edi_field_t SAB_SPM_fld_dsc[]; //CEA.cpp

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode hadb_sab3d_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 26, //SAB_SPM_fld_dsc: +2 additional fields for ACAT
      offs   = -1  //SAB_SPM_fld_dsc: adjust offsets for SAB3D instance
   };

   rcode retU;

   //not using orflags:
   //base_DBC_Init_RootGrp() is passing sub-grp ID -> this would malform the ID
   type_id.t32 = ID_SAB3D | T_SUB_GRP | T_NO_MOVE | T_P_HOLDER;
   parent_grp  = parent;

   CopyInstData(inst, sizeof(hadb_sab_t));

   retU = init_fields(&SAB_SPM_fld_dsc[0], inst_data, fcount, false,
                      Name, Desc, CodN, offs);

   { //move ACAT field to top of list
      u32_t          idx;
      edi_dynfld_t  *pfld;

      idx  = FieldsAr.GetCount() -1;
      pfld = FieldsAr.Detach(idx);
      FieldsAr.Insert(pfld, 0);
   }

   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

extern const char SAB_SPM_CodN[];
extern const char SAB_SPM_Name[];
extern const char SAB_SPM_Desc[];

//RMCD: Room Configuration Data Block: base class
const char  cea_rmcd_cl::Desc[] =
"Room Configuration Data Block (RMCD) describes playback environment, using room coordinate system.";

//Handler for normalized values,
rcode EDID_cl::RMCD_NormV(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   float   fval;
   i32_t   i32;
   i8_t   *inst;

   inst = (i8_t*) getValPtr(p_field);

   if (op == OP_READ) {
      i32   = (i32_t) *inst;
      fval  = i32;
      fval /= 64.0;
      sval.Printf("%.06f", fval);
      ival  = (u32_t) i32;

      RCD_SET_OK(retU);
   } else {
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrFloat(sval, -2.00000001, 1.98437501, fval);
         i32  = (fval * 64.0);

      } else if (op == OP_WRINT) {
         i32 = (i32_t) ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      *inst = (i8_t) i32;
   }
   return retU;
}

const dbc_root_dsc_t cea_rmcd_cl::RMCD_root = {
   .CodN     = "RMCD",
   .Name     = "Room Configuration Data Block",
   .Desc     = Desc,
   .type_id  = ID_RMCD,
   .flags    = 0,
   .min_len  = sizeof(rmcd_t)+1,
   .max_len  = sizeof(rmcd_t)+1,
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .ahf_sz   = 1,
   .ahf_cnt  = 4,
   .ah_flds  = cea_rmcd_cl::ah_flds,
   .grp_arsz = 3,
   .grp_ar   = RMCD_subg
};

const dbc_subg_dsc_t cea_rmcd_cl::RMCD_subg[] = {
   {
   .s_ctor   = &rmcd_spm_cl::group_new,
   .CodN     = SAB_SPM_CodN,
   .Name     = SAB_SPM_Name,
   .Desc     = SAB_SPM_Desc,
   .type_id  = ID_RMCD_SPM | T_SUB_GRP,
   .min_len  = sizeof(spk_mask_t),
   .fcount   = 0,
   .inst_cnt = 1,
   .fields   = {}
   },
   {
   .s_ctor   = &rmcd_spkd_cl::group_new,
   .CodN     = rmcd_spkd_cl::CodN,
   .Name     = rmcd_spkd_cl::Name,
   .Desc     = rmcd_spkd_cl::Desc,
   .type_id  = ID_RMCD_SPKD | T_SUB_GRP,
   .min_len  = sizeof(spk_plpd_t),
   .fcount   = 3,
   .inst_cnt = 1,
   .fields   = rmcd_spkd_cl::fld_dsc
   },
   {
   .s_ctor   = &rmcd_dspc_cl::group_new,
   .CodN     = rmcd_dspc_cl::CodN,
   .Name     = rmcd_dspc_cl::Name,
   .Desc     = rmcd_dspc_cl::Desc,
   .type_id  = ID_RMCD_DSPC | T_SUB_GRP,
   .min_len  = sizeof(disp_xyz_t),
   .fcount   = 3,
   .inst_cnt = 1,
   .fields   = rmcd_dspc_cl::fld_dsc
   }
};

const edi_field_t cea_rmcd_cl::ah_flds[] = {
   {&EDID_cl::BitVal, 0, sizeof(ethdr_t), 5, 1, F_BIT, 0, 1, "SLD",
   "SLD:\n 1: SPKLD Speaker Location Descriptors should be present for every speakr,\n"
   " 0: SLDB::SPKLD are not available, only SPM is used." },
   {&EDID_cl::BitVal, 0, sizeof(ethdr_t), 6, 1, F_BIT, 0, 1, "Speaker",
   "Speaker:\n 1= spk_cnt is valid,\n 0= spk_cnt is unused (should be 0) " },
   {&EDID_cl::BitVal, 0, sizeof(ethdr_t), 7, 1, F_BIT, 0, 1, "Display",
   "1= fields in DSPC (Display Coordinates) are valid (used)" },
   {&EDID_cl::BitF8Val, 0, sizeof(ethdr_t), 0, 5, F_BFD|F_INT, 0, 0x7, "spk_cnt",
   "Speaker Count: number of LPCM_channels -1, required for SLD=1" }
};

rcode cea_rmcd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   orflags |= T_GRP_FIXED;

   retU = base_DBC_Init_RootGrp(inst, &RMCD_root, orflags, parent);
   return retU;
}

//RMCD: SPM: Speaker Presence Mask
//NOTE: Desc is shared with SAB: CEA.cpp
rcode rmcd_spm_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      offs = -1  //SAB_SPM_fld_dsc: adjust offsets for SPM instance
   };

   rcode  retU;
   u32_t  fcount;

   fcount      = SAB_SPM_fields.fcount;
   parent_grp  = parent;
   type_id.t32 = ID_RMCD_SPM | T_SUB_GRP | orflags;

   CopyInstData(inst, sizeof(spk_mask_t));

   retU = init_fields(&SAB_SPM_fld_dsc[0], inst_data, fcount, false,
                      SAB_SPM_Name, SAB_SPM_Desc, SAB_SPM_CodN, offs);
   return retU;
}

//RMCD: SPKD: Speaker Distance
const char  rmcd_spkd_cl::CodN[] = "SPKD";
const char  rmcd_spkd_cl::Name[] = "Speaker Distance";
const char  rmcd_spkd_cl::Desc[] =
"Speaker distances in X,Y,Z axes from Primary Listening Position (PLP) in decimeters [dm]\n\n"
"NOTE:\nCTA-861-H essentially agrees with G-spec, but (apparentl erroneously) defines default "
"values for X/Y/Zmax, saying that f.e. 0x10 (16 dec) corresponds to 32 decimeters";

const edi_field_t rmcd_spkd_cl::fld_dsc[] = {
   {&EDID_cl::ByteVal, 0, 0, 0, 1, F_BTE|F_INT|F_DM, 0, 255, "Xmax",
   "X-axis max distance from PLP\n""If not used, it should be set to 0x10"},
   {&EDID_cl::ByteVal, 0, 1, 0, 1, F_BTE|F_INT|F_DM, 0, 255, "Ymax",
   "Y-axis max distance from PLP\n""If not used, it should be set to 0x10"},
   {&EDID_cl::ByteVal, 0, 2, 0, 1, F_BTE|F_INT|F_DM, 0, 255, "Zmax",
   "Z-axis max distance from PLP\n""If not used, it should be set to 0x08"}
};

rcode rmcd_spkd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 3
   };

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_RMCD_SPKD | T_SUB_GRP | orflags;

   CopyInstData(inst, sizeof(spk_plpd_t));

   retU = init_fields(&fld_dsc[0], inst_data, fcount, false,
                      Name, Desc, CodN);

   return retU;
}

//RMCD: DSPC: Display Coordinates
const char  rmcd_dspc_cl::CodN[] = "DSPC";
const char  rmcd_dspc_cl::Name[] = "Display Coordinates";
const char  rmcd_dspc_cl::Desc[] =
"Display Coordinates normalized to X/Y/Zmax distances in SPKD (Speaker Distance)\n"
"Normalized values are in range -2.0 .. 1.984375";

const edi_field_t rmcd_dspc_cl::fld_dsc[] = {
   {&EDID_cl::RMCD_NormV, 0, 0, 0, 1, F_BTE|F_FLT, 0, 255, "DisplayX",
   "X-coordinate value normalized to Xmax in SPKD\n"
   "If not used, it should be set to 0x00 (0.0)"},
   {&EDID_cl::RMCD_NormV, 0, 1, 0, 1, F_BTE|F_FLT, 0, 255, "DisplayY",
   "Y-coordinate value normalized to Ymax in SPKD\n"
   "If not used, it should be set to 0x40 (1.0)"},
   {&EDID_cl::RMCD_NormV, 0, 2, 0, 1, F_BTE|F_FLT, 0, 255, "DisplayZ",
   "Z-coordinate value normalized to Zmax in SPKD\n"
   "If not used, it should be set to 0x00 (0.0)"}
};

rcode rmcd_dspc_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 3
   };

   rcode  retU;

   parent_grp  = parent;
   type_id.t32 = ID_RMCD_DSPC | T_SUB_GRP | orflags;

   CopyInstData(inst, sizeof(disp_xyz_t));

   retU = init_fields(&fld_dsc[0], inst_data, fcount, false,
                      Name, Desc, CodN);

   return retU;
}


//SLDB: Speaker Location Data Block (DBC_ET_SLDB = 20)
//SLDB: SPKLD: Speaker Location Descriptor

sm_vmap SPKLD_IDX_map = {
   {0x00, {0, "FL"   , "Front Left"  }},
   {0x01, {0, "FR"   , "Front Right" }},
   {0x02, {0, "FC"   , "Front Center"}},
   {0x03, {0, "LFE1" , "Low Frequency Effects 1 (subwoofer1)"}},
   {0x04, {0, "BL"   , "Back Left"   }},
   {0x05, {0, "BR"   , "Back Right"  }},
   {0x06, {0, "FLC"  , "Front Left of Center"}},
   {0x07, {0, "FRC"  , "Front Right of Center"}},
   {0x08, {0, "BC"   , "Back Center"}},
   {0x09, {0, "LFE2" , "Low Frequency Effects 2 (subwoofer2)"}},
   {0x0A, {0, "SiL"  , "Side Left" }},
   {0x0B, {0, "SiR"  , "Side Right"}},
   {0x0C, {0, "TpFL" , "Top Front Left"  }},
   {0x0D, {0, "TpFR" , "Top Front Right" }},
   {0x0E, {0, "TpFC" , "Top Front Center"}},
   {0x0F, {0, "TpC"  , "Top Center"      }},
   {0x10, {0, "TpBL" , "Top Back Left"   }},
   {0x11, {0, "TpBR" , "Top Back Right"  }},
   {0x12, {0, "TpSiL", "Top Side Left"   }},
   {0x13, {0, "TpSiR", "Top Side Right"  }},
   {0x14, {0, "TpBC" , "Top Back Center" }},
   {0x15, {0, "BtFC" , "Bottom Front Center"}},
   {0x16, {0, "BtFL" , "Bottom Front Left"  }},
   {0x17, {0, "BtFR" , "Bottom Front Right" }},
   {0x18, {0, "FLW"  , "Front Left Wide" }},
   {0x19, {0, "FRW"  , "Front Right Wide"}},
   {0x1A, {0, "LS"   , "Left Surround"   }},
   {0x1B, {0, "RS"   , "Right Surround"  }}
};

const char  spkld_cl::Desc[] = "Speaker Location Descriptor";

static const char SLDB_SPK_index[] =
"Speaker index 0..31, CTA-861-G: Table 34:\n"
"0x00 FL\t: Front Left\n"
"0x01 FR\t: Front Right\n"
"0x02 FC\t: Front Center\n"
"0x03 LFE1\t: Low Frequency Effects 1 (subwoofer1)\n"
"0x04 BL\t: Back Left\n"
"0x05 BR\t: Back Right\n"
"0x06 FLC\t: Front Left of Center\n"
"0x07 FRC\t: Front Right of Center\n"
"0x08 BC\t: Back Center\n"
"0x09 LFE2\t: Low Frequency Effects 2 (subwoofer2)\n"
"0x0A SiL\t: Side Left\n"
"0x0B SiR\t: Side Right\n"
"0x0C TpFL\t: Top Front Left\n"
"0x0D TpFR\t: Top Front Right\n"
"0x0E TpFC\t: Top Front Center\n"
"0x0F TpC\t: Top Center\n"
"0x10 TpBL\t: Top Back Left\n"
"0x11 TpBR\t: Top Back Right\n"
"0x12 TpSiL\t: Top Side Left\n"
"0x13 TpSiR\t: Top Side Right\n"
"0x14 TpBC\t: Top Back Center\n"
"0x15 BtFC\t: Bottom Front Center\n"
"0x16 BtFL\t: Bottom Front Left\n"
"0x17 BtFR\t: Bottom Front Right\n"
"0x18 FLW\t: Front Left Wide\n"
"0x19 FRW\t: Front Right Wide\n"
"0x1A LS\t: Left Surround\n"
"0x1B RS\t: Right Surround\n"
"0x1C..0x1F reserved\n";

const dbc_subg_dsc_t spkld_cl::SPKLD_subg = {
   .s_ctor   = &spkld_cl::group_new,
   .CodN     = "SPKLD",
   .Name     = "Speaker Location Descriptor",
   .Desc     = Desc,
   .type_id  = ID_SLDB_SPKLD | T_SUB_GRP | T_NO_COPY,
   .min_len  = offsetof(spkld_t, CoordX),
   .fcount   = 9,
   .inst_cnt = -1,
   .fields   = spkld_cl::fld_dsc
};

const edi_field_t spkld_cl::fld_dsc[] = {
   //channel index, byte0
   {&EDID_cl::BitF8Val, 0, offsetof(spkld_t, channel), 0, 5, F_BFD|F_INT|F_DN, 0, 0x1F, "Cahnnel_IDX",
   "LPCM channel index 0..31" },
   {&EDID_cl::BitVal, 0, offsetof(spkld_t, channel), 5, 1, F_BIT|F_DN, 0, 3, "Active",
   " 1= channel active,\n 0= channel unused" },
   {&EDID_cl::BitVal, 0, offsetof(spkld_t, channel), 6, 1, F_BIT|F_INIT|F_DN, 0, 3, "COORD",
   " 1= CoordX/Y/Z fields are used,\n 0= CoordX/Y/Z fields are not present in SPKLD" },
   {&EDID_cl::BitVal, 0, offsetof(spkld_t, channel), 7, 1, F_BIT, 0, 3, "resvd7",
   "reserved (0)" },
   //speaker index, byte1
   {&EDID_cl::BitF8Val, VS_SPKLD_IDX, offsetof(spkld_t, spk_id), 0, 5, F_BFD|F_HEX|F_VS|F_DN, 0, 0x1B,
   "Speaker_ID", SLDB_SPK_index },
   {&EDID_cl::BitF8Val, 0, offsetof(spkld_t, spk_id), 5, 3, F_BFD, 0, 3, "resvd57",
   "reserved (0)" },
   //speaker position, bytes 2-4
   {&EDID_cl::RMCD_NormV, 0, offsetof(spkld_t, CoordX), 0, 1, F_BTE|F_FLT|F_DN, 0, 255, "CoordX",
   "X-axis position value normalized to Xmax in RMCD::SPKD: Speaker Distance\n"
   "Range: -2.0 .. 1.984375" },
   {&EDID_cl::RMCD_NormV, 0, offsetof(spkld_t, CoordY), 1, 1, F_BTE|F_FLT|F_DN, 0, 255, "CoordY",
   "Y-axis position value normalized to Ymax in RMCD::SPKD: Speaker Distance\n"
   "Range: -2.0 .. 1.984375"},
   {&EDID_cl::RMCD_NormV, 0, offsetof(spkld_t, CoordZ), 2, 1, F_BTE|F_FLT|F_DN, 0, 255, "CoordZ",
   "Z-axis position value normalized to Zmax in RMCD::SPKD: Speaker Distance\n"
   "Range: -2.0 .. 1.984375" }
};

rcode spkld_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   const spkld_t *slocd;

   rcode    retU, retU2;
   u32_t    dlen;
   u32_t    fcount;
   u32_t    unk_byte;

   RCD_SET_OK(retU2);

   slocd    = reinterpret_cast<const spkld_t*> (inst);
   fcount   = SPKLD_subg.fcount;
   dlen     = parent->getFreeSubgSZ();
   unk_byte = 0;

   parent_grp  = parent;
   type_id.t32 = SPKLD_subg.type_id | orflags;

   if (slocd->channel.coord != 0) { //speaker coordinates data present
      if (dlen < sizeof(spkld_t)) {
         wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, SPKLD_subg.CodN);
         unk_byte  = dlen;
         unk_byte -= SPKLD_subg.min_len;
         fcount   -= 3;
      } else {
         dlen = sizeof(spkld_t);
      }
   } else {
      fcount -= 3;
      dlen    = SPKLD_subg.min_len;
   }

   CopyInstData(inst, dlen);

   if (0 == unk_byte) {
      retU = init_fields(&SPKLD_subg.fields[0], inst_data, fcount, false,
                          SPKLD_subg.Name, SPKLD_subg.Desc, SPKLD_subg.CodN);
   } else {
      edi_field_t *p_fld;

      dyn_fcnt  = fcount;
      dyn_fldar = (edi_field_t*) malloc((dyn_fcnt + unk_byte) * EDI_FIELD_SZ);
      if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);

      p_fld = dyn_fldar;
      memcpy( p_fld, &SPKLD_subg.fields[0], (dyn_fcnt * EDI_FIELD_SZ) );
      p_fld    += dyn_fcnt;
      dyn_fcnt += unk_byte;

      insert_unk_byte(p_fld, unk_byte, offsetof(spkld_t, CoordX) );

      retU = init_fields(dyn_fldar, inst_data, dyn_fcnt, false,
                         SPKLD_subg.Name, SPKLD_subg.Desc, SPKLD_subg.CodN);
   }

   if (RCD_IS_OK(retU)) return retU2;
   return retU;
}

void spkld_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   u32_t         ival;
   float         fval;
   i8_t         *CoordN;
   spkld_t      *spkld;
   edi_dynfld_t *p_field;

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   spkld = reinterpret_cast<spkld_t*> (inst_data);

   p_field = FieldsAr.Item(4); //Speaker_ID
   ( EDID.*p_field->field.handlerfn )(OP_READ, gp_name, ival, p_field );

   EDID.getValDesc(gp_name, p_field, ival, VD_NAME);

   if (spkld->channel.active) {
      gp_name << ", CH" << (int) spkld->channel.chn_idx;
   }

   if (! spkld->channel.coord) return;

   //NOTE: this can display trash data if parent grp length is invalid (too short)
   gp_name << ", ";
   ival   = 0;
   CoordN = &spkld->CoordX;
   do {
      fval  = (i32_t) CoordN[ival];
      fval /= 64.0;
      EDID.gp_name.Printf("%.02f", fval);
      gp_name << EDID.gp_name;
      if (ival < 2) gp_name << "/";
      ival ++ ;
   } while (ival < 3);
}

//SLDB: Speaker Location Data Block: base class
const char  cea_sldb_cl::Desc[] = "Speaker Location Data Block";

const dbc_root_dsc_t cea_sldb_cl::SLDB_grp = {
   .CodN     = "SLDB",
   .Name     = "Speaker Location Data Block",
   .Desc     = Desc,
   .type_id  = ID_SLDB | ID_SLDB_SPKLD,
   .flags    = 0,
   .min_len  = offsetof(spkld_t, CoordX)+1,
   .max_len  = 31,
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .ahf_sz   = 0,
   .ahf_cnt  = 0,
   .ah_flds  = NULL,
   .grp_arsz = 1,
   .grp_ar   = &spkld_cl::SPKLD_subg
};

rcode cea_sldb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   retU = base_DBC_Init_RootGrp(inst, &SLDB_grp, orflags, parent);
   return retU;
}

//IFDB: InfoFrame Data Block (DBC_ET_IFDB = 32)
//IFDB: IFPD: InfoFrame Processing Descriptor Header
const char  ifdb_ifpdh_cl::Desc[] = "InfoFrame Processing Descriptor";

const dbc_subg_dsc_t ifdb_ifpdh_cl::IFPD_subg = {
   .s_ctor   = NULL,
   .CodN     = "IFPD",
   .Name     = "InfoFrame Processing Descriptor",
   .Desc     = Desc,
   .type_id  = ID_IFDB_IFPD,
   .min_len  = sizeof(ifpdh_t),
   .fcount   = 3,
   .inst_cnt = 1,
   .fields   = ifdb_ifpdh_cl::fld_dsc
};

const edi_field_t ifdb_ifpdh_cl::fld_dsc[] = {
   {&EDID_cl::BitF8Val, 0, 0, 0, 5, F_BFD|F_INT|F_RD, 0, 0x1F, "resvd04",
    "reserved (0)" },
   {&EDID_cl::BitF8Val, 0, 0, 5, 3, F_BFD|F_INT|F_INIT, 0, 7, "Blk length",
    "Block length: num of bytes following the 'n_VSIFs' byte.\n"
    "For CTA-861-G the payload length should be zero." },
   {&EDID_cl::ByteVal, 0, offsetof(ifpdh_t, n_VSIFs), 0, 1, F_BTE|F_INT, 0, 0xFF, "n_VSIFs",
   "num of additional Vendor-Specific InfoFrames (VSIFs) that can be received simultaneously."
   "The number is encoded as (n_VSIFs - 1), so 0 means 1." }
};

rcode ifdb_ifpdh_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   retU = IFDB_Init_SubGrp(inst, &IFPD_subg, orflags, parent);
   return retU;
}

//IFDB: SIFD: Short InfoFrame Descriptor, InfoFrame Type Code != 0x00, 0x01
const char  ifdb_sifd_cl::Desc[] = "Short InfoFrame Descriptor";

const dbc_subg_dsc_t ifdb_sifd_cl::SIFD_subg = {
   .s_ctor   = NULL,
   .CodN     = "SIFD",
   .Name     = "Short InfoFrame Descriptor",
   .Desc     = Desc,
   .type_id  = ID_IFDB_SIFD,
   .min_len  = sizeof(sifdh_t),
   .fcount   = 2,
   .inst_cnt = 1,
   .fields   = ifdb_sifd_cl::fld_dsc
};

const edi_field_t ifdb_sifd_cl::fld_dsc[] = {
   {&EDID_cl::BitF8Val, 0, 0, 0, 5, F_BFD|F_INT|F_INIT, 0, 0x1F, "IFT Code",
    "InfoFrame Type Code, values 0x00, 0x01 are reserved for other types of descriptors." },
   {&EDID_cl::BitF8Val, 0, 0, 5, 3, F_BFD|F_INT|F_INIT, 0, 7, "Blk length",
    "Block length: num of bytes following the header byte." }
};

rcode ifdb_sifd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   retU = IFDB_Init_SubGrp(inst, &SIFD_subg, orflags, parent);
   return retU;
}

//IFDB: VSIFD: Short Vendor-Specific InfoFrame Descriptor, InfoFrame Type Code = 0x01
const char  ifdb_vsifd_cl::Desc[] = "Short Vendor-Specific InfoFrame Descriptor";

const dbc_subg_dsc_t ifdb_vsifd_cl::VSIFD_subg = {
   .s_ctor   = NULL,
   .CodN     = "VSIFD",
   .Name     = "Short Vendor-Specific InfoFrame Descriptor",
   .Desc     = Desc,
   .type_id  = ID_IFDB_VSD,
   .min_len  = sizeof(svsifd_t),
   .fcount   = 3,
   .inst_cnt = 1,
   .fields   = ifdb_vsifd_cl::fld_dsc
};

const edi_field_t ifdb_vsifd_cl::fld_dsc[] = {
   {&EDID_cl::BitF8Val, 0, offsetof(svsifd_t, ifhdr), 0, 5, F_BFD|F_INT|F_INIT, 0, 0x1F, "IFT Code",
    "Vendor-Specific InfoFrame Type Code == 0x01." },
   {&EDID_cl::BitF8Val, 0, offsetof(svsifd_t, ifhdr), 5, 3, F_BFD|F_INT|F_INIT, 0, 7, "Blk length",
    "Block length: num of bytes following the header byte." },
   //IEEE OUI
   {&EDID_cl::ByteStr, 0, offsetof(svsifd_t, ieee_id), 0, 3, F_STR|F_HEX|F_LE|F_RD, 0, 0xFFFFFF, "IEEE-OUI",
   "IEEE OUI (Organizationally Unique Identifier)" }
};

rcode ifdb_vsifd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   retU = IFDB_Init_SubGrp(inst, &VSIFD_subg, orflags, parent);
   return retU;
}

//IFDB: InfoFrame Data Block: base class
const char  cea_ifdb_cl::CodN[] = "IFDB";
const char  cea_ifdb_cl::Name[] = "InfoFrame Data Block";
const char  cea_ifdb_cl::Desc[] = "InfoFrame Data Block";

rcode cea_ifdb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode        retU, retU2;
   u32_t        dlen;
   u32_t        g_type;
   u32_t        grp_sz;
   u32_t        abs_ofs;
   u32_t        rel_ofs;
   u8_t        *g_inst;
   edi_grp_cl  *pgrp;
   bool         b_edit_mode;

   RCD_SET_OK(retU2);

   b_edit_mode = (0 != (T_MODE_EDIT & orflags));
   parent_grp  = parent;
   type_id.t32 = ID_IFDB;

   dlen    = reinterpret_cast <const ifdb_t*> (inst)->ethdr.ehdr.hdr.tag.blk_len;
   dlen   += sizeof(bhdr_t); //+1 hdr
   grp_sz  = dlen;
   //sizeof(ethdr_t) + 2 bytes IFPDH
   if (dlen < 4) {
      if (! b_edit_mode) RCD_RETURN_FAULT(retU);
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, CodN);
      grp_sz = 32;
   }

   CopyInstData(inst, grp_sz);
   hdr_sz = sizeof(ethdr_t);

   retU = init_fields(&CEA_BlkHdr_fields[0], inst_data, CEA_ETHDR_FCNT, false,
                      Name, Desc, CodN);
   if (! RCD_IS_OK(retU)) return retU;

   subg_sz = dlen;
   if (subg_sz > hdr_sz) {
      subg_sz -= hdr_sz;
   } else {
      subg_sz  = 0;
   }
   if (subg_sz == 0) goto grp_empty;
   g_inst   = inst_data;
   abs_ofs  = abs_offs;
   abs_ofs += hdr_sz;
   rel_ofs  = hdr_sz;
   g_inst  += hdr_sz;

   if (subg_sz < ifdb_ifpdh_cl::IFPD_subg.min_len) {
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, CodN);
      goto unk;
   }

   //create and init sub-groups: 2: IFPDH + 1 short descriptor
   //IFPDH
   pgrp = new ifdb_ifpdh_cl;
   if (pgrp == NULL) RCD_RETURN_FAULT(retU);
   retU2 = pgrp->init(g_inst, (ID_IFDB | T_SUB_GRP | T_GRP_FIXED | orflags), this );
   if (! RCD_IS_OK(retU2)) goto unk;

   pgrp->setAbsOffs(abs_ofs);
   pgrp->setRelOffs(rel_ofs);
   subgroups.Append(pgrp);

   grp_sz  = pgrp->getTotalSize();
   rel_ofs   += grp_sz;

   while (rel_ofs < dlen) {

      g_inst  += grp_sz;
      abs_ofs += grp_sz;
      subg_sz -= grp_sz;
      g_type   = reinterpret_cast <sifdh_t*> (g_inst)->ift_code;

      if (g_type == 0x01) { //Short Vendor-Specific InfoFrame Descriptor
         pgrp = new ifdb_vsifd_cl;
         if (pgrp == NULL) RCD_RETURN_FAULT(retU);
         retU2 = pgrp->init(g_inst, (ID_IFDB | T_SUB_GRP | orflags), this );

      } else { //Check InfoFrame Type Code
         if (g_type == 0x00) {
            wxedid_RCD_SET_FAULT_VMSG(retU2, "[E!] %s: Bad InfoFrame Type Code", CodN);
            if (! b_edit_mode) return retU2;
            goto unk;
         }
         //Short InfoFrame Descriptor
         pgrp = new ifdb_sifd_cl;
         if (pgrp == NULL) RCD_RETURN_FAULT(retU);
         retU2 = pgrp->init(g_inst, (ID_IFDB | T_SUB_GRP | orflags), this );
      }
      if (! RCD_IS_OK(retU2)) goto unk;

      pgrp->setAbsOffs(abs_ofs);
      pgrp->setRelOffs(rel_ofs);
      subgroups.Append(pgrp);

      grp_sz  = pgrp->getTotalSize();
      rel_ofs   += grp_sz;
   }

   if (rel_ofs > dlen) wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, CodN);

unk:
   if (! RCD_IS_OK(retU2)) { //blk_len too small or bad InfoFrame Type Code
      retU = Append_UNK_DAT(g_inst, subg_sz, orflags, abs_ofs, rel_ofs, this);
   }

grp_empty:
   subgroups.CalcDataSZ(this);

   if (RCD_IS_OK(retU)) return retU2;
   return retU;
}

//common init fn for IFDB sub-groups
rcode edi_grp_cl::IFDB_Init_SubGrp(const u8_t* inst, const dbc_subg_dsc_t* pSGDsc, u32_t orflags, edi_grp_cl* parent) {
   rcode        retU, retU2;
   u32_t        gplen;
   u32_t        blklen;
   u32_t        fcount;
   edi_field_t *p_fld;
   bool         b_edit_mode;

   RCD_SET_OK(retU2);

   b_edit_mode = (0 != (T_MODE_EDIT & orflags));
   parent_grp  = parent;
   type_id.t32 = pSGDsc->type_id | orflags;
   fcount      = pSGDsc->fcount;

   //block length:
   //sifdh_t, svsifd_t, ifpdh_t have the block length encoded in the same way,
   //so it doesn't matter which of those will be used here.
   blklen = reinterpret_cast <const sifdh_t*> (inst)->blk_len;
   gplen  = blklen;
   gplen += pSGDsc->min_len;

   if (gplen > parent->getFreeSubgSZ()) {
      if (! b_edit_mode) RCD_RETURN_FAULT(retU);
      wxedid_RCD_SET_FAULT_VMSG(retU2, ERR_GRP_LEN_MSG, pSGDsc->CodN);
      if (blklen > pSGDsc->min_len) return retU2; //insert UNK-DAT group
      gplen = parent->getFreeSubgSZ();
   }

   CopyInstData(inst, gplen);

   if (gplen > pSGDsc->min_len) {
      gplen -= pSGDsc->min_len; //payload size
   } else {
      gplen = 0;
   }

   //pre-alloc buffer for array of fields: fcount + gplen
   dyn_fldar = (edi_field_t*) malloc( (gplen + fcount) * EDI_FIELD_SZ );
   if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);

   p_fld = dyn_fldar;

   memcpy( p_fld, &pSGDsc->fields[0], (fcount * EDI_FIELD_SZ) );
   p_fld    += fcount;
   dyn_fcnt  = fcount;
   dyn_fcnt += gplen;

   //payload data interpreted as unknown
   insert_unk_byte(p_fld, gplen, pSGDsc->min_len);

   retU = init_fields(&dyn_fldar[0], inst_data, dyn_fcnt, false,
                      pSGDsc->Name, pSGDsc->Desc, pSGDsc->CodN);

   if (RCD_IS_OK(retU)) return retU2;
   return retU;
}


//T7VTDB: DisplayID Type VII Video Timing Data Block (DBC_ET_T7VTB = 34)
//T7VTDB: Aspect Ratio selector
sm_vmap T7_AspRatio_map = {
   {0, {0, "1:1"      , NULL}},
   {1, {0, "5:4"      , NULL}},
   {2, {0, "4:3"      , NULL}},
   {3, {0, "15:9"     , NULL}},
   {4, {0, "16:9"     , NULL}},
   {5, {0, "16:10"    , NULL}},
   {6, {0, "64:27"    , NULL}},
   {7, {0, "256:135"  , NULL}},
   {8, {0, "Hres:Vres", NULL}}
};

//T7VTDB, T10VTDB: 3D suppport selector
sm_vmap T7_3Dsupp_map = {
   {0, {0, "no 3D support"      , NULL}},
   {1, {0, "3D-only support"    , NULL}},
   {2, {0, "3D and Mono support", NULL}},
};

rcode EDID_cl::T7VTB_PixClk(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   float   fval;
   u8_t   *inst;

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      ival  = rdWord24_LE(inst);
      fval  = ival;
      fval /= 1000.0;
      sval.Printf("%.03f", fval);
      RCD_SET_OK(retU);
   } else {
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrFloat(sval, 0.001, 16777.215, fval);
         ival = (fval * 1000.0); //kHz
      } else if (op == OP_WRINT) {
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      wrWord24_LE(inst, ival);
   }
   return retU;
}

const char  cea_t7vtb_cl::Desc[] =
"Type 7 Video Timing Data Block";

const gpfld_dsc_t cea_t7vtb_cl::fields = {
   .flags    = 0,
   .dat_sz   = (sizeof(t7vtdb_t) - sizeof(ethdr_t)), //for base_DBC_Init_FlatGrp()
   .inst_cnt = 1,
   .fcount   = 19,
   .fields   = cea_t7vtb_cl::fld_dsc
};

const edi_field_t cea_t7vtb_cl::fld_dsc[] = {
  //t7hdr_t
   {&EDID_cl::BitF8Val, 0, offsetof(t7vtdb_t, t7_hdr), 0, 3, F_INT|F_BFD, 0, 7,
   "Blk_rev",
   "CTA-861-H: supported block revision is 0b010 (2)" },
   {&EDID_cl::BitVal, 0, offsetof(t7vtdb_t, t7_hdr), 3, 1, F_INT|F_BIT, 0, 1,
   "DSC_PT",
   "Reserved for VESA DisplayID, for CTA-861 the bit must be 0" },
   {&EDID_cl::BitF8Val, 0, offsetof(t7vtdb_t, t7_hdr), 4, 3, F_INT|F_BFD, 0, 7,
   "T7_M",
   "Number of additional bytes at the end of the descriptor."
   "Required 0b000 (0), other values are reserved." },
   {&EDID_cl::BitVal, 0, offsetof(t7vtdb_t, t7_hdr), 7, 1, F_INT|F_BIT, 0, 1,
   "F37_rsvd", "Reserved 0" },

 //t7flag_t
   {&EDID_cl::BitF8Val, VS_T7_ASP_RATIO, offsetof(t7vtdb_t, t7_prop), 0, 4, F_BFD|F_INT|F_VS, 0, 15,
   "asp_ratio",
   "Aspect ratio (0-8):\n"
   "0: 1/1 \n1:  5/4 \n2: 4/3 \n3: 15/9 \n4: 16/9\n"
   "5: 16/10 \n6: 64/27 \n7: 256/135 \n"
   "8: HApixels/VApisels\n" "other values are reserved." },
   {&EDID_cl::BitVal, 0, offsetof(t7vtdb_t, t7_prop), 4, 1, F_INT|F_BIT, 0, 1,
   "T7_IL",
   "Interlace support: for CTA-861 it must be 0." },
   {&EDID_cl::BitF8Val, VS_T710_3D_SUPP, offsetof(t7vtdb_t, t7_prop), 5, 2, F_BFD|F_INT|F_VS, 0, 3,
   "3D_support",
   "Support for 3D (stereo image):\n"
   "0b00: (0) not supported \n0b01: (1) only 3D supported\n"
   "0b10: (2) both mono and stereo modes supported \n0b11: (3) reserved\n\n." },
   {&EDID_cl::BitVal, 0, offsetof(t7vtdb_t, t7_prop), 7, 1, F_INT|F_BIT, 0, 1,
   "T7_Y420",
   "YCBCR 4:2:0 sampling mode support, 1: supported, 0: only RGB" },

   {&EDID_cl::T7VTB_PixClk, 0, offsetof(t7vtdb_t, pix_clk), 0, 3, F_FLT|F_MHZ|F_DN, 1, 0xFFFFFF,
   "Pixel clock",
   "Pixel clock in 1 kHz units (0.001–16777.215 MHz, LE).\n"
   "val=0 is reserved." },

   {&EDID_cl::Word16, 0, offsetof(t7vtdb_t, HApix), 0, 2, F_INT|F_PIX|F_DN, 0, 0xFFFF,
   "H-Active pix",
   "Horizontal active pixels (0–65535), X-resolution." },
   {&EDID_cl::Word16, 0, offsetof(t7vtdb_t, HBpix), 0, 2, F_INT|F_PIX|F_DN, 0, 0xFFFF,
   "H-Blank pix",
   "H-Blank pixels (0–65535).\n"
   "This field defines a H-Blank time as number of pixel clock pulses. H-Blank time is a time "
   "break between drawing 2 consecutive lines on the screen. During H-blank time H-sync pulse is sent "
   "to the monitor to ensure correct horizontal alignment of lines." },
   {&EDID_cl::BitF16Val, 0, offsetof(t7vtdb_t, Hoffs), 0, 15, F_BFD|F_INT, 0, 32767,
   "H-Sync offs",
   "H-sync offset in pixel clock pulses (0–65535) from blanking start. This offset value is "
   "responsible for horizontal picture alignment. Higher values are shifting the picture to the "
   "left edge of the screen." },
   {&EDID_cl::Word16, 0, offsetof(t7vtdb_t, HsyncW), 0, 2, F_INT, 0, 0xFFFF,
   "H-Sync width",
   "H-sync pulse width (time) in pixel clock pulses (0–65535)." },
 //NOTE: actual offset is +1 for t7f16s1_t.H_7msb
   {&EDID_cl::BitVal, 0, offsetof(t7vtdb_t, Hoffs)+1, 7, 1, F_INT|F_BIT, 0, 1,
   "T7_HSP",
   "H-sync polarity 1: positive, 0: negative" },

   {&EDID_cl::Word16, 0, offsetof(t7vtdb_t, VAlin), 0, 2, F_INT|F_PIX|F_DN, 0, 0xFFFF,
   "V-Active lin",
   "Vertical active lines (0–65535), V-resolution." },
   {&EDID_cl::Word16, 0, offsetof(t7vtdb_t, VBpix), 0, 2, F_INT|F_PIX|F_DN, 0, 0xFFFF,
   "V-Blank lines",
   "V-blank lines (0–65535).\n"
   "This field defines a V-Blank time as number of H-lines. During V-Blank time V-sync pulse is sent "
   "to the monitor to ensure correct vertical alignment of the picture." },
   {&EDID_cl::BitF16Val, 0, offsetof(t7vtdb_t, Voffs), 0, 15, F_BFD|F_INT, 0, 32767,
   "V-Sync offs",
   "V-sync offset as number of H-lines (0–32767). This offset value is responsible for vertical "
   "picture alignment. Higher values are shifting the picture to the top edge of the screen." },
   {&EDID_cl::Word16, 0, offsetof(t7vtdb_t, VsyncW), 0, 2, F_INT, 0, 0xFFFF,
   "V-Sync width",
   "V-sync pulse width (time) as number of H-lines (0–65535)" },
 //NOTE: actual offset is +1 for t7f16s1_t.H_7msb
   {&EDID_cl::BitVal, 0, offsetof(t7vtdb_t, Voffs)+1, 7, 1, F_INT|F_BIT, 0, 1,
   "T7_VSP",
   "V-sync polarity 1: positive, 0: negative" },
};

const dbc_flatgp_dsc_t cea_t7vtb_cl::T7VTB_grp = {
   .CodN     = "T7VTB",
   .Name     = "Type 7 Video Timing Data Block",
   .Desc     = Desc,
   .type_id  = ID_T7VTDB,
   .flags    = 0,
   .min_len  = (sizeof(t7vtdb_t) - sizeof(bhdr_t)), //for base_DBC_Init_FlatGrp()
   .max_len  = (sizeof(t7vtdb_t) - sizeof(bhdr_t)),
   .max_fld  = (19 + CEA_ETHDR_FCNT),
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .fld_arsz = 1,
   .fld_ar   = &cea_t7vtb_cl::fields
};

rcode cea_t7vtb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   retU = base_DBC_Init_FlatGrp(inst, &T7VTB_grp, orflags, parent);
   return retU;
}

void cea_t7vtb_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   u32_t         dlen;
   u32_t         ival;
   u32_t         xres;
   u32_t         yres;
   edi_dynfld_t *p_field;

   if (! EDID.b_GrpNameDynamic) {
      goto _exit_def_name;
   }

   dlen  = reinterpret_cast <const bhdr_t*> (inst_data)->tag.blk_len;

   if (dlen < T7VTB_grp.max_len) { //invalid blk length (too short)
      goto _exit_def_name;
   }

   //X-res
   p_field = FieldsAr.Item(T7F_IDX_HAPIX);

   EDID.gp_name.Empty();
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, xres, p_field );
   gp_name  = EDID.gp_name;
   gp_name << "x";

   //Y-res
   p_field = FieldsAr.Item(T7F_IDX_VALIN);
   EDID.gp_name.Empty();
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, yres, p_field );
   gp_name << EDID.gp_name;
   gp_name << " @ ";


   {  //V-Refresh
      float lineW, pixclk, lineT, n_lines, frameT;

      p_field = FieldsAr.Item(T7F_IDX_PIXCLK); //H-freq (pix_clk)
      ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
      pixclk  = (float) (ival * 1000); //x1kHz

      p_field = FieldsAr.Item(T7F_IDX_HBPIX); //H-blank
      ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
      lineW   = (float) (ival + xres);
      lineT   = (lineW / pixclk);

      p_field = FieldsAr.Item(T7F_IDX_VBLIN); //V-blank
      ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
      n_lines = (float) (ival + yres);
      frameT  = (n_lines * lineT);

      EDID.gp_name.Printf("%.02fHz", (1.0/frameT));
      gp_name << EDID.gp_name;
   }
   return;

_exit_def_name:
   gp_name = GroupName;
}


//T8VTDB: DisplayID Type VIII Video Timing Data Block (DBC_ET_T8VTB = 35)
const char  cea_t8vtb_cl::Desc[] =
"Type 8 Video Timing Block is used to deeclare support for additional video modes.\n"
"Unlike VDB, T8VTB can be used also with 2-byte video codes if "
"Timing Code Size (TCS) bit is set to 1\n\n"
"NOTE#1: T8VTB should be used with 2-byte VESA-DMT codes; "
"1-byte VICs should be included in VDB or HDMI VSDB\n"
"NOTE#2: VFPDB must reference first Video Mode in first T8VTB in the EDID";

const dbc_root_dsc_t cea_t8vtb_cl::T8VTB_root = {
   .CodN     = "T8VTB",
   .Name     = "Type 8 Video Timing Data Block",
   .Desc     = Desc,
   .type_id  = ID_T8VTDB | ID_T8VTC_T0,
   .flags    = 0,
   .min_len  = 2, //ETag + 1 byte for additioonal fields of root grp header
   .max_len  = 31,
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .ahf_sz   = 1, //1 byte for additioonal fields of root grp header
   .ahf_cnt  = 5,
   .ah_flds  = cea_t8vtb_cl::ah_flds,
   .grp_arsz = 1,
   .grp_ar   = &t8vtb_vtc_cl::VTC_T0_subg //dynmic: VTC_T0_subg/VTC_T1_subg
};

const edi_field_t cea_t8vtb_cl::ah_flds[] = {
   {&EDID_cl::BitF8Val, 0, sizeof(ethdr_t), 0, 3, F_BFD, 0, 0x7, "blk_rev",
   "Block revision: 0b001, other values are reserved" },
   {&EDID_cl::BitVal, 0, sizeof(ethdr_t), 3, 1, F_BIT|F_INIT, 0, 1, "TCS",
   "Timing Code Size:\n 0: VESA DMT 1-byte codes\n 1: VESA DMT 2-byte codes (STI)" },
   {&EDID_cl::BitVal, 0, sizeof(ethdr_t), 4, 1, F_BIT, 0, 1, "F34_res",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, sizeof(ethdr_t), 5, 1, F_BIT, 0, 1, "T8Y420",
   "Indicates support for YCBCR 4:2:0 sampling by all listed video modes.\n"
   "1: All modes support RGB and YCBCR 4:2:0 sampling\n"
   "0: only RGB is supported" },
   {&EDID_cl::BitF8Val, 0, sizeof(ethdr_t), 6, 2, F_BFD, 0, 0x3, "code_type",
   "Code Type: 0b00 (1 or 2-byte DMT codes), other values are reserved" },
};

rcode cea_t8vtb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   const dbc_root_dsc_t *grp_desc;

   rcode  retU;
   int    TCS; //Timing Code Size

   dbc_root_dsc_t T8VTB_tmp;
   dbc_subg_dsc_t VTC_tmp;

   //The T8VTDB block header is @ 3rd byte, after ET hdr
   TCS = reinterpret_cast<const t8hdr_t*> (&inst[2])->tcs;

   orflags  &= T_MODE_EDIT; //the only acceptable flag (type_id depends on TCS)
   grp_desc  = &T8VTB_root;

   if (TCS == 0) goto _1_byte_vtc;

   //The subgroup size depends on t8hdr_t.tcs bit state:
   //Create temporary copy of group descriptor on stack
   //with modified parameters

   memcpy(&T8VTB_tmp, &T8VTB_root, sizeof(dbc_root_dsc_t));
   memset(&VTC_tmp  ,           0, sizeof(dbc_subg_dsc_t));

   /* Dynamic type ID:
      Prevents moving ID_T8VTC_T0/T2 sub-groups between T8VTDB blocks with different
      TCS value */
   T8VTB_tmp.type_id = ID_T8VTDB | ID_DMT2;
   T8VTB_tmp.min_len = sizeof(dmt_std2_t);
   T8VTB_tmp.grp_ar  = &VTC_tmp;

   VTC_tmp.s_ctor    = &dmt_std2_cl::group_new;
   VTC_tmp.min_len   = sizeof(dmt_std2_t);
   VTC_tmp.inst_cnt  = -1;

   grp_desc          = &T8VTB_tmp;

_1_byte_vtc:
   retU = base_DBC_Init_RootGrp(inst, grp_desc, orflags, parent);

   return retU;
}

const char t8vtb_vtc_cl::Desc[] =
"T8VTDB (TCS=0): VESA DMT-ID 1-byte Video Timing Code";

const dbc_subg_dsc_t t8vtb_vtc_cl::VTC_T0_subg = {
   .s_ctor   = &t8vtb_vtc_cl::group_new,
   .CodN     = "DMT-ID1",
   .Name     = "Video Timing Code",
   .Desc     = Desc,
   .type_id  = ID_T8VTC_T0 | T_SUB_GRP,
   .min_len  = sizeof(u8_t), //1-byte video codes
   .fcount   = 1,
   .inst_cnt = -1,
   .fields   = t8vtb_vtc_cl::vtc_1_byte
};

const edi_field_t t8vtb_vtc_cl::vtc_1_byte[] = {
   {&EDID_cl::ByteVal, VS_DMT1_VIDFMT, 0, 0, 1, F_BTE|F_HEX|F_DN|F_VS|F_VSVM, 0, 0xFF, "DMT_1",
   "VESA DMT-ID 1-byte Timing Code" },
};

rcode t8vtb_vtc_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      dlen   = 1,
      fcount = 1
   };

   rcode  retU;

   type_id.t32 = ID_T8VTC_T0 | T_SUB_GRP | orflags;
   parent_grp  = parent;

   CopyInstData(inst, dlen);

   retU = init_fields(&vtc_1_byte[0], inst_data, fcount, false,
                      VTC_T0_subg.Name, VTC_T0_subg.Desc, VTC_T0_subg.CodN);

   return retU;
}

edi_grp_cl* t8vtb_vtc_cl::Clone(rcode& rcd, u32_t flags) {
   t8vtb_vtc_cl *m_vtc;

   //pass TCS layout type to init(parent==NULL)
   //type_id is ID_T8VTC_T0 or ID_T8VTC_T1 == ID_DMT2
   m_vtc = new t8vtb_vtc_cl();
   m_vtc->type_id = type_id;

   return base_clone(rcd, m_vtc, flags);
};

void t8vtb_vtc_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   if (ID_T8VTC_T0 == (type_id.t32 & ID_SUBGRP_MASK)) { //DMT-ID 1-byte codes
      const vmap_ent_t  *m_ent;
      u32_t dmt1;

      dmt1  = (u32_t) inst_data[0];
      m_ent = vmap_GetVmapEntry(VS_DMT1_VIDFMT, VMAP_VAL, dmt1);

      if (NULL == m_ent) {
         gp_name = "<reserved>";
         return;
      }

      gp_name = m_ent->name;

   } else { //STD2 codes
      EDID.STI_DBN(EDID, gp_name, FieldsAr);
   }
}

//T10VTDB: DisplayID Type 10 Video Timing Data Block (DBC_ET_T10VTB = 42)

const char  cea_t10vtb_cl::Desc[] =
"Type 10 Video Timing Block is used to deeclare support for additional IT timing formats. "
"The timing formats declared in T10VTDB must conform to the VESA CVT";

const dbc_root_dsc_t cea_t10vtb_cl::T10VTB_root = {
   .CodN     = "T10VTB",
   .Name     = "Type 10 Video Timing Data Block",
   .Desc     = Desc,
   .type_id  = ID_T10VTDB | ID_T10VTD_M0,
   .flags    = 0,
   .min_len  = 2, //ETag + 1 byte for additioonal fields of root grp header
   .max_len  = 31,
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .ahf_sz   = sizeof(t8hdr_t), //1 byte for additioonal header fields in root grp
   .ahf_cnt  = 4,
   .ah_flds  = cea_t10vtb_cl::ah_flds,
   .grp_arsz = 1,
   .grp_ar   = &t10vtb_vtd_cl::VTD_M0_subg
};

const edi_field_t cea_t10vtb_cl::ah_flds[] = {
   {&EDID_cl::BitF8Val, 0, sizeof(ethdr_t), 0, 3, F_BFD, 0, 0x7, "blk_rev",
   "CTA-861-H: Block revision: 0b000, other values are reserved" },
   {&EDID_cl::BitVal, 0, sizeof(ethdr_t), 3, 1, F_BIT, 0, 1, "F33_res",
   "reserved (0)" },
   {&EDID_cl::BitF8Val, 0, sizeof(ethdr_t), 4, 3, F_BFD|F_INT|F_INIT, 0, 0x7, "T10_M",
   "T10_M field defines the length of timing descriptors included in this block.\n"
   "Supported values are:\n0b000: (0) desc. length is 6 bytes\n"
   "0b001: (1) desc. length is 7 bytes (6 + T10_M value)\n" },
   {&EDID_cl::BitVal, 0, sizeof(ethdr_t), 7, 1, F_BIT, 0, 1, "F37_res",
   "reserved (0)" },
};

rcode cea_t10vtb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   const dbc_root_dsc_t *grp_desc;

   rcode  retU;
   u32_t  T10M; //Timing Descriptor size modifier

   dbc_root_dsc_t T10VTB_tmp;

   //The T10VTDB block header is @ 3rd byte, after ET hdr
   T10M = reinterpret_cast<const t10hdr_t*> (&inst[2])->t10_m;

   grp_desc = &T10VTB_root;

   if (T10M == 0) goto _6_byte_vtc;

   //The subgroup layout depends on t10hdr_t.t10_m bit state:
   //Create temporary copy of group descriptor on stack
   //with modified parameters

   memcpy(&T10VTB_tmp, &T10VTB_root, sizeof(dbc_root_dsc_t));

   /* Dynamic type ID:
      Prevents moving T10VTD_M0/M1 sub-groups between T10VTB blocks with different
      T10M value */
   T10VTB_tmp.type_id = ID_T10VTDB | ID_T10VTD_M1;
   T10VTB_tmp.grp_ar  = &t10vtb_vtd_cl::VTD_M1_subg;

   grp_desc           = &T10VTB_tmp;

_6_byte_vtc:
   retU = base_DBC_Init_RootGrp(inst, grp_desc, orflags, parent);

   return retU;
}

const char t10vtb_vtd_cl::Desc[] =
"T10VTD video timing descriptor.\n"
"Data layout depends on 'T10_M' field value";

const dbc_subg_dsc_t t10vtb_vtd_cl::VTD_M0_subg = {
   .s_ctor   = &t10vtb_vtd_cl::group_new,
   .CodN     = "T10VTD_M0",
   .Name     = "Video Timing Descriptor",
   .Desc     = Desc,
   .type_id  = ID_T10VTD_M0 | T_SUB_GRP,
   .min_len  = sizeof(t10vtm0_t),
   .fcount   = 7,
   .inst_cnt = -1,
   .fields   = t10vtb_vtd_cl::vtd_m0_m1
};

const dbc_subg_dsc_t t10vtb_vtd_cl::VTD_M1_subg = {
   .s_ctor   = &t10vtb_vtd_cl::group_new,
   .CodN     = "T10VTD_M1",
   .Name     = "Video Timing Descriptor",
   .Desc     = Desc,
   .type_id  = ID_T10VTD_M1 | T_SUB_GRP,
   .min_len  = sizeof(t10vtm1_t),
   .fcount   = 7,
   .inst_cnt = -1,
   .fields   = t10vtb_vtd_cl::vtd_m0_m1
};


const edi_field_t t10vtb_vtd_cl::vtd_m0_m1[] = {
  //t10hdr_t
   {&EDID_cl::BitF8Val, 0, offsetof(t10vtm0_t, hdr), 0, 3, F_BFD|F_INT, 0, 0x7,
   "T_formula",
   "Timing formula for Reduced Blanking: RB2: 0b010, RB3: 0b011" },
   {&EDID_cl::BitVal, 0, offsetof(t10vtm0_t, hdr), 3, 1, F_INT|F_BIT, 0, 1,
   "F43_res",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, offsetof(t10vtm0_t, hdr), 4, 1, F_INT|F_BIT, 0, 1,
   "VR_HB",
   "reserved (0)" },
   {&EDID_cl::BitF8Val, VS_T710_3D_SUPP, offsetof(t10vtm0_t, hdr), 5, 2, F_BFD|F_INT|F_VS, 0, 3,
   "3D_support",
   "Support for 3D (stereo image):\n"
   "0b00: (0) not supported \n0b01: (1) only 3D supported\n"
   "0b10: (2) both mono and stereo modes supported \n0b11: (3) reserved\n\n." },
   {&EDID_cl::BitVal, 0, offsetof(t10vtm0_t, hdr), 7, 1, F_INT|F_BIT, 0, 1,
   "T10_Y420",
   "YCBCR 4:2:0 sampling mode support, 1: supported, 0: only RGB" },

   {&EDID_cl::Word16, 0, offsetof(t10vtm0_t, HApix), 0, 2, F_INT|F_PIX|F_DN, 0, 65535,
   "H-Active pix",
   "Horizontal active pixels (0–65535), X-resolution." },
   {&EDID_cl::Word16, 0, offsetof(t10vtm0_t, VAlin), 0, 2, F_INT|F_PIX|F_DN, 0, 65535,
   "V-Active lin",
   "Vertical active lines (0–65535), V-resolution." },
};

const edi_field_t t10vtb_vtd_cl::vtd_m0[] = {
   {&EDID_cl::ByteVal, 0, offsetof(t10vtm0_t, Vref8), 0, 1, F_INT|F_BTE|F_HZ|F_DN, 0, 255,
   "V-refresh",
   "Vertical refresh rate (0–255)" }
};

const edi_field_t t10vtb_vtd_cl::vtd_m1[] = {
   {&EDID_cl::BitF16Val, 0, offsetof(t10vtm1_t, Vref10), 0, 10, F_BFD|F_INT|F_HZ|F_DN, 0, 1023,
   "V-refresh",
   "Vertical refresh rate (0–1023)" },
   {&EDID_cl::BitF16Val, 0, offsetof(t10vtm1_t, Vref10), 10, 6, F_BFD|F_INT, 0, 0x3F,
   "res6",
   "reserved (0)" }
};

rcode t10vtb_vtd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;
   u32_t  dlen;
   u32_t  mx_fcnt;
   int    T10M; //Timing Descriptor size modifier

   bool   append_md = true;

   const edi_field_t *mx_ar;

   parent_grp = parent;

   if (parent != NULL) {
      //The T10VTDB block header is @ 3rd byte, after ET hdr
      T10M = reinterpret_cast<const t10hdr_t*> (&parent->getInstPtr()[2])->t10_m;
   } else {
      //Clone->Init: layout type encoded in type ID
      T10M = (ID_T10VTD_M1 == (type_id.t32 & ID_SUBGRP_MASK));
   }

   if (T10M == 0) {
      VTD_subg = &VTD_M0_subg;
      mx_ar    = vtd_m0;
      mx_fcnt  = 1;
   } else {
      VTD_subg = &VTD_M1_subg;
      mx_ar    = vtd_m1;
      mx_fcnt  = 2;
   }
   type_id.t32 = VTD_subg->type_id | orflags;
   dlen        = VTD_subg->min_len;
   dyn_fcnt    = VTD_subg->fcount;

   CopyInstData(inst, dlen);

   retU = init_fields(&vtd_m0_m1[0], inst_data, VTD_subg->fcount, !append_md,
                      VTD_subg->Name, VTD_subg->Desc, VTD_subg->CodN);

   //fields from vtd_m0_m1 are already initialized by base_DBC_Init_RootGrp()
   //remaining fields depend on T10M value
   retU = init_fields(mx_ar, inst_data, mx_fcnt, append_md, NULL, NULL, NULL);

   return retU;
}

edi_grp_cl* t10vtb_vtd_cl::Clone(rcode& rcd, u32_t flags) {
   t10vtb_vtd_cl *m_vtd;

   //pass T10_M layout type to init(parent==NULL)
   //type_id is ID_T10VTD_M0 or ID_T10VTD_M1
   m_vtd = new t10vtb_vtd_cl();
   m_vtd->type_id = type_id;

   return base_clone(rcd, m_vtd, flags);
};

void t10vtb_vtd_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   enum { //both M1 and M0 layouts have the same field indexes
      IDX_XRES = 5,
      IDX_VRES,
      IDX_VREF,
   };

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   edi_dynfld_t *p_field;
   u32_t         xres;
   u32_t         yres;
   u32_t         vref;

   //X-res
   EDID.gp_name.Empty();
   p_field = FieldsAr.Item(IDX_XRES);
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, xres, p_field );
   //Y-res
   EDID.gp_name.Empty();
   p_field = FieldsAr.Item(IDX_VRES);
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, yres, p_field );
   //V-Refresh: this works for any value of T10_M field: dynamic data layout
   EDID.gp_name.Empty();
   p_field = FieldsAr.Item(IDX_VREF);
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, vref, p_field );

   gp_name << xres << "x" << yres << " @ " << vref << "Hz";
}


//HF-EEODB: HDMI Forum EDID Extension Override Data Block (DBC_ET_HEOVR = 120)
const char cea_hfeeodb_cl::Desc[] =
"Overrides the extension block count of the base EDID block, allowing\n"
"more than one extension. Only valid at the start of the first CTA-861 block.";

const edi_field_t cea_hfeeodb_cl::fld_dsc[] = {
   {&EDID_cl::ByteVal, 0, 2, 0, 1, F_BTE|F_INT, 0, 0xFF, "EEODB_count",
   "Number of EDID extension blocks, overriding the base block count." }
};

const gpfld_dsc_t cea_hfeeodb_cl::fields = {
   .flags    = 0,
   .dat_sz   = 1,
   .inst_cnt = 1,
   .fcount   = 1,
   .fields   = cea_hfeeodb_cl::fld_dsc
};

const dbc_flatgp_dsc_t cea_hfeeodb_cl::HFEEODB_grp = {
   .CodN     = "HF-EEODB",
   .Name     = "HDMI Forum EDID Extension Override Data Block",
   .Desc     = Desc,
   .type_id  = ID_HFEEODB,
   .flags    = 0,
   .min_len  = 2,
   .max_len  = 2,
   .max_fld  = CEA_ETHDR_FCNT + 1 + 31,
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .fld_arsz = 1,
   .fld_ar   = &fields
};

rcode cea_hfeeodb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   return base_DBC_Init_FlatGrp(inst, &HFEEODB_grp, orflags, parent);
}

//HF-SCDB: HDMI Forum Sink Capability Data Block (DBC_ET_HSCDB = 121)
extern const edi_field_t HF_version_fld[];
extern const edi_field_t HF_tmds_fld[];
extern const edi_field_t HF_scdc_fld[];
extern const edi_field_t HF_frl_fld[];
extern const edi_field_t HF_vrr_mode_fld[];
extern const edi_field_t HF_vrr_fld[];
extern const edi_field_t HF_dsc_fld[];
extern const edi_field_t HF_dsc_frl_fld[];
extern const edi_field_t HF_dsc_chunk_fld[];

const char cea_hfscdb_cl::Desc[] =
"HDMI 2.1 sink capabilities: the HDMI Forum VSDB payload in an Extended Tag\n"
"block, preceded by two reserved bytes.";

const edi_field_t cea_hfscdb_cl::rsvd_fld_dsc[] = {
   {&EDID_cl::Word16, 0, 2, 0, 2, F_HEX|F_RD, 0, 0xFFFF, "reserved",
   "reserved (0)" }
};

const gpfld_dsc_t cea_hfscdb_cl::fld_grp[] = {
   { .flags = 0, .dat_sz = 2, .inst_cnt = 1, .fcount = 1,
     .fields = cea_hfscdb_cl::rsvd_fld_dsc },
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

const dbc_flatgp_dsc_t cea_hfscdb_cl::HFSCDB_grp = {
   .CodN     = "HF-SCDB",
   .Name     = "HDMI Forum Sink Capability Data Block",
   .Desc     = Desc,
   .type_id  = ID_HFSCDB,
   .flags    = T_FLEX_LAYOUT,
   .min_len  = 3,
   .max_len  = 31,
   .max_fld  = CEA_ETHDR_FCNT + 39 + 31,
   .hdr_fcnt = CEA_ETHDR_FCNT,
   .hdr_sz   = sizeof(ethdr_t),
   .fld_arsz = 10,
   .fld_ar   = cea_hfscdb_cl::fld_grp
};

rcode cea_hfscdb_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   return base_DBC_Init_FlatGrp(inst, &HFSCDB_grp, orflags, parent);
}

//UNK-ET: Unknown Data Block (Extended Tag Code)
const char cea_unket_cl::CodN[] = "UNK-ET";
const char cea_unket_cl::Name[] = "Unknown Data Block";
const char cea_unket_cl::Desc[] = "Unknown Extended Tag Code";

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode cea_unket_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode        retU;
   u32_t        dlen;
   edi_field_t *p_fld;

   //block length
   dlen   = reinterpret_cast <const ethdr_t*> (inst)->ehdr.hdr.tag.blk_len;

   parent_grp  = parent;
   type_id.t32 = ID_CEA_UETC; //unknown Extended Tag Code
   dlen       ++ ; //hdr

   CopyInstData(inst, dlen);

   dlen     -= 2; // -hdr, -ext_tag
   dyn_fcnt  = CEA_ETHDR_FCNT;

   //pre-alloc buffer for array of fields: hdr_fcnt + dlen
   dyn_fldar = (edi_field_t*) malloc( (dlen + dyn_fcnt) * EDI_FIELD_SZ );
   if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);

   p_fld   = dyn_fldar;

   //DBC-EXT header
   memcpy( p_fld, CEA_BlkHdr_fields, (dyn_fcnt * EDI_FIELD_SZ) );
   p_fld  += dyn_fcnt;

   dyn_fcnt += dlen;

   //payload data interpreted as unknown
   insert_unk_byte(p_fld, dlen, sizeof(ethdr_t) );

   retU = init_fields(&dyn_fldar[0], inst_data, dyn_fcnt, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"


