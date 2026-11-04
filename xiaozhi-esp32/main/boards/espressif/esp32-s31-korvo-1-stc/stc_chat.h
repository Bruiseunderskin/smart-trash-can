#ifndef _STC_CHAT_H_
#define _STC_CHAT_H_

#ifdef __cplusplus
extern "C" {
#endif

// Queue a dynamic question. The request is serialized with any active speech.
void stc_chat_send_text(const char* text);

// Queue a phrase for read-aloud delivery. This uses a constrained Q fallback
// because the official WebSocket protocol has no device-to-server TTS message.
void stc_chat_speak_text(const char* text);

// Return a phrase from assets/phrases.json. Missing keys and missing assets
// return the caller-provided fallback, or an empty string when fallback is null.
const char* stc_phrase_get(const char* key, const char* fallback);

#ifdef __cplusplus
}

void stc_chat_init();
#endif

#endif  // _STC_CHAT_H_
