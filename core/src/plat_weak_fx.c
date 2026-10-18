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
 *   plat_image_xform  near-integer scales: plat_image; otherwise a
 *                     nearest-neighbour blit as horizontal rect runs (reads
 *                     img->pix, so a platform with virtual pixel pointers,
 *                     the PS1, must override it - it does)
 *   plat_post_offset  no-op (software paths use camfx_offset())
 *   plat_post_scale   no-op
 *   plat_post_blend   solid at 16, a colormap darken for black, a 50%
 *                     checker from level 8 for other colours
 *   plat_post_iris    black row spans around camfx_iris_row()'s shape
 *   plat_pal_lerp     no-op (needs a palette: GBA and PS1 implement it)
 *
 * Integer only. Everything draws through the existing plat_* 2D layer.
 */
#include "doomo/doomo.h"
#include "doomo/camfx.h"

#define WEAK __attribute__((weak))

/* Scaled blit as horizontal runs of equal colour (one plat_rect each). */
static void xform_runs(int x0, int y0, int dw, int dh, const DoomoImage *img, int flip,
                       int sil, uint8_t color)
{
    int dy, dx, w = img->w, h = img->h;
    int32_t stepx = ((int32_t)w << 16) / dw, stepy = ((int32_t)h << 16) / dh;
    int sw = plat_screen_w(), shh = plat_screen_h();
    for (dy = 0; dy < dh; dy++) {
        int y = y0 + dy, run_x = 0, run_c = 0;
        const uint8_t *row;
        int32_t fx;
        if (y < 0 || y >= shh) continue;
        row = img->pix + (size_t)((dy * stepy) >> 16) * w;
        fx = stepx >> 1;
        for (dx = 0; dx <= dw; dx++, fx += stepx) {
            int c = 0;
            if (dx < dw) {
                int sx = (int)(fx >> 16);
                c = row[flip ? w - 1 - sx : sx];
                if (c && sil) c = color;
            }
            if (c != run_c) {
                if (run_c && x0 + run_x < sw) plat_rect(x0 + run_x, y, dx - run_x, 1, (uint8_t)run_c);
                run_c = c;
                run_x = dx;
            }
        }
    }
}

WEAK void plat_image_xform(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8,
                           int flags, uint8_t color)
{
    int asx, asy, s, dw, dh, flip;
    if (!img || !img->pix || !img->w || !img->h || sx_q8 == 0 || sy_q8 <= 0) return;
    if (plat_draw_skipped()) return;
    flip = (flags & IMG_FLIPX) ? 1 : 0;
    if (sx_q8 < 0) flip ^= 1;
    asx = sx_q8 < 0 ? -sx_q8 : sx_q8;
    asy = sy_q8;
    s = (asx + asy + 256) / 512;
    if (s >= 1 && s <= 4 && asx == s * 256 && asy == s * 256) {
        plat_image(ax - (int)img->w * s / 2, ay - (int)img->h * s, img, s,
                   (flags & ~IMG_FLIPX) | (flip ? IMG_FLIPX : 0), color);
        return;
    }
    dw = ((int)img->w * asx + 128) >> 8;
    dh = ((int)img->h * asy + 128) >> 8;
    if (dw <= 0 || dh <= 0) return;
    if (flags & IMG_SHADOW)
        xform_runs(ax - dw / 2 + 1, ay - dh + 1, dw, dh, img, flip, 1, PAL_BLACK);
    xform_runs(ax - dw / 2, ay - dh, dw, dh, img, flip, (flags & IMG_SILHOUETTE) ? 1 : 0, color);
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

/* Black outside the iris, one to three rects per row, plus a 1 px rim. */
WEAK void plat_post_iris(int cx, int cy, int r, int heart, uint8_t rim)
{
    int w = plat_screen_w(), h = plat_screen_h(), y;
    if (r < 0) return;
    if (plat_draw_skipped()) return;
    for (y = 0; y < h; y++) {
        int sp[4], n = camfx_iris_row(y, cx, cy, r, heart, sp), x = 0, k;
        for (k = 0; k < n; k++) {
            int l = DOOMO_CLAMP(sp[k * 2], 0, w), rr = DOOMO_CLAMP(sp[k * 2 + 1], 0, w);
            if (l > x) plat_rect(x, y, l - x, 1, PAL_BLACK);
            if (rim && rr > l) {
                if (sp[k * 2] >= 0) plat_rect(l, y, 1, 1, rim);
                if (sp[k * 2 + 1] <= w) plat_rect(rr - 1, y, 1, 1, rim);
            }
            if (rr > x) x = rr;
        }
        if (x < w) plat_rect(x, y, w - x, 1, PAL_BLACK);
    }
}

WEAK void plat_pal_lerp(int pal_id, int level16) { (void)pal_id; (void)level16; }

WEAK void plat_room_ui_opaque(int x, int y, int w, int h)
{
    (void)x; (void)y; (void)w; (void)h;
}
