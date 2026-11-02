# Smart Trash Can（智能垃圾桶）

基于 ESP32-S3 的智能垃圾桶项目：使用舵机控制桶盖开合，并结合 xiaozhi-esp32 语音助手固件实现语音交互。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `arduino/MG90_LidServo_ESP32S3/` | Arduino 示例：MG90S 舵机控制桶盖开合（GPIO9，支持串口命令 `o`/`c`/`s`/角度） |
| `arduino/ESP32S3_Mic_Test/` | Arduino 示例：按小智板型 GPIO4/5/6 录音、统计电平和回放，用于检查 I2S 麦克风 |
| `xiaozhi-esp32/` | 语音助手固件源码，来源为 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) |
| `xiaozhi-esp32/main/boards/smart-trash-can/` | 自定义板型「智能分类垃圾桶」：4 路舵机桶盖 + `self.trash_can.*` MCP 工具 |
| `.vscode/`、`.clangd` | 编辑器 / 语言服务器配置 |

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

## 关于构建产物

以下内容**未纳入版本控制**（可通过官方工具链重新生成）：

- `xiaozhi-esp32/build/`
- `xiaozhi-esp32/managed_components/`
- `xiaozhi-esp32/sdkconfig`、`xiaozhi-esp32/dependencies.lock` 等本地配置

编译 `xiaozhi-esp32` 时，请参考上游仓库的说明安装 ESP-IDF 并运行 `idf.py build`。
