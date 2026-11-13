/* audio_int.h - the Dreamcast sound mixer (DC5; owner: dreamcast).
 *
 * The core speaks to the platform through plat_tone() (four chiptune
 * channels: the voice babble, the comedy one-shots, the Concert Hall; and
 * the music when it runs as the legacy bridge), plat_voice() (DC5b: the
 * music engine's voices on its IRQ tier, see below) and
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
#define DCSND_MVOICES   8                /* music voices (DoomoVoice records;
                                          * the core's driver has 4 today)     */
#define DCSND_MT        8                /* driver ticks (mt) per logic tick   */

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

/* ---- the music engine's IRQ tier (DC5b; musvoice.c makes these calls) ---------
 * With a driver set, dcsnd_push() calls it DCSND_MT times per slot, each
 * time right before it renders the frames of that mt (mt k of a slot starts
 * at frame k * DCSND_PER_TICK / DCSND_MT, rounded down: 0, 183, 367, 551,
 * 735, 918, 1102, 1286), so a record the driver sends from inside the call
 * (dcsnd_mvoice) is heard from exactly that frame. The mixer renders on the
 * main thread, the thread that runs music_update(), so nothing is locked.
 * A push into a full ring still runs the driver (the music keeps the game's
 * time, as the tone channels do) but renders nothing.
 * NULL (the default): no driver, the music arrives as plat_tone (the legacy
 * bridge). dcsnd_reset() leaves the hook as it is. */
void dcsnd_set_driver(void (*tick)(void));
/* one music voice, as the platform's renderer needs it */
enum { DCMW_PULSE, DCMW_TRI, DCMW_SAW, DCMW_NOISE15, DCMW_NOISE7 };
typedef struct DcSndMV {
    uint16_t p16;                        /* pitch, 1/16 semitones above C0      */
    uint16_t hz;                         /* noise: the LFSR's clock in Hz; 0 =
                                          * clocked at the pitch                */
    uint8_t  vol_l, vol_r;               /* 0..63 (63 = a tone at volume 15)    */
    uint8_t  wave;                       /* DCMW_*                              */
    uint8_t  duty;                       /* DCMW_PULSE: the duty in 1/32, 1..31 */
    uint8_t  keyon, pad[3];              /* a note starts with this record      */
} DcSndMV;
/* voice v (0..DCSND_MVOICES-1) from the current render position on. Voices
 * 0-3 sit where the tone channels do (lead and bass centre, harmony a
 * little left, kit a little right); vol_l / vol_r pan on top of that.
 * keyon restarts a noise voice's LFSR and phase (every hit sounds the
 * same) and the phase of a pitched voice that was silent (a sounding one
 * keeps its phase: no click on a legato note). */
void dcsnd_mvoice(int v, const DcSndMV *m);
/* the music bus: 255 = full, 0 = mute (S1's mute and duck); the sample
 * voices and the tone channels are not on it */
void dcsnd_music_gain(uint8_t gain);
/* S1's tape stop: every music voice sounds down16 sixteenths of a semitone
 * lower (0 = off), never below 20 Hz */
void dcsnd_music_tape(int down16);
/* the phase step (Q32 per output frame) of pitch p16 (tests) */
uint32_t dcsnd_pitch_step(int p16);
/* musvoice.c: plat_audio_caps / plat_voice / plat_audio_bus / plat_audio_fx
 * of the IRQ tier, and the hook (a no-op when built with DC_MUSIC_IRQ=0) */
void dc_music_init(void);
void dc_music_tick(void);                /* the driver hook (S1's state, then mus_drv_tick) */
void dc_music_sync_reset(void);          /* tests: a fresh game */

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
    uint32_t records;                     /* music voice records (dcsnd_mvoice) */
    uint32_t drv_ticks;                   /* driver calls (8 per push)          */
} DcSndStats;
extern volatile DcSndStats dcsnd_stats;
int dcsnd_queued(void);                   /* slots not finished (incl. playing) */
int dcsnd_voices_on(void);                /* sample voices sounding (producer)  */

#endif
