# 智分桶 EDU — 總體計劃（Single Source of Truth）

CTEA 2026 香港區賽 · **高級組** ｜ 更新 2026-11-04 ｜ 維護：阿吉 the hermes
本文件整合所有分散決策與子計劃；子文件保留細節，但**範圍、優先級、驗收標準以本文件為準**。

---

## 0. 一頁摘要

| 項目 | 內容 |
|---|---|
| 專案 | AI 垃圾分類桶，4 類別（可回收物／廚餘／有害垃圾／其他垃圾），4 個獨立蓋 |
| 主控 | ESP32-S31-Korvo-1（相機 SC101IOT、4.3″ 800×480 LCD、ES8389 音頻、Wi-Fi） |
| 手部 | ESP32-S3-DevKitC-1（4 舵機、4 × TOF050C 經 TCA9548A） |
| 板間 | ESP-NOW，協議 v1（seq／冪等／重同步） |
| AI | 本機 int8 模型（160×160 RGB）＋ 語音 Q&A（小智雲端，只在 CHAT 模式） |
| 死線 | **M2 2026-11-09**（模型 v1 ＋ 端到端可跑）；M4 2026-11-14（素材初稿） |
| 原則 | **只可以砍功能，唔可以砍證據** |
| 本週並行 | #25 語音（stc_voice）／#26 TOF bring-up／W Ed 訓練模型 |

---

## 1. 目標與評分（決定投資方向）

高級組評分：創新與環保 25 · 工藝 15 · 人機互動 5 · **AI 25** · 報告／影片／展板 20 · 問答 10。

- **AI ＋ 創新 = 50 分**，所以資源優先投在**可量測的 AI 證據**：confusion matrix、per-class F1、裝置 p50/p95、量化前後差異、host/target 一致率
- 語音互動只佔人機互動 5 分 → **AI chat 係加分項，唔係核心**；唔可以阻塞遊戲流程
- 報告 20 分 → 所有數字必須有 git 內可追溯的來源（log／CSV／hash）

---

## 2. 系統架構

```
┌──────────────────────── ESP32-S31-Korvo-1 ────────────────────────┐
│  SC101IOT 相機 ──► 單幀抓拍 ──► 前處理(PPA) ──► int8 模型 ──► label  │
│  4.3″ LCD (LVGL) ◄── 狀態／分數／答案                              │
│  ES8389 音頻 ◄── 本地 PCM 台詞（GAME）／小智 TTS（CHAT）            │
│  應用層：stc_game（唯一改分數）＋ stc_vision ＋ stc_voice ＋ stc_bin│
└───────────────────────────┬────────────────────────────────────────┘
                            │ ESP-NOW（協議 v1：seq／冪等／GET_STATE）
┌───────────────────────────┴────────────────────────────────────────┐
│                    ESP32-S3-DevKitC-1（手部）                       │
│  LEDC ×4 舵機（5V/3A 獨立電源、每顆 470 µF、錯開開啟 80–120 ms）    │
│  TCA9548A(0x70) CH0–CH3 → 4 × TOF050C(VL6180X, 0x29)               │
└────────────────────────────────────────────────────────────────────┘
```

**四條 seam**（`contracts/`）——所有並行工作的切割面，每條都有 stub：

| seam | 職責 | 不得 |
|---|---|---|
| `stc_vision.h` | 抓一幀 → 前處理 → 推論 → `stc_label_t` | 碰 LCD、改分數 |
| `stc_voice.h` | 本地台詞／CHAT 對話／GAME-CHAT 閘 | 改分數、動舵機 |
| `stc_bin.h` | 舵機、ToF、感測器健康狀態 | 改分數 |
| `stc_game.h` | 回合、分數、狀態機（唯一權威） | 直接操作硬件 |

---

## 3. 硬件

### 3.1 BOM（重點件）

| 件 | 數量 | 備註 |
|---|---|---|
| S31-Korvo-1 ＋ SC101IOT | 1 | 已有 |
| S3-DevKitC-1 | 1 | 已有 |
| TOF050C（VL6180X） | 4 | ¥8/粒，0–50 cm，25° FoV，I²C |
| TCA9548A mux | 1 | ¥5，0x70；四粒全留 0x29 |
| 舵機＋獨立 5V/3A 電源 | 4 | 每顆 470 µF |
| 10 kΩ、排線、杜邦 | — | ≈¥11 |
| **新增合計** | | **≈ ¥48（HK$52）** |

### 3.2 ToF 接線與初始化要點

- `S3 ─ I²C(SDA 8, SCL 9, 400 kHz) ─ TCA9548A(0x70) ─ CH0–CH3 ─ TOF(0x29)`
- 通道 i = `stc_bin_t` i（0 可回收物 … 3 其他垃圾）
- 每次切 channel 後 **≥100 µs** 才讀；量測週期 20 ms，四通道一輪 ≈ 80–90 ms
- 每粒 init 四步：`FRESH_OUT_OF_RESET` 檢查 → AN4545 private registers → `RefSpadManagement` → `OffsetSimpleCalibration(50 mm)`；有保護窗加 `XTalkCalibration(100 mm)`；校正值存 NVS
- **不用** SHDN 改址（已核實技術可行但需每次上電重做，程式多 150–200 行；方案存 git 歷史作後備）

### 3.3 電源

- 舵機 5V/3A 獨立；`OPEN_ALL` 錯開 80–120 ms；四蓋共地、回流線直接回 PSU
- ToF 與 S3 用 3V3；**不可與舵機共用 5V rail**
- 4 × 40 mA（ToF）計入 3V3 預算

---

## 4. 模型契約（訓練必須沿用）

`contracts/model/README.md`、`classes.json`、`preprocess.json` 為唯一真相。

1. **類別順序凍結**：0 可回收物／1 廚餘／2 有害垃圾／3 其他垃圾
2. **輸入**：RGB、uint8、NHWC、**160×160**，歸一化在 ONNX 圖內
3. **預處理**：YUV422(UYVY) → RGB → **中心裁切正方形（640×480 → 480×480）** → bilinear resize；**不用 letterbox**
4. **輸出**：`float32[1,4]` softmax；`conf < 0.55` → abstain
5. **必交**：`model.onnx`（opset ≤17）、`classes.json`、`preprocess.json`、`eval.json`（confusion matrix ＋ per-class P/R/F1 ＋ 裝置 p50/p95）
6. **門檻**：macro-F1 ≥ 0.80；每類 recall ≥ 0.70；裝置 p95 ≤ 300 ms
7. **最易錯**：augmentation 只作用在裁切後的 RGB 圖；評估必須用與推論完全相同的管線
8. `contract_version` 不符 → 韌體拒絕啟動

---

## 5. Part 分解

每個 Part 都可獨立驗收；依賴關係見 §6。

### P0 手部 bring-up（ToF ＋ 舵機）— Issue #26

- **範圍**：mux 切換、四粒 init＋校正、序列埠命令、`stc_bin` 實作、G1 量測工具
- **依賴**：無（不需要 S31、不需要模型、不需要 UI）
- **驗收**：H1–H6（四粒辨識／100 mm σ ≤5 mm／每通道 ≥10 Hz／10 分鐘零錯誤／熱插拔 fault bit／NVS 校正持久）
- **不做**：ESP-NOW、舵機實作（先回 `STC_ERR_UNSUPPORTED`）

### P1 語音與 AI chat — Issue #25

- **範圍**：`stc_voice`（本地台詞、模式閘、CHAT 入口、60 s 自動退出）、`stc_game` 最小真實邏輯、stub vision/bin
- **依賴**：無（stub 可跑完整 5 回合）
- **驗收**：A–G（5 回合 smoke／台詞 <50 ms／模式互斥／GAME 零連線／CHAT 真人語音／60 s 退出／契約版本）
- **關鍵未知**：**真人語音路徑從未實測**（Codex 只確認 WakeNet 初始化成功）→ 11/07 必驗

### P2 視覺模型 — Issue #12

- **範圍**：資料分組去重、訓練、int8 量化、ONNX↔PyTorch parity、confusion matrix、裝置延遲
- **現況**：SC101IOT 實測 4 張準確率 0.25（closed-set）、abstain 2、accepted accuracy 0；p50 83.8 ms／p95 83.9 ms；int8 host macro-F1 0.833 vs float 0.865
- **解讀**：0.25 是 **domain gap**（garbage265 網圖 vs 真實課室照片），修法是現場資料，不是換更大模型
- **驗收**：macro-F1 ≥ 0.80（獨立測試集）、per-class recall ≥ 0.70、裝置 p95 ≤ 300 ms、host/target 類別一致率 100%（**只可講「一致」，不可講「準確」**）

### P3 板間通訊（ESP-NOW）

- **範圍**：協議 v1（`seq`／冪等去重 5 s／300 ms 重送 ×3／`GET_STATE` 重同步／200 ms 心跳）
- **依賴**：P0 完成後（要真實的 ToF 事件）
- **驗收**：拔電任一板 10 s 內重同步；100 次指令 0 次重複執行

### P4 UI（LVGL 四屏）

- **範圍**：待機／分類中／等待投放／答案／結算；只讀 `stc_game_snapshot()` 與事件
- **依賴**：P1 的事件順序（可先用 stub 資料）
- **驗收**：每屏 <16 ms 重繪；字級符合簡報可讀性；GAME/CHAT 模式顯示正確

### P5 遊戲核心整合

- **範圍**：`stc_game` 完整狀態機（CAPTURE→OPEN→WAIT→SETTLE→CLOSE→NEXT）、5 回合、+10/−5/0、逾時、結算、分數存 NVS/SD
- **依賴**：P0、P1、P2、P3、P4
- **驗收**：連續 10 回合 0 人工介入；端到端 p95 ≤ 3 s

### P6 交付物

- **範圍**：報告（含所有證據表）、5 分鐘影片、A0 海報、問答稿（預備 20 條）
- **依賴**：P2／P5 的數字
- **驗收**：每個數字都能指到 git 內的 log／CSV／hash

### P7 現場影像驗證（field probe）

- **範圍**：17 張獨立來源圖片（RealWaste + viola77），只答一個問題：**換影像來源時模型掉幾多**
- **現況**：PR #14 已開；待模型跑完出 per-image 表
- **鐵律**：不可用來訓練、不可用來調 threshold、**不可講「現場準確率」**

---

## 6. 時序與里程碑

```
11/04 ─┬─ P0 TOF bring-up（#26）
       ├─ P1 語音（#25）
       └─ P2 模型訓練（W Ed）
11/07 ─┤ 合流檢查：P1 真人語音若失敗 → 改用按鍵直接進聆聽
11/09 ─┤ ★ M2：模型 v1 ＋ 5 回合可跑 ＋ 契約凍結
11/10 ─┤ P3 ESP-NOW ＋ P4 UI
11/12 ─┤ ★ M3：端到端 v1（p95 ≤3 s、10 連跑）；紅期開始（影片＋海報）
11/14 ─┤ ★ M4：素材初稿
11/21 ─┤ 紅期結束 → 紫期緩衝（只修 bug、不改功能）
```

**Cut-line（按此順序砍，永不砍分類本身）**
`背景移除 → AI chat → 語音動畫 → 4 桶減至示範 12 件物件 → 承認 3 類並在問答說明`

---

## 7. 決策記錄（已拍板，不再重開）

| # | 決策 | 理由 | 日期 |
|---|---|---|---|
| D1 | 語音走 **B1（自建 HTTP）＋ B2（小智原生）分工**，遊戲流程只用本地台詞 | B1 可控可量測；B2 保留真人語音問答 | 11-04 |
| D2 | **小智在閘後**：GAME 模式零連線、CHAT 才開 | AFE 爭 CPU、server 會叫相機（實測 9.15 s）、喇叭自我觸發 | 11-04 |
| D3 | **不新增 `kDeviceState`**，用時序互斥 | `kDeviceState` 在 main/ 出現 160 處，改動與 rebase 成本遠高於收益 | 11-04 |
| D4 | 文字注入（#23／#24）**降級為技術驗證**，不作遊戲依賴 | `WakeWordInvoke` 只在 Idle/Notifying 生效，Speaking 丟字、Listening 關通道 | 11-04 |
| D5 | ToF 用 **TOF050C（VL6180X）＋ TCA9548A**，不用 SHDN 改址 | mux 省 150–200 行程式與 4 條 GPIO；SHDN 方案存 git 後備 | 11-04 |
| D6 | 相機 `GetCamera()` 回 `nullptr`（不向 MCP 註冊） | 避免雲端 LLM 觸發 `take_photo`，延遲主因 | 11-04 |
| D7 | 模型輸入 **160×160、中心裁切、不用 letterbox** | 與 PPA 管線一致，訓練/推論同構 | 11-04 |

---

## 8. 風險登記

| 風險 | 影響 | 對策 | 擁有者 |
|---|---|---|---|
| 真人語音路徑從未實測 | CHAT 功能可能整條不可用 | 11/07 前實測；失敗改按鍵 `StartListening()` | P1 |
| 蓋全開落於 ToF FoV | 誤檢、假事件 | G1 先量；必要時改位置＋擋光片 | P0 |
| domain gap（0.25 準確率） | AI 分數低 | 現場資料補救；縮小示範物件集 | P2 |
| 舵機電流不足 | 四蓋同時開失敗 | 3A 電源＋470 µF＋錯開開啟 | P0 |
| 契約漂移 | 訓練與韌體不一致 | `preprocess.json` 版本檢查，不符拒絕啟動 | P2/P5 |
| 場地 Wi-Fi 不穩 | CHAT 失效 | GAME 模式零依賴；本地台詞必定可播 | P1 |
| 時間不足 | 功能做不完 | 按 §6 cut-line 砍，永不砍證據 | 全體 |

---

## 9. 證據清單（報告／答辯要引用的數字）

| 證據 | 狀態 | 來源 |
|---|---|---|
| 模型 confusion matrix ＋ per-class F1 | 待 P2 更新 | `docs/issue12/` |
| int8 vs float 精度差（3 點） | 已有 | issue #12 報告 |
| 裝置推理 p50/p95（83.8／83.9 ms） | 已有（舊模型） | `live-camera/metrics.json` |
| host/target 類別一致率 100%（≠ 準確） | 已有 | `s31-host-parity.json` |
| 現場影像 per-image 表（17 張） | 待模型跑 | PR #14 |
| ToF 偵測漏檢率 / 誤檢率 | 待 P0 | `docs/g1-results.csv` |
| 端到端延遲 p95 | 待 P5 | `timings.csv` |
| 語音 TTFB（模式 S／Q） | 待 P1 | `VALIDATION.md` |

---

## 10. 未決

| # | 問題 | 決定點 |
|---|---|---|
| U1 | 蓋全開是否落於 ToF FoV | G1（11/06） |
| U2 | 15 s 監聽窗是否夠小朋友 | 實測投放時間分佈 |
| U3 | 評審自由問答用 B1 文字還是 B2 語音 | 11/08（看 B1 是否接通） |
| U4 | 每局 5 回合是否改 3 回合（時間控制） | 11/10 |

---

## 11. 子文件索引

| 文件 | 內容 |
|---|---|
| `docs/plan-v3-tof050c.md` | ToF 選型、接線、校正、Codex 實作範圍 |
| `docs/ai-chat-implementation-plan.md` | 語音／CHAT 模式與 stub 範圍 |
| `docs/ai-chat-partition.md` | B1 vs B2 分析、模式閘、為何不加 DeviceState |
| `contracts/` | 四條 seam 的 C 標頭 ＋ 模型契約 |
| `docs/ROADMAP.md` | 舊版日曆（以本文件 §6 為準） |
| `docs/architecture.md` / `docs/development.md` | 架構與開發流程 |
