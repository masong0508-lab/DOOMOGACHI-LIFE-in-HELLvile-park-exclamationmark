/* test_cr4_a1b.c - CR4 art batch A1b (noses, mouths, ears, blush styles), docs/CREATOR.md "CR4 A1b".
 *
 * - the lists are full: noses 24, mouths 32, ears 32, blush styles 16; every new part is live, scripted,
 *   age class ALL, with a cost under the slot cap;
 * - `mat blush` (PS_MAT_BLUSH, the cheek pink of the painters) draws: a blush style paints pixels in the
 *   blush pink that the same face without it does not have;
 * - a scripted mouth is the painter's smile in an expression (the face still emotes);
 * - a scripted ear is drawn on the chibi and the fighter too. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/portrait.h"
#include "doomo/look_fields.h"

#define A1B_COST_CAP 48

static DoomoLook bench(void)
{
    DoomoResident r;
    resident_default(&r);
    return *RES_LOOK_VIEW(&r);
}

static int check_range(int list, int v0, int v1)
{
    int v, bad = 0;
    for (v = v0; v <= v1; v++) {
        const PartMeta *m = part_meta_find(list, v);
        if (!m || PMF_STATE(m->flags) != PSTATE_LIVE || !(m->flags & PMF_SCRIPTED) ||
            PMF_AGE(m->flags) != PAGE_ALL || m->cost > A1B_COST_CAP) {
            fprintf(stderr, "    list %d value %d is not a live scripted all-ages part under the cost cap\n", list, v);
            bad++;
        }
    }
    return bad;
}

TEST(cr4_a1b_lists_are_full_and_every_new_part_is_catalogued)
{
    CHECK_EQ(part_live_count(GF_NOSE_STYLE), 24);
    CHECK_EQ(part_live_count(GF_MOUTH_STYLE), 32);
    CHECK_EQ(part_live_count(GF_EARS), 32);
    CHECK_EQ(part_live_count(GF_BLUSH_STYLE), 16);
    CHECK_EQ(check_range(GF_NOSE_STYLE, 12, 23), 0);
    CHECK_EQ(check_range(GF_MOUTH_STYLE, 20, 31), 0);
    CHECK_EQ(check_range(GF_EARS, 16, 31), 0);
    CHECK_EQ(check_range(GF_BLUSH_STYLE, 5, 15), 0);
}

static int pink_px(const DoomoLook *l, const DoomoDeep *d)
{
    static uint8_t p[PORTRAIT_W * PORTRAIT_H];
    int i, n = 0;
    portrait_render_deep(l, d, RES_ALIVE, EXPR_NEUTRAL, 0, p);
    for (i = 0; i < PORTRAIT_W * PORTRAIT_H; i++) n += p[i] == PAL_RAMP(RAMP_PASTEL_PINK, 5) || p[i] == PAL_RAMP(RAMP_PASTEL_PINK, 4);
    return n;
}

TEST(cr4_a1b_blush_scripts_paint_the_blush_pink)
{
    DoomoLook l = bench();
    DoomoDeep d0 = doomo_deep_zero, d1;
    int v, base, bad = 0;
    l.species = SPECIES_HUMAN;
    l.cheeks = 0;
    l.hair_style = HAIR_BALD;
    base = pink_px(&l, &d0);
    for (v = 5; v <= 15; v++) {
        d1 = doomo_deep_zero;
        look_part_set(&l, &d1, GF_BLUSH_STYLE, v);
        if (pink_px(&l, &d1) <= base) {
            fprintf(stderr, "    blush %d paints no blush pink\n", v);
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(cr4_a1b_a_scripted_mouth_gives_way_to_an_expression)
{
    static uint8_t a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    DoomoLook l = bench(), z = bench();
    int v, bad = 0;
    z.mouth_style = MOUTH_SMILE;
    for (v = 20; v <= 31; v++) {
        l.mouth_style = (uint8_t)v;
        portrait_render(&l, RES_ALIVE, EXPR_HAPPY, 0, a);
        portrait_render(&z, RES_ALIVE, EXPR_HAPPY, 0, b);
        if (memcmp(a, b, sizeof(a))) {
            fprintf(stderr, "    mouth %d changes a happy face\n", v);
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(cr4_a1b_ears_show_on_the_chibi_and_the_fighter)
{
    static uint8_t a[FIGHTER_W * FIGHTER_H], b[FIGHTER_W * FIGHTER_H];
    static uint8_t c[BODYSPR_W * BODYSPR_H], e[BODYSPR_W * BODYSPR_H];
    int v, bad = 0;
    for (v = 16; v <= 31; v++) {
        DoomoLook l = bench(), z = bench();
        l.ears = (uint8_t)v;
        z.ears = EARS_NONE;
        fighter_render(&l, RES_ALIVE, 4, 0, -1, a);
        fighter_render(&z, RES_ALIVE, 4, 0, -1, b);
        bodyspr_render(&l, RES_ALIVE, 0, 0, c);
        bodyspr_render(&z, RES_ALIVE, 0, 0, e);
        if (!memcmp(c, e, sizeof(c))) {
            fprintf(stderr, "    ears %d are invisible on the chibi\n", v);
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
}
