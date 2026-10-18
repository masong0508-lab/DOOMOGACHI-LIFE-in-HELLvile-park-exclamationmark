/* test_music_classic.c - T1: the classic cues ARE the old songs (owner:
 * MU6; docs/design/music/SPEC.md sec.18, 19 T1, D15).
 *
 * Every SONG_* plays through the engine (mus_bind -> its MCF_EXACT classic
 * cue -> rings -> driver -> the legacy bridge -> plat_tone channel states)
 * and through the classic 30 Hz interpreter side by side, 1,500 ticks each.
 * The SPEC allows onset +-1 tick, Hz +-1 and the same wave class; the
 * engine is held to the stricter bar: every channel's (Hz, volume, wave)
 * equal on every tick, and the same key-on count. Frozen at M5. */
#include "test.h"
#include "../src/audio_int.h"
#include "../src/mus_int.h"

#define T1_TICKS 1500

static int t1_audible;   /* channel-ticks with sound in the last t1_diff run */

static int t1_diff(int song, int dual_b, int ticks, int vol, char *why, int cap)
{
    int t, c;
    t1_audible = 0;
    test_reset();
    opt_set(OPT_MUSIC_VOL, vol);
    audio_init();
    if (dual_b) { audio_music_dual(song, dual_b); audio_legacy_dual(song, dual_b); }
    else { audio_music(song); audio_legacy_start(song); }
    for (t = 0; t < ticks; t++) {
        audio_tick();
        audio_legacy_tick();
        for (c = 0; c < AUDIO_CH; c++) {
            DoomoToneState e, l;
            audio_channel_state(c, &e);
            audio_legacy_state(c, &l);
            if (e.vol) t1_audible++;
            if (e.freq != l.freq || e.vol != l.vol || (l.vol && e.wave != l.wave)) {
                snprintf(why, (size_t)cap, "%s tick %d ch%d: engine %uHz v%u w%u, classic %uHz v%u w%u",
                         audio_song_name(song), t, c, e.freq, e.vol, e.wave, l.freq, l.vol, l.wave);
                return 1;
            }
        }
    }
    if (!dual_b && audio_note_count() != audio_legacy_notes()) {
        snprintf(why, (size_t)cap, "%s: %u key-ons, the classic sequencer %u", audio_song_name(song),
                 (unsigned)audio_note_count(), (unsigned)audio_legacy_notes());
        return 1;
    }
    return 0;
}

TEST(music_t1_every_song_equals_the_interpreter)
{
    int s;
    char why[256];
    for (s = SONG_NONE + 1; s < SONG_COUNT; s++) {
        CHECK_MSG(!t1_diff(s, 0, T1_TICKS, 10, why, sizeof(why)), "%s", why);
        /* the 7 story placeholders are silent (CF-4); every real song sounds */
        CHECK_MSG(t1_audible > 1000 || (s > SONG_MINGLE && s != SONG_HEEHEE_JINGLE),
                  "%s is silent", audio_song_name(s));
    }
}

TEST(music_t1_quiet_volumes_round_the_same)
{
    static const uint8_t songs[] = { SONG_TITLE, SONG_BOSS, SONG_CHURCH, SONG_FESTIVAL };
    int i, vol;
    char why[256];
    for (i = 0; i < DOOMO_COUNTOF(songs); i++)
        for (vol = 1; vol <= 9; vol += 4)
            CHECK_MSG(!t1_diff(songs[i], 0, 400, vol, why, sizeof(why)), "vol %d: %s", vol, why);
}

TEST(music_t1_dual_equals_the_interpreter)
{
    char why[256];
    CHECK_MSG(!t1_diff(SONG_HAHALAND_DAY, SONG_HELLVILE, T1_TICKS, 10, why, sizeof(why)), "%s", why);
    CHECK_MSG(!t1_diff(SONG_INVASION, SONG_TITLE, 600, 10, why, sizeof(why)), "%s", why);
}

TEST(music_t1_lengths_and_names_come_from_the_bank)
{
    static const uint32_t pass1[] = { 0 };   /* lengths: the classic formula, from the bank */
    int s;
    (void)pass1;
    for (s = SONG_NONE + 1; s < SONG_COUNT; s++) {
        const char *err = NULL;
        CHECK(audio_song_check(s, &err));
        CHECK_MSG(music_cue_exists(mus_bind_cue(s, -1, -1, 0)), "song %d has no classic cue", s);
        CHECK(audio_song_name(s)[0] != 0);
    }
    CHECK_STR(audio_song_name(SONG_HAHALAND_DAY), "HaHaland Day");
    CHECK_STR(audio_song_name(SONG_NONE), "Silence");
    CHECK_EQ(audio_song_length(SONG_NONE), 0);
    /* TITLE: 8 bars of 4/4 at 140 bpm */
    CHECK_EQ(audio_song_length(SONG_TITLE), (8u * 192u * 256u + 955u) / 956u);
}

TEST(music_t1_once_resumes_like_the_interpreter)
{
    /* a stinger plays once, then the song before it restarts from its top
     * on the tick the stinger's last note ends (audio_music_once) */
    int t, end = -1;
    uint32_t len = audio_song_length(SONG_VICTORY);
    test_reset();
    audio_init();
    audio_music(SONG_HAHALAND_DAY);
    for (t = 0; t < 20; t++) audio_tick();
    audio_music_once(SONG_VICTORY);
    for (t = 0; t < (int)len + 30 && end < 0; t++) {
        audio_tick();
        if (audio_music_ticks() < (uint32_t)t) end = t;
    }
    CHECK_EQ(audio_music_current(), SONG_HAHALAND_DAY);
    CHECK_MSG(end == (int)len - 1, "resumed at tick %d, the stinger lasts %u", end, (unsigned)len);
}
