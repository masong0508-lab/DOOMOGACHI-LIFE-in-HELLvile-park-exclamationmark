/* s04_doomogachi.c - "Raise your very own DOOMOGACHI!"
 *
 * A glossy plastic egg-shaped virtual pet on a keychain (the charm is a
 * plush heart with a suspicious little tail), floating over a candy-stripe
 * bokeh backdrop. The LCD egg wobbles and hatches into a blob ("beep."),
 * real sparkles leap out of the screen. Our fox hero's paw reaches in and
 * presses A: the blob eats a cupcake in three bites, and plush hearts float
 * out of the LCD. It strains... plop: a tiny pixel poop with stink lines.
 * "It loves you!" Then for four frames the whole screen turns into a red
 * LCD pentagram / demon face. Back to cute. "beep." ...but now it has tiny
 * horns and the poop is a smouldering ash pile. "...it's fine."
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
#define F_HATCH 36
#define F_PRESS 56
#define F_FEED 58
#define F_LOVE 78
#define F_STRAIN 92
#define F_POOP 100
#define F_EVIL 128      /* four frames of hell: 128..131 */
#define F_BEEP2 134

/* ---- virtual LCD ------------------------------------------------------------ */
#define LW 24
#define LH 16
#define LPX 3
static uint8_t lcd[LH][LW];      /* 0 off, 1 on, 2 red, 3 hellfire */
static int lcd_x, lcd_y;

static void lcd_clear(void) { memset(lcd, 0, sizeof lcd); }
static void lcd_set(int x, int y, uint8_t v) { if ((unsigned)x < LW && (unsigned)y < LH) lcd[y][x] = v; }
static void lcd_spr(const char *const *rows, int x, int y, int flip) {
    int w = (int)strlen(rows[0]);
    for (int j = 0; rows[j]; j++)
        for (int i = 0; rows[j][i]; i++) {
            char c = rows[j][i];
            int xx = flip ? x + w - 1 - i : x + i;
            if (c == '#') lcd_set(xx, y + j, 1);
            else if (c == 'r') lcd_set(xx, y + j, 2);
            else if (c == 'h') lcd_set(xx, y + j, 3);
            else if (c == 'o') lcd_set(xx, y + j, 0); /* explicit erase */
        }
}
static void lcd_render_at(int ox, int oy, int ps, int evil) {
    uint8_t bg = evil ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_UI_LCD;
    uint8_t sh = evil ? PAL_BLACK : PAL_RAMP(RAMP_SICK, 5);
    uint8_t on = evil ? PAL_UI_RED : PAL_RAMP(RAMP_SICK, 1);
    uint8_t gap = evil ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_SICK, 3);
    uint8_t red = PAL_UI_RED, redg = PAL_RAMP(RAMP_LAVA, 6);
    int so = ps / 3 > 0 ? ps / 3 : 1;
    tk_rect(ox, oy, LW * ps, LH * ps, bg);
    for (int y = 0; y < LH; y++)          /* faint off-pixel ghosts */
        for (int x = 0; x < LW; x++)
            if (((x + y) & 1) == 0 || evil)
                tk_rect(ox + x * ps + ps / 3, oy + y * ps + ps / 3, so, so, evil ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_GRASS, 6));
    for (int y = 0; y < LH; y++)          /* drop shadows */
        for (int x = 0; x < LW; x++)
            if (lcd[y][x]) tk_rect(ox + x * ps + so, oy + y * ps + so, ps, ps, sh);
    for (int y = 0; y < LH; y++)
        for (int x = 0; x < LW; x++) {
            if (!lcd[y][x]) continue;
            int px = ox + x * ps, py = oy + y * ps;
            if (lcd[y][x] == 2) {
                tk_rect(px, py, ps, ps, gap); tk_rect(px, py, ps - 1, ps - 1, red);
                tk_rect(px, py, ps / 3 + 1, ps / 3 + 1, redg);
                continue;
            }
            if (lcd[y][x] == 3) {
                tk_rect(px, py, ps, ps, PAL_RAMP(RAMP_LAVA, 5));
                tk_rect(px + 1, py + 1, ps - 2, ps - 2, PAL_RAMP(RAMP_LAVA, 7));
                tk_rect(px + ps / 3, py + ps / 3, ps / 3, ps / 3, PAL_WHITE);
                continue;
            }
            tk_rect(px, py, ps, ps, gap); tk_rect(px, py, ps - 1, ps - 1, on);
        }
}

/* ---- LCD sprites ---------------------------------------------------------- */
static const char *EGG[] = { "...###...", "..#...#..", ".#.....#.", ".#..#..#.", "#..###..#", "#...#...#",
                             "#.......#", "#.##....#", "#.##..#.#", ".#.....#.", "..#####..", NULL };
static const char *CRACK1[] = { "..#.#....", ".#.#.#...", NULL };
static const char *CRACK2[] = { "..#.#.#..", "##.#.#.##", NULL };
static const char *SHELL_TOP[] = { "...###...", "..#...#..", ".#.....#.", "#.#.#.#.#", NULL };
static const char *BLOB[] = { "....####....", "..##....##..", ".#........#.", "#..##..##..#", "#..##..##..#",
                              "#..........#", "#....##....#", ".#........#.", "..########..", "...#....#...", NULL };
static const char *BLOB_SQ[] = { "....######....", "..##......##..", ".#..##..##..#.", "#...##..##...#",
                                 "#.....##.....#", ".#..........#.", "..##########..", "...##....##...", NULL };
static const char *BLOB_HAPPY[] = { "....####....", "..##....##..", ".#........#.", "#..#....#..#", "#.#.#..#.#.#",
                                    "#..........#", "#...####...#", ".#...##...#.", "..########..", "...#....#...", NULL };
static const char *BLOB_CHOMP[] = { "....####....", "..##....##..", ".#........#.", "#..##..##..#", "#..##..##..#",
                                    "#...####...#", "#...#..#...#", ".#..####..#.", "..########..", "...#....#...", NULL };
static const char *BLOB_STRAIN[] = { "....####....", "..##....##..", ".#........#.", "#.#......#.#", "#..#....#..#",
                                     "#.#......#.#", "#...####...#", ".#........#.", "..########..", "...#....#...", NULL };
static const char *BLOB_DEMON[] = { "#..........#", "##..####..##", ".###....###.", ".#........#.", "#..hh..hh..#",
                                    "#..hh..hh..#", "#..........#", "#.#.####.#.#", ".#.#....#.#.", "..########..", NULL };
static const char *PENTAGRAM[] = {
    "..........rrrr..........", "........rrr..rrr........", ".......rr......rr.......", "......r.rr....rr.r......",
    ".....r..r.rrrr.r..r.....", ".....r...r.rr.r...r.....", "....rr...rr..rr...rr....", "....r...rr....rr...r....",
    "....r..rrrr..rrrr..r....", "....rrr...r..r...rrr....", ".....rrrrrrrrrrrrrr.....", ".....r....rrrr....r.....",
    "......r....rr....r......", ".......r...rr...r.......", "........rrrrrrrr........", "..........rrrr..........", NULL };
static const char *HORNLETS[] = { ".#........#.", "..#......#..", NULL };
static const char *CUPCAKE[] = { "...#...", "..###..", ".#####.", "#######", "#.#.#.#", ".#.#.#.", ".#####.", NULL };
static const char *BITE1[] = { ".....oo", "......o", NULL };
static const char *BITE2[] = { "...oooo", "..ooooo", "....ooo", "......o", NULL };
static const char *BITE3[] = { "ooooooo", "ooooooo", "ooooooo", "ooooooo", "oooo.oo", "oooo.oo", "oooo.oo", NULL };
static const char *HEART[] = { ".#.#.", "#####", ".###.", "..#..", NULL };
static const char *POOP[] = { "...#...", "..##...", ".####..", ".#.#.#.", "#######", "#######", NULL };
static const char *ASH[] = { ".......", ".......", ".......", "..#.#..", ".#.#.#.", "#######", NULL };

/* ---- backdrop ----------------------------------------------------------------- */
static void backdrop(int f) {
    static const uint8_t R[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_VAPE, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_VAPE, 6) };
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float dx = (x - 120) / 140.0f, dy = (y - 90) / 100.0f;
            tk_fb[y][x] = qtone(R, 4, sqrtf(dx * dx + dy * dy) * 3.0f - 0.2f, x, y);
        }
    /* soft diagonal candy stripes drifting */
    const uint8_t *st = cmap(255, 255, 255, 3);
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            int u = (x + y + f / 2) % 28;
            if (u < 10) tk_fb[y][x] = st[tk_fb[y][x]];
        }
    /* bokeh orbs */
    const uint8_t *bk[3] = { cmap(255, 214, 230, 7), cmap(214, 196, 255, 7), cmap(200, 250, 230, 6) };
    for (int i = 0; i < 10; i++) {
        uint32_t s = tk_hash((uint32_t)i * 733u + 11u);
        float r = 6 + (s % 10);
        float x = (float)((s >> 4) % 260) - 10 + 5 * sinf(f * 0.03f + i);
        float y = fmodf((float)((s >> 12) % 200) - f * (0.3f + (s % 4) * 0.1f) + 400.0f, 200.0f) - 20;
        tint_ellipse(x, y, r, r, bk[i % 3], 0.4f, 16);
    }
    /* floating tiny hearts and stars */
    for (int i = 0; i < 12; i++) {
        uint32_t s = tk_hash((uint32_t)i * 191u + 7u);
        int x = (int)((s % 250) - 5 + 4 * sinf(f * 0.07f + i));
        int y = (int)(fmodf((float)((s >> 9) % 190) - f * 0.5f + 380.0f, 190.0f)) - 15;
        if (i & 1) {
            static const int hp[] = { PAINT_BUBBLEGUM, PAINT_LAVENDER, PAINT_SKY };
            uint8_t c = PAL_PAINT(hp[(i / 2) % 3], 1);
            tk_px(x - 1, y, c); tk_px(x + 1, y, c); tk_rect(x - 1, y + 1, 3, 1, c); tk_px(x, y + 2, c);
        } else {
            int ph = (f + i * 5) % 24;
            tk_star(x, y, ph < 12 ? 1 : 2, ph & 4 ? PAL_WHITE : PAL_UI_GOLD);
        }
    }
}

/* ---- the device --------------------------------------------------------------- */
static void mk_egg(float cx, float cy, float rx, float ryt, float ryb) {
    for (int y = (int)(cy - ryt - 1); y <= (int)(cy + ryb + 1); y++)
        for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++) {
            float fx = (x + 0.5f - cx) / rx, fy = y + 0.5f - cy;
            fy = fy < 0 ? fy / ryt : fy / ryb;
            /* slightly narrower at the top */
            float w = fy < 0 ? 1.0f + fy * 0.12f : 1.0f;
            fx /= w;
            if (fx * fx + fy * fy <= 1.0f) mk_px(x, y);
        }
}
static void mk_rrect(float x, float y, float w, float h, float r) {
    for (int j = (int)y; j < (int)(y + h); j++)
        for (int i = (int)x; i < (int)(x + w); i++) {
            float dx = fmaxf(fmaxf(x + r - (i + 0.5f), (i + 0.5f) - (x + w - r)), 0);
            float dy = fmaxf(fmaxf(y + r - (j + 0.5f), (j + 0.5f) - (y + h - r)), 0);
            if (dx * dx + dy * dy <= r * r) mk_px(i, j);
        }
}
static void icon(int ix, int iy, int i, int sel) {
    static const char *IC[8][5] = {
        { "#.#.#", "#.#.#", ".###.", "..#..", "..#.." }, { ".###.", "#.#.#", "#####", "#.#.#", ".###." },
        { ".###.", "#...#", ".#.#.", ".###.", "..#.." }, { ".#.#.", "#####", "#####", ".###.", "..#.." },
        { "....#", "..##.", ".##..", "##...", "#...." }, { ".###.", ".#.#.", "#####", ".###.", ".#.#." },
        { "#####", "..#..", ".###.", "#...#", "#####" }, { "..#..", "..#..", "..#..", ".....", "..#.." } };
    uint8_t c = PAL_RAMP(RAMP_PASTEL_PINK, 3);
    if (sel) { tk_rect(ix - 2, iy - 1, 9, 7, PAL_PAINT(PAINT_ROSE, 1)); c = PAL_WHITE; }
    for (int j = 0; j < 5; j++)
        for (int k = 0; k < 5; k++)
            if (IC[i][j][k] == '#') tk_px(ix + k, iy + j, c);
}
static void device(float cx, float cy, int f, int pressed, int icon_sel, float swing) {
    /* keychain: a chain up out of frame + a tiny charm */
    float kx = cx + 2, ky = cy - 64;
    for (int i = 0; i < 9; i++) {
        float t = i / 8.0f;
        float lx = kx + sinf(swing) * 30 * t * t, ly = ky - 6 - t * 40;
        tk_ellipse((int)lx, (int)ly, 1, 2, PAL_RAMP(RAMP_STEEL, (i & 1) ? 6 : 4));
        tk_px((int)lx, (int)ly - 1, PAL_WHITE);
    }
    tk_ring((int)kx, (int)ky - 1, 5, PAL_RAMP(RAMP_STEEL, 3));
    tk_ring((int)kx, (int)ky - 2, 5, PAL_RAMP(RAMP_STEEL, 6));
    /* the charm: a plush heart hanging off the ring... with a tiny devil tail */
    float chx = kx - 12 - sinf(swing) * 4, chy = ky + 6 + cosf(swing) * 2;
    tk_line((int)kx - 4, (int)ky, (int)chx, (int)chy - 6, PAL_RAMP(RAMP_STEEL, 5));
    for (int y = (int)(chy - 9); y <= (int)chy + 1; y++)
        for (int x = (int)(chx - 6); x <= (int)(chx + 6); x++) {
            float u = (x + 0.5f - chx) / 5.0f * 1.139f, v = -1.0f + (chy - (y + 0.5f)) / 9.0f * 2.25f;
            float a = u * u + v * v - 1.0f;
            if (a * a * a - u * u * v * v * v <= 0.0f) mk_px(x, y);
        }
    static const uint8_t CHR[4] = { PAL_PAINT(PAINT_CHERRY, 0), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_CHERRY, 2), PAL_WHITE };
    pillow(CHR, PAL_PAINT(PAINT_BLOOD, 0), 2.0f, 0.3f);
    tk_px((int)chx + 5, (int)chy - 3, PAL_PAINT(PAINT_CHERRY, 0)); tk_px((int)chx + 6, (int)chy - 4, PAL_PAINT(PAINT_CHERRY, 0));
    tk_px((int)chx + 7, (int)chy - 5, PAL_PAINT(PAINT_CHERRY, 1));

    /* soft contact shadow below the device */
    tint_ellipse(cx + 3, cy + 62, 48, 6, cmap(110, 60, 140, 6), 0.5f, 16);
    /* shell: glossy plastic egg */
    mk_egg(cx, cy, 58, 64, 58);
    static const uint8_t SH[4] = { PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_RAMP(RAMP_PASTEL_PINK, 7) };
    pillow(SH, PAL_RAMP(RAMP_PASTEL_PINK, 1), 11.0f, 0.5f);
    /* specular streaks */
    tint_ellipse(cx - 50, cy - 8, 4, 14, cmap(255, 255, 255, 10), 0.5f, 16);
    tk_rect((int)cx - 52, (int)cy - 16, 2, 9, PAL_WHITE);
    tint_ellipse(cx - 22, cy - 57, 12, 3, cmap(255, 255, 255, 9), 0.5f, 16);
    tk_rect((int)cx + 40, (int)cy + 24, 2, 4, PAL_RAMP(RAMP_PASTEL_PINK, 7));
    /* printed hearts & stars on the shell */
    static const int deco[][3] = { { -46, 16, 0 }, { 46, 18, 1 }, { -30, -46, 1 }, { 34, -44, 0 }, { -50, -8, 1 }, { 50, -10, 0 } };
    for (int i = 0; i < 6; i++) {
        int dx = (int)cx + deco[i][0], dy = (int)cy + deco[i][1];
        if (deco[i][2]) tk_star(dx, dy, ((f / 6 + i) % 4 == 0) ? 3 : 2, ((f / 10 + i) & 1) ? PAL_WHITE : PAL_PAINT(PAINT_LEMON, 2));
        else { tk_px(dx - 1, dy, PAL_PAINT(PAINT_ROSE, 2)); tk_px(dx + 1, dy, PAL_PAINT(PAINT_ROSE, 2)); tk_rect(dx - 1, dy + 1, 3, 1, PAL_PAINT(PAINT_ROSE, 2)); tk_px(dx, dy + 2, PAL_PAINT(PAINT_ROSE, 2)); }
    }
    /* faceplate */
    float bx = cx - 45, by = cy - 50;
    mk_rrect(bx, by, 90, 70, 9);
    static const uint8_t FP[4] = { PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PEACH, 7), PAL_UI_CREAM, PAL_WHITE };
    pillow(FP, PAL_RAMP(RAMP_PASTEL_PINK, 2), 3.0f, 0.4f);
    for (int i = 0; i < 8; i++)
        icon((int)bx + 12 + (i % 4) * 20, i < 4 ? (int)by + 3 : (int)by + 62, i, i == icon_sel);
    /* LCD well with bevel + inner shadow */
    lcd_x = (int)cx - LW * LPX / 2;
    lcd_y = (int)cy - 41;
    tk_rect(lcd_x - 3, lcd_y - 3, LW * LPX + 6, LH * LPX + 6, PAL_RAMP(RAMP_STONE, 5));
    tk_rect(lcd_x - 2, lcd_y - 2, LW * LPX + 4, LH * LPX + 4, PAL_RAMP(RAMP_STONE, 2));
    tk_rect(lcd_x - 3, lcd_y + LH * LPX + 2, LW * LPX + 6, 1, PAL_WHITE);
    /* brand plate, engraved */
    const char *br = "DOOMOGACHI";
    int tw = tk_text_w(br, 1);
    tk_text((int)cx - tw / 2, (int)cy + 26, br, PAL_WHITE, 1);
    tk_text((int)cx - tw / 2, (int)cy + 25, br, PAL_PAINT(PAINT_ROSE, 0), 1);
    /* three candy buttons */
    static const int bp[3] = { PAINT_LEMON, PAINT_MINT, PAINT_SKY };
    for (int i = 0; i < 3; i++) {
        float x = cx - 24 + i * 24, y = cy + 44;
        int p = pressed == i + 1;
        tint_ellipse(x + 1, y + 3, 9, 6, cmap(150, 50, 110, 7), 0.3f, 16);
        mk_ellipse(x, y + (p ? 2 : 0), 8.5f * (p ? 1.1f : 1.0f), (p ? 5.5f : 7.0f));
        uint8_t r4[4] = { PAL_PAINT(bp[i], 0), PAL_PAINT(bp[i], 1), PAL_PAINT(bp[i], 2), PAL_WHITE };
        if (bp[i] == PAINT_LEMON) { r4[0] = PAL_PAINT(PAINT_HONEY, 1); r4[1] = PAL_PAINT(PAINT_LEMON, 1); }
        pillow(r4, PAL_RAMP(RAMP_PASTEL_PINK, 1), 3.5f, 0.5f);
        tk_rect((int)x - 4, (int)y - 4 + (p ? 2 : 0), 3, 2, PAL_WHITE);
    }
}
/* LCD glass: a diagonal glare band (applied after the LCD is drawn) */
static void lcd_glare(int f) {
    const uint8_t *m = cmap(255, 255, 255, 4);
    for (int y = 0; y < LH * LPX; y++)
        for (int x = 0; x < LW * LPX; x++) {
            int u = x + y - (f % 90) * 2 + 40;
            if ((u >= 0 && u < 7) || (u >= 10 && u < 12)) tint_px(lcd_x + x, lcd_y + y, m);
        }
    tint_rect(lcd_x, lcd_y, LW * LPX, 2, cmap(40, 60, 30, 3), 16);     /* inner shadow */
    tint_rect(lcd_x, lcd_y, 2, LH * LPX, cmap(40, 60, 30, 3), 16);
}
/* the fox hero's paw: mint hoodie sleeve + fluffy orange paw with toe beans */
static void paw(float px, float py) {
    for (int k = 0; k <= 8; k++) {
        float t = k / 8.0f;
        mk_ellipse(px + 14 + t * 40, py + 14 + t * 46, 10, 10);
    }
    static const uint8_t SL[4] = { PAL_PAINT(PAINT_MINT, 0), PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_MINT, 2), PAL_WHITE };
    pillow(SL, PAL_RAMP(RAMP_MINT, 3), 4.0f, 0.4f);
    mk_ellipse(px + 3, py + 3, 9, 8);
    mk_ellipse(px + 9, py + 10, 8, 7);
    static const uint8_t PW[4] = { PAL_PAINT(PAINT_TANGERINE, 0), PAL_PAINT(PAINT_TANGERINE, 1), PAL_PAINT(PAINT_TANGERINE, 2), PAL_WHITE };
    pillow(PW, PAL_PAINT(PAINT_RUST, 0), 4.0f, 0.4f);
    for (int i = 0; i < 3; i++) tk_ellipse((int)px - 3 + i * 4, (int)py - 2 + (i == 1 ? -1 : 0), 1, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_ellipse((int)px + 1, (int)py + 3, 3, 2, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_px((int)px, (int)py + 2, PAL_PAINT(PAINT_BUBBLEGUM, 2));
}
static void bubble(int x, int y, const char *s, int age) {
    int w = tk_text_w(s, 2) + 12, h = 22;
    static const float SC[4] = { 0.4f, 1.2f, 0.92f, 1.04f };
    float sc = age < 4 ? SC[age] : 1.0f;
    float cx = x + w / 2.0f, cy = y + h / 2.0f;
    mk_rrect(cx - w * sc / 2, cy - h * sc / 2, w * sc, h * sc, 7 * sc);
    mk_tri(x + 4.0f, y + h - 3.0f, x + 13.0f, y + h - 3.0f, x - 6.0f, y + h + 8.0f);
    mk_ring(2, 1, 2, 0, cmap(90, 40, 120, 6)); mk_reset();
    mk_rrect(cx - w * sc / 2, cy - h * sc / 2, w * sc, h * sc, 7 * sc);
    mk_tri(x + 4.0f, y + h - 3.0f, x + 13.0f, y + h - 3.0f, x - 6.0f, y + h + 8.0f);
    static const uint8_t BW[4] = { PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE, PAL_WHITE, PAL_WHITE };
    pillow(BW, PAL_RAMP(RAMP_VAPE, 1), 2.0f, 0.3f);
    if (age >= 2) tk_text(x + 6, y + 4, s, PAL_RAMP(RAMP_VAPE, 1), 2);
}

void scene_s04_doomogachi(int f, int n) {
    (void)n;
    int evil = f >= F_EVIL && f < F_EVIL + 4;
    backdrop(f);

    /* device motion: drops in, floats, jiggles with events */
    float cx = 120, cy = 89 + 1.5f * sinf(f * 0.1f);
    float swing = 0.25f * sinf(f * 0.13f);
    if (f < 7) { float t = (f + 1) / 7.0f; cy -= 150 * (1 - t * t); swing = 0.6f; }
    else if (f < 14) { static const float B[7] = { 5, 3, -3, -3, -1, 1, 0 }; cy += B[f - 7]; swing = 0.5f * sinf((f - 7) * 0.9f); }
    if (f >= F_PRESS && f < F_PRESS + 4) cy += f < F_PRESS + 2 ? 2 : 1;
    if (f >= F_POOP && f < F_POOP + 3) { cy += f == F_POOP ? 3 : 1; swing += 0.3f; }
    if (f >= F_HATCH && f < F_HATCH + 3) cy -= 2;

    int pressed = (f >= F_PRESS && f < F_PRESS + 4) ? 1 : 0;
    int icon_sel = (f >= F_PRESS && f < F_LOVE) ? 0 : (f >= F_POOP && f < F_EVIL) ? 5 : (f >= F_LOVE && f < F_STRAIN) ? 3 : -1;
    device(cx, cy, f, pressed, icon_sel, swing);

    /* ---- LCD content ---- */
    lcd_clear();
    if (f < F_HATCH) {
        int wob = 0;
        if (f >= 10) { int per = f < 22 ? 6 : 3; wob = ((f / per) & 1) ? 1 : -1; }
        int ex = 8 + wob, ey = 3 + (f >= 30 && (f & 2) ? -1 : 0);
        lcd_spr(EGG, ex, ey, 0);
        if (f >= 28) lcd_spr(CRACK1, ex, ey + 5, 0);
        if (f >= 32) lcd_spr(CRACK2, ex, ey + 5, 0);
    } else if (f < F_HATCH + 4) {
        int a = f - F_HATCH;
        lcd_spr(SHELL_TOP, 8 + a, 2 - a * 2, 0);
        lcd_spr(BLOB_SQ, 5, 7, 0);
        for (int i = 0; i < 4; i++) { lcd_set(3 - a, 5 + i * 2, 1); lcd_set(20 + a, 5 + i * 2, 1); }
    } else {
        int bx = f < 52 ? 6 : f < 58 ? 6 - (f - 52) / 3 : 4, by = 5;
        int hops = f >= 40 && f < 56;
        int bounce = hops ? (((f - 40) / 4) & 1) : 0;
        const char *const *spr = bounce ? BLOB_SQ : BLOB;
        int sx = bounce ? bx - 1 : bx, sy = bounce ? by + 2 : by - (hops ? 1 : 0);
        if (f >= F_FEED && f < F_LOVE) {
            int a = f - F_FEED, cxp = 17, cyp = 7;
            lcd_spr(CUPCAKE, cxp, cyp, 0);
            if (a >= 6) lcd_spr(BITE1, cxp, cyp, 0);
            if (a >= 12) lcd_spr(BITE2, cxp, cyp, 0);
            if (a >= 18) lcd_spr(BITE3, cxp, cyp, 0);
            spr = ((a % 6) >= 3) ? BLOB_CHOMP : BLOB;
            sx = bx + 1; sy = by;
        } else if (f >= F_LOVE && f < F_STRAIN) {
            spr = BLOB_HAPPY;
            int a = f - F_LOVE;
            sy = by - ((a / 3) & 1);
            lcd_spr(HEART, 17, 6 - a / 3, 0);
            if (a > 4) lcd_spr(HEART, 19, 11 - (a - 4) / 2, 0);
        } else if (f >= F_STRAIN && f < F_POOP) {
            spr = BLOB_STRAIN;
            sx = bx + ((f & 1) ? 1 : 0);
            lcd_set(bx - 1, by + 1 + (f & 2 ? 0 : 1), 1);
        } else if (f >= F_POOP && f < F_EVIL) {
            spr = (f < F_POOP + 10) ? BLOB_HAPPY : BLOB;
            if ((f / 24) % 2 == 1 && (f % 24) > 20) spr = BLOB;
        }
        if (evil) {
            if (f < F_EVIL + 2) lcd_spr(PENTAGRAM, 0, 0, 0);
            else { lcd_spr(BLOB_DEMON, 6, 3, 0); lcd_set(5, 2, 1); lcd_set(18, 2, 1); }
        } else {
            if (f >= F_EVIL + 4) lcd_spr(HORNLETS, sx, sy - 2, 0);   /* ...those were not there before */
            lcd_spr(spr, sx, sy, 0);
        }
        if (f >= F_POOP && !evil) {
            int px = 17, py = 10;
            if (f < F_EVIL) {
                lcd_spr(POOP, px, py, 0);
                int ph = (f / 5) & 1;
                for (int k = 0; k < 2; k++) {
                    int sx0 = px + 1 + k * 4;
                    lcd_set(sx0 + ph, py - 2, 1); lcd_set(sx0 + 1 - ph, py - 3, 1);
                    lcd_set(sx0 + ph, py - 4, 1); lcd_set(sx0 + 1 - ph, py - 5, 1);
                }
            } else {
                lcd_spr(ASH, px, py, 0);
                int ph = (f / 4) % 4;
                lcd_set(px + 3 + (ph == 1), py + 1 - ph / 2, 1);
                if (ph > 1) lcd_set(px + 2 + (ph == 3), py - ph / 2, 1);
            }
        }
    }
    if (!evil) { lcd_render_at(lcd_x, lcd_y, LPX, 0); lcd_glare(f); }

    /* hatch burst: real sparkles leap out of the screen */
    if (f >= F_HATCH && f < F_HATCH + 24) {
        tk_sparkles(lcd_x + 36, lcd_y + 24, f - F_HATCH, 5, 44);
        tk_confetti(lcd_x + 36, lcd_y + 10, f - F_HATCH, 17);
    }
    /* love: real plush hearts float out of the LCD */
    if (f >= F_LOVE + 2 && f < F_LOVE + 36) {
        for (int k = 0; k < 4; k++) {
            int a = f - F_LOVE - 2 - k * 5;
            if (a < 0 || a > 24) continue;
            float hx = lcd_x + 54 + k * 6 + 6 * sinf(a * 0.3f + k), tip = lcd_y + 20 - a * 2.2f;
            float s = a < 3 ? 0.5f + a * 0.2f : 1.0f;
            for (int y = (int)(tip - 10 * s - 2); y <= (int)tip + 1; y++)
                for (int x = (int)(hx - 7 * s - 2); x <= (int)(hx + 7 * s + 2); x++) {
                    float u = (x + 0.5f - hx) / (5.5f * s) * 1.139f, v = -1.0f + (tip - (y + 0.5f)) / (10 * s) * 2.25f;
                    float aa = u * u + v * v - 1.0f;
                    if (aa * aa * aa - u * u * v * v * v <= 0.0f) mk_px(x, y);
                }
            static const uint8_t HR[4] = { PAL_PAINT(PAINT_CHERRY, 0), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_ROSE, 2), PAL_WHITE };
            pillow(HR, PAL_PAINT(PAINT_BLOOD, 0), 2.0f, 0.3f);
        }
    }
    /* stink lines escape the device too */
    if (f >= F_POOP + 2 && f < F_EVIL) {
        for (int k = 0; k < 3; k++) {
            int a = (f - F_POOP + k * 7) % 21;
            int sx = lcd_x + 60 + k * 5, sy = lcd_y + 14 - a;
            for (int j = 0; j < 6; j++) tk_px(sx + (int)(1.5f * sinf((sy + j) * 0.7f + k)), sy + j, PAL_RAMP(RAMP_SICK, 4));
        }
    }

    /* the paw that presses A */
    if (f >= F_PRESS - 8 && f < F_PRESS + 14) {
        int a = f - (F_PRESS - 8);
        float k = a < 8 ? tk_ease(a / 8.0f) : a < 12 ? 1.0f : 1.0f - tk_ease((a - 12) / 10.0f);
        float px = cx - 24 + 6 + 70 * (1 - k), py = cy + 44 - 8 + 70 * (1 - k);
        if (a >= 8 && a < 12) py += 2;
        paw(px, py);
    }

    /* ---- captions & bubbles ---- */
    uint8_t oc = PAL_RAMP(RAMP_VAPE, 1);
    if (f >= 6 && f < F_PRESS) {
        bcap(9, "Raise your very own", PAL_WHITE, oc, 1, (f - 6) * 2);
        if (f >= 12) bcap(24, "DOOMOGACHI!", PAL_PAINT(PAINT_BUBBLEGUM, 2), oc, 2, f - 12);
    }
    if (f >= 40 && f < F_PRESS) bubble(165, 40, "beep.", f - 40);
    if (f >= F_POOP + 2 && f < F_EVIL) bcap(16, "It loves you!", PAL_WHITE, oc, 2, f - F_POOP - 2);
    if (f >= F_BEEP2 && f < F_BEEP2 + 18) bubble(165, 40, "beep.", f - F_BEEP2);
    if (f >= 142) bcap(16, "...it's fine.", PAL_WHITE, oc, 2, f - 142);

    if (evil) {
        /* hard cut: the LCD fills the whole screen, 10 px per LCD pixel */
        int sx = (f & 1) ? 3 : -3, sy = (f & 2) ? -2 : 2;
        lcd_render_at(sx, sy, 10, 1);
        tint_ellipse(120, 80, 140, 100, cmap(0, 0, 0, 6), 0.6f, 16);
        if (f & 1) tk_post_glitch(f * 13, 14);
    }
}

void scene_s04_doomogachi_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    tk_sfx(t0 + 7 * fr, SFX_THUD, 0.7f);
    tk_sfx(t0 + 7 * fr, SFX_POP, 0.4f);
    for (int k = 0; k < 6; k++) tk_note(t0 + (10 + k * 4) * fr, 0.04, 76 + k, TKV_SQ50, 0.16f);   /* wobble ticks */
    tk_sfx(t0 + 28 * fr, SFX_BLIP, 0.6f);
    tk_sfx(t0 + 32 * fr, SFX_BLIP, 0.7f);
    tk_sfx(t0 + F_HATCH * fr, SFX_POP, 1.0f);
    tk_sfx(t0 + (F_HATCH + 1) * fr, SFX_SPARKLE, 0.6f);
    tk_note(t0 + 41 * fr, 0.12, 88, TKV_SQ50, 0.32f);                                            /* beep. */
    tk_sfx(t0 + F_PRESS * fr, SFX_SELECT, 0.6f);
    tk_note(t0 + F_PRESS * fr, 0.02, 40, TKV_NOISE, 0.3f);                                        /* click */
    for (int k = 0; k < 3; k++) tk_slide(t0 + (F_FEED + 3 + k * 6) * fr, 0.07, 64, 52, TKV_SQ25, 0.35f);  /* nom nom nom */
    tk_note(t0 + F_LOVE * fr, 0.08, 84, TKV_SQ25, 0.25f);
    tk_note(t0 + (F_LOVE + 3) * fr, 0.08, 88, TKV_SQ25, 0.25f);
    tk_note(t0 + (F_LOVE + 6) * fr, 0.16, 91, TKV_SQ25, 0.25f);
    tk_slide(t0 + F_STRAIN * fr, 0.26, 48, 60, TKV_TRI, 0.35f);                                 /* nnnngh */
    tk_slide(t0 + F_POOP * fr, 0.16, 50, 36, TKV_SAW, 0.4f);                                    /* pfrrt */
    tk_sfx(t0 + F_POOP * fr, SFX_SPLAT, 0.2f);
    tk_sfx(t0 + (F_POOP + 2) * fr, SFX_SPARKLE, 0.35f);
    /* four frames of hell */
    tk_slide(t0 + F_EVIL * fr, 4 * fr, 33, 28, TKV_SAW, 0.9f);
    tk_note(t0 + F_EVIL * fr, 4 * fr, 34, TKV_SQ12, 0.6f);
    tk_note(t0 + F_EVIL * fr, 4 * fr, 40, TKV_SQ12, 0.5f);
    tk_sfx(t0 + F_EVIL * fr, SFX_GLITCH, 0.6f);
    tk_note(t0 + (F_BEEP2 + 1) * fr, 0.12, 88, TKV_SQ50, 0.32f);                                  /* beep. */
}
