// ESP32-S3 / ESP32-S31 TFLite-Micro inference probe
//
// 目的：唔接相機、唔接舵機，只回答一個問題——
//   「呢個 int8 模型在這塊板跑一次要幾多 ms？」
//
// 燒錄後由 monitor 讀出：
//   arena size / input shape / dtype / 20 次推理的 mean / p50 / p95 / top-1
//
// 改 model：把 main/models/waste4_int8.tflite 換成自己的模型即可，
// 檔名必須一致（CMakeLists 用 EMBED_FILES 嵌入）。

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// EMBED_FILES "models/waste4_int8.tflite" 產生的符號
extern const unsigned char waste4_int8_tflite_start[] asm("_binary_waste4_int8_tflite_start");
extern const unsigned char waste4_int8_tflite_end[] asm("_binary_waste4_int8_tflite_end");

static const char *TAG = "probe";
static constexpr int kRuns = 20;
#if CONFIG_TFLITE_PROBE_USE_ESP_NN
#define ESP_NN_MODE "on"
#else
#define ESP_NN_MODE "off"
#endif

namespace {

// 只註冊分類模型常用的算子。缺算子時 TFLM 會明確報錯，唔會靜靜失敗。
void build_resolver(tflite::MicroMutableOpResolver<10> &resolver) {
  resolver.AddConv2D();
  resolver.AddDepthwiseConv2D();
  resolver.AddFullyConnected();
  resolver.AddSoftmax();
  resolver.AddReshape();
  resolver.AddMaxPool2D();
  resolver.AddAveragePool2D();
  resolver.AddAdd();
  resolver.AddMul();
  resolver.AddQuantize();
}

double percentile(std::vector<double> v, double p) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  size_t idx = static_cast<size_t>(p * (v.size() - 1));
  return v[idx];
}

}  // namespace

extern "C" void app_main(void) {
  const int model_len = waste4_int8_tflite_end - waste4_int8_tflite_start;
  ESP_LOGI(TAG, "model=person_detect_int8 bytes=%d idf=%s", model_len, esp_get_idf_version());

  const tflite::Model *model = tflite::GetModel(waste4_int8_tflite_start);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "schema mismatch: model=%d runtime=%d", model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // arena 放 PSRAM（如果有），否則內部 RAM
  constexpr size_t kArenaSize = 4 * 1024 * 1024;
  uint8_t *arena = static_cast<uint8_t *>(
      heap_caps_malloc(kArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  bool arena_in_psram = arena != nullptr;
  if (!arena) {
    arena = static_cast<uint8_t *>(heap_caps_malloc(kArenaSize, MALLOC_CAP_8BIT));
    arena_in_psram = false;
  }
  if (!arena) {
    ESP_LOGE(TAG, "arena alloc failed (%u bytes)", (unsigned)kArenaSize);
    return;
  }
  ESP_LOGI(TAG, "arena: %u bytes in %s", (unsigned)kArenaSize, arena_in_psram ? "PSRAM" : "internal");

  tflite::MicroMutableOpResolver<10> resolver;
  build_resolver(resolver);

  tflite::MicroInterpreter interpreter(model, resolver, arena, kArenaSize);
  if (interpreter.AllocateTensors() != kTfLiteOk) {
    ESP_LOGE(TAG, "AllocateTensors failed — 多數係 arena 太細或缺算子");
    return;
  }
  ESP_LOGI(TAG, "arena used: %u bytes", (unsigned)interpreter.arena_used_bytes());

  TfLiteTensor *input = interpreter.input(0);
  TfLiteTensor *output = interpreter.output(0);
  ESP_LOGI(TAG, "input: type=%d dims=%d shape=[%d,%d,%d,%d]",
           input->type, input->dims->size,
           input->dims->size > 0 ? input->dims->data[0] : -1,
           input->dims->size > 1 ? input->dims->data[1] : -1,
           input->dims->size > 2 ? input->dims->data[2] : -1,
           input->dims->size > 3 ? input->dims->data[3] : -1);
  ESP_LOGI(TAG, "output: type=%d size=%d", output->type, output->bytes);

  // 填一個固定輸入（全 0 / 中位數），目的係量時間，唔係量準確率
  if (input->type == kTfLiteInt8) {
    memset(input->data.int8, 0, input->bytes);
  } else if (input->type == kTfLiteUInt8) {
    // 量化模型常見 uint8 輸入（例如 MobileNet quant）；全 0 只為量時間
    memset(input->data.uint8, 0, input->bytes);
  } else if (input->type == kTfLiteFloat32) {
    memset(input->data.f, 0, input->bytes);
  } else {
    ESP_LOGW(TAG, "unsupported input type %d", input->type);
  }

  std::vector<double> ms;
  ms.reserve(kRuns);
  for (int i = 0; i < kRuns; ++i) {
    const int64_t t0 = esp_timer_get_time();
    if (interpreter.Invoke() != kTfLiteOk) {
      ESP_LOGE(TAG, "Invoke failed at run %d", i);
      return;
    }
    const int64_t t1 = esp_timer_get_time();
    ms.push_back((t1 - t0) / 1000.0);  // us → ms
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // top-1（全 0 輸入時只作 sanity check，數值無意義）
  int best = 0;
  if (output->type == kTfLiteUInt8) {
    for (int i = 0; i < output->bytes; ++i) {
      if (output->data.uint8[i] > output->data.uint8[best]) best = i;
    }
  } else if (output->type == kTfLiteInt8) {
    for (int i = 0; i < output->bytes; ++i) {
      if (output->data.int8[i] > output->data.int8[best]) best = i;
    }
  } else if (output->type == kTfLiteFloat32) {
    for (int i = 0; i < (int)(output->bytes / sizeof(float)); ++i) {
      if (output->data.f[i] > output->data.f[best]) best = i;
    }
  }

  ESP_LOGI(TAG, "RESULT esp_nn=%s runs=%d mean=%.2fms p50=%.2fms p95=%.2fms min=%.2fms max=%.2fms top1=%d arena_used=%u",
           ESP_NN_MODE, kRuns,
           [&] { double s = 0; for (double v : ms) s += v; return s / ms.size(); }(),
           percentile(ms, 0.50), percentile(ms, 0.95),
           *std::min_element(ms.begin(), ms.end()),
           *std::max_element(ms.begin(), ms.end()),
           best, (unsigned)interpreter.arena_used_bytes());
}
