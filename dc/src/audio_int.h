/* audio_int.h - the Dreamcast sound mixer (DC5; owner: dreamcast).
 *
 * The core speaks to the platform through plat_tone() (four chiptune
 * channels; the music engine's legacy bridge and the voice babble) and
 * plat_sample() / plat_sample_ex() (one-shot effects). On the Dreamcast
 * those calls only change the mixer's "current state" on the main thread;
 * at the end of every logic tick dcsnd_push() applies that state and
 * renders exactly DCSND_PER_TICK output frames (1/30 s at 44,100 Hz) in
 * software into the next slot of a PCM ring. The KOS sound stream
 * (audio.c, its own thread) calls dcsnd_render(), which copies slots out
 * to the AICA. So:
 *
 *   - the sound is a pure function of the game's ticks: the same ticks
 *     give the same samples, whatever the stream's chunk sizes, and a gap
 *     (the game late, a load, a harness hold) is silence that leaves the
 *     synth state untouched (the "content" stream continues exactly);
 *   - the synth's CPU time is part of the tick (main thread), not of the
 *     stream thread, whose refill must never be late (mixer.c says why);
 *   - the latency is DCSND_PREFILL ticks of slack plus the stream buffer
 *     (docs/DREAMCAST.md section 6);
 *   - tones and triggers take effect at the start of their tick's frames,
 *     as in the native runner's musrender (core/native/plat_music_native.c).
 *
 * Everything here is portable C99 (no KOS): mixer.c and sfx.c also build on
 * the host, where dc/tests/audio checks them against the native runner.
 * One producer (the main thread) and one consumer (the stream thread): the
 * ring counters are written by one side each.
 */
#ifndef DC_AUDIO_INT_H
#define DC_AUDIO_INT_H

#include <stdint.h>

#define DCSND_RATE      44100            /* output frames per second           */
#define DCSND_PER_TICK  1470             /* = DCSND_RATE / 30: one logic tick  */
#define DCSND_TONES     4                /* plat_tone channels                 */
#define DCSND_VOICES    8                /* sample voices (plat_sample)        */
#define DCSND_TRIG      8                /* sample triggers kept per tick      */
#define DCSND_QUEUE     16               /* PCM slots (power of two; 94 KB)    */
#define DCSND_PREFILL   2                /* slots ready before playing         */
#define DCSND_MAXLAG    4                /* more ready at a slot start: skip   */

typedef struct DcSndTone { uint16_t freq; uint8_t vol, wave; } DcSndTone;   /* plat_tone */
typedef struct DcSndTrig {
    uint8_t sfx, vol, prio;
    int8_t  pan, semis;                  /* -64 left .. 64 right; semitones    */
    uint8_t pad[3];
} DcSndTrig;
typedef struct DcSndFrame {               /* one logic tick                     */
    DcSndTone tone[DCSND_TONES];
    uint8_t   ntrig, pad[3];
    DcSndTrig trig[DCSND_TRIG];
} DcSndFrame;

/* ---- the SFX bank (sfx.c) ---------------------------------------------------
 * dc_sfx_init() copies every effect of audio_sfx_pcm() at boot into 16-bit
 * PCM with one guard sample (for interpolation), so nothing reads the core's
 * PCM later. step is the playback rate in output frames, Q16. */
typedef struct DcSfx { const int16_t *pcm; uint32_t len, step_q16; } DcSfx;
int          dc_sfx_init(void);           /* -> the number of effects loaded    */
const DcSfx *dc_sfx_get(int sfx);         /* NULL: none / empty                 */
uint32_t     dc_sfx_bytes(void);          /* RAM the bank uses                  */

/* ---- the mixer (mixer.c) ------------------------------------------------------ */
void dcsnd_reset(void);                   /* everything silent, ring empty      */

/* producer (main thread) */
void dcsnd_tone(int ch, uint16_t freq_hz, uint8_t vol, uint8_t wave);
void dcsnd_trigger(int sfx, uint8_t vol, int8_t pan, uint8_t prio, int8_t semis);
/* end of a tick: render it into the next slot; 0: the ring is full (the
 * state and the triggers wait for the next push, nothing is rendered) */
int  dcsnd_push(void);
/* 1 = a known pause (a load, a VMU write, a harness hold): an empty ring
 * then is not an underrun. It lasts until the next dcsnd_push(); 0 is a
 * no-op (the stream may reach the empty ring after the pause ended). */
void dcsnd_stall(int on);

/* consumer (the stream thread): `frames` stereo frames, L R interleaved,
 * copied from the ring (silence in a gap) */
void dcsnd_render(int16_t *out, int frames);

typedef struct DcSndStats {
    uint32_t pushed, consumed;            /* slots rendered / begun playing     */
    uint32_t underruns;                   /* empty ring while playing           */
    uint32_t overruns, dropped;           /* push into a full ring; lag skips   */
    uint32_t queue_max;                   /* fullest ring seen (slots)          */
    uint32_t frames, gap_frames;          /* output frames: content / silence   */
    uint32_t clips;                       /* samples over the rail (softened)   */
    uint32_t started, steals;             /* sample voices started / stolen     */
    uint32_t voice_frames;                /* frames rendered by sample voices   */
    uint32_t hash;                        /* FNV-1a of the rendered stream      */
    uint32_t hash600;                     /* the same after 600 slots (20 s)    */
} DcSndStats;
extern volatile DcSndStats dcsnd_stats;
int dcsnd_queued(void);                   /* slots not finished (incl. playing) */
int dcsnd_voices_on(void);                /* sample voices sounding (producer)  */

#endif
