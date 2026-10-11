/* test_audio.c - the chiptune sequencer, the song book, SFX PCM and voice
 * babble (owner: story). */
#include "test.h"
#include "../src/audio_int.h"

static void ticks(int n) { while (n-- > 0) audio_tick(); }

TEST(audio_every_song_is_valid)
{
    int s;
    CHECK(SONG_COUNT - 1 >= 15);
    for (s = SONG_NONE + 1; s < SONG_COUNT; s++) {
        const char *err = NULL;
        CHECK_MSG(audio_song_check(s, &err), "song %d (%s): %s", s, audio_song_name(s), err ? err : "?");
        CHECK_MSG(audio_song_length(s) > 30, "song %s too short", audio_song_name(s));
        CHECK(audio_song_name(s)[0] != 0);
    }
    CHECK(!audio_song_check(SONG_NONE, NULL));
    CHECK(!audio_song_check(SONG_COUNT, NULL));
    CHECK(audio_inst_count >= 16);
}

TEST(audio_cute_and_metal_songs_sound_different)
{
    /* the cute family is led by pulse waves, the doom family by saws */
    static const uint8_t cute[] = { SONG_TITLE, SONG_HAHALAND_DAY, SONG_CREATOR, SONG_PET };
    static const uint8_t metal[] = { SONG_INVASION, SONG_HELL_BREACH, SONG_BOSS };
    int i, k;
    for (i = 0; i < DOOMO_COUNTOF(cute); i++) {
        int saw = 0;
        audio_init();
        opt_set(OPT_MUSIC_VOL, 8);
        audio_music(cute[i]);
        for (k = 0; k < 200; k++) {
            DoomoToneState st;
            audio_tick();
            audio_channel_state(0, &st);
            if (st.vol && st.wave == WAVE_SAW) saw++;
        }
        CHECK_MSG(saw == 0, "cute song %s uses saw on lead", audio_song_name(cute[i]));
    }
    for (i = 0; i < DOOMO_COUNTOF(metal); i++) {
        int saw = 0;
        audio_init();
        audio_music(metal[i]);
        for (k = 0; k < 200; k++) {
            DoomoToneState st;
            audio_tick();
            audio_channel_state(0, &st);
            if (st.vol && st.wave == WAVE_SAW) saw++;
        }
        CHECK_MSG(saw > 20, "metal song %s has no saw riff", audio_song_name(metal[i]));
    }
}

TEST(audio_sequencer_timing_loops_on_the_beat)
{
    uint32_t len, n1, n2;
    audio_init();
    opt_set(OPT_MUSIC_VOL, 8);
    audio_music(SONG_TITLE);
    CHECK_EQ(audio_music_current(), SONG_TITLE);
    len = audio_song_length(SONG_TITLE);
    /* 8 bars of 4/4 at 140 bpm = 32 beats = 13.7 s = ~411 ticks */
    CHECK(len >= 405 && len <= 417);
    ticks(10);
    CHECK(audio_note_count() > 0);
    CHECK_EQ(audio_music_ticks(), 10);
    ticks((int)len - 10);
    n1 = audio_note_count();
    ticks((int)len);
    n2 = audio_note_count();
    /* the second pass plays the same number of notes as the first */
    CHECK_MSG(n2 - n1 + 6 >= n1 && n2 - n1 <= n1 + 6, "pass1 %u pass2 %u", n1, n2 - n1);
    /* restarting the same song is a no-op */
    audio_music(SONG_TITLE);
    CHECK_EQ(audio_music_ticks(), 2 * len);
}

TEST(audio_all_channels_play)
{
    int k, seen[4] = { 0, 0, 0, 0 }, c;
    audio_init();
    opt_set(OPT_MUSIC_VOL, 10);
    audio_music(SONG_HAHALAND_DAY);
    for (k = 0; k < 300; k++) {
        audio_tick();
        for (c = 0; c < 4; c++) {
            DoomoToneState st;
            audio_channel_state(c, &st);
            if (st.vol && st.freq) seen[c]++;
        }
    }
    for (c = 0; c < 4; c++) CHECK_MSG(seen[c] > 10, "channel %d silent", c);
    {
        DoomoToneState st;
        audio_channel_state(2, &st);
        CHECK(st.wave == WAVE_TRIANGLE || st.vol == 0);
        audio_channel_state(9, &st);
        CHECK_EQ(st.freq, 0);
    }
}

TEST(audio_volume_option_mutes_music)
{
    int k, loud = 0;
    audio_init();
    opt_set(OPT_MUSIC_VOL, 0);
    audio_music(SONG_INVASION);
    for (k = 0; k < 120; k++) {
        int c;
        audio_tick();
        for (c = 0; c < 4; c++) { DoomoToneState st; audio_channel_state(c, &st); loud += st.vol; }
    }
    CHECK_EQ(loud, 0);
    opt_set(OPT_MUSIC_VOL, 10);
    for (k = 0; k < 60; k++) {
        int c;
        audio_tick();
        for (c = 0; c < 4; c++) { DoomoToneState st; audio_channel_state(c, &st); loud += st.vol; }
    }
    CHECK(loud > 0);
}

TEST(audio_stinger_returns_to_the_song)
{
    int k;
    audio_init();
    audio_music(SONG_HAHALAND_DAY);
    ticks(20);
    audio_music_once(SONG_VICTORY);
    CHECK_EQ(audio_music_current(), SONG_HAHALAND_DAY);
    for (k = 0; k < (int)audio_song_length(SONG_VICTORY) + 60; k++) audio_tick();
    CHECK_EQ(audio_music_current(), SONG_HAHALAND_DAY);
    CHECK(audio_music_ticks() < 200);              /* restarted after the stinger */
    audio_music(SONG_NONE);
    CHECK_EQ(audio_music_current(), SONG_NONE);
    ticks(5);
    {
        DoomoToneState st;
        audio_channel_state(0, &st);
        CHECK_EQ(st.vol, 0);
    }
}

TEST(audio_sfx_pcm_for_every_effect_within_budget)
{
    int s;
    uint32_t total = 0;
    for (s = SFX_NONE + 1; s < SFX_COUNT; s++) {
        uint32_t len = 0, rate = 0;
        const int8_t *pcm = audio_sfx_pcm(s, &len, &rate);
        int i, peak = 0;
        CHECK_MSG(pcm != NULL && len >= 100, "sfx %s has no pcm", audio_sfx_name(s));
        CHECK(rate >= 4000 && rate <= 22050);
        for (i = 0; i < (int)len; i++) { int a = pcm[i] < 0 ? -pcm[i] : pcm[i]; if (a > peak) peak = a; }
        CHECK_MSG(peak > 20, "sfx %s is silent", audio_sfx_name(s));
        total += len;
        CHECK(audio_sfx_name(s)[0] != 0);
    }
    CHECK(total <= AUDIO_SFX_BUDGET);
    CHECK_EQ(total, audio_sfx_total_bytes());
    CHECK(audio_sfx_pcm(SFX_NONE, NULL, NULL) == NULL);
    /* the volume option */
    opt_set(OPT_SFX_VOL, 0);
    nat_sample_calls = 0;
    audio_sfx(SFX_SQUEAK);
    CHECK_EQ(nat_sample_calls, 0);
    opt_set(OPT_SFX_VOL, 8);
    audio_sfx(SFX_SQUEAK);
    CHECK_EQ(nat_sample_calls, 1);
    CHECK_EQ(nat_last_sample, SFX_SQUEAK);
}

TEST(audio_voice_babble_follows_the_sliders)
{
    uint8_t lo[VOICE_COUNT] = { 2, 16, 16, 4, 8 }, hi[VOICE_COUNT] = { 29, 16, 16, 4, 8 };
    int k, blips = 0;
    CHECK(audio_voice_freq(hi, 'a') > audio_voice_freq(lo, 'a') * 3);
    CHECK(audio_voice_freq(lo, 'a') >= 90 && audio_voice_freq(hi, 'a') <= 1200);
    audio_init();
    opt_set(OPT_VOICE_BABBLE, 1);
    opt_set(OPT_VOICE_VOL, 8);
    for (k = 0; k < 40; k++) {
        audio_voice_char(hi, "hello there citizen"[k % 19]);
        if (audio_voice_busy()) blips++;
        audio_tick();
    }
    CHECK(blips > 5);
    ticks(10);
    CHECK(!audio_voice_busy());
    /* off means off */
    opt_set(OPT_VOICE_BABBLE, 0);
    audio_voice_char(hi, 'a');
    CHECK(!audio_voice_busy());
    opt_set(OPT_VOICE_BABBLE, 1);
    opt_set(OPT_VOICE_VOL, 0);
    audio_voice_char(hi, 'a');
    CHECK(!audio_voice_busy());
}

TEST(audio_tick_is_cheap_and_bounded)
{
    int s, k;
    /* every song survives a long run without the sequencer wedging */
    for (s = SONG_NONE + 1; s < SONG_COUNT; s++) {
        uint32_t notes;
        audio_init();
        audio_music(s);
        for (k = 0; k < 1500; k++) audio_tick();
        notes = audio_note_count();
        CHECK_MSG(notes > 20, "song %s played %u notes", audio_song_name(s), notes);
    }
}
