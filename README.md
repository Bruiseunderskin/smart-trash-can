# Smart Trash Can（智能垃圾桶）

分類小隊：ESP32-S31 智能垃圾桶攤位機。四路舵機控制桶蓋、板載 4.3 吋 LVGL 教學 UI、Edge Impulse 垃圾分類模型，配合 xiaozhi-esp32 語音助手固件。

## 文件地圖

**所有文件先在 [`docs/INDEX.md`](docs/INDEX.md) 登記**：邊份現行、邊份已被取代、幾時核實。新文件請加一行，被取代的不要刪，改狀態。

| 想知什麼 | 睇邊份 |
|---|---|
| 文件全貌與現況 | `docs/INDEX.md` |
| UI 規格與狀態機 | `design/issue-3.md`、`ui/lvgl/README.md` |
| 模型現況（Edge Impulse） | `docs/INDEX.md` 第 3 節 |
| 延遲量測（ESP-NN A/B） | `docs/issue6/report.md` |
| YOLO／esp-dl 替代路線 | `docs/issue8/report.md` |
| 環保問答教材 | `deliverables/environmental_qa_upload_pack/README.md` |

> 硬件註記：部署目標是 **ESP32-S31**（RISC-V @320 MHz、PIE/QACC SIMD、ESP-NN 已支援）。舊文件若寫 ESP32-S3，屬當時的量測記錄，未逐項核實，不要直接當 S31 數字用。

## 目錄結構

| 路徑 | 說明 |
|---|---|
| `ui/lvgl/` | 分類小隊 4.3 吋 800×480、LVGL 9.5、九狀態、四類圖鑑、學習頁、完成總分頁、SDL 桌面預覽 |
| `design/` | Figma 匯出 tokens／layout、教學文案、既有資產與授權、issue #3 修訂規格 |
| `docs/` | 文件索引、模型比較、issue 報告、量測數據 |
| `deliverables/` | 對外教材包（環保問答，md／docx／pptx／xlsx／epub／pdf） |
| `esp32s3/` | 板載專案（tflite probe、waste classifier） |
| `libraries/`、`tools/` | 串口屏庫（擱置）、資料集與量化腳本 |
| `arduino/MG90_LidServo_ESP32S3/` | Arduino 示例：MG90S 舵機控制桶蓋（GPIO9，串口 `o`/`c`/`s`/角度） |
| `arduino/ESP32S3_Mic_Test/` | Arduino 示例：按小智板型 GPIO4/5/6 錄音、統計電平、回放 |
| `arduino/PCA9685_4Servo_Test/` | 四路舵機測試 |
| `xiaozhi-esp32/` | 語音助手固件，來源 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) |
| `xiaozhi-esp32/main/boards/smart-trash-can/` | 自訂板型：四路舵機桶蓋 ＋ `self.trash_can.*` MCP 工具 |

## 硬件接線

- 舵機信號線（橙／黃）→ 可回收物 `GPIO9`、廚餘 `GPIO10`、有害 `GPIO11`、其他 `GPIO12`
- 舵機電源（紅）→ 外部 5V 正極（**不要**用 3.3V）
- 舵機地線（棕／黑）→ 外部 5V 負極，並與主板共地

詳細接線、角度約定與 MCP 工具說明見 [`xiaozhi-esp32/main/boards/smart-trash-can/README.md`](xiaozhi-esp32/main/boards/smart-trash-can/README.md)。

## 編譯固件

```bash
cd xiaozhi-esp32
python scripts/build.py smart-trash-can
idf.py -p /dev/ttyUSB0 flash monitor     # macOS: /dev/cu.usbserial-*
```

## 建置產物

以下不入版本控制（可用官方工具鏈重新生成）：

- `xiaozhi-esp32/build/`
- `xiaozhi-esp32/managed_components/`
- `xiaozhi-esp32/sdkconfig`、`dependencies.lock` 等本地設定

## 4.3 吋教學 UI

[Figma 可編輯設計](https://www.figma.com/design/g5J7a5DhDuX1UYucR6jitn) 與 [LVGL 使用／預覽說明](ui/lvgl/README.md)。

桌面預覽支援九個狀態及四類學習卡。`ui/lvgl` 是可直接加入 ESP-IDF 的純 LVGL C component。**桌面檢查不代表**顯示、觸控、相機、AI 或舵機已在板上通過。
