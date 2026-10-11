/* png.c - a dependency-free PNG writer: indexed colour, zlib stream made
 * of stored (uncompressed) deflate blocks, CRC-32 and Adler-32 computed
 * here (owner: foundation). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"

static uint32_t crc_table[256];

static void crc_init(void)
{
    uint32_t n, k;
    if (crc_table[1]) return;
    for (n = 0; n < 256; n++) {
        uint32_t c = n;
        for (k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}

static uint32_t crc_update(uint32_t c, const uint8_t *buf, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c;
}

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

static int write_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[8];
    uint32_t c;
    put_be32(hdr, len);
    memcpy(hdr + 4, type, 4);
    c = crc_update(0xFFFFFFFFu, hdr + 4, 4);
    if (len) c = crc_update(c, data, len);
    c ^= 0xFFFFFFFFu;
    if (fwrite(hdr, 1, 8, f) != 8) return 0;
    if (len && fwrite(data, 1, len, f) != len) return 0;
    put_be32(hdr, c);
    return fwrite(hdr, 1, 4, f) == 4;
}

int nat_png_encode(const char *path, const uint8_t *pix, int w, int h,
                   const uint8_t *rgb_palette)
{
    static const uint8_t sig[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
    uint8_t ihdr[13];
    size_t raw_len = (size_t)(w + 1) * (size_t)h;
    size_t nblocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + nblocks * 5 + 4;
    uint8_t *raw, *z;
    size_t i, o = 0;
    uint32_t a = 1, b = 0;
    FILE *f;
    int y, ok;

    crc_init();
    raw = (uint8_t *)malloc(raw_len);
    z = (uint8_t *)malloc(z_len);
    if (!raw || !z) { free(raw); free(z); return 0; }
    for (y = 0; y < h; y++) {
        raw[(size_t)y * (w + 1)] = 0;            /* filter: none */
        memcpy(raw + (size_t)y * (w + 1) + 1, pix + (size_t)y * w, (size_t)w);
    }
    z[o++] = 0x78;
    z[o++] = 0x01;
    for (i = 0; i < raw_len; i += 65535) {
        size_t n = raw_len - i < 65535 ? raw_len - i : 65535;
        z[o++] = (uint8_t)(i + n >= raw_len ? 1 : 0);
        z[o++] = (uint8_t)(n & 0xFF);
        z[o++] = (uint8_t)(n >> 8);
        z[o++] = (uint8_t)(~n & 0xFF);
        z[o++] = (uint8_t)((~n >> 8) & 0xFF);
        memcpy(z + o, raw + i, n);
        o += n;
    }
    for (i = 0; i < raw_len; i++) {
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    put_be32(z + o, (b << 16) | a);
    o += 4;

    put_be32(ihdr, (uint32_t)w);
    put_be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;        /* bit depth */
    ihdr[9] = 3;        /* indexed colour */
    ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;

    f = fopen(path, "wb");
    ok = f != NULL;
    if (ok) ok = fwrite(sig, 1, 8, f) == 8;
    if (ok) ok = write_chunk(f, "IHDR", ihdr, 13);
    if (ok) ok = write_chunk(f, "PLTE", rgb_palette, 768);
    if (ok) ok = write_chunk(f, "IDAT", z, (uint32_t)o);
    if (ok) ok = write_chunk(f, "IEND", NULL, 0);
    if (f && fclose(f) != 0) ok = 0;
    free(raw);
    free(z);
    return ok;
}

int nat_png_write(const char *path, const uint8_t *pix, int w, int h, int zoom)
{
    uint8_t pal[768];
    uint8_t *big;
    int i, ok, x, y;
    for (i = 0; i < 256; i++) palette_rgb((uint8_t)i, &pal[i * 3], &pal[i * 3 + 1], &pal[i * 3 + 2]);
    if (zoom <= 1) return nat_png_encode(path, pix, w, h, pal);
    zoom = zoom > 8 ? 8 : zoom;
    big = (uint8_t *)malloc((size_t)w * zoom * h * zoom);
    if (!big) return 0;
    for (y = 0; y < h * zoom; y++)
        for (x = 0; x < w * zoom; x++)
            big[(size_t)y * w * zoom + x] = pix[(y / zoom) * w + x / zoom];
    ok = nat_png_encode(path, big, w * zoom, h * zoom, pal);
    free(big);
    return ok;
}
