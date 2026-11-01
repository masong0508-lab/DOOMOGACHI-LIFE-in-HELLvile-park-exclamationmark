/* iwram_ovl.c - the IWRAM code overlay: one slot, one group of hot ARM
 * code at a time (owner: gba; G4). Rules and groups: iwram_ovl.h; layout:
 * gba.ld (OVERLAY .ovl_3d / .ovl_2d); numbers: docs/GBA.md section 3.
 *
 * THUMB, ROM: this code must never sit in the slot it overwrites.
 */
#include "gba_plat.h"
#include "iwram_ovl.h"

/* gba.ld */
extern uint8_t __iwram_ovl_start[], __iwram_ovl_end[], __sp_usr[];
extern const uint8_t __iwram_ovl_3d_lma[], __iwram_ovl_2d_lma[];
extern const uint8_t __iwram_ovl_3d_size[], __iwram_ovl_2d_size[];

static uint8_t ovl_cur;                  /* OVL_*: the group in the slot */
static uint32_t ovl_loads;               /* groups loaded since boot     */

#if defined(ENGINE_TEST)
/* the test ROM fills the slot's unused tail with undefined instructions,
 * so a call into a group that is not loaded traps instead of running
 * stale code of the other group */
static const uint32_t ovl_poison = 0xE7FFDEFEu;
#endif

int iwram_ovl_current(void)
{
    return ovl_cur;
}

uint32_t iwram_ovl_loads(void)
{
    return ovl_loads;
}

/* the slot's bytes after the 2D group (gba.ld asserts the size G6 needs) */
uint8_t *iwram_ovl_2d_tail(uint32_t *bytes)
{
    uint32_t size = (uint32_t)(uintptr_t)__iwram_ovl_2d_size;
    if (bytes) *bytes = (uint32_t)(__iwram_ovl_end - __iwram_ovl_start) - size;
    return __iwram_ovl_start + size;
}

void iwram_ovl_use(int g)
{
    const uint8_t *src;
    uint32_t size, words;
    volatile uint8_t here;
    if (g == ovl_cur) return;
    /* the IRQ (and SVC) stacks sit above the main stack (gba.ld) */
    if ((uintptr_t)&here >= (uintptr_t)__sp_usr) {
        gba_logf("iwram_ovl: group %d requested inside an IRQ handler", g);
        for (;;) {}
    }
    if (g == OVL_3D) { src = __iwram_ovl_3d_lma; size = (uint32_t)(uintptr_t)__iwram_ovl_3d_size; }
    else if (g == OVL_2D) { src = __iwram_ovl_2d_lma; size = (uint32_t)(uintptr_t)__iwram_ovl_2d_size; }
    else return;
    words = size >> 2;                   /* gba.ld aligns each group to 4 */
#if defined(ENGINE_TEST)
    {
        uint32_t c0 = gba_cycles();
        uint32_t slot = (uint32_t)(__iwram_ovl_end - __iwram_ovl_start) >> 2;
        if (words) dma3_copy32(__iwram_ovl_start, src, words);
        if (slot > words)
            dma3_fill32(__iwram_ovl_start + size, (const volatile uint32_t *)&ovl_poison,
                        slot - words);
        gba_logf("iwram_ovl: group %d in (%u bytes, %u cycles)", g, size, gba_cycles() - c0);
    }
#else
    if (words) dma3_copy32(__iwram_ovl_start, src, words);
#endif
    ovl_cur = (uint8_t)g;
    ovl_loads++;
}

#if defined(ENGINE_TEST)
IWRAM_2D_CODE uint32_t iwram_ovl_probe(uint32_t x)
{
    return x * 3u + 0x2Du;
}
#endif
