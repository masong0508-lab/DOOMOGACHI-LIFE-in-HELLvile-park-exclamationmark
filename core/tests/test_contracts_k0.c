/* test_contracts_k0.c - the cross-area registry changes K0 lands once
 * (LIFESIM_PLAN R3, R5, R6 + section 6): the reserved story callback
 * blocks, the weak plat_ext.h hooks, the six placeholder scenes and the
 * shared life / play-as / profile numbers (life_contract.h). Owner: K0
 * contract.
 * (The save registry and the joint worst case: test_save_budget.c; the
 * endings' unlock ids: test_contracts.c.)
 *
 * Every check holds both before and after the owners land: a block's ids
 * stay where K0 put them, and a scene or hook that is no longer the weak
 * default is left to its owner's tests.
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/plat_ext.h"
#include "doomo/life_contract.h"
#include "doomo/save_budget.h"
#include "../src/story_int.h"

/* side_callback()'s range (story_int.h: story_script.c 1..63, story_side.c
 * 64..127, story_places.c 128..255) */
#define SIDE_CB_FIRST 64
#define SIDE_CB_LAST 127

TEST(k0_story_callback_blocks)
{
    /* right after CB_SIDE_BRANCH, in order, disjoint, with their sizes */
    CHECK_EQ(CB_SIDE_BRANCH, 83);
    CHECK_EQ(CB_LIFE_FIRST, CB_SIDE_BRANCH + 1);
    CHECK_EQ(CB_LIFE_LAST - CB_LIFE_FIRST + 1, 16);
    CHECK_EQ(CB_END_FIRST, CB_LIFE_LAST + 1);
    CHECK_EQ(CB_END_LAST - CB_END_FIRST + 1, 8);
    CHECK_EQ(CB_ROUTE_FIRST, CB_END_LAST + 1);
    CHECK_EQ(CB_ROUTE_LAST - CB_ROUTE_FIRST + 1, 8);
    CHECK_EQ(CB_FAM_FIRST, CB_ROUTE_LAST + 1);              /* section 6 */
    CHECK_EQ(CB_FAM_LAST - CB_FAM_FIRST + 1, 8);
    CHECK_EQ(CB_FAM_LAST, 123);
    /* all inside story_side.c's dispatch range, so side_callback gets them */
    CHECK(CB_LIFE_FIRST >= SIDE_CB_FIRST && CB_FAM_LAST <= SIDE_CB_LAST);
    /* the ids the older side blocks use stay below the reserved ones */
    CHECK((int)CB_SIDE_CHAPTER < (int)CB_LIFE_FIRST && (int)CB_SIDE_NEWS < (int)CB_LIFE_FIRST);
    /* an unfilled block's id does nothing (no crash, no VM write) */
    {
        int cb;
        for (cb = CB_LIFE_FIRST; cb <= CB_FAM_LAST; cb++) side_callback(cb, 0);
        CHECK(1);
    }
}

TEST(k0_plat_ext_weak_defaults)
{
    uint8_t buf[PLAT_CONFIG_MAX + 4];
    uint32_t size = 123;
    /* the bits are distinct and inside the known mask */
    static const uint32_t bits[] = { PLATF_VMU_LCD, PLATF_RUMBLE, PLATF_POINTER, PLATF_KEYBOARD,
                                     PLATF_MULTIPAD, PLATF_EXCLUSIVE, PLATF_3D_QUALITY,
                                     PLATF_DC_DISPLAY, PLATF_CONFIG,
                                     PLATF_TALK /* T0 */ };
    uint32_t all = 0;
    int i;
    for (i = 0; i < DOOMO_COUNTOF(bits); i++) {
        CHECK(bits[i] && !(bits[i] & (bits[i] - 1)));        /* one bit each */
        CHECK(!(all & bits[i]));
        all |= bits[i];
    }
    CHECK_EQ_U(all, PLATF_ALL_KNOWN);
    /* U0's settings record + the adults' security code (section 6) */
    CHECK_EQ(PLAT_CONFIG_MAX, 48);
    CHECK_EQ(PLAT_CONFIG_MAX, CONFIG_SZ_SETTINGS + CONFIG_SZ_SECURITY);
    CHECK_EQ_U(PLAT_TEST_NO_PICKER, 1u);
    /* the native runner implements none of them yet (U0, E2 will; U4 is done):
     * the defaults answer "not here" and never touch their buffers */
    CHECK_EQ_U(plat_features() & ~PLATF_ALL_KNOWN, 0u);
    memset(buf, 0xA5, sizeof(buf));
    if (!(plat_features() & PLATF_CONFIG)) {
        CHECK_EQ(plat_config_read(buf, sizeof(buf)), 0);
        CHECK_EQ(buf[0], 0xA5);
        CHECK_EQ(plat_config_write(buf, PLAT_CONFIG_MAX), 0);
    }
    plat_option_changed(0, 0);                              /* no effect */
    CHECK_EQ_U(plat_test_flags() & ~PLAT_TEST_NO_PICKER, 0u);
    CHECK_EQ(plat_hold_frame(0), 0);
    if (!plat_pack_blob(&size)) CHECK_EQ_U(size, 0u);
    plat_pack_blob(NULL);                                   /* NULL size is fine */
    if (!plat_pack_blob(NULL)) CHECK_EQ_U(plat_pack_read(0, buf, 16), 0u);
}

/* The placeholders pop themselves on their first update, so the scene
 * below resumes; they are overlays with no draw (the scene below keeps
 * drawing) and log a STUB line so --assert-no-stubs catches them. */
TEST(k0_reserved_scenes_pop_themselves)
{
    const DoomoScene *const k0[] = { &SCENE_FAMILY, &SCENE_PLAYAS, &SCENE_ENDING_CARD,
                                     &SCENE_MSLOT, &SCENE_LANG_PICK, &SCENE_SECURITY_CODE };
    static const char *const names[] = { "FAMILY", "PLAYAS", "ENDING_CARD", "MSLOT", "LANG_PICK",
                                         "SECURITY_CODE" };
    int i, j, k;
    for (i = 0; i < DOOMO_COUNTOF(k0); i++) {
        char want[48];
        int seen = 0;
        CHECK_STR(k0[i]->name, names[i]);
        CHECK(scene_by_name(names[i]) == k0[i]);
        if (!scene_is_reserved_stub(k0[i])) continue;       /* its owner landed */
        CHECK(!scene_is_stub(k0[i]));                       /* not stubs.c's kind */
        CHECK(k0[i]->overlay && !k0[i]->draw);
        scene_set(&SCENE_UI_GALLERY, 0);
        scene_push(k0[i], 7);
        CHECK(scene_top() == k0[i]);
        CHECK_EQ(scene_depth(), 2);
        plat_begin_frame();
        scene_draw();                                       /* draws the gallery */
        plat_end_frame();
        scene_update();
        CHECK(scene_top() == &SCENE_UI_GALLERY);
        CHECK_EQ(scene_depth(), 1);
        snprintf(want, sizeof(want), "SCENE_%s", names[i]);
        for (k = 0; k < nat_stub_count(); k++) if (!strcmp(nat_stub_name(k), want)) seen = 1;
        CHECK_MSG(seen || nat_stub_count() >= 128 /* the runner's list is full */,
                  "no STUB line for %s", want);
        /* the only scene on the stack hands over to the title */
        scene_set(k0[i], 0);
        scene_update();
        CHECK(scene_top() == &SCENE_TITLE);
        CHECK_EQ(scene_depth(), 1);
        for (j = 0; j < i; j++) CHECK(k0[j] != k0[i]);
    }
    scene_set(&SCENE_UI_GALLERY, 0);
}

/* The shared numbers (life_contract.h, LIFESIM_PLAN section 6): the
 * chunk sections add up to the budget save_budget.h declares, and the
 * household tiers are the user's 3 -> 10 -> 20 (GBA 3 -> 6 -> 10). */
TEST(k0_life_contract_sizes)
{
    CHECK_EQ(LIFE_SZ_TOTAL, SAVE_EXT_MAX_LIFE);
    CHECK_EQ(LIFE_SZ_LS1_PLAN, 2432);                       /* LS1's plan, unchanged */
    CHECK_EQ(PLAY_SZ_TOTAL, SAVE_EXT_MAX_PLAY);
    CHECK(PLAY_POCKET_ITEMS * 2 + 16 <= PLAY_SZ_POCKET);    /* items + stats fit a pocket */
    CHECK_EQ(PROF_SZ_TOTAL, SAVE_EXT_MAX_PROF);
    CHECK_EQ(PROF_PROFILES_MAX, PROF_GUESTS + 1);
    CHECK_EQ(HOUSEHOLD_TIER_COUNT, 3);
    CHECK_EQ(HOUSEHOLD_CAP(0, HOUSEHOLD_TIER_START), 3);
    CHECK_EQ(HOUSEHOLD_CAP(0, HOUSEHOLD_TIER_GAME_DONE), 10);
    CHECK_EQ(HOUSEHOLD_CAP(0, HOUSEHOLD_TIER_NGPLUS_DONE), 20);
    CHECK_EQ(HOUSEHOLD_CAP(1, HOUSEHOLD_TIER_START), 3);
    CHECK_EQ(HOUSEHOLD_CAP(1, HOUSEHOLD_TIER_GAME_DONE), 6);
    CHECK_EQ(HOUSEHOLD_CAP(1, HOUSEHOLD_TIER_NGPLUS_DONE), 10);
    CHECK_EQ(HOUSEHOLD_CAP_MAX, 20);
    /* 8 B memorial per member of the largest household */
    CHECK_EQ(LIFE_SZ_F6_LOSS, HOUSEHOLD_CAP_MAX * 8);
}
