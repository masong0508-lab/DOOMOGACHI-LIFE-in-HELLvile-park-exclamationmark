/* test_story_ch12.c - Chapters 1-2 of Story Mode (S2 content): the beat
 * stages play to their end by themselves, every Ch1-2 beat has its pack and
 * an objective, the wrongness rows and the cast's idle lines decode, and the
 * two S2 songs are real songs (core/src/story_ch12.c, cutscene_ch12.c,
 * core/data/story/ch1/, ch2/). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/txt.h"
#include "doomo/beat.h"
#include "doomo/story_vm.h"
#include "../src/story_int.h"
#include "../src/story_ch12.h"
#include "../src/overworld_int.h"
#include "../src/audio_int.h"

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

static void fresh_story(uint32_t seed, int beat)
{
    test_reset();
    story_debug_autowin(1);
    town_new(TOWN_STORY, seed, TOWN_POP_PREMADE);
    sim_init();
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
    story_debug_jump(story_beat_chapter(beat), beat);
    frame(0);
}

/* every stage runs on its own (each shot moves on after a while) and pops */
TEST(story_ch12_beat_stages_play_to_the_end)
{
    static const int cuts[6] = { CUT_P0, CUT_BOTTLE, CUT_F1, CUTV_FOG, CUTV_RITE_FAIL, CUTV_GATE_CRACKS };
    int i;
    for (i = 0; i < 6; i++) {
        int n;
        CHECK(cut12_owns(cuts[i]));
        fresh_story(4242, i < 1 ? BEAT_C1_MAYOR : BEAT_C2_EXORCISE);
        story_push_cutscene(cuts[i], CTX_FREE, 0);
        CHECK(scene_top() == &SCENE_CUTSCENE);
        for (n = 0; n < 3000 && scene_top() == &SCENE_CUTSCENE; n++) frame(0);
        CHECK_MSG(scene_top() != &SCENE_CUTSCENE, "cutscene %d never ended", cuts[i]);
        CHECK_MSG(n > 150, "cutscene %d ended after %d frames", cuts[i], n);
    }
    CHECK(!cut12_owns(CUT_ARRIVAL));
    CHECK(!cut12_owns(CUT_FEAST_POP));
    CHECK(!cut12_owns(CUTV_END));
    story_debug_autowin(0);
}

/* START skips a stage at once */
TEST(story_ch12_start_skips_a_stage)
{
    int n;
    fresh_story(77, BEAT_C2_CELLAR);
    story_push_cutscene(CUTV_GATE_CRACKS, CTX_FREE, 0);
    frame(0);
    frame(BTN_START);
    for (n = 0; n < 4 && scene_top() == &SCENE_CUTSCENE; n++) frame(0);
    CHECK(scene_top() != &SCENE_CUTSCENE);
    story_debug_autowin(0);
}

/* every Ch1-2 beat: an episode pack, an objective, a target */
TEST(story_ch12_every_beat_has_a_pack_and_an_objective)
{
    int b;
    for (b = BEAT_C1_ARRIVE; b <= BEAT_C2_CELLAR; b++) {
        int map, cx, cy;
        const char *o;
        fresh_story(900 + (uint32_t)b, b);
        CHECK(ch12_beat(b));
        CHECK_MSG(ch12_pack(b) >= 0 && txt_pack_kind(ch12_pack(b)) == TXT_KIND_EPISODE, "beat %d", b);
        o = ch12_objective();
        CHECK_MSG(o && o[0] && !strchr(o, '@'), "beat %s: objective \"%s\"", story_beat_name(b), o ? o : "");
        ch12_target(&map, &cx, &cy);
        CHECK_MSG(map == MAPID_HAHALAND && cx > 0 && cy > 0, "beat %s: no target", story_beat_name(b));
    }
    CHECK(!ch12_beat(BEAT_C3_ERUPTION));
    CHECK_EQ(ch12_pack(BEAT_C3_ERUPTION), -1);
    story_debug_autowin(0);
}

/* the wrongness rows (W1-W15; W16 is the graveyard door) and the idle lines */
TEST(story_ch12_wrong_rows_and_idle_lines_decode)
{
    int i, seen = 0;
    char buf[200];
    fresh_story(5, BEAT_C1_FRIENDS);
    CHECK(doomo_ow_wrong_rows != NULL);
    for (i = 0; i < 32 && doomo_ow_wrong_rows[i].map != 0xFF; i++) {
        const OwWrongRow *r = &doomo_ow_wrong_rows[i];
        CHECK(r->w < 16);
        CHECK_MSG(!((seen >> r->w) & 1), "row W%d twice", r->w + 1);
        seen |= 1 << r->w;
        CHECK(r->chapter_mask & 0x06);
        CHECK_MSG(txt_get(r->caption, buf, (int)sizeof(buf)) > 0, "W%d caption", r->w + 1);
        if (r->script) CHECK(txt_script(r->script, NULL) != NULL);
    }
    CHECK_EQ(i, 15);
    CHECK(ch12_idle(ROSTER_ID_MAYOR, 1, buf, (int)sizeof(buf)) && buf[0]);
    CHECK(!strchr(buf, '@'));
    CHECK(ch12_idle(ROSTER_ID_VEX, 2, buf, (int)sizeof(buf)) && buf[0]);
    CHECK(!ch12_idle(ROSTER_ID_VEX, 1, buf, (int)sizeof(buf)));
    CHECK(!ch12_idle(ROSTER_ID_MAYOR, 3, buf, (int)sizeof(buf)));
    story_debug_autowin(0);
}

/* the Mingle and "Hee. Hee." are S2's own songs, not the silent placeholders */
TEST(story_ch12_songs_are_real)
{
    static const int songs[2] = { SONG_MINGLE, SONG_HEEHEE_JINGLE };
    int i;
    for (i = 0; i < 2; i++) {
        const char *err = NULL;
        CHECK_MSG(audio_song_check(songs[i], &err), "%s: %s", audio_song_name(songs[i]), err ? err : "");
        CHECK(audio_song_length(songs[i]) > 100);
        CHECK(strncmp(audio_songs[songs[i]].ch[0], "t120 v0", 7) != 0);
    }
}
