/***************************************************************
 * Name:      DisplayID_class.h
 * Purpose:   DisplayID extension groups
 * License:   GPLv3+
 **************************************************************/

#ifndef DISPLAYID_CLASS_H
#define DISPLAYID_CLASS_H 1

#include "EDID_shared.h"

class displayid_hdr_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

   public:
      rcode init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      void SpawnInstance(u8_t* pinst);
};

class displayid_data_block_cl : public dbc_grp_cl {
   private:
      static const edi_field_t fields[];
      u8_t version;

   public:
      rcode init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      u32_t getTotalSize() {return dat_sz;};
      void setDataSize(u32_t dsz) {dat_sz = dsz;};
      edi_grp_cl* Clone(rcode& rcd, u32_t flags) {
         return base_clone(rcd, new displayid_data_block_cl(), version | flags);
      };
};

class displayid_padding_cl : public edi_grp_cl {
   public:
      rcode init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};

class displayid_type1_timing_cl : public edi_grp_cl {
   private:
      static const edi_field_t fields[];

   public:
      rcode init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
      void getGrpName(EDID_cl& EDID, wxc_String& gp_name);
};

class displayid_raw_payload_cl : public edi_grp_cl {
   private:
      char field_names[121][24];

   public:
      rcode init(const u8_t* inst, u32_t orflags, edi_grp_cl* parent);
};

const char* displayid_data_block_name(u8_t version, u8_t tag);

#endif /* DISPLAYID_CLASS_H */
