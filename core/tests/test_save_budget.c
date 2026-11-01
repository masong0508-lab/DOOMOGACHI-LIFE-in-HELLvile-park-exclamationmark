/* test_save_budget.c - the K0 save registry and the joint worst case with
 * the four chunks K0 registers (LIFE, PLAY, ENDS, OPT2; LIFESIM_PLAN R1,
 * section 2.5). Owner: K0 contract.
 *
 * The chunks' owners (LS1, PA1, E1, U0) land later; until then their
 * registry pointers are weak NULL and nothing is written. These tests hold
 * the contract they must keep: their tags, their declared maxima, and that
 * both save slots still fit with every chunk at its worst. The same worst
 * case on the real codec is test_save.c's save_k0_joint_worst_case.
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/save_budget.h"
#include "doomo/home.h"

/* A post-game story save as the life-sim scouts measured it (LIFESIM_PLAN
 * 2.5: town body + every chunk that exists today, STRY included). It is
 * bigger than SAVE_BUDGET_TOWN_BOUND_STORY, the G0 measurement. */
#define K0_POSTGAME_STORY 19342u

static uint32_t lz(uint32_t raw) { return (raw * 129u + 127u) / 128u; }

TEST(save_registry_k0_slots)
{
    const DoomoSaveExt *const k0[4] = {
        doomo_save_ext_life, doomo_save_ext_playas, doomo_save_ext_endings, doomo_save_ext_opt2 };
    static const uint32_t tags[4] = {
        SAVE_EXT_TAG_LIFE, SAVE_EXT_TAG_PLAY, SAVE_EXT_TAG_ENDS, SAVE_EXT_TAG_OPT2 };
    int i, j;
    CHECK_EQ(SAVE_EXT_SLOTS, 11);
    CHECK_EQ(SAVE_EXT_TAG_LIFE, SAVE_EXT_TAG('L', 'I', 'F', 'E'));
    CHECK_EQ(SAVE_EXT_TAG_PLAY, SAVE_EXT_TAG('P', 'L', 'A', 'Y'));
    CHECK_EQ(SAVE_EXT_TAG_ENDS, SAVE_EXT_TAG('E', 'N', 'D', 'S'));
    CHECK_EQ(SAVE_EXT_TAG_OPT2, SAVE_EXT_TAG('O', 'P', 'T', '2'));
    for (i = 0; i < 4; i++) {
        /* a strong descriptor, once its owner lands, carries its K0 tag and
         * declares a max_size within K0's budget */
        if (k0[i]) {
            CHECK_MSG(k0[i]->tag == tags[i], "K0 slot %d has tag %.4s", i, (const char *)&k0[i]->tag);
            CHECK(k0[i]->max_size > 0 && k0[i]->max_size <= save_budget_declared_max(tags[i]));
        }
        for (j = 0; j < i; j++) CHECK(tags[i] != tags[j]);
    }
    /* no chunk registered today uses a K0 tag except through its K0 slot */
    for (i = 0; i < save_ext_count(); i++) {
        const DoomoSaveExt *e = save_ext_at(i);
        for (j = 0; j < 4; j++)
            if (e->tag == tags[j]) CHECK_MSG(e == k0[j], "tag %.4s outside its K0 slot", (const char *)&tags[j]);
    }
}

TEST(save_budget_k0_declared_maxima)
{
    CHECK_EQ(SAVE_EXT_MAX_LIFE, 2432);
    CHECK_EQ(SAVE_EXT_MAX_PLAY, 32);
    CHECK_EQ(SAVE_EXT_MAX_ENDS, 32);
    CHECK_EQ(SAVE_EXT_MAX_OPT2, 40);
    CHECK_EQ(SAVE_BUDGET_K0_RAW, 2432 + 32 + 32 + 40);
    CHECK_EQ(save_budget_declared_max(SAVE_EXT_TAG_LIFE), SAVE_EXT_MAX_LIFE);
    CHECK_EQ(save_budget_declared_max(SAVE_EXT_TAG_PLAY), SAVE_EXT_MAX_PLAY);
    CHECK_EQ(save_budget_declared_max(SAVE_EXT_TAG_ENDS), SAVE_EXT_MAX_ENDS);
    CHECK_EQ(save_budget_declared_max(SAVE_EXT_TAG_OPT2), SAVE_EXT_MAX_OPT2);
    CHECK_EQ(save_budget_declared_max(SAVE_EXT_TAG('H', 'O', 'M', 'E')), 0u);
    CHECK_EQ(save_budget_declared_max(0), 0u);
    CHECK(SAVE_EXT_MAX_LIFE <= DOOMO_SAVE_EXT_MAX);
    /* the file-size cost: the LZ's worst case per chunk + a header each */
    CHECK_EQ(SAVE_BUDGET_K0_WORST, lz(2432) + lz(32) + lz(32) + lz(40) + 4 * SAVE_BUDGET_CHUNK_HDR);
    CHECK_EQ(SAVE_SLOT_STORY_CAP_CFG, 36864);
    CHECK(SAVE_SLOT_STORY_CAP_CFG < SAVE_SLOT_STORY_CAP);
}

/* The computed joint worst case (section 2.5): every chunk at its declared
 * maximum, incompressible, on top of the biggest measured town body, fits
 * both slots with the budget's margin to spare - the story slot also at
 * the 36,864 B cap U1g leaves once the GBA config sector is carved out. */
TEST(save_budget_k0_joint_worst_case)
{
    uint32_t home = lz(HOME_CHUNK_WORST) + SAVE_BUDGET_CHUNK_HDR;
    uint32_t story_g0 = SAVE_BUDGET_TOWN_BOUND_STORY + home + SAVE_BUDGET_K0_WORST;
    uint32_t story_pg = K0_POSTGAME_STORY + home + SAVE_BUDGET_K0_WORST;
    uint32_t fp = SAVE_BUDGET_TOWN_BOUND_FP + home + SAVE_BUDGET_K0_WORST;
    printf("      joint worst: story %u B (post-game %u B), free play %u B\n",
           (unsigned)story_g0, (unsigned)story_pg, (unsigned)fp);
    CHECK(story_pg + SAVE_BUDGET_MARGIN <= SAVE_SLOT_STORY_CAP_CFG);
    CHECK(story_pg + SAVE_BUDGET_MARGIN <= SAVE_SLOT_STORY_CAP);
    CHECK(story_g0 < story_pg);
    CHECK(fp + SAVE_BUDGET_MARGIN <= SAVE_SLOT_FP_CAP);
    /* the same through the live budget's formula: HOME + the four chunks
     * all new since the last save */
    {
        uint32_t now[5] = { HOME_CHUNK_WORST, SAVE_EXT_MAX_LIFE, SAVE_EXT_MAX_PLAY,
                            SAVE_EXT_MAX_ENDS, SAVE_EXT_MAX_OPT2 };
        uint32_t at[5] = { 0, 0, 0, 0, 0 };
        int32_t left = save_budget_compute(SAVE_SLOT_STORY_CAP_CFG, K0_POSTGAME_STORY, now, at, 5);
        printf("      story slot at 36 KB: %d B left after the margin\n", (int)left);
        CHECK(left >= 0);
        left = save_budget_compute(SAVE_SLOT_FP_CAP, SAVE_BUDGET_TOWN_BOUND_FP, now, at, 5);
        CHECK(left >= 0);
        /* and the four K0 chunks alone add exactly their declared worst
         * (header included: they are new) */
        CHECK_EQ(save_budget_compute(SAVE_SLOT_FP_CAP, 0, now + 1, at, 4),
                 (int32_t)(SAVE_SLOT_FP_CAP - SAVE_BUDGET_MARGIN - SAVE_BUDGET_K0_WORST));
    }
}

/* With the K0 chunks still weak, the registry is what it was: the live
 * budget only sees the registered chunks, so a story town's budget is
 * unchanged by K0 (no chunk counted, no header). */
TEST(save_budget_k0_weak_chunks_cost_nothing)
{
    int i, k0 = 0;
    const DoomoSaveExt *const k0s[4] = {
        doomo_save_ext_life, doomo_save_ext_playas, doomo_save_ext_endings, doomo_save_ext_opt2 };
    for (i = 0; i < 4; i++) if (k0s[i]) k0++;
    if (k0) return;              /* an owner landed: its own tests take over */
    for (i = 0; i < save_ext_count(); i++) {
        uint32_t t = save_ext_at(i)->tag;
        CHECK(save_budget_declared_max(t) == 0);
    }
}
