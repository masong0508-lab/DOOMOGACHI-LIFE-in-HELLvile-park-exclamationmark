/* test_furngen.c - the furniture generator and its output (owner: H4; SPEC
 * H4 acceptance, HF sec.6.2, 11.1).
 *
 * The tool's parser and baker are compiled in (FURNGEN_LIB drops its main),
 * so the checks run on the real code: unmatched keys fail, marker paints
 * are respected, and the generated tables obey the ROM budget (H9: art
 * total <= 1.25 MB, average <= 3.6 KB per piece; the four facings). `make -C core check-gen` checks
 * that core/gen/furn_*.c match the inputs byte for byte.
 */
#define FURNGEN_LIB 1
#include "../tools/furngen.c"
#include "test.h"
#include "../src/home_int.h"
#include "../gen/furn_ids.h"

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
    free(im.cls);
    CHECK(fixed_ok);
    bake(1, 1, 0, &im);
    for (i = 0; i < im.w * im.h; i++) if (im.pix[i] && reserved(im.pix[i])) painted++;
    free(im.pix);
    free(im.cls);
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
    const uint8_t *c = b + n;                      /* (H8) the light classes */
    while (*c != FURN_CLASS_END) c += 4;
    n += (int)(c - (b + im->h * 2)) + 2;                  /* the end and the pad */
    for (y = 0; y < im->h; y++) {
        const uint8_t *p = b + (b[y * 2] | (b[y * 2 + 1] << 8));
        const uint8_t *s = p;
        while (*p != 0xFF) p += 2 + p[1];
        n += (int)(p - s) + 1;
    }
    return n;
}

/* The tables: FURN_ISO_FACINGS facings per frame, every one a picture;
 * facings that share bytes (H9) are counted once. The budget: SPEC H4's
 * 0.8 MB / 2.1 KB per piece for the front facings, plus the back facings
 * the user approved for every piece (DECISIONS: about +450 KB of GBA ROM):
 * 1.25 MB in all, 3.6 KB per piece on average. */
TEST(furngen_iso_tables_and_budget)
{
    static uint8_t seen[4 * 1024 * 1024 / 8];
    int i, k, total = 0, bad = 0, shared = 0;
    CHECK(doomo_furn_count > 0);
    CHECK(doomo_furn_iso_count > 0);
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < doomo_furn_count; i++) {
        const DoomoFurn *f = &doomo_furn[i];
        int frames = f->frames ? f->frames : 1, first = doomo_furn_iso_first[i];
        CHECK_MSG(first + frames * FURN_ISO_FACINGS <= doomo_furn_iso_count, "%s: facings out of range", f->name);
        if (i + 1 < doomo_furn_count) CHECK_EQ(doomo_furn_iso_first[i + 1], first + frames * FURN_ISO_FACINGS);
        else CHECK_EQ(doomo_furn_iso_count, first + frames * FURN_ISO_FACINGS);
        for (k = 0; k < frames * FURN_ISO_FACINGS; k++) {
            const DoomoFurnIso *im = &doomo_furn_iso[first + k];
            if (!im->w || !im->h || im->ox > im->w + 64 || im->oy > im->h + 64) bad++;
            if (im->off / 8 >= sizeof(seen)) { bad++; continue; }
            if (seen[im->off / 8] & (1 << (im->off & 7))) { shared++; continue; }
            seen[im->off / 8] |= (uint8_t)(1 << (im->off & 7));
            total += facing_bytes(im);
        }
    }
    CHECK_EQ(bad, 0);
    CHECK(shared > 0);                                   /* the decals' repeats at least */
    CHECK_MSG(total <= 1280 * 1024, "art ROM %d B > 1.25 MB", total);
    CHECK_MSG(total / doomo_furn_count <= 3600, "average %d B per piece > 3.6 KB", total / doomo_furn_count);
}

/* the box (x0, y0)-(x1, y1) of a baked image's opaque pixels */
static void img_extent(const Img *im, int *x0, int *y0, int *x1, int *y1)
{
    int x, y;
    *x0 = *y0 = 9999; *x1 = *y1 = -1;
    for (y = 0; y < im->h; y++)
        for (x = 0; x < im->w; x++)
            if (im->pix[y * im->w + x]) {
                if (x < *x0) *x0 = x;
                if (x > *x1) *x1 = x;
                if (y < *y0) *y0 = y;
                if (y > *y1) *y1 = y;
            }
}

static int img_same(const Img *a, const Img *b)
{
    return a->w == b->w && a->h == b->h && a->ox == b->ox && a->oy == b->oy &&
           !memcmp(a->pix, b->pix, (size_t)a->w * a->h) && !memcmp(a->cls, b->cls, (size_t)a->w * a->h);
}

static void img_free(Img *im)
{
    free(im->pix);
    free(im->cls);
}

/* (H9) the back facings are the prims turned about the footprint: rot 2 of
 * a piece is rot 0 of the same prims authored a half turn round (x' = W -
 * x - w, y' = D - y - d), rot 3 is rot 1 of them, anchored on the same back
 * corner; so a piece that looks the same turned a half turn has rot 2 ==
 * rot 0, and a lopsided one shows something new (not its front mirrored) */
TEST(furngen_back_facings)
{
    Img a[FURN_ISO_FACINGS], b[FURN_ISO_FACINGS], c[FURN_ISO_FACINGS];
    int k, ok, x0, y0, x1, y1, u0, v0, u1, v1;
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    CHECK_EQ(parse_catalog("[p] PASTEL 1 \"P\"\n  foot 2 1 6\n[q] PASTEL 1 \"Q\"\n  foot 2 1 6\n"
                           "[r] PASTEL 1 \"R\"\n  foot 1 1 4\n", "t"), 0);
    /* a tall post at the back left and a low cushion over the front right, a
     * ball on top; then the same a half turn round (W 32, D 16) */
    CHECK_EQ(parse_art("[p]\nbox 0 0 0 6 4 20 A\nbox 12 10 0 20 6 4 B\nblob 20 4 8 6 4 4 CHERRY\ncyl 26 3 0 2 10 LIME\n", "t"), 0);
    CHECK_EQ(parse_art("[q]\nbox 26 12 0 6 4 20 A\nbox 0 0 0 20 6 4 B\nblob 12 12 8 6 4 4 CHERRY\ncyl 6 13 0 2 10 LIME\n", "t"), 0);
    CHECK_EQ(parse_art("[r]\nbox 2 2 0 12 12 8 A\ncyl 8 8 8 3 6 B\n", "t"), 0);
    for (k = 0; k < FURN_ISO_FACINGS; k++) {
        bake_facing(0, 0, k, a);
        bake_facing(1, 0, k, b);
        bake_facing(2, 0, k, c);
    }
    CHECK(img_same(&a[2], &b[0]));                       /* p turned a half turn == q */
    CHECK(img_same(&a[3], &b[1]));
    CHECK(img_same(&b[2], &a[0]));                       /* and back again */
    CHECK(img_same(&c[2], &c[0]));                       /* r is the same all round */
    CHECK(img_same(&c[3], &c[1]));
    CHECK(!img_same(&a[2], &a[0]) && !img_same(&a[3], &a[1]));
    /* not the front mirrored: facing 2 is no flip of facing 1 */
    ok = a[2].w == a[1].w && a[2].h == a[1].h;
    if (ok) {
        int x, y, same = 1;
        for (y = 0; y < a[1].h && same; y++)
            for (x = 0; x < a[1].w && same; x++)
                if (a[2].pix[y * a[1].w + x] != a[1].pix[y * a[1].w + a[1].w - 1 - x]) same = 0;
        CHECK(!same);
    }
    /* the footprint's back corner stays the anchor: a full-footprint box
     * spans the same screen box from it in every facing of its turn */
    img_extent(&a[0], &x0, &y0, &x1, &y1);
    img_extent(&b[2], &u0, &v0, &u1, &v1);
    CHECK_EQ(x0 - a[0].ox, u0 - b[2].ox);
    CHECK_EQ(y1 - a[0].oy, v1 - b[2].oy);
    for (k = 0; k < FURN_ISO_FACINGS; k++) { img_free(&a[k]); img_free(&b[k]); img_free(&c[k]); }
}

/* the glow pixels of a facing (its class spans, furn.h) */
static int glow_px(const DoomoFurnIso *im)
{
    const uint8_t *c = doomo_furn_iso_data + im->off + im->h * 2;
    int n = 0;
    for (; *c != FURN_CLASS_END; c += 4) if (c[3] == FPL_GLOW) n += c[2];
    return n;
}

/* (H9) on the real data: the cottage hearth (a fireplace that stands
 * with its back to a wall, so a turned view shows that back) has back
 * facings of their own, neither its front nor its front mirrored */
TEST(furngen_real_back_facings)
{
    int first = doomo_furn_iso_first[FURN_COTTAGE_HEARTH], k;
    const DoomoFurnIso *im = &doomo_furn_iso[first];
    for (k = 0; k < 2; k++) {
        CHECK(im[k + 2].w && im[k + 2].h);
        CHECK(im[k + 2].off != im[k].off && im[k + 2].off != im[1 - k].off);
        CHECK(facing_bytes(&im[k + 2]) != facing_bytes(&im[k]) || im[k + 2].w != im[k].w);
    }
    /* the hearth's back hides the fire: only the mantel's candle flames
     * glow there (a few px), the front shows the fire's dozens */
    for (k = 0; k < 2; k++) {
        int front = glow_px(&im[k]), back = glow_px(&im[k + 2]);
        CHECK_MSG(front >= 30 && back < front / 3, "facing %d glows %d px, its back %d", k, front, back);
    }
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
    CHECK_EQ(enc[im.h * 2], FURN_CLASS_END);              /* (H8) a placeholder has no glow / pane */
    for (y = 0; y < im.h; y++) {
        const uint8_t *p = enc + (enc[y * 2] | (enc[y * 2 + 1] << 8));
        uint8_t row[256];
        memset(row, 0, sizeof(row));
        x = 0;
        while (*p != 0xFF) { x += p[0]; memcpy(row + x, p + 2, p[1]); x += p[1]; p += 2 + p[1]; }
        if (memcmp(row, im.pix + y * im.w, (size_t)im.w) != 0) ok = 0;
    }
    free(im.pix);
    free(im.cls);
    CHECK(ok);
}

/* (H8) glow and pane prims: their pixels come out as GLOW / PANE runs that
 * never mix classes, the round trip keeps every pixel and its class, the
 * prim records carry the class in mat (furn_prim_mat), a pane is for
 * windows only and a prim is not both */
TEST(furngen_light_classes)
{
    static uint8_t enc[64 * 1024];
    Img im;
    int y, x, ok = 1, n, cls, glow = 0, pane = 0, k;
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    CHECK_EQ(parse_catalog("[a_win] WALL 10 \"W\"\n  foot 1 1 6  flags WALL  role WINDOW\n"
                           "[a_lamp] PASTEL 10 \"L\"\n  foot 1 1 6  flags LIGHT  role LAMP\n", "t"), 0);
    CHECK(parse_art("[a_lamp]\nbox 1 1 0 4 4 4 A pane\n", "t") > 0);          /* not a window */
    CHECK(strstr(fg_lasterr, "windows") != NULL);
    memset(arts, 0, sizeof(arts));
    CHECK(parse_art("[a_win]\nbox 1 1 0 4 2 4 A glow pane\n", "t") > 0);      /* both */
    CHECK(strstr(fg_lasterr, "not both") != NULL);
    memset(arts, 0, sizeof(arts));
    CHECK_EQ(parse_art("[a_win]\nbox 1 0 0 14 3 3 A\nbox 3 0 3 10 2 17 B shiny pane\n", "t"), 0);
    CHECK_EQ(parse_art("[a_lamp]\ncyl 8 8 0 3 12 A\nblob 8 8 16 4 4 4 LEMON glow\n", "t"), 0);
    for (k = 0; k < 2; k++) {
        bake(k, 0, 0, &im);
        n = span_encode(&im, enc);
        CHECK(n > 0);
        {
            const uint8_t *c = enc + im.h * 2;
            int py = -1, px = -1;
            for (y = 0; y < im.h; y++) {
                const uint8_t *p = enc + (enc[y * 2] | (enc[y * 2 + 1] << 8));
                uint8_t row[256], rc[256];
                memset(row, 0, sizeof(row));
                memset(rc, 0, sizeof(rc));
                x = 0;
                while (*p != 0xFF) { x += p[0]; memcpy(row + x, p + 2, p[1]); x += p[1]; p += 2 + p[1]; }
                for (; *c != FURN_CLASS_END && *c == y; c += 4) {
                    int i;
                    if (y == py && c[1] < px) ok = 0;             /* in x order */
                    py = y; px = c[1] + c[2];
                    if (c[3] != FPL_GLOW && c[3] != FPL_PANE) ok = 0;
                    for (i = 0; i < c[2]; i++) {
                        if (!row[c[1] + i]) ok = 0;                /* never over a gap */
                        rc[c[1] + i] = c[3];
                    }
                    glow += c[3] == FPL_GLOW;
                    pane += c[3] == FPL_PANE;
                }
                if (memcmp(row, im.pix + y * im.w, (size_t)im.w) != 0) ok = 0;
                for (x = 0; x < im.w; x++) if (row[x] && rc[x] != im.cls[y * im.w + x]) ok = 0;
            }
            if (*c != FURN_CLASS_END) ok = 0;                     /* in row order, all used */
        }
        free(im.pix);
        free(im.cls);
    }
    CHECK(ok);
    CHECK(glow > 0 && pane > 0);
    /* the generated records: every window with a pane prim, the hearth's fire */
    CHECK_EQ(load_real(), 0);
    for (k = 0, glow = pane = 0; k < doomo_furn_count; k++) {
        const DoomoFurn *f = &doomo_furn[k];
        int j, g = 0, pn = 0;
        for (j = 0; j < f->nprim; j++) {
            const DoomoFurnPrim *p = &doomo_furn_prims[f->prim0 + j];
            int m = furn_prim_mat(p, &cls), a = arts[k].p[j].style;
            CHECK_MSG(cls == ((a & FG_GLOW) ? FPL_GLOW : (a & FG_PANE) ? FPL_PANE : FPL_NONE), "%s prim %d", rows[k].key, j);
            CHECK(m == FM_A || m == FM_B || (m >= PAL_PAINT_BASE && m < PAL_SKIN_BASE) || m >= PAL_RAMP_BASE);
            g += cls == FPL_GLOW;
            pn += cls == FPL_PANE;
        }
        if (k == FURN_COTTAGE_HEARTH) CHECK_MSG(g > 0, "the hearth's fire glows");
        if (f->role == FR_WINDOW) pane += pn > 0;
        glow += g > 0;
    }
    CHECK_MSG(pane >= 10, "%d windows with panes", pane);
    CHECK(glow >= 40);
}

/* (H1 M3 data, H4's joiner) themes, blueprints and layouts are checked by key,
 * role, room, paint and grid, and the real files parse clean */
TEST(furngen_house_sets)
{
    static const char *const rf[] = { "themes", "blueprints", "layouts_s3", NULL };
    char path[700];
    int k;
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    CHECK_EQ(parse_catalog("[a_chair] PASTEL 10 \"A Chair\"\n  foot 1 1 4\n", "t"), 0);
    nbps = nlays = 0;
    memset(themes, 0, sizeof(themes));
    CHECK_EQ(parse_themes("[theme PASTEL] \"P\"\n  walls 0 ROSE\n  floor 1 CARAMEL\n  paints ROSE MINT\n"
                          "  ceiling 2\n  accent LEMON\n", "t"), 0);
    CHECK(themes[THEME_PASTEL].has && themes[THEME_PASTEL].ceil == 2 && themes[THEME_PASTEL].wpaint == PAINT_ROSE);
    CHECK(parse_themes("[theme NOPE] \"X\"\n", "t") > 0);
    CHECK(parse_themes("[theme CLOUD] \"X\"\n  ceiling 9\n", "t") > 0);
    CHECK_EQ(parse_sets("[bp b1] LIVING \"B\"\n  piece a_chair 2 3 S\n  small a_chair 2 3 E 3\n", "t", 0), 0);
    CHECK_EQ(nbps, 1);
    CHECK_EQ(bps[0].n, 2);
    CHECK_EQ(bps[0].p[1].sub, 3);
    CHECK_EQ(bps[0].p[1].rot, 1);
    CHECK(parse_sets("[bp b2] LIVING \"B\"\n  piece no_such 2 3 S\n", "t", 0) > 0);        /* key */
    CHECK(parse_sets("[bp b3] LIVING \"B\"\n  piece a_chair 6 0 S\n", "t", 0) > 0);        /* off the grid */
    CHECK(parse_sets("[bp b4] ATTIC \"B\"\n  piece a_chair 1 1 S\n", "t", 0) > 0);         /* room */
    CHECK(parse_sets("[bp b5] LIVING \"B\"\n  any SEAT 1 1 S\n", "t", 0) > 0);             /* `any`: layouts */
    CHECK_EQ(parse_sets("[layout 3 l1] ANNEX \"L\"\n  tol 2\n  any SEAT 1 1 S\n  need 1\n", "t", 1), 0);
    CHECK_EQ(lays[0].id, 3);
    CHECK(lays[0].p[0].furn == (int)(HOME_PIECE_ROLE | FR_SEAT));
    CHECK(parse_sets("[layout 3 l2] ANNEX \"L\"\n  piece a_chair 1 1 S\n", "t", 1) > 0);   /* id twice */
    CHECK(parse_sets("[layout 16 l3] ANNEX \"L\"\n  piece a_chair 1 1 S\n", "t", 1) > 0);  /* id range */
    /* the real files */
    CHECK(load_real() == 0);
    nbps = nlays = 0;
    memset(themes, 0, sizeof(themes));
    for (k = 0; rf[k]; k++) {
        FILE *f;
        long n;
        char *text;
        snprintf(path, sizeof(path), "%s/../../data/home/%s.txt", test_golden_dir, rf[k]);
        f = fopen(path, "rb");
        CHECK_MSG(f != NULL, "%s", path);
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        text = (char *)malloc((size_t)n + 1);
        if (fread(text, 1, (size_t)n, f) != (size_t)n) n = 0;
        text[n] = 0;
        fclose(f);
        CHECK_MSG((k == 0 ? parse_themes(text, path) : parse_sets(text, path, k == 2)) == 0, "%s: %s", rf[k], fg_lasterr);
        free(text);
    }
    for (k = 0; k < THEME_COUNT; k++) CHECK(themes[k].has);
    CHECK_EQ(nbps, (int)doomo_home_blueprint_count);
    CHECK(nlays >= 1);
}

/* (H4 M2) every catalog row is drawn, and no art uses a paint slot the
 * catalog fixes (its marker shades would show for good) */
TEST(furngen_all_art_and_paint_slots)
{
    int i, e = load_real();
    CHECK_MSG(e == 0, "the real catalog / art failed to parse (%d errors: %s)", e, fg_lasterr);
    for (i = 0; i < nrows; i++) CHECK_MSG(arts[i].has, "%s has no art", rows[i].key);
    CHECK_EQ(check_paint_slots(), 0);
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    CHECK_EQ(parse_catalog("[f] PASTEL 1 \"F\"\n  foot 1 1 4\n  paints CHERRY FIXED\n", "t"), 0);
    CHECK_EQ(parse_art("[f]\nbox 2 2 0 12 12 10 A\nblob 8 8 14 4 4 4 B\n", "t"), 0);
    CHECK(check_paint_slots() > 0);
}

/* (H6) the animation frames agree with the catalog: FF_ANIM <=> frames > 1,
 * no prim names a frame past `frames`, and the frames differ */
static int anim_case(const char *cat, const char *art)
{
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    if (parse_catalog(cat, "t") || parse_art(art, "t")) return -1;
    return check_anim_frames();
}

TEST(furngen_anim_frames_checked)
{
    static const char *const anim = "[a] PASTEL 1 \"A\"\n  foot 1 1 4  flags ANIM\n";
    static const char *const still = "[a] PASTEL 1 \"A\"\n  foot 1 1 4\n";
    int i, e;
    CHECK(anim_case(anim, "[a]\nbox 2 2 0 12 12 8 A\n") > 0);                                  /* no frames */
    CHECK_EQ(anim_case(anim, "[a]\nframes 2\nbox 2 2 0 12 12 8 A\nbox@1 4 4 8 8 8 2 B\n"), 0);
    CHECK(anim_case(still, "[a]\nframes 2\nbox 2 2 0 12 12 8 A\nbox@1 4 4 8 8 8 2 B\n") > 0);  /* not FF_ANIM */
    CHECK(anim_case(anim, "[a]\nframes 2\nbox 2 2 0 12 12 8 A\nbox@2 4 4 8 8 8 2 B\n") > 0);   /* frame 2 of 2 */
    CHECK(anim_case(anim, "[a]\nframes 2\nbox 2 2 0 12 12 8 A\n") > 0);                       /* all the same */
    CHECK(anim_case(anim, "[a]\nframes 2\nbox@01 2 2 0 12 12 8 A\nbox@01 4 4 8 8 8 2 B\n") > 0); /* ... tagged */
    CHECK(anim_case(anim, "[a]\nframes 3\nbox 2 2 0 12 12 8 A\nbox@012 4 4 8 8 8 2 B\n") > 0);
    CHECK_EQ(anim_case(anim, "[a]\nframes 3\nbox 2 2 0 12 12 8 A\nbox@01 4 4 8 8 8 2 B\n"), 0);
    CHECK_EQ(anim_case(still, "[a]\nbox 2 2 0 12 12 8 A\n"), 0);
    /* the real art: every FF_ANIM piece animates (the 6 pieces H6 drew), and
     * the generated tables agree */
    e = load_real();
    CHECK_MSG(e == 0, "the real catalog / art failed to parse (%d errors: %s)", e, fg_lasterr);
    CHECK_EQ(check_anim_frames(), 0);
    for (i = 0; i < nrows; i++)
        if (rows[i].flags & FF_ANIM) CHECK_MSG(arts[i].frames > 1, "%s is FF_ANIM with %d frame", rows[i].key, arts[i].frames);
    for (i = 0; i < doomo_furn_count; i++) {
        const DoomoFurn *f = &doomo_furn[i];
        CHECK_MSG(((f->flags & FF_ANIM) != 0) == (f->frames > 1), "%s: FF_ANIM vs %d frames", f->name, f->frames);
    }
}

/* the box (w x h) of a baked sprite's pixels that are not slot A's paint
 * (its 3 marker shades and its outline): on the haunting's eye, the eye
 * drawn into the slot-A heart */
static void not_a_extent(const DoomoFurnIso *im, int *w, int *h)
{
    const uint8_t *base = doomo_furn_iso_data + im->off;
    int x0 = 255, y0 = 255, x1 = -1, y1 = -1, y, s;
    for (y = 0; y < im->h; y++) {
        const uint8_t *p = base + (base[y * 2] | (base[y * 2 + 1] << 8));
        int x = 0;
        while (*p != 0xFF) {
            int skip = p[0], len = p[1], i;
            p += 2;
            x += skip;
            for (i = 0; i < len; i++) {
                int c = p[i], mark = c == palette_darker(PAL_PAINT(FURN_MARK_A, 0));
                for (s = 0; s < 3; s++) mark |= c == PAL_PAINT(FURN_MARK_A, s);
                if (mark) continue;
                if (x + i < x0) x0 = x + i;
                if (x + i > x1) x1 = x + i;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
            p += len;
            x += len;
        }
    }
    *w = x1 < 0 ? 0 : x1 - x0 + 1;
    *h = y1 < 0 ? 0 : y1 - y0 + 1;
}

/* (H6) a `mirror` wall decal: facing 1 is facing 0 flipped, anchored where
 * an IMG_FLIPX draw of facing 0 sits, and the row is FF_MIRROR (the PS1
 * mirrors the prims); other pieces may not use it; (H9) its facings 2 / 3
 * are facings 0 / 1 again (the same bytes). The haunting's eye is one: in
 * frame 0 the open eye itself (the white and the iris inside the heart) is
 * at least 8x6 px, in the blink frame the lash line is at least 8 px wide. */
TEST(furngen_mirror_decal)
{
    Img a, b, fr[FURN_ISO_FACINGS];
    int x, y, i, ok = 1, first, ew, eh;
    memset(arts, 0, sizeof(arts));
    fg_errors = 0;
    CHECK_EQ(parse_catalog("[w] WALL 1 \"W\"\n  foot 1 1 3  flags WALL\n[p] PASTEL 1 \"P\"\n  foot 1 1 4\n", "t"), 0);
    CHECK_EQ(parse_art("[w]\nmirror\nbox 2 0 2 3 1 4 A\nbox 9 0 6 2 1 2 CHERRY\n", "t"), 0);
    CHECK(arts[0].mirror);
    CHECK(parse_art("[p]\nmirror\nbox 2 2 0 12 12 8 A\n", "t") > 0);
    bake(0, 0, 0, &a);
    mirror_img(&a, &b);
    CHECK_EQ(b.w, a.w);
    CHECK_EQ(b.h, a.h);
    CHECK_EQ(b.oy, a.oy);
    CHECK_EQ(b.ox, a.w - a.ox);
    for (y = 0; y < a.h; y++)
        for (x = 0; x < a.w; x++)
            if (b.pix[y * a.w + x] != a.pix[y * a.w + a.w - 1 - x]) ok = 0;
    CHECK(ok);
    free(a.pix);
    free(a.cls);
    free(b.pix);
    free(b.cls);
    for (x = 0; x < FURN_ISO_FACINGS; x++) bake_facing(0, 0, x, fr);
    CHECK(img_same(&fr[2], &fr[0]));
    CHECK(img_same(&fr[3], &fr[1]));
    for (x = 0; x < FURN_ISO_FACINGS; x++) img_free(&fr[x]);
    first = doomo_furn_iso_first[FURN_HEART_WALLPAPER_EYE];
    CHECK_EQ(doomo_furn[FURN_HEART_WALLPAPER_EYE].frames, 2);
    CHECK(doomo_furn[FURN_HEART_WALLPAPER_EYE].flags2 & FF_MIRROR);
    for (i = 0; i < doomo_furn_count; i++)
        if (doomo_furn[i].flags2 & FF_MIRROR) CHECK_MSG(doomo_furn[i].flags & FF_WALL, "%d: FF_MIRROR off a wall", i);
    for (x = 0; x < 2 * FURN_ISO_FACINGS; x++) {          /* frame x / 4, facing x % 4 */
        const DoomoFurnIso *im = &doomo_furn_iso[first + x];
        int fc = x % FURN_ISO_FACINGS;
        not_a_extent(im, &ew, &eh);
        if (x < FURN_ISO_FACINGS) CHECK_MSG(ew >= 8 && eh >= 6, "the open eye (facing %d) is %dx%d px", fc, ew, eh);
        else CHECK_MSG(ew >= 8 && eh >= 1, "the blink (facing %d) is %dx%d px", fc, ew, eh);
        if (fc == 1) CHECK_EQ(im->ox, im->w - doomo_furn_iso[first + x - 1].ox);
        if (fc >= 2) {
            CHECK_EQ(im->off, im[-2].off);
            CHECK_EQ(im->ox, im[-2].ox);
            CHECK_EQ(im->w, im[-2].w);
        }
    }
}

/* (H4) every theme names its fittings: a door and a window skin of the right
 * role; anything else is refused */
TEST(furngen_theme_fittings)
{
    char path[700];
    char *text;
    FILE *f;
    long n;
    int k;
    CHECK_EQ(load_real(), 0);
    snprintf(path, sizeof(path), "%s/../../data/home/themes.txt", test_golden_dir);
    f = fopen(path, "rb");
    CHECK(f != NULL);
    if (!f) return;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    text = (char *)malloc((size_t)n + 1);
    if (fread(text, 1, (size_t)n, f) != (size_t)n) n = 0;
    text[n] = 0;
    fclose(f);
    memset(themes, 0, sizeof(themes));
    CHECK_EQ(parse_themes(text, path), 0);
    free(text);
    for (k = 0; k < THEME_COUNT; k++) {
        CHECK(themes[k].has);
        CHECK_MSG(themes[k].door >= 0 && rows[themes[k].door].role == FR_DOOR, "theme %d door", k);
        CHECK_MSG(themes[k].window >= 0 && rows[themes[k].window].role == FR_WINDOW, "theme %d window", k);
        CHECK_EQ(doomo_home_themes[k].door, themes[k].door);       /* and the generated table agrees */
        CHECK_EQ(doomo_home_themes[k].window, themes[k].window);
    }
    memset(themes, 0, sizeof(themes));
    CHECK(parse_themes("[theme PASTEL] \"P\"\n  door cupcake_lamp\n", "t") > 0);   /* not a door */
    memset(themes, 0, sizeof(themes));
    CHECK(parse_themes("[theme PASTEL] \"P\"\n  window pink_door\n", "t") > 0);    /* a door, not a window */
    memset(themes, 0, sizeof(themes));
    CHECK(parse_themes("[theme PASTEL] \"P\"\n  door no_such_door\n", "t") > 0);
}
