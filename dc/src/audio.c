/* audio.c - sound on the Dreamcast: plat_tone, plat_sample and
 * plat_sample_ex through the software mixer (mixer.c) and one KOS sound
 * stream on the AICA (DC5; owner: dreamcast).
 *
 *   main thread    plat_tone / plat_sample* change the mixer's current
 *                  state; dc_audio_frame() (plat_end_frame, every logic
 *                  tick, drawn or not) renders the tick into the PCM ring
 *                  (dcsnd_push: 1,470 frames)
 *   stream thread  polls the KOS stream on every reschedule (thd_sleep
 *                  wakes on KallistiOS's 10 ms tick or sooner); when half
 *                  of the AICA buffer has played KOS calls stream_cb(),
 *                  which copies the next 1,536 frames out of the ring
 *                  (dcsnd_render)
 *
 * 44,100 Hz 16-bit stereo, 6144 B per channel of AICA buffer (3072
 * frames, 70 ms), refilled in halves. The stream thread polls at least
 * every 16.7 ms (measured), so a refill is never more than about 45 ms
 * after the one before, against 70 ms of buffer; with 4096 B the margin
 * was 6.5 ms (docs/DREAMCAST.md section 6, with the latency this costs).
 * Music and effects share the one stream, so they stay in step with each
 * other as on every other port.
 *
 * The core's music engine runs as the legacy bridge (tier AT_BRIDGE, the
 * weak plat_audio_caps of core/src/plat_weak_music.c) like on the GBA,
 * the PS1 and native today: its driver runs 8 mt per tick inside
 * music_update() and its four voices arrive as plat_tone(). Section 6
 * says why, and what the IRQ tier (plat_voice at 240 Hz) will need.
 *
 * Counters go to the harness mailbox after every tick (dc_mailbox.snd_*):
 * the mixer's (ring, underruns, the content hash), the render time per
 * tick on the main thread, and the stream thread's refill intervals. A
 * refill later than the whole AICA buffer (3072 frames after the previous
 * one) means the AICA played stale data: `snd_starved`.
 */
#include <string.h>
#include <kos.h>
#include <dc/sound/sound.h>
#include <dc/sound/stream.h>
#include <arch/timer.h>
#include "dc_plat.h"
#include "doomo/music.h"
#include "audio_int.h"

#define STREAM_BUF  6144                  /* bytes per channel in the AICA      */
#define CHUNK_MAX   (STREAM_BUF / 2)      /* frames per callback at most        */
/* the whole AICA buffer in microseconds: a refill interval beyond it starved */
#define STARVE_US   ((uint32_t)((uint64_t)(STREAM_BUF / 2) * 1000000u / DCSND_RATE))

static snd_stream_hnd_t hnd = SND_STREAM_INVALID;
static int16_t mixbuf[CHUNK_MAX * 2] __attribute__((aligned(32)));
static volatile uint32_t cb_gap_max, cb_starved;         /* stream thread */
static uint64_t cb_last;
static uint32_t us_sum, us_max, us_n;                    /* main thread  */
static volatile uint8_t  running;

static void *stream_cb(snd_stream_hnd_t h, int req_bytes, int *got_bytes)
{
    uint64_t now = timer_us_gettime64();
    int frames = req_bytes / 4;           /* the request is in bytes (stereo s16) */
    (void)h;
    if (running) {                        /* the refills, after the prefill at start */
        if (cb_last) {
            uint32_t gap = (uint32_t)(now - cb_last);
            if (gap > cb_gap_max) cb_gap_max = gap;
            if (gap > STARVE_US) cb_starved++;
        }
        cb_last = now;
    }
    if (frames > CHUNK_MAX) frames = CHUNK_MAX;
    if (frames < 1) frames = 1;
    dcsnd_render(mixbuf, frames);         /* a copy out of the ring */
    *got_bytes = frames * 4;
    return mixbuf;
}

static void *stream_thread(void *p)
{
    (void)p;
    while (running) {
        snd_stream_poll(hnd);
        thd_sleep(2);                     /* until the next scheduler tick */
    }
    return NULL;
}

void dc_audio_init(void)
{
    kthread_attr_t at;
    int n;
    uint64_t t0 = timer_us_gettime64();
    dcsnd_reset();
    dc_music_init();
    n = dc_sfx_init();
    if (snd_stream_init_ex(2, STREAM_BUF) < 0) {
        dc_log("DC: audio: snd_stream_init failed, no sound");
        return;
    }
    hnd = snd_stream_alloc(stream_cb, STREAM_BUF);
    if (hnd == SND_STREAM_INVALID) {
        dc_log("DC: audio: no stream, no sound");
        return;
    }
    snd_stream_volume(hnd, 255);
    snd_stream_start(hnd, DCSND_RATE, 1); /* prefills the AICA buffer (silence) */
    running = 1;
    memset(&at, 0, sizeof(at));
    at.prio = PRIO_DEFAULT - 1;           /* ahead of the game loop when it wakes */
    at.label = "dc_audio";
    at.create_detached = true;
    if (!thd_create_ex(&at, stream_thread, NULL)) {
        running = 0;
        dc_log("DC: audio: no stream thread, no sound");
        return;
    }
    dc_logf("DC: audio: stream %d Hz stereo, %d B AICA buffer, %d effects (%u B), %u us",
            DCSND_RATE, STREAM_BUF, n, (unsigned)dc_sfx_bytes(),
            (unsigned)(timer_us_gettime64() - t0));
}

/* end of every logic tick (main.c plat_end_frame): render the tick */
void dc_audio_frame(void)
{
    uint64_t t0 = timer_us_gettime64();
    uint32_t us;
    dcsnd_push();
    us = (uint32_t)(timer_us_gettime64() - t0);
    us_sum += us;
    us_n++;
    if (us > us_max) us_max = us;
    dc_mailbox.snd_pushed = dcsnd_stats.pushed;
    dc_mailbox.snd_consumed = dcsnd_stats.consumed;
    dc_mailbox.snd_underruns = dcsnd_stats.underruns;
    dc_mailbox.snd_overruns = dcsnd_stats.overruns;
    dc_mailbox.snd_dropped = dcsnd_stats.dropped;
    dc_mailbox.snd_queue_max = dcsnd_stats.queue_max;
    dc_mailbox.snd_gap_frames = dcsnd_stats.gap_frames;
    dc_mailbox.snd_frames = dcsnd_stats.frames;
    dc_mailbox.snd_clips = dcsnd_stats.clips;
    dc_mailbox.snd_hash = dcsnd_stats.hash;
    dc_mailbox.snd_hash600 = dcsnd_stats.hash600;
    dc_mailbox.snd_us_sum = us_sum;
    dc_mailbox.snd_us_n = us_n;
    dc_mailbox.snd_us_max = us_max;
    dc_mailbox.snd_started = dcsnd_stats.started;
    dc_mailbox.snd_cb_gap_max = cb_gap_max;
    dc_mailbox.snd_starved = cb_starved;
}

void dc_audio_stall(int on) { dcsnd_stall(on); }

/* ---- platform.h ----------------------------------------------------------------- */
void plat_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave)
{
    dcsnd_tone(channel, freq_hz, vol, wave);
}

void plat_sample(int sfx_id, uint8_t vol)
{
    dcsnd_trigger(sfx_id, vol, 0, 0, 0);
}

void plat_sample_ex(int sfx, uint8_t vol, int8_t pan, uint8_t prio, int8_t semis)
{
    dcsnd_trigger(sfx, vol, pan, prio, semis);
}

/* a load, a save or a harness hold: no slots for a while, on purpose */
void plat_music_stall(int kind) { dcsnd_stall(kind != STALL_NONE); }
