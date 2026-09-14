/***************************************************************
 * Name:      EDID_display.cpp
 * Purpose:   EDID data of connected displays, read from sysfs
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>

#include "EDID_display.h"

std::string edid_display_name(const u8_t* data, size_t size) {
   if (size < 128) return std::string();
   for (u32_t offset=54; offset<=108; offset += 18) {
      const u8_t* descriptor = data + offset;
      if ((descriptor[0] != 0) || (descriptor[1] != 0) || (descriptor[3] != 0xFC)) continue;
      std::string name;
      for (u32_t idx=5; idx<18; idx++) {
         u8_t chr = descriptor[idx];
         if ((chr == 0x0A) || (chr == 0)) break;
         name.push_back(((chr >= 0x20) && (chr < 0x7F)) ? static_cast<char>(chr) : '?');
      }
      while (! name.empty() && (name.back() == ' ')) name.pop_back();
      if (! name.empty()) return name;
   }
   //manufacturer ID: three 5-bit letters, big endian
   u32_t id = (data[8] << 8) | data[9];
   char text[16];
   snprintf(text, sizeof(text), "%c%c%c %04X",
            static_cast<char>('@' + ((id >> 10) & 0x1F)),
            static_cast<char>('@' + ((id >> 5) & 0x1F)),
            static_cast<char>('@' + (id & 0x1F)),
            data[10] | (data[11] << 8));
   return text;
}

std::vector<edid_display> edid_connected_displays(const char* drm_root) {
   std::vector<edid_display> displays;
   DIR* directory = opendir(drm_root);
   if (directory == NULL) return displays;

   struct dirent* entry;
   while ((entry = readdir(directory)) != NULL) {
      if (entry->d_name[0] == '.') continue;
      std::string path = std::string(drm_root) + "/" + entry->d_name + "/edid";
      //sysfs reports a size of 0, so read until the end
      FILE* input = std::fopen(path.c_str(), "rb");
      if (input == NULL) continue;
      std::vector<u8_t> data;
      u8_t chunk[256];
      size_t count;
      while ((count = std::fread(chunk, 1, sizeof(chunk), input)) > 0) {
         data.insert(data.end(), chunk, chunk + count);
         if (data.size() > 4096) break;
      }
      std::fclose(input);
      if (data.size() < 128) continue;

      edid_display display;
      display.connector = entry->d_name;
      display.path = path;
      display.name = edid_display_name(data.data(), data.size());
      display.data = data;
      displays.push_back(display);
   }
   closedir(directory);

   std::sort(displays.begin(), displays.end(),
             [](const edid_display& a, const edid_display& b) {
                return a.connector < b.connector;
             });
   return displays;
}
