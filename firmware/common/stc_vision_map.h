/* stc_vision_map.h — 模型 index → 桶位（#53 §6.1） */
#pragma once
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_MODEL_CLASSES 5

/* 固定表，與 classes.json v3.0.0 及 labels.txt 同序：
 * food → 1（廚餘）、hazardous → 2（有害）、other → 3（其他）、recyclable → 0（可回收）、
 * unknown → STC_BIN_COUNT（冇卡，不開桶，配合 stc_label_t.abstain） */
extern const uint8_t stc_model_to_bin[STC_MODEL_CLASSES];

/* 讀 labels.txt 並逐行核對次序。不符回 STC_ERR_CONTRACT；檔案不存在回 STC_ERR_IO。
 * 成功後 stc_vision_map_bin() 才可用；未載入時回 STC_BIN_COUNT。 */
stc_err_t stc_vision_map_load_labels(const char *path);

/* index 0–4 → 桶位；越界或未載入回 STC_BIN_COUNT。 */
stc_bin_t stc_vision_map_bin(uint8_t class_index);

/* 最後一次載入的標籤名（診斷用）；未載入回 NULL。 */
const char *stc_vision_map_label(uint8_t class_index);

#ifdef __cplusplus
}
#endif
