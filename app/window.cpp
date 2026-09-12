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

//------------
// per-document state
struct wxedid_doc {
   EDID_cl    EDID;
   guilog_cl  GLog;
   char       path[1024]; //current file, empty if none
};

//tree item: GObject holding an edi_grp_cl* for GtkTreeListModel
struct wxedid_item;
typedef struct wxedid_item wxedid_item;

struct wxedid_itemClass {
   GObjectClass parent_class;
};

#define WXEDID_TYPE_ITEM (wxedid_item_get_type())
#define WXEDID_ITEM(obj) ((wxedid_item*) (obj))

struct wxedid_item {
   GObject     parent;
   edi_grp_cl* pgrp;
   EDID_cl*    pEDID;
   char        label[96];
};

static void wxedid_item_init(wxedid_item*) {}
static void wxedid_item_class_init(wxedid_itemClass*) {}

G_DEFINE_FINAL_TYPE(wxedid_item, wxedid_item, G_TYPE_OBJECT)

static wxedid_item* wxedid_item_new(edi_grp_cl* pgrp, EDID_cl* pEDID) {
   wxedid_item* item = (wxedid_item*) g_object_new(WXEDID_TYPE_ITEM, NULL);
   item->pgrp  = pgrp;
   item->pEDID = pEDID;
   return item;
}

static wxedid_item* wxedid_item_new_raw_extension(u32_t block, u8_t tag,
                                                   EDID_cl* pEDID) {
   wxedid_item* item = wxedid_item_new(NULL, pEDID);
   const char* type = (tag == 0x70) ? "DisplayID" : "Unsupported";
   snprintf(item->label, sizeof(item->label),
            "Extension %u: %s (0x%02X), preserved read-only", block, type, tag);
   return item;
}

static void log_sink(const char* msg, void* user_data) {
   GtkTextView* tv = GTK_TEXT_VIEW(user_data);
   if (tv == NULL) return;

   GtkTextBuffer* buf = gtk_text_view_get_buffer(tv);
   GtkTextIter end;
   gtk_text_buffer_get_end_iter(buf, &end);
   gtk_text_buffer_insert(buf, &end, msg, -1);
   gtk_text_buffer_insert(buf, &end, "\n", -1);
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
   int           kind;
   GtkWidget*    entry;  //GtkEditable | GtkDropDown
   u32_t         sel_idx; //dropdown item value
};

//re-read all rows of the field list into the widgets' current display
static void rows_reload(GtkListBox* list, edi_grp_cl* pgrp, EDID_cl* pEDID);

//------------
// helpers shared by rows
static bool field_writable(const edi_field_t& f) {
   return (0 == (f.flags & F_RD));
}

static bool field_has_selector(const edi_field_t& f) {
   return ((f.flags & F_VS) != 0) && (f.vmap_idx != VS_NO_SELECTOR);
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
   } else {
      gtk_widget_add_css_class(GTK_WIDGET(entry), "error");
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
   } else {
      gtk_widget_add_css_class(GTK_WIDGET(dd), "error");
   }
}

//------------
// field list: rebuilt when a group is selected in the tree
static void fields_refresh(GtkListBox* list, edi_grp_cl* pgrp, EDID_cl& EDID) {
   rows_reload(list, pgrp, &EDID);
}

static void rows_reload(GtkListBox* list, edi_grp_cl* pgrp, EDID_cl* pEDID) {
   //drop old rows
   GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(list));
   while (child != NULL) {
      GtkWidget* next = gtk_widget_get_next_sibling(child);
      gtk_list_box_remove(list, child);
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

      GtkWidget* row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
      gtk_widget_set_margin_start(row, 8);
      gtk_widget_set_margin_end(row, 8);
      gtk_widget_set_margin_top(row, 4);
      gtk_widget_set_margin_bottom(row, 4);

      GtkWidget* lbl_name = gtk_label_new(pfld->field.name);
      gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0);
      gtk_label_set_ellipsize(GTK_LABEL(lbl_name), PANGO_ELLIPSIZE_END);
      gtk_widget_add_css_class(lbl_name, "heading");

      gtk_box_append(GTK_BOX(row), lbl_name);

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
            gtk_widget_set_valign(GTK_WIDGET(dd), GTK_ALIGN_CENTER);

            wxedid_row* r = new wxedid_row{pfld, pgrp, pEDID, ROW_COMBO, GTK_WIDGET(dd), 0};
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
            // entry width: default, sval driven

            wxedid_row* r = new wxedid_row{pfld, pgrp, pEDID, ROW_ENTRY, GTK_WIDGET(entry), 0};

            g_signal_connect(entry, "changed", G_CALLBACK(row_on_entry_changed), r);
            g_object_set_data_full(G_OBJECT(entry), "row", r,
                                   [](gpointer data){ delete (wxedid_row*) data; });

            GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
            gtk_box_append(GTK_BOX(hbox), GTK_WIDGET(entry));
            widget = hbox;
         } else {
            //read-only label
            GtkWidget* lbl_val = gtk_label_new(sval.c_str());
            gtk_label_set_xalign(GTK_LABEL(lbl_val), 0.0);
            gtk_label_set_ellipsize(GTK_LABEL(lbl_val), PANGO_ELLIPSIZE_END);
            gtk_label_set_selectable(GTK_LABEL(lbl_val), TRUE);
            gtk_label_set_wrap(GTK_LABEL(lbl_val), TRUE);
            gtk_widget_add_css_class(lbl_val, "monospace");
            if (! RCD_IS_OK(retU)) {
               gtk_widget_add_css_class(lbl_val, "error");
            }
            widget = lbl_val;
         }
      }

      gtk_box_append(GTK_BOX(row), widget);
      gtk_list_box_append(list, row);
   }
}

//------------
// GtkTreeListModel expand callback: sub-groups of a group
static GListModel* tree_item_expand(gpointer item, gpointer /*user_data*/) {
   wxedid_item* it = WXEDID_ITEM(item);
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

//build flat root list of groups for one block array
static void store_fill_block(GListStore* root, GroupAr_cl* grp_ar, EDID_cl* pEDID) {
   u32_t cnt = grp_ar->GetCount();
   for (u32_t idx=0; idx<cnt; idx++) {
      edi_grp_cl* pgrp = grp_ar->Item(idx);
      if (pgrp == NULL) continue;
      wxedid_item* item = wxedid_item_new(pgrp, pEDID);
      g_list_store_append(root, item);
      g_object_unref(item);
   }
}

//------------
// factory: tree cell shows the group name
static void tree_name_setup(GtkSignalListItemFactory* /*factory*/,
                            GtkListItem* item, gpointer /*user_data*/) {
   GtkWidget* expander = gtk_tree_expander_new();
   GtkWidget* lbl = gtk_label_new(NULL);
   gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
   gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), lbl);
   gtk_list_item_set_child(item, expander);
}

static void tree_name_bind(GtkSignalListItemFactory* /*factory*/,
                           GtkListItem* item, gpointer /*user_data*/) {
   GtkTreeListRow* row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(item));
   GtkTreeExpander* expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(item));
   gtk_tree_expander_set_list_row(expander, row);

   GObject* obj    = G_OBJECT(gtk_tree_list_row_get_item(row));
   GtkWidget* cell = gtk_tree_expander_get_child(expander);

   wxedid_item* it = WXEDID_ITEM(obj);
   wxc_String   gname;
   if ((it != NULL) && (it->pgrp != NULL) && (it->pEDID != NULL)) {
      it->pgrp->getGrpName(*it->pEDID, gname);
   } else if (it != NULL) {
      gname = it->label;
   }
   gtk_label_set_text(GTK_LABEL(cell), gname.c_str());
   g_object_unref(obj);
}

//------------
struct wxedid_wnd {
   wxedid_doc*       doc;
   GtkColumnView*    tree;
   GtkSingleSelection* tree_sel;
   GtkTreeListModel* tree_model;
   GtkListBox*       fields;
   GtkTextView*      log;
};

static void wnd_on_tree_select(GtkSelectionModel* selmodel, guint /*position*/,
                               guint /*n_items*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;

   GtkTreeListRow* row = (GtkTreeListRow*) gtk_single_selection_get_selected_item(GTK_SINGLE_SELECTION(selmodel));
   if (row == NULL) return;

   GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
   if (obj == NULL) return;

   wxedid_item* it = WXEDID_ITEM(obj);
   fields_refresh(wnd->fields, it->pgrp, *it->pEDID);

   g_object_unref(obj);   //gtk_tree_list_row_get_item() transfers a full ref
}

static void wnd_load_file(wxedid_wnd* wnd, const char* path) {
   FILE* in = fopen(path, "rb");
   if (in == NULL) {
      wnd->doc->GLog.DoLog("[E!] Cannot open EDID file");
      return;
   }

   u8_t file_data[sizeof(edi_t) + 1] = {};
   size_t rd = fread(file_data, 1, sizeof(file_data), in);
   bool read_failed = (ferror(in) != 0);
   fclose(in);

   if (read_failed || (rd > sizeof(edi_t)) || (rd < sizeof(ediblk_t)) ||
       ((rd % sizeof(ediblk_t)) != 0)) {
      wnd->doc->GLog.DoLog("[E!] EDID file must contain 1 to 4 complete 128-byte blocks");
      return;
   }

   edi_buf_t loaded = {};
   memcpy(loaded.buff, file_data, rd);

   u32_t n_extblk = loaded.edi.base.num_extblk;
   size_t expected = (1U + n_extblk) * sizeof(ediblk_t);
   if ((n_extblk > 3) || (rd != expected)) {
      char msg[192];
      snprintf(msg, sizeof(msg),
               "[E!] EDID declares %u blocks, but the file contains %zu",
               1U + n_extblk, rd / sizeof(ediblk_t));
      wnd->doc->GLog.DoLog(msg);
      return;
   }

   wnd->doc->EDID.Clear();
   edi_buf_t* pbuf = wnd->doc->EDID.getEDID();
   memcpy(pbuf->buff, loaded.buff, rd);

   snprintf(wnd->doc->path, sizeof(wnd->doc->path), "%s", path);

   rcode retU;
   u32_t parsed_extblk = 0;

   retU = wnd->doc->EDID.ParseEDID_Base(parsed_extblk);
   bool parse_ok = RCD_IS_OK(retU);
   if (parse_ok && (parsed_extblk > 0) && (pbuf->blk[EDI_EXT0_IDX][0] == 0x02)) {
      retU = wnd->doc->EDID.ParseEDID_CEA();
      parse_ok = RCD_IS_OK(retU);
   }
   if (parse_ok) {
      wnd->doc->EDID.ForceNumValidBlocks(1U + parsed_extblk);
   }

   //rebuild tree model: parsed groups plus read-only preserved extensions
   GListStore* root = g_list_store_new(WXEDID_TYPE_ITEM);
   store_fill_block(root, &wnd->doc->EDID.EDI_BaseGrpAr, &wnd->doc->EDID);
   store_fill_block(root, &wnd->doc->EDID.EDI_Ext0GrpAr, &wnd->doc->EDID);

   if (parse_ok) {
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
      FALSE,                   //not built lazily (small data)
      tree_item_expand,
      NULL, NULL);
   g_object_unref(root);

   gtk_single_selection_set_model(wnd->tree_sel, G_LIST_MODEL(wnd->tree_model));
   if (g_list_model_get_n_items(G_LIST_MODEL(wnd->tree_model)) > 0) {
      gtk_single_selection_set_selected(wnd->tree_sel, 0);
   }
}

static void wnd_on_open_response(GtkNativeDialog* native_dlg, int response, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;

   if (response == GTK_RESPONSE_ACCEPT) {
      GFile* file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(native_dlg));
      char* path = g_file_get_path(file);
      g_object_unref(file);

      if (path != NULL) {
         wnd_load_file(wnd, path);
         g_free(path);
      }
   }
   g_object_unref(native_dlg);
}

static void wnd_on_open(GtkButton* /*btn*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;

   GtkFileChooserNative* native_dlg = gtk_file_chooser_native_new(
      "Open EDID binary",
      GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))),
      GTK_FILE_CHOOSER_ACTION_OPEN,
      "_Open", "_Cancel");

   g_signal_connect(native_dlg, "response", G_CALLBACK(wnd_on_open_response), wnd);
   gtk_native_dialog_show(GTK_NATIVE_DIALOG(native_dlg));
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
               "[E!] Cannot save: EDID declares %u blocks, but only %u were parsed",
               declared_blocks, parsed_blocks);
      wnd->doc->GLog.DoLog(msg);
      return false;
   }

   rcode retU = wnd->doc->EDID.AssembleEDID();
   if (! RCD_IS_OK(retU)) {
      char msg[1024];
      wxedid_RCD_GET_MSG(retU, msg, sizeof(msg));
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
      wnd->doc->GLog.DoLog("[E!] Cannot open file for writing");
      return false;
   }

   size_t expected = wnd->doc->EDID.getNumValidBlocks() * sizeof(ediblk_t);
   size_t wr = fwrite(pbuf->buff, 1, expected, out);
   int close_rc = fclose(out);
   if ((wr != expected) || (close_rc != 0)) {
      wnd->doc->GLog.DoLog("[E!] Failed to write complete EDID file");
      return false;
   }

   char msg[1152];
   snprintf(msg, sizeof(msg), "[i] Saved %zu bytes to %s", wr, path);
   wnd->doc->GLog.DoLog(msg);
   snprintf(wnd->doc->path, sizeof(wnd->doc->path), "%s", path);
   return true;
}

static void wnd_on_save_response(GtkNativeDialog* native_dlg, int response, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;

   if (response == GTK_RESPONSE_ACCEPT) {
      GFile* file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(native_dlg));
      char* path = g_file_get_path(file);
      g_object_unref(file);

      if (path != NULL) {
         wnd_save_to_file(wnd, path);
         g_free(path);
      }
   }
   g_object_unref(native_dlg);
}

static void wnd_on_save(GtkButton* /*btn*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;

   //already have a path: save in place
   if (wnd->doc->path[0] != 0) {
      wnd_save_to_file(wnd, wnd->doc->path);
      return;
   }

   GtkFileChooserNative* native_dlg = gtk_file_chooser_native_new(
      "Save EDID binary",
      GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))),
      GTK_FILE_CHOOSER_ACTION_SAVE,
      "_Save", "_Cancel");
   gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(native_dlg), "edid.bin");

   g_signal_connect(native_dlg, "response", G_CALLBACK(wnd_on_save_response), wnd);
   gtk_native_dialog_show(GTK_NATIVE_DIALOG(native_dlg));
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
   wxedid_wnd* wnd = new wxedid_wnd;
   wnd->doc        = new wxedid_doc;
   wnd->doc->path[0] = 0;
   wnd->tree_model = NULL;
   wnd->tree_sel   = NULL;

   GtkWidget* window = adw_application_window_new(GTK_APPLICATION(app));
   gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
   gtk_window_set_title(GTK_WINDOW(window), "EDID Editor");

   g_object_set_data_full(G_OBJECT(window), "wxedid-wnd", wnd,
                           [](gpointer data) {
                              wxedid_wnd* w = (wxedid_wnd*) data;
                              delete w->doc;
                              delete w;
                           });

   //header bar
   GtkWidget* header = adw_header_bar_new();
   GtkWidget* btn_open = gtk_button_new_with_mnemonic("_Open");
   g_signal_connect(btn_open, "clicked", G_CALLBACK(wnd_on_open), wnd);
   adw_header_bar_pack_start(ADW_HEADER_BAR(header), btn_open);

   GtkWidget* btn_save = gtk_button_new_with_mnemonic("_Save");
   g_signal_connect(btn_save, "clicked", G_CALLBACK(wnd_on_save), wnd);
   adw_header_bar_pack_start(ADW_HEADER_BAR(header), btn_save);

   //layout: left = block tree, right = field list + log
   GtkWidget* pane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
   gtk_paned_set_position(GTK_PANED(pane), 380);

   //block tree: column view with lazy expander rows
   GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
   g_signal_connect(factory, "setup", G_CALLBACK(tree_name_setup), NULL);
   g_signal_connect(factory, "bind",  G_CALLBACK(tree_name_bind),  NULL);

   GtkColumnViewColumn* col = gtk_column_view_column_new("Group", factory);
   gtk_column_view_column_set_expand(col, TRUE);

   wnd->tree = GTK_COLUMN_VIEW(gtk_column_view_new(NULL));
   gtk_column_view_append_column(wnd->tree, col);
   gtk_column_view_set_show_row_separators(wnd->tree, TRUE);

   //selection: refresh the field list on change
   wnd->tree_sel = GTK_SINGLE_SELECTION(gtk_single_selection_new(NULL));
   gtk_single_selection_set_autoselect(wnd->tree_sel, FALSE);
   gtk_column_view_set_model(wnd->tree, GTK_SELECTION_MODEL(wnd->tree_sel));
   g_signal_connect(wnd->tree_sel, "selection-changed",
                    G_CALLBACK(wnd_on_tree_select), wnd);

   GtkWidget* tree_scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tree_scroll), GTK_WIDGET(wnd->tree));
   gtk_widget_set_hexpand(tree_scroll, TRUE);
   gtk_widget_set_vexpand(tree_scroll, TRUE);

   GtkWidget* right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
   gtk_widget_set_margin_top(right, 6);
   gtk_widget_set_margin_bottom(right, 6);

   wnd->fields = GTK_LIST_BOX(gtk_list_box_new());
   gtk_list_box_set_selection_mode(wnd->fields, GTK_SELECTION_NONE);
   GtkWidget* fields_scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(fields_scroll), GTK_WIDGET(wnd->fields));
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(fields_scroll),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
   gtk_widget_set_vexpand(fields_scroll, TRUE);

   wnd->log = GTK_TEXT_VIEW(gtk_text_view_new());
   gtk_text_view_set_editable(wnd->log, FALSE);
   gtk_text_view_set_monospace(wnd->log, TRUE);
   GtkWidget* log_scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(log_scroll), GTK_WIDGET(wnd->log));
   gtk_widget_set_vexpand(log_scroll, TRUE);
   gtk_widget_set_size_request(log_scroll, -1, 140);

   gtk_box_append(GTK_BOX(right), fields_scroll);
   gtk_box_append(GTK_BOX(right), log_scroll);

   gtk_paned_set_start_child(GTK_PANED(pane), tree_scroll);
   gtk_paned_set_end_child(GTK_PANED(pane), right);

   wnd->doc->GLog.SetSink(log_sink, wnd->log);
   wnd->doc->EDID.SetGuiLogPtr(&wnd->doc->GLog);

   GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_box_append(GTK_BOX(content), header);
   gtk_box_append(GTK_BOX(content), pane);
   gtk_widget_set_vexpand(pane, TRUE);

   adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), content);
   gtk_window_present(GTK_WINDOW(window));
}
