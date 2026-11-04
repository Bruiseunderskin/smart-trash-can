# AI Chat 的分割與實作計劃

2026-11-04 ｜ 阿吉 the hermes ｜ 配套 `docs/plan-v2-tof.md`

---

## 1. 先分清：這專案裡的「AI Chat」其實是三件不同的事

| 代號 | 是甚麼 | 要網？ | 遊戲能不能沒有它 | 誰做 |
|---|---|---|---|---|
| **V1 固定短句播報** | 開場／答對／答錯／逾時／結算，預錄音檔 | 否 | **不能沒有**（無聲＝demo 死） | 自己 |
| **V2 語音喚醒＋命令詞** | 「開始」「下一張」等，ESP-SR 離線 | 否 | 可以（有 PLAY 實體鍵） | 自己 |
| **V3 對話（ASR→LLM→TTS）** | 雲端問答，backend 可換（見 §4.4） | 是 | **可以沒有**（只做場邊展示） | 自己 |

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


### 4.4 V3 的 backend：小智只是其中一個選項

把 V3 寫成一個介面，三個實作，任何一個通了都算 V3 完成：

```c
typedef struct {
  const char *name;
  int  (*init)(void);
  int  (*ask)(const char *text_or_pcm, void (*on_audio)(const uint8_t*, int));
  void (*deinit)(void);
} chat_backend_t;
```

| 代號 | backend | 需要 | 優點 | 缺點 | 建議 |
|---|---|---|---|---|---|
| **B0** | **本地 FAQ ＋ 關鍵詞**（無 LLM） | 無網、無憑證 | 一定通、零延遲、斷網照跑 | 只答預設問題 | **必做**（保底就是它） |
| B1 | 自建 HTTP streaming：ASR → LLM → TTS | 一個 API key ＋ 一張可靠 SIM/Wi-Fi | 可控、可換模型、粵語 ASR 可選 | 要自己寫 streaming 與斷線處理 | 有時間才做 |
| B2 | 小智 WebSocket（repo 已有） | 小智帳號或自架 server | 現成、Opus 省頻寬 | 綁定它的協議與服務 | 想快就用 |

判斷依據（不要憑感覺選）：

- **粵語 ＋ 小朋友語音**：雲端 ASR 對這兩者都容易失手。一場 demo 有 5–10 條問題，B0 的關鍵詞匹配命中率往往高過 ASR。**先做 B0，再考慮 B1／B2。**
- **頻寬**：B1 用 PCM 16 kHz/16-bit ＝ 32 KB/s，比賽場 Wi-Fi 通常夠；B2 用 Opus 只需幾 KB/s，但綁協議。
- **評分**：AI 那 25% 看的是模型的 confusion matrix 與延遲，不是對話。**對話是觀感分，不應該吃掉模型的分鐘。**

憑證：B1／B2 的 key 一律用 `hermes-vault add` 入 vault，裝置端只取一次性 provisioning token 存 NVS；repo 內只准空值。**任何 key 都不要貼在對話或 commit 裡。**


### 4.5 B1 定案：自建 HTTP streaming（裝置 → 自家 server → 雲端 ASR/LLM/TTS）

**架構原則：裝置永遠只信任自家 server，上游供應商的 key 只存在 server 上。**

```
Korvo-1 ──HTTPS/multipart──▶ 場地 laptop (FastAPI)
   ▲                              │  ├─ ASR（粵語）
   └──chunked PCM 16k─────────────┤  ├─ LLM（OpenAI 相容）
                                  │  └─ TTS（粵語）
                                  └── 日誌／錄音（報告用）
```

放在場地 laptop 而唔係直接由 ESP32 連雲端，三個理由：
1. TLS 與 keep-alive 只做一次，ESP32 端唔需要處理憑證輪替與重連
2. 上游換 vendor 唔需要重燒 firmware
3. 每次問答的延遲、音檔、辨識文字都可以落 log —— 報告要的數字就是這樣來的

#### 協議 v1（刻意選「錄完再送」，不做全雙工）

1. 裝置端 VAD（能量＋ESP-SR VAD）偵測開始／結束；最長 5 s，靜音 600 ms 收尾
2. `POST /ask`，`multipart/form-data`：
   - `meta`：`{"device":"stc-01","round":3,"lang":"yue","max_ms":8000}`
   - `audio`：PCM 16 kHz / 16-bit / mono（raw，無 WAV header）
3. server 回應：`Content-Type: application/octet-stream`，chunked，**先送 1 行 JSON header** 再送 PCM：
   ```
   {"text":"...","reply":"...","sr":16000,"fmt":"s16le","ms":1850}\n
   <PCM bytes...>
   ```
4. 裝置邊收邊寫 I2S（8 KB ring buffer），收到 EOF 即完成
5. 任一階段超時（ASR 3 s／LLM 5 s／TTS 3 s）→ server 回 `X-Fallback: 1` 並改送 B0 的預錄答案

#### 為甚麼唔做全雙工串流

- 全雙工要同時處理上傳、播放、AEC，還要 server 端做 VAD 與打斷，工程量約 3 倍
- 一局只有 5 次計分事件，對話不在流程內，1.5–2.5 s 首字延遲完全可接受
- 先做「錄完再送」，通了再看要不要升級；協議不變（同一個 `/ask`）

#### 延遲預算（目標，需實測）

| 階段 | 目標 |
|---|---|
| VAD 收尾 | 600 ms |
| 上傳 5 s PCM（160 KB）於 LAN | 100–200 ms |
| ASR | 400–800 ms |
| LLM 首 token | 300–800 ms |
| TTS 首 chunk | 300–600 ms |
| **合計首字** | **1.5–2.5 s** |

#### 粵語與小朋友語音的硬性要求

- ASR 必須明確支援 `yue-HK`（唔好靠「中文」模型硬食）
- 驗收：**20 條小朋友風格語音**（含背景噪音），字錯率 WER ≤ 20% 才可上場；超過就每題改用 B0
- TTS 用粵語 voice；短句由 server 生成後**快取成音檔**，同一句第二次直接回檔案（省時省錢）

#### 憑證

- 上游 key 只存在 server 的環境變數／vault，**永不落到裝置**
- 裝置只有一個 `device_token`（配網時寫入 NVS），用作呼叫自家 server 的憑證
- repo 內只准出現變數名（`ASR_KEY`、`LLM_KEY`、`TTS_KEY`）與 `.env.example` 空值

#### 分段實作

| Part | 內容 | 驗收 |
|---|---|---|
| B1-a | server 骨架 ＋ `/ask` 回固定 WAV（無 ASR/LLM） | 裝置收到並播出聲音 |
| B1-b | 接 ASR，回 echo 文字 ＋ 固定回覆 | 20 條語音 WER 報告 |
| B1-c | 接 LLM（分類領域 system prompt，限制在垃圾分類話題） | 20 條問答，離題率 <10% |
| B1-d | 接 TTS streaming ＋ 短句快取 | 首字 ≤2.5 s（p95） |
| B1-e | 超時／斷線 → 自動回落 B0 | 拔網線測試：3 s 內改用 B0 |


### 4.6 B2（小智）的文字輸入：已開 issue #23 給 Codex 試

問題：小智協議**冇「使用者文字」訊息**，只有 `listen/start|stop|detect`、`abort`、`mcp`。唯一有機會零改動的路，是把 `listen/detect` 的 `text` 當成使用者輸入（現時它用來送喚醒詞）。

已委派的三級驗證（見 issue #23）：

1. **P1**：直接用現成 `Application::WakeWordInvoke("呢張卡係咩垃圾")`——零上游改動
2. **P2**：在新板目錄加 `Protocol` 子類，試三種 payload 變體
3. **P3**：若都失敗，唔改上游，直接走 B1

硬限制：**新建板變體 `esp32-s31-korvo-1-stc/`，唔改官方板**；上游改動只准一條 Kconfig entry，並記入 `docs/upstream-patches.md`。

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
| A2 | V3 backend | **已定：B1**（自建 HTTP streaming，見 §4.5）；B0 仍為保底與 fallback，兩者一齊做 |
| A3 | 粵語短句由誰錄（真人 vs TTS 生成） | 11/05，影響 P1 |
| A4 | 對話內容範圍（只答垃圾分類，還是自由聊天） | 11/06，影響 prompt 與評審觀感 |
