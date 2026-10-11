/* art_spr.c - the sprite canvas (artgen only).
 *
 * Sprites are built from shaded primitives (balls, capsules, polygons,
 * bezier horns) on a scratch canvas, then finished with two passes that
 * make them read as premium pixel art instead of flat blobs:
 *   - spr_contours(): where a part overlaps a part behind it, the back part
 *     gets a darker line in its OWN colour family (coloured inner lines that
 *     also read as soft ambient occlusion);
 *   - spr_outline(): a 1px coloured selective outline around the
 *     silhouette (dark shade of the local colour, lighter on the lit
 *     top-left, darkest under the feet).
 * Every primitive is lit with half-Lambert soft shading from the top-left,
 * plus a cool rim light on the shadow edge, so 4-5 shade materials give
 * the 3-4 tone hue-shifted look of hand-shaded sprites.
 */
#include "art.h"

#define MAXPART 8192
#define XF_DEPTH 24

static uint8_t  cv[SC_H][SC_W];
static uint16_t pid[SC_H][SC_W];
static uint16_t pflags[MAXPART];
static int npart;

typedef struct {
    int m[4];       /* forward linear part, Q12: [m0 m1; m2 m3]          */
    int im[4];      /* inverse, Q12                                       */
    int tx, ty;     /* local origin on the canvas, 1/16 px rel. anchor    */
} Xf;
static Xf xs[XF_DEPTH];
static int xsp;
static int Lx, Ly, Lz;          /* light direction, Q12, normalised      */
static int amb = 70;
static int rim_amt = 60;

#define XF (xs[xsp])

/* ---- setup / transform ---------------------------------------------------- */
static void xf_identity(Xf *x)
{
    x->m[0] = Q12; x->m[1] = 0; x->m[2] = 0; x->m[3] = Q12;
    x->im[0] = Q12; x->im[1] = 0; x->im[2] = 0; x->im[3] = Q12;
    x->tx = 0; x->ty = 0;
}

void spr_light(int lx, int ly, int lz)
{
    int len = art_dist(lx, art_dist(ly, lz));
    if (len <= 0) len = 1;
    Lx = (int)((int64_t)lx * Q12 / len);
    Ly = (int)((int64_t)ly * Q12 / len);
    Lz = (int)((int64_t)lz * Q12 / len);
}

void spr_ambient(int a) { amb = art_clamp(a, 0, 255); }
void spr_rim(int a) { rim_amt = art_clamp(a, 0, 255); }

void spr_begin(void)
{
    memset(cv, 0, sizeof cv);
    memset(pid, 0, sizeof pid);
    memset(pflags, 0, sizeof pflags);
    npart = 0;
    xsp = 0;
    xf_identity(&xs[0]);
    spr_light(-1900, -2500, 2700);
    amb = 70;
    rim_amt = 60;
}

void spr_push(void)
{
    if (xsp < XF_DEPTH - 1) { xs[xsp + 1] = xs[xsp]; xsp++; }
}

void spr_pop(void)
{
    if (xsp > 0) xsp--;
}

void spr_translate_q(int dx16, int dy16)
{
    XF.tx += (XF.m[0] * dx16 + XF.m[1] * dy16) >> 12;
    XF.ty += (XF.m[2] * dx16 + XF.m[3] * dy16) >> 12;
}

void spr_translate(int dx, int dy) { spr_translate_q(dx * 16, dy * 16); }

static void mat_mul(int *out, const int *a, const int *b)
{
    int r[4];
    r[0] = (a[0] * b[0] + a[1] * b[2]) >> 12;
    r[1] = (a[0] * b[1] + a[1] * b[3]) >> 12;
    r[2] = (a[2] * b[0] + a[3] * b[2]) >> 12;
    r[3] = (a[2] * b[1] + a[3] * b[3]) >> 12;
    memcpy(out, r, sizeof r);
}

void spr_rotate(int ang)
{
    int c = art_cos(ang), s = art_sin(ang);
    int r[4], ir[4];
    r[0] = c; r[1] = -s; r[2] = s; r[3] = c;
    ir[0] = c; ir[1] = s; ir[2] = -s; ir[3] = c;
    mat_mul(XF.m, XF.m, r);
    mat_mul(XF.im, ir, XF.im);
}

void spr_scale(int sx, int sy)
{
    int s[4], is[4];
    if (sx == 0) sx = 1;
    if (sy == 0) sy = 1;
    s[0] = sx; s[1] = 0; s[2] = 0; s[3] = sy;
    is[0] = (int)((int64_t)Q12 * Q12 / sx); is[1] = 0; is[2] = 0;
    is[3] = (int)((int64_t)Q12 * Q12 / sy);
    mat_mul(XF.m, XF.m, s);
    mat_mul(XF.im, is, XF.im);
}

/* canvas pixel -> local coordinates (1/16 px) of its centre */
static void to_local(int px, int py, int *lx, int *ly)
{
    int X = (px - SC_AX) * 16 + 8 - XF.tx, Y = (py - SC_AY) * 16 + 8 - XF.ty;
    *lx = (int)(((int64_t)XF.im[0] * X + (int64_t)XF.im[1] * Y) >> 12);
    *ly = (int)(((int64_t)XF.im[2] * X + (int64_t)XF.im[3] * Y) >> 12);
}

/* local (1/16 px) -> canvas pixel */
static void to_canvas(int lx, int ly, int *px, int *py)
{
    int X = ((XF.m[0] * lx + XF.m[1] * ly) >> 12) + XF.tx;
    int Y = ((XF.m[2] * lx + XF.m[3] * ly) >> 12) + XF.ty;
    *px = SC_AX + (X >= 0 ? X / 16 : -((-X + 15) / 16));
    *py = SC_AY + (Y >= 0 ? Y / 16 : -((-Y + 15) / 16));
}

/* canvas-pixel bounding box of a local box (1/16 px) */
static void bbox(int x0, int y0, int x1, int y1, int *bx0, int *by0, int *bx1, int *by1)
{
    int xs4[4], ys4[4], k;
    to_canvas(x0, y0, &xs4[0], &ys4[0]);
    to_canvas(x1, y0, &xs4[1], &ys4[1]);
    to_canvas(x0, y1, &xs4[2], &ys4[2]);
    to_canvas(x1, y1, &xs4[3], &ys4[3]);
    *bx0 = *bx1 = xs4[0];
    *by0 = *by1 = ys4[0];
    for (k = 1; k < 4; k++) {
        if (xs4[k] < *bx0) *bx0 = xs4[k];
        if (xs4[k] > *bx1) *bx1 = xs4[k];
        if (ys4[k] < *by0) *by0 = ys4[k];
        if (ys4[k] > *by1) *by1 = ys4[k];
    }
    *bx0 = art_clamp(*bx0 - 2, 0, SC_W - 1);
    *by0 = art_clamp(*by0 - 2, 0, SC_H - 1);
    *bx1 = art_clamp(*bx1 + 2, 0, SC_W - 1);
    *by1 = art_clamp(*by1 + 2, 0, SC_H - 1);
}

/* ---- shading ------------------------------------------------------------- */
static int cur_part, cur_fl;

static void begin_part(int fl)
{
    if ((fl & SH_SAMEPART) && npart > 0) { cur_part = npart; }
    else {
        if (npart < MAXPART - 1) npart++;
        cur_part = npart;
        pflags[cur_part] = (uint16_t)fl;
    }
    cur_fl = fl;
}

/* local normal (Q12 xy + z) -> canvas-space normal, keeping its length */
static void normal_to_canvas(int *nx, int *ny)
{
    int ax = (XF.m[0] * *nx + XF.m[1] * *ny) >> 12;
    int ay = (XF.m[2] * *nx + XF.m[3] * *ny) >> 12;
    int l0 = art_dist(*nx, *ny), l1 = art_dist(ax, ay);
    if (l1 > 0) { ax = ax * l0 / l1; ay = ay * l0 / l1; }
    *nx = ax; *ny = ay;
}

/* light value 0..255 from a canvas-space normal (Q12) */
static int light_v(int nx, int ny, int nz)
{
    int dot = (nx * Lx + ny * Ly + nz * Lz) >> 12;      /* -Q12..Q12  */
    int h = (dot + Q12) / 2;                             /* half-Lambert */
    int v = (h * h) >> 12;                               /* squared   */
    int out = amb / 2 + (int)((int64_t)v * (255 - amb / 2) / Q12);
    if (rim_amt && nz < Q12 * 55 / 100) {
        int back = -(nx * Lx + ny * Ly) >> 12;
        if (back > 0) {
            int e = Q12 * 55 / 100 - nz;                 /* 0..0.55    */
            out += (int)((int64_t)rim_amt * e / (Q12 * 55 / 100) * back / Q12);
        }
    }
    return art_clamp(out, 0, 255);
}

static int spec_of(int nx, int ny, int nz)
{
    /* half vector between light and viewer (0,0,1) */
    int hx = Lx, hy = Ly, hz = Lz + Q12;
    int l = art_dist(hx, art_dist(hy, hz));
    if (l <= 0) return 0;
    return (int)(((int64_t)nx * hx + (int64_t)ny * hy + (int64_t)nz * hz) / l);
}

static int spec_best, spec_x, spec_y;
static const ArtMat *spec_mat;

static void put(int px, int py, int v, const ArtMat *m, int fl)
{
    uint8_t c;
    if ((fl & SH_BEHIND) && cv[py][px]) return;
    if ((fl & SH_ONTOP) && !cv[py][px]) return;
    if ((fl & SH_MASK) == SH_FLAT) c = m->c[m->n / 2];
    else c = art_mat_pick(m, v, px, py, fl & SH_DITHER);
    if (!c) return;
    cv[py][px] = c;
    pid[py][px] = (uint16_t)cur_part;
}

static void put_n(int px, int py, int nx, int ny, int nz, const ArtMat *m, int fl)
{
    int v;
    normal_to_canvas(&nx, &ny);
    v = light_v(nx, ny, nz);
    if ((fl & SH_BEHIND) && cv[py][px]) return;
    if ((fl & SH_ONTOP) && !cv[py][px]) return;
    put(px, py, v, m, fl);
    if (fl & SH_SPEC) {
        int s = spec_of(nx, ny, nz);
        if (s > spec_best) { spec_best = s; spec_x = px; spec_y = py; }
    }
}

static void spec_begin(const ArtMat *m) { spec_best = -Q12 * 2; spec_mat = m; }

static void spec_end(int fl, int size)
{
    if (!(fl & SH_SPEC) || spec_best < 0) return;
    (void)spec_mat;
    cv[spec_y][spec_x] = PAL_WHITE;
    if (size > 5 && spec_x > 0) cv[spec_y][spec_x - 1] = PAL_WHITE;
    if (size > 9 && spec_y > 0) {
        cv[spec_y - 1][spec_x] = PAL_WHITE;
        if (spec_x > 0) cv[spec_y - 1][spec_x - 1] = PAL_WHITE;
    }
}

static int isqrt_q24(int64_t v)
{
    if (v <= 0) return 0;
    return (int)art_isqrt((uint32_t)(v > 0xFFFFFFFFLL ? 0xFFFFFFFFLL : v));
}

/* ---- primitives ------------------------------------------------------------ */
void spr_ellipse(int cx, int cy, int rx, int ry, const ArtMat *m, int fl)
{
    int bx0, by0, bx1, by1, px, py;
    int cx16 = cx * 16, cy16 = cy * 16, rx16 = rx * 16, ry16 = ry * 16;
    int shade = fl & SH_MASK;
    if (rx16 < 8) rx16 = 8;
    if (ry16 < 8) ry16 = 8;
    begin_part(fl);
    spec_begin(m);
    bbox(cx16 - rx16, cy16 - ry16, cx16 + rx16, cy16 + ry16, &bx0, &by0, &bx1, &by1);
    for (py = by0; py <= by1; py++)
        for (px = bx0; px <= bx1; px++) {
            int lx, ly, nx, ny;
            int64_t r2;
            to_local(px, py, &lx, &ly);
            nx = (int)((int64_t)(lx - cx16) * Q12 / rx16);
            ny = (int)((int64_t)(ly - cy16) * Q12 / ry16);
            r2 = (int64_t)nx * nx + (int64_t)ny * ny;
            if (r2 > (int64_t)Q12 * Q12) continue;
            if (shade == SH_VGRAD) {
                put(px, py, 255 - (ny + Q12) * 255 / (2 * Q12), m, fl);
            } else if (shade == SH_CYL) {
                int nz = isqrt_q24((int64_t)Q12 * Q12 - (int64_t)nx * nx);
                put_n(px, py, nx, 0, nz, m, fl);
            } else {
                int nz = isqrt_q24((int64_t)Q12 * Q12 - r2);
                put_n(px, py, nx, ny, nz, m, fl);
            }
        }
    spec_end(fl, rx < ry ? rx : ry);
}

void spr_ellipse_r(int cx, int cy, int rx, int ry, int ang, const ArtMat *m, int fl)
{
    spr_push();
    spr_translate(cx, cy);
    spr_rotate(ang);
    spr_ellipse(0, 0, rx, ry, m, fl);
    spr_pop();
}

/* sub-pixel version used by the helpers: centre/radii in 1/16 px */
void spr_ellipse_q(int cx16, int cy16, int rx16, int ry16, const ArtMat *m, int fl)
{
    int bx0, by0, bx1, by1, px, py;
    int shade = fl & SH_MASK;
    if (rx16 < 6) rx16 = 6;
    if (ry16 < 6) ry16 = 6;
    begin_part(fl);
    spec_begin(m);
    bbox(cx16 - rx16, cy16 - ry16, cx16 + rx16, cy16 + ry16, &bx0, &by0, &bx1, &by1);
    for (py = by0; py <= by1; py++)
        for (px = bx0; px <= bx1; px++) {
            int lx, ly, nx, ny;
            int64_t r2;
            to_local(px, py, &lx, &ly);
            nx = (int)((int64_t)(lx - cx16) * Q12 / rx16);
            ny = (int)((int64_t)(ly - cy16) * Q12 / ry16);
            r2 = (int64_t)nx * nx + (int64_t)ny * ny;
            if (r2 > (int64_t)Q12 * Q12) continue;
            if (shade == SH_VGRAD) put(px, py, 255 - (ny + Q12) * 255 / (2 * Q12), m, fl);
            else {
                int nz = isqrt_q24((int64_t)Q12 * Q12 - r2);
                put_n(px, py, nx, ny, nz, m, fl);
            }
        }
    spec_end(fl, (rx16 < ry16 ? rx16 : ry16) / 16);
}

void spr_fluffy(int cx, int cy, int rx, int ry, int tufts, int depth, int phase,
                int a0, int a1, const ArtMat *m, int fl)
{
    int bx0, by0, bx1, by1, px, py;
    int cx16 = cx * 16, cy16 = cy * 16, rx16 = rx * 16, ry16 = ry * 16;
    int span = art_wrap(a1 - a0, ART_TURN);
    if (span == 0) span = ART_TURN;
    if (rx16 < 8) rx16 = 8;
    if (ry16 < 8) ry16 = 8;
    if (tufts < 1) tufts = 1;
    begin_part(fl);
    spec_begin(m);
    bbox(cx16 - rx16, cy16 - ry16, cx16 + rx16, cy16 + ry16, &bx0, &by0, &bx1, &by1);
    for (py = by0; py <= by1; py++)
        for (px = bx0; px <= bx1; px++) {
            int lx, ly, nx, ny, nz, r, lim = Q12;
            int64_t r2;
            to_local(px, py, &lx, &ly);
            nx = (int)((int64_t)(lx - cx16) * Q12 / rx16);
            ny = (int)((int64_t)(ly - cy16) * Q12 / ry16);
            r2 = (int64_t)nx * nx + (int64_t)ny * ny;
            if (r2 > (int64_t)Q12 * Q12) continue;
            r = isqrt_q24(r2);
            {
                int a = art_atan2(ny, nx);
                int rel = art_wrap(a - a0, ART_TURN);
                if (rel <= span) {
                    /* pointed tuft profile: 0 at the tip, 1 in the valley */
                    int f = art_wrap(rel * tufts * Q12 / span + phase, Q12);
                    int tri = art_abs(2 * f - Q12);           /* 0..Q12 */
                    int prof = (tri * tri) >> 12;               /* sharper */
                    /* fade the tufts in/out at the arc ends */
                    int edge = rel < span - rel ? rel : span - rel;
                    int fade = art_clamp(edge * Q12 / 96, 0, Q12);
                    lim = Q12 - (int)((int64_t)depth * prof / Q12 * fade / Q12);
                } else {
                    lim = Q12 - depth / 2;
                }
            }
            if (r > lim) continue;
            if (r > 0 && lim < Q12) {
                /* shade as if the ball were the tuft envelope */
                nx = (int)((int64_t)nx * Q12 / lim);
                ny = (int)((int64_t)ny * Q12 / lim);
                r2 = (int64_t)nx * nx + (int64_t)ny * ny;
                if (r2 > (int64_t)Q12 * Q12) r2 = (int64_t)Q12 * Q12;
            }
            nz = isqrt_q24((int64_t)Q12 * Q12 - r2);
            put_n(px, py, nx, ny, nz, m, fl);
        }
    spec_end(fl, rx < ry ? rx : ry);
}

static void capsule_q(int x0, int y0, int r0, int x1, int y1, int r1,
                      const ArtMat *m, int fl, int newpart)
{
    int bx0, by0, bx1, by1, px, py;
    int64_t dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    int rmax = r0 > r1 ? r0 : r1;
    int minx = (x0 < x1 ? x0 : x1) - rmax, maxx = (x0 > x1 ? x0 : x1) + rmax;
    int miny = (y0 < y1 ? y0 : y1) - rmax, maxy = (y0 > y1 ? y0 : y1) + rmax;
    int shade = fl & SH_MASK;
    if (newpart) begin_part(fl);
    bbox(minx, miny, maxx, maxy, &bx0, &by0, &bx1, &by1);
    for (py = by0; py <= by1; py++)
        for (px = bx0; px <= bx1; px++) {
            int lx, ly, r, d, nx, ny, nz;
            int64_t t;
            int qx, qy;
            to_local(px, py, &lx, &ly);
            if (len2 > 0) {
                t = ((int64_t)(lx - x0) * dx + (int64_t)(ly - y0) * dy) * Q12 / len2;
                if (t < 0) t = 0;
                if (t > Q12) t = Q12;
            } else t = 0;
            qx = x0 + (int)(dx * t / Q12);
            qy = y0 + (int)(dy * t / Q12);
            r = r0 + (int)((int64_t)(r1 - r0) * t / Q12);
            if (r < 6) r = 6;
            d = art_dist(lx - qx, ly - qy);
            if (d > r) continue;
            if (shade == SH_VGRAD) {
                put(px, py, 255 - art_clamp((ly - miny) * 255 / (maxy - miny + 1), 0, 255), m, fl);
                continue;
            }
            nx = (lx - qx) * Q12 / r;
            ny = (ly - qy) * Q12 / r;
            nz = isqrt_q24((int64_t)Q12 * Q12 - (int64_t)nx * nx - (int64_t)ny * ny);
            put_n(px, py, nx, ny, nz, m, fl);
        }
}

void spr_capsule(int x0, int y0, int r0, int x1, int y1, int r1, const ArtMat *m, int fl)
{
    spec_begin(m);
    capsule_q(x0 * 16, y0 * 16, r0 * 16, x1 * 16, y1 * 16, r1 * 16, m, fl, 1);
    spec_end(fl, r0 > r1 ? r0 : r1);
}

void spr_capsule_q(int x0, int y0, int r0, int x1, int y1, int r1, const ArtMat *m, int fl)
{
    spec_begin(m);
    capsule_q(x0, y0, r0, x1, y1, r1, m, fl, 1);
    spec_end(fl, (r0 > r1 ? r0 : r1) / 16);
}

void spr_bezier(int x0, int y0, int x1, int y1, int x2, int y2, int r0, int r2,
                const ArtMat *m, int fl)
{
    int i, n, px = x0 * 16, py = y0 * 16, pr = r0 * 16;
    int span = art_abs(x2 - x0) + art_abs(y2 - y0) + art_abs(x1 - x0) + art_abs(y1 - y0);
    n = art_clamp(span / 2, 4, 24);
    begin_part(fl);
    spec_begin(m);
    for (i = 1; i <= n; i++) {
        int t = i * Q12 / n, u = Q12 - t;
        int64_t a = (int64_t)u * u, b = 2LL * u * t, c = (int64_t)t * t;
        int qx = (int)((a * x0 + b * x1 + c * x2) * 16 / ((int64_t)Q12 * Q12));
        int qy = (int)((a * y0 + b * y1 + c * y2) * 16 / ((int64_t)Q12 * Q12));
        int qr = r0 * 16 + (r2 - r0) * 16 * i / n;
        capsule_q(px, py, pr, qx, qy, qr, m, fl, 0);
        px = qx; py = qy; pr = qr;
    }
    spec_end(fl, r0 > r2 ? r0 : r2);
}

/* polygon fill in local coordinates (1/16 px) with optional lighting */
static void poly_q(const int *xy16, int n, const ArtMat *m, int fl)
{
    int minx = 1 << 30, maxx = -(1 << 30), miny = 1 << 30, maxy = -(1 << 30);
    int i, bx0, by0, bx1, by1, px, py, cx, cy, rx, ry;
    int shade = fl & SH_MASK;
    for (i = 0; i < n; i++) {
        if (xy16[2 * i] < minx) minx = xy16[2 * i];
        if (xy16[2 * i] > maxx) maxx = xy16[2 * i];
        if (xy16[2 * i + 1] < miny) miny = xy16[2 * i + 1];
        if (xy16[2 * i + 1] > maxy) maxy = xy16[2 * i + 1];
    }
    cx = (minx + maxx) / 2; cy = (miny + maxy) / 2;
    rx = (maxx - minx) / 2 + 1; ry = (maxy - miny) / 2 + 1;
    begin_part(fl);
    spec_begin(m);
    bbox(minx, miny, maxx, maxy, &bx0, &by0, &bx1, &by1);
    for (py = by0; py <= by1; py++)
        for (px = bx0; px <= bx1; px++) {
            int lx, ly, inside = 0, j;
            to_local(px, py, &lx, &ly);
            for (i = 0, j = n - 1; i < n; j = i++) {
                int xi = xy16[2 * i], yi = xy16[2 * i + 1];
                int xj = xy16[2 * j], yj = xy16[2 * j + 1];
                if ((yi > ly) != (yj > ly)) {
                    int64_t xc = xi + (int64_t)(ly - yi) * (xj - xi) / (yj - yi);
                    if (lx < xc) inside = !inside;
                }
            }
            if (!inside) continue;
            if (shade == SH_VGRAD) {
                put(px, py, 255 - art_clamp((ly - miny) * 255 / (maxy - miny + 1), 0, 255), m, fl);
            } else {
                int nx = (int)((int64_t)(lx - cx) * Q12 / rx), ny = (int)((int64_t)(ly - cy) * Q12 / ry);
                int64_t r2 = (int64_t)nx * nx + (int64_t)ny * ny;
                int nz;
                if (r2 > (int64_t)Q12 * Q12) {
                    int l = art_dist(nx, ny);
                    nx = (int)((int64_t)nx * Q12 / l); ny = (int)((int64_t)ny * Q12 / l);
                    r2 = (int64_t)Q12 * Q12;
                }
                if (shade == SH_CYL) { ny = 0; r2 = (int64_t)nx * nx; }
                nz = isqrt_q24((int64_t)Q12 * Q12 - r2);
                put_n(px, py, nx, ny, nz, m, fl);
            }
        }
    spec_end(fl, (rx < ry ? rx : ry) / 16);
}

void spr_poly(const int *xy, int n, const ArtMat *m, int fl)
{
    int buf[64], i;
    if (n > 32) n = 32;
    for (i = 0; i < 2 * n; i++) buf[i] = xy[i] * 16;
    poly_q(buf, n, m, fl);
}

void spr_poly_q(const int *xy16, int n, const ArtMat *m, int fl) { poly_q(xy16, n, m, fl); }

void spr_tri(int x0, int y0, int x1, int y1, int x2, int y2, const ArtMat *m, int fl)
{
    int p[6];
    p[0] = x0; p[1] = y0; p[2] = x1; p[3] = y1; p[4] = x2; p[5] = y2;
    spr_poly(p, 3, m, fl);
}

void spr_rect(int x, int y, int w, int h, const ArtMat *m, int fl)
{
    int p[8];
    p[0] = x; p[1] = y; p[2] = x + w; p[3] = y; p[4] = x + w; p[5] = y + h; p[6] = x; p[7] = y + h;
    spr_poly(p, 4, m, fl);
}

/* ---- pixels & lines ------------------------------------------------------- */
void spr_px_raw(int cx, int cy, uint8_t c)
{
    if (cx < 0 || cy < 0 || cx >= SC_W || cy >= SC_H) return;
    cv[cy][cx] = c;
    if (!pid[cy][cx]) pid[cy][cx] = (uint16_t)(npart ? npart : 0);
}

void spr_px(int x, int y, uint8_t c)
{
    int px, py;
    to_canvas(x * 16 + 8, y * 16 + 8, &px, &py);
    spr_px_raw(px, py, c);
}

void spr_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int ax, ay, bx, by, dx, dy, sx, sy, err;
    to_canvas(x0 * 16 + 8, y0 * 16 + 8, &ax, &ay);
    to_canvas(x1 * 16 + 8, y1 * 16 + 8, &bx, &by);
    dx = art_abs(bx - ax); sx = ax < bx ? 1 : -1;
    dy = -art_abs(by - ay); sy = ay < by ? 1 : -1;
    err = dx + dy;
    for (;;) {
        int e2;
        spr_px_raw(ax, ay, c);
        if (ax == bx && ay == by) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; ax += sx; }
        if (e2 <= dx) { err += dx; ay += sy; }
    }
}

static ArtImg canvas_img(void)
{
    ArtImg im;
    im.w = SC_W; im.h = SC_H; im.wrapx = 0; im.wrapy = 0; im.p = &cv[0][0];
    return im;
}

void spr_heart(int cx, int cy, int s, uint8_t c)
{
    ArtImg im = canvas_img();
    int px, py;
    to_canvas(cx * 16 + 8, cy * 16 + 8, &px, &py);
    img_heart(&im, px, py, s, c);
}

void spr_star(int cx, int cy, int r, uint8_t c)
{
    ArtImg im = canvas_img();
    int px, py;
    to_canvas(cx * 16 + 8, cy * 16 + 8, &px, &py);
    img_star(&im, px, py, r, 1, c);
}

/* a 4-point twinkle (+ shape with a bright core) */
void spr_twinkle(int cx, int cy, int r, uint8_t core, uint8_t arm)
{
    int k;
    for (k = 1; k <= r; k++) {
        uint8_t c = k == r ? art_darker(arm, 1) : arm;
        spr_px(cx + k, cy, c); spr_px(cx - k, cy, c);
        spr_px(cx, cy + k, c); spr_px(cx, cy - k, c);
    }
    spr_px(cx, cy, core);
}

uint8_t spr_get(int x, int y)
{
    int px = SC_AX + x, py = SC_AY + y;
    if (px < 0 || py < 0 || px >= SC_W || py >= SC_H) return 0;
    return cv[py][px];
}

void spr_set(int x, int y, uint8_t c)
{
    int px = SC_AX + x, py = SC_AY + y;
    if (px < 0 || py < 0 || px >= SC_W || py >= SC_H) return;
    cv[py][px] = c;
    if (!c) pid[py][px] = 0;
}

/* ---- post passes ----------------------------------------------------------------- */
static uint8_t tmp[SC_H][SC_W];

void spr_contours(void)
{
    int x, y;
    memcpy(tmp, cv, sizeof tmp);
    for (y = 1; y < SC_H - 1; y++)
        for (x = 1; x < SC_W - 1; x++) {
            int p = pid[y][x], k, hit = 0;
            static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            if (!cv[y][x] || !p) continue;
            if (pflags[p] & (SH_NOCONTOUR | SH_GLOW)) continue;
            for (k = 0; k < 4; k++) {
                int q = pid[y + dy[k]][x + dx[k]];
                if (!cv[y + dy[k]][x + dx[k]] || q <= p) continue;
                if (pflags[q] & SH_NOCONTOUR) continue;
                hit = 1;
            }
            if (hit) tmp[y][x] = art_darker(cv[y][x], 2);
        }
    memcpy(cv, tmp, sizeof tmp);
}

static int is_glow(int x, int y) { return pid[y][x] && (pflags[pid[y][x]] & SH_GLOW); }

void spr_outline(void)
{
    int x, y;
    memcpy(tmp, cv, sizeof tmp);
    for (y = 1; y < SC_H - 1; y++)
        for (x = 1; x < SC_W - 1; x++) {
            uint8_t best = 0, c;
            int bl = 1000, from_above = 0, lit = 1, k, n = 0;
            static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            if (cv[y][x]) continue;
            for (k = 0; k < 4; k++) {
                uint8_t q = cv[y + dy[k]][x + dx[k]];
                if (!q) continue;
                n++;
                if (art_luma(q) < bl) { bl = art_luma(q); best = q; }
                if (dy[k] == -1) from_above = 1;
                /* neighbour to our left or above means we are on the
                 * shadow (right/bottom) side of the shape */
                if (dx[k] == -1 || dy[k] == -1) lit = 0;
                if (is_glow(x + dx[k], y + dy[k])) best = q, bl = -1;
            }
            if (!n) continue;
            if (bl == -1) { tmp[y][x] = art_darker(best, 2); continue; }
            c = art_outline_of(best);
            if (from_above) c = art_darker(c, 1);
            else if (lit) c = art_lighter(c, 1);
            tmp[y][x] = c;
        }
    memcpy(cv, tmp, sizeof tmp);
}

void spr_outline_col(uint8_t c)
{
    int x, y;
    memcpy(tmp, cv, sizeof tmp);
    for (y = 1; y < SC_H - 1; y++)
        for (x = 1; x < SC_W - 1; x++)
            if (!cv[y][x] && (cv[y][x - 1] || cv[y][x + 1] || cv[y - 1][x] || cv[y + 1][x]))
                tmp[y][x] = c;
    memcpy(cv, tmp, sizeof tmp);
}

void spr_erode(uint32_t seed, int amount)
{
    /* works on 2x2 blocks with a ragged edge, so dissolving vapour breaks
     * into soft clumps instead of single-pixel noise */
    int x, y;
    for (y = 0; y < SC_H; y++)
        for (x = 0; x < SC_W; x++) {
            uint32_t h = art_hash((uint32_t)(x >> 1), (uint32_t)(y >> 1), seed);
            int a = amount + (int)(art_hash((uint32_t)x, (uint32_t)y, seed + 1u) & 31u) - 16;
            if (cv[y][x] && (int)(h & 255u) < a) {
                cv[y][x] = 0;
                pid[y][x] = 0;
            }
        }
}

/* remove isolated single pixels (no 4-neighbour) - orphan cleanup */
void spr_clean(void)
{
    int x, y;
    for (y = 1; y < SC_H - 1; y++)
        for (x = 1; x < SC_W - 1; x++)
            if (cv[y][x] && !cv[y][x - 1] && !cv[y][x + 1] && !cv[y - 1][x] && !cv[y + 1][x]) {
                cv[y][x] = 0;
                pid[y][x] = 0;
            }
}

void spr_recolor(const uint8_t *map)
{
    int x, y;
    for (y = 0; y < SC_H; y++)
        for (x = 0; x < SC_W; x++)
            if (cv[y][x]) cv[y][x] = map[cv[y][x]];
}

void spr_brighten(int steps)
{
    int x, y;
    for (y = 0; y < SC_H; y++)
        for (x = 0; x < SC_W; x++)
            if (cv[y][x])
                cv[y][x] = steps > 0 ? art_lighter(cv[y][x], steps) : art_darker(cv[y][x], -steps);
}

void spr_flip(void)
{
    int x, y;
    for (y = 0; y < SC_H; y++)
        for (x = 0; x < SC_AX; x++) {
            int x2 = 2 * SC_AX - 1 - x;
            uint8_t c;
            uint16_t p;
            if (x2 >= SC_W) continue;
            c = cv[y][x]; cv[y][x] = cv[y][x2]; cv[y][x2] = c;
            p = pid[y][x]; pid[y][x] = pid[y][x2]; pid[y][x2] = p;
        }
}

/* ---- finish -------------------------------------------------------------- */
void spr_finish(ArtFrame *out, int anchor)
{
    int x, y, minx = SC_W, maxx = -1, miny = SC_H, maxy = -1;
    int x0, x1, y0, y1, w, h;
    uint8_t *p;
    for (y = 0; y < SC_H; y++)
        for (x = 0; x < SC_W; x++)
            if (cv[y][x]) {
                if (x < minx) minx = x;
                if (x > maxx) maxx = x;
                if (y < miny) miny = y;
                if (y > maxy) maxy = y;
            }
    if (maxx < 0) {                        /* empty frame: 2x2 clear       */
        p = art_alloc(4);
        memset(p, 0, 4);
        out->w = 2; out->h = 2; out->pix = p;
        return;
    }
    if (anchor == ANCHOR_TIGHT) {
        x0 = minx; x1 = maxx + 1; y0 = miny; y1 = maxy + 1;
    } else {
        int half = SC_AX - minx;
        if (maxx + 1 - SC_AX > half) half = maxx + 1 - SC_AX;
        x0 = SC_AX - half; x1 = SC_AX + half;
        if (anchor == ANCHOR_CENTER) {
            int vh = SC_AY - miny;
            if (maxy + 1 - SC_AY > vh) vh = maxy + 1 - SC_AY;
            y0 = SC_AY - vh; y1 = SC_AY + vh;
        } else {
            y0 = miny;
            y1 = maxy + 1 > SC_AY ? maxy + 1 : SC_AY;
        }
    }
    w = x1 - x0; h = y1 - y0;
    p = art_alloc(w * h);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int sx = x0 + x, sy = y0 + y;
            p[y * w + x] = (sx >= 0 && sy >= 0 && sx < SC_W && sy < SC_H) ? cv[sy][sx] : 0;
        }
    out->w = w; out->h = h; out->pix = p;
}
