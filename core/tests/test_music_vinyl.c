/* test_music_vinyl.c - T9: the record-scratch DSP (owner: MU6;
 * docs/design/music/SPEC.md sec.7, 19 T9, CF-19, CF-20, CF-40).
 *
 *   - every pattern of the bank is well formed and its displacement is
 *     the integral of its speeds; the patterns do what sec.7.2 says
 *     (BABY returns to where it started, REWIND runs backwards, STOP and
 *     NEEDLE stop, TAPESTOP ends 15 semitones down, WOBBLE stays within 6 %);
 *   - the driver bends with exactly the speed the arranger computes, every
 *     control frame (bit for bit);
 *   - BABY + RESUME comes back to the ghost: after the pattern deck A plays
 *     what it would have played without it (D4: ring A is never rewritten);
 *   - seek equivalence (CF-20): the scrub decoder's note at a random
 *     playhead P equals the note deck A dispatched there - 1,000 random P
 *     per cue, over every classic cue and the test hub with the intensity
 *     changed at random bar lines;
 *   - house rules: a second pattern inside 8 bars is refused; a looping
 *     pattern only bends (no mute window).
 * TAPE (dry re-render) is M7's and is not tested here. */
#include <stdlib.h>
#include "test.h"
#include "../src/audio_int.h"
#include "../src/mus_int.h"
#include "data/mus_testbank.h"

/* ---- the dispatched-event trace ---------------------------------------------------------- */
typedef struct TrEv { uint32_t pos, arg, mt; uint8_t kind, ring, pad[2]; } TrEv;
static TrEv *tr;
static int tr_n, tr_cap;

static void tr_hook(const DoomoMusEv *e, int ring)
{
    if (tr_n == tr_cap) {
        int cap = tr_cap ? tr_cap * 2 : 8192;
        TrEv *n = (TrEv *)realloc(tr, sizeof(TrEv) * (size_t)cap);
        if (!n) return;
        tr = n;
        tr_cap = cap;
    }
    tr[tr_n].pos = MEV_POS(e);
    tr[tr_n].arg = e->arg;
    tr[tr_n].kind = (uint8_t)MEV_KIND(e);
    tr[tr_n].ring = (uint8_t)ring;
    tr[tr_n].mt = mus_drv_mt();
    tr_n++;
}

static void tr_start(void) { tr_n = 0; mus_drv_trace = tr_hook; }
static void tr_stop(void) { mus_drv_trace = NULL; }

static void on_bank(const uint8_t *bank)
{
    test_reset();
    mus_bank_use(bank);
    audio_init();
    mus_core_init();
}

static void off_bank(void)
{
    tr_stop();
    mus_bank_use(NULL);
    mus_core_init();
}

static void run(int n) { while (n-- > 0) music_update(); }

/* ---- patterns ------------------------------------------------------------------------------ */
TEST(music_t9_patterns_integrate_to_their_displacement)
{
    int id, t;
    for (id = 1; id <= VINYL_TAPESTOP; id++) {
        MusVinylView v;
        int32_t sum = 0;
        if (id == VINYL_CRACKLE) { CHECK(!mus_vinyl_get(id, &v)); continue; }   /* a toggle */
        CHECK_MSG(mus_vinyl_get(id, &v), "pattern %d", id);
        CHECK(v.len >= 6 && v.n_seg >= 1);
        for (t = 0; t < v.len; t++) sum += mus_vinyl_speed(&v, t, 1234u);
        CHECK_EQ(mus_vinyl_disp(&v, v.len, 1234u), sum);
        /* pure: the same unit gives the same speed */
        for (t = 0; t < v.len; t += 7) CHECK_EQ(mus_vinyl_speed(&v, t, 99u), mus_vinyl_speed(&v, t, 99u));
    }
    {
        MusVinylView v;
        CHECK(mus_vinyl_get(VINYL_BABY, &v));                       /* forward and back: home */
        CHECK(abs(mus_vinyl_disp(&v, v.len, 7u)) <= 3 * 256);
        CHECK(mus_vinyl_speed(&v, v.len / 4, 7u) > 256);            /* a fast forward stroke */
        CHECK(mus_vinyl_speed(&v, 3 * v.len / 4, 7u) < -256);       /* then back */
        CHECK(mus_vinyl_get(VINYL_REWIND, &v));
        CHECK(mus_vinyl_disp(&v, v.len, 7u) < -16 * 256);           /* backwards */
        CHECK(v.flags & MVF_SNAP);
        CHECK_EQ(v.after, AFTER_CONTINUE);
        CHECK(mus_vinyl_get(VINYL_STOP, &v));
        CHECK(mus_vinyl_speed(&v, 0, 7u) >= 240);
        CHECK(abs(mus_vinyl_speed(&v, v.len, 7u) - 64) <= 6);       /* 1 -> 0.25 (+- the 6 % hand) */
        CHECK(abs(mus_vinyl_speed(&v, v.len / 2, 7u) - 128) <= 10);  /* linear in pitch: an octave down */
        CHECK_EQ(v.after, AFTER_STOP);
        CHECK(mus_vinyl_get(VINYL_NEEDLE, &v));
        CHECK(mus_vinyl_speed(&v, v.len, 7u) == 0);                /* to a standstill */
        CHECK(mus_vinyl_get(VINYL_TAPESTOP, &v));
        CHECK(v.flags & MVF_FRAMES);
        CHECK(abs(mus_vinyl_speed(&v, v.len, 7u) - 108) <= 2);      /* 2^(-15/12) */
        CHECK(mus_vinyl_get(VINYL_WOBBLE, &v));
        CHECK(v.flags & MVF_LOOP);
        for (t = 0; t < 3 * v.len; t++) {
            int sp = mus_vinyl_speed(&v, t, 7u);
            CHECK_MSG(sp >= 238 && sp <= 274, "wobble at %d: %d", t, sp);
        }
        CHECK(mus_vinyl_pitch(256) == 0 && mus_vinyl_pitch(512) == 192 && mus_vinyl_pitch(128) == -192);
        CHECK(mus_vinyl_gain(256) == 256 && mus_vinyl_gain(64) == 128 && mus_vinyl_gain(8) == 0);
    }
}

/* the driver's platter speed equals the arranger's, every control frame */
TEST(music_t9_driver_and_arranger_speeds_agree)
{
    static const int pats[] = { VINYL_BABY, VINYL_SCRIBBLE, VINYL_SPINBACK, VINYL_SLOWMO, VINYL_TAPESTOP };
    int k;
    for (k = 0; k < (int)(sizeof(pats) / sizeof(pats[0])); k++) {
        int frames = 0, i;
        uint32_t S;
        on_bank(NULL);
        audio_music(SONG_HAHALAND_DAY);
        run(40);
        CHECK_MSG(music_vinyl_at(pats[k], music_now() + 3, VSRC_DECK, AFTER_RESUME) >= 0, "pattern %d refused", pats[k]);
        S = mus_vy.start;
        for (i = 0; i < 400; i++) {
            music_update();
            if (mus_drv_vinyl_on()) {
                int t = (int)mus_pdiff(mus_drv_clock(), S);
                CHECK_EQ(mus_drv_vinyl_speed(), mus_vinyl_speed(&mus_drv_vy, t < 0 ? 0 : t, mus_drv_vy_seed));
                frames++;
            }
        }
        CHECK_MSG(frames >= 2, "pattern %d bent %d frames", pats[k], frames);
        off_bank();
    }
}

/* BABY + RESUME: deck A's generation is untouched, and after the pattern
 * the voices play exactly what a run without the scratch plays */
TEST(music_t9_baby_resumes_at_the_ghost)
{
    static DrvVoice ref[200][MUS_DRV_VOICES];
    int i, v, start = 61, diff = 0, compared = 0;
    on_bank(NULL);
    audio_music(SONG_HAHALAND_DAY);
    for (i = 0; i < 200; i++) {
        music_update();
        for (v = 0; v < MUS_DRV_VOICES; v++) ref[i][v] = *mus_drv_voice(v);
    }
    off_bank();
    on_bank(NULL);
    audio_music(SONG_HAHALAND_DAY);
    for (i = 0; i < 200; i++) {
        if (i == start) CHECK(music_vinyl_at(VINYL_BABY, music_now() + 2, VSRC_DECK, AFTER_RESUME) >= 0);
        music_update();
        if (i >= start + 60) {                      /* a bar after the scratch */
            for (v = 0; v < MUS_DRV_VOICES; v++) {
                const DrvVoice *a = mus_drv_voice(v), *b = &ref[i][v];
                compared++;
                if (a->pitch16 != b->pitch16 || a->stage != b->stage || a->env != b->env) diff++;
            }
        }
    }
    CHECK(compared >= 300);
    CHECK_MSG(diff == 0, "%d of %d voice frames differ from the unscratched run", diff, compared);
    off_bank();
}

/* ---- seek equivalence (CF-20) ------------------------------------------------------------------ */
/* the note deck A dispatched on voice v that sounds at P (ring A NOTE
 * events; a note sounds from its position for its gate's frames) */
static int ref_note(int v, uint32_t P, int32_t rate256, uint32_t *start)
{
    int i;
    for (i = tr_n - 1; i >= 0; i--) {
        const TrEv *e = &tr[i];
        uint32_t end;
        if (e->ring != 0 || MEV_VOICE(e->arg) != v) continue;
        if (mus_pdiff(e->pos, P) > 0) continue;
        if (e->kind == MEV_OFF || e->kind == MEV_DRUM) return 0;
        if (e->kind != MEV_NOTE) continue;
        end = e->pos + (uint32_t)MEV_NOTE_DUR(e->arg) * (uint32_t)rate256 / 256u;
        if (mus_pdiff(end, P) <= 0) return 0;
        *start = e->pos;
        return MEV_NOTE_PITCH(e->arg);
    }
    return 0;
}

static int seek_run(int cue, int bankless, int params, uint32_t seed, int *checked)
{
    uint32_t x = seed | 1u;
    int i, bad = 0, n = 0;
    tr_start();
    if (bankless) audio_music(cue);
    else music_set_cue(cue, MQ_NOW);
    for (i = 0; i < 2400 && n < 1000; i++) {
        MusScrubNote nt[MUS_A_VOICES];
        uint32_t P, back;
        uint8_t got;
        int v;
        if (params && i % 37 == 0) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            music_set_param_now(MP_INTENSITY, (int)(x % 256));
        }
        music_update();
        if (i < 30 || i % 2) continue;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        back = x % (2u * 4u * MUS_UPQ);                    /* up to 2 bars back */
        P = (mus_drv_clock() - back) & MUS_POS_MASK;
        if (!mus_core_log_at(P)) continue;
        memset(nt, 0, sizeof(nt));
        got = mus_core_scrub(P, nt);
        for (v = 0; v < MUS_A_VOICES; v++) {
            uint32_t st = 0;
            int want = ref_note(v, P, mus_core_deck(0)->rate256, &st), have = (got >> v) & 1 ? nt[v].pitch : 0;
            if (want && st + 2 > P) continue;               /* a note starting right at P */
            if (have != want) {
                /* a boundary of a gate: within 2 units of the end is a tie */
                if (have && want == 0 && mus_pdiff(nt[v].end, P) <= 2) continue;
                if (bad < 3)
                    fprintf(stderr, "    cue %d tick %d voice %d P %u: scrub %d, deck A %d\n", cue, i, v,
                            (unsigned)P, have, want);
                bad++;
            }
            n++;
        }
    }
    tr_stop();
    *checked = n;
    return bad;
}

TEST(music_t9_seek_equivalence)
{
    int c, total = 0, bad = 0;
    for (c = 1; c < SONG_COUNT; c++) {
        int n = 0;
        on_bank(NULL);
        bad += seek_run(c, 1, 0, 0x1234u + (uint32_t)c, &n);
        total += n;
        off_bank();
    }
    {
        int n = 0;
        on_bank(mus_testbank);
        bad += seek_run(MCT_HUB, 0, 1, 0xBEEFu, &n);
        total += n;
        CHECK_MSG(n >= 900, "hub: %d samples", n);
        off_bank();
    }
    fprintf(stderr, "    T9: %d scrub samples, %d mismatches\n", total, bad);
    CHECK(total >= 20000);
    CHECK_EQ(bad, 0);
}

/* ---- house rules ------------------------------------------------------------------------------- */
TEST(music_t9_house_rules)
{
    on_bank(NULL);
    audio_music(SONG_HAHALAND_DAY);
    run(40);
    CHECK(music_vinyl(VINYL_BABY, MQ_BEAT, VSRC_DECK, AFTER_DEFAULT) >= 0);
    run(4);
    CHECK_EQ(music_vinyl(VINYL_CHIRP, MQ_BEAT, VSRC_DECK, AFTER_DEFAULT), -1);   /* <= 1 per 8 bars */
    run(30 * 8 * 2);
    CHECK(music_vinyl(VINYL_CHIRP, MQ_BEAT, VSRC_DECK, AFTER_DEFAULT) >= 0);
    off_bank();
    /* WOBBLE bends and loops until replaced: no mute window, no stop */
    on_bank(NULL);
    audio_music(SONG_HAHALAND_DAY);
    run(40);
    CHECK(music_vinyl(VINYL_WOBBLE, MQ_BEAT, VSRC_DECK, AFTER_DEFAULT) >= 0);
    run(60);
    CHECK(mus_drv_vinyl_on());
    CHECK(!mus_drv_muted(0, mus_drv_clock()));
    CHECK(!music_gated());
    music_vinyl(VINYL_NONE, MQ_NOW, VSRC_DECK, AFTER_DEFAULT);
    run(4);
    CHECK(!mus_drv_vinyl_on());
    off_bank();
}
