/***************************************************************
 * Name:      window.cpp
 * Purpose:   the window: its construction, actions, banners and document state
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include "window_private.h"

//file name to show for a document: display data is named by its connector
char* document_basename(const char* path) {
   size_t root = strlen(DRM_ROOT);
   if ((0 == strncmp(path, DRM_ROOT, root)) && (path[root] == '/') &&
       g_str_has_suffix(path, "/edid")) {
      char* directory = g_path_get_dirname(path);
      char* connector = g_path_get_basename(directory);
      g_free(directory);
      return connector;
   }
   return g_path_get_basename(path);
}

void wnd_refresh_group_title(wxedid_wnd* wnd, edi_grp_cl* pgrp) {
   if (pgrp == NULL) return;
   gtk_label_set_text(wnd->group_title, edid_group_display_name(pgrp, wnd->doc->EDID).c_str());
}

//a failure of the editor itself, logged and shown in the banner
void wnd_log_error(wxedid_wnd* wnd, const char* format, ...) {
   va_list args;
   va_start(args, format);
   char* text = g_strdup_vprintf(format, args);
   va_end(args);
   char* message = g_strconcat("[E!] ", text, NULL);
   wnd->doc->GLog.DoLog(message);
   g_free(message);
   g_free(text);
}

static void log_sink(const char* msg, void* user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   if ((wnd == NULL) || (wnd->log == NULL)) return;

   GtkTextIter end;
   gtk_text_buffer_get_end_iter(wnd->log, &end);
   gtk_text_buffer_insert(wnd->log, &end, msg, -1);
   gtk_text_buffer_insert(wnd->log, &end, "\n", -1);

   //notices from opening a file belong on the Overview
   bool notice = g_str_has_prefix(msg, "[i]");
   bool error = g_str_has_prefix(msg, "[E!]");
   if (wnd->loading && (notice || error)) {
      const char* text = msg + (notice ? 3 : 4);
      while (*text == ' ') text++;
      wnd->notes.push_back(text);
   }
   if (error) {
      const char* detail = msg + 4;
      while (*detail == ' ') detail++;
      wnd_show_error(wnd, detail);
   }
}

void wnd_update_document_ui(wxedid_wnd* wnd) {
   bool can_save = wnd->loaded && wnd->dirty && (wnd->invalid_fields == 0);
   g_simple_action_set_enabled(wnd->save_action, can_save);
   bool can_write = wnd->loaded && (wnd->invalid_fields == 0);
   g_simple_action_set_enabled(wnd->save_as_action, can_write);
   g_simple_action_set_enabled(wnd->export_hex_action, can_write);
   g_simple_action_set_enabled(wnd->save_report_action, can_write);
   g_simple_action_set_enabled(wnd->compare_file_action, wnd->loaded);
   g_simple_action_set_enabled(wnd->compare_display_action, wnd->loaded);
   gtk_widget_set_visible(wnd->save_button, wnd->loaded);
   gtk_button_set_label(GTK_BUTTON(wnd->save_button), _("_Save"));
   gtk_button_set_use_underline(GTK_BUTTON(wnd->save_button), TRUE);
   gtk_widget_set_tooltip_text(
      wnd->save_button,
      wnd->document_hex    ? _("Save as an EDID binary (Ctrl+S)") :
      wnd->source_writable ? _("Save changes (Ctrl+S)") :
                             _("Save a writable copy (Ctrl+S)"));

   if (wnd->loaded) {
      char* basename = document_basename(wnd->doc->path);
      char* display_path = g_filename_display_name(wnd->doc->path);
      char* window_name = g_strdup_printf(_("%s — EDID Editor"), basename);
      const char* state = wnd->dirty ? _("Modified") : NULL;
      char* subtitle = NULL;
      if (wnd->document_hex) {
         subtitle = (state != NULL)
            ? g_strdup_printf(_("%s · Imported · %s"), state, display_path)
            : g_strdup_printf(_("Imported · %s"), display_path);
      } else if (! wnd->source_writable && (state != NULL)) {
         subtitle = g_strdup_printf(_("%s · Read-only · %s"), state, display_path);
      } else if (! wnd->source_writable) {
         subtitle = g_strdup_printf(_("Read-only · %s"), display_path);
      } else if (state != NULL) {
         subtitle = g_strdup_printf("%s · %s", state, display_path);
      } else {
         subtitle = g_strdup(display_path);
      }

      adw_window_title_set_title(wnd->window_title, basename);
      adw_window_title_set_subtitle(wnd->window_title, subtitle);
      gtk_window_set_title(wnd->window, window_name);

      g_free(subtitle);
      g_free(window_name);
      g_free(display_path);
      g_free(basename);
   } else {
      adw_window_title_set_title(wnd->window_title, _("EDID Editor"));
      adw_window_title_set_subtitle(wnd->window_title, NULL);
      gtk_window_set_title(wnd->window, _("EDID Editor"));
   }

   const char* source_note = NULL;
   if (wnd->loaded && ! wnd->source_writable) {
      if (g_str_has_prefix(wnd->doc->path, DRM_ROOT)) {
         source_note = _("Read from a connected display. Save a copy to keep your changes.");
      } else if (wnd->document_hex) {
         if (wnd->dirty) source_note = _("Imported from hex text. Save it as an EDID binary.");
      } else {
         source_note = _("This file is read-only. Save a copy to keep your changes.");
      }
   }
   if (source_note != NULL) adw_banner_set_title(wnd->source_banner, source_note);
   adw_banner_set_revealed(wnd->source_banner, source_note != NULL);

   if (wnd->loaded && wnd->overview_shown) wnd_refresh_overview(wnd);

   if (wnd->invalid_fields > 0) {
      adw_banner_set_title(wnd->banner, _("Enter a valid value before saving"));
      adw_banner_set_button_label(wnd->banner, NULL);
      adw_banner_set_revealed(wnd->banner, TRUE);
      wnd->banner_is_validation = true;
      wnd->banner_offers_retry = false;
   } else if (wnd->banner_is_validation) {
      adw_banner_set_revealed(wnd->banner, FALSE);
      wnd->banner_is_validation = false;
   }
   wnd_update_header_controls(wnd);
}

void wnd_update_header_controls(wxedid_wnd* wnd) {
   bool collapsed = adw_overlay_split_view_get_collapsed(wnd->split_view);
   gtk_widget_set_visible(GTK_WIDGET(wnd->window_title), ! collapsed);
   gtk_widget_set_visible(wnd->open_button, wnd->loaded && ! collapsed);
   gtk_widget_set_visible(wnd->sidebar_button, wnd->loaded && collapsed);
}

void wnd_show_error(wxedid_wnd* wnd, const char* message) {
   adw_banner_set_title(wnd->banner, message);
   adw_banner_set_button_label(wnd->banner, _("Details"));
   adw_banner_set_revealed(wnd->banner, TRUE);
   wnd->banner_is_validation = false;
   wnd->banner_offers_retry = false;
}

void wnd_clear_feedback(wxedid_wnd* wnd) {
   gtk_text_buffer_set_text(wnd->log, "", -1);
   wnd->notes.clear();
   adw_banner_set_revealed(wnd->banner, FALSE);
   wnd->banner_is_validation = false;
   wnd->banner_offers_retry = false;
}

static void wnd_on_banner_details(AdwBanner* /*banner*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   if (wnd->banner_offers_retry) {
      wnd->doc->EDID.b_ERR_Ignore = true;
      g_simple_action_set_state(wnd->ignore_errors_action,
                                g_variant_new_boolean(TRUE));
      wnd_reload_source(wnd);
      return;
   }
   wnd_present_log(wnd);
}

static void wnd_on_source_banner(AdwBanner*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   g_action_activate(G_ACTION(wnd->save_as_action), NULL);
}

static void wnd_on_toggle_sidebar(GtkButton* /*button*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   gboolean visible = adw_overlay_split_view_get_show_sidebar(wnd->split_view);
   adw_overlay_split_view_set_show_sidebar(wnd->split_view, ! visible);
}

static void wnd_on_split_collapsed(GObject* /*object*/, GParamSpec* /*pspec*/,
                                   gpointer user_data) {
   wnd_update_header_controls((wxedid_wnd*) user_data);
}

static void wnd_on_discard_close_response(GObject* source, GAsyncResult* result,
                                          gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   const char* response = adw_alert_dialog_choose_finish(
      ADW_ALERT_DIALOG(source), result);
   wnd->close_confirmation_open = false;
   if (0 == strcmp(response, "discard")) {
      wnd->dirty = false;
      wnd_save_state(wnd);
      gtk_window_destroy(wnd->window);
   }
}

static gboolean wnd_on_close_request(GtkWindow* /*window*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   if (! wnd->dirty) {
      wnd_save_state(wnd);
      return FALSE;
   }
   if (wnd->close_confirmation_open) return TRUE;

   wnd->close_confirmation_open = true;
   AdwAlertDialog* dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new(
      _("Discard unsaved changes?"),
      _("Closing this window will discard changes to the current EDID.")));
   adw_alert_dialog_add_responses(dialog,
                                  "cancel", _("Cancel"),
                                  "discard", _("Discard"),
                                  NULL);
   adw_alert_dialog_set_close_response(dialog, "cancel");
   adw_alert_dialog_set_default_response(dialog, "cancel");
   adw_alert_dialog_set_response_appearance(dialog, "discard",
                                            ADW_RESPONSE_DESTRUCTIVE);
   adw_alert_dialog_choose(dialog, GTK_WIDGET(wnd->window), NULL,
                           wnd_on_discard_close_response, wnd);
   return TRUE;
}

static void wnd_on_about_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   static const char* developers[] = {
      "Tomasz Pawlak",
      "sachesi",
      NULL,
   };
   AdwAboutDialog* dialog = ADW_ABOUT_DIALOG(adw_about_dialog_new());
   adw_about_dialog_set_application_name(dialog, _("EDID Editor"));
   adw_about_dialog_set_application_icon(dialog, "io.github.sachesi.EdidEditor");
   adw_about_dialog_set_developer_name(dialog, "sachesi");
   adw_about_dialog_set_version(dialog, WXEDID_VERSION);
   adw_about_dialog_set_website(dialog, "https://github.com/sachesi/edid-editor");
   adw_about_dialog_set_issue_url(dialog, "https://github.com/sachesi/edid-editor/issues");
   adw_about_dialog_set_comments(dialog,
      _("Inspect and edit Extended Display Identification Data.\n\n"
        "Based on wxEDID by Tomasz Pawlak."));
   adw_about_dialog_add_link(dialog, _("wxEDID, the original project"),
                             "https://sourceforge.net/projects/wxedid/");
   adw_about_dialog_set_developers(dialog, developers);
   adw_about_dialog_set_copyright(dialog, "Copyright © 2014–2025 Tomasz Pawlak\n"
                                          "Copyright © 2026 sachesi");
   adw_about_dialog_set_license_type(dialog, GTK_LICENSE_GPL_3_0);
   adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(wnd->window));
}

static void wnd_on_ignore_errors_state(GSimpleAction* action, GVariant* value,
                                       gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   bool enabled = g_variant_get_boolean(value);
   g_simple_action_set_state(action, value);
   wnd->doc->EDID.b_ERR_Ignore = enabled;
   if (! enabled || ! wnd->load_had_errors || wnd->source_path.empty()) return;
   if (wnd->dirty) {
      adw_toast_overlay_add_toast(wnd->toast_overlay, adw_toast_new(
         _("Open the file again to read it with errors ignored")));
      return;
   }
   wnd_reload_source(wnd);
}

static void wnd_on_ignore_read_only_state(GSimpleAction* action, GVariant* value,
                                          gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   g_simple_action_set_state(action, value);
   wnd->doc->EDID.b_RD_Ignore = g_variant_get_boolean(value);
   edi_grp_cl* group = wnd_selected_group(wnd);
   if (group != NULL) rows_reload(wnd->fields, group, &wnd->doc->EDID, wnd);
}

static void wnd_on_show_reserved_state(GSimpleAction* action, GVariant* value,
                                       gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   g_simple_action_set_state(action, value);
   wnd->show_reserved = g_variant_get_boolean(value);
   edi_grp_cl* group = wnd_selected_group(wnd);
   if (group != NULL) rows_reload(wnd->fields, group, &wnd->doc->EDID, wnd);
}

static void wnd_on_find_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   if (! wnd->loaded) return;
   if (adw_overlay_split_view_get_collapsed(wnd->split_view)) {
      adw_overlay_split_view_set_show_sidebar(wnd->split_view, TRUE);
   }
   gtk_widget_grab_focus(GTK_WIDGET(wnd->tree_search));
}

static void wnd_on_shortcuts_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   struct shortcut {
      const char* title;
      const char* accelerator;
   };
   struct section {
      const char* title;
      shortcut    items[6];
   };
   static const section sections[] = {
      {N_("Files"), {
         {N_("Open a file"), "<Control>o"},
         {N_("Save changes"), "<Control>s"},
         {N_("Save as a new file"), "<Control><Shift>s"},
      }},
      {N_("Editing"), {
         {N_("Undo"), "<Control>z"},
         {N_("Redo"), "<Control><Shift>z"},
      }},
      {N_("Groups"), {
         {N_("Duplicate group"), "<Control>d"},
         {N_("Delete group"), "Delete"},
         {N_("Move group up"), "<Alt>Up"},
         {N_("Move group down"), "<Alt>Down"},
         {N_("Show group menu"), "<Shift>F10"},
      }},
      {N_("General"), {
         {N_("Search groups"), "<Control>f"},
         {N_("Keyboard shortcuts"), "<Control>question"},
      }},
   };
   AdwDialog* dialog = adw_shortcuts_dialog_new();
   for (const section& spec : sections) {
      AdwShortcutsSection* group = adw_shortcuts_section_new(_(spec.title));
      for (const shortcut& item : spec.items) {
         if (item.title == NULL) break;
         adw_shortcuts_section_add(group,
                                   adw_shortcuts_item_new(_(item.title), item.accelerator));
      }
      adw_shortcuts_dialog_add(ADW_SHORTCUTS_DIALOG(dialog), group);
   }
   adw_dialog_present(dialog, GTK_WIDGET(wnd->window));
}

//------------
// 'open' signal: files passed on the command line
void wxedid_app_open(AdwApplication* app, GFile** files, gint n_files,
                     gchar* /*hint*/, gpointer /*user_data*/) {
   //activate first (creates the window), then load into it
   wxedid_app_activate(app, NULL);

   //find the window created by activate
   GtkWindow* window = gtk_application_get_active_window(GTK_APPLICATION(app));
   if (window == NULL) return;

   wxedid_wnd* wnd = (wxedid_wnd*) g_object_get_data(G_OBJECT(window), "wxedid-wnd");
   if (wnd == NULL) return;

   for (gint i=0; i<n_files; i++) {
      char* path = g_file_get_path(files[i]);
      if (path != NULL) {
         wnd_load_file(wnd, path, path_is_hex_text(path));
         g_free(path);
      }
   }
}

//------------
void wxedid_app_activate(AdwApplication* app, gpointer /*user_data*/) {
   wxedid_wnd* wnd = new wxedid_wnd{};
   wnd->doc        = new wxedid_doc;
   wnd->doc->path[0] = 0;
   wnd->saved_history_position = 0;

   GtkWidget* window = adw_application_window_new(GTK_APPLICATION(app));
   wnd->window = GTK_WINDOW(window);
   gtk_window_set_default_size(GTK_WINDOW(window), 900, 640);
   gtk_window_set_title(GTK_WINDOW(window), _("EDID Editor"));
   g_signal_connect(window, "close-request", G_CALLBACK(wnd_on_close_request), wnd);

   g_object_set_data_full(G_OBJECT(window), "wxedid-wnd", wnd,
                           [](gpointer data) {
                              wxedid_wnd* w = (wxedid_wnd*) data;
                              if (w->recent_changed != 0)
                                 g_signal_handler_disconnect(gtk_recent_manager_get_default(),
                                                             w->recent_changed);
                              g_clear_object(&w->recent_menu);
                              g_clear_object(&w->log);
                              g_clear_object(&w->tree_filtered);
                              g_clear_object(&w->tree_filter);
                              g_clear_object(&w->tree_model);
                              if (w->refresh_source != 0) g_source_remove(w->refresh_source);
                              wnd_clear_history(w);
                              delete w->timing;
                              delete w->doc;
                              delete w;
                           });

   GSimpleAction* open_action = g_simple_action_new("open", NULL);
   g_signal_connect(open_action, "activate", G_CALLBACK(wnd_on_open_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(open_action));
   g_object_unref(open_action);

   wnd->save_action = g_simple_action_new("save", NULL);
   g_signal_connect(wnd->save_action, "activate", G_CALLBACK(wnd_on_save_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->save_action));
   g_object_unref(wnd->save_action);

   wnd->save_as_action = g_simple_action_new("save-as", NULL);
   g_signal_connect(wnd->save_as_action, "activate",
                    G_CALLBACK(wnd_on_save_as_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->save_as_action));
   g_object_unref(wnd->save_as_action);

   wnd->undo_action = g_simple_action_new("undo", NULL);
   g_signal_connect(wnd->undo_action, "activate", G_CALLBACK(wnd_on_undo_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->undo_action));
   g_object_unref(wnd->undo_action);

   wnd->redo_action = g_simple_action_new("redo", NULL);
   g_signal_connect(wnd->redo_action, "activate", G_CALLBACK(wnd_on_redo_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->redo_action));
   g_object_unref(wnd->redo_action);

   GSimpleAction* log_action = g_simple_action_new("show-log", NULL);
   g_signal_connect(log_action, "activate", G_CALLBACK(wnd_on_log_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(log_action));
   g_object_unref(log_action);

   wnd->duplicate_action = g_simple_action_new("duplicate-group", NULL);
   g_signal_connect(wnd->duplicate_action, "activate",
                    G_CALLBACK(wnd_on_duplicate_group), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->duplicate_action));
   g_object_unref(wnd->duplicate_action);

   wnd->make_preferred_action = g_simple_action_new("make-preferred", NULL);
   g_signal_connect(wnd->make_preferred_action, "activate",
                    G_CALLBACK(wnd_on_make_preferred), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->make_preferred_action));
   g_object_unref(wnd->make_preferred_action);

   wnd->delete_action = g_simple_action_new("delete-group", NULL);
   g_signal_connect(wnd->delete_action, "activate",
                    G_CALLBACK(wnd_on_delete_group), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->delete_action));
   g_object_unref(wnd->delete_action);

   wnd->move_up_action = g_simple_action_new("move-group-up", NULL);
   g_signal_connect(wnd->move_up_action, "activate",
                    G_CALLBACK(wnd_on_move_group_up), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->move_up_action));
   g_object_unref(wnd->move_up_action);

   wnd->move_down_action = g_simple_action_new("move-group-down", NULL);
   g_signal_connect(wnd->move_down_action, "activate",
                    G_CALLBACK(wnd_on_move_group_down), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->move_down_action));
   g_object_unref(wnd->move_down_action);

   wnd->add_cta_action = g_simple_action_new("add-cta-group", G_VARIANT_TYPE_STRING);
   g_signal_connect(wnd->add_cta_action, "activate",
                    G_CALLBACK(wnd_on_add_cta_group), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->add_cta_action));
   g_object_unref(wnd->add_cta_action);

   wnd->add_displayid_action = g_simple_action_new("add-displayid-group", NULL);
   g_signal_connect(wnd->add_displayid_action, "activate",
                    G_CALLBACK(wnd_on_add_displayid_group), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->add_displayid_action));
   g_object_unref(wnd->add_displayid_action);

   GSimpleAction* display_action = g_simple_action_new("open-display", NULL);
   g_signal_connect(display_action, "activate",
                    G_CALLBACK(wnd_on_open_display_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(display_action));
   g_object_unref(display_action);

   GSimpleAction* import_action = g_simple_action_new("import-hex", NULL);
   g_signal_connect(import_action, "activate", G_CALLBACK(wnd_on_import_hex_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(import_action));
   g_object_unref(import_action);

   wnd->export_hex_action = g_simple_action_new("export-hex", NULL);
   g_signal_connect(wnd->export_hex_action, "activate",
                    G_CALLBACK(wnd_on_export_hex_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->export_hex_action));
   g_object_unref(wnd->export_hex_action);

   wnd->save_report_action = g_simple_action_new("save-report", NULL);
   g_signal_connect(wnd->save_report_action, "activate",
                    G_CALLBACK(wnd_on_save_report_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->save_report_action));
   g_object_unref(wnd->save_report_action);

   wnd->compare_file_action = g_simple_action_new("compare-file", NULL);
   g_signal_connect(wnd->compare_file_action, "activate",
                    G_CALLBACK(wnd_on_compare_file_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->compare_file_action));
   g_object_unref(wnd->compare_file_action);

   wnd->compare_display_action = g_simple_action_new("compare-display", NULL);
   g_signal_connect(wnd->compare_display_action, "activate",
                    G_CALLBACK(wnd_on_compare_display_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->compare_display_action));
   g_object_unref(wnd->compare_display_action);

   wnd->ignore_errors_action = g_simple_action_new_stateful(
      "ignore-errors", NULL, g_variant_new_boolean(FALSE));
   g_signal_connect(wnd->ignore_errors_action, "change-state",
                    G_CALLBACK(wnd_on_ignore_errors_state), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->ignore_errors_action));
   g_object_unref(wnd->ignore_errors_action);

   wnd->ignore_read_only_action = g_simple_action_new_stateful(
      "ignore-read-only", NULL, g_variant_new_boolean(FALSE));
   g_signal_connect(wnd->ignore_read_only_action, "change-state",
                    G_CALLBACK(wnd_on_ignore_read_only_state), wnd);
   g_action_map_add_action(G_ACTION_MAP(window),
                           G_ACTION(wnd->ignore_read_only_action));
   g_object_unref(wnd->ignore_read_only_action);

   wnd->show_reserved_action = g_simple_action_new_stateful(
      "show-reserved", NULL, g_variant_new_boolean(FALSE));
   g_signal_connect(wnd->show_reserved_action, "change-state",
                    G_CALLBACK(wnd_on_show_reserved_state), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->show_reserved_action));
   g_object_unref(wnd->show_reserved_action);

   GSimpleAction* recent_action = g_simple_action_new("open-recent", G_VARIANT_TYPE_STRING);
   g_signal_connect(recent_action, "activate", G_CALLBACK(wnd_on_open_recent), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(recent_action));
   g_object_unref(recent_action);
   GSimpleAction* no_recent_action = g_simple_action_new("no-recent", NULL);
   g_simple_action_set_enabled(no_recent_action, FALSE);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(no_recent_action));
   g_object_unref(no_recent_action);

   GSimpleAction* find_action = g_simple_action_new("find", NULL);
   g_signal_connect(find_action, "activate", G_CALLBACK(wnd_on_find_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(find_action));
   g_object_unref(find_action);

   GSimpleAction* shortcuts_action = g_simple_action_new("shortcuts", NULL);
   g_signal_connect(shortcuts_action, "activate",
                    G_CALLBACK(wnd_on_shortcuts_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(shortcuts_action));
   g_object_unref(shortcuts_action);

   GSimpleAction* about_action = g_simple_action_new("about", NULL);
   g_signal_connect(about_action, "activate", G_CALLBACK(wnd_on_about_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(about_action));
   g_object_unref(about_action);

   const char* open_accels[] = {"<Control>o", NULL};
   const char* save_accels[] = {"<Control>s", NULL};
   const char* save_as_accels[] = {"<Control><Shift>s", NULL};
   const char* undo_accels[] = {"<Control>z", NULL};
   const char* redo_accels[] = {"<Control><Shift>z", NULL};
   gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.open", open_accels);
   gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.save", save_accels);
   gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.save-as",
                                         save_as_accels);
   gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.undo", undo_accels);
   gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.redo", redo_accels);
   const char* find_accels[] = {"<Control>f", NULL};
   gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.find", find_accels);
   const char* shortcuts_accels[] = {"<Control>question", NULL};
   gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.shortcuts",
                                         shortcuts_accels);

   //header bar
   GtkWidget* header = adw_header_bar_new();
   wnd->window_title = ADW_WINDOW_TITLE(adw_window_title_new(_("EDID Editor"), _("Display identification data")));
   adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), GTK_WIDGET(wnd->window_title));

   GtkWidget* btn_open = gtk_button_new_with_mnemonic(_("_Open"));
   wnd->open_button = btn_open;
   gtk_actionable_set_action_name(GTK_ACTIONABLE(btn_open), "win.open");
   gtk_widget_set_tooltip_text(btn_open, _("Open an EDID file (Ctrl+O)"));
   adw_header_bar_pack_start(ADW_HEADER_BAR(header), btn_open);

   GtkWidget* btn_save = gtk_button_new_with_mnemonic(_("_Save"));
   wnd->save_button = btn_save;
   gtk_actionable_set_action_name(GTK_ACTIONABLE(btn_save), "win.save");
   gtk_widget_set_tooltip_text(btn_save, _("Save changes (Ctrl+S)"));
   gtk_widget_add_css_class(btn_save, "suggested-action");
   adw_header_bar_pack_end(ADW_HEADER_BAR(header), btn_save);

   GMenu* primary_menu = g_menu_new();
   GMenu* open_section = g_menu_new();
   g_menu_append(open_section, _("Open…"), "win.open");
   wnd->recent_menu = g_menu_new();
   g_menu_append_submenu(open_section, _("Open Recent"), G_MENU_MODEL(wnd->recent_menu));
   g_menu_append(open_section, _("Open from Display…"), "win.open-display");
   g_menu_append(open_section, _("Import Hex…"), "win.import-hex");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(open_section));
   g_object_unref(open_section);
   GMenu* save_section = g_menu_new();
   g_menu_append(save_section, _("Save As…"), "win.save-as");
   g_menu_append(save_section, _("Export Hex…"), "win.export-hex");
   g_menu_append(save_section, _("Save Report…"), "win.save-report");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(save_section));
   g_object_unref(save_section);
   GMenu* compare_section = g_menu_new();
   g_menu_append(compare_section, _("Compare with File…"), "win.compare-file");
   g_menu_append(compare_section, _("Compare with Display…"), "win.compare-display");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(compare_section));
   g_object_unref(compare_section);
   GMenu* edit_section = g_menu_new();
   g_menu_append(edit_section, _("Undo"), "win.undo");
   g_menu_append(edit_section, _("Redo"), "win.redo");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(edit_section));
   g_object_unref(edit_section);
   GMenu* option_section = g_menu_new();
   g_menu_append(option_section, _("Ignore EDID Errors"), "win.ignore-errors");
   g_menu_append(option_section, _("Edit Read-Only Fields"), "win.ignore-read-only");
   g_menu_append(option_section, _("Show Reserved Fields"), "win.show-reserved");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(option_section));
   g_object_unref(option_section);
   GMenu* help_section = g_menu_new();
   g_menu_append(help_section, _("EDID Log"), "win.show-log");
   g_menu_append(help_section, _("Keyboard Shortcuts"), "win.shortcuts");
   g_menu_append(help_section, _("About EDID Editor"), "win.about");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(help_section));
   g_object_unref(help_section);
   GtkWidget* btn_menu = gtk_menu_button_new();
   gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(btn_menu), "open-menu-symbolic");
   gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(btn_menu), G_MENU_MODEL(primary_menu));
   gtk_menu_button_set_primary(GTK_MENU_BUTTON(btn_menu), TRUE);
   gtk_widget_set_tooltip_text(btn_menu, _("Main menu"));
   g_object_unref(primary_menu);
   adw_header_bar_pack_end(ADW_HEADER_BAR(header), btn_menu);

   GtkWidget* btn_sidebar = gtk_button_new_from_icon_name("sidebar-show-symbolic");
   wnd->sidebar_button = btn_sidebar;
   gtk_widget_set_tooltip_text(btn_sidebar, _("Show groups"));
   gtk_accessible_update_property(GTK_ACCESSIBLE(btn_sidebar),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Show groups"),
                                  -1);
   gtk_widget_set_visible(btn_sidebar, FALSE);
   g_signal_connect(btn_sidebar, "clicked", G_CALLBACK(wnd_on_toggle_sidebar), wnd);
   adw_header_bar_pack_start(ADW_HEADER_BAR(header), btn_sidebar);

   //block tree: list view with lazy expander rows
   GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
   g_signal_connect(factory, "setup", G_CALLBACK(tree_name_setup), wnd);
   g_signal_connect(factory, "bind",  G_CALLBACK(tree_name_bind),  NULL);
   g_signal_connect(factory, "unbind", G_CALLBACK(tree_name_unbind), NULL);

   //selection: refresh the field list on change
   wnd->tree_filter = gtk_custom_filter_new(tree_filter_match, wnd, NULL);
   wnd->tree_filtered = gtk_filter_list_model_new(
      NULL, GTK_FILTER(g_object_ref(wnd->tree_filter)));
   g_signal_connect(wnd->tree_filtered, "items-changed",
                    G_CALLBACK(tree_filter_items_changed), wnd);
   wnd->tree_sel = GTK_SINGLE_SELECTION(gtk_single_selection_new(
      G_LIST_MODEL(g_object_ref(wnd->tree_filtered))));
   gtk_single_selection_set_autoselect(wnd->tree_sel, FALSE);
   //the overview is shown with no group selected
   gtk_single_selection_set_can_unselect(wnd->tree_sel, TRUE);
   wnd->tree = GTK_LIST_VIEW(gtk_list_view_new(
      GTK_SELECTION_MODEL(wnd->tree_sel), factory));
   gtk_widget_add_css_class(GTK_WIDGET(wnd->tree), "navigation-sidebar");
   g_signal_connect(wnd->tree_sel, "selection-changed",
                    G_CALLBACK(wnd_on_tree_select), wnd);

   //only the groups of the selected block's type are offered
   GMenu* add_menu = g_menu_new();
   static const char* const add_items[][2] = {
      {N_("LPCM Audio Block"), "win.add-cta-group::audio-lpcm"},
      {N_("Extended Audio Block"), "win.add-cta-group::audio-extended"},
      {N_("Video Block"), "win.add-cta-group::video"},
      {N_("Detailed Timing"), "win.add-cta-group::timing"},
      {N_("DisplayID Data Block"), "win.add-displayid-group"},
   };
   for (const auto& spec : add_items) {
      GMenuItem* item = g_menu_item_new(_(spec[0]), spec[1]);
      g_menu_item_set_attribute(item, "hidden-when", "s", "action-disabled");
      g_menu_append_item(add_menu, item);
      g_object_unref(item);
   }

   GMenu* group_menu_model = g_menu_new();
   GMenu* timing_section = g_menu_new();
   GMenuItem* prefer_item = g_menu_item_new(_("Make Preferred"), "win.make-preferred");
   g_menu_item_set_attribute(prefer_item, "hidden-when", "s", "action-disabled");
   g_menu_append_item(timing_section, prefer_item);
   g_object_unref(prefer_item);
   g_menu_append_section(group_menu_model, NULL, G_MENU_MODEL(timing_section));
   g_object_unref(timing_section);
   g_menu_append_submenu(group_menu_model, _("Add"), G_MENU_MODEL(add_menu));
   g_menu_append(group_menu_model, _("Duplicate"), "win.duplicate-group");
   g_menu_append(group_menu_model, _("Move Up"), "win.move-group-up");
   g_menu_append(group_menu_model, _("Move Down"), "win.move-group-down");
   g_menu_append(group_menu_model, _("Delete"), "win.delete-group");
   wnd->group_menu = GTK_POPOVER_MENU(
      gtk_popover_menu_new_from_model(G_MENU_MODEL(group_menu_model)));
   gtk_widget_set_parent(GTK_WIDGET(wnd->group_menu), GTK_WIDGET(wnd->tree));
   //a list view doesn't unparent children it didn't add itself
   g_signal_connect(wnd->tree, "destroy",
                    G_CALLBACK(+[](GtkWidget*, gpointer menu) {
                       gtk_widget_unparent(GTK_WIDGET(menu));
                    }), wnd->group_menu);
   gtk_popover_set_has_arrow(GTK_POPOVER(wnd->group_menu), FALSE);
   g_object_unref(group_menu_model);

   GtkEventController* tree_keys = gtk_event_controller_key_new();
   g_signal_connect(tree_keys, "key-pressed", G_CALLBACK(wnd_on_tree_key), wnd);
   gtk_widget_add_controller(GTK_WIDGET(wnd->tree), tree_keys);

   GtkWidget* tree_scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tree_scroll), GTK_WIDGET(wnd->tree));
   gtk_widget_set_hexpand(tree_scroll, TRUE);
   gtk_widget_set_vexpand(tree_scroll, TRUE);

   wnd->tree_search = GTK_SEARCH_ENTRY(gtk_search_entry_new());
   gtk_search_entry_set_placeholder_text(wnd->tree_search, _("Search groups"));
   gtk_accessible_update_property(GTK_ACCESSIBLE(wnd->tree_search),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  _("Search groups"), -1);
   gtk_widget_set_margin_start(GTK_WIDGET(wnd->tree_search), 12);
   gtk_widget_set_margin_end(GTK_WIDGET(wnd->tree_search), 12);
   gtk_widget_set_margin_top(GTK_WIDGET(wnd->tree_search), 12);
   gtk_widget_set_margin_bottom(GTK_WIDGET(wnd->tree_search), 6);
   g_signal_connect(wnd->tree_search, "search-changed",
                    G_CALLBACK(tree_search_changed), wnd);
   //Escape clears the search; the handler ignores its first argument
   g_signal_connect(wnd->tree_search, "stop-search", G_CALLBACK(tree_search_clear), wnd);

   GtkWidget* search_empty = adw_status_page_new();
   adw_status_page_set_icon_name(ADW_STATUS_PAGE(search_empty),
                                 "edit-find-symbolic");
   adw_status_page_set_title(ADW_STATUS_PAGE(search_empty),
                             _("No matching groups"));
   adw_status_page_set_description(ADW_STATUS_PAGE(search_empty),
                                   _("Try a different search."));
   GtkWidget* clear_search = gtk_button_new_with_mnemonic(_("_Clear Search"));
   gtk_widget_set_halign(clear_search, GTK_ALIGN_CENTER);
   g_signal_connect(clear_search, "clicked", G_CALLBACK(tree_search_clear), wnd);
   adw_status_page_set_child(ADW_STATUS_PAGE(search_empty), clear_search);

   wnd->sidebar_stack = GTK_STACK(gtk_stack_new());
   gtk_stack_add_named(wnd->sidebar_stack, tree_scroll, "tree");
   gtk_stack_add_named(wnd->sidebar_stack, search_empty, "empty");
   gtk_stack_set_visible_child_name(wnd->sidebar_stack, "tree");
   gtk_widget_set_vexpand(GTK_WIDGET(wnd->sidebar_stack), TRUE);

   wnd->overview_list = GTK_LIST_BOX(gtk_list_box_new());
   gtk_list_box_set_selection_mode(wnd->overview_list, GTK_SELECTION_SINGLE);
   gtk_widget_add_css_class(GTK_WIDGET(wnd->overview_list), "navigation-sidebar");
   GtkWidget* overview_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
   gtk_box_append(GTK_BOX(overview_row), gtk_image_new_from_icon_name("view-grid-symbolic"));
   GtkWidget* overview_label = gtk_label_new(_("Overview"));
   gtk_label_set_xalign(GTK_LABEL(overview_label), 0.0);
   gtk_box_append(GTK_BOX(overview_row), overview_label);
   gtk_list_box_append(wnd->overview_list, overview_row);
   g_signal_connect(wnd->overview_list, "row-selected",
                    G_CALLBACK(wnd_on_overview_selected), wnd);

   GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_box_append(GTK_BOX(sidebar), GTK_WIDGET(wnd->overview_list));
   gtk_box_append(GTK_BOX(sidebar), GTK_WIDGET(wnd->tree_search));
   gtk_box_append(GTK_BOX(sidebar), GTK_WIDGET(wnd->sidebar_stack));

   GtkWidget* group_toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_widget_add_css_class(group_toolbar, "toolbar");
   gtk_widget_set_margin_start(group_toolbar, 12);
   gtk_widget_set_margin_end(group_toolbar, 12);
   gtk_widget_set_margin_top(group_toolbar, 6);
   gtk_widget_set_margin_bottom(group_toolbar, 12);
   GtkWidget* add_button = gtk_menu_button_new();
   wnd->add_button = add_button;
   gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(add_button), "list-add-symbolic");
   gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(add_button), G_MENU_MODEL(add_menu));
   gtk_widget_set_tooltip_text(add_button, _("Add a group"));
   gtk_accessible_update_property(GTK_ACCESSIBLE(add_button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Add a group"), -1);
   gtk_box_append(GTK_BOX(group_toolbar), add_button);
   g_object_unref(add_menu);

   struct group_button {
      const char* icon;
      const char* label;
      const char* action;
   };
   static const group_button group_buttons[] = {
      {"edit-copy-symbolic", N_("Duplicate group (Ctrl+D)"), "win.duplicate-group"},
      {"go-up-symbolic", N_("Move group up (Alt+Up)"), "win.move-group-up"},
      {"go-down-symbolic", N_("Move group down (Alt+Down)"), "win.move-group-down"},
      {"user-trash-symbolic", N_("Delete group (Delete)"), "win.delete-group"},
   };
   for (const group_button& spec : group_buttons) {
      GtkWidget* button = gtk_button_new_from_icon_name(spec.icon);
      gtk_actionable_set_action_name(GTK_ACTIONABLE(button), spec.action);
      gtk_widget_set_tooltip_text(button, _(spec.label));
      gtk_accessible_update_property(GTK_ACCESSIBLE(button),
                                     GTK_ACCESSIBLE_PROPERTY_LABEL, _(spec.label), -1);
      gtk_box_append(GTK_BOX(group_toolbar), button);
   }
   gtk_box_append(GTK_BOX(sidebar), group_toolbar);

   GtkWidget* right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

   GtkWidget* editor_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
   gtk_widget_set_margin_start(editor_heading, 18);
   gtk_widget_set_margin_end(editor_heading, 18);
   gtk_widget_set_margin_top(editor_heading, 18);
   gtk_widget_set_margin_bottom(editor_heading, 12);

   GtkWidget* heading_titles = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
   gtk_widget_set_hexpand(heading_titles, TRUE);
   gtk_widget_set_valign(heading_titles, GTK_ALIGN_CENTER);
   wnd->group_title = GTK_LABEL(gtk_label_new(_("Select a group")));
   gtk_label_set_xalign(wnd->group_title, 0.0);
   gtk_label_set_ellipsize(wnd->group_title, PANGO_ELLIPSIZE_END);
   gtk_widget_add_css_class(GTK_WIDGET(wnd->group_title), "title-2");
   gtk_box_append(GTK_BOX(heading_titles), GTK_WIDGET(wnd->group_title));
   wnd->group_subtitle = GTK_LABEL(gtk_label_new(NULL));
   gtk_label_set_xalign(wnd->group_subtitle, 0.0);
   gtk_label_set_ellipsize(wnd->group_subtitle, PANGO_ELLIPSIZE_END);
   gtk_widget_add_css_class(GTK_WIDGET(wnd->group_subtitle), "caption");
   gtk_widget_add_css_class(GTK_WIDGET(wnd->group_subtitle), "dim-label");
   gtk_widget_set_visible(GTK_WIDGET(wnd->group_subtitle), FALSE);
   gtk_box_append(GTK_BOX(heading_titles), GTK_WIDGET(wnd->group_subtitle));
   gtk_box_append(GTK_BOX(editor_heading), heading_titles);

   wnd->fields = GTK_FLOW_BOX(gtk_flow_box_new());
   gtk_flow_box_set_selection_mode(wnd->fields, GTK_SELECTION_NONE);
   gtk_flow_box_set_homogeneous(wnd->fields, TRUE);
   gtk_flow_box_set_min_children_per_line(wnd->fields, 1);
   gtk_flow_box_set_max_children_per_line(wnd->fields, 1);
   gtk_flow_box_set_column_spacing(wnd->fields, 12);
   gtk_flow_box_set_row_spacing(wnd->fields, 12);
   gtk_widget_add_css_class(GTK_WIDGET(wnd->fields), "card-grid");
   gtk_widget_set_valign(GTK_WIDGET(wnd->fields), GTK_ALIGN_START);
   g_object_set_data(G_OBJECT(wnd->fields), "wxedid-wnd", wnd);

   GtkWidget* fields_clamp = adw_clamp_new();
   adw_clamp_set_maximum_size(ADW_CLAMP(fields_clamp), 1100);
   adw_clamp_set_tightening_threshold(ADW_CLAMP(fields_clamp), 760);
   gtk_widget_set_margin_start(fields_clamp, 18);
   gtk_widget_set_margin_end(fields_clamp, 18);
   gtk_widget_set_margin_bottom(fields_clamp, 18);
   GtkWidget* fields_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
   gtk_box_append(GTK_BOX(fields_box), GTK_WIDGET(wnd->fields));
   wnd->reserved_note = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_widget_set_halign(wnd->reserved_note, GTK_ALIGN_CENTER);
   wnd->reserved_label = GTK_LABEL(gtk_label_new(NULL));
   gtk_widget_add_css_class(GTK_WIDGET(wnd->reserved_label), "caption");
   gtk_widget_add_css_class(GTK_WIDGET(wnd->reserved_label), "dim-label");
   gtk_box_append(GTK_BOX(wnd->reserved_note), GTK_WIDGET(wnd->reserved_label));
   GtkWidget* show_reserved = gtk_button_new_with_label(_("Show Reserved Fields"));
   gtk_widget_add_css_class(show_reserved, "flat");
   gtk_widget_add_css_class(show_reserved, "caption");
   gtk_actionable_set_action_name(GTK_ACTIONABLE(show_reserved), "win.show-reserved");
   gtk_box_append(GTK_BOX(wnd->reserved_note), show_reserved);
   gtk_widget_set_visible(wnd->reserved_note, FALSE);
   gtk_box_append(GTK_BOX(fields_box), wnd->reserved_note);
   adw_clamp_set_child(ADW_CLAMP(fields_clamp), fields_box);

   GtkWidget* fields_scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(fields_scroll), fields_clamp);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(fields_scroll),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
   gtk_widget_set_vexpand(fields_scroll, TRUE);

   wnd->timing = new wxedid_timing{};
   wnd->timing->wnd = wnd;
   wnd->timing->history_index = static_cast<size_t>(-1);
   wnd->timing->page = timing_create_page(wnd->timing);

   wnd->raw_view = GTK_TEXT_VIEW(gtk_text_view_new());
   gtk_text_view_set_editable(wnd->raw_view, FALSE);
   gtk_text_view_set_cursor_visible(wnd->raw_view, FALSE);
   gtk_text_view_set_monospace(wnd->raw_view, TRUE);
   gtk_text_view_set_wrap_mode(wnd->raw_view, GTK_WRAP_NONE);
   gtk_text_view_set_left_margin(wnd->raw_view, 18);
   gtk_text_view_set_right_margin(wnd->raw_view, 18);
   gtk_text_view_set_top_margin(wnd->raw_view, 12);
   gtk_text_view_set_bottom_margin(wnd->raw_view, 12);
   gtk_accessible_update_property(GTK_ACCESSIBLE(wnd->raw_view),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  _("Selected group bytes"), -1);
   gtk_text_buffer_create_tag(gtk_text_view_get_buffer(wnd->raw_view), "field",
                              "background", "rgba(53,132,228,0.3)",
                              "weight", PANGO_WEIGHT_BOLD, NULL);
   gtk_text_buffer_create_tag(gtk_text_view_get_buffer(wnd->raw_view), "heading",
                              "weight", PANGO_WEIGHT_BOLD, NULL);
   //the bytes card hugs its lines; the page scrolls as a whole
   GtkWidget* raw_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_widget_add_css_class(raw_card, "card");
   gtk_widget_set_overflow(raw_card, GTK_OVERFLOW_HIDDEN);
   gtk_widget_set_valign(raw_card, GTK_ALIGN_START);
   gtk_widget_set_margin_start(raw_card, 18);
   gtk_widget_set_margin_end(raw_card, 18);
   gtk_widget_set_margin_top(raw_card, 6);
   gtk_widget_set_margin_bottom(raw_card, 18);
   gtk_box_append(GTK_BOX(raw_card), GTK_WIDGET(wnd->raw_view));
   gtk_widget_add_css_class(GTK_WIDGET(wnd->raw_view), "byte-view");
   wnd->raw_caption = GTK_LABEL(gtk_label_new(NULL));
   gtk_label_set_xalign(wnd->raw_caption, 0.0);
   gtk_label_set_ellipsize(wnd->raw_caption, PANGO_ELLIPSIZE_END);
   gtk_widget_add_css_class(GTK_WIDGET(wnd->raw_caption), "caption");
   gtk_widget_add_css_class(GTK_WIDGET(wnd->raw_caption), "dim-label");
   gtk_widget_set_margin_start(GTK_WIDGET(wnd->raw_caption), 18);
   gtk_widget_set_margin_end(GTK_WIDGET(wnd->raw_caption), 18);
   GtkWidget* raw_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_box_append(GTK_BOX(raw_page), GTK_WIDGET(wnd->raw_caption));
   gtk_box_append(GTK_BOX(raw_page), raw_card);
   GtkWidget* raw_scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(raw_scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(raw_scroll), raw_page);

   wnd->editor_stack = ADW_VIEW_STACK(adw_view_stack_new());
   adw_view_stack_set_hhomogeneous(wnd->editor_stack, FALSE);
   adw_view_stack_set_vhomogeneous(wnd->editor_stack, FALSE);
   adw_view_stack_add_titled_with_icon(wnd->editor_stack, fields_scroll,
                                       "fields", _("Fields"), "view-list-symbolic");
   wnd->timing_stack_page = adw_view_stack_add_titled_with_icon(
      wnd->editor_stack, wnd->timing->page,
      "timing", _("Timing"), "video-display-symbolic");
   adw_view_stack_add_titled_with_icon(wnd->editor_stack, raw_scroll,
                                       "bytes", _("Bytes"), "document-properties-symbolic");
   wnd->overview_bin = adw_bin_new();
   wnd->overview_page = adw_view_stack_add_named(wnd->editor_stack, wnd->overview_bin,
                                                 "overview");
   adw_view_stack_page_set_visible(wnd->overview_page, FALSE);
   gtk_widget_set_vexpand(GTK_WIDGET(wnd->editor_stack), TRUE);

   wnd->editor_switcher = adw_view_switcher_new();
   adw_view_switcher_set_policy(ADW_VIEW_SWITCHER(wnd->editor_switcher),
                                ADW_VIEW_SWITCHER_POLICY_WIDE);
   adw_view_switcher_set_stack(ADW_VIEW_SWITCHER(wnd->editor_switcher),
                               wnd->editor_stack);
   gtk_widget_set_visible(wnd->editor_switcher, FALSE);
   gtk_box_append(GTK_BOX(editor_heading), wnd->editor_switcher);
   gtk_box_append(GTK_BOX(right), editor_heading);
   gtk_box_append(GTK_BOX(right), GTK_WIDGET(wnd->editor_stack));

   wnd->log = gtk_text_buffer_new(NULL);

   GtkWidget* split_view = adw_overlay_split_view_new();
   wnd->split_view = ADW_OVERLAY_SPLIT_VIEW(split_view);
   adw_overlay_split_view_set_sidebar(wnd->split_view, sidebar);
   adw_overlay_split_view_set_content(wnd->split_view, right);
   adw_overlay_split_view_set_min_sidebar_width(wnd->split_view, 280.0);
   adw_overlay_split_view_set_max_sidebar_width(wnd->split_view, 340.0);
   adw_overlay_split_view_set_sidebar_width_fraction(wnd->split_view, 0.28);
   g_signal_connect(wnd->split_view, "notify::collapsed",
                    G_CALLBACK(wnd_on_split_collapsed), wnd);

   //Width ranges do not overlap: when several breakpoints match, the last
   //added wins. A field card is at least 240 px wide, the content has 18 px
   //margins, and the sidebar takes 28% of the width (280-340 px).
   struct width_range {
      double min;  //0: no lower bound
      double max;  //0: no upper bound
   };
   auto add_breakpoint = [&](width_range range, bool collapsed, bool narrow,
                             guint columns) {
      AdwBreakpointCondition* condition = NULL;
      if (range.min > 0) {
         condition = adw_breakpoint_condition_new_length(
            ADW_BREAKPOINT_CONDITION_MIN_WIDTH, range.min, ADW_LENGTH_UNIT_SP);
      }
      if (range.max > 0) {
         AdwBreakpointCondition* upper = adw_breakpoint_condition_new_length(
            ADW_BREAKPOINT_CONDITION_MAX_WIDTH, range.max, ADW_LENGTH_UNIT_SP);
         condition = (condition != NULL)
            ? adw_breakpoint_condition_new_and(condition, upper) : upper;
      }
      AdwBreakpoint* breakpoint = adw_breakpoint_new(condition);
      if (collapsed) {
         adw_breakpoint_add_setters(
            breakpoint,
            G_OBJECT(split_view), "collapsed", TRUE,
            G_OBJECT(split_view), "show-sidebar", FALSE,
            G_OBJECT(wnd->timing->drawing), "height-request", 220,
            NULL);
      }
      if (narrow) {
         adw_breakpoint_add_setters(
            breakpoint,
            G_OBJECT(editor_heading), "orientation", GTK_ORIENTATION_VERTICAL,
            G_OBJECT(wnd->timing->summary), "orientation", GTK_ORIENTATION_VERTICAL,
            NULL);
      }
      adw_breakpoint_add_setters(
         breakpoint,
         G_OBJECT(wnd->fields), "min-children-per-line", columns,
         G_OBJECT(wnd->fields), "max-children-per-line", columns,
         NULL);
      adw_application_window_add_breakpoint(ADW_APPLICATION_WINDOW(window), breakpoint);
   };
   add_breakpoint({0, 539}, true, true, 1U);      //phone: one column, stacked headings
   add_breakpoint({540, 700}, true, false, 2U);   //collapsed sidebar, two columns
   //701-809: sidebar shown, one column (no breakpoint)
   add_breakpoint({810, 1180}, false, false, 2U);
   add_breakpoint({1181, 0}, false, false, 3U);

   GtkWidget* empty_page = adw_status_page_new();
   adw_status_page_set_icon_name(ADW_STATUS_PAGE(empty_page), "video-display-symbolic");
   adw_status_page_set_title(ADW_STATUS_PAGE(empty_page), _("Open an EDID file"));
   adw_status_page_set_description(ADW_STATUS_PAGE(empty_page),
                                   _("Inspect and edit display identification data."));
   GtkWidget* empty_open = gtk_button_new_with_mnemonic(_("_Open an EDID File"));
   gtk_actionable_set_action_name(GTK_ACTIONABLE(empty_open), "win.open");
   gtk_widget_add_css_class(empty_open, "suggested-action");
   gtk_widget_add_css_class(empty_open, "pill");
   gtk_widget_set_halign(empty_open, GTK_ALIGN_CENTER);
   GtkWidget* empty_display = gtk_button_new_with_mnemonic(_("Open from _Display"));
   gtk_actionable_set_action_name(GTK_ACTIONABLE(empty_display), "win.open-display");
   gtk_widget_add_css_class(empty_display, "pill");
   gtk_widget_set_halign(empty_display, GTK_ALIGN_CENTER);
   wnd->recent_group = adw_preferences_group_new();
   adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(wnd->recent_group), _("Recent Files"));
   wnd->recent_list = GTK_LIST_BOX(gtk_list_box_new());
   gtk_list_box_set_selection_mode(wnd->recent_list, GTK_SELECTION_NONE);
   gtk_widget_add_css_class(GTK_WIDGET(wnd->recent_list), "boxed-list");
   adw_preferences_group_add(ADW_PREFERENCES_GROUP(wnd->recent_group),
                             GTK_WIDGET(wnd->recent_list));
   gtk_widget_set_margin_top(wnd->recent_group, 18);
   GtkWidget* empty_actions = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
   gtk_box_append(GTK_BOX(empty_actions), empty_open);
   gtk_box_append(GTK_BOX(empty_actions), empty_display);
   gtk_box_append(GTK_BOX(empty_actions), wnd->recent_group);
   GtkWidget* empty_clamp = adw_clamp_new();
   adw_clamp_set_maximum_size(ADW_CLAMP(empty_clamp), 480);
   adw_clamp_set_child(ADW_CLAMP(empty_clamp), empty_actions);
   adw_status_page_set_child(ADW_STATUS_PAGE(empty_page), empty_clamp);

   wnd->content_stack = GTK_STACK(gtk_stack_new());
   gtk_stack_add_named(wnd->content_stack, empty_page, "empty");
   gtk_stack_add_named(wnd->content_stack, split_view, "editor");
   gtk_stack_set_visible_child_name(wnd->content_stack, "empty");
   gtk_widget_set_vexpand(GTK_WIDGET(wnd->content_stack), TRUE);

   wnd->banner = ADW_BANNER(adw_banner_new(""));
   g_signal_connect(wnd->banner, "button-clicked",
                    G_CALLBACK(wnd_on_banner_details), wnd);

   wnd->source_banner = ADW_BANNER(adw_banner_new(""));
   adw_banner_set_button_label(wnd->source_banner, _("Save As…"));
   adw_banner_set_use_markup(wnd->source_banner, FALSE);
   g_signal_connect(wnd->source_banner, "button-clicked",
                    G_CALLBACK(wnd_on_source_banner), wnd);

   GtkWidget* body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_box_append(GTK_BOX(body), GTK_WIDGET(wnd->banner));
   gtk_box_append(GTK_BOX(body), GTK_WIDGET(wnd->source_banner));
   gtk_box_append(GTK_BOX(body), GTK_WIDGET(wnd->content_stack));

   wnd->toast_overlay = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
   adw_toast_overlay_set_child(wnd->toast_overlay, body);

   wnd->doc->GLog.SetSink(log_sink, wnd);
   wnd->doc->EDID.SetGuiLogPtr(&wnd->doc->GLog);

   GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_box_append(GTK_BOX(content), header);
   gtk_box_append(GTK_BOX(content), GTK_WIDGET(wnd->toast_overlay));
   gtk_widget_set_vexpand(GTK_WIDGET(wnd->toast_overlay), TRUE);

   GtkDropTarget* drop = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
   g_signal_connect(drop, "drop", G_CALLBACK(wnd_on_drop), wnd);
   gtk_widget_add_controller(content, GTK_EVENT_CONTROLLER(drop));

   adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), content);
   wnd->recent_changed = g_signal_connect(gtk_recent_manager_get_default(), "changed",
                                          G_CALLBACK(wnd_on_recent_changed), wnd);
   wnd_refresh_recent(wnd);
   wnd_load_state(wnd);
   wnd_update_history_state(wnd);
   wnd_update_document_ui(wnd);
   gtk_window_present(GTK_WINDOW(window));
}
