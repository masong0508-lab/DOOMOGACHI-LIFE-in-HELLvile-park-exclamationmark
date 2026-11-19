/* tonepick.h - pitch register and cycle choice for the PS1 tone voices (spu.c plat_tone).
 * Plain C, no PS1 headers, so ps1/harness/tonepick_test.c can check it on the host.
 *
 * Every tone waveform is uploaded in five zones (2026-11-19; before there were two): a cycle of
 * 112, 56, 28, 14 or 7 samples. The SPU plays a loop at hz x cycle samples a second; above 44.1 kHz
 * it skips samples and the cycle's upper harmonics fold back as notes that are not in the tune. The
 * shorter the cycle, the later that starts (112: 394 Hz, 56: 787 Hz, 28: 1.6 kHz, 14: 3.1 kHz,
 * 7: 6.3 kHz), so every note is played from the longest cycle that still reaches it comfortably and
 * the fold-back stays low across the whole range. The pitch register tops out at 0x3fff (4 x 44100
 * Hz): zone limits 1571 / 3142 / 6284 / 12568 / 25136 Hz.
 * Changing zone means keying the voice on again, and a key-on restarts the envelope and the
 * waveform: a sounding note gets a hard cut (a click). ps1_tone_variant() therefore keeps a
 * sounding note in its zone while the register can reach it and its steps are still fine (about a
 * cent or better), with a gap between the zone edges going up and going down (hysteresis). */
#ifndef PS1_TONEPICK_H
#define PS1_TONEPICK_H
#include <stdint.h>

#define PS1_ZONES         5
#define PS1_CYC_LONG      112      /* zone 0 */
#define PS1_CYC_SHORT     28       /* zone 2: the old short cycle */
#define PS1_PITCH_MAX     0x3fffu
#define PS1_LONG_LIMIT_HZ 1571u    /* zone 0's register end; zone z reaches this x 112 / cycle */

static const uint8_t ps1_zone_cyc[PS1_ZONES] = { 112, 56, 28, 14, 7 };
/* a NEW note in zone z+1 above this many Hz (the register reach of the old rule is kept: 1571 Hz splits 28 from 14) */
static const uint16_t ps1_zone_up[PS1_ZONES - 1] = { 400, 790, 1571, 3150 };
/* a SOUNDING note falls back to zone z-1 below this (register about 1820, a cent per step) */
static const uint16_t ps1_zone_down[PS1_ZONES] = { 0, 350, 700, 1400, 2800 };
/* the last Hz zone z reaches */
static const uint16_t ps1_zone_reach[PS1_ZONES] = { 1571, 3142, 6284, 12568, 25136 };

/* pitch register value for hz played from a cycle of cyc samples; 0x1000 = 44100 Hz.
 * Same result as (hz * cyc * 4096 + 22050) / 44100 clamped to PS1_PITCH_MAX, but the product
 * cannot wrap 32 bits (the old form wrapped above 37,449 Hz on the short cycle). */
static uint16_t ps1_tone_pitch(uint32_t hz, uint32_t cyc)
{
    uint32_t cap = (PS1_PITCH_MAX * 44100u) / (cyc * 4096u) + 1u;   /* the first hz past the register's end */
    uint32_t p;
    if (hz > cap)
        hz = cap;
    p = (hz * cyc * 4096u + 22050u) / 44100u;
    return (uint16_t)(p > PS1_PITCH_MAX ? PS1_PITCH_MAX : p);
}

/* the zone a new note takes */
static int ps1_tone_zone(uint32_t hz)
{
    int z = 0;
    while (z < PS1_ZONES - 1 && hz > ps1_zone_up[z])
        z++;
    return z;
}

/* 0..PS1_ZONES-1. sounding: the voice already plays this waveform (so choosing another zone would
 * key it on again); current: the zone it plays. A new note takes the zone its pitch needs; a
 * sounding note stays while hz is inside [down, reach] of its zone. */
static int ps1_tone_variant(uint32_t hz, int sounding, int current)
{
    if (sounding && current >= 0 && current < PS1_ZONES &&
        hz >= ps1_zone_down[current] && hz <= ps1_zone_reach[current])
        return current;
    return ps1_tone_zone(hz);
}
#endif
