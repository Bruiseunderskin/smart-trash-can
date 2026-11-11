# 遊戲邏輯規格（stub 先行，硬件後接）

狀態：**v2.0.0**（2026-11-05 修訂：改為「揀卡 → 掃描 → 開蓋 → 投放 → 學習頁」真實流程）
契約：`contracts/stc_game.h` v1.2.0、`contracts/stc_ui.h` v1.1.0、`contracts/stc_vision.h` v1.2.0。
前置：`docs/mode-gate.md` M1（模式閘）已完成。

---

## 0. 一局係咩樣

**3 分鐘、5 回合、25–30 張卡牌。** 每回合：

1. 玩家**揀一張卡**（卡上係一件垃圾）
2. 放喺相機前 → **掃描**（相機開、連拍 5 幀、關）
3. 螢幕顯示**掃到嘅類別**
4. **四個桶蓋全開**
5. 玩家**將卡放入其中一個桶**
6. ToF **偵測入咗邊個桶**
7. 不論對錯 → **學習頁**（對答案 ＋ 小知識 ＋ 分數變化）
8. 下一回合；5 回合或 3 分鐘到 → 結算

> 相機**只在第 2 步開**，掃完即關。不串流、不常開。

---

## 1. 回合狀態機

```
IDLE ─PLAY─▶ PROMPT ──card ready──▶ SCAN ──label──▶ RESULT ──▶ OPEN ──▶ WAIT_DROP
                                                                          │
                                    ┌─────────────────────────────────────┘
                                    ▼ drop / timeout
                                 SETTLE ──▶ CLOSE ──▶ LEARNING ──▶ 下一回合／SUMMARY
```

| 階段 | 上限 | 超時／失敗行為 |
|---|---:|---|
| PROMPT | 20 s | 該回合 0 分、記 `timed_out`，直接下一回合 |
| SCAN | 1.5 s | 掃描失敗 → `abstained`，重掃一次；再失敗 → 0 分、進 LEARNING |
| RESULT | 1.5 s | 自動進 OPEN |
| OPEN | 1.5 s | 開蓋失敗 → 記 fault，仍進 WAIT_DROP（玩家可以手動放） |
| WAIT_DROP | 15 s | 冇投放 → 0 分、進 CLOSE |
| SETTLE | 300 ms | 同一桶 300 ms 內多次 DROP 只算一次 |
| CLOSE | 1.5 s | 關蓋失敗 → 記 fault，仍進 LEARNING |
| LEARNING | 6 s | 可按 PLAY 提前跳過 |

**3 分鐘硬上限**：`elapsed_ms > total_time_ms` 時，**做完本回合**（不中途切斷）才進 `SUMMARY`，`ended_by_time = true`。

---

## 2. 計分

| 情況 | 分數 | 語音 |
|---|---:|---|
| 投放桶 = 模型判定類別 | `score_correct`（+10） | `STC_LINE_CORRECT` |
| 投放桶 ≠ 模型判定類別 | `score_wrong`（−5） | `STC_LINE_WRONG` |
| 冇投放（15 s 超時） | 0 | `STC_LINE_TIMEOUT` |
| abstain（兩次掃描都低於閾值） | 0 | `STC_LINE_RETRY` |

- 顯示分數有 floor 0；`stc_result_t.score_raw` 保留真實加總，供報告使用
- **遊戲分數 ≠ 模型準確率**，報告必須分開講

---

## 3. 學習頁（Learning Page）

不論對錯都出，內容固定六項：

```
你掃到：      可回收物  (0.87，5 幀一致 4/5)
你放咗落：    廚餘桶
正確答案：    可回收物桶      ✗ 答錯
本回合：      −5
累計：        25
小知識：      可回收物包括膠樽、鋁罐、紙皮、金屬罐。
```

- 小知識每個類別一段固定文案（4 段），由 `stc_ui_learning()->tip` 傳入
- **不顯示具體物品名**（模型只出類別，寫物品名會係假精確）
- abstain 時改為：「睇唔清，下次試下擺正啲」，不計分

---

## 4. 介面與 stub

| 介面 | 現在（stub） | 之後（真） |
|---|---|---|
| `stc_vision_scan()` | 讀 `assets/playlist.txt` 輪流回 4 類；模擬 5 幀（**其中 1 幀刻意不同**），`agreement` 正確填；`src=STC_SRC_STUB` | 開 SC101IOT → 5 幀 @50 ms → 5 次 int8 推論 → 平均 softmax → 關相機 |
| `stc_bin_open_all()` | 記 timestamp，300 ms 後回 OK | PCA9685 四舵機，錯開 80–120 ms |
| `stc_bin_poll()` | `drop <0-3>`／`drop auto` 注入後 500 ms 回 `stc_drop_t` | ToF050C ＋ TCA9548A |
| `stc_voice_say()` | 印 log ＋ 200 ms | 本地 PCM 片段拼接 |
| UI | `STC_UI_PLACEHOLDER`：黑底白字 | 隊友正式 UI（只換 `.c`） |

**虛擬投放**：序列埠 `drop 0`…`drop 3`，或 `drop auto`（隨機桶），只在 `STC_PH_WAIT_DROP` 有效。

---

## 5. 虛擬時鐘測試

`stc_game_tick(now_ms)` 由測試控制，配合注入事件即可在 host 跑完整局。

必測情境（每個 assert 分數、phase、`stc_result_t`）：

1. 5 回合全對 → 50、`correct=5`
2. 5 回合全錯 → 顯示 0（floor）、`score_raw=-25`、`wrong=5`
3. 每回合 WAIT_DROP 超時 → `timed_out=5`
4. PROMPT 超時 → 該回合 0 分並跳過
5. abstain 兩次 → 0 分、`abstained=1`
6. 同回合連投兩次 → 只計第一次
7. 回合中按 PLAY（非 LEARNING）→ 無效
8. LEARNING 中按 PLAY → 提前進下一回合
9. 第 5 回合後 → `STC_PH_SUMMARY`
10. 3 分鐘到（第 3 回合中）→ 做完本回合才 `SUMMARY`，`ended_by_time=true`
11. `reset()` 後可再開局，分數與計時歸零
12. 隨機 1000 局 → 不變量：分數範圍、`round` 單調、phase 沿轉移表、`elapsed_ms` 單調

**J6 替換性**：用只記錄呼叫的假 UI 取代佔位 UI，`stc_game` 測試仍全綠。

---

## 6. 佔位 UI

- 黑底白字，每屏三至四行
- `IDLE`：`PLAY 開始`
- `PROMPT`：`揀一張卡，放喺相機前`＋倒數
- `SCAN`：`掃描中…`＋剩餘時間
- `RESULT`：類別名 ＋ `conf 0.87` ＋ `SRC`
- `WAIT_DROP`：`放入任何一個桶`＋倒數
- `LEARNING`：§3 六行
- `SUMMARY`：分數、正確數、時間
- `SRC = STUB` 一律**紅色**顯示

替換契約：新 UI 只實作 `stc_ui_init/tick/notify/learning/screen`，`main` 只 include `contracts/stc_ui.h`。

---

## 7. 驗收

| # | 項目 | 標準 |
|---|---|---|
| J1 | host 測試 | §5 的 12 個情境全過 |
| J2 | 隨機局 | 1000 局 0 不變量違反 |
| J3 | 一局時間 | 正常打 5 回合 ≤ 120 s；3 分鐘上限必生效 |
| J4 | 裝置 demo | `start` → 5 回合自動跑完，`drop auto` 0 人工介入 |
| J5 | 佔位 UI | 每個 phase 有對應畫面，`SRC=STUB` 明顯 |
| J6 | 替換性 | 假 UI 替換後測試仍全綠 |
| J7 | 語音失敗 | 拔 codec 後 5 回合照跑、分數正確 |
| J8 | 相機生命週期 | 一局內相機開啟次數 = 掃描次數（5 次），`WAIT_DROP` 期間相機必須已關 |

---

## 8. 連拍 5 幀（掃描內部）

- 采集窗 200 ms（5 × 50 ms），逐幀推論 ≈ 5 × 84 ms ≈ 420 ms，加開相機 300–500 ms → **總計 1.0–1.2 s**
- 聚合：5 幀平均 softmax → argmax；`conf < 0.55` → abstain
- 成功幀 < 3 → `STC_ERR_IO`
- 每回合寫一行 `runs/capture-YYYYMMDD.csv`：

```
t_ms,pred,conf,agreement,f0,f1,f2,f3,f4,t_open_ms,t_capture_ms,t_infer_ms
```

報告講法：「5 幀聚合後準確率由 X% 升到 Y%」、「agreement ≥4/5 佔 Z%」。

---

## 9. 明確不做

- 真實相機／舵機／ToF（零件未齊）
- 正式 UI 美術（隊友負責）
- 語音內容（`#25` M3）
- 逐物品名稱、排行榜、多玩家
