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
#include "doomo/plat_ext.h"

/* 1 once P2/P3 put the held backdrop back on every frame; until then
 * plat_hold_frame says "cannot" (0) and the core's own redraw runs */
#define HOLD_RESTORE_READY 0

static int8_t last_page = -1;      /* the page the last flip showed, -1 tiles / none */
static int8_t held_page = -1;      /* the page that was shown when the hold began, -1 none */

/* video_flip: page now shown (0 / 1), -1 for a tile frame */
void hold_flipped(int page)
{
    last_page = (int8_t)page;
    if (page < 0) held_page = -1;  /* a tile frame ends a hold */
}

int hold_page(void)
{
    return held_page;
}

int plat_hold_frame(int on)
{
    held_page = -1;
    if (!on) return 0;
    if (gba_nodraw == 1 || last_page < 0) return 0;   /* skipped tick, tiles, nothing shown */
    held_page = last_page;
    return HOLD_RESTORE_READY;
}
