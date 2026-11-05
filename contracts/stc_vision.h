/* stc_vision.h — 影像 → 標籤（contract v1.1.0）
 *
 * 實作責任：保持相機串流、抓 5 幀（每 50 ms 一張）、逐幀前處理＋推論、聚合、回 1 個標籤。
 * 不得：觸碰 LCD、改分數、阻塞超過 timeout。
 *
 * v1.1 變更（2026-11-05）：由「抓一幀」改為「連拍 5 幀 ＋ 聚合」。
 *   - 相機在 GAME 模式保持開著（stream_start），避免每回合重新 init 的 300–500 ms
 *   - 5 幀 = 200 ms 采集窗；逐幀推論後取平均 softmax 再 argmax
 *   - 必須回報每幀結果，報告要引用「幀間一致率」
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
    float    frame_conf[STC_VISION_BURST_FRAMES];  /* 逐幀置信度 */
    uint8_t  frames_ok;                            /* 成功解碼的幀數 */
    uint8_t  agreement;                            /* 與最終標籤一致的幀數（0–5） */
    uint32_t t_capture_ms;                         /* 采集窗實際長度 */
    uint32_t t_infer_ms;                           /* 5 幀推論總時間 */
} stc_burst_detail_t;

stc_err_t stc_vision_init(const char *model_dir);

/* 開／關相機串流。GAME 模式進入時開，離開（進 CHAT）時關。
 * 重複呼叫 idempotent。失敗回錯誤，但 stc_game 不得因此中止回合。 */
stc_err_t stc_vision_stream_start(void);
stc_err_t stc_vision_stream_stop(void);
bool      stc_vision_streaming(void);

/* 連拍 5 幀並分類。成功填 out（聚合後）與 detail（可 NULL），回 STC_OK。
 * 成功解碼幀數 < 3 → 回 STC_ERR_IO；總時間超 timeout → STC_ERR_TIMEOUT。 */
stc_err_t stc_vision_capture(stc_label_t *out, stc_burst_detail_t *detail,
                             uint32_t timeout_ms);

/* 逐幀分類結果（報告／除錯：把 5 幀原圖存 SD 時同時寫 CSV） */
stc_err_t stc_vision_dump_last(const char *dir);

void stc_vision_release(void);
const char *stc_vision_version(void);

#ifdef __cplusplus
}
#endif
