/*
 * SPDX-FileCopyrightText: 2026 Smart Trash Can contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "audio_test_common.h"
#include "bsp/esp32_s31_korvo.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RECORD_SECONDS 3
#define RECORD_SAMPLES (AUDIO_TEST_SAMPLE_RATE * RECORD_SECONDS)
#define READ_CHUNK_SAMPLES 1024
#define MIC_GAIN_DB 30.0f

static const char *TAG = "T3_MIC";

static esp_err_t record_seconds(esp_codec_dev_handle_t mic, int16_t *samples, size_t count)
{
    size_t offset = 0;
    const int64_t start_us = esp_timer_get_time();
    while (offset < count) {
        const size_t request = count - offset > READ_CHUNK_SAMPLES ?
                               READ_CHUNK_SAMPLES : count - offset;
        const int ret = esp_codec_dev_read(mic, samples + offset,
                                           request * sizeof(int16_t));
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Microphone read failed: %d", ret);
            return ESP_FAIL;
        }
        offset += request;
    }
    const double elapsed_s = (esp_timer_get_time() - start_us) / 1000000.0;
    ESP_LOGI(TAG, "Capture complete: %zu samples in %.3f s", count, elapsed_s);
    return ESP_OK;
}

void app_main(void)
{
    const char *mount_point = bsp_sdcard_get_mount_point();
    bsp_sdcard_config_t sd_config = BSP_SDCARD_DEFAULT_CONFIG();
    esp_err_t ret = bsp_sdcard_mount(&sd_config, NULL);
    const bool sd_ready = ret == ESP_OK;
    ESP_LOGI(TAG, "SD mount=%s path=%s", sd_ready ? "OK" : esp_err_to_name(ret), mount_point);

    ret = bsp_audio_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "T3_AUTO_CHECK=FAIL reason=bsp_audio_init error=%s", esp_err_to_name(ret));
        return;
    }

    esp_codec_dev_handle_t mic = bsp_audio_codec_microphone_init();
    if (!mic) {
        ESP_LOGE(TAG, "T3_AUTO_CHECK=FAIL reason=microphone_init");
        return;
    }

    esp_codec_dev_sample_info_t sample_info = {
        .sample_rate = AUDIO_TEST_SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
    };
    int codec_ret = esp_codec_dev_open(mic, &sample_info);
    if (codec_ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "T3_AUTO_CHECK=FAIL reason=mic_open error=%d", codec_ret);
        return;
    }
    codec_ret = esp_codec_dev_set_in_gain(mic, MIC_GAIN_DB);
    if (codec_ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "T3_AUTO_CHECK=FAIL reason=mic_gain error=%d", codec_ret);
        esp_codec_dev_close(mic);
        return;
    }

    int16_t *samples = heap_caps_malloc(RECORD_SAMPLES * sizeof(int16_t),
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!samples) {
        ESP_LOGE(TAG, "T3_AUTO_CHECK=FAIL reason=no_buffer");
        esp_codec_dev_close(mic);
        return;
    }

    ESP_LOGI(TAG, "T3_PHASE=QUIET action=stay_silent duration_s=%d", RECORD_SECONDS);
    vTaskDelay(pdMS_TO_TICKS(1500));
    ret = record_seconds(mic, samples, RECORD_SAMPLES);
    audio_test_stats_t quiet = { 0 };
    if (ret == ESP_OK) {
        audio_test_stats(samples, RECORD_SAMPLES, &quiet);
        ESP_LOGI(TAG, "T3_QUIET rms=%.2f peak=%" PRIu32 " clipped=%" PRIu32,
                 quiet.rms, quiet.peak, quiet.clipped_samples);
        if (sd_ready) {
            audio_test_write_wav("/sdcard/mic_quiet_3s.wav", samples, RECORD_SAMPLES,
                                 AUDIO_TEST_SAMPLE_RATE, 1);
        }
    }

    ESP_LOGI(TAG, "T3_PHASE=SPEECH action=speak_now duration_s=%d", RECORD_SECONDS);
    vTaskDelay(pdMS_TO_TICKS(1500));
    ret = record_seconds(mic, samples, RECORD_SAMPLES);
    audio_test_stats_t speech = { 0 };
    if (ret == ESP_OK) {
        audio_test_stats(samples, RECORD_SAMPLES, &speech);
        ESP_LOGI(TAG, "T3_SPEECH rms=%.2f peak=%" PRIu32 " clipped=%" PRIu32,
                 speech.rms, speech.peak, speech.clipped_samples);
        if (sd_ready) {
            ret = audio_test_write_wav("/sdcard/mic_speech_3s.wav", samples, RECORD_SAMPLES,
                                       AUDIO_TEST_SAMPLE_RATE, 1);
        }
    }

    const bool quiet_threshold = quiet.rms > 0 && quiet.rms < 200.0;
    const bool speech_threshold = quiet.rms > 0 && speech.rms >= quiet.rms * 3.0;
    const bool clipping_threshold = speech.clipped_samples < RECORD_SAMPLES / 100;
    const bool wav_threshold = sd_ready && ret == ESP_OK;
    ESP_LOGI(TAG,
             "T3_AUTO_CHECK=%s quiet=%s speech_ratio=%.2f clipping=%s wav=%s",
             quiet_threshold && speech_threshold && clipping_threshold && wav_threshold ?
             "PASS" : "FAIL",
             quiet_threshold ? "pass" : "fail",
             quiet.rms > 0 ? speech.rms / quiet.rms : 0,
             clipping_threshold ? "pass" : "fail",
             wav_threshold ? "pass" : "fail");

    heap_caps_free(samples);
    esp_codec_dev_close(mic);
    if (sd_ready) {
        bsp_sdcard_unmount();
    }
}
