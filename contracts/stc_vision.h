/* stc_vision.h — 影像 → 標籤（contract v1.2.0）
 *
 * 實作責任：**按需要開相機**、連拍 5 幀（每 50 ms 一張）、逐幀前處理＋推論、聚合、回 1 個標籤、關相機。
 * 不得：長期佔用相機、觸碰 LCD、改分數、阻塞超過 timeout。
 *
 * v1.2 變更（2026-11-05）：相機**只在掃卡時開**，掃完即關；取消 GAME 模式常開串流。
 *   v1.1 的連拍 5 幀（50 ms）與聚合規則保留。
 */
#pragma once
#include "stc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_VISION_BURST_FRAMES      5
#define STC_VISION_BURST_INTERVAL_MS 50

/* 每幀明細（報告證據用；可傳 NULL 省略） */
typedef struct {
    uint8_t  frame_bin[STC_VISION_BURST_FRAMES];   /* 逐幀 argmax（0–3，255=無效） */
    float    frame_conf[STC_VISION_BURST_FRAMES];
    uint8_t  frames_ok;                            /* 成功解碼幀數 */
    uint8_t  agreement;                            /* 與最終標籤一致的幀數 */
    uint32_t t_open_ms;                            /* 相機開啟耗時 */
    uint32_t t_capture_ms;                         /* 采集窗實際長度 */
    uint32_t t_infer_ms;                           /* 5 幀推論總時間 */
} stc_burst_detail_t;

stc_err_t stc_vision_init(const char *model_dir);

/* 掃描：開相機 → 連拍 5 幀（間隔 50 ms）→ 逐幀推論 → 平均 softmax → argmax → 關相機。
 * 一次呼叫完成整個生命週期；呼叫者不需要管理相機開關。
 * 成功解碼幀數 < 3 → STC_ERR_IO；總時間超 timeout → STC_ERR_TIMEOUT（仍會關相機）。 */
stc_err_t stc_vision_scan(stc_label_t *out, stc_burst_detail_t *detail,
                          uint32_t timeout_ms);

/* 逐幀結果寫 CSV（報告證據） */
stc_err_t stc_vision_dump_last(const char *dir);

void stc_vision_release(void);
const char *stc_vision_version(void);

#ifdef __cplusplus
}
#endif
