# AI Chat 的分割與實作計劃

2026-11-04 ｜ 阿吉 the hermes ｜ 配套 `docs/plan-v2-tof.md`

---

## 1. 先分清：這專案裡的「AI Chat」其實是三件不同的事

| 代號 | 是甚麼 | 要網？ | 遊戲能不能沒有它 | 誰做 |
|---|---|---|---|---|
| **V1 固定短句播報** | 開場／答對／答錯／逾時／結算，預錄音檔 | 否 | **不能沒有**（無聲＝demo 死） | 自己 |
| **V2 語音喚醒＋命令詞** | 「開始」「下一張」等，ESP-SR 離線 | 否 | 可以（有 PLAY 實體鍵） | 自己 |
| **V3 對話（ASR→LLM→TTS）** | 小智那套雲端問答 | 是 | **可以沒有**（只做場邊展示） | 自己（接現成 pipeline） |

> 最常見的失敗是把三件當一件做，結果 V3 未通就拖住 V1，連遊戲都出不到聲。
> **鐵律：分數與流程只依賴 V1；V2 是加分；V3 是表演。**

---

## 2. 四個 seam（先定介面，才有得並行）

所有平行工作都靠這四條縫切開。每一條都要有「fake 實作」，令另一邊未完成時仍可跑。

```c
/* 1. 視覺 */                      /* 2. 語音 */
typedef struct {                   typedef enum {
  uint8_t label;   /* 0..3 */        VOICE_LINE_OPEN, VOICE_LINE_RIGHT,
  float   conf;                      VOICE_LINE_WRONG, VOICE_LINE_TIMEOUT,
} vision_result_t;                   VOICE_LINE_SUMMARY
int  vision_capture(vision_result_t *out);   /* 抓一幀＋分類，≤1.5 s */ } voice_line_t;
void vision_release(void);         int  voice_say(voice_line_t line);      /* 非阻塞，入佇列 */
                                   int  voice_ask(const char *text);       /* V3，可選 */
/* 3. 手部（ESP-NOW） */           void voice_poll(void);                  /* 在主循環呼叫 */
typedef struct {
  uint8_t bin; uint16_t dist_mm;
} drop_event_t;
int  binlink_open_all(void);
int  binlink_start_monitor(uint16_t timeout_ms);
int  binlink_poll(drop_event_t *ev);   /* 0=無事件 1=有事件 */

/* 4. 遊戲核心（唯一知道規則的地方） */
void game_tick(uint32_t now_ms);
void game_input(game_input_t in);   /* PLAY 鍵、vision 結果、drop 事件、逾時 */
```

規則：
- **只有 game core 可以改分數與回合**；其他三個 module 只出事件。
- 每個 module 都有一個 `*_fake.c`：vision_fake 回固定 label；binlink_fake 3 秒後回一個假 DROP；voice_fake 只 print。
- 有 fake，就可以同時做 UI、做模型、做手部，互不阻塞。

---

## 3. Part 拆解（每 part 可獨立驗收）

| Part | 內容 | 依賴 | 用 fake 頂住 | 驗收 | 估時 |
|---|---|---|---|---|---|
| **P0 音頻迴路** | codec 初始化、TDM、播一段 WAV、回錄自測 | 硬件 | — | T5 loopback：440 Hz 回錄 peak >500 | 0.5 日 |
| **P1 短句庫** | 12–15 句預錄音檔（粵語），flash 分區 + 播放佇列、可打斷 | P0 | — | 連續播 20 次無爆音、被打斷後可續播 | 1 日 |
| **P2 語音事件** | ESP-SR 喚醒詞 + 3 個命令詞 → `game_input` 事件 | P0 | PLAY 鍵 | 室內 1 m、50 次：喚醒 ≥45、命令 ≥40 | 1.5 日 |
| **P3 對話通路** | 接小智 pipeline（配網、憑證、Opus 串流），只在 IDLE 可用 | P0、Wi-Fi | 文字模式 | 一問一答 ≤5 s 出聲；斷網 3 s 內提示 | 2 日 |
| **P4 UI** | LVGL 四屏（待機／分類／投放／答案） | vision fake | 假 label | 每屏 <16 ms 重繪，字級符合規範 | 2 日 |
| **P5 手部** | LEDC 四舵機、XSHUT 改址、ToF 判定、ESP-NOW | — | — | 方案 v2 §9 G2／G3 | 3 日 |
| **P6 整合** | 四 seam 接真、全流程 10 回合 | 全部 | — | 方案 v2 §9 G4 | 1 日 |

**關鍵**：P4（UI）與 P5（手部）今日就可以開工，完全唔需要等模型；P1／P2／P3 只要 P0 通就可以並行。

---

## 4. 語音子系統內部設計

### 4.1 音頻路徑（只有一條，要分優先級）

```
mic → codec(ES8389, TDM) → AFE(AEC/NS/wakeword) → [V2 命令詞] → game_input
                                              └→ [V3 雲端] → Opus → 播放
播放佇列優先級： 遊戲提示(V1) > 對話回覆(V3)
```

- V3 講話中收到 V1 提示 → **V3 立即讓路**（對話可被遊戲打斷，反之不可）
- V2 喚醒在 V1 播放期間暫停（避免自己聲音觸發自己）
- 一局進行中（CAPTURE→CLOSE_ALL）**不接受 V3 新問題**，只保留 V1

### 4.2 為甚麼 V3 一定要在流程外

- 場地 Wi-Fi 不可控：首字延遲 1–3 s，最差斷線
- 一局只有 5 次計分事件，任何一次等雲端 = 直接 0 分
- 評審問「網絡斷咗點算」時，答案應該是「遊戲照跑，對話停用」，而不是「我哋再試吓」

### 4.3 降級表

| 情況 | V1 | V2 | V3 |
|---|---|---|---|
| 無網 | 正常 | 正常 | 停用，畫面顯示「對話不可用」 |
| SD 卡缺失 | 用 flash 音檔 | 正常 | 正常 |
| mic 初始化失敗 | 正常 | 停用 | 停用 |
| CPU 過載（推理中） | 正常 | 暫停 | 暫停 |

---

## 5. 憑證與配網（不可寫入 repo）

- 小智雲端需要 API 憑證 → **用 VaultKnox 存放**，用 `hermes-vault add` 寫入，**不要貼在對話、不要 commit、不要寫進 sdkconfig 預設值**
- 裝置端只存「配網後的 Wi-Fi 憑證」與「一次性 provisioning token」，存 NVS
- repo 內只准出現 `CONFIG_*_TOKEN=""` 這類空值與讀取程式碼
- 換機／換場地時走 provisioning 流程，不靠改 source

---

## 6. 整合順序（時間軸）

```
11/04 ─ P0 音頻 ─┬─ P1 短句 ─┬──────────────┐
                  ├─ P2 語音事件 ────────────┤
                  └─ P3 對話（可延後）        │
11/04 ─ P4 UI（fake label）──────────────────┤
11/04 ─ P5 手部（G2/G3）──────────────────────┤
11/07 ─────────────────────────────── 合流 ──┴─ P6 整合（G4）
11/09 ─ M2 交付
```

**切線（cut line）**：11/07 若 P3 未通，直接把 AI chat 降級為「場邊展示」（IDLE 時可用），遊戲流程不依賴它。這個決定要在 11/07 當日做，不要拖到 11/09。

---

## 7. 風險

| 風險 | 影響 | 對策 |
|---|---|---|
| ES8389 必須 TDM（官方 user guide 明寫） | P0 卡住全部語音 | P0 第一天就驗，出問題即問 Codex 對 `es8389_audio_codec.cc` |
| AFE 佔 CPU，與推理爭核心 | 推理 p95 上升 | 推理時暫停 AFE；或把 AFE 釘在另一個核心 |
| 喇叭聲被 mic 收到 → 自我喚醒 | 誤觸發 | AEC + V1 播放時暫停喚醒 |
| 雲端 TTS 首字慢 | demo 難看 | V3 只在 IDLE；V1 全部預錄 |
| 憑證誤入 repo | 安全與失格風險 | VaultKnox + `.gitignore` + review 時 grep `token|key|secret` |

---

## 8. 未決

| # | 問題 | 決定點 |
|---|---|---|
| A1 | V2 是否需要（有 PLAY 鍵是否夠） | 11/06，看 P2 進度 |
| A2 | V3 用哪個雲端（小智官方 vs 自建） | 11/05 前定，牽涉憑證與費用 |
| A3 | 粵語短句由誰錄（真人 vs TTS 生成） | 11/05，影響 P1 |
| A4 | 對話內容範圍（只答垃圾分類，還是自由聊天） | 11/06，影響 prompt 與評審觀感 |
