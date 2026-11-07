/* stc_game.h — 遊戲核心（contract v1.2.0）
 * 唯一可以改分數與回合的地方。其他模組只出事件。 */
#pragma once
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 回合內階段。
 * 流程：揀卡 → 掃描 → 顯示結果 → 四蓋全開 → 等投放 → 去抖 → 關蓋 → 學習頁 → 下一回合 */
typedef enum {
    STC_PH_IDLE = 0,
    STC_PH_PROMPT,      /* 請玩家揀一張卡並放喺相機前，上限 20 s */
    STC_PH_SCAN,        /* 開相機＋連拍 5 幀＋推論，上限 1.5 s */
    STC_PH_RESULT,      /* 顯示掃到嘅類別，1.5 s */
    STC_PH_OPEN,        /* 四蓋錯開開啟，上限 1.5 s */
    STC_PH_WAIT_DROP,   /* 等卡入桶，上限 15 s */
    STC_PH_SETTLE,      /* 收到 DROP 後 300 ms 去抖 */
    STC_PH_CLOSE,       /* 全部關蓋，上限 1.5 s */
    STC_PH_LEARNING,    /* 學習頁：對答案＋小知識，6 s 或按鍵跳過 */
    STC_PH_SUMMARY,     /* 結算（5 回合或 3 分鐘到） */
    STC_PH_COUNT
} stc_phase_t;

typedef struct {
    uint8_t  rounds_total;       /* 5 */
    uint32_t total_time_ms;      /* 180000（3 分鐘硬上限，超時做完本回合即結算） */
    uint16_t prompt_timeout_ms;  /* 20000 */
    uint16_t scan_timeout_ms;    /* 1500 */
    uint16_t result_ms;          /* 1500 */
    uint16_t open_timeout_ms;    /* 1500 */
    uint16_t wait_drop_ms;       /* 15000 */
    uint16_t settle_ms;          /* 300 */
    uint16_t close_timeout_ms;   /* 1500 */
    uint16_t learning_ms;        /* 6000 */
    int16_t  score_correct;      /* +10 */
    int16_t  score_wrong;        /* -5  */
    int16_t  score_floor;        /* 0（唔顯示負分） */
    uint8_t  max_retry;          /* 1（abstain 時重掃一次） */
} stc_game_config_t;

typedef struct {
    uint8_t       round;         /* 0-based */
    uint8_t       rounds_total;
    int16_t       score;
    stc_phase_t   phase;
    uint32_t      elapsed_ms;    /* 本局已用時間 */
    stc_bin_t     truth;         /* 本回合模型判定類別 */
    stc_bin_t     dropped;       /* 玩家實際投放（無則 = STC_BIN_COUNT） */
    float         conf;          /* 聚合置信度 */
    uint8_t       agreement;     /* 5 幀中與最終結果一致嘅幀數 */
    bool          lid_open;
    bool          voice_busy;
    bool          abstained;
    uint16_t      dist_mm;
    stc_source_t  src;           /* STUB / REAL，UI 角落顯示 */
} stc_snapshot_t;

stc_err_t         stc_game_init(void);
stc_err_t         stc_game_set_config(const stc_game_config_t *cfg);
stc_game_config_t stc_game_get_config(void);

stc_err_t         stc_game_start(void);
void              stc_game_tick(uint32_t now_ms);
stc_err_t         stc_game_input(const stc_event_t *ev);
stc_err_t         stc_game_reset(void);
stc_snapshot_t    stc_game_snapshot(void);

typedef struct {
    uint8_t  rounds_played;
    uint8_t  correct;
    uint8_t  wrong;
    uint8_t  timed_out;
    uint8_t  abstained;
    int16_t  score;          /* 顯示用（已套 floor） */
    int16_t  score_raw;      /* 真實加總（報告用） */
    uint32_t duration_ms;
    bool     ended_by_time;  /* true = 3 分鐘到 */
} stc_result_t;
stc_result_t      stc_game_result(void);

#ifdef __cplusplus
}
#endif
