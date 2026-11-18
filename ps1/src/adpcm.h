/* adpcm.h - SPU ADPCM encoder (owner: ps1). */
#ifndef PS1_ADPCM_H
#define PS1_ADPCM_H

#include <stdint.h>

/* Encode n 16-bit samples into SPU ADPCM blocks (16 bytes per 28 samples,
 * the last block zero padded). Block flags are left 0: the caller sets
 * loop start / end / repeat. The first block always uses filter 0 so a
 * loop restarts cleanly. fast = search filters 0..1 with a pruned shift
 * range (for long sound effects); else every filter and shift is tried
 * with error feedback through the decoder. Returns the block count. */
int adpcm_encode(const int16_t *pcm, int n, uint8_t *out, int fast);

/* Encode one looping cycle (n a multiple of 28 samples, at most ADPCM_LOOP_MAX_BLOCKS blocks) so
 * that it decodes well on every pass. The SPU keeps two samples of decoder history across the loop
 * jump, so a loop is decoded from the state its own end leaves, not the zero state adpcm_encode
 * assumes. This makes several encodings, runs the SPU decoder round the loop until it settles and
 * keeps the one that sounds closest to pcm (a single block: every filter and shift).
 * Use it for the loop part of a sample, after a lead-in made with adpcm_encode. Block flags are
 * left 0. Returns the block count (0 if n does not fit). Boot-time only: it is many block encodes. */
#define ADPCM_LOOP_MAX_BLOCKS 8
int adpcm_encode_loop(const int16_t *pcm, int n, uint8_t *out);

/* Reference decoder (host tests): decodes `blocks` blocks into pcm,
 * ignoring loop flags. Returns samples written. */
int adpcm_decode(const uint8_t *in, int blocks, int16_t *pcm);

#endif
