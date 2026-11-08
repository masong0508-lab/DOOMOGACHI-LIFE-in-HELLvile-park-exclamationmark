/* sndtee.c - the Dreamcast sound mixer on the native runner (DC5; owner:
 * dreamcast).
 *
 * The native runner (core/native) is linked with its sound calls renamed to
 * ref_* (plat_native.c: plat_tone, plat_sample, plat_end_frame; the last
 * one drives musrender, `runner --wav`). This file defines the real ones:
 * each call goes to the reference AND to dc/src/mixer.c, and every logic
 * tick is pushed (rendered into one slot of 1,470 frames at 44,100 Hz) and
 * played out as the Dreamcast stream would. One runner script then gives
 * two WAVs:
 *   --wav FILE         musrender, 22,050 Hz (the reference)
 *   DCSND_WAV=FILE     the Dreamcast mixer, 44,100 Hz stereo
 * and dc/tests/audio/compare.py compares them. The stream plays a slot
 * once two are ready (DCSND_PREFILL), as on the console, and the ring is
 * played out at exit, so frame block k of the Dreamcast WAV is tick k, as
 * in the reference.
 * DCSND_STATS=FILE appends the mixer's counters at exit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_int.h"

void ref_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave);
void ref_sample(int sfx_id, uint8_t vol);
void ref_end_frame(void);

static FILE *wav;
static uint32_t wav_frames;
static int inited;
static int16_t buf[DCSND_PER_TICK * 2];

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v & 0xFFFF); put16(p + 2, v >> 16); }

static void header(void)
{
    uint8_t h[44];
    uint32_t bytes = wav_frames * 4;
    memcpy(h, "RIFF", 4); put32(h + 4, 36 + bytes); memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16); put16(h + 20, 1); put16(h + 22, 2); put32(h + 24, DCSND_RATE);
    put32(h + 28, DCSND_RATE * 4); put16(h + 32, 4); put16(h + 34, 16);
    memcpy(h + 36, "data", 4); put32(h + 40, bytes);
    fseek(wav, 0, SEEK_SET);
    fwrite(h, 1, 44, wav);
    fseek(wav, 0, SEEK_END);
}

static void render_tick(void)
{
    dcsnd_render(buf, DCSND_PER_TICK);
    if (wav) {
        fwrite(buf, sizeof(int16_t), DCSND_PER_TICK * 2, wav);
        wav_frames += DCSND_PER_TICK;
    }
}

static void finish(void)
{
    const char *sp = getenv("DCSND_STATS");
    int n;
    for (n = dcsnd_queued(); n > 0; n--) render_tick();   /* play the ring out */
    if (wav) { header(); fclose(wav); wav = NULL; }
    if (sp) {
        FILE *f = fopen(sp, "a");
        if (f) {
            fprintf(f, "slots %u/%u underruns %u overruns %u dropped %u queue_max %u "
                    "frames %u gaps %u clips %u effects %u steals %u hash 0x%08x hash600 0x%08x\n",
                    dcsnd_stats.consumed, dcsnd_stats.pushed, dcsnd_stats.underruns,
                    dcsnd_stats.overruns, dcsnd_stats.dropped, dcsnd_stats.queue_max,
                    dcsnd_stats.frames, dcsnd_stats.gap_frames, dcsnd_stats.clips,
                    dcsnd_stats.started, dcsnd_stats.steals, dcsnd_stats.hash, dcsnd_stats.hash600);
            fclose(f);
        }
    }
}

static void init(void)
{
    const char *wp = getenv("DCSND_WAV");
    if (inited) return;
    inited = 1;
    dcsnd_reset();
    dc_sfx_init();
    if (wp && *wp) {
        wav = fopen(wp, "wb");
        if (!wav) { fprintf(stderr, "sndtee: cannot write %s\n", wp); exit(2); }
        header();
    }
    atexit(finish);
}

void plat_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave)
{
    init();
    ref_tone(channel, freq_hz, vol, wave);
    dcsnd_tone(channel, freq_hz, vol, wave);
}

void plat_sample(int sfx_id, uint8_t vol)
{
    init();
    ref_sample(sfx_id, vol);
    dcsnd_trigger(sfx_id, vol, 0, 0, 0);
}

void plat_end_frame(void)
{
    init();
    ref_end_frame();
    dcsnd_push();
    if (dcsnd_queued() >= DCSND_PREFILL) render_tick();
}
