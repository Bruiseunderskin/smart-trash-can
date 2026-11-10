# 整合執行時（integration runtime）

讀者：韌體整合與上板驗證人員。何時讀：接線、除錯開機流程、或看故障碼前。

本文只寫「本 repo 已實作」與「仍未上板」的分界，數字未實測一律寫 `未測`。

## 啟動次序（實際實作順序）

1. NVS 初始化
2. `stc_mode_init()` → `STC_ST_BOOT`
3. `stc_lid_link_init()`（CDC 傳輸注入）；節點失敗只記 fault，不阻開機
4. `stc_ui_create()` → BOOT 畫面
5. `stc_vision_map_load_labels("models/wordless-padded/labels.txt")`，失敗即 `STC_ERR_CONTRACT`
6. `stc_game_init()` ＋ `stc_game_set_config()`
7. `stc_mode_input(STC_IN_PLAY, 0)` → `STC_ST_GAME_IDLE`

## 失敗碼 → 畫面文字

| 碼 | 來源 | 畫面文字 |
|---|---|---|
| `STC_ERR_CONTRACT` | `labels.txt` 次序不符／`classes.json` 版本不符 | 「模型同契約唔夾」 |
| `STC_ERR_IO` | 幀來源無效幀、CDC 無 ACK、TOF 無讀值 | 「睇唔到／蓋門無反應」 |
| `STC_ERR_TIMEOUT` | 掃描或開蓋逾時 | 「再試一次」 |
| `STC_ERR_UNSUPPORTED` | 未接硬件路徑（相機、stub lid） | 「此機未支援」 |

## 蓋與感測鏈路

- S31（USB host）↔ S3 節點（USB Serial/JTAG，CDC-ACM），16 bytes 幀、CRC-16/CCITT-FALSE
- `LID_CMD` 300 ms 內無同 `seq` 的 `LID_ACK` → 重試 2 次 → `STC_ERR_IO`
- 節點每 100 ms 送 `NODE_STATE`；主機 500 ms 未收到即標離線
- I²C：SDA GPIO8、SCL GPIO9、400 kHz；PCA9685 `0x40`、TCA9548A `0x70`、TOF 全部 `0x29`

## 電源預算（設計值，非實測）

| 項目 | 數值 | 狀態 |
|---|---|---|
| 節點常態 | 210 mA | 設計值，`未測` |
| 節點上限 | 500 mA | 設計值，`未測` |
| 舵機 | 2.8 A 獨立供電 | 設計值，`未測` |

## 上板餘下步驟

1. 燒錄 `firmware/esp32s3/node`（`idf.py -C firmware/esp32s3/node -D IDF_TARGET=esp32s3 build flash`）
2. 燒錄 S31 bring-up，跑 `t2_camera` 並貼 `RESULT T2 PASS|FAIL` 到 issue #11
3. 量整條 p95（影相→判類→開蓋）與模型 p95，分五個階段記
4. 斷網連續 10 次、四蓋實際到位
