#include "stc_game_runtime.h"

#include "camera_capture.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "espnow_controller.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "replay_classifier.hpp"
#include "stc_game_controller.h"
#include "stc_hand_link.h"
#include "stc_hand_msg.h"
#include "stc_mode_request.h"
#include "stc_preprocess.h"
#include "stc_shell_mode.h"
#include "stc_shell_ui.h"
#include "stc_vision_aggregate.h"
#include "stc_vision_map.h"
#include "stc_voice.h"

#include <atomic>
#include <cstring>

namespace {

constexpr const char *TAG = "StcGameRuntime";
constexpr uint8_t kEspNowChannel = 6;

enum GameEventType {
    kGameEventUiAction = 0,
    kGameEventPlay,
    kGameEventVisionResult,
    kGameEventVisionFailed,
    kGameEventMockLabel,
};

struct GameEvent {
    GameEventType type;
    stc_ui_action_t action;
    int32_t category_id;
    stc_event_t label;
    stc_burst_detail_t detail;
    stc_err_t error;
    uint32_t round_id;
};

struct VisionRequest {
    uint8_t round;
    uint32_t round_id;
};

QueueHandle_t game_queue;
QueueHandle_t vision_queue;
stc_hand_transport_t hand_transport;
std::atomic<bool> runtime_started{false};
std::atomic<bool> model_ready{false};
std::atomic<bool> espnow_ready{false};
std::atomic<bool> camera_ready{false};

uint32_t NowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

bool PostGameEvent(const GameEvent &event) {
    return game_queue != nullptr &&
           xQueueSend(game_queue, &event, pdMS_TO_TICKS(20)) == pdTRUE;
}

void RequestChat(void *ctx) {
    (void)ctx;
    stc_shell_ui_request_chat();
}

void UiActionSink(stc_ui_action_t action, int32_t category_id, void *ctx) {
    (void)ctx;
    GameEvent event = {};
    event.type = kGameEventUiAction;
    event.action = action;
    event.category_id = category_id;
    (void)PostGameEvent(event);
}

stc_err_t RequestScan(uint8_t round, uint32_t round_id, void *ctx) {
    (void)ctx;
    if (!model_ready.load() || vision_queue == nullptr) {
        return STC_ERR_NOT_READY;
    }
    const VisionRequest request = {round, round_id};
    return xQueueSend(vision_queue, &request, pdMS_TO_TICKS(20)) == pdTRUE
               ? STC_OK
               : STC_ERR_IO;
}

stc_err_t HandOpen(uint8_t round, uint32_t round_id, void *ctx) {
    (void)round;
    (void)ctx;
    return espnow_ready.load()
               ? stc_hand_link_command(STC_HAND_ACTION_OPEN_ALL, round_id,
                                       NowMs())
               : STC_ERR_NOT_READY;
}

stc_err_t HandHoldOpen(uint8_t round, uint32_t round_id, void *ctx) {
    (void)round;
    (void)ctx;
    return espnow_ready.load()
               ? stc_hand_link_command(STC_HAND_ACTION_OPEN_ALL_HOLD,
                                       round_id, NowMs())
               : STC_ERR_NOT_READY;
}

stc_err_t HandClose(uint8_t round, uint32_t round_id, void *ctx) {
    (void)round;
    (void)ctx;
    return espnow_ready.load()
               ? stc_hand_link_command(STC_HAND_ACTION_CLOSE_ALL, round_id,
                                       NowMs())
               : STC_ERR_NOT_READY;
}

stc_err_t VoiceSay(stc_line_t line, void *ctx) {
    (void)ctx;
    return stc_voice_say(line);
}

void RenderGame(const stc_snapshot_t *snapshot, const stc_game_view_t *view,
                void *ctx) {
    (void)ctx;
    stc_shell_ui_render_game(snapshot, view);
}

void OnHandAck(stc_hand_action_t action, bool success, uint32_t round_id,
               void *ctx) {
    (void)ctx;
    stc_game_controller_on_hand_ack(round_id, action, success, NowMs());
}

bool OnHandDrop(const stc_hand_msg_t *msg, uint32_t round_id, void *ctx) {
    (void)ctx;
    if (msg == nullptr) {
        return false;
    }
    const stc_drop_t drop = {
        .bin = static_cast<stc_bin_t>(msg->bin),
        .dist_mm = msg->dist_mm,
        .t_ms = NowMs(),
    };
    return stc_game_controller_on_drop(&drop, round_id, NowMs()) == STC_OK;
}

void OnHandOnline(bool online, void *ctx) {
    (void)ctx;
    stc_game_controller_set_hand_online(online, NowMs());
    ESP_LOGI(TAG, "HAND_ONLINE=%d", online ? 1 : 0);
}

void VisionTask(void *) {
    uint8_t *rgb = static_cast<uint8_t *>(
        heap_caps_malloc(STC_INPUT_SIZE * STC_INPUT_SIZE * 3,
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (rgb == nullptr) {
        ESP_LOGE(TAG, "LIVE_CAMERA=FAIL reason=RGB_ALLOC");
        stc_game_controller_set_fault("CAMERA_MEMORY", NowMs());
    }

    bool streaming =
        rgb != nullptr && stc_camera_capture_begin() == STC_OK;
    camera_ready.store(streaming);
    if (streaming) {
        ESP_LOGI(TAG, "LIVE_CAMERA=PREVIEW_READY 640x480_UYVY");
    } else {
        ESP_LOGE(TAG, "LIVE_CAMERA=FAIL reason=START");
        stc_game_controller_set_fault("CAMERA_OFFLINE", NowMs());
    }

    for (;;) {
        VisionRequest request = {};
        const bool has_request =
            vision_queue != nullptr &&
            xQueueReceive(vision_queue, &request, pdMS_TO_TICKS(80)) == pdTRUE;

        if (!has_request) {
            if (streaming && stc_shell_get() == STC_SHELL_GAME &&
                stc_camera_capture_frame(rgb, 1000) == STC_OK) {
                stc_shell_ui_set_camera_rgb160(rgb, STC_INPUT_SIZE,
                                               STC_INPUT_SIZE);
            }
            continue;
        }

        GameEvent event = {};
        event.type = kGameEventVisionResult;
        event.round_id = request.round_id;
        event.label.type = STC_EV_LABEL;
        event.label.round_id = request.round_id;
        event.label.t_ms = NowMs();

        const int64_t started = esp_timer_get_time();
        stc_vision_frame_t frames[STC_VISION_BURST_FRAMES] = {};
        stc_err_t error = streaming ? STC_OK : STC_ERR_IO;
        ESP_LOGI(TAG, "LIVE_SCAN_START round_id=%lu",
                 static_cast<unsigned long>(request.round_id));
        if (streaming) {
            for (uint8_t frame = 0; frame < STC_VISION_BURST_FRAMES;
                 ++frame) {
                vTaskDelay(pdMS_TO_TICKS(50));
                if (stc_camera_capture_frame(rgb, 1000) == STC_OK &&
                    stc_game_classify_rgb160(rgb, frames[frame].probs)) {
                    frames[frame].ok = true;
                    int best = 0;
                    for (int cls = 1; cls < STC_MODEL_CLASSES; ++cls) {
                        if (frames[frame].probs[cls] >
                            frames[frame].probs[best]) {
                            best = cls;
                        }
                    }
                    ESP_LOGI(
                        TAG,
                        "LIVE_SCAN_FRAME n=%u class=%d conf=%.4f "
                        "p=[%.3f,%.3f,%.3f,%.3f,%.3f]",
                        static_cast<unsigned>(frame), best,
                        static_cast<double>(frames[frame].probs[best]),
                        static_cast<double>(frames[frame].probs[0]),
                        static_cast<double>(frames[frame].probs[1]),
                        static_cast<double>(frames[frame].probs[2]),
                        static_cast<double>(frames[frame].probs[3]),
                        static_cast<double>(frames[frame].probs[4]));
                    stc_shell_ui_set_camera_rgb160(
                        rgb, STC_INPUT_SIZE, STC_INPUT_SIZE);
                } else {
                    ESP_LOGW(TAG, "live camera frame=%u failed",
                             static_cast<unsigned>(frame));
                }
            }
        }

        if (error == STC_OK) {
            error = stc_vision_aggregate(
                frames, STC_VISION_BURST_FRAMES, 0.55f,
                &event.label.u.label, &event.detail);
        }
        uint8_t valid_frames = 0;
        for (uint8_t frame = 0; frame < STC_VISION_BURST_FRAMES;
             ++frame) {
            valid_frames += frames[frame].ok ? 1u : 0u;
        }
        event.detail.t_infer_ms = static_cast<uint32_t>(
            (esp_timer_get_time() - started) / 1000);
        if (error == STC_OK) {
            event.label.u.label.src = STC_SRC_REAL;
            event.label.t_ms = NowMs();
            const char* label =
                stc_vision_map_label(event.label.u.label.class_idx);
            ESP_LOGI(TAG,
                     "LIVE_SCAN_RESULT round_id=%lu valid=%u class=%u "
                     "label=%s bin=%d conf=%.4f top2_class=%u "
                     "top2_conf=%.4f agreement=%u infer_ms=%lu",
                     static_cast<unsigned long>(request.round_id),
                     static_cast<unsigned>(valid_frames),
                     static_cast<unsigned>(event.label.u.label.class_idx),
                     label != nullptr ? label : "?",
                     static_cast<int>(event.label.u.label.bin),
                     static_cast<double>(event.label.u.label.conf),
                     static_cast<unsigned>(event.detail.top2_bin),
                     static_cast<double>(event.detail.top2_conf),
                     static_cast<unsigned>(event.detail.agreement),
                     static_cast<unsigned long>(event.detail.t_infer_ms));
        } else {
            event.type = kGameEventVisionFailed;
            event.error = error;
            ESP_LOGW(TAG,
                     "LIVE_SCAN_FAILED round_id=%lu valid=%u error=%d "
                     "infer_ms=%lu",
                     static_cast<unsigned long>(request.round_id),
                     static_cast<unsigned>(valid_frames),
                     static_cast<int>(error),
                     static_cast<unsigned long>(event.detail.t_infer_ms));
        }
        (void)PostGameEvent(event);
    }
}

void HandlePlayEvent() {
    const stc_snapshot_t snapshot = stc_game_snapshot();
    stc_ui_action_t action = STC_UI_ACTION_START;
    if (snapshot.phase == STC_PH_PROMPT) {
        action = STC_UI_ACTION_SCAN;
    } else if (snapshot.phase == STC_PH_LEARNING) {
        action = STC_UI_ACTION_NEXT;
        stc_voice_stop();
    }
    const stc_err_t result =
        stc_game_controller_ui_action(action, NowMs());
    const stc_snapshot_t after = stc_game_snapshot();
    ESP_LOGI(TAG,
             "PLAY_ACTION action=%d phase_before=%d phase_after=%d "
             "round=%u round_id=%lu result=%d",
             static_cast<int>(action), static_cast<int>(snapshot.phase),
             static_cast<int>(after.phase), static_cast<unsigned>(after.round),
             static_cast<unsigned long>(after.round_id),
             static_cast<int>(result));
}

void ProcessEvent(const GameEvent &event) {
    if (stc_shell_get() != STC_SHELL_GAME) {
        return;
    }

    switch (event.type) {
        case kGameEventUiAction: {
            const stc_snapshot_t before = stc_game_snapshot();
            ESP_LOGI(TAG,
                     "UI_ACTION action=%d phase=%d round=%u round_id=%lu "
                     "category=%ld",
                     static_cast<int>(event.action),
                     static_cast<int>(before.phase),
                     static_cast<unsigned>(before.round),
                     static_cast<unsigned long>(before.round_id),
                     static_cast<long>(event.category_id));
            if (event.action == STC_UI_ACTION_ASK) {
                const stc_snapshot_t snapshot = stc_game_snapshot();
                if (snapshot.phase == STC_PH_IDLE ||
                    snapshot.phase == STC_PH_SUMMARY) {
                    (void)stc_mode_request_post(STC_MODE_CHAT);
                } else {
                    ESP_LOGW(TAG, "SHELL_CHAT_REJECT phase=%d",
                             static_cast<int>(snapshot.phase));
                }
                break;
            }
            if (event.action == STC_UI_ACTION_GUIDE) {
                stc_shell_ui_show_guide();
                break;
            }
            if (event.action == STC_UI_ACTION_FINISH) {
                stc_game_runtime_render();
                break;
            }
            if (event.action == STC_UI_ACTION_NEXT ||
                event.action == STC_UI_ACTION_HOME) {
                stc_voice_stop();
            }
            if (event.action == STC_UI_ACTION_SELECT_BIN) {
                const stc_snapshot_t snapshot = stc_game_snapshot();
                if (event.category_id < 0 ||
                    event.category_id >= STC_BIN_COUNT) {
                    ESP_LOGW(TAG, "TOUCH_BIN_INVALID bin=%ld",
                             static_cast<long>(event.category_id));
                    break;
                }
                const stc_drop_t drop = {
                    .bin = static_cast<stc_bin_t>(event.category_id),
                    .dist_mm = 0,
                    .t_ms = NowMs(),
                };
                const stc_err_t accepted = stc_game_controller_on_drop(
                    &drop, snapshot.round_id, NowMs());
                ESP_LOGI(TAG,
                         "TOUCH_BIN_SELECT bin=%ld round_id=%lu accepted=%d",
                         static_cast<long>(event.category_id),
                         static_cast<unsigned long>(snapshot.round_id),
                         static_cast<int>(accepted));
                break;
            }
            const stc_err_t result =
                stc_game_controller_ui_action(event.action, NowMs());
            const stc_snapshot_t after = stc_game_snapshot();
            ESP_LOGI(TAG,
                     "UI_ACTION_RESULT action=%d result=%d phase_after=%d "
                     "round=%u round_id=%lu",
                     static_cast<int>(event.action),
                     static_cast<int>(result),
                     static_cast<int>(after.phase),
                     static_cast<unsigned>(after.round),
                     static_cast<unsigned long>(after.round_id));
            break;
        }
        case kGameEventPlay:
            HandlePlayEvent();
            break;
        case kGameEventVisionResult:
            (void)stc_game_controller_on_label(&event.label, &event.detail,
                                               NowMs());
            break;
        case kGameEventVisionFailed:
            stc_game_controller_on_scan_failed(event.error, event.round_id,
                                               NowMs());
            break;
        case kGameEventMockLabel: {
            const stc_snapshot_t snapshot = stc_game_snapshot();
            if (snapshot.phase != STC_PH_SCAN ||
                event.category_id < 0 ||
                event.category_id >= STC_BIN_COUNT) {
                ESP_LOGW(TAG,
                         "MOCK_LABEL_REJECT phase=%d bin=%ld",
                         static_cast<int>(snapshot.phase),
                         static_cast<long>(event.category_id));
                break;
            }
            stc_event_t label = {};
            label.type = STC_EV_LABEL;
            label.round_id = snapshot.round_id;
            label.t_ms = NowMs();
            label.u.label.bin =
                static_cast<stc_bin_t>(event.category_id);
            label.u.label.conf = 0.99f;
            label.u.label.src = STC_SRC_STUB;
            const stc_err_t result =
                stc_game_controller_on_label(&label, nullptr, NowMs());
            ESP_LOGI(TAG,
                     "MOCK_LABEL round_id=%lu bin=%ld result=%d",
                     static_cast<unsigned long>(snapshot.round_id),
                     static_cast<long>(event.category_id),
                     static_cast<int>(result));
            break;
        }
    }
}

void RuntimeTask(void *) {
    for (;;) {
        GameEvent event = {};
        if (game_queue != nullptr &&
            xQueueReceive(game_queue, &event, pdMS_TO_TICKS(20)) == pdTRUE) {
            ProcessEvent(event);
        }

        if (espnow_ready.load()) {
            stc_hand_msg_t msg = {};
            while (stc_game_espnow_poll(&msg, 0)) {
                stc_hand_link_on_frame(&msg, NowMs());
            }
            (void)stc_hand_link_poll(NowMs());
        }

        if (stc_shell_get() == STC_SHELL_GAME) {
            stc_game_controller_tick(NowMs());
        }
        stc_voice_poll();
    }
}

}  // namespace

extern "C" stc_err_t stc_game_runtime_start(void) {
    if (runtime_started.load()) {
        return STC_OK;
    }
    runtime_started.store(true);

    game_queue = xQueueCreate(16, sizeof(GameEvent));
    vision_queue = xQueueCreate(2, sizeof(VisionRequest));
    if (game_queue == nullptr || vision_queue == nullptr) {
        ESP_LOGE(TAG, "queue initialization failed");
        stc_game_runtime_log_status("queue-failure");
        return STC_ERR_IO;
    }

    (void)stc_game_init();
    stc_game_config_t config = stc_game_get_config();
    config.scan_timeout_ms = 3000;
    (void)stc_game_set_config(&config);

    stc_game_controller_ports_t ports = {};
    ports.request_scan = RequestScan;
    ports.hand_open_all = HandOpen;
    ports.hand_hold_all = HandHoldOpen;
    ports.hand_close_all = HandClose;
    ports.voice_say = VoiceSay;
    ports.render = RenderGame;
    ports.request_chat = RequestChat;
    stc_game_controller_init(&ports, nullptr);
    stc_game_controller_set_hand_online(false, NowMs());

    const stc_err_t model_error = stc_game_model_init();
    model_ready.store(model_error == STC_OK);
    bool model_selftest_ok = false;
    if (!model_ready.load()) {
        ESP_LOGE(TAG, "MODEL_LOAD=FAIL error=%d",
                 static_cast<int>(model_error));
        stc_game_controller_set_fault("MODEL_LOAD", NowMs());
    } else {
        int correct = 0;
        int total = 0;
        const stc_err_t selftest =
            stc_game_replay_selftest(&correct, &total);
        ESP_LOGI(TAG, "MODEL_SELFTEST=%s correct=%d total=%d",
                 selftest == STC_OK ? "PASS" : "FAIL", correct, total);
        model_selftest_ok = selftest == STC_OK;
        if (selftest != STC_OK) {
            ESP_LOGW(TAG,
                     "MODEL_SELFTEST=FAIL correct=%d total=%d "
                     "MODEL_START_GATE=IGNORED",
                     correct, total);
        }
    }

    const stc_err_t espnow_error =
        stc_game_espnow_init(kEspNowChannel);
    espnow_ready.store(espnow_error == STC_OK);
    if (espnow_ready.load()) {
        hand_transport.send = stc_game_espnow_send;
        const stc_hand_hooks_t hooks = {
            .on_ack = OnHandAck,
            .on_drop = OnHandDrop,
            .on_online = OnHandOnline,
        };
        stc_hand_link_init(&hand_transport, &hooks, nullptr);
    } else {
        ESP_LOGE(TAG, "ESPNOW_INIT=FAIL error=%d",
                 static_cast<int>(espnow_error));
        stc_game_controller_set_fault("ESPNOW_INIT", NowMs());
    }

    uint8_t debug_flags = STC_UI_DEBUG_LIVE_CAMERA |
                          STC_UI_DEBUG_REAL_HAND;
    if (!model_selftest_ok) {
        debug_flags |= STC_UI_DEBUG_MODEL_WARN;
    }
    stc_game_controller_set_debug_flags(debug_flags);

    if (xTaskCreateWithCaps(
            VisionTask, "stc_vision", 8192 / sizeof(StackType_t), nullptr,
            5, nullptr, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        ESP_LOGE(TAG, "VISION_TASK_INIT=FAIL");
        stc_game_controller_set_fault("VISION_TASK_INIT", NowMs());
        return STC_ERR_IO;
    }
    if (xTaskCreate(RuntimeTask, "stc_game_loop", 6144, nullptr, 5,
                    nullptr) != pdPASS) {
        ESP_LOGE(TAG, "GAME_TASK_INIT=FAIL");
        stc_game_controller_set_fault("GAME_TASK_INIT", NowMs());
        return STC_ERR_IO;
    }

    stc_shell_ui_set_game_action_callback(UiActionSink);
    stc_game_runtime_render();
    stc_game_runtime_log_status("started");
    return STC_OK;
}

extern "C" void stc_game_runtime_submit_action(stc_ui_action_t action,
                                                int32_t category_id) {
    GameEvent event = {};
    event.type = kGameEventUiAction;
    event.action = action;
    event.category_id = category_id;
    (void)PostGameEvent(event);
}

extern "C" void stc_game_runtime_submit_mock_label(int32_t bin) {
    GameEvent event = {};
    event.type = kGameEventMockLabel;
    event.category_id = bin;
    (void)PostGameEvent(event);
}

extern "C" void stc_game_runtime_play_button(void) {
    if (stc_shell_get() != STC_SHELL_GAME) {
        (void)stc_mode_request_post(STC_MODE_GAME);
        return;
    }
    GameEvent event = {};
    event.type = kGameEventPlay;
    if (!PostGameEvent(event)) {
        ESP_LOGW(TAG, "PLAY_QUEUE=full");
    }
}

extern "C" void stc_game_runtime_render(void) {
    const stc_snapshot_t snapshot = stc_game_snapshot();
    const stc_game_view_t *view = stc_game_controller_view();
    if (view != nullptr) {
        stc_shell_ui_render_game(&snapshot, view);
    }
}

extern "C" stc_err_t stc_game_runtime_suspend_hand(void) {
    const stc_err_t result = stc_game_espnow_suspend();
    stc_game_controller_set_hand_online(false, NowMs());
    return result;
}

extern "C" stc_err_t stc_game_runtime_resume_hand(void) {
    if (!espnow_ready.load()) {
        const stc_err_t error = stc_game_espnow_init(kEspNowChannel);
        espnow_ready.store(error == STC_OK);
        return error;
    }
    return stc_game_espnow_resume(kEspNowChannel);
}

extern "C" bool stc_game_runtime_hand_online(void) {
    return espnow_ready.load() && stc_hand_link_online(NowMs());
}

extern "C" void stc_game_runtime_log_status(const char *source) {
    const stc_snapshot_t snapshot = stc_game_snapshot();
    ESP_LOGI(TAG,
             "GAME_STATUS source=%s phase=%d round=%u round_id=%lu "
             "score=%d time_bar=%ld model=%d camera=%d espnow=%d "
             "hand_online=%d",
             source != nullptr ? source : "unknown",
             static_cast<int>(snapshot.phase),
             static_cast<unsigned>(snapshot.round),
             static_cast<unsigned long>(snapshot.round_id),
             static_cast<int>(snapshot.score),
             static_cast<long>(stc_shell_ui_time_bar_value()),
             model_ready.load() ? 1 : 0,
             camera_ready.load() ? 1 : 0, espnow_ready.load() ? 1 : 0,
             stc_game_runtime_hand_online() ? 1 : 0);
}
