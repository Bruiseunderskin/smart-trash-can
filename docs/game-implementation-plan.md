# 遊戲部分實作計劃

讀者：遊戲控制器、UI、視覺與桶蓋整合的實作者，以及負責上板驗證的人。
何時讀：在修改 `stc_game`、遊戲畫面或端到端流程之前。

本計劃以 `origin/main@d1b86a4`（2026-11-11）為基準。它不是「由零重寫遊戲」
的計劃：PR #61 已合併最小遊戲核心、模式閘、UI adapter 與測試，但目前只證明
host 上的分段行為，尚未證明一局真實遊戲。

> **2026-11-11 stage-one implementation status:** this branch now contains
> `firmware/esp32s31/game/` and `firmware/esp32s3/virtual_hand/`. The shared
> C suite is 18/18 passing, replay/layout Python checks pass, the LVGL native
> test passes, and both ESP-IDF 6.1 target builds succeed. The S31 image embeds
> the real ESP-DL model and generated replay pack; S3 implements the virtual
> hand over ESP-NOW. Physical S31/S3 flashing, the required 40/40 on-device
> model self-test, five-round RF smoke, audio, touch, ACK RTT, and p50/p95
> measurements remain `NOT_TESTED`. The sections below preserve the original
> gap analysis and the reasoning that led to this implementation.

## 1. 目標與完成界線

目標流程：

```text
PLAY → 揀卡提示 → 5 幀掃描 → 類別結果 → 四蓋開啟
     → ToF 偵測投放桶位 → 關蓋 → 每回合作答回饋
     → 第 5 回合或 3 分鐘後結算
```

一局規則維持：

- 5 回合，總時限 180 秒；3 分鐘到時先做完本回合再結算。
- 答對 +10、答錯 -5、沒有投放 0 分、兩次拒判 0 分。
- 顯示分數 floor 為 0；`score_raw` 保留真實加總。
- 每回合相機只開一次，連拍 5 幀；`WAIT_DROP` 時相機必須已關。
- 唯一可改分數與回合的是 `stc_game`；UI、語音與桶蓋不得直接改分。
- GAME 路徑維持本機離線；CHAT 只能由指定入口進入，回合中不可進入。

「host 完成」只代表狀態、計分、adapter 與重播幀測試通過。相機實拍、S31
模型延遲、四個桶蓋實際到位、ToF 漏檢／誤檢、Wi-Fi 鏈路和實屏可讀性都必須
另列上板證據；未量測一律寫 `未測`。

## 2. 已存在、不要重複寫的部分

| 部件 | `main` 現況 | 界線 |
|---|---|---|
| 遊戲狀態與計分 | `firmware/common/stc_game.c` 已有 10 階段、5 回合、+10／-5、floor、abstain 重掃一次 | 只有少量 happy-path 測試；總時限、真實時鐘與部分轉移未實作 |
| 模式閘 | 已加 `app/stc_shell_mode.c` 的 GAME／CHAT 外層 shell；回合中不可入 CHAT，CHAT 暫停 game tick，60 秒退出 | CHAT 語音 backend 仍然 unsupported |
| UI | `ui/lvgl/` 已有 800×480、9 狀態、17 張 framebuffer、50 個 native scenarios | `result_ok` 不能直接到 `learning`，`RESULT_RETRY` 需要兩個候選，現 adapter 沒有提供 |
| 模型 | YOLO11n-cls，S31 w8a8 `models/wordless-padded/wordless_s31.espdl` | 未在 S31 載入；`stc_vision_scan()` 與 ESP 相機 frame source 尚未接成真路徑 |
| 前處理 | UYVY→RGB、中央正方形、160×160 bilinear 已有 golden fixture | 只證明純 C golden，不代表 SC101IOT／PPA／模型整鏈一致 |
| 桶蓋 | USB CDC 協議、ACK／重試／離線邏輯及 S3 節點 source 已有 | 實機 CDC、舵機到位和 ToF 都未驗證；ESP-NOW 另列實驗 |
| 語音 | 本地台詞素材已有 | `stc_voice` 尚未完成 GAME 播放與 CHAT 切換 |

現有 host 測試在 2026-11-11 重跑為 **11/11 passed**，但它們沒有覆蓋本節
「界線」欄列出的問題。

## 3. 已確認的程式缺口

以下均由現行 source 對照規格直接得出，不是估算：

1. **非零開機時間會令第一回合立即超時。**
   `stc_game_start()` 把 phase timestamp 設為 0，但實際應用傳入的是系統
   uptime；只要 uptime 大於 prompt timeout，第一個 tick 就會逾時。
2. **沒有執行 180 秒總時限。**
   `total_time_ms`、`elapsed_ms`、`duration_ms` 與 `ended_by_time` 沒有在
   `stc_game.c` 更新。
3. **PROMPT 超時錯誤地結束整局。**
   規格要求該回合 0 分並進入下一回合；現碼直接進 `SUMMARY`。
4. **LEARNING 不可按 PLAY 跳過。**
   `stc_game_input()` 沒有處理 learning 階段的 PLAY。
5. **遊戲與 UI 的狀態語義不一致。**
   現 UI 只准 `result_ok → idle/summary`，不准遊戲實際需要的
   `result_ok → learning`；現 UI 的 `learning` 是分類小百科，不是「你掃到／
   你放咗／正確答案／本回合分數」的回合回饋。
6. **reject 畫面的資料不足。**
   UI `result_retry` 要求兩個不同且有效的候選；`stc_app.c` 只填第一個，
   第二個保持 0，而且 `stc_vision_scan()` 尚未提供 top-two 聚合。
7. **UI adapter 只做 phase mapping。**
   沒有真正的 controller loop、queue、stale-result 防護、相機／桶蓋／語音
   呼叫，也沒有 UI action 回到遊戲核心的完整路徑。
8. **ToF DROP 的所有權未落到實際鏈路。**
   遊戲接受 `STC_EV_DROP { bin, dist_mm }`，但現有 CDC 幀只有
   `NODE_STATE` 的四個距離，沒有 node-side DROP event。需明確在哪一端做
   邊緣判定，否則會重複或漏計。

## 4. 建議架構

保持 `stc_game` 為純狀態與計分模組，另加一個有界事件控制器：

```text
UI action ───────┐
PLAY / SET ──────┤
vision result ───┼─→ stc_game_controller ─→ stc_game
drop event ──────┤          │
lid link state ──┘          ├─→ stc_vision_scan
                            ├─→ stc_lid / transport
                            ├─→ stc_voice
                            └─→ stc_ui_apply
```

硬規則：

- `q_app` 深度 16、`q_vision` 深度 2、`q_lid` 深度 8；滿載時記 log 並丟新
  事件，不阻塞 scheduler，也不無限增長。
- 每個 scan 與 lid command 帶單調遞增 `round_id`／`seq`；舊結果不可套用到
  新回合。
- 控制器只把已接受的狀態交給 UI；UI callback 只回傳意圖。
- 相機 scan、桶蓋 command、語音播放都經抽象介面，host 測試用 replay／
  loopback，S31 glue 才碰硬件。
- 所有 tick 都使用同一個 monotonic millisecond clock。

## 5. 建議分階段實作

### G0 — 先凍結三個決策

需要 owner 拍板後才改契約：

1. 把現有 `STC_LEARNING` 定義為每回合作答回饋，`STC_CATEGORY_GUIDE` 保留為
   延伸小百科；learning payload 增加掃到類別、投放桶、對錯、本回合加減分。
2. ToF 的 DROP 判定由 S3 node 發出專用事件，還是由 S31 對
   `NODE_STATE` 做邊緣判定。
3. 整機先用哪個 classifier：目前 `mixed_s31.espdl` 仍是 main 上的既有產物；
   `wordless-padded/wordless_s31.espdl` 是較新的未見卡候選，但尚未上 S31。
   兩者不可互相代替，必須由 owner 選定並在啟動時核對 hash／labels。

未拍板前可以補純 bug 與測試，但不要用 `summary` 假裝 learning，不要用
第一候選重複填第二候選來繞過 UI validation。

### G1 — 修正遊戲核心語義

建議改動：

- `contracts/stc_game.h`：加入可在啟動時傳入 `now_ms` 的 API，或定義首次
  tick 的合法初始化方式；不動已拍板的計分常數。
- `firmware/common/stc_game.c`：修正 PROMPT timeout、總時限、learning skip、
  elapsed／duration／ended_by_time、snapshot 的 lid／voice／abstain 欄位。
- `firmware/common/test/test_game.c`：保留現有 case，另加 12 個規格 case。

驗收：

- 使用非零起點（例如 `now=86400000`）仍不會立即 timeout。
- PROMPT timeout 只記一回合，有限局繼續。
- 第 3 回合中到 180 秒時完成該回合才 summary，`ended_by_time=true`。
- LEARNING 中 PLAY 只跳過當前 learning，不越過結果／結算。
- 1000 個隨機局保持 phase 合法、round 單調、`elapsed_ms` 單調、分數界限。
- `test_game`、`test_e2e`、`test_mode_gate` 全部通過。

### G2 — 加遊戲控制器與 UI adapter

建議新增：

- `app/stc_game_controller.h/.c`：接 PLAY、UI action、vision、drop、timeout。
- `app/stc_ui_adapter.h/.c`：`stc_snapshot_t` → payload；top-two → retry；
  每回合 learning → 回合回饋 payload。
- `firmware/common/test/test_game_controller.c`、`test_ui_transition.c`：
  以假 UI、假 vision、假 lid 跑完整一局。

驗收：

- controller 接受 `START` 後才開新局，重複 `START` 只建立一次。
- 回合中 HOME／GUIDE／ASK 不偷切畫面；非法 action 回明確 error。
- 舊 round 的 label／drop 進來時丟棄並記 `stale_event`。
- 一局 5 回合在 host 上自動走完，每回合只改分一次。

### G3 — 接真視覺掃描

建議改動：

- 實作 `stc_vision_scan()`：開相機、5 幀 × 50 ms、平均 softmax、argmax、
  `conf < 0.55` abstain、相機必定關閉。
- 以 `stc_frame_source_t` 先跑 replay，再接 S31 SC101IOT frame source。
- 載入 G0 選定的 `.espdl`，不把候選路徑硬編成已批准模型；啟動時核對
  labels、model hash、preprocess contract。
- `stc_burst_detail_t` 完整填 `frame_bin[5]`、`frame_conf[5]`、agreement、
  各階段耗時，並由 detail 算 top-two candidates。

驗收：

- host replay 可穩定輸出四類與拒判；少於 3 個有效幀回 `STC_ERR_IO`。
- 同一真實 UYVY golden frame 的 target／host tensor 差與類別差有記錄。
- 一局相機 open count 等於 scan count；WAIT_DROP 時相機已關。
- S31 上模型 p50／p95、RAM／PSRAM、開相機與整次 scan 分開量測。
- 未上板前不得把 host 重播結果寫成現場準確率。

### G4 — 完成每回合學習頁

建議 UI payload 增加一個同步複製的回合回饋結構：

```c
typedef struct {
    stc_bin_t scanned;
    stc_bin_t dropped;       /* 無投放 = STC_BIN_COUNT */
    bool      has_drop;
    bool      correct;
    bool      abstained;
    bool      timed_out;
    int16_t   round_delta;
    const char *tip;         /* UI 同步排版，不持有 ownership */
} stc_ui_learning_t;
```

畫面顯示「你掃到、你放咗、正確答案、本回合、累計、小知識」，不顯示模型
無法知道的具體物件名。`category_guide` 只作延伸百科。

驗收：

- 正確、錯誤、超時、兩次拒判各有一張 800×480 framebuffer。
- 第 5 回合 learning 後只出現一次 summary。
- `TEXT_OVERFLOW`、glyph、contrast、touch 檢查全部通過。
- 逐張親眼看過截圖；native PASS 不代表實屏 PASS。

### G5 — 接桶蓋與投放事件

先沿用 CDC loopback／transport injection 完成 host 路徑，再決定 ESP-NOW：

- `LID_CMD` 發出後 300 ms 無同 seq ACK → 重試 2 次；仍失敗進 fault UI。
- node offline 500 ms 不再接受新 lid command。
- DROP event 只計第一次；同一 round 的重送以 `round_id + seq` 去重。
- 四蓋全開失敗時不偽造成功；玩家仍可手動投放，但 UI 顯示 fault 提示。

ESP-NOW 目前是獨立 spike，不是已拍板的 CDC 替代。只有在 S31↔S3 實測通過
距離、掉包、重連、供電與 Wi-Fi 共存後，才另開決策比較 CDC 和 ESP-NOW。

### G6 — 整機與證據

至少保存：

```text
run_id,round,phase,scan_count,camera_open_count,
t_open_ms,t_capture_ms,t_pre_ms,t_infer_ms,
truth,confidence,agreement,bin,dropped,
score_delta,score_raw,score_shown,
lid_seq,lid_ack_ms,lid_retry,node_online,
fault_mask
```

驗收數字分開報：

- 模型準確率：哪個 model hash、哪個資料集、幾張、是否獨立。
- S31 模型延遲 p50／p95。
- PLAY 到首次開蓋 p50／p95。
- 5 幀掃描 p50／p95。
- 五回合總時長、超時率、拒判率、DROP 漏檢／誤檢。
- 連續 10 局的 fault 與重啟次數。

## 6. 測試矩陣

| 層 | 必要測試 | 失敗即阻擋 |
|---|---|---|
| Game core | 12 個規格 case、1000 隨機局、非零時鐘 | 是 |
| Mode | 回合中 0 次進 CHAT；CHAT 中 0 次開局；60 秒退出 | 是 |
| Vision | golden、5 幀、<3 幀、abstain、top-two | 是 |
| UI | 真 `stc_ui_apply` 全 phase、四種 learning、非法 stale payload | 是 |
| Lid | CRC、ACK、300 ms 重試、500 ms offline、duplicate DROP | 是 |
| Transport | CDC loopback；ESP-NOW 另列 spike 結果 | CDC 是；ESP-NOW 比較前是 |
| Hardware | 相機、實屏、四蓋、ToF、供電、整局 | 只可由實機持有人簽 |

## 7. 建議執行次序

1. 先由 owner 回答第 5 節 G0 的三個決策。
2. 同日完成 G1 的純 core bug 與測試，不動 UI 美術。
3. 接着做 G2 controller 與真 UI transition 測試。
4. 視覺與桶蓋可以並行，但兩者都先走 replay／loopback。
5. 最後才做 S31 hardware glue、ESP-NOW 比較與整機量測。

不要把「UI 已通過」當「遊戲已完成」，也不要把「模型檔案存在」當「相機到
開蓋已接通」。這個計劃的成功標準是一條可重播、可量測、失敗可解釋的流程。

## 8. 模型與 UI 基線摘要

### 視覺

- 現行技術路線是 YOLO11n-cls 四類分類，不是物件偵測；目前沒有類別以外的
  邊界框可供自動裁切。
- `models/wordless-padded/wordless_s31.espdl` 是 w8a8、目標 `esp32s31`、
  1,812,848 bytes；與音源測試集 163 張同源場景為 155/163。
- 未見卡 5 折卡片身分驗證為 `0.8251 ± 0.0632`；這是開發候選的較佳證據，
  但不是印刷卡或課室現場準確率。
- 舊 S31 實板模型平均推論約 83.8 ms；這不是新 padded 模型、也不是端到端
  延遲。新模型在 S31 的延遲仍 `未測`。
- 早前 SC101IOT 4 張現場 smoke 只有 1/4 對，17 張 field-probe baseline
  6/17 對；所以不能只靠調高 confidence 自動開蓋。
- 建議遊戲示範固定白底、位置、距離與光源，並保留拒判／人工確認；同時以
  自己的實拍建立 locked field set。

### UI

- 800×480、rotation 0、RGB565、LVGL 9.5；九狀態與四類圖片已實作。
- native render evaluation 為 PASS：17 張 framebuffer、75 個文字對比、
  13 個觸控目標、50 個 native scenarios。
- UI 只負責顯示與發出 action intent，不能改分數、直接開蓋或自行切模式。
- 目前缺口是「每回合作答回饋」與 transition contract，不是重畫整個視覺。

## 9. 參考

- [GitHub issue #31：遊戲邏輯](https://github.com/Bruiseunderskin/smart-trash-can/issues/31)
- [GitHub issue #53：整機整合](https://github.com/Bruiseunderskin/smart-trash-can/issues/53)
- [GitHub PR #61：遊戲核心與整合實作](https://github.com/Bruiseunderskin/smart-trash-can/pull/61)
- [遊戲邏輯規格](game-logic-spec.md)
- [UI 規格與預覽](../ui/lvgl/README.md)
- [模型實驗](../models/wordless-padded/README.md)

[文件索引](README.md) · [專案入口](../README.md)
