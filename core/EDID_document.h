/***************************************************************
 * Name:      EDID_document.h
 * Purpose:   opening, placing groups in, and writing whole EDIDs
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_DOCUMENT_H
#define EDID_DOCUMENT_H 1

#include <cstddef>
#include <string>

#include "EDID_class.h"
#include "guilog.h"

struct edid_load_result {
   bool rejected;         //the data was refused before parsing; EDID is unchanged
   bool opened;           //the base block parsed
   bool extension_failed; //a CTA-861 or DisplayID block failed and is kept as data
   bool partial;          //the last block was incomplete and is padded with zeros
   bool count_adjusted;   //the declared block count was changed to match the data
   bool can_retry;        //opening again with EDID errors ignored may help
};

//Parse EDID data of 1 to 4 blocks into EDID, following EDID.b_ERR_Ignore.
//Notices ("[i] ...") and errors ("[E!] ...") go to log; name identifies the
//data in them.
edid_load_result edid_load(EDID_cl& EDID, const u8_t* data, size_t size,
                           const char* name, guilog_cl& log);

//Put a new group into the groups of a CTA-861 or DisplayID block: after the
//data blocks and before the detailed timings or the DisplayID padding.
//Returns false, leaving array unchanged, when it does not fit.
bool edid_insert_group(GroupAr_cl* array, edi_grp_cl* group);

//Assemble the groups into the EDID buffer and recompute the checksums of
//the parsed blocks. Returns false with an error in log when the data can't
//be written.
bool edid_prepare_output(EDID_cl& EDID, guilog_cl& log);

#endif
