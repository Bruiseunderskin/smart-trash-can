// ESP32-S31-Korvo-1 bring-up 共用定義
//
// 所有 GPIO 來自 xiaozhi-esp32/main/boards/espressif/esp32-s31-korvo-1/config.h。
// ⚠️ 排針實際引出腳位必須對官方原理圖覆核（見 issue #11）。

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------------- 音頻（ES8389 + 2× NS4150B） ----------------
#define BRINGUP_I2S_MCLK 2
#define BRINGUP_I2S_BCLK 3
#define BRINGUP_I2S_WS 4
#define BRINGUP_I2S_DOUT 5   // codec → ESP（播放）
#define BRINGUP_I2S_DIN 6    // ESP → codec（錄音）
#define BRINGUP_CODEC_SDA 0
#define BRINGUP_CODEC_SCL 1
#define BRINGUP_PA_GPIO 7    // 功放使能，唔拉高就冇聲
#define BRINGUP_SAMPLE_RATE 16000

// ---------------- LCD（ST7262E43，RGB565 800×480） ----------------
#define BRINGUP_LCD_H_RES 800
#define BRINGUP_LCD_V_RES 480
#define BRINGUP_LCD_VSYNC 45
#define BRINGUP_LCD_HSYNC 44
#define BRINGUP_LCD_DE 43
#define BRINGUP_LCD_PCLK 40
#define BRINGUP_LCD_DISP 38
// DATA0..15
#define BRINGUP_LCD_DATA0 8
#define BRINGUP_LCD_DATA1 9
#define BRINGUP_LCD_DATA2 10
#define BRINGUP_LCD_DATA3 11
#define BRINGUP_LCD_DATA4 12
#define BRINGUP_LCD_DATA5 13
#define BRINGUP_LCD_DATA6 14
#define BRINGUP_LCD_DATA7 15
#define BRINGUP_LCD_DATA8 16
#define BRINGUP_LCD_DATA9 17
#define BRINGUP_LCD_DATA10 18
#define BRINGUP_LCD_DATA11 19
#define BRINGUP_LCD_DATA12 33
#define BRINGUP_LCD_DATA13 34
#define BRINGUP_LCD_DATA14 35
#define BRINGUP_LCD_DATA15 36

// ---------------- Camera（OV3660，DVP） ----------------
#define BRINGUP_CAM_XCLK 55
#define BRINGUP_CAM_PCLK 54
#define BRINGUP_CAM_VSYNC 56
#define BRINGUP_CAM_HREF 57
#define BRINGUP_CAM_D0 46
#define BRINGUP_CAM_D1 47
#define BRINGUP_CAM_D2 48
#define BRINGUP_CAM_D3 49
#define BRINGUP_CAM_D4 50
#define BRINGUP_CAM_D5 51
#define BRINGUP_CAM_D6 52
#define BRINGUP_CAM_D7 53

// ---------------- 其他 ----------------
#define BRINGUP_LED_GPIO 37
#define BRINGUP_BOOT_GPIO 61

/** 統一的 PASS / FAIL 輸出，方便 grep。 */
#define BRINGUP_REPORT(test, ok, fmt, ...)                                     \
  do {                                                                         \
    if (ok) {                                                                  \
      printf("RESULT %s PASS " fmt "\n", test, ##__VA_ARGS__);                 \
    } else {                                                                   \
      printf("RESULT %s FAIL " fmt "\n", test, ##__VA_ARGS__);                 \
    }                                                                          \
  } while (0)

/** 依賴未完成時用這個，避免報成 FAIL 誤導。 */
#define BRINGUP_BLOCKED(test, why) printf("RESULT %s BLOCKED %s\n", test, why)

/**
 * 初始化 ES8389（I2C + I2S 時鐘 + 播放/錄音通路）。
 *
 * TODO(codec): 這個函數目前只拉高 PA 腳並回傳 ESP_ERR_NOT_SUPPORTED。
 * 要實作，參考（同一個 repo 內）：
 *   xiaozhi-esp32/main/audio/codecs/es8389_audio_codec.cc
 * 需要的動作：I2C master init（SDA 0 / SCL 1）→ 寫 ES8389 暫存器設定
 * 取樣率、位深、ADC/DAC 使能、輸入增益 → 拉高 PA（GPIO7）。
 */
esp_err_t bringup_codec_init(void);

/** I2S 標準模式 TX 初始化（16 kHz、16-bit）。 */
esp_err_t bringup_i2s_tx_init(void **out_tx_handle);

/** I2S 標準模式 RX 初始化（16 kHz、16-bit、mono）。 */
esp_err_t bringup_i2s_rx_init(void **out_rx_handle);

/** 產生正弦波（16-bit mono），寫入 buf（samples 個取樣）。 */
void bringup_sine(int16_t *buf, size_t samples, float freq_hz, float amp);

/** 回傳 RMS 與 peak（對 int16 樣本）。 */
void bringup_rms_peak(const int16_t *buf, size_t samples, float *rms, int32_t *peak);

/**
 * 把 PCM 寫成 WAV 檔（SD 卡內）。
 * TODO(sd): 需先 mount SD（SDMMC 4-bit 或 SPI）；mount 的腳位要對原理圖確認。
 */
esp_err_t bringup_write_wav(const char *path, const int16_t *pcm, size_t samples,
                            uint32_t sample_rate);

#ifdef __cplusplus
}
#endif
