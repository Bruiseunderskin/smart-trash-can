# Smart Trash Can（智能垃圾桶）

基于 ESP32-S3 的智能垃圾桶项目：使用舵机控制桶盖开合，并结合 xiaozhi-esp32 语音助手固件实现语音交互。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `arduino/MG90_LidServo_ESP32S3/` | Arduino 示例：MG90S 舵机控制桶盖开合（GPIO9，支持串口命令 `o`/`c`/`s`/角度） |
| `arduino/ESP32S3_Mic_Test/` | Arduino 示例：按小智板型 GPIO4/5/6 录音、统计电平和回放，用于检查 I2S 麦克风 |
| `xiaozhi-esp32/` | 语音助手固件源码，来源为 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) |
| `xiaozhi-esp32/main/boards/smart-trash-can/` | 自定义板型「智能分类垃圾桶」：4 路舵机桶盖 + `self.trash_can.*` MCP 工具 |
| `ui/lvgl/` | 分類小隊：ESP32-S31 4.3 吋 800×480 LVGL 9.5 UI、九種狀態、四類圖鑑、學習頁、完成總分及 SDL 預覽 |
| `design/` | Figma 匯出 tokens／layout、教學文案、既有資產與授權、issue #3 修訂規格 |
| `.vscode/`、`.clangd` | 编辑器 / 语言服务器配置 |
| `esp32s31/bringup/` | **S31 實板自測**：LCD／Camera／Mic／Speaker／Loopback，一次跑一個，印 `RESULT ... PASS/FAIL/BLOCKED` |
| `esp32s3/tflite_probe/` | int8 模型延遲探測（ESP-NN on/off A/B），先在 S3 驗證 runtime |
| `docs/ROADMAP.md` | M0–M6 里程碑、交付物、叫停規則、砍功能順序 |

## 硬件接线

- 舵机信号线（橙/黄） → 可回收物 `GPIO9`、厨余垃圾 `GPIO10`、有害垃圾 `GPIO11`、其他垃圾 `GPIO12`
- 舵机电源（红） → 外部 5V 电源正极（不要用 ESP32-S3 的 3.3V）
- 舵机地线（棕/黑） → 外部 5V 电源负极，并与 ESP32-S3 共地

## 编译智能分类垃圾桶固件

```bash
cd xiaozhi-esp32
python scripts/build.py smart-trash-can
idf.py -p COM8 flash monitor
```

详细接线、角度约定与 MCP 工具说明见 [`xiaozhi-esp32/main/boards/smart-trash-can/README.md`](xiaozhi-esp32/main/boards/smart-trash-can/README.md)。

## 準備四類影像資料

```bash
python3.11 -m venv .venv-train
. .venv-train/bin/activate
pip install -r tools/requirements-train-macos-arm64.lock.txt
python tools/build_dataset.py --src data/garbage265 --out data/waste4 \
  --per-class-limit 2000 --test-per-class-limit 250 --seed 42
```

來源目錄須有 `train/`、`val/` 和 `classname.txt`。輸出包括四類 `train/val/test`、`dataset-report.json`、逐圖來源記錄 `labels.csv`、類別映射 `mapping.csv`，以及摘要 `report.md`。`--test-per-class-limit` 會限制每個目標類別的 held-out test 數量。

`--download-images` 只會從 ModelScope OSS 分段讀取所選圖片。它不會建立空白 cache、clone 資料集或下載 metadata；`--src` 仍須先包含 ModelScope 的 `classname.txt`、`train.csv`、`val.csv`。缺少 metadata 時，工具會列出缺少的檔案並停止。安裝下載功能依賴時，執行 `pip install -r tools/requirements-data.txt`。

鎖定檔固定本機報告使用的 Python 套件版本，並以 macOS arm64、Python 3.11 測試。重跑模型報告仍須提供相同資料集、權重和 Edge Impulse 匯出；這些大型檔案不納入版本控制。其他平台可使用固定主要套件版本的 `tools/requirements-train.txt`，但報告數字尚未在其他平台重現。

## S31 實板 Bring-up（issue #11）

```bash
cd esp32s31/bringup
idf.py set-target esp32s31
idf.py menuconfig      # Bring-up → Test to run → T1..T5
idf.py -p <port> build flash monitor
```

**已知 GPIO（來自上游 `esp32-s31-korvo-1/config.h`，排針實際引出腳位仍須對原理圖覆核）**

| 用途 | GPIO |
|---|---|
| 音頻 I2S | MCLK 2、BCLK 3、WS 4、DOUT 5、DIN 6 |
| Codec I2C | SDA 0、SCL 1 |
| 功放 PA | **7（唔拉高就冇聲）** |
| LCD RGB | VSYNC 45、HSYNC 44、DE 43、PCLK 40、DISP 38、DATA0–15 = 8–19、33–36 |
| Camera DVP | XCLK 55、PCLK 54、VSYNC 56、HREF 57、D0–D7 = 46–53 |
| 其他 | LED 37、BOOT 61、ADC 按鍵 42 |

舵機：4 × MG90S，**獨立 5V ≥ 4A 供電**、共地、1000 µF 電容，唔准由開發板 3.3V 取電。

## 當前 Issue 索引

| # | 內容 |
|---|---|
| [#2](https://github.com/Bruiseunderskin/smart-trash-can/issues/2) | S31 移植主線：腳位、固件、量化、整合 |
| [#11](https://github.com/Bruiseunderskin/smart-trash-can/issues/11) | Bring-up：四外設自測與通過門檻 |
| [#12](https://github.com/Bruiseunderskin/smart-trash-can/issues/12) | 模型驗證：去重、分組重訓、獨立現場圖片 |
| [#6](https://github.com/Bruiseunderskin/smart-trash-can/issues/6) | S3 上 esp-tflite-micro 延遲 probe |
| [#7](https://github.com/Bruiseunderskin/smart-trash-can/issues/7) / [#8](https://github.com/Bruiseunderskin/smart-trash-can/issues/8) | Edge Impulse baseline ／ Ultralytics → ONNX → esp-ppq → esp-dl |
| [#3](https://github.com/Bruiseunderskin/smart-trash-can/issues/3) / [#4](https://github.com/Bruiseunderskin/smart-trash-can/issues/4) | UI 規格 ／ UI 後續與實機整合 |

## 關於構建產物

以下内容**未纳入版本控制**（可通过官方工具链重新生成）：

- `xiaozhi-esp32/build/`
- `xiaozhi-esp32/managed_components/`
- `xiaozhi-esp32/sdkconfig`、`xiaozhi-esp32/dependencies.lock` 等本地配置

编译 `xiaozhi-esp32` 时，请参考上游仓库的说明安装 ESP-IDF 并运行 `idf.py build`。

## 4.3 吋教學 UI（ESP32-S31）

[Figma 可編輯設計](https://www.figma.com/design/g5J7a5DhDuX1UYucR6jitn) 與 [LVGL 使用／預覽說明](ui/lvgl/README.md)。

桌面預覽支援九個狀態及四類學習卡。`ui/lvgl` 是可加入 ESP-IDF 的純 LVGL C component；本次沒有改動小智板型或燒錄硬體。桌面檢查不代表顯示、觸控、相機、AI 或舵機已在板上通過。

## 已知限制（誠實記錄）

- **`esp32s31/bringup/` 未在任何機器編譯過**：無 ESP-IDF 的環境寫成，第一次 build 可能要修 1–2 個 API 欄位名。
- **ES8389 codec 未初始化**：`bringup_codec_init()` 只拉高 PA（GPIO7），所以 T3／T4／T5 會報 `BLOCKED`，唔會假裝 PASS。
- **Camera 未實作**：S31 走 esp-video（V4L2），`t2_camera.c` 內已列出要接的五個步驟。
- **Edge Impulse 沒有 ONNX 匯出**：只有 C++ library／TFLM／EON，所以「EI → ONNX → esp-ppq」不成立；要麼用 EI C++ library，要麼自己訓（見 #8）。
- **Arduino 與 Zephyr 都未支援 S31**（arduino-esp32 主幹 0 個 `s31` 檔、Zephyr 0 個），現階段只有 ESP-IDF 6.1。
