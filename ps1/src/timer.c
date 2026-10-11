/* timer.c - profiling clock on root counter 1 (hblank source).
 *
 * Root counter 1 counts horizontal blanks (15734 Hz NTSC, 15625 Hz PAL),
 * i.e. one tick every ~63.6 us. timer_us() extends the 16-bit hardware
 * counter in software; it only has to be polled at least every ~4 s
 * (once per frame in practice).
 */
#include <psxapi.h>
#include <hwregs_c.h>
#include "ps1.h"

static uint32_t s_ext;      /* extended hblank count */
static uint16_t s_last;

void timer_init(void)
{
    /* counter 1: source = hblank (bits 8-9 = 1), free running,
     * no IRQ, no reset on target */
    TIMER_CTRL(1) = 0x0100;
    s_last = TIMER_VALUE(1);
    s_ext = 0;
}

uint32_t timer_hblanks(void)
{
    uint16_t v = TIMER_VALUE(1);
    s_ext += (uint16_t)(v - s_last);
    s_last = v;
    return s_ext;
}

uint32_t timer_us(void)
{
#ifdef DOOMO_PAL
    return timer_hblanks() * 64u;
#else
    return timer_hblanks() * 6356u / 100u;
#endif
}
