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
 * (H10) The raycaster's clip checkpoints also serve the autopilot's BFS
 * (core/src/combat_bot.c) inside an update: gba_bot_work, gba_bot_claim().
 */
#include "gba_plat.h"
#include "doomo/save.h"
#include "doomo/plat_ext.h"
#include "../../core/src/fp_island.h"

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
        ".global gba_room\n\t.set gba_room, gba_arena + " ARENA_STR(GBA_ARENA_BB - GBA_BIG_ROOM) "\n\t"
        /* (H10) the autopilot's BFS: the bottom of the clip checkpoints */
        ".global gba_bot_work\n\t.set gba_bot_work, gba_arena + "
        ARENA_STR(GBA_ARENA_BB - GBA_BIG_R3D + GBA_BIG_CLIPS));

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
/* (H10) the autopilot's BFS stays inside the clip checkpoints: the roof
 * table is a cache that must survive it. combat_bot.c asserts that its
 * three arrays add up to 4,096 B; GBA_BIG_BOT is pinned to the same size
 * so neither side can change alone */
DOOMO_STATIC_ASSERT(GBA_BIG_BOT == 4096, bot_work_is_4k);
/* (CR5a) the island hub's lists (fp_island.h FP_ISLAND_SCRATCH_GBA) fit above the largest scratch claim */
DOOMO_STATIC_ASSERT(FP_ISLAND_SCRATCH_GBA % 8 == 0 && DOOMO_SCENE_SCRATCH + FP_ISLAND_SCRATCH_GBA <= GBA_ARENA_SIZE,
                    island_lists_beside_any_claim);
DOOMO_STATIC_ASSERT(GBA_BIG_CLIPS + GBA_BIG_BOT <= GBA_BIG_ROOF && (GBA_ARENA_BB - GBA_BIG_R3D + GBA_BIG_CLIPS) % 4 == 0,
                    bot_in_clips);

static int big_owner = GBA_BIG_FREE;
static uint32_t big_gen;
uint32_t gba_big_epoch;                /* (SP5) bumps on every change of owner or scratch claim */

void ow_invalidate(void);
void r3d_invalidate(void);

void gba_big_claim(int who)
{
    /* a new scene scratch claim may have covered the overworld's or the
     * house's region; the raycaster's clears any claim (asserted above),
     * so a claim alone never costs it its roof table */
    uint32_t gen = scene_scratch_gen();
    if (big_owner == who && (big_gen == gen || who == GBA_BIG_BY_R3D)) return;
    gba_big_epoch++;
    big_gen = gen;
    if (who == GBA_BIG_BY_OW) ow_invalidate();         /* tags were overwritten */
    else if (who == GBA_BIG_BY_R3D) r3d_invalidate();  /* roof table too       */
    big_owner = who;                                   /* (the house: a canary) */
}

int gba_big_fits(uint32_t bytes)
{
    return bytes <= (uint32_t)GBA_ARENA_BB && scene_scratch_used() <= (uint32_t)GBA_ARENA_BB - bytes;
}

/* (H10) the autopilot's BFS borrows the clip checkpoints during an update
 * (gba_plat.h gba_bot_work): they are filled and read inside plat_view3d()
 * only */
void gba_bot_claim(void)
{
    gba_big_claim(GBA_BIG_BY_R3D);
}

/* (CR5a) The island hub's recorded rect lists (core/src/fp_island_gfx.c lr_bind: 13,720 B, formerly .bss) live at
 * the top of the arena, over the region of the other owners and the billboards, which the 2D hub does not use
 * (the owner GBA_BIG_BY_ISL; a scene scratch claim of up to 12 KB sits below, asserted). The pointer is good
 * while *gen keeps its value: the raycaster, the overworld, the house (all claim, and fill the billboards only
 * after they claimed), a save's stream or a new scratch claim change gba_big_epoch, and the hub then records
 * its lists again (the same rects: a frame's work, no pixel changes). NULL (cap 0 lists, every phase drawn
 * live) only if the scratch claim leaves no room, which the assert below rules out. */
void *plat_scene_scratch(uint32_t bytes, uint32_t *gen)
{
    if (bytes > (uint32_t)GBA_ARENA_SIZE || scene_scratch_used() > (uint32_t)GBA_ARENA_SIZE - bytes) {
        if (gen) *gen = 0;
        return 0;
    }
    gba_big_claim(GBA_BIG_BY_ISL);
    if (gen) *gen = gba_big_epoch;
    return gba_arena + GBA_ARENA_SIZE - bytes;
}

void *plat_save_scratch(uint32_t *cap)
{
    uint32_t used = (scene_scratch_used() + 7u) & ~7u;
    gba_big_claim(GBA_BIG_BY_SAVE);
    if (cap) *cap = (uint32_t)GBA_ARENA_SIZE - used;
    return gba_arena + used;
}
