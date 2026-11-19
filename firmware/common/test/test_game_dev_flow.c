/* test_game_dev_flow.c — dev-only five-round autopilot policy. */
#include <stdio.h>
#include <string.h>

#include "stc_game_dev_flow.h"

static stc_snapshot_t snapshot(stc_phase_t phase, uint32_t round_id)
{
    stc_snapshot_t snap;
    memset(&snap, 0, sizeof snap);
    snap.phase = phase;
    snap.round_id = round_id;
    snap.rounds_total = 5;
    return snap;
}

int main(void)
{
    stc_game_dev_flow_t flow;
    stc_game_dev_flow_init(&flow);

    stc_snapshot_t snap = snapshot(STC_PH_IDLE, 1);
    if (stc_game_dev_flow_poll(&flow, &snap, false, 100) != STC_DEV_FLOW_NONE) {
        fprintf(stderr, "started while hand offline\n");
        return 1;
    }
    if (stc_game_dev_flow_poll(&flow, &snap, true, 100) != STC_DEV_FLOW_START) {
        fprintf(stderr, "did not start with hand online\n");
        return 1;
    }

    snap = snapshot(STC_PH_PROMPT, 2);
    if (stc_game_dev_flow_poll(&flow, &snap, true, 200) != STC_DEV_FLOW_SCAN) {
        fprintf(stderr, "did not scan prompt\n");
        return 1;
    }

    snap = snapshot(STC_PH_LEARNING, 3);
    if (stc_game_dev_flow_poll(&flow, &snap, true, 1000) != STC_DEV_FLOW_NONE) {
        fprintf(stderr, "advanced learning immediately\n");
        return 1;
    }
    if (stc_game_dev_flow_poll(
            &flow, &snap, true,
            1000u + STC_DEV_LEARNING_DWELL_MS - 1u) != STC_DEV_FLOW_NONE) {
        fprintf(stderr, "advanced learning before dwell elapsed\n");
        return 1;
    }
    if (stc_game_dev_flow_poll(
            &flow, &snap, true,
            1000u + STC_DEV_LEARNING_DWELL_MS) != STC_DEV_FLOW_NEXT) {
        fprintf(stderr, "did not advance learning after dwell\n");
        return 1;
    }

    snap = snapshot(STC_PH_SUMMARY, 4);
    if (stc_game_dev_flow_poll(&flow, &snap, true, 9999) != STC_DEV_FLOW_NONE) {
        fprintf(stderr, "acted on summary\n");
        return 1;
    }
    puts("test_game_dev_flow: gated, dwell and phase policy OK");
    return 0;
}
