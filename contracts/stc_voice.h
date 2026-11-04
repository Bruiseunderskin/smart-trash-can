/* stc_voice.h — 語音：本地台詞 ＋ CHAT 模式對話（contract v1.1.0）
 *
 * 後端有兩種，由 stc_voice_set_mode() 決定：
 *   GAME 模式 → 本地預錄 PCM（無網、必然可用）
 *   CHAT 模式 → 小智語音通路（喚醒詞／按鍵 → ASR → LLM → TTS）
 *
 * 契約要點：
 *   - GAME 模式不得開啟任何雲端連線
 *   - 後端不支援的能力回 STC_ERR_UNSUPPORTED，不得假裝成功
 *   - 本模組不得改分數、動舵機、開關相機
 */
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

/* 初始化：載入台詞表、初始化 codec、準備兩個後端（不建立任何連線） */
stc_err_t stc_voice_init(void);

/* 播預錄台詞（本地，非阻塞，入佇列）。任何模式都可用。 */
stc_err_t stc_voice_say(stc_line_t line);

/* GAME/CHAT 閘。GAME：關喚醒、關通道；CHAT：開喚醒／或按鍵說話。 */
stc_err_t   stc_voice_set_mode(stc_mode_t mode);
stc_mode_t  stc_voice_get_mode(void);

/* CHAT 模式：開始一次對話（等喚醒詞或直接進聆聽）。GAME 模式回 STC_ERR_NOT_READY。 */
stc_err_t stc_voice_start_dialog(void);

/* 文字 → 語音。後端支援時才可用（xiaozhi 文字注入未實作 → 回 STC_ERR_UNSUPPORTED）。 */
stc_err_t stc_voice_speak_text(const char *text);

/* 文字 → LLM → 回覆文字。同上，後端不支援即回 STC_ERR_UNSUPPORTED。 */
stc_err_t stc_voice_ask(const char *text, char *reply_out, size_t reply_len,
                        uint32_t timeout_ms);

void stc_voice_poll(void);      /* 每 tick 呼叫 */
bool stc_voice_is_busy(void);
void stc_voice_stop(void);

#ifdef __cplusplus
}
#endif
