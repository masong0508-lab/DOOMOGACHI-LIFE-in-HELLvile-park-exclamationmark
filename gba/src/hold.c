/* hold.c - the held frame on the GBA (U4, platform plat_ext.h; part P1 of
 * docs/PAUSE_SPEED.md; owner: gba).
 *
 * plat_hold_frame(1) is asked for when the overworld's pause menu opens:
 * keep the picture just shown as the backdrop so the core repaints only the
 * pause window on top. This file is the state and the rules for it; the
 * restore that makes the backdrop real is not in yet, so for now it answers
 * 0 for everything and the core redraws its backdrop every frame as before
 * (HOLD_RESTORE_READY).
 *
 * What a hold needs (and why the restore is its own step): the two Mode 4
 * pages are both needed to flip, so a clean third copy of the backdrop does
 * not exist, and the core dims and draws the window's shadow over the
 * backdrop on every paused frame, which would compound on a page that is
 * not restored. P2 dims the held pages once, P3 keeps the small ring
 * around the window (shadow, title pill, corners) so it can be put back.
 *
 * Rules decided here, kept by the restore later:
 *   - only a Mode 4 frame can be held: the last flip must have shown a page
 *     (a tile frame, Mode 0, shows -1, and nothing shown yet is -1 too);
 *   - not on a skipped (catch-up) tick: gba_nodraw == 1, nothing is drawn;
 *   - a tile frame flip, plat_hold_frame(0) and any scene_set release it.
 * RAM: 2 bytes.
 */
#include "gba_plat.h"
#include "canvas_int.h"
#include "r3d_int.h"            /* SP3: the raycaster's colormap rows share the VRAM gaps */
#include "doomo/plat_ext.h"
#include "hcmd.h"

void ow_invalidate(void);

/* 1 once P2/P3 put the held backdrop back on every frame; until then
 * plat_hold_frame says "cannot" (0) and the core's own redraw runs */
#define HOLD_RESTORE_READY 0     /* plat_hold_frame(1): the early hold stays off */

static int8_t last_page = -1;      /* the page the last flip showed, -1 tiles / none */
static int8_t held_page = -1;      /* the page that was shown when the hold began, -1 none */
static int8_t building;            /* bit 0, I10: a frame is being built in the hidden page over several ticks;
                                      bit 1, S2-11: this tick's frame is a slice of the house's view turn (room.c) */
static int8_t on_shown;            /* I10: gba_back is pointed at the shown page (PLAT_BUILD_SHOWN) */

/* video_flip: page now shown (0 / 1), -1 for a tile frame */
void hold_flipped(int page)
{
    last_page = (int8_t)page;
    if (page < 0) { held_page = -1; building &= ~1; }  /* a tile frame ends a hold (and a build) */
}

int hold_page(void)
{
    return held_page;
}

int hold_building(void)
{
    return building;
}

/* S2-11 (room.c): the house builds a view turn in the hidden page over several ticks, like an I10 build but on
 * its own: this tick's frame is not whole, so video_flip presents the shown page again. The flag lasts one tick
 * (plat_begin_frame clears it): a tick the house does not draw on flips as usual. */
void hold_room_slice(void)
{
    building |= 2;
}

void hold_frame_begin(void)
{
    building &= ~2;
}

/* the hidden page's address back in gba_back, panels back on */
static void build_hidden(void)
{
    if (on_shown) {
        gba_back = (uint16_t *)(last_page ? VRAM_PAGE0 : VRAM_PAGE1);
        pn_suspend(0);
        on_shown = 0;
    }
}

/* I10 (plat_ext.h): a frame built over several ticks, in the hidden page, while the held page stays shown */
int plat_build_frame(int op)
{
    switch (op) {
    case 1:                                            /* PLAT_BUILD_START */
        if (gba_nodraw == 1 || last_page < 0 || held_page < 0) return 0;
        building |= 1;
        return 1;
    case 2:                                            /* PLAT_BUILD_SHOWN */
        if (!(building & 1) || gba_nodraw == 1 || last_page < 0) return 0;
        gba_back = (uint16_t *)(last_page ? VRAM_PAGE1 : VRAM_PAGE0);
        pn_suspend(1);
        on_shown = 1;
        return 1;
    case 3:                                            /* PLAT_BUILD_HIDDEN */
        build_hidden();
        return 1;
    case 4:                                            /* PLAT_BUILD_BUSY (S2-11) */
        return (building & 2) != 0;
    }
    return 0;
}

/* P2/P3: plat_hold_frame(2) after the pop-in's last (settled, dimmed, full)
 * frame was shown: copy the shown page into the other one, so both pages
 * carry it; every later frame repaints only the window and the title pill
 * (core/src/overworld.c, owfx_draw_pause_ex(1)). 4 asks "still held?" (a tile
 * frame flip, plat_hold_frame(0) or scene_set ended it). No new RAM: the other
 * Mode 4 page is the copy. */
static int hold_settle(void)
{
    volatile uint16_t *src, *dst;
    if (gba_nodraw == 1 || last_page < 0) return 0;
    src = last_page ? VRAM_PAGE1 : VRAM_PAGE0;
    dst = last_page ? VRAM_PAGE0 : VRAM_PAGE1;
    dma3_copy32((void *)dst, (const void *)src, (uint32_t)(SCREEN_W * SCREEN_H / 4));
    held_page = last_page;
    return 1;
}

int plat_hold_frame(int on)
{
    if (on == 4) return held_page >= 0;
    build_hidden();
    building &= ~1;                /* I10: any other end of the hold ends a build, this tick flips */
    held_page = -1;
    if (on == 2) return hold_settle();
    if (!on) return 0;
    if (gba_nodraw == 1 || last_page < 0) return 0;   /* skipped tick, tiles, nothing shown */
    held_page = last_page;
    return HOLD_RESTORE_READY;
}

/* SP3 / I11 (plat_ext.h plat_strip): a clean copy of a band of rows of the frame being drawn, in the two 2,560-byte
 * VRAM gaps the Mode 4 pages leave (0x06009600, 0x06013600: never displayed): the first 10 rows in gap A, the rest in
 * gap B, 240 bytes a row, so up to PLAT_STRIP_MAX_H (20) rows. The gaps are shared (r3d_int.h): the raycaster's
 * colormap rows (rebuilt when r3d_cmrow[0] is 0, set here as the overworld's tile slots do), the overworld's
 * per-frame tile slots and the house's roof tables (rebuilt after any frame the house did not draw). None of them
 * draws in a frame of the island hub, the only user, and a scene change ends the hold the copy is for (the core
 * then saves it again before it restores). Costs 2 DMAs (~0.15 ms for 18 rows) per call; no RAM. */
#define STRIP_GAP_A ((uint8_t *)0x06009600)
#define STRIP_GAP_B ((uint8_t *)0x06013600)
#define STRIP_ROWS_A 10
DOOMO_STATIC_ASSERT(STRIP_ROWS_A * SCREEN_W <= 2560 && (PLAT_STRIP_MAX_H - STRIP_ROWS_A) * SCREEN_W <= 2560,
                    strip_fits_the_gaps);
static int16_t strip_y = -1, strip_h;          /* (int16: a band can start below row 127) */

int plat_strip(int op, int y, int h)
{
    uint8_t *row;
    int a;
    if (gba_nodraw == 1 || y < 0 || h <= 0 || h > PLAT_STRIP_MAX_H || y + h > SCREEN_H) return 0;
    if (gba_nodraw == GBA_NODRAW_FIRST) vid_route(0, y, SCREEN_W, y + h);   /* G5: the first 2D call decides */
    if (gba_nodraw != 0 && gba_nodraw != GBA_NODRAW_MEASURE && gba_nodraw != GBA_NODRAW_PANEL) return 0;  /* tiles */
    if (op == PLAT_STRIP_RESTORE && (y != strip_y || h != strip_h)) return 0;
    if (op != PLAT_STRIP_SAVE && op != PLAT_STRIP_RESTORE) return 0;
    row = (uint8_t *)gba_back + y * SCREEN_W;
    a = h < STRIP_ROWS_A ? h : STRIP_ROWS_A;
    if (op == PLAT_STRIP_SAVE) {
        view_hold_drop();                              /* (S2-2) the held 3D view's rows share the gaps */
        r3d_cmrow[0] = 0;                              /* the raycaster's rows are gone */
        dma3_copy32(STRIP_GAP_A, row, (uint32_t)(a * SCREEN_W / 4));
        if (h > a) dma3_copy32(STRIP_GAP_B, row + a * SCREEN_W, (uint32_t)((h - a) * SCREEN_W / 4));
        strip_y = (int16_t)y;
        strip_h = (int16_t)h;
    } else {
        dma3_copy32(row, STRIP_GAP_A, (uint32_t)(a * SCREEN_W / 4));
        if (h > a) dma3_copy32(row + a * SCREEN_W, STRIP_GAP_B, (uint32_t)((h - a) * SCREEN_W / 4));
    }
    return 1;
}

/* ---- SP5 (G-D1): the band under the dialogue box (plat_ext.h plat_hold_band; docs/GBA.md "D") ---------------
 * The story overworld's held dialogue backdrop: both Mode 4 pages carry the last full frame (plat_hold_frame(2)
 * above), and this keeps a clean copy of the picture under the box's band, taken before the box was drawn, so a
 * held frame puts the band back and the dialogue draws on it as on a full frame.
 *
 * Where: the arena (bigbuf.c) between the scene scratch's claim and the per-frame billboards. While the story
 * overworld draws, that is its lit-tile cache (GBA_BIG_OW) and free bytes below it; a held frame draws no world,
 * so the cache is borrowed and invalidated when the band goes (it rebuilds on the next world frame, a few ms).
 * Any change of the arena's owner or scratch claim (a save's stream, another region's user: gba_big_epoch) loses
 * the copy, and RESTORE then says 0. No new RAM but these 14 bytes of state.
 * What: rows [y, 160) of the page being drawn, without the core rect the dialogue covers with its opaque body on
 * every frame (narrowed to whole words), as full-row runs (one DMA each) and the two strips beside the core.
 * Only a Mode 4 frame is read or written (never a tile frame, a skipped tick, a frame built over several ticks or
 * one the harness reads text from). */
static uint8_t  band_ok;
static uint8_t  band_y;                    /* first row kept */
static uint8_t  band_cy0, band_cy1;        /* the core's rows, clipped to [band_y, 160) */
static uint8_t  band_cx0, band_cx1;        /* the core's columns, whole words */
static uint32_t band_epoch;
static uint8_t *band_mem;

static int band_frame_ok(void)
{
    if (gba_nodraw == 1 || gba_nodraw == GBA_NODRAW_CANVAS || hold_building() || on_shown || gba_text_armed) return 0;
    return 1;
}

/* bytes the band takes from row y */
static uint32_t band_bytes(int y, int cx0, int cy0, int cx1, int cy1)
{
    int c0 = DOOMO_MAX(cy0, y), c1 = DOOMO_MAX(cy1, c0);
    return (uint32_t)(SCREEN_H - y) * SCREEN_W - (uint32_t)(c1 - c0) * (uint32_t)(cx1 - cx0);
}

/* copy the band between the page being drawn and band_mem (dir 0: page -> mem, 1: mem -> page) */
static void band_copy(int dir)
{
    uint8_t *page = (uint8_t *)gba_back, *mem = band_mem;
    int y = band_y, n;
    n = (band_cy0 - y) * SCREEN_W;                    /* full rows above the core */
    if (n > 0) {
        if (dir) dma3_copy32(page + y * SCREEN_W, mem, (uint32_t)n / 4u);
        else dma3_copy32(mem, page + y * SCREEN_W, (uint32_t)n / 4u);
        mem += n;
    }
    for (y = band_cy0; y < band_cy1; y++) {           /* the strips beside it */
        uint32_t *row = (uint32_t *)(void *)(page + y * SCREEN_W), *m = (uint32_t *)(void *)mem;
        int k;
        for (k = 0; k < band_cx0 / 4; k++) { if (dir) row[k] = m[k]; else m[k] = row[k]; }
        m += band_cx0 / 4;
        for (k = band_cx1 / 4; k < SCREEN_W / 4; k++, m++) { if (dir) row[k] = *m; else *m = row[k]; }
        mem = (uint8_t *)m;
    }
    y = DOOMO_MAX(band_cy1, band_y);
    n = (SCREEN_H - y) * SCREEN_W;                    /* full rows below it */
    if (n > 0) {
        if (dir) dma3_copy32(page + y * SCREEN_W, mem, (uint32_t)n / 4u);
        else dma3_copy32(mem, page + y * SCREEN_W, (uint32_t)n / 4u);
    }
}

/* plat_overworld (a world frame) and DROP: the borrowed cache bytes are garbage (once the arena changed hands
 * since, its next claim by the overworld invalidates the cache anyway, and the bytes are the new owner's) */
void hold_band_release(void)
{
    if (!band_ok) return;
    band_ok = 0;
    if (band_epoch == gba_big_epoch) ow_invalidate();
}

int plat_hold_band(int op, const PlatBand *b)
{
    if (op == PLAT_BAND_SAVE) {
        uint32_t used = (scene_scratch_used() + 7u) & ~7u, cap;
        int y, cx0, cx1, cy0, cy1;
        hold_band_release();
        if (!b || !band_frame_ok() || used >= (uint32_t)GBA_ARENA_BB) return -1;
        cap = (uint32_t)GBA_ARENA_BB - used;
        cx0 = (DOOMO_CLAMP(b->cx0, 0, SCREEN_W) + 3) & ~3;
        cx1 = DOOMO_CLAMP(b->cx1, 0, SCREEN_W) & ~3;
        cy0 = DOOMO_CLAMP(b->cy0, 0, SCREEN_H);
        cy1 = DOOMO_CLAMP(b->cy1, 0, SCREEN_H);
        if (cx1 <= cx0 || cy1 <= cy0) { cx0 = cx1 = 0; cy0 = cy1 = SCREEN_H; }
        y = DOOMO_CLAMP(b->y_want, 0, SCREEN_H - 1);
        while (y < b->y_need && band_bytes(y, cx0, cy0, cx1, cy1) > cap) y++;
        if (y > b->y_need || band_bytes(y, cx0, cy0, cx1, cy1) > cap) return -1;
        if (gba_nodraw == GBA_NODRAW_FIRST) gba_nodraw = 0;      /* (nothing was drawn: a Mode 4 frame) */
        gba_big_claim(GBA_BIG_BY_OW);
        band_mem = gba_arena + used;
        band_y = (uint8_t)y;
        band_cy0 = (uint8_t)DOOMO_MAX(cy0, y);
        band_cy1 = (uint8_t)DOOMO_MAX(cy1, band_cy0);
        band_cx0 = (uint8_t)cx0;
        band_cx1 = (uint8_t)cx1;
        band_copy(0);
        band_ok = 1;
        band_epoch = gba_big_epoch;
        return y;
    }
    if (op == PLAT_BAND_RESTORE) {
        if (!band_ok || band_epoch != gba_big_epoch || held_page < 0 || !band_frame_ok()) return 0;
        if (gba_nodraw == GBA_NODRAW_FIRST) gba_nodraw = 0;      /* a held frame is a Mode 4 frame */
        band_copy(1);
        return 1;
    }
    hold_band_release();
    return 0;
}
