#include "stc_chat_ui.h"

#include "stc_lvglpro_assets.h"
#include "src/misc/lv_text_private.h"

#include <stdio.h>
#include <string.h>

#define CHAT_BG        0xFBF7EC
#define CHAT_SURFACE   0xFFFFFF
#define CHAT_FG        0x14342B
#define CHAT_MUTED     0x5A6B62
#define CHAT_BRAND     0x14342B
#define CHAT_ON_BRAND  0xFBF7EC
#define CHAT_ACCENT    0x7FD8B0
#define CHAT_ON_ACCENT 0x14342B

#define FONT_CAPTION   (&stc_lp_font_tc_medium_26)
#define FONT_BODY      (&stc_lp_font_tc_medium_32)
#define FONT_BODY_BOLD (&stc_lp_font_tc_bold_32)
#define FONT_TITLE     (&stc_lp_font_tc_bold_40)

typedef struct {
    stc_chat_ui_t *ui;
    stc_chat_action_t action;
} ChatActionBinding;

struct stc_chat_ui {
    lv_obj_t *container;
    stc_chat_action_cb callback;
    void *user;
    ChatActionBinding actions[2];
    unsigned action_count;
    stc_chat_state_t state;
    const lv_font_t *message_font;
};

typedef struct {
    const char *heading;
    const char *message;
    const char *primary;
    const char *secondary;
    const lv_image_dsc_t *mascot;
} ChatDefaults;

static const ChatDefaults defaults[] = {
    /* READY has no primary action: 問綠豆 hands the turn to the xiaozhi
     * voice app, so the only useful control is the way back. */
    [STC_CHAT_READY] = {
        "有咩想問？",
        "我可以同你傾吓垃圾分類。",
        NULL,
        "返回主頁",
        &stc_lp_img_mascot_idle,
    },
    [STC_CHAT_LISTENING] = {
        "聽緊你講",
        "慢慢講，我聽緊。",
        "講完",
        "返回主頁",
        &stc_lp_img_mascot_listen,
    },
    [STC_CHAT_THINKING] = {
        "諗緊",
        "等我諗一諗。",
        "取消",
        "返回主頁",
        &stc_lp_img_mascot_idle,
    },
    [STC_CHAT_ANSWERING] = {
        "綠豆話",
        "答案會喺呢度顯示。",
        "再問一題",
        "返回主頁",
        &stc_lp_img_mascot_talk,
    },
    [STC_CHAT_OFFLINE] = {
        "暫時未能連線",
        "請稍後再試。",
        "重試",
        "返回主頁",
        &stc_lp_img_mascot_sad,
    },
    [STC_CHAT_ASR_RETRY] = {
        "聽唔清楚",
        "唔緊要，請再講一次。",
        "再講一次",
        "返回主頁",
        &stc_lp_img_mascot_puzzled,
    },
    [STC_CHAT_COUNTDOWN] = {
        "仲想問？",
        "想繼續可以直接講。",
        "繼續問",
        "返回主頁",
        &stc_lp_img_mascot_idle,
    },
};

static lv_obj_t *chat_box(lv_obj_t *parent, int x, int y, int w, int h,
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

static lv_obj_t *chat_label(lv_obj_t *parent, const char *text, int x, int y,
                            int w, int h, const lv_font_t *font,
                            uint32_t color, lv_text_align_t align) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, w);
    if (h == LV_SIZE_CONTENT) {
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

static lv_obj_t *chat_image(lv_obj_t *parent, const lv_image_dsc_t *source,
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

static lv_obj_t *chat_button(lv_obj_t *parent, const char *text, int x,
                             int y, int w, int h, bool primary) {
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 16, 0);
    lv_obj_set_style_bg_color(
        obj, lv_color_hex(primary ? CHAT_ACCENT : CHAT_SURFACE), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, primary ? 0 : 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xBFBFBF), 0);
    lv_obj_set_style_bg_opa(obj, 210, LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(obj, 3, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(obj, lv_color_hex(CHAT_FG),
                                   LV_STATE_FOCUSED);
    lv_obj_t *label =
        chat_label(obj, text, 0, 0, w, h, FONT_BODY_BOLD,
                   primary ? CHAT_ON_ACCENT : CHAT_FG, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_obj_center(label);
    return obj;
}

static void chat_action_event(lv_event_t *event) {
    ChatActionBinding *binding = lv_event_get_user_data(event);
    if (binding == NULL || binding->ui == NULL ||
        binding->ui->callback == NULL ||
        lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    binding->ui->callback(binding->action, binding->ui->user);
}

static void bind_chat_action(stc_chat_ui_t *ui, lv_obj_t *obj,
                             stc_chat_action_t action) {
    if (obj == NULL || ui->action_count >= 2) {
        return;
    }
    ChatActionBinding *binding = &ui->actions[ui->action_count++];
    binding->ui = ui;
    binding->action = action;
    lv_obj_add_event_cb(obj, chat_action_event, LV_EVENT_CLICKED, binding);
}

static const char *choose(const char *value, const char *fallback) {
    return value != NULL && value[0] != '\0' ? value : fallback;
}

/* Figma chat2/01-07 put 已連線 in six frames and 暫時離線 in 05-offline. */
static const char *chat_status(stc_chat_state_t state) {
    switch (state) {
        case STC_CHAT_OFFLINE:
            return "暫時離線";
        default:
            return "已連線";
    }
}

static void render(stc_chat_ui_t *ui, const stc_chat_payload_t *payload) {
    const ChatDefaults *fallback = &defaults[payload->state];
    lv_obj_clean(ui->container);
    ui->action_count = 0;
    memset(ui->actions, 0, sizeof(ui->actions));
    lv_obj_set_style_bg_color(ui->container, lv_color_hex(CHAT_BG), 0);
    lv_obj_set_style_bg_opa(ui->container, LV_OPA_COVER, 0);

    /* Figma chat2-xx: rounded 24,24,752x56 bar, "問綠豆" left, connection
     * state right. This is not the game topbar, by design. */
    lv_obj_t *bar =
        chat_box(ui->container, 24, 24, 752, 56, CHAT_BRAND, 16, 0,
                 CHAT_BRAND);
    chat_label(bar, "問綠豆", 24, 0, 240, 56, FONT_BODY_BOLD,
               CHAT_ON_BRAND, LV_TEXT_ALIGN_LEFT);
    chat_label(bar, chat_status(payload->state), 520, 0, 208, 56,
               FONT_BODY_BOLD, CHAT_ACCENT, LV_TEXT_ALIGN_RIGHT);

    chat_label(ui->container, choose(payload->heading, fallback->heading), 24,
               96, 620, 48, FONT_TITLE, CHAT_FG, LV_TEXT_ALIGN_LEFT);
    chat_image(ui->container, fallback->mascot, 664, 96, 88, 88);

    lv_obj_t *speech =
        chat_box(ui->container, 24, 158, 632, LV_SIZE_CONTENT, CHAT_SURFACE,
                 24, 0, CHAT_SURFACE);
    lv_obj_set_flex_flow(speech, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(speech, 14, 0);
    lv_obj_set_style_pad_row(speech, 6, 0);
    /* Hug the text, but never grow past the room above the buttons. */
    lv_obj_set_style_max_height(speech, 180, 0);
    lv_obj_add_flag(speech, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(speech, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(speech, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_t *role_label =
        chat_label(speech, "綠豆", 0, 0, 560, 32, FONT_BODY_BOLD,
                   CHAT_MUTED, LV_TEXT_ALIGN_LEFT);
    chat_label(speech, choose(payload->message, fallback->message), 0, 0,
               560, LV_SIZE_CONTENT,
               ui->message_font != NULL ? ui->message_font : FONT_CAPTION,
               CHAT_FG, LV_TEXT_ALIGN_LEFT);

    const char *primary =
        choose(payload->primary_label, fallback->primary);
    const char *secondary =
        choose(payload->secondary_label, fallback->secondary);
    if (primary == NULL) {
        /* No primary action: the controller hands the turn to voice. */
        if (secondary != NULL) {
            lv_obj_t *back =
                chat_button(ui->container, secondary, 24, 368, 736, 88, false);
            bind_chat_action(ui, back, STC_CHAT_ACTION_BACK);
        }
    } else if (secondary != NULL) {
        lv_obj_t *back =
            chat_button(ui->container, secondary, 24, 368, 352, 88, false);
        bind_chat_action(ui, back, STC_CHAT_ACTION_BACK);
        lv_obj_t *action =
            chat_button(ui->container, primary, 408, 368, 352, 88, true);
        bind_chat_action(ui, action, STC_CHAT_ACTION_PRIMARY);
    } else {
        lv_obj_t *action =
            chat_button(ui->container, primary, 24, 368, 736, 88, true);
        bind_chat_action(ui, action, STC_CHAT_ACTION_PRIMARY);
    }
    lv_obj_update_layout(ui->container);
    lv_obj_scroll_to_view(role_label, LV_ANIM_OFF);
    ui->state = payload->state;
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
            fprintf(stderr, "CHAT_MISSING_GLYPH U+%04lx\n",
                    (unsigned long)codepoint);
            return false;
        }
    }
    return true;
}

static bool check_tree(lv_obj_t *obj) {
    bool ok = true;
    lv_area_t bounds;
    lv_obj_get_coords(obj, &bounds);
    if (bounds.x1 < 0 || bounds.y1 < 0 || bounds.x2 >= 800 ||
        bounds.y2 >= 480) {
        fprintf(stderr, "CHAT_OUT_OF_FRAME [%ld,%ld,%ld,%ld]\n",
                (long)bounds.x1, (long)bounds.y1, (long)bounds.x2,
                (long)bounds.y2);
        ok = false;
    }
    ok = check_text(obj) && ok;
    const uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; ++i) {
        ok = check_tree(lv_obj_get_child(obj, i)) && ok;
    }
    return ok;
}

stc_chat_ui_t *stc_chat_ui_create(lv_obj_t *parent,
                                  stc_chat_action_cb callback,
                                  void *user_data) {
    if (parent == NULL) {
        return NULL;
    }
    lv_obj_update_layout(parent);
    if (lv_obj_get_width(parent) != 800 || lv_obj_get_height(parent) != 480) {
        return NULL;
    }
    stc_chat_ui_t *ui = lv_malloc_zeroed(sizeof(*ui));
    if (ui == NULL) {
        return NULL;
    }
    ui->callback = callback;
    ui->user = user_data;
    ui->container = lv_obj_create(parent);
    lv_obj_remove_style_all(ui->container);
    lv_obj_set_size(ui->container, 800, 480);
    lv_obj_remove_flag(ui->container, LV_OBJ_FLAG_SCROLLABLE);
    const stc_chat_payload_t initial = {.state = STC_CHAT_READY};
    render(ui, &initial);
    return ui;
}

void stc_chat_ui_destroy(stc_chat_ui_t *ui) {
    if (ui == NULL) {
        return;
    }
    if (ui->container != NULL) {
        lv_obj_delete(ui->container);
    }
    lv_free(ui);
}

bool stc_chat_ui_apply(stc_chat_ui_t *ui,
                       const stc_chat_payload_t *payload) {
    if (ui == NULL || ui->container == NULL || payload == NULL ||
        payload->state < STC_CHAT_READY ||
        payload->state > STC_CHAT_COUNTDOWN) {
        return false;
    }
    render(ui, payload);
    return true;
}

void stc_chat_ui_set_message_font(stc_chat_ui_t *ui,
                                  const lv_font_t *font) {
    if (ui != NULL && ui->message_font != font) {
        ui->message_font = font;
    }
}

stc_chat_state_t stc_chat_ui_state(const stc_chat_ui_t *ui) {
    return ui != NULL ? ui->state : STC_CHAT_READY;
}

bool stc_chat_ui_check_layout(stc_chat_ui_t *ui) {
    if (ui == NULL || ui->container == NULL) {
        return false;
    }
    lv_obj_update_layout(ui->container);
    return check_tree(ui->container);
}
