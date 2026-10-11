/* s03_hahaland.c - "Welcome to HaHaland!"
 * A long camera pan across the sugariest town on any handheld: heart-window
 * cottages, lollipop trees, a fountain, Nana's bakery, waving citizens...
 * the pan brakes at a big ugly wall: "HELLVILE - KEEP OUT". Red eyes pop up
 * over it. "(and 500 not-so-nice people next door)" */
#include "tk.h"
#include <math.h>
#include <string.h>

#define WORLD_W 660
#define WALL_X 532
#define GROUND 118
#define FEET 131
#define CAM_END 400

static int cam;
static int SX(int wx) { return wx - cam; }

/* ---- a chibi that waves (replaces the right arm with a raised one) ------- */
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
    tk_rect(CX(15), CY(15 + b), s, 8 * s, PAL_BLACK); /* body side outline */
    /* raised arm: outfit pixels + skin hand, two wave phases */
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

static TkChibi waver(uint32_t i) {
    TkChibi c = tk_random_citizen(i, 0);
    if (c.hair_style == TKH_LONG || c.hair_style == TKH_TWINTAILS) c.hair_style = TKH_BOB;
    if (c.hair_style == TKH_BOB) c.hair_style = TKH_SHORT;
    c.wings = 0; c.tail = 0; c.blush = 1;
    return c;
}

/* ---- scenery ------------------------------------------------------------- */
static void cloud(int x, int y, int s) {
    uint8_t sh = PAL_RAMP(RAMP_STONE, 7);
    tk_ellipse(x, y + 2, 14 * s / 4, 5 * s / 4, sh);
    tk_circle(x - 6 * s / 4, y - 1, 6 * s / 4, PAL_WHITE);
    tk_circle(x + 3 * s / 4, y - 4 * s / 4, 8 * s / 4, PAL_WHITE);
    tk_circle(x + 10 * s / 4, y, 5 * s / 4, PAL_WHITE);
    tk_ellipse(x, y + 1, 13 * s / 4, 4 * s / 4, PAL_WHITE);
}

static void lollipop_tree(int wx, int f, int r, int p1, int p2) {
    int x = SX(wx);
    if (x < -r - 4 || x > TW + r + 4) return;
    int top = GROUND - 30;
    /* striped stick */
    for (int y = top; y < GROUND + 2; y++) {
        uint8_t c = ((y + x) / 3) & 1 ? PAL_WHITE : PAL_PAINT(PAINT_BUBBLEGUM, 1);
        tk_rect(x - 1, y, 3, 1, c);
        tk_px(x - 2, y, PAL_RAMP(RAMP_PASTEL_PINK, 1)); tk_px(x + 2, y, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    }
    tk_circle(x, top, r + 1, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    /* rotating swirl */
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            float d = sqrtf((float)(i * i + j * j));
            if (d > r + 0.3f) continue;
            float a = atan2f((float)j, (float)i) + f * 0.08f;
            int band = (int)floorf((a / 6.2832f) * 2 + d * 0.28f) & 1;
            uint8_t c = band ? PAL_PAINT(p1, 1) : PAL_PAINT(p2, 2);
            tk_px(x + i, top + j, c);
        }
    tk_rect(x - r / 2, top - r / 2, 2, 2, PAL_WHITE);
}

static void house(int wx, int w, int h, int wall_p, int roof_p, int f, int seed) {
    int x = SX(wx) - w / 2;
    if (x > TW + 10 || x + w < -10) return;
    int y = GROUND + 2 - h;
    uint8_t ol = PAL_RAMP(RAMP_PASTEL_PINK, 1);
    /* chimney + heart smoke */
    int chx = x + w - 14;
    tk_rect(chx - 1, y - 20, 10, 18, ol);
    tk_rect(chx, y - 19, 8, 17, PAL_PAINT(PAINT_RUST, 2));
    tk_rect(chx, y - 19, 8, 2, PAL_PAINT(PAINT_RUST, 1));
    for (int i = 0; i < 3; i++) {
        int ph = (f + i * 13 + seed * 7) % 39;
        int hx = chx + 4 + (int)(3 * sinf(ph * 0.25f + i)), hy = y - 22 - ph;
        if (ph < 30) tk_heart(hx, hy, 1, ph < 18 ? PAL_WHITE : PAL_RAMP(RAMP_PASTEL_PINK, 7));
    }
    /* walls */
    tk_rect(x - 1, y - 1, w + 2, h + 2, ol);
    tk_rect(x, y, w, h, PAL_PAINT(wall_p, 2));
    for (int j = y + 3; j < y + h; j += 6) tk_rect(x, j, w, 1, PAL_PAINT(wall_p, 1)); /* siding */
    /* roof */
    tk_tri(x - 7, y + 1, x + w + 6, y + 1, x + w / 2, y - h / 2 - 8, ol);
    tk_tri(x - 5, y, x + w + 4, y, x + w / 2, y - h / 2 - 6, PAL_PAINT(roof_p, 1));
    for (int k = 1; k < 4; k++) {
        int yy = y - k * (h / 2 + 6) / 4;
        int half = (w / 2 + 4) * (4 - k) / 4;
        for (int i = -half; i <= half; i += 3) tk_px(x + w / 2 + i, yy, PAL_PAINT(roof_p, 0));
    }
    /* round attic window */
    tk_circle(x + w / 2, y - 6, 3, ol); tk_circle(x + w / 2, y - 6, 2, PAL_PAINT(PAINT_LEMON, 2));
    /* door */
    int dx = x + w / 2 - 5;
    tk_rect(dx - 1, y + h - 19, 12, 20, ol);
    tk_rect(dx, y + h - 18, 10, 18, PAL_PAINT(PAINT_CARAMEL, 1));
    tk_rect(dx + 1, y + h - 19, 8, 1, PAL_PAINT(PAINT_CARAMEL, 1));
    tk_heart(dx + 5, y + h - 12, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_px(dx + 8, y + h - 8, PAL_UI_GOLD);
    /* heart windows (lit) with flower boxes */
    for (int s = -1; s <= 1; s += 2) {
        int wxp = x + w / 2 + s * (w / 2 - 9), wyp = y + 10;
        tk_rect(wxp - 7, wyp - 6, 14, 13, PAL_WHITE);
        tk_frame(wxp - 8, wyp - 7, 16, 15, ol);
        tk_heart(wxp, wyp, 2, (f / 20 + seed + s) % 7 == 0 ? PAL_PAINT(PAINT_LEMON, 1) : PAL_PAINT(PAINT_LEMON, 2));
        tk_rect(wxp - 8, wyp + 8, 16, 3, PAL_PAINT(PAINT_CHOCOLATE, 2));
        for (int i = 0; i < 4; i++)
            tk_px(wxp - 6 + i * 4, wyp + 7, i & 1 ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(PAINT_LEMON, 1));
    }
}

static void bakery(int wx, int f) {
    house(wx, 64, 46, PAINT_PEACH, PAINT_CHOCOLATE, f, 3);
    int x = SX(wx);
    if (x < -60 || x > TW + 60) return;
    int y = GROUND + 2 - 46;
    /* giant cupcake on the roof */
    int cy = y - 34 + ((f / 8) & 1);
    uint8_t ol = PAL_RAMP(RAMP_PASTEL_PINK, 1);
    tk_rect(x - 10, cy + 4, 20, 11, ol);
    tk_rect(x - 9, cy + 5, 18, 9, PAL_PAINT(PAINT_LEMON, 1));
    for (int i = -8; i <= 8; i += 3) tk_rect(x + i, cy + 5, 1, 9, PAL_PAINT(PAINT_HONEY, 1));
    tk_ellipse(x, cy + 2, 13, 6, ol);
    tk_ellipse(x, cy + 1, 12, 5, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    tk_ellipse(x, cy - 3, 8, 4, ol);
    tk_ellipse(x, cy - 3, 7, 3, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    tk_circle(x, cy - 8, 3, ol); tk_circle(x, cy - 8, 2, PAL_PAINT(PAINT_CHERRY, 1));
    tk_px(x - 1, cy - 9, PAL_WHITE);
    for (int i = 0; i < 6; i++) tk_px(x - 9 + i * 4, cy + 1 - (i & 1) * 2, (i & 1) ? PAL_PAINT(PAINT_SKY, 1) : PAL_PAINT(PAINT_LEMON, 1));
    /* sign */
    tk_rect(x - 22, y + 21, 44, 11, ol);
    tk_rect(x - 21, y + 22, 42, 9, PAL_UI_CREAM);
    tk_text(x - 20, y + 23, "BAKERY", PAL_PAINT(PAINT_ROSE, 0), 1);
}

static void fountain(int wx, int f) {
    int x = SX(wx);
    if (x < -50 || x > TW + 50) return;
    uint8_t st = PAL_RAMP(RAMP_STONE, 6), sd = PAL_RAMP(RAMP_STONE, 4), ol = PAL_RAMP(RAMP_STONE, 2);
    uint8_t wa = PAL_PAINT(PAINT_SKY, 2), wd = PAL_PAINT(PAINT_SKY, 1);
    int by = FEET - 4;
    /* pillar & top bowl */
    tk_rect(x - 4, by - 34, 8, 30, ol); tk_rect(x - 3, by - 34, 6, 30, st); tk_rect(x + 1, by - 34, 2, 30, sd);
    tk_ellipse(x, by - 34, 13, 4, ol); tk_ellipse(x, by - 35, 12, 3, st); tk_ellipse(x, by - 36, 10, 2, wa);
    /* heart on top */
    tk_heart(x, by - 44 + ((f / 10) & 1), 2, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    /* water arcs */
    for (int s = -1; s <= 1; s += 2)
        for (int k = 0; k < 14; k++) {
            float t = ((k * 5 + f * 2) % 70) / 70.0f;
            int px = x + s * (int)(6 + 22 * t), py = by - 38 + (int)(-8 * t + 36 * t * t);
            tk_px(px, py, (k & 1) ? PAL_WHITE : wa);
            if (k % 3 == 0) tk_px(px, py + 1, wd);
        }
    /* basin */
    tk_ellipse(x, by + 1, 31, 8, ol);
    tk_ellipse(x, by, 30, 7, st);
    tk_ellipse(x, by - 2, 27, 4, wa);
    for (int i = 0; i < 5; i++) { int rx = x - 20 + ((i * 11 + f) % 40); tk_rect(rx, by - 2, 3, 1, PAL_WHITE); }
    tk_rect(x - 30, by + 1, 61, 4, st);
    tk_rect(x - 30, by + 4, 61, 2, sd);
    tk_rect(x - 31, by + 6, 63, 1, ol);
}

static void flower(int x, int y, int col, int f, int seed) {
    int sway = ((f / 6 + seed) % 4 == 0) ? 1 : 0;
    tk_rect(x, y, 1, 5, PAL_RAMP(RAMP_GRASS, 3));
    int fx = x + sway, fy = y - 1;
    uint8_t c = PAL_PAINT(col, 1);
    tk_px(fx - 1, fy, c); tk_px(fx + 1, fy, c); tk_px(fx, fy - 1, c); tk_px(fx, fy + 1, c);
    tk_px(fx, fy, PAL_PAINT(PAINT_LEMON, 1));
}

static void flowerbed(int wx, int w, int f) {
    int x = SX(wx);
    if (x + w < -5 || x > TW + 5) return;
    tk_ellipse(x + w / 2, GROUND + 1, w / 2 + 1, 5, PAL_RAMP(RAMP_GRASS, 2));
    tk_ellipse(x + w / 2, GROUND, w / 2, 4, PAL_RAMP(RAMP_GRASS, 3));
    static const int cols[] = { PAINT_ROSE, PAINT_LEMON, PAINT_LAVENDER, PAINT_SKY, PAINT_BUBBLEGUM, PAINT_PEACH };
    for (int i = 0; i < w / 5; i++) flower(x + 3 + i * 5, GROUND - 6 + (i & 1) * 2, cols[i % 6], f, i);
}

/* ---- the wall ------------------------------------------------------------ */
static void the_wall(int f, int eyes_age) {
    int x = SX(WALL_X);
    if (x > TW) return;
    int top = 40;
    uint8_t m = PAL_RAMP(RAMP_STONE, 1), b1 = PAL_RAMP(RAMP_STONE, 3), b2 = PAL_RAMP(RAMP_STONE, 2),
            hi = PAL_RAMP(RAMP_STONE, 4);
    /* peeking heads with red eyes (behind the wall top) */
    static const int HX[] = { 12, 33, 53, 74, 95, 118, 140, 64 };
    for (int i = 0; i < 8; i++) {
        int a = eyes_age - i * 3 - (i == 7 ? 8 : 0);
        if (eyes_age < 0 || a < 0) continue;
        int rise = a == 0 ? 4 : a == 1 ? 12 : a == 2 ? 10 : 11;
        if (i == 3) rise += 3;
        rise += ((f / 7 + i) % 5 == 0);
        int hx = x + HX[i], hy = top + 6 - rise;
        uint8_t sk = PAL_RAMP(RAMP_VAPE, 0);
        int r = i == 3 ? 9 : 6 + (i & 1);
        tk_circle(hx, hy, r, sk);
        switch (i % 4) {
        case 0: case 2: /* horns */
            tk_tri(hx - r, hy - 2, hx - r + 4, hy - r + 1, hx - r - 2, hy - r - 6, sk);
            tk_tri(hx + r, hy - 2, hx + r - 4, hy - r + 1, hx + r + 2, hy - r - 6, sk);
            break;
        case 1: /* pointy ears */
            tk_tri(hx - r, hy, hx - r + 3, hy - 3, hx - r - 5, hy - 5, sk);
            tk_tri(hx + r, hy, hx + r - 3, hy - 3, hx + r + 5, hy - 5, sk);
            break;
        default: /* party hat. they are still cute-drawn */
            tk_tri(hx - 4, hy - r + 1, hx + 4, hy - r + 1, hx + 1, hy - r - 9, PAL_PAINT(PAINT_GRAPE, 0));
            tk_px(hx + 1, hy - r - 10, PAL_UI_GOLD);
            break;
        }
        int blink = ((f + i * 17) % 45) < 3;
        uint8_t e = PAL_UI_RED, eh = PAL_RAMP(RAMP_LAVA, 6);
        if (i == 3 && a > 30) { /* ...the cyclops waves back */
            int ph = (f / 5) & 1, hxp = hx + r + 3 + ph * 2, hyp = hy - 12 + ph;
            tk_line(hx + r - 2, hy + 2, hxp, hyp + 2, sk); tk_line(hx + r - 1, hy + 2, hxp + 1, hyp + 2, sk);
            tk_line(hx + r - 2, hy + 3, hxp, hyp + 3, sk);
            tk_circle(hxp, hyp, 3, sk);
            tk_rect(hxp - 2, hyp - 6, 1, 4, sk); tk_rect(hxp, hyp - 7, 1, 5, sk); tk_rect(hxp + 2, hyp - 6, 1, 4, sk);
        }
        if (i == 3) { /* the big cyclops */
            if (!blink) { tk_rect(hx - 2, hy - 2, 5, 4, e); tk_rect(hx - 1, hy - 1, 2, 2, eh); }
            else tk_rect(hx - 2, hy, 5, 1, e);
        } else if (!blink) {
            tk_rect(hx - 4, hy - 1, 3, 3, e); tk_px(hx - 4, hy - 1, eh);
            tk_rect(hx + 2, hy - 1, 3, 3, e); tk_px(hx + 2, hy - 1, eh);
        } else { tk_rect(hx - 4, hy + 1, 3, 1, e); tk_rect(hx + 2, hy + 1, 3, 1, e); }
    }
    /* wall body */
    tk_rect(x, top, WORLD_W - WALL_X + 20, GROUND + 4 - top, m);
    for (int row = 0; row * 8 + top < GROUND + 4; row++) {
        int yy = top + row * 8, off = (row & 1) * 9;
        for (int bx = -off; bx < 160; bx += 18) {
            uint32_t s = tk_hash((uint32_t)(row * 31 + bx * 7));
            uint8_t c = (s & 3) == 0 ? b2 : b1;
            int bl = x + bx + 1, bw = 16;
            if (bl < x + 2) { bw -= x + 2 - bl; bl = x + 2; }
            if (bw <= 0) continue;
            tk_rect(bl, yy + 1, bw, 6, c);
            tk_rect(bl, yy + 1, bw, 1, hi);
            if ((s & 7) == 5) { tk_line(x + bx + 4, yy + 2, x + bx + 8, yy + 6, m); }
        }
    }
    /* jagged top + rusty spikes */
    for (int i = 0; i < 160; i += 10) {
        tk_tri(x + i, top, x + i + 6, top, x + i + 3, top - 7, PAL_RAMP(RAMP_WOOD, 2));
        tk_px(x + i + 3, top - 6, PAL_RAMP(RAMP_WOOD, 4));
    }
    /* slime / grime drips */
    for (int i = 0; i < 6; i++) {
        int dx = x + 8 + i * 23, len = 6 + (int)(tk_hash(i + 5) % 14);
        tk_rect(dx, top + 1, 2, len, PAL_RAMP(RAMP_SICK, 3));
        tk_px(dx, top + len + 1, PAL_RAMP(RAMP_SICK, 2));
    }
    tk_rect(x, top, 2, GROUND + 4 - top, PAL_BLACK);
    /* the sign, slightly crooked */
    int sx = x + 18, sy = 58;
    uint8_t wd = PAL_RAMP(RAMP_WOOD, 4), wdd = PAL_RAMP(RAMP_WOOD, 2);
    for (int j = 0; j < 30; j++) {
        int sh = j / 10;
        tk_rect(sx - 1 + sh, sy + j, 82, 1, PAL_BLACK);
        tk_rect(sx + sh, sy + j, 80, 1, (j % 10 == 9) ? wdd : wd);
    }
    tk_px(sx + 4, sy + 3, PAL_GRAY_D); tk_px(sx + 77, sy + 5, PAL_GRAY_D);
    tk_text_outline(sx + 16, sy + 4, "HELLVILE", PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 0), 1);
    tk_text_outline(sx + 17, sy + 17, "KEEP OUT", PAL_UI_RED, PAL_RAMP(RAMP_BLOOD, 0), 1);
    /* a cute note taped on by some HaHalander */
    int nx = sx + 56, ny = sy + 34;
    tk_rect(nx, ny, 30, 16, PAL_RAMP(RAMP_PASTEL_PINK, 7));
    tk_frame(nx, ny, 30, 16, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    tk_rect(nx + 11, ny - 2, 8, 3, PAL_RAMP(RAMP_CANDY, 6));
    tk_text(nx + 3, ny + 3, "pls", PAL_PAINT(PAINT_ROSE, 0), 1);
    tk_heart(nx + 24, ny + 7, 1, PAL_PAINT(PAINT_CHERRY, 1));
    /* purple fog spilling over the top and down the face */
    for (int i = 0; i < 12; i++) {
        float ph = fmodf(f * 0.7f + i * 11.0f, 66.0f);
        int px = x + 8 + i * 12 - (int)(ph * 0.5f);
        int py = top + 3 + (int)(1.5f * sinf(f * 0.1f + i)) + (px < x + 6 ? (x + 6 - px) / 2 : 0);
        int r = 3 + (i % 2) + (int)(1.5f * sinf(ph * 0.095f));
        if (r < 2) r = 2;
        tk_circle(px, py + 1, r, PAL_RAMP(RAMP_VAPE, 4));
        tk_circle(px - 1, py, r, (i & 1) ? PAL_RAMP(RAMP_VAPE, 6) : PAL_RAMP(RAMP_VAPE, 5));
        tk_px(px - r / 2, py - r / 2, PAL_RAMP(RAMP_VAPE, 7));
    }
}

/* ---- scene --------------------------------------------------------------- */
static void caption_pop(int f, int a, int b, int cy, const char *s, int scale) {
    if (f < a || f >= b) return;
    int age = f - a;
    int dy = age == 0 ? -6 : age == 1 ? -2 : age == 2 ? 1 : 0;
    tk_caption(cy + dy, s, PAL_WHITE, PAL_RAMP(RAMP_PASTEL_PINK, 1), scale);
}

void scene_s03_hahaland(int f, int n) {
    (void)n;
    /* camera: ease-in-out pan, brakes hard at the wall with a tiny overshoot */
    float t = tk_clampf(f / 96.0f, 0, 1);
    float e = t < 0.5f ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t);
    cam = (int)(CAM_END * e);
    if (f >= 96 && f < 102) cam = CAM_END + (f < 99 ? 3 : 1) - (f == 101);

    /* sky (turns sinister past the wall) */
    static const uint8_t sky[] = { PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7),
                                   PAL_RAMP(RAMP_PASTEL_PINK, 7) };
    tk_vgrad(0, 0, TW, GROUND, sky, 4);
    int wsx = SX(WALL_X);
    if (wsx < TW) {
        static const uint8_t evil[] = { PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_BLOOD, 3), PAL_RAMP(RAMP_LAVA, 3),
                                        PAL_RAMP(RAMP_LAVA, 5) };
        int x0 = wsx - 14 < 0 ? 0 : wsx - 14;
        for (int x = x0; x < TW; x++) {
            int lvl = (x - (wsx - 14)) * 16 / 14;
            if (lvl > 16) lvl = 16;
            for (int y = 0; y < 48; y++) {
                static const uint8_t B[4][4] = { {0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5} };
                int ylvl = lvl - (x < wsx ? y / 2 : 0);
                if (B[y & 3][x & 3] < ylvl) {
                    int pos = y * 3 * 16 / 42, k = pos >> 4, fr = pos & 15;
                    if (k > 3) { k = 3; fr = 0; }
                    uint8_t c = evil[k];
                    if (k < 3 && B[(y + 1) & 3][(x + 2) & 3] < fr) c = evil[k + 1];
                    tk_px(x, y, c);
                }
            }
        }
    }
    /* clouds (far parallax) */
    for (int i = 0; i < 7; i++) {
        int x = i * 90 + 30 - cam / 6 + f / 5;
        int y = 10 + (i * 29) % 40;
        if (x > -40 && x < TW + 40 && x < wsx + 10) cloud(x, y, 3 + (i % 2));
    }
    /* distant hills (mid parallax) */
    for (int x = 0; x < TW; x++) {
        int wx = x + cam / 3;
        int h = 92 + (int)(9 * sinf(wx * 0.03f) + 5 * sinf(wx * 0.071f + 1.3f));
        uint8_t c = PAL_RAMP(RAMP_MINT, 6), ct = PAL_RAMP(RAMP_MINT, 7);
        if (x >= wsx) { c = PAL_RAMP(RAMP_VAPE, 2); ct = PAL_RAMP(RAMP_VAPE, 3); }
        for (int y = h; y < GROUND; y++) tk_px(x, y, c);
        tk_px(x, h, ct);
        if (x < wsx && (wx % 37) == 0) { /* tiny far trees */
            tk_circle(x, h - 3, 3, PAL_RAMP(RAMP_MINT, 5)); tk_px(x - 1, h - 4, ct);
        }
    }
    /* ground */
    tk_rect(0, GROUND, TW, TH - GROUND, PAL_RAMP(RAMP_GRASS, 6));
    tk_rect(0, GROUND, TW, 1, PAL_RAMP(RAMP_GRASS, 7));
    for (int x = 0; x < TW; x++) {
        int wx = x + cam;
        if ((wx * 7) % 11 == 0) tk_px(x, GROUND + 3 + (wx % 3), PAL_RAMP(RAMP_GRASS, 5));
    }
    /* path */
    tk_rect(0, 124, TW, 10, PAL_RAMP(RAMP_PEACH, 6));
    tk_rect(0, 124, TW, 1, PAL_RAMP(RAMP_PEACH, 5));
    tk_rect(0, 133, TW, 1, PAL_RAMP(RAMP_PEACH, 5));
    for (int i = 0; i < 40; i++) {
        int x = i * 23 - cam % 23;
        tk_rect(x, 127 + (i % 3) * 2, 2, 1, PAL_RAMP(RAMP_PEACH, 7));
    }

    /* buildings & props (back to front) */
    lollipop_tree(22, f, 11, PAINT_BUBBLEGUM, PAINT_LEMON);
    house(74, 56, 40, PAINT_BUBBLEGUM, PAINT_ROSE, f, 1);
    lollipop_tree(140, f, 9, PAINT_MINT, PAINT_WHITE);
    flowerbed(118, 30, f);
    fountain(228, f);
    bakery(330, f);
    lollipop_tree(392, f, 12, PAINT_LAVENDER, PAINT_LEMON);
    flowerbed(398, 40, f);
    house(470, 52, 38, PAINT_MINT, PAINT_SKY, f, 2);
    the_wall(f, f - 100);

    /* citizens */
    struct { int wx, id, wave, hop; } C[] = {
        { 34, 3, 1, 0 }, { 110, 11, 1, 0 }, { 136, 17, 0, 1 }, { 178, 24, 1, 0 },
        { 288, 31, 1, 0 }, { 360, 41, 0, 1 }, { 420, 52, 1, 0 }, { 444, 57, 0, 0 },
        { 506, 64, 1, 0 },
    };
    for (unsigned i = 0; i < sizeof C / sizeof C[0]; i++) {
        int x = SX(C[i].wx);
        if (x < -20 || x > TW + 20) continue;
        TkChibi c = waver((uint32_t)C[i].id);
        int hop = C[i].hop ? (int)(5 * fabsf(sinf((f + i * 5) * 0.21f))) : 0;
        int expr = (i % 3 == 0) ? TKE_HAPPY : TKE_NORMAL;
        int mouth = (i & 1) ? TKM_OPEN : TKM_SMILE;
        int flip = (i == 3 || i == 7);
        if (C[i].wave) chibi_wave(x, FEET - hop, &c, 1, expr, mouth, f + i * 3, flip, ((f + i * 2) / 5) & 1);
        else tk_chibi(x, FEET - hop, &c, 1, expr, mouth, f + i * 3, flip);
    }
    /* cast cameos: Nana outside her bakery, Pip by the fountain, the Mayor fluttering */
    if (SX(300) > -20 && SX(300) < TW + 20) chibi_wave(SX(300), FEET, &TK_NANA, 1, TKE_HAPPY, TKM_OPEN, f, 0, (f / 6) & 1);
    if (SX(196) > -20 && SX(196) < TW + 20) tk_chibi(SX(196), FEET - (int)(4 * fabsf(sinf(f * 0.25f))), &TK_PIP, 1, TKE_SPARKLE, TKM_OPEN, f, 0);
    {
        int mx = SX(258), my = FEET - 30 - (int)(4 * sinf(f * 0.2f));
        if (mx > -20 && mx < TW + 20) {
            tk_chibi(mx, my, &TK_MAYOR, 1, TKE_HAPPY, TKM_OPEN, f, 1);
            tk_sparkles(mx, my - 14, f, 9, 14);
        }
    }
    /* foreground flowers (fast parallax) */
    for (int i = 0; i < 60; i++) {
        int x = i * 17 - (cam * 5 / 4) % 17;
        int wx = i * 17;
        static const int cols[] = { PAINT_ROSE, PAINT_LEMON, PAINT_LAVENDER, PAINT_SKY, PAINT_BUBBLEGUM, PAINT_WHITE };
        int y = 146 + (int)(tk_hash((uint32_t)i + (uint32_t)(cam * 5 / 4 / 17)) % 9);
        (void)wx;
        if (x < -3 || x > TW + 3) continue;
        tk_rect(x - 1, y + 2, 3, 6, PAL_RAMP(RAMP_GRASS, 4));
        uint8_t c = PAL_PAINT(cols[(i + cam * 5 / 4 / 17) % 6], 1);
        tk_rect(x - 2, y - 1, 5, 3, c); tk_rect(x - 1, y - 2, 3, 5, c);
        tk_px(x, y, PAL_PAINT(PAINT_LEMON, 2));
    }

    /* captions */
    caption_pop(f, 4, 44, 26, "Welcome to\nHaHaland!", 2);
    caption_pop(f, 48, 94, 26, "Population: 500\nvery nice people", 2);
    if (f >= 108) {
        int age = f - 108;
        int dy = age == 0 ? -4 : age == 1 ? 1 : 0;
        tk_caption(146 + dy, "(and 500 not-so-nice\npeople next door)", PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1), 1);
    }
    if (f >= 100 && f < 103) tk_post_shake(0, f == 101 ? 1 : 0);
}

void scene_s03_hahaland_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    tk_sfx(t0 + 4 * fr, SFX_FANFARE, 0.5f);
    for (int i = 0; i < 4; i++) tk_sfx(t0 + (20 + i * 17) * fr, SFX_SQUEAK, 0.25f);  /* happy citizen squeaks */
    tk_sfx(t0 + 48 * fr, SFX_SELECT, 0.5f);
    tk_sfx(t0 + 96 * fr, SFX_SCRATCH, 0.8f);                                        /* the brakes */
    for (int i = 0; i < 8; i++)                                                      /* eyes popping up */
        tk_note(t0 + (100 + i * 3 + (i > 5 ? 6 : 0)) * fr, 0.05, 40 + (i * 5) % 9, TKV_SQ12, 0.3f);
    tk_slide(t0 + 102 * fr, 1.4, 36, 33, TKV_SAW, 0.18f);                          /* low drone */
    tk_sfx(t0 + 108 * fr, SFX_KAZOO, 0.45f);
}
