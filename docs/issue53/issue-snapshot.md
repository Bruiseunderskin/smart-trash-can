> **讀者**：Codex（有 terminal、**無 S31 硬件**）＋ **Bruiseunderskin（實機持有人，負責上板驗證）** ＋ W Ed（拍板者）。
> **何時讀**：任何整合工作開始之前，由頭讀到尾。這份是執行文件，不是願景文件。
> **開始前必讀**：`AGENTS.md` → `docs/agents/workflow.md`（六道閘）→ `docs/architecture.md`。

## 0. 一句話

`main` 上現在是一堆各自能跑、彼此不相識的碎片：UI 有自己的 API、模型有自己的標籤次序、契約有四個 seam 但**零個實作**。這個 issue 的工作是把它們接成一條**本機、離線**的路徑：

```
相機幀 → 前處理 160×160 → int8 推理 → 標籤映射（→ 凍結桶位 0–3）→ stc_game 狀態機 → LVGL 畫面 ＋ 開一個蓋
```

**Definition of done**：Codex 交出一條在 host 上可用重播幀跑通的完整路徑（`ctest` ＋ 三個 gate 全綠 ＋ 逐次時序 JSON），並交出**可編譯**的 S31 相機 bring-up；Bruiseunderskin 用 `RESULT T2 PASS` 一行＋實機計時完成最後一段。

---

## 0.5 已拍板（2026-11-08，W Ed）— 照此執行，唔好再問

| # | 題目 | 決定 | 落地方式 |
|---|---|---|---|
| D1 | UI 契約與實作唔通（B1） | **用新嘅一套（`ui/lvgl` 的 `stc_ui_create/apply` API）做標準** | `contracts/stc_ui.h` 升 **v2.0.0** 對齊現有實作；`ui/lvgl/src/stc_ui.c` 一行都唔改（見 I0） |
| D2 | 模型標籤次序（B2） | **跟現行模型（`mixed_s31.espdl`）的次序做標準** | 改 `contracts/model/classes.json` 的 index 次序為 `food, hazardous, other, recyclable`；桶位 0–3 不變，另設固定映射表（見 §6.1） |
| D3 | S31 相機＋codec 未實作（B3） | **Codex 做** | I6：相機 bring-up（esp-video V4L2），編譯＋CI 綠即可交；上板實測由 Bruiseunderskin 做 |
| D4 | 模式閘＋遊戲核心零實作（B4） | **Codex 做** | I3：`stc_mode.c` ＋ `stc_game.c` 由零實作，唔准抄 #41 的 `stc_app_fsm.c` |
| D5 | 屏幕規格矛盾（B5） | **800×480，4.3 吋，S31-Korvo-1 標準模組** | 以 800×480 為唯一目標；順手改 `AGENTS.md` §1 的 480×272 事實（見 I0） |
| D6 | 蓋與感測用邊條路（B6） | **USB CDC 有線 ＋ S3 關 Wi-Fi**：S31（USB host）→ S3（CDC device）→ PCA9685 `0x40` ＋ TCA9548A `0x70` → 4 × TOF050C | 新階段 **I7** 做真實作（S31 host 端 ＋ S3 節點 firmware）；ESP-NOW 只留 config flag 註解；見 §6.4、§7 |
| D7 | PR #41 處置 | **已關閉（2026-11-08，owner 決定）**；有價值部分已合併（main `e5d3af7`，PR #54） | 分支保留一個月作對照；分支內的 `stc_voice.cc` 不沿用，要按 main 契約重寫（issue #25） |

> 七項全部拍板，沒有未決項；Codex 可由 I0 直接開工。

---

## 1. 硬邊界（違反任何一條，這個 PR 就唔會 merge）

1. **`contracts/*.h` 只准改 §0.5 批准的兩處**：`stc_ui.h` 升 v2.0.0、`contracts/model/classes.json` 改次序。兩者都要升版本、同步 `docs/`、在 PR 講明「依 2026-11-08 決定」。**其他任何契約改動 → 停手，開 issue。**
2. **唔可以用 PR #40／#41 做 base，亦唔可以 rebase 佢哋。** 兩條都基於舊契約（v1.0.0，main 已 v1.2.0），而且 **#41 會刪 `contracts/stc_mode.h` 同 `contracts/stc_ui.h`**。只可以按檔抄需要的邏輯，落 main 為本嘅新分支。
3. **硬件驅動只准寫 I6（S31 相機）與 I7（CDC 節點鏈路）。** codec、SD、ESP-NOW 一律留介面後面（回 `STC_ERR_UNSUPPORTED` 的 stub）。節點 firmware **必須關 Wi-Fi**。
4. **分類路徑零網絡。** `server/chat/` 同 xiaozhi 雲端在本 issue 係不可達代碼；GAME 模式一條 socket 都唔可以開。
5. **Gate 紅即停。** `check_docs.py`／`check_mode_gate.py`／unittest 失敗係要報告的 blocker，**唔可以改 gate、改期望值、改 fixture 令它變綠**。
6. **每個 commit 要有 `CHANGELOG.md` `[Unreleased]` 條目**；程式與 commit message 用英文，文件用繁體中文。
7. **角色一律叫「綠豆」**，「小智」在 UI 文案與本 issue 產出中永久禁用（上游代碼名稱例外）。
8. **數字要有來源。** 每個延遲／準確率都要寫「哪個模型、哪個資料集、幾張、邊部機」。無實測就寫 `未測`，唔可以用估算填結論。

---

## 2. 現況地圖（2026-11-08 實查 `origin/main`，語音素材合併後為 `e5d3af7`）

| 部件 | 路徑 | main 上狀態 | 證據／限制 |
|---|---|---|---|
| 契約 | `contracts/stc_{types,vision,voice,bin,game,ui,mode}.h` | 七份定稿：types v1.0.0、vision v1.2.0、voice v1.1.0、bin v1.2.0、game v1.2.0、ui v1.1.0、mode v1.1.0 | **全 repo `grep`：冇任何 `.c/.cc` 呼叫 `stc_vision_scan`／`stc_game_init`／`stc_bin_init`／`stc_voice_init`／`stc_mode_init`。契約零實作。** |
| 模型產物 | `models/mixed-card-object/mixed_s31.espdl`＋`labels.txt`＋`SHA256SUMS` | int8，S31 目標 | 標籤次序 **`food, hazardous, other, recyclable`**（＝本 issue 的模型標準）；主機模擬 292 張同源圖 macro-F1 0.8203；**未上實機** |
| 模型契約 | `contracts/model/classes.json`、`preprocess.json` | 要改 | 現時 index 次序係 0 可回收／1 廚餘／2 有害／3 其他，**同模型相反** → 由 I0 改；preprocess：RGB uint8 NHWC 160×160、UYVY→RGB（PPA、BT.601）→ center crop 480×480 → bilinear（**不用 letterbox**）、softmax `[1,4]`、`conf < 0.55` 拒判 |
| UI | `ui/lvgl/src/stc_ui.c`、`include/stc_ui.h`、`generated/`、`preview/`（SDL） | **真代碼**，`ctest` 過，`preview/render/*.png` 存在 | API：`stc_ui_create`／`stc_ui_apply(stc_payload_t)`／`stc_ui_set_action_callback`；狀態 `idle｜capturing｜classifying｜result_ok｜result_retry｜error｜category_guide｜learning｜summary`；**此為標準** |
| 模式閘 | `docs/mode-gate.md` v1.1.0、`contracts/stc_mode.h` | 規格定稿，**零實作** | GAME 由圖鑑／`IN_CHAT_ENTER` 進 CHAT；`GAME_ROUND × IN_SET_LONG` 拒絕；閒置 **60 s**（`check_mode_gate.py` 把關） |
| 語音 | `contracts/stc_voice.h` v1.1.0；上游 `firmware/xiaozhi-esp32/`；六句台詞 `firmware/assets/voice/`（main `e5d3af7`） | 上游完整；台詞素材已入 main；`stc_voice.cc` 未實作 | GAME＝本地預錄 PCM、零雲端；CHAT＝上游 ASR→LLM→TTS；素材說明見 `firmware/assets/voice/README.md` |
| 板型（S3） | `firmware/xiaozhi-esp32/main/boards/smart-trash-can/` | 可用 | 4 路 LEDC（50 Hz／13-bit）GPIO9–12 ＋ MCP `self.trash_can.*`；已見 3 個舵機轉動 |
| 舵機（另一路） | `firmware/esp32s3/pca9685_servo_test/`、`firmware/arduino/PCA9685_4Servo_Test/` | 可用 | PCA9685 `0x40` CH0–3；**與 GPIO 方案互斥** |
| S31 bring-up | `firmware/esp32s31/bringup/` | T1 LCD 已寫未實測；**T2 相機未實作（TODO）**；T3–T5 卡 ES8389 codec；SD 未 mount | 每次印 `RESULT Tn PASS/BLOCKED` 貼回 #11 |
| ToF／投放偵測 | 只在 PR **#40**：`firmware/esp32s3/tof_bringup/main/{stc_bin.c,tof_driver.c,drop_detector.c}` | 分支限定 | 與 main 衝突中 |
| S31 板目錄＋FSM | 只在 PR **#41**：`.../boards/espressif/esp32-s31-korvo-1-stc/{stc_app_fsm.c,stc_game.c,phrases.json,stub-labels.json}` | 分支限定 | **同分支刪 `stc_mode.h`、`stc_ui.h`** |
| 對話後端（後備） | `server/chat/main.py`（FastAPI，`POST /ask`，PCM 16 k） | 可用，`STUB=1` 可跑 | 本階段**不用**；B1 係後備方案 |
| 設計 | `design/tokens.json`、`layout.json`（`width:800, height:480`）、`export_figma.cjs` | 由 Figma 讀回 | `docs/ui/info-mode-flow.md` v1.4 係 UI 規格權威 |
| 工具 | `tools/{check_docs,check_mode_gate,evaluate_field_probe,check_onnx_parity,quantize_espdl,capture_camera_serial,dev}.py` | 可用 | `dev.py` 係統一入口 |

---

## 3. 六個缺口（已附解法，照做即可）

**B1 — 兩套 UI API 互不相通。** `contracts/stc_ui.h` v1.1.0 宣告 snapshot＋screen 模型；`ui/lvgl/src/stc_ui.c` 用 payload＋action callback 模型。
→ **決定：新 API 為標準。** I0 把 `contracts/stc_ui.h` 改寫成 v2.0.0，內容與 `ui/lvgl/include/stc_ui.h` **逐個簽名一致**（`stc_ui_t*`、`stc_payload_t`、`stc_state_t`、`stc_action_t`、`stc_ui_create/destroy/apply/set_action_callback/show_guide/show_learning/show_summary/retry`），並在檔頭寫 `contract v2.0.0 — 對齊 ui/lvgl 實作（2026-11-08 決定）`。**`stc_ui.c` 與 `include/stc_ui.h` 一行都唔改。**

**B2 — 模型標籤次序同契約相反。** `labels.txt`＝`food, hazardous, other, recyclable`；契約現時 0 可回收／1 廚餘／2 有害／3 其他。
→ **決定：跟模型。** I0 改 `contracts/model/classes.json`：index 0 `food`(bin 1)、1 `hazardous`(bin 2)、2 `other`(bin 3)、3 `recyclable`(bin 0)，`version` 升 2.0.0。桶位 enum **不動**。啟動時檢查 `labels.txt` 的次序與 `classes.json` 完全一致，不一致即 `E_MODEL_LABELS` 拒絕啟動（唔准靠字串猜）。

**B3 — S31 相機未實作。** `firmware/esp32s31/bringup/main/t2_camera.c` 係 TODO（esp-video／V4L2 五步），`bringup_codec_init()` 只拉高 PA（GPIO7）。
→ **決定：Codex 做相機（I6）**，codec 仍留給人類。相機要求：SC101IOT 出 UYVY 640×480、30 幀內 ≥28 有效、量 fps、印 `RESULT T2 PASS` 行；編譯以 `idf.py --preview set-target esp32s31` 為準，CI 綠即可交，**唔可以寫「已驗證」**。

**B4 — 模式閘與遊戲核心零實作。**
→ **決定：Codex 由零實作（I3）**。`stc_mode.c` 依 `docs/mode-gate.md` v1.1.0 六狀態全轉移表；`stc_game.c` 依 `contracts/stc_game.h` v1.2.0（5 回合／180 s／+10／−5／floor 0／learning 6 s）。**唯一可改分數者係 `stc_game`。**

**B5 — 屏幕規格矛盾。**
→ **決定：800×480、4.3 吋、S31-Korvo-1 標準模組。** I0 順手改 `AGENTS.md` §1 該行（及 `CHANGELOG.md` 記一條），其他文件一律 800×480。

**B6 — 執行器與感測路徑。**
→ **決定：USB CDC 有線，S3 節點關 Wi-Fi。** S31 用 USB-A host 口（esp-bsp `bsp_usb_host_start()` 已封裝 500 mA 限流）接 S3 的 CDC；S3 一條 I²C 掛 PCA9685 `0x40`（四舵機）與 TCA9548A `0x70`（CH0–3 各一粒 TOF050C，同為 `0x29`，靠 mux 隔離）。四個硬約束：
1. **GPIO19／20 不可用**（S3 原生 USB 的 D-／D+）；I²C 用 GPIO8／9，避開 strapping 0／3／45／46 與 PSRAM 35–37。
2. **一次只開一個 mux 通道**（`1 << ch`），否則四粒 TOF 的板載上拉並聯。
3. **500 mA 硬上限**：S3 關 Wi-Fi 後 ≈ 40 mA ＋ PCA9685 10 mA ＋ TOF 160 mA ≈ 210 mA，安全；開 Wi-Fi 峰值 355 mA 會超標。
4. **舵機獨立 5V**：四粒 MG90S 堵轉峰值約 2.8 A，三塊板共地，電源不經 USB。

另外 `docs/plan-v3-tof050c.md` §1 仍寫「SHDN 逐粒改址、不用 mux」，與本決定及 `contracts/stc_bin.h` v1.2.0 不一致 → 由 I0 改正。

---

## 4. 目標執行時架構

| 單元 | 責任 | 週期 | 邊界 |
|---|---|---|---|
| `app_main` | 初始化次序（§5），掛 hooks | 一次 | 失敗即入 `STC_ST_FAULT`，畫面顯示錯誤碼 |
| `stc_mode` | 唯一有權決定 GAME／CHAT | 10 ms tick | 唔碰 GPIO、唔碰分數 |
| `stc_game` | 回合、計分、逾時、結算 | 10 ms tick ＋ 事件 | 唯一可改 `score` 者 |
| `stc_vision` | 掃描一次：開相機→5 幀（間隔 50 ms）→前處理→推理→平均 softmax→argmax→關相機 | 只在 `SCAN` 期間 | 單次 `timeout_ms`；解碼 < 3 幀 → `STC_ERR_IO`；逾時仍要關相機 |
| `stc_ui` | 讀 payload、畫 | LVGL task，30 ms | 只可讀狀態；唔可改分數／切模式／開蓋 |
| `stc_lid` | 開關蓋命令 | 事件驅動 | 命令狀態即蓋狀態（無位置感測） |
| `stc_voice` | GAME 本地 PCM 佇列；CHAT 交上游 | 每 tick poll | 起播 < 50 ms；可打斷 |
| `stc_lid_link` | CDC 幀收發、序號、重試、CRC | 事件驅動 ＋ 20 ms poll | 只有它可以碰 USB 主機堆疊 |

**並發規則**：三個 queue，全部有界，滿即 log ＋ drop，不准無限增長。

| Queue | depth | item | 生產→消費 |
|---|---|---|---|
| `q_app` | 16 | `stc_event_t` | UI 觸控／按鍵 → `stc_game_input` |
| `q_vision` | 2 | `stc_label_t` ＋ `stc_burst_detail_t` | vision task → game |
| `q_lid` | 8 | `{bin, action, t_ms}` | game → lid |

記憶體預算先按「static 64 KB ＋ heap 動態」寫在 `docs/integration-runtime.md`，實測數字留空並標 `未測`。

---

## 5. 啟動次序（照抄，唔要自行重排）

1. NVS 初始化 → 讀 version／校正值
2. `stc_mode_init()` → `STC_ST_BOOT`
3. `stc_lid_init()` → `stc_lid_link_start()`：枚舉 S3 節點（2 s 超時）；失敗 → `fault_mask |= 0x1` 並轉 loopback，**不阻開機**
4. `stc_ui_init()` → 顯示 BOOT 畫面
5. `stc_vision_init("models/mixed-card-object")` → **核對 `labels.txt` 次序 = `classes.json` 次序、`preprocess.json` 的 `contract_version` = 韌體常數；任何一項不符即 `E_CONTRACT`／`E_MODEL_LABELS` 拒絕啟動**
6. `stc_game_init()` ＋ `stc_game_set_config()`（5 回合／180 s／+10／−5／floor 0／learning 6 s）
7. `stc_bin_init()`（sensor），失敗 → `fault_mask`，唔好阻開機
8. `stc_voice_init()` → GAME 台詞佇列預熱
9. `stc_mode_input(IN_BOOT_OK)` → `STC_ST_GAME_IDLE`

每一步的失敗碼都要寫入 `stc_mode` 的 FAULT 原因，並在 `docs/integration-runtime.md` 用一張表列出「失敗碼 → 畫面文字」。

---

## 6. 三個必須釘死的介面

### 6.1 標籤映射（固定表，唔准靠字串猜）

模型次序（`labels.txt`，`classes.json` v2.0.0 同序）→ 桶位（`stc_types.h`，凍結不動）：

| 模型 index | 模型標籤 | → 桶位 `stc_bin_t` | 中文 |
|---|---|---|---|
| 0 | `food` | 1 | 廚餘 |
| 1 | `hazardous` | 2 | 有害垃圾 |
| 2 | `other` | 3 | 其他垃圾 |
| 3 | `recyclable` | 0 | 可回收物 |

實作：`static const uint8_t k_model_to_bin[4] = {1, 2, 3, 0};`，加 `_Static_assert` 檢查兩邊 `COUNT == 4`。**啟動時**比對 `labels.txt` 逐行次序；不符即拒絕啟動。未知 label 檔內容 → `E_MODEL_LABELS`，唔可以默認落 `other`。

### 6.2 前處理 golden vectors

放 `firmware/common/test/fixtures/`，總大小 ≤ 200 KB：

- `frame_640x480.uyvy`（真 SC101IOT 幀，或由 repo 內 capture 轉出）
- `expected_rgb160.sha256`：經「UYVY→RGB→center crop 480×480→bilinear 160×160」後 tensor 的 sha256
- `expected_softmax.json`：同一個 tensor 餵模型的期望輸出，容差 `1e-5`
- `expected_label.json`：映射後桶位值

測試名：`test_preprocess_golden`、`test_label_map`。

### 6.3 時序 instrumentation（報告用）

每個階段印一行 JSON，UART／log 一模一樣：

```json
{"ev":"scan","k":1,"t_open_ms":0,"t_capture_ms":243,"t_pre_ms":11,"t_infer_ms":88,"conf":0.83,"raw":"food","bin":1,"src":"real"}
```

`p95` 只可以由這些行算出來；唔准在代碼其他地方自行填時間。

### 6.4 CDC 幀格式（兩邊編譯同一份 header）

```c
typedef struct __attribute__((packed)) {
    uint8_t  sof;        /* 0xA5 */
    uint8_t  ver;        /* 1 */
    uint8_t  type;       /* 0 LID_CMD, 1 LID_ACK, 2 NODE_STATE, 3 NODE_FAULT */
    uint8_t  seq;
    uint8_t  bin;        /* 0–3，type=LID_CMD 時有效 */
    uint8_t  action;     /* 0 CLOSE_ALL, 1 OPEN */
    uint16_t tof_mm[4];  /* type=NODE_STATE 時有效，其餘 0 */
    uint16_t crc16;      /* CCITT-FALSE，覆蓋 sof 至 tof_mm */
} stc_lidmsg_t;           /* 固定 16 bytes，little-endian（需 _Static_assert 檢查） */
```

規則：固定 16 bytes 一幀（1+1+1+1+1+1 ＋ 4×2 ＋ 2），不做流式解析以外的猜測；CRC 錯即丟並累加 `crc_err` 計數；`LID_CMD` 300 ms 內收不到同 `seq` 的 `LID_ACK` → 重試 2 次 → 回 `STC_ERR_IO`，該回合顯示「蓋門無反應」；節點每 100 ms 送一次 `NODE_STATE`（四粒 TOF 讀值），主機側超 500 ms 未收到即標節點離線。幀定義放 `firmware/common/stc_lidmsg.h`，兩邊 `#include` 同一份。

---

## 7. 分階段交付（每段一個 PR，Codex 全部 host 可驗；只有 I6／I7 需要編譯 target）

### I0 — 契約對齊（最先做，唔做後面全部唔成立）
- 改：`contracts/stc_ui.h` → **v2.0.0**（逐個簽名對齊 `ui/lvgl/include/stc_ui.h`）；`contracts/model/classes.json` → **v2.0.0**（模型次序 ＋ `bin` 欄）；`AGENTS.md` §1 屏幕事實改 800×480；`docs/plan-v3-tof050c.md` §1 改為 TCA9548A 方案（現時寫 SHDN 逐粒改址，與契約 v1.2.0 相反）；`CHANGELOG.md` 四條
- 測試：`test_contract_ui`（用 `static_assert` 對齊 `stc_state_t`／`stc_action_t` 數量）、`test_classes_order`（`classes.json` 次序 == `models/mixed-card-object/labels.txt`）
- 完成定義：`check_docs.py`／`check_mode_gate.py` 綠；PR body 寫明「依 2026-11-08 決定，兩處契約改動，其餘零改動」

### I1 — 標籤映射 ＋ golden vectors ＋ 評估 harness
- 新增：`firmware/common/stc_vision_map.c`＋`.h`、`firmware/common/test/fixtures/*`、`tools/evaluate_replay.py`
- 測試：4 個映射值、`labels.txt` 次序不符即 fail、golden preprocess hash、`evaluate_replay.py` 對 `data/field-probe`（17 張）輸出**逐圖表**（truth／pred／conf／correct），**不准印單一總分**
- 完成定義：`ctest` 綠 ＋ PR 附逐圖表

### I2 — frame source ＋ 前處理
- 新增：`firmware/common/stc_frame_source.h`、`stc_frame_replay.c`、`stc_frame_esp.c`（回 `STC_ERR_UNSUPPORTED`）、`stc_preprocess.c`
- 測試：golden vector 對齊、5 幀／50 ms 時序（mock clock）、解碼 < 3 幀回 `STC_ERR_IO`
- 完成定義：`stc_vision_scan()` 在 replay 來源下回得出 4 類標籤，`stc_burst_detail_t` 全欄有值

### I3 — 模式閘 ＋ 遊戲核心
- 新增：`firmware/common/stc_mode.c`、`stc_game.c`（唔准抄 #41 的 `stc_app_fsm.c`）
- 測試：`test_mode_gate`（六狀態 × 全輸入；`GAME_ROUND×IN_SET_LONG` 100 次 0 次進 CHAT；60 s 逾時）；`test_game`（5 回合／計分／逾時／floor／learning 6 s）
- 完成定義：`check_mode_gate.py` 綠；狀態序列 log 貼 PR

### I4 — UI 接線 ＋ SDL 截圖
- 新增：`app/stc_app.c`（把 game snapshot 轉成 `stc_payload_t` 餵 UI）
- 測試：`test_app_wiring`（4 個狀態 → payload 全欄對應）；SDL 截圖 `category_guide`／`learning`／`summary` 三張
- 完成定義：截圖**人手看過**並貼 PR（`ctest` 過 ≠ 畫面對，`AGENTS.md` §4.3）

### I5 — lid 介面 ＋ 時序 hooks ＋ 端到端 replay
- 新增：`stc_lid.h`／`stc_lid_loopback.c`／`stc_lid_stub.c`、`stc_time.h`、`firmware/common/test/test_e2e.c`
- 測試矩陣：**每類 2 張 ＋ 2 張低於 0.55 的拒判樣本 = 10 個 case**，斷言狀態序列、畫面、lid 命令、分數、無 panic
- 完成定義：10/10 case 過；`docs/integration-runtime.md` 寫清楚上板餘下步驟

### I6 — S31 相機 bring-up（唯一硬件代碼）
- 改：`firmware/esp32s31/bringup/main/t2_camera.c`（esp-video／V4L2 五步，參考 `firmware/xiaozhi-esp32/main/boards/common/esp_video.cc`）
- 門檻：30 幀內 ≥28 有效、記錄 fps、印 `RESULT T2 PASS|FAIL code=<n>`；格式 UYVY 640×480
- 完成定義：`dev.py compile --project s31_bringup` ＋ CI 綠；**唔准寫「已驗證」，寫「待上板」**；上板由 Bruiseunderskin 貼 `RESULT` 行到 #11

### I7 — CDC 節點鏈路（S31 host 端 ＋ S3 節點 firmware）
- 新增：`firmware/common/stc_lidmsg.h`（§6.4 的 16 bytes 幀）、`stc_lid_link_cdc.c`（S31 側，`usb_host_cdc_acm`）、`firmware/esp32s3/node/main/{node_main.c,pca9685.c,tca9548a.c,tof050c.c,cdc_dev.c}`、`firmware/esp32s3/node/sdkconfig.defaults`
- 節點要求：**`CONFIG_ESP_WIFI_ENABLED=n`**（節點不需要任何無線）；I²C 用 GPIO8／9、400 kHz；PCA9685 `0x40` 開蓋脈寬逐通道可配；TCA9548A `0x70` 一次只選一個通道；TOF 讀值上報 `NODE_STATE`
- 測試：`test_lidmsg`（`sizeof(stc_lidmsg_t) == 16`、欄位偏移、CRC 覆蓋範圍、序號回繞）、`test_lid_retry`（300 ms × 2 重試後回 `STC_ERR_IO`，用 mock clock）、`test_node_offline`（500 ms 無 `NODE_STATE` 標離線）
- 完成定義：`idf.py build` 兩邊綠（S31 `--preview set-target esp32s31`、S3 `esp32s3`）；`docs/integration-runtime.md` 加一節「節點鏈路」寫明腳位、電源預算、離線行為；**唔准寫「已驗證」**

**砍的次序（由先到後）**：I7 的節點 firmware → I6 → I5 的額外 case → I4 的額外截圖 → I2 的 esp frame source 骨架 → **永不砍 I0／I1 的證據**。

---

## 8. 驗收：分開「Codex 可證明」與「Bruiseunderskin 上板」

### 8.1 Codex 要證明（做不到即未完成）

| # | 項目 | 可驗方式 |
|---|---|---|
| H1 | 四個 seam 都有實作，且無模組繞過 `stc_game` 改分數 | `grep -rn "score" --include=*.c` 逐個解釋；`test_ownership` |
| H2 | 模型→桶位映射正確，`labels.txt` 次序不符即拒絕啟動 | 4 個映射測試 ＋ 錯序 fixture 測試 |
| H3 | 前處理與契約逐位一致 | golden sha256 |
| H4 | 一局 5 回合跑完、計分正確、逾時結算 | `test_game` |
| H5 | 回合中入不到 CHAT（100 次 0 次） | `test_mode_gate` |
| H6 | 10 case e2e 全過，含 2 個拒判 | `test_e2e` log |
| H7 | 離線建置可完成，分類路徑無網絡呼叫 | `CONFIG_STC_OFFLINE_TEST` build ＋ `grep` 無 `http`／`socket` 在路徑上 |
| H8 | 三個 gate 綠 ＋ CHANGELOG 每 commit 有條目 | §10 指令 |
| H9 | 每張 SDL 截圖你親眼看過並貼上 PR | `preview/render/*.png` |
| H10 | 契約只改兩處，其餘 `git diff contracts/` 為空 | PR 貼 diff 摘要 |
| H11 | CDC 幀佈局與 CRC 正確，重試與離線判定照 §6.4 | `test_lidmsg`／`test_lid_retry`／`test_node_offline` |
| H12 | 節點 firmware 已關 Wi-Fi | `firmware/esp32s3/node/sdkconfig.defaults` 內 `CONFIG_ESP_WIFI_ENABLED=n`，PR 貼該行 |
| H13 | 電源預算表已入 `docs/integration-runtime.md`（210 mA 常態／500 mA 上限／舵機 2.8 A 獨立） | 文件連結 ＋ 數字對得上 §3 B6 |

### 8.2 只有上板能做（**唔准寫「已完成」，寫「待上板」**）

- 影相→判類→開蓋整條 **p95 ≤ 3000 ms**（5 個階段分開記）
- 模型在 S31 上 **p95 ≤ 300 ms**
- 4.3 吋 800×480 實屏可讀性、觸控
- 斷網實測、**連續 10 次**（失敗要留）
- 四蓋實際開到位（命令發出 ≠ 開到位）
- I6 的 `RESULT T2 PASS`
- 節點上電後 CDC 枚舉成功、四蓋動作、四粒 TOF 有讀值
- 節點 Wi-Fi 確實關閉（`idf.py size` 或 sdkconfig 對照）

---

## 9. 明確不做（non-goals，寫入 PR「沒有改」一節）

- 不改上游 `firmware/xiaozhi-esp32/main/**`（`application.cc`／`protocols/`／`mcp_server.cc`），不新增 `kDeviceState`
- 不訓練、不量化、不換模型；`models/` 只讀
- 不重畫 UI、不改 `ui/lvgl/src/stc_ui.c` 與 `ui/lvgl/include/stc_ui.h`
- 除 I0 兩處外不改 `contracts/*`
- 唔寫 codec／SD 真驅動；ESP-NOW 不實作（已改 CDC，只留註解）
- 不接 `server/chat`（B1 後備）
- 不改 `tools/check_*.py`

---

## 10. PR 與閘（六道閘的第 4 至 6 道由你執行）

```bash
python3 tools/check_docs.py
python3 tools/check_mode_gate.py
python3 -m unittest discover -s tools/tests -v
grep -rn "90 s\|90000" docs/ contracts/     # 舊值必須清零
```

PR body 用 `.github/pull_request_template.md` 五段：`Closes #53`／改動（逐項對 diff）／沒有改（抄 §9）／驗證（貼上面輸出）／待上板（抄 §8.2）。**body 內每個數字、檔名、版本號都要在 diff 找得到。**

分支命名：`codex/integration-i0` … `codex/integration-i6`，每個對應一段。
**新目錄要加 `README.md`**（`check_docs.py` 會擋）。C 測試建議放 `firmware/common/test/`，開目錄同時寫 README 講「驗什麼、怎麼跑」。

---

## 11. 與 #40 的衝突協議（#41 已關閉，只留記錄）

1. 分支由 `origin/main` 開，唔可以由 #40／#41 開。
2. 需要的邏輯按檔抄，PR 寫「抄自 PR #41 `stc_game.c`，已改為符合 main 契約 v1.2.0」。
3. `contracts/` 除 I0 兩處外逐字等於 main；PR 貼 `git diff --stat contracts/`。
4. 若同一個檔（`stc_game.c`）兩邊都要改，**先留言講，由 repo owner 決定次序**，唔好搶先 merge。
5. **#41 已關閉**（2026-11-08），其分支只作對照；不要把它的檔案搬過來，素材已在 main。

---

## 12. 決策記錄（全部已拍板）

| # | 題目 | 決定 |
|---|---|---|
| D1 | UI 契約 | 用新 API 為標準，`stc_ui.h` 升 v2.0.0 |
| D2 | 模型標籤 | 跟現行模型次序，改 `classes.json` v2.0.0 |
| D3 | 相機／codec | 相機由 Codex 做（I6），codec 留人類 |
| D4 | 模式閘＋遊戲核心 | Codex 由零實作（I3） |
| D5 | 屏幕 | 800×480、4.3 吋、Korvo-1 |
| D6 | 蓋與感測 | USB CDC 有線，S3 關 Wi-Fi（I7） |
| D7 | PR #41 | 已關閉；素材已合併 `e5d3af7` |

---

## 13. 設計來源（UI 唔准自己發明）

- **Figma（唯一視覺來源）**：https://www.figma.com/design/g5J7a5DhDuX1UYucR6jitn
- UI 規格權威：`docs/ui/info-mode-flow.md` v1.4 —— 四區骨架 800×480：邊距 24／TOPBAR 56／BODY 272／ACTION 88；按鈕 232×88、label 32 px；主按鈕全寬貼底。
- 七個對話狀態：`ready／listening／thinking／answering／offline／asr-retry／countdown`（`chat2/01..07`）。
- 角色：**綠豆**，表情 `idle｜happy｜puzzled｜sad｜listen｜talk`。
- Tokens 由 Figma 讀回：`design/tokens.json`、`design/layout.json`；C 端只經 `ui/lvgl/generated/stc_tokens.h` 使用，**唔准硬編顏色**。
- 四類色：可回收 `#1F6FB2`、廚餘 `#C8791E`、有害 `#B3261E`、其他 `#4A4A4A`。

---

## 14. 交付清單（I0–I7 全部完成時）

```
contracts/stc_ui.h                       (v2.0.0，對齊 ui/lvgl 實作)
contracts/model/classes.json             (v2.0.0，模型次序 ＋ bin 欄)
AGENTS.md                                (§1 屏幕事實改 800×480)
firmware/common/stc_vision_map.c/.h
firmware/common/stc_frame_source.h
firmware/common/stc_frame_replay.c
firmware/common/stc_frame_esp.c          (STC_ERR_UNSUPPORTED stub)
firmware/common/stc_preprocess.c
firmware/common/stc_mode.c
firmware/common/stc_game.c
firmware/common/stc_lid.h + stc_lid_loopback.c + stc_lid_stub.c
firmware/common/stc_time.h
firmware/esp32s31/bringup/main/t2_camera.c   (I6，真驅動)
firmware/common/stc_lidmsg.h                  (I7，16 bytes 幀)
firmware/common/stc_lid_link_cdc.c            (I7，S31 host 端)
firmware/esp32s3/node/                        (I7，S3 節點：CDC device + PCA9685 + TCA9548A + TOF)
firmware/esp32s3/node/sdkconfig.defaults      (I7，CONFIG_ESP_WIFI_ENABLED=n)
app/stc_app.c
firmware/common/test/fixtures/{frame_640x480.uyvy,expected_rgb160.sha256,expected_softmax.json,expected_label.json}
firmware/common/test/{test_contract_ui,test_classes_order,test_label_map,test_preprocess_golden,test_mode_gate,test_game,test_app_wiring,test_lid,test_e2e}.c
firmware/common/test/README.md
tools/evaluate_replay.py
docs/integration-runtime.md
CHANGELOG.md（每個 PR 都有 [Unreleased] 條目）
```

一個檔一件事：`stc_game.c` 唔可以順手做埋 lid。

---

## 15. 第一個回覆要講什麼

開工前回一段 ≤10 行：(1) 你由 I0 開始，講邊幾個檔會改；(2) 你打算把 C 測試放在哪個目錄；(3) §2 地圖有無一項你查不到。**唔使等批准就開 I0**——§0.5 與 §12 七項全部拍板，沒有未決項。
