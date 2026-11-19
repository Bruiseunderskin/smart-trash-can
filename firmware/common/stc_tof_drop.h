/* stc_tof_drop.h — per-sensor baseline drop detection for four TOF nodes. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_TOF_SENSOR_COUNT 4
#define STC_TOF_CONFIRM_SAMPLES 3
#define STC_TOF_HIGH_RETURN_PERCENT 150u
#define STC_TOF_LOW_RETURN_PERCENT 60u
#define STC_TOF_DISTANCE_DELTA_MM 8u

typedef struct {
    uint16_t range_mm;
    uint16_t return_rate;
} stc_tof_baseline_t;

typedef struct {
    uint16_t range_mm;
    uint16_t return_rate;
    uint8_t status;
} stc_tof_sample_t;

typedef struct {
    uint8_t sensor_index;
    uint8_t streak;
    uint16_t range_mm;
    uint16_t return_rate;
    uint16_t baseline_range_mm;
    uint16_t baseline_return_rate;
} stc_tof_drop_event_t;

typedef struct {
    stc_tof_baseline_t baseline[STC_TOF_SENSOR_COUNT];
    uint8_t candidate;
    uint8_t streak;
    bool detected;
} stc_tof_drop_detector_t;

stc_err_t stc_tof_drop_init(
    stc_tof_drop_detector_t *detector,
    const stc_tof_baseline_t baseline[STC_TOF_SENSOR_COUNT]);

/* Returns true exactly once when the first stable drop event is confirmed. */
bool stc_tof_drop_update(
    stc_tof_drop_detector_t *detector,
    const stc_tof_sample_t samples[STC_TOF_SENSOR_COUNT],
    stc_tof_drop_event_t *event);

#ifdef __cplusplus
}
#endif
