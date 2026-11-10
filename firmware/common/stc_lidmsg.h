/* stc_lidmsg.h — CDC 16 bytes 幀（#53 §6.4）；S31 與 S3 編譯同一份 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STC_LIDMSG_SOF 0xA5u
#define STC_LIDMSG_VER 1u

typedef enum {
    STC_LID_MSG_CMD = 0,
    STC_LID_MSG_ACK = 1,
    STC_LID_MSG_NODE_STATE = 2,
    STC_LID_MSG_NODE_FAULT = 3
} stc_lidmsg_type_t;

typedef struct __attribute__((packed)) {
    uint8_t  sof;        /* 0xA5 */
    uint8_t  ver;        /* 1 */
    uint8_t  type;
    uint8_t  seq;
    uint8_t  bin;        /* 0–3 */
    uint8_t  action;     /* 0 CLOSE_ALL, 1 OPEN */
    uint16_t tof_mm[4];
    uint16_t crc16;      /* CCITT-FALSE，覆蓋 sof 至 tof_mm */
} stc_lidmsg_t;

#define STC_LIDMSG_SIZE 16

/* CRC-16/CCITT-FALSE，覆蓋 sof 至 tof_mm（前 14 bytes） */
uint16_t stc_lidmsg_crc16(const uint8_t *data, unsigned len);
uint16_t stc_lidmsg_seal(stc_lidmsg_t *msg);
bool     stc_lidmsg_check(const stc_lidmsg_t *msg);

#ifdef __cplusplus
}
#endif
