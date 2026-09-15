/***************************************************************
 * Name:      main.cpp
 * Purpose:   EDID Editor GTK4/libadwaita application entry point
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include <adwaita.h>
#include <glib/gi18n.h>
#include <clocale>

#include "window.h"
#include "wxedid-config.h"

int main(int argc, char* argv[]) {
   setlocale(LC_ALL, "");
   bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
   bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
   textdomain(GETTEXT_PACKAGE);

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
