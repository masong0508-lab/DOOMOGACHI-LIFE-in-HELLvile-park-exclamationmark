/* gfx2d.c - the 2D half of platform.h on the PS1 GPU.
 * Rects are TILEs, dimming is semi-transparent TILEs, text is one SPRT per
 * glyph from the 4bpp font page with a per-colour CLUT (exact palette
 * colours, no dithering), images come from the VRAM image cache and are
 * drawn as SPRTs (scale 1) or raw-textured quads (scale 2..4).
 */
#include <string.h>
#include "ps1.h"

int plat_screen_w(void) { return SCR_W; }
int plat_screen_h(void) { return SCR_H; }

static void tile(int x, int y, int w, int h, uint32_t rgb, int semi)
{
    TILE *t;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0)
        return;
    t = (TILE *)gfx_alloc(sizeof(TILE));
    if (!t)
        return;
    setTile(t);
    setColor0(t, rgb);
    setSemiTrans(t, semi);
    setXY0(t, x, y);
    setWH(t, w, h);
    gfx_chain(t);
}

void plat_clear(uint8_t color)
{
    tile(0, 0, SCR_W, SCR_H, g_rgb[color], 0);
}

void plat_rect(int x, int y, int w, int h, uint8_t color)
{
    tile(x, y, w, h, g_rgb[color], 0);
}

void plat_shade_rect(int x, int y, int w, int h, int level)
{
    /* target: colour * level/15 (doomo_colormap[level]). The GPU has no
     * multiply blend, so halve with B/2+0 (mode 0) n times, then take the
     * rest off with a subtractive pass (mode 2). */
    int halves, s, f256;
    if (level >= 15)
        return;
    if (level <= 0) {
        tile(x, y, w, h, 0, 0);
        return;
    }
    halves = level >= 11 ? 0 : level >= 6 ? 1 : level >= 3 ? 2 : 3;
    f256 = (level * 256 / 15) << halves;      /* remaining factor x256 */
    if (f256 > 256) f256 = 256;
    if (halves) {
        int i;
        gfx_tpage(getTPage(0, 0, 0, 0));
        for (i = 0; i < halves; i++)
            tile(x, y, w, h, 0, 1);
    }
    s = ((256 - f256) * 150) >> 8;
    if (s > 2) {
        gfx_tpage(getTPage(0, 2, 0, 0));
        tile(x, y, w, h, (uint32_t)s | ((uint32_t)s << 8) | ((uint32_t)s << 16), 1);
    }
}

/* Cohen-Sutherland against the screen. */
static int outcode(int x, int y)
{
    int c = 0;
    if (x < 0) c |= 1; else if (x >= SCR_W) c |= 2;
    if (y < 0) c |= 4; else if (y >= SCR_H) c |= 8;
    return c;
}

void plat_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    LINE_F2 *l;
    int c0 = outcode(x0, y0), c1 = outcode(x1, y1), guard = 0;
    while ((c0 | c1) && guard++ < 8) {
        int c, x = 0, y = 0;
        if (c0 & c1)
            return;
        c = c0 ? c0 : c1;
        if (c & 8)      { x = x0 + (x1 - x0) * (SCR_H - 1 - y0) / (y1 - y0); y = SCR_H - 1; }
        else if (c & 4) { x = x0 + (x1 - x0) * (0 - y0) / (y1 - y0); y = 0; }
        else if (c & 2) { y = y0 + (y1 - y0) * (SCR_W - 1 - x0) / (x1 - x0); x = SCR_W - 1; }
        else            { y = y0 + (y1 - y0) * (0 - x0) / (x1 - x0); x = 0; }
        if (c == c0) { x0 = x; y0 = y; c0 = outcode(x0, y0); }
        else         { x1 = x; y1 = y; c1 = outcode(x1, y1); }
    }
    if (c0 | c1)
        return;
    l = (LINE_F2 *)gfx_alloc(sizeof(LINE_F2));
    if (!l)
        return;
    setLineF2(l);
    setColor0(l, g_rgb[color]);
    setXY2(l, x0, y0, x1, y1);
    gfx_chain(l);
    /* the GPU leaves out the end point; draw it */
    if (x0 != x1 || y0 != y1)
        tile(x1, y1, 1, 1, g_rgb[color], 0);
}

void plat_text(int x, int y, const char *s, uint8_t color)
{
    int x0 = x;
    uint16_t clut = vram_font_clut(color);
    int started = 0;
    if (!s)
        return;
    for (; *s; s++) {
        unsigned c = (uint8_t)*s;
        if (c == '\n') {
            x = x0;
            y += FONT_LINE;
            continue;
        }
        if (c > ' ' && c <= FONT_LAST && x > -6 && x < SCR_W && y > -8 && y < SCR_H) {
            unsigned g = c - FONT_FIRST;
            SPRT *p;
            if (!started) {
                gfx_tpage(vram_font_tpage());
                started = 1;
            }
            p = (SPRT *)gfx_alloc(sizeof(SPRT));
            if (!p)
                return;
            setSprt(p);
            setShadeTex(p, 1);
            setXY0(p, x, y);
            setUV0(p, (g & 15) * 8, VR_FONT_Y + (g >> 4) * 8);
            setWH(p, 5, 8);
            p->clut = clut;
            gfx_chain(p);
        }
        x += FONT_ADVANCE;
    }
}

/* One image tile at screen position (sx,sy), tile size tw x th. */
static void blit(int sx, int sy, int tw, int th, int scale, const TexRef *r,
                 uint16_t clut)
{
    if (sx >= SCR_W || sy >= SCR_H || sx + tw * scale <= 0 || sy + th * scale <= 0)
        return;
    if (scale == 1) {
        SPRT *p;
        DR_TPAGE *t = (DR_TPAGE *)gfx_alloc(sizeof(DR_TPAGE) + sizeof(SPRT));
        if (!t)
            return;
        setDrawTPage(t, 1, 1, r->tpage);
        gfx_chain(t);
        p = (SPRT *)(t + 1);
        setSprt(p);
        setShadeTex(p, 1);
        setXY0(p, sx, sy);
        setUV0(p, r->u, r->v);
        setWH(p, tw, th);
        p->clut = clut;
        gfx_chain(p);
    } else {
        POLY_FT4 *p = (POLY_FT4 *)gfx_alloc(sizeof(POLY_FT4));
        int w = tw * scale, h = th * scale;
        if (!p)
            return;
        setPolyFT4(p);
        setShadeTex(p, 1);
        setXYWH(p, sx, sy, w, h);
        setUV4(p, r->u, r->v, r->u + tw, r->v, r->u, r->v + th, r->u + tw, r->v + th);
        p->tpage = r->tpage;
        p->clut = clut;
        gfx_chain(p);
    }
}

static void image_pass(int x, int y, const DoomoImage *img, int scale, int flip,
                       int sil, uint8_t color)
{
    int tx, ty, ntx = (img->w + IMG_TILE - 1) / IMG_TILE;
    int nty = (img->h + IMG_TILE - 1) / IMG_TILE;
    uint16_t silclut = sil ? vram_sil_clut(color) : 0;
    for (ty = 0; ty < nty; ty++)
        for (tx = 0; tx < ntx; tx++) {
            TexRef r;
            int x0 = tx * IMG_TILE, y0 = ty * IMG_TILE;
            int tw = img->w - x0, th = img->h - y0;
            int sx;
            if (tw > IMG_TILE) tw = IMG_TILE;
            if (th > IMG_TILE) th = IMG_TILE;
            /* flipped tiles are counted from the right edge of the image */
            sx = flip ? x + (img->w - x0 - tw) * scale : x + x0 * scale;
            if (sx >= SCR_W || sx + tw * scale <= 0 ||
                y + y0 * scale >= SCR_H || y + (y0 + th) * scale <= 0)
                continue;
            if (!vram_image(img, tx | (flip << 8), ty, &r))
                continue;
            blit(sx, y + y0 * scale, tw, th, scale, &r, sil ? silclut : r.clut);
        }
}

void plat_image(int x, int y, const DoomoImage *img, int scale, int flags,
                uint8_t color)
{
    int flip;
    if (!img || !img->pix || !img->w || !img->h)
        return;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    flip = (flags & IMG_FLIPX) ? 1 : 0;
    if (flags & IMG_SHADOW)
        image_pass(x + 1, y + 1, img, scale, flip, 1, PAL_BLACK);
    image_pass(x, y, img, scale, flip, (flags & IMG_SILHOUETTE) ? 1 : 0, color);
}
