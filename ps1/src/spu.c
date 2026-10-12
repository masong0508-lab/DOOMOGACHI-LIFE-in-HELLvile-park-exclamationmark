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
static uint32_t s_wave_end, s_wave_sum;        /* for audio_selftest()  */

static uint32_t sum_bytes(uint32_t h, const uint8_t *p, size_t n)
{
    while (n--)
        h = (h ^ *p++) * 16777619u;           /* FNV-1a */
    return h;
}

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
    s_wave_sum = sum_bytes(2166136261u, adpcm, 16);
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
            s_wave_sum = sum_bytes(s_wave_sum, adpcm, (size_t)blocks * 16);
            s_wave_addr[w][v] = addr;
            addr += (uint32_t)blocks * 16;
        }
    }
    s_wave_end = addr;
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
/* 8-bit PCM -> ADPCM in SPU RAM at s_sfx_top (one-shot: the last block
 * ends + mutes the voice). Returns the SPU address or 0 if full. */
static uint32_t load_pcm8(const int8_t *src, uint32_t len)
{
    static int16_t pcm[28 * 64];
    static uint8_t adpcm[64 * 16];
    uint32_t done = 0, addr = s_sfx_top, bytes;
    if (len > 44100u * 3u)
        len = 44100u * 3u;
    bytes = (len + 27) / 28 * 16 + 16;
    if (s_sfx_top + bytes > SPU_SFX_END)
        return 0;
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
    return addr;
}

static uint16_t rate_to_pitch(uint32_t rate)
{
    uint32_t p = (rate * 4096u + 22050u) / 44100u;
    return (uint16_t)(p > 0x3fff ? 0x3fff : p);
}

static int convert_sfx(int id)
{
    SfxSlot *s = &s_sfx[id];
    uint32_t len = 0, rate = 0;
    const int8_t *src;
    if (s->state)
        return s->state == 1;
    src = core_sfx_pcm(id, &len, &rate);
    if (!src || !len || !rate || !(s->addr = load_pcm8(src, len))) {
        s->state = 2;
        return 0;
    }
    s->pitch = rate_to_pitch(rate);
    s->state = 1;
    return 1;
}

static void key_sample(int ch, uint32_t addr, uint16_t pitch, int amp)
{
    SPU_CH_VOL_L(ch) = (uint16_t)amp;
    SPU_CH_VOL_R(ch) = (uint16_t)amp;
    SPU_CH_FREQ(ch) = pitch;
    SPU_CH_ADDR(ch) = getSPUAddr(addr);
    SPU_CH_ADSR1(ch) = 0x000f;
    SPU_CH_ADSR2(ch) = 0x0000;
    SpuSetKey(1, 1u << ch);
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
    key_sample(ch, s_sfx[sfx_id].addr, s_sfx[sfx_id].pitch, amp);
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

/* The BIOS memory card calls block the game loop for seconds, and the music
 * only advances from that loop: every tone voice would keep sounding its
 * last note as a frozen drone (and queued effects would loop on). While the
 * loop is blocked all voices are keyed off and the master volume is muted;
 * afterwards the tone voices that were sounding are started again. */
static ToneState s_tone_saved[TONE_VOICES];
static int       s_hushed;

void audio_hw_hush(int on)
{
    int v;
    if (!s_ready || on == s_hushed)
        return;
    s_hushed = on;
    if (on) {
        for (v = 0; v < TONE_VOICES; v++) {
            s_tone_saved[v] = s_tone[v];
            s_tone[v].on = 0;
        }
        for (v = 0; v < 24; v++)              /* all 24 voices off (one at a time:
                                               * the SDK macro rejects a constant mask) */
            SpuSetKey(0, 1u << v);
        SPU_MASTER_VOL_L = 0;
        SPU_MASTER_VOL_R = 0;
    } else {
        SPU_MASTER_VOL_L = 0x3fff;
        SPU_MASTER_VOL_R = 0x3fff;
        for (v = 0; v < TONE_VOICES; v++)
            if (s_tone_saved[v].freq && s_tone_saved[v].vol)
                plat_tone(v, s_tone_saved[v].freq, s_tone_saved[v].vol, s_tone_saved[v].wave);
    }
}

uint32_t audio_spu_used(void) { return s_sfx_top; }

/* ---- self test (engine test build, results on the TTY) ---------------------
 * 1. SPU RAM readback of the waveform table (DMA upload path)
 * 2. a tone voice: envelope (ENVX) rises after key-on, falls after release
 * 3. a synthetic one-shot through the sample converter: sounds, then ends
 *    (ENDX) on its own
 * The SPU RAM the test sample takes is given back afterwards. */
static void wait_vblanks(int n)
{
    uint32_t v = (uint32_t)VSync(-1);
    while ((uint32_t)VSync(-1) - v < (uint32_t)n)
        ;
}

int audio_selftest(void)
{
    static uint32_t buf[1024];
    static int8_t chirp[2205];
    uint32_t a, h = 2166136261u, top0;
    int ok_ram, ok_tone, ok_smp, env_on, env_off, env_smp, ended, i;
    if (!s_ready)
        return 0;
    /* 1 */
    for (a = SPU_WAVE_BASE; a < s_wave_end; a += sizeof(buf)) {
        uint32_t n = s_wave_end - a < sizeof(buf) ? s_wave_end - a : sizeof(buf);
        SpuSetTransferStartAddr(a);
        SpuRead(buf, (n + 63) & ~63u);
        SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
        h = sum_bytes(h, (const uint8_t *)buf, n);
    }
    ok_ram = h == s_wave_sum;
    dbg("audio selftest: SPU RAM readback %u bytes %s\n", (unsigned)(s_wave_end - SPU_WAVE_BASE),
        ok_ram ? "ok" : "MISMATCH");
    /* 2 */
    plat_tone(0, 440, 15, WAVE_TRIANGLE);
    wait_vblanks(6);
    env_on = SPU_CH_ADSR_VOL(0);
    plat_tone(0, 0, 0, 0);
    wait_vblanks(30);
    env_off = SPU_CH_ADSR_VOL(0);
    ok_tone = env_on > 0x4000 && env_off < env_on / 4;
    dbg("audio selftest: tone envelope on %04x released %04x %s\n", env_on, env_off,
        ok_tone ? "ok" : "FAIL");
    /* 3: 0.2 s chirp at 11025 Hz */
    for (i = 0; i < (int)sizeof(chirp); i++) {
        int ph = (i * (40 + i / 16)) & 255;
        chirp[i] = (int8_t)(ph < 128 ? ph - 64 : 191 - ph);
    }
    top0 = s_sfx_top;
    a = load_pcm8(chirp, sizeof(chirp));
    ok_smp = a != 0;
    env_smp = 0;
    ended = 0;
    if (a) {
        key_sample(23, a, rate_to_pitch(11025), 0x2000);
        wait_vblanks(3);
        env_smp = SPU_CH_ADSR_VOL(23);
        ended = (SPU_CHAN_STATUS2 >> 7) & 1;          /* must still play */
        wait_vblanks(24);                            /* 0.2 s + margin */
        ended = !ended && ((SPU_CHAN_STATUS2 >> 7) & 1);
        ok_smp = env_smp > 0x4000 && ended;
        SPU_CH_VOL_L(23) = 0;
        SPU_CH_VOL_R(23) = 0;
    }
    s_sfx_top = top0;
    dbg("audio selftest: sample at %05x envelope %04x ended %d %s\n", (unsigned)a, env_smp,
        ended, ok_smp ? "ok" : "FAIL");
    dbg("audio selftest: %s\n", ok_ram && ok_tone && ok_smp ? "PASS" : "FAIL");
    return ok_ram && ok_tone && ok_smp;
}
