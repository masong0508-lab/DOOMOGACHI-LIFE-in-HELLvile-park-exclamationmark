/* PS2-3 deterministic 30 Hz four-voice tone + small PCM SFX renderer.
 * C99, fixed memory, no PS2SDK headers: test this on the host before
 * feeding 16-bit stereo frames to audsrv/SPU2.
 */
#ifndef DOOMO_PS2_SYNTH_H
#define DOOMO_PS2_SYNTH_H
#include <stdint.h>
#define PS2_SOUND_RATE 22050u
#define PS2_SOUND_FRAMES 735u
#define PS2_SOUND_TONES 4
#define PS2_SOUND_SAMPLES 3

typedef struct Ps2Tone {
    uint32_t phase, step, noise;
    uint16_t hz;
    uint8_t volume, wave;
} Ps2Tone;
typedef struct Ps2Sample {
    const int8_t *pcm;
    uint32_t length, pos, step;
    uint8_t volume;
} Ps2Sample;
typedef struct Ps2Synth {
    Ps2Tone tone[PS2_SOUND_TONES];
    Ps2Sample sample[PS2_SOUND_SAMPLES];
    uint8_t steal;
} Ps2Synth;
void ps2_synth_reset(Ps2Synth *s);
void ps2_synth_tone(Ps2Synth *s,int ch,uint16_t hz,uint8_t volume,uint8_t wave);
void ps2_synth_sample(Ps2Synth *s,const int8_t *pcm,uint32_t n,uint32_t rate,uint8_t volume);
void ps2_synth_render(Ps2Synth *s,int16_t *stereo, unsigned frames);
#endif
