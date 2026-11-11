# ESP32-S3 蓋節點（stc_node）

讀者：要燒錄節點、改腳位或改控制幀的人。何時讀：第一次上電、換 PCB 腳位、或懷疑 ESP-NOW 不通時。

節點只做三件事：收 S31 經 ESP-NOW 送來的 `LID_CMD`、用 GPIO 的 LEDC PWM 開／關對應的蓋、每 100 ms 回一次 `NODE_STATE`（四粒 TOF 在該視窗內的最小距離）。

## 拓撲（2026-11 硬件改版）

| 項目 | 值 | 在哪裡改 |
|---|---|---|
| 舵機 0（可回收物） | GPIO 4，LEDC 50 Hz | `idf.py menuconfig` → STC node → Servo pins |
| 舵機 1（廚餘） | GPIO 5 | 同上 |
| 舵機 2（有害） | GPIO 6 | 同上 |
| 舵機 3（其他） | GPIO 7 | 同上 |
| 舵機閉合脈寬 | 1000 µs（預設） | `STC_SERVO_CLOSED_US` |
| 舵機開啟脈寬 | 1500 µs（預設） | `STC_SERVO_OPEN_US` |
| TOF XSHUT 0–3 | GPIO 15 / 16 / 17 / 18 | `idf.py menuconfig` → STC node → TOF050C XSHUT pins |
| I²C | SDA GPIO 8、SCL GPIO 9、400 kHz、7-bit 地址 | `main/node_main.c` 的 `NODE_SDA` / `NODE_SCL` |
| 距離感測器 | 4 × TOF050C（VL6180X），同掛一條 I²C | `main/tof.c` |
| 感測器驅動 | `nestorit/vl6180x` **v1.0.0**（ESP Component Registry，`main/idf_component.yml`，約束 `^1.0.0`） | `main/idf_component.yml` |
| 感測器地址 | `0x30`–`0x33`（桶 0–3）；上電預設 `0x29` | `main/tof.h` 的 `TOF_FIRST_ADDR` |
| 取樣 | 連續測距，20 ms／粒 = **50 Hz** | `main/tof.c` 的 `TOF_PERIOD_MS` |
| ESP-NOW | Wi-Fi STA、channel 1（預設） | `idf.py menuconfig` → STC node → `STC_NOW_CHANNEL` |
| 藍牙 | 關 | `sdkconfig.defaults` 的 `CONFIG_BT_ENABLED=n` |

四粒 VL6180X 出廠都在 `0x29`，所以開機順序是：四條 XSHUT 全部拉低 → 逐粒拉高放行 → 由 `nestorit/vl6180x` 建立裝置 → `vl6180x_set_address()`（寫 `0x212 I2C_SLAVE__DEVICE_ADDRESS`）改成 `0x30 + i` → 開連續測距。改址只在通電期間有效，所以每次上電都要重跑。

## 感測器取樣：為什麼是「視窗最小值」

卡片從桶口掉下去是一次性暫態：自由落體 10 cm 約 **143 ms**、15 cm 約 **175 ms**（`t = √(2h/g)`）。舊版每 100 ms 才輪詢一次，等於每粒 10 Hz，落下常常只落在 1–2 個取樣點之間。

現在：

| 設定 | 值 | 效果 |
|---|---|---|
| `SYSRANGE__INTERMEASUREMENT_PERIOD`（`0x01B`） | `0x02` = 20 ms | 週期 20 ms |
| `SYSRANGE__MAX_CONVERGENCE_TIME`（`0x01C`） | `0x0C` = 12 ms | 符合 datasheet 2.4.4 的 `max_conv + 5 ≤ period × 0.9`（17 ≤ 18） |
| 中斷模式（元件 `vl6180x_range_start_continuous()` 設定） | new sample ready | `RESULT__INTERRUPT_STATUS_GPIO`（`0x04F`）bit 2 = 有新樣本 |
| 主迴圈輪詢 | 5 ms（200 Hz） | 每個樣本都會被看到 |
| `NODE_STATE` | 100 ms | 每桶送「這個視窗內看過的最小有效距離」，0 = 視窗內沒有有效樣本 |

有效性只認 `RESULT__RANGE_STATUS`（`0x04D`）的 bits [7:4] 錯誤碼 = 0（datasheet Table 8）；不是 0 的樣本會被丟棄，所以 `tof_mm` 不會出現上一輪的舊值。

**殘餘限制**：VL6180X 的 FoV 是 25°，在 100 mm 只有約 **44 mm** 的視野直徑。卡片如果剛好從光斑外緣掉下去，這版程式再快也看不到——加大取樣率只減少漏檢，不能保證偵測。要真正解決需要硬件層面的答案（加蓋／改變感測器安裝幾何或數量），不是韌體能補的。

## 控制幀與 CRC

S31 與節點編譯同一份 `firmware/common/stc_lidmsg.h`，固定 16 bytes：

| 偏移 | 欄位 | 說明 |
|---|---|---|
| 0 | `sof` | 固定 `0xA5` |
| 1 | `ver` | 固定 `1` |
| 2 | `type` | 0 `CMD`、1 `ACK`、2 `NODE_STATE`、3 `NODE_FAULT` |
| 3 | `seq` | 命令序號，ACK 要原樣帶回 |
| 4 | `bin` | 0–3 |
| 5 | `action` | 0 `CLOSE_ALL`、1 `OPEN` |
| 6–13 | `tof_mm[4]` | 四桶距離，mm，小端 |
| 14–15 | `crc16` | CRC-16/CCITT-FALSE（poly `0x1021`、init `0xFFFF`），覆蓋前 14 bytes |

CRC 只用來判斷無線傳輸有沒有打壞；`stc_lidmsg_check()` 不過的幀直接丟掉，不回 ACK。ESP-NOW 本身會用對方 MAC 做單播，節點收到第一幀命令後記住該 MAC，之後 `NODE_STATE` 只單播回去。

## 第一次編譯與上板（完全沒用過 ESP-IDF 也照做）

### 1. 安裝 ESP-IDF 6.0.2

macOS 最省事的是官方安裝器：

```bash
git clone -b v6.0.2 --recursive https://github.com/espressif/esp-idf.git ~/.espressif/v6.0.2/esp-idf
cd ~/.espressif/v6.0.2/esp-idf
./install.sh esp32s3
```

`install.sh` 會自己下載 Xtensa 工具鏈與 Python 環境，跑幾分鐘；出現 `All done!` 就成功。

### 2. 每次開新終端都要「啟用」環境

```bash
. ~/.espressif/v6.0.2/esp-idf/export.sh
```

它只是設好 `PATH`、`IDF_PATH`，不會改你的專案。忘記做這步，之後的 `idf.py` 會是 `command not found`。

### 3. 編譯（專案自帶的入口）

在 repo 根目錄：

```bash
cd /path/to/smart-trash-can
python tools/dev.py compile --project stc_node
```

它等於：

```bash
idf.py -C firmware/esp32s3/node -B build/idf/stc_node -D IDF_TARGET=esp32s3 build
```

成功時最後幾行是：

```
Project build complete. To flash, run:
 idf.py flash
```

產物是 `build/idf/stc_node/stc_node.bin`（連 bootloader 與分割表的下載參數都在 `build/idf/stc_node/flash_args`）。

首次編譯會建 `firmware/esp32s3/node/sdkconfig`（由 `sdkconfig.defaults` 生成，已 gitignore）；改過 menuconfig 之後想回預設，刪掉這個檔再編譯即可。

同一次編譯也會依 `main/idf_component.yml` 從 ESP Component Registry 下載 **`nestorit/vl6180x` v1.0.0**（VL6180X 驅動）到 `firmware/esp32s3/node/managed_components/`，並寫出 `dependencies.lock` 鎖版本。所以第一次編譯要有網路；之後離線也能編，因為檔案已在 `managed_components/`（已 gitignore，換機器要重新下載一次）。

### 4. 插上板子、燒錄、看 log

用 USB 線接上 S3 的 USB 埠（板上有兩個 USB-C 時，選標 `USB`／`UART` 那個）：

```bash
ls /dev/cu.*                     # macOS 找埠，通常是 /dev/cu.usbserial-* 或 /dev/cu.usbmodem*
idf.py -C firmware/esp32s3/node -p /dev/cu.usbmodem1101 flash monitor
```

- `-p` 換成你上面看到的埠；Linux 是 `/dev/ttyUSB0` 或 `/dev/ttyACM0`。
- `flash` 燒錄，`monitor` 直接開序列埠 log。離開 monitor 是 `Ctrl+]`。
- 第一次燒錄若卡在 `Connecting...`，按住板上的 `BOOT` 再按一下 `RST`。

### 5. 改腳位

```bash
idf.py -C firmware/esp32s3/node menuconfig
```

進去後走 `STC node` 這一頁：`Servo pins (one GPIO per lid)` 改四支舵機腳與開合脈寬，`TOF050C XSHUT pins` 改四條 XSHUT 腳，`STC_NOW_CHANNEL` 改 ESP-NOW channel（要和 S31 相同）。存檔離開後重跑第 3 步的編譯指令；設定會寫進 `firmware/esp32s3/node/sdkconfig`。

I²C 的 SDA／SCL 不在 menuconfig 裡，直接改 `main/node_main.c` 的 `NODE_SDA`、`NODE_SCL`。

### 6. 成功的開機 log

```
I (312) tof: bin 0: VL6180X at 0x30, continuous 20 ms (50 Hz)
I (318) tof: bin 1: VL6180X at 0x31, continuous 20 ms (50 Hz)
I (324) tof: bin 2: VL6180X at 0x32, continuous 20 ms (50 Hz)
I (330) tof: bin 3: VL6180X at 0x33, continuous 20 ms (50 Hz)
I (356) now_link: node mac 24:0a:c4:12:34:56 ch=1
I (356) stc_node: node up: i2c sda=8 scl=9, esp-now ch=1
```

判讀：`VL6180X at 0x30…0x33` 四行都有 → 四粒都改址成功；出現 `tof fault mask 0x0?` 表示對應桶（bit 0–3）那粒沒回應，先查 XSHUT 腳與 I²C 接線；`node mac` 是 S31 要填的節點 MAC。

## 上板未知項

舵機實際開合角度、蓋子行程與 TOF 在實桶內的讀值都要上板量；本目錄的代碼只保證在 ESP-IDF 6.0.2 / esp32s3 下編譯通過。
