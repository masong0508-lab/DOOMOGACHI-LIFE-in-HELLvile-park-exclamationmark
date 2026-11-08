/* blit2.c - plat_ui_loops (plat_ext.h, SP6 / G-K1) and the scale-2 image loop of the IWRAM UI group
 * (owner: gba; docs/GBA.md "SP6: the concert").
 *
 * The concert draws its front row of fans and the band at scale 2 (32x48 for a 16x24 chibi): six to ten
 * images a frame. vid_blit_c (video.iwram.c), the general scaled / flipped path, builds a line of the
 * scaled row and then merges it into the page pair by pair: about 2.6 ms for one such chibi, 10-20 ms of
 * the concert's frame. Here a scale-2 image without a flip is drawn straight from its source rows: a source
 * pixel is two pixels on two rows, so on an even x it is ONE halfword stored on both rows (never a read of
 * the page); on an odd x every halfword holds two neighbouring source pixels (the odd-source trick of the
 * scale-1 ARM blit), stored whole when both are opaque and merged with the page only when one of them is
 * transparent. Same pixels as vid_blit_c, every case: index 0 is transparent, IMG_SILHOUETTE paints every
 * opaque pixel `color`, clipped to the screen on all four sides.
 *
 * The loop is ARM code of the IWRAM overlay's UI group (IWRAM_UI_CODE, iwram_ovl.h; the same group as SP4's
 * dots.c). plat_ui_loops() loads the group for a scene that asks for it (the concert, at the start of its
 * draw); plat_image (video.c) takes vid_blit2 only while the group is in, so a scene that never asks, a 3D
 * view or a tile frame that loads its own group later in the frame, all keep the old path.
 */
#include "gba_plat.h"
#include "iwram_ovl.h"
#include "doomo/plat_ext.h"
#include "video_int.h"

#define BLIT2_ARM IWRAM_UI_CODE __attribute__((optimize("O2", "no-tree-loop-distribute-patterns")))

#define LINE (SCREEN_W / 2)

/* One source row at scale 2 onto the halfwords q0 .. q1 - 1 of dest row r0 and, when two, of r1 = r0 + LINE.
 * even: x is even and source pixel j lands in halfword xh + j (both of its bytes); odd: halfword q holds source
 * pixels j = q - xh - 1 (low byte) and j + 1 (high byte), with xh = (x - 1) / 2 rounded down and the pixels -1 and
 * w empty. A halfword whose two bytes are opaque is stored without reading the page; one transparent byte makes it
 * a read-modify-write; two make it a skip. The common case (no silhouette, both rows) has its own loops. */
static inline __attribute__((always_inline)) void put2(volatile uint16_t *a, volatile uint16_t *b, int two,
                                                       uint32_t lo, uint32_t hi)
{
    if (lo && hi) {
        uint32_t v = lo | hi << 8;
        *a = (uint16_t)v;
        if (two) *b = (uint16_t)v;
    } else if (lo) {
        *a = (uint16_t)((*a & 0xFF00u) | lo);
        if (two) *b = (uint16_t)((*b & 0xFF00u) | lo);
    } else if (hi) {
        *a = (uint16_t)((*a & 0x00FFu) | hi << 8);
        if (two) *b = (uint16_t)((*b & 0x00FFu) | hi << 8);
    }
}

BLIT2_ARM void vid_blit2_row(volatile uint16_t *r0, int two, const uint8_t *s, int w, int xh, int q0, int q1,
                             int odd, uint32_t sil)
{
    volatile uint16_t *a = r0 + q0, *b = r0 + LINE + q0;
    int n = q1 - q0;
    if (!odd) {
        const uint8_t *src = s + (q0 - xh);
        if (!sil && two) {
            for (; n > 0; n--, a++, b++) {
                uint32_t v = *src++;
                if (v) { v |= v << 8; *a = (uint16_t)v; *b = (uint16_t)v; }
            }
            return;
        }
        for (; n > 0; n--, a++, b++) {
            uint32_t v = *src++;
            if (!v) continue;
            if (sil) v = sil;
            v |= v << 8;
            *a = (uint16_t)v;
            if (two) *b = (uint16_t)v;
        }
        return;
    }
    {
        int j = q0 - xh - 1;                         /* source pixel in the low byte of the first halfword */
        uint32_t lo = j >= 0 && j < w ? s[j] : 0u;
        for (; n > 0; n--, a++, b++, j++) {
            uint32_t hi = j + 1 < w ? s[j + 1] : 0u, l = lo, h = hi;
            lo = hi;
            if (!(l | h)) continue;
            if (sil) { if (l) l = sil; if (h) h = sil; }
            if (l && h && two) { uint32_t v = l | h << 8; *a = (uint16_t)v; *b = (uint16_t)v; continue; }
            put2(a, b, two, l, h);
        }
    }
}

/* Draw img at scale 2, no flip, at (x, y) into the back page; sil != 0: every opaque pixel is that colour.
 * All in the group (ARM): the clipping, the row walk and the rows. */
BLIT2_ARM void vid_blit2_arm(int x, int y, const DoomoImage *img, uint32_t sil)
{
    const int w = img->w, h = img->h;
    int x0 = x, x1 = x + 2 * w, sy, odd = x & 1, xh, q0, q1;
    if (x0 < 0) x0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (x0 >= x1 || y >= SCREEN_H || y + 2 * h <= 0) return;
    xh = odd ? (x - 1) >> 1 : x >> 1;               /* arithmetic shift: rounded down */
    q0 = x0 >> 1;
    q1 = (x1 + 1) >> 1;
    for (sy = 0; sy < h; sy++) {
        int ry = y + 2 * sy, two = 1;
        if (ry + 1 < 0) continue;
        if (ry >= SCREEN_H) break;
        if (ry < 0) { ry = 0; two = 0; }            /* only the second row is on screen */
        else if (ry + 1 >= SCREEN_H) two = 0;      /* only the first */
        vid_blit2_row((volatile uint16_t *)gba_back + ry * LINE, two, img->pix + sy * w, w, xh, q0, q1, odd, sil);
    }
}

void vid_blit2(int x, int y, const DoomoImage *img, uint32_t sil)
{
    vid_blit2_arm(x, y, img, sil);
}

/* (S2-5) Any other scale (2..4, flipped or not) in the group: vid_blit_c's method and pixels (video.iwram.c), in
 * ARM -O2. A source row is expanded ONCE into a pair-aligned line (scale columns a pixel, flipped, silhouette colour
 * applied, 0 outside the clip), and that line is stored on each of the `scale` rows it covers. When the line is built
 * its words (two VRAM halfwords each) are sorted once into runs of four opaque pixels, which the rows store as plain
 * word copies with no read of the page, and the other non-empty words, which are merged halfword by halfword exactly
 * as vid_blit_c does (an opaque pair stored, a half-transparent one read-modify-written); empty words are skipped.
 * The cutscenes' scale-3 demons and eggs, the weapons wing's scale-3 weapon plans. */
static inline __attribute__((always_inline)) void put_pair(volatile uint16_t *p, uint32_t v)
{
    if (!v) return;
    if ((v & 0xFFu) && (v >> 8)) *p = (uint16_t)v;
    else if (v & 0xFFu) *p = (uint16_t)((*p & 0xFF00u) | v);
    else *p = (uint16_t)((*p & 0x00FFu) | (v & 0xFF00u));
}

#define FULL4(v) (!(((v) - 0x01010101u) & ~(v) & 0x80808080u))     /* no zero byte: four opaque pixels */

BLIT2_ARM void vid_blitn_arm(int x, int y, const DoomoImage *img, int scale, int flags, uint32_t color)
{
    const int iw = img->w, w = iw * scale, h = img->h * scale;
    const int flip = flags & IMG_FLIPX, sil = flags & IMG_SILHOUETTE;
    /* v / scale, as vid_blit_c's 65536 / scale + 1 (no division: the group calls nothing outside itself) */
    const uint32_t inv = scale == 2 ? 32769u : scale == 3 ? 21846u : 16385u;
    int x0 = x, x1 = x + w, y0 = y, y1 = y + h, yy, xa, xb, n, last = -1, lead, mw, nops = 0;
    uint32_t line32[(SCREEN_W + 8) / 4 + 1];
    uint8_t op_s[(SCREEN_W + 8) / 4], op_n[(SCREEN_W + 8) / 4];   /* a run of op_n full words at op_s, or 0: one mixed */
    uint8_t *line;
    const uint16_t *l;
    const uint32_t *lw;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x0 >= x1 || y0 >= y1) return;
    xa = x0 & ~1;
    xb = (x1 + 1) & ~1;
    n = (xb - xa) >> 1;
    lead = (xa >> 1) & 1;                       /* a first halfword on its own, so the words land on word addresses */
    line = (uint8_t *)line32 + (lead ? 2 : 0);  /* ... in the line too: halfword `lead` of the line is word aligned */
    l = (const uint16_t *)(void *)line;
    lw = (const uint32_t *)(const void *)(l + lead);
    mw = (n - lead) >> 1;                       /* whole words after the lead halfword; then maybe one tail halfword */
    line[0] = 0;                                /* the clipped half of an edge pair stays transparent */
    line[xb - xa - 1] = 0;
    for (yy = y0; yy < y1; yy++) {
        volatile uint16_t *p = (volatile uint16_t *)gba_back + yy * LINE + (xa >> 1);
        volatile uint32_t *pw = (volatile uint32_t *)(void *)(p + lead);
        int sy = (int)(((uint32_t)(yy - y) * inv) >> 16), i;
        if (sy != last) {
            const uint8_t *src = img->pix + sy * iw;
            int d = x0 - x, j = (int)(((uint32_t)d * inv) >> 16);
            int ph = d - j * scale, cnt = x1 - x0, step = 1, run;
            uint8_t *o = line + (x0 - xa);
            uint32_t v;
            if (flip) { j = iw - 1 - j; step = -1; }
            last = sy;
            /* the first source pixel (maybe cut by the clip), the whole ones, the last (maybe cut) */
            run = scale - ph;
            if (run > cnt) run = cnt;
            v = src[j];
            if (sil && v) v = color;
            j += step;
            cnt -= run;
            do *o++ = (uint8_t)v; while (--run);
            if (scale == 3) {
                for (; cnt >= 3; cnt -= 3, j += step, o += 3) {
                    v = src[j];
                    if (sil && v) v = color;
                    o[0] = (uint8_t)v; o[1] = (uint8_t)v; o[2] = (uint8_t)v;
                }
            } else if (scale == 2) {
                for (; cnt >= 2; cnt -= 2, j += step, o += 2) {
                    v = src[j];
                    if (sil && v) v = color;
                    o[0] = (uint8_t)v; o[1] = (uint8_t)v;
                }
            } else {
                for (; cnt >= 4; cnt -= 4, j += step, o += 4) {
                    v = src[j];
                    if (sil && v) v = color;
                    o[0] = (uint8_t)v; o[1] = (uint8_t)v; o[2] = (uint8_t)v; o[3] = (uint8_t)v;
                }
            }
            if (cnt > 0) {
                v = src[j];
                if (sil && v) v = color;
                do *o++ = (uint8_t)v; while (--cnt);
            }
            /* the line's words as runs of full words and single mixed ones */
            nops = 0;
            for (i = 0; i < mw;) {
                uint32_t u = lw[i];
                if (!u) { i++; continue; }
                op_s[nops] = (uint8_t)i;
                if (FULL4(u)) {
                    int s0 = i;
                    while (++i < mw && FULL4(lw[i])) {}
                    op_n[nops++] = (uint8_t)(i - s0);
                } else {
                    op_n[nops++] = 0;
                    i++;
                }
            }
        }
        if (lead) put_pair(p, l[0]);
        for (i = 0; i < nops; i++) {
            int s0 = op_s[i], c = op_n[i];
            if (c) {
                const uint32_t *a = lw + s0;
                volatile uint32_t *b = pw + s0;
                for (; c >= 4; c -= 4, a += 4, b += 4) { b[0] = a[0]; b[1] = a[1]; b[2] = a[2]; b[3] = a[3]; }
                for (; c > 0; c--) *b++ = *a++;
            } else {
                uint32_t u = lw[s0];
                put_pair((volatile uint16_t *)(pw + s0), u & 0xFFFFu);
                put_pair((volatile uint16_t *)(pw + s0) + 1, u >> 16);
            }
        }
        if (lead + 2 * mw < n) put_pair(p + n - 1, l[n - 1]);
    }
}

void vid_blitn(int x, int y, const DoomoImage *img, int scale, int flags, uint32_t color)
{
    vid_blitn_arm(x, y, img, scale, flags, color);
}

/* (SP6) plat_checker in the UI group: the same clipping and parity as plat_checker_m4 (video.iwram.c), but a row's
 * pixels (every other one, all in the same byte of their halfwords) are merged a word at a time, two halfwords per
 * read-modify-write: the concert's light beams, the LED wall's sheen and the seats are hundreds of such rows. */
static inline __attribute__((always_inline)) void checker_body(int x, int y, int w, int h, uint32_t color)
{
    int x1 = x + w, y1 = y + h, yy, par;
    if (w <= 0 || h <= 0) return;
    if (!color) color = 1u;                          /* PAL_BLACK, as plat_checker_m4 */
    par = (x + y) & 1;                               /* drawn: ((px + py) & 1) == par */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x >= x1 || y >= y1) return;
    for (yy = y; yy < y1; yy++) {
        int px = x + (((x + yy) & 1) != par), n;
        volatile uint16_t *p;
        uint32_t keep, put;
        if (px >= x1) continue;
        n = (x1 - px + 1) >> 1;                      /* halfwords holding a drawn pixel */
        p = (volatile uint16_t *)gba_back + yy * LINE + (px >> 1);
        if (px & 1) { keep = 0x00FF00FFu; put = color << 8 | color << 24; }
        else        { keep = 0xFF00FF00u; put = color | color << 16; }
        if (((uintptr_t)p & 2) && n) { *p = (uint16_t)((*p & keep) | put); p++; n--; }
        for (; n >= 2; n -= 2, p += 2) {
            volatile uint32_t *q = (volatile uint32_t *)p;
            *q = (*q & keep) | put;
        }
        if (n) *p = (uint16_t)((*p & keep) | put);
    }
}

BLIT2_ARM void vid_checker_ui(int x, int y, int w, int h, uint32_t color)
{
    checker_body(x, y, w, h, color);
}

/* (SP6) plat_beam (plat_ext.h): the concert's light beam, every 3-row band in one ARM call instead of one
 * plat_checker call (and its THUMB set-up) per band: f steps by q (+1 when the remainder r carries over dy), the
 * centre and half width are the core's ax + dx * f / 256 and hw0 + dhw * f / 256, a band starts one pixel later
 * when (x0 + y) is odd, exactly concert_gfx.c's beam() + cspan(). */
BLIT2_ARM void vid_beam_ui(int ax, int ay, int ty, int dx, int hw0, int dhw, int dy, int q, int r, uint32_t color)
{
    int y, f = 0, rem = 0;
    for (y = ay; y < ty; y += 3) {
        int cx = ax + dx * f / 256, hw = hw0 + dhw * f / 256, x0 = cx - hw, x1 = cx + hw + 1;
        if ((x0 + y) & 1) x0++;
        if (x1 > x0) checker_body(x0, y, x1 - x0, 3, color);
        f += q;
        rem += r;
        if (rem >= dy) { rem -= dy; f++; }
    }
}

int plat_beam(int ax, int ay, int tx, int ty, int hw0, int hw1, uint8_t color)
{
    int dy = ty - ay, q;
    if (gba_nodraw || iwram_ovl_current() != OVL_UI) return 0;   /* the core draws the bands (plat_checker) */
    if (dy <= 0) return 1;
    q = 768 / dy;
    vid_beam_ui(ax, ay, ty, tx - ax, hw0, hw1 - hw0, dy, q, 768 - q * dy, color);
    return 1;
}

int plat_ui_loops(void)
{
    if (gba_nodraw == 1) return 0;                      /* a skipped tick: nothing is drawn */
    /* (S2-5) scenes may ask mid-frame (before a scaled image): never under a tile frame, whose canvas calls
     * (cv_blit, cv_text...) are code of the 2D group and run until the frame ends */
    if (gba_nodraw == GBA_NODRAW_CANVAS) return 0;
    iwram_ovl_use(OVL_UI);
    return 1;
}
