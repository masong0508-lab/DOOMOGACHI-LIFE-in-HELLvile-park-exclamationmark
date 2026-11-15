/* test_cr5.c - CR5: the look pool and its save (docs/CREATOR.md "CR5").
 *
 * - the pool: put / find / drop, a run replaced in place, a full pool keeps the old run, reset, the
 *   saved form (raw / load) and what a malformed one does;
 * - the LOOK section of the creator's save chunk: a town's runs survive save_serialize /
 *   save_deserialize (with and without nicknames and gene bits beside them); a save without it (an old
 *   build's, a town that never used a deep part) loads with an empty pool and the same town;
 * - town_free forgets a citizen's run;
 * - the portrait cache sees the run: one image per look, a hit while the run is unchanged. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/look_fields.h"
#include "doomo/look_pool.h"
#include "doomo/portrait.h"
#include "doomo/save.h"
#include "doomo/save_ext.h"

static int pool_free(void)
{
    uint32_t n;
    look_pool_raw(&n);
    return (int)LOOK_POOL_CAP - (int)n;
}

/* the pool's saved bytes into dst; returns how many */
static uint32_t pool_copy(uint8_t *dst)
{
    uint32_t n;
    const uint8_t *p = look_pool_raw(&n);
    memcpy(dst, p, n);
    return n;
}

static const uint8_t RUN_A[] = { DF_LASH, 2, DF_LINER, 1 };
static const uint8_t RUN_B[] = { DF_LID, 3 };

TEST(cr5_pool_put_find_drop)
{
    const uint8_t *r;
    int len = -1;
    look_pool_reset();
    CHECK(look_pool_find(7, RUN_DEEP, &len) == NULL);
    CHECK_EQ(len, 0);
    CHECK_EQ(pool_free(), (int)LOOK_POOL_CAP);
    CHECK(look_pool_put(7, RUN_DEEP, RUN_A, 4));
    r = look_pool_find(7, RUN_DEEP, &len);
    CHECK(r != NULL && len == 4 && memcmp(r, RUN_A, 4) == 0);
    CHECK_EQ(pool_free(), (int)LOOK_POOL_CAP - 8);
    /* another kind and another id are other runs */
    CHECK(look_pool_find(7, RUN_NICK, &len) == NULL);
    CHECK(look_pool_put(8, RUN_DEEP, RUN_B, 2));
    CHECK(look_pool_find(8, RUN_DEEP, &len) != NULL && len == 2);
    /* a put replaces in place (no second record) ... */
    CHECK(look_pool_put(7, RUN_DEEP, RUN_B, 2));
    r = look_pool_find(7, RUN_DEEP, &len);
    CHECK(r != NULL && len == 2 && memcmp(r, RUN_B, 2) == 0);
    CHECK_EQ(pool_free(), (int)LOOK_POOL_CAP - 12);
    /* ... a zero length removes it ... */
    CHECK(look_pool_put(7, RUN_DEEP, NULL, 0));
    CHECK(look_pool_find(7, RUN_DEEP, &len) == NULL);
    CHECK(look_pool_find(8, RUN_DEEP, &len) != NULL);        /* the neighbour is untouched */
    /* ... and drop takes every run of the id */
    CHECK(look_pool_put(8, RUN_NICK, RUN_A, 4));
    look_pool_drop(8);
    CHECK(look_pool_find(8, RUN_DEEP, &len) == NULL && look_pool_find(8, RUN_NICK, &len) == NULL);
    CHECK_EQ(pool_free(), (int)LOOK_POOL_CAP);
    /* bad arguments */
    CHECK(!look_pool_put(-1, RUN_DEEP, RUN_A, 4));
    CHECK(!look_pool_put(1024, RUN_DEEP, RUN_A, 4));
    CHECK(!look_pool_put(3, RUN_DEEP, RUN_A, 256));
    look_pool_reset();
}

TEST(cr5_a_full_pool_keeps_the_old_run)
{
    uint8_t big[200];
    const uint8_t *r;
    int id, n = 0, len;
    look_pool_reset();
    memset(big, 1, sizeof(big));
    for (id = 100; look_pool_put(id, RUN_DEEP, big, 200); id++) n++;
    CHECK(n >= 1);
    CHECK(pool_free() < 204);
    CHECK(look_pool_put(5, RUN_NICK, big, pool_free() - 4));      /* a filler: no byte left */
    CHECK_EQ(pool_free(), 0);
    /* a longer run for a stored id does not fit: it is refused and the old run is still there */
    CHECK(look_pool_put(101, RUN_DEEP, big, 200));                     /* same length: fine */
    CHECK(!look_pool_put(101, RUN_DEEP, big, 201));                    /* one byte more: refused */
    CHECK(!look_pool_put(id, RUN_DEEP, RUN_B, 2));                     /* a new id: full */
    CHECK(look_pool_find(id, RUN_DEEP, &len) == NULL);
    r = look_pool_find(101, RUN_DEEP, &len);
    CHECK(r != NULL && len == 200 && r[0] == 1);
    CHECK(look_pool_put(100, RUN_DEEP, RUN_A, 4));                     /* a shorter one frees room ... */
    CHECK(look_pool_put(id, RUN_DEEP, RUN_B, 2));                      /* ... for a new run */
    look_pool_reset();
    CHECK_EQ(pool_free(), (int)LOOK_POOL_CAP);
}

TEST(cr5_pool_raw_and_load_round_trip)
{
    static uint8_t buf[LOOK_POOL_CAP + 8];
    const uint8_t *r;
    uint32_t n, m;
    int len;
    look_pool_reset();
    look_pool_raw(&n);
    CHECK_EQ(n, 0u);
    CHECK(look_pool_put(3, RUN_DEEP, RUN_A, 4));
    CHECK(look_pool_put(900, RUN_DEEP, RUN_B, 2));
    CHECK(look_pool_put(1001, RUN_DEEP, RUN_B, 2));                   /* a Look Book style */
    n = pool_copy(buf);
    CHECK_EQ(n, 8u + 6u + 6u);
    look_pool_reset();
    CHECK(look_pool_find(3, RUN_DEEP, &len) == NULL);
    CHECK(look_pool_load(buf, n));
    r = look_pool_find(3, RUN_DEEP, &len);
    CHECK(r != NULL && len == 4 && memcmp(r, RUN_A, 4) == 0);
    CHECK(look_pool_find(900, RUN_DEEP, &len) != NULL && look_pool_find(1001, RUN_DEEP, &len) != NULL);
    CHECK_EQ(pool_free(), (int)LOOK_POOL_CAP - (int)n);
    look_pool_raw(&m);
    CHECK_EQ(m, n);
    /* malformed: a record that runs past the end, half a header, too many bytes: refused, the pool is empty */
    CHECK(!look_pool_load(buf, n - 1));
    CHECK(look_pool_find(3, RUN_DEEP, &len) == NULL);
    CHECK(!look_pool_load(buf, 3));
    CHECK(!look_pool_load(buf, LOOK_POOL_CAP + 1));
    CHECK(look_pool_load(buf, n));
    CHECK(look_pool_load(buf, 0));                                    /* nothing: an empty pool */
    CHECK_EQ(pool_free(), (int)LOOK_POOL_CAP);
    look_pool_reset();
}

static int pool_get(int id, int df)
{
    DoomoDeep d;
    look_resolve(NULL, id, &d);
    return d.v[df];
}

static const DoomoSaveExt *look_ext(void) { return doomo_save_ext_extra; }

/* the chunk the creator writes for the current town and pool */
static uint32_t chunk_of(uint8_t *dst, uint32_t cap)
{
    return look_ext()->write(dst, cap);
}

TEST(cr5_look_survives_a_save)
{
    static uint8_t img[DOOMO_SAVE_SLOT1_MAX];
    static uint8_t keep[LOOK_POOL_CAP], now[LOOK_POOL_CAP];
    uint32_t n, kept;
    int len, pass;
    for (pass = 0; pass < 2; pass++) {           /* 0: no gene bits in the town; 1: some, and a nickname */
        town_new(TOWN_FREEPLAY, 777u, TOWN_POP_PREMADE);
        if (pass) {
            DoomoResident *r = town_res(12);
            res_gset(r, GF_GLASSES, 3);                         /* a gene-only field: the trailer comes first */
            resident_sanitize(r);
        }
        CHECK(look_pool_put(3, RUN_DEEP, RUN_A, 4));
        CHECK(look_pool_put(77, RUN_DEEP, RUN_B, 2));
        CHECK(look_pool_put(DOOMO_PLAYER_ID, RUN_DEEP, RUN_A, 4));
        kept = pool_copy(keep);
        CHECK_EQ(kept, 8u + 6u + 8u);
        n = save_serialize(img, sizeof(img));
        CHECK(n > 0);
        look_pool_reset();                                      /* a fresh boot */
        CHECK(look_pool_find(3, RUN_DEEP, &len) == NULL);
        CHECK(save_deserialize(img, n));
        CHECK(pool_copy(now) == kept && memcmp(keep, now, kept) == 0);
        CHECK_EQ(pool_get(3, DF_LASH), 2);
        CHECK_EQ(pool_get(77, DF_LID), 3);
        CHECK_EQ(pool_get(DOOMO_PLAYER_ID, DF_LINER), 1);
        if (pass) CHECK_EQ(res_gget(town_res(12), GF_GLASSES), 3u);   /* the gene trailer still reads */
    }
}

TEST(cr5_saves_without_a_look_section_load_the_same)
{
    static uint8_t img0[DOOMO_SAVE_SLOT1_MAX], img1[DOOMO_SAVE_SLOT1_MAX];
    static DoomoTown after;
    uint8_t chunk[64], nickonly[1 + 16];
    uint32_t n0, n1;
    int len;
    /* a town that never used a deep part writes the chunk it always wrote: no pool section, so the
     * saved image is the same with the pool code or without (compare an empty pool to a reset one) */
    town_new(TOWN_FREEPLAY, 31u, TOWN_POP_PREMADE);
    look_pool_reset();
    n0 = save_serialize(img0, sizeof(img0));
    CHECK(n0 > 0);
    CHECK_EQ(look_ext()->size(), 0u);                              /* nothing to say: no chunk bytes */
    CHECK(look_pool_put(5, RUN_DEEP, RUN_B, 2));
    CHECK(look_ext()->size() > 0u);
    look_pool_drop(5);
    n1 = save_serialize(img1, sizeof(img1));
    CHECK(n0 == n1 && memcmp(img0, img1, n0) == 0);                /* a dropped run leaves no trace */
    /* loading it clears a pool that held something else: the save had no LOOK section */
    CHECK(look_pool_put(9, RUN_DEEP, RUN_A, 4));
    memcpy(&after, &doomo_town, sizeof(after));
    CHECK(save_deserialize(img0, n0));
    CHECK(look_pool_find(9, RUN_DEEP, &len) == NULL);
    CHECK(memcmp(after.res, doomo_town.res, sizeof(after.res)) == 0);   /* and the citizens are the same */
    /* the chunk of a v1 (pre-CR3) build: one nickname and nothing after it */
    memset(nickonly, 0, sizeof(nickonly));
    nickonly[0] = 1; nickonly[1] = 4; nickonly[5] = 'M';
    look_pool_put(9, RUN_DEEP, RUN_A, 4);
    CHECK(look_ext()->read(nickonly, sizeof(nickonly), 1));
    CHECK(look_pool_find(9, RUN_DEEP, &len) == NULL);              /* read starts from an empty pool */
    /* the chunk of a CR3 build: a nickname and a gene trailer with no entries, then nothing */
    memcpy(chunk, nickonly, sizeof(nickonly));
    chunk[sizeof(nickonly)] = 0; chunk[sizeof(nickonly) + 1] = 0;
    CHECK(look_ext()->read(chunk, sizeof(nickonly) + 2, 1));
    CHECK(look_pool_find(9, RUN_DEEP, &len) == NULL);
    look_pool_reset();
}

TEST(cr5_a_damaged_look_section_never_fails_the_load)
{
    uint8_t chunk[64];
    int len;
    town_new(TOWN_FREEPLAY, 31u, TOWN_POP_PREMADE);
    look_pool_reset();
    CHECK(look_pool_put(3, RUN_DEEP, RUN_A, 4));
    memset(chunk, 0, sizeof(chunk));
    CHECK_EQ(chunk_of(chunk, sizeof(chunk)), 1u + 2u + 2u + 8u);   /* no nick, empty gene trailer, the section */
    CHECK(look_ext()->read(chunk, 1 + 2 + 2 + 8, 2));
    CHECK(look_pool_find(3, RUN_DEEP, &len) != NULL);
    /* a section whose length runs past the chunk: the pool is dropped, the chunk still reads */
    chunk[3] = 0x40;
    CHECK(look_ext()->read(chunk, 1 + 2 + 2 + 8, 2));
    CHECK(look_pool_find(3, RUN_DEEP, &len) == NULL);
    /* half a length */
    CHECK(look_ext()->read(chunk, 1 + 2 + 1, 2));
    CHECK(look_pool_find(3, RUN_DEEP, &len) == NULL);
    look_pool_reset();
}

TEST(cr5_town_free_forgets_a_citizens_run)
{
    int len;
    town_new(TOWN_FREEPLAY, 31u, TOWN_POP_PREMADE);
    look_pool_reset();
    CHECK(look_pool_put(40, RUN_DEEP, RUN_A, 4));
    CHECK(look_pool_put(41, RUN_DEEP, RUN_A, 4));
    town_free(40);
    CHECK(look_pool_find(40, RUN_DEEP, &len) == NULL);
    CHECK(look_pool_find(41, RUN_DEEP, &len) != NULL);
    town_new(TOWN_FREEPLAY, 32u, TOWN_POP_PREMADE);                /* a new town starts empty */
    CHECK(look_pool_find(41, RUN_DEEP, &len) == NULL);
}

TEST(cr5_the_portrait_cache_sees_a_run)
{
    const DoomoImage *a, *b, *c;
    uint32_t u0, k0;
    static uint8_t p0[PORTRAIT_W * PORTRAIT_H];
    int id = 12;
    town_new(TOWN_FREEPLAY, 99u, TOWN_POP_PREMADE);
    look_pool_reset();
    portrait_cache_reset();
    a = portrait_image(id, EXPR_NEUTRAL);
    CHECK(a != NULL);
    u0 = a->uid;
    memcpy(p0, a->pix, sizeof(p0));
    b = portrait_image(id, EXPR_NEUTRAL);                          /* no run: a hit */
    CHECK(b != NULL && b->uid == u0);
    k0 = look_key(id);
    CHECK(look_pool_put(id, RUN_DEEP, (const uint8_t[]){ DF_LASH, 2, DF_LINER, 2 }, 4));
    CHECK(look_key(id) != k0);                                     /* the key sees the run, no invalidate */
    c = portrait_image(id, EXPR_NEUTRAL);
    CHECK(c != NULL && c->uid != u0);
    CHECK(memcmp(c->pix, p0, sizeof(p0)) != 0);                    /* and the face is drawn with it */
    u0 = c->uid;
    b = portrait_image(id, EXPR_NEUTRAL);                          /* the same run again: a hit */
    CHECK(b != NULL && b->uid == u0);
    CHECK(look_pool_put(id, RUN_DEEP, (const uint8_t[]){ DF_LASH, 2, DF_LINER, 3 }, 4));
    c = portrait_image(id, EXPR_NEUTRAL);                          /* another run: another image */
    CHECK(c != NULL && c->uid != u0);
    look_pool_drop(id);
    CHECK_EQ(look_key(id), k0);                                    /* the run gone: the key is as it was */
    c = portrait_image(id, EXPR_NEUTRAL);
    CHECK(c != NULL && memcmp(c->pix, p0, sizeof(p0)) == 0);
    look_pool_reset();
    portrait_cache_reset();
}

TEST(cr5_the_chunk_never_goes_over_its_limit)
{
    static uint8_t chunk[DOOMO_SAVE_EXT_MAX + 16];
    uint8_t big[200];
    int i, id;
    town_new(TOWN_FREEPLAY, 31u, TOWN_POP_PREMADE);
    look_pool_reset();
    memset(big, 1, sizeof(big));
    for (id = 100; look_pool_put(id, RUN_DEEP, big, 200); id++) { }       /* a full pool */
    /* few gene entries: the pool is in the chunk */
    CHECK(look_ext()->size() > 5000u);
    CHECK(look_ext()->size() <= DOOMO_SAVE_EXT_MAX);
    CHECK_EQ(chunk_of(chunk, sizeof(chunk)), look_ext()->size());
    /* a town where every citizen has gene bits (8 B each): the pool no longer fits, so it is left out and
     * the chunk stays under the limit (the creator refuses a commit that would grow it that far) */
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *r = town_res(i);
        if (r->status == RES_EMPTY) continue;
        res_gset(r, GF_GLASSES, 3);
    }
    CHECK(look_gene_ext_size() > 4000u);
    CHECK(look_ext()->size() <= DOOMO_SAVE_EXT_MAX);
    CHECK_EQ(chunk_of(chunk, sizeof(chunk)), look_ext()->size());
    CHECK_EQ(look_ext()->size(), 1u + look_gene_ext_size());          /* the nick count and the gene trailer only */
    look_pool_reset();
}

/* CR5-FIX: a damaged pool section rejects only the pool; the nicknames and the gene bits stay (save.c
 * resets a chunk whose read() returns 0, which would drop them too) */
TEST(cr5_a_damaged_pool_keeps_nicknames_and_gene_bits)
{
    static uint8_t chunk[DOOMO_SAVE_EXT_MAX];
    uint32_t n;
    int len;
    DoomoResident *r;
    town_new(TOWN_FREEPLAY, 31u, TOWN_POP_PREMADE);
    look_pool_reset();
    r = town_res(12);
    res_gset(r, GF_GLASSES, 3);
    resident_sanitize(r);
    /* a nickname for citizen 12 through the chunk itself: [1][id][hash][nick] */
    memset(chunk, 0, sizeof(chunk));
    n = chunk_of(chunk, sizeof(chunk));              /* the gene trailer only (no nick yet) */
    CHECK(n > 0);
    {
        uint8_t c2[1 + 16 + 2 + 8 + 2 + 8];
        uint32_t o = 0;
        c2[o++] = 1;
        c2[o++] = 12; c2[o++] = 0; c2[o++] = 0; c2[o++] = 0;       /* hash 0 never matches: the table keeps the entry */
        memset(c2 + o, 0, 12); memcpy(c2 + o, "Nicky", 5); o += 12;
        c2[o++] = 1; c2[o++] = 0;                                  /* one gene entry (id 12, its 6 bytes) */
        memcpy(c2 + o, chunk + (n - 8), 8); o += 8;
        c2[o++] = 40; c2[o++] = 0;                                 /* a pool length that is wrong */
        c2[o++] = 1; c2[o++] = 2; c2[o++] = 3; c2[o++] = 4; c2[o++] = 5; c2[o++] = 6; c2[o++] = 7; c2[o++] = 8;
        res_gset(r, GF_GLASSES, 0);                                /* the gene bits come back from the chunk */
        CHECK(look_pool_put(3, RUN_DEEP, RUN_A, 4));
        CHECK(look_ext()->read(c2, o, 2));                         /* not rejected */
        CHECK_EQ(res_gget(town_res(12), GF_GLASSES), 3u);          /* the gene bits stayed */
        CHECK(look_pool_find(3, RUN_DEEP, &len) == NULL);          /* the pool is empty */
        /* and the nickname table still has its entry (written back, the nick count is 1) */
        CHECK_EQ(look_ext()->write(chunk, sizeof(chunk)) > 0 ? chunk[0] : 255, 1);
    }
    look_pool_reset();
}
