/* test_music_comic.c - T8: the comic-timing bridge (owner: MU6;
 * docs/design/music/SPEC.md sec.9.2, 19 T8, CF-18, CF-25).
 *
 *   - pre-rolled stingers and vinyl land on the frame: music_sting_at() /
 *     music_vinyl_at() for tick T dispatch in the update that brings
 *     music_now() to T, i.e. at driver mt (8T - 8, 8T] (native, both on the
 *     bridge and with an IRQ-style driver);
 *   - music_land() bends the tempo (<= 12 %) so the grid point is audible
 *     exactly `frames` ticks later: residual 0 inside the range, honest
 *     outside it;
 *   - `{r}` (NEEDLE + a resume 14 frames later): music_gated() for exactly
 *     14 frames, then deck A comes back on a quarter downbeat;
 *   - a mute window keeps the grid: beats keep coming, the walk does not
 *     move, the music after it is the music of a run without it;
 *   - the second stinger in a bar is refused; a lower priority never cuts
 *     a higher one. */
#include <stdlib.h>
#include "test.h"
#include "../src/audio_int.h"
#include "../src/mus_int.h"
#include "data/mus_testbank.h"

typedef struct TrEv { uint32_t pos, arg, mt; uint8_t kind, ring, pad[2]; } TrEv;
static TrEv tr[4096];
static int tr_n;

static void tr_hook(const DoomoMusEv *e, int ring)
{
    if (tr_n >= (int)(sizeof(tr) / sizeof(tr[0]))) return;
    tr[tr_n].pos = MEV_POS(e);
    tr[tr_n].arg = e->arg;
    tr[tr_n].kind = (uint8_t)MEV_KIND(e);
    tr[tr_n].ring = (uint8_t)ring;
    tr[tr_n].mt = mus_drv_mt();
    tr_n++;
}

static void on_bank(const uint8_t *bank)
{
    test_reset();
    mus_bank_use(bank);
    audio_init();
    mus_core_init();
    tr_n = 0;
    mus_drv_trace = tr_hook;
}

static void off_bank(void)
{
    mus_drv_trace = NULL;
    mus_bank_use(NULL);
    mus_core_init();
}

static void run(int n) { while (n-- > 0) music_update(); }

/* one 30 Hz tick: the update, then (IRQ style) its 8 driver ticks */
static void tick(int irq)
{
    music_update();
    if (irq) { int m; for (m = 0; m < 8; m++) mus_drv_tick(); }
}

/* the mt at which the first event of `kind` (with arg bits `bits`) dispatched */
static int first_mt(int kind, uint32_t bits, uint32_t mask)
{
    int i;
    for (i = 0; i < tr_n; i++)
        if (tr[i].kind == kind && (tr[i].arg & mask) == bits) return (int)tr[i].mt;
    return -1;
}

TEST(music_t8_stingers_land_on_the_frame)
{
    static const int stings[2] = { STGT_HIT, STGT_TADA };
    int irq, s, lead;
    for (irq = 0; irq < 2; irq++)
        for (s = 0; s < 2; s++)
            for (lead = 1; lead <= 24; lead += 3) {
                uint32_t T;
                int got, mt;
                on_bank(mus_testbank);
                if (irq) mus_core_test_irq(1);
                music_set_cue(MCT_HUB, MQ_NOW);
                { int i; for (i = 0; i < 45 + lead; i++) tick(irq); }
                T = music_now() + (uint32_t)lead;
                tr_n = 0;
                got = music_sting_at(stings[s], T, STF_REPLACE);
                CHECK_MSG(got >= 0, "sting %d lead %d refused", stings[s], lead);
                CHECK_MSG(got == lead, "sting %d: %d ticks to the hit, want %d", stings[s], got, lead);
                while (music_now() < T + 2) tick(irq);
                mt = first_mt(MEV_MARK, MMK_STING_HIT, MMK_STING_HIT);
                CHECK_MSG(mt > 8 * (int)T - 8 && mt <= 8 * (int)T, "%s sting %d lead %d: hit at mt %d, tick %u = mt %u",
                          irq ? "irq" : "bridge", stings[s], lead, mt, (unsigned)T, (unsigned)(8 * T));
                if (irq) mus_core_test_irq(0);
                off_bank();
            }
}

TEST(music_t8_vinyl_lands_on_the_frame)
{
    static const int pats[3] = { VINYL_NEEDLE, VINYL_BABY, VINYL_REWIND };
    int irq, p, lead;
    for (irq = 0; irq < 2; irq++)
        for (p = 0; p < 3; p++)
            for (lead = 1; lead <= 20; lead += 4) {
                uint32_t T;
                int mt;
                on_bank(NULL);
                if (irq) mus_core_test_irq(1);
                audio_music(SONG_HAHALAND_DAY);
                { int i; for (i = 0; i < 40 + lead; i++) tick(irq); }
                T = music_now() + (uint32_t)lead;
                tr_n = 0;
                CHECK_EQ(music_vinyl_at(pats[p], T, VSRC_DECK, AFTER_DEFAULT), lead);
                while (music_now() < T + 2) tick(irq);
                mt = first_mt(MEV_FX, MFX_VINYL_START, 15u);
                CHECK_MSG(mt > 8 * (int)T - 8 && mt <= 8 * (int)T, "%s vinyl %d lead %d: start at mt %d, want tick %u",
                          irq ? "irq" : "bridge", pats[p], lead, mt, (unsigned)T);
                if (irq) mus_core_test_irq(0);
                off_bank();
            }
}

/* the tick at which the next grid event (MB_BEAT / MB_BAR) arrives */
static int grid_tick(int kind, int max)
{
    int i;
    for (i = 0; i < max; i++) {
        MusBeatEv ev[8];
        int n, k;
        music_update();
        n = music_beat_events(ev, 8);
        for (k = 0; k < n; k++) if (ev[k].kind & kind) return (int)music_now();
    }
    return -1;
}

static void drain(void) { MusBeatEv ev[8]; while (music_beat_events(ev, 8)) {} }

TEST(music_t8_land_bends_onto_the_frame)
{
    static const int cases[][2] = { { MQ_BEAT, -1 }, { MQ_BEAT, 1 }, { MQ_BAR, -4 }, { MQ_BAR, 4 }, { MQ_BAR, 2 },
                                    { MQ_BAR, -1 } };
    int c;
    for (c = 0; c < (int)(sizeof(cases) / sizeof(cases[0])); c++) {
        int grid = cases[c][0], kind = grid == MQ_BAR ? MB_BAR : MB_BEAT, natural, frames, res, got;
        uint32_t asked;
        on_bank(mus_testbank);
        music_set_cue(MCT_HUB, MQ_NOW);
        run(50);
        drain();
        grid_tick(MB_BEAT, 40);                         /* just after a beat */
        drain();
        natural = music_ticks_to(grid);
        frames = natural + cases[c][1];
        asked = music_now() + (uint32_t)frames;
        res = music_land(grid, frames, 31);             /* <= 12 % */
        CHECK_MSG(res == 0, "land %d frames on grid %d (natural %d): residual %d", frames, grid, natural, res);
        got = grid_tick(kind, 120);
        CHECK_MSG(got == (int)asked, "grid %d heard at tick %d, asked %u (natural %d)", grid, got, (unsigned)asked,
                  natural);
        off_bank();
    }
    /* out of range: the residual says by how much it misses */
    on_bank(mus_testbank);
    music_set_cue(MCT_HUB, MQ_NOW);
    run(50);
    grid_tick(MB_BEAT, 40);
    {
        int natural = music_ticks_to(MQ_BEAT), res = music_land(MQ_BEAT, natural / 2, 31);
        CHECK_MSG(res > 0, "residual %d", res);
    }
    off_bank();
}

/* {r}: NEEDLE now, music_resume_on_beat() 14 frames later */
TEST(music_t8_scratch_gates_exactly_14_frames)
{
    int i, gated = 0, first = -1, last = -1;
    uint32_t start, resume_tick, keyon_tick = 0;
    on_bank(mus_testbank);
    music_set_cue(MCT_HUB, MQ_NOW);
    run(60);
    start = music_now();
    CHECK(music_vinyl_at(VINYL_NEEDLE, start + 1, VSRC_DECK, AFTER_STOP) >= 0);
    resume_tick = start + 14;                         /* 14 frames after the {r} */
    tr_n = 0;
    for (i = 0; i < 60; i++) {
        if (music_now() == resume_tick) music_resume_on_beat();
        music_update();
        if (music_gated()) { gated++; if (first < 0) first = (int)music_now(); last = (int)music_now(); }
    }
    CHECK_MSG(gated == 14, "gated %d frames (%d..%d)", gated, first, last);
    CHECK_EQ(first, (int)start + 1);
    /* deck A comes back on a quarter downbeat after the resume: the first
     * deck-A note dispatched (ring B catch-up or ring A) after the resume
     * sits on a beat line */
    {
        int k;
        const MusGen *a = mus_core_deck(0);
        (void)a;
        for (k = 0; k < tr_n; k++) {
            if (tr[k].mt <= 8 * resume_tick || tr[k].kind != MEV_NOTE || MEV_VOICE(tr[k].arg) >= MUS_A_VOICES) continue;
            if (tr[k].ring == 0 && mus_drv_muted(0, tr[k].pos)) continue;
            keyon_tick = (tr[k].mt + 7) / 8;
            CHECK_MSG(tr[k].pos % MUS_UPQ == 0, "the music came back at %u, not on a beat", (unsigned)tr[k].pos);
            break;
        }
        CHECK(keyon_tick > resume_tick && keyon_tick <= resume_tick + 16);
    }
    off_bank();
}

/* a mute window keeps the grid and the walk */
TEST(music_t8_mute_keeps_the_grid)
{
    static uint32_t ref_pos[400];
    static uint8_t ref_node[400];
    static int16_t ref_pitch[400][MUS_A_VOICES];
    int i, v, beats = 0, diff = 0;
    on_bank(mus_testbank);
    music_set_cue(MCT_HUB, MQ_NOW);
    for (i = 0; i < 400; i++) {
        MusPos p;
        music_update();
        music_pos(&p);
        ref_pos[i] = (uint32_t)p.bar * 4u + p.beat;
        ref_node[i] = p.node;
        for (v = 0; v < MUS_A_VOICES; v++) ref_pitch[i][v] = mus_drv_voice(v)->pitch16;
    }
    off_bank();
    on_bank(mus_testbank);
    music_set_cue(MCT_HUB, MQ_NOW);
    for (i = 0; i < 400; i++) {
        MusPos p;
        MusBeatEv ev[8];
        int n, k;
        if (i == 100) music_mute_window(1, 61);
        music_update();
        n = music_beat_events(ev, 8);
        if (i > 101 && i < 160) for (k = 0; k < n; k++) beats += (ev[k].kind & MB_BEAT) != 0;
        if (i > 101 && i < 160) CHECK(music_gated());
        music_pos(&p);
        CHECK_MSG((uint32_t)p.bar * 4u + p.beat == ref_pos[i] && p.node == ref_node[i], "tick %d: the grid moved", i);
        if (i >= 200)
            for (v = 0; v < MUS_A_VOICES; v++) diff += mus_drv_voice(v)->pitch16 != ref_pitch[i][v];
    }
    CHECK_MSG(beats >= 3, "%d beats inside the window", beats);
    CHECK_MSG(diff == 0, "%d voice frames differ after the window", diff);
    CHECK(!music_gated());
    off_bank();
}

TEST(music_t8_one_sting_per_bar_and_priority)
{
    int a, b;
    on_bank(mus_testbank);
    music_set_cue(MCT_HUB, MQ_NOW);
    run(50);
    a = music_sting(STGT_HIT, MQ_BEAT, STF_REPLACE);
    CHECK(a >= 0);
    b = music_sting(STGT_HIT, MQ_BEAT, STF_REPLACE);
    CHECK_EQ(b, -1);                                    /* the same bar */
    run(130);                                          /* two bars on */
    CHECK(music_sting(STGT_TADA, MQ_BAR, 0) >= 0);      /* prio 2 */
    CHECK_EQ(music_sting(STGT_HIT, MQ_BAR, STF_REPLACE), -1);   /* prio 1 never cuts prio 2 */
    off_bank();
}
