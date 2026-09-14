/***************************************************************
 * Name:      test_display.cpp
 * Purpose:   connected display listing from a sysfs-like tree
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#include "EDID_display.h"

static int failures = 0;

static void check(bool ok, const char* what) {
   std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
   if (! ok) failures++;
}

static void write_file(const std::string& path, const std::vector<u8_t>& data) {
   FILE* output = std::fopen(path.c_str(), "wb");
   if (output == NULL) return;
   if (! data.empty()) std::fwrite(data.data(), 1, data.size(), output);
   std::fclose(output);
}

int main(int argc, char* argv[]) {
   if (argc != 2) return 2;
   FILE* input = std::fopen(argv[1], "rb");
   if (input == NULL) return 2;
   std::vector<u8_t> edid(512);
   edid.resize(std::fread(edid.data(), 1, edid.size(), input));
   std::fclose(input);

   char root[] = "/tmp/edid-editor-drm-XXXXXX";
   check(mkdtemp(root) != NULL, "create a connector tree");
   std::string base = root;
   const char* connectors[] = {"card0", "card0-HDMI-A-1", "card0-DP-1", "card0-DP-2"};
   for (const char* name : connectors) mkdir((base + "/" + name).c_str(), 0700);
   write_file(base + "/card0-HDMI-A-1/edid", edid);
   write_file(base + "/card0-DP-1/edid", std::vector<u8_t>());   //disconnected
   std::vector<u8_t> nameless(edid.begin(), edid.begin() + 128);
   for (int offset=54; offset<=108; offset += 18) nameless[offset + 3] = 0x10;
   write_file(base + "/card0-DP-2/edid", nameless);
   write_file(base + "/version", std::vector<u8_t>(4, 'x'));

   std::vector<edid_display> displays = edid_connected_displays(root);
   check(displays.size() == 2, "only connectors with EDID data are listed");
   check((displays.size() == 2) && (displays[0].connector == "card0-DP-2") &&
         (displays[1].connector == "card0-HDMI-A-1"), "connectors are sorted by name");
   check((displays.size() == 2) && (displays[1].name == "GTK-PORT") &&
         (displays[1].data == edid) &&
         (displays[1].path == base + "/card0-HDMI-A-1/edid"),
         "a display is named by its monitor name descriptor");
   check((displays.size() == 2) && (displays[0].name == "OXC 1234"),
         "a display without a name shows its manufacturer and product");
   check(edid_connected_displays("/nonexistent/drm").empty(),
         "a missing connector tree lists nothing");

   for (const char* name : connectors) {
      unlink((base + "/" + name + "/edid").c_str());
      rmdir((base + "/" + name).c_str());
   }
   unlink((base + "/version").c_str());
   rmdir(root);

   std::printf("---\n%s\n", failures == 0 ? "ALL OK" : "FAILURES PRESENT");
   return failures == 0 ? 0 : 1;
}
