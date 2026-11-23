#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t category_id;
    float p;
} stc_ui_bridge_candidate_t;

typedef struct {
    int32_t state;
    int32_t category_id;
    float confidence;
    stc_ui_bridge_candidate_t candidates[2];
    uint32_t score;
    uint32_t streak;
    bool lid_open;
    const char *error_code;
    uint8_t round;
    uint8_t rounds_total;
    uint32_t time_left_ms;
    uint32_t time_total_ms;
    uint8_t debug_flags;
    uint8_t reserved;
    int32_t scanned_bin;
    int32_t dropped_bin;
    int32_t correct_bin;
    int32_t outcome;
    int16_t round_delta;
} stc_ui_bridge_payload_t;

typedef void (*stc_ui_bridge_action_cb)(int32_t action, int32_t category_id,
                                        void *ctx);
typedef void (*stc_ui_bridge_retry_cb)(void *ctx);

void *stc_ui_bridge_create(void *parent, stc_ui_bridge_retry_cb retry,
                           void *ctx);
void stc_ui_bridge_destroy(void *ui);
void stc_ui_bridge_set_action_callback(void *ui,
                                       stc_ui_bridge_action_cb callback);
bool stc_ui_bridge_apply(void *ui, const stc_ui_bridge_payload_t *payload);
void stc_ui_bridge_set_images(void *ui, const void *original,
                              const void *cutout);
void stc_ui_bridge_show_chat(void *ui, bool voice_available);
bool stc_ui_bridge_show_intro(void *ui);
void stc_ui_bridge_show_guide(void *ui);

#ifdef __cplusplus
}
#endif
