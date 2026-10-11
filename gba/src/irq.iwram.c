/* irq.iwram.c - interrupt dispatch (ARM, IWRAM; owner: gba).
 * crt0.s gba_irq_master acknowledges and calls gba_irq_dispatch(). */
#include "gba_plat.h"

void gba_irq_dispatch(uint32_t flags);

void gba_irq_dispatch(uint32_t flags)
{
    if (flags & IRQ_VBLANK) {
        gba_vblanks++;
        audio_vblank();
    }
}

void gba_irq_init(void)
{
    REG_IME = 0;
    REG_DISPSTAT = DSTAT_VBL_IRQ;
    REG_IE = IRQ_VBLANK;
    REG_IF = 0xFFFF;
    BIOS_IF = 0;
    REG_IME = 1;
}
