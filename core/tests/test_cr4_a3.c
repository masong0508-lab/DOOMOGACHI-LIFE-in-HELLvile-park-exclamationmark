/* test_cr4_a3.c - CR4 art batch A3 (spooky): 10 horns (gene horns 16..25), 16 tails (tail 16..31), 14 wings
 * (wings 10..23), 4 halos (halo 12..15) and 16 auras (aura 8..23), docs/CREATOR.md "CR4 batch A3".
 *
 * - the lists are full (26, 32, 24, 16, 24), every one of the 60 new parts is scripted, live, age class
 *   ALL, inside the cost cap (the part icon test covers their icons);
 * - a tail and a pair of wings are seen from the front, the side and from behind: the figure and the
 *   fighter's back view differ from the same citizen without them (a part that drew only one side would
 *   leave a bare back);
 * - an aura animates: its two frames (the breath phase) differ;
 * - nothing here is age restricted: every part is worn by a child, so none is gore, a nudity or a
 *   suggestive prop, and look_for_stage keeps them for every stage;
 * - the gene values survive a save (the aura's fifth bit rides in the creator trailer) and
 *   resident_sanitize keeps them;
 * - the old values draw exactly as before (the classic golden covers the pixels; here the painted
 *   horns, tails and wings still differ from the new ones and from each other). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/look_fields.h"
#include "doomo/portrait.h"
#include "doomo/save.h"

#define A3_COST_CAP 48          /* the slot cap: under half of the dearest classic part of these slots (Dragon wings 50) */

typedef struct A3Range { int list, first, last, live; } A3Range;
static const A3Range ranges[5] = {
    { GF_HORNS, 16, 25, 26 }, { GF_TAIL, 16, 31, 32 }, { GF_WINGS, 10, 23, 24 },
    { GF_HALO, 12, 15, 16 }, { GF_AURA, 8, 23, 24 },
};

static DoomoLook bench(void)
{
    DoomoResident r;
    resident_default(&r);
    return *RES_LOOK_VIEW(&r);
}

static uint8_t *look_byte(DoomoLook *l, int list)
{
    switch (list) {
    case GF_HORNS: return &l->horns;
    case GF_TAIL: return &l->tail;
    case GF_WINGS: return &l->wings;
    case GF_HALO: return &l->halo;
    default: return &l->aura;
    }
}

static int diff(const uint8_t *a, const uint8_t *b, int n)
{
    int i, d = 0;
    for (i = 0; i < n; i++) d += a[i] != b[i];
    return d;
}

TEST(cr4_a3_lists_are_full_and_every_new_part_is_catalogued)
{
    int s, v, n = 0;
    for (s = 0; s < 5; s++) {
        CHECK_EQ(part_live_count(ranges[s].list), ranges[s].live);
        for (v = ranges[s].first; v <= ranges[s].last; v++) {
            const PartMeta *m = part_meta_find(ranges[s].list, v);
            CHECK_MSG(m, "list %d value %d missing", ranges[s].list, v);
            if (!m) continue;
            CHECK(PMF_STATE(m->flags) == PSTATE_LIVE);
            CHECK(m->flags & PMF_SCRIPTED);
            CHECK_EQ(PMF_AGE(m->flags), PAGE_ALL);
            CHECK_MSG(m->cost <= A3_COST_CAP, "%s costs %d", part_meta_name(m), m->cost);
            n++;
        }
    }
    CHECK_EQ(n, 60);
}

TEST(cr4_a3_tails_and_wings_show_from_the_front_and_from_behind)
{
    static uint8_t a[FIGURE_W * FIGURE_H], b[FIGURE_W * FIGURE_H];
    static uint8_t fa[FIGHTER_W * FIGHTER_H], fb[FIGHTER_W * FIGHTER_H];
    int s, v, bad = 0;
    for (s = 1; s <= 2; s++)
        for (v = ranges[s].first; v <= ranges[s].last; v++) {
            DoomoLook l = bench(), z = bench();
            *look_byte(&l, ranges[s].list) = (uint8_t)v;
            figure_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, a);
            figure_render(&z, RES_ALIVE, EXPR_NEUTRAL, 0, b);
            if (diff(a, b, (int)sizeof(a)) < 12) {
                fprintf(stderr, "    list %d value %d is hidden on the figure\n", ranges[s].list, v);
                bad++;
            }
            fighter_render(&l, RES_ALIVE, 0, 0, -1, fa);              /* view 0: from behind */
            fighter_render(&z, RES_ALIVE, 0, 0, -1, fb);
            if (diff(fa, fb, (int)sizeof(fa)) < 4) {
                fprintf(stderr, "    list %d value %d is hidden from behind\n", ranges[s].list, v);
                bad++;
            }
        }
    CHECK_EQ(bad, 0);
}

TEST(cr4_a3_every_aura_has_two_frames)
{
    static uint8_t a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    int v, bad = 0;
    for (v = ranges[4].first; v <= ranges[4].last; v++) {
        DoomoLook l = bench();
        l.aura = (uint8_t)v;
        portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, a);
        portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 32, b);       /* the breath phase flips every 32 ticks */
        if (diff(a, b, (int)sizeof(a)) == 0) {
            fprintf(stderr, "    aura %d does not animate\n", v);
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(cr4_a3_a_child_may_wear_all_of_them_and_every_stage_keeps_them)
{
    static const int stages[] = { LIFE_BABY, LIFE_TODDLER, LIFE_CHILD, LIFE_TEEN, LIFE_ADULT, LIFE_ELDER };
    int s, v, k;
    for (k = 0; k < 6; k++)
        for (s = 0; s < 5; s++)
            for (v = ranges[s].first; v <= ranges[s].last; v++) {
                DoomoLook in = bench(), out;
                *look_byte(&in, ranges[s].list) = (uint8_t)v;
                look_for_stage(&in, stages[k], &out);
                CHECK_EQ(*look_byte(&out, ranges[s].list), v);
            }
}

TEST(cr4_a3_gene_values_survive_a_save_and_sanitize)
{
    static uint8_t img[DOOMO_SAVE_SLOT1_MAX];
    static uint8_t want[DOOMO_GENE_BYTES];
    DoomoResident *r;
    uint32_t n;
    town_new(TOWN_FREEPLAY, 4242u, TOWN_POP_PREMADE);
    r = town_res(77);
    CHECK(r != NULL);
    if (r->status == RES_EMPTY) r->status = RES_ALIVE;
    res_gset(r, GF_HORNS, 25);
    res_gset(r, GF_TAIL, 31);
    res_gset(r, GF_WINGS, 23);
    res_gset(r, GF_HALO, 15);
    res_gset(r, GF_AURA, 23);                                   /* the fifth bit rides in the NICK trailer */
    CHECK_EQ(resident_sanitize(r), 0);                          /* every one is a live choice: nothing is fixed */
    memcpy(want, r->gene, DOOMO_GENE_BYTES);
    n = save_serialize(img, sizeof(img));
    CHECK(n > 0);
    memset(r->gene + 30, 0, 18);
    CHECK(save_deserialize(img, n));
    r = town_res(77);
    CHECK(memcmp(r->gene, want, DOOMO_GENE_BYTES) == 0);
    CHECK_EQ((int)res_gget(r, GF_AURA), 23);
    CHECK_EQ((int)res_gget(r, GF_TAIL), 31);
    /* one past the live count is not a choice */
    res_gset(r, GF_HALO, 15);
    res_gset(r, GF_AURA, 24);
    CHECK(resident_sanitize(r) > 0);
    CHECK_EQ((int)res_gget(r, GF_AURA), 0);
}

TEST(cr4_a3_old_values_still_draw_and_differ_from_the_new_ones)
{
    static uint8_t a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    int s, v, o, bad = 0;
    for (s = 0; s < 5; s++)
        for (o = 1; o < ranges[s].first; o++)
            for (v = ranges[s].first; v <= ranges[s].last; v++) {
                DoomoLook l = bench(), z = bench();
                if (ranges[s].list == GF_HALO && o > 11) continue;
                *look_byte(&l, ranges[s].list) = (uint8_t)o;
                *look_byte(&z, ranges[s].list) = (uint8_t)v;
                portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, a);
                portrait_render(&z, RES_ALIVE, EXPR_NEUTRAL, 0, b);
                if (ranges[s].list == GF_TAIL || ranges[s].list == GF_WINGS) {   /* the bust shows only a bit of them */
                    static uint8_t fa[FIGURE_W * FIGURE_H], fb[FIGURE_W * FIGURE_H];
                    figure_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, fa);
                    figure_render(&z, RES_ALIVE, EXPR_NEUTRAL, 0, fb);
                    if (!diff(fa, fb, (int)sizeof(fa)) && !diff(a, b, (int)sizeof(a))) bad++;
                } else if (!diff(a, b, (int)sizeof(a))) {
                    bad++;
                }
            }
    CHECK_EQ(bad, 0);
}
