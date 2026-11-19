/* test_game.c — 5 rounds, scoring, floor, learning, and abstain retry. */
#include <stdio.h>
#include <string.h>

#include "stc_game.h"

static stc_event_t make_event(stc_ev_type_t type, uint32_t t)
{
    stc_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.type = type;
    ev.round_id = stc_game_snapshot().round_id;
    ev.t_ms = t;
    return ev;
}

static int begin_scan(uint32_t t)
{
    stc_event_t ev = make_event(STC_EV_SCAN_REQUEST, t);
    return stc_game_input(&ev) == STC_OK ? 0 : 1;
}

static stc_event_t label_ev(stc_bin_t bin, float conf, bool abstain, uint32_t t)
{
    stc_event_t ev = make_event(STC_EV_LABEL, t);
    ev.u.label.bin = bin;
    ev.u.label.conf = conf;
    ev.u.label.abstain = abstain;
    ev.u.label.src = STC_SRC_STUB;
    return ev;
}

static stc_event_t drop_ev(stc_bin_t bin, uint32_t t)
{
    stc_event_t ev = make_event(STC_EV_DROP, t);
    ev.u.drop.bin = bin;
    ev.u.drop.dist_mm = 40;
    return ev;
}

static int continue_round(uint32_t t)
{
    stc_event_t ev = make_event(STC_EV_CONTINUE, t);
    return stc_game_input(&ev) == STC_OK ? 0 : 1;
}

int main(void)
{
    if (stc_game_init() != STC_OK) return 1;
    if (stc_game_start(0) != STC_OK) return 1;

    uint32_t t = 0;
    const stc_bin_t truth[5] = {
        STC_BIN_FOOD, STC_BIN_HAZARD, STC_BIN_OTHER,
        STC_BIN_RECYCLABLE, STC_BIN_FOOD,
    };
    const bool right[5] = {true, true, false, true, false};

    for (int r = 0; r < 5; ++r) {
        if (begin_scan(t)) { fprintf(stderr, "scan r%d\n", r); return 1; }
        stc_event_t l = label_ev(truth[r], 0.9f, false, t);
        if (stc_game_input(&l) != STC_OK) { fprintf(stderr, "label r%d\n", r); return 1; }
        t += 1500;
        stc_game_tick(t);
        t += 1500;
        stc_game_tick(t);
        const stc_bin_t wrong = (truth[r] == STC_BIN_OTHER)
                                    ? STC_BIN_FOOD
                                    : STC_BIN_OTHER;
        stc_event_t d = drop_ev(right[r] ? truth[r] : wrong, t);
        if (stc_game_input(&d) != STC_OK) { fprintf(stderr, "drop r%d\n", r); return 1; }
        t += 300;
        stc_game_tick(t);
        t += 1500;
        stc_game_tick(t);
        if (stc_game_snapshot().phase != STC_PH_LEARNING) {
            fprintf(stderr, "learning r%d\n", r);
            return 1;
        }
        if (continue_round(t)) { fprintf(stderr, "continue r%d\n", r); return 1; }
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

    /* All-wrong game: five wrong drops produce raw -25 but shown score stays 0. */
    if (stc_game_start(0) != STC_OK) return 1;
    t = 0;
    for (int r = 0; r < 5; ++r) {
        if (begin_scan(t)) return 1;
        stc_event_t l = label_ev(STC_BIN_FOOD, 0.9f, false, t);
        if (stc_game_input(&l) != STC_OK) return 1;
        t += 1500;
        stc_game_tick(t);
        t += 1500;
        stc_game_tick(t);
        stc_event_t d = drop_ev(STC_BIN_OTHER, t);
        if (stc_game_input(&d) != STC_OK) return 1;
        t += 300;
        stc_game_tick(t);
        t += 1500;
        stc_game_tick(t);
        if (continue_round(t)) return 1;
    }
    stc_result_t res2 = stc_game_result();
    if (res2.rounds_played != 5 || res2.wrong != 5) {
        fprintf(stderr, "all-wrong rounds=%u wrong=%u\n",
                (unsigned)res2.rounds_played, (unsigned)res2.wrong);
        return 1;
    }
    if (res2.score != 0) { fprintf(stderr, "floor score %d\n", res2.score); return 1; }
    if (res2.score_raw != -25) {
        fprintf(stderr, "raw %d\n", res2.score_raw);
        return 1;
    }

    /* Abstain: first low-confidence result requests one retry, second scores 0. */
    if (stc_game_start(0) != STC_OK) return 1;
    t = 0;
    if (begin_scan(t)) return 1;
    stc_event_t a1 = label_ev(STC_BIN_OTHER, 0.3f, true, t);
    if (stc_game_input(&a1) != STC_OK) return 1;
    if (stc_game_snapshot().phase != STC_PH_PROMPT) {
        fprintf(stderr, "no retry prompt\n");
        return 1;
    }
    if (begin_scan(100)) return 1;
    stc_event_t a2 = label_ev(STC_BIN_OTHER, 0.3f, true, 100);
    if (stc_game_input(&a2) != STC_OK) return 1;
    if (stc_game_snapshot().phase != STC_PH_LEARNING) {
        fprintf(stderr, "abstain phase\n");
        return 1;
    }
    if (stc_game_snapshot().truth != STC_BIN_COUNT) {
        fprintf(stderr, "abstain leaked top-1 bin=%d\n",
                (int)stc_game_snapshot().truth);
        return 1;
    }
    stc_result_t res3 = stc_game_result();
    if (res3.abstained != 1 || res3.score != 0) {
        fprintf(stderr, "abstain counting\n");
        return 1;
    }

    printf("test_game: 5 rounds, score 20, floor 0, abstain retry\n");
    return 0;
}
