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
#define FONT_BODY      (&stc_lp_font_tc_medium_32)
#define FONT_BODY_BOLD (&stc_lp_font_tc_bold_32)
#define FONT_SUBTITLE  (&stc_lp_font_tc_medium_40)
#define FONT_H2        (&stc_lp_font_tc_bold_40)
#define FONT_H1        (&stc_lp_font_tc_bold_56)
#define FONT_NUM_BODY  (&stc_lp_font_num_bold_32)
#define FONT_NUM_H2    (&stc_lp_font_num_bold_40)
#define FONT_NUM_SCORE (&stc_lp_font_num_bold_80)

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
    lv_obj_t *confidence;
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
    bool lid_open;
    bool reduced;
    bool acknowledged;
    lv_timer_t *auto_timer;
    lv_obj_t *auto_bar;
    uint8_t auto_left;
    bool auto_armed;
    bool auto_done;
    uint8_t debug_flags;
    int learned_scanned_bin;
    int learned_dropped_bin;
    int learned_correct_bin;
    int learned_outcome;
    int learned_round_delta;
    stc_state_t state;
};

static const char *state_names[] = {
    "idle", "capturing", "classifying", "result_ok", "result_retry",
    "error", "category_guide", "learning", "summary", "scan_ready",
    "bin_choice",
};

static const uint32_t category_colors[STC_CATEGORY_COUNT] = {
    0x2455C0, 0x176640, 0x9E3F20, 0x4E5A62,
};

static const uint32_t category_tints[STC_CATEGORY_COUNT] = {
    0xE6E8EA, 0xE5EBDC, 0xF5E7D9, 0xEBE9DF,
};

extern const lv_image_dsc_t stc_icon_food; /* 廚餘, drawn to match the set */

static const lv_image_dsc_t *const category_images[STC_CATEGORY_COUNT] = {
    &stc_lp_img_cat_0,
    &stc_icon_food,
    &stc_lp_img_cat_2,
    &stc_lp_img_cat_3,
};

static const char *const guide_tips[STC_CATEGORY_COUNT] = {
    "清洗後再壓扁",
    "瀝乾水分後丟棄",
    "單獨裝袋後丟棄",
    "不屬於前三類",
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
    lv_obj_t *bar = box(ui->container, 32, 24, 736, 68, COLOR_BRAND, 16, 0,
                        COLOR_BRAND);
    label(bar, "分類小隊", 24, 0, 220, 68, FONT_BODY_BOLD, COLOR_ON_BRAND,
          LV_TEXT_ALIGN_LEFT);
    image(bar, &stc_lp_img_flame, 656, 18, 32, 32);
    ui->header_status =
        label(bar, "", 440, 0, 196, 68, FONT_BODY_BOLD, COLOR_ON_BRAND,
              LV_TEXT_ALIGN_RIGHT);
}

static void page_background(stc_ui_t *ui) {
    lv_obj_set_style_bg_color(ui->container, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(ui->container, LV_OPA_COVER, 0);
}

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
    if (ui->rounds_total > 0 && ui->state != STC_IDLE) {
        unsigned current = ui->round == 0 ? 1u : ui->round;
        if (current > ui->rounds_total) {
            current = ui->rounds_total;
        }
        snprintf(text, sizeof(text), "第 %u / %u 回合", current,
                 (unsigned)ui->rounds_total);
    } else {
        snprintf(text, sizeof(text), "得分 %lu",
                 (unsigned long)ui->score_value);
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
    image(ui->camera, &stc_lp_img_camera, 176, 20, 48, 48);
    label(ui->camera, "等待相機畫面", 24, 72, 352, 44, FONT_BODY,
          COLOR_ON_BRAND, LV_TEXT_ALIGN_CENTER);
}

static lv_obj_t *camera_card(stc_ui_t *ui, const char *title,
                             const char *subtitle) {
    lv_obj_t *card = box(ui->container, 32, 116, 736, 220,
                         COLOR_BRAND_LIGHT, 16, 0, COLOR_BRAND_LIGHT);
    ui->camera = box(card, 136, 18, 400, 132, COLOR_BRAND, 12, 3,
                     COLOR_ON_BRAND);
    lv_obj_set_style_border_opa(ui->camera, 92, 0);
    label(card, title, 24, 150, 688, 38, FONT_BODY_BOLD, COLOR_ON_BRAND,
          LV_TEXT_ALIGN_CENTER);
    label(card, subtitle, 24, 190, 688, 26, FONT_CAPTION, 0xD5E5DC,
          LV_TEXT_ALIGN_CENTER);
    ui->scan = box(card, 652, 24, 56, 56, COLOR_ACCENT, LV_RADIUS_CIRCLE, 0,
                   COLOR_ACCENT);
    lv_obj_set_style_bg_opa(ui->scan, 100, 0);
    label(card, "AI", 652, 39, 56, 32, FONT_BODY_BOLD, COLOR_ON_ACCENT,
          LV_TEXT_ALIGN_CENTER);
    return card;
}

static void home_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    label(ui->container, "一起做分類小達人", 32, 142, 736, 92, FONT_H1,
          COLOR_FG, LV_TEXT_ALIGN_CENTER);
    label(ui->container, "3 分鐘 · 5 回合 · 認識四大類別", 32, 244, 620, 48,
          FONT_SUBTITLE, COLOR_MUTED, LV_TEXT_ALIGN_CENTER);
    image(ui->container, &stc_lp_img_mascot_idle, 664, 142, 72, 72);
    lv_obj_t *start =
        button(ui->container, "開始挑戰", 32, 368, 352, 88, true);
    bind_action(ui, start, STC_ACTION_START, -1, false);
    lv_obj_t *guide =
        button(ui->container, "分類圖鑑", 416, 368, 352, 88, false);
    bind_action(ui, guide, STC_ACTION_GUIDE, -1, false);
}

static void scan_ready_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    lv_obj_t *card =
        box(ui->container, 32, 116, 736, 220, COLOR_BRAND_LIGHT, 16, 0,
            COLOR_BRAND_LIGHT);
    image(card, &stc_lp_img_mic, 336, 28, 64, 64);
    label(card, "將垃圾放在鏡頭前", 32, 108, 672, 56, FONT_H1,
          COLOR_ON_BRAND, LV_TEXT_ALIGN_CENTER);
    label(card, "準備好就按「開始掃描」", 32, 168, 672, 34, FONT_CAPTION,
          0xD5E5DC, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *scan =
        button(ui->container, "開始掃描", 32, 368, 736, 88, true);
    bind_action(ui, scan, STC_ACTION_SCAN, -1, false);
}

static void capturing_screen(stc_ui_t *ui, stc_state_t state) {
    page_background(ui);
    topbar(ui);
    camera_card(ui,
                state == STC_CAPTURING ? "正在辨識卡牌" : "整理緊結果",
                "保持卡牌完整喺鏡頭框內");
    lv_obj_t *cancel =
        button(ui->container, "返回主頁", 32, 368, 736, 88, false);
    bind_action(ui, cancel, STC_ACTION_HOME, -1, false);
    animate_scan(ui);
}

static void retry_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    label(ui->container, "AI 都唔肯定", 32, 116, 736, 68, FONT_H1, COLOR_FG,
          LV_TEXT_ALIGN_LEFT);
    label(ui->container, "請揀最接近嘅答案，或者重新掃描", 32, 186, 736, 32,
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
        button(ui->container, "重新掃描", 32, 368, 352, 88, true);
    bind_action(ui, retry, STC_ACTION_START, -1, false);
    lv_obj_t *home =
        button(ui->container, "返回主頁", 416, 368, 352, 88, false);
    bind_action(ui, home, STC_ACTION_HOME, -1, true);
}

static void progress_bar(stc_ui_t *ui) {
    box(ui->container, 32, 104, 736, 8, COLOR_LINE, LV_RADIUS_CIRCLE, 0,
        COLOR_LINE);
    unsigned total = ui->rounds_total > 0 ? ui->rounds_total : 5;
    unsigned round = ui->round == 0 ? 1u : ui->round;
    if (round > total) {
        round = total;
    }
    int width = (int)(736u * round / total);
    if (width < 24) {
        width = 24;
    }
    box(ui->container, 32, 104, width, 8, COLOR_ACCENT, LV_RADIUS_CIRCLE, 0,
        COLOR_ACCENT);
}

/* Round exit -------------------------------------------------------------
 * The result screen is the round boundary: one 752x88 primary action, plus a
 * three-second auto-advance with a shrinking bar. The timer is cancelled from
 * clear_screen(), so every state change (including an error or a retry) kills
 * it; a lid that is open means the card is still on its way into the bin, so
 * the countdown does not start until the controller reports the lid closed.
 */
#define AUTO_ADVANCE_TICKS 30 /* 30 x 100 ms */

static void auto_disarm(stc_ui_t *ui) {
    ui->auto_armed = false;
    ui->auto_left = 0;
    ui->auto_bar = NULL;
}

/* One persistent 100 ms timer, created with the UI and armed only on the
 * result screen. Nothing is deleted from inside the callback, so a state
 * change during a tick cannot free the timer that is running. */
static void auto_tick(lv_timer_t *timer) {
    stc_ui_t *ui = (stc_ui_t *)lv_timer_get_user_data(timer);
    if (ui == NULL || !ui->auto_armed) {
        return;
    }
    if (ui->state != STC_RESULT_OK || ui->auto_done) {
        auto_disarm(ui);
        return;
    }
    if (ui->auto_left > 0) {
        ui->auto_left--;
    }
    if (ui->auto_bar != NULL) {
        lv_bar_set_value(ui->auto_bar, ui->auto_left, LV_ANIM_OFF);
    }
    if (ui->auto_left == 0) {
        ui->auto_done = true;
        auto_disarm(ui);
        if (ui->action != NULL) {
            ui->action(STC_ACTION_NEXT, -1, ui->user);
        }
    }
}

static bool result_is_last_round(const stc_ui_t *ui) {
    return ui->rounds_total > 0 && ui->round >= ui->rounds_total;
}

static void result_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    progress_bar(ui);
    ui->auto_bar = NULL;
    ui->auto_done = false;
    const int category =
        ui->category_value >= 0 && ui->category_value < STC_CATEGORY_COUNT
            ? ui->category_value
            : -1;
    if (ui->lid_open) {
        char title[96];
        if (category >= 0) {
            snprintf(title, sizeof(title), "請投入%s桶",
                     stc_category_label(category));
        } else {
            snprintf(title, sizeof(title), "請投入正確垃圾桶");
        }
        label(ui->container, title, 32, 124, 736, 56, FONT_H2, COLOR_FG,
              LV_TEXT_ALIGN_LEFT);
    } else {
        label(ui->container, "正在開啟桶蓋", 32, 124, 736, 56, FONT_H1,
              COLOR_FG, LV_TEXT_ALIGN_LEFT);
    }
    ui->confidence =
        label(ui->container, "", 32, 184, 736, 22, FONT_CAPTION, COLOR_MUTED,
              LV_TEXT_ALIGN_LEFT);
    for (int i = 0; i < STC_CATEGORY_COUNT; ++i) {
        const int x = 32 + i * 190;
        const bool selected = i == category;
        lv_obj_t *card =
            box(ui->container, x, 212, 166, 132,
                selected ? category_tints[i] : COLOR_SURFACE, 16,
                selected ? 6 : 2, category_colors[i]);
        image(card, category_images[i], 55, 8, 56, 56);
        label(card, stc_category_label(i), 8, 68, 150, 36, FONT_BODY_BOLD,
              COLOR_FG, LV_TEXT_ALIGN_CENTER);
        if (selected) {
            label(card, "AI 判斷", 20, 98, 126, 20, FONT_CAPTION,
                  category_colors[i], LV_TEXT_ALIGN_CENTER);
        }
    }

    const bool last = result_is_last_round(ui);
    ui->auto_bar = lv_bar_create(ui->container);
    lv_obj_remove_style_all(ui->auto_bar);
    lv_obj_set_pos(ui->auto_bar, 32, 382);
    lv_obj_set_size(ui->auto_bar, 736, 6);
    lv_bar_set_range(ui->auto_bar, 0, AUTO_ADVANCE_TICKS);
    lv_bar_set_value(ui->auto_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui->auto_bar, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_bg_opa(ui->auto_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ui->auto_bar, lv_color_hex(COLOR_BRAND),
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui->auto_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui->auto_bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(ui->auto_bar, LV_RADIUS_CIRCLE,
                            LV_PART_INDICATOR);

    lv_obj_t *next = button(ui->container, last ? "完成 · 睇總分" : "下一回合",
                            32, 390, 736, 80, true);
    bind_action(ui, next, last ? STC_ACTION_FINISH : STC_ACTION_NEXT, -1, false);

    if (ui->lid_open) {
        label(ui->container, "投入卡牌後自動繼續", 32, 356, 704, 22,
              FONT_CAPTION, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
    } else if (!last) {
        label(ui->container, "3 秒後自動繼續", 32, 356, 704, 22,
              FONT_CAPTION, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
        ui->auto_left = AUTO_ADVANCE_TICKS;
        ui->auto_armed = true;
        lv_bar_set_value(ui->auto_bar, AUTO_ADVANCE_TICKS, LV_ANIM_OFF);
    }
}

static void bin_choice_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    progress_bar(ui);
    label(ui->container, "請選擇垃圾桶", 32, 124, 736, 72, FONT_H1,
          COLOR_FG, LV_TEXT_ALIGN_LEFT);
    if ((ui->debug_flags & STC_UI_DEBUG_TOUCH_BINS) != 0) {
        label(ui->container, "S3 手部離線 · 請直接點選實際投放嘅桶",
              32, 184, 736, 32, FONT_CAPTION, COLOR_MUTED,
              LV_TEXT_ALIGN_LEFT);
    } else {
        label(ui->container, "桶蓋已開啟 · 可投入卡牌或直接點選桶",
              32, 184, 736, 32, FONT_CAPTION, COLOR_MUTED,
              LV_TEXT_ALIGN_LEFT);
    }
    const int xs[STC_CATEGORY_COUNT] = {32, 222, 412, 602};
    for (int i = 0; i < STC_CATEGORY_COUNT; ++i) {
        lv_obj_t *card =
            box(ui->container, xs[i], 216, 166, 248, COLOR_SURFACE, 16, 3,
                category_colors[i]);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *icon = image(card, category_images[i], 39, 38, 88, 88);
        lv_obj_t *name =
            label(card, stc_category_label(i), 0, 158, 166, 52,
                  FONT_BODY_BOLD, COLOR_FG, LV_TEXT_ALIGN_CENTER);
        lv_obj_add_flag(icon,
                        LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(name,
                        LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
        bind_action(ui, card, STC_ACTION_SELECT_BIN, i, false);
    }
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
    if (code != NULL && code[0] != '\0') {
        label(ui->container, code, 32, 316, 736, 24, FONT_CAPTION, COLOR_MUTED,
              LV_TEXT_ALIGN_LEFT);
    }
    lv_obj_t *retry = button(ui->container, "重試", 32, 368, 352, 88, true);
    bind_action(ui, retry, STC_ACTION_HOME, -1, true);
    lv_obj_t *home =
        button(ui->container, "返回主頁", 416, 368, 352, 88, false);
    bind_action(ui, home, STC_ACTION_HOME, -1, false);
}

static void guide_card(stc_ui_t *ui, int category, int x, int y) {
    lv_obj_t *card = box(ui->container, x, y, 356, 88,
                         category_tints[category], 16, 3,
                         category_colors[category]);
    image(card, category_images[category], 16, 16, 56, 56);
    label(card, stc_category_label(category), 92, 10, 244, 40, FONT_BODY_BOLD,
          COLOR_FG, LV_TEXT_ALIGN_LEFT);
    label(card, guide_tips[category], 92, 48, 244, 30, FONT_CAPTION,
          COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
    bind_action(ui, card, STC_ACTION_LEARN, category, false);
}

static void guide_screen(stc_ui_t *ui) {
    page_background(ui);
    topbar(ui);
    label(ui->container, "分類圖鑑", 32, 112, 736, 48, FONT_H2, COLOR_FG,
          LV_TEXT_ALIGN_LEFT);
    guide_card(ui, 0, 32, 168);
    guide_card(ui, 1, 412, 168);
    guide_card(ui, 2, 32, 272);
    guide_card(ui, 3, 412, 272);
    lv_obj_t *ask =
        button(ui->container, "問綠豆知多啲", 32, 368, 352, 88, true);
    bind_action(ui, ask, STC_ACTION_ASK, -1, false);
    lv_obj_t *home =
        button(ui->container, "返回主頁", 416, 368, 352, 88, false);
    bind_action(ui, home, STC_ACTION_HOME, -1, false);
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
          152, 128, 480, 64,
          FONT_H1, COLOR_FG, LV_TEXT_ALIGN_LEFT);
    label(ui->container, has_category ? "正確類別" : "本回合",
          560, 144, 208, 32, FONT_CAPTION,
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
    label(info_card, result_line, 20, 8, 696, 38, FONT_CAPTION, COLOR_FG,
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
    label(info_card, outcome_line, 20, 50, 696, 36, FONT_CAPTION,
          COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
    label(ui->container,
          has_category ? learning_tips[category]
                       : "請將卡牌完整放進鏡頭框，然後重新掃描。",
          32, 310, 736, 42,
          FONT_CAPTION, COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *next =
        button(ui->container, "下一回合", 32, 368, 736, 88, true);
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
    label(stats, round_text, 24, 24, 376, 44, FONT_BODY_BOLD, COLOR_FG,
          LV_TEXT_ALIGN_LEFT);
    ui->streak =
        label(stats, "最高連續答對 ×0", 24, 92, 376, 44, FONT_BODY_BOLD,
              COLOR_FG, LV_TEXT_ALIGN_LEFT);
    label(stats, "記住：先分類，再投放", 24, 164, 376, 36, FONT_CAPTION,
          COLOR_MUTED, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *again =
        button(ui->container, "再玩一次", 32, 368, 352, 88, true);
    bind_action(ui, again, STC_ACTION_START, -1, false);
    lv_obj_t *guide =
        button(ui->container, "分類圖鑑", 416, 368, 352, 88, false);
    bind_action(ui, guide, STC_ACTION_GUIDE, -1, false);
    image(ui->container, &stc_lp_img_mascot_happy, 664, 136, 72, 72);
}

static void clear_screen(stc_ui_t *ui) {
    auto_disarm(ui);
    if (ui->scan != NULL) {
        lv_anim_delete(ui->scan, scan_opacity);
    }
    lv_obj_clean(ui->container);
    ui->camera = NULL;
    ui->camera_image = NULL;
    ui->scan = NULL;
    ui->header_status = NULL;
    ui->score = NULL;
    ui->streak = NULL;
    ui->confidence = NULL;
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
        case STC_RESULT_RETRY:
            retry_screen(ui);
            break;
        case STC_ERROR:
            error_screen(ui, error_code);
            break;
        case STC_CATEGORY_GUIDE:
            guide_screen(ui);
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
            return next == STC_SCAN_READY || next == STC_CATEGORY_GUIDE ||
                   next == STC_SUMMARY;
        case STC_SCAN_READY:
            return next == STC_CAPTURING || next == STC_LEARNING ||
                   next == STC_SUMMARY || next == STC_IDLE;
        case STC_CAPTURING:
            return next == STC_SCAN_READY || next == STC_CLASSIFYING ||
                   next == STC_RESULT_RETRY || next == STC_LEARNING ||
                   next == STC_SUMMARY || next == STC_IDLE;
        case STC_CLASSIFYING:
            return next == STC_SCAN_READY || next == STC_RESULT_OK ||
                   next == STC_RESULT_RETRY || next == STC_BIN_CHOICE ||
                   next == STC_LEARNING || next == STC_SUMMARY ||
                   next == STC_IDLE;
        case STC_RESULT_OK:
            return next == STC_BIN_CHOICE || next == STC_LEARNING ||
                   next == STC_SUMMARY || next == STC_IDLE;
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
                   next == STC_CATEGORY_GUIDE || next == STC_IDLE;
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
    if (payload->state == STC_RESULT_OK && ui->confidence != NULL) {
        char text[96];
        if (valid_category(payload->category_id)) {
            snprintf(text, sizeof(text), "AI 判斷：%s · 信心值 %.0f%%",
                     stc_category_label(payload->category_id),
                     (double)payload->confidence * 100.0);
        } else {
            snprintf(text, sizeof(text), "等待模型結果");
        }
        lv_label_set_text(ui->confidence, text);
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
    };
    (void)stc_ui_apply(ui, &payload);
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
    if (payload->state < STC_IDLE || payload->state > STC_BIN_CHOICE) {
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

    ui->score_value = payload->score;
    ui->streak_value = payload->streak;
    ui->round = payload->round;
    ui->rounds_total = payload->rounds_total;
    ui->category_value = payload->category_id;
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
    return state >= STC_IDLE && state <= STC_BIN_CHOICE
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
