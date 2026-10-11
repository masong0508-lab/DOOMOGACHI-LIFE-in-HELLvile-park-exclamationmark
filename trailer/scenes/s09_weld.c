/* s09_weld.c - Rivet's workshop.   (4.5 s, 135 frames)
 *
 * f  0- 69  Rivet (welding mask down) builds a rifle on the bench. Parts fly
 *           in and CLANK into place - barrel, stock, magazine, scope, a heart
 *           sticker, a charm - and get welded. Sparks bounce off the bench,
 *           the arc lights up the room. "WELD YOUR WEAPON."
 *           Gag: a stray spark lands on Rivet's tail. The "DAYS SINCE LAST
 *           FIRE" sign flips 7 -> 0. Rivet flips the mask up: big grin,
 *           thumbs up. (Tail still on fire.)
 * f 70-134  split-screen face-off: CUTE - a pastel PLUSH LAUNCHER (stickers,
 *           teddy ammo peeking out, teddy charm) vs BRUTAL - a gunmetal
 *           SUPER SHOTGUN (spikes, skull charm). Nine part slots pop in
 *           under each. "(9 part slots. Millions of guns.)" The teddy
 *           notices the shotgun. Sweats. Quietly retreats into the barrel.
 */
#include "tk.h"
#include "pal.h"
#include <math.h>
#include <string.h>

#define F_SPLIT 70
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
static uint8_t L_SHADOW[256], L_DIM[256], L_WARM[3][256], L_ARC[4][256], L_LAVA[3][256], L_WHITE[3][256], L_PINK[2][256];
static int luts_ready;
static void luts_init(void) {
    if (luts_ready) return;
    luts_ready = 1;
    lut_make(L_SHADOW, 70, 40, 120, LM_MUL, 0.45f);
    lut_make(L_DIM, 60, 40, 90, LM_MUL, 0.25f);
    for (int i = 0; i < 4; i++) lut_make(L_DARK[i], 60, 40, 110, LM_MUL, 0.12f + 0.11f * i);
    for (int i = 0; i < 3; i++) lut_make(L_WARM[i], 255, 200, 120, LM_ADD, 0.08f + 0.08f * i);
    for (int i = 0; i < 4; i++) lut_make(L_ARC[i], 170, 220, 255, LM_ADD, 0.12f + 0.14f * i);
    for (int i = 0; i < 3; i++) lut_make(L_LAVA[i], 255, 110, 30, LM_ADD, 0.12f + 0.12f * i);
    for (int i = 0; i < 3; i++) lut_make(L_WHITE[i], 255, 255, 255, LM_MIX, 0.25f + 0.2f * i);
    for (int i = 0; i < 2; i++) lut_make(L_PINK[i], 255, 220, 240, LM_MIX, 0.25f + 0.25f * i);
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
/* a thick shaded limb / tube between two points (rounded caps) */
KIT void limb(float x0, float y0, float x1, float y1, float r, Sh s) {
    float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy + 1e-6f, len = sqrtf(L2);
    float nx = -dy / len, ny = dx / len;          /* normal; light from the "up" side */
    if (ny > 0) { nx = -nx; ny = -ny; }
    int minx = (int)floorf(fminf(x0, x1) - r - 2), maxx = (int)ceilf(fmaxf(x0, x1) + r + 2);
    int miny = (int)floorf(fminf(y0, y1) - r - 2), maxy = (int)ceilf(fmaxf(y0, y1) + r + 2);
    for (int y = miny; y <= maxy; y++)
        for (int x = minx; x <= maxx; x++) {
            float px = x + .5f - x0, py = y + .5f - y0;
            float t = (px * dx + py * dy) / L2;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            float qx = px - dx * t, qy = py - dy * t;
            float d = sqrtf(qx * qx + qy * qy);
            if (d > r + 1) continue;
            if (d > r) { if (s.o) tk_px(x, y, s.o); continue; }
            float side = (qx * nx + qy * ny) / r;      /* +1 lit side, -1 shadow side */
            uint8_t c = side > 0.45f ? s.l : side < -0.4f ? s.d : s.b;
            if (side > 0.75f && s.h && r > 2.5f) c = s.h;
            tk_px(x, y, c);
        }
}
/* box with top light / bottom shade and a coloured outline */
KIT void box(int x, int y, int w, int h, Sh s) {
    tk_rect(x - 1, y - 1, w + 2, h + 2, s.o);
    tk_rect(x, y, w, h, s.b);
    tk_rect(x, y, w, 1, s.l);
    if (h > 3) tk_rect(x, y + h - 2, w, 2, s.d); else tk_rect(x, y + h - 1, w, 1, s.d);
    if (s.h && w > 4) tk_rect(x + 1, y, w / 3, 1, s.h);
}
/* rounded-rectangle plate (superellipse), shaded top-lit */
KIT void plate(float cx, float cy, float a, float b, Sh s) {
    for (int y = (int)(cy - b - 2); y <= (int)(cy + b + 2); y++)
        for (int x = (int)(cx - a - 2); x <= (int)(cx + a + 2); x++) {
            float u = (x + .5f - cx), v = (y + .5f - cy);
            float uo = u / (a + 1), vo = v / (b + 1), ui = u / a, vi = v / b;
            if (uo * uo * uo * uo + vo * vo * vo * vo > 1) continue;
            if (ui * ui * ui * ui + vi * vi * vi * vi > 1) { tk_px(x, y, s.o); continue; }
            uint8_t c = s.b;
            if (vi < -0.6f) c = s.l;
            else if (vi > 0.55f || ui > 0.8f) c = s.d;
            if (vi < -0.75f && ui < -0.2f && ui > -0.7f && s.h) c = s.h;
            tk_px(x, y, c);
        }
}
typedef struct { float x, y, r; } Puff;
KIT void cloud(const Puff *p, int n, Sh s) {
    if (s.o) for (int i = 0; i < n; i++) if (p[i].r > 0) disc(p[i].x, p[i].y, p[i].r + 1.2f, s.o);
    Sh t = s; t.o = 0;
    for (int i = 0; i < n; i++) if (p[i].r > 0) ball(p[i].x, p[i].y, p[i].r, p[i].r, t);
}
static uint8_t SAVE_FB[TH][TW];
static uint8_t LAYER_A[TH][TW];
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
static void slam_caption(const char *s, int cy, uint8_t c, uint8_t oc, int scale, int age) {
    if (age < 0) return;
    layer_begin();
    tk_caption(80, s, c, oc, scale);
    layer_end(LAYER_A);
    float k = age < 7 ? 1.0f + (1.0f - tk_bounce(age / 7.0f)) * 1.2f : 1.0f;
    layer_draw(LAYER_A, 120, 80, 120, (float)cy, k, k, 0, 0, TH);
}

/* ========================================================================= */
/* shot A: the workshop                                                       */
/* ========================================================================= */
#define BENCH_Y 112

/* the rifle under construction, part by part; positions relative to (gx,gy) */
enum { P_RECV, P_BARREL, P_STOCK, P_MAG, P_SCOPE, P_STICKER, P_CHARM, P_COUNT };
static const int P_ARRIVE[P_COUNT] = { -1, 8, 18, 27, 35, 43, 49 };
static const float P_FROM[P_COUNT][2] = { { 0, 0 }, { 160, -10 }, { -150, -20 }, { 0, 80 }, { 10, -120 }, { 60, -60 }, { -40, -80 } };
/* weld point for each joint (relative to gun origin) */
static const float P_WELD[P_COUNT][2] = { { 10, 4 }, { 26, 2 }, { -16, 3 }, { 6, 10 }, { 5, -7 }, { 12, 2 }, { 0, 10 } };

static void part_draw(int p, float x, float y, int f) {
    Sh steel = { PAL_RAMP(RAMP_STEEL, 0), PAL_RAMP(RAMP_STEEL, 2), PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 5), PAL_RAMP(RAMP_STEEL, 7) };
    Sh dark = { PAL_RAMP(RAMP_STEEL, 0), PAL_RAMP(RAMP_STEEL, 1), PAL_RAMP(RAMP_STEEL, 2), PAL_RAMP(RAMP_STEEL, 4), 0 };
    Sh mint = { PAL_RAMP(RAMP_MINT, 2), PAL_RAMP(RAMP_MINT, 3), PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_MINT, 7) };
    int X = (int)x, Y = (int)y;
    switch (p) {
    case P_RECV:
        box(X - 6, Y - 4, 30, 11, steel);
        box(X + 16, Y + 7, 6, 8, dark);                        /* grip */
        tk_rect(X + 12, Y + 8, 4, 1, PAL_RAMP(RAMP_STEEL, 1));    /* trigger guard */
        tk_rect(X + 12, Y + 8, 1, 4, PAL_RAMP(RAMP_STEEL, 1));
        tk_rect(X - 2, Y - 1, 14, 2, PAL_RAMP(RAMP_STEEL, 1));    /* ejection port */
        break;
    case P_BARREL:
        box(X + 24, Y - 3, 22, 8, mint);                       /* handguard */
        for (int i = 0; i < 4; i++) tk_rect(X + 27 + i * 5, Y - 1, 2, 4, PAL_RAMP(RAMP_MINT, 3));
        limb(X + 46, Y, X + 66, Y, 1.6f, steel);               /* barrel */
        box(X + 64, Y - 2, 5, 5, dark);                        /* muzzle */
        break;
    case P_STOCK:
        for (int j = 0; j < 12; j++) {
            int w = 22 + j / 3;
            tk_rect(X - 6 - w, Y - 3 + j, w, 1, j < 2 ? PAL_RAMP(RAMP_WOOD, 6) : j > 9 ? PAL_RAMP(RAMP_WOOD, 3) : PAL_RAMP(RAMP_WOOD, 5));
        }
        tk_rect(X - 34, Y - 4, 28, 1, PAL_RAMP(RAMP_WOOD, 2));
        tk_rect(X - 35, Y - 3, 1, 13, PAL_RAMP(RAMP_WOOD, 2));
        tk_rect(X - 32, Y + 9, 26, 1, PAL_RAMP(RAMP_WOOD, 2));
        tk_rect(X - 34, Y + 2, 3, 6, PAL_RAMP(RAMP_WOOD, 2));   /* butt pad */
        break;
    case P_MAG:
        for (int j = 0; j < 14; j++) tk_rect(X + 2 + j / 4, Y + 7 + j, 7, 1, j == 0 ? PAL_RAMP(RAMP_STEEL, 5) : (j & 3) == 3 ? PAL_RAMP(RAMP_STEEL, 1) : PAL_RAMP(RAMP_STEEL, 2));
        tk_rect(X + 1, Y + 7, 1, 14, PAL_RAMP(RAMP_STEEL, 0));
        break;
    case P_SCOPE:
        tk_rect(X + 2, Y - 7, 3, 3, PAL_RAMP(RAMP_STEEL, 1));
        tk_rect(X + 12, Y - 7, 3, 3, PAL_RAMP(RAMP_STEEL, 1));
        limb(X - 2, Y - 10, X + 18, Y - 10, 2.6f, dark);
        disc(X + 20, Y - 10, 2.4f, PAL_PAINT(PAINT_CYAN, 1));
        tk_px(X + 19, Y - 11, PAL_WHITE);
        if ((f / 5) & 1) tk_px(X + 21, Y - 10, PAL_PAINT(PAINT_CYAN, 2));
        break;
    case P_STICKER:
        tk_heart(X + 6, Y + 2, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        break;
    case P_CHARM: {
        float sw = sinf(f * 0.25f) * 0.5f;
        float ex = X + sinf(sw) * 9, ey = Y + 14 + cosf(sw) * 9 - 9;
        tk_line(X, Y + 13, (int)ex, (int)ey + 9, PAL_GRAY);
        ball(ex, ey + 12, 3, 3, SH_PAINT(PAINT_CARAMEL));
        disc(ex - 2, ey + 9.5f, 1.2f, PAL_PAINT(PAINT_CARAMEL, 1));
        disc(ex + 2, ey + 9.5f, 1.2f, PAL_PAINT(PAINT_CARAMEL, 1));
        tk_px((int)ex - 1, (int)ey + 12, PAL_PAINT(PAINT_JET, 0)); tk_px((int)ex + 1, (int)ey + 12, PAL_PAINT(PAINT_JET, 0));
        break; }
    }
}

static void gun_assembly(int f, float gx, float gy, float *wx, float *wy, int *arc) {
    *arc = 0; *wx = gx + P_WELD[0][0]; *wy = gy + P_WELD[0][1];
    /* jig */
    tk_rect((int)gx - 2, (int)gy + 14, 4, BENCH_Y - (int)gy - 14, PAL_RAMP(RAMP_STEEL, 2));
    tk_rect((int)gx - 1, (int)gy + 14, 1, BENCH_Y - (int)gy - 14, PAL_RAMP(RAMP_STEEL, 4));
    tk_rect((int)gx - 9, BENCH_Y - 3, 18, 3, PAL_RAMP(RAMP_STEEL, 1));
    for (int p = 0; p < P_COUNT; p++) {
        int a = f - P_ARRIVE[p];
        if (p > 0 && a < -10) continue;
        float x = gx, y = gy;
        if (p > 0 && a < 0) {          /* flying in, easing */
            float k = 1.0f - (float)(-a) / 10.0f;
            k = k * k;
            x += P_FROM[p][0] * (1 - k); y += P_FROM[p][1] * (1 - k);
        }
        float sq = (p > 0 && a >= 0 && a < 3) ? (3 - a) : 0;  /* clank jolt */
        part_draw(p, x + (p == P_BARREL ? -sq : p == P_STOCK ? sq : 0), y + (p == P_SCOPE ? sq : p == P_MAG ? -sq : 0), f);
    }
    /* which seam is Rivet welding right now? */
    for (int p = 0; p < P_COUNT - 2; p++) {
        int s0 = p == 0 ? 0 : P_ARRIVE[p] + 1, s1 = p == 0 ? 6 : P_ARRIVE[p] + 7;
        if (f >= s0 && f < s1) { *arc = 1; *wx = gx + P_WELD[p][0]; *wy = gy + P_WELD[p][1]; }
    }
    if (f >= P_ARRIVE[P_STICKER] - 4 && !*arc) { *wx = gx + 30; *wy = gy + 6; }
}

/* sparks: deterministic, simulated from birth, bouncing off the bench */
static void sparks(int f, int arc_on_since, float sx, float sy, uint32_t seed, int emit_until) {
    for (int t0 = f - 22; t0 <= f; t0++) {
        if (t0 < arc_on_since || t0 >= emit_until) continue;
        uint32_t s = tk_hash((uint32_t)t0 * 7919u + seed);
        for (int k = 0; k < 4; k++) {
            float vx = (tk_rand(&s, 200) - 100) / 28.0f, vy = -(tk_rand(&s, 160) + 30) / 40.0f;
            float x = sx, y = sy, px = x, py = y;
            int age = f - t0;
            for (int i = 0; i < age; i++) {
                px = x; py = y;
                x += vx; y += vy; vy += 0.32f;
                if (y > BENCH_Y - 1 && vy > 0) { y = BENCH_Y - 1; vy *= -0.45f; vx *= 0.7f; }
            }
            uint8_t c = age < 4 ? PAL_WHITE : age < 8 ? PAL_RAMP(RAMP_CANDY, 7) : age < 13 ? PAL_RAMP(RAMP_LAVA, 6) : PAL_RAMP(RAMP_LAVA, 4);
            if (age > 18 && (k & 1)) continue;
            tk_line((int)px, (int)py, (int)x, (int)y, c);
        }
    }
}

static void window_hell(int f) {
    int x0 = 10, y0 = 10, w = 50, h = 44;
    /* frame */
    tk_rect(x0 - 3, y0 - 3, w + 6, h + 6, PAL_RAMP(RAMP_WOOD, 1));
    tk_rect(x0 - 2, y0 - 2, w + 4, h + 4, PAL_RAMP(RAMP_WOOD, 4));
    /* hell sky outside */
    static const uint8_t g[] = { PAL_RAMP(RAMP_BLOOD, 1), PAL_RAMP(RAMP_BLOOD, 3), PAL_RAMP(RAMP_LAVA, 3), PAL_RAMP(RAMP_LAVA, 5) };
    for (int y = 0; y < h; y++) {
        int b = y * 4 / h;
        for (int x = 0; x < w; x++) {
            uint8_t c = g[b];
            if (b < 3 && (y * 4) % h > h - 8 && BY[y & 3][x & 3] < 8) c = g[b + 1];
            tk_px(x0 + x, y0 + y, c);
        }
    }
    /* a familiar crowned cloud on the horizon */
    Sh vs = { PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 4), 0 };
    float cx = x0 + 30 + sinf(f * 0.05f), cy = y0 + 30;
    Puff p[5] = { { cx - 8, cy + 2, 6 }, { cx + 8, cy + 2, 6 }, { cx, cy - 2, 8 }, { cx - 14, cy + 6, 4 }, { cx + 14, cy + 6, 4 } };
    cloud(p, 5, vs);
    tk_rect((int)cx - 4, (int)cy - 12, 9, 2, PAL_RAMP(RAMP_CANDY, 4));
    tk_px((int)cx - 4, (int)cy - 13, PAL_RAMP(RAMP_CANDY, 4)); tk_px((int)cx, (int)cy - 14, PAL_RAMP(RAMP_CANDY, 4)); tk_px((int)cx + 4, (int)cy - 13, PAL_RAMP(RAMP_CANDY, 4));
    tk_px((int)cx - 3, (int)cy - 1, PAL_UI_RED); tk_px((int)cx + 3, (int)cy - 1, PAL_UI_RED);
    /* burnt rooftops */
    for (int i = 0; i < 5; i++) { int bx = x0 + i * 11, bh = 6 + (i * 5 % 7); tk_rect(bx, y0 + h - bh, 9, bh, PAL_RAMP(RAMP_BLOOD, 0)); tk_tri(bx - 1, y0 + h - bh, bx + 9, y0 + h - bh, bx + 4, y0 + h - bh - 5, PAL_RAMP(RAMP_BLOOD, 0)); }
    /* mullions + glass glint */
    tk_rect(x0 + w / 2, y0, 2, h, PAL_RAMP(RAMP_WOOD, 4));
    tk_rect(x0, y0 + h / 2, w, 2, PAL_RAMP(RAMP_WOOD, 4));
    tk_line(x0 + 4, y0 + 14, x0 + 14, y0 + 4, PAL_RAMP(RAMP_LAVA, 6));
    tk_rect(x0 - 4, y0 + h + 3, w + 8, 3, PAL_RAMP(RAMP_WOOD, 5));
}

static void pegboard(int f) {
    int x0 = 172, y0 = 8, w = 62, h = 50;
    tk_rect(x0 - 1, y0 - 1, w + 2, h + 2, PAL_RAMP(RAMP_WOOD, 2));
    tk_rect(x0, y0, w, h, PAL_RAMP(RAMP_CANDY, 3));
    for (int y = y0 + 3; y < y0 + h; y += 6) for (int x = x0 + 3; x < x0 + w; x += 6) tk_px(x, y, PAL_RAMP(RAMP_CANDY, 1));
    /* wrench */
    Sh steel = { PAL_RAMP(RAMP_STEEL, 1), PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 5), PAL_RAMP(RAMP_STEEL, 6), PAL_RAMP(RAMP_STEEL, 7) };
    limb(x0 + 9, y0 + 10, x0 + 9, y0 + 40, 1.8f, steel);
    disc(x0 + 9, y0 + 9, 4, PAL_RAMP(RAMP_STEEL, 5)); disc(x0 + 9, y0 + 7, 2, PAL_RAMP(RAMP_CANDY, 3));
    /* hammer */
    limb(x0 + 22, y0 + 14, x0 + 22, y0 + 42, 1.6f, (Sh){ PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6), 0 });
    box(x0 + 16, y0 + 8, 13, 6, steel);
    /* saw */
    for (int i = 0; i < 18; i++) tk_rect(x0 + 32 + i, y0 + 10, 1, 8 + i / 3, PAL_RAMP(RAMP_STEEL, 5 + (i == 0)));
    for (int i = 0; i < 18; i += 2) tk_px(x0 + 32 + i, y0 + 18 + i / 3, PAL_RAMP(RAMP_STEEL, 2));
    box(x0 + 48, y0 + 9, 6, 10, (Sh){ PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5), 0 });
    /* a plush teddy hanging among the tools (it's that kind of shop) */
    float sw = sinf(f * 0.12f) * 1.5f;
    tk_line(x0 + 44, y0 + 26, (int)(x0 + 44 + sw), y0 + 31, PAL_GRAY_D);
    ball(x0 + 44 + sw, y0 + 41, 5, 5, SH_PAINT(PAINT_CARAMEL));
    ball(x0 + 44 + sw, y0 + 34, 4.5f, 4, SH_PAINT(PAINT_CARAMEL));
    disc(x0 + 40 + sw, y0 + 31, 1.6f, PAL_PAINT(PAINT_CARAMEL, 0)); disc(x0 + 48 + sw, y0 + 31, 1.6f, PAL_PAINT(PAINT_CARAMEL, 0));
    tk_px((int)(x0 + 42 + sw), y0 + 34, PAL_PAINT(PAINT_JET, 0)); tk_px((int)(x0 + 46 + sw), y0 + 34, PAL_PAINT(PAINT_JET, 0));
    tk_px((int)(x0 + 44 + sw), y0 + 36, PAL_PAINT(PAINT_CHOCOLATE, 0));
}

static void fire_sign(int f) {
    int x0 = 162, y0 = 63, w = 74;
    tk_rect(x0 - 1, y0 - 1, w + 2, 26, PAL_RAMP(RAMP_WOOD, 1));
    tk_rect(x0, y0, w, 24, PAL_WHITE);
    tk_rect(x0, y0 + 22, w, 2, PAL_PAINT(PAINT_WHITE, 0));
    tk_rect(x0, y0, w, 5, PAL_UI_RED);
    tk_text(x0 + 2, y0 + 6, "DAYS SINCE", PAL_GRAY_D, 1);
    tk_text(x0 + 2, y0 + 14, "LAST FIRE", PAL_GRAY_D, 1);
    /* the counter tile */
    int nx = x0 + 63 + (f >= 46 && f < 50 ? ((f & 1) ? 1 : -1) : 0), ny = y0 + 8;
    tk_rect(nx - 1, ny - 1, 11, 13, PAL_GRAY_D);
    tk_rect(nx, ny, 9, 11, PAL_BLACK);
    const char *d = f < 46 ? "7" : (f < 48 ? "-" : "0");
    tk_text(nx + 2, ny + 2, d, f >= 48 ? PAL_UI_RED : PAL_WHITE, 1);
    if (f >= 48 && (f / 4) & 1) light_pool(nx + 4, ny + 5, 10, 10, L_LAVA, 1, 1.0f);
}

static void flames(float x, float y, int f, float s) {
    for (int i = 0; i < 5; i++) {
        float ph = fmodf(f * 0.35f + i * 0.37f, 1.0f);
        float fx = x + sinf(i * 2.1f + f * 0.4f) * 3 * s, fy = y - ph * 12 * s;
        float r = (3.2f - ph * 2.5f) * s;
        if (r < 0.6f) continue;
        disc(fx, fy, r + 0.8f, PAL_RAMP(RAMP_LAVA, 3));
        disc(fx, fy, r, ph < 0.4f ? PAL_RAMP(RAMP_LAVA, 6) : PAL_RAMP(RAMP_LAVA, 5));
        if (ph < 0.25f) disc(fx, fy + 0.5f, r * 0.5f, PAL_RAMP(RAMP_LAVA, 7));
    }
}

static void rivet(int f, float wx, float wy, int arc) {
    const int X = 120, YF = 132;
    int up = f >= 54;                  /* mask flipped up */
    int fire = f >= 41;
    int expr = up ? TKE_HAPPY : TKE_NORMAL, mouth = up ? TKM_OPEN : TKM_FLAT;
    layer_begin();
    tk_chibi(X, YF, &TK_RIVET, 3, expr, mouth, f + 11, 0);
    layer_end(LAYER_A);
    layer_draw(LAYER_A, 0, 0, 0, 0, 1, 1, 0, 0, BENCH_Y);
    if (fire) {   /* the tail, merrily ablaze, poking up behind the bench */
        flames(X + 40, BENCH_Y - 2, f, 1.7f);
        for (int i = 0; i < 3; i++) {
            float k = fmodf(f * 0.04f + i * 0.33f, 1.0f);
            haze(X + 40 + sinf(k * 6 + i) * 4, BENCH_Y - 20 - k * 50, 3 + k * 6, 2 + k * 4, L_DIM);
        }
    }
    /* the welding mask: a flat shield over the face (ears + fluff stay out) */
    float hy = YF + (12.2f - 34.1f) * 3;           /* head centre y */
    Sh steel = { PAL_RAMP(RAMP_STEEL, 0), PAL_RAMP(RAMP_STEEL, 2), PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 5), PAL_RAMP(RAMP_STEEL, 6) };
    if (!up || f < 58) {
        float k = up ? tk_ease((f - 54) / 4.0f) : 0;   /* 0 down .. 1 flipped up */
        float my = hy + 5 - k * 24, sy = 1 - k * 0.72f;
        /* head straps */
        tk_rect(X - 26, (int)(hy - 2 - k * 10), 52, 3, PAL_RAMP(RAMP_STEEL, 1));
        plate(X, my, 19, 15 * sy, steel);
        if (sy > 0.5f) {
            int vy = (int)(my - 4 * sy);
            tk_rect(X - 14, vy - 4, 28, (int)(9 * sy), PAL_RAMP(RAMP_STEEL, 0));
            tk_rect(X - 13, vy - 3, 26, (int)(7 * sy), arc && (f & 1) ? PAL_PAINT(PAINT_ECTOPLASM, 1) : PAL_PAINT(PAINT_FOREST, 0));
            if (arc) tk_rect(X - 8 + (int)((wx - X) * 0.12f), vy - 1, 3, 2, PAL_WHITE);
            tk_rect(X - 12, vy - 3, 7, 1, PAL_PAINT(PAINT_FOREST, 2));
            disc(X - 14, my + 8 * sy, 1.3f, PAL_RAMP(RAMP_STEEL, 6)); disc(X + 14, my + 8 * sy, 1.3f, PAL_RAMP(RAMP_STEEL, 6));
            tk_rect(X - 6, (int)(my + 9 * sy), 12, 1, PAL_RAMP(RAMP_STEEL, 1));
        }
    } else {
        /* parked on the forehead */
        tk_rect(X - 26, (int)hy - 12, 52, 3, PAL_RAMP(RAMP_STEEL, 1));
        plate(X, hy - 19, 18, 5, steel);
        tk_rect(X - 12, (int)hy - 20, 24, 2, PAL_PAINT(PAINT_FOREST, 0));
    }
    /* arms: left paw steadies the gun, right paw holds the torch */
    Sh fur = { PAL_PAINT(PAINT_CHERRY, 0), PAL_PAINT(PAINT_CHERRY, 0), PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_CHERRY, 2), 0 };
    float sxL = X - 16, syL = YF - 30, sxR = X + 16, syR = YF - 30;
    if (up && f >= 58) {
        /* thumbs up! */
        float k = tk_bounce((f - 58) / 6.0f);
        float px = X + 34, py = syR - 22 * k;
        limb(sxR, syR, px, py, 4, fur);
        ball(px, py, 5, 5, fur);
        limb(px - 1, py - 3, px - 1, py - 10, 2.2f, fur);
        tk_sparkles((int)px, (int)py - 8, f, 5, 8);
        limb(sxL, syL, X - 26, BENCH_Y - 4, 4, fur);
        ball(X - 26, BENCH_Y - 4, 5, 4, fur);
        return;
    }
    limb(sxL, syL, X - 30, BENCH_Y - 10, 4, fur);
    ball(X - 30, BENCH_Y - 10, 5, 4, fur);
    /* torch: hose + nozzle */
    float tx = wx + 7, ty = wy - 6;
    for (int i = 0; i < 20; i++) {
        float t = i / 20.0f;
        tk_px((int)(sxR + 30 + t * 30), (int)(BENCH_Y - 2 + sinf(t * 3.14f) * 10), PAL_GRAY_D);
    }
    limb(sxR, syR, tx + 4, ty + 3, 4, fur);
    limb(tx + 4, ty + 3, wx + 1, wy - 1, 1.6f, (Sh){ PAL_RAMP(RAMP_STEEL, 0), PAL_RAMP(RAMP_STEEL, 2), PAL_RAMP(RAMP_STEEL, 4), PAL_RAMP(RAMP_STEEL, 6), 0 });
    ball(tx + 5, ty + 4, 5, 4, fur);
}

static void shot_workshop(int f) {
    /* back wall: warm planks */
    for (int y = 0; y < BENCH_Y; y++)
        for (int x = 0; x < TW; x++) {
            int plank = x / 16;
            uint8_t c = (plank & 1) ? PAL_RAMP(RAMP_WOOD, 2) : PAL_RAMP(RAMP_WOOD, 3);
            if (x % 16 == 0) c = PAL_RAMP(RAMP_WOOD, 1);
            if (x % 16 == 1) c = PAL_RAMP(RAMP_WOOD, 4);
            if (((x * 13 + y / 7 * 7) % 97) == 0) c = PAL_RAMP(RAMP_WOOD, 1);   /* knots */
            tk_fb[y][x] = c;
        }
    window_hell(f);
    pegboard(f);
    fire_sign(f);
    /* red light shaft from the window */
    for (int y = 10; y < BENCH_Y; y++)
        for (int x = 0; x < 120; x++) {
            float u = (x - 10) - (y - 10) * 0.9f;
            if (u > 0 && u < 50 && BY[y & 3][x & 3] < 5) lut_px(x, y, L_LAVA[0]);
        }
    /* hanging lamp */
    float sw = sinf(f * 0.08f) * 2;
    tk_line(120, 0, (int)(120 + sw), 6, PAL_GRAY_D);

    float gx = 110, gy = 92, wx, wy;
    int arc;
    /* bench */
    tk_rect(0, BENCH_Y, TW, 6, PAL_RAMP(RAMP_WOOD, 5));
    tk_rect(0, BENCH_Y, TW, 1, PAL_RAMP(RAMP_WOOD, 7));
    tk_rect(0, BENCH_Y + 6, TW, 1, PAL_RAMP(RAMP_WOOD, 2));
    for (int y = BENCH_Y + 7; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            uint8_t c = PAL_RAMP(RAMP_WOOD, 3);
            if (x % 60 == 0 || x % 60 == 59) c = PAL_RAMP(RAMP_WOOD, 1);
            if (y == BENCH_Y + 24) c = PAL_RAMP(RAMP_WOOD, 1);
            tk_fb[y][x] = c;
        }
    for (int i = 0; i < 4; i++) { tk_rect(26 + i * 60, BENCH_Y + 14, 8, 3, PAL_RAMP(RAMP_STEEL, 4)); tk_rect(26 + i * 60, BENCH_Y + 31, 8, 3, PAL_RAMP(RAMP_STEEL, 4)); }
    /* Rivet first (stands behind the bench), then the gun on its jig */
    gun_assembly(f, gx, gy, &wx, &wy, &arc);   /* to get the weld point */
    rivet(f, wx, wy, arc);
    /* bench top again in front of Rivet's body */
    tk_rect(0, BENCH_Y, TW, 6, PAL_RAMP(RAMP_WOOD, 5));
    tk_rect(0, BENCH_Y, TW, 1, PAL_RAMP(RAMP_WOOD, 7));
    tk_rect(0, BENCH_Y + 6, TW, 1, PAL_RAMP(RAMP_WOOD, 2));
    /* clutter: a vise, bolts, a mug */
    tk_rect(190, BENCH_Y - 10, 14, 10, PAL_RAMP(RAMP_STEEL, 2)); tk_rect(190, BENCH_Y - 10, 14, 1, PAL_RAMP(RAMP_STEEL, 5));
    tk_rect(186, BENCH_Y - 6, 22, 3, PAL_RAMP(RAMP_STEEL, 3));
    for (int i = 0; i < 4; i++) disc(28 + i * 5, BENCH_Y - 1, 1.5f, PAL_RAMP(RAMP_STEEL, 5));
    tk_rect(214, BENCH_Y - 11, 10, 11, PAL_PAINT(PAINT_BUBBLEGUM, 1)); tk_rect(214, BENCH_Y - 11, 10, 1, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    tk_rect(224, BENCH_Y - 9, 2, 6, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_text(216, BENCH_Y - 9, "#1", PAL_WHITE, 1);
    gun_assembly(f, gx, gy, &wx, &wy, &arc);
    /* lamp shade + light cone */
    {
        float lx = 120 + sw * 3;
        for (int y = 14; y < BENCH_Y; y++) {
            float half = 8 + (y - 14) * 0.62f;
            for (int x = (int)(lx - half); x <= (int)(lx + half); x++) {
                float e = 1 - fabsf(x - lx) / half;
                if (BY[y & 3][x & 3] < (int)(e * 10)) lut_px(x, y, L_WARM[0]);
            }
        }
        ball(lx, 10, 12, 6, (Sh){ PAL_RAMP(RAMP_STEEL, 0), PAL_PAINT(PAINT_MINT, 0), PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_MINT, 2), PAL_WHITE });
        tk_rect((int)lx - 10, 14, 21, 2, PAL_RAMP(RAMP_STEEL, 1));
        disc(lx, 15, 3, PAL_RAMP(RAMP_CANDY, 7));
        /* dust motes in the cone */
        for (int i = 0; i < 10; i++) {
            uint32_t s = tk_hash((uint32_t)i * 37u + 5u);
            float my = 20 + fmodf(tk_rand(&s, 90) + f * 0.3f, 90);
            float half = 8 + (my - 14) * 0.62f;
            float mx = lx + (tk_rand(&s, 200) / 100.0f - 1) * half * 0.8f + sinf(f * 0.05f + i) * 2;
            if ((f / 6 + i) % 3) tk_px((int)mx, (int)my, PAL_RAMP(RAMP_CANDY, 6));
        }
    }
    /* the arc: blinding, flickering, lights the room */
    if (arc) {
        int fl = (f * 7) % 3;
        light_pool(wx, wy, 54 + fl * 6, 40 + fl * 4, L_ARC, 2 + (fl == 0), 2.2f);
        disc(wx, wy, 3.5f + fl * 0.5f, PAL_PAINT(PAINT_CYAN, 2));
        disc(wx, wy, 2.0f, PAL_WHITE);
        tk_star((int)wx, (int)wy, 6 + fl * 2, PAL_WHITE);
    }
    /* sparks from every weld */
    for (int p = 0; p < P_COUNT - 2; p++) {
        int s0 = p == 0 ? 0 : P_ARRIVE[p] + 1, s1 = p == 0 ? 6 : P_ARRIVE[p] + 7;
        sparks(f, s0, gx + P_WELD[p][0], gy + P_WELD[p][1], 100u + p, s1);
    }
    /* the fateful spark: arcs from the scope weld onto Rivet's tail */
    if (f >= 37 && f < 42) {
        float k = (f - 37) / 5.0f;
        float x = gx + 5 + (156 - gx - 5) * k, y = gy - 7 - sinf(k * 3.14159f) * 20 + k * 10;
        tk_rect((int)x, (int)y, 2, 2, PAL_RAMP(RAMP_CANDY, 7));
        tk_px((int)x - 1, (int)y + 1, PAL_RAMP(RAMP_LAVA, 5));
    }
    /* clank flashes */
    for (int p = 1; p < P_COUNT; p++) {
        int a = f - P_ARRIVE[p];
        if (a >= 0 && a < 4) tk_star((int)(gx + P_WELD[p][0]), (int)(gy + P_WELD[p][1]), 6 - a, PAL_WHITE);
    }
    /* finished: the gun gleams */
    if (f >= 58) {
        int g = (f - 58) * 9;
        for (int y = (int)gy - 16; y < (int)gy + 24; y++)
            for (int x = (int)gx - 40; x < (int)gx + 80; x++) {
                int u = x - (int)gx + 40 + (y - (int)gy);
                if (u > g - 6 && u < g && tk_get(x, y) >= PAL_RAMP(RAMP_STEEL, 0)) lut_px(x, y, L_WHITE[1]);
            }
        tk_sparkles((int)gx + 20, (int)gy, f, 12, 22);
    }
    /* moody room: darken the corners, keep the lamp + arc as key lights */
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float fx = (x - 120) / 130.0f, fy = (y - 86) / 100.0f, d = fx * fx + fy * fy;
            if (d < 0.35f) continue;
            int lv = (int)((d - 0.35f) * 6.0f + (BY[y & 3][x & 3] + .5f) / 16.0f);
            if (lv > 4) lv = 4;
            if (lv > 0) lut_px(x, y, L_DARK[lv - 1]);
        }
    /* caption on the bench front */
    if (f >= 10) {
        lut_rect(0, 128, TW, 24, L_DARK[1]);
        slam_caption("WELD YOUR WEAPON.", 140, PAL_WHITE, PAL_RAMP(RAMP_WOOD, 0), 2, f - 10);
    }
    /* global flicker from the arc */
    if (arc && (f % 4) == 0) light_pool(wx, wy, 200, 150, L_ARC, 1, 0.6f);
    int sh = 0;
    for (int p = 1; p < P_COUNT - 2; p++) if (f - P_ARRIVE[p] >= 0 && f - P_ARRIVE[p] < 2) sh = 2;
    if (f >= 10 && f < 13) sh = 3;
    if (sh) tk_post_shake((f & 1) ? sh : -sh, 0);
}

/* ========================================================================= */
/* shot B: CUTE vs BRUTAL                                                    */
/* ========================================================================= */
static void teddy_head(float x, float y, float r, int look, int sweat, int f) {
    Sh fur = { PAL_PAINT(PAINT_CHOCOLATE, 1), PAL_PAINT(PAINT_CARAMEL, 0), PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CARAMEL, 2), PAL_PAINT(PAINT_HONEY, 2) };
    ball(x - r * 0.7f, y - r * 0.75f, r * 0.38f, r * 0.38f, fur);
    ball(x + r * 0.7f, y - r * 0.75f, r * 0.38f, r * 0.38f, fur);
    disc(x - r * 0.7f, y - r * 0.72f, r * 0.18f, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    disc(x + r * 0.7f, y - r * 0.72f, r * 0.18f, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    ball(x, y, r, r * 0.92f, fur);
    ball(x, y + r * 0.38f, r * 0.45f, r * 0.32f, (Sh){ 0, PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_HONEY, 2), PAL_PAINT(PAINT_BONE, 2), 0 });
    disc(x, y + r * 0.25f, r * 0.13f + 0.5f, PAL_PAINT(PAINT_CHOCOLATE, 0));
    float ex = r * 0.38f, ey = y - r * 0.12f, lx = look * r * 0.1f;
    disc(x - ex + lx, ey, r * 0.16f + 0.5f, PAL_PAINT(PAINT_JET, 0));
    disc(x + ex + lx, ey, r * 0.16f + 0.5f, PAL_PAINT(PAINT_JET, 0));
    tk_px((int)(x - ex + lx), (int)ey - 1, PAL_WHITE);
    tk_px((int)(x + ex + lx), (int)ey - 1, PAL_WHITE);
    tk_px((int)(x - r * 0.62f), (int)(y + r * 0.3f), PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_px((int)(x + r * 0.62f), (int)(y + r * 0.3f), PAL_PAINT(PAINT_BUBBLEGUM, 1));
    if (sweat) {
        float d = (f % 12) * 0.4f;
        disc(x - r * 0.95f, y - r * 0.4f + d, 1.6f, PAL_PAINT(PAINT_SKY, 2));
        tk_px((int)(x - r * 0.95f), (int)(y - r * 0.4f + d) - 2, PAL_PAINT(PAINT_SKY, 2));
    }
}

static void plush_launcher(float x, float y, int f, int teddy_state, int look) {
    Sh pink = { PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_BUBBLEGUM, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
    Sh mint = { PAL_RAMP(RAMP_MINT, 2), PAL_RAMP(RAMP_MINT, 4), PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_MINT, 7) };
    Sh lav = { PAL_PAINT(PAINT_GRAPE, 0), PAL_PAINT(PAINT_LAVENDER, 0), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2), PAL_WHITE };
    /* grip + trigger + teddy charm */
    limb(x - 6, y + 8, x - 10, y + 24, 4.5f, mint);
    limb(x + 10, y + 8, x + 10, y + 18, 3.5f, mint);
    tk_line((int)x + 1, (int)y + 10, (int)x + 1, (int)y + 15, PAL_RAMP(RAMP_MINT, 2));
    {
        float sw = sinf(f * 0.2f) * 0.6f;
        float ax = x - 10, ay = y + 28, ex = ax + sinf(sw) * 10, ey = ay + cosf(sw) * 10;
        tk_line((int)ax, (int)ay, (int)ex, (int)ey, PAL_GRAY);
        teddy_head(ex, ey + 4, 4.5f, 0, 0, f);
    }
    /* main tube */
    limb(x - 40, y, x + 36, y, 11, pink);
    /* back cap */
    ball(x - 44, y, 6, 12, lav);
    tk_heart((int)x - 45, (int)y, 1, PAL_PAINT(PAINT_MAGENTA, 1));
    /* mint rings */
    for (int i = 0; i < 2; i++) {
        float rx = x - 22 + i * 40;
        for (int yy = -12; yy <= 12; yy++) {
            float v = yy / 12.0f;
            uint8_t c = v < -0.5f ? PAL_RAMP(RAMP_MINT, 6) : v > 0.45f ? PAL_RAMP(RAMP_MINT, 3) : PAL_RAMP(RAMP_MINT, 5);
            tk_rect((int)rx, (int)y + yy, 5, 1, c);
        }
        tk_rect((int)rx, (int)y - 12, 5, 1, PAL_RAMP(RAMP_MINT, 2));
        tk_rect((int)rx, (int)y + 12, 5, 1, PAL_RAMP(RAMP_MINT, 2));
    }
    /* stickers */
    tk_star((int)x - 8, (int)y - 3, 3, PAL_PAINT(PAINT_LEMON, 1));
    tk_px((int)x - 8, (int)y - 3, PAL_PAINT(PAINT_LEMON, 2));
    tk_heart((int)x + 6, (int)y + 4, 1, PAL_PAINT(PAINT_CHERRY, 1));
    disc(x - 30, y + 3, 4, PAL_PAINT(PAINT_LEMON, 0)); disc(x - 30, y + 3, 3.3f, PAL_PAINT(PAINT_LEMON, 1));
    tk_px((int)x - 31, (int)y + 2, PAL_PAINT(PAINT_JET, 0)); tk_px((int)x - 29, (int)y + 2, PAL_PAINT(PAINT_JET, 0));
    tk_px((int)x - 31, (int)y + 4, PAL_PAINT(PAINT_JET, 0)); tk_px((int)x - 30, (int)y + 5, PAL_PAINT(PAINT_JET, 0)); tk_px((int)x - 29, (int)y + 4, PAL_PAINT(PAINT_JET, 0));
    /* bow on top */
    tk_tri((int)x - 26, (int)y - 12, (int)x - 33, (int)y - 17, (int)x - 33, (int)y - 8, PAL_PAINT(PAINT_SKY, 1));
    tk_tri((int)x - 26, (int)y - 12, (int)x - 19, (int)y - 17, (int)x - 19, (int)y - 8, PAL_PAINT(PAINT_SKY, 1));
    disc(x - 26, y - 12, 2, PAL_PAINT(PAINT_SKY, 2));
    /* heart sight */
    tk_rect((int)x + 14, (int)y - 15, 2, 4, PAL_RAMP(RAMP_MINT, 3));
    tk_heart((int)x + 15, (int)y - 17, 1, PAL_PAINT(PAINT_MAGENTA, 1));
    /* muzzle: flared ring + dark mouth + teddy ammo */
    ball(x + 40, y, 6, 14, mint);
    for (int yy = -10; yy <= 10; yy++) {
        float v = yy / 10.0f;
        int w = (int)(4 * sqrtf(1 - v * v));
        tk_rect((int)x + 41 - w, (int)y + yy, w * 2 + 1, 1, PAL_PAINT(PAINT_GRAPE, 0));
    }
    if (teddy_state < 3) {
        float ty = y - (teddy_state == 2 ? 0 : 2), tx = x + 44 - (teddy_state == 2 ? 8 : 0);
        teddy_head(tx, ty, 8, look, teddy_state >= 1, f);
        /* the rim in front of the teddy's chin */
        for (int yy = 4; yy <= 14; yy++) { float v = yy / 14.0f; int w = (int)(6 * sqrtf(1 - v * v)); tk_rect((int)x + 40 - w, (int)y + yy, w * 2 + 1, 1, yy < 8 ? PAL_RAMP(RAMP_MINT, 5) : PAL_RAMP(RAMP_MINT, 4)); }
    }
}

static void super_shotgun(float x, float y, int f) {
    /* points LEFT: muzzle at x - 56 */
    Sh gun = { PAL_RAMP(RAMP_STEEL, 0), PAL_RAMP(RAMP_STEEL, 1), PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 5), PAL_RAMP(RAMP_STEEL, 7) };
    Sh wood = { PAL_RAMP(RAMP_WOOD, 0), PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6) };
    Sh bone = { PAL_RAMP(RAMP_BONE, 2), PAL_RAMP(RAMP_BONE, 4), PAL_RAMP(RAMP_BONE, 6), PAL_RAMP(RAMP_BONE, 7), PAL_WHITE };
    /* stock: a thick wedge of dark wood, a skull carved in */
    for (int j = -6; j <= 12; j++) {
        float t = (j + 6) / 18.0f;
        int x0 = (int)(x + 10), x1 = (int)(x + 44 + t * 6);
        uint8_t c = j < -4 ? PAL_RAMP(RAMP_WOOD, 5) : j > 9 ? PAL_RAMP(RAMP_WOOD, 1) : PAL_RAMP(RAMP_WOOD, 3);
        if (j > 2) x0 = (int)(x + 18 + (j - 2) * 1.5f);
        tk_rect(x0, (int)y + j, x1 - x0, 1, c);
        tk_px(x1, (int)y + j, PAL_RAMP(RAMP_WOOD, 0));
    }
    disc(x + 34, y + 2, 4, PAL_RAMP(RAMP_WOOD, 1));
    disc(x + 34, y + 1.5f, 3.3f, PAL_RAMP(RAMP_BONE, 5));
    tk_px((int)x + 33, (int)y + 1, PAL_RAMP(RAMP_WOOD, 0)); tk_px((int)x + 35, (int)y + 1, PAL_RAMP(RAMP_WOOD, 0));
    tk_rect((int)x + 33, (int)y + 4, 3, 1, PAL_RAMP(RAMP_WOOD, 1));
    /* receiver + trigger */
    box((int)x - 6, (int)y - 6, 18, 12, gun);
    tk_rect((int)x + 2, (int)y + 6, 1, 5, PAL_RAMP(RAMP_STEEL, 1));
    tk_rect((int)x - 2, (int)y + 10, 5, 1, PAL_RAMP(RAMP_STEEL, 1));
    /* twin barrels */
    limb(x - 56, y - 3, x - 4, y - 3, 3.2f, gun);
    limb(x - 56, y + 3, x - 4, y + 3, 3.2f, gun);
    /* forend */
    box((int)x - 30, (int)y + 2, 20, 6, wood);
    /* muzzles */
    disc(x - 57, y - 3, 2.2f, PAL_RAMP(RAMP_STEEL, 0)); disc(x - 57, y + 3, 2.2f, PAL_RAMP(RAMP_STEEL, 0));
    /* welded-on spikes */
    for (int i = 0; i < 5; i++) {
        int sx = (int)x - 50 + i * 10;
        tk_tri(sx - 3, (int)y - 6, sx + 3, (int)y - 6, sx, (int)y - 13, bone.o);
        tk_tri(sx - 2, (int)y - 6, sx + 2, (int)y - 6, sx, (int)y - 12, bone.b);
        tk_px(sx - 1, (int)y - 9, bone.l);
    }
    tk_tri((int)x - 60, (int)y - 2, (int)x - 60, (int)y + 2, (int)x - 70, (int)y, bone.b);     /* bayonet */
    tk_line((int)x - 60, (int)y - 1, (int)x - 68, (int)y, bone.l);
    /* skull charm */
    {
        float sw = sinf(f * 0.2f + 3.1f) * 0.6f;
        float ax = x + 2, ay = y + 12, ex = ax + sinf(sw) * 10, ey = ay + cosf(sw) * 10;
        tk_line((int)ax, (int)ay, (int)ex, (int)ey, PAL_GRAY);
        ball(ex, ey + 4, 4, 3.6f, bone);
        tk_rect((int)ex - 2, (int)ey + 3, 2, 2, PAL_RAMP(RAMP_BONE, 0)); tk_rect((int)ex + 1, (int)ey + 3, 2, 2, PAL_RAMP(RAMP_BONE, 0));
        tk_rect((int)ex - 1, (int)ey + 7, 3, 1, PAL_RAMP(RAMP_BONE, 3));
    }
    /* a drip of something at the muzzle */
    {
        int ph = f % 30;
        float dy = ph < 18 ? ph * 0.2f : 3.6f + (ph - 18) * (ph - 18) * 0.25f;
        disc(x - 56, y + 6 + dy, 1.4f, PAL_RAMP(RAMP_BLOOD, 4));
    }
}

/* nine part slots, popping in one by one */
static void part_slots(float cx, float y, int age, int cute) {
    for (int i = 0; i < 9; i++) {
        int a = age - i * 2;
        if (a < 0) continue;
        float s = a < 6 ? tk_bounce(a / 6.0f) : 1;
        float x = cx - 48 + i * 12;
        int r = (int)(5 * s);
        if (r < 1) continue;
        uint8_t o = cute ? PAL_PAINT(PAINT_ROSE, 0) : PAL_RAMP(RAMP_BLOOD, 0);
        uint8_t b = cute ? PAL_WHITE : PAL_RAMP(RAMP_STEEL, 1);
        tk_rect((int)x - r, (int)y - r, 2 * r + 1, 2 * r + 1, o);
        tk_rect((int)x - r + 1, (int)y - r + 1, 2 * r - 1, 2 * r - 1, b);
        if (s > 0.9f) {
            /* a tiny part icon per slot */
            uint8_t c1 = cute ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_RAMP(RAMP_STEEL, 5);
            uint8_t c2 = cute ? PAL_RAMP(RAMP_MINT, 5) : PAL_RAMP(RAMP_WOOD, 4);
            int X = (int)x, Y = (int)y;
            switch (i) {
            case 0: tk_rect(X - 3, Y - 1, 7, 2, c1); break;                         /* barrel   */
            case 1: tk_rect(X - 3, Y - 1, 4, 3, c2); tk_rect(X + 1, Y, 2, 2, c2); break; /* stock */
            case 2: tk_rect(X - 1, Y - 2, 2, 5, c2); break;                         /* grip     */
            case 3: tk_rect(X - 2, Y - 1, 5, 2, c1); tk_px(X - 3, Y - 1, PAL_PAINT(PAINT_CYAN, 1)); break; /* sight */
            case 4: tk_rect(X - 1, Y - 2, 3, 5, c1); break;                         /* mag      */
            case 5: tk_rect(X - 2, Y - 2, 2, 4, c1); tk_rect(X, Y - 1, 3, 2, c2); break; /* muzzle */
            case 6: tk_px(X, Y - 2, PAL_GRAY); disc(X, Y + 1, 1.6f, cute ? PAL_PAINT(PAINT_CARAMEL, 1) : PAL_RAMP(RAMP_BONE, 6)); break; /* charm */
            case 7: if (cute) tk_heart(X, Y, 1, PAL_PAINT(PAINT_CHERRY, 1)); else { tk_px(X - 1, Y - 1, PAL_RAMP(RAMP_BONE, 6)); tk_px(X + 1, Y - 1, PAL_RAMP(RAMP_BONE, 6)); tk_rect(X - 1, Y + 1, 3, 1, PAL_RAMP(RAMP_BONE, 6)); } break; /* sticker */
            case 8: tk_rect(X - 2, Y - 2, 2, 2, c1); tk_rect(X, Y - 2, 2, 2, c2); tk_rect(X - 2, Y, 2, 2, c2); tk_rect(X, Y, 2, 2, c1); break; /* paint */
            }
        }
    }
}

static void shot_split(int t) {
    /* the divider swoops in: a zigzag from the top to the bottom */
    int div[TH];
    for (int y = 0; y < TH; y++) div[y] = 120 + (((y / 8) & 1) ? 4 : -4) + (y % 8 < 4 ? (y % 8) : 8 - (y % 8)) - 2;
    /* backgrounds */
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            int left = x < div[y];
            uint8_t c;
            if (left) {
                float r = atan2f(y - 76.0f, x - 60.0f) + t * 0.02f;
                int ray = ((int)floorf(r / 0.3927f)) & 1;
                c = ray ? PAL_RAMP(RAMP_PASTEL_PINK, 7) : PAL_RAMP(RAMP_PASTEL_PINK, 6);
            } else {
                float r = atan2f(y - 76.0f, x - 180.0f) - t * 0.02f;
                int ray = ((int)floorf(r / 0.3927f)) & 1;
                c = ray ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_BLOOD, 0);
                if (y > 120) { int e = y - 120; if (BY[y & 3][x & 3] < e / 3) c = PAL_RAMP(RAMP_BLOOD, 2); }
            }
            tk_fb[y][x] = c;
        }
    /* left: drifting hearts + sparkles */
    for (int i = 0; i < 9; i++) {
        uint32_t s = tk_hash((uint32_t)i * 53u + 1u);
        int x = 6 + tk_rand(&s, 100), y = (int)(170 - fmodf(tk_rand(&s, 200) + t * (0.6f + (i % 3) * 0.3f), 190));
        if (x < div[y < 0 ? 0 : y >= TH ? TH - 1 : y] - 6) tk_heart(x, y, 1, i & 1 ? PAL_PAINT(PAINT_BUBBLEGUM, 2) : PAL_WHITE);
    }
    tk_sparkles(60, 70, t, 31, 50);
    /* right: embers rising + lava glow */
    light_pool(180, 175, 80, 50, L_LAVA, 3, 1.2f);
    for (int i = 0; i < 16; i++) {
        uint32_t s = tk_hash((uint32_t)i * 71u + 3u);
        float x = 126 + tk_rand(&s, 114) + sinf(t * 0.1f + i) * 3, y = 165 - fmodf(tk_rand(&s, 170) + t * (1.0f + (i % 4) * 0.4f), 170);
        tk_px((int)x, (int)y, (i & 1) ? PAL_RAMP(RAMP_LAVA, 6) : PAL_RAMP(RAMP_LAVA, 4));
        if (i % 3 == 0) tk_px((int)x, (int)y + 1, PAL_RAMP(RAMP_LAVA, 3));
    }
    /* the weapons, floating */
    float bobL = sinf(t * 0.12f) * 2, bobR = sinf(t * 0.12f + 1.5f) * 2;
    int teddy = 0, look = 0;
    if (t >= 40) { look = 1; }
    if (t >= 46) teddy = 1;
    if (t >= 56) teddy = 2;
    if (t >= 60) teddy = 3;
    float kl = tk_bounce((t - 2) / 10.0f), kr = tk_bounce((t - 4) / 10.0f);
    plush_launcher(54 - (1 - kl) * 120, 74 + bobL, t, teddy, look);
    super_shotgun(190 + (1 - kr) * 120, 74 + bobR, t);
    /* glint sweeps */
    for (int side = 0; side < 2; side++) {
        int g = ((t + side * 20) * 6) % 220 - 40;
        int x0 = side ? 124 : 0, x1 = side ? TW : 116;
        for (int y = 50; y < 110; y++)
            for (int x = x0; x < x1; x++) {
                int u = (x - x0) + (y - 50) / 2;
                if (u < g || u > g + 4) continue;
                uint8_t c = tk_get(x, y);
                int on = side ? (c >= PAL_RAMP(RAMP_STEEL, 0)) : (c >= PAL_PAINT(PAINT_BUBBLEGUM, 0) && c <= PAL_PAINT(PAINT_BUBBLEGUM, 2));
                if (on) lut_px(x, y, L_WHITE[1]);
            }
    }
    /* divider line */
    for (int y = 0; y < TH; y++) {
        int d = div[y];
        tk_px(d - 1, y, PAL_RAMP(RAMP_CANDY, 2));
        tk_px(d, y, PAL_UI_GOLD);
        tk_px(d + 1, y, PAL_RAMP(RAMP_CANDY, 2));
    }
    /* labels */
    if (t >= 3) {
        float s = t < 10 ? 1 + (1 - tk_bounce((t - 3) / 7.0f)) * 0.8f : 1;
        layer_begin();
        tk_text_outline(60 - tk_text_w("CUTE", 3) / 2, 80, "CUTE", PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_ROSE, 0), 3);
        layer_end(LAYER_A);
        layer_draw(LAYER_A, 60, 92, 60, 22, s, s, 0, 0, TH);
        for (int i = 0; i < 3; i++) tk_heart(20 + i * 40 + (int)(sinf(t * 0.2f + i) * 2), 36 + (i & 1) * 2, 1, PAL_PAINT(PAINT_CHERRY, 1));
    }
    if (t >= 7) {
        float s = t < 14 ? 1 + (1 - tk_bounce((t - 7) / 7.0f)) * 0.8f : 1;
        layer_begin();
        tk_text_outline(180 - tk_text_w("BRUTAL", 3) / 2, 80, "BRUTAL", PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 0), 3);
        /* drips off the letters */
        for (int i = 0; i < 6; i++) {
            int dx = 180 - tk_text_w("BRUTAL", 3) / 2 + 6 + i * 18;
            int len = 2 + ((t * (i + 2) / 5) % 7);
            tk_rect(dx, 104, 2, len, PAL_UI_RED_D);
            tk_rect(dx, 104 + len, 2, 2, PAL_UI_RED);
        }
        layer_end(LAYER_A);
        layer_draw(LAYER_A, 180, 92, 180, 22, s, s, 0, 0, TH);
        if (t < 10) tk_post_shake((t & 1) ? 3 : -3, 1);
    }
    /* VS */
    if (t >= 12) {
        float s = t < 18 ? tk_bounce((t - 12) / 6.0f) : 1 + sinf(t * 0.3f) * 0.05f;
        layer_begin();
        tk_text_outline(120 - tk_text_w("VS", 2) / 2, 72, "VS", PAL_UI_GOLD, PAL_RAMP(RAMP_CANDY, 1), 2);
        layer_end(LAYER_A);
        layer_draw(LAYER_A, 120, 80, 120, 76, s, s, 0, 0, TH);
    }
    /* names + nine part slots */
    if (t >= 14) {
        tk_text_outline(60 - tk_text_w("PLUSH LAUNCHER", 1) / 2, 104, "PLUSH LAUNCHER", PAL_PAINT(PAINT_ROSE, 0), PAL_WHITE, 1);
        tk_text_outline(180 - tk_text_w("SUPER SHOTGUN", 1) / 2, 104, "SUPER SHOTGUN", PAL_RAMP(RAMP_BONE, 7), PAL_RAMP(RAMP_BLOOD, 0), 1);
    }
    if (t >= 18) { part_slots(58, 126, t - 18, 1); part_slots(182, 126, t - 20, 0); }
    if (t >= 16) {
        int a = t - 16;
        tk_caption(149 + (a < 4 ? 4 - a : 0), "(9 part slots. Millions of guns.)", PAL_WHITE, PAL_GRAY_D, 1);
    }
}

void scene_s09_weld(int f, int n) {
    (void)n;
    luts_init();
    if (f < F_SPLIT) shot_workshop(f);
    else shot_split(f - F_SPLIT);
    /* hard flash on the cut */
    if (f == F_SPLIT) tk_post_flash(PAL_WHITE, 10);
}

void scene_s09_weld_audio(double t0, double dur) {
#define AT(fr) (t0 + (fr) / 30.0)
    (void)dur;
    /* welding crackle during each arc */
    for (int p = 0; p < P_COUNT - 2; p++) {
        int s0 = p == 0 ? 0 : P_ARRIVE[p] + 1;
        for (int i = 0; i < 6; i++) tk_slide(AT(s0 + i), 0.04, 100 - (i * 7) % 20, 90, TKV_NOISE, 0.22f);
        tk_note(AT(s0), 0.2, 98, TKV_SQ12, 0.05f);
    }
    /* parts: whoosh in, CLANK */
    for (int p = 1; p < P_COUNT; p++) {
        tk_sfx(AT(P_ARRIVE[p] - 6), SFX_WHOOSH, 0.25f);
        if (p < P_STICKER) {
            tk_note(AT(P_ARRIVE[p]), 0.07, 45 + p * 3, TKV_SQ50, 0.4f);
            tk_slide(AT(P_ARRIVE[p]), 0.12, 96, 70, TKV_NOISE, 0.3f);
            tk_note(AT(P_ARRIVE[p]), 0.18, 84 + p, TKV_TRI, 0.12f);
        }
    }
    tk_sfx(AT(P_ARRIVE[P_STICKER]), SFX_POP, 0.6f);     /* sticker: pop */
    tk_sfx(AT(P_ARRIVE[P_CHARM]), SFX_SQUEAK, 0.4f);    /* charm: squeak */
    /* the caption slam */
    tk_sfx(AT(10), SFX_THUD, 0.9f);
    /* the tail catches fire: fwoosh + the sign ding */
    tk_slide(AT(41), 0.35, 40, 70, TKV_NOISE, 0.4f);
    tk_note(AT(46), 0.08, 76, TKV_SQ25, 0.25f);
    tk_note(AT(48), 0.3, 88, TKV_SINE, 0.3f);
    /* mask flips up: clunk, then a proud little fanfare */
    tk_note(AT(54), 0.06, 52, TKV_SQ50, 0.3f);
    tk_sfx(AT(58), SFX_SPARKLE, 0.4f);
    tk_note(AT(59), 0.1, 79, TKV_SQ25, 0.2f);
    tk_note(AT(62), 0.2, 84, TKV_SQ25, 0.2f);
    /* SPLIT: whoosh, CUTE boing, BRUTAL shotgun rack, VS thud */
    tk_sfx(AT(F_SPLIT), SFX_WHOOSH, 0.5f);
    tk_sfx(AT(F_SPLIT + 3), SFX_BOING, 0.35f);
    tk_sfx(AT(F_SPLIT + 3), SFX_SPARKLE, 0.3f);
    tk_slide(AT(F_SPLIT + 7), 0.06, 70, 50, TKV_NOISE, 0.5f);       /* chk- */
    tk_slide(AT(F_SPLIT + 10), 0.08, 60, 80, TKV_NOISE, 0.5f);      /* -chk */
    tk_note(AT(F_SPLIT + 7), 0.1, 40, TKV_SQ50, 0.3f);
    tk_sfx(AT(F_SPLIT + 12), SFX_THUD, 0.8f);
    /* slots popping in */
    for (int i = 0; i < 9; i++) tk_note(AT(F_SPLIT + 18 + i * 2), 0.04, 72 + i * 2, TKV_SQ25, 0.12f);
    /* the teddy notices... gulp... retreats */
    tk_note(AT(F_SPLIT + 46), 0.12, 79, TKV_SINE, 0.25f);
    tk_slide(AT(F_SPLIT + 56), 0.25, 76, 55, TKV_SQ12, 0.25f);
#undef AT
}
