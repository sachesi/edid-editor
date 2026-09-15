/***************************************************************
 * Name:      EDID_names.h
 * Purpose:   plain names of fields and groups, as the editor shows them
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_NAMES_H
#define EDID_NAMES_H 1

#include <string>

#include "EDID_class.h"

//A plain name for a field of the core, such as "Block length" for
//"Blk length"; names without one are shown with spaces for '_' and a
//capital first letter.
std::string edid_field_display_name(const char* name);

//The group name, starting with a capital letter.
std::string edid_group_display_name(edi_grp_cl* pgrp, EDID_cl& EDID);

#endif
