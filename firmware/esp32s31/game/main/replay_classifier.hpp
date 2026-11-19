#pragma once

#include <cstdint>

#include "stc_types.h"
#include "stc_vision_aggregate.h"

#ifdef __cplusplus
extern "C" {
#endif

stc_err_t stc_game_model_init(void);
stc_err_t stc_game_replay_infer_round(uint8_t round,
                                      stc_vision_frame_t frames[STC_VISION_BURST_FRAMES]);
bool stc_game_classify_rgb160(const uint8_t *rgb,
                              float probs[STC_MODEL_CLASSES]);
stc_err_t stc_game_replay_selftest(int *correct, int *total);
uint32_t stc_game_model_crc32(void);

#ifdef __cplusplus
}
#endif
