/***************************************************************
 * Name:      sidebar.cpp
 * Purpose:   the sidebar: blocks and groups, search, group actions, bytes view
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include "window_private.h"

static void wxedid_item_init(wxedid_item*) {}
static void wxedid_item_class_init(wxedid_itemClass*) {}

G_DEFINE_FINAL_TYPE(wxedid_item, wxedid_item, G_TYPE_OBJECT)

static wxedid_item* wxedid_item_new(edi_grp_cl* pgrp, EDID_cl* pEDID) {
   wxedid_item* item = (wxedid_item*) g_object_new(WXEDID_TYPE_ITEM, NULL);
   item->pgrp       = pgrp;
   item->pgrp_ar    = NULL;
   item->pEDID      = pEDID;
   item->selectable = (pgrp != NULL);
   item->raw_block  = -1;
   return item;
}

static wxedid_item* wxedid_item_new_block(const char* label,
                                           GroupAr_cl* pgrp_ar,
                                           EDID_cl* pEDID) {
   wxedid_item* item = wxedid_item_new(NULL, pEDID);
   item->pgrp_ar = pgrp_ar;
   snprintf(item->label, sizeof(item->label), "%s", label);
   return item;
}

static wxedid_item* wxedid_item_new_raw_extension(u32_t block, u8_t tag,
                                                   EDID_cl* pEDID) {
   wxedid_item* item = wxedid_item_new(NULL, pEDID);
   const char* type = (tag == 0x70) ? "DisplayID" : _("Unsupported");
   snprintf(item->label, sizeof(item->label),
            _("Extension %u: %s (0x%02X), preserved read-only"), block, type, tag);
   item->selectable = true;
   item->raw_block = static_cast<int>(block);
   return item;
}

//------------
// GtkTreeListModel expand callback: sub-groups of a group
static GListModel* tree_item_expand(gpointer item, gpointer /*user_data*/) {
   wxedid_item* it = WXEDID_ITEM(item);

   if (it->pgrp_ar != NULL) {
      GListStore* store = g_list_store_new(WXEDID_TYPE_ITEM);
      u32_t cnt = it->pgrp_ar->GetCount();
      for (u32_t idx=0; idx<cnt; idx++) {
         edi_grp_cl* pgrp = it->pgrp_ar->Item(idx);
         if (pgrp == NULL) continue;
         wxedid_item* child = wxedid_item_new(pgrp, it->pEDID);
         g_list_store_append(store, child);
         g_object_unref(child);
      }
      return G_LIST_MODEL(store);
   }

   edi_grp_cl*  pgrp = it->pgrp;
   if (pgrp == NULL) return NULL;

   u32_t subg_cnt = pgrp->getSubGrpCount();
   if (subg_cnt == 0) return NULL;

   GListStore* store = g_list_store_new(WXEDID_TYPE_ITEM);
   for (u32_t idx=0; idx<subg_cnt; idx++) {
      edi_grp_cl* psubg = pgrp->getSubGroup(idx);
      if (psubg == NULL) continue;
      wxedid_item* child = wxedid_item_new(psubg, it->pEDID);
      g_list_store_append(store, child);
      g_object_unref(child);
   }
   return G_LIST_MODEL(store);
}

static bool tree_group_matches(edi_grp_cl* group, EDID_cl* edid,
                               const char* query) {
   if (group == NULL) return false;
   wxc_String name;
   group->getGrpName(*edid, name);
   std::string label = group->CodeName.IsEmpty()
      ? name.std_str() : group->CodeName.std_str() + ": " + name.std_str();
   char* folded = g_utf8_casefold(label.c_str(), -1);
   bool matches = strstr(folded, query) != NULL;
   g_free(folded);
   if (matches) return true;
   for (u32_t index=0; index<group->getSubGrpCount(); index++) {
      if (tree_group_matches(group->getSubGroup(index), edid, query)) return true;
   }
   return false;
}

gboolean tree_filter_match(gpointer object, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   if (wnd->tree_query.empty()) return TRUE;

   GtkTreeListRow* row = GTK_TREE_LIST_ROW(object);
   GObject* child = G_OBJECT(gtk_tree_list_row_get_item(row));
   if (child == NULL) return FALSE;
   wxedid_item* item = WXEDID_ITEM(child);
   bool matches = false;
   if (item->pgrp != NULL) {
      matches = tree_group_matches(item->pgrp, item->pEDID,
                                   wnd->tree_query.c_str());
   } else {
      char* folded = g_utf8_casefold(item->label, -1);
      matches = strstr(folded, wnd->tree_query.c_str()) != NULL;
      g_free(folded);
      if (! matches && (item->pgrp_ar != NULL)) {
         for (u32_t index=0; index<item->pgrp_ar->GetCount(); index++) {
            if (tree_group_matches(item->pgrp_ar->Item(index), item->pEDID,
                                   wnd->tree_query.c_str())) {
               matches = true;
               break;
            }
         }
      }
   }
   g_object_unref(child);
   return matches;
}

static void wnd_update_search_state(wxedid_wnd* wnd) {
   bool no_results = ! wnd->tree_query.empty() &&
      (g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_filtered)) == 0);
   gtk_stack_set_visible_child_name(wnd->sidebar_stack,
                                    no_results ? "empty" : "tree");
}

void tree_filter_items_changed(GListModel*, guint, guint, guint,
                               gpointer user_data) {
   wnd_update_search_state(static_cast<wxedid_wnd*>(user_data));
}

static guint wnd_filtered_position(wxedid_wnd* wnd, edi_grp_cl* group) {
   guint count = g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_filtered));
   for (guint position=0; position<count; position++) {
      GtkTreeListRow* row = GTK_TREE_LIST_ROW(
         g_list_model_get_item(G_LIST_MODEL(wnd->tree_filtered), position));
      GObject* object = G_OBJECT(gtk_tree_list_row_get_item(row));
      bool found = WXEDID_ITEM(object)->pgrp == group;
      g_object_unref(object);
      g_object_unref(row);
      if (found) return position;
   }
   return GTK_INVALID_LIST_POSITION;
}

//open groups whose sub-groups match the search, so the matches show
static void wnd_expand_matches(wxedid_wnd* wnd) {
   if (wnd->tree_query.empty() || (wnd->tree_model == NULL)) return;
   guint position = 0;
   while (position < g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_model))) {
      GtkTreeListRow* row = GTK_TREE_LIST_ROW(
         g_list_model_get_item(G_LIST_MODEL(wnd->tree_model), position));
      GObject* object = G_OBJECT(gtk_tree_list_row_get_item(row));
      edi_grp_cl* group = WXEDID_ITEM(object)->pgrp;
      if ((group != NULL) && ! gtk_tree_list_row_get_expanded(row)) {
         for (u32_t index=0; index<group->getSubGrpCount(); index++) {
            if (tree_group_matches(group->getSubGroup(index), WXEDID_ITEM(object)->pEDID,
                                   wnd->tree_query.c_str())) {
               gtk_tree_list_row_set_expanded(row, TRUE);
               break;
            }
         }
      }
      g_object_unref(object);
      g_object_unref(row);
      position++;
   }
}

void tree_search_changed(GtkSearchEntry* entry, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   char* folded = g_utf8_casefold(gtk_editable_get_text(GTK_EDITABLE(entry)), -1);
   wnd->tree_query = folded;
   g_free(folded);
   wnd_expand_matches(wnd);
   gtk_filter_changed(GTK_FILTER(wnd->tree_filter), GTK_FILTER_CHANGE_DIFFERENT);
   wnd_update_search_state(wnd);
   if ((wnd->last_selected != NULL) &&
       (gtk_single_selection_get_selected_item(wnd->tree_sel) == NULL)) {
      guint position = wnd_filtered_position(wnd, wnd->last_selected);
      if (position != GTK_INVALID_LIST_POSITION)
         gtk_single_selection_set_selected(wnd->tree_sel, position);
   }
}

void tree_search_clear(GtkButton*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   gtk_editable_set_text(GTK_EDITABLE(wnd->tree_search), "");
   gtk_widget_grab_focus(GTK_WIDGET(wnd->tree_search));
}

//------------
// factory: tree cell shows the group name
void tree_name_setup(GtkSignalListItemFactory* /*factory*/,
                     GtkListItem* item, gpointer user_data) {
   //group name, with its code and offset underneath
   GtkWidget* expander = gtk_tree_expander_new();
   GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   GtkWidget* lbl = gtk_label_new(NULL);
   gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
   gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
   gtk_widget_set_hexpand(lbl, TRUE);
   gtk_box_append(GTK_BOX(content), lbl);

   GtkWidget* offset = gtk_label_new(NULL);
   gtk_label_set_xalign(GTK_LABEL(offset), 0.0);
   gtk_label_set_ellipsize(GTK_LABEL(offset), PANGO_ELLIPSIZE_END);
   gtk_widget_add_css_class(offset, "caption");
   gtk_widget_add_css_class(offset, "dim-label");
   gtk_box_append(GTK_BOX(content), offset);

   gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), content);
   gtk_list_item_set_child(item, expander);
   g_object_set_data(G_OBJECT(item), "wxedid-wnd", user_data);
   GtkGesture* context = gtk_gesture_click_new();
   gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context),
                                 GDK_BUTTON_SECONDARY);
   g_signal_connect(context, "pressed",
                    G_CALLBACK(wnd_on_tree_item_context), item);
   gtk_widget_add_controller(expander, GTK_EVENT_CONTROLLER(context));
}

void tree_name_bind(GtkSignalListItemFactory* /*factory*/,
                    GtkListItem* item, gpointer /*user_data*/) {
   GtkTreeListRow* row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(item));
   GtkTreeExpander* expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(item));
   gtk_tree_expander_set_list_row(expander, row);

   GObject* obj    = G_OBJECT(gtk_tree_list_row_get_item(row));
   GtkWidget* cell = gtk_tree_expander_get_child(expander);
   GtkWidget* label = gtk_widget_get_first_child(cell);
   GtkWidget* offset = gtk_widget_get_last_child(cell);

   wxedid_item* it = WXEDID_ITEM(obj);
   it->bound_label = GTK_LABEL(label);
   std::string  display_name;
   if ((it != NULL) && (it->pgrp != NULL) && (it->pEDID != NULL)) {
      display_name = edid_group_display_name(it->pgrp, *it->pEDID);
   } else if (it != NULL) {
      display_name = it->label;
   }
   gtk_label_set_text(GTK_LABEL(label), display_name.c_str());
   gtk_widget_set_tooltip_text(label, display_name.c_str());
   gtk_list_item_set_selectable(item, (it != NULL) && it->selectable);
   //block headings only expand: no hover or activation
   gtk_list_item_set_activatable(item, (it != NULL) && it->selectable);

   if ((it != NULL) && (it->pgrp == NULL)) {
      gtk_widget_add_css_class(label, "heading");
   } else {
      gtk_widget_remove_css_class(label, "heading");
   }

   if ((it != NULL) && (it->pgrp != NULL)) {
      char offset_text[96];
      snprintf(offset_text, sizeof(offset_text), "%s · 0x%03X",
               it->pgrp->CodeName.c_str(), it->pgrp->getAbsOffs());
      gtk_label_set_text(GTK_LABEL(offset), offset_text);
      gtk_widget_set_visible(offset, TRUE);
   } else {
      gtk_widget_set_visible(offset, FALSE);
   }
   g_object_unref(obj);
}

void tree_name_unbind(GtkSignalListItemFactory* /*factory*/,
                      GtkListItem* item, gpointer /*user_data*/) {
   GtkTreeListRow* row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(item));
   if (row == NULL) return;
   GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
   if (obj == NULL) return;
   wxedid_item* it = WXEDID_ITEM(obj);
   it->bound_label = NULL;
   g_object_unref(obj);
}

static void wnd_refresh_tree_label(wxedid_item* item) {
   if ((item != NULL) && (item->pgrp != NULL) && (item->bound_label != NULL)) {
      std::string name = edid_group_display_name(item->pgrp, *item->pEDID);
      gtk_label_set_text(item->bound_label, name.c_str());
      gtk_widget_set_tooltip_text(GTK_WIDGET(item->bound_label), name.c_str());
   }
}

edi_grp_cl* wnd_selected_group(wxedid_wnd* wnd) {
   GtkTreeListRow* row = GTK_TREE_LIST_ROW(
      gtk_single_selection_get_selected_item(wnd->tree_sel));
   if (row == NULL) return NULL;
   GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
   if (obj == NULL) return NULL;
   edi_grp_cl* group = WXEDID_ITEM(obj)->pgrp;
   g_object_unref(obj);
   return group;
}

static edi_grp_cl* wnd_root_group(edi_grp_cl* group) {
   while ((group != NULL) && (group->getParentGrp() != NULL))
      group = group->getParentGrp();
   return group;
}

static u8_t wnd_selected_extension_tag(wxedid_wnd* wnd) {
   edi_grp_cl* root = wnd_root_group(wnd_selected_group(wnd));
   if (root == NULL) return 0;
   u32_t block = root->getAbsOffs() / sizeof(ediblk_t);
   if ((block == 0) || (block >= wnd->doc->EDID.getNumValidBlocks())) return 0;
   return wnd->doc->EDID.getEDID()->blk[block][0];
}

void wnd_update_group_actions(wxedid_wnd* wnd) {
   if (wnd->duplicate_action == NULL) return;
   edi_grp_cl* group = wnd_selected_group(wnd);
   GroupAr_cl* array = (group != NULL) ? group->getParentAr() : NULL;
   u32_t index = (group != NULL) ? group->getParentArIdx() : 0;
   gtid_t type = {};
   if (group != NULL) type = group->getTypeID();

   bool duplicate = (array != NULL) && ! type.t_gp_fixed && ! type.t_no_copy &&
                    array->CanInsertDn(index, group);
   g_simple_action_set_enabled(wnd->duplicate_action, duplicate);
   g_simple_action_set_enabled(wnd->delete_action,
                               (array != NULL) && array->CanDelete(index));
   g_simple_action_set_enabled(wnd->move_up_action,
                               (array != NULL) && array->CanMoveUp(index));
   g_simple_action_set_enabled(wnd->move_down_action,
                               (array != NULL) && array->CanMoveDn(index));

   edid_timing_layout layout;
   g_simple_action_set_enabled(wnd->make_preferred_action,
                               (group != NULL) && edid_timing_layout_of(group, layout));

   u8_t tag = wnd_selected_extension_tag(wnd);
   g_simple_action_set_enabled(wnd->add_cta_action, tag == 0x02);
   g_simple_action_set_enabled(wnd->add_displayid_action, tag == 0x70);
   //groups can only be added to CTA-861 and DisplayID blocks
   bool can_add = (tag == 0x02) || (tag == 0x70);
   gtk_widget_set_sensitive(wnd->add_button, can_add);
   gtk_widget_set_tooltip_text(wnd->add_button, can_add ? _("Add a group") :
      _("Select a group in a CTA-861 or DisplayID block to add groups"));
}

void wnd_refresh_raw_view(wxedid_wnd* wnd) {
   if (wnd->raw_view == NULL) return;

   GtkTreeListRow* row = GTK_TREE_LIST_ROW(
      gtk_single_selection_get_selected_item(wnd->tree_sel));
   if (row == NULL) {
      gtk_text_buffer_set_text(gtk_text_view_get_buffer(wnd->raw_view), "", -1);
      return;
   }

   GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
   if (obj == NULL) return;
   wxedid_item* item = WXEDID_ITEM(obj);
   const u8_t* data = NULL;
   u32_t size = 0;
   u32_t offset = 0;
   if (item->pgrp != NULL) {
      data = item->pgrp->getInstPtr();
      size = item->pgrp->getTotalSize();
      offset = item->pgrp->getAbsOffs();
   } else if (item->raw_block >= 0) {
      data = item->pEDID->getEDID()->blk[item->raw_block];
      size = sizeof(ediblk_t);
      offset = static_cast<u32_t>(item->raw_block) * sizeof(ediblk_t);
   }

   //bytes of the highlighted field, relative to the shown data
   edi_dynfld_t* field = (item->pgrp != NULL) && (item->pgrp == wnd->highlight_group)
      ? wnd->highlight_field : NULL;
   u32_t mark_start = 0;
   u32_t mark_count = 0;
   if (field != NULL) {
      long start = (field->base + field->field.offs) - data;
      bool bits = (field->field.flags & (F_BIT | F_BFD)) != 0;
      u32_t count = bits ? 1 : std::max<u32_t>(1, field->field.fld_sz);
      if ((start >= 0) && (static_cast<u32_t>(start) < size)) {
         mark_start = static_cast<u32_t>(start);
         mark_count = std::min(count, size - mark_start);
      }
   }
   if (mark_count > 0) {
      std::string name = edid_field_display_name(field->field.name);
      char where[160];
      u32_t first = offset + mark_start;
      if ((field->field.flags & F_BIT) != 0) {
         snprintf(where, sizeof(where), _("%s · bit %u of byte 0x%03X"),
                  name.c_str(), field->field.shift, first);
      } else if (mark_count == 1) {
         snprintf(where, sizeof(where), _("%s · byte 0x%03X"), name.c_str(), first);
      } else {
         snprintf(where, sizeof(where), _("%s · bytes 0x%03X–0x%03X"), name.c_str(),
                  first, first + mark_count - 1);
      }
      gtk_label_set_text(wnd->raw_caption, where);
   } else {
      gtk_label_set_text(wnd->raw_caption, _("Select a field to mark its bytes"));
   }

   GString* text = g_string_new("Offset  Hex bytes                                         Text\n");
   for (u32_t pos=0; pos<size; pos += 16) {
      g_string_append_printf(text, "%04X    ", offset + pos);
      for (u32_t byte=0; byte<16; byte++) {
         if (pos + byte < size) {
            g_string_append_printf(text, "%02X ", data[pos + byte]);
         } else {
            g_string_append(text, "   ");
         }
      }
      g_string_append(text, " ");
      for (u32_t byte=0; byte<16 && pos + byte<size; byte++) {
         u8_t value = data[pos + byte];
         g_string_append_c(text, g_ascii_isprint(value) ? static_cast<char>(value) : '.');
      }
      g_string_append_c(text, '\n');
   }
   GtkTextBuffer* buffer = gtk_text_view_get_buffer(wnd->raw_view);
   if ((text->len > 0) && (text->str[text->len - 1] == '\n')) g_string_truncate(text, text->len - 1);
   gtk_text_buffer_set_text(buffer, text->str, -1);
   g_string_free(text, TRUE);
   GtkTextIter heading_start;
   GtkTextIter heading_end;
   gtk_text_buffer_get_iter_at_line(buffer, &heading_start, 0);
   heading_end = heading_start;
   gtk_text_iter_forward_to_line_end(&heading_end);
   gtk_text_buffer_apply_tag_by_name(buffer, "heading", &heading_start, &heading_end);
   for (u32_t pos=mark_start; pos<mark_start + mark_count; pos++) {
      //line 0 is the heading; hex pairs start at column 8, text at 57
      GtkTextIter from;
      GtkTextIter to;
      int line = 1 + static_cast<int>(pos / 16);
      int column = 8 + static_cast<int>(pos % 16) * 3;
      gtk_text_buffer_get_iter_at_line_offset(buffer, &from, line, column);
      gtk_text_buffer_get_iter_at_line_offset(buffer, &to, line, column + 2);
      gtk_text_buffer_apply_tag_by_name(buffer, "field", &from, &to);
      column = 57 + static_cast<int>(pos % 16);
      gtk_text_buffer_get_iter_at_line_offset(buffer, &from, line, column);
      gtk_text_buffer_get_iter_at_line_offset(buffer, &to, line, column + 1);
      gtk_text_buffer_apply_tag_by_name(buffer, "field", &from, &to);
   }
   g_object_unref(obj);
}

void wnd_refresh_selected_tree_label(wxedid_wnd* wnd) {
   GtkTreeListRow* row = GTK_TREE_LIST_ROW(
      gtk_single_selection_get_selected_item(wnd->tree_sel));
   if (row == NULL) return;
   GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
   if (obj == NULL) return;
   wnd_refresh_tree_label(WXEDID_ITEM(obj));
   g_object_unref(obj);
}

void wnd_refresh_group_tree_label(wxedid_wnd* wnd, edi_grp_cl* group) {
   guint count = g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_model));
   for (guint position=0; position<count; position++) {
      GtkTreeListRow* row = GTK_TREE_LIST_ROW(
         g_list_model_get_item(G_LIST_MODEL(wnd->tree_model), position));
      GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
      wxedid_item* item = WXEDID_ITEM(obj);
      if (item->pgrp == group) wnd_refresh_tree_label(item);
      g_object_unref(obj);
      g_object_unref(row);
   }
}

void wnd_on_tree_select(GtkSelectionModel* selmodel, guint /*position*/,
                        guint /*n_items*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;

   GtkTreeListRow* row = (GtkTreeListRow*) gtk_single_selection_get_selected_item(GTK_SINGLE_SELECTION(selmodel));
   if (row == NULL) return;

   GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
   if (obj == NULL) return;

   wxedid_item* it = WXEDID_ITEM(obj);
   wnd->last_selected = it->pgrp;
   if (wnd->overview_shown) {
      wnd->overview_shown = false;
      gtk_list_box_unselect_all(wnd->overview_list);
      adw_view_stack_page_set_visible(wnd->overview_page, FALSE);
   }
   std::string group_name = (it->pgrp != NULL)
      ? edid_group_display_name(it->pgrp, *it->pEDID) : std::string(it->label);
   gtk_label_set_text(wnd->group_title, group_name.c_str());
   if (it->pgrp != NULL) {
      char where[128];
      snprintf(where, sizeof(where), _("%s · offset 0x%03X · block %u"),
               it->pgrp->CodeName.c_str(), it->pgrp->getAbsOffs(),
               it->pgrp->getAbsOffs() / static_cast<u32_t>(sizeof(ediblk_t)));
      gtk_label_set_text(wnd->group_subtitle, where);
   }
   gtk_widget_set_visible(GTK_WIDGET(wnd->group_subtitle), it->pgrp != NULL);
   fields_refresh(wnd->fields, it->pgrp, *it->pEDID);
   bool has_timing = timing_load_group(wnd->timing, it->pgrp, it->pEDID);
   wnd_refresh_raw_view(wnd);
   gtk_widget_set_visible(wnd->editor_switcher, TRUE);
   adw_view_stack_page_set_visible(wnd->timing_stack_page, has_timing);
   adw_view_stack_set_visible_child_name(wnd->editor_stack,
                                         has_timing ? "timing" :
                                         (it->pgrp != NULL ? "fields" : "bytes"));
   if (adw_overlay_split_view_get_collapsed(wnd->split_view)) {
      adw_overlay_split_view_set_show_sidebar(wnd->split_view, FALSE);
   }
   wnd_update_group_actions(wnd);
   gtk_list_view_scroll_to(wnd->tree, gtk_single_selection_get_selected(wnd->tree_sel),
                           GTK_LIST_SCROLL_NONE, NULL);
   //leaving a group applies its pending rebuild
   wnd_schedule_refresh(wnd);

   g_object_unref(obj);   //gtk_tree_list_row_get_item() transfers a full ref
}

static void wnd_finish_structure_change(wxedid_wnd* wnd,
                                        edi_grp_cl* selection,
                                        const char* message) {
   wnd->invalid_fields = 0;
   wnd_rebuild_tree(wnd, selection);
   wnd_update_history_state(wnd);
   wnd_update_document_ui(wnd);
   adw_toast_overlay_add_toast(wnd->toast_overlay, adw_toast_new(message));
}

void wnd_on_duplicate_group(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   wnd_flush_refresh(wnd);
   edi_grp_cl* group = wnd_selected_group(wnd);
   GroupAr_cl* array = (group != NULL) ? group->getParentAr() : NULL;
   if (array == NULL) return;

   rcode result;
   edi_grp_cl* copy = group->Clone(result, T_MODE_EDIT);
   if ((copy == NULL) || ! RCD_IS_OK(result) ||
       ! array->CanInsertDn(group->getParentArIdx(), copy)) {
      delete copy;
      wnd_show_error(wnd, _("This group cannot be duplicated in the available space"));
      return;
   }
   array->InsertDn(group->getParentArIdx(), copy);
   wnd_record_structure(wnd, HISTORY_INSERT, copy, array, copy->getParentArIdx(),
                        false, copy->getParentGrp());
   wnd_finish_structure_change(wnd, copy, _("Group duplicated"));
}

static void wnd_move_group(wxedid_wnd* wnd, bool up) {
   wnd_flush_refresh(wnd);
   edi_grp_cl* group = wnd_selected_group(wnd);
   GroupAr_cl* array = (group != NULL) ? group->getParentAr() : NULL;
   if (array == NULL) return;
   u32_t index = group->getParentArIdx();
   if (up ? array->CanMoveUp(index) : array->CanMoveDn(index)) {
      if (up) array->MoveUp(index); else array->MoveDn(index);
      wnd_record_structure(wnd, HISTORY_MOVE, group, array, index, up,
                           group->getParentGrp());
      wnd_finish_structure_change(wnd, group,
                                  up ? _("Group moved up") : _("Group moved down"));
   }
}

void wnd_on_move_group_up(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_move_group(static_cast<wxedid_wnd*>(user_data), true);
}

void wnd_on_move_group_down(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_move_group(static_cast<wxedid_wnd*>(user_data), false);
}

static void wnd_on_delete_group_response(GObject* source, GAsyncResult* result,
                                         gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   const char* response = adw_alert_dialog_choose_finish(
      ADW_ALERT_DIALOG(source), result);
   edi_grp_cl* group = wnd->pending_delete;
   wnd->pending_delete = NULL;
   if ((0 != strcmp(response, "delete")) || (group == NULL)) return;
   GroupAr_cl* array = group->getParentAr();
   if ((array == NULL) || ! array->CanDelete(group->getParentArIdx())) return;

   u32_t index = group->getParentArIdx();
   edi_grp_cl* parent = group->getParentGrp();
   edi_grp_cl* next = (index + 1 < array->GetCount()) ? array->Item(index + 1) :
                      (index > 0) ? array->Item(index - 1) : parent;
   if (array->Cut(index) != group) {
      wnd_show_error(wnd, _("This group couldn’t be deleted"));
      return;
   }
   wnd_record_structure(wnd, HISTORY_REMOVE, group, array, index, false, parent);
   wnd_finish_structure_change(wnd, next, _("Group deleted"));
}

void wnd_on_delete_group(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   if (wnd->pending_delete != NULL) return;
   wnd_flush_refresh(wnd);
   edi_grp_cl* group = wnd_selected_group(wnd);
   if ((group == NULL) || (group->getParentAr() == NULL) ||
       ! group->getParentAr()->CanDelete(group->getParentArIdx())) return;

   wxc_String name;
   group->getGrpName(wnd->doc->EDID, name);
   char body[512];
   snprintf(body, sizeof(body),
            _("“%s” and its fields will be removed from this EDID."), name.c_str());
   AdwAlertDialog* dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new(
      _("Delete this group?"), body));
   adw_alert_dialog_add_responses(dialog,
                                  "cancel", _("Cancel"),
                                  "delete", _("Delete"),
                                  NULL);
   adw_alert_dialog_set_close_response(dialog, "cancel");
   adw_alert_dialog_set_default_response(dialog, "cancel");
   adw_alert_dialog_set_response_appearance(dialog, "delete",
                                            ADW_RESPONSE_DESTRUCTIVE);
   wnd->pending_delete = group;
   adw_alert_dialog_choose(dialog, GTK_WIDGET(wnd->window), NULL,
                           wnd_on_delete_group_response, wnd);
}

static GroupAr_cl* wnd_selected_root_array(wxedid_wnd* wnd) {
   edi_grp_cl* root = wnd_root_group(wnd_selected_group(wnd));
   return (root != NULL) ? root->getParentAr() : NULL;
}

void wnd_on_add_cta_group(GSimpleAction*, GVariant* parameter,
                          gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   wnd_flush_refresh(wnd);
   const char* value = g_variant_get_string(parameter, NULL);
   EDID_cl::group_template which = EDID_cl::CEA_AUDIO_LPCM;
   if (0 == strcmp(value, "audio-extended")) which = EDID_cl::CEA_AUDIO_EXTENDED;
   else if (0 == strcmp(value, "video")) which = EDID_cl::CEA_VIDEO;
   else if (0 == strcmp(value, "timing")) which = EDID_cl::CEA_TIMING;

   edi_grp_cl* group = NULL;
   rcode result = wnd->doc->EDID.CreateGroup(which, 0, &group);
   //a new timing starts as a copy of the selected one, or of the first
   if (RCD_IS_OK(result) && (which == EDID_cl::CEA_TIMING)) {
      edi_grp_cl* source = wnd_selected_group(wnd);
      edid_timing_layout layout;
      if ((source == NULL) || ! edid_timing_layout_of(source, layout) ||
          ! edid_timing_copy(wnd->doc->EDID, source, group)) {
         source = edid_first_timing(wnd->doc->EDID);
         if (source != NULL) edid_timing_copy(wnd->doc->EDID, source, group);
      }
   }
   GroupAr_cl* array = wnd_selected_root_array(wnd);
   if (! RCD_IS_OK(result) || ! edid_insert_group(array, group)) {
      delete group;
      wnd_show_error(wnd, _("This CTA group does not fit in the selected block"));
      return;
   }
   wnd_record_structure(wnd, HISTORY_INSERT, group, array, group->getParentArIdx(),
                        false, NULL);
   wnd_finish_structure_change(wnd, group, _("CTA group added"));
}

void wnd_on_add_displayid_group(GSimpleAction*, GVariant*,
                                gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   wnd_flush_refresh(wnd);
   GroupAr_cl* array = wnd_selected_root_array(wnd);
   if ((array == NULL) || (array->GetCount() == 0)) return;
   u8_t version = array->Item(0)->getInstPtr()[1];
   edi_grp_cl* group = NULL;
   rcode result = wnd->doc->EDID.CreateGroup(
      EDID_cl::DISPLAYID_DATA, version, &group);
   if (! RCD_IS_OK(result) || ! edid_insert_group(array, group)) {
      delete group;
      wnd_show_error(wnd, _("A DisplayID block does not fit in the selected section"));
      return;
   }
   wnd_record_structure(wnd, HISTORY_INSERT, group, array, group->getParentArIdx(),
                        false, NULL);
   wnd_finish_structure_change(wnd, group, _("DisplayID data block added"));
}

static void wnd_on_prefer_details(GObject* source, GAsyncResult* result, gpointer) {
   adw_alert_dialog_choose_finish(ADW_ALERT_DIALOG(source), result);
}

void wnd_make_preferred(wxedid_wnd* wnd, edi_grp_cl* timing) {
   wnd_flush_refresh(wnd);
   std::vector<edid_data_change> changes;
   std::string message;
   edid_prefer_way way = PREFER_ALREADY_FIRST;
   if (! edid_plan_preferred(wnd->doc->EDID, timing, changes, message, &way)) {
      wnd_show_error(wnd, message.c_str());
      return;
   }
   std::string name = edid_group_display_name(timing, wnd->doc->EDID);
   //after a swap the mode sits in the first timing
   edi_grp_cl* selection = (way == PREFER_SWAPPED) ? edid_first_timing(wnd->doc->EDID) : timing;
   wnd_apply_changes(wnd, changes, selection);
   if (way == PREFER_FLAGGED) {
      AdwAlertDialog* dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new(
         _("Flagged as Preferred in DisplayID"), message.c_str()));
      adw_alert_dialog_add_response(dialog, "close", _("Close"));
      adw_alert_dialog_choose(dialog, GTK_WIDGET(wnd->window), NULL,
                              wnd_on_prefer_details, NULL);
      return;
   }
   char* text = g_strdup_printf((way == PREFER_SWAPPED) ? _("%s is now the preferred timing")
                                                        : _("%s is already the preferred timing"),
                                name.c_str());
   adw_toast_overlay_add_toast(wnd->toast_overlay, adw_toast_new(text));
   g_free(text);
}

void wnd_on_make_preferred(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   wnd_make_preferred(wnd, wnd_selected_group(wnd));
}

void wnd_popup_group_menu(wxedid_wnd* wnd, double x, double y) {
   if ((wnd->group_menu == NULL) || (wnd_selected_group(wnd) == NULL)) return;
   GdkRectangle point = {
      static_cast<int>(x), static_cast<int>(y), 1, 1,
   };
   gtk_popover_set_pointing_to(GTK_POPOVER(wnd->group_menu), &point);
   gtk_popover_popup(GTK_POPOVER(wnd->group_menu));
}

void wnd_on_tree_item_context(GtkGestureClick* gesture, int /*presses*/,
                              double x, double y, gpointer user_data) {
   GtkListItem* item = GTK_LIST_ITEM(user_data);
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(
      g_object_get_data(G_OBJECT(item), "wxedid-wnd"));
   if ((wnd == NULL) || ! gtk_list_item_get_selectable(item)) return;
   gtk_single_selection_set_selected(wnd->tree_sel,
                                     gtk_list_item_get_position(item));
   GtkWidget* source = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
   graphene_point_t source_point = GRAPHENE_POINT_INIT(
      static_cast<float>(x), static_cast<float>(y));
   graphene_point_t tree_point;
   if (! gtk_widget_compute_point(source, GTK_WIDGET(wnd->tree),
                                  &source_point, &tree_point))
      tree_point = source_point;
   wnd_popup_group_menu(wnd, tree_point.x, tree_point.y);
}

gboolean wnd_on_tree_key(GtkEventControllerKey*, guint keyval,
                         guint /*keycode*/, GdkModifierType state,
                         gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   GdkModifierType modifiers = static_cast<GdkModifierType>(
      state & gtk_accelerator_get_default_mod_mask());
   if ((keyval == GDK_KEY_Delete) && (modifiers == 0)) {
      g_action_activate(G_ACTION(wnd->delete_action), NULL);
      return TRUE;
   }
   if ((keyval == GDK_KEY_d) && (modifiers == GDK_CONTROL_MASK)) {
      g_action_activate(G_ACTION(wnd->duplicate_action), NULL);
      return TRUE;
   }
   if ((keyval == GDK_KEY_Up) && (modifiers == GDK_ALT_MASK)) {
      g_action_activate(G_ACTION(wnd->move_up_action), NULL);
      return TRUE;
   }
   if ((keyval == GDK_KEY_Down) && (modifiers == GDK_ALT_MASK)) {
      g_action_activate(G_ACTION(wnd->move_down_action), NULL);
      return TRUE;
   }
   if ((keyval == GDK_KEY_Menu) ||
       ((keyval == GDK_KEY_F10) && (modifiers == GDK_SHIFT_MASK))) {
      wnd_popup_group_menu(wnd, 24, 24);
      return TRUE;
   }
   return FALSE;
}

void wnd_rebuild_tree(wxedid_wnd* wnd, edi_grp_cl* select_group) {
   EDID_cl& edid = wnd->doc->EDID;
   edi_buf_t* buffer = edid.getEDID();
   GListStore* root = g_list_store_new(WXEDID_TYPE_ITEM);

   if (edid.EDI_BaseGrpAr.GetCount() > 0) {
      wxedid_item* base = wxedid_item_new_block(
         _("Block 0: Base EDID"), &edid.EDI_BaseGrpAr, &edid);
      g_list_store_append(root, base);
      g_object_unref(base);
   }

   for (u32_t block=1; block<edid.getNumValidBlocks(); block++) {
      GroupAr_cl* groups = edid.BlkGroupsAr[block];
      if (groups->GetCount() > 0) {
         const char* type = (buffer->blk[block][0] == 0x02) ? "CTA-861" :
                            (buffer->blk[block][0] == 0x70) ? "DisplayID" :
                                                             _("Extension");
         char label[64];
         snprintf(label, sizeof(label), _("Block %u: %s"), block, type);
         wxedid_item* section = wxedid_item_new_block(label, groups, &edid);
         g_list_store_append(root, section);
         g_object_unref(section);
      } else {
         wxedid_item* item = wxedid_item_new_raw_extension(
            block, buffer->blk[block][0], &edid);
         g_list_store_append(root, item);
         g_object_unref(item);
      }
   }

   wnd->last_selected = NULL;
   wnd->highlight_group = NULL;
   wnd->highlight_field = NULL;
   gtk_single_selection_set_selected(wnd->tree_sel, GTK_INVALID_LIST_POSITION);
   gtk_filter_list_model_set_model(wnd->tree_filtered, NULL);
   g_clear_object(&wnd->tree_model);
   //the tree list model takes ownership of root
   wnd->tree_model = gtk_tree_list_model_new(
      G_LIST_MODEL(root), FALSE, FALSE, tree_item_expand, NULL, NULL);
   gtk_editable_set_text(GTK_EDITABLE(wnd->tree_search), "");
   gtk_filter_list_model_set_model(wnd->tree_filtered,
                                   G_LIST_MODEL(wnd->tree_model));

   guint position = 0;
   while (position < g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_model))) {
      GtkTreeListRow* row = GTK_TREE_LIST_ROW(
         g_list_model_get_item(G_LIST_MODEL(wnd->tree_model), position));
      GObject* object = G_OBJECT(gtk_tree_list_row_get_item(row));
      if (WXEDID_ITEM(object)->pgrp_ar != NULL)
         gtk_tree_list_row_set_expanded(row, TRUE);
      g_object_unref(object);
      g_object_unref(row);
      position++;
   }

   //open the groups that hold the one to select, as for a DisplayID timing
   for (guint row_at=0; (select_group != NULL) &&
        (row_at<g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_model))); row_at++) {
      GtkTreeListRow* row = GTK_TREE_LIST_ROW(
         g_list_model_get_item(G_LIST_MODEL(wnd->tree_model), row_at));
      GObject* object = G_OBJECT(gtk_tree_list_row_get_item(row));
      edi_grp_cl* holder = WXEDID_ITEM(object)->pgrp;
      for (edi_grp_cl* up = select_group->getParentGrp(); (holder != NULL) && (up != NULL);
           up = up->getParentGrp()) {
         if (up == holder) gtk_tree_list_row_set_expanded(row, TRUE);
      }
      g_object_unref(object);
      g_object_unref(row);
   }

   //select the requested group, or the first selectable row
   edi_grp_cl* target_group = NULL;
   int target_block = -1;
   guint count = g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_filtered));
   for (position=0; position<count; position++) {
      GtkTreeListRow* row = GTK_TREE_LIST_ROW(
         g_list_model_get_item(G_LIST_MODEL(wnd->tree_filtered), position));
      GObject* object = G_OBJECT(gtk_tree_list_row_get_item(row));
      wxedid_item* item = WXEDID_ITEM(object);
      bool first = (target_group == NULL) && (target_block < 0) && item->selectable;
      bool requested = (select_group != NULL) && (item->pgrp == select_group);
      if (first || requested) {
         target_group = item->pgrp;
         target_block = item->raw_block;
      }
      g_object_unref(object);
      g_object_unref(row);
      if (requested) break;
   }
   if ((target_group == NULL) && (target_block < 0)) return;

   //selecting can deliver pending row insertions that shift positions, so
   //select by item and correct the position once if it moved
   for (int attempt=0; attempt<2; attempt++) {
      guint target = GTK_INVALID_LIST_POSITION;
      count = g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_filtered));
      for (position=0; (position<count) && (target == GTK_INVALID_LIST_POSITION); position++) {
         GtkTreeListRow* row = GTK_TREE_LIST_ROW(
            g_list_model_get_item(G_LIST_MODEL(wnd->tree_filtered), position));
         GObject* object = G_OBJECT(gtk_tree_list_row_get_item(row));
         wxedid_item* item = WXEDID_ITEM(object);
         if ((item->pgrp == target_group) && (item->raw_block == target_block) &&
             item->selectable) target = position;
         g_object_unref(object);
         g_object_unref(row);
      }
      if (target == GTK_INVALID_LIST_POSITION) return;
      if (gtk_single_selection_get_selected(wnd->tree_sel) == target) return;
      gtk_single_selection_set_selected(wnd->tree_sel, target);
   }
}
