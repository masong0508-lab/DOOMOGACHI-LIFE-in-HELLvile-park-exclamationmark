/* portrait_gfx.c - the tiny raster canvas behind every resident image
 * (owner: portrait). Integer-only shaded primitives, selective coloured
 * outlines and inner contours. See portrait_int.h for the model. */
#include "portrait_int.h"
#include "doomo/resident.h"

PCanvas pcv;
static int inv20(int r);

/* ---- math --------------------------------------------------------------- */
int p_abs(int v) { return v < 0 ? -v : v; }

int p_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return (int)r;
}

/* quarter sine, 65 entries, Q8 */
static const uint16_t qsin[65] = {
    0, 6, 13, 19, 25, 31, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92, 98, 104,
    109, 115, 121, 126, 132, 137, 142, 147, 152, 157, 162, 167, 172, 177,
    181, 185, 190, 194, 198, 202, 206, 209, 213, 216, 220, 223, 226, 229,
    231, 234, 237, 239, 241, 243, 245, 247, 248, 250, 251, 252, 253, 254,
    255, 255, 256, 256, 256
};

int p_sin(int a)
{
    a &= 255;
    if (a < 64) return qsin[a];
    if (a < 128) return qsin[128 - a];
    if (a < 192) return -qsin[a - 128];
    return -qsin[256 - a];
}

int p_cos(int a) { return p_sin(a + 64); }

uint32_t p_hash(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

/* ---- materials ------------------------------------------------------------ */
static uint8_t lumat[256];
static uint8_t lumat_ready;

static void luma_init(void)
{
    int i;
    if (lumat_ready) return;
    for (i = 0; i < 256; i++) lumat[i] = (uint8_t)DOOMO_CLAMP(palette_luma((uint8_t)i), 0, 255);
    lumat_ready = 1;
}

uint8_t pm_line_of(uint8_t c)
{
    int k = 0;
    luma_init();
    c = doomo_art_darker[c];
    while (lumat[c] > 78 && k < 4) { c = doomo_art_darker[c]; k++; }
    return c ? c : PAL_BLACK;
}

void pm_from3(PMat *m, uint8_t dark, uint8_t base, uint8_t light)
{
    m->c[0] = palette_darker(dark);
    if (!m->c[0]) m->c[0] = dark;
    m->c[1] = dark;
    m->c[2] = base;
    m->c[3] = light;
    m->c[4] = palette_lighter(light);
    if (!m->c[4]) m->c[4] = light;
    m->line = pm_line_of(dark);
}

void pm_paint(PMat *m, int paint)
{
    if (paint < 0 || paint >= PAL_PAINT_COUNT) paint = PAINT_WHITE;
    pm_from3(m, PAL_PAINT(paint, 0), PAL_PAINT(paint, 1), PAL_PAINT(paint, 2));
}

void pm_ramp(PMat *m, int ramp, int s)
{
    s = DOOMO_CLAMP(s, 1, 6);
    m->c[0] = PAL_RAMP(ramp, s > 1 ? s - 2 : 0);
    m->c[1] = PAL_RAMP(ramp, s - 1);
    m->c[2] = PAL_RAMP(ramp, s);
    m->c[3] = PAL_RAMP(ramp, s + 1);
    m->c[4] = PAL_RAMP(ramp, s + 1 < 7 ? s + 2 : 7);
    m->line = pm_line_of(m->c[1]);
}

void pm_skin(PMat *m, const DoomoLook *l)
{
    pm_from3(m, look_skin(l, 0), look_skin(l, 1), look_skin(l, 2));
}

void pm_flat(PMat *m, uint8_t c)
{
    m->c[0] = m->c[1] = m->c[2] = m->c[3] = m->c[4] = c;
    m->line = pm_line_of(c);
}

/* ---- canvas state ----------------------------------------------------------- */
void pc_begin(uint8_t *out, int w, int h)
{
    pcv.px = out;
    pcv.id = pcv.idu.b;
    pcv.w = w;
    pcv.h = h;
    memset(out, 0, (size_t)(w * h));
    memset(pcv.id, 0, (size_t)(w * h));
    pcv.part = 0;
    pcv.decal = 0;
    pcv.fon = 0;
    pcv.pat = NULL;
    pcv.masked = 0;
    pcv.bias = 0;
    pcv.ops = 0;
    pcv.prims = 0;
    pcv.gen++;
    pcv.x0 = 0; pcv.y0 = 0; pcv.x1 = w; pcv.y1 = h;
    pcv.pline[0] = 0;
}

void pc_part(const PMat *m, int shade, int flags)
{
    if (pcv.part < PID_FX - 1) pcv.part++;
    pcv.m = *m;
    pcv.shade = (uint8_t)shade;
    pcv.flags = (uint8_t)flags;
    pcv.decal = 0;
    pcv.pline[pcv.part] = m->line;
    pcv.pflag[pcv.part] = (uint8_t)flags;
    pcv.fon = 0;
    pcv.pat = NULL;
}

void pc_decal(const PMat *m, int shade)
{
    pcv.m = *m;
    pcv.shade = (uint8_t)shade;
    pcv.decal = 1;
    pcv.fon = 0;
    pcv.pat = NULL;
}

void pc_same(const PMat *m, int shade)
{
    pcv.m = *m;
    pcv.shade = (uint8_t)shade;
    pcv.decal = 0;
    pcv.pat = NULL;
}

void pc_frame(int cx, int cy, int rx, int ry)
{
    if (rx < 4) rx = 4;
    if (ry < 4) ry = 4;
    pcv.fon = 1;
    pcv.fcx = cx;
    pcv.fcy = cy;
    pcv.fiu = inv20(rx);
    pcv.fiv = inv20(ry);
}

void pc_noframe(void) { pcv.fon = 0; }

void pc_clip(int x0, int y0, int x1, int y1)
{
    pcv.x0 = DOOMO_MAX(x0, 0);
    pcv.y0 = DOOMO_MAX(y0, 0);
    pcv.x1 = DOOMO_MIN(x1, pcv.w);
    pcv.y1 = DOOMO_MIN(y1, pcv.h);
}

void pc_noclip(void) { pcv.x0 = 0; pcv.y0 = 0; pcv.x1 = pcv.w; pcv.y1 = pcv.h; }

void pc_mask_none(void) { pcv.masked = 0; memset(pcv.mask, 0, sizeof(pcv.mask)); }

void pc_mask_add(int id)
{
    if (id < 0 || id > 255) return;
    pcv.masked = 1;
    pcv.mask[id >> 5] |= 1u << (id & 31);
}

void pc_mask_range(int lo, int hi)
{
    int i;
    for (i = lo; i <= hi; i++) pc_mask_add(i);
}

void pc_pattern(const uint8_t *tile, const PMat *pm)
{
    pcv.pat = tile;
    if (pm) pcv.pm = *pm;
}

/* sqrt(65536 - k*k): half-width of a unit circle at height k (Q8) */
static const uint16_t circ[257] = {
    256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256,
    256, 256, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    254, 254, 254, 254, 254, 254, 254, 254, 253, 253, 253, 253, 253, 253,
    253, 252, 252, 252, 252, 252, 251, 251, 251, 251, 251, 250, 250, 250,
    250, 250, 249, 249, 249, 249, 248, 248, 248, 248, 247, 247, 247, 247,
    246, 246, 246, 245, 245, 245, 244, 244, 244, 244, 243, 243, 243, 242,
    242, 241, 241, 241, 240, 240, 240, 239, 239, 239, 238, 238, 237, 237,
    236, 236, 236, 235, 235, 234, 234, 233, 233, 233, 232, 232, 231, 231,
    230, 230, 229, 229, 228, 228, 227, 227, 226, 226, 225, 225, 224, 223,
    223, 222, 222, 221, 221, 220, 219, 219, 218, 218, 217, 216, 216, 215,
    214, 214, 213, 212, 212, 211, 210, 210, 209, 208, 207, 207, 206, 205,
    204, 204, 203, 202, 201, 201, 200, 199, 198, 197, 197, 196, 195, 194,
    193, 192, 191, 191, 190, 189, 188, 187, 186, 185, 184, 183, 182, 181,
    180, 179, 178, 177, 176, 175, 174, 173, 172, 170, 169, 168, 167, 166,
    165, 163, 162, 161, 160, 159, 157, 156, 155, 153, 152, 151, 149, 148,
    146, 145, 143, 142, 140, 139, 137, 136, 134, 133, 131, 129, 127, 126,
    124, 122, 120, 118, 116, 114, 112, 110, 108, 106, 104, 102, 99, 97,
    94, 92, 89, 86, 83, 81, 77, 74, 71, 67, 63, 59, 55, 50,
    45, 39, 32, 23, 0
};

/* 65536 / r (r >= 2), for capsule normals without a division */
static const uint16_t rcp[512] = {
    32768, 32768, 32768, 21845, 16384, 13107, 10922, 9362, 8192, 7281, 6553, 5957,
    5461, 5041, 4681, 4369, 4096, 3855, 3640, 3449, 3276, 3120, 2978, 2849,
    2730, 2621, 2520, 2427, 2340, 2259, 2184, 2114, 2048, 1985, 1927, 1872,
    1820, 1771, 1724, 1680, 1638, 1598, 1560, 1524, 1489, 1456, 1424, 1394,
    1365, 1337, 1310, 1285, 1260, 1236, 1213, 1191, 1170, 1149, 1129, 1110,
    1092, 1074, 1057, 1040, 1024, 1008, 992, 978, 963, 949, 936, 923,
    910, 897, 885, 873, 862, 851, 840, 829, 819, 809, 799, 789,
    780, 771, 762, 753, 744, 736, 728, 720, 712, 704, 697, 689,
    682, 675, 668, 661, 655, 648, 642, 636, 630, 624, 618, 612,
    606, 601, 595, 590, 585, 579, 574, 569, 564, 560, 555, 550,
    546, 541, 537, 532, 528, 524, 520, 516, 512, 508, 504, 500,
    496, 492, 489, 485, 481, 478, 474, 471, 468, 464, 461, 458,
    455, 451, 448, 445, 442, 439, 436, 434, 431, 428, 425, 422,
    420, 417, 414, 412, 409, 407, 404, 402, 399, 397, 394, 392,
    390, 387, 385, 383, 381, 378, 376, 374, 372, 370, 368, 366,
    364, 362, 360, 358, 356, 354, 352, 350, 348, 346, 344, 343,
    341, 339, 337, 336, 334, 332, 330, 329, 327, 326, 324, 322,
    321, 319, 318, 316, 315, 313, 312, 310, 309, 307, 306, 304,
    303, 302, 300, 299, 297, 296, 295, 293, 292, 291, 289, 288,
    287, 286, 284, 283, 282, 281, 280, 278, 277, 276, 275, 274,
    273, 271, 270, 269, 268, 267, 266, 265, 264, 263, 262, 261,
    260, 259, 258, 257, 256, 255, 254, 253, 252, 251, 250, 249,
    248, 247, 246, 245, 244, 243, 242, 241, 240, 240, 239, 238,
    237, 236, 235, 234, 234, 233, 232, 231, 230, 229, 229, 228,
    227, 226, 225, 225, 224, 223, 222, 222, 221, 220, 219, 219,
    218, 217, 217, 216, 215, 214, 214, 213, 212, 212, 211, 210,
    210, 209, 208, 208, 207, 206, 206, 205, 204, 204, 203, 202,
    202, 201, 201, 200, 199, 199, 198, 197, 197, 196, 196, 195,
    195, 194, 193, 193, 192, 192, 191, 191, 190, 189, 189, 188,
    188, 187, 187, 186, 186, 185, 185, 184, 184, 183, 183, 182,
    182, 181, 181, 180, 180, 179, 179, 178, 178, 177, 177, 176,
    176, 175, 175, 174, 174, 173, 173, 172, 172, 172, 171, 171,
    170, 170, 169, 169, 168, 168, 168, 167, 167, 166, 166, 165,
    165, 165, 164, 164, 163, 163, 163, 162, 162, 161, 161, 161,
    160, 160, 159, 159, 159, 158, 158, 157, 157, 157, 156, 156,
    156, 155, 155, 154, 154, 154, 153, 153, 153, 152, 152, 152,
    151, 151, 151, 150, 150, 149, 149, 149, 148, 148, 148, 147,
    147, 147, 146, 146, 146, 145, 145, 145, 144, 144, 144, 144,
    143, 143, 143, 142, 142, 142, 141, 141, 141, 140, 140, 140,
    140, 139, 139, 139, 138, 138, 138, 137, 137, 137, 137, 136,
    136, 136, 135, 135, 135, 135, 134, 134, 134, 134, 133, 133,
    133, 132, 132, 132, 132, 131, 131, 131, 131, 130, 130, 130,
    130, 129, 129, 129, 129, 128, 128, 128
};


/* (1 << 20) / r from the reciprocal table (no division for r < 512) */
static int inv20(int r)
{
    if (r < 2) r = 2;
    return r < 512 ? (int)rcp[r] << 4 : (1 << 20) / r;
}

/* ---- shader state ------------------------------------------------------------
 * Every primitive copies the canvas state it needs into a local Sh (the
 * stack: IWRAM on the GBA) so the inner loops never touch the big global
 * canvas struct, and spans shade incrementally (adds, a few multiplies).
 * Shading coordinates u, v are Q8 (256 = the rim of the shape). */
typedef struct Sh {
    uint8_t *px, *id;
    const uint32_t *mask;
    const uint8_t *pat;
    int w, x0, y0, x1, y1;
    int fon, fcx, fcy, fiu, fiv;
    int bias;
    uint8_t col[5], pcol[5];
    uint8_t part, decal, shade, fuzz;
    const uint8_t *lut;     /* band table of the shading mode */
} Sh;

static void sh_load(Sh *s)
{
    int k;
    pcv.prims++;
    s->px = pcv.px;
    s->id = pcv.id;
    s->mask = pcv.masked ? pcv.mask : NULL;
    s->pat = pcv.pat;
    s->w = pcv.w;
    s->x0 = pcv.x0; s->y0 = pcv.y0; s->x1 = pcv.x1; s->y1 = pcv.y1;
    s->fon = pcv.fon; s->fcx = pcv.fcx; s->fcy = pcv.fcy; s->fiu = pcv.fiu; s->fiv = pcv.fiv;
    s->bias = pcv.bias;
    if (!pcv.bias) {
        memcpy(s->col, pcv.m.c, 5);
        memcpy(s->pcol, pcv.pm.c, 5);
    } else for (k = 0; k < 5; k++) {
        int b = DOOMO_CLAMP(k + pcv.bias, 0, 4);
        s->col[k] = pcv.m.c[b];
        s->pcol[k] = pcv.pm.c[b];
    }
    s->part = pcv.part;
    s->decal = pcv.decal;
    s->shade = pcv.shade;
    s->fuzz = (pcv.flags & PF_FUZZ) != 0;
    s->lut = pl_bandlut[pcv.shade == SH_GLOSS ? 1 : (pcv.shade == SH_SOFT ? 2 : 0)];
}

static int lut_index(int q8)
{
    int i = (q8 + 256) >> 3;
    return i < 0 ? 0 : (i > 64 ? 64 : i);
}

static int grad_band(int shade, int v)
{
    if (shade == SH_VGRAD) return v < -110 ? 1 : (v < 60 ? 2 : 3);
    return v < -60 ? 3 : (v < 110 ? 2 : 1);   /* SH_HGRAD */
}

/* Fill pixels xa..xb of row y. uq/duq: shading u in Q20 at xa and per
 * pixel (u = uq >> 12); v: shading v (Q8) of the row. A shading frame, if
 * set, replaces both. */
P_FAST static void sh_span(const Sh *s, int y, int xa, int xb, int uq, int duq, int v)
{
    uint8_t *p, *q;
    int x, band = 2, flat;
    if (y < s->y0 || y >= s->y1) return;
    if (xa < s->x0) { uq += duq * (s->x0 - xa); xa = s->x0; }
    if (xb >= s->x1) xb = s->x1 - 1;
    if (xa > xb) return;
    if (s->fon) {
        uq = (xa * PQ + 8 - s->fcx) * s->fiu;
        duq = PQ * s->fiu;
        v = ((y * PQ + 8 - s->fcy) * s->fiv) >> 12;
    }
    pcv.ops += (uint32_t)(xb - xa + 1);
    p = s->px + y * s->w;
    q = s->id + y * s->w;
    flat = s->shade == SH_FLAT || s->shade == SH_VGRAD || s->shade == SH_HGRAD;
    if (s->shade == SH_VGRAD) band = v < -110 ? 1 : (v < 60 ? 2 : 3);
    else if (s->shade == SH_HGRAD) band = v < -60 ? 3 : (v < 110 ? 2 : 1);
    if (flat && !s->pat && !s->mask) {
        uint8_t c = s->col[band];
        if (s->decal) {
            for (x = xa; x <= xb; x++) p[x] = c;
        } else {
            uint8_t part = s->part;
            for (x = xa; x <= xb; x++) { p[x] = c; q[x] = part; }
        }
        return;
    }
    {
        int iv = (v + 256) >> 3;
        const uint8_t *lut = s->lut + (iv < 0 ? 0 : (iv > 64 ? 64 : iv)) * 65;
        int sh = s->fuzz ? (((xa ^ y) & 1) << 2) : 0, fz = s->fuzz ? 4 : 0;
        const uint8_t *col = s->col;
        uq += 256 << 12;
        if (!s->pat && !s->mask) {
            /* the common case: shaded, unmasked, plain */
            if (s->decal) {
                for (x = xa; x <= xb; x++, uq += duq, sh ^= fz) {
                    int iu = uq >> 15;
                    iu = iu < 0 ? 0 : (iu > 64 ? 64 : iu);
                    p[x] = col[(lut[iu] >> sh) & 15];
                }
            } else {
                uint8_t part = s->part;
                for (x = xa; x <= xb; x++, uq += duq, sh ^= fz) {
                    int iu = uq >> 15;
                    iu = iu < 0 ? 0 : (iu > 64 ? 64 : iu);
                    p[x] = col[(lut[iu] >> sh) & 15];
                    q[x] = part;
                }
            }
            return;
        }
        for (x = xa; x <= xb; x++, uq += duq, sh ^= fz) {
            int b, iu;
            if (s->mask) {
                int id = q[x];
                if (!(s->mask[id >> 5] & (1u << (id & 31)))) continue;
            }
            if (flat) b = band;
            else {
                iu = uq >> 15;
                iu = iu < 0 ? 0 : (iu > 64 ? 64 : iu);
                b = (lut[iu] >> sh) & 15;
            }
            if (s->pat) {
                int t = s->pat[((y & 7) << 3) | (x & 7)];
                p[x] = t == 1 ? s->pcol[b] : (t == 2 ? s->pcol[b > 0 ? b - 1 : 0] : col[b]);
            } else {
                p[x] = col[b];
            }
            if (!s->decal) q[x] = s->part;
        }
    }
}

/* single pixel with explicit shading coordinates */
static void sh_px(const Sh *s, int x, int y, int u, int v)
{
    int i, b;
    if (x < s->x0 || y < s->y0 || x >= s->x1 || y >= s->y1) return;
    i = y * s->w + x;
    if (s->mask) {
        int id = s->id[i];
        if (!(s->mask[id >> 5] & (1u << (id & 31)))) return;
    }
    if (s->fon) {
        u = ((x * PQ + 8 - s->fcx) * s->fiu) >> 12;
        v = ((y * PQ + 8 - s->fcy) * s->fiv) >> 12;
    }
    if (s->shade == SH_FLAT) b = 2;
    else if (s->shade == SH_VGRAD || s->shade == SH_HGRAD) b = grad_band(s->shade, v);
    else b = (s->lut[lut_index(v) * 65 + lut_index(u)] >> ((s->fuzz && ((x ^ y) & 1)) ? 4 : 0)) & 15;
    if (s->pat) {
        int t = s->pat[((y & 7) << 3) | (x & 7)];
        s->px[i] = t == 1 ? s->pcol[b] : (t == 2 ? s->pcol[b > 0 ? b - 1 : 0] : s->col[b]);
    } else {
        s->px[i] = s->col[b];
    }
    if (!s->decal) s->id[i] = s->part;
    pcv.ops++;
}

/* ---- primitives ----------------------------------------------------------------- */
void pc_ellipse(int cx, int cy, int rx, int ry)
{
    int y, ya, yb, iu, iv;
    Sh S;
    sh_load(&S);
    if (rx < 2 || ry < 2) { if (rx >= 0 && ry >= 0) sh_px(&S, cx >> 4, cy >> 4, 0, 0); return; }
    iu = inv20(rx);
    iv = inv20(ry);
    ya = (cy - ry) >> 4;
    yb = (cy + ry) >> 4;
    if (ya < S.y0) ya = S.y0;
    if (yb >= S.y1) yb = S.y1 - 1;
    for (y = ya; y <= yb; y++) {
        int dy = y * PQ + 8 - cy;
        int v = (dy * iv) >> 12, hw, xa, xb, av = v < 0 ? -v : v;
        if (av >= 256) continue;
        hw = (circ[av] * rx) >> 8;
        xa = (cx - hw - 8 + 15) >> 4;
        xb = (cx + hw - 8) >> 4;
        sh_span(&S, y, xa, xb, (xa * PQ + 8 - cx) * iu, PQ * iu, v);
    }
}

void pc_ellipse_sh(int cx, int cy, int rx, int ry, int shear)
{
    int x, y, xa, xb, ya, yb, iu, iv, ext;
    Sh S;
    sh_load(&S);
    if (rx < 2 || ry < 2) { sh_px(&S, cx >> 4, cy >> 4, 0, 0); return; }
    iu = inv20(rx);
    iv = inv20(ry);
    ext = (p_abs(shear) * rx) >> 8;
    xa = (cx - rx) >> 4; xb = (cx + rx) >> 4;
    ya = (cy - ry - ext) >> 4; yb = (cy + ry + ext) >> 4;
    pcv.ops += (uint32_t)((yb - ya + 1) * (xb - xa + 1));
    for (y = ya; y <= yb; y++)
        for (x = xa; x <= xb; x++) {
            int dx = x * PQ + 8 - cx, dy = y * PQ + 8 - cy - ((dx * shear) >> 8);
            int u = (dx * iu) >> 12, v = (dy * iv) >> 12;
            if (u * u + v * v <= 65536) sh_px(&S, x, y, u, v);
        }
}

/* x (Q4) of edge (ax,ay)-(bx,by) at height py, as a Q16 stepper */
typedef struct Edge { int x, dx; } Edge;

static void edge_at(Edge *e, int ax, int ay, int bx, int by, int py)
{
    int dy = by - ay;
    e->dx = dy ? ((bx - ax) << 12) / dy * PQ : 0;      /* Q12 per pixel row */
    e->x = (ax << 12) + (dy ? ((bx - ax) << 12) / dy * (py - ay) : 0);
}

void pc_tri(int x0, int y0, int x1, int y1, int x2, int y2)
{
    int y, ya, yb, fcx, fcy, iu, iv, rx, ry, t;
    Edge el, es;
    Sh S;
    if ((x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0) == 0) return;
    /* sort by y: (x0,y0) top, (x2,y2) bottom */
    if (y1 < y0) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y2 < y0) { t = x0; x0 = x2; x2 = t; t = y0; y0 = y2; y2 = t; }
    if (y2 < y1) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    sh_load(&S);
    /* own shading frame: centroid + half extents */
    fcx = (x0 + x1 + x2) / 3;
    fcy = (y0 + y1 + y2) / 3;
    rx = (DOOMO_MAX(x0, DOOMO_MAX(x1, x2)) - DOOMO_MIN(x0, DOOMO_MIN(x1, x2))) / 2 + 8;
    ry = (y2 - y0) / 2 + 8;
    iu = inv20(rx);
    iv = inv20(ry);
    /* rows whose centre lies inside [y0, y2) */
    ya = (y0 - 8 + 15) >> 4;
    yb = (y2 - 8 - 1) >> 4;
    if (ya < S.y0) ya = S.y0;
    if (yb >= S.y1) yb = S.y1 - 1;
    if (ya > yb) return;
    edge_at(&el, x0, y0, x2, y2, ya * PQ + 8);       /* the long edge */
    for (y = ya; y <= yb; y++) {
        int py = y * PQ + 8, xl, xr, xa, xb;
        if (py < y1) edge_at(&es, x0, y0, x1, y1, py);
        else edge_at(&es, x1, y1, x2, y2, py);
        xl = el.x >> 12;
        xr = es.x >> 12;
        if (xl > xr) { t = xl; xl = xr; xr = t; }
        xa = (xl - 8 + 15) >> 4;
        xb = (xr - 8) >> 4;
        if (xa <= xb)
            sh_span(&S, y, xa, xb, (xa * PQ + 8 - fcx) * iu, PQ * iu, ((py - fcy) * iv) >> 12);
        el.x += el.dx;
    }
}

void pc_quad(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3)
{
    int fon = pcv.fon;
    if (!fon) {
        int minx = DOOMO_MIN(DOOMO_MIN(x0, x1), DOOMO_MIN(x2, x3));
        int maxx = DOOMO_MAX(DOOMO_MAX(x0, x1), DOOMO_MAX(x2, x3));
        int miny = DOOMO_MIN(DOOMO_MIN(y0, y1), DOOMO_MIN(y2, y3));
        int maxy = DOOMO_MAX(DOOMO_MAX(y0, y1), DOOMO_MAX(y2, y3));
        pc_frame((minx + maxx) / 2, (miny + maxy) / 2, (maxx - minx) / 2 + 8, (maxy - miny) / 2 + 8);
    }
    pc_tri(x0, y0, x1, y1, x2, y2);
    pc_tri(x0, y0, x2, y2, x3, y3);
    if (!fon) pcv.fon = 0;
}

/* capsule raster state shared by the plain (IWRAM) and general loops */
typedef struct Cap {
    int x0, y0, dx, dy, r0, r1, rmax;
    int minx, maxx, miny, maxy, step, stepy, Trow, steep, slope;
} Cap;

/* plain shading (unmasked, unpatterned, no frame): the hot path */
P_FAST static void capsule_plain(const Sh *S, const Cap *c)
{
    int x, y, Trow = c->Trow, dr = c->r1 - c->r0;
    int flat = S->shade == SH_FLAT || S->shade == SH_VGRAD || S->shade == SH_HGRAD;
    const uint8_t *lut = S->lut;
    uint8_t part = S->part;
    int fz = S->fuzz ? 4 : 0;
    for (y = c->miny; y <= c->maxy; y++, Trow += c->stepy) {
        int py = y * PQ + 8 - c->y0, px = c->minx * PQ + 8 - c->x0;
        int T = Trow, xs = c->minx, in = 0, sh;
        uint8_t *p = S->px + y * S->w, *q = S->id + y * S->w;
        int fv = S->fon ? ((y * PQ + 8 - S->fcy) * S->fiv) >> 12 : 0, band = 2;
        if (S->shade == SH_VGRAD) band = fv < -110 ? 1 : (fv < 60 ? 2 : 3);
        else if (S->shade == SH_HGRAD) band = fv < -60 ? 3 : (fv < 110 ? 2 : 1);
        if (c->steep) {
            int ty = py < 0 ? (c->dy < 0 ? (py < c->dy ? c->dy : py) : 0)
                            : (c->dy > 0 ? (py > c->dy ? c->dy : py) : 0);
            int ax = c->x0 + ((ty * c->slope) >> 12) - c->rmax - (c->rmax >> 1) - PQ;
            if ((ax >> 4) > xs) {
                int sk = (ax >> 4) - xs;
                xs += sk;
                px += sk * PQ;
                T += sk * c->step;
            }
        }
        sh = ((xs ^ y) & 1) ? fz : 0;
        for (x = xs; x <= c->maxx; x++, px += PQ, T += c->step, sh ^= fz) {
            int t = T < 0 ? 0 : (T > 4096 ? 4096 : T);
            int qx = px - ((c->dx * t) >> 12), qy = py - ((c->dy * t) >> 12);
            int r = c->r0 + ((dr * t) >> 12);
            if (qx * qx + qy * qy <= r * r) {
                int b;
                if (flat) {
                    b = band;
                } else {
                    int u, v, iu, iv;
                    if (S->fon) {
                        u = ((x * PQ + 8 - S->fcx) * S->fiu) >> 12;
                        v = fv;
                    } else {
                        int ir = rcp[r];
                        u = (qx * ir) >> 8;
                        v = (qy * ir) >> 8;
                    }
                    iu = (u + 256) >> 3; iv = (v + 256) >> 3;
                    iu = iu < 0 ? 0 : (iu > 64 ? 64 : iu);
                    iv = iv < 0 ? 0 : (iv > 64 ? 64 : iv);
                    b = (lut[iv * 65 + iu] >> sh) & 15;
                }
                p[x] = S->col[b];
                if (!S->decal) q[x] = part;
                in = 1;
            } else if (in) {
                break;              /* a capsule row is one contiguous run */
            }
        }
    }
}

void pc_capsule(int x0, int y0, int x1, int y1, int r0, int r1)
{
    int len2, x, y;
    Cap c;
    Sh S;
    if (r0 < 1) r0 = 1;
    if (r1 < 1) r1 = 1;
    if (r0 > 511) r0 = 511;
    if (r1 > 511) r1 = 511;
    sh_load(&S);
    c.x0 = x0; c.y0 = y0; c.dx = x1 - x0; c.dy = y1 - y0; c.r0 = r0; c.r1 = r1;
    len2 = c.dx * c.dx + c.dy * c.dy;
    c.rmax = DOOMO_MAX(r0, r1);
    c.minx = (DOOMO_MIN(x0, x1) - c.rmax) >> 4;
    c.maxx = (DOOMO_MAX(x0, x1) + c.rmax) >> 4;
    c.miny = (DOOMO_MIN(y0, y1) - c.rmax) >> 4;
    c.maxy = (DOOMO_MAX(y0, y1) + c.rmax) >> 4;
    if (c.minx < S.x0) c.minx = S.x0;
    if (c.miny < S.y0) c.miny = S.y0;
    if (c.maxx >= S.x1) c.maxx = S.x1 - 1;
    if (c.maxy >= S.y1) c.maxy = S.y1 - 1;
    if (c.minx > c.maxx || c.miny > c.maxy) return;
    pcv.ops += (uint32_t)((c.maxy - c.miny + 1) * (c.maxx - c.minx + 1)) / 2;
    c.steep = p_abs(c.dy) > p_abs(c.dx) && p_abs(c.dy) >= PQ;
    c.slope = c.steep ? (c.dx << 12) / c.dy : 0;
    /* t (Q12 along the axis) is linear in x and y */
    if (len2 < 256) len2 = 0;            /* shorter than a pixel: a disc */
    c.step = len2 ? (c.dx << 16) / len2 : 0;
    c.stepy = len2 ? (c.dy << 16) / len2 : 0;
    c.Trow = len2 ? (((c.minx * PQ + 8 - x0) * c.dx + (c.miny * PQ + 8 - y0) * c.dy) << 8) / (len2 >> 4) : 0;
    if (!S.mask && !S.pat && (!S.fon || (S.shade != SH_VGRAD && S.shade != SH_HGRAD))) {
        capsule_plain(&S, &c);
        return;
    }
    for (y = c.miny; y <= c.maxy; y++, c.Trow += c.stepy) {
        int py = y * PQ + 8 - y0, px = c.minx * PQ + 8 - x0;
        int T = c.Trow, in = 0;
        for (x = c.minx; x <= c.maxx; x++, px += PQ, T += c.step) {
            int t = T < 0 ? 0 : (T > 4096 ? 4096 : T);
            int qx = px - ((c.dx * t) >> 12), qy = py - ((c.dy * t) >> 12);
            int r = r0 + (((r1 - r0) * t) >> 12);
            if (qx * qx + qy * qy <= r * r) {
                int ir = rcp[r];
                sh_px(&S, x, y, (qx * ir) >> 8, (qy * ir) >> 8);
                in = 1;
            } else if (in) {
                break;
            }
        }
    }
}

void pc_bezier(int x0, int y0, int x1, int y1, int x2, int y2, int r0, int r1, int segs)
{
    int i, px = x0, py = y0, pr = r0;
    if (segs < 1) segs = 1;
    for (i = 1; i <= segs; i++) {
        int t = (i * 256) / segs, it = 256 - t;
        int x = (it * it * x0 + 2 * it * t * x1 + t * t * x2) >> 16;
        int y = (it * it * y0 + 2 * it * t * y1 + t * t * y2) >> 16;
        int r = r0 + ((r1 - r0) * t >> 8);
        pc_capsule(px, py, x, y, pr, r);
        px = x; py = y; pr = r;
    }
}

void pc_rrect(int cx, int cy, int hw, int hh, int r)
{
    int y, ya, yb, iu, iv;
    Sh S;
    if (hw < 4) hw = 4;
    if (hh < 4) hh = 4;
    if (r > hw) r = hw;
    if (r > hh) r = hh;
    if (r < 1) r = 1;
    sh_load(&S);
    iu = inv20(hw);
    iv = inv20(hh);
    ya = (cy - hh) >> 4;
    yb = (cy + hh) >> 4;
    if (ya < S.y0) ya = S.y0;
    if (yb >= S.y1) yb = S.y1 - 1;
    for (y = ya; y <= yb; y++) {
        int dy = y * PQ + 8 - cy, ady = p_abs(dy), w, xa, xb;
        if (ady > hh) continue;
        if (ady > hh - r) {
            int k = ((ady - (hh - r)) << 8) / r;
            w = hw - r + ((circ[k > 256 ? 256 : k] * r) >> 8);
        } else {
            w = hw;
        }
        xa = (cx - w - 8 + 15) >> 4;
        xb = (cx + w - 8) >> 4;
        sh_span(&S, y, xa, xb, (xa * PQ + 8 - cx) * iu, PQ * iu, (dy * iv) >> 12);
    }
}

void pc_rect(int x0, int y0, int x1, int y1)
{
    int y, xa = (x0 + 8) >> 4, xb = (x1 - 8) >> 4, ya = (y0 + 8) >> 4, yb = (y1 - 8) >> 4;
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    int iu = inv20(DOOMO_MAX((x1 - x0) / 2, 8)), iv = inv20(DOOMO_MAX((y1 - y0) / 2, 8));
    Sh S;
    sh_load(&S);
    for (y = ya; y <= yb; y++)
        sh_span(&S, y, xa, xb, (xa * PQ + 8 - cx) * iu, PQ * iu, ((y * PQ + 8 - cy) * iv) >> 12);
}

/* ---- pixel helpers ------------------------------------------------------------------ */
void pc_px(int x, int y, uint8_t c)
{
    int i;
    if (x < pcv.x0 || y < pcv.y0 || x >= pcv.x1 || y >= pcv.y1 || !c) return;
    i = y * pcv.w + x;
    if (pcv.masked) {
        int id = pcv.id[i];
        if (!(pcv.mask[id >> 5] & (1u << (id & 31)))) return;
    }
    pcv.px[i] = c;
}

void pc_pxs(int x, int y, uint8_t c)
{
    if (x < 0 || y < 0 || x >= pcv.w || y >= pcv.h) return;
    if (pcv.px[y * pcv.w + x]) pc_px(x, y, c);
}

void pc_fx(int x, int y, uint8_t c)
{
    int i;
    if (x < 0 || y < 0 || x >= pcv.w || y >= pcv.h || !c) return;
    i = y * pcv.w + x;
    pcv.px[i] = c;
    if (!pcv.id[i]) pcv.id[i] = PID_FX;
}

int pc_get(int x, int y)
{
    if (x < 0 || y < 0 || x >= pcv.w || y >= pcv.h) return 0;
    return pcv.px[y * pcv.w + x];
}

int pc_idat(int x, int y)
{
    if (x < 0 || y < 0 || x >= pcv.w || y >= pcv.h) return 0;
    return pcv.id[y * pcv.w + x];
}

void pc_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = p_abs(x1 - x0), dy = -p_abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, n = 0;
    for (;;) {
        pc_px(x0, y0, c);
        if ((x0 == x1 && y0 == y1) || ++n > 200) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

void pc_lineq(int x0, int y0, int x1, int y1, uint8_t c)
{
    pc_line(x0 >> 4, y0 >> 4, x1 >> 4, y1 >> 4, c);
}

void pc_darken_part(int id)
{
    int i, n = pcv.w * pcv.h;
    uint8_t *p = pcv.px, *q = pcv.id;
    for (i = 0; i < n; i++)
        if (q[i] == id && p[i]) p[i] = doomo_art_darker[p[i]];
}

/* ---- finishing passes ------------------------------------------------------------ */

#define SOLID(id) ((uint8_t)((id) - 1) < (uint8_t)(PID_FX - 1))

/* one bit per pixel: candidates found by scanning id transitions, so the
 * full neighbour test only runs on the few boundary pixels */
static uint8_t pmark[PMAXW * PMAXH / 8];
#define MARK(i)   (pmark[(i) >> 3] |= (uint8_t)(1u << ((i) & 7)))

/* inner contour: where a part sits in front of an older one, the older
 * pixel gets one (or two) steps darker in its own colour family */
static inline __attribute__((always_inline)) void contour_px(int i, int x, int y, int w, int h)
{
    uint8_t *px = pcv.px, *idb = pcv.id;
    const uint8_t *pflag = pcv.pflag;
    int a = idb[i], b = 0, n;
    if (!SOLID(a) || (pflag[a] & PF_NOCONT)) return;
    if (y > 0) { n = idb[i - w]; if (SOLID(n) && n > b && !(pflag[n] & PF_NOCAST)) b = n; }
    if (y < h - 1) { n = idb[i + w]; if (SOLID(n) && n > b && !(pflag[n] & PF_NOCAST)) b = n; }
    if (x > 0) { n = idb[i - 1]; if (SOLID(n) && n > b && !(pflag[n] & PF_NOCAST)) b = n; }
    if (x < w - 1) { n = idb[i + 1]; if (SOLID(n) && n > b && !(pflag[n] & PF_NOCAST)) b = n; }
    if (b > a) {
        uint8_t c = doomo_art_darker[px[i]];
        if ((pflag[b] & PF_DARKLINE) || lumat[c] > 150) c = doomo_art_darker[c];
        px[i] = c;
    }
}


/* Mark the pixel that may need work at every id change, a word (4
 * pixels) at a time: the empty side of an empty/solid edge (outline) or
 * the older (lower id) side of a solid/solid edge (inner contour).
 * pmark must be cleared by the caller. */
P_FAST static void scan_transitions(void)
{
    int w = pcv.w, h = pcv.h, wq = w >> 2, y, k;
    const uint32_t *iw = pcv.idu.w;
    uint8_t *mk = pmark;
    for (y = 0; y < h; y++) {
        const uint32_t *row = iw + y * wq;
        for (k = 0; k < wq; k++) {
            uint32_t a = row[k];
            uint32_t nxt = k + 1 < wq ? row[k + 1] : a >> 24;
            uint32_t hd = a ^ ((a >> 8) | (nxt << 24));
            uint32_t vd = y + 1 < h ? a ^ row[k + wq] : 0;
            if (hd | vd) {
                int b, i = y * w + k * 4;
                for (b = 0; b < 4; b++) {
                    int p = (int)((a >> (b * 8)) & 0xFF), j, q;
                    if ((hd >> (b * 8)) & 0xFF) {
                        q = b < 3 ? (int)((a >> (b * 8 + 8)) & 0xFF) : (int)(nxt & 0xFF);
                        j = (p == 0 || (q != 0 && p < q)) ? i + b : i + b + 1;
                        mk[j >> 3] |= (uint8_t)(1u << (j & 7));
                    }
                    if ((vd >> (b * 8)) & 0xFF) {
                        q = (int)((row[k + wq] >> (b * 8)) & 0xFF);
                        j = (p == 0 || (q != 0 && p < q)) ? i + b : i + b + w;
                        mk[j >> 3] |= (uint8_t)(1u << (j & 7));
                    }
                }
            }
        }
    }
}

/* silhouette outline in the coloured line of the front-most neighbour */
static inline __attribute__((always_inline)) void outline_px(int i, int x, int y, int w, int h)
{
    uint8_t *px = pcv.px, *idb = pcv.id;
    const uint8_t *pflag = pcv.pflag, *pline = pcv.pline;
    int up, dn, lf, rt, best;
    uint8_t c;
    if (px[i]) return;
    up = y > 0 ? idb[i - w] : 0;
    dn = y < h - 1 ? idb[i + w] : 0;
    lf = x > 0 ? idb[i - 1] : 0;
    rt = x < w - 1 ? idb[i + 1] : 0;
    if (!SOLID(up) || (pflag[up] & PF_NOLINE)) up = 0;
    if (!SOLID(dn) || (pflag[dn] & PF_NOLINE)) dn = 0;
    if (!SOLID(lf) || (pflag[lf] & PF_NOLINE)) lf = 0;
    if (!SOLID(rt) || (pflag[rt] & PF_NOLINE)) rt = 0;
    best = DOOMO_MAX(DOOMO_MAX(up, dn), DOOMO_MAX(lf, rt));
    if (!best) return;
    c = pline[best];
    if (up && !dn) {
        /* underneath a shape: darkest (contact shadow) */
        uint8_t d = doomo_art_darker[c];
        if (d) c = d;
    } else if ((dn || rt) && !up && !lf) {
        /* lit top-left edge: one step lighter */
        uint8_t lt = doomo_art_lighter[c];
        if (lt && lumat[lt] < 150) c = lt;
    }
    px[i] = c;
    idb[i] = PID_LINE;
}

/* contours + silhouette outline: every marked pixel is either solid
 * (may get an inner contour) or empty (may get an outline) */
P_FAST static void finish_marked(void)
{
    int w = pcv.w, h = pcv.h, bpr = w >> 3, y, k;
    for (y = 0; y < h; y++) {
        const uint8_t *mrow = pmark + y * bpr;
        for (k = 0; k < bpr; k++) {
            int m = mrow[k], b;
            if (!m) continue;
            for (b = 0; b < 8; b++)
                if (m & (1 << b)) {
                    int x = k * 8 + b, i = y * w + x;
                    if (pcv.px[i]) contour_px(i, x, y, w, h);
                    else outline_px(i, x, y, w, h);
                }
        }
    }
}

void pc_finish_lines(void)
{
    int w = pcv.w, h = pcv.h;
    luma_init();
    pcv.ops += (uint32_t)(w * h / 4);
    memset(pmark, 0, (size_t)((w * h + 7) >> 3));
    scan_transitions();
    finish_marked();
}

void pc_contours(void)
{
    int w = pcv.w, h = pcv.h, bpr = w >> 3, y, k;
    luma_init();
    memset(pmark, 0, (size_t)((w * h + 7) >> 3));
    scan_transitions();
    for (y = 0; y < h; y++)
        for (k = 0; k < bpr; k++) {
            int m = pmark[y * bpr + k], b;
            for (b = 0; m && b < 8; b++)
                if (m & (1 << b)) contour_px(y * w + k * 8 + b, k * 8 + b, y, w, h);
        }
}

void pc_outline(void)
{
    int w = pcv.w, h = pcv.h, bpr = w >> 3, y, k;
    luma_init();
    memset(pmark, 0, (size_t)((w * h + 7) >> 3));
    scan_transitions();
    for (y = 0; y < h; y++)
        for (k = 0; k < bpr; k++) {
            int m = pmark[y * bpr + k], b;
            for (b = 0; m && b < 8; b++)
                if (m & (1 << b)) outline_px(y * w + k * 8 + b, k * 8 + b, y, w, h);
        }
}

void pc_flipx(void)
{
    int x, y, w = pcv.w;
    for (y = 0; y < pcv.h; y++) {
        uint8_t *r = pcv.px + y * w, *q = pcv.id + y * w;
        for (x = 0; x < w / 2; x++) {
            uint8_t t = r[x]; r[x] = r[w - 1 - x]; r[w - 1 - x] = t;
            t = q[x]; q[x] = q[w - 1 - x]; q[w - 1 - x] = t;
        }
    }
}

/* ---- pattern tiles -------------------------------------------------------------- */
#define _ 0
#define A 1
#define B 2
const uint8_t p_pattern_tiles[PATTERN_COUNT][64] = {
    /* SOLID */
    { 0 },
    /* STRIPES: two-pixel horizontal bands */
    { A,A,A,A,A,A,A,A,  A,A,A,A,A,A,A,A,  _,_,_,_,_,_,_,_,  _,_,_,_,_,_,_,_,
      A,A,A,A,A,A,A,A,  A,A,A,A,A,A,A,A,  _,_,_,_,_,_,_,_,  _,_,_,_,_,_,_,_ },
    /* POLKA: offset round dots */
    { _,A,A,_,_,_,_,_,  A,A,A,A,_,_,_,_,  A,A,A,A,_,_,_,_,  _,A,A,_,_,_,_,_,
      _,_,_,_,_,A,A,_,  _,_,_,_,A,A,A,A,  _,_,_,_,A,A,A,A,  _,_,_,_,_,A,A,_ },
    /* PLAID */
    { B,A,B,B,B,A,B,B,  A,_,A,A,A,_,A,A,  B,A,B,B,B,A,B,B,  B,A,B,B,B,A,B,B,
      B,A,B,B,B,A,B,B,  A,_,A,A,A,_,A,A,  _,A,_,_,_,A,_,_,  _,A,_,_,_,A,_,_ },
    /* FLAMES: little licks rising */
    { _,_,_,_,_,_,_,_,  _,A,_,_,_,_,A,_,  _,A,_,_,_,A,A,_,  A,A,_,_,_,A,B,A,
      A,B,A,_,A,B,B,A,  B,B,A,A,A,B,B,B,  B,B,B,A,B,B,B,B,  B,B,B,B,B,B,B,B },
    /* SKULLS */
    { _,_,_,_,_,_,_,_,  _,A,A,A,_,_,_,_,  A,B,A,B,A,_,_,_,  A,A,A,A,A,_,_,_,
      _,A,B,A,_,_,_,_,  _,A,A,A,_,_,_,_,  _,_,_,_,_,_,_,_,  _,_,_,_,_,_,_,_ },
    /* HEARTS */
    { _,_,_,_,_,_,_,_,  _,A,_,A,_,_,_,_,  A,A,A,A,A,_,_,_,  A,A,A,A,A,_,_,_,
      _,A,A,A,_,_,_,_,  _,_,A,_,_,_,_,_,  _,_,_,_,_,_,_,_,  _,_,_,_,_,_,_,_ },
    /* CAMO */
    { A,A,_,_,B,B,B,_,  A,A,A,_,_,B,B,_,  _,A,A,_,_,_,_,_,  _,_,_,B,B,_,A,A,
      B,_,_,B,B,B,A,A,  B,B,_,_,B,_,_,A,  _,B,A,A,_,_,_,_,  _,_,A,A,A,_,B,_ },
};
#undef _
#undef A
#undef B
