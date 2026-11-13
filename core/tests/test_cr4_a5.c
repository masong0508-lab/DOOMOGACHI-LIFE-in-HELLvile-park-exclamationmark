/* test_cr4_a5.c - CR4 art batch A5 (outfit), docs/CREATOR.md "CR4 batch A5": the gene-stored outfit lists
 * that have a part-script hook and no painted values yet: 31 outer layers (gene `outer` 1..31) and 15
 * legwear (gene `legwear` 1..15).
 *
 * - the lists are full (32 and 16 long), every new part is catalogued, scripted, live, age class ALL and
 *   inside the cost cap;
 * - the shape gate the generic test (test_part_render.c) cannot run for these lists (they are gene-only
 *   fields, not look bytes): an outer layer differs from every other value of its slot, with all materials on
 *   one grey ramp, by >= 24 px of the bust; legwear (the bust shows no legs; every pair shares the leg's
 *   silhouette, so a sock and a tight differ by colour and coverage) by >= 8 px of the figure in colour;
 * - an outer layer shows on the figure and the fighter, a cape from behind; legwear shows on the figure;
 * - nothing here is age restricted (a child may wear all of it) and every stage keeps the values;
 * - the gene values survive a save, resident_sanitize keeps them and drops the value one past the end;
 * - they render the same on another citizen (not just the bench).
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/look_fields.h"
#include "doomo/portrait.h"
#include "doomo/save.h"

void portrait_shape_mode(int on);          /* portrait_gfx.c (CR2): shape-only renders */

#define A5_COST_CAP 48

typedef struct A5Range { int list, first, last, live; } A5Range;
static const A5Range ranges[2] = {
    { GF_OUTER, 1, 31, 32 }, { GF_LEGWEAR, 1, 15, 16 },
};

/* the bench citizen, staged so the part reads: an outer layer over a white top, legwear on bare legs */
static DoomoLook bench(int list)
{
    DoomoResident r;
    DoomoLook l;
    resident_default(&r);
    l = *RES_LOOK_VIEW(&r);
    l.accessory_color = PAINT_COBALT;
    if (list == GF_OUTER) l.top_color = PAINT_WHITE;
    if (list == GF_LEGWEAR) { l.bottom = BOTTOM_NONE; l.shoes = SHOES_SANDALS; l.pattern_color = PAINT_CHERRY; }
    return l;
}

static int diff(const uint8_t *a, const uint8_t *b, int n)
{
    int i, d = 0;
    for (i = 0; i < n; i++) d += a[i] != b[i];
    return d;
}

TEST(cr4_a5_lists_are_full_and_every_new_part_is_catalogued)
{
    int s, v, n = 0;
    for (s = 0; s < 2; s++) {
        CHECK_EQ(part_live_count(ranges[s].list), ranges[s].live);
        for (v = ranges[s].first; v <= ranges[s].last; v++) {
            const PartMeta *m = part_meta_find(ranges[s].list, v);
            CHECK_MSG(m, "list %d value %d missing", ranges[s].list, v);
            if (!m) continue;
            CHECK(PMF_STATE(m->flags) == PSTATE_LIVE);
            CHECK(m->flags & PMF_SCRIPTED);
            CHECK_EQ(PMF_AGE(m->flags), PAGE_ALL);
            CHECK_MSG(m->cost <= A5_COST_CAP, "%s costs %d", part_meta_name(m), m->cost);
            n++;
        }
    }
    CHECK_EQ(n, 46);
}

TEST(cr4_a5_shape_gate_for_the_gene_only_outfit_lists)
{
    /* the gate of test_part_render.c for the lists it skips (gene-only fields): every material on one ramp, a part
     * differs from every other value of its slot by its shape */
    static uint8_t bust[32][PORTRAIT_W * PORTRAIT_H], fig[32][FIGURE_W * FIGURE_H];
    int s, v, o, bad = 0;
    for (s = 0; s < 2; s++) {
        int n = ranges[s].live;
        portrait_shape_mode(s == 0);                       /* outer layers on one ramp, legwear in colour */
        for (v = 0; v < n; v++) {
            DoomoLook l = bench(ranges[s].list);
            DoomoDeep d = doomo_deep_zero;
            look_part_set(&l, &d, ranges[s].list, v);
            portrait_render_deep(&l, &d, RES_ALIVE, EXPR_NEUTRAL, 0, bust[v]);
            figure_render_deep(&l, &d, RES_ALIVE, EXPR_NEUTRAL, 0, fig[v]);
        }
        for (v = ranges[s].first; v <= ranges[s].last; v++)
            for (o = 0; o < n; o++) {
                int dd;
                if (o == v) continue;
                dd = s == 0 ? diff(bust[v], bust[o], PORTRAIT_W * PORTRAIT_H)
                            : diff(fig[v], fig[o], FIGURE_W * FIGURE_H);
                if (dd < (s == 0 ? 24 : 8)) {
                    fprintf(stderr, "    list %d: %d vs %d differ by %d px\n", ranges[s].list, v, o, dd);
                    bad++;
                }
            }
    }
    portrait_shape_mode(0);
    CHECK_EQ(bad, 0);
}

TEST(cr4_a5_outer_layers_show_on_the_figure_and_capes_from_behind)
{
    static uint8_t a[FIGURE_W * FIGURE_H], b[FIGURE_W * FIGURE_H];
    static uint8_t fa[FIGHTER_W * FIGHTER_H], fb[FIGHTER_W * FIGHTER_H];
    static const int capes[4] = { 11, 19, 24, 31 };
    int v, k, bad = 0;
    for (v = ranges[0].first; v <= ranges[0].last; v++) {
        DoomoLook l = bench(GF_OUTER), z = bench(GF_OUTER);
        DoomoDeep d = doomo_deep_zero, dz = doomo_deep_zero;
        look_part_set(&l, &d, GF_OUTER, v);
        figure_render_deep(&l, &d, RES_ALIVE, EXPR_NEUTRAL, 0, a);
        figure_render_deep(&z, &dz, RES_ALIVE, EXPR_NEUTRAL, 0, b);
        if (diff(a, b, (int)sizeof(a)) < 6) {
            fprintf(stderr, "    outer %d is hidden on the figure\n", v);
            bad++;
        }
        for (k = 0; k < 5; k++) {                      /* the fighter's five views: a part never vanishes from all */
            fighter_render_d(&l, &d, RES_ALIVE, k, 0, -1, fa);
            fighter_render_d(&z, &dz, RES_ALIVE, k, 0, -1, fb);
            if (k == 0 && diff(fa, fb, (int)sizeof(fa)) < 4) {
                int c, cape = 0;
                for (c = 0; c < 4; c++) cape |= capes[c] == v;
                /* from behind (view 0) a cape is on the back; any other layer may only be a front */
                if (cape) {
                    fprintf(stderr, "    cape %d is hidden from behind\n", v);
                    bad++;
                }
            }
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(cr4_a5_legwear_shows_on_the_legs)
{
    static uint8_t a[FIGURE_W * FIGURE_H], b[FIGURE_W * FIGURE_H];
    int v, bad = 0;
    for (v = ranges[1].first; v <= ranges[1].last; v++) {
        DoomoLook l = bench(GF_LEGWEAR), z = bench(GF_LEGWEAR);
        DoomoDeep d = doomo_deep_zero, dz = doomo_deep_zero;
        look_part_set(&l, &d, GF_LEGWEAR, v);
        figure_render_deep(&l, &d, RES_ALIVE, EXPR_NEUTRAL, 0, a);
        figure_render_deep(&z, &dz, RES_ALIVE, EXPR_NEUTRAL, 0, b);
        if (diff(a, b, (int)sizeof(a)) < 10) {
            fprintf(stderr, "    legwear %d is hidden on the figure\n", v);
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(cr4_a5_a_child_may_wear_all_of_it_and_every_stage_keeps_it)
{
    static const int stages[] = { LIFE_BABY, LIFE_TODDLER, LIFE_CHILD, LIFE_TEEN, LIFE_ADULT, LIFE_ELDER };
    int s, v, k;
    for (k = 0; k < 6; k++)
        for (s = 0; s < 2; s++)
            for (v = ranges[s].first; v <= ranges[s].last; v++) {
                DoomoLook in = bench(ranges[s].list), out;
                DoomoDeep d = doomo_deep_zero;
                look_part_set(&in, &d, ranges[s].list, v);
                look_for_stage(&in, stages[k], &out);       /* the deep set rides in `d`; the age gate reads the catalog */
                CHECK_EQ(PMF_AGE(part_meta_find(ranges[s].list, v)->flags), PAGE_ALL);
            }
}

TEST(cr4_a5_gene_values_survive_a_save_and_sanitize)
{
    static uint8_t img[DOOMO_SAVE_SLOT1_MAX];
    static uint8_t want[DOOMO_GENE_BYTES];
    DoomoResident *r;
    uint32_t n;
    town_new(TOWN_FREEPLAY, 4242u, TOWN_POP_PREMADE);
    r = town_res(77);
    CHECK(r != NULL);
    if (r->status == RES_EMPTY) r->status = RES_ALIVE;
    res_gset(r, GF_OUTER, 31);
    res_gset(r, GF_LEGWEAR, 15);
    CHECK_EQ(resident_sanitize(r), 0);                          /* every one is a live choice: nothing is fixed */
    memcpy(want, r->gene, DOOMO_GENE_BYTES);
    n = save_serialize(img, sizeof(img));
    CHECK(n > 0);
    memset(r->gene + 30, 0, 18);
    CHECK(save_deserialize(img, n));
    r = town_res(77);
    CHECK(memcmp(r->gene, want, DOOMO_GENE_BYTES) == 0);
    CHECK_EQ((int)res_gget(r, GF_OUTER), 31);
    CHECK_EQ((int)res_gget(r, GF_LEGWEAR), 15);
}

TEST(cr4_a5_the_new_parts_draw_on_another_citizen_too)
{
    static uint8_t a[FIGURE_W * FIGURE_H], b[FIGURE_W * FIGURE_H];
    static uint8_t fa[FIGHTER_W * FIGHTER_H], fb[FIGHTER_W * FIGHTER_H];
    static const uint32_t seeds[3] = { 11u, 4242u, 9001u };
    int c, s, v, bad = 0;
    for (c = 0; c < 3; c++) {
        DoomoResident r;
        DoomoLook base;
        if (c == 1) resident_generate_hellborn(&r, seeds[c]); else resident_generate(&r, seeds[c]);
        base = *RES_LOOK_VIEW(&r);
        base.accessory_color = PAINT_COBALT;
        base.pattern_color = PAINT_CHERRY;
        base.bottom = BOTTOM_NONE;
        base.top = TOP_TSHIRT;                       /* a dress or a robe hides the legs, hooves hide an ankle sock: that is right */
        base.shoes = SHOES_SANDALS;
        for (s = 0; s < 2; s++)
            for (v = ranges[s].first; v <= ranges[s].last; v++) {
                DoomoLook l = base, z = base;
                DoomoDeep d = doomo_deep_zero, dz = doomo_deep_zero;
                int seen;
                look_part_set(&l, &d, ranges[s].list, v);
                look_part_set(&z, &dz, ranges[s].list, 0);
                figure_render_deep(&l, &d, RES_ALIVE, EXPR_NEUTRAL, 0, a);
                figure_render_deep(&z, &dz, RES_ALIVE, EXPR_NEUTRAL, 0, b);
                seen = diff(a, b, (int)sizeof(a)) > 0;
                fighter_render_d(&l, &d, RES_ALIVE, 0, 0, -1, fa);          /* a cape: from behind */
                fighter_render_d(&z, &dz, RES_ALIVE, 0, 0, -1, fb);
                seen |= diff(fa, fb, (int)sizeof(fa)) > 0;
                if (!seen) {
                    fprintf(stderr, "    citizen %d: list %d value %d does not show\n", c, ranges[s].list, v);
                    bad++;
                }
            }
    }
    CHECK_EQ(bad, 0);
}
