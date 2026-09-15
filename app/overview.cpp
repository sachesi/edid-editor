/***************************************************************
 * Name:      overview.cpp
 * Purpose:   the overview, comparison with another EDID and the EDID log
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include "window_private.h"

//------------
// overview: key facts of the whole EDID
void wnd_refresh_overview(wxedid_wnd* wnd) {
   GtkWidget* page = adw_preferences_page_new();
   if (! wnd->notes.empty()) {
      GtkWidget* notes = adw_preferences_group_new();
      adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(notes), _("Notes"));
      adw_preferences_group_set_description(ADW_PREFERENCES_GROUP(notes),
                                            _("Found while opening this EDID"));
      for (const std::string& note : wnd->notes) {
         GtkWidget* row = adw_action_row_new();
         adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
         adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), note.c_str());
         adw_action_row_set_title_lines(ADW_ACTION_ROW(row), 0);
         adw_action_row_add_prefix(ADW_ACTION_ROW(row),
                                   gtk_image_new_from_icon_name("dialog-information-symbolic"));
         adw_preferences_group_add(ADW_PREFERENCES_GROUP(notes), row);
      }
      adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(notes));
   }
   GtkWidget* group = NULL;
   std::string section;
   for (const edid_summary_item& item : edid_summary(wnd->doc->EDID)) {
      if ((group == NULL) || (item.section != section)) {
         section = item.section;
         group = adw_preferences_group_new();
         adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group), section.c_str());
         adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(group));
      }
      GtkWidget* row = adw_action_row_new();
      adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), item.label.c_str());
      adw_action_row_set_subtitle(ADW_ACTION_ROW(row), item.value.c_str());
      adw_action_row_set_subtitle_selectable(ADW_ACTION_ROW(row), TRUE);
      gtk_widget_add_css_class(row, "property");
      adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), row);
   }
   adw_bin_set_child(ADW_BIN(wnd->overview_bin), page);
}

void wnd_show_overview(wxedid_wnd* wnd) {
   wnd->overview_shown = true;
   wnd->last_selected = NULL;
   gtk_single_selection_set_selected(wnd->tree_sel, GTK_INVALID_LIST_POSITION);
   GtkListBoxRow* row = gtk_list_box_get_row_at_index(wnd->overview_list, 0);
   if (gtk_list_box_get_selected_row(wnd->overview_list) != row) {
      gtk_list_box_select_row(wnd->overview_list, row);
   }
   gtk_label_set_text(wnd->group_title, _("Overview"));
   gtk_widget_set_visible(GTK_WIDGET(wnd->group_subtitle), FALSE);
   gtk_widget_set_visible(wnd->editor_switcher, FALSE);
   wnd_refresh_overview(wnd);
   adw_view_stack_page_set_visible(wnd->overview_page, TRUE);
   adw_view_stack_set_visible_child_name(wnd->editor_stack, "overview");
   wnd_update_group_actions(wnd);
   if (adw_overlay_split_view_get_collapsed(wnd->split_view)) {
      adw_overlay_split_view_set_show_sidebar(wnd->split_view, FALSE);
   }
}

void wnd_on_overview_selected(GtkListBox*, GtkListBoxRow* row, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   if ((row != NULL) && ! wnd->overview_shown) wnd_show_overview(wnd);
}

//everything the parser and the editor logged, for troubleshooting
static void wnd_on_log_copy(GtkButton* button, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   GtkTextIter start;
   GtkTextIter end;
   gtk_text_buffer_get_bounds(wnd->log, &start, &end);
   char* text = gtk_text_buffer_get_text(wnd->log, &start, &end, FALSE);
   gdk_clipboard_set_text(gtk_widget_get_clipboard(GTK_WIDGET(button)), text);
   g_free(text);
   adw_toast_overlay_add_toast(wnd->toast_overlay, adw_toast_new(_("Log copied")));
}

void wnd_present_log(wxedid_wnd* wnd) {
   GtkWidget* content = NULL;
   if (gtk_text_buffer_get_char_count(wnd->log) == 0) {
      content = adw_status_page_new();
      adw_status_page_set_icon_name(ADW_STATUS_PAGE(content), "text-x-generic-symbolic");
      adw_status_page_set_title(ADW_STATUS_PAGE(content), _("Nothing logged"));
      adw_status_page_set_description(ADW_STATUS_PAGE(content),
         _("Messages from opening, editing, and saving an EDID appear here."));
   } else {
      GtkWidget* view = gtk_text_view_new_with_buffer(wnd->log);
      gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
      gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
      gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
      gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 18);
      gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 18);
      gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 12);
      gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(view), 12);
      gtk_widget_add_css_class(view, "byte-view");
      gtk_accessible_update_property(GTK_ACCESSIBLE(view), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                     _("EDID log"), -1);
      content = gtk_scrolled_window_new();
      gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(content), view);
   }

   GtkWidget* header = adw_header_bar_new();
   GtkWidget* copy = gtk_button_new_from_icon_name("edit-copy-symbolic");
   gtk_widget_set_tooltip_text(copy, _("Copy Log"));
   gtk_accessible_update_property(GTK_ACCESSIBLE(copy), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  _("Copy Log"), -1);
   gtk_widget_set_sensitive(copy, gtk_text_buffer_get_char_count(wnd->log) > 0);
   g_signal_connect(copy, "clicked", G_CALLBACK(wnd_on_log_copy), wnd);
   adw_header_bar_pack_start(ADW_HEADER_BAR(header), copy);
   GtkWidget* toolbar = adw_toolbar_view_new();
   adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
   adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), content);
   AdwDialog* dialog = adw_dialog_new();
   adw_dialog_set_title(dialog, _("EDID Log"));
   adw_dialog_set_content_width(dialog, 640);
   adw_dialog_set_content_height(dialog, 480);
   adw_dialog_set_child(dialog, toolbar);
   adw_dialog_present(dialog, GTK_WIDGET(wnd->window));
}

void wnd_on_log_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_present_log(static_cast<wxedid_wnd*>(user_data));
}

//------------
// compare: differences between the document and another EDID
static bool read_edid_source(const char* path, bool hex, std::vector<u8_t>& bytes,
                             std::string& problem) {
   char* contents = NULL;
   gsize length = 0;
   GError* error = NULL;
   if (! g_file_get_contents(path, &contents, &length, &error)) {
      problem = error->message;
      g_error_free(error);
      return false;
   }
   bool ok = true;
   if (hex) {
      ok = edid_hex_decode(contents, length, bytes, problem);
   } else {
      bytes.assign(contents, contents + length);
   }
   g_free(contents);
   return ok;
}

static std::string compare_change(const edid_difference& entry, const char* other) {
   if (entry.field.empty() || (entry.left.empty() != entry.right.empty())) {
      if (entry.right.empty()) return _("Only in this EDID");
      char* text = g_strdup_printf(_("Only in %s"), other);
      std::string only = text;
      g_free(text);
      return only;
   }
   return entry.left + " → " + entry.right;
}

void wnd_present_compare(wxedid_wnd* wnd, const char* path, bool hex) {
   std::vector<u8_t> bytes;
   std::string problem;
   char* other = document_basename(path);
   EDID_cl EDID;
   guilog_cl log;
   log.SetSink([](const char*, void*) {}, NULL);
   EDID.SetGuiLogPtr(&log);
   if (! read_edid_source(path, hex, bytes, problem) ||
       ! edid_parse_bytes(EDID, bytes, problem)) {
      char message[1400];
      snprintf(message, sizeof(message), _("Couldn’t compare with %s: %s"), other, problem.c_str());
      wnd_show_error(wnd, message);
      g_free(other);
      return;
   }
   wnd_flush_refresh(wnd);
   std::vector<edid_difference> found = edid_compare(wnd->doc->EDID, EDID);

   GtkWidget* content = NULL;
   if (found.empty()) {
      content = adw_status_page_new();
      adw_status_page_set_icon_name(ADW_STATUS_PAGE(content), "object-select-symbolic");
      adw_status_page_set_title(ADW_STATUS_PAGE(content), _("No differences"));
      adw_status_page_set_description(ADW_STATUS_PAGE(content),
         _("Both EDIDs hold the same data, apart from their checksums."));
   } else {
      content = adw_preferences_page_new();
      char count[64];
      snprintf(count, sizeof(count), ngettext("%zu difference", "%zu differences", found.size()),
               found.size());
      adw_preferences_page_set_description(ADW_PREFERENCES_PAGE(content), count);
      GtkWidget* group = NULL;
      std::string place;
      for (const edid_difference& entry : found) {
         if ((group == NULL) || (entry.place != place)) {
            place = entry.place;
            group = adw_preferences_group_new();
            adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group), place.c_str());
            adw_preferences_page_add(ADW_PREFERENCES_PAGE(content), ADW_PREFERENCES_GROUP(group));
         }
         GtkWidget* row = adw_action_row_new();
         adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
         std::string title = entry.field.empty() ? _("Group") : edid_field_display_name(entry.field.c_str());
         adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title.c_str());
         adw_action_row_set_subtitle(ADW_ACTION_ROW(row), compare_change(entry, other).c_str());
         adw_action_row_set_subtitle_selectable(ADW_ACTION_ROW(row), TRUE);
         gtk_widget_add_css_class(row, "property");
         adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), row);
      }
   }

   char* current = document_basename(wnd->doc->path);
   char* subtitle = g_strdup_printf("%s → %s", current, other);
   GtkWidget* header = adw_header_bar_new();
   adw_header_bar_set_title_widget(ADW_HEADER_BAR(header),
                                   adw_window_title_new(_("Compare"), subtitle));
   GtkWidget* view = adw_toolbar_view_new();
   adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(view), header);
   adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(view), content);
   AdwDialog* dialog = adw_dialog_new();
   adw_dialog_set_title(dialog, _("Compare"));
   adw_dialog_set_content_width(dialog, 560);
   adw_dialog_set_content_height(dialog, 600);
   adw_dialog_set_child(dialog, view);
   adw_dialog_present(dialog, GTK_WIDGET(wnd->window));
   g_free(subtitle);
   g_free(current);
   g_free(other);
}

static void wnd_on_compare_response(GObject* source, GAsyncResult* result,
                                    gpointer user_data) {
   GtkWindow* window = GTK_WINDOW(user_data);
   wxedid_wnd* wnd = (wxedid_wnd*) g_object_get_data(G_OBJECT(window), "wxedid-wnd");
   GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, NULL);
   if ((file != NULL) && (wnd != NULL)) {
      char* path = g_file_get_path(file);
      if (path != NULL) wnd_present_compare(wnd, path, path_is_hex_text(path));
      g_free(path);
   }
   if (file != NULL) g_object_unref(file);
   g_object_unref(window);
}

void wnd_on_compare_file_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   GtkFileDialog* dialog = gtk_file_dialog_new();
   gtk_file_dialog_set_title(dialog, _("Compare with EDID file"));
   gtk_file_dialog_set_accept_label(dialog, _("Compare"));
   static const char* const patterns[] = {"bin", "hex", "txt", NULL};
   GListStore* filters = file_filters(_("EDID files"), patterns);
   gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
   g_object_unref(filters);
   gtk_file_dialog_open(dialog, wnd->window, NULL, wnd_on_compare_response,
                        g_object_ref(wnd->window));
   g_object_unref(dialog);
}
