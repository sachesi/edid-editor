/***************************************************************
 * Name:      fields.cpp
 * Purpose:   the field cards of a group
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include "window_private.h"

//------------
// helpers shared by rows
static bool field_writable(const edi_field_t& f) {
   return (0 == (f.flags & F_RD));
}

static bool field_has_selector(const edi_field_t& f) {
   return ((f.flags & F_VS) != 0) && (f.vmap_idx != VS_NO_SELECTOR);
}

//reserved bits and bytes, named or described as such
static bool field_is_reserved(const edi_field_t& f) {
   const char* name = (f.name != NULL) ? f.name : "";
   if (g_ascii_strncasecmp(name, "rsvd", 4) == 0) return true;
   if (g_ascii_strncasecmp(name, "resvd", 5) == 0) return true;
   if (g_ascii_strncasecmp(name, "reserved", 8) == 0) return true;
   if ((g_ascii_strncasecmp(name, "res", 3) == 0) && g_ascii_isdigit(name[3])) return true;
   if (g_str_has_suffix(name, "_res") || g_str_has_suffix(name, "_rsvd")) return true;
   const char* desc = (f.desc != NULL) ? f.desc : "";
   return (g_ascii_strcasecmp(desc, "reserved (0)") == 0) ||
          (g_ascii_strcasecmp(desc, "reserved 0") == 0);
}

static void row_set_valid(wxedid_row* row, bool valid) {
   if (row->valid == valid) return;

   row->valid = valid;
   if (valid) {
      if (row->wnd->invalid_fields > 0) row->wnd->invalid_fields--;
   } else {
      row->wnd->invalid_fields++;
   }
   wnd_update_document_ui(row->wnd);
}

static void row_show_validation(wxedid_row* row, rcode result) {
   //only volatile-message faults carry text; others name a source location
   char detail[512] = "Enter a valid value";
   if (result.detail.rcode == RCD_FVMSG) {
      wxedid_RCD_GET_MSG(result, detail, sizeof(detail));
   }
   const char* message = detail;
   if (g_str_has_prefix(message, "[E!] ")) message += 5;
   gtk_label_set_text(row->error_label, message);
   gtk_widget_set_visible(GTK_WIDGET(row->error_label), TRUE);

   std::string field = edid_field_display_name(row->pfld->field.name);
   char banner[680];
   snprintf(banner, sizeof(banner), "%s: %s", field.c_str(), message);
   adw_banner_set_title(row->wnd->banner, banner);
   adw_banner_set_button_label(row->wnd->banner, NULL);
   adw_banner_set_revealed(row->wnd->banner, TRUE);
   row->wnd->banner_is_validation = true;
   row->wnd->banner_offers_retry = false;
}

static void row_clear_validation(wxedid_row* row) {
   gtk_label_set_text(row->error_label, "");
   gtk_widget_set_visible(GTK_WIDGET(row->error_label), FALSE);
}

void wnd_record_edit_history(wxedid_wnd* wnd, edi_grp_cl* group,
                             edi_dynfld_t* field, bool integer,
                             const wxc_String& session_before_text,
                             u32_t session_before_value,
                             const wxc_String& immediate_before_text,
                             u32_t immediate_before_value,
                             const wxc_String& after_text,
                             u32_t after_value, size_t* history_index) {
   const size_t no_index = static_cast<size_t>(-1);
   bool can_coalesce = (*history_index != no_index) &&
      (*history_index + 1 == wnd->history_position) &&
      (wnd->history_position == wnd->history.size()) &&
      (wnd->history[*history_index].group == group) &&
      (wnd->history[*history_index].field == field) &&
      (wnd->saved_history_position != static_cast<long>(wnd->history_position));

   if (can_coalesce) {
      wxedid_history_entry& entry = wnd->history[*history_index];
      entry.after_text = after_text.c_str();
      entry.after_value = after_value;
      bool unchanged = integer ? (entry.before_value == entry.after_value) :
                                 (entry.before_text == entry.after_text);
      if (unchanged) {
         wnd->history.pop_back();
         wnd->history_position--;
         *history_index = no_index;
      }
      wnd_update_history_state(wnd);
      return;
   }

   const wxc_String& before_text = (*history_index == no_index)
      ? session_before_text : immediate_before_text;
   u32_t before_value = (*history_index == no_index)
      ? session_before_value : immediate_before_value;
   size_t previous_size = wnd->history.size();
   wnd_record_history(wnd, group, field, integer, before_text, before_value,
                      after_text, after_value);
   *history_index = (wnd->history.size() > previous_size)
      ? wnd->history.size() - 1 : no_index;
}

static void row_on_focus_enter(GtkEventControllerFocus*, gpointer user_data) {
   wxedid_row* row = static_cast<wxedid_row*>(user_data);
   row->editing = true;
   row->history_index = static_cast<size_t>(-1);
   row->before_text.Empty();
   row->before_value = 0;
   (row->pEDID->*row->pfld->field.handlerfn)(
      OP_READ, row->before_text, row->before_value, row->pfld);
}

static void row_on_focus_leave(GtkEventControllerFocus*, gpointer user_data) {
   wxedid_row* row = static_cast<wxedid_row*>(user_data);
   row->editing = false;
   row->history_index = static_cast<size_t>(-1);
   wnd_schedule_refresh(row->wnd);
}

static void row_on_entry_activate(GtkEntry*, gpointer user_data) {
   wnd_schedule_refresh(static_cast<wxedid_row*>(user_data)->wnd);
}

//------------
//entry changed: write valid text back via the field handler
static void row_on_entry_changed(GtkEditable* entry, gpointer user_data) {
   wxedid_row* r = (wxedid_row*) user_data;

   const char* txt = gtk_editable_get_text(entry);
   wxc_String  sval(txt);
   u32_t       ival = 0;
   wxc_String  before_text;
   u32_t       before_value = 0;
   ( r->pEDID->*r->pfld->field.handlerfn )(
      OP_READ, before_text, before_value, r->pfld);

   rcode retU = ( r->pEDID->*r->pfld->field.handlerfn )(OP_WRSTR, sval, ival, r->pfld);

   if (RCD_IS_OK(retU)) {
      wxc_String after_text;
      u32_t after_value = 0;
      ( r->pEDID->*r->pfld->field.handlerfn )(
         OP_READ, after_text, after_value, r->pfld);
      if (r->editing) {
         wnd_record_edit_history(r->wnd, r->pgrp, r->pfld, false,
                                 r->before_text, r->before_value,
                                 before_text, before_value,
                                 after_text, after_value, &r->history_index);
      } else {
         wnd_record_history(r->wnd, r->pgrp, r->pfld, false,
                            before_text, before_value, after_text, after_value);
      }
      gtk_widget_remove_css_class(GTK_WIDGET(entry), "error");
      r->wnd->highlight_group = r->pgrp;
      r->wnd->highlight_field = r->pfld;
      row_clear_validation(r);
      row_set_valid(r, true);
      wnd_refresh_group_title(r->wnd, r->pgrp);
      wnd_refresh_selected_tree_label(r->wnd);
      timing_load_group(r->wnd->timing, r->pgrp, r->pEDID);
      wnd_refresh_raw_view(r->wnd);
      wnd_request_refresh(r->wnd, r->pgrp, r->pfld, RCD_IS_TRUE(retU), false);
      wnd_update_document_ui(r->wnd);
   } else {
      gtk_widget_add_css_class(GTK_WIDGET(entry), "error");
      row_set_valid(r, false);
      row_show_validation(r, retU);
   }
}

//dropdown chosen: write integer value via OP_WRINT
static void row_on_combo_notify(GtkDropDown* dd, GParamSpec* /*pspec*/, gpointer user_data) {
   wxedid_row* r = (wxedid_row*) user_data;

   gpointer idx = g_object_get_data(G_OBJECT(dd), "sel-idx");
   if (idx == NULL) return; //startup notification

   guint pos = gtk_drop_down_get_selected(dd);
   if (pos == GTK_INVALID_LIST_POSITION) return;

   u32_t* vals = (u32_t*) idx;
   u32_t  val  = vals[pos];

   wxc_String before_text;
   u32_t before_value = 0;
   ( r->pEDID->*r->pfld->field.handlerfn )(
      OP_READ, before_text, before_value, r->pfld);
   wxc_String sval;
   rcode retU = ( r->pEDID->*r->pfld->field.handlerfn )(OP_WRINT, sval, val, r->pfld);

   if (RCD_IS_OK(retU)) {
      wxc_String after_text;
      u32_t after_value = 0;
      ( r->pEDID->*r->pfld->field.handlerfn )(
         OP_READ, after_text, after_value, r->pfld);
      wnd_record_history(r->wnd, r->pgrp, r->pfld, true,
                         before_text, before_value, after_text, after_value);
      gtk_widget_remove_css_class(GTK_WIDGET(dd), "error");
      r->wnd->highlight_group = r->pgrp;
      r->wnd->highlight_field = r->pfld;
      row_clear_validation(r);
      row_set_valid(r, true);
      wnd_refresh_group_title(r->wnd, r->pgrp);
      wnd_refresh_selected_tree_label(r->wnd);
      timing_load_group(r->wnd->timing, r->pgrp, r->pEDID);
      wnd_refresh_raw_view(r->wnd);
      wnd_request_refresh(r->wnd, r->pgrp, r->pfld, RCD_IS_TRUE(retU), true);
      wnd_update_document_ui(r->wnd);
   } else {
      gtk_widget_add_css_class(GTK_WIDGET(dd), "error");
      row_set_valid(r, false);
      row_show_validation(r, retU);
   }
}

//switch toggled: write the bit as text, like an entry
static void row_on_switch_notify(GtkSwitch* toggle, GParamSpec* /*pspec*/, gpointer user_data) {
   wxedid_row* r = (wxedid_row*) user_data;

   wxc_String before_text;
   u32_t before_value = 0;
   ( r->pEDID->*r->pfld->field.handlerfn )(
      OP_READ, before_text, before_value, r->pfld);
   bool active = gtk_switch_get_active(toggle);
   if (active == (before_value != 0)) return;
   wxc_String sval(active ? "1" : "0");
   u32_t ival = 0;
   rcode retU = ( r->pEDID->*r->pfld->field.handlerfn )(OP_WRSTR, sval, ival, r->pfld);

   if (RCD_IS_OK(retU)) {
      wxc_String after_text;
      u32_t after_value = 0;
      ( r->pEDID->*r->pfld->field.handlerfn )(
         OP_READ, after_text, after_value, r->pfld);
      wnd_record_history(r->wnd, r->pgrp, r->pfld, false,
                         before_text, before_value, after_text, after_value);
      r->wnd->highlight_group = r->pgrp;
      r->wnd->highlight_field = r->pfld;
      row_clear_validation(r);
      wnd_refresh_group_title(r->wnd, r->pgrp);
      wnd_refresh_selected_tree_label(r->wnd);
      timing_load_group(r->wnd->timing, r->pgrp, r->pEDID);
      wnd_refresh_raw_view(r->wnd);
      wnd_request_refresh(r->wnd, r->pgrp, r->pfld, RCD_IS_TRUE(retU), true);
      wnd_update_document_ui(r->wnd);
   } else {
      //the bit keeps its value: put the switch back
      g_signal_handlers_block_by_func(toggle, (gpointer) row_on_switch_notify, r);
      gtk_switch_set_active(toggle, before_value != 0);
      g_signal_handlers_unblock_by_func(toggle, (gpointer) row_on_switch_notify, r);
      row_show_validation(r, retU);
   }
}

//------------
// field list: rebuilt when a group is selected in the tree
void fields_refresh(GtkFlowBox* list, edi_grp_cl* pgrp, EDID_cl& EDID) {
   wxedid_wnd* wnd = (wxedid_wnd*) g_object_get_data(G_OBJECT(list), "wxedid-wnd");
   rows_reload(list, pgrp, &EDID, wnd);
}

std::string field_help_summary(const char* description) {
   if ((description == NULL) || (*description == 0)) return {};

   std::string summary;
   bool previous_space = false;
   for (const char* ch = description; *ch != 0; ch++) {
      if ((*ch == '\n') || (*ch == '\r')) break;
      bool space = g_ascii_isspace(static_cast<guchar>(*ch));
      if (space) {
         if (! summary.empty() && ! previous_space) summary.push_back(' ');
      } else {
         summary.push_back(*ch);
      }
      previous_space = space;
      if ((summary.size() >= 140) && (*ch == ' ')) break;
   }
   while (! summary.empty() && (summary.back() == ' ')) summary.pop_back();
   if (summary.size() > 140) {
      size_t cut = summary.rfind(' ', 137);
      if (cut != std::string::npos) {
         summary.resize(cut);
         summary += "…";
      }
   }
   return summary;
}

//dropdown button: ellipsize long value names so cards keep their width
static void dropdown_label_setup(GtkSignalListItemFactory*, GtkListItem* item,
                                 gpointer ellipsize) {
   GtkWidget* label = gtk_label_new(NULL);
   gtk_label_set_xalign(GTK_LABEL(label), 0.0);
   if (ellipsize != NULL) gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
   gtk_list_item_set_child(item, label);
}

static void dropdown_label_bind(GtkSignalListItemFactory*, GtkListItem* item,
                                gpointer) {
   GtkStringObject* value = GTK_STRING_OBJECT(gtk_list_item_get_item(item));
   gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)),
                      gtk_string_object_get_string(value));
}

//full field description, created when the help button is first used
static void field_help_popup(GtkMenuButton* button, gpointer user_data) {
   if (gtk_menu_button_get_popover(button) != NULL) return;
   edi_dynfld_t* pfld = static_cast<edi_dynfld_t*>(user_data);
   std::string text = pfld->field.desc;
   for (char& ch : text) {
      if (ch == '\t') ch = ' ';
   }
   while (! text.empty() && (text.back() == '\n')) text.pop_back();

   GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
   gtk_widget_set_margin_start(box, 6);
   gtk_widget_set_margin_end(box, 6);
   gtk_widget_set_margin_top(box, 6);
   gtk_widget_set_margin_bottom(box, 6);
   GtkWidget* title = gtk_label_new(edid_field_display_name(pfld->field.name).c_str());
   gtk_label_set_xalign(GTK_LABEL(title), 0.0);
   gtk_widget_add_css_class(title, "heading");
   gtk_box_append(GTK_BOX(box), title);
   GtkWidget* body = gtk_label_new(text.c_str());
   gtk_label_set_xalign(GTK_LABEL(body), 0.0);
   gtk_label_set_wrap(GTK_LABEL(body), TRUE);
   gtk_label_set_max_width_chars(GTK_LABEL(body), 48);
   gtk_box_append(GTK_BOX(box), body);

   GtkWidget* popover = gtk_popover_new();
   gtk_popover_set_child(GTK_POPOVER(popover), box);
   gtk_menu_button_set_popover(button, popover);
}

//the focused field's bytes are marked in the bytes view
static void wnd_highlight_field(wxedid_wnd* wnd, edi_grp_cl* pgrp, edi_dynfld_t* pfld) {
   wnd->highlight_group = pgrp;
   wnd->highlight_field = pfld;
   wnd_refresh_raw_view(wnd);
}

static void card_on_focus_enter(GtkEventControllerFocus*, gpointer user_data) {
   wxedid_row* row = static_cast<wxedid_row*>(user_data);
   wnd_highlight_field(row->wnd, row->pgrp, row->pfld);
}

static void card_on_pressed(GtkGestureClick*, int, double, double, gpointer user_data) {
   wxedid_row* row = static_cast<wxedid_row*>(user_data);
   wnd_highlight_field(row->wnd, row->pgrp, row->pfld);
}

void rows_reload(GtkFlowBox* list, edi_grp_cl* pgrp, EDID_cl* pEDID,
                 wxedid_wnd* wnd) {
   if (pgrp != wnd->highlight_group) {
      wnd->highlight_group = NULL;
      wnd->highlight_field = NULL;
   }
   //drop old rows
   wnd->invalid_fields = 0;
   if (wnd->reserved_note != NULL) gtk_widget_set_visible(wnd->reserved_note, FALSE);
   GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(list));
   while (child != NULL) {
      GtkWidget* next = gtk_widget_get_next_sibling(child);
      gtk_flow_box_remove(list, child);
      child = next;
   }

   if (pgrp == NULL) return;

   wxc_String sval;
   wxc_String vdesc;
   u32_t      ival = 0;
   u32_t      cnt  = pgrp->FieldsAr.GetCount();
   u32_t      hidden = 0;

   for (u32_t idx=0; idx<cnt; idx++) {
      edi_dynfld_t* pfld = pgrp->FieldsAr.Item(idx);

      sval.Empty();
      ival = 0;
      rcode retU = ( pEDID->*pfld->field.handlerfn )(OP_READ, sval, ival, pfld);

      //reserved fields holding zero say nothing; a set bit is worth seeing
      if (! wnd->show_reserved && field_is_reserved(pfld->field) &&
          RCD_IS_OK(retU) && (ival == 0)) {
         hidden++;
         continue;
      }

      GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_size_request(card, 240, -1);

      std::string help = field_help_summary(pfld->field.desc);
      if (! help.empty()) gtk_widget_set_tooltip_text(card, help.c_str());

      GtkWidget* card_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
      gtk_widget_set_margin_start(card_content, 12);
      gtk_widget_set_margin_end(card_content, 12);
      gtk_widget_set_margin_top(card_content, 12);
      gtk_widget_set_margin_bottom(card_content, 12);
      gtk_box_append(GTK_BOX(card), card_content);

      std::string title = edid_field_display_name(pfld->field.name);
      wxc_String unit;
      pEDID->getValUnitName(unit, pfld->field.flags);
      if (unit == wxc_String("pix")) unit = "px";
      //many field names already end with their unit, e.g. "H-Active pix"
      bool unit_named = (unit.Len() < title.size()) &&
         g_str_has_suffix(title.c_str(), (" " + unit.std_str()).c_str());
      std::string caption = (unit.IsEmpty() || unit_named)
         ? title : title + " (" + unit.std_str() + ")";
      GtkWidget* label = gtk_label_new(caption.c_str());
      gtk_label_set_xalign(GTK_LABEL(label), 0.0);
      gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
      gtk_widget_add_css_class(label, "caption");
      gtk_widget_add_css_class(label, "dim-label");
      gtk_widget_set_hexpand(label, TRUE);
      GtkWidget* title_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_box_append(GTK_BOX(title_row), label);
      if ((pfld->field.flags & F_NU) != 0) {
         GtkWidget* unused = gtk_label_new("Not used");
         gtk_widget_add_css_class(unused, "caption");
         gtk_widget_add_css_class(unused, "dim-label");
         gtk_box_append(GTK_BOX(title_row), unused);
      }
      if ((pfld->field.desc != NULL) && (pfld->field.desc[0] != 0)) {
         GtkWidget* about = gtk_menu_button_new();
         gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(about), "help-about-symbolic");
         gtk_widget_add_css_class(about, "flat");
         gtk_widget_add_css_class(about, "circular");
         gtk_widget_add_css_class(about, "field-help");
         gtk_widget_set_valign(about, GTK_ALIGN_CENTER);
         std::string about_label = "About " + title;
         gtk_widget_set_tooltip_text(about, about_label.c_str());
         gtk_accessible_update_property(GTK_ACCESSIBLE(about),
                                        GTK_ACCESSIBLE_PROPERTY_LABEL,
                                        about_label.c_str(), -1);
         gtk_menu_button_set_create_popup_func(GTK_MENU_BUTTON(about),
                                               field_help_popup, pfld, NULL);
         gtk_box_append(GTK_BOX(title_row), about);
      }
      gtk_box_append(GTK_BOX(card_content), title_row);

      GtkWidget* validation = gtk_label_new(NULL);
      gtk_label_set_xalign(GTK_LABEL(validation), 0.0);
      gtk_label_set_wrap(GTK_LABEL(validation), TRUE);
      gtk_label_set_wrap_mode(GTK_LABEL(validation), PANGO_WRAP_WORD_CHAR);
      gtk_widget_add_css_class(validation, "caption");
      gtk_widget_add_css_class(validation, "error");
      gtk_widget_set_visible(validation, FALSE);

      GtkWidget* widget = NULL;

      if (field_has_selector(pfld->field)) {
         //value selector dropdown from vmap
         sm_vmap* vmap = vmap_GetVmap(pfld->field.vmap_idx, VMAP_MID);
         if (vmap != NULL) {
            GtkStringList* items = gtk_string_list_new(NULL);
            u32_t*         vals  = new u32_t[vmap->size()];
            u32_t          pos   = 0;
            int            cur   = -1;

            for (auto& kv : *vmap) {
               gtk_string_list_append(items, kv.second.name);
               //F_VSVM: the selectable value lives in vmap_ent_t.val,
               //otherwise it is the map key (menu id)
               u32_t v = (pfld->field.flags & F_VSVM) ? kv.second.val : kv.first;
               vals[pos] = v;
               if (v == ival) cur = pos;
               pos++;
            }

            GtkDropDown* dd = GTK_DROP_DOWN(gtk_drop_down_new(
               G_LIST_MODEL(items), NULL));
            GtkListItemFactory* button_factory = gtk_signal_list_item_factory_new();
            g_signal_connect(button_factory, "setup",
                             G_CALLBACK(dropdown_label_setup), GINT_TO_POINTER(1));
            g_signal_connect(button_factory, "bind",
                             G_CALLBACK(dropdown_label_bind), NULL);
            gtk_drop_down_set_factory(dd, button_factory);
            g_object_unref(button_factory);
            GtkListItemFactory* list_factory = gtk_signal_list_item_factory_new();
            g_signal_connect(list_factory, "setup",
                             G_CALLBACK(dropdown_label_setup), NULL);
            g_signal_connect(list_factory, "bind",
                             G_CALLBACK(dropdown_label_bind), NULL);
            gtk_drop_down_set_list_factory(dd, list_factory);
            g_object_unref(list_factory);
            gtk_drop_down_set_selected(dd, (cur >= 0) ? (guint) cur : GTK_INVALID_LIST_POSITION);
            gtk_widget_set_hexpand(GTK_WIDGET(dd), TRUE);
            gtk_widget_set_halign(GTK_WIDGET(dd), GTK_ALIGN_FILL);

            wxedid_row* r = new wxedid_row{
               pfld, pgrp, pEDID, wnd, ROW_COMBO, GTK_WIDGET(dd),
               GTK_LABEL(validation), 0, true,
               false, static_cast<size_t>(-1), {}, 0
            };
            g_object_set_data_full(G_OBJECT(dd), "sel-idx", vals,
                                   [](gpointer data){ delete[] (u32_t*) data; });
            g_object_set_data_full(G_OBJECT(dd), "row", r,
                                   [](gpointer data){ delete (wxedid_row*) data; });

            g_signal_connect(dd, "notify::selected", G_CALLBACK(row_on_combo_notify), r);
            gtk_widget_set_sensitive(GTK_WIDGET(dd),
                                     field_writable(pfld->field) || pEDID->b_RD_Ignore);
            widget = GTK_WIDGET(dd);
         }
      }

      if ((widget == NULL) && ((pfld->field.flags & F_BIT) != 0) && RCD_IS_OK(retU)) {
         //single bit: on/off switch
         GtkWidget* toggle = gtk_switch_new();
         gtk_switch_set_active(GTK_SWITCH(toggle), ival != 0);
         gtk_widget_set_halign(toggle, GTK_ALIGN_START);
         gtk_widget_set_sensitive(toggle,
                                  field_writable(pfld->field) || pEDID->b_RD_Ignore);
         wxedid_row* r = new wxedid_row{
            pfld, pgrp, pEDID, wnd, ROW_SWITCH, toggle,
            GTK_LABEL(validation), 0, true,
            false, static_cast<size_t>(-1), {}, 0
         };
         g_object_set_data_full(G_OBJECT(toggle), "row", r,
                                [](gpointer data){ delete (wxedid_row*) data; });
         g_signal_connect(toggle, "notify::active", G_CALLBACK(row_on_switch_notify), r);
         widget = toggle;
      }

      if (widget == NULL) {
         if (field_writable(pfld->field) || pEDID->b_RD_Ignore) {
            //text entry
            GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
            gtk_editable_set_text(GTK_EDITABLE(entry), sval.c_str());
            gtk_widget_set_hexpand(GTK_WIDGET(entry), TRUE);
            gtk_widget_set_halign(GTK_WIDGET(entry), GTK_ALIGN_FILL);

            wxedid_row* r = new wxedid_row{
               pfld, pgrp, pEDID, wnd, ROW_ENTRY, GTK_WIDGET(entry),
               GTK_LABEL(validation), 0, true,
               false, static_cast<size_t>(-1), {}, 0
            };

            g_signal_connect(entry, "changed", G_CALLBACK(row_on_entry_changed), r);
            g_signal_connect(entry, "activate", G_CALLBACK(row_on_entry_activate), r);
            GtkEventController* focus = gtk_event_controller_focus_new();
            g_signal_connect(focus, "enter", G_CALLBACK(row_on_focus_enter), r);
            g_signal_connect(focus, "leave", G_CALLBACK(row_on_focus_leave), r);
            gtk_widget_add_controller(GTK_WIDGET(entry), focus);
            g_object_set_data_full(G_OBJECT(entry), "row", r,
                                   [](gpointer data){ delete (wxedid_row*) data; });

            widget = GTK_WIDGET(entry);
         } else {
            //read-only label
            GtkWidget* lbl_val = gtk_label_new(sval.c_str());
            gtk_label_set_xalign(GTK_LABEL(lbl_val), 0.0);
            gtk_label_set_ellipsize(GTK_LABEL(lbl_val), PANGO_ELLIPSIZE_END);
            gtk_label_set_selectable(GTK_LABEL(lbl_val), TRUE);
            gtk_label_set_max_width_chars(GTK_LABEL(lbl_val), 48);
            gtk_widget_set_tooltip_text(lbl_val, sval.c_str());
            gtk_widget_set_hexpand(lbl_val, TRUE);
            gtk_widget_set_halign(lbl_val, GTK_ALIGN_FILL);
            gtk_widget_add_css_class(lbl_val, "monospace");
            if (! RCD_IS_OK(retU)) {
               gtk_widget_add_css_class(lbl_val, "error");
            }
            widget = lbl_val;
         }
      }

      gtk_box_append(GTK_BOX(card_content), widget);
      gtk_box_append(GTK_BOX(card_content), validation);
      //the card remembers its field for the bytes view
      wxedid_row* marker = new wxedid_row{
         pfld, pgrp, pEDID, wnd, ROW_LABEL, widget, GTK_LABEL(validation), 0, true,
         false, static_cast<size_t>(-1), {}, 0
      };
      g_object_set_data_full(G_OBJECT(card), "field", marker,
                             [](gpointer data){ delete (wxedid_row*) data; });
      GtkEventController* card_focus = gtk_event_controller_focus_new();
      g_signal_connect(card_focus, "enter", G_CALLBACK(card_on_focus_enter), marker);
      gtk_widget_add_controller(card, card_focus);
      GtkGesture* card_click = gtk_gesture_click_new();
      g_signal_connect(card_click, "pressed", G_CALLBACK(card_on_pressed), marker);
      gtk_widget_add_controller(card, GTK_EVENT_CONTROLLER(card_click));
      gtk_accessible_update_property(GTK_ACCESSIBLE(widget),
                                     GTK_ACCESSIBLE_PROPERTY_LABEL,
                                     title.c_str(), -1);
      if (! help.empty()) {
         gtk_widget_set_tooltip_text(widget, help.c_str());
         gtk_accessible_update_property(GTK_ACCESSIBLE(widget),
                                        GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                        help.c_str(), -1);
      }
      gtk_widget_add_css_class(card, "card");
      gtk_flow_box_append(list, card);
      gtk_widget_set_focusable(gtk_widget_get_parent(card), FALSE);
   }

   if (wnd->reserved_note != NULL) {
      char note[96];
      snprintf(note, sizeof(note),
               (hidden == 1) ? "%u reserved field is hidden" : "%u reserved fields are hidden",
               hidden);
      gtk_label_set_text(wnd->reserved_label, note);
      gtk_widget_set_visible(wnd->reserved_note, hidden > 0);
   }
   wnd_update_document_ui(wnd);
}
