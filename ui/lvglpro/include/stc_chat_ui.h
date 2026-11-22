#ifndef STC_CHAT_UI_H
#define STC_CHAT_UI_H

#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STC_CHAT_READY = 0,
    STC_CHAT_LISTENING,
    STC_CHAT_THINKING,
    STC_CHAT_ANSWERING,
    STC_CHAT_OFFLINE,
    STC_CHAT_ASR_RETRY,
    STC_CHAT_COUNTDOWN,
    STC_CHAT_NO_ANSWER,
    STC_CHAT_GOODBYE,
} stc_chat_state_t;

typedef enum {
    STC_CHAT_ACTION_PRIMARY = 0,
    STC_CHAT_ACTION_BACK,
} stc_chat_action_t;

typedef struct {
    stc_chat_state_t state;
    const char *heading;
    const char *message;
    const char *primary_label;
    const char *secondary_label;
} stc_chat_payload_t;

typedef struct stc_chat_ui stc_chat_ui_t;
typedef void (*stc_chat_action_cb)(stc_chat_action_t action, void *user_data);

stc_chat_ui_t *stc_chat_ui_create(lv_obj_t *parent,
                                  stc_chat_action_cb callback,
                                  void *user_data);
void stc_chat_ui_destroy(stc_chat_ui_t *ui);
bool stc_chat_ui_apply(stc_chat_ui_t *ui,
                       const stc_chat_payload_t *payload);
void stc_chat_ui_set_message_font(stc_chat_ui_t *ui,
                                  const lv_font_t *font);
stc_chat_state_t stc_chat_ui_state(const stc_chat_ui_t *ui);
bool stc_chat_ui_check_layout(stc_chat_ui_t *ui);

#ifdef __cplusplus
}
#endif

#endif
