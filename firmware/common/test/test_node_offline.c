/* test_node_offline.c — 500 ms 無 NODE_STATE 即離線（#53 H11） */
#include <stdio.h>
#include <string.h>

#include "stc_lid_link.h"

static int drop_send(const stc_lidmsg_t *m, void *c) { (void)m; (void)c; return 0; }

int main(void)
{
    stc_lid_transport_t tx = {drop_send, NULL};
    stc_lid_link_init(&tx);

    if (stc_lid_link_node_online(0)) { fprintf(stderr, "online before any frame\n"); return 1; }

    stc_lidmsg_t state;
    memset(&state, 0, sizeof state);
    state.sof = STC_LIDMSG_SOF;
    state.ver = STC_LIDMSG_VER;
    state.type = STC_LID_MSG_NODE_STATE;
    state.tof_mm[0] = 120;
    stc_lidmsg_seal(&state);

    stc_lid_link_on_frame(&state, 1000);
    if (!stc_lid_link_node_online(1000)) { fprintf(stderr, "not online\n"); return 1; }
    if (!stc_lid_link_node_online(1499)) { fprintf(stderr, "went offline early\n"); return 1; }
    if (stc_lid_link_node_online(1500)) { fprintf(stderr, "still online after 500 ms\n"); return 1; }

    /* CRC 錯的幀不可當作心跳 */
    stc_lidmsg_t bad = state;
    bad.tof_mm[1] = 999;
    stc_lid_link_on_frame(&bad, 2000);
    if (stc_lid_link_node_online(2000)) { fprintf(stderr, "bad crc counted as heartbeat\n"); return 1; }

    printf("test_node_offline: offline after 500 ms, crc enforced\n");
    return 0;
}
