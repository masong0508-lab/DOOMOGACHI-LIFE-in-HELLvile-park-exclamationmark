/* fish_gfx.c - procedural art of the Lake of Fire (owner: fishcook): every
 * fish species (shape + palette + pattern + face + extras, no stored
 * pixels), the lake scenery for day / dusk / night, the pier, the angler
 * (the player's own chibi), the bobber, ripples and underwater shadows.
 *
 * Everything is spans through the UI primitives: soft 3-tone shading
 * (belly, rim light, shadowed lower rows), coloured outlines, big glossy
 * two-highlight eyes, blush, and something always moving (tail wag, fin
 * flap, blinking, bubbles, flames, sparkles). */
#include "fishcook_int.h"

static uint8_t g_sil;               /* silhouette colour, 0 = normal      */
static int     g_dir;               /* +1 faces right, -1 left            */
static int     g_cx;                /* mirror axis                        */
static int     g_lod;               /* 1 = small: fewer spans (grids, lists) */

static uint8_t C(uint8_t c) { return g_sil ? g_sil : c; }
static uint8_t dk(uint8_t c) { return ui_darker(c); }
static uint8_t lt(uint8_t c) { return ui_lighter(c); }

/* A horizontal span in fish space (x offsets relative to g_cx, mirrored
 * when the fish faces left). */
static void span(int x0, int x1, int y, uint8_t c)
{
    int a, b;
    if (x1 < x0) { int s = x0; x0 = x1; x1 = s; }
    if (g_dir > 0) { a = g_cx + x0; b = g_cx + x1; }
    else { a = g_cx - x1; b = g_cx - x0; }
    plat_rect(a, y, b - a + 1, 1, c);
}

static int fx_(int dx) { return g_cx + g_dir * dx; }

/* Half width of an ellipse row (|dy| <= ry). */
static int ehw(int rx, int ry, int dy)
{
    if (ry <= 0) return rx;
    if (dy < 0) dy = -dy;
    if (dy > ry) return -1;
    return rx * (int)fx_isqrt((uint32_t)(ry * ry - dy * dy)) / ry;
}

static void ellipse_at(int dx, int cy, int rx, int ry, uint8_t c)
{
    ui_fill_ellipse(fx_(dx), cy, rx, ry, c);
}

/* An outlined, ring-shaped ellipse (halos, ripples). */
static void ring(int cx, int cy, int rx, int ry, uint8_t c)
{
    int dy;
    for (dy = -ry; dy <= ry; dy++) {
        int o = ehw(rx, ry, dy), i = ehw(rx - 2, ry - 1, dy);
        if (o < 0) continue;
        if (i < 0) { plat_rect(cx - o, cy + dy, 2 * o + 1, 1, c); continue; }
        plat_rect(cx - o, cy + dy, o - i, 1, c);
        plat_rect(cx + i + 1, cy + dy, o - i, 1, c);
    }
}

/* ====================================================================== */
/* parts                                                                    */
static void tail(const FishInfo *f, int x0, int cy, int len, int h0, int h1, int wag, int forked)
{
    int i, pass;
    uint8_t c = C(f->fin), o = C(dk(dk(f->fin))), l = C(lt(f->fin)), d = C(dk(f->fin));
    if (len < 2) len = 2;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i <= len + (pass == 0); i++) {
            int k = DOOMO_MIN(i, len);
            int hh = h0 + (h1 - h0) * k * (2 * len - k) / (len * len);
            int off = wag * k / len, x = fx_(x0 - i), y = cy + off;
            int notch = forked ? DOOMO_MAX(0, (k - len / 2) * hh / len) : 0;
            if (i >= len) hh = hh * 3 / 4;                  /* rounded end    */
            if (pass == 0) {                                /* outline        */
                if (notch > 1) {
                    plat_rect(x, y - hh - 1, 1, hh - notch + 3, o);
                    plat_rect(x, y + notch - 1, 1, hh - notch + 3, o);
                } else {
                    plat_rect(x, y - hh - 1, 1, 2 * hh + 3, o);
                }
                continue;
            }
            if (i > len) continue;
            if (notch > 0) {
                plat_rect(x, y - hh, 1, hh - notch + 1, c);
                plat_rect(x, y + notch, 1, hh - notch + 1, d);
            } else if (g_lod) {
                plat_rect(x, y - hh, 1, 2 * hh + 1, c);
            } else {
                plat_rect(x, y - hh, 1, hh + 1, c);
                plat_rect(x, y + 1, 1, hh, d);
            }
            if (!g_sil && !g_lod && hh >= 3) {              /* fin rays       */
                if (hh / 2 > notch) {
                    plat_rect(x, y - hh / 2, 1, 1, l);
                    plat_rect(x, y + hh / 2, 1, 1, c);
                }
                plat_rect(x, y - hh, 1, 1, l);
            }
        }
    }
}

static void dorsal(const FishInfo *f, int dx, int top, int w, int h, int flap)
{
    int k;
    uint8_t c = C(f->fin), o = C(dk(dk(f->fin)));
    for (k = 0; k <= h; k++) {
        int hw = w * (h - k) / (h + 1) + 1, lean = k * w / (2 * (h + 1)) + flap * k / (h + 1);
        span(dx - hw - lean - 1, dx + hw - lean, top - k, o);
        if (hw > 1) span(dx - hw - lean, dx + hw - lean - 1, top - k, k == h ? o : c);
    }
}

static void eye(const FishInfo *f, int dx, int cy, int re, int t, int happy)
{
    int x = fx_(dx), blink = ((t + f->body * 7) % 110) < 4;
    uint8_t ink = PAL_RAMP(RAMP_STONE, 0);
    if (g_sil || f->eye == FEYE_NONE) return;
    if (re < 2) re = 2;
    if (happy || (blink && f->eye != FEYE_HOLLOW && f->eye != FEYE_BUTTON)) {
        /* closed happy arc */
        plat_rect(x - re + 1, cy, 2 * re - 1, 1, ink);
        plat_rect(x - re, cy + 1, 1, 1, ink);
        plat_rect(x + re, cy + 1, 1, 1, ink);
        return;
    }
    switch (f->eye) {
    case FEYE_HOLLOW:
        ui_fill_circle(x, cy, re, ink);
        plat_rect(x + g_dir, cy, 1, 1, PAL_PAINT(PAINT_CHERRY, 2));
        return;
    case FEYE_BUTTON:
        ui_fill_circle(x, cy, re, PAL_RAMP(RAMP_STONE, 1));
        ui_fill_circle(x, cy, re - 1, PAL_PAINT(PAINT_GRAPE, 0));
        plat_rect(x - 1, cy - 1, 1, 1, lt(PAL_PAINT(PAINT_GRAPE, 1)));
        plat_rect(x + 1, cy + 1, 1, 1, lt(PAL_PAINT(PAINT_GRAPE, 1)));
        return;
    case FEYE_SLEEPY:
        ui_fill_circle(x, cy, re, PAL_WHITE);
        plat_rect(x - re, cy - re, 2 * re + 1, re, dk(f->body));      /* heavy lid */
        plat_rect(x - re, cy - 1, 2 * re + 1, 1, ink);
        plat_rect(x + g_dir, cy + re / 2, 2, DOOMO_MAX(1, re / 2), ink);
        return;
    default: break;
    }
    if (g_lod) {                                       /* small: 3 spans */
        ui_fill_circle(x, cy, re, PAL_WHITE);
        plat_rect(x + g_dir * (re / 3) - (re > 2), cy, 2 + (re > 2), 2, ink);
        plat_rect(x + g_dir * (re / 3) - 1, cy - 1, 1, 1, PAL_WHITE);
        return;
    }
    ui_fill_circle(x, cy, re + 1, dk(dk(f->body)));
    ui_fill_circle(x, cy, re, PAL_WHITE);
    ui_fill_circle(x + g_dir * (re / 3), cy + 1, DOOMO_MAX(1, re * 2 / 3), ink);
    plat_rect(x + g_dir * (re / 3) - 1, cy - 1, re >= 4 ? 2 : 1, re >= 4 ? 2 : 1, PAL_WHITE);
    if (re >= 3) plat_rect(x + g_dir * (re / 3) + 1, cy + 2, 1, 1, PAL_WHITE);
    if (f->eye == FEYE_ANGRY) {
        int k;
        for (k = 0; k <= re + 1; k++) plat_rect(fx_(dx - re + k) , cy - re - 2 + k / 2, 1, 2, ink);
    } else if (f->eye == FEYE_SAD) {
        int k;
        for (k = 0; k <= re + 1; k++) plat_rect(fx_(dx - re + k), cy - re - 1 - k / 2, 1, 1, ink);
        if (((t / 40) & 3) == 0) ui_fill_circle(x - g_dir, cy + re + 2 + (t % 40) / 8, 1, PAL_PAINT(PAINT_SKY, 2));
    } else if (f->eye == FEYE_GOTH) {
        plat_rect(fx_(dx + re + 1), cy - 1, 2, 1, ink);                 /* wing */
        plat_rect(x - re + 1, cy - re - 2, 1, 2, ink);
        plat_rect(x, cy - re - 2, 1, 2, ink);
        plat_rect(x + re - 1, cy - re - 2, 1, 2, ink);
    }
}

static void mouth(const FishInfo *f, int dx, int cy, int r, int t, int fight)
{
    uint8_t ink = PAL_RAMP(RAMP_STONE, 0);
    if (g_sil || f->eye == FEYE_NONE) return;
    if (f->extras & FF_SCREAM) {
        int open = DOOMO_MAX(2, r / 5) + ((t / 3) & 1);
        ellipse_at(dx - open / 2, cy, open / 2 + 1, open, ink);
        ellipse_at(dx - open / 2, cy + open / 2, open / 2, open / 2, PAL_PAINT(PAINT_ROSE, 1));
        if (r >= 14) {
            int k;
            for (k = 0; k < 3; k++)
                fc_text(fx_(dx + 4 + k * 6) - 2, cy - r / 2 - 10 - ((t / 4 + k * 3) % 6), "A",
                        PAL_PAINT(PAINT_LEMON, 2));
        }
        return;
    }
    if (f->extras & FF_TEETH) {
        int w = DOOMO_MAX(3, r / 3), k;
        span(dx - w, dx, cy, ink);
        span(dx - w, dx, cy + 1 + fight, PAL_RAMP(RAMP_BLOOD, 2));
        for (k = 0; k < w; k += 2) span(dx - k - 1, dx - k - 1, cy - 1, PAL_WHITE);
        for (k = 1; k < w; k += 2) span(dx - k - 1, dx - k - 1, cy + 2 + fight, PAL_WHITE);
        return;
    }
    /* a tiny cat-like smile */
    span(dx - 2, dx - 1, cy, ink);
    span(dx - 3, dx - 3, cy - 1, ink);
    if (fight) span(dx - 2, dx - 1, cy + 1, PAL_PAINT(PAINT_ROSE, 1));
}

static const int8_t spot_pos[8][2] = {
    { -6, -5 }, { 2, -7 }, { -12, 1 }, { 6, 0 }, { -2, 3 }, { -9, -9 }, { 10, -5 }, { -15, -4 }
};
static const uint8_t sprinkle_col[6] = {
    PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_LEMON, 1), PAL_PAINT(PAINT_MINT, 1),
    PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_TANGERINE, 1)
};
static const uint8_t rainbow_col[6] = {
    PAL_PAINT(PAINT_CHERRY, 1), PAL_PAINT(PAINT_TANGERINE, 1), PAL_PAINT(PAINT_LEMON, 1),
    PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_LAVENDER, 1)
};

/* Pattern clipped to the body ellipse at (bdx, cy), radii rx, ry. */
static void pattern(const FishInfo *f, int bdx, int cy, int rx, int ry, int t)
{
    int k;
    if (g_sil) return;
    switch (f->pattern) {
    case FPAT_STRIPES:
        for (k = -1; k <= 2; k++) {
            int sx = k * rx / 3, w = DOOMO_MAX(1, rx / 7), x;
            for (x = sx; x < sx + w; x++) {
                int hh = ehw(ry, rx, x) - 1;
                if (hh > 0) plat_rect(fx_(bdx + x), cy - hh, 1, hh * 2 + 1 - hh / 2, dk(f->body));
            }
        }
        break;
    case FPAT_SPOTS:
        for (k = 0; k < 6; k++) {
            int sx = spot_pos[k][0] * rx / 16, sy = spot_pos[k][1] * ry / 12;
            if (ehw(rx - 2, ry - 2, sy) > (sx < 0 ? -sx : sx))
                ellipse_at(bdx + sx, cy + sy, DOOMO_MAX(1, rx / 9), DOOMO_MAX(1, ry / 7), dk(f->body));
        }
        break;
    case FPAT_SPRINKLES:
        for (k = 0; k < 8; k++) {
            int sx = spot_pos[k][0] * rx / 16, sy = spot_pos[k][1] * ry / 12;
            if (ehw(rx - 2, ry - 2, sy) > (sx < 0 ? -sx : sx))
                plat_rect(fx_(bdx + sx), cy + sy, (k & 1) ? 2 : 1, (k & 1) ? 1 : 2, sprinkle_col[k % 6]);
        }
        break;
    case FPAT_STARS:
        for (k = 0; k < 5; k++) {
            int sx = spot_pos[k][0] * rx / 16, sy = spot_pos[k][1] * ry / 12;
            if (ehw(rx - 2, ry - 2, sy) > (sx < 0 ? -sx : sx)) {
                plat_rect(fx_(bdx + sx), cy + sy, 1, 1, f->fin);
                if (rx > 12) ui_sparkle(fx_(bdx + sx), cy + sy, (t / 3 + k * 5) & 15, f->fin);
            }
        }
        break;
    case FPAT_PATCHES:
        ellipse_at(bdx - rx / 3, cy - ry / 3, rx / 3, ry / 3, f->fin);
        ellipse_at(bdx + rx / 3, cy - ry / 2, rx / 4, ry / 4, f->fin);
        ellipse_at(bdx - rx * 2 / 3, cy + ry / 6, rx / 5, ry / 4, PAL_RAMP(RAMP_STONE, 1));
        break;
    case FPAT_CRACKS: {
        uint8_t glow = ((t / 6) & 1) ? PAL_RAMP(RAMP_LAVA, 7) : PAL_RAMP(RAMP_LAVA, 6);
        for (k = 0; k < 3; k++) {
            int x0 = bdx - rx / 2 + k * rx / 3, y0 = cy - ry / 3 + (k & 1) * ry / 3;
            plat_line(fx_(x0), y0, fx_(x0 + rx / 6), y0 + ry / 4, glow);
            plat_line(fx_(x0 + rx / 6), y0 + ry / 4, fx_(x0 + rx / 10), y0 + ry / 2, glow);
        }
        break; }
    case FPAT_BONES: {
        uint8_t bone = PAL_PAINT(PAINT_BONE, 2);
        span(bdx - rx * 3 / 4, bdx + rx / 2, cy, bone);                   /* spine */
        for (k = -2; k <= 2; k++) {
            int x = bdx + k * rx / 4, hh = ehw(ry, rx, k * rx / 4) * 2 / 3;
            if (hh > 0) plat_rect(fx_(x), cy - hh, 1, hh * 2 + 1, bone);
        }
        break; }
    case FPAT_RAINBOW: {
        int dy;
        for (dy = -ry + 1; dy < ry; dy++) {
            int hw = ehw(rx, ry, dy) - 1;
            if (hw > 0) span(bdx - hw, bdx + hw, cy + dy, rainbow_col[(dy + ry) * 6 / (2 * ry + 1)]);
        }
        break; }
    default: break;
    }
}

/* The body: outline, fill with belly band and shading, highlight. */
static void body(const FishInfo *f, int bdx, int cy, int rx, int ry)
{
    int dy;
    uint8_t o = C(dk(dk(f->body))), b = C(f->body), be = C(f->belly);
    uint8_t sh = C(dk(f->body)), hi = C(lt(f->body));
    for (dy = -ry - 1; dy <= ry + 1; dy++) {
        int hw = ehw(rx + 1, ry + 1, dy);
        if (hw >= 0) span(bdx - hw, bdx + hw, cy + dy, dy > ry ? C(PAL_RAMP(RAMP_STONE, 0)) : o);
    }
    for (dy = -ry; dy <= ry; dy++) {
        int hw = ehw(rx, ry, dy);
        if (hw < 0) continue;
        if (dy >= ry - 1 && !g_sil) { span(bdx - hw, bdx + hw, cy + dy, sh); continue; }
        if (dy <= -ry + 1 && !g_sil) { span(bdx - hw, bdx + hw, cy + dy, hi); continue; }
        span(bdx - hw, bdx + hw, cy + dy, b);
        if (dy > ry / 4 && !g_sil && hw > 2) {
            int bw = hw * 3 / 4;
            span(bdx - bw + rx / 8, bdx + bw, cy + dy, be);
        }
    }
    if (!g_sil && !g_lod && rx >= 5) {
        ellipse_at(bdx + rx / 4, cy - ry / 2, DOOMO_MAX(1, rx / 5), DOOMO_MAX(1, ry / 5), lt(hi));
        span(bdx - rx / 2, bdx - rx / 2 + 1, cy - ry + 2, PAL_WHITE);
    }
}

static void extras(const FishInfo *f, int bdx, int cy, int rx, int ry, int r, int t, int headx)
{
    uint16_t e = f->extras;
    int k;
    if (g_sil) return;
    if ((e & FF_BLUSH) && !g_lod) {
        int x = fx_(headx - 1), y = cy + ry / 5 + 2;
        plat_checker(x - DOOMO_MAX(1, r / 9), y, DOOMO_MAX(2, r / 5), 2, PAL_PAINT(PAINT_ROSE, 2));
    }
    if (e & FF_STITCH) {
        for (k = -rx / 2; k < rx / 2; k += 3) span(bdx + k, bdx + k + 1, cy + ry / 3, PAL_RAMP(RAMP_PASTEL_PINK, 4));
        span(bdx - rx / 2, bdx - rx / 2, cy + ry / 3 - 1, PAL_RAMP(RAMP_PASTEL_PINK, 3));
    }
    if (e & FF_WHISKERS) {
        int x = fx_(bdx + rx - 1), wv = ui_wave(t, 40, 2);
        plat_line(x, cy + 1, x + g_dir * (r / 3 + 3), cy + 3 + wv, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        plat_line(x, cy + 2, x + g_dir * (r / 3 + 1), cy + 6 - wv, PAL_PAINT(PAINT_LAVENDER, 2));
    }
    if (e & FF_HORNS) {
        int hx = bdx + rx / 3, h = DOOMO_MAX(3, r / 4);
        for (k = 0; k < h; k++) {
            int w = (h - k) / 2;
            span(hx - w - 2 - k / 2, hx + w - 2 - k / 2, cy - ry - k, k > h - 2 ? PAL_PAINT(PAINT_BONE, 2) : PAL_PAINT(PAINT_BLOOD, 0));
            span(hx + 4 - w + k / 3, hx + 4 + w + k / 3, cy - ry - k + 1, k > h - 2 ? PAL_PAINT(PAINT_BONE, 2) : PAL_PAINT(PAINT_BLOOD, 0));
        }
    }
    if (e & FF_CROWN) {
        int x = fx_(bdx + rx / 6), w = DOOMO_MAX(4, r / 3), y = cy - ry - 3;
        plat_rect(x - w, y, 2 * w + 1, 3, PAL_UI_GOLD);
        plat_rect(x - w, y + 2, 2 * w + 1, 1, dk(PAL_UI_GOLD));
        for (k = -1; k <= 1; k++) {
            int px = x + k * w, ph = 3 + (k == 0) * 2;
            plat_rect(px - 1, y - ph, 3, ph, PAL_UI_GOLD);
            plat_rect(px, y - ph - 1, 1, 1, lt(PAL_UI_GOLD));
        }
        plat_rect(x - 1, y, 2, 2, PAL_PAINT(PAINT_CHERRY, 1));
        ui_sparkle(x + w, y - 4, (t / 3) & 15, PAL_WHITE);
    }
    if (e & FF_HALO) {
        int y = cy - ry - 4 - ui_wave(t, 50, 1);
        ring(fx_(bdx + rx / 6), y, DOOMO_MAX(4, r / 3), DOOMO_MAX(1, r / 10), PAL_UI_GOLD);
    }
    if (e & FF_WINGS) {
        int wx = bdx - rx / 4, fl = ui_wave(t, 24, 2);
        ellipse_at(wx, cy - ry - 1 - fl, DOOMO_MAX(3, rx / 3), DOOMO_MAX(2, ry / 3), PAL_RAMP(RAMP_SKY, 6));
        ellipse_at(wx - 1, cy - ry - 2 - fl, DOOMO_MAX(2, rx / 4), DOOMO_MAX(1, ry / 4), PAL_WHITE);
    }
    if (e & FF_FLAMES) {
        for (k = 0; k < 3; k++) {
            int x = bdx - rx / 2 + k * rx / 3, h = DOOMO_MAX(3, r / 4) + ((t / 3 + k * 2) % 3);
            int y = cy - ehw(ry, rx, x - bdx) - 1, j;
            for (j = 0; j < h; j++) {
                int w = (h - j) / 3;
                uint8_t c = j < h / 3 ? PAL_RAMP(RAMP_LAVA, 4) : j < h * 2 / 3 ? PAL_RAMP(RAMP_LAVA, 6) : PAL_RAMP(RAMP_LAVA, 7);
                span(x - w + ((t / 4 + j + k) % 3 == 0), x + w, y - j, c);
            }
        }
    }
    if (e & FF_VAPE) {
        for (k = 0; k < 3; k++) {
            int p = (t + k * 20) % 60, x = fx_(bdx + rx + 2 + p / 6), y = cy - 2 - p / 3;
            int rr = 1 + p / 15;
            if (p < 54) {
                ui_fill_circle(x, y, rr + 1, PAL_PAINT(PAINT_LAVENDER, 1));
                ui_fill_circle(x - 1, y - 1, rr, PAL_PAINT(PAINT_LAVENDER, 2));
            }
        }
    }
    if (e & FF_BUBBLE) {
        int p = t % 50, x = fx_(bdx + rx + 3), y = cy - 3 - p / 4, rr = 1 + p / 12;
        if (p < 44) {
            ring(x, y, rr + 1, rr + 1, PAL_PAINT(PAINT_BUBBLEGUM, 2));
            plat_rect(x - rr / 2, y - rr / 2, 1, 1, PAL_WHITE);
        }
    }
    if (e & FF_CHAIN) {
        int x = fx_(bdx + rx - 2);
        for (k = 0; k < 4; k++) {
            int y = cy + ry / 2 + k * 3 + ui_wave(t + k * 5, 40, 1);
            ui_frame(x - 1 + (k & 1), y, 3, 3, PAL_RAMP(RAMP_STEEL, 4));
        }
    }
    if (e & FF_GLOW) {
        ui_sparkle(fx_(bdx - rx), cy - ry, (t / 2) & 15, lt(f->fin));
        ui_sparkle(fx_(bdx + rx / 2), cy + ry + 2, (t / 2 + 6) & 15, lt(f->fin));
        ui_sparkle(fx_(bdx + rx + 2), cy - ry / 2, (t / 2 + 11) & 15, PAL_WHITE);
    }
}

/* ====================================================================== */
/* special shapes                                                           */
static void draw_boot(const FishInfo *f, int cx, int cy, int r, int t)
{
    int w = r * 5 / 8, h = r, fx0;
    uint8_t c = C(f->body), o = C(dk(dk(f->body))), l = C(lt(f->body));
    (void)cx;
    /* shaft */
    for (fx0 = cy - h - 1; fx0 <= cy + 1; fx0++) span(-w / 2 - 1, w / 2, fx0, o);
    for (fx0 = cy - h; fx0 <= cy; fx0++) span(-w / 2, w / 2 - 1, fx0, fx0 == cy - h ? l : c);
    /* foot */
    ellipse_at(w / 4, cy + r / 4, r * 3 / 4 + 1, r / 3 + 1, o);
    ellipse_at(w / 4, cy + r / 4, r * 3 / 4, r / 3, c);
    span(-w / 2 + 1, w / 2 + r * 3 / 4 - 2, cy + r / 4 + r / 3 - 1, C(PAL_RAMP(RAMP_STONE, 1)));
    if (!g_sil) {
        int k;
        for (k = 0; k < 3; k++) {                                    /* laces */
            int y = cy - h + 4 + k * (h / 4);
            span(-1, 2, y, f->fin);
            span(0, 1, y + 1, f->fin);
        }
        span(-w / 2 + 2, -w / 2 + 2, cy - h + 3, l);
        ellipse_at(w / 2 + r / 3, cy + r / 6, 2, 1, l);
        /* a little puddle drip */
        ui_fill_circle(fx_(w / 2 + r / 2), cy + r / 2 + 2 + (t / 6) % 4, 1, PAL_PAINT(PAINT_SKY, 2));
    }
}

static void draw_can(const FishInfo *f, int cx, int cy, int r, int t)
{
    int w = r * 3 / 4, h = r * 5 / 4, y0 = cy - h / 2;
    uint8_t c = C(f->body), o = C(dk(dk(f->body)));
    (void)cx;
    ui_round_rect(g_cx - w / 2 - 1, y0 - 1, w + 2, h + 2, 2, o);
    plat_rect(g_cx - w / 2, y0, w, h, c);
    plat_rect(g_cx - w / 2, y0, 2, h, C(lt(f->body)));
    plat_rect(g_cx + w / 2 - 2, y0, 2, h, C(dk(f->body)));
    if (!g_sil) {
        int gy = y0 + h / 2 + ui_wave(t, 30, 1);
        plat_rect(g_cx - w / 2, y0 + h / 4, w, h / 2, f->fin);               /* label */
        plat_rect(g_cx - w / 2, y0 + h / 4, w, 1, lt(f->fin));
        /* the ghost bean, peeking */
        ui_fill_circle(g_cx, gy, DOOMO_MAX(2, r / 5), PAL_PAINT(PAINT_ECTOPLASM, 2));
        plat_rect(g_cx - 1, gy - 1, 1, 1, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(g_cx + 1, gy - 1, 1, 1, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(g_cx - w / 2, y0 + 2, w, 1, dk(f->body));                  /* rims */
        plat_rect(g_cx - w / 2, y0 + h - 3, w, 1, dk(f->body));
    }
}

static void draw_sock(const FishInfo *f, int cx, int cy, int r, int t)
{
    int w = r / 2, h = r, k;
    uint8_t c = C(f->body), o = C(dk(dk(f->body))), st = C(f->belly);
    (void)cx; (void)t;
    /* leg */
    for (k = -h; k <= r / 6; k++) span(-w / 2 - 1, w / 2 + 1, cy + k, o);
    for (k = -h + 1; k <= r / 6; k++) span(-w / 2, w / 2, cy + k, ((k + h) / 3) % 3 == 1 ? st : c);
    /* foot */
    ellipse_at(w / 2, cy + r / 4, r * 2 / 3 + 1, r / 4 + 1, o);
    ellipse_at(w / 2, cy + r / 4, r * 2 / 3, r / 4, c);
    if (!g_sil) {
        ellipse_at(w / 2 + r / 2, cy + r / 4, r / 5, r / 5, f->fin);         /* toe */
        ellipse_at(-w / 2 + 2, cy + r / 4, r / 6, r / 5, f->fin);           /* heel */
        span(-w / 2, w / 2, cy - h + 1, PAL_WHITE);                          /* cuff */
        span(-w / 2, w / 2, cy - h + 2, PAL_RAMP(RAMP_STONE, 5));
    }
}

static void draw_eel(const FishInfo *f, int cx, int cy, int r, int t, int fight)
{
    int n = 8 + r / 6, i, seg = DOOMO_MAX(2, (2 * r) / n), sp = fight ? 4 : 2;
    int amp = DOOMO_MAX(1, r / 8) + fight * 2, rad0 = DOOMO_MAX(2, r / 4);
    int hx = 0, hy = 0;
    (void)cx;
    for (i = n - 1; i >= 0; i--) {                       /* outline pass */
        int dx = r - i * seg, y = cy + ui_wave(t * sp + i * 70, 120, amp);
        int rad = DOOMO_MAX(1, rad0 - i * rad0 / (n + 2));
        ellipse_at(dx, y, rad + 1, rad + 1, C(dk(dk(f->body))));
    }
    for (i = n - 1; i >= 0; i--) {
        int dx = r - i * seg, y = cy + ui_wave(t * sp + i * 70, 120, amp);
        int rad = DOOMO_MAX(1, rad0 - i * rad0 / (n + 2));
        uint8_t c = f->pattern == FPAT_RAINBOW ? rainbow_col[i % 6] : f->body;
        ellipse_at(dx, y, rad, rad, C(c));
        if (!g_sil) {
            plat_rect(fx_(dx) - 1, y - rad + 1, 2, 1, lt(c));
            if (rad > 2) plat_rect(fx_(dx) - 1, y + rad - 1, 3, 1, f->pattern == FPAT_RAINBOW ? dk(c) : f->belly);
            if (f->pattern == FPAT_CRACKS && (i & 1))
                plat_rect(fx_(dx), y, 1 + (rad > 3), 1, ((t / 6 + i) & 1) ? PAL_RAMP(RAMP_LAVA, 7) : PAL_RAMP(RAMP_LAVA, 6));
            if (f->pattern == FPAT_SPOTS && (i % 3) == 1) plat_rect(fx_(dx), y - 1, 2, 2, dk(c));
        }
        if (i == n - 1) {                                 /* tail fin */
            int k;
            for (k = 0; k < 4; k++) span(dx - rad - k, dx - rad - k, y - k / 2 - 1, C(f->fin));
            for (k = 0; k < 4; k++) plat_rect(fx_(dx - rad - k), y - k / 2 - 1, 1, k + 2, C(f->fin));
        }
        if (i == 0) { hx = dx; hy = y; }
    }
    /* head */
    ellipse_at(hx + 1, hy, rad0 + 2, rad0 + 1, C(dk(dk(f->body))));
    ellipse_at(hx + 1, hy, rad0 + 1, rad0, C(f->body));
    if (!g_sil) ellipse_at(hx + 1, hy + rad0 / 2, rad0, rad0 / 2, f->belly);
    eye(f, hx + 2, hy - rad0 / 3, DOOMO_MAX(2, rad0 / 2 + 1), t, 0);
    mouth(f, hx + rad0 + 1, hy + rad0 / 2, r / 2, t, fight);
    if (f->extras & FF_BLUSH && !g_sil)
        plat_checker(fx_(hx) - 1, hy + rad0 / 2, 3, 2, PAL_PAINT(PAINT_ROSE, 2));
    if (f->extras & FF_GLOW && !g_sil) {
        ui_sparkle(fx_(hx - r), cy - r / 3, (t / 2) & 15, lt(f->fin));
        ui_sparkle(fx_(hx - r / 2), cy + r / 3, (t / 2 + 7) & 15, PAL_RAMP(RAMP_LAVA, 7));
    }
}

/* ====================================================================== */
void fish_draw(int sp, int cx, int cy, int r, int t, int flags, uint8_t sil)
{
    const FishInfo *f;
    int rx, ry, bdx, tl, wag, fight = (flags & FDRAW_FIGHT) != 0, flap, spd;
    if (sp < 0 || sp >= FISH_COUNT || r < 3 || plat_draw_skipped()) return;
    f = &fish_info[sp];
    g_sil = (flags & FDRAW_SILHOUETTE) ? (sil ? sil : PAL_RAMP(RAMP_STONE, 1)) : 0;
    g_dir = (flags & FDRAW_FLIP) ? -1 : 1;
    g_cx = cx;
    g_lod = r < 15;
    spd = fight ? 4 : (f->style == FSTY_SLEEPY ? 1 : 2);
    switch (f->shape) {
    case FSH_BOOT: draw_boot(f, cx, cy, r, t); g_sil = 0; return;
    case FSH_CAN: draw_can(f, cx, cy, r, t); g_sil = 0; return;
    case FSH_SOCK:
        draw_sock(f, cx, cy, r, t);
        if (!g_sil) extras(f, 0, cy - r + 1, DOOMO_MAX(2, r / 4), 2, r, t, 0);
        g_sil = 0;
        return;
    case FSH_LONG: draw_eel(f, cx, cy, r, t, fight); g_sil = 0; return;
    default: break;
    }
    switch (f->shape) {
    case FSH_ROUND: rx = r * 10 / 16; ry = r * 9 / 16; break;
    case FSH_FLAT: rx = r * 9 / 16; ry = r * 10 / 16; break;
    case FSH_PIKE: rx = r * 12 / 16; ry = r * 5 / 16; break;
    case FSH_BIG: rx = r * 11 / 16; ry = r * 8 / 16; break;
    default: rx = r * 11 / 16; ry = r * 6 / 16; break;
    }
    rx = DOOMO_MAX(rx, 3);
    ry = DOOMO_MAX(ry, 2);
    if (sp == FISH_MOCHI_PUFFER && fight) { rx = rx * 5 / 4; ry = ry * 5 / 4; }   /* puffs up! */
    bdx = r - rx - (r - rx) / 3;                    /* body sits forward       */
    if (f->shape == FSH_ROUND || f->shape == FSH_FLAT) bdx = r / 6;
    tl = DOOMO_MAX(2, r - (r - rx) / 3 - rx + rx / 3);
    tl = DOOMO_MAX(tl, r / 3);
    wag = ui_wave(t * spd + sp * 13, 40, DOOMO_MAX(1, ry / 3) + fight);
    flap = ui_wave(t * spd + 10, 30, 1);
    /* fins behind the body */
    dorsal(f, bdx - rx / 4, cy - ry + 1, DOOMO_MAX(2, rx / 3),
           f->shape == FSH_FLAT ? ry * 2 / 3 : DOOMO_MAX(2, ry / 2), flap);
    if (f->shape == FSH_FLAT) {                        /* angelfish keel fin */
        int k, h = ry / 2;
        for (k = 0; k <= h; k++) {
            int hw = DOOMO_MAX(1, rx / 3 * (h - k) / (h + 1));
            span(bdx - rx / 4 - hw - k / 2, bdx - rx / 4 + hw - k / 2, cy + ry - 1 + k, C(k == h ? dk(f->fin) : f->fin));
        }
    }
    tail(f, bdx - rx + 1, cy, tl, DOOMO_MAX(1, ry / 4), DOOMO_MAX(2, ry * 3 / 4 + 1), wag,
         f->shape == FSH_PIKE || f->shape == FSH_OVAL);
    body(f, bdx, cy, rx, ry);
    pattern(f, bdx, cy, rx, ry, t);
    if (!g_sil && !g_lod && rx >= 6) {                 /* pectoral fin */
        int px = bdx - rx / 8, py = cy + ry / 5, k, n = DOOMO_MAX(2, rx / 4);
        for (k = 0; k < n; k++) {                      /* a little wing    */
            int hw = (n - k + 1) / 2, y = py + k / 2 + (flap > 0 ? k / 3 : 0);
            span(px - k - hw, px - k + hw, y, dk(f->fin));
            if (hw > 1) span(px - k - hw + 1, px - k + hw - 1, y - 1 + (k == 0), f->fin);
        }
        span(px - 1, px, py - 1, lt(f->fin));
    }
    eye(f, bdx + rx * 5 / 9, cy - ry / 4, DOOMO_MAX(2, r / 6), t, (flags & FDRAW_HAPPY) != 0);
    mouth(f, bdx + rx - 1, cy + ry / 4 + (f->shape == FSH_BIG), r, t, fight);
    extras(f, bdx, cy, rx, ry, r, t, bdx + rx * 5 / 9);
    if (f->shape == FSH_BIG && !g_sil) {               /* jowls */
        plat_rect(fx_(bdx + rx - 2), cy + ry / 2, 2, 1, dk(f->body));
    }
    g_sil = 0;
}

int fish_draw_radius(int sp, int size, int rmax)
{
    const FishInfo *f;
    int span_, k;
    if (sp < 0 || sp >= FISH_COUNT) return rmax;
    f = &fish_info[sp];
    span_ = DOOMO_MAX(1, f->size_max - f->size_min);
    k = DOOMO_CLAMP((size - f->size_min) * 100 / span_, 0, 120);
    return rmax * (72 + k * 28 / 120) / 100;
}

/* ====================================================================== */
/* the lake                                                                 */
void lake_layout(LakeView *v)
{
    int hour = town_hour(), k = fish_hour_kind(hour);
    v->w = plat_screen_w();
    v->h = plat_screen_h();
    v->horizon = v->h * 36 / 100;
    v->pier_y = v->h * 66 / 100;
    v->pier_x1 = v->w * 30 / 100;
    v->night = k == FT_NIGHT;
    v->twilight = k == FT_TWILIGHT;
    v->t = scene_ticks();
}

static void bands(int y0, int y1, const uint8_t *cols, int n, int w)
{
    int i;
    for (i = 0; i < n; i++) {
        int a = y0 + (y1 - y0) * i / n, b = y0 + (y1 - y0) * (i + 1) / n;
        plat_rect(0, a, w, b - a, cols[i]);
        if (i > 0) plat_checker(0, a, w, 1, cols[i - 1]);
    }
}

static void cloud(int cx, int cy, int s, uint8_t c, uint8_t d)
{
    ui_fill_ellipse(cx, cy + 1, s * 2, s - 1, d);
    ui_fill_ellipse(cx - s, cy, s, s - 2, c);
    ui_fill_circle(cx, cy - 2, s - 1, c);
    ui_fill_ellipse(cx + s, cy, s, s - 3, c);
}

void lake_draw_sky(const LakeView *v)
{
    static const uint8_t day[6] = { PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7),
                                    PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PEACH, 6) };
    static const uint8_t dusk[6] = { PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 4),
                                     PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_LAVA, 6) };
    static const uint8_t night[6] = { PAL_RAMP(RAMP_SKY, 0), PAL_RAMP(RAMP_SKY, 0), PAL_RAMP(RAMP_SKY, 1),
                                      PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2) };
    int w = v->w, hz = v->horizon, t = (int)v->t, i;
    if (plat_draw_skipped()) return;
    bands(0, hz, v->night ? night : v->twilight ? dusk : day, 6, w);
    if (v->night) {
        static const int8_t st[12][2] = { {5,8},{18,20},{30,5},{44,28},{55,12},{63,38},{72,4},
                                          {81,22},{88,10},{93,33},{12,40},{38,46} };
        for (i = 0; i < 12; i++) {
            int x = st[i][0] * w / 100, y = st[i][1] * hz / 60;
            int ph = (t / 3 + i * 5) & 15;
            if (i & 1) ui_sparkle(x, y, ph, PAL_WHITE);
            else plat_rect(x, y, 1, 1, ph < 8 ? PAL_WHITE : PAL_RAMP(RAMP_SKY, 5));
        }
        /* a crescent moon */
        ui_fill_circle(w * 78 / 100, hz / 3, 9, PAL_RAMP(RAMP_CANDY, 7));
        ui_fill_circle(w * 78 / 100 + 4, hz / 3 - 3, 8, PAL_RAMP(RAMP_SKY, 0));
    } else {
        int sx = w * 80 / 100, sy = v->twilight ? hz - 6 : hz / 3 + 4;
        int r = v->twilight ? 12 : 10;
        ui_fill_circle(sx, sy, r + 5, v->twilight ? PAL_RAMP(RAMP_LAVA, 5) : PAL_RAMP(RAMP_CANDY, 7));
        ui_fill_circle(sx, sy, r + 2, v->twilight ? PAL_RAMP(RAMP_LAVA, 6) : PAL_WHITE);
        ui_fill_circle(sx, sy, r, v->twilight ? PAL_RAMP(RAMP_LAVA, 7) : PAL_RAMP(RAMP_CANDY, 6));
        ui_fill_circle(sx - 3, sy - 3, r / 3, PAL_WHITE);
    }
    /* clouds drift */
    for (i = 0; i < 3; i++) {
        int x = (int)((uint32_t)(t / (6 + i * 3) + (uint32_t)i * 97) % (uint32_t)(w + 60)) - 30;
        int y = hz / 4 + i * hz / 5;
        uint8_t c = v->night ? PAL_RAMP(RAMP_VAPE, 2) : v->twilight ? PAL_RAMP(RAMP_PASTEL_PINK, 6) : PAL_WHITE;
        uint8_t d = v->night ? PAL_RAMP(RAMP_VAPE, 1) : v->twilight ? PAL_RAMP(RAMP_VAPE, 4) : PAL_RAMP(RAMP_SKY, 6);
        cloud(x, y, 5 + (i & 1) * 2, c, d);
    }
    /* far shore: Hillvile's candy hills on the left ... */
    {
        uint8_t hill = v->night ? PAL_RAMP(RAMP_MINT, 1) : PAL_RAMP(RAMP_MINT, 5);
        uint8_t hill2 = v->night ? PAL_RAMP(RAMP_MINT, 0) : PAL_RAMP(RAMP_MINT, 4);
        ui_fill_ellipse(w * 10 / 100, hz, w * 18 / 100, 12, hill2);
        ui_fill_ellipse(w * 32 / 100, hz + 1, w * 16 / 100, 9, hill);
        for (i = 0; i < 3; i++) {                           /* cottages */
            int x = w * (8 + i * 11) / 100, y = hz - 9 + (i == 1) * 3;
            plat_rect(x - 3, y, 7, 5, v->night ? PAL_RAMP(RAMP_PEACH, 2) : PAL_RAMP(RAMP_PEACH, 7));
            plat_rect(x - 4, y - 1, 9, 1, PAL_PAINT(PAINT_BUBBLEGUM, v->night ? 0 : 1));
            plat_rect(x - 3, y - 2, 7, 1, PAL_PAINT(PAINT_BUBBLEGUM, v->night ? 0 : 1));
            plat_rect(x - 1, y - 3, 3, 1, PAL_PAINT(PAINT_BUBBLEGUM, v->night ? 0 : 1));
            plat_rect(x, y + 2, 1, 1, v->night ? PAL_UI_GOLD : PAL_PAINT(PAINT_SKY, 1));
        }
    }
    /* ... and Hellvile's crooked spires on the right, with a smoking volcano */
    {
        uint8_t sp = v->night ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_VAPE, 1);
        int bx = w * 58 / 100, k;
        static const int8_t hs[7] = { 10, 18, 8, 24, 12, 16, 9 };
        for (i = 0; i < 7; i++) {
            int x = bx + i * w * 5 / 100, h = hs[i];
            plat_rect(x - 3, hz - h / 3, 7, h / 3 + 1, sp);            /* stepped spire */
            plat_rect(x - 2, hz - h * 2 / 3, 5, h / 3 + 1, sp);
            plat_rect(x - 1, hz - h + 2, 3, h / 3, sp);
            plat_rect(x, hz - h, 1, 2, sp);
            if (((t / 20 + i) & 3) != 0) plat_rect(x, hz - h / 2, 1, 1, PAL_RAMP(RAMP_LAVA, 6));
        }
        {
            int vx = w * 93 / 100;
            for (k = 0; k < 14; k += 2) plat_rect(vx - 2 - k, hz - 14 + k, 5 + 2 * k, 2, sp);
            plat_rect(vx - 2, hz - 14, 5, 1, PAL_RAMP(RAMP_LAVA, 5));
            for (k = 0; k < 3; k++) {
                int p = (t / 2 + k * 15) % 45;
                ui_fill_circle(vx + p / 6 - 2, hz - 16 - p / 3, 1 + p / 15,
                               v->night ? PAL_RAMP(RAMP_STONE, 1) : PAL_RAMP(RAMP_STONE, 4));
            }
        }
    }
}

void lake_draw_water(const LakeView *v)
{
    static const uint8_t day[5] = { PAL_RAMP(RAMP_LAVA, 6), PAL_RAMP(RAMP_PASTEL_PINK, 5),
                                    PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_LAVA, 4),
                                    PAL_RAMP(RAMP_PASTEL_PINK, 3) };
    static const uint8_t dusk[5] = { PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_PASTEL_PINK, 4),
                                     PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 3),
                                     PAL_RAMP(RAMP_VAPE, 2) };
    static const uint8_t night[5] = { PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 2),
                                      PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 1),
                                      PAL_RAMP(RAMP_BLOOD, 1) };
    const uint8_t *cols = v->night ? night : v->twilight ? dusk : day;
    int w = v->w, h = v->h, hz = v->horizon, t = (int)v->t, i;
    uint8_t shine = v->night ? PAL_RAMP(RAMP_LAVA, 5) : PAL_RAMP(RAMP_CANDY, 7);
    if (plat_draw_skipped()) return;
    bands(hz, h, cols, 5, w);
    plat_rect(0, hz, w, 1, v->night ? PAL_RAMP(RAMP_LAVA, 3) : PAL_WHITE);
    /* shimmer: dashes that drift, longer and sparser near the viewer */
    for (i = 0; i < 9; i++) {
        int y = hz + 3 + i * i * (h - hz - 6) / 81;
        int len = 3 + i, gap = 22 + i * 7, x;
        int off = (t * (1 + (i & 1)) / 3 + i * 17) % gap;
        for (x = -gap + off; x < w; x += gap)
            plat_rect(x + (i * 13) % 7, y, len, 1, ((x / gap + i) & 1) ? shine : ui_lighter(cols[DOOMO_MIN(4, i / 2)]));
    }
    /* sun / moon glitter column */
    {
        int sx = w * 80 / 100, k;
        for (k = 0; k < 6; k++) {
            int y = hz + 2 + k * 4, ww = 6 - k / 2 + ui_wave(t + k * 9, 30, 2);
            plat_rect(sx - ww / 2, y, ww, 1, v->night ? PAL_RAMP(RAMP_CANDY, 6) : PAL_WHITE);
        }
    }
    /* the lake bubbles (it's toasty) */
    for (i = 0; i < 4; i++) {
        int per = 70 + i * 13, p = (t + i * 31) % per;
        int x = (i * 61 + 37) % (w - 40) + 30, y = hz + 8 + (i * 23) % (h - hz - 20);
        if (x < v->pier_x1 + 6 && y > v->pier_y - 4) continue;
        if (p < 24) ui_fill_circle(x, y, 1 + p / 8, ui_lighter(cols[2]));
        else if (p < 34) ring(x, y, 3 + (p - 24) / 2, 1 + (p - 24) / 5, ui_lighter(cols[1]));
    }
    /* lava lily pads */
    for (i = 0; i < 2; i++) {
        int x = w * (55 + i * 30) / 100, y = hz + (h - hz) * (2 + i * 4) / 10 + ui_wave(t + i * 40, 90, 1);
        int r = 5 + i * 3;
        ui_fill_ellipse(x, y + 1, r + 1, r / 3 + 1, PAL_RAMP(RAMP_GRASS, v->night ? 0 : 2));
        ui_fill_ellipse(x, y, r, r / 3, PAL_RAMP(RAMP_GRASS, v->night ? 1 : 4));
        plat_rect(x, y - r / 3, 2, r / 3 + 1, cols[1]);                     /* notch */
        ui_fill_circle(x - r / 3, y - 1, 1 + i, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        plat_rect(x - r / 3, y - 2, 1, 1, PAL_WHITE);
    }
}

void lake_draw_pier(const LakeView *v)
{
    int y = v->pier_y, x1 = v->pier_x1, h = v->h, k, t = (int)v->t;
    uint8_t top = PAL_RAMP(RAMP_WOOD, v->night ? 3 : 5), face = PAL_RAMP(RAMP_WOOD, v->night ? 1 : 3);
    uint8_t post = PAL_RAMP(RAMP_WOOD, v->night ? 0 : 2), ink = PAL_RAMP(RAMP_WOOD, 0);
    if (plat_draw_skipped()) return;
    /* posts + their ripples */
    for (k = 6; k < x1; k += 22) {
        plat_rect(k, y + 6, 5, h - y - 6, post);
        plat_rect(k, y + 6, 1, h - y - 6, face);
        ring(k + 2, y + 18 + ui_wave(t + k, 60, 1), 6, 2, ui_lighter(PAL_RAMP(RAMP_PASTEL_PINK, 5)));
    }
    plat_rect(0, y - 1, x1 + 1, 1, ink);
    plat_rect(0, y, x1, 4, top);
    plat_rect(0, y, x1, 1, ui_lighter(top));
    for (k = 9; k < x1; k += 12) plat_rect(k, y, 1, 4, face);
    plat_rect(0, y + 4, x1, 3, face);
    plat_rect(0, y + 7, x1 + 1, 1, ink);
    plat_rect(x1, y - 1, 1, 9, ink);
    /* a heart lantern at the end of the pier */
    {
        int lx = x1 - 4, ly = y - 26, glow = (t / 8) & 1;
        plat_rect(lx, ly, 2, 26, post);
        plat_rect(lx - 4, ly, 8, 1, post);
        ui_heart(lx - 3, ly + 6 + ui_wave(t, 60, 1), 3, glow ? PAL_PAINT(PAINT_CHERRY, 2) : PAL_PAINT(PAINT_CHERRY, 1));
        if (v->night || v->twilight) ui_sparkle(lx - 3, ly + 5, (t / 3) & 15, PAL_UI_GOLD);
    }
}

/* The player's own chibi with the rod. pose 0 idle, 1 wind-up, 2 cast,
 * 3 reeling, 4 cheering. bend = rod tip droop (px). */
void lake_draw_angler(const LakeView *v, int x, int y, int pose, int bend, int rod,
                      int *tipx, int *tipy)
{
    const DoomoImage *img;
    int t = (int)v->t, frame = 0, bob = 0, hx, hy, tx, ty, k;
    const FishRod *rd = &fish_rods[DOOMO_CLAMP(rod, 0, ROD_COUNT - 1)];
    if (pose == 3) frame = (t / 4) & 3;
    if (pose == 4) bob = ui_wave(t * 3, 30, 3) > 0 ? -3 : 0;
    else bob = ui_wave(t, 60, 1) > 0 ? -1 : 0;
    img = bodyspr_image(DOOMO_PLAYER_ID, 2, frame);
    hx = x + 22;
    hy = y + 28 + bob;
    switch (pose) {
    case 1: tx = hx - 18; ty = hy - 30; break;                  /* wound up  */
    case 2: tx = hx + 30; ty = hy - 20; break;                  /* flung     */
    default: tx = hx + 26; ty = hy - 30 + bend; break;
    }
    if (!plat_draw_skipped()) {
        fc_shadow(x + 16, y + 48, 12);
        plat_image(x, y + bob, img, 2, 0, 0);
        /* the rod: a bent pole, 2 px thick at the grip */
        for (k = 0; k < 8; k++) {
            int ax = hx + (tx - hx) * k / 8, ay = hy + (ty - hy) * k / 8 + bend * k * k / 128;
            int bx = hx + (tx - hx) * (k + 1) / 8, by = hy + (ty - hy) * (k + 1) / 8 + bend * (k + 1) * (k + 1) / 128;
            plat_line(ax, ay, bx, by, rd->color);
            if (k < 5) plat_line(ax + 1, ay, bx + 1, by, ui_darker(rd->color));
        }
        plat_rect(tx - 1, ty + bend / 2 - 1, 2, 2, rd->tip);
        ui_fill_circle(hx, hy, 2, PAL_RAMP(RAMP_STONE, 4));          /* reel   */
        plat_rect(hx - 1, hy - 1, 1, 1, PAL_WHITE);
    }
    if (tipx) *tipx = tx;
    if (tipy) *tipy = ty + bend / 2;
}

void lake_draw_ripple(int x, int y, int r, uint8_t c)
{
    if (r < 2) return;
    ring(x, y, r, DOOMO_MAX(1, r / 3), c);
}

void lake_draw_bobber(int x, int y, int t, int under)
{
    if (plat_draw_skipped()) return;
    if (under) {
        lake_draw_ripple(x, y + 1, 5 + (t / 2) % 4, PAL_WHITE);
        plat_rect(x - 1, y - 1 - (t / 3) % 3, 2, 2, PAL_WHITE);
        return;
    }
    ui_fill_ellipse(x, y + 2, 5, 1, ui_lighter(PAL_RAMP(RAMP_PASTEL_PINK, 5)));
    ui_fill_circle(x, y, 4, PAL_PAINT(PAINT_CHERRY, 0));
    ui_fill_circle(x, y, 3, PAL_PAINT(PAINT_CHERRY, 1));
    plat_rect(x - 3, y + 1, 7, 2, PAL_WHITE);
    plat_rect(x - 2, y + 3, 5, 1, PAL_RAMP(RAMP_STONE, 5));
    plat_rect(x - 2, y - 2, 1, 1, PAL_WHITE);
    plat_rect(x, y - 6, 1, 3, PAL_RAMP(RAMP_STONE, 1));
    ui_heart(x, y - 7, 2, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    (void)t;
}

void lake_draw_shadow(int x, int y, int r)
{
    int dy, ry = DOOMO_MAX(1, r / 3);
    for (dy = -ry; dy <= ry; dy++) {
        int hw = ehw(r, ry, dy);
        if (hw > 0) plat_checker(x - hw, y + dy, hw * 2, 1, PAL_RAMP(RAMP_VAPE, 0));
    }
}
