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


/* ---------------------------------------------------------------------------
 * Loop encoder. The SPU decoder keeps its two history samples across a loop jump, so a loop
 * block is decoded from the state the END of the loop leaves, not from the state the encoder
 * assumed (adpcm_encode starts every sample at zero). When the two differ the loop decodes off,
 * and the error repeats every cycle: measured on the 28-sample tone loops (one block) as 3 to
 * 16 dB signal-to-noise where a straight decode of the same bytes showed 20+.
 * adpcm_encode_loop makes several encodings of the cycle (each from the state the previous one
 * ended in, one from silence, and a filter-0 floor), plays each one round the loop with the real
 * decoder until its entry state settles, and keeps the one that sounds closest to the source.
 * ------------------------------------------------------------------------- */
#define LOOP_PASSES 8      /* encodings tried per starting state */
#define LOOP_SETTLE 12     /* decoder passes allowed for the entry state to settle */

static void pack_block(uint8_t *out, int filter, int shift, const uint8_t *nib)
{
    int i;
    out[0] = (uint8_t)(shift | (filter << 4));
    out[1] = 0;
    for (i = 0; i < 14; i++)
        out[2 + i] = (uint8_t)(nib[i * 2] | (nib[i * 2 + 1] << 4));
}

static void dec_block(const uint8_t *in, AdpcmState *st, int16_t *pcm)
{
    int shift = in[0] & 15, filter = (in[0] >> 4) & 7, i;
    if (filter > 4) filter = 0;
    for (i = 0; i < 28; i++) {
        int nib = (in[2 + i / 2] >> ((i & 1) * 4)) & 15;
        int s = (int16_t)(nib << 12) >> shift;
        s += (st->old * k_f0[filter] + st->older * k_f1[filter] + 32) >> 6;
        pcm[i] = (int16_t)clamp16(s);
        st->older = st->old;
        st->old = pcm[i];
    }
}

/* squared error (the scale of enc_try) of the loop `in` as the SPU plays it once its entry state
 * has settled: the state is carried round the loop until it repeats (or LOOP_SETTLE passes) */
static uint32_t loop_error(const uint8_t *in, int blocks, const int16_t *src)
{
    AdpcmState st = { 0, 0 }, e;
    int16_t tmp[28];
    int pass, b, i;
    for (pass = 0; ; pass++) {
        uint32_t err = 0;
        e = st;
        for (b = 0; b < blocks; b++) {
            dec_block(in + b * 16, &e, tmp);
            for (i = 0; i < 28; i++) {
                int d = src[b * 28 + i] - tmp[i];
                err += (uint32_t)(d * d) >> 4;
                if (err > 0x7fffffffu) err = 0x7fffffffu;     /* saturate: never wrap into a "good" score */
            }
        }
        if ((e.old == st.old && e.older == st.older) || pass >= LOOP_SETTLE)
            return err;
        st = e;
    }
}

/* best filter / shift for one block from state *st, trying the first `filters` filters; advances
 * *st and writes the 16 bytes (flags 0) */
static void pick_block(const int16_t *blk, int filters, AdpcmState *st, uint8_t *out)
{
    int f, s, best_f = 0, best_s = 0;
    uint32_t best_err = 0xffffffffu;
    AdpcmState best_st = *st;
    uint8_t nib[28], best_nib[28];
    memset(best_nib, 0, sizeof best_nib);
    for (f = 0; f < filters; f++) {
        for (s = 0; s <= 12; s++) {
            AdpcmState t = *st;
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
    *st = best_st;
    pack_block(out, best_f, best_s, best_nib);
}

/* a one-block loop: for every filter and shift, encode from the state the block itself leaves
 * (repeat until that state is its own start), then keep the combination the decoder plays best */
static void encode_self_loop(const int16_t *blk, uint8_t *out)
{
    int f, s, pass;
    uint32_t best = 0xffffffffu;
    uint8_t nib[28], tmp[16];
    for (f = 0; f < 5; f++) {
        /* only shifts around the one that just covers the residual peak (as the fast search) */
        int old = blk[27], older = blk[26], peak = 0, i, s0, s1;
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
        for (s = s0; s <= s1; s++) {
            AdpcmState st, t;
            uint32_t e;
            st.old = blk[27];                    /* start where the signal itself would be */
            st.older = blk[26];
            for (pass = 0; pass < LOOP_PASSES; pass++) {
                t = st;
                enc_try(blk, f, s, &t, nib, 0xffffffffu);
                if (t.old == st.old && t.older == st.older)
                    break;
                st = t;
            }
            pack_block(tmp, f, s, nib);
            e = loop_error(tmp, 1, blk);
            if (e < best) {
                best = e;
                memcpy(out, tmp, 16);
            }
        }
    }
}

int adpcm_encode_loop(const int16_t *pcm, int n, uint8_t *out)
{
    int blocks = n / 28, b, pass, seed;
    uint32_t best = 0xffffffffu;
    uint8_t tmp[ADPCM_LOOP_MAX_BLOCKS * 16];
    if (n < 28 || n % 28 || blocks > ADPCM_LOOP_MAX_BLOCKS)
        return 0;
    if (blocks == 1) {
        encode_self_loop(pcm, out);
        return 1;
    }
    /* seeds: where the signal itself would be at the loop start, then silence */
    for (seed = 0; seed < 2; seed++) {
        AdpcmState st;
        st.old = seed ? 0 : pcm[n - 1];
        st.older = seed ? 0 : pcm[n - 2];
        for (pass = 0; pass < LOOP_PASSES; pass++) {
            AdpcmState cur = st;
            uint32_t e;
            for (b = 0; b < blocks; b++)
                pick_block(pcm + b * 28, 5, &cur, tmp + b * 16);
            e = loop_error(tmp, blocks, pcm);
            if (e < best) {
                best = e;
                memcpy(out, tmp, (size_t)blocks * 16);
            }
            if (cur.old == st.old && cur.older == st.older)
                break;                          /* closed: every pass decodes the same */
            st = cur;
        }
    }
    /* the floor: filter 0 has no history, so it always settles */
    {
        AdpcmState z = { 0, 0 };
        for (b = 0; b < blocks; b++)
            pick_block(pcm + b * 28, 1, &z, tmp + b * 16);
        if (loop_error(tmp, blocks, pcm) < best)
            memcpy(out, tmp, (size_t)blocks * 16);
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
