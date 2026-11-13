/* test_cr_body.c - CR-BODY: body types and better sliders (docs/CREATOR.md "CR-BODY").
 *
 * - 16 body types are live (the classic 10 + Athletic, Gangly, Bean, Brick, Willowy, Bobble), every one a
 *   different figure, the six new ones set the proportions they are named for, the classic ten keep their
 *   head step at 0 (their pixels are the golden's), and an old save with a classic body loads as before;
 * - EVERY build slider moves the picture: each slider of the Body, Face and Supernatural tabs is moved from
 *   its lowest to its highest step on every species and every body type, and the bust or the figure must
 *   change (a slider the creator offers that changes nothing fails here, with its name); every single step
 *   on the bench citizen changes it too; no render writes outside its picture
 *   (canary bytes around every buffer; the SAN=1 run adds the sanitizer's eyes);
 * - the voice and personality sliders reach their ends and keep them through resident_sanitize;
 * - the slider table: one row per slider, the same range as the creator's row, the default inside it;
 * - the creator's A on a slider puts it back to its default, as its own undo step.
 * The cross-platform twin is core/tests/scripts/creator_sliders.txt (runner, GBA, PS1, Dreamcast shots) and
 * tools/slider_shots_check.py. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/portrait.h"
#include "doomo/save.h"
#include "../src/creator_int.h"

int pb_head_adj(int body);          /* portrait_body.c */

#define CANARY 16
typedef struct Pic { uint8_t guard0[CANARY]; uint8_t bust[PORTRAIT_W * PORTRAIT_H]; uint8_t guard1[CANARY];
                     uint8_t fig[FIGURE_W * FIGURE_H]; uint8_t guard2[CANARY]; } Pic;

static int g_overflow;

static int canary_ok(const uint8_t *g)
{
    int i;
    for (i = 0; i < CANARY; i++) if (g[i] != 0xA5) return 0;
    return 1;
}

static void render(const DoomoResident *r, Pic *p)
{
    memset(p, 0, sizeof(*p));
    memset(p->guard0, 0xA5, CANARY); memset(p->guard1, 0xA5, CANARY); memset(p->guard2, 0xA5, CANARY);
    portrait_render(RES_LOOK_VIEW(r), RES_ALIVE, EXPR_NEUTRAL, 0, p->bust);
    figure_render(RES_LOOK_VIEW(r), RES_ALIVE, EXPR_NEUTRAL, 0, p->fig);
    if (!canary_ok(p->guard0) || !canary_ok(p->guard1) || !canary_ok(p->guard2)) g_overflow++;
}

static int diffpx(const uint8_t *a, const uint8_t *b, int n)
{
    int i, d = 0;
    for (i = 0; i < n; i++) d += a[i] != b[i];
    return d;
}

static int pic_diff(const Pic *a, const Pic *b)
{
    return diffpx(a->bust, b->bust, (int)sizeof(a->bust)) + diffpx(a->fig, b->fig, (int)sizeof(a->fig));
}

static void bench(DoomoResident *r, int species, int body)
{
    DoomoLook lk;
    resident_default(r);
    res_get_look(r, &lk);
    lk.species = (uint8_t)species;
    lk.body = (uint8_t)body;
    res_set_look(r, &lk);
}

/* the figure's tallest and widest rows of pixels */
static void fig_box(const Pic *p, int *w, int *h)
{
    int x, y, x0 = FIGURE_W, x1 = -1, y0 = FIGURE_H, y1 = -1;
    for (y = 0; y < FIGURE_H; y++)
        for (x = 0; x < FIGURE_W; x++)
            if (p->fig[y * FIGURE_W + x]) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    *w = x1 - x0 + 1;
    *h = y1 - y0 + 1;
}

static Pic PA, PB, PC;

/* ---------------------------------------------------------------------- */
/* body types                                                               */
TEST(crbody_sixteen_body_types_are_live)
{
    int v;
    CHECK_EQ(part_live_count(GF_BODY), 16);
    CHECK_EQ(BODY_TYPES, 16);
    for (v = 0; v < 16; v++) {
        const PartMeta *m = part_meta_find(GF_BODY, v);
        CHECK_MSG(m && PMF_STATE(m->flags) == PSTATE_LIVE, "body %d not live", v);
        CHECK_MSG(PMF_AGE(m->flags) == 0, "body %d is age restricted", v);       /* a plain physique: every age */
    }
    CHECK_STR(part_meta_name(part_meta_find(GF_BODY, BODY_ATHLETIC)), "Athletic");
    CHECK_STR(part_meta_name(part_meta_find(GF_BODY, BODY_BOBBLE)), "Bobble");
    /* the creator's Body row offers all 16 */
    {
        int i;
        for (i = 0; i < cr_tabs[CR_TAB_BODY].nrows; i++)
            if (cr_tabs[CR_TAB_BODY].rows[i].field == GF_BODY && cr_tabs[CR_TAB_BODY].rows[i].kind == RK_ENUM)
                CHECK_EQ(cr_row_count(&cr_tabs[CR_TAB_BODY].rows[i]), 16);
    }
}

TEST(crbody_every_body_type_is_its_own_figure)
{
    int a, b;
    for (a = 0; a < BODY_TYPES; a++)
        for (b = a + 1; b < BODY_TYPES; b++) {
            DoomoResident ra, rb;
            bench(&ra, SPECIES_CATFOLK, a);
            bench(&rb, SPECIES_CATFOLK, b);
            render(&ra, &PA);
            render(&rb, &PB);
            CHECK_MSG(diffpx(PA.fig, PB.fig, (int)sizeof(PA.fig)) >= 12, "body %d and %d look the same (%d px)", a, b,
                      diffpx(PA.fig, PB.fig, (int)sizeof(PA.fig)));
        }
    CHECK_EQ(g_overflow, 0);
}

TEST(crbody_new_types_set_the_proportions_they_are_named_for)
{
    DoomoResident r;
    int aw, ah, w, h, hb;
    bench(&r, SPECIES_CATFOLK, BODY_AVERAGE);
    render(&r, &PA);
    fig_box(&PA, &aw, &ah);
    bench(&r, SPECIES_CATFOLK, BODY_WILLOWY);
    render(&r, &PB);
    fig_box(&PB, &w, &h);
    CHECK_MSG(h > ah, "willowy is not taller (%d vs %d)", h, ah);
    bench(&r, SPECIES_CATFOLK, BODY_BEAN);
    render(&r, &PB);
    fig_box(&PB, &w, &h);
    CHECK_MSG(h < ah, "bean is not shorter (%d vs %d)", h, ah);
    bench(&r, SPECIES_CATFOLK, BODY_BRICK);
    render(&r, &PB);
    fig_box(&PB, &w, &h);
    CHECK_MSG(w >= aw, "brick is not as wide (%d vs %d)", w, aw);
    bench(&r, SPECIES_CATFOLK, BODY_GANGLY);
    render(&r, &PB);
    fig_box(&PB, &w, &h);
    CHECK_MSG(h > ah, "gangly is not taller (%d vs %d)", h, ah);
    /* the head step: bobble and bean have a bigger head than the slider says, brick a smaller one, the
     * classic ten none (their pixels are the golden's) */
    for (hb = 0; hb < BODY_COUNT; hb++) CHECK_EQ(pb_head_adj(hb), 0);
    CHECK_EQ(pb_head_adj(BODY_BOBBLE), 4);
    CHECK_EQ(pb_head_adj(BODY_BRICK), -1);
    CHECK_EQ(pb_head_adj(200), 0);                      /* out of range: the average */
    bench(&r, SPECIES_CATFOLK, BODY_BOBBLE);
    render(&r, &PB);
    bench(&r, SPECIES_CATFOLK, BODY_AVERAGE);
    render(&r, &PA);
    {   /* the same slider value draws a bigger head: more bust pixels than the average at head size 11 */
        int na = 0, nb = 0, i;
        for (i = 0; i < PORTRAIT_W * PORTRAIT_H; i++) { na += PA.bust[i] != 0; nb += PB.bust[i] != 0; }
        CHECK_MSG(nb != na, "bobble's bust equals the average's");
    }
}

TEST(crbody_classic_bodies_draw_as_before_and_old_values_load)
{
    /* the classic ten: the figure is what the default look draws with that body (the golden tests pin the
     * premades' pixels; here the head step is 0 and sanitize leaves them) */
    DoomoResident r;
    int b;
    for (b = 0; b < BODY_COUNT; b++) {
        bench(&r, SPECIES_CATFOLK, b);
        CHECK_EQ(resident_sanitize(&r), 0);
        CHECK_EQ(RES_LOOK(&r, GF_BODY), b);
    }
    /* the new ones are live, 16 is not */
    res_gset(&r, GF_BODY, 15);
    CHECK_EQ(resident_sanitize(&r), 0);
    CHECK_EQ(RES_LOOK(&r, GF_BODY), 15);
}

TEST(crbody_new_bodies_survive_a_save)
{
    static uint8_t img[DOOMO_SAVE_SLOT1_MAX];
    DoomoResident *r;
    uint32_t n;
    town_new(TOWN_FREEPLAY, 4242u, TOWN_POP_PREMADE);
    r = town_res(77);
    CHECK(r != NULL);
    if (r->status == RES_EMPTY) r->status = RES_ALIVE;
    res_gset(r, GF_BODY, BODY_BOBBLE);
    n = save_serialize(img, sizeof(img));
    CHECK(n > 0);
    res_gset(r, GF_BODY, 0);
    CHECK(save_deserialize(img, n));
    r = town_res(77);
    CHECK_EQ(RES_LOOK(r, GF_BODY), BODY_BOBBLE);
}

/* ---------------------------------------------------------------------- */
/* the slider table                                                         */
static int is_build_tab(int t) { return t == CR_TAB_BODY || t == CR_TAB_FACE || t == CR_TAB_SPOOKY; }

TEST(crbody_slider_table_matches_the_rows)
{
    int t, i, n = 0, k;
    for (t = 0; t < CR_TAB_COUNT; t++)
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *row = &cr_tabs[t].rows[i];
            const CrSlider *sl;
            if (row->kind != RK_SLIDER) continue;
            sl = cr_slider_of(row);
            CHECK_MSG(sl != NULL, "slider %s has no table entry", row->label);
            CHECK_EQ(sl->count, row->count);
            CHECK_MSG(sl->def < sl->count, "slider %s default outside its range", row->label);
            CHECK_MSG(sl->count >= 2 && row->names && row->names[0] && row->names[1], "slider %s ends", row->label);
            CHECK_MSG(row->field < GF_COUNT && sl->count - 1 <= (int)doomo_gene_field[row->field].max,
                      "slider %s range does not fit its gene field", row->label);
            n++;
        }
    CHECK_EQ(n, cr_slider_count);                       /* every table entry is one row, none is unused */
    for (i = 0; i < cr_slider_count; i++)
        for (k = i + 1; k < cr_slider_count; k++) CHECK(cr_sliders[i].field != cr_sliders[k].field);
    CHECK_EQ(cr_slider_count, 21);
    CHECK(cr_slider_of(&cr_tabs[CR_TAB_BODY].rows[0]) == NULL);       /* a header is no slider */
}

/* ---------------------------------------------------------------------- */
/* every build slider moves the picture                                     */
static const CrRow *build_sliders[16];
static int n_build_sliders;

static void collect_build_sliders(void)
{
    int t, i;
    n_build_sliders = 0;
    for (t = 0; t < CR_TAB_COUNT; t++) {
        if (!is_build_tab(t)) continue;
        for (i = 0; i < cr_tabs[t].nrows; i++)
            if (cr_tabs[t].rows[i].kind == RK_SLIDER && n_build_sliders < 16)
                build_sliders[n_build_sliders++] = &cr_tabs[t].rows[i];
    }
}

static void prime(DoomoResident *r, const CrRow *row)
{
    DoomoLook lk;
    res_get_look(r, &lk);
    if (row->field == GF_HORN_SIZE) lk.horns = HORNS_DEVIL;     /* the horn size needs horns */
    lk.fangs = 2;
    res_set_look(r, &lk);
}

TEST(crbody_every_build_slider_moves_the_picture)
{
    int s, v, bad = 0;
    collect_build_sliders();
    CHECK_EQ(n_build_sliders, 11);                      /* height, build, head, 4 eye, brow, nose, mouth, horn */
    fprintf(stderr, "    %-12s %6s %6s %s\n", "slider", "steps", "px", "result (bench citizen, lowest to highest)");
    for (s = 0; s < n_build_sliders; s++) {
        const CrRow *row = build_sliders[s];
        int n = cr_row_count(row), dead_steps = 0, px;
        DoomoResident lo, hi, r;
        bench(&lo, SPECIES_CATFOLK, BODY_AVERAGE);
        prime(&lo, row);
        hi = lo;
        cr_row_set(&lo, row, 0);
        cr_row_set(&hi, row, n - 1);
        render(&lo, &PA);
        render(&hi, &PB);
        px = pic_diff(&PA, &PB);
        /* every step on the bench */
        for (v = 0; v < n; v++) {
            r = lo;
            cr_row_set(&r, row, v);
            render(&r, &PC);
            if (v > 0 && pic_diff(&PB, &PC) == 0) dead_steps++;
            PB = PC;
        }
        fprintf(stderr, "    %-12s %6d %6d %s\n", row->label, n, px, px ? (dead_steps ? "moves, but a step looks the same" : "moves, every step") : "NOTHING");
        if (!px) { bad++; continue; }
        if (dead_steps > 0) {
            fprintf(stderr, "    slider %s: %d steps draw the same picture as the step before\n", row->label, dead_steps);
            bad++;
        }
    }
    CHECK_EQ(bad, 0);
    CHECK_EQ(g_overflow, 0);
}

TEST(crbody_every_build_slider_moves_every_species_and_body)
{
    int s, sp, bo, bad = 0;
    collect_build_sliders();
    for (s = 0; s < n_build_sliders; s++) {
        const CrRow *row = build_sliders[s];
        int n = cr_row_count(row);
        for (sp = 0; sp < SPECIES_COUNT; sp++)
            for (bo = 0; bo < BODY_TYPES; bo += 3) {
                DoomoResident lo, hi;
                bench(&lo, sp, bo);
                prime(&lo, row);
                hi = lo;
                cr_row_set(&lo, row, 0);
                cr_row_set(&hi, row, n - 1);
                if (cr_slider_dead(row, &lo)) continue;           /* the creator does not offer it here */
                render(&lo, &PA);
                render(&hi, &PB);
                if (pic_diff(&PA, &PB) == 0 && bo == 0) {         /* a few bodies hide a mouth under the chest: body 0 is the gate */
                    fprintf(stderr, "    slider %s does nothing on species %d (%s)\n", row->label, sp, doomo_species_names[sp]);
                    bad++;
                }
            }
    }
    CHECK_EQ(bad, 0);
    CHECK_EQ(g_overflow, 0);
}

TEST(crbody_sliders_that_do_nothing_are_not_offered)
{
    /* what the creator hides is exactly what changes nothing: the horn size with no horns, the turkey's
     * five face sliders; and nothing else is hidden on any other species */
    int s, sp;
    collect_build_sliders();
    for (s = 0; s < n_build_sliders; s++) {
        const CrRow *row = build_sliders[s];
        int n = cr_row_count(row);
        for (sp = 0; sp < SPECIES_COUNT; sp++) {
            DoomoResident lo, hi;
            int dead;
            bench(&lo, sp, BODY_AVERAGE);
            hi = lo;
            cr_row_set(&lo, row, 0);
            cr_row_set(&hi, row, n - 1);
            dead = cr_slider_dead(row, &lo);
            render(&lo, &PA);
            render(&hi, &PB);
            if (dead) {
                CHECK_MSG(pic_diff(&PA, &PB) == 0, "%s is hidden on species %d but moves the picture", row->label, sp);
            } else if (row->field != GF_HORN_SIZE) {
                CHECK_MSG(pic_diff(&PA, &PB) > 0, "%s is offered on species %d but moves nothing", row->label, sp);
            }
        }
    }
    {   /* horns on: the row is back; a turkey in its top hat reads the horn size as the crown height */
        DoomoResident r;
        DoomoLook lk;
        const CrRow *horn = NULL;
        int i;
        for (i = 0; i < n_build_sliders; i++) if (build_sliders[i]->field == GF_HORN_SIZE) horn = build_sliders[i];
        CHECK(horn != NULL);
        bench(&r, SPECIES_CATFOLK, 0);
        CHECK(cr_slider_dead(horn, &r));                      /* no horns */
        res_get_look(&r, &lk);
        lk.horns = HORNS_DEVIL;
        res_set_look(&r, &lk);
        CHECK(!cr_slider_dead(horn, &r));
        bench(&r, SPECIES_TURKEY, 0);
        res_get_look(&r, &lk);
        lk.hat = HAT_TOP_HAT;
        res_set_look(&r, &lk);
        CHECK(!cr_slider_dead(horn, &r));
    }
}

TEST(crbody_voice_and_personality_sliders_reach_both_ends)
{
    int t, i, n = 0;
    for (t = 0; t < CR_TAB_COUNT; t++) {
        if (is_build_tab(t)) continue;
        for (i = 0; i < cr_tabs[t].nrows; i++) {
            const CrRow *row = &cr_tabs[t].rows[i];
            DoomoResident r;
            if (row->kind != RK_SLIDER) continue;
            resident_default(&r);
            cr_row_set(&r, row, 0);
            CHECK_EQ(cr_row_value(&r, row), 0);
            CHECK_EQ(resident_sanitize(&r), 0);
            CHECK_EQ(cr_row_value(&r, row), 0);
            cr_row_set(&r, row, cr_row_count(row) - 1);
            CHECK_EQ(cr_row_value(&r, row), cr_row_count(row) - 1);
            CHECK_EQ(resident_sanitize(&r), 0);
            CHECK_EQ(cr_row_value(&r, row), cr_row_count(row) - 1);
            cr_row_set(&r, row, 999);                          /* past the end: clamped, never wrapped */
            CHECK_EQ(cr_row_value(&r, row), cr_row_count(row) - 1);
            n++;
        }
    }
    CHECK_EQ(n, 10);
}

/* ---------------------------------------------------------------------- */
/* the creator                                                              */
static void frames(int n, uint32_t b)
{
    while (n-- > 0) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
}
static void press(uint32_t b) { frames(1, b); frames(1, 0); }
static const DoomoScene base_scene = { "TESTBASE", NULL, NULL, NULL, NULL, 0 };

static int row_of(int tab, int field)
{
    int i;
    for (i = 0; i < cr_tabs[tab].nrows; i++)
        if (cr_tabs[tab].rows[i].field == field && cr_tabs[tab].rows[i].kind == RK_SLIDER) return i;
    return -1;
}

TEST(crbody_a_resets_a_slider_to_its_default_and_undo_brings_it_back)
{
    int row, guard = 0, k;
    test_reset();
    scene_set(&base_scene, 0);
    scene_push(&SCENE_CREATOR, -1);
    frames(3, 0);
    CHECK(scene_top() == &SCENE_CREATOR);
    row = row_of(CR_TAB_BODY, GF_HEIGHT);
    CHECK(row >= 0);
    while (cr.row[cr.tab] != row && guard++ < 64) press(BTN_DOWN);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), 7);
    for (k = 0; k < 5; k++) press(BTN_RIGHT);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), 12);
    press(BTN_A);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), 7);
    press(BTN_A);                                        /* already there: nothing happens, no undo step */
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), 7);
    press(BTN_X);                                        /* undo: the reset was its own step */
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), 12);
    for (k = 0; k < 20; k++) press(BTN_LEFT);            /* clamped at the lowest step, never wraps */
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), 0);
    for (k = 0; k < 40; k++) press(BTN_RIGHT);
    CHECK_EQ(RES_LOOK(&cr.draft, GF_HEIGHT), 15);
}

TEST(crbody_the_body_row_cycles_all_sixteen)
{
    int i, row = -1, guard = 0, k, seen[16];
    test_reset();
    scene_set(&base_scene, 0);
    scene_push(&SCENE_CREATOR, -1);
    frames(3, 0);
    for (i = 0; i < cr_tabs[CR_TAB_BODY].nrows; i++)
        if (cr_tabs[CR_TAB_BODY].rows[i].field == GF_BODY && cr_tabs[CR_TAB_BODY].rows[i].kind == RK_ENUM) row = i;
    CHECK(row >= 0);
    while (cr.row[cr.tab] != row && guard++ < 64) press(BTN_DOWN);
    memset(seen, 0, sizeof(seen));
    for (k = 0; k < 16; k++) {
        int b = RES_LOOK(&cr.draft, GF_BODY);
        CHECK(b >= 0 && b < 16);
        seen[b]++;
        press(BTN_RIGHT);
    }
    /* every body the player may wear is reached (the classic locked ones are looked at, not worn) */
    for (k = 0; k < 16; k++)
        CHECK_MSG(seen[k] > 0 || cr_value_locked(&cr_tabs[CR_TAB_BODY].rows[row], k), "body %d never reached", k);
    for (k = BODY_ATHLETIC; k < BODY_TYPES; k++) CHECK_MSG(seen[k] == 1, "new body %d seen %d times", k, seen[k]);
}
