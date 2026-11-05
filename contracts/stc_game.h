/* stc_game.h — 遊戲核心（contract v1.0.0）
 * 唯一可以改分數與回合的地方。其他模組只出事件。 */
#pragma once
#include "stc_types.h"

#define STC_GAME_CONTRACT_VERSION_MAJOR 1
#define STC_GAME_CONTRACT_VERSION_MINOR 0
#define STC_GAME_CONTRACT_VERSION_PATCH 0
#define STC_GAME_CONTRACT_VERSION_STRING "1.0.0"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t round;        /* 0-based */
    uint8_t rounds_total; /* 預設 5 */
    int16_t score;
    stc_bin_t truth;      /* 本回合標籤 */
    bool lid_open;
    bool voice_busy;
} stc_snapshot_t;

stc_err_t stc_game_init(void);
stc_err_t stc_game_start(void);
void      stc_game_tick(uint32_t now_ms);
stc_err_t stc_game_input(const stc_event_t *ev);
stc_snapshot_t stc_game_snapshot(void);

#ifdef __cplusplus
}
#endif
