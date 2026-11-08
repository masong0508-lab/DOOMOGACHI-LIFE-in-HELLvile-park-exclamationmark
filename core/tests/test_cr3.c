/* test_cr3.c - CR3: the gene's lists grown to their live counts and the gene's new look fields made
 * selectable, with no save format change (docs/CREATOR.md "More parts (CR3/CR4)").
 *
 * - every catalogued gene list's live values are 0..N-1, at least its v1 enum, and what this build
 *   draws (part_live_drawable) never exceeds it;
 * - resident_sanitize keeps every live choice (and the classic generators still roll v1 values only);
 * - the gene's new fields (pupil .. handedness) and the high bit of aura / print / eye count survive a
 *   save: the v1 stream cannot hold them, the creator's NICK chunk carries them as a trailer;
 * - they reach the renderer: a resident's portrait / figure / chibi / fighter change with them, and the
 *   cached image of a look you hold changes with the set you pass (figure_image_d);
 * - the creator's rows edit them, its preview shows them, and they are written into the town. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/part_meta.h"
#include "doomo/look_fields.h"
#include "doomo/portrait.h"
#include "doomo/save.h"
#include "../src/creator_int.h"

TEST(cr3_live_counts_cover_the_classic_lists)
{
    static const struct { int gf, classic; } l[] = {
        { GF_BODY, BODY_COUNT }, { GF_HEAD_SHAPE, HEAD_COUNT }, { GF_EYE_STYLE, EYE_COUNT },
        { GF_BROW_STYLE, BROW_COUNT }, { GF_NOSE_STYLE, NOSE_COUNT }, { GF_MOUTH_STYLE, MOUTH_COUNT },
        { GF_HAIR_STYLE, HAIR_COUNT }, { GF_FACIAL_HAIR, FHAIR_COUNT }, { GF_EARS, EARS_COUNT },
        { GF_HORNS, HORNS_COUNT }, { GF_TAIL, TAIL_COUNT }, { GF_WINGS, WINGS_COUNT },
        { GF_HALO, HALO_ALL_COUNT }, { GF_MARKING, MARK_COUNT }, { GF_TOP, TOP_COUNT },
        { GF_BOTTOM, BOTTOM_COUNT }, { GF_SHOES, SHOES_COUNT }, { GF_HAT, HAT_COUNT },
        { GF_ACCESSORY, ACC_COUNT }, { GF_AURA, AURA_COUNT }, { GF_PATTERN, PATTERN_COUNT },
        { GF_PUPIL, 5 }, { GF_SCLERA, 3 }, { GF_GLASSES, 5 }, { GF_BLUSH_STYLE, 5 }, { GF_EAR_FOLD, 8 },
    };
    int i, v;
    for (i = 0; i < (int)DOOMO_COUNTOF(l); i++) {
        int n = part_live_count(l[i].gf);
        CHECK_MSG(n >= l[i].classic, "field %d: %d live < %d", l[i].gf, n, l[i].classic);
        CHECK(n <= (int)doomo_gene_field[l[i].gf].max + 1);
        CHECK(part_live_drawable(l[i].gf) <= n && part_live_drawable(l[i].gf) >= 1);
        for (v = 0; v < n; v++) {
            const PartMeta *m = part_meta_find(l[i].gf, v);
            CHECK_MSG(m && PMF_STATE(m->flags) == PSTATE_LIVE, "field %d value %d not live", l[i].gf, v);
        }
    }
    CHECK_EQ(part_live_count(GF_LIKE_FOOD), 0);
}

TEST(cr3_sanitize_keeps_every_live_choice)
{
    DoomoResident r;
    int i;
    resident_default(&r);
    RES_SET_LOOK(&r, GF_HALO, part_live_count(GF_HALO) - 1);
    RES_SET_LOOK(&r, GF_HAT, part_live_count(GF_HAT) - 1);
    res_gset(&r, GF_PUPIL, (uint32_t)part_live_count(GF_PUPIL) - 1);
    res_gset(&r, GF_GLASSES, (uint32_t)part_live_count(GF_GLASSES) - 1);
    res_gset(&r, GF_EAR_FOLD, (uint32_t)part_live_count(GF_EAR_FOLD) - 1);
    res_gset(&r, GF_FUR_COLOR3, 7);
    res_gset(&r, GF_HOOD_UP, 1);
    res_gset(&r, GF_HANDEDNESS, 1);
    CHECK_EQ(resident_sanitize(&r), 0);
    /* a value past the live count is put back */
    if (part_live_count(GF_GLASSES) < 32) {
        res_gset(&r, GF_GLASSES, 31);
        CHECK(resident_sanitize(&r) == 1 && res_gget(&r, GF_GLASSES) == 0);
    }
    res_gset(&r, GF_FUR_COLOR3, 60);
    CHECK(resident_sanitize(&r) == 1 && res_gget(&r, GF_FUR_COLOR3) == 0);
    /* the classic generators never roll a new field */
    for (i = 0; i < 200; i++) {
        if (i & 1) resident_generate_hellborn(&r, 5000u + (uint32_t)i); else resident_generate(&r, 5000u + (uint32_t)i);
        CHECK(res_gget(&r, GF_PUPIL) == 0 && res_gget(&r, GF_GLASSES) == 0 && res_gget(&r, GF_OUTER) == 0 &&
              res_gget(&r, GF_LEGWEAR) == 0 && res_gget(&r, GF_FUR_PATTERN) == 0);
        CHECK(RES_LOOK(&r, GF_HAT) < HAT_COUNT && RES_LOOK(&r, GF_HALO) < HALO_COUNT);
    }
}

static void set_extras(DoomoResident *r, int k)
{
    res_gset(r, GF_PUPIL, (uint32_t)(1 + k % (part_live_count(GF_PUPIL) - 1)));
    res_gset(r, GF_SCLERA, (uint32_t)(1 + k % (part_live_count(GF_SCLERA) - 1)));
    res_gset(r, GF_GLASSES, (uint32_t)(1 + k % (part_live_count(GF_GLASSES) - 1)));
    res_gset(r, GF_BLUSH_STYLE, (uint32_t)(1 + k % (part_live_count(GF_BLUSH_STYLE) - 1)));
    res_gset(r, GF_EAR_FOLD, (uint32_t)(1 + k % (part_live_count(GF_EAR_FOLD) - 1)));
    res_gset(r, GF_FUR_COLOR3, (uint32_t)(1 + k % 32));
    res_gset(r, GF_HOOD_UP, (uint32_t)(k & 1));
    res_gset(r, GF_HANDEDNESS, (uint32_t)((k >> 1) & 1));
}

TEST(cr3_gene_fields_survive_a_save)
{
    static uint8_t img[DOOMO_SAVE_SLOT1_MAX];
    static uint8_t want[8][DOOMO_GENE_BYTES];
    static const int ids[8] = { 0, 3, 77, 499, 500, 812, 999, DOOMO_PLAYER_ID };
    uint8_t chunk[128];
    uint32_t n;
    int i;
    town_new(TOWN_FREEPLAY, 4242u, TOWN_POP_PREMADE);
    for (i = 0; i < 8; i++) {
        DoomoResident *r = town_res(ids[i]);
        CHECK(r != NULL);
        if (r->status == RES_EMPTY) r->status = RES_ALIVE;
        set_extras(r, i + 1);
        resident_sanitize(r);                                  /* (the empty player record gets a name) */
        CHECK_EQ(resident_sanitize(r), 0);
        memcpy(want[i], r->gene, DOOMO_GENE_BYTES);
    }
    /* the trailer alone: 2 + 8 per record */
    CHECK_EQ(look_gene_ext_size(), 2u + 8u * 8u);
    CHECK_EQ(look_gene_ext_write(chunk, sizeof(chunk)), 2u + 8u * 8u);
    CHECK_EQ(look_gene_ext_write(chunk, 10), 0u);                     /* no room: nothing */
    n = save_serialize(img, sizeof(img));
    CHECK(n > 0);
    for (i = 0; i < 8; i++) { DoomoResident *r = town_res(ids[i]); memset(r->gene + 30, 0, 18); }
    CHECK(save_deserialize(img, n));
    for (i = 0; i < 8; i++) {
        const DoomoResident *r = town_res(ids[i]);
        CHECK_MSG(memcmp(r->gene, want[i], DOOMO_GENE_BYTES) == 0, "resident %d lost its new fields", ids[i]);
    }
    /* a town nobody customised has no trailer */
    town_new(TOWN_FREEPLAY, 4242u, TOWN_POP_PREMADE);
    CHECK_EQ(look_gene_ext_size(), 0u);
    /* a malformed trailer (an empty slot) is refused but what it can apply is applied */
    {
        uint8_t bad[2 + 8] = { 1, 0, 0xFF, 0x03, 1, 0, 0, 0, 0, 0 };   /* id 1023: no such record */
        CHECK(!look_gene_ext_read(bad, sizeof(bad)));
        CHECK(look_gene_ext_read(bad, 0));
    }
}

TEST(cr3_gene_fields_reach_the_renderer)
{
    static uint8_t a[FIGURE_W * FIGURE_H], b[FIGURE_W * FIGURE_H];
    DoomoResident *r;
    DoomoLook l;
    DoomoDeep d = doomo_deep_zero;
    const DoomoImage *i0, *i1;
    uint32_t u0;
    town_new(TOWN_FREEPLAY, 99u, TOWN_POP_PREMADE);
    portrait_cache_reset();
    r = town_res(12);
    res_gset(r, GF_GLASSES, 0);
    portrait_invalidate(12);
    i0 = portrait_image(12, EXPR_NEUTRAL);
    CHECK(i0 != NULL);
    u0 = i0->uid;
    memcpy(a, i0->pix, PORTRAIT_W * PORTRAIT_H);
    res_gset(r, GF_GLASSES, 2);                                       /* no invalidate: the key sees it */
    i1 = portrait_image(12, EXPR_NEUTRAL);
    CHECK(i1 != NULL && i1->uid != u0);
    CHECK(memcmp(a, i1->pix, PORTRAIT_W * PORTRAIT_H) != 0);
    /* a look you hold: the set you pass */
    res_get_look(r, &l);
    i0 = figure_image_d(&l, &d, RES_ALIVE, EXPR_NEUTRAL);
    memcpy(a, i0->pix, FIGURE_W * FIGURE_H);
    look_part_set(&l, &d, GF_GLASSES, 3);
    i1 = figure_image_d(&l, &d, RES_ALIVE, EXPR_NEUTRAL);
    memcpy(b, i1->pix, FIGURE_W * FIGURE_H);
    CHECK(memcmp(a, b, FIGURE_W * FIGURE_H) != 0);
    /* the record form draws the same as the look + its set */
    res_gset(r, GF_GLASSES, 3);
    i0 = figure_image_res(r, RES_ALIVE, EXPR_NEUTRAL);
    CHECK(memcmp(i0->pix, b, FIGURE_W * FIGURE_H) == 0);
    /* a set of zeroes keeps a Classic look's cache key */
    CHECK_EQ(deep_hash(&doomo_deep_zero, 0), 0u);
    portrait_cache_reset();
}

static int find_row(int tab, int field)
{
    int i;
    for (i = 0; i < cr_tabs[tab].nrows; i++)
        if (cr_tabs[tab].rows[i].field == field && cr_tabs[tab].rows[i].kind != RK_HEADER) return i;
    return -1;
}

TEST(cr3_creator_rows_edit_the_new_fields)
{
    static const struct { int tab, field; } rows[] = {
        { CR_TAB_FACE, GF_PUPIL }, { CR_TAB_FACE, GF_SCLERA }, { CR_TAB_FACE, GF_EAR_FOLD },
        { CR_TAB_FACE, GF_BLUSH_STYLE }, { CR_TAB_BODY, GF_FUR_PATTERN }, { CR_TAB_BODY, GF_FUR_COLOR3 },
        { CR_TAB_OUTFIT, GF_GLASSES }, { CR_TAB_OUTFIT, GF_OUTER }, { CR_TAB_OUTFIT, GF_LEGWEAR },
        { CR_TAB_OUTFIT, GF_HOOD_UP }, { CR_TAB_PERSONA, GF_HANDEDNESS },
    };
    DoomoResident r;
    int i;
    resident_default(&r);
    for (i = 0; i < (int)DOOMO_COUNTOF(rows); i++) {
        int k = find_row(rows[i].tab, rows[i].field), n;
        const CrRow *row;
        CHECK_MSG(k >= 0, "no row for field %d", rows[i].field);
        if (k < 0) continue;
        row = &cr_tabs[rows[i].tab].rows[k];
        n = cr_row_count(row);
        if (rows[i].field == GF_OUTER || rows[i].field == GF_LEGWEAR || rows[i].field == GF_FUR_PATTERN)
            CHECK(n >= 1);                                  /* (their parts come with CR4) */
        else
            CHECK_MSG(n >= 2, "field %d: %d choices", rows[i].field, n);
        cr_row_set(&r, row, n - 1);
        CHECK_EQ(cr_row_value(&r, row), n - 1);
        CHECK(cr_row_help(row)[0] != 0);
        if (row->kind == RK_ENUM && n > 1) {
            char buf[48];
            CHECK(cr_choice_name(row, n - 1, buf)[0] != 0);
        }
    }
    /* the third fur colour: Auto is stored as 0, a paint p as p + 1 */
    {
        const CrRow *row = &cr_tabs[CR_TAB_BODY].rows[find_row(CR_TAB_BODY, GF_FUR_COLOR3)];
        cr_row_set(&r, row, 0);
        CHECK_EQ(res_gget(&r, GF_FUR_COLOR3), 0u);
        cr_row_set(&r, row, 5);
        CHECK_EQ(res_gget(&r, GF_FUR_COLOR3), 5u);
    }
    CHECK_EQ(resident_sanitize(&r), 0);
}
