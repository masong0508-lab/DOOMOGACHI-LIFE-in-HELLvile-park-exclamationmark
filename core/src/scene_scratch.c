/* scene_scratch.c - the shared scene scratch block (scene.h; owner:
 * foundation, perf pass; the G0 memory pass while it runs). Kept in its own
 * file and referenced only through scene_scratch(), so a build where no
 * scene uses it drops the block with --gc-sections.
 *
 * On the GBA the block is the bottom of the platform's arena (gba/src/
 * bigbuf.c, gba_plat.h): the platform's caches, the save window and
 * doomo_frame_bb share that EWRAM block with it, and keep clear of the
 * claimed size (scene_scratch_used()). Elsewhere it is a static of its own.
 */
#include "doomo/scene.h"
#include "doomo/platform.h"

static uint32_t ss_owner, ss_size, ss_gen;

#if defined(DOOMO_PLATFORM_GBA)
extern uint8_t gba_arena[];                            /* gba/src/bigbuf.c */
static uint8_t *ss_block(void) { return gba_arena; }
#else
static uint8_t *ss_block(void)
{
    static uint32_t block[DOOMO_SCENE_SCRATCH / 4];    /* word aligned */
    return (uint8_t *)block;
}

/* The shared per-frame billboard list (scene.h). */
static DoomoBillboard frame_bb[DOOMO_FRAME_BB_MAX];
DoomoBillboard *const doomo_frame_bb = frame_bb;
#endif

void *scene_scratch(uint32_t owner, uint32_t size)
{
    if (size > DOOMO_SCENE_SCRATCH || owner == 0) return NULL;
    ss_owner = owner;
    ss_size = size;
    ss_gen++;
    return ss_block();
}

int scene_scratch_mine(uint32_t owner)
{
    return owner != 0 && ss_owner == owner;
}

void *scene_scratch_block(void)
{
    return ss_block();
}

/* (G0) for the platform: the bytes the current owner claimed (it owns
 * nothing above them), and a counter that changes with every claim. */
uint32_t scene_scratch_used(void) { return ss_owner ? ss_size : 0; }
uint32_t scene_scratch_gen(void) { return ss_gen; }
