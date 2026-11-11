#include "stc_lid_link.h"

#include <string.h>

static stc_lid_transport_t g_tx;
static uint8_t g_seq;
static bool g_waiting;
static uint32_t g_sent_at;
static uint8_t g_retries;
static stc_bin_t g_pending_bin;
static bool g_online;
static uint32_t g_last_state_ms;

void stc_lid_link_init(const stc_lid_transport_t *transport)
{
    memset(&g_tx, 0, sizeof g_tx);
    if (transport) {
        g_tx = *transport;
    }
    g_seq = 0;
    g_waiting = false;
    g_retries = 0;
    g_online = false;
    g_last_state_ms = 0;
}

uint8_t stc_lid_link_last_seq(void) { return g_seq; }

static stc_err_t transmit(stc_bin_t bin)
{
    if (!g_tx.send) {
        return STC_ERR_UNSUPPORTED;
    }
    stc_lidmsg_t msg;
    memset(&msg, 0, sizeof msg);
    msg.sof = STC_LIDMSG_SOF;
    msg.ver = STC_LIDMSG_VER;
    msg.type = STC_LID_MSG_CMD;
    msg.seq = g_seq;
    msg.bin = (uint8_t)bin;
    msg.action = 1;
    stc_lidmsg_seal(&msg);
    return (g_tx.send(&msg, g_tx.ctx) == 0) ? STC_OK : STC_ERR_IO;
}

stc_err_t stc_lid_link_open(stc_bin_t bin, uint32_t now_ms)
{
    g_pending_bin = bin;
    g_retries = 0;
    g_waiting = true;
    g_sent_at = now_ms;
    g_seq = (uint8_t)(g_seq + 1);
    return transmit(bin);
}

void stc_lid_link_on_frame(const stc_lidmsg_t *msg, uint32_t now_ms)
{
    if (!msg || !stc_lidmsg_check(msg)) {
        return;
    }
    if (msg->type == STC_LID_MSG_ACK && g_waiting && msg->seq == g_seq && msg->bin == (uint8_t)g_pending_bin) {
        g_waiting = false;
        return;
    }
    if (msg->type == STC_LID_MSG_NODE_STATE) {
        g_online = true;
        g_last_state_ms = now_ms;
    }
}

/* 由上層每次 tick 呼叫：逾時重試，重試用完即回 IO */
stc_err_t stc_lid_link_poll(uint32_t now_ms)
{
    if (!g_waiting) {
        return STC_OK;
    }
    if ((uint32_t)(now_ms - g_sent_at) < STC_LID_ACK_TIMEOUT_MS) {
        return STC_OK;
    }
    if (g_retries >= STC_LID_MAX_RETRY) {
        g_waiting = false;
        return STC_ERR_IO;
    }
    g_retries++;
    g_sent_at = now_ms;
    return transmit(g_pending_bin);
}

bool stc_lid_link_node_online(uint32_t now_ms)
{
    if (!g_online) {
        return false;
    }
    return (uint32_t)(now_ms - g_last_state_ms) < STC_LID_NODE_TIMEOUT_MS;
}
