/* s02_title.c - the title card.
 *
 * A pastel paradise with depth: banded sky, sun bloom and slow god rays, a
 * translucent rainbow, drifting clouds, hazy candy mountains, cottage hills
 * and a flower meadow, all on parallax layers while the camera cranes up.
 * Puffy candy letters "DOOMOGACHI" drop in with squash & stretch (the two
 * O's of DOOM are eyes), "LIFE" slams down, "in HELLvile Park!" pops in.
 * Then the "HELL" starts to drip... the logo notices, sweats, shakes it off,
 * flings the drop straight onto the camera lens - SPLAT - beat - a smiley
 * star sticker gets slapped over it. "nothing to see here!" Another drip
 * escapes: heart sticker. The logo looks away and whistles.
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
#define F_DRIP 44       /* the drip starts forming */
#define F_FLING 68      /* nervous shake flings it at the camera */
#define F_SPLAT 74      /* it hits the lens */
#define F_STAR 84       /* sticker #1 */
#define F_HEARTS 104    /* sticker #2 */
#define F_WHISTLE 108

static float camx, camy;            /* camera drift (world moves by -cam*depth) */

/* ---- background ------------------------------------------------------------ */
static void cloud(float x, float y, float s) {
    mk_ellipse(x, y + 2.2f * s, 13 * s, 4.2f * s);
    mk_ellipse(x - 6.5f * s, y + 0.5f * s, 5.5f * s, 4.5f * s);
    mk_ellipse(x + 1.5f * s, y - 2.5f * s, 7.5f * s, 6.5f * s);
    mk_ellipse(x + 9 * s, y + 0.8f * s, 5 * s, 4.2f * s);
    static const uint8_t CL[4] = { PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE, PAL_WHITE };
    pillow(CL, PAL_RAMP(RAMP_VAPE, 6), 2.5f, 1.1f);
}
static float ridge_mid(float x) { return 121 + 6 * sinf(x * 0.033f + 1.3f) + 3.5f * sinf(x * 0.081f + 3.0f); }
static float ridge_near(float x) { return 146 + 3 * sinf(x * 0.045f + 0.4f) + 1.5f * sinf(x * 0.11f); }

static void cottage(int x, int g, int roofp, int f, int i) {
    uint8_t ol = PAL_RAMP(RAMP_PEACH, 3);
    tk_rect(x - 7, g - 10, 14, 11, ol);
    tk_rect(x - 6, g - 9, 12, 10, PAL_UI_CREAM);
    tk_rect(x + 3, g - 9, 3, 10, PAL_RAMP(RAMP_PEACH, 7));
    tk_tri(x - 9, g - 9, x + 9, g - 9, x, g - 19, PAL_PAINT(roofp, 0));
    tk_tri(x - 8, g - 10, x + 8, g - 10, x, g - 18, PAL_PAINT(roofp, 1));
    tk_tri(x - 8, g - 10, x - 1, g - 10, x - 1, g - 17, PAL_PAINT(roofp, 2));
    int lit = ((f / 9 + i) % 5) != 0;
    uint8_t wc = lit ? PAL_UI_GOLD : PAL_PAINT(PAINT_HONEY, 1);
    tk_px(x - 4, g - 6, wc); tk_px(x - 2, g - 6, wc); tk_rect(x - 4, g - 5, 3, 1, wc); tk_px(x - 3, g - 4, wc);
    tk_rect(x + 1, g - 5, 3, 5, PAL_PAINT(PAINT_CARAMEL, 1));
    /* chimney puffing tiny hearts */
    tk_rect(x + 4, g - 17, 3, 5, PAL_PAINT(PAINT_RUST, 2));
    for (int k = 0; k < 2; k++) {
        int ph = (f + k * 20 + i * 7) % 40;
        int hx = x + 5 + (int)(2 * sinf(ph * 0.3f + i)), hy = g - 19 - ph / 2;
        if (ph < 30) { uint8_t c = ph < 18 ? PAL_WHITE : PAL_RAMP(RAMP_PASTEL_PINK, 7); tk_px(hx - 1, hy, c); tk_px(hx + 1, hy, c); tk_rect(hx - 1, hy + 1, 3, 1, c); tk_px(hx, hy + 2, c); }
    }
}
static void lolly(int x, int g, int r, int p1, int f) {
    tk_rect(x, g - 14, 1, 14, PAL_WHITE);
    for (int y = g - 14; y < g; y += 3) tk_px(x, y, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_circle(x, g - 14 - r, r + 1, PAL_PAINT(p1, 0));
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            float d = sqrtf((float)(i * i + j * j));
            if (d > r + 0.3f) continue;
            float a = atan2f((float)j, (float)i) + f * 0.07f;
            int band = (int)floorf(a / 6.2832f * 2 + d * 0.45f) & 1;
            tk_px(x + i, g - 14 - r + j, band ? PAL_PAINT(p1, 1) : PAL_WHITE);
        }
    tk_px(x - r / 2, g - 14 - r - r / 2, PAL_WHITE);
}
static void fluffy_tree(float x, float g, float s, int f) {
    float sw = 0.6f * sinf(f * 0.07f + x);
    mk_ellipse(x + sw, g - 9 * s, 6 * s, 5.5f * s);
    mk_ellipse(x - 4 * s + sw, g - 6 * s, 4 * s, 3.5f * s);
    mk_ellipse(x + 4 * s + sw, g - 6 * s, 4 * s, 3.5f * s);
    tk_rect((int)x - 1, (int)(g - 4 * s), 2, (int)(4 * s) + 1, PAL_RAMP(RAMP_WOOD, 3));
    static const uint8_t TR[4] = { PAL_RAMP(RAMP_MINT, 4), PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_MINT, 7) };
    pillow(TR, PAL_RAMP(RAMP_MINT, 3), 2.0f, 0.7f);
}

static void background(int f) {
    /* sky */
    static const uint8_t SK[6] = { PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7),
                                   PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 5) };
    for (int y = 0; y < TH; y++) {
        float L = (y + camy * 0.1f) / 128.0f * 4.6f;
        for (int x = 0; x < TW; x++) tk_fb[y][x] = qtone(SK, 6, L, x, y);
    }
    /* sun bloom behind the logo + slow god rays */
    float sx = 120, sy = 30 + camy * 0.1f;
    tint_ellipse(sx, sy, 120, 70, cmap(255, 250, 225, 3), 0.7f, 16);
    tint_ellipse(sx, sy, 60, 36, cmap(255, 250, 225, 5), 0.6f, 16);
    const uint8_t *ray = cmap(255, 252, 235, 3);
    for (int y = 0; y < 120; y++)
        for (int x = 0; x < TW; x++) {
            float dx = x - sx, dy = y - sy, d = sqrtf(dx * dx + dy * dy);
            if (d < 24 || d > 150) continue;
            float s = sinf(atan2f(dy, dx) * 9.0f + f * 0.018f);
            float edge = (s - 0.55f) * 8.0f;
            int lvl = edge >= 1 ? 16 : edge > 0 ? 8 : 0;
            if (d > 110) lvl = lvl * (int)(150 - d) / 40;
            if (lvl && BY4[y & 3][x & 3] < lvl) tint_px(x, y, ray);
        }
    /* translucent pastel rainbow */
    static const int RB[6][3] = { { 255, 120, 160 }, { 255, 185, 120 }, { 255, 240, 130 },
                                  { 140, 235, 185 }, { 130, 190, 255 }, { 190, 150, 255 } };
    float rcx = 120 - camx * 0.15f, rcy = 182 + camy * 0.2f;
    for (int y = 0; y < 130; y++)
        for (int x = 0; x < TW; x++) {
            float dx = x - rcx, dy = y - rcy, d = sqrtf(dx * dx + dy * dy);
            float b = (160 - d) / 6.0f;
            if (b < 0 || b >= 6) continue;
            int band = (int)b;
            float fr = b - band;
            if ((fr < 0.15f || fr > 0.85f) && ((x + y) & 1)) continue;
            int fade = y > 95 ? 16 - (y - 95) / 2 : 16;
            if (BY4[y & 3][x & 3] >= fade) continue;
            tint_px(x, y, cmap(RB[band][0], RB[band][1], RB[band][2], 7));
        }
    /* drifting clouds (two depths) */
    for (int i = 0; i < 7; i++) {
        float depth = i < 3 ? 0.25f : 0.5f;
        float sp = i < 3 ? 0.06f : 0.14f;
        float x = fmodf(i * 61.0f + 25 + f * sp - camx * depth + 600, 330) - 45;
        float y = 12 + (i * 37) % 56 + camy * depth;
        cloud(x, y, i < 3 ? 0.7f : 0.95f + (i & 1) * 0.15f);
    }
    /* far candy mountains (hazy lavender, snow caps) */
    layer_begin();
    static const float PK[7][3] = { { 0, 62, 46 }, { 52, 72, 40 }, { 104, 56, 52 }, { 160, 70, 42 },
                                    { 212, 60, 48 }, { 262, 74, 40 }, { 310, 64, 46 } };
    for (int k = 0; k < 7; k++) {
        float px = fmodf(PK[k][0] - camx * 0.3f + 640, 340) - 50, py = PK[k][1] + camy * 0.3f, hw = PK[k][2];
        float base = 128 + camy * 0.3f;
        for (int x = (int)(px - hw); x <= (int)(px + hw); x++) {
            float t = fabsf(x + 0.5f - px) / hw;
            float yt = py + (base - py) * powf(t, 1.25f);
            if (t < 0.06f) yt += 1;
            int lit = x < px;
            for (int y = (int)yt; y < (int)base; y++) {
                float dy = y - yt;
                uint8_t c = lit ? PAL_RAMP(RAMP_VAPE, 7) : PAL_RAMP(RAMP_VAPE, 6);
                float snow = (y - py) < 9 + 3 * sinf(x * 0.7f) ? 1 : 0;
                if (snow) c = lit ? PAL_WHITE : PAL_RAMP(RAMP_SKY, 7);
                if (dy < 1 && !snow) c = lit ? PAL_WHITE : PAL_RAMP(RAMP_VAPE, 7);
                tk_px(x, y, c);
            }
        }
    }
    layer_end(cmap(205, 230, 245, 5));
    /* mid hills with cottages, lollipop trees and fluffy trees */
    static const uint8_t GR[4] = { PAL_RAMP(RAMP_GRASS, 4), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 7) };
    for (int x = 0; x < TW; x++) {
        float wx = x + camx * 0.6f;
        int h = (int)(ridge_mid(wx) + camy * 0.6f);
        float slope = ridge_mid(wx + 1) - ridge_mid(wx - 1);
        for (int y = h; y < TH; y++) {
            float L = 3.2f - (y - h) * 0.09f - slope * 1.2f;
            tk_px(x, y, y == h ? PAL_RAMP(RAMP_GRASS, 5) : qtone(GR, 4, L, x, y));
        }
    }
    static const int HX[] = { 18, 52, 86, 176, 208, 250 };
    static const int RF[] = { PAINT_ROSE, PAINT_SKY, PAINT_LAVENDER, PAINT_PEACH, PAINT_MINT, PAINT_BUBBLEGUM };
    for (int i = 0; i < 6; i++) {
        float wx = HX[i] - camx * 0.6f;
        int g = (int)(ridge_mid(HX[i]) + camy * 0.6f) + 2;
        if (i == 2 || i == 5) fluffy_tree(wx, (float)g, 1.0f, f);
        else cottage((int)wx, g, RF[i], f, i);
    }
    lolly((int)(36 - camx * 0.6f), (int)(ridge_mid(36) + camy * 0.6f) + 2, 4, PAINT_BUBBLEGUM, f);
    lolly((int)(194 - camx * 0.6f), (int)(ridge_mid(194) + camy * 0.6f) + 2, 4, PAINT_SKY, f);
    /* near meadow with swaying flowers */
    static const uint8_t NG[3] = { PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 7) };
    for (int x = 0; x < TW; x++) {
        float wx = x + camx;
        int h = (int)(ridge_near(wx) + camy);
        for (int y = h; y < TH; y++) tk_px(x, y, y == h ? PAL_RAMP(RAMP_GRASS, 7) : qtone(NG, 3, 2.2f - (y - h) * 0.18f, x, y));
    }
    static const int FC[] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_WHITE, PAINT_LAVENDER, PAINT_SKY, PAINT_PEACH };
    for (int i = 0; i < 26; i++) {
        uint32_t s = tk_hash((uint32_t)i * 313u + 9u);
        float wx = (float)(s % 300);
        int x = (int)(wx - camx + 330) % 300 - 30;
        int g = (int)(ridge_near(wx) + camy) + 3 + (int)((s >> 9) % 8);
        int sway = (int)(1.3f * sinf(f * 0.12f + i));
        tk_line(x, g, x + sway, g - 5, PAL_RAMP(RAMP_GRASS, 4));
        uint8_t c = PAL_PAINT(FC[i % 6], 1);
        int fx = x + sway, fy = g - 6;
        tk_px(fx - 1, fy, c); tk_px(fx + 1, fy, c); tk_px(fx, fy - 1, c); tk_px(fx, fy + 1, c);
        tk_px(fx, fy, PAL_PAINT(PAINT_LEMON, 2));
    }
    /* floating motes */
    for (int i = 0; i < 16; i++) {
        uint32_t s = tk_hash((uint32_t)i * 71u + 555u);
        int x = (int)((s % 260) + 6 * sinf(f * 0.05f + i)) - 10;
        int y = (int)fmodf((float)((s >> 8) % 170) - f * 0.35f + 340, 170) - 5;
        int ph = (f + (int)(s >> 20) % 30) % 30;
        if (ph < 20) tk_px(x, y, ph < 10 ? PAL_WHITE : PAL_RAMP(RAMP_PASTEL_PINK, 7));
    }
}

/* ---- the logo -------------------------------------------------------------- */
static Tile T1[10], T2[4], T3[17];
static int tiles_ok;
static const char LOGO1[] = "DOOMOGACHI";
static const char LOGO2[] = "LIFE";
static const char SUB[] = "in HELLvile Park!";
#define L1_ADV 19
#define L1_BOT 46
#define L2_ADV 25
#define L2_BOT 86
#define SUB_ADV 12
#define SUB_BOT 110

enum { EYE_OPEN, EYE_HAPPY, EYE_SHOCK, EYE_BLINK };

/* drop-in motion for a letter whose local age is a */
static void drop_motion(int a, int fall, float height, float *dy, float *sx, float *sy) {
    *dy = 0; *sx = 1; *sy = 1;
    if (a < fall) { float t = (float)(a + 1) / fall; *dy = -height * (1 - t * t); *sx = 0.82f; *sy = 1.25f; return; }
    a -= fall;
    static const float SX[] = { 1.38f, 1.2f, 0.9f, 0.96f, 1.03f, 1.0f };
    static const float SY[] = { 0.62f, 0.8f, 1.12f, 1.05f, 0.98f, 1.0f };
    static const float DY[] = { 0, 0, -4, -4, -1, 0 };
    if (a < 6) { *sx = SX[a]; *sy = SY[a]; *dy = DY[a]; }
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
    case EYE_SHOCK:
        tk_rect((int)(x + lx), (int)(y + ly), 2, 2, k);
        break;
    case EYE_HAPPY:
        for (int i = -3; i <= 3; i++) tk_rect((int)x + i, (int)(y + 1 - (3 - abs(i)) * 0.9f), 1, 2, k);
        break;
    default:
        tk_rect((int)x - 3, (int)y, 7, 1, k);
        break;
    }
}

static void logo(int f, float jx, float jy, int eye, float lx, float ly) {
    const uint8_t *shadow = cmap(90, 40, 120, 7);
    uint8_t ol = PAL_RAMP(RAMP_VAPE, 1);
    static const int P1[10] = { PAINT_BUBBLEGUM, PAINT_SKY, PAINT_MINT, PAINT_LAVENDER, PAINT_LEMON,
                                PAINT_PEACH, PAINT_BUBBLEGUM, PAINT_SKY, PAINT_MINT, PAINT_LAVENDER };
    float x1 = 120 - 4.5f * L1_ADV, x2 = 120 - 1.5f * L2_ADV, x3 = 120 - 8.0f * SUB_ADV;
    float eyex[2] = { 0, 0 }, eyey[2] = { 0, 0 };
    int eyes_on = 0;
    for (int pass = 0; pass < 3; pass++) {
        /* DOOMOGACHI drops in letter by letter */
        for (int i = 0; i < 10; i++) {
            int a = f - 2 - (i * 3) / 2;
            if (a < 0) continue;
            float dy, sx, sy;
            drop_motion(a, 7, 80, &dy, &sx, &sy);
            float wob = f > 34 ? 1.3f * sinf(f * 0.17f + i * 0.7f) : 0;
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
                if ((i == 1 || i == 2) && a >= 12) {
                    eyex[i - 1] = cx + 1; eyey[i - 1] = by - 13.5f * sy; eyes_on = 1;
                }
            }
        }
        /* LIFE slams down as one block */
        int a = f - 14;
        if (a >= 0) {
            float dy, sx, sy;
            drop_motion(a, 5, 110, &dy, &sx, &sy);
            for (int i = 0; i < 4; i++) {
                float wob = f > 34 ? 1.3f * sinf(f * 0.17f + i * 0.9f + 2) : 0;
                float cx = 120 + (x2 + i * L2_ADV - 120) * sx + jx;
                tile_mk(&T2[i], cx, L2_BOT + dy + wob + jy, sx, sy);
                if (pass == 0) { mk_ring(3, 2, 3, 0, shadow); mk_reset(); }
                else if (pass == 1) { mk_ring(3, 0, 0, PAL_WHITE, NULL); mk_reset(); }
                else {
                    static const uint8_t LR[4] = { PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_ROSE, 1),
                                                   PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2) };
                    pillow(LR, ol, 3.0f, 0.6f);
                }
            }
        }
        /* subtitle pops in */
        for (int i = 0; SUB[i]; i++) {
            int a2 = f - 24 - i;
            if (a2 < 0 || SUB[i] == ' ') continue;
            float sx = 1, sy = 1, dy = 0;
            if (a2 < 4) { static const float S[4] = { 0.4f, 1.3f, 0.9f, 1.05f }; sx = sy = S[a2]; dy = a2 == 0 ? -3 : 0; }
            float wob = f > 44 ? 1.0f * sinf(f * 0.2f + i * 0.6f) : 0;
            int hell = i >= 3 && i <= 6;
            if (hell) wob = 0;           /* the HELL part does not wobble. it drips. */
            tile_mk(&T3[i], x3 + i * SUB_ADV + jx, SUB_BOT + dy + wob + jy, sx, sy);
            if (pass == 0) { mk_ring(2, 1, 2, 0, shadow); mk_reset(); }
            else if (pass == 1) { mk_ring(2, 0, 0, PAL_WHITE, NULL); mk_reset(); }
            else if (hell) {
                static const uint8_t HR[4] = { PAL_PAINT(PAINT_BLOOD, 1), PAL_PAINT(PAINT_CHERRY, 1),
                                               PAL_PAINT(PAINT_CHERRY, 2), PAL_PAINT(PAINT_ROSE, 2) };
                pillow(HR, PAL_RAMP(RAMP_BLOOD, 1), 2.0f, 0.5f);
            } else {
                static const uint8_t WR[4] = { PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6),
                                               PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE };
                pillow(WR, ol, 2.0f, 0.5f);
            }
        }
    }
    if (eyes_on)
        for (int e = 0; e < 2; e++) logo_eye(eyex[e], eyey[e], eye, lx, ly);
    /* LIFE landing dust */
    int a = f - 19;
    if (a >= 0 && a < 14) {
        for (int s = -1; s <= 1; s += 2) {
            float px = 120 + s * (54 + a * 3.0f), py = L2_BOT - 4 - a * 0.4f, r = 5.5f - a * 0.38f;
            if (r < 1) continue;
            mk_ellipse(px, py, r, r * 0.85f);
            static const uint8_t DU[4] = { PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE, PAL_WHITE };
            pillow(DU, PAL_RAMP(RAMP_VAPE, 5), 2.0f, 0.5f);
        }
    }
}

/* ---- the drip, the splat, the stickers -------------------------------------- */
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
static void splat(float cx, float cy, int age) {
    uint32_t s = tk_hash(4242);
    float grow = age < 2 ? 1.25f - age * 0.12f : 1.0f;
    mk_ellipse(cx, cy, 15 * grow, 13 * grow);
    for (int i = 0; i < 12; i++) {
        float a = tk_rand(&s, 628) / 100.0f;
        float d = (12 + tk_rand(&s, 10)) * grow, r = (float)(2 + tk_rand(&s, 5));
        mk_ellipse(cx + cosf(a) * d, cy + sinf(a) * d * 0.85f, r, r);
        float d2 = d + 6 + tk_rand(&s, 9);
        mk_ellipse(cx + cosf(a) * d2, cy + sinf(a) * d2 * 0.85f, 1.3f, 1.3f);
    }
    for (int i = 0; i < 4; i++) {          /* running drips */
        float dx = -10 + i * 7.0f, len = 4 + (age * (3 + i * 2)) / 6.0f;
        if (len > 30) len = 30;
        for (float yy = 0; yy < len; yy += 1) mk_ellipse(cx + dx, cy + 7 + yy, 1.6f, 1.0f);
        mk_ellipse(cx + dx, cy + 7 + len, 2.4f, 2.6f);
    }
    mk_ellipse(cx - 36, cy + 12, 4, 4);  /* the one splotch that got away */
    static const uint8_t SR[4] = { PAL_RAMP(RAMP_BLOOD, 3), PAL_RAMP(RAMP_BLOOD, 4), PAL_RAMP(RAMP_BLOOD, 5), PAL_RAMP(RAMP_BLOOD, 7) };
    pillow(SR, PAL_RAMP(RAMP_BLOOD, 1), 4.0f, 0.5f);
    tk_rect((int)cx - 8, (int)cy - 9, 5, 2, PAL_WHITE);
    tk_rect((int)cx - 9, (int)cy - 7, 2, 2, PAL_WHITE);
    tk_px((int)cx + 6, (int)cy - 6, PAL_WHITE);
}
static void mk_star(float cx, float cy, float ro, float ri, float rot) {
    for (int y = (int)(cy - ro - 1); y <= (int)(cy + ro + 1); y++)
        for (int x = (int)(cx - ro - 1); x <= (int)(cx + ro + 1); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float a = atan2f(dy, dx) - rot + 1.5708f, d = sqrtf(dx * dx + dy * dy);
            float seg = fmodf(a + 62.83f, 1.2566f) / 1.2566f;          /* 0..1 within a point */
            float tri = fabsf(seg - 0.5f) * 2;                          /* 1 at point, 0 between */
            float lim = ri + (ro - ri) * (1 - tri);
            /* round the tips and the inner corners */
            if (d <= lim - 0.3f) mk_px(x, y);
        }
}
static void star_sticker(float cx, float cy, float sc, int f) {
    mk_star(cx, cy, 24 * sc, 12.5f * sc, 0.12f);
    mk_ring(3, 2, 3, 0, cmap(60, 20, 30, 7)); mk_reset();
    mk_star(cx, cy, 24 * sc, 12.5f * sc, 0.12f);
    mk_ring(3, 0, 0, PAL_WHITE, NULL); mk_reset();
    mk_star(cx, cy, 24 * sc, 12.5f * sc, 0.12f);
    static const uint8_t ST[4] = { PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_LEMON, 2), PAL_WHITE };
    pillow(ST, PAL_PAINT(PAINT_HONEY, 0), 4.0f, 0.5f);
    if (sc > 1.05f) return;
    uint8_t k = PAL_RAMP(RAMP_VAPE, 0);
    int blink = (f % 40) > 36;
    for (int e = -1; e <= 1; e += 2) {
        int ex = (int)cx + e * 6;
        if (blink) tk_rect(ex - 1, (int)cy, 3, 1, k);
        else { tk_rect(ex - 1, (int)cy - 2, 3, 4, k); tk_px(ex - 1, (int)cy - 2, PAL_WHITE); }
        tint_ellipse(ex + e * 4.0f, cy + 4, 2.5f, 1.4f, cmap(255, 120, 140, 9), 0.3f, 16);
    }
    tk_rect((int)cx - 2, (int)cy + 4, 5, 1, k);
    tk_px((int)cx - 3, (int)cy + 3, k); tk_px((int)cx + 3, (int)cy + 3, k);
    /* peeling corner */
    tk_tri((int)cx + 18, (int)cy - 6, (int)cx + 23, (int)cy - 7, (int)cx + 19, (int)cy - 2, PAL_WHITE);
    tk_line((int)cx + 18, (int)cy - 6, (int)cx + 19, (int)cy - 2, PAL_GRAY_L);
}
static int heart_in(float u, float v) {
    float a = u * u + v * v - 1.0f;
    return a * a * a - u * u * v * v * v <= 0.0f;
}
static void heart_sticker(float cx, float tip, float sc) {
    for (int pass = 0; pass < 3; pass++) {
        float w = 9 * sc, h = 16 * sc;
        for (int y = (int)(tip - h - 2); y <= (int)tip + 1; y++)
            for (int x = (int)(cx - w - 2); x <= (int)(cx + w + 2); x++) {
                float u = (x + 0.5f - cx) / w * 1.139f, v = -1.0f + (tip - (y + 0.5f)) / h * 2.25f;
                if (heart_in(u, v)) mk_px(x, y);
            }
        if (pass == 0) { mk_ring(2, 1, 2, 0, cmap(60, 20, 30, 7)); mk_reset(); }
        else if (pass == 1) { mk_ring(2, 0, 0, PAL_WHITE, NULL); mk_reset(); }
        else {
            static const uint8_t HR[4] = { PAL_PAINT(PAINT_BUBBLEGUM, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1),
                                           PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
            pillow(HR, PAL_RAMP(RAMP_PASTEL_PINK, 1), 3.0f, 0.4f);
        }
    }
    tk_rect((int)(cx - 5 * sc), (int)(tip - 12 * sc), 2, 2, PAL_WHITE);
}
static void music_note(int x, int y, uint8_t c) {
    tk_ellipse(x, y, 2, 1, c);
    tk_rect(x + 2, y - 6, 1, 6, c);
    tk_rect(x + 3, y - 6, 2, 1, c); tk_px(x + 4, y - 5, c);
}

void scene_s02_title(int f, int n) {
    (void)n;
    if (!tiles_ok) {
        tiles_ok = 1;
        for (int i = 0; i < 10; i++) make_tile(&T1[i], (unsigned char)LOGO1[i], 3, 2);
        for (int i = 0; i < 4; i++) make_tile(&T2[i], (unsigned char)LOGO2[i], 4, 2);
        for (int i = 0; SUB[i]; i++) make_tile(&T3[i], (unsigned char)SUB[i], 2, 1);
    }
    /* camera: slow crane-up and sideways drift for parallax */
    camy = 18 * (1 - tk_ease(f / 60.0f));
    camx = f * 0.35f;
    background(f);

    /* nervous logic */
    float jx = 0, jy = 0;
    if (f >= 58 && f < F_FLING) { jx = (float)(((f * 7) % 5) - 2); jy = (f & 1) ? -1.0f : 1.0f; }
    if (f >= F_FLING && f < F_FLING + 3) { jx = (f & 1) ? 3.0f : -3.0f; }

    int eye = EYE_OPEN;
    float lx = 0, ly = 0;
    if (f < 26) eye = EYE_BLINK;
    else if (f < 40) { lx = ((f / 6) & 1) ? 1.2f : -1.2f; ly = -1; }
    else if (f < 50) eye = EYE_HAPPY;
    else if (f < 58) { lx = 1.0f; ly = 3.0f; }                  /* ...looking down at the drip */
    else if (f < F_SPLAT) { eye = EYE_SHOCK; lx = 0.5f; ly = 3; }
    else if (f < 82) { eye = EYE_SHOCK; lx = 1.5f; ly = 3; }    /* staring at the lens. */
    else if (f < 98) eye = EYE_HAPPY;
    else if (f < F_HEARTS) { eye = EYE_SHOCK; lx = 1.5f; ly = 3; }
    else if (f < F_WHISTLE) eye = EYE_HAPPY;
    else { lx = -1.5f; ly = -2; }                                /* looks away. whistles. */
    if (f == 44 || f == 45 || f == 116) eye = EYE_BLINK;

    layer_begin();
    logo(f, jx, jy, eye, lx, ly);
    if (f >= 40 && f < 50) {                       /* glossy shine sweep */
        layer_band(cmap(255, 255, 255, 10), 30 + (f - 40) * 32, 8);
        layer_band(cmap(255, 255, 255, 7), 30 + (f - 40) * 32 + 12, 3);
    }
    if (f >= 110 && f < 120) {                     /* ...and once more, innocently */
        layer_band(cmap(255, 255, 255, 10), 30 + (f - 110) * 32, 8);
        layer_band(cmap(255, 255, 255, 7), 30 + (f - 110) * 32 + 12, 3);
    }

    /* sparkles on the logo while it is happy */
    if (f >= 22 && f < F_DRIP + 6) tk_sparkles(120, 60, f - 22, 21, 92);

    /* sweat drop on the D */
    if (f >= 54 && f < 96) {
        float sx = 120 - 4.5f * L1_ADV - 11 + jx, sy = 20 + jy + ((f / 3) % 4);
        mk_ellipse(sx, sy + 2, 2.0f, 2.6f); mk_ellipse(sx, sy, 0.8f, 1.4f);
        static const uint8_t SW[4] = { PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 2), PAL_RAMP(RAMP_SKY, 7), PAL_WHITE };
        pillow(SW, PAL_PAINT(PAINT_COBALT, 1), 1.5f, 0.2f);
    }

    /* THE DRIP from the second L of HELL */
    float dx = 120 - 8.0f * SUB_ADV + 6 * SUB_ADV + 1 + jx, dtop = SUB_BOT - 1 + jy;
    if (f >= F_DRIP && f < F_FLING) {
        int t = f - F_DRIP;
        float len = t < 12 ? t / 3.0f : 4 + (t - 12) * 0.6f;
        float r = 1.2f + t * 0.09f;
        if (r > 3.2f) r = 3.2f;
        red_drop(dx, dtop + len + r, r, dtop);
    }
    /* flung at the camera, growing as it flies closer */
    if (f >= F_FLING && f < F_SPLAT) {
        float t = (f - F_FLING) / (float)(F_SPLAT - F_FLING);
        float x = dx + (198 - dx) * t * t, y = dtop + 14 + (130 - dtop - 14) * t * t - 22 * 4 * t * (1 - t);
        red_drop(x, y, 3 + 17 * t * t, y);
    }
    /* SPLAT on the lens */
    if (f >= F_SPLAT) splat(198, 128, f - F_SPLAT);
    /* the escaped drip */
    if (f >= 96) {
        int a = f - 96;
        float yy = 144 + (a < 8 ? a * 1.2f : 9.6f);
        red_drop(162, yy, 2.4f, 140);
    }
    /* stickers */
    if (f >= F_STAR) {
        static const float SC[] = { 1.6f, 1.25f, 0.92f, 1.04f, 1.0f };
        int a = f - F_STAR;
        star_sticker(200, 131, a < 5 ? SC[a] : 1.0f, f);
    }
    if (f >= F_HEARTS) {
        static const float SC[] = { 1.8f, 1.3f, 0.95f, 1.0f };
        int a = f - F_HEARTS;
        heart_sticker(163, 160, a < 4 ? SC[a] : 1.0f);
    }
    if (f >= F_STAR + 3) {
        int a = f - F_STAR - 3;
        uint8_t oc = PAL_RAMP(RAMP_VAPE, 1);
        int dy = a == 0 ? -3 : a == 1 ? 1 : 0;
        tk_text_outline(10, 133 + dy, "nothing to", PAL_WHITE, oc, 1);
        tk_text_outline(10, 143 + dy, "see here!", PAL_WHITE, oc, 1);
        if (a >= 4) tk_heart(10 + 9 * 6 + 5, 146 + dy, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    }
    /* innocent whistling */
    if (f >= F_WHISTLE) {
        for (int k = 0; k < 3; k++) {
            int ph = (f - F_WHISTLE + k * 7) % 21;
            int nx = 24 + ph / 2 + (int)(2 * sinf(ph * 0.5f + k)), ny = 22 - ph;
            if (f - F_WHISTLE >= k * 4) music_note(nx, ny, PAL_RAMP(RAMP_VAPE, 1));
        }
    }

    if (f >= F_SPLAT && f < F_SPLAT + 3) tk_post_shake((f & 1) ? 3 : -3, (f & 1) ? -2 : 2);
    if (f >= 19 && f < 21) tk_post_shake(0, f == 19 ? 2 : -1);
    if (f < 4) tk_post_flash(PAL_WHITE, 16 - f * 4);
    /* director: cartoon heart iris-out that lands right on the "nothing to see
     * here" sticker, holds a beat, then closes (s03 irises back open) */
    if (f >= 111) {
        int a = f - 111;
        float r = a < 5 ? 260.0f + (26.0f - 260.0f) * tk_ease(a / 5.0f) : (f < 119 ? 26.0f : 0.0f);
        tk_post_iris(200, 128, r, 1, PAL_BLACK, PAL_UI_PINK_D);
    }
}

void scene_s02_title_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    static const int notes[10] = { 72, 74, 76, 77, 79, 81, 83, 84, 86, 88 };
    for (int i = 0; i < 10; i++) tk_slide(t0 + ((i * 3) / 2 + 9) * fr, 0.06, notes[i] - 5, notes[i], TKV_SQ25, 0.2f);
    tk_sfx(t0 + 19 * fr, SFX_THUD, 0.9f);
    tk_sfx(t0 + 19 * fr, SFX_BOING, 0.55f);
    for (int i = 0; i < 6; i++) tk_note(t0 + (25 + i * 2) * fr, 0.04, 84 + i * 2, TKV_SINE, 0.12f);
    tk_note(t0 + 26 * fr, 0.05, 81, TKV_SINE, 0.3f);                 /* eyes open: plink */
    tk_note(t0 + 28 * fr, 0.05, 88, TKV_SINE, 0.3f);
    tk_sfx(t0 + 40 * fr, SFX_SPARKLE, 0.45f);                       /* shine */
    tk_note(t0 + 52 * fr, 0.08, 52, TKV_SQ12, 0.2f);                 /* drip... */
    tk_slide(t0 + 54 * fr, 0.5, 43, 40, TKV_SAW, 0.22f);             /* uh-oh */
    for (int i = 0; i < 10; i++)                                       /* nervous jitter */
        tk_note(t0 + (58 + i) * fr, fr * 0.8, (i & 1) ? 74 : 75, TKV_SQ12, 0.16f);
    tk_sfx(t0 + F_FLING * fr, SFX_WHOOSH, 0.7f);
    tk_sfx(t0 + F_SPLAT * fr, SFX_SPLAT, 1.0f);
    /* ...silence... */
    tk_sfx(t0 + F_STAR * fr, SFX_POP, 0.8f);
    tk_sfx(t0 + (F_STAR + 1) * fr, SFX_SQUEAK, 0.6f);
    tk_sfx(t0 + (F_STAR + 5) * fr, SFX_SPARKLE, 0.35f);
    tk_note(t0 + 96 * fr, 0.08, 50, TKV_SQ12, 0.18f);                /* drip again */
    tk_sfx(t0 + F_HEARTS * fr, SFX_POP, 0.65f);
    /* whistle: innocent little tune */
    static const int W[5] = { 84, 88, 91, 88, 93 };
    for (int i = 0; i < 5; i++) tk_slide(t0 + (F_WHISTLE + i * 2.4) * fr, 0.075, W[i] - 1, W[i], TKV_SINE, 0.2f);
    tk_slide(t0 + 111 * fr, 0.16, 96, 72, TKV_SINE, 0.12f);          /* iris: shwip */
}
