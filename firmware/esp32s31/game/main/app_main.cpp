#include <cstring>
#include <cctype>
#include <fcntl.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "nvs_flash.h"

#include "bsp/esp32_s31_korvo_1.h"
#include "espnow_controller.h"
#include "camera_capture.h"
#include "local_voice.h"
#include "replay_classifier.hpp"
#include "stc_app.h"
#include "stc_game_dev_flow.h"
#include "stc_game_touch_profile.h"
#include "stc_game_controller.h"
#include "stc_hand_link.h"
#include "stc_preprocess.h"
#include "stc_shell_mode.h"
#include "stc_vision_aggregate.h"
#include "ui_port.h"

static const char *TAG = "stc_game";

#if STC_CAMERA_PROBE
extern "C" int stc_camera_probe_run(void);
#endif

typedef enum {
    GAME_EVENT_UI_ACTION = 0,
    GAME_EVENT_PLAY,
    GAME_EVENT_VISION_RESULT,
    GAME_EVENT_VISION_FAILED,
} game_event_type_t;

typedef struct {
    game_event_type_t type;
    stc_ui_action_t action;
    stc_event_t label;
    stc_burst_detail_t detail;
    stc_err_t error;
    uint32_t round_id;
    int32_t category_id;
} game_event_t;

typedef struct {
    uint8_t round;
    uint32_t round_id;
} vision_request_t;

static QueueHandle_t g_game_queue;
static QueueHandle_t g_vision_queue;
static stc_hand_transport_t g_transport;
static bool g_model_ready;
static uint8_t g_debug_flags;
static bool g_intro_pending;
static bool g_intro_seen;
#if STC_GAME_DEV_FLOW
static stc_game_dev_flow_t g_dev_flow;
static bool g_dev_summary_logged;
#endif

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

#if STC_GAME_LIVE_CAMERA && STC_GAME_DEV_FLOW
static void camera_smoke_test(void)
{
    if (stc_camera_capture_begin() != STC_OK) {
        ESP_LOGE(TAG, "LIVE_CAMERA_SMOKE=FAIL reason=START");
        return;
    }
    uint8_t *rgb = static_cast<uint8_t *>(
        heap_caps_malloc(STC_INPUT_SIZE * STC_INPUT_SIZE * 3,
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!rgb) {
        ESP_LOGE(TAG, "LIVE_CAMERA_SMOKE=FAIL reason=RGB_ALLOC");
        stc_camera_capture_end();
        return;
    }
    int ok = 0;
    for (int i = 0; i < STC_VISION_BURST_FRAMES; ++i) {
        vTaskDelay(pdMS_TO_TICKS(50));
        float probs[STC_MODEL_CLASSES] = {};
        if (stc_camera_capture_frame(rgb, 1000) == STC_OK &&
            stc_game_classify_rgb160(rgb, probs)) {
            ++ok;
            ESP_LOGI(TAG, "LIVE_CAMERA_FRAME=%d", i);
        } else {
            ESP_LOGW(TAG, "LIVE_CAMERA_FRAME=%d FAIL", i);
        }
    }
    stc_camera_capture_end();
    heap_caps_free(rgb);
    ESP_LOGI(TAG, "LIVE_CAMERA_SMOKE=%s frames=%d/%d",
             ok == STC_VISION_BURST_FRAMES ? "PASS" : "FAIL",
             ok, STC_VISION_BURST_FRAMES);
}
#endif

static bool post_game_event(const game_event_t *event)
{
    return g_game_queue &&
           xQueueSend(g_game_queue, event, pdMS_TO_TICKS(20)) == pdTRUE;
}

static void ui_action_sink(stc_ui_action_t action, int32_t category_id,
                           void *ctx)
{
    (void)ctx;
    game_event_t event = {};
    event.type = GAME_EVENT_UI_ACTION;
    event.action = action;
    event.category_id = category_id;
    (void)post_game_event(&event);
}

static void vision_task(void *arg)
{
    (void)arg;
#if STC_GAME_LIVE_CAMERA
    uint8_t *rgb = static_cast<uint8_t *>(
        heap_caps_malloc(STC_INPUT_SIZE * STC_INPUT_SIZE * 3,
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    bool camera_ready = rgb && stc_camera_capture_begin() == STC_OK;
    if (!camera_ready) {
        ESP_LOGE(TAG, "LIVE_CAMERA=FAIL reason=START");
    } else {
        ESP_LOGI(TAG, "LIVE_CAMERA=PREVIEW_READY 640x480_UYVY");
    }
#endif
    while (true) {
        vision_request_t request;
#if STC_GAME_LIVE_CAMERA
        if (xQueueReceive(g_vision_queue, &request, pdMS_TO_TICKS(80)) != pdTRUE) {
            if (camera_ready &&
                stc_camera_capture_frame(rgb, 1000) == STC_OK) {
                stc_game_ui_set_camera_rgb160(rgb, STC_INPUT_SIZE,
                                              STC_INPUT_SIZE);
            }
            continue;
        }
#else
        if (xQueueReceive(g_vision_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
#endif
        game_event_t event = {};
        event.type = GAME_EVENT_VISION_RESULT;
        event.round_id = request.round_id;
        event.label.type = STC_EV_LABEL;
        event.label.round_id = request.round_id;
        event.label.t_ms = now_ms();
        const int64_t started = esp_timer_get_time();
        stc_vision_frame_t frames[STC_VISION_BURST_FRAMES];
        ESP_LOGI(TAG, "LIVE_SCAN_START round_id=%lu",
                 (unsigned long)request.round_id);
#if STC_GAME_LIVE_CAMERA
        memset(frames, 0, sizeof(frames));
        stc_err_t err = camera_ready ? STC_OK : STC_ERR_IO;
        bool still_shown = false;
        if (camera_ready) {
            for (uint8_t i = 0; i < STC_VISION_BURST_FRAMES; ++i) {
                vTaskDelay(pdMS_TO_TICKS(50));
                if (stc_camera_capture_frame(rgb, 1000) == STC_OK &&
                    stc_game_classify_rgb160(rgb, frames[i].probs)) {
                    frames[i].ok = true;
                    int best = 0;
                    for (int cls = 1; cls < STC_MODEL_CLASSES; ++cls) {
                        if (frames[i].probs[cls] > frames[i].probs[best]) {
                            best = cls;
                        }
                    }
                    ESP_LOGI(TAG,
                             "LIVE_SCAN_FRAME n=%u class=%d conf=%.4f "
                             "p=[%.3f,%.3f,%.3f,%.3f,%.3f]",
                             (unsigned)i, best,
                             (double)frames[i].probs[best],
                             (double)frames[i].probs[0],
                             (double)frames[i].probs[1],
                             (double)frames[i].probs[2],
                             (double)frames[i].probs[3],
                             (double)frames[i].probs[4]);
                    if (!still_shown) {
                        stc_game_ui_set_camera_rgb160(rgb, STC_INPUT_SIZE,
                                                      STC_INPUT_SIZE);
                        still_shown = true;
                    }
                } else {
                    ESP_LOGW(TAG, "live camera frame=%u failed", (unsigned)i);
                }
            }
        }
#else
        stc_err_t err = stc_game_replay_infer_round(request.round, frames);
#endif
        if (err == STC_OK) {
            err = stc_vision_aggregate(frames, STC_VISION_BURST_FRAMES, 0.55f,
                                       &event.label.u.label, &event.detail);
        }
        uint8_t valid_frames = 0;
        for (uint8_t i = 0; i < STC_VISION_BURST_FRAMES; ++i) {
            valid_frames += frames[i].ok ? 1u : 0u;
        }
        event.detail.t_infer_ms =
            (uint32_t)((esp_timer_get_time() - started) / 1000);
        if (err == STC_OK) {
#if STC_GAME_LIVE_CAMERA
            event.label.u.label.src = STC_SRC_REAL;
#else
            event.label.u.label.src = STC_SRC_STUB;
#endif
            event.label.t_ms = now_ms();
            ESP_LOGI(TAG,
                     "LIVE_SCAN_RESULT round_id=%lu valid=%u bin=%d conf=%.4f "
                     "agreement=%u infer_ms=%lu",
                     (unsigned long)request.round_id, (unsigned)valid_frames,
                     (int)event.label.u.label.bin,
                     (double)event.label.u.label.conf,
                     (unsigned)event.detail.agreement,
                     (unsigned long)event.detail.t_infer_ms);
        } else {
            event.type = GAME_EVENT_VISION_FAILED;
            event.error = err;
            ESP_LOGW(TAG,
                     "LIVE_SCAN_FAILED round_id=%lu valid=%u error=%d "
                     "infer_ms=%lu",
                     (unsigned long)request.round_id,
                     (unsigned)valid_frames, (int)err,
                     (unsigned long)event.detail.t_infer_ms);
        }
        (void)post_game_event(&event);
    }
}

static stc_err_t request_scan(uint8_t round, uint32_t round_id, void *ctx)
{
    (void)ctx;
    if (!g_model_ready || !g_vision_queue) {
        return STC_ERR_NOT_READY;
    }
    const vision_request_t request = {round, round_id};
    return xQueueSend(g_vision_queue, &request, pdMS_TO_TICKS(20)) == pdTRUE
               ? STC_OK
               : STC_ERR_IO;
}

static stc_err_t hand_open(uint8_t round, uint32_t round_id, void *ctx)
{
    (void)round;
    (void)ctx;
    return stc_hand_link_command(STC_HAND_ACTION_OPEN_ALL, round_id, now_ms());
}

static stc_err_t hand_hold_open(uint8_t round, uint32_t round_id, void *ctx)
{
    (void)round;
    (void)ctx;
    return stc_hand_link_command(STC_HAND_ACTION_OPEN_ALL_HOLD, round_id,
                                 now_ms());
}

static stc_err_t hand_close(uint8_t round, uint32_t round_id, void *ctx)
{
    (void)round;
    (void)ctx;
    return stc_hand_link_command(STC_HAND_ACTION_CLOSE_ALL, round_id, now_ms());
}

static stc_err_t voice_say(stc_line_t line, void *ctx)
{
    (void)ctx;
    return stc_voice_say(line);
}

static void on_hand_ack(stc_hand_action_t action, bool success,
                        uint32_t round_id, void *ctx)
{
    (void)ctx;
    stc_game_controller_on_hand_ack(round_id, action, success, now_ms());
}

static bool on_hand_drop(const stc_hand_msg_t *msg, uint32_t round_id,
                         void *ctx)
{
    (void)ctx;
    if (!msg) return false;
    const stc_drop_t drop = {
        .bin = (stc_bin_t)msg->bin,
        .dist_mm = msg->dist_mm,
        .t_ms = now_ms(),
    };
    return stc_game_controller_on_drop(&drop, round_id, now_ms()) == STC_OK;
}

static void on_hand_online(bool online, void *ctx)
{
    (void)ctx;
    stc_game_controller_set_hand_online(online, now_ms());
}

static void shell_exit_game(void)
{
    stc_voice_stop();
}

static void shell_enter_chat(void)
{
    stc_voice_stop();
    const stc_err_t voice_mode = stc_voice_set_mode(STC_MODE_CHAT);
    if (voice_mode != STC_OK) {
        ESP_LOGW(TAG,
                 "CHAT_BACKEND=UNAVAILABLE error=%d shell=chat game=paused",
                 (int)voice_mode);
    }
    stc_game_ui_show_chat(voice_mode == STC_OK);
    ESP_LOGI(TAG, "SHELL_MODE=chat timeout_ms=%u",
             (unsigned)STC_SHELL_CHAT_TIMEOUT_MS);
}

static void shell_exit_chat(void)
{
    stc_voice_stop();
    (void)stc_voice_set_mode(STC_MODE_GAME);
    ESP_LOGI(TAG, "SHELL_MODE=game");
}

static void shell_enter_game(void)
{
    const stc_snapshot_t snap = stc_game_snapshot();
    const stc_game_view_t *view = stc_game_controller_view();
    stc_game_ui_render(&snap, view, NULL);
}

static void cancel_active_round(const char *source)
{
    const stc_snapshot_t snap = stc_game_snapshot();
    const stc_game_view_t *view = stc_game_controller_view();
    stc_voice_stop();

    vision_request_t stale = {};
    while (g_vision_queue &&
           xQueueReceive(g_vision_queue, &stale, 0) == pdTRUE) {
    }

    const bool lid_may_be_open =
        view &&
        (view->lid_open || view->awaiting_open_ack ||
         snap.phase == STC_PH_OPEN ||
         snap.phase == STC_PH_WAIT_DROP ||
         snap.phase == STC_PH_SETTLE ||
         snap.phase == STC_PH_CLOSE);
    stc_err_t close_result = STC_ERR_NOT_READY;
    if (lid_may_be_open) {
        close_result = hand_close(snap.round, snap.round_id, NULL);
    }

    g_intro_pending = false;
    const stc_err_t reset_result =
        stc_game_controller_ui_action(STC_UI_ACTION_HOME, now_ms());
    const stc_snapshot_t after = stc_game_snapshot();
    const stc_game_view_t *after_view = stc_game_controller_view();
    stc_game_ui_render(&after, after_view, NULL);
    ESP_LOGW(TAG,
             "ROUND_CANCEL source=%s phase=%d round_id=%lu close_result=%d "
             "reset_result=%d phase_after=%d",
             source ? source : "unknown", (int)snap.phase,
             (unsigned long)snap.round_id, (int)close_result,
             (int)reset_result, (int)after.phase);
}

static void handle_game_action(stc_ui_action_t action)
{
    const stc_snapshot_t snap = stc_game_snapshot();
    if (action == STC_UI_ACTION_START && snap.phase == STC_PH_IDLE &&
        !g_intro_seen) {
        if (stc_game_ui_show_intro()) {
            g_intro_seen = true;
            g_intro_pending = true;
            return;
        }
    }
    if (action == STC_UI_ACTION_SCAN && snap.phase == STC_PH_IDLE &&
        g_intro_pending) {
        g_intro_pending = false;
        if (stc_game_controller_ui_action(STC_UI_ACTION_START, now_ms()) ==
            STC_OK) {
            (void)stc_game_controller_ui_action(STC_UI_ACTION_SCAN, now_ms());
        }
        return;
    }
    if (action == STC_UI_ACTION_HOME) {
        g_intro_pending = false;
    }
    (void)stc_game_controller_ui_action(action, now_ms());
}

static void process_event(const game_event_t *event)
{
    if (stc_shell_get() != STC_SHELL_GAME) {
        if (event->type == GAME_EVENT_UI_ACTION &&
            event->action == STC_UI_ACTION_HOME) {
            (void)stc_shell_exit_chat();
        }
        return;
    }
    switch (event->type) {
    case GAME_EVENT_UI_ACTION:
        if (event->action == STC_UI_ACTION_CANCEL_ROUND) {
            cancel_active_round("ui_logout");
            break;
        }
        if (event->action == STC_UI_ACTION_ASK) {
            const stc_snapshot_t snap = stc_game_snapshot();
            if (snap.phase == STC_PH_IDLE || snap.phase == STC_PH_SUMMARY) {
                (void)stc_shell_enter_chat(now_ms());
            } else {
                ESP_LOGW(TAG, "SHELL_CHAT_REJECT phase=%d",
                         (int)snap.phase);
            }
            break;
        }
        if (event->action == STC_UI_ACTION_GUIDE) {
            stc_game_ui_show_guide();
            break;
        }
        if (event->action == STC_UI_ACTION_FINISH) {
            const stc_snapshot_t snap = stc_game_snapshot();
            const stc_game_view_t *view = stc_game_controller_view();
            stc_game_ui_render(&snap, view, NULL);
            break;
        }
        if (event->action == STC_UI_ACTION_NEXT ||
            event->action == STC_UI_ACTION_HOME) {
            stc_voice_stop();
        }
        if (event->action == STC_UI_ACTION_SELECT_BIN) {
#if STC_GAME_TOUCH_BIN
            if (event->category_id >= 0 &&
                event->category_id < (int32_t)STC_BIN_COUNT) {
                const stc_snapshot_t snap = stc_game_snapshot();
                const stc_drop_t drop = {
                    .bin = (stc_bin_t)event->category_id,
                    .dist_mm = 0,
                    .t_ms = now_ms(),
                };
                (void)stc_game_controller_on_drop(
                    &drop, snap.round_id, now_ms());
            }
#else
            ESP_LOGW(TAG, "TOUCH_BIN_IGNORED real_hand=enabled");
#endif
        } else {
            handle_game_action(event->action);
        }
        break;
    case GAME_EVENT_PLAY: {
        if (stc_shell_get() != STC_SHELL_GAME) {
            (void)stc_shell_exit_chat();
            break;
        }
        const stc_snapshot_t snap = stc_game_snapshot();
        stc_ui_action_t action = STC_UI_ACTION_START;
        if (snap.phase == STC_PH_PROMPT) {
            action = STC_UI_ACTION_SCAN;
        } else if (snap.phase == STC_PH_LEARNING) {
            action = STC_UI_ACTION_NEXT;
            stc_voice_stop();
        }
        handle_game_action(action);
        break;
    }
    case GAME_EVENT_VISION_RESULT:
        {
            const stc_err_t result = stc_game_controller_on_label(
                &event->label, &event->detail, now_ms());
            ESP_LOGI(TAG, "VISION_EVENT_APPLY round_id=%lu result=%d phase=%d",
                     (unsigned long)event->round_id, (int)result,
                     (int)stc_game_snapshot().phase);
        }
        break;
    case GAME_EVENT_VISION_FAILED:
        ESP_LOGW(TAG, "VISION_EVENT_FAILED round_id=%lu error=%d phase=%d",
                 (unsigned long)event->round_id, (int)event->error,
                 (int)stc_game_snapshot().phase);
        stc_game_controller_on_scan_failed(event->error, event->round_id,
                                           now_ms());
        break;
    default:
        break;
    }
}

static bool play_button_pressed(void)
{
    static bool last = true;
    static bool fired;
    static uint32_t changed_at;
    const bool level = gpio_get_level(GPIO_NUM_61) != 0;
    const uint32_t now = now_ms();
    if (level != last) {
        last = level;
        changed_at = now;
        fired = false;
        return false;
    }
    if (!level && !fired && (uint32_t)(now - changed_at) >= 30u) {
        fired = true;
        return true;
    }
    return false;
}

#if STC_GAME_TOUCH_BIN
static void dev_console_task(void *arg)
{
    (void)arg;
    char line[64] = {};
    size_t used = 0;
    const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
        ESP_LOGI(TAG,
                 "TOUCH_CONSOLE commands=start,scan,bin 0..3,next,chat,game,"
                 "status");
    while (true) {
        uint8_t ch = 0;
        const int count = read(STDIN_FILENO, &ch, 1);
        if (count <= 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (ch != '\r' && ch != '\n') {
            if (isprint(ch) && used < sizeof(line) - 1) {
                line[used++] = (char)ch;
            }
            continue;
        }
        if (used == 0) {
            continue;
        }
        line[used] = '\0';
        used = 0;

        game_event_t event = {};
        event.type = GAME_EVENT_UI_ACTION;
        event.category_id = -1;
        if (strcmp(line, "start") == 0) {
            event.action = STC_UI_ACTION_START;
        } else if (strcmp(line, "scan") == 0) {
            event.action = STC_UI_ACTION_SCAN;
        } else if (strcmp(line, "next") == 0) {
            event.action = STC_UI_ACTION_NEXT;
        } else if (strcmp(line, "home") == 0) {
            event.action = STC_UI_ACTION_HOME;
        } else if (strcmp(line, "chat") == 0) {
            event.action = STC_UI_ACTION_ASK;
        } else if (strcmp(line, "game") == 0) {
            event.action = STC_UI_ACTION_HOME;
        } else if (strncmp(line, "bin ", 4) == 0 &&
                   line[4] >= '0' && line[4] <= '3' && line[5] == '\0') {
            event.action = STC_UI_ACTION_SELECT_BIN;
            event.category_id = line[4] - '0';
        } else if (strcmp(line, "status") == 0) {
            const stc_snapshot_t snap = stc_game_snapshot();
            ESP_LOGI(TAG,
                     "TOUCH_STATUS shell=%s phase=%d round=%u round_id=%lu "
                     "truth=%d dropped=%d score=%d",
                     stc_shell_get() == STC_SHELL_GAME ? "game" : "chat",
                     (int)snap.phase, (unsigned)snap.round,
                     (unsigned long)snap.round_id, (int)snap.truth,
                     (int)snap.dropped, snap.score);
            continue;
        } else {
            ESP_LOGW(TAG, "TOUCH_CONSOLE unknown=%s", line);
            continue;
        }
        ESP_LOGI(TAG, "TOUCH_CONSOLE command=%s", line);
        (void)post_game_event(&event);
    }
}
#endif

extern "C" void app_main(void)
{
    const esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(nvs);
    }

#if STC_CAMERA_PROBE
    (void)stc_camera_probe_run();
    return;
#endif

    gpio_config_t play = {
        .pin_bit_mask = 1ULL << 61,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .hys_ctrl_mode = GPIO_HYS_SOFT_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&play));

    if (stc_game_ui_init(ui_action_sink, NULL) != STC_OK) {
        ESP_LOGE(TAG, "UI init failed");
        return;
    }
    stc_camera_capture_set_i2c_bus(bsp_i2c_get_handle());
    if (stc_game_voice_init() != STC_OK) {
        ESP_LOGE(TAG, "voice init failed");
    }
    stc_shell_init();
    const stc_shell_hooks_t shell_hooks = {
        .exit_game = shell_exit_game,
        .enter_chat = shell_enter_chat,
        .exit_chat = shell_exit_chat,
        .enter_game = shell_enter_game,
    };
    stc_shell_set_hooks(&shell_hooks);

    (void)stc_game_init();
#if STC_GAME_LIVE_CAMERA
    stc_game_config_t game_cfg = stc_game_get_config();
    game_cfg.scan_timeout_ms = 3000u;
    (void)stc_game_set_config(&game_cfg);
#endif
#if STC_GAME_TOUCH_BIN
    const stc_game_config_t touch_cfg =
        stc_game_touch_bin_config(stc_game_get_config());
    (void)stc_game_set_config(&touch_cfg);
#endif
    stc_game_controller_ports_t ports = {};
    ports.request_scan = request_scan;
    ports.voice_say = voice_say;
    ports.render = stc_game_ui_render;
#if !STC_GAME_TOUCH_BIN
    ports.hand_open_all = hand_open;
    ports.hand_hold_all = hand_hold_open;
    ports.hand_close_all = hand_close;
#endif
    stc_game_controller_init(&ports, NULL);
    g_debug_flags = 0;
#if STC_GAME_TOUCH_BIN
    g_debug_flags |= STC_UI_DEBUG_TOUCH_BINS;
#else
    g_debug_flags |= STC_UI_DEBUG_REAL_HAND;
#endif
#if STC_GAME_LIVE_CAMERA
    g_debug_flags |= STC_UI_DEBUG_LIVE_CAMERA;
#else
    g_debug_flags |= STC_UI_DEBUG_REPLAY;
#endif
#if STC_GAME_DEV_FLOW
    g_debug_flags |= STC_UI_DEBUG_DEV_FLOW;
    stc_game_dev_flow_init(&g_dev_flow);
#endif
    stc_game_controller_set_debug_flags(g_debug_flags);
#if STC_GAME_TOUCH_BIN
    stc_game_controller_set_hand_online(true, now_ms());
#else
    stc_game_controller_set_hand_online(false, now_ms());
#endif

    g_game_queue = xQueueCreate(16, sizeof(game_event_t));
    g_vision_queue = xQueueCreate(2, sizeof(vision_request_t));
    if (!g_game_queue || !g_vision_queue) {
        stc_game_controller_set_fault("QUEUE_INIT", now_ms());
        return;
    }

    const stc_err_t model_err = stc_game_model_init();
    g_model_ready = model_err == STC_OK;
    if (!g_model_ready) {
        stc_game_controller_set_fault("MODEL_LOAD", now_ms());
    } else {
        int correct = 0;
        int total = 0;
        const stc_err_t selftest =
            stc_game_replay_selftest(&correct, &total);
        ESP_LOGI(TAG, "MODEL_SELFTEST=%s correct=%d total=%d",
                 selftest == STC_OK ? "PASS" : "FAIL", correct, total);
        if (selftest != STC_OK) {
#if STC_GAME_DEV_FLOW || STC_GAME_IGNORE_MODEL_SELFTEST
#if STC_GAME_IGNORE_MODEL_SELFTEST
            g_debug_flags |= STC_UI_DEBUG_MODEL_WARN;
            stc_game_controller_set_debug_flags(g_debug_flags);
#endif
            ESP_LOGW(TAG, "MODEL_SELFTEST=FAIL correct=%d total=%d "
                          "MODEL_START_GATE=IGNORED",
                     correct, total);
#else
            g_model_ready = false;
            stc_game_controller_set_fault("MODEL_SELFTEST", now_ms());
#endif
        }
    }

#if STC_GAME_LIVE_CAMERA && STC_GAME_DEV_FLOW
    camera_smoke_test();
#endif

#if !STC_GAME_TOUCH_BIN
    if (stc_game_espnow_init(6) != STC_OK) {
        stc_game_controller_set_fault("ESPNOW_INIT", now_ms());
    } else {
        g_transport.send = stc_game_espnow_send;
        const stc_hand_hooks_t hooks = {
            .on_ack = on_hand_ack,
            .on_drop = on_hand_drop,
            .on_online = on_hand_online,
        };
        stc_hand_link_init(&g_transport, &hooks, NULL);
    }
#else
    ESP_LOGI(TAG, "TOUCH_BIN_MODE=ON transport=local_ui hand=disabled");
#endif

    /* ESP-DL's PIE kernels use 128-bit vector loads from the task stack. RTC RAM
     * faults in those kernels, while the internal DMA heap cannot provide the
     * stack size needed by the full camera/UI scan path. PSRAM is CPU-accessible
     * and avoids consuming the already tight internal heap used by Wi-Fi. */
    if (xTaskCreateWithCaps(
            vision_task, "stc_vision", 8192 / sizeof(StackType_t), NULL, 5,
            NULL, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        ESP_LOGE(TAG, "VISION_TASK_INIT=FAIL spiram=%u",
                 (unsigned)heap_caps_get_free_size(
                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        stc_game_controller_set_fault("VISION_TASK_INIT", now_ms());
        return;
    }
    ESP_LOGI(TAG, "VISION_TASK=READY stack=8192 caps=PSRAM");
#if STC_GAME_TOUCH_BIN
    if (xTaskCreate(dev_console_task, "touch_console", 4096, NULL, 3,
                    NULL) != pdPASS) {
        ESP_LOGW(TAG, "TOUCH_CONSOLE init failed");
    }
#endif

    while (true) {
        game_event_t event;
        if (xQueueReceive(g_game_queue, &event, pdMS_TO_TICKS(20)) == pdTRUE) {
            process_event(&event);
        }

#if !STC_GAME_TOUCH_BIN
        stc_hand_msg_t msg;
        while (stc_game_espnow_poll(&msg, 0)) {
            stc_hand_link_on_frame(&msg, now_ms());
        }

        // The ESP-NOW task only owns reception; this loop advances timers.
        stc_hand_link_poll(now_ms());
#endif
        stc_shell_tick(now_ms());
        if (stc_shell_get() == STC_SHELL_GAME) {
            stc_game_controller_tick(now_ms());
        }
#if STC_GAME_DEV_FLOW && !STC_GAME_TOUCH_BIN
        if (g_model_ready) {
            const stc_snapshot_t snap = stc_game_snapshot();
            const stc_game_view_t *view = stc_game_controller_view();
            const stc_dev_flow_action_t dev_action =
                stc_game_dev_flow_poll(&g_dev_flow, &snap,
                                       view->hand_online, now_ms());
            if (dev_action != STC_DEV_FLOW_NONE) {
                stc_ui_action_t ui_action = STC_UI_ACTION_START;
                const char *name = "START";
                if (dev_action == STC_DEV_FLOW_SCAN) {
                    ui_action = STC_UI_ACTION_SCAN;
                    name = "SCAN";
                } else if (dev_action == STC_DEV_FLOW_NEXT) {
                    ui_action = STC_UI_ACTION_NEXT;
                    name = "NEXT";
                    stc_voice_stop();
                }
                const stc_err_t dev_err =
                    stc_game_controller_ui_action(ui_action, now_ms());
                const stc_snapshot_t after = stc_game_snapshot();
                ESP_LOGI(TAG,
                         "DEV_AUTOPILOT action=%s result=%d round_id=%lu "
                         "phase=%d",
                         name, (int)dev_err, (unsigned long)after.round_id,
                         (int)after.phase);
            }
            if (snap.phase == STC_PH_SUMMARY && !g_dev_summary_logged) {
                g_dev_summary_logged = true;
                const stc_result_t result = stc_game_result();
                ESP_LOGI(TAG,
                         "DEV_FLOW=COMPLETE score=%d streak=%u rounds=%u "
                         "correct=%u wrong=%u timed_out=%u duration_ms=%lu "
                         "ended_by_time=%u",
                         snap.score, (unsigned)snap.streak,
                         (unsigned)result.rounds_played,
                         (unsigned)result.correct, (unsigned)result.wrong,
                         (unsigned)result.timed_out,
                         (unsigned long)result.duration_ms,
                         result.ended_by_time ? 1u : 0u);
            }
        }
#endif
        if (play_button_pressed()) {
            if (stc_shell_get() != STC_SHELL_GAME) {
                (void)stc_shell_exit_chat();
                continue;
            }
            event = {};
            event.type = GAME_EVENT_PLAY;
            (void)post_game_event(&event);
        }
        stc_voice_poll();
    }
}
