/* stc_ui.h — 畫面（contract v2.0.0）
 *
 * UI 只做兩件事：讀 payload、畫。不得改分數、不得切模式、不得觸發硬件。
 * v2.0.0：依 2026-11-08 決定 D1，對齊 ui/lvgl 的 create/apply/callback 模型。
 * 本檔不依賴 LVGL；ui/lvgl 把 stc_ui_payload_t 映射到自己的 stc_payload_t。
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STC_UI_IDLE = 0,
    STC_UI_CAPTURING,
    STC_UI_CLASSIFYING,
    STC_UI_RESULT_OK,
    STC_UI_RESULT_RETRY,
    STC_UI_ERROR,
    STC_UI_CATEGORY_GUIDE,
    STC_UI_LEARNING,
    STC_UI_SUMMARY,
    STC_UI_STATE_COUNT
} stc_ui_state_t;

typedef struct {
    int32_t category_id;
    float   p;
} stc_ui_candidate_t;

typedef struct {
    stc_ui_state_t      state;
    int32_t             category_id;
    float               confidence;
    stc_ui_candidate_t  candidates[2];
    uint32_t            score;
    uint32_t            streak;
    bool                lid_open;
    const char         *error_code;   /* 同步複製；只顯示有限 ASCII */
} stc_ui_payload_t;

typedef enum {
    STC_UI_ACTION_START = 0,
    STC_UI_ACTION_HOME,
    STC_UI_ACTION_GUIDE,
    STC_UI_ACTION_LEARN,
    STC_UI_ACTION_FINISH,
    STC_UI_ACTION_ASK,
    STC_UI_ACTION_COUNT
} stc_ui_action_t;

typedef void (*stc_ui_action_cb)(stc_ui_action_t action, int32_t category_id, void *user_data);
typedef void (*stc_ui_retry_cb)(void *user_data);

typedef struct stc_ui stc_ui_t;   /* opaque；實作在 ui/lvgl */

/* parent 在 LVGL 實作內是 lv_obj_t*；契約層用 void* 以免依賴 LVGL。 */
stc_ui_t   *stc_ui_create(void *parent, stc_ui_retry_cb retry, void *user_data);
void        stc_ui_destroy(stc_ui_t *ui);
void        stc_ui_set_action_callback(stc_ui_t *ui, stc_ui_action_cb cb);
bool        stc_ui_apply(stc_ui_t *ui, const stc_ui_payload_t *payload);
void        stc_ui_retry(stc_ui_t *ui);
void        stc_ui_show_guide(stc_ui_t *ui);
void        stc_ui_show_learning(stc_ui_t *ui, int32_t category_id);
void        stc_ui_show_summary(stc_ui_t *ui);
void        stc_ui_set_reduced_motion(stc_ui_t *ui, bool reduced);
stc_ui_state_t stc_ui_state(const stc_ui_t *ui);
bool        stc_ui_check_layout(const stc_ui_t *ui);
const char *stc_ui_state_name(stc_ui_state_t state);

#ifdef __cplusplus
}
#endif
