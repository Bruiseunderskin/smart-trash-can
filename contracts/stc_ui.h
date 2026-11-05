/* stc_ui.h — 畫面（contract v1.0.0）
 *
 * UI 只做兩件事：讀 snapshot、畫。不得改分數、不得切模式、不得觸發硬件。
 * 佔位 UI 與隊友的正式 UI 都實作同一組函式，替換時只換 .c，不動其他模組。
 */
#pragma once
#include "stc_game.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STC_UI_EV_CORRECT = 0,   /* 答對 */
    STC_UI_EV_WRONG,         /* 答錯 */
    STC_UI_EV_TIMEOUT,       /* 超時 */
    STC_UI_EV_ABSTAIN,       /* 睇唔清，重試 */
    STC_UI_EV_MODE_CHANGED,  /* arg = stc_mode_t */
    STC_UI_EV_BIN_FAULT,     /* arg = fault mask */
    STC_UI_EV_ERROR,         /* arg = stc_err_t */
    STC_UI_EV_COUNT
} stc_ui_event_t;

typedef enum {
    STC_SCR_BOOT = 0,     /* 開機／自我檢查 */
    STC_SCR_IDLE,         /* 等待 PLAY */
    STC_SCR_CAPTURE,      /* 辨識中（可顯示相機縮圖或轉圈） */
    STC_SCR_RESULT,       /* 顯示預測類別，等投放 */
    STC_SCR_WAIT_DROP,    /* 蓋已開，等物件入桶 */
    STC_SCR_FEEDBACK,     /* 答對／答錯 1.2 s */
    STC_SCR_SUMMARY,      /* 5 回合結算 */
    STC_SCR_CHAT,         /* CHAT 模式指示 */
    STC_SCR_FAULT,        /* 故障 */
    STC_SCR_COUNT
} stc_screen_t;

/* 初始化畫面（建立 LVGL 物件，只做一次） */
stc_err_t stc_ui_init(void);

/* 每 tick 呼叫；內部按 snapshot 決定畫面，不得阻塞 > 16 ms */
void stc_ui_tick(const stc_snapshot_t *snap, uint32_t now_ms);

/* 顯示一次性事件（答對／答錯／錯誤訊息／模式改變提示） */
void stc_ui_notify(stc_ui_event_t ev, int32_t arg);

/* 目前畫面（測試用） */
stc_screen_t stc_ui_screen(void);

/* 佔位 UI 必須實作：只在左上角顯示 state/round/score，不放品牌、不做動畫 */
#define STC_UI_PLACEHOLDER 1

#ifdef __cplusplus
}
#endif
