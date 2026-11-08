/* test_audio.c - unit tests of the Dreamcast sound mixer (DC5; owner:
 * dreamcast; dc/src/mixer.c, sfx.c, audio_int.h). Host gcc, no KOS.
 *
 *   determinism  one tick sequence (tones of every wave, effects with pan,
 *                pitch and priority) played out in whole ticks, in random
 *                chunk sizes, and in random chunks with pauses (empty-ring
 *                gaps, with and without a stall) gives the same content
 *                stream (the samples outside the gaps) and the same hash
 *   pitch        each tone wave at 110 / 440 / 1760 Hz: the zero-crossing
 *                rate matches the frequency (noise: none of that)
 *   levels       a full-volume centred pulse peaks like musrender (+-5,760)
 *   samples      an effect lasts len * 44100 / rate frames; +12 semitones
 *                halves it, -12 doubles it; pan -64 / 64 silences one side
 *   voices       8 voices; a 9th trigger steals the oldest of the lowest
 *                priority, never a higher one
 *   queue        prefill (nothing before 2 slots), underrun (counted,
 *                not during a stall), overrun (a full ring renders nothing
 *                and keeps the triggers), lag (slots skipped beyond
 *                DCSND_MAXLAG; an effect started in one keeps sounding)
 *   silence      no records -> zeros; a silent record -> zeros, no clips
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/platform.h"
#include "doomo/audio.h"
#include "audio_int.h"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t rng = 12345;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* ---- a scripted record sequence ------------------------------------------------ */
#define SEQ_TICKS 240
static void seq_tick(int t)
{
    static const uint8_t waves[6] = { WAVE_PULSE12, WAVE_PULSE25, WAVE_PULSE50, WAVE_TRIANGLE,
                                      WAVE_SAW, WAVE_NOISE };
    int c;
    for (c = 0; c < 4; c++) {
        if ((t + c * 7) % 11 == 0)
            dcsnd_tone(c, (uint16_t)(110 + ((t * 37 + c * 101) % 1500)), (uint8_t)((t + c) % 16),
                       waves[(t / 5 + c) % 6]);
    }
    if (t % 9 == 0) dcsnd_trigger(1 + (t / 9) % (SFX_COUNT - 1), 12, (int8_t)((t % 129) - 64),
                                  (uint8_t)(t % 3), (int8_t)((t % 25) - 12));
    if (t % 23 == 0) dcsnd_trigger(SFX_EXPLODE, 15, 0, 2, 0);
    dcsnd_push();
}

/* the content stream: everything rendered from records, gaps removed */
typedef struct Capture { int16_t *s; size_t n, cap; } Capture;
static void cap_add(Capture *c, const int16_t *s, int frames, int skip_gaps)
{
    int i;
    for (i = 0; i < frames; i++) {
        if (c->n + 2 > c->cap) {
            c->cap = c->cap ? c->cap * 2 : 1 << 16;
            c->s = (int16_t *)realloc(c->s, c->cap * sizeof(int16_t));
        }
        (void)skip_gaps;
        c->s[c->n++] = s[i * 2];
        c->s[c->n++] = s[i * 2 + 1];
    }
}

/* mode 0: whole ticks after each push (prefill 2); 1: random chunks; 2:
 * random chunks with gaps (the producer pauses; stall or not) */
static uint32_t run_seq(int mode, Capture *out, uint32_t *underruns)
{
    static int16_t buf[4096 * 2];
    int t;
    dcsnd_reset();
    for (t = 0; t < SEQ_TICKS; t++) {
        seq_tick(t);
        if (mode == 0) {
            if (dcsnd_queued() >= DCSND_PREFILL) { dcsnd_render(buf, DCSND_PER_TICK); cap_add(out, buf, DCSND_PER_TICK, 0); }
        } else {
            /* render what a stream would: about one tick per push, in odd chunks;
             * mode 2 sometimes lets the queue run dry and renders past it */
            int want = DCSND_PER_TICK + (int)(rnd() % 400) - 150;
            if (mode == 2 && t % 37 == 5) { want += 4 * DCSND_PER_TICK; if (t % 74 == 5) dcsnd_stall(1); }
            while (want > 0) {
                int n = 1 + (int)(rnd() % 1500), k;
                uint32_t gap0 = dcsnd_stats.gap_frames, fr0 = dcsnd_stats.frames;
                if (n > want) n = want;
                dcsnd_render(buf, n);
                /* keep only content: the mixer renders content first, then a gap */
                k = (int)(dcsnd_stats.frames - fr0);
                cap_add(out, buf, k, 0);
                CHECK((int)(dcsnd_stats.gap_frames - gap0) == n - k, "gap accounting");
                want -= n;
            }
        }
    }
    /* play the rest out: two silent records after the sequence */
    {
        int c, guard = 0;
        for (c = 0; c < 4; c++) dcsnd_tone(c, 0, 0, 0);
        dcsnd_push();
        dcsnd_push();
        while (dcsnd_stats.consumed < SEQ_TICKS && guard++ < 1000) {
            uint32_t fr0 = dcsnd_stats.frames;
            dcsnd_render(buf, 1000);
            cap_add(out, buf, (int)(dcsnd_stats.frames - fr0), 0);
        }
    }
    if (underruns) *underruns = dcsnd_stats.underruns;
    return dcsnd_stats.hash;
}

static void test_determinism(void)
{
    Capture a = { 0 }, b = { 0 }, c = { 0 };
    uint32_t ha, hb, hc, ua, ub, uc;
    size_t n;
    ha = run_seq(0, &a, &ua);
    hb = run_seq(1, &b, &ub);
    hc = run_seq(2, &c, &uc);
    n = (size_t)(SEQ_TICKS - 1) * DCSND_PER_TICK * 2;  /* every run plays >= 239 ticks */
    CHECK(a.n >= n && b.n >= n && c.n >= n, "content lengths %zu %zu %zu", a.n, b.n, c.n);
    CHECK(!memcmp(a.s, b.s, n * sizeof(int16_t)), "whole ticks vs random chunks differ");
    CHECK(!memcmp(a.s, c.s, n * sizeof(int16_t)), "whole ticks vs chunks with gaps differ");
    CHECK(ua == 0, "whole ticks: %u underruns", ua);
    CHECK(uc > 0, "gaps without a stall must count underruns (%u)", uc);
    /* the hash is over what was rendered (at push), so the way it was
     * played out does not change it */
    CHECK(ha == hb && ha == hc, "hash depends on the playout: %08x %08x %08x", ha, hb, hc);
    {
        Capture d = { 0 };
        uint32_t hd = run_seq(0, &d, NULL);
        CHECK(hd == ha, "hash not repeatable: %08x %08x", hd, ha);
        free(d.s);
    }
    {
        long nz = 0;
        size_t i;
        for (i = 0; i < n; i++) nz += a.s[i] != 0;
        CHECK(nz > (long)(n / 2), "the sequence is mostly silent (%ld of %zu)", nz, n);
    }
    printf("determinism: %zu samples equal in 3 render patterns; hashes %08x %08x %08x; "
           "underruns %u %u %u\n", n, ha, hb, hc, ua, ub, uc);
    free(a.s); free(b.s); free(c.s);
}

/* ---- render helpers ------------------------------------------------------------------ */
static int16_t big[44100 * 2 * 3];

/* `ticks` records of one state, rendered tick by tick as a stream would
 * (prefill 2); returns frames */
static int render_state(int ticks)
{
    int f = 0;
    dcsnd_push();
    while (f + DCSND_PER_TICK <= (int)(sizeof(big) / 4) && dcsnd_stats.consumed < (uint32_t)ticks) {
        dcsnd_push();
        dcsnd_render(big + f * 2, DCSND_PER_TICK);
        f += DCSND_PER_TICK;
    }
    return f;
}

static void test_pitch(void)
{
    static const uint16_t fq[3] = { 110, 440, 1760 };
    static const char *const nm[5] = { "pulse12", "pulse25", "pulse50", "triangle", "saw" };
    int w, k;
    for (w = WAVE_PULSE12; w <= WAVE_SAW; w++) {
        for (k = 0; k < 3; k++) {
            int f, i, cross = 0, peak = 0;
            double hz;
            dcsnd_reset();
            dcsnd_tone(0, fq[k], 15, (uint8_t)w);
            f = render_state(30);         /* 1 s */
            for (i = 4410; i < f; i++) {  /* skip the filters' first 100 ms */
                int a = big[(i - 1) * 2], b = big[i * 2];
                if (a < 0 && b >= 0) cross++;
                if (abs(b) > peak) peak = abs(b);
            }
            hz = cross * 44100.0 / (f - 4410);
            CHECK(hz > fq[k] * 0.98 && hz < fq[k] * 1.02, "%s %u Hz: measured %.1f Hz", nm[w], fq[k], hz);
            CHECK(peak > 1000, "%s %u Hz: peak %d", nm[w], fq[k], peak);
            CHECK(big[i * 2 - 2] == big[i * 2 - 1], "%s: the lead channel is centred", nm[w]);
        }
    }
    {   /* noise: loud, no single period */
        int f, i, peak = 0;
        dcsnd_reset();
        dcsnd_tone(3, 8000, 15, WAVE_NOISE);
        f = render_state(15);
        for (i = 4410; i < f; i++) if (abs(big[i * 2]) > peak) peak = abs(big[i * 2]);
        CHECK(peak > 2000, "noise peak %d", peak);
        CHECK(dcsnd_stats.clips == 0, "noise clipped");
    }
    {   /* level: a centred 50 % pulse at 15 against musrender's +-16*15*24 = 5760 */
        int f, i, peak = 0;
        dcsnd_reset();
        dcsnd_tone(0, 220, 15, WAVE_PULSE50);
        f = render_state(30);
        for (i = 22050; i < f; i++) if (abs(big[i * 2]) > peak) peak = abs(big[i * 2]);
        CHECK(peak > 5760 * 85 / 100 && peak < 5760 * 115 / 100, "pulse peak %d vs 5760", peak);
        printf("levels: 220 Hz pulse at 15 peaks at %d (musrender 5760)\n", peak);
    }
    {   /* harmony is a little left, kit a little right */
        int i;
        long l = 0, r = 0;
        dcsnd_reset();
        dcsnd_tone(1, 330, 15, WAVE_PULSE50);
        render_state(10);
        for (i = 4410; i < 14700; i++) { l += abs(big[i * 2]); r += abs(big[i * 2 + 1]); }
        CHECK(l > r, "channel 1 sits left (%ld %ld)", l, r);
        dcsnd_reset();
        dcsnd_tone(3, 4000, 15, WAVE_NOISE);
        render_state(10);
        l = r = 0;
        for (i = 4410; i < 14700; i++) { l += abs(big[i * 2]); r += abs(big[i * 2 + 1]); }
        CHECK(r > l, "channel 3 sits right (%ld %ld)", l, r);
    }
}

/* frames one trigger's voice plays (the mixer's count), and the sound on
 * each side of the stream that carries it */
static int effect_frames(int sfx, int8_t pan, int8_t semis, long *left, long *right)
{
    static int16_t fr[DCSND_PER_TICK * 2];
    int i, guard = 0;
    dcsnd_reset();
    dcsnd_trigger(sfx, 15, pan, 0, semis);
    dcsnd_push();
    *left = *right = 0;
    do {                                  /* tick by tick until the voice ends */
        dcsnd_push();
        dcsnd_render(fr, DCSND_PER_TICK);
        for (i = 0; i < DCSND_PER_TICK; i++) { *left += abs(fr[i * 2]); *right += abs(fr[i * 2 + 1]); }
    } while (dcsnd_voices_on() && ++guard < 30 * 4);
    while (dcsnd_stats.consumed < dcsnd_stats.pushed) {   /* play out what is left */
        dcsnd_render(fr, DCSND_PER_TICK);
        for (i = 0; i < DCSND_PER_TICK; i++) { *left += abs(fr[i * 2]); *right += abs(fr[i * 2 + 1]); }
    }
    return (int)dcsnd_stats.voice_frames;
}

static void test_samples(void)
{
    int n = dc_sfx_init(), sfx, tested = 0;
    CHECK(n > 20, "sfx bank: %d effects", n);
    CHECK(dc_sfx_bytes() > 40000, "sfx bank bytes %u", dc_sfx_bytes());
    CHECK(dc_sfx_get(SFX_NONE) == NULL && dc_sfx_get(SFX_COUNT) == NULL && dc_sfx_get(-3) == NULL,
          "out-of-range effects");
    for (sfx = SFX_NONE + 1; sfx < SFX_COUNT; sfx++) {
        const DcSfx *s = dc_sfx_get(sfx);
        uint32_t len, rate;
        const int8_t *pcm = audio_sfx_pcm(sfx, &len, &rate);
        long l, r;
        int fr, expect;
        if (!s) { CHECK(!pcm || !len, "sfx %d missing from the bank", sfx); continue; }
        CHECK(s->len == len && s->pcm[len] == 0 && s->pcm[0] == pcm[0] * 256, "sfx %d copy", sfx);
        fr = effect_frames(sfx, 0, 0, &l, &r);
        expect = (int)(((uint64_t)len << 16) / s->step_q16);
        CHECK(abs(fr - expect) <= 2, "sfx %d: plays %d frames, PCM lasts %d", sfx, fr, expect);
        CHECK(l == r, "sfx %d: pan 0 is centred", sfx);
        tested++;
    }
    CHECK(tested > 10, "only %d effects long enough to time", tested);
    {   /* pitch: +12 halves, -12 doubles (the explosion) */
        long l, r;
        int f0 = effect_frames(SFX_EXPLODE, 0, 0, &l, &r);
        int fu = effect_frames(SFX_EXPLODE, 0, 12, &l, &r);
        int fd = effect_frames(SFX_EXPLODE, 0, -12, &l, &r);
        CHECK(abs(fu * 2 - f0) <= 4, "+12 semitones: %d frames vs %d", fu, f0);
        CHECK(abs(fd - f0 * 2) <= 4, "-12 semitones: %d frames vs %d", fd, f0);
        printf("samples: explode %d frames, +12 st %d, -12 st %d\n", f0, fu, fd);
        effect_frames(SFX_EXPLODE, -64, 0, &l, &r);
        CHECK(l > 1000 && r == 0, "pan -64: left only (%ld %ld)", l, r);
        effect_frames(SFX_EXPLODE, 64, 0, &l, &r);
        CHECK(r > 1000 && l == 0, "pan 64: right only (%ld %ld)", l, r);
    }
}

static void test_voices(void)
{
    int i;
    /* 8 long effects at priority 1, then a 9th at 1 steals, a 10th at 0 cannot */
    dcsnd_reset();
    for (i = 0; i < 8; i++) dcsnd_trigger(SFX_EXPLODE, 10, 0, 1, 0);
    dcsnd_push(); dcsnd_push();
    dcsnd_render(big, 100);
    CHECK(dcsnd_stats.steals == 0, "8 voices: no steal (%u)", dcsnd_stats.steals);
    dcsnd_trigger(SFX_EXPLODE, 10, 0, 1, 0);
    dcsnd_push();
    dcsnd_render(big, DCSND_PER_TICK * 2);
    CHECK(dcsnd_stats.steals == 1, "9th at equal priority steals (%u)", dcsnd_stats.steals);
    dcsnd_trigger(SFX_EXPLODE, 10, 0, 0, 0);
    dcsnd_push();
    dcsnd_render(big, DCSND_PER_TICK);
    CHECK(dcsnd_stats.steals == 1, "a lower priority never steals (%u)", dcsnd_stats.steals);
    /* more triggers in one tick than DCSND_TRIG: the extra are dropped */
    dcsnd_reset();
    for (i = 0; i < DCSND_TRIG + 3; i++) dcsnd_trigger(SFX_CURSOR, 10, 0, 0, 0);
    dcsnd_push(); dcsnd_push();
    dcsnd_render(big, 10);
    CHECK(dcsnd_stats.steals == 0, "%d triggers in a tick fit the voices", DCSND_TRIG);
    /* vol 0, bad ids: nothing */
    dcsnd_reset();
    dcsnd_trigger(SFX_EXPLODE, 0, 0, 0, 0);
    dcsnd_trigger(0, 15, 0, 0, 0);
    dcsnd_trigger(SFX_COUNT, 15, 0, 0, 0);
    dcsnd_trigger(-1, 15, 0, 0, 0);
    dcsnd_push(); dcsnd_push();
    dcsnd_render(big, DCSND_PER_TICK);
    for (i = 0; i < DCSND_PER_TICK * 2 && !big[i]; i++) {}
    CHECK(i == DCSND_PER_TICK * 2, "silent triggers made sound");
}

static void test_queue(void)
{
    int i;
    /* prefill: one record is not enough */
    dcsnd_reset();
    dcsnd_tone(0, 440, 15, WAVE_PULSE50);
    dcsnd_push();
    dcsnd_render(big, 500);
    CHECK(dcsnd_stats.consumed == 0 && dcsnd_stats.gap_frames == 500, "prefill: played early");
    for (i = 0; i < 1000 && !big[i]; i++) {}
    CHECK(i == 1000, "prefill gap is silent");
    dcsnd_push();
    dcsnd_render(big, DCSND_PER_TICK * 2);
    CHECK(dcsnd_stats.consumed == 2 && dcsnd_stats.frames == DCSND_PER_TICK * 2, "two records played");
    CHECK(dcsnd_stats.underruns == 0, "no underrun yet");
    /* the queue is empty: an underrun, then silence until 2 more */
    dcsnd_render(big, 300);
    CHECK(dcsnd_stats.underruns == 1, "underrun counted (%u)", dcsnd_stats.underruns);
    dcsnd_push();
    dcsnd_render(big, 300);
    CHECK(dcsnd_stats.consumed == 2, "re-primes after an underrun");
    dcsnd_push();
    dcsnd_render(big, DCSND_PER_TICK * 2);
    CHECK(dcsnd_stats.consumed == 4, "plays again");
    /* a stall: the empty queue is not an underrun, and the stall ends with a push */
    dcsnd_stall(1);
    dcsnd_render(big, 600);
    CHECK(dcsnd_stats.underruns == 1, "stall: no underrun (%u)", dcsnd_stats.underruns);
    dcsnd_push();
    dcsnd_push();
    dcsnd_render(big, DCSND_PER_TICK * 2);
    dcsnd_render(big, 10);
    CHECK(dcsnd_stats.underruns == 2, "after the stall, underruns count again (%u)", dcsnd_stats.underruns);
    /* overrun: a full ring renders nothing and keeps the triggers */
    dcsnd_reset();
    for (i = 0; i < DCSND_QUEUE; i++) CHECK(dcsnd_push() == 1, "push %d", i);
    dcsnd_trigger(SFX_EXPLODE, 15, 0, 0, 0);
    CHECK(dcsnd_push() == 0 && dcsnd_stats.overruns == 1, "overrun");
    CHECK(dcsnd_stats.pushed == DCSND_QUEUE && dcsnd_stats.started == 0, "overrun rendered");
    CHECK(dcsnd_stats.queue_max == DCSND_QUEUE, "queue max %u", dcsnd_stats.queue_max);
    /* lag: 16 waiting -> slots beyond DCSND_MAXLAG skipped */
    dcsnd_render(big, 1);
    CHECK(dcsnd_stats.dropped == DCSND_QUEUE - DCSND_MAXLAG, "dropped %u", dcsnd_stats.dropped);
    CHECK(dcsnd_queued() == DCSND_MAXLAG, "ring after the catch-up %d (the head is playing)", dcsnd_queued());
    CHECK(dcsnd_push() == 1 && dcsnd_stats.started == 1, "the kept trigger plays at the next push");
    /* an effect started in a skipped slot keeps sounding in the next ones */
    dcsnd_reset();
    dcsnd_trigger(SFX_EXPLODE, 15, 0, 0, 0);
    for (i = 0; i < DCSND_MAXLAG + 3; i++) dcsnd_push();
    dcsnd_render(big, DCSND_PER_TICK);
    CHECK(dcsnd_stats.dropped == 3, "dropped %u", dcsnd_stats.dropped);
    for (i = 0; i < DCSND_PER_TICK * 2 && abs(big[i]) < 200; i++) {}
    CHECK(i < DCSND_PER_TICK * 2, "an effect from a skipped slot keeps sounding");
    /* silence: zero tones, no clips */
    dcsnd_reset();
    render_state(10);
    for (i = 0; i < DCSND_PER_TICK * 20 && !big[i]; i++) {}
    CHECK(i == DCSND_PER_TICK * 20 && dcsnd_stats.clips == 0, "silent records are silent");
}

static void test_loud(void)
{
    /* everything at once: 4 full tones + 8 full explosions: the samples over
     * the rail are counted, and the knee keeps every one off the rails */
    int i, c, f, rail = 0, over = 0, peak = 0;
    dcsnd_reset();
    for (c = 0; c < 4; c++) dcsnd_tone(c, (uint16_t)(100 + c * 50), 15, WAVE_PULSE50);
    for (i = 0; i < 8; i++) dcsnd_trigger(SFX_EXPLODE, 15, 0, 0, 0);
    f = render_state(20);
    for (i = 0; i < f * 2; i++) {
        int a = abs(big[i]);
        rail += big[i] == 32767 || big[i] == -32768;
        over += a > 24576;
        if (a > peak) peak = a;
    }
    CHECK(dcsnd_stats.clips > 0 && rail == 0 && over >= (int)dcsnd_stats.clips,
          "over the rail %u, at the rails %d, over the knee %d", dcsnd_stats.clips, rail, over);
    CHECK(peak > 31000, "the knee leaves headroom unused (peak %d)", peak);
    printf("loudest: %u of %d samples over the rail, %d over the knee, peak %d, none at the rails\n",
           dcsnd_stats.clips, f * 2, over, peak);
    /* below the knee nothing changes: a full pulse at 15 peaks under it
     * (test_pitch's levels check sees the same 6,293 as before the knee) */
}

int main(void)
{
    audio_init();
    dc_sfx_init();
    test_determinism();
    test_pitch();
    test_samples();
    test_voices();
    test_queue();
    test_loud();
    printf("test_audio: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
