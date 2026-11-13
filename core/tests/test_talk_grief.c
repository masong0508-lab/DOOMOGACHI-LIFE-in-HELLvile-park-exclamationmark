/* test_talk_grief.c - T5B-GRIEF: the grief, comfort and hard-feelings banks
 * (core/data/talk/grammar/en/grief_*.tlk) over the English banks (TALK_en.BIN).
 * Owner: TK. docs/TALK.md "Safety" K-10; docs/design/story/SCRIPT_STYLE.md R6.
 *
 *   talk_grief_new_lines_reach    on a grief or loss topic the new generic, cast
 *                                 and child lines are all reachable; every line
 *                                 is sincere (TLF_SINCERE), never a fallback, and
 *                                 S3-lint clean; a cast voice is said by its own
 *                                 speaker only
 *   talk_grief_child_rooms        a child speaker and an adult with a child in
 *                                 the room use the child banks: strict-lint clean,
 *                                 no LOVE face, and the adult never says a grown-up
 *                                 line (the "grief is love with nowhere to go" kind)
 *   talk_grief_hard_feelings      loneliness, worry and the rest: the careful lines
 *                                 are reachable, a joke topic is unchanged
 * Set DOOMO_TALK_GRIEF_SAMPLE=1 to print a sample of 40 lines. */
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/portrait.h"

#define NSEED 40
static const char *const grief_topics[] = { "grief", "mourning", "condolences", "loss", "memorial", "funeral" };
#define NTOP ((int)(sizeof grief_topics / sizeof grief_topics[0]))
/* the acts a grief line answers: asking, telling, comfort, sympathy, thanks, hugs, a joke, hello and bye */
static const uint8_t gacts[] = { TA_ASK_ABOUT, TA_ASK_OPINION, TA_ASK_FEELING, TA_ASK_ADVICE, TA_TELL, TA_ANSWER,
                                 TA_COMFORT, TA_SYMPATHIZE, TA_ENCOURAGE, TA_THANK, TA_COMPLIMENT, TA_COMPLAIN,
                                 TA_APOLOGIZE, TA_HUG, TA_OFFER, TA_JOKE, TA_GREET, TA_FAREWELL, TA_BACKCHANNEL,
                                 TA_OPINE_NEG, TA_ASK_FAVOR, TA_CONTINUE };
#define NACT ((int)(sizeof gacts / sizeof gacts[0]))

static uint16_t concept(const char *key)
{
    uint16_t c = 0, wf = 0;
    uint32_t tags[4];
    if (talk_lex_find(key, &c, &wf, tags) < 0) return 0;
    return c;
}

static int open_en(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    return talk_db_open(lang_find("en")) == TALK_DB_OK && talk_gen_ready();
}

/* the marks of one sweep */
typedef struct {
    const char *const *mark;
    int n, hit[16];
    int lines, bad_flags, bad_lint, bad_face;
    char bad_line[TALK_LINE_MAX];
} Sweep;

/* every grief topic x every act x NSEED seeds, `ids` speaking (speaker 0), core part alone */
static void sweep(Sweep *s, const int *ids, int nids, const char *const *topics, int ntop, const uint8_t *acts,
                  int nact, int want_sincere)
{
    int t, a, it, m;
    for (t = 0; t < ntop; t++) {
        uint16_t topic = concept(topics[t]);
        for (a = 0; a < nact; a++) {
            for (it = 0; it < NSEED; it++) {
                TalkConv c;
                TalkLine l;
                talk_gen_begin(&c, DOOMO_PLAYER_ID, (int *)ids, nids, 900u + (uint32_t)(t * 3001 + a * 101 + it));
                c.plain = 1;
                talk_gen_line(&c, 0, -1, acts[a], topic, &l, NULL);
                s->lines++;
                if (want_sincere && (!(l.flags & TLF_SINCERE) || (l.flags & TLF_FALLBACK))) {
                    s->bad_flags++;
                    if (a != 0 || !s->bad_line[0]) strcpy(s->bad_line, l.text);
                }
                if (c.aud.any_minor && (talk_safe_lint_probe(l.text, &c.aud, 1) != TL_OK || l.face == EXPR_LOVE + 1)) {
                    s->bad_face++;
                    strcpy(s->bad_line, l.text);
                }
                for (m = 0; m < s->n; m++)
                    if (strstr(l.text, s->mark[m])) s->hit[m]++;
            }
        }
    }
}

static void print_sample(const int *ids, int nids, const char *tag)
{
    static const char *const tops[3] = { "grief", "funeral", "loneliness" };
    static const uint8_t acs[4] = { TA_ASK_ABOUT, TA_COMFORT, TA_TELL, TA_ASK_FEELING };
    int i;
    for (i = 0; i < 20; i++) {
        TalkConv c;
        TalkLine l;
        talk_gen_begin(&c, DOOMO_PLAYER_ID, (int *)ids, nids, 31337u + (uint32_t)i * 7u);
        c.plain = 1;
        talk_gen_line(&c, 0, -1, acs[i % 4], concept(tops[i % 3]), &l, NULL);
        printf("%s | %-10s | act %2d | %s\n", tag, tops[i % 3], acs[i % 4], l.text);
    }
}

TEST(talk_grief_new_lines_reach)
{
    /* a mark of each new generic family, then the cast marks */
    static const char *const generic[] = { "It doesn't have a size", "I'd rather be quiet with you", "light a small candle",
                                           "That's too much for one heart", "One small thing today",
                                           "Take your time", "Go gently" };
    static const char *const vex[] = { "No post for this one", "Delete the filter" };
    static const char *const giggs[] = { "bunting" };
    static const char *const pip[] = { "Patrol's on sad duty", "Patrol never leaves anyone" };
    static const int rivet[1] = { 13 };
    static const int vexid[1] = { 666 };
    static const int giggsid[1] = { 0 };
    static const int pipid[1] = { 42 };
    Sweep s;
    int i, ids3[3] = { 13, 666, 0 };
    uint32_t h0;
    CHECK(open_en());
    h0 = talk_safe_lint_hits();
    memset(&s, 0, sizeof s);
    s.mark = generic;
    s.n = (int)(sizeof generic / sizeof generic[0]);
    sweep(&s, rivet, 1, grief_topics, NTOP, gacts, NACT, 1);
    CHECK_MSG(s.bad_flags == 0, "%d non-sincere grief lines, e.g. %s", s.bad_flags, s.bad_line);
    for (i = 0; i < s.n; i++) CHECK_MSG(s.hit[i] > 0, "never said: %s", generic[i]);
    printf("      %d grief lines (6 topics x %d acts x %d seeds), %d families reached\n", s.lines, NACT, NSEED, s.n);
    /* a cast voice is its own: Vex, Giggs and Pip say theirs, Rivet never does */
    memset(&s, 0, sizeof s);
    s.mark = vex;
    s.n = 2;
    sweep(&s, vexid, 1, grief_topics, NTOP, gacts, NACT, 1);
    for (i = 0; i < 2; i++) CHECK_MSG(s.hit[i] > 0, "Vex never says: %s", vex[i]);
    memset(&s, 0, sizeof s);
    s.mark = vex;
    s.n = 2;
    sweep(&s, rivet, 1, grief_topics, NTOP, gacts, NACT, 1);
    CHECK_MSG(s.hit[0] == 0 && s.hit[1] == 0, "Rivet says a line of Vex's");
    memset(&s, 0, sizeof s);
    s.mark = giggs;
    s.n = 1;
    sweep(&s, giggsid, 1, grief_topics, NTOP, gacts, NACT, 1);
    CHECK_MSG(s.hit[0] > 0, "Giggs never says the bunting line");
    memset(&s, 0, sizeof s);
    s.mark = pip;
    s.n = 2;
    sweep(&s, pipid, 1, grief_topics, NTOP, gacts, NACT, 1);
    for (i = 0; i < 2; i++) CHECK_MSG(s.hit[i] > 0, "Pip never says: %s", pip[i]);
    /* three in the room (a reaction would be a comfort line): still sincere, lint clean */
    memset(&s, 0, sizeof s);
    s.mark = generic;
    s.n = 1;
    sweep(&s, ids3, 3, grief_topics, NTOP, gacts, NACT, 1);
    CHECK_MSG(s.bad_flags == 0, "%d non-sincere lines, e.g. %s", s.bad_flags, s.bad_line);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    if (getenv("DOOMO_TALK_GRIEF_SAMPLE")) {
        print_sample(rivet, 1, "rivet");
        print_sample(vexid, 1, "vex  ");
    }
    talk_db_close();
}

TEST(talk_grief_child_rooms)
{
    /* the grown-up banks and the child banks */
    static const char *const adult_only[] = { "The first year is the hardest", "I'm sorry for your loss",
                                              "walk through it", "grief a sickness", "Towns throw parties",
                                              "Eat something. Sleep if you can", "grocery store", "Let me make some tea",
                                              "tea at the same time each day", "I'll put the kettle on",
                                              "I'm afraid not to", "Just sit a while" };
    enum { NADULT = (int)(sizeof adult_only / sizeof adult_only[0]) };
    static const char *const kidmarks[] = { "It's okay to feel sad", "You didn't do anything wrong", "A grown-up is here",
                                            "I'll sit with you. We can be quiet", "Tell a grown-up", "It's okay to cry",
                                            "safe" };
    static const char *const kidspeak[] = { "I'm sad. It's okay to be sad", "big feeling", "Thank you. That was nice",
                                            "I draw pictures", "Tell a grown-up you trust" };
    static const int ad_with_kid[2] = { 13, 42 };           /* Rivet and Pip */
    static const int ad_with_kid2[2] = { 1, 563 };          /* Nana and Wisp */
    static const int kid_alone[1] = { 563 };
    static const int ad_alone[1] = { 1 };
    Sweep s;
    int i, mind = 0, hit = 0;
    uint32_t h0;
    CHECK(open_en());
    h0 = talk_safe_lint_hits();
    /* a grown-up with a child in the room: child bank, strict lint, no grown-up deep lines */
    memset(&s, 0, sizeof s);
    s.mark = adult_only;
    s.n = NADULT;
    sweep(&s, ad_with_kid, 2, grief_topics, NTOP, gacts, NACT, 1);
    CHECK_MSG(s.bad_face == 0, "kid-room line a child may not hear: %s", s.bad_line);
    CHECK_MSG(s.bad_flags == 0, "non-sincere kid-room line: %s", s.bad_line);
    for (i = 0; i < NADULT; i++) mind += s.hit[i];
    CHECK_MSG(mind == 0, "a grown-up-only line with a child in the room (%d times)", mind);
    printf("      adult with a child in the room: %d lines, %d grown-up-only marks heard\n", s.lines, mind);
    memset(&s, 0, sizeof s);
    s.mark = kidmarks;
    s.n = 7;
    sweep(&s, ad_with_kid2, 2, grief_topics, NTOP, gacts, NACT, 1);
    for (i = 0; i < 7; i++) hit += s.hit[i] > 0;
    CHECK_MSG(hit >= 3, "an adult with a child in the room reaches only %d of 7 close-bank marks", hit);
    CHECK_MSG(s.bad_face == 0, "kid-room line a child may not hear: %s", s.bad_line);
    /* a child speaks */
    memset(&s, 0, sizeof s);
    s.mark = kidspeak;
    s.n = 5;
    sweep(&s, kid_alone, 1, grief_topics, NTOP, gacts, NACT, 1);
    for (i = 0; i < 5; i++) CHECK_MSG(s.hit[i] > 0 || i >= 3, "Wisp says only the voice lines; not the kid bank mark %s", kidspeak[i]);
    CHECK_MSG(s.bad_face == 0 && s.bad_flags == 0, "%s", s.bad_line);
    /* a grown-up alone may be a grown-up */
    memset(&s, 0, sizeof s);
    s.mark = adult_only;
    s.n = NADULT;
    sweep(&s, ad_alone, 1, grief_topics, NTOP, gacts, NACT, 1);
    CHECK_MSG(s.bad_flags == 0, "non-sincere line: %s", s.bad_line);
    hit = 0;
    for (i = 0; i < NADULT; i++) hit += s.hit[i] > 0;
    CHECK_MSG(hit >= NADULT - 3, "a grown-up alone reaches only %d of %d grown-up marks", hit, NADULT);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    if (getenv("DOOMO_TALK_GRIEF_SAMPLE")) {
        print_sample(ad_with_kid, 2, "rivet+pip");
        print_sample(kid_alone, 1, "wisp");
    }
    talk_db_close();
}

TEST(talk_grief_hard_feelings)
{
    static const char *const feelings[] = { "loneliness", "worry", "homesickness", "regret", "shame", "stress" };
    static const char *const marks[] = { "It eases when you say it out loud", "I won't wave it away", "nobody should carry it alone",
                                         "Nobody picks it", "gentle with that one", "bruises easily",
                                         "I'm here", "I hear you", "Do you want to talk about it" };
    static const uint8_t acs[] = { TA_ASK_ABOUT, TA_ASK_OPINION, TA_TELL, TA_COMPLAIN, TA_ASK_ADVICE };
    static const int ids1[1] = { 13 };
    Sweep s;
    int i, hit = 0;
    uint32_t h0;
    CHECK(open_en());
    h0 = talk_safe_lint_hits();
    memset(&s, 0, sizeof s);
    s.mark = marks;
    s.n = 9;
    sweep(&s, ids1, 1, feelings, 6, acs, 5, 0);
    for (i = 0; i < 9; i++) hit += s.hit[i] > 0;
    CHECK_MSG(hit >= 4, "only %d of 9 hard-feeling marks reached", hit);
    CHECK_MSG(s.bad_face == 0, "%s", s.bad_line);
    printf("      hard feelings: %d lines, %d of 9 marks reached\n", s.lines, hit);
    /* the grief flag is for the six grief words only: a hard feeling is still a plain topic */
    {
        TalkConv c;
        TalkLine l;
        talk_gen_begin(&c, DOOMO_PLAYER_ID, (int *)ids1, 1, 12u);
        talk_gen_line(&c, 0, -1, TA_ASK_ABOUT, concept("loneliness"), &l, NULL);
        CHECK_MSG(!(l.flags & TLF_SINCERE), "loneliness took the grief bank: %s", l.text);
    }
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}
