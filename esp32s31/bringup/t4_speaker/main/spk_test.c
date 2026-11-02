/*
 * SPDX-FileCopyrightText: 2026 Smart Trash Can contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "audio_test_common.h"
#include "bsp/esp32_s31_korvo.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define TEST_SAMPLE_RATE       48000
#define TEST_CHANNELS          2
#define CAPTURE_FRAMES         TEST_SAMPLE_RATE
#define PLAY_CHUNK_FRAMES      320
#define TONE_AMPLITUDE         6000.0
#define CAPTURE_DONE_BIT       BIT0
#define CAPTURE_TASK_STACK     4096
#define CAPTURE_TASK_PRIORITY  5

static const char *TAG = "T4_SPEAKER";

typedef struct {
    esp_codec_dev_handle_t mic;
    int16_t *capture;
    size_t capture_samples;
    EventGroupHandle_t complete;
    int error;
} capture_context_t;

typedef struct {
    double rms;
    uint32_t peak;
    uint32_t clipped;
    double target_db;
    double other_db;
} tone_result_t;

static void capture_task(void *arg)
{
    capture_context_t *context = (capture_context_t *)arg;
    size_t offset = 0;
    while (offset < context->capture_samples) {
        const size_t remain = context->capture_samples - offset;
        const size_t request = remain > PLAY_CHUNK_FRAMES * TEST_CHANNELS ?
                               PLAY_CHUNK_FRAMES * TEST_CHANNELS : remain;
        const int ret = esp_codec_dev_read(context->mic, context->capture + offset,
                                           request * sizeof(int16_t));
        if (ret != ESP_CODEC_DEV_OK) {
            context->error = ret;
            break;
        }
        offset += request;
    }
    xEventGroupSetBits(context->complete, CAPTURE_DONE_BIT);
    vTaskDelete(NULL);
}

static esp_err_t play_sine(esp_codec_dev_handle_t speaker, double frequency_hz,
                           double seconds)
{
    int16_t tx[PLAY_CHUNK_FRAMES * TEST_CHANNELS];
    const size_t total_frames = (size_t)(TEST_SAMPLE_RATE * seconds);
    size_t written_frames = 0;

    while (written_frames < total_frames) {
        const size_t frames = total_frames - written_frames > PLAY_CHUNK_FRAMES ?
                              PLAY_CHUNK_FRAMES : total_frames - written_frames;
        for (size_t i = 0; i < frames; i++) {
            const double angle = 2.0 * M_PI * frequency_hz *
                                 (double)(written_frames + i) / TEST_SAMPLE_RATE;
            const int16_t sample = (int16_t)(TONE_AMPLITUDE * sin(angle));
            tx[i * 2] = sample;
            tx[i * 2 + 1] = sample;
        }
        const int ret = esp_codec_dev_write(speaker, tx, frames * TEST_CHANNELS * sizeof(int16_t));
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Speaker write failed: %d", ret);
            return ESP_FAIL;
        }
        written_frames += frames;
    }
    return ESP_OK;
}

static esp_err_t play_sweep(esp_codec_dev_handle_t speaker, double start_hz,
                            double end_hz, double seconds)
{
    int16_t tx[PLAY_CHUNK_FRAMES * TEST_CHANNELS];
    const size_t total_frames = (size_t)(TEST_SAMPLE_RATE * seconds);
    size_t written_frames = 0;

    while (written_frames < total_frames) {
        const size_t frames = total_frames - written_frames > PLAY_CHUNK_FRAMES ?
                              PLAY_CHUNK_FRAMES : total_frames - written_frames;
        for (size_t i = 0; i < frames; i++) {
            const double t = (double)(written_frames + i) / TEST_SAMPLE_RATE;
            const double k = log(end_hz / start_hz) / seconds;
            const double angle = 2.0 * M_PI * start_hz * (exp(k * t) - 1.0) / k;
            const int16_t sample = (int16_t)(TONE_AMPLITUDE * sin(angle));
            tx[i * 2] = sample;
            tx[i * 2 + 1] = sample;
        }
        const int ret = esp_codec_dev_write(speaker, tx, frames * TEST_CHANNELS * sizeof(int16_t));
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Sweep write failed: %d", ret);
            return ESP_FAIL;
        }
        written_frames += frames;
    }
    return ESP_OK;
}

static esp_err_t set_volume(esp_codec_dev_handle_t speaker, int volume)
{
    const int ret = esp_codec_dev_set_out_vol(speaker, volume);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Set speaker volume %d failed: %d", volume, ret);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "T4_VOLUME percent=%d", volume);
    return ESP_OK;
}

static esp_err_t capture_tone(esp_codec_dev_handle_t speaker, esp_codec_dev_handle_t mic,
                              int volume, double frequency_hz, const char *wav_path,
                              tone_result_t *result)
{
    if (set_volume(speaker, volume) != ESP_OK) {
        return ESP_FAIL;
    }

    capture_context_t capture = {
        .mic = mic,
        .capture_samples = CAPTURE_FRAMES * TEST_CHANNELS,
        .complete = xEventGroupCreate(),
        .error = ESP_CODEC_DEV_OK,
    };
    if (!capture.complete) {
        return ESP_ERR_NO_MEM;
    }
    capture.capture = heap_caps_malloc(capture.capture_samples * sizeof(int16_t),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!capture.capture) {
        vEventGroupDelete(capture.complete);
        return ESP_ERR_NO_MEM;
    }

    TaskHandle_t task = NULL;
    if (xTaskCreate(capture_task, "spk_capture", CAPTURE_TASK_STACK, &capture,
                    CAPTURE_TASK_PRIORITY, &task) != pdPASS) {
        heap_caps_free(capture.capture);
        vEventGroupDelete(capture.complete);
        return ESP_ERR_NO_MEM;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    const esp_err_t play_ret = play_sine(speaker, frequency_hz, 1.0);
    const EventBits_t bits = xEventGroupWaitBits(capture.complete, CAPTURE_DONE_BIT,
                                                 pdFALSE, pdTRUE, pdMS_TO_TICKS(3000));
    if (!(bits & CAPTURE_DONE_BIT) || capture.error != ESP_CODEC_DEV_OK || play_ret != ESP_OK) {
        ESP_LOGE(TAG, "Capture/playback failed: task_done=%d codec_error=%d",
                 !!(bits & CAPTURE_DONE_BIT), capture.error);
        heap_caps_free(capture.capture);
        vEventGroupDelete(capture.complete);
        return ESP_FAIL;
    }

    int16_t *left = heap_caps_malloc(CAPTURE_FRAMES * sizeof(int16_t),
                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!left) {
        heap_caps_free(capture.capture);
        vEventGroupDelete(capture.complete);
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < CAPTURE_FRAMES; i++) {
        left[i] = capture.capture[i * TEST_CHANNELS];
    }

    audio_test_stats_t stats = { 0 };
    audio_test_stats(left, CAPTURE_FRAMES, &stats);
    result->rms = stats.rms;
    result->peak = stats.peak;
    result->clipped = stats.clipped_samples;
    result->target_db = audio_test_goertzel_db(left, CAPTURE_FRAMES,
                                                TEST_SAMPLE_RATE, frequency_hz);
    result->other_db = audio_test_goertzel_db(left, CAPTURE_FRAMES,
                                               TEST_SAMPLE_RATE,
                                               frequency_hz < 700 ? 1000 : 440);

    ESP_LOGI(TAG,
             "T4_CAPTURE freq_hz=%.0f volume=%d rms=%.2f peak=%" PRIu32
             " target_db=%.2f other_db=%.2f",
             frequency_hz, volume, result->rms, result->peak,
             result->target_db, result->other_db);

    if (wav_path) {
        audio_test_write_wav(wav_path, left, CAPTURE_FRAMES, TEST_SAMPLE_RATE, 1);
    }

    heap_caps_free(left);
    heap_caps_free(capture.capture);
    vEventGroupDelete(capture.complete);
    return ESP_OK;
}

void app_main(void)
{
    static const int volumes[] = { 30, 60, 100 };
    static const double frequencies[] = { 440.0, 1000.0 };
    tone_result_t results[2][3] = { 0 };
    bool capture_ok = true;

    bsp_sdcard_config_t sd_config = BSP_SDCARD_DEFAULT_CONFIG();
    esp_err_t ret = bsp_sdcard_mount(&sd_config, NULL);
    const bool sd_ready = ret == ESP_OK;
    ESP_LOGI(TAG, "SD mount=%s", sd_ready ? "OK" : esp_err_to_name(ret));

    ret = bsp_audio_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "T4_AUTO_CHECK=FAIL reason=bsp_audio_init error=%s", esp_err_to_name(ret));
        return;
    }

    esp_codec_dev_handle_t mic = bsp_audio_codec_microphone_init();
    esp_codec_dev_handle_t speaker = bsp_audio_codec_speaker_init();
    if (!mic || !speaker) {
        ESP_LOGE(TAG, "T4_AUTO_CHECK=FAIL reason=codec_init");
        return;
    }

    esp_codec_dev_sample_info_t sample_info = {
        .sample_rate = TEST_SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel = TEST_CHANNELS,
        .channel_mask = 0,
    };
    if (esp_codec_dev_open(mic, &sample_info) != ESP_CODEC_DEV_OK ||
            esp_codec_dev_open(speaker, &sample_info) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "T4_AUTO_CHECK=FAIL reason=codec_open");
        return;
    }
    (void)esp_codec_dev_set_in_gain(mic, 30.0f);

    for (size_t f = 0; f < 2; f++) {
        for (size_t v = 0; v < 3; v++) {
            char path[96] = { 0 };
            if (sd_ready) {
                snprintf(path, sizeof(path), "/sdcard/spk_%.0f_v%d.wav",
                         frequencies[f], volumes[v]);
            }
            ESP_LOGI(TAG, "T4_PHASE=tone freq_hz=%.0f volume=%d",
                     frequencies[f], volumes[v]);
            if (capture_tone(speaker, mic, volumes[v], frequencies[f],
                             sd_ready ? path : NULL, &results[f][v]) != ESP_OK) {
                capture_ok = false;
            }
        }
    }

    ESP_LOGI(TAG, "T4_PHASE=sweep start_hz=20 end_hz=16000 duration_s=3 volume=60");
    (void)set_volume(speaker, 60);
    const esp_err_t sweep_ret = play_sweep(speaker, 20.0, 16000.0, 3.0);
    ESP_LOGI(TAG, "T4_SWEEP finished=%s", sweep_ret == ESP_OK ? "yes" : "no");

    const double volume_step_db = 3.0;
    const bool volume_440_30_to_60 =
        results[0][1].target_db > results[0][0].target_db + volume_step_db;
    const bool volume_440_60_to_100 =
        results[0][2].target_db > results[0][1].target_db + volume_step_db;
    const bool volume_1000_30_to_60 =
        results[1][1].target_db > results[1][0].target_db + volume_step_db;
    const bool volume_1000_60_to_100 =
        results[1][2].target_db > results[1][1].target_db + volume_step_db;
    const bool volume_30_to_60 = volume_440_30_to_60 && volume_1000_30_to_60;
    const bool volume_60_to_100 = volume_440_60_to_100 && volume_1000_60_to_100;
    const bool target_440 = results[0][1].target_db > results[0][1].other_db + 6.0;
    const bool target_1000 = results[1][1].target_db > results[1][1].other_db + 6.0;
    const bool clipping_ok = results[0][0].clipped == 0 &&
                             results[0][1].clipped == 0 &&
                             results[0][2].clipped == 0;
    const bool auto_pass = capture_ok && sweep_ret == ESP_OK &&
                           volume_30_to_60 && volume_60_to_100 &&
                           target_440 && target_1000 && clipping_ok;

    ESP_LOGI(TAG,
             "T4_VOLUME_CHECK 30to60=%s 60to100=%s"
             " 440Hz_delta_db=%.2f,%.2f 1kHz_delta_db=%.2f,%.2f",
             volume_30_to_60 ? "pass" : "fail",
             volume_60_to_100 ? "pass" : "fail",
             results[0][1].target_db - results[0][0].target_db,
             results[0][2].target_db - results[0][1].target_db,
             results[1][1].target_db - results[1][0].target_db,
             results[1][2].target_db - results[1][1].target_db);
    ESP_LOGI(TAG, "T4_FREQUENCY_CHECK 440Hz=%s 1kHz=%s",
             target_440 ? "pass" : "fail",
             target_1000 ? "pass" : "fail");
    ESP_LOGI(TAG, "T4_AUTO_CHECK=%s audio_beauty=REQUIRES_EXTERNAL_LISTEN",
             auto_pass ? "PASS" : "FAIL");

    esp_codec_dev_close(mic);
    esp_codec_dev_close(speaker);
    if (sd_ready) {
        bsp_sdcard_unmount();
    }
}
