/* stc_vision_map.h — 模型 index → 桶位（#53 §6.1） */
#pragma once
#include "stc_types.h"
#include "stc_vision.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 固定表，與 classes.json v3.0.0 及 models/wordless-unknown/labels.txt 同序：
 * food → 1、hazardous → 2、other → 3、recyclable → 0、unknown → STC_BIN_COUNT */
extern const uint8_t stc_model_to_bin[STC_MODEL_CLASSES];

/* 讀 labels.txt 並逐行核對次序。不符回 STC_ERR_CONTRACT；檔案不存在回 STC_ERR_IO。
 * 成功後 stc_vision_map_bin() 才可用；未載入時回 STC_BIN_COUNT。 */
stc_err_t stc_vision_map_load_labels(const char *path);
stc_err_t stc_vision_map_load_text(const char *text, size_t len);

/* index 0–4 → 桶位；unknown 和越界均回 STC_BIN_COUNT。 */
stc_bin_t stc_vision_map_bin(uint8_t class_index);

/* 最後一次載入的標籤名（診斷用）；未載入回 NULL。 */
const char *stc_vision_map_label(uint8_t class_index);

#ifdef __cplusplus
}
#endif
