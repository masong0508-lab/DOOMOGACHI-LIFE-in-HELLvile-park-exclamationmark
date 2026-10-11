/* s15_logo.c - the final lockup + post-credits sting.   (5.5 s, 165 frames)
 *
 * f  0- 29  A pastel sky (rainbow arc, puffy clouds, floating hearts and
 *           stars). The same puffy candy logo as the title card drops in
 *           letter by letter: DOOMOGACHI (its OO are eyes) / LIFE / in
 *           HELLvile Park!  Sparkles everywhere.
 * f 30- 43  FWOOMP: the HELL catches fire and the screen rips diagonally -
 *           below the glowing seam it's Doom brick with lava mortar and
 *           rising embers. The logo's eyes look down at the fire in shock...
 *           look at us... and go happy. "(it's fine.)" HELL keeps burning and
 *           dripping for the rest of the shot; the candy hills are devoured.
 * f 44-109  "IN DEVELOPMENT" (with a blinking cursor) + "GBA <3 PlayStation".
 *           A glossy shine sweeps across the logo. Fade to black.
 * f110-164  Post-credits, black screen, a beat of silence: the Doomogachi
 *           (glowing LCD pixels, still wearing its little horns) pops in.
 *           Strains... pfrrt: a poop. Dun: the poop grows tiny horns too.
 *           "beep."  Fade out.
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
#define F_IGNITE  30    /* HELL catches fire, the hell side rips in */
#define F_DEV     44    /* IN DEVELOPMENT                           */
#define F_SHINE   58
#define F_OUT    110    /* cut to black                             */
#define F_BLOB   120    /* post-credits: the Doomogachi pops in     */
#define F_STRAIN 127
#define F_PLOP   135
#define F_HORNS  144    /* ...the poop grows tiny horns too         */
#define F_BEEP   150
#define F_FADE   158

static Tile T1[10], T2[4], T3[17];
static int tiles_ok;
static const char LOGO1[] = "DOOMOGACHI";
static const char LOGO2[] = "LIFE";
static const char SUB[] = "in HELLvile Park!";
#define L1_ADV 19
#define L1_BOT 38
#define L2_ADV 25
#define L2_BOT 76
#define SUB_ADV 12
#define SUB_BOT 100

enum { EYE_OPEN, EYE_HAPPY, EYE_SHOCK, EYE_BLINK };

/* the seam between the two worlds: hell is below it */
static float seam_t;   /* 0 = all pastel .. 1 = settled */
static float seam_y(float x, int f) {
    float base = 64 + (x + 20) * 52 / 280.0f;
    float rag = 1.6f * sinf(x * 0.31f + f * 0.2f) + 1.0f * sinf(x * 0.9f - f * 0.3f);
    return base + rag + (1 - seam_t) * 140;
}

/* ---- pastel world ---------------------------------------------------------- */
static void pastel(int f) {
    static const uint8_t SK[4] = { PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_PASTEL_PINK, 7) };
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) tk_fb[y][x] = qtone(SK, 4, y / 120.0f * 3.2f, x, y);
    /* soft rainbow arc */
    static const int RB[6] = { PAINT_BUBBLEGUM, PAINT_PEACH, PAINT_LEMON, PAINT_MINT, PAINT_SKY, PAINT_LAVENDER };
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float d = sqrtf((x - 120.0f) * (x - 120.0f) + (y - 150.0f) * (y - 150.0f));
            int band = (int)((d - 108) / 4);
            if (band >= 0 && band < 6) {
                int pi = PAL_PAINT(RB[5 - band], 2);
                tint_px(x, y, cmap(TK_PAL[pi][0], TK_PAL[pi][1], TK_PAL[pi][2], 6));
            }
        }
    for (int i = 0; i < 5; i++) {
        float cx = fmodf(i * 61.0f + f * (0.2f + i * 0.04f), 290) - 25, cy = 10 + (i % 3) * 22;
        mk_ellipse(cx, cy + 2, 14, 4.2f); mk_ellipse(cx - 7, cy, 6, 4.5f); mk_ellipse(cx + 2, cy - 3, 8, 6.5f); mk_ellipse(cx + 9, cy, 5, 4.2f);
        static const uint8_t CL[4] = { PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE, PAL_WHITE };
        pillow(CL, PAL_RAMP(RAMP_VAPE, 6), 2.5f, 1.1f);
    }
    /* rolling candy hills with lollipop trees and flowers (soon to be devoured) */
    for (int layer = 0; layer < 2; layer++) {
        static const uint8_t HL[2][4] = { { PAL_RAMP(RAMP_MINT, 4), PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_MINT, 7) },
                                          { PAL_RAMP(RAMP_GRASS, 4), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 7) } };
        for (int x = 0; x < TW; x++) {
            float top = layer ? 134 + 6 * sinf(x * 0.03f + 2.0f) + 3 * sinf(x * 0.09f) : 118 + 8 * sinf(x * 0.022f + 0.5f) + 3 * sinf(x * 0.07f + 1);
            for (int y = (int)top; y < TH; y++) {
                float d = y - top;
                tk_px(x, y, qtone(HL[layer], 4, 3.2f - d * 0.12f - (d < 1.5f ? -0.8f : 0), x, y));
            }
        }
        for (int i = 0; i < 5; i++) {
            float x = 12 + i * 52 + layer * 24, base = layer ? 138 + 6 * sinf(x * 0.03f + 2.0f) : 122 + 8 * sinf(x * 0.022f + 0.5f);
            if (layer == 0) {     /* lollipop trees */
                tk_rect((int)x, (int)base - 12, 2, 12, PAL_WHITE);
                mk_ellipse(x + 1, base - 15, 5, 5);
                static const int LP[3] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_LAVENDER };
                uint8_t r4[4] = { PAL_PAINT(LP[i % 3], 0), PAL_PAINT(LP[i % 3], 1), PAL_PAINT(LP[i % 3], 2), PAL_WHITE };
                pillow(r4, PAL_PAINT(LP[i % 3], 0), 2.0f, 0.4f);
            } else {              /* flowers */
                for (int k = 0; k < 3; k++) {
                    int fx = (int)x + k * 9, fy = (int)base + 4 + (k & 1) * 3 + (int)sinf(f * 0.2f + k + i);
                    uint8_t pc = PAL_PAINT(k == 1 ? PAINT_BUBBLEGUM : PAINT_WHITE, 2);
                    tk_px(fx - 1, fy, pc); tk_px(fx + 1, fy, pc); tk_px(fx, fy - 1, pc); tk_px(fx, fy + 1, pc);
                    tk_px(fx, fy, PAL_PAINT(PAINT_LEMON, 1));
                }
            }
        }
    }
    for (int i = 0; i < 14; i++) {
        uint32_t h = tk_hash((uint32_t)i * 191u + 7u);
        int x = (int)((h % 250) - 5 + 4 * sinf(f * 0.07f + i));
        int y = (int)(fmodf((float)((h >> 9) % 170) - f * 0.4f + 340.0f, 170.0f)) - 5;
        if (i & 1) tk_heart(x, y, 1, PAL_PAINT((i % 3) ? PAINT_BUBBLEGUM : PAINT_LAVENDER, 2));
        else { int ph = (f + i * 5) % 24; tk_star(x, y, ph < 12 ? 1 : 2, ph & 4 ? PAL_WHITE : PAL_UI_GOLD); }
    }
}
/* ---- hell world (drawn over the pastel one, below the seam) ------------------ */
static void hell(int f) {
    if (seam_t <= 0) return;
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float sy = seam_y((float)x, f);
            if (y < sy) continue;
            float d = y - sy;
            uint8_t c;
            /* doom-ish brick with glowing mortar, fading into the dark */
            int row = (int)((y + 200) / 7), bx = (x + (row & 1) * 7 + 400) % 14;
            int mortar = ((y + 200) % 7 == 0) || bx == 0;
            if (d < 2.5f) c = d < 1.2f ? PAL_RAMP(RAMP_LAVA, 7) : PAL_RAMP(RAMP_LAVA, 5);
            else if (mortar) c = d < 30 ? PAL_RAMP(RAMP_LAVA, 3) : PAL_RAMP(RAMP_BLOOD, 2);
            else {
                float L = 2.4f - d / 22.0f;
                static const uint8_t HB[4] = { PAL_RAMP(RAMP_BLOOD, 0), PAL_RAMP(RAMP_BLOOD, 1), PAL_RAMP(RAMP_BLOOD, 2), PAL_RAMP(RAMP_BLOOD, 3) };
                c = qtone(HB, 4, L, x, y);
            }
            tk_fb[y][x] = c;
        }
    /* the seam glows */
    for (int x = 0; x < TW; x++) {
        float sy = seam_y((float)x, f);
        for (int k = 1; k < 7; k++)
            if (BY4[((int)sy - k) & 3][x & 3] < 14 - k * 2) tint_px(x, (int)sy - k, cmap(255, 140, 60, 8));
    }
    /* embers rising */
    for (int i = 0; i < 40; i++) {
        uint32_t h = tk_hash((uint32_t)i * 41u + 3u);
        float x = (float)(h % 240) + 3 * sinf(f * 0.1f + i);
        float y = TH - fmodf((float)((h >> 8) % 160) + f * (0.6f + (h % 5) * 0.2f), 160.0f);
        if (y < seam_y(x, f) + 3) continue;
        tk_px((int)x, (int)y, (h & 1) ? PAL_RAMP(RAMP_LAVA, 6) : PAL_RAMP(RAMP_LAVA, 4));
        if ((h & 6) == 0) tk_px((int)x, (int)y + 1, PAL_RAMP(RAMP_LAVA, 3));
    }
}

/* ---- the logo (same puffy letters as the title card) --------------------------- */
static void drop_motion(int a, int fall, float height, float *dy, float *sx, float *sy) {
    *dy = 0; *sx = 1; *sy = 1;
    if (a < fall) { float t = (float)(a + 1) / fall; *dy = -height * (1 - t * t); *sx = 0.82f; *sy = 1.25f; return; }
    a -= fall;
    static const float SX[] = { 1.38f, 1.2f, 0.9f, 0.96f, 1.03f, 1.0f };
    static const float SY_[] = { 0.62f, 0.8f, 1.12f, 1.05f, 0.98f, 1.0f };
    static const float DY[] = { 0, 0, -4, -4, -1, 0 };
    if (a < 6) { *sx = SX[a]; *sy = SY_[a]; *dy = DY[a]; }
}
static void logo_eye(float x, float y, int state, float lx, float ly) {
    uint8_t k = PAL_RAMP(RAMP_VAPE, 0);
    mk_ellipse(x, y, 4.0f, 6.8f);
    static const uint8_t EW[4] = { PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE, PAL_WHITE };
    pillow(EW, 0, 2.0f, 0.4f);
    switch (state) {
    case EYE_OPEN:
        tk_ellipse((int)(x + lx), (int)(y + ly), 2, 3, k);
        tk_rect((int)(x + lx) - 1, (int)(y + ly) - 2, 2, 2, PAL_WHITE);
        tk_px((int)(x + lx) + 1, (int)(y + ly) + 2, PAL_RAMP(RAMP_VAPE, 4));
        break;
    case EYE_SHOCK: tk_rect((int)(x + lx), (int)(y + ly), 2, 2, k); break;
    case EYE_HAPPY: for (int i = -3; i <= 3; i++) tk_rect((int)x + i, (int)(y + 1 - (3 - abs(i)) * 0.9f), 1, 2, k); break;
    default: tk_rect((int)x - 3, (int)y, 7, 1, k); break;
    }
}
static float hell_x(int i) { return 120 - 8.0f * SUB_ADV + i * SUB_ADV; }
static void flames_behind(int f) {
    if (f < F_IGNITE) return;
    int a = f - F_IGNITE;
    float grow = a < 6 ? 0.4f + a * 0.2f : 1.0f + 0.08f * sinf(f * 0.5f);
    for (int i = 3; i <= 6; i++) {
        float x = hell_x(i);
        for (int k = 0; k < 3; k++) {
            uint32_t h = tk_hash((uint32_t)(f / 2 + i * 7 + k * 31));
            float fx = x - 4 + k * 4 + ((int)(h % 3) - 1), hgt = (8 + (h % 9) + ((i + k) & 1) * 6) * grow;
            float tipx = fx + sinf(f * 0.4f + k + i) * 2.5f;
            mk_tri(fx - 4, SUB_BOT - 8, fx + 4, SUB_BOT - 8, tipx, SUB_BOT - 16 - hgt);
            mk_ellipse(fx, SUB_BOT - 10, 4.5f, 5.5f);
            mk_ellipse((fx + tipx) * 0.5f, SUB_BOT - 14 - hgt * 0.35f, 2.6f, 3.5f);
        }
    }
    static const uint8_t FL[4] = { PAL_RAMP(RAMP_LAVA, 3), PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_LAVA, 6), PAL_RAMP(RAMP_LAVA, 7) };
    pillow(FL, PAL_RAMP(RAMP_BLOOD, 2), 3.0f, -1.2f);
    /* heat glow */
    tint_ellipse(hell_x(4) + 6, SUB_BOT - 12, 34, 20, cmap(255, 150, 60, 5), 0.6f, 16);
}
static void flames_front(int f) {
    if (f < F_IGNITE) return;
    for (int i = 3; i <= 6; i++) {
        float x = hell_x(i);
        for (int k = 0; k < 2; k++) {
            uint32_t h = tk_hash((uint32_t)(f / 2 + i * 13 + k * 5));
            float fx = x - 3 + k * 6 + ((int)(h % 3) - 1), hgt = 3 + (h % 4);
            mk_tri(fx - 1.6f, SUB_BOT - 15, fx + 1.6f, SUB_BOT - 15, fx + sinf(f * 0.5f + k * 2 + i), SUB_BOT - 16 - hgt);
        }
    }
    static const uint8_t FL[4] = { PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_LAVA, 6), PAL_RAMP(RAMP_LAVA, 7), PAL_WHITE };
    pillow(FL, 0, 1.5f, -1.0f);
}
static void red_drop(float x, float y, float r, float neck_top) {
    if (neck_top < y) {
        float w = r * 0.45f;
        for (float yy = neck_top; yy < y; yy += 1.0f) {
            float t = (yy - neck_top) / (y - neck_top + 0.01f);
            mk_ellipse(x, yy, 1.0f + w * t * t, 1.0f);
        }
    }
    mk_ellipse(x, y, r, r * 1.1f);
    static const uint8_t DR[4] = { PAL_PAINT(PAINT_BLOOD, 1), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_CHERRY, 2), PAL_WHITE };
    pillow(DR, PAL_RAMP(RAMP_BLOOD, 1), r * 0.6f + 1, 0.4f);
    tk_px((int)(x - r * 0.4f), (int)(y - r * 0.4f), PAL_WHITE);
}
static void drips(int f) {
    if (f < F_IGNITE + 4) return;
    for (int k = 0; k < 4; k++) {
        int per = 26 + k * 5, a = (f - F_IGNITE - 4 + k * 9) % per;
        float x = hell_x(3 + k) + (k & 1 ? 2 : -2), top = SUB_BOT - 1;
        if (a < 12) { float len = a / 3.0f, r = 1.0f + a * 0.12f; red_drop(x, top + len + r, r, top); }
        else {
            float t = (float)(a - 12);
            float y = top + 6 + 0.5f * 0.5f * t * t;
            if (y < TH + 4) red_drop(x, y, 2.4f, y - 3);
        }
    }
}
static void logo(int f, float jx, float jy, int eye, float lx, float ly) {
    const uint8_t *shadow = cmap(90, 40, 120, 7);
    uint8_t ol = PAL_RAMP(RAMP_VAPE, 1);
    static const int P1[10] = { PAINT_BUBBLEGUM, PAINT_SKY, PAINT_MINT, PAINT_LAVENDER, PAINT_LEMON,
                                PAINT_PEACH, PAINT_BUBBLEGUM, PAINT_SKY, PAINT_MINT, PAINT_LAVENDER };
    float x1 = 120 - 4.5f * L1_ADV, x2 = 120 - 1.5f * L2_ADV;
    float eyex[2] = { 0, 0 }, eyey[2] = { 0, 0 };
    int eyes_on = 0;
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < 10; i++) {
            int a = f - (i * 2) / 2;
            if (a < 0) continue;
            float dy, sx, sy;
            drop_motion(a, 6, 70, &dy, &sx, &sy);
            float wob = f > 24 ? 1.2f * sinf(f * 0.17f + i * 0.7f) : 0;
            float cx = x1 + i * L1_ADV + jx, by = L1_BOT + dy + wob + jy;
            tile_mk(&T1[i], cx, by, sx, sy);
            if (pass == 0) { mk_ring(3, 2, 3, 0, shadow); mk_reset(); }
            else if (pass == 1) { mk_ring(3, 0, 0, PAL_WHITE, NULL); mk_reset(); }
            else {
                int p = P1[i];
                uint8_t r4[4] = { PAL_PAINT(p, 0), PAL_PAINT(p, 1), PAL_PAINT(p, 2), PAL_WHITE };
                if (p == PAINT_LEMON) { r4[0] = PAL_PAINT(PAINT_HONEY, 1); r4[1] = PAL_PAINT(PAINT_LEMON, 1); }
                if (p == PAINT_PEACH) r4[0] = PAL_RAMP(RAMP_PEACH, 4);
                pillow(r4, ol, 2.4f, 0.6f);
                if ((i == 1 || i == 2) && a >= 10) { eyex[i - 1] = cx + 1; eyey[i - 1] = by - 13.5f * sy; eyes_on = 1; }
            }
        }
        int a = f - 10;
        if (a >= 0) {
            float dy, sx, sy;
            drop_motion(a, 4, 100, &dy, &sx, &sy);
            for (int i = 0; i < 4; i++) {
                float wob = f > 24 ? 1.2f * sinf(f * 0.17f + i * 0.9f + 2) : 0;
                float cx = 120 + (x2 + i * L2_ADV - 120) * sx + jx;
                tile_mk(&T2[i], cx, L2_BOT + dy + wob + jy, sx, sy);
                if (pass == 0) { mk_ring(3, 2, 3, 0, shadow); mk_reset(); }
                else if (pass == 1) { mk_ring(3, 0, 0, PAL_WHITE, NULL); mk_reset(); }
                else {
                    static const uint8_t LR[4] = { PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2) };
                    pillow(LR, ol, 3.0f, 0.6f);
                }
            }
        }
        for (int i = 0; SUB[i]; i++) {
            int a2 = f - 16 - i / 2;
            if (a2 < 0 || SUB[i] == ' ') continue;
            float sx = 1, sy = 1, dy = 0;
            if (a2 < 4) { static const float S[4] = { 0.4f, 1.3f, 0.9f, 1.05f }; sx = sy = S[a2]; dy = a2 == 0 ? -3 : 0; }
            int hl = i >= 3 && i <= 6;
            float wob = f > 30 && !hl ? 1.0f * sinf(f * 0.2f + i * 0.6f) : 0;
            if (hl && f >= F_IGNITE && f < F_IGNITE + 6) dy -= (F_IGNITE + 6 - f) * 0.8f;     /* the fwoomp jolts it */
            tile_mk(&T3[i], hell_x(i) + jx, SUB_BOT + dy + wob + jy, sx, sy);
            if (pass == 0) { mk_ring(2, 1, 2, 0, shadow); mk_reset(); }
            else if (pass == 1) { mk_ring(2, 0, 0, hl && f >= F_IGNITE ? PAL_RAMP(RAMP_LAVA, 7) : PAL_WHITE, NULL); mk_reset(); }
            else if (hl) {
                static const uint8_t HR[4] = { PAL_PAINT(PAINT_BLOOD, 1), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_CHERRY, 2), PAL_PAINT(PAINT_ROSE, 2) };
                static const uint8_t HF[4] = { PAL_RAMP(RAMP_BLOOD, 1), PAL_RAMP(RAMP_BLOOD, 3), PAL_UI_RED, PAL_RAMP(RAMP_LAVA, 6) };
                pillow(f >= F_IGNITE ? HF : HR, PAL_RAMP(RAMP_BLOOD, 0), 2.0f, 0.5f);
            } else {
                static const uint8_t WR[4] = { PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE };
                pillow(WR, ol, 2.0f, 0.5f);
            }
        }
    }
    if (eyes_on)
        for (int e = 0; e < 2; e++) logo_eye(eyex[e], eyey[e], eye, lx, ly);
}

/* ---- the post-credits Doomogachi (LCD pixels glowing in the dark) ------------ */
static const char *BLOB[] = { "....####....", "..##....##..", ".#........#.", "#..##..##..#", "#..##..##..#",
                              "#..........#", "#....##....#", ".#........#.", "..########..", "...#....#...", NULL };
static const char *BLOB_SQ[] = { "....######....", "..##......##..", ".#..##..##..#.", "#...##..##...#",
                                 "#.....##.....#", ".#..........#.", "..##########..", "...##....##...", NULL };
static const char *BLOB_STRAIN[] = { "....####....", "..##....##..", ".#........#.", "#.#......#.#", "#..#....#..#",
                                     "#.#......#.#", "#...####...#", ".#........#.", "..########..", "...#....#...", NULL };
static const char *BLOB_HAPPY[] = { "....####....", "..##....##..", ".#........#.", "#..#....#..#", "#.#.#..#.#.#",
                                    "#..........#", "#...####...#", ".#...##...#.", "..########..", "...#....#...", NULL };
static const char *HORNLETS[] = { ".#........#.", "..#......#..", NULL };
static const char *POOP[] = { "...#...", "..##...", ".####..", ".#.#.#.", "#######", "#######", NULL };
static const char *POOP_HORNS[] = { "#.....#", ".#...#.", NULL };
static void lcd_spr(const char *const *rows, float ox, float oy, int ps, int on) {
    for (int j = 0; rows[j]; j++)
        for (int i = 0; rows[j][i]; i++) {
            if (rows[j][i] != '#') continue;
            int x = (int)ox + i * ps, y = (int)oy + j * ps;
            tk_rect(x + 1, y + 1, ps - 1, ps - 1, PAL_RAMP(RAMP_SICK, 0));          /* LCD drop shadow */
            tk_rect(x, y, ps - 1, ps - 1, on ? PAL_UI_LCD : PAL_RAMP(RAMP_SICK, 3));
            tk_rect(x, y, ps - 1, 1, PAL_RAMP(RAMP_GRASS, 7));
        }
}
static void sting(int f) {
    tk_clear(PAL_BLACK);
    if (f < F_BLOB) return;
    int a = f - F_BLOB;
    const int PS = 6;
    /* faint LCD glow in the dark */
    const char *const *spr = BLOB;
    float bx = 78, by = 52;
    float sc = 1;
    if (a < 4) { static const float S[4] = { 0.2f, 1.25f, 0.9f, 1.05f }; sc = S[a]; }
    if (f >= F_STRAIN && f < F_PLOP) { spr = BLOB_STRAIN; bx += (f & 1) ? 1 : -1; }
    else if (f >= F_PLOP && f < F_PLOP + 3) { spr = BLOB_SQ; bx -= PS; by += 2 * PS; }
    else if (f >= F_PLOP + 3 && f < F_BEEP + 2) spr = BLOB_HAPPY;
    if (sc != 1) {
        /* pop-in: draw scaled by sampling the sprite */
        int w = 12, h = 10;
        float cx = bx + w * PS / 2.0f, cy = by + h * PS / 2.0f;
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++)
                if (BLOB[j][i] == '#') {
                    int x = (int)(cx + (i * PS - w * PS / 2.0f) * sc), y = (int)(cy + (j * PS - h * PS / 2.0f) * sc);
                    int s = (int)(PS * sc); if (s < 2) s = 2;
                    tk_rect(x, y, s - 1, s - 1, PAL_UI_LCD);
                }
    } else {
        lcd_spr(HORNLETS, bx + (spr == BLOB_SQ ? PS : 0), by - 2 * PS + (spr == BLOB_SQ ? 2 * PS : 0), PS, 1);
        lcd_spr(spr, bx, by, PS, 1);
    }
    /* strain sweat */
    if (f >= F_STRAIN && f < F_PLOP) {
        int ph = (f - F_STRAIN) % 6;
        tk_rect((int)bx - 8, (int)by + 6 + ph * 2, 3, 4, PAL_UI_LCD);
    }
    /* plop: a poop. it also has tiny horns. */
    if (f >= F_PLOP) {
        int pa = f - F_PLOP;
        float px = 164, py = 82;
        float dy = pa < 3 ? -(3 - pa) * 6.0f : 0;
        if (pa >= 3 && pa < 5) dy = pa == 3 ? 3 : 1;
        lcd_spr(POOP, px, py + dy, PS, 1);
        if (f >= F_HORNS) lcd_spr(POOP_HORNS, px, py - 2 * PS, PS, 1);
        /* stink lines */
        for (int k = 0; k < 2; k++) {
            int ph = ((f / 5) + k) & 1;
            for (int j = 0; j < 4; j++)
                tk_rect((int)px - 10 + k * 52 + ((j + ph) & 1) * PS, (int)py - PS - j * PS, PS - 1, PS - 1, PAL_RAMP(RAMP_SICK, 3));
        }
    }
    /* beep. */
    if (f >= F_BEEP) {
        int ba = f - F_BEEP;
        float s = ba < 4 ? (float[]){ 0.4f, 1.2f, 0.92f, 1.04f }[ba] : 1.0f;
        int w = (int)(76 * s), h = (int)(22 * s);
        int x = 100 - w / 2, y = 20 - h / 2;
        mk_rrect((float)x, (float)y, (float)w, (float)h, 7 * s);
        mk_tri((float)x + 8, (float)y + h - 3, (float)x + 18, (float)y + h - 3, (float)x + 4, (float)y + h + 9);
        static const uint8_t BW[4] = { PAL_RAMP(RAMP_GRASS, 6), PAL_UI_LCD, PAL_UI_LCD, PAL_WHITE };
        pillow(BW, PAL_RAMP(RAMP_SICK, 1), 2.0f, 0.3f);
        if (ba >= 2) tk_text(100 - tk_text_w("beep.", 2) / 2, 13, "beep.", PAL_RAMP(RAMP_SICK, 0), 2);
    }
    tk_post_scanlines();
}

void scene_s15_logo(int f, int n) {
    (void)n;
    if (!tiles_ok) {
        tiles_ok = 1;
        for (int i = 0; i < 10; i++) make_tile(&T1[i], (unsigned char)LOGO1[i], 3, 2);
        for (int i = 0; i < 4; i++) make_tile(&T2[i], (unsigned char)LOGO2[i], 4, 2);
        for (int i = 0; SUB[i]; i++) make_tile(&T3[i], (unsigned char)SUB[i], 2, 1);
    }
    if (f >= F_OUT) {
        sting(f);
        if (f >= F_FADE) tk_post_fade((f - F_FADE) * 16 / (n - F_FADE));
        return;
    }
    seam_t = f < F_IGNITE ? 0 : tk_bounce((f - F_IGNITE) / 10.0f);
    pastel(f);
    hell(f);

    /* logo state */
    float jx = 0, jy = 0;
    if (f >= F_IGNITE && f < F_IGNITE + 6) { jx = (f & 1) ? 2.0f : -2.0f; jy = (f & 2) ? 1 : -1; }
    int eye = EYE_OPEN;
    float lx = 0, ly = 0;
    if (f < 18) eye = EYE_BLINK;
    else if (f < F_IGNITE) { lx = ((f / 5) & 1) ? 1.2f : -1.2f; ly = -1; }
    else if (f < F_IGNITE + 14) { eye = EYE_SHOCK; lx = -1; ly = 3; }       /* looks down at the fire */
    else if (f < F_IGNITE + 22) { lx = 0; ly = 0; }                         /* ...looks at us */
    else if (f < 90) eye = EYE_HAPPY;                                       /* it's fine. */
    else { lx = 1.5f; ly = 1; }
    if (f == 70 || f == 71 || f == 100) eye = EYE_BLINK;

    flames_behind(f);
    layer_begin();
    logo(f, jx, jy, eye, lx, ly);
    if (f >= F_SHINE && f < F_SHINE + 10) {
        layer_band(cmap(255, 255, 255, 10), 30 + (f - F_SHINE) * 32, 8);
        layer_band(cmap(255, 255, 255, 7), 30 + (f - F_SHINE) * 32 + 12, 3);
    }
    flames_front(f);
    drips(f);
    /* sparkles on the cute part */
    if (f >= 12) tk_sparkles(120, 40, f - 12, 21, 96);
    if (f >= 30) tk_sparkles(176, 26, f - 30, 22, 40);

    /* callback: the logo has seen worse */
    if (f >= F_IGNITE + 22 && f < 84) {
        int a = f - F_IGNITE - 22;
        tk_text_outline(150, 106 + (a == 0 ? -2 : a == 1 ? 1 : 0), "(it's fine.)", PAL_WHITE, PAL_RAMP(RAMP_BLOOD, 0), 1);
    }
    /* IN DEVELOPMENT + platforms */
    if (f >= F_DEV) {
        int a = f - F_DEV;
        bcap(126, "IN DEVELOPMENT", PAL_WHITE, PAL_RAMP(RAMP_BLOOD, 0), 1, a * 2);
        if (a >= 10) {
            const char *pl = "GBA  \x7f  PlayStation";
            int w = tk_text_w(pl, 1);
            int dy = a == 10 ? -2 : 0;
            tk_text_outline(120 - w / 2, 138 + dy, pl, PAL_RAMP(RAMP_LAVA, 6), PAL_RAMP(RAMP_BLOOD, 0), 1);
        }
        /* a blinking work-in-progress cursor */
        if ((f / 8) & 1) tk_rect(120 + tk_text_w("IN DEVELOPMENT", 1) / 2 + 3, 122, 4, 8, PAL_WHITE);
    }

    if (f >= F_IGNITE && f < F_IGNITE + 3) tk_post_flash(PAL_RAMP(RAMP_LAVA, 6), 8 - (f - F_IGNITE) * 3);
    if (f >= F_IGNITE && f < F_IGNITE + 5) tk_post_shake((f & 1) ? 3 : -3, (f & 1) ? -2 : 2);
    if (f < 2) tk_post_flash(PAL_WHITE, 8 - f * 4);
    if (f >= F_OUT - 4) tk_post_fade((f - (F_OUT - 4)) * 4);
}

void scene_s15_logo_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    /* letters drop in */
    static const int notes[10] = { 72, 74, 76, 77, 79, 81, 83, 84, 86, 88 };
    for (int i = 0; i < 10; i++) tk_slide(t0 + (i + 6) * fr, 0.05, notes[i] - 5, notes[i], TKV_SQ25, 0.18f);
    tk_sfx(t0 + 14 * fr, SFX_THUD, 0.9f);
    tk_sfx(t0 + 14 * fr, SFX_BOING, 0.5f);
    tk_sfx(t0 + 16 * fr, SFX_SPARKLE, 0.4f);
    /* HELL ignites: FWOOMP + the world rips */
    tk_sfx(t0 + F_IGNITE * fr, SFX_EXPLOSION, 0.6f);
    tk_sfx(t0 + F_IGNITE * fr, SFX_WHOOSH, 0.6f);
    tk_sfx(t0 + (F_IGNITE + 1) * fr, SFX_ROAR, 0.5f);
    tk_note(t0 + (F_IGNITE + 2) * fr, 1.6, 33, TKV_SAW, 0.2f);
    /* angelic choir answers from the pastel side */
    tk_sfx(t0 + (F_IGNITE + 14) * fr, SFX_CHOIR, 0.45f);
    tk_sfx(t0 + F_DEV * fr, SFX_SELECT, 0.4f);
    tk_sfx(t0 + F_SHINE * fr, SFX_SPARKLE, 0.45f);
    /* drips */
    for (int k = 0; k < 4; k++) tk_note(t0 + (F_IGNITE + 16 + k * 14) * fr, 0.07, 52 - k, TKV_SQ12, 0.12f);
    /* ---- post-credits: silence... pop. nnngh. pfrrt. beep. ---- */
    tk_sfx(t0 + F_BLOB * fr, SFX_POP, 0.8f);
    tk_slide(t0 + F_STRAIN * fr, 0.28, 48, 60, TKV_TRI, 0.35f);
    tk_slide(t0 + F_PLOP * fr, 0.16, 50, 36, TKV_SAW, 0.4f);
    tk_sfx(t0 + F_PLOP * fr, SFX_SPLAT, 0.2f);
    tk_note(t0 + F_HORNS * fr, 0.12, 40, TKV_SQ12, 0.25f);                    /* dun. (the poop grows horns) */
    tk_note(t0 + F_HORNS * fr, 0.12, 46, TKV_SQ12, 0.2f);
    tk_note(t0 + (F_BEEP + 1) * fr, 0.12, 88, TKV_SQ50, 0.32f);              /* beep. */
}
