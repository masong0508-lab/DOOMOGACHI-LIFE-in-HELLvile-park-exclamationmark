/* s10_shooter.c - THE DOOM PART.   (8.5 s, 255 frames)
 *
 * A small first-person raycaster lives in this file: a grid map with per-cell
 * floor heights (lava pools sunk into the floor, a candy step you can see the
 * riser of), procedural textures (pastel brick rotting into flesh, hell brick
 * with glowing cracks, a flesh wall with a blinking eyeball, skull pillars, a
 * lava-fall gate), Doom-style chunky texels, distance fog and lava glow via
 * palette LUTs, and a per-pixel depth buffer so sprites tuck behind pillars.
 *
 * f  0- 73  CUTE MODE: an impling charges down the corridor. The PLUSH
 *           LAUNCHER fires a teddy; the imp bursts into confetti, hearts and
 *           plush stuffing, leaving a little imp plushie on the floor.
 * f 74-139  BRUTAL MODE: weapon swap, the HUD turns hellish, the very same imp
 *           respawns. SUPER SHOTGUN: flash, shake, gibs, blood on the lens.
 *           Break-open reload, two shells fly.
 * f140-194  "(We don't judge.)" ... "(We judge a little.)"
 *           Father Fuzz leans into frame. "tsk."
 * f195-254  THIRD PERSON: the camera pulls back over the shoulder of the
 *           fluffy fox hero, minigun spinning up. BRRRRT. Demons pop into
 *           confetti AND gibs. HUD: "GORE: BOTH?!"  "...or third person!"
 */
#include "tk.h"
#include "pal.h"
#include <math.h>
#include <string.h>

#define VW 240
#define VH 132
#define CEIL_H 1.3f
#define KIT static __attribute__((unused))

#define F_CUTE_CAP   3
#define F_IMP1       6
#define F_FIRE1      24
#define F_SWAP       74
#define F_BRUTAL_CAP 80
#define F_IMP2       84
#define F_FIRE2      106
#define F_RELOAD     116
#define F_JUDGE1     138
#define F_JUDGE2     164
#define F_FUZZ       167
#define F_TP         195
#define F_BRRT       206

/* ========================================================================= */
/* art kit                                                                    */
/* ========================================================================= */
static const uint8_t BY[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
typedef struct { uint8_t o, d, b, l, h; } Sh;
#define SH_PAINT(p) ((Sh){ PAL_PAINT(p, 0), PAL_PAINT(p, 0), PAL_PAINT(p, 1), PAL_PAINT(p, 2), PAL_WHITE })

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
/* fog: FOG[l][c], l = 15 untouched .. 0 = fully fogged (dark maroon)
 * glow: GLOW[g][c], warm lava light added on top */
static uint8_t FOG[16][256], GLOW[4][256];
static uint8_t L_DARK[4][256], L_WHITE[3][256], L_PINK[3][256], L_RED[3][256], L_FLASH[3][256];
static int luts_ready;
static void luts_init(void) {
    if (luts_ready) return;
    luts_ready = 1;
    const int fr = 22, fg = 5, fb = 16;
    for (int l = 0; l < 16; l++)
        for (int i = 0; i < 256; i++) {
            if (l == 15 || i == 0) { FOG[l][i] = (uint8_t)i; continue; }
            float k = l / 15.0f;
            FOG[l][i] = nearest_rgb((int)(fr + (TK_PAL[i][0] - fr) * k), (int)(fg + (TK_PAL[i][1] - fg) * k), (int)(fb + (TK_PAL[i][2] - fb) * k));
        }
    for (int g = 0; g < 4; g++) lut_make(GLOW[g], 255, 100, 20, LM_ADD, 0.09f * g);
    for (int i = 0; i < 4; i++) lut_make(L_DARK[i], 60, 30, 70, LM_MUL, 0.14f + 0.13f * i);
    for (int i = 0; i < 3; i++) lut_make(L_WHITE[i], 255, 255, 255, LM_MIX, 0.2f + 0.2f * i);
    for (int i = 0; i < 3; i++) lut_make(L_PINK[i], 255, 150, 210, LM_MIX, 0.15f + 0.15f * i);
    for (int i = 0; i < 3; i++) lut_make(L_RED[i], 200, 0, 20, LM_MIX, 0.15f + 0.15f * i);
    for (int i = 0; i < 3; i++) lut_make(L_FLASH[i], 255, 220, 140, LM_ADD, 0.15f + 0.15f * i);
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
KIT void limb(float x0, float y0, float x1, float y1, float r0, float r1, Sh s) {
    float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy + 1e-6f, len = sqrtf(L2);
    float nx = -dy / len, ny = dx / len;
    if (ny > 0 || (ny == 0 && nx > 0)) { nx = -nx; ny = -ny; }
    float rm = r0 > r1 ? r0 : r1;
    int minx = (int)floorf(fminf(x0, x1) - rm - 2), maxx = (int)ceilf(fmaxf(x0, x1) + rm + 2);
    int miny = (int)floorf(fminf(y0, y1) - rm - 2), maxy = (int)ceilf(fmaxf(y0, y1) + rm + 2);
    for (int y = miny; y <= maxy; y++)
        for (int x = minx; x <= maxx; x++) {
            float px = x + .5f - x0, py = y + .5f - y0;
            float t = (px * dx + py * dy) / L2;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            float r = r0 + (r1 - r0) * t;
            float qx = px - dx * t, qy = py - dy * t;
            float d = sqrtf(qx * qx + qy * qy);
            if (d > r + 1) continue;
            if (d > r) { if (s.o) tk_px(x, y, s.o); continue; }
            float side = (qx * nx + qy * ny) / r;
            uint8_t c = side > 0.4f ? s.l : side < -0.4f ? s.d : s.b;
            if (side > 0.72f && s.h && r > 2.5f) c = s.h;
            tk_px(x, y, c);
        }
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
    float k = age < 7 ? 1.0f + (1.0f - tk_bounce(age / 7.0f)) * (scale >= 3 ? 0.2f : 1.0f) : 1.0f;
    layer_draw(LAYER_A, 120, 80, 120, (float)cy, k, k, 0, 0, TH);
}

/* ========================================================================= */
/* the map: MAP[y][x], x runs down the corridor                               */
/* ========================================================================= */
static const char *const MAP[] = {
    "##########################",
    "#PPPPPPHHHFFFFHHHHPPPPHHD#",
    "#.....S..^^^^.ll..S.ll..D#",
    "#........^^^^...........D#",
    "#.....S..^^^^.ll..S.ll..D#",
    "#PPPPPPHHHFFFFHHHHPPPPHHD#",
    "##########################",
};
#define MAP_W 26
#define MAP_H 7
static char cell(int x, int y) {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return '#';
    return MAP[y][x];
}
static int is_wall(char c) { return c == '#' || c == 'H' || c == 'P' || c == 'F' || c == 'S' || c == 'D'; }
static float floor_h(char c) { return c == 'l' ? -0.16f : c == '^' ? 0.17f : 0.0f; }
static float floor_at(float x, float y) { return floor_h(cell((int)floorf(x), (int)floorf(y))); }

/* camera */
static float cam_x, cam_y, cam_z, cdx, cdy, cpx, cpy, horiz, projY;
static int g_f, g_flash;
static float zbuf[VH][VW];

static void set_camera(float x, float y, float z, float yaw, float hz) {
    cam_x = x; cam_y = y; cam_z = z; horiz = hz;
    cdx = cosf(yaw); cdy = sinf(yaw);
    const float plane = 0.68f;
    cpx = -cdy * plane; cpy = cdx * plane;
    projY = (VW / 2) / plane;
}

/* ---- textures (Doom-style chunky texels: 32 per unit) ------------------- */
#define EMIT 0x100
static float fracf(float v) { return v - floorf(v); }
static uint32_t h2(int a, int b) { return tk_hash((uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u); }
static float q32(float v) { return floorf(v * 32.0f) / 32.0f; }

static int tex_hellbrick(float u, float v, int ci) {
    float row = v / 0.26f;
    int r = (int)floorf(row);
    float bu = u * 2 + ((r & 1) ? 0.5f : 0);
    int bc = (int)floorf(bu);
    float fu = fracf(bu), fv = fracf(row);
    uint32_t hh = h2(ci * 31 + bc, r);
    if (fu < 0.07f || fv < 0.12f) return PAL_RAMP(RAMP_STONE, 1);
    int base = 3 - (int)(hh % 3 == 0);
    if (fv > 0.82f) base--;
    if (fu < 0.14f || fv < 0.24f) base++;
    if ((hh >> 5) % 7 == 0) {   /* a glowing crack */
        float cyv = 0.55f + sinf(fu * 11.0f + (float)(hh & 7)) * 0.16f;
        if (fabsf(fv - cyv) < 0.08f && fu > 0.12f && fu < 0.9f) return PAL_RAMP(RAMP_LAVA, 4 + ((g_f / 5 + (int)hh) & 1)) | EMIT;
    }
    return PAL_RAMP(RAMP_STONE, base);
}
static int tex_flesh(float u, float v, int ci) {
    float pulse = sinf(g_f * 0.2f) * 0.15f;
    float b = sinf(u * 9.0f + ci) * sinf(v * 8.0f + ci * 0.7f) + sinf((u + v) * 15.0f) * 0.3f + pulse;
    int c = b > 0.45f ? 6 : (b > -0.1f ? 5 : 4);
    float vein = fabsf(sinf(u * 11.0f + sinf(v * 6.0f + ci) * 2.5f));
    if (vein < 0.1f) return PAL_RAMP(RAMP_BLOOD, 3 + ((g_f / 6) & 1));
    if ((ci & 1) == 0) {   /* an eyeball: blinks, darts about */
        float ex = u - 0.5f, ey = v - 0.78f;
        float d2 = ex * ex + ey * ey * 1.3f;
        if (d2 < 0.03f) {
            int blink = ((g_f + ci * 13) % 64) < 3;
            if (blink) return PAL_RAMP(RAMP_FLESH, 3);
            if (d2 > 0.024f) return PAL_RAMP(RAMP_FLESH, 2);
            float lx = ex - sinf(g_f * 0.07f + ci) * 0.05f, ly = ey;
            float d = lx * lx + ly * ly;
            if (d < 0.0018f) return PAL_BLACK;
            if (d < 0.006f) return PAL_UI_RED;
            if (ex < -0.06f && ey > 0.02f && d2 < 0.012f) return PAL_WHITE;
            return PAL_RAMP(RAMP_BONE, 7);
        }
    }
    return PAL_RAMP(RAMP_FLESH, c);
}
static int tex_pastel(float u, float v, int ci) {
    /* the rot creeps up from the floor */
    float line = 0.42f + sinf(u * 9.0f + ci * 2.1f) * 0.1f + sinf(u * 23.0f) * 0.04f;
    if (v < line) {
        if (v > line - 0.05f) return PAL_RAMP(RAMP_FLESH, 3);
        return tex_flesh(u, v, ci + 1);
    }
    float drip = fracf(u * 3.0f + ci * 0.37f);
    if (drip < 0.08f && v < line + 0.3f * fracf(ci * 0.61f + u)) return PAL_RAMP(RAMP_FLESH, 4);
    /* heart graffiti ... with devil horns doodled on */
    if (ci % 3 == 1) {
        float hx = (u - 0.5f) * 3.2f, hy = (v - 0.92f) * 3.2f;
        float hf = powf(hx * hx + hy * hy - 0.3f, 3) - hx * hx * hy * hy * hy;
        if (hf < 0 && fabsf(hx) < 1.2f) return PAL_PAINT(PAINT_CHERRY, 1);
        if ((fabsf(hx - 0.45f) < 0.08f || fabsf(hx + 0.45f) < 0.08f) && hy > 0.55f && hy < 0.85f) return PAL_RAMP(RAMP_BLOOD, 2);
    }
    float row = v / 0.2f;
    int r = (int)floorf(row);
    float bu = u * 2.5f + ((r & 1) ? 0.5f : 0);
    float fu = fracf(bu), fv = fracf(row);
    if (fu < 0.07f || fv < 0.12f) return PAL_RAMP(RAMP_PASTEL_PINK, 7);
    uint32_t hh = h2(ci * 17 + (int)floorf(bu), r);
    int base = 5 - (int)(hh % 4 == 0);
    if (fv > 0.8f) base--;
    if (fu < 0.15f || fv < 0.25f) base = 6;
    return PAL_RAMP(RAMP_PASTEL_PINK, base);
}
static int tex_pillar(float u, float v, int ci) {
    float sx = u - 0.5f, sy = v - 0.86f;
    /* carved skull: cranium, sockets with glowing eyes, nose, teeth */
    float cr = sx * sx / 0.05f + sy * sy / 0.03f;
    if (cr < 1 && sy > -0.08f) {
        if ((sx + 0.09f) * (sx + 0.09f) + (sy - 0.0f) * (sy - 0.0f) < 0.0025f) return ((g_f / 4) & 1) ? PAL_UI_RED | EMIT : PAL_RAMP(RAMP_LAVA, 5) | EMIT;
        if ((sx - 0.09f) * (sx - 0.09f) + (sy - 0.0f) * (sy - 0.0f) < 0.0025f) return ((g_f / 4) & 1) ? PAL_UI_RED | EMIT : PAL_RAMP(RAMP_LAVA, 5) | EMIT;
        if ((sx + 0.09f) * (sx + 0.09f) + sy * sy < 0.006f || (sx - 0.09f) * (sx - 0.09f) + sy * sy < 0.006f) return PAL_RAMP(RAMP_BONE, 1);
        if (fabsf(sx) < 0.025f && sy < -0.04f && sy > -0.08f) return PAL_RAMP(RAMP_BONE, 2);
        if (cr > 0.75f) return PAL_RAMP(RAMP_BONE, 3);
        return sx < -0.05f && sy > 0.05f ? PAL_RAMP(RAMP_BONE, 6) : PAL_RAMP(RAMP_BONE, 5);
    }
    if (sy <= -0.08f && sy > -0.17f && fabsf(sx) < 0.12f) {     /* jaw + teeth */
        if (sy > -0.13f) return (fracf(u * 30.0f) < 0.5f) ? PAL_RAMP(RAMP_BONE, 6) : PAL_RAMP(RAMP_BONE, 2);
        return PAL_RAMP(RAMP_BONE, 4);
    }
    /* a torch below the skull */
    if (fabsf(sx) < 0.05f && v > 0.42f && v < 0.56f) return PAL_RAMP(RAMP_WOOD, 2);
    if (v >= 0.56f && v < 0.66f) {
        float fl = 0.06f * (1 - (v - 0.56f) / 0.1f) + 0.01f * sinf(g_f * 0.8f + v * 30);
        if (fabsf(sx + sinf(g_f * 0.6f + v * 20) * 0.015f) < fl) return (v < 0.6f ? PAL_RAMP(RAMP_CANDY, 7) : PAL_RAMP(RAMP_LAVA, 5)) | EMIT;
    }
    return tex_hellbrick(u * 0.5f, v, ci);
}
static int tex_gate(float u, float v) {
    float s = sinf(u * 25.0f + sinf(v * 3.0f) * 2.0f) + sinf((v * 7.0f + g_f * 0.35f) + u * 4.0f);
    int lv = s > 1.0f ? 7 : (s > 0.2f ? 6 : (s > -0.6f ? 5 : 4));
    return PAL_RAMP(RAMP_LAVA, lv) | EMIT;
}
static int tex_wall(char c, float u, float v, int ci) {
    u = q32(u); v = q32(v);
    switch (c) {
    case 'P': return tex_pastel(u, v, ci);
    case 'F': return tex_flesh(u, v, ci);
    case 'S': return tex_pillar(u, v, ci);
    case 'D': return tex_gate(u, v);
    default: return tex_hellbrick(u, v, ci);
    }
}
static int tex_floor(char c, float wx, float wy) {
    wx = q32(wx); wy = q32(wy);
    if (c == 'l') {
        float s = sinf(wx * 6.0f + g_f * 0.12f) + sinf(wy * 7.0f - g_f * 0.09f + wx * 2.0f);
        int lv = s > 1.2f ? 7 : (s > 0.3f ? 6 : (s > -0.7f ? 5 : 4));
        return PAL_RAMP(RAMP_LAVA, lv) | EMIT;
    }
    if (c == '^') {     /* candy platform: pink + cream stripes */
        int st = ((int)floorf((wx + wy) * 5.0f)) & 1;
        if (fracf(wx) < 0.04f) return PAL_PAINT(PAINT_ROSE, 0);
        return st ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_PAINT(PAINT_BONE, 2);
    }
    /* checker tiles: lavender + strawberry, cracked */
    if (fracf(wx * 2) < 0.06f || fracf(wy * 2) < 0.06f) return PAL_RAMP(RAMP_VAPE, 2);
    int chk = ((int)floorf(wx * 2) + (int)floorf(wy * 2)) & 1;
    uint32_t hh = h2((int)floorf(wx * 2), (int)floorf(wy * 2));
    if ((hh & 7) == 0 && fabsf(fracf(wx * 2) - fracf(wy * 2) * 0.7f - 0.2f) < 0.05f) return PAL_RAMP(RAMP_LAVA, 4) | EMIT;
    if (!chk) return PAL_RAMP(RAMP_VAPE, 4 + (int)(hh & 1));
    return PAL_RAMP(RAMP_PASTEL_PINK, 3 + (int)((hh >> 3) & 1));
}
static int tex_ceil(float wx, float wy) {
    wx = q32(wx); wy = q32(wy);
    if (fracf(wx * 0.5f) < 0.08f) return PAL_RAMP(RAMP_WOOD, 2);
    if (fracf(wx * 0.5f) < 0.1f) return PAL_RAMP(RAMP_WOOD, 1);
    uint32_t hh = h2((int)floorf(wx * 3), (int)floorf(wy * 3));
    return PAL_RAMP(RAMP_VAPE, 1 + (int)(hh & 1));
}
static int tex_riser(char c, float u, float v) {
    u = q32(u);
    if (c == '^') {   /* candy-cane stripes */
        int st = ((int)floorf((u * 6.0f) + v * 12.0f)) & 1;
        return st ? PAL_UI_RED : PAL_WHITE;
    }
    (void)v;
    return PAL_RAMP(RAMP_STONE, 1 + (((int)floorf(u * 8)) & 1));
}

/* lava light: how close is a world point to a lava pool / the gate */
static float lava_glow(float x, float y) {
    float g = 0;
    for (int cy = 2; cy <= 4; cy += 2)
        for (int cxx = 0; cxx < MAP_W; cxx++) {
            if (MAP[cy][cxx] != 'l') continue;
            float dx = x - (cxx + 0.5f), dy = y - (cy + 0.5f);
            float d = dx * dx + dy * dy;
            if (d < 2.2f) g += (2.2f - d) * 0.9f;
        }
    if (x > 21) g += (x - 21) * 1.3f;
    return g;
}

static uint8_t shade_d(int texel, float t, float wx, float wy, int x, int y) {
    if (texel & EMIT) return (uint8_t)(texel & 0xFF);
    float lf = 15.4f - t * 0.78f + g_flash * 1.2f + (BY[y & 3][x & 3] + .5f) / 16.0f - 0.5f;
    if (lf > 15) lf = 15;
    if (lf < 1) lf = 1;
    float g = lava_glow(wx, wy) + (BY[y & 3][x & 3] + .5f) / 16.0f - 0.5f;
    return FOG[(int)lf][GLOW[(int)tk_clampf(g, 0, 3)][texel & 0xFF]];
}

/* ---- the raycaster -------------------------------------------------------- */
static int scr_y(float h, float t) { return (int)floorf(horiz + (cam_z - h) * projY / t); }

static void render_view(void) {
    for (int x = 0; x < VW; x++) {
        float camX = 2.0f * (x + 0.5f) / VW - 1.0f;
        float rdx = cdx + cpx * camX, rdy = cdy + cpy * camX;
        int mx = (int)floorf(cam_x), my = (int)floorf(cam_y);
        float ddx = rdx == 0 ? 1e30f : fabsf(1.0f / rdx), ddy = rdy == 0 ? 1e30f : fabsf(1.0f / rdy);
        int sx, sy; float sdx, sdy;
        if (rdx < 0) { sx = -1; sdx = (cam_x - mx) * ddx; } else { sx = 1; sdx = (mx + 1.0f - cam_x) * ddx; }
        if (rdy < 0) { sy = -1; sdy = (cam_y - my) * ddy; } else { sy = 1; sdy = (my + 1.0f - cam_y) * ddy; }
        int ybot = VH, ytop = 0;
        char cc = cell(mx, my);
        float hcur = floor_h(cc);
        for (int it = 0; it < 64 && ytop < ybot; it++) {
            int side = sdx < sdy ? 0 : 1;
            float t1 = side == 0 ? sdx : sdy;
            /* floor of this cell */
            int yf = scr_y(hcur, t1);
            if (yf < ytop) yf = ytop;
            int ys = yf > (int)horiz ? yf : (int)horiz + 1;
            for (int y = ys; y < ybot; y++) {
                float t = (cam_z - hcur) * projY / (y + 0.5f - horiz);
                if (t <= 0) continue;
                float wx = cam_x + rdx * t, wy = cam_y + rdy * t;
                tk_fb[y][x] = shade_d(tex_floor(cc, wx, wy), t, wx, wy, x, y);
                zbuf[y][x] = t;
            }
            if (ys < ybot) ybot = ys;
            if (ybot < ytop) ybot = ytop;
            /* ceiling of this cell */
            int yc = scr_y(CEIL_H, t1) + 1;
            if (yc > ybot) yc = ybot;
            for (int y = ytop; y < yc && y < (int)horiz; y++) {
                float t = (CEIL_H - cam_z) * projY / (horiz - y - 0.5f);
                if (t <= 0) continue;
                float wx = cam_x + rdx * t, wy = cam_y + rdy * t;
                tk_fb[y][x] = shade_d(tex_ceil(wx, wy), t, wx, wy, x, y);
                zbuf[y][x] = t;
            }
            if (yc > ytop) ytop = yc;
            /* step into the next cell */
            if (side == 0) { sdx += ddx; mx += sx; } else { sdy += ddy; my += sy; }
            char nc = cell(mx, my);
            float wpos = side == 0 ? cam_y + t1 * rdy : cam_x + t1 * rdx;
            float u = fracf(wpos);
            if ((side == 0 && sx < 0) || (side == 1 && sy > 0)) u = 1 - u;
            float hx = cam_x + rdx * t1, hy = cam_y + rdy * t1;
            if (is_wall(nc)) {
                int top = scr_y(CEIL_H, t1), bot = scr_y(hcur, t1) + 1;
                if (top < ytop) top = ytop;
                if (bot > ybot) bot = ybot;
                int ci = mx * 7 + my * 3;
                for (int y = top; y < bot; y++) {
                    float v = cam_z - (y + 0.5f - horiz) * t1 / projY;
                    int tx = tex_wall(nc, u, v, ci);
                    uint8_t c = shade_d(tx, t1, hx, hy, x, y);
                    if (side == 1 && !(tx & EMIT)) c = FOG[13][c];   /* simple side shading */
                    tk_fb[y][x] = c;
                    zbuf[y][x] = t1;
                }
                break;
            }
            float hn = floor_h(nc);
            if (hn > hcur) {   /* a riser: the front face of a step / far wall of a pit */
                int top = scr_y(hn, t1), bot = scr_y(hcur, t1) + 1;
                if (top < ytop) top = ytop;
                if (bot > ybot) bot = ybot;
                for (int y = top; y < bot; y++) {
                    float v = cam_z - (y + 0.5f - horiz) * t1 / projY - hcur;
                    tk_fb[y][x] = shade_d(tex_riser(nc == '^' ? '^' : cc, u, v), t1, hx, hy, x, y);
                    zbuf[y][x] = t1;
                }
                if (top < ybot) ybot = top > ytop ? top : ytop;
            }
            hcur = hn; cc = nc;
        }
    }
}

/* project a world point; returns 0 if behind the camera */
static int project(float wx, float wy, float wz, float *sx, float *sy, float *depth) {
    float rx = wx - cam_x, ry = wy - cam_y;
    float inv = 1.0f / (cpx * cdy - cdx * cpy);
    float tx = inv * (cdy * rx - cdx * ry);
    float ty = inv * (-cpy * rx + cpx * ry);
    if (ty < 0.15f) return 0;
    *sx = (VW / 2.0f) * (1 + tx / ty);
    *sy = horiz + (cam_z - wz) * projY / ty;
    *depth = ty;
    return 1;
}

/* billboard: blit layer L (feet at (fx, fy), pixel height ph) standing at a
 * world point with world height hw - depth tested + fogged */
static int billboard(uint8_t (*L)[TW], float fx, float fy, float ph, float wx, float wy, float wz, float hw,
                     float *osx, float *osy, float *oscale) {
    float sx, sy, d;
    if (!project(wx, wy, wz, &sx, &sy, &d)) return 0;
    float sc = (hw * projY / d) / ph;
    int x0 = (int)(sx - 60 * sc) - 1, x1 = (int)(sx + 60 * sc) + 1;
    int y0 = (int)(sy - (ph + 20) * sc) - 1, y1 = (int)(sy + 6 * sc) + 1;
    if (x0 < 0) x0 = 0;
    if (x1 > VW) x1 = VW;
    if (y0 < 0) y0 = 0;
    if (y1 > VH) y1 = VH;
    float lf = 15.4f - d * 0.55f + g_flash * 1.2f;
    if (lf > 15) lf = 15;
    if (lf < 3) lf = 3;
    int gl = (int)tk_clampf(lava_glow(wx, wy), 0, 3);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int u = (int)floorf(fx + (x + .5f - sx) / sc), v = (int)floorf(fy + (y + .5f - sy) / sc);
            if (u < 0 || v < 0 || u >= TW || v >= TH) continue;
            uint8_t p = L[v][u];
            if (!p || zbuf[y][x] < d) continue;
            /* demon eyes glow in the dark */
            int emis = p == PAL_PAINT(PAINT_LEMON, 1) || p == PAL_PAINT(PAINT_LEMON, 2) || p == PAL_UI_RED || p == PAL_RAMP(RAMP_LAVA, 7);
            tk_fb[y][x] = emis ? p : FOG[(int)lf][GLOW[gl][p]];
            zbuf[y][x] = d;
        }
    if (osx) { *osx = sx; *osy = sy; *oscale = sc; }
    return 1;
}

/* capture a chibi into LAYER_A at a size that suits the on-screen height */
static float capture_chibi(const TkChibi *c, int expr, int mouth, int bob, int flip, float want_px) {
    int S = (int)ceilf(want_px / 33.0f);
    if (S < 1) S = 1;
    if (S > 3) S = 3;
    layer_begin();
    tk_chibi(120, 150, c, S, expr, mouth, bob, flip);
    layer_end(LAYER_A);
    return 33.0f * S;   /* pixel height of the capture (feet to horn tips) */
}

/* ========================================================================= */
/* sprites                                                                    */
/* ========================================================================= */
static const TkChibi IMPLING = {
    PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_JET, 1), PAL_PAINT(PAINT_CHARCOAL, 1), PAL_PAINT(PAINT_LEMON, 1),
    TKH_BALD, 1, 1, 0, TKT_DEVIL, 0, 0, TKEAR_CAT, PAL_PAINT(PAINT_PEACH, 1), 1 };

/* draw the impling at world (x,y) charging; returns screen pos */
static int draw_imp(float x, float y, int f, int angry, float *sx, float *sy, float *sc) {
    float wz = floor_at(x, y) + fabsf(sinf(f * 0.55f)) * 0.07f;
    float sxx, syy, d;
    if (!project(x, y, wz, &sxx, &syy, &d)) return 0;
    float want = 0.85f * projY / d;
    float ph = capture_chibi(&IMPLING, angry ? TKE_ANGRY : TKE_NORMAL, TKM_FANGS, f * 3, 0, want);
    return billboard(LAYER_A, 120, 150, ph, x, y, wz, 0.85f, sx, sy, sc);
}

/* plush stuffing: soft white puffs drifting down */
static void stuffing(float x, float y, int age, uint32_t seed, float s) {
    if (age < 0 || age > 50) return;
    uint32_t q = tk_hash(seed);
    for (int i = 0; i < 9; i++) {
        float a = tk_rand(&q, 6283) / 1000.0f, v = (1.0f + tk_rand(&q, 100) / 50.0f) * s;
        float px = x + cosf(a) * v * age * 0.8f, py = y + sinf(a) * v * age * 0.5f - age * 0.8f * s + age * age * 0.03f;
        float r = (2.0f + (i % 3)) * s * (age < 40 ? 1 : (50 - age) / 10.0f);
        if (r < 0.7f) continue;
        disc(px, py, r + 0.7f, PAL_PAINT(PAINT_WHITE, 0));
        disc(px - 0.3f, py - 0.3f, r, PAL_WHITE);
    }
}

/* chunky gibs with a floor bounce at fy */
static void chunks(float x, float y, float floor_y, int age, uint32_t seed, float s) {
    if (age < 0 || age > 60) return;
    uint32_t q = tk_hash(seed);
    for (int i = 0; i < 12; i++) {
        float vx = (tk_rand(&q, 200) - 100) / 30.0f * s, vy = -(tk_rand(&q, 100) + 40) / 30.0f * s;
        float px = x, py = y, cvy = vy;
        for (int k = 0; k < age; k++) {
            px += vx; py += cvy; cvy += 0.25f * s;
            if (py > floor_y && cvy > 0) { py = floor_y; cvy *= -0.3f; vx *= 0.5f; }
        }
        int kind = i % 3;
        float r = (1.5f + (i % 4) * 0.6f) * s;
        if (kind == 0) { disc(px, py, r + 0.8f, PAL_RAMP(RAMP_BLOOD, 1)); disc(px, py, r, PAL_RAMP(RAMP_FLESH, 4)); tk_px((int)(px - r * 0.4f), (int)(py - r * 0.4f), PAL_RAMP(RAMP_FLESH, 6)); }
        else if (kind == 1) { disc(px, py, r + 0.6f, PAL_RAMP(RAMP_BLOOD, 0)); disc(px, py, r, PAL_RAMP(RAMP_BLOOD, 4)); }
        else { tk_rect((int)px - 2, (int)py - 1, (int)(4 * s) + 1, 2, PAL_RAMP(RAMP_BONE, 6)); tk_px((int)px - 2, (int)py - 2, PAL_RAMP(RAMP_BONE, 7)); }
    }
}

/* the teddy projectile */
static void teddy(float x, float y, float r, int spin) {
    Sh fur = { PAL_PAINT(PAINT_CHOCOLATE, 1), PAL_PAINT(PAINT_CARAMEL, 0), PAL_PAINT(PAINT_CARAMEL, 1), PAL_PAINT(PAINT_CARAMEL, 2), PAL_PAINT(PAINT_HONEY, 2) };
    float sq = 1 - 0.25f * fabsf(sinf(spin * 0.8f));
    ball(x - r * 0.75f * sq, y - r * 0.75f, r * 0.38f, r * 0.38f, fur);
    ball(x + r * 0.75f * sq, y - r * 0.75f, r * 0.38f, r * 0.38f, fur);
    ball(x, y + r * 0.9f, r * 0.75f * sq, r * 0.6f, fur);
    ball(x, y, r * sq, r * 0.92f, fur);
    if (r > 3) {
        ball(x, y + r * 0.35f, r * 0.42f * sq, r * 0.3f, (Sh){ 0, PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_HONEY, 2), PAL_PAINT(PAINT_BONE, 2), 0 });
        disc(x, y + r * 0.22f, r * 0.12f + 0.4f, PAL_PAINT(PAINT_CHOCOLATE, 0));
        /* determined little eyebrows */
        tk_px((int)(x - r * 0.35f * sq), (int)(y - r * 0.12f), PAL_PAINT(PAINT_JET, 0));
        tk_px((int)(x + r * 0.35f * sq), (int)(y - r * 0.12f), PAL_PAINT(PAINT_JET, 0));
        if (r > 6) {
            tk_line((int)(x - r * 0.55f * sq), (int)(y - r * 0.4f), (int)(x - r * 0.2f * sq), (int)(y - r * 0.28f), PAL_PAINT(PAINT_JET, 0));
            tk_line((int)(x + r * 0.55f * sq), (int)(y - r * 0.4f), (int)(x + r * 0.2f * sq), (int)(y - r * 0.28f), PAL_PAINT(PAINT_JET, 0));
        }
    }
}

/* the imp plushie left behind in CUTE mode: lying on its back, X eyes */
static void imp_plushie(float x, float y, int f, int age) {
    float sx, sy, d;
    float bounce = age < 20 ? fabsf(sinf(age * 0.35f)) * (20 - age) * 0.012f : 0;
    if (!project(x, y, floor_at(x, y) + bounce, &sx, &sy, &d)) return;
    float s = 0.55f * projY / d / 33.0f;
    layer_begin();
    tk_chibi(120, 150, &IMPLING, 1, TKE_DEAD, TKM_O, 0, 0);
    stuffing(130, 132, 30, 77, 0.6f);
    layer_end(LAYER_A);
    /* lying down: rotate the capture 90 degrees and squash */
    float lf = 15.4f - d * 0.55f;
    if (lf < 3) lf = 3;
    if (lf > 15) lf = 15;
    float ang = 1.45f + sinf(f * 0.1f) * 0.02f;
    float ca = cosf(-ang), sa = sinf(-ang);
    for (int yy = (int)(sy - 30 * s) - 2; yy < (int)(sy + 4); yy++)
        for (int xx = (int)(sx - 40 * s) - 2; xx < (int)(sx + 40 * s) + 2; xx++) {
            if (xx < 0 || yy < 0 || xx >= VW || yy >= VH) continue;
            float ux = (xx + .5f - sx) / s, uy = (yy + .5f - sy) / s;
            float rx = ux * ca - uy * sa, ry = ux * sa + uy * ca;
            int u = (int)floorf(rx + 120), v = (int)floorf(ry + 138);
            if (u < 0 || v < 0 || u >= TW || v >= TH) continue;
            uint8_t p = LAYER_A[v][u];
            if (!p || zbuf[yy][xx] < d) continue;
            tk_fb[yy][xx] = FOG[(int)lf][p];
        }
}

/* ========================================================================= */
/* first-person weapons                                                       */
/* ========================================================================= */
/* a vertical trapezoid "tube" with cylinder shading across its width */
static void tube_v(float xt, float yt, float wt, float xb, float yb, float wb, Sh s) {
    for (int y = (int)yt; y <= (int)yb; y++) {
        float t = (y - yt) / (yb - yt + 0.001f);
        float cx = xt + (xb - xt) * t, w = wt + (wb - wt) * t;
        for (int x = (int)(cx - w / 2) - 1; x <= (int)(cx + w / 2) + 1; x++) {
            float u = (x + .5f - (cx - w / 2)) / w;
            uint8_t c;
            if (u < 0 || u > 1) c = s.o;
            else if (u < 0.12f || u > 0.82f) c = s.d;
            else if (u > 0.22f && u < 0.36f) c = s.l;
            else c = s.b;
            if (u > 0.25f && u < 0.3f && s.h) c = s.h;
            if (u < -0.08f || u > 1.08f) continue;
            tk_px(x, y, c);
        }
    }
}
static void fox_paw(float x, float y, float r) {
    ball(x, y, r, r * 0.85f, (Sh){ PAL_PAINT(PAINT_RUST, 0), PAL_PAINT(PAINT_TANGERINE, 0), PAL_PAINT(PAINT_TANGERINE, 1), PAL_PAINT(PAINT_TANGERINE, 2), PAL_PAINT(PAINT_HONEY, 2) });
    for (int i = -1; i <= 1; i++) disc(x + i * r * 0.42f, y - r * 0.45f, r * 0.2f, PAL_PAINT(PAINT_BONE, 2));
}
static void sleeve(float x0, float y0, float x1, float y1, float r) {
    limb(x0, y0, x1, y1, r, r * 1.1f, (Sh){ PAL_RAMP(RAMP_MINT, 2), PAL_RAMP(RAMP_MINT, 3), PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_MINT, 6), 0 });
}

static void plush_launcher_fp(float bx, float by, int fire_age, int loaded) {
    float rec = fire_age >= 0 && fire_age < 7 ? (7 - fire_age) * 2.0f : 0;
    float mx = 152 + bx, my = 104 + by + rec;          /* muzzle */
    float ex = 170 + bx, ey = 182 + by + rec;          /* base, below the screen */
    Sh pink = { PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_BUBBLEGUM, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
    Sh mint = { PAL_RAMP(RAMP_MINT, 2), PAL_RAMP(RAMP_MINT, 4), PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_MINT, 7) };
    sleeve(96 + bx, 176 + by, 124 + bx, 140 + by + rec, 9);
    tube_v(mx, my, 40, ex, ey, 70, pink);
    /* mint rings */
    for (int i = 0; i < 2; i++) {
        float t = 0.28f + i * 0.34f;
        float cy = my + (ey - my) * t, cx = mx + (ex - mx) * t, w = 40 + 30 * t;
        tube_v(cx, cy, w + 2, cx, cy + 5, w + 3, mint);
    }
    /* stickers */
    tk_heart((int)(mx - 4), (int)(my + 34), 1, PAL_PAINT(PAINT_CHERRY, 1));
    tk_star((int)(mx + 10), (int)(my + 22), 3, PAL_PAINT(PAINT_LEMON, 1));
    disc(mx + 6, my + 52, 5, PAL_PAINT(PAINT_LEMON, 0)); disc(mx + 6, my + 52, 4.2f, PAL_PAINT(PAINT_LEMON, 1));
    tk_px((int)mx + 4, (int)my + 51, PAL_PAINT(PAINT_JET, 0)); tk_px((int)mx + 7, (int)my + 51, PAL_PAINT(PAINT_JET, 0));
    tk_line((int)mx + 4, (int)my + 54, (int)mx + 7, (int)my + 54, PAL_PAINT(PAINT_JET, 0));
    /* muzzle ring: an ellipse seen from slightly above */
    ball(mx, my, 23, 8, mint);
    ball(mx, my + 0.5f, 16, 5, (Sh){ PAL_PAINT(PAINT_GRAPE, 0), PAL_PAINT(PAINT_GRAPE, 0), PAL_PAINT(PAINT_GRAPE, 0), PAL_PAINT(PAINT_GRAPE, 1), 0 });
    if (loaded) teddy(mx, my - 6, 9, 0);
    /* heart sight */
    tk_heart((int)mx, (int)my - (loaded ? 22 : 10), 1, PAL_PAINT(PAINT_MAGENTA, 1));
    sleeve(222 + bx, 176 + by, 196 + bx, 148 + by + rec, 9);
    fox_paw(124 + bx, 136 + by + rec, 9);
    fox_paw(194 + bx, 146 + by + rec, 9);
    /* the shot: pink puff + hearts */
    if (fire_age >= 0 && fire_age < 8) {
        float r = fire_age < 4 ? 6 + fire_age * 3 : 18 - (fire_age - 4) * 4;
        for (int i = 0; i < 6; i++) {
            float an = i * 1.047f + 0.4f;
            float px = mx + cosf(an) * (6 + fire_age * 4), py = my - 8 + sinf(an) * (3 + fire_age * 2) - fire_age * 2;
            disc(px, py, r / 2 + 1, PAL_PAINT(PAINT_BUBBLEGUM, 2));
            disc(px - 1, py - 1, r / 4 + 1, PAL_WHITE);
        }
        tk_hearts((int)mx, (int)my - 6, fire_age * 2, 77);
    }
}

static void shotgun_fp(float bx, float by, int fire_age, int reload_age) {
    float rec = fire_age >= 0 && fire_age < 9 ? (9 - fire_age) * 3.0f : 0;
    float drop = 0, tilt = 0;
    if (reload_age >= 0 && reload_age < 24) { float k = sinf(reload_age / 24.0f * 3.14159f); drop = k * 30; tilt = k; }
    float oy = by + rec + drop;
    Sh gun = { PAL_RAMP(RAMP_STEEL, 0), PAL_RAMP(RAMP_STEEL, 1), PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 5), PAL_RAMP(RAMP_STEEL, 7) };
    Sh wood = { PAL_RAMP(RAMP_WOOD, 0), PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_WOOD, 3), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6) };
    Sh bone = { PAL_RAMP(RAMP_BONE, 2), PAL_RAMP(RAMP_BONE, 4), PAL_RAMP(RAMP_BONE, 6), PAL_RAMP(RAMP_BONE, 7), PAL_WHITE };
    float cx = 122 + bx;
    sleeve(76 + bx, 172 + by, 96 + bx, 144 + oy, 9);
    /* twin barrels in perspective */
    for (int s = -1; s <= 1; s += 2) {
        float tx = cx + s * 9 + tilt * 6, ty = 98 + oy + tilt * 16, bxx = cx + s * 19, byy = 180 + oy;
        tube_v(tx, ty, 16, bxx, byy, 30, gun);
        ball(tx, ty, 8.5f, 4, gun);
        ball(tx, ty + 0.5f, 5, 2.4f, (Sh){ PAL_RAMP(RAMP_STEEL, 0), PAL_RAMP(RAMP_STEEL, 0), PAL_BLACK, PAL_RAMP(RAMP_STEEL, 1), 0 });
        /* spikes welded on the outside */
        for (int i = 0; i < 3; i++) {
            float t = 0.15f + i * 0.25f;
            float yy = ty + (byy - ty) * t, xx = tx + (bxx - tx) * t + s * (8 + 7 * t);
            tk_tri((int)xx, (int)yy - 4, (int)xx, (int)yy + 4, (int)(xx + s * 10), (int)yy, bone.o);
            tk_tri((int)xx, (int)yy - 3, (int)xx, (int)yy + 3, (int)(xx + s * 9), (int)yy, bone.b);
            tk_line((int)xx, (int)yy - 2, (int)(xx + s * 7), (int)yy, bone.l);
        }
    }
    /* rib + bead sight */
    tk_rect((int)(cx - 1 + tilt * 6), (int)(94 + oy + tilt * 16), 3, 40, PAL_RAMP(RAMP_STEEL, 1));
    tk_rect((int)(cx + tilt * 6), (int)(94 + oy + tilt * 16), 1, 40, PAL_RAMP(RAMP_STEEL, 6));
    disc(cx + 0.5f + tilt * 6, 93 + oy + tilt * 16, 1.6f, PAL_UI_RED);
    /* wooden forend */
    tube_v(cx, 148 + oy, 60, cx, 180 + oy, 74, wood);
    for (int i = 0; i < 4; i++) tk_rect((int)cx - 24 + i * 14, (int)(154 + oy), 6, 1, PAL_RAMP(RAMP_WOOD, 2));
    fox_paw(96 + bx, 146 + oy, 9);
    sleeve(200 + bx, 180 + by, 174 + bx, 162 + oy, 9);
    fox_paw(160 + bx, 160 + oy, 10);
    /* muzzle flash */
    if (fire_age >= 0 && fire_age < 4) {
        float r = fire_age < 2 ? 26 : 14;
        for (int s = -1; s <= 1; s += 2) {
            disc(cx + s * 9, 88 + oy, r * 0.6f, PAL_RAMP(RAMP_LAVA, 6));
            disc(cx + s * 9, 88 + oy, r * 0.35f, PAL_RAMP(RAMP_CANDY, 7));
            tk_star((int)(cx + s * 9), (int)(88 + oy), (int)r, PAL_RAMP(RAMP_CANDY, 7));
        }
        disc(cx, 84 + oy, r * 0.3f, PAL_WHITE);
    }
}

/* ========================================================================= */
/* third person: the fluffy fox hero from behind, with a minigun             */
/* ========================================================================= */
static void hero_back(float bx, float by, int f, int firing) {
    Sh fur = { PAL_PAINT(PAINT_RUST, 0), PAL_PAINT(PAINT_TANGERINE, 0), PAL_PAINT(PAINT_TANGERINE, 1), PAL_PAINT(PAINT_TANGERINE, 2), PAL_PAINT(PAINT_HONEY, 2) };
    Sh hood = { PAL_RAMP(RAMP_MINT, 2), PAL_RAMP(RAMP_MINT, 3), PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_MINT, 7) };
    float rk = firing ? ((f & 1) ? 1.0f : -1.0f) : 0;     /* recoil jitter */
    float breathe = sinf(f * 0.16f) * 0.8f;
    float hx = bx + rk, hy = by - 46 + breathe;
    /* tall fox ears (behind the head), twitching with every shot */
    for (int s2 = -1; s2 <= 1; s2 += 2) {
        float tw = (s2 < 0 ? sinf(f * 0.9f) : cosf(f * 0.8f)) * (firing ? 3.0f : 1.0f);
        float ax = hx + s2 * 15, ay = hy - 14;
        float tx = hx + s2 * 24 + tw, ty = hy - 54 - fabsf(tw) * 0.5f;
        tk_tri((int)(ax - 11), (int)ay + 2, (int)(ax + 11), (int)ay + 2, (int)tx, (int)ty, PAL_PAINT(PAINT_RUST, 0));
        tk_tri((int)(ax - 9), (int)ay + 1, (int)(ax + 9), (int)ay + 1, (int)tx, (int)ty + 3, PAL_PAINT(PAINT_TANGERINE, 1));
        tk_tri((int)(ax - 3 - s2 * 3), (int)ay, (int)(ax + 3 - s2 * 3), (int)ay, (int)tx, (int)ty + 4, PAL_PAINT(PAINT_TANGERINE, 2));
        float mx = tx + (ax - tx) * 0.32f, my = ty + (ay - ty) * 0.32f;
        tk_tri((int)(mx - 5), (int)my, (int)(mx + 5), (int)my, (int)tx, (int)ty + 1, PAL_PAINT(PAINT_CHOCOLATE, 0));
    }
    /* body: hoodie, hood bunched at the neck */
    ball(bx, by + 4 + breathe * 0.3f, 27, 26, hood);
    ball(bx, by - 22 + breathe * 0.4f, 20, 9, hood);
    tk_line((int)bx - 12, (int)by - 22, (int)bx + 12, (int)by - 22, PAL_RAMP(RAMP_MINT, 3));
    /* right arm reaching for the minigun */
    limb(bx + 18, by - 12, bx + 32, by - 8, 6, 5, hood);
    /* head (back of it), with fluff tufts */
    for (int i = 0; i < 7; i++) {
        float a = 3.14159f + 0.35f + i * 0.4f;
        float ex = hx + cosf(a) * 27, ey = hy + sinf(a) * 24;
        float tx = hx + cosf(a) * 33, ty = hy + sinf(a) * 30;
        float px = -sinf(a) * 5, py = cosf(a) * 5;
        tk_tri((int)(ex + px), (int)(ey + py), (int)(ex - px), (int)(ey - py), (int)tx, (int)ty, PAL_PAINT(PAINT_RUST, 0));
        tk_tri((int)(ex + px * 0.7f), (int)(ey + py * 0.7f), (int)(ex - px * 0.7f), (int)(ey - py * 0.7f), (int)(tx - cosf(a)), (int)(ty - sinf(a)), PAL_PAINT(PAINT_TANGERINE, 1));
    }
    ball(hx, hy, 29, 26, fur);
    /* a tuft of darker fur down the back of the head */
    for (int i = 0; i < 3; i++) tk_tri((int)hx - 4 + i * 4, (int)hy + 14, (int)hx - 1 + i * 4, (int)hy + 14, (int)hx - 3 + i * 4, (int)hy + 22, PAL_PAINT(PAINT_TANGERINE, 0));
    /* cheek fluff peeking out at the sides */
    for (int s2 = -1; s2 <= 1; s2 += 2) {
        for (int k = 0; k < 2; k++) {
            int y0 = (int)hy + 6 + k * 7;
            tk_tri((int)(hx + s2 * 25), y0, (int)(hx + s2 * 25), y0 + 8, (int)(hx + s2 * 33), y0 + 7, PAL_PAINT(PAINT_RUST, 0));
            tk_tri((int)(hx + s2 * 25), y0 + 1, (int)(hx + s2 * 25), y0 + 7, (int)(hx + s2 * 31), y0 + 6, PAL_PAINT(PAINT_TANGERINE, 2));
        }
    }
    /* the big fluffy tail, wagging happily (closest to the camera) */
    float wag = sinf(f * (firing ? 0.7f : 0.3f));
    float p0x = bx - 10, p0y = by + 16, p1x = bx - 50, p1y = by + 8, p2x = bx - 52 + wag * 9, p2y = by - 34 - wag * 3;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i <= 20; i++) {
            float t = i / 20.0f, it = 1 - t;
            float cx = it * it * p0x + 2 * it * t * p1x + t * t * p2x;
            float cy = it * it * p0y + 2 * it * t * p1y + t * t * p2y;
            float r = 7 + sinf(t * 2.6f + 0.2f) * 9;
            if (pass == 0) { disc(cx, cy, r + 1.2f, PAL_PAINT(PAINT_RUST, 0)); continue; }
            Sh tc = t > 0.76f ? (Sh){ 0, PAL_PAINT(PAINT_WHITE, 0), PAL_PAINT(PAINT_WHITE, 1), PAL_WHITE, PAL_WHITE } : fur;
            tc.o = 0;
            ball(cx, cy, r, r, tc);
        }
}

static void minigun_tp(float hx, float hy, int f, int spin, int firing, float *mzx, float *mzy) {
    Sh gun = { PAL_RAMP(RAMP_STEEL, 0), PAL_RAMP(RAMP_STEEL, 2), PAL_RAMP(RAMP_STEEL, 4), PAL_RAMP(RAMP_STEEL, 6), PAL_RAMP(RAMP_STEEL, 7) };
    Sh pink = { PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_BUBBLEGUM, 0), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_WHITE };
    float jit = firing ? ((f & 1) ? 1.0f : -1.0f) : 0;
    float rx = hx + 48 + jit, ry = hy - 16;           /* rear housing, near the camera */
    float mx = 140 + jit * 0.5f, my = 66;             /* muzzle, down the corridor */
    *mzx = mx; *mzy = my;
    /* ammo drum (pink, with a heart) */
    ball(rx + 20, ry + 10, 14, 12, pink);
    tk_heart((int)rx + 20, (int)ry + 10, 2, PAL_PAINT(PAINT_CHERRY, 1));
    /* belt feed */
    for (int i = 0; i < 6; i++) {
        float t = (i + (spin & 1) * 0.5f) / 6.0f;
        float bxp = rx + 10 - t * 10, byp = ry + 2 - t * 4;
        tk_rect((int)bxp, (int)byp, 2, 4, PAL_RAMP(RAMP_CANDY, 5));
        tk_px((int)bxp, (int)byp, PAL_RAMP(RAMP_CANDY, 7));
    }
    /* barrel cluster in perspective */
    limb(rx, ry, mx, my, 12, 4.5f, gun);
    for (int b = 0; b < 6; b++) {
        float ph = spin * 0.9f + b * 1.047f;
        if (cosf(ph) < 0) continue;
        float o0 = sinf(ph) * 9, o1 = sinf(ph) * 3.2f;
        tk_line((int)(rx + o0), (int)(ry - o0 * 0.3f), (int)(mx + o1), (int)(my - o1 * 0.3f), PAL_RAMP(RAMP_STEEL, 1));
    }
    /* clamp rings */
    for (int i = 1; i <= 2; i++) {
        float t = i / 3.0f;
        ball(rx + (mx - rx) * t, ry + (my - ry) * t, 12 - t * 7.5f, 11 - t * 7, (Sh){ PAL_RAMP(RAMP_MINT, 2), PAL_RAMP(RAMP_MINT, 3), PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_MINT, 6), 0 });
    }
    /* front clamp + muzzle */
    ball(mx + 1, my + 1, 6, 5, gun);
    disc(mx + 1, my + 1, 2.5f, PAL_RAMP(RAMP_STEEL, 0));
    /* rear housing: the chunky back end, facing us */
    ball(rx, ry, 15, 14, gun);
    ball(rx, ry, 9, 8, (Sh){ PAL_RAMP(RAMP_STEEL, 0), PAL_RAMP(RAMP_STEEL, 1), PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 5), 0 });
    for (int b = 0; b < 6; b++) {
        float a = spin * 0.9f + b * 1.047f;
        disc(rx + cosf(a) * 5.5f, ry + sinf(a) * 5, 1.5f, PAL_RAMP(RAMP_STEEL, 0));
    }
    disc(rx, ry, 2, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    /* paws on the handles */
    fox_paw(rx - 15, ry + 8, 7);
    fox_paw(rx + 4, ry - 16, 6.5f);
}

/* ========================================================================= */
/* HUD                                                                        */
/* ========================================================================= */
enum { HUD_CUTE, HUD_BRUTAL, HUD_BOTH };
static void face_portrait(int x, int y, int expr, int mouth, int f, int blood) {
    layer_begin();
    tk_chibi(x + 14, y + 38, &TK_HERO, 1, expr, mouth, f, 0);
    layer_end(LAYER_A);
    for (int j = y; j < y + 25; j++)
        for (int i = x; i < x + 28; i++)
            if (LAYER_A[j][i]) tk_fb[j][i] = LAYER_A[j][i];
    if (blood) { tk_px(x + 9, y + 9, PAL_RAMP(RAMP_BLOOD, 4)); tk_px(x + 18, y + 13, PAL_RAMP(RAMP_BLOOD, 4)); tk_px(x + 19, y + 12, PAL_RAMP(RAMP_BLOOD, 5)); tk_px(x + 7, y + 15, PAL_RAMP(RAMP_BLOOD, 3)); }
}
static void hud(int mode, int ammo, const char *weapon, int f, int expr, int mouth, int blood, int pulse) {
    int y0 = VH;
    uint8_t bg, edge, hi, lab, txt;
    if (mode == HUD_CUTE) { bg = PAL_RAMP(RAMP_PASTEL_PINK, 6); edge = PAL_PAINT(PAINT_ROSE, 0); hi = PAL_WHITE; lab = PAL_PAINT(PAINT_ROSE, 0); txt = PAL_PAINT(PAINT_GRAPE, 0); }
    else if (mode == HUD_BRUTAL) { bg = PAL_RAMP(RAMP_STEEL, 1); edge = PAL_RAMP(RAMP_BLOOD, 3); hi = PAL_RAMP(RAMP_STEEL, 3); lab = PAL_UI_RED; txt = PAL_RAMP(RAMP_BONE, 7); }
    else { bg = PAL_RAMP(RAMP_VAPE, 2); edge = PAL_PAINT(PAINT_MAGENTA, 1); hi = PAL_RAMP(RAMP_VAPE, 4); lab = PAL_PAINT(PAINT_MAGENTA, 2); txt = PAL_WHITE; }
    tk_rect(0, y0, TW, TH - y0, edge);
    tk_rect(0, y0 + 1, TW, TH - y0 - 1, bg);
    tk_rect(0, y0 + 1, TW, 1, hi);
    /* little rivets / hearts in the frame */
    for (int x = 4; x < TW; x += 40) {
        if (mode == HUD_CUTE) tk_heart(x, y0 + 25, 1, PAL_RAMP(RAMP_PASTEL_PINK, 5));
        else disc(x, y0 + 25, 1, hi);
    }
    /* health hearts */
    tk_text(5, y0 + 4, "HEALTH", lab, 1);
    for (int i = 0; i < 5; i++) {
        int hb = (pulse && i == (f / 3) % 5) ? -1 : 0;
        tk_heart(9 + i * 10, y0 + 18 + hb, 1, mode == HUD_BRUTAL ? PAL_UI_RED : PAL_PAINT(PAINT_CHERRY, 1));
    }
    /* ammo */
    {
        int ax = 58;
        if (mode == HUD_CUTE) { disc(ax + 4, y0 + 15, 4, PAL_PAINT(PAINT_CARAMEL, 1)); disc(ax + 1, y0 + 11, 1.6f, PAL_PAINT(PAINT_CARAMEL, 1)); disc(ax + 7, y0 + 11, 1.6f, PAL_PAINT(PAINT_CARAMEL, 1)); tk_px(ax + 3, y0 + 14, PAL_BLACK); tk_px(ax + 5, y0 + 14, PAL_BLACK); }
        else if (mode == HUD_BRUTAL) { tk_rect(ax + 2, y0 + 8, 5, 10, PAL_UI_RED); tk_rect(ax + 2, y0 + 18, 5, 3, PAL_UI_GOLD); tk_rect(ax + 2, y0 + 8, 1, 10, PAL_PAINT(PAINT_CHERRY, 2)); }
        else { for (int i = 0; i < 3; i++) { tk_rect(ax + i * 3, y0 + 9, 2, 9, PAL_UI_GOLD); tk_px(ax + i * 3, y0 + 9, PAL_RAMP(RAMP_CANDY, 7)); } }
        char buf[8];
        int n = ammo < 0 ? 0 : ammo;
        int k = 0;
        if (n >= 100) buf[k++] = (char)('0' + n / 100 % 10);
        if (n >= 10) buf[k++] = (char)('0' + n / 10 % 10);
        buf[k++] = (char)('0' + n % 10);
        buf[k] = 0;
        tk_text_outline(ax + 11, y0 + 7, buf, txt, edge, 2);
    }
    /* face */
    int fx = 110, fy = y0 + 2;
    tk_rect(fx - 2, fy - 1, 32, 27, edge);
    tk_rect(fx - 1, fy, 30, 25, mode == HUD_CUTE ? PAL_PAINT(PAINT_SKY, 2) : mode == HUD_BRUTAL ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_VAPE, 4));
    face_portrait(fx, fy, expr, mouth, f, blood);
    /* weapon + gore setting */
    tk_text(146, y0 + 5, weapon, txt, 1);
    tk_text(146, y0 + 15, "GORE:", lab, 1);
    const char *g = mode == HUD_CUTE ? "CUTE" : mode == HUD_BRUTAL ? "BRUTAL" : "BOTH?!";
    tk_text(180, y0 + 15, g, mode == HUD_CUTE ? PAL_PAINT(PAINT_MAGENTA, 1) : mode == HUD_BRUTAL ? PAL_UI_RED : PAL_UI_GOLD, 1);
    if (mode == HUD_CUTE) tk_heart(212, y0 + 19, 1, PAL_PAINT(PAINT_MAGENTA, 1));
}

/* blood on the lens: irregular splats, droplets, drips */
static void lens_blood(int age) {
    if (age < 0) return;
    static const float S[][3] = { { 24, 24, 8 }, { 212, 34, 10 }, { 204, 100, 6 }, { 74, 116, 5 }, { 162, 14, 4 } };
    for (int i = 0; i < 5; i++) {
        float x = S[i][0], y = S[i][1], R = S[i][2] * (age < 2 ? 0.5f + age * 0.25f : 1);
        uint32_t q = tk_hash((uint32_t)i * 191u + 7u);
        float P[8][3];
        for (int k = 0; k < 8; k++) {
            float a = tk_rand(&q, 6283) / 1000.0f, d = tk_rand(&q, 100) / 100.0f * R * 0.7f;
            P[k][0] = x + cosf(a) * d; P[k][1] = y + sinf(a) * d * 0.8f; P[k][2] = R * (0.35f + tk_rand(&q, 50) / 100.0f);
        }
        for (int k = 0; k < 8; k++) disc(P[k][0], P[k][1], P[k][2] + 1, PAL_RAMP(RAMP_BLOOD, 1));
        for (int k = 0; k < 8; k++) disc(P[k][0], P[k][1], P[k][2], PAL_RAMP(RAMP_BLOOD, 3));
        disc(x - R * 0.35f, y - R * 0.35f, R * 0.22f + 0.4f, PAL_RAMP(RAMP_BLOOD, 5));
        tk_px((int)(x - R * 0.45f), (int)(y - R * 0.45f), PAL_RAMP(RAMP_BLOOD, 7));
        /* flung droplets */
        for (int k = 0; k < 7; k++) {
            float a = tk_rand(&q, 6283) / 1000.0f, d = R * (1.3f + tk_rand(&q, 100) / 60.0f);
            float r = 0.6f + tk_rand(&q, 10) / 10.0f;
            disc(x + cosf(a) * d, y + sinf(a) * d, r, PAL_RAMP(RAMP_BLOOD, 3));
        }
        /* drips run down */
        for (int k = 0; k < 2; k++) {
            float dx = x + (k ? R * 0.45f : -R * 0.25f);
            float len = age * (0.4f + i * 0.08f + k * 0.15f);
            if (len > 40) len = 40;
            tk_rect((int)dx, (int)(y + R * 0.5f), 2, (int)len, PAL_RAMP(RAMP_BLOOD, 3));
            tk_px((int)dx, (int)(y + R * 0.5f), PAL_RAMP(RAMP_BLOOD, 5));
            disc(dx + 1, y + R * 0.5f + len, 1.6f, PAL_RAMP(RAMP_BLOOD, 3));
        }
    }
}

/* ========================================================================= */
/* the scene                                                                  */
/* ========================================================================= */
static float imp1_x(int f) { return 20.5f - (f - F_IMP1) * 0.55f; }
static float imp2_x(int f) { return 20.5f - (f - F_IMP2) * 0.68f; }
static float teddy_x(int f, float cx) { return cx + 1.5f + (f - F_FIRE1) * 0.42f; }
static int hit1_frame(void) {
    for (int f = F_FIRE1; f < F_SWAP; f++) if (teddy_x(f, 2.9f) >= imp1_x(f)) return f;
    return F_FIRE1 + 10;
}

static void poof(float x, float y, int age, int red) {
    float sx, sy, d;
    if (age < 0 || age > 16) return;
    if (!project(x, y, 0.4f, &sx, &sy, &d)) return;
    float s = projY / d / 40.0f;
    for (int i = 0; i < 6; i++) {
        float a = i * 1.047f + age * 0.05f, r = (3 + age * 0.9f) * s * 6;
        float rr = (5 - age * 0.3f) * s * 6;
        if (rr < 1) continue;
        disc(sx + cosf(a) * r, sy + sinf(a) * r * 0.6f, rr + 1, red ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_VAPE, 3));
        disc(sx + cosf(a) * r, sy + sinf(a) * r * 0.6f, rr, red ? PAL_RAMP(RAMP_BLOOD, 4) : PAL_RAMP(RAMP_VAPE, 5));
    }
}

static void first_person(int f) {
    int brutal = f >= F_SWAP;
    /* walking forward a little, with a head bob */
    float walk = f < F_SWAP ? f * 0.012f : F_SWAP * 0.012f + (f - F_SWAP) * 0.004f;
    if (f >= F_JUDGE1) walk = F_SWAP * 0.012f + (F_JUDGE1 - F_SWAP) * 0.004f;
    float cxp = 2.0f + walk;
    float bobp = f * 0.28f;
    float hb = sinf(bobp * 2) * 1.2f;
    int shake = 0;
    g_flash = 0;
    if (brutal && f >= F_FIRE2 && f < F_FIRE2 + 4) g_flash = 4 - (f - F_FIRE2);
    set_camera(cxp, 3.5f + sinf(f * 0.03f) * 0.05f, 0.5f, sinf(f * 0.021f) * 0.03f, VH / 2.0f + hb);
    render_view();

    /* --- sprites --- */
    int hit1 = hit1_frame();
    float isx = 0, isy = 0, isc = 1;
    if (!brutal) {
        if (f >= F_IMP1 && f < hit1) {
            poof(imp1_x(F_IMP1), 3.5f, f - F_IMP1, 0);
            if (f >= F_IMP1 + 3) draw_imp(imp1_x(f), 3.5f, f, 1, &isx, &isy, &isc);
        }
        /* the teddy in flight */
        if (f >= F_FIRE1 && f < hit1) {
            float tx = teddy_x(f, cxp), sx, sy, d;
            float k = tk_clampf((f - F_FIRE1) / 6.0f, 0, 1);
            float ty = 3.5f + 0.32f * (1 - k), tz = 0.3f + 0.15f * k;     /* leaves the muzzle, homes in */
            if (project(tx, ty, tz, &sx, &sy, &d)) teddy(sx, sy, 0.11f * projY / d, f - F_FIRE1);
        }
        if (f >= hit1) {
            float hx = imp1_x(hit1), sx, sy, d;
            imp_plushie(hx - 0.3f, 3.6f, f, f - hit1);
            if (project(hx, 3.5f, 0.45f, &sx, &sy, &d)) {
                float s = projY / d / 60.0f;
                int a = f - hit1;
                if (a < 5) light_pool(sx, sy, 60, 50, L_PINK, 3 - a / 2, 1.0f);
                tk_confetti((int)sx, (int)sy, a, 11);
                tk_confetti((int)sx, (int)sy, a, 12);
                tk_hearts((int)sx, (int)sy - 4, a, 13);
                stuffing(sx, sy, a, 14, s * 2.2f);
                if (a < 6) { tk_star((int)sx, (int)sy, 14 - a * 2, PAL_WHITE); tk_star((int)sx, (int)sy, 7 - a, PAL_PAINT(PAINT_BUBBLEGUM, 2)); }
                if (a >= 2 && a < 24) {
                    float k = a < 6 ? tk_bounce((a - 2) / 4.0f) : 1;
                    layer_begin();
                    tk_text_outline(120 - tk_text_w("*squeak*", 1) / 2, 76, "*squeak*", PAL_WHITE, PAL_PAINT(PAINT_MAGENTA, 0), 1);
                    layer_end(LAYER_A);
                    layer_draw(LAYER_A, 120, 80, sx, sy - 18 - a * 0.5f, k, k, 0, 0, TH);
                }
            }
        }
    } else {
        /* the plushie is still lying there (it has seen things) */
        imp_plushie(imp1_x(hit1) - 0.3f, 3.6f, f, 99);
        if (f >= F_IMP2 && f < F_FIRE2) {
            poof(imp2_x(F_IMP2), 3.5f, f - F_IMP2, 1);
            if (f >= F_IMP2 + 3) draw_imp(imp2_x(f), 3.5f, f, 1, &isx, &isy, &isc);
        }
        if (f >= F_FIRE2) {
            float hx = imp2_x(F_FIRE2), sx, sy, d;
            if (project(hx, 3.5f, 0.45f, &sx, &sy, &d)) {
                float s = projY / d / 60.0f;
                int a = f - F_FIRE2;
                float floor_y = horiz + (cam_z - 0.0f) * projY / d;
                if (a < 30) { tk_gibs((int)sx, (int)sy, a, 21); tk_gibs((int)sx, (int)sy, a, 22); tk_gibs((int)sx, (int)sy - 6, a, 24); }
                chunks(sx, sy, floor_y + 6, a, 23, s * 1.5f);
                chunks(sx, sy - 8, floor_y + 4, a, 25, s * 1.1f);
                /* the initial blood spray: fast streaks */
                if (a < 7) {
                    uint32_t q = tk_hash(31);
                    for (int i = 0; i < 18; i++) {
                        float an = tk_rand(&q, 6283) / 1000.0f, sp = (1.5f + tk_rand(&q, 30) / 10.0f) * s * 4;
                        float x0 = sx + cosf(an) * sp * a, y0 = sy + sinf(an) * sp * a * 0.7f + a * a * 0.3f;
                        float x1 = sx + cosf(an) * sp * (a + 1.2f), y1 = sy + sinf(an) * sp * (a + 1.2f) * 0.7f + (a + 1) * (a + 1) * 0.3f;
                        tk_line((int)x0, (int)y0, (int)x1, (int)y1, PAL_RAMP(RAMP_BLOOD, 3));
                        tk_line((int)x0 + 1, (int)y0, (int)x1 + 1, (int)y1, PAL_RAMP(RAMP_BLOOD, 4));
                        disc(x1, y1, 1.2f, PAL_RAMP(RAMP_BLOOD, 4));
                    }
                }
                /* blood pool spreading on the floor */
                if (a > 6) {
                    float pr = (a < 30 ? (a - 6) * 0.8f : 19) * s * 1.5f;
                    for (int yy = (int)(floor_y - pr * 0.25f); yy <= (int)(floor_y + pr * 0.25f); yy++)
                        for (int xx = (int)(sx - pr); xx <= (int)(sx + pr); xx++) {
                            float u = (xx - sx) / pr, v = (yy - floor_y) / (pr * 0.25f + 0.01f);
                            if (u * u + v * v > 1 || xx < 0 || yy < 0 || xx >= VW || yy >= VH) continue;
                            tk_fb[yy][xx] = (u * u + v * v > 0.7f) ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_BLOOD, 2 + ((xx + yy) % 7 == 0));
                        }
                }
                if (a < 4) tk_star((int)sx, (int)sy, 18 - a * 4, PAL_RAMP(RAMP_CANDY, 7));
            }
        }
    }

    /* --- weapon --- */
    float wbx = sinf(bobp) * 5, wby = fabsf(cosf(bobp)) * 4;
    if (f >= F_JUDGE1) { wbx *= 0.3f; wby *= 0.3f; }
    if (!brutal) {
        plush_launcher_fp(wbx, wby, f - F_FIRE1, f < F_FIRE1 || f > F_FIRE1 + 18);
    } else {
        /* weapon swap: down / up */
        int a = f - F_SWAP;
        if (a < 6) plush_launcher_fp(wbx, wby + a * 14, -1, 1);
        else {
            float up = a < 14 ? (1 - tk_bounce((a - 6) / 8.0f)) * 90 : 0;
            shotgun_fp(wbx, wby + up, f - F_FIRE2, f - F_RELOAD);
            /* shells flying out on the reload */
            int r = f - F_RELOAD;
            if (r >= 6 && r < 24) {
                for (int i = 0; i < 2; i++) {
                    float k = r - 6;
                    float x = 118 + i * 10 + k * (2 + i), y = 128 - k * 5 + k * k * 0.35f;
                    float a2 = k * 0.6f + i;
                    float dx = cosf(a2) * 4, dy = sinf(a2) * 4;
                    tk_line((int)(x - dx), (int)(y - dy), (int)(x + dx), (int)(y + dy), PAL_UI_RED);
                    tk_line((int)(x - dx) + 1, (int)(y - dy), (int)(x + dx) + 1, (int)(y + dy), PAL_PAINT(PAINT_CHERRY, 2));
                    tk_px((int)(x + dx), (int)(y + dy), PAL_UI_GOLD);
                }
            }
        }
        if (f >= F_FIRE2 && f < F_FIRE2 + 7) shake = 7 - (f - F_FIRE2);
    }

    /* --- Father Fuzz disapproves --- */
    if (f >= F_FUZZ) {
        int a = f - F_FUZZ;
        float in = tk_ease(a / 8.0f);
        float x = -40 + in * 76;
        float shakeh = a > 8 ? sinf(a * 0.9f) * 2 : 0;
        layer_begin();
        tk_chibi(120, 150, &TK_FUZZ, 2, TKE_SLEEPY, TKM_FLAT, f, 0);
        layer_end(LAYER_A);
        /* lean in from the side, deadpan */
        layer_draw(LAYER_A, 120, 150, x + shakeh, 142, 1, 1, 0.12f, 0, TH);
        if (a > 10) {
            int bx = (int)x + 22, by = 60;
            tk_rect(bx, by, 30, 13, PAL_GRAY_D);
            tk_rect(bx + 1, by + 1, 28, 11, PAL_WHITE);
            tk_tri(bx + 3, by + 12, bx + 9, by + 12, bx - 2, by + 17, PAL_GRAY_D);
            tk_tri(bx + 4, by + 11, bx + 8, by + 11, bx, by + 15, PAL_WHITE);
            tk_text(bx + 3, by + 3, "tsk.", PAL_GRAY_D, 1);
        }
    }

    /* --- lens blood (brutal) --- */
    if (brutal) lens_blood(f - F_FIRE2 - 1);

    /* --- HUD --- */
    int expr = TKE_NORMAL, mouth = TKM_SMILE, blood = 0;
    if (!brutal) {
        if (f >= F_FIRE1 && f < hit1) { expr = TKE_HAPPY; mouth = TKM_OPEN; }
        else if (f >= hit1) { expr = TKE_LOVE; mouth = TKM_OPEN; }
        hud(HUD_CUTE, f >= F_FIRE1 ? 11 : 12, "PLUSH LAUNCHER", f, expr, mouth, 0, f >= hit1);
    } else {
        expr = TKE_ANGRY; mouth = TKM_FANGS;
        if (f >= F_FIRE2) { expr = TKE_SPARKLE; mouth = TKM_OPEN; blood = 1; }
        if (f >= F_JUDGE1) { expr = TKE_HAPPY; mouth = TKM_SMILE; }
        if (f >= F_JUDGE2) { expr = TKE_SMUG; mouth = TKM_FLAT; }
        if (f >= F_FUZZ + 10) { expr = TKE_SHOCK; mouth = TKM_O; }
        hud(HUD_BRUTAL, f >= F_FIRE2 ? (f >= F_RELOAD + 14 ? 8 : 6) : 8, "SUPER SHOTGUN", f, expr, mouth, blood, 0);
        if (f >= F_SWAP && f < F_SWAP + 3) tk_post_glitch(f, 30);
    }

    /* --- captions --- */
    if (f >= F_CUTE_CAP && f < F_SWAP) {
        slam_caption("CUTE MODE", 22, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_MAGENTA, 0), 3, f - F_CUTE_CAP);
        if (f > F_CUTE_CAP + 6) for (int i = 0; i < 2; i++) tk_heart(i ? 206 : 34, 22 + (int)(sinf(f * 0.25f + i * 3) * 2), 2, PAL_PAINT(PAINT_CHERRY, 1));
    }
    if (f >= F_BRUTAL_CAP && f < F_JUDGE1) {
        slam_caption("BRUTAL MODE", 22, PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 0), 3, f - F_BRUTAL_CAP);
        if (f > F_BRUTAL_CAP + 6) {
            int w = tk_text_w("BRUTAL MODE", 3);
            for (int i = 0; i < 7; i++) {
                int dx = 120 - w / 2 + 8 + i * 28;
                int len = 2 + ((f - F_BRUTAL_CAP) * (i + 3) / 8) % 10;
                tk_rect(dx, 34, 2, len, PAL_UI_RED_D);
                disc(dx + 1, 34 + len, 1.5f, PAL_UI_RED);
            }
        }
    }
    if (f >= F_JUDGE1 && f < F_JUDGE2) slam_caption("(We don't judge.)", 30, PAL_WHITE, PAL_GRAY_D, 2, f - F_JUDGE1);
    if (f >= F_JUDGE2) slam_caption("(We judge\na little.)", 32, PAL_WHITE, PAL_GRAY_D, 2, f - F_JUDGE2);

    if (shake) tk_post_shake((f & 1) ? shake : -shake, (f & 2) ? shake / 2 : -shake / 2);
}

/* ---- third person ---------------------------------------------------------- */
typedef struct { float x, y; int type; int pop; } TpDemon;
static const TpDemon TPD[] = {
    { 6.0f, 3.6f, 0, F_BRRT + 4 }, { 7.2f, 3.15f, 1, F_BRRT + 9 }, { 8.0f, 3.85f, 0, F_BRRT + 14 },
    { 9.2f, 3.4f, 1, F_BRRT + 19 }, { 10.4f, 3.1f, 0, F_BRRT + 24 }, { 11.2f, 3.8f, 0, F_BRRT + 30 },
    { 12.5f, 3.4f, 1, F_BRRT + 36 }, { 13.6f, 3.6f, 0, F_BRRT + 43 },
};
#define NTPD ((int)(sizeof TPD / sizeof TPD[0]))

static void puff_demon_bb(float x, float y, int f, float *sx, float *sy, float *sc) {
    /* a vape-cloud demon, billboarded via a capture */
    layer_begin();
    {
        float cx = 120, cy = 130;
        uint8_t o = PAL_RAMP(RAMP_VAPE, 1);
        float b = sinf(f * 0.4f) * 1.5f;
        tk_tri(108, (int)(cy - 14 + b), 113, (int)(cy - 18 + b), 104, (int)(cy - 28 + b), PAL_PAINT(PAINT_CHERRY, 0));
        tk_tri(132, (int)(cy - 14 + b), 127, (int)(cy - 18 + b), 136, (int)(cy - 28 + b), PAL_PAINT(PAINT_CHERRY, 0));
        float P[5][3] = { { -9, 0, 8 }, { 9, 0, 8 }, { 0, -8, 11 }, { -5, 6, 8 }, { 6, 6, 8 } };
        for (int i = 0; i < 5; i++) disc(cx + P[i][0], cy + P[i][1] + b, P[i][2] + 1, o);
        for (int i = 0; i < 5; i++) ball(cx + P[i][0], cy + P[i][1] + b, P[i][2], P[i][2], (Sh){ 0, PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 6), 0 });
        for (int e = -1; e <= 1; e += 2) { tk_rect((int)cx + e * 5 - 1, (int)(cy - 6 + b), 3, 3, PAL_UI_RED); tk_px((int)cx + e * 5, (int)(cy - 6 + b), PAL_RAMP(RAMP_LAVA, 7)); }
        tk_rect((int)cx - 4, (int)(cy + 1 + b), 9, 2, PAL_RAMP(RAMP_BLOOD, 1));
        tk_px((int)cx - 3, (int)(cy + 1 + b), PAL_WHITE); tk_px((int)cx + 3, (int)(cy + 1 + b), PAL_WHITE);
    }
    layer_end(LAYER_A);
    billboard(LAYER_A, 120, 150, 44, x, y, 0.0f, 0.8f, sx, sy, sc);
}

static void third_person(int f) {
    int a = f - F_TP;
    /* the camera pulls back and up over the shoulder */
    float k = tk_ease(a / 9.0f);
    float cxp = 2.7f - k * 1.2f, cz = 0.5f + k * 0.2f;
    int firing = f >= F_BRRT;
    float shake_amt = firing ? 1.0f : 0;
    g_flash = firing && (f & 1) ? 1 : 0;
    set_camera(cxp, 3.5f, cz, 0.0f, VH / 2.0f - k * 6);
    render_view();
    /* demons charge; each one pops in turn */
    int order[NTPD];
    for (int i = 0; i < NTPD; i++) order[i] = i;
    for (int i = 0; i < NTPD; i++) for (int j = i + 1; j < NTPD; j++) if (TPD[order[j]].x > TPD[order[i]].x) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    float tsx[NTPD], tsy[NTPD];
    int alive[NTPD];
    for (int i = 0; i < NTPD; i++) { tsx[i] = 120; tsy[i] = 60; alive[i] = 0; }
    for (int kk = 0; kk < NTPD; kk++) {
        int i = order[kk];
        float x = TPD[i].x - (f - F_TP) * 0.05f, y = TPD[i].y;
        alive[i] = f < TPD[i].pop;
        float sx = 0, sy = 0, sc = 1;
        if (alive[i]) {
            if (TPD[i].type == 0) draw_imp(x, y, f + i * 7, 1, &sx, &sy, &sc);
            else puff_demon_bb(x, y, f + i * 5, &sx, &sy, &sc);
            if (sx != 0 || sy != 0) { tsx[i] = sx; tsy[i] = sy - 14 * sc; }
        } else {
            float d;
            if (!project(x, y, 0.4f, &tsx[i], &tsy[i], &d)) continue;
            int pa = f - TPD[i].pop;
            if (i & 1) { tk_confetti((int)tsx[i], (int)tsy[i], pa, 40 + i); tk_hearts((int)tsx[i], (int)tsy[i], pa, 50 + i); }
            else { tk_gibs((int)tsx[i], (int)tsy[i], pa, 60 + i); chunks(tsx[i], tsy[i], tsy[i] + 14, pa, 70 + i, 0.8f); }
            if (pa < 3) tk_star((int)tsx[i], (int)tsy[i], 10 - pa * 3, PAL_WHITE);
        }
    }
    /* the hero, from behind, rises into frame */
    float rise = (1 - k) * 90;
    float hx = 76, hy = 128 + rise;
    float mzx, mzy;
    int spin = firing ? f * 3 : (f >= F_TP + 6 ? (f - F_TP - 6) * (f - F_TP - 6) / 8 : 0);
    hero_back(hx, hy, f, firing);
    minigun_tp(hx, hy, f, spin, firing, &mzx, &mzy);
    if (rise > 0) { /* nothing */ }
    if (firing) {
        /* tracers to the nearest living demon */
        int target = -1;
        for (int i = 0; i < NTPD; i++) if (alive[i]) { target = i; break; }
        if (target >= 0 && (f & 1)) {
            for (int t = 0; t < 2; t++) {
                float jx = (t ? 4 : -3), jy = (t ? -2 : 3);
                tk_line((int)mzx, (int)mzy, (int)(tsx[target] + jx), (int)(tsy[target] + jy), PAL_RAMP(RAMP_CANDY, 6));
            }
        }
        /* muzzle flash */
        int fr = f % 3;
        light_pool(mzx, mzy, 40, 30, L_FLASH, 2, 1.4f);
        disc(mzx - 2, mzy, 5 + fr * 2, PAL_RAMP(RAMP_LAVA, 6));
        disc(mzx - 2, mzy, 3 + fr, PAL_RAMP(RAMP_CANDY, 7));
        tk_star((int)mzx - 2, (int)mzy, 9 + fr * 3, PAL_RAMP(RAMP_CANDY, 7));
        /* casings arcing out to the right */
        for (int c = 0; c < 8; c++) {
            int t0 = f - c * 2;
            if (t0 < F_BRRT) continue;
            float age = (float)(f - t0);
            float x = hx + 58 + age * 4.5f + c, y = hy - 10 - age * 4 + age * age * 0.5f;
            tk_rect((int)x, (int)y, 3, 2, PAL_UI_GOLD);
            tk_px((int)x, (int)y, PAL_RAMP(RAMP_CANDY, 7));
        }
    }
    /* HUD */
    int ammo = firing ? 999 - (f - F_BRRT) * 13 : 999;
    hud(HUD_BOTH, ammo, "MINIGUN", f, firing ? TKE_SPARKLE : TKE_HAPPY, TKM_OPEN, 0, firing);
    /* caption */
    if (a >= 4) slam_caption("...or third person!", 20, PAL_PAINT(PAINT_LAVENDER, 2), PAL_RAMP(RAMP_VAPE, 0), 2, a - 4);
    if (shake_amt > 0) tk_post_shake((f & 1) ? 1 : -1, (f & 2) ? 1 : 0);
    if (a < 2) tk_post_flash(PAL_WHITE, 8 - a * 4);
}

void scene_s10_shooter(int f, int n) {
    (void)n;
    luts_init();
    g_f = f;
    tk_clear(PAL_BLACK);
    if (f < F_TP) first_person(f);
    else third_person(f);
}

void scene_s10_shooter_audio(double t0, double dur) {
#define AT(fr) (t0 + (fr) / 30.0)
    (void)dur;
    /* footsteps */
    for (int i = 0; i < 8; i++) tk_note(AT(4 + i * 10), 0.05, 40, TKV_TRI, 0.18f);
    /* CUTE MODE */
    tk_sfx(AT(F_CUTE_CAP), SFX_SPARKLE, 0.4f);
    tk_sfx(AT(F_IMP1), SFX_POP, 0.3f);
    for (int i = 0; i < 4; i++) tk_slide(AT(F_IMP1 + 4 + i * 4), 0.06, 74, 80, TKV_SQ12, 0.12f);   /* imp gibbering */
    tk_sfx(AT(F_FIRE1), SFX_BOING, 0.6f);          /* the plush launcher: thwoomp */
    tk_slide(AT(F_FIRE1), 0.12, 50, 70, TKV_SINE, 0.5f);
    {
        int h = hit1_frame();
        tk_sfx(AT(h), SFX_POP, 1.0f);
        tk_sfx(AT(h + 1), SFX_SQUEAK, 0.7f);
        tk_sfx(AT(h + 2), SFX_SPARKLE, 0.5f);
        tk_sfx(AT(h + 3), SFX_FANFARE, 0.25f);
        tk_note(AT(h + 12), 0.06, 84, TKV_SQ50, 0.12f);    /* the plushie bounces */
        tk_note(AT(h + 18), 0.05, 88, TKV_SQ50, 0.08f);
    }
    /* weapon swap + BRUTAL MODE */
    tk_sfx(AT(F_SWAP), SFX_WHOOSH, 0.4f);
    tk_sfx(AT(F_SWAP), SFX_GLITCH, 0.4f);
    tk_slide(AT(F_SWAP + 10), 0.06, 70, 50, TKV_NOISE, 0.5f);   /* chk */
    tk_slide(AT(F_SWAP + 13), 0.08, 60, 82, TKV_NOISE, 0.5f);   /* chk */
    tk_sfx(AT(F_BRUTAL_CAP), SFX_THUD, 1.0f);
    tk_sfx(AT(F_BRUTAL_CAP), SFX_ROAR, 0.35f);
    tk_sfx(AT(F_IMP2), SFX_SPLAT, 0.3f);
    for (int i = 0; i < 4; i++) tk_slide(AT(F_IMP2 + 4 + i * 4), 0.07, 62, 56, TKV_SAW, 0.14f);
    tk_sfx(AT(F_FIRE2), SFX_SHOTGUN, 1.0f);
    tk_sfx(AT(F_FIRE2), SFX_EXPLOSION, 0.4f);
    tk_sfx(AT(F_FIRE2 + 2), SFX_SPLAT, 1.0f);
    tk_sfx(AT(F_FIRE2 + 8), SFX_SPLAT, 0.4f);
    /* reload: break, shells, snap */
    tk_slide(AT(F_RELOAD + 2), 0.07, 66, 54, TKV_NOISE, 0.5f);
    tk_note(AT(F_RELOAD + 8), 0.05, 96, TKV_SQ25, 0.15f);
    tk_note(AT(F_RELOAD + 10), 0.05, 91, TKV_SQ25, 0.15f);
    tk_slide(AT(F_RELOAD + 20), 0.06, 58, 78, TKV_NOISE, 0.6f);
    /* judgement: a quiet beat, then a sad trombone-ish kazoo for Father Fuzz */
    tk_note(AT(F_JUDGE1), 0.08, 72, TKV_SQ25, 0.15f);
    tk_note(AT(F_JUDGE2), 0.08, 67, TKV_SQ25, 0.15f);
    tk_sfx(AT(F_FUZZ + 10), SFX_KAZOO, 0.5f);
    tk_note(AT(F_FUZZ + 12), 0.04, 90, TKV_SQ12, 0.15f);    /* tsk */
    tk_note(AT(F_FUZZ + 15), 0.04, 90, TKV_SQ12, 0.15f);
    /* third person: whoosh, spin-up, BRRRRT */
    tk_sfx(AT(F_TP), SFX_WHOOSH, 0.6f);
    tk_slide(AT(F_TP + 4), 0.4, 40, 76, TKV_SAW, 0.25f);
    for (int i = 0; i < 46; i++) {
        tk_note(AT(F_BRRT + i), 0.03, 36 + (i & 1) * 2, TKV_SQ50, 0.32f);
        tk_slide(AT(F_BRRT + i), 0.03, 90, 70, TKV_NOISE, 0.32f);
    }
    for (int i = 0; i < NTPD; i++) tk_sfx(AT(TPD[i].pop), (i & 1) ? SFX_POP : SFX_SPLAT, 0.5f);
    tk_slide(AT(F_BRRT + 46), 0.5, 76, 40, TKV_SAW, 0.2f);   /* spin down */
#undef AT
}
