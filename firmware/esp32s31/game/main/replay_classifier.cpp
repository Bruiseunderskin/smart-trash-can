#include "replay_classifier.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "dl_image_define.hpp"
#include "dl_image_jpeg.hpp"
#include "dl_image_preprocessor.hpp"
#include "dl_model_base.hpp"
#include "dl_tensor_base.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "fbs_model.hpp"

#include "stc_replay_pack.h"
#include "stc_preprocess.h"
#include "stc_vision_map.h"

extern const uint8_t game_model_espdl[]
    asm("_binary_wordless_s31_espdl_start");
extern const uint8_t game_model_espdl_end[]
    asm("_binary_wordless_s31_espdl_end");
extern const uint8_t game_replay_pack_bin[]
    asm("_binary_game_replay_pack_bin_start");
extern const uint8_t game_replay_pack_bin_end[]
    asm("_binary_game_replay_pack_bin_end");

static const char *TAG = "stc_game_model";
static dl::Model *g_model;
static dl::image::ImagePreprocessor *g_preprocessor;
static dl::TensorBase *g_output_float;
static stc_replay_pack_t g_replay;
static uint32_t g_model_crc;

static int center_side(const dl::image::img_t &image)
{
    return std::min(image.width, image.height);
}

static bool classify_jpeg(const uint8_t *jpeg, uint32_t jpeg_size,
                          float probs[STC_MODEL_CLASSES])
{
    if (!g_model || !g_preprocessor || !jpeg || jpeg_size == 0) {
        return false;
    }
    dl::image::jpeg_img_t encoded = {
        .data = const_cast<uint8_t *>(jpeg),
        .data_len = jpeg_size,
    };
    dl::image::img_t image = dl::image::hw_decode_jpeg(
        encoded, dl::image::DL_IMAGE_PIX_TYPE_RGB888, 1000,
        JPEG_YUV_RGB_CONV_STD_BT601);
    if (!image.data) {
        return false;
    }
    const int side = center_side(image);
    const int left = (image.width - side) / 2;
    const int top = (image.height - side) / 2;
    g_preprocessor->preprocess(image, {left, top, left + side, top + side});
    g_model->run();

    auto outputs = g_model->get_outputs();
    if (outputs.size() != 1) {
        heap_caps_free(image.data);
        return false;
    }
    dl::TensorBase *output = outputs.begin()->second;
    if (output->get_size() != STC_MODEL_CLASSES) {
        heap_caps_free(image.data);
        return false;
    }
    g_output_float->assign(output);
    const float *values = static_cast<const float *>(g_output_float->data);
    memcpy(probs, values, STC_MODEL_CLASSES * sizeof(float));
    heap_caps_free(image.data);
    return true;
}

extern "C" bool stc_game_classify_rgb160(const uint8_t *rgb,
                                         float probs[STC_MODEL_CLASSES])
{
    if (!g_model || !g_preprocessor || !rgb || !probs) {
        return false;
    }
    dl::image::img_t image = {
        .data = const_cast<uint8_t *>(rgb),
        .width = STC_INPUT_SIZE,
        .height = STC_INPUT_SIZE,
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888,
    };
    g_preprocessor->preprocess(image);
    g_model->run();
    auto outputs = g_model->get_outputs();
    if (outputs.size() != 1) {
        return false;
    }
    dl::TensorBase *output = outputs.begin()->second;
    if (output->get_size() != STC_MODEL_CLASSES) {
        return false;
    }
    g_output_float->assign(output);
    memcpy(probs, g_output_float->data,
           STC_MODEL_CLASSES * sizeof(float));
    return true;
}

extern "C" stc_err_t stc_game_model_init(void)
{
    if (stc_vision_map_load_text(
            "food\nhazardous\nother\nrecyclable\nunknown\n", 40) != STC_OK) {
        ESP_LOGE(TAG, "MODEL_LABELS=FAIL");
        return STC_ERR_CONTRACT;
    }

    const size_t model_size = (size_t)(game_model_espdl_end - game_model_espdl);
    g_model_crc = stc_replay_crc32(game_model_espdl, model_size);
    if (g_model_crc != STC_GAME_MODEL_CRC) {
        ESP_LOGE(TAG, "MODEL_CRC=FAIL actual=%08x expected=%08x",
                 (unsigned)g_model_crc, (unsigned)STC_GAME_MODEL_CRC);
        return STC_ERR_CONTRACT;
    }

    const size_t pack_size =
        (size_t)(game_replay_pack_bin_end - game_replay_pack_bin);
    if (!stc_replay_pack_init(&g_replay, game_replay_pack_bin, pack_size) ||
        g_replay.count != 40 || g_replay.seed != 20261111u) {
        ESP_LOGE(TAG, "REPLAY_PACK=FAIL bytes=%u", (unsigned)pack_size);
        return STC_ERR_CONTRACT;
    }

    g_model = new dl::Model(
        reinterpret_cast<const char *>(game_model_espdl),
        fbs::MODEL_LOCATION_IN_FLASH_RODATA);
    if (!g_model) {
        return STC_ERR_IO;
    }
    g_preprocessor = new dl::image::ImagePreprocessor(
        g_model,
        {0.0f, 0.0f, 0.0f},
        {255.0f, 255.0f, 255.0f});
    if (!g_preprocessor) {
        delete g_model;
        g_model = nullptr;
        return STC_ERR_IO;
    }

    auto inputs = g_model->get_inputs();
    auto outputs = g_model->get_outputs();
    if (inputs.size() != 1 || outputs.size() != 1) {
        ESP_LOGE(TAG, "MODEL_IO=FAIL");
        return STC_ERR_CONTRACT;
    }
    dl::TensorBase *input = inputs.begin()->second;
    dl::TensorBase *output = outputs.begin()->second;
    g_output_float =
        new dl::TensorBase(output->shape, nullptr, 0, dl::DATA_TYPE_FLOAT);
    if (!g_output_float || output->get_size() != STC_MODEL_CLASSES) {
        ESP_LOGE(TAG, "MODEL_OUTPUT=FAIL");
        return STC_ERR_CONTRACT;
    }
    ESP_LOGI(TAG,
             "model_crc=%08x pack=%u bytes seed=%u labels=OK "
             "input_dtype=%d input_exp=%d input_shape=[%d,%d,%d,%d] "
             "output_dtype=%d output_exp=%d output_size=%d",
             (unsigned)g_model_crc, (unsigned)pack_size,
             (unsigned)g_replay.seed, (int)input->dtype,
             (int)input->exponent,
             (int)input->shape[0], (int)input->shape[1],
             (int)input->shape[2], (int)input->shape[3],
             (int)output->dtype, (int)output->exponent,
             (int)output->get_size());
    return STC_OK;
}

extern "C" stc_err_t stc_game_replay_infer_round(
    uint8_t round, stc_vision_frame_t frames[STC_VISION_BURST_FRAMES])
{
    if (!frames || round >= STC_REPLAY_ROUNDS) {
        return STC_ERR_CONTRACT;
    }
    memset(frames, 0, sizeof(stc_vision_frame_t) * STC_VISION_BURST_FRAMES);
    for (uint8_t frame = 0; frame < STC_REPLAY_FRAMES; ++frame) {
        stc_replay_item_t item;
        if (!stc_replay_pack_frame(&g_replay, round, frame, &item)) {
            return STC_ERR_IO;
        }
        frames[frame].ok = classify_jpeg(item.jpeg, item.jpeg_size,
                                         frames[frame].probs);
        ESP_LOGI(TAG, "replay round=%u frame=%u card=%s class=%u valid=%u",
                 (unsigned)round, (unsigned)frame, item.card_id,
                 (unsigned)item.class_index, frames[frame].ok ? 1u : 0u);
    }
    return STC_OK;
}

extern "C" stc_err_t stc_game_replay_selftest(int *correct, int *total)
{
    if (!correct || !total) {
        return STC_ERR_CONTRACT;
    }
    *correct = 0;
    *total = g_replay.count;
    for (uint16_t index = 0; index < g_replay.count; ++index) {
        stc_replay_item_t item;
        float probs[STC_MODEL_CLASSES];
        if (!stc_replay_pack_item(&g_replay, index, &item) ||
            !classify_jpeg(item.jpeg, item.jpeg_size, probs)) {
            ESP_LOGE(TAG, "SELFTEST index=%u decode=FAIL", (unsigned)index);
            continue;
        }
        int prediction = 0;
        for (int cls = 1; cls < STC_MODEL_CLASSES; ++cls) {
            if (probs[cls] > probs[prediction]) prediction = cls;
        }
        const bool ok = prediction == item.class_index;
        *correct += ok ? 1 : 0;
        ESP_LOGI(TAG,
                 "SELFTEST index=%u card=%s expected=%u predicted=%u conf=%.6f %s",
                 (unsigned)index, item.card_id, (unsigned)item.class_index,
                 (unsigned)prediction, (double)probs[prediction],
                 ok ? "PASS" : "FAIL");
    }
    ESP_LOGI(TAG, "REPLAY_SELFTEST=%s correct=%d total=%d",
             *correct == *total ? "PASS" : "FAIL", *correct, *total);
    return *correct == *total ? STC_OK : STC_ERR_CONTRACT;
}

extern "C" uint32_t stc_game_model_crc32(void) { return g_model_crc; }
