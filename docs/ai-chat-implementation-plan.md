# AI Chat 實作方案（整體架構 ＋ Codex 現階段範圍）

2026-11-04 v2 ｜ 阿吉 the hermes ｜ 契約在 `contracts/`
**本方案以「小智只在需要對話時才放出來」為前提，不以 B1 HTTP 為前提。**（B1 保留為後備，見 `docs/ai-chat-partition.md` §10）

---

## 1. 一句話

一部機、兩個模式：**GAME 模式**跑本地遊戲（小智完全靜音、零連線）；**CHAT 模式**才把小智放出來做語音問答；玩完一局（結算畫面）才准轉 CHAT。兩個模式**時間上不重疊**。

## 2. 整體架構

```
                    ┌─────────────────────────────┐
                    │ stc_game  （唯一改分數）      │
                    │ 5 回合 · +10/−5/0 · 關蓋      │
                    └──┬─────────┬──────────┬─────┘
        stc_vision     │         │ stc_bin  │ stc_voice
        影像→標籤       │         │ 舵機/ToF │ 台詞/對話
                       ▼         ▼          ▼
                  SC101IOT   S3+VL53L0X  ┌──────────────┐
                                         │ GAME: 本地PCM │
                                         │ CHAT: 小智    │
                                         └──────────────┘
```

四條 seam 全部定義在 `contracts/stc_*.h`。**只有 `stc_game` 可以改分數與回合**；其他模組只出事件。

## 3. 兩個模式（核心設計）

| | GAME（預設） | CHAT |
|---|---|---|
| 進入方式 | 開機即進入；CHAT 結束自動回來 | 結算畫面按鍵（或長按 SET） |
| 小智狀態 | 喚醒偵測**關**、音頻通道**關**、麥克風上行**關** | 喚醒偵測開（或按鍵直接進聆聽） |
| 發聲 | 本地預錄 PCM | 小智 TTS |
| 網絡 | **零連線** | 需要 Wi-Fi |
| 遊戲 | 正常跑 | 暫停：不收 PLAY、不開蓋 |
| 退出 | 一局 5 回合結束 → 結算 | 60 s 無互動 → 自動回 GAME |

**互斥規則（不可協商）**

1. 回合進行中（CAPTURE → CLOSE_ALL）**禁止**進 CHAT；按鍵無效，畫面提示「完咗先」
2. CHAT 中不得開始新一局
3. 任何模式下，語音失敗都不得阻塞遊戲：GAME 模式失敗→靜音繼續，分數照計

> 為甚麼不新增 `kDeviceState`：`kDeviceState` 在 `main/` 出現 160 處（`application.cc` 85、`display/` 30），加一個 state 要審全部 switch，且每次同步上游都要重貼。時序互斥已達到同等效果。詳見 `docs/ai-chat-partition.md` §12.6。

## 4. 介面（`contracts/`）

| 標頭 | 關鍵簽名 | 現階段 |
|---|---|---|
| `stc_types.h` | `stc_bin_t`(0–3 凍結)、`stc_err_t`、`stc_mode_t`、`stc_label_t`、`stc_drop_t`、`stc_event_t` | 定稿 |
| `stc_vision.h` | `stc_vision_init / capture(label,timeout) / release / version` | **stub** |
| `stc_voice.h` | `init / say(line) / set_mode / start_dialog / speak_text / ask / poll / is_busy / stop` | **真實實作（本階段）** |
| `stc_bin.h` | `init / open_all / close_all / start_monitor / stop_monitor / poll(drop) / distance` | **stub** |
| `stc_game.h` | `init / start / tick / input(event) / snapshot` | **最小真實邏輯** |
| `model/*` | `classes.json`、`preprocess.json`、`model.onnx`、`eval.json` | 契約已定，模型待訓練 |

輪替規則：任何模組要改，**先改 `contracts/*.h` 並升版本**，再改實作。反向不接受。

## 5. 現階段 Codex 交付範圍

### 5.1 真做

1. **`stc_voice` 全部**（見 §4）：
   - GAME 模式：本地預錄 PCM 播放、佇列、可打斷、無爆音
   - CHAT 模式：`stc_voice_start_dialog()` → 小智喚醒／聆聽；回播由小智負責
   - 模式切換：`EnableWakeWordDetection(false)` ＋ `CloseAudioChannel()` 進 GAME；反向進 CHAT
   - 60 s 無互動自動回 GAME
   - GAME 模式**零雲端連線**（要能量測證明）
2. **`stc_game` 最小邏輯**：5 回合、+10／−5／0、每回合關蓋、逾時、結算。這段要真做，它是整合骨架。
3. **模式閘與按鍵**：結算畫面才可進 CHAT；回合中按鍵無效並提示。
4. **台詞表**：`assets/phrases.json` → 預生成 PCM，key 對應 `stc_line_t`。

### 5.2 用 stub（佔位）

| 模組 | stub 行為 |
|---|---|
| `stc_vision` | 讀 `stub-labels.json`（例 `[0,1,2,3,0]`）按回合輪流回標籤，`conf=0.99`、`src=STC_SRC_STUB`、模擬 200 ms |
| `stc_bin` | 開蓋後 3 s 回一個 `stc_drop_t`：50% 正確桶、50% 隨機另一桶，令答對／答錯兩條路都跑到 |
| UI | 不做。狀態只用序列埠 log ＋ 板載 LED |

### 5.3 明確不做

- 不做文字注入（`WakeWordInvoke` / `SendUserText`）、不改 `application.cc`、`protocols/*`、`mcp_server.cc`
- 不做 B1 HTTP client（後備方案，不在本階段）
- **不新增 `kDeviceState`**（見 §3 註）
- 不改官方板目錄

## 6. 給訓練（W Ed）的模型契約 — 必須沿用

完整版 `contracts/model/README.md`；以下是不可違反的部分：

1. **類別順序凍結**：`0` 可回收物、`1` 廚餘、`2` 有害垃圾、`3` 其他垃圾（= 桶位順序 = UI 順序）
2. **輸入**：RGB、uint8、NHWC、**160×160**；歸一化寫在 ONNX 圖內
3. **預處理**：YUV422(UYVY) → RGB → **中心裁切正方形（640×480 → 中央 480×480）** → bilinear resize；**不用 letterbox**
4. **輸出**：`float32[1,4]` softmax；`conf < 0.55` 標 `abstain`
5. **必交**：`model.onnx`（opset ≤ 17）、`classes.json`、`preprocess.json`、`eval.json`（confusion matrix ＋ per-class P/R/F1 ＋ 裝置 p50/p95）
6. **門檻**：macro-F1 ≥ 0.80、每類 recall ≥ 0.70、裝置 p95 ≤ 300 ms
7. **最易出錯**：augmentation 只作用在裁切後的 RGB 圖；評估必須用與推論完全相同的管線
8. 訓練與韌體用同一個 `preprocess.json`；`contract_version` 不符時韌體拒絕啟動

## 7. 驗收（現階段）

| # | 項目 | 標準 |
|---|---|---|
| A | 5 回合 smoke（全 stub） | 序列埠完整輸出 5 回合，分數正確，無卡死 |
| B | 本地台詞 | 起播 < 50 ms，連播 10 句不疊聲、可打斷 |
| C | 模式互斥 | 回合中按鍵 0 次成功進 CHAT；CHAT 中 0 次開始新局 |
| D | GAME 零連線 | 一整局期間 0 條雲端連線（用 server log 或 socket 計數證明） |
| E | CHAT 真人語音 | 真人說喚醒詞 → 一問一答成功（**這是最大未知，必須實測**） |
| F | CHAT 自動退出 | 60 s 無互動自動回 GAME，狀態顯示正確 |
| G | 契約 | 啟動印 `contract_version`；不符即拒絕並提示 |

## 8. 時序

| 日期 | 交付 |
|---|---|
| 11/05 | `contracts/` 定稿；Codex 開工 `stc_voice` ＋ `stc_game` 骨架 |
| 11/06 | stub vision／bin 接上，驗收 A、B、C 通過 |
| 11/07 | 驗收 D、E、F、G；E 若失敗，立即回報（這是語音路徑首次真人實測） |
| 11/08 | UI／模型到貨就換入；未到則保留 stub 上報 |
| 11/09 | M2 交付 |

## 9. 風險

| 風險 | 對策 |
|---|---|
| **真人語音路徑從未實測**（Codex 只確認 WakeNet 初始化成功） | 驗收 E 提前到 11/07，失敗即改用按鍵直接進聆聽（`StartListening()`） |
| 模式切換殘留狀態（通道未真正關閉、喚醒仍活） | 驗收 D 用連線數證明，不靠「應該關了」 |
| stub 冒充真結果 | stub 必須標 `src=STC_SRC_STUB`，log 亦顯示 |
| 契約漂移 | 版本檢查 ＋ `preprocess.json` 唯一真相 |
| Codex 越界改多個模組 | 只准改 `stc_voice`／`stc_game` 與新增 stub 實作 |

---

## 10. 遊戲內「唔死板」的語音：拼接，而不是雲端

### 10.1 問題

實測（issue #24）官方 server 對 `listen/detect.text` 有 **約 16 字符上限**（含前綴），Q 模式回覆長度不可控（實測播放 10–20 s）。所以「每回合叫小智講一句」在技術上只可以講 ≤13 字的短句，而且每次要 0.5–3 s、要 Wi-Fi。

### 10.2 關鍵觀察：遊戲句子其實是有限的組合

遊戲只有 4 類 × 4 桶 = 16 種「你放咗 X 落 Y」的組合，加答對／答錯語氣。**這種結構適合拼接，不適合生成。**

### 10.3 拼接法（concatenative）：零網絡、0 ms、任意組合

預錄 14 段短音檔（同一把聲、同一錄音環境），播放時依序串接：

| 片段 | 內容 |
|---|---|
| A1 | 「呢張係」 |
| A2 | 「你放咗落」 |
| C0–C3 | 「可回收物」「廚餘」「有害垃圾」「其他垃圾」（類別名） |
| B0–B3 | 「可回收物桶」「廚餘桶」「有害垃圾桶」「其他垃圾桶」 |
| P_ok | 「，答啱喇」 |
| P_bad | 「，不過應該係」 |

句子 = `A1 + C<預測> + P_ok` 或 `A1 + C<預測> + A2 + B<玩家投放> + P_bad`

- 覆蓋率：4 × 2 + 4 × 3 = **20 種句子**，全部由 14 段音檔組合而成
- 延遲：**< 50 ms**（本地 flash／SD）
- 網絡：**零**
- 音質：同一把聲、同一環境，唔會有「一句真人一句機械」
- 擴充：加語言版本只需重錄 14 段

### 10.4 何時仍然用雲端（小智）

| 場景 | 用邊個 | 理由 |
|---|---|---|
| 遊戲回合中（宣讀／答對／答錯） | **拼接（本地）** | 3 秒窗口容不下 0.5–3 s 網絡延遲 |
| 評審自由問答（CHAT 模式） | **小智原生語音** | 開放式問題，本來就無固定答案 |
| 逐物件名稱（如「鋁罐」） | 暫時不做 | 模型只出 4 類，沒有物件級標籤；做了會是假精確 |

### 10.5 若要真正的任意長句

唯一乾淨路徑是 **B1（自建 HTTP server）**：文字→TTS 無字符限制，可快取、可控制長度。代價是多一台 server 與網絡依賴。**現階段不做**，因為 4 類遊戲用拼接已足夠，而 B1 帶來的只是修辭差異，不影響任何評分項。

### 10.6 驗收

- 20 種拼接句子全部可播，無爆音、無明顯接縫（接縫 < 20 ms 靜音）
- 拼接後總時長 ≤ 6 s
- 連續 10 句不疊聲
