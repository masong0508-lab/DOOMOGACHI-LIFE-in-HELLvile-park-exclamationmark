/* test_talk_gen.c - T5a: the Talk generator (core/src/talk_gen.c). Owner: TK.
 * docs/TALK.md "The generator"; design 3.6 and 7.5.
 *
 * Engine (core/tests/talk_gen/gentest.tlk, built by `make -C core talk`
 * into TALK_gentest.BIN with the English lexicon): every modifier and
 * field, names, likes, the job callback, weights, novelty (Q5), sub-rules,
 * the bubble fit and its fallback, K-9 (one everyday swear), K-6 (Mature
 * variants), the cast's own lines, faces, the optional parts and the truth
 * of their callbacks (Q4), the exchange (the gate first, K-7 ends it, a
 * romance no is remembered, reactions answer the line before: Q2), the
 * custom name that may not be said, determinism.
 * The English banks (TALK_en.BIN): the property tests P1 (kid rooms), P4
 * (the pet), P5 (Mature), P6 (closed vocabulary) and P8 (the swear cap)
 * over generated lines, and Q1-Q3 on every line of a run of exchanges.
 * Every test also checks the S3 lint counted nothing.
 */
#include "test.h"
#include <stdlib.h>
#include <ctype.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/roster.h"
#include "doomo/sim.h"
#include "doomo/ending.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/portrait.h"
#include "doomo/resident_acc.h"

#define ID_NANA  1
#define ID_RIVET 13
#define ID_PIP   42
#define ID_VEX   666
#define ID_GRUMB 729

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

static void form(uint16_t c, int which, char *out, int cap) { talk_db_form(c, which, out, cap); }

static void conv(TalkConv *c, int a, int b, int d)
{
    int ids[3], n = 0;
    ids[n++] = a;
    if (b >= 0) ids[n++] = b;
    if (d >= 0) ids[n++] = d;
    talk_gen_begin(c, DOOMO_PLAYER_ID, ids, n, 12345u);
    c->plain = 1;                                  /* the core part alone: exact text */
}

static const char *line(TalkConv *c, int who, int to, int act, uint16_t topic)
{
    static TalkLine l;
    talk_gen_line(c, who, to, act, topic, &l, NULL);
    return l.text;
}

/* ---- the modifiers and fields ------------------------------------------- */
TEST(talk_gen_modifiers)
{
    TalkConv c;
    char want[128], a[24], b[24], d[24], e[24];
    uint16_t pie, pancake;
    uint32_t h0 = talk_safe_lint_hits();
    premade_town();
    CHECK(open_gentest());
    pie = concept("pie");
    pancake = concept("pancake");
    CHECK(pie && pancake);
    conv(&c, ID_NANA, ID_RIVET, ID_VEX);
    form(pie, TF_DEFAULT, a, 24); form(pie, TF_PLURAL, b, 24); form(pie, TF_INDEF, d, 24); form(pie, TF_DEF, e, 24);
    snprintf(want, sizeof want, "Forms: %s, %s, %s, %s.", a, b, d, e);
    CHECK_STR(line(&c, 0, -1, TA_ASK_ABOUT, pie), want);
    CHECK_STR(line(&c, 0, -1, TA_ASK_OPINION, pie), "More: pie, Pie, PIE, pie's.");
    CHECK_STR(line(&c, 0, -1, TA_TELL, pie), "Words: it is, has, does, was.");
    {
        TalkConcept pc;
        CHECK(talk_db_concept(pancake, &pc));
        if (pc.flags & TCF_PLURAL) CHECK_STR(line(&c, 0, -1, TA_TELL, pancake), "Words: they are, have, do, were.");
    }
    /* attributes: one said of it, a good one with its article, an association */
    {
        const char *t = line(&c, 0, -1, TA_ASK_FACT, pie);
        CHECK_MSG(strncmp(t, "Attr: ", 6) == 0 && strstr(t, ", near "), "%s", t);
        CHECK_MSG(strstr(t, ", a ") || strstr(t, ", an "), "%s", t);
    }
    /* no topic, or one that may not be said: the line needs {x}, so nothing
     * fits and a safe line plays (never an empty slot) */
    CHECK_STR(line(&c, 0, -1, TA_ASK_ABOUT, TC_NONE), "Anyway.");
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

TEST(talk_gen_names_and_likes)
{
    TalkConv c;
    char want[128], f[24], h[24], m[24], fe[24], ha[24];
    uint32_t h0 = talk_safe_lint_hits();
    premade_town();
    CHECK(open_gentest());
    conv(&c, ID_NANA, ID_RIVET, ID_VEX);
    snprintf(want, sizeof want, "Hi %s, I am %s.", town_res(DOOMO_PLAYER_ID)->name, town_res(ID_NANA)->name);
    if (town_res(DOOMO_PLAYER_ID)->name[0]) CHECK_STR(line(&c, 0, -1, TA_GREET, TC_NONE), want);
    /* {them} the addressed resident, {other} the third */
    snprintf(want, sizeof want, "%s and %s.", town_res(ID_RIVET)->name, town_res(ID_VEX)->name);
    CHECK_STR(line(&c, 0, 1, TA_ASK_WHO, TC_NONE), want);
    form(talk_gen_like(ID_NANA, 0), TF_DEFAULT, f, 24);
    form(talk_gen_like(ID_NANA, 1), TF_DEFAULT, h, 24);
    form(talk_gen_like(ID_NANA, 2), TF_DEFAULT, m, 24);
    snprintf(want, sizeof want, "Likes: %s, %s, %s.", f, h, m);
    CHECK_STR(line(&c, 0, -1, TA_JOKE, TC_NONE), want);
    form(talk_gen_like(ID_NANA, 3), TF_DEFAULT, fe, 24);
    form(talk_gen_like(ID_NANA, 4), TF_DEFAULT, ha, 24);
    snprintf(want, sizeof want, "Fears: %s, hates %s.", fe, ha);
    CHECK_STR(line(&c, 0, -1, TA_LAUGH, TC_NONE), want);
    /* the job callback: an adult premade has one, a child never */
    {
        uint16_t job = 0;
        char j[40];
        CHECK(talk_gen_fact(ID_NANA, TCB_JOB, &job));
        form(job, TF_INDEF, j, 40);
        snprintf(want, sizeof want, "Job: %s.", j);
        CHECK_STR(line(&c, 0, -1, TA_ASK_FEELING, TC_NONE), want);
        CHECK(!talk_gen_fact(ID_PIP, TCB_JOB, &job));
        conv(&c, ID_PIP, -1, -1);
        CHECK_STR(line(&c, 0, -1, TA_ASK_FEELING, TC_NONE), "No job.");
    }
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- selection ----------------------------------------------------------- */
TEST(talk_gen_weights_novelty_subrules)
{
    TalkConv c;
    int i, w9 = 0, alpha = 0, beta = 0;
    char seen[8][4];
    premade_town();
    CHECK(open_gentest());
    conv(&c, ID_NANA, ID_RIVET, -1);
    for (i = 0; i < 2000; i++) {
        memset(c.recent, 0, sizeof c.recent);                 /* no novelty: the weights alone */
        if (strcmp(line(&c, 0, -1, TA_THANK, TC_NONE), "W9.") == 0) w9++;
    }
    printf("      W9 picked %d of 2000 (weight 9 of 10)\n", w9);
    CHECK(w9 > 1700 && w9 < 1900);
    /* Q5: the same core template at most once per speaker in 6 lines */
    memset(seen, 0, sizeof seen);
    conv(&c, ID_NANA, ID_RIVET, -1);
    {
        char hist[64][4];
        for (i = 0; i < 64; i++) {
            int j;
            snprintf(hist[i], sizeof hist[i], "%.3s", line(&c, 0, -1, TA_ASK_REPEAT, TC_NONE));
            for (j = i - 1; j >= 0 && j >= i - 5; j--) CHECK_MSG(strcmp(hist[i], hist[j]) != 0, "%s twice in 6", hist[i]);
        }
    }
    for (i = 0; i < 200; i++) {
        const char *t = line(&c, 0, -1, TA_CONTINUE, TC_NONE);
        alpha += strcmp(t, "Sub: alpha.") == 0;
        beta += strcmp(t, "Sub: beta.") == 0;
    }
    CHECK(alpha > 40 && beta > 40 && alpha + beta == 200);
    talk_db_close();
}

TEST(talk_gen_fit_and_fallback)
{
    TalkConv c;
    TalkLine l;
    uint16_t pie, bagel;
    premade_town();
    CHECK(open_gentest());
    pie = concept("pie");
    bagel = concept("brimstone bagel");
    CHECK(pie && bagel);
    conv(&c, ID_NANA, -1, -1);
    talk_gen_line(&c, 0, -1, TA_BACKCHANNEL, pie, &l, NULL);
    CHECK_MSG(!(l.flags & TLF_FALLBACK) && strstr(l.text, "pie pie"), "%s", l.text);
    CHECK(talk_safe_bubble_lines(l.text, TALK_BUBBLE_COLS) <= TALK_BUBBLE_LINES);
    talk_gen_line(&c, 0, -1, TA_BACKCHANNEL, bagel, &l, NULL);          /* ten long words cannot fit */
    CHECK(l.flags & TLF_FALLBACK);
    CHECK_STR(l.text, "Anyway.");
    talk_db_close();
}

/* K-9: an everyday swear, from a blunt adult, once a conversation; never with a kid */
TEST(talk_gen_swear_cap)
{
    TalkConv c;
    int id, blunt = -1, i, n = 0;
    uint32_t h0 = talk_safe_lint_hits();
    premade_town();
    CHECK(open_gentest());
    for (id = 0; id < DOOMO_MAX_RESIDENTS && blunt < 0; id++)
        if (!talk_safe_minor(id) && RES_PERSONA(town_res(id), PERS_SPEECH) >= 10) blunt = id;
    CHECK(blunt >= 0);
    conv(&c, blunt, ID_RIVET, -1);
    for (i = 0; i < 20; i++) n += strcmp(line(&c, 0, -1, TA_INSULT, TC_NONE), "Damn.") == 0;
    CHECK_EQ(n, 1);
    CHECK_EQ(c.swears, 1);
    conv(&c, blunt, ID_PIP, -1);
    for (i = 0, n = 0; i < 20; i++) n += strcmp(line(&c, 0, -1, TA_INSULT, TC_NONE), "Damn.") == 0;
    CHECK_EQ(n, 0);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* K-6 / P5: a Mature variant needs the option and premade adults only */
TEST(talk_gen_mature_variant)
{
    TalkConv c;
    int i, n = 0;
    premade_town();
    CHECK(open_gentest());
    opt_set(OPT_MATURE, MATURE_OFF);
    conv(&c, ID_NANA, ID_RIVET, -1);
    for (i = 0; i < 50; i++) n += strcmp(line(&c, 0, -1, TA_MOCK, TC_NONE), "Mature variant.") == 0;
    CHECK_EQ(n, 0);
    opt_set(OPT_MATURE, MATURE_ON);
    /* the hero is no premade adult (mature_adult_ok refuses it): never */
    conv(&c, ID_NANA, ID_RIVET, -1);
    for (i = 0, n = 0; i < 50; i++) n += strcmp(line(&c, 0, -1, TA_MOCK, TC_NONE), "Mature variant.") == 0;
    CHECK_EQ(n, 0);
    /* premade adults only, playing as one of them: the variant can play */
    if (mature_adult_ok(ID_NANA) && mature_adult_ok(ID_RIVET) && mature_adult_ok(ID_VEX)) {
        int ids[2] = { ID_NANA, ID_RIVET };
        talk_gen_begin(&c, ID_VEX, ids, 2, 5u);
        c.plain = 1;
        for (i = 0, n = 0; i < 50; i++) {
            memset(c.recent, 0, sizeof c.recent);
            n += strcmp(line(&c, 0, -1, TA_MOCK, TC_NONE), "Mature variant.") == 0;
        }
        printf("      Mature on, premade adults only: the variant %d of 50\n", n);
        CHECK(n > 40);
    }
    conv(&c, ID_NANA, ID_PIP, -1);
    for (i = 0, n = 0; i < 50; i++) n += strcmp(line(&c, 0, -1, TA_MOCK, TC_NONE), "Mature variant.") == 0;
    CHECK_EQ(n, 0);
    opt_set(OPT_MATURE, MATURE_OFF);
    talk_db_close();
}

TEST(talk_gen_cast_and_faces)
{
    TalkConv c;
    TalkLine l;
    int i, nana = 0, rivet = 0;
    premade_town();
    CHECK(open_gentest());
    conv(&c, ID_NANA, ID_RIVET, -1);
    for (i = 0; i < 200; i++) {
        memset(c.recent, 0, sizeof c.recent);
        nana += strcmp(line(&c, 0, -1, TA_NOD, TC_NONE), "Nana nod.") == 0;
        rivet += strcmp(line(&c, 1, -1, TA_NOD, TC_NONE), "Nana nod.") == 0;
    }
    printf("      Nana's own line %d of 200 (weight x8); Rivet %d\n", nana, rivet);
    CHECK(nana > 150);
    CHECK_EQ(rivet, 0);
    talk_gen_line(&c, 0, -1, TA_WAVE, TC_NONE, &l, NULL);
    CHECK_STR(l.text, "Wave.");
    CHECK_EQ(l.face, EXPR_HAPPY + 1);
    CHECK_EQ(l.act, TA_WAVE);
    talk_db_close();
}

/* the optional parts, and every callback true when said (Q4) */
TEST(talk_gen_parts_and_callbacks)
{
    TalkConv c;
    TalkLine l;
    TalkTrace tr;
    int i, opener = 0, cb = 0, tag = 0, hand = 0, catchp = 0;
    uint32_t h0 = talk_safe_lint_hits();
    premade_town();
    CHECK(open_gentest());
    for (i = 0; i < 600; i++) {
        uint16_t v = 0;
        if (i % 40 == 0) { conv(&c, i % 80 ? ID_NANA : ID_RIVET, ID_VEX, ID_GRUMB); c.plain = 0; }
        doomo_town.minute = (uint16_t)((i * 37) % 1440);
        talk_gen_line(&c, 0, -1, TA_COMPLIMENT, TC_NONE, &l, &tr);
        CHECK(tr.parts & TPART_CORE);
        CHECK_MSG(strstr(l.text, "Thanks.") != NULL, "%s", l.text);
        if (tr.parts & TPART_OPEN) { opener++; CHECK(strncmp(l.text, "Oh- ", 4) == 0); }
        if (tr.parts & TPART_CB) {
            cb++;
            CHECK(tr.cb == TCB_JOB || tr.cb == TCB_NIGHT);
            CHECK_MSG(talk_gen_fact(c.id[0], tr.cb, &v) && v == tr.cb_val, "callback %d not true", tr.cb);
            if (tr.cb == TCB_NIGHT) CHECK(town_is_night());
        }
        if (tr.parts & TPART_TAG) tag++;
        if (tr.parts & TPART_HANDOFF) { hand++; CHECK(l.flags & TLF_HANDOFF); }
        catchp += strstr(l.text, doomo_catchphrases[RES_CATCHPHRASE(town_res(c.id[0])) % CATCHPHRASE_COUNT]) != NULL;
        CHECK(talk_safe_bubble_lines(l.text, TALK_BUBBLE_COLS) <= TALK_BUBBLE_LINES);
    }
    printf("      600 lines: opener %d, callback %d, tag %d, hand-off %d, catchphrase %d\n", opener, cb, tag, hand,
           catchp);
    CHECK(opener > 60 && cb > 60 && tag > 60 && hand > 30);
    /* a catchphrase once a conversation at most */
    CHECK(catchp <= 15);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- the exchange -------------------------------------------------------- */
static void utter(TalkUtter *u, int act, uint16_t topic, int target, uint8_t safety)
{
    memset(u, 0, sizeof *u);
    u->act[0] = (uint8_t)act;
    u->topic[0] = topic;
    u->target = (int8_t)target;
    u->safety = safety;
    strcpy(u->shown, "zorpy blorp");
}

TEST(talk_gen_exchange_gate)
{
    TalkConv c;
    TalkUtter u;
    TalkExchange x;
    int i;
    uint32_t h0 = talk_safe_lint_hits();
    premade_town();
    CHECK(open_gentest());
    /* K-7: sexual words with a child in the room end it, adults step in, no echo */
    conv(&c, ID_NANA, ID_PIP, -1);
    utter(&u, TA_ASK_ABOUT, TC_NONE, 0, TS_SEXUAL);
    CHECK(talk_gen_exchange(&c, &u, &x, NULL) >= 1);
    CHECK_EQ(x.gate, TG_END_K7);
    CHECK(c.ended);
    CHECK_STR(x.line[0].text, "We are done here.");
    CHECK(x.line[0].flags & TLF_END);
    CHECK_EQ(x.line[0].who, 0);                                 /* Nana, the adult */
    for (i = 0; i < x.n; i++) CHECK(!strstr(x.line[i].text, "zorp"));
    CHECK_EQ(talk_gen_exchange(&c, &u, &x, NULL), 0);          /* it is over */
    /* refusals: one sincere line, no reactions */
    conv(&c, ID_NANA, ID_RIVET, ID_VEX);
    utter(&u, TA_REFUSE_SEXUAL, TC_NONE, 0, TS_SEXUAL);
    CHECK_EQ(talk_gen_exchange(&c, &u, &x, NULL), 1);
    CHECK_EQ(x.gate, TG_REFUSE);
    CHECK_STR(x.line[0].text, "Not that.");
    utter(&u, TA_TELL, TC_NONE, -1, TS_HATE);
    CHECK_EQ(talk_gen_exchange(&c, &u, &x, NULL), 1);
    CHECK_STR(x.line[0].text, "Not okay.");
    utter(&u, TA_TELL, TC_NONE, -1, TS_DISTRESS);
    CHECK_EQ(talk_gen_exchange(&c, &u, &x, NULL), 1);
    CHECK_EQ(x.gate, TG_DISTRESS);
    CHECK_STR(x.line[0].text, "Please talk to someone you trust.");
    /* romance at a child: REFUSE_MINOR, both ways */
    conv(&c, ID_PIP, ID_NANA, -1);
    utter(&u, TA_FLIRT, TC_NONE, 0, 0);
    CHECK_EQ(talk_gen_exchange(&c, &u, &x, NULL), 1);
    CHECK_EQ(x.gate, TG_REFUSE_MINOR);
    CHECK_STR(x.line[0].text, "No. Never that.");
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

TEST(talk_gen_exchange_romance_consent)
{
    TalkConv c;
    TalkUtter u;
    TalkExchange x;
    int ids[1], avatar = roster_find("Mayor Giggs"), steamy = roster_find("Steamy");
    premade_town();
    CHECK(open_gentest());
    CHECK(avatar >= 0 && steamy >= 0);
    ids[0] = steamy;
    talk_gen_begin(&c, avatar, ids, 1, 99u);
    c.plain = 1;
    c.consent = 1;
    utter(&u, TA_FLIRT, TC_NONE, 0, 0);
    CHECK_EQ(talk_gen_exchange(&c, &u, &x, NULL), 1);
    CHECK_EQ(x.gate, TG_PASS);
    CHECK_STR(x.line[0].text, "Oh! Hello.");
    talk_gen_begin(&c, avatar, ids, 1, 99u);
    c.plain = 1;
    c.consent = 0;
    utter(&u, TA_FLIRT, TC_NONE, 0, 0);
    talk_gen_exchange(&c, &u, &x, NULL);
    CHECK_STR(x.line[0].text, "No thanks.");
    CHECK(c.refused & 1);                                        /* K-5: refused for the day */
    utter(&u, TA_FLIRT, TC_NONE, 0, 0);
    talk_gen_exchange(&c, &u, &x, NULL);
    CHECK_EQ(x.gate, TG_DEFLECT);
    CHECK_STR(x.line[0].text, "Not today.");
    talk_db_close();
}

/* Q2: a reaction answers the line before it (the adjacency table) */
TEST(talk_gen_exchange_reactions)
{
    TalkConv c;
    TalkUtter u;
    TalkExchange x;
    int i, k, reactions = 0;
    uint16_t pie;
    premade_town();
    CHECK(open_gentest());
    pie = concept("pie");
    CHECK(pie);
    conv(&c, ID_NANA, ID_VEX, ID_GRUMB);
    for (i = 0; i < 200; i++) {
        utter(&u, i & 1 ? TA_THANK : TA_ASK_ABOUT, pie, -1, 0);
        talk_gen_exchange(&c, &u, &x, NULL);
        CHECK(x.n >= 1 && x.n <= TALK_EXCHANGE_MAX);
        CHECK_MSG(talk_gen_adjacent(u.act[0], x.line[0].act), "act %d to %d: %s (flags %x)", x.line[0].act, u.act[0],
                  x.line[0].text, x.line[0].flags);
        for (k = 1; k < x.n; k++) {
            CHECK(x.line[k].flags & TLF_REACTION);
            CHECK_MSG(talk_gen_adjacent(x.line[k - 1].act, x.line[k].act), "act %d after %d", x.line[k].act,
                      x.line[k - 1].act);
            CHECK(x.line[k].who != x.line[k - 1].who);
            reactions++;
        }
    }
    printf("      200 exchanges, %d reactions\n", reactions);
    CHECK(reactions > 20);
    talk_db_close();
}

/* a name the player typed that may not be said in this room is never said */
TEST(talk_gen_custom_name)
{
    TalkConv c;
    int i;
    DoomoResident *r;
    uint32_t h0 = talk_safe_lint_hits();
    premade_town();
    CHECK(open_gentest());
    r = town_res(ID_RIVET);
    strcpy(r->name, "Damn");
    r->flags |= RF_CUSTOM;
    conv(&c, ID_NANA, ID_RIVET, ID_PIP);                         /* a child here: no register word */
    for (i = 0; i < 20; i++) CHECK(!strstr(line(&c, 0, 1, TA_ASK_WHO, TC_NONE), "Damn"));
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

TEST(talk_gen_deterministic)
{
    TalkConv a, b;
    int i;
    uint16_t pie;
    char ta[64][TALK_LINE_MAX];
    premade_town();
    CHECK(open_gentest());
    pie = concept("pie");
    conv(&a, ID_NANA, ID_VEX, ID_GRUMB);
    for (i = 0; i < 64; i++) strcpy(ta[i], line(&a, i % 3, -1, TA_COMPLIMENT, pie));
    conv(&b, ID_NANA, ID_VEX, ID_GRUMB);
    for (i = 0; i < 64; i++) CHECK_STR(line(&b, i % 3, -1, TA_COMPLIMENT, pie), ta[i]);
    talk_db_close();
}

/* ---- K-10: grief is not a punchline (SCRIPT_STYLE R6; design 3.6, 7.1) ---------
 * A grief or loss topic takes the sincere `grief` bank whatever the act (the
 * core alone, an act that may answer), a reaction is a `comfort` line or none,
 * the pet stays quiet, and a household member's fresh loss is a fact. */
TEST(talk_gen_grief)
{
    static const uint8_t acts[3] = { TA_ASK_REPEAT, TA_JOKE, TA_ENCOURAGE };
    static const char *const sincere[3] = { "Sincere answer.", "Not about this.", "Thank you, truly." };
    TalkConv c;
    TalkLine l;
    TalkTrace tr[TALK_EXCHANGE_MAX];
    TalkUtter u;
    TalkExchange x;
    TalkConcept tc;
    uint16_t funeral, grief, pie, v = 0;
    int i, k, nmiss = 0, reactions = 0, firsts = 0;
    char miss[48];
    uint32_t h0 = talk_safe_lint_hits(), day0;
    premade_town();
    CHECK(open_gentest());
    funeral = concept("funeral");
    grief = concept("grief");
    pie = concept("pie");
    CHECK(funeral && grief && pie);
    CHECK(talk_db_concept(funeral, &tc) && (tc.flags & TCF_GRIEF));
    CHECK(talk_db_concept(grief, &tc) && (tc.flags & TCF_GRIEF));
    CHECK(talk_db_concept(pie, &tc) && !(tc.flags & TCF_GRIEF));
    /* whatever the act: the sincere line, the core part alone, an act that may answer */
    conv(&c, ID_NANA, ID_VEX, ID_GRUMB);
    c.plain = 0;                                             /* openers, tags, hand-offs would be allowed */
    for (i = 0; i < 90; i++) {
        talk_gen_line(&c, i % 3, -1, acts[i % 3], i & 1 ? funeral : grief, &l, &tr[0]);
        CHECK_STR(l.text, sincere[i % 3]);
        CHECK(l.flags & TLF_SINCERE);
        CHECK(!(l.flags & (TLF_FALLBACK | TLF_HANDOFF)));
        CHECK_EQ(tr[0].parts, TPART_CORE);
        CHECK(talk_gen_adjacent(acts[i % 3], l.act));
    }
    /* an act no grief line here may answer: a quiet safe line, never the joke bank */
    talk_gen_line(&c, 0, -1, TA_GREET, funeral, &l, &tr[0]);
    CHECK_STR(l.text, "Anyway.");
    CHECK((l.flags & TLF_SINCERE) && (l.flags & TLF_FALLBACK));
    /* the safety acts keep their own (sincere) banks; another topic keeps its jokes */
    CHECK_STR(line(&c, 0, -1, TA_DISTRESS, funeral), "Please talk to someone you trust.");
    talk_gen_line(&c, 0, -1, TA_JOKE, pie, &l, NULL);
    CHECK_MSG(strstr(l.text, "Likes: ") && !(l.flags & TLF_SINCERE), "%s", l.text);
    /* a remembered grief topic is no callback, and the memory survives */
    c.mem_topic[0] = funeral;
    talk_gen_line(&c, 0, -1, TA_JOKE, pie, &l, &tr[0]);
    for (k = 0; k < tr[0].nsaid; k++) CHECK(tr[0].said[k] != funeral);
    /* the loss fact: a household member who lost someone lately (not a move away) */
    CHECK(!talk_gen_fact(ID_NANA, TCB_LOSS, &v));
    CHECK(family_add(ID_NANA, FAMROLE_HOUSEMATE, 0) >= 0);
    CHECK(family_add(ID_RIVET, FAMROLE_HOUSEMATE, 0) >= 0);
    CHECK(!talk_gen_fact(ID_NANA, TCB_LOSS, &v));
    CHECK(family_lose(ID_RIVET, FAMLOSS_ILLNESS));
    CHECK(talk_gen_fact(ID_NANA, TCB_LOSS, &v));
    CHECK_EQ(v, ID_RIVET);
    CHECK(!talk_gen_fact(ID_VEX, TCB_LOSS, &v));              /* not of the household */
    CHECK(family_add(5, FAMROLE_HOUSEMATE, 0) >= 0);           /* Bonejangles moves in, then away */
    CHECK(family_lose(5, FAMLOSS_GONE));
    CHECK(talk_gen_fact(ID_NANA, TCB_LOSS, &v));
    CHECK_EQ(v, ID_RIVET);                                    /* a move away is no loss to mourn */
    snprintf(miss, sizeof miss, "I miss %s.", town_res(ID_RIVET)->name);
    conv(&c, ID_NANA, ID_VEX, -1);
    for (i = 0; i < 20; i++) {
        const char *t = line(&c, 0, -1, TA_ASK_REPEAT, funeral);
        CHECK_MSG(strcmp(t, miss) == 0 || strcmp(t, "Sincere answer.") == 0, "%s", t);
        nmiss += strcmp(t, miss) == 0;
        CHECK_STR(line(&c, 1, -1, TA_ASK_REPEAT, funeral), "Sincere answer.");   /* Vex lost no one */
    }
    CHECK(nmiss >= 5);
    day0 = doomo_town.day;
    doomo_town.day += TALK_GRIEF_DAYS + 1;                    /* a month on: no longer fresh */
    CHECK(!talk_gen_fact(ID_NANA, TCB_LOSS, &v));
    for (i = 0; i < 10; i++) CHECK_STR(line(&c, 0, -1, TA_ASK_REPEAT, funeral), "Sincere answer.");
    doomo_town.day = day0;
    /* the exchange: one comfort line at most after it, and the pet stays quiet */
    {
        int ids[3] = { ID_NANA, ID_VEX, TALK_PET_ID };
        talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 3, 4242u);
    }
    for (i = 0; i < 200; i++) {
        utter(&u, TA_ASK_REPEAT, i & 1 ? funeral : grief, i % 3 == 2 ? -1 : i % 2, 0);
        talk_gen_exchange(&c, &u, &x, tr);
        CHECK(x.n >= 1 && x.n <= 2);
        CHECK(x.line[0].flags & TLF_SINCERE);
        if (c.id[x.line[0].who] != TALK_PET_ID) firsts++;
        for (k = 1; k < x.n; k++) {
            CHECK_STR(x.line[k].text, "We are here.");
            CHECK(x.line[k].flags & TLF_REACTION);
            CHECK(x.line[k].flags & TLF_SINCERE);
            CHECK(c.id[x.line[k].who] != TALK_PET_ID);
            CHECK(talk_gen_adjacent(x.line[k - 1].act, x.line[k].act));
            CHECK_EQ(tr[k].parts, TPART_CORE);
            reactions++;
        }
    }
    printf("      200 grief exchanges: %d sincere first lines, %d comfort reactions\n", firsts, reactions);
    CHECK(firsts > 100 && reactions > 10);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}
