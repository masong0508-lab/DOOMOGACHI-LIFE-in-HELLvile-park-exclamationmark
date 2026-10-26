/* sets.c - sets and props for the pilot proof: the bandstand at dawn, the
 * GIGGLE banner (with the old M underneath), the giggling fountain, the Wall,
 * the plaza, Nana's bakery window, the egg, and the crayon storybook bits.
 * All world space through wd.h, all on the game's real palette. */
#include "sets.h"
#include "wd.h"
#include <math.h>
#include <string.h>

#define CREAM  PAL_UI_CREAM
#define PINK   PAL_PAINT(PAINT_BUBBLEGUM, 2)
#define PINKM  PAL_PAINT(PAINT_BUBBLEGUM, 1)
#define HOT    PAL_PAINT(PAINT_MAGENTA, 0)
#define STONE(k) PAL_RAMP(RAMP_STONE, k)
#define WOOD(k)  PAL_RAMP(RAMP_WOOD, k)
#define GRASS(k) PAL_RAMP(RAMP_GRASS, k)

/* ---------------------------------------------------------------- sky ---- */
void set_sky_dawn(float t, float par) {
    static const uint8_t cols[] = {
        PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2), PAL_RAMP(RAMP_PASTEL_PINK, 6),
        PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_CANDY, 6), PAL_RAMP(RAMP_CANDY, 7) };
    /* the gradient slides a little with the camera so tilts read */
    int off = (int)((wd_cy + 60) * wd_z * 0.25f);
    tk_vgrad(0, -40 - off, TW, TH + 80, cols, 6);
    /* sun, low and pale, with a slow shimmer ring */
    float sx = (-150 - wd_cx) * wd_z * par + TW * 0.5f, sy = (-28 - wd_cy) * wd_z * par + TH * 0.5f;
    int r = (int)(26 * wd_z * par + 4);
    tk_circle((int)sx, (int)sy, r + 3 + (int)(1.5f * sinf(t * 0.08f)), PAL_RAMP(RAMP_CANDY, 6));
    tk_circle((int)sx, (int)sy, r, PAL_RAMP(RAMP_CANDY, 7));
    /* drifting clouds */
    for (int i = 0; i < 4; i++) {
        float cx = -220 + i * 130 + t * 0.06f * (1 + i % 2), cy = -120 - (i % 3) * 14;
        wd_blob(cx, cy, 22, 6, 0, PAL_RAMP(RAMP_PASTEL_PINK, 7), WB_SHADE);
        wd_blob(cx + 12, cy - 4, 12, 6, 0, PAL_WHITE, WB_SHADE);
    }
}

static void ground(float t) {
    /* tree line */
    for (int i = 0; i < 26; i++) {
        float x = -320 + i * 26 + (i % 3) * 5, h = 16 + (i * 7 % 5) * 3;
        wd_blob(x, -8 - h * 0.5f + 0.6f * sinf(t * 0.05f + i), 15, h, 0, GRASS((i & 1) ? 4 : 3), WB_ALL);
    }
    wd_rect(-420, -6, 840, 300, GRASS(5), 0);
    /* paved plaza in front */
    wd_rect(-420, 0, 840, 300, PAL_RAMP(RAMP_PEACH, 6), 0);
    for (int i = -20; i < 20; i++) wd_line(i * 22.0f, 0, i * 30.0f, 120, 0.6f, PAL_RAMP(RAMP_PEACH, 5));
    for (int j = 1; j < 8; j++) wd_line(-420, j * 12.0f + j * j * 0.8f, 420, j * 12.0f + j * j * 0.8f, 0.6f, PAL_RAMP(RAMP_PEACH, 5));
}

void set_wall(float t) {
    (void)t;
    float x0 = 112, y0 = -128;
    wd_rect(x0, y0, 420, 134, STONE(3), STONE(1));
    for (int r = 0; r < 14; r++)
        for (int c = 0; c < 30; c++) {
            float bx = x0 + c * 16 + (r & 1) * 8, by = y0 + 4 + r * 9.5f;
            wd_rect(bx, by, 15, 8.5f, (c * 7 + r * 3) % 5 == 0 ? STONE(2) : STONE(4), STONE(1));
        }
    /* spikes along the top */
    for (int c = 0; c < 36; c++) {
        float bx = x0 + c * 12;
        wd_tri(bx, y0, bx + 12, y0, bx + 6, y0 - 9, STONE(2));
    }
    /* a crooked crack and a shadow at its foot */
    wd_line(150, y0 + 30, 158, y0 + 52, 1.0f, STONE(1));
    wd_line(158, y0 + 52, 151, y0 + 70, 1.0f, STONE(1));
    wd_dither(x0 - 30, -8, 470, 14, STONE(2), 8);
}

/* the old mystery M blooming back through the pink paint */
static void old_n(const Banner *b, int crayon) {
    if (b->n_amt <= 0.01f) return;
    /* the whole old word MINGLE ghosting up through the pink GIGGLE paint:
     * its own colour, offset down-right like old paint under new */
    const char *word = "MINGLE";
    float p = LET_PX * (1.0f + (b->n_scale - 1.0f) * 0.3f);
    float wcx = (LET_X0 + 17.5f * LET_PX) + 0.8f, wcy = LET_Y0 + 4.0f * LET_PX + 1.0f;
    float x0w = wcx - 17.5f * p, y0 = wcy - 4.0f * p, x0 = x0w;
    uint8_t c = b->n_red ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(PAINT_NAVY, 2);
    uint8_t cd = b->n_red ? PAL_PAINT(PAINT_CHERRY, 0) : PAL_PAINT(PAINT_NAVY, 1);
    float k = b->n_amt > 1.15f ? 1.15f : b->n_amt;
    for (int pass = 0; pass < 2; pass++)
        for (int li = 0; li < 6; li++) {
            uint8_t g[5];
            wd_glyph(word[li], g);
            float lx = x0w + li * 6 * p;
            for (int col = 0; col < 5; col++)
                for (int row = 0; row < 8; row++) {
                    if (!(g[col] >> row & 1)) continue;
                    float cx = lx + (col + 0.5f) * p, cy = y0 + (row + 0.5f) * p;
                    if (b->rolling && cy < b->roll_y) continue;
                    float wob = 1.0f + 0.12f * sinf(col * 2.1f + row * 1.7f + li);
                    float h = p * 0.5f * k * wob + (pass == 0 ? 0.6f / wd_z : 0);
                    wd_rect(cx - h, cy - h, 2 * h, 2 * h, pass == 0 ? cd : c, 0);
                }
        }
    (void)crayon;
    if (b->n_red && b->n_amt > 0.9f) {
        /* glossy wet highlights: it is paint */
        for (int k = 0; k < 4; k++) {
            float hx = x0 + (k == 0 ? 0.5f : k == 1 ? 4.4f : k == 2 ? 1.6f : 0.6f) * p, hy = y0 + (1.2f + k * 1.6f) * p;
            wd_blob(hx, hy, p * 0.22f, p * 0.32f, 0.3f, PAL_PAINT(PAINT_CHERRY, 2), 0);
        }
        if (b->drip > 0) {
            float dx = x0 + 4.5f * p, dy = y0 + 7.6f * p;
            wd_line(dx, dy, dx, dy + b->drip, p * 0.42f, c);
            wd_blob(dx, dy + b->drip + p * 0.25f, p * 0.36f, p * 0.48f, 0, c, WB_OUT | WB_SHADE);
            wd_blob(dx - p * 0.1f, dy + b->drip, p * 0.1f, p * 0.16f, 0, PAL_PAINT(PAINT_CHERRY, 2), 0);
        }
    }
}

void set_banner(float t, const Banner *b, int crayon) {
    /* strings */
    wd_line(BAN_X0 - 4, -76, BAN_X0 + 2, BAN_Y0 + 1, 0.6f, WOOD(2));
    wd_line(BAN_X1 + 2, -76, BAN_X1 - 2, BAN_Y0 + 1, 0.6f, WOOD(2));
    float sway = crayon ? 0 : 0.5f * sinf(t * 0.05f);
    /* cloth with a scalloped hem */
    wd_rect(BAN_X0, BAN_Y0, BAN_X1 - BAN_X0, BAN_Y1 - BAN_Y0, CREAM, PAL_PAINT(PAINT_BONE, 0));
    for (int i = 0; i < 12; i++) {
        float x = BAN_X0 + 2.8f + i * 5.66f;
        wd_blob(x + sway * (i % 2), BAN_Y1, 2.8f, 2.0f, 0, i & 1 ? PINK : CREAM, WB_OUT);
    }
    /* wet pink paint band (painted over the old word) */
    wd_rect(BAN_X0 + 2.5f, BAN_Y0 + 2.5f, BAN_X1 - BAN_X0 - 5, BAN_Y1 - BAN_Y0 - 5, PINK, 0);
    if (!crayon) wd_dither(BAN_X0 + 2.5f, BAN_Y0 + 2.5f, BAN_X1 - BAN_X0 - 5, 2.5f, PAL_WHITE, 6);
    /* fresh paint over the M where the roller has just been */
    if (b->wet > 0) {
        uint32_t s = 77;
        for (int k = 0; k < 5; k++) {
            float sx = N_CX - 8 + tk_rand(&s, 160) / 10.0f, sy = N_CY - 7 + tk_rand(&s, 140) / 10.0f;
            int ph = ((int)t + k * 7) % 24;
            if (ph < 10 && b->wet > 0.3f) {
                int r = (int)((ph < 5 ? ph : 10 - ph) * 0.5f * (wd_z > 3 ? 2 : 1));
                tk_star((int)wd_sx(sx), (int)wd_sy(sy), r, PAL_WHITE);
            }
        }
    }
    /* the word: GIGGLE, white with hot pink edges */
    /* while the old word bleeds up, the wet GIGGLE paint washes out to a pale
     * ghost, so only one word ever reads at a time */
    float fade = b->n_amt;
    if (b->rolling) fade = 0.4f;
    if (fade > 0.35f) wd_text(LET_X0, LET_Y0, "GIGGLE", LET_PX, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_PAINT(PAINT_BUBBLEGUM, 1));
    else wd_text(LET_X0, LET_Y0, "GIGGLE", LET_PX, PAL_WHITE, HOT);
    old_n(b, crayon);
    /* wet drips under the letters (they creep) */
    if (!crayon)
        for (int i = 0; i < 5; i++) {
            float x = LET_X0 + 3 + i * 12.3f;
            float len = 1.5f + fmodf(t * 0.02f + i * 0.37f, 1.0f) * 2.6f;
            wd_line(x, BAN_Y1 - 3, x, BAN_Y1 - 3 + len, 0.9f, PINKM);
            wd_blob(x, BAN_Y1 - 2.6f + len, 0.7f, 0.9f, 0, PINKM, 0);
        }
}

static void post(float x) {
    wd_rect(x - 2.6f, -78, 5.2f, 70, PAL_WHITE, PAL_PAINT(PAINT_SILVER, 0));
    for (int k = 0; k < 9; k++) {
        float y = -76 + k * 7.6f;
        wd_quad(x - 2.4f, y + 2, x + 2.4f, y, x + 2.4f, y + 2.2f, x - 2.4f, y + 4.2f, PINKM, 0);
    }
}

void set_bandstand(float t, const Banner *b, int crayon) {
    if (!crayon) ground(t);
    /* platform */
    wd_rect(-92, -10, 184, 10, crayon ? PAL_PAINT(PAINT_SILVER, 2) : STONE(6), STONE(3));
    wd_rect(-96, -3, 192, 3, STONE(5), STONE(3));
    /* the boarded-up south half */
    for (int k = 0; k < 8; k++) {
        float y = -73 + k * 8.0f;
        wd_rect(4, y, 66, 7.2f, k & 1 ? WOOD(4) : WOOD(5), WOOD(2));
        wd_blob(8, y + 3.6f, 0.7f, 0.7f, 0, PAL_GRAY, 0);
        wd_blob(66, y + 3.6f, 0.7f, 0.7f, 0, PAL_GRAY, 0);
    }
    wd_line(6, -72, 68, -12, 4.0f, WOOD(3));
    wd_line(68, -72, 6, -12, 4.0f, WOOD(3));
    /* posts */
    post(-74); post(0); post(74);
    /* roof: striped canopy */
    int stripes = 12;
    for (int i = 0; i < stripes; i++) {
        float xa = -90 + i * 180.0f / stripes, xb = xa + 180.0f / stripes;
        wd_tri(xa, -78, xb, -78, 0, -116, i & 1 ? CREAM : PINK);
    }
    for (int i = 0; i < 18; i++) {
        float x = -87 + i * 10.25f;
        wd_blob(x, -78, 5.2f, 3.6f, 0, i & 1 ? CREAM : PINKM, WB_OUT | WB_SHADE);
    }
    wd_line(0, -116, 0, -126, 0.8f, WOOD(2));
    wd_blob(0, -117, 2.6f, 2.6f, 0, PAL_UI_GOLD, WB_ALL);
    float fl = sinf(t * 0.15f);
    wd_tri(0, -126, 0, -121, 8 + fl * 1.5f, -123.5f + fl, PINKM);
    /* lanterns along the eave (north half only - the south lanterns are long gone) */
    for (int i = 0; i < 6; i++) {
        float x = -84 + i * 13, y = -71 + 1.5f * sinf(t * 0.07f + i);
        wd_line(x, -76, x, y - 3, 0.4f, WOOD(2));
        static const uint8_t lc[3] = { PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_ROSE, 2) };
        if (i < 1) wd_blob(x, y, 2.4f, 3.0f, 0, lc[i % 3], WB_ALL);
    }
    set_banner(t, b, crayon);
}

void set_ladder(void) {
    float x = LADDER_X;
    uint8_t ol = WOOD(1);
    /* back rungs first, then two chunky rails with planed faces */
    for (int k = 1; k < 5; k++) {
        float y = -k * 6.5f, w = 10 - k * 1.2f;
        wd_rect(x - w, y - 1.1f, 2 * w, 2.2f, WOOD(4), ol);
        wd_rect(x - w + 0.4f, y - 1.1f, 2 * w - 0.8f, 0.6f, WOOD(5), 0);
        wd_rect(x - w + 0.4f, y + 0.5f, 2 * w - 0.8f, 0.5f, WOOD(3), 0);
    }
    wd_tube(x - 10, 0, x - 4, -33, 1.7f, 1.4f, ol, WOOD(2), WOOD(3), WOOD(5));
    wd_tube(x + 10, 0, x + 4, -33, 1.7f, 1.4f, ol, WOOD(2), WOOD(3), WOOD(4));
    wd_rect(x - 6.5f, -35, 13, 4, WOOD(4), ol);
    wd_rect(x - 6, -34.6f, 12, 1, WOOD(5), 0);
    wd_rect(x - 6, -31.8f, 12, 0.7f, WOOD(2), 0);
    /* paint tray on the top step */
    wd_rect(x - 9, -37.5f, 6, 3, PAL_GRAY_L, PAL_GRAY_D);
    wd_rect(x - 8.5f, -37, 5, 1.2f, PINKM, 0);
}

void set_roller(float hx, float hy, float ang, float wet) {
    float ex = hx + cosf(ang) * 6.5f, ey = hy + sinf(ang) * 6.5f;
    wd_tube(hx, hy, ex, ey, 1.2f, 1.0f, PAL_PAINT(PAINT_CHOCOLATE, 1), PAL_PAINT(PAINT_HONEY, 0), PAL_PAINT(PAINT_HONEY, 1), PAL_PAINT(PAINT_HONEY, 2));
    float rx = ex + cosf(ang) * 2.5f, ry = ey + sinf(ang) * 2.5f;
    wd_tube(ex, ey, rx, ry, 0.55f, 0.55f, PAL_GRAY_D, PAL_GRAY, PAL_GRAY_L, PAL_WHITE);
    wd_blob(rx, ry, 2.4f, 6.2f, ang, PINKM, WB_ALL);
    if (wet > 0) wd_blob(rx - 1.0f, ry, 0.6f, 4.2f, ang, PINK, 0);
}

void set_fountain(float t, float g, float g0) {
    float x = FOUNT_X, y = FOUNT_Y;
    /* basin */
    wd_blob(x, y, 30, 7, 0, STONE(6), WB_ALL);
    wd_rect(x - 30, y - 8, 60, 8, STONE(6), 0);
    wd_blob(x, y - 8, 30, 6, 0, STONE(7), WB_OUT);
    wd_blob(x, y - 8, 27, 4.5f, 0, PAL_PAINT(PAINT_SKY, 2), WB_SHADE);
    /* ripples */
    for (int k = 0; k < 3; k++) {
        float ph = fmodf(t * 0.03f + k / 3.0f, 1.0f);
        float r = 4 + ph * 20;
        if (ph < 0.9f) wd_blob_ol(x, y - 8, r, r * 0.17f, 0, PAL_PAINT(PAINT_SKY, 2), PAL_WHITE, WB_OUT);
    }
    /* pillar */
    wd_rect(x - 3.5f, y - 30, 7, 23, STONE(6), STONE(3));
    /* two stone cherub faces, back to back */
    for (int s = -1; s <= 1; s += 2) {
        float d = t - g0 - (s > 0 ? 3 : 0);
        float gg = (g > 0 && d > 0) ? g : 0;
        float bob = gg > 0 ? -1.6f * fabsf(sinf(d * 0.55f)) : 0;
        float hx = x + s * 6.5f, hy = y - 35 + bob;
        float sq = gg > 0 ? 1.0f + 0.08f * sinf(d * 1.1f) : 1;
        wd_blob(hx, hy, 6.5f / sq, 6.0f * sq, 0, STONE(6), WB_ALL);
        /* curls on top */
        wd_blob(hx - s * 1.5f, hy - 6, 2.2f, 1.8f, 0, STONE(6), WB_OUT | WB_SHADE);
        float fx = hx + s * 3.0f;
        if (gg > 0) {   /* giggling: eyes squeezed, mouth open */
            wd_line(fx - 1.6f, hy - 1.2f, fx - 0.4f, hy - 2.0f, 0.6f, STONE(2));
            wd_line(fx + 0.6f, hy - 2.0f, fx + 1.8f, hy - 1.2f, 0.6f, STONE(2));
            wd_blob(fx + s * 0.8f, hy + 2.2f, 1.6f, 1.2f + 0.6f * fabsf(sinf(d * 0.55f)), 0, STONE(1), 0);
            /* tiny "hee" squirts */
            float sp = fmodf(d * 0.08f, 1.0f);
            wd_blob(fx + s * (3 + sp * 10), hy + 2 - sp * 5, 0.9f, 0.9f, 0, PAL_PAINT(PAINT_SKY, 2), 0);
        } else {
            wd_blob(fx - 0.9f, hy - 1.2f, 0.7f, 0.7f, 0, STONE(2), 0);
            wd_blob(fx + 1.2f, hy - 1.2f, 0.7f, 0.7f, 0, STONE(2), 0);
            wd_blob(fx + s * 0.8f, hy + 2.2f, 1.2f, 0.9f, 0, STONE(2), 0);
        }
        wd_blob(fx + s * 2.0f, hy + 0.8f, 1.0f, 0.7f, 0, PAL_RAMP(RAMP_PASTEL_PINK, 6), 0);
        /* water arcs from the mouths into the basin */
        for (int k = 0; k < 9; k++) {
            float u = fmodf(k / 9.0f + t * 0.035f, 1.0f);
            float wx = fx + s * (2 + u * 18), wy = hy + 2.5f - 6 * u + 18 * u * u;
            wd_blob(wx, wy, 0.9f, 0.9f, 0, k & 1 ? PAL_WHITE : PAL_PAINT(PAINT_SKY, 2), 0);
        }
    }
}

void set_balloon(float x, float y, float face, float sq, float tilt, float tx, float ty, float t) {
    /* string with a lazy curve */
    float bx = x + tilt * 8, by = y + 9;
    for (int k = 0; k < 8; k++) {
        float u0 = k / 8.0f, u1 = (k + 1) / 8.0f;
        float x0 = tx + (bx - tx) * u0 + sinf(u0 * 3.14f) * 2.0f, y0 = ty + (by - ty) * u0;
        float x1 = tx + (bx - tx) * u1 + sinf(u1 * 3.14f) * 2.0f, y1 = ty + (by - ty) * u1;
        wd_line(x0, y0, x1, y1, 0.8f, PAL_GRAY_D);
        wd_line(x0 - 0.15f, y0, x1 - 0.15f, y1, 0.4f, PAL_WHITE);
    }
    float sx = 1.0f / sq, sy = sq;
    /* wrapper: striped trapezoid */
    float wy0 = y + 1, wy1 = y + 8 * sy;
    float sh = tilt;
    wd_quad(x - 7 * sx + sh * 1, wy0, x + 7 * sx + sh * 1, wy0, x + 5 * sx + sh * 8, wy1, x - 5 * sx + sh * 8, wy1,
            PAL_PAINT(PAINT_CARAMEL, 2), PAL_PAINT(PAINT_CARAMEL, 0));
    for (int k = -2; k <= 2; k++)
        wd_line(x + k * 2.6f * sx + sh, wy0 + 0.5f, x + k * 1.9f * sx + sh * 8, wy1 - 0.5f, 0.5f, PAL_PAINT(PAINT_CARAMEL, 1));
    /* frosting dome + swirl + cherry */
    wd_blob(x - sh * 2, y - 1.5f * sy, 8.6f * sx, 5.6f * sy, 0, PAL_PAINT(PAINT_BUBBLEGUM, 2), WB_ALL);
    wd_blob(x - sh * 4, y - 6.0f * sy, 5.0f * sx, 3.4f * sy, 0, PAL_PAINT(PAINT_BUBBLEGUM, 2), WB_ALL);
    wd_blob(x - sh * 5.5f, y - 9.6f * sy, 2.2f * sx, 2.2f * sy, 0, PAL_PAINT(PAINT_CHERRY, 1), WB_ALL);
    wd_line(x - sh * 5.5f, y - 11.5f * sy, x - sh * 5.5f + 1.5f, y - 13.5f * sy, 0.4f, GRASS(3));
    /* sprinkles */
    static const uint8_t sc[3] = { PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_SKY, 1) };
    for (int k = 0; k < 6; k++)
        wd_blob(x - sh * 2 + (k * 2.9f - 7) * sx, y - 1.5f * sy - 2 + (k % 3) * 1.4f, 0.8f, 0.35f, k * 0.9f, sc[k % 3], 0);
    /* little face on the wrapper; face=1 turned toward the Wall (right) */
    float fx = x + sh * 4 + face * 3.2f * sx;
    float ew = 2.2f * sx * (1.0f - 0.35f * fabsf(face));
    float blink = fmodf(t + 20, 97) < 4 ? 0.3f : 1.0f;
    wd_blob(fx - ew, y + 3.6f, 0.75f, 0.95f * blink, 0, PAL_PAINT(PAINT_JET, 1), 0);
    wd_blob(fx + ew, y + 3.6f, 0.75f, 0.95f * blink, 0, PAL_PAINT(PAINT_JET, 1), 0);
    wd_line(fx - 1.0f, y + 5.4f, fx, y + 6.0f, 0.4f, PAL_PAINT(PAINT_JET, 1));
    wd_line(fx, y + 6.0f, fx + 1.0f, y + 5.4f, 0.4f, PAL_PAINT(PAINT_JET, 1));
    /* shine */
    wd_blob(x - 4.5f * sx - sh * 2, y - 3 * sy, 1.0f, 1.6f, 0.5f, PAL_WHITE, 0);
}

/* ---------------------------------------------------------------- plaza --- */
static void lantern_string(float x0, float x1, float t, int seed) {
    float y0 = -54, sag = 10;
    float px = x0, py = y0;
    for (int k = 1; k <= 16; k++) {
        float u = k / 16.0f, x = x0 + (x1 - x0) * u, y = y0 + sag * 4 * u * (1 - u);
        wd_line(px, py, x, y, 0.5f, WOOD(2));
        px = x; py = y;
    }
    static const uint8_t lc[4] = { PAL_PAINT(PAINT_LEMON, 2), PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_ROSE, 2), PAL_PAINT(PAINT_SKY, 2) };
    for (int k = 1; k < 8; k++) {
        float u = k / 8.0f, x = x0 + (x1 - x0) * u, y = y0 + sag * 4 * u * (1 - u);
        float sw = 0.25f * sinf(t * 0.12f + k + seed);
        wd_blob(x + sw * 4, y + 3.5f, 2.2f, 2.8f, sw, lc[(k + seed) % 4], WB_ALL);
    }
}

static void building(float x, float w, float h, uint8_t c, uint8_t roof) {
    wd_rect(x, -h, w, h, c, wd_ol(c));
    wd_rect(x, -h, w, 3, PAL_WHITE, 0);                            /* trim */
    wd_tri(x - 3, -h, x + w + 3, -h, x + w * 0.5f, -h - 16, roof);
    wd_rect(x + w * 0.7f, -h - 14, 5, 9, PAL_PAINT(PAINT_RUST, 1), wd_ol(roof));   /* chimney */
    for (int r = 0; r < (int)(h / 18); r++)
        for (int k = 0; k < (int)(w / 14); k++) {
            float wx = x + 5 + k * 14, wy = -h + 8 + r * 18;
            int lit = (int)(tk_hash((uint32_t)(wx * 7 + wy * 13)) % 3) != 0;
            wd_rect(wx, wy, 7, 9, lit ? PAL_PAINT(PAINT_LEMON, 2) : PAL_PAINT(PAINT_SKY, 2), PAL_WHITE);
            wd_line(wx + 3.5f, wy, wx + 3.5f, wy + 9, 0.4f, PAL_WHITE);
            wd_rect(wx - 1, wy + 9, 9, 2.4f, PAL_PAINT(PAINT_CARAMEL, 1), 0);   /* flower box */
            for (int q = 0; q < 3; q++) wd_blob(wx + 1 + q * 2.6f, wy + 8.6f, 1.1f, 1.1f, 0, q & 1 ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_LEMON, 1), 0);
        }
    /* striped awning over the door, and the door with a knob */
    for (int k = 0; k < 6; k++)
        wd_quad(x + w * 0.5f - 12 + k * 4, -22, x + w * 0.5f - 8 + k * 4, -22, x + w * 0.5f - 7 + k * 4, -17, x + w * 0.5f - 13 + k * 4, -17,
                k & 1 ? PAL_WHITE : roof, 0);
    wd_rect(x + w * 0.5f - 5, -14, 10, 14, PAL_PAINT(PAINT_CHOCOLATE, 2), PAL_PAINT(PAINT_CHOCOLATE, 0));
    wd_blob(x + w * 0.5f + 3, -7, 0.9f, 0.9f, 0, PAL_PAINT(PAINT_HONEY, 2), 0);
}

static void mascotty(float x) {
    /* giant bunny-head mascot: perfectly still. The only thing that does not move. */
    wd_rect(x - 4, -14, 8, 14, PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 0));
    wd_blob(x, -24, 10, 10, 0, PAL_WHITE, WB_OUT | WB_SHADE);
    wd_blob(x - 4.5f, -42, 2.8f, 9, -0.1f, PAL_WHITE, WB_OUT | WB_SHADE);
    wd_blob(x + 4.5f, -42, 2.8f, 9, 0.1f, PAL_WHITE, WB_OUT | WB_SHADE);
    wd_blob(x - 4.5f, -42, 1.2f, 6, -0.1f, PAL_PAINT(PAINT_ROSE, 2), 0);
    wd_blob(x + 4.5f, -42, 1.2f, 6, 0.1f, PAL_PAINT(PAINT_ROSE, 2), 0);
    wd_blob(x - 3.5f, -26, 1.6f, 1.6f, 0, PAL_PAINT(PAINT_JET, 1), 0);
    wd_blob(x + 3.5f, -26, 1.6f, 1.6f, 0, PAL_PAINT(PAINT_JET, 1), 0);
    wd_line(x - 4, -20, x + 4, -20, 0.6f, PAL_PAINT(PAINT_JET, 1));
    for (int k = -3; k <= 3; k += 2) wd_line(x + k, -21, x + k, -19, 0.4f, PAL_PAINT(PAINT_JET, 1));
}

void set_plaza(float t) {
    static const uint8_t cols[] = { PAL_PAINT(PAINT_LAVENDER, 2), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_CANDY, 6) };
    tk_vgrad(0, 0, TW, TH, cols, 4);
    for (int i = 0; i < 12; i++) wd_blob(-60 + i * 60, -30, 40, 30, 0, PAL_RAMP(RAMP_GRASS, 5), WB_SHADE);
    static const uint8_t bc[6] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_PAINT(PAINT_MINT, 2), PAL_PAINT(PAINT_LEMON, 2),
                                   PAL_PAINT(PAINT_LAVENDER, 2), PAL_RAMP(RAMP_PEACH, 6), PAL_PAINT(PAINT_SKY, 2) };
    static const uint8_t rc[3] = { PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_TEAL, 1), PAL_PAINT(PAINT_GRAPE, 2) };
    for (int i = 0; i < 9; i++) building(-40 + i * 72, 56 + (i % 3) * 6, 46 + (i * 5 % 3) * 10, bc[i % 6], rc[i % 3]);
    wd_rect(-200, 0, 1200, 200, PAL_RAMP(RAMP_PEACH, 6), 0);
    for (int j = 1; j < 6; j++) wd_line(-200, j * 9.0f, 1000, j * 9.0f, 0.5f, PAL_RAMP(RAMP_PEACH, 5));
    /* lamp posts + lantern strings */
    for (int i = 0; i < 8; i++) {
        float x = 10 + i * 80;
        wd_rect(x - 1.2f, -56, 2.4f, 56, PAL_PAINT(PAINT_TEAL, 0), 0);
        wd_blob(x, -58, 3, 3, 0, PAL_PAINT(PAINT_LEMON, 2), WB_ALL);
        if (i < 7) lantern_string(x, x + 80, t, i);
    }
    /* the poster on a lamp post */
    {
        float x = 170, y = -40;
        wd_rect(x + 2, y, 66, 30, PAL_WHITE, PAL_PAINT(PAINT_ROSE, 0));
        wd_rect(x + 4, y + 2, 62, 26, PAL_RAMP(RAMP_PASTEL_PINK, 6), 0);
        wd_text(x + 9, y + 3, "MIDSUMMER", 1.0f / wd_z, PAL_PAINT(PAINT_MAGENTA, 0), 0);
        wd_text(x + 15, y + 11, "GIGGLE", 1.0f / wd_z, PAL_PAINT(PAINT_MAGENTA, 0), 0);
        wd_text(x + 12, y + 19, "TONIGHT!", 1.0f / wd_z, PAL_PAINT(PAINT_TEAL, 0), 0);
    }
    mascotty(585);
}

/* ---------------------------------------------------------------- bakery -- */
static void loaf(float x, float y) {
    wd_blob(x, y, 9, 5, 0, PAL_PAINT(PAINT_CARAMEL, 1), WB_ALL);
    for (int k = -1; k <= 1; k++) wd_line(x + k * 4 - 1.5f, y - 2.5f, x + k * 4 + 1.5f, y - 3.8f, 0.6f, PAL_PAINT(PAINT_CARAMEL, 2));
}
static void bun(float x, float y) {
    wd_blob(x, y, 5, 4, 0, PAL_PAINT(PAINT_HONEY, 1), WB_ALL);
    wd_blob(x - 1, y - 3, 3, 1.2f, 0, PAL_WHITE, 0);
}
static void cake(float x, float y) {
    wd_rect(x - 8, y - 9, 16, 9, PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_PAINT(PAINT_ROSE, 0));
    wd_rect(x - 8, y - 5, 16, 1.5f, PAL_PAINT(PAINT_ROSE, 1), 0);
    for (int k = 0; k < 5; k++) wd_blob(x - 7 + k * 3.5f, y - 9.5f, 1.8f, 1.4f, 0, PAL_WHITE, WB_OUT);
    wd_blob(x, y - 12, 1.8f, 1.8f, 0, PAL_PAINT(PAINT_CHERRY, 1), WB_ALL);
}

void set_bakery(float t, float paw, float towel, float lamp, float tag_t) {
    (void)paw;
    /* facade */
    wd_rect(-160, -140, 320, 260, PAL_RAMP(RAMP_PASTEL_PINK, 5), 0);
    for (int r = 0; r < 26; r++)
        for (int c = 0; c < 22; c++)
            if ((r * 7 + c * 3) % 5 == 0)
                wd_rect(-160 + c * 15 + (r & 1) * 7, -140 + r * 10, 13, 8, PAL_RAMP(RAMP_PASTEL_PINK, 4), 0);
    /* awning */
    for (int i = 0; i < 12; i++) {
        float x = -96 + i * 16;
        wd_quad(x, -96, x + 16, -96, x + 18, -74, x + 2, -74, i & 1 ? PAL_WHITE : PAL_PAINT(PAINT_MINT, 1), 0);
        wd_blob(x + 10, -74, 8, 4, 0, i & 1 ? PAL_WHITE : PAL_PAINT(PAINT_MINT, 1), WB_OUT | WB_SHADE);
    }
    wd_text(-38, -92, "NANA'S", 2.0f, PAL_PAINT(PAINT_TEAL, 0), PAL_WHITE);
    /* window: interior */
    wd_rect(-80, -64, 160, 86, PAL_RAMP(RAMP_PEACH, 7), 0);
    wd_rect(-80, -64, 160, 30, PAL_RAMP(RAMP_PEACH, 6), 0);
    /* shelves: two of everything */
    wd_rect(-74, -36, 148, 3, PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 2));
    wd_rect(-74, -6, 148, 3, PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 2));
    loaf(-52, -41); loaf(-30, -41);
    bun(26, -40); bun(40, -40);
    cake(-50, -6); cake(-28, -6);
    /* glass: soft reflections that drift */
    for (int k = 0; k < 3; k++) {
        float gx = -70 + k * 55 + fmodf(t * 0.15f, 30);
        wd_quad(gx, -64, gx + 8, -64, gx - 14, 22, gx - 22, 22, PAL_WHITE, 0);
    }
    wd_dither(-80, -64, 160, 86, PAL_WHITE, 2);
    /* frame */
    wd_rect(-86, -70, 172, 6, PAL_WHITE, PAL_GRAY);
    wd_rect(-86, -70, 6, 96, PAL_WHITE, PAL_GRAY);
    wd_rect(80, -70, 6, 96, PAL_WHITE, PAL_GRAY);
    /* sill */
    wd_rect(-92, 22, 184, 7, PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 2));
    wd_rect(-92, 29, 184, 3, PAL_RAMP(RAMP_WOOD, 3), 0);

    /* basket with the egg */
    float bx = 22, by = 14;
    wd_blob(bx, by - 2, 4.6f, 6.0f, 0, PAL_PAINT(PAINT_BONE, 2), WB_ALL);       /* egg peeking */
    wd_blob(bx + 1.5f, by - 6, 0.9f, 1.2f, 0, PAL_PAINT(PAINT_MINT, 2), 0);
    wd_blob(bx - 2.0f, by - 3, 0.8f, 1.0f, 0, PAL_RAMP(RAMP_PASTEL_PINK, 5), 0);
    /* tea towel: tucks in as `towel` goes 0..1 */
    float tw = towel;
    wd_blob(bx - 6 + tw * 2, by + 1 - tw * 2, 4.5f, 3.2f, -0.4f - tw * 0.3f, PAL_WHITE, WB_ALL);
    wd_blob(bx + 6 - tw * 1.5f, by + 1 - tw * 1.5f, 4.2f, 3.0f + tw, 0.5f, PAL_WHITE, WB_ALL);
    for (int k = 0; k < 4; k++) wd_line(bx - 9 + k * 5, by - 1, bx - 8 + k * 5, by + 3, 0.5f, PAL_PAINT(PAINT_ROSE, 2));
    wd_blob(bx, by + 4, 12, 6, 0, PAL_PAINT(PAINT_CARAMEL, 1), WB_ALL);
    for (int k = -4; k <= 4; k++) wd_line(bx + k * 2.4f, by + 0.5f, bx + k * 2.1f, by + 8.5f, 0.45f, PAL_PAINT(PAINT_CARAMEL, 0));
    wd_line(bx - 11, by + 2, bx + 11, by + 2, 0.8f, PAL_PAINT(PAINT_CARAMEL, 2));
    /* handle */
    for (int k = 0; k < 12; k++) {
        float a0 = 3.14159f + k / 12.0f * 3.14159f, a1 = 3.14159f + (k + 1) / 12.0f * 3.14159f;
        wd_line(bx + cosf(a0) * 11, by + 1 + sinf(a0) * 13, bx + cosf(a1) * 11, by + 1 + sinf(a1) * 13, 1.1f, PAL_PAINT(PAINT_CARAMEL, 0));
    }
    /* small lamp */
    if (lamp > 0) {
        float lx = 52, ly = 21 - (1 - fminf(lamp, 1)) * 30;
        float sq = lamp < 1.2f ? 1 : 1 + 0.25f * expf(-(lamp - 1.2f) * 6) * sinf((lamp - 1.2f) * 30);
        if (lamp >= 1.2f) {
            int lvl = 4 + (int)(2 * sinf(t * 0.2f));
            wd_dither(lx - 22, ly - 34, 44, 34, PAL_RAMP(RAMP_CANDY, 7), lvl);
            wd_blob(lx, ly - 13, 6.5f, 6.5f, 0, PAL_RAMP(RAMP_CANDY, 6), 0);
        }
        wd_rect(lx - 5 * sq, ly - 3, 10 * sq, 3, PAL_PAINT(PAINT_HONEY, 0), 0);
        wd_line(lx, ly - 3, lx, ly - 9, 1.2f, PAL_PAINT(PAINT_HONEY, 1));
        wd_blob(lx, ly - 13 / sq, 4.2f * sq, 4.0f / sq, 0, lamp >= 1.2f ? PAL_RAMP(RAMP_CANDY, 7) : PAL_PAINT(PAINT_BONE, 1), WB_OUT | WB_SHADE);
        wd_quad(lx - 5, ly - 16 / sq, lx + 5, ly - 16 / sq, lx + 3, ly - 20 / sq, lx - 3, ly - 20 / sq, PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_MINT, 0));
    }
    /* the tag on the handle, turning in the warm air (drawn in screen space so it can be read) */
    {
        float ax = bx - 9, ay = by - 7;
        float sw = sinf(tag_t * 0.09f);
        float turn = cosf((tag_t < 165 ? 165 - tag_t : 0) * 0.04f) * (tag_t < 165 ? 1.0f : 1.0f - 0.04f * fabsf(sinf(tag_t * 0.07f)));
        float tx = ax - 4 + sw * 1.5f, ty = ay + 6;
        wd_line(ax, ay, tx, ty, 0.3f, PAL_GRAY_D);
        int sx = (int)wd_sx(tx), sy = (int)wd_sy(ty);
        int w = (int)(fabsf(turn) * 28 * wd_z), h = (int)(7 * wd_z);
        if (w > 1 && h > 2) {
            tk_rect(sx - w / 2, sy, w, h, PAL_PAINT(PAINT_BONE, 2));
            tk_frame(sx - w / 2, sy, w, h, PAL_PAINT(PAINT_CARAMEL, 0));
            if (turn > 0.85f && w > 70) {
                tk_text(sx - 39, sy + h / 2 - 9, "for whoever's", PAL_PAINT(PAINT_CHOCOLATE, 1), 1);
                tk_text(sx - 15, sy + h / 2 + 1, "next.", PAL_PAINT(PAINT_CHOCOLATE, 1), 1);
            } else if (turn > 0.5f) {
                for (int k = 0; k < 2; k++) tk_rect(sx - w / 2 + 3, sy + h / 2 - 6 + k * 9, w - 6, 2, PAL_PAINT(PAINT_CHOCOLATE, 2));
            }
        }
    }
}

void set_paw(float x, float y, float t) {
    /* Nana Puddin's woolly paw in a knitted lavender cardigan sleeve, from the top right */
    uint8_t ol = PAL_PAINT(PAINT_LAVENDER, 0);
    wd_tube(x + 64, y - 74, x + 12, y - 12, 9.5f, 8.0f, PAL_PAINT(PAINT_JET, 1), ol, PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2));
    for (int k = 0; k < 7; k++) {          /* knit ribs across the sleeve */
        float u = k / 7.0f, cx = x + 60 - u * 46, cy = y - 70 + u * 56;
        for (int j = -3; j <= 3; j++)
            wd_blob(cx + j * 1.9f, cy + j * 1.9f, 0.8f, 1.3f, 0.78f, (j + k) & 1 ? PAL_PAINT(PAINT_LAVENDER, 2) : PAL_PAINT(PAINT_LAVENDER, 1), 0);
    }
    /* fluffy woolly cuff: a ring of puffs */
    wd_blob_ol(x + 9, y - 9, 8.0f, 7.5f, 0, PAL_PAINT(PAINT_BONE, 2), PAL_PAINT(PAINT_BONE, 0), WB_SHADE);
    for (int k = 0; k < 11; k++) {
        float a = k * 0.571f;
        wd_blob_ol(x + 9 + cosf(a) * 7.0f, y - 9 + sinf(a) * 7.0f, 3.0f, 2.7f, 0, k & 1 ? PAL_WHITE : PAL_PAINT(PAINT_BONE, 2), PAL_PAINT(PAINT_BONE, 0), WB_SHADE);
    }
    /* the paw: a round pad with four plump fingers */
    wd_blob_ol(x + 2, y - 1, 7.5f, 6.5f, -0.6f, PAL_PAINT(PAINT_BONE, 2), PAL_PAINT(PAINT_CARAMEL, 0), WB_SHADE);
    for (int k = 0; k < 4; k++) {
        float a = 2.0f + k * 0.42f + 0.06f * sinf(t * 0.1f + k);
        wd_blob_ol(x + 2 + cosf(a) * 7.2f, y - 1 + sinf(a) * 6.4f, 2.4f, 2.8f, a, PAL_WHITE, PAL_PAINT(PAINT_CARAMEL, 0), WB_SHADE);
        wd_blob(x + 2 + cosf(a) * 7.6f, y - 1 + sinf(a) * 6.8f, 0.9f, 0.9f, 0, PAL_PAINT(PAINT_ROSE, 2), 0);
    }
}

/* ---------------------------------------------------------------- egg ----- */
void set_egg(float cx, float cy, float r, float rot, float crack, float t) {
    /* (cx,cy) is the bottom pivot. Egg drawn in screen space at world scale. */
    float pcx = wd_sx(cx), pcy = wd_sy(cy), R = r * wd_z;
    float cs = cosf(rot), sn = sinf(rot);
    static const uint8_t BY[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    for (int y = (int)(pcy - R * 2.6f); y <= (int)(pcy + 2); y++)
        for (int x = (int)(pcx - R * 1.6f); x <= (int)(pcx + R * 1.6f); x++) {
            if ((unsigned)x >= TW || (unsigned)y >= TH) continue;
            float dx = x + 0.5f - pcx, dy = y + 0.5f - pcy;
            float lx = dx * cs + dy * sn, ly = -dx * sn + dy * cs;   /* local, pivot at bottom */
            float v = -(ly + R * 1.15f) / (R * 1.15f);               /* -1 bottom .. 1 top */
            float half = R * (1.0f - 0.16f * v) * sqrtf(fmaxf(0, 1 - v * v));
            float edge = fabsf(lx) - half;
            if (v < -1.03f || v > 1.03f || edge > 1.2f) continue;
            if (edge > 0 || v > 1.0f || v < -1.0f) { tk_fb[y][x] = PAL_PAINT(PAINT_BONE, 0); continue; }
            float nx = lx / (half + 0.01f), ny = -v;
            float l = -0.5f * nx - 0.6f * ny + 0.5f * (1 - nx * nx);
            uint8_t c = PAL_PAINT(PAINT_BONE, 2);
            if (l < -0.15f || (l < 0.0f && BY[y & 3][x & 3] < 8)) c = PAL_PAINT(PAINT_BONE, 1);
            if (l > 0.75f) c = PAL_WHITE;
            /* pastel spots */
            static const float sp[6][3] = { { -0.4f, 0.4f, 0.16f }, { 0.35f, 0.15f, 0.12f }, { 0.05f, 0.75f, 0.1f },
                                            { -0.2f, -0.3f, 0.13f }, { 0.5f, -0.45f, 0.1f }, { 0.15f, -0.05f, 0.08f } };
            for (int k = 0; k < 6; k++) {
                float ddx = lx / R - sp[k][0], ddy = v - sp[k][1];
                if (ddx * ddx + ddy * ddy * 0.8f < sp[k][2] * sp[k][2])
                    c = k & 1 ? PAL_PAINT(PAINT_MINT, 2) : PAL_RAMP(RAMP_PASTEL_PINK, 5);
            }
            /* the crack */
            if (crack > 0) {
                float zz = fmodf(fabsf(lx / R * 6 + 10), 2.0f);
                float zig = 0.08f + (zz < 1 ? zz : 2 - zz) * 0.14f;
                float reach = crack * 1.2f * R;
                if (fabsf(v - zig) < 0.035f + 0.01f * crack && fabsf(lx) < reach) c = PAL_PAINT(PAINT_CHOCOLATE, 0);
            }
            tk_fb[y][x] = c;
        }
    (void)t;
}

/* ---------------------------------------------------------------- crayon -- */
void set_storybook(float t) {
    tk_clear(CREAM);
    Banner b = { 0 };
    set_bandstand(t, &b, 1);
    /* the Wall, straight through the middle of the bandstand, in grey crayon */
    wd_rect(2, -140, 14, 160, PAL_PAINT(PAINT_SILVER, 1), PAL_PAINT(PAINT_CHARCOAL, 2));
    for (int k = 0; k < 16; k++) wd_line(2, -140 + k * 10.0f, 16, -140 + k * 10.0f, 0.6f, PAL_PAINT(PAINT_CHARCOAL, 2));
}

void set_crayon_hand(int f) {
    tk_clear(CREAM);
    wd_cam(0, 0, 4);
    uint8_t sk = PAL_SKIN(SKIN_PEACH, 1), ol = PAL_PAINT(PAINT_RUST, 1);
    /* small open hand, palm up, from the bottom */
    wd_line(6, 30, 2, 8, 12, ol);
    wd_line(6, 30, 2, 8, 10, sk);
    wd_blob(0, 4, 8, 6.5f, 0, sk, WB_OUT);
    float fa[4] = { -2.3f, -1.9f, -1.5f, -1.15f };
    for (int k = 0; k < 4; k++) {
        float x0 = -4 + k * 2.8f, y0 = 0;
        float x1 = x0 + cosf(fa[k]) * 9, y1 = y0 + sinf(fa[k]) * 9;
        wd_line(x0, y0, x1, y1, 3.0f, ol);
        wd_line(x0, y0, x1, y1, 2.0f, sk);
    }
    wd_line(-7, 4, -13, -1, 3.0f, ol);
    wd_line(-7, 4, -13, -1, 2.0f, sk);
    /* the loop of string sliding off the fingertips, up and away */
    float u = f / 3.0f;
    float lx = 3 - u * 10, ly = -9 - u * 9;
    wd_blob_ol(lx, ly, 3.2f, 1.6f, -0.5f, CREAM, PAL_PAINT(PAINT_CHARCOAL, 2), WB_OUT);
    wd_line(lx - 2, ly - 1, lx - 30, ly - 60, 0.5f, PAL_PAINT(PAINT_CHARCOAL, 2));
    wd_post_crayon(f * 4, CREAM);
}
