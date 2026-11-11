# stc_node/main — 節點原始碼

讀者：改節點韌體的人。何時讀：加蓋、改腳位、改控制幀或調 TOF 讀值前。

| 檔案 | 責任 |
|---|---|
| `node_main.c` | `app_main`：建 I²C 匯流排（SDA 8 / SCL 9）、初始化舵機與 TOF、啟動 ESP-NOW、5 ms 迴圈（輪詢命令佇列 + 四粒 TOF）、每 100 ms 送 `NODE_STATE`（視窗最小值） |
| `servo.c/.h` | 四支舵機，各自一條 GPIO，LEDC 50 Hz、14-bit 解析度、脈寬 500–2500 µs |
| `tof.c/.h` | XSHUT 逐支放行 + `nestorit/vl6180x` 的薄層：改址成 `0x30 + i`、開 20 ms 連續測距、只在新樣本且 range status = 0 時回值 |
| `idf_component.yml` | 依賴宣告：`nestorit/vl6180x: "^1.0.0"`（ESP Component Registry，首次編譯自動下載） |
| `now_link.c/.h` | Wi-Fi STA + ESP-NOW：收幀、驗 CRC、記住對方 MAC、單播回覆 |
| `Kconfig.projbuild` | menuconfig 的 `STC node` 頁：舵機腳、XSHUT 腳、channel |
| `CMakeLists.txt` | 元件註冊；`stc_lidmsg.c` 直接從 `firmware/common` 編進來 |

## 資料流

```
ESP-NOW recv cb ── CRC 檢查 ──▶ queue ──▶ handle_command()
                                             ├─ action 0 → servo_close_all()
                                             └─ action 1 → servo_open(bin)
                                             └─ 回 ACK（原 seq）

每 5 ms：tof_poll(0..3)，把新有效樣本併進視窗最小值
每 100 ms：視窗最小值 → tof_mm → seal CRC → 單播 NODE_STATE，然後清零
```

接收 callback 在 Wi-Fi task 執行，所以只做 CRC 與入佇列（深度 4），舵機動作與 I²C 讀取都留 `app_main`，避免在 callback 裡阻塞。

## 幀

幀定義在 `firmware/common/stc_lidmsg.h`（16 bytes、小端、CRC-16/CCITT-FALSE 覆蓋前 14 bytes），S31 與節點編譯同一份。欄位順序見上一層 [README](../README.md#控制幀與-crc)。

## 改動時要注意

- 舵機與 XSHUT 腳位一律走 `CONFIG_STC_*`，不要在 `.c` 裡寫死；menuconfig 一改，`servo.c`／`tof.c` 的常數表自動跟著變。
- ESP32-S3 的 LEDC 最高 14-bit，所以 duty 用 `SERVO_DUTY_MAX = 16383` 換算（約 1.22 µs／格）。
- 地址 `0x30 + i` 只在通電期間有效；新增感測器就要重排 `TOF_FIRST_ADDR` 與 `TOF_COUNT`，並照 datasheet 2.4.4 重算 `TOF_PERIOD_MS`（`max_conv + 5 ≤ period × 0.9`）。
- 不要自己再寫一份 register map：初始化、連續模式、狀態解碼都在 `nestorit/vl6180x`；本層只留 XSHUT 與有效性判斷。
- 少一粒感測器不應該讓節點停機：`tof_init()` 失敗只回 `fault mask`，`node_main.c` 只印警告。
