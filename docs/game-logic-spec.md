# 遊戲邏輯規格（stub 先行，硬件後接）

狀態：**v1.0.0，待實作**（2026-11-05）。契約：`contracts/stc_game.h` v1.1.0、`contracts/stc_ui.h` v1.0.0。
前置：`docs/mode-gate.md` M1（模式閘）已完成。

---

## 0. 原則

**邏輯先寫死，接口先定死，硬件與 UI 後補。**

- 遊戲邏輯（回合、計分、超時、結算）用**虛擬時鐘＋假事件**完整測試，唔需要相機、唔需要舵機、唔需要 ToF
- UI 用**佔位版**（左上角顯示 state／round／score），隊友的正式 UI 只需換一個 `.c`
- 所有 stub 都經由契約介面，之後換真實作不改 `stc_game`

三條紅線：

1. `stc_game` 之外任何模組都不得改 `score` 或 `round`
2. UI 只可以讀 snapshot、收 event；不得觸發硬件
3. `src` 欄位（`STC_SRC_STUB`／`STC_SRC_REAL`）必須一路傳到 UI，demo 時一眼看得出係假數據

---

## 1. 回合狀態機

```
IDLE ──PLAY──▶ CAPTURE ──label──▶ OPEN ──▶ WAIT_DROP ──drop──▶ SETTLE ──▶ CLOSE ──▶ FEEDBACK ──▶ 下一回合／SUMMARY
                  │                 │            │                                        │
                  │ timeout/abstain │ 舵機失敗    │ 15 s 無投放                            │
                  ▼                 ▼            ▼                                        │
              FEEDBACK(0分)     FEEDBACK(0分)  FEEDBACK(0分/超時)                         │
```

| 階段 | 上限 | 超時行為 |
|---|---:|---|
| CAPTURE | 1200 ms（5 幀 × 50 ms ＋ 5 × 推論 ≈ 700 ms） | 播「睇唔清」，`abstained=true`，0 分，進 FEEDBACK |
| OPEN | 1500 ms | 開蓋失敗 → 播錯誤，0 分，進 FEEDBACK（不重試） |
| WAIT_DROP | 15000 ms | `timed_out`，0 分，進 CLOSE |
| SETTLE | 300 ms | 去抖：同一桶 300 ms 內多次 DROP 只算一次 |
| CLOSE | 1500 ms | 關蓋失敗 → 記 fault，仍進 FEEDBACK |
| FEEDBACK | 1200 ms | 自動進下一回合 |

**abstain 重試**：`conf < 0.55` 時重抓一幀（`max_retry = 1`）；第二次仍 abstain → 當 timeout，0 分。

---

## 2. 計分

| 情況 | 分數 | 語音 |
|---|---:|---|
| 投放桶 = 預測類別，且預測正確 | `score_correct`（+10） | `STC_LINE_CORRECT` |
| 投放桶 ≠ 預測類別 | `score_wrong`（−5） | `STC_LINE_WRONG` |
| 冇投放（超時） | 0 | `STC_LINE_TIMEOUT` |
| abstain | 0 | `STC_LINE_RETRY` |

- 分數下限 `score_floor = 0`（小朋友唔显示負分；內部仍記真實值供報告）
- **注意**：分數只反映「玩家是否按提示投放」，唔等於模型準確率。報告要分開講，唔可以拿遊戲分數當模型指標。

---

## 3. 接口與 stub

| 介面 | 現在（stub） | 之後（真） |
|---|---|---|
| `stc_vision_capture()` | 讀 `assets/playlist.txt` 輪流回 4 類；模擬 5 幀（其中 1 幀刻意不同以測聚合），`detail.agreement` 要正確填 | SC101IOT 連拍 5 幀（50 ms 間隔）＋ 5 次 int8 推論 ＋ 平均 softmax |
| `stc_bin_open_all()` | 記 timestamp，300 ms 後回 OK | PCA9685 四舵機，錯開 80–120 ms |
| `stc_bin_poll()` | 收到 `stc_game` 的「虛擬投放」注入後 500 ms 回一個 `stc_drop_t` | ToF050C ＋ TCA9548A |
| `stc_voice_say()` | 印 log ＋ `esp_rom_delay_us(200ms)` | 本地 PCM 片段拼接 |
| UI | `STC_UI_PLACEHOLDER`：黑底白字 `state/round/score/src` | 隊友正式 UI |

**虛擬投放**：序列埠 `drop <0-3>` 或 `drop auto`（每回合隨機桶），只在 `STC_PH_WAIT_DROP` 有效。

---

## 4. 虛擬時鐘測試（唔需要硬件）

把 `stc_game_tick(now_ms)` 的 `now_ms` 由測試控制，配合注入事件，就可以在 host 上跑完整局。

```c
// tests/host/test_game_round.c （示意）
uint32_t t = 0;
stc_game_start();
t += 100;  stc_game_tick(t);                 // CAPTURE
inject_label(STC_BIN_FOOD, t);               // → OPEN
t += 200;  stc_game_tick(t);
inject_drop(STC_BIN_FOOD, t);                // 正確
t += 400;  stc_game_tick(t);
assert(stc_game_snapshot().score == 10);
assert(stc_game_snapshot().phase == STC_PH_CLOSE);
```

必須覆蓋的情境（每個都要 assert 分數、phase、`stc_result_t`）：

1. 5 回合全對 → 50 分、`correct=5`
2. 5 回合全錯 → 0 分（floor）、`wrong=5`
3. 每回合都超時 → 0 分、`timed_out=5`
4. abstain 一次後成功 → 該回合正常計分、`abstained=1`
5. 同一回合連投兩次 → 只計第一次
6. CAPTURE 超時 → 0 分、phase 仍可推進
7. 回合中按 PLAY → 無效、狀態不變
8. 第 5 回合後 → `STC_PH_SUMMARY`、`stc_result()` 正確
9. `stc_game_reset()` 後可再開局，分數歸零
10. 隨機 1000 局（隨機正確／錯誤／超時／abstain 比例）→ 不變量：
    - `score` 永遠在 `[floor, rounds_total*score_correct]` 之內
    - `round` 單調遞增，永遠 `< rounds_total`
    - `phase` 只能沿轉移表走

---

## 5. 佔位 UI

- 黑底、白字，四行：`MODE`、`ROUND n/5`、`SCORE`、`SRC`（STUB 用紅色顯示）
- 只在 `stc_ui_tick()` 內更新 label 文字，**不建動畫、不建圖片**
- 屏幕切換按 `snap->phase` 決定，不做額外狀態
- 目的：讓隊友的 UI 只需替換 `stc_ui_*.c`，`stc_game` 與 `mode` 完全不用改

**替換契約**：隊友提供的新 UI 必須實作 `stc_ui_init/tick/notify/screen` 四個函式；
`main` 只 include `contracts/stc_ui.h`，不 include 任何 UI 私有標頭。

---

## 6. 驗收

| # | 項目 | 標準 |
|---|---|---|
| J1 | host 測試 | §4 的 10 個情境全過，`ctest` 綠 |
| J2 | 隨機局 | 1000 局不變量 0 違反 |
| J3 | 一局時間 | 5 回合（含 15 s 超時最壞情況）≤ 100 s |
| J4 | 裝置 demo | 序列埠 `start` → 5 回合自動跑完，`drop auto` 模式 0 人工介入 |
| J5 | UI 佔位 | 四個 phase 都有對應畫面，`SRC=STUB` 明顯可見 |
| J6 | 替換性 | 用假 UI（只記錄呼叫）替換後，`stc_game` 測試仍全綠 |
| J7 | 語音失敗 | 拔 codec 後 5 回合照跑，分數正確（與 G5 一致） |

---

## 7. 明確不做

- 真實相機、真實舵機、真實 ToF（零件未齊）
- 正式 UI 美術（隊友負責）
- 語音內容（M3 之後）
- 排行榜、多玩家、計時賽

---

## 8. 連拍 5 幀（v1.1）

### 8.1 為甚麼是 5 幀 / 50 ms

- 200 ms 采集窗足以跨過手震與單幀噪點，又短到不會令小朋友移動物件
- 5 幀逐幀推論 ≈ 5 × 84 ms ≈ 420 ms；加采集窗 ≈ 620–700 ms，仍在 CAPTURE 預算內
- 兩幀之間的相關性太高（<50 ms）沒有額外資訊，太多幀則開始超時

### 8.2 聚合規則

1. 每幀 → `softmax[4]`
2. 5 幀平均 → 再 `argmax`
3. `conf < 0.55` → abstain（觸發 `max_retry` 一次）
4. 成功解碼幀數 < 3 → `STC_ERR_IO`，該回合 0 分
5. 記錄 `agreement`（幾多幀與最終結果一致）——**這是報告要用的證據**

### 8.3 相機保持開啟

- 進 `GAME` → `stc_vision_stream_start()`；進 `CHAT` → `stc_vision_stream_stop()`
- 好處：省掉每回合 300–500 ms init；壞處：持續耗電 ＋ 可能與 RGB LCD 爭 `LCD_CAM`
- **風險與對策**：若實測見到 LCD 撕裂／掉幀，改為「採集窗內暫停 LCD 重繪（只畫不刷新）」；再不行就退回每回合單幀，並在報告註明

### 8.4 證據輸出

每回合寫一行到 `runs/capture-YYYYMMDD.csv`：

```
t_ms,truth,pred,conf,agreement,f0,f1,f2,f3,f4,t_capture_ms,t_infer_ms
```

報告可以用它講「5 幀聚合後準確率由 X% 升到 Y%」、「agreement ≥4/5 的樣本佔 Z%」。
