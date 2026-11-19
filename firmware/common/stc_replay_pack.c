#include "stc_replay_pack.h"

#include <string.h>

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint32_t stc_replay_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
        }
    }
    return ~crc;
}

static const uint8_t *record_at(const stc_replay_pack_t *pack, uint16_t index)
{
    return pack->data + STC_REPLAY_HEADER_SIZE +
           (size_t)STC_REPLAY_RECORD_SIZE * index;
}

static bool validate_record(const stc_replay_pack_t *pack, uint16_t index)
{
    const uint8_t *record = record_at(pack, index);
    const uint8_t id_len = record[1];
    const uint32_t offset = read_u32(record + 12);
    const uint32_t length = read_u32(record + 16);
    const size_t blob_start =
        STC_REPLAY_HEADER_SIZE + (size_t)STC_REPLAY_RECORD_SIZE * pack->count;
    if (record[0] >= 4 || id_len == 0 || id_len > 8 ||
        offset < blob_start || offset > pack->size ||
        length > pack->size - offset) {
        return false;
    }
    return stc_replay_crc32(pack->data + offset, length) ==
           read_u32(record + 20);
}

bool stc_replay_pack_init(stc_replay_pack_t *pack, const uint8_t *data,
                          size_t size)
{
    if (!pack || !data || size < STC_REPLAY_HEADER_SIZE ||
        memcmp(data, "STCR", 4) != 0 ||
        read_u16(data + 4) != STC_REPLAY_PACK_VERSION) {
        return false;
    }
    const uint16_t count = read_u16(data + 6);
    if (count == 0) {
        return false;
    }
    const size_t records_end =
        STC_REPLAY_HEADER_SIZE + (size_t)STC_REPLAY_RECORD_SIZE * count;
    if (records_end > size) {
        return false;
    }
    pack->data = data;
    pack->size = size;
    pack->count = count;
    pack->seed = read_u32(data + 8);
    for (uint16_t i = 0; i < count; ++i) {
        if (!validate_record(pack, i)) {
            return false;
        }
    }
    for (unsigned i = 0; i < STC_REPLAY_ROUNDS * STC_REPLAY_FRAMES; ++i) {
        if (read_u16(data + 12 + i * 2) >= count) {
            return false;
        }
    }
    return true;
}

bool stc_replay_pack_frame(const stc_replay_pack_t *pack, uint8_t round,
                           uint8_t frame, stc_replay_item_t *out)
{
    if (!pack || !pack->data || !out ||
        round >= STC_REPLAY_ROUNDS || frame >= STC_REPLAY_FRAMES) {
        return false;
    }
    const unsigned playlist_index = round * STC_REPLAY_FRAMES + frame;
    const uint16_t item_index =
        read_u16(pack->data + 12 + playlist_index * 2);
    return stc_replay_pack_item(pack, item_index, out);
}

bool stc_replay_pack_item(const stc_replay_pack_t *pack, uint16_t index,
                          stc_replay_item_t *out)
{
    if (!pack || !pack->data || !out || index >= pack->count) {
        return false;
    }
    const uint8_t *record = record_at(pack, index);
    const uint8_t id_len = record[1];
    const uint32_t offset = read_u32(record + 12);
    const uint32_t length = read_u32(record + 16);

    memset(out, 0, sizeof *out);
    out->class_index = record[0];
    memcpy(out->card_id, record + 4, id_len);
    out->card_id[id_len] = '\0';
    out->variant = read_u16(record + 2);
    out->jpeg = pack->data + offset;
    out->jpeg_size = length;
    out->crc32 = read_u32(record + 20);
    return true;
}
