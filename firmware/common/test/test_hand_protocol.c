/* test_hand_protocol.c — 16-byte S31/S3 ESP-NOW message contract. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "stc_hand_msg.h"

int main(void)
{
    if (sizeof(stc_hand_msg_t) != STC_HAND_MSG_SIZE) {
        fprintf(stderr, "size=%u\n", (unsigned)sizeof(stc_hand_msg_t));
        return 1;
    }
    if (offsetof(stc_hand_msg_t, action) != 6 ||
        offsetof(stc_hand_msg_t, dist_mm) != 8 ||
        offsetof(stc_hand_msg_t, crc16) != 14) {
        fprintf(stderr, "field offsets changed\n");
        return 1;
    }

    stc_hand_msg_t msg;
    memset(&msg, 0, sizeof msg);
    msg.sof = STC_HAND_MSG_SOF;
    msg.ver = STC_HAND_MSG_VERSION;
    msg.type = STC_HAND_MSG_CMD;
    msg.seq = 17;
    msg.round = 3;
    msg.bin = 2;
    msg.action = STC_HAND_ACTION_OPEN_ALL;
    msg.flags = STC_HAND_FLAG_VIRTUAL;
    const uint16_t crc = stc_hand_msg_seal(&msg);
    if (crc == 0 || !stc_hand_msg_check(&msg)) {
        fprintf(stderr, "valid frame rejected\n");
        return 1;
    }
    msg.round++;
    if (stc_hand_msg_check(&msg)) {
        fprintf(stderr, "corrupted frame accepted\n");
        return 1;
    }
    msg.round--;
    if (!stc_hand_msg_check(&msg)) {
        fprintf(stderr, "restored frame rejected\n");
        return 1;
    }
    msg.action = STC_HAND_ACTION_OPEN_ALL_HOLD;
    (void)stc_hand_msg_seal(&msg);
    if (!stc_hand_msg_check(&msg)) {
        fprintf(stderr, "hold action frame rejected\n");
        return 1;
    }

    printf("test_hand_protocol: 16-byte layout and CRC OK\n");
    return 0;
}
