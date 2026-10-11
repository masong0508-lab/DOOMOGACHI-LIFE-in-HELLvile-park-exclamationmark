/* bigbuf.c - one EWRAM block shared, in turn, by the platform's caches
 * and the core's save stream (owner: gba).
 *
 * EWRAM is 256 KB and the core's own state (doomo_town ~110 KB,
 * doomo_map 32 KB, ...) takes most of it, so the overworld's lit-tile
 * cache (17 KB) and the raycaster's per-frame clip checkpoints, roof table
 * and sprite order (13 KB) live in the SAME 17 KB block: a frame draws
 * either the overworld or the 3D view, and each claims the block before
 * use (gba_big_claim), which invalidates the other one's cache - they
 * rebuild in a frame. The save code borrows it too: plat_save_scratch()
 * hands it out for streamed saves (platform.h, save.h: the codec's work
 * area takes the last DOOMO_SAVE_WORK_SIZE bytes, the rest is the stream
 * window; the core streams because 17 KB is less than a slot, so no 88 KB
 * save image is ever needed, see save_flash.c).
 */
#include "gba_plat.h"
#include "doomo/save.h"

EWRAM_BSS uint8_t gba_big[GBA_BIG_SIZE] __attribute__((aligned(8)));
/* the core's save codec keeps its work area in the scratch (save.h) */
DOOMO_STATIC_ASSERT(GBA_BIG_SIZE >= DOOMO_SAVE_STREAM_MIN + 1024u, save_window_fits);
static int big_owner = GBA_BIG_FREE;

void ow_invalidate(void);
void r3d_invalidate(void);

void gba_big_claim(int who)
{
    if (big_owner == who) return;
    if (who == GBA_BIG_BY_OW) ow_invalidate();         /* tags were overwritten */
    else if (who == GBA_BIG_BY_R3D) r3d_invalidate();  /* roof table too       */
    big_owner = who;
}

void *plat_save_scratch(uint32_t *cap)
{
    gba_big_claim(GBA_BIG_BY_SAVE);
    if (cap) *cap = sizeof(gba_big);
    return gba_big;
}
