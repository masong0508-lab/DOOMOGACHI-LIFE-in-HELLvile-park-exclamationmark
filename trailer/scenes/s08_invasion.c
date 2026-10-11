/* s08_invasion.c - the record scratch.   (6.0 s, 180 frames)
 *
 * f  0- 13  Hellvile Park, Hillvile side: a party. Bunting, confetti, the
 *           whole plaza bopping. The hero holds the Doomogachi. (The bunny
 *           from the vape shop is in the crowd. Not bopping. Grinning.)
 * f 14      RECORD SCRATCH. Freeze frame. "!" over every head.
 * f 21      The Doomogachi: "beep."
 * f 28- 40  The sky CRACKS (glitch, shake). The sun sweats, then leaves.
 * f 34- 70  The camera tilts up as a purple vape cloud billows up from
 *           behind the Wall and becomes LORD VAPULA: eyes snap open (ROAR),
 *           teeth, and a crown drops on. Crooked. He has a vape, of course.
 * f 40-100  The pastel town rots into hell (hellify 0 -> 16, Vapula stays
 *           vape-purple). Everybody flees. The bunny: "it's fine."
 * f 92-179  Demons pour out of the cloud: implings, puff demons, hellhound
 *           pups. "THE DEMONS OF HELL" / "HAVE INVADED" slam in.
 * f138      "(wherever you are)" - and polaroids slap down to prove it:
 *           the bath, the bakery, your bed.
 */
#include "tk.h"
#include "pal.h"
#include <math.h>
#include <string.h>

#define F_SCRATCH 14
#define F_BEEP    21
#define F_CRACK   28
#define F_CLOUD   34
#define F_EYES    64
#define F_ROAR    68
#define F_CROWN   76
#define F_FLEE    80
#define F_POUR    90
#define F_CAP1    98
#define F_CAP2    104
#define F_CAP3    134
#define F_POLA    146
#define KIT static __attribute__((unused))

/* ========================================================================= */
/* art kit                                                                    */
/* ========================================================================= */
static const uint8_t BY[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
typedef struct { uint8_t o, d, b, l, h; } Sh;
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
static uint8_t L_DARK[4][256];
static uint8_t L_SHADOW[256], L_WARM[3][256], L_VAPE[4][256], L_LAVA[4][256], L_WHITE[3][256];
static uint8_t HELL[256];
static int luts_ready;
static void luts_init(void) {
    if (luts_ready) return;
    luts_ready = 1;
    lut_make(L_SHADOW, 70, 40, 120, LM_MUL, 0.45f);
    for (int i = 0; i < 4; i++) lut_make(L_DARK[i], 60, 30, 70, LM_MUL, 0.14f + 0.13f * i);
    for (int i = 0; i < 3; i++) lut_make(L_WARM[i], 255, 220, 150, LM_ADD, 0.08f + 0.08f * i);
    for (int i = 0; i < 4; i++) lut_make(L_VAPE[i], 170, 110, 220, LM_MIX, 0.18f + 0.16f * i);
    for (int i = 0; i < 4; i++) lut_make(L_LAVA[i], 255, 120, 30, LM_ADD, 0.12f + 0.12f * i);
    for (int i = 0; i < 3; i++) lut_make(L_WHITE[i], 255, 255, 255, LM_MIX, 0.2f + 0.2f * i);
    /* the rot: pastel -> dark hellish flesh, dead grass, crimson sky */
    for (int i = 0; i < 256; i++) {
        int r = TK_PAL[i][0], g = TK_PAL[i][1], b = TK_PAL[i][2];
        float l = (r * 3 + g * 6 + b) / 10.0f;
        float R, G, B;
        if (g > r + 20 && g >= b) { R = l * 0.62f; G = l * 0.42f; B = l * 0.16f; }          /* dead grass  */
        else if (b > r + 10) { R = l * 0.62f + 16; G = l * 0.08f; B = l * 0.16f + 8; }       /* blood sky   */
        else { R = l * 0.8f + 12; G = l * 0.26f; B = l * 0.24f; }                            /* flesh       */
        if (l > 225) { R = 248; G = 150; B = 50; }                                           /* hot spots   */
        HELL[i] = i == 0 ? 0 : nearest_rgb((int)R, (int)G, (int)B);
    }
}
KIT void lut_px(int x, int y, const uint8_t *L) {
    if ((unsigned)x < TW && (unsigned)y < TH) tk_fb[y][x] = L[tk_fb[y][x]];
}
KIT void lut_rect(int x, int y, int w, int h, const uint8_t *L) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) lut_px(i, j, L);
}
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
            if (c == s.d && d > 0.6f && nx > 0.25f && ny > 0.1f) c = s.b;
            if (L > 0.78f || (L > 0.72f && BY[y & 3][x & 3] < 6)) c = s.l;
            if (L > 0.95f && s.h && rx > 3) c = s.h;
            tk_px(x, y, c);
        }
}
typedef struct { float x, y, r; } Puff;
KIT void cloud(const Puff *p, int n, Sh s) {
    if (s.o) for (int i = 0; i < n; i++) if (p[i].r > 0) disc(p[i].x, p[i].y, p[i].r + 1.2f, s.o);
    Sh t = s; t.o = 0;
    for (int i = 0; i < n; i++) if (p[i].r > 0) ball(p[i].x, p[i].y, p[i].r, p[i].r, t);
}
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
static uint8_t SAVE_FB[TH][TW];
static uint8_t LAYER_A[TH][TW], LAYER_B[TH][TW];
KIT void layer_begin(void) { memcpy(SAVE_FB, tk_fb, sizeof SAVE_FB); memset(tk_fb, 0, sizeof tk_fb); }
KIT void layer_end(uint8_t (*dst)[TW]) { memcpy(dst, tk_fb, sizeof tk_fb); memcpy(tk_fb, SAVE_FB, sizeof SAVE_FB); }
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
KIT void bands(int y0, int y1, const uint8_t *c, int n) {
    for (int y = y0; y < y1; y++) {
        if (y < 0 || y >= TH) continue;
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
static void shadow_blob(float cx, float cy, float rx, float ry) {
    for (int y = (int)(cy - ry); y <= (int)(cy + ry); y++)
        for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
            float fx = (x + .5f - cx) / rx, fy = (y + .5f - cy) / ry, d = fx * fx + fy * fy;
            if (d >= 1) continue;
            int k = (int)((1 - d) * 3.0f + (BY[y & 3][x & 3] + .5f) / 16.0f);
            if (k > 0) lut_px(x, y, L_DARK[k > 4 ? 3 : k - 1]);
        }
}

/* ========================================================================= */
/* the world                                                                  */
/* ========================================================================= */
static int g_cy;        /* camera tilt: world shifts down by this much */

static void sky(int f) {
    static const uint8_t g[] = { PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7),
                                 PAL_PAINT(PAINT_WHITE, 1), PAL_RAMP(RAMP_PASTEL_PINK, 7) };
    bands(-40 + g_cy / 3, 70 + g_cy, g, 5);
    for (int y = 0; y < -40 + g_cy / 3; y++) for (int x = 0; x < TW; x++) tk_fb[y][x] = PAL_RAMP(RAMP_SKY, 5);
    /* drifting clouds */
    Sh cs = { 0, PAL_PAINT(PAINT_WHITE, 0), PAL_PAINT(PAINT_WHITE, 1), PAL_WHITE, PAL_WHITE };
    for (int i = 0; i < 4; i++) {
        float x = fmodf(i * 71.0f + f * 0.3f + 400, 300) - 30, y = 6 + (i * 29 % 26) + g_cy * 0.4f;
        Puff p[4] = { { x - 9, y + 3, 5 }, { x, y, 7 }, { x + 9, y + 2, 5.5f }, { x + 3, y + 4, 5 } };
        cloud(p, 4, cs);
    }
}

/* the sun: happy -> sweating -> leaves */
static void sun(int f) {
    float x = 30, y = 20 + g_cy * 0.4f;
    if (f >= 40) { float k = tk_ease((f - 40) / 14.0f); y += k * 110; x -= k * 10; }
    if (y > TH + 20) return;
    for (int i = 0; i < 8; i++) {
        float a = i * 0.785f + f * 0.03f;
        float r0 = 13, r1 = 17 + ((f / 4 + i) & 1);
        tk_line((int)(x + cosf(a) * r0), (int)(y + sinf(a) * r0), (int)(x + cosf(a) * r1), (int)(y + sinf(a) * r1), PAL_RAMP(RAMP_CANDY, 5));
    }
    light_pool(x, y, 30, 30, L_WARM, 2, 1.5f);
    ball(x, y, 10, 10, (Sh){ PAL_RAMP(RAMP_CANDY, 3), PAL_RAMP(RAMP_CANDY, 4), PAL_RAMP(RAMP_CANDY, 5), PAL_RAMP(RAMP_CANDY, 6), PAL_RAMP(RAMP_CANDY, 7) });
    uint8_t k = PAL_RAMP(RAMP_CANDY, 1);
    int X = (int)x, Y = (int)y;
    if (f < 24) {    /* ^ ^  and a smile */
        tk_px(X - 4, Y - 1, k); tk_px(X - 3, Y - 2, k); tk_px(X - 2, Y - 1, k);
        tk_px(X + 2, Y - 1, k); tk_px(X + 3, Y - 2, k); tk_px(X + 4, Y - 1, k);
        tk_line(X - 3, Y + 3, X - 1, Y + 4, k); tk_line(X - 1, Y + 4, X + 1, Y + 4, k); tk_line(X + 1, Y + 4, X + 3, Y + 3, k);
        tk_px(X - 6, Y + 2, PAL_PAINT(PAINT_BUBBLEGUM, 1)); tk_px(X + 6, Y + 2, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    } else {         /* o o  and a wobbly mouth, sweating */
        disc(X - 3.5f, Y - 1, 1.6f, PAL_WHITE); disc(X + 3.5f, Y - 1, 1.6f, PAL_WHITE);
        tk_px(X - 4, Y - 1, k); tk_px(X + 3, Y - 1, k);
        for (int i = -3; i <= 3; i++) tk_px(X + i, Y + 4 + ((i + f / 2) & 1), k);
        int d = (f / 2) % 8;
        disc(X + 9, Y - 4 + d, 1.3f, PAL_PAINT(PAINT_SKY, 2));
    }
}

/* Hellvile's skyline peeking over the Wall: far, hazy, a little spooky */
static void hellvile_skyline(int f, int crack) {
    int base = 40 + g_cy;
    for (int i = 0; i < 7; i++) {
        int x = 4 + i * 36 + (i * 13 % 11), w = 10 + (i * 7 % 6), h = 6 + (i * 11 % 9);
        uint8_t c = crack ? PAL_RAMP(RAMP_VAPE, 3) : PAL_PAINT(PAINT_LAVENDER, 1);
        uint8_t cl = crack ? PAL_RAMP(RAMP_VAPE, 4) : PAL_PAINT(PAINT_LAVENDER, 2);
        tk_rect(x, base - h, w, h, c);
        tk_tri(x - 1, base - h, x + w, base - h, x + w / 2, base - h - 6 - (i & 1) * 4, c);
        tk_rect(x, base - h, 1, h, cl);
        if ((i + f / 20) % 3) tk_px(x + w / 2, base - h + 3, crack ? PAL_RAMP(RAMP_LAVA, 6) : PAL_RAMP(RAMP_VAPE, 2));
    }
}

static void the_wall(void) {
    int top = 46 + g_cy, bot = 98 + g_cy;
    for (int y = top; y < bot; y++) {
        if (y < 0 || y >= TH) continue;
        for (int x = 0; x < TW; x++) {
            int row = (y - top) / 9, by = (y - top) % 9;
            int bx = (x + (row & 1 ? 7 : 0)) % 15;
            uint32_t hh = tk_hash((uint32_t)(row * 131 + (x + (row & 1 ? 7 : 0)) / 15));
            uint8_t c = (hh % 5 == 0) ? PAL_RAMP(RAMP_STONE, 2) : PAL_RAMP(RAMP_STONE, 3);
            if (by == 0 || bx == 0) c = PAL_RAMP(RAMP_STONE, 1);
            else if (by == 1 || bx == 1) c = PAL_RAMP(RAMP_STONE, 4);
            else if (by == 8 || bx == 14) c = PAL_RAMP(RAMP_STONE, 2);
            tk_fb[y][x] = c;
        }
    }
    for (int x = -3; x < TW; x += 13) {
        tk_rect(x, top - 8, 8, 9, PAL_RAMP(RAMP_STONE, 3));
        tk_rect(x, top - 8, 8, 1, PAL_RAMP(RAMP_STONE, 5));
        tk_rect(x, top - 8, 1, 9, PAL_RAMP(RAMP_STONE, 4));
        tk_rect(x + 7, top - 8, 1, 9, PAL_RAMP(RAMP_STONE, 1));
    }
    tk_rect(0, top, TW, 2, PAL_RAMP(RAMP_STONE, 5));
    /* ambient occlusion at the base */
    for (int y = bot - 8; y < bot; y++) for (int x = 0; x < TW; x++) {
        int k = (int)((y - bot + 8) / 8.0f * 2 + (BY[y & 3][x & 3] + .5f) / 16.0f);
        if (k > 0) lut_px(x, y, L_DARK[k - 1]);
    }
}

/* the sealed gate: arched double door, chains, padlock, and a polite note */
static void gate(int f, int shake) {
    int cx = 120 + shake, top = 56 + g_cy, bot = 98 + g_cy, hw = 22;
    for (int y = top - 6; y < bot; y++)
        for (int x = cx - hw - 4; x <= cx + hw + 4; x++) {
            float fx = (float)(x - cx) / (hw + 4), fy = (float)(y - top) / 10.0f;
            if (y < top && fx * fx + (fy) * (fy) > 1) continue;
            tk_px(x, y, PAL_RAMP(RAMP_STONE, 5));
        }
    for (int y = top - 3; y < bot; y++)
        for (int x = cx - hw; x <= cx + hw; x++) {
            float fx = (float)(x - cx) / hw, fy = (float)(y - top) / 8.0f;
            if (y < top && fx * fx + fy * fy > 1) continue;
            int plank = (x - cx + hw) / 7;
            uint8_t c = (plank & 1) ? PAL_RAMP(RAMP_WOOD, 2) : PAL_RAMP(RAMP_WOOD, 3);
            if ((x - cx + hw) % 7 == 0) c = PAL_RAMP(RAMP_WOOD, 1);
            if (x == cx) c = PAL_RAMP(RAMP_WOOD, 0);
            tk_px(x, y, c);
        }
    /* iron bands */
    for (int i = 0; i < 2; i++) { tk_rect(cx - hw, top + 8 + i * 18, hw * 2 + 1, 3, PAL_RAMP(RAMP_STEEL, 2)); tk_rect(cx - hw, top + 8 + i * 18, hw * 2 + 1, 1, PAL_RAMP(RAMP_STEEL, 4)); }
    /* chains: an X */
    for (int i = 0; i <= 20; i++) {
        float t = i / 20.0f;
        for (int s = -1; s <= 1; s += 2) {
            int x = (int)(cx - s * hw + s * 2 * hw * t), y = (int)(top + 2 + (bot - top - 6) * t + sinf(t * 3.14159f) * 3);
            uint8_t c = (i & 1) ? PAL_RAMP(RAMP_STEEL, 5) : PAL_RAMP(RAMP_STEEL, 3);
            tk_rect(x - 1, y - 1, 3, 2, c);
        }
    }
    /* padlock */
    int ly = top + 18 + (f >= F_CRACK && f < F_CRACK + 30 ? ((f / 2) & 1) : 0);
    tk_rect(cx - 4, ly - 5, 1, 5, PAL_RAMP(RAMP_STEEL, 4)); tk_rect(cx + 3, ly - 5, 1, 5, PAL_RAMP(RAMP_STEEL, 4)); tk_rect(cx - 4, ly - 6, 8, 1, PAL_RAMP(RAMP_STEEL, 4));
    tk_rect(cx - 6, ly, 13, 10, PAL_RAMP(RAMP_CANDY, 2));
    tk_rect(cx - 5, ly + 1, 11, 8, PAL_RAMP(RAMP_CANDY, 4));
    tk_rect(cx - 5, ly + 1, 11, 2, PAL_RAMP(RAMP_CANDY, 6));
    tk_rect(cx, ly + 4, 1, 3, PAL_RAMP(RAMP_CANDY, 1));
    /* KEEP OUT plank + the polite note */
    tk_rect(cx - 24, top - 12, 49, 11, PAL_RAMP(RAMP_WOOD, 1));
    tk_rect(cx - 23, top - 11, 47, 9, PAL_RAMP(RAMP_WOOD, 4));
    tk_rect(cx - 23, top - 11, 47, 1, PAL_RAMP(RAMP_WOOD, 6));
    tk_text(cx - 21, top - 11, "KEEP OUT", PAL_UI_RED_D, 1);
    tk_rect(cx + 12, top + 30, 17, 10, PAL_WHITE);
    tk_rect(cx + 12, top + 38, 17, 2, PAL_PAINT(PAINT_WHITE, 0));
    tk_text(cx + 13, top + 31, "pls", PAL_GRAY_D, 1);
}

static void house(int x, int y, int w, int wallc, int roofc, int f) {
    int h = 30;
    shadow_blob(x + w / 2.0f, y + 1, w * 0.6f, 3);
    /* chimney + a lazy smoke puff */
    tk_rect(x + w - 10, y - h - 16, 6, 12, PAL_RAMP(RAMP_STONE, 4));
    tk_rect(x + w - 10, y - h - 16, 6, 2, PAL_RAMP(RAMP_STONE, 5));
    for (int i = 0; i < 3; i++) {
        float k = fmodf(f * 0.03f + i * 0.33f, 1.0f);
        disc(x + w - 7 + sinf(k * 6 + i) * 3, y - h - 20 - k * 22, 2 + k * 3, PAL_PAINT(PAINT_WHITE, 1));
    }
    /* walls with soft siding */
    for (int j = 0; j < h; j++) {
        uint8_t c = (j % 5 == 4) ? PAL_PAINT(wallc, 0) : PAL_PAINT(wallc, 1);
        tk_rect(x, y - h + j, w, 1, c);
    }
    tk_rect(x, y - h, 2, h, PAL_PAINT(wallc, 2));
    tk_rect(x + w - 3, y - h, 3, h, PAL_PAINT(wallc, 0));
    /* roof with an eave shadow */
    tk_tri(x - 5, y - h + 1, x + w + 4, y - h + 1, x + w / 2, y - h - 18, PAL_PAINT(roofc, 0));
    tk_tri(x - 3, y - h, x + w + 2, y - h, x + w / 2, y - h - 16, PAL_PAINT(roofc, 1));
    tk_line(x + w / 2, y - h - 15, x - 1, y - h, PAL_PAINT(roofc, 2));
    tk_rect(x, y - h + 1, w, 2, PAL_PAINT(wallc, 0));
    /* heart window with a cream frame, round door with a step */
    disc(x + w / 2 + 0.5f, y - h + 11, 5.5f, PAL_UI_CREAM);
    tk_heart(x + w / 2, y - h + 11, 1, PAL_RAMP(RAMP_CANDY, 6 - ((f / 15) & 1)));
    tk_rect(x + w / 2 - 5, y - 13, 10, 13, PAL_UI_CREAM);
    tk_rect(x + w / 2 - 4, y - 12, 8, 12, PAL_RAMP(RAMP_WOOD, 3));
    disc(x + w / 2, y - 12, 4, PAL_RAMP(RAMP_WOOD, 3));
    tk_rect(x + w / 2 - 4, y - 12, 1, 12, PAL_RAMP(RAMP_WOOD, 5));
    tk_px(x + w / 2 + 2, y - 6, PAL_UI_GOLD);
    tk_rect(x + w / 2 - 6, y, 12, 2, PAL_RAMP(RAMP_STONE, 5));
    /* flower box */
    for (int i = 0; i < 4; i++) tk_px(x + 4 + i * 2, y - 14, PAL_PAINT(i & 1 ? PAINT_LEMON : PAINT_BUBBLEGUM, 1));
    tk_rect(x + 3, y - 13, 9, 2, PAL_RAMP(RAMP_WOOD, 4));
}

static void lolly_tree(float x, float y, int col, int f) {
    tk_rect((int)x - 1, (int)y - 20, 3, 20, PAL_RAMP(RAMP_WOOD, 4));
    tk_rect((int)x - 1, (int)y - 20, 1, 20, PAL_RAMP(RAMP_WOOD, 6));
    float sw = sinf(f * 0.07f + x) * 0.6f;
    ball(x + sw, y - 28, 10, 10, SH_PAINT(col));
    for (int i = 0; i < 3; i++) tk_px((int)(x + sw) - 4 + i * 4, (int)y - 30 + (i & 1) * 4, PAL_PAINT(col, 2));
}

static void bunting(int f, int x0, int x1, int y0, float sag) {
    static const int cols[] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_MINT, PAINT_SKY, PAINT_LAVENDER };
    int n = (x1 - x0) / 11;
    for (int i = 0; i < n; i++) {
        float t = (float)i / n, t2 = (float)(i + 1) / n;
        float x = x0 + (x1 - x0) * t, y = y0 + sag * 4 * t * (1 - t);
        float x2 = x0 + (x1 - x0) * t2, y2 = y0 + sag * 4 * t2 * (1 - t2);
        tk_line((int)x, (int)y, (int)x2, (int)y2, PAL_GRAY_D);
        float sw = sinf(f * 0.2f + i * 1.3f) * 1.2f;
        int k = i % 5;
        tk_tri((int)x + 1, (int)y + 1, (int)x + 9, (int)y + 1, (int)(x + 5 + sw), (int)y + 8, PAL_PAINT(cols[k], 0));
        tk_tri((int)x + 2, (int)y + 1, (int)x + 8, (int)y + 1, (int)(x + 5 + sw), (int)y + 6, PAL_PAINT(cols[k], 1));
    }
}

static void ground(void) {
    int y0 = 98 + g_cy;
    for (int y = y0; y < TH; y++) {
        if (y < 0) continue;
        for (int x = 0; x < TW; x++) {
            int dy = y - y0;
            uint8_t c = dy < 8 ? PAL_RAMP(RAMP_GRASS, 6) : PAL_RAMP(RAMP_GRASS, 5);
            if (dy >= 6 && dy < 9 && BY[y & 3][x & 3] < (dy - 5) * 5) c = PAL_RAMP(RAMP_GRASS, 5);
            if (dy > 44 && BY[y & 3][x & 3] < (dy - 44)) c = PAL_RAMP(RAMP_GRASS, 4);
            if (((x * 7 + y * 13) % 41) == 0) c = PAL_RAMP(RAMP_GRASS, 7);
            if (((x * 5 + y * 11) % 53) == 0) c = PAL_RAMP(RAMP_GRASS, 4);
            /* path to the gate */
            float half = 18 + (y - y0) * 0.9f;
            if (fabsf(x - 120.0f) < half) c = (fabsf(x - 120.0f) > half - 2) ? PAL_RAMP(RAMP_PEACH, 5) : PAL_RAMP(RAMP_PEACH, 6);
            tk_fb[y][x] = c;
        }
    }
    /* flowers */
    for (int i = 0; i < 30; i++) {
        uint32_t s = tk_hash((uint32_t)i * 7u + 5u);
        int x = tk_rand(&s, TW), y = y0 + 4 + tk_rand(&s, 60);
        if (fabsf(x - 120.0f) < 20 + (y - y0)) continue;
        static const int fc[] = { PAINT_BUBBLEGUM, PAINT_LEMON, PAINT_WHITE, PAINT_LAVENDER };
        tk_px(x, y + 1, PAL_RAMP(RAMP_GRASS, 4));
        tk_px(x, y, PAL_PAINT(fc[i & 3], 1));
    }
}

/* ========================================================================= */
/* the Doomogachi (callback!)                                                 */
/* ========================================================================= */
static void doomogachi(float x, float y, int evil) {
    Sh shell = { PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE, PAL_WHITE };
    ball(x, y, 8, 9, shell);
    tk_rect((int)x - 5, (int)y - 5, 10, 8, PAL_PAINT(PAINT_ROSE, 0));
    tk_rect((int)x - 4, (int)y - 4, 8, 6, PAL_UI_LCD);
    /* the tiny pet on the LCD */
    uint8_t k = PAL_RAMP(RAMP_GRASS, 1);
    tk_rect((int)x - 2, (int)y - 3, 4, 3, k);
    tk_px((int)x - 1, (int)y - 2, PAL_UI_LCD); tk_px((int)x + 1, (int)y - 2, PAL_UI_LCD);
    if (evil) { tk_px((int)x - 2, (int)y - 4, k); tk_px((int)x + 1, (int)y - 4, k); }
    for (int i = -1; i <= 1; i++) disc(x + i * 3.5f, y + 5, 1.2f, PAL_PAINT(PAINT_LEMON, 1));
}

static void bubble(int x, int y, const char *s, int tail_left) {
    int w = tk_text_w(s, 1) + 8, h = 13;
    tk_rect(x + 1, y, w - 2, h, PAL_GRAY_D);
    tk_rect(x, y + 1, w, h - 2, PAL_GRAY_D);
    tk_rect(x + 1, y + 1, w - 2, h - 2, PAL_WHITE);
    tk_rect(x + 2, y + h - 3, w - 4, 1, PAL_PAINT(PAINT_WHITE, 0));
    int tx = tail_left ? x + 4 : x + w - 8;
    tk_tri(tx, y + h - 1, tx + 5, y + h - 1, tail_left ? tx - 2 : tx + 7, y + h + 4, PAL_GRAY_D);
    tk_tri(tx + 1, y + h - 2, tx + 4, y + h - 2, tail_left ? tx - 1 : tx + 6, y + h + 2, PAL_WHITE);
    tk_text(x + 4, y + 3, s, PAL_GRAY_D, 1);
}

static void bang(int x, int y, int age) {
    if (age < 0) return;
    float s = age < 4 ? tk_bounce(age / 4.0f) : 1;
    int h = (int)(12 * s);
    tk_rect(x - 2, y - h, 5, h - 4, PAL_UI_RED_D);
    tk_rect(x - 1, y - h + 1, 3, h - 6, PAL_UI_RED);
    tk_rect(x - 2, y - 3, 5, 4, PAL_UI_RED_D);
    tk_rect(x - 1, y - 2, 3, 2, PAL_UI_RED);
}

/* ========================================================================= */
/* the crowd                                                                  */
/* ========================================================================= */
typedef struct { int x, y; uint32_t seed; int flee_dir; } Cit;
static const Cit CROWD[] = {
    { 22, 132, 3, -1 }, { 50, 146, 11, -1 }, { 74, 124, 21, -1 }, { 96, 140, 8, -1 },
    { 146, 140, 17, 1 }, { 168, 124, 40, 1 }, { 184, 148, 5, 1 },
};
#define NCROWD ((int)(sizeof CROWD / sizeof CROWD[0]))
static const TkChibi BUN = {
    PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_LEMON, 1),
    PAL_PAINT(PAINT_TEAL, 1), TKH_BALD, 0, 0, 0, TKT_BUNNY, 1, 1,
    TKEAR_BUNNY, PAL_PAINT(PAINT_WHITE, 2), 1 };

static void draw_crowd(int f) {
    int frozen = f >= F_SCRATCH;
    int bobf = frozen ? F_SCRATCH : f;
    /* back-to-front by y */
    int order[NCROWD + 1];
    for (int i = 0; i < NCROWD; i++) order[i] = i;
    for (int i = 0; i < NCROWD; i++) for (int j = i + 1; j < NCROWD; j++) if (CROWD[order[j]].y < CROWD[order[i]].y) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    /* the bunny from the vape shop stands in the middle of it all */
    int bunny_drawn = 0;
    for (int k = 0; k < NCROWD; k++) {
        const Cit *c = &CROWD[order[k]];
        if (!bunny_drawn && c->y > 130) {
            bunny_drawn = 1;
            int by = 130 + g_cy;
            shadow_blob(120, by, 9, 2.5f);
            tk_chibi(120, by, &BUN, 1, TKE_POSSESSED, TKM_FLAT, 0, 0);
            /* a far-too-wide grin, size 1 */
            for (int dx = -5; dx <= 5; dx++) {
                int yy = by - 18 - (dx * dx) / 10;
                tk_px(120 + dx, yy, PAL_PAINT(PAINT_JET, 0));
                tk_px(120 + dx, yy - 1, (dx & 1) ? PAL_GRAY_L : PAL_WHITE);
            }
            if (f >= F_FLEE + 8 && f < F_CAP1) bubble(52, by - 44, "it's fine.", 0);
        }
        TkChibi ch = tk_random_citizen(c->seed, 0);
        float x = c->x, y = c->y + g_cy;
        int expr = TKE_HAPPY, mouth = TKM_OPEN;
        float hop = 0;
        if (!frozen) hop = -fabsf(sinf((f + c->seed * 3) * 0.42f)) * 5;
        else if (f < F_SCRATCH + 3) hop = -fabsf(sinf((F_SCRATCH + c->seed * 3) * 0.42f)) * 5;   /* hangs in the air */
        else hop = -fabsf(sinf((F_SCRATCH + c->seed * 3) * 0.42f)) * 5 * (f < F_CRACK ? 1 : 0);
        if (frozen && f >= F_SCRATCH + 2) { expr = TKE_SHOCK; mouth = TKM_O; }
        int flip = 0;
        if (f >= F_CRACK) { expr = TKE_SHOCK; mouth = TKM_SCREAM; }
        if (f >= F_FLEE) {
            float a = (f - F_FLEE - (int)(c->seed % 7)) * 1.0f;
            if (a > 0) {
                x += c->flee_dir * (a * 2.0f + a * a * 0.35f);
                hop = -fabsf(sinf(a * 0.8f)) * 4;
                flip = c->flee_dir < 0;
            }
        }
        if (x < -20 || x > TW + 20) continue;
        shadow_blob(x, y, 9, 2.5f);
        int bobv = !frozen ? f + (int)c->seed * 5 : f < F_CRACK ? bobf + (int)c->seed * 5 : f * 2 + (int)c->seed * 5;
        if (f >= F_CRACK && f < F_FLEE) x += ((f + (int)c->seed) & 2) ? 1 : 0;     /* trembling */
        tk_chibi((int)x, (int)(y + hop), &ch, 1, expr, mouth, bobv, flip);
        if (f >= F_SCRATCH + 2 && f < F_CRACK + 6) bang((int)x, (int)(y + hop) - 36, f - F_SCRATCH - 2 - (int)(c->seed % 4));
        if (f >= F_FLEE && (f / 3 + (int)c->seed) % 4 == 0) {   /* sweat drops flying off */
            disc(x - c->flee_dir * 9, y - 26 + hop, 1.2f, PAL_PAINT(PAINT_SKY, 2));
        }
    }
}

static void draw_hero(int f) {
    int frozen = f >= F_SCRATCH;
    int x = 206, y = 162 + g_cy / 2;
    int expr = frozen ? TKE_SHOCK : TKE_HAPPY, mouth = frozen ? TKM_O : TKM_SMILE;
    if (f >= F_CRACK) { expr = TKE_SHOCK; mouth = TKM_SCREAM; }
    float hop = frozen ? 0 : -fabsf(sinf(f * 0.42f)) * 3;
    int bobv = !frozen ? f : f < F_CRACK ? F_SCRATCH : f * 2;
    if (f >= F_CRACK && (f & 2)) x += 1;
    tk_chibi(x, (int)(y + hop), &TK_HERO, 2, expr, mouth, bobv, 1);
    float dx = x, dy = y + hop - 22;
    if (f >= F_BEEP && f < F_BEEP + 4) dy -= (F_BEEP + 4 - f);       /* hop on the beep */
    doomogachi(dx, dy, f >= F_POUR);
    /* paws holding it */
    ball(dx - 8, dy + 3, 3, 3, SH_PAINT(PAINT_TANGERINE));
    ball(dx + 8, dy + 3, 3, 3, SH_PAINT(PAINT_TANGERINE));
    if (f >= F_SCRATCH + 2 && f < F_CRACK + 6) bang(x, (int)(y + hop) - 74, f - F_SCRATCH - 4);
    if (f >= F_BEEP && f < F_CRACK + 2) bubble(156, (int)dy - 30, "beep.", 0);
}

/* ========================================================================= */
/* LORD VAPULA                                                                 */
/* ========================================================================= */
static const float VAP_P[][3] = {   /* puff layout around the centre (unit ~ 1 px at scale 1) */
    { 0, 0, 22 }, { -24, 6, 17 }, { 24, 6, 17 }, { -40, 14, 13 }, { 40, 14, 13 }, { -14, -16, 16 }, { 14, -18, 17 },
    { 0, -26, 14 }, { -30, -8, 13 }, { 30, -8, 13 }, { -52, 22, 11 }, { 52, 22, 11 }, { -10, 18, 16 }, { 12, 20, 16 },
    { -28, 24, 13 }, { 28, 24, 13 }, { -60, 34, 10 }, { 60, 34, 10 }, { 0, 30, 14 }, { -44, 30, 11 }, { 44, 30, 11 },
};
#define NVAP ((int)(sizeof VAP_P / sizeof VAP_P[0]))

static void vapula_body(int f, float cx, float cy, float s) {
    Sh vs = { PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 5), 0 };
    Puff p[NVAP];
    for (int i = 0; i < NVAP; i++) {
        float wob = sinf(f * 0.09f + i * 1.7f) * 1.6f;
        p[i] = (Puff){ cx + VAP_P[i][0] * s + wob, cy + VAP_P[i][1] * s + cosf(f * 0.07f + i) * 1.2f, (VAP_P[i][2] + wob * 0.4f) * s };
    }
    cloud(p, NVAP, vs);
    /* soft top light */
    for (int i = 0; i < NVAP; i++) if (VAP_P[i][1] < 0) haze(p[i].x - p[i].r * 0.3f, p[i].y - p[i].r * 0.35f, p[i].r * 0.5f, p[i].r * 0.35f, L_VAPE[3]);
}

static void vapula_face(int f, float cx, float cy, float s) {
    /* eyes snap open */
    float open = f < F_EYES ? 0 : tk_bounce((f - F_EYES) / 6.0f);
    if (open > 0) {
        for (int e = -1; e <= 1; e += 2) {
            float ex = cx + e * 15 * s, ey = cy - 4 * s;
            float h = 6 * s * open;
            light_pool(ex, ey, 14 * s, 10 * s, L_LAVA, 3, 1.4f);
            /* angry slanted eye: a lens shape */
            for (int y = (int)(ey - h - 1); y <= (int)(ey + h + 1); y++)
                for (int x = (int)(ex - 10 * s); x <= (int)(ex + 10 * s); x++) {
                    float u = (x + .5f - ex) / (10 * s), v = (y + .5f - ey - e * u * 3 * s) / (h + 0.01f);
                    if (u * u + v * v * 1.0f > 1) continue;
                    uint8_t c = PAL_RAMP(RAMP_LAVA, 6);
                    if (u * u + v * v > 0.6f) c = PAL_UI_RED;
                    if (v < -0.3f) c = PAL_RAMP(RAMP_LAVA, 7);
                    tk_px(x, y, c);
                }
            /* slit pupil */
            if (open > 0.5f) tk_rect((int)ex - 1, (int)(ey - h * 0.7f), 2, (int)(h * 1.4f), PAL_RAMP(RAMP_BLOOD, 0));
            /* brow */
            for (int k = 0; k < 3; k++)
                tk_line((int)(ex - e * 9 * s), (int)(ey - 3 * s) - k, (int)(ex + e * 11 * s), (int)(ey - 11 * s) - k, PAL_RAMP(RAMP_VAPE, 0));
        }
    }
    /* the grin */
    if (f >= F_ROAR) {
        float m = tk_bounce((f - F_ROAR) / 8.0f);
        float mw = 26 * s, mh = (5 + 7 * m) * s;
        float my = cy + 12 * s;
        for (int y = (int)(my - 4 * s); y <= (int)(my + mh + 2); y++)
            for (int x = (int)(cx - mw - 1); x <= (int)(cx + mw + 1); x++) {
                float u = (x + .5f - cx) / mw;
                if (fabsf(u) > 1) continue;
                float top = my - 3 * s * (1 - u * u) + u * u * -5 * s;
                float bot = my + mh * (1 - u * u * 0.8f) - u * u * 5 * s;
                if (y < top - 1 || y > bot + 1) continue;
                uint8_t c = PAL_RAMP(RAMP_BLOOD, 1);
                if (y < top || y > bot) c = PAL_RAMP(RAMP_VAPE, 0);
                else if (y > (top + bot) / 2) c = PAL_RAMP(RAMP_BLOOD, 2 + ((f / 3) & 1));
                tk_px(x, y, c);
            }
        /* fangs */
        for (int i = -4; i <= 4; i++) {
            float u = i / 5.0f, tx = cx + u * mw;
            float top = my - 3 * s * (1 - u * u) - u * u * 5 * s;
            float bot = my + mh * (1 - u * u * 0.8f) - u * u * 5 * s;
            int tw = (int)(2.5f * s) + 1;
            tk_tri((int)tx - tw, (int)top, (int)tx + tw, (int)top, (int)tx, (int)(top + 5 * s), PAL_RAMP(RAMP_BONE, 7));
            tk_tri((int)tx - tw + 2, (int)bot, (int)tx + tw + 2, (int)bot, (int)tx + 2, (int)(bot - 4 * s), PAL_RAMP(RAMP_BONE, 6));
        }
        light_pool(cx, my + mh / 2, mw, mh + 4, L_LAVA, 1, 1.0f);
    }
    /* the crown drops on. crooked. */
    if (f >= F_CROWN) {
        float k = (f - F_CROWN) / 8.0f;
        float drop = k < 1 ? (1 - k) * (1 - k) * -90 : 0;
        float bounce = k >= 1 && k < 1.6f ? -sinf((k - 1) / 0.6f * 3.14159f) * 5 : 0;
        float tilt = k >= 1 ? 0.25f : 0.0f;
        float kx = cx + 10 * s, ky = cy - 30 * s + drop + bounce;
        int w = (int)(13 * s);
        for (int y = -(int)(14 * s); y <= (int)(4 * s); y++)
            for (int x = -w; x <= w; x++) {
                float rx = x * cosf(tilt) + y * sinf(tilt), ry = -x * sinf(tilt) + y * cosf(tilt);
                int in = 0;
                if (ry >= -2 * s && ry <= 4 * s && fabsf(rx) <= 12 * s) in = 1;
                for (int p = -2; p <= 2; p++) {
                    float px = p * 6 * s;
                    float hgt = (p == 0 ? 13 : 10) * s;
                    if (ry < -2 * s && ry > -hgt && fabsf(rx - px) < (ry + hgt) * 0.3f) in = 1;
                }
                if (!in) continue;
                uint8_t c = PAL_RAMP(RAMP_CANDY, 4);
                if (rx < -6 * s) c = PAL_RAMP(RAMP_CANDY, 5);
                if (ry > 2 * s) c = PAL_RAMP(RAMP_CANDY, 2);
                if (ry < -9 * s) c = PAL_RAMP(RAMP_CANDY, 6);
                tk_px((int)kx + x, (int)ky + y, c);
            }
        disc(kx, ky + 1 * s, 2 * s, PAL_UI_RED);
        tk_px((int)kx - 1, (int)(ky + 0.5f * s) - 1, PAL_WHITE);
        if (k >= 1 && k < 2.2f) tk_sparkles((int)kx, (int)ky - 6, (int)((k - 1) * 30), 99, 16);
    }
}

/* his vape: a giant pen in a cloud fist */
static void vapula_vape(int f, float cx, float cy, float s) {
    float hx = cx + 64 * s, hy = cy + 14 * s + sinf(f * 0.1f) * 2;
    /* pen, diagonal */
    for (int i = 0; i < 34; i++) {
        float t = i / 34.0f;
        float x = hx - 6 * s + t * 30 * s, y = hy + 6 * s - t * 26 * s;
        uint8_t c = t > 0.62f && t < 0.68f ? PAL_UI_GOLD : PAL_PAINT(PAINT_VAPE_PURPLE, 1);
        disc(x, y, 3.2f * s, PAL_PAINT(PAINT_GRAPE, 0));
        disc(x - 0.5f, y - 0.5f, 2.3f * s, c);
        disc(x - 1.2f * s, y - 1.2f * s, 0.8f * s, PAL_PAINT(PAINT_VAPE_PURPLE, 2));
    }
    float ex = hx + 25 * s, ey = hy - 21 * s;
    light_pool(ex, ey, 14 * s, 14 * s, L_LAVA, 3, 1.5f);
    disc(ex, ey, 3 * s, PAL_RAMP(RAMP_LAVA, 6 + ((f / 3) & 1)));
    /* smoke curling off the ember */
    for (int i = 0; i < 4; i++) {
        float k = fmodf(f * 0.03f + i * 0.25f, 1.0f);
        haze(ex + sinf(k * 7 + i) * 6 * s, ey - 4 - k * 40 * s, (3 + k * 8) * s, (2 + k * 5) * s, L_VAPE[1]);
    }
    /* the fist */
    Sh vs = { PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 5), 0 };
    Puff p[4] = { { hx, hy, 9 * s }, { hx - 7 * s, hy + 6 * s, 7 * s }, { hx - 16 * s, hy + 12 * s, 7 * s }, { hx + 6 * s, hy + 5 * s, 6 * s } };
    cloud(p, 4, vs);
}

/* ========================================================================= */
/* demons                                                                     */
/* ========================================================================= */
static void puff_demon(float x, float y, float s, int f, int seed) {
    Sh vs = { PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), 0 };
    float b = sinf(f * 0.4f + seed) * 1.0f;
    tk_tri((int)(x - 7 * s), (int)(y - 8 * s + b), (int)(x - 3 * s), (int)(y - 10 * s + b), (int)(x - 8 * s), (int)(y - 16 * s + b), PAL_PAINT(PAINT_CHERRY, 0));
    tk_tri((int)(x + 7 * s), (int)(y - 8 * s + b), (int)(x + 3 * s), (int)(y - 10 * s + b), (int)(x + 8 * s), (int)(y - 16 * s + b), PAL_PAINT(PAINT_CHERRY, 0));
    Puff p[5] = { { x - 6 * s, y - 2 * s + b, 5 * s }, { x + 6 * s, y - 2 * s + b, 5 * s }, { x, y - 6 * s + b, 7 * s },
                  { x - 3 * s, y + 1 * s + b, 5 * s }, { x + 4 * s, y + 1 * s + b, 5 * s } };
    cloud(p, 5, vs);
    int ey = (int)(y - 6 * s + b);
    for (int e = -1; e <= 1; e += 2) {
        int ex = (int)(x + e * 3 * s);
        tk_rect(ex - 1, ey - 1, 3, 3, PAL_UI_RED);
        tk_px(ex, ey - 1, PAL_RAMP(RAMP_LAVA, 7));
        tk_line(ex - e * 3, ey - 3, ex + e * 1, ey - 2, PAL_RAMP(RAMP_VAPE, 0));
    }
    tk_rect((int)(x - 3 * s), ey + 3, (int)(6 * s) + 1, 2, PAL_RAMP(RAMP_BLOOD, 1));
    tk_px((int)(x - 2 * s), ey + 3, PAL_WHITE); tk_px((int)(x + 2 * s), ey + 3, PAL_WHITE);
}

static void hound(float x, float y, float s, int f, int flip) {
    float run = sinf(f * 0.9f);
    int d = flip ? -1 : 1;
    Sh fur = { PAL_RAMP(RAMP_BLOOD, 0), PAL_RAMP(RAMP_BLOOD, 2), PAL_RAMP(RAMP_BLOOD, 3), PAL_RAMP(RAMP_BLOOD, 4), PAL_RAMP(RAMP_BLOOD, 6) };
    /* flame tail */
    float tx = x - d * 9 * s, ty = y - 7 * s;
    for (int i = 0; i < 3; i++) disc(tx - d * i * 2 * s + sinf(f * 0.8f + i) * s, ty - i * 3 * s, (3 - i * 0.8f) * s, i == 0 ? PAL_RAMP(RAMP_LAVA, 4) : PAL_RAMP(RAMP_LAVA, 5 + i));
    /* legs */
    for (int i = 0; i < 4; i++) {
        float lx = x + (i < 2 ? -5 : 4) * s * d + (i & 1) * 2 * s, ph = run * ((i & 1) ? 1 : -1);
        ball(lx + ph * 2 * s, y - 1 * s, 1.8f * s, 2.0f * s, fur);
    }
    ball(x, y - 6 * s, 8 * s, 5 * s, fur);
    /* head */
    float hx = x + d * 7 * s, hy = y - 11 * s + run * 0.5f;
    tk_tri((int)(hx - 4 * s), (int)(hy - 3 * s), (int)(hx - 1 * s), (int)(hy - 4 * s), (int)(hx - 4 * s), (int)(hy - 9 * s), PAL_RAMP(RAMP_BLOOD, 2));
    tk_tri((int)(hx + 4 * s), (int)(hy - 3 * s), (int)(hx + 1 * s), (int)(hy - 4 * s), (int)(hx + 4 * s), (int)(hy - 9 * s), PAL_RAMP(RAMP_BLOOD, 2));
    ball(hx, hy, 5.5f * s, 5 * s, fur);
    ball(hx + d * 3 * s, hy + 2 * s, 3 * s, 2.2f * s, (Sh){ PAL_RAMP(RAMP_BLOOD, 1), PAL_RAMP(RAMP_FLESH, 3), PAL_RAMP(RAMP_FLESH, 4), PAL_RAMP(RAMP_FLESH, 5), 0 });
    tk_px((int)(hx + d * 5 * s), (int)(hy + 1 * s), PAL_PAINT(PAINT_JET, 0));
    tk_rect((int)(hx + d * 1 * s) - 1, (int)(hy - 2 * s), 2, 2, PAL_RAMP(RAMP_LAVA, 7));
    tk_rect((int)(hx - d * 2 * s) - 1, (int)(hy - 2 * s), 2, 2, PAL_RAMP(RAMP_LAVA, 7));
    /* tongue */
    if ((f / 4) & 1) tk_rect((int)(hx + d * 3 * s), (int)(hy + 4 * s), 2, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
}

typedef struct { int t0, type; float lx, ly; } Demon;
static Demon demon_get(int i) {
    uint32_t s = tk_hash((uint32_t)i * 977u + 12345u);
    Demon d;
    d.t0 = F_POUR + i * 3 + tk_rand(&s, 3);
    d.type = i % 3;
    int side = (i & 1) ? 1 : -1;
    d.lx = 120 + side * (44 + tk_rand(&s, 70));
    d.ly = 108 + tk_rand(&s, 44);
    return d;
}
#define NDEMON 26

static void draw_demons(int f, float mx, float my) {
    /* sort by current y */
    float px[NDEMON], py[NDEMON], sc[NDEMON];
    int ok[NDEMON], idx[NDEMON];
    for (int i = 0; i < NDEMON; i++) {
        Demon d = demon_get(i);
        idx[i] = i; ok[i] = 0; px[i] = py[i] = 0; sc[i] = 1;
        int a = f - d.t0;
        if (a < 0) continue;
        float T = 16;
        if (a < T) {
            float t = a / T;
            px[i] = mx + (d.lx - mx) * t;
            py[i] = my + (d.ly - my) * t - sinf(t * 3.14159f) * 14;
            sc[i] = 0.4f + 0.6f * t;
        } else {
            float b = a - T;
            px[i] = d.lx + (d.lx - 120) * b * 0.02f;
            py[i] = d.ly + b * 0.9f - fabsf(sinf(b * 0.35f)) * 6;
            sc[i] = 1 + b * 0.01f;
        }
        ok[i] = 1;
    }
    for (int i = 0; i < NDEMON; i++) for (int j = i + 1; j < NDEMON; j++) if (py[idx[j]] < py[idx[i]]) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    for (int k = 0; k < NDEMON; k++) {
        int i = idx[k];
        if (!ok[i]) continue;
        Demon d = demon_get(i);
        if (py[i] > TH + 40) continue;
        if (f - d.t0 < 4) { haze(px[i], py[i] - 8, 10, 8, L_VAPE[3]); continue; }
        if (f - d.t0 >= 16) shadow_blob(px[i], py[i], 8 * sc[i], 2);
        if (d.type == 0) tk_chibi((int)px[i], (int)py[i], &TK_IMP, 1, TKE_ANGRY, TKM_FANGS, f * 2 + i * 7, px[i] > 120);
        else if (d.type == 1) puff_demon(px[i], py[i], sc[i], f, i);
        else hound(px[i], py[i], sc[i] * 1.3f, f + i * 3, px[i] < 120);
    }
}

/* ========================================================================= */
/* "(wherever you are)": polaroids                                           */
/* ========================================================================= */
static void polaroid_frame(int x, int y, int w, int h, const char *label) {
    tk_rect(x + 2, y + 2, w, h, PAL_RAMP(RAMP_STONE, 1));
    tk_rect(x, y, w, h, PAL_PAINT(PAINT_WHITE, 0));
    tk_rect(x, y, w - 1, h - 1, PAL_WHITE);
    tk_text(x + w / 2 - tk_text_w(label, 1) / 2, y + h - 11, label, PAL_GRAY_D, 1);
}
static void pola_bath(int x, int y, int f) {
    /* tiles */
    for (int j = 0; j < 46; j++) for (int i = 0; i < 52; i++)
        tk_px(x + i, y + j, ((i % 8 == 0) || (j % 8 == 0)) ? PAL_RAMP(RAMP_MINT, 6) : PAL_RAMP(RAMP_MINT, 7));
    /* a pup citizen soaking in bliss... and an imp with a rubber duck */
    TkChibi pup = tk_random_citizen(5, 0);
    pup.ears = TKEAR_PUP; pup.skin = PAL_PAINT(PAINT_CARAMEL, 1); pup.hat = 0; pup.horns = 0; pup.halo = 0;
    tk_chibi(x + 15, y + 44, &pup, 1, TKE_HAPPY, TKM_SMILE, f, 0);
    tk_chibi(x + 37, y + 44, &TK_IMP, 1, TKE_HAPPY, TKM_FANGS, f + 9, 1);
    /* tub + foam */
    tk_rect(x + 1, y + 30, 50, 16, PAL_PAINT(PAINT_WHITE, 0));
    tk_rect(x + 2, y + 31, 48, 14, PAL_WHITE);
    tk_rect(x + 2, y + 42, 48, 3, PAL_PAINT(PAINT_WHITE, 1));
    for (int i = 0; i < 10; i++) disc(x + 3 + i * 5.2f, y + 29 + (i & 1), 2.5f + (i % 3 == 0), PAL_WHITE);
    for (int i = 0; i < 10; i++) tk_px(x + 3 + i * 5, y + 28 + (i & 1), PAL_PAINT(PAINT_SKY, 2));
    /* the duck, held up proudly */
    ball(x + 47, y + 16, 3, 2.5f, SH_PAINT(PAINT_LEMON));
    disc(x + 45, y + 13, 2, PAL_PAINT(PAINT_LEMON, 1));
    tk_px(x + 43, y + 13, PAL_PAINT(PAINT_TANGERINE, 1));
    tk_px(x + 45, y + 12, PAL_PAINT(PAINT_JET, 0));
    /* bubbles floating */
    for (int i = 0; i < 4; i++) { int by = y + 24 - ((f + i * 9) % 24); tk_ring(x + 8 + i * 11, by, 1, PAL_WHITE); }
}
static void pola_bakery(int x, int y, int f) {
    for (int j = 0; j < 46; j++) for (int i = 0; i < 52; i++)
        tk_px(x + i, y + j, (j < 32) ? (((i / 6) & 1) ? PAL_RAMP(RAMP_PASTEL_PINK, 7) : PAL_RAMP(RAMP_PASTEL_PINK, 6)) : PAL_RAMP(RAMP_WOOD, 5));
    tk_rect(x, y + 32, 52, 1, PAL_RAMP(RAMP_WOOD, 6));
    tk_chibi(x + 12, y + 50, &TK_NANA, 1, TKE_SHOCK, TKM_SCREAM, f, 0);
    /* a giant cupcake... with a guest popping out of the frosting */
    int cx = x + 37, cy = y + 44;
    int pop = (f % 12) < 6 ? 0 : -1;
    tk_chibi(cx, cy - 10 + pop, &TK_IMP, 1, TKE_HAPPY, TKM_FANGS, f + 4, 1);
    for (int j = 0; j < 12; j++) {
        int w = 22 - j / 2;
        for (int i = 0; i < w; i++) tk_px(cx - w / 2 + i, cy - 12 + j + 0, ((i + 1) % 4 == 0) ? PAL_PAINT(PAINT_CARAMEL, 0) : PAL_PAINT(PAINT_CARAMEL, 1));
    }
    Sh fr = { PAL_PAINT(PAINT_BUBBLEGUM, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE, 0 };
    for (int i = 0; i < 5; i++) ball(cx - 10 + i * 5, cy - 14 + (i & 1), 3.2f, 2.6f, fr);
    disc(cx - 6, cy - 16, 0.8f, PAL_PAINT(PAINT_LEMON, 1)); disc(cx + 5, cy - 15, 0.8f, PAL_PAINT(PAINT_SKY, 1));
}
static void pola_bed(int x, int y, int f) {
    /* night-time bedroom */
    for (int j = 0; j < 46; j++) for (int i = 0; i < 52; i++) tk_px(x + i, y + j, j < 30 ? PAL_RAMP(RAMP_SKY, 2) : PAL_RAMP(RAMP_WOOD, 1));
    /* moonlit window */
    tk_rect(x + 34, y + 3, 14, 13, PAL_RAMP(RAMP_SKY, 4));
    disc(x + 41, y + 9, 3.5f, PAL_RAMP(RAMP_CANDY, 7));
    disc(x + 43, y + 8, 3, PAL_RAMP(RAMP_SKY, 4));
    tk_rect(x + 40, y + 3, 1, 13, PAL_RAMP(RAMP_SKY, 1));
    /* a mouse kid fast asleep */
    tk_chibi(x + 17, y + 38, &TK_PIP, 1, TKE_SLEEPY, TKM_FLAT, f, 0);
    if ((f / 12) & 1) tk_text(x + 26, y + 2, "z", PAL_WHITE, 1);
    /* the bed: blanket, frame */
    for (int j = 0; j < 9; j++) for (int i = 0; i < 44; i++)
        tk_px(x + 4 + i, y + 26 + j, (((i + j) / 4) & 1) ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_PAINT(PAINT_BUBBLEGUM, 2));
    tk_rect(x + 4, y + 26, 44, 1, PAL_WHITE);
    tk_rect(x + 2, y + 35, 48, 3, PAL_RAMP(RAMP_WOOD, 4));
    tk_rect(x + 2, y + 22, 3, 24, PAL_RAMP(RAMP_WOOD, 4));
    tk_rect(x + 47, y + 28, 3, 18, PAL_RAMP(RAMP_WOOD, 4));
    /* the dark gap under the bed... and what lives there now */
    tk_rect(x + 5, y + 38, 42, 8, PAL_RAMP(RAMP_STONE, 0));
    int blink = (f % 40) < 2;
    for (int e = 0; e < 2; e++) {
        int ex = x + 22 + e * 7, ey = y + 40;
        if (blink) tk_rect(ex - 1, ey + 1, 3, 1, PAL_PAINT(PAINT_LEMON, 1));
        else { tk_rect(ex - 1, ey, 3, 2, PAL_PAINT(PAINT_LEMON, 1)); tk_px(ex, ey, PAL_UI_RED); }
    }
    for (int i = 0; i < 9; i++) tk_px(x + 21 + i, y + 43 + (i & 1), PAL_WHITE);
    /* a little claw, waving hello */
    float w = sinf(f * 0.5f) * 2;
    tk_rect(x + 36 + (int)w, y + 41, 3, 3, PAL_PAINT(PAINT_CHERRY, 1));
    tk_px(x + 36 + (int)w, y + 40, PAL_PAINT(PAINT_CHERRY, 2)); tk_px(x + 38 + (int)w, y + 40, PAL_PAINT(PAINT_CHERRY, 2));
}

static void polaroid(int which, float cx, float cy, float ang, int age, int f) {
    if (age < 0) return;
    layer_begin();
    int x = 120 - 30, y = 80 - 32;
    polaroid_frame(x, y, 60, 66, which == 0 ? "the bath" : which == 1 ? "the bakery" : "your bed");
    if (which == 0) pola_bath(x + 4, y + 4, f);
    else if (which == 1) pola_bakery(x + 4, y + 4, f);
    else pola_bed(x + 4, y + 4, f);
    /* camera flash bloom on arrival */
    if (age < 3) lut_rect(x, y, 60, 66, L_WHITE[2 - age]);
    layer_end(LAYER_B);
    float s = age < 8 ? 1.0f + (1.0f - tk_bounce(age / 8.0f)) * 0.6f : 1.0f;
    layer_draw(LAYER_B, 120, 80, cx, cy, s, s, ang, 0, TH);
}

/* ========================================================================= */
/* captions                                                                   */
/* ========================================================================= */
static void slam_caption(const char *s, int cy, uint8_t c, uint8_t oc, int scale, int age) {
    if (age < 0) return;
    layer_begin();
    tk_caption(80, s, c, oc, scale);
    layer_end(LAYER_B);
    float k = age < 7 ? 1.0f + (1.0f - tk_bounce(age / 7.0f)) * 1.4f : 1.0f;
    layer_draw(LAYER_B, 120, 80, 120, (float)cy, k, k, 0, 0, TH);
}

/* ========================================================================= */
/* the scene                                                                  */
/* ========================================================================= */
static void crack(int f) {
    int a = f - F_CRACK;
    if (a < 0) return;
    float grow = tk_clampf(a / 6.0f, 0, 1);
    float ox = 120, oy = 14 + g_cy * 0.5f;
    light_pool(ox, oy, 40, 22, L_LAVA, 3, 1.2f);
    for (int br = 0; br < 8; br++) {
        uint32_t s = tk_hash((uint32_t)br * 31u + 9u);
        float ang = 3.14159f + br * 0.45f + (tk_rand(&s, 100) - 50) / 300.0f;
        if (br >= 4) ang = br * 0.45f - 1.4f + 3.14159f * 0 + (tk_rand(&s, 100) - 50) / 300.0f;
        float x = ox, y = oy;
        int segs = 9;
        for (int i = 0; i < segs && i < (int)(grow * segs + 0.999f); i++) {
            float len = 6 + tk_rand(&s, 8);
            float a2 = ang + (tk_rand(&s, 100) - 50) / 45.0f;
            float nx = x + cosf(a2) * len * 1.5f, ny = y + sinf(a2) * len * 0.55f;
            float w = 2.2f * (1.0f - (float)i / segs);
            int steps = (int)(len * 1.6f) + 1;
            for (int k = 0; k <= steps; k++) {
                float t = (float)k / steps, px = x + (nx - x) * t, py = y + (ny - y) * t;
                float ww = w * (1 - t * 0.12f);
                disc(px, py, ww + 1.0f, PAL_RAMP(RAMP_BLOOD, 2));
            }
            for (int k = 0; k <= steps; k++) {
                float t = (float)k / steps, px = x + (nx - x) * t, py = y + (ny - y) * t;
                float ww = w * (1 - t * 0.12f);
                if (ww > 0.6f) disc(px, py, ww, PAL_RAMP(RAMP_LAVA, 5));
                tk_px((int)px, (int)py, PAL_RAMP(RAMP_LAVA, 7));
            }
            /* side twig */
            if (i == 2 || i == 5) {
                float ta = a2 + ((tk_rand(&s, 2)) ? 0.9f : -0.9f);
                tk_line((int)nx, (int)ny, (int)(nx + cosf(ta) * 9), (int)(ny + sinf(ta) * 4), PAL_RAMP(RAMP_LAVA, 5));
            }
            x = nx; y = ny;
        }
    }
    disc(ox, oy, 3 + grow, PAL_RAMP(RAMP_LAVA, 7));
}

/* the rot creeps out from the gate; Vapula's own pixels are spared */
static void rot_town(int f) {
    if (f < 40) return;
    float R = (f - 40) * 5.0f;
    float ox = 120, oy = 76 + g_cy;
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            if (f >= F_CLOUD && LAYER_A[y][x] && LAYER_A[y][x] == tk_fb[y][x]) continue;
            float dx = x - ox, dy = (y - oy) * 1.4f;
            float d = sqrtf(dx * dx + dy * dy);
            float a = atan2f(dy, dx);
            d += sinf(a * 7 + 1.3f) * 7 + sinf(a * 13) * 4;      /* ragged front */
            float e = R - d;
            if (e <= 0) continue;
            if (e < 14 && BY[y & 3][x & 3] >= (int)(e / 14 * 16)) continue;
            tk_fb[y][x] = HELL[tk_fb[y][x]];
        }
}

void scene_s08_invasion(int f, int n) {
    (void)n;
    luts_init();
    /* camera tilt up while the cloud rises */
    g_cy = (int)(tk_ease((f - F_CLOUD) / 34.0f) * 26);
    int shake = 0;
    float cx = 120, cy = 92 - 62 * tk_ease((f - F_CLOUD) / 36.0f) + g_cy * 0.5f, s = 0.25f + 0.75f * tk_ease((f - F_CLOUD) / 30.0f);

    /* --- sky + Vapula --- */
    sky(f);
    sun(f);
    hellvile_skyline(f, f >= F_CRACK);
    crack(f);
    memset(LAYER_A, 0, sizeof LAYER_A);
    if (f >= F_CLOUD) {
        layer_begin();
        vapula_body(f, cx, cy, s);
        if (f >= F_EYES) vapula_face(f, cx, cy, s);
        if (f >= F_CROWN + 6) vapula_vape(f, cx, cy, s);
        layer_end(LAYER_A);
        layer_draw(LAYER_A, 0, 0, 0, 0, 1, 1, 0, 0, TH);
    }
    /* --- the town --- */
    the_wall();
    gate(f, f >= F_CRACK && f < F_CRACK + 30 ? ((f & 1) ? 1 : -1) : 0);
    house(4, 104 + g_cy, 34, PAINT_BUBBLEGUM, PAINT_ROSE, f);
    house(200, 104 + g_cy, 36, PAINT_MINT, PAINT_SKY, f);
    ground();
    lolly_tree(52, 112 + g_cy, PAINT_BUBBLEGUM, f);
    lolly_tree(190, 112 + g_cy, PAINT_LAVENDER, f);
    bunting(f, 52, 190, 84 + g_cy, 10);
    {   /* party confetti drifting down; after the crack it falls like lead */
        int cf = f < F_SCRATCH ? f : F_SCRATCH;
        static const uint8_t cc[] = { PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_SKY, 1), PAL_WHITE };
        for (int i = 0; i < 40; i++) {
            uint32_t q = tk_hash((uint32_t)i * 13u + 1u);
            float x = (float)tk_rand(&q, TW) + sinf(cf * 0.1f + i) * 4;
            float y = fmodf((float)tk_rand(&q, 200) + cf * (1.0f + (i % 3) * 0.4f), 200) - 30;
            if (f >= F_CRACK) y += (f - F_CRACK) * (f - F_CRACK) * 0.05f;
            tk_rect((int)x, (int)y, 2, 1 + ((cf + i) & 1), cc[i % 5]);
        }
    }
    rot_town(f);
    if (f >= 70) light_pool(120, 176, 210, 70, L_LAVA, f < 90 ? 1 : 2, 1.2f);   /* hell glow from below */

    /* --- characters stay in their own colours --- */
    draw_crowd(f);
    if (f >= F_POUR) draw_demons(f, cx, cy + 14 * s);
    draw_hero(f);

    if (f == F_SCRATCH) tk_post_flash(PAL_WHITE, 7);

    /* --- shakes + glitches --- */
    if (f >= F_CRACK && f < F_CRACK + 8) { tk_post_glitch(f, 22 - (f - F_CRACK) * 2); shake = 3; }
    if (f >= F_ROAR && f < F_ROAR + 14) shake = 4 - (f - F_ROAR) / 4;
    if (f >= F_CROWN + 8 && f < F_CROWN + 11) shake = 2;
    if (f == F_ROAR || f == F_ROAR + 1 || f == F_CAP1 || f == F_CAP2) tk_post_glitch(f * 3, 30);

    /* --- captions over a dark band --- */
    if (f >= F_CAP1) {
        int a = f - F_CAP1;
        float lv = a < 6 ? a / 6.0f * 3 : 3;
        for (int y = 86; y < TH; y++) for (int x = 0; x < TW; x++) {
            float v = y < 100 ? lv * (y - 86) / 14.0f : lv;
            int k = (int)(v + (BY[y & 3][x & 3] + .5f) / 16.0f);
            if (k > 0) lut_px(x, y, L_DARK[k > 4 ? 3 : k - 1]);
        }
        slam_caption("THE DEMONS OF HELL", 104, PAL_RAMP(RAMP_BONE, 7), PAL_RAMP(RAMP_BLOOD, 0), 2, a);
    }
    if (f >= F_CAP2) slam_caption("HAVE INVADED", 128, PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 0), 3, f - F_CAP2);
    if ((f >= F_CAP1 && f < F_CAP1 + 5) || (f >= F_CAP2 && f < F_CAP2 + 5)) shake = 3;
    /* polaroids, to prove it */
    if (f >= F_POLA) lut_rect(0, 0, TW, TH, L_DARK[2]);
    polaroid(0, 46, 112, -0.06f, f - F_POLA, f);
    polaroid(1, 120, 108, 0.03f, f - F_POLA - 8, f);
    polaroid(2, 194, 114, 0.07f, f - F_POLA - 16, f);
    if (f >= F_CAP3) {
        int a = f - F_CAP3;
        tk_caption(153 + (a < 4 ? 4 - a : 0), "(wherever you are)", PAL_WHITE, PAL_RAMP(RAMP_BLOOD, 0), 1);
    }
    if (shake) tk_post_shake((f & 1) ? shake : -shake, (f & 2) ? shake / 2 : -shake / 2);
}

void scene_s08_invasion_audio(double t0, double dur) {
#define AT(fr) (t0 + (fr) / 30.0)
    (void)dur;
    /* party: two happy party-horn toots */
    tk_slide(AT(1), 0.12, 72, 79, TKV_SQ25, 0.16f);
    tk_slide(AT(7), 0.16, 76, 84, TKV_SQ25, 0.16f);
    /* RECORD SCRATCH */
    tk_sfx(AT(F_SCRATCH), SFX_SCRATCH, 1.0f);
    for (int i = 0; i < 5; i++) tk_note(AT(F_SCRATCH + 3 + i), 0.04, 84 + i * 2, TKV_SQ12, 0.12f);   /* "!" blips */
    tk_sfx(AT(F_BEEP), SFX_BEEP, 0.7f);                 /* beep. */
    /* the sky cracks */
    tk_sfx(AT(F_CRACK), SFX_GLITCH, 0.7f);
    tk_sfx(AT(F_CRACK), SFX_THUD, 1.0f);
    tk_slide(AT(F_CRACK), 0.5, 90, 40, TKV_NOISE, 0.4f);
    tk_slide(AT(40), 0.5, 76, 52, TKV_SQ25, 0.12f);     /* the sun leaves: sad slide */
    /* the cloud billows: rumble */
    for (int i = 0; i < 8; i++) tk_note(AT(F_CLOUD + i * 4), 0.2, 28 + (i & 1), TKV_TRI, 0.35f);
    tk_slide(AT(F_CLOUD), 1.0, 30, 60, TKV_NOISE, 0.25f);
    /* eyes open + ROAR */
    tk_sfx(AT(F_EYES), SFX_SQUEAK, 0.2f);
    tk_sfx(AT(F_ROAR), SFX_ROAR, 1.0f);
    tk_sfx(AT(F_ROAR), SFX_EXPLOSION, 0.45f);
    tk_sfx(AT(F_ROAR + 2), SFX_GLITCH, 0.4f);
    /* the crown: clonk + sparkle */
    tk_note(AT(F_CROWN + 8), 0.08, 76, TKV_SQ50, 0.3f);
    tk_note(AT(F_CROWN + 8), 0.25, 88, TKV_SINE, 0.25f);
    tk_sfx(AT(F_CROWN + 9), SFX_SPARKLE, 0.35f);
    /* panic: squeaks */
    for (int i = 0; i < 6; i++) tk_sfx(AT(F_FLEE + i * 3), SFX_SQUEAK, 0.18f);
    /* demons pour out: little cackles + landing thuds */
    for (int i = 0; i < NDEMON; i += 2) {
        Demon d = demon_get(i);
        tk_slide(AT(d.t0), 0.08, 70 + (i % 5) * 2, 82, TKV_SQ12, 0.08f);
        if (i % 4 == 0) tk_sfx(AT(d.t0 + 16), SFX_THUD, 0.15f);
    }
    /* caption slams */
    tk_sfx(AT(F_CAP1), SFX_THUD, 1.0f);
    tk_sfx(AT(F_CAP1), SFX_EXPLOSION, 0.35f);
    tk_sfx(AT(F_CAP2), SFX_THUD, 1.0f);
    tk_sfx(AT(F_CAP2), SFX_EXPLOSION, 0.5f);
    /* polaroids: shutter clicks */
    for (int i = 0; i < 3; i++) {
        tk_note(AT(F_POLA + i * 8), 0.03, 96, TKV_SQ50, 0.25f);
        tk_slide(AT(F_POLA + i * 8 + 1), 0.08, 90, 60, TKV_NOISE, 0.25f);
    }
#undef AT
}
