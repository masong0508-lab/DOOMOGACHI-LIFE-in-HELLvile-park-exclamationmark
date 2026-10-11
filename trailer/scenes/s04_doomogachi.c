/* s04_doomogachi.c - "Raise your very own DOOMOGACHI!"
 * A chunky egg-shaped virtual pet. The egg wobbles and hatches into a blob
 * ("beep."), eats a cupcake (hearts!), poops ("It loves you!")... then for
 * four frames the whole world goes to hell: red eyes, pentagram, horns.
 * Back to cute. "beep." ...it's fine. (The poop is now a smoking ash pile,
 * and are those... tiny horns?) */
#include "tk.h"
#include <math.h>
#include <string.h>

/* ---- virtual LCD ------------------------------------------------------------ */
#define LW 24
#define LH 16
#define LPX 3
static uint8_t lcd[LH][LW];      /* 0 off, 1 on, 2 red (evil) */
static int lcd_x, lcd_y;

static void lcd_clear(void) { memset(lcd, 0, sizeof lcd); }
static void lcd_set(int x, int y, uint8_t v) { if ((unsigned)x < LW && (unsigned)y < LH) lcd[y][x] = v; }
static void lcd_spr(const char *const *rows, int x, int y, int flip) {
    int w = (int)strlen(rows[0]);
    for (int j = 0; rows[j]; j++)
        for (int i = 0; rows[j][i]; i++) {
            char c = rows[j][i];
            int xx = flip ? x + w - 1 - i : x + i;
            if (c == '#') lcd_set(xx, y + j, 1);
            else if (c == 'r') lcd_set(xx, y + j, 2);
            else if (c == 'h') lcd_set(xx, y + j, 3);
            else if (c == 'o') lcd_set(xx, y + j, 0); /* explicit erase */
        }
}
static void lcd_render_at(int ox, int oy, int ps, int evil) {
    uint8_t bg = evil ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_UI_LCD;
    uint8_t sh = evil ? PAL_BLACK : PAL_RAMP(RAMP_SICK, 5);
    uint8_t on = evil ? PAL_UI_RED : PAL_RAMP(RAMP_SICK, 1);
    uint8_t gap = evil ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_SICK, 3);
    uint8_t red = PAL_UI_RED, redg = PAL_RAMP(RAMP_LAVA, 6);
    int so = ps / 3 > 0 ? ps / 3 : 1;
    tk_rect(ox, oy, LW * ps, LH * ps, bg);
    /* faint off-pixel ghosts */
    for (int y = 0; y < LH; y++)
        for (int x = 0; x < LW; x++)
            if (((x + y) & 1) == 0 || evil)
                tk_rect(ox + x * ps + ps / 3, oy + y * ps + ps / 3, ps / 3 > 0 ? ps / 3 : 1, ps / 3 > 0 ? ps / 3 : 1,
                        evil ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_GRASS, 6));
    /* drop shadows */
    for (int y = 0; y < LH; y++)
        for (int x = 0; x < LW; x++)
            if (lcd[y][x]) tk_rect(ox + x * ps + so, oy + y * ps + so, ps, ps, sh);
    for (int y = 0; y < LH; y++)
        for (int x = 0; x < LW; x++) {
            if (!lcd[y][x]) continue;
            int px = ox + x * ps, py = oy + y * ps;
            if (lcd[y][x] == 2) {
                tk_rect(px, py, ps, ps, gap);
                tk_rect(px, py, ps - 1, ps - 1, red);
                tk_rect(px, py, ps / 3 + 1, ps / 3 + 1, redg);
                continue;
            }
            if (lcd[y][x] == 3) {
                tk_rect(px, py, ps, ps, PAL_RAMP(RAMP_LAVA, 5));
                tk_rect(px + 1, py + 1, ps - 2, ps - 2, PAL_RAMP(RAMP_LAVA, 7));
                tk_rect(px + ps / 3, py + ps / 3, ps / 3, ps / 3, PAL_WHITE);
                continue;
            }
            tk_rect(px, py, ps, ps, gap);
            tk_rect(px, py, ps - 1, ps - 1, on);
        }
}
static void lcd_render(int evil) { lcd_render_at(lcd_x, lcd_y, LPX, evil); }

/* ---- LCD sprites ---------------------------------------------------------- */
static const char *EGG[] = {
    "...###...",
    "..#...#..",
    ".#.....#.",
    ".#..#..#.",
    "#..###..#",
    "#...#...#",
    "#.......#",
    "#.##....#",
    "#.##..#.#",
    ".#.....#.",
    "..#####..", NULL };
static const char *CRACK1[] = { "..#.#....", ".#.#.#...", NULL };
static const char *CRACK2[] = { "..#.#.#..", "##.#.#.##", NULL };
static const char *SHELL_TOP[] = { "...###...", "..#...#..", ".#.....#.", "#.#.#.#.#", NULL };
static const char *BLOB[] = {
    "....####....",
    "..##....##..",
    ".#........#.",
    "#..##..##..#",
    "#..##..##..#",
    "#..........#",
    "#....##....#",
    ".#........#.",
    "..########..",
    "...#....#...", NULL };
static const char *BLOB_SQ[] = {
    "....######....",
    "..##......##..",
    ".#..##..##..#.",
    "#...##..##...#",
    "#.....##.....#",
    ".#..........#.",
    "..##########..",
    "...##....##...", NULL };
static const char *BLOB_HAPPY[] = {
    "....####....",
    "..##....##..",
    ".#........#.",
    "#..#....#..#",
    "#.#.#..#.#.#",
    "#..........#",
    "#...####...#",
    ".#...##...#.",
    "..########..",
    "...#....#...", NULL };
static const char *BLOB_CHOMP[] = {
    "....####....",
    "..##....##..",
    ".#........#.",
    "#..##..##..#",
    "#..##..##..#",
    "#...####...#",
    "#...#..#...#",
    ".#..####..#.",
    "..########..",
    "...#....#...", NULL };
static const char *BLOB_STRAIN[] = {
    "....####....",
    "..##....##..",
    ".#........#.",
    "#.#......#.#",
    "#..#....#..#",
    "#.#......#.#",
    "#...####...#",
    ".#........#.",
    "..########..",
    "...#....#...", NULL };
static const char *BLOB_DEMON[] = {
    "#..........#",
    "##..####..##",
    ".###....###.",
    ".#........#.",
    "#..hh..hh..#",
    "#..hh..hh..#",
    "#..........#",
    "#.#.####.#.#",
    ".#.#....#.#.",
    "..########..", NULL };
static const char *PENTAGRAM[] = {
    "..........rrrr..........",
    "........rrr..rrr........",
    ".......rr......rr.......",
    "......r.rr....rr.r......",
    ".....r..r.rrrr.r..r.....",
    ".....r...r.rr.r...r.....",
    "....rr...rr..rr...rr....",
    "....r...rr....rr...r....",
    "....r..rrrr..rrrr..r....",
    "....rrr...r..r...rrr....",
    ".....rrrrrrrrrrrrrr.....",
    ".....r....rrrr....r.....",
    "......r....rr....r......",
    ".......r...rr...r.......",
    "........rrrrrrrr........",
    "..........rrrr..........", NULL };
static const char *HORNLETS[] = { ".#........#.", "..#......#..", NULL };
static const char *CUPCAKE[] = {
    "...#...",
    "..###..",
    ".#####.",
    "#######",
    "#.#.#.#",
    ".#.#.#.",
    ".#####.", NULL };
static const char *BITE1[] = { ".....oo", "......o", NULL };
static const char *BITE2[] = { "...oooo", "..ooooo", "....ooo", "......o", NULL };
static const char *BITE3[] = { "ooooooo", "ooooooo", "ooooooo", "ooooooo", "oooo.oo", "oooo.oo", "oooo.oo", NULL };
static const char *HEART[] = { ".#.#.", "#####", ".###.", "..#..", NULL };
static const char *POOP[] = {
    "...#...",
    "..##...",
    ".####..",
    ".#.#.#.",
    "#######",
    "#######", NULL };
static const char *ASH[] = {
    ".......",
    ".......",
    ".......",
    "..#.#..",
    ".#.#.#.",
    "#######", NULL };

/* ---- the device -------------------------------------------------------------- */
static int egg_in(int dx, int dy) {
    float ry = dy < 0 ? 66.0f : 58.0f, rx = 64.0f;
    return (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry) <= 1.0f;
}
static void device(int cx, int cy, int f, int pressed, int icon_sel) {
    uint8_t ol = PAL_RAMP(RAMP_PASTEL_PINK, 1);
    uint8_t body = PAL_PAINT(PAINT_BUBBLEGUM, 2), mid = PAL_PAINT(PAINT_BUBBLEGUM, 1), dk = PAL_PAINT(PAINT_ROSE, 1);
    /* drop shadow + outline + body with shading */
    for (int y = -70; y <= 62; y++)
        for (int x = -68; x <= 68; x++) {
            int in = egg_in(x, y), in1 = egg_in(x + 1, y) || egg_in(x - 1, y) || egg_in(x, y + 1) || egg_in(x, y - 1);
            int sp = egg_in(x - 3, y - 4);
            int px = cx + x, py = cy + y;
            if (in) {
                uint8_t c = body;
                float u = x / 64.0f, v = y / 62.0f;
                float l = u * 0.55f + v * 0.7f;
                if (l > 0.62f) c = dk;
                else if (l > 0.42f) c = ((px + py) & 1) ? mid : dk;
                else if (l > 0.22f) c = mid;
                else if (l > 0.05f && ((px + py) & 1)) c = mid;
                float hx = u + 0.42f, hy = v + 0.55f;
                if (hx * hx * 2.6f + hy * hy * 6.0f < 0.05f) c = PAL_WHITE;
                else if (hx * hx * 2.6f + hy * hy * 6.0f < 0.10f) c = PAL_RAMP(RAMP_PASTEL_PINK, 7);
                tk_px(px, py, c);
            } else if (in1) tk_px(px, py, ol);
            else if (sp && ((px + py) & 1)) tk_px(px, py, PAL_RAMP(RAMP_VAPE, 3));
        }
    /* little printed hearts & stars on the shell */
    static const int deco[][3] = { {-44, 14, 0}, {44, 20, 1}, {-34, -40, 1}, {36, -38, 0}, {-50, -6, 1}, {52, -8, 0} };
    for (int i = 0; i < 6; i++) {
        int dx = cx + deco[i][0], dy = cy + deco[i][1];
        if (deco[i][2]) tk_star(dx, dy, 2, ((f / 10 + i) & 1) ? PAL_WHITE : PAL_PAINT(PAINT_LEMON, 2));
        else tk_heart(dx, dy, 1, PAL_PAINT(PAINT_ROSE, 2));
    }
    /* bezel */
    int bx = cx - 44, by = cy - 48, bw = 88, bh = 68;
    tk_rect(bx + 2, by, bw - 4, bh, ol); tk_rect(bx, by + 2, bw, bh - 4, ol); tk_rect(bx + 1, by + 1, bw - 2, bh - 2, ol);
    tk_rect(bx + 2, by + 1, bw - 4, bh - 2, PAL_UI_CREAM); tk_rect(bx + 1, by + 2, bw - 2, bh - 4, PAL_UI_CREAM);
    tk_rect(bx + 3, by + bh - 3, bw - 6, 1, PAL_RAMP(RAMP_PEACH, 6));
    /* status icons: fork, ball, bulb, heart (top), syringe, toilet, scale, alert (bottom) */
    static const char *IC[8][5] = {
        { "#.#.#", "#.#.#", ".###.", "..#..", "..#.." },
        { ".###.", "#.#.#", "#####", "#.#.#", ".###." },
        { ".###.", "#...#", ".#.#.", ".###.", "..#.." },
        { ".#.#.", "#####", "#####", ".###.", "..#.." },
        { "....#", "..##.", ".##..", "##...", "#...." },
        { ".###.", ".#.#.", "#####", ".###.", ".#.#." },
        { "#####", "..#..", ".###.", "#...#", "#####" },
        { "..#..", "..#..", "..#..", ".....", "..#.." },
    };
    for (int i = 0; i < 8; i++) {
        int ix = bx + 10 + (i % 4) * 20, iy = i < 4 ? by + 3 : by + bh - 9;
        uint8_t c = PAL_RAMP(RAMP_PEACH, 4);
        if (i == icon_sel) {
            tk_rect(ix - 2, iy - 1, 9, 7, PAL_PAINT(PAINT_ROSE, 1));
            c = PAL_WHITE;
        }
        for (int j = 0; j < 5; j++)
            for (int k = 0; k < 5; k++)
                if (IC[i][j][k] == '#') tk_px(ix + k, iy + j, c);
    }
    /* LCD well */
    lcd_x = cx - LW * LPX / 2;
    lcd_y = cy - 38;
    tk_rect(lcd_x - 2, lcd_y - 2, LW * LPX + 4, LH * LPX + 4, PAL_RAMP(RAMP_STONE, 4));
    tk_rect(lcd_x - 1, lcd_y - 1, LW * LPX + 2, LH * LPX + 2, PAL_RAMP(RAMP_STONE, 2));
    /* brand plate */
    tk_text(cx - tk_text_w("DOOMOGACHI", 1) / 2, cy + 23, "DOOMOGACHI", PAL_PAINT(PAINT_ROSE, 0), 1);
    /* buttons */
    static const int bp[3] = { PAINT_LEMON, PAINT_MINT, PAINT_SKY };
    for (int i = 0; i < 3; i++) {
        int x = cx - 22 + i * 22, y = cy + 42, p = (pressed == i + 1);
        tk_circle(x + 1, y + 2, 7, ol);
        tk_circle(x, y + p, 7, ol);
        tk_circle(x, y + p, 6, PAL_PAINT(bp[i], p ? 1 : 2));
        if (!p) { tk_circle(x, y + 2, 5, PAL_PAINT(bp[i], 1)); tk_circle(x, y, 5, PAL_PAINT(bp[i], 2)); }
        tk_rect(x - 3, y - 3 + p, 2, 2, PAL_WHITE);
        static const char *L[3] = { "A", "B", "C" };
        tk_text(x - 2, y + 9, L[i], PAL_PAINT(PAINT_ROSE, 0), 1);
    }
}

/* ---- scene ---------------------------------------------------------------------- */
static void bg(int f) {
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            int u = (x + f / 2) / 16, v = (y + f / 2) / 16;
            tk_fb[y][x] = ((u + v) & 1) ? PAL_RAMP(RAMP_VAPE, 7) : PAL_RAMP(RAMP_PASTEL_PINK, 7);
        }
}
static void bubble(int x, int y, const char *s, int age) {
    int w = tk_text_w(s, 2) + 12, h = 24;
    int dy = age == 0 ? -4 : age == 1 ? 1 : 0;
    y += dy;
    uint8_t ol = PAL_RAMP(RAMP_VAPE, 1);
    tk_rect(x + 1, y, w - 2, h, ol); tk_rect(x, y + 1, w, h - 2, ol);
    tk_rect(x + 2, y + 1, w - 4, h - 2, PAL_WHITE); tk_rect(x + 1, y + 2, w - 2, h - 4, PAL_WHITE);
    tk_tri(x + 4, y + h - 2, x + 12, y + h - 2, x - 6, y + h + 8, ol);
    tk_tri(x + 5, y + h - 3, x + 11, y + h - 3, x - 3, y + h + 5, PAL_WHITE);
    tk_text(x + 6, y + 4, s, PAL_RAMP(RAMP_VAPE, 1), 2);
}
static void cap(int f, int a, int b, int cy, const char *s, int scale, uint8_t c) {
    if (f < a || f >= b) return;
    int age = f - a, dy = age == 0 ? -5 : age == 1 ? 1 : 0;
    tk_caption(cy + dy, s, c, PAL_RAMP(RAMP_VAPE, 1), scale);
}

void scene_s04_doomogachi(int f, int n) {
    (void)n;
    int evil = f >= 128 && f < 132;
    bg(f);

    /* device position: drops in, jiggles with events */
    int cx = 120, cy = 98;
    if (f < 6) cy = 98 - (int)(150 * (1 - (f + 1) / 6.0f) * (1 - (f + 1) / 6.0f) * 1.0f) - (6 - f) * 4;
    else if (f < 12) { static const int B[6] = { 4, 2, -3, -2, 0, 1 }; cy += B[f - 6]; }
    if (f >= 100 && f < 103) cy += f == 100 ? 2 : 1;           /* plop */
    if (evil) { cx += (f & 1) ? 4 : -4; cy += (f & 2) ? -3 : 3; }

    int pressed = (f >= 56 && f < 60) ? 1 : 0;
    int icon = (f >= 56 && f < 80) ? 0 : (f >= 100 && f < 128) ? 5 : (f >= 80 && f < 92) ? 3 : -1;
    if (evil) icon = 7;
    device(cx, cy, f, pressed, icon);

    /* ---- LCD content ---- */
    lcd_clear();
    if (f < 36) {
        int wob = 0;
        if (f >= 10) {
            int amp = f < 20 ? 1 : 1;
            int per = f < 22 ? 6 : 3;
            wob = ((f / per) & 1) ? amp : -amp;
        }
        int ex = 8 + wob, ey = 3 + (f >= 30 && (f & 2) ? -1 : 0);
        lcd_spr(EGG, ex, ey, 0);
        if (f >= 28) lcd_spr(CRACK1, ex, ey + 5, 0);
        if (f >= 32) lcd_spr(CRACK2, ex, ey + 5, 0);
    } else if (f < 40) {
        /* hatch! shell top flies off, sparkle pixels */
        int a = f - 36;
        lcd_spr(SHELL_TOP, 8 + a, 2 - a * 2, 0);
        lcd_spr(BLOB_SQ, 5, 7, 0);
        for (int i = 0; i < 4; i++) { lcd_set(3 - a + i * 0, 5 + i * 2, 1); lcd_set(20 + a, 5 + i * 2, 1); }
    } else {
        int bx = f < 52 ? 6 : f < 58 ? 6 - (f - 52) / 3 : 4, by = 5;
        int hops = f >= 40 && f < 56;
        int bounce = hops ? (((f - 40) / 4) & 1) : 0;
        const char *const *spr = bounce ? BLOB_SQ : BLOB;
        int sx = bounce ? bx - 1 : bx, sy = bounce ? by + 2 : by - (hops ? 1 : 0);
        /* feeding */
        if (f >= 58 && f < 78) {
            int a = f - 58;
            int cxp = 17, cyp = 7;
            lcd_spr(CUPCAKE, cxp, cyp, 0);
            if (a >= 6) lcd_spr(BITE1, cxp, cyp, 0);
            if (a >= 12) lcd_spr(BITE2, cxp, cyp, 0);
            if (a >= 18) lcd_spr(BITE3, cxp, cyp, 0);
            spr = ((a % 6) >= 3) ? BLOB_CHOMP : BLOB;
            sx = bx + 1; sy = by;
        } else if (f >= 78 && f < 92) {
            spr = BLOB_HAPPY;
            int a = f - 78;
            sy = by - ((a / 3) & 1);
            lcd_spr(HEART, 17, 6 - a / 3, 0);
            if (a > 4) lcd_spr(HEART, 19, 11 - (a - 4) / 2, 0);
        } else if (f >= 92 && f < 100) {
            spr = BLOB_STRAIN;
            sx = bx + ((f & 1) ? 1 : 0);
            lcd_set(bx - 1, by + 1 + (f & 2 ? 0 : 1), 1); /* sweat */
        } else if (f >= 100 && f < 128) {
            spr = (f < 110) ? BLOB_HAPPY : BLOB;
            if ((f / 24) % 2 == 1 && (f % 24) > 20) spr = BLOB;
        }
        if (evil) {
            /* 4 frames of hell: 2 of pentagram, 2 of the true face */
            if (f < 130) {
                lcd_spr(PENTAGRAM, 0, 0, 0);
            } else {
                lcd_spr(BLOB_DEMON, 6, 3, 0);
                lcd_set(5, 2, 1); lcd_set(18, 2, 1); /* horn tips */
            }
        } else {
            if (f >= 132) lcd_spr(HORNLETS, sx, sy - 2, 0); /* ...those were not there before */
            lcd_spr(spr, sx, sy, 0);
        }
        /* the poop / the ash */
        if (f >= 100 && !evil) {
            int px = 17, py = 10;
            if (f < 128) {
                lcd_spr(POOP, px, py, 0);
                int ph = (f / 5) & 1;
                for (int k = 0; k < 2; k++) {
                    int sx0 = px + 1 + k * 4;
                    lcd_set(sx0 + ph, py - 2, 1);
                    lcd_set(sx0 + 1 - ph, py - 3, 1);
                    lcd_set(sx0 + ph, py - 4, 1);
                    lcd_set(sx0 + 1 - ph, py - 5, 1);
                }
            } else {
                lcd_spr(ASH, px, py, 0);
                int ph = (f / 4) % 4;
                lcd_set(px + 3 + (ph == 1), py + 1 - ph / 2, 1);
                if (ph > 1) lcd_set(px + 2 + (ph == 3), py - ph / 2, 1);
            }
        }
    }
    if (!evil) lcd_render(0);

    /* ---- captions & bubbles ---- */
    if (f >= 6 && f < 56) {
        int age = f - 6, dy = age == 0 ? -5 : age == 1 ? 1 : 0;
        tk_caption(9 + dy, "Raise your very own", PAL_WHITE, PAL_RAMP(RAMP_VAPE, 1), 1);
        if (f >= 12) {
            int a2 = f - 12, d2 = a2 == 0 ? -6 : a2 == 1 ? 2 : 0;
            tk_caption(24 + d2, "DOOMOGACHI!", PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_RAMP(RAMP_VAPE, 1), 2);
        }
    }
    if (f >= 40 && f < 56) bubble(168, 50, "beep.", f - 40);
    cap(f, 102, 128, 16, "It loves you!", 2, PAL_WHITE);
    if (f >= 134 && f < 152) bubble(168, 50, "beep.", f - 134);
    cap(f, 142, 999, 16, "...it's fine.", 2, PAL_WHITE);

    if (evil) {
        /* hard cut: the LCD fills the whole screen. 24x16 virtual px at 10 px each */
        int sx = (f & 1) ? 3 : -3, sy = (f & 2) ? -2 : 2;
        lcd_render_at(sx, sy, 10, 1);
        if (f & 1) tk_post_glitch(f * 13, 12);
    }
}

void scene_s04_doomogachi_audio(double t0, double dur) {
    (void)dur;
    double fr = 1.0 / TK_FPS;
    tk_sfx(t0 + 6 * fr, SFX_THUD, 0.7f);
    tk_sfx(t0 + 7 * fr, SFX_POP, 0.4f);
    for (int k = 0; k < 6; k++) tk_note(t0 + (10 + k * 4) * fr, 0.04, 76 + k, TKV_SQ50, 0.18f);   /* wobble ticks */
    tk_sfx(t0 + 28 * fr, SFX_BLIP, 0.6f);
    tk_sfx(t0 + 32 * fr, SFX_BLIP, 0.7f);
    tk_sfx(t0 + 36 * fr, SFX_POP, 1.0f);
    tk_sfx(t0 + 37 * fr, SFX_SPARKLE, 0.6f);
    tk_note(t0 + 40 * fr, 0.12, 88, TKV_SQ50, 0.32f);                                             /* beep. */
    tk_sfx(t0 + 56 * fr, SFX_SELECT, 0.6f);
    for (int k = 0; k < 3; k++) tk_slide(t0 + (61 + k * 6) * fr, 0.07, 64, 52, TKV_SQ25, 0.35f);  /* nom nom nom */
    tk_note(t0 + 78 * fr, 0.08, 84, TKV_SQ25, 0.25f);
    tk_note(t0 + 81 * fr, 0.08, 88, TKV_SQ25, 0.25f);
    tk_note(t0 + 84 * fr, 0.16, 91, TKV_SQ25, 0.25f);
    tk_slide(t0 + 92 * fr, 0.26, 48, 60, TKV_TRI, 0.35f);                                        /* nnnngh */
    tk_slide(t0 + 100 * fr, 0.16, 50, 36, TKV_SAW, 0.4f);                                        /* pfrrt */
    tk_sfx(t0 + 100 * fr, SFX_SPLAT, 0.25f);
    tk_sfx(t0 + 102 * fr, SFX_SPARKLE, 0.35f);
    /* four frames of hell */
    tk_slide(t0 + 128 * fr, 4 * fr, 33, 28, TKV_SAW, 0.9f);
    tk_note(t0 + 128 * fr, 4 * fr, 34, TKV_SQ12, 0.6f);
    tk_note(t0 + 128 * fr, 4 * fr, 40, TKV_SQ12, 0.5f);
    tk_sfx(t0 + 128 * fr, SFX_GLITCH, 0.6f);
    tk_note(t0 + 134 * fr, 0.12, 88, TKV_SQ50, 0.32f);                                            /* beep. */
}
