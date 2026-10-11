/* s07_vape.c - "Cloud Nine Vapor - GRAND OPENING!"   (5.5 s, 165 frames)
 *
 * Shot A  f  0- 35  wide: the vape shop right next to the Wall (purple fog
 *                   oozing over it), balloons, a "NO ANGELS" sandwich board.
 *                   A customer in a party hat boings up to the counter.
 * Shot B  f 36- 87  Vex behind the counter. Employee of the Month: a certain
 *                   crowned cloud. "Try Brimstone Blast. It's organic."
 *                   Wink. The pen slides across; a hand snatches it.
 * Shot C  f 88-117  close-up: inhale (stretch) ... hold, smoke leaking out of
 *                   the ears ... FWOOMP: a gigantic purple cloud eats the screen.
 * Shot D  f118-164  the cloud clears: possessed eyes, a smile far too wide,
 *                   party hat still on. Silence. "Everything is fine."
 *                   Head tilt. Creak. A balloon pops.
 */
#include "tk.h"
#include <math.h>
#include <string.h>

#define CUT_B 36
#define CUT_C 88
#define CUT_D 118

static const TkChibi CUSTOMER = {
    PAL_SKIN(SKIN_FAIR, 1), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_LEMON, 1),
    PAL_PAINT(PAINT_TEAL, 1), TKH_BOB, 0, 0, 0, 0, 1, 1 };

/* Vex with a touch more face/hair contrast for the close-up */
static TkChibi vex_look(void) {
    TkChibi v = TK_VEX;
    v.skin = PAL_PAINT(PAINT_LAVENDER, 2);
    return v;
}

/* chibi pixel -> screen (flip = 0): chibi coords as used inside tk.c */
static void cpix(int x, int yf, int s, int cx, int cy, uint8_t c) {
    tk_rect(x + (cx - 9) * s, yf + (cy - 27) * s, s, s, c);
}

/* ---- chibi capture so we can squash & stretch it ------------------------ */
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
/* blit with feet at (x,y), scale sx/sy around the feet; rows above `neck`
 * (in source px above the feet) are shifted by `tilt` px. */
static void cap_blit(const Cap *c, int x, int y, float sx, float sy, int neck, int tilt) {
    const int ax = CAPW / 2, ay = CAPH - 10;
    int x0 = x - (int)(ax * sx) - 4, x1 = x + (int)((CAPW - ax) * sx) + 4;
    int y0 = y - (int)(ay * sy) - 2, y1 = y + (int)((CAPH - ay) * sy) + 2;
    for (int dy = y0; dy < y1; dy++)
        for (int dx = x0; dx < x1; dx++) {
            int v = ay + (int)floorf((dy - y) / sy + 0.5f);
            int sh = (tilt && v < ay - neck) ? tilt : 0;
            int u = ax + (int)floorf((dx - sh - x) / sx + 0.5f);
            if (u < 0 || v < 0 || u >= CAPW || v >= CAPH) continue;
            uint8_t p = c->px[v][u];
            if (p) tk_px(dx, dy, p);
        }
}

/* ---- fluffy cloud masses (outline, shadow, body, highlight passes) -------- */
typedef struct { int x, y, r; } Puff;
static void cloud_mass(const Puff *p, int n) {
    for (int i = 0; i < n; i++) if (p[i].r > 0) tk_circle(p[i].x, p[i].y, p[i].r + 1, PAL_RAMP(RAMP_VAPE, 2));
    for (int i = 0; i < n; i++) if (p[i].r > 0) tk_circle(p[i].x, p[i].y, p[i].r, PAL_RAMP(RAMP_VAPE, 4));
    for (int i = 0; i < n; i++) if (p[i].r > 2) tk_circle(p[i].x - p[i].r / 6, p[i].y - p[i].r / 5, p[i].r * 5 / 6, PAL_RAMP(RAMP_VAPE, 5));
    for (int i = 0; i < n; i++) if (p[i].r > 4) tk_circle(p[i].x - p[i].r / 3, p[i].y - p[i].r / 3, p[i].r / 2, PAL_RAMP(RAMP_VAPE, 6));
    for (int i = 0; i < n; i++) if (p[i].r > 8) tk_circle(p[i].x - p[i].r / 2, p[i].y - p[i].r / 2, p[i].r / 6, PAL_RAMP(RAMP_VAPE, 7));
}
/* a burst of puffs from (cx,cy). dist / rad are multipliers (0..1+) */
static void cloud_burst(int cx, int cy, float dist, float rad, int age, uint32_t seed) {
    Puff p[48];
    uint32_t s = tk_hash(seed);
    int n = 0;
    for (int i = 0; i < 44; i++) {
        float a = tk_rand(&s, 6283) / 1000.0f + age * 0.01f * ((i & 1) ? 1 : -1);
        float d = (float)(10 + tk_rand(&s, 150)) * dist;
        int r = (int)((10 + tk_rand(&s, 26)) * rad);
        p[n++] = (Puff){ cx + (int)(cosf(a) * d * 1.4f), cy + (int)(sinf(a) * d * 0.75f) - age / 3, r };
    }
    cloud_mass(p, n);
}

/* ---- little props --------------------------------------------------------- */
static void balloon(int ax, int ay, int x, int y, uint8_t col) {
    for (int i = 0; i <= 16; i++) {
        float t = i / 16.0f;
        int px = (int)(ax + (x - ax) * t + sinf(t * 6.0f) * 1.5f), py = (int)(ay + (y + 7 - ay) * t);
        tk_px(px, py, PAL_GRAY);
    }
    tk_ellipse(x, y, 5, 6, PAL_BLACK);
    tk_ellipse(x, y, 4, 5, col);
    uint8_t lo = (col > PAL_PAINT_BASE && col < PAL_SKIN_BASE) ? (uint8_t)(col - 1) : col;
    tk_rect(x + 1, y + 2, 3, 2, lo); tk_rect(x + 2, y, 2, 2, lo);
    tk_rect(x - 2, y - 3, 2, 2, PAL_WHITE);
    tk_tri(x - 1, y + 7, x + 1, y + 7, x, y + 5, col);
}

/* the Brimstone Blast pen, scale s, glow 0..2 */
static void pen(int x, int y, int s, int glow) {
    tk_rect(x - s, y - s, 16 * s, 5 * s, PAL_BLACK);
    tk_rect(x, y, 14 * s, 3 * s, PAL_PAINT(PAINT_VAPE_PURPLE, 1));
    tk_rect(x, y, 14 * s, s, PAL_PAINT(PAINT_VAPE_PURPLE, 2));
    tk_rect(x + 9 * s, y, 2 * s, 3 * s, PAL_UI_GOLD);
    tk_rect(x + 3 * s, y + s, s, s, PAL_WHITE);   /* tiny skull logo */
    tk_rect(x + 13 * s, y, s, 3 * s, glow ? PAL_RAMP(RAMP_LAVA, 6 + (glow > 1)) : PAL_RAMP(RAMP_LAVA, 3));
    if (glow > 1) { tk_px(x + 15 * s, y, PAL_RAMP(RAMP_LAVA, 5)); tk_px(x + 15 * s, y + 2 * s, PAL_RAMP(RAMP_LAVA, 5)); }
}

/* the shop's logo: a cloud with tiny devil horns */
static void logo_cloud(int x, int y, uint8_t c) {
    tk_circle(x - 4, y + 1, 3, PAL_BLACK); tk_circle(x + 4, y + 1, 3, PAL_BLACK); tk_circle(x, y - 1, 4, PAL_BLACK);
    tk_circle(x - 4, y + 1, 2, c); tk_circle(x + 4, y + 1, 2, c); tk_circle(x, y - 1, 3, c);
    tk_rect(x - 5, y + 1, 10, 3, c);
    tk_px(x - 3, y - 5, PAL_UI_RED); tk_px(x - 3, y - 6, PAL_UI_RED); tk_px(x - 4, y - 7, PAL_UI_RED);
    tk_px(x + 3, y - 5, PAL_UI_RED); tk_px(x + 3, y - 6, PAL_UI_RED); tk_px(x + 4, y - 7, PAL_UI_RED);
    tk_px(x - 1, y, PAL_BLACK); tk_px(x + 2, y, PAL_BLACK);
}

static void sky(int f) {
    static const uint8_t g[] = { PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_PASTEL_PINK, 7),
                                 PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    tk_vgrad(0, 0, TW, 120, g, 4);
    for (int i = 0; i < 3; i++) {
        int cx = ((i * 97 + f / 3) % 300) - 30, cy = 8 + i * 6;
        tk_ellipse(cx, cy, 12, 4, PAL_WHITE); tk_circle(cx - 4, cy - 3, 4, PAL_WHITE); tk_circle(cx + 4, cy - 4, 5, PAL_WHITE);
    }
}

/* the Wall to Hellvile, purple fog leaking over the top */
static void wall(int x0, int f) {
    tk_rect(x0, 18, TW - x0, 110, PAL_RAMP(RAMP_STONE, 2));
    for (int r = 0; r < 14; r++) {
        int y = 22 + r * 8;
        tk_rect(x0, y, TW - x0, 1, PAL_RAMP(RAMP_STONE, 1));
        for (int x = x0 + ((r & 1) ? 6 : 0); x < TW; x += 14) {
            tk_rect(x, y + 1, 1, 7, PAL_RAMP(RAMP_STONE, 1));
            tk_rect(x + 1, y + 1, 12, 1, PAL_RAMP(RAMP_STONE, 3));
        }
    }
    for (int x = x0; x < TW; x += 12) {
        tk_rect(x, 12, 7, 7, PAL_RAMP(RAMP_STONE, 2)); tk_rect(x, 12, 7, 1, PAL_RAMP(RAMP_STONE, 4));
    }
    tk_rect(x0, 18, 1, 110, PAL_RAMP(RAMP_STONE, 0));
    /* KEEP OUT board */
    tk_rect(x0 + 8, 42, 44, 24, PAL_BLACK);
    tk_rect(x0 + 9, 43, 42, 22, PAL_RAMP(RAMP_WOOD, 3));
    tk_rect(x0 + 9, 53, 42, 1, PAL_RAMP(RAMP_WOOD, 2));
    tk_rect(x0 + 9, 43, 42, 1, PAL_RAMP(RAMP_WOOD, 5));
    tk_text(x0 + 18, 45, "KEEP", PAL_UI_RED, 1);
    tk_text(x0 + 21, 55, "OUT", PAL_UI_RED, 1);
    tk_px(x0 + 11, 45, PAL_GRAY_L); tk_px(x0 + 48, 45, PAL_GRAY_L);
    /* purple fog oozing over the top */
    Puff p[10];
    for (int i = 0; i < 10; i++) {
        uint32_t s = tk_hash(i * 31 + 7);
        int life = 80, ph = (f + tk_rand(&s, life)) % life;
        p[i] = (Puff){ x0 + 44 - tk_rand(&s, 30) - ph * 2 / 3, 10 + ph / 6 + tk_rand(&s, 4), 2 + ph / 12 };
    }
    cloud_mass(p, 10);
}

static void shelves(int x0, int x1, int y, int f) {
    tk_rect(x0, y + 10, x1 - x0, 2, PAL_RAMP(RAMP_WOOD, 3));
    tk_rect(x0, y + 12, x1 - x0, 1, PAL_RAMP(RAMP_WOOD, 1));
    static const uint8_t jc[] = { PAL_PAINT(PAINT_VAPE_PURPLE, 2), PAL_PAINT(PAINT_HELLFIRE, 1), PAL_PAINT(PAINT_SLIME, 1),
                                  PAL_PAINT(PAINT_MAGENTA, 1), PAL_PAINT(PAINT_CHERRY, 1) };
    int k = 0;
    for (int x = x0 + 3; x < x1 - 5; x += 8, k++) {
        uint8_t c = jc[(k * 3) % 5];
        tk_rect(x, y + 2, 5, 8, PAL_BLACK);
        tk_rect(x + 1, y + 3, 3, 6, c);
        tk_rect(x + 1, y, 3, 2, PAL_GRAY_D);
        tk_px(x + 2, y + 5, PAL_WHITE);
        if ((f / 6 + k) % 7 == 0) tk_px(x + 1, y + 3, PAL_WHITE);
    }
}

static void sandwich_board(int x, int y) {
    tk_line(x + 2, y + 26, x + 6, y, PAL_RAMP(RAMP_WOOD, 2));
    tk_line(x + 38, y + 26, x + 34, y, PAL_RAMP(RAMP_WOOD, 2));
    tk_rect(x + 2, y, 36, 22, PAL_BLACK);
    tk_rect(x + 3, y + 1, 34, 20, PAL_WHITE);
    tk_text(x + 15, y + 2, "NO", PAL_UI_RED, 1);
    tk_text(x + 3, y + 11, "ANGELS", PAL_BLACK, 1);
    /* crossed-out halo */
    tk_ellipse(x + 31, y + 5, 3, 2, PAL_UI_GOLD);
    tk_ellipse(x + 31, y + 5, 1, 0, PAL_WHITE);
    tk_line(x + 27, y + 8, x + 35, y + 2, PAL_UI_RED);
}

/* ---- shot A: wide establishing ------------------------------------------- */
static void shot_wide(int f, int cust_x, int cust_y_off, float sx, float sy, int expr) {
    sky(f);
    wall(186, f);
    /* sidewalk + grass */
    tk_rect(0, 120, TW, 40, PAL_RAMP(RAMP_PEACH, 6));
    for (int x = 0; x < TW; x += 16) tk_rect(x, 120, 1, 30, PAL_RAMP(RAMP_PEACH, 5));
    tk_rect(0, 134, TW, 1, PAL_RAMP(RAMP_PEACH, 5));
    tk_rect(0, 150, TW, 2, PAL_RAMP(RAMP_PEACH, 4));
    tk_rect(0, 152, TW, 8, PAL_RAMP(RAMP_GRASS, 5));
    for (int x = 3; x < TW; x += 7) tk_px(x, 151, PAL_RAMP(RAMP_GRASS, 6));
    /* facade */
    tk_rect(20, 44, 162, 78, PAL_PAINT(PAINT_LAVENDER, 1));
    for (int x = 24; x < 180; x += 6) tk_rect(x, 44, 1, 78, PAL_PAINT(PAINT_LAVENDER, 0));
    tk_frame(19, 43, 164, 79, PAL_BLACK);
    /* sign board */
    tk_rect(30, 20, 142, 24, PAL_BLACK);
    tk_rect(31, 21, 140, 22, PAL_PAINT(PAINT_VAPE_PURPLE, 0));
    tk_rect(32, 22, 138, 1, PAL_PAINT(PAINT_VAPE_PURPLE, 1));
    logo_cloud(45, 33, PAL_WHITE);
    tk_text_outline(60, 23, "CLOUD NINE", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0), 1);
    tk_text_outline(60, 32, "VAPOR", PAL_PAINT(PAINT_MAGENTA, 2), PAL_PAINT(PAINT_GRAPE, 0), 1);
    tk_text(98, 32, "~ est. today", PAL_RAMP(RAMP_VAPE, 6), 1);
    /* window / counter opening with Vex inside */
    tk_rect(58, 64, 102, 40, PAL_PAINT(PAINT_GRAPE, 0));
    shelves(62, 156, 70, f);
    tk_frame(57, 63, 104, 42, PAL_BLACK);
    TkChibi vx = vex_look();
    tk_chibi(124, 122, &vx, 2, TKE_SMUG, TKM_CAT, f, 0);
    /* counter */
    tk_rect(54, 102, 110, 4, PAL_PAINT(PAINT_MAGENTA, 2));
    tk_rect(54, 106, 110, 16, PAL_PAINT(PAINT_VAPE_PURPLE, 0));
    tk_frame(53, 101, 112, 21, PAL_BLACK);
    for (int x = 62; x < 160; x += 18) logo_cloud(x + 2, 113, PAL_PAINT(PAINT_LAVENDER, 2));
    /* awning (over Vex's head) */
    for (int i = 0; i < 17; i++) {
        uint8_t c = (i & 1) ? PAL_WHITE : PAL_PAINT(PAINT_VAPE_PURPLE, 1);
        tk_rect(20 + i * 10, 56, 10, 7, c);
        tk_circle(25 + i * 10, 63, 4, c);
    }
    tk_rect(20, 55, 164, 1, PAL_BLACK);
    /* GRAND OPENING ribbon */
    {
        int y = 44;
        tk_tri(34, y, 44, y + 5, 34, y + 10, PAL_UI_RED_D);
        tk_tri(168, y, 158, y + 5, 168, y + 10, PAL_UI_RED_D);
        tk_rect(42, y, 118, 11, PAL_BLACK);
        tk_rect(43, y + 1, 116, 9, PAL_UI_RED);
        int tw = tk_text_w("GRAND OPENING!", 1);
        int wob = (f / 5) & 1;
        tk_text(101 - tw / 2, y + 1 + wob, "GRAND OPENING!", PAL_WHITE, 1);
    }
    /* door */
    tk_rect(26, 70, 24, 52, PAL_PAINT(PAINT_GRAPE, 1));
    tk_frame(25, 69, 26, 53, PAL_BLACK);
    tk_heart(38, 80, 1, PAL_PAINT(PAINT_MAGENTA, 2));
    tk_rect(45, 98, 2, 3, PAL_UI_GOLD);
    sandwich_board(2, 116);
    /* balloons */
    float bo = sinf(f * 0.15f) * 2;
    balloon(56, 102, 48 + (int)bo, 70, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    balloon(56, 102, 59 - (int)bo, 64, PAL_PAINT(PAINT_MINT, 1));
    balloon(162, 102, 166 - (int)bo, 66, PAL_PAINT(PAINT_LEMON, 1));
    balloon(162, 102, 175 + (int)bo, 74, PAL_PAINT(PAINT_LAVENDER, 1));
    tk_sparkles(100, 30, f, 77, 44);
    /* customer */
    Cap c;
    cap_begin();
    tk_chibi(CAP_X, CAP_Y, &CUSTOMER, 2, expr, TKM_OPEN, f, 0);
    cap_end(&c);
    cap_blit(&c, cust_x, 148 + cust_y_off, sx, sy, 0, 0);
}

/* ---- shot B: Vex at the counter ----------------------------------------- */
static void shot_vex(int t) {
    tk_rect(0, 0, TW, 120, PAL_PAINT(PAINT_GRAPE, 0));
    for (int x = 0; x < TW; x += 20) tk_rect(x, 0, 10, 120, PAL_RAMP(RAMP_VAPE, 2));
    int on = !(t % 23 == 3 || t % 23 == 5);
    tk_text_outline(10, 8, "OPEN 6-6-6", on ? PAL_PAINT(PAINT_MAGENTA, 2) : PAL_RAMP(RAMP_VAPE, 3),
                    on ? PAL_PAINT(PAINT_MAGENTA, 0) : PAL_RAMP(RAMP_VAPE, 1), 1);
    shelves(4, 78, 24, t);
    shelves(4, 78, 48, t + 7);
    shelves(4, 78, 72, t + 3);
    /* employee of the month: a certain crowned cloud */
    {
        int x = 166, y = 6;
        tk_rect(x, y, 64, 46, PAL_UI_GOLD);
        tk_rect(x + 2, y + 2, 60, 42, PAL_RAMP(RAMP_CANDY, 2));
        tk_rect(x + 4, y + 4, 56, 28, PAL_RAMP(RAMP_VAPE, 1));
        int cx = x + 32, cy = y + 21;
        Puff p[4] = { { cx - 9, cy + 3, 6 }, { cx + 9, cy + 3, 6 }, { cx, cy, 9 }, { cx, cy + 5, 7 } };
        cloud_mass(p, 4);
        tk_rect(cx - 6, cy - 12, 13, 4, PAL_UI_GOLD);
        tk_tri(cx - 6, cy - 12, cx - 4, cy - 16, cx - 2, cy - 12, PAL_UI_GOLD);
        tk_tri(cx - 2, cy - 12, cx, cy - 17, cx + 2, cy - 12, PAL_UI_GOLD);
        tk_tri(cx + 2, cy - 12, cx + 4, cy - 16, cx + 6, cy - 12, PAL_UI_GOLD);
        tk_px(cx, cy - 10, PAL_UI_RED);
        tk_rect(cx - 5, cy - 2, 3, 2, PAL_UI_RED); tk_rect(cx + 3, cy - 2, 3, 2, PAL_UI_RED);
        tk_rect(cx - 5, cy + 3, 11, 1, PAL_BLACK);
        tk_px(cx - 6, cy + 2, PAL_BLACK); tk_px(cx + 6, cy + 2, PAL_BLACK);
        tk_px(cx - 3, cy + 4, PAL_WHITE); tk_px(cx + 3, cy + 4, PAL_WHITE);
        tk_text(x + 8, y + 34, "EMPLOYEE", PAL_RAMP(RAMP_CANDY, 7), 1);
    }
    tk_text_outline(162, 56, "OF THE MONTH", PAL_UI_GOLD, PAL_BLACK, 1);
    /* Vex, size 3 */
    TkChibi vx = vex_look();
    int x = 116, yf = 120, s = 3;
    int wink = t >= 25 && t < 36;
    tk_chibi(x, yf, &vx, s, TKE_SMUG, TKM_CAT, t, 0);
    if (wink) {
        /* one eye pops open with a sparkle: the charm offensive */
        int b = (t / 4) & 1;
        for (int j = 6; j <= 10; j++) for (int i = 4; i <= 8; i++) cpix(x, yf, s, i, j + b, vx.skin);
        cpix(x, yf, s, 5, 7 + b, PAL_BLACK); cpix(x, yf, s, 6, 7 + b, PAL_BLACK); cpix(x, yf, s, 7, 7 + b, PAL_BLACK);
        for (int j = 8; j <= 10; j++) { cpix(x, yf, s, 5, j + b, PAL_BLACK); cpix(x, yf, s, 7, j + b, PAL_BLACK);
                                        cpix(x, yf, s, 6, j + b, PAL_PAINT(PAINT_MAGENTA, 1)); }
        cpix(x, yf, s, 6, 8 + b, PAL_WHITE);
        tk_star(x - 22, yf - 66, 4 - (t - 25) / 3 > 0 ? 4 - (t - 25) / 3 : 1, PAL_WHITE);
        tk_star(x - 22, yf - 66, 1, PAL_UI_GOLD);
    }
    /* counter */
    tk_rect(0, 96, TW, 6, PAL_PAINT(PAINT_MAGENTA, 2));
    tk_rect(0, 96, TW, 1, PAL_PAINT(PAINT_MAGENTA, 0));
    tk_rect(0, 102, TW, 58, PAL_PAINT(PAINT_VAPE_PURPLE, 0));
    tk_rect(0, 102, TW, 1, PAL_BLACK);
    /* Vex's hand slides the pen across the counter */
    int k0 = t - 27;
    float k = tk_ease(k0 / 10.0f);
    int px = tk_lerpi(84, 26, k);
    int grabbed = t >= 44;
    if (!grabbed) pen(px, 88, 2, 1);
    if (k0 < 14) {
        int hx = px + 30;
        tk_rect(hx, 86, 36, 10, PAL_BLACK);
        tk_rect(hx + 1, 87, 9, 8, vx.skin);
        tk_rect(hx + 10, 87, 26, 8, PAL_PAINT(PAINT_JET, 1));
        tk_rect(hx + 1, 87, 1, 2, PAL_PAINT(PAINT_MAGENTA, 1)); /* nail polish */
    }
    if (k0 > 0 && k0 < 10) for (int i = 0; i < 3; i++) tk_rect(px + 34 + i * 8, 89 + i, 5, 1, PAL_WHITE);
    /* the customer's hand snatches it */
    if (t >= 38) {
        int hx = tk_lerpi(-24, 22, tk_ease((t - 38) / 6.0f));
        if (grabbed) hx = tk_lerpi(22, -60, tk_ease((t - 44) / 5.0f));
        if (grabbed) pen(hx + 2, 88, 2, 1);
        tk_rect(hx - 40, 84, 40, 12, PAL_BLACK);
        tk_rect(hx - 40, 85, 39, 10, PAL_PAINT(PAINT_LEMON, 1));
        tk_rect(hx, 83, 12, 14, PAL_BLACK);
        tk_rect(hx + 1, 84, 10, 12, PAL_SKIN(SKIN_FAIR, 1));
        tk_rect(hx + 1, 94, 10, 2, PAL_SKIN(SKIN_FAIR, 0));
        if (grabbed && t < 48) for (int i = 0; i < 3; i++) tk_rect(hx + 16 + i * 8, 87 + i * 2, 6, 1, PAL_WHITE);
    }
    tk_dialog("VEX", "Try Brimstone Blast. It's organic.", t - 2, TKW_CUTE);
    if (t >= 31) tk_text(12, TH - 46 + 23, "*may contain souls", PAL_UI_PINK_D, 1);   /* the fine print */
}

/* ---- shots C/D: customer close-up ---------------------------------------- */
static void closeup_bg(void) {
    tk_rect(0, 0, TW, 160, PAL_PAINT(PAINT_LAVENDER, 1));
    for (int x = 2; x < TW; x += 8) tk_rect(x, 0, 1, 160, PAL_PAINT(PAINT_LAVENDER, 0));
    for (int i = 0; i < 13; i++) {
        uint8_t c = (i & 1) ? PAL_WHITE : PAL_PAINT(PAINT_VAPE_PURPLE, 1);
        tk_rect(i * 20, 0, 20, 16, c);
        tk_circle(i * 20 + 10, 16, 9, c);
    }
    tk_rect(0, 15, TW, 1, PAL_BLACK);
    tk_rect(0, 130, TW, 30, PAL_RAMP(RAMP_PEACH, 6));
    tk_rect(0, 130, TW, 1, PAL_RAMP(RAMP_PEACH, 3));
    for (int x = 8; x < TW; x += 24) tk_rect(x, 131, 1, 29, PAL_RAMP(RAMP_PEACH, 5));
}

/* possessed grin, far too wide: chibi coords around the mouth */
static void creepy_smile(int x, int yf, int s) {
    for (int i = -6; i <= 6; i++) {
        int a = i < 0 ? -i : i;
        int r = a <= 3 ? 12 : (a == 4 ? 12 : (a == 5 ? 11 : 10));
        if (a == 6) { cpix(x, yf, s, 9 + i, r, PAL_BLACK); continue; }
        cpix(x, yf, s, 9 + i, r - 1, PAL_BLACK);
        cpix(x, yf, s, 9 + i, r, (a % 2 == 1 && a < 5) ? PAL_GRAY_L : PAL_WHITE);
        cpix(x, yf, s, 9 + i, r + 1, PAL_BLACK);
    }
}

static void shot_closeup(int t) {
    closeup_bg();
    float sx = 1, sy = 1;
    int expr = TKE_NORMAL, mouth = TKM_SMILE;
    const int in0 = 4, hold0 = 15, ex0 = 20;
    if (t >= in0 && t < hold0) {
        float k = tk_ease((t - in0) / (float)(hold0 - in0));
        sy = 1 + 0.14f * k; sx = 1 - 0.08f * k; expr = TKE_HAPPY; mouth = TKM_O;
    } else if (t >= hold0 && t < ex0) {
        sy = 1.04f; sx = 1.1f; expr = TKE_SHOCK; mouth = TKM_FLAT;
    } else if (t >= ex0) {
        float k = tk_bounce((t - ex0) / 8.0f);
        sy = 0.86f + 0.14f * k; sx = 1.18f - 0.18f * k;
        expr = TKE_SHOCK; mouth = TKM_SCREAM;
    }
    Cap c;
    cap_begin();
    tk_chibi(CAP_X, CAP_Y, &CUSTOMER, 3, expr, mouth, 0, 0);
    cap_end(&c);
    int cx = 120, cyf = 156;
    if (t >= hold0 && t < ex0) cx += (t & 1) ? 1 : -1;  /* trembling */
    cap_blit(&c, cx, cyf, sx, sy, 0, 0);
    int my = cyf - (int)(15 * 3 * sy);
    /* hand + pen at the mouth */
    if (t < ex0) {
        pen(cx + 4, my, 2, t >= in0 ? 2 : 1);
        tk_rect(cx + 2, my - 3, 10, 11, PAL_BLACK);
        tk_rect(cx + 3, my - 2, 8, 9, PAL_SKIN(SKIN_FAIR, 1));
        tk_rect(cx + 3, my + 5, 8, 2, PAL_SKIN(SKIN_FAIR, 0));
        if (t >= in0 && t < hold0) {   /* embers + air being sucked in */
            for (int i = 0; i < 6; i++) {
                int ph = (t * 4 + i * 7) % 22;
                tk_rect(cx + 60 - ph * 2, my - 6 + (i % 3) * 5, 3 - ph / 8, 1, (i & 1) ? PAL_WHITE : PAL_RAMP(RAMP_LAVA, 6));
            }
        }
    }
    /* holding it in: smoke leaks out of the ears */
    if (t >= hold0 && t < ex0) {
        int a = t - hold0;
        int ey = cyf - (int)(17 * 3 * sy);
        Puff p[4] = { { cx - 31 - a * 3, ey - a, 3 + a }, { cx + 31 + a * 3, ey - a, 3 + a },
                      { cx - 28 - a, ey - 6 - a * 3, 2 + a / 2 }, { cx + 28 + a, ey - 6 - a * 3, 2 + a / 2 } };
        cloud_mass(p, 4);
    }
    /* FWOOMP */
    if (t >= ex0) {
        int a = t - ex0;
        float k = tk_clampf(a / 8.0f, 0, 1);
        float d = 1 - (1 - k) * (1 - k);
        if (a >= 5) {
            int lvl = (a - 5) * 4; if (lvl > 16) lvl = 16;
            tk_dither_rect(0, 0, TW, TH, PAL_RAMP(RAMP_VAPE, 4), lvl);
        }
        cloud_burst(cx, my, 0.15f + d * 0.9f, 0.4f + d * 1.0f, a, 42);
        cloud_burst(cx, my, 0.05f + d * 0.4f, 0.5f + d * 0.9f, a, 43);
        if (a < 3) tk_post_flash(PAL_RAMP(RAMP_VAPE, 7), 12 - a * 4);
        if (a < 8) tk_post_shake((a & 1) ? 4 - a / 2 : -(4 - a / 2), (a & 2) ? 2 : -2);
    }
}

static void shot_possessed(int t) {
    closeup_bg();
    /* the balloon in the corner (it pops) */
    const int pop = 36;
    float bo = sinf((t + CUT_D) * 0.15f) * 2;
    if (t < pop) balloon(30, 160, 30 + (int)bo, 74, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    else if (t < pop + 3) {
        tk_star(30, 74, 7 - (t - pop), PAL_WHITE); tk_star(30, 74, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    } else {
        int a = t - pop - 3;
        for (int i = 0; i < 18; i++) tk_px(30 + ((i / 3) & 1), 100 + a * a / 2 + i * 2, PAL_GRAY);
    }
    if (t >= pop) tk_confetti(30, 74, t - pop, 5);
    /* the customer: still. too still. */
    Cap c;
    cap_begin();
    tk_chibi(CAP_X, CAP_Y, &CUSTOMER, 3, TKE_POSSESSED, TKM_FLAT, 0, 0);
    creepy_smile(CAP_X, CAP_Y, 3);
    cap_end(&c);
    int cx = 120, cyf = 156;
    int tilt = t >= 26 ? (t >= 28 ? 3 : 1) : 0;
    cap_blit(&c, cx, cyf, 1, 1, 36, tilt);
    /* red eye glints */
    if ((t / 3) % 5 == 0) { tk_px(cx - 9, cyf - 54, PAL_WHITE); tk_px(cx + 12, cyf - 54, PAL_WHITE); }
    /* wisps rising off them */
    for (int i = 0; i < 5; i++) {
        int ph = (t * 2 + i * 13) % 40;
        tk_circle(cx - 24 + i * 12 + (int)(sinf(ph * 0.2f + i) * 3), 74 - ph, 1 + (ph < 20), PAL_RAMP(RAMP_VAPE, 4 + (i & 1)));
    }
    /* red eyes glow inside the cloud ... then it parts */
    if (t < 13) {
        float k = t < 4 ? 0 : (t - 4) / 9.0f;
        int lvl = 16 - (t > 4 ? (t - 4) * 3 : 0);
        if (lvl > 0) tk_dither_rect(0, 0, TW, TH, PAL_RAMP(RAMP_VAPE, 4), lvl);
        cloud_burst(cx, 100, 1.0f + k * k * 3.0f, 1.4f - k * 0.4f, 30 + t, 42);
        cloud_burst(cx, 100, 0.45f + k * k * 3.4f, 1.4f - k * 0.4f, 30 + t, 43);
        if (t < 6) {
            int ey = cyf - 55;
            for (int e = -1; e <= 1; e += 2) {
                int ex = cx + e * 11;
                tk_tri(ex - 6, ey - 3 - e * 0, ex + 6, ey - 3 + 0, ex, ey + 4, PAL_UI_RED_D);
                tk_tri(ex - 4, ey - 2, ex + 4, ey - 2, ex, ey + 2, PAL_UI_RED);
                tk_rect(ex - 1, ey - 2, 2, 1, PAL_RAMP(RAMP_LAVA, 7));
            }
        }
    }
    /* beat of silence, then: */
    if (t >= 13) {
        int a = t - 13;
        tk_caption(142 + (a < 3 ? 3 - a : 0), "Everything is fine.", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0), 2);
    }
}

void scene_s07_vape(int f, int n) {
    (void)n;
    if (f < CUT_B) {
        /* four hops in from the left: anticipation squash, stretch in air */
        float k = tk_clampf(f / 28.0f, 0, 1);
        int x = tk_lerpi(-14, 84, k * (2 - k));
        int ph = f % 7, yo = 0;
        float sx = 1, sy = 1;
        if (f < 28) {
            yo = -(int)(sinf(ph / 7.0f * 3.14159f) * 11);
            if (ph == 0) { sx = 1.2f; sy = 0.82f; }
            else if (ph < 3) { sx = 0.88f; sy = 1.14f; }
        } else if (f < 31) { sx = 1.16f; sy = 0.86f; }
        shot_wide(f, x, yo, sx, sy, f < 28 ? TKE_HAPPY : TKE_SPARKLE);
    } else if (f < CUT_C) {
        shot_vex(f - CUT_B);
    } else if (f < CUT_D) {
        shot_closeup(f - CUT_C);
    } else {
        shot_possessed(f - CUT_D);
    }
}

void scene_s07_vape_audio(double t0, double dur) {
#define AT(fr) (t0 + (fr) / 30.0)
    (void)dur;
    /* grand opening: shop bell, party horn */
    tk_note(AT(0), 0.25, 88, TKV_SINE, 0.25f);
    tk_note(AT(3), 0.35, 93, TKV_SINE, 0.2f);
    tk_slide(AT(4), 0.15, 72, 79, TKV_SQ25, 0.22f);
    tk_slide(AT(10), 0.30, 79, 86, TKV_SQ25, 0.22f);
    /* hops */
    for (int i = 0; i < 4; i++) tk_slide(AT(i * 7 + 1), 0.12, 62 + i * 2, 74 + i * 2, TKV_SQ50, 0.18f);
    tk_sfx(AT(29), SFX_SQUEAK, 0.35f);
    /* Vex: smooth, low babble while the text types (~23 frames) */
    {
        static const int m[] = { 50, 53, 48, 55, 52, 50, 57, 48, 53, 50, 47, 45 };
        for (int i = 0; i < 12; i++) tk_note(AT(CUT_B + 2 + i * 2), 0.055, m[i], TKV_SQ12, 0.2f);
    }
    tk_sfx(AT(CUT_B + 25), SFX_SPARKLE, 0.45f);       /* the wink */
    tk_sfx(AT(CUT_B + 27), SFX_WHOOSH, 0.45f);        /* pen slides */
    tk_note(AT(CUT_B + 31), 0.04, 96, TKV_SQ25, 0.12f); /* fine print: blip */
    tk_sfx(AT(CUT_B + 44), SFX_SELECT, 0.3f);         /* snatched  */
    /* inhale ... hold ... FWOOMP */
    tk_slide(AT(CUT_C + 4), 0.38, 50, 88, TKV_NOISE, 0.32f);
    tk_slide(AT(CUT_C + 15), 0.16, 80, 82, TKV_SQ12, 0.08f);  /* strained hum */
    tk_slide(AT(CUT_C + 20), 0.9, 92, 30, TKV_NOISE, 0.9f);
    tk_slide(AT(CUT_C + 20), 0.7, 40, 24, TKV_SINE, 0.85f);
    tk_sfx(AT(CUT_C + 20), SFX_EXPLOSION, 0.45f);
    /* possession: one wrong low chord, then SILENCE for the caption */
    tk_note(AT(CUT_D + 1), 0.45, 37, TKV_SAW, 0.22f);
    tk_note(AT(CUT_D + 1), 0.45, 38, TKV_SAW, 0.18f);
    tk_sfx(AT(CUT_D + 2), SFX_GLITCH, 0.3f);
    tk_note(AT(CUT_D + 13), 0.10, 84, TKV_SINE, 0.2f);
    tk_slide(AT(CUT_D + 26), 0.12, 34, 31, TKV_SQ12, 0.3f);  /* creeeak */
    tk_slide(AT(CUT_D + 28), 0.10, 31, 29, TKV_SQ12, 0.3f);
    tk_sfx(AT(CUT_D + 36), SFX_POP, 1.0f);             /* the balloon */
    tk_sfx(AT(CUT_D + 36), SFX_SPLASH, 0.2f);
#undef AT
}
