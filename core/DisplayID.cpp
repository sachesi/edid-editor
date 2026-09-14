/***************************************************************
 * Name:      DisplayID.cpp
 * Purpose:   DisplayID extension parsing and editable groups
 * License:   GPLv3+
 **************************************************************/

#include "debug.h"
#include "rcdunits.h"
#define RCD_UNIT idEDID_MAIN
#include "rcode/rcode.h"

#include "wxedid_rcd_scope.h"

RCD_AUTOGEN_DEFINE_UNIT

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "DisplayID_class.h"

const char* displayid_data_block_name(u8_t version, u8_t tag) {
   if (version < 0x20) {
      static const char* names[] = {
         "Product Identification", "Display Parameters", "Color Characteristics",
         "Type I Detailed Timings", "Type II Detailed Timings", "Type III Short Timings",
         "Type IV DMT Timings", "Supported DMT Timings", "Supported CTA Timings",
         "Video Timing Range", "Product Serial Number", "General-purpose ASCII String",
         "Display Device Data", "Interface Power Sequencing", "Transfer Characteristics",
         "Display Interface", "Stereo Display Interface", "Type V Short Timings",
         "Tiled Display Topology", "Type VI Detailed Timings"
      };
      if (tag < (sizeof(names) / sizeof(names[0]))) return names[tag];
      if (tag == 0x7f) return "Vendor-specific Data";
   } else {
      static const char* names[] = {
         "Product Identification", "Display Parameters", "Type VII Detailed Timings",
         "Type VIII Enumerated Timings", "Type IX Formula-based Timings",
         "Dynamic Video Timing Range", "Display Interface Features",
         "Stereo Display Interface", "Tiled Display Topology", "Container ID",
         "Type X Formula-based Timings", "Adaptive Sync", "AR/VR Head-mounted Display",
         "AR/VR Layer", "Brightness Luminance Range"
      };
      if ((tag >= 0x20) && (tag <= 0x2e)) return names[tag - 0x20];
      if (tag == 0x7e) return "Vendor-specific Data";
   }
   if (tag == 0x81) return "CTA Data Block";
   return "Reserved or unknown Data Block";
}

const edi_field_t displayid_hdr_cl::fields[] = {
   {&EDID_cl::ByteVal, 0, 0, 0, 1, F_BTE|F_HEX|F_RD, 0, 0xff,
    "Extension tag", "DisplayID extension tag (0x70)"},
   {&EDID_cl::BitF8Val, 0, 1, 4, 4, F_BFD|F_INT|F_RD, 0, 0x0f,
    "Version", "DisplayID major version"},
   {&EDID_cl::BitF8Val, 0, 1, 0, 4, F_BFD|F_INT|F_RD, 0, 0x0f,
    "Revision", "DisplayID revision"},
   {&EDID_cl::ByteVal, 0, 2, 0, 1, F_BTE|F_INT|F_RD, 0, 121,
    "Payload length", "Bytes occupied by DisplayID data blocks"},
   {&EDID_cl::ByteVal, 0, 3, 0, 1, F_BTE|F_HEX, 0, 0xff,
    "Product type", "Display product type"},
   {&EDID_cl::ByteVal, 0, 4, 0, 1, F_BTE|F_INT|F_RD, 0, 0xff,
    "Extension count", "Additional DisplayID sections"},
   {&EDID_cl::ByteVal, 0, 5, 0, 1, F_BTE|F_HEX|F_RD, 0, 0xff,
    "DisplayID checksum", "Checksum of the DisplayID structure"},
   {&EDID_cl::ByteVal, 0, 6, 0, 1, F_BTE|F_HEX|F_RD, 0, 0xff,
    "EDID checksum", "Checksum of the 128-byte extension block"}
};

rcode displayid_hdr_cl::init(const u8_t* inst, u32_t /*orflags*/,
                             edi_grp_cl* parent) {
   rcode retU;
   if ((inst[0] != 0x70) || (inst[2] > 121)) RCD_RETURN_FAULT(retU);

   parent_grp = parent;
   type_id.t32 = ID_DISPLAYID | T_GRP_FIXED;
   memcpy(inst_data, inst, 5);
   hdr_sz = 5;
   inst_data[5] = inst[5 + inst[2]];
   inst_data[6] = inst[127];
   dat_sz = 7;

   return init_fields(fields, inst_data, sizeof(fields) / sizeof(fields[0]),
                      false, "DisplayID header", "DisplayID extension header", "DID-HDR");
}

void displayid_hdr_cl::SpawnInstance(u8_t* pinst) {
   memcpy(pinst, inst_data, 5);
   pinst[5 + inst_data[2]] = inst_data[5];
   pinst[127] = inst_data[6];
}

const edi_field_t displayid_data_block_cl::fields[] = {
   {&EDID_cl::ByteVal, 0, 0, 0, 1, F_BTE|F_HEX|F_RD, 0, 0xff,
    "Tag", "DisplayID data block tag"},
   {&EDID_cl::BitF8Val, 0, 1, 0, 3, F_BFD|F_INT, 0, 7,
    "Revision", "Data block revision"},
   {&EDID_cl::BitF8Val, 0, 1, 3, 5, F_BFD|F_HEX, 0, 0x1f,
    "Flags", "Data block flags"},
   {&EDID_cl::ByteVal, 0, 2, 0, 1, F_BTE|F_INT|F_RD, 0, 121,
    "Payload length", "Bytes following the data block header"}
};

rcode displayid_data_block_cl::init(const u8_t* inst, u32_t orflags,
                                    edi_grp_cl* parent) {
   rcode retU;
   u32_t payload_len = inst[2];
   if (payload_len > 121) RCD_RETURN_FAULT(retU);

   parent_grp = parent;
   version = orflags & 0xff;
   type_id.t32 = ID_DISPLAYID_DB | (orflags & T_MODE_EDIT);
   CopyInstData(inst, 3);
   hdr_sz = 3;
   dat_sz = 3 + payload_len;

   char name[128];
   snprintf(name, sizeof(name), "%s (0x%02X)",
            displayid_data_block_name(orflags & 0xff, inst[0]), inst[0]);
   retU = init_fields(fields, inst_data, sizeof(fields) / sizeof(fields[0]),
                      false, name, "DisplayID data block", "DID-DB");
   if (! RCD_IS_OK(retU)) return retU;

   const u8_t* payload = inst + 3;
   u32_t offset = 3;
   u32_t remaining = payload_len;

   if (((orflags & 0xff) < 0x20) && (inst[0] == 0x03)) {
      while (remaining >= 20) {
         displayid_type1_timing_cl* timing = new displayid_type1_timing_cl;
         if (timing == NULL) RCD_RETURN_FAULT(retU);
         retU = timing->init(payload, T_SUB_GRP|T_NO_MOVE, this);
         if (! RCD_IS_OK(retU)) {
            delete timing;
            return retU;
         }
         timing->setRelOffs(offset);
         timing->setAbsOffs(abs_offs + offset);
         subgroups.Append(timing);
         payload += 20;
         offset += 20;
         remaining -= 20;
      }
   }

   if (remaining > 0) {
      displayid_raw_payload_cl* raw = new displayid_raw_payload_cl;
      if (raw == NULL) RCD_RETURN_FAULT(retU);
      raw->setDataSize(remaining);
      retU = raw->init(payload, T_SUB_GRP|T_NO_MOVE, this);
      if (! RCD_IS_OK(retU)) {
         delete raw;
         return retU;
      }
      raw->setRelOffs(offset);
      raw->setAbsOffs(abs_offs + offset);
      subgroups.Append(raw);
   }

   return retU;
}

rcode displayid_padding_cl::init(const u8_t* inst, u32_t orflags,
                                 edi_grp_cl* parent) {
   rcode retU;
   if ((dat_sz == 0) || (dat_sz > 121)) RCD_RETURN_FAULT(retU);

   parent_grp = parent;
   type_id.t32 = ID_DISPLAYID_PADDING | T_GRP_FIXED |
                 (orflags & T_MODE_EDIT);
   CopyInstData(inst, dat_sz);
   GroupName = "Padding";
   GroupDesc = "Zero padding at the end of the DisplayID payload";
   CodeName = "DID-PAD";
   RCD_RETURN_OK(retU);
}

const edi_field_t displayid_type1_timing_cl::fields[] = {
   {&EDID_cl::DisplayID_PixelClock, 0, 0, 0, 3, F_FLT|F_MHZ|F_DN, 0, 0xffffff,
    "Pixel clock", "Pixel clock in 10 kHz units"},
   {&EDID_cl::BitF8Val, 0, 3, 0, 4, F_BFD|F_INT, 0, 15,
    "Aspect ratio", "Aspect-ratio code"},
   {&EDID_cl::BitVal, 0, 3, 4, 1, F_BIT|F_INT, 0, 1,
    "Interlaced", "Interlaced timing"},
   {&EDID_cl::BitF8Val, 0, 3, 5, 2, F_BFD|F_INT, 0, 3,
    "Stereo support", "Stereo viewing support"},
   {&EDID_cl::BitVal, 0, 3, 7, 1, F_BIT|F_INT, 0, 1,
    "Preferred", "Preferred timing"},
   {&EDID_cl::DisplayID_ValuePlusOne16, 0, 4, 0, 2, F_INT|F_PIX|F_DN, 1, 65536,
    "Horizontal active", "Horizontal active pixels"},
   {&EDID_cl::DisplayID_ValuePlusOne16, 0, 6, 0, 2, F_INT|F_PIX, 1, 65536,
    "Horizontal blanking", "Horizontal blanking pixels"},
   {&EDID_cl::DisplayID_ValuePlusOne15, 0, 8, 0, 2, F_INT|F_PIX, 1, 32768,
    "Horizontal front porch", "Horizontal sync offset"},
   {&EDID_cl::DisplayID_ValuePlusOne16, 0, 10, 0, 2, F_INT|F_PIX, 1, 65536,
    "Horizontal sync width", "Horizontal sync width"},
   {&EDID_cl::BitVal, 0, 9, 7, 1, F_BIT|F_INT, 0, 1,
    "Horizontal sync positive", "Horizontal sync polarity"},
   {&EDID_cl::DisplayID_ValuePlusOne16, 0, 12, 0, 2, F_INT|F_PIX|F_DN, 1, 65536,
    "Vertical active", "Vertical active lines"},
   {&EDID_cl::DisplayID_ValuePlusOne16, 0, 14, 0, 2, F_INT|F_PIX, 1, 65536,
    "Vertical blanking", "Vertical blanking lines"},
   {&EDID_cl::DisplayID_ValuePlusOne15, 0, 16, 0, 2, F_INT|F_PIX, 1, 32768,
    "Vertical front porch", "Vertical sync offset"},
   {&EDID_cl::DisplayID_ValuePlusOne16, 0, 18, 0, 2, F_INT|F_PIX, 1, 65536,
    "Vertical sync width", "Vertical sync width"},
   {&EDID_cl::BitVal, 0, 17, 7, 1, F_BIT|F_INT, 0, 1,
    "Vertical sync positive", "Vertical sync polarity"}
};

rcode displayid_type1_timing_cl::init(const u8_t* inst, u32_t orflags,
                                      edi_grp_cl* parent) {
   parent_grp = parent;
   type_id.t32 = ID_DISPLAYID_TYPE1 | T_SUB_GRP | T_NO_MOVE |
                 (orflags & T_MODE_EDIT);
   CopyInstData(inst, 20);
   return init_fields(fields, inst_data, sizeof(fields) / sizeof(fields[0]),
                      false, "Type I Detailed Timing", "DisplayID Type I timing", "DID-T1");
}

void displayid_type1_timing_cl::getGrpName(EDID_cl& /*EDID*/, wxc_String& gp_name) {
   u32_t pixel_clock_khz =
      (1 + inst_data[0] + (inst_data[1] << 8) + (inst_data[2] << 16)) * 10;
   u32_t hactive = 1 + inst_data[4] + (inst_data[5] << 8);
   u32_t hblank = 1 + inst_data[6] + (inst_data[7] << 8);
   u32_t vactive = 1 + inst_data[12] + (inst_data[13] << 8);
   u32_t vblank = 1 + inst_data[14] + (inst_data[15] << 8);
   double refresh = (pixel_clock_khz * 1000.0) /
                    ((hactive + hblank) * (vactive + vblank));
   gp_name.Printf("%ux%u @ %.2f Hz", hactive, vactive, refresh);
}

rcode displayid_raw_payload_cl::init(const u8_t* inst, u32_t orflags,
                                     edi_grp_cl* parent) {
   rcode retU;
   u32_t dlen = dat_sz;
   if ((dlen == 0) || (dlen > 121)) RCD_RETURN_FAULT(retU);

   parent_grp = parent;
   type_id.t32 = ID_DISPLAYID_RAW | T_SUB_GRP | T_NO_MOVE |
                 (orflags & T_MODE_EDIT);
   CopyInstData(inst, dlen);

   dyn_fldar = (edi_field_t*) malloc(dlen * EDI_FIELD_SZ);
   if (dyn_fldar == NULL) RCD_RETURN_FAULT(retU);
   dyn_fcnt = dlen;

   for (u32_t idx=0; idx<dlen; idx++) {
      snprintf(field_names[idx], sizeof(field_names[idx]), "Payload byte %u", idx);
      dyn_fldar[idx] = {
         &EDID_cl::ByteVal, 0, idx, 0, 1, F_BTE|F_HEX, 0, 0xff,
         field_names[idx], "Raw DisplayID payload byte"
      };
   }

   return init_fields(dyn_fldar, inst_data, dyn_fcnt, false,
                      "Raw payload", "Editable DisplayID payload bytes", "DID-RAW");
}

rcode EDID_cl::DisplayID_PixelClock(u32_t op, wxc_String& sval, u32_t& ival,
                                    edi_dynfld_t* p_field) {
   rcode retU;
   u8_t* inst = getValPtr(p_field);
   if (op == OP_READ) {
      u32_t raw = rdWord24_LE(inst);
      ival = (raw + 1) * 10;
      sval.Printf("%.02f", ival / 1000.0);
      RCD_RETURN_OK(retU);
   }

   u32_t raw;
   if (op == OP_WRSTR) {
      float mhz;
      retU = getStrFloat(sval, 0.01, 167772.16, mhz);
      if (! RCD_IS_OK(retU)) return retU;
      raw = (u32_t) std::lround(mhz * 100.0) - 1;
   } else if (op == OP_WRINT) {
      if ((ival < 10) || (ival > 167772160)) RCD_RETURN_FAULT(retU);
      raw = (ival / 10) - 1;
   } else {
      RCD_RETURN_FAULT(retU);
   }
   wrWord24_LE(inst, raw);
   RCD_RETURN_OK(retU);
}

rcode EDID_cl::DisplayID_ValuePlusOne16(u32_t op, wxc_String& sval, u32_t& ival,
                                        edi_dynfld_t* p_field) {
   rcode retU;
   u8_t* inst = getValPtr(p_field);
   if (op == OP_READ) {
      ival = (u32_t) rdWord16_LE(inst) + 1;
      sval.Empty();
      sval << ival;
      RCD_RETURN_OK(retU);
   }

   ulong value = ival;
   if (op == OP_WRSTR) {
      retU = getStrUint(sval, 10, 1, 65536, value);
      if (! RCD_IS_OK(retU)) return retU;
   } else if ((op != OP_WRINT) || (ival < 1) || (ival > 65536)) {
      RCD_RETURN_FAULT(retU);
   }
   wrWord16_LE(inst, (u16_t) (value - 1));
   RCD_RETURN_OK(retU);
}

rcode EDID_cl::DisplayID_ValuePlusOne15(u32_t op, wxc_String& sval, u32_t& ival,
                                        edi_dynfld_t* p_field) {
   rcode retU;
   u8_t* inst = getValPtr(p_field);
   u16_t word = rdWord16_LE(inst);
   if (op == OP_READ) {
      ival = (word & 0x7fff) + 1;
      sval.Empty();
      sval << ival;
      RCD_RETURN_OK(retU);
   }

   ulong value = ival;
   if (op == OP_WRSTR) {
      retU = getStrUint(sval, 10, 1, 32768, value);
      if (! RCD_IS_OK(retU)) return retU;
   } else if ((op != OP_WRINT) || (ival < 1) || (ival > 32768)) {
      RCD_RETURN_FAULT(retU);
   }
   word = (word & 0x8000) | ((u16_t) value - 1);
   wrWord16_LE(inst, word);
   RCD_RETURN_OK(retU);
}
