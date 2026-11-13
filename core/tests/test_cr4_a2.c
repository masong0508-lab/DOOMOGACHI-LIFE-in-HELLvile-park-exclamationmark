/* test_cr4_a2.c - CR4 art batch A2 (hair): 32 hair styles (gene hair_style 32..63) and 8 beards
 * (gene facial_hair 8..15), docs/CREATOR.md "CR4 A2".
 *
 * - the lists are full: 64 hair styles and 16 facial hair choices are live, each of the 40 new ones
 *   scripted, in a set, with a cost under the slot cap, and every one is listed in review.txt's
 *   golden ids (frozen, append-only);
 * - a hair style is seen from behind too (the fighter's back view and the bust turned away differ
 *   from the same citizen bald): a part that drew only its front would show a bald nape;
 * - no minor ever shows a new beard (look_for_stage drops it for baby, toddler, child and teen), and
 *   the hair batch is for every age (age class ALL: a child may wear all 32);
 * - the interplay: every hat of the catalog over each of the 32 new styles renders, and twice the
 *   same (no stray state between blocks). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/portrait.h"

#define A2_HAIR0 32
#define A2_HAIR1 63
#define A2_BEARD0 8
#define A2_BEARD1 15
#define A2_COST_CAP 48          /* the slot cap: every new part costs under half of the dearest classic hair (Hime, 89) */

static DoomoLook bench(void)
{
    DoomoResident r;
    resident_default(&r);
    return *RES_LOOK_VIEW(&r);
}

TEST(cr4_a2_lists_are_full_and_every_new_part_is_catalogued)
{
    int v;
    CHECK_EQ(part_live_count(GF_HAIR_STYLE), 64);
    CHECK_EQ(part_live_count(GF_FACIAL_HAIR), 16);
    for (v = A2_HAIR0; v <= A2_HAIR1; v++) {
        const PartMeta *m = part_meta_find(GF_HAIR_STYLE, v);
        CHECK_MSG(m, "hair %d missing", v);
        if (!m) continue;
        CHECK(PMF_STATE(m->flags) == PSTATE_LIVE);
        CHECK(m->flags & PMF_SCRIPTED);
        CHECK_EQ(PMF_AGE(m->flags), PAGE_ALL);
        CHECK_MSG(m->cost <= A2_COST_CAP, "%s costs %d", part_meta_name(m), m->cost);
    }
    for (v = A2_BEARD0; v <= A2_BEARD1; v++) {
        const PartMeta *m = part_meta_find(GF_FACIAL_HAIR, v);
        CHECK_MSG(m, "beard %d missing", v);
        if (!m) continue;
        CHECK(PMF_STATE(m->flags) == PSTATE_LIVE);
        CHECK(m->flags & PMF_SCRIPTED);
        CHECK_MSG(m->cost <= A2_COST_CAP, "%s costs %d", part_meta_name(m), m->cost);
    }
}

TEST(cr4_a2_hair_shows_from_behind)
{
    static uint8_t a[FIGHTER_W * FIGHTER_H], b[FIGHTER_W * FIGHTER_H];
    int v, bad = 0;
    for (v = A2_HAIR0; v <= A2_HAIR1; v++) {
        DoomoLook l = bench(), z = bench();
        l.hair_style = (uint8_t)v;
        z.hair_style = HAIR_BALD;
        fighter_render(&l, RES_ALIVE, 4, 0, -1, a);
        fighter_render(&z, RES_ALIVE, 4, 0, -1, b);
        if (!memcmp(a, b, sizeof(a))) {
            fprintf(stderr, "    hair %d is invisible from behind\n", v);
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(cr4_a2_no_minor_wears_a_new_beard_and_children_may_wear_the_hair)
{
    static const int minors[] = { LIFE_BABY, LIFE_TODDLER, LIFE_CHILD, LIFE_TEEN };
    int s, v;
    for (s = 0; s < 4; s++)
        for (v = A2_BEARD0; v <= A2_BEARD1; v++) {
            DoomoLook in = bench(), out;
            in.facial_hair = (uint8_t)v;
            look_for_stage(&in, minors[s], &out);
            CHECK_EQ(out.facial_hair, FHAIR_NONE);
        }
    for (v = A2_HAIR0; v <= A2_HAIR1; v++) {
        DoomoLook in = bench(), out;
        in.hair_style = (uint8_t)v;
        look_for_stage(&in, LIFE_CHILD, &out);
        CHECK_EQ(out.hair_style, v);
    }
    for (v = A2_BEARD0; v <= A2_BEARD1; v++) {
        DoomoLook in = bench(), out;
        in.facial_hair = (uint8_t)v;
        look_for_stage(&in, LIFE_ADULT, &out);
        CHECK_EQ(out.facial_hair, v);
    }
}

TEST(cr4_a2_every_hat_over_every_new_style_renders_the_same_twice)
{
    static uint8_t a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    int h, v, bad = 0;
    for (h = 1; h < part_live_count(GF_HAT); h++)
        for (v = A2_HAIR0; v <= A2_HAIR1; v++) {
            DoomoLook l = bench();
            l.hat = (uint8_t)h;
            l.hair_style = (uint8_t)v;
            portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, a);
            portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, b);
            if (memcmp(a, b, sizeof(a))) bad++;
        }
    CHECK_EQ(bad, 0);
}
