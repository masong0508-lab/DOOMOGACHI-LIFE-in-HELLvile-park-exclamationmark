/* weld_gfx.c - first-person weapon sprites and menu icons, composed at
 * runtime from the welded parts (owner: action).
 *
 * The gun is seen from behind and above, pointing up toward the centre of
 * the screen. Every part is a shaded primitive laid out along that axis
 * ("t" 0..256 from the near end to the far end, "s" sideways), with
 * perspective (parts narrow toward the far end): pill-shaped barrels,
 * boxy receivers, round drums and scopes, cones, spikes, stickers. Each
 * primitive is lit from the upper left with 4-5 tones of its material
 * (hue-shifted paint / skin shades or environment ramps), a specular
 * stripe on cylinders, darker edges toward the shadow side, and the whole
 * silhouette gets a coloured outline (a dark shade of the local colour).
 *
 *   CUTE   pastel toy plastic in the two paints, rounded caps, a little
 *          smiley face, heart and star stickers, a white muzzle cap
 *   BRUTAL gunmetal + the paints as grips and stripes, bone spikes,
 *          rivets, a skull decal and dried blood
 *
 * Frames: 0 idle, 1 fire (kicked back, slide/bolt open), 2 fire2 (smoke),
 * 3 reload (tilted, magazine out). Results are cached in two 96x64 RAM
 * slots (and six 24x16 icon slots) keyed by every part, so the HUD only
 * pays for a composition when you switch weapons.
 */
#include "doomo/weld.h"
#include "doomo/palette.h"
#include "doomo/fixmath.h"
#include "doomo/rng.h"

#define VW WELD_VIEW_W
#define VH WELD_VIEW_H
#define IW WELD_ICON_W
#define IH WELD_ICON_H
#define VIEW_SLOTS 2
#define ICON_SLOTS 6
#define VIEW_WSC 390          /* width scale of the first-person sprite */

static uint8_t   view_pix[VIEW_SLOTS][VW * VH];
static DoomoImage view_img[VIEW_SLOTS];
static uint32_t  view_key[VIEW_SLOTS], view_used[VIEW_SLOTS];
static uint8_t   icon_pix[ICON_SLOTS][IW * IH];
static DoomoImage icon_img[ICON_SLOTS];
static uint32_t  icon_key[ICON_SLOTS], icon_used[ICON_SLOTS];
static uint32_t  use_clock;

/* ---- materials ---------------------------------------------------------- */
typedef struct Mat { uint8_t c[5]; } Mat;   /* 0 darkest .. 4 highlight */

static Mat mat_paint(int p)
{
    Mat m;
    p &= 31;
    m.c[1] = PAL_PAINT(p, 0);
    m.c[2] = PAL_PAINT(p, 1);
    m.c[3] = PAL_PAINT(p, 2);
    m.c[0] = palette_darker(m.c[1]);
    m.c[4] = palette_lighter(m.c[3]);
    if (m.c[4] == m.c[3]) m.c[4] = PAL_WHITE;
    return m;
}

static Mat mat_ramp(int r, int lo)
{
    Mat m;
    int i;
    for (i = 0; i < 5; i++) m.c[i] = PAL_RAMP(r, DOOMO_CLAMP(lo + i, 0, 7));
    return m;
}

/* ---- the drawing context ------------------------------------------------ */
typedef struct G {
    uint8_t *px;
    int w, h;
    int ax, ay;          /* axis start (near end), pixels x16 */
    int ux, uy;          /* unit axis direction x256 */
    int vx, vy;          /* unit perpendicular (to the right) x256 */
    int len;             /* axis length, pixels x16 */
    int persp;           /* width lost at t = 256, x256 */
    int wsc;             /* width scale x256 */
    int cute;
    Mat paint, accent, metal, dark, glow, white, wood, bone, gold, black;
    uint32_t seed;
} G;

static int isqrt_i(int v) { return v > 0 ? (int)fx_isqrt((uint32_t)v) : 0; }

static void put(G *g, int x, int y, uint8_t c)
{
    if (c == PAL_BLACK) c = PAL_RAMP(RAMP_STONE, 0);       /* 1 is the outline marker */
    if ((unsigned)x < (unsigned)g->w && (unsigned)y < (unsigned)g->h && c) g->px[y * g->w + x] = c;
}

static void clear_px(G *g, int x, int y)
{
    if ((unsigned)x < (unsigned)g->w && (unsigned)y < (unsigned)g->h) g->px[y * g->w + x] = 0;
}

static uint8_t get(const G *g, int x, int y)
{
    if ((unsigned)x < (unsigned)g->w && (unsigned)y < (unsigned)g->h) return g->px[y * g->w + x];
    return 0;
}

static void set_axis(G *g, int x0, int y0, int x1, int y1, int persp, int wsc)
{
    int dx = x1 - x0, dy = y1 - y0;
    int l = isqrt_i(dx * dx + dy * dy);
    if (l < 1) l = 1;
    g->ax = x0 * 16;
    g->ay = y0 * 16;
    g->ux = dx * 256 / l;
    g->uy = dy * 256 / l;
    g->vx = -g->uy;              /* rotate the axis 90 degrees clockwise on screen */
    g->vy = g->ux;
    if (g->vx < 0 || (g->vx == 0 && g->vy > 0)) { g->vx = -g->vx; g->vy = -g->vy; }
    g->len = l * 16;
    g->persp = persp;
    g->wsc = wsc;
}

/* perspective scale at t (x256) */
static int pscale(const G *g, int t) { return (256 - g->persp * t / 256) * g->wsc / 256; }

/* screen position of (t, s px) */
static void at(const G *g, int t, int s, int *x, int *y)
{
    int al = g->len * t / 256;                       /* x16 */
    int sd = s * 16 * pscale(g, t) / 256;            /* x16 */
    *x = (g->ax + al * g->ux / 256 + sd * g->vx / 256) >> 4;
    *y = (g->ay + al * g->uy / 256 + sd * g->vy / 256) >> 4;
}

enum { SH_CYL, SH_BOX, SH_GLOW, SH_FLAT };
enum { CAP_NONE = 0, CAP_NEAR = 1, CAP_FAR = 2 };

static uint8_t tone_cyl(const Mat *m, int u)          /* u -256..256, lit from the left */
{
    if (u < -200) return m->c[2];
    if (u < -130) return m->c[4];
    if (u < -40) return m->c[3];
    if (u < 90) return m->c[2];
    if (u < 190) return m->c[1];
    return m->c[0];
}

static uint8_t tone_box(const Mat *m, int u, int near_end, int far_end)
{
    /* a box seen from above-behind, lit from the upper left: a bevelled
     * top face (light), a glossy edge, and the right side face in shade */
    if (u > 150) return u > 225 ? m->c[0] : m->c[1];
    if (far_end) return m->c[3];
    if (near_end) return m->c[1];
    if (u < -205) return m->c[2];
    if (u < -150) return m->c[4];
    if (u < -60) return m->c[3];
    if (u > 110) return m->c[2];
    return m->c[2];
}

static uint8_t tone_glow(const Mat *m, int u)
{
    int a = u < 0 ? -u : u;
    if (a < 70) return m->c[4];
    if (a < 150) return m->c[3];
    if (a < 220) return m->c[2];
    return m->c[1];
}

/* A segment along the axis from t0 to t1, centred s px to the side, half
 * widths w0 -> w1 (px x16 so small parts stay smooth). */
static void seg(G *g, int t0, int t1, int s16, int w0, int w1, const Mat *m, int shade, int caps)
{
    int x0, y0, x1, y1, x2, y2, x3, y3, minx, maxx, miny, maxy, x, y;
    int lt0 = g->len * t0 / 256, lt1 = g->len * t1 / 256;
    int wmax = DOOMO_MAX(w0, w1);
    if (t1 <= t0) return;
    at(g, t0, s16 / 16 - wmax / 16 - 1, &x0, &y0);
    at(g, t0, s16 / 16 + wmax / 16 + 1, &x1, &y1);
    at(g, t1, s16 / 16 - wmax / 16 - 1, &x2, &y2);
    at(g, t1, s16 / 16 + wmax / 16 + 1, &x3, &y3);
    minx = DOOMO_MIN(DOOMO_MIN(x0, x1), DOOMO_MIN(x2, x3)) - 1;
    maxx = DOOMO_MAX(DOOMO_MAX(x0, x1), DOOMO_MAX(x2, x3)) + 1;
    miny = DOOMO_MIN(DOOMO_MIN(y0, y1), DOOMO_MIN(y2, y3)) - 1;
    maxy = DOOMO_MAX(DOOMO_MAX(y0, y1), DOOMO_MAX(y2, y3)) + 1;
    minx = DOOMO_MAX(minx, 0); miny = DOOMO_MAX(miny, 0);
    maxx = DOOMO_MIN(maxx, g->w - 1); maxy = DOOMO_MIN(maxy, g->h - 1);
    for (y = miny; y <= maxy; y++) {
        for (x = minx; x <= maxx; x++) {
            int rx = x * 16 + 8 - g->ax, ry = y * 16 + 8 - g->ay;
            int al = (rx * g->ux + ry * g->uy) / 256;          /* along, x16 */
            int sd = (rx * g->vx + ry * g->vy) / 256;          /* side, x16 */
            int t, ps, hw, sc, u, e0, e1;
            if (al < lt0 || al > lt1) continue;
            t = g->len ? al * 256 / g->len : 0;
            ps = pscale(g, t);
            hw = (w0 + (w1 - w0) * (al - lt0) / DOOMO_MAX(lt1 - lt0, 1)) * ps / 256;
            sc = s16 * ps / 256;
            if (hw < 6) hw = 6;
            /* rounded caps */
            e0 = al - lt0;
            e1 = lt1 - al;
            if ((caps & CAP_NEAR) && e0 < hw) hw = isqrt_i(hw * hw - (hw - e0) * (hw - e0));
            if ((caps & CAP_FAR) && e1 < hw) hw = isqrt_i(hw * hw - (hw - e1) * (hw - e1));
            if (sd < sc - hw || sd > sc + hw || hw <= 0) continue;
            u = (sd - sc) * 256 / hw;
            switch (shade) {
            case SH_CYL: put(g, x, y, tone_cyl(m, u)); break;
            case SH_BOX: put(g, x, y, tone_box(m, u, e0 < 16, e1 < 16)); break;
            case SH_GLOW: put(g, x, y, tone_glow(m, u)); break;
            default: put(g, x, y, m->c[2]); break;
            }
        }
    }
}

/* Shaded ellipse in screen space (centre x16, radii x16), lit top-left. */
static void ell(G *g, int cx16, int cy16, int rx16, int ry16, const Mat *m, int glow)
{
    int x, y;
    int x0 = (cx16 - rx16) >> 4, x1 = (cx16 + rx16) >> 4, y0 = (cy16 - ry16) >> 4, y1 = (cy16 + ry16) >> 4;
    if (rx16 < 8 || ry16 < 8) return;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            int dx = (x * 16 + 8 - cx16) * 256 / rx16, dy = (y * 16 + 8 - cy16) * 256 / ry16;
            int d2 = dx * dx + dy * dy, l;
            if (d2 > 65536) continue;
            if (glow) { put(g, x, y, tone_glow(m, isqrt_i(d2))); continue; }
            l = -dx - dy;                                  /* light from the upper left */
            if (d2 > 52000 && l < 0) put(g, x, y, m->c[0]);
            else if (l > 200 && d2 < 30000 && d2 > 4000) put(g, x, y, m->c[4]);
            else if (l > 60) put(g, x, y, m->c[3]);
            else if (l > -150) put(g, x, y, m->c[2]);
            else put(g, x, y, m->c[1]);
        }
}

static void ell_at(G *g, int t, int s, int r, const Mat *m, int glow)
{
    int x, y, rr = r * pscale(g, t) / 16;
    at(g, t, s, &x, &y);
    ell(g, x * 16 + 8, y * 16 + 8, DOOMO_MAX(rr, 12), DOOMO_MAX(rr, 12), m, glow);
}

static void tri(G *g, int ax, int ay, int bx, int by, int cx, int cy, uint8_t c)
{
    int minx = DOOMO_MIN(ax, DOOMO_MIN(bx, cx)), maxx = DOOMO_MAX(ax, DOOMO_MAX(bx, cx));
    int miny = DOOMO_MIN(ay, DOOMO_MIN(by, cy)), maxy = DOOMO_MAX(ay, DOOMO_MAX(by, cy));
    int x, y;
    for (y = miny; y <= maxy; y++)
        for (x = minx; x <= maxx; x++) {
            int e0 = (bx - ax) * (y - ay) - (by - ay) * (x - ax);
            int e1 = (cx - bx) * (y - by) - (cy - by) * (x - bx);
            int e2 = (ax - cx) * (y - cy) - (ay - cy) * (x - cx);
            if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0)) put(g, x, y, c);
        }
}

static void line(G *g, int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = x1 - x0, dy = y1 - y0, n = DOOMO_MAX(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy), i;
    if (n == 0) { put(g, x0, y0, c); return; }
    for (i = 0; i <= n; i++) put(g, x0 + dx * i / n, y0 + dy * i / n, c);
}

/* little stamped decals */
static void decal_heart(G *g, int x, int y, uint8_t c, uint8_t hi)
{
    static const char *const h[4] = { ".X.X.", "XXXXX", ".XXX.", "..X.." };
    int i, j;
    for (j = 0; j < 4; j++) for (i = 0; i < 5; i++) if (h[j][i] == 'X') put(g, x + i - 2, y + j - 1, c);
    put(g, x - 1, y - 1, hi);
}

static void decal_star(G *g, int x, int y, uint8_t c, uint8_t hi)
{
    static const char *const s[5] = { "..X..", ".XXX.", "XXXXX", ".X.X.", "X...X" };
    int i, j;
    for (j = 0; j < 5; j++) for (i = 0; i < 5; i++) if (s[j][i] == 'X') put(g, x + i - 2, y + j - 2, c);
    put(g, x, y - 1, hi);
}

static void decal_skull(G *g, int x, int y, uint8_t c, uint8_t dark)
{
    static const char *const s[5] = { ".XXX.", "XXXXX", "X.X.X", "XXXXX", ".X.X." };
    int i, j;
    for (j = 0; j < 5; j++) for (i = 0; i < 5; i++) {
        if (s[j][i] == 'X') put(g, x + i - 2, y + j - 2, c);
        else if (j == 2 && (i == 1 || i == 3)) put(g, x + i - 2, y + j - 2, dark);
    }
}

static void decal_face(G *g, int x, int y)
{
    /* the cute toy face: glossy dot eyes, blush, tiny smile */
    put(g, x - 3, y, PAL_PAINT(PAINT_JET, 1)); put(g, x - 3, y + 1, PAL_PAINT(PAINT_JET, 1));
    put(g, x + 3, y, PAL_PAINT(PAINT_JET, 1)); put(g, x + 3, y + 1, PAL_PAINT(PAINT_JET, 1));
    put(g, x - 3, y, PAL_WHITE); put(g, x + 3, y, PAL_WHITE);
    put(g, x - 5, y + 2, PAL_PAINT(PAINT_ROSE, 2)); put(g, x + 5, y + 2, PAL_PAINT(PAINT_ROSE, 2));
    put(g, x - 1, y + 2, PAL_PAINT(PAINT_JET, 1)); put(g, x, y + 3, PAL_PAINT(PAINT_JET, 1));
    put(g, x + 1, y + 2, PAL_PAINT(PAINT_JET, 1));
}

static void rivet(G *g, int t, int s)
{
    int x, y;
    at(g, t, s, &x, &y);
    put(g, x, y, g->metal.c[4]);
    put(g, x + 1, y + 1, g->metal.c[0]);
}

static void splat(G *g, int t, int s, uint32_t seed)
{
    int x, y, k;
    at(g, t, s, &x, &y);
    for (k = 0; k < 7; k++) {
        uint32_t h = doomo_hash32(seed + (uint32_t)k * 77u);
        int ox = (int)(h % 5) - 2, oy = (int)((h >> 4) % 5) - 2;
        if (get(g, x + ox, y + oy)) put(g, x + ox, y + oy, k < 2 ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_BLOOD, 4));
    }
}

/* Spikes along one side of a segment (brutal). */
static void spikes(G *g, int t0, int t1, int side_px, int n, int len)
{
    int i;
    for (i = 0; i < n; i++) {
        int t = t0 + (t1 - t0) * (2 * i + 1) / (2 * n);
        int bx0, by0, bx1, by1, tx, ty;
        at(g, t - 6, side_px, &bx0, &by0);
        at(g, t + 6, side_px, &bx1, &by1);
        at(g, t, side_px + (side_px < 0 ? -len : len), &tx, &ty);
        tri(g, bx0, by0, bx1, by1, tx, ty, g->bone.c[3]);
        put(g, tx, ty, g->bone.c[4]);
        line(g, bx1, by1, tx, ty, g->bone.c[1]);
    }
}

/* ---- the outline pass: coloured selective outlines --------------------- */
static void outline(G *g)
{
    int x, y, w = g->w, h = g->h;
    /* mark outline pixels with 1 then colour them (index 1 is PAL_BLACK and
     * never used inside the composition) */
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            uint8_t c;
            if (g->px[y * w + x]) continue;
            c = get(g, x + 1, y);
            if (!c || c == 1) c = get(g, x, y + 1);
            if (!c || c == 1) c = get(g, x - 1, y);
            if (!c || c == 1) c = get(g, x, y - 1);
            if (c && c != 1) g->px[y * w + x] = 1;
        }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            uint8_t c, d;
            if (g->px[y * w + x] != 1) continue;
            c = get(g, x + 1, y);
            if (!c || c == 1) c = get(g, x, y + 1);
            if (!c || c == 1) c = get(g, x - 1, y);
            if (!c || c == 1) c = get(g, x, y - 1);
            if (!c || c == 1) { g->px[y * w + x] = 0; continue; }
            d = palette_darker(palette_darker(c));
            if (d == c || d == 0 || d == PAL_BLACK) d = PAL_RAMP(RAMP_STONE, 0);
            g->px[y * w + x] = d;
        }
}

/* ---- recipes ---------------------------------------------------------------- */
typedef struct Recipe {
    uint8_t kind;          /* RK_* */
    uint8_t body0, body1;  /* receiver t range */
    uint8_t bw;            /* receiver half width px */
    uint8_t bar0, bar1;    /* barrel t range */
    uint8_t brw;           /* barrel half width (px x4) */
    uint8_t grip;          /* grip t */
    uint8_t fore0, fore1;  /* forend / handguard t range (fore1 = 0: none) */
    uint8_t forew;         /* forend half width px */
} Recipe;
enum { RK_GUN, RK_LAUNCHER, RK_MINIGUN, RK_SHOTGUN, RK_BLADE, RK_HAMMER, RK_SAW, RK_FISTS,
       RK_BOW, RK_XBOW, RK_WAND, RK_SPRAY, RK_DUAL, RK_BFG, RK_RAIL, RK_FLAME, RK_CONE };

static const Recipe recipes[WFRAME_COUNT] = {
    /* kind        body0 body1 bw  bar0 bar1 brw grip fore0 fore1 forew */
    { RK_GUN,       40, 196, 11, 186, 214, 20,  70,   0,   0,  0 },  /* pistol */
    { RK_GUN,       40, 150, 11, 140, 224, 22,  70,   0,   0,  0 },  /* revolver */
    { RK_DUAL,      60, 196,  9, 186, 212, 18,  80,   0,   0,  0 },  /* dual */
    { RK_GUN,       40, 190, 11, 180, 212, 16,  70,   0,   0,  0 },  /* machine pistol */
    { RK_GUN,       20, 150, 12, 185, 226, 15,  80, 140, 192,  9 },  /* smg */
    { RK_GUN,        0, 140, 13, 195, 246, 14,  90, 130, 200, 10 },  /* ar */
    { RK_GUN,        0, 145, 14, 200, 248, 16,  90, 135, 206, 11 },  /* battle */
    { RK_GUN,        0, 170, 14, 200, 242, 14, 120, 164, 206, 10 },  /* bullpup */
    { RK_GUN,        0, 150, 16, 196, 250, 18,  90, 140, 202, 12 },  /* lmg */
    { RK_MINIGUN,    0, 120, 17, 110, 246, 20,  80,   0,   0,  0 },  /* minigun */
    { RK_GUN,        0, 140, 12, 190, 250, 13,  90, 130, 196,  9 },  /* dmr */
    { RK_GUN,        0, 135, 12, 176, 254, 13,  85, 125, 182,  8 },  /* sniper */
    { RK_RAIL,       0, 140, 14, 130, 252, 12,  85,   0,   0,  0 },  /* railgun */
    { RK_XBOW,       0, 180,  9, 170, 236,  8,  80,   0,   0,  0 },  /* crossbow */
    { RK_BOW,        0,   0,  0,   0,   0,  0, 120,   0,   0,  0 },  /* bow */
    { RK_SHOTGUN,    0, 130, 13, 120, 234, 21,  80, 140, 182, 11 },  /* pump */
    { RK_SHOTGUN,    0, 110, 15, 100, 222, 19,  70,   0,   0,  0 },  /* super */
    { RK_SHOTGUN,    0, 145, 14, 140, 234, 21,  85, 140, 186, 11 },  /* auto shotgun */
    { RK_SHOTGUN,    0, 130, 12, 120, 238, 19,  80, 140, 176, 10 },  /* lever */
    { RK_LAUNCHER,   0, 120, 14,  90, 216, 32,  70,   0,   0,  0 },  /* grenade */
    { RK_LAUNCHER,   0, 110, 13,  36, 250, 36,  80,   0,   0,  0 },  /* rocket */
    { RK_FLAME,      0, 140, 13, 176, 230, 17,  80, 130, 182, 10 },  /* flamethrower */
    { RK_GUN,        0, 165, 15, 200, 234, 17,  90, 155, 206, 11 },  /* plasma */
    { RK_GUN,        0, 150, 14, 185, 226, 15,  85, 140, 190, 10 },  /* nailgun */
    { RK_BFG,        0, 150, 21, 140, 222, 28,  80,   0,   0,  0 },  /* bfg */
    { RK_SAW,        0, 110, 15, 100, 252, 14,  60,   0,   0,  0 },  /* chainsaw */
    { RK_BLADE,      0,  60,  4,  60, 252,  7,  40,   0,   0,  0 },  /* katana */
    { RK_HAMMER,     0, 200,  4, 190, 250,  0,  60,   0,   0,  0 },  /* sledgehammer */
    { RK_FISTS,      0,   0,  0,   0,   0,  0,   0,   0,   0,  0 },  /* fists */
    { RK_WAND,       0, 190,  3, 190, 250,  0,  60,   0,   0,  0 },  /* bubble wand */
    { RK_LAUNCHER,   0, 130, 14,  80, 228, 30,  75,   0,   0,  0 },  /* plush launcher */
    { RK_CONE,       0, 120, 13, 100, 232, 30,  70,   0,   0,  0 },  /* confetti cannon */
    { RK_HAMMER,     0, 200,  3, 190, 250,  1,  60,   0,   0,  0 },  /* lollipop */
    { RK_BOW,        0,   0,  0,   0,   0,  1, 120,   0,   0,  0 },  /* heart bow */
    { RK_HAMMER,     0, 200,  4, 185, 250,  2,  60,   0,   0,  0 },  /* squeaky mallet */
    { RK_SPRAY,      0, 150, 13, 150, 218, 10,  80,   0,   0,  0 },  /* glitter sprayer */
    { RK_SPRAY,      0, 140, 13, 140, 214, 10,  75,   0,   0,  0 },  /* water pistol */
    { RK_GUN,       20, 150, 13, 140, 212, 17,  75,   0,   0,  0 },  /* sticker gun */
    { RK_XBOW,       0, 160,  9, 150, 232,  8,  80,   0,   0,  0 },  /* yarn crossbow */
};

typedef struct Pose { int x0, y0, x1, y1; int dropmag; } Pose;

static void core_canister(G *g, const DoomoWeapon *w, int t, int s)
{
    Mat m;
    int glow = 1;
    switch (w->core) {
    case WCORE_PLASMA: case WCORE_RAIL_SLUG: case WCORE_ICE: m = mat_paint(PAINT_CYAN); break;
    case WCORE_HELLFIRE: case WCORE_FUEL: m = mat_ramp(RAMP_LAVA, 3); break;
    case WCORE_HOLY_WATER: m = mat_paint(PAINT_SKY); break;
    case WCORE_SLIME: m = mat_ramp(RAMP_SICK, 2); break;
    case WCORE_LOVE: m = mat_paint(PAINT_BUBBLEGUM); break;
    case WCORE_GLITTER: m = mat_ramp(RAMP_CANDY, 3); break;
    case WCORE_BUBBLE: m = mat_paint(PAINT_SKY); glow = 0; break;
    case WCORE_CANDY: m = mat_paint(PAINT_ROSE); glow = 0; break;
    case WCORE_PLUSH: m = mat_paint(PAINT_CARAMEL); glow = 0; break;
    case WCORE_ROCKET: case WCORE_GRENADE: case WCORE_EXPLOSIVE_ROUND: m = mat_ramp(RAMP_LAVA, 2); glow = 0; break;
    default: return;                                    /* plain ammo: nothing to show */
    }
    seg(g, t, t + 28, s * 16, 40, 36, &g->dark, SH_CYL, CAP_NEAR | CAP_FAR);
    seg(g, t + 3, t + 25, s * 16, 28, 24, &m, glow ? SH_GLOW : SH_CYL, CAP_NEAR | CAP_FAR);
    if (w->core == WCORE_HOLY_WATER) {
        int x, y;
        at(g, t + 14, s, &x, &y);
        put(g, x, y - 1, PAL_WHITE); put(g, x, y, PAL_WHITE); put(g, x, y + 1, PAL_WHITE);
        put(g, x - 1, y, PAL_WHITE); put(g, x + 1, y, PAL_WHITE);
    }
    if (w->core == WCORE_CANDY) {
        int k, x, y;
        for (k = 0; k < 4; k++) { at(g, t + 5 + k * 6, s, &x, &y); put(g, x, y, PAL_WHITE); put(g, x + 1, y, PAL_WHITE); }
    }
}

static void sight_part(G *g, const DoomoWeapon *w, const Recipe *r)
{
    int t = r->body1 - 30;
    Mat lens = mat_paint(w->style == WSTYLE_CUTE ? PAINT_SKY : PAINT_CYAN);
    switch (w->sight) {
    case WSIGHT_IRON: {
        int x, y;
        at(g, r->bar1 - 4, 0, &x, &y);
        put(g, x, y - 1, g->metal.c[4]); put(g, x, y - 2, g->metal.c[3]);
        at(g, r->body1 - 6, -3, &x, &y);
        put(g, x, y, g->metal.c[3]); put(g, x, y - 1, g->metal.c[4]);
        at(g, r->body1 - 6, 3, &x, &y);
        put(g, x, y, g->metal.c[1]); put(g, x, y - 1, g->metal.c[2]);
        break;
    }
    case WSIGHT_RED_DOT: case WSIGHT_HEART_DOT: case WSIGHT_HOLO:
        seg(g, t, t + 22, 0, 52, 48, &g->dark, SH_BOX, 0);
        seg(g, t + 4, t + 18, 0, 36, 34, &lens, SH_GLOW, 0);
        {
            int x, y;
            at(g, t + 11, 0, &x, &y);
            if (w->sight == WSIGHT_HEART_DOT) decal_heart(g, x, y, PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE);
            else { put(g, x, y, PAL_UI_RED); put(g, x + 1, y, PAL_UI_RED); }
        }
        break;
    case WSIGHT_SCOPE_4X: case WSIGHT_SCOPE_8X: case WSIGHT_THERMAL: {
        int long8 = w->sight == WSIGHT_SCOPE_8X;
        int t0 = t - (long8 ? 30 : 16), t1 = t + (long8 ? 60 : 40);
        seg(g, t0, t1, 0, 60, 56, &g->dark, SH_CYL, CAP_NEAR | CAP_FAR);
        ell_at(g, t0 + 4, 0, 4, &g->dark, 0);
        ell_at(g, t0 + 4, 0, 3, w->sight == WSIGHT_THERMAL ? &g->glow : &lens, 1);
        if (w->sight == WSIGHT_THERMAL) seg(g, t + 6, t + 20, 72, 20, 20, &g->glow, SH_GLOW, 0);
        break;
    }
    case WSIGHT_LASER:
        seg(g, r->bar0 + 10, r->bar0 + 40, 7 * 16, 22, 20, &g->dark, SH_BOX, 0);
        {
            int x, y;
            at(g, r->bar0 + 40, 7, &x, &y);
            put(g, x, y, PAL_UI_RED);
            put(g, x, y - 1, PAL_RAMP(RAMP_BLOOD, 6));
        }
        break;
    default:
        break;
    }
}

static void charm_part(G *g, const DoomoWeapon *w, int t, int s)
{
    int x, y, hx, hy;
    if (w->charm == WCHARM_NONE) return;
    at(g, t, s, &x, &y);
    hx = x - 3;
    hy = y + 7;
    line(g, x, y, hx, hy - 3, PAL_RAMP(RAMP_STONE, 5));
    switch (w->charm) {
    case WCHARM_TEDDY: {
        Mat br = mat_paint(PAINT_CARAMEL);
        ell(g, hx * 16 + 8, hy * 16 + 8, 48, 44, &br, 0);
        ell(g, (hx - 3) * 16 + 8, (hy - 3) * 16 + 8, 18, 18, &br, 0);
        ell(g, (hx + 3) * 16 + 8, (hy - 3) * 16 + 8, 18, 18, &br, 0);
        put(g, hx - 1, hy, PAL_PAINT(PAINT_JET, 1)); put(g, hx + 1, hy, PAL_PAINT(PAINT_JET, 1));
        put(g, hx, hy + 1, PAL_PAINT(PAINT_CHOCOLATE, 0));
        break;
    }
    case WCHARM_SKULL: decal_skull(g, hx, hy, g->bone.c[3], g->bone.c[0]); break;
    case WCHARM_DICE:
        ell(g, hx * 16 + 8, hy * 16 + 8, 40, 40, &g->white, 0);
        put(g, hx - 1, hy - 1, PAL_PAINT(PAINT_JET, 1)); put(g, hx + 1, hy + 1, PAL_PAINT(PAINT_JET, 1));
        put(g, hx, hy, PAL_PAINT(PAINT_CHERRY, 1));
        break;
    case WCHARM_HEART: decal_heart(g, hx, hy, PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_ROSE, 2)); break;
    case WCHARM_EYEBALL:
        ell(g, hx * 16 + 8, hy * 16 + 8, 40, 40, &g->white, 0);
        put(g, hx, hy, PAL_PAINT(PAINT_COBALT, 1)); put(g, hx + 1, hy, PAL_PAINT(PAINT_JET, 1));
        break;
    case WCHARM_BELL: ell(g, hx * 16 + 8, hy * 16 + 8, 40, 44, &g->gold, 0); put(g, hx, hy + 2, g->gold.c[0]); break;
    case WCHARM_ROSARY:
        put(g, hx, hy - 2, g->wood.c[3]); put(g, hx, hy - 1, g->wood.c[3]); put(g, hx, hy, g->wood.c[3]);
        put(g, hx, hy + 1, g->wood.c[3]); put(g, hx - 1, hy - 1, g->wood.c[3]); put(g, hx + 1, hy - 1, g->wood.c[3]);
        break;
    default: break;
    }
}

static void mag_part(G *g, const DoomoWeapon *w, const Recipe *r, int drop)
{
    int t = (r->body0 + r->body1) / 2;
    int s = -(r->bw + 3);
    Mat *m = w->style == WSTYLE_CUTE ? &g->accent : &g->dark;
    if (drop) s -= 6;
    switch (w->mag) {
    case WMAG_DRUM:
        ell_at(g, t, s - 4, 9, m, 0);
        ell_at(g, t, s - 4, 4, &g->metal, 0);
        break;
    case WMAG_BELT: {
        int k;
        for (k = 0; k < 6; k++) seg(g, t - 30 + k * 10, t - 24 + k * 10, (s - 2) * 16, 24, 24, &g->gold, SH_CYL, CAP_FAR);
        break;
    }
    case WMAG_CANDY_TUBE: {
        Mat pink = mat_paint(PAINT_BUBBLEGUM);
        seg(g, t - 20, t + 30, (s - 1) * 16, 40, 38, &g->white, SH_CYL, CAP_NEAR | CAP_FAR);
        seg(g, t - 12, t - 4, (s - 1) * 16, 40, 38, &pink, SH_CYL, 0);
        seg(g, t + 6, t + 14, (s - 1) * 16, 40, 38, &pink, SH_CYL, 0);
        break;
    }
    case WMAG_EXTENDED:
        seg(g, t - 26, t + 12, s * 16, 48, 44, m, SH_BOX, 0);
        break;
    case WMAG_QUICK:
        seg(g, t - 16, t + 10, s * 16, 40, 38, m, SH_BOX, 0);
        seg(g, t - 18, t - 12, s * 16, 50, 50, &g->accent, SH_BOX, 0);
        break;
    default:
        seg(g, t - 16, t + 10, s * 16, 40, 38, m, SH_BOX, 0);
        break;
    }
}

static void under_part(G *g, const DoomoWeapon *w, const Recipe *r)
{
    int t0 = r->bar0 + 6, t1 = r->bar0 + (r->bar1 - r->bar0) * 2 / 3;
    int s = r->bw + 1;
    switch (w->under) {
    case WUNDER_GRIP:
        seg(g, r->bar0, r->bar0 + 24, (s + 1) * 16, 40, 36, &g->accent, SH_CYL, CAP_NEAR | CAP_FAR);
        break;
    case WUNDER_GRENADE:
        seg(g, t0, t1, (s + 2) * 16, 64, 58, &g->dark, SH_CYL, CAP_FAR);
        ell_at(g, t1 - 2, s + 2, 3, &g->black, 0);
        break;
    case WUNDER_SHOTGUN:
        seg(g, t0, t1 + 10, (s + 1) * 16, 44, 40, &g->metal, SH_CYL, CAP_FAR);
        seg(g, t0 + 8, t0 + 30, (s + 1) * 16, 52, 50, &g->wood, SH_BOX, 0);
        break;
    case WUNDER_FLASHLIGHT:
        seg(g, t0 + 10, t1, (s + 1) * 16, 40, 44, &g->dark, SH_CYL, 0);
        ell_at(g, t1, s + 1, 3, &g->glow, 1);
        break;
    case WUNDER_CHAINSAW: {
        int k, x, y;
        seg(g, t0, t1 + 20, (s + 2) * 16, 34, 30, &g->metal, SH_BOX, CAP_FAR);
        for (k = 0; k < 8; k++) { at(g, t0 + k * (t1 + 20 - t0) / 8, s + 5, &x, &y); put(g, x, y, g->metal.c[4]); }
        break;
    }
    case WUNDER_BUBBLE: {
        Mat sky = mat_paint(PAINT_SKY);
        seg(g, t0, t1 - 10, (s + 1) * 16, 30, 28, &g->white, SH_CYL, CAP_FAR);
        ell_at(g, t1, s + 2, 5, &sky, 0);
        ell_at(g, t1 + 4, s, 2, &g->white, 0);
        break;
    }
    default:
        break;
    }
}

static void barrel_part(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    int t0 = r->bar0, t1 = r->bar1, bw = r->brw * 4;           /* x16 */
    int n = 1, k, spacing = 0;
    const Mat *m = w->style == WSTYLE_CUTE ? &g->accent : &g->metal;
    switch (w->barrel) {
    case WBARREL_SHORT: t1 = t0 + (t1 - t0) * 6 / 10; break;
    case WBARREL_LONG: t1 = DOOMO_MIN(t1 + 20, 254); bw = bw * 9 / 10; break;
    case WBARREL_HEAVY: bw = bw * 3 / 2; break;
    case WBARREL_DOUBLE: n = 2; spacing = r->brw / 2 + 2; break;
    case WBARREL_TRIPLE: n = 3; spacing = r->brw / 2 + 1; bw = bw * 4 / 5; break;
    case WBARREL_WIDE: bw = bw * 5 / 4; break;
    case WBARREL_SAWED: t1 = t0 + (t1 - t0) / 2; bw = bw * 5 / 4; n = 2; spacing = r->brw / 2 + 1; break;
    default: break;
    }
    if (fire) t1 -= 4;
    for (k = 0; k < n; k++) {
        int s16 = (n == 1 ? 0 : (k * 2 - (n - 1)) * spacing * 8);
        int wide_end = w->barrel == WBARREL_WIDE ? bw * 2 : bw;
        seg(g, t0, t1, s16, bw, wide_end, m, SH_CYL, CAP_NONE);
        /* muzzle */
        if (w->barrel == WBARREL_SUPPRESSED) {
            seg(g, t1 - 30, t1 + 6, s16, bw * 3 / 2, bw * 3 / 2, &g->dark, SH_CYL, CAP_FAR);
        } else if (w->barrel == WBARREL_HEART && k == 0) {
            int x, y;
            at(g, t1, s16 / 16, &x, &y);
            decal_heart(g, x, y, PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_ROSE, 2));
        } else if (w->barrel == WBARREL_STAR && k == 0) {
            int x, y;
            at(g, t1, s16 / 16, &x, &y);
            decal_star(g, x, y, PAL_UI_GOLD, PAL_WHITE);
        } else {
            seg(g, t1 - 8, t1, s16, wide_end + 6, wide_end + 6,
                w->style == WSTYLE_CUTE ? &g->white : &g->dark, SH_CYL, w->style == WSTYLE_CUTE ? CAP_FAR : 0);
            {
                int x, y;
                at(g, t1 - 1, s16 / 16, &x, &y);
                put(g, x, y, PAL_RAMP(RAMP_STONE, 0));
            }
        }
        if (w->barrel == WBARREL_SPIRAL) {
            int j, x, y;
            for (j = t0 + 6; j < t1 - 8; j += 10) { at(g, j, s16 / 16 - 1, &x, &y); put(g, x, y, m->c[4]); }
        }
    }
}

static void mod_part(G *g, const DoomoWeapon *w, const Recipe *r)
{
    int x, y, k;
    switch (w->mod) {
    case WMOD_SPIKES:
        spikes(g, r->body0 + 10, r->body1, -(r->bw + 1), 3, 5);
        spikes(g, r->body0 + 10, r->body1, r->bw + 1, 3, 5);
        break;
    case WMOD_BAYONET:
        at(g, r->bar1 - 10, -2, &x, &y);
        {
            int tx, ty, bx, by;
            at(g, DOOMO_MIN(r->bar1 + 30, 255), -1, &tx, &ty);
            at(g, r->bar1 - 10, 0, &bx, &by);
            tri(g, x, y, bx, by, tx, ty, g->metal.c[3]);
            line(g, bx, by, tx, ty, g->metal.c[4]);
        }
        break;
    case WMOD_STICKERS:
        at(g, r->body0 + 30, -r->bw / 2, &x, &y); decal_star(g, x, y, PAL_UI_GOLD, PAL_WHITE);
        at(g, r->body0 + 50, r->bw / 2, &x, &y); decal_heart(g, x, y, PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_WHITE);
        break;
    case WMOD_COOLER:
        for (k = 0; k < 4; k++) seg(g, r->bar0 + 10 + k * 12, r->bar0 + 14 + k * 12, 0, r->brw * 6, r->brw * 6, &g->metal, SH_BOX, 0);
        break;
    case WMOD_OVERCLOCK:
        for (k = 0; k < 3; k++) seg(g, r->body0 + 20 + k * 16, r->body0 + 26 + k * 16, (r->bw - 2) * 16, 20, 20, &g->glow, SH_GLOW, 0);
        break;
    case WMOD_RICOCHET:
        seg(g, r->bar1 - 20, r->bar1 - 12, 0, r->brw * 7, r->brw * 7, &g->gold, SH_CYL, 0);
        break;
    case WMOD_HOMING:
        at(g, r->body1 - 10, r->bw + 2, &x, &y);
        line(g, x, y, x + 3, y - 8, g->metal.c[3]);
        ell(g, (x + 3) * 16 + 8, (y - 9) * 16 + 8, 24, 24, &g->glow, 1);
        break;
    case WMOD_BURST:
        for (k = 0; k < 3; k++) { at(g, r->body1 - 8, -4 + k * 4, &x, &y); put(g, x, y, g->glow.c[3]); }
        break;
    case WMOD_FULL_AUTO:
        seg(g, r->body0 + 30, r->body0 + 44, (r->bw + 2) * 16, 24, 24, &g->accent, SH_BOX, 0);
        break;
    case WMOD_CHARGE_SHOT:
        seg(g, r->body1 - 26, r->body1 - 6, -(r->bw - 1) * 16, 36, 36, &g->glow, SH_GLOW, CAP_NEAR | CAP_FAR);
        break;
    case WMOD_LIFESTEAL: {
        Mat red = mat_ramp(RAMP_BLOOD, 2);
        seg(g, r->body0 + 20, r->body0 + 50, (r->bw + 3) * 16, 30, 30, &g->white, SH_CYL, CAP_NEAR | CAP_FAR);
        seg(g, r->body0 + 26, r->body0 + 44, (r->bw + 3) * 16, 20, 20, &red, SH_GLOW, 0);
        break;
    }
    case WMOD_SQUEAKER: {
        Mat yel = mat_paint(PAINT_LEMON);
        ell_at(g, r->body0 + 20, -(r->bw + 3), 5, &yel, 0);
        break;
    }
    default: break;
    }
}

static void stock_part(G *g, const DoomoWeapon *w, const Recipe *r)
{
    int t1 = r->body0 + 30;
    Mat *m = w->style == WSTYLE_CUTE ? &g->paint : &g->wood;
    if (r->kind == RK_BLADE || r->kind == RK_HAMMER || r->kind == RK_WAND || r->kind == RK_BOW ||
        r->kind == RK_FISTS)
        return;
    switch (w->stock) {
    case WSTOCK_FIXED: seg(g, 0, t1, 0, (r->bw + 2) * 16, r->bw * 16, m, SH_BOX, 0); break;
    case WSTOCK_FOLDING:
        seg(g, 0, t1, -(r->bw) * 8, 20, 20, &g->metal, SH_CYL, 0);
        seg(g, 0, t1, (r->bw) * 8, 20, 20, &g->metal, SH_CYL, 0);
        break;
    case WSTOCK_HEAVY:
        seg(g, 0, t1, 0, (r->bw + 5) * 16, (r->bw + 2) * 16, w->style == WSTYLE_CUTE ? &g->accent : &g->dark, SH_BOX, 0);
        break;
    case WSTOCK_SKELETON:
        seg(g, 0, t1, -(r->bw) * 10, 18, 18, &g->metal, SH_CYL, 0);
        seg(g, 0, t1, (r->bw) * 10, 18, 18, &g->metal, SH_CYL, 0);
        seg(g, 4, 12, 0, (r->bw) * 12, (r->bw) * 12, &g->metal, SH_BOX, 0);
        break;
    case WSTOCK_PLUSH: {
        Mat fur = mat_paint(PAINT_PEACH);
        seg(g, 0, t1, 0, (r->bw + 4) * 16, (r->bw + 1) * 16, &fur, SH_CYL, CAP_FAR);
        {
            int x, y;
            at(g, t1 / 2, 0, &x, &y);
            decal_heart(g, x, y, PAL_PAINT(PAINT_ROSE, 1), PAL_WHITE);
        }
        break;
    }
    default: break;
    }
}

/* ---- per-kind bodies ---------------------------------------------------------- */
static void body_gun(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    int b0 = r->body0 + (fire ? 4 : 0), b1 = r->body1;
    int bw = r->bw * 16;
    Mat *body = w->style == WSTYLE_CUTE ? &g->paint : &g->metal;
    Mat *trim = w->style == WSTYLE_CUTE ? &g->accent : &g->paint;
    /* handguard, then the receiver with a slide / top rail */
    if (r->fore1)
        seg(g, r->fore0, r->fore1, 0, r->forew * 16, r->forew * 14,
            w->style == WSTYLE_CUTE ? &g->accent : &g->wood, SH_BOX, w->style == WSTYLE_CUTE ? CAP_FAR : 0);
    seg(g, b0, b1, 0, bw, bw * 7 / 8, body, SH_BOX, w->style == WSTYLE_CUTE ? CAP_NEAR | CAP_FAR : 0);
    seg(g, b0 + 6, b1 - (fire ? 14 : 4), 0, bw / 3, bw / 3, trim, SH_CYL, 0);
    if (w->frame == WFRAME_REVOLVER) {
        ell_at(g, (b0 + b1) / 2, 0, r->bw + 2, trim, 0);
        ell_at(g, (b0 + b1) / 2, 0, 3, &g->dark, 0);
    }
    if (w->frame == WFRAME_PLASMA_RIFLE) {
        int k;
        for (k = 0; k < 3; k++) seg(g, b0 + 20 + k * 30, b0 + 32 + k * 30, 0, bw + 8, bw + 8, &g->glow, SH_GLOW, CAP_NEAR | CAP_FAR);
    }
    if (w->frame == WFRAME_NAILGUN) ell_at(g, b0 + 50, -(r->bw + 4), 8, &g->dark, 0);
    if (w->frame == WFRAME_STICKER_GUN) {
        Mat roll = mat_paint(PAINT_LEMON);
        ell_at(g, b0 + 60, 0, 10, &roll, 0);
        ell_at(g, b0 + 60, 0, 4, &g->white, 0);
    }
    if (w->frame == WFRAME_MACHINE_PISTOL || w->frame == WFRAME_SMG)
        seg(g, b0 + 30, b0 + 60, (r->bw + 3) * 16, 30, 28, trim, SH_BOX, 0);
    /* brutal greebles, cute stickers + face */
    if (w->style == WSTYLE_BRUTAL) {
        rivet(g, b0 + 20, -r->bw + 2);
        rivet(g, b1 - 20, r->bw - 3);
        rivet(g, b0 + 20, r->bw - 3);
        splat(g, b0 + 40, r->bw / 2, g->seed);
        if (w->charm == WCHARM_SKULL || (g->seed & 3) == 0) {
            int x, y;
            at(g, b0 + 26, 0, &x, &y);
            decal_skull(g, x, y, g->bone.c[3], g->bone.c[0]);
        }
    } else {
        int x, y;
        at(g, b0 + (b1 - b0) * 2 / 5, 0, &x, &y);
        decal_face(g, x, y);
        at(g, b0 + 16, r->bw / 2 + 1, &x, &y);
        decal_heart(g, x, y, PAL_PAINT(PAINT_CHERRY, 2), PAL_WHITE);
    }
}

static void body_shotgun(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    int pump = fire ? -16 : 0;
    Mat *body = w->style == WSTYLE_CUTE ? &g->paint : &g->metal;
    seg(g, r->body0, r->body1, 0, r->bw * 16, r->bw * 15, body, SH_BOX, w->style == WSTYLE_CUTE ? CAP_NEAR : 0);
    if (w->frame == WFRAME_PUMP_SHOTGUN || w->frame == WFRAME_AUTO_SHOTGUN)
        seg(g, r->bar0 + 10 + pump, r->bar0 + 50 + pump, (r->brw / 2 + 1) * 16, 60, 56,
            w->style == WSTYLE_CUTE ? &g->accent : &g->wood, SH_BOX, 0);
    if (w->frame == WFRAME_LEVER_SHOTGUN) {
        int x, y;
        at(g, r->body0 + 40, r->bw + 1, &x, &y);
        ell(g, x * 16 + 8, (y + 2) * 16 + 8, 40, 30, &g->gold, 0);
    }
    if (w->frame == WFRAME_AUTO_SHOTGUN) ell_at(g, r->body0 + 60, -(r->bw + 3), 8, &g->dark, 0);
    if (w->style == WSTYLE_BRUTAL) {
        rivet(g, r->body0 + 30, -r->bw + 2);
        rivet(g, r->body0 + 60, -r->bw + 2);
        splat(g, r->body0 + 50, 2, g->seed);
    } else {
        int x, y;
        at(g, r->body0 + 50, 0, &x, &y);
        decal_face(g, x, y);
    }
}

static void minigun(G *g, const DoomoWeapon *w, const Recipe *r, int frame)
{
    int k, rot = frame * 3;
    Mat *body = w->style == WSTYLE_CUTE ? &g->paint : &g->dark;
    Mat *bar = w->style == WSTYLE_CUTE ? &g->accent : &g->metal;
    seg(g, r->body0, r->body1, 0, r->bw * 16, r->bw * 14, body, SH_BOX, CAP_NEAR);
    for (k = 0; k < 6; k++) {
        int a = k * 682 + rot * 200;
        int s = FIX2INT(fx_cos(a) * 9);
        if (fx_sin(a) < 0) continue;                       /* back barrels hidden */
        seg(g, r->bar0, r->bar1 - (fx_sin(a) >> 13), s * 16, 30, 26, bar, SH_CYL, CAP_FAR);
    }
    for (k = 0; k < 6; k++) {
        int a = k * 682 + rot * 200;
        int s = FIX2INT(fx_cos(a) * 9);
        if (fx_sin(a) >= 0) continue;
        seg(g, r->bar0, r->bar1, s * 16, 32, 28, bar, SH_CYL, CAP_FAR);
    }
    seg(g, r->bar0 + 10, r->bar0 + 20, 0, 190, 180, body, SH_CYL, 0);
    seg(g, r->bar1 - 26, r->bar1 - 18, 0, 170, 160, body, SH_CYL, 0);
    if (w->style == WSTYLE_CUTE) {
        int x, y;
        at(g, r->body0 + 50, 0, &x, &y);
        decal_face(g, x, y);
    } else {
        spikes(g, r->body0 + 20, r->body1 - 10, -(r->bw + 1), 2, 6);
    }
}

static void launcher(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    int bw = r->brw * 4;
    Mat *tube = w->style == WSTYLE_CUTE ? &g->paint : &g->dark;
    seg(g, r->bar0 - (fire ? 6 : 0), r->bar1, 0, bw, bw, tube, SH_CYL, CAP_NEAR);
    seg(g, r->bar1 - 10, r->bar1, 0, bw + 10, bw + 10, w->style == WSTYLE_CUTE ? &g->accent : &g->metal, SH_CYL, 0);
    seg(g, r->bar0 + 20, r->bar0 + 34, 0, bw + 6, bw + 6, &g->accent, SH_CYL, 0);
    if (w->frame == WFRAME_ROCKET_LAUNCHER && !fire) {
        Mat red = mat_ramp(RAMP_BLOOD, 3);
        ell_at(g, r->bar1 - 2, 0, 4, &red, 0);
    }
    if (w->frame == WFRAME_GRENADE_LAUNCHER) {
        ell_at(g, r->body0 + 50, 0, r->bw + 3, &g->metal, 0);
        ell_at(g, r->body0 + 50, 0, 4, &g->dark, 0);
    }
    if (w->frame == WFRAME_PLUSH_LAUNCHER && !fire) {
        /* a little plush bear peeking out of the barrel */
        Mat fur = mat_paint(PAINT_CARAMEL);
        int x, y;
        at(g, r->bar1 + 4, 0, &x, &y);
        ell(g, x * 16 + 8, y * 16 + 8, 80, 70, &fur, 0);
        ell(g, (x - 4) * 16 + 8, (y - 4) * 16 + 8, 30, 30, &fur, 0);
        ell(g, (x + 4) * 16 + 8, (y - 4) * 16 + 8, 30, 30, &fur, 0);
        put(g, x - 2, y, PAL_PAINT(PAINT_JET, 1)); put(g, x + 2, y, PAL_PAINT(PAINT_JET, 1));
        put(g, x, y + 2, PAL_PAINT(PAINT_CHOCOLATE, 0)); put(g, x - 3, y + 1, PAL_PAINT(PAINT_ROSE, 2));
        put(g, x + 3, y + 1, PAL_PAINT(PAINT_ROSE, 2));
    }
    if (w->style == WSTYLE_BRUTAL) {
        spikes(g, r->bar0 + 30, r->bar1 - 30, -(r->brw / 4 + 1), 2, 6);
        splat(g, r->bar0 + 60, 3, g->seed);
    } else {
        int x, y;
        at(g, r->bar0 + 70, -3, &x, &y);
        decal_star(g, x, y, PAL_UI_GOLD, PAL_WHITE);
        at(g, r->bar0 + 46, 4, &x, &y);
        decal_heart(g, x, y, PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE);
    }
    seg(g, r->body0, r->body0 + 60, (bw / 16 + 2) * 16, 60, 50, &g->accent, SH_BOX, 0);
}

static void cone(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    /* the confetti cannon: a party-popper cone that flares open */
    Mat *m = w->style == WSTYLE_CUTE ? &g->paint : &g->dark;
    int k;
    seg(g, r->bar0, r->bar1, 0, 80, 220, m, SH_CYL, 0);
    if (w->style == WSTYLE_BRUTAL) spikes(g, r->bar0 + 20, r->bar1 - 10, -10, 3, 5);
    for (k = 0; k < 4; k++) seg(g, r->bar0 + 20 + k * 30, r->bar0 + 28 + k * 30, 0, 100 + k * 30, 110 + k * 30, &g->accent, SH_CYL, 0);
    seg(g, r->bar1 - 6, r->bar1, 0, 230, 230, &g->white, SH_CYL, 0);
    if (!fire) {
        int x, y;
        at(g, r->bar1 - 2, 0, &x, &y);
        for (k = 0; k < 10; k++) {
            uint32_t h = doomo_hash32((uint32_t)k * 31u + g->seed);
            uint8_t c = (uint8_t)PAL_PAINT((int)(h % 32), 2);
            put(g, x + (int)(h >> 8) % 13 - 6, y + (int)(h >> 16) % 5 - 2, c);
        }
    }
    seg(g, r->body0, r->body0 + 50, 0, 180, 150, &g->accent, SH_BOX, CAP_NEAR);
}

static void flamer(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    Mat tank = w->style == WSTYLE_CUTE ? mat_paint(PAINT_TANGERINE) : mat_ramp(RAMP_BLOOD, 2);
    body_gun(g, w, r, fire);
    seg(g, r->body0, r->body0 + 90, -(r->bw + 6) * 16, 90, 90, &tank, SH_CYL, CAP_NEAR | CAP_FAR);
    if (!fire) {
        int x, y;
        at(g, r->bar1 + 2, 0, &x, &y);
        put(g, x, y - 1, PAL_RAMP(RAMP_LAVA, 7)); put(g, x, y, PAL_RAMP(RAMP_LAVA, 5));
        put(g, x - 1, y, PAL_RAMP(RAMP_LAVA, 4)); put(g, x + 1, y, PAL_RAMP(RAMP_LAVA, 4));
    }
}

static void bfg(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    Mat *body = w->style == WSTYLE_CUTE ? &g->paint : &g->dark;
    seg(g, r->body0, r->body1, 0, r->bw * 16, r->bw * 14, body, SH_BOX, CAP_NEAR);
    seg(g, r->bar0, r->bar1, 0, r->brw * 4, r->brw * 5, w->style == WSTYLE_CUTE ? &g->accent : &g->metal, SH_CYL, 0);
    ell_at(g, r->bar1 - 6, 0, fire ? 12 : 9, &g->glow, 1);
    ell_at(g, r->body0 + 60, -(r->bw + 2), 6, &g->glow, 1);
    ell_at(g, r->body0 + 60, r->bw + 2, 6, &g->glow, 1);
    if (w->style == WSTYLE_BRUTAL) spikes(g, r->body0 + 10, r->body1 - 10, -(r->bw + 1), 3, 6);
    else {
        int x, y;
        at(g, r->body0 + 40, 0, &x, &y);
        decal_face(g, x, y);
    }
}

static void rail(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    int k;
    body_gun(g, w, r, fire);
    seg(g, r->bar0, r->bar1, -5 * 16, 24, 22, &g->metal, SH_BOX, 0);
    seg(g, r->bar0, r->bar1, 5 * 16, 24, 22, &g->metal, SH_BOX, 0);
    for (k = 0; k < 5; k++) seg(g, r->bar0 + 8 + k * 20, r->bar0 + 14 + k * 20, 0, 60, 56, &g->glow, SH_GLOW, 0);
}

static void blade(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    Mat edge = w->style == WSTYLE_CUTE ? mat_paint(PAINT_LAVENDER) : mat_ramp(RAMP_STEEL, 3);
    (void)fire;
    seg(g, 0, r->body1, 0, 60, 56, w->style == WSTYLE_CUTE ? &g->paint : &g->dark, SH_CYL, CAP_NEAR);
    seg(g, r->body1 - 6, r->body1 + 4, 0, 160, 160, &g->gold, SH_CYL, CAP_NEAR | CAP_FAR);
    seg(g, r->bar0 + 4, r->bar1, 0, r->brw * 16, r->brw * 4, &edge, SH_CYL, CAP_FAR);
    {
        int x0, y0, x1, y1;
        at(g, r->bar0 + 8, -r->brw + 1, &x0, &y0);
        at(g, r->bar1 - 6, -1, &x1, &y1);
        line(g, x0, y0, x1, y1, edge.c[4]);
    }
    if (w->style == WSTYLE_CUTE) {
        int x, y;
        at(g, r->bar0 + 60, 0, &x, &y);
        decal_star(g, x, y, PAL_UI_GOLD, PAL_WHITE);
    } else {
        splat(g, r->bar0 + 90, 0, g->seed);
        splat(g, r->bar0 + 150, 1, g->seed + 9);
    }
}

static void hammer(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    int head0 = r->bar0, head1 = r->bar1;
    (void)fire;
    seg(g, 0, r->body1, 0, 50, 40, w->style == WSTYLE_CUTE && r->brw ? &g->white : &g->wood, SH_CYL, CAP_NEAR);
    if (r->brw == 1) {
        /* lollipop: a big candy swirl disk */
        Mat a = mat_paint(w->paint), b = mat_paint(w->accent);
        int x, y, k;
        at(g, (head0 + head1) / 2, 0, &x, &y);
        ell(g, x * 16 + 8, y * 16 + 8, 22 * 16, 18 * 16, &a, 0);
        for (k = 0; k < 4096; k += 64) {
            int rr = 2 + k * 18 / 4096;
            int px = x + FIX2INT(fx_cos(k * 3) * rr), py = y + FIX2INT(fx_sin(k * 3) * rr * 4 / 5);
            put(g, px, py, b.c[2]);
            put(g, px + 1, py, b.c[3]);
        }
        put(g, x - 8, y - 8, PAL_WHITE); put(g, x - 7, y - 9, PAL_WHITE);
        return;
    }
    if (r->brw == 2) {
        /* squeaky mallet: a toy mallet head across the handle */
        Mat a = mat_paint(w->paint);
        seg(g, head0, head1, 0, 22 * 16, 20 * 16, &a, SH_BOX, CAP_NEAR | CAP_FAR);
        seg(g, head0 + 4, head1 - 4, -16 * 16, 6 * 16, 6 * 16, &g->accent, SH_CYL, CAP_NEAR | CAP_FAR);
        seg(g, head0 + 4, head1 - 4, 16 * 16, 6 * 16, 6 * 16, &g->accent, SH_CYL, CAP_NEAR | CAP_FAR);
        {
            int x, y;
            at(g, (head0 + head1) / 2, 0, &x, &y);
            decal_face(g, x, y);
        }
        return;
    }
    /* sledgehammer */
    seg(g, head0, head1, 0, 20 * 16, 18 * 16, w->style == WSTYLE_CUTE ? &g->paint : &g->metal, SH_BOX, 0);
    if (w->style == WSTYLE_BRUTAL) {
        spikes(g, head0, head1, -19, 2, 5);
        spikes(g, head0, head1, 19, 2, 5);
        splat(g, (head0 + head1) / 2, 4, g->seed);
    } else {
        int x, y;
        at(g, (head0 + head1) / 2, 0, &x, &y);
        decal_heart(g, x, y, PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE);
    }
}

static void wand(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    Mat sky = mat_paint(PAINT_SKY);
    int x, y;
    seg(g, 0, r->body1, 0, 36, 28, &g->paint, SH_CYL, CAP_NEAR);
    at(g, r->bar1 - 10, 0, &x, &y);
    ell(g, x * 16 + 8, y * 16 + 8, 13 * 16, 11 * 16, w->style == WSTYLE_CUTE ? &g->accent : &g->metal, 0);
    /* hollow out the ring */
    {
        int i, j;
        for (j = -8; j <= 8; j++)
            for (i = -10; i <= 10; i++)
                if (i * i * 64 + j * j * 100 <= 64 * 100) clear_px(g, x + i, y + j);
    }
    if (!fire) {
        ell(g, (x + 6) * 16, (y - 12) * 16, 70, 70, &sky, 0);
        put(g, x + 4, y - 14, PAL_WHITE);
    }
    decal_star(g, x + 10, y + 6, PAL_UI_GOLD, PAL_WHITE);
}

static void bow(G *g, const DoomoWeapon *w, const Recipe *r, int frame)
{
    int heart = r->brw == 1, k;
    int draw = frame == 1 ? 0 : 6;
    Mat *limb = heart ? &g->paint : (w->style == WSTYLE_CUTE ? &g->paint : &g->wood);
    /* the bow stands upright on the left, the arrow points up the middle */
    for (k = -24; k <= 24; k++) {
        int bx = 26 - (24 * 24 - k * k) / 48 - (heart && k < 0 ? (k * k) / 70 : 0);
        int by = 34 + k;
        if (heart && k > 0) bx += k / 3;
        put(g, bx, by, limb->c[2]);
        put(g, bx - 1, by, limb->c[1]);
        put(g, bx + 1, by, limb->c[3]);
        if ((k & 7) == 0) put(g, bx + 1, by, limb->c[4]);
    }
    line(g, 26, 10, 26 + draw + 12, 34, PAL_WHITE);
    line(g, 26, 58, 26 + draw + 12, 34, PAL_WHITE);
    if (frame != 1) {
        Mat shaft = w->style == WSTYLE_CUTE ? mat_paint(PAINT_CARAMEL) : mat_ramp(RAMP_WOOD, 2);
        line(g, 38 + draw, 36, 34, 6, shaft.c[2]);
        line(g, 39 + draw, 36, 35, 6, shaft.c[3]);
        if (heart) decal_heart(g, 34, 5, PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE);
        else tri(g, 31, 8, 37, 8, 34, 2, g->metal.c[3]);
        put(g, 37 + draw, 37, g->accent.c[2]); put(g, 40 + draw, 37, g->accent.c[2]);
    }
    if (heart) decal_heart(g, 18, 34, PAL_PAINT(PAINT_ROSE, 2), PAL_WHITE);
}

static void crossbow(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    int x0, y0, x1, y1, xm, ym;
    Mat *stock = w->style == WSTYLE_CUTE ? &g->paint : &g->wood;
    seg(g, r->body0, r->body1, 0, r->bw * 16, r->bw * 12, stock, SH_BOX, CAP_NEAR);
    at(g, r->body1 - 10, -26, &x0, &y0);
    at(g, r->body1 - 10, 26, &x1, &y1);
    at(g, r->body1 - 20, 0, &xm, &ym);
    line(g, x0, y0 + 1, xm, ym - 5, w->style == WSTYLE_CUTE ? g->accent.c[2] : g->metal.c[2]);
    line(g, x1, y1 + 1, xm, ym - 5, w->style == WSTYLE_CUTE ? g->accent.c[2] : g->metal.c[2]);
    line(g, x0, y0, xm, ym - 6, w->style == WSTYLE_CUTE ? g->accent.c[3] : g->metal.c[3]);
    line(g, x1, y1, xm, ym - 6, w->style == WSTYLE_CUTE ? g->accent.c[1] : g->metal.c[1]);
    if (!fire) {
        line(g, x0, y0 + 2, xm, ym + 8, PAL_WHITE);
        line(g, x1, y1 + 2, xm, ym + 8, PAL_WHITE);
        if (w->frame == WFRAME_YARN_CROSSBOW) {
            Mat yarn = mat_paint(PAINT_ROSE);
            ell_at(g, r->bar1 - 10, 0, 6, &yarn, 0);
        } else {
            seg(g, r->bar0 - 30, r->bar1, 0, 14, 12, &g->wood, SH_CYL, 0);
            seg(g, r->bar1 - 10, r->bar1, 0, 30, 6, &g->metal, SH_BOX, CAP_FAR);
        }
    } else {
        line(g, x0, y0 + 2, xm, ym - 2, PAL_WHITE);
        line(g, x1, y1 + 2, xm, ym - 2, PAL_WHITE);
    }
}

static void spray(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    if (w->frame == WFRAME_WATER_PISTOL) {
        /* super-soaker: a big round water tank on top */
        Mat water = mat_paint(PAINT_SKY);
        seg(g, r->body0, r->body1, 0, r->bw * 16, r->bw * 14, &g->paint, SH_BOX, CAP_NEAR | CAP_FAR);
        ell_at(g, r->body0 + 60, 0, 11, &g->accent, 0);
        ell_at(g, r->body0 + 60, -1, 7, &water, 0);
        seg(g, r->bar0, r->bar1, 0, 30, 24, &g->accent, SH_CYL, CAP_FAR);
        if (w->core == WCORE_HOLY_WATER) {
            int x, y;
            at(g, r->body0 + 60, -1, &x, &y);
            put(g, x, y - 2, PAL_WHITE); put(g, x, y - 1, PAL_WHITE); put(g, x, y, PAL_WHITE);
            put(g, x, y + 1, PAL_WHITE); put(g, x - 1, y - 1, PAL_WHITE); put(g, x + 1, y - 1, PAL_WHITE);
        }
    } else {
        /* glitter sprayer: a pump bottle */
        Mat bottle = mat_paint(PAINT_LAVENDER);
        seg(g, r->body0 + 20, r->body1, 0, r->bw * 16, r->bw * 12, &bottle, SH_CYL, CAP_NEAR);
        seg(g, r->body1 - 4, r->body1 + 30, 0, 50, 40, &g->accent, SH_BOX, 0);
        seg(g, r->bar0 + 20, r->bar1, 0, 18, 16, &g->white, SH_CYL, CAP_FAR);
        {
            int k, x, y;
            for (k = 0; k < 8; k++) {
                at(g, r->body0 + 40 + k * 10, (k & 1) ? 3 : -3, &x, &y);
                put(g, x, y, (k & 1) ? PAL_UI_GOLD : PAL_WHITE);
            }
        }
        if (fire) decal_star(g, 40, 6, PAL_UI_GOLD, PAL_WHITE);
    }
}

static void fists(G *g, const DoomoWeapon *w, int frame)
{
    /* two big plush mitts (cute) or spiked knuckle wraps (brutal) */
    Mat *m = w->style == WSTYLE_CUTE ? &g->paint : &g->dark;
    int lx = 20, rx = 72, ly = 50, ry = 48;
    if (frame == 1) { ry -= 18; rx -= 12; }
    ell(g, lx * 16, ly * 16, 15 * 16, 13 * 16, m, 0);
    ell(g, rx * 16, ry * 16, 16 * 16, 14 * 16, m, 0);
    ell(g, (lx - 6) * 16, (ly - 10) * 16, 5 * 16, 4 * 16, m, 0);
    ell(g, lx * 16, (ly - 12) * 16, 5 * 16, 4 * 16, m, 0);
    ell(g, (lx + 6) * 16, (ly - 10) * 16, 5 * 16, 4 * 16, m, 0);
    ell(g, (rx - 7) * 16, (ry - 11) * 16, 5 * 16, 4 * 16, m, 0);
    ell(g, rx * 16, (ry - 13) * 16, 5 * 16, 4 * 16, m, 0);
    ell(g, (rx + 7) * 16, (ry - 11) * 16, 5 * 16, 4 * 16, m, 0);
    if (w->style == WSTYLE_BRUTAL) {
        int k;
        for (k = -1; k <= 1; k++) {
            tri(g, rx + k * 7 - 2, ry - 15, rx + k * 7 + 2, ry - 15, rx + k * 7, ry - 21, g->bone.c[3]);
            tri(g, lx + k * 6 - 2, ly - 13, lx + k * 6 + 2, ly - 13, lx + k * 6, ly - 18, g->bone.c[3]);
        }
        splat(g, 0, 0, g->seed);
    } else {
        decal_heart(g, rx, ry + 2, PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE);
        decal_star(g, lx, ly + 2, PAL_UI_GOLD, PAL_WHITE);
    }
}

static void chainsaw(G *g, const DoomoWeapon *w, const Recipe *r, int frame)
{
    int k, x, y;
    Mat *body = w->style == WSTYLE_CUTE ? &g->paint : &g->accent;
    seg(g, r->body0, r->body1, 0, r->bw * 16, r->bw * 14, body, SH_BOX, CAP_NEAR | CAP_FAR);
    seg(g, r->bar0, r->bar1, 0, r->brw * 16, r->brw * 12, &g->metal, SH_BOX, CAP_FAR);
    for (k = 0; k < 14; k++) {
        int t = r->bar0 + 6 + ((k * 12 + frame * 5) % 150);
        if (t > r->bar1 - 4) continue;
        at(g, t, -r->brw - 1, &x, &y); put(g, x, y, g->metal.c[4]);
        at(g, t, r->brw + 1, &x, &y); put(g, x, y, g->metal.c[0]);
    }
    if (w->style == WSTYLE_BRUTAL) { splat(g, r->bar0 + 60, 0, g->seed); splat(g, r->bar0 + 110, 2, g->seed + 3); }
    else { at(g, r->body0 + 50, 0, &x, &y); decal_face(g, x, y); }
}

static void dual(G *g, const DoomoWeapon *w, const Recipe *r, int fire)
{
    G g2 = *g;
    /* the left-hand pistol: same parts on a mirrored axis */
    set_axis(&g2, 26, 70, 36, 12, 110, 220);
    body_gun(&g2, w, r, 0);
    barrel_part(&g2, w, r, 0);
    body_gun(g, w, r, fire);
}

/* ---- compose -------------------------------------------------------------------- */
static void setup_mats(G *g, const DoomoWeapon *w)
{
    g->cute = w->style == WSTYLE_CUTE;
    g->paint = mat_paint(w->paint);
    g->accent = mat_paint(w->accent);
    g->metal = g->cute ? mat_paint(PAINT_SILVER) : mat_ramp(RAMP_STEEL, 2);
    g->dark = g->cute ? mat_paint(PAINT_CHARCOAL) : mat_ramp(RAMP_STEEL, 0);
    g->white = mat_paint(PAINT_WHITE);
    g->wood = g->cute ? mat_paint(PAINT_CARAMEL) : mat_ramp(RAMP_WOOD, 1);
    g->bone = mat_ramp(RAMP_BONE, 2);
    g->gold = mat_ramp(RAMP_CANDY, 2);
    g->black = mat_paint(PAINT_JET);
    switch (w->core) {
    case WCORE_HELLFIRE: case WCORE_FUEL: case WCORE_ROCKET: g->glow = mat_ramp(RAMP_LAVA, 3); break;
    case WCORE_SLIME: g->glow = mat_ramp(RAMP_SICK, 3); break;
    case WCORE_LOVE: case WCORE_CANDY: g->glow = mat_paint(PAINT_BUBBLEGUM); break;
    case WCORE_HOLY_WATER: g->glow = mat_ramp(RAMP_CANDY, 3); break;
    default: g->glow = mat_paint(g->cute ? PAINT_MINT : PAINT_CYAN); break;
    }
    g->seed = doomo_hash32((uint32_t)w->frame * 131u + w->paint * 7u + w->accent);
}

static void compose(uint8_t *px, int w, int h, const DoomoWeapon *wp, int frame, int icon)
{
    G g;
    const Recipe *r = &recipes[wp->frame < WFRAME_COUNT ? wp->frame : WFRAME_FISTS];
    int fire = frame == 1 || frame == 2;
    memset(&g, 0, sizeof(g));
    memset(px, 0, (size_t)(w * h));
    g.px = px; g.w = w; g.h = h;
    setup_mats(&g, wp);
    if (icon) {
        set_axis(&g, 1, 9, 23, 8, 0, 70);
    } else {
        switch (frame) {
        case 1: set_axis(&g, 66, 82, 40, 12, 110, VIEW_WSC); break;
        case 2: set_axis(&g, 65, 79, 39, 9, 110, VIEW_WSC); break;
        case 3: set_axis(&g, 76, 80, 60, 16, 110, VIEW_WSC); break;
        default: set_axis(&g, 64, 76, 38, 6, 110, VIEW_WSC); break;
        }
        if (r->kind == RK_BLADE || r->kind == RK_HAMMER || r->kind == RK_WAND || r->kind == RK_SAW) {
            if (frame == 1) set_axis(&g, 78, 66, 22, 18, 100, 256);       /* the swing */
            else set_axis(&g, 66, 70, 52, 2, 100, 256);
        }
    }
    if (icon && (r->kind == RK_FISTS || r->kind == RK_BOW)) {
        /* simple icons for the odd ones */
        if (r->kind == RK_FISTS) {
            ell(&g, 8 * 16, 9 * 16, 6 * 16, 5 * 16, wp->style == WSTYLE_CUTE ? &g.paint : &g.dark, 0);
            ell(&g, 16 * 16, 8 * 16, 6 * 16, 5 * 16, wp->style == WSTYLE_CUTE ? &g.paint : &g.dark, 0);
        } else {
            int k;
            for (k = -6; k <= 6; k++) put(&g, 8 - (36 - k * k) / 8, 8 + k, g.paint.c[2]);
            line(&g, 8, 2, 8, 14, PAL_WHITE);
            line(&g, 4, 8, 22, 8, g.wood.c[3]);
            if (r->brw == 1) decal_heart(&g, 21, 8, PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE);
        }
        outline(&g);
        return;
    }
    switch (r->kind) {
    case RK_FISTS: fists(&g, wp, frame); break;
    case RK_BOW: bow(&g, wp, r, frame); break;
    case RK_XBOW:
        stock_part(&g, wp, r);
        crossbow(&g, wp, r, fire);
        sight_part(&g, wp, r);
        break;
    case RK_BLADE: blade(&g, wp, r, fire); break;
    case RK_HAMMER: hammer(&g, wp, r, fire); break;
    case RK_WAND: wand(&g, wp, r, fire); break;
    case RK_SAW: chainsaw(&g, wp, r, frame); break;
    default:
        stock_part(&g, wp, r);
        if (r->kind != RK_LAUNCHER && r->kind != RK_CONE) mag_part(&g, wp, r, frame == 3);
        under_part(&g, wp, r);
        switch (r->kind) {
        case RK_MINIGUN: minigun(&g, wp, r, frame); break;
        case RK_SHOTGUN: barrel_part(&g, wp, r, fire); body_shotgun(&g, wp, r, fire); break;
        case RK_LAUNCHER: launcher(&g, wp, r, fire); break;
        case RK_CONE: cone(&g, wp, r, fire); break;
        case RK_FLAME: barrel_part(&g, wp, r, fire); flamer(&g, wp, r, fire); break;
        case RK_BFG: bfg(&g, wp, r, fire); break;
        case RK_RAIL: barrel_part(&g, wp, r, fire); rail(&g, wp, r, fire); break;
        case RK_SPRAY: spray(&g, wp, r, fire); break;
        case RK_DUAL: barrel_part(&g, wp, r, fire); dual(&g, wp, r, fire); break;
        default: barrel_part(&g, wp, r, fire); body_gun(&g, wp, r, fire); break;
        }
        core_canister(&g, wp, r->body0 + 40, r->bw + 4);
        if (!icon) {
            mod_part(&g, wp, r);
            sight_part(&g, wp, r);
            charm_part(&g, wp, r->body1 - 20, -(r->bw + 2));
        }
        break;
    }
    if (frame == 2 && !icon) {
        /* a puff of smoke at the muzzle */
        int x, y;
        Mat smoke = mat_ramp(RAMP_STONE, 3);
        at(&g, DOOMO_MIN(r->bar1 + 8, 255), 0, &x, &y);
        if (r->kind != RK_BLADE && r->kind != RK_HAMMER && r->kind != RK_FISTS && r->kind != RK_BOW) {
            ell(&g, x * 16, y * 16, 50, 40, &smoke, 0);
            ell(&g, (x + 3) * 16, (y - 3) * 16, 40, 34, &smoke, 0);
        }
    }
    outline(&g);
}

/* ---- caches ----------------------------------------------------------------------- */
static uint32_t weapon_key(const DoomoWeapon *w, int frame)
{
    uint32_t k = (uint32_t)w->frame | ((uint32_t)w->barrel << 6) | ((uint32_t)w->core << 10) |
                 ((uint32_t)w->mod << 15) | ((uint32_t)w->sight << 19) | ((uint32_t)w->stock << 22) |
                 ((uint32_t)w->mag << 25) | ((uint32_t)w->under << 28);
    k = doomo_hash32(k) ^ doomo_hash32((uint32_t)w->charm | ((uint32_t)w->style << 4) |
                                        ((uint32_t)w->paint << 5) | ((uint32_t)w->accent << 10) |
                                        ((uint32_t)frame << 16));
    return k | 1u;
}

const DoomoImage *weld_view_image(const DoomoWeapon *w, int frame)
{
    static const DoomoWeapon fists = { WFRAME_FISTS, 0, 0, 0, 0, 0, 0, 0, 0, WSTYLE_CUTE,
                                       PAINT_ROSE, PAINT_WHITE, 50, WF_VALID, 0, 0 };
    uint32_t key;
    int i, slot = 0;
    if (!w) w = &fists;
    frame = DOOMO_CLAMP(frame, 0, 3);
    key = weapon_key(w, frame);
    use_clock++;
    for (i = 0; i < VIEW_SLOTS; i++)
        if (view_key[i] == key) { view_used[i] = use_clock; return &view_img[i]; }
    for (i = 1; i < VIEW_SLOTS; i++) if (view_used[i] < view_used[slot]) slot = i;
    compose(view_pix[slot], VW, VH, w, frame, 0);
    view_key[slot] = key;
    view_used[slot] = use_clock;
    view_img[slot].w = VW;
    view_img[slot].h = VH;
    view_img[slot].pix = view_pix[slot];
    view_img[slot].uid = doomo_image_new_uid();
    return &view_img[slot];
}

const DoomoImage *weld_icon_image(const DoomoWeapon *w)
{
    static const DoomoWeapon fists = { WFRAME_FISTS, 0, 0, 0, 0, 0, 0, 0, 0, WSTYLE_CUTE,
                                       PAINT_ROSE, PAINT_WHITE, 50, WF_VALID, 0, 0 };
    uint32_t key;
    int i, slot = 0;
    if (!w) w = &fists;
    key = weapon_key(w, 7);
    use_clock++;
    for (i = 0; i < ICON_SLOTS; i++)
        if (icon_key[i] == key) { icon_used[i] = use_clock; return &icon_img[i]; }
    for (i = 1; i < ICON_SLOTS; i++) if (icon_used[i] < icon_used[slot]) slot = i;
    compose(icon_pix[slot], IW, IH, w, 0, 1);
    icon_key[slot] = key;
    icon_used[slot] = use_clock;
    icon_img[slot].w = IW;
    icon_img[slot].h = IH;
    icon_img[slot].pix = icon_pix[slot];
    icon_img[slot].uid = doomo_image_new_uid();
    return &icon_img[slot];
}

void weld_view_points(const DoomoWeapon *w, int *mx, int *my, int *gx, int *gy)
{
    G g;
    const Recipe *r;
    int kind;
    if (!w) { *mx = VW / 2; *my = 6; *gx = VW / 2 + 8; *gy = VH - 10; return; }
    r = &recipes[w->frame < WFRAME_COUNT ? w->frame : WFRAME_FISTS];
    kind = r->kind;
    memset(&g, 0, sizeof(g));
    g.w = VW; g.h = VH;
    set_axis(&g, 64, 76, 38, 6, 110, VIEW_WSC);
    if (kind == RK_BLADE || kind == RK_HAMMER || kind == RK_WAND || kind == RK_SAW)
        set_axis(&g, 66, 70, 52, 2, 100, 256);
    at(&g, kind == RK_BOW ? 230 : DOOMO_MIN(r->bar1 + 2, 255), 0, mx, my);
    at(&g, r->grip ? r->grip : 60, r->bw + 4, gx, gy);
    if (kind == RK_BOW) { *mx = 34; *my = 4; *gx = 28; *gy = 40; }
    if (kind == RK_FISTS) { *mx = 70; *my = 30; *gx = 70; *gy = 50; }
    if (w->barrel == WBARREL_SHORT || w->barrel == WBARREL_SAWED) {
        at(&g, r->bar0 + (r->bar1 - r->bar0) * (w->barrel == WBARREL_SHORT ? 6 : 5) / 10, 0, mx, my);
    }
}

void weld_gfx_invalidate(void)
{
    memset(view_key, 0, sizeof(view_key));
    memset(icon_key, 0, sizeof(icon_key));
}
