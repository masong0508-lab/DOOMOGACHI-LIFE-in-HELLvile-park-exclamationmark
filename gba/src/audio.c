/* audio.c - plat_tone() on the GBA PSG and plat_sample() on DirectSound A
 * (owner: gba).
 *
 * Tone channels (the core sequencer, 30 Hz):
 *   core 0 -> PSG square 1   core 1 -> PSG square 2
 *   core 2 -> PSG wave (32-sample 4-bit wave RAM: triangle, saw, or a
 *             pulse shape)    core 3 -> PSG noise (15-bit LFSR for
 *             WAVE_NOISE, 7-bit "periodic" buzz for tonal waves)
 * Registers are only rewritten when something changed; a pure frequency
 * change never restarts the channel (no clicks), a volume change restarts
 * it with the new initial envelope volume (envelope step 0 = sustain).
 *
 * Samples: DirectSound A, timer 0 at 10512 Hz (exactly 176 samples per
 * frame), DMA1 feeding FIFO A from a 2 x 176 byte double buffer that the
 * VBlank IRQ refills (mixer.iwram.c). 4 software voices with volume and
 * resampling from each sample's own rate.
 */
#include "gba_plat.h"
#include "audio_int.h"
#include "doomo/audio.h"
#include "doomo/platform.h"
#include <string.h>

/* ---- PSG ------------------------------------------------------------------ */
typedef struct ToneState { uint16_t freq; uint8_t vol, wave; } ToneState;
static ToneState tone[4];
static int wave_bank;          /* bank currently selected for playback */
static int wave_loaded = -1;   /* waveform id in the playing bank      */
static int wave_amp = -1;      /* its amplitude (0..256) in the playing bank */
static int wave_mixed = 16;    /* its pluck mix (16 = the plain table)       */

/* Round 2 switches (each 0 = the sound of the first round). */
#ifndef GBA_VIBRATO
#define GBA_VIBRATO 1          /* a delayed vibrato on long lead notes (square 1)            */
#endif
#ifndef GBA_PLUCK
#define GBA_PLUCK 1            /* the triangle on the wave channel starts bright, then mellows */
#endif
/* vibrato: ~23 cents deep at 6 Hz, starts 100 ms into a note and fades in over 130 ms */
#define VIB_DELAY 24
#define VIB_RAMP  32           /* irqs (240 Hz) to full depth; a power of two */
#define VIB_STEP  1638         /* phase step per irq: 65536 * 6 / 240        */
#define VIB_DEPTH 14           /* of 1024: 14 / 1024 = 1.4 % = 23 cents      */
/* pluck: the table moves from the saw to the triangle in 8 steps of 4 irqs (133 ms) */
#define PLUCK_STEP 2           /* of 16 per update, 60 Hz                    */

/* Wave channel loudness: the hardware only has 100 / 75 / 50 / 25 % (4 steps), which made bass decays jump. A
 * volume 1..15 is a hardware level (the smallest one that is loud enough) times a copy of the table scaled to the
 * rest, around the middle of the 4-bit range so the DC level does not move (no pops). {SND3CNT bits, amp 0..256}. */
static const struct { uint16_t hw; uint16_t amp; } wave_vol[16] = {
    {0x0000, 0},   {0x6000, 68},  {0x6000, 136}, {0x6000, 204}, {0x4000, 136}, {0x4000, 170},
    {0x4000, 204}, {0x4000, 238}, {0x8000, 182}, {0x8000, 205}, {0x8000, 228}, {0x8000, 250},
    {0x2000, 205}, {0x2000, 222}, {0x2000, 239}, {0x2000, 256}
};

static uint8_t wave_scale(uint8_t s4, int amp)         /* one 4-bit sample, amp 0..256 */
{
    return (uint8_t)((15 * 256 + (2 * (int)s4 - 15) * amp + 256) >> 9);
}

static const uint8_t wave_tables[WAVE_COUNT][16] = {
    /* 32 4-bit samples, high nibble first */
    { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   /* 12.5% */
    { 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   /* 25% */
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   /* 50% */
    { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 },   /* triangle */
    { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
      0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },   /* saw */
    { 0x8F, 0x3C, 0xA1, 0x57, 0xE2, 0x09, 0xB6, 0x4D,
      0x72, 0xC8, 0x1F, 0x95, 0x3A, 0xE6, 0x0B, 0x64 },   /* noise-ish */
};

static uint8_t wave_nib(const uint8_t *t, int k) { return (uint8_t)((k & 1) ? t[k >> 1] & 15 : t[k >> 1] >> 4); }

/* one table into the idle bank: the waveform, `mix` 0..16 of the way from the saw to it (16 = the table itself),
 * scaled to `amp` */
static void wave_load(int w, int amp, int mix)
{
    const uint8_t *src = wave_tables[w], *saw = wave_tables[WAVE_SAW];
    int i, j;
    /* write the bank that is NOT playing, then switch to it */
    REG_SND3SEL = (uint16_t)(0x80 | (wave_bank << 6));
    for (i = 0; i < 4; i++) {
        uint32_t word = 0;
        for (j = 0; j < 4; j++) {
            uint8_t sb = 0;
            int h;
            for (h = 0; h < 2; h++) {
                int k = (i * 4 + j) * 2 + h, v = wave_nib(src, k);
                if (mix < 16) v = (wave_nib(saw, k) * (16 - mix) + v * mix + 8) >> 4;
                if (amp < 256) v = wave_scale((uint8_t)v, amp);
                sb = (uint8_t)(sb | (h == 0 ? v << 4 : v));
            }
            word |= (uint32_t)sb << (8 * j);
        }
        REG_WAVE_RAM[i] = word;
    }
    wave_bank ^= 1;
    REG_SND3SEL = (uint16_t)(0x80 | (wave_bank << 6));
    wave_loaded = w;
    wave_amp = amp;
    wave_mixed = mix;
}

#if GBA_PLUCK
static uint8_t pluck_m = 16;   /* how far the bass note's table has mellowed, 0 = just struck, 16 = done */
static uint8_t pluck_n;
#define PLUCK_MIX(w) ((w) == WAVE_TRIANGLE ? pluck_m : 16)
#else
#define PLUCK_MIX(w) 16
#endif

static uint16_t sq_rate(uint16_t hz)
{
    int32_t n;
    if (hz < 64) hz = 64;
    n = 2048 - (int32_t)((131072u + (hz >> 1)) / hz);      /* nearest step, not the one below */
    return (uint16_t)(n < 0 ? 0 : (n > 2047 ? 2047 : n));
}

static uint16_t wave_rate(uint16_t hz)
{
    int32_t n;
    if (hz < 32) hz = 32;
    n = 2048 - (int32_t)((65536u + (hz >> 1)) / hz);
    return (uint16_t)(n < 0 ? 0 : (n > 2047 ? 2047 : n));
}

static uint16_t noise_freq(uint16_t hz, int periodic)
{
    /* f = 524288 / r / 2^(s+1), r = 0 meaning 0.5 */
    uint32_t target, s = 0, r;
    if (hz < 2) hz = 2;
    target = 524288u / hz;                 /* = r * 2^(s+1) */
    while ((target >> (s + 1)) > 7 && s < 13) s++;
    r = (target + (1u << s)) >> (s + 1);                   /* rounded */
    if (r > 7) r = 7;
    return (uint16_t)(r | (s << 4) | (periodic ? 0x08 : 0));
}

#if GBA_VIBRATO
static uint16_t vib_age;       /* irqs the lead's current note has been held (240 Hz)    */
static uint16_t vib_ph;
static uint32_t vib_d;         /* the note's divisor, Q8 (131072 / hz * 256)             */
static uint16_t vib_cur;       /* the register value the vibrato wrote last              */
static const int8_t vib_sin[32] = {
    0, 20, 38, 56, 71, 83, 92, 98, 100, 98, 92, 83, 71, 56, 38, 20,
    0, -20, -38, -56, -71, -83, -92, -98, -100, -98, -92, -83, -71, -56, -38, -20
};
#endif

static void tone_set(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave)
{
    ToneState *t;
    int restart;
    if (channel < 0 || channel > 3) return;
    if (vol > 15) vol = 15;
    if (wave >= WAVE_COUNT) wave = WAVE_PULSE50;
    if (freq_hz == 0) vol = 0;
    t = &tone[channel];
    if (t->freq == freq_hz && t->vol == vol && t->wave == wave) return;
    restart = (t->vol != vol) || (t->wave != wave) || (t->vol == 0 && vol);
#if GBA_VIBRATO
    if (channel == 0 && (t->freq != freq_hz || t->vol == 0 || t->wave != wave)) vib_age = 0;   /* a new note */
#endif
#if GBA_PLUCK
    if (channel == 2 && (t->vol == 0 || t->wave != wave || (t->freq > freq_hz ? t->freq - freq_hz : freq_hz - t->freq) * 32 > t->freq))
        pluck_m = 0;                                       /* a new note: the saw-bright start again */
#endif
    t->freq = freq_hz;
    t->vol = vol;
    t->wave = wave;

    switch (channel) {
    case 0:
    case 1: {
        int duty = wave == WAVE_PULSE12 ? 0 : wave == WAVE_PULSE25 ? 1 :
                   wave == WAVE_SAW ? 1 : 2;
        uint16_t cnt = (uint16_t)((duty << 6) | (vol << 12));    /* env step 0 */
        uint16_t fr = sq_rate(freq_hz ? freq_hz : 440);
#if GBA_VIBRATO
        if (channel == 0) {
            vib_d = (33554432u + (freq_hz ? freq_hz : 440) / 2) / (freq_hz ? freq_hz : 440);   /* 131072 / hz, Q8 */
            if (vib_age >= VIB_DELAY) fr = vib_cur;         /* mid-vibrato: a volume step must not snap the pitch back */
        }
#endif
        if (channel == 0) {
            REG_SND1SWEEP = 0x0008;                              /* sweep off */
            if (restart) { REG_SND1CNT = cnt; REG_SND1FREQ = (uint16_t)(fr | 0x8000); }
            else REG_SND1FREQ = fr;
        } else {
            if (restart) { REG_SND2CNT = cnt; REG_SND2FREQ = (uint16_t)(fr | 0x8000); }
            else REG_SND2FREQ = fr;
        }
        break; }
    case 2: {
        if (vol == 0) { REG_SND3CNT = 0; break; }
        if (wave != wave_loaded || wave_vol[vol].amp != wave_amp || PLUCK_MIX(wave) != wave_mixed)
            wave_load(wave, wave_vol[vol].amp, PLUCK_MIX(wave));
        REG_SND3CNT = wave_vol[vol].hw;
        if (restart) REG_SND3FREQ = (uint16_t)(wave_rate(freq_hz) | 0x8000);
        else REG_SND3FREQ = wave_rate(freq_hz);
        break; }
    default: {
        uint16_t nf = noise_freq(freq_hz ? freq_hz : 1000, wave != WAVE_NOISE);
        /* a sweeping drum (the pitch moves, the level does not) keeps its noise running: re-triggering it every tick
         * made a 30 Hz tick under every kick and tom */
        if (restart) { REG_SND4CNT = (uint16_t)(vol << 12); REG_SND4FREQ = (uint16_t)(nf | 0x8000); }
        else REG_SND4FREQ = (uint16_t)nf;
        break; }
    }
}

/* plat_tone is called from the main thread and from the music IRQ: one call at a time */
void plat_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave)
{
    uint16_t ime = REG_IME;
    REG_IME = 0;
    tone_set(channel, freq_hz, vol, wave);
    REG_IME = ime;
}

/* ST-ADV Mix view pan: the PSG can only switch a channel to the left speaker, the right one or both, so left / middle / right
 * is the whole range. SNDDMGCNT bits 12..15 enable channels 1..4 on the left, bits 8..11 on the right; the low byte (the
 * master volumes) is not touched. The default is every channel on both sides, as at boot (0xFF77). */
static uint8_t tone_pan[4] = { 1, 1, 1, 1 };

void plat_tone_pan(int channel, int pan)
{
    uint16_t ime, m = 0;
    int c;
    if (channel < 0 || channel > 3) return;
    tone_pan[channel] = (uint8_t)(pan < 0 ? 0 : pan > 2 ? 2 : pan);
    for (c = 0; c < 4; c++) {
        if (tone_pan[c] != 2) m |= (uint16_t)(0x1000u << c);   /* left on unless panned right */
        if (tone_pan[c] != 0) m |= (uint16_t)(0x0100u << c);   /* right on unless panned left */
    }
    ime = REG_IME;
    REG_IME = 0;
    REG_SNDDMGCNT = (uint16_t)((REG_SNDDMGCNT & 0x00FFu) | m);
    REG_IME = ime;
}

/* ---- the music clock ------------------------------------------------------------------------
 * Timer 1 fires 240 times a second (16777216 / 64 / 1092 = 240.07 Hz) and runs the core's music
 * driver (mus_drv_tick, one mt) whatever the frame loop is doing: a 100 ms hub frame or a 3D frame
 * no longer stalls the notes, and a catch-up tick no longer makes them rush. Every 8th mt (a 30 Hz
 * control frame) the tones go to the PSG at once (audio_seq_irq_flush). The core sees
 * CAP_DRV_IRQ on the legacy bridge tier (mus_core.c: drv_irq): same sound, steady clock. */
static volatile uint8_t music_ready;
static uint8_t music_n;

void plat_audio_caps(DoomoAudioCaps *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->tier = AT_BRIDGE;
    out->flags = CAP_DRV_IRQ;
    music_ready = 1;
}

uint32_t plat_irq_save(void)
{
    uint32_t ime = REG_IME;
    REG_IME = 0;
    return ime;
}

void plat_irq_restore(uint32_t saved) { REG_IME = (uint16_t)saved; }

/* Buzzy Saw on the two pulse channels. The PSG has no saw there, so the lead used to be a plain 25 % pulse, the
 * same sound as "Pulse". Instead the pulse width is swept 12.5 > 25 > 50 > 25 % at 120 Hz: the width register is
 * written live (low byte only, the volume byte is left alone so nothing re-triggers), which makes the thick, moving
 * buzz of a C64 / GB width-swept lead. Set GBA_SAW_PWM to 0 for the old flat 25 %. */
#ifndef GBA_SAW_PWM
#define GBA_SAW_PWM 1
#endif
#if GBA_SAW_PWM
static uint8_t pwm_n;

static void saw_pwm_tick(void)                          /* 240 Hz, from the music IRQ */
{
    static const uint8_t duty[4] = { 0, 1, 2, 1 };
    uint8_t d;
    if (++pwm_n & 1) return;
    d = (uint8_t)(duty[(pwm_n >> 1) & 3] << 6);
    if (tone[0].wave == WAVE_SAW && tone[0].vol) REG8(0x04000062) = d;
    if (tone[1].wave == WAVE_SAW && tone[1].vol) REG8(0x04000068) = d;
}
#endif

#if GBA_VIBRATO
/* 240 Hz: the lead's pitch rides a sine once the note has been held VIB_DELAY irqs. No divide: the note's divisor
 * (Q8) is scaled by (1 - v), which is the pitch up by v to a hair. Writing the frequency without the trigger bit
 * moves a running square without restarting it. */
static void vib_tick(void)
{
    const ToneState *t = &tone[0];
    int k, v;
    uint32_t d;
    if (!t->vol || !t->freq || t->wave == WAVE_NOISE) { vib_age = 0; return; }
    if (vib_age < 0xFFFF) vib_age++;
    if (vib_age < VIB_DELAY) return;
    k = vib_age - VIB_DELAY;
    if (k > VIB_RAMP) k = VIB_RAMP;
    vib_ph = (uint16_t)(vib_ph + VIB_STEP);
    v = (vib_sin[vib_ph >> 11] * VIB_DEPTH * k) / (100 * VIB_RAMP);          /* of 1024 */
    d = (vib_d * (uint32_t)(1024 - v)) >> 10;
    d = (d + 128) >> 8;
    if (d < 1) d = 1;
    if (d > 2047) d = 2047;
    vib_cur = (uint16_t)(2048 - d);
    REG_SND1FREQ = vib_cur;
}
#endif

#if GBA_PLUCK
static void pluck_tick(void)                            /* 240 Hz: every 4th, 60 Hz */
{
    if ((++pluck_n & 3) || pluck_m >= 16) return;
    if (tone[2].wave != WAVE_TRIANGLE || !tone[2].vol) { pluck_m = 16; return; }
    pluck_m = (uint8_t)(pluck_m + PLUCK_STEP > 16 ? 16 : pluck_m + PLUCK_STEP);
    wave_load(WAVE_TRIANGLE, wave_vol[tone[2].vol].amp, pluck_m);
}
#endif

static void music_irq(void)
{
    if (!music_ready) return;
    mus_drv_tick();
#if GBA_VIBRATO
    vib_tick();
#endif
#if GBA_PLUCK
    pluck_tick();
#endif
#if GBA_SAW_PWM
    saw_pwm_tick();
#endif
    if (mus_drv_take_frame() || ++music_n >= 8) {
        music_n = 0;
        audio_seq_irq_flush();
    }
}

extern void (*volatile gba_music_hook)(void);

/* ---- samples ---------------------------------------------------------------- */
void plat_sample(int sfx_id, uint8_t vol)
{
    uint32_t len = 0, rate = 0;
    const int8_t *pcm;
    int i, best = 0;
    uint32_t oldest = 0xFFFFFFFFu;
    if (sfx_id <= 0 || vol == 0) return;
    pcm = audio_sfx_pcm(sfx_id, &len, &rate);
    if (!pcm || len == 0) return;
    if (rate == 0) rate = 10512;
    if (vol > 15) vol = 15;
    /* free voice, else steal the oldest */
    for (i = 0; i < MIX_VOICES; i++) {
        if (!mix_voice[i].active) { best = i; oldest = 0; break; }
        if (mix_voice[i].started < oldest) { oldest = mix_voice[i].started; best = i; }
    }
    {
        MixVoice *v = &mix_voice[best];
        uint16_t ime = REG_IME;
        REG_IME = 0;
        v->active = 0;
        v->pcm = pcm;
        v->len = len << 12;
        v->pos = 0;
        v->step = (uint32_t)(((uint64_t)rate << 12) / MIX_RATE);
        v->vol = vol;
        v->started = gba_vblanks;
        v->active = 1;
        REG_IME = ime;
    }
}

/* ---- init --------------------------------------------------------------------- */
void audio_hw_init(void)
{
    int i;
    REG_SNDSTAT = 0x80;                    /* master enable (first!) */
    REG_SNDDMGCNT = 0xFF77;                /* PSG L/R volume 7, all 4 on both sides */
    /* PSG 100%, DirectSound A 100% on both sides, timer 0, FIFO reset */
    REG_SNDDSCNT = 0x0002 | 0x0004 | 0x0100 | 0x0200 | 0x0800;
    REG_SNDBIAS = 0x0200;
    for (i = 0; i < 4; i++) { tone[i].freq = 0xFFFF; tone[i].vol = 0xFF; }
    for (i = 0; i < 4; i++) plat_tone(i, 0, 0, WAVE_PULSE50);
    wave_load(WAVE_TRIANGLE, 256, 16);
    mixer_init();
    music_n = 0;
    REG_TM1CNT = 0;
    REG_TM1D = (uint16_t)(65536u - 1092u);
    gba_music_hook = music_irq;
    REG_TM1CNT = TM_ENABLE | TM_IRQ | TM_FREQ_64;
}
