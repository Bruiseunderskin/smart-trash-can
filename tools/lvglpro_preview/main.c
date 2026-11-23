/* Renders one stc_ui screen to a raw XRGB8888 frame.
 *
 * usage: preview <state> [ticks_ms] <out.raw>
 *
 * States: idle, scan_ready, capturing, classifying, result_ok, result_ok_2s,
 * result_ok_lid, result_retry, bin_choice, error, learning, summary, ask_ready,
 * ask_listening, ask_thinking, ask_answering, ask_offline, ask_asr_retry,
 * ask_countdown.
 */
#include "lvgl.h"
#include "stc_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 800
#define H 480

static uint16_t fb[W * H];

static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px_map) {
    const int w = lv_area_get_width(area);
    const int h = lv_area_get_height(area);
    const uint16_t *src = (const uint16_t *)px_map;
    for (int y = 0; y < h; ++y) {
        memcpy(fb + (size_t)(area->y1 + y) * W + area->x1,
               src + (size_t)y * w, (size_t)w * 2);
    }
    lv_display_flush_ready(d);
}

static void pump(int ms) {
    for (int t = 0; t < ms; t += 10) {
        lv_tick_inc(10);
        lv_timer_handler();
    }
}

static int dump(const char *path) {
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        perror(path);
        return 0;
    }
    fwrite(fb, 2, (size_t)W * H, f);
    fclose(f);
    return 1;
}

static stc_payload_t base_payload(stc_state_t st) {
    stc_payload_t p;
    memset(&p, 0, sizeof p);
    p.state = st;
    p.round = 3;
    p.rounds_total = 5;
    p.time_left_ms = 120000;
    p.time_total_ms = 180000;
    p.category_id = 1;
    p.confidence = 0.93f;
    p.candidates[0].category_id = 1;
    p.candidates[0].p = 0.93f;
    p.candidates[1].category_id = 0;
    p.candidates[1].p = 0.05f;
    p.score = 320;
    p.streak = 2;
    return p;
}

static void render_ask(stc_ui_t *ui, const char *state, int ticks,
                       const char *out) {
    static const char *const names[] = {
        "ready",    "listening", "thinking", "answering",
        "offline",  "asr_retry", "countdown", "no_answer", "goodbye",
    };
    stc_chat_payload_t cp;
    memset(&cp, 0, sizeof cp);
    cp.state = STC_CHAT_READY;
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); ++i) {
        if (strcmp(state + 4, names[i]) == 0) {
            cp.state = (stc_chat_state_t)i;
        }
    }
    stc_ui_show_ask(ui, &cp);
    pump(60 + ticks);
    printf("%s -> %s (layout=%s)\n", state, out,
           stc_ui_check_layout(ui) ? "PASS" : "FAIL");
    (void)dump(out);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <state> [ticks_ms] <out.raw>\n", argv[0]);
        return 2;
    }
    const char *state = argv[1];
    const int ticks = argc > 3 ? atoi(argv[2]) : 0;
    const char *out = argc > 3 ? argv[3] : argv[2];

    lv_init();
    lv_display_t *disp = lv_display_create(W, H);
    static uint16_t drawbuf[W * H];
    lv_display_set_buffers(disp, drawbuf, NULL, sizeof drawbuf,
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, W, H);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_screen_load(scr);

    stc_ui_t *ui = stc_ui_create(scr, NULL, NULL);
    if (ui == NULL) {
        fprintf(stderr, "stc_ui_create failed\n");
        return 3;
    }
    stc_ui_set_reduced_motion(ui, true);

    if (strncmp(state, "ask_", 4) == 0) {
        render_ask(ui, state, ticks, out);
        return 0;
    }

    const char *lookup = state;
    if (strcmp(state, "result_ok_lid") == 0 ||
        strcmp(state, "result_ok_2s") == 0) {
        lookup = "result_ok";
    }
    const stc_state_t target = stc_ui_state_from_string(lookup);
    stc_payload_t p = base_payload(target);
    p.lid_open = strstr(state, "lid") != NULL;
    if (target == STC_ERROR) {
        p.error_code = "MODEL_TIMEOUT";
    }
    if (target == STC_RESULT_RETRY) {
        p.category_id = -1;
        p.confidence = 0.0f;
    }
    if (target == STC_LEARNING) {
        p.learned_scanned_bin = 1;
        p.learned_dropped_bin = 1;
        p.learned_correct_bin = 1;
        p.learned_outcome = 1;
        p.learned_round_delta = 1;
    }

    /* Walk the legal transition path so strict validation accepts the target. */
    stc_state_t path[8];
    int n = 0;
    if (target == STC_ERROR) {
        path[n++] = STC_ERROR;
    } else if (target == STC_INTRO) {
        path[n++] = STC_IDLE;
        path[n++] = STC_INTRO;
    } else {
        path[n++] = STC_IDLE;
        if (target != STC_IDLE) path[n++] = STC_SCAN_READY;
        if (target != STC_IDLE && target != STC_SCAN_READY) {
            path[n++] = STC_CAPTURING;
        }
        if (target == STC_CLASSIFYING || target == STC_RESULT_OK ||
            target == STC_RESULT_RETRY || target == STC_BIN_CHOICE ||
            target == STC_LEARNING || target == STC_SUMMARY) {
            path[n++] = STC_CLASSIFYING;
        }
        if (target == STC_LEARNING || target == STC_SUMMARY) {
            path[n++] = STC_RESULT_OK;
        }
        if (target != STC_IDLE && target != STC_SCAN_READY &&
            target != STC_CAPTURING && target != STC_CLASSIFYING) {
            path[n++] = target;
        }
    }
    for (int i = 0; i < n; ++i) {
        p.state = path[i];
        if (!stc_ui_apply(ui, &p)) {
            fprintf(stderr, "apply rejected at %s for %s\n",
                    stc_ui_state_name(path[i]), state);
            return 4;
        }
    }
    pump(60 + ticks);

    printf("%s -> %s (layout=%s)\n", state, out,
           stc_ui_check_layout(ui) ? "PASS" : "FAIL");
    return dump(out) ? 0 : 5;
}
