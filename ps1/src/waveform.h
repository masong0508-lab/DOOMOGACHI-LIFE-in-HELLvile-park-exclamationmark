/* waveform.h - boot-time synthesis of the PS1 tone loops (spu.c build_waves).
 * Plain C, integer only, no PS1 headers, so ps1/harness/adpcm_test.c checks the very code the
 * console runs.
 *
 * Tone loops are built once at boot, encoded to SPU ADPCM and replayed by the SPU hardware, so
 * nothing here costs anything per frame. What this file does (2026-11-18, replaces the old
 * "draw hard edges, then soften them" step):
 *
 *  1. Pulse and saw cycles are band-limited sums of harmonics (a sine series, one table of 112
 *     points), not hard edges sampled at whole samples. Every harmonic the cycle can hold is
 *     there and nothing folds back into the loop. The duty is exact: the 12.5 % pulse on the
 *     28-sample cycle is 3.5 samples wide, the old edge was 3 (10.7 %).
 *  2. The 112-sample cycle keeps the response of the old 1:6:1 edge softener,
 *     (6 + 2 cos(2 pi k / len)) / 8, per harmonic, so the tone colour stays what it was; only the
 *     edges are now at the right place. The 28-sample cycle (notes above ~700 Hz, whose upper
 *     harmonics the SPU's playback folds back into the note: spu.c) rolls its harmonics 4 to 13
 *     off to zero with a raised cosine instead: 5 to 10 dB less of that fold-back at 1.5 to 3 kHz
 *     (docs/PS1.md "Tone loop synthesis").
 *  3. No DC offset. The old 12.5 % pulse sat 9000 below zero (0.27 of full scale): inaudible while
 *     sounding, but it is a step at key-on and key-off and eats ADPCM range. The size of the
 *     audible wave (the part that moves) is the same as before.
 *  4. The first 28 samples of the first cycle fade in (raised cosine), so a key-on starts at zero
 *     instead of a full-scale step (a click). The loop restarts after that and never fades.
 *  5. The loop cycle is encoded for the way the SPU really plays it (adpcm_encode_loop): the decoder
 *     carries two samples of history across the loop jump. The old two-cycle encode assumed the
 *     history of the first cycle; on the 28-sample loops they differ and the loop decoded 3 to
 *     16 dB signal-to-noise (adpcm.c).
 * The triangle (no edges, nothing to fold) is the same wave as before; it gets the fade and the
 * closed loop too. */
#ifndef PS1_WAVEFORM_H
#define PS1_WAVEFORM_H
#include <stdint.h>
#include <string.h>
#include "adpcm.h"

#define PS1_WAVE_MAX_CYCLE 112      /* longest cycle: the 112-sample table below, 4 ADPCM blocks */
#define PS1_WAVE_AMP       12000    /* peak-to-peak / 2 of the old pulse and saw (14000 peak for the triangle) */
#define PS1_WAVE_FADE      28       /* key-on fade: the first ADPCM block of the first cycle */

/* sin(2 pi i / 112), Q15, one quarter wave; 112 = 4 x 28 so both cycle lengths (112 and 28) and the
 * pulse duties 1/8, 1/4, 1/2 land on whole table steps */
static const int16_t ps1_wave_qsin[29] = {
    0, 1837, 3669, 5489, 7292, 9071, 10823, 12540, 14218, 15851, 17434, 18962, 20431, 21835, 23170,
    24433, 25619, 26724, 27745, 28679, 29523, 30274, 30929, 31487, 31946, 32305, 32562, 32716, 32767
};

/* response of harmonics 1..13 of the 28-sample cycle, Q12: flat to 3, then a raised cosine to
 * zero at 14 (0.5 (1 + cos(pi (k - 3.5) / 10.5))) */
static const int16_t ps1_wave_short_resp[13] = {
    4096, 4096, 4096, 4073, 3893, 3549, 3072, 2504, 1895, 1300, 771, 356, 91
};

/* sin of idx / 112 of a turn, idx 0..111, Q15 (no division: the synthesis calls it thousands of
 * times at boot) */
static int ps1_wave_sin112(int idx)
{
    if (idx < 28)  return ps1_wave_qsin[idx];
    if (idx < 56)  return ps1_wave_qsin[56 - idx];
    if (idx < 84)  return -ps1_wave_qsin[idx - 56];
    return -ps1_wave_qsin[112 - idx];
}

/* the same for any int idx */
static int ps1_wave_sin(int idx)
{
    idx %= 112;
    return ps1_wave_sin112(idx < 0 ? idx + 112 : idx);
}

/* One cycle of len samples (len = 112 or 28).
 * duty112 = 0: rising saw, -A .. +A. Else a pulse that is high for duty112/112 of the cycle,
 * high level +2A (1 - duty), low level -2A duty: peak-to-peak 2A, mean 0.
 * Series: saw = -(2/pi) sum sin(k t) / k; pulse = (2/pi) sum (sin(k t) - sin(k (t - d))) / k.
 * The phase of every harmonic is stepped, not multiplied out, so the inner loop is table reads
 * and adds (the R3000 divides slowly and this runs 10 times at boot). */
static void ps1_wave_series(int16_t *out, int len, int duty112)
{
    int g[PS1_WAVE_MAX_CYCLE / 2], ph[PS1_WAVE_MAX_CYCLE / 2], back[PS1_WAVE_MAX_CYCLE / 2];
    int half = len / 2, step = 112 / len, i, k;
    /* g[k] = 2 / (pi k) x the response at harmonic k, Q12 (2/pi = 2608 / 4096) */
    for (k = 1; k < half; k++) {
        if (len == 112) {
            int c = ps1_wave_sin(k * step + 28);                   /* cos(2 pi k / len) */
            g[k] = (2608 * (196608 + 2 * c) + 131072 * k) / (262144 * k);
        } else {
            g[k] = (2608 * ps1_wave_short_resp[k - 1] + 2048 * k) / (4096 * k);
        }
        ph[k] = 0;
        back[k] = (112 - (k * duty112) % 112) % 112;               /* phase of t - d */
    }
    for (i = 0; i < len; i++) {
        int sum = 0;
        for (k = 1; k < half; k++) {
            int term;
            if (duty112)
                term = g[k] * (ps1_wave_sin112(ph[k]) - ps1_wave_sin112(back[k]));
            else
                term = -g[k] * ps1_wave_sin112(ph[k]);
            sum += (term + 2048) >> 12;                            /* Q15 of full scale 1.0 */
            ph[k] += k * step;
            if (ph[k] >= 112) ph[k] -= 112;
            back[k] += k * step;
            if (back[k] >= 112) back[k] -= 112;
        }
        out[i] = (int16_t)((sum * PS1_WAVE_AMP + 16384) >> 15);
    }
}

/* duty_eighths 1, 2, 4 = 12.5 %, 25 %, 50 % (a multiple of 112/8 = 14 table steps) */
static void ps1_wave_pulse(int16_t *out, int len, int duty_eighths)
{
    ps1_wave_series(out, len, duty_eighths * 14);
}

static void ps1_wave_saw(int16_t *out, int len)
{
    ps1_wave_series(out, len, 0);
}

/* the old triangle, unchanged: peak 14000 */
static void ps1_wave_triangle(int16_t *out, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        int ph = (i * 4 * 14000) / len;                            /* 0..56000 */
        out[i] = (int16_t)(ph < 14000 ? ph : ph < 42000 ? 28000 - ph : ph - 56000);
    }
}

/* raised-cosine key-on fade over the first PS1_WAVE_FADE samples: sample 0 is silence */
static void ps1_wave_fade_in(int16_t *pcm)
{
    int i;
    for (i = 0; i < PS1_WAVE_FADE; i++) {
        int w = (32768 - ps1_wave_sin112(2 * i + 28)) / 2;         /* (1 - cos(pi i / 28)) / 2, Q15 */
        pcm[i] = (int16_t)((pcm[i] * w) >> 15);
    }
}

/* Encode a tone loop. pcm holds one finished cycle at [0, len) and room for 2 * len samples; adpcm
 * has room for 2 * len / 28 blocks. len must be a multiple of 28. Layout in SPU RAM: [the first
 * cycle, faded in, encoded from silence][the loop cycle, encoded closed]. Loop start is the second
 * cycle, loop end + repeat its last block. The jump from the first cycle into the loop arrives with
 * a history that differs from the loop's by a quantiser step or so; it dies away within the cycle,
 * once, under the fade's tail. Returns the block count (0 for a bad len). */
static int ps1_wave_encode_loop(int16_t *pcm, int len, uint8_t *adpcm)
{
    int first;
    if (len < 28 || len % 28 || len > PS1_WAVE_MAX_CYCLE)
        return 0;
    first = len / 28;
    memcpy(pcm + len, pcm, (size_t)len * sizeof(pcm[0]));
    ps1_wave_fade_in(pcm);
    adpcm_encode(pcm, len, adpcm, 0);                              /* first block: filter 0 */
    if (adpcm_encode_loop(pcm + len, len, adpcm + first * 16) != first)
        return 0;
    adpcm[first * 16 + 1] |= 0x04;                                 /* loop start: cycle 2 */
    adpcm[(first * 2 - 1) * 16 + 1] |= 0x03;                       /* loop end + repeat */
    return first * 2;
}
#endif
