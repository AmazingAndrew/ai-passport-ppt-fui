// main/ui_fui_ppt.c — FUI layout for the PPT controller.
//
// All widgets are owned by this file. The view set is:
//   - HOME  : top bar, main panel (slide flow + timer + action feedback),
//             link panel (BLE status + RSSI), hint bar
//   - MENU  : two rows: reset pairing, about
//   - DIALOG: confirm reset pairing
//
// Layout math is extracted into ui_fui_ppt_home_layout() and
// ui_fui_ppt_menu_row_rect() so the host test can validate bounds without
// pulling in LVGL. The LVGL-only paths are wrapped in
// `#ifdef LVGL_H_INCLUDE_SIMPLE` so the host test compiles without LVGL.

#include "ui_fui_ppt.h"

#include <string.h>

#ifndef LVGL_H_INCLUDE_SIMPLE
/* LVGL is present; declare the embedded fonts. */
LV_FONT_DECLARE(ui_font_kode_regular_11);
LV_FONT_DECLARE(ui_font_kode_regular_13);
LV_FONT_DECLARE(ui_font_kode_bold_13);
LV_FONT_DECLARE(ui_font_kode_bold_15);
LV_FONT_DECLARE(ui_font_kode_bold_21);
#endif

typedef struct ui_fui_ppt_s {
    lv_obj_t *screen;
    lv_obj_t *home_layer;
    lv_obj_t *menu_layer;
    lv_obj_t *dialog_layer;

    /* home */
    lv_obj_t *title_label;
    lv_obj_t *code_top;
    lv_obj_t *code_bottom;
    lv_obj_t *battery_segments[5];
    lv_obj_t *state_label;        /* big state word */
    lv_obj_t *action_label;       /* last-action feedback */
    lv_obj_t *timer_label;        /* MM:SS or HH:MM:SS */
    lv_obj_t *prev_arrow;
    lv_obj_t *next_arrow;
    lv_obj_t *link_dot;
    lv_obj_t *link_value;
    lv_obj_t *link_chart;
    lv_chart_series_t *link_series;
    lv_obj_t *hint_label;

    /* menu */
    lv_obj_t *menu_rows[2];
    lv_obj_t *menu_labels[2];
    lv_obj_t *menu_codes[2];

    /* dialog */
    lv_obj_t *dialog_scrim;
    lv_obj_t *dialog_title;
    lv_obj_t *dialog_confirm;
    lv_obj_t *dialog_cancel;
    bool dialog_focus_confirm;

    ui_fui_ppt_view_t visible;
} ui_fui_ppt_t;

/* ============================================================ */
/* Layout math — kept outside LVGL for host testing.            */
/* ============================================================ */

ui_fui_ppt_layout_rect_t ui_fui_ppt_menu_row_rect(unsigned index) {
    /* Each menu row is 60 px tall, stacked from y=52, 6 px outer margin. */
    ui_fui_ppt_layout_rect_t r = {
        .x = 8, .y = 52 + (int)index * 62, .w = 224, .h = 56,
    };
    return r;
}

ui_fui_ppt_layout_t ui_fui_ppt_home_layout(void) {
    ui_fui_ppt_layout_t l = {
        .top_bar    = { .x = 0,   .y = 0,   .w = 240, .h = 44 },
        .main_panel = { .x = 8,   .y = 50,  .w = 224, .h = 160 },
        .link_panel = { .x = 8,   .y = 218, .w = 224, .h = 64 },
        .hint_bar   = { .x = 0,   .y = 296, .w = 240, .h = 24 },
    };
    return l;
}

#ifdef LVGL_H_INCLUDE_SIMPLE
/* ============================================================ */
/* Helpers                                                       */
/* ============================================================ */

static lv_obj_t *mk_box(lv_obj_t *parent, int x, int y, int w, int h,
                        uint32_t bg, lv_opa_t opa) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    return o;
}

static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, int x, int y,
                          const lv_font_t *font, uint32_t color) {
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, txt);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    return o;
}

static uint32_t action_color(ui_fui_ppt_action_t a) {
    switch (a) {
        case UI_FUI_PPT_ACTION_PREV:
        case UI_FUI_PPT_ACTION_NEXT:   return UI_FUI_PPT_TEAL;
        case UI_FUI_PPT_ACTION_START:  return UI_FUI_PPT_ORANGE;
        case UI_FUI_PPT_ACTION_EXIT:   return UI_FUI_PPT_AMBER;
        case UI_FUI_PPT_ACTION_RESET_ARM:
        case UI_FUI_PPT_ACTION_BT_NOT_READY: return UI_FUI_PPT_AMBER;
        case UI_FUI_PPT_ACTION_RESETTING:
        case UI_FUI_PPT_ACTION_BLE_INIT_FAIL: return UI_FUI_PPT_RED;
        case UI_FUI_PPT_ACTION_READY:
        default:                       return UI_FUI_PPT_MUTED;
    }
}

static const char *action_text(ui_fui_ppt_action_t a) {
    switch (a) {
        case UI_FUI_PPT_ACTION_PREV:           return "<< PREV";
        case UI_FUI_PPT_ACTION_NEXT:           return "NEXT >>";
        case UI_FUI_PPT_ACTION_START:          return "START SHOW";
        case UI_FUI_PPT_ACTION_EXIT:           return "EXIT SHOW";
        case UI_FUI_PPT_ACTION_RESET_ARM:      return "RESET ARM";
        case UI_FUI_PPT_ACTION_RESETTING:      return "RESETTING...";
        case UI_FUI_PPT_ACTION_BLE_INIT_FAIL:  return "BLE INIT FAIL";
        case UI_FUI_PPT_ACTION_BT_NOT_READY:   return "BT NOT READY";
        case UI_FUI_PPT_ACTION_READY:
        default:                               return "READY";
    }
}

/* ============================================================ */
/* Construction                                                  */
/* ============================================================ */

ui_fui_ppt_t *ui_fui_ppt_create(void) {
    ui_fui_ppt_t *ui = lv_mem_alloc(sizeof *ui);
    if (!ui) return NULL;
    memset(ui, 0, sizeof *ui);
    ui->visible = UI_FUI_PPT_VIEW_HOME;

    ui->screen = lv_obj_create(NULL);
    lv_obj_remove_flag(ui->screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui->screen, 0, 0);
    lv_obj_set_style_border_width(ui->screen, 0, 0);
    lv_obj_set_style_bg_color(ui->screen, lv_color_hex(UI_FUI_PPT_BG), 0);

    /* ---- Home ---- */
    ui->home_layer = mk_box(ui->screen, 0, 0, 240, 320,
                            UI_FUI_PPT_BG, LV_OPA_COVER);

    /* top bar */
    ui_fui_ppt_layout_t l = ui_fui_ppt_home_layout();
    mk_box(ui->screen, l.top_bar.x, l.top_bar.y,
           l.top_bar.w, l.top_bar.h, UI_FUI_PPT_PANEL, LV_OPA_COVER);
    mk_box(ui->screen, 0, 41, 52, 2, UI_FUI_PPT_ORANGE, LV_OPA_COVER);
    mk_box(ui->screen, 56, 41, 184, 2, UI_FUI_PPT_CREAM, LV_OPA_80);
    ui->code_top = mk_box(ui->screen, 8, 5, 31, 15,
                          UI_FUI_PPT_RUST, LV_OPA_COVER);
    ui->code_bottom = mk_box(ui->screen, 8, 21, 31, 15,
                             UI_FUI_PPT_ORANGE, LV_OPA_COVER);
    mk_label(ui->code_top, "02", 0, 0,
             &ui_font_kode_bold_13, UI_FUI_PPT_CREAM);
    lv_obj_center(lv_obj_get_child(ui->code_top, 0));
    mk_label(ui->code_bottom, "BT", 0, 0,
             &ui_font_kode_bold_13, UI_FUI_PPT_CREAM);
    lv_obj_center(lv_obj_get_child(ui->code_bottom, 0));

    ui->title_label = mk_label(ui->screen, "PPT CTRL", 49, 5,
                              &ui_font_kode_bold_15, UI_FUI_PPT_TEXT);
    mk_label(ui->screen, "BLE HID REMOTE", 49, 23,
             &ui_font_kode_regular_11, UI_FUI_PPT_MUTED);

    /* battery segments — 5 cells, right-aligned in top bar */
    const int batt_total_w = 5 * 9 + 4 * 3;
    for (unsigned i = 0; i < 5; i++) {
        int x = 240 - 8 - batt_total_w + (int)i * (9 + 3);
        ui->battery_segments[i] = mk_box(ui->screen, x, 16, 9, 9,
                                          UI_FUI_PPT_MUTED, LV_OPA_40);
    }

    /* main panel */
    mk_box(ui->screen, l.main_panel.x, l.main_panel.y,
           l.main_panel.w, l.main_panel.h,
           UI_FUI_PPT_PANEL, LV_OPA_COVER);
    mk_box(ui->screen, l.main_panel.x, l.main_panel.y + 22, 2, 134,
           UI_FUI_PPT_RUST, LV_OPA_COVER);
    mk_box(ui->screen, l.main_panel.x + 4, l.main_panel.y + 22, 1, 134,
           UI_FUI_PPT_ORANGE, LV_OPA_70);
    mk_label(ui->screen, "SLIDE FLOW", l.main_panel.x + 12,
             l.main_panel.y + 4, &ui_font_kode_regular_11,
             UI_FUI_PPT_MUTED);
    mk_label(ui->screen, "MOTION SIGNATURE", l.main_panel.x + 12,
             l.main_panel.y + 14, &ui_font_kode_regular_11,
             UI_FUI_PPT_MUTED);

    ui->prev_arrow = mk_label(ui->screen, "<", l.main_panel.x + 12,
                              l.main_panel.y + 40, &ui_font_kode_bold_21,
                              UI_FUI_PPT_ORANGE);
    ui->timer_label = mk_label(ui->screen, "00:00",
                                l.main_panel.x + 60, l.main_panel.y + 44,
                                &ui_font_kode_bold_21, UI_FUI_PPT_TEAL);
    lv_obj_set_width(ui->timer_label, 100);
    lv_obj_set_style_text_align(ui->timer_label, LV_TEXT_ALIGN_CENTER, 0);
    ui->next_arrow = mk_label(ui->screen, ">",
                              l.main_panel.x + l.main_panel.w - 24,
                              l.main_panel.y + 40, &ui_font_kode_bold_21,
                              UI_FUI_PPT_ORANGE);

    ui->action_label = mk_label(ui->screen, "READY",
                                l.main_panel.x + 12, l.main_panel.y + 84,
                                &ui_font_kode_bold_15, UI_FUI_PPT_MUTED);
    lv_obj_set_width(ui->action_label, l.main_panel.w - 24);
    lv_obj_set_style_text_align(ui->action_label, LV_TEXT_ALIGN_CENTER, 0);

    ui->state_label = mk_label(ui->screen, "PAIR with PPT-Remote",
                                l.main_panel.x + 12,
                                l.main_panel.y + 110,
                                &ui_font_kode_bold_15, UI_FUI_PPT_MAGENTA);
    lv_obj_set_width(ui->state_label, l.main_panel.w - 24);
    lv_obj_set_style_text_align(ui->state_label, LV_TEXT_ALIGN_CENTER, 0);

    /* link panel */
    mk_box(ui->screen, l.link_panel.x, l.link_panel.y,
           l.link_panel.w, l.link_panel.h,
           UI_FUI_PPT_PANEL_ALT, LV_OPA_COVER);
    mk_label(ui->screen, "HOST LINK", l.link_panel.x + 12,
             l.link_panel.y + 4, &ui_font_kode_regular_11,
             UI_FUI_PPT_MUTED);
    mk_label(ui->screen, "RSSI", l.link_panel.x + 12,
             l.link_panel.y + 14, &ui_font_kode_regular_11,
             UI_FUI_PPT_MUTED);

    ui->link_dot = mk_box(ui->screen, l.link_panel.x + 80,
                          l.link_panel.y + 10, 6, 6,
                          UI_FUI_PPT_MAGENTA, LV_OPA_COVER);
    lv_obj_set_style_radius(ui->link_dot, LV_RADIUS_CIRCLE, 0);
    ui->link_value = mk_label(ui->screen, "PAIRING",
                              l.link_panel.x + 92, l.link_panel.y + 6,
                              &ui_font_kode_bold_13, UI_FUI_PPT_MAGENTA);

    ui->link_chart = lv_chart_create(ui->screen);
    lv_obj_remove_flag(ui->link_chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(ui->link_chart, l.link_panel.x + 80,
                   l.link_panel.y + 26);
    lv_obj_set_size(ui->link_chart, 120, 28);
    lv_obj_set_style_pad_all(ui->link_chart, 0, 0);
    lv_obj_set_style_radius(ui->link_chart, 0, 0);
    lv_obj_set_style_bg_color(ui->link_chart,
                              lv_color_hex(UI_FUI_PPT_RUST), 0);
    lv_obj_set_style_bg_opa(ui->link_chart, LV_OPA_30, 0);
    lv_obj_set_style_border_width(ui->link_chart, 0, 0);
    lv_obj_set_style_line_opa(ui->link_chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->link_chart, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(ui->link_chart, 0, LV_PART_ITEMS);
    lv_chart_set_type(ui->link_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(ui->link_chart, 8);
    lv_chart_set_axis_range(ui->link_chart, LV_CHART_AXIS_PRIMARY_Y,
                            -100, -30);
    lv_chart_set_div_line_count(ui->link_chart, 0, 0);
    ui->link_series = lv_chart_add_series(ui->link_chart,
                                          lv_color_hex(UI_FUI_PPT_TEAL),
                                          LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(ui->link_chart, ui->link_series,
                            LV_CHART_POINT_NONE);

    /* hint bar */
    mk_box(ui->screen, l.hint_bar.x, l.hint_bar.y,
           l.hint_bar.w, 1, UI_FUI_PPT_RUST, LV_OPA_COVER);
    ui->hint_label = mk_label(ui->screen,
                              "UP PREV | DOWN NEXT | OK START",
                              0, l.hint_bar.y + 4,
                              &ui_font_kode_regular_11, UI_FUI_PPT_MUTED);
    lv_obj_set_width(ui->hint_label, 240);
    lv_obj_set_style_text_align(ui->hint_label, LV_TEXT_ALIGN_CENTER, 0);

    /* ---- Menu layer (hidden by default) ---- */
    ui->menu_layer = mk_box(ui->screen, 0, 44, 240, 252,
                            UI_FUI_PPT_BG, LV_OPA_COVER);
    lv_obj_add_flag(ui->menu_layer, LV_OBJ_FLAG_HIDDEN);

    const char *names[2] = { "RESET PAIRING", "ABOUT" };
    const char *codes[2] = { "RP", "AB" };
    for (unsigned i = 0; i < 2; i++) {
        ui_fui_ppt_layout_rect_t r = ui_fui_ppt_menu_row_rect(i);
        ui->menu_rows[i] = mk_box(ui->menu_layer, r.x, r.y, r.w, r.h,
                                   UI_FUI_PPT_PANEL, LV_OPA_COVER);
        ui->menu_codes[i] = mk_box(ui->menu_rows[i], 0, 0, 38, r.h,
                                    UI_FUI_PPT_RUST, LV_OPA_COVER);
        mk_label(ui->menu_codes[i], codes[i], 8, 18,
                 &ui_font_kode_bold_13, UI_FUI_PPT_CREAM);
        ui->menu_labels[i] = mk_label(ui->menu_rows[i], names[i], 50, 18,
                                      &ui_font_kode_bold_15,
                                      i == 0 ? UI_FUI_PPT_RED
                                              : UI_FUI_PPT_CREAM);
    }

    /* ---- Dialog layer (hidden) ---- */
    ui->dialog_layer = mk_box(ui->screen, 0, 44, 240, 252,
                              UI_FUI_PPT_BG, LV_OPA_70);
    lv_obj_add_flag(ui->dialog_layer, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *dialog = mk_box(ui->dialog_layer, 20, 70, 200, 120,
                              UI_FUI_PPT_PANEL_ALT, LV_OPA_COVER);
    mk_box(dialog, 0, 0, 4, 116, UI_FUI_PPT_ORANGE, LV_OPA_COVER);
    mk_label(dialog, "04 / SECURE ACTION", 12, 6,
             &ui_font_kode_regular_11, UI_FUI_PPT_MUTED);
    ui->dialog_title = mk_label(dialog, "RESET PAIRING?", 12, 28,
                                &ui_font_kode_bold_15, UI_FUI_PPT_ORANGE);
    lv_obj_set_width(ui->dialog_title, 176);
    lv_obj_set_style_text_align(ui->dialog_title,
                                LV_TEXT_ALIGN_CENTER, 0);
    ui->dialog_cancel = mk_box(dialog, 12, 80, 80, 28,
                               UI_FUI_PPT_PANEL, LV_OPA_COVER);
    ui->dialog_confirm = mk_box(dialog, 108, 80, 80, 28,
                                UI_FUI_PPT_PANEL, LV_OPA_COVER);
    mk_label(ui->dialog_cancel, "CANCEL", 12, 6,
             &ui_font_kode_bold_13, UI_FUI_PPT_CREAM);
    lv_obj_set_width(lv_obj_get_child(ui->dialog_cancel, 0), 80);
    lv_obj_set_style_text_align(lv_obj_get_child(ui->dialog_cancel, 0),
                                LV_TEXT_ALIGN_CENTER, 0);
    mk_label(ui->dialog_confirm, "CONFIRM", 12, 6,
             &ui_font_kode_bold_13, UI_FUI_PPT_RED);
    lv_obj_set_width(lv_obj_get_child(ui->dialog_confirm, 0), 80);
    lv_obj_set_style_text_align(lv_obj_get_child(ui->dialog_confirm, 0),
                                LV_TEXT_ALIGN_CENTER, 0);
    ui->dialog_focus_confirm = false;

    lv_screen_load(ui->screen);
    return ui;
}

void ui_fui_ppt_destroy(ui_fui_ppt_t *ui) {
    if (!ui) return;
    if (ui->screen) lv_obj_delete(ui->screen);
    lv_mem_free(ui);
}

/* ============================================================ */
/* Setters                                                       */
/* ============================================================ */

void ui_fui_ppt_set_action(ui_fui_ppt_t *ui, ui_fui_ppt_action_t action) {
    if (!ui || !ui->action_label) return;
    lv_label_set_text(ui->action_label, action_text(action));
    lv_obj_set_style_text_color(ui->action_label,
                                lv_color_hex(action_color(action)), 0);
}

void ui_fui_ppt_set_state(ui_fui_ppt_t *ui, const char *state) {
    if (!ui || !ui->state_label || !state) return;
    lv_label_set_text(ui->state_label, state);
}

void ui_fui_ppt_set_state_color(ui_fui_ppt_t *ui, uint32_t color) {
    if (!ui || !ui->state_label) return;
    lv_obj_set_style_text_color(ui->state_label, lv_color_hex(color), 0);
}

void ui_fui_ppt_set_timer(ui_fui_ppt_t *ui, const char *timer) {
    if (!ui || !ui->timer_label || !timer) return;
    lv_label_set_text(ui->timer_label, timer);
}

void ui_fui_ppt_set_link(ui_fui_ppt_t *ui, const char *text, uint32_t color) {
    if (!ui || !ui->link_value || !text) return;
    lv_label_set_text(ui->link_value, text);
    lv_obj_set_style_text_color(ui->link_value, lv_color_hex(color), 0);
    if (ui->link_dot) {
        lv_obj_set_style_bg_color(ui->link_dot, lv_color_hex(color), 0);
    }
}

void ui_fui_ppt_set_battery(ui_fui_ppt_t *ui, int soc) {
    if (!ui) return;
    if (soc < 0 || soc > 100) {
        for (unsigned i = 0; i < 5; i++) {
            lv_obj_set_style_bg_color(ui->battery_segments[i],
                                      lv_color_hex(UI_FUI_PPT_MUTED), 0);
            lv_obj_set_style_bg_opa(ui->battery_segments[i], LV_OPA_40, 0);
        }
        return;
    }
    uint32_t color = (soc < 10) ? UI_FUI_PPT_RED
                    : (soc < 20) ? UI_FUI_PPT_AMBER
                                 : UI_FUI_PPT_TEAL;
    unsigned filled = soc > 0
        ? ((unsigned)soc * 5u + 99u) / 100u : 1u;
    for (unsigned i = 0; i < 5; i++) {
        bool on = i < filled;
        lv_obj_set_style_bg_color(ui->battery_segments[i],
            lv_color_hex(on ? color : UI_FUI_PPT_RUST), 0);
        lv_obj_set_style_bg_opa(ui->battery_segments[i],
            on ? LV_OPA_COVER : LV_OPA_40, 0);
    }
}

void ui_fui_ppt_set_visible(ui_fui_ppt_t *ui, ui_fui_ppt_view_t view) {
    if (!ui) return;
    ui->visible = view;
    bool home_visible  = (view == UI_FUI_PPT_VIEW_HOME);
    bool menu_visible  = (view == UI_FUI_PPT_VIEW_MENU);
    bool dialog_visible = (view == UI_FUI_PPT_VIEW_DIALOG);
    if (ui->home_layer) {
        if (home_visible)  lv_obj_remove_flag(ui->home_layer,
                                               LV_OBJ_FLAG_HIDDEN);
        else                lv_obj_add_flag(ui->home_layer,
                                            LV_OBJ_FLAG_HIDDEN);
    }
    if (ui->menu_layer) {
        if (menu_visible)  lv_obj_remove_flag(ui->menu_layer,
                                               LV_OBJ_FLAG_HIDDEN);
        else                lv_obj_add_flag(ui->menu_layer,
                                            LV_OBJ_FLAG_HIDDEN);
    }
    if (ui->dialog_layer) {
        if (dialog_visible) lv_obj_remove_flag(ui->dialog_layer,
                                               LV_OBJ_FLAG_HIDDEN);
        else                lv_obj_add_flag(ui->dialog_layer,
                                            LV_OBJ_FLAG_HIDDEN);
    }
}

#else
/* ============================================================ */
/* Host stubs so the test compiles without LVGL.                 */
/* ============================================================ */

ui_fui_ppt_t *ui_fui_ppt_create(void) { return NULL; }
void           ui_fui_ppt_destroy(ui_fui_ppt_t *ui) { (void)ui; }
void           ui_fui_ppt_set_action(ui_fui_ppt_t *ui, ui_fui_ppt_action_t a) { (void)ui; (void)a; }
void           ui_fui_ppt_set_state(ui_fui_ppt_t *ui, const char *s) { (void)ui; (void)s; }
void           ui_fui_ppt_set_state_color(ui_fui_ppt_t *ui, uint32_t c) { (void)ui; (void)c; }
void           ui_fui_ppt_set_timer(ui_fui_ppt_t *ui, const char *s) { (void)ui; (void)s; }
void           ui_fui_ppt_set_link(ui_fui_ppt_t *ui, const char *s, uint32_t c) { (void)ui; (void)s; (void)c; }
void           ui_fui_ppt_set_battery(ui_fui_ppt_t *ui, int s) { (void)ui; (void)s; }
void           ui_fui_ppt_set_visible(ui_fui_ppt_t *ui, ui_fui_ppt_view_t v) { (void)ui; (void)v; }
#endif