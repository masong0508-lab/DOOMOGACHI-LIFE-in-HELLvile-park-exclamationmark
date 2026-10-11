/* s03_hahaland.c - "Welcome to HaHaland!"
 *
 * A long parallax camera pan across the sugariest town on any handheld:
 * banded sky with sun bloom and drifting clouds, hazy far hills, a row of
 * fluffy trees, then the street: plush toy cottages with glowing heart
 * windows and heart-shaped chimney smoke, lollipop trees, a sparkling
 * fountain, Nana's bakery with a giant bouncing cupcake on the roof, and
 * critter-folk citizens waving, hopping and holding heart balloons. Our fox
 * hero waves from right in front of the camera. Sunbeams drift over it all.
 * "Population: 500 very nice people" ... the pan brakes (record scratch) at
 * a big ugly wall: "HELLVILE - KEEP OUT" (someone taped a "pls <3" note on
 * it). The sky behind it is the colour of a bad day. Red eyes pop up over
 * the top one by one; the citizen nearest the wall waves hello; the big
 * cyclops waves back. "(and 500 not-so-nice people next door)"
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
#define GROUND 114
#define FEET 132
#define WALL_X 548
#define CAM_END 420
#define F_STOP 96         /* pan brakes at the wall */
#define F_EYES 102        /* red eyes start popping up */

static float cam;
static int SX(float wx) { return (int)floorf(wx - cam); }

/* 4-tone ramp + outline for any palette colour (paint / skin / ramp) */
static void ramp_of(uint8_t c, uint8_t r[4], uint8_t *ol) {
    if (c >= PAL_PAINT_BASE && c < PAL_RAMP_BASE) {
        int base = c - (c - PAL_PAINT_BASE) % 3;
        r[0] = (uint8_t)base; r[1] = (uint8_t)(base + 1); r[2] = (uint8_t)(base + 2); r[3] = PAL_WHITE;
        if (c == base + 2) { r[0] = (uint8_t)(base + 1); r[1] = (uint8_t)(base + 2); }
        *ol = (uint8_t)base;
        int l = (TK_PAL[*ol][0] * 3 + TK_PAL[*ol][1] * 6 + TK_PAL[*ol][2]) / 10;
        if (l > 150) *ol = PAL_GRAY;
        else if (l > 95) *ol = PAL_GRAY_D;
    } else if (c >= PAL_RAMP_BASE) {
        int k = (c - PAL_RAMP_BASE) % 8;
        int lo = k > 1 ? c - 1 : c;
        r[0] = (uint8_t)lo; r[1] = c; r[2] = (uint8_t)(k < 7 ? c + 1 : c); r[3] = (uint8_t)(k < 6 ? c + 2 : PAL_WHITE);
        *ol = (uint8_t)(k > 2 ? c - 3 : c - k);
    } else {
        r[0] = PAL_GRAY; r[1] = PAL_GRAY_L; r[2] = PAL_WHITE; r[3] = PAL_WHITE; *ol = PAL_GRAY_D;
    }
}
/* A citizen waving its paw: the chibi's own arm on the far side of the tail
 * is erased (background restored) and a raised arm is painted instead. */
static uint8_t wave_save[60][40];
static void chibi_wave(int x, int yf, const TkChibi *c, int S, int expr, int mouth, int bob, int flip, int phase) {
    /* design-space region of the arm: x 2..6.0, y 21.6..30.0 */
    int rx0 = 2, rx1 = 6;
#define DSX(dx) (flip ? x + (int)floorf((12.0f - (dx)) * S) - 1 : x + (int)floorf(((dx) - 12.0f) * S))
#define DSY(dy) (yf + (int)floorf(((dy) - 34.1f) * S))
    int ax0 = flip ? DSX(rx1) + 1 : DSX(rx0), ax1 = flip ? DSX(rx0) : DSX(rx1) - 1;
    int ay0 = DSY(21.6f), ay1 = DSY(30.0f);
    for (int j = ay0; j <= ay1 && j - ay0 < 60; j++)
        for (int i = ax0; i <= ax1 && i - ax0 < 40; i++) wave_save[j - ay0][i - ax0] = tk_get(i, j);
    tk_chibi(x, yf, c, S, expr, mouth, bob, flip);
    for (int j = ay0; j <= ay1 && j - ay0 < 60; j++)
        for (int i = ax0; i <= ax1 && i - ax0 < 40; i++)
            if ((unsigned)i < TW && (unsigned)j < TH) tk_fb[j][i] = wave_save[j - ay0][i - ax0];
    /* raised arm from the shoulder up to a waving paw */
    float t = (float)bob;
    float hb = sinf(t * 0.16f) * 0.45f;
    float px = phase ? 2.6f : 4.4f, py = (phase ? 16.6f : 15.6f) + hb;
    float sx = 6.6f, sy = 23.2f;
    for (int k = 0; k <= 6; k++) {
        float u = k / 6.0f;
        float dx = sx + (px - sx) * u, dy = sy + (py - sy) * u;
        mk_ellipse((float)DSX(dx) + 0.5f * S, (float)DSY(dy) + 0.5f * S, 1.5f * S, 1.5f * S);
    }
    mk_ellipse((float)DSX(px) + 0.5f * S, (float)DSY(py) + 0.5f * S, 2.0f * S, 2.1f * S);
    uint8_t r4[4], ol;
    ramp_of(c->skin, r4, &ol);
    pillow(r4, ol, 1.2f * S, 0.4f);
    tk_rect(DSX(px), DSY(py + 0.6f), S > 1 ? 2 : 1, S > 1 ? 2 : 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
#undef DSX
#undef DSY
}

/* ---- sky, distance, light ------------------------------------------------ */
static void cloud(float x, float y, float s) {
    mk_ellipse(x, y + 2.2f * s, 13 * s, 4.2f * s);
    mk_ellipse(x - 6.5f * s, y + 0.5f * s, 5.5f * s, 4.5f * s);
    mk_ellipse(x + 1.5f * s, y - 2.5f * s, 7.5f * s, 6.5f * s);
    mk_ellipse(x + 9 * s, y + 0.8f * s, 5 * s, 4.2f * s);
    static const uint8_t CL[4] = { PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE, PAL_WHITE };
    pillow(CL, PAL_RAMP(RAMP_VAPE, 6), 2.5f, 1.1f);
}
static float hill_far(float x) { return 88 + 7 * sinf(x * 0.027f + 0.5f) + 4 * sinf(x * 0.071f + 1.3f); }

static void sky_and_far(int f, int wall_sx) {
    static const uint8_t SK[5] = { PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7),
                                   PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    static const uint8_t HS[5] = { PAL_RAMP(RAMP_VAPE, 0), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_BLOOD, 2),
                                   PAL_RAMP(RAMP_BLOOD, 4), PAL_RAMP(RAMP_LAVA, 4) };
    for (int y = 0; y < GROUND; y++)
        for (int x = 0; x < TW; x++) {
            float L = y / 112.0f * 4.2f;
            uint8_t c = qtone(SK, 5, L, x, y);
            float edge = wall_sx - 5 + 5 * sinf(y * 0.21f + f * 0.12f) + 3 * sinf(y * 0.5f - f * 0.2f);
            float dh = x - edge;
            if (dh > -3) {
                int lvl = dh >= 3 ? 16 : (int)((dh + 3) * 16 / 6);
                if (lvl >= 16 || BY4[y & 3][x & 3] < lvl) {
                    float g = (y - 42) / 15.0f;
                    float flick = 0.3f * sinf(f * 0.3f + x * 0.07f);
                    c = qtone(HS, 5, 0.7f + 2.6f * expf(-g * g) + flick, x, y);
                }
            }
            tk_fb[y][x] = c;
        }
    /* sun bloom (upper left) */
    tint_ellipse(20, 6, 110, 70, cmap(255, 250, 225, 3), 0.7f, 16);
    tint_ellipse(20, 6, 45, 30, cmap(255, 250, 230, 6), 0.6f, 16);
    /* clouds (very far) */
    for (int i = 0; i < 8; i++) {
        float x = i * 95.0f + 30 - cam * 0.12f + f * 0.12f;
        float y = 12 + (i * 29) % 34;
        if (x > -40 && x < wall_sx - 10) cloud(x, y, 0.75f + (i & 1) * 0.25f);
    }
    /* little birds flapping by */
    for (int i = 0; i < 3; i++) {
        int bx = (int)(fmodf(i * 97.0f + f * (0.9f + i * 0.25f) - cam * 0.4f + 900, 330) - 50);
        int by = (int)(26 + i * 12 + 3 * sinf(f * 0.1f + i));
        if (bx > wall_sx - 12) continue;
        int up = ((f / 4 + i) & 1) ? 2 : 0;
        uint8_t c = PAL_RAMP(RAMP_VAPE, 3);
        tk_px(bx, by, c); tk_px(bx - 1, by - 1, c); tk_px(bx + 1, by - 1, c);
        tk_px(bx - 2, by - up, c); tk_px(bx + 2, by - up, c);
        tk_px(bx - 3, by - up + (up ? 0 : 1), c); tk_px(bx + 3, by - up + (up ? 0 : 1), c);
    }
    /* hell smoke over the wall */
    if (wall_sx < TW + 30) {
        for (int i = 0; i < 9; i++) {
            float ph = fmodf(f * 0.6f + i * 17.0f, 80.0f);
            float x = wall_sx + 6 + i * 14 + 6 * sinf(f * 0.05f + i);
            float y = 40 - ph * 0.5f;
            float r = 6 + ph * 0.12f;
            tint_ellipse(x, y, r, r * 0.8f, cmap(30, 10, 25, 6), 0.5f, 16 - (int)(ph / 6));
        }
    }
    /* far hills with tiny houses (hazy) */
    layer_begin();
    for (int x = 0; x < TW; x++) {
        float wx = x + cam * 0.3f;
        int h = (int)hill_far(wx);
        int hell = x > wall_sx - 4;
        for (int y = h; y < GROUND; y++) {
            uint8_t c = y == h ? PAL_RAMP(RAMP_MINT, 7) : (y - h < 6 ? PAL_RAMP(RAMP_MINT, 6) : PAL_RAMP(RAMP_MINT, 5));
            if (hell) c = y == h ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_RAMP(RAMP_VAPE, 1);
            tk_px(x, y, c);
        }
    }
    for (int i = 0; i < 12; i++) {
        float wx = i * 47.0f + 10;
        int x = (int)(wx - cam * 0.3f);
        if (x < -6 || x > wall_sx - 8) continue;
        int g = (int)hill_far(wx) + 3;
        if (i % 3 == 2) {          /* round tree */
            tk_circle(x, g - 5, 4, PAL_RAMP(RAMP_MINT, 4)); tk_circle(x - 1, g - 6, 2, PAL_RAMP(RAMP_MINT, 6));
        } else {                   /* tiny cottage */
            tk_rect(x - 3, g - 5, 7, 5, PAL_UI_CREAM);
            tk_tri(x - 4, g - 5, x + 4, g - 5, x, g - 9, PAL_PAINT(i & 1 ? PAINT_ROSE : PAINT_SKY, 1));
            tk_px(x, g - 3, PAL_UI_GOLD);
        }
    }
    layer_end(cmap(200, 225, 245, 6));
    /* mid layer: a row of fluffy trees (lightly hazed) */
    layer_begin();
    for (int i = 0; i < 16; i++) {
        float wx = i * 41.0f + (i * 13) % 17;
        float x = wx - cam * 0.6f;
        if (x < -20 || x > wall_sx - 6) continue;
        float s = 0.9f + (i % 3) * 0.2f, sw = 0.6f * sinf(f * 0.06f + i);
        float g = GROUND + 1;
        tk_rect((int)x - 1, (int)(g - 6 * s), 3, (int)(6 * s), PAL_RAMP(RAMP_WOOD, 3));
        mk_ellipse(x + sw, g - 15 * s, 8 * s, 7 * s);
        mk_ellipse(x - 6 * s + sw, g - 10 * s, 5.5f * s, 5 * s);
        mk_ellipse(x + 6 * s + sw, g - 10 * s, 5.5f * s, 5 * s);
        static const uint8_t TR[4] = { PAL_RAMP(RAMP_GRASS, 4), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 7) };
        pillow(TR, PAL_RAMP(RAMP_GRASS, 3), 3.0f, 0.8f);
    }
    layer_end(cmap(205, 235, 240, 4));
}

/* ---- the street ---------------------------------------------------------- */
static void ground(int f) {
    static const uint8_t GR[3] = { PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 7) };
    for (int y = GROUND; y < TH; y++)
        for (int x = 0; x < TW; x++) tk_fb[y][x] = qtone(GR, 3, 2.4f - (y - GROUND) * 0.06f, x, y);
    tk_rect(0, GROUND, TW, 1, PAL_RAMP(RAMP_GRASS, 7));
    /* grass speckle (world-anchored) */
    for (int x = 0; x < TW; x++) {
        int wx = x + (int)cam;
        uint32_t s = tk_hash((uint32_t)wx * 7u);
        if (s % 5 == 0) tk_px(x, GROUND + 3 + (int)(s >> 8) % 7, PAL_RAMP(RAMP_GRASS, 5));
        if (s % 9 == 0) tk_px(x, GROUND + 2 + (int)(s >> 12) % 6, PAL_RAMP(RAMP_GRASS, 7));
    }
    /* the candy path: soft peach with a lighter centre and pebbles */
    static const uint8_t PT[4] = { PAL_RAMP(RAMP_PEACH, 4), PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PEACH, 7) };
    for (int y = 123; y < 136; y++)
        for (int x = 0; x < TW; x++) {
            float L = 3.0f - fabsf(y - 128.5f) * 0.32f;
            if (y == 123 || y == 135) L = 0;
            tk_fb[y][x] = qtone(PT, 4, L, x, y);
        }
    for (int i = 0; i < 60; i++) {
        int x = (int)(i * 19 - fmodf(cam, 19.0f));
        uint32_t s = tk_hash((uint32_t)(i + (int)(cam / 19)) * 31u);
        int y = 125 + (int)(s % 9);
        tk_rect(x, y, 3, 1, PAL_RAMP(RAMP_PEACH, 7));
        tk_px(x, y + 1, PAL_RAMP(RAMP_PEACH, 5));
    }
    (void)f;
}

static void flower(int x, int y, int col, int f, int seed) {
    int sway = (int)(1.2f * sinf(f * 0.12f + seed * 1.3f));
    tk_line(x, y + 5, x + sway, y, PAL_RAMP(RAMP_GRASS, 3));
    int fx = x + sway, fy = y - 1;
    uint8_t c = PAL_PAINT(col, 1), d = PAL_PAINT(col, 0);
    tk_px(fx - 1, fy, c); tk_px(fx + 1, fy, c); tk_px(fx, fy - 1, c); tk_px(fx, fy + 1, d);
    tk_px(fx - 1, fy + 1, d); tk_px(fx + 1, fy - 1, PAL_PAINT(col, 2));
    tk_px(fx, fy, PAL_PAINT(PAINT_LEMON, 2));
}
static void flowerbed(float wx, int w, int f) {
    int x = SX(wx);
    if (x + w < -5 || x > TW + 5) return;
    mk_ellipse(x + w / 2.0f, GROUND + 2.0f, w / 2.0f, 5);
    static const uint8_t BD[4] = { PAL_RAMP(RAMP_GRASS, 3), PAL_RAMP(RAMP_GRASS, 4), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6) };
    pillow(BD, PAL_RAMP(RAMP_GRASS, 2), 3.0f, 0.6f);
    static const int cols[] = { PAINT_ROSE, PAINT_LEMON, PAINT_LAVENDER, PAINT_SKY, PAINT_BUBBLEGUM, PAINT_PEACH };
    for (int i = 0; i < w / 5; i++) flower(x + 3 + i * 5, GROUND - 5 + (i & 1) * 3, cols[i % 6], f, i + (int)wx);
}
static void lollipop_tree(float wx, int f, int r, int p1) {
    int x = SX(wx);
    if (x < -r - 4 || x > TW + r + 4) return;
    int top = GROUND - 32;
    mk_rect(x - 1, top, 3, GROUND + 2 - top);
    static const uint8_t ST[4] = { PAL_GRAY_L, PAL_WHITE, PAL_WHITE, PAL_WHITE };
    pillow(ST, PAL_RAMP(RAMP_PASTEL_PINK, 3), 1.0f, 0.0f);
    for (int y = top + 1; y < GROUND + 2; y += 4) { tk_px(x, y, PAL_PAINT(PAINT_BUBBLEGUM, 1)); tk_px(x + 1, y + 1, PAL_PAINT(PAINT_BUBBLEGUM, 1)); }
    tk_circle(x, top, r + 1, PAL_PAINT(p1, 0));
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            float d = sqrtf((float)(i * i + j * j));
            if (d > r + 0.3f) continue;
            float a = atan2f((float)j, (float)i) + f * 0.08f;
            int band = (int)floorf((a / 6.2832f) * 2 + d * 0.3f) & 1;
            uint8_t c = band ? PAL_PAINT(p1, 1) : PAL_WHITE;
            float lit = (-i - j) / (float)r;
            if (d > r - 1.5f && lit < -0.3f) c = band ? PAL_PAINT(p1, 0) : PAL_PAINT(p1, 2);
            tk_px(x + i, top + j, c);
        }
    tk_rect(x - r / 2, top - r / 2, 2, 2, PAL_WHITE);
    tk_px(x - r / 2 + 2, top - r / 2 + 1, PAL_WHITE);
}

/* a plush toy cottage: puffy walls, puffy roof, glowing heart windows */
static void house(float wx, int w, int h, int wallp, int roofp, int f, int seed) {
    int x = SX(wx) - w / 2;
    if (x > TW + 12 || x + w < -12) return;
    int y = GROUND + 2 - h;
    int rt = y - h / 2 - 9;                       /* roof tip */
    /* chimney */
    int chx = x + w - 16;
    mk_rect(chx, rt + 8, 9, y - rt - 4);
    static const uint8_t CH[4] = { PAL_PAINT(PAINT_RUST, 1), PAL_PAINT(PAINT_RUST, 2), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PEACH, 7) };
    pillow(CH, PAL_PAINT(PAINT_RUST, 0), 1.5f, 0.3f);
    tk_rect(chx - 1, rt + 7, 11, 3, PAL_PAINT(PAINT_RUST, 0));
    tk_rect(chx, rt + 7, 9, 2, PAL_RAMP(RAMP_PEACH, 6));
    for (int i = 0; i < 3; i++) {                 /* heart-shaped smoke puffs */
        int ph = (f + i * 13 + seed * 7) % 39;
        float hx = chx + 4 + 3 * sinf(ph * 0.25f + i), hy = rt + 4 - ph * 0.9f;
        if (ph < 32) {
            float r = 1.5f + ph * 0.07f;
            mk_ellipse(hx - r * 0.6f, hy, r, r); mk_ellipse(hx + r * 0.6f, hy, r, r); mk_tri(hx - r * 1.5f, hy + 0.3f, hx + r * 1.5f, hy + 0.3f, hx, hy + r * 2);
            static const uint8_t SM[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_WHITE, PAL_WHITE };
            pillow(SM, ph < 22 ? PAL_RAMP(RAMP_PASTEL_PINK, 5) : 0, 1.5f, 0.3f);
        }
    }
    /* walls */
    mk_rect(x, y, w, h);
    uint8_t WR[4] = { PAL_PAINT(wallp, 1), PAL_PAINT(wallp, 2), PAL_PAINT(wallp, 2), PAL_WHITE };
    if (wallp == PAINT_PEACH) WR[1] = PAL_RAMP(RAMP_PEACH, 7);
    pillow(WR, PAL_PAINT(wallp, 0), 3.0f, 0.4f);
    for (int j = y + 5; j < y + h - 2; j += 6) tk_rect(x + 3, j, w - 6, 1, PAL_PAINT(wallp, 1));
    /* roof */
    mk_tri((float)x - 8, (float)y + 4, (float)(x + w + 7), (float)y + 4, (float)x + w / 2.0f, (float)rt);
    uint8_t RR[4] = { PAL_PAINT(roofp, 0), PAL_PAINT(roofp, 1), PAL_PAINT(roofp, 2), PAL_WHITE };
    pillow(RR, PAL_RAMP(RAMP_VAPE, 1), 3.5f, 0.3f);
    for (int k = 1; k < 4; k++) {                 /* scalloped shingles */
        int yy = rt + k * (y + 4 - rt) / 4;
        float half = (w / 2.0f + 7) * k / 4.0f - 2;
        for (float i = -half; i <= half - 3; i += 4) {
            int sx = (int)(x + w / 2.0f + i);
            tk_px(sx, yy, PAL_PAINT(roofp, 0)); tk_px(sx + 1, yy + 1, PAL_PAINT(roofp, 0)); tk_px(sx + 2, yy + 1, PAL_PAINT(roofp, 0)); tk_px(sx + 3, yy, PAL_PAINT(roofp, 0));
        }
    }
    /* round attic window */
    int ax = x + w / 2, ay = y - 7;
    tk_circle(ax, ay, 4, PAL_RAMP(RAMP_VAPE, 1)); tk_circle(ax, ay, 3, PAL_WHITE); tk_circle(ax, ay, 2, PAL_PAINT(PAINT_LEMON, 2));
    tk_px(ax - 1, ay - 1, PAL_WHITE);
    /* door: arched, with a heart */
    int dx = x + w / 2 - 6, dy = y + h - 21;
    mk_rect(dx, dy + 4, 12, 18); mk_ellipse(dx + 6.0f, dy + 5.0f, 6, 5);
    static const uint8_t DR[4] = { PAL_PAINT(PAINT_CHOCOLATE, 2), PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CARAMEL, 2), PAL_RAMP(RAMP_PEACH, 7) };
    pillow(DR, PAL_PAINT(PAINT_CHOCOLATE, 1), 2.0f, 0.3f);
    tk_heart(dx + 6, dy + 9, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_px(dx + 9, dy + 14, PAL_UI_GOLD);
    /* heart windows with warm light spill + flower boxes */
    for (int s = -1; s <= 1; s += 2) {
        int wxp = x + w / 2 + s * (w / 2 - 10), wyp = y + 11;
        tint_ellipse((float)wxp, (float)wyp + 2, 11, 10, cmap(255, 245, 200, 4), 0.5f, 16);
        tk_rect(wxp - 7, wyp - 6, 14, 13, PAL_RAMP(RAMP_VAPE, 1));
        tk_rect(wxp - 6, wyp - 5, 12, 11, PAL_WHITE);
        tk_rect(wxp - 5, wyp - 4, 10, 9, PAL_RAMP(RAMP_PEACH, 6));
        int on = ((f / 14 + seed + s) % 9) != 0;
        {
            float cx = wxp + 0.5f, tip = wyp + 4.5f, w = 4.6f, h = 8.0f;
            for (int yy = (int)(tip - h - 2); yy <= (int)tip + 1; yy++)
                for (int xx = (int)(cx - w - 2); xx <= (int)(cx + w + 2); xx++) {
                    float u = (xx + 0.5f - cx) / w * 1.139f, v = -1.0f + (tip - (yy + 0.5f)) / h * 2.25f;
                    float a = u * u + v * v - 1.0f;
                    if (a * a * a - u * u * v * v * v <= 0.0f) mk_px(xx, yy);
                }
            static const uint8_t GL[4] = { PAL_PAINT(PAINT_HONEY, 2), PAL_PAINT(PAINT_LEMON, 2), PAL_RAMP(RAMP_CANDY, 7), PAL_WHITE };
            static const uint8_t GO[4] = { PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_HONEY, 2), PAL_PAINT(PAINT_LEMON, 2), PAL_RAMP(RAMP_CANDY, 7) };
            pillow(on ? GL : GO, PAL_PAINT(PAINT_HONEY, 0), 1.5f, 0.3f);
        }
        tk_px(wxp - 2, wyp - 2, PAL_WHITE);
        tk_rect(wxp - 8, wyp + 7, 16, 3, PAL_PAINT(PAINT_CHOCOLATE, 2));
        tk_rect(wxp - 8, wyp + 7, 16, 1, PAL_PAINT(PAINT_CARAMEL, 2));
        for (int i = 0; i < 4; i++) {
            int bob = ((f / 8 + i + s) & 1);
            tk_rect(wxp - 7 + i * 4, wyp + 5 - bob, 2, 2, i & 1 ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_LEMON, 1));
        }
    }
}
static void bakery(float wx, int f) {
    house(wx, 66, 46, PAINT_PEACH, PAINT_CHOCOLATE, f, 3);
    int x = SX(wx);
    if (x < -60 || x > TW + 60) return;
    int y = GROUND + 2 - 46;
    /* a giant cupcake on the roof, gently bouncing */
    float sq = 0.04f * sinf(f * 0.25f);
    float cy = y - 36.0f;
    mk_tri(x - 11.0f, cy + 3, x + 11.0f, cy + 3, x, cy + 20);
    mk_rect(x - 9, (int)cy + 3, 18, 12);
    static const uint8_t WR[4] = { PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_LEMON, 2), PAL_WHITE };
    pillow(WR, PAL_PAINT(PAINT_HONEY, 0), 2.0f, 0.4f);
    for (int i = -7; i <= 7; i += 3) tk_rect(x + i, (int)cy + 5, 1, 9, PAL_PAINT(PAINT_HONEY, 1));
    mk_ellipse((float)x, cy + 1, 13 * (1 + sq), 6 * (1 - sq));
    mk_ellipse((float)x, cy - 4, 9 * (1 + sq), 5 * (1 - sq));
    mk_ellipse((float)x, cy - 8, 5 * (1 + sq), 3.5f * (1 - sq));
    static const uint8_t FR[4] = { PAL_PAINT(PAINT_BUBBLEGUM, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
    pillow(FR, PAL_RAMP(RAMP_PASTEL_PINK, 1), 2.5f, 0.4f);
    mk_ellipse((float)x, cy - 13, 3, 3);
    static const uint8_t CR[4] = { PAL_PAINT(PAINT_BLOOD, 1), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_CHERRY, 2), PAL_WHITE };
    pillow(CR, PAL_PAINT(PAINT_BLOOD, 0), 1.5f, 0.2f);
    tk_px(x - 1, (int)cy - 14, PAL_WHITE);
    static const int SP[] = { PAINT_SKY, PAINT_LEMON, PAINT_MINT, PAINT_LAVENDER };
    for (int i = 0; i < 8; i++) tk_rect(x - 10 + i * 3, (int)cy - 1 - (i % 3) * 2, 2, 1, PAL_PAINT(SP[i & 3], 1));
    /* sign */
    mk_rect(x - 22, y + 24, 44, 11);
    static const uint8_t SG[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_WHITE, PAL_WHITE };
    pillow(SG, PAL_RAMP(RAMP_PASTEL_PINK, 2), 1.5f, 0.2f);
    tk_text(x - 17, y + 26, "BAKERY", PAL_PAINT(PAINT_ROSE, 0), 1);
}
static void fountain(float wx, int f) {
    int x = SX(wx);
    if (x < -50 || x > TW + 50) return;
    int by = FEET - 8;
    static const uint8_t ST[4] = { PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6), PAL_RAMP(RAMP_STONE, 7), PAL_WHITE };
    static const uint8_t WA[4] = { PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 2), PAL_RAMP(RAMP_SKY, 7), PAL_WHITE };
    /* pillar + top bowl */
    mk_rect(x - 4, by - 34, 9, 32);
    pillow(ST, PAL_RAMP(RAMP_STONE, 3), 2.0f, 0.2f);
    mk_ellipse((float)x + 0.5f, by - 35.0f, 13, 4.5f);
    pillow(ST, PAL_RAMP(RAMP_STONE, 3), 2.0f, 0.2f);
    tk_ellipse(x, by - 37, 10, 2, PAL_PAINT(PAINT_SKY, 2));
    /* plump heart on top */
    float hb = (f / 10) & 1;
    for (int pass = 0; pass < 1; pass++) {
        float cx = (float)x + 0.5f, tip = by - 40 - hb, w = 6, h = 11;
        for (int y = (int)(tip - h - 2); y <= (int)tip + 1; y++)
            for (int xx = (int)(cx - w - 2); xx <= (int)(cx + w + 2); xx++) {
                float u = (xx + 0.5f - cx) / w * 1.139f, v = -1.0f + (tip - (y + 0.5f)) / h * 2.25f;
                float a = u * u + v * v - 1.0f;
                if (a * a * a - u * u * v * v * v <= 0.0f) mk_px(xx, y);
            }
        static const uint8_t HR[4] = { PAL_PAINT(PAINT_BUBBLEGUM, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
        pillow(HR, PAL_RAMP(RAMP_PASTEL_PINK, 1), 2.5f, 0.3f);
    }
    /* water arcs */
    for (int s = -1; s <= 1; s += 2)
        for (int k = 0; k < 16; k++) {
            float t = ((k * 5 + f * 2) % 80) / 80.0f;
            int px = x + s * (int)(6 + 23 * t), py = by - 39 + (int)(-9 * t + 38 * t * t);
            tk_px(px, py, (k & 1) ? PAL_WHITE : PAL_PAINT(PAINT_SKY, 2));
            if (k % 3 == 0) tk_px(px, py + 1, PAL_PAINT(PAINT_SKY, 1));
        }
    /* basin */
    mk_ellipse((float)x, (float)by + 1, 31, 8);
    pillow(ST, PAL_RAMP(RAMP_STONE, 3), 3.0f, 0.5f);
    mk_ellipse((float)x, (float)by - 1, 27, 4.5f);
    pillow(WA, PAL_PAINT(PAINT_COBALT, 1), 2.0f, 0.2f);
    for (int i = 0; i < 6; i++) { int rx = x - 22 + ((i * 11 + f) % 44); tk_rect(rx, by - 2 + (i & 1), 3, 1, PAL_WHITE); }
    tk_sparkles(x, by - 30, f, 33, 22);
}

/* ---- the wall ---------------------------------------------------------------- */
static void demon_head(int hx, int hy, int r, int kind, int f, int i, int a) {
    uint8_t sk = PAL_RAMP(RAMP_VAPE, 0), rim = PAL_RAMP(RAMP_BLOOD, 4);
    if (kind == 0 || kind == 2) {       /* horns */
        tk_tri(hx - r + 1, hy - 2, hx - r + 5, hy - r + 2, hx - r - 2, hy - r - 7, sk);
        tk_tri(hx + r - 1, hy - 2, hx + r - 5, hy - r + 2, hx + r + 2, hy - r - 7, sk);
        tk_px(hx - r - 2, hy - r - 7, rim); tk_px(hx + r + 2, hy - r - 7, rim);
    } else if (kind == 1) {             /* pointy ears */
        tk_tri(hx - r, hy, hx - r + 3, hy - 3, hx - r - 6, hy - 6, sk);
        tk_tri(hx + r, hy, hx + r - 3, hy - 3, hx + r + 6, hy - 6, sk);
    } else {                            /* a party hat. they heard there was a party. */
        tk_tri(hx - 4, hy - r + 1, hx + 4, hy - r + 1, hx + 1, hy - r - 10, PAL_PAINT(PAINT_GRAPE, 0));
        tk_px(hx + 1, hy - r - 11, PAL_UI_GOLD);
        tk_px(hx - 1, hy - r - 4, PAL_UI_GOLD);
    }
    tk_circle(hx, hy, r, sk);
    /* red rim light from the hell glow behind */
    for (int k = -r + 2; k <= r - 2; k++) tk_px(hx + k, hy - r + (k * k) / (2 * r) , rim);
    int blink = ((f + i * 17) % 45) < 3;
    uint8_t e = PAL_UI_RED, eh = PAL_RAMP(RAMP_LAVA, 6);
    const uint8_t *glow = cmap(255, 40, 40, 5);
    if (kind == 4) {                    /* the big cyclops */
        tint_ellipse((float)hx, (float)hy, 7, 5, glow, 0.5f, 16);
        if (!blink) { tk_ellipse(hx, hy, 3, 2, e); tk_rect(hx - 1, hy - 1, 2, 2, eh); }
        else tk_rect(hx - 3, hy, 7, 1, e);
    } else if (!blink) {
        tint_ellipse((float)hx, (float)hy, (float)r, 4, glow, 0.5f, 16);
        tk_rect(hx - 4, hy - 1, 3, 3, e); tk_px(hx - 4, hy - 1, eh);
        tk_rect(hx + 2, hy - 1, 3, 3, e); tk_px(hx + 2, hy - 1, eh);
    } else { tk_rect(hx - 4, hy + 1, 3, 1, e); tk_rect(hx + 2, hy + 1, 3, 1, e); }
    (void)a;
}
static void the_wall(int f, int eyes_age) {
    int x = SX(WALL_X);
    if (x > TW) return;
    int top = 44;
    /* peeking heads (behind the wall) */
    static const int HX[] = { 14, 36, 57, 80, 104, 126, 66 };
    static const int KIND[] = { 0, 1, 2, 4, 1, 3, 0 };
    for (int i = 0; i < 7; i++) {
        int a = eyes_age - i * 3 - (i == 6 ? 9 : 0);
        if (eyes_age < 0 || a < 0) continue;
        int rise = a == 0 ? 4 : a == 1 ? 13 : a == 2 ? 10 : 11;
        if (KIND[i] == 4) rise += 4;
        rise += ((f / 7 + i) % 6 == 0);
        int r = KIND[i] == 4 ? 10 : 6 + (i & 1);
        int hx = x + HX[i], hy = top + 6 - rise;
        if (KIND[i] == 4 && a > 26) {    /* ...the cyclops waves back */
            int ph = (f / 5) & 1, hxp = hx + r + 4 + ph * 2, hyp = hy - 13 + ph;
            uint8_t sk = PAL_RAMP(RAMP_VAPE, 0);
            for (int k = 0; k < 3; k++) tk_line(hx + r - 2, hy + 2 + k, hxp, hyp + 2 + k, sk);
            tk_circle(hxp, hyp, 3, sk);
            tk_rect(hxp - 2, hyp - 6, 1, 4, sk); tk_rect(hxp, hyp - 7, 1, 5, sk); tk_rect(hxp + 2, hyp - 6, 1, 4, sk);
            tk_px(hxp, hyp - 7, PAL_RAMP(RAMP_BLOOD, 4));
        }
        demon_head(hx, hy, r, KIND[i], f, i, a);
    }
    /* wall body: heavy dark stone blocks with bevels */
    static const uint8_t SB[4] = { PAL_RAMP(RAMP_STONE, 1), PAL_RAMP(RAMP_STONE, 2), PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 4) };
    tk_rect(x, top, TW - x + 40, GROUND + 6 - top, PAL_RAMP(RAMP_STONE, 0));
    for (int row = 0; row * 9 + top < GROUND + 6; row++) {
        int yy = top + row * 9, off = (row & 1) * 10;
        for (int bx = -off; bx < 200; bx += 20) {
            int bl = x + bx + 1, bw = 18;
            if (bl < x + 2) { bw -= x + 2 - bl; bl = x + 2; }
            if (bw <= 2 || bl > TW) continue;
            uint32_t s = tk_hash((uint32_t)(row * 31 + bx * 7));
            mk_rect(bl, yy + 1, bw, 7);
            uint8_t r4[4] = { SB[0], SB[1], (s & 3) == 0 ? SB[1] : SB[2], SB[3] };
            pillow(r4, 0, 1.5f, 0.6f);
            if ((s & 7) == 5) { tk_line(bl + 4, yy + 2, bl + 8, yy + 6, PAL_RAMP(RAMP_STONE, 0)); tk_px(bl + 9, yy + 6, PAL_RAMP(RAMP_STONE, 0)); }
            if ((s & 15) == 3) tk_rect(bl + 2, yy + 6, 5, 1, PAL_RAMP(RAMP_SICK, 3));   /* moss */
        }
    }
    /* rusty spikes along the top */
    for (int i = 0; i < 170; i += 10) {
        tk_tri(x + i, top + 1, x + i + 7, top + 1, x + i + 3, top - 8, PAL_RAMP(RAMP_WOOD, 1));
        tk_tri(x + i + 1, top + 1, x + i + 3, top + 1, x + i + 3, top - 6, PAL_RAMP(RAMP_WOOD, 3));
        tk_px(x + i + 3, top - 7, PAL_RAMP(RAMP_LAVA, 4));
    }
    /* slime drips, slowly growing */
    for (int i = 0; i < 7; i++) {
        int dx = x + 8 + i * 21, len = 6 + (int)(tk_hash((uint32_t)i + 5) % 14) + ((f / 9 + i) % 4);
        tk_rect(dx, top + 1, 2, len, PAL_RAMP(RAMP_SICK, 4));
        tk_px(dx, top + 1, PAL_RAMP(RAMP_SICK, 6));
        tk_rect(dx, top + len + 1, 2, 2, PAL_RAMP(RAMP_SICK, 3));
    }
    /* shadow edge */
    tk_rect(x, top, 2, GROUND + 6 - top, PAL_BLACK);
    tint_rect(x - 8, GROUND - 4, 8, 14, cmap(40, 20, 60, 8), 8);
    /* the sign, crooked */
    int sx = x + 18, sy = 60;
    for (int j = 0; j < 30; j++) {
        int sh = j / 10;
        tk_rect(sx - 1 + sh, sy + j, 84, 1, PAL_RAMP(RAMP_WOOD, 0));
        uint8_t c = (j % 10 == 9) ? PAL_RAMP(RAMP_WOOD, 2) : (j % 10 < 2 ? PAL_RAMP(RAMP_WOOD, 5) : PAL_RAMP(RAMP_WOOD, 4));
        tk_rect(sx + sh, sy + j, 82, 1, c);
    }
    tk_rect(sx + 3, sy + 3, 2, 2, PAL_GRAY); tk_rect(sx + 78, sy + 4, 2, 2, PAL_GRAY);
    tk_text_outline(sx + 16, sy + 4, "HELLVILE", PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 0), 1);
    tk_text_outline(sx + 17, sy + 17, "KEEP OUT", PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 0), 1);
    /* a cute note taped on by some HaHalander */
    int nx = sx + 52, ny = sy + 34;
    mk_rect(nx, ny, 32, 17);
    static const uint8_t NT[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_WHITE, PAL_WHITE };
    pillow(NT, PAL_PAINT(PAINT_BUBBLEGUM, 1), 1.0f, 0.2f);
    tk_rect(nx + 12, ny - 2, 8, 3, PAL_RAMP(RAMP_CANDY, 6));
    tk_text(nx + 3, ny + 4, "pls", PAL_PAINT(PAINT_ROSE, 0), 1);
    tk_heart(nx + 25, ny + 8, 1, PAL_PAINT(PAINT_CHERRY, 1));
    /* purple fog spilling over the top and down the face */
    for (int i = 0; i < 12; i++) {
        float ph = fmodf(f * 0.7f + i * 11.0f, 66.0f);
        float px = x + 8 + i * 12 - ph * 0.5f;
        float py = top + 3 + 1.5f * sinf(f * 0.1f + i) + (px < x + 6 ? (x + 6 - px) * 0.5f : 0);
        float r = 3.5f + (i % 2) + 1.5f * sinf(ph * 0.095f);
        mk_ellipse(px, py, r, r * 0.85f);
    }
    static const uint8_t VF[4] = { PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7) };
    pillow(VF, PAL_RAMP(RAMP_VAPE, 3), 2.0f, 0.5f);
}

/* ---- scene ----------------------------------------------------------------- */
static TkChibi townie(uint32_t i) {
    TkChibi c = tk_random_citizen(i, 0);
    c.wings = 0;
    if (c.ears == TKEAR_HUMAN) { c.ears = TKEAR_BEAR; c.fluff = 1; c.skin = PAL_PAINT(PAINT_CARAMEL, 1); }
    c.blush = 1;
    return c;
}
static void balloon(int x, int y, int f, int seed) {
    float sw = 2.0f * sinf(f * 0.09f + seed);
    float bx = x + sw, by = y - 22 + sinf(f * 0.13f + seed) * 1.5f;
    tk_line(x, y, (int)bx, (int)by + 7, PAL_GRAY);
    float tip = by + 7, w = 6, h = 12;
    for (int yy = (int)(tip - h - 2); yy <= (int)tip + 1; yy++)
        for (int xx = (int)(bx - w - 2); xx <= (int)(bx + w + 2); xx++) {
            float u = (xx + 0.5f - bx) / w * 1.139f, v = -1.0f + (tip - (yy + 0.5f)) / h * 2.25f;
            float a = u * u + v * v - 1.0f;
            if (a * a * a - u * u * v * v * v <= 0.0f) mk_px(xx, yy);
        }
    static const uint8_t BR[4] = { PAL_PAINT(PAINT_CHERRY, 0), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_CHERRY, 2), PAL_WHITE };
    pillow(BR, PAL_PAINT(PAINT_BLOOD, 0), 2.5f, 0.3f);
    tk_rect((int)bx - 3, (int)by - 3, 2, 2, PAL_WHITE);
}
static void foreground(int f) {
    /* fast parallax: picket fence, tall grass tufts and big flowers */
    float fc = cam * 1.45f;
    for (int i = 0; i < 40; i++) {
        float wx = i * 37.0f + (i * 11) % 13;
        int x = (int)(wx - fc);
        if (x < -12 || x > TW + 12) continue;
        uint32_t s = tk_hash((uint32_t)i * 97u);
        int y = 156 + (int)(s % 5);
        int sway = (int)(1.5f * sinf(f * 0.1f + i));
        for (int k = -3; k <= 3; k++) tk_line(x + k * 2, y + 6, x + k * 3 + sway, y - 6 + abs(k) * 2, k & 1 ? PAL_RAMP(RAMP_GRASS, 4) : PAL_RAMP(RAMP_GRASS, 5));
        if (s % 3 == 0) {
            static const int FC[] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_LAVENDER, PAINT_WHITE };
            int fx = x + sway + 2, fy = y - 9;
            mk_ellipse(fx - 2.0f, (float)fy, 2, 2); mk_ellipse(fx + 2.0f, (float)fy, 2, 2);
            mk_ellipse((float)fx, fy - 2.0f, 2, 2); mk_ellipse((float)fx, fy + 2.0f, 2, 2);
            int p = FC[(s >> 4) % 4];
            uint8_t r4[4] = { PAL_PAINT(p, 0), PAL_PAINT(p, 1), PAL_PAINT(p, 2), PAL_WHITE };
            pillow(r4, PAL_PAINT(p, 0), 1.5f, 0.3f);
            tk_rect(fx - 1, fy - 1, 2, 2, PAL_PAINT(PAINT_LEMON, 1));
        }
    }
}
static void light_shafts(int f, int xmax) {
    const uint8_t *m = cmap(255, 250, 225, 3);
    for (int y = 0; y < 150; y++)
        for (int x = 0; x < xmax && x < TW; x++) {
            float u = x + y * 0.75f + f * 0.25f;
            float s = sinf(u * 0.05f) * 0.6f + sinf(u * 0.021f + 2.0f) * 0.4f;
            if (s < 0.45f) continue;
            int lvl = s > 0.55f ? 16 : 8;
            if (y > 110) lvl = lvl * (150 - y) / 40;
            if (BY4[y & 3][x & 3] < lvl) tint_px(x, y, m);
        }
}

void scene_s03_hahaland(int f, int n) {
    (void)n;
    /* camera: ease-in-out pan, brakes hard at the wall with an overshoot */
    /* accelerate gently, cruise, then slam the brakes at the wall */
    float t = tk_clampf(f / (float)F_STOP, 0, 1), r = 0.3f;
    float e = t < r ? (t * t / (2 * r)) / (1 - r / 2) : (t - r / 2) / (1 - r / 2);
    cam = CAM_END * e;
    if (f >= F_STOP && f < F_STOP + 8) { static const float O[8] = { 6, 8, 6, 2, -1, -2, -1, 0 }; cam = CAM_END + O[f - F_STOP]; }
    else if (f >= F_STOP + 8) cam = CAM_END + 0.04f * (f - F_STOP - 8);   /* slow creep closer */

    int wall_sx = SX(WALL_X);
    sky_and_far(f, wall_sx);
    ground(f);

    /* scenery, back to front */
    lollipop_tree(22, f, 11, PAINT_BUBBLEGUM);
    house(78, 58, 42, PAINT_BUBBLEGUM, PAINT_ROSE, f, 1);
    flowerbed(118, 32, f);
    lollipop_tree(160, f, 9, PAINT_MINT);
    fountain(238, f);
    bakery(342, f);
    lollipop_tree(404, f, 12, PAINT_LAVENDER);
    flowerbed(412, 40, f);
    house(482, 52, 40, PAINT_MINT, PAINT_SKY, f, 2);
    the_wall(f, f - F_EYES);

    /* the townsfolk */
    struct { int wx, id, act; } C[] = {
        { 34, 3, 1 }, { 108, 11, 1 }, { 136, 17, 2 }, { 186, 24, 3 }, { 282, 31, 1 },
        { 382, 41, 2 }, { 436, 52, 1 }, { 458, 57, 0 }, { 522, 64, 4 },
    };
    for (unsigned i = 0; i < sizeof C / sizeof C[0]; i++) {
        int x = SX((float)C[i].wx);
        if (x < -24 || x > TW + 24) continue;
        TkChibi c = townie((uint32_t)C[i].id);
        int act = C[i].act;
        int hop = act == 2 ? (int)(5 * fabsf(sinf((f + i * 5) * 0.21f))) : 0;
        int expr = (i % 3 == 0) ? TKE_HAPPY : TKE_NORMAL;
        int mouth = (i & 1) ? TKM_OPEN : TKM_SMILE;
        int flip = (i == 3 || i == 7);
        tint_ellipse((float)x, FEET + 1.0f, 8, 2, cmap(60, 90, 70, 5), 0.4f, 16);
        if (act == 4) {
            /* the one by the wall: waves at the demons once they show up */
            int waving = f >= F_EYES + 10;
            if (waving) chibi_wave(x, FEET, &c, 1, TKE_HAPPY, TKM_OPEN, f + i * 9, 1, ((f + i * 2) / 5) & 1);
            else tk_chibi(x, FEET, &c, 1, TKE_NORMAL, TKM_SMILE, f + i * 9, 0);
        } else if (act == 1) chibi_wave(x, FEET - hop, &c, 1, expr, mouth, f + i * 9, flip, ((f + i * 2) / 5) & 1);
        else tk_chibi(x, FEET - hop, &c, 1, expr, mouth, f + i * 9, flip);
        if (act == 3) balloon(x + (flip ? -9 : 9), FEET - 14, f, (int)i);
    }
    /* cast cameos: Nana by her bakery, Pip by the fountain, the Mayor fluttering overhead */
    if (SX(306) > -24 && SX(306) < TW + 24) {
        tint_ellipse((float)SX(306), FEET + 1.0f, 8, 2, cmap(60, 90, 70, 5), 0.4f, 16);
        chibi_wave(SX(306), FEET, &TK_NANA, 1, TKE_HAPPY, TKM_OPEN, f, 0, (f / 6) & 1);
    }
    if (SX(206) > -24 && SX(206) < TW + 24) {
        int hop = (int)(4 * fabsf(sinf(f * 0.25f)));
        tint_ellipse((float)SX(206), FEET + 1.0f, 7 - hop * 0.5f, 2, cmap(60, 90, 70, 5), 0.4f, 16);
        tk_chibi(SX(206), FEET - hop, &TK_PIP, 1, TKE_SPARKLE, TKM_OPEN, f + 5, 0);
    }
    {
        int mx = SX(262), my = FEET - 34 - (int)(4 * sinf(f * 0.2f));
        if (mx > -24 && mx < TW + 24) {
            tk_chibi(mx, my, &TK_MAYOR, 1, TKE_HAPPY, TKM_OPEN, f, 1);
            tk_sparkles(mx, my - 14, f, 9, 16);
        }
    }
    /* our hero, close to the camera, waving hello as we pass */
    {
        float hx = 236 - cam * 1.25f + 120 * 0.25f;
        if (hx > -40 && hx < TW + 40) {
            chibi_wave((int)hx, 177, &TK_HERO, 2, TKE_HAPPY, TKM_OPEN, f, 0, (f / 4) & 1);
        }
    }
    foreground(f);
    light_shafts(f, wall_sx - 16);

    /* captions */
    uint8_t oc = PAL_RAMP(RAMP_PASTEL_PINK, 1);
    if (f >= 4 && f < 46) bcap(24, "Welcome to\nHaHaland!", PAL_WHITE, oc, 2, f - 4);
    if (f >= 50 && f < 96) bcap(24, "Population: 500\nvery nice people", PAL_WHITE, oc, 2, (f - 50) * 2);
    if (f >= 105) bcap(146, "(and 500 not-so-nice\npeople next door)", PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1), 1, (f - 105) * 2);
    if (f >= F_STOP && f < F_STOP + 3) tk_post_shake(f == F_STOP + 1 ? -2 : 1, 0);
}

void scene_s03_hahaland_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    tk_sfx(t0 + 4 * fr, SFX_FANFARE, 0.45f);
    for (int i = 0; i < 4; i++) tk_sfx(t0 + (18 + i * 17) * fr, SFX_SQUEAK, 0.2f);     /* happy citizen squeaks */
    tk_sfx(t0 + 50 * fr, SFX_SELECT, 0.45f);
    tk_sfx(t0 + F_STOP * fr, SFX_SCRATCH, 0.8f);                                    /* the brakes */
    for (int i = 0; i < 7; i++)                                                      /* eyes popping up */
        tk_note(t0 + (F_EYES + i * 3 + (i == 6 ? 9 : 0)) * fr, 0.05, 40 + (i * 5) % 9, TKV_SQ12, 0.3f);
    tk_slide(t0 + (F_EYES + 2) * fr, 1.4, 36, 33, TKV_SAW, 0.16f);                 /* low drone */
    tk_sfx(t0 + (F_EYES + 10) * fr, SFX_SQUEAK, 0.3f);                              /* "hi!" */
    tk_sfx(t0 + 106 * fr, SFX_KAZOO, 0.4f);
}
