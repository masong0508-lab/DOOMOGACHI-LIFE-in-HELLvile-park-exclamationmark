/* scene_scratch.c - the shared scene scratch block (scene.h; owner:
 * foundation, perf pass). Kept in its own file and referenced only through
 * scene_scratch(), so a build where no scene uses it drops the block with
 * --gc-sections. */
#include "doomo/scene.h"
#include "doomo/platform.h"

static uint32_t ss_owner;

static uint8_t *ss_block(void)
{
    static uint32_t block[DOOMO_SCENE_SCRATCH / 4];    /* word aligned */
    return (uint8_t *)block;
}

void *scene_scratch(uint32_t owner, uint32_t size)
{
    if (size > DOOMO_SCENE_SCRATCH || owner == 0) return NULL;
    ss_owner = owner;
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

/* The shared per-frame billboard list (scene.h). */
static DoomoBillboard frame_bb[DOOMO_FRAME_BB_MAX];
DoomoBillboard *const doomo_frame_bb = frame_bb;
