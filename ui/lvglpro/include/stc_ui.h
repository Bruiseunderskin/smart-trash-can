#ifndef STC_UI_H
#define STC_UI_H
#include "lvgl.h"
#include "labels.h"
#include "stc_chat_ui.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* LVGL 9.5; call every function on the LVGL task or under lvgl_port_lock(). */
typedef enum { STC_IDLE, STC_CAPTURING, STC_CLASSIFYING, STC_RESULT_OK,
               STC_RESULT_RETRY, STC_ERROR, STC_CATEGORY_GUIDE, STC_LEARNING, STC_SUMMARY,
               STC_SCAN_READY, STC_BIN_CHOICE, STC_ASK, STC_INTRO } stc_state_t;
typedef struct { int category_id; float p; } stc_candidate_t;
typedef struct {
    stc_state_t state;
    int category_id;
    float confidence;
    stc_candidate_t candidates[2];
    uint32_t score, streak;
    bool lid_open;
    const char *error_code; /* copied synchronously; shown as bounded ASCII */
    uint8_t round, rounds_total;
    uint8_t debug_flags;
    int learned_scanned_bin;
    int learned_dropped_bin;
    int learned_correct_bin;
    int learned_outcome;
    int learned_round_delta;
} stc_payload_t;
typedef struct stc_ui stc_ui_t;
enum {
    STC_UI_DEBUG_TOUCH_BINS = 1u << 4,
    STC_UI_DEBUG_REAL_HAND = 1u << 5,
};
typedef enum {
    STC_ACTION_START = 0,
    STC_ACTION_HOME = 1,
    STC_ACTION_GUIDE = 2,
    STC_ACTION_LEARN = 3,
    STC_ACTION_FINISH = 4,
    STC_ACTION_ASK = 5,
    STC_ACTION_SCAN = 6,
    STC_ACTION_NEXT = 7,
    STC_ACTION_SELECT_BIN = 8,
} stc_action_t;
typedef void (*stc_action_cb)(stc_action_t action, int category_id, void *user_data);
/* Actions are intents only: the controller validates round_id, discards stale work,
 * then applies the accepted state. A callback may synchronously apply/update images.
 * Do not destroy the UI inside callbacks; defer destruction until callback returns. */
void stc_ui_set_action_callback(stc_ui_t *ui, stc_action_cb callback);
/* Extension learning is available only after result_ok -> summary.
 * Its single next-round action emits STC_ACTION_NEXT for controller approval. */
void stc_ui_show_guide(stc_ui_t *ui);
void stc_ui_show_learning(stc_ui_t *ui, int category_id);
void stc_ui_show_summary(stc_ui_t *ui);
/* 綠豆 chat: the controller owns the conversation state. This call switches
 * the UI to STC_ASK and renders one chat screen. Primary / back buttons emit
 * STC_ACTION_ASK / STC_ACTION_HOME for the controller to act on. */
void stc_ui_show_ask(stc_ui_t *ui, const stc_chat_payload_t *payload);
typedef void (*stc_retry_cb)(void *user_data);
/* Returns NULL for an incompatible parent size. UI owns its child container. */
stc_ui_t *stc_ui_create(lv_obj_t *parent, stc_retry_cb retry, void *user_data);
/* Destroy before deleting the parent when possible. If the parent was deleted,
 * the handle is inert; call destroy once to release the remaining context. */
void stc_ui_destroy(stc_ui_t *ui);
/* Strict transition/field validation; invalid input renders a visible error. */
bool stc_ui_apply(stc_ui_t *ui, const stc_payload_t *payload);
stc_state_t stc_ui_state_from_string(const char *state); /* unknown => error */
const char *stc_ui_state_name(stc_state_t state);
stc_state_t stc_ui_state(const stc_ui_t *ui);
/* Only explicit user retry acknowledges an error (once per error); controller can then apply idle. */
void stc_ui_retry(stc_ui_t *ui);
void stc_ui_set_reduced_motion(stc_ui_t *ui, bool reduced);
/* Persistent image descriptor/pixels remain owned by caller until replaced.
 * Set cutout=NULL to use original; both NULL shows camera-unavailable placeholder.
 * RGB565 camera buffers must remain valid and must not be written during drawing. */
void stc_ui_set_images(stc_ui_t *ui, const lv_image_dsc_t *original,
                       const lv_image_dsc_t *cutout);
const char *stc_category_label(int id);
/* Native acceptance diagnostics; no hardware calls. */
bool stc_ui_check_layout(stc_ui_t *ui);
#ifdef __cplusplus
}
#endif
#endif
