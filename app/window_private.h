/***************************************************************
 * Name:      window_private.h
 * Purpose:   state and functions shared by the parts of the window
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#ifndef EDID_EDITOR_WINDOW_PRIVATE_H
#define EDID_EDITOR_WINDOW_PRIVATE_H 1

#include "window.h"

#include "wxcompat.h"
#include "wxedid_rcd_scope.h"
#include "guilog.h"
#include "vmap.h"
#include "EDID_class.h"
#include "CEA_class.h"
#include "CEA_ET_class.h"
#include "EDID_text.h"
#include "EDID_document.h"
#include "EDID_names.h"
#include "EDID_timing.h"
#include "EDID_display.h"
#include "EDID_summary.h"
#include "EDID_compare.h"
#include "wxedid-config.h"

#include <glib/gi18n.h>

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
   GtkTextBuffer*      log;           //everything logged, shown in the EDID Log
   std::vector<std::string> notes;    //notices from opening the file, on the Overview
   bool                loading;
   AdwOverlaySplitView* split_view;
   AdwWindowTitle*     window_title;
   GtkLabel*           group_title;
   GtkLabel*           group_subtitle; //code, offset and block of the group
   GtkStack*           content_stack;
   AdwBanner*          banner;
   AdwBanner*          source_banner;  //read-only source: save a copy
   AdwToastOverlay*    toast_overlay;
   GtkWidget*          sidebar_button;
   GtkWidget*          add_button;
   GtkWidget*          open_button;
   GtkWidget*          save_button;
   GSimpleAction*      save_action;
   GSimpleAction*      save_as_action;
   GSimpleAction*      undo_action;
   GSimpleAction*      redo_action;
   GSimpleAction*      duplicate_action;
   GSimpleAction*      delete_action;
   GSimpleAction*      move_up_action;
   GSimpleAction*      move_down_action;
   GSimpleAction*      add_cta_action;
   GSimpleAction*      add_displayid_action;
   GSimpleAction*      export_hex_action;
   GSimpleAction*      save_report_action;
   GSimpleAction*      compare_file_action;
   GSimpleAction*      compare_display_action;
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
   bool                show_reserved;
   edi_grp_cl*         highlight_group; //group of the field shown in the bytes view
   edi_dynfld_t*       highlight_field;
   GtkLabel*           raw_caption;
   GtkWidget*          reserved_note;  //"N reserved fields are hidden"
   GSimpleAction*      show_reserved_action;
   GtkListBox*         overview_list;  //sidebar row above the groups
   GtkWidget*          overview_bin;
   AdwViewStackPage*   overview_page;
   bool                overview_shown;
   GtkListBox*         recent_list;    //recent files on the start page
   GtkWidget*          recent_group;
   GMenu*              recent_menu;    //Open Recent submenu
   gulong              recent_changed;
   GtkLabel*           reserved_label;
   u32_t               invalid_fields;
};

struct wxedid_timing {
   wxedid_wnd*   wnd;
   edi_grp_cl*   pgrp;
   edi_dynfld_t* fields[TIMING_FIELD_COUNT];
   GtkSpinButton* spins[TIMING_FIELD_COUNT];
   GtkWidget*    row_widgets[TIMING_FIELD_COUNT][4];
   GtkLabel*     derived[TIMING_FIELD_COUNT];
   GtkLabel*     clock_unit;
   GtkLabel*     clock_mhz;
   GtkSpinButton* refresh;
   GtkLabel*     htotal;
   GtkLabel*     hfreq;
   GtkLabel*     vtotal;
   GtkLabel*     modeline;
   GtkWidget*    drawing;
   GtkWidget*    summary;
   GtkWidget*    page;
   edid_timing_layout layout;
   double        pixel_hz_factor;
   bool          updating;
   bool          editing;
   timing_field  editing_field;
   size_t        history_index;
   wxc_String    before_text;
   u32_t         before_value;
};

constexpr char DRM_ROOT[] = "/sys/class/drm";

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

GType wxedid_item_get_type(void);

//------------
//field row widgets: what the user interacts with per field
enum {
   ROW_LABEL,     //read-only or not writable: label only
   ROW_ENTRY,     //text entry (OP_WRSTR)
   ROW_COMBO,     //value selector dropdown (F_VS)
   ROW_SWITCH,    //single bit (F_BIT)
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

enum open_mode {
   OPEN_FILE,
   OPEN_HEX,
   OPEN_DISPLAY,
};

// window.cpp
char* document_basename(const char* path);
void wnd_refresh_group_title(wxedid_wnd* wnd, edi_grp_cl* pgrp);
void wnd_update_document_ui(wxedid_wnd* wnd);
void wnd_update_header_controls(wxedid_wnd* wnd);
void wnd_show_error(wxedid_wnd* wnd, const char* message);
void wnd_clear_feedback(wxedid_wnd* wnd);
void wnd_log_error(wxedid_wnd* wnd, const char* format, ...) G_GNUC_PRINTF(2, 3);

// fields.cpp
void wnd_record_edit_history(wxedid_wnd* wnd, edi_grp_cl* group,
                             edi_dynfld_t* field, bool integer,
                             const wxc_String& session_before_text,
                             u32_t session_before_value,
                             const wxc_String& immediate_before_text,
                             u32_t immediate_before_value,
                             const wxc_String& after_text,
                             u32_t after_value, size_t* history_index);
void fields_refresh(GtkFlowBox* list, edi_grp_cl* pgrp, EDID_cl& EDID);
std::string field_help_summary(const char* description);
//re-read all rows of the field list into the widgets' current display
void rows_reload(GtkFlowBox* list, edi_grp_cl* pgrp, EDID_cl* pEDID,
                 wxedid_wnd* wnd);

// timing.cpp
GtkWidget* timing_create_page(wxedid_timing* timing);
bool timing_load_group(wxedid_timing* timing, edi_grp_cl* pgrp,
                       EDID_cl* pEDID);

// sidebar.cpp
gboolean tree_filter_match(gpointer object, gpointer user_data);
void tree_filter_items_changed(GListModel*, guint, guint, guint,
                               gpointer user_data);
void tree_search_changed(GtkSearchEntry* entry, gpointer user_data);
void tree_search_clear(GtkButton*, gpointer user_data);
void tree_name_setup(GtkSignalListItemFactory* /*factory*/,
                     GtkListItem* item, gpointer user_data);
void tree_name_bind(GtkSignalListItemFactory* /*factory*/,
                    GtkListItem* item, gpointer /*user_data*/);
void tree_name_unbind(GtkSignalListItemFactory* /*factory*/,
                      GtkListItem* item, gpointer /*user_data*/);
edi_grp_cl* wnd_selected_group(wxedid_wnd* wnd);
void wnd_update_group_actions(wxedid_wnd* wnd);
void wnd_refresh_raw_view(wxedid_wnd* wnd);
void wnd_refresh_selected_tree_label(wxedid_wnd* wnd);
void wnd_refresh_group_tree_label(wxedid_wnd* wnd, edi_grp_cl* group);
void wnd_on_tree_select(GtkSelectionModel* selmodel, guint /*position*/,
                        guint /*n_items*/, gpointer user_data);
void wnd_on_duplicate_group(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_on_move_group_up(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_on_move_group_down(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_on_delete_group(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_on_add_cta_group(GSimpleAction*, GVariant* parameter,
                          gpointer user_data);
void wnd_on_add_displayid_group(GSimpleAction*, GVariant*,
                                gpointer user_data);
void wnd_popup_group_menu(wxedid_wnd* wnd, double x, double y);
void wnd_on_tree_item_context(GtkGestureClick* gesture, int /*presses*/,
                              double x, double y, gpointer user_data);
gboolean wnd_on_tree_key(GtkEventControllerKey*, guint keyval,
                         guint /*keycode*/, GdkModifierType state,
                         gpointer user_data);
void wnd_rebuild_tree(wxedid_wnd* wnd, edi_grp_cl* select_group = NULL);

// history.cpp
void wnd_update_history_state(wxedid_wnd* wnd);
void wnd_clear_history(wxedid_wnd* wnd);
void wnd_record_history(wxedid_wnd* wnd, edi_grp_cl* group,
                        edi_dynfld_t* field, bool integer,
                        const wxc_String& before_text, u32_t before_value,
                        const wxc_String& after_text, u32_t after_value);
void wnd_record_structure(wxedid_wnd* wnd, history_kind kind,
                          edi_grp_cl* group, GroupAr_cl* array,
                          u32_t index, bool up, edi_grp_cl* parent);
void wnd_flush_refresh(wxedid_wnd* wnd);
void wnd_request_refresh(wxedid_wnd* wnd, edi_grp_cl* group,
                         edi_dynfld_t* field, bool type_changed,
                         bool now);
void wnd_schedule_refresh(wxedid_wnd* wnd);
void wnd_on_undo_action(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_on_redo_action(GSimpleAction*, GVariant*, gpointer user_data);

// overview.cpp
void wnd_refresh_overview(wxedid_wnd* wnd);
void wnd_show_overview(wxedid_wnd* wnd);
void wnd_on_overview_selected(GtkListBox*, GtkListBoxRow* row, gpointer user_data);
void wnd_present_log(wxedid_wnd* wnd);
void wnd_on_log_action(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_present_compare(wxedid_wnd* wnd, const char* path, bool hex);
void wnd_on_compare_file_action(GSimpleAction*, GVariant*, gpointer user_data);

// document.cpp
void wnd_refresh_recent(wxedid_wnd* wnd);
void wnd_on_recent_changed(GtkRecentManager*, gpointer user_data);
void wnd_on_open_recent(GSimpleAction*, GVariant* parameter, gpointer user_data);
void wnd_load_state(wxedid_wnd* wnd);
void wnd_save_state(wxedid_wnd* wnd);
bool path_is_hex_text(const char* path);
void wnd_load_file(wxedid_wnd* wnd, const char* path, bool hex);
void wnd_reload_source(wxedid_wnd* wnd);
GListStore* file_filters(const char* name, const char* const* patterns);
void wnd_request_open_source(wxedid_wnd* wnd, int requested, const char* path);
gboolean wnd_on_drop(GtkDropTarget*, const GValue* value, double, double,
                     gpointer user_data);
void wnd_on_open_action(GSimpleAction* /*action*/, GVariant* /*parameter*/,
                        gpointer user_data);
void wnd_on_import_hex_action(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_on_open_display_action(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_on_compare_display_action(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_on_save_action(GSimpleAction* /*action*/, GVariant* /*parameter*/,
                        gpointer user_data);
void wnd_on_save_as_action(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_on_export_hex_action(GSimpleAction*, GVariant*, gpointer user_data);
void wnd_on_save_report_action(GSimpleAction*, GVariant*, gpointer user_data);

#endif /* EDID_EDITOR_WINDOW_PRIVATE_H */
