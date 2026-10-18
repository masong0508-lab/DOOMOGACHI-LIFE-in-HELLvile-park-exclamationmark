/* xform.c - plat_image_xform on the PS1: each 128-texel tile of the image
 * is one POLY_FT4 scaled about the image's bottom-centre anchor (owner:
 * S1; platform.h S0 block, docs/design/story/SPEC.md sec.7 S1). Same
 * texture cache, flip and silhouette CLUTs as plat_image (gfx2d.c).
 */
#pragma GCC optimize("Os")
#include <psxgpu.h>
#include "ps1.h"

static void xform_pass(int x0, int y0, const DoomoImage *img, int asx, int sy, int flip,
                       int sil, uint8_t color)
{
    int tx, ty, ntx = (img->w + IMG_TILE - 1) / IMG_TILE, nty = (img->h + IMG_TILE - 1) / IMG_TILE;
    uint16_t silclut = sil ? vram_sil_clut(color) : 0;
    for (ty = 0; ty < nty; ty++)
        for (tx = 0; tx < ntx; tx++) {
            TexRef r;
            POLY_FT4 *p;
            int u0 = tx * IMG_TILE, v0 = ty * IMG_TILE;
            int tw = DOOMO_MIN(img->w - u0, IMG_TILE), th = DOOMO_MIN(img->h - v0, IMG_TILE);
            /* flipped tiles are counted from the right edge of the image */
            int l = flip ? img->w - u0 - tw : u0;
            int X0 = x0 + ((l * asx) >> 8), X1 = x0 + (((l + tw) * asx) >> 8);
            int Y0 = y0 + ((v0 * sy) >> 8), Y1 = y0 + (((v0 + th) * sy) >> 8);
            if (X1 <= X0 || Y1 <= Y0 || X0 >= SCR_W || X1 <= 0 || Y0 >= SCR_H || Y1 <= 0) continue;
            if (!vram_image(img, tx | (flip << 8), ty, &r)) continue;
            p = (POLY_FT4 *)gfx_alloc(sizeof(POLY_FT4));
            if (!p) return;
            setPolyFT4(p);
            setShadeTex(p, 1);
            setXY4(p, X0, Y0, X1, Y0, X0, Y1, X1, Y1);
            setUV4(p, r.u, r.v, r.u + tw, r.v, r.u, r.v + th, r.u + tw, r.v + th);
            p->tpage = r.tpage;
            p->clut = sil ? silclut : r.clut;
            gfx_chain(p);
            g_tpage_cur = r.tpage;           /* a textured poly sets the texpage too */
        }
}

void plat_image_xform(int ax, int ay, const DoomoImage *img, int sx_q8, int sy_q8,
                      int flags, uint8_t color)
{
    int flip, asx, dw, dh, x0, y0;
    if (!img || !img->pix || !img->w || !img->h || sx_q8 == 0 || sy_q8 <= 0) return;
    flip = (flags & IMG_FLIPX) ? 1 : 0;
    if (sx_q8 < 0) flip ^= 1;
    asx = DOOMO_MIN(sx_q8 < 0 ? -sx_q8 : sx_q8, 4 * 256);
    sy_q8 = DOOMO_MIN(sy_q8, 4 * 256);
    dw = (img->w * asx) >> 8;
    dh = (img->h * sy_q8) >> 8;
    x0 = ax - dw / 2;
    y0 = ay - dh;
    if (flags & IMG_SHADOW) xform_pass(x0 + 1, y0 + 1, img, asx, sy_q8, flip, 1, PAL_BLACK);
    xform_pass(x0, y0, img, asx, sy_q8, flip, (flags & IMG_SILHOUETTE) ? 1 : 0, color);
}
