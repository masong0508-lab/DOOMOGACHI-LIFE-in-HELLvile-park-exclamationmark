/* test_portrait_script.c - the part-script VM, materials, regions, stamps and look_key
 * (owner: portrait / customisation P1.M1). The demo scripts (core/data/parts/) sit on style values
 * no catalog list reaches, so these tests are the only thing that draws them yet. */
#include "test.h"
#include "../src/portrait_int.h"
#include "doomo/parts.h"
#include "doomo/gene.h"

static uint8_t buf[PORTRAIT_W * PORTRAIT_H + 16];     /* + canary */

static void setup(PCtx *c, DoomoLook *l, int yaw, int lod, int vk)
{
    DoomoResident r;
    resident_default(&r);
    *l = *RES_LOOK_VIEW(&r);
    memset(buf, 0xA5, sizeof(buf));
    pc_begin(buf, PORTRAIT_W, PORTRAIT_H);
    pctx_init(c, l, RES_ALIVE, EXPR_NEUTRAL, 0);
    phead_set(&c->h, PX(PORTRAIT_W / 2), PX(PORTRAIT_H / 2), PX(20), PX(24), yaw, lod);
    c->view_kind = vk;
    c->bx = PX(PORTRAIT_W / 2); c->by = PX(PORTRAIT_H * 3 / 4); c->bw = PX(14); c->bh = PX(14);
}

static int count(int x0, int x1)
{
    int x, y, n = 0;
    for (y = 0; y < PORTRAIT_H; y++)
        for (x = x0; x < x1; x++)
            if (pc_get(x, y)) n++;
    return n;
}

static uint32_t sum(void)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < PORTRAIT_W * PORTRAIT_H; i++) h = (h ^ buf[i]) * 16777619u;
    return h;
}

TEST(script_tables_find_entries)
{
    CHECK(ps_has(GF_EYE_STYLE, 63));
    CHECK(ps_has(GF_BLUSH_STYLE, 31));
    CHECK(ps_has(GF_HEAD_SHAPE, 31));       /* the head set */
    CHECK(!ps_has(GF_EYE_STYLE, 62));
    CHECK(!ps_has(GF_BLUSH_STYLE, 0));
    CHECK(!ps_has(GF_BODY, 0));
}

TEST(script_draws_deterministically_inside_the_canvas)
{
    PCtx c;
    DoomoLook l;
    uint32_t a;
    int ops, i;
    setup(&c, &l, 0, 2, VK_PORTRAIT);
    ops = ps_run(&c, GF_EYE_STYLE, 63, -1);
    CHECK(ops > 0);
    CHECK(count(0, PORTRAIT_W) > 0);
    a = sum();
    setup(&c, &l, 0, 2, VK_PORTRAIT);
    CHECK_EQ(ps_run(&c, GF_EYE_STYLE, 63, -1), ops);
    CHECK_EQ(sum(), a);
    for (i = 0; i < 16; i++) CHECK_EQ(buf[PORTRAIT_W * PORTRAIT_H + i], 0xA5);   /* canary */
    CHECK_EQ(ps_run(&c, GF_EYE_STYLE, 62, -1), 0);           /* no script, nothing drawn */
}

TEST(script_mirror_draws_both_sides_and_the_stamp_one)
{
    PCtx c;
    DoomoLook l;
    int left, right;
    setup(&c, &l, 0, 2, VK_PORTRAIT);
    ps_run_group(&c, GF_EYE_STYLE, 63, PA_HEAD, 3);
    left = count(0, PORTRAIT_W / 2);
    right = count(PORTRAIT_W / 2, PORTRAIT_W);
    CHECK(left > 20 && right > 20);                           /* both eyes */
    CHECK(left > right);                                      /* the heart is left only */
}

TEST(script_lod_view_and_anchor_gates)
{
    PCtx c;
    DoomoLook l;
    int big, chibi;
    setup(&c, &l, 0, 2, VK_PORTRAIT);
    big = ps_run(&c, GF_EYE_STYLE, 63, PA_HEAD);
    CHECK(big > 0);
    CHECK_EQ(ps_run(&c, GF_EYE_STYLE, 63, PA_TORSO), 0);      /* wrong anchor */
    CHECK_EQ(ps_run_group(&c, GF_EYE_STYLE, 63, -1, 2), 0);   /* wrong group */
    setup(&c, &l, 0, 0, VK_CHIBI);                            /* tiny + chibi: the dots only */
    chibi = ps_run(&c, GF_EYE_STYLE, 63, -1);
    CHECK(chibi > 0 && chibi < big);
    CHECK(count(0, PORTRAIT_W) > 0);
    setup(&c, &l, 0, 2, VK_CHIBI);                            /* big lod in the chibi view: neither */
    CHECK_EQ(ps_run(&c, GF_EYE_STYLE, 63, -1), 0);
}

TEST(script_conditions_follow_yaw)
{
    PCtx c;
    DoomoLook l;
    int front, side;
    setup(&c, &l, 0, 2, VK_PORTRAIT);
    pc_part(&c.skin, SH_FLAT, 0);
    pc_ellipse(PX(PORTRAIT_W / 2), PX(PORTRAIT_H / 2), PX(20), PX(24));
    front = ps_run(&c, GF_BLUSH_STYLE, 31, -1);
    setup(&c, &l, 2, 2, VK_PORTRAIT);                         /* left profile: ifyaw 0,1 skips */
    pc_part(&c.skin, SH_FLAT, 0);
    pc_ellipse(PX(PORTRAIT_W / 2), PX(PORTRAIT_H / 2), PX(20), PX(24));
    side = ps_run(&c, GF_BLUSH_STYLE, 31, -1);
    CHECK(front > side);
}

TEST(script_head_tuft_draws_in_its_group_only)
{
    PCtx c;
    DoomoLook l;
    setup(&c, &l, 0, 1, VK_FIGURE);
    CHECK_EQ(ps_run_group(&c, GF_HEAD_SHAPE, 31, -1, 3), 0);
    CHECK(ps_run_group(&c, GF_HEAD_SHAPE, 31, -1, 2) > 0);
    CHECK(count(0, PORTRAIT_W) > 4);
}

TEST(stamp_draws_the_bits_where_they_are)
{
    static const uint8_t checker[8] = { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 };
    PCtx c;
    DoomoLook l;
    PMat m;
    int x, y, bad = 0;
    setup(&c, &l, 0, 2, VK_PORTRAIT);
    pm_paint(&m, 3);
    CHECK(m.c[2] != 0);
    pc_stamp(&c, checker, 8, 8, PX(24), PX(24), 16, &m);       /* top left = (20, 20) */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            int want = (checker[y] >> (7 - x)) & 1;
            if ((pc_get(20 + x, 20 + y) != 0) != want) bad++;
        }
    CHECK_EQ(bad, 0);
    CHECK_EQ(pc_get(19, 20), 0);
    CHECK_EQ(pc_get(28, 20), 0);
    pc_stamp(&c, checker, 8, 8, PX(24), PX(24), 0, &m);        /* nonsense scale: ignored */
    pc_stamp(&c, NULL, 8, 8, PX(24), PX(24), 16, &m);
    pc_stamp(&c, checker, 40, 8, PX(24), PX(24), 16, &m);
}

TEST(regions_clip_to_their_part_of_the_face)
{
    PCtx c;
    DoomoLook l;
    int x, y, lmid = -1, rmid = -1, r;
    for (r = REG_HEAD; r < REG_COUNT; r++) {
        int minx = 999, maxx = -1, miny = 999, maxy = -1;
        setup(&c, &l, 0, 2, VK_PORTRAIT);
        pc_region(&c, r);
        pc_part(&c.skin, SH_FLAT, 0);
        pc_rect(0, 0, PX(PORTRAIT_W), PX(PORTRAIT_H));
        for (y = 0; y < PORTRAIT_H; y++)
            for (x = 0; x < PORTRAIT_W; x++)
                if (pc_get(x, y)) {
                    if (x < minx) minx = x;
                    if (x > maxx) maxx = x;
                    if (y < miny) miny = y;
                    if (y > maxy) maxy = y;
                }
        CHECK_MSG(maxx >= 0, "region %d drew nothing", r);
        CHECK_MSG(maxx - minx < PORTRAIT_W - 2 && maxy - miny < PORTRAIT_H - 2, "region %d is not a clip", r);
        if (r == REG_EYE_L) lmid = minx + maxx;
        if (r == REG_EYE_R) rmid = minx + maxx;
    }
    CHECK(lmid >= 0 && lmid < rmid);                           /* left eye box is left of the right */
    setup(&c, &l, 0, 2, VK_PORTRAIT);                          /* REG_NONE lifts the clip */
    pc_region(&c, REG_HEAD);
    pc_region(&c, REG_NONE);
    pc_part(&c.skin, SH_FLAT, 0);
    pc_rect(0, 0, PX(PORTRAIT_W), PX(PORTRAIT_H));
    CHECK_EQ(count(0, PORTRAIT_W), PORTRAIT_W * PORTRAIT_H);
}

TEST(pm_of_returns_today_materials_and_memoised_tones)
{
    PCtx c;
    DoomoLook l;
    const PMat *a, *b;
    int lf, tone, bad = 0;
    setup(&c, &l, 0, 2, VK_PORTRAIT);
    CHECK(pm_of(&c, GF_SKIN, PM_BASE) == &c.skin);
    CHECK(pm_of(&c, GF_HAIR_COLOR, PM_BASE) == &c.hair);
    CHECK(pm_of(&c, GF_EYE_COLOR, PM_BASE) == &c.eye);
    CHECK(pm_of(&c, GF_BODY, PM_BASE) == &c.skin);             /* not a colour field: skin */
    a = pm_of(&c, GF_SKIN, PM_LIGHT);
    CHECK_EQ(a->c[2], c.skin.c[3]);
    CHECK_EQ(a->c[0], c.skin.c[1]);
    b = pm_of(&c, GF_SKIN, PM_DARK);
    CHECK_EQ(b->c[2], c.skin.c[1]);
    CHECK(pm_of(&c, GF_SKIN, PM_LIGHT) == a);                  /* memo hit, same slot */
    for (lf = GF_SKIN; lf < GF_COUNT; lf++)                    /* more tones than the memo holds */
        for (tone = PM_LIGHT; tone <= PM_DARK; tone++) {
            const PMat *m = pm_of(&c, lf, tone), *base = pm_of(&c, lf, PM_BASE);
            if (m->c[2] != (tone == PM_LIGHT ? base->c[3] : base->c[1])) bad++;
        }
    CHECK_EQ(bad, 0);
}

TEST(look_key_follows_the_gene)
{
    CHECK(look_key(3) != 0);
    CHECK_EQ(look_key(3), look_key(3));
    CHECK(look_key(3) != look_key(4));
    CHECK_EQ(look_key(ROSTER_COUNT + 5), 0);
}

TEST(look_key_changes_when_the_gene_does)
{
    DoomoResident *r = town_res(3);
    DoomoResident keep;
    uint32_t k0, k1;
    CHECK(r != NULL);
    if (!r) return;
    keep = *r;
    roster_premade_record(3, r);                  /* a live record for slot 3 */
    k0 = look_key(3);
    r->gene[5] ^= 0x10;
    k1 = look_key(3);
    *r = keep;
    CHECK(k0 != 0 && k1 != k0);
}

TEST(anim_mask_lists_the_animated_looks)
{
    DoomoResident r;
    DoomoLook l;
    resident_default(&r);
    l = *RES_LOOK_VIEW(&r);
    l.aura = AURA_NONE; l.halo = HALO_NONE; l.cheeks = 0; l.hair_style = HAIR_BALD;
    l.species = SPECIES_HUMAN;
    CHECK_EQ(look_anim_mask(&l, RES_ALIVE, EXPR_NEUTRAL), 0u);
    CHECK_EQ(look_anim_mask(&l, RES_ALIVE, EXPR_TALK), (uint32_t)LAM_TALK);
    CHECK(look_anim_mask(&l, RES_ALIVE, EXPR_LOVE) & LAM_FX);
    CHECK(look_anim_mask(&l, RES_POSSESSED, EXPR_NEUTRAL) & LAM_FX);
    l.halo = HALO_STARS;
    CHECK(look_anim_mask(&l, RES_ALIVE, EXPR_NEUTRAL) & LAM_FX);
    l.halo = HALO_NONE; l.cheeks = CHEEK_SPARKLES;
    CHECK(look_anim_mask(&l, RES_ALIVE, EXPR_NEUTRAL) & LAM_FX);
}

TEST(head_only_draws_a_head_deterministically)
{
    PCtx c;
    DoomoLook l;
    uint32_t a;
    int n, i, lod;
    for (lod = LOD_TINY; lod <= LOD_BIG; lod++) {
        setup(&c, &l, 0, lod, VK_PORTRAIT);
        ph_head_only(&c, lod);
        n = count(0, PORTRAIT_W);
        CHECK(n > 100);
        a = sum();
        setup(&c, &l, 0, lod, VK_PORTRAIT);
        ph_head_only(&c, lod);
        CHECK_EQ(sum(), a);
        for (i = 0; i < 16; i++) CHECK_EQ(buf[PORTRAIT_W * PORTRAIT_H + i], 0xA5);
        CHECK_EQ(count(0, PORTRAIT_W), n);
        /* nothing below the neck line: the body is not drawn */
        CHECK_EQ(pc_get(PORTRAIT_W / 2, PORTRAIT_H - 1), 0);
    }
}

/* ---- the deep-field shim (look_fields.h / look_pool.c) -------------------------------------- */
#include "doomo/look_pool.h"

TEST(deep_resolve_reads_the_run_and_masks_for_chibis)
{
    static const uint8_t run[] = { DF_BLUSH2, 3, DF_SCLERA, 5, DF_BLUSH2, 4, DF_COUNT + 9, 7, DF_MOLE, 0x80 };
    DoomoDeep d;
    look_pool_drop(7);
    look_resolve(NULL, 7, &d);
    CHECK_EQ(d.v[DF_BLUSH2], 0);
    CHECK(look_pool_put(7, RUN_DEEP, run, (int)sizeof(run)));
    look_resolve(NULL, 7, &d);
    CHECK_EQ(d.v[DF_BLUSH2], 4);
    CHECK_EQ(d.v[DF_SCLERA], 5);
    CHECK_EQ(d.v[DF_MOLE], 0);
    look_resolve_mask(NULL, 7, LV_CHIBI, &d);
    CHECK_EQ(d.v[DF_BLUSH2], 4);
    CHECK_EQ(d.v[DF_SCLERA], 0);
    look_pool_drop(7);
    look_resolve(NULL, 7, &d);
    CHECK_EQ(d.v[DF_BLUSH2], 0);
}

TEST(deep_pool_is_bounded_and_replaces_runs)
{
    uint8_t big[200];
    int n = 0, id;
    memset(big, 0, sizeof(big));
    CHECK(!look_pool_put(-1, RUN_DEEP, big, 2));
    CHECK(!look_pool_put(5000, RUN_DEEP, big, 2));
    CHECK(!look_pool_put(8, RUN_DEEP, big, 300));
    for (id = 20; id < 30; id++) n += look_pool_put(id, RUN_DEEP, big, 200);
    CHECK(n >= 1 && n < 10);
    for (id = 20; id < 30; id++) look_pool_drop(id);
    CHECK(look_pool_find(20, RUN_DEEP, &n) == NULL);
    CHECK(look_pool_put(9, RUN_DEEP, (const uint8_t *)"\x02\x01", 2));
    CHECK(look_pool_put(9, RUN_DEEP, (const uint8_t *)"\x02\x02\x03\x01", 4));
    CHECK(look_pool_find(9, RUN_DEEP, &n) != NULL && n == 4);
    look_pool_drop(9);
}

TEST(deep_run_hash_is_zero_without_a_run_and_chibi_filtered)
{
    static const uint8_t a[] = { DF_SCLERA, 1 }, b[] = { DF_SCLERA, 2 };
    static const uint8_t c1[] = { DF_BLUSH2, 1 }, c2[] = { DF_BLUSH2, 2 };
    look_pool_drop(11);
    CHECK_EQ(look_run_hash(11, 0), 0u);
    CHECK(look_pool_put(11, RUN_DEEP, a, 2));
    {
        uint32_t full = look_run_hash(11, 0);
        CHECK(full != 0);
        CHECK_EQ(look_run_hash(11, 1), 0u);
        look_pool_put(11, RUN_DEEP, b, 2);
        CHECK(look_run_hash(11, 0) != full);
        CHECK_EQ(look_run_hash(11, 1), 0u);
    }
    look_pool_put(11, RUN_DEEP, c1, 2);
    {
        uint32_t k1 = look_run_hash(11, 1);
        CHECK(k1 != 0);
        look_pool_put(11, RUN_DEEP, c2, 2);
        CHECK(look_run_hash(11, 1) != k1);
    }
    look_pool_drop(11);
}

TEST(deep_anim_mask_and_cache_stay_correct_with_a_run)
{
    static const uint8_t aura[] = { DF_AURA2, 1 };
    DoomoDeep d = doomo_deep_zero;
    const DoomoImage *i0, *i1;
    uint32_t k0;
    CHECK_EQ(deep_anim_mask(&d), 0u);
    d.v[DF_AURA2] = 1;
    CHECK(deep_anim_mask(&d) & LAM_FX);
    d.v[DF_AURA2] = 0; d.v[DF_BLUSH2] = 2;
    CHECK_EQ(deep_anim_mask(&d), 0u);
    portrait_cache_reset();
    look_pool_drop(3);
    i0 = portrait_image(3, EXPR_NEUTRAL);
    CHECK(i0 != NULL);
    k0 = look_key(3);
    look_pool_put(3, RUN_DEEP, aura, 2);
    CHECK(look_key(3) != k0);
    i1 = portrait_image(3, EXPR_NEUTRAL);
    CHECK(i1 != NULL);
    look_pool_drop(3);
    portrait_cache_reset();
}

TEST(deep_anim_field_asks_for_effect_frames)
{
    static const uint8_t aura[] = { DF_AURA2, 1 }, blush[] = { DF_BLUSH2, 1 };
    DoomoLook l;
    memset(&l, 0, sizeof(l));
    look_pool_drop(4);
    CHECK_EQ(look_anim_mask_rid(&l, 4, RES_ALIVE, EXPR_NEUTRAL), look_anim_mask(&l, RES_ALIVE, EXPR_NEUTRAL));
    CHECK(look_pool_put(4, RUN_DEEP, blush, 2));
    CHECK(!(look_anim_mask_rid(&l, 4, RES_ALIVE, EXPR_NEUTRAL) & LAM_FX));   /* not animated */
    CHECK(look_pool_put(4, RUN_DEEP, aura, 2));
    CHECK(look_anim_mask_rid(&l, 4, RES_ALIVE, EXPR_NEUTRAL) & LAM_FX);
    CHECK(!(look_anim_mask_rid(&l, 5, RES_ALIVE, EXPR_NEUTRAL) & LAM_FX));   /* other ids untouched */
    look_pool_drop(4);
}

TEST(cache_hits_equal_a_fresh_render_with_and_without_a_run)
{
    static const uint8_t run[] = { DF_BLUSH2, 2, DF_AURA2, 1, DF_SCLERA, 3 };
    int id, i;
    uint32_t n0, bad0;
    portrait_cache_reset();
    n0 = portrait_hitcheck_count();
    bad0 = portrait_hitcheck_fails();
    for (id = 1; id < 7; id++) {
        look_pool_drop(id);
        if (id & 1) look_pool_put(id, RUN_DEEP, run, (int)sizeof(run));
        for (i = 0; i < 4; i++) {
            CHECK(portrait_image(id, EXPR_NEUTRAL) != NULL);
            CHECK(portrait_image(id, EXPR_HAPPY) != NULL);
        }
    }
    CHECK(portrait_hitcheck_count() > n0);                         /* the check ran on hits */
    CHECK_EQ(portrait_hitcheck_fails(), bad0);                     /* and none differed */
    for (id = 1; id < 7; id++) look_pool_drop(id);
    portrait_cache_reset();
}

/* ---- the PS1 LZ part blocks behind the 4-slot LRU (part_lru.c) --------------------------------- */
#if defined(DOOMO_PARTS_LZ_TEST)
/* literal-only runs of at most 128 bytes, then one copy: the save codec by hand (no encoder in C) */
static int lz_make(const uint8_t *raw, int n, uint8_t *out)
{
    int o = 0, i = 0;
    while (i < n) {
        int k = n - i > 128 ? 128 : n - i;
        out[o++] = (uint8_t)(k - 1);
        memcpy(out + o, raw + i, (size_t)k);
        o += k; i += k;
    }
    return o;
}

#if !defined(DOOMO_PARTS_LZ)       /* the raw arrays are not linked in the LZ build */
TEST(lz_blocks_of_the_real_sets_equal_the_raw_scripts)
{
    const PartLz *zs[2];
    const PartSet *ss[2];
    int s;
    zs[0] = &doomo_parts_face_lz; ss[0] = &doomo_parts_face;
    zs[1] = &doomo_parts_head_lz; ss[1] = &doomo_parts_head;
    for (s = 0; s < 2; s++) {
        const PartLz *z = zs[s];
        unsigned b;
        part_lru_reset();
        CHECK(z->nblk >= 1);
        for (b = 0; b < z->nblk; b++) {
            unsigned n = (unsigned)(z->rofs[b + 1] - z->rofs[b]);
            CHECK(n <= PART_BLK);
            CHECK(memcmp(part_lz_ops(z, z->rofs[b]), ss[s]->ops + z->rofs[b], n) == 0);
        }
    }
}

#endif

TEST(lz_lru_keeps_four_blocks_and_evicts_the_oldest)
{
    static uint8_t raw[6][700], z[6][1000], big[6 * 700 + 4];
    static uint16_t zofs[7], rofs[7];
    static uint8_t zbuf[6000];
    PartLz pl;
    int b, i, o = 0, zl = 0;
    for (b = 0; b < 6; b++) {
        for (i = 0; i < 700; i++) raw[b][i] = (uint8_t)(b * 40 + (i * 7) % 31);
        rofs[b] = (uint16_t)(b * 700);
        zofs[b] = (uint16_t)zl;
        o = lz_make(raw[b], 650, z[b]);
        /* the last 50 bytes: a copy of 50 from 100 back is not what raw[] holds, so make raw match it */
        z[b][o++] = (uint8_t)(0x80 | (50 - 3)); z[b][o++] = 100; z[b][o++] = 0;
        for (i = 650; i < 700; i++) raw[b][i] = raw[b][i - 100];
        memcpy(zbuf + zl, z[b], (size_t)o);
        zl += o;
        memcpy(big + b * 700, raw[b], 700);
    }
    rofs[6] = 4200; zofs[6] = (uint16_t)zl;
    pl.z = zbuf; pl.zofs = zofs; pl.rofs = rofs; pl.nblk = 6;
    part_lru_reset();
    for (b = 0; b < 4; b++) CHECK(memcmp(part_lz_ops(&pl, (unsigned)(b * 700 + 5)), raw[b] + 5, 695) == 0);
    CHECK_EQ(part_lru_loads(), 4u);
    CHECK(memcmp(part_lz_ops(&pl, 3), raw[0] + 3, 697) == 0);        /* a hit */
    CHECK_EQ(part_lru_loads(), 4u);
    CHECK(memcmp(part_lz_ops(&pl, 4 * 700), raw[4], 700) == 0);      /* evicts block 1 (oldest) */
    CHECK_EQ(part_lru_loads(), 5u);
    CHECK(memcmp(part_lz_ops(&pl, 0), raw[0], 700) == 0);            /* block 0 survived */
    CHECK_EQ(part_lru_loads(), 5u);
    CHECK(memcmp(part_lz_ops(&pl, 700), raw[1], 700) == 0);          /* block 1 was evicted: reload */
    CHECK_EQ(part_lru_loads(), 6u);
    CHECK(memcmp(part_lz_ops(&pl, 5 * 700 + 699), raw[5] + 699, 1) == 0);
    part_lru_reset();
}
#endif

/* ---- wave-2 face fields: pupil, sclera, blush (portrait_head.c) ---------------------------- */
static int px_diff(const uint8_t *a, const uint8_t *b)
{
    int i, n = 0;
    for (i = 0; i < PORTRAIT_W * PORTRAIT_H; i++) n += a[i] != b[i];
    return n;
}

static void deep_bust(const DoomoLook *l, int f, int v, uint8_t *out)
{
    DoomoDeep d = doomo_deep_zero;
    d.v[f] = (uint8_t)v;
    portrait_render_deep(l, &d, RES_ALIVE, EXPR_NEUTRAL, 0, out);
}

TEST(wave2_face_choices_zero_is_today_and_every_choice_is_distinct)
{
    static const struct { int f, n; } fld[] = { { DF_PUPIL, 4 }, { DF_SCLERA, 2 }, { DF_BLUSH2, 4 } };
    static uint8_t base[PORTRAIT_W * PORTRAIT_H], a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    DoomoLook l;
    DoomoDeep z = doomo_deep_zero;
    int k, v, w;
    memset(&l, 0, sizeof(l));
    l.eye_style = EYE_ROUND; l.eye_size = 12;                      /* a glossy eye big enough to show a pupil */
    portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, base);
    portrait_render_deep(&l, &z, RES_ALIVE, EXPR_NEUTRAL, 0, a);
    CHECK_EQ(px_diff(base, a), 0);                                 /* an all-zero set is today's picture */
    for (k = 0; k < 3; k++) {
        for (v = 1; v <= fld[k].n; v++) {
            deep_bust(&l, fld[k].f, v, a);
            CHECK(px_diff(base, a) > 0);                           /* each choice draws something */
            for (w = v + 1; w <= fld[k].n; w++) {
                deep_bust(&l, fld[k].f, w, b);
                CHECK(px_diff(a, b) > 0);                          /* and none repeats another */
            }
        }
    }
}

TEST(wave2_ear_fold_and_glasses_are_distinct_and_zero_is_today)
{
    static uint8_t base[PORTRAIT_W * PORTRAIT_H], a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    DoomoLook l;
    int v, w;
    memset(&l, 0, sizeof(l));
    l.eye_style = EYE_ROUND; l.eye_size = 12; l.ears = EARS_POINTY;
    portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, base);
    for (v = 1; v <= 7; v++) {                                     /* ear fold: 7 choices */
        deep_bust(&l, DF_EAR_FOLD, v, a);
        CHECK(px_diff(base, a) > 0);
        for (w = v + 1; w <= 7; w++) {
            deep_bust(&l, DF_EAR_FOLD, w, b);
            CHECK(px_diff(a, b) > 0);
        }
    }
    for (v = 1; v <= 4; v++) {                                     /* glasses: 4 styles */
        deep_bust(&l, DF_GLASSES2, v, a);
        CHECK(px_diff(base, a) > 0);
        for (w = v + 1; w <= 4; w++) {
            deep_bust(&l, DF_GLASSES2, w, b);
            CHECK(px_diff(a, b) > 0);
        }
    }
    l.accessory = ACC_GLASSES;                                     /* the accessory slot's glasses win */
    portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, base);
    deep_bust(&l, DF_GLASSES2, 2, a);
    CHECK_EQ(px_diff(base, a), 0);
}

TEST(wave2_hood_head_is_distinct_needs_a_hood_and_no_hat)
{
    static uint8_t base[PORTRAIT_W * PORTRAIT_H], a[PORTRAIT_W * PORTRAIT_H], b[PORTRAIT_W * PORTRAIT_H];
    DoomoLook l;
    int v, w;
    memset(&l, 0, sizeof(l));
    l.eye_style = EYE_ROUND; l.eye_size = 12;
    l.top = TOP_TSHIRT;                                            /* no hood: the choice draws nothing */
    portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, base);
    deep_bust(&l, DF_HOOD_HEAD, 2, a);
    CHECK_EQ(px_diff(base, a), 0);
    l.top = TOP_HOODIE; l.top_color = PAINT_BUBBLEGUM;
    portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, base);
    for (v = 1; v <= 3; v++) {
        deep_bust(&l, DF_HOOD_HEAD, v, a);
        CHECK(px_diff(base, a) > 0);
        for (w = v + 1; w <= 3; w++) {
            deep_bust(&l, DF_HOOD_HEAD, w, b);
            CHECK(px_diff(a, b) > 0);
        }
    }
    l.hat = HAT_BEANIE;                                            /* a hat takes the head */
    portrait_render(&l, RES_ALIVE, EXPR_NEUTRAL, 0, base);
    deep_bust(&l, DF_HOOD_HEAD, 2, a);
    CHECK_EQ(px_diff(base, a), 0);
}
