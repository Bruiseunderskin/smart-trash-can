/*
 * SPDX-FileCopyrightText: 2026 Smart Trash Can contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "audio_test_common.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "esp_log.h"

static const char *TAG = "AUDIO_TEST";

static void put_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xff);
    dst[1] = (uint8_t)(value >> 8);
}

static void put_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xff);
    dst[1] = (uint8_t)((value >> 8) & 0xff);
    dst[2] = (uint8_t)((value >> 16) & 0xff);
    dst[3] = (uint8_t)((value >> 24) & 0xff);
}

esp_err_t audio_test_stats(const int16_t *samples, size_t sample_count, audio_test_stats_t *stats)
{
    if (!samples || !stats || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t square_sum = 0;
    uint32_t peak = 0;
    uint32_t clipped = 0;
    for (size_t i = 0; i < sample_count; i++) {
        const int32_t value = samples[i];
        const uint32_t magnitude = value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
        square_sum += (uint64_t)((int64_t)value * value);
        if (magnitude > peak) {
            peak = magnitude;
        }
        if (magnitude >= 32760) {
            clipped++;
        }
    }

    stats->rms = sqrt((double)square_sum / sample_count);
    stats->peak = peak;
    stats->clipped_samples = clipped;
    stats->sample_count = sample_count;
    return ESP_OK;
}

esp_err_t audio_test_write_wav(const char *path, const int16_t *samples, size_t sample_count,
                               uint32_t sample_rate, uint16_t channels)
{
    if (!path || !samples || sample_count == 0 || channels == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint32_t data_size = (uint32_t)(sample_count * sizeof(int16_t));
    const uint32_t byte_rate = sample_rate * channels * sizeof(int16_t);
    uint8_t header[44] = { 0 };
    memcpy(header + 0, "RIFF", 4);
    put_u32_le(header + 4, 36 + data_size);
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    put_u32_le(header + 16, 16);
    put_u16_le(header + 20, 1);
    put_u16_le(header + 22, channels);
    put_u32_le(header + 24, sample_rate);
    put_u32_le(header + 28, byte_rate);
    put_u16_le(header + 32, channels * sizeof(int16_t));
    put_u16_le(header + 34, 16);
    memcpy(header + 36, "data", 4);
    put_u32_le(header + 40, data_size);

    FILE *file = fopen(path, "wb");
    if (!file) {
        ESP_LOGE(TAG, "Open %s failed: errno=%d", path, errno);
        return ESP_FAIL;
    }

    bool ok = fwrite(header, 1, sizeof(header), file) == sizeof(header);
    if (ok) {
        ok = fwrite(samples, sizeof(int16_t), sample_count, file) == sample_count;
    }
    if (ok) {
        ok = fflush(file) == 0;
    }
    if (ok) {
        ok = fsync(fileno(file)) == 0;
    }
    if (fclose(file) != 0) {
        ok = false;
    }

    if (!ok) {
        ESP_LOGE(TAG, "Write WAV %s failed", path);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "WAV saved: %s samples=%zu rate=%" PRIu32 " channels=%u",
             path, sample_count, sample_rate, channels);
    return ESP_OK;
}

double audio_test_goertzel_db(const int16_t *samples, size_t sample_count,
                              uint32_t sample_rate, double target_hz)
{
    if (!samples || sample_count < 2 || sample_rate == 0 ||
            target_hz <= 0 || target_hz >= sample_rate / 2.0) {
        return -INFINITY;
    }

    const double omega = 2.0 * M_PI * target_hz / sample_rate;
    const double coeff = 2.0 * cos(omega);
    double q0 = 0;
    double q1 = 0;
    double q2 = 0;
    for (size_t i = 0; i < sample_count; i++) {
        q0 = coeff * q1 - q2 + samples[i];
        q2 = q1;
        q1 = q0;
    }

    const double power = q1 * q1 + q2 * q2 - coeff * q1 * q2;
    return 10.0 * log10(power / ((double)sample_count * sample_count) + 1e-12);
}
