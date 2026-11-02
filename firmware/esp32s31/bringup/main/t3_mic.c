// T3 — Mic（ES8389 ADC）：錄 3 秒 16 kHz mono → RMS/peak → 存 WAV。
//
// 通過門檻（issue #11）：
//   靜音 RMS < 200、講話時 RMS ≥ 3× 靜音、WAV 可播、無長期削頂

#include "bringup_common.h"

#include <stdlib.h>

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "t3_mic";
#define TOTAL_SAMPLES (BRINGUP_SAMPLE_RATE * 3)

void t3_mic_run(void) {
  const esp_err_t codec = bringup_codec_init();
  const bool codec_ok = (codec == ESP_OK);
  if (!codec_ok) {
    ESP_LOGW(TAG, "codec 未初始化：錄到的資料會是靜音/雜訊，結果標為 BLOCKED");
  }

  void *rx_handle = NULL;
  esp_err_t err = bringup_i2s_rx_init(&rx_handle);
  if (err != ESP_OK) {
    BRINGUP_REPORT("T3", false, "i2s rx init err=0x%x", err);
    return;
  }

  int16_t *buf = calloc(TOTAL_SAMPLES, sizeof(int16_t));
  if (!buf) {
    BRINGUP_REPORT("T3", false, "alloc failed");
    return;
  }

  size_t got = 0;
  while (got < (size_t)TOTAL_SAMPLES) {
    size_t n = 0;
    const size_t want = ((size_t)TOTAL_SAMPLES - got) * sizeof(int16_t);
    err = i2s_channel_read((i2s_chan_handle_t)rx_handle,
                           (uint8_t *)buf + got * sizeof(int16_t), want, &n,
                           pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "read err=0x%x", err);
      break;
    }
    got += n / sizeof(int16_t);
  }

  float rms = 0;
  int32_t peak = 0;
  bringup_rms_peak(buf, got, &rms, &peak);
  ESP_LOGI(TAG, "captured %u samples  rms=%.1f  peak=%ld", (unsigned)got, rms, (long)peak);

  const esp_err_t wav = bringup_write_wav("/sdcard/mic.wav", buf, got, BRINGUP_SAMPLE_RATE);
  if (wav != ESP_OK) {
    ESP_LOGW(TAG, "WAV 未寫出（SD 未 mount）");
  }

  if (!codec_ok) {
    BRINGUP_BLOCKED("T3", "codec not initialised");
  } else {
    const bool ok = (wav == ESP_OK) && (peak < 32000) && (rms > 30.0f);
    BRINGUP_REPORT("T3", ok, "rms=%.1f peak=%ld wav=%s", rms, (long)peak,
                   wav == ESP_OK ? "ok" : "missing");
  }

  free(buf);
  i2s_channel_disable((i2s_chan_handle_t)rx_handle);
  i2s_del_channel((i2s_chan_handle_t)rx_handle);
}
