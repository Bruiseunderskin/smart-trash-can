#include "stc_game.h"

#include "stc_game_internal.h"

static stc_snapshot_t snapshot;
static stc_game_phase_t phase;
static uint32_t phase_started_ms;
static int8_t last_delta;

stc_err_t stc_game_init(void) {
    snapshot = (stc_snapshot_t){0};
    snapshot.rounds_total = 5;
    phase = STC_GAME_IDLE;
    phase_started_ms = 0;
    last_delta = 0;
    return STC_OK;
}

stc_err_t stc_game_start(void) {
    snapshot = (stc_snapshot_t){
        .round = 0,
        .rounds_total = 5,
        .score = 0,
    };
    phase = STC_GAME_CAPTURE;
    phase_started_ms = 0;
    last_delta = 0;
    return STC_OK;
}

void stc_game_tick(uint32_t now_ms) {
    if (phase == STC_GAME_WAIT_DROP &&
        now_ms - phase_started_ms >= STC_GAME_DROP_TIMEOUT_MS) {
        last_delta = 0;
        phase = STC_GAME_ROUND_RESULT;
        phase_started_ms = now_ms;
    }
    if (phase == STC_GAME_ROUND_RESULT &&
        now_ms - phase_started_ms >= STC_GAME_RESULT_MS) {
        ++snapshot.round;
        snapshot.lid_open = false;
        phase = snapshot.round >= snapshot.rounds_total ? STC_GAME_SETTLEMENT
                                                       : STC_GAME_CAPTURE;
        phase_started_ms = now_ms;
    }
}

stc_err_t stc_game_input(const stc_event_t *event) {
    if (!event) {
        return STC_ERR_CONTRACT;
    }
    switch (event->type) {
    case STC_EV_LABEL:
        if (phase != STC_GAME_CAPTURE) {
            return STC_ERR_NOT_READY;
        }
        snapshot.truth = event->u.label.bin;
        snapshot.lid_open = false;
        phase = STC_GAME_WAIT_DROP;
        phase_started_ms = event->t_ms;
        return STC_OK;
    case STC_EV_DROP:
        if (phase != STC_GAME_WAIT_DROP) {
            return STC_ERR_NOT_READY;
        }
        snapshot.lid_open = false;
        if (event->u.drop.bin == snapshot.truth) {
            last_delta = 10;
        } else {
            last_delta = -5;
        }
        snapshot.score += last_delta;
        phase = STC_GAME_ROUND_RESULT;
        phase_started_ms = event->t_ms;
        return STC_OK;
    case STC_EV_INSERT_TIMEOUT:
        if (phase != STC_GAME_WAIT_DROP) {
            return STC_ERR_NOT_READY;
        }
        last_delta = 0;
        snapshot.lid_open = false;
        phase = STC_GAME_ROUND_RESULT;
        phase_started_ms = event->t_ms;
        return STC_OK;
    case STC_EV_PLAY:
        if (phase != STC_GAME_IDLE && phase != STC_GAME_SETTLEMENT) {
            return STC_ERR_NOT_READY;
        }
        return stc_game_start();
    case STC_EV_MODE_CHANGED:
        return STC_OK;
    default:
        return STC_ERR_CONTRACT;
    }
}

stc_snapshot_t stc_game_snapshot(void) { return snapshot; }

stc_game_phase_t stc_game_phase(void) { return phase; }

int8_t stc_game_last_delta(void) { return last_delta; }

bool stc_game_can_enter_chat(void) { return phase == STC_GAME_SETTLEMENT; }
