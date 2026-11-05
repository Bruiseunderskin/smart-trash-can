/* stc_game.h — 遊戲核心（contract v1.1.0）
 * 唯一可以改分數與回合的地方。其他模組只出事件。 */
#pragma once
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 回合內階段。與 stc_mode 的 GAME_ROUND 不同層：
 * mode 講「可不可以聊天」，phase 講「回合做到邊一步」。 */
typedef enum {
    STC_PH_IDLE = 0,
    STC_PH_CAPTURE,     /* 連拍 5 幀＋5 次推論，上限 1200 ms */
    STC_PH_OPEN,        /* 四蓋錯開開啟，上限 1.5 s */
    STC_PH_WAIT_DROP,   /* 等投放，上限 15 s */
    STC_PH_SETTLE,      /* 收到 DROP 後 300 ms 去抖 */
    STC_PH_CLOSE,       /* 全部關蓋，上限 1.5 s */
    STC_PH_FEEDBACK,    /* 1.2 s 顯示結果 */
    STC_PH_SUMMARY,     /* 全部回合完結 */
    STC_PH_COUNT
} stc_phase_t;

typedef struct {
    uint8_t  rounds_total;      /* 預設 5 */
    uint16_t capture_timeout_ms;/* 1200（5 幀連拍 × 50 ms ＋ 5 次推論） */
    uint16_t open_timeout_ms;   /* 1500 */
    uint16_t wait_drop_ms;      /* 15000 */
    uint16_t settle_ms;         /* 300 */
    uint16_t close_timeout_ms;  /* 1500 */
    uint16_t feedback_ms;       /* 1200 */
    int16_t  score_correct;     /* +10 */
    int16_t  score_wrong;       /* -5  */
    int16_t  score_floor;       /* 0（不顯示負分） */
    uint8_t  max_retry;         /* 1（abstain 時重抓一次） */
} stc_game_config_t;

typedef struct {
    uint8_t       round;        /* 0-based */
    uint8_t       rounds_total;
    int16_t       score;
    stc_phase_t   phase;
    stc_bin_t     truth;        /* 本回合標籤 */
    stc_bin_t     dropped;      /* 玩家實際投放（無則 = STC_BIN_COUNT） */
    bool          lid_open;
    bool          voice_busy;
    bool          abstained;    /* 本回合用了 abstain 路徑 */
    uint16_t      dist_mm;      /* 最近一次 ToF 讀數，0 = 無 */
    stc_source_t  src;          /* STUB / REAL，供 UI 角落顯示 */
} stc_snapshot_t;

stc_err_t       stc_game_init(void);
stc_err_t       stc_game_set_config(const stc_game_config_t *cfg);
stc_game_config_t stc_game_get_config(void);

stc_err_t       stc_game_start(void);
void            stc_game_tick(uint32_t now_ms);
stc_err_t       stc_game_input(const stc_event_t *ev);
stc_err_t       stc_game_reset(void);        /* 回 IDLE，清本局 */
stc_snapshot_t  stc_game_snapshot(void);

/* 最近一局的結果（結算畫面與持久化用） */
typedef struct {
    uint8_t  rounds_played;
    uint8_t  correct;
    uint8_t  wrong;
    uint8_t  timed_out;
    uint8_t  abstained;
    int16_t  score;
    uint32_t duration_ms;
} stc_result_t;
stc_result_t    stc_game_result(void);

#ifdef __cplusplus
}
#endif
