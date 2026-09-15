/***************************************************************
 * Name:      history.cpp
 * Purpose:   undo and redo, and rebuilding groups after field edits
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include "window_private.h"

void wnd_update_history_state(wxedid_wnd* wnd) {
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

void wnd_clear_history(wxedid_wnd* wnd) {
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

void wnd_record_history(wxedid_wnd* wnd, edi_grp_cl* group,
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

void wnd_record_structure(wxedid_wnd* wnd, history_kind kind,
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
   if (entry.kind == HISTORY_DATA) {
      const std::string& data = redo ? entry.after_text : entry.before_text;
      memcpy(entry.group->getInstPtr(), data.data(), data.size());
      return entry.group;
   }
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
         wnd_log_error(wnd, _("Couldn’t restore the previous structure. Reopen the file before "
                              "editing again."));
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
      wnd_log_error(wnd, _("Couldn’t restore the previous value. Reopen the file before editing again."));
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

static void wnd_apply_history(wxedid_wnd* wnd, bool redo) {
   wnd_flush_refresh(wnd);
   if (! wnd_apply_history_step(wnd, redo)) return;
   //joined entries, as a rebuild and the field write behind it, go together
   if (redo) {
      while ((wnd->history_position < wnd->history.size()) &&
             wnd->history[wnd->history_position].joined &&
             wnd_apply_history_step(wnd, true)) {}
   } else {
      while ((wnd->history_position < wnd->history.size()) &&
             wnd->history[wnd->history_position].joined &&
             wnd_apply_history_step(wnd, false)) {}
   }
}

//changes to the data of several groups, undone and redone as one
void wnd_apply_changes(wxedid_wnd* wnd, const std::vector<edid_data_change>& changes,
                       edi_grp_cl* selection) {
   if (changes.empty()) return;
   edid_apply_changes(changes, true);
   for (size_t idx=0; idx<changes.size(); idx++) {
      wxedid_history_entry entry = {};
      entry.kind = HISTORY_DATA;
      entry.group = changes[idx].group;
      entry.before_text.assign(changes[idx].before.begin(), changes[idx].before.end());
      entry.after_text.assign(changes[idx].after.begin(), changes[idx].after.end());
      entry.joined = idx > 0;
      wnd_push_history(wnd, entry);
   }
   bool overview = wnd->overview_shown;
   wnd->invalid_fields = 0;
   wnd_rebuild_tree(wnd, selection);
   if (overview) wnd_show_overview(wnd);
   wnd_update_history_state(wnd);
   wnd_update_document_ui(wnd);
}

//------------
// group rebuilds: a field write can change the group type or layout
void wnd_flush_refresh(wxedid_wnd* wnd) {
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
      refused = _("This change is not possible here. The previous value was restored.");
   } else if ((rebuilt != NULL) && ! EDID_cl::ReplaceGroup(target, rebuilt)) {
      delete rebuilt;
      refused = _("This change does not fit in the block. The previous value was restored.");
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
void wnd_request_refresh(wxedid_wnd* wnd, edi_grp_cl* group,
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

void wnd_schedule_refresh(wxedid_wnd* wnd) {
   if ((wnd->refresh_group != NULL) && (wnd->refresh_source == 0)) {
      wnd->refresh_source = g_idle_add(wnd_on_refresh_idle, wnd);
   }
}

void wnd_on_undo_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_apply_history(static_cast<wxedid_wnd*>(user_data), false);
}

void wnd_on_redo_action(GSimpleAction*, GVariant*, gpointer user_data) {
   wnd_apply_history(static_cast<wxedid_wnd*>(user_data), true);
}
