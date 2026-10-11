/* test_portrait.c - procedural resident renderer (owner: portrait).
 *
 * - every option of every catalog list visibly changes the render (the
 *   hash of bust + figure + chibi differs from every other option's);
 * - sliders, expressions and statuses change the image;
 * - rendering never writes outside the buffer (canary bytes), even for
 *   garbage looks, and is deterministic;
 * - the cached accessors keep uids stable until the pixels change;
 * - a bust stays cheap (pixel-visit budget).
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/portrait.h"
#include "doomo/weld.h"

#define CANARY 0xA5
#define PAD    64

static uint32_t fnv(const uint8_t *p, int n, uint32_t h)
{
    int i;
    for (i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static DoomoLook base_look(void)
{
    DoomoResident r;
    resident_default(&r);
    return r.look;
}

/* hash of everything a player can see of a look */
static uint32_t look_render_hash(const DoomoLook *l, int status, int expr)
{
    static uint8_t por[PORTRAIT_W * PORTRAIT_H], fig[FIGURE_W * FIGURE_H];
    static uint8_t spr[BODYSPR_W * BODYSPR_H];
    uint32_t h = 2166136261u;
    portrait_render(l, status, expr, 0, por);
    h = fnv(por, (int)sizeof(por), h);
    figure_render(l, status, expr, 0, fig);
    h = fnv(fig, (int)sizeof(fig), h);
    bodyspr_render(l, status, 0, 0, spr);
    h = fnv(spr, (int)sizeof(spr), h);
    return h;
}

typedef struct OptList { const char *name; size_t off; int count; } OptList;
#define OL(n, f, c) { n, offsetof(DoomoLook, f), c }
static const OptList lists[] = {
    OL("species", species, SPECIES_COUNT), OL("body", body, BODY_COUNT),
    OL("head", head_shape, HEAD_COUNT), OL("eyes", eye_style, EYE_COUNT),
    OL("brows", brow_style, BROW_COUNT), OL("nose", nose_style, NOSE_COUNT),
    OL("mouth", mouth_style, MOUTH_COUNT), OL("ears", ears, EARS_COUNT),
    OL("hair", hair_style, HAIR_COUNT), OL("fhair", facial_hair, FHAIR_COUNT),
    OL("horns", horns, HORNS_COUNT), OL("tail", tail, TAIL_COUNT),
    OL("wings", wings, WINGS_COUNT), OL("halo", halo, HALO_COUNT),
    OL("marking", marking, MARK_COUNT), OL("top", top, TOP_COUNT),
    OL("bottom", bottom, BOTTOM_COUNT), OL("shoes", shoes, SHOES_COUNT),
    OL("hat", hat, HAT_COUNT), OL("accessory", accessory, ACC_COUNT),
    OL("aura", aura, AURA_COUNT), OL("pattern", pattern, PATTERN_COUNT),
};

#define MAXOPT 40

static int check_list_distinct(const OptList *o, const DoomoLook *base)
{
    uint32_t h[MAXOPT];
    int v, k, bad = 0;
    for (v = 0; v < o->count && v < MAXOPT; v++) {
        DoomoLook l = *base;
        *((uint8_t *)&l + o->off) = (uint8_t)v;
        h[v] = look_render_hash(&l, RES_ALIVE, EXPR_NEUTRAL);
        for (k = 0; k < v; k++)
            if (h[k] == h[v]) {
                fprintf(stderr, "    %s: option %d renders the same as option %d\n", o->name, v, k);
                bad++;
            }
    }
    return bad;
}

TEST(portrait_every_catalog_option_is_visible)
{
    int li, bad = 0;
    DoomoLook base = base_look();
    for (li = 0; li < (int)(sizeof(lists) / sizeof(lists[0])); li++)
        bad += check_list_distinct(&lists[li], &base);
    CHECK_EQ(bad, 0);
}

TEST(portrait_options_visible_on_other_citizens)
{
    /* a bald human in a plain tee, and a hellborn: options still differ
     * from option 0 (pairwise distinctness is checked on the default) */
    static const uint32_t seeds[2] = { 4242u, 777u };
    int li, i, v, bad = 0;
    for (i = 0; i < 2; i++) {
        DoomoResident r;
        if (i) {
            /* a hellborn imp; nothing covering the mouth, legs to dress */
            resident_generate_hellborn(&r, seeds[i]);
            r.look.species = SPECIES_IMP;
            r.look.accessory = ACC_NONE;
            r.look.top = TOP_TSHIRT;
            r.look.eye_count = 2;
            r.look.facial_hair = FHAIR_NONE;
            r.look.hair_style = HAIR_SHORT;
            r.look.ears = EARS_POINTY;
        } else {
            resident_default(&r);
            r.look.species = SPECIES_HUMAN;
            r.look.skin = SKIN_TAN;
            r.look.hair_style = HAIR_BALD;
            r.look.ears = EARS_NORMAL;
            r.look.tail = TAIL_NONE;
            r.look.marking = MARK_NONE;
            r.look.top = TOP_TSHIRT;
            (void)seeds;
        }
        for (li = 0; li < (int)(sizeof(lists) / sizeof(lists[0])); li++) {
            const OptList *o = &lists[li];
            DoomoLook l0 = r.look;
            uint32_t h0;
            *((uint8_t *)&l0 + o->off) = 0;
            h0 = look_render_hash(&l0, RES_ALIVE, EXPR_NEUTRAL);
            for (v = 1; v < o->count; v++) {
                DoomoLook l = r.look;
                *((uint8_t *)&l + o->off) = (uint8_t)v;
                if (look_render_hash(&l, RES_ALIVE, EXPR_NEUTRAL) == h0) {
                    fprintf(stderr, "    citizen %d %s: option %d invisible\n", i, o->name, v);
                    bad++;
                }
            }
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(portrait_cheek_flags_are_visible)
{
    DoomoLook base = base_look();
    uint32_t h[9];
    int b, k, bad = 0;
    for (b = 0; b <= 8; b++) {
        DoomoLook l = base;
        l.cheeks = (uint8_t)(b ? 1 << (b - 1) : 0);
        h[b] = look_render_hash(&l, RES_ALIVE, EXPR_NEUTRAL);
        for (k = 0; k < b; k++) if (h[k] == h[b]) { fprintf(stderr, "    cheek %d == %d\n", b, k); bad++; }
    }
    CHECK_EQ(bad, 0);
}

typedef struct Slider { const char *name; size_t off; int lo, hi; } Slider;
#define SL(n, f, lo, hi) { n, offsetof(DoomoLook, f), lo, hi }

TEST(portrait_every_slider_step_is_visible)
{
    static const Slider sl[] = {
        SL("height", height, 0, 15), SL("build", build, 0, 15),
        SL("head_size", head_size, 0, 15), SL("eye_size", eye_size, 0, 15),
        SL("eye_spacing", eye_spacing, 0, 15), SL("eye_height", eye_height, 0, 15),
        SL("eye_tilt", eye_tilt, 0, 15), SL("eye_count", eye_count, 1, 3),
        SL("brow_height", brow_height, 0, 15), SL("nose_size", nose_size, 0, 15),
        SL("mouth_size", mouth_size, 0, 15), SL("fangs", fangs, 0, 3),
        SL("horn_size", horn_size, 0, 15),
    };
    int i, v, bad = 0;
    for (i = 0; i < (int)(sizeof(sl) / sizeof(sl[0])); i++) {
        uint32_t prev = 0;
        for (v = sl[i].lo; v <= sl[i].hi; v++) {
            DoomoLook l = base_look();
            uint32_t h;
            if (sl[i].off == offsetof(DoomoLook, horn_size)) l.horns = HORNS_DEVIL_TALL;
            if (sl[i].off == offsetof(DoomoLook, brow_height)) l.brow_style = BROW_THICK;
            if (sl[i].off == offsetof(DoomoLook, fangs)) l.mouth_style = MOUTH_SMILE;
            *((uint8_t *)&l + sl[i].off) = (uint8_t)v;
            h = look_render_hash(&l, RES_ALIVE, EXPR_NEUTRAL);
            if (v > sl[i].lo && h == prev) {
                fprintf(stderr, "    slider %s: %d looks like %d\n", sl[i].name, v, v - 1);
                bad++;
            }
            prev = h;
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(portrait_colours_and_highlight_are_visible)
{
    DoomoLook base = base_look();
    uint32_t h0 = look_render_hash(&base, RES_ALIVE, EXPR_NEUTRAL);
    DoomoLook l;
    int p, bad = 0;
    for (p = 0; p < PAL_PAINT_COUNT; p++) {
        l = base; l.hair_highlight = (uint8_t)p;
        if (p != l.hair_color && look_render_hash(&l, RES_ALIVE, EXPR_NEUTRAL) == h0) bad++;
        l = base; l.eye_color = (uint8_t)p;
        if (p != base.eye_color && look_render_hash(&l, RES_ALIVE, EXPR_NEUTRAL) == h0) bad++;
        l = base; l.top_color = (uint8_t)p;
        if (p != base.top_color && look_render_hash(&l, RES_ALIVE, EXPR_NEUTRAL) == h0) bad++;
    }
    for (p = 0; p < 8 + PAL_PAINT_COUNT; p++) {
        l = base; l.skin = (uint8_t)p;
        if (p != base.skin && look_render_hash(&l, RES_ALIVE, EXPR_NEUTRAL) == h0) bad++;
    }
    CHECK_EQ(bad, 0);
}

TEST(portrait_expressions_and_statuses_differ)
{
    static const int st[5] = { RES_ALIVE, RES_POSSESSED, RES_SOUL_LOST, RES_DEAD, RES_GHOST };
    DoomoLook l = base_look();
    uint32_t h[EXPR_COUNT];
    int e, k, bad = 0;
    for (e = 0; e < EXPR_COUNT; e++) {
        static uint8_t por[PORTRAIT_W * PORTRAIT_H];
        portrait_render(&l, RES_ALIVE, e, 0, por);
        h[e] = fnv(por, (int)sizeof(por), 2166136261u);
        for (k = 0; k < e; k++) if (h[k] == h[e]) { fprintf(stderr, "    expr %d == %d\n", e, k); bad++; }
    }
    for (e = 0; e < 5; e++) {
        h[e] = look_render_hash(&l, st[e], EXPR_NEUTRAL);
        for (k = 0; k < e; k++) if (h[k] == h[e]) { fprintf(stderr, "    status %d == %d\n", e, k); bad++; }
    }
    CHECK_EQ(bad, 0);
}

TEST(portrait_animates)
{
    /* blinking, breathing and tails change the picture over time */
    DoomoLook l = base_look();
    static uint8_t a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    int t, changes = 0;
    portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, a);
    for (t = 1; t < 200; t++) {
        portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, t, b);
        if (memcmp(a, b, sizeof(a)) != 0) { changes++; memcpy(a, b, sizeof(a)); }
    }
    CHECK(changes >= 4);
}

/* render into a padded buffer and verify the canaries */
static int canary_ok(const uint8_t *buf, int n)
{
    int i;
    for (i = 0; i < PAD; i++)
        if (buf[i] != CANARY || buf[PAD + n + i] != CANARY) return 0;
    return 1;
}

TEST(portrait_never_writes_out_of_bounds)
{
    static uint8_t buf[PAD + FIGURE_W * FIGURE_H + PAD];
    DoomoRng rng;
    int i, k, bad = 0;
    rng_seed(&rng, 31337);
    for (i = 0; i < 160; i++) {
        DoomoResident r;
        int status = (int)rng_range(&rng, 6), expr = (int)rng_range(&rng, EXPR_COUNT);
        if (i < 60) resident_generate(&r, (uint32_t)i * 977u);
        else if (i < 120) resident_generate_hellborn(&r, (uint32_t)i * 977u);
        else {
            /* garbage bytes: the renderer must clamp, never crash */
            uint8_t *p = (uint8_t *)&r.look;
            for (k = 0; k < (int)sizeof(r.look); k++) p[k] = (uint8_t)rng_next(&rng);
        }
        memset(buf, CANARY, sizeof(buf));
        portrait_render(&r.look, status, expr, i * 7, buf + PAD);
        if (!canary_ok(buf, PORTRAIT_W * PORTRAIT_H)) bad++;
        memset(buf, CANARY, sizeof(buf));
        figure_render(&r.look, status, expr, i * 7, buf + PAD);
        if (!canary_ok(buf, FIGURE_W * FIGURE_H)) bad++;
        for (k = 0; k < 16; k++) {
            memset(buf, CANARY, sizeof(buf));
            bodyspr_render(&r.look, status, k >> 2, k & 3, buf + PAD);
            if (!canary_ok(buf, BODYSPR_W * BODYSPR_H)) bad++;
        }
        for (k = 0; k < 35; k++) {
            memset(buf, CANARY, sizeof(buf));
            fighter_render(&r.look, status, k % 5, k / 5, (i + k) % WFRAME_COUNT, buf + PAD);
            if (!canary_ok(buf, FIGHTER_W * FIGHTER_H)) bad++;
        }
    }
    CHECK_EQ(bad, 0);
}

TEST(portrait_is_deterministic_and_nonempty)
{
    static uint8_t a[FIGURE_W * FIGURE_H], b[FIGURE_W * FIGURE_H];
    int i, k, n;
    for (i = 0; i < 20; i++) {
        DoomoResident r;
        resident_generate(&r, 100u + (uint32_t)i);
        portrait_render(&r.look, RES_ALIVE, EXPR_NEUTRAL, 5, a);
        portrait_render(&r.look, RES_ALIVE, EXPR_NEUTRAL, 5, b);
        CHECK(memcmp(a, b, PORTRAIT_W * PORTRAIT_H) == 0);
        for (k = 0, n = 0; k < PORTRAIT_W * PORTRAIT_H; k++) n += a[k] != 0;
        CHECK_MSG(n > PORTRAIT_W * PORTRAIT_H / 3, "portrait %d is mostly empty (%d px)", i, n);
        bodyspr_render(&r.look, RES_ALIVE, 0, 0, a);
        for (k = 0, n = 0; k < BODYSPR_W * BODYSPR_H; k++) n += a[k] != 0;
        CHECK_MSG(n > 80, "chibi %d is mostly empty (%d px)", i, n);
    }
}

TEST(portrait_walk_cycle_and_fighter_views_differ)
{
    static uint8_t a[FIGHTER_W * FIGHTER_H];
    uint32_t h[35];
    DoomoLook l = base_look();
    int d, f, k, bad = 0;
    for (d = 0; d < 4; d++)
        for (f = 0; f < 4; f++) {
            bodyspr_render(&l, RES_ALIVE, d, f, a);
            h[d * 4 + f] = fnv(a, BODYSPR_W * BODYSPR_H, 2166136261u);
        }
    /* the four directions differ, and each walk cycle has motion: frames
     * 1 and 3 are opposite steps, 0 and 2 the in-between stance */
    for (d = 0; d < 4; d++) {
        for (k = 0; k < d; k++) if (h[d * 4] == h[k * 4]) { fprintf(stderr, "    dir %d == dir %d\n", d, k); bad++; }
        if (h[d * 4] == h[d * 4 + 1] || h[d * 4] == h[d * 4 + 3] || h[d * 4 + 1] == h[d * 4 + 3]) {
            fprintf(stderr, "    dir %d walk frames repeat\n", d);
            bad++;
        }
    }
    for (d = 0; d < 5; d++)
        for (f = 0; f < 7; f++) {
            fighter_render(&l, RES_ALIVE, d, f, WFRAME_ASSAULT_RIFLE, a);
            h[d * 7 + f] = fnv(a, FIGHTER_W * FIGHTER_H, 2166136261u);
            for (k = 0; k < d * 7 + f; k++)
                if (h[k] == h[d * 7 + f]) { fprintf(stderr, "    fighter %d/%d == %d/%d\n", d, f, k / 7, k % 7); bad++; }
        }
    CHECK_EQ(bad, 0);
}

TEST(portrait_fighter_weapons_differ)
{
    static uint8_t a[FIGHTER_W * FIGHTER_H];
    uint32_t h[WFRAME_COUNT];
    DoomoLook l = base_look();
    int w, k, bad = 0;
    for (w = 0; w < WFRAME_COUNT; w++) {
        fighter_render(&l, RES_ALIVE, 2, 0, w, a);
        h[w] = fnv(a, FIGHTER_W * FIGHTER_H, 2166136261u);
        for (k = 0; k < w; k++) if (h[k] == h[w]) { fprintf(stderr, "    weapon %d == %d\n", w, k); bad++; }
    }
    CHECK_EQ(bad, 0);
}

TEST(portrait_cache_keeps_uids_until_pixels_change)
{
    const DoomoImage *a, *b, *c;
    uint32_t uid;
    DoomoResident *r;
    int id;
    portrait_cache_reset();
    id = town_alloc(DISTRICT_HILLVILE);
    CHECK(id >= 0);
    r = town_res(id);
    resident_generate(r, 99);
    r->status = RES_ALIVE;
    a = portrait_image(id, EXPR_NEUTRAL);
    CHECK(a && a->w == PORTRAIT_W && a->h == PORTRAIT_H && a->pix);
    uid = a->uid;
    b = portrait_image(id, EXPR_NEUTRAL);
    CHECK(b == a && b->uid == uid);
    /* another expression is another image */
    c = portrait_image(id, EXPR_HAPPY);
    CHECK(c->uid != uid);
    /* the first one is still cached */
    b = portrait_image(id, EXPR_NEUTRAL);
    CHECK(b->uid == uid);
    /* editing the look changes the pixels, so the uid changes */
    r->look.hair_color = (uint8_t)((r->look.hair_color + 5) % PAL_PAINT_COUNT);
    b = portrait_image(id, EXPR_NEUTRAL);
    CHECK(b->uid != uid);
    uid = b->uid;
    portrait_invalidate(id);
    b = portrait_image(id, EXPR_NEUTRAL);
    CHECK(b->uid != uid);
    /* the other accessors */
    a = bodyspr_image(id, 1, 2);
    CHECK(a && a->w == BODYSPR_W && a->h == BODYSPR_H);
    uid = a->uid;
    CHECK(bodyspr_image(id, 1, 2)->uid == uid);
    CHECK(bodyspr_image(id, 1, 3)->uid != uid);
    a = fighter_image(id, 0, 4, WFRAME_SNIPER);
    CHECK(a && a->w == FIGHTER_W && a->h == FIGHTER_H);
    CHECK(fighter_image(id, 0, 4, WFRAME_SNIPER)->uid == a->uid);
    a = figure_image(&r->look, RES_ALIVE, EXPR_NEUTRAL);
    CHECK(a && a->w == FIGURE_W && a->h == FIGURE_H);
    /* empty slots and silly ids give a placeholder, never NULL */
    CHECK(portrait_image(-7, EXPR_NEUTRAL) != NULL);
    CHECK(portrait_image(5000, 99) != NULL);
    CHECK(bodyspr_image(DOOMO_PLAYER_ID, 9, 9) != NULL);
    CHECK(fighter_image(DOOMO_PLAYER_ID, 9, 9, 999) != NULL);
}

TEST(portrait_cache_holds_several_portraits)
{
    const DoomoImage *img[6];
    uint32_t uid[6];
    int ids[6], i;
    portrait_cache_reset();
    for (i = 0; i < 6; i++) {
        DoomoResident *r;
        ids[i] = town_alloc(i & 1 ? DISTRICT_HELLVILE : DISTRICT_HILLVILE);
        CHECK(ids[i] >= 0);
        r = town_res(ids[i]);
        resident_generate(r, 500u + (uint32_t)i);
        r->status = RES_ALIVE;
    }
    for (i = 0; i < 6; i++) { img[i] = portrait_image(ids[i], EXPR_NEUTRAL); uid[i] = img[i]->uid; }
    for (i = 0; i < 6; i++) CHECK(portrait_image(ids[i], EXPR_NEUTRAL)->uid == uid[i]);
    for (i = 1; i < 6; i++) CHECK(uid[i] != uid[i - 1]);
}

TEST(portrait_render_cost_is_bounded)
{
    static uint8_t buf[FIGURE_W * FIGURE_H];
    uint32_t worst = 0;
    int i;
    for (i = 0; i < 64; i++) {
        DoomoResident r;
        if (i & 1) resident_generate_hellborn(&r, 9000u + (uint32_t)i);
        else resident_generate(&r, 9000u + (uint32_t)i);
        portrait_render(&r.look, RES_ALIVE, EXPR_NEUTRAL, i, buf);
        if (portrait_last_ops() > worst) worst = portrait_last_ops();
        figure_render(&r.look, RES_ALIVE, EXPR_NEUTRAL, i, buf);
        if (portrait_last_ops() > worst) worst = portrait_last_ops();
    }
    /* pixel visits per render (each a handful of integer ops) */
    CHECK_MSG(worst < 12000, "worst render visited %u pixels", (unsigned)worst);
}
