/* test_music_drv.c - T13: the driver (owner: MU6; docs/design/music/SPEC.md
 * sec.6.1-6.2, 19 T13): ring merge, the classic envelope law, vibrato
 * delay, arpeggio steps, sweeps, mute windows, gain ramps, the fence,
 * under-runs and the volume arithmetic. */
#include "test.h"
#include "../src/mus_int.h"

/* records from the driver when the bridge's direct path is off */
static DoomoVoice t13_last[MUS_DRV_VOICES];
static int t13_calls;

void plat_voice(int voice, const DoomoVoice *v)   /* strong: the tests' probe */
{
    if (voice >= 0 && voice < MUS_DRV_VOICES) t13_last[voice] = *v;
    t13_calls++;
    if (voice >= 0 && voice < 4) mus_bridge_tone(v, &mus_bridge_tones[voice]);
}

static void t13_reset(int bridge)
{
    test_reset();
    mus_core_init();
    mus_drv_set_bridge(bridge);
    memset(t13_last, 0, sizeof(t13_last));
    t13_calls = 0;
}

static void frames(int n) { while (n-- > 0) mus_drv_run(MUS_MT_PER_TICK); }

static void note_at(int ring, uint32_t pos, int voice, int inst, int pitch, int dur)
{
    mus_drv_push(ring, MEV_MAKE_POS(MEV_INST, pos), MEV_INST_ARG(voice, inst, 0, 48));
    mus_drv_push(ring, MEV_MAKE_POS(MEV_NOTE, pos), MEV_NOTE_ARG(voice, pitch, 48, dur, 0));
}

TEST(music_t13_rings_merge_in_time_ring_a_first_on_ties)
{
    t13_reset(1);
    mus_drv_set_tempo(65536);                         /* one unit per mt */
    mus_drv_push(1, MEV_MAKE_POS(MEV_INST, 4), MEV_INST_ARG(0, 0, 0, 40));
    mus_drv_push(0, MEV_MAKE_POS(MEV_INST, 5), MEV_INST_ARG(0, 0, 0, 10));
    mus_drv_push(1, MEV_MAKE_POS(MEV_INST, 5), MEV_INST_ARG(0, 0, 0, 20));
    mus_drv_push(0, MEV_MAKE_POS(MEV_INST, 6), MEV_INST_ARG(0, 0, 0, 30));
    mus_drv_tick(); mus_drv_tick(); mus_drv_tick();
    CHECK_EQ(mus_drv_voice(0)->vel, 0);
    mus_drv_tick();                                   /* clock 4: B@4 */
    CHECK_EQ(mus_drv_voice(0)->vel, 40);
    mus_drv_tick();                                   /* clock 5: A@5, then B@5 */
    CHECK_EQ(mus_drv_voice(0)->vel, 20);
    mus_drv_tick();
    CHECK_EQ(mus_drv_voice(0)->vel, 30);
    CHECK_EQ(mus_drv_ring_used(0), 0);
    CHECK_EQ(mus_drv_ring_used(1), 0);
}

TEST(music_t13_the_batched_bridge_run_equals_single_ticks)
{
    /* mus_drv_run(8) (one clock move, then the due events) == 8 ticks */
    uint32_t c1;
    int k;
    t13_reset(1);
    for (k = 0; k < 30; k++) note_at(0, (uint32_t)(k * 7), k & 3, 0, 50 + k, 3);
    for (k = 0; k < 80; k++) mus_drv_run(8);
    c1 = mus_drv_clock();
    CHECK_EQ(mus_drv_keyons(), 30);
    t13_reset(1);
    for (k = 0; k < 30; k++) note_at(0, (uint32_t)(k * 7), k & 3, 0, 50 + k, 3);
    for (k = 0; k < 640; k++) mus_drv_tick();
    CHECK_EQ(mus_drv_clock(), c1);
    CHECK_EQ(mus_drv_keyons(), 30);
}

TEST(music_t13_envelope_follows_the_classic_law)
{
    /* @9 soft pad: attack 5 frames to 12*16, sustain 12, release 6 */
    static const uint8_t att[6] = { 38, 76, 114, 152, 190, 192 };
    const MusInst *in = mus_bank_inst(9);
    int f;
    t13_reset(1);
    CHECK_EQ(in->attack, 5);
    CHECK_EQ(in->level, 12);
    note_at(0, 0, 0, 9, 60, 20);                      /* gate: 20 frames */
    for (f = 0; f < 6; f++) { frames(1); CHECK_EQ(mus_drv_voice(0)->env, att[f]); }
    frames(10);
    CHECK_EQ(mus_drv_voice(0)->env, 192);             /* sustain */
    frames(5);                                        /* frame 20: the gate closes */
    CHECK_EQ(mus_drv_voice(0)->env, 192 - 6);
    frames(1);
    CHECK_EQ(mus_drv_voice(0)->env, 192 - 12);
    frames(40);
    CHECK_EQ(mus_drv_voice(0)->env, 0);
    CHECK_EQ(mus_drv_voice(0)->stage, 4);             /* off */
}

TEST(music_t13_a_rest_releases_and_a_cut_silences)
{
    t13_reset(1);
    mus_drv_set_tempo(65536);
    note_at(0, 1, 0, 9, 60, 200);
    mus_drv_push(0, MEV_MAKE_POS(MEV_OFF, 100), MEV_OFF_ARG(0, MOFF_RELEASE));
    mus_drv_push(0, MEV_MAKE_POS(MEV_OFF, 200), MEV_OFF_ARG(0, MOFF_CUT));
    while (mus_drv_clock() < 100) mus_drv_tick();
    CHECK_EQ(mus_drv_voice(0)->stage, 3);             /* releasing */
    while (mus_drv_clock() < 200) mus_drv_tick();
    CHECK_EQ(mus_drv_voice(0)->stage, 4);
}

TEST(music_t13_vibrato_waits_for_its_delay)
{
    /* @0 cute lead: vibrato depth 3, speed 20, delay 10 frames; phase 0 at
     * frame 10 (sin 0), so the first bend is frame 11 */
    int f, moved = -1;
    t13_reset(0);
    CHECK_EQ(mus_bank_inst(0)->vib_delay, 10);
    note_at(0, 0, 0, 0, 60, 200);
    for (f = 0; f < 30; f++) {
        frames(1);
        if (t13_last[0].pitch16 != 60 * 16 && moved < 0) moved = f;
    }
    CHECK_EQ(moved, 11);
}

TEST(music_t13_arpeggios_step_once_per_control_frame)
{
    /* @4 major chord: +0 +4 +7 +4 semitones, one step per frame (8 mt on the
     * bridge, 4 mt at 60 Hz) */
    static const int step[4] = { 0, 4, 7, 4 };
    int f, ctl;
    for (ctl = 8; ctl >= 4; ctl -= 4) {
        t13_reset(0);
        mus_drv_set_ctl(ctl);
        note_at(0, 0, 0, 4, 60, 200);
        for (f = 0; f < 12; f++) {
            int k;
            for (k = 0; k < ctl; k++) mus_drv_tick();
            CHECK_MSG(t13_last[0].pitch16 == (60 + step[f & 3]) * 16,
                      "ctl %d frame %d: pitch16 %d", ctl, f, t13_last[0].pitch16);
        }
    }
}

TEST(music_t13_sweeps_slide_every_frame)
{
    /* @13 zap: +24/16 semitone per frame */
    int f;
    t13_reset(0);
    CHECK_EQ(mus_bank_inst(13)->sweep, 24);
    note_at(0, 0, 0, 13, 48, 200);
    for (f = 0; f < 10; f++) {
        frames(1);
        CHECK_EQ(t13_last[0].pitch16, 48 * 16 + 24 * (f + 1));
    }
}

TEST(music_t13_mute_windows_drop_exactly_their_span)
{
    t13_reset(1);
    mus_drv_set_tempo(65536);
    mus_drv_push(1, MEV_MAKE_POS(MEV_MUTE, 20), MEV_MUTE_ARG(1, 0, 10));   /* [20, 30) */
    note_at(0, 19, 0, 0, 60, 50);
    note_at(0, 20, 0, 0, 61, 50);
    note_at(0, 25, 1, 0, 64, 50);                     /* another voice: not muted */
    note_at(0, 29, 0, 0, 62, 50);
    note_at(0, 30, 0, 0, 63, 50);
    while (mus_drv_clock() < 19) mus_drv_tick();
    CHECK_EQ(mus_drv_keyons(), 1);
    while (mus_drv_clock() < 20) mus_drv_tick();
    CHECK_EQ(mus_drv_voice(0)->stage, 4);             /* cut when the window opens */
    while (mus_drv_clock() < 29) mus_drv_tick();
    CHECK_EQ(mus_drv_keyons(), 2);                    /* + voice 1 */
    while (mus_drv_clock() < 30) mus_drv_tick();
    CHECK_EQ(mus_drv_keyons(), 3);                    /* 30 plays again */
    CHECK_EQ(mus_drv_voice(0)->pitch16, 63 * 16);
}

TEST(music_t13_gain_ramps_over_their_length)
{
    int f, prev = 255;
    t13_reset(1);
    note_at(0, 0, 0, 2, 40, 2000);
    frames(1);
    mus_drv_push(0, MEV_MAKE_POS(MEV_GAIN, mus_drv_clock()), MEV_GAIN_ARG(0, 0, 64));
    for (f = 0; f < 8; f++) {
        frames(1);
        CHECK(mus_drv_voice(0)->gain < prev || mus_drv_voice(0)->gain == 0);
        prev = mus_drv_voice(0)->gain;
    }
    CHECK_EQ(mus_drv_voice(0)->gain, 0);              /* 64 mt = 8 frames */
    CHECK_EQ(mus_bridge_tones[0].vol, 0);
}

TEST(music_t13_the_fence_is_never_passed)
{
    int k;
    t13_reset(1);
    mus_drv_set_tempo(65536 + 30000);
    mus_drv_fence(1, 10);
    note_at(0, 11, 0, 0, 60, 5);
    for (k = 0; k < 50; k++) mus_drv_tick();
    CHECK_EQ(mus_drv_clock(), 10);
    CHECK_EQ(mus_drv_keyons(), 0);
    mus_drv_fence(0, 0);
    mus_drv_tick();
    CHECK_EQ(mus_drv_keyons(), 1);
}

TEST(music_t13_under_runs_are_counted)
{
    int k;
    t13_reset(1);
    mus_drv_set_tempo(65536);
    mus_drv_written(0, 5);
    for (k = 0; k < 4; k++) mus_drv_tick();
    CHECK_EQ(mus_drv_underruns(), 0);
    for (k = 0; k < 4; k++) mus_drv_tick();
    CHECK(mus_drv_underruns() > 0);
}

TEST(music_t13_volume_multiply_shift_equals_the_division)
{
    int level, vel, master;
    for (level = 0; level <= 240; level++)
        for (vel = 0; vel <= 63; vel++)
            for (master = 0; master <= 10; master++) {
                int want = (level * vel * master + 4800) / 9600;
                if (want > 15) want = 15;
                if (mus_drv_vol15(level, vel, master) != want) {
                    CHECK_MSG(0, "level %d vel %d master %d: %d != %d", level, vel, master,
                              mus_drv_vol15(level, vel, master), want);
                }
            }
}
