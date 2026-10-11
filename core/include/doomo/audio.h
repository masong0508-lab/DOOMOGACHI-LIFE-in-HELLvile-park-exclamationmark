/* audio.h - chiptune sequencer, songs, procedural SFX and Tomodachi-style
 * voice babble (owner: story). The sequencer runs at 30 Hz inside
 * doomo_frame() and drives plat_tone(); SFX go through plat_sample(). */
#ifndef DOOMO_AUDIO_H
#define DOOMO_AUDIO_H

#include "types.h"

enum { SONG_NONE, SONG_TITLE, SONG_HAHALAND_DAY, SONG_HAHALAND_NIGHT,
       SONG_CREATOR, SONG_SHOP, SONG_CHURCH, SONG_PET, SONG_HELLVILE,
       SONG_EXORCISM, SONG_WORKSHOP, SONG_INVASION, SONG_HELL_BREACH,
       SONG_BOSS, SONG_VICTORY, SONG_SAD,
       /* ---- story additions (append only) ---- */
       SONG_FESTIVAL,       /* the united park: everyone together          */
       SONG_CREDITS,        /* the parade of all 1000 citizens             */
       SONG_VAPE_LOUNGE,    /* Cloud Nine Vapor: too smooth, too purple    */
       SONG_OMINOUS,        /* the Wall, the fog, cutscenes going wrong    */
       SONG_COUNT };

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

/* ======================================================================
 * story additions (owner: story; additive)
 * ======================================================================
 *
 * SEQUENCER. Songs are written in a small MML dialect (songs.c), one
 * string per channel, interpreted incrementally at 30 Hz (a few bytes per
 * tick, bounded). Channels: 0 lead pulse, 1 harmony pulse (also the voice
 * babble channel), 2 bass (triangle / saw / pulse wave channel), 3 noise
 * drums. Timing is kept in 1/192 whole notes with a fixed-point tick
 * accumulator, so all channels stay sample-exact in sync at any tempo.
 * Instruments give every note a volume envelope (attack, decay, sustain,
 * release, gate), vibrato (delayed), arpeggio tables and pitch sweeps
 * (kicks, lasers). Final volume = envelope x channel volume x
 * OPT_MUSIC_VOL (0 mutes the music completely).
 *
 * audio_music(SONG_NONE) stops the music and silences channels 0..3 once;
 * after that the sequencer leaves plat_tone() alone (except for voice
 * babble on channel 1), so another player (the Concert Hall / Music
 * Studio, song.h) may drive the tone channels itself. */

/* Current background song (SONG_*). */
int  audio_music_current(void);
/* Play `song` once (a stinger: victory, sad, egg hatch) and then resume
 * the song that was playing before. */
void audio_music_once(int song);
/* Display name ("HaHaland Day") and validity of a song (jukebox, tests). */
const char *audio_song_name(int song);
/* Length of one pass of a song in ticks (all channels agree; tests). */
uint32_t audio_song_length(int song);
/* Validate a song's MML (every channel parses, lengths agree). Returns 1
 * if OK; on failure *err (if given) points at a short reason. */
int  audio_song_check(int song, const char **err);
/* Ticks since the current song started (beat-synced visuals). */
uint32_t audio_music_ticks(void);

/* What the sequencer last sent to a tone channel (tests, visualisers). */
typedef struct DoomoToneState {
    uint16_t freq;     /* Hz, 0 = silent */
    uint8_t  vol;      /* 0..15 */
    uint8_t  wave;     /* WAVE_* */
} DoomoToneState;
void audio_channel_state(int ch, DoomoToneState *out);
/* Number of notes started since the current song began (tests). */
uint32_t audio_note_count(void);

/* SFX: generated at build time by tools/sfxgen.py into const tables
 * (core/gen/sfx_data.c - ROM on GBA, no RAM cost). Volume follows
 * OPT_SFX_VOL (0 = muted). */
#define AUDIO_SFX_BUDGET (48u * 1024u)    /* total PCM bytes, all SFX     */
uint32_t audio_sfx_total_bytes(void);
const char *audio_sfx_name(int sfx);

/* VOICE BABBLE. Each call may start a short blip on tone channel 1
 * (vowels and the first letter after a pause always do, consonants
 * modulate the running blip). The five voice sliders shape it:
 *   PITCH       base frequency (deep ~ 110 Hz .. squeaky ~ 900 Hz)
 *   SPEED       blip length (fast voices make short blips)
 *   QUALITY     timbre: thin 12.5% pulse .. round 50% / triangle
 *   INTONATION  how much each syllable's pitch wanders (monotone .. sing-song)
 *   ACCENT      per-vowel / per-letter pitch shape and a final rise or fall
 * Respects OPT_VOICE_VOL and OPT_VOICE_BABBLE. While a blip sounds, the
 * music on channel 1 is replaced; it comes back by itself. */
int  audio_voice_busy(void);           /* 1 while a blip is sounding     */
#define AUDIO_VOICE_CHANNEL 1
/* The tone (Hz) a voice would use for character c (tests, previews). */
uint16_t audio_voice_freq(const uint8_t *voice, char c);

#endif
