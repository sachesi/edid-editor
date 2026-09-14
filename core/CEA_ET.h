/***************************************************************
 * Name:      CEA_ET.h
 * Purpose:   CEA/CTA-861-G Extended Tag Codes
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2019-2025
 * License:   GPLv3+
 **************************************************************/

#ifndef CEA_ET_H
#define CEA_ET_H 1

#include "CEA.h"
#include "EDID.h"

#include "def_types.h"

typedef unsigned int uint;


enum Extended_Tag_Codes {
   DBC_ET_VCDB   =   0, //Video Capability Data Block
   DBC_ET_VSVD   =   1, //Vendor-Specific Video Data Block
   DBC_ET_VDDD   =   2, //VESA Display Device Data Block
   DBC_ET_RSV3   =   3, //Reserved for VESA Video Data Block (fixed in H-spec)
   DBC_ET_RSV4   =   4, //Reserved for HDMI Video Data Block
   DBC_ET_CLDB   =   5, //Colorimetry Data Block
   DBC_ET_HDRS   =   6, //HDR Static Metadata Data Block
   DBC_ET_HDRD   =   7, //HDR Dynamic Metadata Data Block
   //  8..12            //! Reserved for video-related blocks
   DBC_ET_VFPD   =  13, //Video Format Preference Data Block
   DBC_ET_Y42V   =  14, //YCBCR 4:2:0 Video Data Block
   DBC_ET_Y42C   =  15, //YCBCR 4:2:0 Capability Map Data Block
   DBC_ET_RS16   =  16, //Reserved for CTA Miscellaneous Audio Fields
   DBC_ET_VSAD   =  17, //Vendor-Specific Audio Data Block
   DBC_ET_HADB   =  18, //HDMI Audio Data Block
   DBC_ET_RMCD   =  19, //Room Configuration Data Block
   DBC_ET_SLDB   =  20, //Speaker Location Data Block
   // 21..31            //! Reserved for audio-related blocks
   DBC_ET_IFDB   =  32, //InfoFrame Data Block (includes one or more Short InfoFrame Descriptors)
   // 33                //! Reserved
   DBC_ET_T7VTB  =  34, //DisplayID Type VII Video Timing Data Block
   DBC_ET_T8VTB  =  35, //DisplayID Type VIII Video Timing Data Block
   // 36..41            //! Reserved
   DBC_ET_T10VTB =  42, //DisplayID Type X Video Timing Data Block
   // 43..119           //! Reserved
   DBC_ET_HEOVR  = 120, //HDMI Forum EDID Extension Override Data Block
   DBC_ET_HSCDB  = 121, //HDMI Forum Sink Capability Data Block
   // 122..127          //! Reserved for HDMI
   // 128..255          //! Reserved
};


//-------------------------------------------------------------------------------------------------
//CEA-DBC Extended Tag Codes

//VCDB: Video Capability Data Block (DBC_ET_VCDB = 0)
typedef struct __attribute__ ((packed)) { //infoblk_s
   uint    S_CE01  :2;  //bit0,1: CE overscan/underscan: 0- not supp. 1- always oversc, 2- always undersc, 3- both.
   uint    S_IT01  :2;  //bit2,3: IT overscan/underscan: 0- not supp. 1- always oversc, 2- always undersc, 3- both.
   uint    S_PT01  :2;  //bit4,5: PT overscan/underscan: 0- not supp. 1- always oversc, 2- always undersc, 3- both.
   uint    QS      :1;  //bit6  : Quantization Range Selectable: 1 = selectable via AVI Q (RGB only), 0= no data
   uint    QY      :1;  //bit7  : Quantization Range: 1= selectable via AVI YQ (YCC only), 0= no data
} vdbc0_t;

typedef struct __attribute__ ((packed)) { //vdbc_s
   vdbc0_t  vdbc0;  //2: payload
} vcdb_t;

//VSVD: Vendor-Specific Video Data Block (DBC_ET_VSVD =  1)
//VSAD: Vendor-Specific Audio Data Block (DBC_ET_VSAD = 17)
typedef struct __attribute__ ((packed)) { //vsd_s
   u8_t  ieee_id[3]; //2-4: IEEE OUI (Organizationally Unique Identifier)

   //payload (ethdr.ehdr.hdr.tag.blk_len - 4) bytes, max 27
} vs_vadb_t;

//VDDD: VESA Display Device Data Block (DBC_ET_VDDD = 2)
typedef struct __attribute__ ((packed)) { //byte 2
   uint  n_lanes   :4; //number of lanes/channels
   uint  type      :4; //interface type
} if_type_t;

typedef struct __attribute__ ((packed)) { //byte 3: Interface Standard Version and Release Number
   uint  release   :4; //release
   uint  version   :4; //version
} if_ver_t;

typedef struct __attribute__ ((packed)) { //min/max clock frequency for each iface link/channel
   uint  fmax_2msb :2; //byte5: bit0,1: 2 msb bits of max freq
   uint  fmin      :6; //byte5: bit2-7: 6 bits: min. freq in MHz (0-63)
   u8_t  fmax_8lsb ;   //byte6:         8 lsb bits of max freq in MHz (0-1023)
} clkfrng_t;

typedef struct __attribute__ ((packed)) {
   uint  scan_dir :2; //bit0,1: scan direction
   uint  pix0_pos :2; //bit2,3: zero pixel location (scan start)
   uint  rotation :2; //bit4,5: rotation capability
   uint  orientat :2; //bit6,7: default display orientation
} scanrot_t;

typedef struct __attribute__ ((packed)) { //Miscellaneous Display capabilities
   uint  resvd02  :3; //bit0-2: reserved
   uint  de_ilace :1; //bit3  : deinterlacing, 1= possible
   uint  ovrdrive :1; //bit4  : overdrive not recommended, 1= src "should" not overdrive the signal by default
   uint  d_drive  :1; //bit5  : direct-drive, 0= no direct-drive
   uint  dither   :2; //bit6,7: dithering, 0=no, 1=spatial, 2=temporal, 3=both.
} misccaps_t;

typedef struct __attribute__ ((packed)) { //Audio flags
   uint  resvd04  :5; //bit0-4: reserved
   uint  audio_ovr:1; //bit5  : audio input override, 1= automatically override other audio inputs
   uint  sep_achn :1; //bit6  : separate audio channel inputs: not via video interface
   uint  vid_achn :1; //bit7  : 1= audio support on video interface
} audioflg_t;

typedef struct __attribute__ ((packed)) { //Audio delay
   uint  delay    :7; //bit0-6: delay in 2ms resolution. if dly> 254, the value is set to 127, 0= no delay compensation.
   uint  dly_sign :1; //bit7  : audio delay sign: 1= '+', 0= '-'
} audiodly_t;

typedef struct __attribute__ ((packed)) { //Frame rate or mode conversion
   uint  frm_delta:6; //byte19, bit0-5: Frame rate delta range: max FPS deviation from default frame rate (+/- 63 FPS)
   uint  conv_md  :2; //byte19, bit6-7: available conversion mode: 0=none, 1=single frm buff, 2=double buff, 3=interframe interpolation
   u8_t  frm_rate ;   //byte20        : default frame rate (FPS)
} frm_rcnv_t;

typedef struct __attribute__ ((packed)) { //color bit depth (for each color)
   uint  cbd_dev  :4; //bit0-3: color bit depth: display device: value=cbd-1 (1-16)
   uint  cbd_if   :4; //bit4-7: color bit depth: interface: value=cbd-1 (1-16)
} color_bd_t;

//22-29 : Aadditional Chromaticity Coordinates. 10-bit CIE xy coords [0–1023/1024].
//        Used also to identify sub-pixels defined in byte 13 (sub-pixel layout code)
//22    : color 5 and 4 least-significant bits
typedef struct __attribute__ ((packed)) { //rgxy_lsb_s
   uint col5_y  :2; //bit 1-0 : primary color5 Y-coord, 2 lsb
   uint col5_x  :2; //bit 3-2 : primary color5 X-coord, 2 lsb
   uint col4_y  :2; //bit 5-4 : primary color4 Y-coord, 2 lsb
   uint col4_x  :2; //bit 7-6 : primary color4 X-coord, 2 lsb
} c54xy_lsb_t;

//23 : Blue and white least-significant bits
typedef struct __attribute__ ((packed)) { //bwxy_lsb_s
   uint n_colors:2; //bit 1-0 : num of additional chromaticity coordinates
   uint resvd32 :2; //bit 3-2 : reserved 0
   uint col6_y  :2; //bit 5-4 : primary color6 Y-coord, 2 lsb
   uint col6_x  :2; //bit 7-6 : primary color6 X-coord, 2 lsb
} c6xy_lsb_t;

//Additional Chromaticity coordinates : base struct
typedef struct __attribute__ ((packed)) { //chromxy_s
   c54xy_lsb_t rgxy_lsbits; //22 : primarycolor 5 and 4 least-significant bits
   c6xy_lsb_t  bwxy_lsbits; //23 : primary color6 ls 2 bits, n colors.
   u8_t        col4_8h_x;   //24 : primary color4 X-coord, 8 msb. 0–255 -> 0–0.996 (255/256); 0–0.999 -> (1023/1024) with lsbits
   u8_t        col4_8h_y;   //25 : primary color4 Y-coord, 8 msb
   u8_t        col5_8h_x;   //26 : primary color5 X-coord, 8 msb
   u8_t        col5_8h_y;   //27 : primary color5 Y-coord, 8 msb
   u8_t        col6_8h_x;   //28 : primary color6 X-coord, 8 msb
   u8_t        col6_8h_y;   //29 : primary color6 Y-coord, 8 msb
} addchrxy_t;

typedef struct __attribute__ ((packed)) { //30: Display response time [ms]
   uint  time   :7; //bit0-6: response time in milliseconds, max 126. 127 means greater than 126, but unspecified
   uint  type   :1; //bit7  : 1=black-to-white, 0=white-to-black
} resp_tim_t;

typedef struct __attribute__ ((packed)) { //31: Overscan information [%], 0=no overscan (but doesn't mean 100% match), max 15%
   uint  V_ovscan :4; //bit0-3: vertical
   uint  H_ovscan :4; //bit4-7: horizontal
} overscan_t;

//VESA Display Device Data Block: Base struct
typedef struct __attribute__ ((packed)) { //vdddb_s
   ethdr_t    ethdr;     //0,1  : header + extended tag code
   if_type_t  if_type;   //2    : interface type
   if_ver_t   if_version;//3    : interface version
   u8_t       cont_prot; //4    : Content Protection support method
   clkfrng_t  clkfrng;   //5,6  : min/max clock frequency
   u16_t      Hpix_cnt;  //7,8  : count of pixels in horizontal direction -1 (1-65536)
   u16_t      Vpix_cnt;  //9,10 : count of pixels in vertical direction -1 (1-65536)
   u8_t       aspect;    //11   : aspect ratio: 100*((long_axis/short_axis)-1)
   scanrot_t  scanrot;   //12   : scan direction, zero pixel pos, rotation, orientation
   u8_t       subpixlc;  //13   : sub-pixel layout code
   u8_t       Hpix_pitch;//14   : horizontal pixel pitch in 0.01mm (0.00 - 2.55)
   u8_t       Vpix_pitch;//15   : vertical pixel pitch in 0.01mm (0.00 - 2.55)
   misccaps_t misccaps;  //16   : miscellaneous display caps.
   audioflg_t audioflg;  //17   : Audio flags
   audiodly_t audiodly;  //18   : Audio delay
   frm_rcnv_t frm_rcnv;  //19,20: Frame rate and mode conversion
   color_bd_t colbitdpth;//21   : Color bit depth
   addchrxy_t addchrxy;  //22-29: Additional Chromaticity Coordinates
   resp_tim_t resp_time; //30   : Display response time [ms]
   overscan_t overscan;  //31   : Overscan information [%]
} vdddb_t;

//VVTB: VESA Video Timing Block Extension (DBC_ET_RSV3 = 3)

//NOTE: The structure size is 128 bytes, while the DBC Extended Tag header allows only
//      blocks of size <= 30 bytes!
//      Extended Tag Code=3 will be reported as error.

//vtbe structure layout
//w_nDTB*18 + y_nCVT*3 + z_nSTB*2 <= 122 (max VVTBE block size)
typedef struct __attribute__ ((packed)) {
   u8_t   w_nDTB;  //num of Detailed Timing Blocks (18 bytes), n<=6.
   u8_t   y_nCVT;  //num of Coordinated Video Timing blocks (3 bytes), n<=40 (0x28).
   u8_t   z_nSTB;  //num of Standard Timing descriptor blocks (2 bytes), n<=61 (0x3D).
} vtblayout_t;

//from EDID.h: dtd_t: DTD: Detailed Timing Descriptor, class dtd_cl

//CVTD: Coordinated Video Timing  Descriptor
typedef struct __attribute__ ((packed)) {
   u8_t   VSize_8l  ;    //byte0: Vsize 8lsb, val=(VActivePix/2)-1
   uint   resvd01   :2;  //byte1: bit0,1 : reserved 0
   uint   asp_ratio :2;  //byte1: bit2,3 : aspect ratio: 0= 4:3, 1=16:9, 2=16:10, 3= reserved
   uint   VSize_4h  :4;  //byte1: bit4-7 : Vsize 4msb
   uint   rblank60  :1;  //byte2: bit0   : 60Hz Reduced Blanking 1= supported.
   uint   vref85    :1;  //byte2: bit1   : 85Hz refresh rate, std. blanking, 1= supported.
   uint   vref75    :1;  //byte2: bit2   : 75Hz refresh rate, std. blanking, 1= supported.
   uint   vref60    :1;  //byte2: bit3   : 60Hz refresh rate, std. blanking, 1= supported.
   uint   vref50    :1;  //byte2: bit4   : 50Hz refresh rate, std. blanking, 1= supported.
   uint   pref_vr   :2;  //byte2: bit5,6 : preferred refresh rate: 0= 50Hz, 1= 60Hz, 2= 75Hz, 3= 85Hz
   uint   resvd7    :1;  //byte2: bit7   : reserved 0
} cvtd_t;

//from EDID.h: dmt_std2_t: STD: Standard Timing Descriptor, class dmt_std2_cl

//VVTB: VESA Video Timing Block Extension: Base struct
//max block size: 122 bytes
typedef struct __attribute__ ((packed)) { //vvtbe_s
   ethdr_t     ethdr;     //0,1 :       header + extended tag code
   u8_t        vesa_tag;  //2   : 0   : Fixed 0x10, Tag label by VESA
   u8_t        vesa_ver;  //3   : 1   : Fixed 0x01, version number
   vtblayout_t layout;    //4-6 : 2-4 : vtbe structure layout
   //DTD, CVTD, STD blocks
} vvtbe_t;


//CLDB: Colorimetry Data Block (DBC_ET_CLDB = 5)
typedef struct __attribute__ ((packed)) {
   uint       xvYCC601   :1; //2   : 0   : Standard Definition Colorimetry based on IEC 61966-2-4
   uint       xvYCC709   :1; //2   : 1   : High Definition Colorimetry based on IEC 61966-2-4
   uint       sYCC601    :1; //2   : 2   : Colorimetry based on IEC 61966-2-1/Amendment 1
   uint       opYCC601   :1; //2   : 3   : Colorimetry based on IEC 61966-2-5, Annex A
   uint       opRGB      :1; //2   : 4   : Colorimetry based on IEC 61966-2-5
   uint       BT2020cYCC :1; //2   : 5   : Colorimetry based on ITU-R BT.2020, Y’cC’BCC’RC
   uint       BT2020YCC  :1; //2   : 6   : Colorimetry based on ITU-R BT.2020, Y’C’BC’R
   uint       BT2020RGB  :1; //2   : 7   : Colorimetry based on ITU-R BT.2020, R’G’B’
} cldb2_t;

typedef struct __attribute__ ((packed)) {
   uint       MD03       :4; //3   : 0-3 : designated for future gamut-related metadata. As yet undefined, this metadata is carried in an interface-specific way.
   uint       resvd46    :3; //3   : 4-6 : reserved
   uint       DCI_P3     :1; //3   : 7   : Colorimetry based on DCI-P3
} cldb3_t;

//CLDB: Colorimetry Data Block: base struct
typedef struct __attribute__ ((packed)) { //cldb_s
   cldb2_t    cldb2      ; //2
   cldb3_t    cldb3      ; //3
} cldb_t;

//HDRS: HDR Static Metadata Data Block (DBC_ET_HDRS = 6)
typedef struct __attribute__ ((packed)) {
   uint       ET_0    :1; //2   : 0   : Traditional gamma - SDR Luminance Range
   uint       ET_1    :1; //2   : 1   : Traditional gamma - HDR Luminance Range
   uint       ET_2    :1; //2   : 2   : SMPTE ST 2084
   uint       ET_3    :1; //2   : 3   : Hybrid Log-Gamma (HLG) based on Recommendation ITU-R BT.2100-0
   uint       ET_4    :1; //2   : 4   : reserved
   uint       ET_5    :1; //2   : 5   : reserved
   uint       resvd67 :2; //2   : 6,7 : reserved
} hdrs2_t;

typedef struct __attribute__ ((packed)) {
   uint       SM_0    :1; //3   : 0   : Static Metadata Type 1
   uint       SM_17   :7; //3   : 1-7 : reserved
} hdrs3_t;

//HDRS: HDR Static Metadata Data Block: base struct
typedef struct __attribute__ ((packed)) { //hdrs_s
   hdrs2_t    hdrs2   ; //2
   hdrs3_t    hdrs3   ; //3
   u8_t       max_lum ; //4   Desired Content Max Luminance data (8 bits)
   u8_t       avg_lum ; //5   Desired Content Max Frame-average Luminance data (8 bits)
   u8_t       min_lum ; //6   Desired Content Min Luminance data (8 bits)
} hdrs_t;

//HDRD: HDR Dynamic Metadata Data Block (DBC_ET_HDRD = 7)
//dynamic data layout: generated by cea_hdrd_cl::init
typedef struct __attribute__ ((packed)) {
   uint       version :4; // 0-3 : metadata version for metadata types 1,2,4
   uint       resvd47 :4; // 4-7 : reserved
} hdrs_flg_t;

typedef struct __attribute__ ((packed)) { //vvtbe_s
   u8_t       mtd_len ; //0   metadata length (excluding this byte)
   u16_t      mtd_type; //1,2 metadata type: 0x0001..4
   hdrs_flg_t supp_flg; //3   Support Flags for metadata type != 0x0003
} hdrd_mtd_t;

//Extended InfoFrame Type, HDR metadata format, used also for HDRD:
enum {
   //          0x0000    reserved
   HDR_dmtd1 = 0x0001, //HDR Dynamic Metadata according to the syntax, specified in Annex R
   HDR_dmtd2 = 0x0002, //HDR Dynamic Metadata carried in Supplemental Enhancement Information (SEI) messages, according to ETSI TS 103 433
   HDR_dmtd3 = 0x0003, //HDR Dynamic Metadata carried in Color Remapping Information SEI message according to ITU-T H.265
   HDR_dmtd4 = 0x0004  //HDR Dynamic Metadata carried according to the syntax specified in Annex S
   //          0x....    reserved
};

//VFPD: Video Format Preference Data Block (DBC_ET_VFPD = 13)
//dynamic data layout: generated by cea_vfpd_cl::init

//Y42V: YCBCR 4:2:0 Video Data Block (DBC_ET_Y42V = 14)
//dynamic data layout: generated by cea_y42v_cl::init

//Y42C: YCBCR 4:2:0 Capability Map Data Block (DBC_ET_Y42C = 15)
//dynamic data layout: generated by cea_y42c_cl::init
//string: dynamic field names for bitmap bits: SVD numbers
typedef union __attribute__ ((packed)) y42c_svdn_u {
   char svd_num[8];
   typedef struct __attribute__ ((packed)) {
      u32_t    svd ; // 0-3 : string: 'SVD_'
      u32_t    num ; // 4-7 : string: svd number, max 243
   } part;
} y42c_svdn_t;

//HADB: HDMI Audio Data Block (DBC_ET_HADB = 18)
typedef struct __attribute__ ((packed)) { //hadb_hdr_s
   //DBC-ET: byte3
   uint   max_strm_cnt :2;    //0,1 : max number of audio streams in Multi-Stream Audio packets
                              //      0b00: no MSA support, 0b01: 2 streams
                              //      0b10: 2 or 3 streams, 0b11: 2,3 or 4 streams
   uint   ms_not_mix   :1;    //2   : 1: supports mixing of additional streams
   uint   res3_37      :5;    //3-7 : reserved 0
   //DBC-ET: byte4
   uint   num_a3dsc    :3;    //1-2 : number of 3D audio descriptors, (0..7), max 6
   uint   res4_37      :5;    //3-7 : reserved 0
} hadb_hdr_t;
//HADB HDMI_3D_AD == CTA::SAD (short audio desc):

//ACAT: Audio Channel Allocation Type
typedef struct __attribute__ ((packed)) { //hadb_acat_s
   uint   f03_res   :4; //0-3 : reserved 0
   uint   acat      :3; //4-7 : ACAT: 0..7, valid 1,2,3
} hadb_acat_t;

//SAB3D: HDMI_3D_SAD (Speaker Allocation Data Block)
typedef struct __attribute__ ((packed)) { //hadb_sab_s
   sab_t       sab;  //0-3
   hadb_acat_t acat; //4
} hadb_sab_t;


//RMCD: Room Configuration Data Block (DBC_ET_RMCD = 19)
typedef struct __attribute__ ((packed)) {
   uint       spk_cnt :5; // 0-4 : Speaker Count: number of LPCM_channels -1, required for SLD=1
   uint       SLD     :1; // 5   : SLD: Speaker Location Descriptor, 1= fields Xmax, Ymax and Zmax are valid
   uint       Speaker :1; // 6   : Speaker: 1= spk_cnt is valid, 0= spk_cnt is undefined
   uint       Display :1; // 7   : Display: 1= DisplayX,Y,Z fields are valid
} rmcd_hdr_t;

typedef struct __attribute__ ((packed)) {
   spm0_t     spm0;     //3  : Speaker Presence Mask, byte0
   spm1_t     spm1;     //4  : Speaker Presence Mask, byte1
   spm2_t     spm2;     //5  : Speaker Presence Mask, byte2
} spk_mask_t;

typedef struct __attribute__ ((packed)) {
   u8_t       Xmax;     //6 X-axis distance from Primary Listening Position (PLP) in decimeters [dm]
   u8_t       Ymax;     //7 Y-axis distance from PLP in decimeters [dm]
   u8_t       Zmax;     //8 Z-axis distance from PLP in decimeters [dm]
} spk_plpd_t;

typedef struct __attribute__ ((packed)) {
   u8_t       DisplayX; //9  coordinate value normalized to Xmax in spk_plpd_t ?? CTA-861-G: unspecified normalization procedure
   u8_t       DisplayY; //10 normalized to Ymax
   u8_t       DisplayZ; //11 normalized to Zmax
} disp_xyz_t;

typedef struct __attribute__ ((packed)) { //sab_s
   rmcd_hdr_t rmcd_hdr; //2   : RMCD data header
   spk_mask_t spk_mask; //3-5 : SPM: Speaker Presence Mask
   spk_plpd_t spk_plpd; //6-8 : max speaker distance from PLP in X/Y/Z axes
   disp_xyz_t disp_xyz; //9-11: display coordinates in X/Y/Z axes, normalized to corresponding spk_xyz values.
} rmcd_t;

//SLDB: Speaker Location Data Block (DBC_ET_SLDB = 20)
typedef struct __attribute__ ((packed)) {
   uint       chn_idx :5; // 0-4 : Channel index 0..31
   uint       active  :1; // 5   : 1= channel active, 0= unused
   uint       coord   :1; // 6   : 1= CoordX/Y/Z fields are used, 0= CoordX/Y/Z fields are not present in SLDB
   uint       resvd7  :1; // 7   : reserved
} sldb_chn_t;

typedef struct __attribute__ ((packed)) {
   uint       spk_id  :5; // 0-4 : Speaker index 0..31, CTA-861-G: Table 34
   uint       resvd5  :1; // 5   : reserved
   uint       resvd6  :1; // 6   : reserved
   uint       resvd7  :1; // 7   : reserved
} sldb_id_t;

typedef struct __attribute__ ((packed)) { //Speaker Location Descriptor
   sldb_chn_t channel; //0 Channel index 0..31, flags: active, coord.
   sldb_id_t  spk_id;  //1 Speaker index 0..31
   i8_t       CoordX;  //2 speaker position value normalized to Xmax in RMCD: SPKD: Speaker Distance (?)
   i8_t       CoordY;  //3 normalized to Ymax
   i8_t       CoordZ;  //4 normalized to Zmax
} spkld_t;

typedef struct __attribute__ ((packed)) { //Speaker Location Data Block
   ethdr_t  ethdr    ;   //0,1 : header + extended tag code
   spkld_t  slocd[]  ;   //2-6 : array of Speaker Location Descriptors
} sldb_t;

//IFDB: InfoFrame Data Block (DBC_ET_IFDB = 32)
typedef struct __attribute__ ((packed)) { //Short InfoFrame Descriptor Header
   uint      ift_code :5; // 0-4 : InfoFrame Type Code, values 0x00, 0x01 reserved
   uint      blk_len  :3; // 5-7 : payload length in bytes; for svsifd_t it's the number of bytes after the ieee_id.
} sifdh_t;

typedef struct __attribute__ ((packed)) { //Short Vendor-Specific InfoFrame Descriptor Header
   sifdh_t   ifhdr;      //0  : Short InfoFrame Descriptor Header
   u8_t      ieee_id[3]; //1-3: IEEE OUI (Organizationally Unique Identifier)

   //payload (ethdr.ehdr.hdr.tag.blk_len - 4) bytes, max 27
} svsifd_t;

typedef struct __attribute__ ((packed)) { //InfoFrame Processing Descriptor Header
   uint     resvd04  :5; // 0-4 : reserved
   uint     blk_len  :3; // 5-7 : num of bytes following the next byte
   u8_t     n_VSIFs  ;   // num of additional Vendor-Specific InfoFrames (VSIFs) that can be received simultaneously, -1.
} ifpdh_t;

typedef struct __attribute__ ((packed)) { //InfoFrame Data Block
   ethdr_t  ethdr    ;   //0,1 : header + extended tag code
   ifpdh_t  ifpdh    ;   //2,3 : reserved
} ifdb_t;

//T7VTDB: DisplayID Type 7 Video Timing Data Block (DBC_ET_T7VTB = 34)
typedef struct __attribute__ ((packed)) {
   uint     t7rev    :3; //0-2 : block revision, 0b010 - other values are reserved in H spec.
   uint     dsc_pt   :1; //  3 : reserved for VESA DisplayID, for CTA must be ZERO
   uint     t7_m     :3; //4-6 : reserved 0b000: number of extra bytes and the ond of
                         //      the descriptor. ?? then what is the DBC length value for ??
   uint     f37_res  :1; //  7 : reserved 0
} t7hdr_t;

typedef struct __attribute__ ((packed)) {
   uint     t7aspect :4; //0-3 : Aspect ratio: (0-8):
                         //      0:  1/1 ,  1:  5/4 , 2:   4/3  , 3: 15/9, 4: 16/9,
                         //      5: 16/10,  6: 64/27, 7: 256/135,
                         //      8: HApixels/VApisels, other values are reserved.
   uint     t7ilace  :1; //  4 : Interlace support, not supported in CTA, must be ZERO
   uint     t7_3d    :2; //5-6 : 3D support:
                         //      0b00 not supported, 0b01 only 3D supported (stereo)
                         //      0b10 both mono and stereo modes supported, 0b11 reserrved
   uint     t7y420   :1; //  7 : YCBCR 4:2:0 sampling mode support, 1: supported.
} t7prop_t;

typedef struct __attribute__ ((packed)) {
   u8_t   pclk0; //Pixel clock bytes 0..2
   u8_t   pclk1;
   u8_t   pclk2;
} t7_pclk_t;

typedef union {
   u16_t  w16;
   struct __attribute__ ((packed)) {
      u8_t   Lbyte;
      u8_t   Hbyte;
   };
} t710_f16_t;

typedef union {
   u16_t  w16;
   struct __attribute__ ((packed)) {
      u8_t   Lbyte ;
      uint   H_7msb:7; // offset, Front Porch: 15 bits (0–32767)
      uint   sync  :1; // H/V sync polarity: 1: positive, 0: negative
   };
} t7f16s1_t;

typedef struct __attribute__ ((packed)) { //t7vtdb_s
   ethdr_t     ethdr   ; //0,1  : header + extended tag code
   t7hdr_t     t7_hdr  ; //2    : t7 header
   t7_pclk_t   pix_clk ; //3-5  : Pixel clock [kHz] (0-16777215)
   t7prop_t    t7_prop ; //6    : properties

   t710_f16_t  HApix   ; //7,8  : H-active pixels (0–65535)
   t710_f16_t  HBpix   ; //9,10 : H-blank pixels  (0–65535)
   t7f16s1_t   Hoffs   ; //11,12: H-offset, Front Porch: 15 bits (0–32767) + t7hsp: sync polarity bit
   t710_f16_t  HsyncW  ; //13,14: H-sync width    (0–65535)

   t710_f16_t  VAlin   ; //15,16: V-active lines (0–65535)
   t710_f16_t  VBpix   ; //17,18: V-blank lines  (0–65535)
   t7f16s1_t   Voffs   ; //19,20: V-offset, Front Porch: 15 bits (0–32767) + t7vsp: sync polarity bit
   t710_f16_t  VsyncW  ; //21,22: V-sync width    (0–65535)

} t7vtdb_t;

//T8VTDB: DisplayID Type 8 Video Timing Data Block (DBC_ET_T8VTB = 35)
//additional header fields:
typedef struct __attribute__ ((packed)) { //t8hdr_s
   uint   blk_rev   :3; //0-2 : Block revision, 0b001
   uint   tcs       :1; //  3 : Timing Code Size: 0: 1-byte codes, 1: 2-byte codes
   uint   f34_res   :1; //  4 : reserved 0
   uint   t8y420    :1; //  5 : T8Y420: 1: both RGB and YCBCR 4:2:0 supported, 0: only RGB
   uint   code_type :2; //6-7 : 0b00: DMT Timing Code, 1-byte or 2-byte. Other values are reserved.
} t8hdr_t;
//2 kind of timng descriptors can be used:
//TCS=0: DMT-ID 1-byte Video Timing Codes
//TCS=1: DMT-STD 2-byte codes (STI)

//T10VTDB: DisplayID Type 10 Video Timing Data Block (DBC_ET_T10VTB = 42)
//additional header fields:
typedef struct __attribute__ ((packed)) { //t10d_hdr_s
   uint   blk_rev   :3; //0-2 : Block revision, 0b000
   uint   f33_res   :1; //  3 : reserved 0
   uint   t10_m     :3; //4-6 : additional descriptor length, beyond default 6 bytes: 0b000 or 0b001
   uint   f37_res   :1; //  7 : reserved 0
} t10hdr_t;

//timing descriptor header:
typedef struct __attribute__ ((packed)) { //t8d_hdr_s
   uint   t_form    :3; //0-2 : timing formula: RB2: 0b010, RB3: 0b011
   uint   f43_res   :1; //  3 : reserved 0
   uint   vr_hb     :1; //  4 : Video Refresh Rate Option or Horizontal Blank Option,
                        //      depends on timing formula, table 111 in H spec
   uint   t10_3d    :2; //5-6 : 3D support: same as for T7VTDB
   uint   t10y420   :1; //  7 : T10Y420: 1: both RGB and YCBCR 4:2:0 supported, 0: only RGB
} t10d_hdr_t;

//timing descriptor T10_M=0
typedef struct __attribute__ ((packed)) { //t10vtm0_s
   t10d_hdr_t  hdr;   //0
   t710_f16_t  HApix; //1,2 : H-active pixels (0–65535)
   t710_f16_t  VAlin; //3,4 : V-active lines (0–65535)
   u8_t        Vref8; //5   : V-refresh rate (0..255) ???
} t10vtm0_t;

//timing descriptor T10_M=1
typedef union { //t10f16m1_s
   u16_t  w16;
   struct __attribute__ ((packed)) { //t10f16m1_s
      uint   Vref  :10; // 0-9  : 10 bit refresh rate with T10_M=1 (0..1023)
      uint   res6  : 6; //10-15 : reserved 0
   };
} t10f16m1_t;

typedef struct __attribute__ ((packed)) { //t10vtm1_s
   t10d_hdr_t  hdr;    //0
   t710_f16_t  HApix;  //1,2 : H-active pixels (0–65535)
   t710_f16_t  VAlin;  //3,4 : V-active lines (0–65535)
   t10f16m1_t  Vref10; //5   : V-refresh rate, 10bit, upper 6 bits are reserved (0..1023) ???
} t10vtm1_t;


#endif /* CEA_ET_H */
