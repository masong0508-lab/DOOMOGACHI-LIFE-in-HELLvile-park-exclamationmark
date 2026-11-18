/* tonepick.h - pitch register and cycle choice for the PS1 tone voices (spu.c plat_tone).
 * Plain C, no PS1 headers, so ps1/harness/tonepick_test.c can check it on the host.
 *
 * Every tone waveform is uploaded twice: a long cycle (112 samples, for bass and mid notes,
 * fine pitch steps) and a short one (28 samples, for high notes). The SPU pitch register
 * tops out at 0x3fff (4 x 44100 Hz), so the long cycle reaches 1575 Hz and the short one about 6300 Hz.
 * Changing the cycle means keying the voice on again, and a key-on restarts the envelope and
 * the waveform: a sounding note gets a hard cut (a click). ps1_tone_variant() therefore keeps a
 * sounding note on its cycle whenever the pitch register can still reach it. */
#ifndef PS1_TONEPICK_H
#define PS1_TONEPICK_H
#include <stdint.h>

#define PS1_CYC_LONG      112
#define PS1_CYC_SHORT     28
#define PS1_PITCH_MAX     0x3fffu
#define PS1_LONG_LIMIT_HZ 1571u    /* above this the long cycle would run into PS1_PITCH_MAX */
#define PS1_SHORT_STAY_HZ 700u     /* a sounding note on the short cycle goes back to the long one below this */

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

/* 0 = long cycle, 1 = short cycle. sounding: the voice already plays this waveform (so choosing
 * another cycle would key it on again); current: the cycle it plays. A new note takes the cycle
 * the pitch needs, as before; a sounding note on the short cycle stays there down to
 * PS1_SHORT_STAY_HZ (pitch steps there are still about a cent), and one on the long cycle moves
 * up only when it must. */
static int ps1_tone_variant(uint32_t hz, int sounding, int current)
{
    int fresh = hz > PS1_LONG_LIMIT_HZ ? 1 : 0;
    if (sounding && current)
        return hz < PS1_SHORT_STAY_HZ ? 0 : 1;
    return fresh;
}
#endif
