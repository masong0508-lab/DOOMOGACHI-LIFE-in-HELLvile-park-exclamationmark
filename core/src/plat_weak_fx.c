/* plat_weak_fx.c - weak software defaults for the comic post-FX and squash
 * platform hooks (platform.h, S0 block; docs/design/story/SPEC.md sec.4, R12).
 * Owner: S0 -> S1.
 *
 * These are permanent fallbacks, not stubs: a platform without a hardware
 * path (the native runner, or the GBA inside a 3D view, SPEC C7.4) still
 * gets a correct, cheaper look, with the timing unchanged. They never log
 * "STUB:". GBA (gba/src/postfx.c, obj.c) and PS1 (ps1/src/postfx.c,
 * xform.c) override them with strong definitions (S1).
 *
 * Integer only. Everything draws through the existing plat_* 2D layer.
 */
#include "doomo/doomo.h"

#define WEAK __attribute__((weak))

/* Nearest integer scale (1..4) of a Q8.8 squash pair: 0.9/1.1 -> 1. */
static int nearest_scale(int sx_q8, int sy_q8)
{
    int ax = sx_q8 < 0 ? -sx_q8 : sx_q8;
    int ay = sy_q8 < 0 ? -sy_q8 : sy_q8;
    int s = ((ax + ay) / 2 + 128) >> 8;
    return DOOMO_CLAMP(s, 1, 4);
}

WEAK void plat_image_xform(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8,
                           int flags, uint8_t color)
{
    int s;
    if (!img || !img->w || !img->h || sx_q8 == 0 || sy_q8 == 0) return;
    s = nearest_scale(sx_q8, sy_q8);
    if (sx_q8 < 0) flags ^= IMG_FLIPX;
    plat_image(ax - (int)img->w * s / 2, ay - (int)img->h * s, img, s, flags, color);
}

WEAK void plat_post_offset(int dx, int dy) { (void)dx; (void)dy; }

WEAK void plat_post_scale(int sx_q8, int sy_q8, int cx, int cy)
{
    (void)sx_q8; (void)sy_q8; (void)cx; (void)cy;
}

/* Colour over the whole frame: solid at 16, a 50% checker from 8, a
 * colormap darken for black fades, nothing below 8 for other colours (too
 * faint to matter without blending hardware). */
WEAK void plat_post_blend(uint8_t color, int level16)
{
    int w = plat_screen_w(), h = plat_screen_h();
    if (level16 <= 0) return;
    if (level16 >= 16) { plat_rect(0, 0, w, h, color); return; }
    if (color == PAL_BLACK) { plat_shade_rect(0, 0, w, h, 15 - level16 * 15 / 16); return; }
    if (level16 >= 8) plat_checker(0, 0, w, h, color);
}

static int isqrt32(uint32_t v)
{
    uint32_t r = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (int)r;
}

/* Black outside a circle of radius r around (cx, cy), one rect pair per
 * row. The heart shape falls back to the circle in software. */
WEAK void plat_post_iris(int cx, int cy, int r, int heart, uint8_t rim)
{
    int w = plat_screen_w(), h = plat_screen_h(), y;
    (void)heart;
    if (r < 0) return;
    for (y = 0; y < h; y++) {
        int dy = y - cy, half, l, rr;
        if (dy < 0) dy = -dy;
        if (dy >= r) { plat_rect(0, y, w, 1, PAL_BLACK); continue; }
        half = isqrt32((uint32_t)(r * r - dy * dy));
        l = cx - half;
        rr = cx + half;
        if (l > 0) plat_rect(0, y, DOOMO_MIN(l, w), 1, PAL_BLACK);
        if (rr < w) plat_rect(DOOMO_MAX(rr, 0), y, w - DOOMO_MAX(rr, 0), 1, PAL_BLACK);
        if (rim) {
            if (l >= 0 && l < w) plat_rect(l, y, 1, 1, rim);
            if (rr - 1 >= 0 && rr - 1 < w) plat_rect(rr - 1, y, 1, 1, rim);
        }
    }
}

WEAK void plat_pal_lerp(int pal_id, int level16) { (void)pal_id; (void)level16; }

WEAK void plat_room_ui_opaque(int x, int y, int w, int h)
{
    (void)x; (void)y; (void)w; (void)h;
}
