/***************************************************************
 * Name:      window.cpp
 * Purpose:   main application window (GTK4/libadwaita)
 * License:   GPLv3+
 **************************************************************/

#include "window.h"

#include "wxcompat.h"
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
// field list: refreshed when a group is selected in the tree
static void fields_refresh(GtkListBox* list, edi_grp_cl* pgrp, EDID_cl& EDID) {
   //drop old rows
   GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(list));
   while (child != NULL) {
      GtkWidget* next = gtk_widget_get_next_sibling(child);
      gtk_list_box_remove(list, child);
      child = next;
   }

   if (pgrp == NULL) return;

   wxc_String sval;
   u32_t      ival = 0;
   u32_t      cnt  = pgrp->FieldsAr.GetCount();

   for (u32_t idx=0; idx<cnt; idx++) {
      edi_dynfld_t* pfld = pgrp->FieldsAr.Item(idx);

      sval.Empty();
      ival = 0;
      rcode retU = ( EDID.*pfld->field.handlerfn )(OP_READ, sval, ival, pfld);

      GtkWidget* row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
      gtk_widget_set_margin_start(row, 8);
      gtk_widget_set_margin_end(row, 8);
      gtk_widget_set_margin_top(row, 4);
      gtk_widget_set_margin_bottom(row, 4);

      GtkWidget* lbl_name = gtk_label_new(pfld->field.name);
      gtk_label_set_xalign(GTK_LABEL(lbl_name), 0.0);
      gtk_label_set_ellipsize(GTK_LABEL(lbl_name), PANGO_ELLIPSIZE_END);
      gtk_widget_add_css_class(lbl_name, "heading");

      GtkWidget* lbl_val = gtk_label_new(sval.c_str());
      gtk_label_set_xalign(GTK_LABEL(lbl_val), 0.0);
      gtk_label_set_ellipsize(GTK_LABEL(lbl_val), PANGO_ELLIPSIZE_END);
      gtk_label_set_selectable(GTK_LABEL(lbl_val), TRUE);
      gtk_label_set_wrap(GTK_LABEL(lbl_val), TRUE);
      gtk_widget_add_css_class(lbl_val, "monospace");
      if (! RCD_IS_OK(retU)) {
         gtk_widget_add_css_class(lbl_val, "error");
      }

      gtk_box_append(GTK_BOX(row), lbl_name);
      gtk_box_append(GTK_BOX(row), lbl_val);

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
      g_list_store_append(store, wxedid_item_new(psubg, it->pEDID));
   }
   return G_LIST_MODEL(store);
}

//build flat root list of groups for one block array
static void store_fill_block(GListStore* root, GroupAr_cl* grp_ar, EDID_cl* pEDID) {
   u32_t cnt = grp_ar->GetCount();
   for (u32_t idx=0; idx<cnt; idx++) {
      edi_grp_cl* pgrp = grp_ar->Item(idx);
      if (pgrp == NULL) continue;
      g_list_store_append(root, wxedid_item_new(pgrp, pEDID));
   }
}

//------------
// factory: tree cell shows the group name
static void tree_name_setup(GtkSignalListItemFactory* /*factory*/,
                            GtkListItem* item, gpointer /*user_data*/) {
   GtkWidget* lbl = gtk_label_new(NULL);
   gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
   gtk_list_item_set_child(item, lbl);
}

static void tree_name_bind(GtkSignalListItemFactory* /*factory*/,
                           GtkListItem* item, gpointer /*user_data*/) {
   GObject* obj    = G_OBJECT(gtk_list_item_get_item(item));
   GtkWidget* cell = gtk_list_item_get_child(item);

   wxedid_item* it = WXEDID_ITEM(obj);
   wxc_String   gname;
   if ((it != NULL) && (it->pgrp != NULL) && (it->pEDID != NULL)) {
      it->pgrp->getGrpName(*it->pEDID, gname);
   }
   gtk_label_set_text(GTK_LABEL(cell), gname.c_str());
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

static void wnd_on_tree_select(GtkSelectionModel* selmodel, GParamSpec* /*pspec*/, gpointer user_data) {
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
   if (in == NULL) return;

   edi_buf_t* pbuf = wnd->doc->EDID.getEDID();
   memset(pbuf, 0, sizeof(edi_buf_t));
   fread(pbuf, 1, sizeof(edi_buf_t), in);
   fclose(in);

   rcode retU;
   u32_t n_extblk = 0;

   wnd->doc->EDID.Clear();
   retU = wnd->doc->EDID.ParseEDID_Base(n_extblk);
   if (RCD_IS_OK(retU) && (n_extblk > 0)) {
      wnd->doc->EDID.ParseEDID_CEA();
   }

   //rebuild tree model: base + ext0 blocks, sub-groups expand lazily
   GListStore* root = g_list_store_new(WXEDID_TYPE_ITEM);
   store_fill_block(root, &wnd->doc->EDID.EDI_BaseGrpAr, &wnd->doc->EDID);
   store_fill_block(root, &wnd->doc->EDID.EDI_Ext0GrpAr, &wnd->doc->EDID);

   if (wnd->tree_model != NULL) g_object_unref(wnd->tree_model);
   wnd->tree_model = gtk_tree_list_model_new(
      G_LIST_MODEL(root),
      FALSE,                   //passthrough: rows are GtkTreeListRow
      FALSE,                   //not built lazily (small data)
      tree_item_expand,
      NULL, NULL);

   gtk_column_view_set_model(wnd->tree, GTK_SELECTION_MODEL(wnd->tree_sel));
   gtk_single_selection_set_model(wnd->tree_sel, G_LIST_MODEL(wnd->tree_model));
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
void wxedid_app_activate(AdwApplication* app, gpointer /*user_data*/) {
   wxedid_wnd* wnd = new wxedid_wnd;
   wnd->doc        = new wxedid_doc;
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
