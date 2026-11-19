/* test_vision_aggregate.c — five-frame softmax aggregation. */
#include <stdio.h>
#include <string.h>

#include "stc_vision_aggregate.h"
#include "stc_vision_map.h"

int main(void)
{
    if (stc_vision_map_load_text(
            "food\nhazardous\nother\nrecyclable\nunknown\n", 40) != STC_OK) {
        return 1;
    }
    stc_vision_frame_t frames[5];
    memset(frames, 0, sizeof frames);
    for (int i = 0; i < 5; ++i) {
        frames[i].ok = true;
        frames[i].probs[1] = 0.80f;
        frames[i].probs[2] = 0.15f;
        frames[i].probs[0] = 0.05f;
        frames[i].probs[3] = 0.00f;
    }
    frames[4].probs[1] = 0.20f;
    frames[4].probs[2] = 0.75f;

    stc_label_t out;
    stc_burst_detail_t detail;
    if (stc_vision_aggregate(frames, 5, 0.55f, &out, &detail) != STC_OK) return 1;
    if (out.class_idx != 1 || out.bin != STC_BIN_HAZARD || out.abstain) {
        fprintf(stderr, "aggregate class=%u bin=%d conf=%f\n",
                out.class_idx, (int)out.bin, (double)out.conf);
        return 1;
    }
    if (detail.frames_ok != 5 || detail.agreement != 4 ||
        detail.top2_bin != 2) {
        fprintf(stderr, "detail ok=%u agree=%u top2=%u\n",
                detail.frames_ok, detail.agreement, detail.top2_bin);
        return 1;
    }

    for (int i = 0; i < 5; ++i) {
        frames[i].probs[0] = 0.30f;
        frames[i].probs[1] = 0.25f;
        frames[i].probs[2] = 0.25f;
        frames[i].probs[3] = 0.20f;
    }
    if (stc_vision_aggregate(frames, 5, 0.55f, &out, &detail) != STC_OK ||
        !out.abstain || out.bin != STC_BIN_COUNT) {
        fprintf(stderr, "low confidence not abstained\n");
        return 1;
    }

    for (int i = 0; i < 5; ++i) {
        memset(frames[i].probs, 0, sizeof frames[i].probs);
        frames[i].ok = true;
        frames[i].probs[STC_MODEL_CLASSES - 1] = 0.95f;
        frames[i].probs[0] = 0.05f;
    }
    if (stc_vision_aggregate(frames, 5, 0.55f, &out, &detail) != STC_OK ||
        !out.abstain || out.class_idx != STC_MODEL_CLASSES - 1 ||
        out.bin != STC_BIN_COUNT) {
        fprintf(stderr, "unknown class did not abstain\n");
        return 1;
    }

    for (int i = 0; i < 5; ++i) {
        frames[i].probs[0] = 0.30f;
        frames[i].probs[1] = 0.25f;
        frames[i].probs[2] = 0.25f;
        frames[i].probs[3] = 0.20f;
        frames[i].probs[4] = 0.00f;
    }
    frames[0].ok = false;
    frames[1].ok = false;
    frames[2].ok = false;
    if (stc_vision_aggregate(frames, 5, 0.55f, &out, &detail) != STC_ERR_IO) {
        fprintf(stderr, "two valid frames accepted\n");
        return 1;
    }
    printf("test_vision_aggregate: mean softmax, top-two and abstain OK\n");
    return 0;
}
