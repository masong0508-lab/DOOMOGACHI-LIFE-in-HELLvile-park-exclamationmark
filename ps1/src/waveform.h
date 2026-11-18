/* waveform.h - boot-time waveform shaping for the PS1 tone loops.
 * Tone loops are uploaded once to SPU RAM and replayed by hardware. The
 * cyclic 1:6:1 filter softens pulse/saw edges without adding per-frame work
 * or changing loop length. Each cycle is filtered independently for seamless
 * loop boundaries. */
#ifndef PS1_WAVEFORM_H
#define PS1_WAVEFORM_H
#include <stdint.h>
#include <string.h>
#define PS1_WAVE_MAX_CYCLE 112
static void ps1_wave_soften_cycle(int16_t *samples, int length)
{
    int16_t original[PS1_WAVE_MAX_CYCLE];
    int i;
    if (!samples || length < 3 || length > PS1_WAVE_MAX_CYCLE) return;
    memcpy(original, samples, (size_t)length * sizeof(original[0]));
    for (i = 0; i < length; i++) {
        int prev = original[(i + length - 1) % length];
        int cur = original[i];
        int next = original[(i + 1) % length];
        samples[i] = (int16_t)((prev + 6 * cur + next) / 8);
    }
}
#endif
