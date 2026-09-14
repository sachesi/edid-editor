/***************************************************************
 * Name:      EDID_compare.h
 * Purpose:   field-level differences between two parsed EDIDs
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_COMPARE_H
#define EDID_COMPARE_H 1

#include <string>
#include <vector>

#include "def_types.h"

class EDID_cl;

struct edid_difference {
   std::string place; //block and group, e.g. "Block 1 · DTD: 2560x1440 @ 99.95Hz"
   std::string field; //field name as defined in the core; empty for a whole group
   std::string left;  //value in the first EDID; empty when the group is missing
   std::string right; //value in the second EDID; empty when the group is missing
};

//Groups are paired by their code within each block, keeping their order;
//fields of paired groups are compared by their shown value. Checksums are
//left out: they follow from the other bytes.
std::vector<edid_difference> edid_compare(EDID_cl& left, EDID_cl& right);

//Parse raw EDID bytes into EDID, with EDID errors ignored. Returns false
//with a reason when the data can't be parsed at all.
bool edid_parse_bytes(EDID_cl& EDID, const std::vector<u8_t>& bytes, std::string& problem);

#endif
