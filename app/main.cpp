/***************************************************************
 * Name:      main.cpp
 * Purpose:   EDID Editor GTK4/libadwaita application entry point
 * License:   GPLv3+
 **************************************************************/

#include <adwaita.h>

#include "window.h"

int main(int argc, char* argv[]) {
   adw_init();

   AdwApplication* app = adw_application_new(
      "io.github.sachesi.EdidEditor",
      (GApplicationFlags) (G_APPLICATION_HANDLES_OPEN | G_APPLICATION_NON_UNIQUE)
   );

   g_signal_connect(app, "activate", G_CALLBACK(wxedid_app_activate), NULL);
   g_signal_connect(app, "open",     G_CALLBACK(wxedid_app_open),     NULL);

   int status = g_application_run(G_APPLICATION(app), argc, argv);

   g_object_unref(app);
   return status;
}
