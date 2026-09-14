/***************************************************************
 * Name:      EDID_display.h
 * Purpose:   EDID data of connected displays, read from sysfs
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_DISPLAY_H
#define EDID_DISPLAY_H 1

#include <string>
#include <vector>

#include "def_types.h"

struct edid_display {
   std::string       connector; //e.g. card1-DP-1
   std::string       path;      //the connector's edid file
   std::string       name;      //monitor name, or manufacturer and product
   std::vector<u8_t> data;
};

//Connectors under drm_root (normally /sys/class/drm) whose edid file holds
//data, sorted by connector name.
std::vector<edid_display> edid_connected_displays(const char* drm_root);

//Monitor name descriptor text, or the manufacturer ID and product code.
std::string edid_display_name(const u8_t* data, size_t size);

#endif
