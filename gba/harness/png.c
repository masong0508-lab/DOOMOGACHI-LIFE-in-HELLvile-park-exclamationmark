/* png.c - tiny dependency-free PNG writer (stored deflate blocks) for the
 * GBA test harness (owner: gba). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "png.h"

static uint32_t crc_table[256];

static void crc_init(void)
{
    uint32_t n, c;
    int k;
    if (crc_table[1]) return;
    for (n = 0; n < 256; n++) {
        c = n;
        for (k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}

static uint32_t crc(uint32_t c, const uint8_t *buf, size_t len)
{
    size_t i;
    c ^= 0xFFFFFFFFu;
    for (i = 0; i < len; i++) c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static int chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[8];
    uint8_t tail[4];
    uint32_t c;
    put32(hdr, len);
    memcpy(hdr + 4, type, 4);
    c = crc(0, hdr + 4, 4);
    c = crc(c, data, len);
    put32(tail, c);
    return fwrite(hdr, 1, 8, f) == 8 && (len == 0 || fwrite(data, 1, len, f) == len) &&
           fwrite(tail, 1, 4, f) == 4;
}

int png_write_rgb(const char *path, const uint8_t *rgb, int w, int h)
{
    static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    size_t raw_len = (size_t)(w * 3 + 1) * (size_t)h;
    size_t nblocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + nblocks * 5 + 4;
    uint8_t *raw = malloc(raw_len), *z = malloc(z_len), *q;
    uint8_t ihdr[13];
    uint32_t a = 1, b = 0;
    size_t i, off;
    int y, ok;
    FILE *f;
    if (!raw || !z) { free(raw); free(z); return 0; }
    crc_init();
    for (y = 0; y < h; y++) {
        raw[(size_t)y * (w * 3 + 1)] = 0;
        memcpy(raw + (size_t)y * (w * 3 + 1) + 1, rgb + (size_t)y * w * 3, (size_t)w * 3);
    }
    q = z;
    *q++ = 0x78; *q++ = 0x01;
    for (off = 0; off < raw_len; ) {
        size_t n = raw_len - off;
        if (n > 65535) n = 65535;
        *q++ = (uint8_t)(off + n >= raw_len ? 1 : 0);
        *q++ = (uint8_t)(n & 0xFF); *q++ = (uint8_t)(n >> 8);
        *q++ = (uint8_t)(~n & 0xFF); *q++ = (uint8_t)((~n >> 8) & 0xFF);
        memcpy(q, raw + off, n);
        q += n;
        off += n;
    }
    for (i = 0; i < raw_len; i++) {
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    put32(q, (b << 16) | a);
    q += 4;
    put32(ihdr, (uint32_t)w);
    put32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    f = fopen(path, "wb");
    ok = f != NULL;
    if (ok) {
        ok = fwrite(sig, 1, 8, f) == 8 && chunk(f, "IHDR", ihdr, 13) &&
             chunk(f, "IDAT", z, (uint32_t)(q - z)) && chunk(f, "IEND", NULL, 0);
        ok = (fclose(f) == 0) && ok;
    }
    free(raw);
    free(z);
    return ok;
}
