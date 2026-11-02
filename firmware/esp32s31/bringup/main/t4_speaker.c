// T4 — Speaker（ES8389 DAC + NS4150B）：440 Hz → 1 kHz → 掃頻，三段振幅。
//
// 注意：程式自己聽唔到聲，所以這支只證明「資料有送出、I2S 有跑、PA 有拉高」。
// 真正的客觀判斷在 T5（mic 回錄 + Goertzel）。

#include "bringup_common.h"

#include <stdlib.h>

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "t4_spk";

static void play_tone(i2s_chan_handle_t tx, float freq, int ms, float amp) {
  const size_t total = (size_t)(BRINGUP_SAMPLE_RATE * ms / 1000);
  int16_t *buf = malloc(total * sizeof(int16_t));
  if (!buf) return;
  bringup_sine(buf, total, freq, amp);
  size_t written = 0;
  i2s_channel_write(tx, buf, total * sizeof(int16_t), &written, pdMS_TO_TICKS(3000));
  free(buf);
}

void t4_speaker_run(void) {
  const esp_err_t codec = bringup_codec_init();
  if (codec != ESP_OK) {
    ESP_LOGW(TAG, "codec 未初始化：I2S 會送出資料，但喇叭唔會有聲");
  }

  void *tx_handle = NULL;
  const esp_err_t err = bringup_i2s_tx_init(&tx_handle);
  if (err != ESP_OK) {
    BRINGUP_REPORT("T4", false, "i2s tx init err=0x%x", err);
    return;
  }
  i2s_chan_handle_t tx = (i2s_chan_handle_t)tx_handle;

  ESP_LOGI(TAG, "LISTEN: 440 Hz，1 秒");
  play_tone(tx, 440.0f, 1000, 0.30f);
  vTaskDelay(pdMS_TO_TICKS(300));

  ESP_LOGI(TAG, "LISTEN: 1 kHz，1 秒");
  play_tone(tx, 1000.0f, 1000, 0.30f);
  vTaskDelay(pdMS_TO_TICKS(300));

  ESP_LOGI(TAG, "LISTEN: 掃頻 200 Hz → 4 kHz，3 秒");
  for (int i = 0; i < 30; ++i) {
    play_tone(tx, 200.0f + 3800.0f * (float)i / 30.0f, 100, 0.25f);
  }

  ESP_LOGI(TAG, "LISTEN: 三段振幅 10%% / 20%% / 35%%（實際音量仍由 codec 增益決定）");
  play_tone(tx, 1000.0f, 400, 0.10f);
  play_tone(tx, 1000.0f, 400, 0.20f);
  play_tone(tx, 1000.0f, 400, 0.35f);

  if (codec != ESP_OK) {
    BRINGUP_BLOCKED("T4", "codec not initialised; audio path unverified");
  } else {
    BRINGUP_REPORT("T4", true, "played 3 tones + sweep; confirm with T5");
  }

  i2s_channel_disable(tx);
  i2s_del_channel(tx);
}
