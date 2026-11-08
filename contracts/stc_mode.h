/* stc_mode.h — 模式閘（contract v1.1.0）
 *
 * 唯一有權改「而家係遊戲定聊天」的地方。
 * 其他模組只可以讀 stc_mode_get()／stc_mode_layer()，不得自行切換。
 *
 * 為何要這一層：小智的 kDeviceState 在 main/ 出現 160 處，改它等於每次同步上游都要重貼。
 * 所以我們不改它，改為在自己這一層做閘；小智只在 enter_chat 之後才被允許發聲。
 */
#pragma once
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STC_ST_BOOT = 0,
    STC_ST_GAME_IDLE,   /* 等 PLAY；唯一可以開新局的地方 */
    STC_ST_GAME_ROUND,  /* 回合進行中；禁止進 CHAT */
    STC_ST_SUMMARY,     /* 結算畫面；唯一可以進 CHAT 的地方 */
    STC_ST_CHAT,        /* 小智原生語音；遊戲暫停 */
    STC_ST_FAULT,       /* 契約／硬件錯誤；只可 RESET 離開 */
    STC_ST_COUNT
} stc_state_t;

typedef enum {
    STC_IN_PLAY = 0,    /* PLAY 短按 */
    STC_IN_CHAT_ENTER,  /* 分類圖鑑「問綠豆」；CHAT 的唯一入口 */
    STC_IN_ROUND_END,   /* 回合結束（由 stc_game 送） */
    STC_IN_SET_LONG,    /* SET 長按 ≥800 ms */
    STC_IN_CHAT_TIMEOUT,/* CHAT 閒置 60 s */
    STC_IN_CHAT_EXIT,   /* CHAT 內按 SET 或語音說「唔玩」 */
    STC_IN_FAULT,
    STC_IN_RESET,
    STC_IN_COUNT
} stc_input_t;

typedef struct {
    void (*enter_game)(void);   /* 關喚醒、關音頻通道、停麥克風上行 */
    void (*enter_chat)(void);   /* 開喚醒或直接進聆聽 */
    void (*exit_chat)(void);    /* 收尾：停播放、回 GAME 前置 */
    void (*enter_round)(void);
    void (*exit_round)(void);
} stc_mode_hooks_t;

stc_err_t   stc_mode_init(void);
void        stc_mode_set_hooks(const stc_mode_hooks_t *hooks);

stc_state_t stc_mode_get(void);
stc_mode_t  stc_mode_layer(void);      /* GAME：BOOT/IDLE/ROUND/SUMMARY；CHAT：CHAT */
const char *stc_mode_state_name(stc_state_t s);

/* 唯一寫入者。非法轉移回 STC_ERR_NOT_READY 並保持原狀態，不得有任何副作用。 */
stc_err_t   stc_mode_input(stc_input_t in, uint32_t now_ms);
void        stc_mode_tick(uint32_t now_ms);   /* CHAT 60 s 閒置自動回 GAME */

bool stc_mode_allows_game(void);
bool stc_mode_allows_chat(void);

/* 診斷：GAME 模式下偵測到雲端連線時呼叫（不切狀態，只記錄）。 */
void stc_mode_note_cloud_activity(void);
uint16_t stc_mode_cloud_violations(void);

#ifdef __cplusplus
}
#endif
