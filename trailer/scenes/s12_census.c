/* s12_census.c - the census.   (4.0 s, 120 frames)
 *
 * f  0- 21  Close on the Wall, nose to nose: a pastel critter on one side, an
 *           edgy horned one on the other. The camera pulls back and back while
 *           a population counter rolls up ... every citizen is a tiny plush
 *           critter that breathes, blinks and hops.
 * f 22- 45  "1000 CITIZENS." - all thousand hop once as they are counted.
 * f 46- 67  "500 GOOD." - the Hillvile half (pastel sky, sunshine) does a
 *           stadium wave with hearts; the other half dims.
 * f 68- 89  "500 BAD." - the Hellvile half (blood sky, sick moon, embers)
 *           sways menacingly with glowing eyes and little flames.
 * f 90-119  "1 PARK." - pull back further: a white picket fence and trees
 *           around all of them. The fence squeezes in, the crowd squishes...
 *           and one bunny pops out of the top like a cork. "(cozy.)"
 */
#include <stdio.h>
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

/* -- extra helpers for the ending scenes ----------------------------------- */
/* rounded rectangle into the mask */
U void mk_rrect(float x, float y, float w, float h, float r) {
    for (int j = (int)floorf(y); j < (int)ceilf(y + h); j++)
        for (int i = (int)floorf(x); i < (int)ceilf(x + w); i++) {
            float dx = fmaxf(fmaxf(x + r - (i + 0.5f), (i + 0.5f) - (x + w - r)), 0);
            float dy = fmaxf(fmaxf(y + r - (j + 0.5f), (j + 0.5f) - (y + h - r)), 0);
            if (dx * dx + dy * dy <= r * r) mk_px(i, j);
        }
}
/* heart curve test + heart mask: tip at (cx, tip), half-width w, height h */
U int heart_in(float u, float v) {
    float a = u * u + v * v - 1.0f;
    return a * a * a - u * u * v * v * v <= 0.0f;
}
U void mk_heart(float cx, float tip, float w, float h) {
    for (int y = (int)(tip - h - 2); y <= (int)tip + 1; y++)
        for (int x = (int)(cx - w - 2); x <= (int)(cx + w + 2); x++) {
            float u = (x + 0.5f - cx) / w * 1.139f, v = -1.0f + (tip - (y + 0.5f)) / h * 2.25f;
            if (heart_in(u, v)) mk_px(x, y);
        }
}
/* a soft-edged 4-tone pillow heart, ready to use */
U void plush_heart(float cx, float tip, float w, float h, int paint, uint8_t ol) {
    mk_heart(cx, tip, w, h);
    uint8_t r4[4] = { PAL_PAINT(paint, 0), PAL_PAINT(paint, 1), PAL_PAINT(paint, 2), PAL_WHITE };
    pillow(r4, ol, w > 4 ? 2.0f : 1.2f, 0.3f);
}
/* text buffer: smoothed puffy glyph tiles laid out flat, then stamped into
 * the mask with any scale / rotation (for slams, stamps and logo letters) */
static uint8_t tbuf[48][420];
static int tb_w, tb_h;
U void tb_build(const char *s, int sc, int bold, int gap) {
    memset(tbuf, 0, sizeof tbuf);
    int x = 0;
    tb_h = sc == 3 ? 24 : sc == 4 ? 32 : 16;
    for (; *s; s++) {
        if (*s == ' ') { x += 3 * sc + gap; continue; }
        Tile t;
        make_tile(&t, (unsigned char)*s, sc, bold);
        for (int y = 0; y < t.h && y < 48; y++)
            for (int i = 0; i < t.w; i++)
                if (t.m[y][i] && x + i < 420) tbuf[y][x + i] = 1;
        x += t.w + gap;
    }
    tb_w = x - gap > 0 ? x - gap : 1;
}
/* stamp the text buffer into the mask, centred at (cx, cy) */
U void tb_mk(float cx, float cy, float sx, float sy, float ang) {
    float ca = cosf(ang), sa = sinf(ang);
    float R = 0.5f * sqrtf(tb_w * sx * tb_w * sx + tb_h * sy * tb_h * sy) + 2;
    for (int y = (int)(cy - R); y <= (int)(cy + R); y++)
        for (int x = (int)(cx - R); x <= (int)(cx + R); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float u = (dx * ca + dy * sa) / sx + tb_w * 0.5f, v = (-dx * sa + dy * ca) / sy + tb_h * 0.5f;
            if (u >= 0 && v >= 0 && u < tb_w && v < tb_h && tbuf[(int)v][(int)u]) mk_px(x, y);
        }
}
/* the whole puffy-text treatment: drop shadow, white sticker ring, pillow */
U void puffy_text(const char *s, int sc, int bold, int gap, float cx, float cy, float sx, float sy, float ang,
                  const uint8_t *r4, uint8_t ol, int ring, uint8_t ringc, float bevel) {
    tb_build(s, sc, bold, gap);
    tb_mk(cx, cy, sx, sy, ang); mk_ring(ring, 1, 2, 0, cmap(40, 10, 40, 8)); mk_reset();
    if (ringc) { tb_mk(cx, cy, sx, sy, ang); mk_ring(ring, 0, 0, ringc, NULL); mk_reset(); }
    tb_mk(cx, cy, sx, sy, ang);
    pillow(r4, ol, bevel, 0.5f);
}
/* chibi design space -> screen (feet at yf, centre x, size S) */
U float CX(float x, int S, float dx) { return x + (dx - 12.0f) * S; }
U float CY(float yf, int S, float dy) { return yf + (dy - 34.1f) * S; }
/* a simple soft drop of water / blood: round bottom, pointy top */
U void mk_drop(float x, float y, float r) {
    mk_ellipse(x, y, r, r * 1.05f);
    mk_tri(x - r * 0.8f, y - r * 0.3f, x + r * 0.8f, y - r * 0.3f, x, y - r * 2.4f);
}
/* little mono LCD-style shadowed text bubble */
U void small_bubble(int x, int y, int w, int h, int tailx, int taily, uint8_t fill, uint8_t ol) {
    mk_rrect((float)x, (float)y, (float)w, (float)h, 4);
    mk_tri((float)tailx - 3, (float)(y + h - 2), (float)tailx + 3, (float)(y + h - 2), (float)tailx - 4, (float)taily);
    mk_ring(1, 1, 2, 0, cmap(0, 0, 0, 7)); mk_reset();
    mk_rrect((float)x, (float)y, (float)w, (float)h, 4);
    mk_tri((float)tailx - 3, (float)(y + h - 2), (float)tailx + 3, (float)(y + h - 2), (float)tailx - 4, (float)taily);
    uint8_t r4[4] = { fill, fill, fill, fill };
    pillow(r4, ol, 1.0f, 0.0f);
}

/* ========================================================================== */
/* the scene                                                                  */
/* ========================================================================== */
#define F_1000   22     /* "1000 CITIZENS." */
#define F_GOOD   46     /* "500 GOOD."      */
#define F_BAD    68     /* "500 BAD."       */
#define F_PARK   90     /* "1 PARK."        */
#define F_SQUISH 100    /* the fence squeezes in */
#define F_POP    108    /* ...and someone pops out like a cork */

#define COLS 20         /* per side */
#define ROWS 25
#define CW_ 11.0f       /* world cell width  */
#define RH_ 9.0f        /* world row height  */
#define WALLW 9.0f      /* half-width of the wall gap */

static float zk, zfx, zfy;       /* zoom: px per world unit, focus point */
static float squish = 1.0f;      /* crowd spacing squeeze */
static TkChibi cit[2][ROWS][COLS];
static int cit_ok;

static float SXw(float wx) { return 120 + (wx - zfx) * zk; }
static float SYw(float wy) { return 96 + (wy - zfy) * zk; }

/* ---- a mini critter citizen at any scale (k px per unit), feet at (x, y) - */
static void ramp_of(uint8_t b, uint8_t r4[4], uint8_t *ol) {
    if (b >= PAL_PAINT_BASE && b < PAL_RAMP_BASE) {
        int base = b - (b - PAL_PAINT_BASE) % 3;
        r4[0] = (uint8_t)base; r4[1] = (uint8_t)(base + 1); r4[2] = (uint8_t)(base + 1); r4[3] = (uint8_t)(base + 2);
        *ol = (uint8_t)base;
        if (base == PAL_PAINT(PAINT_JET, 0) || base == PAL_PAINT(PAINT_CHARCOAL, 0) || base == PAL_PAINT(PAINT_NAVY, 0)) *ol = PAL_BLACK;
        if (base == PAL_PAINT(PAINT_WHITE, 0)) *ol = PAL_GRAY;
        return;
    }
    r4[0] = r4[1] = r4[2] = r4[3] = b; *ol = PAL_GRAY_D;
}
static void mini(float x, float y, float k, const TkChibi *c, int bad, int f, int idx, float hop, int glow, int arm) {
    float bob = 0.35f * sinf(f * 0.16f + idx * 0.7f);
    y -= (hop + bob) * k;
    uint8_t r4[4], ol;
    /* devil tail */
    if (c->tail == TKT_DEVIL && k > 0.7f) {
        float sw = sinf(f * 0.25f + idx) * 1.2f;
        tk_line((int)(x + 2.5f * k), (int)(y - 2 * k), (int)(x + (5 + sw) * k), (int)(y - 5 * k), PAL_PAINT(PAINT_CHERRY, 1));
        tk_px((int)(x + (5 + sw) * k), (int)(y - 6 * k), PAL_PAINT(PAINT_CHERRY, 2));
    }
    /* body + paws */
    mk_ellipse(x, y - 3.0f * k, 3.4f * k, 3.0f * k);
    ramp_of(c->outfit, r4, &ol);
    pillow(r4, ol, 1.2f, 0.3f);
    float ay = arm ? -7.0f : -3.0f;
    mk_ellipse(x - 3.7f * k, y + ay * k, 1.25f * k, 1.25f * k);
    mk_ellipse(x + 3.7f * k, y - 3.0f * k, 1.25f * k, 1.25f * k);
    /* head + ears in one fur mask */
    float hx = x, hy = y - 9.0f * k;
    mk_ellipse(hx, hy, 4.6f * k, 4.0f * k);
    switch (c->ears) {
    case TKEAR_CAT: case TKEAR_FOX:
        for (int s = -1; s <= 1; s += 2)
            mk_tri(hx + s * 1.2f * k, hy - 2.5f * k, hx + s * 4.4f * k, hy - 1.6f * k, hx + s * 3.4f * k, hy - (c->ears == TKEAR_FOX ? 7.6f : 6.4f) * k);
        break;
    case TKEAR_BUNNY:
        for (int s = -1; s <= 1; s += 2) mk_ellipse(hx + s * 1.8f * k, hy - 5.6f * k, 1.15f * k, 3.4f * k);
        break;
    case TKEAR_BEAR: case TKEAR_MOUSE:
        for (int s = -1; s <= 1; s += 2) mk_ellipse(hx + s * 3.4f * k, hy - 3.2f * k, (c->ears == TKEAR_MOUSE ? 2.0f : 1.5f) * k, (c->ears == TKEAR_MOUSE ? 2.0f : 1.5f) * k);
        break;
    case TKEAR_PUP:
        for (int s = -1; s <= 1; s += 2) mk_ellipse(hx + s * 4.4f * k, hy + 0.2f * k, 1.2f * k, 2.4f * k);
        break;
    case TKEAR_SHEEP:
        for (int s = -1; s <= 1; s += 2) mk_ellipse(hx + s * 4.8f * k, hy - 0.8f * k, 1.8f * k, 0.9f * k);
        break;
    case TKEAR_OWL:
        for (int s = -1; s <= 1; s += 2) mk_tri(hx + s * 1.5f * k, hy - 3.0f * k, hx + s * 3.8f * k, hy - 2.2f * k, hx + s * 3.4f * k, hy - 5.6f * k);
        break;
    default:
        for (int s = -1; s <= 1; s += 2) mk_ellipse(hx + s * 4.6f * k, hy + 0.4f * k, 0.9f * k, 1.2f * k);
        break;
    }
    ramp_of(c->skin, r4, &ol);
    pillow(r4, ol, 1.4f, 0.4f);
    /* muzzle, eyes, blush */
    if (c->ears != TKEAR_HUMAN && k >= 0.8f) tk_ellipse((int)hx, (int)(hy + 1.8f * k), (int)(1.9f * k), (int)(1.1f * k + 0.5f), c->fur2 ? c->fur2 : PAL_PAINT(PAINT_BONE, 2));
    uint8_t ek = PAL_PAINT(PAINT_JET, 0);
    if (glow) ek = (idx & 1) ? PAL_UI_RED : PAL_RAMP(RAMP_LAVA, 6);
    int blink = ((f + idx * 13) % 71) > 67;
    for (int s = -1; s <= 1; s += 2) {
        int ex = (int)(hx + s * 1.9f * k), ey = (int)(hy - 0.2f * k);
        if (blink) { tk_px(ex, ey + (k > 1.2f), ek); continue; }
        if (k < 0.9f) tk_px(ex, ey, ek);
        else {
            int ew = k > 1.6f ? 2 : 1, eh = k > 1.6f ? 3 : 2;
            tk_rect(ex - ew / 2, ey - 1, ew, eh, ek);
            if (k >= 1.2f) tk_px(ex - ew / 2, ey - 1, glow ? PAL_RAMP(RAMP_LAVA, 7) : PAL_WHITE);
        }
        if (glow && k < 0.9f) tk_px(ex, ey - 1, PAL_RAMP(RAMP_LAVA, 5));
        if (c->blush && k >= 1.0f) tk_px((int)(hx + s * 3.2f * k), (int)(hy + 1.4f * k), PAL_PAINT(PAINT_BUBBLEGUM, 1));
    }
    /* horns, halo, party hat */
    if (c->horns) {
        uint8_t hc = c->horns == 2 ? PAL_RAMP(RAMP_BONE, 5) : PAL_PAINT(PAINT_CHERRY, 0);
        for (int s = -1; s <= 1; s += 2)
            tk_tri((int)(hx + s * 1.4f * k), (int)(hy - 3.2f * k), (int)(hx + s * 3.0f * k), (int)(hy - 2.8f * k), (int)(hx + s * 3.0f * k), (int)(hy - 6.2f * k), hc);
    }
    if (c->halo) {
        int hw = (int)(3.2f * k) + 1, yy = (int)(hy - 7.0f * k - 1 + sinf(f * 0.1f + idx));
        tk_rect((int)hx - hw, yy, 2 * hw + 1, 1, PAL_UI_GOLD);
    }
    if (c->hat == 1 && k > 0.7f)
        tk_tri((int)(hx - 2 * k), (int)(hy - 3.6f * k), (int)(hx + 2 * k), (int)(hy - 3.6f * k), (int)(hx + 0.5f * k), (int)(hy - 8.5f * k), PAL_PAINT(PAINT_BUBBLEGUM, 2));
}

/* far away: a soft, outline-free micro critter (pastel or dark by side) */
static uint8_t tone(uint8_t b, int shade) {
    if (b >= PAL_PAINT_BASE && b < PAL_RAMP_BASE) return (uint8_t)(b - (b - PAL_PAINT_BASE) % 3 + shade);
    return b;
}
static void micro(float x, float y, float k, const TkChibi *c, int bad, int f, int idx, float hop, int glow) {
    y -= (hop + 0.35f * sinf(f * 0.16f + idx * 0.7f)) * k;
    uint8_t fur = tone(c->skin, bad ? 0 : 2), fur_l = bad ? tone(c->skin, 1) : PAL_WHITE;
    uint8_t out = tone(c->outfit, bad ? 0 : 1);
    if (bad && (fur == PAL_PAINT(PAINT_JET, 0) || fur == PAL_PAINT(PAINT_NAVY, 0))) fur = tone(c->skin, 1);
    int hx = (int)x, hy = (int)(y - 9.0f * k);
    tk_ellipse(hx, (int)(y - 3.0f * k), (int)(3.0f * k + 0.5f), (int)(2.6f * k + 0.5f), out);
    switch (c->ears) {
    case TKEAR_CAT: case TKEAR_FOX: case TKEAR_OWL:
        tk_px(hx - (int)(3.0f * k), hy - (int)(4.6f * k), fur); tk_px(hx + (int)(3.0f * k), hy - (int)(4.6f * k), fur);
        tk_px(hx - (int)(3.0f * k), hy - (int)(3.6f * k), fur); tk_px(hx + (int)(3.0f * k), hy - (int)(3.6f * k), fur);
        break;
    case TKEAR_BUNNY:
        tk_rect(hx - (int)(2.0f * k), hy - (int)(8.0f * k), 1, (int)(4.5f * k) + 1, fur);
        tk_rect(hx + (int)(2.0f * k), hy - (int)(8.0f * k), 1, (int)(4.5f * k) + 1, fur);
        break;
    case TKEAR_BEAR: case TKEAR_MOUSE: case TKEAR_SHEEP: case TKEAR_PUP:
        tk_px(hx - (int)(3.6f * k), hy - (int)(3.4f * k), fur); tk_px(hx + (int)(3.6f * k), hy - (int)(3.4f * k), fur);
        break;
    default: break;
    }
    tk_ellipse(hx, hy, (int)(4.2f * k + 0.5f), (int)(3.6f * k + 0.5f), fur);
    tk_px(hx - (int)(1.6f * k), hy - (int)(1.8f * k), fur_l);
    if (c->horns) { tk_px(hx - (int)(2.4f * k), hy - (int)(4.4f * k), PAL_PAINT(PAINT_CHERRY, 1)); tk_px(hx + (int)(2.4f * k), hy - (int)(4.4f * k), PAL_PAINT(PAINT_CHERRY, 1)); }
    if (glow) { tk_px(hx - 1, hy, (idx & 1) ? PAL_UI_RED : PAL_RAMP(RAMP_LAVA, 6)); tk_px(hx + 1, hy, (idx & 1) ? PAL_UI_RED : PAL_RAMP(RAMP_LAVA, 6)); }
    else if (k >= 0.6f && ((f + idx * 13) % 71) < 68) { tk_px(hx - 1, hy, PAL_PAINT(PAINT_JET, 0)); tk_px(hx + 1, hy, PAL_PAINT(PAINT_JET, 0)); }
    if (c->halo) tk_rect(hx - 1, hy - (int)(5.4f * k) - 1, 3, 1, PAL_UI_GOLD);
}

/* ---- skies ------------------------------------------------------------------- */
static void skies(int f, float wallx) {
    static const uint8_t G[4] = { PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    static const uint8_t B[4] = { PAL_RAMP(RAMP_BLOOD, 0), PAL_RAMP(RAMP_BLOOD, 1), PAL_RAMP(RAMP_BLOOD, 3), PAL_RAMP(RAMP_LAVA, 3) };
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float L = y / 160.0f * 3.6f;
            tk_fb[y][x] = x < wallx ? qtone(G, 4, L, x, y) : qtone(B, 4, L, x, y);
        }
    /* left: a sun bloom + puffy clouds; right: a sick moon + embers */
    float sx = wallx - 70 - f * 0.05f;
    tint_ellipse(sx, 18, 30, 26, cmap(255, 250, 220, 7), 0.6f, 16);
    tk_circle((int)sx, 18, 8, PAL_RAMP(RAMP_CANDY, 7));
    tk_circle((int)sx - 2, 16, 4, PAL_WHITE);
    for (int i = 0; i < 4; i++) {
        float cx = fmodf(i * 61.0f + f * 0.25f, wallx + 40) - 20, cy = 14 + (i & 1) * 12;
        if (cx > wallx - 12) continue;
        mk_ellipse(cx, cy, 11, 4); mk_ellipse(cx - 5, cy - 2, 5, 4); mk_ellipse(cx + 3, cy - 4, 6, 5);
        static const uint8_t CL[4] = { PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE, PAL_WHITE };
        pillow(CL, PAL_RAMP(RAMP_VAPE, 6), 2.0f, 0.8f);
    }
    float mx = wallx + 64 + f * 0.04f;
    tint_ellipse(mx, 20, 22, 20, cmap(255, 90, 40, 6), 0.6f, 16);
    tk_circle((int)mx, 20, 8, PAL_RAMP(RAMP_LAVA, 6));
    tk_circle((int)mx + 3, 18, 7, PAL_RAMP(RAMP_BLOOD, 1));
    for (int i = 0; i < 24; i++) {
        uint32_t h = tk_hash((uint32_t)i * 41u + 3u);
        float ex = wallx + 4 + (h % 140), ey = fmodf((float)((h >> 8) % 160) - f * (0.4f + (h % 5) * 0.15f) + 320, 160);
        if (ex < wallx + 2) continue;
        tk_px((int)(ex + 2 * sinf(f * 0.1f + i)), (int)ey, (h & 1) ? PAL_RAMP(RAMP_LAVA, 6) : PAL_RAMP(RAMP_LAVA, 4));
    }
}
static void the_wall(float wx_center, float top_wy, float bot_wy, int f) {
    float x0 = SXw(wx_center - 4.5f), x1 = SXw(wx_center + 4.5f);
    float y0 = SYw(top_wy), y1 = SYw(bot_wy);
    if (x1 - x0 < 2) x1 = x0 + 2;
    static const uint8_t W[4] = { PAL_RAMP(RAMP_STONE, 2), PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 5) };
    for (int y = (int)y0; y < (int)y1; y++)
        for (int x = (int)x0; x < (int)x1; x++) {
            float u = (x + 0.5f - x0) / (x1 - x0);
            float L = 2.6f - u * 2.2f;
            uint8_t c = qtone(W, 4, L, x, y);
            float bh = 5 * zk, bw = 9 * zk;
            int row = (int)((y - y0) / (bh > 2 ? bh : 2));
            if (bh > 2.5f && (int)fmodf(y - y0, bh) == 0) c = PAL_RAMP(RAMP_STONE, 1);
            if (bw > 3 && (int)fmodf(x - x0 + (row & 1) * bw * 0.5f, bw) == 0) c = PAL_RAMP(RAMP_STONE, 1);
            tk_px(x, y, c);
        }
    /* crenellations */
    float cw = (x1 - x0) / 3.0f;
    for (int i = 0; i < 3; i += 2) tk_rect((int)(x0 + i * cw), (int)(y0 - cw), (int)cw + 1, (int)cw, W[i ? 1 : 3]);
    tk_rect((int)x0, (int)y0, 1, (int)(y1 - y0), PAL_RAMP(RAMP_STONE, 6));
    /* the hell-side is scorched */
    tint_rect((int)(x0 + (x1 - x0) * 0.6f), (int)y0, (int)((x1 - x0) * 0.4f) + 1, (int)(y1 - y0), cmap(120, 20, 10, 6), 8);
    (void)f;
}
static void picket_fence(float wy_line, float wx0, float wx1, int front) {
    float y = SYw(wy_line), h = 7 * zk + 2;
    float step = 5 * zk;
    if (step < 2.5f) step = 2.5f;
    tk_rect((int)SXw(wx0), (int)(y - h * 0.65f), (int)(SXw(wx1) - SXw(wx0)), 1 + (zk > 0.5f), PAL_RAMP(RAMP_PEACH, 6));
    for (float x = SXw(wx0); x <= SXw(wx1); x += step) {
        tk_rect((int)x, (int)(y - h), 2, (int)h, PAL_WHITE);
        tk_px((int)x + 1, (int)(y - h) + 1, PAL_RAMP(RAMP_VAPE, 6));
        tk_px((int)x, (int)(y - h) - 1, PAL_WHITE);
    }
    (void)front;
}
static void tree(float x, float y, float s, int f, int hell) {
    tk_rect((int)(x - 1.5f * s), (int)(y - 8 * s), (int)(3 * s) + 1, (int)(8 * s), hell ? PAL_RAMP(RAMP_WOOD, 1) : PAL_RAMP(RAMP_WOOD, 3));
    float sway = sinf(f * 0.08f + x) * s * 0.6f;
    mk_ellipse(x + sway, y - 13 * s, 7 * s, 6 * s);
    mk_ellipse(x - 4 * s + sway, y - 10 * s, 4.5f * s, 4 * s);
    mk_ellipse(x + 4 * s + sway, y - 10 * s, 4.5f * s, 4 * s);
    static const uint8_t TG[4] = { PAL_RAMP(RAMP_GRASS, 3), PAL_RAMP(RAMP_GRASS, 4), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6) };
    static const uint8_t TH_[4] = { PAL_RAMP(RAMP_SICK, 1), PAL_RAMP(RAMP_SICK, 2), PAL_RAMP(RAMP_SICK, 3), PAL_RAMP(RAMP_SICK, 4) };
    pillow(hell ? TH_ : TG, hell ? PAL_RAMP(RAMP_SICK, 0) : PAL_RAMP(RAMP_GRASS, 1), 2.0f, 0.5f);
}

/* ---- caption lockups: a big puffy number over a puffy word ----------------- */
static void lockup(const char *num, const char *word, float cx, float cy, int age, int style) {
    if (age < 0) return;
    static const float SL[] = { 0.2f, 1.35f, 0.9f, 1.06f, 0.98f, 1.0f };
    float s = age < 6 ? SL[age] : 1.0f;
    static const uint8_t PK[4] = { PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
    static const uint8_t MN[4] = { PAL_PAINT(PAINT_TEAL, 0), PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_MINT, 2), PAL_WHITE };
    static const uint8_t RD[4] = { PAL_RAMP(RAMP_BLOOD, 3), PAL_UI_RED, PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_LAVA, 7) };
    static const uint8_t LV[4] = { PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE };
    static const uint8_t GD[4] = { PAL_PAINT(PAINT_HONEY, 0), PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_LEMON, 2) };
    const uint8_t *rn = style == 1 ? PK : style == 2 ? RD : style == 3 ? MN : GD;
    const uint8_t *rw = style == 1 ? MN : style == 2 ? RD : style == 3 ? PK : LV;
    uint8_t ol = style == 2 ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_VAPE, 1);
    puffy_text(num, 4, 2, 1, cx, cy - 10, s, s, style == 2 ? 0.05f : -0.04f, rn, ol, 3, style == 2 ? PAL_RAMP(RAMP_LAVA, 7) : PAL_WHITE, 3.0f);
    int wa = age - 3;
    if (wa >= 0) {
        float ws = wa < 6 ? SL[wa] : 1.0f;
        puffy_text(word, 2, 1, 1, cx, cy + 17, ws, ws, 0, rw, ol, 2, style == 2 ? PAL_RAMP(RAMP_LAVA, 7) : PAL_WHITE, 2.0f);
    }
}

void scene_s12_census(int f, int n) {
    (void)n;
    if (!cit_ok) {
        cit_ok = 1;
        for (int s = 0; s < 2; s++)
            for (int r = 0; r < ROWS; r++)
                for (int c = 0; c < COLS; c++) {
                    uint32_t h = tk_hash((uint32_t)(s * 1000 + r * COLS + c + 77));
                    TkChibi *q = &cit[s][r][c];
                    *q = tk_random_citizen((uint32_t)(s * 1000 + r * COLS + c + 77), s);
                    static const int GF[] = { PAINT_WHITE, PAINT_CARAMEL, PAINT_TANGERINE, PAINT_SILVER, PAINT_PEACH, PAINT_HONEY,
                                              PAINT_BUBBLEGUM, PAINT_LAVENDER, PAINT_SKY, PAINT_MINT, PAINT_LEMON, PAINT_CHOCOLATE };
                    static const int GO[] = { PAINT_BUBBLEGUM, PAINT_MINT, PAINT_SKY, PAINT_LEMON, PAINT_LAVENDER, PAINT_PEACH, PAINT_ROSE, PAINT_WHITE };
                    static const int BF[] = { PAINT_CHERRY, PAINT_CHARCOAL, PAINT_GRAPE, PAINT_RUST, PAINT_NAVY, PAINT_VAPE_PURPLE, PAINT_MAGENTA, PAINT_BLOOD };
                    static const int BO[] = { PAINT_JET, PAINT_CHARCOAL, PAINT_BLOOD, PAINT_GRAPE, PAINT_NAVY };
                    static const int BE[] = { TKEAR_CAT, TKEAR_FOX, TKEAR_BEAR, TKEAR_PUP, TKEAR_MOUSE, TKEAR_CAT };
                    if (s) {
                        q->skin = PAL_PAINT(BF[h % 8], 1); q->outfit = PAL_PAINT(BO[(h >> 4) % 5], 1);
                        q->ears = (uint8_t)BE[(h >> 8) % 6]; q->halo = 0;
                    } else {
                        q->skin = PAL_PAINT(GF[h % 12], 1); q->outfit = PAL_PAINT(GO[(h >> 4) % 8], 1);
                        if (q->ears == TKEAR_HUMAN) q->ears = TKEAR_BEAR;
                        if (h % 41) q->horns = 0;          /* (one or two little devils snuck in) */
                    }
                }
    }
    /* ---- camera ---- */
    float crowd_h = (ROWS - 1) * RH_;
    const float KM = 0.9f, YM = 73.0f;              /* the "mid" framing */
    if (f < F_1000) {
        float t = tk_ease(f / (float)F_1000);
        t = t * (2 - t);
        zk = 2.2f + (KM - 2.2f) * t;
        zfx = 0;
        zfy = (crowd_h - 4) + (YM - (crowd_h - 4)) * t;
    } else if (f < F_PARK) {
        zk = KM; zfx = 0; zfy = YM;
        if (f >= F_GOOD && f < F_BAD) { float t = tk_ease((f - F_GOOD) / 6.0f); zfx = -100 * t; }
        if (f >= F_BAD) { float t = tk_ease((f - F_BAD) / 6.0f); zfx = -100 + 200 * t; }
    } else {
        float t = tk_ease((f - F_PARK) / 9.0f);
        zk = KM + (0.42f - KM) * t; zfx = 100 - 100 * t; zfy = YM + (89 - YM) * t;
    }
    float wallx = SXw(0);
    skies(f, wallx);
    /* ground strip */
    float gy = SYw(-6);
    for (int y = (int)gy; y < TH; y++)
        for (int x = 0; x < TW; x++)
            if (y >= 0) tk_fb[y][x] = x < wallx ? PAL_RAMP(RAMP_GRASS, 5) : PAL_RAMP(RAMP_BLOOD, 2);

    /* the park around them (visible once zoomed out) */
    if (f >= F_PARK - 2) {
        float half = (COLS * CW_ + WALLW + 6) * squish;
        for (int s = -1; s <= 1; s += 2) {
            tree(SXw(s * (half + 10)), SYw(-2), 2.2f * zk + 0.6f, f, s > 0);
            tree(SXw(s * (half + 22)), SYw(crowd_h * 0.6f), 2.6f * zk + 0.8f, f, s > 0);
            tree(SXw(s * (half + 8)), SYw(crowd_h + 10), 3.0f * zk + 0.8f, f, s > 0);
        }
        picket_fence(-4, -half, half, 0);
    }
    the_wall(0, -26, crowd_h + 6, f);

    /* ---- the crowd, back row to front ---- */
    int phase_good = f >= F_GOOD && f < F_BAD, phase_bad = f >= F_BAD && f < F_PARK;
    for (int r = 0; r < ROWS; r++) {
        float wy = r * RH_;
        float sy = SYw(wy);
        if (sy < -10 || sy - 16 * zk > TH) continue;
        for (int s = 0; s < 2; s++)
            for (int c = 0; c < COLS; c++) {
                uint32_t jh = tk_hash((uint32_t)(s * 7777 + r * 131 + c * 17));
                float jx = ((int)(jh % 9) - 4) * 0.55f, jy = ((int)((jh >> 5) % 7) - 3) * 0.5f;
                float wx = (WALLW + 5 + c * CW_ + (r & 1) * CW_ * 0.5f + jx) * squish;
                if (!s) wx = -wx;
                float sx = SXw(wx), syj = sy + jy * zk;
                if (sx < -10 || sx > TW + 10) continue;
                int idx = s * 1000 + r * COLS + c;
                float hop = 0;
                int arm = 0, glow = 0;
                if (phase_good && !s) {          /* stadium wave of joy */
                    float ph = (f - F_GOOD) * 0.45f - c * 0.5f * (s ? 1 : -1) * -1 - r * 0.08f;
                    float v = sinf(ph);
                    hop = v > 0 ? v * v * 3.0f : 0;
                    arm = v > 0.3f;
                }
                if (phase_bad && s) {            /* menacing sway, glowing eyes */
                    hop = 0.8f * fabsf(sinf((f - F_BAD) * 0.3f + c * 0.4f + r * 0.2f));
                    glow = 1;
                    arm = ((f / 6 + c + r) % 5) == 0;
                }
                if (f < F_GOOD && f >= F_1000 - 4) {     /* everyone hops once when counted */
                    int a = f - (F_1000 - 4) - (c + r) / 6;
                    if (a >= 0 && a < 6) hop = 2.5f * sinf(a / 6.0f * 3.14159f);
                }
                if (f >= F_POP && s == 0 && r == 0 && c == 6) continue;     /* the cork (drawn later) */
                if (zk >= 0.75f) mini(sx, syj, zk, &cit[s][r][c], s, f, idx, hop, glow, arm);
                else micro(sx, syj, zk, &cit[s][r][c], s, f, idx, hop, glow);
            }
        /* little hearts over the happy side */
        if (phase_good && (r % 5) == 2) {
            for (int c = 0; c < COLS; c += 3) {
                int a = (f - F_GOOD + c * 3 + r) % 24;
                float hx = SXw(-(WALLW + 5 + c * CW_) * squish), hy = sy - 20 * zk - a * 0.8f;
                if (a < 18) tk_heart((int)hx, (int)hy, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
            }
        }
        if (phase_bad && (r % 5) == 3) {      /* little flames over the edgy side */
            for (int c = 1; c < COLS; c += 3) {
                int a = (f - F_BAD + c * 5 + r) % 16;
                float hx = SXw((WALLW + 5 + c * CW_) * squish), hy = sy - 17 * zk - a * 0.5f;
                if (a < 12) {
                    mk_drop(hx + sinf(a * 0.8f), hy, 1.4f - a * 0.06f);
                    static const uint8_t FL[4] = { PAL_RAMP(RAMP_LAVA, 4), PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_LAVA, 6), PAL_RAMP(RAMP_LAVA, 7) };
                    pillow(FL, PAL_RAMP(RAMP_BLOOD, 2), 1.0f, -0.5f);
                }
            }
        }
    }
    /* front fence + "pop!" */
    if (f >= F_PARK - 2) picket_fence(crowd_h + 3, -(COLS * CW_ + WALLW + 6) * squish, (COLS * CW_ + WALLW + 6) * squish, 1);
    if (f >= F_PARK + 4) {
        int sa = f - F_PARK - 4;
        static const float SG[] = { -14, -6, 2, 0, -1, 0 };
        float sy = SYw(crowd_h + 3) - 4 + (sa < 6 ? SG[sa] : 0);
        mk_rrect(80, sy - 3, 80, 13, 3);
        static const uint8_t WD[4] = { PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6), PAL_RAMP(RAMP_PEACH, 7) };
        pillow(WD, PAL_RAMP(RAMP_WOOD, 1), 2.0f, 0.3f);
        tk_text(120 - tk_text_w("HELLVILE PARK", 1) / 2, (int)sy, "HELLVILE PARK", PAL_RAMP(RAMP_WOOD, 1), 1);
    }
    if (f >= F_POP) {
        /* the cork: shoots up out of the crowd, growing as it flies at us */
        int a = f - F_POP;
        float x0 = SXw(-(WALLW + 5 + 6 * CW_) * squish), y0 = SYw(0);
        float t = a / 6.0f;
        float ck = zk + (1.15f - zk) * tk_clampf(t, 0, 1);
        /* up it goes... and lands on top of everyone: crowd-surfing */
        float cx = x0 + a * 1.6f, cy = y0 - a * 7.0f + a * a * 0.3f;
        if (cy > y0 - 2) cy = y0 - 2 + sinf(f * 0.5f);
        mini(cx, cy, ck, &cit[0][0][6], 0, f, 6, 0, 0, 1);
        if (a < 10) {
            tk_text_outline((int)x0 - 10, (int)y0 - 22 - a, "pop!", PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1), 1);
            tk_sparkles((int)x0, (int)y0 - 6, a * 3, 55, 12);
        }
    }

    /* ---- dimming the other side while one side has the spotlight ---- */
    if (phase_good || phase_bad) {
        int a = f - (phase_good ? F_GOOD : F_BAD);
        const uint8_t *dim = cmap(20, 10, 40, a < 3 ? 3 + a * 2 : 9);
        for (int y = 0; y < TH; y++)
            for (int x = 0; x < TW; x++)
                if (phase_good ? x > wallx + 3 : x < wallx - 3) tint_px(x, y, dim);
    }

    /* ---- the numbers ---- */
    if (f < F_1000) {
        /* the population counter rolls up while we zoom out */
        int v = (int)(1000 * powf(f / (float)F_1000, 2.2f));
        char buf[8];
        snprintf(buf, sizeof buf, "%04d", v);
        static const uint8_t GD[4] = { PAL_PAINT(PAINT_HONEY, 0), PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_LEMON, 2) };
        if (f >= 3) puffy_text(buf, 3, 2, 1, 120, 20, 1, 1, 0, GD, PAL_RAMP(RAMP_VAPE, 1), 2, PAL_WHITE, 2.0f);
    } else if (f < F_GOOD) lockup("1000", "CITIZENS.", 120, 34, f - F_1000, 0);
    else if (f < F_BAD) lockup("500", "GOOD.", 104, 34, f - F_GOOD, 1);
    else if (f < F_PARK) lockup("500", "BAD.", 136, 34, f - F_BAD, 2);
    else {
        lockup("1", "PARK.", 120, 34, f - F_PARK, 3);
        if (f >= F_POP + 3) {
            int ca = f - F_POP - 3;
            tk_text_outline(160, 38 + (ca == 0 ? -3 : ca == 1 ? 1 : 0), "(cozy.)", PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1), 1);
        }
    }
    if (f == F_1000 || f == F_1000 + 1) tk_post_shake(0, f == F_1000 ? 3 : -1);
    if (f == F_PARK || f == F_PARK + 1) tk_post_shake(0, f == F_PARK ? 3 : -1);
}

void scene_s12_census_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    /* counter roll: accelerating ticks */
    for (int i = 0; i < 16; i++) {
        double tt = F_1000 * pow(i / 16.0, 0.6);
        tk_note(t0 + tt * fr, 0.03, 76 + i, TKV_SQ25, 0.14f);
    }
    tk_sfx(t0 + F_1000 * fr, SFX_THUD, 0.8f);
    tk_sfx(t0 + F_1000 * fr, SFX_FANFARE, 0.45f);
    tk_sfx(t0 + (F_1000 + 3) * fr, SFX_POP, 0.4f);
    /* GOOD: a sparkly major arpeggio + the wave */
    tk_sfx(t0 + F_GOOD * fr, SFX_POP, 0.6f);
    tk_sfx(t0 + (F_GOOD + 2) * fr, SFX_SPARKLE, 0.45f);
    static const int MAJ[4] = { 72, 76, 79, 84 };
    for (int i = 0; i < 4; i++) tk_note(t0 + (F_GOOD + 3 + i * 3) * fr, 0.09, MAJ[i], TKV_SQ25, 0.22f);
    tk_sfx(t0 + (F_GOOD + 6) * fr, SFX_WHOOSH, 0.25f);
    /* BAD: a low growl + diminished stab */
    tk_sfx(t0 + F_BAD * fr, SFX_THUD, 0.8f);
    static const int DIM[3] = { 48, 51, 54 };
    for (int i = 0; i < 3; i++) tk_note(t0 + F_BAD * fr, 0.4, DIM[i], TKV_SAW, 0.14f);
    tk_slide(t0 + (F_BAD + 2) * fr, 0.5, 36, 31, TKV_SQ12, 0.3f);
    /* PARK: boing, squeeze, POP */
    tk_sfx(t0 + F_PARK * fr, SFX_BOING, 0.6f);
    tk_slide(t0 + F_SQUISH * fr, 0.25, 60, 72, TKV_SQ12, 0.2f);          /* nnngh */
    tk_sfx(t0 + F_POP * fr, SFX_POP, 1.0f);
    tk_slide(t0 + (F_POP + 1) * fr, 0.4, 72, 96, TKV_SINE, 0.25f);       /* wheee */
}
