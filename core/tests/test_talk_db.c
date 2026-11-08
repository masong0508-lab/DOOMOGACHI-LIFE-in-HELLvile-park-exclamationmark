/* test_talk_db.c - T2: the Talk database reader (core/src/talk_db.c) over
 * the compiled fixture (core/gen/talk_fixture.c, talkc.py) and the real
 * TALK_en.BIN the native runner reads (make -C core talk, which `unit`
 * depends on). Tool -> reader round trip, forms and articles, categories,
 * associations, the safety classes the gate will read, CRC and
 * dictionary refusals, bounds. Owner: TK. docs/TALK.md.
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/txt.h"
#include "doomo/lang.h"
#include "doomo/plat_ext.h"

extern const uint32_t doomo_talk_fixture_size;
extern const uint32_t doomo_talk_fixture[];
extern const uint16_t doomo_talk_fixture_nkeys;
extern const char *const doomo_talk_fixture_keys[];

static const uint8_t *fx(void) { return (const uint8_t *)doomo_talk_fixture; }

/* the fixture's id of a key (its order in fixture.txt), 0 if absent */
static int fid(const char *key)
{
    int i;
    for (i = 0; i < doomo_talk_fixture_nkeys; i++)
        if (strcmp(doomo_talk_fixture_keys[i], key) == 0) return i + 1;
    return 0;
}

static const char *form(const char *key, int which)
{
    static char buf[4][96];
    static int k;
    char *b = buf[k++ & 3];
    if (talk_db_form(fid(key), which, b, 96) < 0) return "<none>";
    return b;
}

static int open_fixture(void)
{
    return talk_db_open_mem(fx(), doomo_talk_fixture_size);
}

TEST(talk_db_fixture_opens_and_verifies)
{
    uint32_t off, packed, raw;
    CHECK_EQ(open_fixture(), TALK_DB_OK);
    CHECK(talk_db_is_open());
    CHECK_EQ(talk_db_nconcepts(), doomo_talk_fixture_nkeys);
    CHECK_EQ(talk_db_lang(), 0);
    CHECK_EQ(talk_db_verify(), TALK_DB_OK);
    /* every bank T1 writes is there; the reserved ones are not yet */
    CHECK(talk_db_bank(TB_CONCEPT, &off, &packed, &raw) && off % TALK_ALIGN == 0 &&
          packed == 8u * (doomo_talk_fixture_nkeys + 1u));
    CHECK(talk_db_bank(TB_ASSOC, NULL, &packed, NULL) && packed == 16u * (doomo_talk_fixture_nkeys + 1u));
    CHECK(talk_db_bank(TB_ATTR, NULL, NULL, NULL));
    CHECK(talk_db_bank(TB_FORMS, NULL, &packed, &raw) && packed > 0 && raw > 0);
    CHECK(talk_db_bank(TB_CATS, NULL, NULL, NULL) && talk_db_bank(TB_CATNAMES, NULL, NULL, NULL));
    CHECK(talk_db_bank(TB_EDGES, NULL, NULL, NULL));
    CHECK(!talk_db_bank(TB_GRAMMAR, NULL, NULL, NULL) && !talk_db_bank(TB_PARSE, NULL, NULL, NULL));
    /* the fixture was built with this build's story codec (make gen keeps it so) */
    CHECK_EQ_U(((const uint32_t *)fx())[2], txt_codec_hash());
    talk_db_close();
    CHECK(!talk_db_is_open());
    CHECK_EQ(talk_db_nconcepts(), 0);
}

TEST(talk_db_forms_and_articles)
{
    CHECK_EQ(open_fixture(), TALK_DB_OK);
    CHECK_STR(form("pie", TF_LEMMA), "pie");
    CHECK_STR(form("pie", TF_PLURAL), "pies");
    CHECK_STR(form("pie", TF_DEFAULT), "pie");
    CHECK_STR(form("pie", TF_INDEF), "a pie");
    CHECK_STR(form("pie", TF_DEF), "the pie");
    CHECK_STR(form("owl", TF_INDEF), "an owl");
    CHECK_STR(form("apple", TF_INDEF), "an apple");
    /* mass: no plural, "some" */
    CHECK_STR(form("butter", TF_PLURAL), "butter");
    CHECK_STR(form("butter", TF_INDEF), "some butter");
    CHECK_STR(form("tea", TF_INDEF), "some tea");
    /* usually plural (a catalog food) */
    CHECK_STR(form("pancake", TF_LEMMA), "pancake");
    CHECK_STR(form("pancake", TF_DEFAULT), "pancakes");
    CHECK_STR(form("pancake", TF_INDEF), "some pancakes");
    CHECK_STR(form("pancake", TF_DEF), "the pancakes");
    /* irregular plurals */
    CHECK_STR(form("sheep", TF_PLURAL), "sheep");
    CHECK_STR(form("mouse", TF_PLURAL), "mice");
    CHECK_STR(form("child", TF_PLURAL), "children");
    /* names keep their capitals and take no article; "the" names keep it */
    CHECK_STR(form("nana_puddin", TF_INDEF), "Nana Puddin");
    CHECK_STR(form("nana_puddin", TF_DEF), "Nana Puddin");
    CHECK_STR(form("sunny", TF_DEFAULT), "Sunny");
    CHECK_STR(form("feast", TF_DEF), "the Feast");
    CHECK_STR(form("hell_gate", TF_INDEF), "the hell gate");
    CHECK_STR(form("dark", TF_INDEF), "the dark");
    CHECK_STR(form("monday", TF_LEMMA), "Monday");
    CHECK_STR(form("teddy_bear", TF_INDEF), "a teddy bear");
    /* bounds */
    {
        char b[8];
        CHECK_EQ(talk_db_form(0, TF_LEMMA, b, sizeof b), -1);
        CHECK_EQ(talk_db_form(talk_db_nconcepts() + 1, TF_LEMMA, b, sizeof b), -1);
        CHECK_EQ(talk_db_form(fid("pie"), 99, b, sizeof b), -1);
        /* a short buffer truncates, always NUL-terminated */
        CHECK_EQ(talk_db_form(fid("teddy_bear"), TF_INDEF, b, 5), 4);
        CHECK_STR(b, "a te");
    }
    talk_db_close();
}

TEST(talk_db_concepts_categories_and_classes)
{
    TalkConcept c;
    char name[32];
    CHECK_EQ(open_fixture(), TALK_DB_OK);
    CHECK(talk_db_concept(fid("pie"), &c));
    CHECK_EQ(c.id, fid("pie"));
    CHECK_EQ(c.sent, 2);
    CHECK_EQ(c.reg, TR_CLEAN);
    CHECK_EQ(c.age, TAGE_ANY);
    CHECK(c.flags & TCF_COUNTABLE);
    CHECK(!(c.flags & (TCF_MASS | TCF_PROPER | TCF_GENERATED)));
    CHECK(talk_db_cat_name(c.cat, name, sizeof name) > 0);
    CHECK_STR(name, "food.dish");
    CHECK_EQ(talk_db_cat_root(c.cat), TCR_FOOD);
    /* the roots are the TCR_* ids, in order */
    CHECK(talk_db_cat_name(TCR_PEOPLE, name, sizeof name) > 0); CHECK_STR(name, "people");
    CHECK(talk_db_cat_name(TCR_SAFETY, name, sizeof name) > 0); CHECK_STR(name, "safety");
    CHECK(talk_db_cat_name(TCR_STUFF, name, sizeof name) > 0); CHECK_STR(name, "stuff");
    CHECK_EQ(talk_db_cat_root(TCR_ROMANCE), TCR_ROMANCE);
    {
        uint8_t parent = 9, fl = 0;
        CHECK(talk_db_cat(TCR_ROMANCE, &parent, &fl));
        CHECK_EQ(parent, 0);
        CHECK(fl & TCAT_GATED);
        CHECK(talk_db_cat(TCR_SAFETY, &parent, &fl) && (fl & TCAT_SAFETY) && !(fl & TCAT_TALKABLE));
    }
    CHECK(talk_db_ncats() > TCR_COUNT);
    CHECK(!talk_db_cat(0, NULL, NULL) && !talk_db_cat(talk_db_ncats() + 1, NULL, NULL));
    /* generated rows say so; the cast are proper names */
    CHECK(talk_db_concept(fid("pancake"), &c) && (c.flags & TCF_GENERATED) && (c.flags & TCF_PLURAL));
    CHECK(talk_db_concept(fid("grumbleguts"), &c) && (c.flags & TCF_PROPER) && (c.flags & TCF_GENERATED));
    CHECK_EQ(talk_db_cat_root(c.cat), TCR_LORE);
    /* the classes the T4 gate reads: what a kid's room may never hear */
    CHECK(talk_db_concept(fid("sex"), &c) && c.reg == TR_SEXUAL && c.age == TAGE_NEVER);
    CHECK(!(c.flags & TCF_COUNTABLE));             /* safety words get no made-up plurals */
    CHECK_STR(form("sex", TF_PLURAL), "sex");
    CHECK_EQ(talk_db_cat_root(c.cat), TCR_SAFETY);
    CHECK(talk_db_concept(fid("fuck"), &c) && c.reg == TR_STRONG && c.age == TAGE_NEVER);
    CHECK(talk_db_concept(fid("damn"), &c) && c.reg == TR_STRONG && c.age == TAGE_ADULT);
    CHECK(talk_db_concept(fid("booze"), &c) && c.reg == TR_CRUDE && c.age == TAGE_ADULT);
    CHECK(talk_db_concept(fid("wedding"), &c) && c.age == TAGE_ADULT && talk_db_cat_root(c.cat) == TCR_ROMANCE);
    CHECK(talk_db_concept(fid("kid"), &c) && (c.flags & TCF_MINOR_WORD) && c.age == TAGE_ANY);
    CHECK(talk_db_concept(fid("child"), &c) && (c.flags & TCF_MINOR_WORD));
    CHECK(talk_db_concept(fid("happy"), &c) && (c.flags & TCF_GENERATED));
    CHECK(talk_db_concept(fid("pie"), &c) && !(c.flags & TCF_MINOR_WORD));
    CHECK(!talk_db_concept(0, &c) && !talk_db_concept(talk_db_nconcepts() + 1, &c) && !talk_db_concept(1, NULL));
    talk_db_close();
}

TEST(talk_db_attributes_and_associations)
{
    uint16_t ids[16];
    uint8_t w[16];
    char name[48];
    int n, i, j, found;
    TalkConcept a, b;
    CHECK_EQ(open_fixture(), TALK_DB_OK);
    /* pie: warm, flaky, baked, stolen (food.tsv) */
    n = talk_db_attrs(fid("pie"), ids);
    CHECK_EQ(n, 4);
    {
        static const char *const want[] = { "warm", "flaky", "baked", "stolen" };
        for (i = 0; i < n && i < 4; i++) {
            CHECK(talk_db_form(ids[i], TF_LEMMA, name, sizeof name) > 0);
            CHECK_STR(name, want[i]);
            CHECK(talk_db_concept(ids[i], &a));
            CHECK_EQ(talk_db_cat_root(a.cat), TCR_DESCRIBE);
        }
    }
    /* a generated food with no attrs of its own takes its leaf's defaults */
    CHECK(talk_db_attrs(fid("pancake"), ids) == 3);
    /* pie -> nana_puddin is a hand edge at full weight, kept in the fixture */
    n = talk_db_assoc(fid("pie"), ids, w);
    CHECK(n >= 1 && n <= TALK_ASSOC_TOP);
    found = 0;
    for (i = 0; i < n; i++) if (ids[i] == fid("nana_puddin")) { found = 1; CHECK_EQ(w[i], 15); }
    CHECK(found);
    /* every association in the fixture: in range, strongest first, a talkable
     * concept, and never from a concept anyone may hear into an adults-only one */
    for (i = 1; i <= talk_db_nconcepts(); i++) {
        n = talk_db_assoc(i, ids, w);
        for (j = 0; j < n; j++) {
            uint8_t fl = 0, rfl = 0;
            CHECK(ids[j] >= 1 && ids[j] <= talk_db_nconcepts() && ids[j] != i);
            CHECK(w[j] >= 1 && w[j] <= 15);
            if (j) CHECK(w[j] <= w[j - 1]);
            CHECK(talk_db_concept(i, &a) && talk_db_concept(ids[j], &b));
            talk_db_cat(b.cat, NULL, &fl);
            talk_db_cat(talk_db_cat_root(b.cat), NULL, &rfl);
            CHECK_MSG(b.age != TAGE_NEVER && (fl & TCAT_TALKABLE), "edge %d -> %d into a non-talkable", i, ids[j]);
            if (b.age == TAGE_ADULT || (rfl & TCAT_GATED))
                CHECK_MSG(a.age == TAGE_ADULT, "edge %d -> %d into an adults-only concept", i, ids[j]);
        }
        /* the full edge list (DC) begins with the top 8 */
        {
            uint16_t e[16];
            uint8_t ew[16];
            int m = talk_db_edges(i, e, ew, 16), k;
            CHECK(m >= n);
            for (k = 0; k < n; k++) CHECK(e[k] == ids[k] && ew[k] == w[k]);
        }
    }
    CHECK_EQ(talk_db_assoc(0, ids, w), 0);
    CHECK_EQ(talk_db_attrs(talk_db_nconcepts() + 1, ids), 0);
    talk_db_close();
}

/* a corrupted, foreign or truncated file is refused, and nothing reads */
TEST(talk_db_refuses_bad_files)
{
    static uint8_t img[1u << 16];
    uint32_t size = doomo_talk_fixture_size, off, packed;
    TalkConcept c;
    CHECK(size <= sizeof img);
    /* a flipped byte in a bank: opens (directory fine), fails verify */
    memcpy(img, fx(), size);
    CHECK_EQ(talk_db_open_mem(img, size), TALK_DB_OK);
    CHECK(talk_db_bank(TB_FORMS, &off, &packed, NULL));
    img[off + packed / 2] ^= 0x40;
    CHECK_EQ(talk_db_verify(), TALK_DB_BAD_CRC);
    talk_db_close();
    memcpy(img, fx(), size);
    img[0] ^= 1;
    CHECK_EQ(talk_db_open_mem(img, size), TALK_DB_BAD_MAGIC);
    CHECK(!talk_db_is_open() && !talk_db_concept(1, &c));
    memcpy(img, fx(), size);
    img[4] = 99;
    CHECK_EQ(talk_db_open_mem(img, size), TALK_DB_BAD_VERSION);
    /* built with another story dictionary */
    memcpy(img, fx(), size);
    img[8] ^= 0x5A;
    CHECK_EQ(talk_db_open_mem(img, size), TALK_DB_DICT_MISMATCH);
    /* truncated (the header's total no longer matches) */
    memcpy(img, fx(), size);
    CHECK_EQ(talk_db_open_mem(img, size - 64), TALK_DB_BAD_DIR);
    CHECK_EQ(talk_db_open_mem(img, 10), TALK_DB_BAD_DIR);
    /* a bank pointing past the end */
    memcpy(img, fx(), size);
    img[TALK_HDR_SIZE + 4 + 3] = 0x7F;
    CHECK_EQ(talk_db_open_mem(img, size), TALK_DB_BAD_DIR);
    /* the same bank twice */
    memcpy(img, fx(), size);
    img[TALK_HDR_SIZE + TALK_DIR_ENTRY] = img[TALK_HDR_SIZE];
    img[TALK_HDR_SIZE + TALK_DIR_ENTRY + 1] = img[TALK_HDR_SIZE + 1];
    CHECK_EQ(talk_db_open_mem(img, size), TALK_DB_BAD_DIR);
    CHECK_EQ(talk_db_open_mem(NULL, size), TALK_DB_NONE);
    /* no such language file: nothing open */
    CHECK_EQ(talk_db_open(-1), TALK_DB_NONE);
    CHECK_EQ(talk_db_verify(), TALK_DB_NONE);
}

TEST(talk_db_crc32_is_ieee)
{
    static const uint8_t s[] = "123456789";
    CHECK_EQ_U(talk_crc32(0, s, 9), 0xCBF43926u);
    /* chunked = whole */
    CHECK_EQ_U(talk_crc32(talk_crc32(0, s, 4), s + 4, 5), 0xCBF43926u);
    CHECK_EQ_U(talk_crc32(0, s, 0), 0u);
}

/* The real network through the native platform (make -C core talk): the
 * same reader on the full file, as the runner and later packages use it. */
TEST(talk_db_native_full_network)
{
    TalkConcept c;
    char buf[96];
    int i, n, pie = 0, roots[TCR_COUNT] = { 0 }, never = 0, gen = 0, edges = 0;
    uint16_t ids[8];
    CHECK_EQ(talk_db_open(lang_find("en")), TALK_DB_OK);
    CHECK_EQ(talk_db_verify(), TALK_DB_OK);
    n = talk_db_nconcepts();
    printf("      TALK_en.BIN: %d concepts, %d categories\n", n, talk_db_ncats());
    CHECK(n >= 3500 && n <= TALK_MAX_CONCEPTS);
    for (i = 1; i <= n; i++) {
        if (!talk_db_concept(i, &c)) continue;
        roots[talk_db_cat_root(c.cat)]++;
        if (c.age == TAGE_NEVER) never++;
        if (c.flags & TCF_GENERATED) gen++;
        edges += talk_db_assoc(i, ids, NULL);
        CHECK(talk_db_form(i, TF_DEFAULT, buf, sizeof buf) > 0);
        if (!pie && strcmp(buf, "pie") == 0) pie = i;
        /* every never-generated concept is a safety one, or a swear */
        if (c.age == TAGE_NEVER && talk_db_cat_root(c.cat) != TCR_SAFETY)
            CHECK_MSG(c.reg != TR_CLEAN, "concept %d '%s' is never generated but clean", i, buf);
    }
    for (i = 1; i < TCR_COUNT; i++) CHECK_MSG(roots[i] > 0, "root %d has no concept", i);
    printf("      %d generated, %d never generated, %d top associations\n", gen, never, edges);
    CHECK(gen >= 1000);
    CHECK(edges >= 15000);
    CHECK(pie > 0);
    CHECK(talk_db_attrs(pie, ids) == 4);
    CHECK(talk_db_form(ids[1], TF_LEMMA, buf, sizeof buf) > 0);
    CHECK_STR(buf, "flaky");
    talk_db_close();
    /* an unknown language has no file */
    CHECK_EQ(plat_talk_load("zz"), 0u);
}
