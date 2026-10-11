/* s06_life.c - life in Hillvile Towers (a Tomodachi-style apartment cut-away).
 * The camera tilts down the building, floor by floor:
 *   top:    two friends high-five... then a full grawlix argument and a
 *           table flip ("Make friends!"); next door someone sleeps through
 *           it while their Doomogachi goes "beep."
 *   middle: two citizens fall in love (heart burst) ("Fall in love!");
 *           Nana rates her own cupcake 10/10 ("Eat cupcakes!")
 *   bottom: a dance party, and a guy in a dark room who has not moved,
 *           blinked or looked away once. ("Ignore that one.")
 *           (He is already visible, staring, at the bottom of the
 *           previous shot.) At the very end he waves. */
#include "tk.h"
#include <math.h>
#include <string.h>

#define BX0 8
#define BX1 232
#define ROOF_H 40
#define FLOOR_H 76
#define ROOM_H 70
#define LX 14      /* left room x */
#define RXR 123    /* right room x */
#define ROOM_W 103

static int camy;
static int floor_top(int fl) { return ROOF_H + 2 + (2 - fl) * FLOOR_H - camy; } /* fl 2 = top */

/* ---- a chibi that waves (raised right arm) ------------------------------- */
static void chibi_wave(int x, int yf, const TkChibi *c, int s, int expr, int mouth,
                       int bob, int flip, int phase) {
    int b = (bob / 4) & 1;
#define CX(lx) (flip ? x + (8 - (lx)) * s : x + ((lx) - 9) * s)
#define CY(ly) (yf + ((ly) - 27) * s)
    int rx = CX(15) < CX(17) ? CX(15) : CX(17), ry = CY(15 + b);
    int rw = 3 * s, rh = 8 * s;
    static uint8_t save[64][64];
    for (int j = 0; j < rh && j < 64; j++)
        for (int i = 0; i < rw && i < 64; i++) save[j][i] = tk_get(rx + i, ry + j);
    tk_chibi(x, yf, c, s, expr, mouth, bob, flip);
    for (int j = 0; j < rh && j < 64; j++)
        for (int i = 0; i < rw && i < 64; i++)
            if ((unsigned)(rx + i) < TW && (unsigned)(ry + j) < TH) tk_fb[ry + j][rx + i] = save[j][i];
    tk_rect(CX(15), CY(15 + b), s, 8 * s, PAL_BLACK);
    static const int A0[][2] = { {15, 16}, {16, 15}, {17, 14}, {18, 13} };
    static const int H0[][2] = { {19, 11}, {20, 11}, {19, 12}, {20, 12} };
    static const int A1[][2] = { {15, 16}, {16, 15}, {16, 14}, {17, 13} };
    static const int H1[][2] = { {17, 10}, {18, 10}, {17, 11}, {18, 11} };
    const int (*A)[2] = phase ? A1 : A0, (*H)[2] = phase ? H1 : H0;
    for (int i = 0; i < 4; i++) {
        tk_rect(CX(A[i][0]) - s, CY(A[i][1] + b) - s, 3 * s, 3 * s, PAL_BLACK);
        tk_rect(CX(H[i][0]) - s, CY(H[i][1] + b) - s, 3 * s, 3 * s, PAL_BLACK);
    }
    for (int i = 0; i < 4; i++) {
        tk_rect(CX(A[i][0]), CY(A[i][1] + b), s, s, c->outfit);
        tk_rect(CX(H[i][0]), CY(H[i][1] + b), s, s, c->skin);
    }
#undef CX
#undef CY
}

/* ---- per-room clipping: save the frame, draw, restore outside the room ---- */
static uint8_t clip_save[TH][TW];
static void clip_begin(void) { memcpy(clip_save, tk_fb, sizeof clip_save); }
static void clip_end(int x, int y, int w, int h) {
    for (int j = 0; j < TH; j++)
        for (int i = 0; i < TW; i++)
            if (i < x || i >= x + w || j < y || j >= y + h) tk_fb[j][i] = clip_save[j][i];
}

/* ---- room pieces ---------------------------------------------------------- */
static void wallpaper(int x, int y, int w, int h, uint8_t a, uint8_t b, int pat) {
    tk_rect(x, y, w, h, a);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int on = 0;
            if (pat == 0) on = (i % 8) < 3;                               /* stripes */
            else if (pat == 1) on = ((i + (j / 8) * 4) % 8 == 0) && (j % 8 == 2); /* dots */
            else on = ((i / 6 + j / 6) & 1) && ((i + j) & 1);             /* soft checks */
            if (on) tk_px(x + i, y + j, b);
        }
    /* floorboards + baseboard */
    tk_rect(x, y + h - 8, w, 8, PAL_RAMP(RAMP_WOOD, 5));
    for (int i = 0; i < w; i += 13) tk_rect(x + i, y + h - 8, 1, 8, PAL_RAMP(RAMP_WOOD, 4));
    tk_rect(x, y + h - 8, w, 1, PAL_RAMP(RAMP_WOOD, 3));
    tk_rect(x, y + h - 10, w, 2, PAL_WHITE);
}
static void lamp_glow(int x, int y, int h) {
    tk_rect(x, y, 1, 4, PAL_GRAY_D);
    tk_ellipse(x, y + 6, 5, 3, PAL_PAINT(PAINT_LEMON, 2));
    tk_rect(x - 5, y + 4, 11, 2, PAL_PAINT(PAINT_HONEY, 1));
    for (int j = 0; j < h; j++) tk_dither_rect(x - 6 - j / 3, y + 9 + j, 13 + 2 * (j / 3), 1, PAL_PAINT(PAINT_LEMON, 2), 3);
}
static void night_window(int x, int y, int f) {
    tk_rect(x - 1, y - 1, 22, 18, PAL_WHITE);
    tk_rect(x, y, 20, 16, PAL_RAMP(RAMP_SKY, 1));
    tk_px(x + 4, y + 4, PAL_WHITE); tk_px(x + 15, y + 9, (f / 10) & 1 ? PAL_WHITE : PAL_RAMP(RAMP_SKY, 5));
    tk_circle(x + 13, y + 5, 3, PAL_PAINT(PAINT_LEMON, 2)); tk_circle(x + 14, y + 4, 2, PAL_RAMP(RAMP_SKY, 1));
    tk_rect(x + 9, y, 2, 16, PAL_WHITE); tk_rect(x, y + 7, 20, 2, PAL_WHITE);
    tk_rect(x - 3, y - 3, 7, 21, PAL_PAINT(PAINT_ROSE, 2)); tk_rect(x + 17, y - 3, 7, 21, PAL_PAINT(PAINT_ROSE, 2));
}
static void anger(int x, int y, int t) {
    /* cartoon anger vein */
    uint8_t r = PAL_UI_RED;
    int s = (t / 3) & 1;
    tk_rect(x - 3 - s, y - 1, 2, 2, r); tk_rect(x + 2 + s, y - 1, 2, 2, r);
    tk_rect(x - 1, y - 3 - s, 2, 2, r); tk_rect(x - 1, y + 2 + s, 2, 2, r);
}
static void grawlix(int x, int y, int age, const char *s) {
    int w = tk_text_w(s, 1) + 8, dy = age == 0 ? -3 : 0;
    tk_rect(x, y + dy, w, 13, PAL_BLACK);
    tk_rect(x + 1, y + 1 + dy, w - 2, 11, PAL_WHITE);
    tk_tri(x + w / 2 - 3, y + 12 + dy, x + w / 2 + 3, y + 12 + dy, x + w / 2, y + 17 + dy, PAL_BLACK);
    tk_text(x + 4, y + 3 + dy, s, PAL_UI_RED, 1);
}
static void cupcake(int x, int y, int bites) {
    /* bottom-centre at (x, y) */
    uint8_t ol = PAL_RAMP(RAMP_PASTEL_PINK, 1);
    tk_rect(x - 5, y - 7, 10, 7, ol);
    tk_rect(x - 4, y - 6, 8, 6, PAL_PAINT(PAINT_SKY, 2));
    tk_rect(x - 2, y - 6, 1, 6, PAL_PAINT(PAINT_SKY, 1)); tk_rect(x + 1, y - 6, 1, 6, PAL_PAINT(PAINT_SKY, 1));
    if (bites >= 3) return;
    tk_ellipse(x, y - 9, 6, 3, ol);
    tk_ellipse(x, y - 9, 5, 2, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    if (bites < 2) { tk_ellipse(x, y - 12, 3, 2, ol); tk_ellipse(x, y - 12, 2, 1, PAL_PAINT(PAINT_BUBBLEGUM, 2)); }
    if (bites < 1) { tk_circle(x, y - 15, 2, ol); tk_rect(x - 1, y - 16, 2, 2, PAL_PAINT(PAINT_CHERRY, 1)); }
    if (bites >= 1) tk_rect(x + 2, y - 13, 4, 3, PAL_PAINT(PAINT_BUBBLEGUM, 2) == 0 ? 0 : PAL_RAMP(RAMP_PEACH, 7));
}
static void zzz(int x, int y, int f) {
    for (int i = 0; i < 3; i++) {
        int ph = (f + i * 12) % 36;
        int zx = x + ph / 3 + (int)(2 * sinf(ph * 0.3f)), zy = y - ph;
        const char *z = i == 1 ? "Z" : "z";
        if (ph < 30) tk_text_outline(zx, zy, z, PAL_WHITE, PAL_RAMP(RAMP_SKY, 2), 1);
    }
}

/* ---- the six rooms ---------------------------------------------------------- */
static void room_friends(int x, int y, int f) {
    wallpaper(x, y, ROOM_W, ROOM_H, PAL_PAINT(PAINT_LEMON, 2), PAL_RAMP(RAMP_CANDY, 6), 0);
    lamp_glow(x + 51, y, 20);
    int fy = y + ROOM_H - 6;
    TkChibi a = tk_random_citizen(120, 0), b = tk_random_citizen(77, 0);
    a.hair_style = TKH_SPIKY; a.wings = 0; a.tail = 0; a.outfit = PAL_PAINT(PAINT_SKY, 1);
    b.hair_style = TKH_BOB; b.wings = 0; b.tail = 0; b.outfit = PAL_PAINT(PAINT_ROSE, 1); b.hat = 0;
    int t = f;
    if (t < 13) { /* high five! */
        int hop = (t > 4 && t < 10) ? 3 : 0;
        chibi_wave(x + 30, fy - hop, &a, 2, TKE_HAPPY, TKM_OPEN, t, 0, 1);
        chibi_wave(x + 73, fy - hop, &b, 2, TKE_HAPPY, TKM_OPEN, t, 1, 1);
        if (t >= 7 && t < 12) { tk_star(x + 51, fy - 46 - hop, 4 - (t - 7) / 2, PAL_WHITE); tk_sparkles(x + 51, fy - 44, t, 3, 8); }
    } else {
        int sh = (t / 2) & 1;
        int lean = t < 16 ? 0 : t < 30 ? 3 : 5;
        tk_chibi(x + 28 + lean + sh, fy, &a, 2, TKE_ANGRY, (t / 4) & 1 ? TKM_SCREAM : TKM_OPEN, t, 0);
        tk_chibi(x + 75 - lean - sh, fy, &b, 2, TKE_ANGRY, (t / 4) & 1 ? TKM_OPEN : TKM_SCREAM, t, 1);
        anger(x + 40, fy - 50, t);
        anger(x + 63, fy - 52, t + 2);
        int pow = t >= 30 && t < 37;
        if (t >= 15 && !pow) grawlix(x + 4, y + 2, t - 15, (t / 6) & 1 ? "#@%!" : "&*$#");
        if (t >= 19 && !pow) grawlix(x + 58, y + 4, t - 19, (t / 6) & 1 ? "!!$#" : "%@!!");
    }
    /* glare lightning between their eyes, then POW */
    if (t >= 22) {
        int ey = fy - 36, x0 = x + 40, x1 = x + 63;
        uint32_t r = tk_hash((uint32_t)t * 31u);
        int px = x0, py = ey;
        for (int i = 1; i <= 5; i++) {
            int nx = x0 + (x1 - x0) * i / 5, ny = ey + (i < 5 ? tk_rand(&r, 7) - 3 : 0);
            tk_line(px, py - 1, nx, ny - 1, PAL_PAINT(PAINT_COBALT, 1));
            tk_line(px, py + 1, nx, ny + 1, PAL_PAINT(PAINT_COBALT, 1));
            tk_line(px, py, nx, ny, PAL_PAINT(PAINT_CYAN, 2));
            px = nx; py = ny;
        }
    }
    if (t >= 30 && t < 38) {
        int a2 = t - 30, r = a2 < 3 ? 4 + a2 * 3 : 12;
        tk_star(x + 51, fy - 44, r, PAL_UI_GOLD);
        tk_star(x + 51, fy - 44, r - 3, PAL_WHITE);
        if (a2 < 7) tk_text_outline(x + 34, y + 3 + (a2 < 2 ? -3 : 0), "POW", PAL_UI_GOLD, PAL_UI_RED_D, 2);
    }
}

static void room_sleep(int x, int y, int f) {
    wallpaper(x, y, ROOM_W, ROOM_H, PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7), 1);
    night_window(x + 70, y + 8, f);
    int fy = y + ROOM_H - 6;
    /* bed (front view): headboard, pillow, sleeper, blanket, frame */
    tk_rect(x + 10, fy - 46, 44, 46, PAL_RAMP(RAMP_WOOD, 2));
    tk_rect(x + 12, fy - 44, 40, 44, PAL_RAMP(RAMP_WOOD, 4));
    tk_heart(x + 32, fy - 40, 1, PAL_PAINT(PAINT_ROSE, 2));
    tk_rect(x + 14, fy - 30, 36, 12, PAL_WHITE);                             /* pillow */
    tk_rect(x + 14, fy - 19, 36, 1, PAL_GRAY_L);
    TkChibi s = tk_random_citizen(31, 0);
    s.hat = 0; s.wings = 0; s.halo = 0; s.horns = 0; s.hair_style = TKH_SHORT;
    int breathe = ((f / 12) & 1);
    tk_chibi(x + 32, fy + 15, &s, 2, TKE_SLEEPY, TKM_O, 0, 0);
    tk_rect(x + 8, fy - 10 + breathe, 48, 10 - breathe, PAL_RAMP(RAMP_PASTEL_PINK, 1));   /* blanket */
    tk_rect(x + 9, fy - 9 + breathe, 46, 8 - breathe, PAL_PAINT(PAINT_LAVENDER, 2));
    for (int i = 0; i < 46; i += 8) tk_rect(x + 9 + i, fy - 9 + breathe, 4, 8 - breathe, PAL_PAINT(PAINT_LAVENDER, 1));
    tk_rect(x + 9, fy - 9 + breathe, 46, 2, PAL_WHITE);
    tk_rect(x + 8, fy - 1, 48, 7, PAL_RAMP(RAMP_WOOD, 2));
    tk_rect(x + 10, fy, 44, 1, PAL_RAMP(RAMP_WOOD, 4));
    zzz(x + 44, fy - 34, f);
    /* nightstand with the Doomogachi on it */
    int nx = x + 66, ny = fy;
    tk_rect(nx, ny - 14, 20, 14, PAL_RAMP(RAMP_WOOD, 3));
    tk_rect(nx + 1, ny - 13, 18, 1, PAL_RAMP(RAMP_WOOD, 5));
    tk_rect(nx + 8, ny - 8, 4, 2, PAL_RAMP(RAMP_WOOD, 5));
    tk_ellipse(nx + 10, ny - 20, 5, 6, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    tk_ellipse(nx + 10, ny - 20, 4, 5, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    int beep = (f % 60) >= 20 && (f % 60) < 34;
    tk_rect(nx + 8, ny - 22, 5, 4, beep && ((f / 2) & 1) ? PAL_WHITE : PAL_UI_LCD);
    if (beep) tk_text_outline(nx - 6, ny - 36, "beep.", PAL_WHITE, PAL_RAMP(RAMP_SKY, 2), 1);
}

static void room_love(int x, int y, int f) {
    wallpaper(x, y, ROOM_W, ROOM_H, PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), 2);
    lamp_glow(x + 51, y, 16);
    int fy = y + ROOM_H - 6;
    TkChibi a = tk_random_citizen(201, 0), b = tk_random_citizen(9, 0);
    a.hair_style = TKH_SHORT; a.wings = 0; a.hat = 0; a.halo = 0; a.horns = 0; a.outfit = PAL_PAINT(PAINT_MINT, 1);
    b.hair_style = TKH_TWINTAILS; b.wings = 0; b.hat = 0; b.halo = 0; b.horns = 0; b.outfit = PAL_PAINT(PAINT_LAVENDER, 1);
    int burst = 60;
    float t = tk_clampf((f - 46) / 14.0f, 0, 1);
    int close = (int)(9 * tk_ease(t));
    int hop = (f >= burst && f < burst + 8) ? (int)(4 * sinf((f - burst) * 0.4f)) : 0;
    tk_chibi(x + 26 + close, fy - hop, &a, 2, f >= 46 ? TKE_LOVE : TKE_HAPPY, TKM_SMILE, f, 0);
    tk_chibi(x + 77 - close, fy - hop, &b, 2, f >= 46 ? TKE_LOVE : TKE_HAPPY, TKM_CAT, f + 7, 1);
    if (f >= 40 && f < burst) { /* little rising hearts */
        tk_heart(x + 51 + (int)(3 * sinf(f * 0.4f)), fy - 40 - (f - 40) % 12, 1, PAL_PAINT(PAINT_CHERRY, 1));
    }
    if (f >= burst) {
        int a2 = f - burst;
        static const int SZ[] = { 1, 2, 4, 5, 4, 4 };
        int sz = a2 < 6 ? SZ[a2] : 4 + ((a2 / 6) & 1);
        tk_heart(x + 51, fy - 48 - (a2 < 6 ? 0 : 2), sz, PAL_PAINT(PAINT_CHERRY, 1));
        tk_hearts(x + 51, fy - 30, a2, 11);
        if (a2 < 30) tk_confetti(x + 51, fy - 46, a2, 23);
    }
}

static void room_cupcake(int x, int y, int f) {
    wallpaper(x, y, ROOM_W, ROOM_H, PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_MINT, 7), 0);
    lamp_glow(x + 30, y, 16);
    int fy = y + ROOM_H - 6;
    /* counter with more cupcakes */
    tk_rect(x + 62, fy - 16, 38, 16, PAL_RAMP(RAMP_PEACH, 4));
    tk_rect(x + 62, fy - 17, 38, 2, PAL_RAMP(RAMP_PEACH, 6));
    for (int i = 0; i < 3; i++) cupcake(x + 70 + i * 11, fy - 17, 0);
    int t = f - 60;
    int bites = t < 6 ? 0 : t < 12 ? 1 : t < 18 ? 2 : 3;
    int sign = t >= 22;
    int hop = sign ? (int)(3 * fabsf(sinf(t * 0.35f))) : 0;
    int chew = (t >= 0 && t < 20 && (t % 6) < 3);
    tk_chibi(x + 36, fy - hop, &TK_NANA, 2, sign ? TKE_SPARKLE : TKE_HAPPY, chew ? TKM_CAT : TKM_OPEN, f, 0);
    if (!sign) {
        int cy = fy - 26 - (t >= 0 && t < 20 && (t % 6) < 2 ? 4 : 0);
        cupcake(x + 50, cy, bites);
        if (bites > 0 && t < 22) for (int i = 0; i < 3; i++) tk_px(x + 46 + i * 3, fy - 20 + ((t + i * 2) % 8), PAL_RAMP(RAMP_PEACH, 6));
    } else {
        /* "10/10" (she baked it herself) */
        int sx = x + 64, sy = fy - 50 - hop + (t < 24 ? 6 : 0);
        tk_rect(sx - 1, sy + 14, 3, 14, PAL_RAMP(RAMP_WOOD, 3));
        tk_rect(sx - 18, sy - 1, 37, 16, PAL_BLACK);
        tk_rect(sx - 17, sy, 35, 14, PAL_WHITE);
        tk_frame(sx - 16, sy + 1, 33, 12, PAL_PAINT(PAINT_CHERRY, 1));
        tk_text(sx - 14, sy + 3, "10/10", PAL_PAINT(PAINT_CHERRY, 1), 1);
        tk_sparkles(sx, sy + 6, t, 41, 20);
    }
}

static void room_party(int x, int y, int f) {
    wallpaper(x, y, ROOM_W, ROOM_H, PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 7), 1);
    int fy = y + ROOM_H - 6;
    /* disco ball + light spots */
    tk_rect(x + 51, y, 1, 6, PAL_GRAY_D);
    tk_circle(x + 51, y + 10, 5, PAL_GRAY_L);
    for (int j = -4; j <= 4; j += 2) for (int i = -4; i <= 4; i += 2) if (((i + j + f / 3) & 2) == 0) tk_px(x + 51 + i, y + 10 + j, PAL_WHITE);
    static const int SC[] = { PAINT_CHERRY, PAINT_LEMON, PAINT_MINT, PAINT_SKY, PAINT_MAGENTA };
    for (int i = 0; i < 6; i++) {
        int sx = x + 8 + ((i * 37 + f * 3) % 88), sy = y + 14 + ((i * 23 + f) % 30);
        tk_dither_rect(sx, sy, 6, 4, PAL_PAINT(SC[(i + f / 8) % 5], 2), 8);
    }
    TkChibi d = tk_random_citizen(404, 0);
    d.hat = 1; d.wings = 0; d.hair_style = TKH_AFRO; d.outfit = PAL_PAINT(PAINT_MAGENTA, 1);
    int hop = (int)(6 * fabsf(sinf(f * 0.39f)));
    tk_chibi(x + 40, fy - hop, &d, 2, TKE_HAPPY, TKM_OPEN, f, (f / 8) & 1);
    /* boombox */
    tk_rect(x + 72, fy - 12, 22, 12, PAL_GRAY_D);
    tk_circle(x + 77, fy - 6, 3, PAL_BLACK); tk_circle(x + 89, fy - 6, 3, PAL_BLACK);
    int pump = (f / 4) & 1;
    tk_circle(x + 77, fy - 6, 1 + pump, PAL_GRAY); tk_circle(x + 89, fy - 6, 1 + pump, PAL_GRAY);
    tk_rect(x + 76, fy - 15, 14, 2, PAL_GRAY);
    for (int i = 0; i < 2; i++) {
        int ph = (f + i * 15) % 30;
        int nx = x + 80 + i * 6 + ph / 4, ny = fy - 18 - ph;
        tk_rect(nx, ny, 2, 2, PAL_BLACK); tk_rect(nx + 1, ny - 5, 1, 5, PAL_BLACK); tk_px(nx + 2, ny - 5, PAL_BLACK);
    }
}

static void room_starer(int x, int y, int f) {
    /* lights off. he does not need them. */
    wallpaper(x, y, ROOM_W, ROOM_H, PAL_RAMP(RAMP_STONE, 2), PAL_RAMP(RAMP_STONE, 1), 0);
    tk_rect(x, y + ROOM_H - 8, ROOM_W, 8, PAL_RAMP(RAMP_WOOD, 1));
    tk_rect(x, y + ROOM_H - 10, ROOM_W, 2, PAL_RAMP(RAMP_STONE, 3));
    int fy = y + ROOM_H - 6;
    /* a bare bulb, off */
    tk_rect(x + 51, y, 1, 8, PAL_GRAY_D);
    tk_circle(x + 51, y + 10, 2, PAL_GRAY_D);
    /* a lonely chair */
    tk_rect(x + 80, fy - 18, 2, 18, PAL_RAMP(RAMP_WOOD, 2));
    tk_rect(x + 80, fy - 9, 12, 2, PAL_RAMP(RAMP_WOOD, 2));
    tk_rect(x + 90, fy - 9, 2, 9, PAL_RAMP(RAMP_WOOD, 2));
    TkChibi s = { PAL_SKIN(SKIN_FAIR, 1), PAL_PAINT(PAINT_CHOCOLATE, 1), PAL_PAINT(PAINT_SILVER, 0),
                  PAL_PAINT(PAINT_CHARCOAL, 1), TKH_BOB, 0, 0, 0, 0, 0, 0 };
    if (f >= 136) chibi_wave(x + 51, fy, &s, 2, TKE_NORMAL, TKM_FLAT, 0, 0, ((f - 136) / 5) & 1);
    else tk_chibi(x + 51, fy, &s, 2, TKE_NORMAL, TKM_FLAT, 0, 0);  /* bob 0: no bounce, never blinks */
}

/* ---- building --------------------------------------------------------------- */
static void sky(int f) {
    static const uint8_t c[] = { PAL_RAMP(RAMP_SKY, 1), PAL_RAMP(RAMP_SKY, 2), PAL_RAMP(RAMP_VAPE, 3),
                                 PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 5) };
    tk_vgrad(0, -camy, TW, 260, c, 5);
    if (camy > 0) tk_rect(0, 260 - camy, TW, TH, PAL_RAMP(RAMP_PASTEL_PINK, 5));
    for (int i = 0; i < 30; i++) {
        uint32_t s = tk_hash((uint32_t)i * 77u + 5u);
        int x = (int)(s % TW), y = (int)((s >> 8) % 150) - camy;
        if ((f + i * 7) % 40 < 34) tk_px(x, y, PAL_WHITE);
    }
}
static void building(int f) {
    uint8_t ol = PAL_RAMP(RAMP_PASTEL_PINK, 1), wall = PAL_RAMP(RAMP_PEACH, 6), wd = PAL_RAMP(RAMP_PEACH, 5);
    int top = ROOF_H - 6 - camy, bot = ROOF_H + 2 + 3 * FLOOR_H - camy;
    tk_rect(BX0 - 1, top, BX1 - BX0 + 2, bot - top + 20, ol);
    tk_rect(BX0, top + 1, BX1 - BX0, bot - top + 20, wall);
    for (int y = top + 4; y < bot + 20; y += 6)
        for (int x = BX0 + ((y / 6) & 1) * 6; x < BX1; x += 12) tk_rect(x, y, 5, 1, wd);
    /* roof sign */
    int sy = 4 - camy;
    tk_rect(BX0 + 14, sy + 4, BX1 - BX0 - 28, 24, ol);
    tk_rect(BX0 + 15, sy + 5, BX1 - BX0 - 30, 22, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    for (int i = 0; i < 18; i++) {
        int bx = BX0 + 18 + i * 11, on = ((i + f / 4) % 3) != 0;
        tk_px(bx, sy + 6, on ? PAL_UI_GOLD : PAL_PAINT(PAINT_HONEY, 0));
        tk_px(bx, sy + 25, on ? PAL_UI_GOLD : PAL_PAINT(PAINT_HONEY, 0));
    }
    tk_caption(sy + 16, "HILLVILE TOWERS", PAL_WHITE, ol, 1);
    tk_rect(BX0 + 30, sy + 28, 3, top - sy - 28 + 1, ol);
    tk_rect(BX1 - 33, sy + 28, 3, top - sy - 28 + 1, ol);
    tk_heart(120, sy, 2, PAL_PAINT(PAINT_CHERRY, 1));
    /* ground */
    int gy = bot + 6;
    tk_rect(0, gy, TW, TH, PAL_RAMP(RAMP_GRASS, 4));
    tk_rect(0, gy, TW, 2, PAL_RAMP(RAMP_GRASS, 5));
    for (int i = 0; i < 24; i++) tk_px(i * 10 + 3, gy + 4 + (i % 3), PAL_RAMP(RAMP_GRASS, 3));
    /* the front door */
    tk_rect(112, bot - 2, 16, 8, ol);
}
static void room_frame(int x, int y) {
    tk_frame(x - 1, y - 1, ROOM_W + 2, ROOM_H + 2, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    tk_dither_rect(x, y, ROOM_W, 3, PAL_BLACK, 5);
}

void scene_s06_life(int f, int n) {
    (void)n;
    /* camera tilt: hold, ease, hold, ease, hold */
    float c;
    if (f < 38) c = 0 + f * 0.1f;
    else if (f < 50) c = 3.8f + (80 - 3.8f) * tk_ease((f - 38) / 12.0f);
    else if (f < 98) c = 80 + (f - 50) * 0.06f;
    else if (f < 110) c = 82.9f + (128 - 82.9f) * tk_ease((f - 98) / 12.0f);
    else c = 128 + (f - 110) * 0.05f;
    camy = (int)c;

    sky(f);
    building(f);
    for (int fl = 2; fl >= 0; fl--) {
        int y = floor_top(fl);
        if (y > TH || y + ROOM_H < 0) continue;
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
            clip_end(rx, y, ROOM_W, ROOM_H);
        }
        room_frame(LX, y);
        room_frame(RXR, y);
    }

    /* captions */
    uint8_t oc = PAL_RAMP(RAMP_PASTEL_PINK, 1);
    struct { int a, b, cy; const char *s; } C[] = {
        { 4, 38, 141, "Make friends!" },
        { 52, 74, 16, "Fall in love!" },
        { 76, 98, 16, "Eat cupcakes!" },
        { 114, 999, 149, "Ignore that one." },
    };
    for (int i = 0; i < 4; i++) {
        if (f < C[i].a || f >= C[i].b) continue;
        int age = f - C[i].a, dy = age == 0 ? -5 : age == 1 ? 1 : 0;
        tk_caption(C[i].cy + dy, C[i].s, PAL_WHITE, oc, 2);
    }
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
    tk_sfx(t0 + 52 * fr, SFX_SPARKLE, 0.4f);
    tk_sfx(t0 + 60 * fr, SFX_POP, 0.8f);                      /* heart burst */
    tk_sfx(t0 + 61 * fr, SFX_CHOIR, 0.35f);
    for (int k = 0; k < 3; k++) tk_slide(t0 + (61 + k * 6) * fr, 0.06, 64, 52, TKV_SQ25, 0.3f);  /* nom */
    tk_sfx(t0 + 82 * fr, SFX_FANFARE, 0.4f);                  /* 10/10 */
    /* the starer: total silence, then one tiny squeak when he waves */
    tk_sfx(t0 + 136 * fr, SFX_SQUEAK, 0.35f);
}
