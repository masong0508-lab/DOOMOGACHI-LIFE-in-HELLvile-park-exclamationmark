/* s09_weld.c - Rivet's workshop.   (4.5 s, 135 frames)
 *
 * f  0- 65  Rivet (welding mask down) welds a gun together in a jig. Parts
 *           fly in and CLANK into place: barrel, stock, magazine, scope,
 *           heart charm. Sparks everywhere. "WELD YOUR WEAPON." slams in.
 *           Gag: a spark lights Rivet's tail; the "DAYS SINCE LAST FIRE"
 *           sign flips 7 -> 0. Rivet flips the mask up: thumbs up. (Still on fire.)
 * f 66-134  split screen face-off: CUTE (pastel PLUSH LAUNCHER, stickers,
 *           teddy ammo peeking out, teddy charm) vs BRUTAL (gunmetal SUPER
 *           SHOTGUN, spikes, skull charm). The charms swing in sync.
 *           "(9 part slots. Millions of guns.)"  The teddy notices the
 *           shotgun. Sweats.
 */
#include "tk.h"
#include <math.h>
#include <string.h>

#define F_SPLIT 66

/* chibi pixel -> screen for a flipped chibi (mirror about cx 9) */
static void cpixf(int x, int yf, int s, int cx, int cy, uint8_t c) {
    tk_rect(x + (9 - cx) * s - s, yf + (cy - 27) * s, s, s, c);
}

static const uint8_t BAY[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
/* dithered round glow, denser in the middle */
static void glow(int cx, int cy, int r, uint8_t c, int level) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            int d2 = x * x + y * y;
            if (d2 > r * r) continue;
            int lv = level * (r * r - d2) / (r * r) + 1;
            int px = cx + x, py = cy + y;
            if ((unsigned)px < TW && (unsigned)py < TH && BAY[py & 3][px & 3] < lv) tk_fb[py][px] = c;
        }
}
/* dithered light cone from (x, y0) down to y1 */
static void cone(int x, int y0, int y1, int w0, int w1, uint8_t c, int level) {
    for (int y = y0; y < y1; y++) {
        int w = w0 + (w1 - w0) * (y - y0) / (y1 - y0);
        for (int i = x - w; i <= x + w; i++)
            if ((unsigned)i < TW && (unsigned)y < TH && BAY[y & 3][i & 3] < level) tk_fb[y][i] = c;
    }
}

/* ---- sparks ---------------------------------------------------------------- */
/* a shower of welding sparks born continuously from (ox,oy) between frames
 * [b0,b1), bouncing on the floor line `floor_y` */
static void sparks(int ox, int oy, int f, int b0, int b1, int floor_y, uint32_t seed, int rate) {
    static const uint8_t col[] = { PAL_WHITE, PAL_RAMP(RAMP_LAVA, 7), PAL_RAMP(RAMP_LAVA, 6),
                                   PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_LAVA, 4), PAL_RAMP(RAMP_LAVA, 3) };
    for (int b = b0; b < b1 && b <= f; b++)
        for (int i = 0; i < rate; i++) {
            uint32_t s = tk_hash(seed + (uint32_t)b * 131u + (uint32_t)i * 7u);
            int age = f - b, life = 8 + tk_rand(&s, 10);
            if (age > life) continue;
            float a = -3.14159f * (0.05f + tk_rand(&s, 900) / 1000.0f);
            float v = 2.0f + tk_rand(&s, 300) / 100.0f;
            float vx = cosf(a) * v, vy = sinf(a) * v - 0.5f;
            float g = 0.32f;
            float x1 = ox + vx * age, y1 = oy + vy * age + 0.5f * g * age * age;
            float x0 = ox + vx * (age - 1), y0 = oy + vy * (age - 1) + 0.5f * g * (age - 1) * (age - 1);
            if (age == 0) { x0 = (float)ox; y0 = (float)oy; }
            if (y1 > floor_y) { y1 = floor_y - (y1 - floor_y) * 0.3f; }
            if (y0 > floor_y) { y0 = floor_y - (y0 - floor_y) * 0.3f; }
            int ci = age * 6 / (life + 1);
            tk_line((int)x0, (int)y0, (int)x1, (int)y1, col[ci]);
        }
}

/* ---- the workshop ----------------------------------------------------------- */
static void flame(int x, int y, int f, int sz) {
    int fl = (f / 2) & 1;
    tk_tri(x - sz, y, x + sz, y, x + (fl ? 1 : -1), y - sz * 3, PAL_RAMP(RAMP_LAVA, 4));
    tk_tri(x - sz + 1, y, x + sz - 1, y, x + (fl ? -1 : 1), y - sz * 2, PAL_RAMP(RAMP_LAVA, 6));
    tk_px(x, y - 1, PAL_RAMP(RAMP_LAVA, 7));
}

static void workshop_bg(int f, int sign_digit, int flip_age) {
    /* plank wall */
    tk_rect(0, 0, TW, 160, PAL_RAMP(RAMP_WOOD, 2));
    for (int x = 0; x < TW; x += 16) {
        tk_rect(x, 0, 1, 160, PAL_RAMP(RAMP_WOOD, 1));
        tk_rect(x + 1, 0, 1, 160, PAL_RAMP(RAMP_WOOD, 3));
        for (int y = (x * 7) % 40; y < 160; y += 47) tk_px(x + 8, y, PAL_RAMP(RAMP_WOOD, 1));
    }
    /* window with the (now hellish) sky and a certain cloud */
    {
        int x = 10, y = 8, w = 52, h = 36;
        tk_rect(x - 2, y - 2, w + 4, h + 4, PAL_RAMP(RAMP_WOOD, 0));
        static const uint8_t g[] = { PAL_RAMP(RAMP_BLOOD, 3), PAL_RAMP(RAMP_LAVA, 3), PAL_RAMP(RAMP_LAVA, 5) };
        tk_vgrad(x, y, w, h, g, 3);
        tk_circle(x + 30, y + 18, 9, PAL_RAMP(RAMP_VAPE, 3));
        tk_circle(x + 22, y + 21, 6, PAL_RAMP(RAMP_VAPE, 3));
        tk_circle(x + 38, y + 21, 6, PAL_RAMP(RAMP_VAPE, 3));
        tk_px(x + 27, y + 18, PAL_UI_RED); tk_px(x + 33, y + 18, PAL_UI_RED);
        tk_rect(x + 28, y + 7, 5, 2, PAL_UI_GOLD);
        for (int i = 0; i < 3; i++) tk_rect(x, y + 28 + i * 2, w, 1, PAL_RAMP(RAMP_STONE, 1)); /* rooftops */
        tk_rect(x + w / 2 - 1, y, 2, h, PAL_RAMP(RAMP_WOOD, 0));
        tk_rect(x, y + h / 2 - 1, w, 2, PAL_RAMP(RAMP_WOOD, 0));
    }
    /* pegboard with tools */
    {
        int x = 76, y = 6, w = 64, h = 44;
        tk_rect(x, y, w, h, PAL_RAMP(RAMP_WOOD, 5));
        tk_frame(x, y, w, h, PAL_RAMP(RAMP_WOOD, 1));
        for (int j = y + 4; j < y + h; j += 6) for (int i = x + 4; i < x + w; i += 6) tk_px(i, j, PAL_RAMP(RAMP_WOOD, 3));
        uint8_t m = PAL_RAMP(RAMP_STEEL, 5), md = PAL_RAMP(RAMP_STEEL, 3);
        /* wrench */
        tk_rect(x + 8, y + 8, 3, 26, m); tk_circle(x + 9, y + 8, 4, m); tk_rect(x + 8, y + 3, 3, 5, PAL_RAMP(RAMP_WOOD, 5));
        /* hammer */
        tk_rect(x + 22, y + 12, 3, 24, PAL_RAMP(RAMP_WOOD, 2)); tk_rect(x + 17, y + 8, 13, 6, md);
        /* saw */
        tk_rect(x + 36, y + 8, 6, 8, PAL_PAINT(PAINT_CHERRY, 1)); tk_tri(x + 36, y + 16, x + 42, y + 16, x + 39, y + 38, m);
        /* a plush, for reasons */
        tk_circle(x + 54, y + 20, 6, PAL_PAINT(PAINT_CARAMEL, 1)); tk_circle(x + 50, y + 14, 2, PAL_PAINT(PAINT_CARAMEL, 1));
        tk_circle(x + 58, y + 14, 2, PAL_PAINT(PAINT_CARAMEL, 1)); tk_px(x + 52, y + 19, PAL_BLACK); tk_px(x + 56, y + 19, PAL_BLACK);
        tk_px(x + 54, y + 22, PAL_PAINT(PAINT_ROSE, 1));
        tk_rect(x + 50, y + 28, 9, 12, PAL_PAINT(PAINT_CARAMEL, 1));
    }
    /* DAYS SINCE LAST FIRE */
    {
        int x = 150, y = 8;
        tk_rect(x, y, 84, 30, PAL_BLACK);
        tk_rect(x + 1, y + 1, 82, 28, PAL_UI_CREAM);
        tk_rect(x + 1, y + 1, 82, 3, PAL_UI_GOLD);
        tk_text(x + 4, y + 6, "DAYS SINCE", PAL_BLACK, 1);
        tk_text(x + 4, y + 16, "LAST FIRE:", PAL_BLACK, 1);
        int dx = x + 66, dy = y + 5;
        tk_rect(dx - 1, dy, 15, 22, PAL_BLACK);
        int sq = flip_age >= 0 && flip_age < 3 ? flip_age : -1;
        char d[2] = { (char)('0' + sign_digit), 0 };
        if (sq >= 0) { /* flip animation: the card squashes */
            tk_rect(dx, dy + 2 + sq * 3, 13, 18 - sq * 6 > 2 ? 18 - sq * 6 : 2, PAL_WHITE);
        } else {
            tk_rect(dx, dy + 1, 13, 20, PAL_WHITE);
            tk_text(dx + 2, dy + 2, d, sign_digit == 0 ? PAL_UI_RED : PAL_BLACK, 2);
            tk_rect(dx, dy + 10, 13, 1, PAL_GRAY_L);
        }
    }
    /* lamp */
    {
        float sw = sinf(f * 0.12f) * 3;
        int lx = 140 + (int)sw;
        tk_line(140, 0, lx, 52, PAL_BLACK);
        tk_tri(lx - 8, 60, lx + 8, 60, lx, 52, PAL_RAMP(RAMP_STEEL, 3));
        tk_rect(lx - 2, 59, 5, 3, PAL_RAMP(RAMP_CANDY, 7));
        cone(lx, 62, 118, 6, 34, PAL_RAMP(RAMP_WOOD, 4), 4);
    }
    /* floor */
    tk_rect(0, 140, TW, 20, PAL_RAMP(RAMP_WOOD, 1));
    tk_rect(0, 140, TW, 1, PAL_RAMP(RAMP_WOOD, 0));
}

/* ---- the gun being welded (an assault rifle) -------------------------------- */
#define GX 156
#define GY 96
static void part_receiver(int x, int y) {
    tk_rect(x - 16, y - 6, 34, 15, PAL_BLACK);
    tk_rect(x - 15, y - 5, 32, 13, PAL_RAMP(RAMP_STEEL, 3));
    tk_rect(x - 15, y - 5, 32, 2, PAL_RAMP(RAMP_STEEL, 5));
    tk_rect(x - 15, y + 6, 32, 2, PAL_RAMP(RAMP_STEEL, 2));
    tk_rect(x - 6, y - 1, 10, 3, PAL_RAMP(RAMP_STEEL, 1));   /* ejection port */
    tk_heart(x + 10, y + 1, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1)); /* a sticker */
    /* grip + trigger */
    tk_rect(x - 4, y + 8, 7, 12, PAL_BLACK); tk_rect(x - 3, y + 8, 5, 11, PAL_RAMP(RAMP_STEEL, 2));
    tk_ring(x + 6, y + 11, 3, PAL_BLACK);
}
static void part_barrel(int x, int y) {
    tk_rect(x, y - 3, 44, 7, PAL_BLACK);
    tk_rect(x, y - 2, 43, 5, PAL_RAMP(RAMP_STEEL, 2));
    tk_rect(x, y - 2, 43, 1, PAL_RAMP(RAMP_STEEL, 4));
    tk_rect(x + 2, y - 5, 18, 11, PAL_BLACK); tk_rect(x + 3, y - 4, 16, 9, PAL_RAMP(RAMP_STEEL, 3)); /* handguard */
    for (int i = 0; i < 4; i++) tk_rect(x + 5 + i * 4, y - 2, 2, 5, PAL_RAMP(RAMP_STEEL, 1));
    tk_rect(x + 40, y - 4, 7, 9, PAL_BLACK); tk_rect(x + 41, y - 3, 5, 7, PAL_RAMP(RAMP_STEEL, 4)); /* muzzle brake */
}
static void part_stock(int x, int y) {
    tk_tri(x, y - 5, x - 30, y - 2, x - 30, y + 12, PAL_BLACK);
    tk_rect(x - 31, y - 3, 31, 9, PAL_BLACK);
    tk_rect(x - 30, y - 2, 30, 7, PAL_RAMP(RAMP_WOOD, 3));
    tk_tri(x - 1, y + 4, x - 29, y + 4, x - 29, y + 11, PAL_RAMP(RAMP_WOOD, 3));
    tk_rect(x - 30, y - 2, 30, 1, PAL_RAMP(RAMP_WOOD, 5));
    tk_rect(x - 33, y - 3, 3, 15, PAL_BLACK); tk_rect(x - 32, y - 2, 2, 13, PAL_RAMP(RAMP_STEEL, 1));
}
static void part_mag(int x, int y) {
    tk_tri(x, y, x + 8, y, x + 12, y + 16, PAL_BLACK);
    tk_tri(x, y, x + 12, y + 16, x + 4, y + 17, PAL_BLACK);
    tk_tri(x + 1, y, x + 7, y, x + 10, y + 14, PAL_RAMP(RAMP_STEEL, 2));
    tk_tri(x + 1, y, x + 10, y + 14, x + 5, y + 15, PAL_RAMP(RAMP_STEEL, 2));
}
static void part_scope(int x, int y) {
    tk_rect(x - 12, y - 4, 25, 7, PAL_BLACK);
    tk_rect(x - 11, y - 3, 23, 5, PAL_RAMP(RAMP_STEEL, 1));
    tk_rect(x - 13, y - 5, 4, 9, PAL_BLACK); tk_rect(x + 10, y - 5, 4, 9, PAL_BLACK);
    tk_rect(x + 11, y - 4, 2, 7, PAL_PAINT(PAINT_CYAN, 1));
    tk_rect(x - 9, y - 3, 6, 1, PAL_RAMP(RAMP_STEEL, 4));
    tk_rect(x - 3, y + 3, 6, 3, PAL_BLACK);
}
static void charm(int ax, int ay, float ang, int kind) {
    int len = 9;
    int ex = ax + (int)(sinf(ang) * len), ey = ay + (int)(cosf(ang) * len);
    tk_line(ax, ay, ex, ey, PAL_GRAY_L);
    if (kind == 0) tk_heart(ex, ey + 3, 1, PAL_PAINT(PAINT_CHERRY, 1));
    else if (kind == 1) { /* teddy */
        tk_circle(ex, ey + 4, 4, PAL_BLACK); tk_circle(ex - 3, ey + 1, 2, PAL_BLACK); tk_circle(ex + 3, ey + 1, 2, PAL_BLACK);
        tk_circle(ex, ey + 4, 3, PAL_PAINT(PAINT_CARAMEL, 1)); tk_circle(ex - 3, ey + 1, 1, PAL_PAINT(PAINT_CARAMEL, 1));
        tk_circle(ex + 3, ey + 1, 1, PAL_PAINT(PAINT_CARAMEL, 1));
        tk_px(ex - 1, ey + 3, PAL_BLACK); tk_px(ex + 1, ey + 3, PAL_BLACK); tk_px(ex, ey + 5, PAL_PAINT(PAINT_ROSE, 1));
        tk_rect(ex - 2, ey + 8, 5, 4, PAL_BLACK); tk_rect(ex - 1, ey + 8, 3, 3, PAL_PAINT(PAINT_CARAMEL, 1));
    } else { /* skull */
        tk_circle(ex, ey + 4, 4, PAL_BLACK);
        tk_circle(ex, ey + 4, 3, PAL_RAMP(RAMP_BONE, 6));
        tk_rect(ex - 2, ey + 7, 5, 3, PAL_BLACK); tk_rect(ex - 1, ey + 7, 3, 2, PAL_RAMP(RAMP_BONE, 6));
        tk_px(ex - 1, ey + 3, PAL_BLACK); tk_px(ex + 1, ey + 3, PAL_BLACK);
        tk_px(ex - 1, ey + 4, PAL_UI_RED); tk_px(ex + 1, ey + 4, PAL_UI_RED);
        tk_px(ex, ey + 8, PAL_BLACK);
    }
}

/* part arrival: returns offset; t = frames since launch, 6-frame flight */
static float arrive(int t) { if (t < 0) return 1.0f; if (t >= 6) return 0.0f; float k = t / 6.0f; return 1.0f - k * k; }

static void scene_workshop(int f) {
    int fire = f >= 38;
    int digit = f >= 46 ? 0 : 7;
    workshop_bg(f, digit, f >= 43 && f < 46 ? f - 43 : -1);
    /* jig stand + bench */
    tk_rect(GX - 4, GY + 20, 8, 22, PAL_RAMP(RAMP_STEEL, 1));
    tk_rect(GX - 12, GY + 18, 24, 4, PAL_RAMP(RAMP_STEEL, 3));
    tk_rect(86, 118, TW - 86, 6, PAL_RAMP(RAMP_WOOD, 4));
    tk_rect(86, 118, TW - 86, 1, PAL_RAMP(RAMP_WOOD, 6));
    tk_rect(86, 124, TW - 86, 36, PAL_RAMP(RAMP_WOOD, 1));
    tk_rect(92, 124, 4, 36, PAL_RAMP(RAMP_WOOD, 0));
    tk_rect(230, 124, 4, 36, PAL_RAMP(RAMP_WOOD, 0));
    /* scattered bolts on the bench */
    for (int i = 0; i < 6; i++) tk_rect(100 + i * 22 + (i * 7) % 5, 116, 3, 2, PAL_RAMP(RAMP_STEEL, 4));
    /* the gun: parts fly in and clank */
    int shake = 0;
    static const int launch[5] = { 3, 11, 17, 23, 30 };
    for (int i = 0; i < 5; i++) { int a = f - launch[i] - 6; if (a >= 0 && a < 3) shake = (a & 1) ? -1 : 1; }
    int gx = GX + shake, gy = GY;
    part_stock(gx - 16 - (int)(arrive(f - launch[1]) * 120), gy);
    part_barrel(gx + 18 + (int)(arrive(f - launch[0]) * 110), gy - (int)(arrive(f - launch[0]) * 20));
    part_mag(gx + 6, gy + 8 + (int)(arrive(f - launch[2]) * 60));
    part_receiver(gx, gy);
    part_scope(gx + 2, gy - 11 - (int)(arrive(f - launch[3]) * 100));
    if (f >= launch[4]) {
        int a = f - launch[4];
        int dy = a < 6 ? (int)(arrive(a) * 90) : 0;
        float ang = a < 6 ? 0 : sinf((a - 6) * 0.35f) * 0.7f * expf(-(a - 6) * 0.04f);
        charm(gx - 44, gy + 8 - dy, ang, 0);
    }
    /* weld glow + sparks at the current joint */
    static const int jx[] = { 18, -16, 8, 0, 18 };
    static const int jy[] = { 0, 0, 8, -6, 0 };
    int seg = f < 50 ? (f / 10) % 5 : -1;
    for (int i = 0; i < 5; i++) {   /* clank burst when a part lands */
        int a = f - launch[i] - 6;
        if (a >= 0 && a < 8) tk_sparkles(gx + jx[i], gy + jy[i], a, 50 + i, 8);
    }
    /* Rivet */
    int mask_up = f >= 50;
    int x = 50, yf = 152, s = 3;
    tk_chibi(x, yf, &TK_RIVET, s, mask_up ? TKE_HAPPY : TKE_NORMAL, mask_up ? TKM_FANGS : TKM_FLAT, f, 1);
    if (!mask_up) {
        for (int j = 4; j <= 14; j++) for (int i = 2; i <= 16; i++) {
            int edge = (j == 4 || j == 14 || i == 2 || i == 16);
            cpixf(x, yf, s, i, j, edge ? PAL_BLACK : PAL_RAMP(RAMP_STEEL, 3));
        }
        for (int i = 4; i <= 14; i++) { cpixf(x, yf, s, i, 8, PAL_BLACK); cpixf(x, yf, s, i, 9, PAL_PAINT(PAINT_FOREST, 0)); cpixf(x, yf, s, i, 10, PAL_BLACK); }
        cpixf(x, yf, s, 12, 9, PAL_RAMP(RAMP_SICK, 6));
        cpixf(x, yf, s, 4, 5, PAL_RAMP(RAMP_STEEL, 5)); cpixf(x, yf, s, 5, 5, PAL_RAMP(RAMP_STEEL, 5));
    } else {
        /* mask flipped up onto the forehead */
        int b = f - 50 < 3 ? 3 - (f - 50) : 0;
        for (int j = -6 - b; j <= -1 - b; j++) for (int i = 3; i <= 15; i++) {
            int edge = (j == -6 - b || j == -1 - b || i == 3 || i == 15);
            cpixf(x, yf, s, i, j, edge ? PAL_BLACK : PAL_RAMP(RAMP_STEEL, 5));
        }
        for (int i = 4; i <= 14; i++) cpixf(x, yf, s, i, -5 - b, PAL_RAMP(RAMP_STEEL, 6));
        for (int i = 5; i <= 13; i++) cpixf(x, yf, s, i, -3 - b, PAL_PAINT(PAINT_FOREST, 0));
        cpixf(x, yf, s, 11, -3 - b, PAL_RAMP(RAMP_SICK, 6));
        /* thumbs up */
        int tx = x + 30, ty = yf - 66;
        tk_rect(tx - 1, ty + 5, 11, 10, PAL_BLACK);
        tk_rect(tx, ty + 6, 9, 8, PAL_PAINT(PAINT_CHERRY, 1));
        tk_rect(tx + 1, ty - 1, 5, 8, PAL_BLACK);
        tk_rect(tx + 2, ty, 3, 7, PAL_PAINT(PAINT_CHERRY, 1));
        tk_rect(tx + 1, ty + 9, 7, 1, PAL_PAINT(PAINT_CHERRY, 0));
        tk_sparkles(tx + 4, ty + 2, f, 33, 10);
    }
    /* the torch: from Rivet's hand to the joint */
    if (seg >= 0) {
        int hx = x + 22, hy = yf - 20;
        int tx = gx + jx[seg] - 6, ty = gy + jy[seg] + 6;
        tk_line(hx, hy, tx, ty, PAL_BLACK);
        tk_line(hx, hy - 1, tx, ty - 1, PAL_RAMP(RAMP_STEEL, 4));
        tk_line(hx + 1, hy, tx + 1, ty, PAL_RAMP(RAMP_STEEL, 2));
        tk_rect(hx - 3, hy - 3, 7, 7, PAL_BLACK); tk_rect(hx - 2, hy - 2, 5, 5, PAL_PAINT(PAINT_CHERRY, 1));
        int fx = gx + jx[seg], fy = gy + jy[seg];
        glow(fx, fy, 13 + (f & 1), PAL_RAMP(RAMP_SKY, 7), 9);
        tk_circle(fx, fy, 3 + (f & 1), PAL_RAMP(RAMP_SKY, 6));
        tk_circle(fx, fy, 2, PAL_WHITE);
        sparks(fx, fy, f, f - 16 > 0 ? f - 16 : 0, f + 1, 117, 777u, 3);
    }
    /* the tail catches fire */
    if (fire) {
        int tx = x + (9 - 19) * s - s + 1, ty = yf + (17 - 27) * s - 1;
        flame(tx + 1, ty, f, 3 + (f - 38 < 4 ? 0 : 2));
        if (f - 38 < 6) tk_sparkles(tx, ty - 4, f - 38, 3, 6);
        /* smoke wisps */
        for (int i = 0; i < 3; i++) {
            int ph = (f * 2 + i * 9) % 26;
            tk_circle(tx + (int)(sinf(ph * 0.3f + i) * 2), ty - 10 - ph, 1 + ph / 10, PAL_GRAY_D);
        }
    }
    /* caption */
    if (f >= 10 && f < 50) {
        int a = f - 10;
        int y = 66 - (a < 3 ? (3 - a) * 5 : 0);
        tk_caption(y, "WELD YOUR WEAPON.", PAL_WHITE, PAL_RAMP(RAMP_WOOD, 0), 2);
    }
    if (f >= 10 && f < 13) tk_post_shake(0, f == 10 ? 3 : (f == 11 ? -2 : 1));
    if (shake) tk_post_shake(shake, 0);
}

/* ---- split screen --------------------------------------------------------- */
static void plush_launcher(int x, int y, int f, int nervous) {
    /* x,y: centre of the tube; points right */
    uint8_t b0 = PAL_PAINT(PAINT_BUBBLEGUM, 0), b1 = PAL_PAINT(PAINT_BUBBLEGUM, 1), b2 = PAL_PAINT(PAINT_BUBBLEGUM, 2);
    /* stock (lavender heart-ish) */
    tk_circle(x - 40, y + 4, 9, PAL_BLACK);
    tk_circle(x - 40, y + 4, 8, PAL_PAINT(PAINT_LAVENDER, 1));
    tk_circle(x - 42, y + 1, 3, PAL_PAINT(PAINT_LAVENDER, 2));
    tk_rect(x - 36, y - 1, 10, 9, PAL_BLACK); tk_rect(x - 36, y, 10, 7, PAL_PAINT(PAINT_LAVENDER, 1));
    /* grip */
    tk_rect(x - 14, y + 8, 10, 17, PAL_BLACK); tk_rect(x - 13, y + 8, 8, 16, PAL_PAINT(PAINT_MINT, 1));
    tk_rect(x - 13, y + 8, 2, 16, PAL_PAINT(PAINT_MINT, 2));
    tk_ring(x - 1, y + 13, 4, PAL_BLACK);
    /* tube */
    tk_rect(x - 30, y - 12, 62, 24, PAL_BLACK);
    tk_rect(x - 29, y - 11, 60, 22, b1);
    tk_rect(x - 29, y - 11, 60, 4, b2);
    tk_rect(x - 29, y + 7, 60, 4, b0);
    tk_rect(x - 25, y - 9, 20, 1, PAL_WHITE);
    /* muzzle ring */
    tk_rect(x + 30, y - 14, 9, 28, PAL_BLACK);
    tk_rect(x + 31, y - 13, 7, 26, PAL_PAINT(PAINT_MINT, 1));
    tk_rect(x + 31, y - 13, 7, 3, PAL_PAINT(PAINT_MINT, 2));
    /* teddy ammo peeking out of the muzzle */
    {
        int tx = x + 44 + (nervous && (f & 2) ? 1 : 0), ty = y - 2;
        tk_circle(tx - 4, ty - 8, 3, PAL_BLACK); tk_circle(tx + 5, ty - 8, 3, PAL_BLACK);
        tk_circle(tx, ty, 9, PAL_BLACK);
        tk_circle(tx - 4, ty - 8, 2, PAL_PAINT(PAINT_CARAMEL, 1)); tk_circle(tx + 5, ty - 8, 2, PAL_PAINT(PAINT_CARAMEL, 1));
        tk_circle(tx, ty, 8, PAL_PAINT(PAINT_CARAMEL, 1));
        tk_ellipse(tx + 1, ty + 3, 4, 3, PAL_PAINT(PAINT_CARAMEL, 2));
        tk_px(tx + 1, ty + 2, PAL_BLACK);
        int look = nervous ? 1 : 0;
        int bl = (f % 40) > 37;
        if (bl) { tk_rect(tx - 4, ty - 2, 3, 1, PAL_BLACK); tk_rect(tx + 3, ty - 2, 3, 1, PAL_BLACK); }
        else { tk_rect(tx - 3 + look, ty - 3, 2, 3, PAL_BLACK); tk_rect(tx + 4 + look, ty - 3, 2, 3, PAL_BLACK);
               tk_px(tx - 3 + look, ty - 3, PAL_WHITE); tk_px(tx + 4 + look, ty - 3, PAL_WHITE); }
        if (nervous) { tk_line(tx, ty + 6, tx + 3, ty + 5, PAL_BLACK); }
        else { tk_px(tx, ty + 5, PAL_BLACK); tk_px(tx + 1, ty + 6, PAL_BLACK); tk_px(tx + 2, ty + 5, PAL_BLACK); }
        tk_px(tx - 6, ty + 2, PAL_PAINT(PAINT_BUBBLEGUM, 1)); tk_px(tx + 7, ty + 2, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        if (nervous) { /* big anime sweat drop */
            int d = (f / 4) % 3;
            int sx = tx - 10, sy = ty - 10 + d;
            tk_tri(sx, sy - 4, sx - 3, sy + 2, sx + 3, sy + 2, PAL_BLACK);
            tk_circle(sx, sy + 2, 3, PAL_BLACK);
            tk_tri(sx, sy - 3, sx - 2, sy + 2, sx + 2, sy + 2, PAL_PAINT(PAINT_SKY, 2));
            tk_circle(sx, sy + 2, 2, PAL_PAINT(PAINT_SKY, 2));
            tk_px(sx - 1, sy + 1, PAL_WHITE);
        }
    }
    /* stickers */
    tk_star(x - 18, y - 2, 3, PAL_PAINT(PAINT_LEMON, 1)); tk_px(x - 18, y - 2, PAL_WHITE);
    tk_heart(x + 2, y, 1, PAL_PAINT(PAINT_CHERRY, 1));
    tk_circle(x + 18, y + 1, 4, PAL_PAINT(PAINT_LEMON, 1));
    tk_px(x + 16, y, PAL_BLACK); tk_px(x + 20, y, PAL_BLACK); tk_px(x + 17, y + 3, PAL_BLACK); tk_px(x + 18, y + 3, PAL_BLACK); tk_px(x + 19, y + 3, PAL_BLACK);
    /* heart sight on top */
    tk_rect(x - 4, y - 16, 3, 5, PAL_BLACK);
    tk_heart(x - 3, y - 19, 1, PAL_PAINT(PAINT_MAGENTA, 1));
    /* bow */
    tk_tri(x - 30, y - 12, x - 36, y - 18, x - 36, y - 6, PAL_PAINT(PAINT_SKY, 1));
    tk_tri(x - 30, y - 12, x - 24, y - 18, x - 24, y - 6, PAL_PAINT(PAINT_SKY, 1));
    tk_circle(x - 30, y - 12, 2, PAL_PAINT(PAINT_SKY, 0));
}

static void super_shotgun(int x, int y, int f) {
    /* x,y: centre of the receiver; barrels point left */
    uint8_t s1 = PAL_RAMP(RAMP_STEEL, 1), s2 = PAL_RAMP(RAMP_STEEL, 2), s3 = PAL_RAMP(RAMP_STEEL, 3), s5 = PAL_RAMP(RAMP_STEEL, 5);
    /* stock */
    tk_tri(x + 10, y - 6, x + 44, y + 2, x + 44, y + 18, PAL_BLACK);
    tk_tri(x + 10, y - 6, x + 44, y + 18, x + 10, y + 8, PAL_BLACK);
    tk_tri(x + 11, y - 4, x + 42, y + 3, x + 42, y + 16, PAL_RAMP(RAMP_WOOD, 1));
    tk_tri(x + 11, y - 4, x + 42, y + 16, x + 11, y + 6, PAL_RAMP(RAMP_WOOD, 1));
    tk_line(x + 12, y - 3, x + 41, y + 4, PAL_RAMP(RAMP_WOOD, 3));
    tk_rect(x + 43, y + 1, 4, 19, PAL_BLACK); tk_rect(x + 44, y + 2, 2, 17, PAL_RAMP(RAMP_BLOOD, 2));
    /* stock spikes */
    for (int i = 0; i < 3; i++) {
        int sx = x + 18 + i * 9, sy = y - 3 + i * 2;
        tk_tri(sx - 2, sy, sx + 2, sy, sx, sy - 6, PAL_BLACK); tk_tri(sx - 1, sy, sx + 1, sy, sx, sy - 5, PAL_RAMP(RAMP_BONE, 6));
    }
    /* receiver */
    tk_rect(x - 12, y - 8, 24, 16, PAL_BLACK);
    tk_rect(x - 11, y - 7, 22, 14, s3);
    tk_rect(x - 11, y - 7, 22, 2, s5);
    tk_rect(x - 6, y - 2, 6, 3, s1);
    /* trigger + guard */
    tk_ring(x - 2, y + 11, 4, PAL_BLACK);
    tk_rect(x + 2, y + 6, 6, 12, PAL_BLACK); tk_rect(x + 3, y + 6, 4, 11, PAL_RAMP(RAMP_WOOD, 1));
    /* twin barrels */
    tk_rect(x - 72, y - 9, 62, 14, PAL_BLACK);
    tk_rect(x - 71, y - 8, 60, 5, s2); tk_rect(x - 71, y - 8, 60, 1, s5);
    tk_rect(x - 71, y - 2, 60, 5, s2); tk_rect(x - 71, y - 2, 60, 1, PAL_RAMP(RAMP_STEEL, 4));
    tk_rect(x - 71, y - 3, 60, 1, PAL_BLACK);
    tk_rect(x - 73, y - 9, 3, 14, s1);
    tk_rect(x - 74, y - 7, 2, 3, PAL_BLACK); tk_rect(x - 74, y - 1, 2, 3, PAL_BLACK);
    /* fore-end */
    tk_rect(x - 46, y + 2, 26, 8, PAL_BLACK); tk_rect(x - 45, y + 3, 24, 6, PAL_RAMP(RAMP_WOOD, 2));
    for (int i = 0; i < 5; i++) tk_rect(x - 42 + i * 5, y + 4, 1, 4, PAL_RAMP(RAMP_WOOD, 0));
    /* barrel spikes */
    for (int i = 0; i < 5; i++) {
        int sx = x - 66 + i * 11, sy = y - 9;
        tk_tri(sx - 3, sy, sx + 3, sy, sx, sy - 7, PAL_BLACK); tk_tri(sx - 2, sy, sx + 2, sy, sx, sy - 6, PAL_RAMP(RAMP_BONE, 6));
    }
    /* drips */
    tk_rect(x + 30, y + 12, 1, 3 + (f / 8) % 3, PAL_RAMP(RAMP_BLOOD, 4));
    tk_rect(x + 20, y + 8, 1, 2 + (f / 10) % 2, PAL_RAMP(RAMP_BLOOD, 4));
    /* metal glint sweeping along the barrels */
    int g = (f * 4) % 160 - 40;
    if (g > -70 && g < -10) { tk_rect(x + g - 1, y - 8, 2, 1, PAL_WHITE); tk_rect(x + g, y - 2, 2, 1, PAL_WHITE); }
    /* smoke from the muzzle */
    for (int i = 0; i < 4; i++) {
        int ph = (f * 2 + i * 11) % 40;
        tk_circle(x - 76 - ph / 4 + (int)(sinf(ph * 0.25f + i) * 2), y - 4 - ph, 1 + ph / 12, ph < 24 ? PAL_GRAY : PAL_GRAY_D);
    }
}

static void scene_split(int t) {
    /* t = frames since the split */
    float kin = tk_bounce(t / 9.0f);
    int offL = (int)((1 - kin) * -130), offR = (int)((1 - kin) * 130);
    if (t > 9) { offL = 0; offR = 0; }
    /* --- CUTE panel --- */
    tk_rect(0, 0, TW, TH, PAL_BLACK);
    {
        int ox = offL;
        tk_rect(ox, 0, 120, TH, PAL_RAMP(RAMP_PASTEL_PINK, 6));
        for (int j = -1; j < 9; j++)
            for (int i = -1; i < 7; i++) {
                int px = ox + i * 20 + ((j & 1) ? 10 : 0) + (t / 2) % 20, py = j * 20 + (t / 2) % 20;
                if (px < ox + 120 && px > ox - 4) tk_heart(px, py, 1, PAL_RAMP(RAMP_PASTEL_PINK, 7));
            }
        int bounce = (int)(fabsf(sinf(t * 0.25f)) * 3);
        tk_text_outline(ox + 60 - tk_text_w("CUTE", 3) / 2, 10 - bounce, "CUTE", PAL_WHITE, PAL_UI_PINK_D, 3);
        int bob = (int)(sinf(t * 0.2f) * 2);
        plush_launcher(ox + 52, 82 + bob, t, t >= 46);
        /* teddy charm off the stock */
        charm(ox + 14, 90 + bob, sinf(t * 0.3f) * 0.6f, 1);
        tk_sparkles(ox + 58, 80, t, 21, 46);
        tk_text_outline(ox + 6, 116, "PLUSH LAUNCHER", PAL_PAINT(PAINT_GRAPE, 1), PAL_WHITE, 1);
    }
    /* --- BRUTAL panel --- */
    {
        int ox = 120 + offR;
        tk_rect(ox, 0, 120, TH, PAL_RAMP(RAMP_BLOOD, 1));
        for (int j = 0; j < TH; j += 8) tk_dither_rect(ox, j, 120, 4, PAL_RAMP(RAMP_BLOOD, 0), 6);
        /* flames along the bottom */
        for (int i = 0; i < 16; i++) {
            int fx = ox + i * 8 + 4, h = 10 + (int)(fabsf(sinf(t * 0.4f + i * 1.7f)) * 12);
            tk_tri(fx - 5, TH, fx + 5, TH, fx + ((t + i) & 1 ? 1 : -1), TH - h, PAL_RAMP(RAMP_LAVA, 3));
            tk_tri(fx - 3, TH, fx + 3, TH, fx, TH - h * 2 / 3, PAL_RAMP(RAMP_LAVA, 5));
        }
        int sh = (t % 20) < 2 ? 1 : 0;
        int tw = tk_text_w("BRUTAL", 3);
        tk_text_outline(ox + 60 - tw / 2 + sh, 10, "BRUTAL", PAL_UI_RED, PAL_BLACK, 3);
        /* drips off the letters */
        for (int i = 0; i < 5; i++) {
            int dx = ox + 60 - tw / 2 + 6 + i * 19, len = ((t + i * 7) / 3) % 9;
            tk_rect(dx, 34, 2, len, PAL_UI_RED); tk_px(dx, 34 + len, PAL_UI_RED_D);
        }
        int bob = (int)(sinf(t * 0.2f + 1.5f) * 2);
        super_shotgun(ox + 76, 80 + bob, t);
        charm(ox + 112, 98 + bob, -sinf(t * 0.3f) * 0.6f, 2);
        tk_text_outline(ox + 12, 116, "SUPER SHOTGUN", PAL_RAMP(RAMP_STEEL, 6), PAL_BLACK, 1);
    }
    /* --- the divider: a jagged bolt --- */
    if (t >= 6) {
        for (int y = 0; y < TH; y += 8) {
            int x0 = 120 + (((y / 8) & 1) ? 3 : -3), x1 = 120 + ((((y / 8) + 1) & 1) ? 3 : -3);
            for (int d = -2; d <= 2; d++) tk_line(x0 + d, y, x1 + d, y + 8, PAL_BLACK);
            tk_line(x0, y, x1, y + 8, PAL_WHITE);
            tk_line(x0 + 1, y, x1 + 1, y + 8, PAL_UI_GOLD);
        }
    }
    /* "VS" */
    if (t >= 10) {
        int a = t - 10;
        int sc = a < 2 ? 3 : 2;
        tk_text_outline(120 - tk_text_w("VS", sc) / 2, 64 - sc * 4, "VS", PAL_UI_GOLD, PAL_BLACK, sc);
    }
    /* tiny caption */
    if (t >= 24) {
        int a = t - 24;
        tk_caption(146 + (a < 3 ? 3 - a : 0), "(9 part slots. Millions of guns.)", PAL_WHITE, PAL_BLACK, 1);
    }
    if (t >= 10 && t < 14) tk_post_shake((t & 1) ? 3 : -3, 0);
}

void scene_s09_weld(int f, int n) {
    (void)n;
    if (f < F_SPLIT) scene_workshop(f);
    else scene_split(f - F_SPLIT);
}

void scene_s09_weld_audio(double t0, double dur) {
#define AT(fr) (t0 + (fr) / 30.0)
    (void)dur;
    /* welding sizzle: crackly high noise in bursts */
    for (int fr = 0; fr < 50; fr += 2) {
        uint32_t s = tk_hash((uint32_t)fr + 99u);
        if (tk_rand(&s, 3)) tk_slide(AT(fr), 0.05, 100 + tk_rand(&s, 12), 96, TKV_NOISE, 0.18f);
    }
    /* CLANK x5 */
    static const int launch[5] = { 3, 11, 17, 23, 30 };
    for (int i = 0; i < 5; i++) {
        double t = AT(launch[i] + 6);
        tk_note(t, 0.10, 91 + (i * 3) % 7, TKV_SQ12, 0.3f);
        tk_note(t, 0.25, 79 + (i * 3) % 7, TKV_TRI, 0.25f);
        tk_sfx(t, SFX_THUD, 0.4f);
    }
    tk_sfx(AT(10), SFX_THUD, 0.7f);              /* caption slam */
    tk_slide(AT(38), 0.25, 60, 84, TKV_NOISE, 0.35f);   /* fwoosh: tail ignites */
    tk_note(AT(46), 0.15, 88, TKV_SINE, 0.35f);          /* the sign: ding */
    tk_note(AT(46) + 0.15, 0.3, 84, TKV_SINE, 0.3f);
    tk_sfx(AT(50), SFX_SELECT, 0.4f);                    /* mask up */
    tk_sfx(AT(52), SFX_SPARKLE, 0.45f);                  /* thumbs up */
    /* split screen */
    tk_sfx(AT(F_SPLIT), SFX_WHOOSH, 0.6f);
    tk_sfx(AT(F_SPLIT + 10), SFX_THUD, 0.9f);
    tk_sfx(AT(F_SPLIT + 10), SFX_EXPLOSION, 0.3f);
    tk_sfx(AT(F_SPLIT + 14), SFX_SQUEAK, 0.5f);          /* cute */
    tk_sfx(AT(F_SPLIT + 15), SFX_SPARKLE, 0.35f);
    tk_sfx(AT(F_SPLIT + 20), SFX_SHOTGUN, 0.45f);        /* brutal: chk-chk */
    tk_sfx(AT(F_SPLIT + 24), SFX_BLIP, 0.35f);
    tk_slide(AT(F_SPLIT + 46), 0.15, 84, 80, TKV_SQ12, 0.2f);  /* teddy gulp */
    tk_slide(AT(F_SPLIT + 50), 0.1, 80, 76, TKV_SQ12, 0.2f);
#undef AT
}
