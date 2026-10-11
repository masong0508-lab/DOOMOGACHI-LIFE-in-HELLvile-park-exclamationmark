/* s14_platforms.c - "Coming to GBA ... and PlayStation".   (4.5 s, 135 frames)
 *
 * A glossy product-shot studio: violet gradient backdrop with drifting light
 * stripes, bokeh and sparkles, spotlights, and a mirror floor that reflects
 * everything. (Generic hardware shapes only - no logos.)
 * f  0- 37  A purple horizontal handheld drops in, squashes, settles, and
 *           powers on (CRT line + ping): the game's title screen in miniature
 *           - DOOMOGACHI / LIFE / in HELLvile (it drips here too) / PRESS
 *           START. "Coming to GBA"
 * f 38- 73  It scoots over. A big grey console slides in - CLUNK - its lid
 *           pops open and a rainbow-sheened disc rises out, spinning.
 *           "and PlayStation"
 * f 74-101  Deadpan, silent: "Yes, the actual PlayStation." ... "The grey one."
 * f102-134  Both sprout little cartoon arms with white gloves... wind up...
 *           HIGH FIVE. *clap*  Then they keep their hands up, wiggling.
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
#define F_LAND    8     /* the handheld lands             */
#define F_ON     14     /* ...and powers on               */
#define F_SLIDE  38     /* it scoots over to make room    */
#define F_CON    48     /* the grey console slides in     */
#define F_CLUNK  56
#define F_LID    62     /* the lid pops open              */
#define F_DISC   64     /* the disc rises, spinning       */
#define F_YES    74     /* "Yes, the actual PlayStation." */
#define F_GREY   92     /* "The grey one."                */
#define F_ARMS  102     /* little arms pop out...          */
#define F_CLAP  112     /* ...HIGH FIVE                    */

#define FLOOR 118

/* ---- the studio: gradient backdrop, spotlights, bokeh ---------------------- */
static void studio(int f) {
    static const uint8_t BG[4] = { PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 5) };
    for (int y = 0; y < FLOOR; y++)
        for (int x = 0; x < TW; x++) {
            float dx = (x - 120) / 160.0f, dy = (y - 100) / 110.0f;
            tk_fb[y][x] = qtone(BG, 4, 3.3f - sqrtf(dx * dx + dy * dy) * 3.6f, x, y);
        }
    /* slow diagonal light stripes */
    const uint8_t *st = cmap(255, 230, 255, 1);
    for (int y = 0; y < FLOOR; y++)
        for (int x = 0; x < TW; x++)
            if (((x + y * 2 + f / 2) / 20) % 4 == 0) tint_px(x, y, st);
    /* bokeh */
    const uint8_t *bk[3] = { cmap(255, 200, 230, 6), cmap(200, 190, 255, 6), cmap(190, 240, 230, 5) };
    for (int i = 0; i < 9; i++) {
        uint32_t h = tk_hash((uint32_t)i * 313u + 17u);
        float r = 5 + (h % 8), x = (float)((h >> 4) % 250) - 5 + 4 * sinf(f * 0.04f + i);
        float y = fmodf((float)((h >> 12) % 130) - f * (0.2f + (h % 3) * 0.1f) + 260, 130) - 10;
        tint_ellipse(x, y, r, r, bk[i % 3], 0.4f, 16);
    }
    /* floating sparkles */
    for (int i = 0; i < 10; i++) {
        uint32_t h = tk_hash((uint32_t)i * 97u + 3u);
        int x = (int)(h % 240), y = (int)((h >> 8) % 100) + 4;
        int ph = (f + (int)(h >> 20) % 30) % 30;
        if (ph < 12) tk_star(x, y, ph < 6 ? ph / 2 : (12 - ph) / 2, ph & 2 ? PAL_WHITE : PAL_RAMP(RAMP_CANDY, 7));
    }
}
static void spotlight(float x, float w, int lvl) {
    const uint8_t *m = cmap(255, 245, 255, lvl);
    for (int y = 0; y < FLOOR; y++) {
        float t = y / (float)FLOOR, hw = w * (0.35f + 0.65f * t);
        for (int xx = (int)(x - hw); xx <= (int)(x + hw); xx++) {
            float e = 1 - fabsf(xx + 0.5f - x) / hw;
            if (e > 0.25f || BY4[y & 3][xx & 3] < (int)(e * 64)) tint_px(xx, y, m);
        }
    }
    tint_ellipse(x, FLOOR + 3, w * 1.1f, 6, cmap(255, 245, 255, lvl + 2), 0.6f, 16);
}
/* glossy floor: mirror everything above it, tinted */
static void floor_reflect(void) {
    const uint8_t *m = cmap(70, 30, 100, 10);
    for (int y = FLOOR; y < TH; y++) {
        int sy = 2 * FLOOR - y - 1;
        for (int x = 0; x < TW; x++) tk_fb[y][x] = m[tk_fb[sy < 0 ? 0 : sy][x]];
    }
    tk_rect(0, FLOOR, TW, 1, PAL_RAMP(RAMP_VAPE, 6));
    tint_rect(0, FLOOR + 1, TW, 3, cmap(255, 255, 255, 3), 16);
    /* the reflection fades out toward us */
    for (int y = FLOOR + 10; y < TH; y++) {
        int lvl = (y - FLOOR - 10) * 16 / 32;
        for (int x = 0; x < TW; x++) if (BY4[y & 3][x & 3] < lvl) tk_fb[y][x] = PAL_RAMP(RAMP_VAPE, 1);
    }
}

/* ---- the handheld (generic: no logos) ---------------------------------------- */
static void mini_title(int x, int y, int f, int on_age) {
    /* the game's title screen, in miniature */
    static const uint8_t SK[4] = { PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    for (int j = 0; j < 44; j++)
        for (int i = 0; i < 66; i++) tk_px(x + i, y + j, qtone(SK, 4, j / 44.0f * 3.2f, x + i, y + j));
    for (int k = 0; k < 2; k++) {
        int cx = x + 10 + k * 40 + (int)((f * 0.2f + k * 20)) % 30, cy = y + 30 + k * 4;
        tk_ellipse(cx, cy, 6, 2, PAL_WHITE); tk_ellipse(cx + 3, cy - 2, 3, 2, PAL_WHITE);
    }
    tk_rect(x, y + 38, 66, 6, PAL_RAMP(RAMP_GRASS, 5));
    tk_rect(x, y + 38, 66, 1, PAL_RAMP(RAMP_GRASS, 6));
    int bob = (f / 8) & 1;
    tk_text(x + 4, y + 4 + bob, "DOOMOGACHI", PAL_PAINT(PAINT_ROSE, 0), 1);
    tk_text(x + 3, y + 3 + bob, "DOOMOGACHI", PAL_WHITE, 1);
    tk_text(x + 22, y + 13 + bob, "LIFE", PAL_PAINT(PAINT_ROSE, 0), 1);
    tk_text(x + 21, y + 12 + bob, "LIFE", PAL_PAINT(PAINT_BUBBLEGUM, 1), 1);
    tk_text(x + 0, y + 22, "in", PAL_RAMP(RAMP_VAPE, 3), 1);
    tk_text(x + 15, y + 22, "HELL", PAL_UI_RED, 1);
    tk_text(x + 40, y + 22, "vile", PAL_RAMP(RAMP_VAPE, 3), 1);
    if ((f / 6) % 3) tk_px(x + 33, y + 30 + (f / 3) % 4, PAL_UI_RED);        /* it drips here too */
    if ((f / 10) & 1) tk_text(x + 1, y + 37, "PRESS START", PAL_WHITE, 1);
    (void)on_age;
}
static void handheld(float cx, float cy, float sx, float sy, int f, int on_age) {
    float W = 118 * sx, H = 66 * sy;
    /* contact shadow */
    tint_ellipse(cx, FLOOR, W * 0.48f, 4, cmap(30, 0, 40, 8), 0.5f, 16);
    /* shoulder buttons */
    mk_rrect(cx - W / 2 + 6, cy - H / 2 - 3, 30 * sx, 8, 3);
    mk_rrect(cx + W / 2 - 6 - 30 * sx, cy - H / 2 - 3, 30 * sx, 8, 3);
    static const uint8_t SH[4] = { PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 4) };
    pillow(SH, PAL_RAMP(RAMP_VAPE, 0), 2.0f, 0.3f);
    /* the body: wide, with rounded wings */
    mk_rrect(cx - W / 2, cy - H / 2, W, H, 22 * sy);
    mk_rrect(cx - W / 2 + 14 * sx, cy - H / 2 - 2, W - 28 * sx, H + 2, 10);
    static const uint8_t BD[4] = { PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2), PAL_RAMP(RAMP_VAPE, 7) };
    pillow(BD, PAL_RAMP(RAMP_VAPE, 1), 6.0f, 0.5f);
    /* screen bezel */
    float bw = 78 * sx, bh = 52 * sy;
    mk_rrect(cx - bw / 2, cy - H / 2 + 5 * sy, bw, bh, 6);
    static const uint8_t BZ[4] = { PAL_RAMP(RAMP_VAPE, 0), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2) };
    pillow(BZ, PAL_BLACK, 2.0f, 0.2f);
    int lx = (int)(cx - 33 * sx), ly = (int)(cy - H / 2 + 9 * sy);
    if (on_age >= 0) {
        if (on_age < 3) {               /* CRT-ish power-on line */
            tk_rect(lx, ly, 66, 44, PAL_RAMP(RAMP_SICK, 1));
            int hw = 4 + on_age * 14;
            tk_rect(lx + 33 - hw, ly + 21, hw * 2, 2, PAL_WHITE);
        } else {
            mini_title(lx, ly, f, on_age);
            if (on_age < 7) tint_rect(lx, ly, 66, 44, cmap(255, 255, 255, 12 - on_age * 2), 16);
        }
    } else tk_rect(lx, ly, 66, 44, PAL_RAMP(RAMP_SICK, 0));
    /* glass glare */
    const uint8_t *gl = cmap(255, 255, 255, 3);
    for (int j = 0; j < 44; j++)
        for (int i = 0; i < 66; i++) {
            int u = i + j - (f % 80) * 2 + 30;
            if ((u >= 0 && u < 6) || (u >= 9 && u < 11)) tint_px(lx + i, ly + j, gl);
        }
    /* power LED */
    tk_rect(lx - 6, ly + 6, 2, 2, on_age >= 0 ? PAL_RAMP(RAMP_LAVA, 6) : PAL_RAMP(RAMP_BLOOD, 2));
    if (on_age >= 0 && (f / 10) % 3) tk_px(lx - 6, ly + 6, PAL_WHITE);
    /* d-pad */
    float dx = cx - W / 2 + 18 * sx, dy = cy + 2 * sy;
    mk_rect((int)dx - 2, (int)dy - 7, 5, 15); mk_rect((int)dx - 7, (int)dy - 2, 15, 5);
    static const uint8_t DP[4] = { PAL_GRAY_D, PAL_RAMP(RAMP_STONE, 2), PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 4) };
    pillow(DP, PAL_BLACK, 1.5f, 0.3f);
    /* A / B */
    float ax = cx + W / 2 - 14 * sx, ay = cy - 4 * sy;
    for (int k = 0; k < 2; k++) {
        mk_ellipse(ax - k * 10, ay + k * 6, 4.5f, 4.5f);
        static const uint8_t AB[4] = { PAL_PAINT(PAINT_GRAPE, 0), PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_GRAPE, 2), PAL_RAMP(RAMP_VAPE, 6) };
        pillow(AB, PAL_RAMP(RAMP_VAPE, 0), 2.0f, 0.3f);
    }
    /* start / select pills + speaker holes */
    for (int k = 0; k < 2; k++) {
        mk_rrect(dx - 8 + k * 9, cy + H / 2 - 13 * sy, 7, 3, 1.5f);
        pillow(DP, PAL_BLACK, 1.0f, 0.2f);
    }
    for (int k = 0; k < 6; k++) tk_px((int)(ax - 8 + (k % 3) * 3), (int)(cy + H / 2 - 14 * sy + (k / 3) * 3), PAL_RAMP(RAMP_VAPE, 1));
}

/* ---- the grey console (generic: no logos) -------------------------------------- */
static void console(float cx, float by, float sq, int f, float lid) {
    float W = 104, D = 20 * sq, Hf = 24 * sq;
    tint_ellipse(cx, FLOOR, 56, 4, cmap(30, 0, 40, 8), 0.5f, 16);
    /* front face */
    mk_rrect(cx - W / 2, by - Hf, W, Hf, 3);
    static const uint8_t FR[4] = { PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6) };
    pillow(FR, PAL_RAMP(RAMP_STONE, 1), 2.0f, 0.3f);
    /* top face (a trapezoid going back) */
    float ty = by - Hf, tb = ty - D;
    mk_tri(cx - W / 2, ty + 1, cx + W / 2, ty + 1, cx + W / 2 - 8, tb);
    mk_tri(cx - W / 2, ty + 1, cx + W / 2 - 8, tb, cx - W / 2 + 8, tb);
    static const uint8_t TP[4] = { PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6), PAL_GRAY_L, PAL_WHITE };
    pillow(TP, PAL_RAMP(RAMP_STONE, 2), 3.0f, 0.4f);
    tk_line((int)(cx - W / 2 + 2), (int)ty, (int)(cx + W / 2 - 2), (int)ty, PAL_WHITE);
    /* the round disc lid well + lid */
    float lx = cx + 10, ly = (ty + tb) * 0.5f;
    tk_ellipse((int)lx, (int)ly, 30, (int)(7 * sq), PAL_RAMP(RAMP_STONE, 3));
    if (lid > 0) {
        tk_ellipse((int)lx, (int)ly, 27, (int)(6 * sq), PAL_RAMP(RAMP_STONE, 1));      /* the open well */
        tk_ellipse((int)lx, (int)ly, 5, 2, PAL_RAMP(RAMP_STONE, 4));                    /* spindle */
        /* the lid swings up on its back hinge and stands behind the well */
        float yb = ly - 6 * sq;
        float lcy = ly + (yb - 15 - ly) * lid, lry = 6 * sq + (15 - 6 * sq) * lid;
        mk_ellipse(lx, lcy, 27, lry);
        static const uint8_t LD[4] = { PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6) };
        pillow(LD, PAL_RAMP(RAMP_STONE, 2), 3.0f, 0.3f);
        tk_ellipse((int)lx, (int)lcy, 20, (int)(lry * 0.7f), PAL_RAMP(RAMP_STONE, 4));     /* the inside of the lid */
        tk_ellipse((int)lx, (int)lcy, 6, (int)(lry * 0.2f) + 1, PAL_RAMP(RAMP_STONE, 3));
    } else {
        mk_ellipse(lx, ly, 27, 6 * sq);
        static const uint8_t LD[4] = { PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6), PAL_GRAY_L, PAL_WHITE };
        pillow(LD, PAL_RAMP(RAMP_STONE, 3), 2.0f, 0.3f);
        tk_ellipse((int)lx, (int)ly, 9, (int)(2 * sq), PAL_RAMP(RAMP_STONE, 5));
    }
    /* buttons on top: power, reset, open */
    for (int k = 0; k < 3; k++) {
        float bx = k < 2 ? cx - 36 + k * 13 : cx + 38, byy = ly + (k == 1 ? 1 : 0);
        int pressed = k == 2 && f >= F_LID - 2 && f < F_LID;
        mk_ellipse(bx, byy + pressed, 4, 2.2f * sq);
        static const uint8_t BT[4] = { PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 5), PAL_GRAY_L };
        pillow(BT, PAL_RAMP(RAMP_STONE, 1), 1.5f, 0.2f);
    }
    tk_px((int)cx - 36, (int)ly - 3, f >= F_CLUNK ? PAL_PAINT(PAINT_LIME, 1) : PAL_RAMP(RAMP_STONE, 2));    /* power LED */
    /* front: two pad ports + two card slots */
    for (int k = 0; k < 2; k++) {
        float px = cx - 34 + k * 22, py = by - Hf * 0.42f;
        mk_rrect(px - 7, py - 3, 14, 7, 2.5f);
        static const uint8_t PT[4] = { PAL_RAMP(RAMP_STONE, 1), PAL_RAMP(RAMP_STONE, 1), PAL_RAMP(RAMP_STONE, 2), PAL_RAMP(RAMP_STONE, 3) };
        pillow(PT, PAL_RAMP(RAMP_STONE, 0), 1.5f, 0.2f);
        tk_rect((int)px - 5, (int)py - 1, 10, 2, PAL_RAMP(RAMP_STONE, 0));
        tk_rect((int)px - 6, (int)(by - Hf + 3), 12, 2, PAL_RAMP(RAMP_STONE, 1));      /* card slot */
        tk_rect((int)px - 6, (int)(by - Hf + 5), 12, 1, PAL_RAMP(RAMP_STONE, 6));
    }
    /* vent ridges on the right of the front */
    for (int k = 0; k < 5; k++) tk_rect((int)(cx + 18 + k * 6), (int)(by - Hf + 5), 2, (int)(Hf - 9), PAL_RAMP(RAMP_STONE, 3));
}
static void disc(float cx, float cy, float r, int f) {
    float rot = f * 0.35f;
    for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
        for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx * dx + dy * dy);
            if (d > r) continue;
            if (d < r * 0.18f) continue;                       /* the hole */
            uint8_t c;
            if (d > r - 1 || (d < r * 0.18f + 1)) c = PAL_RAMP(RAMP_STONE, 3);
            else if (d < r * 0.36f) c = PAL_RAMP(RAMP_STONE, 6);
            else {
                float a = atan2f(dy, dx) + rot;
                float band = sinf(2 * a);
                static const int RB[5] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_MINT, PAINT_SKY, PAINT_LAVENDER };
                if (band > 0.55f) c = PAL_PAINT(RB[(int)((d / r) * 5 + 0.5f) % 5], 2);
                else if (band > 0.2f) c = PAL_WHITE;
                else if (band < -0.6f) c = PAL_RAMP(RAMP_STONE, 5);
                else c = PAL_GRAY_L;
            }
            tk_px(x, y, c);
        }
    tk_px((int)(cx - r * 0.5f), (int)(cy - r * 0.6f), PAL_WHITE);
}
/* cartoon noodle arm with a white glove, from (x0,y0) to the hand at (x1,y1) */
static void arm(float x0, float y0, float x1, float y1, float bend, int flip) {
    float mx = (x0 + x1) * 0.5f + bend, my = (y0 + y1) * 0.5f + fabsf(bend) * 0.4f;
    float px = x0, py = y0;
    for (int i = 1; i <= 14; i++) {
        float t = i / 14.0f, u = 1 - t;
        float qx = u * u * x0 + 2 * u * t * mx + t * t * x1, qy = u * u * y0 + 2 * u * t * my + t * t * y1;
        tk_line((int)px, (int)py, (int)qx, (int)qy, PAL_RAMP(RAMP_VAPE, 0));
        tk_line((int)px + 1, (int)py, (int)qx + 1, (int)qy, PAL_RAMP(RAMP_VAPE, 0));
        tk_line((int)px, (int)py + 1, (int)qx, (int)qy + 1, PAL_RAMP(RAMP_VAPE, 0));
        px = qx; py = qy;
    }
    mk_ellipse(x1, y1, 5, 5.5f);
    for (int k = 0; k < 3; k++) mk_ellipse(x1 + (flip ? -1 : 1) * (k - 1) * 3.2f, y1 - 5, 1.6f, 2.6f);
    mk_ellipse(x1 + (flip ? 5 : -5), y1 + 1, 2.2f, 1.6f);
    static const uint8_t GL[4] = { PAL_GRAY_L, PAL_WHITE, PAL_WHITE, PAL_WHITE };
    pillow(GL, PAL_RAMP(RAMP_VAPE, 0), 1.5f, 0.3f);
    tk_rect((int)x1 - 3, (int)y1 + 4, 7, 2, PAL_GRAY_L);
}

void scene_s14_platforms(int f, int n) {
    (void)n;
    studio(f);
    /* ---- the handheld: drops in, lands, powers on, scoots over ---- */
    float hx = 120, hy = 84, hsx = 1, hsy = 1;
    if (f < F_LAND) { float t = (f + 1) / (float)F_LAND; hy = 84 - 150 * (1 - t * t); hsx = 0.85f; hsy = 1.18f; }
    else if (f < F_LAND + 6) {
        static const float SX[6] = { 1.22f, 1.1f, 0.95f, 0.98f, 1.02f, 1.0f }, SY_[6] = { 0.78f, 0.9f, 1.07f, 1.03f, 0.99f, 1.0f };
        static const float DY[6] = { 6, 3, -3, -2, 0, 0 };
        hsx = SX[f - F_LAND]; hsy = SY_[f - F_LAND]; hy += DY[f - F_LAND];
    }
    if (f >= F_SLIDE) {
        float t = tk_ease((f - F_SLIDE) / 9.0f);
        hx = 120 + (63 - 120) * t;
        if (f < F_SLIDE + 9) { hsx = 1.06f; hsy = 0.96f; }
    }
    hy += sinf(f * 0.12f) * 1.2f;
    /* the high five squash */
    if (f >= F_CLAP && f < F_CLAP + 5) { static const float Q[5] = { 0.9f, 0.94f, 1.04f, 1.02f, 1.0f }; hsy = Q[f - F_CLAP]; hsx = 2 - Q[f - F_CLAP]; }
    float hand_y = hy;

    /* ---- the grey console ---- */
    float cx = 300, csq = 1;
    if (f >= F_CON) {
        float t = tk_clampf((f - F_CON) / (float)(F_CLUNK - F_CON), 0, 1);
        cx = 300 + (184 - 300) * (t * t);
        if (f >= F_CLUNK && f < F_CLUNK + 5) { static const float Q[5] = { 0.82f, 0.92f, 1.06f, 1.02f, 1.0f }; csq = Q[f - F_CLUNK]; }
        if (f >= F_CLAP && f < F_CLAP + 5) { static const float Q[5] = { 0.88f, 0.94f, 1.05f, 1.02f, 1.0f }; csq = Q[f - F_CLAP]; }
    }
    float lid = f >= F_LID ? tk_bounce((f - F_LID) / 8.0f) : 0;

    /* spotlights */
    spotlight(hx, 40, f < F_SLIDE ? 5 : 4);
    if (f >= F_CLUNK) spotlight(cx, 46, f < F_CLUNK + 4 ? 7 : 4);

    if (f >= 0) handheld(hx, hand_y, hsx, hsy, f, f >= F_ON ? f - F_ON : -1);
    if (f >= F_CON) console(cx, FLOOR - 2, csq, f, lid);
    /* the disc rises out of the console and hovers, spinning */
    if (f >= F_DISC) {
        float t = tk_ease((f - F_DISC) / 10.0f);
        float dyy = (FLOOR - 40) - 34 * t + 2 * sinf(f * 0.15f);
        float r = 6 + 12 * t;
        disc(cx + 10, dyy, r, f);
        if (f < F_DISC + 14) tk_sparkles((int)cx + 10, (int)dyy, (f - F_DISC) * 2, 33, 20);
    }
    /* ---- the high five ---- */
    if (f >= F_ARMS) {
        int a = f - F_ARMS;
        float ext = tk_bounce(a / 7.0f);
        float meet_x = 121, meet_y = 62;
        float k = f < F_CLAP ? tk_ease(tk_clampf((a - 4) / 6.0f, 0, 1)) : 1;
        /* wind-up: hands pull back, then swing in */
        float l0x = hx + 44 * hsx, l0y = hand_y - 28, r0x = cx - 40, r0y = FLOOR - 40;
        float lhx = l0x + (meet_x - 6 - l0x) * k - (1 - k) * 6 * ext, lhy = l0y - 12 * ext + (meet_y - (l0y - 12 * ext)) * k;
        float rhx = r0x + (meet_x + 6 - r0x) * k + (1 - k) * 6 * ext, rhy = r0y - 14 * ext + (meet_y - (r0y - 14 * ext)) * k;
        if (f >= F_CLAP + 10) {     /* hands stay up, wiggling with joy */
            lhy += sinf(f * 0.6f) * 2; rhy += sinf(f * 0.6f + 1) * 2;
        }
        arm(l0x, l0y, lhx, lhy, -10, 0);
        arm(r0x, r0y, rhx, rhy, 10, 1);
        if (f >= F_CLAP && f < F_CLAP + 12) {
            int ca = f - F_CLAP;
            for (int i = 0; i < 8; i++) {          /* impact lines */
                float an = i * 0.785f + 0.3f, r0 = 9 + ca * 2, r1 = r0 + 6;
                tk_line((int)(meet_x + cosf(an) * r0), (int)(meet_y - 6 + sinf(an) * r0), (int)(meet_x + cosf(an) * r1), (int)(meet_y - 6 + sinf(an) * r1), PAL_WHITE);
            }
            tk_star((int)meet_x, (int)meet_y - 6, 6 - ca / 2, PAL_UI_GOLD);
            tk_sparkles((int)meet_x, (int)meet_y - 6, ca * 2, 91, 26);
            if (ca < 4) tint_ellipse(meet_x, meet_y - 6, 18 - ca * 3, 18 - ca * 3, cmap(255, 255, 220, 12), 0.5f, 16);
        }
        if (f >= F_CLAP + 2) {
            int ca = f - F_CLAP - 2;
            tk_text_outline((int)meet_x - 17, (int)meet_y + 14 - (ca < 4 ? ca : 4), "*clap*", PAL_WHITE, PAL_RAMP(RAMP_VAPE, 0), 1);
        }
    }

    floor_reflect();

    /* ---- captions ---- */
    uint8_t oc = PAL_RAMP(RAMP_VAPE, 0);
    static const uint8_t GB[4] = { PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2), PAL_WHITE };
    static const uint8_t PS[4] = { PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 6), PAL_GRAY_L, PAL_WHITE };
    static const float SL[] = { 0.3f, 1.3f, 0.92f, 1.04f, 1.0f };
    if (f >= 4) {
        float x = hx;
        int a = f - 4;
        tk_text_outline((int)x - tk_text_w("Coming to", 1) / 2, 6 + (a < 2 ? -3 : 0), "Coming to", PAL_WHITE, oc, 1);
        if (a >= 4) {
            float s = a - 4 < 5 ? SL[a - 4] : 1.0f;
            puffy_text("GBA", 4, 2, 1, x, 32, s, s, -0.03f, GB, PAL_RAMP(RAMP_VAPE, 0), 2, PAL_WHITE, 3.0f);
        }
    }
    if (f >= F_CLUNK) {
        int a = f - F_CLUNK;
        float px = cx < 240 ? 176 + (cx - 184) : cx;
        tk_text_outline((int)px - tk_text_w("and", 1) / 2, 6 + (a < 2 ? -3 : 0), "and", PAL_WHITE, oc, 1);
        if (a >= 3) {
            float s = a - 3 < 5 ? SL[a - 3] : 1.0f;
            puffy_text("PlayStation", 2, 1, 0, px, 28, s, s, 0.03f, PS, PAL_RAMP(RAMP_STONE, 1), 2, PAL_WHITE, 2.0f);
        }
    }
    if (f >= F_YES) bcap(136, "Yes, the actual PlayStation.", PAL_WHITE, oc, 1, (f - F_YES) * 2);
    if (f >= F_GREY) bcap(148, "The grey one.", PAL_GRAY_L, oc, 1, (f - F_GREY) * 2);

    if (f >= F_LAND && f < F_LAND + 2) tk_post_shake(0, 2);
    if (f >= F_CLUNK && f < F_CLUNK + 3) tk_post_shake((f & 1) ? 2 : -2, 2);
    if (f >= F_CLAP && f < F_CLAP + 2) tk_post_flash(PAL_WHITE, 5);
}

void scene_s14_platforms_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    tk_sfx(t0, SFX_WHOOSH, 0.35f);
    tk_sfx(t0 + F_LAND * fr, SFX_THUD, 0.6f);
    tk_sfx(t0 + F_LAND * fr, SFX_BOING, 0.4f);
    /* a generic handheld power-on "ping" */
    tk_note(t0 + F_ON * fr, 0.09, 84, TKV_SQ50, 0.25f);
    tk_note(t0 + (F_ON + 3) * fr, 0.45, 96, TKV_SQ25, 0.22f);
    tk_note(t0 + (F_ON + 3) * fr, 0.45, 91, TKV_SINE, 0.15f);
    tk_sfx(t0 + (F_ON + 4) * fr, SFX_SPARKLE, 0.3f);
    /* scoot */
    tk_slide(t0 + F_SLIDE * fr, 0.2, 72, 60, TKV_SQ12, 0.15f);
    /* the console: slide + CLUNK */
    tk_sfx(t0 + F_CON * fr, SFX_WHOOSH, 0.4f);
    tk_sfx(t0 + F_CLUNK * fr, SFX_THUD, 1.0f);
    tk_note(t0 + F_CLUNK * fr, 0.06, 40, TKV_NOISE, 0.4f);
    /* a deep, shimmering generic boot swell */
    tk_note(t0 + (F_CLUNK + 2) * fr, 1.4, 36, TKV_TRI, 0.28f);
    tk_note(t0 + (F_CLUNK + 2) * fr, 1.4, 43, TKV_TRI, 0.2f);
    tk_slide(t0 + (F_CLUNK + 2) * fr, 1.2, 67, 79, TKV_SINE, 0.1f);
    tk_slide(t0 + (F_CLUNK + 6) * fr, 1.0, 74, 86, TKV_SINE, 0.07f);
    /* lid pops, disc spins up */
    tk_sfx(t0 + (F_LID - 2) * fr, SFX_BLIP, 0.3f);
    tk_sfx(t0 + F_LID * fr, SFX_POP, 0.6f);
    tk_slide(t0 + F_DISC * fr, 0.5, 60, 96, TKV_SQ12, 0.12f);
    tk_sfx(t0 + (F_DISC + 6) * fr, SFX_SPARKLE, 0.3f);
    /* (deadpan: no sound under "Yes, the actual PlayStation.") */
    /* arms: boing boing, then CLAP */
    tk_sfx(t0 + F_ARMS * fr, SFX_BOING, 0.4f);
    tk_slide(t0 + (F_CLAP - 4) * fr, 0.12, 60, 84, TKV_NOISE, 0.15f);
    tk_note(t0 + F_CLAP * fr, 0.12, 100, TKV_NOISE, 0.9f);
    tk_note(t0 + F_CLAP * fr, 0.05, 60, TKV_NOISE, 0.6f);
    tk_sfx(t0 + (F_CLAP + 1) * fr, SFX_FANFARE, 0.4f);
    tk_sfx(t0 + (F_CLAP + 2) * fr, SFX_SPARKLE, 0.4f);
}
