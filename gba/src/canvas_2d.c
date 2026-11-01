/* canvas_2d.c - the overlay canvas's pixel loops (owner: gba; G5, layout
 * and frame kinds in canvas_int.h, docs/GBA.md section 6b).
 *
 * Every function here is ARM code of the IWRAM overlay's 2D group
 * (IWRAM_2D_CODE): plat_overworld loads the group (iwram_ovl_use) before a
 * tile frame, and only a tile frame calls these. The canvas is 8bpp tiles,
 * a tile row = 8 bytes = two words; VRAM ignores byte writes, so pixels
 * go in as word read-modify-writes under a byte mask (gba_nibmask). Each
 * loop draws exactly the pixels the Mode 4 loop of the same primitive
 * draws (video.iwram.c, video.c, ow.iwram.c), at the same screen place:
 * the canvas is screen-aligned, the world below it is BG2.
 */
#include "canvas_int.h"
#include "video_int.h"
#include "ow_int.h"
#include "doomo/palette.h"

#define INL static inline __attribute__((always_inline, target("arm")))
typedef volatile uint32_t vu32_t;

/* the sink for a sparse frame's tiles once the pool is full: the zero tile
 * (that frame is dropped, and the next sparse frame zeroes it again) */
#define cv_sink ((uint32_t *)(CV_VRAM + CV_ZERO_TILE * 64u))

INL void zero_tile(vu32_t *p)
{
    int i;
    for (i = 0; i < 16; i++) p[i] = 0;
}

/* the tile at screen tile (tx, ty), allocated (zeroed) on first touch
 * (one copy for every loop: inlined it took the 2D group past its slot) */
vu32_t *cv_tile(int tx, int ty) CV_ARM;
vu32_t *cv_tile(int tx, int ty)
{
    int t = ty * CV_TW + tx;
    if (cv.mode == CV_DENSE) {
        vu32_t *p = (vu32_t *)(CV_VRAM + CV_DENSE_OFS + (uint32_t)t * 64u);
        uint32_t b = 1u << (t & 31);
        if (!(cv.used[t >> 5] & b)) {
            cv.used[t >> 5] |= b;
            cv.used_n++;
            zero_tile(p);
        }
        return p;
    } else {
        uint16_t *e = &cv_meta->cmap[ty * 32 + tx];
        uint32_t v = *e;
        if (v == CV_ZERO_TILE) {
            if (cv.pool_next >= CV_POOL_N) {
                cv.overflow = 1;
                return (vu32_t *)cv_sink;
            }
            v = CV_ZERO_TILE + cv.pool_next++;
            *e = (uint16_t)v;
            zero_tile((vu32_t *)(CV_VRAM + v * 64u));
        }
        return (vu32_t *)(CV_VRAM + v * 64u);
    }
}
/* the tile of a dense frame that is already in use: no call */
INL vu32_t *tile_at(int tx, int ty)
{
    const int t = ty * CV_TW + tx;
    if (cv.mode == CV_DENSE && (cv.used[t >> 5] & (1u << (t & 31))))
        return (vu32_t *)(CV_VRAM + CV_DENSE_OFS + (uint32_t)t * 64u);
    return cv_tile(tx, ty);
}

/* the world pixel (BG2) at screen (x, y): composite tile of the shadow map */
INL uint32_t world_px(int x, int y)
{
    int wx = x + (cv.sx & 7), wy = y + (cv.sy & 7);
    uint32_t e = cv_meta->wmap[(wy >> 3) * CV_WMAP_W + (wx >> 3)];
    return *(const vu8 *)(CV_VRAM + e * 64u + (uint32_t)(((wy & 7) << 3) | (wx & 7)));
}

/* 8 pixels of a tile row under a byte-column mask (bit c = column c) */
INL void put8(vu32_t *row, uint32_t lo, uint32_t hi, uint32_t bits)
{
    uint32_t m;
    m = gba_nibmask[bits & 15];
    if (m == 0xFFFFFFFFu) row[0] = lo;
    else if (m) row[0] = (row[0] & ~m) | (lo & m);
    m = gba_nibmask[(bits >> 4) & 15];
    if (m == 0xFFFFFFFFu) row[1] = hi;
    else if (m) row[1] = (row[1] & ~m) | (hi & m);
}

/* columns [a, b) of a tile as a bit mask */
INL uint32_t colbits(int a, int b) { return (0xFFu >> (8 - (b - a))) << a; }

/* ---- fills ----------------------------------------------------------------- */
void cv_fill(int x0, int y0, int x1, int y1, uint32_t color)
{
    const uint32_t c4 = (color & 0xFF) * 0x01010101u;
    int ty, tx;
    for (ty = y0 >> 3; ty <= (y1 - 1) >> 3; ty++) {
        int r0 = (y0 > ty * 8 ? y0 : ty * 8) - ty * 8;
        int r1 = (y1 < ty * 8 + 8 ? y1 : ty * 8 + 8) - ty * 8;
        for (tx = x0 >> 3; tx <= (x1 - 1) >> 3; tx++) {
            int a = (x0 > tx * 8 ? x0 : tx * 8) - tx * 8;
            int b = (x1 < tx * 8 + 8 ? x1 : tx * 8 + 8) - tx * 8;
            uint32_t bits = colbits(a, b);
            vu32_t *t = tile_at(tx, ty) + r0 * 2;
            int r;
            if (bits == 0xFF)
                for (r = r0; r < r1; r++, t += 2) { t[0] = c4; t[1] = c4; }
            else
                for (r = r0; r < r1; r++, t += 2) put8(t, c4, c4, bits);
        }
    }
}

/* the 8 world pixels (BG2) of screen x .. x+7 (x a multiple of 8) on row
 * y, as two words: the map's 31 visible tiles start (sx & 7) pixels to
 * the left of the screen, so the 8 come from at most two world tiles */
INL void world8(int x, int y, uint32_t *w)
{
    const int wy = y + (cv.sy & 7), o = cv.sx & 6, i = (x >> 3);
    const uint16_t *m = &cv_meta->wmap[(wy >> 3) * CV_WMAP_W + i];
    const uint32_t ro = (uint32_t)(wy & 7) * 8u;
    const uint32_t *a = (const uint32_t *)(CV_VRAM + (uint32_t)m[0] * 64u + ro);
    if (!o) { w[0] = a[0]; w[1] = a[1]; return; }
    {
        const uint32_t *b = (const uint32_t *)(CV_VRAM + (uint32_t)m[1] * 64u + ro);
        /* bytes o..7 of tile a, then 0..o-1 of tile b (o = 2, 4, 6) */
        uint32_t a0 = a[0], a1 = a[1], b0 = b[0], b1 = b[1];
        if (o == 4) { w[0] = a1; w[1] = b0; }
        else if (o == 2) { w[0] = (a0 >> 16) | (a1 << 16); w[1] = (a1 >> 16) | (b0 << 16); }
        else { w[0] = (a1 >> 16) | (b0 << 16); w[1] = (b0 >> 16) | (b1 << 16); }
    }
}

/* plat_shade_rect / ow_shade: every pixel of the rect through cm; where the
 * canvas is still transparent, the world pixel under it */
INL void shade8(vu32_t *t, int x, int y, uint32_t bits, const uint8_t *cm)
{
    uint32_t cw[2], ww[2], out[2];
    const uint8_t *c = (const uint8_t *)cw, *wp = (const uint8_t *)ww;
    uint8_t *o = (uint8_t *)out;
    int k;
    cw[0] = t[0];
    cw[1] = t[1];
    world8(x, y, ww);
    for (k = 0; k < 8; k++) {
        uint32_t v = c[k];
        if (!v) v = wp[k];
        o[k] = cm[v];
    }
    put8(t, out[0], out[1], bits);
}

void cv_shade(int x0, int y0, int x1, int y1, const uint8_t *cm)
{
    int ty, tx;
    for (ty = y0 >> 3; ty <= (y1 - 1) >> 3; ty++) {
        int r0 = (y0 > ty * 8 ? y0 : ty * 8) - ty * 8;
        int r1 = (y1 < ty * 8 + 8 ? y1 : ty * 8 + 8) - ty * 8;
        for (tx = x0 >> 3; tx <= (x1 - 1) >> 3; tx++) {
            int a = (x0 > tx * 8 ? x0 : tx * 8) - tx * 8;
            int b = (x1 < tx * 8 + 8 ? x1 : tx * 8 + 8) - tx * 8;
            uint32_t bits = colbits(a, b);
            vu32_t *t = tile_at(tx, ty) + r0 * 2;
            int r;
            for (r = r0; r < r1; r++, t += 2) shade8(t, tx * 8, ty * 8 + r, bits, cm);
        }
    }
}

/* the bits of the 8 columns at screen x (a multiple of 8) of screen row y
 * that no raised cell of map rows > row covers (the frame's lift grid) */
INL uint32_t occ_free(int x, int y, int row)
{
    const CvMeta *M = cv_meta;
    uint32_t bits = 0xFF;
    const int wy = y + cv.sy, cyr = wy >> 4, j = cyr - M->gy0;
    int cx = (x + cv.sx) >> 4, cx1 = (x + 7 + cv.sx) >> 4;
    for (; cx <= cx1; cx++) {
        int i = cx - M->gx0, cover = 0, a, b;
        if ((unsigned)i >= M->gw) continue;
        if (cyr > row && (unsigned)j < M->gh && M->lift[j][i]) cover = 1;
        else if (cyr + 1 > row && (unsigned)(j + 1) < M->gh) {
            int l = M->lift[j + 1][i];
            cover = l && wy >= (cyr + 1) * 16 - l;
        }
        if (!cover) continue;
        a = cx * 16 - cv.sx - x;
        b = a + 16;
        if (a < 0) a = 0;
        if (b > 8) b = 8;
        if (a < b) bits &= ~colbits(a, b);
    }
    return bits;
}

/* 1 if a raised cell of map rows > row covers any of [x0,x1) x [y0,y1) */
INL int occ_any(int x0, int y0, int x1, int y1, int row)
{
    const CvMeta *M = cv_meta;
    int cy, cx;
    if (row >= CV_ROW_NONE) return 0;
    for (cy = (y0 + cv.sy) >> 4; cy <= ((y1 - 1 + cv.sy) >> 4) + 1; cy++) {
        int j = cy - M->gy0;
        if (cy <= row || (unsigned)j >= M->gh) continue;
        for (cx = (x0 + cv.sx) >> 4; cx <= (x1 - 1 + cv.sx) >> 4; cx++) {
            int i = cx - M->gx0, l;
            if ((unsigned)i >= M->gw || !(l = M->lift[j][i])) continue;
            if (cy * 16 - cv.sy - l < y1) return 1;
        }
    }
    return 0;
}

/* The occluders of a box as one bit mask per screen row: bit i = cell
 * column cxa + i (screen x [cxa * 16 - sx, +16)) is covered at that row.
 * keep8() turns a row's mask into the 8 columns of tile column tx. */
INL void occ_rows(int y0, int y1, int cxa, int cxb, int row, uint32_t *rm)
{
    const CvMeta *M = cv_meta;
    const int gx0 = M->gx0, gy0 = M->gy0, gw = M->gw, gh = M->gh;
    int y;
    for (y = y0; y < y1; y++) {
        const int wy = y + cv.sy, cyr = wy >> 4, j = cyr - gy0;
        uint32_t mk = 0;
        int cx;
        for (cx = cxa; cx <= cxb; cx++) {
            int i = cx - gx0, l;
            if ((unsigned)i >= (unsigned)gw) continue;
            if (cyr > row && (unsigned)j < (unsigned)gh && M->lift[j][i]) mk |= 1u << (cx - cxa);
            else if (cyr + 1 > row && (unsigned)(j + 1) < (unsigned)gh && (l = M->lift[j + 1][i]) &&
                     wy >= (cyr + 1) * 16 - l)
                mk |= 1u << (cx - cxa);
        }
        rm[y - y0] = mk;
    }
}

INL uint32_t keep8(uint32_t mk, int tx, int cxa)
{
    /* the 8 screen columns tx*8.. start o columns into cell column c0;
     * the first n0 of them are in it, the rest in c0 + 1 */
    const int wx = tx * 8 + cv.sx, c0 = (wx >> 4) - cxa, o = wx & 15;
    const int n0 = o > 8 ? 16 - o : 8;
    uint32_t clear = 0;
    if (!mk) return 0xFF;
    if (c0 >= 0 && c0 < 32 && ((mk >> c0) & 1)) clear |= (1u << n0) - 1u;
    if (n0 < 8 && c0 + 1 >= 0 && c0 + 1 < 32 && ((mk >> (c0 + 1)) & 1)) clear |= (0xFFu << n0) & 0xFFu;
    return ~clear & 0xFFu;
}

void cv_shade_occ(int x0, int x1, int y, const uint8_t *cm, int row)
{
    int tx, occ = occ_any(x0, y, x1, y + 1, row);
    for (tx = x0 >> 3; tx <= (x1 - 1) >> 3; tx++) {
        int a = (x0 > tx * 8 ? x0 : tx * 8) - tx * 8;
        int b = (x1 < tx * 8 + 8 ? x1 : tx * 8 + 8) - tx * 8;
        uint32_t bits = colbits(a, b);
        if (occ) bits &= occ_free(tx * 8, y, row);
        if (bits) shade8(tile_at(tx, y >> 3) + (y & 7) * 2, tx * 8, y, bits, cm);
    }
}

/* plat_checker: pixel (x, y) where ((x + y) & 1) == par */
void cv_checker(int x0, int y0, int x1, int y1, int par, uint32_t color)
{
    const uint32_t c4 = (color & 0xFF) * 0x01010101u;
    int ty, tx;
    for (ty = y0 >> 3; ty <= (y1 - 1) >> 3; ty++) {
        int r0 = (y0 > ty * 8 ? y0 : ty * 8) - ty * 8;
        int r1 = (y1 < ty * 8 + 8 ? y1 : ty * 8 + 8) - ty * 8;
        for (tx = x0 >> 3; tx <= (x1 - 1) >> 3; tx++) {
            int a = (x0 > tx * 8 ? x0 : tx * 8) - tx * 8;
            int b = (x1 < tx * 8 + 8 ? x1 : tx * 8 + 8) - tx * 8;
            uint32_t bits = colbits(a, b);
            vu32_t *t = tile_at(tx, ty) + r0 * 2;
            int r;
            for (r = r0; r < r1; r++, t += 2) {
                /* column c drawn when (c + y) & 1 == par (tile x is even) */
                uint32_t odd = (uint32_t)((par ^ (ty * 8 + r)) & 1);
                put8(t, c4, c4, bits & (odd ? 0xAAu : 0x55u));
            }
        }
    }
}

void cv_pixel(int x, int y, uint32_t color)
{
    const uint32_t c4 = (color & 0xFF) * 0x01010101u;
    put8(tile_at(x >> 3, y >> 3) + (y & 7) * 2, c4, c4, 1u << (x & 7));
}

/* text: glyph rows (bit c = column c, up to 16 columns) at (x, y), clipped
 * to the screen per pixel like vid_text / vid_text_dil */
void cv_glyph(const uint16_t *g, int rows, int x, int y, uint32_t color)
{
    const uint32_t c4 = (color & 0xFF) * 0x01010101u;
    const int bx = x >> 3, off = x - bx * 8;
    vu32_t *tc[3] = { 0, 0, 0 };
    int r, cty = -1;
    for (r = 0; r < rows; r++) {
        int py = y + r, k;
        uint32_t w;
        if ((unsigned)py >= SCREEN_H || !g[r]) continue;
        if ((py >> 3) != cty) { cty = py >> 3; tc[0] = tc[1] = tc[2] = 0; }
        w = (uint32_t)g[r] << off;
        for (k = 0; k < 3; k++) {
            int tx = bx + k;
            uint32_t bits = (w >> (8 * k)) & 0xFF;
            if (!bits || (unsigned)tx >= CV_TW) continue;
            if (!tc[k]) tc[k] = tile_at(tx, cty);
            put8(tc[k] + (py & 7) * 2, c4, c4, bits);
        }
    }
}

/* text into the canvas: vid_text / vid_text_dil's glyph acceptance, then
 * cv_glyph (it clips per pixel like they do) */
void cv_text(int x, int y, const char *s, uint32_t color, int dil)
{
    int cx = x, cy = y;
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        uint16_t g[11];
        if (ch == '\n') { cx = x; cy += FONT_LINE; continue; }
        if (ch >= FONT_FIRST && ch <= FONT_LAST && ch != ' ') {
            const uint8_t *rows = vid_glyph_rows(ch);
            int r;
            if (dil) {
                if (cx > -FONT_ADVANCE - 1 && cx < SCREEN_W + 1 && cy > -10 && cy < SCREEN_H + 1) {
                    vid_glyph_dilate(rows, g);
                    cv_glyph(g, 11, cx - 1, cy - 1, color);
                }
            } else if (cx > -FONT_ADVANCE && cx < SCREEN_W && cy > -8 && cy < SCREEN_H) {
                for (r = 0; r < 8; r++) g[r] = rows[r];
                cv_glyph(g, 8, cx, cy, color);
            }
        }
        cx += FONT_ADVANCE;
    }
}

/* plat_rect in a tile frame, straight from video_loops.s (gba_nodraw ==
 * GBA_NODRAW_CANVAS): clip, then the fill; a big rect goes to the slow
 * path (canvas.c), which leaves the canvas first */
void plat_rect_slow(int x, int y, int w, int h, uint8_t color) __attribute__((long_call));
void cv_rect_fast(int x, int y, int w, int h, uint8_t color) CV_ARM;
void cv_rect_fast(int x, int y, int w, int h, uint8_t color)
{
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x >= x1 || y >= y1) return;
    if ((x1 - x) * (y1 - y) >= SCREEN_W * SCREEN_H / 2) { plat_rect_slow(x, y, x1 - x, y1 - y, color); return; }
    cv_fill(x, y, x1, y1, color ? color : PAL_BLACK);
}

/* plat_image (vid_blit): 8bpp, 0 transparent, integer scale, flip,
 * silhouette; clipped to the screen; tile by tile */
void cv_blit(int x, int y, const DoomoImage *img, int scale, int flags, uint32_t color)
{
    const int w = img->w * scale, h = img->h * scale;
    const int flip = flags & IMG_FLIPX, sil = flags & IMG_SILHOUETTE;
    const int direct = scale == 1 && !flip;
    const uint32_t inv = 65536u / (uint32_t)scale + 1u;
    int x0 = x, x1 = x + w, y0 = y, y1 = y + h, tx, ty;
    uint16_t lut[SCREEN_W];
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x0 >= x1 || y0 >= y1) return;
    if (!direct) {
        int px;
        for (px = x0; px < x1; px++) {
            int t = (int)(((uint32_t)(px - x) * inv) >> 16);
            if (t >= img->w) t = img->w - 1;
            if (flip) t = img->w - 1 - t;
            lut[px - x0] = (uint16_t)t;
        }
    }
    for (ty = y0 >> 3; ty <= (y1 - 1) >> 3; ty++) {
        const int ya = y0 > ty * 8 ? y0 : ty * 8, yb = y1 < ty * 8 + 8 ? y1 : ty * 8 + 8;
        for (tx = x0 >> 3; tx <= (x1 - 1) >> 3; tx++) {
            const int a = x0 > tx * 8 ? x0 : tx * 8, b = x1 < tx * 8 + 8 ? x1 : tx * 8 + 8;
            vu32_t *t = 0;
            int yy;
            for (yy = ya; yy < yb; yy++) {
                const uint8_t *src = img->pix +
                    (scale == 1 ? (uint32_t)(yy - y) : (((uint32_t)(yy - y) * inv) >> 16)) * img->w;
                uint32_t px8[2] = { 0, 0 }, bits = 0;
                uint8_t *o = (uint8_t *)px8;
                int px;
                for (px = a; px < b; px++) {
                    uint32_t v = direct ? src[px - x] : src[lut[px - x0]], c = (uint32_t)px & 7;
                    if (!v) continue;
                    o[c] = (uint8_t)(sil ? color : v);
                    bits |= 1u << c;
                }
                if (!bits) continue;
                if (!t) t = tile_at(tx, ty);
                put8(t + (yy & 7) * 2, px8[0], px8[1], bits);
            }
        }
    }
}

/* ow_sprite's pixels (ow.iwram.c): texels lit through cm, then the night /
 * view tint dither at the screen pixel, translucency as a checker; minus
 * the occluders (the raised cells drawn after the sprite in Mode 4) */
INL uint32_t post(uint32_t c, int x, int y)
{
    int b = ow_bayer[((y & 3) << 2) | (x & 3)];
    if (b < owf.night) c = owf.night_map[c];
    if (b < owf.tint_level) c = owf.tint_map[c];
    return c;
}

void cv_sprite(const DoomoImage *img, int x0, int y0, int s, int flags, const uint8_t *cm,
               int row)
{
    const int w = img->w * s, h = img->h * s;
    int cx0 = x0, cx1 = x0 + w, cy0 = y0, cy1 = y0 + h;
    const int post_on = owf.night || owf.tint_level;
    const int trans = flags & BB_TRANSLUCENT;
    const int flip = flags & BB_FLIPX;
    const int direct = s == 1 && !flip;
    const uint32_t inv = 65536u / (uint32_t)s + 1u;
    const uint8_t *nmap = owf.night_map, *tmap = owf.tint_map;
    uint8_t lut[256];
    uint32_t nmask[4], tmask[4], rm[SCREEN_H];
    int x, occ, tx, ty, cxa = 0;
    if (cx0 < owf.ox) cx0 = owf.ox;
    if (cx1 > owf.ox + owf.ow) cx1 = owf.ox + owf.ow;
    if (cy0 < owf.oy) cy0 = owf.oy;
    if (cy1 > owf.oy + owf.oh) cy1 = owf.oy + owf.oh;
    if (cx0 >= cx1 || cy0 >= cy1 || cx1 - cx0 > 256) return;
    occ = occ_any(cx0, cy0, cx1, cy1, row);
    if (occ) {
        cxa = (cx0 + cv.sx) >> 4;
        occ_rows(cy0, cy1, cxa, (cx1 - 1 + cv.sx) >> 4, row, rm);
    }
    if (!direct)
        for (x = cx0; x < cx1; x++) {
            int t = (int)(((uint32_t)(x - x0) * inv) >> 16);
            if (flip) t = img->w - 1 - t;
            lut[x - cx0] = (uint8_t)t;
        }
    /* post(): pixel (x, y) takes the night map where ow_bayer[y&3][x&3] <
     * night, then the tint map where it is < tint_level: per screen row
     * phase, the bits of the 4 column phases that do */
    if (post_on)
        for (ty = 0; ty < 4; ty++) {
            uint32_t n = 0, t = 0;
            for (x = 0; x < 4; x++) {
                int b = ow_bayer[(ty << 2) | x];
                if (b < owf.night) n |= 1u << x;
                if (b < owf.tint_level) t |= 1u << x;
            }
            nmask[ty] = n | (n << 4);
            tmask[ty] = t | (t << 4);
        }
    if (direct && !post_on && !trans && !occ) {
        /* the common case: 1:1 texels through cm, nothing in front */
        for (ty = cy0 >> 3; ty <= (cy1 - 1) >> 3; ty++) {
            const int ya = cy0 > ty * 8 ? cy0 : ty * 8, yb = cy1 < ty * 8 + 8 ? cy1 : ty * 8 + 8;
            for (tx = cx0 >> 3; tx <= (cx1 - 1) >> 3; tx++) {
                const int a = cx0 > tx * 8 ? cx0 : tx * 8, b = cx1 < tx * 8 + 8 ? cx1 : tx * 8 + 8;
                const uint8_t *sr = img->pix + (ya - y0) * img->w + (a - x0);
                vu32_t *t = 0;
                int y;
                for (y = ya; y < yb; y++, sr += img->w) {
                    uint32_t lo = 0, hi = 0, bits = 0, v;
                    if (b - a == 8) {
                        if ((v = sr[0])) { lo = cm[v]; bits = 0x01; }
                        if ((v = sr[1])) { lo |= (uint32_t)cm[v] << 8; bits |= 0x02; }
                        if ((v = sr[2])) { lo |= (uint32_t)cm[v] << 16; bits |= 0x04; }
                        if ((v = sr[3])) { lo |= (uint32_t)cm[v] << 24; bits |= 0x08; }
                        if ((v = sr[4])) { hi = cm[v]; bits |= 0x10; }
                        if ((v = sr[5])) { hi |= (uint32_t)cm[v] << 8; bits |= 0x20; }
                        if ((v = sr[6])) { hi |= (uint32_t)cm[v] << 16; bits |= 0x40; }
                        if ((v = sr[7])) { hi |= (uint32_t)cm[v] << 24; bits |= 0x80; }
                    } else {
                        uint32_t px8[2] = { 0, 0 };
                        uint8_t *o = (uint8_t *)px8;
                        int px;
                        for (px = a; px < b; px++)
                            if ((v = sr[px - a])) { o[px & 7] = cm[v]; bits |= 1u << (px & 7); }
                        lo = px8[0];
                        hi = px8[1];
                    }
                    if (!bits) continue;
                    if (!t) t = tile_at(tx, ty);
                    put8(t + (y & 7) * 2, lo, hi, bits);
                }
            }
        }
        return;
    }
    for (ty = cy0 >> 3; ty <= (cy1 - 1) >> 3; ty++) {
        const int ya = cy0 > ty * 8 ? cy0 : ty * 8, yb = cy1 < ty * 8 + 8 ? cy1 : ty * 8 + 8;
        for (tx = cx0 >> 3; tx <= (cx1 - 1) >> 3; tx++) {
            const int a = cx0 > tx * 8 ? cx0 : tx * 8, b = cx1 < tx * 8 + 8 ? cx1 : tx * 8 + 8;
            vu32_t *t = 0;
            int y;
            for (y = ya; y < yb; y++) {
                uint32_t px8[2] = { 0, 0 }, bits = 0;
                uint32_t keep = occ ? keep8(rm[y - cy0], tx, cxa) : 0xFF;
                uint32_t nm = post_on ? nmask[y & 3] : 0, tm = post_on ? tmask[y & 3] : 0;
                uint8_t *o = (uint8_t *)px8;
                int px;
                if (trans) keep &= (y & 1) ? 0xAAu : 0x55u;   /* (x + y) even; tile x is even */
                if (direct) {
                    const uint8_t *sr = img->pix + (y - y0) * img->w - x0;
                    for (px = a; px < b; px++) {
                        uint32_t v, c = (uint32_t)px & 7;
                        if (!((keep >> c) & 1) || !(v = sr[px])) continue;
                        v = cm[v];
                        if ((nm >> c) & 1) v = nmap[v];
                        if ((tm >> c) & 1) v = tmap[v];
                        o[c] = (uint8_t)v;
                        bits |= 1u << c;
                    }
                } else {
                    const uint8_t *sr = img->pix + (((uint32_t)(y - y0) * inv) >> 16) * img->w;
                    for (px = a; px < b; px++) {
                        uint32_t v, c = (uint32_t)px & 7;
                        if (!((keep >> c) & 1) || !(v = sr[lut[px - cx0]])) continue;
                        v = cm[v];
                        if ((nm >> c) & 1) v = nmap[v];
                        if ((tm >> c) & 1) v = tmap[v];
                        o[c] = (uint8_t)v;
                        bits |= 1u << c;
                    }
                }
                if (!bits) continue;
                if (!t) t = tile_at(tx, ty);
                put8(t + (y & 7) * 2, px8[0], px8[1], bits);
            }
        }
    }
}

/* ---- the world's rows ---------------------------------------------------------- */
/* screen row y of the world: line[0 .. 248) holds map columns 0..30 of the
 * row; the screen's pixel x is line[(sx & 7) + x] */
void cv_world_row(uint8_t *line, int y)
{
    int wy = y + (cv.sy & 7), i;
    const uint16_t *m = &cv_meta->wmap[(wy >> 3) * CV_WMAP_W];
    uint32_t *d = (uint32_t *)line;
    const uint32_t ro = (uint32_t)(wy & 7) * 8u;
    for (i = 0; i < 31; i++, d += 2) {
        const uint32_t *s = (const uint32_t *)(CV_VRAM + (uint32_t)m[i] * 64u + ro);
        d[0] = s[0];
        d[1] = s[1];
    }
}

/* Dense frame -> Mode 4 page 1, in place. Screen tile row r's 30 tiles sit
 * at page 1 + 1920 r, exactly the bytes of screen rows 8r..8r+7: each
 * band is copied out, then its 8 rows are written as world + canvas. */
void cv_convert(void)
{
    uint32_t band[1920 / 4];
    uint32_t line[256 / 4];
    const int o = cv.sx & 7;
    int r, k, tx;
    for (r = 0; r < CV_TH; r++) {
        uint32_t used_any = 0;
        uint8_t *page = (uint8_t *)(CV_VRAM + CV_DENSE_OFS + (uint32_t)r * 1920u);
        for (tx = 0; tx < CV_TW; tx++) {
            int t = r * CV_TW + tx;
            used_any |= cv.used[t >> 5] & (1u << (t & 31));
        }
        if (used_any) dma3_copy32(band, page, 1920 / 4);
        for (k = 0; k < 8; k++) {
            uint8_t *l = (uint8_t *)line + o;
            cv_world_row((uint8_t *)line, r * 8 + k);
            if (used_any)
                for (tx = 0; tx < CV_TW; tx++) {
                    int t = r * CV_TW + tx, c;
                    const uint8_t *s;
                    if (!(cv.used[t >> 5] & (1u << (t & 31)))) continue;
                    s = (const uint8_t *)band + tx * 64 + k * 8;
                    for (c = 0; c < 8; c++)
                        if (s[c]) l[tx * 8 + c] = s[c];
                }
            /* halfword aligned (o is even): one DMA per row */
            dma3_copy16(page + k * SCREEN_W, l, SCREEN_W / 2);
        }
    }
}

/* Dense frame -> the pool: every used tile gets a pool tile, identical
 * tiles share one (solid panel interiors, a dialogue box) and empty ones
 * use the zero tile; the canvas map shadow points at them and the runs
 * list what the vblank copies. Returns the pool tiles used. */
uint32_t cv_plan_dedup(void)
{
    CvMeta *M = cv_meta;
    uint16_t *m = M->cmap;
    uint32_t p = 1, nruns = 0, *u = cv.used;
    int t, i, ty, tx;
    for (i = 0; i < CV_TH * 32 / 2; i++) ((uint32_t *)m)[i] = CV_ZERO_TILE * 0x10001u;
    for (i = 0; i < 256; i++) M->dedup[i] = 0;
    for (ty = 0, t = 0; ty < CV_TH; ty++)
        for (tx = 0; tx < CV_TW; tx++, t++) {
            const uint32_t *tp;
            uint32_t h = 0, any = 0, e;
            if (!(u[t >> 5] & (1u << (t & 31)))) continue;
            tp = (const uint32_t *)(CV_VRAM + CV_DENSE_OFS + (uint32_t)t * 64u);
            for (i = 0; i < 16; i++) { uint32_t w = tp[i]; any |= w; h = (h * 31u) ^ w; }
            if (!any) continue;                 /* the zero tile */
            h = (h ^ (h >> 16) ^ (h >> 8)) & 255;
            e = M->dedup[h];
            if (e) {
                const uint32_t *sp = (const uint32_t *)(CV_VRAM + CV_DENSE_OFS + (e & 1023u) * 64u);
                for (i = 0; i < 16 && sp[i] == tp[i]; i++) {}
                if (i == 16) { m[ty * 32 + tx] = (uint16_t)(CV_ZERO_TILE + (e >> 10)); continue; }
            }
            if (p >= CV_POOL_N) return p + 1;   /* does not fit */
            M->dedup[h] = (uint32_t)t | (p << 10);
            m[ty * 32 + tx] = (uint16_t)(CV_ZERO_TILE + p);
            if (nruns && M->run_src[nruns - 1] + M->run_n[nruns - 1] == (uint32_t)t &&
                M->run_dst[nruns - 1] + M->run_n[nruns - 1] == p)
                M->run_n[nruns - 1]++;
            else {
                M->run_src[nruns] = (uint16_t)t;
                M->run_dst[nruns] = (uint16_t)p;
                M->run_n[nruns] = 1;
                nruns++;
            }
            p++;
        }
    M->nruns = (uint16_t)nruns;
    return p;
}


uint32_t cv_plan(void)
{
    CvMeta *M = cv_meta;
    uint16_t *m = M->cmap;
    uint32_t p = 1, nruns = 0, *u = cv.used;
    int t, ty, tx;
    if (cv.used_n >= CV_POOL_N) return cv_plan_dedup();
    {
        /* they fit: pool tile = order of first touch... in screen order,
         * one run per stretch of used tiles; no dedupe pass */
        for (ty = 0, t = 0; ty < CV_TH; ty++) {
            uint16_t *mr = m + ty * 32;
            for (tx = 0; tx < 32; tx += 2) ((uint32_t *)mr)[tx >> 1] = CV_ZERO_TILE * 0x10001u;
            for (tx = 0; tx < CV_TW; tx++, t++) {
                if (!(u[t >> 5] & (1u << (t & 31)))) continue;
                mr[tx] = (uint16_t)(CV_ZERO_TILE + p);
                if (nruns && M->run_src[nruns - 1] + M->run_n[nruns - 1] == (uint32_t)t)
                    M->run_n[nruns - 1]++;
                else {
                    M->run_src[nruns] = (uint16_t)t;
                    M->run_dst[nruns] = (uint16_t)p;
                    M->run_n[nruns] = 1;
                    nruns++;
                }
                p++;
            }
        }
        M->nruns = (uint16_t)nruns;
        return p;
    }
}
