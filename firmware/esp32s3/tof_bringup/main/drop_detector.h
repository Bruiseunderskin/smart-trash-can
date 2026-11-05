#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_DROP_BIN_COUNT 4
#define STC_DROP_HISTORY_SIZE 10

typedef struct {
    uint16_t history_mm[STC_DROP_BIN_COUNT][STC_DROP_HISTORY_SIZE];
    uint8_t history_count[STC_DROP_BIN_COUNT];
    uint8_t history_index[STC_DROP_BIN_COUNT];
    uint16_t baseline_mm[STC_DROP_BIN_COUNT];
    uint32_t baseline_update_ms[STC_DROP_BIN_COUNT];
    uint8_t candidate_count[STC_DROP_BIN_COUNT];
    uint32_t candidate_start_ms[STC_DROP_BIN_COUNT];
    uint32_t last_near_ms[STC_DROP_BIN_COUNT];
    uint16_t candidate_min_mm[STC_DROP_BIN_COUNT];
    bool waiting_recovery[STC_DROP_BIN_COUNT];
    bool monitor_enabled;
    bool event_emitted;
    uint32_t monitor_start_ms;
} stc_drop_detector_t;

void stc_drop_detector_reset(stc_drop_detector_t *detector);
void stc_drop_detector_set_monitor(stc_drop_detector_t *detector, bool enabled,
                                   uint32_t now_ms);

/* Returns 1 for one confirmed drop, 0 for no event, or -1 for invalid input. */
int stc_drop_detector_feed(stc_drop_detector_t *detector,
                           const uint16_t distance_mm[STC_DROP_BIN_COUNT],
                           const bool valid[STC_DROP_BIN_COUNT], uint32_t now_ms,
                           stc_drop_t *event);

#ifdef __cplusplus
}
#endif
