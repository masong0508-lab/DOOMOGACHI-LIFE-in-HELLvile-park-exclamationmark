/* save_scratch.c - the default buffer for save_game()/load_game() (owner:
 * foundation). Platforms short on RAM override plat_save_scratch() with
 * memory that is otherwise busy (and link with --gc-sections so this
 * buffer is dropped). */
#include "doomo/platform.h"
#include "doomo/save.h"

__attribute__((weak)) void *plat_save_scratch(uint32_t *cap)
{
    static uint8_t buf[DOOMO_SAVE_SCRATCH_MIN];
    if (cap) *cap = sizeof(buf);
    return buf;
}
