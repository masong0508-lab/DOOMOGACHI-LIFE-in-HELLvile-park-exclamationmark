/* mixer.c - the Dreamcast software sound mixer (DC5; owner: dreamcast).
 * The interface and the timing model are in audio_int.h; the KOS stream
 * that plays the result is audio.c.
 *
 * dcsnd_push() (the main thread, the end of every logic tick) applies the
 * tick's tones and triggers and renders exactly DCSND_PER_TICK output
 * frames into the next slot of a PCM ring; dcsnd_render() (the stream
 * thread) only copies slots out. The synth never runs on the stream
 * thread: KallistiOS's timer preemption hands the CPU to the main thread
 * for a whole 10 ms slice whenever its tick lands inside the stream
 * thread's work, whatever the priorities (thd_schedule() never compares
 * the running thread with the queue), and a 10 ms hole in the middle of
 * a 23 ms refill window is too close to the AICA running dry
 * (docs/DREAMCAST.md section 6). Copying 1,024 frames takes microseconds.
 *
 * Per output frame (44,100 Hz, stereo):
 *   - 4 tone channels, the voices of plat_tone(): 12.5 / 25 / 50 % pulses,
 *     a triangle and a saw at full resolution (the GBA's 4-bit wave RAM
 *     steps are gone), and the 15-bit LFSR noise clocked at the channel
 *     frequency, as musrender does. Each sits at a fixed place in the
 *     stereo field (lead and bass centre, harmony / babble a little left,
 *     kit a little right), with constant-sum gains so L + R is the mono
 *     reference;
 *   - 8 sample voices for plat_sample() / plat_sample_ex(): 16-bit PCM at
 *     the effect's own rate with linear interpolation, volume, pan, pitch in
 *     semitones and priority stealing (a free voice, else the lowest
 *     priority, oldest first);
 *   - the mix goes through the native reference's DC block (a one-pole
 *     high-pass of about 14 Hz) and a gentle one-pole low-pass (about 12
 *     kHz) that takes the edge off the naive pulses' aliasing, and is scaled
 *     like musrender (core/native/plat_music_native.c): `mix * 24`, so the
 *     levels and the harness's expect-audio numbers match the native ones.
 *
 * Integer only; no division per sample. The content stream (every rendered
 * slot, in order) is hashed (FNV-1a over the 16-bit samples) so two runs
 * can be compared without a WAV.
 */
#include <string.h>
#include "doomo/platform.h"
#include "audio_int.h"

#if defined(__GNUC__)
#define DCSND_BARRIER() __asm__ __volatile__("" ::: "memory")
#else
#define DCSND_BARRIER() ((void)0)
#endif

volatile DcSndStats dcsnd_stats;

/* ---- the ring: slots of rendered PCM, one per logic tick ---------------------------- */
static DcSndFrame cur;                    /* this tick's state, main thread      */
static int16_t ring[DCSND_QUEUE][DCSND_PER_TICK * 2] __attribute__((aligned(32)));
static volatile uint32_t q_wr, q_rd;      /* slots written / finished (counters) */
static volatile uint8_t  stalled;

/* ---- the synth (main thread) ---------------------------------------------------------- */
typedef struct Tone {
    uint32_t ph, step;
    uint16_t freq, lfsr;
    uint8_t  vol, wave;
} Tone;
typedef struct Voice {
    const int16_t *pcm;
    uint32_t len, ipos, step;             /* step: Q16 frames of PCM per output   */
    uint16_t frac;
    uint8_t  on, prio;
    int32_t  gl, gr;                      /* vol * pan / 4                       */
    uint32_t age;
} Voice;

static Tone     tone[DCSND_TONES];
static Voice    voice[DCSND_VOICES];
static uint32_t age_ctr;
static int32_t  dcx[2], dcy[2], lp[2];

/* ---- the consumer (the stream thread) -------------------------------------------------- */
static uint32_t rec_at;                   /* frames of the head slot played      */
static uint8_t  playing;                  /* primed: slots are being consumed    */

/* the stereo place of the four tone channels (L, R; L + R = 256) */
static const int16_t tone_pan[DCSND_TONES][2] = { { 128, 128 }, { 152, 104 }, { 128, 128 }, { 104, 152 } };

/* 2^(k/12) in Q16 */
static const uint32_t semi_q16[12] = { 65536, 69433, 73562, 77936, 82570, 87480,
                                       92682, 98193, 104032, 110218, 116772, 123715 };

void dcsnd_reset(void)
{
    int c;
    memset(&cur, 0, sizeof(cur));
    q_wr = q_rd = 0;
    stalled = 0;
    memset(tone, 0, sizeof(tone));
    for (c = 0; c < DCSND_TONES; c++) tone[c].lfsr = 0x7FFF;
    memset(voice, 0, sizeof(voice));
    age_ctr = 0;
    rec_at = 0;
    playing = 0;
    memset(dcx, 0, sizeof(dcx));
    memset(dcy, 0, sizeof(dcy));
    memset(lp, 0, sizeof(lp));
    memset((void *)&dcsnd_stats, 0, sizeof(dcsnd_stats));
    dcsnd_stats.hash = 2166136261u;
}

/* ---- producer (main thread) ---------------------------------------------------------- */
void dcsnd_tone(int ch, uint16_t freq_hz, uint8_t vol, uint8_t wave)
{
    if (ch < 0 || ch >= DCSND_TONES) return;
    cur.tone[ch].freq = freq_hz;
    cur.tone[ch].vol = vol > 15 ? 15 : vol;
    cur.tone[ch].wave = wave;
}

void dcsnd_trigger(int sfx, uint8_t vol, int8_t pan, uint8_t prio, int8_t semis)
{
    DcSndTrig *t;
    if (sfx <= 0 || sfx > 255 || !vol || cur.ntrig >= DCSND_TRIG) return;
    t = &cur.trig[cur.ntrig++];
    t->sfx = (uint8_t)sfx;
    t->vol = vol > 15 ? 15 : vol;
    t->prio = prio;
    t->pan = pan;
    t->semis = semis;
}

/* on: until the next dcsnd_push() (the consumer may only notice the empty
 * ring after the pause); off changes nothing for that reason */
void dcsnd_stall(int on) { if (on) stalled = 1; }

int dcsnd_queued(void) { return (int)(q_wr - q_rd); }

int dcsnd_voices_on(void)
{
    int i, n = 0;
    for (i = 0; i < DCSND_VOICES; i++) n += voice[i].on;
    return n;
}

/* ---- the synth ------------------------------------------------------------------------- */
static void set_tone(int c, const DcSndTone *t)
{
    Tone *o = &tone[c];
    if (t->freq != o->freq) {
        o->freq = t->freq;
        o->step = (uint32_t)(((uint64_t)t->freq << 32) / DCSND_RATE);
    }
    o->vol = t->vol;
    o->wave = t->wave;
}

static void start_voice(const DcSndTrig *t)
{
    const DcSfx *s = dc_sfx_get(t->sfx);
    Voice *v = NULL;
    uint32_t step;
    int i, pan, semis, oct;
    if (!s) return;
    for (i = 0; i < DCSND_VOICES; i++)
        if (!voice[i].on) { v = &voice[i]; break; }
    if (!v) {                             /* steal: the lowest priority, the oldest */
        for (i = 0; i < DCSND_VOICES; i++) {
            Voice *w = &voice[i];
            if (w->prio > t->prio) continue;
            if (!v || w->prio < v->prio || (w->prio == v->prio && w->age < v->age)) v = w;
        }
        if (!v) return;                   /* everything playing matters more */
        dcsnd_stats.steals++;
    }
    semis = t->semis < -36 ? -36 : t->semis > 36 ? 36 : t->semis;
    oct = semis >= 0 ? semis / 12 : -((11 - semis) / 12);
    step = (uint32_t)(((uint64_t)s->step_q16 * semi_q16[semis - oct * 12]) >> 16);
    step = oct >= 0 ? step << oct : step >> -oct;
    if (!step) step = 1;
    pan = t->pan < -64 ? -64 : t->pan > 64 ? 64 : t->pan;
    v->pcm = s->pcm;
    v->len = s->len;
    v->ipos = 0;
    v->frac = 0;
    v->step = step;
    v->prio = t->prio;
    v->age = ++age_ctr;
    v->gl = (int32_t)t->vol * (128 - pan * 2) / 4;
    v->gr = (int32_t)t->vol * (128 + pan * 2) / 4;
    v->on = 1;
    dcsnd_stats.started++;
}

/* ---- the synth, in blocks of up to BLOCK frames ------------------------------------------
 * Each source adds itself into the block's two int32 accumulators in its own
 * tight loop (one wave per loop, no per-sample switch), then one pass does
 * the filters, the clamp and the hash. */
#define BLOCK 128
static int32_t acc_l[BLOCK], acc_r[BLOCK];

/* one tone channel into the block: +-4096 * vol per sample, panned */
static void tone_block(Tone *t, const int16_t pan[2], int n)
{
    const int32_t gl = pan[0] * t->vol, gr = pan[1] * t->vol;
    uint32_t ph = t->ph, step = t->step;
    int i;
    if (!t->vol || !t->freq) return;
    switch (t->wave) {
    case WAVE_NOISE: {
        uint32_t lfsr = t->lfsr;
        for (i = 0; i < n; i++) {
            uint32_t p0 = ph;
            int32_t s;
            ph += step;
            if (ph < p0) {                /* the LFSR clocks at the frequency */
                uint32_t bit = (lfsr ^ (lfsr >> 1)) & 1u;
                lfsr = (lfsr >> 1) | (bit << 14);
            }
            s = (lfsr & 1) ? 4096 : -4096;
            acc_l[i] += s * gl;
            acc_r[i] += s * gr;
        }
        t->lfsr = (uint16_t)lfsr;
        break;
    }
    case WAVE_TRIANGLE:
        for (i = 0; i < n; i++) {
            int32_t u = (int32_t)(ph >> 16), s;
            s = ((u < 32768 ? u : 65535 - u) - 16384) >> 2;
            ph += step;
            acc_l[i] += s * gl;
            acc_r[i] += s * gr;
        }
        break;
    case WAVE_SAW:
        for (i = 0; i < n; i++) {
            int32_t s = ((int32_t)(ph >> 16) - 32768) >> 3;
            ph += step;
            acc_l[i] += s * gl;
            acc_r[i] += s * gr;
        }
        break;
    default: {                            /* the pulses: 12.5 / 25 / 50 % */
        const uint32_t duty = t->wave == WAVE_PULSE12 ? 0x20000000u :
                              t->wave == WAVE_PULSE25 ? 0x40000000u : 0x80000000u;
        const int32_t hl = 4096 * gl, hr = 4096 * gr;
        for (i = 0; i < n; i++) {
            if (ph < duty) { acc_l[i] += hl; acc_r[i] += hr; }
            else           { acc_l[i] -= hl; acc_r[i] -= hr; }
            ph += step;
        }
        break;
    }
    }
    t->ph = ph;
}

/* one sample voice into the block (linear interpolation; the guard sample
 * at len makes pcm[ipos + 1] safe). The weight is the position's top 15
 * fraction bits: neighbours of the 8-bit PCM x 256 can be 65,280 apart
 * (any two int16 at most 65,535), and 65,535 * 32,767 still fits an int32,
 * where the full 16 bits (x 65,535) did not: the product wrapped on the SH4
 * (one 32-bit mul.l) to a sample 65,536 too low, a click in most effects
 * (test_audio's smooth check). One 32-bit multiply, no 64-bit one. */
static void voice_block(Voice *v, int n)
{
    const int16_t *pcm = v->pcm;
    const int32_t gl = v->gl, gr = v->gr;
    const uint32_t len = v->len, whole = v->step >> 16, part = v->step & 0xFFFFu;
    uint32_t ipos = v->ipos, frac = v->frac;
    int i;
    for (i = 0; i < n; i++) {
        int32_t a = pcm[ipos], b = pcm[ipos + 1];
        int32_t s = a + (((b - a) * (int32_t)(frac >> 1)) >> 15);
        acc_l[i] += s * gl;
        acc_r[i] += s * gr;
        frac += part;
        ipos += whole + (frac >> 16);
        frac &= 0xFFFFu;
        if (ipos >= len) { v->on = 0; i++; break; }
    }
    dcsnd_stats.voice_frames += (uint32_t)i;
    v->ipos = ipos;
    v->frac = (uint16_t)frac;
}

/* the soft knee: transparent below KNEE; above it the excess e is bent
 * to KNEE + e * ROOM / (e + ROOM), which approaches the rail but never
 * reaches it, so a loud moment (the invaded town's metal with effects on
 * top) is rounded off instead of hard-clipped. `clips` still counts the
 * samples that would have passed the rail. Only samples over the knee pay
 * for the division (none in normal play: the music peaks near 20,000). */
#define KNEE 24576
#define ROOM (32767 - KNEE)
#if defined(__GNUC__)
#define DCSND_COLD   __attribute__((noinline, cold))
#define DCSND_INLINE __attribute__((always_inline)) inline
#else
#define DCSND_COLD
#define DCSND_INLINE inline
#endif
static DCSND_COLD int32_t knee(int32_t y)
{
    int32_t a = y < 0 ? -y : y, e;
    if (a > 32767) dcsnd_stats.clips++;
    e = a - KNEE;
    a = KNEE + (int32_t)(((int64_t)e * ROOM) / (e + ROOM));
    return y < 0 ? -a : a;
}

/* the mix -> the output: the native reference's DC block (about 14 Hz at
 * 44.1 kHz), a one-pole low-pass (about 12 kHz), x 24 / 256, the knee */
static DCSND_INLINE int16_t out_sample(int ch, int32_t mix)
{
    int32_t x = mix >> 7, y, d, v = dcy[ch];   /* Q8 of the native reference's units */
    /* y = x - x1 + y * 511/512, the leak rounded away from zero so the
     * filter settles at exactly 0 (a floor would leave up to 511 behind) */
    v -= v >= 0 ? (v + 511) >> 9 : -((511 - v) >> 9);
    dcy[ch] = x - dcx[ch] + v;
    dcx[ch] = x;
    d = dcy[ch] - lp[ch];                 /* |d| < 10M: d * 210 fits */
    lp[ch] += (d * 210) >> 8;
    y = lp[ch] * 3;                       /* x 24 / 256, rounded toward 0 */
    y = (y + (y < 0 ? 31 : 0)) >> 5;
    if (y > KNEE || y < -KNEE) y = knee(y);     /* rare: out of line */
    return (int16_t)y;
}

static void synth(int16_t *out, int n)
{
    uint32_t h = dcsnd_stats.hash;
    int i, c, k;
    for (k = 0; k < n; k += BLOCK) {
        int m = n - k < BLOCK ? n - k : BLOCK;
        memset(acc_l, 0, (size_t)m * sizeof(int32_t));
        memset(acc_r, 0, (size_t)m * sizeof(int32_t));
        for (c = 0; c < DCSND_TONES; c++) tone_block(&tone[c], tone_pan[c], m);
        for (c = 0; c < DCSND_VOICES; c++) if (voice[c].on) voice_block(&voice[c], m);
        for (i = 0; i < m; i++) {
            int16_t l = out_sample(0, acc_l[i]), r = out_sample(1, acc_r[i]);
            out[0] = l;
            out[1] = r;
            out += 2;
            h = (h ^ (uint16_t)l) * 16777619u;
            h = (h ^ (uint16_t)r) * 16777619u;
        }
    }
    dcsnd_stats.hash = h;
}

/* ---- producer (main thread): render the tick into the next slot ------------------------ */
int dcsnd_push(void)
{
    uint32_t used = q_wr - q_rd, n;
    int c, i;
    if (used >= DCSND_QUEUE) {            /* the stream is not taking slots */
        dcsnd_stats.overruns++;
        return 0;                         /* the state and triggers wait for the next push */
    }
    for (c = 0; c < DCSND_TONES; c++) set_tone(c, &cur.tone[c]);
    for (i = 0; i < cur.ntrig; i++) start_voice(&cur.trig[i]);
    cur.ntrig = 0;
    synth(ring[q_wr & (DCSND_QUEUE - 1)], DCSND_PER_TICK);
    n = dcsnd_stats.pushed + 1;
    if (n == 600) dcsnd_stats.hash600 = dcsnd_stats.hash;
    DCSND_BARRIER();
    q_wr = q_wr + 1;
    stalled = 0;                          /* a pause ends with the next slot */
    dcsnd_stats.pushed = n;
    used++;
    if (used > dcsnd_stats.queue_max) dcsnd_stats.queue_max = used;
    return 1;
}

/* ---- consumer (the stream thread): copy slots out ------------------------------------------ */
/* the head slot is ready to play (1), or 0 for a gap */
static int head_ready(void)
{
    uint32_t avail = q_wr - q_rd;
    if (rec_at) return 1;                 /* in the middle of a slot */
    if (!playing) {
        if (avail < DCSND_PREFILL) return 0;
        playing = 1;
    }
    if (!avail) {
        if (!stalled) dcsnd_stats.underruns++;
        playing = 0;                      /* prime again: rebuild the slack */
        return 0;
    }
    while (avail > DCSND_MAXLAG) {        /* far behind the game: skip ahead */
        DCSND_BARRIER();
        q_rd = q_rd + 1;
        avail--;
        dcsnd_stats.dropped++;
    }
    dcsnd_stats.consumed++;
    return 1;
}

void dcsnd_render(int16_t *out, int frames)
{
    while (frames > 0) {
        int n;
        if (!head_ready()) {
            /* a gap: silence; the slots and the synth are untouched */
            memset(out, 0, (size_t)frames * 2 * sizeof(int16_t));
            dcsnd_stats.gap_frames += (uint32_t)frames;
            return;
        }
        DCSND_BARRIER();
        n = DCSND_PER_TICK - (int)rec_at;
        if (n > frames) n = frames;
        memcpy(out, &ring[q_rd & (DCSND_QUEUE - 1)][rec_at * 2], (size_t)n * 2 * sizeof(int16_t));
        out += n * 2;
        frames -= n;
        dcsnd_stats.frames += (uint32_t)n;
        rec_at += (uint32_t)n;
        if (rec_at == DCSND_PER_TICK) {   /* the slot is done: hand it back */
            rec_at = 0;
            DCSND_BARRIER();
            q_rd = q_rd + 1;
        }
    }
}
