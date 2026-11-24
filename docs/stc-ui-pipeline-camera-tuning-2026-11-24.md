# S31 UI、離線配網與相機預覽調校

讀者：負責 S31 螢幕、綠豆模式閘或相機預覽的開發者。
何時讀：修改 CHAT 字幕版面、離線／配網流程，或調整相機 FPS 前。

## 1. CHAT 字幕

「問綠豆」頁不再在 speech card 內重複顯示「綠豆」角色行；角色身份只由
上方「綠豆話」標題和綠豆貼圖表達。答案獨佔 scroll viewport，長答案不會
遮住上方標題。字幕 chunks 現在直接串接，不再逐段插入換行；同一狀態的
串流更新只改 label 文字，不重建整個 CHAT 畫面。

所有 CHAT 狀態共用同一 speech card：card 由 y=190 開始，答案 viewport
由 y=228 開始，並按 runtime font line-height 取完整行數。按鈕位於 y=408。
用戶講「拜拜」、「再見」、
`bye` 或 `goodbye` 後，若回覆正在播放，會等播放完成；否則最多等 6 秒，
再顯示約 1.5 秒「再傾」畫面，自動返回 GAME 主頁。

遊戲畫面亦共用綠豆 anchor（x=688、y=112、64×64）。`確認卡片／完整入框`
和 `正在辨識` 共用同一 title box；`5 秒後自動掃描` 跟 title 同一水平中心。
除主頁外，長按綠豆會走同一安全返回流程；CHAT 綠豆長按亦會返回 GAME。

回歸畫面：

- `tools/lvglpro_preview/baselines/ask_answering_long.png`
- `tools/lvglpro_preview/baselines/ask_connecting.png`
- `tools/lvglpro_preview/baselines/ask_provisioning.png`

結算頁固定提供兩個出口：「再玩一次」和「返回主頁」。

## 2. 投放提示

`STC_LINE_OPEN`（請將卡片放入任何一個桶）不再於 `STC_PH_OPEN` 發出。
提示延後至桶蓋開啟完成並進入 `STC_PH_WAIT_DROP` 時播放，每個 round 只播一次。

## 3. 離線轉配網

STC 產品層把 WiFi connect timeout 設為 8 秒；沒有已存 SSID 時即時進配網，
不再等待上游預設的 60 秒。CHAT_STARTING 期間 UI 立即顯示「正在連線」；
config AP 真正啟動後顯示「要連線」和 AP SSID／`http://192.168.4.1`。

離開 CHAT 時，config AP 的 `ConfigModeExit` 不得再觸發自動重連。S31 board
只在 network gate 仍為 CHAT 時允許重連，否則保持 WiFi 關閉並恢復 ESP-NOW。

## 4. S3 閒置

ESP-NOW hand protocol 升至 v4，新增 `PAUSE`／`RESUME`。進入 CHAT 前 S31
先要求 S3 關蓋、停止 ToF continuous mode 和 PWM，之後 S3 只保留 ESP-NOW
RX，不再送 250 ms heartbeat。返回 GAME 後 S31 先恢復 radio，再送 `RESUME`；
S3 收到後才重新 heartbeat 和接受回合工作。

`ledc_stop()` 會停用 PWM channel；S3 servo driver 因此記錄 stopped 狀態，
下一次 OPEN／CLOSE 前自動重建 timer 和四條 channel。否則 CHAT→GAME 後
ESP-NOW ACK 會成功，但舵機完全冇 pulse，形成「程式話開蓋、實物唔動」。

## 5. 相機預覽

SC101IOT 的 640×480 UYVY profile 實機回報 `frame interval=1/15 fps=15`。
預覽源由 160×160 提升至 240×240，模型 input 維持 160×160。

實機量測（COM5，連續三個 5 秒窗口）：

| 版本 | Preview | FPS | Avg work | Avg wait | Avg convert |
|---|---:|---:|---:|---:|---:|
| float，4-sample/pixel | 320×320 | 3.05–3.13 | 319 ms | 未分開量 | 未分開量 |
| float box filter | 240×240 | 10.24–10.45 | 95–97 ms | 17–19 ms | 71 ms |
| fixed-point box filter | 240×240 | **15.26–15.62** | **63 ms** | **5–7 ms** | **52 ms** |

固定點版本三個窗口均 `failures=0`，冇 task watchdog。預覽已追上鏡頭
15 FPS 上限。

不切 1280×720 25 FPS：感測器有這個 profile，但 DVP 頻寬及前處理成本高幾倍，
而模型仍是 160×160，對分類沒有直接收益。若將來要 25 FPS，應先改用 PPA
硬體色彩轉換／縮放，再獨立量測。
