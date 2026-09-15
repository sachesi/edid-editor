/***************************************************************
 * Name:      DisplayID.cpp
 * Purpose:   DisplayID extension parsing and editable groups
 * Copyright: sachesi (C) 2026
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
#include "vmap.h"

sm_vmap DID_stereo_map = {
   {0, {0, "no stereo"           , NULL}},
   {1, {0, "stereo"              , NULL}},
   {2, {0, "stereo on user action", NULL}}
};

sm_vmap DID_adaptive_map = {
   {0, {0, "Fixed average V-total"                , NULL}},
   {1, {0, "Fixed average and adaptive V-total"   , NULL}},
   {2, {0, "Reserved"                             , NULL}},
   {3, {0, "Reserved"                             , NULL}}
};

sm_vmap DID_product_map = {
   {0, {0, "Extension section"                   , NULL}},
   {1, {0, "Test structure"                      , NULL}},
   {2, {0, "Display panel or other transducer"   , NULL}},
   {3, {0, "Standalone display device"           , NULL}},
   {4, {0, "Television receiver"                 , NULL}},
   {5, {0, "Repeater/translator"                 , NULL}},
   {6, {0, "Direct drive monitor"                , NULL}}
};

sm_vmap DID2_product_map = {
   {0, {0, "Same use case as the base section"   , NULL}},
   {1, {0, "Test structure"                      , NULL}},
   {2, {0, "Generic display"                     , NULL}},
   {3, {0, "Television"                          , NULL}},
   {4, {0, "Desktop productivity display"        , NULL}},
   {5, {0, "Desktop gaming display"              , NULL}},
   {6, {0, "Presentation display"                , NULL}},
   {7, {0, "Head-mounted VR display"             , NULL}},
   {8, {0, "Head-mounted AR display"             , NULL}}
};

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
   {&EDID_cl::ByteVal, 0, 4, 0, 1, F_BTE|F_INT|F_RD, 0, 0xff,
    "Extension count", "Additional DisplayID sections"},
   {&EDID_cl::ByteVal, 0, 5, 0, 1, F_BTE|F_HEX|F_RD, 0, 0xff,
    "DisplayID checksum", "Checksum of the DisplayID structure"},
   {&EDID_cl::ByteVal, 0, 6, 0, 1, F_BTE|F_HEX|F_RD, 0, 0xff,
    "EDID checksum", "Checksum of the 128-byte extension block"}
};

//the product type's meaning depends on the DisplayID version
static const edi_field_t displayid_product_fld[] = {
   {&EDID_cl::ByteVal, VS_DID_PRODUCT, 3, 0, 1, F_BTE|F_HEX|F_VS, 0, 0xff,
    "Product type", "Display product type"},
   {&EDID_cl::ByteVal, VS_DID2_PRODUCT, 3, 0, 1, F_BTE|F_HEX|F_VS, 0, 0xff,
    "Product use case", "Display product primary use case"}
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

   retU = init_fields(fields, inst_data, 4, false, "DisplayID header",
                      "DisplayID extension header", "DID-HDR");
   if (! RCD_IS_OK(retU)) return retU;
   retU = init_fields(&displayid_product_fld[(inst[1] >= 0x20) ? 1 : 0], inst_data, 1, true);
   if (! RCD_IS_OK(retU)) return retU;
   return init_fields(&fields[4], inst_data, (sizeof(fields) / sizeof(fields[0])) - 4, true);
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

   u32_t version = orflags & 0xff;
   if ((version >= 0x20) && (inst[0] == 0x22)) {
      u32_t size = 20 + ((inst[1] & 0x70) >> 4);
      while (remaining >= size) {
         displayid_type7_timing_cl* timing = new displayid_type7_timing_cl;
         timing->setDataSize(size);
         retU = timing->init(payload, T_SUB_GRP|T_NO_MOVE, this);
         if (! RCD_IS_OK(retU)) {
            delete timing;
            return retU;
         }
         timing->setRelOffs(offset);
         timing->setAbsOffs(abs_offs + offset);
         subgroups.Append(timing);
         payload += size;
         offset += size;
         remaining -= size;
      }
   } else if ((version >= 0x20) && (inst[0] == 0x25) && (remaining == 9)) {
      displayid_range_cl* range = new displayid_range_cl;
      retU = range->init(payload, T_SUB_GRP|T_NO_MOVE, this);
      if (! RCD_IS_OK(retU)) {
         delete range;
         return retU;
      }
      range->setRelOffs(offset);
      range->setAbsOffs(abs_offs + offset);
      subgroups.Append(range);
      payload += 9;
      offset += 9;
      remaining = 0;
   } else if ((version >= 0x20) && (inst[0] == 0x2b)) {
      u32_t size = 6 + ((inst[1] >> 4) & 7);
      while (remaining >= size) {
         displayid_adaptive_sync_cl* range = new displayid_adaptive_sync_cl;
         range->setDataSize(size);
         retU = range->init(payload, T_SUB_GRP|T_NO_MOVE, this);
         if (! RCD_IS_OK(retU)) {
            delete range;
            return retU;
         }
         range->setRelOffs(offset);
         range->setAbsOffs(abs_offs + offset);
         subgroups.Append(range);
         payload += size;
         offset += size;
         remaining -= size;
      }
   } else if (inst[0] == 0x81) {
      //CTA-861 data blocks; blocks with sub-groups of their own stay raw
      while (remaining > 0) {
         u32_t size = 1 + (payload[0] & 0x1f);
         if (size > remaining) break;
         edi_grp_cl* cta = NULL;
         EDID_cl::ParseDBC_TAG(const_cast<u8_t*>(payload), &cta);
         if (cta != NULL) {
            rcode result = cta->init(payload, orflags & T_MODE_EDIT, this);
            if ((! RCD_IS_OK(result) && (result.detail.rcode > RCD_FVMSG)) ||
                (cta->getSubGrpCount() != 0) || (cta->getTotalSize() != size)) {
               delete cta;
               cta = NULL;
            }
         }
         edi_grp_cl* part = cta;
         if (part != NULL) {
            gtid_t type = part->getTypeID();
            type.t32 |= T_SUB_GRP | T_NO_MOVE | T_GRP_FIXED;
            part->setTypeID(type);
         } else {
            displayid_raw_payload_cl* raw = new displayid_raw_payload_cl;
            raw->setDataSize(size);
            retU = raw->init(payload, T_SUB_GRP|T_NO_MOVE, this);
            if (! RCD_IS_OK(retU)) {
               delete raw;
               return retU;
            }
            part = raw;
         }
         part->setRelOffs(offset);
         part->setAbsOffs(abs_offs + offset);
         subgroups.Append(part);
         payload += size;
         offset += size;
         remaining -= size;
      }
   } else if ((version < 0x20) && (inst[0] == 0x03)) {
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
   {&EDID_cl::BitF8Val, VS_T7_ASP_RATIO, 3, 0, 4, F_BFD|F_INT|F_VS, 0, 15,
    "Aspect ratio", "Aspect-ratio code"},
   {&EDID_cl::BitVal, 0, 3, 4, 1, F_BIT|F_INT, 0, 1,
    "Interlaced", "Interlaced timing"},
   {&EDID_cl::BitF8Val, VS_DID_STEREO, 3, 5, 2, F_BFD|F_INT|F_VS, 0, 3,
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

const edi_field_t displayid_type7_timing_cl::fields[] = {
   {&EDID_cl::DisplayID_PixelClockKHz, 0, 0, 0, 3, F_FLT|F_MHZ|F_DN, 0, 0xffffff,
    "Pixel clock", "Pixel clock in 1 kHz units"},
   {&EDID_cl::BitF8Val, VS_T7_ASP_RATIO, 3, 0, 4, F_BFD|F_INT|F_VS, 0, 15,
    "Aspect ratio", "Aspect-ratio code"},
   {&EDID_cl::BitVal, 0, 3, 4, 1, F_BIT|F_INT, 0, 1,
    "Interlaced", "Interlaced timing"},
   {&EDID_cl::BitF8Val, VS_DID_STEREO, 3, 5, 2, F_BFD|F_INT|F_VS, 0, 3,
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

rcode displayid_type7_timing_cl::init(const u8_t* inst, u32_t orflags,
                                      edi_grp_cl* parent) {
   rcode retU;
   if ((dat_sz < 20) || (dat_sz > 27)) RCD_RETURN_FAULT(retU);
   parent_grp = parent;
   type_id.t32 = ID_DISPLAYID_TYPE7 | T_SUB_GRP | T_NO_MOVE |
                 (orflags & T_MODE_EDIT);
   CopyInstData(inst, dat_sz);

   //from data block revision 2, bit 7 flags YCbCr 4:2:0 instead
   memcpy(dyn_fields, fields, sizeof(dyn_fields));
   u32_t block_revision = (parent != NULL) ? (parent->getInstPtr()[1] & 7) : 0;
   if (block_revision >= 2) {
      dyn_fields[4].name = "YCbCr 4:2:0";
      dyn_fields[4].desc = "Timing supports YCbCr 4:2:0";
   }
   return init_fields(dyn_fields, inst_data, sizeof(fields) / sizeof(fields[0]),
                      false, "Type VII Detailed Timing", "DisplayID Type VII timing",
                      "DID-T7");
}

void displayid_type7_timing_cl::getGrpName(EDID_cl& /*EDID*/, wxc_String& gp_name) {
   u32_t pixel_clock_khz = 1 + inst_data[0] + (inst_data[1] << 8) + (inst_data[2] << 16);
   u32_t hactive = 1 + inst_data[4] + (inst_data[5] << 8);
   u32_t hblank = 1 + inst_data[6] + (inst_data[7] << 8);
   u32_t vactive = 1 + inst_data[12] + (inst_data[13] << 8);
   u32_t vblank = 1 + inst_data[14] + (inst_data[15] << 8);
   double refresh = (pixel_clock_khz * 1000.0) /
                    ((double) (hactive + hblank) * (vactive + vblank));
   gp_name.Printf("%ux%u @ %.2f Hz", hactive, vactive, refresh);
}

const edi_field_t displayid_range_cl::fields[] = {
   {&EDID_cl::DisplayID_PixelClockKHz, 0, 0, 0, 3, F_FLT|F_MHZ, 0, 0xffffff,
    "Minimum pixel clock", "Minimum pixel clock in 1 kHz units"},
   {&EDID_cl::DisplayID_PixelClockKHz, 0, 3, 0, 3, F_FLT|F_MHZ, 0, 0xffffff,
    "Maximum pixel clock", "Maximum pixel clock in 1 kHz units"},
   {&EDID_cl::ByteVal, 0, 6, 0, 1, F_BTE|F_INT|F_HZ, 0, 255,
    "Minimum refresh", "Minimum vertical refresh rate"},
   {&EDID_cl::DisplayID_MaxRefresh, 0, 7, 0, 2, F_INT|F_HZ, 0, 1023,
    "Maximum refresh", "Maximum vertical refresh rate"},
   {&EDID_cl::BitVal, 0, 8, 7, 1, F_BIT|F_INT, 0, 1,
    "Seamless timing change", "Supports seamless dynamic video timing changes"}
};

rcode displayid_range_cl::init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent) {
   parent_grp = parent;
   type_id.t32 = ID_DISPLAYID_RANGE | T_SUB_GRP | T_NO_MOVE | (orflags & T_MODE_EDIT);
   CopyInstData(inst, 9);
   dat_sz = 9;

   //revision 0 has an 8-bit maximum refresh rate
   memcpy(dyn_fields, fields, sizeof(dyn_fields));
   u32_t block_revision = (parent != NULL) ? (parent->getInstPtr()[1] & 7) : 0;
   if (block_revision == 0) {
      dyn_fields[3].handlerfn = &EDID_cl::ByteVal;
      dyn_fields[3].fld_sz = 1;
      dyn_fields[3].flags = F_BTE|F_INT|F_HZ;
      dyn_fields[3].maxv = 255;
   }
   return init_fields(dyn_fields, inst_data, sizeof(fields) / sizeof(fields[0]),
                      false, "Dynamic Video Timing Range", "DisplayID timing range limits",
                      "DID-RANGE");
}

//Adaptive Sync descriptors, laid out as edid-decode reads them
const edi_field_t displayid_adaptive_sync_cl::fields[] = {
   {&EDID_cl::BitVal, 0, 0, 0, 1, F_BIT|F_INT, 0, 1,
    "Native panel range", "The range is native to the panel"},
   {&EDID_cl::BitVal, 0, 0, 1, 1, F_BIT|F_INT, 0, 1,
    "Increase without jitter",
    "A frame can be lengthened by the maximum duration increase without jitter"},
   {&EDID_cl::BitF8Val, VS_DID_ADAPTIVE, 0, 2, 2, F_BFD|F_INT|F_VS, 0, 3,
    "Refresh type", "How the refresh rate varies within the range"},
   {&EDID_cl::BitVal, 0, 0, 4, 1, F_BIT|F_INT, 0, 1,
    "No seamless transition",
    "Changing between refresh rates in the range is not seamless"},
   {&EDID_cl::BitVal, 0, 0, 5, 1, F_BIT|F_INT, 0, 1,
    "Decrease without jitter",
    "A frame can be shortened by the maximum duration decrease without jitter"},
   {&EDID_cl::DisplayID_QuarterMs, 0, 1, 0, 1, F_FLT|F_MLS, 0, 255,
    "Maximum duration increase", "Longest single frame lengthening, in 1/4 ms units"},
   {&EDID_cl::ByteVal, 0, 2, 0, 1, F_BTE|F_INT|F_HZ|F_DN, 0, 255,
    "Minimum refresh", "Minimum refresh rate"},
   {&EDID_cl::DisplayID_MaxRefreshPlusOne, 0, 3, 0, 2, F_INT|F_HZ|F_DN, 1, 1024,
    "Maximum refresh", "Maximum refresh rate: 1 more than the stored 10-bit value"},
   {&EDID_cl::DisplayID_QuarterMs, 0, 5, 0, 1, F_FLT|F_MLS, 0, 255,
    "Maximum duration decrease", "Longest single frame shortening, in 1/4 ms units"}
};

rcode displayid_adaptive_sync_cl::init(const u8_t* inst, u32_t orflags,
                                       edi_grp_cl* parent) {
   rcode retU;
   if ((dat_sz < 6) || (dat_sz > 13)) RCD_RETURN_FAULT(retU);
   parent_grp = parent;
   type_id.t32 = ID_DISPLAYID_ADAPTIVE | T_SUB_GRP | T_NO_MOVE | (orflags & T_MODE_EDIT);
   CopyInstData(inst, dat_sz);
   return init_fields(fields, inst_data, sizeof(fields) / sizeof(fields[0]),
                      false, "Adaptive Sync Range", "DisplayID Adaptive Sync descriptor",
                      "DID-AS");
}

void displayid_adaptive_sync_cl::getGrpName(EDID_cl& /*EDID*/, wxc_String& gp_name) {
   u32_t maximum = 1 + inst_data[3] + ((inst_data[4] & 0x03) << 8);
   gp_name.Printf("%u–%u Hz%s", inst_data[2], maximum,
                  (inst_data[0] & 0x01) ? ", native" : "");
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

rcode EDID_cl::DisplayID_PixelClockKHz(u32_t op, wxc_String& sval, u32_t& ival,
                                       edi_dynfld_t* p_field) {
   rcode retU;
   u8_t* inst = getValPtr(p_field);
   if (op == OP_READ) {
      ival = rdWord24_LE(inst) + 1;
      sval.Printf("%.03f", ival / 1000.0);
      RCD_RETURN_OK(retU);
   }

   u32_t khz;
   if (op == OP_WRSTR) {
      float mhz;
      retU = getStrFloat(sval, 0.001, 16777.216, mhz);
      if (! RCD_IS_OK(retU)) return retU;
      khz = (u32_t) std::lround(mhz * 1000.0);
   } else if (op == OP_WRINT) {
      khz = ival;
   } else {
      RCD_RETURN_FAULT(retU);
   }
   if ((khz < 1) || (khz > 16777216)) RCD_RETURN_FAULT(retU);
   wrWord24_LE(inst, khz - 1);
   RCD_RETURN_OK(retU);
}

//10-bit rate: low byte, then bits 0-1 of the next byte
rcode EDID_cl::DisplayID_MaxRefresh(u32_t op, wxc_String& sval, u32_t& ival,
                                    edi_dynfld_t* p_field) {
   rcode retU;
   u8_t* inst = getValPtr(p_field);
   if (op == OP_READ) {
      ival = inst[0] | ((inst[1] & 0x03) << 8);
      sval.Empty();
      sval << ival;
      RCD_RETURN_OK(retU);
   }

   ulong value = ival;
   if (op == OP_WRSTR) {
      retU = getStrUint(sval, 10, 0, 1023, value);
      if (! RCD_IS_OK(retU)) return retU;
   } else if ((op != OP_WRINT) || (ival > 1023)) {
      RCD_RETURN_FAULT(retU);
   }
   inst[0] = value & 0xff;
   inst[1] = (inst[1] & 0xfc) | ((value >> 8) & 0x03);
   RCD_RETURN_OK(retU);
}

//Adaptive Sync: 10 bits, low byte first, holding the rate minus one
rcode EDID_cl::DisplayID_MaxRefreshPlusOne(u32_t op, wxc_String& sval, u32_t& ival,
                                           edi_dynfld_t* p_field) {
   rcode retU;
   u8_t* inst = getValPtr(p_field);
   if (op == OP_READ) {
      ival = 1 + (inst[0] | ((inst[1] & 0x03) << 8));
      sval.Empty();
      sval << ival;
      RCD_RETURN_OK(retU);
   }

   ulong value = ival;
   if (op == OP_WRSTR) {
      retU = getStrUint(sval, 10, 1, 1024, value);
      if (! RCD_IS_OK(retU)) return retU;
   } else if ((op != OP_WRINT) || (ival < 1) || (ival > 1024)) {
      RCD_RETURN_FAULT(retU);
   }
   value -= 1;
   inst[0] = value & 0xff;
   inst[1] = (inst[1] & 0xfc) | ((value >> 8) & 0x03);
   RCD_RETURN_OK(retU);
}

//a byte in 1/4 ms units; the integer value is the stored byte
rcode EDID_cl::DisplayID_QuarterMs(u32_t op, wxc_String& sval, u32_t& ival,
                                   edi_dynfld_t* p_field) {
   rcode retU;
   u8_t* inst = getValPtr(p_field);
   if (op == OP_READ) {
      ival = inst[0];
      sval.Printf("%.2f", ival / 4.0);
      RCD_RETURN_OK(retU);
   }

   long code;
   if (op == OP_WRSTR) {
      float value;
      retU = getStrFloat(sval, 0.0, 63.75 + 0.005, value);
      if (! RCD_IS_OK(retU)) return retU;
      code = std::lround(value * 4.0);
   } else if (op == OP_WRINT) {
      code = ival;
   } else {
      RCD_RETURN_FAULT(retU);
   }
   if ((code < 0) || (code > 255)) RCD_RETURN_FAULT(retU);
   inst[0] = code;
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
