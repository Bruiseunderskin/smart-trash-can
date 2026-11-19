#include <stdint.h>

#include "bsp/esp32_s31_korvo_1.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "stc_ui.h"

static const char *TAG = "stc_ui_smoke";
static stc_ui_t *g_ui;

static void action_cb(stc_action_t action, int category, void *user)
{
    (void)user;
    ESP_LOGI(TAG, "UI_ACTION action=%d category=%d",
             (int)action, category);
}

static void retry_cb(void *user)
{
    (void)user;
    ESP_LOGI(TAG, "UI_ACTION retry");
}

static stc_payload_t payload(stc_state_t state, int category)
{
    return (stc_payload_t){
        .state = state,
        .category_id = category,
        .confidence = 0.93f,
        .candidates = {{category, 0.52f}, {3, 0.43f}},
        .score = 40,
        .streak = 2,
        .lid_open = true,
        .error_code = "CAMERA_OFFLINE",
        .round = 1,
        .rounds_total = 5,
    };
}

static void set_state(stc_state_t state)
{
    stc_payload_t snapshot = payload(STC_IDLE, 0);
    if (bsp_display_lock(100)) {
        if (stc_ui_state(g_ui) == STC_ERROR) {
            stc_ui_retry(g_ui);
        }
        (void)stc_ui_apply(g_ui, &snapshot);
        if (state != STC_IDLE) {
            snapshot = payload(STC_SCAN_READY, 0);
            (void)stc_ui_apply(g_ui, &snapshot);
        }
        if (state != STC_IDLE && state != STC_SCAN_READY) {
            snapshot = payload(STC_CAPTURING, 0);
            (void)stc_ui_apply(g_ui, &snapshot);
        }
        if (state == STC_CLASSIFYING || state == STC_RESULT_RETRY ||
            state == STC_RESULT_OK || state == STC_LEARNING ||
            state == STC_SUMMARY) {
            snapshot = payload(STC_CLASSIFYING, 0);
            (void)stc_ui_apply(g_ui, &snapshot);
        }
        snapshot = payload(state, 0);
        (void)stc_ui_apply(g_ui, &snapshot);
        if (state == STC_LEARNING) {
            stc_ui_show_learning(g_ui, 0);
        } else if (state == STC_SUMMARY) {
            stc_ui_show_summary(g_ui);
        }
        bsp_display_unlock();
    }
    ESP_LOGI(TAG, "UI_STATE state=%s", stc_ui_state_name(state));
}

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * 40,
        .double_buffer = false,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        },
    };
    lv_display_t *display = bsp_display_start_with_config(&cfg);
    if (display == NULL) {
        ESP_LOGE(TAG, "DISPLAY=FAIL");
        return;
    }
    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "DISPLAY_LOCK=FAIL");
        return;
    }
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_style_all(screen);
    lv_obj_t *root = lv_obj_create(screen);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 800, 480);
    g_ui = stc_ui_create(root, retry_cb, NULL);
    if (g_ui != NULL) {
        stc_ui_set_action_callback(g_ui, action_cb);
    }
    bsp_display_unlock();
    if (g_ui == NULL) {
        ESP_LOGE(TAG, "UI_CREATE=FAIL");
        return;
    }
    ESP_LOGI(TAG, "UI_CREATE=PASS display=800x480");

    const stc_state_t states[] = {
        STC_IDLE,
        STC_SCAN_READY,
        STC_CAPTURING,
        STC_CLASSIFYING,
        STC_RESULT_RETRY,
        STC_RESULT_OK,
        STC_LEARNING,
        STC_SUMMARY,
    };
    size_t index = 0;
    for (;;) {
        set_state(states[index]);
        index = (index + 1) % (sizeof(states) / sizeof(states[0]));
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
