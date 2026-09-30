// main/ble_hid.h — BLE HID Keyboard service interface.
//
// Sends HID keyboard reports over Bluedroid (esp_hid) to control slide
// navigation. Ported from YeatsLiao/ai-passport-ppt; battery dependency
// removed for the BSP-style boundary.
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief 初始化 BLE HID Keyboard 服务
 * @return ESP_OK 成功
 */
esp_err_t ble_hid_init(void);

/**
 * @brief 发送按键按下+释放（单次击键）
 * @param keycode HID Usage ID（如 0x50 = Left Arrow）
 * @note 非阻塞，内部通过任务执行
 */
void ble_hid_key_press(uint8_t keycode);

/**
 * @brief 重置蓝牙配对：擦除 NVS 中的绑定密钥并重启，重启后回到未配对状态（按键触发配对广播）。
 * @note 调用后不会返回。
 */
void ble_hid_reset_bonding(void);

/**
 * @brief 发送带修饰键的击键（如 Cmd+Shift+Return）
 * @param modifier HID 修饰键位图（0x01=L-Ctrl, 0x02=L-Shift, 0x04=L-Alt, 0x08=L-Cmd/GUI...）
 * @param keycode  HID Usage ID
 */
void ble_hid_key_press_mod(uint8_t modifier, uint8_t keycode);

/**
 * @brief 发送"开始放映"组合击键，同时兼容 Windows 与 macOS：
 *        先发 F5（Windows/WPS/LibreOffice），再发 Cmd+Shift+Return（macOS PowerPoint），
 *        再发 Cmd+Alt+P（macOS Keynote）。
 */
void ble_hid_press_start_slideshow(void);

/**
 * @brief 检查是否有设备连接且已经过加密认证
 * @return true 已连接并认证
 */
bool ble_hid_is_connected(void);

/**
 * @brief 查询 NVS 中是否存在 BLE 绑定记录（配对过的宿主机）
 * @return true 至少有一条绑定
 */
bool ble_hid_has_bond(void);

/**
 * @brief 是否正在广播（仅由 ADV_START_COMPLETE 成功事件置位，反映真实状态）
 */
bool ble_hid_is_advertising(void);

/**
 * @brief 请求开始配对广播（幂等）。未绑定开机不自动广播；若协议栈尚未就绪，
 *        请求会被挂起，HID/广播数据就绪后自动补开。UI 应结合
 *        ble_hid_is_advertising() 展示真实广播状态。
 * @return true 请求已受理；false 协议栈未初始化
 */
bool ble_hid_start_pairing(void);

/**
 * @brief 触发一次对端 RSSI 读取并返回上一个有效样本（用于 UI 每秒轮询）
 * @param rssi_out 输出 dBm 值（-127..20，来自上次完成事件）
 * @return true 已连接且有有效样本；false 未连接或尚无样本
 */
bool ble_hid_poll_rssi(int8_t *rssi_out);

/**
 * @brief 停止 BLE HID 服务（停止广播，不卸载协议栈）
 */
void ble_hid_stop(void);
