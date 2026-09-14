/***************************************************************
 * Name:      EDID_summary.h
 * Purpose:   short overview of a parsed EDID
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_SUMMARY_H
#define EDID_SUMMARY_H 1

#include <string>
#include <vector>

class EDID_cl;

struct edid_summary_item {
   std::string section; //e.g. "Display"
   std::string label;   //e.g. "Name"
   std::string value;   //e.g. "Mi Monitor"
};

//Key facts read from the parsed groups, so unsaved edits are included.
//Items without data are left out.
std::vector<edid_summary_item> edid_summary(EDID_cl& EDID);

#endif
