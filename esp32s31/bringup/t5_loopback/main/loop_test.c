/*
 * SPDX-FileCopyrightText: 2026 Smart Trash Can contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "audio_test_common.h"
#include "bsp/esp32_s31_korvo.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"

#define CHUNK_FRAMES        320
#define CHUNK_BYTES         (CHUNK_FRAMES * sizeof(int16_t))
#define DELAY_MS            200
#define DELAY_CHUNKS        (DELAY_MS / 20)
#define RUN_SECONDS         10
#define TOTAL_CHUNKS        (RUN_SECONDS * 50)
#define OUTPUT_VOLUME       30

static const char *TAG = "T5_LOOPBACK";

void app_main(void)
{
    const char *mount_point = bsp_sdcard_get_mount_point();
    bsp_sdcard_config_t sd_config = BSP_SDCARD_DEFAULT_CONFIG();
    esp_err_t ret = bsp_sdcard_mount(&sd_config, NULL);
    const bool sd_ready = ret == ESP_OK;
    ESP_LOGI(TAG, "SD mount=%s path=%s", sd_ready ? "OK" : esp_err_to_name(ret), mount_point);

    ret = bsp_audio_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "T5_AUTO_CHECK=FAIL reason=bsp_audio_init error=%s", esp_err_to_name(ret));
        return;
    }

    esp_codec_dev_handle_t mic = bsp_audio_codec_microphone_init();
    esp_codec_dev_handle_t speaker = bsp_audio_codec_speaker_init();
    if (!mic || !speaker) {
        ESP_LOGE(TAG, "T5_AUTO_CHECK=FAIL reason=codec_init");
        return;
    }

    esp_codec_dev_sample_info_t mic_info = {
        .sample_rate = AUDIO_TEST_SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
    };
    esp_codec_dev_sample_info_t speaker_info = {
        .sample_rate = AUDIO_TEST_SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel = 2,
        .channel_mask = 0,
    };
    if (esp_codec_dev_open(mic, &mic_info) != ESP_CODEC_DEV_OK ||
            esp_codec_dev_open(speaker, &speaker_info) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "T5_AUTO_CHECK=FAIL reason=codec_open");
        return;
    }
    (void)esp_codec_dev_set_in_gain(mic, 25.0f);
    (void)esp_codec_dev_set_out_vol(speaker, OUTPUT_VOLUME);

    RingbufHandle_t ring = xRingbufferCreate(CHUNK_BYTES * (DELAY_CHUNKS + 2),
                                             RINGBUF_TYPE_NOSPLIT);
    int16_t *source = heap_caps_malloc(TOTAL_CHUNKS * CHUNK_BYTES,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *output = heap_caps_malloc(TOTAL_CHUNKS * CHUNK_BYTES,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *mic_chunk = heap_caps_malloc(CHUNK_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    int16_t *stereo_chunk = heap_caps_malloc(CHUNK_BYTES * 2,
                                              MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!ring || !source || !output || !mic_chunk || !stereo_chunk) {
        ESP_LOGE(TAG, "T5_AUTO_CHECK=FAIL reason=buffer_alloc");
        return;
    }

    int16_t silence[CHUNK_FRAMES] = { 0 };
    for (int i = 0; i < DELAY_CHUNKS; i++) {
        if (!xRingbufferSend(ring, silence, CHUNK_BYTES, 0)) {
            ESP_LOGE(TAG, "T5_AUTO_CHECK=FAIL reason=delay_prefill");
            return;
        }
    }

    uint32_t underruns = 0;
    uint32_t overruns = 0;
    uint32_t read_errors = 0;
    uint32_t write_errors = 0;
    size_t output_chunks = 0;

    ESP_LOGI(TAG,
             "T5_START delay_ms=%d duration_s=%d volume=%d action=speak_into_mic",
             DELAY_MS, RUN_SECONDS, OUTPUT_VOLUME);

    for (int chunk_index = 0; chunk_index < TOTAL_CHUNKS; chunk_index++) {
        if (esp_codec_dev_read(mic, mic_chunk, CHUNK_BYTES) != ESP_CODEC_DEV_OK) {
            read_errors++;
            memset(mic_chunk, 0, CHUNK_BYTES);
        }
        memcpy(source + (size_t)chunk_index * CHUNK_FRAMES,
               mic_chunk, CHUNK_BYTES);

        if (!xRingbufferSend(ring, mic_chunk, CHUNK_BYTES, 0)) {
            overruns++;
        }

        size_t item_size = 0;
        const int16_t *delayed = xRingbufferReceive(ring, &item_size, 0);
        if (!delayed || item_size != CHUNK_BYTES) {
            underruns++;
            memset(stereo_chunk, 0, CHUNK_BYTES * 2);
        } else {
            memcpy(output + (size_t)output_chunks * CHUNK_FRAMES,
                   delayed, CHUNK_BYTES);
            output_chunks++;
            for (int i = 0; i < CHUNK_FRAMES; i++) {
                stereo_chunk[i * 2] = delayed[i];
                stereo_chunk[i * 2 + 1] = delayed[i];
            }
            vRingbufferReturnItem(ring, (void *)delayed);
        }

        if (esp_codec_dev_write(speaker, stereo_chunk,
                                CHUNK_FRAMES * 2 * sizeof(int16_t)) != ESP_CODEC_DEV_OK) {
            write_errors++;
        }
    }

    audio_test_stats_t input_stats = { 0 };
    audio_test_stats(source, TOTAL_CHUNKS * CHUNK_FRAMES, &input_stats);

    bool wav_ok = false;
    if (sd_ready) {
        if (audio_test_write_wav("/sdcard/loopback_source.wav", source,
                                 TOTAL_CHUNKS * CHUNK_FRAMES,
                                 AUDIO_TEST_SAMPLE_RATE, 1) == ESP_OK) {
            wav_ok = audio_test_write_wav("/sdcard/loopback_output.wav", output,
                                          output_chunks * CHUNK_FRAMES,
                                          AUDIO_TEST_SAMPLE_RATE, 1) == ESP_OK;
        }
    }

    const bool buffers_ok = underruns == 0 && overruns == 0;
    const bool io_ok = read_errors == 0 && write_errors == 0;
    ESP_LOGI(TAG,
             "T5_STATS underruns=%" PRIu32 " overruns=%" PRIu32
             " read_errors=%" PRIu32 " write_errors=%" PRIu32
             " rms=%.2f peak=%" PRIu32,
             underruns, overruns, read_errors, write_errors,
             input_stats.rms, input_stats.peak);
    ESP_LOGI(TAG, "T5_AUTO_CHECK=%s buffers=%s io=%s wav=%s feedback=REQUIRES_EXTERNAL_LISTEN",
             buffers_ok && io_ok && wav_ok ? "PASS" : "FAIL",
             buffers_ok ? "pass" : "fail",
             io_ok ? "pass" : "fail",
             wav_ok ? "pass" : "fail");

    vRingbufferDelete(ring);
    heap_caps_free(source);
    heap_caps_free(output);
    heap_caps_free(mic_chunk);
    heap_caps_free(stereo_chunk);
    esp_codec_dev_close(mic);
    esp_codec_dev_close(speaker);
    if (sd_ready) {
        bsp_sdcard_unmount();
    }
}
