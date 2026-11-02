/*
 * SPDX-FileCopyrightText: 2026 Smart Trash Can contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

#include "dl_model_base.hpp"
#include "dl_image_define.hpp"
#include "dl_image_jpeg.hpp"
#include "dl_image_preprocessor.hpp"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "fbs_model.hpp"
#include "cases.h"

extern const uint8_t waste4_grouped_int8_espdl[]
    asm("_binary_waste4_grouped_int8_espdl_start");

static const char *TAG = "S31_MODEL";

static size_t free_internal()
{
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static size_t free_psram()
{
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static std::vector<int> center_square(const dl::image::img_t &image)
{
    const int side = std::min(image.width, image.height);
    const int left = (image.width - side) / 2;
    const int top = (image.height - side) / 2;
    return {left, top, left + side, top + side};
}

static uint32_t quantize_rgb888_image(dl::TensorBase *input, const dl::image::img_t &image)
{
    constexpr float mean[3] = {123.675f, 116.28f, 103.53f};
    constexpr float stddev[3] = {58.395f, 57.12f, 57.375f};
    const int height = input->shape[1];
    const int width = input->shape[2];
    if (input->dtype != dl::DATA_TYPE_INT8 ||
        image.pix_type != dl::image::DL_IMAGE_PIX_TYPE_RGB888 ||
        image.width != width || image.height != height) {
        return 0;
    }

    const float inv_scale = 1.0f / std::ldexp(1.0f, input->exponent);
    const uint8_t *source = static_cast<const uint8_t *>(image.data);
    int8_t *destination = static_cast<int8_t *>(input->data);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            for (int channel = 0; channel < 3; channel++) {
                const size_t source_index =
                    ((size_t)y * width + x) * 3 + channel;
                const float normalized =
                    (source[source_index] - mean[channel]) / stddev[channel];
                const int quantized = static_cast<int>(
                    std::lround(normalized * inv_scale));
                destination[source_index] = static_cast<int8_t>(
                    std::clamp(quantized, -128, 127));
            }
        }
    }
    return esp_rom_crc32_le(
        0, reinterpret_cast<const uint8_t *>(destination),
        static_cast<uint32_t>(input->get_size()));
}

extern "C" void app_main(void)
{
    const size_t internal_before = free_internal();
    const size_t psram_before = free_psram();
    ESP_LOGI(TAG, "MEM_BEFORE internal=%zu psram=%zu", internal_before, psram_before);

    auto *model = new dl::Model(
        reinterpret_cast<const char *>(waste4_grouped_int8_espdl),
        fbs::MODEL_LOCATION_IN_FLASH_RODATA);
    if (!model) {
        ESP_LOGE(TAG, "S31_MODEL=AUTO_CHECK_FAIL reason=model_load");
        return;
    }

    auto inputs = model->get_inputs();
    auto outputs = model->get_outputs();
    if (inputs.size() != 1 || outputs.size() != 1) {
        ESP_LOGE(TAG, "S31_MODEL=AUTO_CHECK_FAIL reason=io_count inputs=%zu outputs=%zu",
                 inputs.size(), outputs.size());
        delete model;
        return;
    }

    dl::TensorBase *input = inputs.begin()->second;
    dl::TensorBase *output = outputs.begin()->second;
    ESP_LOGI(TAG,
             "MODEL_IO input=%s shape=[%d,%d,%d,%d] dtype=%d exponent=%d"
             " output=%s shape=[%d,%d] dtype=%d exponent=%d",
             inputs.begin()->first.c_str(),
             input->shape[0], input->shape[1], input->shape[2], input->shape[3],
             input->dtype, input->exponent,
             outputs.begin()->first.c_str(), output->shape[0], output->shape[1],
             output->dtype, output->exponent);

    const esp_err_t test_result = model->test();
    if (test_result != ESP_OK) {
        ESP_LOGE(TAG, "S31_MODEL=AUTO_CHECK_FAIL reason=model_self_test error=%s",
                 esp_err_to_name(test_result));
        delete model;
        return;
    }
    ESP_LOGI(TAG, "MODEL_SELF_TEST=PASS");

    std::vector<int64_t> durations_us;
    durations_us.reserve(20);
    for (int run = 0; run < 20; ++run) {
        const int64_t start = esp_timer_get_time();
        model->run();
        durations_us.push_back(esp_timer_get_time() - start);
    }
    std::sort(durations_us.begin(), durations_us.end());
    const int64_t sum_us = std::accumulate(durations_us.begin(), durations_us.end(), int64_t{0});
    const int64_t p50_us = durations_us[9];
    const int64_t p95_us = durations_us[18];

    const size_t internal_after = free_internal();
    const size_t psram_after = free_psram();
    ESP_LOGI(TAG,
             "MODEL_TIMING runs=20 mean_us=%" PRId64 " p50_us=%" PRId64 " p95_us=%" PRId64,
             sum_us / static_cast<int64_t>(durations_us.size()), p50_us, p95_us);
    ESP_LOGI(TAG,
             "MEM_AFTER internal=%zu psram=%zu internal_delta=%zu psram_delta=%zu",
             internal_after, psram_after,
             internal_before > internal_after ? internal_before - internal_after : 0,
             psram_before > psram_after ? psram_before - psram_after : 0);

    auto *preprocessor = new dl::image::ImagePreprocessor(
        model,
        {123.675f, 116.28f, 103.53f},
        {58.395f, 57.12f, 57.375f});
    int case_passes = 0;
    double confidence_delta_sum = 0;
    int confidence_count = 0;
    for (size_t index = 0; index < sizeof(ON_DEVICE_CASES) / sizeof(ON_DEVICE_CASES[0]); index++) {
        const auto &test_case = ON_DEVICE_CASES[index];
        dl::image::jpeg_img_t jpeg = {
            .data = const_cast<uint8_t *>(test_case.jpeg),
            .data_len = test_case.jpeg_size,
        };

        dl::image::img_t image = dl::image::hw_decode_jpeg(
            jpeg,
            dl::image::DL_IMAGE_PIX_TYPE_RGB888,
            1000,
            JPEG_YUV_RGB_CONV_STD_BT601);
        if (!image.data) {
            ESP_LOGE(TAG, "CASE_RESULT id=%s status=FAIL reason=jpeg_decode",
                     test_case.case_id);
            continue;
        }
        dl::image::img_t raw_image = {
            .data = const_cast<uint8_t *>(test_case.rgb),
            .width = static_cast<uint16_t>(image.width),
            .height = static_cast<uint16_t>(image.height),
            .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888,
        };
        if (test_case.rgb_size !=
            static_cast<uint32_t>(raw_image.width) * raw_image.height * 3) {
            ESP_LOGE(TAG, "CASE_RESULT id=%s status=FAIL reason=raw_size",
                     test_case.case_id);
            heap_caps_free(image.data);
            continue;
        }

        const int64_t pre_start = esp_timer_get_time();
        preprocessor->preprocess(raw_image, center_square(raw_image));
        const uint32_t preprocessor_crc = esp_rom_crc32_le(
            0, static_cast<const uint8_t *>(input->data),
            static_cast<uint32_t>(input->get_size()));
        const uint32_t explicit_crc = quantize_rgb888_image(input, raw_image);
        const int64_t pre_us = esp_timer_get_time() - pre_start;
        const int64_t model_start = esp_timer_get_time();
        model->run();
        const int64_t model_us = esp_timer_get_time() - model_start;

        const float *probabilities = static_cast<const float *>(output->data);
        int prediction = 0;
        for (int class_index = 1; class_index < output->get_size(); class_index++) {
            if (probabilities[class_index] > probabilities[prediction]) {
                prediction = class_index;
            }
        }
        const float confidence = probabilities[prediction];
        const bool passed = prediction == test_case.expected_index &&
                            std::abs(confidence - test_case.host_confidence) < 0.20f;
        case_passes += passed;
        confidence_delta_sum += std::abs(confidence - test_case.host_confidence);
        confidence_count++;
        ESP_LOGI(TAG,
                 "CASE_RESULT id=%s expected=%s predicted=%d confidence=%.6f"
                 " host_confidence=%.6f delta=%.6f pre_us=%" PRId64
                 " model_us=%" PRId64 " status=%s",
                 test_case.case_id, test_case.expected_class, prediction,
                 confidence, test_case.host_confidence,
                 std::abs(confidence - test_case.host_confidence),
                 pre_us, model_us, passed ? "PASS" : "FAIL");
        ESP_LOGI(TAG,
                 "PREPROCESS_CRC id=%s preprocessor=%08" PRIx32
                 " explicit=%08" PRIx32 " match=%s",
                 test_case.case_id, preprocessor_crc, explicit_crc,
                 preprocessor_crc == explicit_crc ? "yes" : "no");
        heap_caps_free(image.data);
    }
    delete preprocessor;
    const int case_count = sizeof(ON_DEVICE_CASES) / sizeof(ON_DEVICE_CASES[0]);
    const double mean_confidence_delta =
        confidence_count ? confidence_delta_sum / confidence_count : 0;
    ESP_LOGI(TAG,
             "ON_DEVICE_CASES pass=%d/%d mean_abs_confidence_delta=%.6f",
             case_passes, case_count, mean_confidence_delta);

    model->profile_memory();
    model->profile_module();
    ESP_LOGI(TAG,
             "S31_MODEL=%s self_test=pass runs=20 cases=%d/%d",
             case_passes == case_count && confidence_count == case_count ?
             "AUTO_CHECK_PASS" : "AUTO_CHECK_FAIL",
             case_passes, case_count);
    delete model;
}
