/* cook_gfx.c - procedural art of the Brimstone Bites kitchen (owner:
 * fishcook): the cosy kitchen (striped wallpaper, a window on the sky,
 * jars, swaying copper pans, a hellfire-heart sign), the counter, Granny
 * Hopscotch the plush bunny granny chef, every dish and the ingredients.
 * All spans through the UI primitives; everything breathes, sways,
 * steams or twinkles. */
#include "fishcook_int.h"

#define P(p)  PAL_PAINT(PAINT_##p, 1)
#define PL(p) PAL_PAINT(PAINT_##p, 2)
#define PD(p) PAL_PAINT(PAINT_##p, 0)
#define R(r, s) PAL_RAMP(RAMP_##r, s)

static uint8_t dk(uint8_t c) { return ui_darker(c); }
static uint8_t lt(uint8_t c) { return ui_lighter(c); }

/* an outlined ellipse: outline, fill, light top */
static void blob(int cx, int cy, int rx, int ry, uint8_t c)
{
    ui_fill_ellipse(cx, cy, rx + 1, ry + 1, dk(dk(c)));
    ui_fill_ellipse(cx, cy, rx, ry, c);
    if (rx >= 3 && ry >= 3) ui_fill_ellipse(cx - rx / 3, cy - ry / 3, DOOMO_MAX(1, rx / 3), DOOMO_MAX(1, ry / 4), lt(c));
}

void kitchen_steam(int cx, int cy, int t, int n)
{
    int k;
    for (k = 0; k < n; k++) {
        int p = (t + k * 17) % 48, x = cx - 6 + k * 6 + ui_wave(t + k * 30, 40, 2), y = cy - p / 2;
        if (p < 40) plat_checker(x, y, 2, 3, p < 20 ? PAL_WHITE : R(STONE, 6));
    }
}

/* ====================================================================== */
/* the kitchen                                                              */
void kitchen_draw_back(uint32_t tt, int busy)
{
    int w = fc_w(), h = fc_h(), t = (int)tt, x, k;
    int night = town_is_night();
    uint8_t s1 = R(PASTEL_PINK, 7), s2 = PL(PEACH);
    if (plat_draw_skipped()) return;
    /* striped wallpaper with tiny hearts */
    plat_rect(0, 0, w, h, s1);
    for (x = 0; x < w; x += 16) plat_rect(x, 0, 8, h, s2);
    for (x = 12; x < w; x += 32)                          /* tiny hearts */
        for (k = 14 + ((x / 32) & 1) * 16; k < h * 2 / 5; k += 32) {
            plat_rect(x - 2, k, 2, 2, R(PASTEL_PINK, 6));
            plat_rect(x + 1, k, 2, 2, R(PASTEL_PINK, 6));
            plat_rect(x - 1, k + 2, 3, 1, R(PASTEL_PINK, 6));
        }
    /* chair rail */
    plat_rect(0, h * 44 / 100, w, 3, R(WOOD, 5));
    plat_rect(0, h * 44 / 100 + 3, w, 1, R(WOOD, 3));
    /* mint tiles below the rail: one fill, grout lines, a glint per tile row */
    {
        int y0 = h * 44 / 100 + 4;
        plat_rect(0, y0, w, h - y0, R(MINT, 6));
        for (x = 11; x < w; x += 12) plat_rect(x, y0, 1, h - y0, R(MINT, 5));
        for (k = y0 + 11; k < h; k += 12) {
            plat_rect(0, k, w, 1, R(MINT, 5));
            plat_rect(((k / 12) * 37) % (w - 12) + 2, k - 9, 3, 1, R(MINT, 7));
        }
    }
    /* the window (day or night outside) */
    {
        int wx = w * 6 / 100, wy = 8, ww = w * 22 / 100, wh = h * 26 / 100;
        ui_round_rect(wx - 2, wy - 2, ww + 4, wh + 4, 4, R(WOOD, 3));
        ui_round_rect(wx, wy, ww, wh, 3, night ? R(SKY, 1) : R(SKY, 6));
        plat_rect(wx, wy + wh * 2 / 3, ww, wh / 3, night ? R(VAPE, 1) : R(PASTEL_PINK, 6));
        if (night) { ui_fill_circle(wx + ww - 8, wy + 7, 3, R(CANDY, 7)); ui_sparkle(wx + 6, wy + 6, (t / 3) & 15, PAL_WHITE); }
        else { ui_fill_circle(wx + ww - 9, wy + 8, 4, PL(LEMON)); ui_fill_ellipse(wx + 9 + (t / 20) % 10, wy + 10, 5, 2, PAL_WHITE); }
        plat_rect(wx + ww / 2, wy, 1, wh, R(WOOD, 4));
        plat_rect(wx, wy + wh / 2, ww, 1, R(WOOD, 4));
        /* scalloped curtains */
        for (k = 0; k < ww + 4; k += 6) {
            ui_fill_circle(wx - 2 + k, wy - 1, 3, P(BUBBLEGUM));
            plat_rect(wx - 5 + k, wy - 4, 6, 2, P(BUBBLEGUM));
        }
        /* sill + a potted flower */
        plat_rect(wx - 4, wy + wh + 2, ww + 8, 3, R(WOOD, 5));
        plat_rect(wx + ww - 10, wy + wh - 5, 7, 7, P(RUST));
        ui_fill_circle(wx + ww - 7, wy + wh - 9 + ui_wave(t, 80, 1), 3, P(CHERRY));
        plat_rect(wx + ww - 7, wy + wh - 9 + ui_wave(t, 80, 1), 1, 1, PL(LEMON));
    }
    /* a shelf of jars (and one skull cookie jar) */
    {
        int sx = w * 70 / 100, sy = h * 24 / 100, sw = w * 26 / 100;
        static const uint8_t lid[4] = { P(CHERRY), P(MINT), P(LEMON), P(GRAPE) };
        plat_rect(sx, sy, sw, 3, R(WOOD, 5));
        plat_rect(sx, sy + 3, sw, 1, R(WOOD, 2));
        for (k = 0; k < 4; k++) {
            int jx = sx + 5 + k * (sw - 10) / 4, jh = 8 + (k & 1) * 3;
            if (k == 2) {                                    /* skull jar */
                ui_fill_circle(jx + 3, sy - 6, 5, PL(BONE));
                plat_rect(jx + 1, sy - 7, 2, 2, R(STONE, 0));
                plat_rect(jx + 4, sy - 7, 2, 2, R(STONE, 0));
                plat_rect(jx + 2, sy - 3, 3, 2, PL(BONE));
                continue;
            }
            ui_round_rect(jx, sy - jh, 7, jh, 2, R(SKY, 7));
            plat_rect(jx + 1, sy - jh + 3, 5, jh - 4, lt(lid[k]));
            plat_rect(jx, sy - jh - 1, 7, 2, lid[k]);
            plat_rect(jx + 1, sy - jh + 1, 1, jh - 2, PAL_WHITE);
        }
    }
    /* copper pans swinging from a rail (not while a minigame needs the space) */
    if (!busy) {
        int rx0 = w * 34 / 100, rx1 = w * 64 / 100, ry = h * 30 / 100;
        plat_rect(rx0, ry - 12, rx1 - rx0, 2, R(STEEL, 3));
        for (k = 0; k < 3; k++) {
            int px = rx0 + 10 + k * (rx1 - rx0 - 20) / 2, sw = ui_wave(t + k * 25, 120, 2);
            plat_line(px, ry - 10, px + sw, ry - 4, R(STEEL, 2));
            ui_fill_circle(px + sw, ry + 1, 5 + (k == 1), R(WOOD, 2));
            ui_fill_circle(px + sw, ry + 1, 4 + (k == 1), P(TANGERINE));
            plat_rect(px + sw - 2, ry - 2, 2, 1, PL(PEACH));
        }
    }
}

void kitchen_draw_counter(int y, uint32_t tt)
{
    int w = fc_w(), h = fc_h(), x, t = (int)tt;
    if (plat_draw_skipped()) return;
    plat_rect(0, y, w, 6, R(WOOD, 5));
    plat_rect(0, y, w, 1, R(WOOD, 6));
    plat_rect(0, y + 6, w, 2, R(WOOD, 2));
    plat_rect(0, y + 8, w, h - y - 8, P(BUBBLEGUM));
    for (x = 6; x < w - 20; x += 44) {
        ui_round_rect(x, y + 13, 38, h - y - 16, 4, PD(BUBBLEGUM));
        ui_round_rect(x + 1, y + 14, 36, h - y - 18, 3, PL(BUBBLEGUM));
        ui_fill_circle(x + 19, y + 18, 2, PAL_UI_GOLD);
    }
    /* Brimstone Bites' logo on the counter: a heart on fire */
    if (h - y > 34) {
        int cx = w / 2, cy = y + 27, k;
        for (k = -1; k <= 1; k++)
            plat_rect(cx + k * 4 - 1, cy - 8 - ((t / 4 + k) % 3) - (k == 0) * 2, 3, 4 + (k == 0) * 2, R(LAVA, 6));
        ui_heart(cx, cy, 5, P(CHERRY));
    }
}

/* ====================================================================== */
/* Granny Hopscotch                                                         */
void granny_draw(int cx, int cy, int s, int t, int mood)
{
    /* s = px per 16 units; cy = where her chest meets the counter */
    int u = DOOMO_MAX(6, s), br = ui_wave(t, 70, 1), sway = ui_wave(t + 20, 90, 2);
    int hx = cx, hy = cy - u * 30 / 16 + br, hr = u * 15 / 16, k;
    uint8_t fur = R(CANDY, 7), fur_d = R(PEACH, 6), fur_o = R(PEACH, 3);
    uint8_t inner = PL(BUBBLEGUM), ink = R(STONE, 0);
    int blink = (t % 120) < 5, talk = mood == GRAN_TALK && ((t / 4) & 1);
    if (plat_draw_skipped()) return;
    /* body: lavender dress, white apron with a heart pocket */
    ui_fill_ellipse(cx, cy + 2, u * 17 / 16 + 1, u * 14 / 16 + 1 - br, PD(LAVENDER));
    ui_fill_ellipse(cx, cy + 2, u * 17 / 16, u * 14 / 16 - br, P(LAVENDER));
    ui_fill_ellipse(cx, cy + 6, u * 11 / 16, u * 12 / 16 - br, PAL_WHITE);
    for (k = -2; k <= 2; k++) ui_fill_circle(cx + k * u / 4, cy - u * 7 / 16 + br, u / 8 + 1, PAL_WHITE); /* frilly collar */
    ui_heart(cx, cy + u * 3 / 8, DOOMO_MAX(2, u / 5), PL(ROSE));
    /* paws: one holds the wooden spoon (raised to cheer, wagged to talk) */
    {
        int px = cx + u * 13 / 16, py = cy - u / 8, ang;
        int wave = mood == GRAN_CHEER ? ui_wave(t * 4, 30, 5) : mood == GRAN_TALK ? ui_wave(t * 2, 40, 2) : 0;
        int sx = px + 2 + wave / 2, sy = py - u * 12 / 16 - (mood == GRAN_CHEER ? u / 2 : 0) + wave;
        ang = 0;
        (void)ang;
        plat_line(px, py, sx, sy, R(WOOD, 3));
        plat_line(px + 1, py, sx + 1, sy, R(WOOD, 4));
        ui_fill_ellipse(sx, sy - 2, 2 + u / 16, 3 + u / 12, R(WOOD, 4));
        plat_rect(sx - 1, sy - 4, 1, 2, R(WOOD, 6));
        blob(px, py, u / 4 + 1, u / 4, fur);
        blob(cx - u * 13 / 16, cy - u / 8 + (mood == GRAN_DISMAY ? -u / 2 : 0), u / 4 + 1, u / 4, fur);
    }
    /* lop ears hang down beside her face */
    ui_fill_ellipse(hx - hr + 1, hy + u * 4 / 16 + sway / 2, u * 5 / 16 + 1, u * 13 / 16 + 1, fur_o);
    ui_fill_ellipse(hx - hr + 1, hy + u * 4 / 16 + sway / 2, u * 5 / 16, u * 13 / 16, fur);
    ui_fill_ellipse(hx - hr + 2, hy + u * 6 / 16 + sway / 2, u * 2 / 16, u * 9 / 16, inner);
    ui_fill_ellipse(hx + hr - 1, hy + u * 4 / 16 - sway / 2, u * 5 / 16 + 1, u * 13 / 16 + 1, fur_o);
    ui_fill_ellipse(hx + hr - 1, hy + u * 4 / 16 - sway / 2, u * 5 / 16, u * 13 / 16, fur);
    ui_fill_ellipse(hx + hr - 2, hy + u * 6 / 16 - sway / 2, u * 2 / 16, u * 9 / 16, inner);
    /* the grey bun with a cherry hairpin */
    blob(hx, hy - hr + 1, u * 6 / 16, u * 5 / 16, R(STONE, 6));
    ui_fill_circle(hx + u * 5 / 16, hy - hr - u * 3 / 16, 2, P(CHERRY));
    plat_line(hx + u * 2 / 16, hy - hr, hx + u * 6 / 16, hy - hr - u * 4 / 16, R(WOOD, 3));
    /* head (with cheek tufts) */
    ui_fill_ellipse(hx, hy, hr + 1, hr * 14 / 16 + 1, fur_o);
    ui_fill_circle(hx - hr + 1, hy + hr / 3, u / 6 + 1, fur_o);
    ui_fill_circle(hx + hr - 1, hy + hr / 3, u / 6 + 1, fur_o);
    ui_fill_ellipse(hx, hy, hr, hr * 14 / 16, fur);
    ui_fill_circle(hx - hr + 1, hy + hr / 3, u / 6, fur);
    ui_fill_circle(hx + hr - 1, hy + hr / 3, u / 6, fur);
    ui_fill_ellipse(hx + hr / 4, hy + hr / 2, hr * 2 / 3, hr / 3, fur_d);          /* soft shade */
    ui_fill_ellipse(hx, hy + hr / 3, hr * 3 / 4, hr / 3, fur);
    plat_rect(hx - hr / 2, hy - hr * 13 / 16, hr, 1, PAL_WHITE);                     /* rim light */
    /* muzzle, nose, mouth */
    ui_fill_ellipse(hx, hy + hr * 5 / 16, hr / 3, hr / 5, PAL_WHITE);
    ui_fill_ellipse(hx, hy + hr / 6, 2, 1, PL(ROSE));
    plat_rect(hx - 1, hy + hr / 6 - 1, 1, 1, PAL_WHITE);
    if (talk || mood == GRAN_CHEER) {
        ui_fill_ellipse(hx, hy + hr * 7 / 16, 2, 2, PD(ROSE));
        plat_rect(hx - 1, hy + hr * 7 / 16 + 1, 3, 1, P(ROSE));
    } else if (mood == GRAN_DISMAY) {
        plat_rect(hx - 2, hy + hr * 7 / 16, 5, 1, ink);
    } else {
        plat_rect(hx - 2, hy + hr * 6 / 16, 1, 1, ink);
        plat_rect(hx - 1, hy + hr * 7 / 16, 1, 1, ink);
        plat_rect(hx, hy + hr * 6 / 16, 1, 1, ink);
        plat_rect(hx + 1, hy + hr * 7 / 16, 1, 1, ink);
        plat_rect(hx + 2, hy + hr * 6 / 16, 1, 1, ink);
    }
    /* blush */
    plat_checker(hx - hr * 3 / 4, hy + hr / 4, u / 4 + 1, 2, PL(ROSE));
    plat_checker(hx + hr * 3 / 4 - u / 4, hy + hr / 4, u / 4 + 1, 2, PL(ROSE));
    /* eyes behind round gold spectacles: always smiling */
    {
        int ex = hr * 6 / 16, ey = hy - hr / 8, er = DOOMO_MAX(3, u * 4 / 16);
        for (k = -1; k <= 1; k += 2) {
            int x = hx + k * ex;
            ui_fill_circle(x, ey, er, PAL_UI_GOLD);
            ui_fill_circle(x, ey, er - 1, R(SKY, 7));
            if (mood == GRAN_DISMAY) {
                plat_line(x - 2, ey - 2, x + 2, ey + 2, ink);
                plat_line(x - 2, ey + 2, x + 2, ey - 2, ink);
            } else if (mood == GRAN_CHEER) {
                ui_sparkle(x, ey, 4, ink);
            } else if (blink) {
                plat_rect(x - 2, ey, 5, 1, ink);
            } else {
                plat_rect(x - 2, ey + 1, 1, 1, ink);               /* ^ ^ */
                plat_rect(x - 1, ey, 1, 1, ink);
                plat_rect(x, ey - 1, 1, 1, ink);
                plat_rect(x + 1, ey, 1, 1, ink);
                plat_rect(x + 2, ey + 1, 1, 1, ink);
            }
            plat_rect(x - er + 2, ey - er + 2, 1, 1, PAL_WHITE);
        }
        plat_rect(hx - ex + er, ey - 1, 2 * ex - 2 * er + 1, 1, PAL_UI_GOLD);           /* bridge */
    }
    if (mood == GRAN_DISMAY) ui_fill_circle(hx + hr, hy - hr / 2 + (t / 5) % 4, 1, PL(SKY));
}

/* ====================================================================== */
/* dishes                                                                   */
static void plate(int cx, int cy, int rw)
{
    ui_fill_ellipse(cx, cy + 1, rw + 2, rw / 3 + 2, R(STONE, 3));
    ui_fill_ellipse(cx, cy, rw + 2, rw / 3 + 1, PAL_WHITE);
    ui_fill_ellipse(cx, cy, rw - 2, rw / 4, R(SKY, 7));
    plat_rect(cx - rw / 2, cy - rw / 3, rw / 3, 1, PAL_WHITE);
}

void dish_draw(int r, int cx, int cy, int rw, int t, int burnt)
{
    const Recipe *rc;
    uint8_t c1, c2, c3;
    int k, art;
    if (r < 0 || r >= RECIPE_COUNT || plat_draw_skipped()) return;
    rc = &cook_recipes[r];
    c1 = rc->c1; c2 = rc->c2; c3 = rc->c3;
    art = burnt ? DISH_LUMP : rc->art;
    if (burnt) { c1 = P(JET); c2 = P(HELLFIRE); c3 = P(CHARCOAL); }
    rw = DOOMO_MAX(rw, 6);
    switch (art) {
    case DISH_CUPCAKE: {
        int wy = cy - rw / 6, hh = rw * 2 / 3;
        plate(cx, cy + rw / 3, rw);
        for (k = 0; k < hh; k++) {                           /* wrapper */
            int hw = rw / 2 + k * rw / (4 * hh) ;
            plat_rect(cx - hw, wy + k, hw * 2, 1, (k == 0) ? dk(c2) : c2);
        }
        for (k = -rw / 2 + 2; k < rw / 2; k += 3) plat_rect(cx + k, wy + 1, 1, hh - 1, dk(c2));
        blob(cx, wy - rw / 5, rw * 3 / 5, rw / 3, c1);
        blob(cx + 1, wy - rw / 2, rw / 3, rw / 4, c1);
        for (k = 0; k < 6; k++)
            plat_rect(cx - rw / 3 + (k * 7) % (rw * 2 / 3), wy - rw / 3 + (k * 5) % (rw / 3 + 1), 2, 1,
                      k % 3 == 0 ? P(MINT) : k % 3 == 1 ? P(LEMON) : P(SKY));
        ui_fill_circle(cx + 1, wy - rw * 3 / 4, DOOMO_MAX(2, rw / 6), c3);
        plat_rect(cx, wy - rw * 3 / 4 - 1, 1, 1, PAL_WHITE);
        plat_line(cx + 1, wy - rw * 3 / 4 - 2, cx + 3, wy - rw * 3 / 4 - 5, P(FOREST));
        break; }
    case DISH_BOWL: case DISH_SALAD: {
        int by = cy;
        uint8_t bowl = art == DISH_SALAD ? PAL_WHITE : c3;
        ui_fill_ellipse(cx, by + rw / 4, rw * 9 / 10 + 1, rw / 2 + 1, dk(dk(bowl)));
        ui_fill_ellipse(cx, by + rw / 4, rw * 9 / 10, rw / 2, bowl);
        plat_rect(cx - rw * 9 / 10, by - rw / 6, rw * 9 / 5 + 1, rw / 3, art == DISH_SALAD ? R(STONE, 6) : bowl);
        ui_fill_ellipse(cx, by - rw / 6, rw, rw / 4 + 1, dk(dk(bowl)));
        ui_fill_ellipse(cx, by - rw / 6, rw - 1, rw / 4, art == DISH_SALAD ? P(FOREST) : c1);
        if (art == DISH_SALAD) {
            for (k = 0; k < 5; k++) blob(cx - rw / 2 + k * rw / 4, by - rw / 4 - (k & 1) * 2, rw / 6 + 1, rw / 8 + 1, k & 1 ? c1 : c2);
            blob(cx - rw / 4, by - rw / 3, rw / 6, rw / 8, c3);
            blob(cx + rw / 3, by - rw / 3, rw / 6, rw / 8, c3);
        } else {
            for (k = 0; k < 3; k++) {                         /* noodles / bits */
                int yy = by - rw / 6 - 1 + (k - 1) * 2;
                plat_line(cx - rw / 2, yy, cx + rw / 2, yy + ui_wave(t + k * 20, 40, 1), c2);
            }
            if (r == 9 || r == 21) {                         /* half an egg */
                ui_fill_ellipse(cx + rw / 3, by - rw / 5, rw / 6 + 1, rw / 8 + 1, PAL_WHITE);
                ui_fill_circle(cx + rw / 3, by - rw / 5, DOOMO_MAX(1, rw / 10), PL(TANGERINE));
            }
            if (r == 21) {                                   /* the sock! */
                plat_rect(cx - rw / 3, by - rw / 2, rw / 5, rw / 3, P(VAPE_PURPLE));
                plat_rect(cx - rw / 3, by - rw / 2 + 2, rw / 5, 1, PAL_WHITE);
            }
            kitchen_steam(cx, by - rw / 3, t, 3);
        }
        plat_rect(cx - rw / 2, by + rw / 8, rw / 4, 1, lt(bowl));
        break; }
    case DISH_STACK: {
        plate(cx, cy + rw / 3, rw);
        for (k = 0; k < 4; k++) blob(cx + (k & 1), cy + rw / 5 - k * rw / 6, rw * 3 / 4, rw / 5, k == 3 ? lt(c1) : c1);
        plat_rect(cx - rw / 6, cy - rw / 2 - 1, rw / 3, rw / 5 + 1, c2);                      /* butter */
        plat_rect(cx - rw / 6, cy - rw / 2 - 1, rw / 3, 1, lt(c2));
        for (k = 0; k < 3; k++)                                                               /* syrup */
            plat_rect(cx - rw / 2 + k * rw / 2, cy - rw / 3, 2, rw / 4 + (t / 10 + k) % 3, c3);
        kitchen_steam(cx, cy - rw / 2, t, 2);
        break; }
    case DISH_PLATE: {
        plate(cx, cy + rw / 4, rw);
        blob(cx - rw / 3, cy, rw / 2, rw / 3, c2);                                           /* rice */
        blob(cx + rw / 4, cy + 1, rw / 2, rw / 4, c1);                                        /* curry */
        for (k = 0; k < 3; k++) plat_rect(cx + k * rw / 6, cy - 1 + (k & 1), 2, 2, c3);
        kitchen_steam(cx + rw / 4, cy - rw / 4, t, 2);
        break; }
    case DISH_ROLL: {
        plate(cx, cy + rw / 4, rw);
        for (k = 0; k < 3; k++) {
            int x = cx - rw / 2 + k * rw / 2, y = cy - (k == 1) * 2;
            ui_fill_circle(x, y, rw / 4 + 1, c2);
            ui_fill_circle(x, y, rw / 4 - 1, c1);
            ui_fill_circle(x, y, DOOMO_MAX(1, rw / 9), c3);
            plat_rect(x - rw / 4 + 1, y - rw / 4, 2, 1, lt(c1));
        }
        break; }
    case DISH_BALLS: {
        plate(cx, cy + rw / 4, rw);
        for (k = 0; k < 5; k++) {
            int x = cx - rw / 2 + (k % 3) * rw / 2 + (k >= 3) * rw / 4, y = cy - (k >= 3) * rw / 3;
            int br = DOOMO_MAX(2, rw / 4);
            blob(x, y, br, br, c1);
            ui_fill_circle(x + 1, y, DOOMO_MAX(1, br / 2), c2);
            plat_rect(x + 1, y, 1, 1, k & 1 ? c3 : PAL_WHITE);
        }
        break; }
    case DISH_PIZZA: {
        plate(cx, cy + 2, rw);
        ui_fill_ellipse(cx, cy, rw, rw / 3, dk(c1));
        ui_fill_ellipse(cx, cy - 1, rw - 1, rw / 3 - 1, c1);
        ui_fill_ellipse(cx, cy - 1, rw - 3, rw / 3 - 2, c2);
        for (k = 0; k < 5; k++) {
            int x = cx - rw / 2 + (k * 7) % rw, y = cy - 2 + (k * 3) % 4 - 1;
            ui_fill_ellipse(x, y, 2, 1, c3);
            plat_rect(x, y, 1, 1, lt(c3));
        }
        /* a slice lifted, cheese stretching */
        plat_line(cx + rw / 3, cy - 2, cx + rw / 2, cy - rw / 2 - ui_wave(t, 40, 2), PL(LEMON));
        break; }
    case DISH_CAKE: {
        int ch = rw * 3 / 4, top = cy - ch / 2;
        plate(cx, cy + ch / 2, rw);
        ui_round_rect(cx - rw * 3 / 4 - 1, top - 1, rw * 3 / 2 + 2, ch + 2, 3, dk(dk(c1)));
        ui_round_rect(cx - rw * 3 / 4, top, rw * 3 / 2, ch, 3, c1);
        plat_rect(cx - rw * 3 / 4 + 1, top + ch / 2, rw * 3 / 2 - 2, 1, dk(c1));
        plat_rect(cx - rw * 3 / 4, top, rw * 3 / 2, 2, c2);                                  /* icing */
        for (k = 0; k < 5; k++)
            plat_rect(cx - rw * 3 / 4 + 2 + k * rw * 3 / 10, top + 2, 2, 2 + (k * 3 + t / 12) % 4, c2);
        if (rc->item == ITEM_GRAVE_CAKE) {
            ui_round_rect(cx - 3, top - 8, 7, 8, 2, c3);
            plat_rect(cx - 1, top - 6, 3, 1, dk(c3));
            plat_rect(cx, top - 7, 1, 3, dk(c3));
            plat_line(cx - rw / 2, top - 1, cx - rw / 3, top - 3 + ui_wave(t, 30, 1), c2);
        } else if (rc->item == ITEM_LAVA_CAKE) {
            for (k = 0; k < 4; k++) plat_rect(cx - 1 + ui_wave(t + k * 10, 20, 1), top - 2 - k * 2, 3 - k / 2, 2, k < 2 ? c2 : c3);
        } else {
            int hy = top - 6 + ui_wave(t, 50, 1);
            ui_fill_ellipse(cx, hy, rw / 3, 2, PAL_UI_GOLD);
            ui_fill_ellipse(cx, hy, rw / 3 - 2, 1, c1);
        }
        break; }
    case DISH_JELLY: {
        int sq = ui_wave(t * 3, 30, 2);
        plate(cx, cy + rw / 3, rw);
        ui_fill_ellipse(cx, cy, rw * 2 / 3 + sq + 1, rw / 2 - sq + 1, dk(c1));
        ui_fill_ellipse(cx, cy, rw * 2 / 3 + sq, rw / 2 - sq, c1);
        ui_fill_ellipse(cx - rw / 4, cy - rw / 4, rw / 6, rw / 8, lt(lt(c1)));
        for (k = -1; k <= 1; k += 2) {
            ui_fill_circle(cx + k * rw / 4, cy + 1, DOOMO_MAX(2, rw / 6), c2);
            ui_fill_circle(cx + k * rw / 4 + 1, cy + 1, DOOMO_MAX(1, rw / 10), c3);
        }
        break; }
    case DISH_BAGEL: {
        plate(cx, cy + rw / 3, rw);
        ui_fill_ellipse(cx, cy, rw * 3 / 4 + 1, rw / 2 + 1, dk(dk(c1)));
        ui_fill_ellipse(cx, cy, rw * 3 / 4, rw / 2, c1);
        ui_fill_ellipse(cx - 1, cy - 2, rw / 2, rw / 4, lt(c1));
        ui_fill_ellipse(cx, cy, rw / 5, rw / 8 + 1, R(SKY, 7));
        for (k = 0; k < 6; k++)
            plat_rect(cx - rw / 2 + (k * 11) % rw, cy - rw / 4 + (k * 5) % (rw / 2 + 1), 1 + (k & 1), 1, k & 1 ? c2 : c3);
        break; }
    case DISH_STEAK: {
        plate(cx, cy + rw / 4, rw);
        blob(cx, cy, rw * 3 / 4, rw / 3, c1);
        for (k = -1; k <= 1; k++) plat_line(cx + k * rw / 3 - 3, cy - 2, cx + k * rw / 3 + 2, cy + 3, dk(dk(c1)));
        plat_rect(cx - rw / 3, cy - rw / 5, rw / 4, 1, c2);
        blob(cx + rw / 2, cy - rw / 4, 2, 2, c3);
        kitchen_steam(cx, cy - rw / 3, t, 2);
        break; }
    case DISH_PUDDING: {
        int sq = ui_wave(t * 2, 40, 1), ph = rw * 3 / 5;
        plate(cx, cy + ph / 2, rw);
        for (k = 0; k < ph; k++) {
            int hw = rw / 3 + k * rw / (3 * ph) + sq;
            plat_rect(cx - hw - 1, cy - ph / 2 + k, hw * 2 + 2, 1, dk(c1));
            plat_rect(cx - hw, cy - ph / 2 + k, hw * 2, 1, k < 3 ? c2 : c1);
        }
        plat_rect(cx - rw / 3, cy - ph / 2 + 3, 2, 3 + (t / 15) % 2, c2);
        if (c1 == P(NAVY)) {                                 /* starlight */
            ui_sparkle(cx - 2, cy, (t / 3) & 15, c2);
            ui_sparkle(cx + rw / 4, cy + 3, (t / 3 + 7) & 15, c2);
        }
        ui_fill_circle(cx, cy - ph / 2 - 2, DOOMO_MAX(2, rw / 6), c3);
        ui_fill_circle(cx + 1, cy - ph / 2 - 3 - rw / 6, 2, P(CHERRY));
        break; }
    default: {                                                /* DISH_LUMP */
        plate(cx, cy + rw / 4, rw);
        blob(cx, cy - 1, rw * 2 / 3, rw / 3 + 1, c1);
        blob(cx + rw / 4, cy - rw / 4, rw / 3, rw / 4, c3);
        for (k = 0; k < 4; k++)
            plat_rect(cx - rw / 3 + k * rw / 5, cy - 1 + (k & 1) * 2, 2, 1, ((t / 5 + k) & 1) ? c2 : R(LAVA, 6));
        for (k = 0; k < 3; k++) {
            int p = (t + k * 16) % 48;
            ui_fill_circle(cx - 4 + k * 4 + p / 12, cy - rw / 3 - p / 2, 1 + p / 16, p < 30 ? R(STONE, 3) : R(STONE, 5));
        }
        break; }
    }
}

void dish_draw_item(int item, int cx, int cy, int rw, int t)
{
    int r;
    for (r = 0; r < RECIPE_COUNT; r++)
        if (cook_recipes[r].item == item) { dish_draw(r, cx, cy, rw, t, 0); return; }
    {
        CookIng g;
        g.kind = ING_ITEM;
        g.id = (uint8_t)item;
        g.qty = 1;
        ingredient_draw(&g, cx, cy, rw, t);
    }
}

/* ====================================================================== */
/* ingredients                                                              */
static void item_art(int item, int cx, int cy, int rw, int t)
{
    int k;
    switch (item) {
    case ITEM_CLOUD_FLOUR:
        ui_round_rect(cx - rw * 2 / 3 - 1, cy - rw * 2 / 3, rw * 4 / 3 + 2, rw * 4 / 3 + 1, 3, R(STONE, 4));
        ui_round_rect(cx - rw * 2 / 3, cy - rw * 2 / 3 + 1, rw * 4 / 3, rw * 4 / 3 - 1, 3, PAL_WHITE);
        plat_rect(cx - rw / 3, cy - rw * 2 / 3 - 2, rw * 2 / 3, 3, R(STONE, 6));
        ui_fill_ellipse(cx, cy + 1, rw / 2, rw / 4, PL(SKY));
        ui_fill_circle(cx - rw / 4, cy, rw / 4, PL(SKY));
        break;
    case ITEM_PIXIE_SUGAR:
        ui_round_rect(cx - rw / 2 - 1, cy - rw / 2, rw + 2, rw + 1, 2, R(SKY, 5));
        ui_round_rect(cx - rw / 2, cy - rw / 2 + 1, rw, rw - 1, 2, R(SKY, 7));
        plat_rect(cx - rw / 2 + 1, cy, rw - 2, rw / 2, PL(BUBBLEGUM));
        plat_rect(cx - rw / 2 - 1, cy - rw / 2 - 2, rw + 2, 3, P(BUBBLEGUM));
        ui_sparkle(cx + rw / 3, cy - rw / 3, (t / 3) & 15, PAL_UI_GOLD);
        break;
    case ITEM_SPECKLED_EGG:
        blob(cx, cy, rw * 3 / 5, rw * 3 / 4, R(PEACH, 7));
        for (k = 0; k < 4; k++) plat_rect(cx - rw / 3 + (k * 5) % (rw * 2 / 3 + 1), cy - rw / 3 + (k * 7) % (rw / 2 + 1), 1, 1, R(WOOD, 3));
        break;
    case ITEM_BLOOD_ORANGE:
        blob(cx, cy + 1, rw * 2 / 3, rw * 2 / 3, P(TANGERINE));
        ui_fill_ellipse(cx + 2, cy - rw / 2 - 1, 2, 1, P(FOREST));
        ui_fill_circle(cx + rw / 3, cy + rw / 2 + 1 + (t / 8) % 3, 1, P(BLOOD));
        break;
    case ITEM_PICKLES:
        ui_round_rect(cx - rw / 2 - 1, cy - rw * 2 / 3, rw + 2, rw * 4 / 3, 3, R(MINT, 3));
        ui_round_rect(cx - rw / 2, cy - rw * 2 / 3 + 1, rw, rw * 4 / 3 - 2, 3, R(MINT, 7));
        for (k = 0; k < 3; k++) ui_fill_ellipse(cx - rw / 4 + k * rw / 4, cy + (k & 1) * 2, 2, rw / 3, P(LIME));
        plat_rect(cx - rw / 2, cy - rw * 2 / 3 - 2, rw, 3, P(CHERRY));
        break;
    case ITEM_SPROUTS:
        for (k = 0; k < 3; k++) blob(cx - rw / 3 + k * rw / 3, cy + (k == 1 ? -rw / 4 : rw / 6), rw / 3 + 1, rw / 3 + 1, k == 1 ? P(LIME) : P(FOREST));
        break;
    case ITEM_MILK:
        ui_round_rect(cx - rw / 2, cy - rw / 2, rw, rw + 2, 1, PAL_WHITE);
        plat_rect(cx - rw / 2, cy - rw / 2, rw, rw / 3, P(SKY));
        for (k = 0; k < rw / 2; k++) plat_rect(cx - k, cy - rw / 2 - rw / 2 + k, 2 * k + 1, 1, PAL_WHITE);
        ui_heart(cx, cy + 2, 2, P(ROSE));
        break;
    case ITEM_HELL_PEPPERS:
        for (k = 0; k < rw; k++) {
            int hw = DOOMO_MAX(1, (rw - k) / 3 + 1);
            plat_rect(cx - rw / 2 + k + 1, cy - hw + k / 4, 2, hw * 2, k == 0 ? PD(CHERRY) : P(CHERRY));
        }
        plat_rect(cx - rw / 2 - 1, cy - 2, 3, 2, P(FOREST));
        plat_rect(cx - rw / 3, cy - 2, rw / 3, 1, PL(CHERRY));
        break;
    case ITEM_ONIGIRI:
        for (k = 0; k < rw; k++) plat_rect(cx - k * 3 / 4 - 1, cy - rw / 2 + k, k * 3 / 2 + 3, 1, PAL_WHITE);
        plat_rect(cx - rw / 3, cy + rw / 4, rw * 2 / 3 + 1, rw / 4 + 1, P(JET));
        plat_rect(cx - 2, cy, 1, 1, R(STONE, 0));
        plat_rect(cx + 2, cy, 1, 1, R(STONE, 0));
        break;
    case ITEM_COFFEE:
        ui_round_rect(cx - rw / 2, cy - rw / 3, rw, rw, 2, PAL_WHITE);
        ui_fill_ellipse(cx, cy - rw / 3 + 1, rw / 2 - 1, 1, P(CHOCOLATE));
        plat_rect(cx + rw / 2, cy - rw / 6, 2, rw / 2, PAL_WHITE);
        kitchen_steam(cx, cy - rw / 2, t, 2);
        break;
    default: {
        /* any other food: its icon on a little plate */
        char s[2];
        s[0] = (char)doomo_items[item].icon;
        s[1] = 0;
        ui_fill_ellipse(cx, cy + 2, rw * 2 / 3, rw / 3, PAL_WHITE);
        fc_text(cx - 3, cy - 4, s, P(TANGERINE));
        break; }
    }
}

void ingredient_draw(const CookIng *g, int cx, int cy, int rw, int t)
{
    if (!g || g->kind == ING_NONE || plat_draw_skipped()) return;
    switch (g->kind) {
    case ING_ITEM: {
        int r;
        for (r = 0; r < RECIPE_COUNT; r++)
            if (cook_recipes[r].item == g->id) {
                dish_draw(r, cx, cy, rw, t, 0);
                return;
            }
        item_art(g->id, cx, cy, rw, t);
        break; }
    case ING_FISH: fish_draw(g->id, cx, cy, rw + 2, t, 0, 0); break;
    case ING_CUTE_FISH: fish_draw(FISH_BUBBLEGUM_GUPPY, cx, cy, rw + 2, t, 0, 0); break;
    case ING_HELL_FISH: fish_draw(FISH_BRIMSTONE_BREAM, cx, cy, rw + 2, t, 0, 0); break;
    default: fish_draw(FISH_HONEY_BASS, cx, cy, rw + 2, t, 0, 0); break;
    }
}
