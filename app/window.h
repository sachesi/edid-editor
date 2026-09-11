/***************************************************************
 * Name:      window.h
 * Purpose:   main application window (GTK4/libadwaita)
 * License:   GPLv3+
 **************************************************************/

#ifndef WXEDID_WINDOW_H
#define WXEDID_WINDOW_H 1

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void wxedid_app_activate(AdwApplication* app, gpointer user_data);

G_END_DECLS

#endif /* WXEDID_WINDOW_H */
