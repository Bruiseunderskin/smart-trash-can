#include <algorithm>
#include <array>
#include <cmath>
#include <cinttypes>
#include <cstddef>
#include <cstdint>

#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

constexpr char kTag[] = "tflite_probe";
constexpr size_t kArenaSize = 4 * 1024 * 1024;
constexpr size_t kRuns = 20;

extern const uint8_t model_start[] asm("_binary_waste4_int8_tflite_start");
extern const uint8_t model_end[] asm("_binary_waste4_int8_tflite_end");

const char* KernelMode() {
#if CONFIG_TFLITE_PROBE_USE_ESP_NN
    return "on";
#else
    return "off";
#endif
}

double Mean(const std::array<int64_t, kRuns>& samples) {
    int64_t total = 0;
    for (const int64_t sample : samples) {
        total += sample;
    }
    return static_cast<double>(total) / static_cast<double>(samples.size());
}

double P50(const std::array<int64_t, kRuns>& sorted) {
    return (static_cast<double>(sorted[kRuns / 2 - 1]) +
            static_cast<double>(sorted[kRuns / 2])) / 2.0;
}

int64_t P95NearestRank(const std::array<int64_t, kRuns>& sorted) {
    const size_t index = static_cast<size_t>(std::ceil(0.95 * kRuns)) - 1;
    return sorted[index];
}

}  // namespace

extern "C" void app_main(void) {
    const size_t model_bytes = static_cast<size_t>(model_end - model_start);
    ESP_LOGI(kTag,
             "target=esp32s3 idf=%s model=person_detect_int8 bytes=%u "
             "esp_nn=%s runs=%u cpu_mhz=%u psram_bytes=%u",
             esp_get_idf_version(), static_cast<unsigned>(model_bytes), KernelMode(),
             static_cast<unsigned>(kRuns),
             static_cast<unsigned>(esp_clk_cpu_freq() / 1000000),
             static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)));

    const tflite::Model* model = tflite::GetModel(model_start);
    if (model == nullptr || model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(kTag, "Invalid TFLite model or schema version mismatch");
        return;
    }

    auto* arena_allocation = static_cast<uint8_t*>(
        heap_caps_malloc(kArenaSize + 15, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (arena_allocation == nullptr) {
        ESP_LOGE(kTag,
                 "Cannot allocate %u-byte tensor arena in PSRAM; confirm PSRAM is enabled "
                 "and available on this board",
                 static_cast<unsigned>(kArenaSize));
        return;
    }
    auto* arena = reinterpret_cast<uint8_t*>(
        (reinterpret_cast<uintptr_t>(arena_allocation) + 15U) & ~static_cast<uintptr_t>(15U));

    tflite::MicroMutableOpResolver<7> resolver;
    if (resolver.AddConv2D() != kTfLiteOk ||
        resolver.AddDepthwiseConv2D() != kTfLiteOk ||
        resolver.AddAveragePool2D() != kTfLiteOk ||
        resolver.AddReshape() != kTfLiteOk ||
        resolver.AddSoftmax() != kTfLiteOk ||
        resolver.AddMaxPool2D() != kTfLiteOk ||
        resolver.AddFullyConnected() != kTfLiteOk) {
        ESP_LOGE(kTag, "Failed to register model operators");
        heap_caps_free(arena_allocation);
        return;
    }

    tflite::MicroInterpreter interpreter(model, resolver, arena, kArenaSize);
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(kTag, "AllocateTensors failed; check PSRAM and model operators");
        heap_caps_free(arena_allocation);
        return;
    }

    TfLiteTensor* input = interpreter.input(0);
    if (input == nullptr) {
        ESP_LOGE(kTag, "Model has no input tensor");
        heap_caps_free(arena_allocation);
        return;
    }
    std::fill_n(input->data.int8, input->bytes, static_cast<int8_t>(0));

    std::array<int64_t, kRuns> samples{};
    for (size_t i = 0; i < kRuns; ++i) {
        const int64_t start_us = esp_timer_get_time();
        if (interpreter.Invoke() != kTfLiteOk) {
            ESP_LOGE(kTag, "Invoke failed at run %u", static_cast<unsigned>(i + 1));
            heap_caps_free(arena_allocation);
            return;
        }
        samples[i] = esp_timer_get_time() - start_us;
    }

    std::array<int64_t, kRuns> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    ESP_LOGI(kTag,
             "RESULT esp_nn=%s runs=%u mean_us=%.0f p50_us=%.0f p95_us=%" PRId64
             " arena_used=%u",
             KernelMode(), static_cast<unsigned>(kRuns), Mean(samples), P50(sorted),
             P95NearestRank(sorted),
             static_cast<unsigned>(interpreter.arena_used_bytes()));

    heap_caps_free(arena_allocation);
}
