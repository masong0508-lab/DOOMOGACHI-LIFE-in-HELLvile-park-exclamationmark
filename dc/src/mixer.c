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
 *   - (DC5b) up to 8 music voices, the DoomoVoice records of the music
 *     engine's IRQ tier (dcsnd_mvoice, from musvoice.c's plat_voice): the
 *     same oscillators with a pitch in 1/16 semitones (a 192-entry table
 *     of phase steps, shifted per octave), any pulse duty in 1/32, the
 *     7-bit noise, a volume of 0..63 per side, the music bus gain and the
 *     tape stop's pitch. With a driver set (dcsnd_set_driver) dcsnd_push()
 *     calls it 8 times a slot, before the frames of each mt, so the 240 Hz
 *     driver's records land at sample-exact positions (audio_int.h);
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

/* a music voice (DC5b): the last record, and what the oscillator runs on */
typedef struct MVoice {
    uint32_t ph, step, duty;
    int32_t  gl, gr;                      /* 0 / 0: silent                       */
    uint16_t lfsr;
    uint8_t  wave, pad;
    DcSndMV  m;
} MVoice;

static Tone     tone[DCSND_TONES];
static Voice    voice[DCSND_VOICES];
static MVoice   mv[DCSND_MVOICES];
static uint8_t  m_gain = 255;             /* the music bus                       */
static uint16_t m_tape;                   /* the tape stop, 1/16 semitones down  */
static void   (*drv_tick)(void);          /* the 240 Hz music driver, or NULL    */
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

/* the phase step (Q32 per output frame) of C8 .. B8 in 1/16 semitones:
 * round(C0 * 2^(8 + i / 192) * 2^32 / 44100), C0 = 440 * 2^(-57/12) Hz.
 * A lower octave is the same step shifted right (test_tier checks every
 * pitch against the formula). */
static const uint32_t step_c8[192] = {
    407681904u, 409156353u, 410636135u, 412121268u, 413611773u, 415107669u, 416608975u, 418115710u,
    419627895u, 421145549u, 422668692u, 424197343u, 425731523u, 427271252u, 428816550u, 430367436u,
    431923931u, 433486056u, 435053830u, 436627274u, 438206409u, 439791256u, 441381834u, 442978164u,
    444580268u, 446188167u, 447801880u, 449421430u, 451046837u, 452678123u, 454315309u, 455958415u,
    457607465u, 459262478u, 460923477u, 462590483u, 464263518u, 465942604u, 467627763u, 469319017u,
    471016387u, 472719896u, 474429566u, 476145419u, 477867478u, 479595765u, 481330303u, 483071114u,
    484818220u, 486571646u, 488331413u, 490097545u, 491870064u, 493648994u, 495434357u, 497226178u,
    499024479u, 500829283u, 502640616u, 504458499u, 506282957u, 508114013u, 509951692u, 511796017u,
    513647012u, 515504702u, 517369110u, 519240261u, 521118180u, 523002890u, 524894417u, 526792785u,
    528698018u, 530610142u, 532529182u, 534455162u, 536388108u, 538328045u, 540274997u, 542228992u,
    544190053u, 546158206u, 548133478u, 550115894u, 552105479u, 554102260u, 556106263u, 558117514u,
    560136038u, 562161863u, 564195015u, 566235520u, 568283405u, 570338696u, 572401421u, 574471605u,
    576549277u, 578634463u, 580727191u, 582827487u, 584935380u, 587050895u, 589174062u, 591304908u,
    593443461u, 595589747u, 597743796u, 599905636u, 602075294u, 604252800u, 606438180u, 608631465u,
    610832681u, 613041859u, 615259027u, 617484213u, 619717447u, 621958758u, 624208175u, 626465728u,
    628731445u, 631005357u, 633287492u, 635577882u, 637876554u, 640183541u, 642498871u, 644822575u,
    647154683u, 649495225u, 651844232u, 654201735u, 656567764u, 658942351u, 661325525u, 663717318u,
    666117762u, 668526888u, 670944726u, 673371309u, 675806668u, 678250835u, 680703842u, 683165720u,
    685636503u, 688116221u, 690604907u, 693102595u, 695609315u, 698125102u, 700649987u, 703184004u,
    705727186u, 708279565u, 710841176u, 713412051u, 715992224u, 718581729u, 721180599u, 723788868u,
    726406571u, 729033741u, 731670412u, 734316620u, 736972398u, 739637780u, 742312803u, 744997501u,
    747691908u, 750396060u, 753109992u, 755833739u, 758567337u, 761310822u, 764064229u, 766827594u,
    769600953u, 772384343u, 775177799u, 777981358u, 780795057u, 783618932u, 786453020u, 789297357u,
    792151982u, 795016931u, 797892242u, 800777952u, 803674098u, 806580719u, 809497852u, 812425535u,
};
#define P16_MAX   (192 * 9 - 1)           /* B8 + 15/16                          */
#define P16_20HZ  56                      /* the first pitch at or above 20 Hz   */

uint32_t dcsnd_pitch_step(int p16)
{
    int oct;
    if (p16 < 1) p16 = 1;
    if (p16 > P16_MAX) p16 = P16_MAX;
    oct = p16 / 192;
    return step_c8[p16 - oct * 192] >> (8 - oct);
}

void dcsnd_reset(void)
{
    int c;
    memset(&cur, 0, sizeof(cur));
    memset(mv, 0, sizeof(mv));
    for (c = 0; c < DCSND_MVOICES; c++) mv[c].lfsr = 0x7FFF;
    m_gain = 255;
    m_tape = 0;
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

/* one oscillator into the block: +-4096 per sample times the two gains.
 * wave is DCMW_*; duty the pulse's high part of the phase (Q32). */
static void osc_block(uint32_t *php, uint32_t step, uint16_t *lfsrp, int wave, uint32_t duty,
                      int32_t gl, int32_t gr, int n)
{
    uint32_t ph = *php;
    int i;
    switch (wave) {
    case DCMW_NOISE15:
    case DCMW_NOISE7: {
        const uint32_t tap7 = wave == DCMW_NOISE7;
        uint32_t lfsr = *lfsrp;
        for (i = 0; i < n; i++) {
            uint32_t p0 = ph;
            int32_t s;
            ph += step;
            if (ph < p0) {                /* the LFSR clocks at the frequency */
                uint32_t bit = (lfsr ^ (lfsr >> 1)) & 1u;
                lfsr = (lfsr >> 1) | (bit << 14);
                if (tap7) lfsr = (lfsr & ~0x40u) | (bit << 6);   /* the short, metallic one */
            }
            s = (lfsr & 1) ? 4096 : -4096;
            acc_l[i] += s * gl;
            acc_r[i] += s * gr;
        }
        *lfsrp = (uint16_t)lfsr;
        break;
    }
    case DCMW_TRI:
        for (i = 0; i < n; i++) {
            int32_t u = (int32_t)(ph >> 16), s;
            s = ((u < 32768 ? u : 65535 - u) - 16384) >> 2;
            ph += step;
            acc_l[i] += s * gl;
            acc_r[i] += s * gr;
        }
        break;
    case DCMW_SAW:
        for (i = 0; i < n; i++) {
            int32_t s = ((int32_t)(ph >> 16) - 32768) >> 3;
            ph += step;
            acc_l[i] += s * gl;
            acc_r[i] += s * gr;
        }
        break;
    default: {                            /* the pulse */
        const int32_t hl = 4096 * gl, hr = 4096 * gr;
        for (i = 0; i < n; i++) {
            if (ph < duty) { acc_l[i] += hl; acc_r[i] += hr; }
            else           { acc_l[i] -= hl; acc_r[i] -= hr; }
            ph += step;
        }
        break;
    }
    }
    *php = ph;
}

/* one tone channel into the block: +-4096 * vol per sample, panned */
static void tone_block(Tone *t, const int16_t pan[2], int n)
{
    int wave = DCMW_PULSE;
    uint32_t duty = 0x80000000u;
    if (!t->vol || !t->freq) return;
    switch (t->wave) {
    case WAVE_NOISE:    wave = DCMW_NOISE15; break;
    case WAVE_TRIANGLE: wave = DCMW_TRI; break;
    case WAVE_SAW:      wave = DCMW_SAW; break;
    case WAVE_PULSE12:  duty = 0x20000000u; break;
    case WAVE_PULSE25:  duty = 0x40000000u; break;
    default: break;                       /* 50 % */
    }
    osc_block(&t->ph, t->step, &t->lfsr, wave, duty, pan[0] * t->vol, pan[1] * t->vol, n);
}

/* ---- the music voices (DC5b) ------------------------------------------------------------ */
/* a tone at volume 15 and a music voice at 63 are equally loud: the gain is
 * place * vol * 15 / 63, times the bus (255 = 1) */
static int32_t mv_gain(int place, int vol)
{
    return (int32_t)(((uint32_t)place * (uint32_t)vol * m_gain * 15u + 8032u) / 16065u);
}

/* the voice's step and gains from its record, the bus and the tape stop */
static void mv_apply(int v)
{
    MVoice *o = &mv[v];
    const DcSndMV *m = &o->m;
    const int16_t *pan = tone_pan[v < DCSND_TONES ? v : 0];
    uint32_t step;
    if (m->wave >= DCMW_NOISE15 && m->hz) {
        step = (uint32_t)(((uint64_t)m->hz << 32) / DCSND_RATE);
        if (m_tape) {                     /* x 2^(-tape / 192), not below 20 Hz */
            int oct = m_tape / 192, r = m_tape - oct * 192;
            if (r) step = (uint32_t)(((uint64_t)step * step_c8[192 - r]) / step_c8[0]) >> 1;
            step >>= oct;
            if (step < 1947816u) step = 1947816u;      /* 20 Hz */
        }
    } else if (m->p16) {
        int p = m->p16;
        if (m_tape) { p -= m_tape; if (p < P16_20HZ) p = P16_20HZ; }
        step = dcsnd_pitch_step(p);
    } else step = 0;
    o->step = step;
    o->gl = step ? mv_gain(pan[0], m->vol_l) : 0;
    o->gr = step ? mv_gain(pan[1], m->vol_r) : 0;
}

void dcsnd_mvoice(int v, const DcSndMV *m)
{
    MVoice *o;
    if (v < 0 || v >= DCSND_MVOICES || !m) return;
    o = &mv[v];
    if (m->keyon) {
        if (m->wave >= DCMW_NOISE15) { o->lfsr = 0x7FFF; o->ph = 0; }
        else if (!(o->gl | o->gr)) o->ph = 0;
    }
    o->m = *m;
    if (o->m.vol_l > 63) o->m.vol_l = 63;
    if (o->m.vol_r > 63) o->m.vol_r = 63;
    o->wave = m->wave <= DCMW_NOISE7 ? m->wave : DCMW_PULSE;
    o->duty = (uint32_t)(m->duty < 1 ? 16 : m->duty > 31 ? 31 : m->duty) << 27;
    mv_apply(v);
    dcsnd_stats.records++;
}

void dcsnd_music_gain(uint8_t gain)
{
    int v;
    if (gain == m_gain) return;
    m_gain = gain;
    for (v = 0; v < DCSND_MVOICES; v++) mv_apply(v);
}

void dcsnd_music_tape(int down16)
{
    int v;
    if (down16 < 0) down16 = 0;
    if (down16 > 16 * 48) down16 = 16 * 48;
    if ((uint16_t)down16 == m_tape) return;
    m_tape = (uint16_t)down16;
    for (v = 0; v < DCSND_MVOICES; v++) mv_apply(v);
}

void dcsnd_set_driver(void (*tick)(void)) { drv_tick = tick; }

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
 * top passes the knee; many effects at once can pass the rail) is rounded
 * off instead of hard-clipped. `clips` still counts the samples that would
 * have passed the rail. Only samples over the knee pay for the division
 * (rare: the music peaks near 20,000). */
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
        for (c = 0; c < DCSND_MVOICES; c++) {
            MVoice *o = &mv[c];
            if (o->gl | o->gr) osc_block(&o->ph, o->step, &o->lfsr, o->wave, o->duty, o->gl, o->gr, m);
        }
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
        /* the music keeps the game's time (its records still reach the voices) */
        if (drv_tick) for (i = 0; i < DCSND_MT; i++) { drv_tick(); dcsnd_stats.drv_ticks++; }
        return 0;                         /* the state and triggers wait for the next push */
    }
    for (c = 0; c < DCSND_TONES; c++) set_tone(c, &cur.tone[c]);
    for (i = 0; i < cur.ntrig; i++) start_voice(&cur.trig[i]);
    cur.ntrig = 0;
    if (!drv_tick) synth(ring[q_wr & (DCSND_QUEUE - 1)], DCSND_PER_TICK);
    else {
        /* the IRQ tier: the driver's mt k, then the frames of mt k */
        int16_t *slot = ring[q_wr & (DCSND_QUEUE - 1)];
        for (i = 0; i < DCSND_MT; i++) {
            int f0 = i * DCSND_PER_TICK / DCSND_MT, f1 = (i + 1) * DCSND_PER_TICK / DCSND_MT;
            drv_tick();
            dcsnd_stats.drv_ticks++;
            synth(slot + f0 * 2, f1 - f0);
        }
    }
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
