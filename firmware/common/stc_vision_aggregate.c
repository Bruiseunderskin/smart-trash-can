#include "stc_vision_aggregate.h"

#include <string.h>

#include "stc_vision_map.h"

static uint8_t argmax(const float probs[STC_MODEL_CLASSES])
{
    uint8_t best = 0;
    for (uint8_t i = 1; i < STC_MODEL_CLASSES; ++i) {
        if (probs[i] > probs[best]) {
            best = i;
        }
    }
    return best;
}

stc_err_t stc_vision_aggregate(const stc_vision_frame_t *frames, uint8_t count,
                               float threshold, stc_label_t *out,
                               stc_burst_detail_t *detail)
{
    if (!frames || !out || count == 0 || count > STC_VISION_BURST_FRAMES) {
        return STC_ERR_CONTRACT;
    }
    if (detail) {
        memset(detail, 0, sizeof *detail);
    }

    float average[STC_MODEL_CLASSES] = {0};
    uint8_t valid = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (!frames[i].ok) {
            continue;
        }
        valid++;
        for (uint8_t cls = 0; cls < STC_MODEL_CLASSES; ++cls) {
            average[cls] += frames[i].probs[cls];
        }
    }
    if (detail) {
        detail->frames_ok = valid;
    }
    if (valid < 3) {
        return STC_ERR_IO;
    }
    for (uint8_t cls = 0; cls < STC_MODEL_CLASSES; ++cls) {
        average[cls] /= (float)valid;
    }

    uint8_t top1 = argmax(average);
    uint8_t top2 = top1 == 0 ? 1 : 0;
    for (uint8_t cls = 0; cls < STC_MODEL_CLASSES; ++cls) {
        if (cls != top1 && average[cls] > average[top2]) {
            top2 = cls;
        }
    }

    const stc_bin_t bin = stc_vision_map_bin(top1);
    if (bin == STC_BIN_COUNT && top1 != STC_MODEL_CLASSES - 1) {
        return STC_ERR_CONTRACT;
    }

    memset(out, 0, sizeof *out);
    out->class_idx = top1;
    out->conf = average[top1];
    out->abstain = top1 == STC_MODEL_CLASSES - 1 ||
                   average[top1] < threshold;
    out->bin = out->abstain ? STC_BIN_COUNT : bin;
    out->src = STC_SRC_REAL;

    if (detail) {
        uint8_t agreement = 0;
        for (uint8_t i = 0; i < count; ++i) {
            if (!frames[i].ok) {
                detail->frame_bin[i] = 0xFF;
                continue;
            }
            const uint8_t frame_top = argmax(frames[i].probs);
            detail->frame_bin[i] = frame_top;
            detail->frame_conf[i] = frames[i].probs[frame_top];
            if (frame_top == top1) {
                agreement++;
            }
        }
        detail->agreement = agreement;
        detail->top2_bin = top2;
        detail->top2_conf = average[top2];
    }
    return STC_OK;
}
