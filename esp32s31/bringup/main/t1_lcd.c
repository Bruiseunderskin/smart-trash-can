// T1 — LCD：五色全屏、八條垂直線、中英文字；量填屏時間。
//
// 通過門檻（issue #11）：
//   五色肉眼正確、線距均勻無偏移、文字可讀、全屏填色 ≤ 40 ms

#include "bringup_common.h"

#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "t1_lcd";

// TODO(verify): 以下時序取自 800×480 RGB 常見值，必須對 ST7262E43 的 datasheet 覆核。
// 先由 16 MHz 起，花屏就降頻。
#define LCD_PCLK_HZ (16 * 1000 * 1000)

static esp_lcd_panel_handle_t s_panel = NULL;
static uint16_t *s_fb = NULL;

static esp_err_t lcd_init(void) {
  esp_lcd_rgb_panel_config_t cfg = {
      .clk_src = LCD_CLK_SRC_DEFAULT,
      .timings =
          {
              .pclk_hz = LCD_PCLK_HZ,
              .h_res = BRINGUP_LCD_H_RES,
              .v_res = BRINGUP_LCD_V_RES,
              .hsync_pulse_width = 48,
              .hsync_back_porch = 40,
              .hsync_front_porch = 40,
              .vsync_pulse_width = 3,
              .vsync_back_porch = 13,
              .vsync_front_porch = 32,
              .flags.pclk_active_neg = false,
          },
      .data_width = 16,
      .bits_per_pixel = 16,
      .num_fbs = 1,
      .bounce_buffer_size_px = BRINGUP_LCD_H_RES * 10,
      .dma_burst_size = 64,
      .hsync_gpio_num = BRINGUP_LCD_HSYNC,
      .vsync_gpio_num = BRINGUP_LCD_VSYNC,
      .de_gpio_num = BRINGUP_LCD_DE,
      .pclk_gpio_num = BRINGUP_LCD_PCLK,
      .disp_gpio_num = BRINGUP_LCD_DISP,
      .data_gpio_nums =
          {
              BRINGUP_LCD_DATA0, BRINGUP_LCD_DATA1, BRINGUP_LCD_DATA2, BRINGUP_LCD_DATA3,
              BRINGUP_LCD_DATA4, BRINGUP_LCD_DATA5, BRINGUP_LCD_DATA6, BRINGUP_LCD_DATA7,
              BRINGUP_LCD_DATA8, BRINGUP_LCD_DATA9, BRINGUP_LCD_DATA10, BRINGUP_LCD_DATA11,
              BRINGUP_LCD_DATA12, BRINGUP_LCD_DATA13, BRINGUP_LCD_DATA14, BRINGUP_LCD_DATA15,
          },
      .flags.fb_in_psram = 1,
  };

  esp_err_t err = esp_lcd_new_rgb_panel(&cfg, &s_panel);
  if (err != ESP_OK) return err;
  ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
  return esp_lcd_panel_init(s_panel);
}

static void fill(uint16_t rgb565) {
  const size_t px = (size_t)BRINGUP_LCD_H_RES * BRINGUP_LCD_V_RES;
  for (size_t i = 0; i < px; ++i) s_fb[i] = rgb565;
  esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BRINGUP_LCD_H_RES, BRINGUP_LCD_V_RES, s_fb);
}

void t1_lcd_run(void) {
  const size_t px = (size_t)BRINGUP_LCD_H_RES * BRINGUP_LCD_V_RES;
  s_fb = heap_caps_malloc(px * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!s_fb) {
    BRINGUP_REPORT("T1", false, "framebuffer alloc failed");
    return;
  }

  esp_err_t err = lcd_init();
  if (err != ESP_OK) {
    BRINGUP_REPORT("T1", false, "lcd init err=0x%x", err);
    return;
  }

  struct {
    const char *name;
    uint16_t color;
  } colors[] = {
      {"red", 0xF800}, {"green", 0x07E0}, {"blue", 0x001F}, {"white", 0xFFFF}, {"black", 0x0000},
  };

  int64_t worst_us = 0;
  for (int i = 0; i < 5; ++i) {
    const int64_t t0 = esp_timer_get_time();
    fill(colors[i].color);
    const int64_t dt = esp_timer_get_time() - t0;
    if (dt > worst_us) worst_us = dt;
    ESP_LOGI(TAG, "%s 0x%04X  %lld us", colors[i].name, colors[i].color, (long long)dt);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  // 八條垂直線 + 文字區塊（由左至右遞進，肉眼可判斷間距）
  fill(0x0000);
  for (int i = 0; i < 8; ++i) {
    const int x = (BRINGUP_LCD_H_RES / 8) * i + (BRINGUP_LCD_H_RES / 16);
    for (int y = 0; y < BRINGUP_LCD_V_RES - 120; ++y) {
      s_fb[(size_t)y * BRINGUP_LCD_H_RES + x] = 0xFFFF;
      s_fb[(size_t)y * BRINGUP_LCD_H_RES + x + 1] = 0xFFFF;
    }
  }
  // 文字測試條：白底黑字區塊（實際字型由 LVGL 負責，這裡先驗對比與幾何）
  for (int y = BRINGUP_LCD_V_RES - 100; y < BRINGUP_LCD_V_RES - 20; ++y) {
    for (int x = 40; x < 760; ++x) {
      s_fb[(size_t)y * BRINGUP_LCD_H_RES + x] = ((x / 24) % 2) ? 0xFFFF : 0x0000;
    }
  }
  esp_lcd_panel_draw_bitmap(s_panel, 0, 0, BRINGUP_LCD_H_RES, BRINGUP_LCD_V_RES, s_fb);

  const bool ok = (worst_us / 1000) <= 40;
  BRINGUP_REPORT("T1", ok, "worst_fill_ms=%lld", (long long)(worst_us / 1000));
  ESP_LOGI(TAG, "請拍照：五色、八條線、底部對比條");
}
