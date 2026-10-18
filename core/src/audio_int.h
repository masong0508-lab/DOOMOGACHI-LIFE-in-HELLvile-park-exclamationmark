/* audio_int.h - internals shared by audio.c (SFX, babble), audio_seq.c (the
 * sequencer) and songs.c (the song book) (owner: story; the audio.c /
 * audio_seq.c parts: MU1, music SPEC CF-30).
 *
 * THE MML DIALECT (one const string per channel, see songs.c):
 *   t<bpm>        tempo (global; put it at the start of channel 0)
 *   o<n> < >      octave 0..8, octave down / up
 *   l<n>[.]       default note length (1 whole, 2, 4, 8, 16, 32, 3/6/12/24
 *                 for triplets); dots add half, then a quarter ...
 *   c d e f g a b note, then +/# sharp or - flat, then optional length and
 *                 dots, then any number of ^<len> ties (no re-attack)
 *   r[len]        rest
 *   K S H O C T P drums: kick, snare, closed hat, open hat, crash, tom,
 *                 clap (with optional length) - use them on channel 3
 *   v<n>          channel volume 0..15
 *   @<n>          instrument (audio_insts[])
 *   q<n>          gate 1..8 (eighths of the note before release; 0 = the
 *                 instrument's own)
 *   k<n> k-<n>    transpose in semitones
 *   [ ... ]<n>    repeat n times (nesting up to 4)
 *   L             loop point: when the channel runs out it jumps here
 *   spaces, '|' and newlines are ignored (bar lines for readability)
 * Durations are counted in 1/192 of a whole note; all channels of a song
 * must have the same total length (audio_song_check verifies it).
 */
#ifndef DOOMO_AUDIO_INT_H
#define DOOMO_AUDIO_INT_H

#include "doomo/doomo.h"

#define AUDIO_CH 4
#define AUDIO_WHOLE 192            /* duration units per whole note        */

enum { ASONG_LOOP = 0, ASONG_ONCE = 1 };

typedef struct AudioSong {
    const char *name;
    uint8_t     flags;             /* ASONG_* */
    const char *ch[AUDIO_CH];      /* MML per channel (NULL = silent)      */
} AudioSong;
extern const AudioSong audio_songs[SONG_COUNT];

/* Arpeggio tables (AudioInst.arp). */
enum { ARP_NONE, ARP_MAJOR, ARP_MINOR, ARP_OCTAVE, ARP_POWER, ARP_SUS4,
       ARP_DIM, ARP_MAJ7, ARP_TRILL, ARP_COUNT };

typedef struct AudioInst {
    uint8_t wave;        /* WAVE_*                                          */
    uint8_t attack;      /* ticks to reach full level (0 = instant)         */
    uint8_t decay;       /* level drop per tick toward sustain, 1/16 steps  */
    uint8_t sustain;     /* 0..15                                           */
    uint8_t release;     /* level drop per tick after the gate, 1/16 steps  */
    uint8_t vib_depth;   /* vibrato depth in 1/16 semitones                 */
    uint8_t vib_speed;   /* vibrato phase step per tick (256 = one cycle)   */
    uint8_t vib_delay;   /* ticks before the vibrato starts                 */
    uint8_t arp;         /* ARP_*                                           */
    int8_t  sweep;       /* pitch change per tick in 1/16 semitones         */
    uint8_t gate;        /* 1..8 eighths of the note held before release    */
    uint8_t level;       /* peak level 0..15                                */
} AudioInst;
extern const AudioInst audio_insts[];
extern const int audio_inst_count;

/* --- S1: comic-timing hooks (audio_fx.c <-> audio.c; SPEC sec.6.3) --- */
/* audio_fx.c, called by audio.c */
void audio_fx_tick(int on_quarter);        /* once per audio_tick, first    */
void audio_fx_music(int c, uint16_t *freq, int *vol);  /* the mixer hunk    */
int  audio_fx_owns(int c);                 /* channel borrowed by a one-shot */
void audio_fx_song_started(void);
void audio_fx_synth_step(void);
void audio_fx_reset(void);                 /* audio_init                     */
/* audio.c, the S1 hunk, called by audio_fx.c */
uint16_t audio_int_hz(int32_t p16);        /* 1/16 semitones above C0 -> Hz  */
void audio_int_release(int c);             /* give a channel back to music  */
void audio_int_dual(int song_a, int song_b);
/* tests and tools */
int  audio_sfx_synth_frames(int sfx);
const char *audio_sfx_synth_name(int sfx);
int  audio_sfx_synth_busy(void);
int  audio_fx_music_gated(void);           /* 1 while a mute holds the music */
/* --- end S1 --- */

/* --- AUD-SPLIT (MU1; music SPEC D13): audio.c <-> audio_seq.c --- */
/* audio.c: the tone-channel cache and init state */
void audio_int_send(int c, uint16_t f, uint8_t v, uint8_t w);  /* plat_tone if it changes */
void audio_int_unsend(int c);              /* forget channel c's cached state */
int  audio_int_inited(void);
/* audio_seq.c: the sequencer */
void audio_seq_init(void);                 /* audio_init, before audio_fx_reset */
void audio_seq_reset(void);                /* audio_init, after audio_fx_reset: silence */
void audio_seq_tick(void);                 /* audio_tick, after the voice babble */
int  audio_seq_song(void);                 /* the song playing now (SONG_NONE: none) */
/* audio_seq.c: the classic interpreter as T1's reference (MU1 M1; native
 * tests only, never the game; M5 deletes it). Pure: no gains, no plat_tone. */
void audio_legacy_start(int song);
void audio_legacy_dual(int song_a, int song_b);
void audio_legacy_tick(void);
void audio_legacy_state(int c, DoomoToneState *out);
uint32_t audio_legacy_notes(void);
/* --- end AUD-SPLIT --- */

#endif
