#include "bringup_common.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "bringup";

esp_err_t bringup_codec_init(void) {
  // PA 使能：唔拉高，喇叭永遠冇聲（最常見的「以為板壞」）
  gpio_config_t pa = {
      .pin_bit_mask = 1ULL << BRINGUP_PA_GPIO,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&pa));
  gpio_set_level(BRINGUP_PA_GPIO, 1);

  // TODO(codec): ES8389 尚未初始化。未做之前 I2S 有資料但 codec 不出聲。
  // 參考 xiaozhi-esp32/main/audio/codecs/es8389_audio_codec.cc
  ESP_LOGW(TAG, "ES8389 not initialised (TODO): PA(GPIO%d) raised only", BRINGUP_PA_GPIO);
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bringup_i2s_tx_init(void **out_tx_handle) {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_chan_handle_t tx = NULL;
  esp_err_t err = i2s_new_channel(&chan_cfg, &tx, NULL);
  if (err != ESP_OK) return err;

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BRINGUP_SAMPLE_RATE),
      .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,  // 這塊板不用 MCLK
          .bclk = BRINGUP_I2S_BCLK,
          .ws = BRINGUP_I2S_WS,
          .dout = BRINGUP_I2S_DOUT,
          .din = I2S_GPIO_UNUSED,
          .invert_flags = {false, false, false},
      },
  };
  err = i2s_channel_init_std_mode(tx, &std_cfg);
  if (err != ESP_OK) return err;
  err = i2s_channel_enable(tx);
  if (err != ESP_OK) return err;
  *out_tx_handle = (void *)tx;
  return ESP_OK;
}

esp_err_t bringup_i2s_rx_init(void **out_rx_handle) {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_chan_handle_t rx = NULL;
  esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &rx);
  if (err != ESP_OK) return err;

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BRINGUP_SAMPLE_RATE),
      .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,
          .bclk = BRINGUP_I2S_BCLK,
          .ws = BRINGUP_I2S_WS,
          .dout = I2S_GPIO_UNUSED,
          .din = BRINGUP_I2S_DIN,
          .invert_flags = {false, false, false},
      },
  };
  err = i2s_channel_init_std_mode(rx, &std_cfg);
  if (err != ESP_OK) return err;
  err = i2s_channel_enable(rx);
  if (err != ESP_OK) return err;
  *out_rx_handle = (void *)rx;
  return ESP_OK;
}

void bringup_sine(int16_t *buf, size_t samples, float freq_hz, float amp) {
  const float w = 2.0f * (float)M_PI * freq_hz / (float)BRINGUP_SAMPLE_RATE;
  for (size_t i = 0; i < samples; ++i) {
    buf[i] = (int16_t)(amp * 32767.0f * sinf(w * (float)i));
  }
}

void bringup_rms_peak(const int16_t *buf, size_t samples, float *rms, int32_t *peak) {
  double acc = 0.0;
  int32_t pk = 0;
  for (size_t i = 0; i < samples; ++i) {
    const int32_t v = buf[i];
    acc += (double)v * (double)v;
    if (abs(v) > pk) pk = abs(v);
  }
  *rms = samples ? (float)sqrt(acc / (double)samples) : 0.0f;
  *peak = pk;
}

esp_err_t bringup_write_wav(const char *path, const int16_t *pcm, size_t samples,
                            uint32_t sample_rate) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    ESP_LOGE(TAG, "fopen(%s) failed — SD 未 mount？", path);
    return ESP_ERR_NOT_FOUND;
  }
  const uint32_t data_bytes = (uint32_t)(samples * sizeof(int16_t));
  const uint32_t byte_rate = sample_rate * 2;  // mono, 16-bit
  const uint16_t block_align = 2;
  const uint32_t chunk = 36 + data_bytes;
  const uint32_t fmt_len = 16;
  const uint16_t pcm_fmt = 1;
  const uint16_t channels = 1;
  const uint16_t bits = 16;

  fwrite("RIFF", 1, 4, f);
  fwrite(&chunk, 4, 1, f);
  fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&fmt_len, 4, 1, f);
  fwrite(&pcm_fmt, 2, 1, f);
  fwrite(&channels, 2, 1, f);
  fwrite(&sample_rate, 4, 1, f);
  fwrite(&byte_rate, 4, 1, f);
  fwrite(&block_align, 2, 1, f);
  fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f);
  fwrite(&data_bytes, 4, 1, f);
  fwrite(pcm, 1, data_bytes, f);
  fclose(f);
  ESP_LOGI(TAG, "wav written: %s (%u bytes)", path, (unsigned)data_bytes);
  return ESP_OK;
}
