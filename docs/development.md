# 開發檢查與編譯

所有命令在 repository root 執行；共用入口為 `python3 tools/dev.py`。

| 時機 | 命令 | 檢查 |
| --- | --- | --- |
| 啟用 hooks | `python3 tools/dev.py install-hooks` | 設定目前 checkout 的 core.hooksPath |
| commit 前 | `python3 tools/dev.py pre-commit` | whitespace、tracked README 及文件連結 |
| push 前 | `python3 tools/dev.py pre-push` | 快速檢查、工具測試及相關韌體編譯 |
| PR 前 | `python3 tools/dev.py pre-pr` | 同 push；`pr -- ...` 通過後可建立 PR |
| 單獨編譯 | `python3 tools/dev.py compile --project pca9685_servo_test` | ESP-IDF 編譯，不會燒錄 |

commit 前須將 tracked 變更全部 stage；push／PR 前須提交 tracked 變更，避免驗證其他版本的內容。

push／PR 的變更基準預設為本機 `origin/main`，使用前先 `git fetch origin main`；可用 `--base` 指定其他基準。腳本不會自行改動遠端分支或下載模型。

## 環境

- 文件檢查及技能測試：Python 3.12；主機工具及技能測試另需 `pip install -r tools/requirements-dev.txt`。
- Arduino：arduino-cli、Espressif boards URL 的 `esp32:esp32@3.3.12`，以及 `ESP32Servo@3.0.9`。三個 sketch 均以 `esp32:esp32:esp32s3` 編譯。
- S3 ESP-IDF：啟用 ESP-IDF 6.0.2 的 `export.sh`。TFLite probe 需先依其 README 準備模型；CI 下載公開 person-detect 模型並驗 SHA-256。
- Edge Impulse probe 需要使用者提供的模型。沒有模型時會停止，不能用 person-detect 替代它來宣稱模型相容。

## CI 範圍與缺口

Firmware compile 工作流程編譯三個 Arduino sketch、PCA9685 ESP-IDF 和公開 person-detect TFLite probe；既有 LVGL 工作流程負責 native UI 建置及渲染。文件 workflow 負責 README 與連結檢查。

S31 使用 ESP-IDF 6.1 的獨立 job，編譯 bring-up 與 XiaoZhi `esp32-s31-korvo-1`。本機使用對應 SDK，執行 `python3 tools/dev.py compile --project s31_bringup` 或 `--project esp32-s31-korvo-1`。S31 target 仍使用 preview 開關；runner 會拒絕低於 6.1 的 SDK。修改 XiaoZhi 程式時，push／PR 檢查會建置 S3 自訂板及 S31 Korvo 板，需分別具備相容 SDK（6.1 可涵蓋兩者，CI S3 維持固定 6.0.2）。

編譯 CI 不會操作硬件，不能證明馬達、麥克風、顯示屏、分類準確率或整機流程正常。
