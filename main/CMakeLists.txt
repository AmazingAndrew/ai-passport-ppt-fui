set(app_demo_srcs
    "demo_display.c"
    "demo_button.c"
    "demo_audio.c"
    "demo_battery.c"
    "demo_radio.c"
    "demo_wifi.c"
    "demo_ble.c"
    "demo_low_power.c"
)
set(app_ppt_srcs
    "app_ppt.c"
    "ble_hid.c"
    "ppt_timer.c"
    "ui_fui_ppt.c"
)

set(common_srcs
    "main.c"
    "demo_navigation.c"
    "ui_pixel.c"
    "ui_pixel_math.c"
    "fonts/kode_mono/ui_font_kode_regular_11.c"
    "fonts/kode_mono/ui_font_kode_regular_13.c"
    "fonts/kode_mono/ui_font_kode_bold_13.c"
    "fonts/kode_mono/ui_font_kode_bold_15.c"
    "fonts/kode_mono/ui_font_kode_bold_21.c"
)

if(CONFIG_APP_PPT_ENABLED)
    set(component_srcs ${common_srcs} ${app_ppt_srcs})
else()
    set(component_srcs ${common_srcs} ${app_demo_srcs})
endif()

idf_component_register(
    SRCS ${component_srcs}
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
)
