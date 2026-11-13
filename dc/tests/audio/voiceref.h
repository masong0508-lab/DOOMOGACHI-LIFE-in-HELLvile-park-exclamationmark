/* voiceref.h - the DoomoVoice-level reference renderer (voiceref.c) */
#ifndef DC_VOICEREF_H
#define DC_VOICEREF_H
#include <stdint.h>
#include "doomo/mus_data.h"
void     ref_reset(void);
void     ref_voice(int v, const DoomoVoice *r);   /* from the next frame on */
void     ref_bus(int gain);                       /* 255 = full */
void     ref_tape(int down16);
void     ref_render(int16_t *out, int frames);    /* stereo, L R */
uint32_t ref_pitch_step(int p16);
#endif
