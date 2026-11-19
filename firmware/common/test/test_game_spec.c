/* test_game_spec.c — stage-one game contract regression tests. */
#include <stdio.h>
#include <string.h>

#include "stc_game.h"

static stc_event_t event(stc_ev_type_t type, uint32_t round_id, uint32_t t_ms)
{
    stc_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type;
    ev.round_id = round_id;
    ev.t_ms = t_ms;
    return ev;
}

static int begin_scan(uint32_t now_ms)
{
    const stc_snapshot_t before = stc_game_snapshot();
    stc_event_t scan = event(STC_EV_SCAN_REQUEST, before.round_id, now_ms);
    if (stc_game_input(&scan) != STC_OK) {
        fprintf(stderr, "scan request rejected\n");
        return 1;
    }
    if (stc_game_snapshot().phase != STC_PH_SCAN) {
        fprintf(stderr, "scan request did not enter SCAN\n");
        return 1;
    }
    return 0;
}

static int submit_label(stc_bin_t bin, float conf, bool abstain, uint32_t now_ms)
{
    const stc_snapshot_t snap = stc_game_snapshot();
    stc_event_t label = event(STC_EV_LABEL, snap.round_id, now_ms);
    label.u.label.bin = bin;
    label.u.label.conf = conf;
    label.u.label.abstain = abstain;
    label.u.label.src = STC_SRC_STUB;
    if (stc_game_input(&label) != STC_OK) {
        fprintf(stderr, "label rejected phase=%d\n", (int)snap.phase);
        return 1;
    }
    return 0;
}

static int continue_round(uint32_t now_ms)
{
    const stc_snapshot_t snap = stc_game_snapshot();
    stc_event_t next = event(STC_EV_CONTINUE, snap.round_id, now_ms);
    return stc_game_input(&next) == STC_OK ? 0 : 1;
}

static int test_nonzero_start_and_continue(void)
{
    const uint32_t start = 86400000u;
    if (stc_game_init() != STC_OK) return 1;
    if (stc_game_start(start) != STC_OK) return 1;
    if (stc_game_snapshot().phase != STC_PH_PROMPT) return 1;

    stc_game_tick(start + 1);
    if (stc_game_snapshot().phase != STC_PH_PROMPT) {
        fprintf(stderr, "first round timed out immediately\n");
        return 1;
    }

    const stc_game_config_t cfg = stc_game_get_config();
    uint32_t now = start + 10;
    if (begin_scan(now)) return 1;
    if (submit_label(STC_BIN_FOOD, 0.9f, false, now)) return 1;
    now += cfg.result_ms;
    stc_game_tick(now);
    now += cfg.open_timeout_ms;
    stc_game_tick(now);
    if (stc_game_snapshot().phase != STC_PH_WAIT_DROP) return 1;

    const stc_snapshot_t snap = stc_game_snapshot();
    stc_event_t drop = event(STC_EV_DROP, snap.round_id, now);
    drop.u.drop.bin = STC_BIN_FOOD;
    drop.u.drop.dist_mm = 40;
    if (stc_game_input(&drop) != STC_OK) return 1;
    now += cfg.settle_ms;
    stc_game_tick(now);
    now += cfg.close_timeout_ms;
    stc_game_tick(now);
    if (stc_game_snapshot().phase != STC_PH_LEARNING) return 1;

    if (continue_round(now + 10000)) {
        fprintf(stderr, "continue rejected\n");
        return 1;
    }
    if (stc_game_snapshot().phase != STC_PH_PROMPT ||
        stc_game_snapshot().round != 1) {
        fprintf(stderr, "continue did not advance exactly one round\n");
        return 1;
    }
    if (stc_game_snapshot().streak != 1) {
        fprintf(stderr, "correct streak=%u expected 1\n",
                (unsigned)stc_game_snapshot().streak);
        return 1;
    }
    return 0;
}

static int test_prompt_timeout_advances_to_learning(void)
{
    const uint32_t start = 500000u;
    if (stc_game_init() != STC_OK || stc_game_start(start) != STC_OK) return 1;
    stc_game_tick(start + stc_game_get_config().prompt_timeout_ms);

    const stc_snapshot_t snap = stc_game_snapshot();
    if (snap.phase != STC_PH_LEARNING) {
        fprintf(stderr, "prompt timeout phase=%d expected learning\n", (int)snap.phase);
        return 1;
    }
    if (snap.last_outcome != STC_ROUND_NO_SCAN) {
        fprintf(stderr, "prompt timeout outcome=%d\n", (int)snap.last_outcome);
        return 1;
    }
    if (stc_game_result().timed_out != 1 || stc_game_result().score_raw != 0) {
        fprintf(stderr, "prompt timeout result mismatch\n");
        return 1;
    }
    return 0;
}

static int test_scan_timeout_retries_then_abstains(void)
{
    const uint32_t start = 700000u;
    if (stc_game_init() != STC_OK || stc_game_start(start) != STC_OK) return 1;
    const stc_game_config_t cfg = stc_game_get_config();
    if (begin_scan(start + 10)) return 1;
    stc_game_tick(start + 10 + cfg.scan_timeout_ms);
    if (stc_game_snapshot().phase != STC_PH_PROMPT) {
        fprintf(stderr, "first scan timeout did not return to prompt\n");
        return 1;
    }
    if (begin_scan(start + 20 + cfg.scan_timeout_ms)) return 1;
    stc_game_tick(start + 20 + 2u * cfg.scan_timeout_ms);
    const stc_snapshot_t snap = stc_game_snapshot();
    if (snap.phase != STC_PH_LEARNING ||
        snap.last_outcome != STC_ROUND_ABSTAIN ||
        stc_game_result().abstained != 1 ||
        stc_game_result().score_raw != 0) {
        fprintf(stderr, "second scan timeout did not abstain\n");
        return 1;
    }
    return 0;
}

static int test_wait_drop_timeout_scores_zero(void)
{
    const uint32_t start = 800000u;
    if (stc_game_init() != STC_OK || stc_game_start(start) != STC_OK) return 1;
    const stc_game_config_t cfg = stc_game_get_config();
    uint32_t now = start + 10;
    if (begin_scan(now)) return 1;
    if (submit_label(STC_BIN_FOOD, 0.9f, false, now)) return 1;
    now += cfg.result_ms;
    stc_game_tick(now);
    now += cfg.open_timeout_ms;
    stc_game_tick(now);
    if (stc_game_snapshot().phase != STC_PH_WAIT_DROP) return 1;

    now += cfg.wait_drop_ms;
    stc_game_tick(now);
    const stc_snapshot_t snap = stc_game_snapshot();
    if (snap.phase != STC_PH_CLOSE ||
        snap.last_outcome != STC_ROUND_TIMEOUT ||
        snap.round_delta != 0 ||
        stc_game_result().timed_out != 1 ||
        stc_game_result().score_raw != 0) {
        fprintf(stderr, "WAIT_DROP timeout mismatch phase=%d outcome=%d\n",
                (int)snap.phase, (int)snap.last_outcome);
        return 1;
    }
    return 0;
}

static int test_total_time_ends_after_current_round(void)
{
    const uint32_t start = 1000u;
    if (stc_game_init() != STC_OK || stc_game_start(start) != STC_OK) return 1;

    uint32_t now = start + 1;
    const stc_game_config_t cfg = stc_game_get_config();
    for (int round = 0; round < 2; ++round) {
        if (begin_scan(now)) return 1;
        if (submit_label(STC_BIN_FOOD, 0.9f, false, now)) return 1;
        if (round == 0) {
            now += cfg.result_ms;
            stc_game_tick(now);
            now += cfg.open_timeout_ms;
            stc_game_tick(now);
            const stc_snapshot_t snap = stc_game_snapshot();
            stc_event_t drop = event(STC_EV_DROP, snap.round_id, now);
            drop.u.drop.bin = STC_BIN_FOOD;
            drop.u.drop.dist_mm = 40;
            if (stc_game_input(&drop) != STC_OK) return 1;
            now += cfg.settle_ms;
            stc_game_tick(now);
            now += cfg.close_timeout_ms;
            stc_game_tick(now);
            if (stc_game_snapshot().phase != STC_PH_LEARNING) return 1;
            if (continue_round(now)) return 1;
        } else {
            now = start + cfg.total_time_ms - 2000;
            stc_game_tick(now);
            now = start + cfg.total_time_ms - 500;
            stc_game_tick(now);
            /*
             * Cross the 180 s boundary while WAIT_DROP is active. The game
             * may finish this round, but must not jump to SUMMARY yet.
             */
            if (stc_game_snapshot().phase != STC_PH_WAIT_DROP) return 1;
            const stc_snapshot_t snap = stc_game_snapshot();
            stc_event_t drop = event(STC_EV_DROP, snap.round_id, now);
            drop.u.drop.bin = STC_BIN_FOOD;
            drop.u.drop.dist_mm = 40;
            if (stc_game_input(&drop) != STC_OK) return 1;
            if (stc_game_snapshot().phase == STC_PH_SUMMARY) {
                fprintf(stderr, "total time cut an active round\n");
                return 1;
            }
            now += cfg.settle_ms;
            stc_game_tick(now);
            now += cfg.close_timeout_ms;
            stc_game_tick(now);
            if (stc_game_snapshot().phase != STC_PH_LEARNING) return 1;
            if (continue_round(now)) return 1;
        }
        now += 10;
    }

    const stc_result_t result = stc_game_result();
    if (stc_game_snapshot().phase != STC_PH_SUMMARY ||
        !result.ended_by_time || result.duration_ms < cfg.total_time_ms) {
        fprintf(stderr, "time-limited summary not reached phase=%d ended=%d duration=%u\n",
                (int)stc_game_snapshot().phase, result.ended_by_time,
                (unsigned)result.duration_ms);
        return 1;
    }
    return 0;
}

static int test_stale_round_and_duplicate_drop(void)
{
    if (stc_game_init() != STC_OK || stc_game_start(1000) != STC_OK) return 1;

    const uint32_t old_round = stc_game_snapshot().round_id;
    if (begin_scan(1010)) return 1;
    stc_event_t stale = event(STC_EV_LABEL, old_round, 1020);
    stale.u.label.bin = STC_BIN_FOOD;
    stale.u.label.conf = 0.9f;
    stale.u.label.src = STC_SRC_STUB;
    if (stc_game_input(&stale) != STC_ERR_NOT_READY) {
        fprintf(stderr, "stale label was not rejected\n");
        return 1;
    }

    if (submit_label(STC_BIN_OTHER, 0.9f, false, 1020)) return 1;
    uint32_t now = 1020 + stc_game_get_config().result_ms;
    stc_game_tick(now);
    now += stc_game_get_config().open_timeout_ms;
    stc_game_tick(now);

    const stc_snapshot_t snap = stc_game_snapshot();
    stc_event_t drop = event(STC_EV_DROP, snap.round_id, now);
    drop.u.drop.bin = STC_BIN_OTHER;
    drop.u.drop.dist_mm = 40;
    if (stc_game_input(&drop) != STC_OK) return 1;
    if (stc_game_input(&drop) != STC_ERR_NOT_READY) {
        fprintf(stderr, "duplicate drop was accepted\n");
        return 1;
    }
    if (stc_game_result().rounds_played != 1 ||
        stc_game_result().correct != 1) {
        fprintf(stderr, "duplicate drop changed score\n");
        return 1;
    }
    return 0;
}

static int test_thousand_games_invariants(void)
{
    uint32_t seed = 0x13579BDFu;
    for (int game = 0; game < 1000; ++game) {
        if (stc_game_init() != STC_OK || stc_game_start((uint32_t)game * 1000u) != STC_OK) {
            return 1;
        }
        uint32_t now = (uint32_t)game * 1000u + 1u;
        uint8_t last_round = 0;
        for (int step = 0; step < 100; ++step) {
            const stc_snapshot_t snap = stc_game_snapshot();
            if (snap.score < 0 || snap.round < last_round) {
                fprintf(stderr, "invariant score/round game=%d step=%d\n", game, step);
                return 1;
            }
            last_round = snap.round;
            seed = seed * 1664525u + 1013904223u;
            switch (snap.phase) {
            case STC_PH_PROMPT: {
                stc_event_t ev = event(STC_EV_SCAN_REQUEST, snap.round_id, now);
                if (stc_game_input(&ev) != STC_OK) return 1;
                break;
            }
            case STC_PH_SCAN: {
                stc_event_t ev = event(STC_EV_LABEL, snap.round_id, now);
                ev.u.label.bin = (stc_bin_t)(seed % STC_BIN_COUNT);
                ev.u.label.conf = ((seed >> 8) % 100u) / 100.0f;
                ev.u.label.abstain = (seed & 0x80000000u) != 0;
                ev.u.label.src = STC_SRC_STUB;
                if (stc_game_input(&ev) != STC_OK) return 1;
                break;
            }
            case STC_PH_RESULT:
                now += stc_game_get_config().result_ms;
                stc_game_tick(now);
                break;
            case STC_PH_OPEN:
                now += stc_game_get_config().open_timeout_ms;
                stc_game_tick(now);
                break;
            case STC_PH_WAIT_DROP: {
                stc_event_t ev = event(STC_EV_DROP, snap.round_id, now);
                ev.u.drop.bin = (stc_bin_t)((seed >> 16) % STC_BIN_COUNT);
                ev.u.drop.dist_mm = 40;
                if (stc_game_input(&ev) != STC_OK) return 1;
                break;
            }
            case STC_PH_SETTLE:
                now += stc_game_get_config().settle_ms;
                stc_game_tick(now);
                break;
            case STC_PH_CLOSE:
                now += stc_game_get_config().close_timeout_ms;
                stc_game_tick(now);
                break;
            case STC_PH_LEARNING: {
                stc_event_t ev = event(STC_EV_CONTINUE, snap.round_id, now);
                if (stc_game_input(&ev) != STC_OK) return 1;
                break;
            }
            case STC_PH_SUMMARY:
                goto game_done;
            default:
                fprintf(stderr, "invalid phase game=%d phase=%d\n", game, (int)snap.phase);
                return 1;
            }
            now += 1;
        }
        fprintf(stderr, "game %d did not finish\n", game);
        return 1;
game_done:
        if (stc_game_result().rounds_played > 5 ||
            stc_game_result().score < 0) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    if (test_nonzero_start_and_continue()) return 1;
    if (test_prompt_timeout_advances_to_learning()) return 1;
    if (test_scan_timeout_retries_then_abstains()) return 1;
    if (test_wait_drop_timeout_scores_zero()) return 1;
    if (test_total_time_ends_after_current_round()) return 1;
    if (test_stale_round_and_duplicate_drop()) return 1;
    if (test_thousand_games_invariants()) return 1;
    printf("test_game_spec: OK\n");
    return 0;
}
