/* stc_hand_msg.h — S31/S3 ESP-NOW hand protocol v4. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STC_HAND_MSG_SOF 0xA5u
#define STC_HAND_MSG_VERSION 4u
#define STC_HAND_MSG_SIZE 16u

typedef enum {
    STC_HAND_MSG_CMD = 0,
    STC_HAND_MSG_ACK,
    STC_HAND_MSG_DROP,
    STC_HAND_MSG_STATE,
    STC_HAND_MSG_FAULT,
    STC_HAND_MSG_TYPE_COUNT
} stc_hand_msg_type_t;

typedef enum {
    STC_HAND_ACTION_NONE = 0,
    STC_HAND_ACTION_OPEN_ALL,
    STC_HAND_ACTION_CLOSE_ALL,
    STC_HAND_ACTION_OPEN_ALL_HOLD,
    STC_HAND_ACTION_PAUSE,
    STC_HAND_ACTION_RESUME,
} stc_hand_action_t;

enum {
    STC_HAND_FLAG_READY = 1u << 0,
    STC_HAND_FLAG_OPEN = 1u << 1,
    STC_HAND_FLAG_DROP_VALID = 1u << 2,
    STC_HAND_FLAG_VIRTUAL = 1u << 3,
};

typedef struct __attribute__((packed)) {
    uint8_t sof;
    uint8_t ver;
    uint8_t type;
    uint8_t seq;
    uint8_t round;
    uint8_t bin;
    uint8_t action;
    uint8_t flags;
    uint16_t dist_mm;
    uint16_t fault_mask;
    uint16_t reserved;
    uint16_t crc16;
} stc_hand_msg_t;

uint16_t stc_hand_msg_crc16(const uint8_t *data, unsigned len);
uint16_t stc_hand_msg_seal(stc_hand_msg_t *msg);
bool stc_hand_msg_check(const stc_hand_msg_t *msg);

#ifdef __cplusplus
}
#endif
