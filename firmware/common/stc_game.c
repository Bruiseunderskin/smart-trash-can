#include "stc_game.h"

#include <string.h>

#include "stc_time.h"

static stc_game_config_t g_cfg;
static stc_snapshot_t g_snap;
static stc_result_t g_result;
static uint32_t g_phase_at;
static uint32_t g_start_at;
static uint32_t g_next_round_id = 1;
static uint32_t g_round_id;
static uint8_t g_retry;
static uint8_t g_streak;
static bool g_time_up;
static bool g_started;

static void enter_phase(stc_phase_t phase, uint32_t now)
{
    g_snap.phase = phase;
    g_snap.phase_elapsed_ms = 0;
    g_phase_at = now;
}

static uint32_t phase_elapsed(uint32_t now) { return now - g_phase_at; }

static void update_elapsed(uint32_t now)
{
    g_snap.elapsed_ms = now - g_start_at;
    g_snap.phase_elapsed_ms = now - g_phase_at;
    if (g_snap.elapsed_ms >= g_cfg.total_time_ms) {
        g_time_up = true;
    }
}

static void prepare_round(uint32_t now)
{
    g_snap.truth = STC_BIN_COUNT;
    g_snap.dropped = STC_BIN_COUNT;
    g_snap.conf = 0.0f;
    g_snap.agreement = 0;
    g_snap.abstained = false;
    g_snap.round_delta = 0;
    g_snap.dist_mm = 0;
    g_snap.src = STC_SRC_STUB;
    g_snap.streak = g_streak;
    g_snap.last_outcome = STC_ROUND_NONE;
    g_retry = 0;
    enter_phase(STC_PH_PROMPT, now);
}

static void start_round(uint32_t now)
{
    g_round_id = g_next_round_id++;
    g_snap.round_id = g_round_id;
    prepare_round(now);
}

static void restart_round(uint32_t now)
{
    g_round_id = g_next_round_id++;
    g_snap.round_id = g_round_id;
    prepare_round(now);
}

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
    memset(&g_result, 0, sizeof g_result);
    g_snap.phase = STC_PH_IDLE;
    g_snap.truth = STC_BIN_COUNT;
    g_snap.dropped = STC_BIN_COUNT;
    g_snap.last_outcome = STC_ROUND_NONE;
    g_streak = 0;
    g_time_up = false;
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

stc_err_t stc_game_start(uint32_t now_ms)
{
    memset(&g_snap, 0, sizeof g_snap);
    memset(&g_result, 0, sizeof g_result);
    g_snap.rounds_total = g_cfg.rounds_total;
    g_snap.last_outcome = STC_ROUND_NONE;
    g_start_at = now_ms;
    g_streak = 0;
    g_time_up = false;
    g_started = true;
    start_round(now_ms);
    g_snap.elapsed_ms = 0;
    return STC_OK;
}

static void finish_round(stc_round_outcome_t outcome, uint32_t now)
{
    int16_t delta = 0;
    if (outcome == STC_ROUND_CORRECT) {
        delta = g_cfg.score_correct;
    } else if (outcome == STC_ROUND_WRONG) {
        delta = g_cfg.score_wrong;
    }

    const int16_t raw = (int16_t)(g_result.score_raw + delta);
    g_result.score_raw = raw;
    g_result.score = raw < g_cfg.score_floor ? g_cfg.score_floor : raw;

    g_snap.score = g_result.score;
    g_snap.round_delta = delta;
    g_snap.last_outcome = outcome;
    g_result.rounds_played++;

    switch (outcome) {
    case STC_ROUND_CORRECT:
        g_result.correct++;
        if (g_streak < UINT8_MAX) {
            g_streak++;
        }
        if (g_streak > g_result.best_streak) {
            g_result.best_streak = g_streak;
        }
        break;
    case STC_ROUND_WRONG:
        g_result.wrong++;
        g_streak = 0;
        break;
    case STC_ROUND_TIMEOUT:
    case STC_ROUND_NO_SCAN:
        g_result.timed_out++;
        g_streak = 0;
        break;
    case STC_ROUND_ABSTAIN:
        g_result.abstained++;
        g_streak = 0;
        break;
    default:
        break;
    }

    g_snap.streak = g_streak;
    update_elapsed(now);
}

static void finish_game(uint32_t now)
{
    update_elapsed(now);
    g_result.duration_ms = g_snap.elapsed_ms;
    g_result.ended_by_time = g_time_up;
    enter_phase(STC_PH_SUMMARY, now);
}

void stc_game_tick(uint32_t now_ms)
{
    if (!g_started) {
        return;
    }

    update_elapsed(now_ms);
    const uint32_t elapsed = phase_elapsed(now_ms);

    switch (g_snap.phase) {
    case STC_PH_PROMPT:
        if (elapsed >= g_cfg.prompt_timeout_ms) {
            finish_round(STC_ROUND_NO_SCAN, now_ms);
            enter_phase(STC_PH_LEARNING, now_ms);
        }
        break;
    case STC_PH_SCAN:
        if (elapsed >= g_cfg.scan_timeout_ms) {
            if (g_retry < g_cfg.max_retry) {
                g_retry++;
                enter_phase(STC_PH_PROMPT, now_ms);
            } else {
                g_snap.abstained = true;
                finish_round(STC_ROUND_ABSTAIN, now_ms);
                enter_phase(STC_PH_LEARNING, now_ms);
            }
        }
        break;
    case STC_PH_RESULT:
        if (elapsed >= g_cfg.result_ms) {
            enter_phase(STC_PH_OPEN, now_ms);
        }
        break;
    case STC_PH_OPEN:
        if (elapsed >= g_cfg.open_timeout_ms) {
            enter_phase(STC_PH_WAIT_DROP, now_ms);
        }
        break;
    case STC_PH_WAIT_DROP:
        if (elapsed >= g_cfg.wait_drop_ms) {
            finish_round(STC_ROUND_TIMEOUT, now_ms);
            enter_phase(STC_PH_CLOSE, now_ms);
        }
        break;
    case STC_PH_SETTLE:
        if (elapsed >= g_cfg.settle_ms) {
            enter_phase(STC_PH_CLOSE, now_ms);
        }
        break;
    case STC_PH_CLOSE:
        if (elapsed >= g_cfg.close_timeout_ms) {
            enter_phase(STC_PH_LEARNING, now_ms);
        }
        break;
    default:
        break;
    }
}

static stc_err_t continue_round(const stc_event_t *ev)
{
    if (g_snap.phase != STC_PH_LEARNING || ev->round_id != g_round_id) {
        return STC_ERR_NOT_READY;
    }
    if (g_time_up ||
        (uint8_t)(g_snap.round + 1) >= g_cfg.rounds_total) {
        finish_game(ev->t_ms);
    } else {
        g_snap.round++;
        start_round(ev->t_ms);
    }
    return STC_OK;
}

static stc_err_t begin_scan(const stc_event_t *ev)
{
    if (g_snap.phase != STC_PH_PROMPT || ev->round_id != g_round_id) {
        return STC_ERR_NOT_READY;
    }
    g_round_id = g_next_round_id++;
    g_snap.round_id = g_round_id;
    enter_phase(STC_PH_SCAN, ev->t_ms);
    return STC_OK;
}

static stc_err_t accept_label(const stc_event_t *ev)
{
    if (g_snap.phase != STC_PH_SCAN || ev->round_id != g_round_id) {
        return STC_ERR_NOT_READY;
    }
    if (!ev->u.label.abstain && ev->u.label.bin >= STC_BIN_COUNT) {
        return STC_ERR_CONTRACT;
    }

    g_snap.truth = ev->u.label.abstain ? STC_BIN_COUNT
                                       : ev->u.label.bin;
    g_snap.conf = ev->u.label.conf;
    g_snap.src = ev->u.label.src;
    g_snap.abstained = ev->u.label.abstain;

    if (ev->u.label.abstain) {
        restart_round(ev->t_ms);
        return STC_OK;
    }

    enter_phase(STC_PH_RESULT, ev->t_ms);
    return STC_OK;
}

static stc_err_t accept_drop(const stc_event_t *ev)
{
    if (g_snap.phase != STC_PH_WAIT_DROP || ev->round_id != g_round_id) {
        return STC_ERR_NOT_READY;
    }
    if (ev->u.drop.bin >= STC_BIN_COUNT) {
        return STC_ERR_CONTRACT;
    }

    g_snap.dropped = ev->u.drop.bin;
    g_snap.dist_mm = ev->u.drop.dist_mm;
    finish_round(ev->u.drop.bin == g_snap.truth
                     ? STC_ROUND_CORRECT
                     : STC_ROUND_WRONG,
                 ev->t_ms);
    enter_phase(STC_PH_SETTLE, ev->t_ms);
    return STC_OK;
}

static stc_err_t accept_insert_timeout(const stc_event_t *ev)
{
    if (g_snap.phase != STC_PH_WAIT_DROP || ev->round_id != g_round_id) {
        return STC_ERR_NOT_READY;
    }
    finish_round(STC_ROUND_TIMEOUT, ev->t_ms);
    enter_phase(STC_PH_CLOSE, ev->t_ms);
    return STC_OK;
}

stc_err_t stc_game_input(const stc_event_t *ev)
{
    if (!ev || !g_started) {
        return ev ? STC_ERR_NOT_READY : STC_ERR_CONTRACT;
    }

    update_elapsed(ev->t_ms);
    switch (ev->type) {
    case STC_EV_SCAN_REQUEST:
        return begin_scan(ev);
    case STC_EV_CONTINUE:
        return continue_round(ev);
    case STC_EV_LABEL:
        return accept_label(ev);
    case STC_EV_DROP:
        return accept_drop(ev);
    case STC_EV_INSERT_TIMEOUT:
        return accept_insert_timeout(ev);
    default:
        return STC_ERR_NOT_READY;
    }
}

stc_err_t stc_game_reset(void)
{
    memset(&g_snap, 0, sizeof g_snap);
    memset(&g_result, 0, sizeof g_result);
    g_started = false;
    g_streak = 0;
    g_time_up = false;
    g_snap.phase = STC_PH_IDLE;
    g_snap.truth = STC_BIN_COUNT;
    g_snap.dropped = STC_BIN_COUNT;
    g_snap.last_outcome = STC_ROUND_NONE;
    return STC_OK;
}

stc_snapshot_t stc_game_snapshot(void) { return g_snap; }
stc_result_t stc_game_result(void) { return g_result; }
