# ESP-NOW S31 <-> S3 link spike

讀者：要驗證 ESP32-S31 與 ESP32-S3 無線鏈路的人。何時讀：在接伺服機、
TOF 或正式遊戲控制器之前。

這個獨立實驗只證明傳輸：S3 定時廣播 `NODE_STATE`，S31 發現 S3 後每 3 秒
發一次 `LID_CMD`，S3 回 `LID_ACK`。它沒有驅動相機、螢幕、伺服機或 TOF，
也不是 USB CDC 已拍板方案的替代品。

## 協定與限制

- 直接重用 `firmware/common/stc_lidmsg.h` 的固定 16 bytes 幀與
  CRC-16/CCITT-FALSE，不另立第二套應用協定。
- 兩端必須在同一固定 Wi-Fi 主信道；預設信道 6，可在 menuconfig 修改。
- 此版本不連接路由器。正式使用時，如果 S31 同時連接 Wi-Fi AP，信道會受
  AP 控制；信道不一致時 ESP-NOW 會失聯。
- 此版本不使用 ESP-NOW 加密，只適合受控實驗。啟用 v2.0 加密前必須先量
  本板性能；不要憑文檔上限推論實測吞吐。
- ESP-NOW 的傳送 callback 只表示 MAC 層結果，不保證應用層收到；所以
  `LID_ACK` 仍然是必要條件。
- Wi-Fi 開啟後 S3 的耗電和原 USB CDC 拓撲不同。正式裝機應分開供電並
  共同接地；本 repo 沒有實測這條無線拓撲的電流、距離或延遲。

## 建置

需要 ESP-IDF **6.1 preview**（S31 target）及兩個獨立 build directory。
從 repository root 執行：

```sh
. "$IDF_PATH/export.sh"

idf.py --preview -C firmware/espnow_link_spike \
  -B build/idf/espnow_s31 \
  -D IDF_TARGET=esp32s31 \
  -D SDKCONFIG="$PWD/build/idf/espnow_s31/sdkconfig" \
  -D SDKCONFIG_DEFAULTS="$PWD/firmware/espnow_link_spike/sdkconfig.defaults" \
  build

idf.py -C firmware/espnow_link_spike \
  -B build/idf/espnow_s3 \
  -D IDF_TARGET=esp32s3 \
  -D SDKCONFIG="$PWD/build/idf/espnow_s3/sdkconfig" \
  -D SDKCONFIG_DEFAULTS="$PWD/firmware/espnow_link_spike/sdkconfig.defaults;$PWD/firmware/espnow_link_spike/sdkconfig.defaults.node" \
  build
```

燒錄時沿用同一個 `-B`、`-D IDF_TARGET=...` 和 `-D SDKCONFIG=...`，再加上
`-p <PORT> flash monitor`。不要讓兩個 target 共用 `sdkconfig`。

## 上板判準

兩端 UART 應出現：

```text
role=controller ... channel=6 encryption=off
role=node ... channel=6 encryption=off
node discovered: <MAC>
LINK UP
sent LID_CMD seq=0 bin=0
received LID_CMD seq=0 bin=0 action=1
sent LID_ACK seq=0
ACK seq=0 bin=0 rtt_us=<measured>
```

一次實驗至少記錄：

1. 連續 100 個 command，`LID_ACK` 成功數與失敗數。
2. `rtt_us` 的 p50／p95，不以估算代替。
3. 斷電 S3 後 `LINK DOWN` 出現時間，以及 S3 重開後重新發現時間。
4. 修改 S31 或 AP 的 Wi-Fi 信道後，鏈路是否符合預期的斷線行為。
5. S3 開 Wi-Fi 的峰值電流與供電方式；沒有量測就寫 `未測`。

在以上證據出現前，只能寫「ESP-NOW spike 已實作／待上板」，不能寫
「ESP-NOW 已驗證」或「可以取代 USB CDC」。

[上層韌體入口](../README.md) · [專案入口](../../README.md)
