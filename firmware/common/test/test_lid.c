/* test_lid.c — CDC 幀佈局、CRC、序號回繞、loopback 命令（#53 I5／H11） */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "stc_lid.h"
#include "stc_lidmsg.h"

int main(void)
{
    if (sizeof(stc_lidmsg_t) != 16) { fprintf(stderr, "frame size\n"); return 1; }
    if (offsetof(stc_lidmsg_t, tof_mm) != 6 || offsetof(stc_lidmsg_t, crc16) != 14) {
        fprintf(stderr, "offsets\n");
        return 1;
    }

    stc_lidmsg_t msg;
    memset(&msg, 0, sizeof msg);
    msg.sof = STC_LIDMSG_SOF;
    msg.ver = STC_LIDMSG_VER;
    msg.type = STC_LID_MSG_CMD;
    msg.seq = 255;
    msg.bin = 2;
    msg.action = 1;
    const uint16_t crc = stc_lidmsg_seal(&msg);
    if (!stc_lidmsg_check(&msg)) { fprintf(stderr, "crc rejected\n"); return 1; }

    msg.tof_mm[3] = 1234;
    if (stc_lidmsg_check(&msg)) { fprintf(stderr, "corruption accepted\n"); return 1; }
    msg.tof_mm[3] = 0;
    if (!stc_lidmsg_check(&msg)) { fprintf(stderr, "restore failed\n"); return 1; }

    /* 序號回繞：255 → 0 */
    msg.seq = (uint8_t)(msg.seq + 1);
    if (msg.seq != 0) { fprintf(stderr, "seq wrap\n"); return 1; }
    stc_lidmsg_seal(&msg);
    if (!stc_lidmsg_check(&msg)) { fprintf(stderr, "wrapped frame rejected\n"); return 1; }

    /* loopback：命令被記錄，假投放事件可取出 */
    const stc_lid_ops_t *lb = stc_lid_loopback();
    if (!lb || lb->open_bin(STC_BIN_FOOD) != STC_OK) { fprintf(stderr, "open_bin\n"); return 1; }
    stc_drop_t ev;
    if (lb->poll(&ev) != 0) { fprintf(stderr, "phantom event\n"); return 1; }
    stc_lid_loopback_inject_drop(STC_BIN_HAZARD, 33);
    if (lb->poll(&ev) != 1 || ev.bin != STC_BIN_HAZARD || ev.dist_mm != 33) {
        fprintf(stderr, "drop event\n");
        return 1;
    }
    if (stc_lid_stub()->open_bin(STC_BIN_FOOD) != STC_ERR_UNSUPPORTED) {
        fprintf(stderr, "stub must be unsupported\n");
        return 1;
    }

    printf("test_lid: 16-byte frame, crc %04x, seq wrap, loopback ok\n", crc);
    return 0;
}
