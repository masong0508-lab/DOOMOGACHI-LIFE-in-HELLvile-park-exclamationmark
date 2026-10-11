/* audio.h - chiptune sequencer, songs, procedural SFX and Tomodachi-style
 * voice babble (owner: story). The sequencer runs at 30 Hz inside
 * doomo_frame() and drives plat_tone(); SFX go through plat_sample(). */
#ifndef DOOMO_AUDIO_H
#define DOOMO_AUDIO_H

#include "types.h"

enum { SONG_NONE, SONG_TITLE, SONG_HAHALAND_DAY, SONG_HAHALAND_NIGHT,
       SONG_CREATOR, SONG_SHOP, SONG_CHURCH, SONG_PET, SONG_HELLVILE,
       SONG_EXORCISM, SONG_WORKSHOP, SONG_INVASION, SONG_HELL_BREACH,
       SONG_BOSS, SONG_VICTORY, SONG_SAD, SONG_COUNT };

enum { SFX_NONE, SFX_CURSOR, SFX_SELECT, SFX_CANCEL, SFX_ERROR, SFX_BLIP,
       SFX_EAT, SFX_HAPPY, SFX_SAD, SFX_LOVE, SFX_SPARKLE, SFX_SPLASH,
       SFX_POOP, SFX_EGG_CRACK, SFX_EVOLVE, SFX_PISTOL, SFX_SHOTGUN,
       SFX_CHAINGUN, SFX_ROCKET, SFX_PLASMA, SFX_BUBBLE, SFX_SQUEAK,
       SFX_CHAINSAW, SFX_EXPLODE, SFX_SPLAT, SFX_CONFETTI, SFX_DEMON_ROAR,
       SFX_DEMON_PAIN, SFX_DEMON_DIE, SFX_PLAYER_HURT, SFX_PICKUP,
       SFX_DOOR, SFX_WELD, SFX_HOLY, SFX_POSSESS, SFX_BELL, SFX_COUNT };

void audio_init(void);
void audio_tick(void);                 /* called by doomo_frame()      */
void audio_music(int song);            /* restarts only if different   */
void audio_sfx(int sfx);
/* PCM for a sample: signed 8-bit mono at *rate Hz. Generated at boot. */
const int8_t *audio_sfx_pcm(int sfx, uint32_t *len, uint32_t *rate);
/* Speak `text` with a resident's voice (voice[] sliders): one blip per
 * syllable-ish character while dialogue types out. */
void audio_voice_char(const uint8_t *voice, char c);

#endif
