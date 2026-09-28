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

static void update_action(ui_fui_ppt_action_t a) {
    if (!bsp_lvgl_lock(500)) return;
    ui_fui_ppt_set_action(s_ui, a);
    bsp_lvgl_unlock();
}

static void update_state(const char *text, uint32_t color) {
    if (!bsp_lvgl_lock(500)) return;
    ui_fui_ppt_set_state(s_ui, text);
    if (s_ui) {
        /* best-effort color on the state label: re-use set_link for the dot,
         * but the state label itself takes raw lv color via a small helper */
        extern void ui_fui_ppt_set_state_color(ui_fui_ppt_t *, uint32_t);
        ui_fui_ppt_set_state_color(s_ui, color);
    }
    bsp_lvgl_unlock();
}

/* status_tick is an LVGL timer callback, so it runs inside the LVGL task and
 * does not need to lock. It just repaints the state-label color in step with
 * the link-panel dot. */
static void update_state_color_only(uint32_t color) {
    if (!s_ui) return;
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

static void status_tick(lv_timer_t *t) {
    (void)t;
    int soc = bsp_battery_soc();
    if (s_ui) ui_fui_ppt_set_battery(s_ui, soc);

    if (ble_hid_is_connected()) {
        char peer[20];
        char buf[32];
        if (ble_hid_get_peer_str(peer, sizeof(peer))) {
            snprintf(buf, sizeof(buf), "LINK %s", peer);
        } else {
            snprintf(buf, sizeof(buf), "LINK ACTIVE");
        }
        if (s_ui) ui_fui_ppt_set_link(s_ui, buf, UI_FUI_PPT_TEAL);
        if (s_ui) update_state_color_only(UI_FUI_PPT_TEAL);
    } else {
        if (s_ui) ui_fui_ppt_set_link(s_ui, "PAIR with PPT-Remote",
                                      UI_FUI_PPT_MAGENTA);
        if (s_ui) update_state_color_only(UI_FUI_PPT_MAGENTA);
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