/* test_virtual_hand.c — virtual S3 command, delayed DROP, retry and ACK. */
#include <stdio.h>
#include <string.h>

#include "stc_virtual_hand.h"

static stc_hand_msg_t command(stc_hand_action_t action, uint8_t seq,
                              uint8_t round)
{
    stc_hand_msg_t msg;
    memset(&msg, 0, sizeof msg);
    msg.sof = STC_HAND_MSG_SOF;
    msg.ver = STC_HAND_MSG_VERSION;
    msg.type = STC_HAND_MSG_CMD;
    msg.seq = seq;
    msg.round = round;
    msg.bin = 0xFF;
    msg.action = (uint8_t)action;
    stc_hand_msg_seal(&msg);
    return msg;
}

int main(void)
{
    stc_virtual_hand_init();
    stc_virtual_hand_output_t out;
    const stc_hand_msg_t open = command(STC_HAND_ACTION_OPEN_ALL, 7, 44);
    stc_virtual_hand_on_command(&open, 100, &out);
    if (out.count != 1 || out.msgs[0].type != STC_HAND_MSG_ACK ||
        out.msgs[0].seq != 7 || out.msgs[0].round != 44) {
        fprintf(stderr, "open ACK missing\n");
        return 1;
    }
    if (out.msgs[0].flags & STC_HAND_FLAG_OPEN) {
        fprintf(stderr, "open flag set before delay\n");
        return 1;
    }

    /* Retrying the same command only re-ACKs; it must not reset the drop. */
    stc_virtual_hand_on_command(&open, 200, &out);
    if (out.count != 1 || out.msgs[0].type != STC_HAND_MSG_ACK) {
        fprintf(stderr, "duplicate open ACK mismatch\n");
        return 1;
    }

    stc_virtual_hand_poll(1599, &out);
    if (out.count != 0) return 1;
    stc_virtual_hand_poll(1600, &out);
    if (out.count != 1 || out.msgs[0].type != STC_HAND_MSG_DROP ||
        out.msgs[0].bin != 1 || out.msgs[0].dist_mm != 40 ||
        out.msgs[0].round != 44 || out.msgs[0].seq != 7) {
        fprintf(stderr, "first virtual drop mismatch\n");
        return 1;
    }
    const uint8_t drop_seq = out.msgs[0].seq;
    stc_virtual_hand_poll(1900, &out);
    if (out.count != 1 || out.msgs[0].seq != drop_seq) return 1;
    stc_virtual_hand_poll(2200, &out);
    if (out.count != 1 || out.msgs[0].seq != drop_seq) return 1;
    stc_virtual_hand_poll(2500, &out);
    if (out.count != 0) return 1;

    /* A manual override affects the next open, and close still ACKs. */
    stc_virtual_hand_set_next_drop(3);
    const stc_hand_msg_t open2 = command(STC_HAND_ACTION_OPEN_ALL, 8, 45);
    stc_virtual_hand_on_command(&open2, 3000, &out);
    if (out.count != 1) return 1;
    stc_virtual_hand_poll(4500, &out);
    if (out.count != 1 || out.msgs[0].bin != 3 ||
        out.msgs[0].round != 45 || out.msgs[0].seq != 8) {
        return 1;
    }
    stc_hand_msg_t ack = out.msgs[0];
    ack.type = STC_HAND_MSG_ACK;
    ack.action = 0;
    stc_hand_msg_seal(&ack);
    stc_virtual_hand_on_ack(&ack);
    stc_virtual_hand_poll(4800, &out);
    if (out.count != 0) return 1;

    const stc_hand_msg_t close = command(STC_HAND_ACTION_CLOSE_ALL, 9, 45);
    stc_virtual_hand_on_command(&close, 5000, &out);
    if (out.count != 1 || out.msgs[0].type != STC_HAND_MSG_ACK ||
        !(out.msgs[0].flags & STC_HAND_FLAG_VIRTUAL)) {
        fprintf(stderr, "close ACK mismatch\n");
        return 1;
    }
    printf("test_virtual_hand: ACK, delayed DROP, retry and close OK\n");
    return 0;
}
