/* stc_ui.h — 畫面（contract v1.1.0）
 *
 * UI 只做兩件事：讀 snapshot、畫。不得改分數、不得切模式、不得觸發硬件。
 * 佔位 UI 與隊友的正式 UI 都實作同一組函式，替換時只換 .c。
 *
 * v1.1：加入 PROMPT（揀卡）、SCAN（掃描中）、LEARNING（學習頁）三個畫面。
 */
#pragma once
#include "stc_game.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STC_UI_EV_CORRECT = 0,
    STC_UI_EV_WRONG,
    STC_UI_EV_TIMEOUT,
    STC_UI_EV_ABSTAIN,
    STC_UI_EV_MODE_CHANGED,   /* arg = stc_mode_t */
    STC_UI_EV_BIN_FAULT,      /* arg = fault mask */
    STC_UI_EV_ERROR,          /* arg = stc_err_t */
    STC_UI_EV_COUNT
} stc_ui_event_t;

typedef enum {
    STC_SCR_BOOT = 0,
    STC_SCR_IDLE,        /* 等 PLAY */
    STC_SCR_PROMPT,      /* 「揀一張卡，放喺相機前」 */
    STC_SCR_SCAN,        /* 掃描中（進度／轉圈，可顯示原始縮圖） */
    STC_SCR_RESULT,      /* 掃到嘅類別，等開蓋 */
    STC_SCR_WAIT_DROP,   /* 蓋已開，等放入 */
    STC_SCR_LEARNING,    /* 學習頁：對答案＋小知識＋分數變化 */
    STC_SCR_SUMMARY,     /* 5 回合／3 分鐘結算 */
    STC_SCR_CHAT,
    STC_SCR_FAULT,
    STC_SCR_COUNT
} stc_screen_t;

/* 學習頁內容（由 game 提供，UI 只負責排版） */
typedef struct {
    stc_bin_t scanned;      /* 模型判定 */
    stc_bin_t dropped;      /* 玩家投放 */
    stc_bin_t correct;      /* 正確桶（= scanned） */
    float     conf;
    uint8_t   agreement;    /* 0–5 */
    int16_t   delta;        /* 本回合加減分 */
    int16_t   score;        /* 累計 */
    bool      abstained;
    const char *tip;        /* 一句小知識（UTF-8，≤40 字） */
} stc_learning_t;

stc_err_t     stc_ui_init(void);
void          stc_ui_tick(const stc_snapshot_t *snap, uint32_t now_ms);
void          stc_ui_notify(stc_ui_event_t ev, int32_t arg);
void          stc_ui_learning(const stc_learning_t *info);  /* 進 LEARNING 前呼叫一次 */
stc_screen_t  stc_ui_screen(void);

#define STC_UI_PLACEHOLDER 1

#ifdef __cplusplus
}
#endif
