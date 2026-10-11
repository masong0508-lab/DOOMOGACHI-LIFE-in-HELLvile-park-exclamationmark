/* portrait_gfx.c - the tiny raster canvas behind every resident image
 * (owner: portrait). Integer-only shaded primitives, selective coloured
 * outlines and inner contours. See portrait_int.h for the model. */
#include "portrait_int.h"
#include "doomo/resident.h"

PCanvas pcv;

/* sqrt(1 - i/64) * 255 */
static const uint8_t ztab[65] = {
    255, 253, 251, 249, 247, 245, 243, 241, 239, 236, 234, 232, 230, 228,
    225, 223, 221, 219, 216, 214, 211, 209, 207, 204, 202, 199, 196, 194,
    191, 189, 186, 183, 180, 177, 175, 172, 169, 166, 163, 159, 156, 153,
    150, 146, 143, 139, 135, 131, 128, 123, 119, 115, 110, 106, 101, 96,
    90, 84, 78, 71, 64, 55, 45, 32, 0
};

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
uint8_t pm_line_of(uint8_t c)
{
    int k = 0;
    c = palette_darker(c);
    while (palette_luma(c) > 78 && k < 4) { c = palette_darker(c); k++; }
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
    pcv.fiu = (1 << 20) / rx;
    pcv.fiv = (1 << 20) / ry;
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

/* ---- shading ---------------------------------------------------------------- */
/* u, v: Q8 normal-ish coordinates (256 = edge of the shape). */
static int band_of(int u, int v, int x, int y)
{
    int d2, z, dot, h, b;
    switch (pcv.shade) {
    case SH_FLAT:
        return 2;
    case SH_VGRAD:
        b = v < -110 ? 1 : (v < 60 ? 2 : 3);
        return b;
    case SH_HGRAD:
        b = v < -60 ? 3 : (v < 110 ? 2 : 1);
        return b;
    default:
        break;
    }
    d2 = (u * u + v * v) >> 8;
    if (d2 > 256) d2 = 256;
    if (d2 < 0) d2 = 0;
    z = ztab[d2 >> 2];
    dot = (-115 * u - 141 * v + 179 * z) >> 8;
    h = (dot + 256) >> 1;
    if (pcv.shade == SH_SOFT) h += 18;
    if (h >= (pcv.shade == SH_GLOSS ? 232 : 247)) b = 4;
    else if (h >= 192) b = 3;
    else if (h >= 112) b = 2;
    else b = 1;
    /* rim light on the shadow side of the edge */
    if (b == 1 && d2 > 196 && u + v > 120) b = 2;
    /* soft fur dither on band boundaries */
    if ((pcv.flags & PF_FUZZ) && ((x ^ y) & 1)) {
        if (b == 2 && h >= 184) b = 3;
        else if (b == 1 && h >= 104) b = 2;
    }
    if (pcv.shade == SH_GLOSS && b == 3 && h < 205) b = 2;
    return b;
}

static void plot(int x, int y, int u, int v)
{
    int i, b;
    uint8_t c;
    if (x < pcv.x0 || y < pcv.y0 || x >= pcv.x1 || y >= pcv.y1) return;
    i = y * pcv.w + x;
    pcv.ops++;
    if (pcv.masked) {
        int id = pcv.id[i];
        if (!(pcv.mask[id >> 5] & (1u << (id & 31)))) return;
    }
    if (pcv.fon) {
        u = ((x * PQ + 8 - pcv.fcx) * pcv.fiu) >> 12;
        v = ((y * PQ + 8 - pcv.fcy) * pcv.fiv) >> 12;
    }
    b = band_of(u, v, x, y) + pcv.bias;
    b = DOOMO_CLAMP(b, 0, 4);
    if (pcv.pat) {
        int t = pcv.pat[((y & 7) << 3) | (x & 7)];
        if (t == 1) c = pcv.pm.c[b];
        else if (t == 2) c = pcv.pm.c[b > 0 ? b - 1 : 0];
        else c = pcv.m.c[b];
    } else {
        c = pcv.m.c[b];
    }
    pcv.px[i] = c;
    if (!pcv.decal) pcv.id[i] = pcv.part;
}

/* ---- primitives ----------------------------------------------------------------- */
void pc_ellipse(int cx, int cy, int rx, int ry)
{
    int y, ya, yb, iu, iv;
    if (rx < 2 || ry < 2) { if (rx >= 0 && ry >= 0) plot(cx >> 4, cy >> 4, 0, 0); return; }
    iu = (1 << 20) / rx;
    iv = (1 << 20) / ry;
    ya = (cy - ry) >> 4;
    yb = (cy + ry) >> 4;
    if (ya < pcv.y0) ya = pcv.y0;
    if (yb >= pcv.y1) yb = pcv.y1 - 1;
    for (y = ya; y <= yb; y++) {
        int dy = y * PQ + 8 - cy;
        int v = (dy * iv) >> 12, rem, hw, xa, xb, x;
        if (v <= -256 || v >= 256) continue;
        rem = 65536 - v * v;
        hw = (p_isqrt((uint32_t)rem) * rx) >> 8;
        xa = (cx - hw - 8 + 15) >> 4;
        xb = (cx + hw - 8) >> 4;
        if (xa < pcv.x0) xa = pcv.x0;
        if (xb >= pcv.x1) xb = pcv.x1 - 1;
        for (x = xa; x <= xb; x++)
            plot(x, y, ((x * PQ + 8 - cx) * iu) >> 12, v);
    }
}

void pc_ellipse_sh(int cx, int cy, int rx, int ry, int shear)
{
    int x, y, xa, xb, ya, yb, iu, iv, ext;
    if (rx < 2 || ry < 2) { plot(cx >> 4, cy >> 4, 0, 0); return; }
    iu = (1 << 20) / rx;
    iv = (1 << 20) / ry;
    ext = (p_abs(shear) * rx) >> 8;
    xa = (cx - rx) >> 4; xb = (cx + rx) >> 4;
    ya = (cy - ry - ext) >> 4; yb = (cy + ry + ext) >> 4;
    for (y = ya; y <= yb; y++)
        for (x = xa; x <= xb; x++) {
            int dx = x * PQ + 8 - cx, dy = y * PQ + 8 - cy - ((dx * shear) >> 8);
            int u = (dx * iu) >> 12, v = (dy * iv) >> 12;
            if (u * u + v * v <= 65536) plot(x, y, u, v);
        }
}

static int edge(int ax, int ay, int bx, int by, int px, int py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void pc_tri(int x0, int y0, int x1, int y1, int x2, int y2)
{
    int minx, maxx, miny, maxy, x, y, area, fcx, fcy, iu, iv, rx, ry;
    area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;
    if (area < 0) { int t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    minx = DOOMO_MIN(x0, DOOMO_MIN(x1, x2)) >> 4;
    maxx = DOOMO_MAX(x0, DOOMO_MAX(x1, x2)) >> 4;
    miny = DOOMO_MIN(y0, DOOMO_MIN(y1, y2)) >> 4;
    maxy = DOOMO_MAX(y0, DOOMO_MAX(y1, y2)) >> 4;
    if (minx < pcv.x0) minx = pcv.x0;
    if (miny < pcv.y0) miny = pcv.y0;
    if (maxx >= pcv.x1) maxx = pcv.x1 - 1;
    if (maxy >= pcv.y1) maxy = pcv.y1 - 1;
    /* own shading frame: centroid + half extents */
    fcx = (x0 + x1 + x2) / 3;
    fcy = (y0 + y1 + y2) / 3;
    rx = (DOOMO_MAX(x0, DOOMO_MAX(x1, x2)) - DOOMO_MIN(x0, DOOMO_MIN(x1, x2))) / 2 + 8;
    ry = (DOOMO_MAX(y0, DOOMO_MAX(y1, y2)) - DOOMO_MIN(y0, DOOMO_MIN(y1, y2))) / 2 + 8;
    iu = (1 << 20) / rx;
    iv = (1 << 20) / ry;
    for (y = miny; y <= maxy; y++) {
        int py = y * PQ + 8;
        for (x = minx; x <= maxx; x++) {
            int px = x * PQ + 8;
            if (edge(x0, y0, x1, y1, px, py) >= 0 && edge(x1, y1, x2, y2, px, py) >= 0 &&
                edge(x2, y2, x0, y0, px, py) >= 0)
                plot(x, y, ((px - fcx) * iu) >> 12, ((py - fcy) * iv) >> 12);
        }
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

void pc_capsule(int x0, int y0, int x1, int y1, int r0, int r1)
{
    int dx = x1 - x0, dy = y1 - y0;
    int len2 = dx * dx + dy * dy;
    int inv, minx, maxx, miny, maxy, x, y, rmax;
    if (r0 < 1) r0 = 1;
    if (r1 < 1) r1 = 1;
    rmax = DOOMO_MAX(r0, r1);
    minx = (DOOMO_MIN(x0, x1) - rmax) >> 4;
    maxx = (DOOMO_MAX(x0, x1) + rmax) >> 4;
    miny = (DOOMO_MIN(y0, y1) - rmax) >> 4;
    maxy = (DOOMO_MAX(y0, y1) + rmax) >> 4;
    if (minx < pcv.x0) minx = pcv.x0;
    if (miny < pcv.y0) miny = pcv.y0;
    if (maxx >= pcv.x1) maxx = pcv.x1 - 1;
    if (maxy >= pcv.y1) maxy = pcv.y1 - 1;
    /* t in Q12 = dot(p - a, d) * inv >> 12 */
    inv = len2 > 0 ? (int)((1u << 24) / (uint32_t)((len2 + 15) >> 4)) : 0;
    for (y = miny; y <= maxy; y++) {
        int py = y * PQ + 8 - y0;
        for (x = minx; x <= maxx; x++) {
            int px = x * PQ + 8 - x0;
            int t = 0, qx, qy, r, d2;
            if (len2 > 0) {
                t = (int)(((int64_t)(px * dx + py * dy) * inv) >> 16);
                t = DOOMO_CLAMP(t, 0, 4096);
            }
            qx = px - ((dx * t) >> 12);
            qy = py - ((dy * t) >> 12);
            r = r0 + (((r1 - r0) * t) >> 12);
            d2 = qx * qx + qy * qy;
            if (d2 <= r * r) {
                int iu = (1 << 16) / (r > 0 ? r : 1);
                plot(x, y, (qx * iu) >> 8, (qy * iu) >> 8);
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
    if (hw < 4) hw = 4;
    if (hh < 4) hh = 4;
    if (r > hw) r = hw;
    if (r > hh) r = hh;
    if (r < 1) r = 1;
    iu = (1 << 20) / hw;
    iv = (1 << 20) / hh;
    ya = (cy - hh) >> 4;
    yb = (cy + hh) >> 4;
    if (ya < pcv.y0) ya = pcv.y0;
    if (yb >= pcv.y1) yb = pcv.y1 - 1;
    for (y = ya; y <= yb; y++) {
        int dy = y * PQ + 8 - cy, ady = p_abs(dy), w, xa, xb, x;
        if (ady > hh) continue;
        if (ady > hh - r) {
            int k = ady - (hh - r);
            w = hw - r + p_isqrt((uint32_t)(r * r - k * k));
        } else {
            w = hw;
        }
        xa = (cx - w - 8 + 15) >> 4;
        xb = (cx + w - 8) >> 4;
        if (xa < pcv.x0) xa = pcv.x0;
        if (xb >= pcv.x1) xb = pcv.x1 - 1;
        for (x = xa; x <= xb; x++)
            plot(x, y, ((x * PQ + 8 - cx) * iu) >> 12, (dy * iv) >> 12);
    }
}

void pc_rect(int x0, int y0, int x1, int y1)
{
    int x, y, xa = (x0 + 8) >> 4, xb = (x1 - 8) >> 4, ya = (y0 + 8) >> 4, yb = (y1 - 8) >> 4;
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    int iu = (1 << 20) / DOOMO_MAX((x1 - x0) / 2, 8), iv = (1 << 20) / DOOMO_MAX((y1 - y0) / 2, 8);
    for (y = ya; y <= yb; y++)
        for (x = xa; x <= xb; x++)
            plot(x, y, ((x * PQ + 8 - cx) * iu) >> 12, ((y * PQ + 8 - cy) * iv) >> 12);
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
    for (i = 0; i < n; i++)
        if (pcv.id[i] == id && pcv.px[i]) pcv.px[i] = palette_darker(pcv.px[i]);
}

/* ---- finishing passes ------------------------------------------------------------ */
static int solid(int id) { return id > 0 && id < PID_FX; }

void pc_contours(void)
{
    int x, y, w = pcv.w, h = pcv.h;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int i = y * w + x, a = pcv.id[i], b = 0, k;
            int nb[4];
            if (!solid(a) || (pcv.pflag[a] & PF_NOCONT)) continue;
            nb[0] = y > 0 ? pcv.id[i - w] : 0;
            nb[1] = y < h - 1 ? pcv.id[i + w] : 0;
            nb[2] = x > 0 ? pcv.id[i - 1] : 0;
            nb[3] = x < w - 1 ? pcv.id[i + 1] : 0;
            for (k = 0; k < 4; k++)
                if (solid(nb[k]) && nb[k] > a && !(pcv.pflag[nb[k]] & PF_NOCAST) &&
                    nb[k] > b) b = nb[k];
            if (b) {
                uint8_t c = palette_darker(pcv.px[i]);
                if ((pcv.pflag[b] & PF_DARKLINE) || palette_luma(c) > 150)
                    c = palette_darker(c);
                pcv.px[i] = c;
            }
        }
    }
}

void pc_outline(void)
{
    int x, y, w = pcv.w, h = pcv.h;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int i = y * w + x, up, dn, lf, rt, best = 0;
            uint8_t c;
            if (pcv.px[i]) continue;
            up = y > 0 ? pcv.id[i - w] : 0;
            dn = y < h - 1 ? pcv.id[i + w] : 0;
            lf = x > 0 ? pcv.id[i - 1] : 0;
            rt = x < w - 1 ? pcv.id[i + 1] : 0;
            if (!solid(up) || (pcv.pflag[up] & PF_NOLINE)) up = 0;
            if (!solid(dn) || (pcv.pflag[dn] & PF_NOLINE)) dn = 0;
            if (!solid(lf) || (pcv.pflag[lf] & PF_NOLINE)) lf = 0;
            if (!solid(rt) || (pcv.pflag[rt] & PF_NOLINE)) rt = 0;
            best = DOOMO_MAX(DOOMO_MAX(up, dn), DOOMO_MAX(lf, rt));
            if (!best) continue;
            c = pcv.pline[best];
            if (up && !dn) {
                /* underneath a shape: darkest (contact shadow) */
                uint8_t d = palette_darker(c);
                if (d) c = d;
            } else if ((dn || rt) && !up && !lf) {
                /* lit top-left edge: one step lighter */
                uint8_t lt = palette_lighter(c);
                if (lt && palette_luma(lt) < 150) c = lt;
            }
            pcv.px[i] = c;
            pcv.id[i] = PID_LINE;
        }
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
