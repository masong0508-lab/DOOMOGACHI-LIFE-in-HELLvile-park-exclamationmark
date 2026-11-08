/* test_talk_safety.c - T4: the Talk safety gate (core/src/talk_safe.c) over
 * the real TALK_en.BIN (make -C core talk, which `unit` depends on) and a
 * premade town. Owner: TK. docs/TALK.md "Safety"; design section 7.
 *
 *   7.3  one source of "minor": talk_safe_minor() agrees with LS0 / E1 for
 *        all 1,000 premade ids, the kid cast file, every pool stage; unknown
 *        ids are minors (fail safe)
 *   7.6  the labelled safety corpus (100% on every must-refuse class, 97%
 *        or better on the near misses), the hand-written adversarial corpus,
 *        machine mutation of every must-refuse line (99% or better; 100% on
 *        every child-related line), every must-refuse line again after 41
 *        junk tokens (the token cap: 100%, in its class, nothing shown past
 *        the cut), and a child named in the line but not in the room
 *   7.5  property tests at the gate: P1 (kid rooms), P2 (both directions),
 *        P3 (K-7: the conversation ends, the input is never shown), P4
 *        (pet), P5 (Mature), P6 (the parser only ever hands on concepts that
 *        may be generated), P7 (the gate changes no romance state), P8 (the
 *        swear cap), plus the S3 output lint over the whole vocabulary. The
 *        generator-level halves of P1-P8 go live with T5a / T6 (no line is
 *        generated before then).
 * DOOMO_TALK_MISS=1 lists the misses. DOOMO_HATE_PLAIN=file (the reviewer's
 * local plaintext hate list, never committed) checks every word on it.
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

#define ID_NANA  1
#define ID_RIVET 13
#define ID_PIP   42           /* kid cast */
#define ID_TEEN  11           /* ROSTER_T_TEEN */
#define ID_VEX   666

static int open_db(void) { return talk_db_open(lang_find("en")) == TALK_DB_OK; }

static void premade_town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
}

static const char *core_path(const char *rel, char *buf, int cap)
{
    snprintf(buf, (size_t)cap, "%s/../../%s", test_golden_dir, rel);
    return buf;
}

/* ---- 7.3: one source ------------------------------------------------------ */
TEST(talk_safe_minor_single_source)
{
    int id, minors = 0, kids_in_file = 0;
    char path[512], line[200];
    FILE *f;
    premade_town();
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        int want = LIFE_IS_MINOR(sim_life_stage(id)) || (roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN)) ||
                   ending_kid_cast(id);
        CHECK_MSG(talk_safe_minor(id) == want, "id %d: talk_safe_minor %d, LS0/E1 say %d", id, talk_safe_minor(id), want);
        minors += want;
        /* never an adult-only ok for a minor */
        if (want) CHECK(!mature_adult_ok(id));
    }
    printf("      %d of %d premade ids are minors\n", minors, DOOMO_MAX_RESIDENTS);
    CHECK(minors > 0);
    /* the kid cast file, read here directly: every id on it is a minor */
    f = fopen(core_path("data/story/kid_cast.txt", path, sizeof path), "r");
    CHECK(f != NULL);
    if (f) {
        while (fgets(line, sizeof line, f)) {
            if (!isdigit((unsigned char)line[0])) continue;
            id = atoi(line);
            CHECK_MSG(talk_safe_minor(id), "kid cast %d is not a minor to Talk", id);
            kids_in_file++;
        }
        fclose(f);
    }
    CHECK(kids_in_file >= 2);
    CHECK(talk_safe_minor(ID_PIP));
    CHECK(talk_safe_minor(ID_TEEN));
    CHECK(!talk_safe_minor(ID_NANA));
    /* a canon kid given a makeover is still a kid (sim_life_stage keeps the roster's stage) */
    {
        DoomoResident *r = town_res(ID_PIP);
        CHECK(r != NULL);
        if (r) resident_set_stage(r, LIFE_ADULT);
        CHECK(talk_safe_minor(ID_PIP));
    }
    /* the hero follows the player's stage */
    CHECK_EQ(talk_safe_minor(DOOMO_PLAYER_ID), LIFE_IS_MINOR(sim_life_stage(DOOMO_PLAYER_ID)));
    /* unknown ids: minors (fail safe); the pet: not a minor, but the pet */
    CHECK(talk_safe_minor(-1));
    CHECK(talk_safe_minor(5000));
    CHECK(talk_safe_minor(DOOMO_PLAYER_ID + 19));      /* an empty pool slot */
    CHECK(!talk_safe_minor(TALK_PET_ID));
    CHECK(talk_safe_is_pet(TALK_PET_ID));
    CHECK(!talk_safe_is_pet(ID_NANA));
}

/* pool members carry their own stage; every stage maps through LIFE_IS_MINOR */
TEST(talk_safe_minor_pool_stages)
{
    int st;
    premade_town();
    for (st = 0; st < LIFE_COUNT; st++) {
        int id = family_pool_add(FAMROLE_CHILD, st, 1234u + (uint32_t)st, "Kiddo", PRONOUN_THEY);
        if (id < 0) id = family_pool_add(FAMROLE_SIBLING, st, 99u + (uint32_t)st, "Sib", PRONOUN_THEY);
        if (id < 0) continue;
        CHECK_MSG(talk_safe_minor(id) == LIFE_IS_MINOR(family_find(id)->stage), "pool %d stage %d", id, st);
    }
}

/* ---- the rooms of the corpora ---------------------------------------------- */
typedef struct Room { int avatar, ids[TALK_PARTS_MAX], n, ring; const char *names[TALK_PARTS_MAX]; } Room;

static Room room_of(const char *kind)
{
    Room r;
    memset(&r, 0, sizeof r);
    r.avatar = DOOMO_PLAYER_ID;
    r.n = 3;
    r.ids[0] = ID_NANA; r.ids[1] = ID_VEX; r.ids[2] = ID_RIVET;
    r.names[0] = "Nana Puddin"; r.names[1] = "Vex"; r.names[2] = "Rivet";
    if (strcmp(kind, "kid") == 0 || strcmp(kind, "kidtarget") == 0) {
        r.ids[1] = ID_PIP; r.names[1] = "Pip";
        r.ids[2] = ID_VEX; r.names[2] = "Vex";
        r.ring = strcmp(kind, "kidtarget") == 0 ? 1 : 0;
    } else if (strcmp(kind, "pet") == 0) {
        r.ids[2] = TALK_PET_ID; r.names[2] = "Boreagotchi";
    } else if (strcmp(kind, "teenavatar") == 0) {
        r.avatar = ID_TEEN;
    } else if (strcmp(kind, "kidavatar") == 0) {
        r.avatar = ID_PIP;
    } else if (strcmp(kind, "lovers") == 0) {
        /* Mayor Giggs talking to Steamy: two adults the romance gate allows
         * both ways (talk_safe_lovers_room checks it), so only the line
         * itself can stop a romance act */
        r.avatar = roster_find("Mayor Giggs");
        r.n = 1;
        r.ids[0] = roster_find("Steamy"); r.names[0] = "Steamy";
        r.ring = 0;
    }
    return r;
}

/* The game's person lookup (T5a passes the town's): a resident by full
 * name, else by first name; when several share a first name, a child if
 * one of them is (fail safe: "wisp" is the kid Wisp and Wisp Willow). */
static int town_person(const char *canon_name)
{
    static char names[ROSTER_COUNT][48];
    static int ready;
    int i, first = -1;
    if (!ready) {
        for (i = 0; i < ROSTER_COUNT; i++) talk_lex_canon(roster_name(i), names[i], (int)sizeof names[i]);
        ready = 1;
    }
    for (i = 0; i < ROSTER_COUNT; i++) if (strcmp(names[i], canon_name) == 0) return i;
    for (i = 0; i < ROSTER_COUNT; i++) {
        size_t l = strcspn(names[i], " ");
        if (strlen(canon_name) != l || strncmp(names[i], canon_name, l) != 0) continue;
        if (talk_safe_minor(i)) return i;
        if (first < 0) first = i;
    }
    return first;
}

/* parse + audience + gate, as Talk will run them */
static int gate_line(const char *text, const char *kind, TalkUtter *u, TalkParseInfo *info)
{
    Room r = room_of(kind);
    TalkParseCtx c;
    TalkAudience aud;
    int i, target;
    memset(&c, 0, sizeof c);
    c.nparts = (uint8_t)r.n;
    c.ring = (int8_t)r.ring;
    c.hops = 1;
    c.person = town_person;
    for (i = 0; i < r.n; i++) c.name[i] = r.names[i];
    talk_safe_ctx(r.avatar, r.ids, r.n, r.ring, &aud);
    c.kid_room = aud.any_minor;
    talk_parse(text, &c, u, info);
    talk_safe_ctx(r.avatar, r.ids, r.n, u->target, &aud);
    target = u->target >= 0 && u->target < r.n ? r.ids[u->target] : -1;
    return talk_safe_gate(u, &aud, r.avatar, target, 0);
}

static const char *const tg_names[] = { "PASS", "REFUSE", "REFUSE_MINOR", "DEFLECT", "HATE", "DISTRESS", "K7" };

/* does gate result g meet expectation e? */
static int meets(const char *e, int g, const TalkUtter *u)
{
    if (strcmp(e, "ok") == 0) return g == TG_PASS;
    if (strcmp(e, "K7") == 0) return g == TG_END_K7;
    if (strcmp(e, "REFUSE_SEXUAL") == 0) return g == TG_REFUSE;
    if (strcmp(e, "REFUSE_HATE") == 0) return g == TG_HATE;
    if (strcmp(e, "DISTRESS") == 0) return g == TG_DISTRESS;
    if (strcmp(e, "REFUSE_MINOR") == 0) return g == TG_REFUSE_MINOR;
    if (strcmp(e, "DEFLECT") == 0) return g == TG_DEFLECT;
    if (strcmp(e, "ROMANCE") == 0) return g == TG_DEFLECT || (g == TG_PASS && (TA_IS_ROMANCE(u->act[0]) || u->tone == TONE_FLIRTY));
    return 0;
}
static int must_refuse(const char *e)
{
    return strcmp(e, "K7") == 0 || strcmp(e, "REFUSE_SEXUAL") == 0 || strcmp(e, "REFUSE_HATE") == 0 ||
           strcmp(e, "DISTRESS") == 0 || strcmp(e, "REFUSE_MINOR") == 0;
}

typedef struct Line { char text[96], expect[16], room[16]; } Line;
#define LINES_MAX 600
static Line lines[LINES_MAX];

static int load_corpus(const char *rel)
{
    char path[512], buf[256];
    FILE *f = fopen(core_path(rel, path, sizeof path), "r");
    int n = 0;
    if (!f) return -1;
    while (fgets(buf, sizeof buf, f) && n < LINES_MAX) {
        char *a, *b;
        buf[strcspn(buf, "\r\n")] = 0;
        if (!buf[0] || buf[0] == '#') continue;
        a = strchr(buf, '\t');
        if (!a) continue;
        *a++ = 0;
        b = strchr(a, '\t');
        if (!b) continue;
        *b++ = 0;
        snprintf(lines[n].text, sizeof lines[n].text, "%.95s", buf);
        snprintf(lines[n].expect, sizeof lines[n].expect, "%.15s", a);
        snprintf(lines[n].room, sizeof lines[n].room, "%.15s", b);
        n++;
    }
    fclose(f);
    return n;
}

static void run_corpus(const char *rel, int *n_out, int *refuse_n, int *refuse_ok, int *near_n, int *near_ok,
                       int *exact_ok)
{
    int n = load_corpus(rel), i, miss = getenv("DOOMO_TALK_MISS") != 0;
    *n_out = n;
    *refuse_n = *refuse_ok = *near_n = *near_ok = *exact_ok = 0;
    for (i = 0; i < n; i++) {
        TalkUtter u;
        int g = gate_line(lines[i].text, lines[i].room, &u, NULL), ok = meets(lines[i].expect, g, &u);
        if (ok) (*exact_ok)++;
        if (must_refuse(lines[i].expect)) {
            (*refuse_n)++;
            if (g != TG_PASS) (*refuse_ok)++;
        } else {
            (*near_n)++;
            if (ok) (*near_ok)++;
        }
        if (!ok && miss)
            printf("      MISS %-44s [%s] want %s got %s (%s)\n", lines[i].text, lines[i].room, lines[i].expect,
                   tg_names[g], u.act[0] < TA_COUNT ? "" : "?");
        /* never an echo: a K-7, explicit or hate line leaves nothing to show */
        if (g == TG_END_K7 || g == TG_REFUSE || g == TG_HATE) CHECK_MSG(u.shown[0] == 0, "%.90s: shown", lines[i].text);
    }
}

TEST(talk_safe_corpus)
{
    int n, rn, rok, nn, nok, ex;
    premade_town();
    CHECK(open_db());
    run_corpus("data/talk/corpus/safety_en.tsv", &n, &rn, &rok, &nn, &nok, &ex);
    printf("      safety_en: %d lines; must-refuse refused %d/%d, exact %d/%d; near misses ok %d/%d\n", n, rok, rn, ex,
           n, nok, nn);
    CHECK(n >= 300);
    CHECK_MSG(rok == rn, "must-refuse recall %d/%d (needs 100%%)", rok, rn);
    CHECK_MSG(ex >= n - (nn - nok), "a must-refuse line got the wrong class (%d exact of %d)", ex, n);
    CHECK_MSG(nok * 100 >= nn * 97, "near-miss precision %d/%d under 97%%", nok, nn);
    run_corpus("data/talk/corpus/adversarial_en.tsv", &n, &rn, &rok, &nn, &nok, &ex);
    printf("      adversarial_en: %d lines; must-refuse refused %d/%d, exact %d/%d; near misses ok %d/%d\n", n, rok,
           rn, ex, n, nok, nn);
    CHECK(n >= 50);
    CHECK_MSG(rok == rn, "adversarial recall %d/%d", rok, rn);
    CHECK_MSG(nok * 100 >= nn * 97, "adversarial near-miss precision %d/%d", nok, nn);
    talk_db_close();
}

/* the "lovers" room means something: the gate would let romance through */
TEST(talk_safe_lovers_room)
{
    Room r;
    TalkAudience aud;
    premade_town();
    r = room_of("lovers");
    CHECK(r.avatar >= 0 && r.ids[0] >= 0);
    talk_safe_ctx(r.avatar, r.ids, r.n, 0, &aud);
    CHECK(!aud.any_minor && !aud.pet);
    CHECK(talk_safe_romance_ok(&aud, r.avatar, r.ids[0]));
}

/* A child named in the line, not in the room: TS_MINOR from the game's
 * lookup or the roster's cast (one source: talk_safe_minor), so sexual
 * words about them end the conversation (K-7) and romance about them is
 * REFUSE_MINOR; the same words about an adult are a flirt */
static int pedro_kid(const char *canon_name) { return strcmp(canon_name, "pedro") == 0 ? ID_PIP : -1; }
static int pedro_adult(const char *canon_name) { return strcmp(canon_name, "pedro") == 0 ? ID_NANA : -1; }

TEST(talk_safe_named_child)
{
    TalkUtter u;
    TalkParseInfo info;
    TalkParseCtx c;
    int i, pip = -1;
    premade_town();
    CHECK(open_db());
    CHECK_EQ(gate_line("pip is sexy", "lovers", &u, NULL), TG_END_K7);
    CHECK(u.shown[0] == 0);
    CHECK_EQ(gate_line("wisp is a hottie", "lovers", &u, NULL), TG_END_K7);   /* Wisp the kid, not Wisp Willow */
    CHECK_EQ(gate_line("i want to have sex with pip", "adults", &u, NULL), TG_END_K7);
    CHECK_EQ(gate_line("*kisses pip*", "lovers", &u, NULL), TG_REFUSE_MINOR);
    CHECK_EQ(gate_line("pip is cute", "lovers", &u, NULL), TG_PASS);
    CHECK_EQ(gate_line("nana is a hottie", "lovers", &u, NULL), TG_PASS);    /* an adult named: a flirt */
    /* the token carries the id: the roster's cast even without a lookup */
    memset(&c, 0, sizeof c);
    c.nparts = 1; c.name[0] = "Steamy"; c.hops = 1;
    talk_parse("pip is sexy", &c, &u, &info);
    for (i = 0; i < info.ntok; i++) if (strcmp(info.tok[i].text, "pip") == 0) pip = info.tok[i].person;
    CHECK_EQ(pip, ID_PIP);
    CHECK((u.safety & (TS_SEXUAL | TS_MINOR)) == (TS_SEXUAL | TS_MINOR));
    /* a first name of names_en.txt: a custom resident who has it */
    c.person = pedro_kid;
    talk_parse("pedro is sexy", &c, &u, &info);
    CHECK(u.safety & TS_MINOR);
    c.person = pedro_adult;
    talk_parse("pedro is sexy", &c, &u, &info);
    CHECK(!(u.safety & TS_MINOR));
    c.person = NULL;
    talk_parse("pedro is sexy", &c, &u, &info);
    CHECK(!(u.safety & TS_MINOR));
    talk_db_close();
}

/* The token cap (TALK_TOK_MAX): a line with more words than the tagger
 * holds is cut. The bubble never shows a byte past the cut, and S1 reads
 * every word: each must-refuse line of the corpus after 40-odd junk tokens
 * is still refused, in its class. Every pad is 2 bytes a token, so the
 * whole line stays inside the 2 x TALK_INPUT_MAX bytes the parser reads. */
TEST(talk_safe_long_lines)
{
    static const char *const pads[] = { "a?", "a ", "q-", "1 ", "u,", "?a" };
    int n, i, k, total = 0, refused = 0, exact = 0, miss = getenv("DOOMO_TALK_MISS") != 0;
    TalkUtter u;
    TalkParseInfo info;
    TalkParseCtx c;
    char text[256];
    premade_town();
    CHECK(open_db());
    n = load_corpus("data/talk/corpus/safety_en.tsv");
    CHECK(n > 0);
    for (i = 0; i < n; i++) {
        Line ln = lines[i];
        if (!(strcmp(ln.expect, "K7") == 0 || strcmp(ln.expect, "REFUSE_SEXUAL") == 0 ||
              strcmp(ln.expect, "REFUSE_HATE") == 0 || strcmp(ln.expect, "DISTRESS") == 0)) continue;
        for (k = 0; k < (int)(sizeof pads / sizeof pads[0]); k++) {
            int o = 0, r, g;
            for (r = 0; r < 41; r++) o += snprintf(text + o, sizeof text - (size_t)o, "%s", pads[k]);
            snprintf(text + o, sizeof text - (size_t)o, " %s", ln.text);
            if ((int)strlen(text) > 2 * TALK_INPUT_MAX) continue;
            g = gate_line(text, ln.room, &u, &info);
            total++;
            CHECK_MSG(info.cut > 0, "%s: not cut", text);
            CHECK_MSG((int)strlen(u.shown) <= info.cut, "%s: the bubble shows past the cut", text);
            if (g == TG_END_K7 || g == TG_REFUSE || g == TG_HATE) CHECK_MSG(u.shown[0] == 0, "%s: shown", text);
            if (g != TG_PASS) refused++;
            if (meets(ln.expect, g, &u)) exact++;
            else if (miss) printf("      LONG MISS [%s] want %s got %s: %s\n", ln.room, ln.expect, tg_names[g], text);
        }
    }
    printf("      long lines (a must-refuse line after 41 junk pads): %d, refused %d, exact class %d\n", total,
           refused, exact);
    CHECK(total >= 500);
    CHECK_EQ(refused, total);
    CHECK_EQ(exact, total);
    /* the reviewer's three: K-7, distress and a swear after 40 tokens */
    CHECK_EQ(gate_line("a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a? have sex with pip", "kid", &u, &info), TG_END_K7);
    CHECK_EQ(info.ntok, TALK_TOK_MAX);
    CHECK_EQ(gate_line("a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a? i want to die", "kid", &u, &info), TG_DISTRESS);
    CHECK(strstr(u.shown, "die") == NULL);
    memset(&c, 0, sizeof c);
    c.nparts = 1; c.name[0] = "Vex"; c.hops = 1; c.kid_room = 1;
    talk_parse("a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a? fuck you", &c, &u, &info);
    CHECK_EQ(info.cut, 41);
    CHECK_EQ((int)strlen(u.shown), 40);
    CHECK(strstr(u.shown, "fuck") == NULL);
    CHECK(u.safety & TS_STRONG);
    /* a line of exactly TALK_TOK_MAX tokens is read whole: no cut */
    talk_parse("a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?", &c, &u, &info);
    CHECK_EQ(info.ntok, TALK_TOK_MAX);
    CHECK_EQ(info.cut, 0);
    CHECK(strlen(u.shown) == 40);
    /* one run of 90 bytes ("q-q-...-sex"): raw words of 40 bytes at most, so the cut is
     * inside the line, never at its start */
    talk_parse("q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-q-sex", &c, &u,
               &info);
    CHECK(info.cut > 0);
    CHECK(u.safety & TS_SEXUAL);
    CHECK(strstr(u.shown, "sex") == NULL);
    talk_db_close();
}

/* obfuscation: one invisible format character inside a word is no gap
 * ("se<ZWSP>x"), and still a gap between two words ("sex<ZWSP>with");
 * a safety word split by a space into two non-words joins ("fu ck"), two
 * words never do ("pen is") */
TEST(talk_safe_split_words)
{
    TalkUtter u;
    TalkParseCtx c;
    premade_town();
    CHECK(open_db());
    memset(&c, 0, sizeof c);
    c.nparts = 1; c.name[0] = "Vex"; c.hops = 1;
    talk_parse("se\xe2\x80\x8bx", &c, &u, NULL);
    CHECK(u.safety & TS_SEXUAL);
    talk_parse("s\xc2\xad" "ex", &c, &u, NULL);
    CHECK(u.safety & TS_SEXUAL);
    talk_parse("na\xef\xbb\xbf" "ked", &c, &u, NULL);
    CHECK(u.safety & TS_SEXUAL);
    talk_parse("sex\xe2\x80\x8bwith pip", &c, &u, NULL);
    CHECK(u.safety & TS_SEXUAL);
    CHECK_EQ(gate_line("sex\xe2\x80\x8bwith pip", "kid", &u, NULL), TG_END_K7);
    talk_parse("se x", &c, &u, NULL);
    CHECK(u.safety & TS_SEXUAL);
    CHECK_EQ(gate_line("na ked", "kid", &u, NULL), TG_END_K7);
    talk_parse("fu ck you", &c, &u, NULL);
    CHECK(u.safety & TS_STRONG);
    CHECK(strstr(u.shown, "fu") == NULL && strstr(u.shown, "ck") == NULL);
    talk_parse("the pen is mightier than the sword", &c, &u, NULL);
    CHECK(!(u.safety & (TS_SEXUAL | TS_STRONG)));
    CHECK_EQ(gate_line("the pen is blue", "kid", &u, NULL), TG_PASS);
    talk_db_close();
}

/* ---- 7.6: machine mutation of every must-refuse line ------------------------- */
static uint32_t rng_s;
static uint32_t rnd(uint32_t n) { rng_s = rng_s * 1664525u + 1013904223u; return n ? (rng_s >> 8) % n : 0; }

/* mutate the word w (letters) into out; returns the mutation kind or -1 */
static int mutate_word(const char *w, int kind, char *out, int cap)
{
    int n = (int)strlen(w), i, o = 0, p;
    static const char leet_from[] = "aeiostbg", *const leet_to[] = { "4", "3", "1", "0", "5", "7", "8", "9" };
    static const struct { char c; const char *u; } look[] = {
        { 'a', "\xd0\xb0" }, { 'e', "\xd0\xb5" }, { 'o', "\xd0\xbe" }, { 'c', "\xd1\x81" }, { 'p', "\xd1\x80" },
        { 'x', "\xd1\x85" }, { 'y', "\xd1\x83" }, { 'i', "\xd1\x96" }, { 's', "\xd1\x95" } };
    if (n < 2 || cap < 4 * n + 8) return -1;
    out[0] = 0;
    switch (kind) {
    case 0: {                                                  /* leet: one or two letters */
        int done = 0, k;
        for (i = 0; i < n; i++) {
            const char *q = strchr(leet_from, w[i]);
            if (q && done < 2 && rnd(2) == 0) {
                k = (int)(q - leet_from);
                out[o++] = leet_to[k][0];
                done++;
            } else out[o++] = w[i];
        }
        out[o] = 0;
        for (i = 0; out[i]; i++) if (isalpha((unsigned char)out[i])) break;
        return done && out[i] ? 0 : -1;                         /* a leet word keeps a letter: "70" is a number */
    }
    case 1:                                                    /* s p a c e d */
        for (i = 0; i < n; i++) { if (i) out[o++] = ' '; out[o++] = w[i]; }
        out[o] = 0;
        return 1;
    case 2: {                                                  /* s.e.x / s-e-x / s*x inside */
        static const char marks[] = ".-_.";
        char m = marks[rnd(4)];
        int all = rnd(2);
        p = 1 + (int)rnd((uint32_t)(n - 1));
        for (i = 0; i < n; i++) {
            if (i && (all || i == p)) out[o++] = m;
            out[o++] = w[i];
        }
        out[o] = 0;
        return 2;
    }
    case 3:                                                    /* repeats */
        p = (int)rnd((uint32_t)n);
        for (i = 0; i < n; i++) {
            int r = i == p ? 2 + (int)rnd(4) : 1;
            while (r--) out[o++] = w[i];
        }
        out[o] = 0;
        return 3;
    case 4: {                                                  /* a look-alike letter */
        int k, done = 0;
        for (i = 0; i < n; i++) {
            if (!done) {
                for (k = 0; k < (int)(sizeof look / sizeof look[0]); k++)
                    if (look[k].c == w[i] && rnd(2) == 0) {
                        memcpy(out + o, look[k].u, 2); o += 2; done = 1; break;
                    }
                if (done) continue;
            }
            out[o++] = w[i];
        }
        out[o] = 0;
        return done ? 4 : -1;
    }
    case 5:                                                    /* CaSe */
        for (i = 0; i < n; i++) out[o++] = (char)(rnd(2) ? toupper((unsigned char)w[i]) : w[i]);
        out[o] = 0;
        return 5;
    default: {                                                 /* one typo after the first letter */
        int t = (int)rnd(4);
        if (n < 4) return -1;
        p = 1 + (int)rnd((uint32_t)(n - 1));
        for (i = 0; i < n; i++) {
            if (i == p) {
                if (t == 0) continue;                                          /* deletion */
                if (t == 1 && i + 1 < n) { out[o++] = w[i + 1]; out[o++] = w[i]; i++; continue; }   /* swap */
                if (t == 2) { out[o++] = (char)('a' + rnd(26)); continue; }    /* substitution */
                if (t == 3) out[o++] = (char)('a' + rnd(26));                  /* insertion */
            }
            out[o++] = w[i];
        }
        out[o] = 0;
        return strcmp(out, w) ? 6 : -1;
    }
    }
}

TEST(talk_safe_mutation_fuzz)
{
    int n, i, k, total = 0, refused = 0, exact = 0, skipped = 0, child_total = 0, child_refused = 0,
        miss = getenv("DOOMO_TALK_MISS") != 0, by_kind[8] = { 0 }, by_kind_ok[8] = { 0 };
    premade_town();
    CHECK(open_db());
    n = load_corpus("data/talk/corpus/safety_en.tsv");
    CHECK(n > 0);
    rng_s = 20261108u;
    for (i = 0; i < n; i++) {
        TalkUtter u;
        TalkParseInfo info;
        Line ln = lines[i];
        int child = strcmp(ln.expect, "K7") == 0;
        if (!(child || strcmp(ln.expect, "REFUSE_SEXUAL") == 0 || strcmp(ln.expect, "REFUSE_HATE") == 0 ||
              strcmp(ln.expect, "DISTRESS") == 0)) continue;
        gate_line(ln.text, ln.room, &u, &info);
        for (k = 0; k < 24; k++) {
            int t, a, b, j, kind, got;
            char word[48], mut[200], text[256], canon[64];
            uint16_t c, wf;
            /* a trigger token, and one word of letters inside its bytes */
            int trig[TALK_TOK_MAX], nt = 0;
            for (t = 0; t < info.ntok; t++)
                if (info.tok[t].wflags & (TW_SEXUAL | TW_SUGGEST | TW_HATE | TW_DISTRESS | TW_MINOR | TW_GROUP))
                    trig[nt++] = t;
            if (!nt) { skipped++; break; }
            t = trig[rnd((uint32_t)nt)];
            a = info.tok[t].src0; b = info.tok[t].src1;
            {
                int starts[16], ends_[16], nw = 0;
                for (j = a; j < b && nw < 16; ) {
                    while (j < b && !isalpha((unsigned char)ln.text[j])) j++;
                    if (j >= b) break;
                    starts[nw] = j;
                    while (j < b && isalpha((unsigned char)ln.text[j])) j++;
                    ends_[nw++] = j;
                }
                if (!nw) { skipped++; continue; }
                j = (int)rnd((uint32_t)nw);
                a = starts[j]; b = ends_[j];
            }
            if (b - a >= (int)sizeof word || b - a < 2) { skipped++; continue; }
            memcpy(word, ln.text + a, (size_t)(b - a));
            word[b - a] = 0;
            for (j = 0; word[j]; j++) word[j] = (char)tolower((unsigned char)word[j]);
            kind = mutate_word(word, (int)rnd(8), mut, (int)sizeof mut);
            if (kind < 0) { skipped++; continue; }
            /* a typo that lands on another real word is no longer the same word */
            if (kind == 6 || kind == 0) {
                talk_lex_canon(mut, canon, (int)sizeof canon);
                if (talk_lex_known(canon, &wf) &&
                    !(wf & (TW_SEXUAL | TW_SUGGEST | TW_HATE | TW_DISTRESS | TW_MINOR | TW_GROUP))) {
                    skipped++;
                    continue;
                }
                (void)c;
            }
            snprintf(text, sizeof text, "%.*s%s%s", a, ln.text, mut, ln.text + b);
            got = gate_line(text, ln.room, &u, NULL);
            total++;
            by_kind[kind]++;
            if (got != TG_PASS) { refused++; by_kind_ok[kind]++; }
            else if (miss) printf("      FUZZ MISS [%s] %s  <- %s\n", ln.room, text, ln.text);
            if (meets(ln.expect, got, &u)) exact++;
            if (child) { child_total++; if (got != TG_PASS) child_refused++; }
        }
    }
    printf("      mutated must-refuse lines: %d (skipped %d), refused %d (%d.%d%%), exact class %d;"
           " child-related refused %d/%d\n", total, skipped, refused, total ? refused * 100 / total : 0,
           total ? refused * 1000 / total % 10 : 0, exact, child_refused, child_total);
    printf("      by mutation: leet %d/%d spaced %d/%d marks %d/%d repeats %d/%d lookalike %d/%d case %d/%d typo %d/%d\n",
           by_kind_ok[0], by_kind[0], by_kind_ok[1], by_kind[1], by_kind_ok[2], by_kind[2], by_kind_ok[3], by_kind[3],
           by_kind_ok[4], by_kind[4], by_kind_ok[5], by_kind[5], by_kind_ok[6], by_kind[6]);
    CHECK(total >= 2000);
    CHECK_MSG(refused * 100 >= total * 99, "mutated recall %d/%d under 99%%", refused, total);
    CHECK_MSG(child_refused == child_total, "a mutated child-related line got through (%d/%d)", child_refused,
              child_total);
    talk_db_close();
}

/* ---- the hashed hate list ------------------------------------------------------ */
TEST(talk_safe_hate_hash)
{
    int i, n, hits = 0;
    char buf[96];
    CHECK(open_db());
    /* the C hash is talkc_safe.py's: salt 0x9E3779B97F4A7C15, squeezed "testword" */
    CHECK_EQ_U(talk_safe_hash("testword"), 0x04980A02F28746C0ull);
    CHECK_EQ_U(talk_safe_hash("T3ST-W00RD"), 0x04980A02F28746C0ull);     /* squeeze: leet, case, marks, repeats */
    /* no word of the lexicon is on the list (the list is slurs only) */
    n = talk_db_nconcepts();
    for (i = 1; i <= n; i++) {
        if (talk_db_form(i, TF_LEMMA, buf, sizeof buf) <= 0) continue;
        if (talk_safe_hate_word(buf, 0)) { hits++; printf("      lexicon word on the hate list: %s\n", buf); }
    }
    CHECK_EQ(hits, 0);
    /* the reviewer's local plaintext list, when given: every word, plural, leet and one typo */
    {
        const char *plain = getenv("DOOMO_HATE_PLAIN");
        FILE *f = plain ? fopen(plain, "r") : NULL;
        int words = 0, caught = 0;
        if (f) {
            char line[64];
            while (fgets(line, sizeof line, f)) {
                char w[80], v[80];
                size_t l;
                if (line[0] == '#' || line[0] == '\n') continue;
                line[strcspn(line, "\r\n ")] = 0;
                l = strlen(line);
                if (l < 3) continue;
                words++;
                snprintf(w, sizeof w, "%.60ss", line);
                snprintf(v, sizeof v, "%.60s", line);
                if (l >= 5) v[2] = v[2] == 'a' ? 'e' : 'a';         /* a typo after the first letter */
                caught += talk_safe_hate_word(line, 0) && talk_safe_hate_word(w, 0) && (l < 5 || talk_safe_hate_word(v, 1));
            }
            fclose(f);
            printf("      DOOMO_HATE_PLAIN: %d of %d words caught (with plural and a typo)\n", caught, words);
            CHECK_EQ(caught, words);
        }
    }
    talk_db_close();
}

/* ---- 7.5: property tests at the gate ------------------------------------------- */
static int adults[64], nadult, minors[64], nminor;

static void pick_people(void)
{
    int id;
    nadult = nminor = 0;
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        if (talk_safe_minor(id)) { if (nminor < 64 && id % 3 == 0) minors[nminor++] = id; }
        else if (nadult < 64 && id % 11 == 0) adults[nadult++] = id;
    }
}

TEST(talk_safe_property_gate)
{
    static const uint8_t romance_acts[] = { TA_FLIRT, TA_CONFESS, TA_ASK_OUT, TA_KISS };
    int it, p1 = 0, p2 = 0, p3 = 0, p4 = 0, passes_adult = 0;
    int edges_before, sweep;
    premade_town();
    pick_people();
    CHECK(nminor > 10 && nadult > 10);
    edges_before = doomo_town.edge_count;
    rng_s = 777u;
    for (it = 0; it < 200000; it++) {
        TalkUtter u;
        TalkAudience aud;
        int ids[TALK_PARTS_MAX], n = 1 + (int)rnd(3), i, avatar, tslot, target, g, any_minor;
        for (i = 0; i < n; i++) {
            int r = (int)rnd(10);
            ids[i] = r < 4 ? minors[rnd((uint32_t)nminor)] : r < 9 ? adults[rnd((uint32_t)nadult)] : TALK_PET_ID;
        }
        avatar = rnd(5) == 0 ? minors[rnd((uint32_t)nminor)] : rnd(4) == 0 ? adults[rnd((uint32_t)nadult)]
                                                                             : DOOMO_PLAYER_ID;
        tslot = rnd(6) == 0 ? -1 : (int)rnd((uint32_t)n);
        target = tslot >= 0 ? ids[tslot] : -1;
        talk_safe_ctx(avatar, ids, n, tslot, &aud);
        any_minor = talk_safe_minor(avatar);
        for (i = 0; i < n; i++) any_minor |= talk_safe_minor(ids[i]);
        CHECK(aud.any_minor == any_minor);
        memset(&u, 0, sizeof u);
        strcpy(u.shown, "what the player typed");
        u.act[0] = (uint8_t)(rnd(3) == 0 ? romance_acts[rnd(4)] : rnd(TA_FIRST_SAFETY));
        u.tone = (uint8_t)rnd(TONE_COUNT);
        u.safety = (uint8_t)(rnd(4) == 0 ? TS_SEXUAL : 0) | (uint8_t)(rnd(6) == 0 ? TS_MINOR : 0);
        if (rnd(10) == 0) u.act[0] = TA_REFUSE_SEXUAL;
        g = talk_safe_gate(&u, &aud, avatar, target, (int)rnd(8) == 0);
        /* P3: sexual + any child anywhere ends it, unseen */
        if ((u.safety & TS_SEXUAL) || g == TG_END_K7) {
            /* (the gate rewrote u; judge on what came in) */
        }
        if (g == TG_END_K7) { CHECK(u.shown[0] == 0); CHECK_EQ(u.act[0], TA_REFUSE_SEXUAL); p3++; }
        /* P1: in a room with a non-adult nothing romantic or sexual passes */
        if (any_minor) {
            CHECK_MSG(!(g == TG_PASS && (TA_IS_ROMANCE(u.act[0]) || TA_IS_ROMANCE(u.act[1]) || u.tone == TONE_FLIRTY ||
                                         (u.safety & TS_SEXUAL))), "a romance act passed in a kid room");
            p1++;
        }
        /* P4: the pet: never romance or sexual */
        if (aud.pet) CHECK(!(g == TG_PASS && (TA_IS_ROMANCE(u.act[0]) || (u.safety & TS_SEXUAL))));
        if (aud.pet) p4++;
        /* romance passes only between two adults the sim's gates allow, both ways */
        if (g == TG_PASS && TA_IS_ROMANCE(u.act[0])) {
            CHECK(target >= 0 && !talk_safe_minor(avatar) && !talk_safe_minor(target) && !aud.pet);
            CHECK(playas_romance_ok(avatar, target) && playas_romance_ok(target, avatar));
            passes_adult++;
        }
    }
    /* P2: every (adult, minor) pair, both directions */
    {
        int a, m;
        for (a = 0; a < nadult; a++)
            for (m = 0; m < nminor; m++) {
                TalkUtter u;
                TalkAudience aud;
                int ids[2];
                ids[0] = adults[a]; ids[1] = minors[m];
                memset(&u, 0, sizeof u);
                u.act[0] = TA_CONFESS;
                talk_safe_ctx(adults[a], ids + 1, 1, 0, &aud);           /* an adult avatar, the minor addressed */
                CHECK_EQ(talk_safe_gate(&u, &aud, adults[a], minors[m], 0), TG_REFUSE_MINOR);
                memset(&u, 0, sizeof u);
                u.act[0] = TA_FLIRT;
                talk_safe_ctx(minors[m], ids, 1, 0, &aud);               /* the minor as the avatar, at the adult */
                CHECK_EQ(talk_safe_gate(&u, &aud, minors[m], adults[a], 0), TG_REFUSE_MINOR);
                CHECK(!talk_safe_romance_ok(&aud, minors[m], adults[a]));
                p2++;
            }
    }
    /* P7: the gate changed no romance state */
    sweep = sim_romance_sweep(-1);
    CHECK_EQ(sweep, 0);
    CHECK_EQ(doomo_town.edge_count, edges_before);
    printf("      200000 random gate calls: %d kid rooms, %d pet rooms, %d K-7 endings, %d adult romances passed;"
           " %d adult/minor pairs both ways\n", p1, p4, p3, passes_adult, p2);
}

/* P5: Mature: never without the option, never for a custom or non-adult */
TEST(talk_safe_mature_gate)
{
    int ids[2], id, any_ok = 0;
    premade_town();
    opt_set(OPT_MATURE, MATURE_OFF);
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id += 7) { ids[0] = id; CHECK(!talk_safe_jmat_ok(ids, 1)); }
    opt_set(OPT_MATURE, MATURE_ON);
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        ids[0] = id;
        CHECK(talk_safe_jmat_ok(ids, 1) == mature_adult_ok(id));
        if (talk_safe_jmat_ok(ids, 1)) { any_ok = 1; CHECK(!talk_safe_minor(id)); }
    }
    CHECK(any_ok);
    ids[0] = ID_NANA; ids[1] = ID_PIP;
    CHECK(!talk_safe_jmat_ok(ids, 2));
    ids[1] = DOOMO_PLAYER_ID;                     /* the hero is no premade: never */
    CHECK(!talk_safe_jmat_ok(ids, 2));
    {
        TalkAudience aud;
        int room[2] = { ID_NANA, ID_PIP };
        talk_safe_ctx(DOOMO_PLAYER_ID, room, 2, 0, &aud);
        CHECK(!aud.mature_ok);
    }
    opt_set(OPT_MATURE, MATURE_OFF);
}

/* P6: whatever is typed, the parser hands on only acts and concepts that may
 * be generated: never the typed words (they stay in the masked bubble) */
TEST(talk_safe_property_closed_vocabulary)
{
    static const char *const junk[] = { "zorp", "fuck", "shit", "asdf", "qwerty", "blorp", "naked", "slur",
        "ghost", "pie", "kid", "sexy", "booze", "damn", "hitler", "suicide", "glorp", "xyzzy", "wank", "nude" };
    int it, bad = 0;
    TalkParseCtx c;
    memset(&c, 0, sizeof c);
    c.nparts = 1; c.name[0] = "Nana Puddin"; c.hops = 1;
    CHECK(open_db());
    rng_s = 4242u;
    for (it = 0; it < 50000; it++) {
        char text[96];
        TalkUtter u;
        int w = 1 + (int)rnd(5), i, o = 0;
        for (i = 0; i < w && o < 80; i++)
            o += snprintf(text + o, sizeof text - (size_t)o, "%s%s", i ? " " : "", junk[rnd(20)]);
        talk_parse(text, &c, &u, NULL);
        for (i = 0; i < 2; i++) {
            TalkConcept tc;
            if (!u.topic[i]) continue;
            if (!talk_db_concept(u.topic[i], &tc) || tc.age == TAGE_NEVER || tc.reg >= TR_SEXUAL) bad++;
        }
        CHECK(u.act[0] < TA_COUNT && u.act[1] < TA_COUNT);
    }
    CHECK_EQ(bad, 0);
    talk_db_close();
}

/* ---- S3: the output lint --------------------------------------------------------- */
TEST(talk_safe_output_lint_units)
{
    TalkAudience adults, kids, pet;
    int sw = 0;
    uint32_t h0;
    CHECK(open_db());
    memset(&adults, 0, sizeof adults);
    memset(&kids, 0, sizeof kids); kids.any_minor = 1;
    memset(&pet, 0, sizeof pet); pet.pet = 1;
    h0 = talk_safe_lint_hits();
    CHECK_EQ(talk_safe_lint("Pie? Sweet pea, it's butter.", &kids, &sw), TL_OK);
    CHECK_EQ(talk_safe_lint("Booze before noon? Bold.", &adults, &sw), TL_OK);
    CHECK_EQ(talk_safe_lint("Booze before noon? Bold.", &kids, &sw), TL_REGISTER_KID);
    CHECK_EQ(talk_safe_lint("Hubba hubba.", &adults, &sw), TL_FORBIDDEN);       /* the sexual class: never generated */
    CHECK_EQ(talk_safe_lint("Hubba hubba.", &pet, &sw), TL_FORBIDDEN);
    CHECK_EQ(talk_safe_lint("Hubba hubba.", &kids, &sw), TL_FORBIDDEN);
    CHECK_EQ(talk_safe_lint("Tug of war in the park!", &kids, &sw), TL_OK);    /* an allowed phrase with "war" in it */
    CHECK_EQ(talk_safe_lint("War is coming.", &kids, &sw), TL_FORBIDDEN);
    CHECK_EQ(talk_safe_lint("Get naked.", &adults, &sw), TL_FORBIDDEN);          /* explicit: never */
    CHECK_EQ(talk_safe_lint("Oh, f-", &adults, &sw), TL_OK);                        /* a stammer is a reviewer's call */
    CHECK_EQ(talk_safe_lint("Well, shit.", &adults, &sw), TL_FORBIDDEN);          /* a capped word: never in Talk */
    CHECK_EQ(talk_safe_lint("What a bastard.", &adults, &sw), TL_FORBIDDEN);
    CHECK_EQ(talk_safe_lint("Bullshit-flavoured pie.", &adults, &sw), TL_FORBIDDEN);
    CHECK_EQ(talk_safe_lint("Let's talk politics.", &adults, &sw), TL_FORBIDDEN);  /* real world */
    CHECK_EQ(talk_safe_lint("I feel hopeless.", &adults, &sw), TL_FORBIDDEN);      /* distress words: the generator never */
    CHECK_EQ(talk_safe_lint("Hell gate's open, damn.", &kids, &sw), TL_REGISTER_KID);
    sw = 0;
    CHECK_EQ(talk_safe_lint("Damn, that's good pie.", &adults, &sw), TL_OK);
    CHECK_EQ(sw, 1);
    CHECK_EQ(talk_safe_lint("Well, crap.", &adults, &sw), TL_SWEAR_CAP);           /* K-9: one a conversation */
    CHECK_EQ(talk_safe_lint("The Hell Breach! Hellvile! Hell.", &kids, &sw), TL_OK);  /* the place, not a swear */
    CHECK_EQ(talk_safe_lint("[R4]{.}Needs salt.", &kids, &sw), TL_OK);
    CHECK_EQ(talk_safe_lint("This line is far, far too long to fit in a bubble of three short lines, so the lint"
                            " says no to it.", &adults, &sw), TL_LENGTH);
    CHECK(talk_safe_lint_hits() > h0);
    {
        /* distress wins over hate: the kind answer, the ugly words unseen */
        TalkUtter u;
        TalkAudience a;
        memset(&u, 0, sizeof u);
        memset(&a, 0, sizeof a);
        strcpy(u.shown, "x");
        u.safety = TS_DISTRESS | TS_HATE;
        CHECK_EQ(talk_safe_gate(&u, &a, DOOMO_PLAYER_ID, ID_NANA, 0), TG_DISTRESS);
        CHECK_EQ(u.act[0], TA_DISTRESS);
        CHECK(u.shown[0] == 0);
    }
    CHECK_EQ(talk_safe_bubble_lines("Pie? Sweet pea, it's mostly butter and secrets.", 26), 2);
    CHECK_EQ(talk_safe_bubble_lines("", 26), 1);
    talk_db_close();
    /* no database: no line passes */
    CHECK_EQ(talk_safe_lint("Pie?", &adults, &sw), TL_NO_DB);
}

/* P1 / K-2 over the vocabulary: every concept form the generator may use in a
 * kids' room lints clean there, and every adults-only register word is
 * caught; nothing never-generated gets through anywhere */
TEST(talk_safe_output_lint_vocabulary)
{
    TalkAudience adults, kids;
    int i, n, kid_ok = 0, adult_reg = 0, never = 0, bad = 0, sw;
    char form[96], line[160];
    CHECK(open_db());
    memset(&adults, 0, sizeof adults);
    memset(&kids, 0, sizeof kids); kids.any_minor = 1;
    n = talk_db_nconcepts();
    for (i = 1; i <= n; i++) {
        TalkConcept c;
        int r;
        if (!talk_db_concept(i, &c) || talk_db_form(i, TF_LEMMA, form, sizeof form) <= 0) continue;
        snprintf(line, sizeof line, "Ooh, %s!", form);
        sw = 0;
        r = talk_safe_lint(line, &kids, &sw);
        if (c.age == TAGE_ANY && c.reg == TR_CLEAN) {
            if (r == TL_OK) kid_ok++;
            else if (r != TL_LENGTH) { bad++; printf("      kid-safe concept refused in a kid room: %s (%d)\n", form, r); }
        } else if (c.reg == TR_CRUDE || c.reg == TR_STRONG) {
            if (r != TL_OK) adult_reg++;
            else { bad++; printf("      register word passed in a kid room: %s\n", form); }
        }
        if (c.age == TAGE_NEVER && c.reg != TR_CLEAN) {
            sw = 0;
            if (talk_safe_lint(line, &adults, &sw) != TL_OK) never++;
            else { bad++; printf("      never-generated word passed: %s\n", form); }
        }
    }
    printf("      vocabulary: %d kid-safe concepts clean in a kid room, %d adult register words caught,"
           " %d never-generated words caught\n", kid_ok, adult_reg, never);
    CHECK_EQ(bad, 0);
    CHECK(kid_ok > 3000);
    CHECK(never >= 80);
    talk_db_close();
}

/* the player's bubble: blocked words are masked, the rest stays */
TEST(talk_safe_bubble_mask)
{
    TalkUtter u;
    TalkParseCtx c;
    memset(&c, 0, sizeof c);
    c.nparts = 1; c.name[0] = "Vex"; c.hops = 1;
    CHECK(open_db());
    talk_parse("you're a fucking genius, vex", &c, &u, NULL);
    CHECK(u.safety & TS_MASKED);
    CHECK(strstr(u.shown, "fucking") == NULL);
    CHECK(strstr(u.shown, "#######") != NULL);
    CHECK(strstr(u.shown, "genius") != NULL);
    c.kid_room = 1;
    talk_parse("pass the booze", &c, &u, NULL);
    CHECK(strstr(u.shown, "booze") == NULL);
    c.kid_room = 0;
    talk_parse("pass the booze", &c, &u, NULL);
    CHECK(strstr(u.shown, "booze") != NULL);
    talk_db_close();
}
