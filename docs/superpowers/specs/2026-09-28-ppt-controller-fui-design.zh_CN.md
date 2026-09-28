<p align="right">
  <strong>简体中文</strong> · <a href="2026-09-28-ppt-controller-fui-design.md">English</a>
</p>

# PPT 控制器 — FUI 版 — 设计文档

| | |
| --- | --- |
| 状态 | 草案,待评审 |
| 日期 | 2026-09-28 |
| 目标 | FoloToy AI Passport(ESP32-C3、8 MB Flash、无 PSRAM) |
| 基线 | `FoloToy/ai-passport` 的 `main` |
| 分支 | `feature/ppt-controller-fui` |
| UI 语言 | 英文 + 符号(不含中文) |
| 交付物 | 可在 `0x0` 烧录的合并固件 `build/FoloToy-AI-Passport-full.bin` |

## 1. 目标

把 FoloToy AI Passport 做成跨平台的无线 PowerPoint 遥控器,通过蓝牙低功耗
HID Keyboard 与 Windows / macOS / iPadOS / 主流 Linux 桌面配对,无需安装任何
软件。提供:

- 上一页 / 下一页
- 开始放映(跨平台:Windows/WPS/LibreOffice 的 F5,macOS PowerPoint 的
  `Cmd+Shift+Return`,macOS Keynote 的 `Opt+Cmd+P`)
- 退出放映(`Escape`)
- 演讲计时器(首次开始时启动,退出时停止并归零)
- 屏幕状态:BLE 连接、电量、最近一次按键

应用运行在与 demo 完全相同的 BSP 上,但**不沿用 demo 测试菜单或屏幕**,
也**不使用 `ui_pixel_*`**。FUI 视觉风格参考
`HwzLoveDz/folo-ai-passport-gesture-wand`,但完全为 PPT 遥控场景重新设计
(没有手势轨迹、宏管理器对话框、PIN 输入)。

## 2. 范围外

- 黑屏 / 激光笔 / 演示者视图切换(用户未要求)。
- 自定义键映射 UI(用户未要求)。
- 跨重启保留幻灯片计数(幻灯片索引在 PC 端,不在遥控端)。
- iOS QuickPath / Android 遥控配对(原生 iOS/Android 不支持 BLE HID)。
- 现有 FUI 扫描线和脉动之外的动画(不新增粒子或运动跟踪)。
- 复用 `main/demo_*` / `main/ui_pixel*` / `main/main.c` /
  `main/demo_navigation*`。只能复用 BSP。

## 3. BSP 与模块边界

`components/bsp` 的所有内容原样复用:

- `bsp_display_*` / `bsp_lvgl_*` —— ST7789P3 240×320 SPI、LVGL 锁、背光。
- `bsp_button_*` —— UP / DOWN / OK,非阻塞 click 与 long 事件。
- `bsp_audio_*` —— 初始化但本应用不放音(保留可用)。
- `bsp_battery_*` —— CW2017 电量百分比,供 BAT 指示用。
- `bsp_i2c_*` —— I2C0 总线扫描(与基线一致)。
- `bsp_pins.h` —— 引脚常量唯一事实源。

应用自有:

- `main/app_ppt.*` —— 生命周期,把 BSP / BLE / UI / 计时器串起来。
- `main/ble_hid.*` —— BLE HID Keyboard(ESP-IDF `esp_hid` + Bluedroid)。
- `main/ppt_keys.h` —— 五个动作的 HID Usage ID 常量。
- `main/ppt_timer.*` —— 演讲计时状态机,与 LVGL 解耦。
- `main/ui_fui_ppt.*` —— 所有 LVGL 控件、布局、颜色 token。
- `main/fonts/kode_mono/*` —— 从 gesture-wand 同源复制的 Kode Mono 字体源。
- `assets/fonts/kode_mono/` —— Kode Mono 的许可证与来源登记。

## 4. BLE HID Keyboard 设计

把 `YeatsLiao/ai-passport-ppt` 的 `main/ble_hid.c` 移植到当前 main 分支的
`main/ble_hid.{h,c}`。需要适配的:

1. `nvs_flash_init()` 的样板改为 BSP 风格(`ble_hid_init()` 之前完成,出现
   `ESP_ERR_NVS_NO_FREE_PAGES` / `ESP_ERR_NVS_NEW_VERSION_FOUND` 时擦后重试)。
2. 删除 `ble_hid.c` 对 `bsp_battery_*` 的依赖(电量显示属于 UI)。
3. 保留按键时序(`KEY_HOLD_MS = 120`,`KEY_COMBO_GAP_MS = 200`),macOS 兼容性。
4. 完整保留 `ble_hid_press_start_slideshow()` —— 经过验证的跨平台序列:
   F5 → 等 200 ms → Cmd+Shift+Return → 等 200 ms → Opt+Cmd+P,各平台响应
   其中的一个。
5. 保留 `ble_hid_reset_bonding()`(擦 NVS 后重启)。

公开 API(与源端一致):

```c
esp_err_t ble_hid_init(void);
void ble_hid_key_press(uint8_t keycode);
void ble_hid_key_press_mod(uint8_t modifier, uint8_t keycode);
void ble_hid_press_start_slideshow(void);
bool ble_hid_is_connected(void);
bool ble_hid_get_peer_str(char *buf, size_t len);
void ble_hid_reset_bonding(void);
void ble_hid_stop(void);
```

`ble_hid_init()` 必须在 `nvs_flash_init()` 之后、LVGL 初始化之后调用(这样
UI 能展示 `BLE INIT FAIL`)。广播名仍为 `PPT-Remote`。

### 4.1. sdkconfig 追加项

```text
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED_CLASSIC_BLE=y
CONFIG_BT_NIMBLE_ENABLED=n
CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y
CONFIG_BT_BLE_50_FEATURES_SUPPORTED=y
CONFIG_BT_GATTS_ENABLE=y
CONFIG_BT_CLASSIC_ENABLED=n
```

这覆盖 `sdkconfig.defaults` 中的 NimBLE 配置。这些覆盖必须放在应用层
`sdkconfig.defaults.ppt-controller` 中,基础基线仍保留 NimBLE 默认,
不影响其他 demo。

## 5. 按键映射

HID Usage ID(Keyboard/Keypad page 0x07):

| 宏 | Usage ID | 含义 |
| --- | --- | --- |
| `HID_KEY_LEFT_ARROW`  | `0x50` | 上一页 |
| `HID_KEY_RIGHT_ARROW` | `0x4F` | 下一页 |
| `HID_KEY_ESCAPE`      | `0x29` | 退出放映 |
| `HID_KEY_F5`          | `0x3E` | 开始放映,Windows / WPS / LibreOffice |
| `HID_KEY_RETURN`      | `0x28` | Return,与 `Cmd+Shift` 组合 |
| `HID_KEY_P`           | `0x13` | `P`,与 `Cmd+Opt` 组合 |
| `HID_MOD_LEFT_SHIFT`  | `0x02` | 修饰键 |
| `HID_MOD_LEFT_ALT`    | `0x04` | 修饰键 |
| `HID_MOD_LEFT_GUI`    | `0x08` | macOS 上为 Cmd,Windows 上为 Win |

动作绑定(在 `app_ppt.c`,运行在按键组件任务中 —— 必须非阻塞):

| 物理按键 | 事件 | HID 动作 |
| --- | --- | --- |
| 上    | click   | `ble_hid_key_press(HID_KEY_LEFT_ARROW)` |
| 上    | long    | 武装重置配对(记录 tick) |
| 下    | click   | `ble_hid_key_press(HID_KEY_RIGHT_ARROW)` |
| 下    | long    | 若 3 秒内已武装 → `ble_hid_reset_bonding()`(不返回) |
| OK    | click   | `ble_hid_press_start_slideshow()` + 若计时器未在运行则启动 |
| OK    | long    | `ble_hid_key_press(HID_KEY_ESCAPE)` + 停止并归零计时器 |

武装时间窗由 `TickType_t s_reset_arm_tick = 0` 维护。任何其他事件清除武装。
3 秒窗口在 DOWN long 事件处理时校验。

## 6. 演讲计时器

`ppt_timer_t` 持有 `running` 与 `seconds`,由 1 Hz LVGL 定时器驱动,只在
`running` 为真时计数。公开 API:

```c
void ppt_timer_init(ppt_timer_t *t);
void ppt_timer_start(ppt_timer_t *t);  // running=true, seconds=0
void ppt_timer_stop(ppt_timer_t *t);   // running=false, seconds=0
bool ppt_timer_format(const ppt_timer_t *t, char *out, size_t out_len);
```

`ppt_timer_format()` 输出 `MM:SS` 或 `HH:MM:SS`。纯逻辑,只依赖 `seconds`。
`tests/test_ppt_timer.c` 覆盖:

- 0 s → `00:00`
- 59 s → `00:59`
- 60 s → `01:00`
- 3599 s → `59:59`
- 3600 s → `01:00:00`
- 86399 s → `23:59:59`

定时器在 LVGL 任务中跑,触发时已持有 LVGL 锁。按钮任务读写 `running` 与
`seconds` 是合法的 —— 见 `app_ppt.c` 的锁协议。

## 7. UI 布局(FUI 全新设计)

屏幕 240×320。不复用 `ui_pixel_*`。所有控件由 `main/ui_fui_ppt.c` 拥有。

### 7.1. 颜色 token

```c
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
#define UI_FUI_PPT_TEXT         UI_FUI_PPT_CREAM
#define UI_FUI_PPT_MUTED        0x829097U
```

设计意图与 gesture-wand 同源,但命名空间独立,互不引用。

### 7.2. 字体

使用 `HwzLoveDz/folo-ai-passport-gesture-wand` 的
`main/fonts/kode_mono/`。把字体源复制到本项目 `main/fonts/kode_mono/`,
并在 `main/CMakeLists.txt` 中注册。字号继承 gesture-wand 选择:

```c
meta   = &ui_font_kode_regular_11
body   = &ui_font_kode_regular_13
strong = &ui_font_kode_bold_13
title  = &ui_font_kode_bold_15
display= &ui_font_kode_bold_21
```

`assets/fonts/kode_mono/README.md` 登记原始来源 URL 与许可证。

### 7.3. Home 视图

```
┌──────────────────────────────────────────┐ 240×320
│  [rust|orange] 02 BT          [▮▮▮▮▯]    │ 0–44: 顶栏
│ ──────── (橙 + 米白分隔条)               │
│                                          │
│  ┌──────────────────────────────────┐    │ 主面板
│  │ SLIDE FLOW   MOTION SIGNATURE    │    │
│  │ ──────────── rule                │    │
│  │                                  │    │
│  │   ◀ PREV      00:12:34   NEXT ▶  │    │
│  │                                  │    │
│  │   << PREV          NEXT >>       │    │
│  │                                  │    │
│  │   STATE: STARTING SHOW           │    │
│  └──────────────────────────────────┘    │
│                                          │
│  ┌──────────────────────────────────┐    │
│  │ HOST LINK  (RSSI 柱)             │    │
│  │ PAIR with PPT-Remote  ●●●○○      │    │
│  └──────────────────────────────────┘    │
│                                          │
│  ──────────── rule                       │
│  UP PREV | DOWN NEXT | OK START         │ 底部提示
└──────────────────────────────────────────┘
```

顶栏:左侧 rust+orange 段码块 `02 BT`;同排标题 `PPT CTRL`(Kode bold 15);
右侧五段电量(TEAL ≥ 20 %,AMBER 10–19 %,RED < 10 %)。

主面板:段标 `SLIDE FLOW` + 副标 `MOTION SIGNATURE`;三行反馈区:

- 上:大 ◀ / 计时 / ▶
- 中:最近动作(`<< PREV` / `NEXT >>` / `START SHOW` / `EXIT SHOW` / `READY` /
  `RESET ARM`)
- 下:状态词(`PAIRING` / `CONNECTED` / `STARTING SHOW` 等)

底栏:HOST LINK,状态点 + 文字(`PAIRING` / `CONNECTED` 及配对 MAC),
右侧 8 点 RSSI 柱状图(ESP32-C3 不直接暴露 RSSI 时显示中性灰柱)。

提示栏:三键图例,一行,mute 色。

### 7.4. 菜单视图

在放映**未**开始时长按 OK 进入。两行:`RESET PAIRING`(红)、`ABOUT`(米白)。
OK 选中,UP/DOWN 切换。OK 长按或 30 秒无操作返回 home。

选中 `RESET PAIRING` 打开确认对话框(7.5)。

### 7.5. 确认对话框(模态)

`04 / SECURE ACTION` 顶、`RESET PAIRING?` 标题、`CONFIRM / CANCEL` 按钮。
UP/DOWN 切换,OK 确认。CANCEL 关闭。CONFIRM 调用 `ble_hid_reset_bonding()`。

### 7.6. 动作反馈状态

| 反馈文字 | 颜色 | 触发 |
| --- | --- | --- |
| `READY`         | mute   | 启动完成,尚无动作 |
| `<< PREV`       | teal   | UP click 成功 |
| `NEXT >>`       | teal   | DOWN click 成功 |
| `START SHOW`    | orange | OK click |
| `EXIT SHOW`     | amber  | OK long |
| `RESET ARM`     | amber  | UP long,等待 3 秒内 DOWN long |
| `RESETTING…`    | red    | 正在重置 |
| `BLE INIT FAIL` | red    | `ble_hid_init()` 失败 |
| `BT NOT READY`  | amber  | HID 击键被丢弃(未连接/未认证) |

`BT NOT READY` 显示 1.5 秒,不阻塞用户,配对完成后可重试。

## 8. 生命周期

```
app_main()
 ├── nvs_flash_init()
 ├── bsp_i2c_init() + bsp_i2c_scan()
 ├── bsp_battery_init()           (失败优雅降级)
 ├── bsp_display_init() + bsp_lvgl_init()
 ├── bsp_display_backlight(100)
 ├── ppt_timer_init()
 ├── bsp_button_init(on_key, NULL)
 ├── ui_fui_ppt_create()         (持 LVGL 锁)
 ├── status_timer = lv_timer_create(status_tick, 500, NULL)
 ├── ble_hid_init()
 └── 主循环让出 FreeRTOS
```

`on_key()` 是按键回调,运行在按键组件任务,必须非阻塞。HID 操作送入
`ble_hid.c` 内部的小队列;UI 标签更新走 `bsp_lvgl_lock(500)`。

`status_tick()` 每 500 ms 在 LVGL 任务中(已持锁)刷新电量、连接、RSSI。

`ppt_timer_tick()` 每 1 s 在 LVGL 任务中触发,仅当 `s_ppt_timer.running`。

禁止 demo 风格:没有菜单屏幕、没有卡片列表、没有吉祥物。只有 home / menu /
dialog 三个屏幕。

## 9. 分区表

保持现有 `partitions.csv`(NVS、PHY、单 factory app)。BLE HID 不增加分区。
字体 ≈ 60 KB,可装入 factory app。

## 10. 验证与交付

依次执行:

1. `./tools/validate.sh --static` —— 仓库检查 + host tests。
   覆盖 `ppt_timer_format`(见 §6)与 `ui_fui_ppt` 布局数学(host test
   验证 home 面板坐标全部落在 240×320 内,无负偏移)。
2. `./tools/validate.sh --firmware` —— ESP-IDF 5.5.3 + esp32c3 编译,
   合并镜像校验 `0x0` 偏移,产出 `build/FoloToy-AI-Passport-full.bin`。
3. GitHub Actions:`static-checks.yml`、`firmware-checks.yml` 在 PR 上跑。

最终报告格式(必须符合 AGENTS.md §"必须执行的验证与交付格式"):

```text
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: ...
```

模拟器测试(`https://folotoy-passport-simulator.onrender.com`)由用户在交付后
手动运行。模拟器不验证真实 BLE,只解析合并镜像元数据并基于应用截屏渲染。

### 10.1. 本地 vs CI 的 ESP-IDF

激活后执行 `idf.py --version`。若本机有 ESP-IDF v5.5.3 则跑本地固件门禁;
否则本地标记 `NOT RUN`,推送到 GitHub 由 Actions 跑,再读回结果。

## 11. 风险与缓解

| 风险 | 缓解 |
| --- | --- |
| Kode Mono 许可证不明 | 读上游 README;在 `assets/fonts/kode_mono/README.md` 登记;不可用则回退到同度量宽松字体。 |
| macOS 拒绝 HID | 保留已验证的跨平台开始序列;在 PR 描述中注明限制。 |
| Bluedroid 与 NimBLE 冲突 | 在 `sdkconfig.defaults.ppt-controller` 显式 `Bluedroid=y, NimBLE=n`;基线 `sdkconfig.defaults` 不动。 |
| 重置时按键事件卡死 UI | `ble_hid_reset_bonding()` 前留 500 ms 展示 `RESETTING…`,设备先重启。 |
| 中文按键提示的 Unicode 字形 | §7 明确禁止 UI 字符串含中文;无需字形检查。 |

## 12. 文件级变更清单

本分支替换基线 demo 的 `app_main` 入口。新应用与 demo 同在 `main/`,demo
文件保留但不再是默认入口。Kconfig 选项 `CONFIG_APP_PPT_ENABLED`(本分支
默认 `y`,`main` 分支默认 `n`)决定 `main/main.c` 调用哪个入口。
这与 `demo/blufi-provisioning` 的门控模式一致。

```
ai-passport-base/
├── .github/workflows/                 (UNCHANGED,原样复用)
├── components/bsp/                   (UNCHANGED)
├── partitions.csv                    (UNCHANGED)
├── sdkconfig.defaults                (UNCHANGED;门禁在存在
│                                      sdkconfig.defaults.ppt-controller 时优先用它)
├── sdkconfig.defaults.ppt-controller (NEW;Bluedroid ON,NimBLE OFF)
├── Kconfig.projbuild                 (NEW;CONFIG_APP_PPT_ENABLED 开关)
├── main/
│   ├── main.c                        (MODIFIED;按 CONFIG_APP_PPT_ENABLED 分支)
│   ├── demo_*                        (UNCHANGED;保留供 main 分支使用)
│   ├── ui_pixel*                     (UNCHANGED;本应用不使用)
│   ├── app_ppt.h                     (NEW)
│   ├── app_ppt.c                     (NEW)
│   ├── ble_hid.h                     (NEW;移植自 YeatsLiao/ai-passport-ppt)
│   ├── ble_hid.c                     (NEW;移植自 YeatsLiao/ai-passport-ppt)
│   ├── ppt_keys.h                    (NEW;HID Usage ID 常量)
│   ├── ppt_timer.h                   (NEW)
│   ├── ppt_timer.c                   (NEW;纯逻辑)
│   ├── ui_fui_ppt.h                  (NEW)
│   ├── ui_fui_ppt.c                  (NEW;重新设计的 FUI 布局)
│   ├── fonts/kode_mono/              (NEW;复制自 gesture-wand)
│   └── CMakeLists.txt                (MODIFIED;追加新源文件)
├── assets/
│   └── fonts/kode_mono/README.md     (NEW;许可证 + 上游 URL)
├── tests/
│   ├── test_ppt_timer.c              (NEW)
│   └── test_ui_fui_ppt_layout.c      (NEW)
└── docs/superpowers/specs/
    ├── 2026-09-28-ppt-controller-fui-design.md  (英文,本文件的对偶)
    └── 2026-09-28-ppt-controller-fui-design.zh_CN.md  (本文)
```

`main/CMakeLists.txt` 在现有 `idf_component_register` 块上追加新源文件。
新源文件始终被编译,Kconfig 选项只选择哪个 `app_main` 主体运行。

## 13. 开放问题

无。设计决策均有理由,没有 TBD。