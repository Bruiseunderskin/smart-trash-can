/* test_lid_retry.c — 300 ms 無 ACK 重試 2 次後回 IO（#53 H11） */
#include <stdio.h>
#include <string.h>

#include "stc_lid_link.h"

static int g_sends;
static stc_lidmsg_t g_last;

static int fake_send(const stc_lidmsg_t *msg, void *ctx)
{
    (void)ctx;
    g_sends++;
    g_last = *msg;
    return 0;
}

int main(void)
{
    stc_lid_transport_t tx = {fake_send, NULL};
    stc_lid_link_init(&tx);
    g_sends = 0;

    if (stc_lid_link_open(STC_BIN_FOOD, 0) != STC_OK) return 1;
    if (g_sends != 1) { fprintf(stderr, "first send %d\n", g_sends); return 1; }
    if (!stc_lidmsg_check(&g_last) || g_last.type != STC_LID_MSG_CMD || g_last.bin != 1) {
        fprintf(stderr, "command frame wrong\n");
        return 1;
    }

    if (stc_lid_link_poll(299) != STC_OK) return 1;
    if (g_sends != 1) { fprintf(stderr, "early retry\n"); return 1; }

    if (stc_lid_link_poll(300) != STC_OK) return 1;
    if (g_sends != 2) { fprintf(stderr, "retry 1 missing (%d)\n", g_sends); return 1; }

    if (stc_lid_link_poll(600) != STC_OK) return 1;
    if (g_sends != 3) { fprintf(stderr, "retry 2 missing (%d)\n", g_sends); return 1; }

    if (stc_lid_link_poll(900) != STC_ERR_IO) { fprintf(stderr, "no IO after retries\n"); return 1; }
    if (g_sends != 3) { fprintf(stderr, "extra send\n"); return 1; }

    /* ACK 路徑：同 seq 的 ACK 立即結束等待，不再重試 */
    g_sends = 0;
    if (stc_lid_link_open(STC_BIN_HAZARD, 1000) != STC_OK) return 1;
    stc_lidmsg_t ack = g_last;
    ack.type = STC_LID_MSG_ACK;
    stc_lidmsg_seal(&ack);
    stc_lid_link_on_frame(&ack, 1010);
    if (stc_lid_link_poll(1400) != STC_OK) return 1;
    if (g_sends != 1) { fprintf(stderr, "retried after ack (%d)\n", g_sends); return 1; }

    printf("test_lid_retry: 3 sends max, ack stops retry\n");
    return 0;
}
