/***************************************************************
 * Name:      EDID_names.cpp
 * Purpose:   plain names of fields and groups, as the editor shows them
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include <cstdio>
#include <cstring>

#include "EDID_names.h"

std::string edid_field_display_name(const char* name) {
   struct field_name {
      const char* raw;
      const char* display;
   };
   static const field_name names[] = {
      {"header", "Header"},
      {"mfc_id", "Manufacturer ID"},
      {"prod_id", "Product ID"},
      {"serial", "Serial number"},
      {"prod_week", "Manufacture week"},
      {"prod_year", "Manufacture year"},
      {"edid_ver", "EDID version"},
      {"edid_rev", "EDID revision"},
      {"num_extblk", "Extension blocks"},
      {"checksum", "Checksum"},
      {"Input Type", "Input type"},
      {"VESA compat", "VESA compatibility"},
      {"IF Type", "Interface type"},
      {"Color Depth", "Color depth"},
      {"sync_green", "Sync on green"},
      {"comp_sync", "Composite sync"},
      {"sep_sync", "Separate sync"},
      {"blank_black", "Blank-to-black setup"},
      {"sync_wh_lvl", "Signal levels"},
      {"max_hsize", "Screen width"},
      {"max_vsize", "Screen height"},
      {"dpms_off", "DPMS off"},
      {"dpms_susp", "DPMS suspend"},
      {"dpms_stby", "DPMS standby"},
      {"vsig_format", "Signal format"},
      {"std_srbg", "sRGB default"},
      {"dtd0_native", "Native preferred timing"},
      {"gtf_cfreq", "Continuous frequency"},
      {"red_x", "Red x"}, {"red_y", "Red y"},
      {"green_x", "Green x"}, {"green_y", "Green y"},
      {"blue_x", "Blue x"}, {"blue_y", "Blue y"},
      {"white_x", "White x"}, {"white_y", "White y"},
      {"X-res", "Horizontal resolution"},
      {"Y-res", "Vertical resolution"},
      {"V-freq", "Refresh rate"},
      {"V-refresh", "Refresh rate"},
      {"asp_ratio", "Aspect ratio"},
      {"AspRatio", "Aspect ratio"},
      {"DMT_1", "DMT code"},
      {"DMT_2", "DMT code"},
      {"CVT_3", "CVT code"},
      {"H-Active pix", "Horizontal active"},
      {"H-Blank pix", "Horizontal blanking"},
      {"H-Border pix", "Horizontal border"},
      {"H-Sync offs", "Horizontal sync offset"},
      {"H-Sync width", "Horizontal sync width"},
      {"V-Active lines", "Vertical active"},
      {"V-Active lin", "Vertical active"},
      {"V-Blank lines", "Vertical blanking"},
      {"V-Border lines", "Vertical border"},
      {"V-Sync offs", "Vertical sync offset"},
      {"V-Sync width", "Vertical sync width"},
      {"H-Size", "Image width"},
      {"V-Size", "Image height"},
      {"sync_type", "Sync type"},
      {"Hsync_type", "Horizontal sync type"},
      {"Vsync_type", "Vertical sync type"},
      {"il2w_stereo", "Interleaved stereo"},
      {"stereo_mode", "Stereo mode"},
      {"interlace", "Interlaced"},
      {"zero_hdr", "Descriptor header"},
      {"desc_type", "Descriptor type"},
      {"min_Vfreq", "Minimum vertical rate"},
      {"max_Vfreq", "Maximum vertical rate"},
      {"min_Hfreq", "Minimum horizontal rate"},
      {"max_Hfreq", "Maximum horizontal rate"},
      {"max_PixClk", "Maximum pixel clock"},
      {"mrl_ext", "Timing support"},
      {"sfreq_sec", "Secondary curve start"},
      {"gtf_c", "GTF C"}, {"gtf_m", "GTF M"}, {"gtf_k", "GTF K"}, {"gtf_j", "GTF J"},
      {"CVT_majorV", "CVT major version"},
      {"CVT_minorV", "CVT minor version"},
      {"maxPixClk_apb", "Pixel clock precision"},
      {"max_HApix", "Maximum active pixels"},
      {"aspr_4_3", "4:3"}, {"aspr_16_9", "16:9"}, {"aspr_16_10", "16:10"},
      {"aspr_5_4", "5:4"}, {"aspr_15_9", "15:9"},
      {"blank_std", "Standard blanking"},
      {"blank_rb", "Reduced blanking"},
      {"pref_ar", "Preferred aspect ratio"},
      {"H_shrink", "Horizontal shrink"},
      {"H_stretch", "Horizontal stretch"},
      {"V_shrink", "Vertical shrink"},
      {"V_stretch", "Vertical stretch"},
      {"pref_vref", "Preferred refresh rate"},
      {"hex_text", "Text bytes"},
      {"wp1_idx", "White point 1 index"}, {"wp1_x", "White point 1 x"},
      {"wp1_y", "White point 1 y"}, {"wp1_gamma", "White point 1 gamma"},
      {"wp2_idx", "White point 2 index"}, {"wp2_x", "White point 2 x"},
      {"wp2_y", "White point 2 y"}, {"wp2_gamma", "White point 2 gamma"},
      {"vref_50", "50 Hz"}, {"vref_60", "60 Hz"}, {"vref_60_rb", "60 Hz reduced blanking"},
      {"vref_75", "75 Hz"}, {"vref_85", "85 Hz"},
      {"num_dtd", "Native timings"},
      {"Blk length", "Block length"},
      {"Blk_rev", "Block revision"},
      {"blk_rev", "Block revision"},
      {"Tag Code", "Tag code"},
      {"Ext Tag Code", "Extended tag code"},
      {"IEEE-OUI", "IEEE OUI"},
      {"num_chn", "Channels"},
      {"AFC", "Audio format"},
      {"ACE_TC", "Audio coding extension"},
      {"AFC_dep_val", "Format-dependent value"},
      {"sf_32kHz", "32 kHz"}, {"sf_44.1kHz", "44.1 kHz"}, {"sf_48kHz", "48 kHz"},
      {"sf_88.2kHz", "88.2 kHz"}, {"sf_96kHz", "96 kHz"}, {"sf_176.4kHz", "176.4 kHz"},
      {"sf_192kHz", "192 kHz"},
      {"sample16b", "16-bit"}, {"sample20b", "20-bit"}, {"sample24b", "24-bit"},
      {"s16bit", "16-bit"}, {"s20bit", "20-bit"}, {"s24bit", "24-bit"},
      {"FL_FR", "Front left/right"},
      {"LFE1", "Low-frequency effects 1"},
      {"LFE2", "Low-frequency effects 2"},
      {"FC", "Front center"},
      {"BL_BR", "Back left/right"},
      {"BC", "Back center"},
      {"FLC_FRC", "Front left/right of center"},
      {"FLW_FRW", "Front left/right wide"},
      {"TpFL_TpFR", "Top front left/right"},
      {"TpC", "Top center"},
      {"TpFC", "Top front center"},
      {"LS_RS", "Left/right surround"},
      {"TpBC", "Top back center"},
      {"SiL_SiR", "Side left/right"},
      {"TpSiL_TpSiR", "Top side left/right"},
      {"TpBL_TpBR", "Top back left/right"},
      {"BtFC", "Bottom front center"},
      {"BtFL_BtFR", "Bottom front left/right"},
      {"src phy", "Physical address"},
      {"Supports_AI", "Supports AI"},
      {"DC_48bit", "Deep color 48-bit"},
      {"DC_36bit", "Deep color 36-bit"},
      {"DC_30bit", "Deep color 30-bit"},
      {"DC_Y444", "Deep color in YCbCr 4:4:4"},
      {"DC_48bit_420", "Deep color 48-bit 4:2:0"},
      {"DC_36bit_420", "Deep color 36-bit 4:2:0"},
      {"DC_30bit_420", "Deep color 30-bit 4:2:0"},
      {"DVI_dual", "DVI dual link"},
      {"Max_TMDS", "Maximum TMDS clock"},
      {"latency_f", "Latency present"},
      {"i_latency", "Interlaced latency present"},
      {"Video iLatency", "Interlaced video latency"},
      {"Audio iLatency", "Interlaced audio latency"},
      {"SCDC_Present", "SCDC present"},
      {"RR_Capable", "Read request capable"},
      {"LTE_340Mcsc_Scramble", "Scrambling at 340 Mcsc or less"},
      {"Max_FRL_Rate", "Maximum FRL rate"},
      {"ALLM", "Auto low latency mode"},
      {"FVA", "Fast vactive"},
      {"CNMVRR", "Negative MVRR"},
      {"CinemaVRR", "Cinema VRR"},
      {"M_delta", "M delta"},
      {"VRRmin", "Minimum VRR"},
      {"VRRmax", "Maximum VRR"},
      {"QMS_TFRmin", "QMS minimum TFR"},
      {"QMS_TFRmax", "QMS maximum TFR"},
      {"DSC_1p2", "DSC 1.2"},
      {"DSC_Native_420", "DSC native 4:2:0"},
      {"DSC_All_bpp", "DSC all bit depths"},
      {"DSC_10bpc", "DSC 10 bpc"},
      {"DSC_12bpc", "DSC 12 bpc"},
      {"DSC_16bpc", "DSC 16 bpc"},
      {"DSC_MaxSlices", "DSC maximum slices"},
      {"DSC_Max_FRL_Rate", "DSC maximum FRL rate"},
      {"DSC_TotalChunkKBytes", "DSC total chunk size"},
      {"UHD_VIC", "UHD VIC"},
      {"EEODB_count", "Block count"},
      {"Feature_Caps", "Feature capabilities"},
      {"Min_Refresh", "Minimum refresh rate"},
      {"Max_Refresh", "Maximum refresh rate"},
      {"Max_Refresh_8bit", "Maximum refresh rate (8-bit)"},
      {"Flags_1x", "FreeSync 1 flags"},
      {"Flags_2x", "FreeSync 2 flags"},
      {"Max_Luminance", "Maximum luminance"},
      {"Min_Luminance", "Minimum luminance"},
      {"Max_Luminance_2", "Maximum luminance 2"},
      {"Min_Luminance_2", "Minimum luminance 2"},
      {"SMPTE", "SMPTE ST 2084"},
      {"HLG", "Hybrid log-gamma"},
      {"SDR", "SDR gamma"},
      {"HDR", "HDR gamma"},
      {"SM_0", "Static metadata type 1"},
      {"max_lum", "Maximum luminance"},
      {"avg_lum", "Average luminance"},
      {"min_lum", "Minimum luminance"},
      {"QY", "YCC quantization selectable"},
      {"QS", "RGB quantization selectable"},
      {"S_PT01", "Preferred timing scan"},
      {"S_IT01", "IT scan"},
      {"S_CE01", "CE scan"},
   };

   for (const field_name& item : names) {
      if (0 == strcmp(name, item.raw)) return item.display;
   }

   //established timings: 800x600x60 -> 800×600 @ 60 Hz
   unsigned width = 0;
   unsigned height = 0;
   unsigned rate = 0;
   char mode = 0;
   int fields = sscanf(name, "%ux%ux%u%c", &width, &height, &rate, &mode);
   if ((fields >= 3) && (width > 0) && ((fields == 3) || (mode == 'i'))) {
      char timing[48];
      snprintf(timing, sizeof(timing), "%u×%u%s @ %u Hz", width, height,
               (fields == 4) ? "i" : "", rate);
      return timing;
   }

   std::string display = name;
   for (char& ch : display) {
      if (ch == '_') ch = ' ';
   }
   if (! display.empty() && (display[0] >= 'a') && (display[0] <= 'z')) {
      display[0] = static_cast<char>(display[0] - 'a' + 'A');
   }
   return display;
}

std::string edid_group_display_name(edi_grp_cl* pgrp, EDID_cl& EDID) {
   wxc_String name;
   pgrp->getGrpName(EDID, name);
   std::string display = name.std_str();
   if (! display.empty() && (display[0] >= 'a') && (display[0] <= 'z')) {
      display[0] = static_cast<char>(display[0] - 'a' + 'A');
   }
   return display;
}
