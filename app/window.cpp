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
#include "EDID_text.h"
#include "EDID_display.h"
#include "wxedid-config.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <glib/gstdio.h>
#include <pango/pangocairo.h>

struct wxedid_timing;

enum history_kind {
   HISTORY_FIELD,  //field value change
   HISTORY_INSERT, //group inserted at index of array
   HISTORY_REMOVE, //group removed from index of array
   HISTORY_MOVE,   //group moved from index one step up or down
   HISTORY_REPLACE,//group rebuilt after a field changed its type or layout
};

//Structural entries own their group while it is out of the document: an
//insert while undone, a removal while applied. A replacement owns the old
//group while applied and the replacement while undone.
struct wxedid_history_entry {
   history_kind  kind;
   edi_grp_cl*   group;
   edi_dynfld_t* field;
   bool          integer;
   std::string   before_text;
   std::string   after_text;
   u32_t         before_value;
   u32_t         after_value;
   GroupAr_cl*   array;
   edi_grp_cl*   parent; //sub-group parent, restores into an emptied array
   edi_grp_cl*   replacement;
   u32_t         index;
   bool          up;
   bool          joined; //undone and redone together with the previous entry
};

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
   GtkCustomFilter*    tree_filter;
   GtkFilterListModel* tree_filtered;
   GtkSearchEntry*     tree_search;
   GtkStack*           sidebar_stack;
   GtkFlowBox*         fields;
   wxedid_timing*      timing;
   AdwViewStack*       editor_stack;
   GtkWidget*          editor_switcher;
   GtkTextView*        raw_view;
   AdwViewStackPage*   timing_stack_page;
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
   GSimpleAction*      save_as_action;
   GSimpleAction*      undo_action;
   GSimpleAction*      redo_action;
   GSimpleAction*      details_action;
   GSimpleAction*      duplicate_action;
   GSimpleAction*      delete_action;
   GSimpleAction*      move_up_action;
   GSimpleAction*      move_down_action;
   GSimpleAction*      add_cta_action;
   GSimpleAction*      add_displayid_action;
   GSimpleAction*      export_hex_action;
   GSimpleAction*      save_report_action;
   GSimpleAction*      ignore_errors_action;
   GSimpleAction*      ignore_read_only_action;
   GtkPopoverMenu*     group_menu;
   edi_grp_cl*         pending_delete;
   edi_grp_cl*         last_selected;  //restored when a search shows it again
   edi_grp_cl*         refresh_group;  //field write that may need a rebuild
   edi_dynfld_t*       refresh_field;
   bool                refresh_type_changed;
   guint               refresh_source;
   std::string         tree_query;
   std::string         source_path;   //last file opened or attempted
   bool                source_hex;    //source_path holds hexadecimal text
   std::vector<wxedid_history_entry> history;
   size_t              history_position;
   long                saved_history_position;
   bool                loaded;
   bool                dirty;
   bool                source_writable;
   bool                document_hex;  //document was imported from hex text
   bool                load_had_errors;
   bool                applying_history;
   bool                banner_is_validation;
   bool                banner_offers_retry;
   bool                close_confirmation_open;
   bool                details_available;
   u32_t               invalid_fields;
};

enum timing_field {
   TIMING_PIXCLK,
   TIMING_HACTIVE,
   TIMING_HBLANK,
   TIMING_VACTIVE,
   TIMING_VBLANK,
   TIMING_HOFFSET,
   TIMING_HWIDTH,
   TIMING_VOFFSET,
   TIMING_VWIDTH,
   TIMING_HBORDER,
   TIMING_VBORDER,
   TIMING_FIELD_COUNT,
};

struct wxedid_timing {
   wxedid_wnd*   wnd;
   edi_grp_cl*   pgrp;
   edi_dynfld_t* fields[TIMING_FIELD_COUNT];
   GtkSpinButton* spins[TIMING_FIELD_COUNT];
   GtkWidget*    row_widgets[TIMING_FIELD_COUNT][4];
   GtkLabel*     derived[TIMING_FIELD_COUNT];
   GtkLabel*     clock_unit;
   GtkLabel*     refresh;
   GtkLabel*     htotal;
   GtkLabel*     hfreq;
   GtkLabel*     vtotal;
   GtkLabel*     modeline;
   GtkWidget*    drawing;
   GtkWidget*    sections;
   GtkWidget*    page;
   double        pixel_hz_factor;
   bool          updating;
   bool          editing;
   timing_field  editing_field;
   size_t        history_index;
   wxc_String    before_text;
   u32_t         before_value;
};

static void wnd_update_document_ui(wxedid_wnd* wnd);
static void wnd_update_history_state(wxedid_wnd* wnd);
static void wnd_show_error(wxedid_wnd* wnd, const char* message);
static void wnd_update_header_controls(wxedid_wnd* wnd);
static void wnd_refresh_selected_tree_label(wxedid_wnd* wnd);
static void wnd_refresh_raw_view(wxedid_wnd* wnd);
static void wnd_update_group_actions(wxedid_wnd* wnd);
static void wnd_rebuild_tree(wxedid_wnd* wnd, edi_grp_cl* select_group);
static void wnd_popup_group_menu(wxedid_wnd* wnd, double x, double y);
static void wnd_on_tree_item_context(GtkGestureClick*, int, double, double,
                                     gpointer user_data);
static bool timing_load_group(wxedid_timing* timing, edi_grp_cl* pgrp,
                              EDID_cl* pEDID);
static void wnd_record_history(wxedid_wnd* wnd, edi_grp_cl* group,
                               edi_dynfld_t* field, bool integer,
                               const wxc_String& before_text, u32_t before_value,
                               const wxc_String& after_text, u32_t after_value);
static std::string field_display_name(const char* name);
static void wnd_request_refresh(wxedid_wnd* wnd, edi_grp_cl* group,
                                edi_dynfld_t* field, bool type_changed, bool now);
static void wnd_schedule_refresh(wxedid_wnd* wnd);
static void wnd_flush_refresh(wxedid_wnd* wnd);

static const char DRM_ROOT[] = "/sys/class/drm";

//file name to show for a document: display data is named by its connector
static char* document_basename(const char* path) {
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

static void wnd_refresh_group_title(wxedid_wnd* wnd, edi_grp_cl* pgrp) {
   if (pgrp == NULL) return;
   wxc_String group_name;
   pgrp->getGrpName(wnd->doc->EDID, group_name);
   gtk_label_set_text(wnd->group_title, group_name.c_str());
}

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
   GtkLabel*    bound_label;
   bool         selectable;
   int          raw_block;
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
   const char* type = (tag == 0x70) ? "DisplayID" : "Unsupported";
   snprintf(item->label, sizeof(item->label),
            "Extension %u: %s (0x%02X), preserved read-only", block, type, tag);
   item->selectable = true;
   item->raw_block = static_cast<int>(block);
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
   GtkLabel*     error_label;
   u32_t         sel_idx; //dropdown item value
   bool          valid;
   bool          editing;
   size_t        history_index;
   wxc_String    before_text;
   u32_t         before_value;
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

   std::string field = field_display_name(row->pfld->field.name);
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

static void wnd_record_edit_history(wxedid_wnd* wnd, edi_grp_cl* group,
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

static std::string field_help_summary(const char* description) {
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

      std::string help = field_help_summary(pfld->field.desc);
      if (! help.empty()) gtk_widget_set_tooltip_text(card, help.c_str());

      GtkWidget* card_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
      gtk_widget_set_margin_start(card_content, 12);
      gtk_widget_set_margin_end(card_content, 12);
      gtk_widget_set_margin_top(card_content, 12);
      gtk_widget_set_margin_bottom(card_content, 12);
      gtk_box_append(GTK_BOX(card), card_content);

      std::string title = field_display_name(pfld->field.name);
      wxc_String unit;
      pEDID->getValUnitName(unit, pfld->field.flags);
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
      gtk_accessible_update_property(GTK_ACCESSIBLE(widget),
                                     GTK_ACCESSIBLE_PROPERTY_LABEL,
                                     title.c_str(), -1);
      if (! help.empty()) {
         gtk_widget_set_tooltip_text(widget, help.c_str());
         gtk_accessible_update_property(GTK_ACCESSIBLE(widget),
                                        GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                        help.c_str(), -1);
      }
      gtk_flow_box_append(list, card);
      gtk_widget_add_css_class(gtk_widget_get_parent(card), "card");
   }

   wnd_update_document_ui(wnd);
}

//------------
// visual editor for EDID Detailed Timing Descriptors
static u32_t timing_value(const wxedid_timing* timing, timing_field field) {
   return static_cast<u32_t>(gtk_spin_button_get_value_as_int(timing->spins[field]));
}

static void timing_set_text(GtkLabel* label, const char* format, double value) {
   char text[48];
   snprintf(text, sizeof(text), format, value);
   gtk_label_set_text(label, text);
}

static void timing_update_outputs(wxedid_timing* timing) {
   const double pixclk = timing_value(timing, TIMING_PIXCLK) *
                         timing->pixel_hz_factor;
   const u32_t hactive = timing_value(timing, TIMING_HACTIVE);
   const u32_t hblank = timing_value(timing, TIMING_HBLANK);
   const u32_t vactive = timing_value(timing, TIMING_VACTIVE);
   const u32_t vblank = timing_value(timing, TIMING_VBLANK);
   const u32_t htotal = hactive + hblank;
   const u32_t vtotal = vactive + vblank;

   if ((pixclk <= 0.0) || (htotal == 0) || (vtotal == 0)) return;

   const double pixel_us = 1000000.0 / pixclk;
   const double line_ms = htotal * 1000.0 / pixclk;
   const double refresh = pixclk / (htotal * static_cast<double>(vtotal));

   timing_set_text(timing->derived[TIMING_HACTIVE], "%.3f us", hactive * pixel_us);
   timing_set_text(timing->derived[TIMING_HBLANK], "%.4f us", hblank * pixel_us);
   timing_set_text(timing->derived[TIMING_HOFFSET], "%.4f us",
                   timing_value(timing, TIMING_HOFFSET) * pixel_us);
   timing_set_text(timing->derived[TIMING_HWIDTH], "%.4f us",
                   timing_value(timing, TIMING_HWIDTH) * pixel_us);
   timing_set_text(timing->derived[TIMING_VACTIVE], "%.3f ms", vactive * line_ms);
   timing_set_text(timing->derived[TIMING_VBLANK], "%.4f ms", vblank * line_ms);
   timing_set_text(timing->derived[TIMING_VOFFSET], "%.4f ms",
                   timing_value(timing, TIMING_VOFFSET) * line_ms);
   timing_set_text(timing->derived[TIMING_VWIDTH], "%.4f ms",
                   timing_value(timing, TIMING_VWIDTH) * line_ms);

   char text[64];
   snprintf(text, sizeof(text), "%.2f Hz", refresh);
   gtk_label_set_text(timing->refresh, text);
   snprintf(text, sizeof(text), "%u px  ·  %.3f us", htotal,
            htotal * pixel_us);
   gtk_label_set_text(timing->htotal, text);
   snprintf(text, sizeof(text), "%.2f kHz", pixclk / htotal / 1000.0);
   gtk_label_set_text(timing->hfreq, text);
   snprintf(text, sizeof(text), "%u lines  ·  %.3f ms", vtotal,
            vtotal * line_ms);
   gtk_label_set_text(timing->vtotal, text);

   const u32_t hsync_start = hactive + timing_value(timing, TIMING_HOFFSET);
   const u32_t hsync_end = hsync_start + timing_value(timing, TIMING_HWIDTH);
   const u32_t vsync_start = vactive + timing_value(timing, TIMING_VOFFSET);
   const u32_t vsync_end = vsync_start + timing_value(timing, TIMING_VWIDTH);
   char modeline[256];
   snprintf(modeline, sizeof(modeline),
            "\"%ux%u@%.2f\" %.2f  %u %u %u %u  %u %u %u %u",
            hactive, vactive, refresh, pixclk / 1000000.0,
            hactive, hsync_start, hsync_end, htotal,
            vactive, vsync_start, vsync_end, vtotal);
   gtk_label_set_text(timing->modeline, modeline);
   gtk_widget_queue_draw(timing->drawing);
}

static void timing_draw(GtkDrawingArea* area, cairo_t* cr, int width, int height,
                        gpointer user_data) {
   wxedid_timing* timing = static_cast<wxedid_timing*>(user_data);
   const double htotal = timing_value(timing, TIMING_HACTIVE) +
                         timing_value(timing, TIMING_HBLANK);
   const double vtotal = timing_value(timing, TIMING_VACTIVE) +
                         timing_value(timing, TIMING_VBLANK);
   if ((htotal <= 0.0) || (vtotal <= 0.0)) return;

   const double pad = 18.0;
   const double canvas_w = std::max(1.0, width - (2.0 * pad));
   const double canvas_h = std::max(1.0, height - (2.0 * pad));
   const double hback = std::max(0.0,
      static_cast<double>(timing_value(timing, TIMING_HBLANK)) -
      timing_value(timing, TIMING_HOFFSET));
   const double vback = std::max(0.0,
      static_cast<double>(timing_value(timing, TIMING_VBLANK)) -
      timing_value(timing, TIMING_VOFFSET));
   const double active_x = pad + (hback / htotal) * canvas_w;
   const double active_y = pad + (vback / vtotal) * canvas_h;
   const double active_w = timing_value(timing, TIMING_HACTIVE) / htotal * canvas_w;
   const double active_h = timing_value(timing, TIMING_VACTIVE) / vtotal * canvas_h;
   const double hsync_w = std::max(1.0,
      timing_value(timing, TIMING_HWIDTH) / htotal * canvas_w);
   const double vsync_h = std::max(1.0,
      timing_value(timing, TIMING_VWIDTH) / vtotal * canvas_h);

   GdkRGBA color;
   gtk_widget_get_color(GTK_WIDGET(area), &color);
   cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.08);
   cairo_rectangle(cr, pad, pad, canvas_w, canvas_h);
   cairo_fill(cr);
   cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.32);
   cairo_rectangle(cr, pad, pad, hsync_w, canvas_h);
   cairo_fill(cr);
   cairo_rectangle(cr, pad, pad, canvas_w, vsync_h);
   cairo_fill(cr);
   cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.20);
   cairo_rectangle(cr, active_x, active_y, active_w, active_h);
   cairo_fill_preserve(cr);
   cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.72);
   cairo_set_line_width(cr, 1.0);
   cairo_stroke(cr);

   char active_text[48];
   snprintf(active_text, sizeof(active_text), "%u × %u",
            timing_value(timing, TIMING_HACTIVE),
            timing_value(timing, TIMING_VACTIVE));
   PangoLayout* layout = gtk_widget_create_pango_layout(GTK_WIDGET(area), active_text);
   PangoFontDescription* font = pango_font_description_new();
   pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
   pango_layout_set_font_description(layout, font);
   int text_w = 0;
   int text_h = 0;
   pango_layout_get_pixel_size(layout, &text_w, &text_h);
   cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
   cairo_move_to(cr, active_x + ((active_w - text_w) / 2.0),
                 active_y + ((active_h - text_h) / 2.0));
   pango_cairo_show_layout(cr, layout);
   pango_font_description_free(font);
   g_object_unref(layout);
}

static void timing_on_changed(GtkSpinButton* spin, gpointer user_data) {
   wxedid_timing* timing = static_cast<wxedid_timing*>(user_data);
   if (timing->updating || (timing->pgrp == NULL)) return;

   timing_field changed = static_cast<timing_field>(
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(spin), "timing-field")));
   timing->updating = true;

   u32_t value = timing_value(timing, changed);
   if (changed == TIMING_HBLANK) {
      value = std::max(value, timing_value(timing, TIMING_HOFFSET) +
                              timing_value(timing, TIMING_HWIDTH));
   } else if (changed == TIMING_HOFFSET) {
      const u32_t blank = timing_value(timing, TIMING_HBLANK);
      const u32_t width = timing_value(timing, TIMING_HWIDTH);
      value = std::min(value, (blank > width) ? blank - width : 0U);
   } else if (changed == TIMING_HWIDTH) {
      const u32_t blank = timing_value(timing, TIMING_HBLANK);
      const u32_t offset = timing_value(timing, TIMING_HOFFSET);
      value = std::min(value, (blank > offset) ? blank - offset : 0U);
   } else if (changed == TIMING_VBLANK) {
      value = std::max(value, timing_value(timing, TIMING_VOFFSET) +
                              timing_value(timing, TIMING_VWIDTH));
   } else if (changed == TIMING_VOFFSET) {
      const u32_t blank = timing_value(timing, TIMING_VBLANK);
      const u32_t width = timing_value(timing, TIMING_VWIDTH);
      value = std::min(value, (blank > width) ? blank - width : 0U);
   } else if (changed == TIMING_VWIDTH) {
      const u32_t blank = timing_value(timing, TIMING_VBLANK);
      const u32_t offset = timing_value(timing, TIMING_VOFFSET);
      value = std::min(value, (blank > offset) ? blank - offset : 0U);
   }
   gtk_spin_button_set_value(spin, value);

   edi_dynfld_t* field = timing->fields[changed];
   wxc_String before_text;
   u32_t before_value = 0;
   (timing->wnd->doc->EDID.*field->field.handlerfn)(
      OP_READ, before_text, before_value, field);
   wxc_String sval;
   rcode ret = (timing->wnd->doc->EDID.*field->field.handlerfn)(
      OP_WRINT, sval, value, field);
   if (RCD_IS_OK(ret)) {
      wxc_String after_text;
      u32_t after_value = 0;
      (timing->wnd->doc->EDID.*field->field.handlerfn)(
         OP_READ, after_text, after_value, field);
      if (timing->editing && (timing->editing_field == changed)) {
         wnd_record_edit_history(timing->wnd, timing->pgrp, field, true,
                                 timing->before_text, timing->before_value,
                                 before_text, before_value,
                                 after_text, after_value,
                                 &timing->history_index);
      } else {
         wnd_record_history(timing->wnd, timing->pgrp, field, true,
                            before_text, before_value, after_text, after_value);
      }
      gtk_widget_remove_css_class(GTK_WIDGET(spin), "error");
      timing_update_outputs(timing);
      wnd_refresh_group_title(timing->wnd, timing->pgrp);
      wnd_refresh_selected_tree_label(timing->wnd);
      rows_reload(timing->wnd->fields, timing->pgrp, &timing->wnd->doc->EDID,
                  timing->wnd);
      wnd_refresh_raw_view(timing->wnd);
   } else {
      gtk_widget_add_css_class(GTK_WIDGET(spin), "error");
      timing->wnd->doc->GLog.PrintRcode(ret);
   }
   timing->updating = false;
   wnd_update_document_ui(timing->wnd);
}

static void timing_on_focus_enter(GtkEventControllerFocus* controller,
                                  gpointer user_data) {
   wxedid_timing* timing = static_cast<wxedid_timing*>(user_data);
   GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
   timing->editing_field = static_cast<timing_field>(
      GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "timing-field")));
   timing->editing = true;
   timing->history_index = static_cast<size_t>(-1);
   timing->before_text.Empty();
   timing->before_value = 0;
   edi_dynfld_t* field = timing->fields[timing->editing_field];
   if ((field != NULL) && (timing->pgrp != NULL)) {
      (timing->wnd->doc->EDID.*field->field.handlerfn)(
         OP_READ, timing->before_text, timing->before_value, field);
   }
}

static void timing_on_focus_leave(GtkEventControllerFocus*, gpointer user_data) {
   wxedid_timing* timing = static_cast<wxedid_timing*>(user_data);
   timing->editing = false;
   timing->history_index = static_cast<size_t>(-1);
}

static void timing_add_focus_controller(wxedid_timing* timing, GtkWidget* spin) {
   GtkEventController* focus = gtk_event_controller_focus_new();
   g_signal_connect(focus, "enter", G_CALLBACK(timing_on_focus_enter), timing);
   g_signal_connect(focus, "leave", G_CALLBACK(timing_on_focus_leave), timing);
   gtk_widget_add_controller(spin, focus);
}

static GtkWidget* timing_add_edit_row(wxedid_timing* timing, GtkGrid* grid,
                                      int row, timing_field field,
                                      const char* title, const char* unit) {
   GtkWidget* label = gtk_label_new(title);
   gtk_label_set_xalign(GTK_LABEL(label), 0.0);
   gtk_widget_set_hexpand(label, TRUE);
   gtk_grid_attach(grid, label, 0, row, 1, 1);

   GtkAdjustment* adjustment = gtk_adjustment_new(0, 0, 65535, 1, 10, 0);
   GtkWidget* spin = gtk_spin_button_new(adjustment, 1, 0);
   gtk_widget_set_valign(spin, GTK_ALIGN_CENTER);
   gtk_widget_set_size_request(spin, 88, -1);
   gtk_accessible_update_property(GTK_ACCESSIBLE(spin),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, title, -1);
   timing->spins[field] = GTK_SPIN_BUTTON(spin);
   timing->row_widgets[field][0] = label;
   timing->row_widgets[field][1] = spin;
   g_object_set_data(G_OBJECT(spin), "timing-field", GINT_TO_POINTER(field));
   g_signal_connect(spin, "value-changed", G_CALLBACK(timing_on_changed), timing);
   timing_add_focus_controller(timing, spin);
   gtk_grid_attach(grid, spin, 1, row, 1, 1);

   GtkWidget* unit_label = gtk_label_new(unit);
   timing->row_widgets[field][2] = unit_label;
   gtk_widget_add_css_class(unit_label, "dim-label");
   gtk_grid_attach(grid, unit_label, 2, row, 1, 1);

   GtkWidget* derived = gtk_label_new(NULL);
   gtk_label_set_xalign(GTK_LABEL(derived), 1.0);
   gtk_widget_add_css_class(derived, "dim-label");
   gtk_widget_add_css_class(derived, "numeric");
   timing->derived[field] = GTK_LABEL(derived);
   timing->row_widgets[field][3] = derived;
   gtk_grid_attach(grid, derived, 3, row, 1, 1);
   return spin;
}

static void timing_add_value_row(GtkGrid* grid, int row, const char* title,
                                 GtkLabel** value) {
   GtkWidget* label = gtk_label_new(title);
   gtk_label_set_xalign(GTK_LABEL(label), 0.0);
   gtk_widget_set_hexpand(label, TRUE);
   gtk_grid_attach(grid, label, 0, row, 1, 1);
   *value = GTK_LABEL(gtk_label_new(NULL));
   gtk_label_set_xalign(*value, 1.0);
   gtk_widget_add_css_class(GTK_WIDGET(*value), "dim-label");
   gtk_widget_add_css_class(GTK_WIDGET(*value), "numeric");
   gtk_grid_attach(grid, GTK_WIDGET(*value), 1, row, 3, 1);
}

static GtkWidget* timing_section(const char* title, GtkGrid** grid_out) {
   GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
   gtk_widget_add_css_class(card, "card");
   gtk_widget_set_margin_start(card, 0);
   GtkWidget* heading = gtk_label_new(title);
   gtk_label_set_xalign(GTK_LABEL(heading), 0.0);
   gtk_widget_add_css_class(heading, "heading");
   gtk_widget_set_margin_start(heading, 12);
   gtk_widget_set_margin_end(heading, 12);
   gtk_widget_set_margin_top(heading, 12);
   gtk_box_append(GTK_BOX(card), heading);

   GtkWidget* grid = gtk_grid_new();
   gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
   gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
   gtk_widget_set_margin_start(grid, 9);
   gtk_widget_set_margin_end(grid, 9);
   gtk_widget_set_margin_bottom(grid, 12);
   gtk_box_append(GTK_BOX(card), grid);
   *grid_out = GTK_GRID(grid);
   return card;
}

static GtkWidget* timing_create_page(wxedid_timing* timing) {
   GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

   GtkWidget* summary = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
   gtk_widget_add_css_class(summary, "card");
   gtk_widget_set_margin_top(summary, 2);
   gtk_widget_set_margin_start(summary, 0);
   gtk_widget_set_margin_end(summary, 0);
   gtk_widget_set_margin_bottom(summary, 0);
   GtkWidget* clock_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
   gtk_widget_set_margin_start(clock_box, 12);
   gtk_widget_set_margin_top(clock_box, 12);
   gtk_widget_set_margin_bottom(clock_box, 12);
   GtkWidget* clock_title = gtk_label_new("Pixel clock");
   gtk_label_set_xalign(GTK_LABEL(clock_title), 0.0);
   gtk_widget_add_css_class(clock_title, "caption");
   gtk_widget_add_css_class(clock_title, "dim-label");
   gtk_box_append(GTK_BOX(clock_box), clock_title);
   GtkWidget* clock_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   GtkAdjustment* clock_adj = gtk_adjustment_new(1, 1, 65535, 1, 100, 0);
   GtkWidget* clock_spin = gtk_spin_button_new(clock_adj, 1, 0);
   timing->spins[TIMING_PIXCLK] = GTK_SPIN_BUTTON(clock_spin);
   g_object_set_data(G_OBJECT(clock_spin), "timing-field",
                     GINT_TO_POINTER(TIMING_PIXCLK));
   g_signal_connect(clock_spin, "value-changed", G_CALLBACK(timing_on_changed), timing);
   timing_add_focus_controller(timing, clock_spin);
   gtk_accessible_update_property(GTK_ACCESSIBLE(clock_spin),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, "Pixel clock", -1);
   gtk_box_append(GTK_BOX(clock_row), clock_spin);
   timing->clock_unit = GTK_LABEL(gtk_label_new("×10 kHz"));
   gtk_widget_add_css_class(GTK_WIDGET(timing->clock_unit), "dim-label");
   gtk_box_append(GTK_BOX(clock_row), GTK_WIDGET(timing->clock_unit));
   gtk_box_append(GTK_BOX(clock_box), clock_row);
   gtk_box_append(GTK_BOX(summary), clock_box);

   GtkWidget* refresh_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
   gtk_widget_set_margin_top(refresh_box, 12);
   gtk_widget_set_margin_bottom(refresh_box, 12);
   GtkWidget* refresh_title = gtk_label_new("Vertical refresh");
   gtk_label_set_xalign(GTK_LABEL(refresh_title), 0.0);
   gtk_widget_add_css_class(refresh_title, "caption");
   gtk_widget_add_css_class(refresh_title, "dim-label");
   gtk_box_append(GTK_BOX(refresh_box), refresh_title);
   timing->refresh = GTK_LABEL(gtk_label_new(NULL));
   gtk_label_set_xalign(timing->refresh, 0.0);
   gtk_widget_add_css_class(GTK_WIDGET(timing->refresh), "title-3");
   gtk_widget_add_css_class(GTK_WIDGET(timing->refresh), "numeric");
   gtk_box_append(GTK_BOX(refresh_box), GTK_WIDGET(timing->refresh));
   gtk_box_append(GTK_BOX(summary), refresh_box);
   gtk_box_append(GTK_BOX(content), summary);

   timing->drawing = gtk_drawing_area_new();
   gtk_widget_set_size_request(timing->drawing, -1, 300);
   gtk_widget_set_hexpand(timing->drawing, TRUE);
   gtk_widget_add_css_class(timing->drawing, "card");
   gtk_accessible_update_property(GTK_ACCESSIBLE(timing->drawing),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  "Active image and blanking diagram", -1);
   gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(timing->drawing),
                                  timing_draw, timing, NULL);
   gtk_box_append(GTK_BOX(content), timing->drawing);

   GtkWidget* sections = gtk_flow_box_new();
   timing->sections = sections;
   gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(sections), GTK_SELECTION_NONE);
   gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(sections), TRUE);
   gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(sections), 1);
   gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(sections), 1);
   gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(sections), 12);
   gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(sections), 12);
   GtkGrid* horizontal = NULL;
   GtkWidget* horizontal_card = timing_section("Horizontal timing", &horizontal);
   timing_add_edit_row(timing, horizontal, 0, TIMING_HACTIVE, "Active", "px");
   timing_add_edit_row(timing, horizontal, 1, TIMING_HBORDER, "Border", "px");
   timing_add_edit_row(timing, horizontal, 2, TIMING_HBLANK, "Blanking", "px");
   timing_add_edit_row(timing, horizontal, 3, TIMING_HOFFSET, "Sync offset", "px");
   timing_add_edit_row(timing, horizontal, 4, TIMING_HWIDTH, "Sync width", "px");
   timing_add_value_row(horizontal, 5, "Total", &timing->htotal);
   timing_add_value_row(horizontal, 6, "Frequency", &timing->hfreq);
   gtk_flow_box_append(GTK_FLOW_BOX(sections), horizontal_card);

   GtkGrid* vertical = NULL;
   GtkWidget* vertical_card = timing_section("Vertical timing", &vertical);
   timing_add_edit_row(timing, vertical, 0, TIMING_VACTIVE, "Active", "lines");
   timing_add_edit_row(timing, vertical, 1, TIMING_VBORDER, "Border", "lines");
   timing_add_edit_row(timing, vertical, 2, TIMING_VBLANK, "Blanking", "lines");
   timing_add_edit_row(timing, vertical, 3, TIMING_VOFFSET, "Sync offset", "lines");
   timing_add_edit_row(timing, vertical, 4, TIMING_VWIDTH, "Sync width", "lines");
   timing_add_value_row(vertical, 5, "Total", &timing->vtotal);
   gtk_flow_box_append(GTK_FLOW_BOX(sections), vertical_card);
   gtk_box_append(GTK_BOX(content), sections);

   GtkWidget* modeline_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
   gtk_widget_add_css_class(modeline_card, "card");
   GtkWidget* modeline_title = gtk_label_new("X11 ModeLine");
   gtk_label_set_xalign(GTK_LABEL(modeline_title), 0.0);
   gtk_widget_add_css_class(modeline_title, "caption");
   gtk_widget_add_css_class(modeline_title, "dim-label");
   gtk_widget_set_margin_start(modeline_title, 12);
   gtk_widget_set_margin_end(modeline_title, 12);
   gtk_widget_set_margin_top(modeline_title, 12);
   gtk_box_append(GTK_BOX(modeline_card), modeline_title);
   timing->modeline = GTK_LABEL(gtk_label_new(NULL));
   gtk_label_set_xalign(timing->modeline, 0.0);
   gtk_label_set_selectable(timing->modeline, TRUE);
   gtk_label_set_wrap(timing->modeline, TRUE);
   gtk_widget_add_css_class(GTK_WIDGET(timing->modeline), "monospace");
   gtk_widget_set_margin_start(GTK_WIDGET(timing->modeline), 12);
   gtk_widget_set_margin_end(GTK_WIDGET(timing->modeline), 12);
   gtk_widget_set_margin_bottom(GTK_WIDGET(timing->modeline), 12);
   gtk_box_append(GTK_BOX(modeline_card), GTK_WIDGET(timing->modeline));
   gtk_box_append(GTK_BOX(content), modeline_card);

   GtkWidget* clamp = adw_clamp_new();
   adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 1100);
   adw_clamp_set_tightening_threshold(ADW_CLAMP(clamp), 760);
   gtk_widget_set_margin_start(clamp, 18);
   gtk_widget_set_margin_end(clamp, 18);
   gtk_widget_set_margin_bottom(clamp, 18);
   adw_clamp_set_child(ADW_CLAMP(clamp), content);

   GtkWidget* scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), clamp);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
   return scroll;
}

static bool timing_load_group(wxedid_timing* timing, edi_grp_cl* pgrp,
                              EDID_cl* pEDID) {
   if (pgrp == NULL) {
      timing->pgrp = NULL;
      return false;
   }

   static const int dtd_fields[TIMING_FIELD_COUNT] = {
      DTD_IDX_PIXCLK, DTD_IDX_HAPIX, DTD_IDX_HBPIX, DTD_IDX_VALIN,
      DTD_IDX_VBLIN, DTD_IDX_HSOFFS, DTD_IDX_HSWIDTH, DTD_IDX_VSOFFS,
      DTD_IDX_VSWIDTH, DTD_IDX_HBORD, DTD_IDX_VBORD,
   };
   static const int displayid_type1_fields[TIMING_FIELD_COUNT] = {
      0, 5, 6, 10, 11, 7, 8, 12, 13, -1, -1,
   };
   static const int t7_fields[TIMING_FIELD_COUNT] = {
      T7F_IDX_PIXCLK, T7F_IDX_HAPIX, T7F_IDX_HBPIX, T7F_IDX_VALIN,
      T7F_IDX_VBLIN, T7F_IDX_HSOFFS, T7F_IDX_HSWIDTH, T7F_IDX_VSOFFS,
      T7F_IDX_VSWIDTH, -1, -1,
   };

   const int* field_indices = NULL;
   const char* code = pgrp->CodeName.c_str();
   if (0 == strcmp(code, "DTD")) {
      field_indices = dtd_fields;
      timing->pixel_hz_factor = 10000.0;
      gtk_label_set_text(timing->clock_unit, "×10 kHz");
   } else if ((0 == strcmp(code, "DID-T1")) || (0 == strcmp(code, "DID-T7"))) {
      //Type VII shares the Type I layout
      field_indices = displayid_type1_fields;
      timing->pixel_hz_factor = 1000.0;
      gtk_label_set_text(timing->clock_unit, "kHz");
   } else if (0 == strcmp(code, "T7VTB")) {
      field_indices = t7_fields;
      timing->pixel_hz_factor = 1000.0;
      gtk_label_set_text(timing->clock_unit, "kHz");
   } else {
      timing->pgrp = NULL;
      return false;
   }

   timing->updating = true;
   timing->pgrp = pgrp;
   for (int idx = 0; idx < TIMING_FIELD_COUNT; idx++) {
      bool available = field_indices[idx] >= 0;
      if ((idx == TIMING_HBORDER) || (idx == TIMING_VBORDER)) {
         for (GtkWidget* widget : timing->row_widgets[idx]) {
            gtk_widget_set_visible(widget, available);
         }
      }
      if (! available) {
         timing->fields[idx] = NULL;
         gtk_spin_button_set_value(timing->spins[idx], 0);
         continue;
      }
      if (static_cast<u32_t>(field_indices[idx]) >= pgrp->FieldsAr.GetCount()) {
         timing->pgrp = NULL;
         timing->updating = false;
         return false;
      }
      edi_dynfld_t* field = pgrp->FieldsAr.Item(field_indices[idx]);
      timing->fields[idx] = field;
      std::string help = field_help_summary(field->field.desc);
      gtk_widget_set_tooltip_text(GTK_WIDGET(timing->spins[idx]),
                                  help.empty() ? NULL : help.c_str());
      gtk_accessible_update_property(GTK_ACCESSIBLE(timing->spins[idx]),
                                     GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                     help.empty() ? NULL : help.c_str(), -1);
      wxc_String text;
      u32_t value = 0;
      rcode ret = (pEDID->*field->field.handlerfn)(OP_READ, text, value, field);
      if (! RCD_IS_OK(ret)) {
         timing->pgrp = NULL;
         timing->updating = false;
         return false;
      }
      double minimum = field->field.minv;
      if (idx == TIMING_PIXCLK) minimum = 1;
      double maximum = field->field.maxv;
      double step = 1;
      if ((idx == TIMING_PIXCLK) && (0 == strcmp(code, "DID-T1"))) {
         maximum = 167772160;
         step = 10;
      } else if ((idx == TIMING_PIXCLK) && (0 == strcmp(code, "DID-T7"))) {
         maximum = 16777216;
      }
      GtkAdjustment* adjustment = gtk_spin_button_get_adjustment(timing->spins[idx]);
      gtk_adjustment_set_lower(adjustment, minimum);
      gtk_adjustment_set_upper(adjustment, maximum);
      gtk_adjustment_set_step_increment(adjustment, step);
      gtk_spin_button_set_value(timing->spins[idx], value);
   }
   timing_update_outputs(timing);
   timing->updating = false;
   return true;
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

static gboolean tree_filter_match(gpointer object, gpointer user_data) {
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

static void tree_filter_items_changed(GListModel*, guint, guint, guint,
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

static void tree_search_changed(GtkSearchEntry* entry, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   char* folded = g_utf8_casefold(gtk_editable_get_text(GTK_EDITABLE(entry)), -1);
   wnd->tree_query = folded;
   g_free(folded);
   gtk_filter_changed(GTK_FILTER(wnd->tree_filter), GTK_FILTER_CHANGE_DIFFERENT);
   wnd_update_search_state(wnd);
   if ((wnd->last_selected != NULL) &&
       (gtk_single_selection_get_selected_item(wnd->tree_sel) == NULL)) {
      guint position = wnd_filtered_position(wnd, wnd->last_selected);
      if (position != GTK_INVALID_LIST_POSITION)
         gtk_single_selection_set_selected(wnd->tree_sel, position);
   }
}

static void tree_search_clear(GtkButton*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   gtk_editable_set_text(GTK_EDITABLE(wnd->tree_search), "");
   gtk_widget_grab_focus(GTK_WIDGET(wnd->tree_search));
}

//------------
// factory: tree cell shows the group name
static void tree_name_setup(GtkSignalListItemFactory* /*factory*/,
                            GtkListItem* item, gpointer user_data) {
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
   g_object_set_data(G_OBJECT(item), "wxedid-wnd", user_data);
   GtkGesture* context = gtk_gesture_click_new();
   gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context),
                                 GDK_BUTTON_SECONDARY);
   g_signal_connect(context, "pressed",
                    G_CALLBACK(wnd_on_tree_item_context), item);
   gtk_widget_add_controller(expander, GTK_EVENT_CONTROLLER(context));
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
   it->bound_label = GTK_LABEL(label);
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

static void tree_name_unbind(GtkSignalListItemFactory* /*factory*/,
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
      wxc_String name;
      item->pgrp->getGrpName(*item->pEDID, name);
      std::string display = item->pgrp->CodeName.IsEmpty()
         ? name.std_str()
         : item->pgrp->CodeName.std_str() + ": " + name.std_str();
      gtk_label_set_text(item->bound_label, display.c_str());
      gtk_widget_set_tooltip_text(GTK_WIDGET(item->bound_label), display.c_str());
   }
}

static edi_grp_cl* wnd_selected_group(wxedid_wnd* wnd) {
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

static void wnd_update_group_actions(wxedid_wnd* wnd) {
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

   u8_t tag = wnd_selected_extension_tag(wnd);
   g_simple_action_set_enabled(wnd->add_cta_action, tag == 0x02);
   g_simple_action_set_enabled(wnd->add_displayid_action, tag == 0x70);
}

static void wnd_refresh_raw_view(wxedid_wnd* wnd) {
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
   gtk_text_buffer_set_text(gtk_text_view_get_buffer(wnd->raw_view), text->str, -1);
   g_string_free(text, TRUE);
   g_object_unref(obj);
}

static void wnd_refresh_selected_tree_label(wxedid_wnd* wnd) {
   GtkTreeListRow* row = GTK_TREE_LIST_ROW(
      gtk_single_selection_get_selected_item(wnd->tree_sel));
   if (row == NULL) return;
   GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
   if (obj == NULL) return;
   wnd_refresh_tree_label(WXEDID_ITEM(obj));
   g_object_unref(obj);
}

static void wnd_refresh_group_tree_label(wxedid_wnd* wnd, edi_grp_cl* group) {
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

static void wnd_update_history_state(wxedid_wnd* wnd) {
   wnd->dirty = (wnd->saved_history_position < 0) ||
      (wnd->history_position != static_cast<size_t>(wnd->saved_history_position));
   g_simple_action_set_enabled(wnd->undo_action, wnd->history_position > 0);
   g_simple_action_set_enabled(wnd->redo_action,
                               wnd->history_position < wnd->history.size());
}

static edi_grp_cl* history_owned_group(const wxedid_wnd* wnd, size_t index) {
   const wxedid_history_entry& entry = wnd->history[index];
   bool applied = index < wnd->history_position;
   if (entry.kind == HISTORY_INSERT) return applied ? NULL : entry.group;
   if (entry.kind == HISTORY_REMOVE) return applied ? entry.group : NULL;
   if (entry.kind == HISTORY_REPLACE) return applied ? entry.group : entry.replacement;
   return NULL;
}

static void wnd_drop_history(wxedid_wnd* wnd, size_t from) {
   for (size_t index=from; index<wnd->history.size(); index++) {
      delete history_owned_group(wnd, index);
   }
   wnd->history.erase(wnd->history.begin() + from, wnd->history.end());
}

static void wnd_clear_history(wxedid_wnd* wnd) {
   wnd_drop_history(wnd, 0);
   wnd->history_position = 0;
}

static void wnd_push_history(wxedid_wnd* wnd, const wxedid_history_entry& entry) {
   if (wnd->history_position < wnd->history.size()) {
      if ((wnd->saved_history_position >= 0) &&
          (static_cast<size_t>(wnd->saved_history_position) > wnd->history_position)) {
         wnd->saved_history_position = -1;
      }
      wnd_drop_history(wnd, wnd->history_position);
   }
   wnd->history.push_back(entry);
   wnd->history_position = wnd->history.size();
   wnd_update_history_state(wnd);
}

static void wnd_record_history(wxedid_wnd* wnd, edi_grp_cl* group,
                               edi_dynfld_t* field, bool integer,
                               const wxc_String& before_text, u32_t before_value,
                               const wxc_String& after_text, u32_t after_value) {
   if (wnd->applying_history) return;
   if (integer ? (before_value == after_value) : (before_text == after_text)) return;

   wxedid_history_entry entry = {};
   entry.kind = HISTORY_FIELD;
   entry.group = group;
   entry.field = field;
   entry.integer = integer;
   entry.before_text = before_text.c_str();
   entry.after_text = after_text.c_str();
   entry.before_value = before_value;
   entry.after_value = after_value;
   wnd_push_history(wnd, entry);
}

static void wnd_record_structure(wxedid_wnd* wnd, history_kind kind,
                                 edi_grp_cl* group, GroupAr_cl* array,
                                 u32_t index, bool up, edi_grp_cl* parent) {
   wxedid_history_entry entry = {};
   entry.kind = kind;
   entry.group = group;
   entry.array = array;
   entry.parent = parent;
   entry.index = index;
   entry.up = up;
   wnd_push_history(wnd, entry);
}

//insert a group that a removal took out, at its original index
static void history_restore_group(const wxedid_history_entry& entry) {
   EDID_cl::InsertGroupAt(entry.array, entry.index, entry.group, entry.parent);
}

//replay or revert a structural entry; returns the group to select
static edi_grp_cl* history_apply_structure(const wxedid_history_entry& entry,
                                           bool redo, bool* ok) {
   GroupAr_cl* array = entry.array;
   *ok = true;
   if (entry.kind == HISTORY_REPLACE) {
      edi_grp_cl* current = redo ? entry.group : entry.replacement;
      edi_grp_cl* next = redo ? entry.replacement : entry.group;
      *ok = EDID_cl::ReplaceGroup(current, next);
      return next;
   }
   if (entry.kind == HISTORY_MOVE) {
      if (redo) {
         if (entry.up) array->MoveUp(entry.index); else array->MoveDn(entry.index);
      } else {
         if (entry.up) array->MoveDn(entry.index - 1); else array->MoveUp(entry.index + 1);
      }
      return entry.group;
   }

   bool insert = (entry.kind == HISTORY_INSERT) == redo;
   if (insert) {
      history_restore_group(entry);
      return entry.group;
   }
   if ((entry.index >= array->GetCount()) ||
       (array->Item(entry.index) != entry.group) ||
       (array->Cut(entry.index) != entry.group)) {
      *ok = false;
      return NULL;
   }
   if (entry.index < array->GetCount()) return array->Item(entry.index);
   return (entry.index > 0) ? array->Item(entry.index - 1) : entry.parent;
}

static bool wnd_apply_history_step(wxedid_wnd* wnd, bool redo) {
   if (redo) {
      if (wnd->history_position >= wnd->history.size()) return false;
   } else if (wnd->history_position == 0) {
      return false;
   }

   size_t index = redo ? wnd->history_position : wnd->history_position - 1;
   const wxedid_history_entry& entry = wnd->history[index];
   if (entry.kind != HISTORY_FIELD) {
      bool ok = false;
      edi_grp_cl* selection = history_apply_structure(entry, redo, &ok);
      if (! ok) {
         wnd->doc->GLog.DoLog(
            "[E!] Couldn’t restore the previous structure. Reopen the file before "
            "editing again.");
         return false;
      }
      wnd->history_position = redo ? index + 1 : index;
      wnd->invalid_fields = 0;
      wnd_rebuild_tree(wnd, selection);
      wnd_update_history_state(wnd);
      wnd_update_document_ui(wnd);
      return true;
   }

   wxc_String text(redo ? entry.after_text.c_str() : entry.before_text.c_str());
   u32_t value = redo ? entry.after_value : entry.before_value;
   wnd->applying_history = true;
   rcode result = (wnd->doc->EDID.*entry.field->field.handlerfn)(
      entry.integer ? OP_WRINT : OP_WRSTR, text, value, entry.field);
   wnd->applying_history = false;
   if (! RCD_IS_OK(result)) {
      wnd->doc->GLog.DoLog(
         "[E!] Couldn’t restore the previous value. Reopen the file before editing again.");
      return false;
   }

   wnd->history_position = redo ? index + 1 : index;
   wnd->invalid_fields = 0;
   wnd_refresh_group_tree_label(wnd, entry.group);
   if (wnd_selected_group(wnd) == entry.group) {
      wnd_refresh_group_title(wnd, entry.group);
      rows_reload(wnd->fields, entry.group, &wnd->doc->EDID, wnd);
      timing_load_group(wnd->timing, entry.group, &wnd->doc->EDID);
      wnd_refresh_raw_view(wnd);
   }
   wnd_update_history_state(wnd);
   wnd_update_document_ui(wnd);
   return true;
}

static void wnd_flush_refresh(wxedid_wnd* wnd);

static void wnd_apply_history(wxedid_wnd* wnd, bool redo) {
   wnd_flush_refresh(wnd);
   if (! wnd_apply_history_step(wnd, redo)) return;
   //a rebuild is undone and redone together with the field write behind it
   if (redo) {
      size_t next = wnd->history_position;
      if ((next < wnd->history.size()) && wnd->history[next].joined)
         wnd_apply_history_step(wnd, true);
   } else if (wnd->history[wnd->history_position].joined) {
      wnd_apply_history_step(wnd, false);
   }
}

//------------
// group rebuilds: a field write can change the group type or layout
static void wnd_flush_refresh(wxedid_wnd* wnd) {
   if (wnd->refresh_source != 0) {
      g_source_remove(wnd->refresh_source);
      wnd->refresh_source = 0;
   }
   edi_grp_cl* group = wnd->refresh_group;
   edi_dynfld_t* field = wnd->refresh_field;
   bool type_changed = wnd->refresh_type_changed;
   wnd->refresh_group = NULL;
   wnd->refresh_field = NULL;
   wnd->refresh_type_changed = false;
   if (group == NULL) return;

   const wxedid_history_entry* last = (wnd->history_position == 0) ? NULL :
      &wnd->history[wnd->history_position - 1];
   bool joined = (last != NULL) && (wnd->history_position == wnd->history.size()) &&
                 (last->kind == HISTORY_FIELD) && (last->group == group) &&
                 (last->field == field);

   edi_grp_cl* target = NULL;
   rcode result;
   edi_grp_cl* rebuilt = wnd->doc->EDID.RebuildGroup(group, field, type_changed,
                                                     &target, result);
   const char* refused = NULL;
   if ((rebuilt == NULL) && ! RCD_IS_OK(result)) {
      refused = "This change is not possible here. The previous value was restored.";
   } else if ((rebuilt != NULL) && ! EDID_cl::ReplaceGroup(target, rebuilt)) {
      delete rebuilt;
      refused = "This change does not fit in the block. The previous value was restored.";
   }
   if (refused != NULL) {
      if (joined) {
         wnd_apply_history_step(wnd, false);
         wnd_drop_history(wnd, wnd->history_position);
         wnd_update_history_state(wnd);
      }
      wnd_show_error(wnd, refused);
      return;
   }
   if (rebuilt == NULL) return;

   //the replacement now sits where target was
   GroupAr_cl* array = rebuilt->getParentAr();
   u32_t index = rebuilt->getParentArIdx();
   edi_grp_cl* parent = rebuilt->getParentGrp();
   edi_grp_cl* selected = wnd_selected_group(wnd);
   bool follow = (selected == target) || (selected == group);

   wxedid_history_entry entry = {};
   entry.kind = HISTORY_REPLACE;
   entry.group = target;
   entry.replacement = rebuilt;
   entry.array = array;
   entry.parent = parent;
   entry.index = index;
   entry.joined = joined;
   wnd_push_history(wnd, entry);
   wnd->invalid_fields = 0;
   wnd_rebuild_tree(wnd, follow ? rebuilt : selected);
   wnd_update_document_ui(wnd);
}

static gboolean wnd_on_refresh_idle(gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   wnd->refresh_source = 0;
   wnd_flush_refresh(wnd);
   return G_SOURCE_REMOVE;
}

//remember a write that may need a rebuild; entries apply it once editing ends
static void wnd_request_refresh(wxedid_wnd* wnd, edi_grp_cl* group,
                                edi_dynfld_t* field, bool type_changed,
                                bool now) {
   bool layout_field = (field->field.flags & (F_FR | F_INIT)) != 0;
   if (! type_changed && ! layout_field) return;
   if ((wnd->refresh_group != NULL) &&
       ((wnd->refresh_group != group) || (wnd->refresh_field != field))) {
      wnd_flush_refresh(wnd);
   }
   wnd->refresh_group = group;
   wnd->refresh_field = field;
   wnd->refresh_type_changed = wnd->refresh_type_changed || type_changed;
   if (now && (wnd->refresh_source == 0)) {
      wnd->refresh_source = g_idle_add(wnd_on_refresh_idle, wnd);
   }
}

static void wnd_schedule_refresh(wxedid_wnd* wnd) {
   if ((wnd->refresh_group != NULL) && (wnd->refresh_source == 0)) {
      wnd->refresh_source = g_idle_add(wnd_on_refresh_idle, wnd);
   }
}

static void wnd_on_undo_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_apply_history(static_cast<wxedid_wnd*>(user_data), false);
}

static void wnd_on_redo_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_apply_history(static_cast<wxedid_wnd*>(user_data), true);
}

static void wnd_on_tree_select(GtkSelectionModel* selmodel, guint /*position*/,
                               guint /*n_items*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;

   GtkTreeListRow* row = (GtkTreeListRow*) gtk_single_selection_get_selected_item(GTK_SINGLE_SELECTION(selmodel));
   if (row == NULL) return;

   GObject* obj = G_OBJECT(gtk_tree_list_row_get_item(row));
   if (obj == NULL) return;

   wxedid_item* it = WXEDID_ITEM(obj);
   wnd->last_selected = it->pgrp;
   wxc_String group_name;
   if (it->pgrp != NULL) {
      it->pgrp->getGrpName(*it->pEDID, group_name);
   } else {
      group_name = it->label;
   }
   gtk_label_set_text(wnd->group_title, group_name.c_str());
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

static void wnd_on_duplicate_group(GSimpleAction*, GVariant*, gpointer user_data) {
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
      wnd_show_error(wnd, "This group cannot be duplicated in the available space");
      return;
   }
   array->InsertDn(group->getParentArIdx(), copy);
   wnd_record_structure(wnd, HISTORY_INSERT, copy, array, copy->getParentArIdx(),
                        false, copy->getParentGrp());
   wnd_finish_structure_change(wnd, copy, "Group duplicated");
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
                                  up ? "Group moved up" : "Group moved down");
   }
}

static void wnd_on_move_group_up(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_move_group(static_cast<wxedid_wnd*>(user_data), true);
}

static void wnd_on_move_group_down(GSimpleAction*, GVariant*, gpointer user_data) {
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
      wnd_show_error(wnd, "This group couldn’t be deleted");
      return;
   }
   wnd_record_structure(wnd, HISTORY_REMOVE, group, array, index, false, parent);
   wnd_finish_structure_change(wnd, next, "Group deleted");
}

static void wnd_on_delete_group(GSimpleAction*, GVariant*, gpointer user_data) {
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
            "“%s” and its fields will be removed from this EDID.", name.c_str());
   AdwAlertDialog* dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new(
      "Delete this group?", body));
   adw_alert_dialog_add_responses(dialog,
                                  "cancel", "Cancel",
                                  "delete", "Delete",
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

static bool wnd_insert_group(GroupAr_cl* array, edi_grp_cl* group) {
   if ((array == NULL) || (group == NULL) || (array->GetCount() == 0)) return false;
   gtid_t type = group->getTypeID();
   bool timing = (type.base_id == ID_DTD);
   u32_t last_data = 0;
   for (u32_t index=1; index<array->GetCount(); index++) {
      gtid_t candidate = array->Item(index)->getTypeID();
      if ((candidate.base_id == ID_DTD) ||
          ((candidate.t32 & ID_PARENT_MASK) == ID_DISPLAYID_PADDING)) {
         if (array->CanInsertUp(index, group)) {
            array->InsertUp(index, group);
            return true;
         }
         break;
      }
      last_data = index;
   }
   if (timing && (last_data + 1 < array->GetCount())) return false;
   if (array->CanInsertDn(last_data, group)) {
      array->InsertDn(last_data, group);
      return true;
   }
   return false;
}

static void wnd_on_add_cta_group(GSimpleAction*, GVariant* parameter,
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
   GroupAr_cl* array = wnd_selected_root_array(wnd);
   if (! RCD_IS_OK(result) || ! wnd_insert_group(array, group)) {
      delete group;
      wnd_show_error(wnd, "This CTA group does not fit in the selected block");
      return;
   }
   wnd_record_structure(wnd, HISTORY_INSERT, group, array, group->getParentArIdx(),
                        false, NULL);
   wnd_finish_structure_change(wnd, group, "CTA group added");
}

static void wnd_on_add_displayid_group(GSimpleAction*, GVariant*,
                                       gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   wnd_flush_refresh(wnd);
   GroupAr_cl* array = wnd_selected_root_array(wnd);
   if ((array == NULL) || (array->GetCount() == 0)) return;
   u8_t version = array->Item(0)->getInstPtr()[1];
   edi_grp_cl* group = NULL;
   rcode result = wnd->doc->EDID.CreateGroup(
      EDID_cl::DISPLAYID_DATA, version, &group);
   if (! RCD_IS_OK(result) || ! wnd_insert_group(array, group)) {
      delete group;
      wnd_show_error(wnd, "A DisplayID block does not fit in the selected section");
      return;
   }
   wnd_record_structure(wnd, HISTORY_INSERT, group, array, group->getParentArIdx(),
                        false, NULL);
   wnd_finish_structure_change(wnd, group, "DisplayID data block added");
}

static void wnd_popup_group_menu(wxedid_wnd* wnd, double x, double y) {
   if ((wnd->group_menu == NULL) || (wnd_selected_group(wnd) == NULL)) return;
   GdkRectangle point = {
      static_cast<int>(x), static_cast<int>(y), 1, 1,
   };
   gtk_popover_set_pointing_to(GTK_POPOVER(wnd->group_menu), &point);
   gtk_popover_popup(GTK_POPOVER(wnd->group_menu));
}

static void wnd_on_tree_item_context(GtkGestureClick* gesture, int /*presses*/,
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

static gboolean wnd_on_tree_key(GtkEventControllerKey*, guint keyval,
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

static void wnd_update_document_ui(wxedid_wnd* wnd) {
   bool can_save = wnd->loaded && wnd->dirty && (wnd->invalid_fields == 0);
   g_simple_action_set_enabled(wnd->save_action, can_save);
   bool can_write = wnd->loaded && (wnd->invalid_fields == 0);
   g_simple_action_set_enabled(wnd->save_as_action, can_write);
   g_simple_action_set_enabled(wnd->export_hex_action, can_write);
   g_simple_action_set_enabled(wnd->save_report_action, can_write);
   gtk_widget_set_visible(wnd->save_button, wnd->loaded);
   gtk_button_set_label(GTK_BUTTON(wnd->save_button), "_Save");
   gtk_button_set_use_underline(GTK_BUTTON(wnd->save_button), TRUE);
   gtk_widget_set_tooltip_text(
      wnd->save_button,
      wnd->document_hex    ? "Save as an EDID binary (Ctrl+S)" :
      wnd->source_writable ? "Save changes (Ctrl+S)" :
                             "Save a writable copy (Ctrl+S)");

   if (wnd->loaded) {
      char* basename = document_basename(wnd->doc->path);
      char* display_path = g_filename_display_name(wnd->doc->path);
      char* window_name = g_strdup_printf("%s — EDID Editor", basename);
      const char* state = wnd->dirty ? "Modified" : NULL;
      char* subtitle = NULL;
      if (wnd->document_hex) {
         subtitle = (state != NULL)
            ? g_strdup_printf("%s · Imported · %s", state, display_path)
            : g_strdup_printf("Imported · %s", display_path);
      } else if (! wnd->source_writable && (state != NULL)) {
         subtitle = g_strdup_printf("%s · Read-only · %s", state, display_path);
      } else if (! wnd->source_writable) {
         subtitle = g_strdup_printf("Read-only · %s", display_path);
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
      adw_window_title_set_title(wnd->window_title, "EDID Editor");
      adw_window_title_set_subtitle(wnd->window_title, "EDID editor");
      gtk_window_set_title(wnd->window, "EDID Editor");
   }

   if (wnd->invalid_fields > 0) {
      adw_banner_set_title(wnd->banner, "Enter a valid value before saving");
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

static void wnd_update_header_controls(wxedid_wnd* wnd) {
   bool collapsed = adw_overlay_split_view_get_collapsed(wnd->split_view);
   gtk_widget_set_visible(GTK_WIDGET(wnd->window_title), ! collapsed);
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
   wnd->banner_offers_retry = false;
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
   wnd->banner_offers_retry = false;
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

static void wnd_reload_source(wxedid_wnd* wnd);

static void wnd_on_banner_details(AdwBanner* /*banner*/, gpointer user_data) {
   wxedid_wnd* wnd = (wxedid_wnd*) user_data;
   if (wnd->banner_offers_retry) {
      wnd->doc->EDID.b_ERR_Ignore = true;
      g_simple_action_set_state(wnd->ignore_errors_action,
                                g_variant_new_boolean(TRUE));
      wnd_reload_source(wnd);
      return;
   }
   GVariant* state = g_action_get_state(G_ACTION(wnd->details_action));
   bool visible = g_variant_get_boolean(state);
   g_variant_unref(state);
   if (! visible) g_action_activate(G_ACTION(wnd->details_action), NULL);
}

static void wnd_rebuild_tree(wxedid_wnd* wnd, edi_grp_cl* select_group = NULL) {
   EDID_cl& edid = wnd->doc->EDID;
   edi_buf_t* buffer = edid.getEDID();
   GListStore* root = g_list_store_new(WXEDID_TYPE_ITEM);

   if (edid.EDI_BaseGrpAr.GetCount() > 0) {
      wxedid_item* base = wxedid_item_new_block(
         "Block 0: Base EDID", &edid.EDI_BaseGrpAr, &edid);
      g_list_store_append(root, base);
      g_object_unref(base);
   }

   for (u32_t block=1; block<edid.getNumValidBlocks(); block++) {
      GroupAr_cl* groups = edid.BlkGroupsAr[block];
      if (groups->GetCount() > 0) {
         const char* type = (buffer->blk[block][0] == 0x02) ? "CTA-861" :
                            (buffer->blk[block][0] == 0x70) ? "DisplayID" :
                                                             "Extension";
         char label[64];
         snprintf(label, sizeof(label), "Block %u: %s", block, type);
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

static void wnd_offer_retry(wxedid_wnd* wnd) {
   if (wnd->doc->EDID.b_ERR_Ignore || wnd->source_path.empty()) return;
   adw_banner_set_button_label(wnd->banner, "Open Anyway");
   adw_banner_set_revealed(wnd->banner, TRUE);
   wnd->banner_offers_retry = true;
}

static void wnd_load_bytes(wxedid_wnd* wnd, const char* path,
                           const u8_t* data, size_t size, bool hex_source) {
   bool ignore_errors = wnd->doc->EDID.b_ERR_Ignore;
   bool partial = (size % sizeof(ediblk_t)) != 0;
   if ((size == 0) || (size > sizeof(edi_t)) || (partial && ! ignore_errors)) {
      char msg[1400];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t open %s: EDID data must contain 1 to 4 complete "
               "128-byte blocks. Choose another EDID file.", path);
      wnd->doc->GLog.DoLog(msg);
      if ((size > 0) && (size <= sizeof(edi_t))) wnd_offer_retry(wnd);
      return;
   }

   edi_buf_t loaded = {};
   memcpy(loaded.buff, data, size);
   size_t blocks = (size + sizeof(ediblk_t) - 1) / sizeof(ediblk_t);
   if (partial) {
      char msg[160];
      snprintf(msg, sizeof(msg),
               "[i] The last block is incomplete; its missing %zu bytes are read as zero",
               (blocks * sizeof(ediblk_t)) - size);
      wnd->doc->GLog.DoLog(msg);
   }

   //dumps often hold only the blocks the base block counts, even when an
   //override block declares more
   u32_t declared = EDID_cl::DeclaredBlocks(loaded.buff, blocks * sizeof(ediblk_t));
   u32_t base_declared = 1U + loaded.edi.base.num_extblk;
   bool block_count_adjusted = false;
   if ((declared != blocks) && (base_declared == blocks)) {
      char msg[192];
      snprintf(msg, sizeof(msg),
               "[i] The EDID Extension Override block declares %u blocks; the file "
               "holds the %zu blocks the base block declares", declared, blocks);
      wnd->doc->GLog.DoLog(msg);
   } else if (declared != blocks) {
      char msg[192];
      if (! ignore_errors) {
         snprintf(msg, sizeof(msg),
                  "[E!] Couldn’t open this EDID: it declares %u blocks, but the "
                  "file contains %zu. Choose a file with a matching block count.",
                  declared, blocks);
         wnd->doc->GLog.DoLog(msg);
         wnd_offer_retry(wnd);
         return;
      }
      snprintf(msg, sizeof(msg),
               "[i] This EDID declares %u blocks, but %zu are present; the block "
               "count now matches the data", declared, blocks);
      wnd->doc->GLog.DoLog(msg);
      //the count lives in the override block when there is one
      if (declared != (1U + loaded.edi.base.num_extblk)) {
         loaded.buff[sizeof(ediblk_t) + 6] = static_cast<u8_t>(blocks - 1);
      } else {
         loaded.edi.base.num_extblk = static_cast<u8_t>(blocks - 1);
      }
      block_count_adjusted = true;
   }

   //the pending rebuild refers to groups that are about to be released
   if (wnd->refresh_source != 0) g_source_remove(wnd->refresh_source);
   wnd->refresh_source = 0;
   wnd->refresh_group = NULL;
   wnd->refresh_field = NULL;
   wnd->refresh_type_changed = false;
   wnd->doc->EDID.Clear();
   edi_buf_t* pbuf = wnd->doc->EDID.getEDID();
   memcpy(pbuf->buff, loaded.buff, blocks * sizeof(ediblk_t));

   rcode retU;
   u32_t parsed_extblk = 0;
   bool extension_failed = false;

   retU = wnd->doc->EDID.ParseEDID_Base(parsed_extblk);
   bool base_ok = RCD_IS_OK(retU);
   parsed_extblk = blocks - 1;
   if (base_ok) {
      for (u32_t block=1; block<=parsed_extblk; block++) {
         u8_t tag = pbuf->blk[block][0];
         bool parsed = false;
         if (tag == 0x02) {
            retU = wnd->doc->EDID.ParseEDID_CEA(block);
            parsed = true;
         } else if (tag == 0x70) {
            retU = wnd->doc->EDID.ParseEDID_DisplayID(block);
            parsed = true;
         }
         if (parsed && ! RCD_IS_OK(retU)) {
            wnd->doc->GLog.PrintRcode(retU);
            wnd->doc->EDID.BlkGroupsAr[block]->Clear();
            extension_failed = true;
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

   if (base_ok) {
      for (u32_t block=1; block<=parsed_extblk; block++) {
         if (wnd->doc->EDID.BlkGroupsAr[block]->GetCount() != 0) continue;
         u8_t tag = pbuf->blk[block][0];
         char msg[144];
         snprintf(msg, sizeof(msg),
                  "[i] Extension block %u (tag 0x%02X) preserved read-only",
                  block, tag);
         wnd->doc->GLog.DoLog(msg);
      }
   }

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
      gtk_stack_set_visible_child_name(wnd->content_stack, "editor");
      wnd_rebuild_tree(wnd);
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

static bool path_is_hex_text(const char* path) {
   char* folded = g_utf8_casefold(path, -1);
   bool hex = g_str_has_suffix(folded, ".hex") || g_str_has_suffix(folded, ".txt");
   g_free(folded);
   return hex;
}

static void wnd_load_file(wxedid_wnd* wnd, const char* path, bool hex) {
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

static void wnd_reload_source(wxedid_wnd* wnd) {
   std::string path = wnd->source_path;
   if (! path.empty()) wnd_load_file(wnd, path.c_str(), wnd->source_hex);
}

static GListStore* file_filters(const char* name, const char* const* patterns) {
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

enum open_mode {
   OPEN_FILE,
   OPEN_HEX,
   OPEN_DISPLAY,
};

static void wnd_on_display_open(GtkButton* button, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   const char* path = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "path"));
   AdwDialog* dialog = ADW_DIALOG(gtk_widget_get_ancestor(GTK_WIDGET(button), ADW_TYPE_DIALOG));
   std::string source = (path != NULL) ? path : "";
   if (dialog != NULL) adw_dialog_close(dialog);
   if (! source.empty()) wnd_load_file(wnd, source.c_str(), false);
}

static void wnd_present_display_dialog(wxedid_wnd* wnd) {
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
      gtk_widget_set_tooltip_text(open, "Open");
      g_object_set_data_full(G_OBJECT(open), "path", g_strdup(display.path.c_str()), g_free);
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
   adw_dialog_set_title(dialog, "Open from Display");
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
   open_mode mode = static_cast<open_mode>(
      GPOINTER_TO_INT(g_object_get_data(source, "wxedid-open-mode")));
   if (0 == strcmp(response, "discard")) wnd_present_open_dialog(wnd, mode);
}

static void wnd_request_open(wxedid_wnd* wnd, open_mode mode) {
   if (! wnd->dirty) {
      wnd_present_open_dialog(wnd, mode);
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
   adw_alert_dialog_choose(dialog, GTK_WIDGET(wnd->window), NULL,
                           wnd_on_discard_open_response, wnd);
}

static void wnd_on_open_action(GSimpleAction* /*action*/, GVariant* /*parameter*/,
                               gpointer user_data) {
   wnd_request_open((wxedid_wnd*) user_data, OPEN_FILE);
}

static void wnd_on_import_hex_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_request_open(static_cast<wxedid_wnd*>(user_data), OPEN_HEX);
}

static void wnd_on_open_display_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_request_open(static_cast<wxedid_wnd*>(user_data), OPEN_DISPLAY);
}

//------------
// output: assemble groups into the buffer and recompute checksums
static bool wnd_prepare_output(wxedid_wnd* wnd) {
   wnd_flush_refresh(wnd);
   edi_buf_t* pbuf = wnd->doc->EDID.getEDID();
   u32_t parsed_blocks = wnd->doc->EDID.getNumValidBlocks();
   u32_t declared_blocks = EDID_cl::DeclaredBlocks(pbuf->buff,
                                                   parsed_blocks * sizeof(ediblk_t));
   if ((declared_blocks != parsed_blocks) &&
       ((1U + pbuf->edi.base.num_extblk) != parsed_blocks)) {
      char msg[160];
      snprintf(msg, sizeof(msg),
               "[E!] Couldn’t write this EDID: it declares %u blocks, but only %u "
               "were parsed. Reopen valid EDID data, then try again.",
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
               "[E!] Couldn’t write this EDID: %s. Fix invalid data, then try again.",
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
   wnd_refresh_raw_view(wnd);
   return true;
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

static void wnd_on_save_action(GSimpleAction* /*action*/, GVariant* /*parameter*/,
                               gpointer user_data) {
   wnd_request_save((wxedid_wnd*) user_data);
}

static void wnd_on_save_as_action(GSimpleAction*, GVariant*, gpointer user_data) {
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

static void wnd_on_export_hex_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   if (! wnd_prepare_output(wnd)) return;
   edi_buf_t* buffer = wnd->doc->EDID.getEDID();
   std::string hex = edid_hex_encode(
      buffer->buff, wnd->doc->EDID.getNumValidBlocks() * sizeof(ediblk_t));
   wnd_present_text_save_dialog(wnd, "Export EDID as hex", "hex", "Hex text",
                                hex, "Exported");
}

static void wnd_on_save_report_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   if (! wnd_prepare_output(wnd)) return;
   char* source = document_basename(wnd->doc->path);
   std::string report = edid_text_report(wnd->doc->EDID, source, WXEDID_VERSION);
   g_free(source);
   wnd_present_text_save_dialog(wnd, "Save EDID report", "txt", "Text",
                                report, "Saved report");
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

static void wnd_on_about_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wxedid_wnd* wnd = static_cast<wxedid_wnd*>(user_data);
   static const char* developers[] = {
      "Tomasz Pawlak",
      "sachesi",
      NULL,
   };
   AdwAboutDialog* dialog = ADW_ABOUT_DIALOG(adw_about_dialog_new());
   adw_about_dialog_set_application_name(dialog, "EDID Editor");
   adw_about_dialog_set_application_icon(dialog, "io.github.sachesi.EdidEditor");
   adw_about_dialog_set_developer_name(dialog, "sachesi");
   adw_about_dialog_set_version(dialog, WXEDID_VERSION);
   adw_about_dialog_set_comments(dialog,
      "Inspect and edit Extended Display Identification Data.\n\n"
      "Based on wxEDID by Tomasz Pawlak.");
   adw_about_dialog_add_link(dialog, "wxEDID, the original project",
                             "https://sourceforge.net/projects/wxedid/");
   adw_about_dialog_set_developers(dialog, developers);
   adw_about_dialog_set_copyright(dialog, "Copyright © 2014–2025 Tomasz Pawlak");
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
         "Open the file again to read it with errors ignored"));
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
      {"Files", {
         {"Open a file", "<Control>o"},
         {"Save changes", "<Control>s"},
         {"Save as a new file", "<Control><Shift>s"},
      }},
      {"Editing", {
         {"Undo", "<Control>z"},
         {"Redo", "<Control><Shift>z"},
      }},
      {"Groups", {
         {"Duplicate group", "<Control>d"},
         {"Delete group", "Delete"},
         {"Move group up", "<Alt>Up"},
         {"Move group down", "<Alt>Down"},
         {"Show group menu", "<Shift>F10"},
      }},
      {"General", {
         {"Keyboard shortcuts", "<Control>question"},
      }},
   };
   AdwDialog* dialog = adw_shortcuts_dialog_new();
   for (const section& spec : sections) {
      AdwShortcutsSection* group = adw_shortcuts_section_new(spec.title);
      for (const shortcut& item : spec.items) {
         if (item.title == NULL) break;
         adw_shortcuts_section_add(group,
                                   adw_shortcuts_item_new(item.title, item.accelerator));
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
   gtk_window_set_title(GTK_WINDOW(window), "EDID Editor");
   g_signal_connect(window, "close-request", G_CALLBACK(wnd_on_close_request), wnd);

   g_object_set_data_full(G_OBJECT(window), "wxedid-wnd", wnd,
                           [](gpointer data) {
                              wxedid_wnd* w = (wxedid_wnd*) data;
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

   wnd->details_action = g_simple_action_new_stateful(
      "details", NULL, g_variant_new_boolean(FALSE));
   g_simple_action_set_enabled(wnd->details_action, FALSE);
   g_signal_connect(wnd->details_action, "activate",
                    G_CALLBACK(wnd_on_details_action), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->details_action));
   g_object_unref(wnd->details_action);

   wnd->duplicate_action = g_simple_action_new("duplicate-group", NULL);
   g_signal_connect(wnd->duplicate_action, "activate",
                    G_CALLBACK(wnd_on_duplicate_group), wnd);
   g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(wnd->duplicate_action));
   g_object_unref(wnd->duplicate_action);

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
   const char* shortcuts_accels[] = {"<Control>question", NULL};
   gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.shortcuts",
                                         shortcuts_accels);

   //header bar
   GtkWidget* header = adw_header_bar_new();
   wnd->window_title = ADW_WINDOW_TITLE(adw_window_title_new("EDID Editor", "Display identification data"));
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
   GMenu* open_section = g_menu_new();
   g_menu_append(open_section, "Open…", "win.open");
   g_menu_append(open_section, "Open from Display…", "win.open-display");
   g_menu_append(open_section, "Import Hex…", "win.import-hex");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(open_section));
   g_object_unref(open_section);
   GMenu* save_section = g_menu_new();
   g_menu_append(save_section, "Save As…", "win.save-as");
   g_menu_append(save_section, "Export Hex…", "win.export-hex");
   g_menu_append(save_section, "Save Report…", "win.save-report");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(save_section));
   g_object_unref(save_section);
   GMenu* edit_section = g_menu_new();
   g_menu_append(edit_section, "Undo", "win.undo");
   g_menu_append(edit_section, "Redo", "win.redo");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(edit_section));
   g_object_unref(edit_section);
   GMenu* option_section = g_menu_new();
   g_menu_append(option_section, "Ignore EDID Errors", "win.ignore-errors");
   g_menu_append(option_section, "Edit Read-Only Fields", "win.ignore-read-only");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(option_section));
   g_object_unref(option_section);
   GMenu* help_section = g_menu_new();
   g_menu_append(help_section, "Show details", "win.details");
   g_menu_append(help_section, "Keyboard Shortcuts", "win.shortcuts");
   g_menu_append(help_section, "About EDID Editor", "win.about");
   g_menu_append_section(primary_menu, NULL, G_MENU_MODEL(help_section));
   g_object_unref(help_section);
   GtkWidget* btn_menu = gtk_menu_button_new();
   gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(btn_menu), "open-menu-symbolic");
   gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(btn_menu), G_MENU_MODEL(primary_menu));
   gtk_menu_button_set_primary(GTK_MENU_BUTTON(btn_menu), TRUE);
   gtk_widget_set_tooltip_text(btn_menu, "Main menu");
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
   wnd->tree = GTK_LIST_VIEW(gtk_list_view_new(
      GTK_SELECTION_MODEL(wnd->tree_sel), factory));
   gtk_widget_add_css_class(GTK_WIDGET(wnd->tree), "navigation-sidebar");
   g_signal_connect(wnd->tree_sel, "selection-changed",
                    G_CALLBACK(wnd_on_tree_select), wnd);

   GMenu* add_menu = g_menu_new();
   g_menu_append(add_menu, "LPCM Audio Block", "win.add-cta-group::audio-lpcm");
   g_menu_append(add_menu, "Extended Audio Block", "win.add-cta-group::audio-extended");
   g_menu_append(add_menu, "Video Block", "win.add-cta-group::video");
   g_menu_append(add_menu, "Detailed Timing", "win.add-cta-group::timing");
   g_menu_append(add_menu, "DisplayID Data Block", "win.add-displayid-group");

   GMenu* group_menu_model = g_menu_new();
   g_menu_append_submenu(group_menu_model, "Add", G_MENU_MODEL(add_menu));
   g_menu_append(group_menu_model, "Duplicate", "win.duplicate-group");
   g_menu_append(group_menu_model, "Move Up", "win.move-group-up");
   g_menu_append(group_menu_model, "Move Down", "win.move-group-down");
   g_menu_append(group_menu_model, "Delete", "win.delete-group");
   wnd->group_menu = GTK_POPOVER_MENU(
      gtk_popover_menu_new_from_model(G_MENU_MODEL(group_menu_model)));
   gtk_widget_set_parent(GTK_WIDGET(wnd->group_menu), GTK_WIDGET(wnd->tree));
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
   gtk_search_entry_set_placeholder_text(wnd->tree_search, "Search groups");
   gtk_accessible_update_property(GTK_ACCESSIBLE(wnd->tree_search),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  "Search groups", -1);
   gtk_widget_set_margin_start(GTK_WIDGET(wnd->tree_search), 12);
   gtk_widget_set_margin_end(GTK_WIDGET(wnd->tree_search), 12);
   gtk_widget_set_margin_top(GTK_WIDGET(wnd->tree_search), 12);
   gtk_widget_set_margin_bottom(GTK_WIDGET(wnd->tree_search), 6);
   g_signal_connect(wnd->tree_search, "search-changed",
                    G_CALLBACK(tree_search_changed), wnd);

   GtkWidget* search_empty = adw_status_page_new();
   adw_status_page_set_icon_name(ADW_STATUS_PAGE(search_empty),
                                 "edit-find-symbolic");
   adw_status_page_set_title(ADW_STATUS_PAGE(search_empty),
                             "No matching groups");
   adw_status_page_set_description(ADW_STATUS_PAGE(search_empty),
                                   "Try a different search.");
   GtkWidget* clear_search = gtk_button_new_with_mnemonic("_Clear Search");
   gtk_widget_set_halign(clear_search, GTK_ALIGN_CENTER);
   g_signal_connect(clear_search, "clicked", G_CALLBACK(tree_search_clear), wnd);
   adw_status_page_set_child(ADW_STATUS_PAGE(search_empty), clear_search);

   wnd->sidebar_stack = GTK_STACK(gtk_stack_new());
   gtk_stack_add_named(wnd->sidebar_stack, tree_scroll, "tree");
   gtk_stack_add_named(wnd->sidebar_stack, search_empty, "empty");
   gtk_stack_set_visible_child_name(wnd->sidebar_stack, "tree");
   gtk_widget_set_vexpand(GTK_WIDGET(wnd->sidebar_stack), TRUE);

   GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_box_append(GTK_BOX(sidebar), GTK_WIDGET(wnd->tree_search));
   gtk_box_append(GTK_BOX(sidebar), GTK_WIDGET(wnd->sidebar_stack));

   GtkWidget* group_toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_widget_set_margin_start(group_toolbar, 12);
   gtk_widget_set_margin_end(group_toolbar, 12);
   gtk_widget_set_margin_top(group_toolbar, 6);
   gtk_widget_set_margin_bottom(group_toolbar, 12);
   GtkWidget* add_button = gtk_menu_button_new();
   gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(add_button), "list-add-symbolic");
   gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(add_button), G_MENU_MODEL(add_menu));
   gtk_widget_set_tooltip_text(add_button, "Add a group");
   gtk_accessible_update_property(GTK_ACCESSIBLE(add_button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, "Add a group", -1);
   gtk_box_append(GTK_BOX(group_toolbar), add_button);
   g_object_unref(add_menu);

   struct group_button {
      const char* icon;
      const char* label;
      const char* action;
   };
   static const group_button group_buttons[] = {
      {"edit-copy-symbolic", "Duplicate group (Ctrl+D)", "win.duplicate-group"},
      {"go-up-symbolic", "Move group up (Alt+Up)", "win.move-group-up"},
      {"go-down-symbolic", "Move group down (Alt+Down)", "win.move-group-down"},
      {"user-trash-symbolic", "Delete group (Delete)", "win.delete-group"},
   };
   for (const group_button& spec : group_buttons) {
      GtkWidget* button = gtk_button_new_from_icon_name(spec.icon);
      gtk_actionable_set_action_name(GTK_ACTIONABLE(button), spec.action);
      gtk_widget_set_tooltip_text(button, spec.label);
      gtk_accessible_update_property(GTK_ACCESSIBLE(button),
                                     GTK_ACCESSIBLE_PROPERTY_LABEL, spec.label, -1);
      gtk_box_append(GTK_BOX(group_toolbar), button);
   }
   gtk_box_append(GTK_BOX(sidebar), group_toolbar);

   GtkWidget* right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

   GtkWidget* editor_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
   gtk_widget_set_margin_start(editor_heading, 18);
   gtk_widget_set_margin_end(editor_heading, 18);
   gtk_widget_set_margin_top(editor_heading, 18);
   gtk_widget_set_margin_bottom(editor_heading, 12);

   wnd->group_title = GTK_LABEL(gtk_label_new("Select a group"));
   gtk_label_set_xalign(wnd->group_title, 0.0);
   gtk_label_set_ellipsize(wnd->group_title, PANGO_ELLIPSIZE_END);
   gtk_widget_set_hexpand(GTK_WIDGET(wnd->group_title), TRUE);
   gtk_widget_add_css_class(GTK_WIDGET(wnd->group_title), "title-2");
   gtk_box_append(GTK_BOX(editor_heading), GTK_WIDGET(wnd->group_title));

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
   gtk_text_view_set_bottom_margin(wnd->raw_view, 18);
   gtk_accessible_update_property(GTK_ACCESSIBLE(wnd->raw_view),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  "Selected group bytes", -1);
   GtkWidget* raw_scroll = gtk_scrolled_window_new();
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(raw_scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(raw_scroll),
                                 GTK_WIDGET(wnd->raw_view));

   wnd->editor_stack = ADW_VIEW_STACK(adw_view_stack_new());
   adw_view_stack_set_hhomogeneous(wnd->editor_stack, FALSE);
   adw_view_stack_set_vhomogeneous(wnd->editor_stack, FALSE);
   adw_view_stack_add_titled_with_icon(wnd->editor_stack, fields_scroll,
                                       "fields", "Fields", "view-list-symbolic");
   wnd->timing_stack_page = adw_view_stack_add_titled_with_icon(
      wnd->editor_stack, wnd->timing->page,
      "timing", "Timing", "video-display-symbolic");
   adw_view_stack_add_titled_with_icon(wnd->editor_stack, raw_scroll,
                                       "bytes", "Bytes", "document-properties-symbolic");
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
      G_OBJECT(wnd->timing->drawing), "height-request", 220,
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
   wnd_update_history_state(wnd);
   wnd_update_document_ui(wnd);
   gtk_window_present(GTK_WINDOW(window));
}
