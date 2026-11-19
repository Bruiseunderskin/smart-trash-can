/* stc_game_dev_flow.h — dev-only five-round game autopilot policy. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stc_game.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_DEV_LEARNING_DWELL_MS 1800u

typedef enum {
    STC_DEV_FLOW_NONE = 0,
    STC_DEV_FLOW_START,
    STC_DEV_FLOW_SCAN,
    STC_DEV_FLOW_NEXT,
} stc_dev_flow_action_t;

typedef struct {
    bool learning_active;
    uint32_t learning_round_id;
    uint32_t learning_since_ms;
} stc_game_dev_flow_t;

void stc_game_dev_flow_init(stc_game_dev_flow_t *flow);
stc_dev_flow_action_t stc_game_dev_flow_poll(
    stc_game_dev_flow_t *flow,
    const stc_snapshot_t *snapshot,
    bool hand_online,
    uint32_t now_ms);

#ifdef __cplusplus
}
#endif
