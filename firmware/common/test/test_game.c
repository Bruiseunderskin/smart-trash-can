/* test_game.c — 5 回合、計分、floor、learning、超時（#53 I3／H4） */
#include <stdio.h>
#include <string.h>

#include "stc_game.h"

static stc_event_t label_ev(stc_bin_t bin, float conf, bool abstain, uint32_t t)
{
    stc_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.type = STC_EV_LABEL;
    ev.t_ms = t;
    ev.u.label.bin = bin;
    ev.u.label.conf = conf;
    ev.u.label.abstain = abstain;
    ev.u.label.src = STC_SRC_STUB;
    return ev;
}

static stc_event_t drop_ev(stc_bin_t bin, uint32_t t)
{
    stc_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.type = STC_EV_DROP;
    ev.t_ms = t;
    ev.u.drop.bin = bin;
    ev.u.drop.dist_mm = 40;
    return ev;
}

int main(void)
{
    if (stc_game_init() != STC_OK) return 1;
    if (stc_game_start() != STC_OK) return 1;

    uint32_t t = 0;
    /* 5 回合：3 對 2 錯 → 3*10 - 2*5 = 20 */
    const stc_bin_t truth[5] = {STC_BIN_FOOD, STC_BIN_HAZARD, STC_BIN_OTHER,
                                STC_BIN_RECYCLABLE, STC_BIN_FOOD};
    const bool right[5] = {true, true, false, true, false};

    for (int r = 0; r < 5; ++r) {
        stc_event_t l = label_ev(truth[r], 0.9f, false, t);
        if (stc_game_input(&l) != STC_OK) { fprintf(stderr, "label r%d\n", r); return 1; }
        t += 1500;
        stc_game_tick(t);                       /* RESULT → OPEN */
        t += 1500;
        stc_game_tick(t);                       /* OPEN → WAIT_DROP */
        const stc_bin_t wrong = (truth[r] == STC_BIN_OTHER) ? STC_BIN_FOOD : STC_BIN_OTHER;
        stc_event_t d = drop_ev(right[r] ? truth[r] : wrong, t);
        if (stc_game_input(&d) != STC_OK) { fprintf(stderr, "drop r%d\n", r); return 1; }
        t += 300;
        stc_game_tick(t);                       /* SETTLE → CLOSE */
        t += 1500;
        stc_game_tick(t);                       /* CLOSE → LEARNING */
        t += 6000;
        stc_game_tick(t);                       /* LEARNING → next PROMPT or SUMMARY */
    }

    stc_result_t res = stc_game_result();
    if (res.rounds_played != 5) { fprintf(stderr, "rounds %u\n", res.rounds_played); return 1; }
    if (res.correct != 3 || res.wrong != 2) {
        fprintf(stderr, "correct %u wrong %u\n", res.correct, res.wrong);
        return 1;
    }
    if (res.score != 20) { fprintf(stderr, "score %d, expected 20\n", res.score); return 1; }
    if (stc_game_snapshot().phase != STC_PH_SUMMARY) {
        fprintf(stderr, "phase after 5 rounds = %d\n", (int)stc_game_snapshot().phase);
        return 1;
    }

    /* floor：連續投錯 3 次，分數不得為負 */
    stc_game_start();
    t = 0;
    for (int r = 0; r < 3; ++r) {
        stc_event_t l = label_ev(STC_BIN_FOOD, 0.9f, false, t);
        stc_game_input(&l);
        t += 1500; stc_game_tick(t);
        t += 1500; stc_game_tick(t);
        stc_event_t d = drop_ev(STC_BIN_OTHER, t);   /* truth 係 FOOD，所以 OTHER 一定錯 */
        stc_game_input(&d);
        t += 300; stc_game_tick(t);
        t += 1500; stc_game_tick(t);
        t += 6000; stc_game_tick(t);
    }
    stc_result_t res2 = stc_game_result();
    if (res2.score != 0) { fprintf(stderr, "floor score %d\n", res2.score); return 1; }
    if (res2.score_raw != -15) { fprintf(stderr, "raw %d\n", res2.score_raw); return 1; }

    /* abstain：低於門檻，第一次重掃，第二次當 abstain 不扣分 */
    stc_game_start();
    t = 0;
    stc_event_t a1 = label_ev(STC_BIN_COUNT, 0.3f, true, 0);
    if (stc_game_input(&a1) != STC_OK) return 1;
    if (stc_game_snapshot().phase != STC_PH_PROMPT) { fprintf(stderr, "no retry prompt\n"); return 1; }
    stc_event_t a2 = label_ev(STC_BIN_COUNT, 0.3f, true, 100);
    if (stc_game_input(&a2) != STC_OK) return 1;
    if (stc_game_snapshot().phase != STC_PH_LEARNING) { fprintf(stderr, "abstain phase\n"); return 1; }
    stc_result_t res3 = stc_game_result();
    if (res3.abstained != 1 || res3.score != 0) { fprintf(stderr, "abstain counting\n"); return 1; }

    printf("test_game: 5 rounds, score 20, floor 0, abstain retry\n");
    return 0;
}
