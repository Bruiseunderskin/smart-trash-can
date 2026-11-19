#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stc_hand_msg.h"
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

stc_err_t stc_game_espnow_init(uint8_t channel);
stc_err_t stc_game_espnow_suspend(void);
stc_err_t stc_game_espnow_resume(uint8_t channel);
int stc_game_espnow_send(const stc_hand_msg_t *msg, void *ctx);
bool stc_game_espnow_poll(stc_hand_msg_t *msg, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
