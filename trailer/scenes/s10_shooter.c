/* s10_shooter.c - THE DOOM PART.   (8.5 s, 255 frames)
 *
 * A tiny first-person raycaster lives in this file: grid map with per-cell
 * floor heights (lava pits below, steps up to a platform - with visible
 * risers), procedural hell textures (corrupted pastel brick, flesh wall with
 * a blinking eyeball, hell brick with glowing cracks, a lava-fall end wall),
 * distance fog through a palette shade table, and a per-pixel depth buffer
 * so sprites tuck behind pillars and steps.
 *
 * f  0- 69  CUTE MODE: an impling charges down the Vape Cellar. The PLUSH
 *           LAUNCHER fires a teddy - the imp bursts into confetti, hearts and
 *           stuffing. The teddy plops onto the floor. HUD face: heart eyes.
 * f 70-129  BRUTAL MODE: weapon swap, the HUD turns hellish, the very same
 *           imp respawns (poof), SUPER SHOTGUN: flash, shake, gibs, blood on
 *           the lens. Break-open reload, shells fly.
 * f130-194  "(We don't judge.)" ... beat ... "(We judge a little.)"
 *           Father Fuzz peeks over the HUD, shaking his head. Tsk tsk.
 * f195-254  THIRD PERSON: over the shoulder of the hero holding a minigun.
 *           BRRRRT. Demons pop into confetti AND gibs. HUD: "GORE: BOTH?!"
 *           "...or third person!"
 */
#include "tk.h"
#include "pal.h"
#include <math.h>
#include <string.h>

#define VW 240
#define VH 136
#define CEIL_H 1.4f

#define F_CUTE_FIRE   24
#define F_CUTE_HIT    34
#define F_SWAP        70
#define F_RESPAWN     74
#define F_BRUTAL_FIRE 102
#define F_JUDGE1      132
#define F_JUDGE2      166
#define F_TP          195
#define F_BRRT        203

/* ======================================================================== */
/* shade table: palette index x light level -> palette index (fog: maroon)  */
/* ======================================================================== */
static uint8_t SH[16][256];
static int sh_ready;
static int pal_nearest(int r, int g, int b) {
    int best = 1; long bd = 1L << 40;
    for (int i = 1; i < 256; i++) {
        long rm = (TK_PAL[i][0] + r) / 2;
        long dr = TK_PAL[i][0] - r, dg = TK_PAL[i][1] - g, db = TK_PAL[i][2] - b;
        long d = (((512 + rm) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rm) * db * db) >> 8);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
static void build_shade(void) {
    if (sh_ready) return;
    sh_ready = 1;
    const int fr = 26, fg = 6, fb = 14;
    for (int l = 0; l < 16; l++)
        for (int i = 0; i < 256; i++) {
            if (l == 15 || i == 0) { SH[l][i] = (uint8_t)i; continue; }
            int r = fr + (TK_PAL[i][0] - fr) * l / 15, g = fg + (TK_PAL[i][1] - fg) * l / 15, b = fb + (TK_PAL[i][2] - fb) * l / 15;
            SH[l][i] = (uint8_t)pal_nearest(r, g, b);
        }
}

/* ======================================================================== */
/* the map: MAP[y][x], x = forward                                          */
/* ======================================================================== */
static const char *const MAP[] = {
    "################",
    "#HHHHPPPPFFFFHHD",
    "#....ll.S..;,,,D",
    "#..........;,,,D",
    "#....ll.S..;,,,D",
    "#HHHHPPPPFFFFHHD",
    "################",
};
#define MAP_W 16
#define MAP_H 7
static char cell(int x, int y) {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return '#';
    return MAP[y][x];
}
static int is_wall(char c) { return c == '#' || c == 'H' || c == 'P' || c == 'F' || c == 'S' || c == 'D'; }
static float floor_h(char c) {
    switch (c) { case 'l': return -0.18f; case ';': return 0.12f; case ',': return 0.25f; default: return 0.0f; }
}
static float floor_at(float x, float y) { return floor_h(cell((int)floorf(x), (int)floorf(y))); }

/* camera */
static float cx_, cy_, cdx, cdy, cpx, cpy, cz, horiz, projY;
static int g_f, g_light_boost;
static float zbuf[VH][VW];

static void set_camera(float x, float y, float z, float yaw, float hz) {
    cx_ = x; cy_ = y; cz = z; horiz = hz;
    cdx = cosf(yaw); cdy = sinf(yaw);
    const float plane = 0.66f;
    cpx = -cdy * plane; cpy = cdx * plane;
    projY = (VW / 2) / plane;
}

/* ---- textures: return palette index, | 0x100 for emissive ------------ */
static float fracf(float v) { return v - floorf(v); }
static uint32_t h2(int a, int b) { return tk_hash((uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u); }

static int tex_brick(float u, float v, int cxi, int cyi, int pastel) {
    float row = v / 0.175f;
    int r = (int)floorf(row);
    float bu = u * 2 + ((r & 1) ? 0.5f : 0);
    int bc = (int)floorf(bu);
    float fu = fracf(bu), fv = fracf(row);
    uint32_t hh = h2(cxi * 31 + bc, cyi * 17 + r);
    if (fu < 0.05f || fv < 0.1f) return pastel ? PAL_RAMP(RAMP_PASTEL_PINK, 6) : PAL_RAMP(RAMP_STONE, 1);
    if (pastel) {
        int base = (hh & 3) == 0 ? 4 : 5;
        if (fv > 0.8f) base--;
        if (fu < 0.12f && fv < 0.4f) base = 6;
        return PAL_RAMP(RAMP_PASTEL_PINK, base);
    }
    int base = 3 + (int)(hh % 3) - 1;
    if (fv > 0.82f) base -= 1;
    if (base < 1) base = 1;
    if ((hh >> 4) % 11 == 0) {   /* the odd glowing crack */
        float cyv = 0.5f + sinf(fu * 12.0f + (float)(hh & 7)) * 0.15f;
        if (fabsf(fv - cyv) < 0.07f && fu > 0.15f && fu < 0.85f) return PAL_RAMP(RAMP_LAVA, 4 + ((g_f / 6 + (int)hh) & 1)) | 0x100;
    }
    return PAL_RAMP(RAMP_STONE, base);
}

static int tex_flesh(float u, float v, int cxi, int cyi) {
    float b = sinf(u * 9.0f + cxi) * sinf(v * 8.0f + cyi) + sinf((u + v) * 15.0f) * 0.3f;
    int c = b > 0.4f ? 5 : (b > -0.2f ? 4 : 3);
    float vein = fabsf(sinf(u * 11.0f + sinf(v * 6.0f + cxi) * 2.5f));
    if (vein < 0.09f) return PAL_RAMP(RAMP_BLOOD, 3 + ((g_f / 6) & 1));
    /* an eyeball in some panels: it follows the action, and blinks */
    if ((h2(cxi, cyi) & 1) == 0) {
        float ex = u - 0.5f, ey = v - 0.85f;
        float d2 = ex * ex + ey * ey * 1.2f;
        if (d2 < 0.024f) {
            int blink = ((g_f + cxi * 13) % 70) < 3;
            if (blink) return PAL_RAMP(RAMP_FLESH, 2);
            float lx = ex - 0.04f, ly = ey;
            float d = lx * lx + ly * ly;
            if (d < 0.0016f) return PAL_BLACK;
            if (d < 0.005f) return PAL_UI_RED;
            if (d2 > 0.019f) return PAL_RAMP(RAMP_FLESH, 2);
            return PAL_RAMP(RAMP_BONE, 7);
        }
    }
    return PAL_RAMP(RAMP_FLESH, c);
}

static int tex_wall(char c, float u, float v, int cxi, int cyi) {
    switch (c) {
    case 'P': {   /* pastel brick, rotting into flesh from the floor up */
        float line = 0.45f + sinf(u * 9.0f + cxi * 2.1f) * 0.12f + sinf(u * 23.0f) * 0.04f;
        if (v < line) {
            if (v > line - 0.05f) return PAL_RAMP(RAMP_FLESH, 2);
            return tex_flesh(u, v, cxi, cyi);
        }
        /* drips of rot running down */
        float drip = fracf(u * 3.0f + cxi * 0.37f);
        if (drip < 0.07f && v < line + 0.25f * fracf(cxi * 0.61f + u)) return PAL_RAMP(RAMP_FLESH, 3);
        /* heart graffiti */
        float hx = (u - 0.5f) * 1.2f, hy = (v - 1.0f) * 1.4f;
        float hf = powf(hx * hx + hy * hy - 0.04f, 3) - hx * hx * hy * hy * hy * 0.8f;
        if ((cxi & 1) && hf < 0 && hx * hx + hy * hy < 0.09f) return PAL_PAINT(PAINT_CHERRY, 1);
        return tex_brick(u, v, cxi, cyi, 1);
    }
    case 'F': return tex_flesh(u, v, cxi, cyi);
    case 'S': {   /* pillar: dark stone with a carved skull + torch */
        int b = tex_brick(u * 0.5f, v, cxi, cyi, 0);
        float sx = (u - 0.5f), sy = (v - 0.9f);
        if (sx * sx + sy * sy * 0.8f < 0.03f) {
            if ((fabsf(sx + 0.07f) < 0.04f || fabsf(sx - 0.07f) < 0.04f) && fabsf(sy - 0.02f) < 0.04f) return PAL_UI_RED | 0x100;
            if (sy < -0.1f && fabsf(sx) < 0.08f && fracf(u * 25.0f) < 0.5f) return PAL_RAMP(RAMP_BONE, 3);
            return PAL_RAMP(RAMP_BONE, 5);
        }
        return b;
    }
    case 'D': {   /* lava fall */
        float s = sinf(u * 25.0f + sinf(v * 3.0f) * 2.0f) + sinf((v * 7.0f + g_f * 0.35f) + u * 4.0f);
        int lv = s > 1.0f ? 7 : (s > 0.2f ? 6 : (s > -0.6f ? 5 : 4));
        return PAL_RAMP(RAMP_LAVA, lv) | 0x100;
    }
    default: return tex_brick(u, v, cxi, cyi, 0);
    }
}

static int tex_floor(char c, float wx, float wy) {
    if (c == 'l') {
        float s = sinf(wx * 6.0f + g_f * 0.12f) + sinf(wy * 7.0f - g_f * 0.09f + wx * 2.0f);
        int lv = s > 1.2f ? 7 : (s > 0.3f ? 6 : (s > -0.7f ? 5 : 4));
        return PAL_RAMP(RAMP_LAVA, lv) | 0x100;
    }
    float fx = fracf(wx), fy = fracf(wy);
    if (c == ';' || c == ',') {     /* metal grating + hazard edge */
        if (c == ';' && fx < 0.14f) return (((int)floorf((wy) * 8.0f + fx * 8.0f)) & 1) ? PAL_UI_GOLD : PAL_BLACK;
        if (fracf(wx * 4) < 0.12f || fracf(wy * 4) < 0.12f) return PAL_RAMP(RAMP_STEEL, 1);
        return PAL_RAMP(RAMP_STEEL, 3);
    }
    /* checker tiles: cool stone + raspberry, grout */
    if (fracf(wx * 2) < 0.06f || fracf(wy * 2) < 0.06f) return PAL_RAMP(RAMP_STONE, 1);
    int chk = ((int)floorf(wx * 2) + (int)floorf(wy * 2)) & 1;
    uint32_t hh = h2((int)floorf(wx * 2), (int)floorf(wy * 2));
    if (!chk) return PAL_RAMP(RAMP_STONE, 3 - (int)(hh & 1));
    (void)fx; (void)fy;
    return PAL_RAMP(RAMP_PASTEL_PINK, 1 + (int)(hh & 1));
}
static int tex_ceil(float wx, float wy) {
    if (fracf(wx) < 0.14f) return PAL_RAMP(RAMP_WOOD, 1);
    if (fracf(wx) < 0.18f) return PAL_RAMP(RAMP_WOOD, 0);
    (void)wy;
    return PAL_RAMP(RAMP_STONE, 1);
}
static int tex_riser(float u, float v) {
    if (v > -0.03f && fracf(u * 6) < 0.5f) return PAL_UI_GOLD;
    return PAL_RAMP(RAMP_STEEL, 2);
}

static uint8_t lit(int texel, float t) {
    if (texel & 0x100) return (uint8_t)(texel & 0xFF);
    int l = (int)(15.5f - t * 1.05f) + g_light_boost;
    if (l > 15) l = 15;
    if (l < 1) l = 1;
    return SH[l][texel & 0xFF];
}

/* ---- the raycaster -------------------------------------------------------- */
static int scr_y(float h, float t) { return (int)floorf(horiz + (cz - h) * projY / t); }

static void render_view(void) {
    for (int x = 0; x < VW; x++) {
        float camX = 2.0f * (x + 0.5f) / VW - 1.0f;
        float rdx = cdx + cpx * camX, rdy = cdy + cpy * camX;
        int mx = (int)floorf(cx_), my = (int)floorf(cy_);
        float ddx = rdx == 0 ? 1e30f : fabsf(1.0f / rdx), ddy = rdy == 0 ? 1e30f : fabsf(1.0f / rdy);
        int sx, sy; float sdx, sdy;
        if (rdx < 0) { sx = -1; sdx = (cx_ - mx) * ddx; } else { sx = 1; sdx = (mx + 1.0f - cx_) * ddx; }
        if (rdy < 0) { sy = -1; sdy = (cy_ - my) * ddy; } else { sy = 1; sdy = (my + 1.0f - cy_) * ddy; }
        int ybot = VH, ytop = 0;
        float t0 = 0.02f;
        char cc = cell(mx, my);
        float hcur = floor_h(cc);
        for (int it = 0; it < 64 && ytop < ybot; it++) {
            int side = sdx < sdy ? 0 : 1;
            float t1 = side == 0 ? sdx : sdy;
            /* floor of this cell */
            int yf = scr_y(hcur, t1);
            if (yf < ytop) yf = ytop;
            for (int y = (yf > (int)horiz ? yf : (int)horiz + 1); y < ybot; y++) {
                float t = (cz - hcur) * projY / (y + 0.5f - horiz);
                if (t <= 0) continue;
                float wx = cx_ + rdx * t, wy = cy_ + rdy * t;
                tk_fb[y][x] = lit(tex_floor(cc, wx, wy), t);
                zbuf[y][x] = t;
            }
            if (yf < ybot) ybot = yf > (int)horiz ? yf : (int)horiz + 1;
            if (ybot < ytop) ybot = ytop;
            /* ceiling of this cell */
            int yc = scr_y(CEIL_H, t1) + 1;
            if (yc > ybot) yc = ybot;
            for (int y = ytop; y < yc && y < (int)horiz; y++) {
                float t = (CEIL_H - cz) * projY / (horiz - y - 0.5f);
                if (t <= 0) continue;
                float wx = cx_ + rdx * t, wy = cy_ + rdy * t;
                tk_fb[y][x] = lit(tex_ceil(wx, wy), t);
                zbuf[y][x] = t;
            }
            if (yc > ytop) ytop = yc;
            /* step into the next cell */
            if (side == 0) { sdx += ddx; mx += sx; } else { sdy += ddy; my += sy; }
            char nc = cell(mx, my);
            float wpos = side == 0 ? cy_ + t1 * rdy : cx_ + t1 * rdx;
            float u = fracf(wpos);
            if (is_wall(nc)) {
                int top = scr_y(CEIL_H, t1), bot = scr_y(hcur, t1) + 1;
                if (top < ytop) top = ytop;
                if (bot > ybot) bot = ybot;
                for (int y = top; y < bot; y++) {
                    float v = cz - (y + 0.5f - horiz) * t1 / projY;
                    tk_fb[y][x] = lit(tex_wall(nc, u, v, mx * 3 + my, my), t1);
                    zbuf[y][x] = t1;
                }
                break;
            }
            float hn = floor_h(nc);
            if (hn > hcur) {   /* a riser: the front face of a step */
                int top = scr_y(hn, t1), bot = scr_y(hcur, t1) + 1;
                if (top < ytop) top = ytop;
                if (bot > ybot) bot = ybot;
                for (int y = top; y < bot; y++) {
                    float v = cz - (y + 0.5f - horiz) * t1 / projY - hn;
                    tk_fb[y][x] = lit(tex_riser(u, v), t1);
                    zbuf[y][x] = t1;
                }
                if (top < ybot) ybot = top;
            } else if (hn < hcur) {
                /* dropping into a pit: the far wall of the pit shows up later */
            }
            hcur = hn; cc = nc; t0 = t1;
        }
        (void)t0;
    }
}

/* project a world point; returns 0 if behind the camera */
static int project(float wx, float wy, float wz, float *sx, float *sy, float *depth) {
    float rx = wx - cx_, ry = wy - cy_;
    float inv = 1.0f / (cpx * cdy - cdx * cpy);
    float tx = inv * (cdy * rx - cdx * ry);
    float ty = inv * (-cpy * rx + cpx * ry);
    if (ty < 0.1f) return 0;
    *sx = (VW / 2.0f) * (1 + tx / ty);
    *sy = horiz + (cz - wz) * projY / ty;
    *depth = ty;
    return 1;
}

/* ---- chibi capture --------------------------------------------------------- */
#define CAPW 120
#define CAPH 130
#define CAP_X (TW / 2)
#define CAP_Y 140
typedef struct { uint8_t px[CAPH][CAPW]; } Cap;
static uint8_t s_save[TH][TW];
static void cap_begin(void) { memcpy(s_save, tk_fb, sizeof s_save); memset(tk_fb, 0, sizeof tk_fb); }
static void cap_end(Cap *c) {
    for (int y = 0; y < CAPH; y++)
        for (int x = 0; x < CAPW; x++) {
            int sx = CAP_X - CAPW / 2 + x, sy = CAP_Y - CAPH + 10 + y;
            c->px[y][x] = (sx >= 0 && sx < TW && sy >= 0 && sy < TH) ? tk_fb[sy][sx] : 0;
        }
    memcpy(tk_fb, s_save, sizeof s_save);
}
/* billboard a captured chibi standing at world (wx,wy,wz); `hw` = world
 * height of the 54-px-tall size-2 chibi. depth-tested + fogged */
static int cap_world(const Cap *c, float wx, float wy, float wz, float hw, float *ssx, float *ssy, float *sscale) {
    float sx, sy, d;
    if (!project(wx, wy, wz, &sx, &sy, &d)) return 0;
    float sc = (hw * projY / d) / 54.0f;
    const int ax = CAPW / 2, ay = CAPH - 10;
    int x0 = (int)(sx - ax * sc) - 1, x1 = (int)(sx + (CAPW - ax) * sc) + 1;
    int y0 = (int)(sy - ay * sc) - 1, y1 = (int)(sy + 10 * sc) + 1;
    if (x0 < 0) x0 = 0;
    if (x1 > VW) x1 = VW;
    if (y0 < 0) y0 = 0;
    if (y1 > VH) y1 = VH;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int u = ax + (int)floorf((x - sx) / sc), v = ay + (int)floorf((y - sy) / sc);
            if (u < 0 || v < 0 || u >= CAPW || v >= CAPH) continue;
            uint8_t p = c->px[v][u];
            if (!p || zbuf[y][x] < d) continue;
            tk_fb[y][x] = lit(p, d * 0.8f);
        }
    if (ssx) { *ssx = sx; *ssy = sy; *sscale = sc; }
    return 1;
}

/* ---- the Impling's path (stage 1 & 2) ------------------------------------- */
static float imp_x(int f, int start, float speed) { return 13.4f - (f - start) * speed; }
static float imp_y(int f) { return 3.5f + sinf(f * 0.25f) * 0.25f; }

/* ---- first-person weapons --------------------------------------------------- */
static void hand(int x, int y, int flip) {
    uint8_t sk = PAL_SKIN(SKIN_PEACH, 1), skd = PAL_SKIN(SKIN_PEACH, 0);
    tk_rect(x - 8, y - 1, 17, 14, PAL_BLACK);
    tk_rect(x - 7, y, 15, 12, sk);
    for (int i = 0; i < 3; i++) tk_rect(x - 6 + i * 5 + (flip ? 1 : 0), y + 1, 1, 6, skd);
    tk_rect(x - 7, y + 10, 15, 2, skd);
    /* sleeve */
    tk_rect(x - 9 + (flip ? 2 : -2), y + 12, 19, 30, PAL_BLACK);
    tk_rect(x - 8 + (flip ? 2 : -2), y + 13, 17, 30, PAL_PAINT(PAINT_MINT, 1));
    tk_rect(x - 8 + (flip ? 2 : -2), y + 13, 3, 30, PAL_PAINT(PAINT_MINT, 2));
}

static void trapezoid(int cx, int y0, int y1, int w0, int w1, uint8_t c) {
    /* y0 = top (width w0), y1 = bottom (width w1) */
    for (int y = y0; y <= y1; y++) {
        int w = w0 + (w1 - w0) * (y - y0) / (y1 - y0 > 0 ? y1 - y0 : 1);
        tk_rect(cx - w / 2, y, w, 1, c);
    }
}

/* mood: 0 content, 1 angry (in flight), 2 horrified + splattered */
static void teddy_head(int x, int y, int r, int mood) {
    tk_circle(x - r * 2 / 3, y - r * 3 / 4, r / 3 + 1, PAL_BLACK); tk_circle(x + r * 2 / 3, y - r * 3 / 4, r / 3 + 1, PAL_BLACK);
    tk_circle(x, y, r + 1, PAL_BLACK);
    tk_circle(x - r * 2 / 3, y - r * 3 / 4, r / 3, PAL_PAINT(PAINT_CARAMEL, 1)); tk_circle(x + r * 2 / 3, y - r * 3 / 4, r / 3, PAL_PAINT(PAINT_CARAMEL, 1));
    tk_circle(x, y, r, PAL_PAINT(PAINT_CARAMEL, 1));
    tk_ellipse(x, y + r / 3, r / 2, r / 3, PAL_PAINT(PAINT_CARAMEL, 2));
    int e = r >= 6 ? 2 : 1;
    if (mood == 2) {   /* wide eyes, tiny o mouth */
        tk_rect(x - r / 3 - e / 2 - 1, y - r / 4 - e / 2 - 1, e + 2, e + 2, PAL_WHITE);
        tk_rect(x + r / 3 - e / 2 - 1, y - r / 4 - e / 2 - 1, e + 2, e + 2, PAL_WHITE);
        tk_px(x - r / 3, y - r / 4, PAL_BLACK); tk_px(x + r / 3, y - r / 4, PAL_BLACK);
        tk_ring(x, y + r / 3, 1, PAL_BLACK);
        tk_px(x - r / 2, y - r / 2, PAL_RAMP(RAMP_BLOOD, 4)); tk_px(x + r / 2 + 1, y, PAL_RAMP(RAMP_BLOOD, 4));
        tk_px(x - r / 2 + 1, y + r / 2, PAL_RAMP(RAMP_BLOOD, 3)); tk_px(x + 1, y - r + 1, PAL_RAMP(RAMP_BLOOD, 4));
        return;
    }
    tk_rect(x - r / 3 - e / 2, y - r / 4 - e / 2, e, e + (r >= 6), PAL_BLACK);
    tk_rect(x + r / 3 - e / 2, y - r / 4 - e / 2, e, e + (r >= 6), PAL_BLACK);
    tk_px(x, y + r / 4, PAL_BLACK);
    if (mood == 1 && r >= 5) { tk_line(x - r / 2, y - r / 2, x - r / 6, y - r / 3, PAL_BLACK); tk_line(x + r / 2, y - r / 2, x + r / 6, y - r / 3, PAL_BLACK); }
}

static void plush_launcher_fp(int bx, int by, int fire_age, int loaded) {
    int cx = 128 + bx, top = 96 + by, bot = 150 + by;
    int rec = fire_age >= 0 && fire_age < 6 ? (6 - fire_age) * 2 : 0;
    top += rec; bot += rec;
    hand(cx - 34, top + 30, 0);
    trapezoid(cx, top, bot, 40, 64, PAL_BLACK);
    trapezoid(cx, top + 1, bot, 38, 62, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    trapezoid(cx - 10, top + 2, bot, 10, 16, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    trapezoid(cx + 14, top + 2, bot, 8, 14, PAL_PAINT(PAINT_BUBBLEGUM, 0));
    tk_rect(cx - 15, top + 6, 2, 12, PAL_WHITE);
    /* stickers */
    tk_heart(cx - 4, top + 26, 1, PAL_PAINT(PAINT_CHERRY, 1));
    tk_star(cx + 8, top + 18, 3, PAL_PAINT(PAINT_LEMON, 1));
    tk_circle(cx + 2, top + 38, 4, PAL_PAINT(PAINT_LEMON, 1));
    tk_px(cx, top + 37, PAL_BLACK); tk_px(cx + 4, top + 37, PAL_BLACK);
    tk_px(cx + 1, top + 40, PAL_BLACK); tk_px(cx + 2, top + 40, PAL_BLACK); tk_px(cx + 3, top + 40, PAL_BLACK);
    /* muzzle ring */
    tk_ellipse(cx, top, 22, 7, PAL_BLACK);
    tk_ellipse(cx, top, 21, 6, PAL_PAINT(PAINT_MINT, 1));
    tk_ellipse(cx, top, 15, 4, PAL_PAINT(PAINT_GRAPE, 0));
    tk_rect(cx - 18, top - 3, 6, 1, PAL_PAINT(PAINT_MINT, 2));
    if (loaded) {
        teddy_head(cx, top - 5, 9, 0);
    }
    /* heart sight */
    tk_heart(cx, top - 10 - (loaded ? 10 : 0), 1, PAL_PAINT(PAINT_MAGENTA, 1));
    hand(cx + 36, top + 34, 1);
    /* the shot: pink puff + hearts */
    if (fire_age >= 0 && fire_age < 7) {
        int r = fire_age < 4 ? 5 + fire_age * 2 : 11 - (fire_age - 4) * 3;
        for (int i = 0; i < 5; i++) {
            float an = i * 1.256f + 0.6f;
            int px = cx + (int)(cosf(an) * (6 + fire_age * 3)), py = top - 6 + (int)(sinf(an) * (3 + fire_age * 2)) - fire_age * 2;
            tk_circle(px, py, r / 2 + 1, PAL_PAINT(PAINT_BUBBLEGUM, 2));
            tk_circle(px - 1, py - 1, r / 4 + 1, PAL_WHITE);
        }
        tk_hearts(cx, top - 4, fire_age * 2, 77);
    }
}

static void shotgun_fp(int bx, int by, int fire_age, int reload_age) {
    int cx = 122 + bx, top = 80 + by, bot = 150 + by;
    int rec = fire_age >= 0 && fire_age < 8 ? (8 - fire_age) * 3 : 0;
    int drop = 0;
    if (reload_age >= 0 && reload_age < 22) drop = (int)(sinf(reload_age / 22.0f * 3.14159f) * 36);
    top += rec + drop; bot += rec + drop;
    /* twin barrels */
    for (int s = -1; s <= 1; s += 2) {
        int bxc = cx + s * 12;
        trapezoid(bxc + s * 2, top, bot, 20, 32, PAL_BLACK);
        trapezoid(bxc + s * 2, top + 1, bot, 18, 30, PAL_RAMP(RAMP_STEEL, 3));
        trapezoid(bxc + s * 2 - s * 5, top + 1, bot, 4, 8, PAL_RAMP(RAMP_STEEL, 2));
        trapezoid(bxc + s * 2 + s * 4 - 2 * s, top + 1, bot, 3, 5, PAL_RAMP(RAMP_STEEL, 5));
        tk_ellipse(bxc + s * 2, top + 1, 9, 4, PAL_BLACK);
        tk_ellipse(bxc + s * 2, top + 1, 8, 3, PAL_RAMP(RAMP_STEEL, 5));
        tk_ellipse(bxc + s * 2, top + 1, 5, 2, PAL_BLACK);
    }
    /* rib between the barrels with a bead sight */
    tk_rect(cx - 1, top - 3, 3, 30, PAL_BLACK);
    tk_rect(cx, top - 2, 1, 29, PAL_RAMP(RAMP_STEEL, 6));
    tk_rect(cx - 1, top - 5, 3, 3, PAL_UI_RED);
    /* spikes welded on the sides */
    for (int i = 0; i < 3; i++) {
        int y = top + 8 + i * 12, w = 34 + i * 4;
        tk_tri(cx - w - 3, y, cx - w + 6, y - 3, cx - w + 6, y + 4, PAL_BLACK);
        tk_tri(cx + w + 3, y, cx + w - 6, y - 3, cx + w - 6, y + 4, PAL_BLACK);
        tk_tri(cx - w - 1, y, cx - w + 5, y - 2, cx - w + 5, y + 3, PAL_RAMP(RAMP_BONE, 6));
        tk_tri(cx + w + 1, y, cx + w - 5, y - 2, cx + w - 5, y + 3, PAL_RAMP(RAMP_BONE, 6));
    }
    /* wooden fore-end */
    trapezoid(cx, top + 28, bot, 52, 66, PAL_BLACK);
    trapezoid(cx, top + 29, bot, 50, 64, PAL_RAMP(RAMP_WOOD, 3));
    trapezoid(cx, top + 29, top + 31, 50, 52, PAL_RAMP(RAMP_WOOD, 5));
    for (int i = -2; i <= 2; i++) tk_line(cx + i * 9, top + 33, cx + i * 11, bot, PAL_RAMP(RAMP_WOOD, 1));
    hand(cx - 32, top + 36, 0);
    hand(cx + 32, top + 38, 1);
    /* muzzle flash */
    if (fire_age >= 0 && fire_age < 4) {
        int r = fire_age < 2 ? 40 : 22;
        int fy = top - 10;
        for (int i = 0; i < 12; i++) {
            float a = i * 0.5236f + fire_age * 0.7f;
            int rr = (i & 1) ? r : r * 2 / 3;
            tk_tri(cx, fy, cx + (int)(cosf(a) * rr), fy + (int)(sinf(a) * rr * 0.7f), cx + (int)(cosf(a + 0.45f) * rr / 3), fy + (int)(sinf(a + 0.45f) * rr / 4), PAL_RAMP(RAMP_LAVA, 5));
        }
        tk_ellipse(cx, fy, r / 2, r / 3, PAL_RAMP(RAMP_LAVA, 6));
        tk_ellipse(cx, fy, r / 3, r / 4, PAL_RAMP(RAMP_LAVA, 7));
        tk_ellipse(cx, fy, r / 5, r / 6, PAL_WHITE);
    }
    /* reload: two shells pop out of the breech */
    if (reload_age >= 6 && reload_age < 26) {
        int a = reload_age - 6;
        for (int s = 0; s < 2; s++) {
            int sx = cx + 16 + a * (4 + s), sy = top + 12 - a * 5 + a * a / 2 + s * 6;
            tk_rect(sx - 1, sy - 1, 9, 6, PAL_BLACK);
            tk_rect(sx, sy, 5, 4, PAL_UI_RED);
            tk_rect(sx, sy, 5, 1, PAL_PAINT(PAINT_CHERRY, 2));
            tk_rect(sx + 5, sy, 2, 4, PAL_UI_GOLD);
        }
    }
}

/* ---- HUD -------------------------------------------------------------------- */
enum { HUD_CUTE, HUD_BRUTAL, HUD_BOTH };
static void hud(int style, int ammo, int expr, int mouth, int blood, const char *wname, int f) {
    uint8_t fill = PAL_UI_PINK, edge = PAL_UI_PINK_D, txt = PAL_WHITE, sub = PAL_UI_PINK_D;
    if (style == HUD_BRUTAL) { fill = PAL_RAMP(RAMP_BLOOD, 1); edge = PAL_UI_RED; txt = PAL_UI_GOLD; sub = PAL_RAMP(RAMP_LAVA, 5); }
    if (style == HUD_BOTH) { fill = PAL_PAINT(PAINT_GRAPE, 0); edge = PAL_UI_PURPLE; txt = PAL_WHITE; sub = PAL_PAINT(PAINT_MAGENTA, 2); }
    tk_rect(0, VH, TW, TH - VH, PAL_BLACK);
    tk_rect(1, VH + 1, TW - 2, TH - VH - 2, edge);
    tk_rect(2, VH + 2, TW - 4, TH - VH - 4, fill);
    tk_rect(3, VH + 2, TW - 6, 1, style == HUD_CUTE ? PAL_WHITE : edge);
    /* health hearts */
    for (int i = 0; i < 5; i++) {
        int hb = (f / 6 + i) % 10 == 0 ? -1 : 0;
        tk_heart(10 + i * 9, 151 + hb, 1, style == HUD_BRUTAL ? PAL_UI_RED : PAL_PAINT(PAINT_CHERRY, 1));
    }
    tk_text(8, 138, "HEALTH", sub, 1);
    /* ammo */
    char buf[16];
    int n = ammo, len = 0;
    char tmp[16];
    do { tmp[len++] = (char)('0' + n % 10); n /= 10; } while (n && len < 15);
    for (int i = 0; i < len; i++) buf[i] = tmp[len - 1 - i];
    buf[len] = 0;
    tk_text_outline(102 - tk_text_w(buf, 2), 140, buf, txt, PAL_BLACK, 2);
    /* ammo icon */
    if (style == HUD_CUTE) { tk_circle(64, 149, 4, PAL_BLACK); tk_circle(64, 149, 3, PAL_PAINT(PAINT_CARAMEL, 1));
                             tk_circle(61, 145, 1, PAL_PAINT(PAINT_CARAMEL, 1)); tk_circle(67, 145, 1, PAL_PAINT(PAINT_CARAMEL, 1));
                             tk_px(63, 148, PAL_BLACK); tk_px(65, 148, PAL_BLACK); }
    else if (style == HUD_BRUTAL) { tk_rect(61, 142, 7, 13, PAL_BLACK); tk_rect(62, 143, 5, 8, PAL_UI_RED); tk_rect(62, 151, 5, 3, PAL_UI_GOLD); }
    else { for (int i = 0; i < 2; i++) { tk_rect(60 + i * 5, 143, 4, 12, PAL_BLACK); tk_rect(61 + i * 5, 146, 2, 8, PAL_UI_GOLD); tk_rect(61 + i * 5, 144, 2, 2, PAL_RAMP(RAMP_CANDY, 3)); } }
    /* face box */
    tk_rect(106, VH + 2, 28, TH - VH - 4, PAL_BLACK);
    tk_rect(107, VH + 3, 26, TH - VH - 6, style == HUD_BRUTAL ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_RAMP(RAMP_SKY, 5));
    tk_chibi(120, 168, &TK_HERO, 1, expr, mouth, f, 0);
    if (blood) { tk_px(114, 148, PAL_RAMP(RAMP_BLOOD, 4)); tk_px(115, 149, PAL_RAMP(RAMP_BLOOD, 4)); tk_px(124, 145, PAL_RAMP(RAMP_BLOOD, 4)); tk_px(113, 150, PAL_RAMP(RAMP_BLOOD, 3)); }
    tk_rect(106, VH + 2, 28, 1, PAL_BLACK);
    tk_rect(106, TH - 3, 28, 1, PAL_BLACK);
    /* weapon + gore mode */
    tk_text(140, 140, wname, txt, 1);
    const char *mode = style == HUD_CUTE ? "GORE: CUTE" : (style == HUD_BRUTAL ? "GORE: BRUTAL" : "GORE: BOTH?!");
    tk_text(140, 150, mode, sub, 1);
}

/* screen-space blood on the lens (brutal) */
static const uint8_t BAY[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
static void dcircle(int cx, int cy, int r, uint8_t c, int level) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r + r) {
                int px = cx + x, py = cy + y;
                if ((unsigned)px < TW && (unsigned)py < VH && BAY[py & 3][px & 3] < level) tk_fb[py][px] = c;
            }
}
static void splat(int x, int y, int r, uint32_t seed, int age) {
    uint32_t s = tk_hash(seed);
    int grow = age < 2 ? age + 1 : 3;
    r = r * grow / 3;
    if (r < 1) r = 1;
    int ox[6], oy[6], rr[6];
    for (int i = 0; i < 6; i++) { ox[i] = tk_rand(&s, r + 1) - r / 2; oy[i] = tk_rand(&s, r + 1) - r / 2; rr[i] = r / 2 + tk_rand(&s, r / 2 + 1); }
    for (int i = 0; i < 6; i++) dcircle(x + ox[i], y + oy[i], rr[i] + 2, PAL_RAMP(RAMP_BLOOD, 3), 9);
    for (int i = 0; i < 6; i++) dcircle(x + ox[i], y + oy[i], rr[i], PAL_RAMP(RAMP_BLOOD, 4), 16);
    for (int i = 0; i < 9; i++) {
        float an = tk_rand(&s, 628) / 100.0f;
        int d = r + 2 + tk_rand(&s, r + 4);
        dcircle(x + (int)(cosf(an) * d), y + (int)(sinf(an) * d), tk_rand(&s, 2), PAL_RAMP(RAMP_BLOOD, 4), 16);
    }
    tk_px(x - r / 3, y - r / 3, PAL_RAMP(RAMP_BLOOD, 7));
    tk_px(x - r / 3 + 1, y - r / 3, PAL_RAMP(RAMP_BLOOD, 6));
    /* drips crawl down the lens */
    for (int k = 0; k < 2; k++) {
        int dx = x + ox[k] - 1 + k * 2, len = age > 3 ? (age - 3) * (1 + k + (int)(seed % 2)) / 2 : 0;
        if (len > 50) len = 50;
        for (int j = 0; j < len; j++) tk_px(dx + ((j / 9) & 1), y + oy[k] + rr[k] + j, PAL_RAMP(RAMP_BLOOD, 4));
        if (len > 0) { tk_rect(dx - 1 + ((len / 9) & 1), y + oy[k] + rr[k] + len, 3, 2, PAL_RAMP(RAMP_BLOOD, 4)); }
    }
}
static void lens_blood(int age) {
    static const signed char B[][3] = { { 22, 22, 10 }, { 206, 28, 12 }, { 44, 96, 6 }, { 226, 82, 6 }, { 170, 10, 5 }, { 8, 60, 5 } };
    for (int i = 0; i < 6; i++) splat(B[i][0], B[i][1], B[i][2], 900u + (uint32_t)i * 7u, age - (i > 2 ? 1 : 0));
}

/* the back-view hero for the third-person shot (hair honey, outfit mint) */
static const char *const HERO_BACK[] = {
    "......kkkkkkkk......",
    "....kkzzzyyyyykk....",
    "...kzzyyyyyyyyyyk...",
    "..kzyyyyyyyyyyyyyk..",
    ".kzyyyyyyyyyyyyyyyk.",
    ".kyyyyyyyyyyyyyyyyk.",
    "kyyyyyyyyyyyyyyyyyyk",
    "kyyyyyyyyyyyyyyyyyyk",
    "kyyyyyyyyyyyyyyyyyyk",
    "kyyyyyyyyyyyyyyyyyyk",
    "kyyyyyyyyyyyyyyyyyyk",
    "ksyyyyyyyyyyyyyyyysk",
    "kSYyyyyyyyyyyyyyyYSk",
    ".kYYYyyyyyyyyyyYYYk.",
    "..kkYYYYYYYYYYYYkk..",
    "....kkkSSSSSSkkk....",
    "..kkmmmmmmmmmmmmkk..",
    ".kmmnmmmmmmmmmmmmMk.",
    "kmmmnmmmmmmmmmmmmmMk",
    "kmmknmmmmmmmmmmmkmMk",
    "kmmknmmmmmmmmmmmkmMk",
    "kssknmmmmmmmmmmmkssk",
    ".kk.kmmmmmmmmmmk.kk.",
    "....kMMMMMMMMMMk....",
    "....kMMMk..kMMMk....",
    "....kMMMk..kMMMk....",
    "...kEEEEk..kEEEEk...",
    "...kkkkk....kkkkk...",
    NULL };

static void minigun_tp(int hx, int hy, int f, int firing) {
    /* from the hero's right hip toward the vanishing point */
    int mx = 150, my = 76;
    int jit = firing ? ((f & 1) ? 1 : -1) : 0;
    hx += jit; hy += jit;
    /* barrel cluster as a tapering tube */
    for (int i = 0; i <= 20; i++) {
        float k = i / 20.0f;
        int x = (int)(hx + (mx - hx) * k), y = (int)(hy + (my - hy) * k);
        int r = (int)(13 - k * 8);
        tk_circle(x, y, r + 1, PAL_BLACK);
    }
    for (int i = 0; i <= 20; i++) {
        float k = i / 20.0f;
        int x = (int)(hx + (mx - hx) * k), y = (int)(hy + (my - hy) * k);
        int r = (int)(13 - k * 8);
        tk_circle(x, y, r, PAL_RAMP(RAMP_STEEL, 2));
        /* spinning barrel stripes */
        float a = (firing ? f * 1.3f : f * 0.2f);
        for (int b = 0; b < 3; b++) {
            float aa = a + b * 2.094f;
            tk_px(x + (int)(cosf(aa) * r * 0.7f), y + (int)(sinf(aa) * r * 0.7f), PAL_RAMP(RAMP_STEEL, 5));
        }
        if ((i % 7) == 3) tk_ring(x, y, r, PAL_RAMP(RAMP_STEEL, 1));
    }
    /* muzzle */
    tk_circle(mx, my, 5, PAL_RAMP(RAMP_STEEL, 4));
    for (int b = 0; b < 6; b++) {
        float aa = (firing ? f * 1.3f : f * 0.2f) + b * 1.047f;
        tk_px(mx + (int)(cosf(aa) * 3), my + (int)(sinf(aa) * 3), PAL_BLACK);
    }
    /* body box on the hip */
    tk_rect(hx - 14, hy - 10, 26, 22, PAL_BLACK);
    tk_rect(hx - 13, hy - 9, 24, 20, PAL_RAMP(RAMP_STEEL, 3));
    tk_rect(hx - 13, hy - 9, 24, 3, PAL_RAMP(RAMP_STEEL, 5));
    tk_heart(hx - 2, hy + 2, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    /* ammo belt */
    for (int i = 0; i < 6; i++) tk_rect(hx - 20 - i * 2, hy + 8 + i * 3, 4, 3, (i & 1) ? PAL_UI_GOLD : PAL_RAMP(RAMP_CANDY, 3));
    if (firing && (f & 1)) {
        for (int i = 0; i < 8; i++) {
            float a = i * 0.785f + f;
            int rr = (i & 1) ? 14 : 7;
            tk_tri(mx, my, mx + (int)(cosf(a) * rr), my + (int)(sinf(a) * rr), mx + (int)(cosf(a + 0.4f) * 4), my + (int)(sinf(a + 0.4f) * 4), PAL_RAMP(RAMP_LAVA, 6));
        }
        tk_circle(mx, my, 4, PAL_WHITE);
    }
    /* spent casings flying out to the right */
    if (firing)
        for (int i = 0; i < 6; i++) {
            int a = (f * 3 + i * 5) % 18;
            int x = hx + 12 + a * 3, y = hy - 6 - a * 2 + a * a / 4;
            tk_rect(x, y, 2, 1 + ((a + i) & 1), PAL_UI_GOLD);
        }
}

/* ---- puff demon (screen space, scaled) -------------------------------------- */
static void puff_demon(int x, int ybot, float sc, float d) {
    int r = (int)(7 * sc + 0.5f);
    if (r < 2) r = 2;
    int y = ybot - r;
    tk_tri(x - r + 1, y - r / 2, x - r / 2, y - r + 1, x - r - 1, y - r - 2 - r / 3, PAL_BLACK);
    tk_tri(x + r - 1, y - r / 2, x + r / 2, y - r + 1, x + r + 1, y - r - 2 - r / 3, PAL_BLACK);
    tk_tri(x - r + 2, y - r / 2, x - r / 2, y - r + 2, x - r, y - r - r / 3, lit(PAL_RAMP(RAMP_BONE, 6), d));
    tk_tri(x + r - 2, y - r / 2, x + r / 2, y - r + 2, x + r, y - r - r / 3, lit(PAL_RAMP(RAMP_BONE, 6), d));
    tk_circle(x, y, r + 1, PAL_RAMP(RAMP_VAPE, 1));
    tk_circle(x, y, r, lit(PAL_RAMP(RAMP_VAPE, 4), d));
    tk_circle(x - r / 4, y - r / 4, r * 2 / 3, lit(PAL_RAMP(RAMP_VAPE, 5), d));
    tk_circle(x - r / 2, y - r / 2, r / 4, lit(PAL_RAMP(RAMP_VAPE, 6), d));
    int e = r / 3 > 0 ? r / 3 : 1;
    tk_rect(x - e * 2, y - e, e, e, PAL_UI_RED); tk_rect(x + e, y - e, e, e, PAL_UI_RED);
    tk_line(x - e * 2 - 1, y - e - 2, x - e + 1, y - e - 1, PAL_BLACK);
    tk_line(x + e * 2 + 1, y - e - 2, x + e - 1, y - e - 1, PAL_BLACK);
    tk_rect(x - e * 2, y + e / 2 + 1, e * 4 + 1, e > 1 ? e : 1, PAL_BLACK);
}

/* ======================================================================== */
/* the scene                                                                */
/* ======================================================================== */
static void caption_pop(int age, int cy, const char *s, uint8_t c, uint8_t oc, int sc) {
    if (age < 0) return;
    int o = age < 3 ? (3 - age) * 3 : 0;
    tk_caption(cy - o, s, c, oc, sc);
}

static void first_person(int f) {
    int brutal = f >= F_SWAP + 6;
    /* camera: gentle sway, kick on the shotgun */
    float kick = 0;
    if (f >= F_BRUTAL_FIRE && f < F_BRUTAL_FIRE + 6) kick = (6 - (f - F_BRUTAL_FIRE)) * 0.006f;
    set_camera(1.45f, 3.5f, 0.55f, sinf(f * 0.05f) * 0.03f, 64.0f - kick * 300);
    g_light_boost = 0;
    if (f >= F_BRUTAL_FIRE && f < F_BRUTAL_FIRE + 3) g_light_boost = 6 - (f - F_BRUTAL_FIRE) * 2;
    if (f >= F_CUTE_FIRE && f < F_CUTE_FIRE + 2) g_light_boost = 2;
    render_view();

    /* the impling */
    Cap imp;
    int alive1 = f < F_CUTE_HIT, alive2 = f >= F_RESPAWN && f < F_BRUTAL_FIRE + 1;
    float ix = 0, iy = imp_y(f), iz = 0;
    if (alive1) ix = imp_x(f, 0, 0.27f);
    if (alive2) ix = imp_x(f, F_RESPAWN + 4, 0.40f);
    if (alive2 && f < F_RESPAWN + 4) ix = 13.4f;
    float isx = 0, isy = 0, isc = 0;
    if (alive1 || alive2) {
        iz = floor_at(ix, iy) + fabsf(sinf(f * 0.5f)) * 0.06f;
        cap_begin();
        int scream = (alive2 && f > F_BRUTAL_FIRE - 6) || (alive1 && f > F_CUTE_HIT - 5);
        tk_chibi(CAP_X, CAP_Y, &TK_IMP, 2, scream ? TKE_SHOCK : TKE_ANGRY, scream ? TKM_SCREAM : TKM_FANGS, f * 2, (f / 4) & 1);
        cap_end(&imp);
        cap_world(&imp, ix, iy, iz, 0.82f, &isx, &isy, &isc);
    }
    /* respawn poof */
    if (f >= F_RESPAWN - 2 && f < F_RESPAWN + 12) {
        float sx, sy, d;
        if (project(13.4f, 3.5f, 0.6f, &sx, &sy, &d)) {
            int a = f - F_RESPAWN + 2;
            for (int i = 0; i < 6; i++) {
                float an = i * 1.047f;
                int r = 4 + a / 2 - (a > 8 ? (a - 8) : 0);
                if (r > 0) tk_circle((int)sx + (int)(cosf(an) * a * 1.5f), (int)sy + (int)(sinf(an) * a), r, PAL_RAMP(RAMP_VAPE, 4 + (i & 1)));
            }
        }
    }

    /* CUTE: the teddy flies, the imp pops */
    if (f >= F_CUTE_FIRE && f < F_CUTE_HIT) {
        float k = (f - F_CUTE_FIRE) / (float)(F_CUTE_HIT - F_CUTE_FIRE);
        float hx = imp_x(F_CUTE_HIT, 0, 0.27f);
        float tx = 2.4f + (hx - 2.4f) * k, ty = 3.55f + (imp_y(F_CUTE_HIT) - 3.55f) * k;
        float tz = 0.36f + 0.14f * k + sinf(k * 3.14159f) * 0.12f;
        float sx, sy, d;
        if (project(tx, ty, tz, &sx, &sy, &d)) {
            int r = (int)(0.09f * projY / d);
            if (r < 2) r = 2;
            teddy_head((int)sx, (int)sy, r, 1);
            /* little arms flailing */
            int fl = (f & 1) ? 1 : -1;
            tk_line((int)sx - r, (int)sy + r / 2, (int)sx - r - r / 2, (int)sy + r / 2 * fl, PAL_PAINT(PAINT_CARAMEL, 0));
            tk_line((int)sx + r, (int)sy + r / 2, (int)sx + r + r / 2, (int)sy - r / 2 * fl, PAL_PAINT(PAINT_CARAMEL, 0));
        }
    }
    if (f >= F_CUTE_HIT && f < F_SWAP + 10) {
        float hx = imp_x(F_CUTE_HIT, 0, 0.27f), hy = imp_y(F_CUTE_HIT);
        float sx, sy, d;
        if (project(hx, hy, floor_at(hx, hy) + 0.35f, &sx, &sy, &d)) {
            int a = f - F_CUTE_HIT;
            if (a < 4) { tk_circle((int)sx, (int)sy, 6 + a * 5, PAL_WHITE); tk_circle((int)sx, (int)sy, 3 + a * 4, PAL_PAINT(PAINT_BUBBLEGUM, 2)); }
            tk_confetti((int)sx, (int)sy, a, 11);
            tk_confetti((int)sx, (int)sy - 4, a, 12);
            tk_hearts((int)sx, (int)sy, a, 13);
            /* plush stuffing tufts */
            for (int i = 0; i < 7; i++) {
                float an = i * 0.9f + 0.3f;
                int px = (int)sx + (int)(cosf(an) * a * 2.2f), py = (int)sy + (int)(sinf(an) * a * 1.4f) - a + a * a / 22;
                if (a < 40) { tk_circle(px, py, 2, PAL_WHITE); tk_px(px - 1, py - 1, PAL_GRAY_L); }
            }
        }
    }
    /* the teddy plops onto the floor and sits there, content... until later */
    if (f >= F_CUTE_HIT) {
        int a = f - F_CUTE_HIT;
        float hx = imp_x(F_CUTE_HIT, 0, 0.27f), hy = imp_y(F_CUTE_HIT);
        float bx = hx - 0.2f - (a < 12 ? a : 12) * 0.03f, by2 = hy + 0.35f + (a < 12 ? a : 12) * 0.02f;
        float bz = floor_at(bx, by2) + 0.1f + (a < 14 ? fabsf(sinf(a * 0.45f)) * 0.35f * (14 - a) / 14.0f : 0);
        float tsx, tsy, td;
        int mood = f >= F_BRUTAL_FIRE + 2 ? 2 : 0;
        int hop = (f >= F_BRUTAL_FIRE + 2 && f < F_BRUTAL_FIRE + 6) ? 3 : 0;   /* flinch */
        if (project(bx, by2, bz, &tsx, &tsy, &td)) teddy_head((int)tsx, (int)tsy - hop, (int)(0.1f * projY / td), mood);
    }
    /* BRUTAL: gibs */
    if (f >= F_BRUTAL_FIRE) {
        float hx = imp_x(F_BRUTAL_FIRE, F_RESPAWN + 4, 0.40f), hy = imp_y(F_BRUTAL_FIRE);
        float sx, sy, d;
        if (project(hx, hy, floor_at(hx, hy) + 0.38f, &sx, &sy, &d)) {
            int a = f - F_BRUTAL_FIRE;
            if (a < 4) {
                int R = 14 + a * 7;
                for (int i = 0; i < 11; i++) {
                    float an = i * 0.571f + a * 0.3f;
                    int rr = (i & 1) ? R : R / 2;
                    tk_tri((int)sx, (int)sy, (int)sx + (int)(cosf(an) * rr), (int)sy + (int)(sinf(an) * rr),
                           (int)sx + (int)(cosf(an + 0.35f) * rr / 3), (int)sy + (int)(sinf(an + 0.35f) * rr / 3), a < 2 ? PAL_RAMP(RAMP_BLOOD, 5) : PAL_RAMP(RAMP_BLOOD, 3));
                }
                if (a < 2) tk_circle((int)sx, (int)sy, 6 - a * 2, PAL_RAMP(RAMP_LAVA, 7));
            }
            tk_gibs((int)sx, (int)sy, a, 21);
            tk_gibs((int)sx, (int)sy + 4, a, 22);
            /* what's left on the floor */
            if (a > 10) for (int i = 0; i < 9; i++) {
                uint32_t s = tk_hash(i + 400);
                float gx = hx - 0.5f + tk_rand(&s, 100) / 100.0f, gy = hy - 0.5f + tk_rand(&s, 100) / 100.0f;
                float px, py, pd;
                if (project(gx, gy, floor_at(gx, gy), &px, &py, &pd)) {
                    int r = (int)(0.05f * projY / pd);
                    tk_ellipse((int)px, (int)py, r + 1, r / 2 + 1, (i % 3) ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_RAMP(RAMP_FLESH, 3));
                }
            }
            /* the imp's little horns land last. clink. */
            if (a > 18) {
                float px, py, pd;
                if (project(hx - 0.3f, hy + 0.1f, floor_at(hx, hy), &px, &py, &pd)) {
                    tk_tri((int)px - 4, (int)py, (int)px - 1, (int)py, (int)px - 3, (int)py - 5, PAL_PAINT(PAINT_CHERRY, 0));
                    tk_tri((int)px + 2, (int)py, (int)px + 5, (int)py, (int)px + 4, (int)py - 5, PAL_PAINT(PAINT_CHERRY, 0));
                }
            }
        }
    }

    /* the weapon (bobbing) */
    float bobt = f * 0.18f;
    int bx = (int)(sinf(bobt) * 4), by = (int)(fabsf(cosf(bobt)) * 4);
    if (f < F_SWAP) {
        plush_launcher_fp(bx, by, f >= F_CUTE_FIRE ? f - F_CUTE_FIRE : -1, f < F_CUTE_FIRE || f > F_CUTE_FIRE + 22);
    } else if (f < F_SWAP + 6) {
        int a = f - F_SWAP;
        plush_launcher_fp(bx, by + a * a * 3, -1, 1);
    } else {
        int a = f - F_SWAP - 6;
        int up = a < 8 ? (8 - a) * (8 - a) : 0;
        shotgun_fp(bx, by + up, f >= F_BRUTAL_FIRE ? f - F_BRUTAL_FIRE : -1, f >= F_BRUTAL_FIRE + 14 ? f - F_BRUTAL_FIRE - 14 : -1);
    }
    if (brutal && f >= F_BRUTAL_FIRE) lens_blood(f - F_BRUTAL_FIRE);

    /* Father Fuzz disapproves (peeks over the HUD) */
    if (f >= F_JUDGE2 + 4) {
        int a = f - F_JUDGE2 - 4;
        int rise = (int)(tk_bounce(a / 10.0f) * 40);
        int shake = (a / 4) & 1;
        int fyf = VH + 46 - rise;
        tk_chibi(40, fyf, &TK_FUZZ, 2, TKE_SMUG, TKM_FLAT, 0, shake);
        tk_rect(40 - 5, fyf - 25, 10, 3, PAL_BLACK);        /* clerical collar */
        tk_rect(40 - 2, fyf - 24, 4, 2, PAL_WHITE);
        tk_rect(40 + 8, fyf - 18, 2, 8, PAL_UI_GOLD);       /* a little plush cross */
        tk_rect(40 + 6, fyf - 16, 6, 2, PAL_UI_GOLD);
        if (a >= 8) {
            tk_window(58, VH - rise - 22, 50, 15, TKW_CREAM);
            tk_text(63, VH - rise - 18, (a / 6) & 1 ? "tsk." : "tsk tsk", PAL_GRAY_D, 1);
        }
    }

    /* HUD */
    int style = brutal ? HUD_BRUTAL : HUD_CUTE;
    int ammo = brutal ? (f >= F_BRUTAL_FIRE ? 6 : 8) : (f >= F_CUTE_FIRE ? 11 : 12);
    int expr = TKE_NORMAL, mouth = TKM_SMILE, blood = 0;
    if (!brutal) {
        if (f >= F_CUTE_FIRE) { expr = TKE_HAPPY; mouth = TKM_OPEN; }
        if (f >= F_CUTE_HIT) { expr = TKE_LOVE; mouth = TKM_CAT; }
    } else {
        expr = TKE_ANGRY; mouth = TKM_FANGS;
        if (f >= F_BRUTAL_FIRE) { expr = TKE_ANGRY; mouth = TKM_SCREAM; }
        if (f >= F_BRUTAL_FIRE + 8) { expr = TKE_SHOCK; mouth = TKM_FLAT; blood = 1; }
        if (f >= F_JUDGE2) { expr = TKE_SAD; mouth = TKM_FLAT; }
    }
    hud(style, ammo, expr, mouth, blood, brutal ? "SUPER SHOTGUN" : "PLUSH LAUNCHER", f);

    /* captions */
    if (f < F_SWAP) caption_pop(f - 3, 18, "CUTE MODE", PAL_WHITE, PAL_UI_PINK_D, 3);
    if (f >= F_SWAP + 8 && f < F_JUDGE1 - 2) caption_pop(f - F_SWAP - 8, 18, "BRUTAL MODE", PAL_UI_RED, PAL_BLACK, 3);
    if (f >= F_JUDGE1 && f < F_JUDGE2 - 4) caption_pop(f - F_JUDGE1, 60, "(We don't judge.)", PAL_WHITE, PAL_BLACK, 2);
    if (f >= F_JUDGE2) caption_pop(f - F_JUDGE2, 52, "(We judge\na little.)", PAL_WHITE, PAL_BLACK, 2);

    /* post */
    if (f >= F_BRUTAL_FIRE && f < F_BRUTAL_FIRE + 8) {
        int m = 5 - (f - F_BRUTAL_FIRE) * 2 / 3;
        tk_post_shake((f & 1) ? m : -m, (f & 2) ? m : -m / 2);
    }
    if (f == F_BRUTAL_FIRE) tk_post_flash(PAL_RAMP(RAMP_LAVA, 7), 6);
    if (f == F_CUTE_HIT) tk_post_flash(PAL_WHITE, 4);
}

static void third_person(int f) {
    int a = f - F_TP;
    set_camera(0.9f + a * 0.004f, 3.75f, 0.78f, -0.06f, 58.0f);
    int firing = f >= F_BRRT && f < 250;
    g_light_boost = firing && (f & 1) ? 3 : 0;
    render_view();
    /* demons charging; each gets mowed down in turn */
    Cap imp[2];
    for (int k = 0; k < 2; k++) {
        cap_begin();
        tk_chibi(CAP_X, CAP_Y, &TK_IMP, 2, TKE_ANGRY, TKM_FANGS, k * 4, k);
        cap_end(&imp[k]);
    }
    static const struct { float x, y; int type, hit; } D[] = {
        { 7.0f, 3.1f, 0, 207 }, { 8.5f, 3.9f, 1, 213 }, { 10.0f, 3.3f, 0, 219 }, { 9.0f, 2.6f, 1, 225 },
        { 12.0f, 3.7f, 0, 231 }, { 11.0f, 4.3f, 1, 237 }, { 13.0f, 2.8f, 0, 243 },
    };
    int nd = (int)(sizeof D / sizeof D[0]);
    float msx = 150, msy = 76;   /* minigun muzzle on screen */
    for (int i = nd - 1; i >= 0; i--) {
        float dx = D[i].x - (f - F_TP) * 0.05f, dy = D[i].y;
        float dz = floor_at(dx, dy);
        int hit = D[i].hit;
        float sx, sy, d;
        if (!project(dx, dy, dz + 0.35f, &sx, &sy, &d)) continue;
        if (f < hit) {
            if (D[i].type == 0) cap_world(&imp[(f / 3 + i) & 1], dx, dy, dz + fabsf(sinf(f * 0.5f + i)) * 0.05f, 0.82f, NULL, NULL, NULL);
            else {
                float bsx, bsy, bd;
                if (project(dx, dy, dz + fabsf(sinf(f * 0.4f + i)) * 0.08f, &bsx, &bsy, &bd))
                    puff_demon((int)bsx, (int)bsy, 0.3f * projY / bd / 7.0f, bd * 0.8f);
            }
            /* tracers to the next victim */
            if (firing && f >= hit - 6 && (f & 1)) {
                tk_line((int)msx, (int)msy, (int)sx + ((f & 2) ? 2 : -2), (int)sy, PAL_RAMP(RAMP_LAVA, 7));
                tk_line((int)msx + 1, (int)msy, (int)sx + ((f & 2) ? 3 : -1), (int)sy + 1, PAL_RAMP(RAMP_LAVA, 5));
            }
        } else {
            int b = f - hit;
            if (b < 3) tk_circle((int)sx, (int)sy, 4 + b * 4, PAL_WHITE);
            tk_confetti((int)sx, (int)sy, b, 100 + i);   /* GORE: BOTH */
            tk_gibs((int)sx, (int)sy, b, 200 + i);
            tk_hearts((int)sx, (int)sy, b, 300 + i);
        }
    }
    /* the hero, from behind */
    static uint8_t map[128];
    if (!map['k']) {
        map['k'] = PAL_BLACK; map['y'] = PAL_PAINT(PAINT_HONEY, 1); map['Y'] = PAL_PAINT(PAINT_HONEY, 0); map['z'] = PAL_PAINT(PAINT_HONEY, 2);
        map['s'] = PAL_SKIN(SKIN_PEACH, 1); map['S'] = PAL_SKIN(SKIN_PEACH, 0);
        map['m'] = PAL_PAINT(PAINT_MINT, 1); map['M'] = PAL_PAINT(PAINT_MINT, 0); map['n'] = PAL_PAINT(PAINT_MINT, 2);
        map['E'] = PAL_GRAY_D;
    }
    int jit = firing ? ((f & 2) ? 1 : 0) : 0;
    int hx = 26 + jit, hy = VH - 84 + 4 + (firing ? (f & 1) : 0);
    tk_sprite(HERO_BACK, hx, hy, map, 3, 0);
    /* hair strands + a cowlick */
    for (int i = 0; i < 4; i++) tk_line(hx + 15 + i * 9, hy + 12 + (i & 1) * 3, hx + 12 + i * 10, hy + 30 + (i & 1) * 2, PAL_PAINT(PAINT_HONEY, 0));
    tk_rect(hx + 30, hy - 3, 3, 4, PAL_BLACK); tk_rect(hx + 33, hy - 6, 3, 4, PAL_BLACK);
    tk_px(hx + 31, hy - 2, PAL_PAINT(PAINT_HONEY, 1)); tk_px(hx + 34, hy - 5, PAL_PAINT(PAINT_HONEY, 1));
    tk_rect(hx + 12, hy + 6, 6, 2, PAL_PAINT(PAINT_HONEY, 2));
    /* the Doomogachi, clipped to the belt, swinging */
    {
        int kx = hx + 20 + (int)(sinf(f * 0.6f) * 2), ky = hy + 70;
        tk_line(hx + 20, hy + 66, kx, ky - 4, PAL_GRAY_L);
        tk_ellipse(kx, ky, 4, 5, PAL_BLACK);
        tk_ellipse(kx, ky, 3, 4, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        tk_rect(kx - 2, ky - 2, 5, 4, PAL_UI_LCD);
        tk_px(kx - 1, ky - 1, PAL_BLACK); tk_px(kx + 1, ky - 1, PAL_BLACK);
    }
    minigun_tp(hx + 66, hy + 58, f, firing);
    /* right hand on the gun */
    tk_rect(hx + 52, hy + 58, 10, 9, PAL_BLACK);
    tk_rect(hx + 53, hy + 59, 8, 7, PAL_SKIN(SKIN_PEACH, 1));
    /* HUD: ammo pouring away */
    int ammo = 999 - (firing ? (f - F_BRRT) * 17 : 0);
    if (f >= 250) ammo = 999 - (250 - F_BRRT) * 17;
    hud(HUD_BOTH, ammo, firing ? TKE_SPARKLE : TKE_HAPPY, firing ? TKM_SCREAM : TKM_OPEN, 0, "MINIGUN", f);
    caption_pop(f - F_TP - 4, 18, "...or third\nperson!", PAL_WHITE, PAL_UI_PURPLE, 2);
    if (firing) tk_post_shake((f & 1) ? 1 : -1, (f & 2) ? 1 : 0);
}

void scene_s10_shooter(int f, int n) {
    (void)n;
    build_shade();
    g_f = f;
    if (f < F_TP) first_person(f);
    else third_person(f);
    if (f == F_TP) tk_post_flash(PAL_WHITE, 10);
}

void scene_s10_shooter_audio(double t0, double dur) {
#define AT(fr) (t0 + (fr) / 30.0)
    (void)dur;
    /* imp footsteps + screech */
    for (int fr = 0; fr < F_CUTE_HIT; fr += 6) tk_note(AT(fr), 0.04, 40, TKV_TRI, 0.25f);
    tk_slide(AT(4), 0.25, 79, 91, TKV_SQ12, 0.18f);
    /* CUTE: plush launcher - boing! squeak! pop + confetti fanfare */
    tk_sfx(AT(F_CUTE_FIRE), SFX_BOING, 0.6f);
    tk_sfx(AT(F_CUTE_FIRE), SFX_SQUEAK, 0.4f);
    tk_sfx(AT(F_CUTE_HIT), SFX_POP, 1.0f);
    tk_sfx(AT(F_CUTE_HIT), SFX_SQUEAK, 0.6f);
    tk_sfx(AT(F_CUTE_HIT + 1), SFX_SPARKLE, 0.6f);
    tk_note(AT(F_CUTE_HIT + 3), 0.1, 84, TKV_SQ25, 0.25f);
    tk_note(AT(F_CUTE_HIT + 6), 0.1, 88, TKV_SQ25, 0.25f);
    tk_note(AT(F_CUTE_HIT + 9), 0.25, 91, TKV_SQ25, 0.25f);
    tk_sfx(AT(F_CUTE_HIT + 10), SFX_BOING, 0.2f);   /* the teddy bounces */
    tk_sfx(AT(3), SFX_SELECT, 0.4f);                /* caption */
    /* swap */
    tk_sfx(AT(F_SWAP), SFX_WHOOSH, 0.5f);
    tk_note(AT(F_SWAP + 12), 0.05, 50, TKV_NOISE, 0.6f);   /* chk */
    tk_note(AT(F_SWAP + 15), 0.05, 46, TKV_NOISE, 0.6f);   /* chk */
    tk_sfx(AT(F_SWAP + 8), SFX_THUD, 0.8f);         /* BRUTAL MODE slam */
    tk_sfx(AT(F_RESPAWN), SFX_POP, 0.5f);           /* poof: respawn */
    tk_slide(AT(F_RESPAWN + 6), 0.3, 74, 86, TKV_SQ12, 0.18f);
    for (int fr = F_RESPAWN + 6; fr < F_BRUTAL_FIRE; fr += 5) tk_note(AT(fr), 0.04, 40, TKV_TRI, 0.25f);
    /* BOOM */
    tk_sfx(AT(F_BRUTAL_FIRE), SFX_SHOTGUN, 1.0f);
    tk_sfx(AT(F_BRUTAL_FIRE), SFX_EXPLOSION, 0.4f);
    tk_sfx(AT(F_BRUTAL_FIRE + 1), SFX_SPLAT, 0.9f);
    tk_sfx(AT(F_BRUTAL_FIRE + 4), SFX_SPLAT, 0.5f);   /* on the lens */
    tk_note(AT(F_BRUTAL_FIRE + 20), 0.08, 100, TKV_SINE, 0.2f);  /* the horns land: clink */
    /* judgement: silence, then a disapproving kazoo + tsk tsk */
    tk_note(AT(F_JUDGE1), 0.08, 72, TKV_SINE, 0.2f);
    tk_sfx(AT(F_JUDGE2), SFX_KAZOO, 0.6f);
    tk_note(AT(F_JUDGE2 + 14), 0.03, 110, TKV_NOISE, 0.35f);
    tk_note(AT(F_JUDGE2 + 19), 0.03, 110, TKV_NOISE, 0.35f);
    tk_note(AT(F_JUDGE2 + 26), 0.03, 110, TKV_NOISE, 0.35f);
    tk_note(AT(F_JUDGE2 + 31), 0.03, 110, TKV_NOISE, 0.35f);
    /* third person: whoosh, spin-up, BRRRRT */
    tk_sfx(AT(F_TP), SFX_WHOOSH, 0.7f);
    tk_slide(AT(F_TP), (F_BRRT - F_TP) / 30.0, 40, 70, TKV_SQ25, 0.25f);
    for (int fr = F_BRRT; fr < 250; fr++) {
        tk_note(AT(fr), 0.03, 30, TKV_NOISE, 0.4f);
        if (fr % 2 == 0) tk_note(AT(fr), 0.025, 38, TKV_SQ50, 0.2f);
    }
    tk_slide(AT(250), 0.3, 70, 40, TKV_SQ25, 0.2f);
    static const int hits[] = { 207, 213, 219, 225, 231, 237, 243 };
    for (int i = 0; i < 7; i++) { tk_sfx(AT(hits[i]), SFX_POP, 0.6f); tk_sfx(AT(hits[i]), SFX_SPLAT, 0.35f); }
#undef AT
}
