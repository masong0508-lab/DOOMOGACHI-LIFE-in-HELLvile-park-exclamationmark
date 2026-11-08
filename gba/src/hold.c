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

/* 1 once P2/P3 put the held backdrop back on every frame; until then
 * plat_hold_frame says "cannot" (0) and the core's own redraw runs */
#define HOLD_RESTORE_READY 0     /* plat_hold_frame(1): the early hold stays off */

static int8_t last_page = -1;      /* the page the last flip showed, -1 tiles / none */
static int8_t held_page = -1;      /* the page that was shown when the hold began, -1 none */
static int8_t building;            /* I10: a frame is being built in the hidden page over several ticks */
static int8_t on_shown;            /* I10: gba_back is pointed at the shown page (PLAT_BUILD_SHOWN) */

/* video_flip: page now shown (0 / 1), -1 for a tile frame */
void hold_flipped(int page)
{
    last_page = (int8_t)page;
    if (page < 0) { held_page = -1; building = 0; }  /* a tile frame ends a hold (and a build) */
}

int hold_page(void)
{
    return held_page;
}

int hold_building(void)
{
    return building;
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
        building = 1;
        return 1;
    case 2:                                            /* PLAT_BUILD_SHOWN */
        if (!building || gba_nodraw == 1 || last_page < 0) return 0;
        gba_back = (uint16_t *)(last_page ? VRAM_PAGE1 : VRAM_PAGE0);
        pn_suspend(1);
        on_shown = 1;
        return 1;
    case 3:                                            /* PLAT_BUILD_HIDDEN */
        build_hidden();
        return 1;
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
    building = 0;                  /* I10: any other end of the hold ends a build, this tick flips */
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
