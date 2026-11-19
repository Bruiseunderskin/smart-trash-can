#include "stc_hand_msg.h"

#include <stddef.h>

#include "stc_lidmsg.h"

_Static_assert(sizeof(stc_hand_msg_t) == STC_HAND_MSG_SIZE,
               "hand message must be 16 bytes");
_Static_assert(offsetof(stc_hand_msg_t, action) == 6,
               "action at offset 6");
_Static_assert(offsetof(stc_hand_msg_t, dist_mm) == 8,
               "dist_mm at offset 8");
_Static_assert(offsetof(stc_hand_msg_t, crc16) == 14,
               "crc16 at offset 14");

uint16_t stc_hand_msg_crc16(const uint8_t *data, unsigned len)
{
    return stc_lidmsg_crc16(data, len);
}

uint16_t stc_hand_msg_seal(stc_hand_msg_t *msg)
{
    if (!msg) {
        return 0;
    }
    msg->crc16 = stc_hand_msg_crc16(
        (const uint8_t *)msg, (unsigned)offsetof(stc_hand_msg_t, crc16));
    return msg->crc16;
}

bool stc_hand_msg_check(const stc_hand_msg_t *msg)
{
    if (!msg || msg->sof != STC_HAND_MSG_SOF ||
        msg->ver != STC_HAND_MSG_VERSION ||
        msg->type >= STC_HAND_MSG_TYPE_COUNT) {
        return false;
    }
    const uint16_t crc = stc_hand_msg_crc16(
        (const uint8_t *)msg, (unsigned)offsetof(stc_hand_msg_t, crc16));
    return crc == msg->crc16;
}
