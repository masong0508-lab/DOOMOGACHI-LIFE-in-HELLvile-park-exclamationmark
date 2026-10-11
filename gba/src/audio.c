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

/* ---- PSG ------------------------------------------------------------------ */
typedef struct ToneState { uint16_t freq; uint8_t vol, wave; } ToneState;
static ToneState tone[4];
static int wave_bank;          /* bank currently selected for playback */
static int wave_loaded = -1;   /* waveform id in the playing bank      */

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

static void wave_load(int w)
{
    const uint8_t *src = wave_tables[w];
    int i;
    /* write the bank that is NOT playing, then switch to it */
    REG_SND3SEL = (uint16_t)(0x80 | (wave_bank << 6));
    for (i = 0; i < 4; i++)
        REG_WAVE_RAM[i] = (uint32_t)src[i * 4] | ((uint32_t)src[i * 4 + 1] << 8) |
                          ((uint32_t)src[i * 4 + 2] << 16) | ((uint32_t)src[i * 4 + 3] << 24);
    wave_bank ^= 1;
    REG_SND3SEL = (uint16_t)(0x80 | (wave_bank << 6));
    wave_loaded = w;
}

static uint16_t sq_rate(uint16_t hz)
{
    int32_t n;
    if (hz < 64) hz = 64;
    n = 2048 - (int32_t)(131072u / hz);
    return (uint16_t)(n < 0 ? 0 : (n > 2047 ? 2047 : n));
}

static uint16_t wave_rate(uint16_t hz)
{
    int32_t n;
    if (hz < 32) hz = 32;
    n = 2048 - (int32_t)(65536u / hz);
    return (uint16_t)(n < 0 ? 0 : (n > 2047 ? 2047 : n));
}

static uint16_t noise_freq(uint16_t hz, int periodic)
{
    /* f = 524288 / r / 2^(s+1), r = 0 meaning 0.5 */
    uint32_t target, s = 0, r;
    if (hz < 2) hz = 2;
    target = 524288u / hz;                 /* = r * 2^(s+1) */
    while ((target >> (s + 1)) > 7 && s < 13) s++;
    r = target >> (s + 1);
    if (r == 0) r = 0;
    if (r > 7) r = 7;
    return (uint16_t)(r | (s << 4) | (periodic ? 0x08 : 0));
}

void plat_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave)
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
        uint16_t lvl;
        if (vol == 0) { REG_SND3CNT = 0; break; }
        if (wave != wave_loaded) wave_load(wave);
        lvl = vol >= 13 ? 0x2000 : vol >= 9 ? 0x8000 : vol >= 5 ? 0x4000 : 0x6000;
        REG_SND3CNT = lvl;
        if (restart) REG_SND3FREQ = (uint16_t)(wave_rate(freq_hz) | 0x8000);
        else REG_SND3FREQ = wave_rate(freq_hz);
        break; }
    default: {
        uint16_t nf = noise_freq(freq_hz ? freq_hz : 1000, wave != WAVE_NOISE);
        REG_SND4CNT = (uint16_t)(vol << 12);
        REG_SND4FREQ = (uint16_t)(nf | 0x8000);
        break; }
    }
}

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
    wave_load(WAVE_TRIANGLE);
    mixer_init();
}
