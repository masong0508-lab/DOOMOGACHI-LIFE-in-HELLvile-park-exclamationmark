/* bigbuf.c - the arena: one EWRAM block shared by the core's scene
 * scratch, the platform's caches, the per-frame billboards and the core's
 * save stream (owner: gba; layout in gba_plat.h).
 *
 * EWRAM is 256 KB and the core's own state (doomo_town ~110 KB,
 * doomo_map 32 KB, ...) takes most of it. Until G0 (task #77) the scene
 * scratch (12 KB), gba_big (17 KB: the overworld's lit-tile cache, the
 * raycaster's clip checkpoints / roof table / sprite order, the house, the
 * save window) and the billboard list (2.4 KB) were three blocks, 32,096 B.
 * They are never all full at once: combat holds the weapon views (12 KB of
 * scratch) and the raycaster's 11.8 KB, the story overworld its 6.5 KB of
 * scratch and the 17 KB tile cache, a save needs 14.6 KB for one update
 * call. So they are one 26,992 B block: the scratch from the bottom, the
 * billboards at the top, the platform owner's region right below them
 * (gba_big_claim: whoever uses it claims it first, which invalidates the
 * previous owner's cache - it is rebuilt in a frame), the save above the
 * scratch's claim. A scene owns the bytes it claimed (scene.h); a new claim
 * changes scene_scratch_gen(), which invalidates the platform cache too.
 */
#include "gba_plat.h"
#include "doomo/save.h"

EWRAM_BSS uint8_t gba_arena[GBA_ARENA_SIZE] __attribute__((aligned(8)));
/* the core's per-frame billboard list (scene.h), at the top */
DoomoBillboard *const doomo_frame_bb = (DoomoBillboard *)(void *)(gba_arena + GBA_ARENA_BB);
/* gba_r3d / gba_ow / gba_room (gba_plat.h): the raycaster's, the
 * overworld's and the house's region bases as symbols of their own: with
 * `gba_arena + offset` GCC keeps gba_arena in a register and cannot fold
 * the offset into THUMB loads, which slowed room.c's sort by a third */
#define ARENA_STR_(x) #x
#define ARENA_STR(x) ARENA_STR_(x)
__asm__(".global gba_r3d\n\t.set gba_r3d, gba_arena + " ARENA_STR(GBA_ARENA_BB - GBA_BIG_R3D) "\n\t"
        ".global gba_ow\n\t.set gba_ow, gba_arena + " ARENA_STR(GBA_ARENA_BB - GBA_BIG_OW) "\n\t"
        ".global gba_room\n\t.set gba_room, gba_arena + " ARENA_STR(GBA_ARENA_BB - GBA_BIG_ROOM));

DOOMO_STATIC_ASSERT(GBA_BB_BYTES == DOOMO_FRAME_BB_MAX * sizeof(DoomoBillboard), bb_bytes);
DOOMO_STATIC_ASSERT(GBA_ARENA_SIZE % 8 == 0 && GBA_ARENA_BB % 8 == 0, arena_aligned);
DOOMO_STATIC_ASSERT((GBA_ARENA_BB - GBA_BIG_R3D) % 8 == 0 && (GBA_ARENA_BB - GBA_BIG_OW) % 4 == 0,
                    regions_aligned);
/* the raycaster's region clears the largest scratch claim */
DOOMO_STATIC_ASSERT((uint32_t)GBA_ARENA_BB - DOOMO_SCENE_SCRATCH >= (uint32_t)GBA_BIG_R3D, r3d_beside_any_claim);
/* a save fits above the largest claim: the codec's work area and a window
 * of at least 64 B (save.h; the tests cover every window from 64 B) */
DOOMO_STATIC_ASSERT(GBA_ARENA_SIZE - DOOMO_SCENE_SCRATCH >= DOOMO_SAVE_STREAM_MIN + 48u, save_beside_any_claim);
/* ... and a 1 KB window beside the free-play island's 11.3 KB (its
 * autosave) */
DOOMO_STATIC_ASSERT(GBA_ARENA_SIZE - 11312u >= DOOMO_SAVE_STREAM_MIN + 1024u, island_save_window);

static int big_owner = GBA_BIG_FREE;
static uint32_t big_gen;

void ow_invalidate(void);
void r3d_invalidate(void);

void gba_big_claim(int who)
{
    /* a new scene scratch claim may have covered the overworld's or the
     * house's region; the raycaster's clears any claim (asserted above),
     * so a claim alone never costs it its roof table */
    uint32_t gen = scene_scratch_gen();
    if (big_owner == who && (big_gen == gen || who == GBA_BIG_BY_R3D)) return;
    big_gen = gen;
    if (who == GBA_BIG_BY_OW) ow_invalidate();         /* tags were overwritten */
    else if (who == GBA_BIG_BY_R3D) r3d_invalidate();  /* roof table too       */
    big_owner = who;                                   /* (the house: a canary) */
}

int gba_big_fits(uint32_t bytes)
{
    return bytes <= (uint32_t)GBA_ARENA_BB && scene_scratch_used() <= (uint32_t)GBA_ARENA_BB - bytes;
}

void *plat_save_scratch(uint32_t *cap)
{
    uint32_t used = (scene_scratch_used() + 7u) & ~7u;
    gba_big_claim(GBA_BIG_BY_SAVE);
    if (cap) *cap = (uint32_t)GBA_ARENA_SIZE - used;
    return gba_arena + used;
}
