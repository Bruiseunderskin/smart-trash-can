/* test_hand_link.c — S31-side command ACK, retry, drop and offline logic. */
#include <stdio.h>
#include <string.h>

#include "stc_hand_link.h"

static stc_hand_msg_t g_sent[16];
static int g_sent_count;
static int g_drop_count;
static int g_drop_attempts;
static stc_hand_msg_t g_last_drop;
static bool g_accept_drop;
static int g_ack_count;
static bool g_ack_success;
static stc_hand_action_t g_ack_action;
static uint32_t g_ack_round_id;

static int transport_send(const stc_hand_msg_t *msg, void *ctx)
{
    (void)ctx;
    if (g_sent_count < (int)(sizeof(g_sent) / sizeof(g_sent[0]))) {
        g_sent[g_sent_count] = *msg;
    }
    g_sent_count++;
    return 0;
}

static bool on_drop(const stc_hand_msg_t *msg, uint32_t round_id, void *ctx)
{
    (void)ctx;
    g_drop_attempts++;
    if (round_id != g_ack_round_id) {
        return false;
    }
    if (!g_accept_drop) {
        return false;
    }
    g_drop_count++;
    g_last_drop = *msg;
    return true;
}

static void on_ack(stc_hand_action_t action, bool success, uint32_t round_id,
                   void *ctx)
{
    (void)ctx;
    g_ack_count++;
    g_ack_action = action;
    g_ack_success = success;
    g_ack_round_id = round_id;
}

static stc_hand_msg_t frame(stc_hand_msg_type_t type, uint8_t seq,
                            uint8_t round, uint8_t bin)
{
    stc_hand_msg_t msg;
    memset(&msg, 0, sizeof msg);
    msg.sof = STC_HAND_MSG_SOF;
    msg.ver = STC_HAND_MSG_VERSION;
    msg.type = type;
    msg.seq = seq;
    msg.round = round;
    msg.bin = bin;
    msg.flags = STC_HAND_FLAG_VIRTUAL;
    stc_hand_msg_seal(&msg);
    return msg;
}

static void reset(void)
{
    g_sent_count = 0;
    g_drop_count = 0;
    g_drop_attempts = 0;
    g_accept_drop = true;
    g_ack_count = 0;
    g_ack_success = false;
    g_ack_action = STC_HAND_ACTION_NONE;
    g_ack_round_id = 0;
    memset(g_sent, 0, sizeof g_sent);
    memset(&g_last_drop, 0, sizeof g_last_drop);
}

static int test_heartbeat_online(void)
{
    reset();
    const stc_hand_transport_t tx = {transport_send, NULL};
    stc_hand_link_init(&tx, NULL, NULL);
    if (stc_hand_link_online(0)) return 1;
    stc_hand_msg_t state = frame(STC_HAND_MSG_STATE, 1, 0, 0xFF);
    stc_hand_link_on_frame(&state, 100);
    if (!stc_hand_link_online(1099)) return 1;
    if (stc_hand_link_online(1100)) return 1;
    return 0;
}

static int test_command_retry_timeout(void)
{
    reset();
    const stc_hand_transport_t tx = {transport_send, NULL};
    stc_hand_link_init(&tx, NULL, NULL);
    if (stc_hand_link_command(STC_HAND_ACTION_OPEN_ALL, 0x1234u, 0) != STC_OK) return 1;
    if (g_sent[0].round != 0x34u) {
        fprintf(stderr, "wire round token=%u\n", (unsigned)g_sent[0].round);
        return 1;
    }
    if (g_sent_count != 1) return 1;
    if (stc_hand_link_poll(299) != STC_OK || g_sent_count != 1) return 1;
    if (stc_hand_link_poll(300) != STC_OK || g_sent_count != 2) return 1;
    if (stc_hand_link_poll(600) != STC_OK || g_sent_count != 3) return 1;
    if (stc_hand_link_poll(900) != STC_ERR_TIMEOUT) return 1;
    if (stc_hand_link_command_state() != STC_HAND_CMD_TIMEOUT) return 1;
    return 0;
}

static int test_ack_and_drop_dedup(void)
{
    reset();
    const stc_hand_hooks_t hooks = {.on_drop = on_drop, .on_ack = on_ack};
    const stc_hand_transport_t tx = {transport_send, NULL};
    stc_hand_link_init(&tx, &hooks, NULL);
    const uint32_t round_id = 0x1202u;
    if (stc_hand_link_command(STC_HAND_ACTION_OPEN_ALL, round_id, 0) != STC_OK) return 1;
    const uint8_t cmd_seq = g_sent[0].seq;

    stc_hand_msg_t stale_ack = frame(STC_HAND_MSG_ACK, cmd_seq, 0x11, 0xFF);
    stale_ack.action = STC_HAND_ACTION_OPEN_ALL;
    stc_hand_msg_seal(&stale_ack);
    stc_hand_link_on_frame(&stale_ack, 40);
    if (g_ack_count != 0 ||
        stc_hand_link_command_state() != STC_HAND_CMD_WAITING) {
        fprintf(stderr, "stale ACK accepted\n");
        return 1;
    }

    stc_hand_msg_t ack = frame(STC_HAND_MSG_ACK, cmd_seq, 0x02, 0xFF);
    ack.action = STC_HAND_ACTION_OPEN_ALL;
    stc_hand_msg_seal(&ack);
    stc_hand_link_on_frame(&ack, 50);
    if (stc_hand_link_command_state() != STC_HAND_CMD_ACKED) return 1;
    if (g_ack_count != 1 || !g_ack_success ||
        g_ack_action != STC_HAND_ACTION_OPEN_ALL ||
        g_ack_round_id != round_id) {
        fprintf(stderr, "ACK callback mismatch\n");
        return 1;
    }

    stc_hand_msg_t stale_drop = frame(STC_HAND_MSG_DROP, cmd_seq, 0x01, 0);
    stale_drop.dist_mm = 40;
    stc_hand_msg_seal(&stale_drop);
    stc_hand_link_on_frame(&stale_drop, 80);
    if (g_drop_count != 0) {
        fprintf(stderr, "stale DROP accepted\n");
        return 1;
    }

    stc_hand_msg_t drop = frame(STC_HAND_MSG_DROP, cmd_seq, 0x02, 1);
    drop.dist_mm = 40;
    stc_hand_msg_seal(&drop);
    stc_hand_link_on_frame(&drop, 100);
    stc_hand_link_on_frame(&drop, 400);
    if (g_drop_count != 1) {
        fprintf(stderr, "drop callback count=%d\n", g_drop_count);
        return 1;
    }
    if (g_last_drop.bin != 1) return 1;
    if (g_sent_count < 2 || g_sent[g_sent_count - 1].type != STC_HAND_MSG_ACK ||
        g_sent[g_sent_count - 1].seq != cmd_seq ||
        g_sent[g_sent_count - 1].round != 0x02) {
        fprintf(stderr, "drop ACK not sent\n");
        return 1;
    }

    stc_hand_msg_t stale = frame(STC_HAND_MSG_DROP, cmd_seq, 1, 0);
    stc_hand_link_on_frame(&stale, 500);
    if (g_drop_count != 1) return 1;
    return 0;
}

static int test_ack_timeout_callback(void)
{
    reset();
    const stc_hand_hooks_t hooks = {.on_ack = on_ack};
    const stc_hand_transport_t tx = {transport_send, NULL};
    stc_hand_link_init(&tx, &hooks, NULL);
    const uint32_t round_id = 0x2345u;
    if (stc_hand_link_command(STC_HAND_ACTION_OPEN_ALL, round_id, 0) != STC_OK) return 1;
    (void)stc_hand_link_poll(300);
    (void)stc_hand_link_poll(600);
    if (stc_hand_link_poll(900) != STC_ERR_TIMEOUT) return 1;
    if (g_ack_count != 1 || g_ack_success ||
        g_ack_action != STC_HAND_ACTION_OPEN_ALL ||
        g_ack_round_id != round_id) {
        fprintf(stderr, "timeout callback mismatch\n");
        return 1;
    }
    return 0;
}

static int test_early_drop_is_retried_after_rejection(void)
{
    reset();
    const stc_hand_hooks_t hooks = {.on_drop = on_drop, .on_ack = on_ack};
    const stc_hand_transport_t tx = {transport_send, NULL};
    stc_hand_link_init(&tx, &hooks, NULL);
    const uint32_t round_id = 0x3456u;
    if (stc_hand_link_command(STC_HAND_ACTION_OPEN_ALL, round_id, 0) != STC_OK) {
        return 1;
    }
    const uint8_t seq = g_sent[0].seq;
    stc_hand_msg_t ack = frame(STC_HAND_MSG_ACK, seq, 0x56, 0xFF);
    ack.action = STC_HAND_ACTION_OPEN_ALL;
    stc_hand_msg_seal(&ack);
    stc_hand_link_on_frame(&ack, 10);

    stc_hand_msg_t drop = frame(STC_HAND_MSG_DROP, seq, 0x56, 1);
    drop.dist_mm = 40;
    stc_hand_msg_seal(&drop);
    g_accept_drop = false;
    stc_hand_link_on_frame(&drop, 100);
    if (g_drop_attempts != 1 || g_drop_count != 0) {
        fprintf(stderr, "rejected early DROP was consumed\n");
        return 1;
    }
    if (g_sent_count != 1) {
        fprintf(stderr, "rejected early DROP was acknowledged\n");
        return 1;
    }

    g_accept_drop = true;
    stc_hand_link_on_frame(&drop, 400);
    stc_hand_link_on_frame(&drop, 700);
    if (g_drop_attempts != 2 || g_drop_count != 1) {
        fprintf(stderr, "accepted DROP dedup mismatch attempts=%d accepted=%d\n",
                g_drop_attempts, g_drop_count);
        return 1;
    }
    if (g_sent_count < 2 ||
        g_sent[g_sent_count - 1].type != STC_HAND_MSG_ACK ||
        g_sent[g_sent_count - 1].seq != seq) {
        fprintf(stderr, "accepted DROP was not acknowledged\n");
        return 1;
    }
    return 0;
}

static int test_hold_action_has_no_drop_window(void)
{
    reset();
    const stc_hand_hooks_t hooks = {.on_drop = on_drop, .on_ack = on_ack};
    const stc_hand_transport_t tx = {transport_send, NULL};
    stc_hand_link_init(&tx, &hooks, NULL);
    const uint32_t round_id = 0x4567u;
    if (stc_hand_link_command(STC_HAND_ACTION_OPEN_ALL_HOLD, round_id, 0) !=
        STC_OK) {
        fprintf(stderr, "hold command rejected\n");
        return 1;
    }
    if (g_sent_count != 1 ||
        g_sent[0].action != STC_HAND_ACTION_OPEN_ALL_HOLD) {
        fprintf(stderr, "hold command not transmitted\n");
        return 1;
    }
    const uint8_t seq = g_sent[0].seq;
    stc_hand_msg_t ack = frame(STC_HAND_MSG_ACK, seq, 0x67, 0xFF);
    ack.action = STC_HAND_ACTION_OPEN_ALL_HOLD;
    stc_hand_msg_seal(&ack);
    stc_hand_link_on_frame(&ack, 10);
    if (g_ack_count != 1 || !g_ack_success ||
        g_ack_action != STC_HAND_ACTION_OPEN_ALL_HOLD) {
        fprintf(stderr, "hold ACK mismatch\n");
        return 1;
    }

    stc_hand_msg_t drop = frame(STC_HAND_MSG_DROP, seq, 0x67, 2);
    drop.dist_mm = 40;
    stc_hand_msg_seal(&drop);
    stc_hand_link_on_frame(&drop, 100);
    if (g_drop_count != 0) {
        fprintf(stderr, "hold action opened a DROP window\n");
        return 1;
    }
    return 0;
}

int main(void)
{
    if (test_heartbeat_online()) return 1;
    if (test_command_retry_timeout()) return 1;
    if (test_ack_and_drop_dedup()) return 1;
    if (test_ack_timeout_callback()) return 1;
    if (test_early_drop_is_retried_after_rejection()) return 1;
    if (test_hold_action_has_no_drop_window()) return 1;
    printf("test_hand_link: online, correlation, retry, ACK, DROP and hold OK\n");
    return 0;
}
