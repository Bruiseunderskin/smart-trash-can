#include "stc_game_placeholder.h"

#include <lvgl.h>

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);

void stc_game_placeholder_build(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x07111F), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_style_pad_all(parent, 32, 0);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "分類遊戲");
    lv_obj_set_style_text_font(title, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xF8FAFC), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* hint = lv_label_create(parent);
    lv_label_set_text(hint, "按「CHAT」問綠豆。\n遊戲 UI 將於下一階段接入。");
    lv_obj_set_style_text_font(hint, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x9FB4C8), 0);
    lv_obj_set_style_text_line_space(hint, 8, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
}
