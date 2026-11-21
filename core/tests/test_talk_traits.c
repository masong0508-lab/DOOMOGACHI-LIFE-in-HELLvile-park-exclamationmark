/* test_talk_traits.c - TM1-TRAITS: the trait model and the personality API
 * (core/src/talk_traits.c, doomo/talk_traits.h; docs/TALK.md "Traits and the
 * personality API"). Owner: TALK.
 *
 *   the table, editing, validation, pack / unpack
 *   the default set (a pure function of the record: deterministic, valid, spread)
 *   explicit sets, the 'TALK' chunk tail, an old chunk, a bad tail, a gone person
 *   the one-line read
 *   the bias: the safety sweep (every trait x every life stage x every act x every
 *     topic root) and what each trait leans to
 *   the hooks: stance on a liked / disliked root, the drama manager's take value
 *   the live preview on a character that does not exist yet
 */
#include "test.h"
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/talk_traits.h"
#include "doomo/lang.h"
#include "doomo/sim.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/resident_acc.h"

#define ID_NANA 1
#define ID_RIVET 13

static void premade_town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
}

static int open_en(void) { return talk_db_open(lang_find("en")) == TALK_DB_OK && talk_gen_ready(); }

static const DoomoSaveExt *ext(void) { return doomo_save_ext_talk; }

/* ---- the table -------------------------------------------------------------------- */
TEST(traits_table)
{
    int t, u;
    CHECK_EQ(TT_COUNT, 25);                                  /* 24 traits and none: 5 bits */
    CHECK_STR(talk_trait_name(TT_NONE), "");
    for (t = 1; t < TT_COUNT; t++) {
        CHECK_MSG(talk_trait_name(t)[0] && talk_trait_desc(t)[0] && talk_trait_adj(t)[0], "trait %d has no text", t);
        CHECK_MSG(strlen(talk_trait_desc(t)) < 100, "trait %d description too long", t);
        for (u = t + 1; u < TT_COUNT; u++) CHECK_MSG(strcmp(talk_trait_name(t), talk_trait_name(u)) != 0, "twin names");
        CHECK(!talk_trait_conflict(t, t));
        for (u = 1; u < TT_COUNT; u++) CHECK_EQ(talk_trait_conflict(t, u), talk_trait_conflict(u, t));
    }
    CHECK(talk_trait_conflict(TT_KIND, TT_MEAN));
    CHECK(talk_trait_conflict(TT_CHEERFUL, TT_GLOOMY));
    CHECK(talk_trait_conflict(TT_NEAT, TT_SLOB));
    CHECK(!talk_trait_conflict(TT_KIND, TT_SHY));
    CHECK_STR(talk_trait_name(99), "");
    CHECK(talk_trait_is_romance(TT_ROMANTIC) && talk_trait_is_romance(TT_JEALOUS) && !talk_trait_is_romance(TT_MEAN));
    CHECK(talk_trait_inert(TT_ROMANTIC, LIFE_CHILD) && talk_trait_inert(TT_JEALOUS, LIFE_TEEN));
    CHECK(!talk_trait_inert(TT_ROMANTIC, LIFE_ADULT) && !talk_trait_inert(TT_ROMANTIC, LIFE_ELDER));
    CHECK(!talk_trait_inert(TT_KIND, LIFE_BABY));
    CHECK(talk_trait_topic_ok(TCR_FOOD, LIFE_CHILD) && !talk_trait_topic_ok(TCR_ROMANCE, LIFE_TEEN) &&
          talk_trait_topic_ok(TCR_ROMANCE, LIFE_ADULT) && !talk_trait_topic_ok(0, LIFE_ADULT) &&
          !talk_trait_topic_ok(16, LIFE_ADULT));
}

/* ---- editing, validation, pack ----------------------------------------------------------- */
TEST(traits_edit_and_validate)
{
    TalkTraitSet ts, o;
    uint32_t pk;
    int i;
    talk_traitset_clear(&ts);
    CHECK_EQ(talk_traitset_count(&ts), 0);
    CHECK_EQ(talk_trait_validate(&ts, LIFE_ADULT), TTE_OK);
    CHECK_EQ(talk_traitset_add(&ts, TT_KIND), TTE_OK);
    CHECK_EQ(talk_traitset_add(&ts, TT_KIND), TTE_DUP);
    CHECK_EQ(talk_traitset_add(&ts, TT_MEAN), TTE_CONFLICT);           /* kind + mean is impossible */
    CHECK_EQ(talk_traitset_add(&ts, 0), TTE_RANGE);
    CHECK_EQ(talk_traitset_add(&ts, TT_COUNT), TTE_RANGE);
    CHECK_EQ(talk_traitset_add(&ts, TT_SHY), TTE_OK);
    CHECK_EQ(talk_traitset_add(&ts, TT_BOOKWORM), TTE_OK);
    CHECK_EQ(talk_traitset_add(&ts, TT_CREATIVE), TTE_FULL);           /* at most three */
    CHECK_EQ(talk_traitset_count(&ts), 3);
    CHECK(talk_traitset_has(&ts, TT_SHY) && !talk_traitset_has(&ts, TT_CREATIVE));
    talk_traitset_remove(&ts, TT_KIND);
    CHECK(ts.trait[0] == TT_SHY && ts.trait[1] == TT_BOOKWORM && ts.trait[2] == 0);   /* the gap closes */
    /* topics */
    CHECK_EQ(talk_traitset_like(&ts, TCR_FOOD), TTE_OK);
    CHECK_EQ(talk_traitset_like(&ts, TCR_FOOD), TTE_DUP);
    CHECK_EQ(talk_traitset_dislike(&ts, TCR_FOOD), TTE_TOPIC_BOTH);
    CHECK_EQ(talk_traitset_like(&ts, TCR_MUSIC), TTE_OK);
    CHECK_EQ(talk_traitset_like(&ts, TCR_WORK), TTE_FULL);
    CHECK_EQ(talk_traitset_like(&ts, 0), TTE_RANGE);
    CHECK_EQ(talk_traitset_dislike(&ts, 16), TTE_RANGE);
    CHECK_EQ(talk_traitset_dislike(&ts, TCR_WORK), TTE_OK);
    CHECK_EQ(talk_trait_validate(&ts, LIFE_ADULT), TTE_OK);
    talk_traitset_forget_topic(&ts, TCR_FOOD);
    CHECK(ts.like[0] == TCR_MUSIC && ts.like[1] == 0);
    /* the validator names each problem */
    o = ts; o.trait[0] = 0; CHECK_EQ(talk_trait_validate(&o, LIFE_ADULT), TTE_GAP);
    o = ts; o.trait[2] = TT_SHY; CHECK_EQ(talk_trait_validate(&o, LIFE_ADULT), TTE_DUP);
    o = ts; o.trait[0] = TT_KIND; o.trait[1] = TT_MEAN; CHECK_EQ(talk_trait_validate(&o, LIFE_ADULT), TTE_CONFLICT);
    o = ts; o.trait[2] = 77; CHECK_EQ(talk_trait_validate(&o, LIFE_ADULT), TTE_RANGE);
    o = ts; o.dislike[0] = o.like[0]; CHECK_EQ(talk_trait_validate(&o, LIFE_ADULT), TTE_TOPIC_BOTH);
    o = ts; o.like[0] = TCR_ROMANCE;
    CHECK_EQ(talk_trait_validate(&o, LIFE_ADULT), TTE_OK);
    CHECK_EQ(talk_trait_validate(&o, LIFE_TEEN), TTE_RANGE);          /* no romance topic for a minor */
    /* normalise makes anything valid, and leaves a valid set alone */
    CHECK_EQ(talk_trait_normalize(&ts, LIFE_ADULT), 0);
    memset(&o, 0xFF, sizeof o);
    CHECK(talk_trait_normalize(&o, LIFE_ADULT) > 0);
    CHECK_EQ(talk_trait_validate(&o, LIFE_ADULT), TTE_OK);
    memset(&o, 0, sizeof o); o.trait[0] = TT_KIND; o.trait[1] = TT_MEAN; o.trait[2] = TT_SHY;
    CHECK(talk_trait_normalize(&o, LIFE_ADULT) > 0);
    CHECK(o.trait[0] == TT_KIND && o.trait[1] == TT_SHY && o.trait[2] == 0);
    /* pack: 31 bits, every field survives */
    talk_traitset_clear(&ts);
    talk_traitset_add(&ts, TT_HONEST); talk_traitset_add(&ts, TT_CURIOUS); talk_traitset_add(&ts, TT_GLUTTON);
    talk_traitset_like(&ts, 15); talk_traitset_like(&ts, 1); talk_traitset_dislike(&ts, 7); talk_traitset_dislike(&ts, 2);
    pk = talk_traitset_pack(&ts);
    CHECK((pk >> 31) == 0);
    talk_traitset_unpack(pk, &o);
    CHECK(memcmp(&o, &ts, sizeof o) == 0);
    /* unpack never gives an invalid set, whatever the bits */
    for (i = 0; i < 4000; i++) {
        talk_traitset_unpack(doomo_hash32((uint32_t)i) & 0x7FFFFFFFu, &o);
        CHECK_MSG(talk_trait_validate(&o, LIFE_ADULT) == TTE_OK, "unpack %d gave an invalid set", i);
    }
}

/* ---- the default set: a pure function of the record ----------------------------------------- */
TEST(traits_default_is_pure_valid_and_spread)
{
    int seen[TT_COUNT], i, n = 0, stage;
    memset(seen, 0, sizeof seen);
    for (i = 0; i < 600; i++) {
        DoomoResident a, b;
        TalkTraitSet ta, tb;
        resident_generate(&a, 1000u + (uint32_t)i);
        b = a;
        talk_trait_default(&a, &ta);
        talk_trait_default(&b, &tb);
        CHECK(memcmp(&ta, &tb, sizeof ta) == 0);                       /* the same record, the same set */
        CHECK_EQ(talk_trait_validate(&ta, RES_STAGE(&a)), TTE_OK);
        CHECK_EQ(talk_traitset_count(&ta), 2);
        CHECK(ta.like[0] && ta.dislike[0]);
        seen[ta.trait[0]]++;
        seen[ta.trait[1]]++;
    }
    for (i = 1; i < TT_COUNT; i++) n += seen[i] > 0;
    CHECK_MSG(n >= 20, "only %d of 24 traits ever come out of the default", n);
    /* the sliders pull: a blunt, serious, quick person vs a gentle, easygoing, slow one */
    {
        DoomoResident r;
        TalkTraitSet t;
        int hot = 0, gentle = 0;
        for (i = 0; i < 200; i++) {
            resident_generate(&r, 5000u + (uint32_t)i);
            RES_SET_STAGE(&r, LIFE_ADULT);
            RES_SET_PERSONA(&r, PERS_MOVEMENT, 14); RES_SET_PERSONA(&r, PERS_SPEECH, 15);
            RES_SET_PERSONA(&r, PERS_EXPRESSION, 9); RES_SET_PERSONA(&r, PERS_ATTITUDE, 1);
            talk_trait_default(&r, &t);
            hot += talk_traitset_has(&t, TT_HOTHEADED) || talk_traitset_has(&t, TT_MEAN) || talk_traitset_has(&t, TT_HONEST);
            RES_SET_PERSONA(&r, PERS_MOVEMENT, 2); RES_SET_PERSONA(&r, PERS_SPEECH, 1);
            RES_SET_PERSONA(&r, PERS_EXPRESSION, 12); RES_SET_PERSONA(&r, PERS_ATTITUDE, 14);
            talk_trait_default(&r, &t);
            gentle += talk_traitset_has(&t, TT_KIND) || talk_traitset_has(&t, TT_CHEERFUL) || talk_traitset_has(&t, TT_LAZY);
        }
        CHECK_MSG(hot > 120 && gentle > 120, "sliders barely steer the default (%d, %d of 200)", hot, gentle);
    }
    /* a minor's default never has a romance trait or "mean" */
    for (stage = LIFE_BABY; stage <= LIFE_TEEN; stage++) {
        for (i = 0; i < 300; i++) {
            DoomoResident r;
            TalkTraitSet t;
            int k;
            resident_generate(&r, 9000u + (uint32_t)i);
            RES_SET_STAGE(&r, stage);
            talk_trait_default(&r, &t);
            for (k = 0; k < TALK_TRAIT_MAX; k++)
                CHECK_MSG(!talk_trait_is_romance(t.trait[k]) && t.trait[k] != TT_MEAN, "a minor got trait %d", t.trait[k]);
            CHECK(!talk_traitset_has(&t, TT_NONE) || 1);
            CHECK(t.like[0] != TCR_ROMANCE && t.dislike[0] != TCR_ROMANCE);
        }
    }
}

/* ---- explicit sets and the chunk ----------------------------------------------------------- */
static uint32_t chunk(uint8_t *b, uint32_t cap)
{
    uint32_t n = ext()->size();
    return n <= cap && ext()->write(b, cap) == n ? n : 0;
}

TEST(traits_custom_set_and_get)
{
    TalkTraitSet ts, g, d;
    DoomoResident r;
    premade_town();
    CHECK(talk_trait_get(ID_NANA, &d));                      /* the default of a premade resident */
    CHECK(!talk_trait_is_custom(ID_NANA));
    CHECK_EQ(talk_trait_custom_count(), 0);
    talk_traitset_clear(&ts);
    talk_traitset_add(&ts, TT_GOOFBALL); talk_traitset_add(&ts, TT_LOYAL);
    talk_traitset_like(&ts, TCR_MUSIC); talk_traitset_dislike(&ts, TCR_WORK);
    CHECK_EQ(talk_trait_set(ID_NANA, &ts), TTE_OK);
    CHECK(talk_trait_is_custom(ID_NANA) && talk_trait_custom_count() == 1);
    CHECK(talk_trait_get(ID_NANA, &g) && memcmp(&g, &ts, sizeof g) == 0);
    CHECK(talk_trait_get(ID_RIVET, &g));                     /* somebody else is untouched */
    CHECK(!talk_trait_is_custom(ID_RIVET));
    talk_trait_clear(ID_NANA);
    CHECK(talk_trait_get(ID_NANA, &g) && memcmp(&g, &d, sizeof g) == 0);
    CHECK_EQ(talk_trait_set(5000, &ts), TTE_NOBODY);          /* nobody has that id */
    CHECK_EQ(talk_trait_set(TALK_PET_ID, &ts), TTE_NOBODY);
    CHECK(!talk_trait_get(TALK_PET_ID, &g) && talk_traitset_count(&g) == 0);
    CHECK(!talk_trait_get(TALK_PREVIEW_ID, &g));              /* no preview running */
    /* a bad set is normalised on the way in */
    memset(&ts, 0, sizeof ts); ts.trait[0] = TT_KIND; ts.trait[1] = TT_MEAN; ts.trait[2] = TT_KIND;
    CHECK_EQ(talk_trait_set(ID_NANA, &ts), TTE_OK);
    talk_trait_get(ID_NANA, &g);
    CHECK(talk_trait_validate(&g, LIFE_ADULT) == TTE_OK && g.trait[0] == TT_KIND && g.trait[1] == 0);
    /* the table is bounded */
    {
        int id, n = 0;
        talk_trait_reset();
        talk_traitset_clear(&ts);
        talk_traitset_add(&ts, TT_SHY);
        for (id = 0; id < DOOMO_PLAYER_ID && n < TALK_TRAIT_CUSTOM_MAX + 3; id++) {
            const DoomoResident *q = town_res(id);
            if (!q || q->status == RES_EMPTY) continue;
            CHECK_EQ(talk_trait_set(id, &ts), n < TALK_TRAIT_CUSTOM_MAX ? TTE_OK : TTE_FULL);
            n++;
        }
        CHECK_EQ(talk_trait_custom_count(), TALK_TRAIT_CUSTOM_MAX);
    }
    (void)r;
    talk_trait_reset();
}

TEST(traits_save_round_trip_and_old_saves)
{
    static uint8_t b[TALK_SAVE_WORST + 16], b2[TALK_SAVE_WORST + 16];
    TalkTraitSet ts, g, before_nana, before_rivet;
    uint32_t n, n2;
    premade_town();
    CHECK_EQ(ext()->size(), 0);                              /* nothing at all: the empty chunk, as before */
    talk_trait_get(ID_NANA, &before_nana);
    talk_trait_get(ID_RIVET, &before_rivet);
    /* an old chunk (memory only, version byte 1) loads and every default is unchanged */
    doomo_town.day = 5;
    talk_mem_touch(ID_RIVET);
    n = chunk(b, sizeof b);
    CHECK(n == TALK_MEM_HDR + TALK_MEM_ENTRY && b[0] == TALK_MEM_VERSION_EVENTS);
    /* Simulate a true pre-event v1 payload: the original writer used 1.
     * This must still load, while new writes explicitly use v3/v4 so
     * unused high bits in old likes never become false memories. */
    b[0] = TALK_MEM_VERSION;
    talk_mem_reset();
    CHECK(ext()->read(b, n, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 1);
    CHECK(talk_trait_get(ID_NANA, &g) && memcmp(&g, &before_nana, sizeof g) == 0);
    CHECK(talk_trait_get(ID_RIVET, &g) && memcmp(&g, &before_rivet, sizeof g) == 0);
    CHECK(!talk_trait_is_custom(ID_NANA));
    /* with explicit sets: a version 2 chunk, the same bytes on a second write, a clean round trip */
    talk_traitset_clear(&ts);
    talk_traitset_add(&ts, TT_CREATIVE); talk_traitset_add(&ts, TT_PARANOID); talk_traitset_add(&ts, TT_HONEST);
    talk_traitset_like(&ts, TCR_MUSIC); talk_traitset_like(&ts, TCR_LORE); talk_traitset_dislike(&ts, TCR_PEOPLE);
    CHECK_EQ(talk_trait_set(ID_NANA, &ts), TTE_OK);
    n = chunk(b, sizeof b);
    CHECK_EQ(n, TALK_MEM_HDR + TALK_MEM_ENTRY + 1 + 6);
    CHECK_EQ(b[0], TALK_MEM_VERSION_EVENTS_TRAITS);
    n2 = chunk(b2, sizeof b2);
    CHECK(n2 == n && memcmp(b, b2, n) == 0);
    CHECK(n <= SAVE_EXT_MAX_TALK);
    talk_mem_reset();
    CHECK_EQ(talk_trait_custom_count(), 0);
    CHECK(ext()->read(b, n, TALK_MEM_VERSION));
    CHECK(talk_trait_is_custom(ID_NANA) && talk_trait_custom_count() == 1);
    CHECK(talk_trait_get(ID_NANA, &g) && memcmp(&g, &ts, sizeof g) == 0);
    CHECK_EQ(talk_mem_count(), 1);
    n2 = chunk(b2, sizeof b2);
    CHECK(n2 == n && memcmp(b, b2, n) == 0);                  /* write(read(x)) == x */
    /* traits alone (no memory entry) are saved too */
    talk_mem_reset();
    CHECK_EQ(talk_trait_set(ID_NANA, &ts), TTE_OK);
    CHECK_EQ(ext()->size(), TALK_MEM_HDR + 1 + 6);
    n = chunk(b, sizeof b);
    talk_mem_reset();
    CHECK(ext()->read(b, n, TALK_MEM_VERSION) && talk_trait_is_custom(ID_NANA));
    /* a tail that does not fit is refused and leaves nothing half loaded */
    CHECK(!ext()->read(b, n - 1, TALK_MEM_VERSION));
    CHECK_EQ(talk_trait_custom_count(), 0);
    CHECK_EQ(talk_mem_count(), 0);
    b[TALK_MEM_HDR] = 25;                                      /* more sets than the table holds */
    CHECK(!ext()->read(b, n, TALK_MEM_VERSION));
    b[TALK_MEM_HDR] = 1;
    b[0] = TALK_MEM_VERSION;                                   /* a v1 chunk with a tail is not v1 */
    CHECK(!ext()->read(b, n, TALK_MEM_VERSION));
    b[0] = TALK_MEM_VERSION_TRAITS;
    /* a person who is gone drops out; a corrupted set loads as a valid one */
    b[TALK_MEM_HDR + 1] = 0xE7; b[TALK_MEM_HDR + 2] = 0x03;   /* id 999: not on the island */
    CHECK(ext()->read(b, n, TALK_MEM_VERSION));
    CHECK_EQ(talk_trait_custom_count(), town_res(999) && town_res(999)->status != RES_EMPTY ? 1 : 0);
    b[TALK_MEM_HDR + 1] = ID_NANA; b[TALK_MEM_HDR + 2] = 0;
    b[TALK_MEM_HDR + 3] = b[TALK_MEM_HDR + 4] = b[TALK_MEM_HDR + 5] = b[TALK_MEM_HDR + 6] = 0xFF;
    CHECK(ext()->read(b, n, TALK_MEM_VERSION));
    CHECK(talk_trait_get(ID_NANA, &g) && talk_trait_validate(&g, LIFE_ADULT) == TTE_OK);
    /* the person leaves: the set goes with them (talk_mem_forget) */
    talk_trait_set(ID_NANA, &ts);
    talk_mem_forget(ID_NANA);
    CHECK(!talk_trait_is_custom(ID_NANA));
    /* a new town clears them */
    talk_trait_set(ID_NANA, &ts);
    ext()->reset();
    CHECK_EQ(talk_trait_custom_count(), 0);
}

/* ---- the one-line read -------------------------------------------------------------------- */
TEST(traits_read_line)
{
    DoomoResident r;
    TalkTraitSet ts;
    char s[160], small[12];
    int n;
    resident_default(&r);
    RES_SET_STAGE(&r, LIFE_ADULT);
    RES_SET_PERSONA(&r, PERS_MOVEMENT, 3); RES_SET_PERSONA(&r, PERS_EXPRESSION, 3);
    RES_SET_PERSONA(&r, PERS_SPEECH, 3); RES_SET_PERSONA(&r, PERS_ATTITUDE, 3);
    CHECK_STR(talk_trait_title(&r), "Quiet Bookworm");
    talk_traitset_clear(&ts);
    talk_traitset_add(&ts, TT_SHY); talk_traitset_add(&ts, TT_KIND);
    talk_traitset_like(&ts, TCR_FOOD); talk_traitset_dislike(&ts, TCR_WORK);
    n = talk_trait_read(&r, &ts, s, sizeof s);
    CHECK_STR(s, "Quiet Bookworm: shy and kind. Loves food; avoids work.");
    CHECK_EQ(n, (int)strlen(s));
    talk_traitset_clear(&ts);
    talk_traitset_add(&ts, TT_GLUTTON); talk_traitset_add(&ts, TT_CURIOUS); talk_traitset_add(&ts, TT_LOYAL);
    talk_trait_read(&r, &ts, s, sizeof s);
    CHECK_STR(s, "Quiet Bookworm: food-loving, curious and loyal.");
    talk_traitset_clear(&ts);
    talk_trait_read(&r, &ts, s, sizeof s);
    CHECK_STR(s, "Quiet Bookworm");
    talk_trait_read(&r, NULL, s, sizeof s);                    /* the default set */
    CHECK(strncmp(s, "Quiet Bookworm: ", 16) == 0 && strlen(s) > 20);
    n = talk_trait_read(&r, NULL, small, sizeof small);       /* a short buffer: cut, NUL ended */
    CHECK(n == 11 && small[11] == 0 && strncmp(small, "Quiet Bookw", 11) == 0);
    CHECK_EQ(talk_trait_read(&r, NULL, small, 1), 0);
    CHECK_EQ(talk_trait_read(NULL, NULL, s, sizeof s), 0);
    CHECK_EQ(talk_trait_read(&r, NULL, NULL, 10), 0);
}

/* ---- the bias: safety first --------------------------------------------------------------- */
static int is_minor_stage(int st) { return LIFE_IS_MINOR(st); }

TEST(traits_bias_safety_every_trait_stage_act)
{
    int t, stage, act, root, k;
    long checked = 0;
    for (t = 1; t < TT_COUNT; t++) {
        for (stage = 0; stage < LIFE_COUNT; stage++) {
            TalkTraitSet one, empty;
            talk_traitset_clear(&one);
            talk_traitset_clear(&empty);
            talk_traitset_add(&one, t);
            talk_traitset_like(&one, TCR_ROMANCE);                /* even a liked romance topic */
            talk_traitset_dislike(&one, TCR_PEOPLE);
            for (act = TA_NONE; act < TA_COUNT; act++) {
                for (root = 0; root <= TALK_TRAIT_ROOT_MAX + 1; root++) {
                    for (k = 0; k < 2; k++) {                      /* minor_room 0 / 1 */
                        TalkTraitBias b = talk_trait_bias(&one, stage, act, root, k);
                        TalkTraitBias e = talk_trait_bias(&empty, stage, act, root, k);
                        checked++;
                        CHECK(b.accept >= 0 && b.accept <= 4 && b.deny >= 0 && b.deny <= 4);
                        CHECK(b.mood >= -3 && b.mood <= 3 && b.stance >= -3 && b.stance <= 3);
                        /* a child in the room or the speaker a child: nobody gets harsher */
                        if (k || is_minor_stage(stage)) {
                            CHECK_MSG(b.deny == 0 && b.mood >= 0, "trait %d stage %d act %d: deny %d mood %d with a child",
                                      t, stage, act, b.deny, b.mood);
                        }
                        /* a romance trait is inert for a minor: the empty set's answer, fields for field */
                        if (is_minor_stage(stage) && talk_trait_is_romance(t)) {
                            TalkTraitSet only;
                            TalkTraitBias o;
                            talk_traitset_clear(&only);
                            talk_traitset_add(&only, t);
                            o = talk_trait_bias(&only, stage, act, root, k);
                            CHECK_MSG(o.accept == e.accept && o.deny == e.deny && o.mood == e.mood && o.stance == 0,
                                      "romance trait %d moves a minor (stage %d act %d)", t, stage, act);
                        }
                        /* no romance act gets a lean in a room with a child */
                        if (k && TA_IS_ROMANCE(act)) CHECK(b.accept == 0 && b.deny == 0 && b.mood == 0);
                        /* the safety acts are never touched */
                        if (TA_IS_SAFETY(act)) CHECK(b.accept == 0 && b.deny == 0 && b.mood == 0);
                    }
                }
            }
        }
    }
    printf("      %ld trait x stage x act x topic x room cases: no trait sharpens toward a child, "
           "romance traits inert for minors\n", checked);
    /* every set of three, children: same */
    {
        int a, b2, c2;
        for (a = 1; a < TT_COUNT; a++)
            for (b2 = a + 1; b2 < TT_COUNT; b2++)
                for (c2 = b2 + 1; c2 < TT_COUNT; c2++) {
                    TalkTraitSet s3;
                    talk_traitset_clear(&s3);
                    s3.trait[0] = (uint8_t)a; s3.trait[1] = (uint8_t)b2; s3.trait[2] = (uint8_t)c2;   /* conflicts allowed here: worst case */
                    for (act = TA_GREET; act < TA_REFUSE_SEXUAL; act++) {
                        TalkTraitBias x = talk_trait_bias(&s3, LIFE_CHILD, act, TCR_FOOD, 1);
                        CHECK(x.deny == 0 && x.mood >= 0);
                        x = talk_trait_bias(&s3, LIFE_ADULT, act, TCR_FOOD, 1);
                        CHECK(x.deny == 0 && x.mood >= 0);
                    }
                }
    }
}

TEST(traits_bias_leanings)
{
    TalkTraitSet kind, mean, lazy, out, glut, hot, empty;
    TalkTraitBias k, m;
    talk_traitset_clear(&kind); talk_traitset_add(&kind, TT_KIND);
    talk_traitset_clear(&mean); talk_traitset_add(&mean, TT_MEAN);
    talk_traitset_clear(&lazy); talk_traitset_add(&lazy, TT_LAZY);
    talk_traitset_clear(&out); talk_traitset_add(&out, TT_OUTGOING);
    talk_traitset_clear(&glut); talk_traitset_add(&glut, TT_GLUTTON);
    talk_traitset_clear(&hot); talk_traitset_add(&hot, TT_HOTHEADED);
    talk_traitset_clear(&empty);
    k = talk_trait_bias(&kind, LIFE_ADULT, TA_HUG, 0, 0);
    m = talk_trait_bias(&mean, LIFE_ADULT, TA_HUG, 0, 0);
    CHECK(talk_trait_take(&k) > talk_trait_take(&m));
    CHECK(talk_trait_take(&k) >= 2 && k.mood > 0);
    CHECK(talk_trait_take(&m) < 0 && m.deny > 0);               /* a mean adult turns a hug down ... */
    m = talk_trait_bias(&mean, LIFE_ADULT, TA_HUG, 0, 1);
    CHECK_EQ(m.deny, 0);                                         /* ... but not a child's */
    k = talk_trait_bias(&lazy, LIFE_ADULT, TA_INVITE, 0, 0);
    CHECK(talk_trait_take(&k) < 0);
    k = talk_trait_bias(&out, LIFE_ADULT, TA_INVITE, 0, 0);
    CHECK(talk_trait_take(&k) > 0);
    CHECK(talk_trait_bias(&glut, LIFE_ADULT, TA_OFFER, 0, 0).accept >= 2);
    CHECK(talk_trait_bias(&hot, LIFE_ADULT, TA_INSULT, 0, 0).deny >= 2);
    CHECK(talk_trait_bias(&hot, LIFE_ADULT, TA_INSULT, 0, 0).mood < 0);
    CHECK_EQ(talk_trait_bias(&hot, LIFE_CHILD, TA_INSULT, 0, 0).mood, 0);     /* a hot-headed child is no sharper */
    k = talk_trait_bias(&empty, LIFE_ADULT, TA_INVITE, 0, 0);
    CHECK_EQ(talk_trait_take(&k), 0);
    /* the topic lean: a trait's own root, a liked root, a disliked root */
    CHECK_EQ(talk_trait_topic_lean(&glut, LIFE_ADULT, TCR_FOOD), 1);
    glut.like[0] = TCR_FOOD;
    CHECK_EQ(talk_trait_topic_lean(&glut, LIFE_ADULT, TCR_FOOD), 3);
    glut.like[0] = 0; glut.dislike[0] = TCR_FOOD;
    CHECK_EQ(talk_trait_topic_lean(&glut, LIFE_ADULT, TCR_FOOD), -1);
    CHECK_EQ(talk_trait_topic_lean(&empty, LIFE_ADULT, TCR_FOOD), 0);
    CHECK_EQ(talk_trait_topic_lean(&glut, LIFE_ADULT, 0), 0);
    empty.like[0] = TCR_ROMANCE;
    CHECK_EQ(talk_trait_topic_lean(&empty, LIFE_TEEN, TCR_ROMANCE), 0);   /* no romance topic lean for a minor */
    CHECK_EQ(talk_trait_topic_lean(&empty, LIFE_ADULT, TCR_ROMANCE), 2);
}

/* ---- the hooks: stance and the drama manager -------------------------------------------------- */
/* concepts of a root that are talkable and clean */
static int concepts_of(int root, uint16_t *out, int cap)
{
    int n = 0, i, nc = talk_db_nconcepts();
    for (i = 1; i <= nc && n < cap; i++) {
        TalkConcept tc;
        uint8_t parent = 0, flags = 0;
        if (!talk_db_concept((uint16_t)i, &tc) || !talk_db_cat(tc.cat, &parent, &flags) || !(flags & TCAT_TALKABLE)) continue;
        if (talk_db_cat_root(tc.cat) != root || tc.age != TAGE_ANY || tc.reg != TR_CLEAN || (flags & TCAT_GATED)) continue;
        out[n++] = (uint16_t)i;
    }
    return n;
}

TEST(traits_hook_stance_follows_liked_and_disliked_topics)
{
    uint16_t cs[64];
    int n, i, pro_like = 0, pro_dis = 0, con_like = 0, con_dis = 0, id;
    premade_town();
    CHECK(open_en());
    n = concepts_of(TCR_FOOD, cs, 64);
    CHECK(n >= 20);
    for (id = 0; id < 40; id++) {
        const DoomoResident *q = town_res(id);
        TalkTraitSet ts;
        if (!q || q->status == RES_EMPTY || LIFE_IS_MINOR(sim_life_stage(id))) continue;
        talk_traitset_clear(&ts);
        talk_traitset_like(&ts, TCR_FOOD);
        talk_trait_set(id, &ts);
        for (i = 0; i < n; i++) { int s = talk_gen_stance(id, cs[i]); pro_like += s == TST_PRO; con_like += s == TST_CON; }
        talk_traitset_clear(&ts);
        talk_traitset_dislike(&ts, TCR_FOOD);
        talk_trait_set(id, &ts);
        for (i = 0; i < n; i++) { int s = talk_gen_stance(id, cs[i]); pro_dis += s == TST_PRO; con_dis += s == TST_CON; }
    }
    CHECK_MSG(pro_like > pro_dis + 100 && con_dis > con_like + 100, "stance barely follows the traits (pro %d/%d con %d/%d)",
              pro_like, pro_dis, con_like, con_dis);
    talk_db_close();
    talk_trait_reset();
}

TEST(traits_hook_drama_manager_take)
{
    TalkState s;
    TalkTraitSet lazy, out;
    int ids[1], a, b, id = ID_NANA;
    premade_town();
    CHECK(open_en() && talk_dm_ready());
    ids[0] = id;
    talk_dm_begin(&s, DOOMO_PLAYER_ID, ids, 1, 99u);
    talk_traitset_clear(&lazy);
    talk_trait_set(id, &lazy);                                   /* an explicit empty set: the plain take */
    a = talk_dm_expect(&s, 0, TA_INVITE, TONE_PLAIN, 0);
    talk_traitset_add(&lazy, TT_LAZY);
    talk_traitset_clear(&out); talk_traitset_add(&out, TT_OUTGOING);
    talk_trait_set(id, &lazy);
    b = talk_dm_expect(&s, 0, TA_INVITE, TONE_PLAIN, 0);
    CHECK_EQ(b, a - 2);                                          /* lazy: an invite is turned down harder by exactly 2 */
    talk_trait_set(id, &out);
    a = talk_dm_expect(&s, 0, TA_INVITE, TONE_PLAIN, 0);
    CHECK_MSG(a > b, "an outgoing person takes an invite no better than a lazy one (%d, %d)", a, b);
    talk_trait_reset();
    talk_db_close();
}

/* ---- the live preview ---------------------------------------------------------------------- */
static void sample(DoomoResident *r, int stage, int mov, int spe, int exp, int att, int quirk)
{
    resident_default(r);
    RES_SET_STAGE(r, stage);
    RES_SET_PERSONA(r, PERS_MOVEMENT, mov); RES_SET_PERSONA(r, PERS_SPEECH, spe);
    RES_SET_PERSONA(r, PERS_EXPRESSION, exp); RES_SET_PERSONA(r, PERS_ATTITUDE, att);
    RES_SET_PERSONA(r, PERS_QUIRK, quirk);
}

TEST(traits_preview_real_engine_deterministic_and_stateless)
{
    DoomoResident r;
    TalkTraitSet ts;
    char a[TALK_LINE_MAX], b[TALK_LINE_MAX];
    int kind, i, distinct;
    char seen[20][TALK_LINE_MAX];
    uint32_t h0;
    premade_town();
    sample(&r, LIFE_ADULT, 9, 4, 12, 11, 6);
    talk_traitset_clear(&ts);
    talk_traitset_add(&ts, TT_GLUTTON); talk_traitset_add(&ts, TT_OUTGOING);
    talk_traitset_like(&ts, TCR_FOOD); talk_traitset_dislike(&ts, TCR_WORK);
    CHECK_EQ(talk_preview(&r, &ts, TP_GREETING, 1, a, sizeof a), 0);        /* no database: nothing */
    CHECK(open_en());
    h0 = talk_safe_lint_hits();
    for (kind = 0; kind < TP_KINDS; kind++) {
        int n = talk_preview(&r, &ts, kind, 77u, a, sizeof a);
        CHECK_MSG(n > 3 && n == (int)strlen(a), "preview kind %d gave \"%s\"", kind, a);
        CHECK_EQ(talk_preview(&r, &ts, kind, 77u, b, sizeof b), n);          /* same arguments, same line */
        CHECK_STR(a, b);
        distinct = 0;
        for (i = 0; i < 20; i++) {
            int j, dup = 0;
            talk_preview(&r, &ts, kind, 100u + (uint32_t)i, seen[i], TALK_LINE_MAX);
            CHECK(seen[i][0]);
            for (j = 0; j < i; j++) dup |= strcmp(seen[i], seen[j]) == 0;
            distinct += !dup;
        }
        CHECK_MSG(distinct >= 4, "preview kind %d: only %d different lines in 20 seeds", kind, distinct);
    }
    CHECK_EQ(talk_safe_lint_hits(), h0);                                     /* the real engine's lint: nothing replaced */
    CHECK(!talk_trait_get(TALK_PREVIEW_ID, &ts));                            /* nothing is left behind */
    CHECK(talk_preview_resident() == NULL);
    CHECK_EQ(talk_preview(&r, &ts, TP_KINDS, 1, a, sizeof a), 0);
    CHECK_EQ(talk_preview(NULL, &ts, TP_GREETING, 1, a, sizeof a), 0);
    CHECK_EQ(talk_preview(&r, &ts, TP_GREETING, 1, a, 1), 0);                /* no room: an empty string */
    CHECK_EQ(talk_preview(&r, NULL, TP_ANSWER, 5, a, sizeof a) > 0, 1);      /* the default set */
    CHECK_EQ(talk_preview(&r, &ts, TP_ANSWER, 5, b, 9) > 0 && strlen(b) == 8, 1);   /* cut to the buffer */
    /* the town's own state is untouched: its own residents still talk as before */
    CHECK(talk_trait_get(ID_NANA, &ts));
    talk_db_close();
}

TEST(traits_preview_traits_change_what_they_talk_about)
{
    DoomoResident r;
    TalkTraitSet food, work;
    char a[TALK_LINE_MAX], b[TALK_LINE_MAX];
    int i, differ = 0;
    premade_town();
    CHECK(open_en());
    sample(&r, LIFE_ADULT, 8, 8, 8, 8, 8);
    talk_traitset_clear(&food); talk_traitset_like(&food, TCR_FOOD);
    talk_traitset_clear(&work); talk_traitset_like(&work, TCR_HOBBIES);
    for (i = 0; i < 24; i++) {
        talk_preview(&r, &food, TP_ANSWER, 300u + (uint32_t)i, a, sizeof a);
        talk_preview(&r, &work, TP_ANSWER, 300u + (uint32_t)i, b, sizeof b);
        differ += strcmp(a, b) != 0;
    }
    CHECK_MSG(differ >= 12, "the liked topic does not steer the preview (%d of 24 differ)", differ);
    talk_db_close();
}

TEST(traits_preview_safe_for_every_trait_and_stage)
{
    int t, stage, kind, seed;
    uint32_t h0;
    long lines = 0;
    premade_town();
    CHECK(open_en());
    h0 = talk_safe_lint_hits();
    for (stage = 0; stage < LIFE_COUNT; stage++) {
        for (t = 1; t < TT_COUNT; t++) {
            DoomoResident r;
            TalkTraitSet ts;
            sample(&r, stage, 4 + t % 9, 3 + t % 11, 2 + t % 13, 1 + t % 14, t % 16);
            talk_traitset_clear(&ts);
            talk_traitset_add(&ts, t);
            talk_traitset_like(&ts, TCR_ROMANCE);                  /* a minor's romance topic is dropped, an adult's is fine */
            talk_traitset_dislike(&ts, TCR_PEOPLE);
            for (kind = 0; kind < TP_KINDS; kind++) {
                for (seed = 0; seed < 4; seed++) {
                    char line[TALK_LINE_MAX];
                    TalkAudience aud;
                    int ids[1];
                    ids[0] = TALK_PREVIEW_ID;
                    talk_preview(&r, &ts, kind, 40u + (uint32_t)seed, line, sizeof line);
                    CHECK_MSG(line[0], "empty preview: trait %d stage %d kind %d", t, stage, kind);
                    lines++;
                    /* the lint as the engine would run it for this room: a child present means kid rules */
                    memset(&aud, 0, sizeof aud);
                    aud.any_minor = (uint8_t)LIFE_IS_MINOR(stage);
                    aud.target_minor = aud.any_minor;
                    CHECK_MSG(talk_safe_lint(line, &aud, NULL) == TL_OK, "preview line fails the lint: \"%s\" (trait %d stage %d)",
                              line, t, stage);
                    (void)ids;
                }
            }
        }
    }
    CHECK_EQ(talk_safe_lint_hits(), h0);
    printf("      %ld preview lines, 24 traits x 6 life stages: the S3 lint replaced none\n", lines);
    talk_db_close();
}
