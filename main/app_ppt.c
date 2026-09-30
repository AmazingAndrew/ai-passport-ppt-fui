// main/app_ppt.c — glue layer wiring BSP, BLE HID, slide timer, and FUI UI.
//
// Lifecycle:
//   1. NVS init (with erase-and-retry)
//   2. BSP i2c + display + LVGL + backlight + battery (graceful fail)
//   3. Slide timer init
//   4. FUI UI creation (under LVGL lock)
//   5. Button callback registration (runs in button task; must be non-blocking)
//   6. Two LVGL timers: status (500 ms) + slide timer tick (1 s)
//   7. BLE HID init; on failure, surface BLE_INIT_FAIL to the UI
//
// After app_main_ppt() returns, the FreeRTOS scheduler runs. No busy loop.
#include "app_ppt.h"

#include "ble_hid.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "ppt_keys.h"
#include "ppt_timer.h"
#include "ui_fui_ppt.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "app_ppt";

static const char *const HINT_NORMAL = "UP PREV | DOWN NEXT | OK START";
static const char *const HINT_PAIR   = "PRESS OK TO PAIR";
static const char *const HINT_SEARCH = "SEARCH \"PPT-Remote\" ON PC";
#define DEVICE_NAME_UI "PPT-Remote"   /* must match DEVICE_NAME in ble_hid.c */

static ui_fui_ppt_t *s_ui = NULL;
static ppt_timer_t   s_timer;
static volatile TickType_t s_reset_arm_tick = 0;
static lv_timer_t    *s_status_timer = NULL;
static lv_timer_t    *s_ppt_timer = NULL;

/* animation state, all advanced by the 500 ms status tick */
static bool      s_blink_half = false;
static const char *s_state_shown = NULL;
static const char *s_link_shown = NULL;
static const char *s_hint_shown = NULL;
static unsigned  s_rssi_div = 0;
static uint8_t   s_arrow_ticks = 0;
static bool      s_arrow_prev = false;

/* action line = transient feedback (4 s) overlaid on a phase display that
 * is derived from the live link state — PAIRING persists as long as the
 * radio is really advertising, and ends the moment the link comes up. */
static volatile ui_fui_ppt_action_t s_transient = UI_FUI_PPT_ACTION_READY;
static volatile TickType_t s_transient_until = 0;
static int s_action_shown = -1;

static void show_action(ui_fui_ppt_action_t a) {
    if ((int)a == s_action_shown) return;
    s_action_shown = (int)a;
    ui_fui_ppt_set_action(s_ui, a);
}

static void update_action(ui_fui_ppt_action_t a) {
    s_transient = a;
    s_transient_until = xTaskGetTickCount() + pdMS_TO_TICKS(4000);
    if (!bsp_lvgl_lock(500)) return;
    show_action(a);
    bsp_lvgl_unlock();
}

static void flash_arrow(bool prev) {
    if (!bsp_lvgl_lock(500)) return;
    ui_fui_ppt_set_arrow(s_ui, prev, true);
    bsp_lvgl_unlock();
    s_arrow_prev = prev;
    s_arrow_ticks = 2; /* ~1 s, cleared in status_tick */
}

static void show_state(const char *text, uint32_t color) {
    if (s_state_shown == text) return;
    s_state_shown = text;
    ui_fui_ppt_set_state(s_ui, text);
    ui_fui_ppt_set_state_color(s_ui, color);
}

static void update_timer_label(void) {
    if (!bsp_lvgl_lock(500)) return;
    char buf[16];
    if (ppt_timer_format(&s_timer, buf, sizeof(buf))) {
        ui_fui_ppt_set_timer(s_ui, buf);
    }
    bsp_lvgl_unlock();
}

/* status_tick is an LVGL timer callback: it runs inside the LVGL task, so
 * the ui_fui_ppt_* calls below need no lock. */
static void status_tick(lv_timer_t *t) {
    (void)t;
    s_blink_half = !s_blink_half;

    if (s_ui) ui_fui_ppt_set_battery(s_ui, bsp_battery_soc());
    if (!s_ui) return;

    bool connected = ble_hid_is_connected();

    /* unpair combo stays visible for 3 s after the UP-long arming press */
    bool armed = false;
    if (s_reset_arm_tick != 0) {
        if (xTaskGetTickCount() - s_reset_arm_tick <= pdMS_TO_TICKS(3000)) {
            armed = true;
        } else {
            s_reset_arm_tick = 0;
        }
    }

    bool adv = connected || ble_hid_is_advertising();
    const char *link_text = connected ? "PC CONNECTED"
                              : (adv ? "PAIRING" : "WAITING");
    uint32_t link_color = connected ? UI_FUI_PPT_TEAL
                            : (adv ? UI_FUI_PPT_MAGENTA : UI_FUI_PPT_MUTED);
    if (link_text != s_link_shown) {
        s_link_shown = link_text;
        ui_fui_ppt_set_link(s_ui, link_text, link_color);
    }

    if (armed) {
        show_state("NOW HOLD DOWN", UI_FUI_PPT_AMBER);
        ui_fui_ppt_set_pairing_blink(s_ui, true);
    } else if (connected) {
        show_state(s_timer.running ? "LIVE" : "READY",
                   s_timer.running ? UI_FUI_PPT_GREEN
                                   : UI_FUI_PPT_AMBER);
        ui_fui_ppt_set_pairing_blink(s_ui, true);
        if (++s_rssi_div >= 2) {
            s_rssi_div = 0;
            int8_t rssi = 0;
            ui_fui_ppt_set_rssi(s_ui,
                                ble_hid_poll_rssi(&rssi) ? rssi : 1);
        }
    } else {
        s_rssi_div = 0;
        ui_fui_ppt_set_rssi(s_ui, 1); /* invalid -> "--" quality */
        if (adv) {
            /* advertise the searchable name where "PAIRING" used to repeat
             * the action line */
            show_state(DEVICE_NAME_UI, UI_FUI_PPT_CREAM);
            ui_fui_ppt_set_pairing_blink(s_ui, s_blink_half);
        } else {
            /* unbonded boot: radio idle until OK is pressed */
            show_state("NO PAIR", UI_FUI_PPT_MUTED);
            ui_fui_ppt_set_pairing_blink(s_ui, true);
        }
    }

    /* action line: phase display unless a transient feedback is on screen */
    ui_fui_ppt_action_t phase = (!connected && adv)
        ? UI_FUI_PPT_ACTION_PAIRING : UI_FUI_PPT_ACTION_READY;
    if (s_transient_until != 0) {
        bool done = xTaskGetTickCount() > s_transient_until ||
                    (phase == UI_FUI_PPT_ACTION_READY &&
                     s_transient == UI_FUI_PPT_ACTION_PAIRING);
        if (done) {
            s_transient_until = 0;
            s_transient = phase;
        }
    }
    if (s_transient_until == 0) show_action(phase);

    const char *hint = connected ? HINT_NORMAL
                       : (adv ? HINT_SEARCH : HINT_PAIR);
    if (hint != s_hint_shown) {
        s_hint_shown = hint;
        ui_fui_ppt_set_hint(s_ui, hint);
    }

    if (s_arrow_ticks > 0 && --s_arrow_ticks == 0) {
        ui_fui_ppt_set_arrow(s_ui, s_arrow_prev, false);
    }
}

static void ppt_tick(lv_timer_t *t) {
    (void)t;
    if (!s_timer.running) return;
    ppt_timer_tick(&s_timer);
    update_timer_label();
}

/* OK is the single pairing entry point: it asks the BLE layer to advertise
 * (queued automatically if the stack is still warming up). UP/DOWN never
 * start pairing — idle they point the user at OK, pairing they just
 * re-confirm the phase. */
static void pair_or_notify(void) {
    if (ble_hid_start_pairing()) {
        update_action(UI_FUI_PPT_ACTION_PAIRING);
    } else {
        update_action(UI_FUI_PPT_ACTION_BT_NOT_READY);
    }
}

static void not_ready_hint(void) {
    update_action(ble_hid_is_advertising() ? UI_FUI_PPT_ACTION_PAIRING
                                           : UI_FUI_PPT_ACTION_OK_TO_PAIR);
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    TickType_t now = xTaskGetTickCount();

    switch (btn) {
    case BSP_BTN_UP:
        if (ev == BSP_BTN_CLICK) {
            if (!ble_hid_is_connected()) {
                not_ready_hint();
            } else {
                ble_hid_key_press(HID_KEY_LEFT_ARROW);
                update_action(UI_FUI_PPT_ACTION_PREV);
                flash_arrow(true);
            }
        } else if (ev == BSP_BTN_LONG) {
            s_reset_arm_tick = now;
            update_action(UI_FUI_PPT_ACTION_RESET_ARM);
        }
        break;
    case BSP_BTN_DOWN:
        if (ev == BSP_BTN_CLICK) {
            if (!ble_hid_is_connected()) {
                not_ready_hint();
            } else {
                ble_hid_key_press(HID_KEY_RIGHT_ARROW);
                update_action(UI_FUI_PPT_ACTION_NEXT);
                flash_arrow(false);
            }
        } else if (ev == BSP_BTN_LONG) {
            if (s_reset_arm_tick != 0 &&
                (now - s_reset_arm_tick) <= pdMS_TO_TICKS(3000)) {
                update_action(UI_FUI_PPT_ACTION_RESETTING);
                vTaskDelay(pdMS_TO_TICKS(500));
                ble_hid_reset_bonding();
            }
            s_reset_arm_tick = 0;
        }
        break;
    case BSP_BTN_OK:
        s_reset_arm_tick = 0;
        if (ev == BSP_BTN_CLICK) {
            if (!ble_hid_is_connected()) {
                pair_or_notify();
            } else {
                ble_hid_press_start_slideshow();
                if (!s_timer.running) {
                    ppt_timer_start(&s_timer);
                    update_timer_label();
                }
                update_action(UI_FUI_PPT_ACTION_START);
            }
        } else if (ev == BSP_BTN_LONG) {
            if (ble_hid_is_connected()) {
                ble_hid_key_press(HID_KEY_ESCAPE);
            }
            ppt_timer_stop(&s_timer);
            update_timer_label();
            update_action(UI_FUI_PPT_ACTION_EXIT);
        }
        break;
    default:
        break;
    }
}

void app_main_ppt(void) {
    ESP_LOGI(TAG, "AI Passport PPT controller (FUI edition)");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    bsp_i2c_init();
    bsp_i2c_scan();
    (void)bsp_battery_init(); /* graceful fail */

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed");
        return;
    }
    bsp_display_backlight(100);

    ppt_timer_init(&s_timer);

    if (bsp_lvgl_lock(1000)) {
        s_ui = ui_fui_ppt_create();
        bsp_lvgl_unlock();
    }
    if (!s_ui) {
        ESP_LOGE(TAG, "ui_fui_ppt_create failed");
        return;
    }

    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "button init failed");
        return;
    }

    s_status_timer = lv_timer_create(status_tick, 500, NULL);
    s_ppt_timer    = lv_timer_create(ppt_tick, 1000, NULL);

    err = ble_hid_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BLE HID init failed: %s", esp_err_to_name(err));
        update_action(UI_FUI_PPT_ACTION_BLE_INIT_FAIL);
    } else {
        ESP_LOGI(TAG, "ready: pair 'PPT-Remote' in PC Bluetooth");
    }
}