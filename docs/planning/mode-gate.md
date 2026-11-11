# 模式閘（Mode Gate）規格

狀態：**v1.1.0，待實作**（2026-11-08）。契約：`contracts/stc_mode.h`（v1.1.0）。
決策依據：`docs/PROJECT-PLAN.md` D2（小智在閘後）、D3（不新增 `kDeviceState`）。

---

## 0. 一句話

**小智不需要被修改，只需要被鎖在閘後。** 閘本身是我們自己的一層狀態機，
它決定「而家係遊戲定聊天」，並且是唯一有權決定的地方。

---

## 1. 為何不是改小智內部

`kDeviceState` 在 `main/` 出現 **160 處**（`application.cc` 85 處），每次同步上游都要重貼補丁。
本專案 11/09 之前沒有這個預算。所以：

| 做法 | 成本 | 結論 |
|---|---|---|
| 改 `kDeviceState` 加 STC_GAME／STC_CHAT | 160 處 ＋ 每次 rebase 重貼 | ✗ |
| 外層 `stc_mode` ＋ hooks（本方案） | 約 180 行 ＋ host 單元測試 | ✓ |

小智看到的仍然是它熟悉的 `kDeviceStateIdle`；我們只是**在外面決定它有沒有機會被喚醒**。

---

## 2. 狀態

| 狀態 | 語義 | 遊戲 | 語音 | 可進 CHAT |
|---|---|---|---|---|
| `STC_ST_BOOT` | 初始化中 | ✗ | ✗ | ✗ |
| `STC_ST_GAME_IDLE` | 等 PLAY | 可開局 | 本地台詞 | **✓ 經圖鑑「問綠豆」** |
| `STC_ST_GAME_ROUND` | 回合進行中 | **進行中** | 本地台詞 | **✗（硬性）** |
| `STC_ST_SUMMARY` | 結算畫面 | ✗ | 本地台詞 | ✗ |
| `STC_ST_CHAT` | 小智原生語音 | **暫停** | 小智 ASR→LLM→TTS | — |
| `STC_ST_FAULT` | 契約／硬件錯誤 | ✗ | ✗ | ✗ |

`stc_mode_layer()` 把前四個映射為 `STC_MODE_GAME`、`STC_ST_CHAT` 映射為 `STC_MODE_CHAT`，
所以 `stc_voice_set_mode()` 的既有語義不變。

---

## 3. 轉移表（唯一合法轉移）

| From | 輸入 | To | 副作用 |
|---|---|---|---|
| BOOT | init 成功 | GAME_IDLE | `enter_game()` |
| BOOT | init 失敗 | FAULT | 顯示錯誤、蜂鳴 |
| GAME_IDLE | `IN_PLAY` | GAME_ROUND | `enter_round()`、`stc_game_start()` |
| GAME_IDLE | `IN_CHAT_ENTER`（分類圖鑑「問綠豆」） | CHAT | `enter_chat()` |
| GAME_ROUND | `IN_PLAY` | （拒絕） | 無 |
| GAME_ROUND | `IN_SET_LONG` | （拒絕） | 無；UI 提示「完咗先」 |
| GAME_ROUND | `IN_ROUND_END`，未到最後一回合 | GAME_IDLE | `exit_round()` |
| GAME_ROUND | `IN_ROUND_END`，最後一回合 | SUMMARY | `exit_round()`、播結算 |
| SUMMARY | `IN_PLAY` | GAME_IDLE | 清結算畫面 |
| SUMMARY | `IN_SET_LONG` | （拒絕） | 無；結算頁不設聊天入口 |
| CHAT | `IN_CHAT_EXIT` | GAME_IDLE | `exit_chat()`、`enter_game()` |
| CHAT | `IN_CHAT_TIMEOUT`（60 s） | GAME_IDLE | 同上 |
| 任何 | `IN_FAULT` | FAULT | 停語音、開蓋？**不開**，顯示故障 |
| FAULT | `IN_RESET` | BOOT | 重新 init |

**非法轉移一律 `STC_ERR_NOT_READY`，狀態不變，零副作用。** 這一條要有單元測試。

---

## 4. 不變量（用 property test 驗）

- **I1**：`CHAT` 唯一入口 = 分類圖鑑「問綠豆」（`GAME_IDLE` ＋ `IN_CHAT_ENTER`）。
  任何其他路徑（包括 `SUMMARY` ＋ `IN_SET_LONG`）都是 bug。
- **I2**：`CHAT` 期間 `stc_game_tick()` 不被呼叫（或呼叫後分數不變）
- **I3**：`GAME` 期間不開任何雲端連線（見 §5 的 counter）
- **I4**：只有 `stc_mode_input()` 改狀態，其他模組只讀
- **I5**：任何非法輸入不改變狀態、不觸發 hook

測試方法：隨機 10,000 步輸入序列（含非法輸入），每步檢查 I1–I5。

---

## 5. 「鎖好小智」的具體含義

`enter_game()` 必須做齊（順序不能反）：

1. `EnableWakeWordDetection(false)`
2. `CloseAudioChannel()`（若仍開著）
3. 停麥克風上行（AFE 可留，但不上傳）
4. 確認 `GetCamera()` 回 `nullptr`（`STC_MCP_CAMERA=n`），server 不會發起拍照
5. 抑制 notification／MCP 主動發聲

`enter_chat()`：

1. `EnableWakeWordDetection(true)`
2. 或按鍵直接 `StartListening()`（若 11/07 真人語音未通，走這一條）

**GAME 期間若觀察到雲端活動**（socket／MQTT 事件），呼叫 `stc_mode_note_cloud_activity()`；
counter > 0 時在結算畫面角落顯示警告，並寫 log。**不自動切 FAULT**——現場寧可繼續玩。

---

## 6. 60 秒自動退出

- `CHAT` 進入時記 `t_enter`；每次偵測到語音活動（VAD／播放）更新 `t_last_activity`
- `stc_mode_tick()` 內：`now - t_last_activity > 60000` → `IN_CHAT_TIMEOUT`
- 退出時播一句本地台詞「返去玩啦」，然後 `enter_game()`

---

## 7. 語音失敗不得阻塞計分

GAME 模式下任何語音呼叫失敗（codec、佇列滿、檔案缺）→ 靜音繼續，回合照跑、分數照計。
`stc_voice_say()` 回錯誤時，`stc_game` **不得**改變流程。

---

## 8. 實作順序（給 Codex）

| 步 | 內容 | 可否在 host 測 |
|---|---|---|
| M1 | `stc_mode` 狀態機 ＋ 轉移表 ＋ 10k 步 property test | **可以**（純 C，`tests/host/`） |
| M2 | `stc_mode` 接上 `stc_voice_set_mode()`（stub 後端即可） | 可以 |
| M3 | 序列埠命令 `mode` / `mode game` / `mode chat` / `mode reset` | 需要板 |
| M4 | 真人由圖鑑「問綠豆」進 CHAT、60 s 自動退出 | 需要板 |

**M1 先做，因為它不需要硬件、不需要模型，而且它決定後面所有東西的形狀。**
M1 做完，#25 其他部分才有穩定的地基。

---

## 9. 驗收

| # | 項目 | 標準 |
|---|---|---|
| G1 | 轉移表 | 表中每一格都有測試；非法格回 `STC_ERR_NOT_READY` 且狀態不變 |
| G2 | 不變量 | 10,000 步隨機輸入，I1–I5 全過 |
| G3 | 回合鎖 | `GAME_ROUND` 期間 `IN_SET_LONG` 100 次，0 次進 CHAT |
| G4 | 自動退出 | CHAT 閒置 60 s ± 1 s 回 GAME_IDLE |
| G5 | 語音失敗 | 拔掉 codec，5 回合照跑完，分數正確 |
| G6 | 雲端零連線 | GAME 模式 10 分鐘，`stc_mode_cloud_violations() == 0` |
