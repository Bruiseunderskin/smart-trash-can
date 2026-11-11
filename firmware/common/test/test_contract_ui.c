/* test_contract_ui.c — contracts/stc_ui.h v2.0.0 的次序與佈局。

 * 對齊對象是 ui/lvgl/include/stc_ui.h（LVGL 實作），但本測試不引入 LVGL：
 * 只釘死契約自己的數值，跨檔一致由 tools/tests/test_contract_ui.py 逐名比對。
 */
#include <stddef.h>
#include <stdio.h>

#include "stc_ui.h"

_Static_assert(STC_UI_STATE_COUNT == 9, "nine UI states");
_Static_assert(STC_UI_ACTION_COUNT == 6, "six UI actions");

_Static_assert(STC_UI_IDLE == 0, "idle is zero");
_Static_assert(STC_UI_CAPTURING == 1, "capturing follows idle");
_Static_assert(STC_UI_CLASSIFYING == 2, "classifying follows capturing");
_Static_assert(STC_UI_RESULT_OK == 3, "result_ok after classifying");
_Static_assert(STC_UI_RESULT_RETRY == 4, "result_retry after result_ok");
_Static_assert(STC_UI_ERROR == 5, "error after retry");
_Static_assert(STC_UI_CATEGORY_GUIDE == 6, "guide after error");
_Static_assert(STC_UI_LEARNING == 7, "learning after guide");
_Static_assert(STC_UI_SUMMARY == 8, "summary last");

_Static_assert(STC_UI_ACTION_START == 0, "start is zero");
_Static_assert(STC_UI_ACTION_ASK == 5, "ask last");

_Static_assert(sizeof(((stc_ui_payload_t *)0)->state) == sizeof(int), "state is an int-sized enum");
_Static_assert(sizeof(((stc_ui_payload_t *)0)->candidates) == 2 * sizeof(stc_ui_candidate_t),
               "exactly two candidates");
_Static_assert(sizeof(((stc_ui_payload_t *)0)->error_code) == sizeof(const char *),
               "error_code is a borrowed pointer");

int main(void)
{
    stc_ui_payload_t payload = {0};
    const size_t state_off = offsetof(stc_ui_payload_t, state);
    const size_t category_off = offsetof(stc_ui_payload_t, category_id);
    const size_t conf_off = offsetof(stc_ui_payload_t, confidence);
    const size_t cand_off = offsetof(stc_ui_payload_t, candidates);
    const size_t score_off = offsetof(stc_ui_payload_t, score);
    const size_t streak_off = offsetof(stc_ui_payload_t, streak);
    const size_t lid_off = offsetof(stc_ui_payload_t, lid_open);
    const size_t err_off = offsetof(stc_ui_payload_t, error_code);

    if (!(state_off < category_off && category_off < conf_off && conf_off < cand_off &&
          cand_off < score_off && score_off < streak_off && streak_off < lid_off &&
          lid_off < err_off)) {
        fprintf(stderr, "payload field order changed\n");
        return 1;
    }

    payload.state = STC_UI_RESULT_OK;
    payload.category_id = 1;
    payload.lid_open = true;
    if (payload.state != STC_UI_RESULT_OK || payload.category_id != 1 || !payload.lid_open) {
        fprintf(stderr, "payload round-trip failed\n");
        return 1;
    }

    printf("test_contract_ui: %d states, %d actions, payload %zu bytes\n",
           (int)STC_UI_STATE_COUNT, (int)STC_UI_ACTION_COUNT, sizeof(stc_ui_payload_t));
    return 0;
}
