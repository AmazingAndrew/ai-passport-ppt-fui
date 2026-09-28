<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# Kode Mono UI 字体

> 完整内容请阅读英文版 [README.md](README.md)。本文件为仓库静态门禁 (`tools/check_repo.py`) 所需的简体中文对偶文件,提供要点摘要。

- **字体来源:** <https://github.com/isaozler/kode-mono>
- **许可证:** SIL Open Font License 1.1(完整文本随 `OFL.txt` 一起分发)

固件将可打印 ASCII 区间(`0x20-0x7E`)嵌入到 UI 字体家族所用的 5 个字号:

- Regular 11 — 元数据
- Regular 13 — 正文
- Bold 13 — 紧凑强调
- Bold 15 — 区块标题
- Bold 21 — 主状态

生成产物使用 LVGL 格式、4-bpp 抗锯齿、强小屏自动 hinting;因 Kode Mono 为等宽字体,不使用字距调整。源 TTF 的 SHA-256 见英文版。