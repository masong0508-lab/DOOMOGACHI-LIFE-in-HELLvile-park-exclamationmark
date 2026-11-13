/* test_talk_save.c - T6: talk memory and the 'TALK' save chunk
 * (core/src/talk_save.c). Owner: TK. docs/TALK.md 6e; design 6.8.
 *
 * The table (find, touch, the last two topics, the LRU, likes, who may
 * have an entry), the day rollover (refused_today only), the flags of a
 * running conversation (seen from the next one on), the chunk (empty: 0
 * bytes; the worst case; a full save round trip; an old save; truncated,
 * too long, wrong version, a count over the cap; a removed resident, the
 * pet, a double id and a retired concept in a loaded chunk), and the
 * generator's memory guards and callback through the test grammar
 * (core/tests/talk_gen/mem.tlk), K-5 across two conversations included.
 */
#include "test.h"
#include <stdlib.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/roster.h"
#include "doomo/family.h"
#include "doomo/save.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"

#define ID_NANA  1
#define ID_RIVET 13
#define ID_VEX   666

static uint8_t *gentest;
static uint32_t gentest_size;

static int open_gentest(void)
{
    if (!gentest) {
        FILE *f = fopen(NAT_TALK_DIR "/TALK_gentest.BIN", "rb");
        long n;
        if (!f) return 0;
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        gentest = (uint8_t *)malloc((size_t)n);
        if (!gentest || fread(gentest, 1, (size_t)n, f) != (size_t)n) { fclose(f); return 0; }
        fclose(f);
        gentest_size = (uint32_t)n;
    }
    return talk_db_open_mem(gentest, gentest_size) == TALK_DB_OK && talk_gen_ready();
}

static void premade_town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
}

static uint16_t concept(const char *key)
{
    uint16_t c = 0, wf = 0;
    uint32_t tags[4];
    if (talk_lex_find(key, &c, &wf, tags) < 0) return 0;
    return c;
}

/* the n-th resident who is on the island */
static int alive(int n)
{
    int id;
    for (id = 0; id < DOOMO_PLAYER_ID; id++) {
        const DoomoResident *r = town_res(id);
        if (r && r->status != RES_EMPTY && n-- == 0) return id;
    }
    return -1;
}

static const DoomoSaveExt *ext(void) { return doomo_save_ext_talk; }

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* a chunk by hand: the header and n entries */
static uint32_t chunk(uint8_t *b, int version, int count, unsigned day)
{
    memset(b, 0, TALK_MEM_HDR + (size_t)(count > 0 ? count : 0) * TALK_MEM_ENTRY);
    b[0] = (uint8_t)version;
    b[1] = (uint8_t)count;
    put16(b + 2, day);
    return (uint32_t)(TALK_MEM_HDR + count * TALK_MEM_ENTRY);
}

static void entry(uint8_t *b, int i, int id, unsigned day, unsigned ta, unsigned tb, unsigned flags, unsigned likes)
{
    uint8_t *p = b + TALK_MEM_HDR + i * TALK_MEM_ENTRY;
    put16(p, (unsigned)id); put16(p + 2, day); put16(p + 4, ta); put16(p + 6, tb);
    p[8] = (uint8_t)flags; p[9] = (uint8_t)likes;
    put16(p + 10, 0xBEEF);                               /* spare: ignored */
}

TEST(talk_mem_registered_here)
{
    /* the native host has Talk: the chunk is in the registry, within its budget */
    int i, n = 0;
    CHECK(ext() != NULL);
    CHECK_EQ_U(ext()->tag, SAVE_EXT_TAG_TALK);
    CHECK_EQ(ext()->version, TALK_MEM_VERSION);
    CHECK_EQ(ext()->max_size, TALK_MEM_WORST);
    CHECK(ext()->max_size <= save_budget_declared_max(SAVE_EXT_TAG_TALK));
    for (i = 0; i < save_ext_count(); i++) n += save_ext_at(i) == ext();
    CHECK_EQ(n, 1);
    CHECK_EQ(sizeof(TalkMem), TALK_MEM_ENTRY);
}

TEST(talk_mem_table)
{
    const TalkMem *e;
    premade_town();
    CHECK_EQ(talk_mem_count(), 0);                       /* a new town: empty */
    CHECK(talk_mem_find(ID_NANA) == NULL);
    CHECK(talk_mem_at(0) == NULL);
    doomo_town.day = 9;
    CHECK(talk_mem_touch(ID_NANA));
    CHECK_EQ(talk_mem_count(), 1);
    e = talk_mem_find(ID_NANA);
    CHECK(e && e->id == ID_NANA && e->day == 9 && !e->topic_a && !e->flags && !e->likes_known && !e->spare);
    /* the last two topics, newest first; the same topic again changes nothing */
    talk_mem_note_topic(ID_NANA, 100);
    talk_mem_note_topic(ID_NANA, 200);
    e = talk_mem_find(ID_NANA);
    CHECK(e && e->topic_a == 200 && e->topic_b == 100);
    talk_mem_note_topic(ID_NANA, 200);
    CHECK(e->topic_a == 200 && e->topic_b == 100);
    talk_mem_note_topic(ID_NANA, 4095);
    CHECK(e->topic_a == 4095 && e->topic_b == 200);
    talk_mem_note_topic(ID_NANA, TC_NONE);               /* no topic, or not a 12-bit id: ignored */
    talk_mem_note_topic(ID_NANA, 4096);
    CHECK(e->topic_a == 4095 && e->topic_b == 200);
    /* noting makes the entry */
    talk_mem_note_topic(ID_RIVET, 7);
    CHECK_EQ(talk_mem_count(), 2);
    CHECK_EQ(talk_mem_at(0)->id, ID_RIVET);              /* the most recent first */
    CHECK_EQ(talk_mem_at(1)->id, ID_NANA);
    CHECK(talk_mem_touch(DOOMO_PLAYER_ID));              /* the hero (somebody else is played) */
    /* nobody: the pet, a bad id, an empty slot, a pool id nobody has */
    CHECK(!talk_mem_touch(TALK_PET_ID));
    CHECK(!talk_mem_touch(-1));
    CHECK(!talk_mem_touch(FAMILY_POOL_FIRST));
    CHECK(!talk_mem_touch(40000));
    talk_mem_note_topic(TALK_PET_ID, 5);
    talk_mem_set_flags(-1, TMF_INSULTED);
    talk_mem_know_like(FAMILY_POOL_FIRST, 0);
    CHECK_EQ(talk_mem_count(), 3);
    /* forget, and a new town */
    talk_mem_forget(ID_RIVET);
    CHECK_EQ(talk_mem_count(), 2);
    CHECK(talk_mem_find(ID_RIVET) == NULL);
    talk_mem_forget(ID_RIVET);
    CHECK_EQ(talk_mem_count(), 2);
    town_new(TOWN_FREEPLAY, 8, TOWN_POP_PREMADE);
    CHECK_EQ(talk_mem_count(), 0);
}

TEST(talk_mem_pool_member_and_removed_resident)
{
    int kid;
    premade_town();
    CHECK(family_add(ID_NANA, FAMROLE_HOUSEMATE, 0) >= 0);
    kid = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 77u, "Pim", -1);
    CHECK(kid >= FAMILY_POOL_FIRST);
    CHECK(talk_mem_touch(kid));
    talk_mem_note_topic(kid, 12);
    CHECK(talk_mem_find(kid) && talk_mem_find(kid)->topic_a == 12);
    /* the pool member leaves: the entry goes the next time anybody asks */
    CHECK(family_remove(kid));
    CHECK_EQ(talk_mem_count(), 1);
    CHECK(talk_mem_find(kid) == NULL);
    CHECK_EQ(talk_mem_count(), 0);
    /* a resident whose slot is freed */
    CHECK(talk_mem_touch(ID_RIVET));
    town_free(ID_RIVET);
    CHECK(!talk_mem_refused_today(ID_RIVET));
    CHECK_EQ(talk_mem_count(), 0);
    CHECK(!talk_mem_touch(ID_RIVET));
    talk_mem_reset();
}

TEST(talk_mem_lru_evicts_the_oldest)
{
    int i, first, second;
    premade_town();
    first = alive(0);
    second = alive(1);
    for (i = 0; i < TALK_MEM_MAX; i++) {
        CHECK(alive(i) >= 0);
        talk_mem_note_topic(alive(i), (uint16_t)(i + 1));
    }
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK_EQ(talk_mem_at(TALK_MEM_MAX - 1)->id, first);
    CHECK_EQ(ext()->size(), TALK_MEM_WORST);              /* the worst case is the full table */
    /* a 25th person: the oldest goes, nobody else */
    CHECK(talk_mem_touch(alive(TALK_MEM_MAX)));
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK(talk_mem_find(first) == NULL);
    CHECK_EQ(talk_mem_at(0)->id, alive(TALK_MEM_MAX));
    CHECK_EQ(talk_mem_at(TALK_MEM_MAX - 1)->id, second);
    for (i = 1; i < TALK_MEM_MAX; i++) {
        const TalkMem *e = talk_mem_find(alive(i));
        CHECK(e && e->topic_a == i + 1);
    }
    /* using an entry saves it: `second` is touched, so the 26th evicts alive(2) */
    talk_mem_know_like(second, 1);
    CHECK_EQ(talk_mem_at(0)->id, second);
    CHECK(talk_mem_touch(alive(TALK_MEM_MAX + 1)));
    CHECK(talk_mem_find(second) != NULL);
    CHECK(talk_mem_find(alive(2)) == NULL);
    /* a look (find, the queries) does not count as use */
    CHECK_EQ(talk_mem_at(TALK_MEM_MAX - 1)->id, alive(3));
    CHECK(talk_mem_find(alive(3)) && !talk_mem_refused_today(alive(3)) && !talk_mem_like_known(alive(3), 0));
    CHECK_EQ(talk_mem_at(TALK_MEM_MAX - 1)->id, alive(3));
    CHECK_EQ(ext()->size(), TALK_MEM_WORST);
    talk_mem_reset();
}

TEST(talk_mem_day_rollover_clears_refused_today_only)
{
    const TalkMem *e;
    premade_town();
    doomo_town.day = 20;
    talk_mem_note_topic(ID_NANA, 31);
    talk_mem_note_topic(ID_NANA, 32);
    talk_mem_know_like(ID_NANA, 0);
    talk_mem_know_like(ID_NANA, 3);
    talk_mem_set_flags(ID_NANA, TMF_REFUSED_TODAY | TMF_PROMISED);
    talk_mem_set_flags(ID_VEX, TMF_REFUSED_TODAY);
    CHECK(talk_mem_refused_today(ID_NANA) && talk_mem_refused_today(ID_VEX));
    CHECK(!talk_mem_refused_today(ID_RIVET));
    {   /* the promise becomes "last time" when the conversation ends */
        TalkConv c;
        int ids[1] = { ID_NANA };
        memset(&c, 0, sizeof c);
        c.n = 1; c.id[0] = (int16_t)ids[0]; c.spoke = 1;
        talk_mem_end(&c);
    }
    e = talk_mem_find(ID_NANA);
    CHECK(e && e->flags == (TMF_REFUSED_TODAY | TMF_PROMISED));
    /* the same day, later: still refused */
    CHECK(talk_mem_refused_today(ID_NANA));
    /* the next day: only refused_today goes */
    doomo_town.day = 21;
    CHECK(!talk_mem_refused_today(ID_NANA));
    CHECK(!talk_mem_refused_today(ID_VEX));
    e = talk_mem_find(ID_NANA);
    CHECK(e && e->flags == TMF_PROMISED);
    CHECK(e->topic_a == 32 && e->topic_b == 31 && e->likes_known == 0x09 && e->day == 20);
    CHECK_EQ(talk_mem_count(), 2);
    /* a promise kept */
    talk_mem_clear_flags(ID_NANA, TMF_PROMISED);
    CHECK_EQ(talk_mem_find(ID_NANA)->flags, 0);
    CHECK_EQ(talk_mem_find(ID_NANA)->day, 20);            /* clearing is not a talk */
    talk_mem_reset();
}

TEST(talk_mem_likes_known)
{
    int w;
    premade_town();
    for (w = 0; w < 5; w++) CHECK(!talk_mem_like_known(ID_NANA, w));
    talk_mem_know_like(ID_NANA, 1);
    talk_mem_know_like(ID_NANA, 3);
    CHECK(!talk_mem_like_known(ID_NANA, 0) && talk_mem_like_known(ID_NANA, 1));
    CHECK(!talk_mem_like_known(ID_NANA, 2) && talk_mem_like_known(ID_NANA, 3));
    CHECK(!talk_mem_like_known(ID_NANA, 4));
    talk_mem_know_like(ID_NANA, 4);
    CHECK(talk_mem_like_known(ID_NANA, 4));
    talk_mem_know_like(ID_NANA, 5);                       /* not a like */
    talk_mem_know_like(ID_NANA, -1);
    CHECK_EQ(talk_mem_find(ID_NANA)->likes_known, 0x1A);
    CHECK(!talk_mem_like_known(ID_NANA, 5) && !talk_mem_like_known(ID_NANA, -1));
    CHECK(!talk_mem_like_known(ID_RIVET, 1));
    talk_mem_reset();
}

TEST(talk_mem_chunk_empty_and_old_saves)
{
    static uint8_t img[262144];
    uint8_t b[TALK_MEM_WORST + 16];
    uint32_t n;
    premade_town();
    /* an empty table writes nothing */
    CHECK_EQ(ext()->size(), 0u);
    CHECK_EQ(ext()->write(b, sizeof b), 0u);
    n = save_serialize(img, sizeof img);
    CHECK(n > 0);
    /* a save without talk memory loads as an empty table, whatever was there */
    talk_mem_note_topic(ID_NANA, 5);
    CHECK_EQ(talk_mem_count(), 1);
    CHECK(save_deserialize(img, n));
    CHECK_EQ(talk_mem_count(), 0);
    CHECK_EQ(ext()->size(), 0u);
    /* an old save has no 'TALK' chunk at all: save.c resets a registered
     * chunk the stream lacks (test_save.c) through this */
    talk_mem_note_topic(ID_NANA, 5);
    ext()->reset();
    CHECK_EQ(talk_mem_count(), 0);
    /* the empty chunk reads as empty too */
    talk_mem_note_topic(ID_NANA, 5);
    CHECK(ext()->read(b, 0, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 0);
}

TEST(talk_mem_chunk_round_trip)
{
    static uint8_t img[262144];
    TalkMem want[TALK_MEM_MAX];
    uint8_t b[TALK_MEM_WORST + 16], b2[TALK_MEM_WORST + 16];
    uint32_t n, len;
    int i, kid;
    premade_town();
    doomo_town.day = 70000;                               /* past 16 bits: the low half is kept */
    CHECK(family_add(ID_NANA, FAMROLE_HOUSEMATE, 0) >= 0);
    kid = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 78u, "Pim", -1);
    CHECK(kid >= FAMILY_POOL_FIRST);
    talk_mem_note_topic(kid, 300);
    for (i = 0; i < TALK_MEM_MAX - 1; i++) {
        int id = alive(i);
        talk_mem_note_topic(id, (uint16_t)(1000 + i));
        talk_mem_note_topic(id, (uint16_t)(2000 + i));
        if (i % 3 == 0) talk_mem_know_like(id, i % 5);
        if (i % 4 == 0) talk_mem_set_flags(id, TMF_REFUSED_TODAY);
        if (i % 5 == 0) talk_mem_set_flags(id, TMF_COMPLIMENTED | TMF_PROMISED);
    }
    {   /* end a conversation with the last three, so their flags are saved ones */
        TalkConv c;
        memset(&c, 0, sizeof c);
        c.n = 3; c.spoke = 7;
        for (i = 0; i < 3; i++) c.id[i] = (int16_t)alive(TALK_MEM_MAX - 2 - i);
        talk_mem_end(&c);
    }
    talk_mem_set_flags(alive(0), TMF_INSULTED);           /* pending: never saved */
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    for (i = 0; i < TALK_MEM_MAX; i++) {
        want[i] = *talk_mem_at(i);
        want[i].flags &= 0x0F;
    }
    /* the chunk: the worst case, exactly */
    CHECK_EQ(ext()->size(), TALK_MEM_WORST);
    CHECK_EQ(ext()->write(b, TALK_MEM_WORST - 1), 0u);    /* too small: writes nothing */
    len = ext()->write(b, sizeof b);
    CHECK_EQ(len, TALK_MEM_WORST);
    CHECK(len <= SAVE_EXT_MAX_TALK);
    CHECK_EQ(b[0], TALK_MEM_VERSION);
    CHECK_EQ(b[1], TALK_MEM_MAX);
    CHECK_EQ(b[2] | (b[3] << 8), 70000 & 0xFFFF);
    CHECK_EQ(ext()->write(b2, sizeof b2), len);           /* twice: the same bytes (a streamed save) */
    CHECK(memcmp(b, b2, len) == 0);
    /* through a whole save */
    n = save_serialize(img, sizeof img);
    CHECK(n > 0);
    talk_mem_reset();
    town_new(TOWN_FREEPLAY, 9, TOWN_POP_PREMADE);
    CHECK(save_deserialize(img, n));
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    for (i = 0; i < TALK_MEM_MAX; i++) CHECK(memcmp(talk_mem_at(i), &want[i], sizeof(TalkMem)) == 0);
    CHECK(talk_mem_find(kid) && talk_mem_find(kid)->topic_a == 300);
    CHECK(talk_mem_refused_today(alive(0)));              /* the same town day */
    CHECK_EQ(ext()->write(b2, sizeof b2), len);
    CHECK(memcmp(b, b2, len) == 0);
    /* loaded on a later day: nobody has refused today */
    CHECK(save_deserialize(img, n));
    doomo_town.day++;
    CHECK(!talk_mem_refused_today(alive(0)));
    for (i = 0; i < TALK_MEM_MAX; i++) CHECK(!(talk_mem_at(i)->flags & TMF_REFUSED_TODAY));
    CHECK_EQ(talk_mem_find(alive(20))->flags, TMF_COMPLIMENTED | TMF_PROMISED);   /* 20: complimented, promised, refused */
    talk_mem_reset();
}

TEST(talk_mem_chunk_bad_input)
{
    uint8_t b[TALK_MEM_WORST + 64];
    uint32_t len;
    int i;
    premade_town();
    /* a good one first */
    len = chunk(b, TALK_MEM_VERSION, 2, 3);
    entry(b, 0, ID_NANA, 3, 10, 11, TMF_INSULTED, 2);
    entry(b, 1, ID_VEX, 2, 12, 0, 0, 0);
    CHECK(ext()->read(b, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 2);
    /* a later version of the chunk, or of the header: ignored */
    CHECK(!ext()->read(b, len, TALK_MEM_VERSION + 1));
    CHECK_EQ(talk_mem_count(), 0);
    CHECK(ext()->read(b, len, TALK_MEM_VERSION));
    b[0] = TALK_MEM_VERSION + 1;
    CHECK(!ext()->read(b, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 0);
    b[0] = 0;
    CHECK(!ext()->read(b, len, TALK_MEM_VERSION));
    b[0] = TALK_MEM_VERSION;
    /* truncated at every length, and too long */
    for (i = 1; i < (int)len; i++) {
        CHECK(ext()->read(b, len, TALK_MEM_VERSION));
        CHECK_MSG(!ext()->read(b, (uint32_t)i, TALK_MEM_VERSION), "length %d", i);
        CHECK_EQ(talk_mem_count(), 0);
    }
    CHECK(!ext()->read(b, len + 1, TALK_MEM_VERSION));
    CHECK(!ext()->read(b, len + TALK_MEM_ENTRY, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 0);
    /* a count over the cap, with the bytes to match (an oversized chunk) */
    memset(b, 0, sizeof b);
    len = chunk(b, TALK_MEM_VERSION, TALK_MEM_MAX + 1, 3);
    for (i = 0; i <= TALK_MEM_MAX; i++) entry(b, i, alive(i), 3, 10, 11, 0, 0);
    CHECK(len > TALK_MEM_WORST);
    CHECK(!ext()->read(b, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 0);
    b[1] = 255;
    CHECK(!ext()->read(b, TALK_MEM_WORST, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 0);
    /* a refused chunk never leaves half a table: save.c resets after a 0 */
    ext()->reset();
    CHECK_EQ(talk_mem_count(), 0);
}

TEST(talk_mem_chunk_stale_entries_are_harmless)
{
    uint8_t b[TALK_MEM_WORST];
    uint32_t len;
    const TalkMem *e;
    TalkConv c;
    TalkLine l;
    int ids[2] = { ID_NANA, ID_VEX };
    uint16_t pie;
    premade_town();
    CHECK(open_gentest());
    pie = concept("pie");
    CHECK(pie);
    doomo_town.day = 5;
    town_free(ID_RIVET);                                  /* Rivet left the island after the save */
    len = chunk(b, TALK_MEM_VERSION, 7, 5);
    entry(b, 0, ID_RIVET, 5, pie, 0, TMF_COMPLIMENTED, 1);      /* removed */
    entry(b, 1, ID_NANA, 5, 0xF000u | 4090u, pie, 0xF0u | TMF_INSULTED, 0xE0u | 2u);   /* a concept id nobody has; stray bits */
    entry(b, 2, TALK_PET_ID, 5, pie, 0, 0, 0);            /* the pet */
    entry(b, 3, FAMILY_POOL_FIRST + 3, 5, pie, 0, 0, 0);  /* a pool id nobody has */
    entry(b, 4, ID_NANA, 4, pie, pie, TMF_PROMISED, 0);   /* twice: the first stands */
    entry(b, 5, 60000, 5, pie, 0, 0, 0);                  /* nobody */
    entry(b, 6, ID_VEX, 5, 4091, 4092, 0, 0);             /* both topics unknown to the database */
    CHECK(ext()->read(b, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 2);
    CHECK(talk_mem_find(ID_RIVET) == NULL);
    e = talk_mem_find(ID_NANA);
    CHECK(e != NULL);
    CHECK_EQ(e->topic_a, 4090);                           /* 12 bits; kept: another language pack may know it */
    CHECK_EQ(e->topic_b, pie);
    CHECK_EQ(e->flags, TMF_INSULTED);
    CHECK_EQ(e->likes_known, 2);
    CHECK_EQ(e->spare, 0);
    CHECK(talk_db_nconcepts() < 4090);
    /* a conversation: the unknown topic is skipped for the one before; Vex has none */
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 77u);
    talk_mem_begin(&c);
    CHECK_EQ(c.mem_topic[0], pie);
    CHECK_EQ(c.mem_flags[0], TMF_INSULTED);
    CHECK_EQ(c.mem_topic[1], TC_NONE);
    CHECK_EQ(c.mem_flags[1], 0);
    c.plain = 1;
    CHECK(talk_gen_line(&c, 1, -1, TA_ASK_ADVICE, TC_NONE, &l, NULL));
    CHECK_STR(l.text, "Nothing yet.");
    /* and without a database nothing is remembered aloud */
    talk_db_close();
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 77u);
    talk_mem_begin(&c);
    CHECK_EQ(c.mem_topic[0], TC_NONE);
    CHECK_EQ(c.mem_flags[0], TMF_INSULTED);
    talk_mem_reset();
}

/* Q4: what the generator says about "last time" is what the table holds */
TEST(talk_mem_conversation_sees_the_memory)
{
    TalkConv c;
    TalkLine l;
    TalkTrace tr;
    char want[64], f[24];
    int ids[2] = { ID_NANA, ID_VEX }, i, ncb = 0;
    uint16_t pie, pancake;
    uint32_t h0 = talk_safe_lint_hits();
    premade_town();
    CHECK(open_gentest());
    pie = concept("pie");
    pancake = concept("pancake");
    CHECK(pie && pancake);
    /* the first conversation: nothing to remember */
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 31u);
    talk_mem_begin(&c);
    c.plain = 1;
    CHECK_EQ(c.mem_topic[0], TC_NONE);
    CHECK_EQ(c.mem_flags[0], 0);
    talk_gen_line(&c, 0, -1, TA_ASK_ADVICE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Nothing yet.");
    talk_gen_line(&c, 0, -1, TA_OFFER, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Oh, thanks.");
    /* in it: pancakes, then pie, a compliment and a promise to Nana, an insult to Vex */
    talk_mem_note_topic(ID_NANA, pancake);
    talk_mem_note_topic(ID_NANA, pie);
    talk_mem_set_flags(ID_NANA, TMF_COMPLIMENTED | TMF_PROMISED);
    talk_mem_set_flags(ID_VEX, TMF_INSULTED);
    /* "last time" is never this conversation: its own lines do not see it, nor a begin
     * without an end (the pending flags of a talk that never ended are dropped) */
    talk_gen_line(&c, 0, -1, TA_OFFER, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Oh, thanks.");
    talk_gen_line(&c, 0, -1, TA_ASK_ADVICE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Nothing yet.");
    CHECK_EQ(talk_mem_find(ID_NANA)->flags & 0x0F, 0);
    talk_mem_end(&c);
    CHECK_EQ(talk_mem_find(ID_NANA)->flags, TMF_COMPLIMENTED | TMF_PROMISED);
    CHECK_EQ(talk_mem_find(ID_VEX)->flags, TMF_INSULTED);
    /* the next conversation */
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 32u);
    talk_mem_begin(&c);
    c.plain = 1;
    CHECK_EQ(c.mem_topic[0], pie);
    CHECK_EQ(c.mem_flags[0], TMF_COMPLIMENTED | TMF_PROMISED);
    CHECK_EQ(c.mem_topic[1], TC_NONE);
    CHECK_EQ(c.mem_flags[1], TMF_INSULTED);
    CHECK_EQ(c.refused, 0);
    talk_db_form(pie, TF_DEFAULT, f, sizeof f);
    snprintf(want, sizeof want, "Last time: %s.", f);
    talk_gen_line(&c, 0, -1, TA_ASK_ADVICE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, want);
    talk_gen_line(&c, 1, -1, TA_ASK_ADVICE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Nothing yet.");
    talk_gen_line(&c, 0, -1, TA_OFFER, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "You were kind last time.");
    talk_gen_line(&c, 1, -1, TA_OFFER, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Oh, thanks.");
    talk_gen_line(&c, 0, -1, TA_PROMISE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "You promised last time.");
    talk_gen_line(&c, 1, -1, TA_PROMISE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "A first promise.");
    talk_gen_line(&c, 0, -1, TA_APOLOGIZE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Sorry for what?");
    talk_gen_line(&c, 1, -1, TA_APOLOGIZE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Still cross about last time.");
    /* the callback part: when it is the memory one, its value is the remembered topic */
    c.plain = 0;
    snprintf(want, sizeof want, "Again %s.", f);
    for (i = 0; i < 300; i++) {
        talk_gen_line(&c, 0, -1, TA_COMPLIMENT, TC_NONE, &l, &tr);
        if (tr.cb != TCB_MEMORY) continue;
        ncb++;
        CHECK_EQ(tr.cb_val, pie);
        CHECK_MSG(strstr(l.text, want) != NULL, "%s", l.text);
    }
    CHECK(ncb >= 3);
    /* nobody is complimented in this one: after it Nana is no longer "complimented
     * last time"; the promise stands until it is cleared; Vex said nothing and keeps hers */
    c.spoke = 1;
    talk_mem_end(&c);
    CHECK_EQ(talk_mem_find(ID_NANA)->flags, TMF_PROMISED);
    CHECK_EQ(talk_mem_find(ID_VEX)->flags, TMF_INSULTED);
    CHECK_EQ(talk_mem_at(0)->id, ID_NANA);
    talk_mem_clear_flags(ID_NANA, TMF_PROMISED);
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 33u);
    talk_mem_begin(&c);
    CHECK_EQ(c.mem_flags[0], 0);
    CHECK_EQ(c.mem_topic[0], pie);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
    talk_mem_reset();
}

/* K-5 across conversations: a no holds for the town day, through a save too */
TEST(talk_mem_refusal_holds_for_the_day)
{
    static uint8_t img[262144];
    TalkConv c;
    TalkUtter u;
    TalkExchange x;
    uint32_t n;
    int ids[1], avatar = roster_find("Mayor Giggs"), steamy = roster_find("Steamy");
    premade_town();
    CHECK(open_gentest());
    CHECK(avatar >= 0 && steamy >= 0);
    ids[0] = steamy;
    doomo_town.day = 11;
    talk_gen_begin(&c, avatar, ids, 1, 99u);
    talk_mem_begin(&c);
    c.plain = 1;
    c.consent = 0;
    memset(&u, 0, sizeof u);
    u.act[0] = TA_FLIRT;
    CHECK_EQ(talk_gen_exchange(&c, &u, &x, NULL), 1);
    CHECK_EQ(x.gate, TG_PASS);
    CHECK_STR(x.line[0].text, "No thanks.");
    CHECK(c.refused & 1);
    talk_mem_end(&c);
    CHECK(talk_mem_refused_today(steamy));
    n = save_serialize(img, sizeof img);
    CHECK(n > 0);
    talk_mem_reset();
    CHECK(save_deserialize(img, n));
    /* a new conversation the same day, even one where they would say yes: the gate deflects */
    talk_gen_begin(&c, avatar, ids, 1, 100u);
    talk_mem_begin(&c);
    c.plain = 1;
    c.consent = 1;
    CHECK(c.refused & 1);
    memset(&u, 0, sizeof u);
    u.act[0] = TA_FLIRT;
    talk_gen_exchange(&c, &u, &x, NULL);
    CHECK_EQ(x.gate, TG_DEFLECT);
    CHECK_STR(x.line[0].text, "Not today.");
    talk_mem_end(&c);
    /* the next town day it is a question again */
    doomo_town.day = 12;
    talk_gen_begin(&c, avatar, ids, 1, 101u);
    talk_mem_begin(&c);
    c.plain = 1;
    c.consent = 1;
    CHECK_EQ(c.refused, 0);
    memset(&u, 0, sizeof u);
    u.act[0] = TA_FLIRT;
    talk_gen_exchange(&c, &u, &x, NULL);
    CHECK_EQ(x.gate, TG_PASS);
    CHECK_STR(x.line[0].text, "Oh! Hello.");
    talk_mem_end(&c);
    CHECK(!talk_mem_refused_today(steamy));
    /* a K-7 end locks the whole room for the day */
    {
        int two[2] = { ID_NANA, ID_VEX };
        talk_gen_begin(&c, DOOMO_PLAYER_ID, two, 2, 5u);
        talk_mem_begin(&c);
        c.ended = 1;
        talk_mem_end(&c);
        CHECK(talk_mem_refused_today(ID_NANA) && talk_mem_refused_today(ID_VEX));
        talk_gen_begin(&c, DOOMO_PLAYER_ID, two, 2, 6u);
        talk_mem_begin(&c);
        CHECK_EQ(c.refused, 3);
    }
    talk_db_close();
    talk_mem_reset();
}
