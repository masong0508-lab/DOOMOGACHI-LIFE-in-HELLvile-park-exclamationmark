/* spu.c - SPU audio: 4 chiptune tone channels + one-shot samples.
 *
 * At boot, single-cycle waveforms (pulse 12.5/25/50 %, triangle, saw) and
 * a noise loop are synthesised and encoded to SPU ADPCM by our own encoder
 * (adpcm_encode below), then uploaded to SPU RAM as looping samples.
 * plat_tone() maps a channel to SPU voice 0..3: pitch from freq_hz, volume
 * from vol, waveform = sample start address (re-keyed when it changes).
 *
 * Sound effects: the core synthesises 8-bit PCM (audio_sfx_pcm) in
 * audio_init(). Each effect is converted to ADPCM lazily - a few per frame
 * in the background after boot, or immediately on first use - and played
 * on voices 4..23 (oldest voice stolen when all are busy).
 *
 * SPU RAM map (512 KB):
 *   0x00000-0x00fff  capture buffers (reserved by hardware)
 *   0x01000-0x01fff  silent block + waveforms
 *   0x02000-0x7dfff  sound effects (bump allocator)
 *   0x7e000-0x7ffff  (reverb work area, unused)
 */
#include <string.h>
#include <psxapi.h>
#include <psxspu.h>
#include <hwregs_c.h>
#include "ps1.h"
#include "adpcm.h"

#define SPU_WAVE_BASE   0x1000
#define SPU_SFX_BASE    0x2000
#define SPU_SFX_END     0x7e000
#define TONE_VOICES     4
#define SFX_VOICE0      4
#define SFX_VOICES      20

/* waveform cycle lengths: long for low notes, short for high ones */
#define CYC_LONG   112
#define CYC_SHORT  28

static uint32_t s_wave_addr[WAVE_COUNT][2];   /* [wave][0 long, 1 short] */
static uint32_t s_silent_addr;

typedef struct ToneState {
    uint16_t freq;
    uint8_t  vol, wave, on, variant;
} ToneState;
static ToneState s_tone[TONE_VOICES];

typedef struct SfxSlot {
    uint32_t addr;        /* 0 = not converted                       */
    uint16_t pitch;
    uint8_t  state;       /* 0 todo, 1 ready, 2 failed / empty         */
} SfxSlot;
static SfxSlot  s_sfx[SFX_COUNT];
static uint32_t s_sfx_top = SPU_SFX_BASE;
static uint32_t s_voice_age[SFX_VOICES];
static uint32_t s_age;
static int      s_bg_next = 1;
static int      s_ready;

/* ---- upload ------------------------------------------------------------- */
static void spu_upload(uint32_t addr, const void *data, size_t bytes)
{
    /* DMA wants 64-byte chunks of word-aligned data */
    static uint32_t tmp[1024];
    const uint8_t *src = (const uint8_t *)data;
    while (bytes) {
        size_t n = bytes > sizeof(tmp) ? sizeof(tmp) : bytes;
        size_t padded = (n + 63) & ~(size_t)63;
        memset(tmp, 0, padded);
        memcpy(tmp, src, n);
        SpuSetTransferStartAddr(addr);
        SpuWrite(tmp, padded);
        SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
        addr += (uint32_t)n;
        src += n;
        bytes -= n;
    }
}

/* ---- waveforms ------------------------------------------------------------ */
static int16_t wave_sample(int wave, int i, int len, uint32_t *rng)
{
    int duty;
    switch (wave) {
    case WAVE_PULSE12: duty = len / 8; return (int16_t)(i < duty ? 12000 : -12000);
    case WAVE_PULSE25: duty = len / 4; return (int16_t)(i < duty ? 12000 : -12000);
    case WAVE_PULSE50: duty = len / 2; return (int16_t)(i < duty ? 12000 : -12000);
    case WAVE_TRIANGLE: {
        int ph = (i * 4 * 14000) / len;  /* 0..56000 */
        int v = ph < 14000 ? ph : ph < 42000 ? 28000 - ph : ph - 56000;
        return (int16_t)v;
    }
    case WAVE_SAW:
        return (int16_t)(-12000 + (i * 24000) / (len - 1));
    default: /* noise */
        *rng = *rng * 1103515245u + 12345u;
        return (int16_t)(((*rng >> 16) & 1) ? 11000 : -11000);
    }
}

#define NOISE_LEN (28 * 64)

static void build_waves(void)
{
    static int16_t pcm[NOISE_LEN];
    static uint8_t adpcm[NOISE_LEN / 28 * 16];
    uint32_t addr = SPU_WAVE_BASE, rng = 0x1234567u;
    int w, v, i;
    /* a silent looping block (idle voices point here) */
    memset(adpcm, 0, 16);
    adpcm[1] = 0x07;          /* loop start + end + repeat */
    spu_upload(addr, adpcm, 16);
    s_silent_addr = addr;
    addr += 16;
    for (w = 0; w < WAVE_COUNT; w++) {
        for (v = 0; v < 2; v++) {
            int len = w == WAVE_NOISE ? NOISE_LEN : (v ? CYC_SHORT : CYC_LONG);
            int blocks;
            if (w == WAVE_NOISE && v) {
                s_wave_addr[w][1] = s_wave_addr[w][0];
                continue;
            }
            if (w == WAVE_NOISE) {
                for (i = 0; i < len; i++)
                    pcm[i] = wave_sample(w, i, len, &rng);
                blocks = adpcm_encode(pcm, len, adpcm, 0);
                adpcm[1] |= 0x04;                   /* loop start */
            } else {
                /* two cycles; the loop restarts at the second one, whose
                 * blocks were encoded with the real decoder history (the
                 * first block of a sample must use filter 0) */
                for (i = 0; i < len * 2; i++)
                    pcm[i] = wave_sample(w, i % len, len, &rng);
                blocks = adpcm_encode(pcm, len * 2, adpcm, 0);
                adpcm[(len / 28) * 16 + 1] |= 0x04; /* loop start: cycle 2 */
            }
            adpcm[(blocks - 1) * 16 + 1] |= 0x03; /* loop end + repeat */
            spu_upload(addr, adpcm, (size_t)blocks * 16);
            s_wave_addr[w][v] = addr;
            addr += (uint32_t)blocks * 16;
        }
    }
}

/* ---- tone channels --------------------------------------------------------- */
static int s_music_vol = 15, s_sfx_vol = 15;

static uint16_t hz_to_pitch(uint32_t hz, int cyc)
{
    /* pitch 0x1000 = 44100 Hz sample rate; sample rate = hz * cycle */
    uint32_t p = (hz * (uint32_t)cyc * 4096u + 22050u) / 44100u;
    return (uint16_t)(p > 0x3fff ? 0x3fff : p);
}

void plat_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave)
{
    ToneState *t;
    int variant, cyc;
    uint16_t pitch;
    int amp;
    if (channel < 0 || channel >= TONE_VOICES || !s_ready)
        return;
    t = &s_tone[channel];
    if (wave >= WAVE_COUNT)
        wave = WAVE_PULSE50;
    if (freq_hz == 0 || vol == 0) {
        if (t->on) {
            SpuSetKey(0, 1u << channel);
            t->on = 0;
        }
        t->freq = 0;
        return;
    }
    if (vol > 15) vol = 15;
    if (wave == WAVE_NOISE) {
        /* noise loop: freq_hz selects the playback rate (periodic noise) */
        variant = 0;
        pitch = hz_to_pitch(freq_hz, 4);
    } else {
        variant = (uint32_t)freq_hz * CYC_LONG > 176000u ? 1 : 0;
        cyc = variant ? CYC_SHORT : CYC_LONG;
        pitch = hz_to_pitch(freq_hz, cyc);
    }
    amp = (vol * s_music_vol * 0x2000) / (15 * 15);
    SPU_CH_VOL_L(channel) = (uint16_t)amp;
    SPU_CH_VOL_R(channel) = (uint16_t)amp;
    SPU_CH_FREQ(channel) = pitch;
    if (!t->on || t->wave != wave || t->variant != variant) {
        SPU_CH_ADDR(channel) = getSPUAddr(s_wave_addr[wave][variant]);
        SPU_CH_ADSR1(channel) = 0x000f;       /* instant attack, max sustain */
        SPU_CH_ADSR2(channel) = 0x0008;       /* short linear release        */
        SpuSetKey(1, 1u << channel);
        t->on = 1;
    }
    t->freq = freq_hz;
    t->vol = vol;
    t->wave = wave;
    t->variant = (uint8_t)variant;
}

/* ---- samples ----------------------------------------------------------------- */
static int convert_sfx(int id)
{
    static int16_t pcm[28 * 64];
    static uint8_t adpcm[64 * 16];
    SfxSlot *s = &s_sfx[id];
    uint32_t len = 0, rate = 0, done = 0, bytes;
    const int8_t *src;
    if (s->state)
        return s->state == 1;
    src = core_sfx_pcm(id, &len, &rate);
    if (!src || !len || !rate) {
        s->state = 2;
        return 0;
    }
    if (len > 44100u * 3u)
        len = 44100u * 3u;
    bytes = (len + 27) / 28 * 16 + 16;
    if (s_sfx_top + bytes > SPU_SFX_END) {
        s->state = 2;
        return 0;
    }
    s->addr = s_sfx_top;
    /* chunks of 64 blocks; the encoder state restarts per chunk (filter 0
     * on each chunk's first block), inaudible at these lengths */
    while (done < len) {
        uint32_t n = len - done, i;
        int blocks;
        if (n > sizeof(pcm) / 2) n = sizeof(pcm) / 2;
        for (i = 0; i < n; i++)
            pcm[i] = (int16_t)(src[done + i] * 256);
        blocks = adpcm_encode(pcm, (int)n, adpcm, 1);
        if (done + n >= len)
            adpcm[(blocks - 1) * 16 + 1] = 0x01;   /* end: release + mute */
        spu_upload(s_sfx_top, adpcm, (size_t)blocks * 16);
        s_sfx_top += (uint32_t)blocks * 16;
        done += n;
    }
    s->pitch = (uint16_t)((rate * 4096u + 22050u) / 44100u);
    if (s->pitch > 0x3fff) s->pitch = 0x3fff;
    s->state = 1;
    return 1;
}

void plat_sample(int sfx_id, uint8_t vol)
{
    int v, best = 0, ch;
    uint32_t oldest = 0xffffffffu;
    uint16_t endx;
    int amp;
    if (!s_ready || sfx_id <= 0 || sfx_id >= SFX_COUNT || vol == 0)
        return;
    if (!convert_sfx(sfx_id))
        return;
    endx = SPU_CHAN_STATUS1;
    /* prefer a voice that finished (ENDX) or the oldest one */
    for (v = 0; v < SFX_VOICES; v++) {
        int bit = SFX_VOICE0 + v;
        uint32_t ended = bit < 16 ? (endx >> bit) & 1 : (SPU_CHAN_STATUS2 >> (bit - 16)) & 1;
        if (ended || !s_voice_age[v]) { best = v; break; }
        if (s_voice_age[v] < oldest) { oldest = s_voice_age[v]; best = v; }
    }
    ch = SFX_VOICE0 + best;
    s_voice_age[best] = ++s_age;
    if (vol > 15) vol = 15;
    amp = (vol * s_sfx_vol * 0x3800) / (15 * 15);
    SPU_CH_VOL_L(ch) = (uint16_t)amp;
    SPU_CH_VOL_R(ch) = (uint16_t)amp;
    SPU_CH_FREQ(ch) = s_sfx[sfx_id].pitch;
    SPU_CH_ADDR(ch) = getSPUAddr(s_sfx[sfx_id].addr);
    SPU_CH_ADSR1(ch) = 0x000f;
    SPU_CH_ADSR2(ch) = 0x0000;
    SpuSetKey(1, 1u << ch);
}

void audio_set_volumes(int music, int sfx)
{
    s_music_vol = music < 0 ? 0 : music > 15 ? 15 : music;
    s_sfx_vol = sfx < 0 ? 0 : sfx > 15 ? 15 : sfx;
}

void audio_hw_init(void)
{
    int v;
    SpuInit();
    SPU_MASTER_VOL_L = 0x3fff;
    SPU_MASTER_VOL_R = 0x3fff;
    build_waves();
    for (v = 0; v < 24; v++) {
        SPU_CH_VOL_L(v) = 0;
        SPU_CH_VOL_R(v) = 0;
        SPU_CH_ADDR(v) = getSPUAddr(s_silent_addr);
    }
    memset(s_sfx, 0, sizeof(s_sfx));
    s_ready = 1;
}

/* Background conversion: one effect per frame after the core is up. */
void audio_hw_frame(void)
{
    if (!s_ready || s_bg_next >= SFX_COUNT)
        return;
    if (g_frame < 2)
        return;
    convert_sfx(s_bg_next++);
}

uint32_t audio_spu_used(void) { return s_sfx_top; }
