/* sfx.c - the sound effects bank of the Dreamcast mixer (DC5; owner:
 * dreamcast; audio_int.h).
 *
 * The core makes its effects as 8-bit PCM (audio_sfx_pcm(), about 48 KB
 * for all of them at 10-22 kHz). At boot each one is copied once into
 * 16-bit PCM in main RAM with a silent guard sample at the end, so the
 * mixer can interpolate between two samples without a bounds test and the
 * core's buffer is never read again (it may live on the disc on other
 * ports). The playback step is rate / 44,100 in Q16. Portable C99: the
 * host test (dc/tests/audio) builds this file too.
 */
#include <stdlib.h>
#include <string.h>
#include "doomo/audio.h"
#include "audio_int.h"

static DcSfx    bank[SFX_COUNT];
static int16_t *pool;
static uint32_t pool_bytes;
static int      loaded;

int dc_sfx_init(void)
{
    uint32_t total = 0, len, rate, at = 0;
    int i, n = 0;
    if (pool) return loaded;              /* once */
    memset(bank, 0, sizeof(bank));
    for (i = SFX_NONE + 1; i < SFX_COUNT; i++)
        if (audio_sfx_pcm(i, &len, &rate) && len) total += len + 1;
    if (!total) return 0;
    pool = (int16_t *)malloc(total * sizeof(int16_t));
    if (!pool) return 0;
    pool_bytes = total * (uint32_t)sizeof(int16_t);
    for (i = SFX_NONE + 1; i < SFX_COUNT; i++) {
        const int8_t *src = audio_sfx_pcm(i, &len, &rate);
        uint32_t k;
        if (!src || !len) continue;
        if (!rate) rate = 10512;          /* the core's default synth rate */
        for (k = 0; k < len; k++) pool[at + k] = (int16_t)(src[k] * 256);
        pool[at + len] = 0;               /* guard */
        bank[i].pcm = &pool[at];
        bank[i].len = len;
        bank[i].step_q16 = (uint32_t)(((uint64_t)rate << 16) / DCSND_RATE);
        at += len + 1;
        n++;
    }
    loaded = n;
    return n;
}

const DcSfx *dc_sfx_get(int sfx)
{
    if (sfx <= SFX_NONE || sfx >= SFX_COUNT || !bank[sfx].pcm) return NULL;
    return &bank[sfx];
}

uint32_t dc_sfx_bytes(void) { return pool_bytes; }
