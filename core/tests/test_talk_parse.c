/* test_talk_parse.c - T3: the free-text parser (core/src/talk_lex.c,
 * core/src/talk_parse.c) over the real TALK_en.BIN (make -C core talk,
 * which `unit` depends on). Owner: TK. docs/TALK.md "The parser".
 *
 *   the canonical key (talkc_parse.py canon() is the same), S0 folding
 *   (leet, look-alikes, repeats, spaced letters, starred letters), lemmas,
 *   multi-word keys, persons, edit distance 1, topics, the target, the
 *   bubble mask, determinism, and the labelled corpus
 *   core/data/talk/corpus/parse_en.tsv against the design's bars:
 *   act 90%, topic 85%, addressee 95% (DOOMO_TALK_MISS=1 lists the misses).
 */
#include "test.h"
#include <stdlib.h>
#include <ctype.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/roster.h"

/* talk.h's TA_* names in enum order (talkc_parse.py reads the header the same way) */
static const char *const act_names[TA_COUNT] = {
    "NONE", "GREET", "FAREWELL", "THANK", "APOLOGIZE", "INTRODUCE",
    "ASK_ABOUT", "ASK_OPINION", "ASK_FEELING", "ASK_FACT", "ASK_WHO", "ASK_ADVICE", "ASK_FAVOR", "ASK_REPEAT",
    "TELL", "OPINE_POS", "OPINE_NEG", "AGREE", "DISAGREE", "ANSWER", "YES", "NO",
    "COMPLIMENT", "PRAISE_OTHER", "COMFORT", "ENCOURAGE", "SYMPATHIZE", "JOKE", "LAUGH", "OFFER", "INVITE",
    "PROMISE",
    "INSULT", "CRITICIZE", "TEASE", "MOCK", "COMPLAIN", "BLAME", "DISMISS", "THREATEN",
    "GOSSIP", "DEFEND", "ACCUSE", "TAKE_SIDE", "MEDIATE",
    "CHANGE_TOPIC", "CONTINUE", "BACKCHANNEL", "UNKNOWN",
    "FLIRT", "CONFESS", "ASK_OUT", "KISS",
    "WAVE", "NOD", "SHAKE_HEAD", "HUG", "GIVE", "WALK_AWAY", "HIGH_FIVE",
    "REFUSE_SEXUAL", "REFUSE_MINOR", "REFUSE_HATE", "DISTRESS", "DEFLECT_ROMANCE",
};

static int act_id(const char *name)
{
    int i;
    for (i = 0; i < TA_COUNT; i++) if (strcmp(act_names[i], name) == 0) return i;
    return -1;
}

static const char *core_path(const char *rel, char *buf, int cap)
{
    snprintf(buf, (size_t)cap, "%s/../../%s", test_golden_dir, rel);
    return buf;
}

/* concepts.lock: key -> id */
#define LOCK_MAX 4096
static char lock_key[LOCK_MAX][40];
static int lock_loaded;
static void load_lock(void)
{
    char path[512], line[128];
    FILE *f;
    if (lock_loaded) return;
    lock_loaded = 1;
    f = fopen(core_path("data/talk/concepts.lock", path, sizeof path), "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        int id;
        char key[40];
        if (!isdigit((unsigned char)line[0])) continue;
        if (sscanf(line, "%d\t%39s", &id, key) == 2 && id > 0 && id < LOCK_MAX) memcpy(lock_key[id], key, sizeof key);
    }
    fclose(f);
}
static int concept_of(const char *key)
{
    int i;
    load_lock();
    for (i = 1; i < LOCK_MAX; i++) if (strcmp(lock_key[i], key) == 0) return i;
    return 0;
}
static const char *key_of(int id)
{
    load_lock();
    return id > 0 && id < LOCK_MAX && lock_key[id][0] ? lock_key[id] : "-";
}

/* an absent person: any roster first name (the game passes the town lookup) */
static int roster_person(const char *canon_name)
{
    int i;
    char c[48];
    for (i = 0; i < ROSTER_COUNT; i++) {
        char *sp;
        talk_lex_canon(roster_name(i), c, (int)sizeof c);
        sp = strchr(c, ' ');
        if (sp) *sp = 0;
        if (strcmp(c, canon_name) == 0) return i;
    }
    return -1;
}

static TalkParseCtx room3(void)
{
    static const char *const names[3] = { "Nana Puddin", "Vex", "Rivet" };
    TalkParseCtx c;
    memset(&c, 0, sizeof c);
    c.nparts = 3;
    c.ring = 0;
    c.hops = 1;
    c.name[0] = names[0]; c.name[1] = names[1]; c.name[2] = names[2];
    c.person = roster_person;
    return c;
}

static int open_db(void) { return talk_db_open(lang_find("en")) == TALK_DB_OK; }

static int parse(const char *s, TalkUtter *u, TalkParseInfo *info)
{
    TalkParseCtx c = room3();
    return talk_parse(s, &c, u, info);
}

TEST(talk_parse_canon)
{
    char b[96];
    talk_lex_canon("Dr. Bloom's  LO-FI caf\xc3\xa9!", b, sizeof b);
    CHECK_STR(b, "dr bloom's lo fi cafe");
    talk_lex_canon("  'Hello,   World'  ", b, sizeof b);
    CHECK_STR(b, "hello world");
    talk_lex_canon("rock'n'roll", b, sizeof b);
    CHECK_STR(b, "rock'n'roll");
    talk_lex_canon("S.E.X.", b, sizeof b);
    CHECK_STR(b, "sex");
    talk_lex_canon("abc", b, 3);
    CHECK_STR(b, "ab");
}

TEST(talk_parse_words_and_folding)
{
    TalkTok t[TALK_TOK_MAX];
    TalkParseCtx c = room3();
    uint16_t pie = (uint16_t)concept_of("pie");
    int n;
    CHECK(open_db());
    CHECK(pie > 0);
    n = talk_lex("I LOVE PIE!!", &c, t, TALK_TOK_MAX);
    CHECK_EQ(n, 4);
    CHECK_EQ(t[2].concept, pie);
    CHECK_EQ(t[3].kind, TTK_EXCL);
    /* plural, repeats, leet, look-alikes, lemmas, edit distance 1 */
    n = talk_lex("piiies p13 \xd1\x80ie piee", &c, t, TALK_TOK_MAX);
    CHECK_EQ(n, 4);
    CHECK_EQ(t[0].concept, pie);
    CHECK_EQ(t[1].concept, pie);
    CHECK(t[1].flags & TTF_LEET);
    CHECK_EQ(t[2].concept, pie);
    CHECK_EQ(t[3].concept, pie);                 /* a stretched letter folds back */
    n = talk_lex("chocolat", &c, t, TALK_TOK_MAX);
    CHECK_EQ(n, 1);
    CHECK_EQ(t[0].concept, concept_of("chocolate"));
    CHECK(t[0].flags & TTF_FUZZY);
    /* multi-word keys, longest first */
    n = talk_lex("the hell gate", &c, t, TALK_TOK_MAX);
    CHECK(n >= 1);
    CHECK_EQ(t[n - 1].concept, concept_of("hell_gate"));
    /* persons: full name, first name, possessive; an absent roster name */
    n = talk_lex("Nana Puddin, Vex's hat, rivet", &c, t, TALK_TOK_MAX);
    CHECK(n >= 4);
    CHECK_EQ(t[0].kind, TTK_PERSON); CHECK_EQ(t[0].person, 0);
    CHECK_EQ(t[1].kind, TTK_PERSON); CHECK_EQ(t[1].person, 1);
    CHECK_EQ(t[n - 1].kind, TTK_PERSON); CHECK_EQ(t[n - 1].person, 2);
    /* numbers, "12yo" */
    n = talk_lex("a 12yo", &c, t, TALK_TOK_MAX);
    CHECK(n >= 2);
    CHECK_EQ(t[1].kind, TTK_NUM);
    CHECK_EQ(t[1].value, 12);
    /* spaced-out letters join only into a word the lexicon knows */
    n = talk_lex("p i e", &c, t, TALK_TOK_MAX);
    CHECK_EQ(n, 1);
    CHECK_EQ(t[0].concept, pie);
    n = talk_lex("u r a b c", &c, t, TALK_TOK_MAX);
    CHECK(n >= 3);
    talk_db_close();
}

TEST(talk_parse_acts_topics_targets)
{
    TalkUtter u;
    TalkParseInfo info;
    CHECK(open_db());
    CHECK(parse("Nana, what's in the pie?", &u, &info));
    CHECK_STR(act_names[u.act[0]], "ASK_ABOUT");
    CHECK_STR(key_of(u.topic[0]), "pie");
    CHECK_EQ(u.target, 0);
    CHECK(parse("hey vex!", &u, &info));
    CHECK_STR(act_names[u.act[0]], "GREET");
    CHECK_EQ(u.target, 1);
    CHECK(parse("bye everyone", &u, &info));
    CHECK_STR(act_names[u.act[0]], "FAREWELL");
    CHECK_EQ(u.target, -1);
    CHECK(parse("I love jazz", &u, &info));
    CHECK_STR(act_names[u.act[0]], "OPINE_POS");
    CHECK_STR(key_of(u.topic[0]), "jazz");
    CHECK_EQ(u.target, 0);                       /* the ring */
    CHECK(parse("asdfgh qwerty", &u, &info));
    CHECK_STR(act_names[u.act[0]], "UNKNOWN");
    CHECK_EQ(u.conf, 0);
    talk_db_close();
}

/* a resident named like a word of names_en.txt (a custom one, say) */
static int pedro_person(const char *canon_name) { return strcmp(canon_name, "pedro") == 0 ? 7 : -1; }

/* T4 precision: a real word or a first name one letter from a safety word
 * is that word, not the safety word (docs/TALK.md 6b "Precision") */
TEST(talk_parse_near_miss_words_and_names)
{
    static const char *const words[] = { "invest", "port", "deck", "tweet", "manor", "reward", "homeless", "lofi" };
    TalkTok t[TALK_TOK_MAX];
    TalkParseCtx c = room3();
    unsigned i;
    int n;
    CHECK(open_db());
    for (i = 0; i < sizeof words / sizeof words[0]; i++) {
        uint16_t wf = 0xFFFF;
        CHECK_MSG(talk_lex_known(words[i], &wf), "%s is not a known word", words[i]);
        CHECK_MSG(!(wf & (TW_SEXUAL | TW_SUGGEST | TW_HATE | TW_DISTRESS | TW_MINOR)), "%s has safety flags %04x",
                  words[i], wf);
        n = talk_lex(words[i], &c, t, TALK_TOK_MAX);
        CHECK_EQ(n, 1);
        CHECK_MSG(!(t[0].flags & (TTF_FUZZY | TTF_UNK)), "%s was corrected", words[i]);
        CHECK_EQ(talk_safe_scan(t, n) & (TS_SEXUAL | TS_HATE | TS_DISTRESS | TS_MINOR), 0);
    }
    /* a name nobody in town has: a plain known word, no safety class */
    n = talk_lex("pedro says hi", &c, t, TALK_TOK_MAX);
    CHECK(n >= 1);
    CHECK_EQ(t[0].kind, TTK_WORD);
    CHECK(t[0].wflags & TW_NAME);
    CHECK_EQ(talk_safe_scan(t, n) & (TS_SEXUAL | TS_HATE | TS_MINOR), 0);
    /* the same name when a resident has it: a person (gossip) */
    c.person = pedro_person;
    n = talk_lex("pedro says hi", &c, t, TALK_TOK_MAX);
    CHECK(n >= 1);
    CHECK_EQ(t[0].kind, TTK_PERSON);
    CHECK_EQ(t[0].person, 7);
    CHECK(t[0].flags & TTF_ABSENT);
    /* a name in the room is the participant, as before */
    c = room3();
    n = talk_lex("vex, hi", &c, t, TALK_TOK_MAX);
    CHECK_EQ(t[0].kind, TTK_PERSON);
    CHECK_EQ(t[0].person, 1);
    /* one raw word cut by a mark still joins into a safety word ("hot_tie") */
    n = talk_lex("you're a hot_tie", &c, t, TALK_TOK_MAX);
    CHECK(talk_safe_scan(t, n) & TS_SEXUAL);
    /* two words with a mark between them stay two ("well-known") */
    n = talk_lex("a tie-dye shirt", &c, t, TALK_TOK_MAX);
    CHECK_EQ(talk_safe_scan(t, n) & (TS_SEXUAL | TS_HATE), 0);
    talk_db_close();
}

/* the same line always parses the same; the parser keeps no state */
TEST(talk_parse_deterministic)
{
    static const char *const lines[] = {
        "Nana, what's in the pie?", "u r so funny lol", "did you hear about Mayor Giggs?",
        "I h8 broccoli", "*hugs Rivet*", "can you cook?", "whatever", "Vex is right",
    };
    TalkUtter a, b;
    unsigned i;
    CHECK(open_db());
    for (i = 0; i < sizeof lines / sizeof lines[0]; i++) {
        CHECK(parse(lines[i], &a, NULL));
        CHECK(parse(lines[i], &b, NULL));
        CHECK(memcmp(&a, &b, sizeof a) == 0);
    }
    talk_db_close();
    /* no database: UNKNOWN, no crash */
    CHECK_EQ(parse("hello", &a, NULL), 0);
    CHECK_EQ(a.act[0], TA_UNKNOWN);
}

/* the labelled corpus against the design's bars (3.2) */
TEST(talk_parse_corpus_accuracy)
{
    char path[512], line[512];
    char names[TALK_PARTS_MAX][32];
    FILE *f;
    TalkParseCtx c;
    int n = 0, act_ok = 0, top_ok = 0, tgt_ok = 0, act2_n = 0, act2_ok = 0, miss = getenv("DOOMO_TALK_MISS") != 0;
    int i;
    CHECK(open_db());
    CHECK_EQ(act_id("DEFLECT_ROMANCE"), TA_DEFLECT_ROMANCE);
    memset(&c, 0, sizeof c);
    /* DOOMO_TALK_CORPUS=file measures another corpus (a held-out set) with the same code */
    if (getenv("DOOMO_TALK_CORPUS")) snprintf(path, sizeof path, "%s", getenv("DOOMO_TALK_CORPUS"));
    else core_path("data/talk/corpus/parse_en.tsv", path, sizeof path);
    f = fopen(path, "r");
    CHECK(f != NULL);
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *col[4], *p = line, *acts, *a2;
        TalkUtter u;
        int k, want_act, want_act2 = -1, want_top, want_tgt;
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#') continue;
        if (strncmp(line, "@room", 5) == 0) {
            char *q = line + 5;
            memset(&c, 0, sizeof c);
            c.ring = 0; c.hops = 1; c.person = roster_person;
            while (*q && c.nparts < TALK_PARTS_MAX) {
                char *e;
                while (*q == ' ' || *q == ',') q++;
                if (!*q) break;
                e = strchr(q, ',');
                if (!e) e = q + strlen(q);
                k = (int)(e - q);
                while (k > 0 && q[k - 1] == ' ') k--;
                if (k > 31) k = 31;
                memcpy(names[c.nparts], q, (size_t)k);
                names[c.nparts][k] = 0;
                c.name[c.nparts] = names[c.nparts];
                c.nparts++;
                q = e;
            }
            continue;
        }
        for (k = 0; k < 4; k++) {
            col[k] = p;
            p = strchr(p, '\t');
            if (!p) break;
            *p++ = 0;
        }
        CHECK_MSG(k == 3, "corpus line %d: 4 columns", n + 1);
        if (k != 3) continue;
        acts = col[1];
        a2 = strchr(acts, ',');
        if (a2) { *a2++ = 0; want_act2 = act_id(a2); }
        want_act = act_id(acts);
        CHECK_MSG(want_act >= 0, "act %s", acts);
        want_top = strcmp(col[2], "-") == 0 ? 0 : concept_of(col[2]);
        CHECK_MSG(strcmp(col[2], "-") == 0 || want_top > 0, "topic %s", col[2]);
        if (strcmp(col[3], "all") == 0) want_tgt = -1;
        else if (strcmp(col[3], "-") == 0) want_tgt = c.ring;
        else {
            want_tgt = -9;
            for (i = 0; i < c.nparts; i++) if (strcmp(c.name[i], col[3]) == 0) want_tgt = i;
        }
        talk_parse(col[0], &c, &u, NULL);
        n++;
        if (u.act[0] == want_act) act_ok++;
        else if (miss) printf("      ACT   %-40s want %-12s got %s\n", col[0], act_names[want_act], act_names[u.act[0]]);
        if (u.topic[0] == want_top) top_ok++;
        else if (miss) printf("      TOPIC %-40s want %-12s got %s\n", col[0], col[2], key_of(u.topic[0]));
        if (u.target == want_tgt) tgt_ok++;
        else if (miss) printf("      TGT   %-40s want %d got %d\n", col[0], want_tgt, u.target);
        if (want_act2 >= 0) { act2_n++; if (u.act[1] == want_act2) act2_ok++; }
    }
    fclose(f);
    talk_db_close();
    printf("      corpus: %d lines; act %d.%d%%, topic %d.%d%%, addressee %d.%d%% (second act %d of %d)\n", n,
           n ? act_ok * 100 / n : 0, n ? act_ok * 1000 / n % 10 : 0, n ? top_ok * 100 / n : 0,
           n ? top_ok * 1000 / n % 10 : 0, n ? tgt_ok * 100 / n : 0, n ? tgt_ok * 1000 / n % 10 : 0,
           act2_ok, act2_n);
    if (getenv("DOOMO_TALK_CORPUS")) return;            /* a held-out set: report only */
    CHECK(n >= 1500);
    CHECK_MSG(act_ok * 100 >= n * 90, "act accuracy %d/%d under 90%%", act_ok, n);
    CHECK_MSG(top_ok * 100 >= n * 85, "topic accuracy %d/%d under 85%%", top_ok, n);
    CHECK_MSG(tgt_ok * 100 >= n * 95, "addressee accuracy %d/%d under 95%%", tgt_ok, n);
}

/* DOOMO_TALK_LINE="text" prints how the parser tags and parses one line
 * (a writer's and reviewer's tool; no checks) */
TEST(talk_parse_debug_line)
{
    const char *line = getenv("DOOMO_TALK_LINE");
    TalkUtter u;
    TalkParseInfo info;
    int i;
    if (!line) return;
    CHECK(open_db());
    parse(line, &u, &info);
    for (i = 0; i < info.ntok; i++)
        printf("      tok %-16s concept %-20s kind %d flags %02x wflags %04x person %d tags %08x %08x %08x %08x\n",
               info.tok[i].text, key_of(info.tok[i].concept), info.tok[i].kind, info.tok[i].flags,
               info.tok[i].wflags, info.tok[i].person, info.tok[i].tags[0], info.tok[i].tags[1], info.tok[i].tags[2],
               info.tok[i].tags[3]);
    printf("      act %s %s topic %s %s target %d tone %d safety %02x conf %d rule %d %d shown '%s'\n",
           act_names[u.act[0]], act_names[u.act[1]], key_of(u.topic[0]), key_of(u.topic[1]), u.target, u.tone,
           u.safety, u.conf, info.rule[0], info.rule[1], u.shown);
    talk_db_close();
}
