/* s01_splash.c - "a HaHaland production".
 *
 * A plump, glossy pastel heart drops onto a dreamy bokeh backdrop, lands
 * with a squashy BOING, bounces, and the puffy "HaHaland" letters pop in
 * one by one. "(100% demon-free!)"
 * ...then, for exactly three frames, a little devil tail flicks out from
 * behind the heart. The heart freezes (it stops breathing), side-eyes the
 * audience, sweats. The tagline quietly grows an asterisk: "*mostly".
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
#define F_LAND 14
#define F_TAIL 44       /* 3 frames: 44, 45, 46 */
#define F_FREEZE 47
#define F_SIDE 50

static int heart_in(float u, float v) {
    float a = u * u + v * v - 1.0f;
    return a * a * a - u * u * v * v * v <= 0.0f;
}
/* heart mask: bottom tip at (cx, tip), half width w, total height h */
static void mk_heart(float cx, float tip, float w, float h) {
    for (int y = (int)(tip - h - 2); y <= (int)tip + 1; y++)
        for (int x = (int)(cx - w - 2); x <= (int)(cx + w + 2); x++) {
            float u = (x + 0.5f - cx) / w * 1.139f;
            float v = -1.0f + (tip - (y + 0.5f)) / h * 2.25f;
            if (heart_in(u, v)) mk_px(x, y);
        }
}

/* soft dreamy backdrop: radial gradient, bokeh orbs, drifting tiny hearts */
static void backdrop(int f) {
    static const uint8_t R[] = { PAL_WHITE, PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6),
                                 PAL_RAMP(RAMP_PASTEL_PINK, 5) };
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float dx = (x - 120) / 150.0f, dy = (y - 66) / 105.0f;
            float d = sqrtf(dx * dx + dy * dy) * 3.4f - 0.3f;
            tk_fb[y][x] = qtone(R, 4, d, x, y);
        }
    /* bokeh: big soft light orbs of peach / lavender / mint, drifting up */
    const uint8_t *bk[3] = { cmap(255, 214, 190, 6), cmap(214, 196, 255, 6), cmap(190, 245, 225, 6) };
    for (int i = 0; i < 11; i++) {
        uint32_t s = tk_hash((uint32_t)i * 977u + 3u);
        float r = 7 + (s % 9);
        float x = (float)((s >> 4) % 260) - 10 + 6 * sinf(f * 0.03f + i);
        float y = fmodf((float)((s >> 12) % 200) - f * (0.25f + (s % 5) * 0.08f) + 400.0f, 200.0f) - 20;
        tint_ellipse(x, y, r, r, bk[i % 3], 0.35f, 16);
        tint_ellipse(x - r * 0.3f, y - r * 0.3f, r * 0.35f, r * 0.35f, cmap(255, 255, 255, 5), 0.5f, 12);
    }
    /* far tiny hearts (hazy) and near ones (crisp), floating up */
    for (int i = 0; i < 14; i++) {
        uint32_t s = tk_hash((uint32_t)i * 131u + 77u);
        int near = i < 5;
        float sp = near ? 0.9f : 0.45f;
        int x = (int)((s % 250) - 5 + 4 * sinf(f * 0.08f + i * 1.7f));
        int y = (int)(fmodf((float)((s >> 9) % 190) - f * sp + 380.0f, 190.0f)) - 15;
        static const int hp[] = { PAINT_BUBBLEGUM, PAINT_LAVENDER, PAINT_PEACH, PAINT_MINT, PAINT_SKY };
        uint8_t c = PAL_PAINT(hp[i % 5], near ? 1 : 2);
        if (near) {
            mk_heart((float)x, (float)y + 3, 3.4f, 6.0f);
            uint8_t r4[4] = { PAL_PAINT(hp[i % 5], 0), c, PAL_PAINT(hp[i % 5], 2), PAL_WHITE };
            pillow(r4, PAL_PAINT(hp[i % 5], 0), 2.0f, 0.2f);
        } else {
            tk_px(x - 1, y, c); tk_px(x + 1, y, c);
            tk_rect(x - 1, y + 1, 3, 1, c); tk_px(x, y + 2, c);
        }
    }
    /* twinkling motes */
    for (int i = 0; i < 18; i++) {
        uint32_t s = tk_hash((uint32_t)i * 59u + 1234u);
        int x = (int)(s % TW), y = (int)((s >> 8) % TH);
        int ph = (f + (int)(s >> 20) % 40) % 40;
        if (ph < 10) tk_star(x, y, ph < 5 ? ph / 2 : (10 - ph) / 2, ph & 2 ? PAL_WHITE : PAL_UI_GOLD);
    }
}

/* heart vertical motion: tip offset above rest (px) and squash (+ = wide) */
static void heart_motion(int f, float *off, float *sq) {
    *off = 0; *sq = 0;
    if (f < F_LAND) { float t = (f + 1) / (float)F_LAND; *off = 120 * (1 - t * t); *sq = -0.28f * t; return; }
    if (f < F_LAND + 3) { static const float s[3] = { 0.34f, 0.24f, 0.08f }; *sq = s[f - F_LAND]; return; }
    if (f < 27) { float t = (f - 17) / 10.0f; *off = 18 * 4 * t * (1 - t); *sq = t < .5f ? -0.14f * (1 - 2 * t) : -0.06f; return; }
    if (f < 29) { *sq = f == 27 ? 0.2f : 0.08f; return; }
    if (f < 35) { float t = (f - 29) / 6.0f; *off = 7 * 4 * t * (1 - t); return; }
    if (f < 37) { *sq = 0.06f; return; }
    if (f >= F_FREEZE) { *sq = 0.025f * sinf(F_FREEZE * 0.22f); return; }   /* frozen. not breathing. */
    *sq = 0.025f * sinf(f * 0.22f);
}

enum { FACE_WOW, FACE_SQUISH, FACE_HAPPY, FACE_OPEN, FACE_SHOCK, FACE_SIDE };

static void heart_face(float cx, float cy, float w, float h, int face, int f) {
    float s = w / 30.0f;
    float ey = cy - 1 * s, exl = cx - 10.5f * s, exr = cx + 10.5f * s;
    uint8_t k = PAL_RAMP(RAMP_VAPE, 0), mo = PAL_RAMP(RAMP_PASTEL_PINK, 0);
    /* blush */
    const uint8_t *bl = cmap(255, 90, 140, 7);
    tint_ellipse(exl - 5 * s, ey + 7 * s, 5 * s, 2.6f * s, bl, 0.3f, 14);
    tint_ellipse(exr + 5 * s, ey + 7 * s, 5 * s, 2.6f * s, bl, 0.3f, 14);
    int blink = face == FACE_OPEN && (f % 47) >= 44;
    for (int e = 0; e < 2; e++) {
        float ex = e ? exr : exl;
        int side = e ? 1 : -1;
        switch (face) {
        case FACE_HAPPY: /* ^ ^ */
            for (int i = -3; i <= 3; i++) {
                int yy = (int)(ey + 1 - (3 - abs(i)) * 0.8f);
                tk_rect((int)ex + i, yy, 1, 2, k);
            }
            break;
        case FACE_SQUISH: /* > < */
            for (int i = 0; i < 4; i++) {
                tk_rect((int)ex - 2 * side + i * side - 1, (int)ey - 3 + i, 2, 1, k);
                tk_rect((int)ex - 2 * side + i * side - 1, (int)ey + 3 - i, 2, 1, k);
            }
            break;
        case FACE_SHOCK:
            tk_ellipse((int)ex, (int)ey, (int)(4 * s + 0.5f), (int)(5 * s + 0.5f), k);
            tk_ellipse((int)ex, (int)ey, (int)(3 * s + 0.5f), (int)(4 * s + 0.5f), PAL_WHITE);
            tk_rect((int)ex - 1, (int)ey - 1, 2, 2, k);
            break;
        case FACE_SIDE:
            /* heavy-lidded side-eye straight at the audience */
            tk_ellipse((int)ex, (int)ey, (int)(4 * s + 0.5f), (int)(4 * s + 0.5f), k);
            tk_ellipse((int)ex, (int)ey, (int)(3 * s + 0.5f), (int)(3 * s + 0.5f), PAL_WHITE);
            tk_rect((int)ex + 1, (int)ey - 1, 3, 3, k);
            tk_px((int)ex + 1, (int)ey - 1, PAL_WHITE);
            tk_rect((int)(ex - 4 * s), (int)(ey - 4 * s), (int)(8 * s) + 2, (int)(3 * s) + 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
            tk_rect((int)(ex - 4 * s), (int)(ey - 4 * s) + (int)(3 * s) + 1, (int)(8 * s) + 2, 1, k);
            break;
        default:
            if (blink) { tk_rect((int)ex - 3, (int)ey + 1, 7, 1, k); break; }
            gloss_eye(ex, ey, (face == FACE_WOW ? 3.9f : 3.5f) * s, (face == FACE_WOW ? 5.4f : 4.8f) * s,
                      k, PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 4));
            break;
        }
    }
    /* mouth */
    int mx = (int)cx, my = (int)(ey + 6 * s);
    if (face == FACE_WOW || face == FACE_SHOCK) {
        tk_ellipse(mx, my + 2, 2, face == FACE_SHOCK ? 1 : 3, mo);
        tk_rect(mx - 1, my + 3, 2, 1, PAL_PAINT(PAINT_CHERRY, 1));
    } else if (face == FACE_SIDE) {
        tk_rect(mx - 2, my + 2, 5, 1, mo);           /* "nothing happened." */
        tk_px(mx + 3, my + 1, mo);
    } else if (face == FACE_SQUISH) {
        tk_rect(mx - 3, my + 1, 7, 1, mo);
        tk_rect(mx - 2, my + 2, 5, 2, mo);
    } else {
        tk_rect(mx - 3, my + 1, 7, 1, mo);
        tk_rect(mx - 3, my + 2, 7, 2, mo);
        tk_rect(mx - 2, my + 4, 5, 1, mo);
        tk_rect(mx - 1, my + 3, 3, 2, PAL_PAINT(PAINT_CHERRY, 2));
    }
}

/* the forbidden tail: base hidden behind the heart's right lobe */
static void devil_tail(float cx, float cy, int stage) {
    static const float P[3][5][2] = {
        { { 6, 4 }, { 18, 4 }, { 26, -2 }, { 29, -10 }, { 27, -16 } },
        { { 6, 4 }, { 22, 8 }, { 36, 2 }, { 44, -9 }, { 42, -21 } },
        { { 6, 4 }, { 17, 6 }, { 24, 3 }, { 27, -3 }, { 26, -8 } },
    };
    const float (*p)[2] = P[stage];
    float tx = 0, ty = 0;
    for (int i = 0; i < 4; i++)
        for (int k = 0; k <= 10; k++) {
            float t = k / 10.0f;
            float x = cx + p[i][0] + (p[i + 1][0] - p[i][0]) * t;
            float y = cy + p[i][1] + (p[i + 1][1] - p[i][1]) * t;
            float r = 2.6f - (i * 10 + k) * 0.03f;
            mk_ellipse(x, y, r, r);
            tx = x; ty = y;
        }
    /* spade tip */
    float dx = p[4][0] - p[3][0], dy = p[4][1] - p[3][1], l = sqrtf(dx * dx + dy * dy);
    dx /= l; dy /= l;
    for (int j = 0; j < 9; j++) {
        float w = j < 5 ? 1.2f + j * 1.0f : 5.2f - (j - 5) * 1.6f;
        float x = tx + dx * (j - 2), y = ty + dy * (j - 2);
        mk_ellipse(x, y, w * 0.75f, w * 0.75f);
    }
    static const uint8_t R4[4] = { PAL_PAINT(PAINT_BLOOD, 1), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_CHERRY, 2),
                                   PAL_PAINT(PAINT_ROSE, 2) };
    pillow(R4, PAL_PAINT(PAINT_BLOOD, 0), 2.0f, 0.1f);
}

static Tile T_LOGO[8];
static int tiles_ok;

void scene_s01_splash(int f, int n) {
    if (!tiles_ok) {
        tiles_ok = 1;
        for (int i = 0; i < 8; i++) make_tile(&T_LOGO[i], (unsigned char)"HaHaland"[i], 3, 2);
    }
    backdrop(f);

    const float cx = 120, rest = 78;
    float off, sq;
    heart_motion(f, &off, &sq);
    float w = 34 * (1 + sq), h = 58 * (1 - sq * 0.9f);
    float tip = rest - off;
    float drift = f >= 37 && f < F_FREEZE ? 1.2f * sinf(f * 0.11f) : 0;   /* gentle hover */
    tip -= drift;

    /* contact shadow: grows as the heart nears the floor */
    float sh = tk_clampf(1.0f - off / 120.0f, 0.15f, 1.0f);
    tint_ellipse(cx, rest + 3, (17 + sq * 30) * sh + 3, 3.2f * sh + 1, cmap(150, 60, 120, 6), 0.45f, 16);

    /* landing impact: soft light ring + puffs */
    if (f >= F_LAND && f < F_LAND + 12) {
        int a = f - F_LAND;
        float r = 18 + a * 5.5f;
        const uint8_t *lm = cmap(255, 255, 255, 9 - a * 3 / 4);
        for (int k = 0; k < 2; k++)
            tint_ellipse(cx, rest + 2, r - k * 4, (r - k * 4) * 0.22f, lm, 0.12f, 16 - a);
        for (int s = -1; s <= 1; s += 2) {
            float px = cx + s * (26 + a * 3.0f), py = rest - a * 0.6f;
            float pr = 6 - a * 0.45f;
            if (pr > 1) {
                mk_ellipse(px, py, pr, pr * 0.8f);
                static const uint8_t PF[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PASTEL_PINK, 6),
                                               PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_WHITE };
                pillow(PF, PAL_RAMP(RAMP_PASTEL_PINK, 4), 2.0f, 0.3f);
            }
        }
    }
    if (f >= F_LAND && f < F_LAND + 40) tk_confetti((int)cx, (int)rest - 6, f - F_LAND, 5);

    /* THE TAIL (3 frames, drawn behind the heart) */
    if (f >= F_TAIL && f < F_TAIL + 3) devil_tail(cx + 10, tip - h * 0.55f, f - F_TAIL);

    /* the heart */
    mk_heart(cx, tip, w, h);
    static const uint8_t HR[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 3), PAL_PAINT(PAINT_BUBBLEGUM, 1),
                                   PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_RAMP(RAMP_PASTEL_PINK, 7) };
    pillow(HR, PAL_RAMP(RAMP_PASTEL_PINK, 1), 9.0f, 0.42f);
    /* glossy highlights: a big soft one on the left lobe, a small one right */
    float lx = cx - w * 0.5f, ly = tip - h * 0.78f;
    tint_ellipse(lx, ly, w * 0.2f, h * 0.09f, cmap(255, 255, 255, 12), 0.4f, 16);
    tk_rect((int)(lx - w * 0.08f), (int)(ly - 1), (int)(w * 0.14f), 2, PAL_WHITE);
    tk_rect((int)(cx + w * 0.55f), (int)(tip - h * 0.75f), 2, 2, PAL_WHITE);

    int face = FACE_OPEN;
    if (f < F_LAND) face = FACE_WOW;
    else if (f < F_LAND + 3 || f == 27 || f == 28) face = FACE_SQUISH;
    else if (f < 37) face = FACE_HAPPY;
    if (f >= F_TAIL && f < F_SIDE) face = FACE_SHOCK;
    if (f >= F_SIDE) face = FACE_SIDE;
    heart_face(cx, tip - h * 0.5f, w, h, face, f);

    /* sweat drop sliding down */
    if (f >= F_SIDE + 3) {
        int a = f - F_SIDE - 3;
        int sx = (int)(cx - w * 0.82f), sy = (int)(tip - h * 0.8f) + (a < 10 ? a / 2 : 5);
        mk_ellipse((float)sx, (float)sy + 3, 2.2f, 2.8f);
        mk_ellipse((float)sx, (float)sy + 1, 1.0f, 1.6f);
        static const uint8_t SW[4] = { PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 2), PAL_RAMP(RAMP_SKY, 7), PAL_WHITE };
        pillow(SW, PAL_PAINT(PAINT_COBALT, 1), 1.5f, 0.2f);
        tk_px(sx - 1, sy + 2, PAL_WHITE);
    }

    /* sparkles around the heart once it has landed */
    if (f >= F_LAND + 1 && f < F_FREEZE) tk_sparkles((int)cx, (int)(tip - h * 0.5f), f - 15, 77, 46);

    /* "HaHaland": puffy candy letters pop in one by one */
    static const int LP[8] = { PAINT_BUBBLEGUM, PAINT_MINT, PAINT_LEMON, PAINT_SKY,
                               PAINT_LAVENDER, PAINT_PEACH, PAINT_BUBBLEGUM, PAINT_MINT };
    int adv = 19, x0 = 120 - (7 * adv) / 2;
    const uint8_t *shadow = cmap(120, 40, 110, 7);
    layer_begin();
    for (int pass = 0; pass < 3; pass++)
        for (int i = 0; i < 8; i++) {
            int a = f - (17 + i * 2);
            if (a < 0) continue;
            float sx = 1, sy = 1, dy = 0;
            if (a < 3) { static const float D[3] = { -18, -8, -2 }; dy = D[a]; sx = 0.8f; sy = 1.25f; }
            else if (a < 8) {
                static const float SX[5] = { 1.3f, 1.12f, 0.94f, 1.03f, 1.0f };
                static const float SY[5] = { 0.72f, 0.9f, 1.08f, 0.98f, 1.0f };
                static const float DY[5] = { 0, 0, -3, -1, 0 };
                sx = SX[a - 3]; sy = SY[a - 3]; dy = DY[a - 3];
            }
            if (f >= 40 && f < F_FREEZE) dy += 1.3f * sinf(f * 0.2f + i * 0.8f);
            float lcx = x0 + i * adv, lby = 127 + dy;
            tile_mk(&T_LOGO[i], lcx, lby, sx, sy);
            if (pass == 0) { mk_ring(3, 2, 3, 0, shadow); mk_reset(); }
            else if (pass == 1) { mk_ring(3, 0, 0, PAL_WHITE, NULL); mk_reset(); }
            else {
                int p = LP[i];
                uint8_t r4[4] = { PAL_PAINT(p, 0), PAL_PAINT(p, 1), PAL_PAINT(p, 2), PAL_WHITE };
                if (p == PAINT_LEMON) { r4[0] = PAL_PAINT(PAINT_HONEY, 1); r4[1] = PAL_PAINT(PAINT_LEMON, 1); }
                if (p == PAINT_PEACH) r4[0] = PAL_RAMP(RAMP_PEACH, 4);
                pillow(r4, PAL_RAMP(RAMP_VAPE, 1), 2.4f, 0.55f);
            }
        }
    if (f >= 36 && f < 46) {                       /* glossy shine sweep */
        layer_band(cmap(255, 255, 255, 10), 150 + (f - 36) * 18, 6);
        layer_band(cmap(255, 255, 255, 7), 150 + (f - 36) * 18 + 9, 2);
    }
    if (f >= 30) {
        uint8_t c = PAL_RAMP(RAMP_PASTEL_PINK, 2);
        int a = f - 30, dy = a < 2 ? -3 + a : 0;
        tk_text_outline(x0 - 9, 91 + dy, "a", c, PAL_WHITE, 1);
        tk_text_outline(x0 + 7 * adv + 9 - tk_text_w("production", 1), 130 + dy, "production", c, PAL_WHITE, 1);
    }
    /* the tagline ... and its footnote */
    if (f >= 34) {
        const char *tg = f >= F_SIDE + 2 ? "(100% demon-free!*)" : "(100% demon-free!)";
        int tw = tk_text_w("(100% demon-free!)", 1);
        int a = f - 34, dy = a < 3 ? (a == 0 ? -3 : a == 1 ? 1 : 0) : 0;
        tk_text_outline(120 - tw / 2, 141 + dy, tg, PAL_RAMP(RAMP_VAPE, 2), PAL_WHITE, 1);
        if (f >= F_SIDE + 2 && f < F_SIDE + 4) tk_star(120 + tw / 2 + 3, 142, 3, PAL_UI_GOLD);
    }
    if (f >= F_SIDE + 5) {
        int a = f - F_SIDE - 5;
        tk_text_outline(TW - 52, 149 + (a < 2 ? 2 - a : 0), "*mostly", PAL_GRAY_D, PAL_WHITE, 1);
    }

    /* fade in from white, out to white */
    if (f < 5) tk_post_flash(PAL_WHITE, 16 - f * 3);
    if (f > n - 5) tk_post_flash(PAL_WHITE, (f - (n - 5)) * 3 + 2);
}

void scene_s01_splash_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    tk_slide(t0 + 1 * fr, 13 * fr, 88, 64, TKV_SINE, 0.32f);            /* falling whistle */
    tk_sfx(t0 + F_LAND * fr, SFX_BOING, 1.0f);
    tk_sfx(t0 + F_LAND * fr, SFX_THUD, 0.35f);
    tk_sfx(t0 + 27 * fr, SFX_POP, 0.45f);
    for (int i = 0; i < 8; i++) tk_note(t0 + (20 + i * 2) * fr, 0.05, 76 + i * 2, TKV_SQ25, 0.22f);
    tk_sfx(t0 + 36 * fr, SFX_SPARKLE, 0.55f);                           /* shine */
    /* the tail: a tiny whip "fwip" ... then dead silence while it freezes */
    tk_slide(t0 + F_TAIL * fr, 0.07, 70, 98, TKV_SQ12, 0.28f);
    tk_sfx(t0 + F_TAIL * fr, SFX_WHOOSH, 0.25f);
    tk_note(t0 + F_SIDE * fr, 0.04, 91, TKV_SQ12, 0.2f);                  /* eyes: tik */
    tk_note(t0 + (F_SIDE + 4) * fr, 0.04, 86, TKV_SQ12, 0.2f);            /* ...tik */
    tk_note(t0 + (F_SIDE + 2) * fr, 0.03, 100, TKV_SINE, 0.15f);          /* asterisk plink */
}
