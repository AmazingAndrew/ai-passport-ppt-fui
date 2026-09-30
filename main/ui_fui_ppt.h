// main/ui_fui_ppt.h — FUI-styled UI for the PPT controller.
//
// Color tokens mirror HwzLoveDz/folo-ai-passport-gesture-wand by intent,
// but the namespace is independent. No widget code is shared.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

#define UI_FUI_PPT_BG           0x04070BU
#define UI_FUI_PPT_PANEL        0x0C1117U
#define UI_FUI_PPT_PANEL_ALT    0x171216U
#define UI_FUI_PPT_ORANGE       0xFF6A1AU
#define UI_FUI_PPT_RUST         0x762311U
#define UI_FUI_PPT_CREAM        0xF1E8D2U
#define UI_FUI_PPT_TEAL         0x7AD8D1U
#define UI_FUI_PPT_MAGENTA      0xC43D1BU
#define UI_FUI_PPT_AMBER        0xFF9B35U
#define UI_FUI_PPT_RED          0xFF4D6DU
#define UI_FUI_PPT_GREEN        0x57C775U
#define UI_FUI_PPT_TEXT         UI_FUI_PPT_CREAM
#define UI_FUI_PPT_MUTED        0x829097U

typedef enum {
    UI_FUI_PPT_ACTION_READY = 0,
    UI_FUI_PPT_ACTION_PREV,
    UI_FUI_PPT_ACTION_NEXT,
    UI_FUI_PPT_ACTION_START,
    UI_FUI_PPT_ACTION_EXIT,
    UI_FUI_PPT_ACTION_RESET_ARM,
    UI_FUI_PPT_ACTION_RESETTING,
    UI_FUI_PPT_ACTION_BLE_INIT_FAIL,
    UI_FUI_PPT_ACTION_BT_NOT_READY,
    UI_FUI_PPT_ACTION_PAIRING,
    UI_FUI_PPT_ACTION_OK_TO_PAIR,
} ui_fui_ppt_action_t;

typedef enum {
    UI_FUI_PPT_VIEW_HOME = 0,
    UI_FUI_PPT_VIEW_MENU,
    UI_FUI_PPT_VIEW_DIALOG,
} ui_fui_ppt_view_t;

typedef struct {
    int x, y, w, h;
} ui_fui_ppt_layout_rect_t;

typedef struct {
    ui_fui_ppt_layout_rect_t top_bar;
    ui_fui_ppt_layout_rect_t main_panel;
    ui_fui_ppt_layout_rect_t link_panel;
    ui_fui_ppt_layout_rect_t hint_bar;
} ui_fui_ppt_layout_t;

/* Internal widget rects (screen coordinates) for the two content panels.
 * Exposed as pure layout math so the host test can prove no element
 * overlaps or leaves its panel; the ESP build consumes these to place the
 * real LVGL objects, keeping test and firmware on one source of truth. */
typedef struct {
    ui_fui_ppt_layout_rect_t timer;
    ui_fui_ppt_layout_rect_t prev_arrow;
    ui_fui_ppt_layout_rect_t next_arrow;
    ui_fui_ppt_layout_rect_t action;
    ui_fui_ppt_layout_rect_t state;
} ui_fui_ppt_main_layout_t;

typedef struct {
    ui_fui_ppt_layout_rect_t host_link;
    ui_fui_ppt_layout_rect_t status_dot;
    ui_fui_ppt_layout_rect_t status_value;
    ui_fui_ppt_layout_rect_t signal_label;
    ui_fui_ppt_layout_rect_t signal_value;
} ui_fui_ppt_link_layout_t;

/* User-facing link-quality buckets derived from RSSI. Plain words instead
 * of dBm numbers or signal bars (v0.2.1 device feedback: unreadable).
 * Mapping/text/color are pure functions outside the ESP guard so the host
 * test pins every threshold. */
typedef enum {
    UI_FUI_PPT_LINK_NONE = 0,   /* no valid sample */
    UI_FUI_PPT_LINK_WEAK,
    UI_FUI_PPT_LINK_MEDIUM,
    UI_FUI_PPT_LINK_STRONG,
} ui_fui_ppt_link_quality_t;

ui_fui_ppt_link_quality_t ui_fui_ppt_link_quality(int rssi_dbm);
const char *ui_fui_ppt_link_quality_text(ui_fui_ppt_link_quality_t q);
uint32_t    ui_fui_ppt_link_quality_color(ui_fui_ppt_link_quality_t q);

typedef struct ui_fui_ppt_s ui_fui_ppt_t;

ui_fui_ppt_layout_t    ui_fui_ppt_home_layout(void);
ui_fui_ppt_layout_rect_t ui_fui_ppt_menu_row_rect(unsigned index);
ui_fui_ppt_main_layout_t ui_fui_ppt_main_layout(void);
ui_fui_ppt_link_layout_t ui_fui_ppt_link_layout(void);

ui_fui_ppt_t          *ui_fui_ppt_create(void);
void                   ui_fui_ppt_destroy(ui_fui_ppt_t *ui);

void                   ui_fui_ppt_set_action(ui_fui_ppt_t *ui,
                                            ui_fui_ppt_action_t action);
void                   ui_fui_ppt_set_state(ui_fui_ppt_t *ui,
                                            const char *state);
void                   ui_fui_ppt_set_state_color(ui_fui_ppt_t *ui,
                                                  uint32_t color);
void                   ui_fui_ppt_set_timer(ui_fui_ppt_t *ui,
                                             const char *timer);
void                   ui_fui_ppt_set_link(ui_fui_ppt_t *ui,
                                           const char *text, uint32_t color);
void                   ui_fui_ppt_set_battery(ui_fui_ppt_t *ui, int soc);
void                   ui_fui_ppt_set_rssi(ui_fui_ppt_t *ui, int rssi_dbm);
void                   ui_fui_ppt_set_hint(ui_fui_ppt_t *ui,
                                           const char *text);
void                   ui_fui_ppt_set_pairing_blink(ui_fui_ppt_t *ui,
                                                    bool bright);
void                   ui_fui_ppt_set_arrow(ui_fui_ppt_t *ui, bool prev,
                                            bool active);
void                   ui_fui_ppt_set_visible(ui_fui_ppt_t *ui,
                                             ui_fui_ppt_view_t view);