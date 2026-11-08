# 方案 v3：改用 TOF050C（VL6180X）量落卡

狀態：**取代 v2 的感測器選型，其餘架構不變** ｜ 2026-11-04 ｜ 阿吉 the hermes
v2（VL53L0X）保留作對照；`contracts/stc_bin.h` **不需要改**（介面只回 mm，與型號無關）

---

## 0. 為何換

4 粒 VL53L0X vs 4 粒 TOF050C：

- 量程：200 cm vs 50 cm — 我們只量 **8–15 cm**，多出的量程用不著
- FoV：兩者同為 **25°** — 桶間互見風險相同，換型號不改變
- 取樣：VL6180X 為近距設計，短距模式可更快（**對偵測快速落卡有利**）
- 驅動：VL6180X 有現成 ESP-IDF component（見 §6）；VL53L0X 用家更多
- 價：¥8 vs ¥12／粒（4 粒差約 HK$18）

**結論：未買，就買 TOF050C。** 但換型號不會令 G1（安裝幾何）變好——幾何仍是唯一可能推倒重來的風險。

## 1. 硬件與接線

| 項目 | 內容 |
|---|---|
| 感測器 | 4 × TOF050C（VL6180X），0–50 cm，盲區 0–2 cm，25°，I²C |
| 供電 | 3V3，**每粒 40 mA max → 4 粒 160 mA**，不可與舵機共用 5V rail |
| I²C | SDA 8 / SCL 9，400 kHz |
| 改址腳 | **SHDN**（非 XSHUT），每條 **10 kΩ 下拉到 GND** |
| 位址 | 預設 0x29 → 依序改為 0x2A、0x2B、0x2C、0x2D |
| 安裝 | 桶口下 **8–12 cm**（盲區 0–2 cm，故必須 ≥5 cm），光軸橫切落卡路徑 |

**改址順序（與 VL53L0X 不同，要留意）**

```
1. 四條 SHDN 全部拉低（全部關機）
2. 放開 SHDN#0 → 等 ≥1.5 ms → 它出現在 0x29
3. 寫 I2C_SLAVE_DEVICE_ADDRESS = 0x2A
4. 放開 SHDN#1 → 改 0x2B；如此類推
5. 全部完成後，逐粒做初始化與校正
```

**VL6180X 的關鍵差別**：SHDN 放開後**設定全部丟失**，每次上電／每次 SHDN 循環都要重寫量測設定（VL53L0X 亦有類似行為，但 VL6180X 更明顯）。初始化函式必須冪等、可重入。


### 1.1 已核實：SHDN 只係開關機，改址係另一個動作

查 ST 官方 datasheet（DocID026171 Rev 6／Rev 7）與 Pololu 文件，結論明確：

| 問題 | 事實 | 出處 |
|---|---|---|
| SHDN 係咩？ | VL6180X 的 **GPIO0**，預設為 **active-low shutdown input**；拉低 = hardware standby（<1 µA），唔使用時要接 AVDD | datasheet §2.2 |
| SHDN 會改地址嗎？ | **唔會**。SHDN 只控制開關機 | 同上 |
| 改地址靠咩？ | 寫 register **0x212 `I2C_SLAVE__DEVICE_ADDRESS`**（7-bit，預設 0x29） | datasheet §6.2.55 |
| 改完會保留嗎？ | **唔會**。Pololu 明文：「new address only applies until the sensor is reset or powered off」；斷電／reset 後回到 0x29 | Pololu 文件、datasheet 預設值 |

**datasheet §6.2.55 原文**：

```
I2C_SLAVE__DEVICE_ADDRESS   Address: 0x212   Type: R/W   Reset: 0x29
super_i2c_slave__device_address: User programmable I2C address (7-bit).
Device address can be re-designated after power-up.
```

即係「可以改」是官方明文，而且 `Reset: 0x29` 說明每次 reset 都回到 0x29 —— 這正好是逐粒隔離上電可行的原因。

**datasheet §2.3 原文**：

```
Hardware standby is the reset state (GPIO0=0). The device is held in reset
until GPIO0 is de-asserted. Note that the device will not respond to I2C
communication in this mode.
```

**實際程式佐證**（Pololu `vl6180x-arduino`）：

```cpp
void VL6180X::setAddress(uint8_t new_addr) {
  writeReg(I2C_SLAVE__DEVICE_ADDRESS, new_addr & 0x7F);
  if (last_status == 0) { address = new_addr; }
}
```

所以四粒共用一條 I²C 匯流排時，**SHDN 同寫 0x212 兩者缺一不可**（但用 TCA9548A 就兩者都不需要，見 §1.2）：SHDN 負責「隔離」，0x212 負責「分配」。單純拉 SHDN 唔會令四粒有唔同地址——佢哋上電後全部仍然係 0x29，直接衝突。

### 1.2 選型定案：TCA9548A（主）／4×SHDN 改址（後備）

兩條路線都可行（SHDN 改址已用 datasheet §6.2.55、§2.3 同 Pololu 實作核實）。比較實際工作量之後，**TCA9548A 明顯簡單**，因為省掉的不是接線，而是 firmware 裡一整類時序與狀態問題。

#### 工作量對比（不是形容詞，是行數）

| | TCA9548A | 4×SHDN ＋ 改址 |
|---|---|---|
| 新增程式 | 一個 `mux_select(ch)`（約 15 行）＋ 每次讀寫前呼叫 | 地址分配狀態機、boot 時序、讀回驗證、重試、身份重認、健康檢查（約 150–200 行） |
| 開機序列 | 無（四粒全部留在 0x29） | 逐粒「拉低→放開→等 400 µs→等 1 ms→寫 0x212→讀回→init」 |
| 主要故障模式 | mux 本身壞（單點） | 兩粒同時 boot 撞 0x29、地址寫入未生效、sensor 重啟後身份不明 |
| 調試難度 | 低：切 channel 就見到對應那粒 | 高：症狀是「有時得有時唔得」 |
| GPIO | 0（SDA/SCL 共用）＋ 4 條 SHDN 可省 | 4 條 |
| 額外成本 | ¥5、一個 I²C 地址（0x70） | 0 |
| 彈性 | 可同時開多個 channel（本專案用不到） | 需要時可以逐粒 power-cycle |

**結論：用 TCA9548A。** 省 4 條 GPIO、省 150–200 行程式、省一類「隨機失敗」的調試時間，代價 ¥5。

#### 接線

```
S3 ── I²C（SDA 8 / SCL 9, 400 kHz）── TCA9548A（0x70）
                                        ├─ CH0 → TOF#0（0x29）
                                        ├─ CH1 → TOF#1（0x29）
                                        ├─ CH2 → TOF#2（0x29）
                                        └─ CH3 → TOF#3（0x29）
四粒 SHDN 全部接 AVDD（不用；模組上若已接則保持高）
```

#### 程式骨架

```c
static i2c_master_dev_handle_t mux;   /* 0x70 */

static esp_err_t mux_select(uint8_t ch) {
    uint8_t mask = (uint8_t)(1u << ch);      /* 一次只開一個 channel */
    return i2c_master_transmit(mux, &mask, 1, 100);
}

esp_err_t tof_read_mm(uint8_t ch, uint16_t *out_mm) {
    ESP_RETURN_ON_ERROR(mux_select(ch), TAG, "mux");
    return vl6180x_read_range(TOF_ADDR_DEFAULT, out_mm);   /* 永遠 0x29 */
}
```

#### 必須留意的三點

1. **上電後 mux 預設無 channel 開啟**——四粒天然被隔離，不會撞地址；這是它最大的優點
2. **每次切 channel 後加 ≥100 µs 穩定時間**（便宜 mux 板有寄生電容），再讀距離
3. **`FRESH_OUT_OF_RESET` 仍然要檢查**：mux 只解決地址，不解決初始化

#### 後備方案（保留，不施工）

若日後買不到 mux 或想省 ¥5，改回 4×SHDN 改址：接線與序列寫在 `docs/plan-v3-tof050c.md` 的 git 歷史（2026-11-04 20:45 版本），有完整開機序列、讀回驗證與身份重認程序。

## 2. 初始化與校正（每粒都要做）

依序：

1. `VL6180X_Init()`（component 的預設初始化，含 private register 載入）
2. `PerformRefSpadManagement()` — 出廠 SPAD 差异校正，**必須做**，否則近距靈敏度不穩
3. `PerformOffsetSimpleCalibration(50)` — 用 50 mm 白卡做偏移校正
4. **若有保護窗**：`PerformXTalkCalibration(100)` — 用 100 mm 目標做串擾補償；無窗可略
5. 寫入連續量測模式：`SYSRANGE_START=0x03`，`INTERMEASUREMENT_PERIOD ≈ 20 ms`

校正結果（offset、xtalk）**寫入 NVS**，下次上電直接套用；換 sensor 或換安裝位置要重做。

## 3. 取樣與判定（與 v2 相同的演算法，參數改為實測值）

- 連續模式，量測週期 **20 ms**（4 粒輪詢一輪 ≈ 80–90 ms）
- 判定不看絕對距離，看**邊緣＋形態**：

```
baseline = 最近 10 個樣本中位數（每 2 s 更新，避開事件窗）
事件 = (a) 比 baseline 短 ≥ 25 mm
       (b) 連續 ≥ 3 個樣本（≥ 60 ms）
       (c) 之後 500 ms 內回到 baseline ±15 mm
開蓋後 300 ms 內不判定
四桶同時 candidate → 取距離變化最大者；一回合只接受一次
```

- 距離單位統一 mm；`stc_bin.h` 的 `dist_mm` 不變

## 4. 已知失效模式（要寫入報告）

| 模式 | 原因 | 對策 |
|---|---|---|
| 全黑／絨面卡 | 850 nm 吸收，回波弱 | 實測；必要時該桶加擋光片或用較近安裝位 |
| 鏡面覆膜斜角 | 鏡面反射離開 FoV | 用啞面過膠卡；列入驗收 |
| 手伸入 | 先於卡觸發 | (c) 回復條件過濾；需人手實測校準 |
| 環境光過強 | 室內一般無問題，戶外會飽和 | 比賽在室內，記錄 ALS 讀數作證據 |
| 桶間互見 | 25° FoV | 桶間加隔板／擋光片（**由 G1 決定是否必要**） |

## 5. 驗收（數字不變）

**G1 幾何閘（開工前，半日）**
四種材質（印刷啞面／全黑／鏡面／皺紙）各 4 次、真實桶口位置：

- 漏檢 ≤ 1/16，誤檢 = 0
- 蓋全開時，四粒讀數不得出現「恆定 <100 mm」

**G2 偵測（實作後）**

- 4 桶 × 20 次投放（每材質 5 次）：總漏檢 ≤ 1/80、誤檢 = 0
- 手伸入再離開、不投卡：20 次測試 0 次誤判

**G3 延遲**：PLAY → 蓋開始動 p95 ≤ 500 ms；PLAY → 蓋全開 p95 ≤ 3 s；ToF 事件 → S31 收到 p95 ≤ 50 ms

**G4 穩定**：連續 10 回合 0 介入；30 分鐘連續運行無掉幀、無需重啟

**G5 通訊**：拔電／重插任一板 → 10 s 內重同步；100 次指令 0 次重複執行

## 6. 驅動與建置（S31 需先驗）

| 選項 | 說明 |
|---|---|
| `nestorit/vl6180x`（Espressif Component Registry） | native I²C，含 ALS；**registry 列出的 target 未見 esp32s31** |
| `kerikun11/ESP32_VL6180X` | C++ ESP-IDF component，MIT |
| ST 官方 VL6180X API | 完整校正函式（offset／xtalk／SPAD），需自行接 I²C 讀寫 |

**第一件事**：在 `esp32s31` target 上試建一次。建不過就改用 ST 官方 API 直接移植校正函式（工作量約半日），**不要因為建不過就換回 VL53L0X**——先確認是哪個問題。

## 7. 風險

| 風險 | 影響 | 對策 |
|---|---|---|
| S31 target 無現成 component | 卡 1–2 日 | 先試建；不成用 ST API 手接 I²C |
| 忘記做 SPAD／offset 校正 | 近距離讀數漂移，誤檢 | 列入初始化流程與驗收 |
| SHDN 改址漏做 10 kΩ 下拉 | 上電時兩粒同時在 0x29，匯流排衝突 | 硬件檢查表第一項 |
| 保護窗未做 xtalk 校正 | 固定偏差 | 有窗就必須校正 |
| 幾何未驗就開工 | 全部白做 | **G1 先行** |

## 8. BOM（4 桶）

| 項目 | 數量 | 單價 | 小計 |
|---|---|---|---|
| TOF050C | 4 | ¥8 | ¥32 |
| 10 kΩ 電阻 | 4 | — | ~¥1 |
| 排線／插座 | 4 組 | — | ~¥10 |
| **合計** | | | **≈ ¥43（HK$47）** |

---

## 9. Codex 實作範圍（TOF bring-up ＋ G1 量測工具）

硬件事定：**4 × TOF050C（VL6180X）→ TCA9548A（0x70）CH0–CH3 → S3 DevKitC-1**。四粒全部 0x29。

### 9.1 第一階段：開機與讀數（不需要桶，不需要幾何）

1. `mux_select(ch)`：`i2c_master_transmit(0x70, &(1<<ch), 1)`，之後 **≥100 µs** 才讀
2. 每粒 init：
   - 讀 `SYSTEM__FRESH_OUT_OF_RESET`，等於 1 才寫 AN4545 mandatory private registers
   - `PerformRefSpadManagement()`
   - `PerformOffsetSimpleCalibration(50)`（50 mm 白卡）
   - 有保護窗才做 `PerformXTalkCalibration(100)`
   - 校正值寫 NVS（key: `tof_ch<n>_offset`、`tof_ch<n>_xtalk`）
3. 連續量測：`INTERMEASUREMENT_PERIOD = 20 ms`；四通道輪詢一輪 ≈ 80–90 ms
4. **序列埠命令**（dev 用）：
   - `tof info` — 四粒 ID、校正值、fault mask
   - `tof stream on|off` — 20 Hz 印 `ch,dist_mm,status`
   - `tof cal <ch> <mm>` — 用已知距離重做 offset
   - `tof selftest` — `stc_bin_selftest()`

### 9.2 第二階段：G1 量測（人手做，Codex 只提供工具）

用 `tof stream` 邊移動感測器邊看數字，記錄：

| 情境 | 要記錄 |
|---|---|
| 四種材質落卡（啞面／全黑／鏡面／皺紙） | 距離波形、是否漏檢 |
| 蓋全開 | 四粒讀數是否「恆定 <100 mm」 |
| 手伸入再離開 | 是否產生假事件 |
| 相鄰桶同時干擾 | 兩粒讀數是否同步跳動 |

輸出：`docs/g1-results.csv`（時間、ch、距離、備註）＋ 一段波形圖（可用 `tof stream` 原始輸出畫）。

### 9.3 驗收（bring-up）

| # | 項目 | 標準 |
|---|---|---|
| H1 | 四粒全部辨識 | `tof info` 顯示 CH0–CH3 都有回應，fault mask = 0 |
| H2 | 穩定性 | 固定目標 100 mm，30 s 內 σ ≤ 5 mm |
| H3 | 取樣率 | `tof stream` 每通道 ≥ 10 Hz（四通道合計 ≥ 40 Hz） |
| H4 | 長跑 | 10 分鐘連續讀取 0 次 I²C 錯誤、0 次 NaN |
| H5 | 熱插拔 | 拔掉一粒 → fault mask 對應 bit 置 1，其餘三粒照常；插回並 `tof selftest` 可恢復 |
| H6 | 校正持久 | 重開機後 NVS 校正值仍在，100 mm 目標讀數偏差 < 3 mm |

### 9.4 明確不做

- 不改 `contracts/stc_bin.h` 的簽名（v1.2.0 已定）
- 不實作 ESP-NOW 傳輸（S3 → S31 的手部通訊留待下一階段；本階段用序列埠讀數）
- 不動舵機（`open_all`／`close_all` 可先回 `STC_ERR_UNSUPPORTED`）
- 不改官方板、`application.cc`、`protocols/*`

### 9.5 與 issue #25 的關係

issue #25 的 `stc_bin` **保持 stub**；本階段完成後，把 stub 換成真實作即可，`stc_game` 不需要改動。
