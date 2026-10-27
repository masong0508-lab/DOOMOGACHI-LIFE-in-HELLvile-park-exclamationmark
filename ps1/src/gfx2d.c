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

/* ---- rect batching ----------------------------------------------------------
 * The core draws its cutesy UI with thousands of plat_rect spans per frame
 * (shaded shapes row by row, 50% checker seams, polka dots). Each call
 * becomes one GPU prim, so consecutive rects are merged here:
 *   - a rect directly below / right of the previous one, same colour and
 *     same width / height, extends the previous TILE in place
 *   - a row of single pixels with stride 2 (a checker seam) is collected
 *     and drawn as ONE 50% semi-transparent tile, which is what the
 *     checker is meant to look like
 * Nothing else is ever drawn between the merged pieces, so the result is
 * identical in painter's order. */
int g_run_kind;            /* RUN_* of the open run (ps1.h gfx2d_flush) */
#define s_run_kind g_run_kind
static struct {
    uint32_t *prim;
    int x, y, w, h, n;
    int nx, ny;            /* next x of a dot run / next y of a tile run */
    int c;                 /* -1 = none */
} s_run = { 0, 0, 0, 0, 0, 0, 0, 0, -1 };

void gfx2d_reset(void) { s_run_kind = RUN_NONE; s_run.c = -1; }

static inline __attribute__((always_inline))
uint32_t *emit_tile(int x, int y, int w, int h, uint32_t rgb, int semi)
{
    uint32_t *p = (uint32_t *)g_prim_ptr;
    if (g_prim_ptr + 16 > g_prim_end)
        return NULL;
    g_prim_ptr += 16;
    p[1] = rgb | (semi ? 0x62000000u : 0x60000000u);
    p[2] = ((uint32_t)y << 16) | (uint32_t)x;
    p[3] = ((uint32_t)h << 16) | (uint32_t)w;
    gfx_link_raw(p, 3);
    return p;
}

static void emit_dot(int x, int y, uint32_t rgb)
{
    uint32_t *p = (uint32_t *)g_prim_ptr;
    if (g_prim_ptr + 12 > g_prim_end)
        return;
    g_prim_ptr += 12;
    p[1] = rgb | 0x68000000u;           /* TILE_1 */
    p[2] = ((uint32_t)y << 16) | (uint32_t)x;
    gfx_link_raw(p, 2);
}

/* the 50/50 semi-transparency texpage for the blended checker tiles */
static void tpage_half(void)
{
    DR_TPAGE *t;
    if (g_tpage_cur == getTPage(0, 0, 0, 0))
        return;                             /* already set (a run of rows) */
    t = (DR_TPAGE *)gfx_alloc(sizeof(DR_TPAGE));
    if (t) {
        setDrawTPage(t, 1, 1, getTPage(0, 0, 0, 0));
        gfx_link_raw((uint32_t *)t, 1);
        g_tpage_cur = getTPage(0, 0, 0, 0);
    }
}

void gfx2d_flush_run(void)
{
    if (s_run_kind == RUN_DOTS) {
        s_run_kind = RUN_NONE;
        if (s_run.n >= 4) {
            tpage_half();
            emit_tile(s_run.x, s_run.y, s_run.n * 2 - 1, 1, g_rgb[s_run.c], 1);
        } else {
            int i;
            for (i = 0; i < s_run.n; i++)
                emit_dot(s_run.x + i * 2, s_run.y, g_rgb[s_run.c]);
        }
    }
    s_run_kind = RUN_NONE;
    s_run.c = -1;
}

static void tile(int x, int y, int w, int h, uint32_t rgb, int semi)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0)
        return;
    gfx2d_flush();
    emit_tile(x, y, w, h, rgb, semi);
}

void plat_clear(uint8_t color)
{
    tile(0, 0, SCR_W, SCR_H, g_rgb[color], 0);
}

/* everything but the run continuations: kept out of plat_rect() so the
 * common case is a leaf without a stack frame (the core calls this a few
 * thousand times per frame) */
static void __attribute__((noinline)) rect_slow(int x, int y, int w, int h, uint8_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    if (w <= 0 || h <= 0)
        return;
    if (s_run.c == color) {
        if (s_run_kind == RUN_TILE) {
            if (y == s_run.y && h == s_run.h && x == s_run.x + s_run.w) {
                s_run.w += w;                       /* grow rightward */
                s_run.prim[3] = ((uint32_t)s_run.h << 16) | (uint32_t)s_run.w;
                return;
            }
        } else if (s_run_kind == RUN_DOTS && w == 1 && h == 1 && y == s_run.y &&
                   s_run.n == 1 && x == s_run.x + 1) {
            /* two adjacent pixels: it is a span after all */
            s_run_kind = RUN_NONE;
            x = s_run.x;
            w = 2;
        }
    }
    if (s_run_kind == RUN_DOTS)
        gfx2d_flush_run();
    if (w == 1 && h == 1) {
        s_run_kind = RUN_DOTS;
        s_run.x = x; s_run.y = y; s_run.n = 1; s_run.nx = x + 2; s_run.c = color;
        return;
    }
    s_run.prim = emit_tile(x, y, w, h, g_rgb[color], 0);
    if (s_run.prim) {
        s_run_kind = RUN_TILE;
        s_run.x = x; s_run.y = y; s_run.w = w; s_run.h = h; s_run.ny = y + h;
        s_run.c = color;
    } else {
        s_run_kind = RUN_NONE;
        s_run.c = -1;
    }
}

void plat_rect(int x, int y, int w, int h, uint8_t color)
{
    /* fast paths: continuing the current run needs no clipping beyond the
     * screen-edge test */
    if (color == s_run.c) {
        if (s_run_kind == RUN_DOTS) {
            if (y == s_run.y && x == s_run.nx && (w | h) == 1 && (unsigned)x < SCR_W) {
                s_run.n++;
                s_run.nx += 2;
                return;
            }
        } else if (s_run_kind == RUN_TILE) {
            if (x == s_run.x && w == s_run.w && y == s_run.ny && (unsigned)(y + h) <= SCR_H &&
                y >= 0) {
                s_run.h += h;
                s_run.ny += h;
                s_run.prim[3] = ((uint32_t)s_run.h << 16) | (uint32_t)s_run.w;
                return;
            }
        }
    }
    rect_slow(x, y, w, h, color);
}

/* 50% checkerboard (platform.h): each row of 4+ dots is ONE 50% semi-
 * transparent TILE from its first to its last dot, exactly what a row of
 * stride-2 single-pixel rects becomes in rect_slow(); shorter rows are
 * drawn as dots. Rows of a taller checker alternate between two extents
 * (even rows start at x, odd rows at x + 1), so the columns every row
 * covers are ONE tile of the full height, and only the edge column(s)
 * that alternate are drawn pixel by pixel (the concert's light beams and
 * seams were one texpage + one tile per row, 14% of its frame). */
static void checker_rows(int x, int y, int w, int h, uint8_t color)
{
    int j;
    for (j = 0; j < h; j++) {
        int px = x + (j & 1), last = x + w - 1, yy = y + j, n;
        if (yy < 0 || yy >= SCR_H)
            continue;
        if ((last - px) & 1)
            last--;                         /* last pixel of this row's dots */
        if (px < 0)
            px += (1 - px) & ~1;            /* first on-screen dot (same parity) */
        if (last > SCR_W - 1)
            last -= (last - (SCR_W - 1) + 1) & ~1;
        if (last < px)
            continue;
        n = (last - px) / 2 + 1;
        if (n >= 4) {
            gfx2d_flush();
            tpage_half();
            emit_tile(px, yy, last - px + 1, 1, g_rgb[color], 1);
        } else {
            for (; px <= last; px += 2)
                plat_rect(px, yy, 1, 1, color);
        }
    }
}

void plat_checker(int x, int y, int w, int h, uint8_t color)
{
    int p, px[2], last[2], j0, j1, bx0, bx1, j;
    uint32_t rgb = g_rgb[color];
    if (h < 2 || w < 8) {
        checker_rows(x, y, w, h, color);
        return;
    }
    j0 = y < 0 ? -y : 0;
    j1 = y + h > SCR_H ? SCR_H - y : h;
    if (j0 >= j1)
        return;
    /* the two row kinds' dot extents, clipped as checker_rows() does */
    for (p = 0; p < 2; p++) {
        px[p] = x + p;
        last[p] = x + w - 1;
        if ((last[p] - px[p]) & 1)
            last[p]--;
        if (px[p] < 0)
            px[p] += (1 - px[p]) & ~1;
        if (last[p] > SCR_W - 1)
            last[p] -= (last[p] - (SCR_W - 1) + 1) & ~1;
        if (last[p] < px[p] || (last[p] - px[p]) / 2 + 1 < 4) {
            checker_rows(x, y, w, h, color);       /* dots: row by row */
            return;
        }
    }
    bx0 = px[0] > px[1] ? px[0] : px[1];
    bx1 = last[0] < last[1] ? last[0] : last[1];
    gfx2d_flush();
    tpage_half();
    if (bx1 >= bx0)
        emit_tile(bx0, y + j0, bx1 - bx0 + 1, j1 - j0, rgb, 1);
    /* the alternating edge columns, blended like the rest of their row */
    for (p = 0; p < 2; p++) {
        int q = p ^ 1;
        if (px[p] < px[q])                     /* the left edge: kind p only */
            for (j = j0 + ((j0 & 1) != p); j < j1; j += 2)
                emit_tile(px[p], y + j, 1, 1, rgb, 1);
        if (last[p] > last[q])                 /* the right edge             */
            for (j = j0 + ((j0 & 1) != p); j < j1; j += 2)
                emit_tile(last[p], y + j, 1, 1, rgb, 1);
    }
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
            setUV0(p, FONT_U(g), FONT_V(g));
            setWH(p, 5, 8);
            p->clut = clut;
            gfx_chain(p);
        }
        x += FONT_ADVANCE;
    }
}

/* platform.h: the outline as one sprite per glyph from the dilated glyph
 * page (7x11, origin (-1, -1)), then the text (was ten plat_text passes
 * = ten sprites per glyph). */
void plat_text_outline(int x, int y, const char *s, uint8_t color, uint8_t outline)
{
    int x0 = x, ox = x, oy = y;
    uint16_t clut = vram_font_clut(outline);
    const char *p;
    int started = 0;
    if (!s)
        return;
    for (p = s; *p; p++) {
        unsigned c = (uint8_t)*p;
        if (c == '\n') {
            x = x0;
            y += FONT_LINE;
            continue;
        }
        if (c > ' ' && c <= FONT_LAST && x > -7 && x < SCR_W + 1 && y > -10 && y < SCR_H + 1) {
            unsigned g = c - FONT_FIRST;
            int extra = g >= FONT_GRID_GLYPHS;   /* Spanish letters: dilated cell in the font tpage */
            SPRT *q;
            if (started != 1 + extra) {
                gfx_tpage(extra ? vram_font_tpage() : vram_fontd_tpage());
                started = 1 + extra;
            }
            q = (SPRT *)gfx_alloc(sizeof(SPRT));
            if (!q)
                return;
            setSprt(q);
            setShadeTex(q, 1);
            setXY0(q, x - 1, y - 1);
            if (extra)
                setUV0(q, FONTD_X_U(g), FONTD_X_V(g));
            else
                setUV0(q, (g & 15) * 8, VR_FONTD_Y - 256 + (g >> 4) * VR_FONTD_CELL_H);
            setWH(q, 7, 11);
            q->clut = clut;
            gfx_chain(q);
        }
        x += FONT_ADVANCE;
    }
    plat_text(ox, oy, s, color);
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
        g_tpage_cur = r->tpage;
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
        g_tpage_cur = r->tpage;         /* a textured poly sets the GPU's texpage too */
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
