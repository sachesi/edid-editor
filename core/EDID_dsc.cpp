/***************************************************************
 * Name:      EDID_dsc.cpp
 * Purpose:   EDID DTD and alternative descriptors
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2014-2025
 * License:   GPLv3+
 **************************************************************/

#include "debug.h"
#include "rcdunits.h"
#ifndef idEDID_DESC
   #error "EDID_desc.cpp: missing unit ID"
#endif
#define RCD_UNIT idEDID_DESC
#include "rcode/rcode.h"

#include "wxedid_rcd_scope.h"

RCD_AUTOGEN_DEFINE_UNIT

#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#include "vmap.h"
#include "EDID_class.h"

//bad block length msg (defined in CEA.cpp)
extern const char ERR_GRP_LEN_MSG[];

//EDID_base.cpp: shared STI/DMT descriptions
extern const char stiXres8dsc   [];
extern const char stiVsyncDsc   [];
extern const char stiAspRatioDsc[];
extern const char stiDMT2B_Dsc  [];

//unknown/invalid byte field (defined in CEA.cpp)
extern const edi_field_t unknown_byte_fld;
extern void insert_unk_byte(edi_field_t *p_fld, u32_t len, u32_t s_offs);


//DTD : detailed timing descriptor : handlers
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
rcode EDID_cl::DTD_PixClk(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   float   fval;
   u16_t   w16;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      w16 = rdWord16_LE((u8_t*) &inst->pix_clk);
      _BE_SWAP16(w16);

      ival  = w16;
      fval  = ival;
      fval /= 100.0;
      sval.Printf("%.02f", fval);
      RCD_SET_OK(retU);
   } else {
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrFloat(sval, 0.0, 655.35, fval);
         ival = lround(fval * 100.0);
      } else if (op == OP_WRINT) {
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      w16 = ival;
      _BE_SWAP16(w16);

      wrWord16_LE((u8_t*) &inst->pix_clk, w16);

      //pix_clk==0: change desc. type to Alternative Descriptor
      if (0 == ival) RCD_SET_TRUE(retU);
   }
   return retU;
}
#pragma GCC diagnostic warning "-Waddress-of-packed-member"

rcode EDID_cl::DTD_HApix(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->HApix_8lsb;
      ival |= (inst->HApix_4msb << 8);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->HApix_8lsb = (tmpv & 0xFF);
      inst->HApix_4msb = ((tmpv >> 8) & 0x0F);
   }

   return retU;
}

rcode EDID_cl::DTD_HBpix(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->HBpix_8lsb;
      ival |= (inst->HBpix_4msb << 8);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->HBpix_8lsb = (tmpv & 0xFF);
      inst->HBpix_4msb = ((tmpv >> 8) & 0x0F);
   }
   return retU;
}

rcode EDID_cl::DTD_VAlin(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->VAlin_8lsb;
      ival |= (inst->VAlin_4msb << 8);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->VAlin_8lsb = (tmpv & 0xFF);
      inst->VAlin_4msb = ((tmpv >> 8) & 0x0F);
   }
   return retU;
}

rcode EDID_cl::DTD_VBlin(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->VBlin_8lsb;
      ival |= (inst->VBlin_4msb << 8);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->VBlin_8lsb = (tmpv & 0xFF);
      inst->VBlin_4msb = ((tmpv >> 8) & 0x0F);
   }
   RCD_SET_OK(retU);
   return retU;
}

rcode EDID_cl::DTD_HOsync(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->HOsync_8lsb;
      ival |= (inst->HOsync_2msb << 8);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->HOsync_8lsb = (tmpv & 0xFF);
      inst->HOsync_2msb = ((tmpv >> 8) & 0x03);
   }
   return retU;
}

rcode EDID_cl::DTD_HsyncW(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->HsyncW_8lsb;
      ival |= (inst->HsyncW_2msb << 8);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->HsyncW_8lsb = (tmpv & 0xFF);
      inst->HsyncW_2msb = ((tmpv >> 8) & 0x03);
   }
   return retU;
}

rcode EDID_cl::DTD_VOsync(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->VOsync_4lsb;
      ival |= (inst->VOsync_2msb << 4);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->VOsync_4lsb = (tmpv & 0x0F);
      inst->VOsync_2msb = ((tmpv >> 4) & 0x03);
   }
   return retU;
}

rcode EDID_cl::DTD_VsyncW(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->VsyncW_4lsb;
      ival |= (inst->VsyncW_2msb << 4);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->VsyncW_4lsb = (tmpv & 0x0F);
      inst->VsyncW_2msb = ((tmpv >> 4) & 0x03);
   }
   return retU;
}

rcode EDID_cl::DTD_Hsize(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->Hsize_8lsb;
      ival |= (inst->Hsize_4msb << 8);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->Hsize_8lsb = (tmpv & 0xFF);
      inst->Hsize_4msb = ((tmpv >> 8) & 0x0F);
   }
   return retU;
}

rcode EDID_cl::DTD_Vsize(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   dtd_t  *inst;

   inst = reinterpret_cast <dtd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->Vsize_8lsb;
      ival |= (inst->Vsize_4msb << 8);
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->Vsize_8lsb = (tmpv & 0xFF);
      inst->Vsize_4msb = ((tmpv >> 8) & 0x0F);
   }
   return retU;
}

//DTD : detailed timing descriptor
const char  dtd_cl::CodN[] = "DTD";
const char  dtd_cl::Name[] = "Detailed Timing Descriptor";
const char  dtd_cl::Desc[] =
"Detailed Timing Descriptor\n\n"
"To change DTD into alternative descriptor type, set the Pixel Clock to 0.\n\n";

const edi_field_t dtd_cl::fields[] = {
   {&EDID_cl::DTD_PixClk, 0, offsetof(dtd_t, pix_clk), 0, 2, F_FLT|F_FR|F_MHZ|F_DN, 0, 65535,
   "Pixel clock",
   "Pixel clock in 10 kHz units (0.01–655.35 MHz, LE).\n"
   "val=0 is reserved -> indicates alternate descriptor format MSN, MRL, WPD, ...\n\n"
   "To change DTD into alternative descriptor type, set the Pixel Clock to 0.\n\n"
   "NOTE:\nMost of gfx/video cards require pixel clock value divisible by 0.25MHz\n" },
   {&EDID_cl::DTD_HApix, 0, offsetof(dtd_t, HApix_8lsb), 0, 12, F_INT|F_BFD|F_PIX|F_DN, 0, 4095,
   "H-Active pix",
   "Horizontal active pixels (0–4095), X-resolution." },
   {&EDID_cl::DTD_HBpix, 0, offsetof(dtd_t, HBpix_8lsb), 0, 12, F_INT|F_BFD|F_PIX|F_DN, 0, 4095,
   "H-Blank pix",
   "H-blank pixels (0–4095).\n"
   "This field defines a H-Blank time as number of pixel clock pulses. H-Blank time is a time "
   "break between drawing 2 consecutive lines on the screen. During H-blank time H-sync pulse is sent "
   "to the monitor to ensure correct horizontal alignment of lines." },
   {&EDID_cl::DTD_VAlin, 0, offsetof(dtd_t, VAlin_8lsb), 0, 12, F_INT|F_BFD|F_PIX|F_DN, 0, 4095,
   "V-Active lines",
   "Vertical active lines (0–4095), V-resolution." },
   {&EDID_cl::DTD_VBlin, 0, offsetof(dtd_t, VBlin_8lsb), 0, 12, F_INT|F_BFD|F_PIX|F_DN, 0, 4095,
   "V-Blank lines",
   "V-blank lines (0–4095).\n"
   "This field defines a V-Blank time as number of H-lines. During V-Blank time V-sync pulse is sent "
   "to the monitor to ensure correct vertical alignment of the picture." },
   {&EDID_cl::DTD_HOsync, 0, offsetof(dtd_t, HOsync_8lsb), 0, 10, F_INT|F_BFD|F_PIX, 0, 1023,
   "H-Sync offs",
   "H-sync offset in pixel clock pulses (0–1023) from blanking start. This offset value is "
   "responsible for horizontal picture alignment. Higher values are shifting the picture to the "
   "left edge of the screen." },
   {&EDID_cl::DTD_HsyncW, 0, offsetof(dtd_t, HsyncW_8lsb), 0, 10, F_INT|F_BFD|F_PIX, 0, 1023,
   "H-Sync width",
   "H-sync pulse width (time) in pixel clock pulses (0–1023)." },
   {&EDID_cl::DTD_VOsync, 0, offsetof(dtd_t, Vsize_8lsb)+1, 0, 6, F_INT|F_BFD|F_PIX, 0, 63,
   "V-Sync offs",
   "V-sync offset as number of H-lines (0–63). This offset value is responsible for vertical "
   "picture alignment. Higher values are shifting the picture to the top edge of the screen." },
   {&EDID_cl::DTD_VsyncW, 0, offsetof(dtd_t, Vsize_8lsb)+1, 0, 6, F_INT|F_BFD|F_PIX, 0, 63,
   "V-Sync width",
   "V-sync pulse width (time) as number of H-lines (0–63)" },
   {&EDID_cl::DTD_Hsize, 0, offsetof(dtd_t, Hsize_8lsb), 0, 12, F_INT|F_BFD|F_MM, 0, 4095,
   "H-Size",
   "Horizontal display size, mm (0–4095)" },
   {&EDID_cl::DTD_Vsize, 0, offsetof(dtd_t, Vsize_8lsb), 0, 12, F_INT|F_BFD|F_MM, 0, 4095,
   "V-Size",
   "Vertical display size, mm (0–4095)" },
   {&EDID_cl::ByteVal, 0, offsetof(dtd_t, Hborder_pix), 0, 1, F_BTE|F_INT|F_PIX, 0, 255,
   "H-Border pix",
   "Horizontal border pixels: overscan compensation (each side; total is twice this)" },
   {&EDID_cl::ByteVal, 0, offsetof(dtd_t, Vborder_lin), 0, 1, F_BTE|F_INT|F_PIX, 0, 255,
   "V-Border lines",
   "Vertical border pixels: overscan compensation (each side; total is twice this)" },
//Features flags
   {&EDID_cl::BitF8Val, 0, offsetof(dtd_t, features), 3, 2, F_BFD, 0, 3, "sync_type",
   "Sync type:\n 00=Analog composite\n 01=Bipolar analog composite\n"
   " 10=Digital composite (on HSync)\n 11=Digital separate" },
   {&EDID_cl::BitVal, 0, offsetof(dtd_t, features), 1, 1, F_BIT, 0, 1, "Hsync_type",
   "Analog sync: 1: Sync on all 3 RGB lines, 0: sync_on_green\n"
   "Digital: HSync polarity: 1=positive" },
   {&EDID_cl::BitVal, 0, offsetof(dtd_t, features), 2, 1, F_BIT, 0, 1, "Vsync_type",
   "Separated digital sync: Vsync polarity: 1=positive\n"
   "Other types: composite VSync (HSync during VSync)" },
   {&EDID_cl::BitVal, 0, offsetof(dtd_t, features), 0, 1, F_BIT, 0, 1, "il2w_stereo",
   "2-way line-interleaved stereo, if \"stereo_mode\" is not 0b00" },
   {&EDID_cl::BitF8Val, 0, offsetof(dtd_t, features), 5, 2, F_BFD, 0, 3, "stereo_mode",
   "Stereo mode:\n00=No stereo\nother values depend on \"il2w_stereo\" (bit 0):\n"
   "il2w_stereo=0:\n"
   "01= Field sequential stereo, stereo sync=1 during right image\n"
   "10= Field sequential stereo, stereo sync=1 during left image\n"
   "11= 4-way interleaved stereo\n"
   "il2w_stereo=1:\n"
   "01= 2-way interleaved stereo, right image on even lines\n"
   "10= 2-way interleaved stereo, left image on even lines\n"
   "11= Side-by-side interleaved stereo\n\n" },
   {&EDID_cl::BitVal, 0, offsetof(dtd_t, features), 7, 1, F_BIT, 0, 1, "interlace",
   "interlaced" }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode dtd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   static const u32_t fcount = 19;

   rcode retU;

   parent_grp  = parent;
   type_id.t32 = ID_DTD;

   CopyInstData(inst, sizeof(dtd_t));

   retU = init_fields(&fields[0], inst_data, fcount, false, Name, Desc, CodN);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

void dtd_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   u32_t         ival;
   u32_t         xres;
   u32_t         yres;
   edi_dynfld_t *p_field;

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   //X-res
   p_field = FieldsAr.Item(DTD_IDX_HAPIX);
   EDID.gp_name.Empty();
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, xres, p_field );
   gp_name  = EDID.gp_name;
   gp_name << "x";

   //Y-res
   p_field = FieldsAr.Item(DTD_IDX_VALIN);
   EDID.gp_name.Empty();
   ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, yres, p_field );
   gp_name << EDID.gp_name;
   gp_name << " @ ";


   {  //V-Refresh
      float lineW, pixclk, lineT, n_lines, frameT;

      p_field = FieldsAr.Item(DTD_IDX_PIXCLK); //H-freq (pix_clk)
      ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
      pixclk  = (float) (ival * 10000); //x10kHz

      p_field = FieldsAr.Item(DTD_IDX_HBPIX); //H-blank
      ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
      lineW   = (float) (ival + xres);
      lineT   = (lineW / pixclk);

      p_field = FieldsAr.Item(DTD_IDX_VBLIN); //V-blank
      ( EDID.*p_field->field.handlerfn )(OP_READ, EDID.gp_name, ival, p_field );
      n_lines = (float) (ival + yres);
      frameT  = (n_lines * lineT);

      EDID.gp_name.Printf("%.02fHz", (1.0/frameT));
      gp_name << EDID.gp_name;
   }
}

//----> Alternative Descriptors

//Alt. Descriptor type handler
rcode EDID_cl::ALT_DType (u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   u32_t   dtype;
   unk_t  *pdsc;

   pdsc  = reinterpret_cast <unk_t*> (getInstancePtr(p_field));
   dtype = pdsc->hdr.dsc_type;

   retU = ByteVal(op, sval, ival, p_field);
   if (! RCD_IS_OK(retU)) return retU;

   if (dtype != pdsc->hdr.dsc_type) {
      RCD_SET_TRUE(retU); //Desc. type changed
   }

   return retU;
}

//Alt. Descriptor type selector.
sm_vmap AltDescType_map = {
   {0xFF, {0, "MSN" , "Monitor Serial Number"}},
   {0xFE, {0, "UTX" , "Unspecified Text"     }},
   {0xFD, {0, "MRL" , "Monitor Range Limits" }},
   {0xFC, {0, "MND" , "Monitor Name"         }},
   {0xFB, {0, "WPD" , "White Point Data"     }},
   {0xFA, {0, "AST" , "Additional Standard Timings"       }},
   {0xF9, {0, "DCM" , "Display Color Management"          }},
   {0xF8, {0, "CT3" , "VESA-CVT 3-byte Video Timing Codes"}},
   {0xF7, {0, "ET3" , "Estabilished Timings 3"            }},
   {0x10, {0, "VOID", "dummy place holder descriptor"     }}
};

static const char altHdrDsc[] = "Zero hdr (3 bytes) -> DTD:pix_clk=0, HApix_8lsb=0 -> "
                                "alternative descriptor type (means not a DTD)";
static const char altZeroDsc[] = "Mandatory zero for a non-DTD (alternative) descriptor.";

static const char AltDesc[] = "Defined descriptor types:\n"
" 0xFF: MSN: Monitor serial number (text, ASCII)\n"
" 0xFE: UTX: Unspecified text (text, ASCII)\n"
" 0xFD: MRL: Monitor range limits: (Mandatory) 6- or 13-byte binary descriptor.\n"
" 0xFC: MND: Monitor name (text, ASCII), padded with 0x0A, 0x20 (..) (LF, SP)\n"
" 0xFB: WPD: Additional white point data. 2x 5-byte descriptors, padded with 0A 20 20.\n"
" 0xFA: AST: Additional Standard Timing Identifiers. 6x 2-byte descriptors, padded with 0A.\n"
" 0xF9: DCM: Display Color Management Data\n"
" 0xF8: CT3: VESA-CVT 3-byte Video Timing Codes\n"
" 0xF7: ET3: Estabilished Timings 3 Descriptor\n"
" 0x10: VOID: Dummy descriptor for marking unused space, all bytes should be 0x00\n"
" 0x00-0x0F reserved for vendors, interpreted as UNK (unknown structures).\n";

//Alt. Descriptor header: shared.
edi_field_t AltDescHdr[] = {
//header
   {&EDID_cl::Word24, 0, offsetof(dshd_t, zero_hdr), 0, 3, F_STR|F_HEX|F_RD, 0, 0xFFFFFF,
   "zero_hdr", altHdrDsc },
   {&EDID_cl::ALT_DType, VS_ALT_DSC_TYPE, offsetof(dshd_t, dsc_type), 0, 1, F_BTE|F_HEX|F_VS|F_RD, 0, 255,
   "desc_type", AltDesc },
   {&EDID_cl::ByteVal, 0, offsetof(dshd_t, rsvd4), 0, 1, F_BTE|F_HEX|F_RD, 0, 0, "rsvd4",
   altZeroDsc },
};

//version field, used by CT3 and ET3
static const edi_field_t dsc_ver_fld[] = {
   {&EDID_cl::ByteVal, 0, offsetof(dcm_t, ver), 0, 1, F_BTE|F_HEX|F_RD, 0, 255,
   "ver", NULL }
};


//MRL: Monitor Range Limits Descriptor (type 0xFD)
//MRL: V/H rate limits: EDID 1.4 adds 255 to a limit when its offset flag in
//byte 4 is set; bits 0-1: V-rate, bits 2-3: H-rate, 0b10: max, 0b11: max and min
rcode EDID_cl::MRL_Freq(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   mrl_t  *inst;
   u8_t   *pval;
   u32_t   offs;
   u32_t   shift;
   u32_t   flags;
   bool    is_max;
   bool    max_ofs;
   bool    min_ofs;

   inst    = reinterpret_cast <mrl_t*> (getInstancePtr(p_field));
   pval    = getValPtr(p_field);
   offs    = p_field->field.offs;
   shift   = (offs <= offsetof(mrl_t, max_Vfreq)) ? 0 : 2;
   is_max  = (offs == offsetof(mrl_t, max_Vfreq)) || (offs == offsetof(mrl_t, max_Hfreq));
   flags   = (inst->hdr.rsvd4 >> shift) & 0x3;
   max_ofs = (flags & 0x2) != 0;
   min_ofs = (flags == 0x3);

   if (op == OP_READ) {
      ival  = pval[0];
      ival += (is_max ? max_ofs : min_ofs) ? 255 : 0;
      sval << ival;
      RCD_RETURN_OK(retU);
   }

   ulong tmpv;
   if (op == OP_WRSTR) {
      retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      if (! RCD_IS_OK(retU)) return retU;
   } else if (op == OP_WRINT) {
      if (ival > p_field->field.maxv) RCD_RETURN_FAULT(retU);
      tmpv = ival;
   } else {
      RCD_RETURN_FAULT(retU); //wrong op code
   }

   bool offset = (tmpv > 255);
   if (is_max) {
      if (! offset && min_ofs) {
         RCD_RETURN_FAULT_MSG(retU, "[E!] MRL: the maximum rate can't be below the "
                                    "minimum rate above 255");
      }
      max_ofs = offset;
   } else {
      if (offset && ! max_ofs) {
         RCD_RETURN_FAULT_MSG(retU, "[E!] MRL: a minimum rate above 255 needs a "
                                    "maximum rate above 255");
      }
      min_ofs = offset;
   }
   flags = (max_ofs ? 0x2 : 0) | (min_ofs ? 0x1 : 0);
   inst->hdr.rsvd4 = (inst->hdr.rsvd4 & ~(0x3 << shift)) | (flags << shift);
   pval[0] = (tmpv - (offset ? 255 : 0)) & 0xFF;
   RCD_RETURN_OK(retU);
}

//MRL: extension selector: handlers
rcode EDID_cl::MRL_02_GTFM(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode      retU;
   mrl_gtf_t *inst;

   inst = reinterpret_cast <mrl_gtf_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival = inst->gtf_m;
      sval << ival;
      RCD_SET_OK(retU);
   } else {
      ulong   tmpv;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 10, p_field->field.minv, p_field->field.maxv, tmpv);
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->gtf_m = tmpv;
   }
   return retU;
}

rcode EDID_cl::MRL_MaxPixClk(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   u32_t   dtype;
   mrl_t  *inst;
   i32_t   ckr10;
   float   fract;
   float   tmpv  = 0.0;

   inst  = reinterpret_cast <mrl_t*> (getInstancePtr(p_field));
   dtype = inst->mrl_ext;

   if (op == OP_READ) {
      ival = (inst->max_pixclk * 10);
      tmpv = (double) ival;
      if (0x04 == dtype) {
         ival   = inst->ex_dat.cvt_s.maxPixClk_6lsb;
         fract  = (float) ival;
         fract *= 0.25;
         tmpv -= fract;
      }
      sval.Printf("%.02f", tmpv);
      RCD_SET_OK(retU);
   } else {
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU  = getStrFloat(sval, 10.0, 2550.0, tmpv);
         ckr10 = (i32_t) tmpv;

      } else if (op == OP_WRINT) {
         ckr10 = ival;
         tmpv  = (float) ival;
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      if (0x04 == dtype) {
         //value rounded-up to multiple of 10MHz
         ckr10  = (ckr10 / 10) +1;
         ckr10 *= 10;
         //num of 0.25MHz units
         fract  = ((float) ckr10) - tmpv;
         fract /= 0.25;
         ival   = (u32_t) fract;

         if (ival >= 40) { //auto-adjust max_pixclk
            ival  -= 40;
            ckr10 -= 10;
            if (ckr10 <= 0) RCD_RETURN_FAULT(retU);
         }
         inst->ex_dat.cvt_s.maxPixClk_6lsb = ival;
      }
      ival = ckr10;

      ival /= 10;
      inst->max_pixclk = ival;
   }

   return retU;
}

rcode EDID_cl::MRL_04_PixClk(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   mrl_t  *inst;
   i32_t   ckr10;
   float   tmpv;

   inst  = reinterpret_cast <mrl_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival  = inst->ex_dat.cvt_s.maxPixClk_6lsb;
      tmpv  = (float) ival;
      tmpv *= 0.25;
      sval.Printf("%.02f", tmpv);
      RCD_SET_OK(retU);
   } else {

      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU  = getStrFloat(sval, 0.0, 15.75, tmpv);
         tmpv /= 0.25;
         ival  = (u32_t) tmpv;
      } else if (op == OP_WRINT) {
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      if (ival >= 40) { //auto-adjust max_pixclk
         ival  -= 40;
         ckr10  = inst->max_pixclk;
         ckr10 -= 1;
         if (ckr10 <= 0) RCD_RETURN_FAULT(retU);
         inst->max_pixclk = ckr10;
      }

      inst->ex_dat.cvt_s.maxPixClk_6lsb = ival;
   }

   return retU;
}

rcode EDID_cl::MRL_04_HAlin(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   mrl_t  *inst;

   inst  = reinterpret_cast <mrl_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival   = inst->ex_dat.cvt_s.maxHApix_2msb;
      ival <<= 8;
      ival  |= inst->ex_dat.cvt_s.maxHApix_8lsb;
      sval  << ival;
      RCD_SET_OK(retU);
   } else {
      ulong  tmpv;

      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU  = getStrUint(sval, 10, 0, 1023, tmpv);
         ival  = (u32_t) tmpv;
      } else if (op == OP_WRINT) {
         RCD_SET_OK(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      inst->ex_dat.cvt_s.maxHApix_8lsb = ival;
      ival >>= 8;
      inst->ex_dat.cvt_s.maxHApix_2msb = ival;
   }

   return retU;
}

//MRL: extension selector
sm_vmap MRL_ext_map = {
   {0x00, {0, "Default GTF support", NULL}},
   {0x01, {0, "Range Limits only"  , NULL}},
   {0x02, {0, "Secondary GTF"      , NULL}},
   {0x04, {0, "CVT support info"   , NULL}},
};

const char  mrl_cl::CodN[] = "MRL";
const char  mrl_cl::Name[] = "Monitor Range Limits";
const char  mrl_cl::Desc[] =
"MRL describes supported H/V-frequency range and maximum Pixel Clock.\n"
"Optionally, a Secondary GTF curve parameters or CVT support information "
"can be included - this depends on the value of 'mrl_ext' field";

//byte 4: rate offsets, written by the rate handlers
const edi_field_t mrl_cl::offsets_fld[] = {
   {&EDID_cl::ByteVal, 0, offsetof(dshd_t, rsvd4), 0, 1, F_BTE|F_HEX|F_RD, 0, 0x0F,
   "rate_offsets",
   "EDID 1.4: rate offsets, set when the rate fields are written:\n"
   "bits 0-1: V-rate, bits 2-3: H-rate:\n"
   "0b00: no offset,\n0b10: +255 for the maximum,\n0b11: +255 for the maximum and minimum\n"
   "bits 4-7: reserved (0)\n\n"
   "EDID 1.3: mandatory zero" },
};

const edi_field_t mrl_cl::fields[] = {
//data: bytes 5-17
   {&EDID_cl::MRL_Freq, 0, offsetof(mrl_t, min_Vfreq), 0, 1, F_INT|F_HZ|F_FR, 1, 510,
   "min_Vfreq", "minimal V-frequency: 1..255Hz, EDID 1.4: 1..510Hz" },
   {&EDID_cl::MRL_Freq, 0, offsetof(mrl_t, max_Vfreq), 0, 1, F_INT|F_HZ|F_FR, 1, 510,
   "max_Vfreq", "maximum V-frequency: 1..255Hz, EDID 1.4: 1..510Hz" },
   {&EDID_cl::MRL_Freq, 0, offsetof(mrl_t, min_Hfreq), 0, 1, F_INT|F_KHZ|F_FR, 1, 510,
   "min_Hfreq", "minimal H-frequency: 1..255kHz, EDID 1.4: 1..510kHz" },
   {&EDID_cl::MRL_Freq, 0, offsetof(mrl_t, max_Hfreq), 0, 1, F_INT|F_KHZ|F_FR, 1, 510,
   "max_Hfreq", "maximum H-frequency: 1..255kHz, EDID 1.4: 1..510kHz" },
   {&EDID_cl::MRL_MaxPixClk, 0, offsetof(mrl_t, max_pixclk), 0, 1, F_BTE|F_FLT|F_FR|F_MHZ, 10, 2550,
   "max_PixClk",
   "Max pixel clock rate.\n"
   "Stored value: (pix_clk/10 MHz), range: 10..2550MHz\n\n"
   "If 'mrl_ext' == 0x04, that is, CVT support information is available, "
   "then additional pixel clock bits from the extension are taken into account (maxPixClk_apb)\n\n"
   "The field handler reads from and writes to both those fields.\n"
   "With additional precision bits, the accuracy is increased to 0,25MHz\n\n"
   "Additionally, the field handler ensures that maxPixClk_apb never exceeds 10MHz, "
   "by adjusting max_PixClk to multiple of 10MHz units." },
   {&EDID_cl::ByteVal, VS_MRL_EXT, offsetof(mrl_t, mrl_ext), 0, 1,
   F_BTE|F_HEX|F_FR|F_VS, 0, 0x04,
   "mrl_ext",
   "Extended timing info: defines interpretation of bytes 12-17\n\n"
   "0x00: Default GTF supported if enabled in SPF (no secondary GTF)\n"
   "0x01: Range Limits only,\n"
   "      in both cases bytes 12-17 should be set to 0x0A 20 20 20 20 20\n"
   "0x02: Secondary GTF parameters in bytes 12-17\n"
   "0x04: VESA CVT support information in bytes 12-17\n\n"
   "other values are reserved"},
};

#define mrl_offs(_ofs) \
   (offsetof(mrl_t, ex_dat) + offsetof(mrl_gtf_t, _ofs))

const edi_field_t mrl_cl::fields_GTF[] = {
   {&EDID_cl::ByteVal, 0, mrl_offs(resvd), 0, 1, F_BTE|F_HEX|F_RD, 0, 0, "resvd",
   "Mandatory zero if ext_timg is 0x02 otherwise it should be 0x0A (LF)" },
   {&EDID_cl::ByteVal, 0, mrl_offs(sfreq_sec), 0, 1, F_BTE|F_INT, 0, 255, "sfreq_sec",
   "Start frequency for secondary curve, 2 kHz units (0–510 kHz)" },
   {&EDID_cl::ByteVal, 0, mrl_offs(gtf_c), 0, 1, F_BTE|F_INT, 0, 255, "gtf_c",
   "GTF C value, multiplied by 2: 0...127.5 -> 0...255" },
   {&EDID_cl::MRL_02_GTFM, 0, mrl_offs(gtf_m), 0, 2, F_INT, 0, 65535, "gtf_m",
   "GTF M value (0–65535, LE)" },
   {&EDID_cl::ByteVal, 0, mrl_offs(gtf_k), 0, 1, F_BTE|F_INT, 0, 255, "gtf_k",
   "GTF K value (0–255)" },
   {&EDID_cl::ByteVal, 0, mrl_offs(gtf_j), 0, 1, F_BTE|F_INT, 0, 255, "gtf_j",
   "GTF J value, multiplied by 2: 0...127.5 -> 0...255" }
};
#undef mrl_offs

#define mrl_offs(_ofs) \
   (offsetof(mrl_t, ex_dat) + offsetof(mrl_cvt_t, _ofs))

const edi_field_t mrl_cl::fields_CVT[] = {
   {&EDID_cl::BitF8Val, 0, mrl_offs(cvt_ver), 4, 4, F_BFD|F_INT, 0, 15,
   "CVT_majorV",
   "CVT standard major version number, 4msb, range 1..15" },
   {&EDID_cl::BitF8Val, 0, mrl_offs(cvt_ver), 0, 4, F_BFD|F_INT, 0, 15,
   "CVT_minorV",
   "CVT standard minor version number, 4lsb, range 0..15" },
   {&EDID_cl::MRL_04_PixClk, 0, 12, 2, 6, F_BFD|F_FLT|F_MHZ|F_FR, 0, 63, "maxPixClk_apb",
   "Additional Precision Bits for use with byte9 'max_PixClk'\n"
   "The aditional bits are used to increase accuracy of max_PixClk to 0.25MHz\n\n"
   "Final max_PixClk=(max_PixClk-(maxPixClk_apb*0.25MHz))\n\n"
   "NOTE:\n"
   "The max field value is 63*0.25=15.75MHz, but the field handler automatically adjusts max_PixClk value,"
   "so that maxPixClk_apb never exceed 10MHz, (bitfield value: 40*0.25MHz)." },
   {&EDID_cl::ByteVal, 0, mrl_offs(maxHApix_8lsb), 0, 1, F_BTE|F_INT, 0, 1023,
   "max_HApix",
   "Max horizontal active pixels,\n10bit range: 0..1023, 0x00: no limit" },

   {&EDID_cl::BitF8Val, 0, 14, 0, 3, F_BFD|F_RD, 0, 0x3, "rsvd14_02",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, 14, 3, 1, F_BIT, 0, 1, "aspr_4_3",
   "aspect ratio 4:3 (1: supported )" },
   {&EDID_cl::BitVal, 0, 14, 4, 1, F_BIT, 0, 1, "aspr_16_9",
   "aspect ratio 16:9 (1: supported )" },
   {&EDID_cl::BitVal, 0, 14, 5, 1, F_BIT, 0, 1, "aspr_16_10",
   "aspect ratio 16:10 (1: supported )" },
   {&EDID_cl::BitVal, 0, 14, 6, 1, F_BIT, 0, 1, "aspr_5_4",
   "aspect ratio 5:4 (1: supported )" },
   {&EDID_cl::BitVal, 0, 14, 7, 1, F_BIT, 0, 1, "aspr_15_9",
   "aspect ratio 15:9 (1: supported )" },

   {&EDID_cl::BitF8Val, 0, 15, 0, 3, F_BFD|F_RD, 0, 0x3, "rsvd15_02",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, 15, 3, 1, F_BIT, 0, 1, "blank_std",
   "std CVT blanking, 1: supported" },
   {&EDID_cl::BitVal, 0, 15, 4, 1, F_BIT, 0, 1, "blank_rb",
   "CVT Reduced Blanking, 1: supported" },
   {&EDID_cl::BitF8Val, 0, 15, 5, 3, F_BFD, 0, 0x7, "pref_ar",
   "preferred aspect ratio:\n"
   "0b000: 4:3, \n0b001: 16:9, \n0b010: 16:10 \n0b011: 5:4, \n0b100: 15:9" },

   {&EDID_cl::BitF8Val, 0, 16, 0, 4, F_BFD|F_RD, 0, 0x3, "rsvd16_03",
   "reserved (0)" },
   {&EDID_cl::BitVal, 0, 16, 4, 1, F_BIT, 0, 1, "H_shrink",
   "Horizontal shrink, 1: supported" },
   {&EDID_cl::BitVal, 0, 16, 5, 1, F_BIT, 0, 1, "H_stretch",
   "Horizontal stretch, 1: supported" },
   {&EDID_cl::BitVal, 0, 16, 6, 1, F_BIT, 0, 1, "V_shrink",
   "Vertival shrink, 1: supported" },
   {&EDID_cl::BitVal, 0, 16, 7, 1, F_BIT, 0, 1, "V_stretch",
   "Vertival stretch, 1: supported" },

   {&EDID_cl::ByteVal, 0, mrl_offs(pref_vref), 0, 1, F_BTE|F_INT|F_HZ, 0, 255,
   "pref_vref",
   "Preferred V-Refresh rate, 1..255Hz, 0: reserved" },
};
#undef mrl_offs

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode mrl_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode  retU;

   type_id.t32 = ID_MRL;

   CopyInstData(inst, sizeof(mrl_t));

   retU = gen_data_layout(inst_data);

   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

rcode mrl_cl::ForcedGrpRefresh() {
   rcode  retU;

   retU = gen_data_layout(inst_data);

   return retU;
}

rcode mrl_cl::gen_data_layout(const u8_t* inst) {
   enum {
      fcount   = 6,
      fcnt_gtf = 6,
      unk_fcnt = fcnt_gtf +1, //gtf_m -> 2 bytes
      fcnt_cvt = 20
   };

   const edi_field_t *fld_ar;
         edi_field_t *fld_buf;

   rcode  retU;
   u32_t  dtype;
   u32_t  e_fcnt;
   bool   append_md = true;

   dtype = reinterpret_cast<const mrl_t*> (inst)->mrl_ext;

   retU = init_fields(&AltDescHdr[0], inst_data, 2, !append_md, Name, Desc, CodN);
   if (! RCD_IS_OK(retU)) return retU;

   retU = init_fields(&offsets_fld[0], inst_data, 1, append_md);
   if (! RCD_IS_OK(retU)) return retU;

   retU = init_fields(&fields[0], inst_data, fcount, append_md);
   if (! RCD_IS_OK(retU)) return retU;

   switch (dtype) {
      case 0x04: //CVT support mode
         fld_ar = fields_CVT;
         e_fcnt = fcnt_cvt;
         break;
      case 0x02: //GTF mode
         fld_ar = fields_GTF;
         e_fcnt = fcnt_gtf;
         break;
      case 0x00:
      case 0x01:
      default:
         //unused descriptor data:
         e_fcnt    = unk_fcnt;
         fld_buf   = (edi_field_t*) realloc(dyn_fldar, unk_fcnt * EDI_FIELD_SZ );
         if (NULL == fld_buf) RCD_RETURN_FAULT(retU);
         dyn_fldar = fld_buf;

         insert_unk_byte(dyn_fldar, unk_fcnt, offsetof(mrl_t, ex_dat));
         fld_ar = dyn_fldar;
   }

   retU = init_fields(fld_ar, inst_data , e_fcnt, append_md);
   if (! RCD_IS_OK(retU)) return retU;

   return retU;
}

//WPD: White Point Descriptor
rcode EDID_cl::WPD_pad(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   wpd_t  *inst = NULL;

   inst = reinterpret_cast <wpd_t*> (getInstancePtr(p_field));

   if (op == OP_READ) {
      ival  =  inst->pad[0];
      ival |= (inst->pad[1] << 8);
      ival |= (inst->pad[2] << 16);
      sval.Printf("%06X", ival);
      RCD_SET_OK(retU);
   } else {
      ulong tmpv = 0;
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         retU = getStrUint(sval, 16, p_field->field.minv, p_field->field.maxv, tmpv);
         if (! RCD_IS_OK(retU)) return retU;
      } else if (op == OP_WRINT) {
         tmpv = ival;
         RCD_SET_OK(retU);
      }

      inst->pad[0] = (tmpv & 0xFF);
      tmpv = tmpv >> 8;
      inst->pad[1] = (tmpv & 0xFF);
      tmpv = tmpv >> 8;
      inst->pad[2] = (tmpv & 0xFF);
   }
   return retU;
}

//WPD: White Point Descriptor (type 0xFB)
const char  wpt_cl::CodN[] = "WPD";
const char  wpt_cl::Name[] = "White Point Descriptor";
const char  wpt_cl::Desc[] =
"2 additional sets of chromacity coordinates with Gamma value";

const edi_field_t wpt_cl::fields[] = {
//data: bytes 5-17
//white point desc. 1
   {&EDID_cl::ByteVal, 0, offsetof(wpd_t, wp1_idx), 0, 1, F_BTE, 0, 255, "wp1_idx",
   "White point1 index number (1–255), 0 -> descriptor not used." },
   {&EDID_cl::ByteVal, 0, offsetof(wpd_t, wp1x_8msb), 0, 1, F_BTE, 0, 255, "wp1_x",
   "White point1 x value." },
   {&EDID_cl::ByteVal, 0, offsetof(wpd_t, wp1y_8msb), 0, 1, F_BTE, 0, 255, "wp1_y",
   "White point1 y value." },
   {&EDID_cl::Gamma, 0, offsetof(wpd_t, wp1_gamma), 0, 1, 0, 0, 255, "wp1_gamma",
   "(gamma*100)-100 (1.0 ... 3.54)" },
//white point desc. 2
   {&EDID_cl::ByteVal, 0, offsetof(wpd_t, wp2_idx), 0, 1, F_BTE, 0, 255, "wp2_idx",
   "White point1 index number (1–255), 0 -> descriptor not used." },
   {&EDID_cl::ByteVal, 0, offsetof(wpd_t, wp2x_8msb), 0, 1, F_BTE, 0, 255, "wp2_x",
   "White point2 x value." },
   {&EDID_cl::ByteVal, 0, offsetof(wpd_t, wp2y_8msb), 0, 1, F_BTE, 0, 255, "wp2_y",
   "White point2 y value." },
   {&EDID_cl::Gamma, 0, offsetof(wpd_t, wp2_gamma), 0, 1, 0, 0, 255, "wp2_gamma",
   "(gamma*100)-100 (1.0 ... 3.54)" },
//pad
   {&EDID_cl::WPD_pad, 0, offsetof(wpd_t, pad), 0, 3, F_STR|F_HEX|F_RD, 0, 0x0A2020, "pad",
   "unused, should be 0A 20 20. (LF,SP,SP)" }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode wpt_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 9
   };

   rcode  retU;
   bool   append_md = true;

   type_id.t32 = ID_WPD;

   CopyInstData(inst, sizeof(wpd_t));

   retU = init_fields(&AltDescHdr[0], inst_data, 3, !append_md, Name, Desc, CodN);
   if (! RCD_IS_OK(retU)) return retU;

   retU = init_fields(&fields[0], inst_data, fcount, append_md);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

//Common class for text descriptors
//MND: Monitor Name Descriptor (type 0xFC)
//UTX: UnSpecified Text (type 0xFE)
//MSN: Monitor Serial Number Descriptor (type 0xFF)
const char  txtd_cl::CodN_MND[] = "MND";
const char  txtd_cl::Name_MND[] = "Monitor Name Descriptor";
const char  txtd_cl::CodN_UTX[] = "UTX";
const char  txtd_cl::Name_UTX[] = "Unspecified Text";
const char  txtd_cl::CodN_MSN[] = "MSN";
const char  txtd_cl::Name_MSN[] = "Monitor Serial Number";

const char *txtd_cl::Desc       = AltDesc;

const char  txtd_cl::fname_mnd[] = "Monitor name";
const char  txtd_cl::fname_utx[] = "Text";
const char  txtd_cl::fname_msn[] = "Monitor SN";

const char  txtd_cl::dsc_mnd[] =
"Monitor name: text string,\n"
"padded with 0x0A,0x20,0x20, ... (LF,SP,SP, ...), max 13 chars.";

const char  txtd_cl::dsc_utx[] =
"Unspecified (general purpose) text string,\n"
"padded with 0x0A,0x20,0x20, ... (LF,SP,SP, ...), max 13 chars.";

const char  txtd_cl::dsc_msn[] =
"Monitor serial number: text string,\n"
"padded with 0x0A,0x20,0x20, ... (LF,SP,SP, ...), max 13 chars.";

const edi_field_t txtd_cl::fields[] = {
   {&EDID_cl::FldPadStr, 0, offsetof(utx_t, text), 0, 13, F_STR|F_FR|F_RD|F_GD|F_NI|F_DN, 0, 0,
   NULL, NULL },
   {&EDID_cl::ByteStr, 0, offsetof(utx_t, text), 0, 13, F_STR|F_HEX|F_FR|F_RD|F_NI|F_DN, 0, 0,
   "hex_text",
   "Hexadecimal representation of the text string.\n"
   "This fields overlaps the text field.\n" }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode txtd_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   const char *CodN;
   const char *Name;
   const char *fname;
   const char *fdsc;

   rcode         retU;
   u8_t          dtype;
   u32_t         fidx;
   edi_dynfld_t *ftxt;
   bool          append_md = true;

   dtype = reinterpret_cast<const dshd_t*> (inst)->dsc_type;

   switch (dtype) {
      case 0xFC: //MND
         type_id.t32 = ID_MND;
         CodN  = CodN_MND;
         Name  = Name_MND;
         fname = fname_mnd;
         fdsc  = dsc_mnd;
         break;
      case 0xFE: //UTX
         type_id.t32 = ID_UTX;
         CodN  = CodN_UTX;
         Name  = Name_UTX;
         fname = fname_utx;
         fdsc  = dsc_utx;
         break;
      case 0xFF: //MSN
         type_id.t32 = ID_MSN;
         CodN  = CodN_MSN;
         Name  = Name_MSN;
         fname = fname_msn;
         fdsc  = dsc_msn;
         break;
      default:
         RCD_RETURN_FAULT(retU);
   }

   CopyInstData(inst, sizeof(utx_t));

   retU = init_fields(&AltDescHdr[0], inst_data, 3, false, Name, fdsc, CodN);
   if (! RCD_IS_OK(retU)) return retU;

   //append text and hex_text fields, then set the description
   fidx = FieldsAr.GetCount(); //3

   retU = init_fields(&fields[0], inst_data, 2, append_md);
   if (! RCD_IS_OK(retU)) return retU;

   ftxt = FieldsAr.Item(fidx);
   ftxt->field.name = fname;

   RCD_RETURN_OK(retU);
}
#pragma GCC diagnostic warning "-Wunused-parameter"

rcode txtd_cl::StrValid() {
   rcode       retU;
   const u8_t *cbuf;
   u8_t        cval;
   u32_t       itc;

   cbuf = reinterpret_cast<const utx_t*> (inst_data)->text;

   for (itc=0; itc<13; ++itc) {
      cval = cbuf[itc];
      if ((cval < 0x20) || (cval > 0x7E)) {
         if (cval != 0x0A) RCD_RETURN_FALSE(retU);
      }
   }

   RCD_RETURN_TRUE(retU);
}

void txtd_cl::getGrpName(EDID_cl& EDID, wxc_String& gp_name) {
   rcode         retU;
   u32_t         ival;
   edi_dynfld_t *p_field;

   if (! EDID.b_GrpNameDynamic) {
      gp_name = GroupName;
      return;
   }

   retU = StrValid();
   if (! RCD_IS_TRUE(retU)) {
      gp_name = "<invalid_str>";
      return;
   }

   //descriptor text
   p_field = FieldsAr.Item(3);
   ( EDID.*p_field->field.handlerfn )(OP_READ, gp_name, ival, p_field );
}

//Additional Standard Timings
//AST: Additional Standard Timings identifiers (type 0xFA)

const char ast_cl::Desc[] =
"Up to 6 DMT 2-byte Video Timing Codes.\n"
"Unused bytes bytes should be set to 0x01, except the last byte in AST, which should be set to 0x0A (LF)";

const edi_rootgp_dsc_t ast_cl::AST_dsc = {
   .CodN     = "AST",
   .Name     = "Additional Standard Timings",
   .Desc     = ast_cl::Desc,
   .PadDsc   = "Single padding byte, required 0x0A (LF)",
   .type_id  = ID_AST | ID_DMT2,
   .grp_offs = 0,
   .grp_arsz = 1,
   .grp_ar   = ast_cl::STI_grp
};

const edi_subg_dsc_t ast_cl::STI_grp[] = {
   {
      .s_ctor   = &dmt_std2_cl::group_new,
      .inst_cnt = 6
   }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode ast_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode retU;

   orflags |= (T_P_HOLDER|T_SUB_GRP);

   retU = base_AltDesc_Init_RootGrp(inst, &AST_dsc, orflags, parent);

   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

//DCM: Display Color Management Data (type 0xF9)
rcode EDID_cl::DCM_coef16(u32_t op, wxc_String& sval, u32_t& ival, edi_dynfld_t* p_field) {
   rcode   retU;
   u8_t   *p_w16;
   i32_t   c32;
   i16_t   c16;
   float   f32;

   RCD_SET_FAULT(retU);

   p_w16 = getValPtr(p_field);

   if (op == OP_READ) {
      c32  = (i32_t) (i16_t) rdWord16_LE(p_w16);
      f32  = (float) c32;
      f32 /= 100.0; //display coeff. values, not encoding

      ival = c32; //not used
      sval.Printf("%.03f", f32);
      RCD_SET_OK(retU);
   } else {
      RCD_SET_FAULT(retU);

      if (op == OP_WRSTR) {
         float  tmpv = 0.0;
         retU = getStrFloat(sval, -32768.0, 32767.0, tmpv);
         tmpv *= 100.0;
         c16   = (i16_t) tmpv;

      } else if (op == OP_WRINT) {
         RCD_SET_FAULT(retU);
      }
      if (! RCD_IS_OK(retU)) return retU;

      wrWord16_LE(p_w16, c16);
   }
   return retU;
}

const char  dcm_cl::CodN[] = "DCM";
const char  dcm_cl::Name[] = "Display Color Management";
const char  dcm_cl::Desc[] =
"Polynomial coefficients:\n"
"stored value = coef_val*100, rounded to int16_t, LE";

const char dcm_cl::VerDsc[] = "DCM version = 0x03, other values reserved";

const edi_field_t dcm_cl::fields[] = {
//data: bytes 6-17
   {&EDID_cl::DCM_coef16, 0, offsetof(dcm_t, red_a3), 0, 2, F_FLT, 0, 0xFFFF,
   "red_a3", NULL },
   {&EDID_cl::DCM_coef16, 0, offsetof(dcm_t, red_a2), 0, 2, F_FLT, 0, 0xFFFF,
   "red_a2", NULL },
   {&EDID_cl::DCM_coef16, 0, offsetof(dcm_t, grn_a3), 0, 2, F_FLT, 0, 0xFFFF,
   "green_a3", NULL },
   {&EDID_cl::DCM_coef16, 0, offsetof(dcm_t, grn_a2), 0, 2, F_FLT, 0, 0xFFFF,
   "green_a2", NULL },
   {&EDID_cl::DCM_coef16, 0, offsetof(dcm_t, blu_a3), 0, 2, F_FLT, 0, 0xFFFF,
   "blue_a3", NULL },
   {&EDID_cl::DCM_coef16, 0, offsetof(dcm_t, blu_a2), 0, 2, F_FLT, 0, 0xFFFF,
   "blue_a2", NULL }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode dcm_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      fcount = 6
   };

   rcode         retU;
   u32_t         fidx;
   edi_dynfld_t *fver;
   bool          append_md = true;

   type_id.t32 = ID_DCM;

   CopyInstData(inst, sizeof(utx_t));

   retU = init_fields(&AltDescHdr[0], inst_data, 3, !append_md, Name, Desc, CodN);
   if (! RCD_IS_OK(retU)) return retU;

   //append version field, then set its description
   fidx = FieldsAr.GetCount(); //4

   retU = init_fields(&dsc_ver_fld[0], inst_data, 1, append_md);

   fver = FieldsAr.Item(fidx);
   fver->field.desc = VerDsc;

   //append DCM fields
   retU = init_fields(&fields[0], inst_data, fcount, append_md);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

//CT3: VESA-CVT 3-byte Video Timing Codes (type 0xF8)
const char ct3_cl::Desc[] =
"Up to 4 CVT-3 Video Timing Codes.\n"
"Unused bytes bytes should be set to 0x00";

const char ct3_cl::VerDsc[] = "CT3 version = 0x01, other values reserved";

const edi_rootgp_dsc_t ct3_cl::CT3_dsc = {
   .CodN     = "CT3",
   .Name     = "CVT-3 Video Timing Codes",
   .Desc     = ct3_cl::Desc,
   .PadDsc   = NULL,
   .type_id  = ID_CT3,
   .grp_offs = 1, //version field
   .grp_arsz = 1,
   .grp_ar   = ct3_cl::CVT3_grp
};

const edi_subg_dsc_t ct3_cl::CVT3_grp[] = {
   {
      .s_ctor   = &dmt_cvt3_cl::group_new,
      .inst_cnt = 4
   }
};

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode ct3_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   rcode         retU;
   u32_t         fidx;
   edi_dynfld_t *fver;
   bool          append_md = true;

   orflags |= (T_P_HOLDER|T_SUB_GRP);

   retU = base_AltDesc_Init_RootGrp(inst, &CT3_dsc, orflags, parent);
   if (! RCD_IS_OK(retU)) return retU;

   //append version field, then set its description
   fidx = FieldsAr.GetCount(); //4

   retU = init_fields(&dsc_ver_fld[0], inst_data, 1, append_md);
   if (! RCD_IS_OK(retU)) return retU;

   fver = FieldsAr.Item(fidx);
   fver->field.desc = VerDsc;

   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

//ET3: ET3: Estabilished Timings 3 Descriptor (type 0xF7)
extern const char* ET3_mode[]; //vid_fmt.cpp

const char  et3_cl::CodN[] = "ET3";
const char  et3_cl::Name[] = "Estabilished Timings 3";
const char  et3_cl::Desc[] =
"Additional estabilished timings:\n"
"Each bit indicates whether the corresponding timing is supported.";

const char et3_cl::VerDsc[] = "ET3 version = 0x0A, other values reserved";
const char et3_cl::flddsc[] = "Field name format: Xres_Yres_Vref<_RB>, (Reduced Blanking)";
const char et3_cl::resvd [] = "reserved (0)";

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode et3_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      //3byte hdr + 1byte ver + 5bytes * 8bits + 4bits in byte 11 + res11_03
      //+6 unused bytes
      f_alloc = 55
   };
   rcode         retU;
   u32_t         fidx;
   edi_dynfld_t *fver;
   bool          append_md = true;

   type_id.t32 = ID_ET3;

   CopyInstData(inst, sizeof(et3_t));

   FieldsAr.Alloc(f_alloc);

   retU = init_fields(&AltDescHdr[0], inst_data, 3, !append_md, Name, Desc, CodN);
   if (! RCD_IS_OK(retU)) return retU;

   //append version field, then set its description
   fidx = FieldsAr.GetCount(); //4

   retU = init_fields(&dsc_ver_fld[0], inst_data, 1, append_md);

   fver = FieldsAr.Item(fidx);
   fver->field.desc = VerDsc;

   retU = gen_data_fields();

   return retU;
}

rcode et3_cl::gen_data_fields() {
   enum {
      //5bytes * 8bits + 4bits in byte 11
      fcount   = 44,
      n_unused = 6  //unused bytes 12..17
   };

   edi_field_t  fld;
   const char  *vidfmt;
   rcode        retU;
   u32_t        itf;
   u32_t        offs;
   u32_t        shift;
   bool         append_md = true;

   memset(&fld, 0, sizeof(edi_field_t));

   fld.handlerfn = &EDID_cl::BitVal;
   fld.fld_sz    = 1;
   fld.flags     = F_BIT;
   fld.maxv      = 1;
   fld.desc      = flddsc;

   offs   = offsetof(et3_t, ver) +1;
   shift  = 0;

   for (itf=0; itf<fcount; ++itf) {

      vidfmt    = ET3_mode[itf];
      fld.offs  = offs;
      fld.shift = shift;
      fld.name  = vidfmt;

      retU = init_fields(&fld, inst_data, 1, append_md);
      if (! RCD_IS_OK(retU)) return retU;

      shift ++ ;
      if (shift > 7) {
         offs  ++ ;
         shift &= 0x7;

         if (offs == 11) {
            shift = 4; //last 4 bits in byte 11
         }
      }
   }

   //append "res11_03" for byte 11
   offs          = 11;
   vidfmt        = ET3_mode[itf];
   fld.handlerfn = &EDID_cl::BitF8Val;
   fld.offs      = offs;
   fld.shift     = 0;
   fld.fld_sz    = 4;
   fld.flags     = F_BFD|F_RD;
   fld.name      = vidfmt;
   fld.maxv      = 0xF;
   fld.desc      = resvd;

   retU = init_fields(&fld, inst_data, 1, append_md);
   if (! RCD_IS_OK(retU)) return retU;

   //append unused bytes @ offset 12..17
   dyn_fcnt  = n_unused;
   dyn_fldar = (edi_field_t*) malloc( n_unused * EDI_FIELD_SZ );
   if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);

   insert_unk_byte(dyn_fldar, n_unused, 12);

   retU = init_fields(&dyn_fldar[0], &inst_data[12], n_unused, append_md);
   if (! RCD_IS_OK(retU)) return retU;

   RCD_RETURN_OK(retU);
}


//UNK : Unknown Descriptor (type != 0xFA..0xFF)
//VOID: Placeholder Descriptor (type 0x10)
const char  unk_cl::CodN_UNK [] = "UNK";
const char  unk_cl::Name_UNK [] = "Unknown Descriptor";
const char  unk_cl::CodN_VOID[] = "VOID";
const char  unk_cl::Name_VOID[] = "Unused Data";
const char *unk_cl::Desc        = AltDesc;

#pragma GCC diagnostic ignored "-Wunused-parameter"
rcode unk_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   enum {
      unk_fcnt = 13
   };

   const char *CodN;
   const char *Name;

   rcode  retU;
   u8_t   dtype;
   bool   append_md = true;

   dtype = reinterpret_cast<const dshd_t*> (inst)->dsc_type;
   if (0x10 != dtype) {
      CodN = CodN_UNK;
      Name = Name_UNK;
   } else {
      CodN = CodN_VOID;
      Name = Name_VOID;
   }

   type_id.t32 = ID_UNK;

   CopyInstData(inst, sizeof(unk_t));

   retU = init_fields(&AltDescHdr[0], inst_data, 3, false, Name, Desc, CodN);
   if (! RCD_IS_OK(retU)) return retU;

   //pre-alloc buffer for array of fields:
   dyn_fcnt  = unk_fcnt;
   dyn_fldar = (edi_field_t*) malloc( unk_fcnt * EDI_FIELD_SZ );
   if (NULL == dyn_fldar) RCD_RETURN_FAULT(retU);

   //descriptor data interpreted as unknown
   insert_unk_byte(dyn_fldar, unk_fcnt, offsetof(unk_t, unkb));

   retU = init_fields(&dyn_fldar[0], inst_data, unk_fcnt, append_md);
   return retU;
}
#pragma GCC diagnostic warning "-Wunused-parameter"

rcode edi_adsc_cl::base_AltDesc_Init_RootGrp(const u8_t* inst, const edi_rootgp_dsc_t *pGDsc, u32_t orflags, edi_grp_cl* parent) {
   rcode        retU, retU2;
   u32_t        dlen;
   u32_t        offs;
   u32_t        gr_idx;
   u32_t        gr_inst;
   gtid_t       sub_id;
   edi_grp_cl  *pgrp;
   const u8_t  *pgrp_inst;

   RCD_SET_OK(retU);
   RCD_SET_OK(retU2);

   dat_sz  = sizeof(unk_t );  //18 bytes for all descriptors

   CopyInstData(inst, dat_sz);

   hdr_sz  = sizeof(dshd_t);  //Alt. Desc header
   hdr_sz += pGDsc->grp_offs; //offset for addtional hdr fields
   dlen    = dat_sz;
   dlen   -= hdr_sz;

   parent_grp  = parent;
   type_id.t32 = pGDsc->type_id;
   pgrp_inst   = inst;
   offs        = hdr_sz;
   pgrp_inst  += offs;

   sub_id      = type_id;
   sub_id.t32 |= orflags;
   sub_id.t_md_edit = 0;

   for (gr_idx=0; gr_idx<pGDsc->grp_arsz; ++gr_idx) {
      const edi_subg_dsc_t *pSubGDsc;
      u32_t                 dsz;

      pSubGDsc = &pGDsc->grp_ar[gr_idx];

      for (gr_inst=0; gr_inst<pSubGDsc->inst_cnt; ++gr_inst) {

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
         subgroups.Append(pgrp);

         dsz        = pgrp->getTotalSize();
         offs      += dsz;
         pgrp_inst += dsz;
         dlen      -= dsz;
      }
   }

payload:
   //insert padding bytes:
   if (dlen > 0) {
      retU = Append_UNK_DAT(pgrp_inst, dlen, type_id.t32, (offs + abs_offs), offs, this);
      if (! RCD_IS_OK(retU)) return retU;

      if (RCD_IS_OK(retU2)) {
         //UNK-DAT group: change description and type ID
         gtid_t tid;
         gr_idx  = subgroups.GetCount();
         gr_idx -= 1;
         tid.t32 = T_GRP_FIXED;
         pgrp    = subgroups.Item(gr_idx);
         pgrp->setTypeID(tid);
         pgrp->CodeName  = "PAD";
         pgrp->GroupName = "Padding bytes";
         pgrp->GroupDesc = pGDsc->PadDsc;
      }
   }

   //Alt. Descriptor header fields
   retU = init_fields(&AltDescHdr[0], inst_data, 3, false,
                      pGDsc->Name, pGDsc->Desc, pGDsc->CodN);
   if (! RCD_IS_OK(retU)) return retU;

   if (! RCD_IS_OK(retU2)) return retU2;
   return retU;
}


