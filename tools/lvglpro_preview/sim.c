/* Behavioural simulation of the ui/lvglpro state machine, no hardware.
 *
 * Checks what rendering cannot: that the 3 s countdowns fire the right action
 * and that the 綠豆 chat state is reachable from the result and summary
 * screens. A failure here means a tap or a timer does nothing on device.
 *
 * Built by tools/lvglpro_preview/build.sh into build/sim.
 */
#include "lvgl.h"
#include "stc_ui.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static lv_display_t *disp;
static int got_scan, got_next;

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *p) {
    (void)a;
    (void)p;
    lv_display_flush_ready(d);
}

static void on_action(stc_action_t action, int category, void *user) {
    (void)category;
    (void)user;
    if (action == STC_ACTION_SCAN) {
        got_scan++;
    }
    if (action == STC_ACTION_NEXT) {
        got_next++;
    }
}

static void pump(int ms) {
    for (int t = 0; t < ms; t += 10) {
        lv_tick_inc(10);
        lv_timer_handler();
    }
}

static stc_payload_t base(stc_state_t state) {
    stc_payload_t p;
    memset(&p, 0, sizeof p);
    p.state = state;
    p.round = 1;
    p.rounds_total = 5;
    p.category_id = 1;
    p.confidence = 0.9f;
    p.candidates[0].category_id = 1;
    p.candidates[0].p = 0.9f;
    p.candidates[1].category_id = 0;
    p.candidates[1].p = 0.05f;
    return p;
}

int main(void) {
    lv_init();
    disp = lv_display_create(800, 480);
    static uint32_t buf[800 * 480];
    lv_display_set_buffers(disp, buf, NULL, sizeof buf,
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, 800, 480);
    lv_screen_load(scr);

    stc_ui_t *ui = stc_ui_create(scr, NULL, NULL);
    assert(ui);
    stc_ui_set_action_callback(ui, on_action);

    stc_payload_t p = base(STC_IDLE);
    assert(stc_ui_apply(ui, &p));
    p.state = STC_INTRO;
    assert(stc_ui_apply(ui, &p));
    assert(stc_ui_state(ui) == STC_INTRO);
    printf("idle: INTRO reachable\n");
    p.state = STC_SCAN_READY;
    assert(stc_ui_apply(ui, &p));
    pump(3200);
    assert(got_scan == 1);
    printf("scan_ready: SCAN after 3 s\n");

    p.state = STC_CAPTURING;
    assert(stc_ui_apply(ui, &p));
    p.state = STC_CLASSIFYING;
    assert(stc_ui_apply(ui, &p));
    p.state = STC_RESULT_OK;
    assert(stc_ui_apply(ui, &p));
    pump(3200);
    assert(got_next == 1);
    printf("result_ok: NEXT after 3 s\n");

    stc_chat_payload_t chat;
    memset(&chat, 0, sizeof chat);
    chat.state = STC_CHAT_READY;
    stc_ui_show_ask(ui, &chat);
    assert(stc_ui_state(ui) == STC_ASK);
    printf("result_ok: 綠豆 reachable\n");

    p.state = STC_SUMMARY;
    assert(stc_ui_apply(ui, &p));
    stc_ui_show_ask(ui, &chat);
    assert(stc_ui_state(ui) == STC_ASK);
    printf("summary: 綠豆 reachable\n");

    puts("SIM OK");
    return 0;
}
