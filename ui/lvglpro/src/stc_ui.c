#include "../include/stc_ui.h"

#include "stc_lvglpro_assets.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "src/misc/lv_text_private.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define COLOR_BG          0xFBF7EC
#define COLOR_SURFACE     0xFFFFFF
#define COLOR_FG          0x14342B
#define COLOR_MUTED       0x5A6B62
#define COLOR_ERROR       0xC2532F
#define COLOR_BRAND       0x14342B
#define COLOR_ON_BRAND    0xFBF7EC
#define COLOR_ACCENT      0x7FD8B0
#define COLOR_ON_ACCENT   0x14342B
#define COLOR_BRAND_LIGHT 0x2C6B4F
#define COLOR_CELEBRATE   0xDCE45A
#define COLOR_LINE        0xE2D9C6

#define FONT_CAPTION   (&stc_lp_font_tc_medium_26)
#define FONT_HINT      (&stc_lp_font_tc_medium_32)
#define FONT_BODY      (&stc_lp_font_tc_medium_32)
#define FONT_BODY_BOLD (&stc_lp_font_tc_bold_32)
#define FONT_SUBTITLE  (&stc_lp_font_tc_medium_40)
#define FONT_H2        (&stc_lp_font_tc_bold_40)
#define FONT_H1        (&stc_lp_font_tc_bold_56)
#define FONT_CAT       (&stc_lp_font_tc_bold_112)
extern const lv_font_t stc_lp_font_tc_bold_112;
#define FONT_NUM_BODY  (&stc_lp_font_num_bold_32)
#define FONT_NUM_H2    (&stc_lp_font_num_bold_40)
#define FONT_NUM_SCORE (&stc_lp_font_num_bold_80)

/* Figma space variables (design/tokens.json). */
#define SP_XS 8
#define SP_SM 16
#define SP_MD 24
#define SP_LG 40
#define SP_XL 64
#define OUTER SP_MD
#define CARD_PAD SP_MD
#define COMPACT_PAD SP_SM
#define ICON_GAP SP_MD
#define SECTION_GAP SP_SM
#define TEXT_GAP SP_XS
#define CONTROL_H SP_XL
#define PAGE_W (800 - 2 * OUTER)

#define MAX_ACTIONS 24

typedef struct {
    stc_ui_t *ui;
    stc_action_t action;
    int category;
    bool retry;
} ActionBinding;

struct stc_ui {
    lv_obj_t *container;
    lv_obj_t *camera;
    lv_obj_t *camera_image;
    lv_obj_t *scan;
    lv_obj_t *header_status;
    lv_obj_t *score;
    lv_obj_t *streak;
    lv_obj_t *candidate_icon[2];
    lv_obj_t *candidate_name[2];
    lv_obj_t *candidate_prob[2];
    const lv_image_dsc_t *original;
    const lv_image_dsc_t *cutout;
    ActionBinding actions[MAX_ACTIONS];
    unsigned action_count;
    stc_retry_cb retry;
    stc_action_cb action;
    void *user;
    uint32_t score_value;
    uint32_t streak_value;
    uint8_t round;
    uint8_t rounds_total;
    int category_value;
    float confidence_value;
    bool lid_open;
    bool reduced;
    bool acknowledged;
    lv_timer_t *auto_timer;
    lv_obj_t *time_bar;
    uint8_t auto_left;
    stc_action_t auto_action;
    bool auto_armed;
    bool auto_done;
    uint32_t time_left_ms;
    uint32_t time_total_ms;
    uint8_t debug_flags;
    int learned_scanned_bin;
    int learned_dropped_bin;
    int learned_correct_bin;
    int learned_outcome;
    int learned_round_delta;
    stc_state_t state;
    stc_chat_ui_t *ask;
    stc_chat_payload_t ask_payload;
    bool ask_valid;
    bool intro_seen;
};

static const char *state_names[] = {
    "idle", "capturing", "classifying", "result_ok", "result_retry",
    "error", "category_guide", "learning", "summary", "scan_ready",
    "bin_choice", "ask", "intro",
};

static const uint32_t category_colors[STC_CATEGORY_COUNT] = {
    0x2455C0, 0x176640, 0x9E3F20, 0x4E5A62,
};


/* The bin cards use the short words from Figma 110:848 / 124:214, not the
 * longer category labels ("可回收", not "可回收物"). */
static const char *const bin_names[STC_CATEGORY_COUNT] = {
    "可回收", "廚餘", "有害", "其他",
};

/* 96 px sources so the result page shows the glyph 1:1 instead of a blurry
 * 2x upscale of the 48 px asset. Smaller slots downscale, which stays sharp. */
extern const lv_image_dsc_t stc_lp_img_cat_0_96;
extern const lv_image_dsc_t stc_lp_img_cat_1_96;
extern const lv_image_dsc_t stc_lp_img_cat_2_96;
extern const lv_image_dsc_t stc_lp_img_cat_3_96;

static const lv_image_dsc_t *const category_images[STC_CATEGORY_COUNT] = {
    &stc_lp_img_cat_0_96,
    &stc_lp_img_cat_1_96,
    &stc_lp_img_cat_2_96,
    &stc_lp_img_cat_3_96,
};


static const char *const learning_tips[STC_CATEGORY_COUNT] = {
    "請先清空、清洗並壓扁，再放入回收桶。",
    "請先瀝乾水分，包裝另作分類。",
    "請交由大人處理，送到專門回收點。",
    "紙巾不能當廢紙回收，先減少使用更好。",
};

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h,
                     uint32_t bg, int radius, int border_width,
                     uint32_t border_color) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, border_width, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(border_color), 0);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       int w, int h, const lv_font_t *font, uint32_t color,
                       lv_text_align_t align) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    if (w < 0) {
        lv_obj_set_width(obj, LV_SIZE_CONTENT);
    } else {
        lv_obj_set_width(obj, w);
    }
    if (h < 0) {
        lv_obj_set_height(obj, LV_SIZE_CONTENT);
    } else {
        lv_obj_set_height(obj, h);
    }
    lv_label_set_text(obj, text != NULL ? text : "");
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(obj, align, 0);
    lv_obj_set_style_text_line_space(obj, 2, 0);
    return obj;
}

static lv_obj_t *image(lv_obj_t *parent, const lv_image_dsc_t *source,
                       int x, int y, int w, int h) {
    lv_obj_t *obj = lv_image_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_image_set_src(obj, source);
    if (source != NULL && source->header.w > 0 && source->header.h > 0) {
        const uint32_t scale_x = (uint32_t)w * 256u / source->header.w;
        const uint32_t scale_y = (uint32_t)h * 256u / source->header.h;
        lv_image_set_scale(obj, scale_x < scale_y ? scale_x : scale_y);
    }
    return obj;
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y,
                        int w, int h, bool primary) {
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 16, 0);
    lv_obj_set_style_bg_color(
        obj, lv_color_hex(primary ? COLOR_ACCENT : COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, primary ? 0 : 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xBFBFBF), 0);
    lv_obj_set_style_bg_opa(obj, 210, LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(obj, 3, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(obj, lv_color_hex(COLOR_FG),
                                   LV_STATE_FOCUSED);
    lv_obj_t *text_label =
        label(obj, text, 0, 0, w, h, FONT_BODY_BOLD,
              primary ? COLOR_ON_ACCENT : COLOR_FG, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_height(text_label, LV_SIZE_CONTENT);
    lv_obj_center(text_label);
    return obj;
}

static void topbar(stc_ui_t *ui) {
    /* Full-bleed bar, matching design/layout.json: 800x72 at 0,0. */
    lv_obj_t *bar = box(ui->container, 0, 0, 800, 72, COLOR_BRAND, 0, 0,
                        COLOR_BRAND);
    label(bar, "分類小隊", 24, 16, 352, 44, FONT_BODY_BOLD, COLOR_ON_BRAND,
          LV_TEXT_ALIGN_LEFT);
    image(bar, &stc_lp_img_flame, 744, 20, 32, 32);
    ui->header_status =
        label(bar, "", 424, 16, 216, 44, FONT_BODY_BOLD, COLOR_ON_BRAND,
              LV_TEXT_ALIGN_RIGHT);
}

static void page_background(stc_ui_t *ui) {
    lv_obj_set_style_bg_color(ui->container, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(ui->container, LV_OPA_COVER, 0);
}

static void round_time_bar(stc_ui_t *ui);
static void auto_advance(stc_ui_t *ui, stc_action_t action, bool arm,
                         uint8_t ticks);
static void action_event(lv_event_t *event) {
    ActionBinding *binding = lv_event_get_user_data(event);
    if (binding == NULL || binding->ui == NULL ||
        lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    stc_ui_t *ui = binding->ui;
    if (ui->state == STC_ERROR && binding->retry) {
        stc_ui_retry(ui);
        return;
    }
    if (ui->state == STC_ERROR) {
        /* HOME must acknowledge the error before the controller can apply
         * IDLE. Retry uses stc_ui_retry(), which performs the same step and
         * then invokes the retry callback. */
        ui->acknowledged = true;
    }
    if (binding->retry) {
        stc_ui_retry(ui);
        return;
    }
    if (ui->action == NULL) {
        return;
    }
    ui->action(binding->action, binding->category, ui->user);
}

static void bind_action(stc_ui_t *ui, lv_obj_t *obj, stc_action_t action,
                        int category, bool retry) {
    if (ui->action_count >= MAX_ACTIONS) {
        return;
    }
    ActionBinding *binding = &ui->actions[ui->action_count++];
    binding->ui = ui;
    binding->action = action;
    binding->category = category;
    binding->retry = retry;
    lv_obj_add_event_cb(obj, action_event, LV_EVENT_CLICKED, binding);
}

static void set_header(stc_ui_t *ui) {
    if (ui->header_status == NULL) {
        return;
    }
    char text[64];
    if (ui->state == STC_IDLE || ui->state == STC_INTRO) {
        /* Figma state/idle (5:38): the bar says 準備好 before a round starts;
         * a score only exists after the first round. */
        snprintf(text, sizeof(text), "準備好");
    } else if (ui->rounds_total > 0) {
        unsigned current = ui->round == 0 ? 1u : ui->round;
        if (current > ui->rounds_total) {
            current = ui->rounds_total;
        }
        snprintf(text, sizeof(text), "第 %u / %u 回合", current,
                 (unsigned)ui->rounds_total);
    } else {
        snprintf(text, sizeof(text), "準備好");
    }
    lv_label_set_text(ui->header_status, text);
}

static void set_score(lv_obj_t *obj, uint32_t value, const lv_font_t *large,
                      const lv_font_t *small) {
    if (obj == NULL) {
        return;
    }
    char text[32];
    snprintf(text, sizeof(text), "%lu", (unsigned long)value);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_font(obj, value > 99999 ? small : large, 0);
}

static void scan_opacity(void *obj, int32_t value) {
    lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void container_deleted(lv_event_t *event) {
    stc_ui_t *ui = lv_event_get_user_data(event);
    if (ui == NULL) {
        return;
    }
    if (ui->scan != NULL) {
        lv_anim_delete(ui->scan, scan_opacity);
    }
    if (ui->auto_timer != NULL) {
        lv_timer_del(ui->auto_timer);
        ui->auto_timer = NULL;
    }
    ui->container = NULL;
    ui->camera = NULL;
    ui->camera_image = NULL;
    ui->scan = NULL;
    ui->time_bar = NULL;
}

static void animate_scan(stc_ui_t *ui) {
    if (ui->scan == NULL) {
        return;
    }
    lv_anim_delete(ui->scan, scan_opacity);
    lv_obj_set_style_bg_opa(ui->scan, 80, 0);
    if (ui->reduced) {
        return;
    }
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, ui->scan);
    lv_anim_set_exec_cb(&animation, scan_opacity);
    lv_anim_set_values(&animation, 60, 180);
    lv_anim_set_duration(&animation, 260);
    lv_anim_set_reverse_duration(&animation, 260);
    lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&animation);
}

static void camera_update(stc_ui_t *ui) {
    if (ui->camera == NULL) {
        return;
    }
    lv_obj_update_layout(ui->camera);
    const lv_image_dsc_t *source =
        (ui->state == STC_RESULT_OK && ui->cutout != NULL) ? ui->cutout
                                                          : ui->original;
    lv_obj_clean(ui->camera);
    ui->camera_image = NULL;
    if (source != NULL && source->header.w > 0 && source->header.h > 0) {
        const int w = lv_obj_get_width(ui->camera);
        const int h = lv_obj_get_height(ui->camera);
        ui->camera_image = image(ui->camera, source, 0, 0, w, h);
        lv_obj_center(ui->camera_image);
        return;
    }
    image(ui->camera, &stc_lp_img_camera, 336, 20, 48, 48);
    /* The placeholder must not contradict the status line underneath it. */
    const char *placeholder = "等待相機畫面";
    if (ui->state == STC_CLASSIFYING) {
        placeholder = "相機畫面";
    } else if (ui->state == STC_CAPTURING) {
        placeholder = "相機畫面";
    }
    label(ui->camera, placeholder, 32, 96, 592, 44, FONT_BODY,
          COLOR_ON_BRAND, LV_TEXT_ALIGN_CENTER);
}

static lv_obj_t *camera_card(stc_ui_t *ui, const char *title,
                             const char *subtitle) {
    lv_obj_t *card = box(ui->container, OUTER, 104, PAGE_W, 360,
                         COLOR_BRAND_LIGHT, 16, 0, COLOR_BRAND_LIGHT);
    ui->camera = box(card, COMPACT_PAD, COMPACT_PAD, PAGE_W - 2 * COMPACT_PAD,
                     244, COLOR_BRAND, 12, 3, COLOR_ON_BRAND);
    lv_obj_set_style_border_opa(ui->camera, 92, 0);
    label(card, title, 32, 270, 688, 44, FONT_H2, COLOR_ON_BRAND,
          LV_TEXT_ALIGN_CENTER);
    label(card, subtitle, 32, 314, 688, 26, FONT_CAPTION, 0xD5E5DC,
          LV_TEXT_ALIGN_CENTER);

    return card;
}

static void home_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    /* First page: title only, then 開始遊戲 / 問綠豆. */
    label(ui->container, "一起做分類小達人", OUTER, 152, PAGE_W, 96, FONT_H1,
          COLOR_FG, LV_TEXT_ALIGN_CENTER);
    label(ui->container, "5 回合 · 認識垃圾分類", OUTER, 252, PAGE_W, 48,
          FONT_SUBTITLE, COLOR_MUTED, LV_TEXT_ALIGN_CENTER);
    image(ui->container, &stc_lp_img_mascot_idle, 688, 128, 64, 64);
    lv_obj_t *start =
        button(ui->container, "開始遊戲", OUTER, 392, 364, CONTROL_H, true);
    bind_action(ui, start, STC_ACTION_START, -1, false);
    lv_obj_t *ask =
        button(ui->container, "問綠豆", 412, 392, 364, CONTROL_H, false);
    bind_action(ui, ask, STC_ACTION_ASK, -1, false);
}

static void intro_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    /* Figma state/03-intro (110:780): three numbered steps, then start. */
    static const char *const numbers[3] = {"1", "2", "3"};
    static const char *const titles[3] = {
        "放在鏡頭前", "投入垃圾桶", "等蓋關閉",
    };
    for (int i = 0; i < 3; ++i) {
        const int card_w = 240;
        const int x = OUTER + i * (card_w + SECTION_GAP);
        box(ui->container, x, 144, card_w, 188, COLOR_SURFACE, 16, 0,
            COLOR_SURFACE);
        lv_obj_t *dot =
            box(ui->container, x + 98, 172, 44, 44, COLOR_ACCENT,
                LV_RADIUS_CIRCLE, 0, COLOR_ACCENT);
        lv_obj_t *num =
            label(ui->container, numbers[i], 0, 0, -1, -1, FONT_BODY_BOLD,
                  COLOR_ON_ACCENT, LV_TEXT_ALIGN_CENTER);
        lv_obj_align_to(num, dot, LV_ALIGN_CENTER, 0, 0);
        label(ui->container, titles[i], x + TEXT_GAP, 248, card_w - 2 * TEXT_GAP,
              56, FONT_H2, COLOR_FG, LV_TEXT_ALIGN_CENTER);
    }
    lv_obj_t *start =
        button(ui->container, "開始掃描", OUTER, 392, PAGE_W, CONTROL_H, true);
    bind_action(ui, start, STC_ACTION_SCAN, -1, false);
}

static void scan_ready_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    /* Five seconds gives a child enough time to place the whole card in view.
     * One tap per session stays on the home screen. */
    round_time_bar(ui);
    auto_advance(ui, STC_ACTION_SCAN, true, 50);
    lv_obj_t *card =
        box(ui->container, OUTER, 112, PAGE_W, 340, COLOR_BRAND_LIGHT, 16, 0,
            COLOR_BRAND_LIGHT);
    ui->camera =
        box(card, COMPACT_PAD, COMPACT_PAD, PAGE_W - 2 * COMPACT_PAD, 244,
            COLOR_BRAND, 12, 3, COLOR_ON_BRAND);
    lv_obj_set_style_border_opa(ui->camera, 92, 0);
    label(card, "確認卡牌完整入框", 32, 270, 688, 44, FONT_H2,
          COLOR_ON_BRAND, LV_TEXT_ALIGN_CENTER);
    label(card, "5 秒後自動掃描", 32, 314, 688, 26, FONT_CAPTION,
          0xD5E5DC,
          LV_TEXT_ALIGN_CENTER);
}

static void capturing_screen(stc_ui_t *ui, stc_state_t state) {
    page_background(ui);
    topbar(ui);
    round_time_bar(ui);
    /* Whole screen is the camera: no back button during capture. */
    camera_card(ui,
                state == STC_CAPTURING ? "正在辨識" : "整理緊結果",
                "保持卡牌完整喺鏡頭框內");
    animate_scan(ui);
}

static void retry_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    label(ui->container, "AI 都唔肯定", 32, 112, 736, 72, FONT_H1, COLOR_FG,
          LV_TEXT_ALIGN_LEFT);
    label(ui->container, "請揀最接近嘅答案，或者重新掃描", 32, 192, 736, 32,
          FONT_CAPTION, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
    for (int i = 0; i < 2; ++i) {
        const int x = i == 0 ? 32 : 408;
        lv_obj_t *card = box(ui->container, x, 242, 360, 104, COLOR_SURFACE,
                             16, 2, 0xBFBFBF);
        ui->candidate_icon[i] =
            image(card, category_images[0], 20, 24, 56, 56);
        ui->candidate_name[i] =
            label(card, stc_category_label(0), 92, 16, 164, 44,
                  FONT_BODY_BOLD, COLOR_FG, LV_TEXT_ALIGN_LEFT);
        ui->candidate_prob[i] =
            label(card, "0%", 260, 34, 76, 36, FONT_NUM_BODY, COLOR_MUTED,
                  LV_TEXT_ALIGN_RIGHT);
    }
    lv_obj_t *retry =
        button(ui->container, "重新掃描", OUTER, 392, 364, CONTROL_H, true);
    bind_action(ui, retry, STC_ACTION_START, -1, false);
    lv_obj_t *home =
        button(ui->container, "返回主頁", 412, 392, 364, CONTROL_H, false);
    bind_action(ui, home, STC_ACTION_HOME, -1, true);
}

/* The thin line under the top bar is the active round step's time window. It
 * is not a round-position indicator; the exact "第 N / 5 回合" text stays in
 * the top bar. Keep the bar at 1000 internal units so a 100 ms UI tick makes a
 * clearly visible decrement without controller rendering at frame rate. */
#define TIME_BAR_RANGE 1000

static bool round_is_live(stc_state_t state) {
    return state == STC_SCAN_READY || state == STC_CAPTURING ||
           state == STC_CLASSIFYING || state == STC_RESULT_OK ||
           state == STC_BIN_CHOICE || state == STC_LEARNING;
}

static void update_time_bar(stc_ui_t *ui) {
    if (ui == NULL || ui->time_bar == NULL || ui->time_total_ms == 0) {
        return;
    }
    uint32_t left = ui->time_left_ms;
    if (left > ui->time_total_ms) {
        left = ui->time_total_ms;
    }
    const int32_t value =
        (int32_t)((uint64_t)TIME_BAR_RANGE * left / ui->time_total_ms);
    lv_bar_set_value(ui->time_bar, value, LV_ANIM_OFF);
}

static void round_time_bar(stc_ui_t *ui) {
    ui->time_bar = lv_bar_create(ui->container);
    lv_obj_remove_style_all(ui->time_bar);
    lv_obj_set_pos(ui->time_bar, 32, 88);
    lv_obj_set_size(ui->time_bar, 736, 8);
    lv_bar_set_range(ui->time_bar, 0, TIME_BAR_RANGE);
    lv_obj_set_style_bg_color(ui->time_bar, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_bg_opa(ui->time_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ui->time_bar, lv_color_hex(COLOR_ACCENT),
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui->time_bar, LV_OPA_COVER,
                            LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui->time_bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(ui->time_bar, LV_RADIUS_CIRCLE,
                            LV_PART_INDICATOR);
    update_time_bar(ui);
}

/* Round exit -------------------------------------------------------------
 * The result screen is the round boundary: one 752x88 primary action, plus a
 * three-second auto-action. The visible top bar is the separate game clock and
 * keeps shrinking through the round. The timer is cancelled from clear_screen(),
 * so every state change (including an error or a retry) kills it; a lid that is
 * open means the card is still on its way into the bin, so the auto-action does
 * not start until the controller reports the lid closed.
 */
#define AUTO_ADVANCE_TICKS 30 /* 30 x 100 ms */

static void auto_disarm(stc_ui_t *ui) {
    ui->auto_armed = false;
    ui->auto_left = 0;
}

/* One persistent 100 ms timer. It advances the visible round clock on every
 * live game screen and separately emits the scan/result auto actions. Nothing
 * is deleted from inside the callback, so a state change during a tick cannot
 * free the timer that is running. */
static void auto_tick(lv_timer_t *timer) {
    stc_ui_t *ui = (stc_ui_t *)lv_timer_get_user_data(timer);
    if (ui == NULL) {
        return;
    }
    if (round_is_live(ui->state) && ui->time_total_ms > 0 &&
        ui->time_left_ms > 0) {
        ui->time_left_ms =
            ui->time_left_ms > 100 ? ui->time_left_ms - 100 : 0;
        update_time_bar(ui);
    }
    if (!ui->auto_armed) {
        return;
    }
    if ((ui->state != STC_RESULT_OK && ui->state != STC_SCAN_READY) ||
        ui->auto_done) {
        auto_disarm(ui);
        return;
    }
    if (ui->auto_left > 0) {
        ui->auto_left--;
    }
    if (ui->auto_left == 0) {
        ui->auto_done = true;
        auto_disarm(ui);
        if (ui->action != NULL) {
            ui->action(ui->auto_action, -1, ui->user);
        }
    }
}

static bool result_is_last_round(const stc_ui_t *ui) {
    return ui->rounds_total > 0 && ui->round >= ui->rounds_total;
}

/* Arm an automatic action without coupling it to the visible game clock.
 * The result page may deliberately hold while the lid/card is still moving. */
static void auto_advance(stc_ui_t *ui, stc_action_t action, bool arm,
                         uint8_t ticks) {
    ui->auto_action = action;
    if (arm) {
        ui->auto_left = ticks;
        ui->auto_armed = true;
        ui->auto_done = false;
    }
}

static void bin_card(stc_ui_t *ui, int index) {
    const int card_w = 166;
    const int gap = (PAGE_W - STC_CATEGORY_COUNT * card_w) /
                    (STC_CATEGORY_COUNT - 1);
    const int x = OUTER + index * (card_w + gap);
    const uint32_t accent = category_colors[index];
    lv_obj_t *card = lv_obj_create(ui->container);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, x, 216);
    lv_obj_set_size(card, card_w, 248);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(accent), 0);
    image(card, category_images[index], (card_w - 88) / 2, 40, 88, 88);
    label(card, bin_names[index], 0, 172, card_w, 52, FONT_H2, COLOR_FG,
          LV_TEXT_ALIGN_CENTER);
}

static void open_bins_screen(stc_ui_t *ui, const char *title) {
    page_background(ui);
    topbar(ui);
    round_time_bar(ui);
    /* Both pre-drop pages share this exact layout. Only the headline changes;
     * the AI category stays hidden until the S3 hand reports the drop. */
    label(ui->container, title, 32, 128, 640, 64, FONT_H1, COLOR_FG,
          LV_TEXT_ALIGN_LEFT);
    image(ui->container, &stc_lp_img_mascot_happy, 704, 128, 64, 64);
    for (int i = 0; i < STC_CATEGORY_COUNT; ++i) {
        bin_card(ui, i);
    }
}

static void result_screen(stc_ui_t *ui) {
    const bool last = result_is_last_round(ui);
    const bool waiting = ui->lid_open;
    auto_advance(ui, STC_ACTION_NEXT, !waiting && !last, AUTO_ADVANCE_TICKS);
    open_bins_screen(ui, "正在開啟桶蓋");
}

static void bin_choice_screen(stc_ui_t *ui) {
    open_bins_screen(ui, "請將卡牌放入垃圾桶");
}

static void error_screen(stc_ui_t *ui, const char *code) {
    page_background(ui);
    topbar(ui);
    const bool hand = code != NULL &&
                      (strncmp(code, "HAND_", 5) == 0 ||
                       strncmp(code, "ESPNOW", 6) == 0);
    const bool close = code != NULL &&
                       (strstr(code, "CLOSE") != NULL ||
                        strstr(code, "NACK") != NULL);
    const bool no_object =
        code != NULL &&
        (strstr(code, "NO_OBJECT") != NULL || strstr(code, "VISION") != NULL);
    const bool camera =
        code != NULL && strcmp(code, "CAMERA_OFFLINE") == 0;
    const bool transition =
        code != NULL && strstr(code, "INVALID_") != NULL;
    const char *title =
        hand ? (close ? "桶蓋未能關閉" : "桶蓋未能開啟")
             : (no_object ? "未偵測到物件"
                          : (camera ? "相機未就緒"
                                    : (transition ? "畫面未能同步"
                                                  : "暫時未能辨識")));
    const char *hint =
        hand ? "請確認 S3 手部連線及 ESP-NOW，再按重試"
             : (no_object
                    ? "請將卡牌完整放入鏡頭框內，再按重試"
                    : (camera ? "請檢查相機排線，再按重試"
                              : (transition
                                     ? "請按重試返回遊戲，重新開始本回合"
                                     : "請檢查相機、模型及網絡，再按重試")));
    lv_obj_t *headline =
        label(ui->container, title, 32, 112, 736, 68,
              strlen(title) > 7 ? FONT_H2 : FONT_H1, COLOR_FG,
              LV_TEXT_ALIGN_LEFT);
    (void)headline;
    label(ui->container, hint, 32, 184, 736, 56, FONT_BODY, COLOR_MUTED,
          LV_TEXT_ALIGN_LEFT);
    image(ui->container,
          no_object ? &stc_lp_img_no_object : &stc_lp_img_camera, 368, 248,
          56, 56);
    /* The code is a developer string: show it only when a debug flag is on. */
    if ((ui->debug_flags != 0) && code != NULL && code[0] != '\0') {
        label(ui->container, code, 32, 314, 736, 26, FONT_CAPTION, COLOR_MUTED,
              LV_TEXT_ALIGN_LEFT);
    }
    lv_obj_t *retry =
        button(ui->container, "重試", OUTER, 392, PAGE_W, CONTROL_H, true);
    bind_action(ui, retry, STC_ACTION_HOME, -1, true);
}

/* 綠豆. The chat module draws the whole 800x480 canvas, so it is hosted as a
 * sibling of the game screens and recreated for every STC_ASK render. */
static void ask_action(stc_chat_action_t action, void *user) {
    stc_ui_t *ui = (stc_ui_t *)user;
    if (ui == NULL || ui->action == NULL) {
        return;
    }
    ui->action(action == STC_CHAT_ACTION_BACK ? STC_ACTION_HOME : STC_ACTION_ASK,
               -1, ui->user);
}

static void ask_screen(stc_ui_t *ui) {
    if (ui->ask == NULL) {
        ui->ask = stc_chat_ui_create(ui->container, ask_action, ui);
    }
    if (ui->ask == NULL) {
        error_screen(ui, "ASK_UI");
        return;
    }
    const stc_chat_payload_t idle = {.state = STC_CHAT_READY};
    (void)stc_chat_ui_apply(ui->ask, ui->ask_valid ? &ui->ask_payload : &idle);
}



static const char *outcome_text(int outcome) {
    switch (outcome) {
        case 1:
            return "答對";
        case 2:
            return "答錯";
        case 3:
            return "未投放";
        case 4:
            return "兩次拒判";
        case 5:
            return "未掃描";
        default:
            return "已完成";
    }
}

static void learning_screen(stc_ui_t *ui, int category) {
    page_background(ui);
    topbar(ui);
    round_time_bar(ui);
    if (ui->learned_correct_bin >= 0 &&
        ui->learned_correct_bin < STC_CATEGORY_COUNT) {
        category = ui->learned_correct_bin;
    }
    const bool has_category =
        category >= 0 && category < STC_CATEGORY_COUNT;
    box(ui->container, 32, 116, 88, 88, COLOR_SURFACE, LV_RADIUS_CIRCLE, 0,
        COLOR_SURFACE);
    image(ui->container,
          has_category ? category_images[category]
                       : &stc_lp_img_mascot_puzzled,
          48, 132, 56, 56);
    label(ui->container,
          has_category ? stc_category_label(category) : "未能分類",
          152, 128, 384, 64,
          FONT_H1, COLOR_FG, LV_TEXT_ALIGN_LEFT);
    label(ui->container, has_category ? "正確類別" : "本回合",
          536, 148, 232, 32, FONT_CAPTION,
          has_category ? category_colors[category] : COLOR_MUTED,
          LV_TEXT_ALIGN_RIGHT);
    lv_obj_t *info_card =
        box(ui->container, 32, 204, 736, 100, COLOR_SURFACE, 16, 0,
            COLOR_SURFACE);
    char result_line[96];
    if (ui->learned_scanned_bin >= 0 &&
        ui->learned_scanned_bin < STC_CATEGORY_COUNT) {
        if (ui->learned_dropped_bin >= 0 &&
            ui->learned_dropped_bin < STC_CATEGORY_COUNT) {
            snprintf(result_line, sizeof(result_line), "掃描：%s · 實際：%s",
                     stc_category_label(ui->learned_scanned_bin),
                     stc_category_label(ui->learned_dropped_bin));
        } else {
            snprintf(result_line, sizeof(result_line),
                     "掃描：%s · 實際：未投放",
                     stc_category_label(ui->learned_scanned_bin));
        }
    } else {
        snprintf(result_line, sizeof(result_line), "本回合未能得出類別");
    }
    label(info_card, result_line, 20, 6, 696, 38, FONT_HINT, COLOR_FG,
          LV_TEXT_ALIGN_LEFT);
    char outcome_line[96];
    if (has_category) {
        snprintf(outcome_line, sizeof(outcome_line), "正確：%s · %s %+d 分",
                 stc_category_label(category),
                 outcome_text(ui->learned_outcome),
                 ui->learned_round_delta);
    } else {
        snprintf(outcome_line, sizeof(outcome_line), "正確：未能判斷 · %s %+d 分",
                 outcome_text(ui->learned_outcome),
                 ui->learned_round_delta);
    }
    label(info_card, outcome_line, 20, 50, 696, 36, FONT_HINT,
          COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
    label(ui->container,
          has_category ? learning_tips[category]
                       : "請將卡牌完整放進鏡頭框，然後重新掃描。",
          32, 310, 736, 42,
          FONT_CAPTION, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *next =
        button(ui->container, "下一回合", OUTER, 392, PAGE_W, CONTROL_H, true);
    bind_action(ui, next, STC_ACTION_NEXT, -1, false);
}

static void summary_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    lv_obj_t *score_card =
        box(ui->container, 32, 124, 280, 236, COLOR_CELEBRATE, 16, 0,
            COLOR_CELEBRATE);
    label(score_card, "總分", 24, 18, 232, 54, FONT_H2, COLOR_FG,
          LV_TEXT_ALIGN_LEFT);
    ui->score =
        label(score_card, "0", 24, 76, 232, 92, FONT_NUM_SCORE, COLOR_FG,
              LV_TEXT_ALIGN_LEFT);
    box(score_card, 24, 180, 220, 40, COLOR_SURFACE, LV_RADIUS_CIRCLE, 2,
        0x2455C0);
    label(score_card, "分類小達人", 36, 180, 196, 40, FONT_BODY_BOLD,
          0x2455C0, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *stats =
        box(ui->container, 344, 124, 424, 236, COLOR_SURFACE, 16, 0,
            COLOR_SURFACE);
    char round_text[48];
    if (ui->rounds_total > 0) {
        const unsigned completed =
            ui->round == 0
                ? 1u
                : (ui->round < ui->rounds_total ? ui->round
                                                 : ui->rounds_total);
        snprintf(round_text, sizeof(round_text), "完成 %u / %u 回合",
                 completed,
                 (unsigned)ui->rounds_total);
    } else {
        snprintf(round_text, sizeof(round_text), "完成 5 回合");
    }
    label(stats, round_text, 24, 24, 260, 44, FONT_BODY_BOLD, COLOR_FG,
          LV_TEXT_ALIGN_LEFT);
    ui->streak =
        label(stats, "最高連續答對 ×0", 24, 92, 376, 44, FONT_BODY_BOLD,
              COLOR_FG, LV_TEXT_ALIGN_LEFT);
    label(stats, "記住：先分類，再投放", 24, 160, 376, 40, FONT_HINT,
          COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *again =
        button(ui->container, "再玩一次", OUTER, 392, 364, CONTROL_H, true);
    bind_action(ui, again, STC_ACTION_START, -1, false);
    lv_obj_t *ask =
        button(ui->container, "問綠豆", 412, 392, 364, CONTROL_H, false);
    bind_action(ui, ask, STC_ACTION_ASK, -1, false);
    image(ui->container, &stc_lp_img_mascot_happy, 664, 136, 72, 72);
}

static void clear_screen(stc_ui_t *ui) {
    auto_disarm(ui);
    if (ui->ask != NULL) {
        stc_chat_ui_destroy(ui->ask);
        ui->ask = NULL;
    }
    if (ui->scan != NULL) {
        lv_anim_delete(ui->scan, scan_opacity);
    }
    lv_obj_clean(ui->container);
    ui->camera = NULL;
    ui->camera_image = NULL;
    ui->scan = NULL;
    ui->time_bar = NULL;
    ui->header_status = NULL;
    ui->score = NULL;
    ui->streak = NULL;
    ui->candidate_icon[0] = NULL;
    ui->candidate_icon[1] = NULL;
    ui->candidate_name[0] = NULL;
    ui->candidate_name[1] = NULL;
    ui->candidate_prob[0] = NULL;
    ui->candidate_prob[1] = NULL;
    ui->action_count = 0;
    memset(ui->actions, 0, sizeof(ui->actions));
}

static void render(stc_ui_t *ui, stc_state_t state, const char *error_code) {
    clear_screen(ui);
    ui->state = state;
    switch (state) {
        case STC_IDLE:
            home_screen(ui);
            break;
        case STC_SCAN_READY:
            scan_ready_screen(ui);
            break;
        case STC_CAPTURING:
        case STC_CLASSIFYING:
            capturing_screen(ui, state);
            break;
        case STC_RESULT_OK:
            result_screen(ui);
            break;
        case STC_BIN_CHOICE:
            bin_choice_screen(ui);
            break;
        case STC_ASK:
            ask_screen(ui);
            break;
        case STC_INTRO:
            ui->intro_seen = true;
            intro_screen(ui);
            break;
        case STC_RESULT_RETRY:
            retry_screen(ui);
            break;
        case STC_ERROR:
            error_screen(ui, error_code);
            break;
        case STC_CATEGORY_GUIDE:
            /* The category guide page is gone; the ask flow replaces it. */
            home_screen(ui);
            break;
        case STC_LEARNING:
            learning_screen(ui, ui->category_value);
            break;
        case STC_SUMMARY:
            summary_screen(ui);
            break;
        default:
            error_screen(ui, "INVALID_STATE");
            break;
    }
    set_header(ui);
    camera_update(ui);
}

static bool valid_category(int category) {
    return category >= 0 && category < STC_CATEGORY_COUNT;
}

static bool valid_probability(float probability) {
    return isfinite(probability) && probability >= 0.0f &&
           probability <= 1.0f;
}

static bool transition(stc_ui_t *ui, stc_state_t next) {
    if (next == STC_ERROR) {
        return true;
    }
    if (ui->state == STC_ERROR) {
        return ui->acknowledged &&
               (next == STC_IDLE || next == STC_SCAN_READY ||
                next == STC_CAPTURING);
    }
    if (next == ui->state) {
        return true;
    }
    switch (ui->state) {
        case STC_IDLE:
            return next == STC_INTRO || next == STC_SCAN_READY ||
                   next == STC_CATEGORY_GUIDE || next == STC_SUMMARY ||
                   next == STC_ASK;
        case STC_INTRO:
            return next == STC_SCAN_READY || next == STC_CAPTURING ||
                   next == STC_IDLE || next == STC_ASK;
        case STC_ASK:
            return next == STC_IDLE || next == STC_SCAN_READY ||
                   next == STC_CAPTURING || next == STC_SUMMARY;
        case STC_SCAN_READY:
            return next == STC_CAPTURING || next == STC_LEARNING ||
                   next == STC_SUMMARY || next == STC_IDLE;
        case STC_CAPTURING:
            return next == STC_SCAN_READY || next == STC_CLASSIFYING ||
                   next == STC_RESULT_OK || next == STC_RESULT_RETRY ||
                   next == STC_LEARNING || next == STC_SUMMARY ||
                   next == STC_IDLE;
        case STC_CLASSIFYING:
            return next == STC_SCAN_READY || next == STC_RESULT_OK ||
                   next == STC_RESULT_RETRY || next == STC_BIN_CHOICE ||
                   next == STC_LEARNING || next == STC_SUMMARY ||
                   next == STC_IDLE;
        case STC_RESULT_OK:
            return next == STC_BIN_CHOICE || next == STC_LEARNING ||
                   next == STC_SUMMARY || next == STC_IDLE ||
                   next == STC_ASK;
        case STC_BIN_CHOICE:
            return next == STC_LEARNING || next == STC_SUMMARY ||
                   next == STC_IDLE;
        case STC_RESULT_RETRY:
            return next == STC_SCAN_READY || next == STC_CAPTURING ||
                   next == STC_IDLE;
        case STC_CATEGORY_GUIDE:
            return next == STC_LEARNING || next == STC_SUMMARY ||
                   next == STC_IDLE;
        case STC_LEARNING:
            return next == STC_SCAN_READY || next == STC_CAPTURING ||
                   next == STC_SUMMARY ||
                   next == STC_CATEGORY_GUIDE || next == STC_IDLE;
        case STC_SUMMARY:
            return next == STC_SCAN_READY || next == STC_CAPTURING ||
                   next == STC_CATEGORY_GUIDE || next == STC_IDLE ||
                   next == STC_ASK;
        default:
            return false;
    }
}

static void refresh_text(stc_ui_t *ui, const stc_payload_t *payload) {
    set_header(ui);
    if (payload->state == STC_RESULT_RETRY) {
        for (int i = 0; i < 2; ++i) {
            const int category = payload->candidates[i].category_id;
            if (ui->candidate_icon[i] != NULL && valid_category(category)) {
                lv_image_set_src(ui->candidate_icon[i],
                                 category_images[category]);
            }
            if (ui->candidate_name[i] != NULL && valid_category(category)) {
                lv_label_set_text(ui->candidate_name[i],
                                  stc_category_label(category));
            }
            if (ui->candidate_prob[i] != NULL) {
                char probability[16];
                snprintf(probability, sizeof(probability), "%.0f%%",
                         (double)payload->candidates[i].p * 100.0);
                lv_label_set_text(ui->candidate_prob[i], probability);
            }
        }
    }
    if (payload->state == STC_SUMMARY) {
        set_score(ui->score, payload->score, FONT_NUM_SCORE, FONT_NUM_H2);
        if (ui->streak != NULL) {
            char text[64];
            snprintf(text, sizeof(text), "最高連續答對 ×%lu",
                     (unsigned long)payload->streak);
            lv_label_set_text(ui->streak, text);
        }
    }
}

stc_ui_t *stc_ui_create(lv_obj_t *parent, stc_retry_cb retry, void *user) {
    if (parent == NULL) {
        return NULL;
    }
    lv_obj_update_layout(parent);
    if (lv_obj_get_width(parent) != 800 || lv_obj_get_height(parent) != 480) {
        return NULL;
    }
    stc_ui_t *ui = lv_malloc_zeroed(sizeof(*ui));
    if (ui == NULL) {
        return NULL;
    }
    ui->retry = retry;
    ui->user = user;
    ui->category_value = -1;
    ui->learned_scanned_bin = -1;
    ui->learned_dropped_bin = -1;
    ui->learned_correct_bin = -1;
    ui->rounds_total = 5;
    ui->container = lv_obj_create(parent);
    lv_obj_remove_style_all(ui->container);
    lv_obj_set_pos(ui->container, 0, 0);
    lv_obj_set_size(ui->container, 800, 480);
    lv_obj_remove_flag(ui->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui->container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui->container, container_deleted, LV_EVENT_DELETE, ui);
    ui->auto_timer = lv_timer_create(auto_tick, 100, ui);
    render(ui, STC_IDLE, NULL);
    return ui;
}

void stc_ui_destroy(stc_ui_t *ui) {
    if (ui == NULL) {
        return;
    }
    if (ui->scan != NULL) {
        lv_anim_delete(ui->scan, scan_opacity);
    }
    if (ui->container != NULL) {
        lv_obj_delete(ui->container);
    }
    if (ui->original != NULL) {
        lv_image_cache_drop(ui->original);
    }
    if (ui->cutout != NULL) {
        lv_image_cache_drop(ui->cutout);
    }
    lv_free(ui);
}

void stc_ui_set_action_callback(stc_ui_t *ui, stc_action_cb callback) {
    if (ui != NULL) {
        ui->action = callback;
    }
}

void stc_ui_show_guide(stc_ui_t *ui) {
    if (ui == NULL) {
        return;
    }
    stc_payload_t payload = {
        .state = STC_CATEGORY_GUIDE,
        .score = ui->score_value,
        .streak = ui->streak_value,
        .round = ui->round,
        .rounds_total = ui->rounds_total,
        .time_left_ms = ui->time_left_ms,
        .time_total_ms = ui->time_total_ms,
    };
    (void)stc_ui_apply(ui, &payload);
}

void stc_ui_show_learning(stc_ui_t *ui, int category) {
    if (ui == NULL) {
        return;
    }
    stc_payload_t payload = {
        .state = STC_LEARNING,
        .category_id = category,
        .learned_scanned_bin = -1,
        .learned_dropped_bin = -1,
        .learned_correct_bin = category,
        .score = ui->score_value,
        .streak = ui->streak_value,
        .round = ui->round,
        .rounds_total = ui->rounds_total,
        .time_left_ms = ui->time_left_ms,
        .time_total_ms = ui->time_total_ms,
    };
    (void)stc_ui_apply(ui, &payload);
}

void stc_ui_show_summary(stc_ui_t *ui) {
    if (ui == NULL) {
        return;
    }
    stc_payload_t payload = {
        .state = STC_SUMMARY,
        .score = ui->score_value,
        .streak = ui->streak_value,
        .round = ui->round,
        .rounds_total = ui->rounds_total,
        .time_left_ms = ui->time_left_ms,
        .time_total_ms = ui->time_total_ms,
    };
    (void)stc_ui_apply(ui, &payload);
}

void stc_ui_show_ask(stc_ui_t *ui, const stc_chat_payload_t *payload) {
    if (ui == NULL || payload == NULL || payload->state < STC_CHAT_READY ||
        payload->state > STC_CHAT_GOODBYE) {
        return;
    }
    ui->ask_payload = *payload;
    ui->ask_valid = true;
    if (ui->state == STC_ASK) {
        render(ui, STC_ASK, NULL);
        return;
    }
    stc_payload_t apply = {
        .state = STC_ASK,
        .score = ui->score_value,
        .streak = ui->streak_value,
        .round = ui->round,
        .rounds_total = ui->rounds_total,
        .time_left_ms = ui->time_left_ms,
        .time_total_ms = ui->time_total_ms,
    };
    (void)stc_ui_apply(ui, &apply);
}

void stc_ui_retry(stc_ui_t *ui) {
    if (ui == NULL || ui->state != STC_ERROR || ui->acknowledged) {
        return;
    }
    ui->acknowledged = true;
    if (ui->retry != NULL) {
        ui->retry(ui->user);
    }
}

void stc_ui_set_reduced_motion(stc_ui_t *ui, bool reduced) {
    if (ui != NULL) {
        ui->reduced = reduced;
        animate_scan(ui);
    }
}

void stc_ui_set_images(stc_ui_t *ui, const lv_image_dsc_t *original,
                       const lv_image_dsc_t *cutout) {
    if (ui == NULL || ui->container == NULL) {
        return;
    }
    if (ui->original != NULL) {
        lv_image_cache_drop(ui->original);
    }
    if (ui->cutout != NULL) {
        lv_image_cache_drop(ui->cutout);
    }
    ui->original = original;
    ui->cutout = cutout;
    camera_update(ui);
}

bool stc_ui_apply(stc_ui_t *ui, const stc_payload_t *payload) {
    if (ui == NULL || ui->container == NULL || payload == NULL) {
        return false;
    }
    if (payload->state < STC_IDLE || payload->state > STC_INTRO) {
        render(ui, STC_ERROR, "INVALID_STATE");
        ui->acknowledged = false;
        return false;
    }
    if (!transition(ui, payload->state)) {
        render(ui, STC_ERROR, "INVALID_TRANSITION");
        ui->acknowledged = false;
        return false;
    }
    if (payload->state == STC_RESULT_OK &&
        (!valid_category(payload->category_id) ||
         !valid_probability(payload->confidence))) {
        render(ui, STC_ERROR, "INVALID_RESULT");
        ui->acknowledged = false;
        return false;
    }
    if (payload->state == STC_RESULT_RETRY &&
        (!valid_category(payload->candidates[0].category_id) ||
         !valid_category(payload->candidates[1].category_id) ||
         payload->candidates[0].category_id ==
             payload->candidates[1].category_id ||
         !valid_probability(payload->candidates[0].p) ||
         !valid_probability(payload->candidates[1].p))) {
        render(ui, STC_ERROR, "INVALID_CANDIDATES");
        ui->acknowledged = false;
        return false;
    }
    if (payload->state == STC_LEARNING &&
        payload->category_id != -1 &&
        !valid_category(payload->category_id)) {
        render(ui, STC_ERROR, "INVALID_CATEGORY");
        ui->acknowledged = false;
        return false;
    }
    if (payload->state == STC_BIN_CHOICE &&
        payload->category_id != -1 &&
        !valid_category(payload->category_id)) {
        render(ui, STC_ERROR, "INVALID_CATEGORY");
        ui->acknowledged = false;
        return false;
    }
    if (payload->state == STC_ERROR) {
        render(ui, STC_ERROR, payload->error_code);
        ui->acknowledged = false;
        return true;
    }

    const bool state_changed = ui->state != payload->state;
    const bool result_changed =
        payload->state == STC_RESULT_OK &&
        (ui->category_value != payload->category_id ||
         ui->lid_open != payload->lid_open);
    const bool bin_changed =
        payload->state == STC_BIN_CHOICE &&
        (ui->category_value != payload->category_id ||
         ui->lid_open != payload->lid_open ||
         ui->debug_flags != payload->debug_flags);
    const bool learning_changed =
        payload->state == STC_LEARNING &&
        (ui->category_value != payload->category_id ||
         ui->learned_scanned_bin != payload->learned_scanned_bin ||
         ui->learned_dropped_bin != payload->learned_dropped_bin ||
         ui->learned_correct_bin != payload->learned_correct_bin ||
         ui->learned_outcome != payload->learned_outcome ||
         ui->learned_round_delta != payload->learned_round_delta);
    const bool time_changed =
        ui->time_left_ms != payload->time_left_ms ||
        ui->time_total_ms != payload->time_total_ms;

    ui->score_value = payload->score;
    ui->streak_value = payload->streak;
    ui->round = payload->round;
    ui->rounds_total = payload->rounds_total;
    ui->time_left_ms = payload->time_left_ms;
    ui->time_total_ms = payload->time_total_ms;
    ui->category_value = payload->category_id;
    ui->confidence_value = payload->confidence;
    ui->lid_open = payload->lid_open;
    ui->debug_flags = payload->debug_flags;
    ui->learned_scanned_bin = payload->learned_scanned_bin;
    ui->learned_dropped_bin = payload->learned_dropped_bin;
    ui->learned_correct_bin = payload->learned_correct_bin;
    ui->learned_outcome = payload->learned_outcome;
    ui->learned_round_delta = payload->learned_round_delta;
    ui->acknowledged = false;
    if (state_changed || result_changed || bin_changed || learning_changed) {
        render(ui, payload->state, payload->error_code);
    } else if (time_changed) {
        update_time_bar(ui);
    }
    refresh_text(ui, payload);
    return true;
}

stc_state_t stc_ui_state_from_string(const char *state) {
    for (unsigned i = 0;
         state != NULL && i < sizeof(state_names) / sizeof(state_names[0]);
         ++i) {
        if (strcmp(state, state_names[i]) == 0) {
            return (stc_state_t)i;
        }
    }
    return STC_ERROR;
}

const char *stc_ui_state_name(stc_state_t state) {
    return state >= STC_IDLE && state <= STC_INTRO
               ? state_names[state]
               : "error";
}

stc_state_t stc_ui_state(const stc_ui_t *ui) {
    return ui != NULL ? ui->state : STC_ERROR;
}

const char *stc_category_label(int category) {
    static const char *const labels[STC_CATEGORY_COUNT] = {
        "可回收物", "廚餘", "有害", "其他",
    };
    return valid_category(category) ? labels[category] : "";
}

int32_t stc_ui_time_bar_value(const stc_ui_t *ui) {
    return ui != NULL && ui->time_bar != NULL
               ? (int32_t)lv_bar_get_value(ui->time_bar)
               : -1;
}

static bool check_text(lv_obj_t *obj) {
    if (!lv_obj_check_type(obj, &lv_label_class)) {
        return true;
    }
    const char *text = lv_label_get_text(obj);
    const lv_font_t *font = lv_obj_get_style_text_font(obj, 0);
    uint32_t index = 0;
    while (text[index] != '\0') {
        const uint32_t codepoint = lv_text_encoded_next(text, &index);
        if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t') {
            continue;
        }
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(font, &glyph, codepoint, 0) ||
            glyph.is_placeholder) {
            fprintf(stderr, "MISSING_GLYPH U+%04lx\n",
                    (unsigned long)codepoint);
            return false;
        }
    }
    return true;
}

static bool check_tree(lv_obj_t *obj) {
    bool ok = true;
    if (!lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        lv_area_t bounds;
        lv_obj_get_coords(obj, &bounds);
        if (bounds.x1 < 0 || bounds.y1 < 0 || bounds.x2 >= 800 ||
            bounds.y2 >= 480) {
            fprintf(stderr, "OUT_OF_FRAME [%ld,%ld,%ld,%ld]\n",
                    (long)bounds.x1, (long)bounds.y1, (long)bounds.x2,
                    (long)bounds.y2);
            ok = false;
        }
        ok = check_text(obj) && ok;
    }
    const uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; ++i) {
        ok = check_tree(lv_obj_get_child(obj, i)) && ok;
    }
    return ok;
}

bool stc_ui_check_layout(stc_ui_t *ui) {
    if (ui == NULL || ui->container == NULL) {
        return false;
    }
    lv_obj_update_layout(ui->container);
    return check_tree(ui->container);
}
