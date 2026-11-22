/* test_cr_sliderx.c - CR-SLIDERX: the cheat "Slider range x1 / x2 / x3 / x4" (docs/CREATOR.md "CR-SLIDERX").
 *
 * - the option: OPT_SLIDER_RANGE is the next free OPT2 id, x1 by default, on the cheats page, saved in the
 *   OPT2 chunk, and an older chunk (one without it) or no chunk reads x1;
 * - what it does: each of the 11 build sliders of the look to draw goes def + (value - def) * m, clamped to
 *   the table's safe range (look_sliders[], which must agree with the creator's slider table); x1 is bit for
 *   bit what was drawn before; the saved look (its gene) never changes, so a look made at x3 is the same look
 *   at x1; a minor is never scaled; the creator's preview and the value on the bar follow;
 * - EVERY slider at EVERY multiplier, on every step: the bust, the figure, the fighter and the chibi change
 *   whenever the slider's effective value changes, never when it does not, nothing writes outside its picture
 *   (canary bytes; the SAN=1 run adds the sanitizer's eyes), on every species and several body types;
 * - no look at any range is a sexual or nude shape: the draw is the same parts, only their sizes move inside a
 *   clamp (the extremes read as comic: the picture grows, it never gains a part).
 * The cross-platform twins are core/tests/scripts/creator_sliders_x2 / _x3 / _x4.txt (runner, GBA, PS1,
 * Dreamcast) read by tools/slider_shots_check.py. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/portrait.h"
#include "doomo/options_store.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "../src/creator_int.h"

#define CANARY 16
typedef struct XPic {
    uint8_t g0[CANARY], bust[PORTRAIT_W * PORTRAIT_H], g1[CANARY], fig[FIGURE_W * FIGURE_H], g2[CANARY],
            fgt[FIGHTER_W * FIGHTER_H], g3[CANARY], chi[BODYSPR_W * BODYSPR_H], g4[CANARY];
} XPic;
static int g_overflow;
static XPic PA, PB;

static int guard_ok(const uint8_t *g)
{
    int i;
    for (i = 0; i < CANARY; i++) if (g[i] != 0xA5) return 0;
    return 1;
}

/* the look the game draws a grown-up with, in all four sizes */
static void render_look(const DoomoLook *drawn, XPic *p)
{
    memset(p, 0, sizeof(*p));
    memset(p->g0, 0xA5, CANARY); memset(p->g1, 0xA5, CANARY); memset(p->g2, 0xA5, CANARY);
    memset(p->g3, 0xA5, CANARY); memset(p->g4, 0xA5, CANARY);
    portrait_render(drawn, RES_ALIVE, EXPR_NEUTRAL, 0, p->bust);
    figure_render(drawn, RES_ALIVE, EXPR_NEUTRAL, 0, p->fig);
    fighter_render(drawn, RES_ALIVE, 4, 0, -1, p->fgt);
    bodyspr_render(drawn, RES_ALIVE, 0, 0, p->chi);
    if (!guard_ok(p->g0) || !guard_ok(p->g1) || !guard_ok(p->g2) || !guard_ok(p->g3) || !guard_ok(p->g4)) g_overflow++;
}

static int diffpx(const uint8_t *a, const uint8_t *b, int n)
{
    int i, d = 0;
    for (i = 0; i < n; i++) d += a[i] != b[i];
    return d;
}

static int pic_diff(const XPic *a, const XPic *b)
{
    return diffpx(a->bust, b->bust, (int)sizeof(a->bust)) + diffpx(a->fig, b->fig, (int)sizeof(a->fig)) +
           diffpx(a->fgt, b->fgt, (int)sizeof(a->fgt)) + diffpx(a->chi, b->chi, (int)sizeof(a->chi));
}

static void bench(DoomoResident *r, int species, int body)
{
    DoomoLook lk;
    resident_default(r);
    res_get_look(r, &lk);
    lk.species = (uint8_t)species;
    lk.body = (uint8_t)body;
    lk.fangs = 2;
    lk.horns = HORNS_DEVIL;                      /* so the horn size draws */
    res_set_look(r, &lk);
}

static int look_byte(const DoomoLook *l, int i) { return (int8_t)((const uint8_t *)l)[look_sliders[i].off]; }

/* what a slider's value is worth at multiplier m, worked out here from the creator's own table */
static int expect_eff(int i, int v, int m)
{
    int def = cr_sliders[i].def, e = def + (v - def) * m;
    if (m == 1) return v;
    return e < look_sliders[i].lo ? look_sliders[i].lo : e > look_sliders[i].hi ? look_sliders[i].hi : e;
}

static const CrRow *slider_row(int i)        /* the row of look slider i (the table's first 11 are the build sliders) */
{
    int t, k;
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (k = 0; k < cr_tabs[t].nrows; k++)
            if (cr_tabs[t].rows[k].kind == RK_SLIDER && cr_tabs[t].rows[k].field == cr_sliders[i].field)
                return &cr_tabs[t].rows[k];
    return NULL;
}

/* the range the drawing code is working at: eye size 9 is one step under its default 10, so it lands m under it */
static int mult_in_force(void)
{
    DoomoLook l;
    memset(&l, 0, sizeof l);
    l.eye_size = 9;
    look_slider_range(&l);
    return 10 - l.eye_size;
}

/* the look as the world draws a grown-up of this record at the current range */
static void drawn_look(const DoomoResident *r, DoomoLook *out)
{
    look_for_stage(RES_LOOK_VIEW(r), LIFE_ADULT, out);
}

TEST(crsx_table_agrees_with_the_creators_slider_table)
{
    int i;
    CHECK_EQ(LOOK_SLIDERS, 11);
    for (i = 0; i < LOOK_SLIDERS; i++) {
        const LookSlider *s = &look_sliders[i];
        const CrRow *row = slider_row(i);
        CHECK_MSG(row != NULL, "slider %d has no creator row", i);
        CHECK_EQ(cr_sliders[i].count, 16);                           /* a build slider: 0..15, 4 bits */
        CHECK_EQ(s->def, cr_sliders[i].def);
        CHECK_MSG(s->lo <= 0 && s->hi >= 15, "slider %d: the safe range does not hold 0..15", i);
        CHECK_MSG(s->def >= 0 && s->def <= 15, "slider %d default", i);
        CHECK_MSG(s->lo > -64 && s->hi < 64, "slider %d range is not small", i);
    }
    /* the byte offsets are the fields they are named for */
    {
        DoomoLook l;
        memset(&l, 0, sizeof l);
        l.height = 1; l.build = 2; l.head_size = 3; l.eye_size = 4; l.eye_spacing = 5; l.eye_height = 6;
        l.eye_tilt = 7; l.brow_height = 8; l.nose_size = 9; l.mouth_size = 10; l.horn_size = 11;
        for (i = 0; i < LOOK_SLIDERS; i++) CHECK_EQ(look_byte(&l, i), i + 1);
    }
    CHECK(cr_sliders[0].field == GF_HEIGHT && cr_sliders[10].field == GF_HORN_SIZE);
}

TEST(crsx_option_is_the_next_opt2_id_and_a_cheat)
{
    const DoomoOptionInfo *oi = &doomo_option2_info[OPT_SLIDER_RANGE - OPT2_FIRST];
    CHECK_EQ(OPT_SLIDER_RANGE, OPT_STICK_MODE + 1);
    CHECK(OPT_SLIDER_RANGE + 1 <= OPT2_END);              /* LV11's two ids follow it (not on the PS1) */
    CHECK_EQ(oi->count, 4);
    CHECK_EQ(oi->def, 0);
    CHECK_EQ(oi->page, OPTPAGE_CHEATS);
    CHECK_STR(oi->name, "Slider range");
    CHECK_STR(oi->values[0], "x1");
    CHECK_STR(oi->values[3], "x4");
    test_reset();
    CHECK_EQ(opt_get(OPT_SLIDER_RANGE), 0);
    CHECK_EQ(mult_in_force(), 1);
    opt_set(OPT_SLIDER_RANGE, 2);
    CHECK_EQ(opt_get(OPT_SLIDER_RANGE), 2);
    CHECK_EQ(mult_in_force(), 3);
    opt_set(OPT_SLIDER_RANGE, 99);                       /* clamped to x4 */
    CHECK_EQ(opt_get(OPT_SLIDER_RANGE), 3);
    CHECK_EQ(mult_in_force(), 4);
    opt_defaults();                                      /* Defaults puts it back to x1 */
    CHECK_EQ(mult_in_force(), 1);
    CHECK_EQ(opt_get(OPT_STICK_MODE), STICK_TURN);       /* its neighbour in the same byte is untouched */
    opt_set(OPT_STICK_MODE, STICK_STRAFE);
    opt_set(OPT_SLIDER_RANGE, 3);
    CHECK_EQ(opt_get(OPT_STICK_MODE), STICK_STRAFE);
    opt_set(OPT_STICK_MODE, STICK_TURN);
    CHECK_EQ(opt_get(OPT_SLIDER_RANGE), 3);
    opt_defaults();
}

TEST(crsx_saved_in_opt2_and_an_old_save_reads_x1)
{
    uint8_t buf[64], old[64];
    uint32_t n, k;
    const DoomoSaveExt *ext = doomo_save_ext_opt2;
    test_reset();
    opt_set(OPT_SLIDER_RANGE, 2);
    n = ext->write(buf, sizeof(buf));
    /* The trailing elder recall option is only written when enabled. */
    CHECK_EQ(n, 4u + (uint32_t)(OPT_MEMORY_LAPSES - OPT2_FIRST));
    CHECK(n <= SAVE_EXT_MAX_OPT2);
    CHECK_EQ(buf[4 + OPT_SLIDER_RANGE - OPT2_FIRST], 2); /* its own byte, in id order: the chunk only grew at its end */
    opt_set(OPT_SLIDER_RANGE, 0);
    CHECK(ext->read(buf, n, 2));
    CHECK_EQ(opt_get(OPT_SLIDER_RANGE), 2);
    CHECK_EQ(mult_in_force(), 3);                /* and the draw follows the load */
    /* a chunk written before this option (one byte shorter): x1 */
    memcpy(old, buf, n);
    k = 4u + (uint32_t)(OPT_SLIDER_RANGE - OPT2_FIRST);     /* the chunk ends where the slider's byte was */
    CHECK(ext->read(old, k, 2));
    CHECK_EQ(opt_get(OPT_SLIDER_RANGE), 0);
    CHECK_EQ(mult_in_force(), 1);
    /* a byte past the choices reads the default, never a wild range */
    opt_set(OPT_SLIDER_RANGE, 3);
    buf[4 + OPT_SLIDER_RANGE - OPT2_FIRST] = 9;
    CHECK(ext->read(buf, n, 2));
    CHECK_EQ(opt_get(OPT_SLIDER_RANGE), 0);
    CHECK_EQ(mult_in_force(), 1);
    /* a save with no chunk at all */
    opt_set(OPT_SLIDER_RANGE, 3);
    ext->reset();
    CHECK_EQ(opt_get(OPT_SLIDER_RANGE), 0);
    CHECK_EQ(mult_in_force(), 1);
    opt_defaults();
}

TEST(crsx_x1_is_bit_for_bit_what_was_drawn_before)
{
    int sp, bo, i;
    test_reset();
    opt_set(OPT_SLIDER_RANGE, 0);
    for (sp = 0; sp < SPECIES_COUNT; sp++)
        for (bo = 0; bo < BODY_TYPES; bo += 5) {
            DoomoResident r;
            DoomoLook a, b;
            bench(&r, sp, bo);
            drawn_look(&r, &a);
            res_get_look(&r, &b);
            CHECK_MSG(memcmp(&a, &b, sizeof a) == 0, "x1 moved the look of species %d body %d", sp, bo);
            look_slider_range(&b);                       /* the call itself is a no-op at x1 */
            CHECK(memcmp(&a, &b, sizeof a) == 0);
        }
    /* and an out-of-range raw byte is still pulled back to 15 by the renderer's own sanitize (as it always was) */
    {
        DoomoResident r;
        DoomoLook raw, ref;
        XPic x, y;
        bench(&r, SPECIES_CATFOLK, 0);
        res_get_look(&r, &raw);
        ref = raw;
        for (i = 0; i < LOOK_SLIDERS; i++) ((uint8_t *)&raw)[look_sliders[i].off] = 200;
        for (i = 0; i < LOOK_SLIDERS; i++) ((uint8_t *)&ref)[look_sliders[i].off] = 15;
        render_look(&raw, &x);
        render_look(&ref, &y);
        CHECK_EQ(pic_diff(&x, &y), 0);
    }
    CHECK_EQ(g_overflow, 0);
}

TEST(crsx_each_slider_at_each_multiplier_is_the_formula_inside_its_range)
{
    int m, i, v;
    test_reset();
    for (m = 0; m < 4; m++) {
        opt_set(OPT_SLIDER_RANGE, m);
        for (i = 0; i < LOOK_SLIDERS; i++) {
            const CrRow *row = slider_row(i);
            for (v = 0; v < 16; v++) {
                DoomoResident r;
                DoomoLook d, saved;
                bench(&r, SPECIES_CATFOLK, 0);
                cr_row_set(&r, row, v);
                drawn_look(&r, &d);
                res_get_look(&r, &saved);
                CHECK_MSG(look_byte(&saved, i) == v, "the saved look changed (slider %d step %d x%d)", i, v, m + 1);
                CHECK_MSG(look_byte(&d, i) == expect_eff(i, v, m + 1), "slider %d step %d at x%d: drew %d, wanted %d", i, v,
                          m + 1, look_byte(&d, i), expect_eff(i, v, m + 1));
            }
        }
    }
    /* the default step is the default at every multiplier, and the far ends go further the higher the range */
    for (i = 0; i < LOOK_SLIDERS; i++) {
        CHECK_EQ(expect_eff(i, cr_sliders[i].def, 4), cr_sliders[i].def);
        CHECK(expect_eff(i, 15, 3) >= expect_eff(i, 15, 2) && expect_eff(i, 0, 3) <= expect_eff(i, 0, 2));
        CHECK(expect_eff(i, 15, 2) > 15 || cr_sliders[i].def == 15 || expect_eff(i, 15, 2) == look_sliders[i].hi);
    }
    opt_defaults();
}

TEST(crsx_a_look_made_at_x3_is_the_same_look_at_x1)
{
    DoomoResident r;
    DoomoLook at3, at1, saved3, saved1;
    int i;
    test_reset();
    bench(&r, SPECIES_HUMAN, 0);
    opt_set(OPT_SLIDER_RANGE, 2);
    for (i = 0; i < LOOK_SLIDERS; i++) cr_row_set(&r, slider_row(i), (i * 5) & 15);   /* a look made at x3 */
    CHECK_EQ(resident_sanitize(&r), 0);                  /* nothing in it is out of range */
    res_get_look(&r, &saved3);
    drawn_look(&r, &at3);
    opt_set(OPT_SLIDER_RANGE, 0);                        /* x1 is back */
    CHECK_EQ(resident_sanitize(&r), 0);
    res_get_look(&r, &saved1);
    drawn_look(&r, &at1);
    CHECK(memcmp(&saved3, &saved1, sizeof saved1) == 0); /* the saved look is untouched */
    CHECK(memcmp(&at1, &saved1, sizeof at1) == 0);       /* and at x1 it draws as itself */
    CHECK(memcmp(&at3, &at1, sizeof at1) != 0);          /* (at x3 it drew further) */
    for (i = 0; i < LOOK_SLIDERS; i++)
        CHECK_MSG(look_byte(&at1, i) >= 0 && look_byte(&at1, i) <= 15, "slider %d left 0..15 at x1", i);
    opt_defaults();
}

TEST(crsx_a_minor_is_never_scaled)
{
    DoomoResident r;
    int st, i;
    test_reset();
    bench(&r, SPECIES_HUMAN, 0);
    for (i = 0; i < LOOK_SLIDERS; i++) cr_row_set(&r, slider_row(i), i & 1 ? 15 : 0);   /* off the defaults, so scaling shows */
    for (st = LIFE_BABY; st <= LIFE_TEEN; st++) {
        DoomoLook a, b;
        opt_set(OPT_SLIDER_RANGE, 0);
        look_for_stage(RES_LOOK_VIEW(&r), st, &a);
        for (i = 1; i < 4; i++) {
            opt_set(OPT_SLIDER_RANGE, i);
            look_for_stage(RES_LOOK_VIEW(&r), st, &b);
            CHECK_MSG(memcmp(&a, &b, sizeof a) == 0, "stage %d changed at x%d", st, i + 1);
        }
    }
    {   /* adult and elder do scale */
        DoomoLook a, b;
        opt_set(OPT_SLIDER_RANGE, 3);
        look_for_stage(RES_LOOK_VIEW(&r), LIFE_ADULT, &a);
        look_for_stage(RES_LOOK_VIEW(&r), LIFE_ELDER, &b);
        CHECK(look_byte(&a, 0) < 0 && look_byte(&a, 1) > 15);        /* x4: short and chonky past the saved 0 and 15 */
        CHECK(memcmp(&a, &b, sizeof a) != 0);
    }
    opt_defaults();
}

TEST(crsx_every_slider_every_multiplier_every_step_moves_the_picture_and_stays_inside)
{
    int m, i, v, bad = 0;
    test_reset();
    fprintf(stderr, "    %-13s %8s %8s %8s %s\n", "slider", "x2 px", "x3 px", "x4 px", "(lowest to highest step, bust+figure+fighter+chibi)");
    for (i = 0; i < LOOK_SLIDERS; i++) {
        const CrRow *row = slider_row(i);
        int px[4] = { 0, 0, 0, 0 };
        for (m = 0; m < 4; m++) {
            int prev_eff = -999;
            opt_set(OPT_SLIDER_RANGE, m);
            for (v = 0; v < 16; v++) {
                DoomoResident r;
                DoomoLook d;
                int eff;
                bench(&r, SPECIES_CATFOLK, 0);
                cr_row_set(&r, row, v);
                drawn_look(&r, &d);
                eff = look_byte(&d, i);
                render_look(&d, &PB);
                if (v > 0) {
                    int dpx = pic_diff(&PA, &PB);
                    if (eff != prev_eff && dpx == 0) {
                        fprintf(stderr, "    slider %s x%d: step %d (%d -> %d) draws the same picture\n", row->label, m + 1, v, prev_eff, eff);
                        bad++;
                    }
                    if (eff == prev_eff && dpx != 0) {
                        fprintf(stderr, "    slider %s x%d: step %d repeats the value %d but the picture moved\n", row->label, m + 1, v, eff);
                        bad++;
                    }
                }
                if (v == 0) px[m] = -1;
                PA = PB;
                prev_eff = eff;
                if (v == 15) {                                      /* lowest against highest */
                    DoomoResident r0;
                    DoomoLook d0;
                    bench(&r0, SPECIES_CATFOLK, 0);
                    cr_row_set(&r0, row, 0);
                    drawn_look(&r0, &d0);
                    render_look(&d0, &PB);
                    px[m] = pic_diff(&PA, &PB);
                    if (px[m] == 0) { fprintf(stderr, "    slider %s x%d: lowest and highest draw the same\n", row->label, m + 1); bad++; }
                }
            }
        }
        fprintf(stderr, "    %-13s %8d %8d %8d\n", row->label, px[1], px[2], px[3]);
    }
    CHECK_EQ(bad, 0);
    CHECK_EQ(g_overflow, 0);
    opt_defaults();
}

TEST(crsx_extremes_stay_inside_on_every_species_body_and_both_ends_together)
{
    int m, sp, bo, i, ends;
    test_reset();
    for (m = 1; m < 4; m++) {
        opt_set(OPT_SLIDER_RANGE, m);
        for (sp = 0; sp < SPECIES_COUNT; sp++)
            for (bo = 0; bo < BODY_TYPES; bo += 3)
                for (ends = 0; ends < 3; ends++) {          /* every slider at its lowest, at its highest, alternating */
                    DoomoResident r;
                    DoomoLook d;
                    bench(&r, sp, bo);
                    for (i = 0; i < LOOK_SLIDERS; i++) cr_row_set(&r, slider_row(i), ends == 0 ? 0 : ends == 1 ? 15 : (i & 1) * 15);
                    drawn_look(&r, &d);
                    for (i = 0; i < LOOK_SLIDERS; i++)
                        CHECK_MSG(look_byte(&d, i) >= look_sliders[i].lo && look_byte(&d, i) <= look_sliders[i].hi, "slider %d out of its range", i);
                    render_look(&d, &PA);
                }
    }
    CHECK_EQ(g_overflow, 0);
    opt_defaults();
}

TEST(crsx_renderers_survive_every_value_the_clamp_allows_even_at_once_and_a_raw_wild_one)
{
    /* the bound of every slider on its own, on all species: the renderers must stay inside their pictures */
    int sp, i, e;
    test_reset();
    opt_set(OPT_SLIDER_RANGE, 3);
    for (sp = 0; sp < SPECIES_COUNT; sp++)
        for (i = 0; i < LOOK_SLIDERS; i++)
            for (e = 0; e < 2; e++) {
                DoomoResident r;
                DoomoLook d;
                bench(&r, sp, BODY_TYPES - 1);
                res_get_look(&r, &d);
                ((uint8_t *)&d)[look_sliders[i].off] = (uint8_t)(int8_t)(e ? look_sliders[i].hi : look_sliders[i].lo);
                render_look(&d, &PA);
            }
    /* wild raw bytes at x4 are clamped into the range, not trusted */
    {
        DoomoResident r;
        DoomoLook d;
        bench(&r, SPECIES_CATFOLK, 0);
        res_get_look(&r, &d);
        for (i = 0; i < LOOK_SLIDERS; i++) ((uint8_t *)&d)[look_sliders[i].off] = (uint8_t)(i & 1 ? 120 : 130);
        render_look(&d, &PA);
    }
    CHECK_EQ(g_overflow, 0);
    opt_defaults();
}

TEST(crsx_cache_follows_the_range_and_forgets_nothing_it_should_keep)
{
    /* a citizen drawn at x1, then at x3, then at x1: three pictures that are the right ones (the cache keys carry
     * the look as drawn, and the memoised hash of the body is dropped when the range changes) */
    DoomoResident *r;
    DoomoLook lk;
    const DoomoImage *a, *b, *c;
    uint32_t uid1, uid3;
    uint8_t first[BODYSPR_W * BODYSPR_H], bust1[PORTRAIT_W * PORTRAIT_H];
    int id;
    test_reset();
    portrait_cache_reset();
    id = town_alloc(DISTRICT_HILLVILE);
    CHECK(id >= 0);
    r = town_res(id);
    resident_generate(r, 99);
    r->status = RES_ALIVE;
    res_get_look(r, &lk);
    lk.height = 0; lk.build = 15; lk.head_size = 15; lk.nose_size = 15;
    res_set_look(r, &lk);
    RES_SET_STAGE(r, LIFE_ADULT);
    a = bodyspr_image(id, 0, 0);
    CHECK(a != NULL);
    memcpy(first, a->pix, sizeof first);
    uid1 = a->uid;
    b = portrait_image(id, EXPR_NEUTRAL);
    memcpy(bust1, b->pix, sizeof bust1);
    opt_set(OPT_SLIDER_RANGE, 2);
    a = bodyspr_image(id, 0, 0);
    CHECK(a != NULL);
    CHECK(a->uid != uid1);                                 /* x3 is another body, not x1's cached one */
    CHECK(memcmp(first, a->pix, sizeof first) != 0);
    uid3 = a->uid;
    b = portrait_image(id, EXPR_NEUTRAL);
    CHECK(memcmp(bust1, b->pix, sizeof bust1) != 0);
    opt_set(OPT_SLIDER_RANGE, 0);
    c = bodyspr_image(id, 0, 0);
    CHECK(c != NULL);
    CHECK(c->uid != uid3);
    CHECK(memcmp(first, c->pix, sizeof first) == 0);       /* and x1 is back to the first picture, not x3's */
    b = portrait_image(id, EXPR_NEUTRAL);
    CHECK(memcmp(bust1, b->pix, sizeof bust1) == 0);
    opt_defaults();
}

/* ---------------------------------------------------------------------- */
/* the creator                                                              */
static void frames(int n, uint32_t b)
{
    while (n-- > 0) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
}
static const DoomoScene base_scene = { "TESTBASE", NULL, NULL, NULL, NULL, 0 };

TEST(crsx_the_creator_previews_the_range_and_saves_the_look_as_it_is)
{
    int i;
    DoomoLook shown, saved;
    test_reset();
    opt_set(OPT_SLIDER_RANGE, 3);
    scene_set(&base_scene, 0);
    scene_push(&SCENE_CREATOR, -1);
    frames(6, 0);
    CHECK(scene_top() == &SCENE_CREATOR);
    shown = cr.shown;
    res_get_look(&cr.draft, &saved);
    if (LIFE_IS_ADULT(RES_STAGE(&cr.draft))) {
        for (i = 0; i < LOOK_SLIDERS; i++) {
            int want = expect_eff(i, look_byte(&saved, i), 4);
            CHECK_MSG(look_byte(&shown, i) == want, "the preview of slider %d shows %d, wanted %d (saved %d)", i,
                      look_byte(&shown, i), want, look_byte(&saved, i));
            CHECK_MSG(look_byte(&saved, i) >= 0 && look_byte(&saved, i) <= 15, "the draft holds %d", look_byte(&saved, i));
        }
    }
    scene_pop();
    opt_defaults();
}

/* The safe ranges were found by scanning with the default styles; this one varies every style too (species,
 * body, head, eye, brow, nose, mouth, hair, ears, horns, facial hair) with each slider at an end or random,
 * so a style that breaks a range shows up (reviewer finding, CR-SLIDERX). */
static uint32_t crsx_rs = 12345;
static uint32_t crsx_rnd(void)
{
    crsx_rs = crsx_rs * 1664525u + 1013904223u;
    return crsx_rs >> 8;
}
static int crsx_guard_ok(const uint8_t *g)
{
    int i;
    for (i = 0; i < CANARY; i++)
        if (g[i] != 0xA5)
            return 0;
    return 1;
}
static XPic g_fz;
TEST(crsx_extremes_stay_inside_with_every_style_randomised)
{
    int it, m, i, bad = 0;
    test_reset();
    for (m = 1; m < 4; m++) {
        opt_set(OPT_SLIDER_RANGE, m);
        for (it = 0; it < 300; it++) {
            DoomoResident r;
            DoomoLook lk, d;
            resident_default(&r);
            res_get_look(&r, &lk);
            lk.species = crsx_rnd() % SPECIES_COUNT;
            lk.body = crsx_rnd() % BODY_TYPES;
            lk.head_shape = crsx_rnd() % HEAD_COUNT;
            lk.eye_style = crsx_rnd() % EYE_COUNT;
            lk.eye_count = 1 + crsx_rnd() % 3;
            lk.brow_style = crsx_rnd() % BROW_COUNT;
            lk.nose_style = crsx_rnd() % NOSE_COUNT;
            lk.mouth_style = crsx_rnd() % MOUTH_COUNT;
            lk.hair_style = crsx_rnd() % HAIR_COUNT;
            lk.ears = crsx_rnd() % EARS_COUNT;
            lk.horns = crsx_rnd() % HORNS_COUNT;
            lk.facial_hair = crsx_rnd() % FHAIR_COUNT;
            lk.fangs = crsx_rnd() % 4;
            for (i = 0; i < LOOK_SLIDERS; i++) {
                int k = (int)(crsx_rnd() % 4);
                ((uint8_t *)&lk)[look_sliders[i].off] = (uint8_t)(k == 0 ? 0 : k == 1 ? 15 : crsx_rnd() % 16);
            }
            res_set_look(&r, &lk);
            look_for_stage(RES_LOOK_VIEW(&r), LIFE_ADULT, &d);
            memset(&g_fz, 0, sizeof g_fz);
            memset(g_fz.g0, 0xA5, CANARY);
            memset(g_fz.g1, 0xA5, CANARY);
            memset(g_fz.g2, 0xA5, CANARY);
            memset(g_fz.g3, 0xA5, CANARY);
            memset(g_fz.g4, 0xA5, CANARY);
            portrait_render(&d, RES_ALIVE, (int)(crsx_rnd() % 6), 0, g_fz.bust);
            figure_render(&d, RES_ALIVE, EXPR_NEUTRAL, 0, g_fz.fig);
            fighter_render(&d, RES_ALIVE, 4, 0, -1, g_fz.fgt);
            bodyspr_render(&d, RES_ALIVE, (int)(crsx_rnd() % 4), (int)(crsx_rnd() % 2), g_fz.chi);
            if (!crsx_guard_ok(g_fz.g0) || !crsx_guard_ok(g_fz.g1) || !crsx_guard_ok(g_fz.g2) ||
                !crsx_guard_ok(g_fz.g3) || !crsx_guard_ok(g_fz.g4))
                bad++;
        }
    }
    CHECK_EQ(bad, 0);
    opt_defaults();
}
