<p align="right">
  <strong>简体中文</strong> · <a href="2026-09-28-ppt-controller-fui.md">English</a>
</p>

# PPT 控制器 FUI 实施计划 — 中文摘要

> 完整内容请阅读英文版 [2026-09-28-ppt-controller-fui.md](2026-09-28-ppt-controller-fui.md)。本文件为仓库静态门禁 (`tools/check_repo.py`) 所需的简体中文对偶文件,提供要点摘要以满足"每个维护中的 Markdown 文档必须有英文版与配对的 `.zh_CN.md`"的约定。

## 目标

在 FoloToy AI Passport(ESP32-C3、8 MB Flash、无 PSRAM、ESP-IDF v5.5.3)上构建一个 BLE HID Keyboard 形态的 PPT 遥控器应用,UI 采用 FUI 风格,交付可在 `0x0` 烧录的合并固件 `build/FoloToy-AI-Passport-full.bin`。

## 架构

新应用代码全部位于 `main/`,与基线 demo 文件并存;通过新增的 `CONFIG_APP_PPT_ENABLED` 选项切换入口。模块组成:

- `main/app_ppt.{h,c}` — 应用生命周期与事件分发
- `main/ble_hid.{h,c}` — 移植自 YeatsLiao/ai-passport-ppt 的 BLE HID 驱动
- `main/ppt_keys.h` — HID Usage ID 常量
- `main/ppt_timer.{h,c}` — 演讲计时器(纯逻辑,可 host test)
- `main/ui_fui_ppt.{h,c}` — 全新的 FUI UI,不沿用 demo
- `main/fonts/kode_mono/` — Kode Mono 字体源,取自 gesture-wand 同源

## 关键依赖

- ESP-IDF v5.5.3 + esp32c3
- LVGL 9.5(随 ESP-IDF)
- ESP-IDF `esp_hid` + Bluedroid(替换基线的 NimBLE)
- CW2017 电量计
- ST7789P3 SPI 显示
- Kode Mono(OFL 1.1)

## 任务清单(11 项)

| # | 任务 | 验证 |
| --- | --- | --- |
| 0 | 环境检查 + 5 个 skill 安装 + 创建 GH 仓库 | 分支 / idf.py / gh 状态 |
| 1 | 导入 Kode Mono 字体 + 资产登记 | 文件复制 + 许可证文本 |
| 2 | `Kconfig.projbuild` + `sdkconfig.defaults.ppt-controller` | `tools/validate.sh` 校验通过 |
| 3 | `main/ppt_keys.h` 常量头 | 头文件可独立编译 |
| 4 | `ppt_timer` + host test | `test_ppt_timer: PASS` |
| 5 | `ui_fui_ppt` + 布局 host test | `test_ui_fui_ppt_layout: PASS` |
| 6 | `ble_hid.{h,c}` 移植 | 结构检查 47/47 通过 |
| 7 | `app_ppt.{h,c}` 串接 | 结构检查 87/87 通过 |
| 8 | `main/main.c` + `CMakeLists.txt` | diff 仅含目标增量 |
| 9 | 运行 `--static` / `--firmware` 门禁 | exit 0;固件本地 NOT RUN |
| 10 | 推送分支 + GH Actions + 交付合并镜像 | CI green + 下载产物 |
| 11 | 在 folotoy-passport-simulator.onrender.com 测试 | 截图记录 |

## 验证与交付格式

```text
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: 仍需板卡、仪器或用户确认的事项
```

## 全局约束(继承自 AGENTS.md)

- 目标:ESP32-C3、8 MB Flash、无 PSRAM、ESP-IDF v5.5.3
- BSP 文件不可改
- LVGL 非线程安全;非 LVGL 任务访问 LVGL 必须持 `bsp_lvgl_lock()`
- 按键回调必须非阻塞
- 删除 demo 屏前必须停止所有可能访问其 UI 的任务/定时器/回调/事件处理器
- 不提交凭证、二维码秘密、私钥、个人数据、未脱敏日志
- 所有 Markdown 必有英文版 + 配对的 `.zh_CN.md`
- 不提交 `build/` 产物
- 二次开发 UI 强制重新设计:禁止沿用 `main/demo_*`、`main/ui_pixel*`、`main/demo_navigation*`