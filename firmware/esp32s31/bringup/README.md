# ESP32-S31-Korvo-1 Bring-up

四個外設 + 一個迴路測試。**一次只跑一個**，跑完把 `RESULT` 行貼回 [issue #11](https://github.com/Bruiseunderskin/smart-trash-can/issues/11)。

編譯 CI 使用 ESP-IDF 6.1，透過 `tools/dev.py compile --project s31_bringup` 建置。[搬移後編譯紀錄](https://github.com/Bruiseunderskin/smart-trash-can/actions/runs/41489144893)已通過 bring-up 及 Korvo-1 板型編譯；編譯不代表外設已在實板通過。Codec、camera 和 SD 的未完成依賴如下。

## 跑法

```bash
idf.py --preview set-target esp32s31
idf.py menuconfig      # Bring-up → Test to run → 選 T1..T5
idf.py --preview -p <port> build flash monitor
```

每個測試都會印一行：

```
RESULT T1 PASS worst_fill_ms=23
RESULT T3 BLOCKED codec not initialised
```

`BLOCKED` 代表依賴未完成（目前是 ES8389 codec 與 camera），**不是**硬件壞。

## 五個測試與通過門檻

| 代號 | 測什麼 | 通過門檻 | 現狀 |
|---|---|---|---|
| T1 | LCD 800×480 | 五色正確、8 線均勻、填屏 ≤ 40 ms | 已寫，待實測 |
| T2 | Camera SC101IOT | 30 幀 ≥28 有效、JPEG 可開、記錄 fps | **未實作**（見 `t2_camera.c` 的 TODO） |
| T3 | Mic | 靜音 RMS < 200、講話 RMS ≥ 3×、WAV 可播 | 已寫，**卡在 codec** |
| T4 | Speaker | 三種音量可分辨、無爆音、可被回錄 | 已寫，出聲需 codec |
| T5 | Loopback | 回錄 peak > 500、Goertzel(440) ≥ 3× P(1000) | 已寫，**卡在 codec** |

## 兩個尚未接上的依賴

1. **ES8389 codec 初始化**（擋住 T3/T4/T5）
   `bringup_codec_init()` 目前只拉高 PA（GPIO7）。要補 I2C + ES8389 暫存器設定，
   參考同 repo 的 `firmware/xiaozhi-esp32/main/audio/codecs/es8389_audio_codec.cc`。

2. **Camera 初始化**（擋住 T2）
   S31 走 esp-video（V4L2），`t2_camera.c` 內已列出要接的 5 個步驟。
   參考 `firmware/xiaozhi-esp32/main/boards/common/esp_video.cc`。

3. **SD 卡 mount**（影響 T3 的 WAV 輸出）
   `bringup_write_wav()` 只做 `fopen("/sdcard/...")`，前提是 SD 已 mount。
   SDMMC 4-bit 的腳位要對原理圖確認。

## 為什麼 T4 唔自己判 PASS

喇叭有冇聲，程式自己聽唔到。所以 T4 只報「已送出」，真正的判斷在 **T5**：
播 440 Hz 的同時用 mic 回錄，再用 Goertzel 量 440 Hz 的能量是否明顯高於 1 kHz。
這是唯一不靠人耳的客觀驗證。

## 實測結果（填真數字，唔准寫「大約」）

| 測試 | 結果 | 關鍵數字 | 日期 |
|---|---|---|---|
| T1 LCD | | worst_fill_ms = | |
| T2 Camera | | fps = ，KB/幀 = | |
| T3 Mic | | 靜音 RMS = ，講話 RMS = | |
| T4 Speaker | | 三段音量可分辨：是/否 | |
| T5 Loopback | | peak = ，P440/P1000 = | |

## 過關之後

全部 PASS → 進入 `docs/ROADMAP.md` 的 M2（模型）與 M3（整合）。
任何一項 FAIL 且無法在 1 日內修好 → 觸發 M1 叫停規則：手部改用 Pico / ESP32，S31 只做眼＋腦。
