# GitHub Actions

- [docs.yml](docs.yml)：push 和 PR 執行 README／連結檢查及 checker／開發 runner 的測試。
- [firmware.yml](firmware.yml)：相關來源改動時，以固定版本 Arduino core 與 ESP-IDF 編譯第一方 S3 入口及 smart-trash-can 板型；亦支援手動執行。
- [lvgl-ui.yml](lvgl-ui.yml)：UI、設計與預覽工具變動時，重建 Linux native UI，執行測試及核對 framebuffer。

韌體矩陣的建置命令共用 `tools/dev.py compile --project ...`。失敗必須修正或明確縮小覆蓋，不能將未通過的工作改為 continue-on-error。

[板型覆蓋表](../README.md) · [本機開發指南](../../docs/development.md) · [專案入口](../../README.md)
