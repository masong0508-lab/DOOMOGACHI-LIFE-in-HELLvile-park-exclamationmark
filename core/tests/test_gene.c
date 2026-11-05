/* test_gene.c - the 384-bit gene codec and its v1 bridge (customisation P0.M0a).
 * The layout checks pin docs/design/customization/SPEC.md 1.3; the bridge
 * checks prove that every one of the 1000 premade citizens and 4000
 * generated ones survives v1 -> gene -> v1 unchanged, i.e. that every v1
 * value fits its v2 width. Also compiles the look_pool.h and settings.h
 * layout asserts. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/gene.h"
#include "doomo/look_pool.h"
#include "doomo/settings.h"
#include "doomo/roster.h"

TEST(gene_layout_matches_the_spec)
{
    int i, end = 0, look = 0, ident = 0, fresh = 0;
    CHECK_EQ(GF_COUNT, 83);
    for (i = 0; i < GF_COUNT; i++) {
        const DoomoGeneField *f = &doomo_gene_field[i];
        CHECK_EQ(f->pos, end);                        /* contiguous, no gaps or overlaps */
        CHECK_EQ(f->max, (1u << f->bits) - 1u);
        end += f->bits;
        if (i <= GF_PATTERN_COLOR) look += f->bits;
        else if (i <= GF_CATCHPHRASE) ident += f->bits;
        else if (i < GF_SPARE) fresh += f->bits;
    }
    CHECK_EQ(end, DOOMO_GENE_BITS);
    CHECK_EQ(look, 225);                              /* legacy look, species excluded */
    CHECK_EQ(ident, 97);
    CHECK_EQ(fresh, 59);
    CHECK_EQ(doomo_gene_field[GF_SPARE].bits, 3);
    CHECK_EQ(doomo_gene_field[GF_LOOK_SEED].max, 2047);
    CHECK_EQ(doomo_gene_field[GF_HOOD_UP].pos, 379);
    CHECK_EQ(doomo_gene_field[GF_HANDEDNESS].pos, 380);
}

TEST(gene_get_set_round_trip_at_odd_offsets)
{
    uint8_t buf[DOOMO_GENE_BYTES + 3];
    uint32_t s = 12345u;
    int round, i;
    for (round = 0; round < 200; round++) {
        uint8_t *g = buf + 1 + (round & 1);            /* never word-aligned on purpose */
        uint32_t want[GF_COUNT];
        memset(buf, 0xA5, sizeof buf);
        memset(g, 0, DOOMO_GENE_BYTES);
        for (i = 0; i < GF_COUNT; i++) {
            s = s * 1664525u + 1013904223u;
            want[i] = (s >> 8) & doomo_gene_field[i].max;
            gene_set(g, i, want[i]);
        }
        for (i = 0; i < GF_COUNT; i++) CHECK_EQ(gene_get(g, i), want[i]);
        CHECK_EQ(buf[0], 0xA5);                        /* nothing written around the gene */
        CHECK_EQ(buf[1 + (round & 1) + DOOMO_GENE_BYTES], 0xA5);
    }
}

TEST(gene_set_touches_only_its_own_bits_and_clamps)
{
    uint8_t g[DOOMO_GENE_BYTES], h[DOOMO_GENE_BYTES];
    int i, j;
    for (i = 0; i < GF_COUNT; i++) {
        memset(g, 0xFF, sizeof g);
        memcpy(h, g, sizeof h);
        gene_set(g, i, 0);
        CHECK_EQ(gene_get(g, i), 0);
        for (j = 0; j < GF_COUNT; j++)
            if (j != i) CHECK_EQ(gene_get(g, j), gene_get(h, j));
        gene_set(g, i, 0xFFFFu);                       /* clamps to max */
        CHECK_EQ(gene_get(g, i), doomo_gene_field[i].max);
    }
}

/* r2 = r with every member the gene carries wiped, then unpacked */
static int bridge_ok(const DoomoResident *r)
{
    uint8_t g[DOOMO_GENE_BYTES];
    DoomoResident r2 = *r;
    int fit = gene_pack_v1(r, g);
    memset((uint8_t *)&r2.look + 1, 0x77, sizeof(DoomoLook) - 1);
    memset(r2.voice, 0x77, sizeof r2.voice);
    memset(r2.persona, 0x77, sizeof r2.persona);
    r2.pronoun = r2.bday_month = r2.bday_day = r2.like_food = r2.like_hobby =
        r2.like_music = r2.like_color = r2.hate_food = r2.fear = r2.catchphrase = 0x77;
    r2.romance = (uint8_t)(r->romance & LIFE_STAGE_MASK);
    gene_unpack_v1(g, &r2);
    return fit && memcmp(r, &r2, sizeof r2) == 0
        && gene_get(g, GF_SPARE) == 0 && gene_get(g, GF_LOOK_SEED) == 0
        && gene_get(g, GF_AGE) == 0 && gene_get(g, GF_HANDEDNESS) == 0;
}

TEST(gene_bridge_round_trips_all_1000_premade_citizens)
{
    int id, bad = 0;
    for (id = 0; id < ROSTER_COUNT; id++)
        if (!bridge_ok(roster_premade(id))) bad++;
    CHECK_EQ(bad, 0);
}

TEST(gene_bridge_round_trips_generated_citizens)
{
    uint32_t seed;
    int bad = 0, none = 0;
    for (seed = 1; seed <= 4000; seed++) {
        DoomoResident r;
        resident_generate(&r, seed * 2654435761u);
        if (r.look.hair_highlight == LOOK_NO_COLOR) none++;
        if (!bridge_ok(&r)) bad++;
    }
    CHECK_EQ(bad, 0);
    CHECK(none > 0);                                   /* the 0xFF sentinel was really exercised */
}

TEST(gene_pack_reports_a_value_that_does_not_fit)
{
    uint8_t g[DOOMO_GENE_BYTES];
    DoomoResident r = *roster_premade(0);
    CHECK(gene_pack_v1(&r, g));
    r.look.body = 16;                                  /* the gene's body field is 4 bits */
    CHECK(!gene_pack_v1(&r, g));
    CHECK_EQ(gene_get(g, GF_BODY), 15);                /* clamped, neighbours intact */
    r = *roster_premade(0);
    r.like_music = 16;                                 /* trimmed from 5 to 4 bits */
    CHECK(!gene_pack_v1(&r, g));
}

TEST(gene_pack_keeps_the_life_stage_out_of_romance)
{
    uint8_t g[DOOMO_GENE_BYTES];
    DoomoResident r = *roster_premade(0);
    r.romance = (uint8_t)((r.romance & ROMANCE_PREF_MASK) | (LIFE_CHILD << LIFE_STAGE_SHIFT));
    CHECK(gene_pack_v1(&r, g));
    CHECK_EQ(gene_get(g, GF_ROMANCE), r.romance & ROMANCE_PREF_MASK);
    gene_unpack_v1(g, &r);                             /* the stage bits stay as they were */
    CHECK_EQ(RES_STAGE(&r), LIFE_CHILD);
}
