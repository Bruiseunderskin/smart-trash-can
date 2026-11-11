# ESP-NOW：ESP32-S31 ↔ ESP32-S3 可行性與限制

查證日期：2026-11-11

基準：ESP-IDF v6.1 官方文檔與 `espressif/esp-idf` `v6.1` source

範圍：傳輸可行性，不含距離、電流、吞吐、射頻現場表現或舵機安全驗證

## 結論

ESP32-S31 與 ESP32-S3 都有 v6.1 ESP-NOW 文檔與 Wi-Fi 實作，現有 16-byte
`stc_lidmsg` 遠低於 v1.0 的 250-byte 上限，因此可作 S31 controller → S3
node 的實驗鏈路。實作必須：

1. 兩端先啟動 Wi-Fi，再初始化 ESP-NOW。
2. 兩端處於同一 Wi-Fi channel；peer 的 channel 必須等於本機目前 channel。
3. 在 callback 中只複製資料到 queue，由較低優先次序 task 處理。
4. 不把 `ESP_NOW_SEND_SUCCESS` 當作應用層已收到。
5. 用 ACK、sequence 與重送處理應用層可靠性。
6. 將 MAC 發現和加密方案分開設計；現有 broadcast discovery 不能依賴加密
   multicast。

這證明「可以做傳輸 spike」，不證明「ESP-NOW 應取代已拍板的 USB CDC」。

## 官方能力與版本

Espressif 的 ESP32-S31 v6.1 ESP-NOW 文檔存在：

- <https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s31/api-reference/network/esp_now.html>

ESP32-S3 的同一版本文檔：

- <https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/network/esp_now.html>

v6.1 Wi-Fi component `Kconfig` 包含 `esp32s31` 目錄，ESP-NOW API header 亦在
`components/esp_wifi/include/esp_now.h`：

- <https://github.com/espressif/esp-idf/tree/v6.1/components/esp_wifi/esp32s31>
- <https://github.com/espressif/esp-idf/blob/v6.1/components/esp_wifi/include/esp_now.h>

v6.1 header 定義：

- ESP-NOW v1.0 單次資料上限 `ESP_NOW_MAX_DATA_LEN = 250` bytes。
- ESP-NOW v2.0 上限 `ESP_NOW_MAX_DATA_LEN_V2 = 1470` bytes。
- 現有 lid frame 是 16 bytes，屬兩者皆可承載的小 frame。

## Wi-Fi 生命週期與 channel

官方 ESP-NOW 文檔明確要求先啟動 Wi-Fi，再初始化 ESP-NOW：

- <https://raw.githubusercontent.com/espressif/esp-idf/v6.1/docs/en/api-reference/network/esp_now.rst>

本 spike 的次序是 `esp_wifi_init → esp_wifi_start →
esp_wifi_set_channel → esp_now_init`，與文檔一致。

`esp_now_peer_info_t.channel` 的 source 註解要求該值等於 station/AP 當前的
channel；channel 為 0 時才使用目前 channel。channel 不一致可令
`esp_now_send()` 回 `ESP_ERR_ESPNOW_CHAN` 或在空中遺失。來源：

- <https://github.com/espressif/esp-idf/blob/v6.1/components/esp_wifi/include/esp_now.h>

Wi-Fi station 連接 AP 後，`WIFI_EVENT_STA_CONNECTED` 事件直接提供 AP 的
channel；因此正式 GAME/CHAT 共存時，不能永遠假設固定 channel 6：

- <https://github.com/espressif/esp-idf/blob/v6.1/components/esp_wifi/include/esp_wifi_types_generic.h>

本 spike 不連接 AP，只設固定 channel 6。若 S31 之後接 router，應在連線
事件後讀取 AP channel，令 S3 peer 使用同一 channel；否則應在 CHAT 模式暫停
ESP-NOW，或使用能切 channel 的受控流程。v6.1 有
`esp_now_switch_channel_tx()`，但它只處理一次傳送，不能用來持續接收另一
channel 的 heartbeat：

- <https://github.com/espressif/esp-idf/blob/v6.1/components/esp_wifi/include/esp_now.h>

## callback 與可靠性

官方文檔說明：

- send／receive callback 在 Wi-Fi task 執行，不可做長時間工作；應 post 到
  queue。
- 太短的連續 send 可能令 callback 次序混亂；建議上一筆 callback 返回後再
  送下一筆。
- `ESP_NOW_SEND_SUCCESS` 只代表 MAC 層收到；不保證 application layer 收到。
- 需要自行回 ACK、做 timeout／retransmit，並用 sequence 去重。

來源同為 v6.1 ESP-NOW 文檔。spike 因此採單一 in-flight frame、固定
`LINK_EVENT` queue、每 250 ms 一個 node heartbeat，以及 controller 對每個
command 等待 `LID_ACK`。這只是 minimal spike；300 ms × 2 重試仍應沿用正式
`stc_lid_link` 的測試定義。

## Broadcast、Mac discovery 與加密

官方文檔明確指出：

- 傳送 broadcast 前必須先加入 broadcast MAC peer。
- 接收端沒有 paired peer 時，仍可接收 broadcast 與未加密 unicast；要收
  加密 unicast，則必須有 peer 與相同 LMK。
- encrypted multicast vendor-specific action frame 不支援。
- PMK 用 AES-128 加密每部 peer 的 LMK；未設 LMK 的 peer frame 不加密。

來源：

- <https://raw.githubusercontent.com/espressif/esp-idf/v6.1/docs/en/api-reference/network/esp_now.rst>

本 spike 以 S3 broadcast `NODE_STATE` 讓 S31 學到 node MAC，之後 command
用 unicast。兩端都設 `encrypt=false`，所以只能作短距離、受控實驗。正式版
若要安全：

1. 先在 provisioning 時交換／儲存 peer MAC 與 secret。
2. 對 unicast peer 設同一 PMK／LMK 並啟用 encryption。
3. 不以 broadcast heartbeat 傳送需要保密的內容。
4. 對重放、sequence 回繞、舊 round event 與未知 MAC 做應用層防護。

不能只靠 CRC。CRC 可擋傳輸錯誤，不提供身分認證。

## 功耗與供電界線

本查證沒有取得可套用到目前機箱的 S31/S3 峰值電流數字，因此不提供估算。
可以確定的是：

- ESP-NOW 使用 Wi-Fi activity frame；原本 USB CDC node 的
  `CONFIG_ESP_WIFI_ENABLED=n` 必須改成 Wi-Fi 開啟。
- 現有 repo 的 210 mA 常態／500 mA USB 上限是 USB CDC 設計值，不可當成
  Wi-Fi 鏈路的實測值。
- v6.1 Wi-Fi header 顯示 power save 預設為 `WIFI_PS_MIN_MODEM`；spike 改為
  `WIFI_PS_NONE` 以持續收發。這會改變功耗，必須實測。

source：

- <https://github.com/espressif/esp-idf/blob/v6.1/components/esp_wifi/include/esp_wifi.h>

上板時應量：

- 待機、beacon 發送、command ACK、舵機堵轉時的分支電流與電壓跌落。
- S31 同時連接 AP／語音資料流時，ESP-NOW ACK 成功率與延遲分布。
- S3 使用獨立穩壓供電與共同接地時的 100+ 次 command 成功率。

## 建議的比較順序

1. USB CDC 保持為現行基準，因為它已有協議、測試與電源預算決策。
2. 用本 spike 只驗 transport：發現、heartbeat、command、ACK、重連。
3. 量 S31↔S3 的 p50／p95 RTT、100 次成功率、掉電重連、AP channel 共存。
4. 只有在供電、可靠性和共存均通過後，才開 issue 比較 CDC 與 ESP-NOW。
5. 如果只為免除 USB 線而犧牲可重現的 300 ms ACK／500 ms offline 邏輯，
   不應切換。

## 本機驗證狀態

- 實作：`firmware/espnow_link_spike/`。
- CI：已加入 S31/S3 target build matrix。
- 本機本次：未完成 target build；可用 WSL 沒有現成 v6.1 IDF，
  container image 下載時間不可接受，故不能寫「已編譯」。
- 實板：未連接 S31 或 S3，因此未燒錄、未收發、未量 RTT／距離／功耗。

[Research 索引](README.md) · [文件索引](../README.md)
