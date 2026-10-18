/* plat_music_native.c - the native platform's music hooks: musrender
 * (owner: MU1; docs/design/music/SPEC.md sec.9.4, 16.2, 16.3, CF-46).
 *
 * `runner --wav FILE --script S` records what the game plays as a 22,050 Hz
 * 16-bit stereo WAV: the four plat_tone channels as the GBA's PSG would
 * voice them (12.5/25/50 % pulses, a 4-bit triangle and saw, a 15-bit LFSR
 * noise; channel 3's tonal waves as written) and plat_sample's PCM at its
 * own rate. It is the M1 listening reference ("ref" renders): on the weak
 * legacy bridge the engine drives exactly these calls, so the render IS
 * the game's music. Host code (stdio, floats would be allowed here) but it
 * stays integer like the core.
 *
 * On native the engine renders through the weak bridge (plat_weak_music.c);
 * the DoomoVoice-level hooks (hash of every record, mus_synth) come with
 * MU1's later milestones. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "plat_music_native.h"

#define WAV_RATE 22050
#define WAV_PER_FRAME (WAV_RATE / 30)          /* 735 */

static FILE *wav;
static uint32_t wav_samples;
static struct { uint16_t freq; uint8_t vol, wave; uint32_t ph; uint16_t lfsr; } tch[4];
static struct { const int8_t *pcm; uint32_t len, pos_q16, step_q16; uint8_t vol; } smp[4];
static int smp_next;
static int32_t dc_x, dc_y;

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v & 0xFFFF); put16(p + 2, v >> 16); }

static void header(uint32_t frames)
{
    uint8_t h[44];
    uint32_t bytes = frames * 4;
    memcpy(h, "RIFF", 4); put32(h + 4, 36 + bytes); memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16); put16(h + 20, 1); put16(h + 22, 2); put32(h + 24, WAV_RATE);
    put32(h + 28, WAV_RATE * 4); put16(h + 32, 4); put16(h + 34, 16);
    memcpy(h + 36, "data", 4); put32(h + 40, bytes);
    fwrite(h, 1, 44, wav);
}

void nat_wav_close(void)
{
    if (!wav) return;
    fseek(wav, 0, SEEK_SET);
    header(wav_samples);
    fclose(wav);
    wav = NULL;
}

int nat_wav_open(const char *path)
{
    int c;
    nat_wav_close();
    wav = fopen(path, "wb");
    if (!wav) return 0;
    wav_samples = 0;
    header(0);
    memset(tch, 0, sizeof(tch));
    memset(smp, 0, sizeof(smp));
    for (c = 0; c < 4; c++) tch[c].lfsr = 0x7FFF;
    {
        static int registered;
        if (!registered) { registered = 1; atexit(nat_wav_close); }
    }
    return 1;
}

void nat_wav_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave)
{
    if (!wav || channel < 0 || channel > 3) return;
    tch[channel].freq = freq_hz;
    tch[channel].vol = vol > 15 ? 15 : vol;
    tch[channel].wave = wave;
}

void nat_wav_sample(int sfx, uint8_t vol)
{
    uint32_t len = 0, rate = 0;
    const int8_t *pcm;
    if (!wav) return;
    pcm = audio_sfx_pcm(sfx, &len, &rate);
    if (!pcm || !len) return;
    smp[smp_next].pcm = pcm;
    smp[smp_next].len = len;
    smp[smp_next].pos_q16 = 0;
    smp[smp_next].step_q16 = (uint32_t)(((uint64_t)(rate ? rate : 10512) << 16) / WAV_RATE);
    smp[smp_next].vol = vol > 15 ? 15 : vol;
    smp_next = (smp_next + 1) & 3;
}

/* one channel's sample, -15*16 .. 15*16 */
static int tone_sample(int c)
{
    int v = tch[c].vol, w = tch[c].wave, s;
    uint32_t step, ph;
    if (!v || !tch[c].freq) return 0;
    step = (uint32_t)(((uint64_t)tch[c].freq << 32) / WAV_RATE);
    ph = tch[c].ph;
    if (w == WAVE_NOISE) {
        /* the LFSR clocks at the channel "frequency" (the GBA noise's clock) */
        tch[c].ph = ph + step;
        if (tch[c].ph < ph) {
            unsigned bit = (tch[c].lfsr ^ (tch[c].lfsr >> 1)) & 1u;
            tch[c].lfsr = (uint16_t)((tch[c].lfsr >> 1) | (bit << 14));
        }
        s = (tch[c].lfsr & 1) ? 16 : -16;
    } else {
        unsigned x = ph >> 27;                     /* 32 steps a cycle (the wave RAM) */
        tch[c].ph = ph + step;
        switch (w) {
        case WAVE_PULSE12: s = x < 4 ? 16 : -16; break;
        case WAVE_PULSE25: s = x < 8 ? 16 : -16; break;
        case WAVE_TRIANGLE: s = (x < 16 ? (int)x : 31 - (int)x) * 2 - 15; break;
        case WAVE_SAW: s = (int)x - 16; break;
        default: s = x < 16 ? 16 : -16; break;
        }
    }
    return s * v;
}

void nat_wav_frame(void)
{
    static int16_t buf[WAV_PER_FRAME * 2];
    int i, c;
    if (!wav) return;
    for (i = 0; i < WAV_PER_FRAME; i++) {
        int32_t mix = 0;
        for (c = 0; c < 4; c++) mix += tone_sample(c);
        for (c = 0; c < 4; c++) {
            if (!smp[c].pcm) continue;
            if ((smp[c].pos_q16 >> 16) >= smp[c].len) { smp[c].pcm = NULL; continue; }
            mix += smp[c].pcm[smp[c].pos_q16 >> 16] * smp[c].vol / 4;
            smp[c].pos_q16 += smp[c].step_q16;
        }
        /* the DAC's DC block (a one-pole high-pass, about 20 Hz) */
        dc_y = mix * 256 - dc_x + (dc_y * 255 >> 8);
        dc_x = mix * 256;
        mix = (dc_y >> 8) * 24;                    /* 4 loud channels ~ 0.7 full scale */
        if (mix > 32767) mix = 32767;
        if (mix < -32768) mix = -32768;
        buf[i * 2] = buf[i * 2 + 1] = (int16_t)mix;
    }
    fwrite(buf, sizeof(int16_t), WAV_PER_FRAME * 2, wav);
    wav_samples += WAV_PER_FRAME;
}
