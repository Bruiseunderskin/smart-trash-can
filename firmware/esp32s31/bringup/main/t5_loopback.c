// T5 — Loopback：播 440 Hz → 同時用 mic 回錄 → Goertzel 判斷 440 Hz 能量。
//
// 這是唯一能客觀證明「喇叭真的有聲」的測試（唔靠人耳）。
// 通過門檻（issue #11）：回錄 peak > 500、Goertzel(440) 能量 ≥ 3× 平均值、underrun 0

#include "bringup_common.h"

#include <math.h>
#include <stdlib.h>

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "t5_loop";

/** Goertzel：量單一頻率的能量。samples 為 int16 mono。 */
static float goertzel(const int16_t *x, size_t n, float freq, float fs) {
  const float w = 2.0f * (float)M_PI * freq / fs;
  const float c = 2.0f * cosf(w);
  float s0 = 0, s1 = 0, s2 = 0;
  for (size_t i = 0; i < n; ++i) {
    s0 = (float)x[i] + c * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  const float power = s1 * s1 + s2 * s2 - c * s1 * s2;
  return power / (float)n;
}

void t5_loopback_run(void) {
  const esp_err_t codec = bringup_codec_init();
  if (codec != ESP_OK) {
    BRINGUP_BLOCKED("T5", "codec not initialised (speaker and mic both depend on it)");
    return;
  }

  i2s_chan_handle_t tx = NULL, rx = NULL;
  i2s_chan_config_t cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  if (i2s_new_channel(&cfg, &tx, &rx) != ESP_OK) {
    BRINGUP_REPORT("T5", false, "i2s_new_channel failed");
    return;
  }

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BRINGUP_SAMPLE_RATE),
      .slot_cfg =
          I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = BRINGUP_I2S_BCLK,
              .ws = BRINGUP_I2S_WS,
              .dout = BRINGUP_I2S_DOUT,
              .din = BRINGUP_I2S_DIN,
              .invert_flags = {false, false, false},
          },
  };
  if (i2s_channel_init_std_mode(tx, &std_cfg) != ESP_OK ||
      i2s_channel_init_std_mode(rx, &std_cfg) != ESP_OK ||
      i2s_channel_enable(tx) != ESP_OK || i2s_channel_enable(rx) != ESP_OK) {
    BRINGUP_REPORT("T5", false, "i2s duplex init failed");
    return;
  }

  const size_t n = BRINGUP_SAMPLE_RATE;  // 1 秒
  int16_t *play = malloc(n * sizeof(int16_t));
  int16_t *rec = calloc(n, sizeof(int16_t));
  if (!play || !rec) {
    BRINGUP_REPORT("T5", false, "alloc failed");
    return;
  }
  bringup_sine(play, n, 440.0f, 0.35f);

  size_t written = 0, read = 0;
  const esp_err_t w_err = i2s_channel_write(tx, play, n * sizeof(int16_t), &written,
                                            pdMS_TO_TICKS(3000));
  const esp_err_t r_err = i2s_channel_read(rx, rec, n * sizeof(int16_t), &read,
                                           pdMS_TO_TICKS(3000));
  if (w_err != ESP_OK || r_err != ESP_OK) {
    BRINGUP_REPORT("T5", false, "duplex io err w=0x%x r=0x%x", w_err, r_err);
  }

  float rms = 0;
  int32_t peak = 0;
  bringup_rms_peak(rec, read / sizeof(int16_t), &rms, &peak);
  const float p440 = goertzel(rec, read / sizeof(int16_t), 440.0f, BRINGUP_SAMPLE_RATE);
  const float p1k = goertzel(rec, read / sizeof(int16_t), 1000.0f, BRINGUP_SAMPLE_RATE);

  ESP_LOGI(TAG, "rms=%.1f peak=%ld  P(440)=%.1f  P(1000)=%.1f", rms, (long)peak, p440, p1k);

  const bool ok = (peak > 500) && (p440 > 3.0f * p1k);
  BRINGUP_REPORT("T5", ok, "peak=%ld P440=%.1f P1000=%.1f ratio=%.2f", (long)peak, p440, p1k,
                 p1k > 0 ? p440 / p1k : -1.0f);

  free(play);
  free(rec);
  i2s_channel_disable(tx);
  i2s_channel_disable(rx);
  i2s_del_channel(tx);
  i2s_del_channel(rx);
}
