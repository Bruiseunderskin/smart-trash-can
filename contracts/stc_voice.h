/* stc_voice.h — 語音播報與對話（contract v1.0.0）
 * 這是 AI chat 的主體：本地台詞、模式 S（say）、模式 Q（ask）、GAME/CHAT 閘。
 * 不得：改分數、動舵機、開關相機。 */
#pragma once
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STC_LINE_OPEN = 0,      /* 請放入任何一個桶 */
    STC_LINE_CORRECT,       /* 答對 */
    STC_LINE_WRONG,         /* 答錯 */
    STC_LINE_TIMEOUT,       /* 冇投入 */
    STC_LINE_RETRY,         /* 睇唔清，再試 */
    STC_LINE_SUMMARY,       /* 結算 */
    STC_LINE_COUNT
} stc_line_t;

stc_err_t stc_voice_init(void);

/* 播預錄台詞（本地，非阻塞，入佇列）。GAME 模式唯一允許的發聲方式。 */
stc_err_t stc_voice_say(stc_line_t line);

/* 模式 S：文字 → TTS（B1 /say），非阻塞，回傳佇列位置。 */
stc_err_t stc_voice_speak_text(const char *text);

/* 模式 Q：文字 → LLM → TTS（B1 /ask）。阻塞直到回覆文字到手（音頻其後播放）。*/
stc_err_t stc_voice_ask(const char *text, char *reply_out, size_t reply_len,
                        uint32_t timeout_ms);

/* 主循環泵：處理播放佇列、HTTP 串流、逾時。每 tick 呼叫一次。 */
void stc_voice_poll(void);

/* GAME/CHAT 閘：GAME 關閉雲端通道與喚醒；CHAT 開啟。 */
stc_err_t stc_voice_set_mode(stc_mode_t mode);
stc_mode_t stc_voice_get_mode(void);

bool stc_voice_is_busy(void);   /* 正在播放或等待回覆 */
void stc_voice_stop(void);      /* 立即停止播放並清佇列 */

#ifdef __cplusplus
}
#endif
