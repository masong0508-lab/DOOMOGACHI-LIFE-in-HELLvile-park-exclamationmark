/* stage.c - the kept backdrop and the retained stage on the GBA (SP7; plat_ext.h plat_stage_*,
 * core/src/backdrop.c; owner: gba; docs/EXORCISM_SPEED.md).
 *
 * The backdrops are baked at build time: the native runner draws every key of every family
 * (core/include/doomo/backdrop.h) with the core's own drawing code at 240x160 and the ROM
 * links the file (backdrop_blob.s, build/gen/backdrops.bin: "BKD2", w, h, n, a sorted table of
 * { id << 24 | key, offset, box }, then raw 8bpp pictures; (S2-4b) a picture is a whole screen, a
 * band of screen rows, or a stamp: a small picture with holes that plat_stage_stamp draws anywhere
 * as an image, straight from the ROM). A frame that asks for a baked key gets
 * plat_stage_begin = 1 and restores boxes of the picture with DMA from the ROM: the church the
 * exorcism drew in ~70 ms of rects, lines and circles is a ~3.5 ms copy, and only where a
 * layer changed.
 *
 * Pages: a Mode 4 frame is drawn into the page not shown. Each page remembers the serial of the
 * stage frame it holds (0: none, or something else was drawn into it since) and the box every
 * 2D call covered after that frame's plat_stage_end (dialogue, toasts, hints): from
 * plat_stage_end to the flip gba_nodraw is GBA_NODRAW_TRACK, so every 2D call takes its slow
 * path (vid_route, canvas.c), which widens the box (and still routes panels, G7); a kept panel
 * (plat_panel_begin, panel.c) adds its rect, which it copies without a 2D call.
 * Frames that are not stage frames (another scene, a tile frame, a held or built frame)
 * forget what their page held. RAM: 34 B of EWRAM .bss.
 */
#include "gba_plat.h"
#include "canvas_int.h"
#include "doomo/plat_ext.h"
#include "iwram_ovl.h"
#include "video_int.h"                               /* vid_blit */

extern const uint8_t gba_backdrops[];          /* backdrop_blob.s */

/* a table entry (4-aligned in the ROM): a whole screen or a band has w = SCREEN_W and its rows
 * [by, by + h); a stamp has its box's corner (bx, by) from the anchor */
typedef struct { uint32_t idkey, off; int16_t bx, by; uint16_t w, h; } BkEnt;
static const BkEnt *cur;                       /* the restores' source picture (ROM) */
static uint32_t serial;                        /* Mode 4 frames presented           */
static uint32_t page_serial[2];                /* stage frame each page holds, 0    */
static int16_t page_dirt[2][4];                /* drawn over it after plat_stage_end */
static int16_t trk[4];
static uint8_t frame_stage, tracking;

/* ARM in the room group of the IWRAM overlay (iwram_ovl.h; the group no stage scene uses): the per-row work and
 * the table search run from IWRAM instead of the cartridge */
#define STAGE_ARM IWRAM_ROOM_CODE __attribute__((optimize("O2", "no-tree-loop-distribute-patterns")))

/* the entry of `want` in the sorted table t[0..n) (a stamp is asked for a hundred times a frame) */
STAGE_ARM const BkEnt *stage_find(const BkEnt *t, int n, uint32_t want)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        uint32_t k = t[mid].idkey;
        if (k == want) return &t[mid];
        if (k < want) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

/* the baked picture of (id, key), NULL if it is not in the ROM (binary search). The blob's header is
 * checked once (a game asks for a hundred stamps a frame); `bk_n` is the entry count, 0 until then, -1
 * for a blob that is not this build's */
static int16_t bk_n;
static const BkEnt *find(int id, uint32_t key)
{
    const uint8_t *b = gba_backdrops;
    const BkEnt *t = (const BkEnt *)(const void *)(b + 12);
    uint32_t want = (uint32_t)id << 24 | (key & 0xFFFFFFu);
    if (bk_n <= 0) {
        if (bk_n < 0 || b[0] != 'B' || b[1] != 'K' || b[2] != 'D' || b[3] != '2' ||
            (b[4] | b[5] << 8) != SCREEN_W || (b[6] | b[7] << 8) != SCREEN_H) { bk_n = -1; return NULL; }
        bk_n = (int16_t)*(const uint32_t *)(const void *)(b + 8);
    }
    iwram_ovl_use(OVL_ROOM);
    return stage_find(t, bk_n, want);
}

int plat_stage_begin(int id, uint32_t key, void (*draw)(uint32_t key), PlatStageInfo *info)
{
    int back;
    (void)draw;
    frame_stage = 0;
    if (!info || gba_nodraw == 1) return 0;              /* a skipped tick */
    if (gba_nodraw == GBA_NODRAW_FIRST) gba_nodraw = 0;  /* (G5) the first call: Mode 4 */
    if (gba_nodraw != 0 && gba_nodraw != GBA_NODRAW_PANEL) return 0;   /* a tile frame */
    if (hold_building() || hold_page() >= 0) return 0;   /* held / built frames: not ours */
    cur = find(id, key);
    if (!cur || cur->w != SCREEN_W) { cur = NULL; return 0; }   /* (a stamp is not a backdrop) */
    back = gba_back == (uint16_t *)VRAM_PAGE1;
    info->held = page_serial[back];
    info->serial = serial + 1;
    info->dx0 = page_dirt[back][0]; info->dy0 = page_dirt[back][1];
    info->dx1 = page_dirt[back][2]; info->dy1 = page_dirt[back][3];
    frame_stage = 1;
    return 1;
}

/* columns [x0, x1) of rows [y0, y1) of the picture into the page: halfwords, with a
 * read-modify-write for an odd edge (VRAM takes no byte writes); a span of 4 halfwords or more by
 * DMA (S2-4c: ~3 cycles a halfword from the ROM after a short setup, where the CPU loop takes ~10:
 * the kitchen's narrow boxes, Granny's 42 px, were ~2 ms a frame), shorter ones by the CPU. ARM
 * in the room group of the IWRAM overlay (iwram_ovl.h: the house's group, which no stage scene
 * uses; loaded by plat_stage_restore), so the per-row work runs from IWRAM (STAGE_ARM above). */
STAGE_ARM void stage_rows(volatile uint16_t *page, const uint8_t *img, int x0, int x1, int y0, int y1)
{
    int yy;
    for (yy = y0; yy < y1; yy++) {
        const uint8_t *s = img + yy * SCREEN_W;
        volatile uint16_t *d = page + yy * (SCREEN_W / 2);
        int a = x0, b = x1, n;
        if (a & 1) {                                   /* left edge: the high byte */
            d[a >> 1] = (uint16_t)((d[a >> 1] & 0x00FF) | (uint16_t)s[a] << 8);
            a++;
        }
        if (b & 1) {                                   /* right edge: the low byte */
            b--;
            d[b >> 1] = (uint16_t)((d[b >> 1] & 0xFF00) | s[b]);
        }
        n = (b - a) >> 1;
        if (n >= 4) {
            dma3_copy16((void *)(d + (a >> 1)), s + a, (uint32_t)n);
        } else {
            const uint16_t *q = (const uint16_t *)(const void *)(s + a);
            volatile uint16_t *e = d + (a >> 1);
            while (n-- > 0) *e++ = *q++;
        }
    }
}

/* (S2-4b) n boxes (x, y, w, h), each moved by (dx, dy) and clipped to the screen's columns and the
 * picture's rows [ylo, yhi): the many small restores of a frame without a ROM (THUMB) call each */
STAGE_ARM void stage_list(volatile uint16_t *page, const uint8_t *img, const int16_t *b, int n, int dx, int dy, int ylo, int yhi)
{
    for (; n > 0; n--, b += 4) {
        int x0 = b[0] + dx, y0 = b[1] + dy, x1 = x0 + b[2], y1 = y0 + b[3];
        if (x0 < 0) x0 = 0;
        if (x1 > SCREEN_W) x1 = SCREEN_W;
        if (y0 < ylo) y0 = ylo;
        if (y1 > yhi) y1 = yhi;
        if (x0 < x1 && y0 < y1) stage_rows(page, img, x0, x1, y0, y1);
    }
}

void plat_stage_restore_list(const int16_t *xywh, int n, int dx, int dy)
{
    int ylo, yhi;
    if (!frame_stage || !cur || gba_nodraw == 1 || n <= 0) return;
    ylo = cur->by < 0 ? 0 : cur->by;
    yhi = cur->by + cur->h > SCREEN_H ? SCREEN_H : cur->by + cur->h;
    iwram_ovl_use(OVL_ROOM);
    stage_list(gba_back, gba_backdrops + cur->off - cur->by * SCREEN_W, xywh, n, dx, dy, ylo, yhi);
}

/* rows [y0, y1) of the picture, columns [x0, x1), into the back page. Full rows go 16 at a time
 * in one 32-bit DMA, so the VBlank IRQ (the mixer) waits at most ~0.4 ms. */
void plat_stage_restore(int x, int y, int w, int h)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > SCREEN_W ? SCREEN_W : x + w, y1 = y + h > SCREEN_H ? SCREEN_H : y + h, yy;
    const uint8_t *cur_img;
    if (!frame_stage || !cur || gba_nodraw == 1) return;
    if (y0 < cur->by) y0 = cur->by;                      /* (S2-4b) a band holds only its rows */
    if (y1 > cur->by + cur->h) y1 = cur->by + cur->h;
    if (x0 >= x1 || y0 >= y1) return;
    cur_img = gba_backdrops + cur->off - cur->by * SCREEN_W;     /* row yy of the screen at yy * SCREEN_W */
    if (x0 == 0 && x1 == SCREEN_W) {
        for (yy = y0; yy < y1; yy += 16) {
            int n = y1 - yy < 16 ? y1 - yy : 16;
            dma3_copy32(gba_back + yy * (SCREEN_W / 2), cur_img + yy * SCREEN_W, (uint32_t)(n * SCREEN_W / 4));
        }
        return;
    }
    iwram_ovl_use(OVL_ROOM);
    stage_rows(gba_back, cur_img, x0, x1, y0, y1);
}

/* (S2-4b) the stamp of (id, key) with its anchor at (x, y): an image in the ROM (index 0 a hole) */
int plat_stage_stamp(int id, uint32_t key, int x, int y)
{
    const BkEnt *e;
    DoomoImage im;
    if (gba_nodraw == 1) return 1;                       /* a skipped tick draws nothing */
    e = find(id, key);
    if (!e) return 0;
    if (!e->w) return 1;
    im.w = e->w;
    im.h = e->h;
    im.pix = gba_backdrops + e->off;
    im.uid = 0x80000000u | e->off;
    if (!gba_nodraw) vid_blit(x + e->bx, y + e->by, &im, 1, 0, 0);     /* a plain Mode 4 frame: the ARM blit */
    else plat_image(x + e->bx, y + e->by, &im, 1, 0, 0);               /* tracked frames: the routed path */
    return 1;
}

void plat_stage_end(void)
{
    if (!frame_stage || gba_nodraw == 1) return;
    tracking = 1;
    trk[0] = trk[1] = 0x7FFF;
    trk[2] = trk[3] = -0x7FFF;
    if (gba_nodraw == 0 || gba_nodraw == GBA_NODRAW_PANEL) gba_nodraw = GBA_NODRAW_TRACK;
}

/* vid_route (GBA_NODRAW_TRACK), plat_panel_begin: a box drawn after plat_stage_end */
void stage_note(int x0, int y0, int x1, int y1)
{
    if (!tracking) return;
    if (x0 < trk[0]) trk[0] = (int16_t)x0;
    if (y0 < trk[1]) trk[1] = (int16_t)y0;
    if (x1 > trk[2]) trk[2] = (int16_t)x1;
    if (y1 > trk[3]) trk[3] = (int16_t)y1;
}

/* video_flip: the page now shown (0 / 1), -1 for a tile frame or a frame presented again */
void stage_flipped(int page)
{
    serial++;
    if (page < 0) {
        page_serial[0] = page_serial[1] = 0;
    } else {
        page_serial[page] = frame_stage ? serial : 0;
        if (frame_stage && tracking && trk[0] < trk[2] && trk[1] < trk[3]) {
            page_dirt[page][0] = trk[0]; page_dirt[page][1] = trk[1];
            page_dirt[page][2] = trk[2]; page_dirt[page][3] = trk[3];
        } else {
            page_dirt[page][0] = page_dirt[page][1] = page_dirt[page][2] = page_dirt[page][3] = 0;
        }
    }
    frame_stage = 0;
    tracking = 0;
    if (gba_nodraw == GBA_NODRAW_TRACK) gba_nodraw = 0;
}
