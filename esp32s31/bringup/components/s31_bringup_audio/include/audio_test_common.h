/*
 * SPDX-FileCopyrightText: 2026 Smart Trash Can contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_TEST_SAMPLE_RATE 16000

typedef struct {
    double rms;
    uint32_t peak;
    uint32_t clipped_samples;
    size_t sample_count;
} audio_test_stats_t;

esp_err_t audio_test_stats(const int16_t *samples, size_t sample_count, audio_test_stats_t *stats);

esp_err_t audio_test_write_wav(const char *path, const int16_t *samples, size_t sample_count,
                               uint32_t sample_rate, uint16_t channels);

double audio_test_goertzel_db(const int16_t *samples, size_t sample_count,
                              uint32_t sample_rate, double target_hz);

static inline int16_t audio_test_clamp_i16(int32_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

#ifdef __cplusplus
}
#endif
