/* s11_exorcism.c - the Church of St. Cuddles.   (7.5 s, 225 frames)
 *
 * f  0- 59  wide, slow parallax truck through a pastel church: rosette
 *           window (St. Cuddles, haloed teddy, in the middle pane), lancet
 *           windows with paw prints, light shafts with dust motes, candles,
 *           pews, a teddy on the altar. The pink party bunny from the vape
 *           shop floats over the aisle, possessed, twitching, leaking vape.
 *           Father Fuzz aims a spray bottle of holy water (like you'd use on
 *           a cat). pssht. pssht. "THE POWER OF CUDDLES COMPELS YOU!"
 * f 60- 91  close-up, rosette bokeh. Silence. Drip. Drip. The demon inside,
 *           tiny and deadpan: "ow. my everything."
 * f 92-135  EXORCISM FAILED x1 (stamp slam, sad kazoo). Attempt 2: Fuzz
 *           hugs its (floating) legs - hearts - x2. Attempt 3: the whole
 *           bucket. SPLASH. The bunny shakes itself dry like a wet dog,
 *           soaking Fuzz. x3. wah wah wahhh.
 * f136-155  The candles go out. "SOUL LOST." - and the little ghost really
 *           is lost: it looks left, looks right, checks a map (upside down)
 *           and wanders off the wrong way.
 * f156-224  "Lay to Rest?" [YES] [NO] "(only after 3 failed exorcisms.)"
 *           ...a suspicious extra row slides in. The cursor sneaks down.
 *           MONSTER MODE: ON. Everything turns red. Fuzz screams.
 *           "(or turn on MONSTER MODE, you monster.)"
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
#define F_SHOUT    8    /* Father Fuzz starts yelling                      */
#define F_CLOSE   60    /* cut: close-up on the possessed bunny            */
#define F_OW      70    /* "ow. my everything."                            */
#define F_FAIL1   92    /* EXORCISM FAILED x1                              */
#define F_HUG     95    /* attempt 2: the power of actual cuddles          */
#define F_FAIL2  107
#define F_BUCKET 109    /* attempt 3: the whole bucket                     */
#define F_DUMP   113
#define F_SPLASH 116
#define F_FAIL3  123
#define F_LOST   136    /* SOUL LOST.                                       */
#define F_MENU   156    /* Lay to Rest?                                     */
#define F_CAP1   160
#define F_MROW   174    /* a suspicious extra menu row appears              */
#define F_FLIP   186    /* MONSTER MODE: ON                                 */
#define F_CAP2   186

#define HZ 76           /* floor horizon */
#define GROUND 108      /* where the actors stand */
#define ZC 12.35f       /* depth of the actors */
static float camx;      /* camera truck in screen px at the actors' depth */

/* the pink party bunny from the vape shop. still wearing the party hat. */
static const TkChibi BUN = {
    PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_LEMON, 1),
    PAL_PAINT(PAINT_TEAL, 1), TKH_BALD, 0, 0, 0, TKT_BUNNY, 1, 1,
    TKEAR_BUNNY, PAL_PAINT(PAINT_WHITE, 2), 1 };

static int imod(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }

/* ---- the Church of St. Cuddles --------------------------------------------- */
static void wall(float ox) {
    static const uint8_t W[4] = { PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    const uint8_t *mort = cmap(110, 50, 120, 4);
    for (int y = 0; y < HZ + 4; y++) {
        float L = 0.15f + y / (float)HZ * 3.1f;
        for (int x = 0; x < TW; x++) {
            tk_fb[y][x] = qtone(W, 4, L, x, y);
            int wx = x - (int)ox;
            if (y % 10 == 9 || imod(wx + (y / 10) * 13, 26) == 0) tint_px(x, y, mort);
        }
    }
}
static void column(float x, float top, float bot, float w) {
    static const uint8_t C[4] = { PAL_RAMP(RAMP_STONE, 3), PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6), PAL_RAMP(RAMP_STONE, 7) };
    uint8_t ol = PAL_RAMP(RAMP_STONE, 2);
    for (int y = (int)top; y < (int)bot; y++)
        for (int i = 0; i < (int)w; i++) {
            float u = (i + 0.5f) / w * 2 - 1;
            float L = 2.3f - 1.4f * u - 1.3f * u * u;
            uint8_t c = qtone(C, 4, L, (int)x + i, y);
            if (i == 0 || i == (int)w - 1) c = ol;
            else if ((i % 4) == 2 && u > -0.6f) c = C[L > 2.2f ? 2 : 1];   /* fluting */
            tk_px((int)x + i, y, c);
        }
    /* capital + base: wider blocks */
    for (int k = 0; k < 2; k++) {
        float yy = k ? bot - 5 : top;
        mk_rrect(x - 3, yy, w + 6, 5, 1.5f);
        static const uint8_t CC[4] = { PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 6), PAL_RAMP(RAMP_STONE, 7), PAL_WHITE };
        pillow(CC, ol, 1.5f, 0.3f);
    }
}
static void rosette(float cx, float cy, float R, int f, int glow) {
    uint8_t lead = PAL_RAMP(RAMP_VAPE, 1);
    static const int P1[6] = { PAINT_BUBBLEGUM, PAINT_SKY, PAINT_MINT, PAINT_LEMON, PAINT_LAVENDER, PAINT_PEACH };
    static const int P2[4] = { PAINT_LAVENDER, PAINT_BUBBLEGUM, PAINT_SKY, PAINT_LEMON };
    if (glow) tint_ellipse(cx, cy, R + 16, R + 14, cmap(255, 236, 244, 7), 0.6f, 12);
    for (int y = (int)(cy - R - 5); y <= (int)(cy + R + 5); y++)
        for (int x = (int)(cx - R - 5); x <= (int)(cx + R + 5); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx * dx + dy * dy);
            if (d > R + 4) continue;
            if (d > R) {
                float lit = (-dx - dy) / (d + 0.01f);
                uint8_t c = d > R + 3 ? PAL_RAMP(RAMP_STONE, 2) : lit > 0.3f ? PAL_RAMP(RAMP_STONE, 7) :
                            lit > -0.3f ? PAL_RAMP(RAMP_STONE, 6) : PAL_RAMP(RAMP_STONE, 4);
                tk_px(x, y, c);
                continue;
            }
            float a = atan2f(dy, dx) + 3.14159f;
            int ring = d < R * 0.34f ? 0 : d < R * 0.67f ? 1 : 2;
            int nsec = ring == 1 ? 8 : 16;
            float sf = a / 6.28318f * nsec + (ring == 2 ? 0.5f : 0.0f);
            int sec = imod((int)floorf(sf), nsec);
            float fr = sf - floorf(sf), arcl = d * 6.28318f / nsec;
            int is_lead = fabsf(d - R * 0.34f) < 0.7f || fabsf(d - R * 0.67f) < 0.7f || d > R - 1.0f ||
                          (ring > 0 && fminf(fr, 1 - fr) * arcl < 0.6f);
            if (is_lead) { tk_px(x, y, lead); continue; }
            int paint = ring == 0 ? PAINT_SKY : ring == 1 ? P1[sec % 6] : P2[sec % 4];
            float lit = (-dx * 0.5f - dy * 0.8f) / R;
            uint32_t h = tk_hash((uint32_t)(sec * 31 + ring * 7));
            float tw = sinf(f * 0.15f + (h % 100) * 0.1f);
            int shade = lit + tw * 0.3f > 0.1f ? 2 : 1;
            /* soft inner glow toward each pane's centre */
            if (ring > 0 && fminf(fr, 1 - fr) * arcl > 2.2f && shade == 1 && ((x + y) & 1)) shade = 2;
            tk_px(x, y, glow ? PAL_PAINT(paint, shade) : PAL_PAINT(paint, 0));
        }
    /* St. Cuddles himself, haloed, in the middle pane */
    uint8_t br = glow ? PAL_PAINT(PAINT_CARAMEL, 2) : PAL_PAINT(PAINT_CARAMEL, 0);
    tk_circle((int)cx - 3, (int)cy - 3, 2, lead); tk_circle((int)cx + 3, (int)cy - 3, 2, lead);
    tk_circle((int)cx, (int)cy + 1, 5, lead);
    tk_circle((int)cx - 3, (int)cy - 3, 1, br); tk_circle((int)cx + 3, (int)cy - 3, 1, br);
    tk_circle((int)cx, (int)cy + 1, 4, br);
    tk_ellipse((int)cx, (int)cy + 3, 2, 1, glow ? PAL_PAINT(PAINT_HONEY, 2) : PAL_PAINT(PAINT_HONEY, 0));
    tk_px((int)cx - 2, (int)cy, lead); tk_px((int)cx + 2, (int)cy, lead); tk_px((int)cx, (int)cy + 2, lead);
    tk_rect((int)cx - 3, (int)cy - 7, 7, 1, glow ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_HONEY, 0));
}
static void lancet(float cx, float top, float w, float h, int f, int glow) {
    float a = w * 0.9f;
    for (int pass = 0; pass < 2; pass++) {
        float g = pass ? 0 : 2.5f;
        for (int y = (int)(top - g); y <= (int)(top + h + g); y++)
            for (int x = (int)(cx - w / 2 - g - 1); x <= (int)(cx + w / 2 + g + 1); x++) {
                float yy = y + 0.5f, hw = w / 2 + g;
                if (yy < top + a - g * 0.2f) {
                    float t = (top + a - yy) / (a + g);
                    if (t > 1) continue;
                    hw *= sqrtf(1 - t * t) * (1 - 0.3f * t);
                }
                if (fabsf(x + 0.5f - cx) > hw) continue;
                if (!pass) { mk_px(x, y); continue; }
                float u = (x + 0.5f - (cx - w / 2)) / w, v = (yy - top) / h;
                int lead = imod(x - (int)(cx - w / 2), 5) == 0 || (int)(v * 7) != (int)((yy - 1 - top) / h * 7);
                int pz = v < 0.33f ? PAINT_LAVENDER : v < 0.66f ? PAINT_SKY : PAINT_MINT;
                int shade = (u + v * 0.3f + 0.15f * sinf(f * 0.12f + v * 9) < 0.55f) ? 2 : 1;
                uint8_t c = lead ? PAL_RAMP(RAMP_VAPE, 1) : glow ? PAL_PAINT(pz, shade) : PAL_PAINT(pz, 0);
                tk_px(x, y, c);
            }
        if (!pass) {
            static const uint8_t ST[4] = { PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 6), PAL_RAMP(RAMP_STONE, 7), PAL_WHITE };
            pillow(ST, PAL_RAMP(RAMP_STONE, 2), 1.5f, 0.3f);
        }
    }
    /* a bear paw print in the glass: St. Cuddles was here */
    float px = cx, py = top + h * 0.58f;
    uint8_t pc = glow ? PAL_PAINT(PAINT_BUBBLEGUM, 2) : PAL_PAINT(PAINT_BUBBLEGUM, 0);
    tk_ellipse((int)px, (int)py + 2, 3, 2, pc);
    tk_px((int)px - 3, (int)py - 2, pc); tk_px((int)px, (int)py - 3, pc); tk_px((int)px + 3, (int)py - 2, pc);
    tk_rect((int)px - 4, (int)py - 2, 2, 2, pc); tk_rect((int)px - 1, (int)py - 4, 2, 2, pc); tk_rect((int)px + 3, (int)py - 2, 2, 2, pc);
}
/* perspective floor: pastel checker tiles + a raspberry carpet aisle */
static float floor_y(float z) { return HZ + 420.0f / z - 2; }
static void floor_draw(void) {
    float camw = camx * ZC / 60.0f;
    for (int y = HZ; y < TH; y++) {
        float d = (float)(y - HZ + 2), z = 420.0f / d;
        float haze = tk_clampf((z - 14) / 60.0f, 0, 1);
        for (int x = 0; x < TW; x++) {
            float wx = (x - 120) * z / 60.0f + camw;
            uint8_t c;
            float ax = fabsf(wx);
            if (ax < 6.0f) {
                c = ax > 5.2f ? PAL_RAMP(RAMP_CANDY, 5) : (imod((int)floorf(z * 1.2f), 2) ? PAL_RAMP(RAMP_PASTEL_PINK, 3) : PAL_RAMP(RAMP_PASTEL_PINK, 2));
                if (ax < 5.2f && ax > 4.6f) c = PAL_RAMP(RAMP_PASTEL_PINK, 1);
            } else {
                int ck = imod((int)floorf(wx / 3.2f) + (int)floorf(z / 3.2f), 2);
                c = ck ? PAL_RAMP(RAMP_PEACH, 7) : PAL_RAMP(RAMP_PASTEL_PINK, 6);
                if (z > 36) c = PAL_RAMP(RAMP_PASTEL_PINK, 5);
            }
            tk_fb[y][x] = c;
            if (haze > 0 && BY4[y & 3][x & 3] < (int)(haze * 10)) tk_fb[y][x] = PAL_RAMP(RAMP_VAPE, 5);
        }
    }
    /* the wall meets the floor: a soft contact shadow */
    tint_rect(0, HZ, TW, 2, cmap(60, 20, 80, 5), 16);
}
static void pews(void) {
    float camw = camx * ZC / 60.0f;
    static const float Z[] = { 58, 42, 31, 23, 17 };
    for (int k = 0; k < 5; k++) {
        float z = Z[k], fy = floor_y(z), ht = 150.0f / z, seat = 70.0f / z;
        for (int side = -1; side <= 1; side += 2) {
            float xin = 120 + (side * 8.5f - camw) * 60.0f / z, xout = 120 + (side * 40.0f - camw) * 60.0f / z;
            float x0 = fminf(xin, xout), x1 = fmaxf(xin, xout);
            /* back panel */
            for (int y = (int)(fy - ht - seat); y < (int)fy; y++)
                for (int x = (int)x0; x < (int)x1; x++) {
                    float v = (y - (fy - ht - seat)) / (ht + seat);
                    uint8_t c = v < 0.12f ? PAL_RAMP(RAMP_WOOD, 5) : v < 0.6f ? PAL_RAMP(RAMP_WOOD, 3) : PAL_RAMP(RAMP_WOOD, 2);
                    if (v > 0.6f && v < 0.68f) c = PAL_RAMP(RAMP_WOOD, 4);              /* seat lip */
                    tk_px(x, y, c);
                }
            tk_rect((int)x0, (int)(fy - ht - seat), (int)(x1 - x0), 1, PAL_RAMP(RAMP_WOOD, 6));
            /* the carved pew end facing the aisle (with a little heart) */
            float ex = side < 0 ? x1 : x0;
            int ew = (int)(10.0f / z * 6) + 2, eh = (int)(ht + seat + 3);
            int exl = side < 0 ? (int)ex - ew : (int)ex;
            mk_rrect((float)exl, fy - eh, (float)ew, (float)eh, ew * 0.45f);
            static const uint8_t PE[4] = { PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6) };
            pillow(PE, PAL_RAMP(RAMP_WOOD, 1), 1.5f, 0.3f);
            if (ew >= 6) {
                int hx = exl + ew / 2, hy = (int)(fy - eh * 0.6f);
                tk_px(hx - 1, hy, PAL_PAINT(PAINT_ROSE, 1)); tk_px(hx + 1, hy, PAL_PAINT(PAINT_ROSE, 1));
                tk_rect(hx - 1, hy + 1, 3, 1, PAL_PAINT(PAINT_ROSE, 1)); tk_px(hx, hy + 2, PAL_PAINT(PAINT_ROSE, 1));
            }
            tint_rect((int)x0, (int)fy, (int)(x1 - x0), 2, cmap(60, 20, 60, 6), 12);
        }
    }
}
/* the front pews (closer than the actors) frame the shot */
static void near_pews(void) {
    float camw = camx * ZC / 60.0f;
    static const float Z[] = { 7.0f, 5.0f };
    for (int k = 0; k < 2; k++) {
        float z = Z[k], fy = floor_y(z), ht = 150.0f / z, seat = 70.0f / z;
        for (int side = -1; side <= 1; side += 2) {
            float xin = 120 + (side * 8.5f - camw) * 60.0f / z, xout = 120 + (side * 60.0f - camw) * 60.0f / z;
            float x0 = fminf(xin, xout), x1 = fmaxf(xin, xout);
            float top = fy - ht - seat;
            mk_rrect(x0, top, x1 - x0, ht + seat, 3);
            static const uint8_t PW[4] = { PAL_RAMP(RAMP_WOOD, 1), PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 4) };
            pillow(PW, PAL_RAMP(RAMP_WOOD, 0), 3.0f, 0.4f);
            tk_rect((int)x0 + 2, (int)top + 1, (int)(x1 - x0) - 4, 1, PAL_RAMP(RAMP_WOOD, 5));
            /* carved end with a heart */
            float ex = side < 0 ? x1 - 9 : x0;
            mk_rrect(ex - 1, top - 4, 10, ht + seat + 4, 4.5f);
            static const uint8_t PE[4] = { PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 6) };
            pillow(PE, PAL_RAMP(RAMP_WOOD, 0), 2.0f, 0.4f);
            plush_heart(ex + 4, top + 9, 2.6f, 4.5f, PAINT_ROSE, PAL_PAINT(PAINT_CHERRY, 0));
        }
    }
}
static void petals(void) {
    float camw = camx * ZC / 60.0f;
    for (int i = 0; i < 14; i++) {
        uint32_t h = tk_hash((uint32_t)i * 313u + 9u);
        float z = 6 + (h % 300) / 10.0f, wx = ((int)((h >> 9) % 100) - 50) / 10.0f;
        float sx = 120 + (wx - camw) * 60.0f / z, sy = floor_y(z);
        int sz = z < 12 ? 2 : 1;
        uint8_t c = (i & 1) ? PAL_PAINT(PAINT_BUBBLEGUM, 2) : PAL_PAINT(PAINT_ROSE, 2);
        tk_rect((int)sx, (int)sy, sz + 1, sz, c);
        if (sz > 1) tk_px((int)sx + 1, (int)sy + 2, c);
    }
}
static void flame(float x, float y, float s, int f, int i) {
    float sway = sinf(f * 0.5f + i * 1.7f) * 0.6f + ((tk_hash((uint32_t)(f / 2 + i * 13)) & 3) == 0 ? 0.5f : 0);
    mk_drop(x + sway, y, 1.4f * s);
    static const uint8_t FL[4] = { PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_LAVA, 6), PAL_RAMP(RAMP_LAVA, 7), PAL_WHITE };
    pillow(FL, PAL_RAMP(RAMP_LAVA, 4), 1.2f, -0.6f);
    tk_px((int)(x + sway), (int)y, PAL_WHITE);
}
/* a candle on a gold stand; lit = 0 shows a smoke wisp instead */
static void candle(float x, float base, float h, float pole, int f, int i, int lit, int out_age) {
    float top = base - pole - h;
    if (lit) {
        float fl = (tk_hash((uint32_t)(f / 3 + i * 7)) % 3) * 0.6f;
        tint_ellipse(x, top - 4, 12 + fl, 13 + fl, cmap(255, 214, 150, 7), 0.75f, 12);
    }
    if (pole > 0) {
        tk_rect((int)x - 1, (int)(base - pole), 3, (int)pole, PAL_RAMP(RAMP_CANDY, 3));
        tk_rect((int)x - 1, (int)(base - pole), 1, (int)pole, PAL_RAMP(RAMP_CANDY, 6));
        tk_rect((int)x - 4, (int)(base - pole) - 1, 9, 2, PAL_RAMP(RAMP_CANDY, 4));
        tk_rect((int)x - 4, (int)(base - pole) - 1, 9, 1, PAL_RAMP(RAMP_CANDY, 6));
        mk_ellipse(x, base - 1, 4, 2);
        static const uint8_t GB[4] = { PAL_RAMP(RAMP_CANDY, 2), PAL_RAMP(RAMP_CANDY, 4), PAL_RAMP(RAMP_CANDY, 6), PAL_WHITE };
        pillow(GB, PAL_RAMP(RAMP_WOOD, 1), 1.0f, 0.2f);
    }
    tk_rect((int)x - 1, (int)top, 3, (int)h, PAL_UI_CREAM);
    tk_rect((int)x + 1, (int)top, 1, (int)h, PAL_RAMP(RAMP_PEACH, 6));
    tk_px((int)x - 1, (int)top, PAL_WHITE);
    tk_px((int)x - 2, (int)top + 2, PAL_UI_CREAM);                     /* a drip of wax */
    if (lit) flame(x, top - 2, 1.0f, f, i);
    else if (out_age >= 0 && out_age < 30) {
        for (int k = 0; k < 7; k++) {
            int yy = (int)top - 2 - k - out_age / 3;
            tk_px((int)(x + sinf(k * 0.8f + out_age * 0.3f) * 1.5f), yy, k < 4 ? PAL_GRAY_L : PAL_GRAY);
        }
    }
}
/* a soft shaft of light, from (x0,y0) w0 wide down to (x1,y1) w1 wide */
static void shaft(float x0, float y0, float x1, float y1, float w0, float w1, float lvl, int f, const uint8_t *m) {
    for (int y = (int)y0; y < (int)y1; y++) {
        float t = (y - y0) / (y1 - y0), c = x0 + (x1 - x0) * t, hw = (w0 + (w1 - w0) * t) * 0.5f;
        for (int x = (int)(c - hw); x <= (int)(c + hw); x++) {
            float e = 1 - fabsf(x + 0.5f - c) / hw;
            if (e <= 0) continue;
            float cov = lvl * fminf(1, e * 2.2f) * (1 - 0.6f * t) * (0.8f + 0.2f * sinf(f * 0.07f + x * 0.09f - y * 0.03f));
            /* solid soft core, dithered only at the very edge */
            if (cov >= lvl * 0.55f || BY4[y & 3][x & 3] < (int)(cov / lvl * 28)) tint_px(x, y, m);
        }
    }
}
static void motes(int f, float ox) {
    for (int i = 0; i < 26; i++) {
        uint32_t s = tk_hash((uint32_t)i * 977u + 5u);
        float x = 90 + (s % 120) + ox * 0.6f + 6 * sinf(f * 0.03f + i);
        float y = fmodf((float)((s >> 8) % 120) + f * 0.12f * (1 + (s % 3)), 120.0f) + 20;
        int ph = (f + (int)(s >> 20) % 40) % 40;
        if (ph < 26) tk_px((int)x, (int)y, ph < 13 ? PAL_WHITE : PAL_RAMP(RAMP_CANDY, 7));
    }
}
static void altar(float cx, float base, int f, int lit) {
    /* cloth-draped altar table */
    mk_rrect(cx - 26, base - 15, 52, 15, 2);
    static const uint8_t CL[4] = { PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE };
    pillow(CL, PAL_RAMP(RAMP_VAPE, 3), 2.0f, 0.4f);
    tk_rect((int)cx - 26, (int)base - 13, 52, 2, PAL_RAMP(RAMP_CANDY, 5));          /* gold trim */
    for (int i = 0; i < 13; i++) tk_px((int)cx - 25 + i * 4, (int)base - 11, PAL_RAMP(RAMP_CANDY, 4));
    plush_heart(cx, base - 2, 4.5f, 7, PAINT_ROSE, PAL_PAINT(PAINT_CHERRY, 0));
    /* the little patron teddy on the altar (haloed), plus two candles */
    float ty = base - 15;
    mk_ellipse(cx, ty - 4, 5, 4.5f);
    mk_ellipse(cx, ty - 11, 5, 4.5f);
    mk_ellipse(cx - 4, ty - 15, 1.8f, 1.8f); mk_ellipse(cx + 4, ty - 15, 1.8f, 1.8f);
    static const uint8_t TB[4] = { PAL_PAINT(PAINT_CARAMEL, 0), PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CARAMEL, 2), PAL_PAINT(PAINT_HONEY, 2) };
    pillow(TB, PAL_PAINT(PAINT_CHOCOLATE, 0), 2.0f, 0.3f);
    tk_ellipse((int)cx, (int)ty - 9, 2, 1, PAL_PAINT(PAINT_HONEY, 2));
    tk_px((int)cx - 2, (int)ty - 12, PAL_PAINT(PAINT_JET, 0)); tk_px((int)cx + 2, (int)ty - 12, PAL_PAINT(PAINT_JET, 0));
    tk_px((int)cx, (int)ty - 10, PAL_PAINT(PAINT_CHOCOLATE, 0));
    float hy = ty - 19 + sinf(f * 0.1f);
    tk_rect((int)cx - 3, (int)hy, 7, 1, PAL_UI_GOLD); tk_px((int)cx - 4, (int)hy + 1, PAL_UI_GOLD); tk_px((int)cx + 4, (int)hy + 1, PAL_UI_GOLD);
    candle(cx - 18, base - 15, 7, 0, f, 11, lit > 0, lit > 0 ? -1 : 10);
    candle(cx + 18, base - 15, 7, 0, f, 12, lit > 0, lit > 0 ? -1 : 10);
}
/* everything behind the actors */
static void church(int f, int lit, int glow) {
    float ow = -camx * 0.3f, oc = -camx * 0.45f;
    wall(ow);
    rosette(140 + ow, 33, 25, f, glow);
    lancet(64 + ow, 14, 16, 46, f, glow);
    lancet(216 + ow, 14, 16, 46, f, glow);
    column(96 + oc, 0, HZ + 4, 12);
    column(172 + oc, 0, HZ + 4, 12);
    column(14 + oc, 0, HZ + 8, 13);
    column(254 + oc, 0, HZ + 8, 13);
    /* vaulted ceiling shadow */
    tint_rect(0, 0, TW, 6, cmap(40, 10, 60, 6), 16);
    tint_rect(0, 6, TW, 6, cmap(40, 10, 60, 4), 10);
    floor_draw();
    altar(140 + ow * 1.2f, floor_y(64), f, lit >= 3);
    pews();
    petals();
    /* tall candle stands by the aisle */
    float camw = camx * ZC / 60.0f;
    for (int s = -1; s <= 1; s += 2) {
        float z = 26, x = 120 + (s * 10.0f - camw) * 60.0f / z;
        candle(x, floor_y(z), 8, 20, f, s + 5, lit >= (s < 0 ? 1 : 2), lit >= (s < 0 ? 1 : 2) ? -1 : f - F_LOST - (s < 0 ? 4 : 2));
    }
    if (glow) {
        const uint8_t *m = cmap(255, 246, 232, 4);
        shaft(140 + ow, 40, 116 + ow * 0.2f, 160, 26, 74, 6.5f, f, m);
        shaft(64 + ow, 44, 44, 150, 10, 32, 4.5f, f, m);
        shaft(216 + ow, 44, 236, 150, 10, 32, 4.5f, f, m);
        motes(f, ow);
    }
}

/* ---- props ------------------------------------------------------------------- */
static void spray_bottle(float px, float py, int squeeze, int wet) {
    /* body */
    mk_rrect(px - 4, py - 2, 9, 13, 3.0f);
    static const uint8_t B[4] = { PAL_RAMP(RAMP_SKY, 4), PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_WHITE };
    pillow(B, PAL_PAINT(PAINT_COBALT, 0), 2.0f, 0.3f);
    tk_rect((int)px - 3, (int)py + 5, 7, 4, PAL_PAINT(PAINT_SKY, 1));                 /* the holy water inside */
    tk_rect((int)px - 3, (int)py + 5, 7, 1, PAL_RAMP(RAMP_SKY, 7));
    uint8_t hp = PAL_PAINT(PAINT_BUBBLEGUM, 1);                                         /* heart label */
    tk_rect((int)px - 2, (int)py, 5, 4, PAL_WHITE);
    tk_px((int)px - 1, (int)py + 1, hp); tk_px((int)px + 1, (int)py + 1, hp); tk_px((int)px, (int)py + 2, hp);
    /* neck + trigger sprayer with a long nozzle */
    tk_rect((int)px - 1, (int)py - 4, 3, 3, PAL_GRAY_L);
    mk_rrect(px - 3, py - 9, 11, 5, 2.0f);
    static const uint8_t T[4] = { PAL_GRAY_L, PAL_WHITE, PAL_WHITE, PAL_WHITE };
    pillow(T, PAL_GRAY, 1.0f, 0.2f);
    tk_rect((int)px + 8, (int)py - 8, 3, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_px((int)px + 10, (int)py - 7, PAL_PAINT(PAINT_ROSE, 0));
    tk_rect((int)px + 4 - squeeze, (int)py - 4, 2, 5, PAL_GRAY);                        /* trigger */
    tk_rect((int)px + 4 - squeeze, (int)py - 4, 1, 5, PAL_WHITE);
    if (wet) tk_px((int)px + 11, (int)py - 5 + (wet & 1), PAL_RAMP(RAMP_SKY, 7));
}
static void paw_over(float x, float y, int S) {
    mk_ellipse(x, y, 1.9f * S, 1.9f * S);
    static const uint8_t PW[4] = { PAL_PAINT(PAINT_CARAMEL, 0), PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CARAMEL, 2), PAL_PAINT(PAINT_HONEY, 2) };
    pillow(PW, PAL_PAINT(PAINT_CHOCOLATE, 0), 1.5f, 0.3f);
    tk_ellipse((int)x, (int)y + 1, 1, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
}
/* a cone of holy mist from the nozzle to the target */
static void mist(float x0, float y0, float x1, float y1, int a, uint32_t seed) {
    if (a < 0 || a > 14) return;
    uint32_t s = tk_hash(seed);
    for (int i = 0; i < 30; i++) {
        float delay = tk_rand(&s, 40) / 10.0f, sp = 0.15f + tk_rand(&s, 70) / 1000.0f;
        float spread = (tk_rand(&s, 200) - 100) / 100.0f;
        float t = (a - delay) * sp;
        if (t < 0 || t > 1.1f) continue;
        float px = x0 + (x1 - x0) * t + spread * 3 * t, py = y0 + (y1 - y0) * t + spread * 9 * t + 3 * t * t;
        uint8_t c = (i & 3) ? PAL_RAMP(RAMP_SKY, 7) : PAL_WHITE;
        tk_px((int)px, (int)py, c);
        if (i % 4 == 0) tk_px((int)px + 1, (int)py, PAL_RAMP(RAMP_SKY, 6));
    }
    if (a < 3) { tk_star((int)x0 + 1, (int)y0, 2 - a / 2, PAL_WHITE); }
}
/* holy water hitting a demon: sizzle puffs */
static void steam(float x, float y, int a, uint32_t seed) {
    if (a < 0 || a > 20) return;
    uint32_t s = tk_hash(seed);
    for (int k = 0; k < 4; k++) {
        float px = x + tk_rand(&s, 16) - 8 + 2 * sinf(a * 0.3f + k), py = y - a * 0.9f - k * 3 + tk_rand(&s, 4);
        float r = 1.5f + a * 0.14f;
        tint_ellipse(px, py, r, r, cmap(255, 255, 255, 12), 0.5f, 16 - a * 3 / 4);
    }
}
static void bucket(float cx, float cy, float ang) {
    float ca = cosf(ang), sa = sinf(ang);
    float P[4][2] = { { -9, -8 }, { 9, -8 }, { 7, 8 }, { -7, 8 } };
    float Q[4][2];
    for (int i = 0; i < 4; i++) { Q[i][0] = cx + P[i][0] * ca - P[i][1] * sa; Q[i][1] = cy + P[i][0] * sa + P[i][1] * ca; }
    mk_tri(Q[0][0], Q[0][1], Q[1][0], Q[1][1], Q[2][0], Q[2][1]);
    mk_tri(Q[0][0], Q[0][1], Q[2][0], Q[2][1], Q[3][0], Q[3][1]);
    static const uint8_t WB[4] = { PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6) };
    pillow(WB, PAL_RAMP(RAMP_WOOD, 1), 1.5f, 0.3f);
    for (int k = 0; k < 2; k++) {               /* gold bands */
        float v = k ? 4.0f : -4.0f, hw = 8.4f - k * 1.2f;
        tk_line((int)(cx - hw * ca - v * sa), (int)(cy - hw * sa + v * ca), (int)(cx + hw * ca - v * sa), (int)(cy + hw * sa + v * ca), PAL_RAMP(RAMP_CANDY, 5));
    }
    /* the rim of water while upright */
    if (fabsf(ang) < 0.5f) tk_line((int)Q[0][0] + 1, (int)Q[0][1] + 1, (int)Q[1][0] - 1, (int)Q[1][1] + 1, PAL_RAMP(RAMP_SKY, 6));
    /* handle */
    float hx = cx + 0 * ca - (-14) * sa, hy = cy + 0 * sa + (-14) * ca;
    if (fabsf(ang) < 0.5f) {    /* sloshing */
        tk_px((int)(cx - 4), (int)(cy - 10), PAL_RAMP(RAMP_SKY, 7));
        tk_px((int)(cx + 5), (int)(cy - 11), PAL_RAMP(RAMP_SKY, 6));
    }
    tk_line((int)Q[0][0], (int)Q[0][1], (int)hx, (int)hy, PAL_GRAY);
    tk_line((int)Q[1][0], (int)Q[1][1], (int)hx, (int)hy, PAL_GRAY);
}
static void water_blob(float x, float y, float r) {
    mk_ellipse(x, y, r, r * 0.8f);
    mk_ellipse(x - r * 0.6f, y - r * 0.5f, r * 0.55f, r * 0.5f);
    mk_ellipse(x + r * 0.7f, y - r * 0.3f, r * 0.45f, r * 0.45f);
    static const uint8_t WR[4] = { PAL_RAMP(RAMP_SKY, 4), PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 7), PAL_WHITE };
    pillow(WR, PAL_PAINT(PAINT_COBALT, 1), 2.5f, 0.4f);
}
/* drops flying out radially (splash / dog-shake) */
static void droplets(float x, float y, int a, uint32_t seed, int n, float sp, float spread_y) {
    if (a < 0 || a > 20) return;
    uint32_t s = tk_hash(seed);
    for (int i = 0; i < n; i++) {
        float ang = tk_rand(&s, 628) / 100.0f, v = sp * (0.5f + tk_rand(&s, 50) / 100.0f);
        float px = x + cosf(ang) * v * a, py = y + sinf(ang) * v * a * spread_y + 0.18f * a * a;
        uint8_t c = (i & 1) ? PAL_RAMP(RAMP_SKY, 6) : PAL_RAMP(RAMP_SKY, 7);
        tk_px((int)px, (int)py, c);
        tk_px((int)(px - cosf(ang)), (int)(py - sinf(ang) * spread_y), PAL_RAMP(RAMP_SKY, 5));
        if (i % 3 == 0) tk_px((int)px + 1, (int)py, PAL_WHITE);
    }
}
static void soul(float x, float y, int f, int look, int map) {
    const float k = 1.6f;
    float wob = sinf(f * 0.3f);
    /* ghost bunny: round head, two tall ears, wavy little tail */
    mk_ellipse(x - 2.5f * k, y - 9 * k, 1.6f * k, 4.5f * k);
    mk_ellipse(x + 2.5f * k, y - 9 * k + wob, 1.6f * k, 4.5f * k);
    mk_ellipse(x, y, 6 * k, 5.5f * k);
    for (int i = 0; i < 7; i++) mk_ellipse(x + (sinf(i * 0.9f + f * 0.4f) * 1.5f + i * 0.5f) * k, y + (4 + i * 1.2f) * k, (4.5f - i * 0.6f) * k, 1.6f * k);
    static const uint8_t G[4] = { PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), PAL_WHITE };
    pillow(G, PAL_RAMP(RAMP_VAPE, 3), 2.5f, 0.3f);
    tk_rect((int)(x - 1.5f * k) - 3 + look, (int)y - 2, 2, 3, PAL_RAMP(RAMP_VAPE, 0));
    tk_rect((int)(x + 1.5f * k) + 1 + look, (int)y - 2, 2, 3, PAL_RAMP(RAMP_VAPE, 0));
    tk_px((int)(x - 1.5f * k) - 3 + look, (int)y - 2, PAL_WHITE);
    tk_px((int)(x + 1.5f * k) + 1 + look, (int)y - 2, PAL_WHITE);
    tk_ellipse((int)x + look, (int)y + 3, 1, 1, PAL_RAMP(RAMP_VAPE, 1));
    tint_ellipse(x - 6 * k + 2, y + 2, 2, 1, cmap(255, 150, 200, 8), 0.3f, 16);
    tint_ellipse(x + 6 * k - 2, y + 2, 2, 1, cmap(255, 150, 200, 8), 0.3f, 16);
    /* the party hat survived too */
    tk_tri((int)x - 3, (int)(y - 5 * k), (int)x + 3, (int)(y - 5 * k), (int)x + 1, (int)(y - 12 * k), PAL_PAINT(PAINT_BUBBLEGUM, 2));
    tk_line((int)x - 2, (int)(y - 7 * k), (int)x + 2, (int)(y - 7 * k), PAL_PAINT(PAINT_LEMON, 2));
    tk_line((int)x - 1, (int)(y - 9 * k), (int)x + 2, (int)(y - 9 * k), PAL_PAINT(PAINT_LEMON, 2));
    tk_px((int)x + 1, (int)(y - 12 * k) - 1, PAL_WHITE);
    if (map) {      /* holding a little map. upside down. */
        int mx = (int)(x + 6 * k), my = (int)y;
        tk_rect(mx, my, 11, 8, PAL_UI_CREAM);
        tk_frame(mx, my, 11, 8, PAL_RAMP(RAMP_PEACH, 4));
        tk_line(mx + 4, my, mx + 4, my + 7, PAL_RAMP(RAMP_PEACH, 6));
        tk_line(mx + 7, my, mx + 7, my + 7, PAL_RAMP(RAMP_PEACH, 6));
        tk_px(mx + 2, my + 5, PAL_PAINT(PAINT_CHERRY, 1));
        tk_line(mx + 3, my + 4, mx + 8, my + 2, PAL_PAINT(PAINT_SKY, 0));
    }
}

/* ---- big stamp + counter ------------------------------------------------------ */
static void fail_stamp(int f) {
    if (f < F_FAIL1) return;
    int a = f - F_FAIL1;
    static const float SL[] = { 2.4f, 1.6f, 0.92f, 1.04f, 1.0f };
    float s = a < 5 ? SL[a] : 1.0f;
    static const uint8_t RR[4] = { PAL_RAMP(RAMP_BLOOD, 3), PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 6), PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    puffy_text("EXORCISM FAILED", 2, 1, 1, 104, 20, s, s, -0.07f, RR, PAL_RAMP(RAMP_BLOOD, 1), 2, PAL_WHITE, 2.0f);
    /* the counter */
    int k = f >= F_FAIL3 ? 3 : f >= F_FAIL2 ? 2 : 1;
    int ka = f - (k == 3 ? F_FAIL3 : k == 2 ? F_FAIL2 : F_FAIL1);
    static const float SC[] = { 2.2f, 1.5f, 0.85f, 1.1f, 0.97f, 1.0f };
    float cs = ka < 6 ? SC[ka] : 1.0f;
    if (k == 3 && ka < 6) cs *= 1.2f;
    char buf[4] = { 'x', (char)('0' + k), 0, 0 };
    static const uint8_t CR[4] = { PAL_RAMP(RAMP_LAVA, 3), PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_LAVA, 6), PAL_RAMP(RAMP_LAVA, 7) };
    puffy_text(buf, 3, 2, 1, 214, 23 + (k == 3 && ka < 20 ? (ka & 1) : 0), cs, cs, 0.1f, CR, PAL_RAMP(RAMP_BLOOD, 1), 2, PAL_WHITE, 2.5f);
}

/* ---- the close-up -------------------------------------------------------------- */
static void closeup(int f) {
    int a = f - F_CLOSE;
    static const uint8_t D[4] = { PAL_RAMP(RAMP_VAPE, 0), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3) };
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float dx = (x - 120) / 150.0f, dy = (y - 60) / 120.0f;
            tk_fb[y][x] = qtone(D, 4, 3.2f - sqrtf(dx * dx + dy * dy) * 3.6f, x, y);
        }
    /* the rosette, way out of focus: a ring of coloured bokeh */
    static const int P[6] = { PAINT_BUBBLEGUM, PAINT_SKY, PAINT_MINT, PAINT_LEMON, PAINT_LAVENDER, PAINT_PEACH };
    for (int i = 0; i < 12; i++) {
        float an = i * 0.5236f + a * 0.004f;
        float bx = 66 + cosf(an) * 38 - a * 0.15f, by = 46 + sinf(an) * 34;
        uint8_t r, g, b;
        int pi = PAL_PAINT(P[i % 6], 1);
        r = TK_PAL[pi][0]; g = TK_PAL[pi][1]; b = TK_PAL[pi][2];
        tint_ellipse(bx, by, 12, 12, cmap(r, g, b, 12), 0.3f, 14);
        tint_ellipse(bx - 3, by - 3, 4, 4, cmap(255, 255, 255, 5), 0.5f, 10);
    }
    tint_ellipse(66 - a * 0.15f, 46, 14, 14, cmap(200, 230, 255, 9), 0.4f, 12);
    for (int i = 0; i < 5; i++) {                       /* candle bokeh */
        float bx = 20 + i * 52 + (i & 1) * 9 - a * 0.25f, by = 132 - (i & 1) * 18;
        float fl = (tk_hash((uint32_t)(f / 3 + i * 5)) % 3) * 0.7f;
        tint_ellipse(bx, by, 9 + fl, 9 + fl, cmap(255, 200, 120, 9), 0.4f, 13);
        tint_ellipse(bx, by, 3, 3, cmap(255, 250, 220, 12), 0.5f, 16);
    }
    /* possession aura */
    float bx = 118 - a * 0.1f, yf = 178 + 2.5f * sinf(f * 0.11f);
    tint_ellipse(bx, yf - 62, 62 + 3 * sinf(f * 0.2f), 64, cmap(150, 40, 200, 7), 0.6f, 12);
    tk_chibi((int)bx, (int)yf, &BUN, 3, TKE_POSSESSED, TKM_FLAT, f + 30, 0);
    /* soaked: wet shine streaks + drips + sizzle */
    float hx = CX(bx, 3, 12), hy = CY(yf, 3, 12.2f);
    for (int k = 0; k < 3; k++) tk_line((int)hx - 18 + k * 5, (int)hy - 14 + k, (int)hx - 15 + k * 5, (int)hy - 17 + k, PAL_WHITE);
    for (int k = 0; k < 3; k++) {
        static const float DX[3] = { -9, 2, 14 };
        int ph = (a + k * 9) % 20;
        float dx = hx + DX[k], dy = hy + 24 + (k == 1 ? 3 : 0) + (ph > 8 ? (ph - 8) * (ph - 8) * 0.35f : 0);
        float r = ph < 8 ? 0.6f + ph * 0.12f : 1.6f;
        mk_drop(dx, dy, r);
        static const uint8_t WD[4] = { PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7), PAL_WHITE };
        pillow(WD, PAL_PAINT(PAINT_COBALT, 1), 1.0f, 0.3f);
    }
    steam(hx - 8, hy - 26, a % 21, 7);
    steam(hx + 10, hy - 30, (a + 10) % 21, 8);
    /* "ow. my everything." - tiny, deadpan */
    if (f >= F_OW) {
        small_bubble(146, 92, 92, 26, 150, 116, PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 0));
        tk_text(153, 96, "ow.", PAL_RAMP(RAMP_VAPE, 6), 1);
        tk_text(153, 106, "my everything.", PAL_RAMP(RAMP_VAPE, 6), 1);
    }
    tk_post_scanlines();
}

/* ---- the main draw ------------------------------------------------------------- */
void scene_s11_exorcism(int f, int n) {
    (void)n;
    if (f >= F_CLOSE && f < F_FAIL1) { closeup(f); return; }

    camx = 8 - 16 * tk_ease(f / 225.0f);
    int lit = f < F_LOST ? 3 : f < F_LOST + 2 ? 2 : f < F_LOST + 4 ? 1 : 0;
    int glow = f < F_LOST;
    church(f, lit, glow);

    /* ---- the possessed bunny: floating, twitching ---- */
    float bx = 156 - camx, byf = GROUND - 22 + 3 * sinf(f * 0.11f);
    int twitch = (f % 46) < 7 && f < F_LOST;
    float shx = twitch ? ((f & 1) ? 2.0f : -2.0f) : 0;
    if (f >= F_SPLASH + 1 && f < F_FAIL3) shx = ((f - F_SPLASH) & 1) ? 3.0f : -3.0f;     /* dog-shake */
    int bmouth = TKM_FLAT;
    if (f >= F_FAIL3 && f < F_LOST) bmouth = TKM_FANGS;                                   /* smug */
    float floor_by = GROUND;
    float hgt = floor_by - byf;
    tint_ellipse(bx, floor_by, 15 - hgt * 0.2f, 3, cmap(60, 10, 80, 7), 0.5f, 16);
    if (f < F_LOST + 6)
        tint_ellipse(bx, byf - 30, 36 + 3 * sinf(f * 0.2f), 42, cmap(150, 40, 200, 5), 0.35f, 16);
    /* vape wisps rising off it */
    for (int k = 0; k < 6; k++) {
        int ph = (f + k * 11) % 40;
        float wx = bx - 14 + k * 6 + 3 * sinf(ph * 0.25f + k), wy = byf - 20 - ph * 0.9f;
        if (ph < 30) tint_ellipse(wx, wy, 2.5f + ph * 0.06f, 2.0f + ph * 0.05f, cmap(170, 90, 230, 9), 0.4f, 16);
    }

    /* ---- Father Fuzz ---- */
    float fx = 80 - camx, fyf = GROUND;
    int fexpr = TKE_ANGRY, fmouth = TKM_FLAT, fflip = 0;
    int tsk = f - F_SHOUT;
    if (tsk >= 0 && tsk < 34) fmouth = TKM_OPEN;
    if (f >= F_FAIL1 && f < F_HUG) { fexpr = TKE_SAD; fmouth = TKM_FROWN; }
    int hugging = 0;
    if (f >= F_HUG && f < F_FAIL2 + 1) {
        int ha = f - F_HUG;
        float k = tk_ease(ha / 4.0f);
        fx = (80 - camx) + ((bx - 17) - (80 - camx)) * k;
        fyf = GROUND - 14 * k - (ha >= 4 ? 2 * sinf(ha * 0.7f) : 4 * sinf(k * 3.14159f));   /* leaps up and dangles */
        fexpr = ha < 4 ? TKE_ANGRY : TKE_HAPPY;
        fmouth = ha < 4 ? TKM_OPEN : TKM_SMILE;
        hugging = ha >= 4;
    } else if (f >= F_FAIL2 + 1 && f < F_LOST) {
        int ra = f - F_FAIL2 - 1;
        float k = tk_ease(ra / 4.0f);
        fx = (bx - 17) + ((100 - camx) - (bx - 17)) * k;
        fyf = GROUND - (ra < 5 ? 14 * (1 - ra / 5.0f) + 6 * sinf(ra / 5.0f * 3.14159f) : 0);
        fexpr = f < F_SPLASH ? TKE_ANGRY : TKE_SAD;
        fmouth = f < F_SPLASH ? TKM_FLAT : TKM_FROWN;
    } else if (f >= F_LOST) {
        fx = 100 - camx;
        fexpr = f >= F_FLIP ? TKE_SHOCK : TKE_SAD;
        fmouth = f >= F_FLIP ? TKM_SCREAM : TKM_FROWN;
    }

    /* draw order: while hugging, Fuzz is behind the bunny with his paws around it */
    if (!hugging) tk_chibi((int)(bx + shx), (int)byf, &BUN, 2, TKE_POSSESSED, bmouth, f, 0);
    tk_chibi((int)fx, (int)fyf, &TK_FUZZ, 2, fexpr, fmouth, f + 17, fflip);
    /* the clerical collar */
    tk_rect((int)CX(fx, 2, 11), (int)CY(fyf, 2, 20.2f), 4, 2, PAL_WHITE);
    tk_px((int)CX(fx, 2, 11) + 3, (int)CY(fyf, 2, 20.2f) + 1, PAL_GRAY_L);
    if (hugging) {
        tk_chibi((int)(bx + shx), (int)byf, &BUN, 2, TKE_POSSESSED, bmouth, f, 0);
        /* paws wrapped around the floating bunny's tummy, feet dangling + hearts */
        paw_over(CX(bx, 2, 4.5f), CY(byf, 2, 24.5f), 2);
        paw_over(CX(bx, 2, 19.5f), CY(byf, 2, 25.5f), 2);
        int ha = f - F_HUG - 4;
        for (int k = 0; k < 3; k++) {
            int hh = ha - k * 3;
            if (hh < 0) continue;
            plush_heart(bx - 22 + k * 12 + 2 * sinf(hh * 0.5f), CY(byf, 2, 4) - hh * 1.6f, 3.0f, 5.5f, PAINT_BUBBLEGUM, PAL_PAINT(PAINT_ROSE, 0));
        }
        tk_text_outline((int)bx + 22, (int)CY(byf, 2, 10), "squeeze", PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_ROSE, 0), 1);
    }
    /* spray bottle (attempt 1) */
    if (f < F_HUG) {
        float px = CX(fx, 2, 18.6f) + 3, py = CY(fyf, 2, 25.0f) - 6;
        static const int SPR[4] = { 14, 24, 36, 46 };
        int sq = 0;
        for (int k = 0; k < 4; k++) if (f >= SPR[k] && f < SPR[k] + 3) sq = 1;
        spray_bottle(px, py, sq, f >= F_FAIL1 ? f : 0);
        paw_over(px - 2, py + 4, 2);
        for (int k = 0; k < 4; k++) {
            int sa = f - SPR[k];
            if (sa >= 0 && sa < 4) {
                float tx = bx - 8, ty = byf - 40, nx = px + 11, ny = py - 7;
                float ln = sqrtf((tx - nx) * (tx - nx) + (ty - ny) * (ty - ny)), qx = -(ty - ny) / ln, qy = (tx - nx) / ln;
                float reach = (sa + 1) / 4.0f, ex = nx + (tx - nx) * reach, ey = ny + (ty - ny) * reach;
                mk_tri(nx, ny, ex + qx * 7 * reach, ey + qy * 7 * reach, ex - qx * 7 * reach, ey - qy * 7 * reach);
                for (int yy = mby0; yy <= mby1 && mbx1 >= 0; yy++)
                    for (int xx = mbx0; xx <= mbx1; xx++)
                        if (mk[yy][xx] && BY4[yy & 3][xx & 3] < 9 - sa * 2) tint_px(xx, yy, cmap(220, 245, 255, 9));
                mk_reset();
            }
            if (sa >= 0 && sa < 7) {
                static const int OX[4] = { -4, 6, -2, 8 };
                tk_text_outline((int)px + OX[k], (int)py - 22 - sa / 2, "pssht", PAL_RAMP(RAMP_SKY, 7), PAL_PAINT(PAINT_COBALT, 0), 1);
            }
            mist(px + 11, py - 7, bx - 8, byf - 40, f - SPR[k], (uint32_t)(k * 77 + 3));
            steam(bx - 6, byf - 44, f - SPR[k] - 5, (uint32_t)(k + 40));
        }
    }
    /* the bucket (attempt 3) */
    if (f >= F_BUCKET && f < F_SPLASH + 6) {
        int ba = f - F_BUCKET;
        float lift = tk_ease(ba / 4.0f);
        float bkx = fx + 1, bky = CY(fyf, 2, 6) - 4 - 8 * lift;
        float ang = f < F_DUMP ? -0.1f * sinf(ba * 0.9f) : -2.1f * tk_ease((f - F_DUMP) / 3.0f);
        bucket(bkx, bky, ang);
        paw_over(bkx - 9, bky + 3, 2);
        paw_over(bkx + 9, bky + 3, 2);
    }
    if (f >= F_DUMP && f < F_SPLASH) {
        float t = (f - F_DUMP + 1) / (float)(F_SPLASH - F_DUMP);
        float sx = fx + 8, sy = CY(fyf, 2, 6) - 18;
        float tx = bx, ty = byf - 54;
        water_blob(sx + (tx - sx) * t, sy + (ty - sy) * t - 14 * sinf(t * 3.14159f), 6 + 4 * t);
    }
    if (f >= F_SPLASH && f < F_SPLASH + 20) {
        int sa = f - F_SPLASH;
        droplets(bx, byf - 50, sa, 991, 40, 2.4f, 0.8f);
        if (sa < 6) water_blob(bx, byf - 50 + sa * 3, 12 - sa);
    }
    if (f >= F_SPLASH + 1 && f < F_FAIL3 + 6) droplets(bx, byf - 30, (f - F_SPLASH - 1) % 7 + 2, (uint32_t)(f / 7 + 5), 14, 3.0f, 0.5f);
    /* everybody is soaked afterwards */
    if (f >= F_SPLASH + 2 && f < F_LOST + 20) {
        for (int k = 0; k < 4; k++) {
            int ph = (f + k * 5) % 16;
            float dx = (k < 2 ? fx : bx) - 8 + (k & 1) * 16, dy = (k < 2 ? CY(fyf, 2, 22) : byf - 8) + ph * ph * 0.12f;
            tk_px((int)dx, (int)dy, PAL_RAMP(RAMP_SKY, 7));
            tk_px((int)dx, (int)dy + 1, PAL_RAMP(RAMP_SKY, 5));
        }
    }

    near_pews();

    /* ---- SOUL LOST. ---- */
    if (f >= F_LOST) {
        int la = f - F_LOST;
        int dark = la < 8 ? 15 - la : 7;
        tk_post_darken(dark);
        tint_rect(0, 0, TW, TH, cmap(60, 30, 120, 6), 6);
        if (f < F_MENU + 8) {
            /* the soul pops out, looks around, consults the map, wanders off the wrong way */
            float sx = bx, sy = byf - 50 - tk_ease(la / 10.0f) * 16;
            int look = 0, map = la >= 12;
            if (la >= 6 && la < 9) look = -2;
            else if (la >= 9 && la < 12) look = 2;
            if (la >= 14) { sx -= (la - 14) * (la - 14) * 0.5f; sy += sinf(la * 0.4f) * 2; look = -2; }
            if (la >= 2) soul(sx, sy, f, look, map);
            if (la >= 6 && la < 16) tk_text_outline((int)sx + 10, (int)sy - 30 - ((la / 3) & 1), "?", PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1), 2);
        }
        static const uint8_t SR[4] = { PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 6), PAL_RAMP(RAMP_STONE, 7), PAL_WHITE };
        if (f < F_MENU) {
            static const float SL[] = { 0.3f, 1.3f, 0.92f, 1.05f, 1.0f };
            float s = la < 5 ? SL[la] : 1.0f;
            puffy_text("SOUL LOST.", 3, 2, 1, 120, 40, s, s, 0, SR, PAL_RAMP(RAMP_VAPE, 0), 2, 0, 2.5f);
        }
    }

    /* ---- the menu ---- */
    if (f >= F_MENU) {
        int ma = f - F_MENU;
        int on = f >= F_FLIP;
        if (on) {
            tk_post_hellify(f < F_FLIP + 2 ? 16 : 7);
        }
        static const float SC[] = { 0.15f, 0.55f, 1.12f, 0.95f, 1.03f, 1.0f };
        float s = ma < 6 ? SC[ma] : 1.0f;
        int mh0 = f >= F_MROW ? 70 : 52;
        if (f >= F_MROW && f < F_MROW + 4) mh0 = 52 + (f - F_MROW + 1) * 5;
        int w = (int)(156 * s), h = (int)(mh0 * s);
        int cx = 120, cy = 34 + mh0 / 2;
        tk_window(cx - w / 2, cy - h / 2, w, h, on ? TKW_HELL : TKW_CREAM);
        if (ma >= 3) {
            int top = cy - mh0 / 2;
            uint8_t tc = on ? PAL_UI_GOLD : PAL_UI_PINK_D;
            const char *title = "Lay to Rest?";
            tk_text(cx - tk_text_w(title, 2) / 2 + 1, top + 8, title, on ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_PASTEL_PINK, 5), 2);
            tk_text(cx - tk_text_w(title, 2) / 2, top + 7, title, tc, 2);
            /* YES / NO pills */
            for (int k = 0; k < 2; k++) {
                int bxp = cx - 50 + k * 58, byp = top + 28;
                tk_window(bxp, byp, 42, 14, on ? TKW_DARK : (k ? TKW_CUTE : TKW_MINT));
                const char *lb = k ? "NO" : "YES";
                tk_text(bxp + 21 - tk_text_w(lb, 1) / 2, byp + 4, lb, PAL_WHITE, 1);
            }
            /* the suspicious extra row */
            if (f >= F_MROW + 2) {
                int ry = top + 50;
                const char *mm = "MONSTER MODE";
                tk_text(cx - 66, ry + 2, mm, on ? PAL_UI_GOLD : PAL_RAMP(RAMP_BLOOD, 3), 1);
                /* toggle switch */
                float kx = on ? tk_ease((f - F_FLIP) / 3.0f) : 0;
                mk_rrect((float)(cx + 22), (float)ry, 26, 11, 5.5f);
                uint8_t r4[4] = { PAL_GRAY_D, PAL_GRAY, PAL_GRAY, PAL_GRAY_L };
                uint8_t r4on[4] = { PAL_RAMP(RAMP_BLOOD, 3), PAL_UI_RED, PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_LAVA, 6) };
                pillow(on ? r4on : r4, PAL_BLACK, 2.0f, 0.3f);
                mk_ellipse(cx + 27.5f + kx * 15, ry + 5.5f, 4.5f, 4.5f);
                static const uint8_t KN[4] = { PAL_GRAY, PAL_GRAY_L, PAL_WHITE, PAL_WHITE };
                pillow(KN, PAL_GRAY_D, 1.5f, 0.3f);
                tk_text(cx + 52, ry + 2, on ? "ON" : "OFF", on ? PAL_WHITE : PAL_GRAY, 1);
            }
            /* heart cursor: hovers YES... then sneaks down */
            float hx = cx - 58, hy = top + 35;
            if (f >= F_MROW + 4) {
                float k = tk_ease((f - F_MROW - 4) / 5.0f);
                hx = cx - 58 + (cx + 14 - (cx - 58)) * k;
                hy = top + 35 + (top + 56 - (top + 35)) * k;
            }
            hx += 1.5f * sinf(f * 0.4f);
            if (f >= F_FLIP - 2 && f < F_FLIP) hx += 2;               /* the click */
            plush_heart(hx, hy + 4, 3.2f, 6, on ? PAINT_CHERRY : PAINT_BUBBLEGUM, PAL_PAINT(PAINT_ROSE, 0));
        }
        uint8_t oc = PAL_RAMP(RAMP_VAPE, 0);
        if (f >= F_CAP1) bcap(120, "(only after 3 failed exorcisms.)", PAL_WHITE, oc, 1, (f - F_CAP1) * 2);
        if (f >= F_CAP2) bcap(140, "(or turn on MONSTER MODE,\nyou monster.)", on ? PAL_RAMP(RAMP_LAVA, 6) : PAL_WHITE, oc, 1, (f - F_CAP2) * 2);
        if (on && f < F_FLIP + 3) tk_post_flash(PAL_UI_RED, 10 - (f - F_FLIP) * 3);
        if (on && f < F_FLIP + 4) tk_post_shake((f & 1) ? 3 : -3, (f & 2) ? 2 : -2);
    }

    /* ---- UI over the action ---- */
    if (f >= F_SHOUT && f < F_CLOSE) {
        int t = f - F_SHOUT;
        tk_dialog("FATHER FUZZ", "THE POWER OF CUDDLES\nCOMPELS YOU!", t, TKW_CREAM);
        if (t >= 22 && t < 34 && ((t / 3) & 1)) {
            tk_text_outline(205, 104, "!", PAL_UI_RED, PAL_WHITE, 2);
            tk_text_outline(218, 100, "!", PAL_UI_RED, PAL_WHITE, 2);
        }
    }
    if (f >= F_FAIL1 && f < F_LOST) fail_stamp(f);

    /* slams */
    int sl = -1;
    if (f >= F_FAIL1 && f < F_FAIL1 + 3) sl = f - F_FAIL1;
    if (f >= F_FAIL2 && f < F_FAIL2 + 3) sl = f - F_FAIL2;
    if (f >= F_FAIL3 && f < F_FAIL3 + 5) sl = (f - F_FAIL3) / 2;
    if (f >= F_LOST && f < F_LOST + 3) sl = f - F_LOST;
    if (sl >= 0) tk_post_shake((f & 1) ? 3 - sl : sl - 3, (f & 1) ? -2 : 2);
    if (f >= F_SHOUT + 22 && f < F_SHOUT + 25) tk_post_shake((f & 1) ? 2 : -2, 0);
    if (f >= F_FAIL1 && f < F_FAIL1 + 3) tk_post_flash(PAL_WHITE, 6 - (f - F_FAIL1) * 2);
    if (f >= F_FAIL3 && f < F_FAIL3 + 3) tk_post_flash(PAL_UI_RED, 8 - (f - F_FAIL3) * 3);
    if (f < 2) tk_post_flash(PAL_WHITE, 8 - f * 4);
}

void scene_s11_exorcism_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    /* organ */
    tk_sfx(t0, SFX_CHOIR, 0.55f);
    tk_note(t0, 1.6, 41, TKV_TRI, 0.3f);
    tk_note(t0, 1.6, 48, TKV_TRI, 0.22f);
    /* Fuzz yells: GBA text blips */
    for (int i = 0; i < 12; i++) tk_note(t0 + (F_SHOUT + i * 2) * fr, 0.035, 72 + ((i * 5) % 7), TKV_SQ25, 0.16f);
    /* pssht pssht pssht pssht + sizzle */
    static const int SPR[4] = { 14, 24, 36, 46 };
    for (int k = 0; k < 4; k++) {
        tk_slide(t0 + SPR[k] * fr, 0.11, 112, 104, TKV_NOISE, 0.35f);
        tk_slide(t0 + (SPR[k] + 5) * fr, 0.2, 100, 108, TKV_NOISE, 0.12f);
    }
    /* close-up: silence... drip... drip */
    tk_note(t0 + (F_CLOSE + 3) * fr, 0.05, 91, TKV_SINE, 0.25f);
    tk_note(t0 + (F_CLOSE + 9) * fr, 0.05, 86, TKV_SINE, 0.2f);
    /* "ow. my everything." - a flat, low, unimpressed demon mumble */
    static const double OW[5] = { 0, 6, 10, 13, 16 };
    for (int i = 0; i < 5; i++) tk_slide(t0 + (F_OW + OW[i]) * fr, 0.09, 37, 35, TKV_SAW, 0.3f);
    /* fail #1: stamp + sad kazoo */
    tk_sfx(t0 + F_FAIL1 * fr, SFX_THUD, 0.9f);
    tk_slide(t0 + (F_FAIL1 + 1) * fr, 0.22, 62, 57, TKV_SAW, 0.3f);
    /* attempt 2: the hug */
    tk_sfx(t0 + (F_HUG + 4) * fr, SFX_SQUEAK, 0.5f);
    tk_sfx(t0 + (F_HUG + 7) * fr, SFX_SQUEAK, 0.4f);
    tk_sfx(t0 + (F_HUG + 5) * fr, SFX_SPARKLE, 0.3f);
    tk_sfx(t0 + F_FAIL2 * fr, SFX_THUD, 0.9f);
    tk_slide(t0 + (F_FAIL2 + 1) * fr, 0.22, 58, 53, TKV_SAW, 0.3f);
    /* attempt 3: the bucket */
    tk_sfx(t0 + F_DUMP * fr, SFX_WHOOSH, 0.5f);
    tk_sfx(t0 + F_SPLASH * fr, SFX_SPLASH, 0.9f);
    tk_sfx(t0 + F_SPLASH * fr, SFX_SPLAT, 0.3f);
    for (int i = 0; i < 6; i++) tk_slide(t0 + (F_SPLASH + 1 + i) * fr, 0.03, 98, 92, TKV_NOISE, 0.2f);  /* brrrrr */
    tk_sfx(t0 + F_FAIL3 * fr, SFX_THUD, 1.0f);
    tk_slide(t0 + (F_FAIL3 + 1) * fr, 0.18, 55, 52, TKV_SAW, 0.32f);
    tk_slide(t0 + (F_FAIL3 + 7) * fr, 0.18, 52, 48, TKV_SAW, 0.32f);
    tk_slide(t0 + (F_FAIL3 + 13) * fr, 0.55, 48, 40, TKV_SAW, 0.32f);       /* wah wah wahhhh */
    /* SOUL LOST: candles out, minor organ, a confused little ghost */
    for (int k = 0; k < 3; k++) tk_slide(t0 + (F_LOST + k * 2) * fr, 0.08, 96, 80, TKV_NOISE, 0.25f);
    tk_sfx(t0 + F_LOST * fr, SFX_THUD, 0.8f);
    tk_note(t0 + F_LOST * fr, 0.9, 45, TKV_TRI, 0.3f);
    tk_note(t0 + F_LOST * fr, 0.9, 48, TKV_TRI, 0.3f);
    tk_note(t0 + F_LOST * fr, 0.9, 52, TKV_TRI, 0.3f);
    tk_slide(t0 + (F_LOST + 3) * fr, 0.3, 76, 83, TKV_SINE, 0.18f);
    tk_slide(t0 + (F_LOST + 13) * fr, 0.25, 79, 74, TKV_SINE, 0.15f);       /* "...huh?" */
    /* menu */
    tk_sfx(t0 + F_MENU * fr, SFX_BOING, 0.5f);
    tk_sfx(t0 + (F_MENU + 3) * fr, SFX_POP, 0.4f);
    tk_note(t0 + (F_MROW + 1) * fr, 0.25, 40, TKV_SQ12, 0.25f);             /* dun... */
    tk_sfx(t0 + (F_MROW + 5) * fr, SFX_BLIP, 0.5f);
    tk_sfx(t0 + (F_MROW + 8) * fr, SFX_BLIP, 0.5f);
    tk_sfx(t0 + F_FLIP * fr, SFX_SELECT, 0.6f);
    tk_sfx(t0 + F_FLIP * fr, SFX_GLITCH, 0.5f);
    tk_sfx(t0 + (F_FLIP + 1) * fr, SFX_ROAR, 0.45f);
}
