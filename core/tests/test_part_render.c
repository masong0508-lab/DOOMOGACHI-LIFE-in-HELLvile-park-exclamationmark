/* test_part_render.c - drawing many parts (CR2): the part scripts as live slot values.
 *
 * - a scripted value is drawn by its painter's hook: it renders, it is distinct from every other
 *   value of its slot (on three citizens), and sanitize keeps it while it clamps an unscripted one;
 * - the shape gate: a scripted part differs from every other value of its slot by >= 24 px of the
 *   bust with every material forced to one ramp (no palette swaps), >= 6 px on a chibi if it draws
 *   there at all;
 * - a part without a chibi block changes no chibi pixel (crowds pay nothing for it);
 * - every macro op draws, inside the canvas, deterministically, in every view;
 * - a kitchen-sink look of scripted parts stays inside the pixel-visit bound;
 * - the catalog's cost weights match the renderer (core/data/parts/catalog/costs.txt);
 * - the tall-hat top extent makes room on the bust. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/portrait.h"
#include "doomo/part_meta.h"
#include "doomo/parts.h"
#include "../src/portrait_int.h"

static DoomoLook bench_look(void)
{
    DoomoResident r;
    resident_default(&r);
    return *RES_LOOK_VIEW(&r);
}

static uint32_t fnv(const uint8_t *p, int n, uint32_t h)
{
    int i;
    for (i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

/* the look byte of a gene list (GF_BODY + i is look byte 1 + i) */
static int look_off(int list)
{
    if (list == PL_SPECIES) return 0;
    if (list >= 0 && list <= GF_PATTERN_COLOR) return 1 + list;
    return -1;
}

static void set_part(DoomoLook *l, int list, int v)
{
    int o = look_off(list);
    if (o >= 0) *((uint8_t *)l + o) = (uint8_t)v;
}

static uint32_t render_hash(const DoomoLook *l)
{
    static uint8_t por[PORTRAIT_W * PORTRAIT_H], fig[FIGURE_W * FIGURE_H], spr[BODYSPR_W * BODYSPR_H];
    static uint8_t fgt[FIGHTER_W * FIGHTER_H];
    uint32_t h = 2166136261u;
    portrait_render(l, RES_ALIVE, EXPR_NEUTRAL, 0, por);
    h = fnv(por, (int)sizeof(por), h);
    figure_render(l, RES_ALIVE, EXPR_NEUTRAL, 0, fig);
    h = fnv(fig, (int)sizeof(fig), h);
    bodyspr_render(l, RES_ALIVE, 0, 0, spr);
    h = fnv(spr, (int)sizeof(spr), h);
    fighter_render(l, RES_ALIVE, 4, 0, -1, fgt);
    return fnv(fgt, (int)sizeof(fgt), h);
}

/* a value the slot can show: live, or drawn by a script (hidden rows of scripted lists) */
static int drawable(const PartMeta *m)
{
    int st = PMF_STATE(m->flags);
    return st == PSTATE_LIVE || (st == PSTATE_HIDDEN && (m->flags & PMF_SCRIPTED));
}

static int scripted(const PartMeta *m)
{
    return (m->flags & PMF_SCRIPTED) && PMF_STATE(m->flags) != PSTATE_TEST && look_off(m->list) >= 0;
}

TEST(part_scripts_are_drawn_by_their_slot_hooks_and_distinct)
{
    int s, i, k, bad = 0, nscripted = 0;
    for (s = 0; s < PART_SLOT_COUNT; s++) {
        const PartSlotInfo *sl = &doomo_part_slots[s];
        static uint32_t h[256];
        int any = 0;
        if (look_off(sl->list) < 0) continue;
        for (i = sl->first; i < sl->first + sl->count; i++) any |= scripted(&doomo_part_meta[i]);
        if (!any) continue;
        for (i = sl->first; i < sl->first + sl->count; i++) {
            const PartMeta *m = &doomo_part_meta[i];
            DoomoLook l = bench_look();
            if (!drawable(m)) continue;
            set_part(&l, m->list, m->value);
            if (m->list == GF_TOP) l.top_color = PAINT_COBALT;
            h[m->value] = render_hash(&l);
            for (k = sl->first; k < i; k++) {
                const PartMeta *o = &doomo_part_meta[k];
                if (drawable(o) && (scripted(m) || scripted(o)) && h[o->value] == h[m->value]) {
                    fprintf(stderr, "    %s: %d renders the same as %d\n", sl->key, m->value, o->value);
                    bad++;
                }
            }
            if (scripted(m)) nscripted++;
        }
    }
    CHECK_EQ(bad, 0);
    CHECK(nscripted >= 15);                                    /* the CR2 family batch */
}

TEST(part_scripts_show_on_other_citizens_too)
{
    static const uint32_t seeds[3] = { 11u, 4242u, 9001u };
    int c, i, bad = 0;
    for (c = 0; c < 3; c++) {
        DoomoResident r;
        DoomoLook base;
        if (c == 1) resident_generate_hellborn(&r, seeds[c]); else resident_generate(&r, seeds[c]);
        base = *RES_LOOK_VIEW(&r);
        for (i = 0; i < PART_META_COUNT; i++) {
            const PartMeta *m = &doomo_part_meta[i];
            DoomoLook a = base, z = base;
            if (!scripted(m)) continue;
            set_part(&a, m->list, m->value);
            set_part(&z, m->list, 0);
            if (render_hash(&a) == render_hash(&z)) {
                fprintf(stderr, "    citizen %d: %s is invisible\n", c, part_meta_name(m));
                bad++;
            }
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(part_sanitize_keeps_scripted_values_and_clamps_the_rest)
{
    DoomoLook a = bench_look(), b = bench_look(), z = bench_look();
    a.hat = 30;                                                /* Bow Beret: a script */
    b.hat = 62;                                                /* nothing draws 62 */
    z.hat = 0;
    CHECK(render_hash(&a) != render_hash(&z));
    CHECK(render_hash(&b) == render_hash(&z));
    b = bench_look(); b.accessory = 31;
    z = bench_look(); z.accessory = 0;
    CHECK(render_hash(&b) == render_hash(&z));
}

static int px_diff(const uint8_t *a, const uint8_t *b, int n)
{
    int i, d = 0;
    for (i = 0; i < n; i++) d += a[i] != b[i];
    return d;
}

TEST(part_scripts_pass_the_shape_gate)
{
    /* every material on one ramp: a part must differ from every other value of its slot by its
     * SHAPE, >= 24 px of the bust; and on a chibi by >= 6 px unless it does not draw there at all */
    static uint8_t bust[256][PORTRAIT_W * PORTRAIT_H], chibi[256][BODYSPR_W * BODYSPR_H];
    int s, i, k, bad = 0;
    portrait_shape_mode(1);
    for (s = 0; s < PART_SLOT_COUNT; s++) {
        const PartSlotInfo *sl = &doomo_part_slots[s];
        int any = 0;
        if (look_off(sl->list) < 0) continue;
        for (i = sl->first; i < sl->first + sl->count; i++) any |= scripted(&doomo_part_meta[i]);
        if (!any) continue;
        for (i = sl->first; i < sl->first + sl->count; i++) {
            const PartMeta *m = &doomo_part_meta[i];
            DoomoLook l = bench_look();
            if (!drawable(m)) continue;
            set_part(&l, m->list, m->value);
            portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, bust[m->value]);
            bodyspr_render(&l, RES_ALIVE, 0, 0, chibi[m->value]);
        }
        for (i = sl->first; i < sl->first + sl->count; i++) {
            const PartMeta *m = &doomo_part_meta[i];
            int c0;
            if (!scripted(m)) continue;
            c0 = px_diff(chibi[m->value], chibi[0], BODYSPR_W * BODYSPR_H);
            for (k = sl->first; k < sl->first + sl->count; k++) {
                const PartMeta *o = &doomo_part_meta[k];
                int d, dc;
                if (o == m || !drawable(o)) continue;
                d = px_diff(bust[m->value], bust[o->value], PORTRAIT_W * PORTRAIT_H);
                dc = px_diff(chibi[m->value], chibi[o->value], BODYSPR_W * BODYSPR_H);
                if (d < 24) {
                    fprintf(stderr, "    %s: %s vs %s differ by %d px (shape gate 24)\n", sl->key,
                            part_meta_name(m), part_meta_name(o), d);
                    bad++;
                }
                if (c0 && o->value && dc < 6 && (o->flags & PMF_SCRIPTED)) {
                    fprintf(stderr, "    %s: %s vs %s differ by %d chibi px (gate 6)\n", sl->key,
                            part_meta_name(m), part_meta_name(o), dc);
                    bad++;
                }
            }
        }
    }
    portrait_shape_mode(0);
    CHECK_EQ(bad, 0);
}

TEST(part_without_a_chibi_block_changes_no_chibi_pixel)
{
    static uint8_t a[BODYSPR_W * BODYSPR_H], b[BODYSPR_W * BODYSPR_H];
    DoomoLook l = bench_look();
    int dir, fr;
    for (dir = 0; dir < 4; dir++)
        for (fr = 0; fr < 4; fr++) {
            l.accessory = ACC_NONE;
            bodyspr_render(&l, RES_ALIVE, dir, fr, a);
            l.accessory = 21;                                  /* Pearl Necklace: lod mid,big only */
            bodyspr_render(&l, RES_ALIVE, dir, fr, b);
            CHECK(memcmp(a, b, sizeof(a)) == 0);
        }
    l.accessory = 21;                                          /* ... but the bust shows it */
    {
        static uint8_t p0[PORTRAIT_W * PORTRAIT_H], p1[PORTRAIT_W * PORTRAIT_H];
        portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, p1);
        l.accessory = ACC_NONE;
        portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, p0);
        CHECK(px_diff(p0, p1, PORTRAIT_W * PORTRAIT_H) > 10);
    }
}

TEST(part_macro_ops_draw_inside_the_canvas_in_every_view)
{
    /* a canary round each buffer; every scripted part in bust, figure, chibi (4 dirs) and the 5
     * fighter views, twice: the same pixels both times */
    enum { PAD = 32 };
    static uint8_t buf[FIGURE_W * FIGURE_H + 2 * PAD], again[FIGURE_W * FIGURE_H];
    int i, v, bad = 0;
    for (i = 0; i < PART_META_COUNT; i++) {
        const PartMeta *m = &doomo_part_meta[i];
        DoomoLook l = bench_look();
        if (!scripted(m)) continue;
        set_part(&l, m->list, m->value);
        for (v = 0; v < 11; v++) {
            int n, k;
            uint8_t *out = buf + PAD;
            memset(buf, 0xA5, sizeof(buf));
            if (v == 0) { portrait_render(&l, RES_ALIVE, EXPR_HAPPY, 92, out); n = PORTRAIT_W * PORTRAIT_H; }
            else if (v == 1) { figure_render(&l, RES_ALIVE, EXPR_NEUTRAL, 40, out); n = FIGURE_W * FIGURE_H; }
            else if (v < 6) { bodyspr_render(&l, RES_ALIVE, v - 2, v, out); n = BODYSPR_W * BODYSPR_H; }
            else { fighter_render(&l, RES_ALIVE, v - 6, 1, -1, out); n = FIGHTER_W * FIGHTER_H; }
            for (k = 0; k < PAD; k++) bad += buf[k] != 0xA5 || out[n + k] != 0xA5;
            memcpy(again, out, (size_t)n);
            if (v == 0) portrait_render(&l, RES_ALIVE, EXPR_HAPPY, 92, out);
            else if (v == 1) figure_render(&l, RES_ALIVE, EXPR_NEUTRAL, 40, out);
            else if (v < 6) bodyspr_render(&l, RES_ALIVE, v - 2, v, out);
            else fighter_render(&l, RES_ALIVE, v - 6, 1, -1, out);
            bad += memcmp(again, out, (size_t)n) != 0;
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(part_kitchen_sink_stays_inside_the_render_budget)
{
    DoomoLook l = bench_look();
    static uint8_t por[PORTRAIT_W * PORTRAIT_H], fig[FIGURE_W * FIGURE_H];
    int h;
    l.top = 24; l.accessory = 20; l.hair_style = HAIR_LONG; l.horns = HORNS_RAM; l.wings = WINGS_ANGEL;
    l.tail = TAIL_FOX; l.aura = AURA_SPARKLES; l.halo = HALO_HALO; l.ears = EARS_BUNNY;
    for (h = 24; h <= 35; h++) {
        l.hat = (uint8_t)h;
        portrait_render(&l, RES_ALIVE, EXPR_HAPPY, 0, por);
        CHECK_MSG(portrait_last_ops() < 12000, "hat %d bust: %u visits", h, (unsigned)portrait_last_ops());
        figure_render(&l, RES_ALIVE, EXPR_HAPPY, 0, fig);
        CHECK_MSG(portrait_last_ops() < 12000, "hat %d figure: %u visits", h, (unsigned)portrait_last_ops());
    }
}

TEST(part_tall_scripted_hats_make_room_on_the_bust)
{
    DoomoLook l = bench_look();
    l.ears = EARS_NORMAL; l.horns = HORNS_NONE; l.hair_style = HAIR_SHORT;
    l.hat = 25;                                                /* Stovepipe */
    CHECK(ps_top(GF_HAT, 25) >= 400);
    CHECK(ph_top_extent(&l) >= ps_top(GF_HAT, 25));
    l.hat = 27;                                                /* Pillbox: a low hat */
    CHECK(ps_top(GF_HAT, 27) < ps_top(GF_HAT, 25));
    CHECK_EQ(ps_top(GF_HAT, 3), 0);                            /* a painted hat has no script */
}

/* ---- cost weights ---------------------------------------------------------------------------- */
static int measure_cost(const PartMeta *m)
{
    static uint8_t por[PORTRAIT_W * PORTRAIT_H], fig[FIGURE_W * FIGURE_H], spr[BODYSPR_W * BODYSPR_H];
    DoomoLook l = bench_look();
    uint32_t with = 0, without = 0;
    int pass;
    if (look_off(m->list) < 0 && !PL_IS_DEEP(m->list)) return 0;
    for (pass = 0; pass < 2; pass++) {
        uint32_t t = 0;
        int v = pass ? 0 : m->value;
        if (PL_IS_DEEP(m->list)) {
            DoomoDeep d = doomo_deep_zero;
            d.v[m->list - PL_DEEP0] = (uint8_t)v;
            portrait_render_deep(&l, &d, RES_ALIVE, EXPR_NEUTRAL, 0, por);
            t = portrait_last_ops();
        } else {
            set_part(&l, m->list, v);
            portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, por); t += portrait_last_ops();
            figure_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, fig); t += portrait_last_ops();
            bodyspr_render(&l, RES_ALIVE, 0, 0, spr); t += portrait_last_ops();
        }
        if (pass) without = t; else with = t;
    }
    if (with <= without) return 0;
    return (int)((with - without + 32) / 64) > 255 ? 255 : (int)((with - without + 32) / 64);
}

TEST(part_costs_match_the_renderer)
{
    /* units of 64 pixel visits over the slot's value 0 on the bench citizen (bust + figure + chibi;
     * deep fields: the bust). A drift of a quarter (at least 2 units) fails: refresh with
     * `build/tests --update-golden part_costs_match_the_renderer` and `python3 tools/partcat_build.py` */
    int i, bad = 0;
    FILE *f = NULL;
    if (test_update_golden) {
        char path[512];
        snprintf(path, sizeof(path), "%s/../../data/parts/catalog/costs.txt", test_golden_dir);
        f = fopen(path, "w");
        CHECK_MSG(f != NULL, "cannot write %s", path);
        fprintf(f, "# core/data/parts/catalog/costs.txt - draw cost weights (CR2): SLOT VALUE COST, in units of 64\n"
                   "# pixel visits over the slot's value 0 on the bench citizen (bust + figure + chibi; deep fields\n"
                   "# the bust). MEASURED, not hand-written: build/tests --update-golden part_costs_match_the_renderer\n"
                   "# then python3 tools/partcat_build.py. The creator's Busy meter sums them (CR9).\n");
    }
    for (i = 0; i < PART_META_COUNT; i++) {
        const PartMeta *m = &doomo_part_meta[i];
        int c, tol;
        if (!drawable(m) && PMF_STATE(m->flags) != PSTATE_HIDDEN) continue;
        c = measure_cost(m);
        if (f && c) fprintf(f, "%s %d %d\n", doomo_part_slots[m->slot].key, m->value, c);
        tol = m->cost / 4 > 2 ? m->cost / 4 : 2;
        if (!f && (c > m->cost + tol || c + tol < m->cost)) {
            fprintf(stderr, "    %s %d (%s): measured %d, catalog %d\n", doomo_part_slots[m->slot].key, m->value,
                    part_meta_name(m), c, m->cost);
            bad++;
        }
    }
    if (f) fclose(f);
    CHECK_EQ(bad, 0);
}
