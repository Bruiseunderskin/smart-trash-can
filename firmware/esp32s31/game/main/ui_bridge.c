#include "ui_bridge.h"

#include <string.h>

/* Include the LVGL implementation header directly. The shared contract has
 * the same public base name but deliberately different payload and action
 * types. Keeping the boundary explicit avoids a compile-unit type clash. */
#include "../../../../ui/lvglpro/include/stc_ui.h"

static stc_ui_bridge_action_cb g_action;
static stc_ui_bridge_retry_cb g_retry;
static void *g_user;
static stc_ui_t *g_ui;

static void bridge_action(stc_action_t action, int category_id, void *ctx)
{
    (void)ctx;
    if (g_action) {
        g_action((int32_t)action, (int32_t)category_id, g_user);
    }
}

static void bridge_retry(void *ctx)
{
    (void)ctx;
    if (g_retry) {
        g_retry(g_user);
    }
}

void *stc_ui_bridge_create(void *parent, stc_ui_bridge_retry_cb retry,
                           void *ctx)
{
    g_action = NULL;
    g_retry = retry;
    g_user = ctx;
    g_ui = stc_ui_create((lv_obj_t *)parent,
                         retry ? bridge_retry : NULL, NULL);
    return g_ui;
}

void stc_ui_bridge_destroy(void *ui)
{
    stc_ui_destroy((stc_ui_t *)ui);
    g_action = NULL;
    g_retry = NULL;
    g_user = NULL;
    g_ui = NULL;
}

void stc_ui_bridge_set_action_callback(void *ui,
                                       stc_ui_bridge_action_cb callback)
{
    g_action = callback;
    stc_ui_set_action_callback((stc_ui_t *)ui, bridge_action);
}

bool stc_ui_bridge_apply(void *ui, const stc_ui_bridge_payload_t *payload)
{
    if (!payload) {
        return false;
    }
    stc_payload_t out;
    memset(&out, 0, sizeof out);
    out.state = (stc_state_t)payload->state;
    out.category_id = payload->category_id;
    out.confidence = payload->confidence;
    for (int i = 0; i < 2; ++i) {
        out.candidates[i].category_id = payload->candidates[i].category_id;
        out.candidates[i].p = payload->candidates[i].p;
    }
    out.score = payload->score;
    out.streak = payload->streak;
    out.lid_open = payload->lid_open;
    out.error_code = payload->error_code;
    out.round = payload->round;
    out.rounds_total = payload->rounds_total;
    out.debug_flags = payload->debug_flags;
    out.learned_scanned_bin = payload->scanned_bin;
    out.learned_dropped_bin = payload->dropped_bin;
    out.learned_correct_bin = payload->correct_bin;
    out.learned_outcome = payload->outcome;
    out.learned_round_delta = payload->round_delta;
    return stc_ui_apply((stc_ui_t *)ui, &out);
}

void stc_ui_bridge_set_images(void *ui, const void *original,
                              const void *cutout)
{
    stc_ui_set_images((stc_ui_t *)ui,
                      (const lv_image_dsc_t *)original,
                      (const lv_image_dsc_t *)cutout);
}

void stc_ui_bridge_show_chat(void *ui)
{
    (void)ui;
}

void stc_ui_bridge_show_guide(void *ui)
{
    stc_ui_show_guide((stc_ui_t *)ui);
}
