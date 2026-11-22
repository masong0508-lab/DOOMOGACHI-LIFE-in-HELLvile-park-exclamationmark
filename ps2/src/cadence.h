#ifndef PS2_CADENCE_H
#define PS2_CADENCE_H

#include <stdint.h>

enum {
    PS2_CADENCE_UPDATE,
    PS2_CADENCE_AUDIO,
    PS2_CADENCE_PREPARE,
    PS2_CADENCE_FLIP,
    PS2_CADENCE_PHASES
};

typedef struct {
    uint32_t last_vblank;
    uint32_t vblanks;
    uint32_t skipped_refreshes;
    uint32_t zero_vblank_iterations;
    uint32_t max_vblank_gap;
    uint32_t flips;
    uint32_t holds;
    uint64_t max_ticks[PS2_CADENCE_PHASES];
    uint64_t max_hold_ticks;
} Ps2CadenceWindow;

static inline void ps2_cadence_record(Ps2CadenceWindow *window,
                                      uint32_t vblank, int flipped,
                                      const uint64_t *phase_ticks,
                                      uint64_t hold_ticks)
{
    uint32_t gap = vblank - window->last_vblank;
    unsigned phase;

    window->last_vblank = vblank;
    window->vblanks += gap;
    if (gap > 1) window->skipped_refreshes += gap - 1;
    if (gap == 0) window->zero_vblank_iterations++;
    if (gap > window->max_vblank_gap) window->max_vblank_gap = gap;
    if (flipped) {
        window->flips++;
        for (phase = 0; phase < PS2_CADENCE_PHASES; phase++) {
            if (phase_ticks[phase] > window->max_ticks[phase])
                window->max_ticks[phase] = phase_ticks[phase];
        }
    } else {
        window->holds++;
        if (hold_ticks > window->max_hold_ticks)
            window->max_hold_ticks = hold_ticks;
    }
}

#endif
