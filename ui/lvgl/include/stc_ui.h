#ifndef STC_UI_H
#define STC_UI_H
#include "lvgl.h"
#include "labels.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* LVGL 9.5; call every function on the LVGL task or under lvgl_port_lock(). */
typedef enum { STC_IDLE, STC_CAPTURING, STC_CLASSIFYING, STC_RESULT_OK,
               STC_RESULT_RETRY, STC_ERROR, STC_CATEGORY_GUIDE, STC_LEARNING, STC_SUMMARY,
               STC_SCAN_READY, STC_BIN_CHOICE, STC_CHAT } stc_state_t;
typedef struct { int category_id; float p; } stc_candidate_t;
typedef enum { STC_ROUND_NONE, STC_ROUND_CORRECT, STC_ROUND_WRONG, STC_ROUND_TIMEOUT,
               STC_ROUND_ABSTAIN, STC_ROUND_NO_SCAN } stc_round_outcome_t;
typedef struct {
    int scanned_bin, dropped_bin, correct_bin;
    stc_round_outcome_t outcome;
    int16_t round_delta;
} stc_learning_t;
enum { STC_DEBUG_REPLAY = 1, STC_DEBUG_VIRTUAL_HAND = 2,
       STC_DEBUG_LIVE_CAMERA = 4, STC_DEBUG_DEV_FLOW = 8,
       STC_DEBUG_TOUCH_BINS = 16, STC_DEBUG_REAL_HAND = 32,
       STC_DEBUG_MODEL_WARN = 64 };
typedef struct {
    stc_state_t state;
    int category_id;
    float confidence;
    stc_candidate_t candidates[2];
    uint32_t score, streak;
    bool lid_open;
    const char *error_code; /* copied synchronously; shown as bounded ASCII */
    uint8_t round, rounds_total, debug_flags, reserved;
    stc_learning_t learning;
} stc_payload_t;
typedef struct stc_ui stc_ui_t;
typedef enum { STC_ACTION_START, STC_ACTION_HOME, STC_ACTION_GUIDE,
               STC_ACTION_LEARN, STC_ACTION_FINISH,
               STC_ACTION_ASK, STC_ACTION_SCAN, STC_ACTION_NEXT,
               STC_ACTION_SELECT_BIN } stc_action_t;
typedef void (*stc_action_cb)(stc_action_t action, int category_id, void *user_data);
/* Actions are intents only: the controller validates round_id, discards stale work,
 * then applies the accepted state. A callback may synchronously apply/update images.
 * Do not destroy the UI inside callbacks; defer destruction until callback returns. */
void stc_ui_set_action_callback(stc_ui_t *ui, stc_action_cb callback);
/* Extension learning is available only after result_ok -> summary.
 * Its single next-round action emits STC_ACTION_START for controller approval. */
void stc_ui_show_guide(stc_ui_t *ui);
void stc_ui_show_learning(stc_ui_t *ui, int category_id);
void stc_ui_show_summary(stc_ui_t *ui);
void stc_ui_show_chat(stc_ui_t *ui);
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
