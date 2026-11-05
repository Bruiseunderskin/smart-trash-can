#include "stc_bin.h"
#include "stc_vision.h"

#include "assets.h"

#include <cJSON.h>

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <memory>
#include <vector>

namespace {

constexpr const char* TAG = "StcStubs";
constexpr uint32_t kVisionDelayMs = 200;
constexpr uint32_t kDropDelayMs = 3000;

std::vector<stc_bin_t> labels = {
    STC_BIN_RECYCLABLE, STC_BIN_FOOD, STC_BIN_HAZARD, STC_BIN_OTHER, STC_BIN_RECYCLABLE,
};
size_t next_label;
bool lid_open;
bool monitor_active;
uint32_t monitor_started_ms;
stc_bin_t expected_bin = STC_BIN_RECYCLABLE;
uint32_t drop_sequence;

uint32_t NowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

std::vector<stc_bin_t> LoadLabels() {
    void* data = nullptr;
    size_t size = 0;
    if (!Assets::GetInstance().GetAssetData("stub-labels.json", data, size) || data == nullptr) {
        ESP_LOGW(TAG, "stub-labels.json missing; using built-in sequence");
        return labels;
    }
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        cJSON_ParseWithLength(static_cast<const char*>(data), size), &cJSON_Delete);
    if (!root || !cJSON_IsArray(root.get())) {
        ESP_LOGW(TAG, "stub-labels.json invalid; using built-in sequence");
        return labels;
    }
    std::vector<stc_bin_t> parsed;
    for (const cJSON* item = root->child; item != nullptr; item = item->next) {
        if (cJSON_IsNumber(item) && item->valueint >= 0 && item->valueint < STC_BIN_COUNT) {
            parsed.push_back(static_cast<stc_bin_t>(item->valueint));
        }
    }
    return parsed.empty() ? labels : parsed;
}

}  // namespace

extern "C" stc_err_t stc_vision_init(const char* model_dir) {
    (void)model_dir;
    labels = LoadLabels();
    next_label = 0;
    return STC_OK;
}

extern "C" stc_err_t stc_vision_capture(stc_label_t* out, uint32_t timeout_ms) {
    if (out == nullptr || timeout_ms < kVisionDelayMs) {
        return STC_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(kVisionDelayMs));
    const stc_bin_t bin = labels[next_label % labels.size()];
    ++next_label;
    *out = {
        .bin = bin,
        .class_idx = static_cast<uint8_t>(bin),
        .conf = 0.99F,
        .abstain = false,
        .t_ms = NowMs(),
        .src = STC_SRC_STUB,
    };
    return STC_OK;
}

extern "C" void stc_vision_release(void) {}

extern "C" const char* stc_vision_version(void) { return "stub-1"; }

extern "C" void stc_bin_stub_set_truth(stc_bin_t truth) { expected_bin = truth; }

extern "C" stc_err_t stc_bin_init(void) {
    lid_open = false;
    monitor_active = false;
    monitor_started_ms = 0;
    drop_sequence = 0;
    return STC_OK;
}

extern "C" stc_err_t stc_bin_selftest(void) { return STC_OK; }

extern "C" uint8_t stc_bin_fault_mask(void) { return 0; }

extern "C" stc_err_t stc_bin_open_all(void) {
    lid_open = true;
    return STC_OK;
}

extern "C" stc_err_t stc_bin_close_all(void) {
    lid_open = false;
    monitor_active = false;
    return STC_OK;
}

extern "C" stc_err_t stc_bin_start_monitor(uint16_t timeout_ms) {
    (void)timeout_ms;
    monitor_active = true;
    monitor_started_ms = NowMs();
    return STC_OK;
}

extern "C" stc_err_t stc_bin_stop_monitor(void) {
    monitor_active = false;
    return STC_OK;
}

extern "C" int stc_bin_poll(stc_drop_t* event) {
    if (event == nullptr) {
        return STC_ERR_CONTRACT;
    }
    *event = {};
    if (!monitor_active || NowMs() - monitor_started_ms < kDropDelayMs) {
        return 0;
    }
    const bool drop_correct = (drop_sequence++ % 2U) == 0;
    stc_bin_t bin = expected_bin;
    if (!drop_correct) {
        bin = static_cast<stc_bin_t>(
            (static_cast<unsigned>(expected_bin) + 1U + esp_random() % 3U) % STC_BIN_COUNT);
    }
    monitor_active = false;
    lid_open = false;
    event->bin = bin;
    event->dist_mm = 55;
    event->t_ms = NowMs();
    ESP_LOGI(TAG, "STUB_DROP truth=%d selected=%d correct=%d", static_cast<int>(expected_bin),
             static_cast<int>(bin), drop_correct);
    return 1;
}

extern "C" uint16_t stc_bin_distance(stc_bin_t bin) {
    if (bin < 0 || bin >= STC_BIN_COUNT) {
        return 0;
    }
    return lid_open ? 55 : 100;
}
