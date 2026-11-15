/* test_dmv.c - host test of the boot FMV decoder (DC9C; owner: dreamcast).
 * test_dmv DIR: DIR has gen.py's clip.dmv, expected.bin, source.bin, audio.bin.
 *   - the header parses, bad headers do not;
 *   - every frame decodes to within 2 levels of an ideal float IDCT of the
 *     same quantised coefficients (the integer IDCT) and to RGB565 close to
 *     the source (the smooth frames);
 *   - the records walk to the end of the file with the audio of each intact;
 *   - a truncated frame and 3000 mutated ones never read or write outside
 *     their buffers (run under ASan: make SAN=1) and a short frame is refused;
 *   - grey planes convert to the grey RGB565 value. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dmv.h"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint8_t *slurp(const char *dir, const char *name, size_t *n)
{
    char p[1024];
    FILE *f;
    uint8_t *b;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    f = fopen(p, "rb");
    if (!f) { printf("cannot open %s\n", p); exit(2); }
    fseek(f, 0, SEEK_END);
    *n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc(*n + 1);
    if (fread(b, 1, *n, f) != *n) exit(2);
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    size_t fn, en, sn, an;
    uint8_t *file, *exp, *src, *aud, *planes, *bad, hdr[DMV_HDR_SIZE];
    DmvInfo in;
    uint32_t off = DMV_HDR_SIZE, next, vlen, last = 0;
    const uint8_t *v, *a;
    int f, i, psize;
    unsigned seed = 1;
    uint16_t *rgb;
    if (argc < 2) return 2;
    file = slurp(argv[1], "clip.dmv", &fn);
    exp = slurp(argv[1], "expected.bin", &en);
    src = slurp(argv[1], "source.bin", &sn);
    aud = slurp(argv[1], "audio.bin", &an);

    CHECK(dmv_parse_header(file, (uint32_t)fn, &in), "header");
    CHECK(in.w == 64 && in.h == 48 && in.fps == 15 && in.frames == 5 && in.arate == 22050 && in.apr == 1470,
          "fields %d %d %d %d", in.w, in.h, in.fps, in.frames);
    memcpy(hdr, file, sizeof hdr);
    CHECK(!dmv_parse_header(hdr, DMV_HDR_SIZE - 1, &in), "short header refused");
    hdr[0] = 'X';
    CHECK(!dmv_parse_header(hdr, DMV_HDR_SIZE, &in), "bad magic refused");
    memcpy(hdr, file, sizeof hdr);
    hdr[4] = 65;
    CHECK(!dmv_parse_header(hdr, DMV_HDR_SIZE, &in), "width not a multiple of 16 refused");
    memcpy(hdr, file, sizeof hdr);
    hdr[5] = 9;
    CHECK(!dmv_parse_header(hdr, DMV_HDR_SIZE, &in), "too wide refused");
    dmv_parse_header(file, (uint32_t)fn, &in);

    psize = in.w * in.h * 3 / 2;
    CHECK((size_t)psize * in.frames == en, "expected size");
    planes = malloc((size_t)psize + 64);
    rgb = malloc((size_t)in.w * in.h * 2);
    for (f = 0; f < in.frames; f++) {
        int worst = 0, d;
        double mse = 0;
        CHECK(dmv_record(&in, file, (uint32_t)fn, off, &v, &vlen, &a, &next), "record %d", f);
        CHECK(!memcmp(a, aud + (size_t)f * 1470 * 2, 1470 * 2), "audio of record %d", f);
        CHECK(dmv_decode(&in, v, vlen, planes), "decode %d", f);
        for (i = 0; i < psize; i++) {
            d = abs((int)planes[i] - (int)exp[(size_t)f * psize + i]);
            if (d > worst) worst = d;
        }
        CHECK(worst <= 2, "frame %d: integer IDCT %d off the float one", f, worst);
        dmv_to_rgb565(&in, planes, rgb, in.w);
        for (i = 0; i < in.w * in.h; i++) {
            const uint8_t *s = src + ((size_t)f * in.w * in.h + i) * 3;
            int r = (rgb[i] >> 11) << 3, g = ((rgb[i] >> 5) & 63) << 2, b = (rgb[i] & 31) << 3;
            mse += (r - s[0]) * (r - s[0]) + (g - s[1]) * (g - s[1]) + (b - s[2]) * (b - s[2]);
        }
        mse /= (double)in.w * in.h * 3;
        /* the hard stripes (3) and the noise (4) are lossy by design */
        if (f < 3) CHECK(10 * log10(255.0 * 255.0 / (mse + 1e-9)) > 28.0, "frame %d PSNR %.1f", f,
                         10 * log10(255.0 * 255.0 / (mse + 1e-9)));
        if (f == 3) {                          /* escape codes: the stripes still read as stripes */
            const uint8_t *s0 = planes + 10 * in.w;
            CHECK(abs((int)s0[20] - (int)s0[21]) > 150, "stripe contrast %d", abs((int)s0[20] - (int)s0[21]));
        }
        /* a frame cut short is refused */
        if (vlen > 8) {
            CHECK(!dmv_decode(&in, v, vlen - 8, planes), "frame %d cut by 8 bytes is refused", f);
            CHECK(!dmv_decode(&in, v, 0, planes), "an empty frame is refused");
        }
        last = off;
        off = next;
    }
    CHECK(off == fn, "the records end at the end of the file (%u of %zu)", (unsigned)off, fn);
    CHECK(!dmv_record(&in, file, (uint32_t)fn, (uint32_t)fn, &v, &vlen, &a, &next), "no record past the end");
    CHECK(!dmv_record(&in, file, (uint32_t)fn - 10, DMV_HDR_SIZE + 3000 * 100, &v, &vlen, &a, &next), "offset past the data");
    CHECK(!dmv_record(&in, file, (uint32_t)fn - 1, last, &v, &vlen, &a, &next), "a file cut inside the last record");

    /* mutated and random frames: no crash, nothing outside the buffers (ASan) */
    CHECK(dmv_record(&in, file, (uint32_t)fn, DMV_HDR_SIZE, &v, &vlen, &a, &next), "record 0 again");
    bad = malloc(vlen + 1);
    {
        uint8_t *guard = malloc(psize);                  /* exactly the plane size: ASan sees an overrun */
        for (i = 0; i < 3000; i++) {
            uint32_t k, n = vlen;
            memcpy(bad, v, vlen);
            for (k = 0; k < 1 + (uint32_t)(i % 6); k++) {
                seed = seed * 1103515245u + 12345u;
                bad[(seed >> 4) % vlen] = (uint8_t)(seed >> 11);
            }
            if (i % 5 == 0) { seed = seed * 1103515245u + 12345u; n = (seed >> 4) % vlen; }
            dmv_decode(&in, bad, n, guard);
        }
        free(guard);
    }
    free(bad);

    /* grey in, grey out; the exact RGB565 of Y 128 and of white */
    memset(planes, 128, (size_t)psize);
    dmv_to_rgb565(&in, planes, rgb, in.w);
    CHECK(rgb[0] == 0x8410 && rgb[in.w * in.h - 1] == 0x8410, "grey %04x", rgb[0]);
    memset(planes, 255, (size_t)in.w * in.h);
    memset(planes + in.w * in.h, 128, (size_t)psize - in.w * in.h);
    dmv_to_rgb565(&in, planes, rgb, in.w);
    CHECK(rgb[5] == 0xFFFF, "white %04x", rgb[5]);

    free(file); free(exp); free(src); free(aud); free(planes); free(rgb);
    printf("test_dmv: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
