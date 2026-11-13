/* test_tier.c - the music engine's two tiers on the Dreamcast mixer (DC5b;
 * owner: dreamcast). Host gcc, no KOS. Built twice:
 *
 *   tier_irq     -DTIER_IRQ, with dc/src/musvoice.c: CAP_DRV_IRQ, the
 *                driver ticked 8 times a slot inside dcsnd_push(), the
 *                voices as DoomoVoice records (what the console runs)
 *   tier_bridge  without it: the core's weak legacy bridge, the music as
 *                plat_tone (what the console ran before DC5b, and what a
 *                DC_MUSIC_IRQ=0 build runs)
 *
 *   tier_X list                         the song table: "N ticks name"
 *   tier_X song N TICKS OUT.raw KEYS    play song N for TICKS logic ticks:
 *                                       the mixer's stream (s16 stereo,
 *                                       44,100 Hz) and the driver's key-on
 *                                       count after every tick
 *   tier_irq unit                       the tier's unit tests
 *
 * tier_irq taps plat_voice / plat_audio_bus / plat_audio_fx (musvoice.c is
 * compiled with those three renamed to dc_*): every call also goes to the
 * reference renderer (voiceref.c) at the frame it takes effect, and `song`
 * compares the two streams sample by sample ("ref maxdiff").
 * run_tier.sh plays every song on both and tier_compare.py gates the pair.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/music.h"
#include "../../../core/src/mus_int.h"
#include "../../../core/src/audio_int.h"
#include "audio_int.h"
#include "voiceref.h"

void ref_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave);   /* plat_native.c, unused */

/* ---- the platform's sound calls ------------------------------------------------------ */
static DoomoToneState last_tone[4];
static int tone_calls;

void plat_tone(int channel, uint16_t freq_hz, uint8_t vol, uint8_t wave)
{
    if (channel >= 0 && channel < 4) {
        last_tone[channel].freq = freq_hz;
        last_tone[channel].vol = vol;
        last_tone[channel].wave = wave;
    }
    tone_calls++;
    dcsnd_tone(channel, freq_hz, vol, wave);
}
void plat_sample(int sfx_id, uint8_t vol) { dcsnd_trigger(sfx_id, vol, 0, 0, 0); }
void plat_end_frame(void) { }

#ifdef TIER_IRQ
void dc_plat_voice(int voice, const DoomoVoice *v);
void dc_plat_audio_bus(int bus, uint8_t gain_q8);
void dc_plat_audio_fx(int fx, int a, int b);
void dc_music_tape(int down16);                 /* musvoice.c's S1 outputs: tapped here */
void dc_music_bus(uint8_t gain);

static DoomoVoice last_rec[DCSND_MVOICES];
static uint32_t n_rec, n_keyon;
static int last_bus = 255, last_tape, max_tape, timbre_seen[TIMBRE_COUNT], kit_seen;
static int tap_frame = -1;               /* unit: the frame of the first record */
static int cur_frame;

void plat_voice(int voice, const DoomoVoice *v)
{
    if (voice >= 0 && voice < DCSND_MVOICES) last_rec[voice] = *v;
    n_rec++;
    if (v->flags & DVF_KEYON) n_keyon++;
    if (v->inst >= MUS_INST_KIT0) kit_seen++;
    else if (v->pitch16) timbre_seen[MUS_INST(v->inst)->timbre]++;
    if (tap_frame < 0) tap_frame = cur_frame;
    ref_voice(voice, v);
    dc_plat_voice(voice, v);
}
void plat_audio_bus(int bus, uint8_t g)
{
    if (bus == 0) { last_bus = g; ref_bus(g); }
    dc_plat_audio_bus(bus, g);
}
void plat_audio_fx(int fx, int a, int b) { dc_plat_audio_fx(fx, a, b); }
void dc_music_bus(uint8_t g) { last_bus = g; ref_bus(g); dcsnd_music_gain(g); }
void dc_music_tape(int a)
{
    last_tape = a; if (a > max_tape) max_tape = a; ref_tape(a);
    dcsnd_music_tape(a);
}
#endif

/* ---- one run: ticks of the game's audio into two streams ---------------------------------- */
#ifndef TIER_IRQ
static int cur_frame;
#endif
static int16_t *mix, *refb;
static size_t mix_n, ref_n, cap_frames;
static int seg_k, pending;
#ifdef TIER_IRQ
static void (*inner_tick)(void);
#endif

#ifdef TIER_IRQ
static void ref_flush(void)
{
    if (pending && refb && ref_n + (size_t)pending <= cap_frames) {
        ref_render(refb + ref_n * 2, pending);
        ref_n += (size_t)pending;
    }
    cur_frame += pending;
    pending = 0;
}
#else
static void ref_flush(void) { }
#endif

#ifdef TIER_IRQ
/* the driver as the mixer calls it: mt k of the slot, before its frames */
static void tap_tick(void)
{
    ref_flush();
    if (inner_tick) inner_tick();
    pending = (seg_k + 1) * DCSND_PER_TICK / DCSND_MT - seg_k * DCSND_PER_TICK / DCSND_MT;
    seg_k = (seg_k + 1) % DCSND_MT;
}
#endif

static void run_begin(int ticks)
{
    cap_frames = (size_t)(ticks + DCSND_QUEUE) * DCSND_PER_TICK;
    free(mix); free(refb);
    mix = calloc(cap_frames * 2, sizeof(int16_t));
    refb = calloc(cap_frames * 2, sizeof(int16_t));
    if (!mix || !refb) { fprintf(stderr, "test_tier: out of memory\n"); exit(2); }
    mix_n = ref_n = 0;
    seg_k = pending = 0;
    cur_frame = 0;
    dcsnd_reset();
#ifdef TIER_IRQ
    ref_reset();
    dc_music_sync_reset();
    inner_tick = dc_music_tick;
    dcsnd_set_driver(tap_tick);
    memset(last_rec, 0, sizeof(last_rec));
    last_bus = 255;
    last_tape = max_tape = 0;
    tap_frame = -1;
#else
    dcsnd_set_driver(NULL);
#endif
}

static void take_slot(void)
{
    if (mix_n + DCSND_PER_TICK > cap_frames) return;
    dcsnd_render(mix + mix_n * 2, DCSND_PER_TICK);
    mix_n += DCSND_PER_TICK;
}

/* the end of a logic tick: render it (the platform's plat_end_frame) */
static void end_tick(void)
{
    dcsnd_push();
    ref_flush();
#ifndef TIER_IRQ
    cur_frame += DCSND_PER_TICK;
#endif
    if (dcsnd_queued() >= DCSND_PREFILL) take_slot();
}

static void run_end(void)
{
    int n;
    for (n = dcsnd_queued(); n > 0; n--) take_slot();
}

static void game_reset(void)
{
    opt_defaults();
    audio_init();
}

#ifdef TIER_IRQ
static long ref_maxdiff(void)
{
    size_t i, n = (mix_n < ref_n ? mix_n : ref_n) * 2;
    long worst = 0;
    for (i = 0; i < n; i++) {
        long d = labs((long)mix[i] - (long)refb[i]);
        if (d > worst) worst = d;
        if (d > 1 && getenv("TDBG")) { printf("first diff at %zu (frame %zu): mix %d ref %d\n", i, i/2, mix[i], refb[i]); getenv("TDBG")[0]=0; break; }
    }
    if (mix_n != ref_n) worst = 99999;
    return worst;
}
#endif

static int cmd_list(void)
{
    int s;
    game_reset();
    for (s = SONG_NONE + 1; s < SONG_COUNT; s++)
        printf("%d %u %s\n", s, (unsigned)audio_song_length(s), audio_song_name(s));
    return 0;
}

static int cmd_song(int song, int ticks, const char *out, const char *keys)
{
    FILE *f, *k;
    int t;
    game_reset();
    run_begin(ticks);
    k = fopen(keys, "w");
    if (!k) { fprintf(stderr, "test_tier: cannot write %s\n", keys); return 2; }
    audio_music(song);
    for (t = 0; t < ticks; t++) {
        audio_tick();
        end_tick();
        fprintf(k, "%u\n", (unsigned)mus_drv_keyons());
    }
    fclose(k);
    run_end();
    f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "test_tier: cannot write %s\n", out); return 2; }
    fwrite(mix, sizeof(int16_t) * 2, mix_n, f);
    fclose(f);
#ifdef TIER_IRQ
    {
        long d = ref_maxdiff();
        int i;
        printf("song %d ticks %d frames %u records %u keyons %u underruns %u clips %u ref_maxdiff %ld hash 0x%08x timbres",
               song, ticks, (unsigned)mix_n, (unsigned)n_rec, (unsigned)mus_drv_keyons(),
               (unsigned)mus_drv_underruns(), (unsigned)dcsnd_stats.clips, d, (unsigned)dcsnd_stats.hash);
        for (i = 0; i < TIMBRE_COUNT; i++) if (timbre_seen[i]) printf(" %d:%d", i, timbre_seen[i]);
        printf(" kit:%d\n", kit_seen);
        return d <= 1 && mus_drv_underruns() == 0 ? 0 : 1;
    }
#else
    printf("song %d ticks %d frames %u keyons %u clips %u hash 0x%08x\n", song, ticks, (unsigned)mix_n,
           (unsigned)mus_drv_keyons(), (unsigned)dcsnd_stats.clips, (unsigned)dcsnd_stats.hash);
    return 0;
#endif
}

#ifdef TIER_IRQ
/* ---- unit tests --------------------------------------------------------------------------- */
static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

static int inst_with(int timbre)
{
    int i;
    for (i = 0; i < (int)mus_bank()->n_inst; i++)
        if (mus_bank_inst(i)->timbre == timbre) return i;
    return -1;
}

static DoomoVoice rec(int inst, int p16, int vl, int vr, int tparam, int flags)
{
    DoomoVoice r;
    memset(&r, 0, sizeof(r));
    r.pitch16 = (uint16_t)p16;
    r.vol_l = (uint8_t)vl;
    r.vol_r = (uint8_t)vr;
    r.env = 240;
    r.inst = (uint8_t)inst;
    r.tparam = (uint8_t)tparam;
    r.flags = (uint8_t)flags;
    return r;
}

/* a scripted driver: records by (slot, mt) */
typedef struct Step { int slot, mt, voice; DoomoVoice r; } Step;
static const Step *sc_steps;
static int sc_n, sc_slot, sc_mt;
static void script_tick(void)
{
    int i;
    for (i = 0; i < sc_n; i++)
        if (sc_steps[i].slot == sc_slot && sc_steps[i].mt == sc_mt) plat_voice(sc_steps[i].voice, &sc_steps[i].r);
    if (++sc_mt == DCSND_MT) { sc_mt = 0; sc_slot++; }
}

static void play_script(const Step *st, int n, int slots)
{
    int t;
    run_begin(slots);
    inner_tick = script_tick;
    sc_steps = st; sc_n = n; sc_slot = sc_mt = 0;
    for (t = 0; t < slots; t++) end_tick();
    run_end();
}

static double rms(const int16_t *s, size_t f0, size_t f1, int ch)
{
    double a = 0;
    size_t i;
    for (i = f0; i < f1; i++) a += (double)s[i * 2 + ch] * s[i * 2 + ch];
    return f1 > f0 ? sqrt(a / (double)(f1 - f0)) : 0;
}

static double zc_hz(const int16_t *s, size_t f0, size_t f1)
{
    size_t i;
    int n = 0;
    for (i = f0 + 1; i < f1; i++) if (s[(i - 1) * 2] < 0 && s[i * 2] >= 0) n++;
    return n * (double)DCSND_RATE / (double)(f1 - f0);
}

static void test_pitch(void)
{
    const double c0 = 440.0 * pow(2.0, -57.0 / 12.0);
    int p, bad = 0;
    double worst = 0;
    for (p = 1; p <= 1727; p++) {
        uint32_t s = dcsnd_pitch_step(p);
        double hz = s * 44100.0 / 4294967296.0, want = c0 * pow(2.0, p / 192.0), e = fabs(hz / want - 1.0);
        if (s != ref_pitch_step(p)) bad++;
        if (e > worst) worst = e;
    }
    CHECK(bad == 0, "%d of 1727 pitch steps differ from the formula", bad);
    /* the low octaves lose the shifted-out bits: under a tenth of a cent */
    CHECK(worst < 5e-5, "the worst pitch is %.2e off", worst);
    CHECK(dcsnd_pitch_step(0) == dcsnd_pitch_step(1) && dcsnd_pitch_step(5000) == dcsnd_pitch_step(1727),
          "pitches outside 1..1727 are not clamped");
}

static void test_records(void)
{
    int pulse = inst_with(TIMBRE_PULSE), tri = inst_with(TIMBRE_TRI), saw = inst_with(TIMBRE_SAW);
    int k, a4 = 57 * 16;
    CHECK(pulse >= 0, "the bank has no pulse instrument");
    {   /* a full voice sounds like a full tone: A4, the same level, exact against the reference */
        Step st[1];
        double hz, lv, tone_lv;
        size_t i;
        st[0].slot = 0; st[0].mt = 0; st[0].voice = 0; st[0].r = rec(pulse, a4, 63, 63, 16, DVF_KEYON);
        play_script(st, 1, 40);
        hz = zc_hz(mix, 4410, 44100);
        lv = rms(mix, 22050, 44100, 0);
        CHECK(fabs(hz - 440.0) < 0.6, "A4 plays at %.2f Hz", hz);
        CHECK(ref_maxdiff() == 0, "one record: %ld off the reference", ref_maxdiff());
        run_begin(40);
        dcsnd_set_driver(NULL);
        dcsnd_tone(0, 440, 15, WAVE_PULSE50);
        for (i = 0; i < 40; i++) end_tick();
        run_end();
        tone_lv = rms(mix, 22050, 44100, 0);
        CHECK(fabs(lv / tone_lv - 1.0) < 0.01, "a voice at 63 is %.3f of a tone at 15", lv / tone_lv);
    }
    for (k = 0; k < DCSND_MT; k++) {   /* a record of mt k is heard from frame k * 1470 / 8 of its slot */
        Step st[1];
        size_t i, first = 0;
        st[0].slot = 3; st[0].mt = k; st[0].voice = 2; st[0].r = rec(pulse, a4, 63, 63, 16, DVF_KEYON);
        play_script(st, 1, 6);
        for (i = 0; i < mix_n; i++) if (mix[i * 2]) { first = i; break; }
        CHECK(first == (size_t)(3 * DCSND_PER_TICK + k * DCSND_PER_TICK / DCSND_MT),
              "mt %d: the first sample is frame %u", k, (unsigned)first);
        CHECK(tap_frame == (int)first, "mt %d: the tap saw frame %d", k, tap_frame);
    }
    {   /* stereo: one side only */
        Step st[2];
        st[0].slot = 0; st[0].mt = 0; st[0].voice = 0; st[0].r = rec(pulse, a4, 63, 0, 16, DVF_KEYON);
        st[1].slot = 10; st[1].mt = 0; st[1].voice = 0; st[1].r = rec(pulse, a4, 0, 63, 16, 0);
        play_script(st, 2, 20);
        CHECK(rms(mix, 1470, 14700, 0) > 3000 && rms(mix, 1470, 14700, 1) == 0, "vol_r 0 is not silent on the right");
        CHECK(rms(mix, 22050, 29400, 1) > 3000 && rms(mix, 22050, 29400, 0) < 40, "vol_l 0 is not silent on the left");
        CHECK(ref_maxdiff() == 0, "stereo: %ld off the reference", ref_maxdiff());
    }
    {   /* duty: tparam / 32; the mean of a 12.5 % pulse sits 3/4 of its swing below zero before the DC block
         * takes it out, so compare the share of high samples instead */
        static const int duty[3] = { 4, 8, 16 };
        for (k = 0; k < 3; k++) {
            Step st[1];
            size_t i, hi = 0;
            st[0].slot = 0; st[0].mt = 0; st[0].voice = 0; st[0].r = rec(pulse, a4, 63, 63, duty[k], DVF_KEYON);
            play_script(st, 1, 30);
            for (i = 22050; i < 44100; i++) {
                double mid = 0;
                size_t j;
                for (j = i - 50; j < i + 50; j++) mid += mix[j * 2];
                if (mix[i * 2] > mid / 100.0) hi++;
            }
            CHECK(fabs(hi / 22050.0 - duty[k] / 32.0) < 0.03, "duty %d/32: high %.3f of the time", duty[k], hi / 22050.0);
        }
    }
    if (tri >= 0 && saw >= 0) {   /* the other waves: the right pitch, exact */
        Step st[2];
        st[0].slot = 0; st[0].mt = 0; st[0].voice = 0; st[0].r = rec(tri, a4 - 192, 63, 63, 0, DVF_KEYON);
        st[1].slot = 20; st[1].mt = 0; st[1].voice = 0; st[1].r = rec(saw, a4 + 192, 63, 63, 0, 0);
        play_script(st, 2, 40);
        CHECK(fabs(zc_hz(mix, 4410, 26460) - 220.0) < 0.6, "the triangle plays at %.2f Hz", zc_hz(mix, 4410, 26460));
        CHECK(fabs(zc_hz(mix, 30870, 57330) - 880.0) < 1.0, "the saw plays at %.2f Hz", zc_hz(mix, 30870, 57330));
        CHECK(ref_maxdiff() == 0, "triangle and saw: %ld off the reference", ref_maxdiff());
    }
    {   /* a kit hit is noise, the same samples every time; DVF_CUT is silence */
        Step st[8];
        size_t i;
        long d = 0;
        int n = 0;
        for (k = 0; k < 3; k++) {
            st[n].slot = k; st[n].mt = 0; st[n].voice = 3;
            st[n++].r = rec(MUS_INST_KIT0, KH_SNARE, 50, 50, k, k ? 0 : DVF_KEYON);
        }
        st[n].slot = 3; st[n].mt = 0; st[n].voice = 3; st[n++].r = rec(MUS_INST_KIT0, KH_SNARE, 50, 50, 3, DVF_CUT);
        for (k = 0; k < 3; k++) {
            st[n].slot = 60 + k; st[n].mt = 0; st[n].voice = 3;
            st[n++].r = rec(MUS_INST_KIT0, KH_SNARE, 50, 50, k, k ? 0 : DVF_KEYON);
        }
        st[n].slot = 63; st[n].mt = 0; st[n].voice = 3; st[n++].r = rec(MUS_INST_KIT0, 0, 0, 0, 0, 0);
        play_script(st, n, 70);
        for (i = 0; i < 3 * DCSND_PER_TICK; i++) {
            long e = labs((long)mix[i * 2] - (long)mix[(i + 60 * DCSND_PER_TICK) * 2]);
            if (e > d) d = e;
        }
        CHECK(rms(mix, 0, 4410, 0) > 1000, "a snare hit is silent");
        CHECK(d <= 1, "the second snare differs from the first by %ld", d);
        CHECK(rms(mix, 8 * 1470, 50 * 1470, 0) < 2, "DVF_CUT leaves sound behind");
        CHECK(ref_maxdiff() == 0, "kit: %ld off the reference", ref_maxdiff());
    }
}

/* random records of every kind, bus and tape changes: the mixer against the reference */
static uint32_t rng = 20261112u;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static int fz_inst[8], fz_n;
static void fuzz_tick(void)
{
    if (rnd() % 3 == 0) {
        int v = (int)(rnd() % (DCSND_MVOICES + 1));     /* one past the end: ignored */
        DoomoVoice r;
        if (rnd() % 4 == 0)
            r = rec(MUS_INST_KIT0 + (int)(rnd() % 6), (int)(rnd() % 8), (int)(rnd() % 64), (int)(rnd() % 64),
                    (int)(rnd() % 12), (rnd() & 1 ? DVF_KEYON : 0) | (rnd() % 5 == 0 ? DVF_NOISE7 : 0));
        else
            r = rec(fz_inst[rnd() % (unsigned)fz_n], (int)(rnd() % 1800), (int)(rnd() % 70), (int)(rnd() % 64),
                    (int)(rnd() % 40), (rnd() & 1 ? DVF_KEYON : 0) | (rnd() % 9 == 0 ? DVF_CUT : 0) |
                    (rnd() % 7 == 0 ? DVF_NOISE7 | DVF_ECHO | DVF_LEGATO : 0));
        if (v < DCSND_MVOICES || rnd() % 2) plat_voice(v, &r);
    }
    if (rnd() % 40 == 0) plat_audio_bus(0, (uint8_t)(rnd() % 3 ? rnd() % 256 : 255));
    if (rnd() % 60 == 0) dc_music_tape((int)(rnd() % 3 ? rnd() % 300 : 0));
}

static void test_fuzz(void)
{
    int i, t;
    fz_n = 0;
    for (i = 0; i < TIMBRE_COUNT && fz_n < 8; i++) {
        int in = inst_with(i);
        if (in >= 0) fz_inst[fz_n++] = in;
    }
    CHECK(fz_n >= 3, "only %d timbres in the bank", fz_n);
    run_begin(900);
    inner_tick = fuzz_tick;
    for (t = 0; t < 900; t++) end_tick();
    run_end();
    CHECK(rms(mix, 0, mix_n, 0) > 500 && rms(mix, 0, mix_n, 1) > 500, "the fuzz run is silent");
    CHECK(ref_maxdiff() == 0, "900 ticks of random records: %ld off the reference", ref_maxdiff());
}

/* the game's audio for n ticks (the song keeps playing) */
static void game_ticks(int n)
{
    int t;
    for (t = 0; t < n; t++) { audio_tick(); end_tick(); }
}

static void test_s1(void)
{
    double before, during, after;
    size_t f;
    int t, grew = 1, prev = 0;
    /* mute: silent at once, back afterwards, the song still on the grid */
    game_reset();
    run_begin(400);
    audio_music(SONG_TITLE);
    game_ticks(90);
    f = mix_n;
    before = rms(mix, f - 30 * 1470, f, 0);
    audio_music_mute(40);
    game_ticks(40);
    during = rms(mix, f + 8 * 1470, f + 38 * 1470, 0);
    game_ticks(60);
    after = rms(mix, f + 60 * 1470, f + 98 * 1470, 0);
    CHECK(before > 1000, "the title cue is silent on the IRQ tier (rms %.0f)", before);
    CHECK(during < 5, "a mute leaves rms %.1f", during);
    CHECK(after > before * 0.5, "the music does not come back after a mute (%.0f of %.0f)", after, before);
    CHECK(last_bus == 255, "the bus is %d after the mute", last_bus);
    /* duck to 128 / 256: about half */
    f = mix_n;
    audio_music_duck(128, 60);
    game_ticks(60);
    during = rms(mix, f + 4 * 1470, f + 58 * 1470, 0);
    game_ticks(10);
    CHECK(last_bus == 255, "the bus is %d after the duck", last_bus);
    run_end();
    CHECK(ref_maxdiff() <= 1, "mute and duck: %ld off the reference", ref_maxdiff());
    {   /* the same 60 ticks without the duck */
        double plain;
        game_reset();
        run_begin(400);
        audio_music(SONG_TITLE);
        game_ticks(90); game_ticks(40); game_ticks(60);
        f = mix_n;
        game_ticks(70);
        run_end();
        plain = rms(mix, f + 4 * 1470, f + 58 * 1470, 0);
        CHECK(during > plain * 0.42 && during < plain * 0.58, "a duck to 128 plays at %.2f", during / plain);
    }
    /* tape stop: the pitch falls 15 semitones over the frames, then the music stays muted */
    game_reset();
    run_begin(300);
    audio_music(SONG_TITLE);
    game_ticks(60);
    audio_tapestop(30);
    for (t = 0; t < 30; t++) {
        game_ticks(1);
        if (last_bus && last_tape + 8 < prev) grew = 0;     /* (the core caps the table at 14 semitones: 232 -> 224 at the end) */
        prev = last_tape;
    }
    CHECK(grew && max_tape >= 14 * 16 && max_tape <= 15 * 16, "the tape stop reached %d sixteenths (224..240)", max_tape);
    f = mix_n;
    game_ticks(40);
    CHECK(last_bus == 0, "the music is not held muted after a tape stop (bus %d)", last_bus);
    CHECK(rms(mix, f + 8 * 1470, f + 36 * 1470, 0) < 5, "sound after the tape stop");
    audio_music(SONG_SHOP);                 /* a new song ends the hold */
    game_ticks(60);
    f = mix_n;
    run_end();
    CHECK(last_bus == 255 && last_tape == 0, "a new song starts with bus %d tape %d", last_bus, last_tape);
    CHECK(rms(mix, f - 30 * 1470, f, 0) > 500, "the new song is silent");
    CHECK(ref_maxdiff() <= 1, "tape stop: %ld off the reference", ref_maxdiff());
    {   /* the tape's pitch on a plain voice: an octave down halves it */
        Step st[1];
        st[0].slot = 0; st[0].mt = 0; st[0].voice = 0; st[0].r = rec(inst_with(TIMBRE_PULSE), 57 * 16, 63, 63, 16, DVF_KEYON);
        run_begin(60);
        inner_tick = script_tick;
        sc_steps = st; sc_n = 1; sc_slot = sc_mt = 0;
        for (t = 0; t < 30; t++) end_tick();
        dc_music_tape(192);
        for (t = 0; t < 30; t++) end_tick();
        run_end();
        CHECK(fabs(zc_hz(mix, 46305, 86730) - 220.0) < 0.6, "an octave of tape plays A4 at %.2f Hz", zc_hz(mix, 46305, 86730));
        CHECK(ref_maxdiff() == 0, "tape on a voice: %ld off the reference", ref_maxdiff());
    }
}

static void test_babble_and_state(void)
{
    DoomoToneState st;
    int t, harm = 0, sang = 0, seen = 0, c;
    game_reset();
    run_begin(300);
    audio_music(SONG_TITLE);
    game_ticks(60);
    CHECK(tone_calls >= 0 && last_tone[0].vol == 0 && last_tone[2].vol == 0 && last_tone[3].vol == 0,
          "the music reached plat_tone on the IRQ tier");
    /* the visualisers' view (the jukebox bars) still shows the music */
    for (t = 0; t < 60; t++) {
        game_ticks(1);
        for (c = 0; c < 4; c++) { audio_channel_state(c, &st); if (st.vol && st.freq) seen |= 1 << c; }
    }
    CHECK(seen == 15, "audio_channel_state shows channels %x of the title cue", seen);
    /* the babble sings on tone channel 1 while the harmony voice keeps playing */
    for (t = 0; t < 40; t++) {
        if (t % 4 == 0) audio_voice_char(NULL, "aeiou"[(t / 4) % 5]);
        game_ticks(1);
        if (audio_voice_busy() && last_tone[1].vol) sang++;
        if (audio_voice_busy() && last_rec[1].vol_l) harm++;
    }
    CHECK(sang >= 8, "the babble sounded on %d ticks", sang);
    CHECK(harm >= 4, "the harmony voice sounded on %d babble ticks", harm);
    game_ticks(10);
    CHECK(!audio_voice_busy() && last_tone[1].vol == 0, "tone channel 1 is not silent after the babble (vol %d)",
          last_tone[1].vol);
    /* a comedy one-shot borrows tone channels 2 and 3 and gives them back silent */
    audio_sfx(SFX_BOING);
    game_ticks(3);
    CHECK(last_tone[2].vol > 0, "the boing is silent");
    game_ticks(20);
    CHECK(last_tone[2].vol == 0 && last_tone[3].vol == 0, "the one-shot's channels are not silent afterwards");
    /* no music: every voice silent, exact zeros in the end */
    audio_music(SONG_NONE);
    game_ticks(60);
    run_end();
    CHECK(rms(mix, mix_n - 20 * 1470, mix_n, 0) == 0 && rms(mix, mix_n - 20 * 1470, mix_n, 1) == 0,
          "SONG_NONE leaves sound behind");
}

static void test_queue(void)
{
    uint32_t k0, t;
    game_reset();
    run_begin(10);
    audio_music(SONG_BOSS);
    /* nothing takes slots: the ring fills, then the driver still runs 8 mt a push */
    for (t = 0; t < DCSND_QUEUE; t++) { audio_tick(); dcsnd_push(); }
    k0 = mus_drv_keyons();
    CHECK(dcsnd_stats.drv_ticks == DCSND_QUEUE * DCSND_MT && dcsnd_stats.overruns == 0, "16 pushes: %u driver ticks",
          (unsigned)dcsnd_stats.drv_ticks);
    for (t = 0; t < 60; t++) { audio_tick(); dcsnd_push(); }
    CHECK(dcsnd_stats.overruns == 60 && dcsnd_stats.pushed == DCSND_QUEUE, "60 pushes into a full ring: %u overruns",
          (unsigned)dcsnd_stats.overruns);
    CHECK(dcsnd_stats.drv_ticks == (DCSND_QUEUE + 60) * DCSND_MT, "the driver stopped with the ring full (%u ticks)",
          (unsigned)dcsnd_stats.drv_ticks);
    CHECK(mus_drv_keyons() > k0 + 20, "the music stood still with the ring full");
    CHECK(mus_drv_underruns() == 0, "%u driver under-runs", (unsigned)mus_drv_underruns());
    /* without a driver the music engine is not ticked at all (the bridge build) */
    dcsnd_reset();
    dcsnd_set_driver(NULL);
    dcsnd_push();
    CHECK(dcsnd_stats.drv_ticks == 0, "a driver ran without one set");
}

static void test_caps(void)
{
    DoomoAudioCaps c;
    plat_audio_caps(&c);
    CHECK(c.tier != AT_BRIDGE && (c.flags & CAP_DRV_IRQ) && (c.flags & CAP_STEREO), "caps: tier %d flags 0x%x",
          c.tier, c.flags);
    CHECK(c.soft_voices == DCSND_MVOICES && c.mix_rate == DCSND_RATE && c.latency_mt == 0, "caps: voices %d rate %d",
          c.soft_voices, c.mix_rate);
    game_reset();
    CHECK(!mus_core_bridge(), "the core still runs the bridge");
    /* the classic law needs 30 Hz control frames: 8 mt (the core keeps them for an IRQ port) */
    {
        uint32_t r0;
        run_begin(40);
        audio_music(SONG_TITLE);
        music_profile(AP_OVERWORLD);
        game_ticks(10);
        r0 = n_rec;
        game_ticks(20);
        run_end();
        /* the core sets 4 mt per control frame under an IRQ driver: 2 frames a tick, 4 voices */
        CHECK(n_rec - r0 <= 20 * 2 * 4, "%u records in 20 ticks: more than two control frames a tick", (unsigned)(n_rec - r0));
    }
}

static int cmd_unit(void)
{
    game_reset();
    dc_sfx_init();
    test_caps();
    test_pitch();
    test_records();
    test_fuzz();
    test_s1();
    test_babble_and_state();
    test_queue();
    printf("test_tier: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
#endif

int main(int argc, char **argv)
{
    (void)ref_tone;
    if (argc == 2 && !strcmp(argv[1], "list")) return cmd_list();
    if (argc == 6 && !strcmp(argv[1], "song")) return cmd_song(atoi(argv[2]), atoi(argv[3]), argv[4], argv[5]);
#ifdef TIER_IRQ
    if (argc == 2 && !strcmp(argv[1], "unit")) return cmd_unit();
#endif
    fprintf(stderr, "usage: %s list | song N TICKS OUT.raw KEYS.txt%s\n", argv[0],
#ifdef TIER_IRQ
            " | unit"
#else
            ""
#endif
            );
    return 2;
}
