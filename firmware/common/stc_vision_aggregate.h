/* stc_vision_aggregate.h — five-frame model output aggregation. */
#pragma once

#include "stc_types.h"
#include "stc_vision.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool ok;
    float probs[STC_MODEL_CLASSES];
} stc_vision_frame_t;

stc_err_t stc_vision_aggregate(const stc_vision_frame_t *frames, uint8_t count,
                               float threshold, stc_label_t *out,
                               stc_burst_detail_t *detail);

#ifdef __cplusplus
}
#endif
