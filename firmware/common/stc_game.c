#include "stc_game.h"

#include <string.h>

#include "stc_time.h"

static stc_game_config_t g_cfg;
static stc_snapshot_t g_snap;
static stc_result_t g_result;
static uint32_t g_phase_at;
static uint8_t g_retry;
static bool g_started;

static void enter_phase(stc_phase_t phase, uint32_t now)
{
    g_snap.phase = phase;
    g_phase_at = now;
}

static uint32_t phase_elapsed(uint32_t now) { return now - g_phase_at; }

stc_err_t stc_game_init(void)
{
    memset(&g_cfg, 0, sizeof g_cfg);
    g_cfg.rounds_total = 5;
    g_cfg.total_time_ms = 180000u;
    g_cfg.prompt_timeout_ms = 20000u;
    g_cfg.scan_timeout_ms = 1500u;
    g_cfg.result_ms = 1500u;
    g_cfg.open_timeout_ms = 1500u;
    g_cfg.wait_drop_ms = 15000u;
    g_cfg.settle_ms = 300u;
    g_cfg.close_timeout_ms = 1500u;
    g_cfg.learning_ms = 6000u;
    g_cfg.score_correct = 10;
    g_cfg.score_wrong = -5;
    g_cfg.score_floor = 0;
    g_cfg.max_retry = 1;
    memset(&g_snap, 0, sizeof g_snap);
    g_snap.phase = STC_PH_IDLE;
    g_snap.truth = STC_BIN_COUNT;
    g_snap.dropped = STC_BIN_COUNT;
    memset(&g_result, 0, sizeof g_result);
    g_started = false;
    return STC_OK;
}

stc_err_t stc_game_set_config(const stc_game_config_t *cfg)
{
    if (!cfg || cfg->rounds_total == 0) {
        return STC_ERR_CONTRACT;
    }
    g_cfg = *cfg;
    return STC_OK;
}

stc_game_config_t stc_game_get_config(void) { return g_cfg; }

stc_err_t stc_game_start(void)
{
    memset(&g_snap, 0, sizeof g_snap);
    memset(&g_result, 0, sizeof g_result);
    g_snap.rounds_total = g_cfg.rounds_total;
    g_snap.truth = STC_BIN_COUNT;
    g_snap.dropped = STC_BIN_COUNT;
    g_retry = 0;
    g_started = true;
    enter_phase(STC_PH_PROMPT, 0);
    return STC_OK;
}

static void finish_round(bool correct, bool abstained, bool timeout)
{
    const int16_t delta = correct ? g_cfg.score_correct : (abstained ? 0 : g_cfg.score_wrong);
    const int16_t raw = (int16_t)(g_result.score_raw + delta);
    g_result.score_raw = raw;
    g_result.score = (raw < g_cfg.score_floor) ? g_cfg.score_floor : raw;
    g_snap.score = g_result.score;
    g_result.rounds_played++;
    if (correct) g_result.correct++;
    else if (abstained) g_result.abstained++;
    else if (timeout) g_result.timed_out++;
    else g_result.wrong++;
}

void stc_game_tick(uint32_t now_ms_in)
{
    if (!g_started) {
        return;
    }
    const uint32_t elapsed = phase_elapsed(now_ms_in);

    switch (g_snap.phase) {
    case STC_PH_PROMPT:
        if (elapsed >= g_cfg.prompt_timeout_ms) {
            finish_round(false, false, true);
            enter_phase(STC_PH_SUMMARY, now_ms_in);
        }
        break;
    case STC_PH_SCAN:
        if (elapsed >= g_cfg.scan_timeout_ms) {
            finish_round(false, true, true);
            enter_phase(STC_PH_LEARNING, now_ms_in);
        }
        break;
    case STC_PH_RESULT:
        if (elapsed >= g_cfg.result_ms) {
            enter_phase(STC_PH_OPEN, now_ms_in);
        }
        break;
    case STC_PH_OPEN:
        if (elapsed >= g_cfg.open_timeout_ms) {
            enter_phase(STC_PH_WAIT_DROP, now_ms_in);
        }
        break;
    case STC_PH_WAIT_DROP:
        if (elapsed >= g_cfg.wait_drop_ms) {
            finish_round(false, false, true);
            enter_phase(STC_PH_CLOSE, now_ms_in);
        }
        break;
    case STC_PH_SETTLE:
        if (elapsed >= g_cfg.settle_ms) {
            enter_phase(STC_PH_CLOSE, now_ms_in);
        }
        break;
    case STC_PH_CLOSE:
        if (elapsed >= g_cfg.close_timeout_ms) {
            enter_phase(STC_PH_LEARNING, now_ms_in);
        }
        break;
    case STC_PH_LEARNING:
        if (elapsed >= g_cfg.learning_ms) {
            if ((uint8_t)(g_snap.round + 1) >= g_cfg.rounds_total) {
                enter_phase(STC_PH_SUMMARY, now_ms_in);
            } else {
                g_snap.round++;
                g_snap.truth = STC_BIN_COUNT;
                g_snap.dropped = STC_BIN_COUNT;
                g_retry = 0;
                enter_phase(STC_PH_PROMPT, now_ms_in);
            }
        }
        break;
    default:
        break;
    }

}

stc_err_t stc_game_input(const stc_event_t *ev)
{
    if (!ev) {
        return STC_ERR_CONTRACT;
    }
    const uint32_t now = (uint32_t)((ev->t_ms));
    switch (ev->type) {
    case STC_EV_LABEL: {
        if (g_snap.phase != STC_PH_SCAN && g_snap.phase != STC_PH_PROMPT) {
            return STC_ERR_NOT_READY;
        }
        g_snap.truth = ev->u.label.bin;
        g_snap.conf = ev->u.label.conf;
        g_snap.src = ev->u.label.src;
        g_snap.abstained = ev->u.label.abstain;
        if (ev->u.label.abstain && g_retry < g_cfg.max_retry) {
            g_retry++;
            enter_phase(STC_PH_PROMPT, now);
            return STC_OK;
        }
        if (ev->u.label.abstain) {
            finish_round(false, true, false);
            enter_phase(STC_PH_LEARNING, now);
            return STC_OK;
        }
        enter_phase(STC_PH_RESULT, now);
        return STC_OK;
    }
    case STC_EV_DROP: {
        if (g_snap.phase != STC_PH_WAIT_DROP) {
            return STC_ERR_NOT_READY;
        }
        g_snap.dropped = ev->u.drop.bin;
        g_snap.dist_mm = ev->u.drop.dist_mm;
        finish_round(ev->u.drop.bin == g_snap.truth, false, false);
        enter_phase(STC_PH_SETTLE, now);
        return STC_OK;
    }
    case STC_EV_INSERT_TIMEOUT:
        if (g_snap.phase != STC_PH_WAIT_DROP) {
            return STC_ERR_NOT_READY;
        }
        finish_round(false, false, true);
        enter_phase(STC_PH_CLOSE, now);
        return STC_OK;
    default:
        return STC_ERR_NOT_READY;
    }
}

stc_err_t stc_game_reset(void)
{
    memset(&g_snap, 0, sizeof g_snap);
    memset(&g_result, 0, sizeof g_result);
    g_started = false;
    g_snap.phase = STC_PH_IDLE;
    g_snap.truth = STC_BIN_COUNT;
    g_snap.dropped = STC_BIN_COUNT;
    return STC_OK;
}

stc_snapshot_t stc_game_snapshot(void) { return g_snap; }
stc_result_t stc_game_result(void) { return g_result; }
