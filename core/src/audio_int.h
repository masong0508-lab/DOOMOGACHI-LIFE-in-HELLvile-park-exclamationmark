/* audio_int.h - internals shared by audio.c (sequencer) and songs.c (the
 * song book) (owner: story).
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

#endif
