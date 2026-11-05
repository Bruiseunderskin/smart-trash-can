#pragma once

#include "stc_game.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_GAME_RESULT_MS 1000U
#define STC_GAME_DROP_TIMEOUT_MS 10000U
#define STC_GAME_START_MS 1U
#define ESTIMATE_TIMEOUT_MS(start_ms) ((start_ms) + STC_GAME_DROP_TIMEOUT_MS + 1U)

typedef enum {
    STC_GAME_IDLE = 0,
    STC_GAME_CAPTURE,
    STC_GAME_WAIT_DROP,
    STC_GAME_ROUND_RESULT,
    STC_GAME_SETTLEMENT,
} stc_game_phase_t;

stc_game_phase_t stc_game_phase(void);
int8_t stc_game_last_delta(void);
bool stc_game_can_enter_chat(void);

#ifdef __cplusplus
}
#endif
