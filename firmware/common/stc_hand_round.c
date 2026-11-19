#include "stc_hand_round.h"

#include <string.h>

static bool time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

void stc_hand_round_init(stc_hand_round_t *round)
{
    if (!round) {
        return;
    }
    memset(round, 0, sizeof *round);
    round->phase = STC_HAND_ROUND_IDLE;
    round->outcome = STC_HAND_ROUND_OUTCOME_NONE;
    round->pending_action = STC_HAND_ROUND_ACTION_NONE;
    round->selected_bin = 0xFF;
}

stc_err_t stc_hand_round_start(stc_hand_round_t *round, uint8_t round_id,
                               uint32_t now_ms)
{
    (void)now_ms;
    if (!round) {
        return STC_ERR_CONTRACT;
    }
    if (round->phase != STC_HAND_ROUND_IDLE &&
        round->phase != STC_HAND_ROUND_RESULT) {
        return STC_ERR_NOT_READY;
    }

    stc_hand_round_init(round);
    round->phase = STC_HAND_ROUND_OPENING;
    round->pending_action = STC_HAND_ROUND_ACTION_OPEN_ALL;
    round->round = round_id;
    return STC_OK;
}

stc_err_t stc_hand_round_hold_open(stc_hand_round_t *round, uint8_t round_id,
                                   uint32_t now_ms)
{
    (void)now_ms;
    if (!round) {
        return STC_ERR_CONTRACT;
    }
    if (round->phase != STC_HAND_ROUND_IDLE &&
        round->phase != STC_HAND_ROUND_RESULT &&
        round->phase != STC_HAND_ROUND_WAIT_CLOSE) {
        return STC_ERR_NOT_READY;
    }

    stc_hand_round_init(round);
    round->phase = STC_HAND_ROUND_HOLDING;
    round->pending_action = STC_HAND_ROUND_ACTION_OPEN_ALL;
    round->round = round_id;
    return STC_OK;
}

stc_err_t stc_hand_round_arm(stc_hand_round_t *round, uint32_t now_ms)
{
    if (!round || round->phase != STC_HAND_ROUND_OPENING) {
        return round ? STC_ERR_NOT_READY : STC_ERR_CONTRACT;
    }
    round->phase = STC_HAND_ROUND_DETECTING;
    round->deadline_ms = now_ms + STC_HAND_ROUND_DETECT_TIMEOUT_MS;
    return STC_OK;
}

bool stc_hand_round_drop_accepted(stc_hand_round_t *round, uint8_t round_id,
                                  uint8_t bin, uint32_t now_ms)
{
    if (!round || bin >= STC_BIN_COUNT ||
        round->phase != STC_HAND_ROUND_DETECTING ||
        round_id != round->round ||
        time_reached(now_ms, round->deadline_ms)) {
        return false;
    }

    round->phase = STC_HAND_ROUND_WAIT_CLOSE;
    round->outcome = STC_HAND_ROUND_OUTCOME_DROP;
    round->selected_bin = bin;
    round->deadline_ms = now_ms + STC_HAND_ROUND_DROP_CLOSE_DELAY_MS;
    round->pending_action = STC_HAND_ROUND_ACTION_NONE;
    return true;
}

void stc_hand_round_poll(stc_hand_round_t *round, uint32_t now_ms)
{
    if (!round || round->pending_action != STC_HAND_ROUND_ACTION_NONE ||
        !time_reached(now_ms, round->deadline_ms)) {
        return;
    }

    if (round->phase == STC_HAND_ROUND_DETECTING) {
        round->outcome = STC_HAND_ROUND_OUTCOME_TIMEOUT;
        round->selected_bin = 0xFF;
        round->phase = STC_HAND_ROUND_WAIT_CLOSE;
        round->pending_action = STC_HAND_ROUND_ACTION_CLOSE_ALL;
        round->close_action_issued = true;
    } else if (round->phase == STC_HAND_ROUND_WAIT_CLOSE &&
               round->outcome != STC_HAND_ROUND_OUTCOME_NONE &&
               !round->close_action_issued) {
        round->pending_action = STC_HAND_ROUND_ACTION_CLOSE_ALL;
        round->close_action_issued = true;
    }
}

stc_hand_round_action_t stc_hand_round_take_action(stc_hand_round_t *round)
{
    if (!round) {
        return STC_HAND_ROUND_ACTION_NONE;
    }
    const stc_hand_round_action_t action = round->pending_action;
    round->pending_action = STC_HAND_ROUND_ACTION_NONE;
    return action;
}

stc_err_t stc_hand_round_close_complete(stc_hand_round_t *round,
                                        uint32_t now_ms)
{
    (void)now_ms;
    if (!round) {
        return STC_ERR_CONTRACT;
    }
    if (round->phase != STC_HAND_ROUND_WAIT_CLOSE ||
        round->outcome == STC_HAND_ROUND_OUTCOME_NONE ||
        !round->close_action_issued ||
        round->pending_action != STC_HAND_ROUND_ACTION_NONE) {
        return STC_ERR_NOT_READY;
    }
    round->phase = STC_HAND_ROUND_RESULT;
    round->deadline_ms = 0;
    return STC_OK;
}

stc_hand_round_phase_t stc_hand_round_phase(const stc_hand_round_t *round)
{
    return round ? round->phase : STC_HAND_ROUND_IDLE;
}

stc_hand_round_outcome_t stc_hand_round_outcome(
    const stc_hand_round_t *round)
{
    return round ? round->outcome : STC_HAND_ROUND_OUTCOME_NONE;
}

uint8_t stc_hand_round_selected_bin(const stc_hand_round_t *round)
{
    return round ? round->selected_bin : 0xFF;
}
