/* Renders one stc_ui screen to a raw XRGB8888 frame.
 *
 * usage: preview <state> [ticks_ms] <out.raw>
 *
 * States: idle, scan_ready, capturing, classifying, result_ok, result_ok_2s,
 * result_ok_lid, result_retry, bin_choice, error, learning, summary, ask_ready,
 * ask_listening, ask_thinking, ask_answering, ask_offline, ask_asr_retry,
 * ask_countdown, ask_answering_long, ask_connecting, ask_provisioning.
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
static void json_string(FILE *f, const char *s) {
    fputc('"', f);
    if (s != NULL) {
        for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
            switch (*p) {
                case '"': fputs("\\\"", f); break;
                case '\\': fputs("\\\\", f); break;
                case '\n': fputs("\\n", f); break;
                case '\r': fputs("\\r", f); break;
                case '\t': fputs("\\t", f); break;
                default:
                    if (*p < 0x20) {
                        fprintf(f, "\\u%04x", *p);
                    } else {
                        fputc(*p, f);
                    }
                    break;
            }
        }
    }
    fputc('"', f);
}

static const char *widget_type(const lv_obj_t *obj) {
    if (lv_obj_check_type(obj, &lv_label_class)) return "label";
    if (lv_obj_check_type(obj, &lv_button_class)) return "button";
    if (lv_obj_check_type(obj, &lv_image_class)) return "image";
    return "object";
}

static const char *text_align_name(lv_text_align_t align) {
    switch (align) {
        case LV_TEXT_ALIGN_CENTER: return "center";
        case LV_TEXT_ALIGN_RIGHT: return "right";
        case LV_TEXT_ALIGN_AUTO: return "auto";
        case LV_TEXT_ALIGN_LEFT:
        default: return "left";
    }
}

static void dump_obj(FILE *f, lv_obj_t *obj, int *next_id) {
    const int id = (*next_id)++;
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    const lv_color_t bg = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
    const lv_color_t border = lv_obj_get_style_border_color(obj, LV_PART_MAIN);
    const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
    const lv_color_t text_color =
        lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    const uint32_t child_count = lv_obj_get_child_count(obj);

    fprintf(f,
            "{\"id\":\"o%d\",\"widget\":\"%s\",\"x\":%ld,\"y\":%ld,"
            "\"w\":%ld,\"h\":%ld,\"hidden\":%s,"
            "\"bg\":\"#%06lX\",\"bgOpacity\":%u,"
            "\"border\":\"#%06lX\",\"borderWidth\":%ld,"
            "\"radius\":%ld,\"opacity\":%u,"
            "\"textColor\":\"#%06lX\",\"fontLineHeight\":%ld,"
            "\"textAlign\":\"%s\"",
            id, widget_type(obj), (long)area.x1, (long)area.y1,
            (long)lv_area_get_width(&area), (long)lv_area_get_height(&area),
            lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) ? "true" : "false",
            (unsigned long)(lv_color_to_u32(bg) & 0xFFFFFFu),
            (unsigned)lv_obj_get_style_bg_opa(obj, LV_PART_MAIN),
            (unsigned long)(lv_color_to_u32(border) & 0xFFFFFFu),
            (long)lv_obj_get_style_border_width(obj, LV_PART_MAIN),
            (long)lv_obj_get_style_radius(obj, LV_PART_MAIN),
            (unsigned)lv_obj_get_style_opa(obj, LV_PART_MAIN),
            (unsigned long)(lv_color_to_u32(text_color) & 0xFFFFFFu),
            font != NULL ? (long)font->line_height : 0L,
            text_align_name(lv_obj_get_style_text_align(obj, LV_PART_MAIN)));

    if (lv_obj_check_type(obj, &lv_label_class)) {
        fputs(",\"text\":", f);
        json_string(f, lv_label_get_text(obj));
    }
    if (lv_obj_check_type(obj, &lv_image_class)) {
        fputs(",\"image\":true", f);
    }

    fputs(",\"children\":[", f);
    for (uint32_t i = 0; i < child_count; ++i) {
        if (i != 0) fputc(',', f);
        dump_obj(f, lv_obj_get_child(obj, i), next_id);
    }
    fputs("]}", f);
}

static void maybe_dump_layout(const char *state, int ticks, lv_obj_t *root) {
    const char *path = getenv("STC_PREVIEW_LAYOUT_PATH");
    if (path == NULL || path[0] == '\0') {
        return;
    }
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        perror(path);
        return;
    }
    int next_id = 0;
    fputs("{\"schemaVersion\":\"1.0\",\"state\":", f);
    json_string(f, state);
    fprintf(f,
            ",\"canvas\":{\"width\":%d,\"height\":%d,\"rotation\":0,"
            "\"colorDepth\":16,\"pixelFormat\":\"RGB565\"},"
            "\"previewTicksMs\":%d,\"root\":",
            W, H, ticks);
    dump_obj(f, root, &next_id);
    fputs("}\n", f);
    fclose(f);
}

static stc_payload_t base_payload(stc_state_t st) {
    stc_payload_t p;
    memset(&p, 0, sizeof p);
    p.state = st;
    p.round = 3;
    p.rounds_total = 5;
    p.time_left_ms = 5000;
    p.time_total_ms = 5000;
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
        "connecting", "provisioning",
    };
    stc_chat_payload_t cp;
    memset(&cp, 0, sizeof cp);
    cp.state = STC_CHAT_READY;
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); ++i) {
        if (strcmp(state + 4, names[i]) == 0) {
            cp.state = (stc_chat_state_t)i;
        }
    }
    static const char kLongAnswer[] =
        "答案會喺呢度顯示。我可以同你傾吓垃圾分類。"
        "答案會喺呢度顯示。我可以同你傾吓垃圾分類。"
        "答案會喺呢度顯示。我可以同你傾吓垃圾分類。"
        "答案會喺呢度顯示。我可以同你傾吓垃圾分類。";
    if (strcmp(state, "ask_answering_long") == 0) {
        cp.state = STC_CHAT_ANSWERING;
        cp.message = kLongAnswer;
    }
    stc_ui_show_ask(ui, &cp);
    pump(60 + ticks);
    printf("%s -> %s (layout=%s)\n", state, out,
           stc_ui_check_layout(ui) ? "PASS" : "FAIL");
    maybe_dump_layout(state, ticks, lv_screen_active());
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
    const char *reduced_motion = getenv("STC_PREVIEW_REDUCED_MOTION");
    stc_ui_set_reduced_motion(
        ui, !(reduced_motion != NULL && strcmp(reduced_motion, "0") == 0));

    if (strncmp(state, "ask_", 4) == 0) {
        render_ask(ui, state, ticks, out);
        return 0;
    }

    const char *lookup = state;
    if (strcmp(state, "scan_logout_confirm") == 0) {
        lookup = "scan_ready";
    } else if (strcmp(state, "result_ok_lid") == 0 ||
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
    if (strcmp(state, "scan_logout_confirm") == 0) {
        stc_ui_preview_open_logout(ui);
        pump(60);
    }

    printf("%s -> %s (layout=%s)\n", state, out,
           stc_ui_check_layout(ui) ? "PASS" : "FAIL");
    maybe_dump_layout(state, ticks, lv_screen_active());
    return dump(out) ? 0 : 5;
}
