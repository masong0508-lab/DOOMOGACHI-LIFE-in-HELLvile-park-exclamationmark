/* save_scratch.c - the default buffer for save_game()/load_game() (owner:
 * foundation). Platforms short on RAM override plat_save_scratch() with
 * memory that is otherwise busy (and link with --gc-sections so this
 * buffer is dropped), or with a small buffer plus the streaming calls
 * (platform.h), whose defaults here say "not supported". */
#include "doomo/platform.h"
#include "doomo/save.h"

__attribute__((weak)) int plat_save_stream_begin(int slot, uint32_t size)
{
    (void)slot; (void)size;
    return 0;
}

__attribute__((weak)) int plat_save_stream_write(const void *data, uint32_t n)
{
    (void)data; (void)n;
    return 0;
}

__attribute__((weak)) int plat_save_stream_end(int commit)
{
    (void)commit;
    return 0;
}

__attribute__((weak)) int plat_save_read_at(int slot, uint32_t offset, void *data, uint32_t n,
                                            uint32_t *size)
{
    (void)slot; (void)offset; (void)data; (void)n; (void)size;
    return 0;
}

__attribute__((weak)) void *plat_save_scratch(uint32_t *cap)
{
    static uint8_t buf[DOOMO_SAVE_SCRATCH_MIN];
    if (cap) *cap = sizeof(buf);
    return buf;
}
