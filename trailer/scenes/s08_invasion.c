/* s08_invasion.c - the record scratch.   (6.0 s, 180 frames)
 *
 * f  0- 13  Hillvile Park party: bunting, confetti, everyone bouncing.
 *           (The possessed customer from s07 stands in the crowd, grinning.)
 * f 14      RECORD SCRATCH. Freeze frame. Everyone hangs in mid-air.
 * f 16- 25  "!" pops over every head. The Doomogachi: "beep."
 * f 28- 44  the sky CRACKS (glitch, shake). The sun is concerned.
 * f 36-100  the vape cloud billows up from behind the Wall and becomes
 *           LORD VAPULA. Eyes open (ROAR). His crown drops on. Crooked.
 *           The town rots into hell (tk_post_hellify 0 -> 16).
 * f 84-180  the frozen citizens fall down and flee; implings, puff demons
 *           and hellhound pups pour over the Wall and charge the camera.
 * f100/106  "THE DEMONS OF HELL" / "HAVE INVADED" slam in.
 * f138      "(wherever you are)"
 * f150-180  an impling pops up at the bottom of YOUR screen and waves. "hi."
 */
#include "tk.h"
#include <math.h>
#include <string.h>

#define F_SCRATCH 14
#define F_CRACK   28
#define F_CLOUD   36
#define F_EYES    68
#define F_CROWN   76
#define F_FLEE    76
#define F_CAP1    104
#define F_CAP2    110
#define F_CAP3    140
#define F_PEEK    150

/* ---- chibi capture for scaled blits ------------------------------------- */
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
static void cap_blit(const Cap *c, int x, int y, float sx, float sy, int flip) {
    const int ax = CAPW / 2, ay = CAPH - 10;
    int x0 = x - (int)(ax * sx) - 2, x1 = x + (int)((CAPW - ax) * sx) + 2;
    int y0 = y - (int)(ay * sy) - 2, y1 = y + (int)((CAPH - ay) * sy) + 2;
    for (int dy = y0; dy < y1; dy++)
        for (int dx = x0; dx < x1; dx++) {
            int v = ay + (int)floorf((dy - y) / sy + 0.5f);
            int u = ax + (int)floorf((dx - x) / sx + 0.5f);
            if (flip) u = 2 * ax - u;
            if (u < 0 || v < 0 || u >= CAPW || v >= CAPH) continue;
            uint8_t p = c->px[v][u];
            if (p) tk_px(dx, dy, p);
        }
}

/* ---- ASCII sprite with float scale, anchored bottom-centre --------------- */
static void spr_blit(const char *const *rows, const uint8_t *map, int x, int ybot, float sc, int flip) {
    int w = 0, h = 0;
    for (h = 0; rows[h]; h++) { int l = (int)strlen(rows[h]); if (l > w) w = l; }
    int dw = (int)(w * sc + 0.5f), dh = (int)(h * sc + 0.5f);
    int x0 = x - dw / 2, y0 = ybot - dh;
    for (int dy = 0; dy < dh; dy++)
        for (int dx = 0; dx < dw; dx++) {
            int sy = (int)(dy / sc), sx = (int)(dx / sc);
            if (sy >= h) continue;
            if (flip) sx = w - 1 - sx;
            if (sx < 0 || sx >= (int)strlen(rows[sy])) continue;
            unsigned ch = (unsigned char)rows[sy][sx];
            if (ch == '.' || ch == ' ' || ch >= 128) continue;
            uint8_t c = map[ch];
            if (c) tk_px(x0 + dx, y0 + dy, c);
        }
}

/* ---- fluffy cloud masses -------------------------------------------------- */
typedef struct { int x, y, r; } Puff;
static void cloud_mass(const Puff *p, int n, int dark) {
    int o = dark ? 1 : 0;
    for (int i = 0; i < n; i++) if (p[i].r > 0) tk_circle(p[i].x, p[i].y, p[i].r + 1, PAL_RAMP(RAMP_VAPE, 1 + o * 0));
    for (int i = 0; i < n; i++) if (p[i].r > 0) tk_circle(p[i].x, p[i].y, p[i].r, PAL_RAMP(RAMP_VAPE, 3 + 0 * o));
    for (int i = 0; i < n; i++) if (p[i].r > 2) tk_circle(p[i].x - p[i].r / 6, p[i].y - p[i].r / 5, p[i].r * 5 / 6, PAL_RAMP(RAMP_VAPE, 4));
    for (int i = 0; i < n; i++) if (p[i].r > 4) tk_circle(p[i].x - p[i].r / 3, p[i].y - p[i].r / 3, p[i].r / 2, PAL_RAMP(RAMP_VAPE, 5));
    for (int i = 0; i < n; i++) if (p[i].r > 9) tk_circle(p[i].x - p[i].r / 2, p[i].y - p[i].r / 2, p[i].r / 6, PAL_RAMP(RAMP_VAPE, 6));
}

/* ---- the cute town --------------------------------------------------------- */
#define WALL_TOP 52
#define GROUND   88
#define GATE_X   120

static void house(int x, int w, int y, uint8_t wallc, uint8_t roofc) {
    tk_rect(x, y, w, GROUND + 14 - y, PAL_BLACK);
    tk_rect(x + 1, y + 1, w - 2, GROUND + 12 - y, wallc);
    tk_tri(x - 4, y + 1, x + w + 3, y + 1, x + w / 2, y - 16, PAL_BLACK);
    tk_tri(x - 2, y, x + w + 1, y, x + w / 2, y - 14, roofc);
    tk_rect(x + w / 2 - 4, y + 12, 8, GROUND + 12 - y - 12, PAL_RAMP(RAMP_WOOD, 3));
    tk_rect(x + 4, y + 5, 6, 6, PAL_RAMP(RAMP_SKY, 6)); tk_frame(x + 4, y + 5, 6, 6, PAL_BLACK);
    tk_rect(x + w - 10, y + 5, 6, 6, PAL_RAMP(RAMP_SKY, 6)); tk_frame(x + w - 10, y + 5, 6, 6, PAL_BLACK);
    tk_heart(x + w / 2, y - 6, 1, PAL_PAINT(PAINT_CHERRY, 1));
}
static void tree(int x, int y, int r) {
    tk_rect(x - 2, y, 5, 16, PAL_RAMP(RAMP_WOOD, 2));
    tk_circle(x, y - r / 2, r + 1, PAL_BLACK);
    tk_circle(x, y - r / 2, r, PAL_RAMP(RAMP_GRASS, 4));
    tk_circle(x - r / 4, y - r / 2 - r / 4, r * 2 / 3, PAL_RAMP(RAMP_GRASS, 5));
    tk_circle(x - r / 3, y - r / 2 - r / 3, r / 4, PAL_RAMP(RAMP_GRASS, 6));
    tk_circle(x + r / 3, y - r / 3, 1, PAL_PAINT(PAINT_CHERRY, 1));
    tk_circle(x - r / 2, y - r / 6, 1, PAL_PAINT(PAINT_CHERRY, 1));
}

static void sun(int at, int scared) {
    int x = 28, y = 22;
    for (int i = 0; i < 8; i++) {
        float a = i * 0.785f + at * 0.05f;
        tk_line(x + (int)(cosf(a) * 12), y + (int)(sinf(a) * 12), x + (int)(cosf(a) * 16), y + (int)(sinf(a) * 16), PAL_RAMP(RAMP_CANDY, 5));
    }
    tk_circle(x, y, 10, PAL_BLACK);
    tk_circle(x, y, 9, PAL_RAMP(RAMP_CANDY, 5));
    tk_circle(x - 2, y - 2, 6, PAL_RAMP(RAMP_CANDY, 6));
    if (!scared) {
        tk_px(x - 4, y - 1, PAL_BLACK); tk_px(x - 3, y - 2, PAL_BLACK); tk_px(x - 2, y - 1, PAL_BLACK);
        tk_px(x + 2, y - 1, PAL_BLACK); tk_px(x + 3, y - 2, PAL_BLACK); tk_px(x + 4, y - 1, PAL_BLACK);
        tk_line(x - 3, y + 3, x - 1, y + 5, PAL_BLACK); tk_line(x - 1, y + 5, x + 1, y + 5, PAL_BLACK); tk_line(x + 1, y + 5, x + 3, y + 3, PAL_BLACK);
    } else {
        tk_rect(x - 4, y - 3, 2, 3, PAL_BLACK); tk_rect(x + 2, y - 3, 2, 3, PAL_BLACK);
        tk_ring(x, y + 4, 2, PAL_BLACK);
        tk_px(x + 7, y - 6, PAL_PAINT(PAINT_SKY, 1)); tk_px(x + 7, y - 5, PAL_PAINT(PAINT_SKY, 1)); tk_px(x + 8, y - 4, PAL_PAINT(PAINT_SKY, 2));
    }
    tk_px(x - 6, y + 2, PAL_PAINT(PAINT_BUBBLEGUM, 1)); tk_px(x + 6, y + 2, PAL_PAINT(PAINT_BUBBLEGUM, 1));
}

static void town(int at, int scared, int rot) {
    static const uint8_t g[] = { PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7),
                                 PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6) };
    tk_vgrad(0, 0, TW, GROUND, g, 5);
    for (int i = 0; i < 4; i++) {
        int cx = ((i * 71 + at / 2) % 280) - 20, cy = 10 + (i * 13) % 24;
        tk_ellipse(cx, cy, 11, 3, PAL_WHITE); tk_circle(cx - 3, cy - 2, 3, PAL_WHITE); tk_circle(cx + 4, cy - 3, 4, PAL_WHITE);
    }
    sun(at, scared);
    /* Hellvile spires peeking over the wall */
    for (int i = 0; i < 7; i++) {
        int x = 52 + i * 24 + (i * 7) % 9, h = 10 + (i * 37) % 18;
        tk_rect(x, WALL_TOP - h, 8, h, PAL_RAMP(RAMP_STONE, 1));
        tk_tri(x - 1, WALL_TOP - h, x + 8, WALL_TOP - h, x + 4, WALL_TOP - h - 8, PAL_RAMP(RAMP_STONE, 1));
        tk_px(x + 3, WALL_TOP - h + 4, PAL_UI_RED);
    }
    /* purple fog leaking over */
    for (int i = 0; i < 12; i++) {
        uint32_t s = tk_hash(i * 13 + 1);
        int x = tk_rand(&s, TW), r = 4 + tk_rand(&s, 5);
        tk_circle(x + (int)(sinf(at * 0.05f + i) * 3), WALL_TOP - 1, r, PAL_RAMP(RAMP_VAPE, 5));
    }
    /* the Wall */
    tk_rect(0, WALL_TOP, TW, GROUND - WALL_TOP, PAL_RAMP(RAMP_STONE, 3));
    for (int r = 0; r < 5; r++) {
        int y = WALL_TOP + 3 + r * 7;
        tk_rect(0, y, TW, 1, PAL_RAMP(RAMP_STONE, 2));
        for (int x = (r & 1) ? 5 : 0; x < TW; x += 11) { tk_rect(x, y + 1, 1, 6, PAL_RAMP(RAMP_STONE, 2)); tk_rect(x + 1, y + 1, 9, 1, PAL_RAMP(RAMP_STONE, 4)); }
    }
    for (int x = 0; x < TW; x += 10) { tk_rect(x, WALL_TOP - 5, 6, 5, PAL_RAMP(RAMP_STONE, 3)); tk_rect(x, WALL_TOP - 5, 6, 1, PAL_RAMP(RAMP_STONE, 5)); }
    tk_rect(0, WALL_TOP, TW, 1, PAL_RAMP(RAMP_STONE, 5));
    /* the sealed gate */
    tk_rect(GATE_X - 18, WALL_TOP - 8, 36, GROUND - WALL_TOP + 8, PAL_BLACK);
    tk_circle(GATE_X, WALL_TOP - 4, 17, PAL_BLACK);
    tk_rect(GATE_X - 16, WALL_TOP - 6, 32, GROUND - WALL_TOP + 6, PAL_RAMP(RAMP_WOOD, 1));
    tk_circle(GATE_X, WALL_TOP - 4, 15, PAL_RAMP(RAMP_WOOD, 1));
    for (int x = GATE_X - 12; x < GATE_X + 16; x += 6) tk_rect(x, WALL_TOP - 14, 1, GROUND - WALL_TOP + 14, PAL_RAMP(RAMP_WOOD, 0));
    tk_line(GATE_X - 16, WALL_TOP + 4, GATE_X + 16, WALL_TOP + 22, PAL_GRAY);
    tk_line(GATE_X + 16, WALL_TOP + 4, GATE_X - 16, WALL_TOP + 22, PAL_GRAY);
    tk_rect(GATE_X - 4, WALL_TOP + 10, 9, 8, PAL_UI_GOLD); tk_frame(GATE_X - 4, WALL_TOP + 10, 9, 8, PAL_BLACK);
    tk_ring(GATE_X, WALL_TOP + 9, 3, PAL_GRAY_D);
    /* houses + trees */
    house(2, 40, 72, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_CHERRY, 1));
    house(198, 40, 70, PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_SKY, 0));
    /* grass + path */
    tk_rect(0, GROUND, TW, TH - GROUND, PAL_RAMP(RAMP_GRASS, 5));
    tk_dither_rect(0, GROUND, TW, 6, PAL_RAMP(RAMP_GRASS, 4), 8);
    tk_rect(0, GROUND + 12, 44, 2, PAL_RAMP(RAMP_GRASS, 4));
    tk_rect(198, GROUND + 12, 42, 2, PAL_RAMP(RAMP_GRASS, 4));
    tk_tri(GATE_X - 14, GROUND, GATE_X + 14, GROUND, GATE_X + 50, TH, PAL_RAMP(RAMP_PEACH, 6));
    tk_tri(GATE_X - 14, GROUND, GATE_X - 50, TH, GATE_X + 50, TH, PAL_RAMP(RAMP_PEACH, 6));
    for (int i = 0; i < 40; i++) {
        uint32_t s = tk_hash(i * 7 + 3);
        int x = tk_rand(&s, TW), y = GROUND + 8 + tk_rand(&s, TH - GROUND - 8);
        if (x > GATE_X - 30 && x < GATE_X + 30) continue;
        static const uint8_t fl[] = { PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_LEMON, 2), PAL_WHITE, PAL_PAINT(PAINT_LAVENDER, 2) };
        tk_px(x, y, fl[i & 3]); tk_px(x - 1, y + 1, PAL_RAMP(RAMP_GRASS, 4)); tk_px(x + 1, y + 1, PAL_RAMP(RAMP_GRASS, 4));
    }
    /* the lawn dies */
    if (rot > 0) {
        tk_dither_rect(0, GROUND, TW, TH - GROUND, PAL_RAMP(RAMP_WOOD, 1), rot);
        tk_dither_rect(0, GROUND + 1, TW, TH - GROUND, PAL_RAMP(RAMP_BLOOD, 2), rot / 2);
    }
    tree(58, 98, 13);
    tree(186, 96, 12);
    if (rot > 0) {   /* ...and so do the trees */
        tk_dither_rect(42, 72, 32, 24, PAL_RAMP(RAMP_WOOD, 1), rot * 3 / 4);
        tk_dither_rect(171, 71, 30, 24, PAL_RAMP(RAMP_WOOD, 1), rot * 3 / 4);
    }
    /* bunting between the trees */
    for (int i = 0; i <= 20; i++) {
        float t = i / 20.0f;
        int x = (int)(62 + t * 120), y = 84 + (int)(sinf(t * 3.14159f) * 10 + sinf(at * 0.2f + t * 6) * 0.8f);
        tk_px(x, y, PAL_GRAY_D);
        if (i % 2 == 0 && i < 20) {
            static const uint8_t bc[] = { PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_SKY, 1) };
            tk_tri(x, y + 1, x + 5, y + 1, x + 2, y + 6, bc[(i / 2) & 3]);
        }
    }
}

/* ---- the crowd -------------------------------------------------------------- */
static const TkChibi CUSTOMER = {
    PAL_SKIN(SKIN_FAIR, 1), PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_LEMON, 1),
    PAL_PAINT(PAINT_TEAL, 1), TKH_BOB, 0, 0, 0, 0, 1, 1 };
typedef struct { int x, y, id, flip, phase, dir; } Cit;
static const Cit CROWD[] = {
    {  22, 124,  3, 0, 0, -1 }, {  50, 112,  7, 1, 3, -1 }, {  80, 140, 12, 0, 6, -1 },
    { 150, 118, 21, 0, 2,  1 }, { 166, 118, 22, 1, 2,  1 }, { 206, 134, 30, 1, 5,  1 },
    {  96, 112, 41, 0, 4, -1 }, { 186, 148, 44, 1, 1,  1 },
};
#define NCROWD (int)(sizeof CROWD / sizeof CROWD[0])

static int jump_h(int at, int ph) {
    int p = (at + ph * 3) % 12;
    return (int)(sinf(p / 12.0f * 3.14159f) * 9);
}

static void bang(int x, int y, int age) {
    if (age < 0) return;
    int s = age < 2 ? 2 : 1;
    int h = 9 * s;
    tk_rect(x - 3 * s, y - h - 2, 7 * s, h + 3, PAL_BLACK);
    tk_rect(x - 3 * s + 1, y - h - 1, 7 * s - 2, h + 1, PAL_WHITE);
    tk_text(x - 2 * s, y - h, "!", PAL_UI_RED, s);
}

/* the hero, holding the Doomogachi */
static void hero(int f, int frozen, int fleeing) {
    int x = 214, yf = 166;
    if (fleeing) x += (f - F_FLEE) * 4;
    int expr = TKE_HAPPY, mouth = TKM_OPEN;
    if (f >= F_SCRATCH + 3) { expr = TKE_SHOCK; mouth = TKM_O; }
    if (fleeing) { expr = TKE_SHOCK; mouth = TKM_SCREAM; }
    int bob = frozen ? F_SCRATCH : f * (fleeing ? 3 : 1);
    tk_chibi(x, yf, &TK_HERO, 2, expr, mouth, bob, 1);
    /* the Doomogachi (egg-shaped LCD toy) in their hands */
    int dx = x - 2, dy = yf - 30;
    tk_ellipse(dx, dy, 7, 8, PAL_BLACK);
    tk_ellipse(dx, dy, 6, 7, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    tk_rect(dx - 4, dy - 4, 9, 7, PAL_BLACK);
    tk_rect(dx - 3, dy - 3, 7, 5, PAL_UI_LCD);
    int blink = (f / 4) & 1;
    tk_px(dx - 1, dy - 2, PAL_BLACK); tk_px(dx + 1, dy - 2, PAL_BLACK);
    if (!blink || f < F_SCRATCH) { tk_px(dx, dy, PAL_BLACK); }
    tk_px(dx - 3, dy + 5, PAL_PAINT(PAINT_CHERRY, 1)); tk_px(dx, dy + 5, PAL_PAINT(PAINT_LEMON, 1)); tk_px(dx + 3, dy + 5, PAL_PAINT(PAINT_MINT, 1));
    /* "beep." */
    if (f >= 20 && f < 46) {
        int bx = dx - 44, by = dy - 26;
        tk_window(bx, by, 34, 15, TKW_LCD);
        tk_tri(bx + 26, by + 14, bx + 32, by + 14, bx + 36, by + 20, PAL_GRAY_D);
        tk_text(bx + 4, by + 4, "beep.", PAL_BLACK, 1);
    }
}

/* ---- the demons ------------------------------------------------------------- */
static const char *const HOUND[] = {
    "kk.........kk",
    "kEk.......kEk",
    "kEEkkkkkkkEEk",
    "kEEEEEEEEEEEk",
    "kErrEEEEErrEk",
    "kErwEEEEEwrEk",
    ".kEEEEkEEEEk.",
    ".kEEkwkwkEEk.",
    "..kxkxkxkxk..",
    "..kEEEEEEEk..",
    ".kEEkEEEkEEk.",
    ".kk.kk.kk.kk.",
    NULL };
static const char *const HOUND2[] = {
    "kk.........kk",
    "kEk.......kEk",
    "kEEkkkkkkkEEk",
    "kEEEEEEEEEEEk",
    "kErrEEEEErrEk",
    "kErwEEEEEwrEk",
    ".kEEEEkEEEEk.",
    ".kEEkpppkEEk.",
    "..kxkxpxkxk..",
    "..kEEEEEEEk..",
    "..kEkEEEkEk..",
    "..kk.kkk.kk..",
    NULL };
static uint8_t hound_map[128];

static void puff_demon(int x, int ybot, float sc, int f) {
    int r = (int)(7 * sc + 0.5f);
    if (r < 2) r = 2;
    int y = ybot - r;
    /* horns */
    tk_tri(x - r + 1, y - r / 2, x - r / 2, y - r + 1, x - r - 1, y - r - 2 - r / 3, PAL_BLACK);
    tk_tri(x + r - 1, y - r / 2, x + r / 2, y - r + 1, x + r + 1, y - r - 2 - r / 3, PAL_BLACK);
    tk_tri(x - r + 2, y - r / 2, x - r / 2, y - r + 2, x - r, y - r - r / 3, PAL_RAMP(RAMP_BONE, 6));
    tk_tri(x + r - 2, y - r / 2, x + r / 2, y - r + 2, x + r, y - r - r / 3, PAL_RAMP(RAMP_BONE, 6));
    Puff p[4] = { { x - r / 2, y + r / 4, r * 2 / 3 }, { x + r / 2, y + r / 4, r * 2 / 3 }, { x, y, r }, { x, y - r / 3, r * 3 / 4 } };
    cloud_mass(p, 4, 0);
    /* angry eyes + fangs */
    int e = r / 3 > 0 ? r / 3 : 1;
    tk_rect(x - e * 2, y - e, e, e, PAL_UI_RED); tk_rect(x + e, y - e, e, e, PAL_UI_RED);
    tk_line(x - e * 2 - 1, y - e - 2, x - e + 1, y - e - 1, PAL_BLACK);
    tk_line(x + e * 2 + 1, y - e - 2, x + e - 1, y - e - 1, PAL_BLACK);
    tk_rect(x - e * 2, y + e / 2 + 1, e * 4 + 1, e > 1 ? e : 1, PAL_BLACK);
    tk_px(x - e, y + e / 2 + 1 + (e > 1 ? e : 1), PAL_WHITE); tk_px(x + e, y + e / 2 + 1 + (e > 1 ? e : 1), PAL_WHITE);
    (void)f;
}

typedef struct { int t0, type, lx, side; } Demon;
#define NDEMONS 26
static Demon demon_get(int i) {
    uint32_t s = tk_hash((uint32_t)i * 2246822519u + 17u);
    Demon d;
    d.t0 = 70 + i * 3 + tk_rand(&s, 2);
    d.type = (i * 2 + tk_rand(&s, 2)) % 3;
    d.lx = 24 + tk_rand(&s, 192);
    d.side = d.lx < GATE_X - 10 ? -1 : (d.lx > GATE_X + 10 ? 1 : 0);
    return d;
}

/* where a demon is at frame f: leap out of Vapula's mouth, over the wall,
 * land in the park, then charge the camera */
static int demon_pos(const Demon *d, int f, int my, float *x, float *y, float *sc) {
    int a = f - d->t0;
    if (a < 0) return 0;
    const int fly = 15;
    if (a < fly) {
        float k = a / (float)fly;
        *x = GATE_X + (d->lx - GATE_X) * k;
        *y = my + (GROUND + 8 - my) * k - sinf(k * 3.14159f) * 22;
        *sc = 0.35f + 0.35f * k;
        return 1;
    }
    float k = (a - fly) / 58.0f;
    *x = d->lx + d->side * k * k * 50 + (d->lx - GATE_X) * k * 0.5f;
    *y = GROUND + 8 + k * k * 86 + k * 14;
    *sc = 0.7f + k * k * 1.8f + k * 0.3f;
    return *y < TH + 50;
}

/* ---- Lord Vapula -------------------------------------------------------- */
#define VX GATE_X
#define VY 46
#define VS 0.82f
#define MOUTH_Y (VY + (int)(18 * VS))
static void vapula(int f) {
    int a = f - F_CLOUD;
    if (a < 0) return;
    const int cx = VX, cy = VY;
    const float S = VS;
    static const signed char P[][3] = {
        {   0, -10, 26 }, { -26,  -4, 18 }, {  26,  -4, 18 }, { -42,   8, 14 }, {  42,   8, 14 },
        { -20,  14, 20 }, {  20,  14, 20 }, {   0,  18, 22 }, { -48,  -8, 10 }, {  48,  -8, 10 },
        { -12, -28, 14 }, {  14, -26, 13 }, { -34, -18, 12 }, {  34, -18, 12 }, {   0,  32, 12 },
        { -30,  28, 12 }, {  30,  28, 12 }, {  -6,  40, 10 }, {  6,  46, 8 },
    };
    const int np = (int)(sizeof P / sizeof P[0]);
    Puff p[24];
    for (int i = 0; i < np; i++) {
        float k = tk_clampf((a - i * 1.3f) / 24.0f, 0, 1);
        float e = 1 - (1 - k) * (1 - k);
        int wob = (int)(sinf(f * 0.15f + i) * 1.5f);
        int tx = cx + (int)(P[i][0] * S) + wob, ty = cy + (int)(P[i][1] * S);
        int sx = GATE_X + P[i][0] / 4, sy = WALL_TOP + 6;
        p[i] = (Puff){ (int)(sx + (tx - sx) * e), (int)(sy + (ty - sy) * e), (int)(P[i][2] * S * (0.2f + 0.8f * e)) };
    }
    /* the stem of vapour pouring out of the gate */
    Puff stem[5];
    for (int i = 0; i < 5; i++) {
        int y = GROUND - 6 - i * 8 - (f % 8);
        stem[i] = (Puff){ GATE_X + (int)(sinf(f * 0.3f + i) * 3), y, 8 + (i & 1) * 2 };
    }
    cloud_mass(stem, a > 4 ? 5 : 0, 0);
    cloud_mass(p, np, 0);
    if (a < 24) return;
    /* face */
    int ex = (int)(18 * S), ey = cy + 2;
    int open = f < F_EYES ? 0 : (f < F_EYES + 4 ? (f - F_EYES + 1) : 5);
    for (int s = -1; s <= 1; s += 2) {
        int x = cx + s * ex;
        if (!open) { tk_line(x - 6, ey, x + 6, ey, PAL_RAMP(RAMP_VAPE, 1)); tk_line(x - 6, ey - 1, x + 6, ey - 1, PAL_RAMP(RAMP_VAPE, 1)); continue; }
        int h = open;
        tk_tri(x - 9 * s, ey - h - 2, x + 8 * s, ey - 1, x - 9 * s, ey + h, PAL_RAMP(RAMP_VAPE, 0));
        tk_tri(x - 7 * s, ey - h, x + 5 * s, ey, x - 7 * s, ey + h - 1, PAL_UI_RED);
        tk_rect(x - 4 * s - (s > 0 ? 1 : 0), ey - 1, 2, 2, PAL_RAMP(RAMP_LAVA, 7));
    }
    for (int d = 0; d < 2; d++) {   /* furious brows */
        tk_line(cx - 25, ey - 9 + d, cx - 7, ey - 4 + d, PAL_RAMP(RAMP_VAPE, 0));
        tk_line(cx + 25, ey - 9 + d, cx + 7, ey - 4 + d, PAL_RAMP(RAMP_VAPE, 0));
    }
    /* the grin: opens with the roar */
    int mo = f < F_EYES ? 1 : (f < F_EYES + 5 ? 1 + (f - F_EYES) * 2 : 9);
    int my = MOUTH_Y, mw = 20;
    tk_ellipse(cx, my, mw, mo, PAL_RAMP(RAMP_VAPE, 0));
    tk_rect(cx - mw, my - mo, mw * 2 + 1, mo, PAL_RAMP(RAMP_VAPE, 0));
    if (mo > 2) {
        tk_ellipse(cx, my + mo / 2, 11, mo / 2, PAL_UI_RED_D);
        for (int i = -16; i <= 16; i += 8) tk_tri(cx + i - 3, my - mo, cx + i + 3, my - mo, cx + i, my - mo + 5, PAL_WHITE);
        for (int i = -12; i <= 12; i += 8) tk_tri(cx + i - 2, my + mo - 1, cx + i + 2, my + mo - 1, cx + i, my + mo - 5, PAL_WHITE);
    }
    /* the crown drops on. it's tiny. and crooked. */
    if (f >= F_CROWN) {
        int b = f - F_CROWN;
        int drop = b < 6 ? (6 - b) * (6 - b) * 2 : (b < 10 ? -(int)(sinf((b - 6) / 4.0f * 3.14159f) * 3) : 0);
        int kx = cx + 12, ky = cy - 24 - drop;
        int tilt = b >= 6 ? 2 : 0;
        uint8_t gd = PAL_UI_GOLD, gk = PAL_RAMP(RAMP_CANDY, 2);
        tk_rect(kx - 9, ky + tilt - 1, 9, 7, PAL_BLACK); tk_rect(kx, ky - 1, 10, 7, PAL_BLACK);
        tk_rect(kx - 8, ky + tilt, 8, 5, gd); tk_rect(kx, ky, 9, 5, gd);
        tk_rect(kx - 8, ky + 4 + tilt, 8, 1, gk); tk_rect(kx, ky + 4, 9, 1, gk);
        for (int i = 0; i < 3; i++) {
            int px = kx - 6 + i * 6, py = (i < 1 ? ky + tilt : ky);
            tk_tri(px - 3, py, px + 3, py, px, py - 7, PAL_BLACK);
            tk_tri(px - 2, py, px + 2, py, px, py - 5, gd);
            tk_px(px, py - 6, PAL_UI_RED);
        }
        tk_rect(kx - 5, ky + 1 + tilt, 2, 2, PAL_UI_RED); tk_rect(kx + 4, ky + 1, 2, 2, PAL_PAINT(PAINT_MINT, 1));
        if (b >= 6 && b < 16) tk_sparkles(kx, ky, b, 9, 12);
    }
}

/* ---- the sky crack ----------------------------------------------------- */
static void crack_seg(int x0, int y0, int x1, int y1, int w) {
    for (int t = -w - 1; t <= w + 1; t++) { tk_line(x0, y0 + t, x1, y1 + t, PAL_BLACK); tk_line(x0 + t, y0, x1 + t, y1, PAL_BLACK); }
    for (int t = -w; t <= w; t++) tk_line(x0, y0 + t, x1, y1 + t, PAL_RAMP(RAMP_LAVA, 4));
    if (w > 0) tk_line(x0, y0, x1, y1, PAL_RAMP(RAMP_LAVA, 7));
}
static void crack(int f) {
    int a = f - F_CRACK;
    if (a < 0) return;
    /* main bolt across the sky + side branches */
    static const signed char M[][2] = { { -120, 14 }, { -96, 6 }, { -84, 16 }, { -60, 4 }, { -44, 12 }, { -24, -2 },
                                        { -8, 8 }, { 10, -4 }, { 28, 6 }, { 46, -6 }, { 66, 4 }, { 84, -8 }, { 104, 2 }, { 120, -6 } };
    static const signed char B[][4] = { { -60, 4, -66, -10 }, { -24, -2, -16, -16 }, { 28, 6, 22, 20 }, { 66, 4, 74, -8 },
                                        { -84, 16, -90, 26 }, { 104, 2, 110, 14 } };
    int n = 14, mid = 7;
    int reach = a * 2 + 1;               /* segments grow from the middle */
    int w = a < 3 ? 0 : (a < 10 ? 1 : 2);
    int cy = 30;
    if (w >= 1) tk_dither_rect(0, cy - 14, TW, 32, PAL_RAMP(RAMP_LAVA, 3), a < 16 ? a / 2 : 8);
    for (int i = 0; i < n - 1; i++) {
        int d = i < mid ? mid - 1 - i : i - mid;
        if (d >= reach) continue;
        crack_seg(GATE_X + M[i][0], cy + M[i][1], GATE_X + M[i + 1][0], cy + M[i + 1][1], w);
    }
    if (a >= 4)
        for (int i = 0; i < 6; i++) crack_seg(GATE_X + B[i][0], cy + B[i][1], GATE_X + B[i][2], cy + B[i][3], a > 10 ? 1 : 0);
}

void scene_s08_invasion(int f, int n) {
    (void)n;
    if (!hound_map['k']) {
        hound_map['k'] = PAL_BLACK; hound_map['E'] = PAL_PAINT(PAINT_CHARCOAL, 1);
        hound_map['r'] = PAL_UI_RED; hound_map['w'] = PAL_WHITE; hound_map['x'] = PAL_RAMP(RAMP_BONE, 6);
        hound_map['p'] = PAL_PAINT(PAINT_ROSE, 1);
    }
    int frozen = f >= F_SCRATCH && f < F_FLEE;
    int at = f < F_SCRATCH ? f : F_SCRATCH;     /* town animation clock (stops) */
    if (f >= F_FLEE) at = F_SCRATCH + (f - F_FLEE);
    /* --- the town (gets hellified) --- */
    int hell = f < F_CLOUD ? 0 : (f - F_CLOUD) * 16 / 48;
    if (hell > 16) hell = 16;
    town(at, f >= F_CRACK, hell * 3 / 4);
    crack(f);
    tk_post_hellify(hell);
    if (f >= F_SCRATCH && f < F_CLOUD) tk_post_darken(13);
    /* --- Vapula rises (stays vapour-purple) --- */
    vapula(f);
    /* --- crowd --- */
    for (int i = 0; i < NCROWD; i++) {
        const Cit *c = &CROWD[i];
        TkChibi ch = tk_random_citizen((uint32_t)c->id, 0);
        int x = c->x, y = c->y, flip = c->flip;
        int expr = TKE_HAPPY, mouth = TKM_OPEN, bob = at + c->phase;
        int jy = jump_h(f < F_SCRATCH ? f : F_SCRATCH, c->phase);
        if (f >= F_SCRATCH + 2 + i % 3) { expr = TKE_SHOCK; mouth = TKM_O; }
        if (f >= F_FLEE) {
            int a = f - F_FLEE;
            /* drop out of the frozen mid-air pose, then run */
            int fall = jy - a * a;
            jy = fall > 0 ? fall : 0;
            int run = a > 3 ? (a - 3) : 0;
            x += c->dir * (run * 3 + run * run / 12);
            flip = c->dir > 0 ? 0 : 1;
            mouth = TKM_SCREAM; bob = f * 3;
            if (run > 0) jy += (f & 2) ? 1 : 0;
        }
        tk_chibi(x, y - jy, &ch, 1, expr, mouth, bob, flip);
        if (f >= F_FLEE && ((f / 3 + i) & 1)) { tk_px(x + 7, y - 26, PAL_PAINT(PAINT_SKY, 2)); tk_px(x + 8, y - 25, PAL_PAINT(PAINT_SKY, 1)); }
        if (f >= F_SCRATCH + 2 + i % 3 && f < F_FLEE + 6) bang(x, y - jy - 30, f - F_SCRATCH - 2 - i % 3);
    }
    /* the possessed customer: unbothered */
    {
        int x = 128, y = 116;
        tk_chibi(x, y, &CUSTOMER, 1, TKE_POSSESSED, TKM_FANGS, 0, 0);
    }
    /* --- demons pour out (sorted back to front by y) --- */
    if (f >= demon_get(0).t0) {
        Cap imp[2];
        for (int k = 0; k < 2; k++) {
            cap_begin();
            tk_chibi(CAP_X, CAP_Y, &TK_IMP, 2, TKE_ANGRY, TKM_FANGS, k * 4, 0);
            cap_end(&imp[k]);
        }
        int order[NDEMONS];
        float xs[NDEMONS], ys[NDEMONS], ss[NDEMONS];
        Demon dm[NDEMONS];
        int m = 0;
        for (int i = 0; i < NDEMONS; i++) {
            dm[i] = demon_get(i);
            if (demon_pos(&dm[i], f, MOUTH_Y, &xs[i], &ys[i], &ss[i])) order[m++] = i;
        }
        for (int i = 1; i < m; i++)
            for (int j = i; j > 0 && ys[order[j]] < ys[order[j - 1]]; j--) { int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; }
        for (int q = 0; q < m; q++) {
            int i = order[q];
            const Demon *d = &dm[i];
            int x = (int)xs[i], y = (int)ys[i];
            float sc = ss[i];
            int flip = d->side < 0 || (d->side == 0 && ((f / 6) & 1));
            int step = ((f + i) / 3) & 1;
            /* shadow */
            if (f - d->t0 >= 14) tk_dither_rect(x - (int)(8 * sc), y - (int)(1 * sc), (int)(16 * sc), (int)(2 * sc) + 1, PAL_BLACK, 8);
            if (d->type == 0) cap_blit(&imp[step], x, y + (step ? 0 : -1), sc * 0.5f, sc * 0.5f, flip);
            else if (d->type == 1) puff_demon(x, y - (step ? 1 : 0), sc * 0.9f, f);
            else spr_blit(step ? HOUND2 : HOUND, hound_map, x, y, sc * 0.95f, flip);
        }
    }
    /* callback: the possessed customer is fine. everything is fine. */
    if (f >= F_FLEE + 8 && f < F_CAP1) {
        int x = 128, y = 116;
        tk_window(x + 8, y - 46, 68, 15, TKW_CREAM);
        tk_tri(x + 10, y - 33, x + 17, y - 33, x + 5, y - 27, PAL_UI_PINK_D);
        tk_text(x + 13, y - 42, "it's fine.", PAL_GRAY_D, 1);
    }
    /* --- the hero + Doomogachi (foreground) --- */
    hero(f, frozen, f >= F_FLEE + 6);
    /* --- confetti during the party, frozen mid-air after the scratch --- */
    {
        int cf = f < F_SCRATCH ? f : F_SCRATCH;
        for (int i = 0; i < 26; i++) {
            uint32_t s = tk_hash(i * 97 + 5);
            static const uint8_t cc[] = { PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_MINT, 1),
                                          PAL_PAINT(PAINT_SKY, 1), PAL_WHITE };
            int x = tk_rand(&s, TW) + (int)(sinf(cf * 0.2f + i) * 3), y = (tk_rand(&s, 120) + cf * 2) % 130 + 10;
            if (f >= F_FLEE) y += (f - F_FLEE) * 3;
            if (y < TH) tk_rect(x, y, 2, 1 + ((cf + i) & 1), cc[i % 5]);
        }
    }
    /* --- captions --- */
    if (f >= F_CAP1) {
        int a = f - F_CAP1;
        int y1 = 104 - (a < 3 ? (3 - a) * 4 : 0);
        tk_caption(y1, "THE DEMONS OF HELL", PAL_RAMP(RAMP_LAVA, 6), PAL_BLACK, 2);
    }
    if (f >= F_CAP2) {
        int a = f - F_CAP2;
        int y2 = 124 - (a < 3 ? (3 - a) * 4 : 0);
        tk_caption(y2, "HAVE INVADED", PAL_UI_RED, PAL_BLACK, 2);
    }
    if (f >= F_CAP3) tk_caption(146, "(wherever you are)", PAL_WHITE, PAL_BLACK, 1);
    /* --- the fourth-wall impling: peeks up over the bottom edge of YOUR screen --- */
    if (f >= F_PEEK) {
        int a = f - F_PEEK;
        float k = tk_bounce(a / 10.0f);
        int x = 220, yf = 240 - (int)(k * 42);
        tk_chibi(x, yf, &TK_IMP, 3, a >= 12 && a < 16 ? TKE_HAPPY : TKE_NORMAL, TKM_FANGS, 0, 1);
        if (a >= 8) {   /* little waving hand on the screen edge */
            int wv = (a / 4) & 1;
            int hx = x - 34 + (wv ? -1 : 1), hy = yf - 72;
            tk_rect(hx - 1, hy - 1, 9, 10, PAL_BLACK);
            tk_rect(hx, hy, 7, 8, PAL_PAINT(PAINT_CHERRY, 1));
            for (int i = 0; i < 3; i++) {
                tk_rect(hx + i * 3 - 1 + (wv ? -1 : 1) * (i - 1), hy - 4, 3, 5, PAL_BLACK);
                tk_rect(hx + i * 3 + (wv ? -1 : 1) * (i - 1), hy - 3, 1, 4, PAL_PAINT(PAINT_CHERRY, 1));
            }
            tk_rect(hx + 1, hy + 1, 2, 1, PAL_PAINT(PAINT_CHERRY, 2));
        }
        if (a >= 12) {
            tk_window(176, 136, 24, 15, TKW_CREAM);
            tk_text(181, 139, "hi.", PAL_BLACK, 1);
        }
    }
    /* --- post: shakes + glitches --- */
    if (f == F_SCRATCH) tk_post_flash(PAL_WHITE, 9);
    if (f >= F_CRACK && f < F_CRACK + 3) { tk_post_glitch(f, 30); tk_post_shake((f & 1) ? 2 : -2, 1); }
    if (f >= F_EYES && f < F_EYES + 18) {
        int m = 4 - (f - F_EYES) / 5;
        tk_post_shake((f & 1) ? m : -m, (f & 2) ? m / 2 : -m / 2);
        if (f < F_EYES + 3) tk_post_glitch(f * 3, 35);
    }
    if (f >= F_CAP1 && f < F_CAP1 + 4) tk_post_shake(0, (f & 1) ? 3 : -2);
    if (f >= F_CAP2 && f < F_CAP2 + 4) tk_post_shake((f & 1) ? 3 : -3, 0);
    if (f == F_CAP1 || f == F_CAP2) tk_post_flash(PAL_WHITE, 4);
    if (f > F_CAP2 && (f * 7) % 23 == 0) tk_post_glitch(f, 12);
}

void scene_s08_invasion_audio(double t0, double dur) {
#define AT(fr) (t0 + (fr) / 30.0)
    (void)dur;
    /* party: a little toot */
    tk_slide(AT(2), 0.12, 79, 84, TKV_SQ25, 0.2f);
    tk_slide(AT(7), 0.12, 79, 86, TKV_SQ25, 0.2f);
    /* SCRRRATCH */
    tk_sfx(AT(F_SCRATCH), SFX_SCRATCH, 1.0f);
    /* "!" blips, staggered */
    for (int i = 0; i < 3; i++) tk_note(AT(F_SCRATCH + 2 + i), 0.05, 88 + i * 3, TKV_SQ25, 0.25f);
    tk_sfx(AT(20), SFX_BEEP, 0.6f);                  /* beep. */
    /* the sky cracks */
    tk_sfx(AT(F_CRACK), SFX_GLITCH, 0.7f);
    tk_sfx(AT(F_CRACK), SFX_THUD, 0.9f);
    tk_slide(AT(F_CRACK), 0.3, 100, 70, TKV_NOISE, 0.4f);
    /* rumble as the cloud rises */
    tk_slide(AT(F_CLOUD), 1.1, 28, 34, TKV_NOISE, 0.55f);
    tk_slide(AT(F_CLOUD), 1.1, 26, 31, TKV_SAW, 0.25f);
    /* the ROAR */
    tk_sfx(AT(F_EYES), SFX_ROAR, 1.0f);
    tk_sfx(AT(F_EYES), SFX_EXPLOSION, 0.5f);
    tk_sfx(AT(F_EYES + 2), SFX_GLITCH, 0.4f);
    /* crown: clink */
    tk_note(AT(F_CROWN + 6), 0.12, 96, TKV_SINE, 0.35f);
    tk_note(AT(F_CROWN + 8), 0.2, 103, TKV_SINE, 0.25f);
    /* citizens scream (tiny high squeals) */
    for (int i = 0; i < 5; i++) tk_slide(AT(F_FLEE + 2 + i * 3), 0.18, 86 + (i * 5) % 9, 94 + (i * 3) % 7, TKV_SQ12, 0.12f);
    /* demons popping over the wall */
    for (int i = 0; i < NDEMONS; i += 2) {
        Demon d = demon_get(i);
        tk_slide(AT(d.t0), 0.08, 50 + (i * 7) % 12, 62 + (i * 5) % 12, TKV_SQ50, 0.1f);
        if (i % 4 == 2) tk_sfx(AT(d.t0 + 15), SFX_THUD, 0.2f);
    }
    /* caption slams */
    tk_sfx(AT(F_CAP1), SFX_THUD, 1.0f);
    tk_sfx(AT(F_CAP1), SFX_EXPLOSION, 0.6f);
    tk_sfx(AT(F_CAP2), SFX_THUD, 1.0f);
    tk_sfx(AT(F_CAP2), SFX_SHOTGUN, 0.5f);
    tk_sfx(AT(F_CAP3), SFX_BLIP, 0.4f);
    tk_note(AT(F_FLEE + 8), 0.08, 84, TKV_SINE, 0.2f);   /* it's fine. */
    /* the impling says hi */
    tk_sfx(AT(F_PEEK + 2), SFX_BOING, 0.4f);
    tk_sfx(AT(F_PEEK + 10), SFX_SQUEAK, 0.45f);
#undef AT
}
