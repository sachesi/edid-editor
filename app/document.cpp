/***************************************************************
 * Name:      document.cpp
 * Purpose:   opening, saving and exporting EDIDs, recent files
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include "window_private.h"

//------------
// recent files and window state
static const char RECENT_GROUP[] = "edid-editor";

static void wnd_add_recent(const char* path, bool hex) {
   if (g_str_has_prefix(path, DRM_ROOT)) return;
   char* uri = g_filename_to_uri(path, NULL, NULL);
   if (uri == NULL) return;
   const char* groups[] = {RECENT_GROUP, NULL};
   GtkRecentData data = {};
   data.mime_type = const_cast<char*>(hex ? "text/plain" : "application/octet-stream");
   data.app_name = const_cast<char*>("edid-editor");
   data.app_exec = const_cast<char*>("edid-editor %f");
   data.groups = const_cast<char**>(groups);
   gtk_recent_manager_add_full(gtk_recent_manager_get_default(), uri, &data);
   g_free(uri);
}

//local files this application opened, newest first
static std::vector<std::string> recent_paths(size_t limit) {
   std::vector<std::pair<gint64, std::string>> found;
   GList* items = gtk_recent_manager_get_items(gtk_recent_manager_get_default());
   for (GList* node = items; node != NULL; node = node->next) {
      GtkRecentInfo* info = static_cast<GtkRecentInfo*>(node->data);
      if (gtk_recent_info_has_group(info, RECENT_GROUP) && gtk_recent_info_is_local(info) &&
          gtk_recent_info_exists(info)) {
         char* path = g_filename_from_uri(gtk_recent_info_get_uri(info), NULL, NULL);
         if (path != NULL) {
            GDateTime* modified = gtk_recent_info_get_modified(info);
            found.emplace_back((modified != NULL) ? g_date_time_to_unix(modified) : 0, path);
            g_free(path);
         }
      }
   }
   g_list_free_full(items, (GDestroyNotify) gtk_recent_info_unref);
   std::stable_sort(found.begin(), found.end(),
                    [](const std::pair<gint64, std::string>& a,
                       const std::pair<gint64, std::string>& b) { return a.first > b.first; });
   std::vector<std::string> paths;
   for (const auto& entry : found) {
      if (paths.size() >= limit) break;
      paths.push_back(entry.second);
   }
   return paths;
}

static void wnd_on_recent_open(GtkButton* button, gpointer user_data) {
   const char* path = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "path"));
   if (path != NULL) wnd_request_open_source(static_cast<wxedid_wnd*>(user_data), 0, path);
}

void wnd_refresh_recent(wxedid_wnd* wnd) {
   std::vector<std::string> paths = recent_paths(6);
   if (wnd->recent_list != NULL) {
      gtk_list_box_remove_all(wnd->recent_list);
      for (const std::string& path : paths) {
         char* name = g_path_get_basename(path.c_str());
         char* folder = g_path_get_dirname(path.c_str());
         char* display = g_filename_display_name(folder);
         const char* home = g_get_home_dir();
         size_t home_length = strlen(home);
         if ((home_length > 1) && (0 == strncmp(display, home, home_length)) &&
             ((display[home_length] == '/') || (display[home_length] == 0))) {
            char* shortened = g_strconcat("~", display + home_length, NULL);
            g_free(display);
            display = shortened;
         }
         GtkWidget* row = adw_action_row_new();
         adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
         adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), name);
         adw_action_row_set_subtitle(ADW_ACTION_ROW(row), display);
         GtkWidget* open = gtk_button_new_from_icon_name("go-next-symbolic");
         gtk_widget_add_css_class(open, "flat");
         gtk_widget_set_valign(open, GTK_ALIGN_CENTER);
         gtk_widget_set_tooltip_text(open, "Open");
         g_object_set_data_full(G_OBJECT(open), "path", g_strdup(path.c_str()), g_free);
         g_signal_connect(open, "clicked", G_CALLBACK(wnd_on_recent_open), wnd);
         adw_action_row_add_suffix(ADW_ACTION_ROW(row), open);
         adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), open);
         gtk_list_box_append(wnd->recent_list, row);
         g_free(display);
         g_free(folder);
         g_free(name);
      }
      gtk_widget_set_visible(wnd->recent_group, ! paths.empty());
   }
   if (wnd->recent_menu != NULL) {
      g_menu_remove_all(wnd->recent_menu);
      for (const std::string& path : paths) {
         char* name = g_path_get_basename(path.c_str());
         GMenuItem* item = g_menu_item_new(name, NULL);
         g_menu_item_set_action_and_target_value(item, "win.open-recent",
                                                 g_variant_new_string(path.c_str()));
         g_menu_append_item(wnd->recent_menu, item);
         g_object_unref(item);
         g_free(name);
      }
      if (paths.empty()) g_menu_append(wnd->recent_menu, "No Recent Files", "win.no-recent");
   }
}

void wnd_on_recent_changed(GtkRecentManager*, gpointer user_data) {
   wnd_refresh_recent(static_cast<wxedid_wnd*>(user_data));
}

void wnd_on_open_recent(GSimpleAction*, GVariant* parameter, gpointer user_data) {
   wnd_request_open_source(static_cast<wxedid_wnd*>(user_data), 0,
                           g_variant_get_string(parameter, NULL));
}

static char* state_file_path() {
   return g_build_filename(g_get_user_state_dir(), "edid-editor", "state.ini", NULL);
}

void wnd_load_state(wxedid_wnd* wnd) {
   char* path = state_file_path();
   GKeyFile* state = g_key_file_new();
   if (g_key_file_load_from_file(state, path, G_KEY_FILE_NONE, NULL)) {
      int width = g_key_file_get_integer(state, "window", "width", NULL);
      int height = g_key_file_get_integer(state, "window", "height", NULL);
      if ((width >= 360) && (height >= 294)) {
         gtk_window_set_default_size(wnd->window, width, height);
      }
      if (g_key_file_get_boolean(state, "window", "maximized", NULL)) {
         gtk_window_maximize(wnd->window);
      }
      if (g_key_file_get_boolean(state, "fields", "show-reserved", NULL)) {
         g_action_change_state(G_ACTION(wnd->show_reserved_action),
                               g_variant_new_boolean(TRUE));
      }
   }
   g_key_file_free(state);
   g_free(path);
}

void wnd_save_state(wxedid_wnd* wnd) {
   GKeyFile* state = g_key_file_new();
   int width = 0;
   int height = 0;
   gtk_window_get_default_size(wnd->window, &width, &height);
   g_key_file_set_integer(state, "window", "width", width);
   g_key_file_set_integer(state, "window", "height", height);
   g_key_file_set_boolean(state, "window", "maximized", gtk_window_is_maximized(wnd->window));
   g_key_file_set_boolean(state, "fields", "show-reserved", wnd->show_reserved);
   char* path = state_file_path();
   char* folder = g_path_get_dirname(path);
   if (g_mkdir_with_parents(folder, 0700) == 0) {
      g_key_file_save_to_file(state, path, NULL);
   }
   g_free(folder);
   g_free(path);
   g_key_file_free(state);
}

static void wnd_offer_retry(wxedid_wnd* wnd) {
   if (wnd->doc->EDID.b_ERR_Ignore || wnd->source_path.empty()) return;
   adw_banner_set_button_label(wnd->banner, "Open Anyway");
   adw_banner_set_revealed(wnd->banner, TRUE);
   wnd->banner_offers_retry = true;
}

static void wnd_load_bytes(wxedid_wnd* wnd, const char* path,
                           const u8_t* data, size_t size, bool hex_source) {
   //a pending rebuild refers to groups that loading may release
   wnd_flush_refresh(wnd);
   edid_load_result loaded = edid_load(wnd->doc->EDID, data, size, path, wnd->doc->GLog);
   if (loaded.rejected) {
      if (loaded.can_retry) wnd_offer_retry(wnd);
      return;
   }
   bool base_ok = loaded.opened;
   bool extension_failed = loaded.extension_failed;
   bool partial = loaded.partial;
   bool block_count_adjusted = loaded.count_adjusted;

   wnd->loaded = base_ok;
   wnd->dirty = false;
   wnd->source_writable = false;
   wnd->document_hex = hex_source;
   wnd->invalid_fields = 0;
   wnd_clear_history(wnd);
   wnd->saved_history_position = block_count_adjusted ? -1 : 0;
   if (base_ok) {
      snprintf(wnd->doc->path, sizeof(wnd->doc->path), "%s", path);
      wnd->source_writable = ! hex_source && (g_access(path, W_OK) == 0);
      wnd_add_recent(path, hex_source);
      gtk_stack_set_visible_child_name(wnd->content_stack, "editor");
      wnd->overview_shown = false;
      wnd_rebuild_tree(wnd);
      wnd_show_overview(wnd);
   } else {
      wnd->doc->path[0] = 0;
      gtk_stack_set_visible_child_name(wnd->content_stack, "empty");
   }
   wnd_update_history_state(wnd);
   wnd_update_document_ui(wnd);
   wnd->load_had_errors = ! base_ok || extension_failed || partial ||
                          block_count_adjusted;
   if (! base_ok || extension_failed) wnd_offer_retry(wnd);
}

bool path_is_hex_text(const char* path) {
   char* folded = g_utf8_casefold(path, -1);
   bool hex = g_str_has_suffix(folded, ".hex") || g_str_has_suffix(folded, ".txt");
   g_free(folded);
   return hex;
}

static void wnd_read_file(wxedid_wnd* wnd, const char* path, bool hex) {
   wnd_clear_feedback(wnd);
   wnd->source_path = path;
   wnd->source_hex = hex;
   wnd->load_had_errors = true;

   if (hex) {
      GStatBuf info;
      if ((g_stat(path, &info) == 0) && (info.st_size > 65536)) {
         char msg[1400];
         snprintf(msg, sizeof(msg),
                  "[E!] Couldn’t import %s: it is too large to be EDID hex text. "
                  "Choose another file.", path);
         wnd->doc->GLog.DoLog(msg);
         return;
      }
      char* contents = NULL;
      gsize length = 0;
      GError* error = NULL;
      if (! g_file_get_contents(path, &contents, &length, &error)) {
         char msg[1400];
         snprintf(msg, sizeof(msg),
                  "[E!] Couldn’t read %s: %s. Check the file, then try again.",
                  path, error->message);
         wnd->doc->GLog.DoLog(msg);
         g_error_free(error);
         return;
      }
      std::vector<u8_t> bytes;
      std::string problem;
      bool decoded = edid_hex_decode(contents, length, bytes, problem);
      g_free(contents);
      if (! decoded) {
         char msg[1400];
         snprintf(msg, sizeof(msg),
                  "[E!] Couldn’t import %s: %s. Choose a file with EDID hex data.",
                  path, problem.c_str());
         wnd->doc->GLog.DoLog(msg);
         return;
      }
      wnd_load_bytes(wnd, path, bytes.data(), bytes.size(), true);
      return;
   }

   FILE* in = fopen(path, "rb");
   if (in == NULL) {
      char msg[1400];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t open %s: %s. Check its permissions, then try again.",
               path, strerror(errno));
      wnd->doc->GLog.DoLog(msg);
      return;
   }

   u8_t file_data[sizeof(edi_t) + 1] = {};
   size_t rd = fread(file_data, 1, sizeof(file_data), in);
   bool read_failed = (ferror(in) != 0);
   int read_errno = errno;
   fclose(in);

   if (read_failed) {
      char msg[1400];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t read %s: %s. Check the file, then try again.",
               path, strerror(read_errno));
      wnd->doc->GLog.DoLog(msg);
      return;
   }
   wnd_load_bytes(wnd, path, file_data, rd, false);
}

//notices logged while a file opens are kept for the Overview
void wnd_load_file(wxedid_wnd* wnd, const char* path, bool hex) {
   wnd->loading = true;
   wnd_read_file(wnd, path, hex);
   wnd->loading = false;
   if (wnd->loaded && wnd->overview_shown) wnd_refresh_overview(wnd);
}

void wnd_reload_source(wxedid_wnd* wnd) {
   std::string path = wnd->source_path;
   if (! path.empty()) wnd_load_file(wnd, path.c_str(), wnd->source_hex);
}

GListStore* file_filters(const char* name, const char* const* patterns) {
   GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
   GtkFileFilter* filter = gtk_file_filter_new();
   gtk_file_filter_set_name(filter, name);
   for (const char* const* pattern = patterns; *pattern != NULL; pattern++) {
      gtk_file_filter_add_suffix(filter, *pattern);
   }
   g_list_store_append(filters, filter);
   g_object_unref(filter);
   GtkFileFilter* all = gtk_file_filter_new();
   gtk_file_filter_set_name(all, "All files");
   gtk_file_filter_add_pattern(all, "*");
   g_list_store_append(filters, all);
   g_object_unref(all);
   return filters;
}

static void wnd_on_open_response(GObject* source, GAsyncResult* result,
                                 gpointer user_data) {
   GtkWindow* window = GTK_WINDOW(user_data);
   wxedid_wnd* wnd = (wxedid_wnd*)
      g_object_get_data(G_OBJECT(window), "wxedid-wnd");
   GError* error = NULL;
   GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);

   if (file != NULL) {
      if (wnd != NULL) {
         char* path = g_file_get_path(file);
         if (path != NULL) {
            bool import_hex = g_object_get_data(source, "wxedid-import-hex") != NULL;
            wnd_load_file(wnd, path, import_hex || path_is_hex_text(path));
            g_free(path);
         } else {
            wnd->doc->GLog.DoLog(
               "[E!] Couldn’t open the selected location: only local EDID files "
               "are supported. Choose a local file.");
         }
      }
      g_object_unref(file);
   } else if ((wnd != NULL) && (error != NULL) &&
              ! g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) &&
              ! g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED)) {
      char msg[1200];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t open an EDID file: %s. Try again or choose another file.",
               error->message);
      wnd->doc->GLog.DoLog(msg);
   }

   g_clear_error(&error);
   g_object_unref(window);
}

static void wnd_on_display_open(GtkButton* button, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   const char* path = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "path"));
   bool compare = g_object_get_data(G_OBJECT(button), "compare") != NULL;
   AdwDialog* dialog = ADW_DIALOG(gtk_widget_get_ancestor(GTK_WIDGET(button), ADW_TYPE_DIALOG));
   std::string source = (path != NULL) ? path : "";
   if (dialog != NULL) adw_dialog_close(dialog);
   if (source.empty()) return;
   if (compare) {
      wnd_present_compare(wnd, source.c_str(), false);
   } else {
      wnd_load_file(wnd, source.c_str(), false);
   }
}

static void wnd_present_display_dialog(wxedid_wnd* wnd, bool compare = false) {
   std::vector<edid_display> displays = edid_connected_displays(DRM_ROOT);
   if (displays.empty()) {
      AdwAlertDialog* alert = ADW_ALERT_DIALOG(adw_alert_dialog_new(
         "No display data found",
         "No connected display reports EDID data in /sys/class/drm."));
      adw_alert_dialog_add_response(alert, "close", "Close");
      adw_dialog_present(ADW_DIALOG(alert), GTK_WIDGET(wnd->window));
      return;
   }

   GtkWidget* group = adw_preferences_group_new();
   adw_preferences_group_set_description(ADW_PREFERENCES_GROUP(group),
      "The EDID is read from the display connection; the display itself is not changed.");
   for (const edid_display& display : displays) {
      GtkWidget* row = adw_action_row_new();
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), display.name.c_str());
      adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
      adw_action_row_set_subtitle(ADW_ACTION_ROW(row), display.connector.c_str());
      GtkWidget* open = gtk_button_new_from_icon_name("go-next-symbolic");
      gtk_widget_add_css_class(open, "flat");
      gtk_widget_set_valign(open, GTK_ALIGN_CENTER);
      gtk_widget_set_tooltip_text(open, compare ? "Compare" : "Open");
      g_object_set_data_full(G_OBJECT(open), "path", g_strdup(display.path.c_str()), g_free);
      if (compare) g_object_set_data(G_OBJECT(open), "compare", GINT_TO_POINTER(1));
      g_signal_connect(open, "clicked", G_CALLBACK(wnd_on_display_open), wnd);
      adw_action_row_add_suffix(ADW_ACTION_ROW(row), open);
      adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), open);
      adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), row);
   }
   GtkWidget* page = adw_preferences_page_new();
   adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(group));

   GtkWidget* view = adw_toolbar_view_new();
   adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(view), adw_header_bar_new());
   adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(view), page);

   AdwDialog* dialog = adw_dialog_new();
   adw_dialog_set_title(dialog, compare ? "Compare with Display" : "Open from Display");
   adw_dialog_set_content_width(dialog, 420);
   adw_dialog_set_child(dialog, view);
   adw_dialog_present(dialog, GTK_WIDGET(wnd->window));
}

static void wnd_present_open_dialog(wxedid_wnd* wnd, open_mode mode) {
   if (mode == OPEN_DISPLAY) {
      wnd_present_display_dialog(wnd);
      return;
   }
   bool import_hex = (mode == OPEN_HEX);
   GtkFileDialog* dialog = gtk_file_dialog_new();
   gtk_file_dialog_set_title(dialog, import_hex ? "Import EDID from hex"
                                                : "Open EDID file");
   gtk_file_dialog_set_accept_label(dialog, import_hex ? "Import" : "Open");
   static const char* const binary_patterns[] = {"bin", "hex", "txt", NULL};
   static const char* const hex_patterns[] = {"hex", "txt", NULL};
   GListStore* filters = file_filters(import_hex ? "Hex text" : "EDID files",
                                      import_hex ? hex_patterns : binary_patterns);
   gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
   g_object_unref(filters);
   if (import_hex) {
      g_object_set_data(G_OBJECT(dialog), "wxedid-import-hex", GINT_TO_POINTER(1));
   }
   gtk_file_dialog_open(dialog, wnd->window, NULL, wnd_on_open_response,
                        g_object_ref(wnd->window));
   g_object_unref(dialog);
}

static void wnd_on_discard_open_response(GObject* source, GAsyncResult* result,
                                         gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   const char* response = adw_alert_dialog_choose_finish(
      ADW_ALERT_DIALOG(source), result);
   if (0 != strcmp(response, "discard")) return;
   const char* path = static_cast<const char*>(g_object_get_data(source, "wxedid-open-path"));
   if (path != NULL) {
      std::string target = path;
      wnd_load_file(wnd, target.c_str(), path_is_hex_text(target.c_str()));
      return;
   }
   open_mode mode = static_cast<open_mode>(
      GPOINTER_TO_INT(g_object_get_data(source, "wxedid-open-mode")));
   wnd_present_open_dialog(wnd, mode);
}

//open a known file, or a dialog when path is NULL; unsaved changes are
//confirmed first
void wnd_request_open_source(wxedid_wnd* wnd, int requested, const char* path) {
   open_mode mode = static_cast<open_mode>(requested);
   if (! wnd->dirty) {
      if (path != NULL) {
         wnd_load_file(wnd, path, path_is_hex_text(path));
      } else {
         wnd_present_open_dialog(wnd, mode);
      }
      return;
   }

   AdwAlertDialog* dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new(
      "Discard unsaved changes?",
      "Opening another EDID will discard changes to the current EDID."));
   adw_alert_dialog_add_responses(dialog,
                                  "cancel", "Cancel",
                                  "discard", "Discard",
                                  NULL);
   adw_alert_dialog_set_close_response(dialog, "cancel");
   adw_alert_dialog_set_default_response(dialog, "cancel");
   adw_alert_dialog_set_response_appearance(dialog, "discard",
                                            ADW_RESPONSE_DESTRUCTIVE);
   g_object_set_data(G_OBJECT(dialog), "wxedid-open-mode", GINT_TO_POINTER(mode));
   if (path != NULL) {
      g_object_set_data_full(G_OBJECT(dialog), "wxedid-open-path", g_strdup(path), g_free);
   }
   adw_alert_dialog_choose(dialog, GTK_WIDGET(wnd->window), NULL,
                           wnd_on_discard_open_response, wnd);
}

static void wnd_request_open(wxedid_wnd* wnd, open_mode mode) {
   wnd_request_open_source(wnd, mode, NULL);
}

//a file dropped on the window opens like one chosen in the open dialog
gboolean wnd_on_drop(GtkDropTarget*, const GValue* value, double, double,
                     gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   if (! G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) return FALSE;
   GSList* files = static_cast<GSList*>(g_value_get_boxed(value));
   if (files == NULL) return FALSE;
   char* path = g_file_get_path(G_FILE(files->data));
   if (path == NULL) {
      wnd->doc->GLog.DoLog(
         "[E!] Couldn’t open the dropped item: only local EDID files are supported.");
      return FALSE;
   }
   wnd_request_open_source(wnd, OPEN_FILE, path);
   g_free(path);
   return TRUE;
}

void wnd_on_open_action(GSimpleAction* /*action*/, GVariant* /*parameter*/,
                        gpointer user_data) {
   wnd_request_open((wxedid_wnd*) user_data, OPEN_FILE);
}

void wnd_on_import_hex_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_request_open(static_cast<wxedid_wnd*>(user_data), OPEN_HEX);
}

void wnd_on_open_display_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_request_open(static_cast<wxedid_wnd*>(user_data), OPEN_DISPLAY);
}

void wnd_on_compare_display_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_present_display_dialog(static_cast<wxedid_wnd*>(user_data), true);
}

//------------
// output: assemble groups into the buffer and recompute checksums
static bool wnd_prepare_output(wxedid_wnd* wnd) {
   wnd_flush_refresh(wnd);
   bool prepared = edid_prepare_output(wnd->doc->EDID, wnd->doc->GLog);
   if (prepared) wnd_refresh_raw_view(wnd);
   return prepared;
}

//------------
// save: write the buffer to a given path, recompute checksums first
static bool wnd_save_to_file(wxedid_wnd* wnd, const char* path) {
   if (! wnd_prepare_output(wnd)) return false;
   edi_buf_t* pbuf = wnd->doc->EDID.getEDID();

   FILE* out = fopen(path, "wb");
   if (out == NULL) {
      char msg[1400];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t save %s: %s. Check its permissions, then save again.",
               path, strerror(errno));
      wnd->doc->GLog.DoLog(msg);
      return false;
   }

   size_t expected = wnd->doc->EDID.getNumValidBlocks() * sizeof(ediblk_t);
   size_t wr = fwrite(pbuf->buff, 1, expected, out);
   int close_rc = fclose(out);
   if ((wr != expected) || (close_rc != 0)) {
      char msg[1400];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t save %s completely. Check free space and permissions, "
               "then save again.", path);
      wnd->doc->GLog.DoLog(msg);
      return false;
   }

   char msg[1152];
   snprintf(msg, sizeof(msg), "[i] Saved %zu bytes to %s", wr, path);
   wnd->doc->GLog.DoLog(msg);
   wnd_add_recent(path, false);
   if (strcmp(path, wnd->doc->path) != 0) {
      snprintf(wnd->doc->path, sizeof(wnd->doc->path), "%s", path);
   }
   wnd->source_writable = (g_access(wnd->doc->path, W_OK) == 0);
   wnd->document_hex = false;
   wnd->saved_history_position = static_cast<long>(wnd->history_position);
   wnd_update_history_state(wnd);
   wnd_update_document_ui(wnd);

   char* basename = g_path_get_basename(wnd->doc->path);
   char* toast_title = g_strdup_printf("Saved %s", basename);
   adw_toast_overlay_add_toast(wnd->toast_overlay, adw_toast_new(toast_title));
   g_free(toast_title);
   g_free(basename);
   return true;
}

static void wnd_on_save_response(GObject* source, GAsyncResult* result,
                                 gpointer user_data) {
   GtkWindow* window = GTK_WINDOW(user_data);
   wxedid_wnd* wnd = (wxedid_wnd*)
      g_object_get_data(G_OBJECT(window), "wxedid-wnd");
   GError* error = NULL;
   GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error);

   if (file != NULL) {
      if (wnd != NULL) {
         char* path = g_file_get_path(file);
         if (path != NULL) {
            wnd_save_to_file(wnd, path);
            g_free(path);
         } else {
            wnd->doc->GLog.DoLog(
               "[E!] Couldn’t save to the selected location: only local files are "
               "supported. Choose a local file.");
         }
      }
      g_object_unref(file);
   } else if ((wnd != NULL) && (error != NULL) &&
              ! g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) &&
              ! g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED)) {
      char msg[1200];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t choose where to save: %s. Try again or choose another location.",
               error->message);
      wnd->doc->GLog.DoLog(msg);
   }

   g_clear_error(&error);
   g_object_unref(window);
}

static void wnd_present_save_dialog(wxedid_wnd* wnd) {
   GtkFileDialog* dialog = gtk_file_dialog_new();
   gtk_file_dialog_set_title(dialog, "Save EDID binary");
   gtk_file_dialog_set_accept_label(dialog, "Save");
   char* basename = document_basename(wnd->doc->path);
   char* initial_name = NULL;
   if (g_str_has_prefix(wnd->doc->path, DRM_ROOT)) {
      initial_name = g_strdup_printf("%s.bin", basename);
   } else if (wnd->document_hex && (basename != NULL)) {
      char* extension = strrchr(basename, '.');
      char* stem = (extension != NULL) && (extension != basename)
         ? g_strndup(basename, extension - basename) : g_strdup(basename);
      initial_name = g_strdup_printf("%s.bin", stem);
      g_free(stem);
   } else if (! wnd->source_writable && (basename != NULL)) {
      char* extension = strrchr(basename, '.');
      if ((extension != NULL) && (extension != basename)) {
         char* stem = g_strndup(basename, extension - basename);
         initial_name = g_strdup_printf("%s-copy%s", stem, extension);
         g_free(stem);
      } else {
         initial_name = g_strdup_printf("%s-copy", basename);
      }
   }
   gtk_file_dialog_set_initial_name(
      dialog, initial_name != NULL ? initial_name :
      ((basename != NULL) && (basename[0] != 0) ? basename : "edid.bin"));
   g_free(initial_name);
   g_free(basename);

   char* directory = g_path_get_dirname(wnd->doc->path);
   if ((directory != NULL) && (directory[0] != 0) &&
       ! g_str_has_prefix(wnd->doc->path, DRM_ROOT)) {
      GFile* folder = g_file_new_for_path(directory);
      gtk_file_dialog_set_initial_folder(dialog, folder);
      g_object_unref(folder);
   }
   g_free(directory);
   gtk_file_dialog_save(dialog, wnd->window, NULL, wnd_on_save_response,
                        g_object_ref(wnd->window));
   g_object_unref(dialog);
}

static void wnd_request_save(wxedid_wnd* wnd) {
   if ((wnd->doc->path[0] != 0) && wnd->source_writable) {
      wnd_save_to_file(wnd, wnd->doc->path);
   } else {
      wnd_present_save_dialog(wnd);
   }
}

void wnd_on_save_action(GSimpleAction* /*action*/, GVariant* /*parameter*/,
                        gpointer user_data) {
   wnd_request_save((wxedid_wnd*) user_data);
}

void wnd_on_save_as_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_present_save_dialog(static_cast<wxedid_wnd*>(user_data));
}

//------------
// text output: hex export and structure report
struct wxedid_text_output {
   GtkWindow*  window;
   std::string contents;
   const char* done; //toast title prefix
};

static void wnd_on_text_save_response(GObject* source, GAsyncResult* result,
                                      gpointer user_data) {
   wxedid_text_output* output = static_cast<wxedid_text_output*>(user_data);
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(
      g_object_get_data(G_OBJECT(output->window), "wxedid-wnd"));
   GError* error = NULL;
   GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error);

   if ((file != NULL) && (wnd != NULL)) {
      char* path = g_file_get_path(file);
      GError* write_error = NULL;
      if (path == NULL) {
         wnd->doc->GLog.DoLog(
            "[E!] Couldn’t save to the selected location: only local files are "
            "supported. Choose a local file.");
      } else if (! g_file_set_contents(path, output->contents.data(),
                                       output->contents.size(), &write_error)) {
         char msg[1400];
         snprintf(msg, sizeof(msg),
                  "[E!] Couldn’t save %s: %s. Check its permissions, then try again.",
                  path, write_error->message);
         wnd->doc->GLog.DoLog(msg);
         g_error_free(write_error);
      } else {
         char msg[1152];
         snprintf(msg, sizeof(msg), "[i] Saved %zu bytes to %s",
                  output->contents.size(), path);
         wnd->doc->GLog.DoLog(msg);
         char* basename = g_path_get_basename(path);
         char* title = g_strdup_printf("%s %s", output->done, basename);
         adw_toast_overlay_add_toast(wnd->toast_overlay, adw_toast_new(title));
         g_free(title);
         g_free(basename);
      }
      g_free(path);
   } else if ((wnd != NULL) && (error != NULL) &&
              ! g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) &&
              ! g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED)) {
      char msg[1200];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t choose where to save: %s. Try again or choose another location.",
               error->message);
      wnd->doc->GLog.DoLog(msg);
   }

   if (file != NULL) g_object_unref(file);
   g_clear_error(&error);
   g_object_unref(output->window);
   delete output;
}

static void wnd_present_text_save_dialog(wxedid_wnd* wnd, const char* title,
                                         const char* extension,
                                         const char* filter_name,
                                         std::string contents,
                                         const char* done) {
   GtkFileDialog* dialog = gtk_file_dialog_new();
   gtk_file_dialog_set_title(dialog, title);
   gtk_file_dialog_set_accept_label(dialog, "Save");
   const char* patterns[] = {extension, NULL};
   GListStore* filters = file_filters(filter_name, patterns);
   gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
   g_object_unref(filters);

   char* basename = document_basename(wnd->doc->path);
   char* dot = strrchr(basename, '.');
   if ((dot != NULL) && (dot != basename)) *dot = 0;
   char* initial_name = g_strdup_printf("%s.%s", basename, extension);
   gtk_file_dialog_set_initial_name(dialog, initial_name);
   g_free(initial_name);
   g_free(basename);

   char* directory = g_path_get_dirname(wnd->doc->path);
   if ((directory != NULL) && (directory[0] != 0) &&
       ! g_str_has_prefix(wnd->doc->path, DRM_ROOT)) {
      GFile* folder = g_file_new_for_path(directory);
      gtk_file_dialog_set_initial_folder(dialog, folder);
      g_object_unref(folder);
   }
   g_free(directory);

   wxedid_text_output* output = new wxedid_text_output{
      GTK_WINDOW(g_object_ref(wnd->window)), std::move(contents), done,
   };
   gtk_file_dialog_save(dialog, wnd->window, NULL, wnd_on_text_save_response, output);
   g_object_unref(dialog);
}

void wnd_on_export_hex_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   if (! wnd_prepare_output(wnd)) return;
   edi_buf_t* buffer = wnd->doc->EDID.getEDID();
   std::string hex = edid_hex_encode(
      buffer->buff, wnd->doc->EDID.getNumValidBlocks() * sizeof(ediblk_t));
   wnd_present_text_save_dialog(wnd, "Export EDID as hex", "hex", "Hex text",
                                hex, "Exported");
}

void wnd_on_save_report_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   if (! wnd_prepare_output(wnd)) return;
   char* source = document_basename(wnd->doc->path);
   std::string report = edid_text_report(wnd->doc->EDID, source, WXEDID_VERSION);
   g_free(source);
   wnd_present_text_save_dialog(wnd, "Save EDID report", "txt", "Text",
                                report, "Saved report");
}
