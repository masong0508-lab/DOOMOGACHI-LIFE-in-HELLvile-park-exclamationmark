/* plat_weak_ext.c - weak defaults for every hook in plat_ext.h (owner: K0
 * contract, LIFESIM_PLAN R3). A port overrides a hook with a strong
 * definition in its own tree; nobody edits this file to implement one.
 *
 * Every default means "not supported here" and is silent (no STUB log):
 * these hooks are called on normal paths (boot, opt_set, pause) where the
 * answer "no" is correct, not a missing module. With --gc-sections an
 * overridden or unused default costs nothing.
 */
#include "doomo/plat_ext.h"

#define WEAK __attribute__((weak))

WEAK uint32_t plat_features(void) { return 0; }

WEAK int plat_config_read(uint8_t *dst, uint32_t cap)
{
    (void)dst; (void)cap;
    return 0;
}

WEAK int plat_config_write(const uint8_t *src, uint32_t len)
{
    (void)src; (void)len;
    return 0;
}

WEAK void plat_option_changed(int opt, int value) { (void)opt; (void)value; }

WEAK void plat_rumble(int strength, int ticks) { (void)strength; (void)ticks; }

WEAK uint32_t plat_test_flags(void) { return 0; }

WEAK int plat_hold_frame(int on) { (void)on; return 0; }

WEAK uint8_t *plat_mslot_buf(uint32_t *cap)
{
    if (cap) *cap = 0;
    return 0;
}

WEAK const uint8_t *plat_pack_blob(uint32_t *size)
{
    if (size) *size = 0;
    return 0;
}

WEAK uint32_t plat_pack_read(uint32_t off, void *dst, uint32_t len)
{
    (void)off; (void)dst; (void)len;
    return 0;
}
