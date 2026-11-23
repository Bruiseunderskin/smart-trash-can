#include "ui_port.h"

#include <string.h>

#include "bsp/esp32_s31_korvo_1.h"
#include "esp_log.h"
#include "lvgl.h"

#include "stc_app.h"
#include "stc_preprocess.h"
#include "ui_bridge.h"

static const char *TAG = "stc_ui_port";
static void *g_ui;
static stc_ui_action_sink_t g_sink;
static void *g_sink_ctx;
static uint16_t g_preview_pixels[2][160 * 160];
static int g_preview_index = -1;
static lv_image_dsc_t g_preview_desc[2] = {
    {
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .w = 160,
            .h = 160,
            .stride = 160 * 2,
        },
        .data_size = sizeof(g_preview_pixels[0]),
        .data = (const uint8_t *)g_preview_pixels[0],
    },
    {
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .w = 160,
            .h = 160,
            .stride = 160 * 2,
        },
        .data_size = sizeof(g_preview_pixels[1]),
        .data = (const uint8_t *)g_preview_pixels[1],
    },
};

static void action_callback(int32_t action, int32_t category, void *user_data)
{
    (void)user_data;
    if (g_sink) {
        g_sink((stc_ui_action_t)action, category, g_sink_ctx);
    }
}

static void retry_callback(void *user_data)
{
    (void)user_data;
    if (g_sink) {
        g_sink(STC_UI_ACTION_HOME, -1, g_sink_ctx);
    }
}

stc_err_t stc_game_ui_init(stc_ui_action_sink_t sink, void *ctx)
{
    g_sink = sink;
    g_sink_ctx = ctx;
    /* The BSP default allocates two 100-line DMA buffers in internal RAM.
     * That exceeds this board's remaining internal heap after Wi-Fi starts.
     * A single 40-line buffer stays DMA-capable and leaves RAM for ESP-DL. */
    bsp_display_cfg_t display_cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * 40,
        .double_buffer = false,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        },
    };
    lv_display_t *display = bsp_display_start_with_config(&display_cfg);
    if (!display) {
        return STC_ERR_IO;
    }
    (void)bsp_display_brightness_set(90);
    if (!bsp_display_lock(0)) {
        return STC_ERR_TIMEOUT;
    }
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_style_all(screen);
    lv_obj_t *root = lv_obj_create(screen);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 800, 480);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    g_ui = stc_ui_bridge_create(root, retry_callback, NULL);
    if (g_ui) {
        stc_ui_bridge_set_action_callback(g_ui, action_callback);
    }
    bsp_display_unlock();
    ESP_LOGI(TAG, "800x480 UI ready");
    return g_ui ? STC_OK : STC_ERR_IO;
}

void stc_game_ui_render(const stc_snapshot_t *snapshot,
                        const stc_game_view_t *view, void *ctx)
{
    (void)ctx;
    if (!g_ui || !snapshot || !view) {
        return;
    }
    stc_ui_payload_t payload;
    if (!stc_app_payload_from_state(snapshot, view, &payload)) {
        return;
    }
    stc_ui_bridge_payload_t bridge = {
        .state = (int32_t)payload.state,
        .category_id = payload.category_id,
        .confidence = payload.confidence,
        .score = payload.score,
        .streak = payload.streak,
        .lid_open = payload.lid_open,
        .error_code = payload.error_code,
        .round = payload.round,
        .rounds_total = payload.rounds_total,
        .debug_flags = payload.debug_flags,
        .reserved = payload.reserved,
        .scanned_bin = payload.learning.scanned_bin,
        .dropped_bin = payload.learning.dropped_bin,
        .correct_bin = payload.learning.correct_bin,
        .outcome = (int32_t)payload.learning.outcome,
        .round_delta = payload.learning.round_delta,
    };
    for (int i = 0; i < 2; ++i) {
        bridge.candidates[i].category_id = payload.candidates[i].category_id;
        bridge.candidates[i].p = payload.candidates[i].p;
    }
    if (bsp_display_lock(100)) {
        if (!stc_ui_bridge_apply(g_ui, &bridge)) {
            ESP_LOGW(TAG, "UI rejected state=%d error=%s",
                     (int)payload.state,
                     payload.error_code ? payload.error_code : "none");
        }
        bsp_display_unlock();
    }
}

void stc_game_ui_set_camera_rgb160(const uint8_t *rgb, uint16_t width,
                                   uint16_t height)
{
    if (!g_ui || !rgb || width != 160 || height != 160) {
        return;
    }
    const int next = g_preview_index < 0 ? 0 : 1 - g_preview_index;
    if (stc_preprocess_rgb_to_rgb565(rgb, 160 * 160,
                                     g_preview_pixels[next]) != STC_OK) {
        return;
    }
    if (bsp_display_lock(100)) {
        stc_ui_bridge_set_images(g_ui, &g_preview_desc[next], NULL);
        g_preview_index = next;
        bsp_display_unlock();
    }
}

void stc_game_ui_show_chat(bool voice_available)
{
    if (!g_ui) {
        return;
    }
    if (bsp_display_lock(100)) {
        stc_ui_bridge_show_chat(g_ui, voice_available);
        bsp_display_unlock();
    }
}

bool stc_game_ui_show_intro(void)
{
    bool shown = false;
    if (!g_ui) {
        return false;
    }
    if (bsp_display_lock(100)) {
        shown = stc_ui_bridge_show_intro(g_ui);
        bsp_display_unlock();
    }
    return shown;
}

void stc_game_ui_show_guide(void)
{
    if (!g_ui) {
        return;
    }
    if (bsp_display_lock(100)) {
        stc_ui_bridge_show_guide(g_ui);
        bsp_display_unlock();
    }
}

void stc_game_ui_deinit(void)
{
    if (!g_ui) {
        return;
    }
    if (bsp_display_lock(100)) {
        stc_ui_bridge_destroy(g_ui);
        g_ui = NULL;
        bsp_display_unlock();
    }
}
