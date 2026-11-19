#include <stdio.h>

#include "stc_hand_round.h"

static int expect(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

int main(void)
{
    stc_hand_round_t round;
    stc_hand_round_init(&round);

    if (expect(stc_hand_round_start(&round, 7, 100) == STC_OK,
               "start a round") ||
        expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_OPEN_ALL,
               "start opens every lid") ||
        expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_NONE,
               "open action is emitted once") ||
        expect(stc_hand_round_arm(&round, 200) == STC_OK,
               "arm detection after opening") ||
        expect(stc_hand_round_phase(&round) ==
                   STC_HAND_ROUND_DETECTING,
               "round enters detection")) {
        return 1;
    }

    stc_hand_round_poll(&round, 15199);
    if (expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_NONE,
               "15 second timeout has not expired") ||
        expect(!stc_hand_round_drop_accepted(&round, 7, 2, 15200),
               "drop at timeout boundary is rejected")) {
        return 1;
    }

    stc_hand_round_poll(&round, 15200);
    if (expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_CLOSE_ALL,
               "timeout closes every lid") ||
        expect(stc_hand_round_outcome(&round) ==
                   STC_HAND_ROUND_OUTCOME_TIMEOUT,
               "timeout is recorded") ||
        expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_NONE,
               "timeout close is emitted once")) {
        return 1;
    }

    stc_hand_round_poll(&round, 15300);
    if (expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_NONE,
               "close action is not re-emitted while lids settle") ||
        expect(stc_hand_round_close_complete(&round, 16000) == STC_OK,
               "close completion finishes the round") ||
        expect(stc_hand_round_phase(&round) ==
                   STC_HAND_ROUND_RESULT,
               "timeout result is available")) {
        return 1;
    }

    if (expect(stc_hand_round_start(&round, 8, 20000) == STC_OK,
               "restart after result") ||
        expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_OPEN_ALL,
               "restart opens lids") ||
        expect(stc_hand_round_arm(&round, 21000) == STC_OK,
               "restart arms detection") ||
        expect(!stc_hand_round_drop_accepted(&round, 7, 1, 22000),
               "old round drop is rejected") ||
        expect(stc_hand_round_drop_accepted(&round, 8, 2, 22000),
               "current round drop is accepted") ||
        expect(!stc_hand_round_drop_accepted(&round, 8, 1, 23000),
               "later drop cannot change the answer") ||
        expect(stc_hand_round_outcome(&round) ==
                   STC_HAND_ROUND_OUTCOME_DROP,
               "drop outcome is recorded") ||
        expect(stc_hand_round_selected_bin(&round) == 2,
               "drop bin is latched")) {
        return 1;
    }

    stc_hand_round_poll(&round, 24999);
    if (expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_NONE,
               "drop close delay has not expired") ||
        expect(!stc_hand_round_drop_accepted(&round, 8, 3, 25000),
               "drop exactly at close time is not a new answer") ||
        expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_NONE,
               "late duplicate does not close early")) {
        return 1;
    }

    stc_hand_round_poll(&round, 25000);
    if (expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_CLOSE_ALL,
               "drop closes lids after three seconds") ||
        expect(stc_hand_round_selected_bin(&round) == 2,
               "late duplicate does not overwrite answer") ||
        expect(stc_hand_round_close_complete(&round, 26000) == STC_OK,
               "drop close completion finishes the round")) {
        return 1;
    }

    if (expect(stc_hand_round_hold_open(&round, 9, 30000) == STC_OK,
               "summary hold opens lids") ||
        expect(stc_hand_round_phase(&round) == STC_HAND_ROUND_HOLDING,
               "summary hold phase is latched") ||
        expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_OPEN_ALL,
               "summary hold emits one open action") ||
        expect(!stc_hand_round_drop_accepted(&round, 9, 1, 31000),
               "summary hold does not accept drops")) {
        return 1;
    }
    stc_hand_round_poll(&round, 120000);
    if (expect(stc_hand_round_take_action(&round) ==
                   STC_HAND_ROUND_ACTION_NONE,
               "summary hold never auto-closes") ||
        expect(stc_hand_round_start(&round, 10, 120001) ==
                   STC_ERR_NOT_READY,
               "new round cannot start while summary hold is active")) {
        return 1;
    }

    stc_hand_round_t wait_close;
    stc_hand_round_init(&wait_close);
    if (expect(stc_hand_round_start(&wait_close, 11, 130000) == STC_OK,
               "prepare wait-close hold test") ||
        expect(stc_hand_round_arm(&wait_close, 130100) == STC_OK,
               "arm wait-close hold test") ||
        expect(stc_hand_round_drop_accepted(&wait_close, 11, 0, 130200),
               "enter wait-close before summary hold") ||
        expect(stc_hand_round_hold_open(&wait_close, 11, 130300) == STC_OK,
               "summary hold can replace an outstanding close") ||
        expect(stc_hand_round_phase(&wait_close) ==
                   STC_HAND_ROUND_HOLDING,
               "summary hold wins over deferred close")) {
        return 1;
    }

    puts("test_hand_round: open, timeout, drop latch, close and hold OK");
    return 0;
}
