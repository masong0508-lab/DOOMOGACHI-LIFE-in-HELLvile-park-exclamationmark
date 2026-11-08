/* stage.c - the kept backdrop and the retained stage on the GBA (SP7; plat_ext.h plat_stage_*,
 * core/src/backdrop.c; owner: gba; docs/EXORCISM_SPEED.md).
 *
 * The backdrops are baked at build time: the native runner draws every key of every family
 * (core/include/doomo/backdrop.h) with the core's own drawing code at 240x160 and the ROM
 * links the file (backdrop_blob.s, build/gen/backdrops.bin: "BKD1", w, h, n, a sorted table of
 * id << 24 | key -> offset, then raw 8bpp pictures). A frame that asks for a baked key gets
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

extern const uint8_t gba_backdrops[];          /* backdrop_blob.s */

static const uint8_t *cur_img;                 /* this frame's backdrop (ROM)       */
static uint32_t serial;                        /* Mode 4 frames presented           */
static uint32_t page_serial[2];                /* stage frame each page holds, 0    */
static int16_t page_dirt[2][4];                /* drawn over it after plat_stage_end */
static int16_t trk[4];
static uint8_t frame_stage, tracking;

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* the baked picture of (id, key), NULL if it is not in the ROM (binary search) */
static const uint8_t *find(int id, uint32_t key)
{
    const uint8_t *b = gba_backdrops;
    uint32_t want = (uint32_t)id << 24 | (key & 0xFFFFFFu), n;
    int lo = 0, hi;
    if (b[0] != 'B' || b[1] != 'K' || b[2] != 'D' || b[3] != '1') return NULL;
    if ((b[4] | b[5] << 8) != SCREEN_W || (b[6] | b[7] << 8) != SCREEN_H) return NULL;
    n = rd32(b + 8);
    hi = (int)n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t k = rd32(b + 12 + 8 * mid);
        if (k == want) return b + rd32(b + 16 + 8 * mid);
        if (k < want) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
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
    cur_img = find(id, key);
    if (!cur_img) return 0;
    back = gba_back == (uint16_t *)VRAM_PAGE1;
    info->held = page_serial[back];
    info->serial = serial + 1;
    info->dx0 = page_dirt[back][0]; info->dy0 = page_dirt[back][1];
    info->dx1 = page_dirt[back][2]; info->dy1 = page_dirt[back][3];
    frame_stage = 1;
    return 1;
}

/* columns [x0, x1) of rows [y0, y1) of the picture into the page: halfwords, with a
 * read-modify-write for an odd edge (VRAM takes no byte writes); a short span by the CPU, a long
 * one by DMA. ARM in the room group of the IWRAM overlay (iwram_ovl.h: the house's group, which
 * no stage scene uses; loaded by plat_stage_restore), so the per-row work runs from IWRAM. */
#define STAGE_ARM IWRAM_ROOM_CODE __attribute__((optimize("O2", "no-tree-loop-distribute-patterns")))
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
        if (n >= 24) {
            dma3_copy16((void *)(d + (a >> 1)), s + a, (uint32_t)n);
        } else {
            const uint16_t *q = (const uint16_t *)(const void *)(s + a);
            volatile uint16_t *e = d + (a >> 1);
            while (n-- > 0) *e++ = *q++;
        }
    }
}

/* rows [y0, y1) of the picture, columns [x0, x1), into the back page. Full rows go 16 at a time
 * in one 32-bit DMA, so the VBlank IRQ (the mixer) waits at most ~0.4 ms. */
void plat_stage_restore(int x, int y, int w, int h)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > SCREEN_W ? SCREEN_W : x + w, y1 = y + h > SCREEN_H ? SCREEN_H : y + h, yy;
    if (!frame_stage || !cur_img || gba_nodraw == 1 || x0 >= x1 || y0 >= y1) return;
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
