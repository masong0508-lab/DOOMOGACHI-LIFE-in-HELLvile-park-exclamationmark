/* adpcm.c - SPU ADPCM encoder (owner: ps1). Plain C, no PS1 headers, so
 * the host test (harness/adpcm_test.c) can verify it bit-exactly against a
 * reference decoder. */
#include <string.h>
#include "adpcm.h"

/* ---------------------------------------------------------------------------
 * ADPCM encoder. Each 16-byte block = header (shift | filter << 4), flags,
 * 14 bytes of 4-bit residuals (28 samples). Decoder:
 *   s = (nibble << 12 >> shift) + (old * f0 + older * f1 + 32) / 64
 * The encoder tries every filter (0..4) and shift (0..12) per block, with
 * proper error feedback through the decoder state, and keeps the best.
 * ------------------------------------------------------------------------- */
static const int k_f0[5] = { 0, 60, 115, 98, 122 };
static const int k_f1[5] = { 0, 0, -52, -55, -60 };

typedef struct AdpcmState { int old, older; } AdpcmState;

static int clamp16(int v) { return v < -32768 ? -32768 : v > 32767 ? 32767 : v; }

/* encode one block with a fixed filter/shift; returns squared error */
static uint32_t enc_try(const int16_t *in, int filter, int shift, AdpcmState *st,
                        uint8_t *nib, uint32_t limit)
{
    int i, old = st->old, older = st->older;
    uint32_t err = 0;
    for (i = 0; i < 28; i++) {
        int pred = (old * k_f0[filter] + older * k_f1[filter] + 32) >> 6;
        int diff = in[i] - pred;
        /* quantise: diff ~ (n << 12) >> shift  ->  n = diff << shift >> 12 */
        int n = ((diff << shift) + (diff >= 0 ? 2048 : -2048)) >> 12;
        int dec, e;
        if (n > 7) n = 7;
        if (n < -8) n = -8;
        dec = clamp16(((n << 12) >> shift) + pred);
        e = in[i] - dec;
        if (e < 0) e = -e;
        err += (uint32_t)e * (uint32_t)e >> 4;
        if (err > limit)
            return 0xffffffffu;
        nib[i] = (uint8_t)(n & 15);
        older = old;
        old = dec;
    }
    st->old = old;
    st->older = older;
    return err;
}

/* Encode `n` samples into ADPCM blocks. Flags: loop markers set by
 * caller. Returns blocks written. `fast` restricts the search. */
/* coarsest shift whose quantiser range (7 << 12 >> shift) covers r */
static int shift_for(int r)
{
    int s = 12;
    while (s > 0 && ((7 << 12) >> s) < r)
        s--;
    return s;
}

int adpcm_encode(const int16_t *pcm, int n, uint8_t *out, int fast)
{
    AdpcmState st = { 0, 0 };
    int blocks = (n + 27) / 28, b, i;
    int16_t blk[28];
    uint8_t nib[28], best_nib[28];
    for (b = 0; b < blocks; b++) {
        int best_f = 0, best_s = 0, f, s;
        uint32_t best_err = 0xffffffffu;
        AdpcmState best_st = st;
        for (i = 0; i < 28; i++) {
            int idx = b * 28 + i;
            blk[i] = idx < n ? pcm[idx] : 0;
        }
        /* only filter 0 for the first block so loops restart cleanly */
        for (f = 0; f < (b == 0 ? 1 : 5); f++) {
            int s0 = 0, s1 = 12;
            if (fast) {
                /* predict the residual peak for this filter from the source
                 * (exact history for the first two samples) and only try
                 * the shifts around the one that just covers it */
                int old = st.old, older = st.older, peak = 0;
                for (i = 0; i < 28; i++) {
                    int r = blk[i] - ((old * k_f0[f] + older * k_f1[f] + 32) >> 6);
                    if (r < 0) r = -r;
                    if (r > peak) peak = r;
                    older = old;
                    old = blk[i];
                }
                s = shift_for(peak);
                s0 = s > 1 ? s - 1 : 0;
                s1 = s < 12 ? s + 1 : 12;
            }
            for (s = s0; s <= s1; s++) {
                AdpcmState t = st;
                uint32_t e = enc_try(blk, f, s, &t, nib, best_err);
                if (e < best_err) {
                    best_err = e;
                    best_f = f;
                    best_s = s;
                    best_st = t;
                    memcpy(best_nib, nib, 28);
                }
            }
        }
        st = best_st;
        out[0] = (uint8_t)(best_s | (best_f << 4));
        out[1] = 0;
        for (i = 0; i < 14; i++)
            out[2 + i] = (uint8_t)(best_nib[i * 2] | (best_nib[i * 2 + 1] << 4));
        out += 16;
    }
    return blocks;
}


int adpcm_decode(const uint8_t *in, int blocks, int16_t *pcm)
{
    int b, i, old = 0, older = 0, n = 0;
    for (b = 0; b < blocks; b++, in += 16) {
        int shift = in[0] & 15, filter = (in[0] >> 4) & 7;
        if (filter > 4) filter = 0;
        for (i = 0; i < 28; i++) {
            int nib = (in[2 + i / 2] >> ((i & 1) * 4)) & 15;
            int s = (int16_t)(nib << 12) >> shift;
            s += (old * k_f0[filter] + older * k_f1[filter] + 32) >> 6;
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            pcm[n++] = (int16_t)s;
            older = old;
            old = s;
        }
    }
    return n;
}
