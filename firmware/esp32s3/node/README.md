# ESP32-S3 蓋節點（stc_node）

讀者：韌體與硬件整合人員。何時讀：燒錄節點、改 I²C 腳位或 CDC 幀前。

節點只做三件事：收 S31 經 USB Serial/JTAG（CDC-ACM）來的 `LID_CMD`、驅動 PCA9685 開關蓋、每 100 ms 回一次 `NODE_STATE`（四粒 TOF 讀值）。

| 項目 | 值 |
|---|---|
| I²C | SDA GPIO8、SCL GPIO9、400 kHz |
| PCA9685 | `0x40`，50 Hz，CH0–CH3 對應桶位 0–3 |
| TCA9548A | `0x70`，每次只選一個通道；TOF 全部留在 `0x29` |
| TOF | 4 × TOF050C（VL6180X） |
| CDC | USB Serial/JTAG（內建，無需 tinyusb） |
| 無線 | **全關**（`CONFIG_ESP_WIFI_ENABLED=n`、`CONFIG_BT_ENABLED=n`） |

編譯：

```bash
idf.py -C firmware/esp32s3/node -D IDF_TARGET=esp32s3 build
```

上板實測（`RESULT`、四蓋動作、TOF 讀值）仍屬 Bruiseunderskin 的範圍，本目錄的代碼只保證編譯。
