/* stc_hand_round.h — S3 lid/drop round state machine. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_HAND_ROUND_DETECT_TIMEOUT_MS 15000u
#define STC_HAND_ROUND_DROP_CLOSE_DELAY_MS 3000u

typedef enum {
    STC_HAND_ROUND_IDLE = 0,
    STC_HAND_ROUND_OPENING,
    STC_HAND_ROUND_DETECTING,
    STC_HAND_ROUND_WAIT_CLOSE,
    STC_HAND_ROUND_RESULT,
    STC_HAND_ROUND_HOLDING,
} stc_hand_round_phase_t;

typedef enum {
    STC_HAND_ROUND_ACTION_NONE = 0,
    STC_HAND_ROUND_ACTION_OPEN_ALL,
    STC_HAND_ROUND_ACTION_CLOSE_ALL,
} stc_hand_round_action_t;

typedef enum {
    STC_HAND_ROUND_OUTCOME_NONE = 0,
    STC_HAND_ROUND_OUTCOME_DROP,
    STC_HAND_ROUND_OUTCOME_TIMEOUT,
} stc_hand_round_outcome_t;

typedef struct {
    stc_hand_round_phase_t phase;
    stc_hand_round_outcome_t outcome;
    stc_hand_round_action_t pending_action;
    uint8_t round;
    uint8_t selected_bin;
    uint32_t deadline_ms;
    bool close_action_issued;
} stc_hand_round_t;

void stc_hand_round_init(stc_hand_round_t *round);
stc_err_t stc_hand_round_start(stc_hand_round_t *round, uint8_t round_id,
                               uint32_t now_ms);
stc_err_t stc_hand_round_hold_open(stc_hand_round_t *round, uint8_t round_id,
                                   uint32_t now_ms);
stc_err_t stc_hand_round_arm(stc_hand_round_t *round, uint32_t now_ms);
bool stc_hand_round_drop_accepted(stc_hand_round_t *round, uint8_t round_id,
                                  uint8_t bin, uint32_t now_ms);
void stc_hand_round_poll(stc_hand_round_t *round, uint32_t now_ms);
stc_hand_round_action_t stc_hand_round_take_action(stc_hand_round_t *round);
stc_err_t stc_hand_round_close_complete(stc_hand_round_t *round,
                                        uint32_t now_ms);

stc_hand_round_phase_t stc_hand_round_phase(const stc_hand_round_t *round);
stc_hand_round_outcome_t stc_hand_round_outcome(
    const stc_hand_round_t *round);
uint8_t stc_hand_round_selected_bin(const stc_hand_round_t *round);

#ifdef __cplusplus
}
#endif
