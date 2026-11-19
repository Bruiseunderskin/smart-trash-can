/* stc_replay_pack.h — immutable replay images embedded in S31 flash. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STC_REPLAY_PACK_VERSION 1u
#define STC_REPLAY_ROUNDS 5u
#define STC_REPLAY_FRAMES 5u
#define STC_REPLAY_HEADER_SIZE 62u
#define STC_REPLAY_RECORD_SIZE 24u

typedef struct {
    const uint8_t *data;
    size_t size;
    uint16_t count;
    uint32_t seed;
} stc_replay_pack_t;

typedef struct {
    uint8_t class_index;
    char card_id[9];
    uint16_t variant;
    const uint8_t *jpeg;
    uint32_t jpeg_size;
    uint32_t crc32;
} stc_replay_item_t;

uint32_t stc_replay_crc32(const uint8_t *data, size_t len);
bool stc_replay_pack_init(stc_replay_pack_t *pack, const uint8_t *data,
                          size_t size);
bool stc_replay_pack_item(const stc_replay_pack_t *pack, uint16_t index,
                          stc_replay_item_t *out);
bool stc_replay_pack_frame(const stc_replay_pack_t *pack, uint8_t round,
                           uint8_t frame, stc_replay_item_t *out);

#ifdef __cplusplus
}
#endif
