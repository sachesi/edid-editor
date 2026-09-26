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

//each window asks about its unsaved changes as it closes
static void wxedid_app_quit(GSimpleAction*, GVariant*, gpointer app) {
   GList* windows = g_list_copy(gtk_application_get_windows(GTK_APPLICATION(app)));
   for (GList* it = windows; it != NULL; it = it->next) {
      gtk_window_close(GTK_WINDOW(it->data));
   }
   g_list_free(windows);
}

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

   GSimpleAction* quit_action = g_simple_action_new("quit", NULL);
   g_signal_connect(quit_action, "activate", G_CALLBACK(wxedid_app_quit), app);
   g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(quit_action));
   g_object_unref(quit_action);

   int status = g_application_run(G_APPLICATION(app), argc, argv);

   g_object_unref(app);
   return status;
}
