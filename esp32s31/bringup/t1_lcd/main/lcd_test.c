/*
 * SPDX-FileCopyrightText: 2026 Smart Trash Can contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "bsp/esp32_s31_korvo.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define COLOR_HOLD_MS 2000
#define MAX_FILL_US   40000

static const char *TAG = "T1_LCD";

typedef struct {
    const char *name;
    lv_color_t color;
} fill_case_t;

static uint32_t fill_screen(lv_display_t *display, lv_obj_t *screen, lv_color_t color)
{
    const int64_t start_us = esp_timer_get_time();
    lv_obj_set_style_bg_color(screen, color, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_invalidate(screen);
    lv_refr_now(display);
    return (uint32_t)(esp_timer_get_time() - start_us);
}

static void create_lines_and_text(lv_obj_t *screen)
{
    static const uint32_t line_colors[] = {
        0xFF4D4D, 0xFFA726, 0xFFEB3B, 0x66BB6A,
        0x26C6DA, 0x42A5F5, 0xAB47BC, 0xEF5350,
    };

    for (int i = 0; i < 8; i++) {
        lv_obj_t *line = lv_obj_create(screen);
        lv_obj_set_pos(line, 52 + i * 96, 150);
        lv_obj_set_size(line, 8, 250);
        lv_obj_set_style_bg_color(line, lv_color_hex(line_colors[i]), 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(line, 0, 0);
        lv_obj_set_style_radius(line, 4, 0);
        lv_obj_set_style_pad_all(line, 0, 0);
    }

    lv_obj_t *title = lv_label_create(screen);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_style_text_font(title, &lv_font_source_han_sans_sc_16_cjk, 0);
    lv_label_set_text(title, "LCD 屏幕测试 / SCREEN TEST");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *details = lv_label_create(screen);
    lv_obj_set_style_text_color(details, lv_color_hex(0x283593), 0);
    lv_obj_set_style_text_font(details, &lv_font_source_han_sans_sc_16_cjk, 0);
    lv_label_set_text(details, "800 x 480 RGB565 | 8 vertical lines | 中英文字");
    lv_obj_align(details, LV_ALIGN_BOTTOM_MID, 0, -54);
}

void app_main(void)
{
    bsp_display_config_t config = BSP_DISPLAY_DEFAULT_CONFIG();
    config.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL;
    config.buffer_height = 160;
    config.enable_ppa_accel = true;
    config.enable_touch = false;

    lv_display_t *display = bsp_display_start_with_config(&config);
    if (!display) {
        ESP_LOGE(TAG, "T1_AUTO_CHECK=FAIL reason=display_init");
        return;
    }

    if (!bsp_display_lock(-1)) {
        ESP_LOGE(TAG, "T1_AUTO_CHECK=FAIL reason=display_lock");
        return;
    }

    lv_obj_t *screen = lv_disp_get_scr_act(display);
    lv_obj_clean(screen);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_radius(screen, 0, 0);
    bsp_display_unlock();

    static const fill_case_t colors[] = {
        { "red",   { .red = 0xFF, .green = 0x00, .blue = 0x00 } },
        { "green", { .red = 0x00, .green = 0xFF, .blue = 0x00 } },
        { "blue",  { .red = 0x00, .green = 0x00, .blue = 0xFF } },
        { "white", { .red = 0xFF, .green = 0xFF, .blue = 0xFF } },
        { "black", { .red = 0x00, .green = 0x00, .blue = 0x00 } },
    };

    uint32_t max_fill_us = 0;
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        if (!bsp_display_lock(-1)) {
            ESP_LOGE(TAG, "T1_AUTO_CHECK=FAIL reason=display_lock");
            return;
        }
        const uint32_t elapsed_us = fill_screen(display, screen, colors[i].color);
        bsp_display_unlock();

        if (elapsed_us > max_fill_us) {
            max_fill_us = elapsed_us;
        }
        ESP_LOGI(TAG, "T1_FILL color=%s time_us=%" PRIu32 " time_ms=%.2f",
                 colors[i].name, elapsed_us, elapsed_us / 1000.0);
        vTaskDelay(pdMS_TO_TICKS(COLOR_HOLD_MS));
    }

    if (!bsp_display_lock(-1)) {
        ESP_LOGE(TAG, "T1_AUTO_CHECK=FAIL reason=display_lock");
        return;
    }
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    create_lines_and_text(screen);
    lv_obj_invalidate(screen);
    lv_refr_now(display);
    bsp_display_unlock();

    const bool timing_pass = max_fill_us <= MAX_FILL_US;
    ESP_LOGI(TAG, "T1_MAX_FILL max_us=%" PRIu32 " threshold_us=%u",
             max_fill_us, MAX_FILL_US);
    ESP_LOGI(TAG, "T1_AUTO_CHECK=%s timing=%s visual=REQUIRES_EXTERNAL_PHOTO",
             timing_pass ? "PASS" : "FAIL", timing_pass ? "pass" : "fail");
}
