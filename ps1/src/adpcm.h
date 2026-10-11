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

/* Reference decoder (host tests): decodes `blocks` blocks into pcm,
 * ignoring loop flags. Returns samples written. */
int adpcm_decode(const uint8_t *in, int blocks, int16_t *pcm);

#endif
