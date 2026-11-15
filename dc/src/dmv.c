/* dmv.c - decoder of the boot FMV (DC9C; owner: dreamcast). Format: dmv.h.
 * Integer only, so the host and the SH4 decode the same bytes to the same
 * pixels. */
#include <string.h>
#include "dmv.h"

static const uint8_t zz[64] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

/* a real coefficient is within +-1100; clamping keeps a corrupt frame's
 * arithmetic inside 32 bits */
static int clampc(int v) { return v < -2048 ? -2048 : v > 2048 ? 2048 : v; }

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int dmv_parse_header(const uint8_t *buf, uint32_t len, DmvInfo *o)
{
    int i;
    if (!buf || !o || len < DMV_HDR_SIZE) return 0;
    if (memcmp(buf, "DMV1", 4)) return 0;
    o->w = rd16(buf + 4);
    o->h = rd16(buf + 6);
    o->fps = buf[8];
    o->frames = rd16(buf + 10);
    o->arate = rd32(buf + 12);
    o->apr = rd16(buf + 16);
    for (i = 0; i < 128; i++) o->dq[i >> 6][i & 63] = rd16(buf + 20 + i * 2);
    if (o->w < 16 || o->h < 16 || (o->w & 15) || (o->h & 15)) return 0;
    if (o->w > DMV_MAX_W || o->h > DMV_MAX_H) return 0;
    if (!o->fps || !o->frames || !o->arate || o->apr > 8192) return 0;
    return 1;
}

/* The coefficients of one block (natural order, dequantised, row r = vertical
 * frequency) into 8x8 pixels: the integer IDCT of the IJG's jidctint (the
 * "islow" one, 13-bit constants), which needs about a tenth of the multiplies
 * of the plain matrix form (the SH4 has no fast integer multiply-add). A
 * column or row with only a DC term is a copy. */
#define CB 13
#define P1 2
#define FIX(x) ((int)((x) * (1 << CB) + 0.5))
#define DESCALE(x, n) (((x) + (1 << ((n) - 1))) >> (n))

static void idct1(const int *in, int step, int *out, int ostep, int shift, int add)
{
    int z1, z2, z3, z4, z5, t0, t1, t2, t3, t10, t11, t12, t13;
    z2 = in[2 * step]; z3 = in[6 * step];
    z1 = (z2 + z3) * FIX(0.541196100);
    t2 = z1 + z3 * -FIX(1.847759065);
    t3 = z1 + z2 * FIX(0.765366865);
    z2 = in[0]; z3 = in[4 * step];
    t0 = (z2 + z3) * (1 << CB);
    t1 = (z2 - z3) * (1 << CB);
    t10 = t0 + t3; t13 = t0 - t3; t11 = t1 + t2; t12 = t1 - t2;
    t0 = in[7 * step]; t1 = in[5 * step]; t2 = in[3 * step]; t3 = in[1 * step];
    z1 = t0 + t3; z2 = t1 + t2; z3 = t0 + t2; z4 = t1 + t3;
    z5 = (z3 + z4) * FIX(1.175875602);
    t0 *= FIX(0.298631336); t1 *= FIX(2.053119869); t2 *= FIX(3.072711026); t3 *= FIX(1.501321110);
    z1 *= -FIX(0.899976223); z2 *= -FIX(2.562915447); z3 *= -FIX(1.961570560); z4 *= -FIX(0.390180644);
    z3 += z5; z4 += z5;
    t0 += z1 + z3; t1 += z2 + z4; t2 += z2 + z3; t3 += z1 + z4;
    out[0]         = DESCALE(t10 + t3, shift) + add;
    out[7 * ostep] = DESCALE(t10 - t3, shift) + add;
    out[ostep]     = DESCALE(t11 + t2, shift) + add;
    out[6 * ostep] = DESCALE(t11 - t2, shift) + add;
    out[2 * ostep] = DESCALE(t12 + t1, shift) + add;
    out[5 * ostep] = DESCALE(t12 - t1, shift) + add;
    out[3 * ostep] = DESCALE(t13 + t0, shift) + add;
    out[4 * ostep] = DESCALE(t13 - t0, shift) + add;
}

static uint8_t px(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

static void idct(const int *f, uint8_t *out, int stride, int dc_only)
{
    int ws[64], t[8], r, c, y;
    if (dc_only) {                                  /* a flat block */
        uint8_t v = px(DESCALE(f[0], 3) + 128);
        for (y = 0; y < 8; y++) memset(out + y * stride, v, 8);
        return;
    }
    for (c = 0; c < 8; c++) {                       /* vertical */
        if (!(f[8 + c] | f[16 + c] | f[24 + c] | f[32 + c] | f[40 + c] | f[48 + c] | f[56 + c])) {
            int dcv = f[c] * (1 << P1);
            for (r = 0; r < 8; r++) ws[r * 8 + c] = dcv;
            continue;
        }
        idct1(f + c, 8, ws + c, 8, CB - P1, 0);
    }
    for (r = 0; r < 8; r++) {                       /* horizontal */
        const int *w = ws + r * 8;
        if (!(w[1] | w[2] | w[3] | w[4] | w[5] | w[6] | w[7])) {
            uint8_t v = px(DESCALE(w[0], P1 + 3) + 128);
            memset(out + r * stride, v, 8);
            continue;
        }
        idct1(w, 1, t, 1, CB + P1 + 3, 128);
        for (c = 0; c < 8; c++) out[r * stride + c] = px(t[c]);
    }
}

/* one block; *pp moves on. 0: corrupt or out of bytes */
static int block(const uint8_t **pp, const uint8_t *end, const uint16_t *dq, int *dcp,
                 uint8_t *out, int stride)
{
    const uint8_t *p = *pp;
    int f[64], i, ac = 0;
    uint32_t v = 0;
    int shift = 0;
    memset(f, 0, sizeof(f));
    for (;;) {                          /* the DC difference: a varint */
        uint8_t b;
        if (p >= end || shift > 28) return 0;
        b = *p++;
        v |= (uint32_t)(b & 0x7F) << shift;
        shift += 7;
        if (!(b & 0x80)) break;
    }
    *dcp += (int)((v >> 1) & 0xFFFFF) ^ -(int)(v & 1);
    if (*dcp < -(1 << 20) || *dcp > (1 << 20)) return 0;
    f[0] = clampc(*dcp * dq[0]);
    i = 1;
    for (;;) {
        unsigned t;
        int run, code, level;
        if (p >= end) return 0;
        t = *p++;
        if (!t) break;
        if (t & 0x80) return 0;
        run = (int)(t >> 4);
        code = (int)(t & 15);
        if (!code) {                    /* skip 8 * run zeros */
            i += 8 * run;
            if (i > 64) return 0;
            continue;
        }
        i += run;
        if (i > 63) return 0;
        if (code == 15) {
            if (p >= end) return 0;
            level = (int8_t)*p++;
        } else {
            level = (code & 1) ? (code + 1) >> 1 : -(code >> 1);
        }
        f[zz[i]] = clampc(level * dq[i]);
        ac = 1;
        i++;
    }
    idct(f, out, stride, !ac);
    *pp = p;
    return 1;
}

int dmv_decode(const DmvInfo *in, const uint8_t *vid, uint32_t len, uint8_t *planes)
{
    const uint8_t *p = vid, *end = vid + len;
    int pl, bx, by;
    for (pl = 0; pl < 3; pl++) {
        int w = pl ? in->w / 2 : in->w, h = pl ? in->h / 2 : in->h;
        uint8_t *base = planes + (pl == 0 ? 0 : pl == 1 ? in->w * in->h : in->w * in->h * 5 / 4);
        const uint16_t *dq = in->dq[pl ? 1 : 0];
        int dc = 0;
        for (by = 0; by < h / 8; by++)
            for (bx = 0; bx < w / 8; bx++)
                if (!block(&p, end, dq, &dc, base + by * 8 * w + bx * 8, w)) return 0;
    }
    return 1;
}

int dmv_record(const DmvInfo *in, const uint8_t *file, uint32_t have, uint32_t off,
               const uint8_t **vid, uint32_t *vlen, const uint8_t **aud, uint32_t *next)
{
    uint32_t vb, ab = (uint32_t)in->apr * 2u;
    if (off > have || have - off < 4) return 0;
    vb = rd32(file + off);
    if (vb > have - off - 4 || ab > have - off - 4 - vb) return 0;
    *vid = file + off + 4;
    *vlen = vb;
    *aud = file + off + 4 + vb;
    *next = off + 4 + vb + ab;
    return 1;
}

/* clamp(v) for v in -256..511 */
static uint8_t clp[768];
static int clp_ok;

void dmv_to_rgb565(const DmvInfo *in, const uint8_t *planes, uint16_t *dst, int stride)
{
    int w = in->w, h = in->h, cx, cy;
    const uint8_t *py = planes, *pu = planes + w * h, *pv = pu + (w / 2) * (h / 2);
    if (!clp_ok) {
        int i;
        for (i = 0; i < 768; i++) clp[i] = (uint8_t)(i < 256 ? 0 : i > 511 ? 255 : i - 256);
        clp_ok = 1;
    }
    /* one chroma sample covers 2x2 pixels: its three terms are made once */
    for (cy = 0; cy < h / 2; cy++) {
        const uint8_t *y0 = py + cy * 2 * w, *y1 = y0 + w;
        const uint8_t *ur = pu + cy * (w / 2), *vr = pv + cy * (w / 2);
        uint16_t *d0 = dst + cy * 2 * stride, *d1 = d0 + stride;
        for (cx = 0; cx < w / 2; cx++) {
            int cb = ur[cx] - 128, cr = vr[cx] - 128;
            int dr = ((91881 * cr + 32768) >> 16) + 256;
            int dg = 256 - ((22554 * cb + 46802 * cr + 32768) >> 16);
            int db = ((116130 * cb + 32768) >> 16) + 256;
            int a = y0[cx * 2], b = y0[cx * 2 + 1], c = y1[cx * 2], e = y1[cx * 2 + 1];
            d0[cx * 2]     = (uint16_t)(((clp[a + dr] >> 3) << 11) | ((clp[a + dg] >> 2) << 5) | (clp[a + db] >> 3));
            d0[cx * 2 + 1] = (uint16_t)(((clp[b + dr] >> 3) << 11) | ((clp[b + dg] >> 2) << 5) | (clp[b + db] >> 3));
            d1[cx * 2]     = (uint16_t)(((clp[c + dr] >> 3) << 11) | ((clp[c + dg] >> 2) << 5) | (clp[c + db] >> 3));
            d1[cx * 2 + 1] = (uint16_t)(((clp[e + dr] >> 3) << 11) | ((clp[e + dg] >> 2) << 5) | (clp[e + db] >> 3));
        }
    }
}
