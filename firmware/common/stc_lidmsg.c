#include "stc_lidmsg.h"

#include <stdbool.h>
#include <stddef.h>

_Static_assert(sizeof(stc_lidmsg_t) == STC_LIDMSG_SIZE, "lidmsg must be 16 bytes");
_Static_assert(offsetof(stc_lidmsg_t, tof_mm) == 6, "tof_mm at offset 6");
_Static_assert(offsetof(stc_lidmsg_t, crc16) == 14, "crc16 at offset 14");

uint16_t stc_lidmsg_crc16(const uint8_t *data, unsigned len)
{
    uint16_t crc = 0xFFFFu;
    for (unsigned i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint16_t stc_lidmsg_seal(stc_lidmsg_t *msg)
{
    msg->crc16 = stc_lidmsg_crc16((const uint8_t *)msg, (unsigned)offsetof(stc_lidmsg_t, crc16));
    return msg->crc16;
}

bool stc_lidmsg_check(const stc_lidmsg_t *msg)
{
    if (!msg || msg->sof != STC_LIDMSG_SOF || msg->ver != STC_LIDMSG_VER) {
        return false;
    }
    const uint16_t crc = stc_lidmsg_crc16((const uint8_t *)msg, (unsigned)offsetof(stc_lidmsg_t, crc16));
    return crc == msg->crc16;
}
