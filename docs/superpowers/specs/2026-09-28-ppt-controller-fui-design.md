<p align="right">
  <a href="2026-09-28-ppt-controller-fui-design.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# PPT Controller — FUI Edition — Design

| | |
| --- | --- |
| Status | Draft for review |
| Date | 2026-09-28 |
| Target | FoloToy AI Passport (ESP32-C3, 8 MB Flash, no PSRAM) |
| Base | `FoloToy/ai-passport` @ `main` |
| Branch | `feature/ppt-controller-fui` |
| UI language | English + symbols (no Chinese characters) |
| Deliverable | Merged `build/FoloToy-AI-Passport-full.bin` flashable at `0x0` |

## 1. Goal

Turn a FoloToy AI Passport into a cross-platform wireless PowerPoint remote control
over Bluetooth Low Energy HID Keyboard. The device advertises as a generic HID
keyboard, pairs with Windows, macOS, iPadOS, and most Linux desktops without
software installation, and exposes:

- Slide forward / backward
- Start slideshow (cross-platform: F5 on Windows/WPS/LibreOffice,
  `Cmd+Shift+Return` on macOS PowerPoint, `Opt+Cmd+P` on macOS Keynote)
- Exit slideshow (`Escape`)
- Slide timer (starts on first Start, stops on Exit, resets on Exit)
- On-screen status: BLE connection, battery, last action

The application runs on the same BSP as the demo. It does not reuse the demo
test menu, demo screens, or `ui_pixel_*`. The FUI visual style is inspired by
`HwzLoveDz/folo-ai-passport-gesture-wand` but is fully redesigned for the PPT
control context (no gesture trace, manager dialog, or PIN gate).

## 2. Out of scope

- Black-screen / laser-pointer / presenter-view switching (not requested).
- Custom key remapping UI (not requested).
- Persistent slide counter across reboots (timer resets every exit; the slide
  index lives on the host PC, not the remote).
- iOS QuickPath / Android remote pairing (HID over BLE is not supported by
  stock iOS / Android — outside this design).
- Animations beyond the existing FUI scan-line and pulse patterns (no new
  particle systems or motion tracking).
- Reuse of the existing `main/demo_*.c`, `main/ui_pixel*`, `main/main.c`,
  `main/demo_navigation*` files. BSP is the only allowed import.

## 3. BSP and module boundary

Everything in `components/bsp` is reusable as-is:

- `bsp_display_*`, `bsp_lvgl_*` — ST7789P3 240×320 SPI, LVGL lock, backlight.
- `bsp_button_*` — UP / DOWN / OK with non-blocking click and long events.
- `bsp_audio_*` — initialized but unused by the deck (no tones). Stays available.
- `bsp_battery_*` — CW2017 state-of-charge for the BAT indicator.
- `bsp_i2c_*` — shared I2C0 bus scan at boot (parity with the demo baseline).
- `bsp_pins.h` — single source of truth for all pin constants.

The application owns:

- `main/app_ppt.*` — application lifecycle, glue between BSP, BLE, UI, timer.
- `main/ble_hid.*` — BLE HID Keyboard (ESP-IDF `esp_hid` + Bluedroid).
- `main/ppt_keys.h` — HID Usage ID constants for the five key actions.
- `main/ppt_timer.*` — slide timer state machine, decoupled from LVGL.
- `main/ui_fui_ppt.*` — all LVGL widgets, layout, and color tokens.
- `main/fonts/kode_mono/*` — Kode Mono font source copied from
  `HwzLoveDz/folo-ai-passport-gesture-wand`'s `main/fonts/kode_mono/`.
- `assets/fonts/kode_mono/` — license + source attribution for Kode Mono.

## 4. BLE HID Keyboard design

Port `YeatsLiao/ai-passport-ppt`'s `main/ble_hid.c` into a new
`main/ble_hid.{h,c}` under the current main branch. Required adaptations:

1. Replace `nvs_flash_init()` boilerplate with the BSP-driven pattern (call
   `nvs_flash_init()` before `ble_hid_init()`; on no-free-pages / new-version
   erase and reinit).
2. Drop `bsp_battery_*` dependency from `ble_hid.c` (battery display lives in
   the UI module, not in the BLE driver).
3. Keep the existing key timing (`KEY_HOLD_MS = 120`, `KEY_COMBO_GAP_MS = 200`)
   for macOS compatibility.
4. Keep `ble_hid_press_start_slideshow()` exactly — it is the proven
   cross-platform sequence: F5, wait 200 ms, Cmd+Shift+Return, wait 200 ms,
   Opt+Cmd+P. Each platform responds to one of the three.
5. Keep `ble_hid_reset_bonding()` which erases NVS and reboots.

Public API (unchanged from the source port):

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

`ble_hid_init()` must be called after `nvs_flash_init()` and after LVGL is
initialized (so the UI can display BLE INIT FAIL if it errors). The advertising
name remains `PPT-Remote`.

### 4.1. SDK config additions

```text
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED Classic and BLE
CONFIG_BT_NIMBLE_ENABLED=n
CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y
CONFIG_BT_BLE_50_FEATURES_SUPPORTED=y
CONFIG_BT_GATTS_ENABLE=y
CONFIG_BT_CLASSIC_ENABLED=n
```

These override the NimBLE-only settings in `sdkconfig.defaults`. The override
must be applied in the application's `sdkconfig.defaults` or in a separate
`sdkconfig.defaults.ppt-controller` consumed by the gate; the existing
baseline keeps NimBLE as the default so unrelated demos still build.

## 5. Key mapping

HID Usage IDs (Keyboard/Keypad page 0x07):

| Macro | Usage ID | Meaning |
| --- | --- | --- |
| `HID_KEY_LEFT_ARROW`  | `0x50` | Previous slide |
| `HID_KEY_RIGHT_ARROW` | `0x4F` | Next slide |
| `HID_KEY_ESCAPE`      | `0x29` | Exit slideshow |
| `HID_KEY_F5`          | `0x3E` | Start slideshow (Windows / WPS / LibreOffice) |
| `HID_KEY_RETURN`      | `0x28` | Return (used with `Cmd+Shift`) |
| `HID_KEY_P`           | `0x13` | `P` (used with `Cmd+Opt`) |
| `HID_MOD_LEFT_SHIFT`  | `0x02` | Modifier |
| `HID_MOD_LEFT_ALT`    | `0x04` | Modifier |
| `HID_MOD_LEFT_GUI`    | `0x08` | Cmd on macOS, Win on Windows |

Action binding (in `app_ppt.c`, runs in the button task — non-blocking):

| Physical button | Event | HID action |
| --- | --- | --- |
| UP    | click   | `ble_hid_key_press(HID_KEY_LEFT_ARROW)` |
| UP    | long    | Arm reset pairing (record tick) |
| DOWN  | click   | `ble_hid_key_press(HID_KEY_RIGHT_ARROW)` |
| DOWN  | long    | If armed within 3 s → `ble_hid_reset_bonding()` (no return) |
| OK    | click   | `ble_hid_press_start_slideshow()` + start timer if not running |
| OK    | long    | `ble_hid_key_press(HID_KEY_ESCAPE)` + stop + reset timer |

Long-press arming: stored as `TickType_t s_reset_arm_tick = 0`. Any other
button event clears the arming. The 3 s window is enforced at the time of the
DOWN long event.

## 6. Slide timer

`ppt_timer_t` holds `running`, `seconds`, and is driven by a 1 Hz LVGL timer
that runs only while `running` is true. Public API:

```c
void ppt_timer_init(ppt_timer_t *t);
void ppt_timer_start(ppt_timer_t *t);  // sets running=true, seconds=0
void ppt_timer_stop(ppt_timer_t *t);   // sets running=false, seconds=0
bool ppt_timer_format(const ppt_timer_t *t, char *out, size_t out_len);
```

`ppt_timer_format()` writes `MM:SS` or `HH:MM:SS` into `out`. This function
is pure logic: it depends only on `seconds`. A host test in
`tests/test_ppt_timer.c` covers:

- 0 s → `00:00`
- 59 s → `00:59`
- 60 s → `01:00`
- 3599 s → `59:59`
- 3600 s → `01:00:00`
- 86399 s → `23:59:59`

The timer ticks inside the LVGL task and is already inside the LVGL lock when
the LVGL timer fires. Reading `running` and `seconds` from the button task is
allowed because the button task only writes them via the LVGL timer callback
chain — see `app_ppt.c` for the lock discipline.

## 7. UI layout (FUI design — fully redesigned, not a port)

Screen is 240×320. No reuse of `ui_pixel_*`. All widgets are owned by
`main/ui_fui_ppt.c`.

### 7.1. Color tokens

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

These mirror the gesture-wand tokens by design intent but are kept in a
separate namespace so neither application imports the other.

### 7.2. Typography

Use Kode Mono from `HwzLoveDz/folo-ai-passport-gesture-wand`'s
`main/fonts/kode_mono/`. Copy the font source files into this project's
`main/fonts/kode_mono/` and register them in `main/CMakeLists.txt`. Sizes used
match the gesture-wand selection:

```c
meta   = &ui_font_kode_regular_11
body   = &ui_font_kode_regular_13
strong = &ui_font_kode_bold_13
title  = &ui_font_kode_bold_15
display= &ui_font_kode_bold_21
```

`assets/fonts/kode_mono/README.md` records the original source URL and
license.

### 7.3. Home view

```
┌──────────────────────────────────────────┐ 240×320
│  [rust|orange] 02 BT          [▮▮▮▮▯]    │ 0–44: top bar
│ ──────── (orange + cream section rule)    │
│                                          │
│  ┌──────────────────────────────────┐    │ home layer 44–298
│  │ SLIDE FLOW  (motion signature)   │    │
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
│  │ HOST LINK  (RSSI bars)           │    │
│  │ PAIR with PPT-Remote  ●●●○○      │    │
│  └──────────────────────────────────┘    │
│                                          │
│  ──────────── rule                       │
│  UP PREV | DOWN NEXT | OK START         │ 298–320: hint bar
└──────────────────────────────────────────┘
```

Top bar:

- Code block on the left (rust + orange split), label `02 BT`.
- Title `PPT CTRL` (Kode bold 15) on the same row.
- Battery segments on the right (5 segments, color = TEAL ≥ 20 %, AMBER 10–19 %, RED < 10 %).

Main panel:

- Section label `SLIDE FLOW` + sub-label `MOTION SIGNATURE`.
- Three-line action feedback zone:
  - Top row: large ◀ / timer / ▶
  - Middle row: last-action label (`<< PREV`, `NEXT >>`, `START SHOW`, `EXIT SHOW`, `READY`, `RESET ARM`).
  - Bottom row: status word (`PAIRING`, `CONNECTED`, `STARTING SHOW`, etc.).

Bottom link panel:

- Section label `HOST LINK`.
- Status dot + label (`PAIRING`, `CONNECTED`, plus the peer MAC when connected).
- Compact bar chart of the last 8 RSSI samples (or greyed out when unavailable — the ESP32-C3 does not report RSSI without a custom hook; if the read returns `ESP_FAIL` we display neutral bars, not blank).

Hint bar:

- One-line compact legend for the three buttons. Color = muted.

### 7.4. Menu view

Reached by long-pressing OK from the home view while the slideshow is **not**
running. Two rows: `RESET PAIRING` (red), `ABOUT` (cream). OK selects, UP/DOWN
navigate. OK-long or 30 s of inactivity returns to home.

Selecting `RESET PAIRING` opens the confirm dialog (section 7.5).

### 7.5. Confirm dialog (modal)

`04 / SECURE ACTION` header, `RESET PAIRING?` title, `CONFIRM / CANCEL`
buttons. UP/DOWN select, OK confirm. CANCEL dismisses. CONFIRM calls
`ble_hid_reset_bonding()`.

### 7.6. Action feedback states

| Action feedback label | Color | Trigger |
| --- | --- | --- |
| `READY`            | muted   | boot complete, no actions yet |
| `<< PREV`          | teal    | UP click succeeded |
| `NEXT >>`          | teal    | DOWN click succeeded |
| `START SHOW`       | orange  | OK click |
| `EXIT SHOW`        | amber   | OK long |
| `RESET ARM`        | amber   | UP long, awaiting DOWN long within 3 s |
| `RESETTING…`       | red     | reset in progress |
| `BLE INIT FAIL`    | red     | `ble_hid_init()` failed |
| `BT NOT READY`     | amber   | key press attempted while not authenticated |

`BT NOT READY` is shown for 1.5 s when a HID key press is dropped because
`s_connected` or `s_auth_ok` is false. It does not block the user; they can
retry once pairing completes.

## 8. Lifecycle

```
app_main()
 ├── nvs_flash_init()
 ├── bsp_i2c_init() + bsp_i2c_scan()
 ├── bsp_battery_init()           (graceful fail if chip missing)
 ├── bsp_display_init() + bsp_lvgl_init()
 ├── bsp_display_backlight(100)
 ├── ppt_timer_init()
 ├── bsp_button_init(on_key, NULL)
 ├── ui_fui_ppt_create()         (called with LVGL lock)
 ├── status_timer = lv_timer_create(status_tick, 500, NULL)
 ├── ble_hid_init()
 └── main loop yields to FreeRTOS
```

`on_key()` is the button callback. It runs in the button component task and
must be non-blocking. HID operations are dispatched to a small FreeRTOS
queue (depth 8) processed by an internal task inside `ble_hid.c`; UI updates
from the button path take `bsp_lvgl_lock(500)` for the brief label change.

`status_tick()` runs in the LVGL task every 500 ms (already inside LVGL lock)
and refreshes battery, connection, and RSSI indicators.

`ppt_timer_tick()` runs in the LVGL task every 1 s (already inside LVGL lock)
when `s_ppt_timer.running`.

Demo-page style is forbidden: no menu screen, no card list, no mascot. Only
the home / menu / dialog screens described here exist.

## 9. Partition table

Keep the existing `partitions.csv` (NVS, PHY, single factory app). BLE HID
adds no extra partitions; the application's text + font data must fit in the
factory app. Font footprint of Kode Mono is ≈ 60 KB across the five sizes;
acceptable.

## 10. Validation and delivery

Validation gate, in order:

1. `./tools/validate.sh --static` — repository checks + host tests.
   Host tests cover `ppt_timer_format` (see section 6) and a `ui_fui_ppt`
   layout-math test that verifies the home panel coordinates fit inside the
   240×320 frame with no negative offsets.
2. `./tools/validate.sh --firmware` — ESP-IDF 5.5.3 + esp32c3 build, merged
   image verified at `0x0`, `build/FoloToy-AI-Passport-full.bin` produced.
3. GitHub Actions `static-checks.yml` and `firmware-checks.yml` run on PR.

Final report format (must match AGENTS.md §"Required validation and delivery"):

```text
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: ...
```

The simulator test (`https://folotoy-passport-simulator.onrender.com`) is run
last by the user. The simulator does not exercise real BLE; it parses the
merged image's metadata and renders the UI from the application's screen
captures. The deliverable list includes the merged image plus the source
branch.

### 10.1. Local vs CI ESP-IDF

The agent checks `idf.py --version` after activation. If ESP-IDF v5.5.3 is
available locally, run the firmware gate locally. Otherwise, mark the firmware
gate as `NOT RUN` locally, push the branch, and let GitHub Actions perform
the firmware gate; the result is read back from the Actions run.

## 11. Risks and mitigations

| Risk | Mitigation |
| --- | --- |
| Kode Mono license unclear | Read upstream README; record attribution in `assets/fonts/kode_mono/README.md`; if license is incompatible, fall back to a permissive font of similar metrics. |
| BLE HID rejected by macOS | Keep the proven cross-platform Start sequence; document the limitation in the user-facing PR description. |
| Bluedroid + NimBLE conflict | Set both `CONFIG_BT_BLUEDROID_ENABLED=y` and `CONFIG_BT_NIMBLE_ENABLED=n` in the application's tracked `sdkconfig.defaults`; the baseline `sdkconfig.defaults` already disables Bluedroid, so this override must be in a separate `sdkconfig.defaults.ppt-controller` consumed by the gate. |
| Button event during reset causes stuck UI | `ble_hid_reset_bonding()` runs after a 500 ms grace during which the UI shows `RESETTING…`; the device reboots before any new event is processed. |
| Unicode glyph in Chinese button hints | Forbidden by section 7 — UI strings are English + symbols only; no glyph check is required. |

## 12. File-by-file change list

This branch replaces the baseline demo's `app_main` entry point. The new
application lives in `main/` alongside the demo files; the demo files stay on
disk but are no longer the default entry. A Kconfig option
`CONFIG_APP_PPT_ENABLED` (default `y` on this branch, default `n` on `main`)
selects which entry point `main/main.c` calls. This pattern mirrors how
`demo/blufi-provisioning` is gated.

```
ai-passport-base/
├── .github/workflows/                 (UNCHANGED, reused as-is)
├── components/bsp/                   (UNCHANGED)
├── partitions.csv                    (UNCHANGED)
├── sdkconfig.defaults                (UNCHANGED at top level; the gate
│                                      consumes sdkconfig.defaults.ppt-controller
│                                      when present, otherwise the default)
├── sdkconfig.defaults.ppt-controller (NEW; Bluedroid ON, NimBLE OFF)
├── Kconfig.projbuild                 (NEW; CONFIG_APP_PPT_ENABLED toggle)
├── main/
│   ├── main.c                        (MODIFIED; branches on CONFIG_APP_PPT_ENABLED)
│   ├── demo_*                        (UNCHANGED; baseline preserved for main)
│   ├── ui_pixel*                     (UNCHANGED; NOT used by this app)
│   ├── app_ppt.h                     (NEW)
│   ├── app_ppt.c                     (NEW)
│   ├── ble_hid.h                     (NEW; ported from YeatsLiao/ai-passport-ppt)
│   ├── ble_hid.c                     (NEW; ported from YeatsLiao/ai-passport-ppt)
│   ├── ppt_keys.h                    (NEW; HID Usage ID constants)
│   ├── ppt_timer.h                   (NEW)
│   ├── ppt_timer.c                   (NEW; pure logic)
│   ├── ui_fui_ppt.h                  (NEW)
│   ├── ui_fui_ppt.c                  (NEW; redesigned FUI layout)
│   ├── fonts/kode_mono/              (NEW; sources copied from gesture-wand)
│   └── CMakeLists.txt                (MODIFIED; add new sources)
├── assets/
│   └── fonts/kode_mono/README.md     (NEW; license + upstream URL)
├── tests/
│   ├── test_ppt_timer.c              (NEW)
│   └── test_ui_fui_ppt_layout.c      (NEW)
└── docs/superpowers/specs/
    ├── 2026-09-28-ppt-controller-fui-design.md  (this file)
    └── 2026-09-28-ppt-controller-fui-design.zh_CN.md  (paired translation)
```

`main/CMakeLists.txt` appends the new sources to the existing
`idf_component_register` block. The new sources are always compiled; the
Kconfig option only selects which `app_main` body runs.

## 13. Open questions

None at this point. All design decisions have a rationale; nothing is left as
TBD.