# S31-Korvo-1 GPIO 預算（結論：0 空閒）

狀態：**已查證**，2026-11-04。作者：阿吉 the hermes（代 W Ed / sillysben）。
關聯文件：`docs/lid-detect-research.md`（未決項 L3）、`docs/sensor-node-wiring.md`（解法）。

---

## 0. 一句話結論

**ESP32-S31-Korvo-1 冇任何可用的備用 GPIO，而且冇任何排針／引出焊盤。**
所有感測器與舵機必須交給另一塊板（節點），S31 只做「眼＋腦」。

---

## 1. 晶片與模組層面

| 項目 | 數字 | 出處 |
|---|---:|---|
| 模組型號 | ESP32-S31-WROOM-3（16 MB flash / 16 MB PSRAM） | 模組 datasheet v0.7 §1.2 |
| 模組可用 GPIO | **54** | 模組 datasheet 首頁 "54 GPIOs" |
| 晶片 GPIO 編號 | GPIO0–40、GPIO42–61（**GPIO41 不存在**） | 晶片 datasheet v0.5 §2.2 Pin Overview |
| GPIO26–32 | 封裝內 flash／PSRAM 的 SPI 腳，**未引到模組引腳** | 模組 datasheet §2 Block Diagram |
| GPIO20–25 | SDIO（microSD／SPI NAND 共用） | 同上 |

54 = GPIO0–19（20）＋ GPIO20–25（6）＋ GPIO33–40（8）＋ GPIO42–61（20）。

---

## 2. 板上實際佔用（官方 Pin Assignment Table）

| 周邊 | GPIO | 條數 |
|---|---|---:|
| 音頻 I²S（MCLK/SCLK/LRCLK/SDOUT/DSIN） | 2, 3, 4, 5, 6 | 5 |
| 音頻／觸控／相機共用 I²C（SDA/SCL） | 0, 1 | 2 |
| 功放使能 PA_CTRL | 7 | 1 |
| WS2812 RGB LED | 37 | 1 |
| ADC 按鍵（4 鍵電阻分壓） | 42 | 1 |
| UART0（USB-UART 橋） | 58, 59 | 2 |
| microSD 4-bit（D0–D3/CLK/CMD） | 20, 21, 22, 23, 24, 25 | 6 |
| SD 電源控制（同時係 boot mode 1 strap） | 39 | 1 |
| RGB LCD 資料 D0–D15 | 8–19, 33, 34, 35, 36 | 16 |
| LCD CS（兼 boot mode 0）、PCLK（兼 boot 2）、DE、HSYNC、VSYNC | 38, 40, 43, 44, 45 | 5 |
| LCD SPI（MOSI/SCK，兼 boot mode 3/4） | 60, 61 | 2 |
| DVP 相機 D0–D7 | 46–53 | 8 |
| 相機 PCLK / XCLK / VSYNC / HREF | 54, 55, 56, 57 | 4 |
| **合計（去重）** | **54 / 54** | **54** |

GPIO41 不存在，26–32 不導出，因此 54 條已經係全部。

---

## 3. 板面層面

- 官方 user guide 元件清單只有：USB-C（電源）×1、USB-C（UART）×1、USB-A Host ×1、左右喇叭座、LCD 連接器、microSD 槽、相機連接器、4 個功能鍵、Reset/Boot。
- 全文搜 "header / breakout / test point / extension"：**0 次命中**。
- **V1.1 的 LCD 副板係疊在主板上**（V1.0 才是伸出板邊、露出板面功能），所以連焊點都摸唔到。

唯一未計入 54 條的是 4 個 JTAG 腳（MTDO／MTCK／MTDI／MTMS），不在排針上，不能當常規 GPIO 使用。

---

## 4. 「唔用 microSD 賺 6 條 GPIO」為甚麼唔成立

| 理由 | 說明 |
|---|---|
| 位置 | GPIO20–25 焊在 microSD 卡座內，要飛線 |
| 板面 | V1.1 副板疊住，難以施工 |
| Strapping | GPIO39（SD_CTRL）同時係 boot mode 1，唔可以當普通 GPIO |
| 風險 | 拆焊失敗 = 整塊板報廢，11/09 冇 buffer 等新板 |
| 回報 | 6 條仍然唔夠 8 個輸入 + 4 路 PWM |

---

## 5. 因此的架構決定

| 方案 | 可行性 | 備註 |
|---|---|---|
| 直接接 Korvo-1 | ✗ | 0 空閒 GPIO |
| 焊接飛線／拆卡座 | ✗ | 風險高、回報低 |
| **外掛節點（USB CDC 有線）** | ✓ **採用** | 見 `docs/sensor-node-wiring.md` |
| 外掛節點（ESP-NOW 無線） | ✓ 備選 | 需同頻道，多一個變數 |
| 換 ESP32-S31-Function-CoreBoard-1 | ✓ | repo 已有 BSP；只用了 I²S 52–56、I²C 50/51、PA 57、LED 60、BOOT 61，GPIO 大量空閒（前提：有排針引出） |

---

## 6. 出處

- ESP32-S31-Korvo-1 V1.1 User Guide（Pin Assignment Table、元件清單、V1.0/V1.1 差異）
  https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html
- ESP32-S31 Series Datasheet v0.5（§2.1 Pin Layout、§2.2 Pin Overview）
  https://documentation.espressif.com/esp32-s31_datasheet_en.html
- ESP32-S31-WROOM-3 / WROOM-3U Datasheet v0.7（"54 GPIOs"、Block Diagram、§3.2 Pin Description）
  https://documentation.espressif.com/esp32-s31-wroom-3_wroom-3u_datasheet_en.pdf
- esp-bsp `esp32_s31_korvo_1` API.md（I²C 0/1、SD 20–25 等巨集）
  https://github.com/espressif/esp-bsp/blob/master/bsp/esp32_s31_korvo_1/API.md
- 本 repo 板級設定：`xiaozhi-esp32/main/boards/espressif/esp32-s31-korvo-1/config.h`
