#include "stc_game_placeholder.h"

#include <material_symbols.h>

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(font_material_symbols_30_4);

void stc_game_placeholder_build(lv_obj_t* parent) {
    // Integration seam: the game team can replace this body without touching
    // networking, XiaoZhi, the outer mode FSM, or the persistent mode bar.
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0E1726), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_style_pad_all(parent, 28, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* icon = lv_label_create(parent);
    lv_label_set_text(icon, MATERIAL_SYMBOLS_SPORTS_ESPORTS);
    lv_obj_set_style_text_font(icon, &font_material_symbols_30_4, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x38BDF8), 0);

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "GAME MODE");
    lv_obj_set_style_text_font(title, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t* message = lv_label_create(parent);
    lv_label_set_text(message, "Game UI placeholder");
    lv_obj_set_style_text_font(message, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(message, lv_color_hex(0xAFC3D9), 0);

    lv_obj_t* replace_hint = lv_label_create(parent);
    lv_label_set_text(replace_hint, "Replace stc_game_placeholder.cc");
    lv_obj_set_style_text_font(replace_hint, &BUILTIN_TEXT_FONT, 0);
    lv_obj_set_style_text_color(replace_hint, lv_color_hex(0x64748B), 0);
}
