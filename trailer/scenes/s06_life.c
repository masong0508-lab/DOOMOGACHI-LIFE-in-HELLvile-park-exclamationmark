/* s06_life.c - life in Hillvile Towers (a Tomodachi-style cut-away).
 *
 * Night. The camera tilts down a plush apartment block, floor by floor,
 * past a hazy twinkling city and a big moon. Each room is a little
 * dollhouse box (back wall, side walls, ceiling, perspective floorboards)
 * lit by its own lamp:
 *   top:    two friends high-five... it escalates into a grawlix screaming
 *           match with glare lightning and a POW ("Make friends!"). Next
 *           door a sheep sleeps through it in moonlight while their
 *           Doomogachi goes "beep." (its screen flashes red for 2 frames).
 *   middle: a fox and a cat fall in love, giant heart burst ("Fall in
 *           love!"); Nana eats a cupcake and rates it 10/10 - she baked it
 *           herself ("Eat cupcakes!").
 *   bottom: a dance party with a disco ball... and, in a room with the
 *           lights off, the only plain human in town, standing perfectly
 *           still, not breathing, not blinking, staring at you. His eyes
 *           already glint at the bottom of the previous shot.
 *           ("Ignore that one.")
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
#define BX0 12
#define BX1 228
#define RW 98            /* room width  */
#define RH 84            /* room height */
#define FH 92            /* floor pitch */
#define TOPF 44          /* world y of the top floor's rooms */
#define LX 18            /* left room x  */
#define RXR 124          /* right room x */

static float camy;
static int floor_top(int fl) { return TOPF + (2 - fl) * FH - (int)camy; }   /* fl 2 = top */

/* ---- per-room clipping: snapshot, draw, restore outside the room ---------- */
static uint8_t clip_save[TH][TW];
static void clip_begin(void) { memcpy(clip_save, tk_fb, sizeof clip_save); }
static void clip_end(int x, int y, int w, int h) {
    for (int j = 0; j < TH; j++)
        for (int i = 0; i < TW; i++)
            if (i < x || i >= x + w || j < y || j >= y + h) tk_fb[j][i] = clip_save[j][i];
}
static void mk_rrect(float x, float y, float w, float h, float r) {
    for (int j = (int)y; j < (int)(y + h); j++)
        for (int i = (int)x; i < (int)(x + w); i++) {
            float dx = fmaxf(fmaxf(x + r - (i + 0.5f), (i + 0.5f) - (x + w - r)), 0);
            float dy = fmaxf(fmaxf(y + r - (j + 0.5f), (j + 0.5f) - (y + h - r)), 0);
            if (dx * dx + dy * dy <= r * r) mk_px(i, j);
        }
}
static void mk_heart(float cx, float tip, float w, float h) {
    for (int y = (int)(tip - h - 2); y <= (int)tip + 1; y++)
        for (int x = (int)(cx - w - 2); x <= (int)(cx + w + 2); x++) {
            float u = (x + 0.5f - cx) / w * 1.139f, v = -1.0f + (tip - (y + 0.5f)) / h * 2.25f;
            float a = u * u + v * v - 1.0f;
            if (a * a * a - u * u * v * v * v <= 0.0f) mk_px(x, y);
        }
}
static void pheart(float cx, float tip, float s, int p) {
    mk_heart(cx, tip, 4.5f * s, 8 * s);
    uint8_t r4[4] = { PAL_PAINT(p, 0), PAL_PAINT(p, 1), PAL_PAINT(p, 2), PAL_WHITE };
    pillow(r4, p == PAINT_CHERRY ? PAL_PAINT(PAINT_BLOOD, 0) : PAL_PAINT(p, 0), 2.0f * s, 0.3f);
}

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

/* ---- a dollhouse room: back wall, side walls, ceiling, floor in perspective - */
#define DEPTH 9          /* how far the back wall is inset */
#define FLOOR_D 14       /* floor depth on screen */
static void room_shell(int x, int y, uint8_t wa, uint8_t wb, int pat, uint8_t fl0, uint8_t fl1) {
    int bx0 = x + DEPTH, bx1 = x + RW - DEPTH, by0 = y + 5, by1 = y + RH - FLOOR_D;
    /* back wall with wallpaper */
    for (int j = by0; j < by1; j++)
        for (int i = bx0; i < bx1; i++) {
            int u = i - bx0, v = j - by0, on = 0;
            if (pat == 0) on = (u % 8) < 3;
            else if (pat == 1) on = ((u + (v / 8) * 4) % 8 == 0) && (v % 8 == 2);
            else if (pat == 2) on = ((u / 6 + v / 6) & 1) && ((u + v) & 1);
            else on = (v % 10 == 0) || (u % 14 == 0);
            tk_px(i, j, on ? wb : wa);
        }
    /* side walls + ceiling: same paper, in shade */
    const uint8_t *shade = cmap(70, 40, 110, 5), *deep = cmap(60, 30, 90, 8);
    for (int j = y; j < y + RH; j++)
        for (int i = x; i < x + RW; i++) {
            int left = i < bx0 && (i - x) * (by1 - by0 + 0) >= 0;
            float tl = (float)(i - x) / DEPTH;                 /* 0 at the outer edge */
            int in_left = i < bx0 && j >= y + (int)(5 * tl) && j < by1 + (int)((1 - tl) * FLOOR_D);
            float tr = (float)(x + RW - 1 - i) / DEPTH;
            int in_right = i >= bx1 && j >= y + (int)(5 * tr) && j < by1 + (int)((1 - tr) * FLOOR_D);
            int in_ceil = j < by0 && i >= x + (int)((j - y) * DEPTH / 5.0f) && i < x + RW - (int)((j - y) * DEPTH / 5.0f);
            (void)left;
            if (in_left || in_right) tk_px(i, j, shade[(((i + j) / 3) & 1) && pat == 0 ? wb : wa]);
            else if (in_ceil) tk_px(i, j, deep[wa]);
        }
    /* floor: planks converging toward the back wall */
    for (int j = by1; j < y + RH; j++) {
        float t = (float)(j - by1) / FLOOR_D;                 /* 0 back .. 1 front */
        int l = bx0 - (int)(DEPTH * t), r = bx1 + (int)(DEPTH * t);
        for (int i = l; i < r; i++) {
            float u = (float)(i - l) / (r - l);
            int plank = (int)(u * 7);
            uint8_t c = (plank & 1) ? fl0 : fl1;
            if ((int)(u * 7 * 8) % 8 == 0) c = cmap(60, 30, 40, 6)[fl0];
            tk_px(i, j, c);
        }
    }
    tk_rect(bx0, by1 - 2, bx1 - bx0, 2, PAL_WHITE);                  /* skirting */
    tk_rect(bx0, by1, bx1 - bx0, 1, cmap(60, 30, 40, 6)[fl0]);
}
static void lamp(int x, int y, int on) {
    tk_rect(x, y, 1, 6, PAL_GRAY_D);
    mk_ellipse((float)x + 0.5f, (float)y + 8, 6, 3.5f);
    mk_rect(x - 5, y + 8, 12, 2);
    static const uint8_t LO[4] = { PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_LEMON, 2), PAL_WHITE };
    static const uint8_t LF[4] = { PAL_GRAY_D, PAL_GRAY, PAL_GRAY_L, PAL_GRAY_L };
    pillow(on ? LO : LF, on ? PAL_PAINT(PAINT_HONEY, 0) : PAL_GRAY_D, 2.0f, 0.4f);
    if (on) tk_rect(x - 1, y + 10, 3, 2, PAL_WHITE);
}
static void lamp_light(int x, int y) {
    tint_ellipse((float)x, (float)y + 24, 46, 34, cmap(255, 240, 200, 3), 0.6f, 16);
    tint_ellipse((float)x, (float)y + 12, 18, 10, cmap(255, 250, 220, 5), 0.6f, 16);
}
static void room_vignette(int x, int y) {
    const uint8_t *m = cmap(60, 30, 90, 4);
    for (int j = y; j < y + RH; j++)
        for (int i = x; i < x + RW; i++) {
            float dx = (i - (x + RW / 2.0f)) / (RW / 2.0f), dy = (j - (y + RH / 2.0f)) / (RH / 2.0f);
            float d = dx * dx * 0.8f + dy * dy;
            if (d > 0.7f) {
                int lvl = (int)((d - 0.7f) * 30);
                if (BY4[j & 3][i & 3] < lvl) tint_px(i, j, m);
            }
        }
}
static void night_window(int x, int y, int f) {
    mk_rrect((float)x - 2, (float)y - 2, 26, 22, 3);
    static const uint8_t FR[4] = { PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PEACH, 7), PAL_WHITE, PAL_WHITE };
    pillow(FR, PAL_RAMP(RAMP_PASTEL_PINK, 2), 1.5f, 0.2f);
    static const uint8_t NS[3] = { PAL_RAMP(RAMP_SKY, 1), PAL_RAMP(RAMP_SKY, 2), PAL_RAMP(RAMP_VAPE, 3) };
    for (int j = 0; j < 18; j++) for (int i = 0; i < 22; i++) tk_px(x + i, y + j, qtone(NS, 3, j / 18.0f * 2.2f, x + i, y + j));
    tk_px(x + 4, y + 4, PAL_WHITE); tk_px(x + 17, y + 11, (f / 10) & 1 ? PAL_WHITE : PAL_RAMP(RAMP_SKY, 5));
    tk_circle(x + 14, y + 6, 3, PAL_PAINT(PAINT_LEMON, 2)); tk_circle(x + 15, y + 5, 2, PAL_RAMP(RAMP_SKY, 1));
    tk_rect(x + 10, y, 2, 18, PAL_WHITE); tk_rect(x, y + 8, 22, 2, PAL_WHITE);
    tk_rect(x - 4, y - 4, 6, 25, PAL_PAINT(PAINT_ROSE, 2)); tk_rect(x + 20, y - 4, 6, 25, PAL_PAINT(PAINT_ROSE, 2));
    tk_rect(x - 4, y - 4, 1, 25, PAL_PAINT(PAINT_ROSE, 1)); tk_rect(x + 25, y - 4, 1, 25, PAL_PAINT(PAINT_ROSE, 1));
}
static void anger(int x, int y, int t) {
    uint8_t r = PAL_UI_RED;
    int s = (t / 3) & 1;
    for (int k = 0; k < 4; k++) {
        int dx = (k & 1) ? (k == 1 ? 3 + s : -4 - s) : -1, dy = (k & 1) ? -1 : (k == 0 ? -4 - s : 2 + s);
        tk_rect(x + dx, y + dy, 2, 2, r);
        tk_px(x + dx + (dx > 0 ? -1 : 2), y + dy + (dy > 0 ? -1 : 2), r);
    }
}
static void grawlix(int x, int y, int age, const char *s) {
    int w = tk_text_w(s, 1) + 8, dy = age == 0 ? -3 : 0;
    mk_rrect((float)x, (float)(y + dy), (float)w, 13, 4);
    mk_tri(x + w / 2.0f - 3, y + 12.0f + dy, x + w / 2.0f + 3, y + 12.0f + dy, x + w / 2.0f, y + 17.0f + dy);
    static const uint8_t BW[4] = { PAL_GRAY_L, PAL_WHITE, PAL_WHITE, PAL_WHITE };
    pillow(BW, PAL_BLACK, 1.5f, 0.2f);
    tk_text(x + 4, y + 3 + dy, s, PAL_UI_RED, 1);
}
static void cupcake(float x, float y, int bites) {
    /* bottom-centre at (x, y) */
    mk_tri(x - 6, y - 8, x + 6, y - 8, x, y + 6);
    mk_rect((int)x - 5, (int)y - 8, 10, 8);
    static const uint8_t WR[4] = { PAL_PAINT(PAINT_SKY, 0), PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 2), PAL_WHITE };
    pillow(WR, PAL_PAINT(PAINT_COBALT, 1), 1.5f, 0.3f);
    for (int i = -3; i <= 3; i += 2) tk_rect((int)x + i, (int)y - 7, 1, 6, PAL_PAINT(PAINT_SKY, 1));
    if (bites >= 3) return;
    mk_ellipse(x, y - 9, 7, 3.5f);
    if (bites < 2) mk_ellipse(x, y - 12.5f, 4.5f, 2.5f);
    static const uint8_t FR[4] = { PAL_PAINT(PAINT_BUBBLEGUM, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
    pillow(FR, PAL_RAMP(RAMP_PASTEL_PINK, 1), 2.0f, 0.3f);
    if (bites < 1) { mk_ellipse(x, y - 16, 2, 2); static const uint8_t CR[4] = { PAL_PAINT(PAINT_BLOOD, 1), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_CHERRY, 2), PAL_WHITE }; pillow(CR, PAL_PAINT(PAINT_BLOOD, 0), 1.0f, 0.2f); }
    if (bites >= 1) tk_ellipse((int)x + 4, (int)y - 11, 2, 2, PAL_RAMP(RAMP_PASTEL_PINK, 7));
}
static void zzz(int x, int y, int f) {
    for (int i = 0; i < 3; i++) {
        int ph = (f + i * 12) % 36;
        int zx = x + ph / 3 + (int)(2 * sinf(ph * 0.3f)), zy = y - ph;
        if (ph < 30) tk_text_outline(zx, zy, i == 1 ? "Z" : "z", PAL_WHITE, PAL_RAMP(RAMP_SKY, 2), 1);
    }
}

/* ---- the six rooms ----------------------------------------------------------- */
static void room_friends(int x, int y, int f) {
    room_shell(x, y, PAL_PAINT(PAINT_LEMON, 2), PAL_RAMP(RAMP_CANDY, 7), 0, PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6));
    lamp(x + RW / 2, y, 1);
    int fy = y + RH - 5;
    TkChibi a = tk_random_citizen(120, 0), b = tk_random_citizen(77, 0);
    a.ears = TKEAR_CAT; a.tail = TKT_CAT; a.skin = PAL_PAINT(PAINT_SKY, 1); a.fluff = 1; a.hair_style = TKH_SPIKY; a.hair = PAL_PAINT(PAINT_COBALT, 1);
    a.wings = 0; a.hat = 0; a.halo = 0; a.horns = 0; a.outfit = PAL_PAINT(PAINT_WHITE, 1);
    b.ears = TKEAR_BUNNY; b.tail = TKT_BUNNY; b.skin = PAL_PAINT(PAINT_PEACH, 1); b.fluff = 1; b.hair_style = TKH_BALD;
    b.wings = 0; b.hat = 0; b.halo = 0; b.horns = 0; b.outfit = PAL_PAINT(PAINT_ROSE, 1);
    int t = f;
    if (t < 13) {           /* high five! */
        int hop = (t > 4 && t < 10) ? 3 : 0;
        tk_chibi(x + 30, fy - hop, &a, 2, TKE_HAPPY, TKM_OPEN, t, 0);
        tk_chibi(x + 68, fy - hop, &b, 2, TKE_HAPPY, TKM_OPEN, t + 30, 1);
        if (t >= 6 && t < 12) { tk_star(x + 49, fy - 42 - hop, 6 - (t - 6), PAL_WHITE); tk_sparkles(x + 49, fy - 40, t, 3, 10); }
    } else {
        int sh = (t / 2) & 1;
        int lean = t < 16 ? 0 : t < 30 ? 3 : 5;
        tk_chibi(x + 28 + lean + sh, fy, &a, 2, TKE_ANGRY, (t / 4) & 1 ? TKM_SCREAM : TKM_OPEN, t, 0);
        tk_chibi(x + 70 - lean - sh, fy, &b, 2, TKE_ANGRY, (t / 4) & 1 ? TKM_OPEN : TKM_SCREAM, t + 30, 1);
        anger(x + 40 + lean, fy - 62, t);
        anger(x + 60 - lean, fy - 64, t + 2);
        int pow = t >= 30 && t < 37;
        if (t >= 15 && !pow) grawlix(x + 6, y + 6, t - 15, (t / 6) & 1 ? "#@%!" : "&*$#");
        if (t >= 19 && !pow) grawlix(x + 56, y + 8, t - 19, (t / 6) & 1 ? "!!$#" : "%@!!");
    }
    if (t >= 22) {          /* glare lightning between their eyes */
        int ey = fy - 42, x0 = x + 40, x1 = x + 58;
        uint32_t r = tk_hash((uint32_t)t * 31u);
        int px = x0, py = ey;
        for (int i = 1; i <= 4; i++) {
            int nx = x0 + (x1 - x0) * i / 4, ny = ey + (i < 4 ? tk_rand(&r, 7) - 3 : 0);
            tk_line(px, py - 1, nx, ny - 1, PAL_PAINT(PAINT_COBALT, 1));
            tk_line(px, py + 1, nx, ny + 1, PAL_PAINT(PAINT_COBALT, 1));
            tk_line(px, py, nx, ny, PAL_PAINT(PAINT_CYAN, 2));
            px = nx; py = ny;
        }
    }
    if (t >= 30 && t < 38) {
        int a2 = t - 30, r = a2 < 3 ? 4 + a2 * 3 : 12;
        tk_star(x + 49, fy - 44, r, PAL_UI_GOLD);
        tk_star(x + 49, fy - 44, r - 3, PAL_WHITE);
        if (a2 < 7) tk_text_outline(x + 32, y + 8 + (a2 < 2 ? -3 : 0), "POW", PAL_UI_GOLD, PAL_UI_RED_D, 2);
    }
    lamp_light(x + RW / 2, y);
}
static void room_sleep(int x, int y, int f) {
    room_shell(x, y, PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7), 1, PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5));
    night_window(x + 64, y + 14, f);
    int fy = y + RH - 5;
    /* bed (front view): headboard, pillow, sleeper, blanket */
    mk_rrect((float)x + 10, (float)fy - 48, 46, 48, 6);
    static const uint8_t HB[4] = { PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6) };
    pillow(HB, PAL_RAMP(RAMP_WOOD, 1), 3.0f, 0.4f);
    tk_heart(x + 33, fy - 41, 1, PAL_PAINT(PAINT_ROSE, 2));
    mk_rrect((float)x + 14, (float)fy - 32, 38, 13, 5);
    static const uint8_t PL[4] = { PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE, PAL_WHITE };
    pillow(PL, PAL_RAMP(RAMP_VAPE, 4), 3.0f, 0.4f);
    TkChibi s = tk_random_citizen(31, 0);
    s.ears = TKEAR_SHEEP; s.skin = PAL_PAINT(PAINT_WHITE, 1); s.fur2 = PAL_SKIN(SKIN_PEACH, 2); s.fluff = 1;
    s.hat = 0; s.wings = 0; s.halo = 0; s.horns = 0; s.hair_style = TKH_AFRO; s.hair = PAL_PAINT(PAINT_WHITE, 2);
    float breathe = sinf(f * 0.12f);
    tk_chibi(x + 33, fy + 18, &s, 2, TKE_SLEEPY, TKM_O, 0, 0);
    int bl = (int)(breathe * 1.2f);
    mk_rrect((float)x + 8, (float)fy - 12 + bl, 50, 14 - bl, 4);
    static const uint8_t BK[4] = { PAL_PAINT(PAINT_LAVENDER, 0), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2), PAL_WHITE };
    pillow(BK, PAL_RAMP(RAMP_VAPE, 3), 3.0f, 0.4f);
    for (int i = 0; i < 46; i += 9) tk_heart(x + 13 + i, fy - 6 + bl, 1, PAL_PAINT(PAINT_LAVENDER, 2));
    zzz(x + 46, fy - 38, f);
    /* nightstand with the Doomogachi on it */
    int nx = x + 66, ny = fy;
    mk_rrect((float)nx, (float)ny - 15, 22, 15, 2);
    static const uint8_t NS[4] = { PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6) };
    pillow(NS, PAL_RAMP(RAMP_WOOD, 1), 2.0f, 0.4f);
    tk_rect(nx + 9, ny - 9, 4, 2, PAL_RAMP(RAMP_WOOD, 6));
    mk_ellipse((float)nx + 11, (float)ny - 22, 5.5f, 6.5f);
    static const uint8_t DG[4] = { PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
    pillow(DG, PAL_RAMP(RAMP_PASTEL_PINK, 1), 2.0f, 0.3f);
    int beep = (f % 60) >= 20 && (f % 60) < 34;
    int evil = f == 31 || f == 32;              /* ...did you see that? */
    tk_rect(nx + 8, ny - 25, 6, 5, evil ? PAL_UI_RED : beep && ((f / 2) & 1) ? PAL_WHITE : PAL_UI_LCD);
    if (beep && !evil) tk_text_outline(nx - 6, ny - 40, "beep.", PAL_WHITE, PAL_RAMP(RAMP_SKY, 2), 1);
    /* night: cool, dim, a soft moonbeam from the window */
    tint_rect(x, y, RW, RH, cmap(40, 50, 110, 5), 16);
    for (int j = 0; j < RH; j++)
        for (int i = 0; i < 26; i++) {
            int px = x + 60 - j / 2 + i, py = y + 30 + j;
            if (py < y + RH) tint_px(px, py, cmap(220, 230, 255, 3));
        }
}
static void room_love(int x, int y, int f) {
    room_shell(x, y, PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), 2, PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6));
    lamp(x + RW / 2, y, 1);
    int fy = y + RH - 5;
    TkChibi a = tk_random_citizen(201, 0), b = tk_random_citizen(9, 0);
    a.ears = TKEAR_FOX; a.tail = TKT_FOX; a.skin = PAL_PAINT(PAINT_HONEY, 1); a.fur2 = PAL_PAINT(PAINT_WHITE, 1); a.fluff = 1;
    a.hair_style = TKH_BALD; a.wings = 0; a.hat = 0; a.halo = 0; a.horns = 0; a.outfit = PAL_PAINT(PAINT_MINT, 1);
    b.ears = TKEAR_CAT; b.tail = TKT_CAT; b.skin = PAL_PAINT(PAINT_LAVENDER, 1); b.fluff = 1;
    b.hair_style = TKH_TWINTAILS; b.hair = PAL_PAINT(PAINT_BUBBLEGUM, 1); b.wings = 0; b.hat = 0; b.halo = 0; b.horns = 0; b.outfit = PAL_PAINT(PAINT_WHITE, 1);
    int burst = 60;
    float t = tk_clampf((f - 46) / 14.0f, 0, 1);
    int close = (int)(8 * tk_ease(t));
    int hop = (f >= burst && f < burst + 8) ? (int)(4 * sinf((f - burst) * 0.4f)) : 0;
    tk_chibi(x + 26 + close, fy - hop, &a, 2, f >= 46 ? TKE_LOVE : TKE_HAPPY, TKM_SMILE, f, 0);
    tk_chibi(x + 72 - close, fy - hop, &b, 2, f >= 46 ? TKE_LOVE : TKE_HAPPY, TKM_CAT, f + 7, 1);
    if (f >= 40 && f < burst) pheart(x + 49 + 3 * sinf(f * 0.4f), fy - 46 - (f - 40) % 12, 0.6f, PAINT_CHERRY);
    if (f >= burst) {
        int a2 = f - burst;
        static const float SZ[] = { 0.3f, 0.8f, 1.6f, 1.9f, 1.6f, 1.6f };
        float sz = a2 < 6 ? SZ[a2] : 1.6f + 0.15f * sinf(a2 * 0.5f);
        pheart(x + 49, fy - 50 + sz * 4, sz, PAINT_CHERRY);
        tk_hearts(x + 49, fy - 30, a2, 11);
        if (a2 < 30) tk_confetti(x + 49, fy - 50, a2, 23);
    }
    lamp_light(x + RW / 2, y);
}
static void room_cupcake(int x, int y, int f) {
    room_shell(x, y, PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_MINT, 7), 0, PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5));
    lamp(x + 32, y, 1);
    int fy = y + RH - 5;
    /* counter with more cupcakes */
    mk_rrect((float)x + 60, (float)fy - 18, 32, 18, 2);
    static const uint8_t CT[4] = { PAL_RAMP(RAMP_PEACH, 4), PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PEACH, 7) };
    pillow(CT, PAL_RAMP(RAMP_PEACH, 2), 2.0f, 0.4f);
    for (int i = 0; i < 2; i++) cupcake(x + 69.0f + i * 14, fy - 19.0f, 0);
    int t = f - 60;
    int bites = t < 6 ? 0 : t < 12 ? 1 : t < 18 ? 2 : 3;
    int sign = t >= 22;
    int hop = sign ? (int)(3 * fabsf(sinf(t * 0.35f))) : 0;
    int chew = (t >= 0 && t < 20 && (t % 6) < 3);
    tk_chibi(x + 34, fy - hop, &TK_NANA, 2, sign ? TKE_SPARKLE : TKE_HAPPY, chew ? TKM_CAT : TKM_OPEN, f, 0);
    if (!sign) {
        float cy = fy - 26.0f - (t >= 0 && t < 20 && (t % 6) < 2 ? 4 : 0);
        cupcake(x + 50.0f, cy, bites);
        if (bites > 0 && t < 22) for (int i = 0; i < 3; i++) tk_px(x + 46 + i * 3, fy - 20 + ((t + i * 2) % 8), PAL_RAMP(RAMP_PEACH, 6));
    } else {
        /* "10/10" (she baked it herself) */
        int sx = x + 66, sy = fy - 54 - hop + (t < 24 ? 6 : 0);
        tk_rect(sx - 1, sy + 14, 3, 18, PAL_RAMP(RAMP_WOOD, 3));
        mk_rrect((float)sx - 18, (float)sy - 1, 37, 16, 3);
        static const uint8_t SG[4] = { PAL_GRAY_L, PAL_WHITE, PAL_WHITE, PAL_WHITE };
        pillow(SG, PAL_PAINT(PAINT_CHERRY, 0), 1.5f, 0.2f);
        tk_text(sx - 14, sy + 3, "10/10", PAL_PAINT(PAINT_CHERRY, 1), 1);
        tk_sparkles(sx, sy + 6, t, 41, 20);
    }
    lamp_light(x + 32, y);
}
static void room_party(int x, int y, int f) {
    room_shell(x, y, PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), 1, PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5));
    int fy = y + RH - 5;
    /* disco ball + travelling light spots */
    tk_rect(x + RW / 2, y, 1, 7, PAL_GRAY_D);
    mk_ellipse(x + RW / 2 + 0.5f, y + 12.0f, 6, 6);
    static const uint8_t DB[4] = { PAL_GRAY, PAL_GRAY_L, PAL_WHITE, PAL_WHITE };
    pillow(DB, PAL_GRAY_D, 2.0f, 0.4f);
    for (int j = -4; j <= 4; j += 2) for (int i = -4; i <= 4; i += 2) if (((i + j + f / 3) & 2) == 0) tk_px(x + RW / 2 + i, y + 12 + j, PAL_WHITE);
    static const int SC[3][3] = { { 255, 120, 180 }, { 140, 230, 255 }, { 255, 240, 120 } };
    for (int i = 0; i < 6; i++) {
        float sx = x + 10 + fmodf(i * 37.0f + f * 2.5f, 78), sy = y + 16 + fmodf(i * 23.0f + f * 0.8f, 46);
        tint_ellipse(sx, sy, 6, 4, cmap(SC[i % 3][0], SC[i % 3][1], SC[i % 3][2], 8), 0.4f, 16);
    }
    TkChibi d = tk_random_citizen(404, 0);
    d.ears = TKEAR_PUP; d.tail = TKT_PUP; d.skin = PAL_PAINT(PAINT_CARAMEL, 1); d.fluff = 1;
    d.hat = 1; d.wings = 0; d.halo = 0; d.horns = 0; d.hair_style = TKH_AFRO; d.hair = PAL_PAINT(PAINT_MAGENTA, 1); d.outfit = PAL_PAINT(PAINT_MAGENTA, 1);
    int hop = (int)(6 * fabsf(sinf(f * 0.39f)));
    tint_ellipse(x + 38.0f, fy + 1.0f, 14 - hop, 3, cmap(40, 20, 60, 6), 0.4f, 16);
    tk_chibi(x + 38, fy - hop, &d, 2, TKE_HAPPY, TKM_OPEN, f, (f / 8) & 1);
    /* boombox */
    int bx = x + 66, by = fy - 13;
    mk_rrect((float)bx, (float)by, 24, 13, 2);
    static const uint8_t BB[4] = { PAL_GRAY_D, PAL_GRAY, PAL_GRAY_L, PAL_WHITE };
    pillow(BB, PAL_BLACK, 1.5f, 0.3f);
    int pump = (f / 4) & 1;
    for (int s = 0; s < 2; s++) { tk_circle(bx + 6 + s * 12, by + 7, 4, PAL_BLACK); tk_circle(bx + 6 + s * 12, by + 7, 1 + pump, PAL_GRAY); }
    tk_rect(bx + 5, by - 3, 14, 2, PAL_GRAY);
    for (int i = 0; i < 3; i++) {
        int ph = (f + i * 10) % 30;
        int nx = bx + 8 + i * 5 + ph / 4, ny = by - 6 - ph;
        uint8_t c = PAL_RAMP(RAMP_VAPE, 1);
        tk_ellipse(nx, ny, 1, 1, c); tk_rect(nx + 1, ny - 5, 1, 5, c); tk_px(nx + 2, ny - 5, c); tk_px(nx + 3, ny - 4, c);
    }
    room_vignette(x, y);
}
static void room_starer(int x, int y, int f) {
    /* lights off. he does not need them. */
    room_shell(x, y, PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 4), 3, PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 4));
    lamp(x + RW / 2, y, 0);
    int fy = y + RH - 5;
    /* a lonely chair */
    tk_rect(x + 74, fy - 22, 2, 22, PAL_RAMP(RAMP_WOOD, 3));
    tk_rect(x + 74, fy - 10, 14, 3, PAL_RAMP(RAMP_WOOD, 3));
    tk_rect(x + 86, fy - 10, 2, 10, PAL_RAMP(RAMP_WOOD, 3));
    /* the starer: not a critter. not animated. not blinking. ever. */
    TkChibi s;
    memset(&s, 0, sizeof s);
    s.skin = PAL_SKIN(SKIN_FAIR, 1); s.hair = PAL_PAINT(PAINT_CHOCOLATE, 1); s.outfit = PAL_PAINT(PAINT_SILVER, 0);
    s.eyes = PAL_PAINT(PAINT_CHARCOAL, 1); s.hair_style = TKH_SHORT; s.ears = TKEAR_HUMAN;
    /* darkness first... */
    tint_rect(x, y, RW, RH, cmap(10, 10, 30, 11), 16);
    /* ...then him, in a faint cold light that comes from nowhere */
    layer_begin();
    if (f >= 136) chibi_wave(x + 46, fy, &s, 2, TKE_NORMAL, TKM_FLAT, 0, 0, ((f - 136) / 8) & 1);
    else tk_chibi(x + 46, fy, &s, 2, TKE_NORMAL, TKM_FLAT, 0, 0);       /* bob 0: no breath, no blink */
    layer_end(cmap(60, 70, 120, 7));
    tint_ellipse(x + 46.0f, fy + 1.0f, 16, 3, cmap(0, 0, 10, 8), 0.4f, 16);
    /* the eyes catch the light */
    int ey = fy - 44;
    tk_px(x + 46 - 9, ey, PAL_WHITE); tk_px(x + 46 + 7, ey, PAL_WHITE);
    (void)f;
}

/* ---- the building + night sky ------------------------------------------------- */
static void sky(int f) {
    static const uint8_t c[5] = { PAL_RAMP(RAMP_SKY, 1), PAL_RAMP(RAMP_SKY, 2), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_PASTEL_PINK, 3) };
    float sy = camy * 0.3f;
    for (int y = 0; y < TH; y++) {
        float L = (y + sy) / 230.0f * 4.0f;
        for (int x = 0; x < TW; x++) tk_fb[y][x] = qtone(c, 5, L, x, y);
    }
    for (int i = 0; i < 40; i++) {
        uint32_t s = tk_hash((uint32_t)i * 77u + 5u);
        int x = (int)(s % TW), y = (int)((s >> 8) % 200) - (int)sy;
        int ph = (f + i * 7) % 40;
        if (ph < 34) tk_px(x, y, ph < 4 ? PAL_RAMP(RAMP_SKY, 5) : PAL_WHITE);
        if (ph == 0 && (i % 4) == 0) tk_star(x, y, 2, PAL_WHITE);
    }
    /* moon with a glow */
    int mx = 205, my = 22 - (int)sy;
    tint_ellipse((float)mx, (float)my, 28, 28, cmap(200, 200, 255, 3), 0.7f, 16);
    tk_circle(mx, my, 9, PAL_PAINT(PAINT_LEMON, 2));
    tk_circle(mx + 4, my - 3, 8, PAL_RAMP(RAMP_SKY, 1));
    /* far city: hazy silhouettes with twinkly windows (parallax) */
    float py = camy * 0.6f;
    for (int i = 0; i < 9; i++) {
        uint32_t s = tk_hash((uint32_t)i * 919u);
        int bx = i * 30 - 10 + (int)(s % 10), bw = 18 + (int)(s % 12), top = 70 + (int)((s >> 8) % 60) - (int)py;
        tk_rect(bx, top, bw, TH + 400, PAL_RAMP(RAMP_VAPE, 2));
        tk_rect(bx, top, bw, 1, PAL_RAMP(RAMP_VAPE, 3));
        for (int wy = top + 4; wy < TH; wy += 7)
            for (int wx = bx + 3; wx < bx + bw - 3; wx += 5) {
                uint32_t q = tk_hash((uint32_t)(wx * 31 + (wy + (int)py) * 7));
                if (q % 3 == 0) tk_rect(wx, wy, 2, 3, ((f / 20 + (int)q) % 9) ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_HONEY, 1));
            }
    }
}
static void building(int f) {
    int top = TOPF - 10 - (int)camy, bot = TOPF + 3 * FH - (int)camy;
    /* body */
    mk_rect(BX0, top, BX1 - BX0, bot - top + 30);
    static const uint8_t BW[4] = { PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PEACH, 7), PAL_WHITE };
    pillow(BW, PAL_RAMP(RAMP_PASTEL_PINK, 1), 3.0f, 0.0f);
    for (int y = top + 4; y < bot + 30; y += 6)
        for (int x = BX0 + 2 + ((y / 6) & 1) * 6; x < BX1 - 2; x += 12) if (y >= 0 && y < TH) tk_rect(x, y, 5, 1, PAL_RAMP(RAMP_PEACH, 5));
    /* floor slabs */
    for (int fl = 0; fl <= 3; fl++) {
        int sy = TOPF - 8 + (3 - fl) * FH - (int)camy;
        if (fl == 3) sy = TOPF - 8 - (int)camy;
        mk_rrect((float)BX0 - 4, (float)sy, (float)(BX1 - BX0 + 8), 8, 3);
        static const uint8_t SL[4] = { PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_WHITE };
        pillow(SL, PAL_RAMP(RAMP_PASTEL_PINK, 2), 2.0f, 0.4f);
    }
    /* roof + sign */
    int sy = 2 - (int)camy;
    mk_rrect((float)BX0 + 16, (float)sy + 4, (float)(BX1 - BX0 - 32), 24, 5);
    static const uint8_t SG[4] = { PAL_PAINT(PAINT_BUBBLEGUM, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
    pillow(SG, PAL_RAMP(RAMP_PASTEL_PINK, 1), 3.0f, 0.4f);
    for (int i = 0; i < 16; i++) {
        int bx = BX0 + 22 + i * 11, on = ((i + f / 4) % 3) != 0;
        tk_px(bx, sy + 6, on ? PAL_UI_GOLD : PAL_PAINT(PAINT_HONEY, 0));
        tk_px(bx, sy + 25, on ? PAL_UI_GOLD : PAL_PAINT(PAINT_HONEY, 0));
        if (on) { tint_ellipse((float)bx, sy + 6.0f, 3, 3, cmap(255, 240, 150, 6), 0.5f, 16); }
    }
    tk_caption(sy + 16, "HILLVILE TOWERS", PAL_WHITE, PAL_RAMP(RAMP_PASTEL_PINK, 1), 1);
    tk_rect(BX0 + 30, sy + 28, 3, top - sy - 28 + 1, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    tk_rect(BX1 - 33, sy + 28, 3, top - sy - 28 + 1, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    pheart(120, sy + 4.0f, 0.9f, PAINT_CHERRY);
    /* ground: lawn, bushes, the front door */
    int gy = bot + 4;
    static const uint8_t LG[3] = { PAL_RAMP(RAMP_GRASS, 2), PAL_RAMP(RAMP_GRASS, 3), PAL_RAMP(RAMP_GRASS, 4) };
    for (int y = gy; y < TH; y++)
        if (y >= 0) for (int x = 0; x < TW; x++) tk_fb[y][x] = qtone(LG, 3, 2.2f - (y - gy) * 0.05f, x, y);
    tk_rect(0, gy, TW, 2, PAL_RAMP(RAMP_GRASS, 5));
    /* stepping-stone path to the door, flowers, fireflies */
    for (int k = 0; k < 6; k++) {
        float px = 120 + (k & 1 ? 4 : -4), py = gy + 6 + k * 9;
        mk_ellipse(px, py, 7 + k * 0.6f, 2.5f + k * 0.3f);
        static const uint8_t PS[4] = { PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6), PAL_RAMP(RAMP_STONE, 7) };
        pillow(PS, PAL_RAMP(RAMP_STONE, 2), 1.5f, 0.4f);
    }
    for (int i = 0; i < 18; i++) {
        uint32_t q = tk_hash((uint32_t)i * 433u);
        int fx = (int)(q % TW), fy2 = gy + 8 + (int)((q >> 8) % 60);
        if (fx > 104 && fx < 136) continue;
        static const int FC[] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_WHITE, PAINT_LAVENDER };
        uint8_t c = PAL_PAINT(FC[i % 4], 1);
        tk_px(fx, fy2 + 1, PAL_RAMP(RAMP_GRASS, 5));
        tk_px(fx - 1, fy2, c); tk_px(fx + 1, fy2, c); tk_px(fx, fy2 - 1, c); tk_px(fx, fy2, PAL_PAINT(PAINT_LEMON, 2));
    }
    for (int i = 0; i < 9; i++) {
        float t = f * 0.03f + i * 1.7f;
        int fx = (int)(20 + fmodf(i * 53.0f + 18 * sinf(t), 210)), fy2 = gy - 6 + (int)(12 * sinf(t * 1.3f + i)) + (i % 3) * 8;
        if (((f + i * 7) % 24) < 16) {
            tint_ellipse((float)fx, (float)fy2, 3, 3, cmap(255, 250, 150, 6), 0.5f, 16);
            tk_px(fx, fy2, PAL_PAINT(PAINT_LEMON, 2));
        }
    }
    for (int i = 0; i < 8; i++) {
        float bx = i * 34.0f + 6;
        if (bx > 96 && bx < 144) continue;
        mk_ellipse(bx, (float)gy - 2, 12, 8);
        static const uint8_t BU[4] = { PAL_RAMP(RAMP_GRASS, 3), PAL_RAMP(RAMP_GRASS, 4), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6) };
        pillow(BU, PAL_RAMP(RAMP_GRASS, 2), 3.0f, 0.6f);
        tk_px((int)bx - 3, gy - 6, PAL_PAINT(PAINT_ROSE, 1)); tk_px((int)bx + 4, gy - 3, PAL_PAINT(PAINT_LEMON, 1));
    }
}

void scene_s06_life(int f, int n) {
    (void)n;
    /* camera tilt: hold, ease, hold, ease, hold (with gentle drift) */
    float c;
    if (f < 38) c = f * 0.12f;
    else if (f < 50) c = 4.6f + (112 - 4.6f) * tk_ease((f - 38) / 12.0f);
    else if (f < 98) c = 112 + (f - 50) * 0.08f;
    else if (f < 110) c = 115.8f + (206 - 115.8f) * tk_ease((f - 98) / 12.0f);
    else c = 206 + (f - 110) * 0.06f;
    camy = c;

    sky(f);
    building(f);
    for (int fl = 2; fl >= 0; fl--) {
        int y = floor_top(fl);
        if (y > TH || y + RH < 0) continue;
        for (int side = 0; side < 2; side++) {
            int rx = side ? RXR : LX;
            clip_begin();
            switch (fl * 2 + side) {
            case 4: room_friends(rx, y, f); break;
            case 5: room_sleep(rx, y, f); break;
            case 2: room_love(rx, y, f); break;
            case 3: room_cupcake(rx, y, f); break;
            case 0: room_party(rx, y, f); break;
            default: room_starer(rx, y, f); break;
            }
            clip_end(rx, y, RW, RH);
            tk_frame(rx - 1, y - 1, RW + 2, RH + 2, PAL_RAMP(RAMP_PASTEL_PINK, 1));
        }
    }

    /* captions */
    uint8_t oc = PAL_RAMP(RAMP_PASTEL_PINK, 1);
    if (f >= 4 && f < 38) bcap(146, "Make friends!", PAL_WHITE, oc, 2, f - 4);
    if (f >= 50 && f < 76) bcap(12, "Fall in love!", PAL_WHITE, oc, 2, f - 50);
    if (f >= 76 && f < 101) bcap(12, "Eat cupcakes!", PAL_WHITE, oc, 2, f - 76);
    if (f >= 114) bcap(146, "Ignore that one.", PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1), 2, f - 114);
    /* the POW rattles the camera */
    if (f >= 30 && f < 33) tk_post_shake((f & 1) ? 2 : -2, 1);
}

void scene_s06_life_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    tk_sfx(t0 + 7 * fr, SFX_POP, 0.7f);                       /* high five */
    tk_sfx(t0 + 4 * fr, SFX_SELECT, 0.35f);
    for (int i = 0; i < 6; i++)                               /* angry gibberish */
        tk_slide(t0 + (15 + i * 2) * fr, 0.06, 60 + (i * 7) % 10, 50 + (i * 5) % 8, TKV_SAW, 0.18f);
    tk_sfx(t0 + 22 * fr, SFX_GLITCH, 0.3f);                   /* glare lightning */
    tk_slide(t0 + 22 * fr, 0.25, 90, 70, TKV_SQ12, 0.12f);
    tk_sfx(t0 + 30 * fr, SFX_THUD, 0.8f);                     /* POW */
    tk_sfx(t0 + 30 * fr, SFX_POP, 0.6f);
    tk_note(t0 + 20 * fr, 0.1, 88, TKV_SQ50, 0.15f);          /* distant "beep." */
    tk_sfx(t0 + 38 * fr, SFX_WHOOSH, 0.3f);                   /* tilt */
    tk_sfx(t0 + 52 * fr, SFX_SPARKLE, 0.4f);
    tk_sfx(t0 + 60 * fr, SFX_POP, 0.8f);                      /* heart burst */
    tk_sfx(t0 + 61 * fr, SFX_CHOIR, 0.35f);
    for (int k = 0; k < 3; k++) tk_slide(t0 + (61 + k * 6) * fr, 0.06, 64, 52, TKV_SQ25, 0.3f);  /* nom */
    tk_sfx(t0 + 82 * fr, SFX_FANFARE, 0.4f);                  /* 10/10 */
    tk_sfx(t0 + 98 * fr, SFX_WHOOSH, 0.3f);                   /* tilt */
    /* the starer: nothing. no sound at all (the music should duck here)... then one tiny squeak as he waves */
    tk_sfx(t0 + 136 * fr, SFX_SQUEAK, 0.25f);
}
