/* stc_virtual_hand.h — host-testable S3 placeholder actuator state. */
#pragma once

#include <stdint.h>

#include "stc_hand_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STC_VIRTUAL_HAND_OUTPUT_CAPACITY 4

typedef struct {
    stc_hand_msg_t msgs[STC_VIRTUAL_HAND_OUTPUT_CAPACITY];
    uint8_t count;
} stc_virtual_hand_output_t;

void stc_virtual_hand_init(void);
void stc_virtual_hand_on_command(const stc_hand_msg_t *msg, uint32_t now_ms,
                                 stc_virtual_hand_output_t *out);
void stc_virtual_hand_poll(uint32_t now_ms, stc_virtual_hand_output_t *out);
void stc_virtual_hand_on_ack(const stc_hand_msg_t *msg);
void stc_virtual_hand_set_next_drop(int8_t bin);
void stc_virtual_hand_set_pattern(const uint8_t pattern[5]);
void stc_virtual_hand_set_delay(uint32_t delay_ms);
uint32_t stc_virtual_hand_delay(void);
bool stc_virtual_hand_open(void);
const uint8_t *stc_virtual_hand_pattern(void);

#ifdef __cplusplus
}
#endif
