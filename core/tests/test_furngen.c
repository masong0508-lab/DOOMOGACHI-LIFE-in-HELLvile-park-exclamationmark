/* test_furngen.c - the furniture generator and its output (owner: H4; SPEC
 * H4 acceptance, HF sec.6.2, 11.1).
 *
 * The tool's parser and baker are compiled in (FURNGEN_LIB drops its main),
 * so the checks run on the real code: unmatched keys fail, marker paints
 * are respected, and the generated tables obey the ROM budget (art total
 * <= 0.8 MB, average <= 2.1 KB per piece). `make -C core check-gen` checks
 * that core/gen/furn_*.c match the inputs byte for byte.
 */
#define FURNGEN_LIB 1
#include "../tools/furngen.c"
#include "test.h"
#include "../src/home_int.h"

static int load_real(void)
{
    char path[700];
    char *text;
    int k, e = 0;
    FILE *f;
    long n;
    static const char *const files[] = { "catalog", "art/pastel", "art/story", "art/gothic", "art/hellish",
                                         "art/plush", "art/kitchen", "art/tech", "art/garden", "art/light",
                                         "art/wall", "art/rug", "art/trophy", "art/seasonal", NULL };
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    for (k = 0; files[k]; k++) {
        snprintf(path, sizeof(path), "%s/../../data/furn/%s.txt", test_golden_dir, files[k]);
        f = fopen(path, "rb");
        if (!f) { if (k == 0) return -1; continue; }
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        text = (char *)malloc((size_t)n + 1);
        if (fread(text, 1, (size_t)n, f) != (size_t)n) n = 0;
        text[n] = 0;
        fclose(f);
        e += k == 0 ? parse_catalog(text, path) : parse_art(text, path);
        free(text);
    }
    return e;
}

TEST(furngen_rejects_unmatched_keys)
{
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    CHECK_EQ(parse_catalog("[a_chair] PASTEL 10 \"A Chair\"\n  foot 1 1 4\n", "t"), 0);
    CHECK_EQ(nrows, 1);
    CHECK(parse_art("[a_chair]\nbox 2 2 0 12 12 8 A\n", "t") == 0);
    CHECK(parse_art("[no_such_piece]\nbox 0 0 0 4 4 4 A\n", "t") > 0);
    CHECK(parse_art("[a_chair]\nbox 0 0 0 4 4 4 A\n", "t") > 0);       /* twice */
    CHECK(parse_catalog("[x] PASTEL 1 \"X\"\n[x] PASTEL 1 \"Y\"\n", "t") > 0);   /* duplicate */
    CHECK(parse_catalog("[x] NOPE 1 \"X\"\n", "t") > 0);                     /* category */
    CHECK(parse_catalog("[x] PASTEL 1 \"X\"\n  frobnicate 3\n", "t") > 0);   /* attribute */
}

TEST(furngen_rejects_marker_paints_as_fixed)
{
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    CHECK_EQ(parse_catalog("[a_chair] PASTEL 10 \"A Chair\"\n", "t"), 0);
    CHECK(parse_art("[a_chair]\nbox 2 2 0 12 12 8 BUBBLEGUM\n", "t") > 0);
    memset(arts, 0, sizeof(arts));
    CHECK(parse_art("[a_chair]\nbox 2 2 0 12 12 8 MINT\n", "t") > 0);
    memset(arts, 0, sizeof(arts));
    CHECK(parse_art("[a_chair]\nbox 2 2 0 12 12 8 CHERRY\n", "t") == 0);
}

/* A baked fixed-colour piece never uses the 8 recolour indices; a painted
 * one uses them only for its painted prims. */
TEST(furngen_marker_paints_respected)
{
    Img im;
    int i, fixed_ok = 1, painted = 0;
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    CHECK_EQ(parse_catalog("[f] PASTEL 1 \"F\"\n  foot 1 1 4\n[p] PASTEL 1 \"P\"\n  foot 1 1 4\n", "t"), 0);
    CHECK_EQ(parse_art("[f]\nbox 2 2 0 12 12 10 CHERRY\nblob 8 8 14 4 4 4 ~WOOD soft\n"
                       "[p]\nbox 2 2 0 12 12 10 A\nblob 8 8 14 4 4 4 B shiny\n", "t"), 0);
    bake(0, 0, 0, &im);
    for (i = 0; i < im.w * im.h; i++) if (im.pix[i] && reserved(im.pix[i])) fixed_ok = 0;
    free(im.pix);
    CHECK(fixed_ok);
    bake(1, 1, 0, &im);
    for (i = 0; i < im.w * im.h; i++) if (im.pix[i] && reserved(im.pix[i])) painted++;
    free(im.pix);
    CHECK(painted > im.w);           /* the paint slots are recolourable */
}

TEST(furngen_m1_art_set)
{
    int i, with_art = 0, e = load_real();
    CHECK_MSG(e == 0, "the real catalog / art failed to parse (%d errors: %s)", e, fg_lasterr);
    CHECK_EQ(nrows, doomo_furn_count);
    for (i = 0; i < nrows; i++) if (arts[i].has) with_art++;
    CHECK_MSG(with_art >= 60, "only %d pieces have art (M1: 60)", with_art);
    for (i = 0; i < nrows; i++)                  /* the starter kit is drawn */
        if (rows[i].flags2 & FF_STARTER) CHECK_MSG(arts[i].has, "%s has no art", rows[i].key);
    {                                            /* so are the fixed slots */
        static const int roles[] = { FR_DOOR, FR_WINDOW, FR_HEARTH };
        for (i = 0; i < 3; i++) {
            int f = home_furn_by_role(roles[i]);
            CHECK(f >= 0 && f < nrows);
            CHECK_MSG(arts[f].has, "%s has no art", rows[f].key);
        }
    }
}

static int facing_bytes(const DoomoFurnIso *im)
{
    const uint8_t *b = doomo_furn_iso_data + im->off;
    int y, n = im->h * 2;
    for (y = 0; y < im->h; y++) {
        const uint8_t *p = b + (b[y * 2] | (b[y * 2 + 1] << 8));
        const uint8_t *s = p;
        while (*p != 0xFF) p += 2 + p[1];
        n += (int)(p - s) + 1;
    }
    return n;
}

TEST(furngen_iso_tables_and_budget)
{
    int i, k, total = 0, bad = 0;
    CHECK(doomo_furn_count > 0);
    CHECK(doomo_furn_iso_count > 0);
    for (i = 0; i < doomo_furn_count; i++) {
        const DoomoFurn *f = &doomo_furn[i];
        int frames = f->frames ? f->frames : 1, first = doomo_furn_iso_first[i];
        CHECK_MSG(first + frames * 2 <= doomo_furn_iso_count, "%s: facings out of range", f->name);
        if (i + 1 < doomo_furn_count) CHECK_EQ(doomo_furn_iso_first[i + 1], first + frames * 2);
        for (k = 0; k < frames * 2; k++) {
            const DoomoFurnIso *im = &doomo_furn_iso[first + k];
            if (!im->w || !im->h || im->ox > im->w + 64 || im->oy > im->h + 64) bad++;
            total += facing_bytes(im);
        }
    }
    CHECK_EQ(bad, 0);
    CHECK_MSG(total <= 800 * 1024, "art ROM %d B > 0.8 MB", total);
    CHECK_MSG(total / doomo_furn_count <= 2150, "average %d B per piece > 2.1 KB", total / doomo_furn_count);
}

TEST(furngen_prims_match_catalog)
{
    int i, n = 0;
    for (i = 0; i < doomo_furn_count; i++) {
        const DoomoFurn *f = &doomo_furn[i];
        CHECK_MSG(f->nprim > 0, "%s has no prims", f->name);
        CHECK_EQ(f->prim0, n);
        n += f->nprim;
    }
    for (i = 0; i < n; i++) {
        const DoomoFurnPrim *p = &doomo_furn_prims[i];
        CHECK(FURN_PRIM_KIND(p) < FP_COUNT);
        CHECK(p->w > 0 && p->h > 0);
    }
}

/* The resident grant table only names pieces that exist. */
TEST(furngen_grant_table)
{
    const DoomoFurnGrant *g;
    int n = 0;
    for (g = doomo_furn_grants; g->name; g++, n++) {
        CHECK(g->furn < doomo_furn_count);
        CHECK(g->unlock != 0xFFFF);
        CHECK_STR(g->name, doomo_furn[g->furn].name);
    }
    CHECK(n < 128);
}

/* span encoding round-trips, and a row without art gets a placeholder box */
TEST(furngen_span_roundtrip_and_placeholder)
{
    Img im;
    static uint8_t enc[64 * 1024];
    int n, y, x, ok = 1;
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    CHECK_EQ(parse_catalog("[a_sofa] PASTEL 10 \"Sofa\"\n  foot 2 1 6\n", "t"), 0);
    art_default(0);
    CHECK_EQ(arts[0].nprim, 1);
    bake(0, 1, 0, &im);
    CHECK(im.w > 16 && im.h > 8);
    n = span_encode(&im, enc);
    CHECK(n > im.h * 3);
    for (y = 0; y < im.h; y++) {
        const uint8_t *p = enc + (enc[y * 2] | (enc[y * 2 + 1] << 8));
        uint8_t row[256];
        memset(row, 0, sizeof(row));
        x = 0;
        while (*p != 0xFF) { x += p[0]; memcpy(row + x, p + 2, p[1]); x += p[1]; p += 2 + p[1]; }
        if (memcmp(row, im.pix + y * im.w, (size_t)im.w) != 0) ok = 0;
    }
    free(im.pix);
    CHECK(ok);
}
