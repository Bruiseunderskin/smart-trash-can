# AI Chat 實作方案（整體架構 ＋ Codex 現階段範圍）

2026-11-04 ｜ 阿吉 the hermes ｜ 契約檔在 `contracts/`

---

## 1. 全景：四個 seam，一個契約

```
                ┌──────────────────────────────┐
                │  stc_game  （唯一改分數）      │
                └───┬──────────┬───────────┬───┘
       stc_vision   │          │ stc_bin   │  stc_voice
       影像→標籤     │          │ 舵機/ToF  │  台詞/對話
                    ▼          ▼           ▼
               SC101IOT     S3+VL53L0X   ES8389 + B1 server
```

- 契約：`contracts/stc_*.h`（四個 seam）＋ `contracts/model/*`（模型 I/O）
- 鐵律：只有 `stc_game` 可以改分數；每個 seam 都要有 stub；改契約要升版本並同步韌體與訓練

## 2. 現階段（UI 與模型未就緒）— Codex 只做 AI chat

### 2.1 要做（真實實作）

| 範圍 | 具體 |
|---|---|
| `stc_voice` 全部 | 本地台詞播放（預生成 PCM）、模式 S `/say`、模式 Q `/ask`、播放佇列與打斷、GAME/CHAT 閘、60 s 自動回 GAME |
| HTTP client | 連 `server/chat`（B1）；串流邊收邊寫 I2S；超時與斷線回落本地台詞 |
| 台詞表 | `phrases.json` → 預生成 PCM，key 對應 `stc_line_t` |
| 模式切換 | 長按 SET 進 CHAT、再長按回 GAME；序列埠命令 `mode game|chat` |
| 計時 | 五點 timestamp（呼叫→送出→首 frame→解碼→出聲），寫入 `VALIDATION.md` |

### 2.2 不要做（用 stub 佔位）

| 模組 | stub 行為 |
|---|---|
| `stc_vision` | 讀 `stub-labels.json`（例如 `[0,1,2,3,0]`）按回合輪流回標籤，`conf=0.99`，`src=STC_SRC_STUB`，延遲 200 ms 模擬 |
| `stc_bin` | 開蓋後 3 s 自動回一個 `stc_drop_t`（桶位＝stub 標籤對應桶 50%、隨機另一個桶 50%），令「答對／答錯」兩條路都跑到 |
| `stc_game` | **最小真實邏輯**：5 回合、+10/−5/0、每回合關蓋、逾時、結算。這一部分值得先做真的，因為它是整合骨架 |
| UI | 不做。狀態一律用序列埠 log ＋ 板載 LED |

### 2.3 為甚麼是這個切法

- `stc_voice` 是唯一完全唔依賴模型與 UI 的模組 → 現在做，之後換 UI／換模型都唔使動它
- `stc_game` 先寫真的，等於先定好事件順序；UI 同模型到位後只是換掉 stub 實作
- vision／bin 用 stub，就可以跑「5 回合完整流程」，提早發現時序與佇列問題

## 3. 驗收（現階段）

| # | 項目 | 標準 |
|---|---|---|
| A | 5 回合 smoke（全 stub） | 序列埠完整輸出 5 回合，分數正確，無卡死 |
| B | 模式 S 延遲 | 文字 → 出聲 p95 ≤ 1.0 s（n ≥ 30） |
| C | 模式 Q 延遲 | p95 ≤ 3.0 s（n ≥ 30，關掉 MCP 相機工具後量） |
| D | 斷線回落 | 拔網 3 s 內改播本地台詞，遊戲繼續 |
| E | GAME/CHAT 閘 | GAME 模式下 **0 次**雲端連線、0 次喚醒觸發 |
| F | 佇列 | 連續 10 句台詞不疊聲、可被打斷、不爆音 |
| G | 契約 | 序列埠印出 `stc_vision_version()`、`contract_version`；不符時拒絕啟動並提示 |

**統計要求**：n < 30 不准報 p95；一律 median／p95／max 三個數。

## 4. 之後怎樣換入真模組（零改線）

| 到貨 | 動作 |
|---|---|
| UI（LVGL 四屏） | 只讀 `stc_game_snapshot()` 與事件；不改任何模組 |
| 模型（`model.onnx` ＋ `classes.json` ＋ `preprocess.json`） | 換掉 `stc_vision` stub；其他模組不動 |
| S3 手部（PCA9685／ToF） | 換掉 `stc_bin` stub；`stc_game` 不動 |

交接規則：**任何模組要改，先改 `contracts/*.h` 並升版本，再改實作**。反向（先改實作再改契約）一律不接受。

## 5. 給訓練（W Ed）的契約

見 `contracts/model/README.md`，重點：

- 類別順序凍結：`0 可回收物 / 1 廚餘 / 2 有害垃圾 / 3 其他垃圾`
- 輸入：RGB、uint8、NHWC、160×160、歸一化在圖內
- 預處理：YUV422 → RGB → **中心裁切正方形** → bilinear resize（不做 letterbox）
- 輸出：`float32[1,4]` softmax；低於 0.55 → abstain
- 必交：`model.onnx`（opset ≤17）、`classes.json`、`preprocess.json`、`eval.json`（confusion matrix ＋ per-class F1 ＋ p50/p95）
- 門檻：macro-F1 ≥ 0.80、每類 recall ≥ 0.70、裝置 p95 ≤ 300 ms

**訓練端最易出錯的一點**：augmentation 只可以作用在裁切後的 RGB 圖；評估時必須用與推論完全相同的管線，否則數字不可信。

## 6. 時序

| 日期 | 交付 |
|---|---|
| 11/05 | `contracts/` 定稿（本文件）；Codex 開工 `stc_voice` |
| 11/06 | stub vision／bin／game ＋ 5 回合 smoke 通過（驗收 A） |
| 11/07 | 模式 S／Q 延遲量測（B、C、D、E、F） |
| 11/08 | UI／模型一到位就換入；否則保留 stub 上報 |
| 11/09 | M2 交付（AI chat 可演示、契約固定） |

## 7. 風險

| 風險 | 對策 |
|---|---|
| 契約改動波及訓練 | 契約版本寫入 `preprocess.json`，韌體啟動檢查；不符即拒絕 |
| stub 與真模組行為差異導致整合驚喜 | stub 必須回 `src=STC_SRC_STUB`，遊戲與 log 都顯示，不准冒充真結果 |
| 雲端不穩拖慢遊戲 | GAME 模式閘死；語音失敗靜音繼續 |
| Codex 同時改多個模組 | 只准改 `stc_voice` 與其 stub 接口；其他模組只准新增 stub 實作 |
