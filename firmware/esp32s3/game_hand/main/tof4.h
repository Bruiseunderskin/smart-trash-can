#pragma once

#include <stdint.h>

#include "esp_err.h"

#include "stc_tof_drop.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_TOF4_SENSOR_COUNT STC_TOF_SENSOR_COUNT
#define STC_TOF4_BASELINE_SAMPLES 30

esp_err_t stc_tof4_init(void);
esp_err_t stc_tof4_start_continuous(void);
esp_err_t stc_tof4_capture_baseline(
    stc_tof_baseline_t baseline[STC_TOF4_SENSOR_COUNT],
    uint32_t sample_timeout_ms);
esp_err_t stc_tof4_read_round(
    stc_tof_sample_t samples[STC_TOF4_SENSOR_COUNT],
    uint32_t timeout_ms, uint8_t *missing_mask);
int stc_tof4_ready_count(void);
int stc_tof4_gpio(int sensor_index);

#ifdef __cplusplus
}
#endif
