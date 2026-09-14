/***************************************************************
 * Name:      EDID_base.cpp
 * Purpose:   EDID base data groups
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2014-2025
 * License:   GPLv3+
 **************************************************************/

#include "debug.h"
#include "rcdunits.h"
#ifndef idEDID_BASE
   #error "EDID_base.cpp: missing unit ID"
#endif
#define RCD_UNIT idEDID_BASE
#include "rcode/rcode.h"

#include "wxedid_rcd_scope.h"

RCD_AUTOGEN_DEFINE_UNIT

#include <stddef.h>

#include "vmap.h"
#include "EDID_class.h"
#include "CEA_class.h"
#include "CEA_ET_class.h"


//WX_DEFINE_OBJARRAY: no-op with wxc_PtrArray (see wxcompat.h)

//bad block length msg (defined in CEA.cpp)
extern const char ERR_GRP_LEN_MSG[];

//shared for CEA_ET_class.cpp: STI/DMT descriptions
extern const char stiXres8dsc   [];
extern const char stiVsyncDsc   [];
extern const char stiAspRatioDsc[];
extern const char stiDMT2B_Dsc  [];

//unknown/invalid byte field (defined in CEA.cpp)
extern const edi_field_t unknown_byte_fld;
extern void insert_unk_byte(edi_field_t *p_fld, u32_t len, u32_t s_offs);


//BED: Base EDID data
//BED: Base EDID data : handlers
rcode EDID_cl::MfcId(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode     retU;
   u8_t     *inst;
   mfc_id_u  mfc_swap;
   char      cbuff[4];

   inst = getValPtr(p_field);

   if (op == OP_READ) {
      //MfcId: ASCII letters A-Z less 0x40

      //swap bytes in PNP_ID - BE->LE byte order:
      mfc_swap.u16 = rdWord16_LE(inst);
      _LE_SWAP16(mfc_swap.u16);
      ival = mfc_swap.u16;

      cbuff[0] = (0x40+mfc_swap.mfc.ltr1);
      cbuff[1] = (0x40+mfc_swap.mfc.ltr2);
      cbuff[2] = (0x40+mfc_swap.mfc.ltr3);
      cbuff[3] = 0;
      sval = wxc_String::FromAscii(cbuff);
   } else {
      if (op == OP_WRINT) RCD_RETURN_FAULT(retU);
      if (sval.Len() != 3) RCD_RETURN_FAULT(retU);

      memcpy(cbuff, sval.ToAscii(), 3);
      mfc_swap.mfc.ltr1  = (cbuff[0]-0x40); //& 0x1F;
      mfc_swap.mfc.ltr2  = (cbuff[1]-0x40);
      mfc_swap.mfc.ltr3  = (cbuff[2]-0x40);
      mfc_swap.mfc.resvd = 0;

      _LE_SWAP16(mfc_swap.u16);
      wrWord16_LE(inst, mfc_swap.u16);
   }

   RCD_RETURN_OK(retU);
}

rcode EDID_cl::ProdSN(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u32_t *inst;

   inst = reinterpret_cast <u32_t*> (getValPtr(p_field)); //EDID_buff.edi.base.serial;

   if (op == OP_READ) {
      ival  = *inst;
      _BE_SWAP32(ival);
      sval <<  ival;
      RCD_SET_OK(retU);
   } else {
      ulong tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         if (! sval.ToULong(&tmpv, 10)) RCD_RETURN_FAULT(retU);
         ival = tmpv;
         RCD_SET_OK(retU);
      } else if (op == OP_WRINT) {
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;
      _BE_SWAP32(ival);
      *inst = ival;
   }
   return retU;
}

rcode EDID_cl::ProdWk(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   inst = getValPtr(p_field); //EDID_buff.edi.base.prodweek

   if (op == OP_READ) {
      sval << (int) *inst;
      ival = *inst;
      RCD_SET_OK(retU);
   } else {
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         ulong tmpv;
         retU = getStrUint(sval, 10, 0, 255, tmpv);
         if (! RCD_IS_OK(retU)) return retU;
         ival = tmpv;
      }
      if (op == OP_WRINT) {
         RCD_SET_OK(retU);
      }
      if (RCD_IS_OK(retU)) {
         if ((ival == 0) || ((ival > 52) && (ival != 255)) ) {
            RCD_RETURN_FAULT(retU);
         }
         *inst = ival;
      }
   }
   return retU;
}

rcode EDID_cl::ProdYr(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode  retU;
   u8_t  *inst;

   inst = getValPtr(p_field); //EDID_buff.edi.base.year

   if (op == OP_READ) {
      ival  = *inst;
      ival += 1990;
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong tmpv = 0;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, 1990, 2245, tmpv);
         if (! RCD_IS_OK(retU)) return retU;
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      tmpv -= 1990;
     *inst  = tmpv;
   }
   return retU;
}

//EDID: STI dynamic block name
void EDID_cl::STI_DBN(EDID_cl& /*EDID*/, wxc_String& dyngp_name, wxArGrpField& FieldsAr) {
   enum {
      STI_IDX_DMT2 = 3,
   };

   typedef struct {
      u32_t numrtr;
      u32_t denomr;
   } ratio_t;

   typedef union {
      u16_t       dmt2;
      dmt_std2_t  std2T;
   } stdT_u;

   //NOTE: not checking for EDID version < 1.3, 1st aspect ratio is fixed, 16:10
   ratio_t asp_ratio[4] = {
      {16, 10},
      { 4,  3},
      { 5,  4},
      {16,  9}
   };

   edi_dynfld_t     *p_field;
   const vmap_ent_t *m_ent;

   u32_t   xres;
   u32_t   yres;
   u32_t   aspr;
   u32_t   vref;
   stdT_u  STD2;

   //DMT STD2 code
   p_field   = FieldsAr.Item(STI_IDX_DMT2);
   STD2.dmt2 = rdWord16_LE(p_field->base);

   if (0x0101 == STD2.dmt2) {
      dyngp_name = "not used";
      return;
   }

   //STD2 code bytes are stored in reversed order:
   STD2.dmt2 = __bswap_16(STD2.dmt2);
   //check if the STD2 code is stanardised:
   m_ent = vmap_GetVmapEntry(VS_STD2_VIDFMT, VMAP_VAL, STD2.dmt2);

   if (m_ent != NULL) { //found std name, which can contain RB mark
      dyngp_name = wxc_String::FromAscii(m_ent->name);
      return;
   }

   STD2.dmt2 = __bswap_16(STD2.dmt2);
   //X-res
   xres   = STD2.std2T.HApix;
   xres  += 31;
   xres <<= 3;
   //aspect ratio
   aspr = STD2.std2T.asp_ratio;

   //V-Refresh
   vref  = STD2.std2T.v_ref;
   vref += 60;

   yres  = xres;
   yres *= asp_ratio[aspr].denomr;
   yres /= asp_ratio[aspr].numrtr;

   dyngp_name.Empty();
   dyngp_name << xres << "x" << yres << " @ " << vref << "Hz";

   dyngp_name << " (!std)";
}

//BED: Base EDID data
const char  edibase_cl::CodN[] = "BED";
const char  edibase_cl::Name[] = "Basic EDID data";
const char  edibase_cl::Desc[] = "Basic EDID data";

const edi_field_t edibase_cl::fields[] = {
   {&EDID_cl::ByteStr, 0, offsetof(edid_t, hdr), 0, 8, F_STR|F_HEX|F_RD|F_NI, 0, 0, "header",
   "*magic* byte sequence for EDID structure identification:\n 00 FF FF FF FF FF FF 00" },
   {&EDID_cl::MfcId, 0, offsetof(edid_t, mfc_id), 0, 2, F_STR|F_RD|F_NI|F_DN, 0, 0, "mfc_id",
   "Short manufacturer id: 3 capital letters.\n2 bytes in Big Endian byte order, "
   "5 bits per letter, 1 bit (msb) left reserved = 0." },
   {&EDID_cl::Word16, 0, offsetof(edid_t, prod_id), 0, 2, F_HEX|F_RD|F_DN, 0, 0xFFFF, "prod_id",
   "Product identifier number" },
   {&EDID_cl::ProdSN, 0, offsetof(edid_t, serial), 0, 4, F_INT|F_RD, 0, 0xFFFFFFFF, "serial",
   "Product serial number, 32bit" },
   {&EDID_cl::ProdWk, 0, offsetof(edid_t, prodweek), 0, 1, F_INT|F_RD, 0, 255, "prod_week",
   "Week of the year in which the product was manufactured.\n"
   "If week=0, then the field is not used.\n"
   "If week=255 (0xFF), then the Year field means the model release year." },
   {&EDID_cl::ProdYr, 0, offsetof(edid_t, year), 0, 1, F_INT|F_RD, 0, 255, "prod_year",
   "Year of manufacuring or model release,\n"
   "If week=255, then the value is a model release year.\n"
   "Values 0-255 are mapped to years 1990–2245" },
   {&EDID_cl::ByteVal, 0, offsetof(edid_t, edid_ver), 0, 1, F_BTE|F_INT|F_RD, 1, 1, "edid_ver",
   "EDID version" },
   {&EDID_cl::ByteVal, 0, offsetof(edid_t, edid_rev), 0, 1, F_BTE|F_INT|F_RD, 0, 4, "edid_rev",
   "EDID revision" },
   {&EDID_cl::ByteVal, 0, offsetof(edid_t, edid_rev)+1, 0, 1, F_BTE|F_INT|F_RD, 0, 3, "num_extblk",
   "Number of extension blocks.\n\n"
   "If num_extblk==0, extension block is not saved/exported, regardles of whether it was loaded or not.\n"
   "If num_extblk==1, and the extension block is not present, Saving/Exporting functions will fail.\n"
   "If num_extblk >1, ERROR: wxEDID supports only 1 extension of type CTA-861, "
   "Saving/Exporting functions will fail."},
   {&EDID_cl::ByteVal, 0, offsetof(edid_t, edid_rev)+2, 0, 1, F_BTE|F_HEX|F_RD, 0, 255, "checksum",
   "Block checksum:\n [sum_of_bytes(0-127) mod 256] + checkum_field_val must be equal to zero.\n\n"
   "NOTE: this byte is physically located at the end of the block (offset 127)" }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode edibase_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   static const u32_t fcount = 10;

   rcode         retU;
   const edid_t *pedid;

   pedid       = reinterpret_cast<const edid_t*> (inst);
   parent_grp  = parent;
   type_id.t32 = ID_BED | T_GRP_FIXED;
   abs_offs    = offsetof(edid_t, hdr); //0
   rel_offs    = abs_offs;

   //num_extblk & chksum are copied to local buffer,
   //so the corresponding field offsets are changed
   CopyInstData(inst, offsetof(edid_t, vinput_dsc)); //20: last byte is edid_t.edid_rev (19).
   hdr_sz              = dat_sz;
   inst_data[dat_sz++] = pedid->num_extblk;
   inst_data[dat_sz++] = pedid->chksum;

   retU = init_fields(&fields[0], inst_data, fcount, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

void edibase_cl::SpawnInstance(u8_t *pinst) {
   u32_t   dsz;
   edid_t *pedid;

   pedid  = reinterpret_cast<edid_t*> (pinst);
   dsz    = dat_sz;
   dsz   -= 2; //num_extblk & chksum

   memcpy(pinst, inst_data, dsz);

   pedid->num_extblk = inst_data[dsz++];
   pedid->chksum     = inst_data[dsz  ];
}

void edibase_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   edi_dynfld_t *p_field;
   u32_t         ival;

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   //MFC ID
   p_field = FieldsAr.Item(1);
   EDID.gp_name.Empty();
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
   gp_name = EDID.gp_name;
   //Prod ID
   p_field = FieldsAr.Item(2);
   EDID.gp_name.Empty();
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
   gp_name << ", prod_ID " << EDID.gp_name;
}

//VID: Video Input Descriptor
//VID: input type selector.
sm_vmap VID_input_map = {
   {0, {0, "Analog" , NULL}},
   {1, {0, "Digital", NULL}}
};

sm_vmap VID_if_type_map = {
   {0, {0, "undefined"  , NULL}},
   {1, {0, "DVI"        , NULL}},
   {2, {0, "HDMI-a"     , NULL}},
   {3, {0, "HDMI-b"     , NULL}},
   {4, {0, "MDDI"       , NULL}},
   {5, {0, "DisplayPort", NULL}}
};

sm_vmap VID_depth_map = {
   {0, {0, "undefined"  , NULL}},
   {1, {0, "6 bits"     , NULL}},
   {2, {0, "8 bits"     , NULL}},
   {3, {0, "10 bits"    , NULL}},
   {4, {0, "12 bits"    , NULL}},
   {5, {0, "14 bits"    , NULL}},
   {6, {0, "16 bits"    , NULL}}
};

//VID: Video Input Descriptor : handlers
const char  vindsc_cl::CodN[] = "VID";
const char  vindsc_cl::Name[] = "Video Input Descriptor";
const char  vindsc_cl::Desc[] = "Video Input Descriptor.\n"
"Some fields have different meaning depending on input type. This results in "
"that some fields are overlapped and changing one value will change the other.\n\n"
"NOTE: Bits 1-6: Digital input: Mandatory zero for EDID version < v1.4.\n";

//VID: bit7=0, analog input
const edi_field_t vindsc_cl::in_analog[] = {
   {&EDID_cl::BitVal, VS_VID_INPUT, 0, 7, 1, F_BIT|F_VS|F_FR|F_DN, 0, 1, "Input Type",
   "Bit7: Input signal type: 1=digital, 0=analog" },
   {&EDID_cl::BitVal, 0, 0, 0, 1, F_BIT|F_FR, 0, 1, "vsync",
   "Bit0: Analog input: 1= if composite sync/sync-on-green is used, VSync pulse is \"serrated\". "
   "This is more commonly known as \"SandCastle Pulse\" - created from overlapped H-sync "
   "and V-sync pulses during V-Blank time." },
   {&EDID_cl::BitVal, 0, 0, 1, 1, F_BIT, 0, 1, "sync_green",
   "Bit1: Analog input: Sync_on_green supported (SOG)." },
   {&EDID_cl::BitVal, 0, 0, 2, 1, F_BIT, 0, 1, "comp_sync",
   "Bit2: Analog input: Composite sync (on HSync line) supported" },
   {&EDID_cl::BitVal, 0, 0, 3, 1, F_BIT, 0, 1, "sep_sync",
   "Bit3: Analog input: Separate sync supported" },
   {&EDID_cl::BitVal, 0, 0, 4, 1, F_BIT, 0, 1, "blank_black",
   "Bit4: Analog input: Blank-to-black setup or pedestal per appropriate Signal Level Standard expected" },
   {&EDID_cl::BitF8Val, 0, 0, 5, 2, F_BFD, 0, 3, "sync_wh_lvl",
   "Bits 5,6: Analog input: Video white and sync levels, relative to blank:\n"
   "00: +0.7/−0.3 V;\n"
   "01: +0.714/−0.286 V;\n"
   "10: +1.0/−0.4 V;\n"
   "11: +0.7/0 V" }
};
//VID: bit7=1, digital input
const edi_field_t vindsc_cl::in_digital[] = {
   {&EDID_cl::BitVal, VS_VID_INPUT, 0, 7, 1, F_BIT|F_VS|F_FR|F_DN, 0, 1, "Input Type",
   "Bit7: Input signal type: 1=digital, 0=analog" },
   {&EDID_cl::BitVal, 0, 0, 0, 1, F_BIT, 0, 1, "VESA compat",
   "Bit0: Digital input: EDID v1.3: compatibility with VESA DFP v1.x TMDS CRGB, 1 pix per clock, "
   "up to 8 bits per color, MSB aligned, DE active high.\n\n"
   "NOTE#1: In EDID v1.4 this bit has changed meaning: LSB of \"Digital Interface Type\"\n"
   "NOTE#2: This field overlap fields defined for Analog input." },
   {&EDID_cl::BitF8Val, VS_VID_IF_TYPE, 0, 0, 4, F_BFD|F_VS, 0, 0xF, "IF Type",
   "Bits 0-3: EDID v1.4: Digital input: Digital Interface Type:\n"
   "0000  = undefined / mandatory zero for EDID v1.3\n"
   "0001  = DVI / EDIDv1.3: VESA DFP v1.x compatibility flag\n"
   "0010  = HDMI-a\n"
   "0011  = HDMI-b\n"
   "0100  = MDDI\n"
   "0101  = DisplayPort\n"
   ">0101 = reserved\n\n"
   "NOTE: This field overlap fields defined for Analog input." },
   {&EDID_cl::BitF8Val, VS_VID_COLOR_DEPTH, 0, 4, 3, F_BFD|F_VS, 0, 0x7, "Color Depth",
   "Bits 4-6: Digital input: Color Bit Depth (bits per primary color):\n"
   "000 = undefined / mandatory zero for EDID v1.3\n"
   "001 = 6\n"
   "010 = 8\n"
   "011 = 10\n"
   "100 = 12\n"
   "101 = 14\n"
   "110 = 16\n"
   "111 = reserved\n\n"
   "NOTE: This field overlap fields defined for Analog input." },
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode vindsc_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_VID | T_GRP_FIXED;
   abs_offs    = offsetof(edid_t, vinput_dsc);
   rel_offs    = abs_offs;

   CopyInstData(inst, sizeof(vid_in_t));

   //pre-alloc buffer for array of fields
   dyn_fldar = (edi_field_t*) malloc( max_fcnt * sizeof(edi_field_t) );
   if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);

   //no error possible, init_fields is always called.
   retU = gen_data_layout(inst_data);

   retU = init_fields(&dyn_fldar[0], inst_data, dyn_fcnt, false, Name, Desc, CodN);

   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

rcode vindsc_cl::ForcedGrpRefresh() {
   rcode retU;

   //clear fields
   memset( (void*) dyn_fldar, 0, ( max_fcnt * EDI_FIELD_SZ ) );

   //no error possible, init_fields is always called.
   retU = gen_data_layout(inst_data);

   retU = init_fields(dyn_fldar, inst_data, dyn_fcnt);

   return retU;
}

rcode vindsc_cl::gen_data_layout(const u8_t* inst) {
   rcode  retU;
   int    md_digital;

   //note: EDID block 0: static descriptors use block0 ptr as instance ptr.
   md_digital = reinterpret_cast <const vid_in_t*> (inst)->digital.input_type;

   if (1 == md_digital) {
      memcpy( dyn_fldar, in_digital, (in_digital_fcnt * EDI_FIELD_SZ) );
      dyn_fcnt = in_digital_fcnt;
   } else {
      memcpy( dyn_fldar, in_analog, (in_analog_fcnt * EDI_FIELD_SZ) );
      dyn_fcnt = in_analog_fcnt;
   }

   RCD_RETURN_OK(retU);
}

void vindsc_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   edi_dynfld_t *p_field;
   u32_t         ival;

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   //input type
   p_field = FieldsAr.Item(0);
   EDID.gp_name.Empty();
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
   if (0 == ival) {
      EDID.gp_name = "Analog";
   } else {
      EDID.gp_name = "Digital";
   }
   gp_name  = "Video input: ";
   gp_name << EDID.gp_name;
}

//BDD: basic display descriptor : handlers
//BDD: basic display descriptor
const char  bddcs_cl::CodN[] = "BDD";
const char  bddcs_cl::Name[] = "Basic Display Descriptor";
const char  bddcs_cl::Desc[] = "Basic Display Descriptor";

const edi_field_t bddcs_cl::fields[] = {
   {&EDID_cl::ByteVal, 0, offsetof(bdd_t, max_hsize), 0, 1, F_BTE|F_INT|F_CM|F_DN, 0, 255, "max_hsize",
   "Max horizontal image size, in cm, 0=undefined" },
   {&EDID_cl::ByteVal, 0, offsetof(bdd_t, max_vsize), 0, 1, F_BTE|F_INT|F_CM|F_DN, 0, 255, "max_vsize",
   "Max vertical image size, in cm, 0=undefined" },
   {&EDID_cl::Gamma, 0, offsetof(bdd_t, gamma), 0, 1, F_FLT|F_NI, 0, 255, "gamma",
   "Byte value = (gamma*100)-100 (range 1.00–3.54)" }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode bddcs_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   static const u32_t fcount = 3;
   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_BDD | T_GRP_FIXED;
   abs_offs    = offsetof(edid_t, bdd);
   rel_offs    = abs_offs;

   CopyInstData(inst, sizeof(bdd_t));

   retU = init_fields(&fields[0], inst_data, fcount, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

void bddcs_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   edi_dynfld_t *p_field;
   u32_t         ival;

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   //X size
   p_field = FieldsAr.Item(0);
   EDID.gp_name.Empty();
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
   gp_name  = "size: ";
   gp_name << EDID.gp_name;
   //Y size
   p_field = FieldsAr.Item(1);
   EDID.gp_name.Empty();
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
   gp_name << "x" << EDID.gp_name << "cm";

}

//SPF: Supported features : handlers
//SPF: Supported features
const char  spft_cl::CodN[] = "SPF";
const char  spft_cl::Name[] = "Supported features";
const char  spft_cl::Desc[] = "Supported features.";

const edi_field_t spft_cl::fields[] = {
   {&EDID_cl::BitVal, 0, 0, 0, 1, F_BIT, 0, 1, "gtf_cfreq",
   "EDID v1.3: GTF (General Timing Formula) supported with default parameter values\n"
   "If 'gtf_cfreq'=0, the MRL descriptor can include CVT support information "
   "EDID v1.4: 'Continuous Frequency Supported': if 1, then MRL descriptor presence is mandatory."
   },
   {&EDID_cl::BitVal, 0, 0, 1, 1, F_BIT, 0, 1, "dtd0_native",
   "If set to 1, DTD0 (offs=54) contains \"preferred timing mode\" - i.e. the native and "
   "thus recommended timings (pixel-correct).\nFor EDID v1.3+ DTD0 is always treated as \"preferred\", "
   "and this bit should be set to 1." },
   {&EDID_cl::BitVal, 0, 0, 2, 1, F_BIT, 0, 1, "std_srbg",
   "Standard sRGB colour space. Chromacity coords (bytes 25–34) must contain sRGB standard values." },
   {&EDID_cl::BitF8Val, 0, 0, 3, 2, F_BFD, 0, 3, "vsig_format",
   "Video signal format:\nDisplay type analog:\n"
   " 00 = Monochrome or Grayscale;\n 01 = RGB color;\n"
   " 10 = Non-RGB multi-color;\n 11 = Undefined\n"
   "Display type digital:\n 00 = RGB 4:4:4;\n 01 = RGB 4:4:4 + YCrCb 4:4:4;\n"
   " 10 = RGB 4:4:4 + YCrCb 4:2:2;\n 11 = RGB 4:4:4 + YCrCb 4:4:4 + YCrCb 4:2:2" },
   {&EDID_cl::BitVal, 0, 0, 5, 1, F_BIT, 0, 1, "dpms_off",
   "DPMS active-off supported" },
   {&EDID_cl::BitVal, 0, 0, 6, 1, F_BIT, 0, 1, "dpms_susp",
   "DPMS suspend supported" },
   {&EDID_cl::BitVal, 0, 0, 7, 1, F_BIT, 0, 1, "dpms_stby",
   "DPMS standby supported" }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode spft_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   const u32_t fcount = 7;

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_SPF | T_GRP_FIXED;
   abs_offs    = offsetof(edid_t, features);
   rel_offs    = abs_offs;

   CopyInstData(inst, sizeof(dsp_feat_t)); //1

   retU = init_fields(&fields[0], inst_data, fcount, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

//CXY: Chromacity coords : handlers
rcode EDID_cl::ChrXY_getWriteVal(u32_t op, wxc_String& sval, u32_t& ival) {
   rcode retU;
   RCD_SET_FAULT(retU);

   if (op == OP_WRSTR) {
      float  dval;
      retU = getStrFloat(sval, 0.0, 0.999, dval);
      if (! RCD_IS_OK(retU)) return retU;
      ival = (dval*1024.0);
   } else if (op == OP_WRINT) {
      ival = (ival & 0x3F);
      RCD_SET_OK(retU);
   }
   return retU;
}

rcode EDID_cl::CHredX(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* field) {
   rcode      retU;
   ulong      tmpv;
   chromxy_t *chrxy;

   chrxy = reinterpret_cast <chromxy_t*> (getInstancePtr(field));

   if (field == NULL) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      float  dval;
      tmpv  = (chrxy->red8h_x << 2);
      tmpv |= chrxy->rgxy_lsbits.red_x;
      ival  = tmpv;
      dval  = tmpv;
      if (sval.Printf("%.03f", (dval/1024.0)) < 0) {
         RCD_SET_FAULT(retU);
      } else {
         RCD_SET_OK(retU);
      }

   } else {
      retU = ChrXY_getWriteVal(op, sval, ival);
      if (!RCD_IS_OK(retU)) return retU;

      tmpv = ival;

      chrxy->rgxy_lsbits.red_x = (tmpv  & 0x03);
      chrxy->red8h_x           = (tmpv >> 2   );
   }
   return retU;
}

rcode EDID_cl::CHredY(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* field) {
   rcode      retU;
   ulong      tmpv;
   chromxy_t *chrxy;

   chrxy = reinterpret_cast <chromxy_t*> (getInstancePtr(field));

   if (field == NULL) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      float  dval;
      tmpv  = (chrxy->red8h_y << 2);
      tmpv |= chrxy->rgxy_lsbits.red_y;
      ival  = tmpv;
      dval  = tmpv;
      if (sval.Printf("%.03f", (dval/1024.0)) < 0) {
         RCD_SET_FAULT(retU);
      } else {
         RCD_SET_OK(retU);
      }

   } else {
      retU = ChrXY_getWriteVal(op, sval, ival);
      if (!RCD_IS_OK(retU)) return retU;

      tmpv = ival;

      chrxy->rgxy_lsbits.red_y = (tmpv  & 0x03);
      chrxy->red8h_y           = (tmpv >> 2   );
   }
   return retU;
}

rcode EDID_cl::CHgrnX(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* field) {
   rcode      retU;
   ulong      tmpv;
   chromxy_t *chrxy;

   chrxy = reinterpret_cast <chromxy_t*> (getInstancePtr(field));

   if (field == NULL) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      float  dval;
      tmpv  = (chrxy->green8h_x << 2);
      tmpv |= chrxy->rgxy_lsbits.green_x;
      ival  = tmpv;
      dval  = tmpv;
      if (sval.Printf("%.03f", (dval/1024.0)) < 0) {
         RCD_SET_FAULT(retU);
      } else {
         RCD_SET_OK(retU);
      }

   } else {
      retU = ChrXY_getWriteVal(op, sval, ival);
      if (!RCD_IS_OK(retU)) return retU;

      tmpv = ival;

      chrxy->rgxy_lsbits.green_x = (tmpv  & 0x03);
      chrxy->green8h_x           = (tmpv >> 2   );
   }
   return retU;
}

rcode EDID_cl::CHgrnY(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* field) {
   rcode      retU;
   ulong      tmpv;
   chromxy_t *chrxy;

   chrxy = reinterpret_cast <chromxy_t*> (getInstancePtr(field));

   if (field == NULL) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      float  dval;
      tmpv  = (chrxy->green8h_y << 2);
      tmpv |= chrxy->rgxy_lsbits.green_y;
      ival  = tmpv;
      dval  = tmpv;
      if (sval.Printf("%.03f", (dval/1024.0)) < 0) {
         RCD_SET_FAULT(retU);
      } else {
         RCD_SET_OK(retU);
      }

   } else {
      retU = ChrXY_getWriteVal(op, sval, ival);
      if (!RCD_IS_OK(retU)) return retU;

      tmpv = ival;

      chrxy->rgxy_lsbits.green_y = (tmpv  & 0x03);
      chrxy->green8h_y           = (tmpv >> 2   );
   }
   return retU;
}

rcode EDID_cl::CHbluX(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* field) {
   rcode      retU;
   ulong      tmpv;
   chromxy_t *chrxy;

   chrxy = reinterpret_cast <chromxy_t*> (getInstancePtr(field));

   if (field == NULL) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      float  dval;
      tmpv  = (chrxy->blue8h_x << 2);
      tmpv |= chrxy->bwxy_lsbits.blue_x;
      ival  = tmpv;
      dval  = tmpv;
      if (sval.Printf("%.03f", (dval/1024.0)) < 0) {
         RCD_SET_FAULT(retU);
      } else {
         RCD_SET_OK(retU);
      }

   } else {
      retU = ChrXY_getWriteVal(op, sval, ival);
      if (!RCD_IS_OK(retU)) return retU;

      tmpv = ival;

      chrxy->bwxy_lsbits.blue_x = (tmpv  & 0x03);
      chrxy->blue8h_x           = (tmpv >> 2   );
   }
   return retU;
}

rcode EDID_cl::CHbluY(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* field) {
   rcode      retU;
   ulong      tmpv;
   chromxy_t *chrxy;

   chrxy = reinterpret_cast <chromxy_t*> (getInstancePtr(field));

   if (field == NULL) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      float  dval;
      tmpv  = (chrxy->blue8h_y << 2);
      tmpv |= chrxy->bwxy_lsbits.blue_y;
      ival  = tmpv;
      dval  = tmpv;
      if (sval.Printf("%.03f", (dval/1024.0)) < 0) {
         RCD_SET_FAULT(retU);
      } else {
         RCD_SET_OK(retU);
      }

   } else {
      retU = ChrXY_getWriteVal(op, sval, ival);
      if (!RCD_IS_OK(retU)) return retU;

      tmpv = ival;

      chrxy->bwxy_lsbits.blue_y = (tmpv  & 0x03);
      chrxy->blue8h_y           = (tmpv >> 2   );
   }
   return retU;
}

rcode EDID_cl::CHwhtX(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* field) {
   rcode      retU;
   ulong      tmpv;
   chromxy_t *chrxy;

   chrxy = reinterpret_cast <chromxy_t*> (getInstancePtr(field));

   if (field == NULL) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      float  dval;
      tmpv  = (chrxy->white8h_x << 2);
      tmpv |= chrxy->bwxy_lsbits.white_x;
      ival  = tmpv;
      dval  = tmpv;
      if (sval.Printf("%.03f", (dval/1024.0)) < 0) {
         RCD_SET_FAULT(retU);
      } else {
         RCD_SET_OK(retU);
      }

   } else {
      retU = ChrXY_getWriteVal(op, sval, ival);
      if (!RCD_IS_OK(retU)) return retU;

      tmpv = ival;

      chrxy->bwxy_lsbits.white_x = (tmpv  & 0x03);
      chrxy->white8h_x           = (tmpv >> 2   );
   }
   return retU;
}

rcode EDID_cl::CHwhtY(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* field) {
   rcode      retU;
   ulong      tmpv;
   chromxy_t *chrxy;

   chrxy = reinterpret_cast <chromxy_t*> (getInstancePtr(field));

   if (field == NULL) RCD_RETURN_FAULT(retU);

   if (op == OP_READ) {
      float  dval;
      tmpv  = (chrxy->white8h_y << 2);
      tmpv |= chrxy->bwxy_lsbits.white_y;
      ival  = tmpv;
      dval  = tmpv;
      if (sval.Printf("%.03f", (dval/1024.0)) < 0) {
         RCD_SET_FAULT(retU);
      } else {
         RCD_SET_OK(retU);
      }

   } else {
      retU = ChrXY_getWriteVal(op, sval, ival);
      if (!RCD_IS_OK(retU)) return retU;

      tmpv = ival;

      chrxy->bwxy_lsbits.white_y = (tmpv  & 0x03);
      chrxy->white8h_y           = (tmpv >> 2   );
   }
   return retU;
}

//CXY: Chromacity coords
const char  chromxy_cl::CodN[] = "CXY";
const char  chromxy_cl::Name[] = "Chromacity Coords";
const char  chromxy_cl::Desc[] = "CIE Chromacity Coordinates.\n"
"The values are 10bit bitfields, value range of 0 - 1023 is converted to a fixed point fractional "
"in range 0.0 - 0.999.\nFor monochrome displays only White Point coordinates should be defined, "
"and the coordinaes of Red, Green and Blue should be set to 0.0\n\n"
"https://en.wikipedia.org/wiki/CIE_1931_color_space";

const edi_field_t chromxy_cl::fields[] = {
   {&EDID_cl::CHredX, 0, 0, 0, 10, F_BFD|F_FLT|F_GD|F_NI, 0, 1, "red_x", "" },
   {&EDID_cl::CHredY, 0, 0, 0, 10, F_BFD|F_FLT|F_GD|F_NI, 0, 1, "red_y", "" },
   {&EDID_cl::CHgrnX, 0, 0, 0, 10, F_BFD|F_FLT|F_GD|F_NI, 0, 1, "green_x", "" },
   {&EDID_cl::CHgrnY, 0, 0, 0, 10, F_BFD|F_FLT|F_GD|F_NI, 0, 1, "green_y", "" },
   {&EDID_cl::CHbluX, 0, 0, 0, 10, F_BFD|F_FLT|F_GD|F_NI, 0, 1, "blue_x", "" },
   {&EDID_cl::CHbluY, 0, 0, 0, 10, F_BFD|F_FLT|F_GD|F_NI, 0, 1, "blue_y", "" },
   {&EDID_cl::CHwhtX, 0, 0, 0, 10, F_BFD|F_FLT|F_GD|F_NI, 0, 1, "white_x", "" },
   {&EDID_cl::CHwhtY, 0, 0, 0, 10, F_BFD|F_FLT|F_GD|F_NI, 0, 1, "white_y", "" }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode chromxy_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   static const u32_t fcount = 8;

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_CXY | T_GRP_FIXED;
   abs_offs    = offsetof(edid_t, chromxy);
   rel_offs    = abs_offs;

   CopyInstData(inst, sizeof(chromxy_t));

   retU = init_fields(&fields[0], inst_data, fcount, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

//ETM: Established Timings : no dedicated handlers
//ETM: Established Timings
const char  resmap_cl::CodN[] = "ETM";
const char  resmap_cl::Name[] = "Established Timings Map";
const char  resmap_cl::Desc[] =
"Standard display modes (VESA): each field is a flag telling whether particular display "
"mode is supported.\nField name describes the resolution and verical refresh rate:\n"
"Hres x Vres x Vrefresh, i = interlaced.\nreservedX are modes reserved for manufacturers.";

const edi_field_t resmap_cl::fields[] = {
//byte 35
{&EDID_cl::BitVal, 0, 0, 0, 1, F_BIT|F_GD, 0, 1, "800x600x60", "" },
{&EDID_cl::BitVal, 0, 0, 1, 1, F_BIT|F_GD, 0, 1, "800x600x56", "" },
{&EDID_cl::BitVal, 0, 0, 2, 1, F_BIT|F_GD, 0, 1, "640x480x75", "" },
{&EDID_cl::BitVal, 0, 0, 3, 1, F_BIT|F_GD, 0, 1, "640x480x72", "" },
{&EDID_cl::BitVal, 0, 0, 4, 1, F_BIT|F_GD, 0, 1, "640x480x67", "" },
{&EDID_cl::BitVal, 0, 0, 5, 1, F_BIT|F_GD, 0, 1, "640x480x60", "" },
{&EDID_cl::BitVal, 0, 0, 6, 1, F_BIT|F_GD, 0, 1, "720x400x88", "" },
{&EDID_cl::BitVal, 0, 0, 7, 1, F_BIT|F_GD, 0, 1, "720x400x70", "" },
//byte 36:
{&EDID_cl::BitVal, 0, 1, 0, 1, F_BIT|F_GD, 0, 1, "1280x1024x75", "" },
{&EDID_cl::BitVal, 0, 1, 1, 1, F_BIT|F_GD, 0, 1, "1024x768x75" , "" },
{&EDID_cl::BitVal, 0, 1, 2, 1, F_BIT|F_GD, 0, 1, "1024x768x72" , "" },
{&EDID_cl::BitVal, 0, 1, 3, 1, F_BIT|F_GD, 0, 1, "1024x768x60" , "" },
{&EDID_cl::BitVal, 0, 1, 4, 1, F_BIT|F_GD, 0, 1, "1024x768x87i", "" },
{&EDID_cl::BitVal, 0, 1, 5, 1, F_BIT|F_GD, 0, 1, "832x624x75"  , "" },
{&EDID_cl::BitVal, 0, 1, 6, 1, F_BIT|F_GD, 0, 1, "800x600x75"  , "" },
{&EDID_cl::BitVal, 0, 1, 7, 1, F_BIT|F_GD, 0, 1, "800x600x72"  , "" },
//byte 37:
{&EDID_cl::BitVal, 0, 2, 0, 1, F_BIT|F_GD, 0, 1, "reserved0", "" },
{&EDID_cl::BitVal, 0, 2, 1, 1, F_BIT|F_GD, 0, 1, "reserved1", "" },
{&EDID_cl::BitVal, 0, 2, 2, 1, F_BIT|F_GD, 0, 1, "reserved2", "" },
{&EDID_cl::BitVal, 0, 2, 3, 1, F_BIT|F_GD, 0, 1, "reserved3", "" },
{&EDID_cl::BitVal, 0, 2, 4, 1, F_BIT|F_GD, 0, 1, "reserved4", "" },
{&EDID_cl::BitVal, 0, 2, 5, 1, F_BIT|F_GD, 0, 1, "reserved5", "" },
{&EDID_cl::BitVal, 0, 2, 6, 1, F_BIT|F_GD, 0, 1, "reserved6", "" },
{&EDID_cl::BitVal, 0, 2, 7, 1, F_BIT|F_GD, 0, 1, "1152x870x75", "" }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode resmap_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   static const u32_t fcount = 24;

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_ETM | T_GRP_FIXED;
   abs_offs    = offsetof(edid_t, res_map);
   rel_offs    = abs_offs;

   CopyInstData(inst, sizeof(est_map_t));

   retU = init_fields(&fields[0], inst_data, fcount, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

//STI: std timing descriptors : handlers
rcode EDID_cl::STI_Xres(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode       retU;
   dmt_std2_t* inst;

   inst = reinterpret_cast <dmt_std2_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival   = inst->HApix;
      if (ival == 0x01) p_field->field.flags |= F_NU; //unused field
      ival  += 31;
      ival <<= 3;
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong tmpv = 0;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);

      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      tmpv = (tmpv >> 3)-31;
      if (tmpv != 0x01) p_field->field.flags &= ~F_NU;
      inst->HApix = tmpv;
   }
   return retU;
}

rcode EDID_cl::STI_Vref(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode       retU;
   dmt_std2_t *inst = NULL;


   inst = reinterpret_cast <dmt_std2_t*> (getInstancePtr(p_field));
   ival = (reinterpret_cast <u8_t*> (inst))[1];

   if (ival == 0x01) {
      p_field->field.flags |= F_NU;
   } else {
      p_field->field.flags &= ~F_NU;
   }

   if (op == OP_READ) {
      ival  = inst->v_ref;
      ival += 60;
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong tmpv = 0;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, 60, 123, tmpv);
         if (! RCD_IS_OK(retU)) return retU;
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      inst->v_ref = (tmpv - 60);
   }
   return retU;
}

//STI: Std Timing Descriptor
//STI: Aspect ratio selector
sm_vmap STI_asp_ratio = {
   {0, {0, "16:10", NULL}},
   {1, {0, "4:3"  , NULL}},
   {2, {0, "5:4"  , NULL}},
   {3, {0, "16:9" , NULL}},
};

const char  dmt_std2_cl::CodN[] = "STI";
const char  dmt_std2_cl::Name[] = "Standard Timing Information";
const char  dmt_std2_cl::Desc[] =
"Standard Timing Information: VESA-DMT STD 2-byte Video Timing code\n"
"If the group is unused, both bytes are set to 0x01, NU flag is set.\n"
"In such case the interpreted values are:\n X-res = 256;\n V-freq = 61;\n"
" asp_ratio = 0b00\n DMT_2 = 0x0101";

const char stiXres8dsc[] = "Byte value: ((X-res / 8) - 31) : 256–2288 pixels, value 00 is "
"reserved.\n"
"Vertical resolution is calculated from X-res value, using image aspect ratio";
const char stiVsyncDsc[] = "V-freq: (60–123 Hz).\n"
"Bitfield (6bits) value is stored as Vfreq-60 (0-63)";
const char stiAspRatioDsc[] = "Image aspect ratio:\n "
"0b00 (0) =16:10;\n 0b01 (1) =4:3;\n 0b10 (2) =5:4;\n 0b11 (3) =16:9.\n"
"EDID versions prior to 1.3 defined 0b00 as ratio 1:1.";
const char stiDMT2B_Dsc[] = "VESA-DMT STD 2-byte Video Timing Code\n\n"
"This field overlaps the fields 'X-res', 'V-freq' and 'asp_ratio' - it allows one to select "
"standardised video code directly from value selector menu\n";

const edi_field_t dmt_std2_cl::fields[] = {
   {&EDID_cl::STI_Xres, 0, 0, 0, 1, F_INT|F_PIX|F_DN|F_FR, 256, 2288,
   "X-res", stiXres8dsc },
   {&EDID_cl::STI_Vref, 0, 1, 0, 6, F_INT|F_BFD|F_HZ|F_DN|F_FR, 0, 63,
   "V-freq", stiVsyncDsc },
   {&EDID_cl::BitF8Val, VS_STD2_ASP_RATIO, 1, 6, 2, F_BFD|F_INT|F_DN|F_VS|F_FR, 0, 3,
   "asp_ratio", stiAspRatioDsc },
   {&EDID_cl::Word16, VS_STD2_VIDFMT, 0, 0, 2, F_LE|F_HEX|F_DN|F_VS|F_VSVM|F_FR, 0, 0xFFFF,
   "DMT_2", stiDMT2B_Dsc}
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode dmt_std2_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 4
   };

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_DMT2 | T_SUB_GRP | orflags;

   CopyInstData(inst, sizeof(dmt_std2_t));

   retU = init_fields(&fields[0], inst_data, fcount, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

void dmt_std2_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   EDID.STI_DBN(EDID, gp_name, FieldsAr);
}

//CVT3: VESA-CVT 3-byte Timing Code

//CVT3: VESA-CVT 3-byte Timing Code : handlers
rcode EDID_cl::CVT3_Yres (u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode       retU;
   dmt_cvt3_t* inst = NULL;

   inst = reinterpret_cast <dmt_cvt3_t*> (getInstancePtr(p_field));

   if (op == OP_READ) { //Yres field encoded as [(VAlin/2)–1]
      ival   = inst->VAlin_4msb;
      ival <<= 8;
      ival  |= inst->VAlin_8lsb;
      ival  += 1;
      ival <<= 1;

      if (ival == 0) p_field->field.flags |= F_NU; //unused field
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong tmpv = 0;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
         ival = tmpv;
      } else if (op == OP_WRINT) {
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      if (ival != 0) p_field->field.flags &= ~F_NU;

      ival >>= 1;
      ival  -= 1;

      inst->VAlin_8lsb = ival;
      ival           >>= 8;
      inst->VAlin_4msb = ival;
   }
   return retU;
}

//CVT3: Aspect ratio selector
sm_vmap CVT3_asp_ratio = {
   {0, {0, "4:3"  , NULL}},
   {1, {0, "16:9" , NULL}},
   {2, {0, "16:10", NULL}},
   {3, {0, "15:9" , NULL}},
};
//CVT3: Preferred V-refresh selector
sm_vmap CVT3_pref_vref = {
   {0, {0, "50Hz"  , NULL}},
   {1, {0, "60Hz"  , NULL}},
   {2, {0, "75Hz"  , NULL}},
   {3, {0, "85Hz"  , NULL}},
};

const char  dmt_cvt3_cl::CodN[] = "CVT3";
const char  dmt_cvt3_cl::Name[] = "VESA-CVT 3-byte Timing Code";
const char  dmt_cvt3_cl::Desc[] = "VESA-CVT 3-byte Timing Code";

const char CVT3_DMT3B_Dsc[] = "VESA-CVT 3-byte Timing Code\n\n"
"This field overlaps all other the fields in this group - it allows one to select "
"standardised CVT-3 video code directly from value selector menu\n";

const char CVT3_Yres_Dsc[] =
"Y-Res: Vertical addresable lines,\n"
"range 2..8192 encoded in 12bits as ((VAlin/2)–1)\n\n"
"min = ((0+1)*2) = 2\n"
"max = ((4095+1)*2) = 8192\n";

//NOTE: fields res0_01 & res2_7 have F_DN flag set:
//      the dmt_cvt3_cl::getGrpName() indicates invalid CVT3 codes
//      if reserved fields are not zeroed.
const edi_field_t dmt_cvt3_cl::fields[] = {
   {&EDID_cl::CVT3_Yres, 0, 0, 0, 1, F_INT|F_FR|F_PIX|F_DN, 2, 8192,
   "Y-res", CVT3_Yres_Dsc },
   {&EDID_cl::BitF8Val, 0, 1, 0, 2, F_BFD|F_DN|F_RD, 0, 3,
   "res0_01", "reserved (0)" },
   {&EDID_cl::BitF8Val, VS_CVT3_ASP_RATIO, 1, 2, 2, F_BFD|F_INT|F_FR|F_DN|F_VS, 0, 3,
   "asp_ratio",
   " 0b00 (0): 4:3\n 0b01 (1): 16:9\n 0b10 (2): 16:10\n 0b11 (3): 15:9\n" },
   {&EDID_cl::BitVal, 0, 2, 0, 1, F_BIT, 0, 1, "vref_60_rb",
   "V-refresh 60Hz Reduced Blanking, 1: supported" },
   {&EDID_cl::BitVal, 0, 2, 1, 1, F_BIT, 0, 1, "vref_85",
   "V-refresh 85Hz CRT mode, 1: supported" },
   {&EDID_cl::BitVal, 0, 2, 2, 1, F_BIT, 0, 1, "vref_75",
   "V-refresh 75Hz CRT mode, 1: supported" },
   {&EDID_cl::BitVal, 0, 2, 3, 1, F_BIT, 0, 1, "vref_60",
   "V-refresh 60Hz CRT mode, 1: supported" },
   {&EDID_cl::BitVal, 0, 2, 4, 1, F_BIT, 0, 1, "vref_50",
   "V-refresh 50Hz CRT mode, 1: supported" },
   {&EDID_cl::BitF8Val, VS_CVT3_PREF_VREF, 2, 5, 2, F_BFD|F_INT|F_VS, 0, 3, "pref_vref",
   "Preferred Vertical Refresh rate:\n"
   " 0b00 (0): 50Hz\n 0b01 (1): 60Hz\n 0b10 (2): 75Hz\n 0b11 (3): 85Hz\n" },
   {&EDID_cl::BitVal, 0, 2, 7, 1, F_BIT|F_DN|F_RD, 0, 1, "res2_7",
   "reserved (0)" },
   {&EDID_cl::Word24, VS_CVT3_VIDFMT, 0, 0, 3, F_LE|F_HEX|F_FR|F_DN|F_VS|F_VSVM, 0, 0xFFFFFF,
   "CVT_3", CVT3_DMT3B_Dsc}
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode dmt_cvt3_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 11
   };

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_CVT3 | T_SUB_GRP | orflags;

   CopyInstData(inst, sizeof(dmt_cvt3_t));

   retU = init_fields(&fields[0], inst_data, fcount, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

void dmt_cvt3_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   enum {
      CVT3_IDX_CVT3  = 10, //fld index
      CVT3_VREF_60HZ =  1,
   };

   typedef struct {
      u32_t numrtr;
      u32_t denomr;
   } ratio_t;

   typedef union {
      u32_t       cvt3;
      dmt_cvt3_t  cvtT;
   } cvtT_u;

   const ratio_t asp_ratio[4] = {
      { 4,  3},
      {16,  9},
      {16, 10},
      {15,  9}
   };

   const int vref_hz[4] = {50, 60, 75, 85};

   rcode         retU;
   edi_dynfld_t *p_field;
   u32_t         xres;
   u32_t         yres;
   u32_t         aspr;
   u32_t         vref;
   cvtT_u        s_cvt;
   bool          b_invalid;
   bool          b_std;

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   //CVT3 code
   p_field = FieldsAr.Item(CVT3_IDX_CVT3);
   gp_name.Empty();
   ( EDID.*p_field->field.handlerfn )(OP_READ, gp_name, s_cvt.cvt3, p_field );

   if (0 == s_cvt.cvt3) {
      gp_name = "not used";
      return;
   }

   //check if the cvt3 code is stanardised:
   retU  = vmap_GetValueAsString(VS_CVT3_VIDFMT, VMAP_VAL, s_cvt.cvt3, F_VSVM, gp_name);
   b_std = RCD_IS_TRUE(retU);

   //CVT3 code bytes are stored in reversed order:
   s_cvt.cvt3   = __bswap_32(s_cvt.cvt3);
   s_cvt.cvt3 >>= 8;
   //Y-res field encoded as [(VAlin/2)–1]
   yres   = s_cvt.cvtT.VAlin_4msb;
   yres <<= 8;
   yres  |= s_cvt.cvtT.VAlin_8lsb;
   yres  += 1;
   yres <<= 1;
   //aspect ratio
   aspr = s_cvt.cvtT.asp_ratio;
   //V-Refresh
   vref = s_cvt.cvtT.pref_vref;
   //Y-res
   xres  = yres;
   xres *= asp_ratio[aspr].numrtr;
   xres /= asp_ratio[aspr].denomr;

   gp_name.Empty();
   gp_name << xres << "x" << yres << " @ " << vref_hz[vref] << "Hz";

   b_invalid  = (0 == yres);
   b_invalid |= (0 != s_cvt.cvtT.res0_01);
   b_invalid |= (0 != s_cvt.cvtT.res2_7 );

   if (b_invalid) {
      gp_name << " <invalid>";
      return;
   }

   if ((CVT3_VREF_60HZ == s_cvt.cvtT.pref_vref) && s_cvt.cvtT.vref60rb ) {
      gp_name << " (RB)"; //Reduced Blanking
   }

   if (! b_std) {
      gp_name << " (!std)";
   }
}


//----- MISC/helpers

//copy local data back to EDID buffer:
//hdr_sz != 0 : sub-groups keep their own instance data copy
//hdr_sz == 0 : no sub-groups, data in root group
void edi_grp_cl::SpawnInstance(u8_t *pinst) {
   u32_t bsz;
   bsz = getDataSize();
   memcpy(pinst, inst_data, bsz);
}

//copy sub-group data to parent' group local data buffer
rcode edi_grp_cl::AssembleGroup() {
   rcode       retU;
   u32_t       dlen;
   u32_t       dofs;
   u32_t       dsz;
   u32_t       gpidx;
   u8_t       *pbuf;
   edi_grp_cl *psubg;
   GroupAr_cl *sub_ar;

   sub_ar = getSubGrpAr();

   if (sub_ar == NULL) {
      RCD_RETURN_OK(retU);
   }

   dofs = (hdr_sz + ahf_sz);
   dlen = (sizeof(inst_data) - dofs);
   pbuf = &inst_data[dofs];

   for (gpidx=0; gpidx<sub_ar->GetCount(); ++gpidx) {
      psubg = sub_ar->Item(gpidx);
      dsz   = psubg ->getTotalSize();

      if (dlen < dsz) RCD_RETURN_FAULT(retU);

      memcpy(pbuf, psubg->getInstPtr(), dsz);
      pbuf += dsz;
      dlen -= dsz;
   }

   RCD_RETURN_OK(retU);
}

//called from derived class::Clone()
edi_grp_cl* edi_grp_cl::base_clone(rcode& rcd, edi_grp_cl* pgrp, u32_t orflags) {
   rcode       retU;
   GroupAr_cl *sub_ar;

   //Root grps: assemble sub-grps before copying: otherwise changes made in
   //subgroups won't be visible in copy.
   retU = AssembleGroup();
   if (!RCD_IS_OK(retU)) {
      rcd = retU;
      if (retU.detail.rcode > RCD_FVMSG) {
         delete pgrp;
         return NULL;
      }
   }
   rcd = retU;

   //NOTE: with orflags & T_MODE_EDIT init() is copying entire local buffer
   retU = pgrp->init(inst_data, orflags, NULL);
   if (!RCD_IS_OK(retU)) {
      rcd = retU;
      //the fault message is logged, but ignored
      if (retU.detail.rcode > RCD_FVMSG) {
         delete pgrp;
         return NULL;
      }
   }
   rcd = retU; //messages passed with RCD_TRUE

   pgrp->abs_offs = abs_offs;
   pgrp->rel_offs = rel_offs;

   //special case: cea_unkdat_cl has variable size
   if (ID_CEA_UDAT == (type_id.t32 & ID_SUBGRP_MASK)) {
      pgrp->dat_sz  = dat_sz;
   }

   pgrp->grp_ar     = grp_ar;
   pgrp->grp_idx    = grp_idx;
   pgrp->parent_grp = parent_grp;

   pgrp->type_id.t_md_edit = 0;

   //restore parent array pointer
   sub_ar = getSubGrpAr();

   if (sub_ar != NULL) {
      sub_ar = pgrp->getSubGrpAr();
      sub_ar->setParentArray(grp_ar);
   }

   return pgrp;
}

rcode edi_grp_cl::init_fields(const edi_field_t* field_ar, const u8_t* inst, u32_t fcount,
                              bool b_append, const char *pname, const char *pdesc, const char *pcodn,
                              i32_t offs)
{
   rcode   retU;
   RCD_SET_OK(retU);

   if (fcount == 0) RCD_RETURN_FAULT_MSG(retU, "field_count == 0");

   if (pname != NULL) GroupName = wxc_String::FromAscii(pname);
   if (pdesc != NULL) GroupDesc = wxc_String::FromAscii(pdesc);
   if (pcodn != NULL) CodeName  = wxc_String::FromAscii(pcodn);

   //append fields (default: false)
   if (! b_append) FieldsAr.Empty();

   for (u32_t itf=0; itf<fcount; itf++) {
      edi_dynfld_t *pfld;
      pfld = new edi_dynfld_t;
      if (pfld == NULL) RCD_RETURN_FAULT(retU);

      memcpy( &pfld->field, &field_ar[itf], EDI_FIELD_SZ);

      pfld->field.offs   = ((i32_t) pfld->field.offs + offs); //optional offset correction
      pfld->base         = const_cast<u8_t*> (inst);

      if ((pfld->field.flags & F_VS) != 0) {
         //A field with F_VS flag set must have defined valid VS index.
         if (VS_NO_SELECTOR == pfld->field.vmap_idx) RCD_RETURN_FAULT(retU);
      }

      FieldsAr.Add(pfld);
   }
   return retU;
}

void edi_grp_cl::clear_fields() {

   for (u32_t itf=0; itf<FieldsAr.GetCount(); itf++) {
      edi_dynfld_t* pfield = FieldsAr.Item(itf);

      delete pfield;
   }
   FieldsAr.Clear();
}

void dbc_grp_cl::delete_subg() {
   subgroups.Clear();
}

//Insert subgroup of unknown bytes
rcode dbc_grp_cl::Append_UNK_DAT(const u8_t* inst, u32_t dlen, u32_t orflags, u32_t abs_offs, u32_t rel_offs, edi_grp_cl* parent_grp) {
   rcode        retU;
   edi_grp_cl  *pgrp;

   pgrp = new cea_unkdat_cl;
   if (NULL == pgrp) RCD_RETURN_FAULT(retU);

   //special case: cea_unkdat_cl has variable size
   pgrp->setDataSize(dlen);

   retU = pgrp->init(inst, orflags, parent_grp);
   if (! RCD_IS_OK(retU)) return retU;

   pgrp->setAbsOffs(abs_offs);
   pgrp->setRelOffs(rel_offs);

   subgroups.Append(pgrp);

   RCD_RETURN_OK(retU);
}

//Total group size, including subgroups
u32_t dbc_grp_cl::getTotalSize() {
   u32_t  dsize;

   dsize  = reinterpret_cast <bhdr_t*> (inst_data)->tag.blk_len;
   dsize += sizeof(bhdr_t);

   return dsize;
}
