/* stc_vision.h — 影像 → 標籤（contract v1.0.0）
 * 實作責任：抓一幀、依 model/preprocess.json 前處理、跑模型、回標籤。
 * 不得：觸碰 LCD、改分數、阻塞超過 timeout。 */
#pragma once
#include "stc_types.h"

#define STC_VISION_CONTRACT_VERSION_MAJOR 1
#define STC_VISION_CONTRACT_VERSION_MINOR 0
#define STC_VISION_CONTRACT_VERSION_PATCH 0
#define STC_VISION_CONTRACT_VERSION_STRING "1.0.0"

#ifdef __cplusplus
extern "C" {
#endif

/* 載入模型與契約檔。model_dir 為 nullptr 時使用內嵌／預設路徑。 */
stc_err_t stc_vision_init(const char *model_dir);

/* 抓一幀並分類。成功時填 out 並回 STC_OK；超時回 STC_ERR_TIMEOUT。 */
stc_err_t stc_vision_capture(stc_label_t *out, uint32_t timeout_ms);

/* 釋放相機（回合結束時呼叫，讓 LCD／其他周邊使用） */
void stc_vision_release(void);

/* 目前實作版本："stub-1" 或 "<contract_version>:<sha8>" */
const char *stc_vision_version(void);

#ifdef __cplusplus
}
#endif
