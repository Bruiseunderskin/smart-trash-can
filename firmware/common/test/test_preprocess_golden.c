/* test_preprocess_golden.c — UYVY→RGB→crop→160 的 golden 向量（#53 §6.2）
 *
 * fixtures/frame_640x480.uyvy  →  stc_preprocess_uyvy_to_rgb  →  sha256
 * 與 fixtures/expected_rgb160.sha256 比對；--update 會重寫 fixture（只可
 * 在明確知道改了前處理時用）。
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stc_preprocess.h"

#define FRAME_BYTES ((size_t)STC_FRAME_W * (size_t)STC_FRAME_H * 2u)

/* --- 精簡 SHA-256（public-domain 風格實作，只用於測試） --- */
typedef struct { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t fill; } sha256_ctx;

static const uint32_t K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256_block(sha256_ctx *c, const uint8_t *p)
{
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t)p[i*4] << 24 | (uint32_t)p[i*4+1] << 16 | (uint32_t)p[i*4+2] << 8 | p[i*4+3];
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ror(w[i-15], 7) ^ ror(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = ror(w[i-2], 17) ^ ror(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = ror(e,6) ^ ror(e,11) ^ ror(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ror(a,2) ^ ror(a,13) ^ ror(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}

static void sha256_init(sha256_ctx *c)
{
    static const uint32_t init[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                                     0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    memcpy(c->h, init, sizeof init);
    c->len = 0; c->fill = 0;
}

static void sha256_update(sha256_ctx *c, const uint8_t *data, size_t n)
{
    c->len += n;
    while (n--) {
        c->buf[c->fill++] = *data++;
        if (c->fill == 64) { sha256_block(c, c->buf); c->fill = 0; }
    }
}

static void sha256_final(sha256_ctx *c, uint8_t out[32])
{
    uint64_t bits = c->len * 8u;
    uint8_t pad = 0x80;
    sha256_update(c, &pad, 1);
    uint8_t zero = 0;
    while (c->fill != 56) sha256_update(c, &zero, 1);
    uint8_t lenbuf[8];
    for (int i = 0; i < 8; ++i) lenbuf[i] = (uint8_t)(bits >> (56 - 8*i));
    c->len -= 0;  /* len 不再使用 */
    for (int i = 0; i < 8; ++i) { c->buf[c->fill++] = lenbuf[i]; }
    sha256_block(c, c->buf);
    for (int i = 0; i < 8; ++i) {
        out[i*4]   = (uint8_t)(c->h[i] >> 24);
        out[i*4+1] = (uint8_t)(c->h[i] >> 16);
        out[i*4+2] = (uint8_t)(c->h[i] >> 8);
        out[i*4+3] = (uint8_t)(c->h[i]);
    }
}

static int read_hex32(const char *path, char out[65])
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (fscanf(f, "%64s", out) != 1) { fclose(f); return -1; }
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    const int update = (argc > 1 && strcmp(argv[1], "--update") == 0);

    static uint8_t frame[FRAME_BYTES];
    static uint8_t rgb[STC_INPUT_SIZE * STC_INPUT_SIZE * 3];
    FILE *f = fopen("fixtures/frame_640x480.uyvy", "rb");
    if (!f) {
        fprintf(stderr, "cannot open fixtures/frame_640x480.uyvy\\n");
        return 2;
    }
    const size_t n = fread(frame, 1, sizeof frame, f);
    fclose(f);
    if (n != sizeof frame) {
        fprintf(stderr, "frame is %zu bytes, expected %zu\\n", n, sizeof frame);
        return 2;
    }

    if (stc_preprocess_uyvy_to_rgb(frame, STC_FRAME_W, STC_FRAME_H, rgb, STC_INPUT_SIZE) != STC_OK) {
        fprintf(stderr, "preprocess failed\\n");
        return 1;
    }

    sha256_ctx ctx;
    uint8_t digest[32];
    char hex[65];
    sha256_init(&ctx);
    sha256_update(&ctx, rgb, sizeof rgb);
    sha256_final(&ctx, digest);
    for (int i = 0; i < 32; ++i) snprintf(hex + i*2, 3, "%02x", digest[i]);

    char expected[65];
    if (update || read_hex32("fixtures/expected_rgb160.sha256", expected) != 0) {
        FILE *o = fopen("fixtures/expected_rgb160.sha256", "w");
        if (!o) { fprintf(stderr, "cannot write fixture\\n"); return 2; }
        fprintf(o, "%s\\n", hex);
        fclose(o);
        printf("test_preprocess_golden: fixture written %s\\n", hex);
        return 0;
    }

    if (strcmp(hex, expected) != 0) {
        fprintf(stderr, "tensor mismatch\\n  got      %s\\n  expected %s\\n", hex, expected);
        return 1;
    }
    printf("test_preprocess_golden: %s\\n", hex);
    return 0;
}
