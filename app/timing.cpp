/***************************************************************
 * Name:      timing.cpp
 * Purpose:   the visual editor of detailed timings
 * Copyright: sachesi (C) 2026
 * License:   GPLv3+
 **************************************************************/

#include "window_private.h"

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

   timing_set_text(timing->derived[TIMING_HACTIVE], "%.3f µs", hactive * pixel_us);
   timing_set_text(timing->derived[TIMING_HBLANK], "%.4f µs", hblank * pixel_us);
   timing_set_text(timing->derived[TIMING_HOFFSET], "%.4f µs",
                   timing_value(timing, TIMING_HOFFSET) * pixel_us);
   timing_set_text(timing->derived[TIMING_HWIDTH], "%.4f µs",
                   timing_value(timing, TIMING_HWIDTH) * pixel_us);
   timing_set_text(timing->derived[TIMING_VACTIVE], "%.3f ms", vactive * line_ms);
   timing_set_text(timing->derived[TIMING_VBLANK], "%.4f ms", vblank * line_ms);
   timing_set_text(timing->derived[TIMING_VOFFSET], "%.4f ms",
                   timing_value(timing, TIMING_VOFFSET) * line_ms);
   timing_set_text(timing->derived[TIMING_VWIDTH], "%.4f ms",
                   timing_value(timing, TIMING_VWIDTH) * line_ms);

   gtk_spin_button_set_value(timing->refresh, std::round(refresh * 100.0) / 100.0);
   char text[64];
   snprintf(text, sizeof(text), "%.3f MHz", pixclk / 1000000.0);
   gtk_label_set_text(timing->clock_mhz, text);
   snprintf(text, sizeof(text), "%u px  ·  %.3f µs", htotal,
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

//Blanking band of the diagram: its label goes inside when the band is thick
//enough, else beside it in the margin; a vertical band is labelled sideways.
struct timing_band {
   double x, y, w, h;         //the band
   double out_x, out_y;       //centre of the label in the margin
   bool   vertical;
   char   text[64];
   char   short_text[24];
};

static void timing_draw_band_label(cairo_t* cr, PangoLayout* layout,
                                   const timing_band& band) {
   const double thickness = band.vertical ? band.w : band.h;
   const double length = band.vertical ? band.h : band.w;
   if (length <= 0.0) return;
   int text_w = 0;
   int text_h = 0;
   pango_layout_set_text(layout, band.text, -1);
   pango_layout_get_pixel_size(layout, &text_w, &text_h);
   if (text_w > length - 8.0) {
      pango_layout_set_text(layout, band.short_text, -1);
      pango_layout_get_pixel_size(layout, &text_w, &text_h);
      if (text_w > length - 4.0) return;
   }
   const bool inside = thickness >= text_h + 4.0;
   const double cx = inside ? band.x + (band.w / 2.0) :
                     (band.vertical ? band.out_x : band.x + (band.w / 2.0));
   const double cy = inside ? band.y + (band.h / 2.0) :
                     (band.vertical ? band.y + (band.h / 2.0) : band.out_y);
   cairo_save(cr);
   cairo_translate(cr, cx, cy);
   if (band.vertical) cairo_rotate(cr, -G_PI / 2.0);
   cairo_move_to(cr, -text_w / 2.0, -text_h / 2.0);
   pango_cairo_show_layout(cr, layout);
   cairo_restore(cr);
}

static void timing_draw(GtkDrawingArea* area, cairo_t* cr, int width, int height,
                        gpointer user_data) {
   wxedid_timing* timing = static_cast<wxedid_timing*>(user_data);
   const u32_t hactive = timing_value(timing, TIMING_HACTIVE);
   const u32_t hblank = timing_value(timing, TIMING_HBLANK);
   const u32_t hoffset = timing_value(timing, TIMING_HOFFSET);
   const u32_t hwidth = timing_value(timing, TIMING_HWIDTH);
   const u32_t vactive = timing_value(timing, TIMING_VACTIVE);
   const u32_t vblank = timing_value(timing, TIMING_VBLANK);
   const u32_t voffset = timing_value(timing, TIMING_VOFFSET);
   const u32_t vwidth = timing_value(timing, TIMING_VWIDTH);
   const double htotal = hactive + hblank;
   const double vtotal = vactive + vblank;
   if ((htotal <= 0.0) || (vtotal <= 0.0)) return;

   //captions in a smaller size of the widget's font
   PangoLayout* layout = gtk_widget_create_pango_layout(GTK_WIDGET(area), NULL);
   PangoFontDescription* small = pango_font_description_copy(
      pango_context_get_font_description(gtk_widget_get_pango_context(GTK_WIDGET(area))));
   const gint size = static_cast<gint>(pango_font_description_get_size(small) * 0.85);
   if (pango_font_description_get_size_is_absolute(small)) {
      pango_font_description_set_absolute_size(small, size);
   } else {
      pango_font_description_set_size(small, size);
   }
   pango_layout_set_font_description(layout, small);
   pango_layout_set_text(layout, "0", -1);
   int caption_h = 0;
   pango_layout_get_pixel_size(layout, NULL, &caption_h);

   //the margins hold the labels of bands too thin to hold them
   const double pad = caption_h + 10.0;
   const double canvas_w = std::max(1.0, width - (2.0 * pad));
   const double canvas_h = std::max(1.0, height - (2.0 * pad));
   //sync and the back porch come before the active image, the front porch after
   const u32_t hback = (hblank > hoffset) ? hblank - hoffset : 0;
   const u32_t vback = (vblank > voffset) ? vblank - voffset : 0;
   const double active_x = pad + (hback / htotal) * canvas_w;
   const double active_y = pad + (vback / vtotal) * canvas_h;
   const double active_w = hactive / htotal * canvas_w;
   const double active_h = vactive / vtotal * canvas_h;
   const double hsync_w = std::max(1.0, hwidth / htotal * canvas_w);
   const double vsync_h = std::max(1.0, vwidth / vtotal * canvas_h);

   //blanking in the text color, sync and the active image in the accent color
   GdkRGBA color;
   gtk_widget_get_color(GTK_WIDGET(area), &color);
   GdkRGBA* accent = adw_style_manager_get_accent_color_rgba(adw_style_manager_get_default());
   cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.08);
   cairo_rectangle(cr, pad, pad, canvas_w, canvas_h);
   cairo_fill(cr);
   cairo_set_source_rgba(cr, accent->red, accent->green, accent->blue, 0.45);
   cairo_rectangle(cr, pad, pad, hsync_w, canvas_h);
   cairo_fill(cr);
   cairo_rectangle(cr, pad, pad, canvas_w, vsync_h);
   cairo_fill(cr);
   cairo_set_source_rgba(cr, accent->red, accent->green, accent->blue, 0.18);
   cairo_rectangle(cr, active_x, active_y, active_w, active_h);
   cairo_fill_preserve(cr);
   cairo_set_source_rgba(cr, accent->red, accent->green, accent->blue, 0.9);
   cairo_set_line_width(cr, 1.5);
   cairo_stroke(cr);
   gdk_rgba_free(accent);

   const u32_t hback_porch = (hback > hwidth) ? hback - hwidth : 0;
   const u32_t vback_porch = (vback > vwidth) ? vback - vwidth : 0;
   const double right = pad + canvas_w;
   const double bottom = pad + canvas_h;
   timing_band bands[4] = {
      {pad, pad, canvas_w, active_y - pad, 0.0, pad / 2.0, false, {}, {}},
      {pad, active_y + active_h, canvas_w, bottom - (active_y + active_h),
       0.0, bottom + (pad / 2.0), false, {}, {}},
      {pad, pad, active_x - pad, canvas_h, pad / 2.0, 0.0, true, {}, {}},
      {active_x + active_w, pad, right - (active_x + active_w), canvas_h,
       right + (pad / 2.0), 0.0, true, {}, {}},
   };
   snprintf(bands[0].text, sizeof(bands[0].text), "Sync %u + back porch %u lines",
            vwidth, vback_porch);
   snprintf(bands[0].short_text, sizeof(bands[0].short_text), "%u lines", vback);
   snprintf(bands[1].text, sizeof(bands[1].text), "Sync offset %u lines", voffset);
   snprintf(bands[1].short_text, sizeof(bands[1].short_text), "%u lines", voffset);
   snprintf(bands[2].text, sizeof(bands[2].text), "Sync %u + back porch %u px",
            hwidth, hback_porch);
   snprintf(bands[2].short_text, sizeof(bands[2].short_text), "%u px", hback);
   snprintf(bands[3].text, sizeof(bands[3].text), "Sync offset %u px", hoffset);
   snprintf(bands[3].short_text, sizeof(bands[3].short_text), "%u px", hoffset);
   const u32_t sizes[4] = {vback, voffset, hback, hoffset};
   cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha * 0.7);
   for (int idx = 0; idx < 4; idx++) {
      if (sizes[idx] > 0) timing_draw_band_label(cr, layout, bands[idx]);
   }

   //the active image: its size, and the whole frame below it
   char active_text[48];
   snprintf(active_text, sizeof(active_text), "%u × %u", hactive, vactive);
   char total_text[64];
   snprintf(total_text, sizeof(total_text), "Total %.0f × %.0f", htotal, vtotal);
   int total_w = 0;
   int total_h = 0;
   pango_layout_set_text(layout, total_text, -1);
   pango_layout_get_pixel_size(layout, &total_w, &total_h);
   PangoLayout* bold = gtk_widget_create_pango_layout(GTK_WIDGET(area), active_text);
   PangoFontDescription* font = pango_font_description_new();
   pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
   pango_layout_set_font_description(bold, font);
   int text_w = 0;
   int text_h = 0;
   pango_layout_get_pixel_size(bold, &text_w, &text_h);
   const bool with_total = (active_h >= text_h + total_h + 12.0) &&
                           (active_w >= total_w + 8.0);
   const double block_h = with_total ? text_h + 2.0 + total_h : text_h;
   const double block_y = active_y + ((active_h - block_h) / 2.0);
   cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
   cairo_move_to(cr, active_x + ((active_w - text_w) / 2.0), block_y);
   pango_cairo_show_layout(cr, bold);
   if (with_total) {
      cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha * 0.7);
      cairo_move_to(cr, active_x + ((active_w - total_w) / 2.0), block_y + text_h + 2.0);
      pango_cairo_show_layout(cr, layout);
   }
   pango_font_description_free(font);
   pango_font_description_free(small);
   g_object_unref(bold);
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

static void timing_on_refresh_changed(GtkSpinButton* spin, gpointer user_data) {
   wxedid_timing* timing = static_cast<wxedid_timing*>(user_data);
   if (timing->updating || (timing->pgrp == NULL)) return;

   const double htotal = timing_value(timing, TIMING_HACTIVE) +
                         timing_value(timing, TIMING_HBLANK);
   const double vtotal = timing_value(timing, TIMING_VACTIVE) +
                         timing_value(timing, TIMING_VBLANK);
   if ((htotal <= 0.0) || (vtotal <= 0.0)) return;
   const double current = timing_value(timing, TIMING_PIXCLK) *
                          timing->pixel_hz_factor / (htotal * vtotal);
   const double target = gtk_spin_button_get_value(spin);
   //the rate is shown rounded, so leaving the field must not move the clock
   if (std::fabs(target - current) < 0.005) return;

   gtk_spin_button_set_value(timing->spins[TIMING_PIXCLK],
                             edid_timing_clock_for(timing->layout, target, htotal, vtotal));
   //show the rate the clock reaches, also when it did not change
   timing->updating = true;
   timing_update_outputs(timing);
   timing->updating = false;
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
   gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
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
   gtk_widget_set_margin_start(derived, 6);
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
   gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
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
   gtk_widget_set_margin_start(grid, 12);
   gtk_widget_set_margin_end(grid, 12);
   gtk_widget_set_margin_bottom(grid, 12);
   gtk_box_append(GTK_BOX(card), grid);
   *grid_out = GTK_GRID(grid);
   return card;
}

GtkWidget* timing_create_page(wxedid_timing* timing) {
   GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

   GtkWidget* summary = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
   timing->summary = summary;
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
   timing->clock_mhz = GTK_LABEL(gtk_label_new(NULL));
   gtk_label_set_xalign(timing->clock_mhz, 0.0);
   gtk_widget_add_css_class(GTK_WIDGET(timing->clock_mhz), "caption");
   gtk_widget_add_css_class(GTK_WIDGET(timing->clock_mhz), "dim-label");
   gtk_widget_add_css_class(GTK_WIDGET(timing->clock_mhz), "timing-derived");
   gtk_box_append(GTK_BOX(clock_box), GTK_WIDGET(timing->clock_mhz));
   gtk_box_append(GTK_BOX(summary), clock_box);

   GtkWidget* refresh_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
   gtk_widget_set_margin_start(refresh_box, 12);
   gtk_widget_set_margin_end(refresh_box, 12);
   gtk_widget_set_margin_top(refresh_box, 12);
   gtk_widget_set_margin_bottom(refresh_box, 12);
   GtkWidget* refresh_title = gtk_label_new("Vertical refresh");
   gtk_label_set_xalign(GTK_LABEL(refresh_title), 0.0);
   gtk_widget_add_css_class(refresh_title, "caption");
   gtk_widget_add_css_class(refresh_title, "dim-label");
   gtk_box_append(GTK_BOX(refresh_box), refresh_title);
   //a new rate is reached through the pixel clock, keeping the blanking
   GtkWidget* refresh_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   GtkAdjustment* refresh_adj = gtk_adjustment_new(1, 1, 10000, 1, 10, 0);
   GtkWidget* refresh_spin = gtk_spin_button_new(refresh_adj, 1, 2);
   timing->refresh = GTK_SPIN_BUTTON(refresh_spin);
   g_object_set_data(G_OBJECT(refresh_spin), "timing-field",
                     GINT_TO_POINTER(TIMING_PIXCLK));
   g_signal_connect(refresh_spin, "value-changed",
                    G_CALLBACK(timing_on_refresh_changed), timing);
   timing_add_focus_controller(timing, refresh_spin);
   gtk_accessible_update_property(GTK_ACCESSIBLE(refresh_spin),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, "Vertical refresh", -1);
   gtk_widget_set_tooltip_text(refresh_spin,
      "A new refresh rate changes the pixel clock; the blanking stays as it is");
   gtk_box_append(GTK_BOX(refresh_row), refresh_spin);
   GtkWidget* refresh_unit = gtk_label_new("Hz");
   gtk_widget_add_css_class(refresh_unit, "dim-label");
   gtk_box_append(GTK_BOX(refresh_row), refresh_unit);
   gtk_box_append(GTK_BOX(refresh_box), refresh_row);
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

   //a plain stack: the cards sit directly on the page like the others
   GtkWidget* sections = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
   GtkGrid* horizontal = NULL;
   GtkWidget* horizontal_card = timing_section("Horizontal timing", &horizontal);
   timing_add_edit_row(timing, horizontal, 0, TIMING_HACTIVE, "Active", "px");
   timing_add_edit_row(timing, horizontal, 1, TIMING_HBORDER, "Border", "px");
   timing_add_edit_row(timing, horizontal, 2, TIMING_HBLANK, "Blanking", "px");
   timing_add_edit_row(timing, horizontal, 3, TIMING_HOFFSET, "Sync offset", "px");
   timing_add_edit_row(timing, horizontal, 4, TIMING_HWIDTH, "Sync width", "px");
   timing_add_value_row(horizontal, 5, "Total", &timing->htotal);
   timing_add_value_row(horizontal, 6, "Frequency", &timing->hfreq);
   gtk_box_append(GTK_BOX(sections), horizontal_card);

   GtkGrid* vertical = NULL;
   GtkWidget* vertical_card = timing_section("Vertical timing", &vertical);
   timing_add_edit_row(timing, vertical, 0, TIMING_VACTIVE, "Active", "lines");
   timing_add_edit_row(timing, vertical, 1, TIMING_VBORDER, "Border", "lines");
   timing_add_edit_row(timing, vertical, 2, TIMING_VBLANK, "Blanking", "lines");
   timing_add_edit_row(timing, vertical, 3, TIMING_VOFFSET, "Sync offset", "lines");
   timing_add_edit_row(timing, vertical, 4, TIMING_VWIDTH, "Sync width", "lines");
   timing_add_value_row(vertical, 5, "Total", &timing->vtotal);
   gtk_box_append(GTK_BOX(sections), vertical_card);
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
   gtk_widget_add_css_class(GTK_WIDGET(timing->modeline), "modeline");
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

bool timing_load_group(wxedid_timing* timing, edi_grp_cl* pgrp,
                       EDID_cl* pEDID) {
   if (pgrp == NULL) {
      timing->pgrp = NULL;
      return false;
   }

   edid_timing_layout& layout = timing->layout;
   if (! edid_timing_layout_of(pgrp, layout)) {
      timing->pgrp = NULL;
      return false;
   }
   const int* field_indices = layout.fields;
   timing->pixel_hz_factor = layout.pixel_hz;
   gtk_label_set_text(timing->clock_unit, (layout.pixel_hz == 10000.0) ? "×10 kHz" : "kHz");

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
      double maximum = field->field.maxv;
      double step = 1;
      if (idx == TIMING_PIXCLK) {
         minimum = 1;
         step = layout.clock_step;
         if (layout.clock_max > 0.0) maximum = layout.clock_max;
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
