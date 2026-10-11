/* s13_ending.c - the ending.   (5.0 s, 150 frames)
 *
 * f  0- 35  Hellvile Park at sunset: banded violet-to-gold sky with first
 *           stars and sweeping sun rays, clouds lit from below, Hillvile's
 *           round cottages on the left, Hellvile's smoking spires on the
 *           right, lamp posts and fairy lights. The Wall in the middle (still
 *           with its KEEP OUT sign and the taped "pls <3" note). Both sides
 *           stare at it, sad. "...and maybe, just maybe..." It rumbles. Cracks.
 * f 36- 51  It comes down block by block into a rubble mound, dust puffs,
 *           shake. "!" over every head.
 * f 52- 97  Everyone runs over the rubble and hugs: hearts everywhere. Up
 *           front our fox hero and a goth cat from Hellvile share a big plush
 *           heart; the party bunny (cured!) bounces in with a heart balloon.
 *           Heart-shaped fireworks. Dancing. "everyone can get along."
 * f 98-149  The camera tilts down to the foreground: a mountain of brass
 *           shell casings and shotgun shells, Lord Vapula's dented crown on
 *           top, one last sad wisp of vape leaving it.
 *           "(after a LOT of shooting.)"  ...one casing rolls off. tink.
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
#define F_CAP1     4    /* "...and maybe, just maybe..."   */
#define F_RUMBLE  16    /* the wall starts to shake + crack */
#define F_CRUMBLE 36    /* down it comes                    */
#define F_SEE     46    /* "!" - they see each other        */
#define F_RUN     52
#define F_HUG     64
#define F_CAP2    64    /* "everyone can get along."        */
#define F_FIRE    70    /* fireworks                         */
#define F_DANCE   80
#define F_TILT    98    /* camera tilts down to the foreground pile */
#define F_CAP3   110    /* "(after a LOT of shooting.)"      */
#define F_TINK   114

#define GY 112          /* ground line (feet of the mid row) */
#define WX0 88          /* the Wall: big, dark, right in front of the sunset */
#define WX1 152
#define WTOP (-12)
#define WBOT 118

static float camy;      /* camera tilt: + looks down */
static float SY(float y, float depth) { return y - camy * depth; }

/* the pink party bunny again - back to her old self */
static const TkChibi BUN = {
    PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_LEMON, 1),
    PAL_PAINT(PAINT_TEAL, 1), TKH_BALD, 0, 0, 0, TKT_BUNNY, 1, 1,
    TKEAR_BUNNY, PAL_PAINT(PAINT_WHITE, 2), 1 };
/* callbacks: Kevin (s05, mint pup, three eyes) and the starer (s06) */
static const TkChibi KEVIN = {
    PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_LEMON, 1),
    PAL_PAINT(PAINT_SKY, 1), TKH_BALD, 0, 0, 0, TKT_PUP, 1, 0,
    TKEAR_PUP, PAL_PAINT(PAINT_WHITE, 1), 1 };
static const TkChibi STARER = {
    PAL_SKIN(SKIN_FAIR, 1), PAL_PAINT(PAINT_CHOCOLATE, 1), PAL_PAINT(PAINT_SILVER, 0),
    PAL_PAINT(PAINT_CHARCOAL, 1), TKH_SHORT, 0, 0, 0, 0, 0, 0,
    TKEAR_HUMAN, 0, 0 };
static void fill_ell(float cx, float cy, float rx, float ry, uint8_t c) {
    for (int y = (int)floorf(cy - ry - 1); y <= (int)ceilf(cy + ry + 1); y++)
        for (int x = (int)floorf(cx - rx - 1); x <= (int)ceilf(cx + rx + 1); x++) {
            float fx = (x + .5f - cx) / rx, fy = (y + .5f - cy) / ry;
            if (fx * fx + fy * fy <= 1) tk_px(x, y, c);
        }
}
/* Kevin's third eye, same geometry as the s05 close-up, at size 1 */
static void kevin_eye(int x, int yf, int bob) {
    float hb = sinf(bob * 0.16f) * 0.45f;
    float ey = yf + (12.2f + hb - 4.6f - 34.1f) + 0.5f;
    fill_ell(x + 0.5f, ey, 1.9f, 2.3f, PAL_PAINT(PAINT_JET, 0));
    fill_ell(x + 0.5f, ey + 0.4f, 1.2f, 1.5f, PAL_PAINT(PAINT_SKY, 1));
    tk_px(x, (int)(ey - 0.8f), PAL_WHITE);
}

/* ---- sky, sun, far towns, lawn --------------------------------------------- */
static void sky(int f) {
    static const uint8_t SK[6] = { PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 4),
                                   PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_CANDY, 6) };
    for (int y = 0; y < TH; y++) {
        float wy = y + camy * 0.4f;
        float L = (wy + 40) / 124.0f * 5.0f;
        for (int x = 0; x < TW; x++) tk_fb[y][x] = qtone(SK, 6, L, x, y);
    }
    /* first stars */
    for (int i = 0; i < 30; i++) {
        uint32_t h = tk_hash((uint32_t)i * 59u + 1u);
        int x = (int)(h % 240), y = (int)SY((float)((h >> 9) % 46), 0.4f);
        int ph = (f + (int)(h >> 20) % 50) % 50;
        if (ph < 36) tk_px(x, y, ph < 18 ? PAL_WHITE : PAL_RAMP(RAMP_VAPE, 7));
        if ((h & 7) == 0 && ph < 10) tk_star(x, y, 1, PAL_WHITE);
    }
    /* the setting sun, right behind the wall */
    float sy = SY(76, 0.5f);
    tint_ellipse(120, sy, 90, 46, cmap(255, 214, 160, 6), 0.7f, 16);
    tint_ellipse(120, sy, 46, 30, cmap(255, 236, 190, 8), 0.6f, 16);
    tk_circle(120, (int)sy, 17, PAL_RAMP(RAMP_CANDY, 6));
    tk_circle(120, (int)sy, 15, PAL_RAMP(RAMP_CANDY, 7));
    tk_circle(116, (int)sy - 4, 7, PAL_WHITE);
    /* soft sun rays sweeping */
    const uint8_t *ray = cmap(255, 240, 210, 4);
    for (int y = 0; y < (int)sy + 10 && y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float a = atan2f(y - sy, x - 120.0f) + f * 0.004f;
            if (fmodf(a * 9.0f + 31.4f, 2.0f) < 0.5f && ((x + y) & 1)) tint_px(x, y, ray);
        }
}
static void clouds(int f) {
    for (int i = 0; i < 5; i++) {
        float x = fmodf(i * 57.0f + f * (0.12f + i * 0.03f), 300) - 30, y = SY(14 + (i % 3) * 13.0f, 0.45f);
        mk_ellipse(x, y, 13, 3.5f); mk_ellipse(x - 6, y - 1.5f, 5, 3.5f); mk_ellipse(x + 3, y - 3, 7, 4.5f);
        static const uint8_t CL[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_CANDY, 7) };
        pillow(CL, 0, 2.0f, -0.8f);       /* lit from below by the sunset */
    }
}
static void far_towns(int f) {
    float g = SY(80, 0.6f);
    layer_begin();
    /* soft far hills */
    for (int x = 0; x < TW; x++) {
        int h = (int)(g - 7 - 4 * sinf(x * 0.035f + 1) - 2 * sinf(x * 0.11f));
        for (int y = h; y < (int)g + 2; y++) tk_px(x, y, x < 120 ? PAL_RAMP(RAMP_GRASS, 3) : PAL_RAMP(RAMP_SICK, 1));
    }
    /* Hillvile: plush cottages with candy roofs and glowing heart windows */
    static const int RP[5] = { PAINT_ROSE, PAINT_SKY, PAINT_MINT, PAINT_LEMON, PAINT_LAVENDER };
    for (int i = 0; i < 6; i++) {
        int x = 4 + i * 14 + (i & 1) * 2, w = 11, h = 8 + (i % 3) * 2, base = (int)g - (i & 1);
        tk_rect(x, base - h, w, h, PAL_RAMP(RAMP_PEACH, 7));
        tk_rect(x + w - 3, base - h, 3, h, PAL_RAMP(RAMP_PEACH, 5));
        tk_tri(x - 2, base - h + 1, x + w + 1, base - h + 1, x + w / 2, base - h - 7, PAL_PAINT(RP[i % 5], 0));
        tk_tri(x - 1, base - h, x + w, base - h, x + w / 2, base - h - 6, PAL_PAINT(RP[i % 5], 1));
        tk_line(x + w / 2, base - h - 6, x, base - h, PAL_PAINT(RP[i % 5], 2));
        int lit = ((f / 11 + i) % 7) != 0;
        uint8_t wc = lit ? PAL_RAMP(RAMP_CANDY, 6) : PAL_RAMP(RAMP_PEACH, 4);
        int wx = x + 3, wy = base - h + 3;
        tk_px(wx, wy, wc); tk_px(wx + 2, wy, wc); tk_rect(wx, wy + 1, 3, 1, wc); tk_px(wx + 1, wy + 2, wc);
        tk_rect(x + w - 4, base - 4, 2, 4, PAL_RAMP(RAMP_WOOD, 3));
        if (i % 2 == 0) {     /* heart-shaped chimney puffs */
            int ph = (f + i * 7) % 30;
            tk_heart(x + 2 + ph / 6, base - h - 8 - ph / 2, 1, ph < 20 ? PAL_PAINT(PAINT_BUBBLEGUM, 2) : PAL_RAMP(RAMP_PASTEL_PINK, 6));
        }
    }
    /* Hellvile: gothic spires, red windows, a little smoke */
    for (int i = 0; i < 6; i++) {
        int x = 158 + i * 13 + (i & 1) * 2, w = 8 + (i % 2) * 2, h = 12 + (i * 7) % 10, base = (int)g;
        tk_rect(x, base - h, w, h, PAL_RAMP(RAMP_BLOOD, 1));
        tk_rect(x, base - h, 2, h, PAL_RAMP(RAMP_BLOOD, 2));
        tk_tri(x - 2, base - h, x + w + 1, base - h, x + w / 2, base - h - 10 - (i & 1) * 3, PAL_RAMP(RAMP_STONE, 1));
        tk_tri(x - 1, base - h, x + w / 2, base - h, x + w / 2, base - h - 9 - (i & 1) * 3, PAL_RAMP(RAMP_STONE, 2));
        for (int k = 0; k < 2; k++) {
            int on = ((f / 9 + i + k) % 5) != 0;
            tk_rect(x + 2 + k * 3, base - h + 4 + k * 4, 1, 2, on ? PAL_RAMP(RAMP_LAVA, 5) : PAL_RAMP(RAMP_BLOOD, 0));
        }
        if (i % 3 == 1)
            for (int k = 0; k < 5; k++) {
                int ph = (f + k * 8 + i * 5) % 40;
                tint_ellipse(x + w / 2 + ph * 0.3f, base - h - 12 - ph * 0.6f, 2 + ph * 0.06f, 1.6f + ph * 0.05f, cmap(90, 60, 80, 8), 0.5f, 14 - ph / 3);
            }
    }
    /* aerial haze: everything far away melts into the sunset */
    layer_end(cmap(250, 170, 190, 6));
}
static void lawn(int f) {
    float g = SY(80, 0.6f);
    static const uint8_t LG[4] = { PAL_RAMP(RAMP_GRASS, 3), PAL_RAMP(RAMP_GRASS, 4), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6) };
    for (int y = (int)g; y < TH; y++) {
        float d = (y - g) / 80.0f;
        for (int x = 0; x < TW; x++) {
            /* warm sunset light near the horizon, cooler toward us */
            float L = 3.2f - d * 2.6f + 0.6f * expf(-((x - 120) * (x - 120)) / 3000.0f) * (1 - d);
            uint8_t c = qtone(LG, 4, L, x, y);
            tk_fb[y][x] = c;
        }
    }
    /* a winding path */
    for (int y = (int)g; y < TH; y++) {
        float d = (y - g + 2) / 80.0f;
        float cx = 120 + sinf(d * 3.0f) * 14, hw = 3 + d * 30;
        for (int x = (int)(cx - hw); x <= (int)(cx + hw); x++)
            tk_px(x, y, fabsf(x - cx) > hw - 1.5f ? PAL_RAMP(RAMP_PEACH, 5) : PAL_RAMP(RAMP_PEACH, 6));
    }
    /* lamp posts with warm glow */
    for (int i = 0; i < 4; i++) {
        static const float LX[4] = { 22, 78, 166, 222 };
        float base = SY(108 + (i & 1) * 4.0f, 1.0f), x = LX[i];
        tint_ellipse(x, base - 30, 14, 14, cmap(255, 220, 150, 7), 0.6f, 14);
        tk_rect((int)x - 1, (int)base - 28, 2, 28, PAL_RAMP(RAMP_STONE, 2));
        tk_rect((int)x - 2, (int)base - 2, 4, 2, PAL_RAMP(RAMP_STONE, 2));
        mk_ellipse(x, base - 31, 3, 3.5f);
        static const uint8_t LP[4] = { PAL_RAMP(RAMP_CANDY, 5), PAL_RAMP(RAMP_CANDY, 6), PAL_RAMP(RAMP_CANDY, 7), PAL_WHITE };
        pillow(LP, PAL_RAMP(RAMP_STONE, 2), 1.5f, 0.2f);
        tk_rect((int)x - 3, (int)base - 35, 7, 1, PAL_RAMP(RAMP_STONE, 2));
    }
    /* fairy lights strung between the lamps */
    for (int seg = 0; seg < 2; seg++) {
        float xa = seg ? 166 : 22, xb = seg ? 222 : 78, ya = SY(73 + (seg ? 4 : 0), 1.0f), yb = SY(77 - (seg ? 4 : 0), 1.0f);
        for (int k = 0; k <= 12; k++) {
            float t = k / 12.0f, x = xa + (xb - xa) * t, y = ya + (yb - ya) * t + 6 * sinf(t * 3.14159f);
            static const int FC[4] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_MINT, PAINT_SKY };
            int on = ((f / 6 + k) % 4) != 0;
            tk_px((int)x, (int)y, on ? PAL_PAINT(FC[k % 4], 2) : PAL_PAINT(FC[k % 4], 0));
            if (k < 12) tk_px((int)(x + (xb - xa) / 24), (int)(y + 1), PAL_RAMP(RAMP_STONE, 3));
        }
    }
}

/* ---- the Wall: cracks, then blocks tumble into a rubble mound --------------- */
#define NBX 5
#define NBY 14
static float rubble_h(float x) {      /* height of the settled mound at x */
    float d = (x - 120) / 46.0f;
    return d * d < 1 ? 14 * (1 - d * d) : 0;
}
static void wall(int f) {
    float g = SY(WBOT, 1.0f);
    float top = SY(WTOP, 1.0f);
    float bh = (WBOT - WTOP) / (float)NBY, bw = (WX1 - WX0) / (float)NBX;
    int jit = f >= F_RUMBLE && f < F_CRUMBLE ? ((f & 1) ? 1 : -1) * (f > F_RUMBLE + 10 ? 2 : 1) : 0;
    static const uint8_t ST[4] = { PAL_RAMP(RAMP_STONE, 1), PAL_RAMP(RAMP_STONE, 2), PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 4) };
    static const uint8_t SR[4] = { PAL_RAMP(RAMP_BLOOD, 0), PAL_RAMP(RAMP_BLOOD, 1), PAL_RAMP(RAMP_STONE, 2), PAL_RAMP(RAMP_STONE, 3) };
    /* a contact shadow under it (and under the rubble later) */
    tint_ellipse(120, g + 1, 52, 5, cmap(40, 20, 50, 8), 0.5f, 16);
    for (int j = 0; j < NBY; j++)
        for (int i = 0; i < NBX + 1; i++) {
            float x0 = WX0 + i * bw - (j & 1) * bw * 0.5f, x1 = x0 + bw;
            if (x0 < WX0) x0 = WX0;
            if (x1 > WX1) x1 = WX1;
            if (x1 - x0 < 3) continue;
            float y0 = top + j * bh;
            float cx = (x0 + x1) * 0.5f, cy = y0 + bh * 0.5f;
            int fallen = 0;
            if (f >= F_CRUMBLE) {
                uint32_t h = tk_hash((uint32_t)(j * 17 + i * 5 + 3));
                float delay = (NBY - 1 - j) * 0.12f + (h % 4) * 0.4f;    /* the bottom gives way first */
                float t = f - F_CRUMBLE - delay;
                if (t > 0) {
                    float vx = ((int)(h % 200) - 100) / 100.0f * 0.9f + (cx - 120) * 0.035f;
                    float xx = cx + vx * fminf(t, 16);
                    float land = g - rubble_h(xx) - 1 - (h % 4);
                    float yy = cy + 0.5f * 1.4f * t * t;
                    if (yy > land) {        /* settle with one small bounce */
                        float tl = sqrtf(fmaxf(land - cy, 0) / 0.7f), over = t - tl;
                        yy = land - (over < 5 ? 3 * sinf(over / 5 * 3.14159f) : 0);
                    }
                    cx = xx; cy = yy; fallen = 1;
                }
            }
            if (cy + bh * 0.5f < -2) continue;
            mk_rrect(cx - (x1 - x0) * 0.5f + jit + 0.5f, cy - bh * 0.5f + 0.5f, x1 - x0 - 1, bh - 1, 2.0f);
            pillow(cx > 126 && !fallen ? SR : ST, PAL_RAMP(RAMP_STONE, 0), 2.0f, 0.3f);
            /* the Hillvile side has chalk hearts doodled on it */
            if (!fallen && (j * 7 + i * 3) % 9 == 2 && cx < 116)
                tk_heart((int)cx + jit, (int)cy, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
        }
    if (f < F_CRUMBLE) {
        /* rim light from the sun behind it */
        tk_rect(WX0 + jit - 1, 0, 1, (int)g, PAL_RAMP(RAMP_PEACH, 6));
        tk_rect(WX1 + jit, 0, 1, (int)g, PAL_RAMP(RAMP_CANDY, 6));
        /* the old sign */
        int sx = 99 + jit, sy = (int)SY(44, 1.0f);
        mk_rrect((float)sx, (float)sy, 42, 24, 3);
        mk_ring(1, 1, 2, 0, cmap(0, 0, 0, 6)); mk_reset();
        mk_rrect((float)sx, (float)sy, 42, 24, 3);
        static const uint8_t SG[4] = { PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6), PAL_RAMP(RAMP_PEACH, 7) };
        pillow(SG, PAL_RAMP(RAMP_WOOD, 1), 2.0f, 0.3f);
        tk_text(sx + 9, sy + 3, "KEEP", PAL_UI_RED_D, 1);
        tk_text(sx + 12, sy + 13, "OUT", PAL_UI_RED_D, 1);
        /* the taped "pls <3" note */
        tk_rect(sx + 31, sy + 14, 9, 8, PAL_UI_CREAM);
        tk_rect(sx + 39, sy + 15, 1, 7, PAL_RAMP(RAMP_PEACH, 6));
        tk_heart(sx + 35, sy + 18, 1, PAL_PAINT(PAINT_ROSE, 1));
        tk_rect(sx + 30, sy + 13, 3, 1, PAL_GRAY_L); tk_rect(sx + 38, sy + 13, 3, 1, PAL_GRAY_L);
        /* growing cracks */
        if (f >= F_RUMBLE) {
            int a = f - F_RUMBLE;
            for (int k = 0; k < 4; k++) {
                uint32_t s2 = tk_hash((uint32_t)(k * 991 + 7));
                float x = 94 + k * 16, y = SY(WBOT - 4 - (k & 1) * 30, 1.0f);
                int len = a * 3 - k * 5;
                for (int st = 0; st < len && st < 60; st++) {
                    float nx = x + tk_rand(&s2, 5) - 2, ny = y - 1.8f;
                    tk_line((int)x + jit, (int)y, (int)nx + jit, (int)ny, PAL_RAMP(RAMP_STONE, 0));
                    if ((st & 3) == 0) tk_px((int)nx + jit + 1, (int)ny, PAL_RAMP(RAMP_CANDY, 6));    /* sunlight leaks through */
                    x = nx; y = ny;
                }
            }
            for (int k = 0; k < 8; k++) {            /* trickling dust */
                int ph = (a * 2 + k * 7) % 40;
                tk_px(90 + k * 8 + jit, (int)(top + 14 + ph * 2.6f), PAL_RAMP(RAMP_STONE, 5));
            }
        }
    }
}
static void dust(int f) {
    if (f < F_CRUMBLE + 3 || f > F_CRUMBLE + 38) return;
    int a = f - F_CRUMBLE - 3;
    float g = SY(WBOT, 1.0f);
    float fade = a > 18 ? (a - 18) / 17.0f : 0;
    for (int i = 0; i < 18; i++) {
        uint32_t h = tk_hash((uint32_t)i * 77u + 5u);
        float dir = (i - 8.5f) / 8.5f;
        float up = (h % 7) / 6.0f;                       /* some puffs billow upward */
        float x = 120 + dir * (14 + a * 1.7f * (0.7f + (h % 5) * 0.1f));
        float y = g - 4 - up * a * 1.1f - a * 0.2f - (h % 9);
        float r = (5 + a * 0.36f * (0.8f + up * 0.6f) + (h % 4)) * (1 - fade * fade);
        y -= fade * 10;
        if (fade >= 1 || r < 1) continue;
        for (int yy = (int)(y - r); yy <= (int)(y + r); yy++)
            for (int xx = (int)(x - r); xx <= (int)(x + r); xx++) {
                float dx = (xx + 0.5f - x) / r, dy = (yy + 0.5f - y) / r;
                float d = dx * dx + dy * dy;
                if (d > 1) continue;
                mk_px(xx, yy);
            }
    }
    static const uint8_t DU[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_CANDY, 7) };
    pillow(DU, 0, 5.0f, 0.6f);
}

/* ---- fireworks ---------------------------------------------------------------- */
static void firework(float x, float y, int age, uint32_t seed, int heart, int pal) {
    if (age < 0 || age > 34) return;
    static const uint8_t PALS[3][3] = {
        { PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_ROSE, 1) },
        { PAL_RAMP(RAMP_LAVA, 7), PAL_RAMP(RAMP_LAVA, 5), PAL_UI_RED },
        { PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_SKY, 2), PAL_PAINT(PAINT_LAVENDER, 2) } };
    const uint8_t *C = PALS[pal % 3];
    if (age < 8) {      /* the rocket going up */
        float yy = y + (8 - age) * 7;
        tk_px((int)x, (int)yy, PAL_WHITE);
        tk_px((int)x, (int)yy + 1, C[0]); tk_px((int)x, (int)yy + 3, C[1]);
        return;
    }
    int a = age - 8;
    if (a < 3) tint_ellipse(x, y, 10 + a * 4, 10 + a * 4, cmap(255, 255, 230, 9 - a * 3), 0.6f, 16);
    uint32_t s = tk_hash(seed);
    int n = heart ? 28 : 24;
    float R = 28 * (1 - expf(-a * 0.2f));
    for (int i = 0; i < n; i++) {
        float px, py;
        if (heart) {
            float t = i / (float)n * 6.28318f;
            float hx = 16 * powf(sinf(t), 3), hy = -(13 * cosf(t) - 5 * cosf(2 * t) - 2 * cosf(3 * t) - cosf(4 * t));
            px = x + hx * R / 17.0f; py = y + hy * R / 17.0f;
        } else {
            float an = i / (float)n * 6.28318f + tk_rand(&s, 100) * 0.003f;
            px = x + cosf(an) * R; py = y + sinf(an) * R;
        }
        py += a * a * 0.02f;
        if (a > 18 && ((i + a) & 1)) continue;
        if (a > 22 && (i % 3)) continue;
        uint8_t c = a < 5 ? PAL_WHITE : C[(i + a / 4) % 3];
        if (a < 12) tk_rect((int)px, (int)py, 2, 2, c);
        else tk_px((int)px, (int)py, c);
        if (a >= 12 && a < 20 && (i % 4) == (a & 3)) tk_star((int)px, (int)py, 1, PAL_WHITE);
        if (a < 14) {    /* little trails */
            float tx = x + (px - x) * 0.85f, ty = y + (py - y) * 0.85f;
            tk_px((int)tx, (int)ty, C[2]);
        }
    }
}

/* ---- the punchline pile: shell casings + Vapula's crown ---------------------- */
static void casing(float x, float y, int kind, int flat) {
    int ix = (int)x, iy = (int)y;
    uint8_t ol = PAL_RAMP(RAMP_WOOD, 1);
    if (kind == 0 && !flat) {           /* brass casing standing */
        tk_rect(ix - 1, iy - 1, 5, 9, ol);
        tk_rect(ix, iy, 3, 7, PAL_RAMP(RAMP_CANDY, 4));
        tk_rect(ix, iy, 1, 7, PAL_RAMP(RAMP_CANDY, 6));
        tk_rect(ix + 2, iy, 1, 7, PAL_RAMP(RAMP_CANDY, 2));
        tk_rect(ix, iy, 3, 1, PAL_RAMP(RAMP_WOOD, 2));
    } else if (kind == 0) {             /* lying on its side */
        tk_rect(ix - 1, iy - 1, 9, 5, ol);
        tk_rect(ix, iy, 7, 3, PAL_RAMP(RAMP_CANDY, 4));
        tk_rect(ix, iy, 7, 1, PAL_RAMP(RAMP_CANDY, 6));
        tk_rect(ix, iy + 2, 7, 1, PAL_RAMP(RAMP_CANDY, 2));
        tk_rect(ix + 6, iy, 1, 3, PAL_RAMP(RAMP_CANDY, 3));
    } else {                            /* a shotgun shell */
        tk_rect(ix - 1, iy - 1, 10, 6, PAL_RAMP(RAMP_BLOOD, 0));
        tk_rect(ix, iy, 6, 4, PAL_UI_RED);
        tk_rect(ix, iy, 6, 1, PAL_RAMP(RAMP_LAVA, 5));
        tk_rect(ix + 6, iy, 2, 4, PAL_RAMP(RAMP_CANDY, 5));
        tk_rect(ix + 6, iy, 2, 1, PAL_RAMP(RAMP_CANDY, 7));
    }
}
static void pile(int f) {
    float base = SY(240, 2.6f);
    if (base - 64 > TH) return;
    /* the mound body */
    for (int x = 0; x < TW; x++) {
        float u = (x - 96) / 110.0f;
        if (fabsf(u) >= 1) continue;
        int top = (int)(base - (1 - u * u) * 40);
        for (int y = top; y < TH; y++) tk_px(x, y, ((x * 3 + y * 5) % 7) ? PAL_RAMP(RAMP_CANDY, 2) : PAL_RAMP(RAMP_WOOD, 2));
    }
    /* hundreds of casings, back to front */
    for (int i = 0; i < 360; i++) {
        uint32_t h = tk_hash((uint32_t)i * 2654435761u + 11u);
        float v = (i / 360.0f);
        float u = ((h % 1000) / 1000.0f) * 2 - 1;
        float hgt = (1 - u * u) * 40;
        float x = 96 + u * 106, y = base - hgt * (1 - v) + v * 6 - 4;
        casing(x, y, (h >> 22) % 8 == 0, (h >> 25) % 3 != 0);
    }
    /* still smoking hot */
    for (int k = 0; k < 4; k++) {
        static const float SXP[4] = { 40, 74, 128, 160 };
        int ph = (f + k * 10) % 40;
        float x = SXP[k] + 3 * sinf(ph * 0.25f + k), y = base - (1 - ((SXP[k] - 96) / 106) * ((SXP[k] - 96) / 106)) * 40 - 2 - ph * 0.9f;
        tint_ellipse(x, y, 2 + ph * 0.08f, 1.6f + ph * 0.06f, cmap(220, 210, 230, 8), 0.5f, 15 - ph / 3);
    }
    /* specular glints running over the brass */
    for (int k = 0; k < 7; k++) {
        int ph = (f + k * 11) % 36;
        if (ph < 8) tk_star(28 + k * 24 + (k & 1) * 7, (int)(base - 8 - (k % 3) * 9), ph < 4 ? ph / 2 + 1 : (8 - ph) / 2, PAL_WHITE);
    }
    /* the crown, dented and crooked, on top */
    float cx = 96, cy = base - 46;
    float tilt = 0.3f;
    for (int y = -18; y <= 8; y++)
        for (int x = -18; x <= 18; x++) {
            float rx = x * cosf(tilt) + y * sinf(tilt), ry = -x * sinf(tilt) + y * cosf(tilt);
            int in = ry >= -3 && ry <= 6 && fabsf(rx) <= 14;
            for (int p = -2; p <= 2; p++) {
                float px = p * 6.5f, hg = p == 0 ? 16.0f : 12.0f;
                if (p == 1) hg = 7.0f;                    /* the bent spike */
                if (ry < -3 && ry > -hg && fabsf(rx - px) < (ry + hg) * 0.3f) in = 1;
            }
            if (in) mk_px((int)cx + x, (int)cy + y);
        }
    mk_ring(1, 1, 2, 0, cmap(40, 20, 0, 8)); mk_reset();
    for (int y = -18; y <= 8; y++)
        for (int x = -18; x <= 18; x++) {
            float rx = x * cosf(tilt) + y * sinf(tilt), ry = -x * sinf(tilt) + y * cosf(tilt);
            int in = ry >= -3 && ry <= 6 && fabsf(rx) <= 14;
            for (int p = -2; p <= 2; p++) {
                float px = p * 6.5f, hg = p == 0 ? 16.0f : 12.0f;
                if (p == 1) hg = 7.0f;
                if (ry < -3 && ry > -hg && fabsf(rx - px) < (ry + hg) * 0.3f) in = 1;
            }
            if (in) mk_px((int)cx + x, (int)cy + y);
        }
    static const uint8_t GD[4] = { PAL_RAMP(RAMP_CANDY, 3), PAL_RAMP(RAMP_CANDY, 5), PAL_RAMP(RAMP_CANDY, 6), PAL_WHITE };
    pillow(GD, PAL_RAMP(RAMP_WOOD, 1), 2.5f, 0.4f);
    tk_circle((int)cx - 1, (int)cy + 1, 2, PAL_UI_RED);
    tk_px((int)cx - 2, (int)cy, PAL_WHITE);
    for (int k = -1; k <= 1; k += 2) tk_px((int)cx + k * 7, (int)cy + 2, PAL_PAINT(PAINT_VAPE_PURPLE, 1));
    tk_line((int)cx + 6, (int)cy - 2, (int)cx + 9, (int)cy + 2, PAL_RAMP(RAMP_CANDY, 1));       /* the dent */
    /* one last, sad little wisp of vape leaving the crown */
    for (int k = 0; k < 4; k++) {
        int ph = (f + k * 9) % 36;
        tint_ellipse(cx + 6 + ph * 0.4f + 2 * sinf(ph * 0.3f), cy - 18 - ph * 0.8f, 1.5f + ph * 0.07f, 1.2f + ph * 0.05f,
                     cmap(190, 120, 240, 9), 0.5f, 15 - ph / 3);
    }
}

/* ---- the citizens ---------------------------------------------------------------- */
typedef struct { const TkChibi *c; float x0, x1, y; int delay; } Actor;
static TkChibi extra[16];
static int extra_ok;
#define NACT 20
static Actor A[NACT];

static void draw_actor(int i, int f) {
    Actor *a = &A[i];
    float x = a->x0, y = SY(a->y, 1.0f);
    int expr = TKE_NORMAL, mouth = TKM_SMILE, flip = a->x0 > 120;
    float hop = 0;
    if (f >= F_SEE && f < F_RUN) { expr = TKE_SHOCK; mouth = TKM_O; }
    if (f >= F_RUN) {
        float t = tk_clampf((f - F_RUN - a->delay) / 13.0f, 0, 1);
        x = a->x0 + (a->x1 - a->x0) * tk_ease(t);
        if (t > 0 && t < 1) { hop = fabsf(sinf((f - F_RUN) * 0.8f + i)) * 3; expr = TKE_HAPPY; mouth = TKM_OPEN; }
        if (t >= 1) { expr = TKE_LOVE; mouth = TKM_SMILE; flip = !flip; }
        if (f >= F_DANCE) { hop = fabsf(sinf((f - F_DANCE) * 0.35f + i)) * 3; expr = (f / 12 + i) % 3 ? TKE_HAPPY : TKE_LOVE; mouth = TKM_OPEN; }
        y -= rubble_h(x) * 0.6f;
    }
    if (f < F_RUMBLE) { expr = TKE_SAD; mouth = TKM_FROWN; }   /* staring at the wall */
    tk_chibi((int)x, (int)(y - hop), a->c, 1, expr, mouth, f + i * 11, flip);
    if (a->c == &KEVIN) kevin_eye((int)x, (int)(y - hop), f + i * 11);
    if (f >= F_SEE && f < F_RUN + 4) {
        int ea = f - F_SEE - (i % 3);
        if (ea >= 0) tk_text_outline((int)x - 2, (int)(y - 40 - (ea < 3 ? 3 - ea : 0)), "!", PAL_WHITE, PAL_UI_RED_D, 1);
    }
}

void scene_s13_ending(int f, int n) {
    (void)n;
    if (!extra_ok) {
        extra_ok = 1;
        for (int i = 0; i < 16; i++) extra[i] = tk_random_citizen((uint32_t)(300 + i), i & 1);
        int k = 0;
        /* back row (far): random folk who swap sides - comingling */
        for (int i = 0; i < 4; i++) A[k++] = (Actor){ &extra[i * 2], 10.0f + i * 18, 128.0f + i * 22, GY - 12, i };
        for (int i = 0; i < 4; i++) A[k++] = (Actor){ &extra[i * 2 + 1], 230.0f - i * 18, 112.0f - i * 22, GY - 12, i + 1 };
        /* mid row: the cast */
        A[k++] = (Actor){ &TK_NANA, 20, 98, GY, 4 };
        A[k++] = (Actor){ &TK_MAYOR, 44, 120, GY - 4, 1 };
        A[k++] = (Actor){ &TK_FUZZ, 66, 84, GY + 2, 6 };
        A[k++] = (Actor){ &TK_PIP, 2, 60, GY + 4, 7 };
        A[k++] = (Actor){ &extra[8], 78, 146, GY - 6, 2 };
        A[k++] = (Actor){ &TK_IMP, 220, 106, GY + 2, 2 };
        A[k++] = (Actor){ &TK_RIVET, 172, 92, GY - 2, 0 };
        A[k++] = (Actor){ &KEVIN, 250, 210, GY + 6, 4 };      /* callback: Kevin (three eyes) */
        A[k++] = (Actor){ &extra[11], 238, 158, GY, 6 };
        A[k++] = (Actor){ &extra[13], 160, 176, GY - 6, 3 };
        A[k++] = (Actor){ &extra[10], 30, 66, GY - 8, 5 };
        A[k++] = (Actor){ &extra[15], 212, 186, GY - 8, 5 };
        /* sort back to front once */
        for (int i = 0; i < NACT; i++)
            for (int j = i + 1; j < NACT; j++)
                if (A[j].y < A[i].y) { Actor t = A[i]; A[i] = A[j]; A[j] = t; }
    }
    /* camera: settle from the sky, hold, then tilt down to the pile */
    camy = -14 * (1 - tk_ease(f / 30.0f));
    if (f >= F_TILT) camy = 30 * tk_ease((f - F_TILT) / 16.0f);
    float shake = 0;
    if (f >= F_CRUMBLE && f < F_CRUMBLE + 12) shake = (f & 1) ? 3 : -3;

    sky(f);
    if (f >= F_FIRE) {
        static const struct { int t; float x, y; int heart, pal; } FW[] = {
            { 0, 34, 48, 1, 0 }, { 5, 204, 44, 0, 1 }, { 11, 120, 30, 1, 2 }, { 16, 30, 26, 0, 2 },
            { 22, 210, 24, 1, 0 }, { 28, 64, 40, 0, 1 }, { 34, 178, 50, 1, 1 }, { 40, 40, 30, 1, 2 },
            { 46, 196, 34, 0, 0 }, { 52, 100, 26, 1, 0 }, { 58, 150, 30, 0, 2 }, { 64, 26, 46, 1, 1 } };
        for (int i = 0; i < (int)(sizeof FW / sizeof FW[0]); i++)
            firework(FW[i].x, SY(FW[i].y, 0.4f), f - F_FIRE - FW[i].t, (uint32_t)(i * 31 + 7), FW[i].heart, FW[i].pal);
    }
    clouds(f);
    far_towns(f);
    lawn(f);
    /* the sun floods in when the wall falls */
    if (f >= F_CRUMBLE + 4 && f < F_CRUMBLE + 20) {
        int a = f - F_CRUMBLE - 4;
        tint_ellipse(120, SY(76, 0.5f), 60 + a * 6, 40 + a * 4, cmap(255, 240, 200, 10 - a / 2), 0.7f, 16);
    }

    /* back rows, then the wall + dust, then the mid row */
    int wall_drawn = 0;
    for (int i = 0; i < NACT; i++) {
        if (!wall_drawn && A[i].y > GY - 10) { wall(f); dust(f); wall_drawn = 1; }
        draw_actor(i, f);
    }
    if (!wall_drawn) { wall(f); dust(f); }
    /* hugs: hearts float up over the meeting points */
    if (f >= F_HUG) {
        static const float HX[6] = { 98, 124, 146, 70, 168, 188 };
        for (int k = 0; k < 6; k++) {
            int ha = f - F_HUG - k * 3;
            if (ha < 0) continue;
            int ph = ha % 30;
            plush_heart(HX[k] + 3 * sinf(ph * 0.3f + k), SY(GY - 30, 1.0f) - ph * 1.2f, 2.6f, 5, k & 1 ? PAINT_CHERRY : PAINT_BUBBLEGUM, PAL_PAINT(PAINT_ROSE, 0));
        }
    }

    /* the front: our fox hero + a goth cat from Hellvile, and the party bunny */
    {
        float y = SY(GY + 28, 1.15f);
        float xl = 30, xr = 210;
        int expr = TKE_NORMAL, mouth = TKM_SMILE;
        if (f >= F_SEE && f < F_RUN) { expr = TKE_SHOCK; mouth = TKM_O; }
        float t = tk_clampf((f - F_RUN - 2) / 11.0f, 0, 1);
        float k = tk_ease(t);
        float xh = xl + (106 - xl) * k, xg = xr + (136 - xr) * k, hop = 0;
        if (t > 0 && t < 1) { hop = fabsf(sinf((f - F_RUN) * 0.8f)) * 4; expr = TKE_HAPPY; mouth = TKM_OPEN; }
        if (t >= 1) { expr = TKE_LOVE; mouth = TKM_SMILE; }
        if (f >= F_DANCE) hop = fabsf(sinf((f - F_DANCE) * 0.3f)) * 4;
        if (f < F_RUMBLE) { expr = TKE_SAD; mouth = TKM_FROWN; }
        tint_ellipse(xg, y, 18, 3, cmap(40, 30, 40, 6), 0.5f, 16);
        tint_ellipse(xh, y, 18, 3, cmap(40, 30, 40, 6), 0.5f, 16);
        tk_chibi((int)xg, (int)(y - hop), &TK_GOTH, 2, expr, mouth, f + 5, 1);
        tk_chibi((int)xh, (int)(y - hop), &TK_HERO, 2, expr, mouth, f + 9, 0);
        if (t >= 1) {
            int ha = f - F_RUN - 13;
            float hs = ha < 4 ? 0.5f + ha * 0.18f : 1.0f + 0.06f * sinf(ha * 0.4f);
            plush_heart(121, y - 64 - hop, 7 * hs, 12 * hs, PAINT_BUBBLEGUM, PAL_PAINT(PAINT_ROSE, 0));
            if (ha < 12) tk_sparkles(121, (int)(y - 72), ha * 2, 77, 22);
        }
        /* the party bunny (cured!) bounces in with a heart balloon */
        if (f >= F_RUN) {
            float bxp = 236 - 52 * tk_ease(tk_clampf((f - F_RUN - 6) / 12.0f, 0, 1));
            float byp = SY(GY + 16, 1.1f) - fabsf(sinf(f * 0.3f)) * 5;
            float sw = sinf(f * 0.1f) * 3;
            tk_line((int)bxp + 9, (int)byp - 18, (int)(bxp + 14 + sw), (int)byp - 46, PAL_WHITE);
            plush_heart(bxp + 14 + sw, byp - 44, 5, 9, PAINT_CHERRY, PAL_PAINT(PAINT_BLOOD, 0));
            tk_chibi((int)bxp, (int)byp, &BUN, 1, TKE_HAPPY, TKM_OPEN, f + 3, 1);
        }
    }

    /* callback: the starer from s06 is at the party now. He did not walk in.
     * He is simply there. No breathing, no blinking, no dancing. */
    if (f >= F_RUN + 16) {
        float sy = SY(GY + 10, 1.05f);
        tint_ellipse(16, sy, 9, 2, cmap(40, 30, 40, 6), 0.5f, 16);
        tk_chibi(16, (int)sy, &STARER, 1, TKE_NORMAL, TKM_FLAT, 0, 0);
    }

    /* the pile in the foreground (only revealed by the tilt) */
    pile(f);

    /* captions */
    uint8_t oc = PAL_RAMP(RAMP_VAPE, 0);
    if (f >= F_CAP1 && f < F_CAP2) bcap(12, "...and maybe, just maybe...", PAL_WHITE, oc, 1, (f - F_CAP1) * 2);
    if (f >= F_CAP2) bcap(20, "everyone can\nget along.", PAL_WHITE, oc, 2, f - F_CAP2);
    if (f >= F_CAP3) {
        int a = f - F_CAP3;
        int dy = a == 0 ? -3 : a == 1 ? 1 : 0;
        tk_text_outline(152, 124 + dy, "(after a LOT", PAL_WHITE, oc, 1);
        tk_text_outline(152, 134 + dy, "of shooting.)", PAL_WHITE, oc, 1);
    }
    /* a single casing rolls off the pile: tink. */
    if (f >= F_TINK) {
        int a = f - F_TINK;
        float base = SY(240, 2.6f);
        float x = 150 + a * 1.8f, y = base - 22 + fminf(a * a * 0.35f, 22) - (a > 13 && a < 18 ? 2 : 0);
        casing(x, y, 0, 1);
    }
    if (shake != 0) tk_post_shake((int)shake, f & 2 ? 2 : -1);
    if (f < 3) tk_post_fade(10 - f * 4);
}

void scene_s13_ending_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    /* a warm, hopeful pad under the first caption */
    tk_note(t0, 1.6, 60, TKV_TRI, 0.18f);
    tk_note(t0, 1.6, 64, TKV_TRI, 0.14f);
    tk_note(t0, 1.6, 67, TKV_SINE, 0.14f);
    /* rumble + cracks */
    tk_slide(t0 + F_RUMBLE * fr, (F_CRUMBLE - F_RUMBLE) * fr, 30, 34, TKV_NOISE, 0.25f);
    for (int k = 0; k < 4; k++) tk_sfx(t0 + (F_RUMBLE + 4 + k * 4) * fr, SFX_BLIP, 0.3f);
    /* down it comes */
    tk_sfx(t0 + F_CRUMBLE * fr, SFX_EXPLOSION, 0.75f);
    for (int k = 0; k < 8; k++) tk_sfx(t0 + (F_CRUMBLE + 4 + k * 2) * fr, SFX_THUD, 0.35f);
    tk_sfx(t0 + (F_CRUMBLE + 4) * fr, SFX_WHOOSH, 0.4f);
    /* !  */
    tk_sfx(t0 + F_SEE * fr, SFX_BEEP, 0.3f);
    /* running feet + hugs */
    for (int k = 0; k < 6; k++) tk_note(t0 + (F_RUN + k * 2) * fr, 0.03, 70 + (k & 1) * 5, TKV_SQ25, 0.14f);
    tk_sfx(t0 + F_HUG * fr, SFX_SQUEAK, 0.5f);
    tk_sfx(t0 + (F_HUG + 3) * fr, SFX_SQUEAK, 0.45f);
    tk_sfx(t0 + (F_HUG + 1) * fr, SFX_CHOIR, 0.5f);
    tk_sfx(t0 + (F_HUG + 2) * fr, SFX_SPARKLE, 0.4f);
    /* fireworks: launch whistle + pop + crackle */
    static const int FT[] = { 0, 6, 13, 20, 26, 33, 40, 46, 52, 58, 64 };
    for (int i = 0; i < 11; i++) {
        double t = t0 + (F_FIRE + FT[i]) * fr;
        tk_slide(t, 8 * fr, 84, 96, TKV_SINE, 0.07f);
        tk_sfx(t + 8 * fr, SFX_POP, 0.45f);
        tk_note(t + 8 * fr, 0.2, 60, TKV_NOISE, 0.18f);
        for (int k = 0; k < 4; k++) tk_note(t + (12 + k * 3) * fr, 0.02, 110, TKV_NOISE, 0.08f);
    }
    /* the pile reveal: silence, then one casing goes tink. */
    tk_note(t0 + (F_TINK + 6) * fr, 0.08, 100, TKV_SINE, 0.3f);
    tk_note(t0 + (F_TINK + 10) * fr, 0.06, 103, TKV_SINE, 0.2f);
    tk_note(t0 + (F_TINK + 13) * fr, 0.05, 101, TKV_SINE, 0.12f);
}
