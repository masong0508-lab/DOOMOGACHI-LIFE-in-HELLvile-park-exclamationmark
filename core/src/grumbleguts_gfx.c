/* grumbleguts_gfx.c - GATEKEEPER GRUMBLEGUTS and his stage, drawn the way
 * the concept sheets draw them (see grumbleguts_gfx.h; owner: story).
 *
 * Everything is integer scanline work: ellipses come out as one span per
 * row (an incremental square root, no division per pixel, equal rows
 * merged into one rect), triangles as spans, lines as plat_line. The
 * emitters below write either straight to the platform (plat_rect /
 * plat_checker / plat_line) or into a cache canvas (an 8bpp image in the
 * scene scratch), where the dither seams can also test the pixel under
 * them exactly like the concept renderer does.
 */
#include <string.h>
#include "grumbleguts_gfx.h"

/* ====================================================================== */
/* emitters: direct, or the cache canvas                                    */
static uint8_t *cv_pix;
static int cv_on, cv_x0, cv_y0, cv_w, cv_h;
static int cv_bx0, cv_by0, cv_bx1, cv_by1;

static void cv_begin(uint8_t *pix, int x0, int y0, int w, int h)
{
    cv_on = 1;
    cv_pix = pix;
    cv_x0 = x0; cv_y0 = y0; cv_w = w; cv_h = h;
    cv_bx0 = cv_by0 = 0x7FFF;
    cv_bx1 = cv_by1 = -0x7FFF;
}

static void cv_end(void)
{
    cv_on = 0;
    cv_pix = NULL;
}

static void cv_bounds(int x, int y, int w, int h)
{
    cv_bx0 = DOOMO_MIN(cv_bx0, x); cv_by0 = DOOMO_MIN(cv_by0, y);
    cv_bx1 = DOOMO_MAX(cv_bx1, x + w); cv_by1 = DOOMO_MAX(cv_by1, y + h);
}

static void em_rect(int x, int y, int w, int h, uint8_t c)
{
    int j;
    if (w <= 0 || h <= 0) return;
    if (!cv_on) { plat_rect(x, y, w, h, c); return; }
    cv_bounds(x, y, w, h);
    if (!cv_pix) return;
    x -= cv_x0; y -= cv_y0;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > cv_w) w = cv_w - x;
    if (y + h > cv_h) h = cv_h - y;
    if (w <= 0 || h <= 0) return;
    if (!c) c = PAL_BLACK;
    for (j = 0; j < h; j++) memset(cv_pix + (y + j) * cv_w + x, c, (size_t)w);
}

/* one checkerboard row: the pixels where (x + y) is even; on the canvas
 * only over colour `over` (0 = any) */
static void em_checker(int x, int y, int w, uint8_t c, uint8_t over)
{
    int i, x1;
    uint8_t *p;
    if (w <= 0) return;
    if (!cv_on) {
        int o = (x + y) & 1;
        if (w - o > 0) plat_checker(x + o, y, w - o, 1, c);
        return;
    }
    cv_bounds(x, y, w, 1);
    if (!cv_pix) return;
    x1 = x + w - cv_x0;
    x -= cv_x0; y -= cv_y0;
    if (y < 0 || y >= cv_h) return;
    if (x < 0) x = 0;
    if (x1 > cv_w) x1 = cv_w;
    if (!c) c = PAL_BLACK;
    p = cv_pix + y * cv_w;
    for (i = x + ((x + cv_x0 + y + cv_y0) & 1); i < x1; i += 2)
        if (!over || p[i] == over) p[i] = c;
}

void gg_rect(int x, int y, int w, int h, uint8_t c) { em_rect(x, y, w, h, c); }
void gg_checker(int x, int y, int w, int h, uint8_t c)
{
    int j;
    if (!cv_on) { plat_checker(x, y, w, h, c); return; }
    for (j = 0; j < h; j++) em_checker(x, y + j, w, c, 0);
}

void gg_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx, sx, dy, sy, err;
    if (!cv_on) { plat_line(x0, y0, x1, y1, c); return; }
    dx = x1 > x0 ? x1 - x0 : x0 - x1;
    dy = y1 > y0 ? y0 - y1 : y1 - y0;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;
    for (;;) {
        int e2;
        em_rect(x0, y0, 1, 1, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ====================================================================== */
/* shapes                                                                   */
#define GG_RMAX 160

/* half-widths of an ellipse, one per row 0..ry (an incremental integer
 * square root: the root only ever shrinks from row to row) */
static void ell_rows(int rx, int ry, int16_t *dx)
{
    uint32_t s = 2u * (uint32_t)ry + 1u, t, k = rx == ry ? 0u : ((uint32_t)rx << 15) / (uint32_t)ry;
    int y;
    for (y = 0; y <= ry; y++) {
        t = (uint32_t)((ry * ry - y * y) * 4 + ry * 2);
        while (s * s > t) s--;
        dx[y] = (int16_t)(k ? ((s * k) >> 16) : s / 2u);
    }
}

void gg_ellipse(int cx, int cy, int rx, int ry, uint8_t c)
{
    int16_t tab[GG_RMAX + 1];
    int y, prev = -1, y0 = 0;
    if (rx < 0 || ry < 0) return;
    if (ry == 0) { em_rect(cx - rx, cy, 2 * rx + 1, 1, c); return; }
    if (ry > GG_RMAX) ry = GG_RMAX;
    ell_rows(rx, ry, tab);
    for (y = -ry; y <= ry + 1; y++) {
        int d = y <= ry ? tab[y < 0 ? -y : y] : -2;
        if (d != prev) {
            if (prev >= 0) em_rect(cx - prev, cy + y0, 2 * prev + 1, y - y0, c);
            prev = d;
            y0 = y;
        }
    }
}

/* a 2 px dithered ring just outside the ellipse (rx, ry) */
static void gg_ring(int cx, int cy, int rx, int ry, uint8_t c, uint8_t over)
{
    int16_t ti[GG_RMAX + 1], to[GG_RMAX + 1];
    int y;
    if (rx < 1 || ry < 1 || ry + 2 > GG_RMAX) return;
    ell_rows(rx, ry, ti);
    ell_rows(rx + 2, ry + 2, to);
    for (y = -(ry + 2); y <= ry + 2; y++) {
        int ay = y < 0 ? -y : y, dxo = to[ay], dxi = ay <= ry ? ti[ay] : -1;
        if (dxi < 0) em_checker(cx - dxo, cy + y, 2 * dxo + 1, c, over);
        else {
            em_checker(cx - dxo, cy + y, dxo - dxi, c, over);
            em_checker(cx + dxi + 1, cy + y, dxo - dxi, c, over);
        }
    }
}

void gg_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c)
{
    /* sorted by y; the long edge runs top to bottom, the two short edges
     * meet at the middle vertex: fixed-point 16.16 steps, one division per
     * edge */
    int t;
    int32_t xl, xs, dl, ds;
    int y, ym;
    if (y1 < y0) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y2 < y0) { t = x0; x0 = x2; x2 = t; t = y0; y0 = y2; y2 = t; }
    if (y2 < y1) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    if (y2 == y0) {
        int lo = DOOMO_MIN(x0, DOOMO_MIN(x1, x2)), hi = DOOMO_MAX(x0, DOOMO_MAX(x1, x2));
        em_rect(lo, y0, hi - lo + 1, 1, c);
        return;
    }
    dl = ((int32_t)(x2 - x0) << 16) / (y2 - y0);
    xl = ((int32_t)x0 << 16) + 0x8000;
    ds = y1 > y0 ? ((int32_t)(x1 - x0) << 16) / (y1 - y0) : 0;
    xs = ((int32_t)x0 << 16) + 0x8000;
    ym = y1;
    for (y = y0; y <= y2; y++) {
        int a, b;
        if (y == y1) {
            xs = ((int32_t)x1 << 16) + 0x8000;
            ds = y2 > y1 ? ((int32_t)(x2 - x1) << 16) / (y2 - y1) : 0;
            if (y1 == y0) { int lo = DOOMO_MIN(x0, x1), hi = DOOMO_MAX(x0, x1); em_rect(lo, y, hi - lo + 1, 1, c); xl += dl; continue; }
        }
        a = (int)(xl >> 16);
        b = (int)(xs >> 16);
        if (y == y2) b = x2;
        if (a > b) { t = a; a = b; b = t; }
        em_rect(a, y, b - a + 1, 1, c);
        xl += dl;
        xs += ds;
    }
    (void)ym;
}

/* the shaded blob: outline, five tones of the ramp lit from the upper
 * left, a rim light bottom-right, a dithered seam on every tone step, a
 * specular. seams: 0 none, 1 both, 2 the outer one only (cheap) */
static int gg_q = 2;
void gg_set_quality(int q) { gg_q = q; }
/* the figure's internal layers (the public GGL_FRONT = both arms + collar +
 * head), so the cache can keep them as separate, tighter images */
enum { L_ARM_L = 256, L_ARM_R = 512, L_COLHEAD = 1024 };

void gg_blob(int cx, int cy, int rx, int ry, int ramp, int seams)
{
    uint8_t m[5];
    int k;
    m[0] = PAL_RAMP(ramp, 2); m[1] = PAL_RAMP(ramp, 3); m[2] = PAL_RAMP(ramp, 5);
    m[3] = PAL_RAMP(ramp, 6); m[4] = PAL_RAMP(ramp, 7);
    rx = DOOMO_MAX(rx, 2);
    ry = DOOMO_MAX(ry, 2);
    if (!cv_on && gg_q < 2) {                                       /* the cheap ones: outline, shade, light, a specular dot */
        gg_ellipse(cx, cy, rx + 1, ry + 1, GG_LINE);
        gg_ellipse(cx, cy, rx, ry, m[1]);
        gg_ellipse(cx - rx / 6, cy - ry / 6, rx - rx / 4, ry - ry / 4, m[2]);
        if (gg_q == 1 && rx >= 5) gg_ellipse(cx - rx / 3, cy - ry / 3, rx / 2, ry / 2, m[3]);
        em_rect(cx - rx / 2 - rx / 10, cy - ry / 2, rx / 4 + 1, ry / 6 + 1, m[4]);
        return;
    }
    gg_ellipse(cx, cy, rx + 1, ry + 1, GG_LINE);
    gg_ellipse(cx, cy, rx, ry, m[0]);
    gg_ellipse(cx + rx / 10, cy + ry / 10, rx - rx / 8, ry - ry / 8, m[4]);      /* rim light */
    gg_ellipse(cx, cy, rx - rx / 7, ry - ry / 7, m[1]);
    for (k = 2; k <= 3; k++) {
        int sh = k - 1, ex = cx - rx * sh / 6, ey = cy - ry * sh / 6;
        int erx = rx - rx * (k + 1) / 7, ery = ry - ry * (k + 1) / 7;
        if (erx < 1 || ery < 1) continue;
        if (seams && (k == 2 || seams == 1) && erx >= 4 && ery >= 3) gg_ring(ex, ey, erx, ery, m[k], m[k - 1]);
        gg_ellipse(ex, ey, erx, ery, m[k]);
    }
    gg_ellipse(cx - rx / 2, cy - ry / 2, rx / 5 + 1, ry / 6 + 1, m[4]);          /* specular */
}

void gg_studs(int x0, int y0, int x1, int y1, int n, int s)
{
    int i;
    if (n < 1) n = 1;
    for (i = 0; i <= n; i++) em_rect(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, s + 1, s + 1, GG_STUD);
}

void gg_strap(int x0, int y0, int x1, int y1, int w)
{
    int k;
    for (k = -w; k <= w; k++) gg_line(x0 + k, y0, x1 + k, y1, GG_LEATH);
    gg_line(x0 - w - 1, y0, x1 - w - 1, y1, GG_LINE);
    gg_line(x0 + w + 1, y0, x1 + w + 1, y1, GG_LINE);
    gg_studs(x0, y0, x1, y1, 4, w);
}

void gg_heart(int x, int y, int size, uint8_t c)
{
    int s = DOOMO_MAX(size, 1);
    gg_ellipse(x - s, y - s / 2, s, s, c);
    gg_ellipse(x + s, y - s / 2, s, s, c);
    gg_tri(x - 2 * s, y, x + 2 * s, y, x, y + 2 * s, c);
}

void gg_shout(int x, int y, const char *s, uint8_t c)
{
    int w = ui_text_width(s), hi = plat_screen_w() - w - 2;
    ui_text_outline(DOOMO_CLAMP(x, 2, DOOMO_MAX(hi, 2)), y, s, c, PAL_BLACK);
}

void gg_burst(int x, int y, int r, uint8_t c)
{
    static const int8_t dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 }, dy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    int i;
    for (i = 0; i < 8; i++) {
        int d = (i & 1) ? r * 7 / 10 : r;
        gg_line(x + dx[i] * d / 2, y + dy[i] * d / 2, x + dx[i] * d, y + dy[i] * d, c);
        gg_line(x + dx[i] * d / 2 + 1, y + dy[i] * d / 2, x + dx[i] * d + 1, y + dy[i] * d, c);
    }
}

/* ====================================================================== */
/* the man himself                                                          */
#define SKIN     PAL_RAMP(RAMP_BLOOD, 5)
#define SKIN_D   PAL_RAMP(RAMP_BLOOD, 3)
#define SKIN_L   PAL_RAMP(RAMP_BLOOD, 6)
#define HORN_D   PAL_RAMP(RAMP_BONE, 3)
#define WING     PAL_RAMP(RAMP_VAPE, 2)
#define WING_D   PAL_RAMP(RAMP_VAPE, 1)
#define WING_L   PAL_RAMP(RAMP_VAPE, 3)
#define EYE_W    PAL_RAMP(RAMP_LAVA, 6)
#define EYE_O    PAL_RAMP(RAMP_LAVA, 4)
#define PUPIL    PAL_RAMP(RAMP_STONE, 0)
#define FANG     PAL_RAMP(RAMP_BONE, 7)
#define MOUTH    PAL_RAMP(RAMP_FLESH, 1)
#define TONGUE   PAL_RAMP(RAMP_FLESH, 4)
#define BLUSH    PAL_RAMP(RAMP_CANDY, 6)
#define NAVEL    PAL_RAMP(RAMP_PEACH, 3)

static int gg_sw;           /* seams for the blobs of the current figure */

void gg_params_init(GgParams *p)
{
    memset(p, 0, sizeof(*p));
    p->dir = 1;
}

int gg_belly_r(int s, int pose, int sw)
{
    if (pose == GGP_INFLATED && sw < 256) sw = 256;
    if (pose == GGP_SUCK_IN) return 13 * s;
    return 18 * s + 9 * s * sw / 256;
}
int gg_belly_ry(int s, int pose, int sw)
{
    if (pose == GGP_INFLATED && sw < 256) sw = 256;
    if (pose == GGP_SUCK_IN) return 9 * s;
    return 14 * s + 8 * s * sw / 256;
}
int gg_butt_r(int s, int pose, int sw)
{
    if (pose == GGP_INFLATED && sw < 256) sw = 256;
    return 13 * s + 6 * s * sw / 256;
}
int gg_belly_cy(int yf, int s, int pose, int sw, int bob, int belly_y)
{
    return yf - 30 * s - 6 * s + bob - 2 * s + gg_belly_ry(s, pose, sw) + belly_y;
}

static void wing(int x, int y, int s, int dir, int flap)
{
    int tip = 17 * s, x1 = x + dir * tip, y1 = y - 8 * s + flap;
    int x2 = x + dir * (tip - 2 * s), y2 = y + 2 * s + flap / 2;
    int x3 = x + dir * (tip / 2), y3 = y + 3 * s;
    gg_tri(x, y, x1, y1, x2, y2, GG_LINE);
    gg_tri(x, y, x2, y2, x3, y3, GG_LINE);
    gg_tri(x + dir, y, x1 - dir, y1 + 1, x2 - dir, y2 - 1, WING);
    gg_tri(x + dir, y, x2 - dir, y2 - 1, x3 - dir, y3 - 1, WING_D);
    gg_line(x, y, x1, y1, WING_L);
    gg_line(x, y, x2, y2, WING_L);
    gg_line(x, y, x3, y3, WING_L);
    gg_ellipse(x1, y1, 1, 1, GG_HORN);                            /* the thumb claw */
}

static void tail(int x, int y, int s, int dir, int lag)
{
    int i, tx, ty, r, step = (!cv_on && gg_q == 0) ? 2 : 1;        /* cheap: every other bead */
    for (i = 0; i < 9; i += step) {
        tx = x + dir * i * 3 * s / 2;
        ty = y + (i * i) / 3 - i * 2 + lag * i / 9;
        r = s + 2 - i / 4 + (step - 1);
        gg_ellipse(tx, ty, r + 1, r + 1, GG_LINE);
        gg_ellipse(tx, ty, r, r, (i & 1) ? SKIN : SKIN_D);
        if (i % 3 == 1) gg_tri(tx, ty - r, tx - 2, ty - r - 3 * s / 2, tx + 2, ty - r - 3 * s / 2, GG_HORN);
    }
    tx = x + dir * 27 * s / 2;
    ty = y + 9 + lag;
    gg_tri(tx - dir * 2, ty - 2 * s - 1, tx + dir * 5 * s / 2 + dir, ty, tx - dir * 2, ty + 2 * s + 1, GG_LINE);
    gg_tri(tx - dir * 2, ty - 2 * s, tx + dir * 5 * s / 2, ty, tx - dir * 2, ty + 2 * s, SKIN_D);
}

static void horns(int cx, int cy, int s)
{
    int side;
    for (side = -1; side <= 1; side += 2) {
        int x = cx + side * 9 * s / 2;
        gg_ellipse(x, cy - 2 * s, 3 * s / 2 + 1, 4 * s + 1, GG_LINE);
        gg_ellipse(x, cy - 2 * s, 3 * s / 2, 4 * s, HORN_D);
        gg_ellipse(x - side, cy - 3 * s, s, 3 * s, GG_HORN);
        gg_ellipse(x + side * s, cy - 6 * s, s, s, GG_HORN);
    }
}

static void ears(int hx, int head_y, int s)
{
    int side;
    for (side = -1; side <= 1; side += 2) {
        gg_tri(hx + side * 13 * s, head_y, hx + side * 19 * s, head_y - 7 * s, hx + side * 10 * s, head_y - 7 * s, GG_LINE);
        gg_tri(hx + side * 13 * s, head_y + 1, hx + side * 18 * s, head_y - 6 * s, hx + side * 11 * s, head_y - 6 * s, SKIN);
    }
}

/* the wide fanged grin (w = half width) */
static void grin(int cx, int my, int s, int w, int tongue)
{
    int i;
    gg_ellipse(cx, my, w + 1, 3 * s, GG_LINE);
    gg_ellipse(cx, my, w, 3 * s - 1, MOUTH);
    gg_ellipse(cx, my - 2 * s, w + 1, 2 * s, SKIN);                 /* upper lip cut */
    gg_line(cx - w - 1, my - s, cx + w + 1, my - s, GG_LINE);
    for (i = -2; i <= 2; i++) {                                     /* fangs */
        int fx = cx + i * w / 2;
        gg_tri(fx - 2, my - s, fx + 2, my - s, fx, my + s + (i == 0 ? 2 : 0), FANG);
    }
    if (tongue) gg_ellipse(cx, my + 2 * s, w / 2, s, TONGUE);
}

static void face(int cx, int cy, int s, int expr, int dir, int blink)
{
    int ex = 4 * s, ey = cy - 2 * s, side, my = cy + 4 * s, w;
    int wide = expr == GGE_SHOCK || expr == GGE_YELP || expr == GGE_DEADPAN;
    for (side = -1; side <= 1; side += 2) {
        int x = cx + side * ex, rx = 3 * s, ry = wide ? 3 * s : 2 * s, by, px, pr;
        int shut = blink || expr == GGE_RELIEF || (expr == GGE_WINK && side == dir);
        if (expr == GGE_GRIT) ry = s + 1;
        if (shut) {
            if (expr == GGE_RELIEF) {                               /* happy arcs */
                gg_line(x - 2 * s, ey + 1, x, ey - s, GG_LINE);
                gg_line(x, ey - s, x + 2 * s, ey + 1, GG_LINE);
                gg_line(x - 2 * s, ey + 2, x, ey - s + 1, GG_LINE);
                gg_line(x, ey - s + 1, x + 2 * s, ey + 2, GG_LINE);
            } else {
                gg_line(x - 2 * s, ey, x + 2 * s, ey + 1, GG_LINE);
                gg_line(x - 2 * s, ey + 1, x + 2 * s, ey + 2, GG_LINE);
            }
            continue;
        }
        gg_ellipse(x, ey, rx + 1, ry + 1, GG_LINE);
        gg_ellipse(x, ey, rx, ry, EYE_O);
        gg_ellipse(x - 1, ey - 1, rx - 1, ry - 1, EYE_W);
        px = x + dir * s;
        pr = expr == GGE_SHOCK ? 1 : s + 1;
        if (expr == GGE_DOPEY) px = x - side * s;                   /* crossed */
        if (expr == GGE_DEADPAN) { px = x; pr = s; }
        if (expr == GGE_GRIT) { px = x; pr = s; }
        if (expr == GGE_LOVE) gg_heart(x - 1, ey - 1, s + 1, PAL_PAINT(PAINT_CHERRY, 1));
        else {
            gg_ellipse(px, ey, pr, pr, PUPIL);
            em_rect(px - 1, ey - 1, 1, 1, PAL_WHITE);
        }
        if (expr == GGE_SULTRY) {                                   /* heavy lids */
            gg_ellipse(x, ey - ry / 2 - s, rx + 1, ry / 2 + s, SKIN);
            gg_line(x - rx - 1, ey - s + 1, x + rx + 1, ey - s + 1, GG_LINE);
            gg_line(x - rx - 1, ey - s, x + rx + 1, ey - s, GG_LINE);
        }
        by = ey - ry - 2;                                           /* brows */
        if (wide) {
            gg_line(x - 3 * s, by - 2, x + 3 * s, by - 2, GG_LINE);
            gg_line(x - 3 * s, by - 3, x + 3 * s, by - 3, GG_LINE);
        } else if (expr == GGE_SULTRY) {
            gg_line(x - 3 * s * side, by + 1, x + 3 * s * side, by, GG_LINE);
            gg_line(x - 3 * s * side, by + 2, x + 3 * s * side, by + 1, GG_LINE);
        } else {
            gg_line(x - 3 * s * side, by + 2, x + 3 * s * side, by - 1, GG_LINE);
            gg_line(x - 3 * s * side, by + 3, x + 3 * s * side, by, GG_LINE);
        }
    }
    if (expr == GGE_BLUSH) {
        /* big blush patches, one eye bulging more than the other, a tiny "o" */
        int i;
        gg_ellipse(cx - 7 * s, cy + 2 * s, 4 * s, 2 * s, BLUSH);
        gg_ellipse(cx + 7 * s, cy + 2 * s, 4 * s, 2 * s, BLUSH);
        for (i = 0; i < 3; i++) {
            gg_line(cx - 9 * s + i * 2, cy + s, cx - 8 * s + i * 2, cy + 3 * s, PAL_RAMP(RAMP_BLOOD, 7));
            gg_line(cx + 5 * s + i * 2, cy + s, cx + 6 * s + i * 2, cy + 3 * s, PAL_RAMP(RAMP_BLOOD, 7));
        }
        gg_ellipse(cx + 4 * s, cy - 2 * s, 4 * s, 4 * s, GG_LINE);
        gg_ellipse(cx + 4 * s, cy - 2 * s, 4 * s - 1, 4 * s - 1, EYE_W);
        gg_ellipse(cx + 4 * s, cy - 2 * s, 1, 1, PUPIL);
        gg_ellipse(cx, cy + 5 * s, 2, 2, GG_LINE);
        gg_ellipse(cx, cy + 5 * s, 1, 1, MOUTH);
        return;
    }
    if (expr == GGE_SUCK) { gg_ellipse(cx, my, 2, 2, GG_LINE); gg_ellipse(cx, my, 1, 1, MOUTH); return; }
    if (expr == GGE_SHOCK || expr == GGE_YELP) {
        gg_ellipse(cx, my + s, 3 * s, 3 * s + 1, GG_LINE);
        gg_ellipse(cx, my + s, 3 * s - 1, 3 * s, MOUTH);
        gg_ellipse(cx, my + 2 * s + 1, 2 * s, s, TONGUE);
        gg_tri(cx - 2 * s, my - s, cx - s, my + s, cx - 3 * s + 1, my + s, FANG);
        gg_tri(cx + 2 * s, my - s, cx + s, my + s, cx + 3 * s - 1, my + s, FANG);
        return;
    }
    if (expr == GGE_DEADPAN) {
        em_rect(cx - 4 * s, my, 8 * s, s + 1, GG_LINE);
        return;
    }
    if (expr == GGE_GRIT) {                                         /* gritted teeth */
        int i;
        em_rect(cx - 6 * s - 1, my - s - 1, 12 * s + 2, 3 * s + 2, GG_LINE);
        em_rect(cx - 6 * s, my - s, 12 * s, 3 * s, FANG);
        for (i = -2; i <= 2; i++) em_rect(cx + i * 3 * s - (i == -2 ? 0 : 1), my - s, 1, 3 * s, GG_LINE);
        em_rect(cx - 6 * s, my, 12 * s, 1, GG_LINE);
        return;
    }
    if (expr == GGE_RELIEF) {                                       /* a small, soft smile */
        gg_ellipse(cx, my - s, 3 * s + 1, 2 * s + 1, GG_LINE);
        gg_ellipse(cx, my - 2 * s, 4 * s, 2 * s + 1, SKIN);
        gg_ellipse(cx, my - s, 3 * s, 2 * s, MOUTH);
        gg_ellipse(cx, my - 2 * s, 4 * s, 2 * s, SKIN);
        gg_ellipse(cx - 7 * s, cy + 2 * s, 3 * s, 2 * s, BLUSH);
        gg_ellipse(cx + 7 * s, cy + 2 * s, 3 * s, 2 * s, BLUSH);
        return;
    }
    w = (expr == GGE_SMUG || expr == GGE_SULTRY) ? 6 * s : 8 * s;
    grin(cx + dir * s, my, s, w, expr != GGE_DOPEY);
    if (expr == GGE_DOPEY) {                                        /* tongue lolling out */
        gg_ellipse(cx + dir * 3 * s, my + 4 * s, 2 * s + 1, 3 * s + 1, GG_LINE);
        gg_ellipse(cx + dir * 3 * s, my + 4 * s, 2 * s, 3 * s, TONGUE);
        gg_line(cx + dir * 3 * s, my + 2 * s, cx + dir * 3 * s, my + 6 * s, PAL_RAMP(RAMP_FLESH, 2));
    }
    if (expr == GGE_WINK || expr == GGE_LOVE || expr == GGE_SULTRY || expr == GGE_DOPEY) {
        gg_ellipse(cx + 6 * s, cy + 2 * s, 2 + s, 1 + s, PAL_PAINT(PAINT_ROSE, 0));
        gg_ellipse(cx - 6 * s, cy + 2 * s, 2 + s, 1 + s, PAL_PAINT(PAINT_ROSE, 0));
    }
}

static void torso(int cx, int top, int bottom, int wtop, int wbot)
{
    gg_tri(cx - wtop - 1, top, cx + wtop + 1, top, cx + wbot + 1, bottom + 1, GG_LINE);
    gg_tri(cx - wtop - 1, top, cx + wbot + 1, bottom + 1, cx - wbot - 1, bottom + 1, GG_LINE);
    gg_tri(cx - wtop, top, cx + wtop, top, cx + wbot, bottom, SKIN);
    gg_tri(cx - wtop, top, cx + wbot, bottom, cx - wbot, bottom, SKIN);
    gg_tri(cx - wtop, top, cx - wtop / 3, top, cx - wbot, bottom, SKIN_D);        /* left shade  */
    gg_tri(cx + wtop / 3, top, cx + wtop, top, cx + wbot, bottom, SKIN_L);        /* right light */
}

static void deltoid(int cx, int top, int wtop, int s, int side)
{
    gg_blob(cx + side * (wtop - 2 * s), top + 3 * s, 6 * s + 1, 5 * s + 1, RAMP_BLOOD, gg_sw);
}

static void pecs(int cx, int y, int w, int chest)
{
    gg_blob(cx - w / 2, y + chest, w / 2 + 1, w / 3, RAMP_BLOOD, gg_sw);
    gg_blob(cx + w / 2, y - chest / 2, w / 2 + 1, w / 3, RAMP_BLOOD, gg_sw);
    gg_line(cx, y - w / 3, cx, y + w / 3, GG_LINE);
    gg_line(cx - w / 2 - 2, y + w / 3 + 1, cx + w / 2 + 2, y + w / 3 + 1, SKIN_D);   /* under-pec shadow */
}

static void claws(int x, int y, int s, int n)
{
    int c;
    for (c = -(n / 2); c <= n / 2; c++)
        gg_tri(x + c * 3 * s - 2, y, x + c * 3 * s + 2, y, x + c * 3 * s, y + 2 * s + 2, GG_HORN);
}

static void leg(int lx, int hip_y, int yf, int s, int side)
{
    gg_blob(lx, (hip_y + yf - 8 * s) / 2, 7 * s, (yf - 8 * s - hip_y) / 2 + 4 * s, RAMP_BLOOD, gg_sw);   /* thigh */
    gg_blob(lx + side * s, yf - 7 * s, 6 * s, 7 * s, RAMP_BLOOD, gg_sw);                              /* calf  */
    gg_line(lx - 5 * s, yf - 12 * s, lx + 5 * s, yf - 12 * s, SKIN_D);                             /* knee  */
    claws(lx, yf - 2 * s, s, 3);
}

static void fist(int hx, int hy, int s, int cuff)
{
    gg_blob(hx, hy, 5 * s, 5 * s, RAMP_BLOOD, gg_sw);
    if (cuff) {
        em_rect(hx - 5 * s, hy - 7 * s, 10 * s, 2 * s, GG_LEATH);
        gg_studs(hx - 4 * s, hy - 6 * s, hx + 4 * s, hy - 6 * s, 3, s - 1);
    }
    claws(hx, hy + 4 * s, s, 3);
}

static void bagel(int x, int y, int s)
{
    gg_ellipse(x, y, 4 * s + 1, 3 * s + 1, PAL_RAMP(RAMP_WOOD, 1));
    gg_ellipse(x, y, 4 * s, 3 * s, PAL_PAINT(PAINT_CARAMEL, 1));
    gg_ellipse(x - s, y - s, 3 * s, 2 * s, PAL_PAINT(PAINT_CARAMEL, 2));
    gg_ellipse(x, y, s, s - (s > 1 ? 1 : 0), PAL_RAMP(RAMP_WOOD, 1));
    em_rect(x + s, y - 2 * s, 1, 1, PAL_PAINT(PAINT_HELLFIRE, 1));
}

static void arms(int cx, int chest_top, int head_y, int body_y, int s, int view, int pose, int dir, int chest, int which)
{
    int side;
    for (side = -1; side <= 1; side += 2) {
        int ax = cx + side * 22 * s, ay = chest_top + 10 * s + (side < 0 ? chest : -chest / 2), hx, hy;
        int raised = (pose == GGP_SPRING || pose == GGP_CROP) && side == 1;
        if (!(which & (side < 0 ? L_ARM_L : L_ARM_R))) continue;
        if (view == GGV_SIDE && side != dir) continue;             /* the far arm is hidden */
        if (view != GGV_SIDE) deltoid(cx, chest_top, (view == GGV_FRONT || view == GGV_BACK || view == GGV_BACK_LOOK) ? 19 * s : 12 * s, s, side);
        else deltoid(cx + dir * 2 * s, chest_top, 12 * s, s, side);
        if (view == GGV_SIDE) { ax = cx + dir * 8 * s; ay = chest_top + 10 * s + chest; }
        if (pose == GGP_WIGGLE || view == GGV_BACK || view == GGV_BACK_LOOK) ay = chest_top + 12 * s;
        gg_blob(ax, raised ? ay - 10 * s : ay, 6 * s, 9 * s, RAMP_BLOOD, gg_sw);          /* bicep */
        hx = ax + side * 2 * s;
        hy = raised ? ay - 22 * s : ay + 12 * s;
        if (view == GGV_SIDE) hx = ax + dir * 3 * s;
        if (pose == GGP_SNACK && side == 1) { hx = cx + 12 * s; hy = head_y + 6 * s; }
        if (pose == GGP_MIRROR && side == 1) { hx = cx + 26 * s; hy = body_y - 18 * s; }
        fist(hx, hy, s, 1);
        if (pose == GGP_SNACK && side == 1) bagel(hx + 4 * s, hy - 2 * s, s);
        if (pose == GGP_CROP && side == 1) {
            gg_line(hx, hy - 4 * s, hx + 2 * s, hy - 30 * s, GG_LINE);
            gg_line(hx + 1, hy - 4 * s, hx + 2 * s + 1, hy - 30 * s, GG_LEATH);
            em_rect(hx + 2 * s - 2, hy - 34 * s, 5, 5 * s, PAL_RAMP(RAMP_WOOD, 0));
        }
    }
}

static void collar(int cx, int ny, int s, int key)
{
    int i;
    em_rect(cx - 10 * s, ny, 20 * s, 3 * s, GG_LEATH);
    gg_line(cx - 10 * s, ny, cx + 10 * s, ny, GG_LINE);
    gg_line(cx - 10 * s, ny + 3 * s - 1, cx + 10 * s, ny + 3 * s - 1, GG_LINE);
    for (i = -3; i <= 3; i++) gg_tri(cx + i * 3 * s - 1, ny, cx + i * 3 * s + 1, ny, cx + i * 3 * s, ny - 3 * s, GG_CHAIN);
    if (key) {                                                      /* the gate key on its chain */
        gg_line(cx, ny + 3 * s, cx, ny + 8 * s, GG_CHAIN);
        gg_ellipse(cx, ny + 10 * s, 2, 2, GG_STUD);
        gg_ellipse(cx, ny + 10 * s, 1, 1, GG_LINE);
        gg_line(cx, ny + 12 * s, cx, ny + 16 * s, GG_STUD);
        em_rect(cx + 1, ny + 14 * s, 1, 1, GG_STUD);
        em_rect(cx + 1, ny + 15 * s + 1, 1, 1, GG_STUD);
    }
}

/* the head without its face (layers & GGL_FRONT) and / or the face (& GGL_FACE) */
static void head(int hx, int head_y, int s, int expr, int dir, int blink, int layers)
{
    if (layers & GGL_FRONT) {
        gg_blob(hx, head_y, 14 * s, 13 * s, RAMP_BLOOD, gg_sw);
        ears(hx, head_y, s);
        horns(hx, head_y - 9 * s, s);
        gg_tri(hx - 3 * s, head_y + 11 * s, hx + 3 * s, head_y + 11 * s, hx, head_y + 17 * s, WING_D);
    }
    if (layers & GGL_FACE) face(hx, head_y, s, expr, dir, blink);
}

/* belt with buckle across [x0, x1) at y */
static void belt(int x0, int x1, int y, int s, int buckle_x, int open)
{
    if (x1 - x0 < 4) return;
    em_rect(x0, y, x1 - x0, 3 * s, GG_LEATH);
    gg_line(x0, y, x1 - 1, y, GG_LINE);
    gg_line(x0, y + 3 * s - 1, x1 - 1, y + 3 * s - 1, GG_LINE);
    em_rect(x0, y, 1, 3 * s, GG_LINE);
    em_rect(x1 - 1, y, 1, 3 * s, GG_LINE);
    gg_studs(x0 + 4, y + s, x1 - 5, y + s, (x1 - x0) / (6 * s) + 1, s - 1);
    if (open) {
        em_rect(buckle_x - 3 * s, y - s, 6 * s, 5 * s, GG_STUD);
        gg_line(buckle_x - 3 * s, y - s, buckle_x + 3 * s, y - s, GG_LINE);
        gg_line(buckle_x - 3 * s, y + 4 * s, buckle_x + 3 * s, y + 4 * s, GG_LINE);
        gg_line(buckle_x + 3 * s, y + s, buckle_x + 9 * s, y + 6 * s, GG_LEATH);
    } else if (buckle_x > x0 + 2 && buckle_x < x1 - 2) {
        em_rect(buckle_x - 2 * s, y, 4 * s, 3 * s, GG_STUD);
        em_rect(buckle_x - 2 * s, y, 4 * s, 1, GG_LINE);
        em_rect(buckle_x - 2 * s, y + 3 * s - 1, 4 * s, 1, GG_LINE);
        em_rect(buckle_x - 2 * s, y, 1, 3 * s, GG_LINE);
        em_rect(buckle_x + 2 * s - 1, y, 1, 3 * s, GG_LINE);
    }
}

/* the figure, layer by layer (the GGL_* mask picks what is drawn) */
static void figure(int cx, int yf, int s, int view, int pose, int expr, const GgParams *p, int layers)
{
    int sw = p->swell, dir = p->dir < 0 ? -1 : 1;
    int belly_r = gg_belly_r(s, pose, sw), belly_ry = gg_belly_ry(s, pose, sw), butt_r = gg_butt_r(s, pose, sw);
    int body_y = yf - 30 * s + p->bob;
    int chest_top = body_y - 24 * s, chest_bot = body_y - 6 * s;
    int belly_cy = chest_bot - 2 * s + belly_ry + p->belly_y;
    int brx = belly_r - p->belly_y / 3, bry = belly_ry + p->belly_y / 3;
    int head_y = yf - 60 * s + p->bob * 2 / 3, wy = body_y - 26 * s;
    int i, side, by, head_only = (layers & GGL_HEAD) != 0;

    gg_sw = p->seams ? 1 : 0;
    if (cv_on) gg_sw = 1;                                           /* the cache always dithers */

    /* ---- wings, behind everything ---- */
    if (layers & GGL_WINGS) {
        if (view == GGV_FRONT) { wing(cx - 14 * s, wy, s, -1, p->flap); wing(cx + 14 * s, wy, s, 1, p->flap); }
        else if (view == GGV_SIDE) wing(cx - dir * 8 * s, wy, s, -dir, p->flap);
        else { wing(cx - 10 * s, wy, s, -1, p->flap); wing(cx + 10 * s, wy, s, 1, p->flap); }
    }

    /* ---- the tail wags: live, behind the body ---- */
    if (layers & GGL_LIVE) {
        if (view == GGV_SIDE) tail(cx - dir * 12 * s, yf - 22 * s, s, -dir, p->tail);
        else if (view == GGV_FRONT) tail(cx + 22 * s, yf - 16 * s, s, 1, p->tail);
        else tail(cx + 2 * s, yf - 24 * s, s, 1, p->tail);
    }

    /* ---- legs, torso, harness ---- */
    if (layers & GGL_BACK) {
        if (view == GGV_SIDE) {
            for (side = -1; side <= 1; side += 2) {                 /* two legs, one a stride ahead */
                int lx = cx + side * 5 * s - 2 * s * dir + (side == dir ? 2 * s : 0);
                leg(lx, body_y + 8 * s, yf, s, dir);
            }
            torso(cx + dir * 2 * s, chest_top, chest_bot, 12 * s, 10 * s);
            gg_blob(cx + dir * 9 * s, chest_top + 8 * s + p->chest, 8 * s, 5 * s, RAMP_BLOOD, gg_sw);   /* pec */
            if (!p->no_harness) gg_strap(cx - dir * 4 * s, chest_top + 2 * s, cx + dir * 10 * s, chest_bot + 2 * s, s);
        } else if (view == GGV_FRONT) {
            int hip_y = body_y + 14 * s;                            /* the thighs tuck under the belly */
            leg(cx - 9 * s, hip_y, yf, s, -1);
            leg(cx + 9 * s, hip_y, yf, s, 1);
            gg_blob(cx, hip_y, 17 * s, 7 * s, RAMP_BLOOD, gg_sw);   /* hips */
            torso(cx, chest_top, chest_bot, 19 * s, 15 * s);
            pecs(cx, chest_top + 8 * s, 16 * s, p->chest);
            if (!p->no_harness) {                                   /* X harness, ring at the sternum */
                gg_strap(cx - 15 * s, chest_top + 2 * s, cx + 11 * s, chest_bot + 4 * s, s);
                gg_strap(cx + 15 * s, chest_top + 2 * s, cx - 11 * s, chest_bot + 4 * s, s);
                gg_ellipse(cx, chest_top + 10 * s, 3 * s, 3 * s, GG_LINE);
                gg_ellipse(cx, chest_top + 10 * s, 2 * s, 2 * s, GG_CHAIN);
                gg_ellipse(cx, chest_top + 10 * s, s, s, GG_LINE);
            }
        } else {
            leg(cx - 9 * s, body_y + 8 * s, yf, s, -1);
            leg(cx + 9 * s, body_y + 8 * s, yf, s, 1);
            torso(cx, chest_top, chest_bot + 4 * s, 19 * s, 14 * s);
            gg_blob(cx - 10 * s, chest_top + 9 * s + p->chest, 8 * s, 6 * s, RAMP_BLOOD, gg_sw);    /* shoulder blades */
            gg_blob(cx + 10 * s, chest_top + 9 * s - p->chest / 2, 8 * s, 6 * s, RAMP_BLOOD, gg_sw);
            gg_line(cx, chest_top + 4 * s, cx, chest_bot, SKIN_D);                             /* spine */
            if (!p->no_harness) {
                gg_strap(cx - 15 * s, chest_top + 2 * s, cx + 10 * s, chest_bot + 2 * s, s);
                gg_strap(cx + 15 * s, chest_top + 2 * s, cx - 10 * s, chest_bot + 2 * s, s);
            }
        }
    }

    /* ---- the parts that move: belly and belt, or the cheeks ---- */
    if (layers & GGL_LIVE) {
        if (view == GGV_SIDE) {
            int butx = cx - dir * 10 * s - dir * p->lx, buty = body_y + 4 * s + p->ly;
            int bx = cx + dir * (6 * s + p->belly_x), x0, x1;
            gg_blob(butx, buty + butt_r / 2 - s, butt_r + p->ly / 3, butt_r - 2 * s - p->ly / 4, RAMP_BLOOD, gg_sw);   /* the rump */
            gg_line(butx + dir * butt_r / 3, body_y + s, butx + dir * butt_r / 3, buty + butt_r - 2 * s, GG_LINE);
            gg_blob(bx, belly_cy, brx - 2 * s, bry, RAMP_PEACH, gg_sw);                           /* the belly */
            gg_ellipse(bx + dir * (brx - 5 * s), belly_cy + 2 * s, 2, 2, NAVEL);
            if (sw >= 200) {                                                                    /* stretch marks */
                for (i = 0; i < 3; i++) gg_line(bx + dir * (brx / 3) + i * 4 * s, belly_cy - bry / 2, bx + dir * (brx / 3) + i * 4 * s + 2 * s, belly_cy - bry / 4, PAL_RAMP(RAMP_PEACH, 4));
            }
            by = belly_cy + bry - 5 * s;
            x0 = DOOMO_MIN(butx - dir * butt_r, bx + dir * (brx - 2 * s));
            x1 = DOOMO_MAX(butx - dir * butt_r, bx + dir * (brx - 2 * s));
            if (!p->no_belt) belt(x0 + 2, x1 - 2, by, s, -1000, 0);
        } else if (view == GGV_FRONT) {
            int bx = cx + p->belly_x;
            gg_blob(bx, belly_cy, brx, bry, RAMP_PEACH, gg_sw);
            gg_ellipse(bx, belly_cy + bry / 3, 2, 3, NAVEL);
            if (sw >= 200) {
                gg_ellipse(bx, belly_cy, brx - 4 * s, bry - 4 * s, PAL_RAMP(RAMP_PEACH, 4));
                gg_ellipse(bx, belly_cy, brx - 4 * s - 1, bry - 4 * s - 1, PAL_RAMP(RAMP_PEACH, 5));
                gg_ellipse(bx - brx / 6, belly_cy - bry / 6, brx - 9 * s, bry - 8 * s, PAL_RAMP(RAMP_PEACH, 4));
                gg_ellipse(bx - brx / 6, belly_cy - bry / 6, brx - 9 * s - 1, bry - 8 * s - 1, PAL_RAMP(RAMP_PEACH, 6));
                gg_ellipse(bx - brx / 2, belly_cy - bry / 2, brx / 5 + 1, bry / 6 + 1, PAL_RAMP(RAMP_PEACH, 7));
                gg_ellipse(bx, belly_cy + bry / 3, 2, 3, NAVEL);
            }
            by = belly_cy + bry - 5 * s;
            if (!p->no_belt) belt(bx - brx + 2, bx + brx - 2, by, s, bx, pose == GGP_UNBUCKLE);
        } else {
            int r = butt_r, cyc, lcx, rcx, lcy, rcy;
            by = body_y + 4 * s;
            cyc = by + r / 2;
            lcx = cx - r * 7 / 10 + p->lx; rcx = cx + r * 7 / 10 + p->rx;
            lcy = cyc + p->ly; rcy = cyc + p->ry;
            gg_blob(lcx, lcy, r * 19 / 20 + (p->ly > 0 ? p->ly / 3 : 0), r - p->ly / 4, RAMP_BLOOD, gg_sw);   /* THE cheeks */
            gg_blob(rcx, rcy, r * 19 / 20 + (p->ry > 0 ? p->ry / 3 : 0), r - p->ry / 4, RAMP_BLOOD, gg_sw);
            gg_line(cx, by - 2, cx, cyc + r - 2 * s, GG_LINE);
            gg_line(cx + 1, by - 2, cx + 1, cyc + r - 2 * s, GG_LINE);
            if (!p->no_belt) belt(cx - 17 * s, cx + 17 * s, by - 2 * s, s, -1000, 0);       /* belt above the cheeks */
            if (pose == GGP_WIGGLE) {
                for (i = 0; i < 3; i++) {
                    gg_line(cx - r - 4 * s - i * 3, by + 2 + i * 4, cx - r - 1 * s - i * 3, by + 2 + i * 4, GG_LINE);
                    gg_line(cx + r + 1 * s + i * 3, by + 2 + i * 4, cx + r + 4 * s + i * 3, by + 2 + i * 4, GG_LINE);
                }
            }
            if (pose == GGP_INFLATED) {                              /* the pressure gauge on a cheek */
                gg_ellipse(rcx, rcy, 4, 4, GG_LINE);
                gg_ellipse(rcx, rcy, 3, 3, PAL_WHITE);
                gg_line(rcx, rcy, rcx + 3, rcy - 2, PAL_UI_RED);
            }
        }
    }

    /* ---- arms, collar, head (FRONT), the face (FACE) ---- */
    if (layers & GGL_HEAD) layers = (layers & ~GGL_HEAD) | L_COLHEAD;      /* the head alone */
    if (layers & GGL_FRONT) layers |= L_ARM_L | L_ARM_R | L_COLHEAD;
    if (layers & (L_ARM_L | L_ARM_R)) arms(cx, chest_top, head_y, body_y, s, view, pose, dir, p->chest, layers);
    if (layers & L_COLHEAD) layers |= GGL_FRONT;                            /* head(): the head without its face */
    else layers &= ~GGL_FRONT;
    if (layers & (GGL_FRONT | GGL_FACE)) {
        int ny = head_y + 15 * s;
        if (view == GGV_SIDE) {
            int hx = cx + dir * 3 * s;
            if ((layers & GGL_FRONT) && !head_only) {
                collar(hx, ny, s, 0);
                gg_line(hx + dir * 4 * s, ny + 3 * s, hx + dir * 4 * s, ny + 7 * s, GG_CHAIN);
                gg_ellipse(hx + dir * 4 * s, ny + 9 * s, 2, 2, GG_STUD);
            }
            head(hx, head_y, s, expr, dir, p->blink, layers);
        } else if (view == GGV_BACK) {
            if ((layers & GGL_FRONT) && !head_only) collar(cx, ny, s, 0);
            if (layers & GGL_FRONT) {
                gg_blob(cx, head_y, 13 * s, 13 * s, RAMP_BLOOD, gg_sw);
                horns(cx, head_y - 8 * s, s);
                gg_tri(cx - 12 * s, head_y, cx - 18 * s, head_y - 6 * s, cx - 10 * s, head_y - 6 * s, SKIN);
                gg_tri(cx + 12 * s, head_y, cx + 18 * s, head_y - 6 * s, cx + 10 * s, head_y - 6 * s, SKIN);
            }
        } else if (view == GGV_BACK_LOOK) {
            if ((layers & GGL_FRONT) && !head_only) collar(cx, ny, s, 0);
            head(cx - 5 * s, head_y, s, expr, -1, p->blink, layers);
        } else {
            if ((layers & GGL_FRONT) && !head_only) collar(cx, ny, s, 1);
            head(cx, head_y, s, expr, 1, p->blink, layers);
        }
    }
}

void gg_draw(int x, int y_feet, int scale, int view, int pose, int expr, const GgParams *p)
{
    GgParams d;
    if (plat_draw_skipped()) return;
    if (!p) { gg_params_init(&d); p = &d; }
    figure(x, y_feet, scale < 1 ? 1 : scale, view, pose, expr, p, p->layers ? (int)p->layers : GGL_ALL);
}

/* ====================================================================== */
/* the cache                                                                */
typedef struct GgLayer {
    uint32_t   key;
    int16_t    ox, oy;             /* image origin relative to (x, y_feet) */
    uint32_t   off, size;          /* pixels in the pool                    */
    uint16_t   w, h;               /* measured size                         */
    uint8_t    pooled;             /* kept as strips in the portrait pool   */
    DoomoImage img;
} GgLayer;
#define GG_SLOTS 6
typedef struct GgCache {
    GgLayer  lay[GG_SLOTS];
} GgCache;
#define GG_HDR   ((sizeof(GgCache) + 3u) & ~3u)
#define GG_POOL  (DOOMO_SCENE_SCRATCH - GG_HDR)
DOOMO_STATIC_ASSERT(GG_HDR + 4096u <= DOOMO_SCENE_SCRATCH, gg_cache_fits);

static GgCache *gg_cache(void)
{
    GgCache *c;
    if (scene_scratch_mine(GG_SCRATCH_TAG)) return (GgCache *)scene_scratch_block();
    c = (GgCache *)scene_scratch(GG_SCRATCH_TAG, DOOMO_SCENE_SCRATCH);
    if (!c) return NULL;
    memset(c, 0, sizeof(*c));
    return c;
}

/* (re)build slot i for key by drawing `fn` twice (measure, then render);
 * slots are laid out one after another, so the slots above i die with it.
 * 0 if it does not fit. */
typedef struct FigJob { int s, view, pose, expr, layer; const GgParams *p; } FigJob;
static void fig_fn(void *ctx)
{
    const FigJob *j = (const FigJob *)ctx;
    figure(0, 0, j->s, j->view, j->pose, j->expr, j->p, j->layer);
}

static int gg_build(GgCache *c, int i, uint32_t key, GgDrawFn fn, void *ctx)
{
    GgLayer *L = &c->lay[i];
    int w, h, j;
    uint32_t off = 0;
    uint8_t *pool = (uint8_t *)c + GG_HDR;
    for (j = 0; j < i; j++)
        if (c->lay[j].key && c->lay[j].off + c->lay[j].size > off) off = c->lay[j].off + c->lay[j].size;
    for (j = i + 1; j < GG_SLOTS; j++) c->lay[j].key = 0;
    L->key = 0;
    cv_begin(NULL, 0, 0, 0, 0);
    fn(ctx);
    cv_end();
    w = cv_bx1 - cv_bx0;
    h = cv_by1 - cv_by0;
    L->w = (uint16_t)(w > 0 ? w : 0);
    L->h = (uint16_t)(h > 0 ? h : 0);
    L->ox = (int16_t)cv_bx0;
    L->oy = (int16_t)cv_by0;
    L->pooled = 0;
    if (w <= 0 || h <= 0 || off + (uint32_t)(w * h) > GG_POOL) {
        L->key = key;                                               /* remember the miss: no retry every frame */
        L->size = 0;
        L->img.pix = NULL;
        /* too big for the block: the 320x240 figure's arms and head go to the
         * portrait pool instead, in strips of <= PORTRAIT_POOL_IMAGE_MAX
         * bytes (portrait.h: small images of other modules share its LRU) */
        if (w > 0 && h > 0 && w <= PORTRAIT_POOL_IMAGE_MAX && (h + (PORTRAIT_POOL_IMAGE_MAX / w) - 1) / (PORTRAIT_POOL_IMAGE_MAX / w) <= 4) L->pooled = 1;
        return 0;
    }
    memset(pool + off, 0, (size_t)(w * h));
    cv_begin(pool + off, cv_bx0, cv_by0, w, h);
    fn(ctx);
    cv_end();
    L->key = key;
    L->off = off;
    L->size = (uint32_t)(w * h);
    L->img.w = (uint16_t)w;
    L->img.h = (uint16_t)h;
    L->img.pix = pool + off;
    L->img.uid = doomo_image_new_uid();
    {   /* memory budget diagnostics (the native runner prints these) */
        char msg[64];
        doomo_snprintf(msg, sizeof msg, "gg: slot %d %dx%d at %u (%u of %u)", i, w, h, (unsigned)off, (unsigned)(off + L->size), (unsigned)GG_POOL);
        plat_log(msg);
    }
    return 1;
}

/* draw cached slot i at (x, y): its scratch image, or its strips from the
 * portrait pool (rendered through fn when the pool had to allocate them),
 * or 0 when neither holds it (draw it direct) */
static int gg_blit(GgCache *c, int i, uint32_t key, int x, int y, GgDrawFn fn, void *ctx)
{
    GgLayer *L = &c->lay[i];
    int strip_h, n, k;
    if (L->key != key) return 0;
    if (L->img.pix) { plat_image(x + L->ox, y + L->oy, &L->img, 1, 0, 0); return 1; }
    if (!L->pooled) return 0;
    strip_h = PORTRAIT_POOL_IMAGE_MAX / L->w;
    n = (L->h + strip_h - 1) / strip_h;
    for (k = 0; k < n; k++) {
        int y0 = k * strip_h, h = DOOMO_MIN(strip_h, L->h - y0), fresh = 0;
        uint32_t pk = (key * 0x9E3779B1u) ^ ((uint32_t)i << 28) ^ ((uint32_t)k << 24) ^ 0x47475858u;
        const DoomoImage *im = portrait_pool_image(pk, L->w, h, &fresh);
        if (!im) return k > 0;                                      /* the pool is full: finish direct next frame */
        if (fresh) {
            uint8_t *pix = (uint8_t *)im->pix;
            memset(pix, 0, (size_t)(L->w * h));
            cv_begin(pix, L->ox, L->oy + y0, L->w, h);
            fn(ctx);
            cv_end();
        }
        plat_image(x + L->ox, y + L->oy + y0, im, 1, 0, 0);
    }
    return 1;
}

const DoomoImage *gg_cache_image(int slot, uint32_t key, GgDrawFn fn, void *ctx, int *ox, int *oy)
{
    GgCache *c;
    if (slot < 0 || slot >= GG_SLOTS || key == 0 || plat_draw_skipped()) return NULL;
    c = gg_cache();
    if (!c) return NULL;
    if (c->lay[slot].key != key && !gg_build(c, slot, key, fn, ctx)) return NULL;
    if (!c->lay[slot].img.pix) return NULL;
    *ox = c->lay[slot].ox;
    *oy = c->lay[slot].oy;
    return &c->lay[slot].img;
}

void gg_draw_cached(int x, int y_feet, int scale, int view, int pose, int expr, const GgParams *p)
{
    GgParams still, d;
    GgCache *c;
    uint32_t key;
    int s = scale < 1 ? 1 : scale, i, head_only;
    uint32_t keys[4];
    FigJob jobs[4];
    if (plat_draw_skipped()) return;
    if (!p) { gg_params_init(&d); p = &d; }
    still = *p;                                                     /* the still layers: no springs */
    still.bob = still.chest = still.tail = still.flap = 0;
    still.belly_x = still.belly_y = still.lx = still.ly = still.rx = still.ry = 0;
    /* the still layers do not depend on the expression or the swell; the two
     * rear views share everything but the (live) head */
    head_only = p->layers && !(p->layers & GGL_FRONT) && (p->layers & GGL_HEAD);
    key = 1u | ((uint32_t)pose << 3) | ((uint32_t)s << 8)
        | ((uint32_t)(p->dir < 0) << 11) | ((uint32_t)(p->no_harness != 0) << 12) | ((uint32_t)head_only << 13);
    c = gg_cache();
    /* slots 1-4: the body, each arm, the collar + head: four tight images
     * instead of one with the arms' empty span, so the 320x240 figure fits
     * the 12 KB block too */
    for (i = 0; i < 4; i++) {
        uint32_t k = key | ((uint32_t)(i == 0 && view == GGV_BACK_LOOK ? GGV_BACK : view) << 1);   /* the rear views share the back */
        static const int lay[4] = { GGL_BACK, L_ARM_L, L_ARM_R, L_COLHEAD };
        keys[i] = k;
        jobs[i].s = s; jobs[i].view = view; jobs[i].pose = pose; jobs[i].expr = expr; jobs[i].p = &still;
        jobs[i].layer = lay[i] == L_COLHEAD && head_only ? GGL_HEAD : lay[i];
        if (!c || (head_only && (i == 1 || i == 2))) continue;
        if (c->lay[1 + i].key != k) gg_build(c, 1 + i, k, fig_fn, &jobs[i]);
    }
    figure(x, y_feet, s, view, pose, expr, p, GGL_WINGS);
    if (!c || !gg_blit(c, 1, keys[0], x, y_feet + p->bob, fig_fn, &jobs[0])) figure(x, y_feet, s, view, pose, expr, p, GGL_BACK);
    figure(x, y_feet, s, view, pose, expr, p, GGL_LIVE);
    if (!head_only)
        for (i = 1; i <= 2; i++)
            if (!c || !gg_blit(c, 1 + i, keys[i], x, y_feet + p->bob, fig_fn, &jobs[i])) figure(x, y_feet, s, view, pose, expr, p, i == 1 ? L_ARM_L : L_ARM_R);
    if (!c || !gg_blit(c, 4, keys[3], x, y_feet + p->bob, fig_fn, &jobs[3])) figure(x, y_feet, s, view, pose, expr, p, head_only ? GGL_HEAD : L_COLHEAD);
    figure(x, y_feet, s, view, pose, expr, p, GGL_FACE);
}

/* ====================================================================== */
/* staging                                                                  */
/* one paper lantern hanging from (x, y): glow, body, highlight, tassel */
void gg_lantern(int x, int y, int r)
{
    int ly = y + r + 2;
    gg_checker(x - r - 2, ly - r - 1, 2 * r + 5, 2 * r + 3, PAL_RAMP(RAMP_CANDY, 2));     /* glow */
    gg_rect(x, y, 1, 2, PAL_RAMP(RAMP_WOOD, 2));
    gg_ellipse(x, ly, r + 1, r + 2, PAL_RAMP(RAMP_CANDY, 1));
    gg_ellipse(x, ly, r, r + 1, PAL_RAMP(RAMP_CANDY, 5));
    gg_rect(x - r / 2 - 1, ly - r / 2 - 1, r - 1, r / 2 + 1, PAL_RAMP(RAMP_CANDY, 7));
    gg_rect(x - r + 1, ly - 1, 2 * r - 1, 1, PAL_RAMP(RAMP_CANDY, 2));
    gg_rect(x - 1, ly + r + 2, 2, 2, PAL_UI_RED);                  /* tassel */
}

/* the string itself, sagging between the lanterns; returns nothing, calls
 * `hang` for every lantern position */
static void lantern_string(int y, uint32_t t, void (*hang)(int, int, int, void *), int r, void *ctx)
{
    int sw = plat_screen_w(), i, n = sw / 26 + 1, px = -4, py = y;
    for (i = 0; i < n; i++) {
        int nx = i * 26 + 10, ny = y + ((i & 1) ? 6 : 2);
        gg_line(px, py, (px + nx) / 2, DOOMO_MAX(py, ny) + 3, PAL_RAMP(RAMP_WOOD, 2));
        gg_line((px + nx) / 2, DOOMO_MAX(py, ny) + 3, nx, ny, PAL_RAMP(RAMP_WOOD, 2));
        px = nx; py = ny;
    }
    gg_line(px, py, sw, y + 2, PAL_RAMP(RAMP_WOOD, 2));
    for (i = 0; i < n; i++) hang(i * 26 + 10 + ui_wave((int)t + i * 13, 50, 1), y + ((i & 1) ? 6 : 2), r, ctx);
}

static void hang_direct(int x, int y, int r, void *ctx) { (void)ctx; gg_lantern(x, y, r); }
typedef struct { const DoomoImage *im; int ox, oy; } HangBlit;
static void hang_blit(int x, int y, int r, void *ctx)
{
    const HangBlit *h = (const HangBlit *)ctx;
    (void)r;
    plat_image(x + h->ox, y + h->oy, h->im, 1, 0, 0);
}

void gg_lanterns(int y, uint32_t t)
{
    lantern_string(y, t, hang_direct, plat_screen_h() >= 200 ? 5 : 4, NULL);
}

void gg_lantern_string(int y, int r, uint32_t t, const DoomoImage *lantern, int ox, int oy)
{
    HangBlit h;
    h.im = lantern; h.ox = ox; h.oy = oy;
    lantern_string(y, t, hang_blit, r, &h);
}

void gg_hall(int floor_y, int mood, uint32_t t)
{
    static const uint8_t dusk[4] = { PAL_RAMP(RAMP_VAPE, 0), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_LAVA, 1), PAL_RAMP(RAMP_STONE, 3) };
    static const uint8_t night[4] = { PAL_RAMP(RAMP_VAPE, 0), PAL_RAMP(RAMP_VAPE, 0), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_STONE, 2) };
    static const uint8_t dawn[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_CANDY, 6), PAL_RAMP(RAMP_CANDY, 7) };
    const uint8_t *c = mood == GG_HALL_NIGHT ? night : mood == GG_HALL_DAWN ? dawn : dusk;
    int sw = plat_screen_w(), sh = plat_screen_h(), i, bh = floor_y / 4, y = 0;
    for (i = 0; i < 4; i++) {                                       /* four bands, a 2-row dither seam between them */
        int h = i == 3 ? floor_y - y : bh;
        plat_rect(0, y, sw, h, c[i]);
        if (i) plat_checker(0, y - 1, sw, 2, c[i - 1]);
        y += h;
    }
    if (mood == GG_HALL_NIGHT) {
        int mx = sw - 46, my = floor_y / 3;
        for (i = 0; i < 9; i++) ui_sparkle((i * 47 + 9) % sw, 6 + (i * 23) % (floor_y / 2), (int)((t / 3 + (uint32_t)i * 3) & 15), PAL_WHITE);
        gg_ellipse(mx, my, 15, 15, PAL_RAMP(RAMP_BONE, 7));
        gg_ellipse(mx + 6, my - 4, 13, 13, c[0]);
    } else if (mood == GG_HALL_DAWN) {
        gg_ellipse(sw - 70, floor_y - 2, 24, 24, PAL_UI_GOLD);
        gg_ellipse(sw - 70, floor_y - 2, 18, 18, PAL_RAMP(RAMP_CANDY, 7));
    }
    plat_rect(0, floor_y, sw, sh - floor_y, mood == GG_HALL_NIGHT ? PAL_RAMP(RAMP_STONE, 1) : mood == GG_HALL_DAWN ? PAL_RAMP(RAMP_GRASS, 4) : PAL_RAMP(RAMP_STONE, 2));
    plat_rect(0, floor_y, sw, 1, mood == GG_HALL_NIGHT ? PAL_RAMP(RAMP_VAPE, 3) : mood == GG_HALL_DAWN ? PAL_RAMP(RAMP_GRASS, 6) : PAL_RAMP(RAMP_LAVA, 3));
    plat_checker(0, floor_y + 1, sw, 3, mood == GG_HALL_NIGHT ? PAL_RAMP(RAMP_STONE, 2) : mood == GG_HALL_DAWN ? PAL_RAMP(RAMP_GRASS, 5) : PAL_RAMP(RAMP_STONE, 3));
}

void gg_table(int x, int y, int w, int n, uint32_t t)
{
    int i, s = plat_screen_h() >= 200 ? 2 : 1;
    gg_rect(x + 3, y + 5 * s, 4 * s, 12 * s, PAL_RAMP(RAMP_WOOD, 1));
    gg_rect(x + w - 3 - 4 * s, y + 5 * s, 4 * s, 12 * s, PAL_RAMP(RAMP_WOOD, 1));
    gg_rect(x, y, w, 5 * s, PAL_RAMP(RAMP_WOOD, 3));
    gg_rect(x, y, w, 1, PAL_RAMP(RAMP_WOOD, 5));
    gg_rect(x, y + 5 * s - 1, w, 1, PAL_RAMP(RAMP_WOOD, 0));
    gg_rect(x, y, 1, 5 * s, PAL_RAMP(RAMP_WOOD, 0));
    gg_rect(x + w - 1, y, 1, 5 * s, PAL_RAMP(RAMP_WOOD, 0));
    gg_rect(x + 2, y - 2, w - 4, 2, PAL_UI_CREAM);                /* the tablecloth's edge */
    for (i = 0; i < n; i++) bagel(x + 8 * s + i * 10 * s + (i & 1) * 2, y - 3 * s + ((i + (int)(t / 64)) % 2), s);
}

void gg_chair(int x, int y, int w, int tilt)
{
    int s = plat_screen_h() >= 200 ? 2 : 1;
    gg_rect(x + 2, y + 2 * s + tilt, 3 * s, 10 * s - tilt, PAL_RAMP(RAMP_WOOD, 1));
    gg_rect(x + w - 2 - 3 * s, y + 2 * s + tilt, 3 * s, 10 * s - tilt, PAL_RAMP(RAMP_WOOD, 1));
    gg_rect(x, y + tilt, w, 3 * s, PAL_RAMP(RAMP_WOOD, 3));
    gg_rect(x, y + tilt, w, 1, PAL_RAMP(RAMP_WOOD, 5));
    gg_rect(x, y - 14 * s + tilt * 2, 3 * s, 14 * s, PAL_RAMP(RAMP_WOOD, 2));     /* the back */
    gg_rect(x, y - 14 * s + tilt * 2, 3 * s, 1, PAL_RAMP(RAMP_WOOD, 5));
    gg_rect(x + 3 * s, y - 12 * s + tilt * 2, 2 * s, 2 * s, PAL_RAMP(RAMP_WOOD, 2));
}

void gg_crowd(const int16_t *ids, int n, int x0, int y, int dx, int dir, int scale, int mood, uint32_t t)
{
    int i;
    for (i = 0; i < n; i++) {
        int frame = mood == GG_CROWD_CHEER ? (int)((t / 6 + (uint32_t)i) & 3) : 0;
        const DoomoImage *b = bodyspr_image(ids[i], dir, frame);
        int x = x0 + i * dx, hop = 0, sway = 0;
        if (mood == GG_CROWD_CHEER) { hop = ui_wave((int)t * 2 + i * 11, 8, 4); hop = hop < 0 ? -hop : hop; }
        if (mood == GG_CROWD_SWOON) sway = ui_wave((int)t + i * 7, 30, 2);
        if (mood == GG_CROWD_SHOCK) hop = ((t / 3 + (uint32_t)i) & 1) ? 1 : 0;
        if (mood == GG_CROWD_SLEEP) { hop = -6 * scale; }
        if (!b) continue;
        gg_ellipse(x, y - 1, b->w * scale / 3 + 2, 2, PAL_RAMP(RAMP_STONE, 1));
        plat_image(x - b->w * scale / 2 + sway, y - b->h * scale + 2 * scale - hop, b, scale, 0, 0);
        switch (mood) {
        case GG_CROWD_SHOCK:
            if (((t / 5 + (uint32_t)i) & 3) == 0) ui_text_outline(x - 2, y - b->h * scale - 10, "!", PAL_UI_GOLD, PAL_BLACK);
            break;
        case GG_CROWD_SWOON:
            if (i & 1) ui_heart(x + 6, y - b->h * scale - 4 - (int)((t / 2 + (uint32_t)i * 5) % 16u), 2, PAL_UI_PINK);
            else {
                int fl = ui_wave((int)t * 2 + i * 3, 8, 3);         /* fanning themselves */
                plat_line(x + 6, y - b->h * scale + 8, x + 13, y - b->h * scale + 3 + fl, PAL_WHITE);
                plat_line(x + 6, y - b->h * scale + 8, x + 14, y - b->h * scale + 9 + fl, PAL_UI_GOLD);
            }
            break;
        case GG_CROWD_CHEER:
            if (((t / 4 + (uint32_t)i) & 3) == 0) ui_sparkle(x + 8, y - b->h * scale - 6 - (i & 1) * 4, (int)((t + (uint32_t)i * 3) & 15), PAL_UI_GOLD);
            break;
        case GG_CROWD_SLEEP:
            if (((t / 20u) + (uint32_t)i) % 4u == 0) ui_text_outline(x + 6, y - b->h * scale - 10 + (int)(t / 4 % 6u), "z", PAL_WHITE, PAL_BLACK);
            break;
        default: break;
        }
    }
}

void gg_gas(int cx, int cy, int r, int cute, uint32_t t)
{
    uint8_t g = cute ? PAL_RAMP(RAMP_MINT, 5) : PAL_RAMP(RAMP_SICK, 3);
    uint8_t l = cute ? PAL_RAMP(RAMP_MINT, 7) : PAL_RAMP(RAMP_SICK, 5);
    int w = ui_wave((int)t * 3, 20, 1);
    gg_ellipse(cx - r, cy + 2 + w, r * 2 / 3 + 1, r * 2 / 3 + 1, cute ? PAL_RAMP(RAMP_MINT, 3) : PAL_RAMP(RAMP_SICK, 1));
    gg_ellipse(cx + r, cy + 2 - w, r * 2 / 3 + 1, r * 2 / 3 + 1, cute ? PAL_RAMP(RAMP_MINT, 3) : PAL_RAMP(RAMP_SICK, 1));
    gg_ellipse(cx, cy, r + 1, r + 1, cute ? PAL_RAMP(RAMP_MINT, 3) : PAL_RAMP(RAMP_SICK, 1));
    gg_ellipse(cx - r, cy + 2 + w, r * 2 / 3, r * 2 / 3, g);
    gg_ellipse(cx + r, cy + 2 - w, r * 2 / 3, r * 2 / 3, g);
    gg_ellipse(cx, cy, r, r, g);
    gg_ellipse(cx - r / 3, cy - r / 3, r / 2, r / 3 + 1, l);
    if (cute) {
        ui_heart(cx - 2, cy - 2, 2, PAL_RAMP(RAMP_CANDY, 6));
        ui_sparkle(cx + r / 2, cy - r / 2, (int)((t / 2) & 15), PAL_WHITE);
    } else {
        int k;
        for (k = 0; k < 3; k++) {                                     /* flies */
            int a = (int)t * 60 + k * 1365, fx = cx + FIX2INT(fx_cos(a) * (r + 4)), fy = cy + FIX2INT(fx_sin(a) * (r / 2 + 2));
            plat_rect(fx, fy, 3, 2, PAL_BLACK);
            plat_rect(fx - 1, fy - 2, 2, 2, PAL_WHITE);
            plat_rect(fx + 2, fy - 2, 2, 2, PAL_WHITE);
        }
        plat_rect(cx + 1, cy - 1, 1, 1, PAL_BLACK);
        plat_rect(cx - r / 2, cy + 1, 1, 1, PAL_BLACK);
    }
}

void gg_bubble(int x, int y, int w, const char *txt, int tail)
{
    int tw = ui_text_width(txt) + 8;
    if (w < tw) w = tw;
    ui_round_rect(x + 1, y + 2, w, 13, 4, PAL_RAMP(RAMP_BLOOD, 0));
    ui_round_rect(x, y, w, 13, 4, PAL_UI_CREAM);
    ui_frame(x, y, w, 13, GG_LINE);
    if (tail < 0) {
        gg_tri(x + 8, y + 12, x + 16, y + 12, x + 6, y + 18, GG_LINE);
        gg_tri(x + 9, y + 11, x + 15, y + 11, x + 8, y + 16, PAL_UI_CREAM);
    } else {
        gg_tri(x + w - 16, y + 12, x + w - 8, y + 12, x + w - 6, y + 18, GG_LINE);
        gg_tri(x + w - 15, y + 11, x + w - 9, y + 11, x + w - 8, y + 16, PAL_UI_CREAM);
    }
    ui_text(x + 4, y + 3, txt, PAL_BLACK);
}

void gg_hand(int x, int y, int s, int dir, uint8_t col)
{
    int f;
    uint8_t d = ui_darker(col), o = ui_darker(d);
    gg_ellipse(x, y, 5 * s + 1, 4 * s + 1, o);
    gg_ellipse(x, y, 5 * s, 4 * s, col);
    gg_ellipse(x - s, y - s, 3 * s, 2 * s, ui_lighter(col));
    for (f = 0; f < 4; f++) {
        int fx, fy, rx, ry;
        if (dir == 0) { fx = x - 4 * s + f * 3 * s - s; fy = y - 4 * s - (f == 1 || f == 2 ? s : 0); rx = s + 1; ry = 3 * s; }
        else { fx = x + dir * 5 * s; fy = y - 3 * s + f * 2 * s; rx = 3 * s; ry = s + 1; }
        gg_ellipse(fx, fy, rx + 1, ry + 1, o);
        gg_ellipse(fx, fy, rx, ry, col);
        gg_ellipse(fx - (dir ? 0 : 1), fy - (dir ? 1 : 0), DOOMO_MAX(rx - 1, 1), DOOMO_MAX(ry - 1, 1), ui_lighter(col));
    }
    if (dir == 0) { gg_ellipse(x + 6 * s, y + s, 2 * s + 1, 3 * s + 1, o); gg_ellipse(x + 6 * s, y + s, 2 * s, 3 * s, col); }
    else { gg_ellipse(x + dir * 2 * s, y - 5 * s, 2 * s + 1, 2 * s + 1, o); gg_ellipse(x + dir * 2 * s, y - 5 * s, 2 * s, 2 * s, col); }
}

void gg_cheeks_closeup(int cx, int cy, int r, int sq_l, int sq_r, int jig, int belt_edge)
{
    int sw = plat_screen_w(), big = plat_screen_h() >= 200, s = big ? 2 : 1;
    int lx = cx - r * 3 / 4 + sq_l, rx = cx + r * 3 / 4 - sq_r, top = cy - r;
    gg_blob(lx, cy + jig, r - sq_l, r + sq_l / 3, RAMP_BLOOD, big ? 2 : 0);
    gg_blob(rx, cy - jig, r - sq_r, r + sq_r / 3, RAMP_BLOOD, big ? 2 : 0);
    plat_rect(cx, cy - r * 2 / 3, 1, r + r * 2 / 3, GG_LINE);
    plat_rect(cx + 1, cy - r * 2 / 3, 1, r + r * 2 / 3, SKIN_D);
    if (belt_edge) {                                                /* the belt edge along the top */
        int by = top - 12 * s, bh = 10 * s;
        plat_rect(0, by, sw, bh, GG_LEATH);
        plat_rect(0, by, sw, 1, GG_LINE);
        plat_rect(0, by + bh - 1, sw, 1, GG_LINE);
        plat_rect(0, by + 1, sw, 1, PAL_RAMP(RAMP_WOOD, 2));
        gg_studs(12, by + bh / 2 - s, sw - 12, by + bh / 2 - s, sw / 24, s);
    }
}
