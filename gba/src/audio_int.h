/* audio_int.h - DirectSound mixer shared between audio.c and the
 * IWRAM mixer (owner: gba). */
#ifndef GBA_AUDIO_INT_H
#define GBA_AUDIO_INT_H

#include "gba_plat.h"

#define MIX_RATE    10512u        /* Hz: 16777216 / 1596, 176 per frame */
#define MIX_SAMPLES 176           /* per vblank (multiple of 16)        */
#define MIX_VOICES  4

typedef struct MixVoice {
    const int8_t *pcm;
    uint32_t pos, step, len;      /* 20.12 sample positions             */
    uint32_t started;             /* vblank count at start (stealing)   */
    uint8_t  vol;                 /* 0..15                              */
    volatile uint8_t active;
    uint8_t  pad[2];
} MixVoice;

extern MixVoice mix_voice[MIX_VOICES];
void mixer_init(void) IWRAM_CALL;

#endif
