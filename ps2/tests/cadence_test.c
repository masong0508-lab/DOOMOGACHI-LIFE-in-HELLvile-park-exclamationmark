#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "cadence.h"

int main(void)
{
    Ps2CadenceWindow window = {0};
    const uint64_t fast[PS2_CADENCE_PHASES] = {31, 12, 75, 120};
    const uint64_t slow[PS2_CADENCE_PHASES] = {200, 10, 91, 55};
    const uint64_t idle[PS2_CADENCE_PHASES] = {0};

    window.last_vblank = UINT32_MAX;
    ps2_cadence_record(&window, 0, 1, fast, 0);
    ps2_cadence_record(&window, 3, 1, slow, 0);
    ps2_cadence_record(&window, 3, 0, idle, 110);
    ps2_cadence_record(&window, 4, 0, idle, 70);

    assert(window.vblanks == 5);
    assert(window.skipped_refreshes == 2);
    assert(window.zero_vblank_iterations == 1);
    assert(window.max_vblank_gap == 3);
    assert(window.flips == 2);
    assert(window.holds == 2);
    assert(window.max_ticks[PS2_CADENCE_UPDATE] == 200);
    assert(window.max_ticks[PS2_CADENCE_AUDIO] == 12);
    assert(window.max_ticks[PS2_CADENCE_PREPARE] == 91);
    assert(window.max_ticks[PS2_CADENCE_FLIP] == 120);
    assert(window.max_hold_ticks == 110);
    puts("PS2 cadence: IRQ rollover, skipped refreshes, phase peaks and holds passed");
    return 0;
}
