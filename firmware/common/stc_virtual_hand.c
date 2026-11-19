#include "stc_virtual_hand.h"

#include <string.h>

#define DROP_RETRY_MS 300u
#define DROP_MAX_SENDS 3u

static uint8_t g_pattern[5] = {1, 0, 3, 0, 2};
static uint32_t g_delay_ms = 1500u;
static bool g_open;
static bool g_drop_pending;
static int8_t g_next_override;
static uint8_t g_round;
static uint8_t g_bin;
static uint8_t g_drop_seq;
static uint8_t g_drop_sends;
static uint32_t g_drop_due;
static uint8_t g_open_count;
static bool g_have_last_command;
static uint8_t g_last_seq;
static uint8_t g_last_round;
static uint8_t g_last_action;

static void append(stc_virtual_hand_output_t *out, const stc_hand_msg_t *msg)
{
    if (!out || out->count >= STC_VIRTUAL_HAND_OUTPUT_CAPACITY) {
        return;
    }
    out->msgs[out->count++] = *msg;
}

static stc_hand_msg_t base_msg(stc_hand_msg_type_t type)
{
    stc_hand_msg_t msg = {0};
    msg.sof = STC_HAND_MSG_SOF;
    msg.ver = STC_HAND_MSG_VERSION;
    msg.type = type;
    msg.bin = 0xFF;
    msg.flags = STC_HAND_FLAG_READY | STC_HAND_FLAG_VIRTUAL;
    return msg;
}

void stc_virtual_hand_init(void)
{
    static const uint8_t pattern[5] = {1, 0, 3, 0, 2};
    memcpy(g_pattern, pattern, sizeof pattern);
    g_delay_ms = 1500u;
    g_open = false;
    g_drop_pending = false;
    g_next_override = -1;
    g_round = 0;
    g_bin = 0xFF;
    g_drop_seq = 0;
    g_drop_sends = 0;
    g_drop_due = 0;
    g_open_count = 0;
    g_have_last_command = false;
    g_last_seq = 0;
    g_last_round = 0;
    g_last_action = 0;
}

void stc_virtual_hand_on_command(const stc_hand_msg_t *msg, uint32_t now_ms,
                                 stc_virtual_hand_output_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof *out);
    if (!stc_hand_msg_check(msg) || msg->type != STC_HAND_MSG_CMD ||
        (msg->action != STC_HAND_ACTION_OPEN_ALL &&
         msg->action != STC_HAND_ACTION_CLOSE_ALL)) {
        return;
    }

    stc_hand_msg_t ack = base_msg(STC_HAND_MSG_ACK);
    ack.seq = msg->seq;
    ack.round = msg->round;
    ack.action = msg->action;
    stc_hand_msg_seal(&ack);
    append(out, &ack);

    const bool duplicate =
        g_have_last_command && g_last_seq == msg->seq &&
        g_last_round == msg->round && g_last_action == msg->action;
    if (duplicate) {
        return;
    }

    if (msg->action == STC_HAND_ACTION_OPEN_ALL) {
        g_open = true;
        g_drop_pending = true;
        g_round = msg->round;
        g_bin = g_next_override >= 0
                    ? (uint8_t)g_next_override
                    : g_pattern[g_open_count % 5u];
        g_open_count++;
        g_next_override = -1;
        g_drop_seq = msg->seq;
        g_drop_sends = 0;
        g_drop_due = now_ms + g_delay_ms;
    } else if (msg->action == STC_HAND_ACTION_CLOSE_ALL) {
        g_open = false;
        g_drop_pending = false;
    }
    g_have_last_command = true;
    g_last_seq = msg->seq;
    g_last_round = msg->round;
    g_last_action = msg->action;
}

void stc_virtual_hand_poll(uint32_t now_ms, stc_virtual_hand_output_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof *out);
    if (!g_drop_pending || g_drop_sends >= DROP_MAX_SENDS ||
        (int32_t)(now_ms - g_drop_due) < 0) {
        return;
    }

    stc_hand_msg_t drop = base_msg(STC_HAND_MSG_DROP);
    drop.seq = g_drop_seq;
    drop.round = g_round;
    drop.bin = g_bin;
    drop.dist_mm = 40;
    drop.flags |= STC_HAND_FLAG_OPEN | STC_HAND_FLAG_DROP_VALID;
    stc_hand_msg_seal(&drop);
    append(out, &drop);
    g_drop_sends++;
    g_drop_due += DROP_RETRY_MS;
    if (g_drop_sends >= DROP_MAX_SENDS) {
        g_drop_pending = false;
    }
}

void stc_virtual_hand_on_ack(const stc_hand_msg_t *msg)
{
    if (!stc_hand_msg_check(msg) || msg->type != STC_HAND_MSG_ACK ||
        msg->round != g_round || msg->seq != g_drop_seq) {
        return;
    }
    g_drop_pending = false;
}

void stc_virtual_hand_set_next_drop(int8_t bin)
{
    g_next_override = bin >= 0 && bin < 4 ? bin : -1;
}

void stc_virtual_hand_set_pattern(const uint8_t pattern[5])
{
    if (!pattern) {
        return;
    }
    for (int i = 0; i < 5; ++i) {
        g_pattern[i] = pattern[i] < 4 ? pattern[i] : 0;
    }
}

void stc_virtual_hand_set_delay(uint32_t delay_ms)
{
    g_delay_ms = delay_ms;
}

uint32_t stc_virtual_hand_delay(void) { return g_delay_ms; }
bool stc_virtual_hand_open(void) { return g_open; }
const uint8_t *stc_virtual_hand_pattern(void) { return g_pattern; }
