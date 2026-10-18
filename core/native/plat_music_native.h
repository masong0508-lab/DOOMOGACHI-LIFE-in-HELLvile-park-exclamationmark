/* plat_music_native.h - the native platform's music hooks (owner: MU1;
 * docs/design/music/SPEC.md sec.9.4, 16.2 "musrender", CF-46).
 * plat_native.c (foundation) keeps plat_tone/plat_sample and calls these
 * from one marked hunk; runner_main.c's `--wav FILE` turns recording on. */
#ifndef DOOMO_PLAT_MUSIC_NATIVE_H
#define DOOMO_PLAT_MUSIC_NATIVE_H

#include <stdint.h>

/* start writing a 22,050 Hz 16-bit stereo WAV of everything the game
 * plays (closed at exit). 0 = cannot open. */
int  nat_wav_open(const char *path);
void nat_wav_close(void);
/* plat_native.c: the tone channels and samples as they are set */
void nat_wav_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave);
void nat_wav_sample(int sfx, uint8_t vol);
/* plat_native.c: one 30 Hz frame ended (renders 735 samples) */
void nat_wav_frame(void);

#endif
