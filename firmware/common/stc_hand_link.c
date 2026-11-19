#include "stc_hand_link.h"

#include <string.h>

#define HEARTBEAT_TIMEOUT_MS 1000u
#define ACK_TIMEOUT_MS 300u
#define ACK_MAX_RETRIES 2u

static stc_hand_transport_t g_tx;
static stc_hand_hooks_t g_hooks;
static void *g_user;
static stc_hand_cmd_state_t g_cmd_state;
static stc_hand_action_t g_action;
static uint32_t g_round_id;
static uint8_t g_next_seq;
static uint8_t g_cmd_seq;
static uint8_t g_retries;
static uint32_t g_cmd_sent_at;
static uint32_t g_last_rx_at;
static bool g_online;
static bool g_have_drop;
static stc_hand_msg_t g_last_drop;
static bool g_open_valid;
static uint32_t g_open_round_id;
static uint8_t g_open_seq;

static void set_online(bool online)
{
    if (g_online == online) {
        return;
    }
    g_online = online;
    if (g_hooks.on_online) {
        g_hooks.on_online(online, g_user);
    }
}

static void note_rx(uint32_t now_ms)
{
    g_last_rx_at = now_ms;
    set_online(true);
}

static int send_msg(const stc_hand_msg_t *msg)
{
    return g_tx.send ? g_tx.send(msg, g_tx.ctx) : -1;
}

static void report_ack(bool success)
{
    if (g_hooks.on_ack) {
        g_hooks.on_ack(g_action, success, g_round_id, g_user);
    }
}

static void send_ack(const stc_hand_msg_t *received, uint32_t now_ms)
{
    (void)now_ms;
    stc_hand_msg_t ack = {0};
    ack.sof = STC_HAND_MSG_SOF;
    ack.ver = STC_HAND_MSG_VERSION;
    ack.type = STC_HAND_MSG_ACK;
    ack.seq = received->seq;
    ack.round = received->round;
    ack.bin = received->bin;
    ack.flags = STC_HAND_FLAG_READY | STC_HAND_FLAG_DROP_VALID;
    stc_hand_msg_seal(&ack);
    (void)send_msg(&ack);
}

void stc_hand_link_init(const stc_hand_transport_t *transport,
                        const stc_hand_hooks_t *hooks, void *user_ctx)
{
    memset(&g_tx, 0, sizeof g_tx);
    memset(&g_hooks, 0, sizeof g_hooks);
    if (transport) {
        g_tx = *transport;
    }
    if (hooks) {
        g_hooks = *hooks;
    }
    g_user = user_ctx;
    g_cmd_state = STC_HAND_CMD_IDLE;
    g_action = STC_HAND_ACTION_NONE;
    g_round_id = 0;
    g_cmd_seq = 0;
    g_retries = 0;
    g_cmd_sent_at = 0;
    g_last_rx_at = 0;
    g_online = false;
    g_have_drop = false;
    g_open_valid = false;
    g_open_round_id = 0;
    g_open_seq = 0;
    memset(&g_last_drop, 0, sizeof g_last_drop);
}

stc_err_t stc_hand_link_command(stc_hand_action_t action, uint32_t round_id,
                                uint32_t now_ms)
{
    if (!g_tx.send ||
        (action != STC_HAND_ACTION_OPEN_ALL &&
         action != STC_HAND_ACTION_CLOSE_ALL &&
         action != STC_HAND_ACTION_OPEN_ALL_HOLD)) {
        return STC_ERR_UNSUPPORTED;
    }
    if (g_cmd_state == STC_HAND_CMD_WAITING) {
        return STC_ERR_NOT_READY;
    }

    stc_hand_msg_t msg = {0};
    msg.sof = STC_HAND_MSG_SOF;
    msg.ver = STC_HAND_MSG_VERSION;
    msg.type = STC_HAND_MSG_CMD;
    msg.seq = g_next_seq++;
    msg.round = (uint8_t)round_id;
    msg.bin = 0xFF;
    msg.action = (uint8_t)action;
    msg.flags = STC_HAND_FLAG_READY;
    stc_hand_msg_seal(&msg);

    g_action = action;
    g_round_id = round_id;
    g_cmd_seq = msg.seq;
    g_retries = 0;
    g_cmd_sent_at = now_ms;
    g_cmd_state = STC_HAND_CMD_WAITING;
    if (action == STC_HAND_ACTION_OPEN_ALL) {
        g_open_valid = true;
        g_open_round_id = round_id;
        g_open_seq = msg.seq;
        g_have_drop = false;
        memset(&g_last_drop, 0, sizeof g_last_drop);
    }
    if (send_msg(&msg) != 0) {
        g_cmd_state = STC_HAND_CMD_TIMEOUT;
        report_ack(false);
        return STC_ERR_IO;
    }
    return STC_OK;
}

void stc_hand_link_on_frame(const stc_hand_msg_t *msg, uint32_t now_ms)
{
    if (!stc_hand_msg_check(msg)) {
        return;
    }

    note_rx(now_ms);
    if (msg->type == STC_HAND_MSG_ACK) {
        if (g_cmd_state == STC_HAND_CMD_WAITING &&
            msg->seq == g_cmd_seq &&
            msg->round == (uint8_t)g_round_id &&
            msg->action == (uint8_t)g_action) {
            g_cmd_state = STC_HAND_CMD_ACKED;
            report_ack(true);
        }
        return;
    }

    if (msg->type != STC_HAND_MSG_DROP || msg->bin >= STC_BIN_COUNT ||
        !g_open_valid || msg->seq != g_open_seq ||
        msg->round != (uint8_t)g_open_round_id) {
        return;
    }

    const bool duplicate = g_have_drop &&
                           g_last_drop.round == msg->round &&
                           g_last_drop.seq == msg->seq;
    if (duplicate) {
        send_ack(msg, now_ms);
        return;
    }

    const bool accepted =
        g_hooks.on_drop ? g_hooks.on_drop(msg, g_open_round_id, g_user) : true;
    if (accepted) {
        g_last_drop = *msg;
        g_have_drop = true;
        send_ack(msg, now_ms);
    }
}

stc_err_t stc_hand_link_poll(uint32_t now_ms)
{
    if (g_online &&
        (uint32_t)(now_ms - g_last_rx_at) >= HEARTBEAT_TIMEOUT_MS) {
        set_online(false);
    }

    if (g_cmd_state != STC_HAND_CMD_WAITING ||
        (uint32_t)(now_ms - g_cmd_sent_at) < ACK_TIMEOUT_MS) {
        return STC_OK;
    }
    if (g_retries >= ACK_MAX_RETRIES) {
        g_cmd_state = STC_HAND_CMD_TIMEOUT;
        report_ack(false);
        return STC_ERR_TIMEOUT;
    }

    stc_hand_msg_t msg = {0};
    msg.sof = STC_HAND_MSG_SOF;
    msg.ver = STC_HAND_MSG_VERSION;
    msg.type = STC_HAND_MSG_CMD;
    msg.seq = g_cmd_seq;
    msg.round = (uint8_t)g_round_id;
    msg.bin = 0xFF;
    msg.action = (uint8_t)g_action;
    msg.flags = STC_HAND_FLAG_READY;
    stc_hand_msg_seal(&msg);
    if (send_msg(&msg) != 0) {
        g_cmd_state = STC_HAND_CMD_TIMEOUT;
        report_ack(false);
        return STC_ERR_IO;
    }
    g_retries++;
    g_cmd_sent_at = now_ms;
    return STC_OK;
}

bool stc_hand_link_online(uint32_t now_ms)
{
    return g_online &&
           (uint32_t)(now_ms - g_last_rx_at) < HEARTBEAT_TIMEOUT_MS;
}

stc_hand_cmd_state_t stc_hand_link_command_state(void) { return g_cmd_state; }
uint8_t stc_hand_link_last_seq(void) { return g_cmd_seq; }

bool stc_hand_link_last_drop(stc_hand_msg_t *out)
{
    if (!out || !g_have_drop) {
        return false;
    }
    *out = g_last_drop;
    return true;
}
