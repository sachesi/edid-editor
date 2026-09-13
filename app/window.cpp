/***************************************************************
 * Name:      window.cpp
 * Purpose:   main application window (GTK4/libadwaita)
 * License:   GPLv3+
 **************************************************************/

#include "window.h"

#include "wxcompat.h"
#include "wxedid_rcd_scope.h"
#include "guilog.h"
#include "vmap.h"
#include "EDID_class.h"
#include "CEA_class.h"
#include "CEA_ET_class.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <string>

//------------
// per-document state
struct wxedid_doc {
   EDID_cl    EDID;
   guilog_cl  GLog;
   char       path[1024]; //current file, empty if none
};

struct wxedid_wnd {
   wxedid_doc*         doc;
   GtkWindow*          window;
   GtkListView*        tree;
   GtkSingleSelection* tree_sel;
   GtkTreeListModel*   tree_model;
   GtkFlowBox*         fields;
   GtkTextView*        log;
   AdwOverlaySplitView* split_view;
   AdwWindowTitle*     window_title;
   GtkLabel*           group_title;
   GtkStack*           content_stack;
   AdwBanner*          banner;
   AdwToastOverlay*    toast_overlay;
   GtkRevealer*        log_revealer;
   GtkWidget*          details_button;
   GtkWidget*          sidebar_button;
   GtkWidget*          open_button;
   GtkWidget*          save_button;
   GSimpleAction*      save_action;
   GSimpleAction*      details_action;
   bool                loaded;
   bool                dirty;
   bool                banner_is_validation;
   bool                close_confirmation_open;
   bool                details_available;
   u32_t               invalid_fields;
};

static void wnd_update_document_ui(wxedid_wnd* wnd);
static void wnd_show_error(wxedid_wnd* wnd, const char* message);
static void wnd_update_header_controls(wxedid_wnd* wnd);

//tree item: GObject holding an edi_grp_cl* for GtkTreeListModel
struct wxedid_item;
typedef struct wxedid_item wxedid_item;

struct wxedid_itemClass {
   GObjectClass parent_class;
};

#define WXEDID_TYPE_ITEM (wxedid_item_get_type())
#define WXEDID_ITEM(obj) ((wxedid_item*) (obj))

struct wxedid_item {
   GObject      parent;
   edi_grp_cl*  pgrp;
   GroupAr_cl*  pgrp_ar;
   EDID_cl*     pEDID;
   bool         selectable;
   char         label[96];
};

static void wxedid_item_init(wxedid_item*) {}
static void wxedid_item_class_init(wxedid_itemClass*) {}

G_DEFINE_FINAL_TYPE(wxedid_item, wxedid_item, G_TYPE_OBJECT)

static wxedid_item* wxedid_item_new(edi_grp_cl* pgrp, EDID_cl* pEDID) {
   wxedid_item* item = (wxedid_item*) g_object_new(WXEDID_TYPE_ITEM, NULL);
   item->pgrp       = pgrp;
   item->pgrp_ar    = NULL;
   item->pEDID      = pEDID;
   item->selectable = (pgrp != NULL);
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
   const char* type = (tag == 0x70) ? "DisplayID" : "Unsupported";
   snprintf(item->label, sizeof(item->label),
            "Extension %u: %s (0x%02X), preserved read-only", block, type, tag);
   item->selectable = true;
   return item;
}

static void log_sink(const char* msg, void* user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   if ((wnd == NULL) || (wnd->log == NULL)) return;

   GtkTextBuffer* buf = gtk_text_view_get_buffer(wnd->log);
   GtkTextIter end;
   gtk_text_buffer_get_end_iter(buf, &end);
   gtk_text_buffer_insert(buf, &end, msg, -1);
   gtk_text_buffer_insert(buf, &end, "\n", -1);

   wnd->details_available = true;
   g_simple_action_set_enabled(wnd->details_action, TRUE);
   wnd_update_header_controls(wnd);
   if (g_str_has_prefix(msg, "[E!]")) {
      const char* detail = msg + 4;
      while (*detail == ' ') detail++;
      wnd_show_error(wnd, detail);
   }
}

//------------
//field row widgets: what the user interacts with per field
enum {
   ROW_LABEL,     //read-only or not writable: label only
   ROW_ENTRY,     //text entry (OP_WRSTR)
   ROW_COMBO,     //value selector dropdown (F_VS)
};

struct wxedid_row {
   edi_dynfld_t* pfld;
   edi_grp_cl*   pgrp;
   EDID_cl*      pEDID;
   wxedid_wnd*   wnd;
   int           kind;
   GtkWidget*    entry;  //GtkEditable | GtkDropDown
   u32_t         sel_idx; //dropdown item value
   bool          valid;
};

//re-read all rows of the field list into the widgets' current display
static void rows_reload(GtkFlowBox* list, edi_grp_cl* pgrp, EDID_cl* pEDID,
                        wxedid_wnd* wnd);

//------------
// helpers shared by rows
static bool field_writable(const edi_field_t& f) {
   return (0 == (f.flags & F_RD));
}

static bool field_has_selector(const edi_field_t& f) {
   return ((f.flags & F_VS) != 0) && (f.vmap_idx != VS_NO_SELECTOR);
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

static void row_mark_changed(wxedid_row* row) {
   row->wnd->dirty = true;
   wnd_update_document_ui(row->wnd);
}

//------------
//entry changed: write valid text back via the field handler
static void row_on_entry_changed(GtkEditable* entry, gpointer user_data) {
   wxedid_row* r = (wxedid_row*) user_data;

   const char* txt = gtk_editable_get_text(entry);
   wxc_String  sval(txt);
   u32_t       ival = 0;

   rcode retU = ( r->pEDID->*r->pfld->field.handlerfn )(OP_WRSTR, sval, ival, r->pfld);

   if (RCD_IS_OK(retU)) {
      gtk_widget_remove_css_class(GTK_WIDGET(entry), "error");
      row_set_valid(r, true);
      row_mark_changed(r);
   } else {
      gtk_widget_add_css_class(GTK_WIDGET(entry), "error");
      row_set_valid(r, false);
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

   wxc_String sval;
   rcode retU = ( r->pEDID->*r->pfld->field.handlerfn )(OP_WRINT, sval, val, r->pfld);

   if (RCD_IS_OK(retU)) {
      gtk_widget_remove_css_class(GTK_WIDGET(dd), "error");
      row_set_valid(r, true);
      row_mark_changed(r);
   } else {
      gtk_widget_add_css_class(GTK_WIDGET(dd), "error");
      row_set_valid(r, false);
   }
}

//------------
// field list: rebuilt when a group is selected in the tree
static void fields_refresh(GtkFlowBox* list, edi_grp_cl* pgrp, EDID_cl& EDID) {
   wxedid_wnd* wnd = (wxedid_wnd*) g_object_get_data(G_OBJECT(list), "wxedid-wnd");
   rows_reload(list, pgrp, &EDID, wnd);
}

static std::string field_display_name(const char* name) {
   struct field_name {
      const char* raw;
      const char* display;
   };
   static const field_name names[] = {
      {"header", "Header"},
      {"mfc_id", "Manufacturer ID"},
      {"prod_id", "Product ID"},
      {"serial", "Serial number"},
      {"prod_week", "Manufacture week"},
      {"prod_year", "Manufacture year"},
      {"edid_ver", "EDID version"},
      {"edid_rev", "EDID revision"},
      {"num_extblk", "Extension blocks"},
      {"checksum", "Checksum"},
      {"Input Type", "Input type"},
      {"VESA compat", "VESA compatibility"},
      {"IF Type", "Interface type"},
      {"Color Depth", "Color depth"},
   };

   for (const field_name& item : names) {
      if (0 == strcmp(name, item.raw)) return item.display;
   }

   std::string display = name;
   for (char& ch : display) {
      if (ch == '_') ch = ' ';
   }
   if (! display.empty() && (display[0] >= 'a') && (display[0] <= 'z')) {
      display[0] = (char) (display[0] - 'a' + 'A');
   }
   return display;
}

static void rows_reload(GtkFlowBox* list, edi_grp_cl* pgrp, EDID_cl* pEDID,
                        wxedid_wnd* wnd) {
   //drop old rows
   wnd->invalid_fields = 0;
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

   for (u32_t idx=0; idx<cnt; idx++) {
      edi_dynfld_t* pfld = pgrp->FieldsAr.Item(idx);

      sval.Empty();
      ival = 0;
      rcode retU = ( pEDID->*pfld->field.handlerfn )(OP_READ, sval, ival, pfld);

      GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_size_request(card, 240, -1);

      GtkWidget* card_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
      gtk_widget_set_margin_start(card_content, 12);
      gtk_widget_set_margin_end(card_content, 12);
      gtk_widget_set_margin_top(card_content, 12);
      gtk_widget_set_margin_bottom(card_content, 12);
      gtk_box_append(GTK_BOX(card), card_content);

      std::string title = field_display_name(pfld->field.name);
      GtkWidget* label = gtk_label_new(title.c_str());
      gtk_label_set_xalign(GTK_LABEL(label), 0.0);
      gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
      gtk_widget_add_css_class(label, "caption");
      gtk_widget_add_css_class(label, "dim-label");
      gtk_box_append(GTK_BOX(card_content), label);

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
            gtk_drop_down_set_selected(dd, (cur >= 0) ? (guint) cur : GTK_INVALID_LIST_POSITION);
            gtk_widget_set_hexpand(GTK_WIDGET(dd), TRUE);
            gtk_widget_set_halign(GTK_WIDGET(dd), GTK_ALIGN_FILL);

            wxedid_row* r = new wxedid_row{
               pfld, pgrp, pEDID, wnd, ROW_COMBO, GTK_WIDGET(dd), 0, true
            };
            g_object_set_data_full(G_OBJECT(dd), "sel-idx", vals,
                                   [](gpointer data){ delete[] (u32_t*) data; });
            g_object_set_data_full(G_OBJECT(dd), "row", r,
                                   [](gpointer data){ delete (wxedid_row*) data; });

            g_signal_connect(dd, "notify::selected", G_CALLBACK(row_on_combo_notify), r);
            widget = GTK_WIDGET(dd);
         }
      }

      if (widget == NULL) {
         if (field_writable(pfld->field)) {
            //text entry
            GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
            gtk_editable_set_text(GTK_EDITABLE(entry), sval.c_str());
            gtk_widget_set_hexpand(GTK_WIDGET(entry), TRUE);
            gtk_widget_set_halign(GTK_WIDGET(entry), GTK_ALIGN_FILL);

            wxedid_row* r = new wxedid_row{
               pfld, pgrp, pEDID, wnd, ROW_ENTRY, GTK_WIDGET(entry), 0, true
            };

            g_signal_connect(entry, "changed", G_CALLBACK(row_on_entry_changed), r);
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
      gtk_flow_box_append(list, card);
      gtk_widget_add_css_class(gtk_widget_get_parent(card), "card");
   }

   wnd_update_document_ui(wnd);
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

//------------
// factory: tree cell shows the group name
static void tree_name_setup(GtkSignalListItemFactory* /*factory*/,
                            GtkListItem* item, gpointer /*user_data*/) {
   GtkWidget* expander = gtk_tree_expander_new();
   GtkWidget* content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   GtkWidget* lbl = gtk_label_new(NULL);
   gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
   gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
   gtk_widget_set_hexpand(lbl, TRUE);
   gtk_box_append(GTK_BOX(content), lbl);

   GtkWidget* offset = gtk_label_new(NULL);
   gtk_widget_add_css_class(offset, "caption");
   gtk_widget_add_css_class(offset, "dim-label");
   gtk_widget_add_css_class(offset, "monospace");
   gtk_box_append(GTK_BOX(content), offset);

   gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), content);
   gtk_list_item_set_child(item, expander);
}

static void tree_name_bind(GtkSignalListItemFactory* /*factory*/,
                           GtkListItem* item, gpointer /*user_data*/) {
   GtkTreeListRow* row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(item));
   GtkTreeExpander* expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(item));
   gtk_tree_expander_set_list_row(expander, row);

   GObject* obj    = G_OBJECT(gtk_tree_list_row_get_item(row));
   GtkWidget* cell = gtk_tree_expander_get_child(expander);
   GtkWidget* label = gtk_widget_get_first_child(cell);
   GtkWidget* offset = gtk_widget_get_last_child(cell);

   wxedid_item* it = WXEDID_ITEM(obj);
   wxc_String   gname;
   std::string  display_name;
   if ((it != NULL) && (it->pgrp != NULL) && (it->pEDID != NULL)) {
      it->pgrp->getGrpName(*it->pEDID, gname);
      if (! it->pgrp->CodeName.IsEmpty()) {
         display_name = it->pgrp->CodeName.std_str() + ": " + gname.std_str();
      } else {
         display_name = gname.std_str();
      }
   } else if (it != NULL) {
      display_name = it->label;
   }
   gtk_label_set_text(GTK_LABEL(label), display_name.c_str());
   gtk_widget_set_tooltip_text(label, display_name.c_str());
   gtk_list_item_set_selectable(item, (it != NULL) && it->selectable);

   if ((it != NULL) && (it->pgrp == NULL)) {
      gtk_widget_add_css_class(label, "heading");
   } else {
      gtk_widget_remove_css_class(label, "heading");
   }

   if ((it != NULL) && (it->pgrp != NULL)) {
      char offset_text[16];
      snprintf(offset_text, sizeof(offset_text), "0x%03X", it->pgrp->getAbsOffs());
      gtk_label_set_text(GTK_LABEL(offset), offset_text);
      gtk_widget_set_visible(offset, TRUE);
   } else {
      gtk_widget_set_visible(offset, FALSE);
   }
   g_object_unref(obj);
}

static void wnd_on_tree_select(GtkSelectionModel* selmodel, guint /*position*/,
                               guint /*n_items*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;

   GtkTreeListRow* row = (GtkTreeListRow*) gtk_single_selection_get_selected_item(GTK_SINGLE_SELECTION(selmodel));
   if (row == NULL) return;

   GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
   if (obj == NULL) return;

   wxedid_item* it = WXEDID_ITEM(obj);
   wxc_String group_name;
   if (it->pgrp != NULL) {
      it->pgrp->getGrpName(*it->pEDID, group_name);
   } else {
      group_name = it->label;
   }
   gtk_label_set_text(wnd->group_title, group_name.c_str());
   fields_refresh(wnd->fields, it->pgrp, *it->pEDID);
   if (adw_overlay_split_view_get_collapsed(wnd->split_view)) {
      adw_overlay_split_view_set_show_sidebar(wnd->split_view, FALSE);
   }

   g_object_unref(obj);   //gtk_tree_list_row_get_item() transfers a full ref
}

static void wnd_update_document_ui(wxedid_wnd* wnd) {
   bool can_save = wnd->loaded && wnd->dirty && (wnd->invalid_fields == 0);
   g_simple_action_set_enabled(wnd->save_action, can_save);
   gtk_widget_set_visible(wnd->save_button, wnd->loaded);

   if (wnd->loaded) {
      char* basename = g_path_get_basename(wnd->doc->path);
      char* display_path = g_filename_display_name(wnd->doc->path);
      char* window_name = g_strdup_printf("%s — EDID Editor", basename);
      char* subtitle = wnd->dirty
         ? g_strdup_printf("Modified · %s", display_path)
         : g_strdup(display_path);

      adw_window_title_set_title(wnd->window_title, basename);
      adw_window_title_set_subtitle(wnd->window_title, subtitle);
      gtk_window_set_title(wnd->window, window_name);

      g_free(subtitle);
      g_free(window_name);
      g_free(display_path);
      g_free(basename);
   } else {
      adw_window_title_set_title(wnd->window_title, "EDID Editor");
      adw_window_title_set_subtitle(wnd->window_title, "EDID editor");
      gtk_window_set_title(wnd->window, "EDID Editor");
   }

   if (wnd->invalid_fields > 0) {
      adw_banner_set_title(wnd->banner, "Enter a valid value before saving");
      adw_banner_set_button_label(wnd->banner, NULL);
      adw_banner_set_revealed(wnd->banner, TRUE);
      wnd->banner_is_validation = true;
   } else if (wnd->banner_is_validation) {
      adw_banner_set_revealed(wnd->banner, FALSE);
      wnd->banner_is_validation = false;
   }
   wnd_update_header_controls(wnd);
}

static void wnd_update_header_controls(wxedid_wnd* wnd) {
   bool collapsed = adw_overlay_split_view_get_collapsed(wnd->split_view);
   gtk_widget_set_visible(wnd->open_button, wnd->loaded && ! collapsed);
   gtk_widget_set_visible(wnd->sidebar_button, wnd->loaded && collapsed);
   gtk_widget_set_visible(wnd->details_button,
                          wnd->details_available && ! collapsed);
}

static void wnd_show_error(wxedid_wnd* wnd, const char* message) {
   adw_banner_set_title(wnd->banner, message);
   adw_banner_set_button_label(wnd->banner, "Details");
   adw_banner_set_revealed(wnd->banner, TRUE);
   wnd->banner_is_validation = false;
}

static void wnd_clear_feedback(wxedid_wnd* wnd) {
   GtkTextBuffer* buffer = gtk_text_view_get_buffer(wnd->log);
   gtk_text_buffer_set_text(buffer, "", -1);
   g_simple_action_set_state(wnd->details_action, g_variant_new_boolean(FALSE));
   gtk_revealer_set_reveal_child(wnd->log_revealer, FALSE);
   g_simple_action_set_enabled(wnd->details_action, FALSE);
   wnd->details_available = false;
   wnd_update_header_controls(wnd);
   adw_banner_set_revealed(wnd->banner, FALSE);
   wnd->banner_is_validation = false;
}

static void wnd_on_details_action(GSimpleAction* action, GVariant* /*parameter*/,
                                  gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   GVariant* state = g_action_get_state(G_ACTION(action));
   bool visible = ! g_variant_get_boolean(state);
   g_variant_unref(state);
   g_simple_action_set_state(action, g_variant_new_boolean(visible));
   gtk_revealer_set_reveal_child(wnd->log_revealer, visible);
   gtk_widget_set_tooltip_text(wnd->details_button,
                               visible ? "Hide details" : "Show details");
}

static void wnd_on_banner_details(AdwBanner* /*banner*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   GVariant* state = g_action_get_state(G_ACTION(wnd->details_action));
   bool visible = g_variant_get_boolean(state);
   g_variant_unref(state);
   if (! visible) g_action_activate(G_ACTION(wnd->details_action), NULL);
}

static void wnd_load_file(wxedid_wnd* wnd, const char* path) {
   wnd_clear_feedback(wnd);

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
   if ((rd > sizeof(edi_t)) || (rd < sizeof(ediblk_t)) ||
       ((rd % sizeof(ediblk_t)) != 0)) {
      char msg[1400];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t open %s: EDID data must contain 1 to 4 complete "
               "128-byte blocks. Choose another EDID file.", path);
      wnd->doc->GLog.DoLog(msg);
      return;
   }

   edi_buf_t loaded = {};
   memcpy(loaded.buff, file_data, rd);

   u32_t n_extblk = loaded.edi.base.num_extblk;
   size_t expected = (1U + n_extblk) * sizeof(ediblk_t);
   if ((n_extblk > 3) || (rd != expected)) {
      char msg[192];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t open this EDID: it declares %u blocks, but the "
               "file contains %zu. Choose a file with a matching block count.",
               1U + n_extblk, rd / sizeof(ediblk_t));
      wnd->doc->GLog.DoLog(msg);
      return;
   }

   wnd->doc->EDID.Clear();
   edi_buf_t* pbuf = wnd->doc->EDID.getEDID();
   memcpy(pbuf->buff, loaded.buff, rd);

   rcode retU;
   u32_t parsed_extblk = 0;

   retU = wnd->doc->EDID.ParseEDID_Base(parsed_extblk);
   bool base_ok = RCD_IS_OK(retU);
   if (base_ok) {
      for (u32_t block=1; block<=parsed_extblk; block++) {
         u8_t tag = pbuf->blk[block][0];
         bool parsed = false;
         if ((block == EDI_EXT0_IDX) && (tag == 0x02)) {
            retU = wnd->doc->EDID.ParseEDID_CEA();
            parsed = true;
         } else if (tag == 0x70) {
            retU = wnd->doc->EDID.ParseEDID_DisplayID(block);
            parsed = true;
         }
         if (parsed && ! RCD_IS_OK(retU)) {
            wnd->doc->GLog.PrintRcode(retU);
            wnd->doc->EDID.BlkGroupsAr[block]->Clear();
         }
      }
      wnd->doc->EDID.ForceNumValidBlocks(1U + parsed_extblk);
   } else {
      char msg[1400];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t parse %s as base EDID data. Choose a valid EDID file.",
               path);
      wnd->doc->GLog.DoLog(msg);
      wnd->doc->GLog.PrintRcode(retU);
   }

   //rebuild tree model: block sections containing groups and sub-groups
   GListStore* root = g_list_store_new(WXEDID_TYPE_ITEM);
   if (wnd->doc->EDID.EDI_BaseGrpAr.GetCount() > 0) {
      wxedid_item* base = wxedid_item_new_block(
         "Block 0: Base EDID", &wnd->doc->EDID.EDI_BaseGrpAr, &wnd->doc->EDID);
      g_list_store_append(root, base);
      g_object_unref(base);
   }

   for (u32_t block=1; block<=parsed_extblk; block++) {
      GroupAr_cl* groups = wnd->doc->EDID.BlkGroupsAr[block];
      if (groups->GetCount() == 0) continue;

      const char* type = (pbuf->blk[block][0] == 0x02) ? "CTA-861" :
                         (pbuf->blk[block][0] == 0x70) ? "DisplayID" : "Extension";
      char label[64];
      snprintf(label, sizeof(label), "Block %u: %s", block, type);
      wxedid_item* section = wxedid_item_new_block(
         label, groups, &wnd->doc->EDID);
      g_list_store_append(root, section);
      g_object_unref(section);
   }

   if (base_ok) {
      for (u32_t block=1; block<=parsed_extblk; block++) {
         if (wnd->doc->EDID.BlkGroupsAr[block]->GetCount() != 0) continue;

         u8_t tag = pbuf->blk[block][0];
         wxedid_item* item =
            wxedid_item_new_raw_extension(block, tag, &wnd->doc->EDID);
         g_list_store_append(root, item);
         g_object_unref(item);

         char msg[144];
         snprintf(msg, sizeof(msg),
                  "[i] Extension block %u (tag 0x%02X) preserved read-only",
                  block, tag);
         wnd->doc->GLog.DoLog(msg);
      }
   }

   if (wnd->tree_model != NULL) g_object_unref(wnd->tree_model);
   wnd->tree_model = gtk_tree_list_model_new(
      G_LIST_MODEL(root),
      FALSE,                   //passthrough: rows are GtkTreeListRow
      FALSE,                   //expand only block sections by default
      tree_item_expand,
      NULL, NULL);
   g_object_unref(root);

   gtk_single_selection_set_model(wnd->tree_sel, G_LIST_MODEL(wnd->tree_model));

   guint position = 0;
   while (position < g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_model))) {
      GtkTreeListRow* row = GTK_TREE_LIST_ROW(
         g_list_model_get_item(G_LIST_MODEL(wnd->tree_model), position));
      GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
      wxedid_item* item = WXEDID_ITEM(obj);
      if (item->pgrp_ar != NULL) gtk_tree_list_row_set_expanded(row, TRUE);
      g_object_unref(obj);
      g_object_unref(row);
      position++;
   }

   guint n_items = g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_model));
   for (position=0; position<n_items; position++) {
      GtkTreeListRow* row = GTK_TREE_LIST_ROW(
         g_list_model_get_item(G_LIST_MODEL(wnd->tree_model), position));
      GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
      bool selectable = WXEDID_ITEM(obj)->selectable;
      g_object_unref(obj);
      g_object_unref(row);
      if (selectable) {
         gtk_single_selection_set_selected(wnd->tree_sel, position);
         break;
      }
   }

   wnd->loaded = base_ok;
   wnd->dirty = false;
   wnd->invalid_fields = 0;
   if (base_ok) {
      snprintf(wnd->doc->path, sizeof(wnd->doc->path), "%s", path);
      gtk_stack_set_visible_child_name(wnd->content_stack, "editor");
   } else {
      wnd->doc->path[0] = 0;
      gtk_stack_set_visible_child_name(wnd->content_stack, "empty");
   }
   wnd_update_document_ui(wnd);
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
            wnd_load_file(wnd, path);
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

static void wnd_present_open_dialog(wxedid_wnd* wnd) {
   GtkFileDialog* dialog = gtk_file_dialog_new();
   gtk_file_dialog_set_title(dialog, "Open EDID binary");
   gtk_file_dialog_set_accept_label(dialog, "Open");
   gtk_file_dialog_open(dialog, wnd->window, NULL, wnd_on_open_response,
                        g_object_ref(wnd->window));
   g_object_unref(dialog);
}

static void wnd_on_discard_open_response(GObject* source, GAsyncResult* result,
                                         gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   const char* response = adw_alert_dialog_choose_finish(
      ADW_ALERT_DIALOG(source), result);
   if (0 == strcmp(response, "discard")) wnd_present_open_dialog(wnd);
}

static void wnd_request_open(wxedid_wnd* wnd) {
   if (! wnd->dirty) {
      wnd_present_open_dialog(wnd);
      return;
   }

   AdwAlertDialog* dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new(
      "Discard unsaved changes?",
      "Opening another file will discard changes to the current EDID."));
   adw_alert_dialog_add_responses(dialog,
                                  "cancel", "Cancel",
                                  "discard", "Discard",
                                  NULL);
   adw_alert_dialog_set_close_response(dialog, "cancel");
   adw_alert_dialog_set_default_response(dialog, "cancel");
   adw_alert_dialog_set_response_appearance(dialog, "discard",
                                            ADW_RESPONSE_DESTRUCTIVE);
   adw_alert_dialog_choose(dialog, GTK_WIDGET(wnd->window), NULL,
                           wnd_on_discard_open_response, wnd);
}

static void wnd_on_open_action(GSimpleAction* /*action*/, GVariant* /*parameter*/,
                               gpointer user_data) {
   wnd_request_open((wxedid_wnd*) user_data);
}

//------------
// save: write the buffer to a given path, recompute checksums first
static bool wnd_save_to_file(wxedid_wnd* wnd, const char* path) {
   edi_buf_t* pbuf = wnd->doc->EDID.getEDID();
   u32_t declared_blocks = 1U + pbuf->edi.base.num_extblk;
   u32_t parsed_blocks = wnd->doc->EDID.getNumValidBlocks();
   if (declared_blocks != parsed_blocks) {
      char msg[160];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t save this EDID: it declares %u blocks, but only %u "
               "were parsed. Reopen valid EDID data, then save again.",
               declared_blocks, parsed_blocks);
      wnd->doc->GLog.DoLog(msg);
      return false;
   }

   rcode retU = wnd->doc->EDID.AssembleEDID();
   if (! RCD_IS_OK(retU)) {
      char detail[1024];
      char msg[1400];
      wxedid_RCD_GET_MSG(retU, detail, sizeof(detail));
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t save this EDID: %s. Fix invalid data, then save again.",
               detail);
      wnd->doc->GLog.DoLog(msg);
      return false;
   }

   //checksums: base + all valid extension blocks
   for (u32_t blk=0; blk < wnd->doc->EDID.getNumValidBlocks(); blk++) {
      if (wnd->doc->EDID.BlkGroupsAr[blk]->GetCount() != 0) {
         wnd->doc->EDID.genChksum(blk);
      }
   }

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
   if (path != wnd->doc->path) {
      snprintf(wnd->doc->path, sizeof(wnd->doc->path), "%s", path);
   }
   wnd->dirty = false;
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

static void wnd_request_save(wxedid_wnd* wnd) {
   //already have a path: save in place
   if (wnd->doc->path[0] != 0) {
      wnd_save_to_file(wnd, wnd->doc->path);
      return;
   }

   GtkFileDialog* dialog = gtk_file_dialog_new();
   gtk_file_dialog_set_title(dialog, "Save EDID binary");
   gtk_file_dialog_set_accept_label(dialog, "Save");
   gtk_file_dialog_set_initial_name(dialog, "edid.bin");
   gtk_file_dialog_save(dialog, wnd->window, NULL, wnd_on_save_response,
                        g_object_ref(wnd->window));
   g_object_unref(dialog);
}

static void wnd_on_save_action(GSimpleAction* /*action*/, GVariant* /*parameter*/,
                               gpointer user_data) {
   wnd_request_save((wxedid_wnd*) user_data);
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
      gtk_window_destroy(wnd->window);
   }
}

static gboolean wnd_on_close_request(GtkWindow* /*window*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   if (! wnd->dirty) return FALSE;
   if (wnd->close_confirmation_open) return TRUE;

   wnd->close_confirmation_open = true;
   AdwAlertDialog* dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new(
      "Discard unsaved changes?",
      "Closing this window will discard changes to the current EDID."));
   adw_alert_dialog_add_responses(dialog,
                                  "cancel", "Cancel",
                                  "discard", "Discard",
                                  NULL);
   adw_alert_dialog_set_close_response(dialog, "cancel");
   adw_alert_dialog_set_default_response(dialog, "cancel");
   adw_alert_dialog_set_response_appearance(dialog, "discard",
                                            ADW_RESPONSE_DESTRUCTIVE);
   adw_alert_dialog_choose(dialog, GTK_WIDGET(wnd->window), NULL,
                           wnd_on_discard_close_response, wnd);
   return TRUE;
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
         wnd_load_file(wnd, path);
         g_free(path);
      }
   }
}

//------------
void wxedid_app_activate(AdwApplication* app, gpointer /*user_data*/) {
   wxedid_wnd* wnd = new wxedid_wnd{};
   wnd->doc        = new wxedid_doc;
   wnd->doc->path[0] = 0;

   GtkWidget* window = adw_application_window_new(GTK_APPLICATION(app));
   wnd->window = GTK_WINDOW(window);
   gtk_window_set_default_size(GTK_WINDOW(window), 900, 640);
   gtk_window_set_title(GTK_WINDOW(window), "EDID Editor");
   g_signal_connect(window, "close-request", G_CALLBACK(wnd_on_close_request), wnd);

   g_object_set_data_full(G_OBJECT(window), "wxedid-wnd", wnd,
                           [](gpointer data) {
                              wxedid_wnd* w = (wxedid_wnd*) data;
                              g_clear_object(&w->tree_model);
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

   wnd->details_action = g_simple_action_new_stateful(
      "details", NULL, g_variant_new_boolean(FALSE));
   g_simple_action_set_enabled(wnd->details_action, FALSE);
   g_signal_connect(wnd->details_action, "activate",
                    G_CALLBACK(wnd_on_details_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->details_action));
   g_object_unref(wnd->details_action);

   const char* open_accels[] = {"<Control>o", NULL};
   const char* save_accels[] = {"<Control>s", NULL};
   gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.open", open_accels);
   gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.save", save_accels);

   //header bar
   GtkWidget* header = adw_header_bar_new();
   wnd->window_title = ADW_WINDOW_TITLE(adw_window_title_new("EDID Editor", "EDID editor"));
   adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), GTK_WIDGET(wnd->window_title));

   GtkWidget* btn_open = gtk_button_new_with_mnemonic("_Open");
   wnd->open_button = btn_open;
   gtk_actionable_set_action_name(GTK_ACTIONABLE(btn_open), "win.open");
   gtk_widget_set_tooltip_text(btn_open, "Open an EDID file (Ctrl+O)");
   adw_header_bar_pack_start(ADW_HEADER_BAR(header), btn_open);

   GtkWidget* btn_save = gtk_button_new_with_mnemonic("_Save");
   wnd->save_button = btn_save;
   gtk_actionable_set_action_name(GTK_ACTIONABLE(btn_save), "win.save");
   gtk_widget_set_tooltip_text(btn_save, "Save changes (Ctrl+S)");
   gtk_widget_add_css_class(btn_save, "suggested-action");
   adw_header_bar_pack_end(ADW_HEADER_BAR(header), btn_save);

   wnd->details_button = gtk_toggle_button_new();
   gtk_button_set_icon_name(GTK_BUTTON(wnd->details_button), "dialog-information-symbolic");
   gtk_actionable_set_action_name(GTK_ACTIONABLE(wnd->details_button), "win.details");
   gtk_widget_set_tooltip_text(wnd->details_button, "Show details");
   gtk_accessible_update_property(GTK_ACCESSIBLE(wnd->details_button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, "Show details",
                                  -1);
   gtk_widget_set_visible(wnd->details_button, FALSE);
   adw_header_bar_pack_end(ADW_HEADER_BAR(header), wnd->details_button);

   GMenu* primary_menu = g_menu_new();
   g_menu_append(primary_menu, "Open…", "win.open");
   g_menu_append(primary_menu, "Show details", "win.details");
   GtkWidget* btn_menu = gtk_menu_button_new();
   gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(btn_menu), "open-menu-symbolic");
   gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(btn_menu), G_MENU_MODEL(primary_menu));
   gtk_menu_button_set_primary(GTK_MENU_BUTTON(btn_menu), TRUE);
   gtk_widget_set_tooltip_text(btn_menu, "Main menu");
   gtk_widget_set_visible(btn_menu, FALSE);
   g_object_unref(primary_menu);
   adw_header_bar_pack_end(ADW_HEADER_BAR(header), btn_menu);

   GtkWidget* btn_sidebar = gtk_button_new_from_icon_name("sidebar-show-symbolic");
   wnd->sidebar_button = btn_sidebar;
   gtk_widget_set_tooltip_text(btn_sidebar, "Show groups");
   gtk_accessible_update_property(GTK_ACCESSIBLE(btn_sidebar),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, "Show groups",
                                  -1);
   gtk_widget_set_visible(btn_sidebar, FALSE);
   g_signal_connect(btn_sidebar, "clicked", G_CALLBACK(wnd_on_toggle_sidebar), wnd);
   adw_header_bar_pack_start(ADW_HEADER_BAR(header), btn_sidebar);

   //block tree: list view with lazy expander rows
   GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
   g_signal_connect(factory, "setup", G_CALLBACK(tree_name_setup), NULL);
   g_signal_connect(factory, "bind",  G_CALLBACK(tree_name_bind),  NULL);

   //selection: refresh the field list on change
   wnd->tree_sel = GTK_SINGLE_SELECTION(gtk_single_selection_new(NULL));
   gtk_single_selection_set_autoselect(wnd->tree_sel, FALSE);
   wnd->tree = GTK_LIST_VIEW(gtk_list_view_new(
      GTK_SELECTION_MODEL(wnd->tree_sel), factory));
   gtk_widget_add_css_class(GTK_WIDGET(wnd->tree), "navigation-sidebar");
   g_signal_connect(wnd->tree_sel, "selection-changed",
                    G_CALLBACK(wnd_on_tree_select), wnd);

   GtkWidget* tree_scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tree_scroll), GTK_WIDGET(wnd->tree));
   gtk_widget_set_hexpand(tree_scroll, TRUE);
   gtk_widget_set_vexpand(tree_scroll, TRUE);

   GtkWidget* sidebar = tree_scroll;

   GtkWidget* right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

   wnd->group_title = GTK_LABEL(gtk_label_new("Select a group"));
   gtk_label_set_xalign(wnd->group_title, 0.0);
   gtk_label_set_ellipsize(wnd->group_title, PANGO_ELLIPSIZE_END);
   gtk_widget_add_css_class(GTK_WIDGET(wnd->group_title), "title-2");
   gtk_widget_set_margin_start(GTK_WIDGET(wnd->group_title), 18);
   gtk_widget_set_margin_end(GTK_WIDGET(wnd->group_title), 18);
   gtk_widget_set_margin_top(GTK_WIDGET(wnd->group_title), 18);
   gtk_widget_set_margin_bottom(GTK_WIDGET(wnd->group_title), 12);
   gtk_box_append(GTK_BOX(right), GTK_WIDGET(wnd->group_title));

   wnd->fields = GTK_FLOW_BOX(gtk_flow_box_new());
   gtk_flow_box_set_selection_mode(wnd->fields, GTK_SELECTION_NONE);
   gtk_flow_box_set_homogeneous(wnd->fields, TRUE);
   gtk_flow_box_set_min_children_per_line(wnd->fields, 1);
   gtk_flow_box_set_max_children_per_line(wnd->fields, 1);
   gtk_flow_box_set_column_spacing(wnd->fields, 12);
   gtk_flow_box_set_row_spacing(wnd->fields, 12);
   gtk_widget_set_valign(GTK_WIDGET(wnd->fields), GTK_ALIGN_START);
   g_object_set_data(G_OBJECT(wnd->fields), "wxedid-wnd", wnd);

   GtkWidget* fields_clamp = adw_clamp_new();
   adw_clamp_set_maximum_size(ADW_CLAMP(fields_clamp), 1100);
   adw_clamp_set_tightening_threshold(ADW_CLAMP(fields_clamp), 760);
   gtk_widget_set_margin_start(fields_clamp, 18);
   gtk_widget_set_margin_end(fields_clamp, 18);
   gtk_widget_set_margin_bottom(fields_clamp, 18);
   adw_clamp_set_child(ADW_CLAMP(fields_clamp), GTK_WIDGET(wnd->fields));

   GtkWidget* fields_scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(fields_scroll), fields_clamp);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(fields_scroll),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
   gtk_widget_set_vexpand(fields_scroll, TRUE);
   gtk_box_append(GTK_BOX(right), fields_scroll);

   wnd->log = GTK_TEXT_VIEW(gtk_text_view_new());
   gtk_text_view_set_editable(wnd->log, FALSE);
   gtk_text_view_set_monospace(wnd->log, TRUE);
   gtk_text_view_set_wrap_mode(wnd->log, GTK_WRAP_WORD_CHAR);
   gtk_text_view_set_left_margin(wnd->log, 12);
   gtk_text_view_set_right_margin(wnd->log, 12);
   gtk_text_view_set_top_margin(wnd->log, 8);
   gtk_text_view_set_bottom_margin(wnd->log, 8);
   GtkWidget* log_scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(log_scroll), GTK_WIDGET(wnd->log));
   gtk_widget_set_size_request(log_scroll, -1, 160);
   wnd->log_revealer = GTK_REVEALER(gtk_revealer_new());
   gtk_revealer_set_transition_type(wnd->log_revealer,
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
   gtk_revealer_set_child(wnd->log_revealer, log_scroll);

   GtkWidget* split_view = adw_overlay_split_view_new();
   wnd->split_view = ADW_OVERLAY_SPLIT_VIEW(split_view);
   adw_overlay_split_view_set_sidebar(wnd->split_view, sidebar);
   adw_overlay_split_view_set_content(wnd->split_view, right);
   adw_overlay_split_view_set_min_sidebar_width(wnd->split_view, 280.0);
   adw_overlay_split_view_set_max_sidebar_width(wnd->split_view, 340.0);
   adw_overlay_split_view_set_sidebar_width_fraction(wnd->split_view, 0.28);
   g_signal_connect(wnd->split_view, "notify::collapsed",
                    G_CALLBACK(wnd_on_split_collapsed), wnd);

   AdwBreakpointCondition* condition = adw_breakpoint_condition_new_length(
      ADW_BREAKPOINT_CONDITION_MAX_WIDTH, 700.0, ADW_LENGTH_UNIT_SP);
   AdwBreakpoint* breakpoint = adw_breakpoint_new(condition);
   adw_breakpoint_add_setters(
      breakpoint,
      G_OBJECT(split_view), "collapsed", TRUE,
      G_OBJECT(split_view), "show-sidebar", FALSE,
      G_OBJECT(btn_menu), "visible", TRUE,
      NULL);
   adw_application_window_add_breakpoint(ADW_APPLICATION_WINDOW(window), breakpoint);

   AdwBreakpointCondition* medium_min = adw_breakpoint_condition_new_length(
      ADW_BREAKPOINT_CONDITION_MIN_WIDTH, 560.0, ADW_LENGTH_UNIT_SP);
   AdwBreakpointCondition* medium_max = adw_breakpoint_condition_new_length(
      ADW_BREAKPOINT_CONDITION_MAX_WIDTH, 1180.0, ADW_LENGTH_UNIT_SP);
   AdwBreakpoint* medium_grid = adw_breakpoint_new(
      adw_breakpoint_condition_new_and(medium_min, medium_max));
   adw_breakpoint_add_setters(
      medium_grid,
      G_OBJECT(wnd->fields), "min-children-per-line", 2U,
      G_OBJECT(wnd->fields), "max-children-per-line", 2U,
      NULL);
   adw_application_window_add_breakpoint(ADW_APPLICATION_WINDOW(window), medium_grid);

   AdwBreakpointCondition* wide_grid_condition =
      adw_breakpoint_condition_new_length(
         ADW_BREAKPOINT_CONDITION_MIN_WIDTH, 1181.0, ADW_LENGTH_UNIT_SP);
   AdwBreakpoint* wide_grid = adw_breakpoint_new(wide_grid_condition);
   adw_breakpoint_add_setters(
      wide_grid,
      G_OBJECT(wnd->fields), "min-children-per-line", 3U,
      G_OBJECT(wnd->fields), "max-children-per-line", 3U,
      NULL);
   adw_application_window_add_breakpoint(ADW_APPLICATION_WINDOW(window), wide_grid);

   GtkWidget* empty_page = adw_status_page_new();
   adw_status_page_set_icon_name(ADW_STATUS_PAGE(empty_page), "video-display-symbolic");
   adw_status_page_set_title(ADW_STATUS_PAGE(empty_page), "Open an EDID file");
   adw_status_page_set_description(ADW_STATUS_PAGE(empty_page),
                                   "Inspect and edit display identification data.");
   GtkWidget* empty_open = gtk_button_new_with_mnemonic("_Open an EDID File");
   gtk_actionable_set_action_name(GTK_ACTIONABLE(empty_open), "win.open");
   gtk_widget_add_css_class(empty_open, "suggested-action");
   gtk_widget_set_halign(empty_open, GTK_ALIGN_CENTER);
   adw_status_page_set_child(ADW_STATUS_PAGE(empty_page), empty_open);

   wnd->content_stack = GTK_STACK(gtk_stack_new());
   gtk_stack_add_named(wnd->content_stack, empty_page, "empty");
   gtk_stack_add_named(wnd->content_stack, split_view, "editor");
   gtk_stack_set_visible_child_name(wnd->content_stack, "empty");
   gtk_widget_set_vexpand(GTK_WIDGET(wnd->content_stack), TRUE);

   wnd->banner = ADW_BANNER(adw_banner_new(""));
   g_signal_connect(wnd->banner, "button-clicked",
                    G_CALLBACK(wnd_on_banner_details), wnd);

   GtkWidget* body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_box_append(GTK_BOX(body), GTK_WIDGET(wnd->banner));
   gtk_box_append(GTK_BOX(body), GTK_WIDGET(wnd->content_stack));
   gtk_box_append(GTK_BOX(body), GTK_WIDGET(wnd->log_revealer));

   wnd->toast_overlay = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
   adw_toast_overlay_set_child(wnd->toast_overlay, body);

   wnd->doc->GLog.SetSink(log_sink, wnd);
   wnd->doc->EDID.SetGuiLogPtr(&wnd->doc->GLog);

   GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_box_append(GTK_BOX(content), header);
   gtk_box_append(GTK_BOX(content), GTK_WIDGET(wnd->toast_overlay));
   gtk_widget_set_vexpand(GTK_WIDGET(wnd->toast_overlay), TRUE);

   adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), content);
   wnd_update_document_ui(wnd);
   gtk_window_present(GTK_WINDOW(window));
}
