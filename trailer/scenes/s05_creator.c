/* s05_creator.c - the citizen creator.
 *
 * A puffy pink editor window boings up over a mint candy-stripe backdrop:
 * tabs, a spotlit turntable preview, a KIND selector, fur swatches,
 * sliders (CUTE is pinned at max, EVIL wobbles) and an EYES counter. The
 * cursor clicks around and the preview cycles FAST through critter-folk,
 * the cast, and increasingly absurd combos (halo + horns + wings, slime fur,
 * a space-marine bunny). "Create ANYONE." ... "Literally anyone."
 * It settles on Kevin, a perfectly normal mint pup. Click: EYES 3 (?!).
 * SNAP ZOOM to a full-screen close-up: a third eye pops open on Kevin's
 * forehead. "Yes, three eyes." Later, only the third eye blinks.
 */
#include "tk.h"
#include "pal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================== */
/* shared premium-rendering helpers (static, copied per scene file)            */
/* ========================================================================== */
#define U static __attribute__((unused))
static const uint8_t BY4[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

/* -- palette blend maps: every entry pushed a/16 of the way toward an RGB -- */
typedef struct { int key; uint8_t m[256]; } CMap;
static CMap cm_cache[40];
static int cm_n;
U int near_rgb(int r, int g, int b) {
    int best = 1; long bd = 1L << 40;
    for (int i = 1; i < 256; i++) {
        long rm = (TK_PAL[i][0] + r) / 2;
        long dr = TK_PAL[i][0] - r, dg = TK_PAL[i][1] - g, db = TK_PAL[i][2] - b;
        long d = (((512 + rm) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rm) * db * db) >> 8);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
U const uint8_t *cmap(int r, int g, int b, int a) {
    int key = ((r >> 3) << 15) | ((g >> 3) << 10) | ((b >> 3) << 5) | (a & 31);
    for (int i = 0; i < cm_n; i++) if (cm_cache[i].key == key) return cm_cache[i].m;
    CMap *c = &cm_cache[cm_n < 40 ? cm_n++ : 39];
    c->key = key;
    for (int i = 0; i < 256; i++) {
        int R = TK_PAL[i][0] + (r - TK_PAL[i][0]) * a / 16;
        int G = TK_PAL[i][1] + (g - TK_PAL[i][1]) * a / 16;
        int B = TK_PAL[i][2] + (b - TK_PAL[i][2]) * a / 16;
        c->m[i] = (uint8_t)near_rgb(R, G, B);
    }
    return c->m;
}
U void tint_px(int x, int y, const uint8_t *m) {
    if ((unsigned)x < TW && (unsigned)y < TH) tk_fb[y][x] = m[tk_fb[y][x]];
}
/* pick from an n-tone ramp at level L, dithering only a thin seam */
U uint8_t qtone(const uint8_t *R, int n, float L, int x, int y) {
    if (L <= 0) return R[0];
    if (L >= n - 1) return R[n - 1];
    int a = (int)L;
    float fr = L - a;
    if (fr > 0.62f) return R[a + 1];
    if (fr < 0.38f) return R[a];
    return BY4[y & 3][x & 3] < (int)((fr - 0.38f) / 0.24f * 16) ? R[a + 1] : R[a];
}
/* tint a rect with ordered-dither coverage lvl 0..16 */
U void tint_rect(int x, int y, int w, int h, const uint8_t *m, int lvl) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (BY4[j & 3][i & 3] < lvl) tint_px(i, j, m);
}
/* soft tinted ellipse: full inside, dithered falloff over the outer `soft` */
U void tint_ellipse(float cx, float cy, float rx, float ry, const uint8_t *m, float soft, int maxlvl) {
    if (rx < 0.5f || ry < 0.5f) return;
    for (int y = (int)floorf(cy - ry); y <= (int)ceilf(cy + ry); y++)
        for (int x = (int)floorf(cx - rx); x <= (int)ceilf(cx + rx); x++) {
            float fx = (x + 0.5f - cx) / rx, fy = (y + 0.5f - cy) / ry;
            float d = sqrtf(fx * fx + fy * fy);
            if (d >= 1.0f) continue;
            int lvl = d < 1.0f - soft ? 16 : (int)((1.0f - d) / soft * 16.0f);
            if (lvl > maxlvl) lvl = maxlvl;
            if (BY4[y & 3][x & 3] < lvl) tint_px(x, y, m);
        }
}
/* layers: snapshot, draw anything, then tint only what changed */
static uint8_t snap[TH][TW];
U void layer_begin(void) { memcpy(snap, tk_fb, sizeof snap); }
U void layer_end(const uint8_t *m) {
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++)
            if (tk_fb[y][x] != snap[y][x]) tk_fb[y][x] = m[tk_fb[y][x]];
}

/* tint only what changed since layer_begin, and only inside a diagonal
 * band (x + y in [pos, pos + w)) - a glossy shine sweep across a logo */
U void layer_band(const uint8_t *m, int pos, int w) {
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            int u = x + y - pos;
            if (u < 0 || u >= w || tk_fb[y][x] == snap[y][x]) continue;
            tk_fb[y][x] = m[tk_fb[y][x]];
        }
}

/* -- masks + pillow shading: inflated plush / glossy plastic look ---------- */
static uint8_t mk[TH][TW];
static int mbx0 = TW, mby0 = TH, mbx1 = -1, mby1 = -1;
U void mk_px(int x, int y) {
    if ((unsigned)x >= TW || (unsigned)y >= TH) return;
    mk[y][x] = 1;
    if (x < mbx0) mbx0 = x;
    if (x > mbx1) mbx1 = x;
    if (y < mby0) mby0 = y;
    if (y > mby1) mby1 = y;
}
U void mk_ellipse(float cx, float cy, float rx, float ry) {
    for (int y = (int)floorf(cy - ry - 1); y <= (int)ceilf(cy + ry + 1); y++)
        for (int x = (int)floorf(cx - rx - 1); x <= (int)ceilf(cx + rx + 1); x++) {
            float fx = (x + 0.5f - cx) / rx, fy = (y + 0.5f - cy) / ry;
            if (fx * fx + fy * fy <= 1.0f) mk_px(x, y);
        }
}
U void mk_rect(int x, int y, int w, int h) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) mk_px(i, j);
}
U void mk_tri(float x0, float y0, float x1, float y1, float x2, float y2) {
    float minx = fminf(x0, fminf(x1, x2)), maxx = fmaxf(x0, fmaxf(x1, x2));
    float miny = fminf(y0, fminf(y1, y2)), maxy = fmaxf(y0, fmaxf(y1, y2));
    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (fabsf(area) < 0.01f) return;
    for (int y = (int)floorf(miny); y <= (int)ceilf(maxy); y++)
        for (int x = (int)floorf(minx); x <= (int)ceilf(maxx); x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area;
            float w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area;
            float w2 = 1 - w0 - w1;
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) mk_px(x, y);
        }
}
U void mk_reset(void) {
    if (mbx1 >= 0)
        for (int y = mby0; y <= mby1; y++) memset(&mk[y][mbx0], 0, (size_t)(mbx1 - mbx0 + 1));
    mbx0 = TW; mby0 = TH; mbx1 = -1; mby1 = -1;
}
static uint16_t dwk[TH + 8][TW + 8];
/* Shade the current mask: 4-tone ramp r[0..3] (dark..light), light from
 * the upper left, bevel = width (px) of the rounded edge, coloured 1-px
 * outline `ol` (0 = none), reflected rim light on the lower-right edge.
 * Clears the mask afterwards. */
U void pillow(const uint8_t *r, uint8_t ol, float bevel, float tilt) {
    if (mbx1 < 0) return;
    int x0 = mbx0 - 2, y0 = mby0 - 2, W = mbx1 - mbx0 + 5, H = mby1 - mby0 + 5;
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            int sx = x0 + i, sy = y0 + j;
            if (sx < 0) sx = 0;
            if (sx >= TW) sx = TW - 1;
            if (sy < 0) sy = 0;
            if (sy >= TH) sy = TH - 1;
            dwk[j][i] = mk[sy][sx] ? 30000 : 0;
        }
    for (int j = 1; j < H; j++)
        for (int i = 1; i < W - 1; i++) {
            uint16_t v = dwk[j][i];
            if (!v) continue;
            if (dwk[j - 1][i] + 3 < v) v = dwk[j - 1][i] + 3;
            if (dwk[j][i - 1] + 3 < v) v = dwk[j][i - 1] + 3;
            if (dwk[j - 1][i - 1] + 4 < v) v = dwk[j - 1][i - 1] + 4;
            if (dwk[j - 1][i + 1] + 4 < v) v = dwk[j - 1][i + 1] + 4;
            dwk[j][i] = v;
        }
    for (int j = H - 2; j >= 0; j--)
        for (int i = W - 2; i >= 1; i--) {
            uint16_t v = dwk[j][i];
            if (!v) continue;
            if (dwk[j + 1][i] + 3 < v) v = dwk[j + 1][i] + 3;
            if (dwk[j][i + 1] + 3 < v) v = dwk[j][i + 1] + 3;
            if (dwk[j + 1][i + 1] + 4 < v) v = dwk[j + 1][i + 1] + 4;
            if (dwk[j + 1][i - 1] + 4 < v) v = dwk[j + 1][i - 1] + 4;
            dwk[j][i] = v;
        }
    float cyc = (mby0 + mby1) * 0.5f, hh = (mby1 - mby0 + 1) * 0.5f;
    float cxc = (mbx0 + mbx1) * 0.5f, hw = (mbx1 - mbx0 + 1) * 0.5f;
    for (int j = 1; j < H - 1; j++)
        for (int i = 1; i < W - 1; i++) {
            int sx = x0 + i, sy = y0 + j;
            if ((unsigned)sx >= TW || (unsigned)sy >= TH) continue;
            if (!dwk[j][i]) {
                if (ol && (dwk[j - 1][i] || dwk[j + 1][i] || dwk[j][i - 1] || dwk[j][i + 1]))
                    tk_fb[sy][sx] = ol;
                continue;
            }
            float t = dwk[j][i] / 3.0f;
            float gx = (float)dwk[j][i + 1] - (float)dwk[j][i - 1];
            float gy = (float)dwk[j + 1][i] - (float)dwk[j - 1][i];
            float len = sqrtf(gx * gx + gy * gy);
            float nx = 0, ny = 0;
            if (len > 0.01f) { nx = -gx / len; ny = -gy / len; }
            float e = 1.0f - (t - 0.5f) / bevel;
            e = tk_clampf(e, 0, 1);
            e = e * (2.0f - e);
            float lit = (-0.62f * nx - 0.78f * ny) * e;
            float L = 1.55f + 1.75f * lit - tilt * ((sy - cyc) / hh * 0.85f + (sx - cxc) / hw * 0.45f);
            int tone = (int)floorf(L);
            float fr = L - tone;
            if (fr > 0.68f) tone++;
            else if (fr > 0.32f && ((sx + sy) & 1)) tone++;
            if (tone < 0) tone = 0;
            if (tone > 3) tone = 3;
            if (t <= 1.01f && lit < -0.3f) tone = 2;           /* reflected rim */
            tk_fb[sy][sx] = r[tone];
        }
    mk_reset();
}
/* -- glossy two-highlight eye -------------------------------------------- */
U void gloss_eye(float cx, float cy, float rx, float ry, uint8_t rim, uint8_t iris, uint8_t glow) {
    for (int y = (int)floorf(cy - ry - 1); y <= (int)ceilf(cy + ry + 1); y++)
        for (int x = (int)floorf(cx - rx - 1); x <= (int)ceilf(cx + rx + 1); x++) {
            float fx = (x + 0.5f - cx) / rx, fy = (y + 0.5f - cy) / ry;
            float d = fx * fx + fy * fy;
            if (d > 1.0f) continue;
            uint8_t c = rim;
            if (d < 0.55f && fy > -0.35f) c = iris;
            if (d < 0.5f && fy > 0.25f) c = glow;
            tk_px(x, y, c);
        }
    tk_rect((int)(cx - rx * 0.55f), (int)(cy - ry * 0.6f), rx > 2.5f ? 2 : 1, ry > 3 ? 2 : 1, PAL_WHITE);
    tk_px((int)(cx + rx * 0.4f), (int)(cy + ry * 0.35f), PAL_WHITE);
}
/* -- bouncy per-letter caption: letters hop in one after another ---------- */
U void bcap(int cy, const char *s, uint8_t c, uint8_t oc, int sc, int age) {
    if (age < 0) return;
    char line[96];
    int lines = 1;
    for (const char *p = s; *p; p++) if (*p == '\n') lines++;
    int y = cy - (lines * 9 * sc) / 2, k = 0;
    const char *p = s;
    while (*p) {
        int n = 0;
        while (p[n] && p[n] != '\n' && n < 95) { line[n] = p[n]; n++; }
        line[n] = 0;
        int x = TW / 2 - tk_text_w(line, sc) / 2;
        for (int i = 0; i < n; i++, k++) {
            int a = age - k / 2;
            if (a < 0) continue;
            static const int HOP[] = { -7, -4, 1, 2, 1, 0 };
            int dy = a < 6 ? HOP[a] * sc / 2 : 0;
            char ch[2] = { line[i], 0 };
            if (line[i] != ' ') tk_text_outline(x + i * 6 * sc, y + dy, ch, c, oc, sc);
        }
        y += 9 * sc;
        p += n;
        if (*p == '\n') p++;
    }
}
/* -- puffy logo letters: font glyph -> Scale3x/Scale2x smoothing ---------- */
typedef struct { int w, h; uint8_t m[48][40]; } Tile;
static void glyph1(unsigned char ch, uint8_t g[8][5]) {
    uint8_t save[8][6];
    for (int y = 0; y < 8; y++) for (int x = 0; x < 6; x++) { save[y][x] = tk_fb[y][x]; tk_fb[y][x] = 0; }
    char s[2] = { (char)ch, 0 };
    tk_text(0, 0, s, 1, 1);
    for (int y = 0; y < 8; y++) for (int x = 0; x < 5; x++) g[y][x] = tk_fb[y][x] != 0;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 6; x++) tk_fb[y][x] = save[y][x];
}
static int gat(const uint8_t *m, int w, int h, int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h) return 0;
    return m[y * w + x];
}
/* scale2x (EPX) of a w*h binary image into out (2w*2h) */
static void scale2x(const uint8_t *in, int w, int h, uint8_t *out) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int P = gat(in, w, h, x, y), A = gat(in, w, h, x, y - 1), B = gat(in, w, h, x + 1, y);
            int C = gat(in, w, h, x - 1, y), D = gat(in, w, h, x, y + 1);
            int e0 = (C == A && C != D && A != B) ? A : P, e1 = (A == B && A != C && B != D) ? B : P;
            int e2 = (D == C && D != B && C != A) ? C : P, e3 = (B == D && B != A && D != C) ? D : P;
            out[(2 * y) * 2 * w + 2 * x] = (uint8_t)e0; out[(2 * y) * 2 * w + 2 * x + 1] = (uint8_t)e1;
            out[(2 * y + 1) * 2 * w + 2 * x] = (uint8_t)e2; out[(2 * y + 1) * 2 * w + 2 * x + 1] = (uint8_t)e3;
        }
}
static void scale3x(const uint8_t *in, int w, int h, uint8_t *out) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int A = gat(in, w, h, x - 1, y - 1), B = gat(in, w, h, x, y - 1), C = gat(in, w, h, x + 1, y - 1);
            int D = gat(in, w, h, x - 1, y), E = gat(in, w, h, x, y), F = gat(in, w, h, x + 1, y);
            int G = gat(in, w, h, x - 1, y + 1), H = gat(in, w, h, x, y + 1), I = gat(in, w, h, x + 1, y + 1);
            int o[9];
            if (B != H && D != F) {
                o[0] = D == B ? D : E;
                o[1] = (D == B && E != C) || (B == F && E != A) ? B : E;
                o[2] = B == F ? F : E;
                o[3] = (D == B && E != G) || (D == H && E != A) ? D : E;
                o[4] = E;
                o[5] = (B == F && E != I) || (H == F && E != C) ? F : E;
                o[6] = D == H ? D : E;
                o[7] = (D == H && E != I) || (H == F && E != G) ? H : E;
                o[8] = H == F ? F : E;
            } else for (int k = 0; k < 9; k++) o[k] = E;
            for (int k = 0; k < 9; k++) out[(3 * y + k / 3) * 3 * w + 3 * x + k % 3] = (uint8_t)o[k];
        }
}
/* build a smoothed, bold glyph mask at scale 2, 3 or 4 */
U void make_tile(Tile *t, unsigned char ch, int sc, int bold) {
    uint8_t g[8][5], a[16 * 10], b[32 * 20];
    glyph1(ch, g);
    int w = 5, h = 8;
    memset(t, 0, sizeof *t);
    if (sc == 3) { scale3x(&g[0][0], 5, 8, b); w = 15; h = 24; }
    else if (sc == 4) { scale2x(&g[0][0], 5, 8, a); scale2x(a, 10, 16, b); w = 20; h = 32; }
    else { scale2x(&g[0][0], 5, 8, b); w = 10; h = 16; }
    t->w = w + bold; t->h = h;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (b[y * w + x]) for (int k = 0; k <= bold; k++) t->m[y][x + k] = 1;
}
/* stamp a tile into the mask, bottom-centre at (cx, by), scaled sx, sy */
U void tile_mk(const Tile *t, float cx, float by, float sx, float sy) {
    int W = (int)(t->w * sx + 0.5f), H = (int)(t->h * sy + 0.5f);
    int x0 = (int)(cx - W * 0.5f), y0 = (int)by - H;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int u = (int)(x / sx), v = (int)(y / sy);
            if (u < t->w && v < t->h && t->m[v][u]) mk_px(x0 + x, y0 + y);
        }
}
/* outer ring of the current mask (distance d0 < d <= d1, chebyshev-ish
 * round kernel) painted in colour c, optionally offset (drop shadow tint) */
U void mk_ring(int d1, int dx, int dy, uint8_t c, const uint8_t *tint) {
    if (mbx1 < 0) return;
    for (int y = mby0 - d1 + dy; y <= mby1 + d1 + dy; y++)
        for (int x = mbx0 - d1 + dx; x <= mbx1 + d1 + dx; x++) {
            if ((unsigned)x >= TW || (unsigned)y >= TH) continue;
            int hit = 0;
            for (int j = -d1; j <= d1 && !hit; j++)
                for (int i = -d1; i <= d1 && !hit; i++) {
                    if (i * i + j * j > d1 * d1 + d1 / 2) continue;
                    int mx = x - dx + i, my = y - dy + j;
                    if ((unsigned)mx < TW && (unsigned)my < TH && mk[my][mx]) hit = 1;
                }
            if (!hit) continue;
            if (tint) tint_px(x, y, tint);
            else tk_fb[y][x] = c;
        }
}

/* ========================================================================== */
/* the scene                                                                  */
/* ========================================================================== */
#define PVX 55           /* preview feet x */
#define PVY 117          /* preview feet y */
#define F_FAST 58        /* cycling speeds up */
#define F_FINAL 100      /* cycling stops on Kevin */
#define F_CLICK 104      /* EYES [+] */
#define F_ZOOM 109       /* snap zoom to the close-up */
#define F_EYE 113        /* the third eye opens */
#define F_CAP 118

static TkChibi kevin(void) {
    TkChibi c;
    memset(&c, 0, sizeof c);
    c.skin = PAL_PAINT(PAINT_MINT, 1); c.hair = PAL_PAINT(PAINT_MINT, 1); c.outfit = PAL_PAINT(PAINT_LEMON, 1);
    c.eyes = PAL_PAINT(PAINT_SKY, 1); c.hair_style = TKH_BALD; c.tail = TKT_PUP; c.blush = 1;
    c.ears = TKEAR_PUP; c.fur2 = PAL_PAINT(PAINT_WHITE, 1); c.fluff = 1;
    return c;
}
/* look number k in the cycle: gets more absurd as k grows */
static TkChibi look(int k) {
    const TkChibi *cast[] = { &TK_RIVET, &TK_VEX, &TK_FUZZ, &TK_IMP, &TK_GOTH, &TK_MAYOR, &TK_NANA, &TK_PIP, &TK_HERO };
    TkChibi c;
    if (k % 5 == 3) c = *cast[(k / 5) % 9];
    else c = tk_random_citizen((uint32_t)(k * 13 + 5), k > 14 && (k & 1));
    if (k > 12) {             /* "Literally anyone." escalation */
        c.horns = (uint8_t)(k % 3);
        c.wings = (uint8_t)((k / 2) % 3);
        c.halo = (k % 4) == 1;
        c.hat = (uint8_t)(k % 4);
        if (k % 5 == 0) { c.skin = PAL_PAINT(PAINT_SLIME, 1); c.hair = PAL_PAINT(PAINT_HELLFIRE, 1); }
        if (k % 7 == 4) { c.skin = PAL_PAINT(PAINT_SKY, 1); c.hair_style = TKH_MOHAWK; c.halo = 1; c.horns = 2; c.wings = 2; }
        if (k % 9 == 6) { c.ears = TKEAR_BUNNY; c.hat = 3; c.skin = PAL_PAINT(PAINT_WHITE, 1); c.fluff = 1; }  /* space marine bunny */
    } else if (k > 5) {
        c.horns = k % 4 == 2; c.wings = k % 5 == 1 ? 2 : 0; c.hat = k % 6 == 4 ? 1 : 0;
    }
    if (c.ears == TKEAR_HUMAN && k % 3) { c.ears = TKEAR_CAT; c.fluff = 1; }
    return c;
}
static const char *species(const TkChibi *c) {
    if (c->horns && c->wings == 1 && c->ears == TKEAR_HUMAN) return "IMP";
    switch (c->ears) {
    case TKEAR_CAT: return "CAT"; case TKEAR_FOX: return "FOX"; case TKEAR_BUNNY: return "BUNNY";
    case TKEAR_BEAR: return "BEAR"; case TKEAR_PUP: return "PUP"; case TKEAR_MOUSE: return "MOUSE";
    case TKEAR_SHEEP: return "SHEEP"; case TKEAR_OWL: return "OWL"; case TKEAR_HUMAN: return "HUMAN";
    default: return "???";
    }
}

/* ---- backdrop -------------------------------------------------------------- */
static void backdrop(int f) {
    static const uint8_t R[4] = { PAL_RAMP(RAMP_MINT, 7), PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_SKY, 6) };
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float L = (x * 0.4f + y) / 160.0f * 3.2f;
            uint8_t c = qtone(R, 4, L, x, y);
            if (((x + y + f) / 12) & 1) c = cmap(255, 255, 255, 3)[c];
            tk_fb[y][x] = c;
        }
    const uint8_t *bk[2] = { cmap(255, 255, 255, 5), cmap(255, 220, 240, 6) };
    for (int i = 0; i < 9; i++) {
        uint32_t s = tk_hash((uint32_t)i * 557u + 3u);
        float r = 6 + (s % 10);
        float x = (float)((s >> 4) % 260) - 10, y = fmodf((float)((s >> 12) % 200) - f * 0.4f + 400.0f, 200.0f) - 20;
        tint_ellipse(x, y, r, r, bk[i & 1], 0.4f, 16);
    }
}

/* ---- widgets ------------------------------------------------------------------ */
static void mk_rrect(float x, float y, float w, float h, float r) {
    for (int j = (int)y; j < (int)(y + h); j++)
        for (int i = (int)x; i < (int)(x + w); i++) {
            float dx = fmaxf(fmaxf(x + r - (i + 0.5f), (i + 0.5f) - (x + w - r)), 0);
            float dy = fmaxf(fmaxf(y + r - (j + 0.5f), (j + 0.5f) - (y + h - r)), 0);
            if (dx * dx + dy * dy <= r * r) mk_px(i, j);
        }
}
static void pill(int x, int y, int w, int h, const uint8_t *r4, uint8_t ol) {
    mk_rrect((float)x, (float)y, (float)w, (float)h, h * 0.5f);
    pillow(r4, ol, 2.0f, 0.4f);
}
static void label(int x, int y, const char *s) { tk_text(x, y, s, PAL_UI_PINK_D, 1); }
static void slider(int x, int y, int w, float v, int heart, int f) {
    static const uint8_t TR[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7) };
    pill(x, y + 2, w, 5, TR, PAL_UI_PINK_D);
    int fill = (int)((w - 4) * v);
    tk_rect(x + 2, y + 4, fill, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_rect(x + 2, y + 3, fill, 1, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    int kx = x + 2 + fill;
    if (heart) {
        float s = 1.0f + 0.15f * sinf(f * 0.4f);
        float tip = y + 9.5f, w2 = 4.5f * s, h2 = 8 * s;
        for (int yy = (int)(tip - h2 - 2); yy <= (int)tip + 1; yy++)
            for (int xx = (int)(kx - w2 - 2); xx <= (int)(kx + w2 + 2); xx++) {
                float u = (xx + 0.5f - kx) / w2 * 1.139f, vv = -1.0f + (tip - (yy + 0.5f)) / h2 * 2.25f;
                float a = u * u + vv * vv - 1.0f;
                if (a * a * a - u * u * vv * vv * vv <= 0.0f) mk_px(xx, yy);
            }
        static const uint8_t HR[4] = { PAL_PAINT(PAINT_CHERRY, 0), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_ROSE, 2), PAL_WHITE };
        pillow(HR, PAL_PAINT(PAINT_BLOOD, 0), 2.0f, 0.3f);
    } else {
        mk_ellipse((float)kx, y + 4.5f, 4, 4);
        static const uint8_t KN[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_WHITE, PAL_WHITE };
        pillow(KN, PAL_UI_PINK_D, 2.0f, 0.4f);
    }
}
static void cursor(int x, int y, int click) {
    static const char *AR[] = {
        "k........", "kk.......", "kwk......", "kwwk.....", "kwwwk....", "kwwwwk...", "kwwwwwk..",
        "kwwwwwwk.", "kwwwwkkkk", "kwkkwk...", "kk..kwk..", "k....kwk.", ".....kk..", NULL };
    uint8_t m[128] = { 0 };
    m['k'] = PAL_RAMP(RAMP_VAPE, 1); m['w'] = PAL_WHITE;
    const uint8_t *sh = cmap(60, 30, 90, 6);
    for (int j = 0; AR[j]; j++)
        for (int i = 0; AR[j][i]; i++) if (AR[j][i] != '.') tint_px(x + i + 2, y + j + 2, sh);
    tk_sprite(AR, x, y, m, 1, 0);
    tk_px(x + 1, y + 3, PAL_RAMP(RAMP_PASTEL_PINK, 7));
    if (click > 0 && click < 7) {
        tk_ring(x, y, 2 + click * 2, click < 4 ? PAL_WHITE : PAL_PAINT(PAINT_BUBBLEGUM, 2));
        if (click < 3) tk_star(x, y, 2 + click, PAL_UI_GOLD);
    }
}
static void cursor_pos(int f, int *x, int *y) {
    static const int K[][3] = {
        { 0, 250, 150 }, { 10, 170, 52 }, { 24, 208, 50 }, { 36, 150, 66 }, { 46, 186, 69 },
        { 58, 214, 64 }, { 70, 168, 92 }, { 82, 200, 66 }, { 94, 150, 104 }, { 101, 199, 116 },
        { F_CLICK, 199, 116 }, { 150, 199, 116 } };
    int n = (int)(sizeof K / sizeof K[0]);
    for (int i = 0; i < n - 1; i++)
        if (f >= K[i][0] && f < K[i + 1][0]) {
            float t = tk_ease((float)(f - K[i][0]) / (float)(K[i + 1][0] - K[i][0]));
            *x = tk_lerpi(K[i][1], K[i + 1][1], t);
            *y = tk_lerpi(K[i][2], K[i + 1][2], t);
            return;
        }
    *x = K[n - 1][1]; *y = K[n - 1][2];
}

/* the third eye, drawn in the chibi renderer's own eye style.
 * S = chibi size, bob = its animation clock. state: 0 open, 1 closed,
 * 2.. = popping open (scale) */
static void ell(float cx, float cy, float rx, float ry, uint8_t c) {
    for (int y = (int)floorf(cy - ry - 1); y <= (int)ceilf(cy + ry + 1); y++)
        for (int x = (int)floorf(cx - rx - 1); x <= (int)ceilf(cx + rx + 1); x++) {
            float fx = (x + 0.5f - cx) / rx, fy = (y + 0.5f - cy) / ry;
            if (fx * fx + fy * fy <= 1.0f) tk_px(x, y, c);
        }
}
static void third_eye(int x, int yf, int S, int bob, uint8_t iris, uint8_t fur, float open) {
    float hb = sinf(bob * 0.16f) * 0.45f;
    float ex = x + 0.5f * 0, ey = yf + (12.2f + hb - 4.6f - 34.1f) * S + 0.5f * S;
    float s = S;
    uint8_t k = PAL_PAINT(PAINT_JET, 0);
    /* a little furrowed patch so it sits IN the forehead */
    ell(ex, ey + 0.3f * s, 2.3f * s, 2.6f * s, fur);
    if (open <= 0.05f) {           /* closed: a sleepy lash line */
        ell(ex, ey + 0.4f * s, 1.7f * s, 0.45f * s, k);
        return;
    }
    float o = open > 1 ? 1 : open;
    ell(ex, ey, 1.75f * s, 2.25f * s * o, k);
    if (o > 0.5f) {
        ell(ex, ey + 0.35f * s * o, 1.32f * s, 1.75f * s * o, iris);
        uint8_t lt = (iris >= PAL_PAINT_BASE && iris < PAL_RAMP_BASE && (iris - PAL_PAINT_BASE) % 3 < 2) ? (uint8_t)(iris + 1) : PAL_WHITE;
        ell(ex, ey + 1.0f * s * o, 1.05f * s, 0.95f * s * o, lt);
        ell(ex, ey + 0.2f * s * o, 0.7f * s, 0.7f * s * o, k);
        ell(ex - 0.7f * s, ey - 0.85f * s * o, 0.6f * s, 0.6f * s, PAL_WHITE);
        ell(ex + 0.65f * s, ey + 0.95f * s * o, 0.3f * s, 0.3f * s, PAL_WHITE);
    }
}

/* ---- the editor ---------------------------------------------------------------- */
static void editor(int f, int oy, int k, const TkChibi *c) {
    int wx = 4, wy = 2 + oy, ww = 232, wh = 130;
    /* window: drop shadow, puffy body, title bar */
    mk_rrect((float)wx, (float)wy, (float)ww, (float)wh, 8);
    mk_ring(1, 3, 4, 0, cmap(40, 70, 90, 7)); mk_reset();
    mk_rrect((float)wx, (float)wy, (float)ww, (float)wh, 8);
    static const uint8_t WB[4] = { PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_UI_PINK, PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_WHITE };
    pillow(WB, PAL_UI_PINK_D, 3.0f, 0.3f);
    {   /* title */
        float s = 1.0f;
        int tx = wx + 8, ty = wy + 3;
        (void)s;
        tk_heart(tx + 3, ty + 4, 1, PAL_WHITE);
        tk_text_outline(tx + 10, ty, "CITIZEN CREATOR", PAL_WHITE, PAL_UI_PINK_D, 1);
        for (int i = 0; i < 3; i++) {
            static const int BC[3] = { PAINT_LEMON, PAINT_MINT, PAINT_CHERRY };
            mk_ellipse((float)(wx + ww - 30 + i * 9), (float)(wy + 7), 3, 3);
            uint8_t r4[4] = { PAL_PAINT(BC[i], 0), PAL_PAINT(BC[i], 1), PAL_PAINT(BC[i], 2), PAL_WHITE };
            pillow(r4, PAL_UI_PINK_D, 1.5f, 0.3f);
        }
        tk_text(wx + ww - 70, wy + 4, "#0042", PAL_UI_PINK_D, 1);
    }
    /* tabs */
    static const char *TABS[6] = { "HAIR", "FACE", "EYES", "EARS", "HORN", "HATS" };
    int tab = k < 0 ? 2 : k % 6;
    for (int i = 0; i < 6; i++) {
        int tx = wx + 6 + i * 37, ty = wy + 15;
        int act = i == tab;
        static const uint8_t TA[4] = { PAL_RAMP(RAMP_PEACH, 7), PAL_UI_CREAM, PAL_WHITE, PAL_WHITE };
        static const uint8_t TI[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_WHITE };
        mk_rrect((float)tx, (float)(ty + (act ? 0 : 2)), 35, act ? 14.0f : 11.0f, 4);
        pillow(act ? TA : TI, PAL_UI_PINK_D, 2.0f, 0.2f);
        tk_text(tx + 6, ty + (act ? 3 : 4), TABS[i], act ? PAL_UI_PINK_D : PAL_WHITE, 1);
    }
    /* content panel */
    int cy0 = wy + 28;
    tk_rect(wx + 4, cy0, ww - 8, wh - 32, PAL_UI_CREAM);
    tk_rect(wx + 4, cy0, ww - 8, 1, PAL_UI_PINK_D);
    tint_rect(wx + 4, cy0 + 1, ww - 8, 2, cmap(200, 120, 150, 4), 16);
    /* preview pane: gradient, spotlight, turntable */
    int pvx = wx + 6, pvy = cy0 + 2, pvw = 90, pvh = wh - 36;
    static const uint8_t PG[4] = { PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_VAPE, 7), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    for (int y = 0; y < pvh; y++)
        for (int x = 0; x < pvw; x++) tk_px(pvx + x, pvy + y, qtone(PG, 4, y / (float)pvh * 3.2f, pvx + x, pvy + y));
    tk_frame(pvx - 1, pvy - 1, pvw + 2, pvh + 2, PAL_UI_PINK_D);
    const uint8_t *spot = cmap(255, 255, 240, 5);
    for (int y = pvy; y < pvy + pvh; y++) {
        float t = (y - pvy) / (float)pvh;
        int half = (int)(10 + t * 30);
        for (int x = PVX - half; x <= PVX + half; x++) if (BY4[y & 3][x & 3] < (x == PVX - half || x == PVX + half ? 8 : 16)) tint_px(x, y, spot);
    }
    mk_ellipse(PVX, (float)PVY + 1 + oy, 32, 6);
    static const uint8_t PD[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_WHITE };
    pillow(PD, PAL_UI_PINK_D, 2.5f, 0.6f);
    for (int i = 0; i < 6; i++) {               /* orbiting sparkles */
        float a = f * 0.09f + i * 1.047f;
        int sx = PVX + (int)(cosf(a) * 36), sy = PVY - 40 + (int)(sinf(a) * 8) + oy;
        tk_star(sx, sy, ((f + i * 5) / 4) % 3, i & 1 ? PAL_WHITE : PAL_UI_GOLD);
    }
    /* the citizen: boing + turntable flip on every change */
    int changed_age = 99;
    if (f >= 12 && f < F_FINAL) changed_age = f < F_FAST ? (f - 12) % 4 : (f - F_FAST) % 3;
    if (f >= F_FINAL && f < F_FINAL + 4) changed_age = f - F_FINAL;
    int pyo = changed_age == 0 ? 3 : changed_age == 1 ? -2 : 0;
    int expr = TKE_NORMAL, mouth = TKM_SMILE;
    if (k >= 0) {
        static const int E[] = { TKE_HAPPY, TKE_NORMAL, TKE_SPARKLE, TKE_SMUG, TKE_LOVE, TKE_NORMAL, TKE_ANGRY };
        static const int M[] = { TKM_OPEN, TKM_SMILE, TKM_CAT, TKM_SMILE, TKM_OPEN, TKM_O, TKM_FANGS };
        expr = E[k % 7]; mouth = M[k % 7];
    }
    tint_ellipse(PVX, PVY + 1.0f + oy, 18, 3, cmap(120, 50, 100, 6), 0.4f, 16);
    tk_chibi(PVX, PVY + pyo + oy, c, 2, expr, mouth, f, k >= 0 ? (k & 1) : 0);
    if (changed_age < 3) tk_sparkles(PVX, PVY - 40 + oy, changed_age + 3, (uint32_t)k * 7u, 26);

    /* right panel */
    int rx = wx + 102, ry = cy0 + 5;
    label(rx, ry, "NAME");
    static const uint8_t FW[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_WHITE, PAL_WHITE, PAL_WHITE };
    mk_rrect((float)rx + 34, (float)ry - 3, 88, 13, 4);
    pillow(FW, PAL_UI_PINK_D, 1.5f, 0.2f);
    tk_text(rx + 39, ry, "Kevin", PAL_GRAY_D, 1);
    if ((f / 8) & 1) tk_rect(rx + 39 + 30, ry - 1, 1, 9, PAL_GRAY_D);

    ry += 14;
    label(rx, ry, "KIND");
    tk_text(rx + 36, ry, "<", PAL_UI_PINK_D, 1);
    tk_text(rx + 116, ry, ">", PAL_UI_PINK_D, 1);
    const char *sp = species(c);
    int nd = changed_age == 0 ? -2 : 0;
    tk_text(rx + 79 - tk_text_w(sp, 1) / 2, ry + nd, sp, PAL_GRAY_D, 1);

    ry += 13;
    label(rx, ry + 3, "FUR");
    static const int SW[16] = { PAINT_WHITE, PAINT_JET, PAINT_CHERRY, PAINT_ROSE, PAINT_BUBBLEGUM, PAINT_TANGERINE,
                                PAINT_HONEY, PAINT_LEMON, PAINT_LIME, PAINT_MINT, PAINT_CYAN, PAINT_SKY, PAINT_COBALT,
                                PAINT_LAVENDER, PAINT_GRAPE, PAINT_CHOCOLATE };
    int sel = -1;
    for (int i = 0; i < 16; i++) if (PAL_PAINT(SW[i], 1) == c->skin) sel = i;
    if (sel < 0) sel = (k < 0 ? 9 : (k * 5) % 16);
    for (int i = 0; i < 16; i++) {
        int sx = rx + 34 + (i % 8) * 11, sy = ry - 2 + (i / 8) * 10;
        int on = i == sel;
        mk_rrect((float)sx, (float)sy - (on ? 1 : 0), 9, 8 + (on ? 1.0f : 0.0f), 2.5f);
        uint8_t r4[4] = { PAL_PAINT(SW[i], 0), PAL_PAINT(SW[i], 1), PAL_PAINT(SW[i], 2), PAL_WHITE };
        pillow(r4, on ? PAL_WHITE : PAL_UI_PINK_D, 1.5f, 0.3f);
        if (on) tk_frame(sx - 2, sy - 3, 13, 13, PAL_UI_PINK_D);
    }
    ry += 24;
    label(rx, ry, "HORNS");
    slider(rx + 36, ry - 1, 84, c->horns / 2.0f, 0, f);
    ry += 12;
    label(rx, ry, "CUTE");
    slider(rx + 36, ry - 1, 84, 1.0f, 1, f);
    ry += 12;
    label(rx, ry, "EVIL");
    float ev = k < 0 ? 0.06f : (c->horns ? 0.3f + 0.3f * c->horns : 0.1f) + (c->halo ? -0.1f : 0.0f) + (c->wings == 1 ? 0.15f : 0.0f);
    ev += 0.04f * sinf(f * 0.7f);
    slider(rx + 36, ry - 1, 84, tk_clampf(ev, 0.02f, 0.98f), 0, f);
    ry += 12;
    label(rx, ry, "EYES");
    int eyes = f >= F_CLICK + 2 ? 3 : 2;
    int pressed = f >= F_CLICK && f < F_CLICK + 4;
    static const uint8_t BT[4] = { PAL_PAINT(PAINT_BUBBLEGUM, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
    static const uint8_t BP[4] = { PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_BUBBLEGUM, 1) };
    mk_rrect((float)rx + 36, (float)ry - 2, 11, 11, 3); pillow(BT, PAL_UI_PINK_D, 2.0f, 0.3f);
    tk_text(rx + 39, ry, "-", PAL_WHITE, 1);
    char num[2] = { (char)('0' + eyes), 0 };
    int ny = (f >= F_CLICK + 2 && f < F_CLICK + 5) ? -3 : 0;
    tk_text_outline(rx + 57, ry + ny, num, eyes == 3 ? PAL_UI_GOLD : PAL_GRAY_D, eyes == 3 ? PAL_UI_PINK_D : PAL_UI_CREAM, 1);
    mk_rrect((float)rx + 72, (float)ry - 2 + pressed, 11, 11, 3); pillow(pressed ? BP : BT, PAL_UI_PINK_D, 2.0f, 0.3f);
    tk_text(rx + 75, ry + pressed, "+", PAL_WHITE, 1);
    if (f >= F_CLICK + 3) tk_text(rx + 88, ry, "(?!)", PAL_GRAY, 1);
}

/* close-up: full-screen turntable, size-4 Kevin, the third eye opens */
static void closeup(int f, const TkChibi *c) {
    int a = f - F_ZOOM;
    static const uint8_t PG[4] = { PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_VAPE, 7), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) tk_fb[y][x] = qtone(PG, 4, y / 160.0f * 3.2f, x, y);
    /* dramatic speed lines radiating from the forehead */
    int ox = 120, oyy = 34;
    const uint8_t *ln = cmap(255, 255, 255, 8);
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float dx = x - ox, dy = y - oyy, d = sqrtf(dx * dx + dy * dy);
            if (d < 40) continue;
            float s = sinf(atan2f(dy, dx) * 22.0f + (a & 1) * 0.8f);
            if (s > 0.8f) tint_px(x, y, ln);
        }
    /* spotlight + turntable */
    mk_ellipse(120, 152, 70, 12);
    static const uint8_t PD[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_WHITE };
    pillow(PD, PAL_UI_PINK_D, 4.0f, 0.6f);
    tint_ellipse(120, 152, 40, 7, cmap(120, 50, 100, 6), 0.4f, 16);
    /* snap-zoom wobble */
    int zy = a == 0 ? 6 : a == 1 ? -3 : a == 2 ? 1 : 0;
    int expr = TKE_NORMAL, mouth = TKM_SMILE;
    if (f >= F_EYE + 2 && f < F_EYE + 12) mouth = TKM_O;
    tk_chibi(120, 152 + zy, c, 4, expr, mouth, f, 0);
    float open = 0;
    if (f >= F_EYE) {
        int e = f - F_EYE;
        static const float OP[5] = { 0.3f, 1.35f, 0.85f, 1.1f, 1.0f };
        open = e < 5 ? OP[e] : 1.0f;
        if (f >= 134 && f < 137) open = 0;              /* ...it blinks. alone. */
    }
    if (f >= F_EYE) third_eye(120, 152 + zy, 4, f, c->eyes, c->skin, open);
    if (f >= F_EYE && f < F_EYE + 14) tk_sparkles(120, 40, f - F_EYE, 99, 30);
    if (f >= F_EYE && f < F_EYE + 3) tk_star(120, 49 + zy, 8 - (f - F_EYE) * 2, PAL_WHITE);
}

void scene_s05_creator(int f, int n) {
    (void)n;
    TkChibi kv = kevin();
    if (f >= F_ZOOM) {
        closeup(f, &kv);
        if (f >= F_CAP) bcap(146, "Yes, three eyes.", PAL_WHITE, PAL_UI_PINK_D, 2, f - F_CAP);
        if (f < F_ZOOM + 2) tk_post_flash(PAL_WHITE, 10 - (f - F_ZOOM) * 5);
        return;
    }
    backdrop(f);
    /* window boings up with anticipation + overshoot */
    int oy = 0;
    if (f < 11) { static const int B[11] = { 160, 120, 70, 28, -2, -9, -6, 1, 3, 1, 0 }; oy = B[f]; }
    int k;
    if (f < 12) k = 0;
    else if (f < F_FAST) k = (f - 12) / 4;
    else if (f < F_FINAL) k = 12 + (f - F_FAST) / 3;
    else k = -1;
    TkChibi c = k < 0 ? kv : look(k);
    editor(f, oy, k, &c);

    /* cursor */
    int cx, cyy;
    cursor_pos(f, &cx, &cyy);
    int click = 0;
    static const int CLK[] = { 10, 24, 36, 46, 58, 70, 82, 94, F_CLICK };
    for (unsigned i = 0; i < sizeof CLK / sizeof CLK[0]; i++) if (f >= CLK[i] && f < CLK[i] + 7) click = f - CLK[i];
    cursor(cx, cyy + oy, click);

    /* captions under the window */
    if (f >= 14 && f < F_FAST) bcap(146, "Create ANYONE.", PAL_WHITE, PAL_UI_PINK_D, 2, f - 14);
    if (f >= F_FAST + 2 && f < F_ZOOM) bcap(146, "Literally anyone.", PAL_WHITE, PAL_UI_PINK_D, 2, f - F_FAST - 2);
}

void scene_s05_creator_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    tk_sfx(t0 + 1 * fr, SFX_WHOOSH, 0.45f);
    tk_sfx(t0 + 5 * fr, SFX_BOING, 0.45f);
    /* part changes: a rising scale of blips, faster in the second half */
    for (int i = 0; i < 12; i++) tk_note(t0 + (12 + i * 4) * fr, 0.035, 72 + (i * 5) % 12, TKV_SQ25, 0.18f);
    for (int i = 0; i < 14; i++) tk_note(t0 + (F_FAST + i * 3) * fr, 0.03, 79 + (i * 7) % 12, TKV_SQ25, 0.18f);
    static const int CLK[] = { 10, 24, 36, 46, 58, 70, 82, 94 };
    for (int i = 0; i < 8; i++) tk_note(t0 + CLK[i] * fr, 0.015, 96, TKV_NOISE, 0.15f);
    tk_sfx(t0 + F_CLICK * fr, SFX_SELECT, 0.6f);
    tk_sfx(t0 + F_ZOOM * fr, SFX_WHOOSH, 0.7f);                 /* snap zoom */
    tk_slide(t0 + F_ZOOM * fr, 0.25, 50, 74, TKV_SQ50, 0.25f);  /* dun-DUN */
    tk_sfx(t0 + F_EYE * fr, SFX_POP, 1.0f);
    tk_sfx(t0 + (F_EYE + 1) * fr, SFX_SPARKLE, 0.6f);
    tk_sfx(t0 + (F_CAP + 2) * fr, SFX_FANFARE, 0.4f);
    tk_note(t0 + 134 * fr, 0.05, 91, TKV_SINE, 0.25f);         /* the lone blink: plink */
}
