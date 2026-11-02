# 韌體入口

ESP32-S31-Korvo-1 是整機目標板，ESP32-S3 保留作子系統測試及模型研究。每個子專案有獨立入口與 SDK 設定。

| 目錄 | 用途 | 編譯方式 |
| --- | --- | --- |
| [esp32s31](esp32s31/README.md) | S31 外設 bring-up | ESP-IDF 6.1：`python3 tools/dev.py compile --project s31_bringup` |
| [xiaozhi-esp32](xiaozhi-esp32/README.md) | 上游框架、自訂 S3 板及 S31 Korvo 板 | ESP-IDF 6.1：`python3 tools/dev.py compile --project esp32-s31-korvo-1` |
| [esp32s3](esp32s3/README.md) | PCA9685 及模型 probe | 依各專案指南使用 ESP-IDF |
| [arduino](arduino/README.md) | S3 伺服與麥克風測試 | Arduino IDE／arduino-cli |

表中 `tools/dev.py` 命令從 repository root 執行。不要在本目錄直接執行 idf.py。共用 runner 的獨立 IDF 建置輸出位於根目錄 `build/idf/<project>`；不是韌體來源。

原根目錄 `arduino/`、`esp32s3/`、`esp32s31/`、`xiaozhi-esp32/` 現在均位於 `firmware/` 下；更新書籤、IDE 與本機腳本。舊報告中的實驗路徑按其記錄時間解讀，模型／資料 hash 和測試 provenance 沒有因搬移而重算。

[架構與整合邊界](../docs/architecture.md) · [開發與 CI](../docs/development.md) · [專案入口](../README.md)
