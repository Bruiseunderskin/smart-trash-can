#include "stc_game_dev_flow.h"

#include <string.h>

void stc_game_dev_flow_init(stc_game_dev_flow_t *flow)
{
    if (flow) {
        memset(flow, 0, sizeof *flow);
    }
}

stc_dev_flow_action_t stc_game_dev_flow_poll(
    stc_game_dev_flow_t *flow,
    const stc_snapshot_t *snapshot,
    bool hand_online,
    uint32_t now_ms)
{
    if (!flow || !snapshot) {
        return STC_DEV_FLOW_NONE;
    }
    if (snapshot->phase != STC_PH_LEARNING) {
        flow->learning_active = false;
    }
    if (!hand_online) {
        return STC_DEV_FLOW_NONE;
    }

    switch (snapshot->phase) {
    case STC_PH_IDLE:
        return STC_DEV_FLOW_START;
    case STC_PH_PROMPT:
        return STC_DEV_FLOW_SCAN;
    case STC_PH_LEARNING:
        if (!flow->learning_active ||
            flow->learning_round_id != snapshot->round_id) {
            flow->learning_active = true;
            flow->learning_round_id = snapshot->round_id;
            flow->learning_since_ms = now_ms;
            return STC_DEV_FLOW_NONE;
        }
        if ((uint32_t)(now_ms - flow->learning_since_ms) >=
            STC_DEV_LEARNING_DWELL_MS) {
            return STC_DEV_FLOW_NEXT;
        }
        return STC_DEV_FLOW_NONE;
    default:
        return STC_DEV_FLOW_NONE;
    }
}
