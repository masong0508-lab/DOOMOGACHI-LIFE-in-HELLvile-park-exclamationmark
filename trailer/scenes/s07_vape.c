/* s07_vape.c - "Cloud Nine Vapor - GRAND OPENING!"   (5.5 s, 165 frames)
 *
 * Shot A  f  0- 39  wide, slow parallax truck: the brand-new vape shop right
 *                   next to the Wall (purple fog already oozing over it),
 *                   balloons, bunting, a "NO ANGELS" sandwich board. A pink
 *                   bunny in a party hat hops up to the counter. Vex waves.
 * Shot B  f 40- 91  Vex behind the counter (size 3). Employee of the Month:
 *                   a certain crowned cloud. "Try Brimstone Blast. It's
 *                   organic."  (*may contain souls). Wink. The pen slides
 *                   across the counter; a pink paw snatches it.
 * Shot C  f 92-119  close-up: a long inhale (stretch) ... hold it ... smoke
 *                   leaks out of the ears ... FWOOMP. Purple cloud eats the
 *                   whole screen.
 * Shot D  f120-164  it clears: grey fur, black eyes, a smile far too wide,
 *                   party hat still on. Head tilt. Creak. Silence.
 *                   "Everything is fine."  A balloon pops.
 */
#include "tk.h"
#include "pal.h"
#include <math.h>
#include <string.h>

#define CUT_B 40
#define CUT_C 92
#define CUT_D 120
#define KIT static __attribute__((unused))

/* ========================================================================= */
/* art kit: soft-shaded shapes, palette LUT lighting, layers                  */
/* ========================================================================= */
static const uint8_t BY[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

typedef struct { uint8_t o, d, b, l, h; } Sh;   /* outline, shadow, base, light, highlight */
#define SH_PAINT(p) ((Sh){ PAL_PAINT(p, 0), PAL_PAINT(p, 0), PAL_PAINT(p, 1), PAL_PAINT(p, 2), PAL_WHITE })
#define SH_RAMP(r, k) ((Sh){ PAL_RAMP(r, (k) - 3), PAL_RAMP(r, (k) - 1), PAL_RAMP(r, k), PAL_RAMP(r, (k) + 1), PAL_RAMP(r, (k) + 2) })

static uint8_t nearest_rgb(int r, int g, int b) {
    int best = 1; long bd = 1L << 40;
    for (int i = 1; i < 256; i++) {
        long rm = (TK_PAL[i][0] + r) / 2;
        long dr = TK_PAL[i][0] - r, dg = TK_PAL[i][1] - g, db = TK_PAL[i][2] - b;
        long d = (((512 + rm) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rm) * db * db) >> 8);
        if (d < bd) { bd = d; best = i; }
    }
    return (uint8_t)best;
}
enum { LM_MIX, LM_ADD, LM_MUL };
static void lut_make(uint8_t *out, int r, int g, int b, int mode, float k) {
    out[0] = 0;
    for (int i = 1; i < 256; i++) {
        float R = TK_PAL[i][0], G = TK_PAL[i][1], B = TK_PAL[i][2];
        if (mode == LM_MIX) { R += (r - R) * k; G += (g - G) * k; B += (b - B) * k; }
        else if (mode == LM_ADD) { R += r * k; G += g * k; B += b * k; }
        else { R *= 1 - k + k * r / 255.0f; G *= 1 - k + k * g / 255.0f; B *= 1 - k + k * b / 255.0f; }
        out[i] = nearest_rgb(R > 255 ? 255 : (int)R, G > 255 ? 255 : (int)G, B > 255 ? 255 : (int)B);
    }
}
/* LUT banks (built once) */
static uint8_t L_DARK[4][256];
static uint8_t L_SHADOW[256], L_DEEP[256];          /* cool purple shade      */
static uint8_t L_WARM[4][256];                      /* warm lamp light        */
static uint8_t L_VAPE[4][256];                      /* purple haze            */
static uint8_t L_PINKGLOW[3][256];                  /* neon glow              */
static uint8_t L_WHITE[3][256];                     /* bokeh / soft light     */
static uint8_t L_RED[3][256];                       /* creepy under-light     */
static uint8_t L_BOKEH[6][256];                     /* out-of-focus lights    */
static int luts_ready;
static void luts_init(void) {
    if (luts_ready) return;
    luts_ready = 1;
    lut_make(L_SHADOW, 70, 40, 120, LM_MUL, 0.45f);
    lut_make(L_DEEP, 40, 20, 80, LM_MUL, 0.75f);
    for (int i = 0; i < 4; i++) lut_make(L_DARK[i], 70, 40, 120, LM_MUL, 0.12f + 0.11f * i);
    for (int i = 0; i < 4; i++) lut_make(L_WARM[i], 255, 220, 150, LM_ADD, 0.07f + 0.07f * i);
    for (int i = 0; i < 4; i++) lut_make(L_VAPE[i], 170, 110, 220, LM_MIX, 0.18f + 0.16f * i);
    for (int i = 0; i < 3; i++) lut_make(L_PINKGLOW[i], 255, 90, 220, LM_ADD, 0.12f + 0.12f * i);
    for (int i = 0; i < 3; i++) lut_make(L_WHITE[i], 255, 250, 255, LM_MIX, 0.18f + 0.15f * i);
    for (int i = 0; i < 3; i++) lut_make(L_RED[i], 255, 40, 60, LM_MIX, 0.12f + 0.12f * i);
    lut_make(L_BOKEH[0], 255, 170, 220, LM_MIX, 0.35f);
    lut_make(L_BOKEH[1], 170, 255, 220, LM_MIX, 0.30f);
    lut_make(L_BOKEH[2], 255, 240, 150, LM_MIX, 0.35f);
    lut_make(L_BOKEH[3], 210, 170, 255, LM_MIX, 0.35f);
    lut_make(L_BOKEH[4], 255, 60, 80, LM_MIX, 0.30f);
    lut_make(L_BOKEH[5], 150, 60, 220, LM_MIX, 0.30f);
}
KIT void lut_px(int x, int y, const uint8_t *L) {
    if ((unsigned)x < TW && (unsigned)y < TH) tk_fb[y][x] = L[tk_fb[y][x]];
}
KIT void lut_rect(int x, int y, int w, int h, const uint8_t *L) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) lut_px(i, j, L);
}
/* radial light pool: n LUT levels (weak -> strong), dithered falloff */
KIT void light_pool(float cx, float cy, float rx, float ry, uint8_t (*L)[256], int n, float power) {
    for (int y = (int)(cy - ry); y <= (int)(cy + ry); y++)
        for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
            float fx = (x + .5f - cx) / rx, fy = (y + .5f - cy) / ry, d = fx * fx + fy * fy;
            if (d >= 1) continue;
            float i = powf(1 - d, power) * n;
            int lv = (int)(i + (BY[y & 3][x & 3] + .5f) / 16.0f);
            if (lv > n) lv = n;
            if (lv > 0) lut_px(x, y, L[lv - 1]);
        }
}
/* translucent ellipse (one LUT, soft dithered rim) */
KIT void haze(float cx, float cy, float rx, float ry, const uint8_t *L) {
    for (int y = (int)(cy - ry - 1); y <= (int)(cy + ry + 1); y++)
        for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++) {
            float fx = (x + .5f - cx) / rx, fy = (y + .5f - cy) / ry, d = fx * fx + fy * fy;
            if (d > 1) continue;
            if (d > 0.75f && BY[y & 3][x & 3] < (int)((d - 0.75f) * 64)) continue;
            lut_px(x, y, L);
        }
}
KIT void disc(float cx, float cy, float r, uint8_t c) {
    for (int y = (int)floorf(cy - r - 1); y <= (int)ceilf(cy + r + 1); y++)
        for (int x = (int)floorf(cx - r - 1); x <= (int)ceilf(cx + r + 1); x++) {
            float fx = x + .5f - cx, fy = y + .5f - cy;
            if (fx * fx + fy * fy <= r * r) tk_px(x, y, c);
        }
}
KIT void oval(float cx, float cy, float rx, float ry, uint8_t c) {
    for (int y = (int)floorf(cy - ry - 1); y <= (int)ceilf(cy + ry + 1); y++)
        for (int x = (int)floorf(cx - rx - 1); x <= (int)ceilf(cx + rx + 1); x++) {
            float fx = (x + .5f - cx) / rx, fy = (y + .5f - cy) / ry;
            if (fx * fx + fy * fy <= 1) tk_px(x, y, c);
        }
}
/* soft-shaded ball: light from the upper left, bounce light lower right,
 * coloured outline (skip with o = 0). */
KIT void ball(float cx, float cy, float rx, float ry, Sh s) {
    float orx = rx + (s.o ? 1 : 0), ory = ry + (s.o ? 1 : 0);
    for (int y = (int)floorf(cy - ory - 1); y <= (int)ceilf(cy + ory + 1); y++)
        for (int x = (int)floorf(cx - orx - 1); x <= (int)ceilf(cx + orx + 1); x++) {
            float fx = x + .5f - cx, fy = y + .5f - cy;
            if ((fx * fx) / (orx * orx) + (fy * fy) / (ory * ory) > 1) continue;
            float nx = fx / rx, ny = fy / ry, d = nx * nx + ny * ny;
            if (d > 1) { tk_px(x, y, s.o); continue; }
            float nz = sqrtf(1 - d);
            float L = -0.45f * nx - 0.6f * ny + 0.66f * nz;
            uint8_t c = s.b;
            if (L < 0.18f || (L < 0.26f && BY[y & 3][x & 3] < 6)) c = s.d;
            if (c == s.d && d > 0.6f && nx > 0.25f && ny > 0.1f) c = s.b;          /* bounce light */
            if (L > 0.78f || (L > 0.72f && BY[y & 3][x & 3] < 6)) c = s.l;
            if (L > 0.95f && s.h && rx > 3) c = s.h;
            tk_px(x, y, c);
        }
}
typedef struct { float x, y, r; } Puff;
/* a cloud = union of puffs with one shared outline */
KIT void cloud(const Puff *p, int n, Sh s) {
    if (s.o) for (int i = 0; i < n; i++) if (p[i].r > 0) disc(p[i].x, p[i].y, p[i].r + 1.2f, s.o);
    Sh t = s; t.o = 0;
    for (int i = 0; i < n; i++) if (p[i].r > 0) ball(p[i].x, p[i].y, p[i].r, p[i].r, t);
}
/* shaded capsule along a horizontal axis (pens, tubes) */
KIT void capsule(float x0, float x1, float cy, float r, Sh s) {
    for (int y = (int)floorf(cy - r - 2); y <= (int)ceilf(cy + r + 2); y++)
        for (int x = (int)floorf(x0 - r - 2); x <= (int)ceilf(x1 + r + 2); x++) {
            float fx = x + .5f, fy = y + .5f - cy;
            float qx = fx < x0 ? fx - x0 : fx > x1 ? fx - x1 : 0;
            float d = sqrtf(qx * qx + fy * fy);
            if (d > r + 1) continue;
            if (d > r) { tk_px(x, y, s.o); continue; }
            float v = fy / r;
            uint8_t c = v < -0.45f ? s.l : v > 0.4f ? s.d : s.b;
            if (v < -0.2f && v > -0.55f && s.h) c = s.h;
            tk_px(x, y, c);
        }
}
/* ---- layers: draw something into an empty frame, then blit it transformed */
static uint8_t SAVE_FB[TH][TW];
static uint8_t LAYER_A[TH][TW];
KIT void layer_begin(void) { memcpy(SAVE_FB, tk_fb, sizeof SAVE_FB); memset(tk_fb, 0, sizeof tk_fb); }
KIT void layer_end(uint8_t (*dst)[TW]) { memcpy(dst, tk_fb, sizeof tk_fb); memcpy(tk_fb, SAVE_FB, sizeof SAVE_FB); }
/* source pivot (px,py) lands on (dx,dy); scale sx,sy; rotate ang; only
 * source rows r0..r1-1 */
KIT void layer_draw(uint8_t (*L)[TW], float px, float py, float dx, float dy,
                    float sx, float sy, float ang, int r0, int r1) {
    float ca = cosf(-ang), sa = sinf(-ang);
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float ux = x + .5f - dx, uy = y + .5f - dy;
            float rx = ux * ca - uy * sa, ry = ux * sa + uy * ca;
            int u = (int)floorf(rx / sx + px), v = (int)floorf(ry / sy + py);
            if (u < 0 || u >= TW || v < r0 || v >= r1 || v < 0 || v >= TH) continue;
            uint8_t c = L[v][u];
            if (c) tk_fb[y][x] = c;
        }
}
/* hard bands with a 2-row dithered seam: clean pixel-art sky */
KIT void bands(int y0, int y1, const uint8_t *c, int n) {
    for (int y = y0; y < y1; y++) {
        float p = (float)(y - y0) / (y1 - y0) * n;
        int a = (int)p; if (a >= n) a = n - 1;
        float fr = p - a;
        for (int x = 0; x < TW; x++) {
            uint8_t col = c[a];
            if (a + 1 < n && fr > 0.86f && BY[y & 3][x & 3] < 8) col = c[a + 1];
            tk_fb[y][x] = col;
        }
    }
}
/* outlined text with a soft neon glow behind it */
KIT void neon_text(int x, int y, const char *s, uint8_t c, uint8_t oc, int scale, uint8_t (*G)[256]) {
    int w = tk_text_w(s, scale), h = 8 * scale;
    for (int j = -5; j < h + 5; j++)
        for (int i = -6; i < w + 6; i++) {
            float fx = (i - w / 2.0f) / (w / 2.0f + 6), fy = (j - h / 2.0f) / (h / 2.0f + 5);
            float d = fx * fx * fx * fx + fy * fy;
            int lv = d < 0.55f ? 1 : d < 1.0f ? (BY[(y + j) & 3][(x + i) & 3] < (int)((1.0f - d) * 30)) : 0;
            if (lv) lut_px(x + i, y + j, G[0]);
        }
    tk_text_outline(x, y, s, c, oc, scale);
}

/* ========================================================================= */
/* cast                                                                      */
/* ========================================================================= */
/* the customer: a pink bunny kid in a party hat */
static const TkChibi BUN = {
    PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_LEMON, 1),
    PAL_PAINT(PAINT_TEAL, 1), TKH_BALD, 0, 0, 0, TKT_BUNNY, 1, 1,
    TKEAR_BUNNY, PAL_PAINT(PAINT_WHITE, 2), 1 };

/* chibi design space -> screen: x' = x + (dx-12)*S, y' = yf + (dy-34.1)*S */
static float CX(int x, int S, float dx) { return x + (dx - 12.0f) * S; }
static float CY(int yf, int S, float dy) { return yf + (dy - 34.1f) * S; }

/* ========================================================================= */
/* props                                                                     */
/* ========================================================================= */
static void balloon(float ax, float ay, float x, float y, int pcol, int f) {
    /* string with a lazy S-curve */
    for (int i = 0; i <= 24; i++) {
        float t = i / 24.0f;
        float px = ax + (x - ax) * t + sinf(t * 6.0f + f * 0.1f) * 1.4f * (1 - t);
        float py = ay + (y + 8 - ay) * t;
        tk_px((int)px, (int)py, PAL_GRAY);
    }
    Sh s = SH_PAINT(pcol);
    ball(x, y, 5.5f, 6.5f, s);
    tk_tri((int)x - 1, (int)y + 8, (int)x + 1, (int)y + 8, (int)x, (int)y + 6, PAL_PAINT(pcol, 0));
    tk_px((int)x - 2, (int)y - 3, PAL_WHITE); tk_px((int)x - 3, (int)y - 2, PAL_WHITE);
}

/* the Brimstone Blast pen: glossy purple capsule, gold band, skull, ember */
static void pen_r(float x, float y, float len, int glow, float r) {
    capsule(x, x + len, y, r, (Sh){ PAL_PAINT(PAINT_GRAPE, 0), PAL_PAINT(PAINT_VAPE_PURPLE, 0),
                                   PAL_PAINT(PAINT_VAPE_PURPLE, 1), PAL_PAINT(PAINT_VAPE_PURPLE, 2), PAL_PAINT(PAINT_LAVENDER, 2) });
    int bh = (int)(r * 2 + 1);
    tk_rect((int)(x + len * 0.62f), (int)(y - r), (int)(r * 0.9f) + 1, bh, PAL_UI_GOLD);
    tk_px((int)(x + len * 0.62f), (int)(y - r), PAL_RAMP(RAMP_CANDY, 7));
    /* tiny skull logo */
    int sx = (int)(x + len * 0.3f), sy = (int)(y - r * 0.4f);
    tk_px(sx, sy, PAL_WHITE); tk_px(sx + 1, sy, PAL_WHITE); tk_px(sx, sy + 1, PAL_GRAY_L);
    if (r > 2.5f) { tk_px(sx + 2, sy, PAL_WHITE); tk_px(sx + 1, sy + 1, PAL_GRAY_D); tk_px(sx + 2, sy + 1, PAL_GRAY_L); }
    /* ember tip */
    uint8_t e = glow >= 2 ? PAL_RAMP(RAMP_LAVA, 7) : glow ? PAL_RAMP(RAMP_LAVA, 5) : PAL_RAMP(RAMP_LAVA, 3);
    tk_rect((int)(x + len) + 1, (int)(y - r * 0.5f), (int)(r * 0.9f) + 1, (int)(r + 1), e);
    if (glow) light_pool(x + len + 2, y, 6 + glow * 3 + r, 6 + glow * 3 + r, L_WARM, 3, 1.5f);
}
static void pen(float x, float y, float len, int glow) { pen_r(x, y, len, glow, 2.2f); }

/* the shop logo: a cloud wearing tiny devil horns */
static void logo_cloud(float x, float y, float s) {
    Puff p[3] = { { x - 4 * s, y + 1 * s, 3 * s }, { x + 4 * s, y + 1 * s, 3 * s }, { x, y - 1 * s, 4 * s } };
    tk_tri((int)(x - 4 * s), (int)(y - 3 * s), (int)(x - 1 * s), (int)(y - 4 * s), (int)(x - 4 * s), (int)(y - 8 * s), PAL_UI_RED);
    tk_tri((int)(x + 4 * s), (int)(y - 3 * s), (int)(x + 1 * s), (int)(y - 4 * s), (int)(x + 4 * s), (int)(y - 8 * s), PAL_UI_RED);
    cloud(p, 3, (Sh){ PAL_PAINT(PAINT_GRAPE, 0), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2), PAL_WHITE, PAL_WHITE });
    tk_px((int)(x - 1.5f * s), (int)(y), PAL_PAINT(PAINT_JET, 1));
    tk_px((int)(x + 1.5f * s), (int)(y), PAL_PAINT(PAINT_JET, 1));
}

/* ========================================================================= */
/* shot A: the wide establishing shot                                        */
/* ========================================================================= */
static void sky_and_far(int f, float cam) {
    static const uint8_t sky[] = { PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7), PAL_PAINT(PAINT_WHITE, 1),
                                   PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    bands(0, 112, sky, 5);
    /* drifting fluffy clouds (far parallax) */
    Sh cs = { 0, PAL_PAINT(PAINT_WHITE, 0), PAL_PAINT(PAINT_WHITE, 1), PAL_WHITE, PAL_WHITE };
    for (int i = 0; i < 4; i++) {
        float x = fmodf(i * 83.0f + f * 0.25f - cam * 0.15f + 400, 320) - 40, y = 14 + (i * 37 % 30);
        Puff p[4] = { { x - 9, y + 3, 5 }, { x, y, 7 }, { x + 9, y + 2, 5.5f }, { x + 3, y + 4, 5 } };
        cloud(p, 4, cs);
    }
    /* far pastel hills */
    for (int x = 0; x < TW; x++) {
        float wx = x + cam * 0.3f;
        int h1 = 86 + (int)(sinf(wx * 0.035f) * 6 + sinf(wx * 0.011f + 1) * 5);
        for (int y = h1; y < 112; y++) tk_px(x, y, y == h1 ? PAL_RAMP(RAMP_MINT, 7) : PAL_RAMP(RAMP_MINT, 6));
        int h2 = 98 + (int)(sinf(wx * 0.05f + 2) * 5 + sinf(wx * 0.017f) * 4);
        for (int y = h2; y < 112; y++) tk_px(x, y, y == h2 ? PAL_RAMP(RAMP_GRASS, 7) : PAL_RAMP(RAMP_GRASS, 6));
    }
    /* tiny lollipop trees on the far hill */
    for (int i = 0; i < 5; i++) {
        float x = fmodf(i * 61.0f - cam * 0.45f + 600, 300) - 30;
        int y = 99 + (int)(sinf((x + cam * 0.45f) * 0.05f + 2) * 5);
        tk_rect((int)x, y - 6, 1, 6, PAL_RAMP(RAMP_WOOD, 4));
        ball(x + 0.5f, y - 9, 3.5f, 3.5f, (Sh){ PAL_RAMP(RAMP_GRASS, 4), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 7), PAL_RAMP(RAMP_GRASS, 7) });
    }
}

/* the Wall to Hellvile, purple fog oozing over the top */
static void the_wall(float x0, int f) {
    int X = (int)x0;
    int top = 22;
    /* blocks */
    for (int y = top; y < 124; y++)
        for (int x = X; x < TW; x++) {
            int row = (y - top) / 9, by = (y - top) % 9;
            int bx = (x - X + (row & 1 ? 7 : 0)) % 15;
            uint8_t c = PAL_RAMP(RAMP_STONE, 3);
            uint32_t hh = tk_hash((uint32_t)(row * 131 + (x - X + (row & 1 ? 7 : 0)) / 15));
            if (hh % 5 == 0) c = PAL_RAMP(RAMP_STONE, 2);
            if (by == 0 || bx == 0) c = PAL_RAMP(RAMP_STONE, 1);
            else if (by == 1 || bx == 1) c = PAL_RAMP(RAMP_STONE, 4);
            else if (by == 8 || bx == 14) c = PAL_RAMP(RAMP_STONE, 2);
            tk_fb[y][x] = c;
        }
    /* shadow side + ambient occlusion at the base */
    lut_rect(X, top, 3, 102, L_SHADOW);
    for (int y = 110; y < 124; y++) for (int x = X; x < TW; x++) {
        int k = (int)((y - 110) / 14.0f * 3 + (BY[y & 3][x & 3] + .5f) / 16.0f);
        if (k > 0) lut_px(x, y, L_DARK[k - 1]);
    }
    /* crenellations */
    for (int x = X - 2; x < TW; x += 13) {
        tk_rect(x, top - 8, 8, 9, PAL_RAMP(RAMP_STONE, 3));
        tk_rect(x, top - 8, 8, 1, PAL_RAMP(RAMP_STONE, 5));
        tk_rect(x, top - 8, 1, 9, PAL_RAMP(RAMP_STONE, 4));
        tk_rect(x + 7, top - 8, 1, 9, PAL_RAMP(RAMP_STONE, 1));
    }
    tk_rect(X - 2, top, TW, 2, PAL_RAMP(RAMP_STONE, 5));
    /* "KEEP OUT" plank, nailed on crooked */
    {
        int sx = X + 14, sy = 50;
        for (int i = 0; i < 44; i++) {
            int yo = i / 14;
            tk_rect(sx + i, sy + yo - 1, 1, 18, PAL_RAMP(RAMP_WOOD, 1));
            tk_rect(sx + i, sy + yo, 1, 16, PAL_RAMP(RAMP_WOOD, 4));
            tk_px(sx + i, sy + yo, PAL_RAMP(RAMP_WOOD, 6));
            tk_px(sx + i, sy + yo + 15, PAL_RAMP(RAMP_WOOD, 3));
        }
        tk_text(sx + 4, sy + 1, "KEEP", PAL_UI_RED_D, 1);
        tk_text(sx + 15, sy + 8, "OUT", PAL_UI_RED_D, 1);
        tk_px(sx + 2, sy + 2, PAL_GRAY_L); tk_px(sx + 41, sy + 4, PAL_GRAY_L);
    }
    /* vapor oozing over the top and creeping down the face */
    Sh vs = { PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), 0 };
    Puff p[16];
    for (int i = 0; i < 16; i++) {
        uint32_t s = tk_hash((uint32_t)i * 31u + 7u);
        int life = 120, ph = (f + tk_rand(&s, life)) % life;
        float x = X - 4 + tk_rand(&s, TW - X + 10) - ph * 0.3f;
        float r = 3.0f + tk_rand(&s, 3) + sinf(ph / (float)life * 3.14159f) * 4.0f;
        p[i] = (Puff){ x, top - 9 + ph * 0.06f + tk_rand(&s, 4), r };
    }
    for (int i = 0; i < 6; i++) haze(X + 10 + i * 12 + sinf(f * 0.05f + i) * 4, top - 8, 10, 6, L_VAPE[1]);
    cloud(p, 16, vs);
    /* a lazy tendril sliding down the face */
    for (int i = 0; i < 5; i++) {
        float t = fmodf(f * 0.012f + i * 0.2f, 1.0f);
        haze(X + 20 + i * 7 + sinf(t * 6 + i) * 3, top + 4 + t * 50, 4 - t * 2, 3, L_VAPE[1]);
    }
}

static void bunting(float cam, int f, int y0, float sag) {
    static const int cols[] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_MINT, PAINT_SKY, PAINT_LAVENDER };
    float off = cam * 1.4f;
    for (int i = -2; i < 22; i++) {
        float x = i * 14 - fmodf(off, 14.0f);
        float t = (x + 20) / 280.0f;
        float y = y0 + sag * 4 * t * (1 - t) + sinf(f * 0.12f + i) * 0.6f;
        float x2 = x + 14, t2 = (x2 + 20) / 280.0f, y2 = y0 + sag * 4 * t2 * (1 - t2);
        tk_line((int)x, (int)y, (int)x2, (int)y2, PAL_GRAY_D);
        int k = ((i + (int)floorf(off / 14.0f)) % 5 + 5) % 5;
        float sw = sinf(f * 0.2f + i * 1.3f) * 1.5f;
        tk_tri((int)x + 2, (int)y + 1, (int)x + 11, (int)y + 1, (int)(x + 6.5f + sw), (int)y + 10, PAL_PAINT(cols[k], 0));
        tk_tri((int)x + 3, (int)y + 1, (int)x + 10, (int)y + 1, (int)(x + 6.5f + sw), (int)y + 8, PAL_PAINT(cols[k], 1));
        tk_line((int)x + 4, (int)y + 2, (int)(x + 6 + sw * 0.5f), (int)y + 5, PAL_PAINT(cols[k], 2));
    }
}

static void shelves(int x0, int x1, int y, int f, int big) {
    static const int jc[] = { PAINT_VAPE_PURPLE, PAINT_HELLFIRE, PAINT_SLIME, PAINT_MAGENTA, PAINT_CHERRY, PAINT_ECTOPLASM };
    int k = 0, step = big ? 9 : 6, hgt = big ? 10 : 6;
    for (int x = x0 + 3; x < x1 - step; x += step, k++) {
        int c = jc[(k * 5 + y) % 6];
        int glow = ((f / 4 + k * 3) % 23) < 2;
        tk_rect(x, y - hgt, step - 3, hgt, PAL_PAINT(c, 0));
        tk_rect(x + 1, y - hgt + 1, step - 5, hgt - 2, glow ? PAL_PAINT(c, 2) : PAL_PAINT(c, 1));
        tk_rect(x + 1, y - hgt + 1, 1, hgt - 3, PAL_PAINT(c, 2));
        tk_rect(x + 1, y - hgt - 2, step - 5, 2, PAL_GRAY_D);
        if (big) tk_px(x + 3, y - hgt + 4, PAL_WHITE);
    }
    tk_rect(x0, y, x1 - x0, 2, PAL_RAMP(RAMP_WOOD, 4));
    tk_rect(x0, y, x1 - x0, 1, PAL_RAMP(RAMP_WOOD, 6));
    tk_rect(x0, y + 2, x1 - x0, 1, PAL_RAMP(RAMP_WOOD, 1));
}

static void shopfront(int f, float cam, int vex_wave) {
    int o = -(int)cam;
    /* facade */
    for (int y = 36; y < 124; y++)
        for (int x = 16 + o; x < 180 + o; x++) {
            uint8_t c = ((x - o) % 7 == 0) ? PAL_RAMP(RAMP_VAPE, 5) : PAL_PAINT(PAINT_LAVENDER, 1);
            if ((x - o) % 7 == 1) c = PAL_PAINT(PAINT_LAVENDER, 2);
            tk_px(x, y, c);
        }
    tk_rect(15 + o, 36, 1, 88, PAL_PAINT(PAINT_GRAPE, 0));
    tk_rect(180 + o, 36, 1, 88, PAL_PAINT(PAINT_GRAPE, 0));
    lut_rect(172 + o, 36, 8, 88, L_SHADOW);
    /* window opening, interior with shelves + Vex */
    int wx0 = 56 + o, wx1 = 162 + o, wy0 = 58, wy1 = 100;
    tk_rect(wx0, wy0, wx1 - wx0, wy1 - wy0, PAL_RAMP(RAMP_VAPE, 1));
    for (int x = wx0; x < wx1; x += 10) tk_rect(x, wy0, 5, wy1 - wy0, PAL_RAMP(RAMP_VAPE, 2));
    shelves(wx0 + 2, wx0 + 40, 74, f, 0);
    shelves(wx0 + 2, wx0 + 40, 88, f + 9, 0);
    shelves(wx1 - 40, wx1 - 2, 74, f + 4, 0);
    shelves(wx1 - 40, wx1 - 2, 88, f + 13, 0);
    light_pool(124 + o, 64, 40, 34, L_PINKGLOW, 2, 1.2f);
    TkChibi vx = TK_VEX;
    layer_begin();
    tk_chibi(126 + o, 128, &vx, 2, vex_wave ? TKE_HAPPY : TKE_SMUG, TKM_SMILE, f, 0);
    layer_end(LAYER_A);
    layer_draw(LAYER_A, 0, 0, 0, 0, 1, 1, 0, 0, 100);
    if (vex_wave) {   /* a little paw wave over the counter */
        float a = sinf(f * 0.6f) * 3;
        ball(146 + o + a, 88 - fabsf(a) * 0.3f, 3.5f, 3.5f, SH_PAINT(PAINT_LAVENDER));
    }
    tk_frame(wx0 - 1, wy0 - 1, wx1 - wx0 + 2, wy1 - wy0 + 2, PAL_PAINT(PAINT_GRAPE, 0));
    /* counter */
    tk_rect(52 + o, 98, 114, 4, PAL_PAINT(PAINT_MAGENTA, 1));
    tk_rect(52 + o, 98, 114, 1, PAL_PAINT(PAINT_MAGENTA, 2));
    tk_rect(52 + o, 101, 114, 1, PAL_PAINT(PAINT_MAGENTA, 0));
    for (int y = 102; y < 124; y++)
        for (int x = 54 + o; x < 164 + o; x++)
            tk_px(x, y, (y < 104) ? PAL_RAMP(RAMP_VAPE, 1) : ((x - o) % 18 < 1 ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_VAPE, 3)));
    for (int i = 0; i < 6; i++) logo_cloud(64 + o + i * 18, 114, 0.8f);
    /* awning (scalloped, gently fluttering) */
    for (int i = 0; i < 17; i++) {
        int x = 14 + o + i * 10;
        int k = i & 1;
        uint8_t c = k ? PAL_WHITE : PAL_PAINT(PAINT_VAPE_PURPLE, 1);
        uint8_t cd = k ? PAL_PAINT(PAINT_WHITE, 0) : PAL_PAINT(PAINT_VAPE_PURPLE, 0);
        uint8_t cl = k ? PAL_WHITE : PAL_PAINT(PAINT_VAPE_PURPLE, 2);
        float fl = sinf(f * 0.18f + i * 0.9f) * 0.8f;
        tk_rect(x, 44, 10, 12, c);
        tk_rect(x, 44, 10, 2, cl);
        tk_rect(x, 53, 10, 3, cd);
        disc(x + 5, 56 + fl, 5, cd);
        disc(x + 5, 55.5f + fl, 4.4f, c);
    }
    tk_rect(14 + o, 43, 170, 1, PAL_PAINT(PAINT_GRAPE, 0));
    /* shadow under the awning */
    for (int y = 61; y < 68; y++) for (int x = 16 + o; x < 180 + o; x++) {
        int k = (int)((68 - y) / 7.0f * 3 + (BY[y & 3][x & 3] + .5f) / 16.0f);
        if (k > 0) lut_px(x, y, L_DARK[k - 1]);
    }
    /* sign board */
    {
        int x = 30 + o, y = 6, w = 142, h = 34;
        tk_rect(x + 2, y, w - 4, h, PAL_PAINT(PAINT_GRAPE, 0));
        tk_rect(x, y + 2, w, h - 4, PAL_PAINT(PAINT_GRAPE, 0));
        tk_rect(x + 2, y + 2, w - 4, h - 4, PAL_RAMP(RAMP_VAPE, 1));
        tk_rect(x + 3, y + 2, w - 6, 1, PAL_RAMP(RAMP_VAPE, 3));
        tk_rect(x + 20, y + h, 2, 4, PAL_GRAY_D); tk_rect(x + w - 22, y + h, 2, 4, PAL_GRAY_D);
        int flick = (f % 37) == 11 || (f % 37) == 13;
        logo_cloud(x + 18, y + 19, 1.3f);
        neon_text(x + 36, y + 4, "CLOUD NINE", flick ? PAL_PAINT(PAINT_LAVENDER, 0) : PAL_WHITE, PAL_RAMP(RAMP_VAPE, 0), 1, L_PINKGLOW);
        neon_text(x + 36, y + 14, "VAPOR", PAL_PAINT(PAINT_MAGENTA, 2), PAL_RAMP(RAMP_VAPE, 0), 2, L_PINKGLOW);
        tk_text(x + 102, y + 13, "est.\ntoday", PAL_RAMP(RAMP_VAPE, 5), 1);
    }
    /* GRAND OPENING ribbon across the awning */
    {
        int y = 46;
        float wob = sinf(f * 0.3f) * 1.0f;
        tk_tri(30 + o, y + 1, 42 + o, y + 6, 30 + o, y + 12, PAL_UI_RED_D);
        tk_tri(172 + o, y + 1, 160 + o, y + 6, 172 + o, y + 12, PAL_UI_RED_D);
        tk_rect(40 + o, y, 122, 12, PAL_UI_RED_D);
        tk_rect(41 + o, y + 1, 120, 10, PAL_UI_RED);
        tk_rect(41 + o, y + 1, 120, 1, PAL_PAINT(PAINT_CHERRY, 2));
        const char *g = "GRAND OPENING!";
        int tw = tk_text_w(g, 1);
        for (int i = 0; g[i]; i++) {
            char ch[2] = { g[i], 0 };
            int yy = y + 2 + (int)(sinf(f * 0.35f - i * 0.6f) * 1.2f + wob * 0);
            tk_text(101 + o - tw / 2 + i * 6, yy, ch, PAL_WHITE, 1);
        }
    }
    /* door with a round window and an OPEN neon */
    tk_rect(22 + o, 66, 26, 58, PAL_PAINT(PAINT_GRAPE, 0));
    tk_rect(23 + o, 67, 24, 57, PAL_PAINT(PAINT_GRAPE, 1));
    tk_rect(23 + o, 67, 2, 57, PAL_PAINT(PAINT_GRAPE, 2));
    disc(35 + o, 80, 7, PAL_PAINT(PAINT_GRAPE, 0));
    disc(35 + o, 80, 6, PAL_RAMP(RAMP_VAPE, 1));
    tk_px(32 + o, 77, PAL_RAMP(RAMP_VAPE, 4)); tk_px(33 + o, 76, PAL_RAMP(RAMP_VAPE, 4));
    {
        int on = (f % 29) != 7;
        tk_text(28 + o, 77, "OPEN", on ? PAL_PAINT(PAINT_MAGENTA, 2) : PAL_RAMP(RAMP_VAPE, 3), 1);
    }
    tk_heart(35 + o, 96, 1, PAL_PAINT(PAINT_MAGENTA, 1));
    disc(43 + o, 104, 1.5f, PAL_UI_GOLD);
}

static void shadow_blob(float cx, float cy, float rx, float ry);
static void sandwich_board(int x, int y, int f) {
    (void)f;
    tk_line(x + 3, y + 28, x + 7, y + 2, PAL_RAMP(RAMP_WOOD, 2));
    tk_line(x + 37, y + 28, x + 33, y + 2, PAL_RAMP(RAMP_WOOD, 2));
    tk_rect(x + 2, y, 36, 24, PAL_RAMP(RAMP_WOOD, 2));
    tk_rect(x + 3, y + 1, 34, 22, PAL_WHITE);
    tk_rect(x + 3, y + 20, 34, 3, PAL_PAINT(PAINT_WHITE, 0));
    tk_text(x + 14, y + 2, "NO", PAL_UI_RED, 1);
    tk_text(x + 3, y + 11, "ANGELS", PAL_GRAY_D, 1);
    /* crossed-out halo */
    oval(x + 30, y + 6, 3.5f, 1.8f, PAL_UI_GOLD);
    oval(x + 30, y + 6, 1.6f, 0.6f, PAL_WHITE);
    tk_line(x + 26, y + 9, x + 34, y + 3, PAL_UI_RED);
    /* soft contact shadow */
    shadow_blob(x + 20, y + 29, 20, 3);
}

static void ground(float cam) {
    int o = -(int)cam;
    for (int y = 124; y < 148; y++)
        for (int x = 0; x < TW; x++) {
            int tx = ((x - o) % 20 + 20) % 20, ty = (y - 124) % 12;
            uint8_t c = PAL_RAMP(RAMP_PEACH, 6);
            if (tx == 0 || ty == 0) c = PAL_RAMP(RAMP_PEACH, 5);
            else if (tx == 1 || ty == 1) c = PAL_RAMP(RAMP_PEACH, 7);
            tk_fb[y][x] = c;
        }
    tk_rect(0, 124, TW, 1, PAL_RAMP(RAMP_PEACH, 4));
    for (int y = 125; y < 131; y++) for (int x = 0; x < TW; x++) {
        int k = (int)((131 - y) / 6.0f * 2 + (BY[y & 3][x & 3] + .5f) / 16.0f);
        if (k > 0) lut_px(x, y, L_DARK[k - 1]);
    }
    tk_rect(0, 146, TW, 2, PAL_RAMP(RAMP_PEACH, 4));
    for (int y = 148; y < TH; y++)
        for (int x = 0; x < TW; x++) tk_fb[y][x] = y == 148 ? PAL_RAMP(RAMP_GRASS, 6) : PAL_RAMP(RAMP_GRASS, 5);
    for (int i = 0; i < 26; i++) {
        int x = ((i * 37 - (int)(cam * 1.2f)) % 260 + 260) % 260 - 10;
        int c = (i % 3 == 0) ? PAINT_BUBBLEGUM : (i % 3 == 1) ? PAINT_LEMON : PAINT_WHITE;
        tk_px(x, 151 + i % 5, PAL_RAMP(RAMP_GRASS, 4));
        tk_px(x, 150 + i % 5, PAL_PAINT(c, 1));
        tk_px(x + 1, 149 + i % 5, PAL_RAMP(RAMP_GRASS, 7));
    }
}

static void shadow_blob(float cx, float cy, float rx, float ry) {
    for (int y = (int)(cy - ry); y <= (int)(cy + ry); y++)
        for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
            float fx = (x + .5f - cx) / rx, fy = (y + .5f - cy) / ry, d = fx * fx + fy * fy;
            if (d >= 1) continue;
            int k = (int)((1 - d) * 3.0f + (BY[y & 3][x & 3] + .5f) / 16.0f);
            if (k > 0) lut_px(x, y, L_DARK[k > 4 ? 3 : k - 1]);
        }
}

static void shot_wide(int f) {
    float k = tk_ease(f / 40.0f);
    float cam = k * 18.0f;
    sky_and_far(f, cam);
    the_wall(186 - cam, f);
    ground(cam);
    shopfront(f, cam, f >= 30);
    sandwich_board(196 - (int)cam, 118, f);
    /* balloons tied to the awning corners */
    float bo = sinf(f * 0.12f) * 2;
    int o = -(int)cam;
    balloon(16 + o, 60, 8 + o + bo, 30, PAINT_BUBBLEGUM, f);
    balloon(16 + o, 60, 20 + o - bo, 24, PAINT_MINT, f + 9);
    balloon(16 + o, 60, 6 + o - bo * 0.5f, 40, PAINT_LEMON, f + 3);
    balloon(180 + o, 60, 186 + o + bo, 28, PAINT_LAVENDER, f + 5);
    balloon(180 + o, 60, 175 + o - bo, 36, PAINT_SKY, f + 2);
    tk_sparkles(101 + o, 26, f, 77, 52);
    tk_sparkles(40 + o, 50, f + 11, 78, 20);
    /* the customer: four hops in from the left, squash/stretch */
    float hk = tk_clampf(f / 28.0f, 0, 1);
    float x = -16 + (74 + 16) * (1 - (1 - hk) * (1 - hk));
    int ph = f % 7;
    float yo = 0, sx = 1, sy = 1;
    if (f < 28) {
        yo = -sinf(ph / 7.0f * 3.14159f) * 12;
        if (ph == 0) { sx = 1.22f; sy = 0.8f; }
        else if (ph < 3) { sx = 0.86f; sy = 1.16f; }
    } else if (f < 32) { float a = (f - 28) / 4.0f; sx = 1.2f - 0.2f * a; sy = 0.82f + 0.18f * a; }
    shadow_blob(x + cam * 0, 146, 13 - (-yo) * 0.4f, 3);
    layer_begin();
    tk_chibi(120, 140, &BUN, 2, f < 28 ? TKE_HAPPY : TKE_SPARKLE, f < 28 ? TKM_OPEN : TKM_SMILE, f * 2, 0);
    layer_end(LAYER_A);
    layer_draw(LAYER_A, 120, 140, x, 146 + yo, sx, sy, 0, 0, TH);
    if (f >= 30) {   /* little hearts: so excited */
        tk_hearts((int)x, 86, f - 30, 9);
    }
    bunting(cam, f, -6, 6);
    /* warm sunlight from the upper left */
    light_pool(30, 10, 120, 70, L_WARM, 2, 1.0f);
}

/* ========================================================================= */
/* shot B: Vex at the counter                                                */
/* ========================================================================= */
static void employee_of_the_month(int x, int y, int t) {
    /* gold frame */
    tk_rect(x, y, 56, 50, PAL_RAMP(RAMP_CANDY, 2));
    tk_rect(x + 1, y + 1, 54, 48, PAL_RAMP(RAMP_CANDY, 5));
    tk_rect(x + 1, y + 1, 54, 1, PAL_RAMP(RAMP_CANDY, 7));
    tk_rect(x + 3, y + 3, 50, 44, PAL_RAMP(RAMP_CANDY, 3));
    tk_rect(x + 4, y + 4, 48, 32, PAL_RAMP(RAMP_VAPE, 2));
    for (int i = 0; i < 48; i += 4) tk_rect(x + 4 + i, y + 4, 2, 32, PAL_RAMP(RAMP_VAPE, 1));
    /* a certain crowned cloud, looking smug */
    float cx = x + 28, cy = y + 24 + sinf(t * 0.15f);
    Puff p[5] = { { cx - 10, cy + 3, 6 }, { cx + 10, cy + 3, 6 }, { cx - 4, cy - 2, 7 }, { cx + 5, cy - 1, 7 }, { cx, cy + 4, 7 } };
    cloud(p, 5, (Sh){ PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7) });
    /* crown */
    tk_rect((int)cx - 7, (int)cy - 12, 15, 4, PAL_RAMP(RAMP_CANDY, 4));
    tk_tri((int)cx - 7, (int)cy - 12, (int)cx - 5, (int)cy - 17, (int)cx - 3, (int)cy - 12, PAL_RAMP(RAMP_CANDY, 4));
    tk_tri((int)cx - 2, (int)cy - 12, (int)cx, (int)cy - 18, (int)cx + 2, (int)cy - 12, PAL_RAMP(RAMP_CANDY, 4));
    tk_tri((int)cx + 3, (int)cy - 12, (int)cx + 5, (int)cy - 17, (int)cx + 7, (int)cy - 12, PAL_RAMP(RAMP_CANDY, 4));
    tk_rect((int)cx - 7, (int)cy - 12, 15, 1, PAL_RAMP(RAMP_CANDY, 7));
    tk_px((int)cx, (int)cy - 10, PAL_UI_RED);
    /* smug face */
    tk_line((int)cx - 7, (int)cy - 1, (int)cx - 3, (int)cy, PAL_RAMP(RAMP_VAPE, 0));
    tk_line((int)cx + 3, (int)cy, (int)cx + 7, (int)cy - 1, PAL_RAMP(RAMP_VAPE, 0));
    tk_px((int)cx - 5, (int)cy + 1, PAL_UI_RED); tk_px((int)cx + 5, (int)cy + 1, PAL_UI_RED);
    tk_line((int)cx - 5, (int)cy + 5, (int)cx + 5, (int)cy + 5, PAL_RAMP(RAMP_VAPE, 0));
    tk_px((int)cx - 6, (int)cy + 4, PAL_RAMP(RAMP_VAPE, 0)); tk_px((int)cx + 6, (int)cy + 4, PAL_RAMP(RAMP_VAPE, 0));
    tk_px((int)cx - 3, (int)cy + 6, PAL_WHITE); tk_px((int)cx + 3, (int)cy + 6, PAL_WHITE);
    /* plaque */
    tk_text(x + 6, y + 38, "EMPLOYEE", PAL_RAMP(RAMP_CANDY, 7), 1);
    tk_rect(x + 3, y + 3, 1, 44, PAL_RAMP(RAMP_CANDY, 6));
    /* glass glint sweeping */
    int g = (t * 3) % 120;
    if (g < 60) for (int j = 0; j < 32; j++) { tk_px(x + 4 + g - j / 2, y + 4 + j, PAL_RAMP(RAMP_VAPE, 4)); }
}

static void shot_vex(int t) {
    /* interior: striped wallpaper, vignette */
    for (int y = 0; y < 112; y++)
        for (int x = 0; x < TW; x++) {
            uint8_t c = (x % 16 < 8) ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_VAPE, 1);
            if (x % 16 == 8) c = PAL_RAMP(RAMP_VAPE, 3);
            tk_fb[y][x] = c;
        }
    /* wainscot */
    tk_rect(0, 92, TW, 20, PAL_PAINT(PAINT_GRAPE, 0));
    tk_rect(0, 92, TW, 1, PAL_PAINT(PAINT_GRAPE, 2));
    /* shelves of glowing flavours (left) */
    shelves(4, 84, 34, t, 1);
    shelves(4, 84, 56, t + 7, 1);
    shelves(4, 84, 78, t + 3, 1);
    light_pool(44, 52, 46, 40, L_PINKGLOW, 2, 1.4f);
    /* OPEN 6-6-6 neon */
    {
        int on = !((t % 23) == 3 || (t % 23) == 5 || (t % 41) == 20);
        if (on) neon_text(8, 6, "OPEN 6-6-6", PAL_PAINT(PAINT_MAGENTA, 2), PAL_PAINT(PAINT_MAGENTA, 0), 1, L_PINKGLOW);
        else tk_text_outline(8, 6, "OPEN 6-6-6", PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 1), 1);
    }
    /* employee of the month */
    employee_of_the_month(174, 8, t + CUT_B);
    tk_text_outline(166, 62, "OF THE MONTH", PAL_UI_GOLD, PAL_RAMP(RAMP_CANDY, 1), 1);
    /* warm key light on Vex */
    light_pool(118, 56, 64, 58, L_WARM, 3, 1.3f);
    /* drifting vapor wisps (translucent) */
    for (int i = 0; i < 6; i++) {
        float ph = fmodf(t * 0.6f + i * 47, 280) - 20;
        haze(ph, 30 + i * 13 + sinf(t * 0.05f + i) * 4, 14 + (i % 3) * 4, 5, L_VAPE[0]);
    }
    /* Vex (size 3): smug, then the wink */
    int wink = t >= 30 && t < 40;
    int vb = t + 300;
    tk_chibi(124, 130, &TK_VEX, 3, wink ? TKE_HAPPY : TKE_SMUG, wink ? TKM_CAT : TKM_SMILE, vb, 0);
    if (wink) {
        float a = (t - 30) / 10.0f;
        int r = (int)(6 * (1 - a)) + 2;
        tk_star(146, 46, r, PAL_WHITE);
        tk_star(146, 46, r / 2, PAL_UI_GOLD);
        tk_star(100, 40, r / 2, PAL_WHITE);
    }
    /* counter top (glossy) */
    tk_rect(0, 100, TW, 8, PAL_PAINT(PAINT_MAGENTA, 1));
    tk_rect(0, 100, TW, 2, PAL_PAINT(PAINT_MAGENTA, 2));
    tk_rect(0, 107, TW, 1, PAL_PAINT(PAINT_MAGENTA, 0));
    tk_rect(0, 108, TW, 52, PAL_RAMP(RAMP_VAPE, 1));
    for (int x = (t * 2) % 60 - 30; x < TW; x += 60) tk_rect(x, 102, 16, 1, PAL_PAINT(PAINT_MAGENTA, 2));
    /* the pen slides across the counter; Vex's paw pushes it */
    int k0 = t - 34;
    float k = tk_ease(k0 / 9.0f);
    float px = 150 - (150 - 48) * k;
    int grabbed = t >= 47;
    if (!grabbed) pen(px, 98, 16, 1);
    if (k0 >= -4 && k0 < 13) {
        float hx = px + 22 + (k0 < 0 ? -k0 * 2 : 0);
        ball(hx, 98, 5, 4, SH_PAINT(PAINT_LAVENDER));
        disc(hx - 2, 99, 1.2f, PAL_PAINT(PAINT_GRAPE, 2));
    }
    if (k0 > 0 && k0 < 9) for (int i = 0; i < 3; i++) tk_rect((int)px + 26 + i * 9, 95 + i * 2, 6, 1, PAL_WHITE);
    /* a pink paw snatches it */
    if (t >= 41) {
        float hx = -30 + 74 * tk_ease((t - 41) / 6.0f);
        if (grabbed) hx = 44 - 100 * tk_ease((t - 47) / 5.0f);
        if (grabbed) pen(hx + 2, 98, 16, 1);
        capsule(hx - 60, hx - 6, 99, 5, (Sh){ PAL_PAINT(PAINT_LEMON, 0), PAL_PAINT(PAINT_LEMON, 0), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_LEMON, 2), 0 });
        ball(hx, 97, 6, 5, SH_PAINT(PAINT_BUBBLEGUM));
        disc(hx + 2, 99, 1.4f, PAL_PAINT(PAINT_BUBBLEGUM, 0));
        if (grabbed && t < 52) for (int i = 0; i < 3; i++) tk_rect((int)hx + 14 + i * 8, 92 + i * 3, 6, 1, PAL_WHITE);
    }
    /* dialogue box slides up with a little overshoot */
    int dt = t - 2;
    float up = tk_bounce(dt / 9.0f);
    layer_begin();
    tk_dialog("VEX", "Try Brimstone Blast. It's organic.", (dt - 3) * 3 / 2, TKW_CUTE);   /* director: typed faster so the fine print gets read */
    if (t >= 22) {   /* the fine print */
        int a = t - 22;
        const char *fp = "*may contain souls";
        char buf[32]; int n = a * 3; if (n > 18) n = 18;
        memcpy(buf, fp, (size_t)n); buf[n] = 0;
        tk_text(12, TH - 46 + 22, buf, PAL_UI_PINK_D, 1);
    }
    layer_end(LAYER_A);
    if (dt >= 0) layer_draw(LAYER_A, 0, 0, 0, (1 - up) * 50, 1, 1, 0, 0, TH);
}

/* ========================================================================= */
/* shots C/D: close-ups                                                      */
/* ========================================================================= */
#define CS 4          /* close-up size */
#define CYF 194       /* close-up feet (off screen) */
static void bokeh_bg(int t, int creepy) {
    static const uint8_t g1[] = { PAL_PAINT(PAINT_LAVENDER, 2), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_PAINT(PAINT_LAVENDER, 1), PAL_RAMP(RAMP_VAPE, 5) };
    static const uint8_t g2[] = { PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 1) };
    bands(0, TH, creepy ? g2 : g1, 5);
    /* soft, out-of-focus awning stripes up top */
    for (int x = 0; x < TW; x++) {
        int k = ((x + t / 2) / 20) & 1;
        int h = 14 + (int)(sinf((x + t / 2) * 0.157f) * 3);
        for (int y = 0; y < h; y++) {
            if (y > h - 3 && BY[y & 3][x & 3] < 8) continue;
            lut_px(x, y, creepy ? (k ? L_VAPE[1] : L_DEEP) : (k ? L_WHITE[2] : L_VAPE[2]));
        }
    }
    /* the shop's glowing bottles, way out of focus: bokeh with bright rims */
    for (int i = 0; i < 16; i++) {
        uint32_t q = tk_hash((uint32_t)i * 977u + 3u);
        float x = (float)tk_rand(&q, 270) - 15 + sinf(t * 0.02f + i) * 4 - t * 0.1f;
        float y = 26 + (float)tk_rand(&q, 90) - t * 0.08f * (1 + i % 3);
        float r = 5 + tk_rand(&q, 11);
        int c = creepy ? 4 + (i & 1) : i % 4;
        haze(x, y, r, r, L_BOKEH[c]);
        if (!creepy && r > 9) haze(x - r * 0.25f, y - r * 0.25f, r * 0.45f, r * 0.45f, L_WHITE[0]);
    }
    /* the counter edge, blurred, at the bottom */
    for (int y = 132; y < TH; y++) for (int x = 0; x < TW; x++) lut_px(x, y, y < 134 ? (creepy ? L_RED[0] : L_BOKEH[0]) : (creepy ? L_DEEP : L_VAPE[1]));
}

/* ears: steam-kettle jets of vapor out of the ear tips */
static void ear_smoke(int x, int yf, int a) {
    Sh vs = { PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), 0 };
    float k = CS / 3.0f;
    for (int s = -1; s <= 1; s += 2) {
        float ex = x + s * 15.0f * k, ey = CY(yf, CS, -3.0f);
        Puff p[5];
        int n = a + 2 > 5 ? 5 : a + 2;
        float px = ex, py = ey;
        for (int i = 0; i < n; i++) {
            float r = (1.6f + i * 0.9f + a * 0.15f) * k;
            p[i] = (Puff){ px, py, r };
            px += s * r * 0.55f; py -= r * 1.1f;
        }
        cloud(p, n, vs);
    }
}

/* a cloud burst from (cx,cy): grows from 0 to `size` as k goes 0->1 */
static void fwoomp(float cx, float cy, float k, int age, uint32_t seed) {
    Sh vs = { PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), 0 };
    Puff p[40];
    uint32_t s = tk_hash(seed);
    for (int i = 0; i < 40; i++) {
        float a = tk_rand(&s, 6283) / 1000.0f + age * 0.008f * ((i & 1) ? 1 : -1);
        float d = (8 + tk_rand(&s, 150)) * k;
        float r = (10 + tk_rand(&s, 22)) * (0.35f + 0.9f * k);
        p[i] = (Puff){ cx + cosf(a) * d * 1.5f, cy + sinf(a) * d * 0.8f - age * 0.4f, r };
    }
    cloud(p, 40, vs);
}

/* that grin. far too wide. (drawn over the close-up chibi) */
static void grin(int x, int yf, int open) {
    float k = CS / 3.0f;
    float my = CY(yf, CS, 16.6f);
    int w = (int)(17 * k);
    for (int dx = -w; dx <= w; dx++) {
        float u = (float)dx / w;
        float top = my - 1 - u * u * 7 * k;
        float bot = my + (3 + open) * k - u * u * (9 + open) * k;
        if (bot < top + 1) bot = top + 1;
        for (int y = (int)floorf(top) - 1; y <= (int)ceilf(bot) + 1; y++) {
            uint8_t c;
            if (y < top || y > bot) c = PAL_PAINT(PAINT_JET, 0);
            else {
                int seam = ((dx + w) % 5) == 0;
                c = seam ? PAL_GRAY_L : PAL_WHITE;
                if (y > (top + bot) / 2 + 1 && open > 0) c = seam ? PAL_GRAY : PAL_WHITE;
                if (y < top + 1.5f && !seam) c = PAL_WHITE;
                if (y > bot - 1.5f) c = PAL_GRAY_L;
            }
            tk_px(x + dx, y, c);
        }
    }
    /* dimples */
    tk_px(x - w - 1, (int)(my - 8 * k), PAL_PAINT(PAINT_JET, 0));
    tk_px(x + w + 1, (int)(my - 8 * k), PAL_PAINT(PAINT_JET, 0));
    tk_px(x - w - 1, (int)(my - 8 * k) + 1, PAL_PAINT(PAINT_JET, 0));
    tk_px(x + w + 1, (int)(my - 8 * k) + 1, PAL_PAINT(PAINT_JET, 0));
}

static void shot_inhale(int t) {
    bokeh_bg(t + 60, 0);
    const int in0 = 3, hold0 = 13, ex0 = 18;
    float sx = 1, sy = 1;
    int expr = TKE_NORMAL, mouth = TKM_O;
    if (t >= in0 && t < hold0) {
        float k = tk_ease((t - in0) / (float)(hold0 - in0));
        sy = 1 + 0.12f * k; sx = 1 - 0.06f * k; expr = TKE_HAPPY; mouth = TKM_O;
    } else if (t >= hold0 && t < ex0) {
        sy = 1.03f; sx = 1.08f; expr = TKE_SHOCK; mouth = TKM_FLAT;
    } else if (t >= ex0) {
        float k = tk_bounce((t - ex0) / 8.0f);
        sy = 0.88f + 0.12f * k; sx = 1.16f - 0.16f * k; expr = TKE_SHOCK; mouth = TKM_SCREAM;
    }
    int x = 120, yf = CYF;
    if (t >= hold0 && t < ex0) x += (t & 1) ? 1 : -1;
    float k3 = CS / 3.0f;
    layer_begin();
    tk_chibi(120, CYF, &BUN, CS, expr, mouth, t + 40, 0);
    if (t >= hold0 && t < ex0) {     /* cheeks puffed up */
        Sh ck = { PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE, 0 };
        for (int s2 = -1; s2 <= 1; s2 += 2) {
            ball(CX(120, CS, 12 + s2 * 7.6f), CY(CYF, CS, 17.2f), 6.5f * k3, 5.5f * k3, ck);
            tk_px((int)CX(120, CS, 12 + s2 * 7.6f) - 3, (int)CY(CYF, CS, 17.2f) - 3, PAL_WHITE);
        }
    }
    layer_end(LAYER_A);
    layer_draw(LAYER_A, 120, CYF, x, yf, sx, sy, 0, 0, TH);
    float my = yf + (16.4f - 34.1f) * CS * sy;
    if (t < ex0) {
        /* pen + paw at the mouth */
        int glow = t >= in0 ? 2 : 1;
        pen_r(x + 6, my, 32, glow, 3.0f);
        ball(x + 10, my + 5, 8, 6.5f, SH_PAINT(PAINT_BUBBLEGUM));
        disc(x + 13, my + 8, 2, PAL_PAINT(PAINT_BUBBLEGUM, 0));
        if (t >= in0 && t < hold0) {   /* air + embers sucked in */
            for (int i = 0; i < 9; i++) {
                int ph = (t * 5 + i * 9) % 30;
                float r = 80 - ph * 2.4f, a = i * 0.7f - 0.6f;
                tk_rect((int)(x + 40 + cosf(a) * r), (int)(my + sinf(a) * r * 0.5f), 5 - ph / 7, 1,
                        (i & 1) ? PAL_WHITE : PAL_RAMP(RAMP_LAVA, 6));
            }
        }
    }
    if (t >= hold0 && t < ex0) ear_smoke(x, yf, t - hold0);
    if (t >= ex0) {
        int a = t - ex0;
        float k = tk_clampf(a / 6.0f, 0, 1);
        float d = 1 - (1 - k) * (1 - k);
        if (a >= 3) lut_rect(0, 0, TW, TH, L_VAPE[(a - 3) / 2 > 3 ? 3 : (a - 3) / 2]);
        fwoomp(x, my, 0.2f + d * 1.0f, a, 42);
        fwoomp(x, my, 0.08f + d * 0.45f, a, 43);
        if (a < 3) tk_post_flash(PAL_RAMP(RAMP_VAPE, 7), 12 - a * 4);
        if (a < 8) tk_post_shake((a & 1) ? 4 - a / 2 : -(4 - a / 2), (a & 2) ? 2 : -2);
    }
}

static void shot_possessed(int t) {
    bokeh_bg(t + 90, 1);
    /* the balloon (it pops) */
    const int pop = 38;
    float bo = sinf((t + CUT_D) * 0.12f) * 2;
    if (t < pop) balloon(30, 170, 30 + bo, 66, PAINT_BUBBLEGUM, t);
    else if (t < pop + 3) { tk_star(30, 66, 8 - (t - pop) * 2, PAL_WHITE); tk_star(30, 66, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1)); }
    else {
        int a = t - pop - 3;
        for (int i = 0; i < 24; i++) tk_px(30 + (int)(sinf(i * 0.6f) * 1.5f), 76 + a * a / 2 + i * 2, PAL_GRAY);
    }
    if (t >= pop) tk_confetti(30, 66, t - pop, 5);
    /* creepy under-light */
    light_pool(120, 176, 100, 76, L_RED, 2, 1.0f);
    /* the customer: still. too still. head tilts. */
    int x = 120, yf = CYF;
    layer_begin();
    tk_chibi(120, CYF, &BUN, CS, TKE_POSSESSED, TKM_FLAT, 0, 0);
    grin(120, CYF, t >= 12 ? 1 : 0);
    layer_end(LAYER_A);
    float tilt = 0;
    if (t >= 20) tilt = -0.14f * tk_ease((t - 20) / 6.0f);
    int neck = (int)CY(CYF, CS, 21.5f);
    layer_draw(LAYER_A, 120, CYF, x, yf, 1, 1, 0, neck, TH);                   /* body  */
    layer_draw(LAYER_A, 120, neck, x, neck, 1, 1, tilt, 0, neck);            /* head  */
    /* red glints in the eyes */
    if ((t / 3) % 6 == 0) {
        float ex = CX(120, CS, 12 - 3.9f), ey = CY(yf, CS, 12.6f);
        tk_star((int)ex, (int)ey, 3, PAL_WHITE);
    }
    /* wisps curling up off them */
    for (int i = 0; i < 6; i++) {
        int ph = (t * 2 + i * 11) % 44;
        haze(x - 36 + i * 14 + sinf(ph * 0.2f + i) * 4, 60 - ph, 3 + ph / 10, 2 + ph / 14, L_VAPE[2]);
    }
    /* the cloud parts */
    if (t < 12) {
        float k = t / 12.0f;
        int lvl = 3 - t / 3;
        if (lvl >= 0) lut_rect(0, 0, TW, TH, L_VAPE[lvl]);
        fwoomp(x, 100, 1.2f + k * k * 3.0f, 30 + t, 42);
        fwoomp(x, 100, 0.55f + k * k * 3.4f, 30 + t, 43);
        if (t < 7) {   /* two red eyes glowing in the fog first */
            for (int e = -1; e <= 1; e += 2) {
                int ex = (int)CX(120, CS, 12 + e * 3.9f), ey = (int)CY(yf, CS, 12.6f);
                tk_tri(ex - 7, ey - 2, ex + 7, ey - 2, ex, ey + 4, PAL_UI_RED_D);
                tk_tri(ex - 5, ey - 1, ex + 5, ey - 1, ex, ey + 3, PAL_UI_RED);
                tk_px(ex, ey, PAL_RAMP(RAMP_LAVA, 7));
            }
        }
    }
    tk_post_scanlines();
    /* beat of silence, then the line */
    if (t >= 14) {
        int a = t - 14;
        int dy = a < 4 ? (4 - a) * 2 : 0;
        tk_caption(144 + dy, "Everything is fine.", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0), 2);
    }
}

void scene_s07_vape(int f, int n) {
    (void)n;
    luts_init();
    if (f < CUT_B) shot_wide(f);
    else if (f < CUT_C) shot_vex(f - CUT_B);
    else if (f < CUT_D) shot_inhale(f - CUT_C);
    else shot_possessed(f - CUT_D);
}

void scene_s07_vape_audio(double t0, double dur) {
#define AT(fr) (t0 + (fr) / 30.0)
    (void)dur;
    /* grand opening: shop bell, party horn */
    tk_note(AT(0), 0.25, 88, TKV_SINE, 0.25f);
    tk_note(AT(3), 0.35, 93, TKV_SINE, 0.2f);
    tk_slide(AT(5), 0.12, 72, 79, TKV_SQ25, 0.18f);
    tk_slide(AT(10), 0.30, 79, 86, TKV_SQ25, 0.18f);
    /* four hops */
    for (int i = 0; i < 4; i++) tk_slide(AT(i * 7 + 1), 0.12, 62 + i * 2, 74 + i * 2, TKV_SQ50, 0.16f);
    tk_sfx(AT(29), SFX_SQUEAK, 0.3f);
    tk_sfx(AT(31), SFX_SPARKLE, 0.25f);
    /* Vex: smooth, low babble while the text types */
    {
        static const int m[] = { 50, 53, 48, 55, 52, 50, 57, 48, 53, 50, 47, 45 };
        for (int i = 0; i < 12; i++) tk_note(AT(CUT_B + 5 + i * 1.4), 0.05, m[i], TKV_SQ12, 0.18f);
    }
    tk_sfx(AT(CUT_B + 30), SFX_SPARKLE, 0.45f);        /* the wink */
    tk_note(AT(CUT_B + 30), 0.3, 96, TKV_SINE, 0.15f);
    tk_sfx(AT(CUT_B + 34), SFX_WHOOSH, 0.4f);          /* pen slides */
    tk_note(AT(CUT_B + 22), 0.04, 96, TKV_SQ25, 0.1f);  /* fine print blip */
    tk_sfx(AT(CUT_B + 47), SFX_SELECT, 0.3f);          /* snatched  */
    /* inhale ... hold ... FWOOMP */
    tk_slide(AT(CUT_C + 3), 0.34, 50, 88, TKV_NOISE, 0.3f);
    tk_slide(AT(CUT_C + 13), 0.16, 80, 82, TKV_SQ12, 0.08f);   /* strained hum */
    tk_slide(AT(CUT_C + 18), 0.9, 92, 30, TKV_NOISE, 0.9f);
    tk_slide(AT(CUT_C + 18), 0.7, 40, 24, TKV_SINE, 0.85f);
    tk_sfx(AT(CUT_C + 18), SFX_EXPLOSION, 0.45f);
    /* possession: one wrong low chord, then SILENCE for the caption */
    tk_note(AT(CUT_D + 1), 0.45, 37, TKV_SAW, 0.2f);
    tk_note(AT(CUT_D + 1), 0.45, 38, TKV_SAW, 0.16f);
    tk_sfx(AT(CUT_D + 2), SFX_GLITCH, 0.25f);
    tk_slide(AT(CUT_D + 20), 0.14, 34, 31, TKV_SQ12, 0.28f);  /* creeeak */
    tk_slide(AT(CUT_D + 23), 0.12, 31, 29, TKV_SQ12, 0.28f);
    tk_sfx(AT(CUT_D + 38), SFX_POP, 1.0f);             /* the balloon */
    tk_sfx(AT(CUT_D + 38), SFX_SPLASH, 0.2f);
#undef AT
}
