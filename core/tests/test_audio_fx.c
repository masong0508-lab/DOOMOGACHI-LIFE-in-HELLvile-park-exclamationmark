/* test_audio_fx.c - the comic-timing audio hooks and the comedy kit
 * (audio_fx.c; owner: S1, SPEC sec.7 S1 M2, C11; CT sec.3.8, 4, 7.5). */
#include "test.h"
#include "../src/audio_int.h"

static void ticks(int n) { while (n-- > 0) audio_tick(); }

static int loud(void)
{
    int c, v = 0;
    for (c = 0; c < AUDIO_CH; c++) {
        DoomoToneState st;
        audio_channel_state(c, &st);
        v += st.vol;
    }
    return v;
}

TEST(audio_fx_mute_keeps_the_sequencer_running)
{
    uint32_t n0, t0;
    int i, quiet = 1;
    test_reset();
    audio_init();
    audio_music(SONG_HAHALAND_DAY);
    ticks(40);
    CHECK(loud() > 0);
    n0 = audio_note_count();
    t0 = audio_music_ticks();
    audio_music_mute(30);
    for (i = 0; i < 30; i++) { audio_tick(); if (loud()) quiet = 0; }
    CHECK(quiet);
    CHECK(audio_note_count() > n0);               /* notes kept starting underneath */
    CHECK_EQ(audio_music_ticks(), t0 + 30);       /* on the grid: no pause */
    ticks(20);
    CHECK(loud() > 0);                            /* re-entered by itself */
    CHECK(!audio_fx_music_gated());
}

TEST(audio_fx_duck_scales_the_volume)
{
    int a, b, i, sum_a = 0, sum_b = 0;
    test_reset();
    audio_init();
    audio_music(SONG_HAHALAND_DAY);
    ticks(10);
    for (i = 0; i < 24; i++) { audio_tick(); sum_a += loud(); }
    audio_init();
    audio_music(SONG_HAHALAND_DAY);
    ticks(10);
    audio_music_duck(128, 24);
    for (i = 0; i < 24; i++) { audio_tick(); sum_b += loud(); }
    a = sum_a; b = sum_b;
    CHECK(a > 0);
    CHECK_MSG(b * 10 >= a * 3 && b * 10 <= a * 7, "duck 0.5: %d vs %d", b, a);
}

TEST(audio_fx_tapestop_slides_down_then_holds_until_a_new_song)
{
    DoomoToneState st;
    int i, prev = 0, falls = 0;
    test_reset();
    audio_init();
    audio_music(SONG_BOSS);
    ticks(6);
    audio_tapestop(5);
    for (i = 0; i < 5; i++) {
        audio_tick();
        audio_channel_state(2, &st);
        if (prev && st.freq && st.freq < prev) falls++;
        if (st.freq) prev = st.freq;
    }
    CHECK(falls >= 1);
    ticks(2);
    CHECK(audio_fx_music_gated());
    ticks(60);
    CHECK_EQ(loud(), 0);                          /* the scratch holds the silence */
    audio_music(SONG_HELLVILE);                   /* the next theme comes in */
    ticks(10);
    CHECK(loud() > 0);
}

TEST(audio_fx_resume_on_beat_lands_on_a_quarter)
{
    int i, at = -1;
    test_reset();
    audio_init();
    audio_music(SONG_HAHALAND_DAY);
    ticks(7);
    audio_tapestop(3);
    ticks(10);
    CHECK(audio_fx_music_gated());
    audio_music_resume_on_beat();
    for (i = 0; i < 40 && at < 0; i++) { audio_tick(); if (!audio_fx_music_gated()) at = i; }
    CHECK(at >= 0);
    CHECK(at <= 30);                              /* within one quarter at any tempo >= 60 */
}

TEST(audio_fx_comedy_kit_is_short_synth_and_borrows_channels)
{
    int s;
    test_reset();
    audio_init();
    CHECK_EQ(SFX_SYNTH_COUNT, 15);
    for (s = SFX_SYNTH_FIRST; s < SFX_SYNTH_END; s++) {
        int f = audio_sfx_synth_frames(s), i;
        uint32_t samples = nat_sample_calls, tones = nat_tone_calls;
        CHECK_MSG(f >= 1 && f <= 12, "%s lasts %d frames", audio_sfx_synth_name(s), f);
        CHECK(audio_sfx_synth_name(s)[0]);
        audio_sfx(s);                             /* routes to the synth */
        CHECK(audio_sfx_synth_busy());
        CHECK(audio_fx_owns(2) || audio_fx_owns(3));
        CHECK(!audio_fx_owns(0) && !audio_fx_owns(1));
        for (i = 0; i < 13; i++) audio_tick();
        CHECK(!audio_sfx_synth_busy());
        CHECK(!audio_fx_owns(2) && !audio_fx_owns(3));
        CHECK_EQ(nat_sample_calls, samples);      /* never PCM (C11) */
        CHECK(nat_tone_calls > tones);
    }
    opt_set(OPT_SFX_VOL, 0);
    audio_sfx_synth(SFX_BOING);
    CHECK(!audio_sfx_synth_busy());               /* muted SFX stay silent */
    opt_set(OPT_SFX_VOL, 8);
    audio_sfx_pitch(SFX_KAZOO, -5);               /* the kazoo steps down */
    CHECK(audio_sfx_synth_busy());
}

TEST(audio_fx_music_comes_back_after_a_one_shot)
{
    DoomoToneState st;
    test_reset();
    audio_init();
    audio_music(SONG_BOSS);
    ticks(8);
    audio_sfx_synth(SFX_THUD);
    ticks(14);
    audio_channel_state(2, &st);
    CHECK(!audio_fx_owns(2));
    CHECK(audio_music_current() == SONG_BOSS);
}

TEST(audio_fx_dual_music_and_the_gurgle_voice)
{
    const uint8_t *g = audio_voice_preset(VOICE_GURGLE);
    DoomoToneState st;
    int i, seen1 = 0;
    test_reset();
    audio_init();
    audio_music_dual(SONG_HAHALAND_DAY, SONG_HELLVILE);
    CHECK_EQ(audio_music_current(), SONG_HAHALAND_DAY);
    for (i = 0; i < 90; i++) {
        audio_tick();
        audio_channel_state(1, &st);
        if (st.vol) seen1 = 1;
    }
    CHECK(seen1);
    CHECK(g != NULL);
    CHECK(g[VOICE_PITCH] <= 2);                   /* the belly is the deepest voice */
    CHECK(audio_voice_freq(g, 'o') < 140);
    CHECK(audio_voice_preset(VOICE_NONE) == NULL);
}
