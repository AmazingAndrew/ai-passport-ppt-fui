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

static ui_fui_ppt_t *s_ui = NULL;
static ppt_timer_t   s_timer;
static TickType_t     s_reset_arm_tick = 0;
static lv_timer_t    *s_status_timer = NULL;
static lv_timer_t    *s_ppt_timer = NULL;

/* animation state, all advanced by the 500 ms status tick */
static bool      s_blink_half = false;
static bool      s_connected_shown = false;
static const char *s_state_shown = NULL;
static unsigned  s_rssi_div = 0;
static uint8_t   s_arrow_ticks = 0;
static bool      s_arrow_prev = false;

static void update_action(ui_fui_ppt_action_t a) {
    if (!bsp_lvgl_lock(500)) return;
    ui_fui_ppt_set_action(s_ui, a);
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
        if (s_timer.running && s_blink_half) {
            for (char *p = buf; *p; p++) {
                if (*p == ':') *p = ' ';
            }
        }
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

    bool connected = ble_hid_is_connected();
    if (s_ui) {
        if (connected != s_connected_shown) {
            s_connected_shown = connected;
            ui_fui_ppt_set_link(s_ui,
                connected ? "PC CONNECTED" : "PAIRING",
                connected ? UI_FUI_PPT_TEAL : UI_FUI_PPT_MAGENTA);
        }
        if (connected) {
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
            show_state("PAIRING", UI_FUI_PPT_MAGENTA);
            ui_fui_ppt_set_pairing_blink(s_ui, s_blink_half);
            ui_fui_ppt_set_rssi(s_ui, 1); /* invalid -> "-- dBm" + gap */
        }
        if (s_arrow_ticks > 0 && --s_arrow_ticks == 0) {
            ui_fui_ppt_set_arrow(s_ui, s_arrow_prev, false);
        }
        if (s_timer.running) update_timer_label();  /* drives colon blink */
    }
}

static void ppt_tick(lv_timer_t *t) {
    (void)t;
    if (!s_timer.running) return;
    ppt_timer_tick(&s_timer);
    update_timer_label();
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    TickType_t now = xTaskGetTickCount();

    switch (btn) {
    case BSP_BTN_UP:
        if (ev == BSP_BTN_CLICK) {
            if (!ble_hid_is_connected()) {
                update_action(UI_FUI_PPT_ACTION_BT_NOT_READY);
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
                update_action(UI_FUI_PPT_ACTION_BT_NOT_READY);
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
                update_action(UI_FUI_PPT_ACTION_BT_NOT_READY);
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