/* bigbuf.c - one big EWRAM block shared by the platform caches and the
 * core's save scratch (owner: gba).
 *
 * The core's save/load needs DOOMO_SAVE_SCRATCH_MIN (88 KB) of byte-
 * addressable RAM (VRAM cannot be used: it ignores byte writes). EWRAM is
 * 256 KB and the core's own state (doomo_town ~110 KB, doomo_map 32 KB,
 * ...) takes most of it, so the platform's rebuildable caches - the
 * overworld lit-tile cache, the raycaster's clip checkpoints, roof table,
 * tinted colormap and sprite order - live INSIDE that block.
 * plat_save_scratch() hands the whole block to the core and invalidates
 * the caches; they rebuild on the next 3D / overworld frame.
 */
#include "gba_plat.h"
#include "doomo/save.h"

EWRAM_BSS uint8_t gba_big[GBA_BIG_SIZE] __attribute__((aligned(8)));

void ow_invalidate(void);
void r3d_invalidate(void);

void *plat_save_scratch(uint32_t *cap)
{
    ow_invalidate();
    r3d_invalidate();
    if (cap) *cap = sizeof(gba_big);
    return gba_big;
}
