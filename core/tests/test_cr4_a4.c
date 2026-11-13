/* test_cr4_a4.c - CR4 art batch A4 (fur and skin): 24 fur patterns (gene fur_pattern 1..24) and 9 face marks
 * (gene marking 23..31), docs/CREATOR.md "CR4 batch A4".
 *
 * - the fur pattern list is live (25 values) and the marking list is full (32): every new part is scripted,
 *   live, age class ALL, in a set and inside the cost cap;
 * - each pattern changes the bust against the same citizen without one, and no two patterns draw the same
 *   bust (a pattern the painter ignored would equal "None");
 * - the patterns follow the third fur colour: with another spot colour the bust changes again;
 * - a mark past the painted ones (MARK_COUNT..31) is drawn: the sanitiser in portrait.c keeps a scripted
 *   mark, and it differs from "None" and from every painted mark;
 * - every stage keeps them (age class ALL: a child may wear all of them);
 * - the gene values survive a save and resident_sanitize keeps them, one past the live count is not a choice;
 * - the part scripts draw nothing outside the head part: the figure below the neck is the same with and
 *   without a pattern. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/look_fields.h"
#include "doomo/portrait.h"
#include "doomo/save.h"

#define A4_COST_CAP 40

static DoomoLook bench(void)
{
    DoomoResident r;
    resident_default(&r);
    return *RES_LOOK_VIEW(&r);
}

static int diff(const uint8_t *a, const uint8_t *b, int n)
{
    int i, d = 0;
    for (i = 0; i < n; i++) d += a[i] != b[i];
    return d;
}

static void fur_bust(int v, int spot, uint8_t *out)
{
    DoomoLook l = bench();
    DoomoDeep d = doomo_deep_zero;
    look_part_set(&l, &d, GF_FUR_PATTERN, v);
    if (spot) look_part_set(&l, &d, GF_FUR_COLOR3, spot);
    portrait_render_deep(&l, &d, RES_ALIVE, EXPR_NEUTRAL, 0, out);
}

TEST(cr4_a4_the_lists_are_live_and_every_new_part_is_catalogued)
{
    int v, n = 0;
    CHECK_EQ(part_live_count(GF_FUR_PATTERN), 25);
    CHECK_EQ(part_live_count(GF_MARKING), 32);
    for (v = 1; v <= 24; v++) {
        const PartMeta *m = part_meta_find(GF_FUR_PATTERN, v);
        CHECK_MSG(m, "fur pattern %d missing", v);
        if (!m) continue;
        CHECK(PMF_STATE(m->flags) == PSTATE_LIVE);
        CHECK(m->flags & PMF_SCRIPTED);
        CHECK_EQ(PMF_AGE(m->flags), PAGE_ALL);
        CHECK_MSG(m->cost <= A4_COST_CAP, "%s costs %d", part_meta_name(m), m->cost);
        n++;
    }
    for (v = MARK_COUNT; v <= 31; v++) {
        const PartMeta *m = part_meta_find(GF_MARKING, v);
        CHECK_MSG(m, "mark %d missing", v);
        if (!m) continue;
        CHECK(PMF_STATE(m->flags) == PSTATE_LIVE);
        CHECK(m->flags & PMF_SCRIPTED);
        CHECK_EQ(PMF_AGE(m->flags), PAGE_ALL);
        CHECK_MSG(m->cost <= A4_COST_CAP, "%s costs %d", part_meta_name(m), m->cost);
        n++;
    }
    CHECK_EQ(MARK_COUNT, 23);
    CHECK_EQ(n, 33);
}

TEST(cr4_a4_every_fur_pattern_changes_the_bust_and_no_two_are_alike)
{
    static uint8_t none[PORTRAIT_W * PORTRAIT_H], a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    int v, w, bad = 0;
    fur_bust(0, 0, none);
    for (v = 1; v <= 24; v++) {
        fur_bust(v, 0, a);
        if (diff(a, none, (int)sizeof(a)) < 8) {
            fprintf(stderr, "    fur pattern %d hardly shows\n", v);
            bad++;
        }
        for (w = v + 1; w <= 24; w++) {
            fur_bust(w, 0, b);
            if (diff(a, b, (int)sizeof(a)) < 8) {
                fprintf(stderr, "    fur patterns %d and %d look alike\n", v, w);
                bad++;
            }
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(cr4_a4_a_fur_pattern_follows_the_spot_colour)
{
    static uint8_t a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    int v, bad = 0;
    for (v = 1; v <= 24; v++) {
        fur_bust(v, 0, a);
        fur_bust(v, 1 + PAINT_COBALT, b);
        if (!diff(a, b, (int)sizeof(a))) {
            /* the pale-and-dusk tones of a pattern still follow the skin, but at least the main colour must move */
            fprintf(stderr, "    fur pattern %d ignores the spot colour\n", v);
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(cr4_a4_the_new_marks_are_drawn_and_differ_from_the_old_ones)
{
    static uint8_t none[PORTRAIT_W * PORTRAIT_H], a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    int v, o, bad = 0;
    DoomoLook l = bench();
    l.marking = MARK_NONE;
    portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, none);
    for (v = MARK_COUNT; v <= 31; v++) {
        l.marking = (uint8_t)v;
        portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, a);
        if (diff(a, none, (int)sizeof(a)) < 8) {
            fprintf(stderr, "    mark %d hardly shows\n", v);
            bad++;
        }
        for (o = 1; o < 31; o++) {
            DoomoLook z = bench();
            if (o == v) continue;
            z.marking = (uint8_t)o;
            if (o >= MARK_COUNT && o < v) continue;      /* each pair of new marks once */
            portrait_render(&z, RES_ALIVE, EXPR_NEUTRAL, 0, b);
            if (o < MARK_COUNT && !diff(a, b, (int)sizeof(a))) {
                fprintf(stderr, "    mark %d equals the old mark %d\n", v, o);
                bad++;
            }
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(cr4_a4_every_stage_keeps_them_and_a_child_may_wear_all)
{
    static const int stages[] = { LIFE_BABY, LIFE_TODDLER, LIFE_CHILD, LIFE_TEEN, LIFE_ADULT, LIFE_ELDER };
    int k, v;
    for (k = 0; k < 6; k++) {
        for (v = 1; v <= 24; v++) CHECK(part_age_ok(GF_FUR_PATTERN, v, stages[k]));
        for (v = MARK_COUNT; v <= 31; v++) {
            DoomoLook in = bench(), out;
            in.marking = (uint8_t)v;
            look_for_stage(&in, stages[k], &out);
            CHECK_EQ(out.marking, v);
            CHECK(part_age_ok(GF_MARKING, v, stages[k]));
        }
    }
}

TEST(cr4_a4_gene_values_survive_a_save_and_sanitize)
{
    static uint8_t img[DOOMO_SAVE_SLOT1_MAX];
    static uint8_t want[DOOMO_GENE_BYTES];
    DoomoResident *r;
    uint32_t n;
    town_new(TOWN_FREEPLAY, 4242u, TOWN_POP_PREMADE);
    r = town_res(77);
    CHECK(r != NULL);
    if (r->status == RES_EMPTY) r->status = RES_ALIVE;
    res_gset(r, GF_FUR_PATTERN, 24);
    res_gset(r, GF_MARKING, 31);
    CHECK_EQ(resident_sanitize(r), 0);                          /* every one is a live choice: nothing is fixed */
    memcpy(want, r->gene, DOOMO_GENE_BYTES);
    n = save_serialize(img, sizeof(img));
    CHECK(n > 0);
    memset(r->gene + 30, 0, 18);
    CHECK(save_deserialize(img, n));
    r = town_res(77);
    CHECK(memcmp(r->gene, want, DOOMO_GENE_BYTES) == 0);
    CHECK_EQ((int)res_gget(r, GF_FUR_PATTERN), 24);
    CHECK_EQ((int)res_gget(r, GF_MARKING), 31);
    res_gset(r, GF_FUR_PATTERN, 25);                            /* one past the live count is not a choice */
    CHECK(resident_sanitize(r) > 0);
    CHECK_EQ((int)res_gget(r, GF_FUR_PATTERN), 0);
}

TEST(cr4_a4_patterns_stay_on_the_head)
{
    static uint8_t a[FIGURE_W * FIGURE_H], b[FIGURE_W * FIGURE_H];
    int v, bad = 0;
    for (v = 1; v <= 24; v++) {
        DoomoLook l = bench();
        DoomoDeep d = doomo_deep_zero, z = doomo_deep_zero;
        look_part_set(&l, &d, GF_FUR_PATTERN, v);
        figure_render_deep(&l, &d, RES_ALIVE, EXPR_NEUTRAL, 0, a);
        figure_render_deep(&l, &z, RES_ALIVE, EXPR_NEUTRAL, 0, b);
        /* the legs (the lower half of the figure) are the same */
        if (diff(a + (FIGURE_H * 3 / 4) * FIGURE_W, b + (FIGURE_H * 3 / 4) * FIGURE_W, (FIGURE_H / 4) * FIGURE_W)) {
            fprintf(stderr, "    fur pattern %d leaks below the head\n", v);
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}
