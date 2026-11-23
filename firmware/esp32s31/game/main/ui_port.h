#pragma once

#include "stc_game.h"
#include "stc_game_controller.h"
#include "stc_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*stc_ui_action_sink_t)(stc_ui_action_t action,
                                    int32_t category_id, void *ctx);

stc_err_t stc_game_ui_init(stc_ui_action_sink_t sink, void *ctx);
void stc_game_ui_render(const stc_snapshot_t *snapshot,
                        const stc_game_view_t *view, void *ctx);
void stc_game_ui_set_camera_rgb160(const uint8_t *rgb, uint16_t width,
                                   uint16_t height);
void stc_game_ui_show_chat(bool voice_available);
bool stc_game_ui_show_intro(void);
void stc_game_ui_show_guide(void);
void stc_game_ui_deinit(void);

#ifdef __cplusplus
}
#endif
