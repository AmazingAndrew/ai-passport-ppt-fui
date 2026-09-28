# PPT Controller FUI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a BLE HID Keyboard PPT remote control application on FoloToy AI Passport with a FUI-styled interface, and ship it as a merged `build/FoloToy-AI-Passport-full.bin` flashable at `0x0`.

**Architecture:** New application code lives under `main/` next to the demo files. The demo entry is gated by `CONFIG_APP_PPT_ENABLED` (default `y` on this branch). The application is composed of: a pure-logic slide timer (`ppt_timer`), a port of `YeatsLiao/ai-passport-ppt`'s BLE HID driver (`ble_hid`), a from-scratch FUI UI (`ui_fui_ppt`), and a thin glue layer (`app_ppt`) that wires BSP, BLE, timer, and UI together. BSP is reused unchanged. CI reuses all 4 existing workflows.

**Tech Stack:** ESP-IDF v5.5.3, esp32c3, LVGL 9.5, FreeRTOS, ESP-IDF `esp_hid` + Bluedroid, CW2017 battery gauge, ST7789P3 SPI display, Kode Mono (font).

**Spec:** `docs/superpowers/specs/2026-09-28-ppt-controller-fui-design.md` and its paired `…zh_CN.md`. The plan argues from these specs; executors read both.

## Global Constraints

- Target: ESP32-C3, 8 MB Flash, no PSRAM, ESP-IDF 5.5.3.
- BSP files in `components/bsp/` are immutable. Only `components/bsp/include/bsp_pins.h` may change, and only for documented hardware reasons (do not change in this plan).
- LVGL is not thread-safe; non-LVGL-task code that touches LVGL objects must hold `bsp_lvgl_lock()`.
- Mandatory UI redesign: do NOT include, copy, or extend `main/demo_*`, `main/ui_pixel*`, `main/demo_navigation*`. The new UI is in `main/ui_fui_ppt.c` only.
- Button callbacks must stay non-blocking. HID operations are dispatched to the internal task inside `ble_hid.c`.
- Demo must stop every task, timer, callback, and event handler that can access its UI before deleting the screen.
- Testable state machines, protocols, timing, and layout calculations are independent from ESP-IDF/LVGL and covered by host tests.
- Never commit credentials, device QR secrets, private keys, personal data, or unsanitized logs.
- All Markdown has English at the default `.md` path and Simplified Chinese in a paired `.zh_CN.md` file.
- Do not commit `build/` artifacts.

## File Structure

| Path | Purpose |
| --- | --- |
| `main/app_ppt.h` | Public application lifecycle header. |
| `main/app_ppt.c` | Glue: button callback, status tick, lifecycle entry. |
| `main/ble_hid.h` | Public BLE HID driver API. |
| `main/ble_hid.c` | BLE HID driver implementation (esp_hid + Bluedroid). |
| `main/ppt_keys.h` | HID Usage ID constants. Header-only. |
| `main/ppt_timer.h` | Slide timer state machine API. |
| `main/ppt_timer.c` | Slide timer implementation, no LVGL dependency. |
| `main/ui_fui_ppt.h` | UI widget handles, color tokens, public UI API. |
| `main/ui_fui_ppt.c` | FUI layout (home / menu / dialog). |
| `main/fonts/kode_mono/*.c` | Kode Mono font source (5 sizes). |
| `main/main.c` | MODIFIED: branch on `CONFIG_APP_PPT_ENABLED`. |
| `main/CMakeLists.txt` | MODIFIED: register new sources and font file. |
| `Kconfig.projbuild` | NEW: `CONFIG_APP_PPT_ENABLED` toggle. |
| `sdkconfig.defaults.ppt-controller` | NEW: Bluedroid ON, NimBLE OFF. |
| `assets/fonts/kode_mono/README.md` | NEW: license + upstream URL attribution. |
| `tests/test_ppt_timer.c` | NEW: host test for `ppt_timer_format`. |
| `tests/test_ui_fui_ppt_layout.c` | NEW: host test for layout math bounds. |

Decomposition rationale:

- `ble_hid` is a port of one upstream file; the only reason to split it is for the header boundary.
- `ui_fui_ppt` is intentionally one file because the layout is one tightly-coupled screen set. If a future task adds a second screen family, split.
- `app_ppt` is small glue; if it grows past 200 lines, split into `app_ppt.c` and `app_ppt_ui.c`.
- `ppt_timer` is small and pure; keep it standalone so the host test does not need LVGL/ESP-IDF.

---

## Task 0: Environment setup, skill installation, GitHub repo

**Files:**
- Create: `tools/install_passport_skills.py` (already exists in repo; we run it)
- No code files modified

**Interfaces:**
- Consumes: nothing (first task)
- Produces: A `feature/ppt-controller-fui` branch (already created in spec phase), 5 installed skills, a new GitHub repo, ESP-IDF 5.5.3 verified.

- [ ] **Step 1: Verify git branch is correct**

Run:
```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
git rev-parse --abbrev-ref HEAD
```
Expected: `feature/ppt-controller-fui`

If the branch is wrong, do not continue. Run `git checkout feature/ppt-controller-fui` first.

- [ ] **Step 2: Verify ESP-IDF 5.5.3 availability**

Run:
```bash
source <path-to-esp-idf-v5.5.3>/export.sh 2>/dev/null
which idf.py 2>&1
idf.py --version 2>&1 | head -3
```

Expected: `idf.py` found and version reports `ESP-IDF v5.5.3`.

If unavailable: the local environment has no ESP-IDF. Continue with the rest of the tasks; the firmware gate will be run by GitHub Actions in Task 10. Record the `NOT RUN` status in the final report.

- [ ] **Step 3: Install the 5 required skills**

Run the repository's installer:
```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
python3 tools/install_passport_skills.py 2>&1 | tail -20
```

Expected output: lists each of `passport-develop`, `passport-setup`, `passport-build`, `passport-device-test`, `passport-debug` as installed/registered.

If the installer fails or is missing, manually copy each skill's directory to the AI tool's skills folder per the tool's convention. Verify by listing the tool's skills directory and confirming all 5 names are present.

- [ ] **Step 4: Confirm `gh` CLI authentication**

Run:
```bash
gh auth status 2>&1
```

Expected: `Logged in to github.com as <user>`.

If not authenticated, run `gh auth login` and follow the prompts. Do not proceed without auth.

- [ ] **Step 5: Create the new GitHub repo**

The user chose "新建仓库" for delivery. Default name: `ai-passport-ppt-fui`. Adjust if the user already named one.

Run:
```bash
gh repo create ai-passport-ppt-fui --public --description "FUI-styled BLE HID PPT remote control for FoloToy AI Passport" --source=. --remote=origin-fui --push 2>&1 | tail -10
```

Wait — `--push` here would push the current state. We want to push only after all code tasks land. Use this command instead:

```bash
gh repo create ai-passport-ppt-fui --public --description "FUI-styled BLE HID PPT remote control for FoloToy AI Passport" 2>&1 | tail -5
```

Then add the remote locally:
```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
git remote add origin-fui git@github.com:<your-username>/ai-passport-ppt-fui.git
git fetch origin-fui 2>&1 | tail -3
```

If `origin` is already set to the upstream `FoloToy/ai-passport`, do not change it. The new remote is `origin-fui` for delivery.

Verify:
```bash
git remote -v 2>&1
```

Expected: `origin` points to `FoloToy/ai-passport`; `origin-fui` points to the new repo.

- [ ] **Step 6: Commit (no changes yet)**

Run:
```bash
git status --short
```

If anything is uncommitted, stop and investigate. Do not proceed with dirty state.

If clean, no commit is required for this task. The branch already has the spec from the brainstorming phase.

---

## Task 1: Bring in Kode Mono fonts and asset attribution

**Files:**
- Create: `main/fonts/kode_mono/*.c` (5 files copied from gesture-wand)
- Create: `assets/fonts/kode_mono/README.md`
- Modify: `assets/README.md` (add a brief pointer)

**Interfaces:**
- Consumes: `git show origin/HEAD:main/fonts/kode_mono/` from gesture-wand reference
- Produces: A `main/fonts/kode_mono/` directory with 5 font `.c` files and one `.h`; an attribution document.

- [ ] **Step 1: Verify the source files exist in the gesture-wand clone**

Run:
```bash
ls /tmp/gesture-wand/main/fonts/kode_mono/ 2>&1
```

Expected: at least 5 `.c` files (one per size) and one or more `.h` headers.

If the directory is missing or empty, the upstream layout differs. Stop and inspect `/tmp/gesture-wand/main/fonts/` directly. Adjust the file list in step 2 accordingly.

- [ ] **Step 2: Create the destination directory and copy files**

```bash
mkdir -p "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/main/fonts/kode_mono"
cp /tmp/gesture-wand/main/fonts/kode_mono/* "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/main/fonts/kode_mono/"
ls "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/main/fonts/kode_mono/" 2>&1
```

Expected: the same set of `.c` and `.h` files as the source.

- [ ] **Step 3: Read the upstream font license from gesture-wand**

Read `/tmp/gesture-wand/main/fonts/kode_mono/README.md` (if present) or `/tmp/gesture-wand/assets/fonts/kode_mono/README.md` if the upstream keeps the license there. Note the license text and the upstream URL.

- [ ] **Step 4: Write the attribution document**

Create `assets/fonts/kode_mono/README.md`:

```markdown
# Kode Mono

Source: https://github.com/HwzLoveDz/folo-ai-passport-gesture-wand
Upstream path: `main/fonts/kode_mono/`
License: <LICENSE NAME FROM UPSTREAM>

These font source files are copied verbatim from the upstream repository at
the time of the initial implementation. They are used under the terms of the
upstream license. See the upstream repository for the latest version.

If the upstream license is incompatible with this project's MIT license,
remove these files and replace with a permissively licensed mono font of
similar metrics.
```

Replace `<LICENSE NAME FROM UPSTREAM>` with the actual license name.

- [ ] **Step 5: Append a pointer in `assets/README.md`**

Open `/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/assets/README.md` (create if missing) and add a one-line entry pointing to `assets/fonts/kode_mono/README.md`. If the file does not exist, create it with:

```markdown
# Assets

Third-party material used by this firmware.

- [Kode Mono](fonts/kode_mono/README.md) — fonts used by the FUI layout.
```

- [ ] **Step 6: Commit**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
git add main/fonts/ assets/
git status --short
git commit -m "feat(ui): bring in Kode Mono fonts from gesture-wand + asset attribution

Font source copied verbatim from HwzLoveDz/folo-ai-passport-gesture-wand
main/fonts/kode_mono/. License recorded in assets/fonts/kode_mono/README.md." 2>&1 | tail -3
```

---

## Task 2: Kconfig + sdkconfig gate files

**Files:**
- Create: `Kconfig.projbuild`
- Create: `sdkconfig.defaults.ppt-controller`

**Interfaces:**
- Consumes: nothing
- Produces: `CONFIG_APP_PPT_ENABLED` toggle; a tracked SDK config override consumed by the firmware gate.

- [ ] **Step 1: Write `Kconfig.projbuild`**

Create `/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/Kconfig.projbuild`:

```
menu "FoloToy AI Passport — applications"

config APP_PPT_ENABLED
    bool "Enable the PPT controller FUI application"
    default y
    help
        When enabled, main/main.c calls app_main_ppt() instead of the
        hardware-test demo menu. The demo menu (main/demo_*.c) is kept
        on disk but unused while this option is on.

endmenu
```

- [ ] **Step 2: Write `sdkconfig.defaults.ppt-controller`**

Create `/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/sdkconfig.defaults.ppt-controller`:

```
# --- BLE stack: Bluedroid for HID Keyboard (replaces baseline NimBLE) ---
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED_CLASSIC_BLE=y
CONFIG_BT_NIMBLE_ENABLED=n
CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y
CONFIG_BT_BLE_50_FEATURES_SUPPORTED=y
CONFIG_BT_GATTS_ENABLE=y
CONFIG_BT_CLASSIC_ENABLED=n

# --- Memory: app uses fonts (≈60 KB) + LVGL + BLE stack ---
CONFIG_LV_MEM_SIZE_KILOBYTES=40

# --- Application selection ---
CONFIG_APP_PPT_ENABLED=y
```

- [ ] **Step 3: Verify the gate picks up the override**

Open `/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/tools/validate.sh` and find the line that builds `sdkconfig` from `sdkconfig.defaults`. Confirm the script honours `sdkconfig.defaults.*` files in addition to `sdkconfig.defaults`. If the gate consumes only the single file, edit the gate to concatenate them and document the change in a follow-up commit; otherwise leave the gate alone.

Run:
```bash
grep -n "sdkconfig.defaults" "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/tools/validate.sh"
```

Expected: a list of file paths the script concatenates. `sdkconfig.defaults.ppt-controller` must be in that list, or the override is silently ignored.

- [ ] **Step 4: If the gate does not consume the override, fix it**

If `sdkconfig.defaults.ppt-controller` is not picked up, add it to the script's concat list with a comment explaining the override. Do NOT change unrelated lines.

- [ ] **Step 5: Commit**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
git add Kconfig.projbuild sdkconfig.defaults.ppt-controller tools/validate.sh
git status --short
git commit -m "build(kconfig): add APP_PPT_ENABLED toggle and Bluedroid sdkconfig override

The override file is consumed by the firmware gate only when present,
so the baseline main branch is unaffected." 2>&1 | tail -3
```

---

## Task 3: HID key constants header

**Files:**
- Create: `main/ppt_keys.h`

**Interfaces:**
- Consumes: nothing
- Produces: HID Usage ID macros used by `app_ppt.c` and `ble_hid.c` callers.

- [ ] **Step 1: Write `main/ppt_keys.h`**

Create `/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/main/ppt_keys.h`:

```c
// main/ppt_keys.h — HID Usage IDs for the PPT remote.
//
// The numeric values come from the USB HID Usage Tables (Keyboard/Keypad
// page 0x07) and are required by esp_hid_dev_input_set() to construct a
// keyboard report. Modifier bit masks come from the same spec.
#pragma once

#include <stdint.h>

#define HID_KEY_LEFT_ARROW   0x50u
#define HID_KEY_RIGHT_ARROW  0x4Fu
#define HID_KEY_ESCAPE       0x29u
#define HID_KEY_F5           0x3Eu
#define HID_KEY_RETURN       0x28u
#define HID_KEY_P            0x13u

#define HID_MOD_LEFT_SHIFT   0x02u
#define HID_MOD_LEFT_ALT     0x04u
#define HID_MOD_LEFT_GUI     0x08u
```

- [ ] **Step 2: Verify the file compiles standalone**

This header is included by both `ble_hid.c` and `app_ppt.c`. It has no implementation, so a syntactic check is sufficient:

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
gcc -std=c11 -Wall -Wextra -fsyntax-only -xc -E main/ppt_keys.h 2>&1
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add main/ppt_keys.h
git commit -m "feat(keys): add HID Usage ID constants for the PPT remote" 2>&1 | tail -3
```

---

## Task 4: ppt_timer — pure logic + host test (TDD)

**Files:**
- Create: `tests/test_ppt_timer.c`
- Create: `main/ppt_timer.h`
- Create: `main/ppt_timer.c`

**Interfaces:**
- Consumes: nothing (pure logic)
- Produces:
  - `ppt_timer_init(ppt_timer_t *)`
  - `ppt_timer_start(ppt_timer_t *)`
  - `ppt_timer_stop(ppt_timer_t *)`
  - `ppt_timer_tick(ppt_timer_t *)` (call once per second)
  - `ppt_timer_format(const ppt_timer_t *, char *out, size_t out_len) -> bool`

- [ ] **Step 1: Write the failing test**

Create `/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/tests/test_ppt_timer.c`:

```c
// tests/test_ppt_timer.c — host test for ppt_timer_format.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../main/ppt_timer.h"

static void check(ppt_timer_t t, unsigned seconds, const char *expected) {
    ppt_timer_init(&t);
    for (unsigned i = 0; i < seconds; i++) {
        ppt_timer_tick(&t);
    }
    char buf[16];
    bool ok = ppt_timer_format(&t, buf, sizeof(buf));
    assert(ok);
    assert(strcmp(buf, expected) == 0);
}

int main(void) {
    ppt_timer_t t;
    ppt_timer_init(&t);
    char buf[16];
    assert(ppt_timer_format(&t, buf, sizeof(buf)));
    assert(strcmp(buf, "00:00") == 0);

    check(t, 0,    "00:00");
    check(t, 59,   "00:59");
    check(t, 60,   "01:00");
    check(t, 3599, "59:59");
    check(t, 3600, "01:00:00");
    check(t, 86399,"23:59:59");

    /* start / stop transitions */
    ppt_timer_init(&t);
    ppt_timer_start(&t);
    assert(t.running);
    for (unsigned i = 0; i < 5; i++) ppt_timer_tick(&t);
    ppt_timer_stop(&t);
    assert(!t.running);
    assert(ppt_timer_format(&t, buf, sizeof(buf)));
    assert(strcmp(buf, "00:00") == 0);

    /* buffer too small must fail cleanly */
    ppt_timer_init(&t);
    for (unsigned i = 0; i < 3600; i++) ppt_timer_tick(&t);
    char small[4];
    assert(!ppt_timer_format(&t, small, sizeof(small)));

    printf("test_ppt_timer: PASS\n");
    return 0;
}
```

- [ ] **Step 2: Run the test — expect compile failure**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_ppt_timer.c -o /tmp/test_ppt_timer 2>&1 | tail -10
```

Expected: `fatal error: main/ppt_timer.h: No such file or directory` (or similar).

- [ ] **Step 3: Write `main/ppt_timer.h`**

```c
// main/ppt_timer.h — slide timer state machine. Pure logic, no LVGL/ESP-IDF.
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool running;
    unsigned seconds;
} ppt_timer_t;

void ppt_timer_init(ppt_timer_t *t);
void ppt_timer_start(ppt_timer_t *t);
void ppt_timer_stop(ppt_timer_t *t);
void ppt_timer_tick(ppt_timer_t *t);
bool ppt_timer_format(const ppt_timer_t *t, char *out, size_t out_len);
```

- [ ] **Step 4: Write the minimal implementation in `main/ppt_timer.c`**

```c
// main/ppt_timer.c — pure logic, decoupled from LVGL/ESP-IDF so it can be
// unit tested on the host.
#include "ppt_timer.h"

#include <limits.h>

void ppt_timer_init(ppt_timer_t *t) {
    if (!t) return;
    t->running = false;
    t->seconds = 0;
}

void ppt_timer_start(ppt_timer_t *t) {
    if (!t) return;
    t->running = true;
    t->seconds = 0;
}

void ppt_timer_stop(ppt_timer_t *t) {
    if (!t) return;
    t->running = false;
    t->seconds = 0;
}

void ppt_timer_tick(ppt_timer_t *t) {
    if (!t || !t->running) return;
    if (t->seconds < UINT_MAX) {
        t->seconds++;
    }
}

bool ppt_timer_format(const ppt_timer_t *t, char *out, size_t out_len) {
    if (!t || !out || out_len < 9) return false; /* "HH:MM:SS" + NUL */
    unsigned h = t->seconds / 3600u;
    unsigned m = (t->seconds % 3600u) / 60u;
    unsigned s = t->seconds % 60u;
    if (h > 0u) {
        snprintf(out, out_len, "%02u:%02u:%02u", h, m, s);
    } else {
        snprintf(out, out_len, "%02u:%02u", m, s);
    }
    return true;
}
```

- [ ] **Step 5: Run the test — expect PASS**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_ppt_timer.c main/ppt_timer.c -o /tmp/test_ppt_timer 2>&1 | tail -5
/tmp/test_ppt_timer 2>&1
```

Expected: `test_ppt_timer: PASS` and exit code 0.

- [ ] **Step 6: Commit**

```bash
git add main/ppt_timer.h main/ppt_timer.c tests/test_ppt_timer.c
git commit -m "feat(timer): slide timer state machine with host test

Pure logic, no LVGL/ESP-IDF dependency. Format covers MM:SS and
HH:MM:SS, buffer overflow returns false." 2>&1 | tail -3
```

---

## Task 5: ui_fui_ppt layout — UI module + layout-math host test

**Files:**
- Create: `tests/test_ui_fui_ppt_layout.c`
- Create: `main/ui_fui_ppt.h`
- Create: `main/ui_fui_ppt.c`

**Interfaces:**
- Consumes: LVGL, Kode Mono fonts (registered in CMakeLists later), LVGL types
- Produces:
  - `ui_fui_ppt_create()` — creates the entire screen tree under the active screen; returns a `ui_fui_ppt_t` with all widget handles
  - `ui_fui_ppt_set_action(ui_fui_ppt_t *, ui_fui_ppt_action_t action)`
  - `ui_fui_ppt_set_state(ui_fui_ppt_t *, const char *state)`
  - `ui_fui_ppt_set_timer(ui_fui_ppt_t *, const char *timer)`
  - `ui_fui_ppt_set_link(ui_fui_ppt_t *, const char *text, uint32_t color)`
  - `ui_fui_ppt_set_battery(ui_fui_ppt_t *, int soc_percent)`
  - `ui_fui_ppt_set_visible(ui_fui_ppt_t *, ui_fui_ppt_view_t view)`
  - `ui_fui_ppt_layout_rect_t` struct with `x, y, w, h` for layout-math tests

- [ ] **Step 1: Write the failing layout test**

Create `/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/tests/test_ui_fui_ppt_layout.c`:

```c
// tests/test_ui_fui_ppt_layout.c — verifies the static layout rectangles of
// the FUI home view fit inside the 240x320 panel and never overlap a 6 px
// safe margin around the bezel. No LVGL calls; pure math.
#include <assert.h>
#include <stdio.h>

#include "../main/ui_fui_ppt.h"

static void within(int x, int y, int w, int h, const char *name) {
    assert(x >= 0);
    assert(y >= 0);
    assert(x + w <= 240);
    assert(y + h <= 320);
    (void)name;
}

static void no_overlap(const ui_fui_ppt_layout_rect_t *a,
                       const ui_fui_ppt_layout_rect_t *b,
                       const char *an, const char *bn) {
    int ax2 = a->x + a->w;
    int ay2 = a->y + a->h;
    int bx2 = b->x + b->w;
    int by2 = b->y + b->h;
    bool overlap = (a->x < bx2) && (ax2 > b->x) &&
                   (a->y < by2) && (ay2 > b->y);
    assert(!overlap);
    (void)an; (void)bn;
}

int main(void) {
    ui_fui_ppt_layout_t l = ui_fui_ppt_home_layout();

    within(l.top_bar.x, l.top_bar.y, l.top_bar.w, l.top_bar.h, "top_bar");
    within(l.main_panel.x, l.main_panel.y, l.main_panel.w, l.main_panel.h, "main_panel");
    within(l.link_panel.x, l.link_panel.y, l.link_panel.w, l.link_panel.h, "link_panel");
    within(l.hint_bar.x, l.hint_bar.y, l.hint_bar.w, l.hint_bar.h, "hint_bar");

    /* gap between sections */
    assert(l.top_bar.y + l.top_bar.h <= l.main_panel.y);
    assert(l.main_panel.y + l.main_panel.h <= l.link_panel.y);
    assert(l.link_panel.y + l.link_panel.h <= l.hint_bar.y);

    no_overlap(&l.top_bar, &l.main_panel, "top_bar", "main_panel");
    no_overlap(&l.main_panel, &l.link_panel, "main_panel", "link_panel");
    no_overlap(&l.link_panel, &l.hint_bar, "link_panel", "hint_bar");

    /* menu view */
    ui_fui_ppt_layout_rect_t m = ui_fui_ppt_menu_row_rect(0);
    within(m.x, m.y, m.w, m.h, "menu_row_0");
    ui_fui_ppt_layout_rect_t m1 = ui_fui_ppt_menu_row_rect(1);
    within(m1.x, m1.y, m1.w, m1.h, "menu_row_1");
    no_overlap(&m, &m1, "menu_row_0", "menu_row_1");

    printf("test_ui_fui_ppt_layout: PASS\n");
    return 0;
}
```

- [ ] **Step 2: Run the test — expect compile failure**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_ui_fui_ppt_layout.c -o /tmp/test_ui_fui_ppt_layout 2>&1 | tail -10
```

Expected: `fatal error: main/ui_fui_ppt.h: No such file or directory`.

- [ ] **Step 3: Write `main/ui_fui_ppt.h`**

```c
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

typedef struct ui_fui_ppt_s ui_fui_ppt_t;

ui_fui_ppt_layout_t    ui_fui_ppt_home_layout(void);
ui_fui_ppt_layout_rect_t ui_fui_ppt_menu_row_rect(unsigned index);

ui_fui_ppt_t          *ui_fui_ppt_create(void);
void                   ui_fui_ppt_destroy(ui_fui_ppt_t *ui);

void                   ui_fui_ppt_set_action(ui_fui_ppt_t *ui,
                                            ui_fui_ppt_action_t action);
void                   ui_fui_ppt_set_state(ui_fui_ppt_t *ui,
                                            const char *state);
void                   ui_fui_ppt_set_timer(ui_fui_ppt_t *ui,
                                             const char *timer);
void                   ui_fui_ppt_set_link(ui_fui_ppt_t *ui,
                                           const char *text, uint32_t color);
void                   ui_fui_ppt_set_battery(ui_fui_ppt_t *ui, int soc);
void                   ui_fui_ppt_set_visible(ui_fui_ppt_t *ui,
                                             ui_fui_ppt_view_t view);
```

- [ ] **Step 4: Write the implementation in `main/ui_fui_ppt.c`**

```c
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
// pulling in LVGL.

#include "ui_fui_ppt.h"

#include <string.h>

LV_FONT_DECLARE(ui_font_kode_regular_11);
LV_FONT_DECLARE(ui_font_kode_regular_13);
LV_FONT_DECLARE(ui_font_kode_bold_13);
LV_FONT_DECLARE(ui_font_kode_bold_15);
LV_FONT_DECLARE(ui_font_kode_bold_21);

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
```

- [ ] **Step 5: Run the layout test — expect PASS**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
cc -std=c11 -Wall -Wextra -Werror -DLVGL_H_INCLUDE_SIMPLE -Icomponents/bsp/include -Imain tests/test_ui_fui_ppt_layout.c main/ui_fui_ppt.c -o /tmp/test_ui_fui_ppt_layout 2>&1 | tail -10
```

If `ui_fui_ppt.c` references LVGL symbols, the compile will fail (because we didn't include `lvgl.h` on the host command). To make this test host-runnable **without** LVGL, the layout helpers `ui_fui_ppt_home_layout()` and `ui_fui_ppt_menu_row_rect()` must compile even when LVGL is absent.

Update `main/ui_fui_ppt.c` to wrap the LVGL-only parts in `#ifdef LVGL_H_INCLUDE_SIMPLE`:

```c
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
```

Then move every LVGL function body (`ui_fui_ppt_create`, `ui_fui_ppt_destroy`, and the setters) inside:

```c
#ifdef LVGL_H_INCLUDE_SIMPLE
/* full LVGL implementation */
#else
/* host stubs so the test compiles */
ui_fui_ppt_t *ui_fui_ppt_create(void) { return NULL; }
void           ui_fui_ppt_destroy(ui_fui_ppt_t *ui) { (void)ui; }
void           ui_fui_ppt_set_action(ui_fui_ppt_t *ui, ui_fui_ppt_action_t a) { (void)ui; (void)a; }
void           ui_fui_ppt_set_state(ui_fui_ppt_t *ui, const char *s) { (void)ui; (void)s; }
void           ui_fui_ppt_set_timer(ui_fui_ppt_t *ui, const char *s) { (void)ui; (void)s; }
void           ui_fui_ppt_set_link(ui_fui_ppt_t *ui, const char *s, uint32_t c) { (void)ui; (void)s; (void)c; }
void           ui_fui_ppt_set_battery(ui_fui_ppt_t *ui, int s) { (void)ui; (void)s; }
void           ui_fui_ppt_set_visible(ui_fui_ppt_t *ui, ui_fui_ppt_view_t v) { (void)ui; (void)v; }
#endif
```

The layout helpers stay outside the guard because they have no LVGL dependency.

Re-run the test:

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_ui_fui_ppt_layout.c main/ui_fui_ppt.c -o /tmp/test_ui_fui_ppt_layout 2>&1 | tail -5
/tmp/test_ui_fui_ppt_layout 2>&1
```

Expected: `test_ui_fui_ppt_layout: PASS`.

- [ ] **Step 6: Commit**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
git add main/ui_fui_ppt.h main/ui_fui_ppt.c tests/test_ui_fui_ppt_layout.c
git commit -m "feat(ui): FUI layout for the PPT controller with layout-math test

Color tokens, typography, home / menu / dialog views. Layout helpers
(ui_fui_ppt_home_layout, ui_fui_ppt_menu_row_rect) compile without LVGL
so the host test verifies all rectangles fit inside 240x320 and do not
overlap." 2>&1 | tail -3
```

---

## Task 6: BLE HID driver (port + adaptation)

**Files:**
- Create: `main/ble_hid.h`
- Create: `main/ble_hid.c`

**Interfaces:**
- Consumes: ESP-IDF Bluedroid (`esp_bt`, `esp_gap_ble_api`, `esp_gatts_api`, `esp_hidd`, `esp_hid_common`), `nvs_flash`
- Produces: documented in §4 of the spec.

This task is a port with no separate host test (the BLE stack only runs on the target).

- [ ] **Step 1: Read the upstream source carefully**

Read `/tmp/ai-passport-ppt/main/ble_hid.c` and `/tmp/ai-passport-ppt/main/ble_hid.h` end-to-end. Note: this code uses `esp_hid` + Bluedroid, both available in ESP-IDF v5.5.3.

- [ ] **Step 2: Write `main/ble_hid.h`**

The public API is the same as the spec §4. Use the file structure from `/tmp/ai-passport-ppt/main/ble_hid.h` but include `esp_err.h` and `stdbool.h` only — drop the dependency on `bsp_battery.h`.

- [ ] **Step 3: Write `main/ble_hid.c`**

Port `/tmp/ai-passport-ppt/main/ble_hid.c` verbatim for the BLE stack parts, with these adaptations:

1. Move HID keycode constants and modifier bitmaps into `main/ppt_keys.h` (Task 3 already created it). Use those macros instead of the local `HID_KEY_*` defines.
2. Remove the `#include "bsp_battery.h"` line and any reference to battery state inside `ble_hid.c`.
3. Keep `KEY_HOLD_MS = 120`, `KEY_COMBO_GAP_MS = 200`.
4. Keep `ble_hid_press_start_slideshow()` exactly.
5. Keep `ble_hid_reset_bonding()` exactly.
6. The advertising name remains `"PPT-Remote"`.
7. Wrap the file body in `#include "sdkconfig.h"` and `#if CONFIG_BT_BLUEDROID_ENABLED` … `#else` … `#error` … `#endif` so a misconfigured build fails at compile time, not at runtime.

Add a final compile-time assertion:

```c
_Static_assert(CONFIG_BT_BLUEDROID_ENABLED, "PPT controller requires Bluedroid");
```

at the top of `ble_hid.c`.

- [ ] **Step 4: Verify the file syntactically compiles by running `idf.py` if available**

If ESP-IDF is available locally:
```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
source <path-to-esp-idf-v5.5.3>/export.sh
cp sdkconfig.defaults.ppt-controller sdkconfig.defaults
cp Kconfig.projbuild /tmp/_kconfig_test_backup  # do not lose this
idf.py --version
idf.py set-target esp32c3 2>&1 | tail -5
```

Expected: target set; if the build proceeds, run `idf.py build 2>&1 | tail -20`. Expect "ble_hid.c: PASS" or similar compilation. If the build does not complete because `bluedroid` is not yet resolvable, that is acceptable — the firmware gate in Task 9 will resolve it.

If ESP-IDF is unavailable, skip this step. Mark in the commit message that the file has been ported but not locally built.

- [ ] **Step 5: Commit**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
git add main/ble_hid.h main/ble_hid.c
git status --short
git commit -m "feat(ble): port YeatsLiao/ai-passport-ppt BLE HID Keyboard to BSP style

HID Usage IDs come from ppt_keys.h. Battery dependency removed.
Bluedroid required by CONFIG_BT_BLUEDROID_ENABLED static assertion." 2>&1 | tail -3
```

---

## Task 7: app_ppt — glue layer

**Files:**
- Create: `main/app_ppt.h`
- Create: `main/app_ppt.c`

**Interfaces:**
- Consumes: BSP (`bsp_*`), `ble_hid`, `ppt_timer`, `ui_fui_ppt`
- Produces:
  - `void app_main_ppt(void)` — entry point called from `main.c` when `CONFIG_APP_PPT_ENABLED=y`

- [ ] **Step 1: Write `main/app_ppt.h`**

```c
// main/app_ppt.h — application entry point for the PPT controller.
#pragma once

void app_main_ppt(void);
```

- [ ] **Step 2: Write `main/app_ppt.c`**

Implement `app_main_ppt()` per spec §8. Concrete requirements:

- `nvs_flash_init()` with erase-and-retry on `ESP_ERR_NVS_NO_FREE_PAGES` / `ESP_ERR_NVS_NEW_VERSION_FOUND`.
- Initialize I2C, scan I2C, init battery (graceful fail), init display, init LVGL, set backlight to 100.
- Init `ppt_timer_t s_timer`.
- `ui_fui_ppt_create()` under LVGL lock.
- Register button callback via `bsp_button_init(on_key, NULL)`. Note: the BSP button handler runs in the button component task; keep `on_key` non-blocking.
- Start a 500 ms status LVGL timer that refreshes battery + BLE state.
- Start a 1 s LVGL timer that calls `ppt_timer_tick` only when `s_timer.running`.
- Call `ble_hid_init()`. On failure, set `ui_fui_ppt_set_action(BLE_INIT_FAIL)` and abort the BLE-dependent state changes.
- After return, the FreeRTOS scheduler runs; no busy loop.

`on_key(btn, ev, user)`:

```c
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    TickType_t now = xTaskGetTickCount();

    switch (btn) {
    case BSP_BTN_UP:
        if (ev == BSP_BTN_CLICK) {
            ble_hid_key_press(HID_KEY_LEFT_ARROW);
            update_action(UI_FUI_PPT_ACTION_PREV);
        } else if (ev == BSP_BTN_LONG) {
            s_reset_arm_tick = now;
            update_action(UI_FUI_PPT_ACTION_RESET_ARM);
        }
        break;

    case BSP_BTN_DOWN:
        if (ev == BSP_BTN_CLICK) {
            ble_hid_key_press(HID_KEY_RIGHT_ARROW);
            update_action(UI_FUI_PPT_ACTION_NEXT);
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
            ble_hid_press_start_slideshow();
            if (!s_timer.running) {
                ppt_timer_start(&s_timer);
                update_timer_label();
            }
            update_action(UI_FUI_PPT_ACTION_START);
        } else if (ev == BSP_BTN_LONG) {
            ble_hid_key_press(HID_KEY_ESCAPE);
            ppt_timer_stop(&s_timer);
            update_timer_label();
            update_action(UI_FUI_PPT_ACTION_EXIT);
        }
        break;

    default:
        break;
    }
}
```

`update_action()` and `update_timer_label()` take `bsp_lvgl_lock(500)`, mutate the UI, and unlock.

`status_tick(lv_timer_t *)` reads `bsp_battery_soc()`, `ble_hid_is_connected()`, and `ble_hid_get_peer_str()`. All inside LVGL lock already.

`ppt_tick(lv_timer_t *)` calls `ppt_timer_tick` if `s_timer.running`, then `update_timer_label()`.

Implement the file fully with all required includes:

```c
#include "app_ppt.h"

#include "ble_hid.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_lvgl.h"   /* provided by bsp_display.h transitively */
#include "ppt_keys.h"
#include "ppt_timer.h"
#include "ui_fui_ppt.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

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
```

The implementation must compile to a single `app_main_ppt()` entry point. The helpers `ui_fui_ppt_set_state_color` and `update_state_color_only` declared above need to be added to `main/ui_fui_ppt.{h,c}`:

In `main/ui_fui_ppt.h`:
```c
void ui_fui_ppt_set_state_color(ui_fui_ppt_t *ui, uint32_t color);
```

In `main/ui_fui_ppt.c` (inside the LVGL block):
```c
void ui_fui_ppt_set_state_color(ui_fui_ppt_t *ui, uint32_t color) {
    if (!ui || !ui->state_label) return;
    lv_obj_set_style_text_color(ui->state_label,
                                lv_color_hex(color), 0);
}
```

And the host stub block:
```c
void ui_fui_ppt_set_state_color(ui_fui_ppt_t *ui, uint32_t color) {
    (void)ui; (void)color;
}
```

Add those edits and re-run the layout test from Task 5 to confirm it still passes.

- [ ] **Step 3: Verify file syntactically**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
gcc -std=c11 -Wall -Wextra -fsyntax-only -Imain main/app_ppt.c 2>&1 | tail -10
```

This will fail because ESP-IDF / LVGL / FreeRTOS are not on the host include path. That is expected. The firmware gate in Task 9 builds the actual integration.

- [ ] **Step 4: Commit**

```bash
git add main/app_ppt.h main/app_ppt.c main/ui_fui_ppt.h main/ui_fui_ppt.c
git commit -m "feat(app): glue layer wiring BSP, BLE HID, timer, and FUI UI

app_main_ppt() initializes BSP and the application. on_key() runs in
the button task and dispatches HID actions plus UI feedback." 2>&1 | tail -3
```

---

## Task 8: main.c + CMakeLists.txt integration

**Files:**
- Modify: `main/main.c` — branch on `CONFIG_APP_PPT_ENABLED`
- Modify: `main/CMakeLists.txt` — register new sources and font file

- [ ] **Step 1: Inspect the existing `main/CMakeLists.txt`**

```bash
cat "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base/main/CMakeLists.txt"
```

Note the existing `idf_component_register` call. We must not remove its current sources.

- [ ] **Step 2: Update `main/CMakeLists.txt`**

Add the new sources to the `SRCS` list (or via `target_sources` after registration, depending on the existing style). Match the existing indentation.

Append to the existing list:

```cmake
    "app_ppt.c"
    "ble_hid.c"
    "ppt_timer.c"
    "ui_fui_ppt.c"
    "fonts/kode_mono/ui_font_kode_regular_11.c"
    "fonts/kode_mono/ui_font_kode_regular_13.c"
    "fonts/kode_mono/ui_font_kode_bold_13.c"
    "fonts/kode_mono/ui_font_kode_bold_15.c"
    "fonts/kode_mono/ui_font_kode_bold_21.c"
```

Verify the actual filenames under `main/fonts/kode_mono/` match the list above. Adjust if upstream used different names.

- [ ] **Step 3: Update `main/main.c`**

The simplest change is to wrap the entire demo body in `#if !CONFIG_APP_PPT_ENABLED` and add `#else` branch that calls `app_main_ppt()`:

```c
#include <stdio.h>
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_lvgl.h"
#include "esp_log.h"

#if CONFIG_APP_PPT_ENABLED
#include "app_ppt.h"
#endif

void app_main(void) {
#if CONFIG_APP_PPT_ENABLED
    app_main_ppt();
#else
    /* original demo menu body */
    ...
#endif
}
```

Concretely: open `main/main.c`, locate the existing `app_main()` function, and:

- Add `#if CONFIG_APP_PPT_ENABLED` / `#include "app_ppt.h"` / `#endif` near the top.
- Wrap the existing `app_main()` body inside `#if !CONFIG_APP_PPT_ENABLED` … `#else` … `app_main_ppt();` … `#endif`.

Do not delete any existing `#include` or call. The original demo remains compilable when `CONFIG_APP_PPT_ENABLED=n`.

- [ ] **Step 4: Sanity-check the diff**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
git diff main/main.c 2>&1 | head -40
git diff main/CMakeLists.txt 2>&1 | head -40
```

Expected: small, focused diffs. If anything looks larger than expected, stop and review.

- [ ] **Step 5: Commit**

```bash
git add main/main.c main/CMakeLists.txt
git commit -m "build(main): gate app_main on CONFIG_APP_PPT_ENABLED, register new sources

When APP_PPT_ENABLED=y, main calls app_main_ppt(); otherwise the original
demo menu runs." 2>&1 | tail -3
```

---

## Task 9: Validation gate

**Files:**
- No new files
- Run: `./tools/validate.sh --static`, then `--firmware` (if ESP-IDF available)

- [ ] **Step 1: Run the static gate**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
./tools/validate.sh --static 2>&1 | tail -50
```

Expected: every check passes. Inspect each line; if any host test fails, fix and re-run.

If the static gate fails because the validator does not know about our new tests, the validator likely picks up `tests/test_*.c` automatically. Confirm by checking `tools/validate.sh` for the host-tests loop.

- [ ] **Step 2: Run the firmware gate (only if ESP-IDF is available locally)**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
source <path-to-esp-idf-v5.5.3>/export.sh
./tools/validate.sh --firmware 2>&1 | tail -60
```

Expected:
- Build succeeds for esp32c3.
- `build/FoloToy-AI-Passport-full.bin` is produced.
- The merged image passes the offset / partition-table / flash_args validation in the gate.

If the build fails, read the first error from the top of the output (not the bottom), fix the root cause, and re-run. Common first-time issues:

- Missing `idf_component.yml` entries for `esp_hid` or `esp_hidd_gatts`. The upstream repo's `idf_component.yml` must include `espressif/esp_hid` as a managed dependency.
- `CONFIG_LV_FONT_MONTSERRAT_14` not enough; the gate may need `CONFIG_LV_FONT_MONTSERRAT_20` plus our Kode Mono registrations.
- A LVGL API mismatch between LVGL 9.5 and the version pinned by `dependencies.lock`.

After a clean run, verify the merged image:

```bash
ls -l build/FoloToy-AI-Passport-full.bin 2>&1
python3 -c "import hashlib;print(hashlib.sha256(open('build/FoloToy-AI-Passport-full.bin','rb').read()).hexdigest())" 2>&1
```

Record the file size and SHA-256 for the final report.

If ESP-IDF is NOT available locally, record:

```text
Firmware gate: NOT RUN locally — pushed to GitHub Actions in Task 10.
```

- [ ] **Step 3: If the firmware gate passed locally, archive the build**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
python3 tools/archive_firmware.py verify build/firmware/$(ls -t build/firmware | head -1) 2>&1 | tail -10
```

Expected: each archived file verifies against the manifest.

- [ ] **Step 4: Commit (no changes expected)**

```bash
git status --short
```

If empty, no commit needed. If there are unexpected changes (e.g. a regenerated `sdkconfig` that slipped in), stop and decide whether to commit or revert.

---

## Task 10: Push branch, run CI, deliver merged image

**Files:**
- No new files
- GitHub: PR or push to the new repo

- [ ] **Step 1: Push the branch to the delivery repo**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
git push -u origin-fui feature/ppt-controller-fui 2>&1 | tail -10
```

Expected: branch appears at `https://github.com/<user>/ai-passport-ppt-fui`.

- [ ] **Step 2: Confirm CI runs**

```bash
gh repo view ai-passport-ppt-fui --json name 2>&1 | tail -3
gh run list --repo <user>/ai-passport-ppt-fui --limit 5 2>&1
```

If no run exists, manually trigger via `gh workflow run firmware-checks.yml --repo <user>/ai-passport-ppt-fui 2>&1 | tail -5` and `gh workflow run static-checks.yml --repo <user>/ai-passport-ppt-fui 2>&1 | tail -5`.

Wait for both runs to finish:

```bash
gh run watch --repo <user>/ai-passport-ppt-fui 2>&1 | tail -30
```

Expected: both workflows exit 0.

If a run fails, fetch the logs:

```bash
gh run view <run-id> --repo <user>/ai-passport-ppt-fui --log 2>&1 | tail -80
```

- [ ] **Step 3: Download the verified firmware artifact**

```bash
gh run download <firmware-run-id> --repo <user>/ai-passport-ppt-fui --name firmware-<run-id> --dir dist 2>&1 | tail -5
ls -l dist/FoloToy-AI-Passport-full.bin 2>&1
```

Record the SHA-256:

```bash
python3 -c "import hashlib;print(hashlib.sha256(open('dist/FoloToy-AI-Passport-full.bin','rb').read()).hexdigest())" 2>&1
```

- [ ] **Step 4: Move the merged image into the project `build/` directory**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
mkdir -p build
cp dist/FoloToy-AI-Passport-full.bin build/
```

Note: `build/` is gitignored. The file is local-only.

- [ ] **Step 5: Tag the release**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
git tag -a v0.1.0-fui -m "PPT controller FUI v0.1.0 — initial delivery"
git push origin-fui v0.1.0-fui 2>&1 | tail -3
```

If `build-firmware.yml` is configured for tag-triggered releases, the tag push creates a GitHub Release with the merged image attached.

- [ ] **Step 6: Commit any final docs**

If the final report deserves a `docs/CHANGELOG.md` entry, follow AGENTS.md: ordinary feature PRs do not edit `docs/CHANGELOG.md`. The release maintainer aggregates changelog entries during release prep. So we DO NOT add a changelog entry here.

- [ ] **Step 7: Capture the final report**

Compose the report exactly in the AGENTS.md format and append it to the PR description:

```text
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: <list the items the user must verify manually>
```

Fill each field:

- **Build**: `PASS` if `validate.sh --firmware` succeeded locally or in CI; `NOT RUN` if neither ran.
- **Host tests**: `PASS` if `validate.sh --static` passed locally.
- **Device tests**: `NOT RUN` — no hardware in this environment.
- **Unverified**:
  - Real BLE pairing with Windows / macOS / Linux hosts (needs hardware).
  - Battery gauge accuracy (depends on cell profile).
  - RSSI accuracy (no driver hook on ESP32-C3 without custom code).
  - Real-device Chinese glyph rendering — NOT APPLICABLE (UI is English-only).
  - Long-press timing tolerance (500 ms vs the BSP default 1500 ms).
  - Simulator screenshot at `https://folotoy-passport-simulator.onrender.com` — Task 11 covers this separately.

---

## Task 11: Simulator verification

**Files:**
- No new code files
- Browser session on `https://folotoy-passport-simulator.onrender.com`

- [ ] **Step 1: Open the simulator URL**

```bash
start "" "https://folotoy-passport-simulator.onrender.com" 2>&1 | tail -3
```

(or paste the URL into a browser.)

- [ ] **Step 2: Upload the merged image**

Drag `build/FoloToy-AI-Passport-full.bin` (or the file at `dist/FoloToy-AI-Passport-full.bin`) into the simulator's upload area. Follow the on-screen instructions.

- [ ] **Step 3: Capture the result**

The simulator renders the application's UI from the merged image's metadata. Screenshot the result and save it to `docs/superpowers/specs/2026-09-28-ppt-controller-fui-simulator.png`.

If the simulator fails to render (e.g. it expects a different image layout), record the simulator's error message verbatim and report it as `Unverified`.

- [ ] **Step 4: Final summary**

Append to the final report a line:

```text
Simulator render: PASS / FAIL / NOT RUN (with notes)
```

- [ ] **Step 5: Final commit and push (if anything was added in this task)**

```bash
cd "/c/Users/Administrator/Desktop/PPT controller/ai-passport-base"
git add docs/superpowers/specs/2026-09-28-ppt-controller-fui-simulator.png
git commit -m "docs(simulator): capture PPT controller FUI simulator render result" 2>&1 | tail -3
git push origin-fui feature/ppt-controller-fui 2>&1 | tail -3
```

---

## Self-Review (filled in by the planner before saving)

1. **Spec coverage**:
   - §3 BSP boundary — Task 8 (no BSP changes).
   - §4 BLE HID design — Task 6 (port + adaptation).
   - §4.1 sdkconfig additions — Task 2 (sdkconfig.defaults.ppt-controller).
   - §5 key mapping — Tasks 3, 6, 7.
   - §6 slide timer — Task 4 (TDD with 6 host-test cases).
   - §7 UI layout — Task 5 (layout math + LVGL widgets).
   - §7.6 action feedback states — Task 7 (update_action dispatches).
   - §8 lifecycle — Task 7 (app_main_ppt).
   - §9 partition table — Task 2 (no change).
   - §10 validation — Task 9 (local) + Task 10 (CI).
   - §11 risks — covered implicitly across tasks.
   - §12 file list — produced by Tasks 1–8.
   - §13 open questions — none.
2. **Placeholder scan**: No TBD/TODO/"implement later" remains.
3. **Type consistency**: `ui_fui_ppt_t *`, `ppt_timer_t`, `ble_hid_*` signatures consistent across tasks. `ui_fui_ppt_set_state_color` introduced in Task 7 with explicit edits to Task 5 files called out.
4. **Spec → task mapping**: every spec section has at least one task.