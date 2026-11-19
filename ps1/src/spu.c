/* spu.c - SPU audio: 4 hybrid instrument tone channels + one-shot samples.
 *
 * At boot, instrument_bank.h builds five fixed-point melodic patches (glass bell,
 * electric piano, organ, round bass and dark string pad) plus a noise loop. The
 * cycles are encoded to SPU ADPCM by our encoder and uploaded as looping samples.
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
 *   0x02000-0x4ffff  sound effects (bump allocator; about 35 KB used)
 *   0x50000-0x7dfff  the part scripts (CR10, partstore.c: PARTS.BIN, 143,360 B today)
 *   0x7e000-0x7ffff  the reverb work area (reverb.h: Studio Small at 0x7e0c0; off until plat_reverb())
 */
#include <string.h>
#include <psxapi.h>
#include <psxspu.h>
#include <hwregs_c.h>
#include "ps1.h"
#include "doomo/plat_ext.h"
#include "adpcm.h"
#include "waveform.h"
#include "instrument_bank.h"
#include "reverb.h"
#include "tonepick.h"

#define SPU_WAVE_BASE   0x1000
#define SPU_SFX_BASE    0x2000
#define SPU_SFX_END     SPU_PARTS_BASE      /* CR10: the part store (partstore.c) is above the effects */
#define TONE_VOICES     4
#define SFX_VOICE0      4
#define SFX_VOICES      20

/* waveform cycle lengths: five zones, long for low notes, short for high ones (tonepick.h) */
static uint32_t s_wave_addr[WAVE_COUNT][PS1_ZONES];   /* [wave][zone]; the noise has one loop, in every slot */
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

/* ---- staging -------------------------------------------------------------
 * PCM -> ADPCM staging, shared by the boot-time waveforms and the converted
 * effects (load_pcm8): the PCM of one chunk, its ADPCM blocks, and the
 * word-aligned copy the SPU DMA wants (spu_upload).
 * (RAM headroom 2026-11-12) On the game disc every conversion runs at boot,
 * before the first game frame: the waveforms (audio_hw_init) and all of
 * SFX.BIN (audio_hw_sfx_preload). So the three buffers are no .bss there
 * (8.7 KB): stage_open() takes them from the display lists, which are idle
 * until the game loop starts (plat_save_scratch, gfx.c), and stage_close()
 * drops them. Nothing converts later (every effect is ready or failed by
 * then); a conversion that found no buffer would fail like a full SPU RAM.
 * The engine test also converts at run time (its self test) and keeps the
 * buffers as statics. */
#define STAGE_PCM    (28 * 64)          /* samples per chunk       */
#define STAGE_ADPCM  (64 * 16)          /* their ADPCM blocks      */
#define STAGE_UP     4096               /* bytes per DMA transfer  */
#if defined(DOOMO_SFX_ON_DISC) && !defined(DOOMO_ENGINE_TEST)
static int16_t  *s_pcm;
static uint8_t  *s_adpcm;
static uint32_t *s_up;
typedef char stage_fits[STAGE_UP + STAGE_PCM * 2 + STAGE_ADPCM <= 2 * PRIM_BYTES ? 1 : -1];

static void stage_open(void)
{
    uint8_t *b = (uint8_t *)plat_save_scratch(NULL);
    s_up = (uint32_t *)b;
    s_pcm = (int16_t *)(b + STAGE_UP);
    s_adpcm = b + STAGE_UP + STAGE_PCM * 2;
}

static void stage_close(void)
{
    s_up = NULL;
    s_pcm = NULL;
    s_adpcm = NULL;
}
#else
static int16_t  s_pcm[STAGE_PCM];
static uint8_t  s_adpcm[STAGE_ADPCM];
static uint32_t s_up[STAGE_UP / 4];
static void stage_open(void) {}
static void stage_close(void) {}
#endif

/* ---- upload ------------------------------------------------------------- */
static uint32_t s_up_sum = 2166136261u;    /* FNV-1a of every byte sent to SPU RAM (the boot log) */

static void spu_upload(uint32_t addr, const void *data, size_t bytes)
{
    /* DMA wants 64-byte chunks of word-aligned data */
    uint32_t *tmp = s_up;
    const uint8_t *src = (const uint8_t *)data;
    s_up_sum = sum_bytes(s_up_sum, src, bytes);
    while (bytes) {
        size_t n = bytes > STAGE_UP ? STAGE_UP : bytes;
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

/* ---- waveforms ------------------------------------------------------------
 * The pulse, saw and triangle cycles come from waveform.h (band-limited, no DC offset, fade-in on
 * the first cycle, loop encoded for the SPU's real decoder history). Only the noise is drawn here. */
static int16_t noise_sample(uint32_t *rng)
{
    *rng = *rng * 1103515245u + 12345u;
    return (int16_t)(((*rng >> 16) & 1) ? 11000 : -11000);
}

#define NOISE_LEN (28 * 64)
typedef char noise_fits[NOISE_LEN <= STAGE_PCM ? 1 : -1];

static void build_waves(void)
{
    int16_t *pcm = s_pcm;
    uint8_t *adpcm = s_adpcm;
    uint32_t addr = SPU_WAVE_BASE, rng = 0x1234567u, rms28 = 0;
    int w, v, i;
    /* a silent looping block (idle voices point here) */
    memset(adpcm, 0, 16);
    adpcm[1] = 0x07;          /* loop start + end + repeat */
    spu_upload(addr, adpcm, 16);
    s_wave_sum = sum_bytes(2166136261u, adpcm, 16);
    s_silent_addr = addr;
    addr += 16;
    for (w = 0; w < WAVE_COUNT; w++) {
        for (v = 0; v < PS1_ZONES; v++) {
            int len = w == WAVE_NOISE ? NOISE_LEN : ps1_zone_cyc[v];
            int blocks;
            if (w == WAVE_NOISE && v) {
                s_wave_addr[w][v] = s_wave_addr[w][0];
                continue;
            }
            if (w == WAVE_NOISE) {
                for (i = 0; i < len; i++)
                    pcm[i] = noise_sample(&rng);
                blocks = adpcm_encode(pcm, len, adpcm, 0);
                adpcm[1] |= 0x04;                   /* loop start */
            } else {
                /* two cycles: the first fades in from silence and plays once, the loop restarts
                 * at the second (waveform.h; its blocks are encoded for the decoder history the
                 * loop jump really arrives with; the first block of a sample uses filter 0) */
                /* PS1-only hybrid patch bank. The stable WAVE_* IDs select a bell,
                 * electric tine, organ, round bass / flute or dark string pad. The
                 * noise kit remains the original noise loop. Bank sizes stay within
                 * the existing 0x1000-0x1fff SPU waveform allocation. */
                if (!ps1_instrument_cycle((uint8_t)w, pcm, len))
                    ps1_wave_triangle(pcm, len); /* defensive fallback for an unknown ID */
                /* the 14 and 7 sample cycles are scaled to the level of the 28 sample one (waveform.h) */
                if (v == 2)
                    rms28 = ps1_wave_ms(pcm, len);
                else if (v > 2)
                    ps1_wave_match(pcm, len, rms28);
                blocks = ps1_wave_encode_cycle(pcm, len, adpcm);   /* sets loop start + end flags */
                spu_upload(addr, adpcm, (size_t)blocks * 16);
                s_wave_sum = sum_bytes(s_wave_sum, adpcm, (size_t)blocks * 16);
                s_wave_addr[w][v] = addr;
                addr += (uint32_t)blocks * 16;
                continue;
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
    /* pitch 0x1000 = 44100 Hz sample rate; sample rate = hz * cycle (tonepick.h) */
    return ps1_tone_pitch(hz, (uint32_t)cyc);
}

/* ST-ADV Mix view pan (0 left, 1 middle, 2 right): the far side keeps a quarter of the level, so a panned lane stays heard
 * on a mono output. The default is the same level on both sides, as always. */
static uint8_t s_tone_pan[TONE_VOICES] = { 1, 1, 1, 1 };

static void tone_write_vol(int channel, int amp)
{
    int p = s_tone_pan[channel];
    SPU_CH_VOL_L(channel) = (uint16_t)(p == 2 ? amp / 4 : amp);
    SPU_CH_VOL_R(channel) = (uint16_t)(p == 0 ? amp / 4 : amp);
}

const char *plat_tone_name(uint8_t wave)
{
    if (wave < WAVE_NOISE)
        return ps1_patches[wave].name;
    if (wave == WAVE_NOISE)
        return "Noise Percussion";
    return NULL;
}

void plat_tone_pan(int channel, int pan)
{
    if (channel < 0 || channel >= TONE_VOICES) return;
    s_tone_pan[channel] = (uint8_t)(pan < 0 ? 0 : pan > 2 ? 2 : pan);
    if (s_ready && s_tone[channel].on)
        tone_write_vol(channel, (s_tone[channel].vol * s_music_vol * 0x2000) / (15 * 15));
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
        /* a note that is already sounding this waveform keeps its cycle while the pitch register
         * reaches it: another cycle would mean a key-on, a click (tonepick.h) */
        variant = ps1_tone_variant(freq_hz, t->on && t->wave == wave, t->variant);
        cyc = ps1_zone_cyc[variant];
        pitch = hz_to_pitch(freq_hz, cyc);
    }
    amp = (vol * s_music_vol * 0x2000) / (15 * 15);
    tone_write_vol(channel, amp);
    SPU_CH_FREQ(channel) = pitch;
    if (!t->on || t->wave != wave || t->variant != variant) {
        SPU_CH_ADDR(channel) = getSPUAddr(s_wave_addr[wave][variant]);
        SPU_CH_ADSR1(channel) = ps1_instrument_adsr1(wave);
        SPU_CH_ADSR2(channel) = ps1_instrument_adsr2(wave);
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
    int16_t *pcm = s_pcm;
    uint8_t *adpcm = s_adpcm;
    uint32_t done = 0, addr = s_sfx_top, bytes;
    if (len > 44100u * 3u)
        len = 44100u * 3u;
    bytes = (len + 27) / 28 * 16 + 16;
    if (s_sfx_top + bytes > SPU_SFX_END || !pcm)
        return 0;
    /* chunks of 64 blocks; the encoder state restarts per chunk (filter 0
     * on each chunk's first block), inaudible at these lengths */
    while (done < len) {
        uint32_t n = len - done, i;
        int blocks;
        if (n > STAGE_PCM) n = STAGE_PCM;
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
    stage_open();
    build_waves();
    stage_close();
    for (v = 0; v < 24; v++) {
        SPU_CH_VOL_L(v) = 0;
        SPU_CH_VOL_R(v) = 0;
        SPU_CH_ADDR(v) = getSPUAddr(s_silent_addr);
    }
    memset(s_sfx, 0, sizeof(s_sfx));
    s_ready = 1;
}

/* SFX.BIN is on the disc (sfxdisc.c): convert every effect once at boot,
 * right after the CD is up, instead of one disc read per frame later. */
void audio_hw_sfx_preload(void)
{
#ifdef DOOMO_SFX_ON_DISC
    int id, ok = 0;
    if (!s_ready)
        return;
    stage_open();
    for (id = 1; id < SFX_COUNT; id++)
        ok += convert_sfx(id);
    stage_close();
    s_bg_next = SFX_COUNT;
    /* what is in SPU RAM now: the same line from two builds = the same sound data.
     * "(all)" is what harness/scripts/game_smoke.txt expects: an effect that was not
     * converted here stays silent (nothing converts after boot, see "staging" above) */
    dbg("spu: %d of %d effects (%s), top %05x, sum %08x\n", ok, SFX_COUNT - 1,
        ok == SFX_COUNT - 1 ? "all" : "SOME MISSING", (unsigned)s_sfx_top, (unsigned)s_up_sum);
#endif
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

/* ---- reverb (MS-S4) ------------------------------------------------------------
 * The work area at the top of SPU RAM is zeroed and the preset written the first time a mode other
 * than OFF is asked for. Only the four tone voices (0-3) send to the reverb; effects and babble
 * stay dry. OFF clears the send and the master enable, so the default build sounds as before. */
#define SPU_REG16(a) (*(volatile uint16_t *)(a))
static int s_reverb_mode;
typedef char reverb_modes_match[(int)REVERB_ROOM == (int)PLAT_REVERB_ROOM && (int)REVERB_HALL == (int)PLAT_REVERB_HALL ? 1 : -1];

void plat_reverb(int mode)
{
    static int loaded;
    int i;
    if (!s_ready || mode < 0 || mode >= REVERB_COUNT || mode == s_reverb_mode)
        return;
    if (mode != REVERB_OFF && !loaded) {
        static const uint32_t zero[64 / 4];
        uint32_t a;
        for (a = PS1_REVERB_AREA_START; a < PS1_REVERB_AREA_END; a += 64) {   /* 128 x 64 B */
            SpuSetTransferStartAddr(a);
            SpuWrite((uint32_t *)zero, 64);
            SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
        }
        SPU_REG16(PS1_REVERB_MBASE_REG) = ps1_reverb_studio_small.mbase;
        for (i = 0; i < PS1_REVERB_REGS; i++)
            SPU_REG16(PS1_REVERB_REG_BASE + 2u * (uint32_t)i) = ps1_reverb_studio_small.reg[i];
        loaded = 1;
    }
    SPU_REG16(PS1_REVERB_VOL_L_REG) = (uint16_t)ps1_reverb_level(mode);
    SPU_REG16(PS1_REVERB_VOL_R_REG) = (uint16_t)ps1_reverb_level(mode);
    SPU_REG16(PS1_REVERB_EON1_REG) = mode == REVERB_OFF ? 0 : ((1u << TONE_VOICES) - 1u);
    SPU_REG16(PS1_REVERB_EON2_REG) = 0;
    if (mode == REVERB_OFF)
        SPU_REG16(PS1_REVERB_SPUCNT_REG) &= (uint16_t)~PS1_REVERB_SPUCNT_EN;
    else
        SPU_REG16(PS1_REVERB_SPUCNT_REG) |= PS1_REVERB_SPUCNT_EN;
    s_reverb_mode = mode;
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
