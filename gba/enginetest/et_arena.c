/* et_arena.c - (CR5a) checks the island hub's lists region at the top of the arena (bigbuf.c plat_scene_scratch)
 * at engine-test boot (owner: gba). Logs "ET arena ok" or "ET arena FAIL ...". */
#include "gba_plat.h"
#include "doomo/plat_ext.h"
#include "doomo/scene.h"
#include "../../core/src/fp_island.h"
#include "enginetest.h"

void et_arena_test(void)
{
    uint32_t g1 = 0, g2 = 0, g3 = 0, g4 = 0, cap = 0;
    uint8_t *p, *q;
    /* even beside the largest scene scratch claim, the lists clear it and the arena's end is theirs */
    scene_scratch(0xE7A1u, DOOMO_SCENE_SCRATCH);
    p = (uint8_t *)plat_scene_scratch(FP_ISLAND_SCRATCH_GBA, &g1);
    if (!p || p != gba_arena + GBA_ARENA_SIZE - FP_ISLAND_SCRATCH_GBA || ((uintptr_t)p & 3u) ||
        p < gba_arena + DOOMO_SCENE_SCRATCH) {
        gba_logf("ET arena FAIL region %p", (void *)p);
        return;
    }
    q = (uint8_t *)plat_scene_scratch(FP_ISLAND_SCRATCH_GBA, &g2);
    if (q != p || g2 != g1) { gba_logf("ET arena FAIL not stable %u %u", (unsigned)g1, (unsigned)g2); return; }
    /* the raycaster (and the autopilot's BFS) takes the memory: the lists must be recorded again */
    gba_bot_claim();
    plat_scene_scratch(FP_ISLAND_SCRATCH_GBA, &g3);
    if (g3 == g2) { gba_logf("ET arena FAIL r3d claim kept the lists"); return; }
    /* a save's stream overwrites them too */
    (void)plat_save_scratch(&cap);
    plat_scene_scratch(FP_ISLAND_SCRATCH_GBA, &g4);
    if (g4 == g3) { gba_logf("ET arena FAIL save kept the lists"); return; }
    /* a claim bigger than the arena is refused, with no pointer (the lists then run with cap 0) */
    if (plat_scene_scratch(GBA_ARENA_SIZE + 8u, &g4) != 0) { gba_logf("ET arena FAIL too big"); return; }
    scene_scratch(0xE7A1u, 0);
    gba_logf("ET arena ok");
}
