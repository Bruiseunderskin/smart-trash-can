# Repository 自動檢查與覆蓋範圍

此 repository 同時含本專案韌體、實驗與 XiaoZhi 上游來源。CI 按入口編譯，並不等於每一種硬件已通過測試。

| 檢查 | 覆蓋來源 | 不代表甚麼 |
| --- | --- | --- |
| Documentation | 所有第一方目錄 README、行內本地連結、文件檢查工具測試 | 不檢查外部 URL 或內容正確性 |
| Firmware compile：Arduino | ESP32-S3 的三個測試 sketch | 不會燒錄或量測硬件 |
| Firmware compile：ESP-IDF | S3 PCA9685、公開 person-detect TFLite probe、XiaoZhi smart-trash-can 板型 | EI 私有模型、S31 和其他上游板型不在編譯矩陣 |
| LVGL native UI | SDL native 建置、互動測試及渲染基準 | 不等於實屏、相機、觸控或模型整合通過 |

XiaoZhi 來源含大量上游板型，不能把單一 S3 板型的成功當作全部板型相容。完整板型設定可在啟用 ESP-IDF 後，以 `cd xiaozhi-esp32`、`python scripts/build.py --list-boards` 列出；各板的 `config.json` 是建置變體來源。

S31 bring-up 尚有未完成依賴，EI probe 缺少公開可取得的指定模型，因此沒有假設它們通過。需要加入新板型時，先加入其建置設定及依賴，再於 workflow 矩陣加入對應入口，保留真實 CI 結果。

[工作流程](workflows/README.md) · [本機 commit／push／PR 檢查](../docs/development.md) · [架構邊界](../docs/architecture.md) · [專案入口](../README.md)
