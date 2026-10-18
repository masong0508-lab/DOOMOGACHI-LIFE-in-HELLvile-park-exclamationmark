/* test_contracts.c - the story / comic timing / dream house contracts that
 * package S0 writes (docs/design/story/SPEC.md sec.4, R7, R13, R14, R16, R21,
 * C9, C14, C19). Owner: S0 -> I9.
 *
 * Checks the frozen layouts (sizes and offsets), the id ranges, the
 * registries (read through volatile pointers, so strong definitions in
 * other files are seen), the joint save budget and the test registry cap.
 */
#include <stddef.h>
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/unlock_ids.h"
#include "doomo/tween.h"
#include "doomo/beat.h"
#include "doomo/camfx.h"
#include "doomo/story_vm.h"
#include "doomo/txt.h"
#include "doomo/home.h"
#include "doomo/house_api.h"
#include "doomo/save_budget.h"
#include "../src/audio_int.h"

int test_registry_cap(void);
int test_registry_count(void);

TEST(contracts_struct_sizes)
{
    CHECK_EQ(sizeof(DoomoStry), 96);
    CHECK_EQ(sizeof(DoomoStryLater), 4);
    CHECK_EQ(sizeof(BeatEv), 8);
    CHECK_EQ(sizeof(StoryOp), 4);
    CHECK_EQ(sizeof(StoryScript), 6);
    CHECK_EQ(sizeof(DlgLine), 6);
    CHECK_EQ(sizeof(DoomoRoomObj), 12);
    CHECK_EQ(sizeof(TwSpring), 16);
    /* the STRY layout is the save format: every u16 aligned, nothing moved */
    CHECK_EQ(offsetof(DoomoStry, side2), 8);
    CHECK_EQ(offsetof(DoomoStry, sayit_said), 44);
    CHECK_EQ(offsetof(DoomoStry, fountain_q8), 60);
    CHECK_EQ(offsetof(DoomoStry, grumble), 64);
    CHECK_EQ(offsetof(DoomoStry, gag_cool), 72);
    CHECK_EQ(offsetof(DoomoStry, later), 80);
    /* 38 episode/special bits fit episode_done[] */
    CHECK(BEAT_NEW_COUNT <= (int)sizeof(((DoomoStry *)0)->episode_done) * 8);
    /* 118 side rows x 2 bits fit side2[] */
    CHECK(118 * 2 <= (int)sizeof(((DoomoStry *)0)->side2) * 8);
    /* HOME v2 worst case (R9, R10, C9) and the per-chunk cap */
    CHECK_EQ(HOME_SZ_ROOM_MAX, 250);
    CHECK_EQ(HOME_SZ_OWNED, 192);
    CHECK_EQ(HOME_SZ_SEEN_NEW, 48);
    CHECK_EQ(HOME_SZ_CACHE, 16);
    CHECK_EQ(HOME_CHUNK_WORST, 3432);
    CHECK(HOME_CHUNK_WORST <= DOOMO_SAVE_EXT_MAX);
    CHECK(FURN_MAX <= 512);                       /* ids stay 9 bits */
}

TEST(contracts_ids_are_appended)
{
    int i, j;
    /* story.h: appended after the legacy ids, which keep their values */
    CHECK_EQ(BEAT_DONE, 27);
    CHECK_EQ(BEAT_COUNT, 28);
    CHECK_EQ(BEAT_P0, BEAT_DONE + 1);
    CHECK_EQ(BEAT_NEW_COUNT, 38);
    CHECK_EQ(CUT_FEAST_POP, 11);
    CHECK_EQ(CUT_COUNT, 12);
    CHECK_EQ(CUT_P0, CUT_FEAST_POP + 1);
    CHECK_EQ(CUT_ALL_COUNT - CUT_P0, 12);
    /* audio.h: 9 songs after SONG_OMINOUS; synth SFX after the PCM ones */
    CHECK_EQ(SONG_MINGLE, SONG_OMINOUS + 1);
    CHECK_EQ(SONG_COUNT - SONG_MINGLE, 9);
    CHECK_EQ(SONG_NUMBER_TEN, SONG_COUNT - 1);
    CHECK_EQ(SFX_SYNTH_FIRST, SFX_COUNT);
    CHECK_EQ(SFX_SYNTH_COUNT, 15);
    /* flags: the hot one-shots live in 228..255 */
    CHECK_EQ(SF_ASKED_OTHER_SIDE, 228);
    CHECK_EQ(SF_NGPLUS, 252);
    CHECK(SF_SPARE_253 < 256);
    /* knocks + wrongness use the spare met/nest/possessed bits, disjointly,
     * never the two used met bits (56 Tallyworn, 57 Mother Nox) */
    {
        uint8_t used[64];
        memset(used, 0, sizeof(used));
        for (i = 0; i < STORY_KNOCKS + STORY_WRONGS; i++) {
            int b = i < STORY_KNOCKS ? SF_KNOCK(i) : SF_WRONG(i - STORY_KNOCKS);
            CHECK_MSG(b >= 36 && b < 64 && b != 56 && b != 57 && !(b >= 40 && b <= 41),
                      "bit %d of entry %d", b, i);
            CHECK_MSG(!used[b], "bit %d twice", b);
            used[b] = 1;
        }
    }
    /* text ids: pack:7 | index:9 */
    CHECK_EQ(TXT_PACK(TXT_ID(127, 511)), 127);
    CHECK_EQ(TXT_INDEX(TXT_ID(127, 511)), 511);
    CHECK_EQ(TXT_PACK(TXT_ID(70, 3)), 70);
    CHECK_EQ(TXT_ID(127, 511), TXT_NONE);         /* reserved: never emitted */
    /* VM op / push / home sub-op enums stay inside a byte */
    CHECK(OP_COUNT <= 256 && BOP_COUNT <= 256 && HOME_SUBOP_COUNT <= 256);
    CHECK_EQ(PEND_OTHER, 9);                      /* the legacy values kept */
    CHECK(L_PUNCH < BEAT_LABEL_MAX && L_BUTTON < BEAT_LABEL_MAX && L_PUNCH != L_BUTTON);
    for (i = 0; i < TWT_COUNT; i++) for (j = 0; j < i; j++) CHECK(i != j);
    /* SCENE_HOME args (C14) */
    CHECK_EQ(HOME_ARG_OWN(HOME_COTTAGE), -1);
    CHECK(HOME_ARG_IS_OWN(HOME_ARG_OWN(HOME_PARK_PLOT)));
    CHECK(HOME_ARG_IS_VISIT(HOME_ARG_VISIT(0)) && HOME_ARG_IS_VISIT(HOME_ARG_VISIT(999)));
    CHECK(HOME_ARG_IS_STORY(HOME_ARG_STORY(HSM_DEFEND)));
    for (i = 0; i <= 1; i++)
        CHECK(!HOME_ARG_IS_OWN(i) && !HOME_ARG_IS_VISIT(i) && !HOME_ARG_IS_STORY(i));
}

TEST(contracts_unlock_ranges_disjoint)
{
    static const struct { const char *who; int first, last; } r[] = {
        { "foundation", UL_IDS_FOUNDATION_FIRST, UL_IDS_FOUNDATION_LAST },
        { "studio",     UL_IDS_STUDIO_FIRST,     UL_IDS_STUDIO_LAST },
        { "creator",    UL_IDS_CREATOR_FIRST,    UL_IDS_CREATOR_LAST },
        { "life",       UL_IDS_LIFE_FIRST,       UL_IDS_LIFE_LAST },
        { "library",    UL_IDS_LIBRARY_FIRST,    UL_IDS_LIBRARY_LAST },
        { "action",     UL_IDS_ACTION_FIRST,     UL_IDS_ACTION_LAST },
        { "home",       UL_IDS_HOME_FIRST,       UL_IDS_HOME_LAST },
        { "reserve",    UL_IDS_RESERVE_FIRST,    UL_IDS_RESERVE_LAST },
        { "story",      UL_IDS_STORY_FIRST,      UL_IDS_STORY_LAST },
        { "freeplay",   UL_IDS_FREEPLAY_FIRST,   UL_IDS_FREEPLAY_LAST },
        { "minigames",  UL_IDS_MINIGAMES_FIRST,  UL_IDS_MINIGAMES_LAST },
        { "levels",     UL_IDS_LEVELS_FIRST,     UL_IDS_LEVELS_LAST },
        { "concert",    UL_IDS_CONCERT_FIRST,    UL_IDS_CONCERT_LAST },
        { "fishcook",   UL_IDS_FISHCOOK_FIRST,   UL_IDS_FISHCOOK_LAST },
    };
    int i, n = DOOMO_COUNTOF(r), id;
    /* the team ranges tile 0..1023 exactly, in order, without overlap */
    CHECK_EQ(r[0].first, 0);
    for (i = 0; i < n; i++) {
        CHECK_MSG(r[i].first <= r[i].last, "%s empty", r[i].who);
        if (i) CHECK_MSG(r[i].first == r[i - 1].last + 1, "%s does not follow %s", r[i].who, r[i - 1].who);
    }
    CHECK_EQ(r[n - 1].last, DOOMO_UNLOCK_BITS - 1);
    CHECK_EQ(UL_IDS_HOME_LAST - UL_IDS_HOME_FIRST + 1, 32);
    /* the custom reserve sits inside foundation's tail */
    CHECK(UL_IDS_CUSTOM_FIRST >= UL_IDS_FOUNDATION_FIRST && UL_IDS_CUSTOM_LAST == UL_IDS_FOUNDATION_LAST);
    /* the story sub-blocks: inside story, in order, disjoint */
    CHECK_EQ(UL_STORY_CUT2_FIRST, 677);           /* after 660 + 17 rewards */
    CHECK_EQ(UL_STORY_CUT2(CUT_P0), UL_STORY_CUT2_FIRST);
    CHECK_EQ(UL_STORY_CUT2(CUT_ALL_COUNT - 1), UL_STORY_CUT2_LAST);
    CHECK_EQ(UL_STORY_SONG2(SONG_MINGLE), UL_STORY_SONG2_FIRST);
    CHECK_EQ(UL_STORY_SONG2(SONG_COUNT - 1), UL_STORY_SONG2_LAST);
    CHECK_EQ(UL_STORY_SONG2_FIRST, UL_STORY_CUT2_LAST + 1);
    CHECK_EQ(UL_STORY_MISC2_FIRST, UL_STORY_SONG2_LAST + 1);
    CHECK_EQ(UL_STORY_MISC2_LAST - UL_STORY_MISC2_FIRST + 1, 13);
    CHECK(UL_STORY_SPARE_FIRST == UL_STORY_MISC2_LAST + 1 && UL_STORY_SPARE_FIRST <= UL_IDS_STORY_LAST);
    /* nothing registered today sits in the reserve, the custom reserve or
     * the story's new sub-blocks (their owners fill them) */
    for (id = 0; id < DOOMO_UNLOCK_BITS; id++) {
        const DoomoUnlock *u = unlock_by_id(id);
        if (!u) continue;
        CHECK_MSG(!(id >= UL_IDS_RESERVE_FIRST && id <= UL_IDS_RESERVE_LAST), "unlock %d in the reserve", id);
        CHECK_MSG(!(id >= UL_IDS_CUSTOM_FIRST && id <= UL_IDS_CUSTOM_LAST), "unlock %d in the custom reserve", id);
    }
}

/* The registries' weak NULL defaults live in other files: reading the
 * pointers here (no folding: they are extern in this file) must agree
 * with what the registries' readers (volatile reads) report. */
TEST(contracts_registries_read_through_volatile)
{
    const DoomoSaveExt *const exts[SAVE_EXT_SLOTS] = {
        doomo_save_ext_studio, doomo_save_ext_extra, doomo_save_ext_test,
        doomo_save_ext_freeplay, doomo_save_ext_fishcook,
        doomo_save_ext_home, doomo_save_ext_story2 };
    const DoomoActivity *const acts[] = {
        doomo_activities_minigames, doomo_activities_concert, doomo_activities_collections,
        doomo_activities_combat, doomo_activities_extra, doomo_activities_studio,
        doomo_activities_home };
    const StoryMgEntry *const mgs[] = { doomo_story_mg_a, doomo_story_mg_b };
    int i, n = 0, k = 0;
    for (i = 0; i < SAVE_EXT_SLOTS; i++) if (exts[i]) {
        CHECK_MSG(save_ext_at(n) == exts[i], "save_ext_at(%d)", n);
        n++;
    }
    CHECK_EQ(save_ext_count(), n);
    CHECK(save_ext_at(n) == NULL);
    n = 0;
    for (i = 0; i < DOOMO_COUNTOF(acts); i++) {
        const DoomoActivity *a = acts[i];
        while (a && a->name) { CHECK(activity_at(n) == a); n++; a++; }
    }
    CHECK_EQ(activity_count(), n);
    for (i = 0; i < DOOMO_COUNTOF(mgs); i++) {
        const StoryMgEntry *e = mgs[i];
        while (e && e->name) { CHECK(story_mg_at(k) == e); CHECK(story_mg_find(e->id) != NULL); k++; e++; }
    }
    CHECK_EQ(story_mg_count(), k);
    CHECK(story_mg_at(k) == NULL);
    /* the home table is registered in its own slot and stays in its range */
    for (i = 0; doomo_unlocks_home[i].name; i++)
        CHECK(doomo_unlocks_home[i].id >= UL_IDS_HOME_FIRST && doomo_unlocks_home[i].id <= UL_IDS_HOME_LAST);
    /* the new scenes are in the scene table */
    CHECK(scene_by_name("HOME") == &SCENE_HOME);
    CHECK(scene_by_name("HOME_MART") == &SCENE_HOME_MART);
    CHECK(scene_by_name("STORY_MG") == &SCENE_STORY_MG);
    CHECK(scene_by_name("SCENE_STORY_TIMING") == &SCENE_STORY_TIMING);
    /* a declared max_size never exceeds the staging cap */
    for (i = 0; i < save_ext_count(); i++) CHECK(save_ext_at(i)->max_size <= DOOMO_SAVE_EXT_MAX);
}

TEST(contracts_save_budget_monotonic)
{
    uint32_t now[3] = { 100, 0, 0 }, at[3] = { 100, 0, 0 };
    int32_t prev, v, left0, left1;
    int k;
    /* growth only ever lowers the budget, and by at least the raw bytes */
    prev = save_budget_compute(SAVE_SLOT_FP_CAP, 60000, now, at, 3);
    CHECK_EQ(prev, (int32_t)(SAVE_SLOT_FP_CAP - 60000 - SAVE_BUDGET_MARGIN));
    for (k = 1; k <= 400; k++) {
        now[0] = 100 + (uint32_t)k * 7;
        now[1] = (uint32_t)k * 3;              /* a new chunk appears at k = 1 */
        v = save_budget_compute(SAVE_SLOT_FP_CAP, 60000, now, at, 3);
        CHECK_MSG(v < prev, "k=%d: %d !< %d", k, v, prev);
        CHECK(prev - v >= 10);
        prev = v;
    }
    /* shrinking never frees space (the LZ could get worse) */
    now[0] = 50; now[1] = 0;
    CHECK_EQ(save_budget_compute(SAVE_SLOT_FP_CAP, 60000, now, at, 3),
             (int32_t)(SAVE_SLOT_FP_CAP - 60000 - SAVE_BUDGET_MARGIN));
    /* the LZ worst case: 128 raw bytes cost 129 */
    now[0] = 100 + 128;
    CHECK_EQ(save_budget_compute(SAVE_SLOT_FP_CAP, 60000, now, at, 3),
             (int32_t)(SAVE_SLOT_FP_CAP - 60000 - SAVE_BUDGET_MARGIN - 129));
    /* the live budget: sane on both slots, meter in range, I/O snapshots */
    left0 = save_budget_left(0);
    left1 = save_budget_left(1);
    CHECK(left0 < (int32_t)SAVE_SLOT_STORY_CAP && left1 < (int32_t)SAVE_SLOT_FP_CAP);
    CHECK(save_budget_meter(0) >= 0 && save_budget_meter(0) <= 100);
    CHECK(save_budget_meter(1) >= 0 && save_budget_meter(1) <= 100);
    save_budget_note_io(1, 20000);
    CHECK_EQ(save_budget_left(1), (int32_t)(SAVE_SLOT_FP_CAP - 20000 - SAVE_BUDGET_MARGIN));
    CHECK(save_budget_fits(1, 1000));
    CHECK(!save_budget_fits(1, SAVE_SLOT_FP_CAP));
    save_budget_reset();
    CHECK_EQ(save_budget_left(1), left1);
}

TEST(contracts_max_tests)
{
    CHECK(test_registry_cap() >= 1024);
    CHECK(test_registry_count() < test_registry_cap());   /* nothing dropped */
}

/* C19: an old save (no STRY chunk) loads at the right beat, and the missing
 * chunks' reset() runs after the town body is decoded (save.c resets the
 * missing chunks last), so S2's STRY reset can derive the episode from
 * doomo_town.story.quest. */
static uint8_t mig_image[128 * 1024];
TEST(story_save_migrates_old_beats)
{
    int b;
    for (b = BEAT_C1_ARRIVE; b <= BEAT_DONE; b++) {
        uint32_t n;
        town_new(TOWN_STORY, 77, TOWN_POP_EMPTY);
        doomo_town.story.quest = (uint8_t)b;
        doomo_town.story.chapter = (uint8_t)story_beat_chapter(b);
        n = save_serialize(mig_image, sizeof(mig_image));
        CHECK_MSG(n > 0, "beat %d: %s", b, save_last_error());
        CHECK(n <= SAVE_SLOT_STORY_CAP);
        memset(&doomo_town.story, 0x5A, sizeof(doomo_town.story));
        CHECK_MSG(save_deserialize(mig_image, n), "beat %d: %s", b, save_last_error());
        CHECK_EQ(doomo_town.mode, TOWN_STORY);
        CHECK_EQ(doomo_town.story.quest, b);
        CHECK_EQ(story_beat(), b);
    }
}

/* The weak software post-FX defaults draw without touching out-of-range
 * pixels (ASan) at both screen sizes' worth of arguments. */
TEST(contracts_weak_fx_defaults)
{
    static const uint8_t px[4] = { 2, 2, 2, 2 };
    DoomoImage img;
    img.w = 2; img.h = 2; img.pix = px; img.uid = doomo_image_new_uid();
    plat_begin_frame();
    plat_image_xform(20, 20, &img, 256, 256, 0, 0);
    plat_image_xform(20, 20, &img, -300, 230, 0, 0);
    plat_image_xform(0, 0, &img, 1024, 1024, 0, 0);
    plat_post_offset(3, -2);
    plat_post_scale(300, 300, 10, 10);
    plat_post_blend(PAL_WHITE, 9);
    plat_post_blend(PAL_BLACK, 7);
    plat_post_blend(PAL_WHITE, 16);
    plat_post_iris(plat_screen_w() / 2, plat_screen_h() / 2, 40, 1, PAL_WHITE);
    plat_post_iris(-50, 400, 600, 0, 0);
    plat_post_iris(10, 10, 0, 0, PAL_WHITE);
    plat_pal_lerp(PAL_SEPIA, 8);
    plat_room_ui_opaque(0, 0, 10, 10);
    plat_end_frame();
    CHECK(1);
}
