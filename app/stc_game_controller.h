/* stc_game_controller.h — platform-independent game orchestration. */
#pragma once

#include "stc_game.h"
#include "stc_hand_msg.h"
#include "stc_ui.h"
#include "stc_vision.h"
#include "stc_voice.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool started;
    bool hand_online;
    bool lid_open;
    bool awaiting_open_ack;
    uint8_t debug_flags;
    const char *fault_code;
} stc_game_view_t;

typedef struct {
    stc_err_t (*request_scan)(uint8_t round, uint32_t round_id, void *ctx);
    stc_err_t (*hand_open_all)(uint8_t round, uint32_t round_id, void *ctx);
    stc_err_t (*hand_hold_all)(uint8_t round, uint32_t round_id, void *ctx);
    stc_err_t (*hand_close_all)(uint8_t round, uint32_t round_id, void *ctx);
    stc_err_t (*voice_say)(stc_line_t line, void *ctx);
    stc_err_t (*voice_sfx)(stc_sfx_t sfx, void *ctx);
    void (*render)(const stc_snapshot_t *snapshot,
                   const stc_game_view_t *view, void *ctx);
    /* Optional: hand the screen to the voice assistant (綠豆). NULL on builds
     * without the chat shell; STC_UI_ACTION_ASK is then ignored. */
    void (*request_chat)(void *ctx);
} stc_game_controller_ports_t;

void stc_game_controller_init(const stc_game_controller_ports_t *ports,
                              void *user_ctx);
void stc_game_controller_set_debug_flags(uint8_t flags);
void stc_game_controller_set_hand_online(bool online, uint32_t now_ms);

stc_err_t stc_game_controller_ui_action(stc_ui_action_t action,
                                        uint32_t now_ms);
stc_err_t stc_game_controller_on_label(const stc_event_t *event,
                                       const stc_burst_detail_t *detail,
                                       uint32_t now_ms);
stc_err_t stc_game_controller_on_drop(const stc_drop_t *drop,
                                      uint32_t round_id, uint32_t now_ms);
void stc_game_controller_on_scan_failed(stc_err_t error,
                                        uint32_t round_id, uint32_t now_ms);
void stc_game_controller_on_hand_ack(uint32_t round_id,
                                     stc_hand_action_t action, bool success,
                                     uint32_t now_ms);
void stc_game_controller_tick(uint32_t now_ms);

const stc_game_view_t *stc_game_controller_view(void);
stc_err_t stc_game_controller_clear_fault(uint32_t now_ms);
void stc_game_controller_set_fault(const char *code, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
