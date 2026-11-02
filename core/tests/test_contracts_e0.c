/* test_contracts_e0.c - the endings / content pack contracts E0 lands
 * (LIFESIM_PLAN E0, section 6 amendments): ending.h ids and categories,
 * the 'ENDS' layout, pack.h's file layout and op whitelist, the new VM
 * ops and push kinds, and the safe weak defaults. Owner: E0 contract.
 * (The VM side - JMAT, SHOW, PEND_ENDING / PEND_MSLOT, TXF_RAW user packs -
 * is in test_story_vm.c.)
 *
 * Every check holds before and after the owners land (E1 ending.c, E2
 * pack.c, U0 opt_mature): the safety checks ask only what any correct
 * implementation must answer (a kid is never a mature adult; no pack is
 * open on a plain native run), and the visibility rule is checked against
 * whatever opt_mature() says.
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/ending.h"
#include "doomo/pack.h"
#include "doomo/story_vm.h"
#include "doomo/txt.h"
#include "doomo/save_budget.h"
#include "doomo/unlock_ids.h"
#include "doomo/roster.h"

TEST(e0_endings_are_14_plus_the_bonus)
{
    int k, n[ENDCAT_COUNT] = { 0 };
    CHECK_EQ(ENDING_COUNT, 15);
    CHECK_EQ(ENDING_MAIN_COUNT, 14);
    CHECK_EQ(ENDING_NOPE, 14);                   /* the very secret 15th (answer 19) */
    /* one unlock id each, the whole K0 range and nothing past it */
    CHECK_EQ(UL_STORY_END(ENDING_E01), UL_STORY_END_FIRST);
    CHECK_EQ(UL_STORY_END(ENDING_NOPE), UL_STORY_END_LAST);
    /* the 'ENDS' chunk is exactly K0's declared maximum */
    CHECK_EQ((int)sizeof(DoomoEnds), (int)SAVE_EXT_MAX_ENDS);
    CHECK((int)sizeof(((DoomoEnds *)0)->seen) * 8 >= ENDING_COUNT);
    /* the categories: 4 good, 4 bad, 3 secret, 3 sexy, the bonus */
    for (k = 0; k < ENDING_COUNT; k++) {
        int c = ending_category(k);
        CHECK(c >= 0 && c < ENDCAT_COUNT);
        n[c]++;
    }
    CHECK_EQ(n[ENDCAT_GOOD], 4);
    CHECK_EQ(n[ENDCAT_BAD], 4);
    CHECK_EQ(n[ENDCAT_SECRET], 3);
    CHECK_EQ(n[ENDCAT_SEXY], 3);
    CHECK_EQ(n[ENDCAT_BONUS], 1);
    CHECK_EQ(ending_category(ENDING_E04), ENDCAT_GOOD);
    CHECK_EQ(ending_category(ENDING_E08), ENDCAT_BAD);
    CHECK_EQ(ending_category(ENDING_E10), ENDCAT_SECRET);
    CHECK_EQ(ending_category(ENDING_E12), ENDCAT_SEXY);
    CHECK_EQ(ending_category(ENDING_NOPE), ENDCAT_BONUS);
    CHECK_EQ(ending_category(-1), -1);
    CHECK_EQ(ending_category(ENDING_COUNT), -1);
}

TEST(e0_ending_visibility_rules)
{
    int k;
    test_reset();
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    for (k = 0; k < ENDING_COUNT; k++) {
        int c = ending_category(k);
        if (c == ENDCAT_SEXY)                     /* hidden while Mature is Off (answer 21) */
            CHECK_EQ(ending_visible(k), opt_mature() != MATURE_OFF);
        else if (c == ENDCAT_BONUS)               /* only once found (answer 19) */
            CHECK_EQ(ending_visible(k), ending_seen(k));
        else
            CHECK_EQ(ending_visible(k), ENDING_IS_RETIRED(k) ? 0 : 1);
    }
    CHECK_EQ(ending_visible(-1), 0);
    CHECK_EQ(ending_visible(ENDING_COUNT), 0);
    /* the counter is out of the 14: the bonus never counts */
    CHECK(ending_count_seen() >= 0 && ending_count_seen() <= ENDING_MAIN_COUNT);
    CHECK(opt_mature() >= MATURE_OFF && opt_mature() < MATURE_LEVELS);
}

TEST(e0_the_adult_gate_and_packs_are_safe_by_default)
{
    DoomoPackInfo info;
    uint32_t size = 123;
    test_reset();
    town_new(TOWN_STORY, 5, TOWN_POP_PREMADE);
    /* a kid, nobody and out-of-range ids are never mature adults */
    CHECK_EQ(mature_adult_ok(ROSTER_ID_PIP), 0);
    CHECK_EQ(mature_adult_ok(-1), 0);
    CHECK_EQ(mature_adult_ok(DOOMO_MAX_RESIDENTS + 5000), 0);
    /* a plain native run has no content pack */
    CHECK_EQ(pack_open(), 0);
    memset(&info, 0x5A, sizeof(info));
    CHECK_EQ(pack_info(&info), 0);
    CHECK_EQ(info.name[0], 0);
    CHECK_EQ(info.total_size, 0u);
    CHECK(pack_art(PK_KIND_CG, PACK_UID(PK_KIND_CG, 0), &size) == NULL);
    CHECK_EQ(size, 0u);
    CHECK_EQ(pack_script(0), 0);
    CHECK(ending_resolve_finale() >= ENDING_E01 && ending_resolve_finale() < ENDING_MAIN_COUNT);
}

TEST(e0_pack_format_and_the_script_whitelist)
{
    int op;
    CHECK_EQ((int)sizeof(DoomoPackHeader), (int)PACK_HEADER_SIZE);
    CHECK_EQ((int)sizeof(DoomoPackEntry), (int)PACK_ENTRY_SIZE);
    CHECK_EQ(PACK_MAGIC, 0x4B504D44u);           /* "DMPK" on disk, little endian */
    CHECK(PACK_UID(PK_KIND_CG, 0) == PACK_UID_BASE);
    CHECK(PACK_UID(PK_KIND_SCRIPT, 0xFFFF) < PACK_UID_BASE + 0x00100000u);
    CHECK(PACK_UID_SCRIPT(3) == PACK_UID(PK_KIND_SCRIPT, 3));
    CHECK(PACK_CAP_GBA_DEFAULT <= PACK_CAP_GBA_HARD);
    CHECK(MSLOT_MAX <= 64);
    /* the whitelist fits the op ids and allows only harmless ops */
    CHECK(OP_COUNT <= 64);
    CHECK(PACK_OP_OK(OP_SAY) && PACK_OP_OK(OP_CHOICE) && PACK_OP_OK(OP_END));
    CHECK(PACK_OP_OK(OP_JMAT) && PACK_OP_OK(OP_SHOW) && PACK_OP_OK(OP_DATA));
    CHECK(PACK_OP_OK(OP_WAIT) && PACK_OP_OK(OP_FADE) && PACK_OP_OK(OP_CALL));
    {
        static const int never[] = { OP_SETF, OP_CLRF, OP_BEAT, OP_PUSH, OP_POSSESS, OP_UNPOSSESS,
                                     OP_GIVE, OP_TAKE, OP_REWARD, OP_BEFRIEND, OP_TELEPORT, OP_CAST,
                                     OP_NPC, OP_CAM, OP_INSERT, OP_CUT, OP_FREEZE_CAP, OP_HOME,
                                     OP_QUEST, OP_SET_BEAT, OP_LATER, OP_TIME, OP_NATIVE, OP_STASH };
        unsigned i;
        for (i = 0; i < DOOMO_COUNTOF(never); i++)
            CHECK_MSG(!PACK_OP_OK(never[i]), "op %d is allowed in a user pack", never[i]);
    }
    for (op = OP_COUNT; op < 70; op++) CHECK(!PACK_OP_OK(op));
    /* user text packs live above every built-in pack */
    CHECK_EQ(TXT_PACK_USER_FIRST + TXT_PACK_USER_COUNT, TXT_PACK_MAX);
    CHECK((int)doomo_txt_npacks <= TXT_PACK_USER_FIRST);
}

TEST(e0_vm_ops_and_push_kinds_are_appended)
{
    /* append-only: the old ids are where packs store them */
    CHECK_EQ(OP_STASH, 55);
    CHECK_EQ(OP_JMAT, OP_STASH + 1);
    CHECK_EQ(OP_SHOW, OP_STASH + 2);
    CHECK_EQ(OP_COUNT, OP_SHOW + 1);
    CHECK_EQ(PEND_MINIGAME_SIDE, 15);
    CHECK_EQ(PEND_ENDING, PEND_MINIGAME_SIDE + 1);
    CHECK_EQ(PEND_MSLOT, PEND_MINIGAME_SIDE + 2);
    CHECK_EQ(PEND_COUNT, PEND_MSLOT + 1);
    CHECK_EQ(JMAT_LEVEL_MASK & JMAT_NOT, 0);
    CHECK(MATURE_PACK <= JMAT_LEVEL_MASK);
    CHECK_EQ(SHOW_OFF & 0xFF, SHOW_OFF);
    CHECK(PK_KIND_COUNT < SHOW_OFF);
}
