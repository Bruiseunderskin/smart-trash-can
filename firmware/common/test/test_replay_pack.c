/* test_replay_pack.c — replay pack bounds and playlist parser. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stc_replay_pack.h"

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

int main(void)
{
    uint8_t pack[128];
    memset(pack, 0, sizeof pack);
    memcpy(pack, "STCR", 4);
    put16(pack + 4, 1);
    put16(pack + 6, 2);
    put32(pack + 8, 20261111);
    for (int i = 0; i < 25; ++i) {
        put16(pack + 12 + i * 2, (uint16_t)(i == 24 ? 1 : 0));
    }
    const size_t records = 62;
    const size_t blobs = records + 24 * 2;
    const uint8_t jpeg0[] = {0xff, 0xd8, 0x00};
    const uint8_t jpeg1[] = {0xff, 0xd8, 0x01};
    memcpy(pack + blobs, jpeg0, sizeof jpeg0);
    memcpy(pack + blobs + sizeof jpeg0, jpeg1, sizeof jpeg1);

    uint8_t *r0 = pack + records;
    r0[0] = 1;
    r0[1] = 4;
    put16(r0 + 2, 0);
    memcpy(r0 + 4, "B-14", 4);
    put32(r0 + 12, (uint32_t)blobs);
    put32(r0 + 16, sizeof jpeg0);
    put32(r0 + 20, stc_replay_crc32(jpeg0, sizeof jpeg0));
    uint8_t *r1 = pack + records + 24;
    r1[0] = 3;
    r1[1] = 1;
    put16(r1 + 2, 0);
    memcpy(r1 + 4, "X", 1);
    put32(r1 + 12, (uint32_t)(blobs + sizeof jpeg0));
    put32(r1 + 16, sizeof jpeg1);
    put32(r1 + 20, stc_replay_crc32(jpeg1, sizeof jpeg1));

    stc_replay_pack_t parsed;
    if (!stc_replay_pack_init(&parsed, pack, blobs + sizeof jpeg0 + sizeof jpeg1)) {
        fprintf(stderr, "valid pack rejected\n");
        return 1;
    }
    if (parsed.seed != 20261111 || parsed.count != 2) return 1;
    stc_replay_item_t item;
    if (!stc_replay_pack_frame(&parsed, 0, 0, &item)) return 1;
    if (item.class_index != 1 || item.jpeg_size != sizeof jpeg0 ||
        memcmp(item.jpeg, jpeg0, sizeof jpeg0) != 0 || strcmp(item.card_id, "B-14") != 0) {
        fprintf(stderr, "first item mismatch\n");
        return 1;
    }
    if (!stc_replay_pack_frame(&parsed, 4, 4, &item) ||
        item.class_index != 3 || item.jpeg[2] != 1) {
        fprintf(stderr, "last item mismatch\n");
        return 1;
    }
    if (stc_replay_pack_frame(&parsed, 5, 0, &item)) return 1;

    const size_t size = blobs + sizeof jpeg0 + sizeof jpeg1;
    pack[0] = 0;
    if (stc_replay_pack_init(&parsed, pack, size)) return 1;
    printf("test_replay_pack: bounds and playlist OK\n");
    return 0;
}
