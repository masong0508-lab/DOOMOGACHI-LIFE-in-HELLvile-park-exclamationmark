/* test_talk_quick.c - Talk: personalities that react differently, and the quick replies of the Talk screen.
 * Owner: TK. core/src/talk_dm.c (reaction, talk_dm_expect), core/src/talk_scene.c (talk_quick_*).
 * docs/TALK.md 6i.
 *
 * Over the real TALK_en.BIN. The persona tests set the five sliders, the mood and the bond of two
 * residents and check the same act lands differently, in the number (talk_dm_expect) and in what a
 * session leaves behind (the bond change). The quick-reply tests build the line a resident just said
 * by hand, so every situation is reached, and check what is offered: the right kind of reply, never a
 * duplicate act, nothing romantic, no snark at a kids' room, no joke about grief, a reply that suits the
 * resident's persona in the second slot, and that every reply goes through the drama manager into a
 * non-empty answer with the S3 lint counting nothing.
 */
#include "test.h"
#include <stdlib.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/roster.h"
#include "doomo/sim.h"
#include "doomo/town.h"
#include "doomo/resident_acc.h"
#include "doomo/talk_traits.h"

#define ID_NANA  1
#define ID_RIVET 13
#define ID_PIP   42           /* kid cast */

static int open_db(void) { return talk_db_open(lang_find("en")) == TALK_DB_OK && talk_dm_ready(); }

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

static void set_persona(int id, int speech, int expr, int att, int quirk, int move)
{
    DoomoResident *r = town_res(id);
    RES_SET_PERSONA(r, PERS_SPEECH, speech);
    RES_SET_PERSONA(r, PERS_EXPRESSION, expr);
    RES_SET_PERSONA(r, PERS_ATTITUDE, att);
    RES_SET_PERSONA(r, PERS_QUIRK, quirk);
    RES_SET_PERSONA(r, PERS_MOVEMENT, move);
}

static void begin2(TalkState *s, int a, int b, uint32_t seed)
{
    int ids[2], n = 0;
    ids[n++] = a;
    if (b >= 0) ids[n++] = b;
    talk_dm_begin(s, DOOMO_PLAYER_ID, ids, n, seed);
}

/* a resident line made by hand: who, to (-1 the player), reply act, topic, flags */
static void put_line(TalkState *s, int who, int to, int act, uint16_t topic, unsigned flags)
{
    TalkLine *l = &s->line[0];
    memset(s->line, 0, sizeof s->line);
    snprintf(l->text, sizeof l->text, "A line.");
    l->who = (int8_t)who;
    l->to = (int8_t)to;
    l->act = (uint8_t)act;
    l->topic = topic;
    l->flags = (uint8_t)flags;
    s->nlines = 1;
}

/* say a reply through the drama manager; returns the lines made */
static int say(TalkState *s, const TalkSugg *g, int target)
{
    TalkUtter u;
    int n = 0;
    memset(&u, 0, sizeof u);
    u.act[0] = g->act;
    u.topic[0] = g->topic;
    u.target = (int8_t)target;
    u.tone = g->tone;
    u.conf = 15;
    snprintf(u.shown, sizeof u.shown, "%s", g->text);
    talk_dm_say(s, &u);
    while (talk_dm_step(s, NULL)) n++;
    return n;
}

/* ---- personalities: the same act lands differently ---------------------------------------- */
TEST(talk_quick_persona_changes_the_reaction)
{
    TalkState s;
    premade_town();
    CHECK(open_db());
    set_persona(ID_NANA, 14, 13, 13, 13, 12);     /* blunt, expressive, easygoing, unhinged, quick */
    set_persona(ID_RIVET, 2, 2, 2, 2, 2);         /* polite, flat, serious, normal, slow           */
    town_res(ID_NANA)->mood = MOOD_OK;
    town_res(ID_RIVET)->mood = MOOD_OK;
    begin2(&s, ID_NANA, ID_RIVET, 11u);
    /* a tease and a snarky line are banter to the first and an insult to the second */
    CHECK(talk_dm_expect(&s, 0, TA_TEASE, TONE_CHEEKY, TC_NONE) > talk_dm_expect(&s, 1, TA_TEASE, TONE_CHEEKY, TC_NONE));
    CHECK(talk_dm_expect(&s, 0, TA_JOKE, TONE_SNARKY, TC_NONE) > talk_dm_expect(&s, 1, TA_JOKE, TONE_SNARKY, TC_NONE));
    /* a disagreement does not bother the blunt one and stings the polite one */
    CHECK(talk_dm_expect(&s, 0, TA_DISAGREE, TONE_PLAIN, TC_NONE) > talk_dm_expect(&s, 1, TA_DISAGREE, TONE_PLAIN, TC_NONE));
    /* an apology matters more to the polite one; a hug to the expressive one */
    CHECK(talk_dm_expect(&s, 1, TA_APOLOGIZE, TONE_PLAIN, TC_NONE) > talk_dm_expect(&s, 0, TA_APOLOGIZE, TONE_PLAIN, TC_NONE));
    CHECK(talk_dm_expect(&s, 0, TA_HUG, TONE_PLAIN, TC_NONE) > talk_dm_expect(&s, 1, TA_HUG, TONE_PLAIN, TC_NONE));
    /* the pet, a bad slot and no state are all 0 */
    CHECK_EQ(talk_dm_expect(&s, 5, TA_TEASE, TONE_PLAIN, TC_NONE), 0);
    CHECK_EQ(talk_dm_expect(&s, -1, TA_TEASE, TONE_PLAIN, TC_NONE), 0);
    CHECK_EQ(talk_dm_expect(NULL, 0, TA_TEASE, TONE_PLAIN, TC_NONE), 0);
    /* nothing leaves the -10..8 range */
    {
        int a, t;
        for (a = 1; a < TA_COUNT; a++)
            for (t = 0; t < TONE_COUNT; t++) {
                int v = talk_dm_expect(&s, 0, a, t, TC_NONE);
                CHECK_MSG(v >= -10 && v <= 8, "act %d tone %d -> %d", a, t, v);
            }
    }
    talk_db_close();
}

TEST(talk_quick_mood_and_bond_change_the_reaction)
{
    TalkState s;
    premade_town();
    CHECK(open_db());
    set_persona(ID_NANA, 6, 6, 6, 6, 6);
    begin2(&s, ID_NANA, ID_RIVET, 12u);
    town_res(ID_NANA)->mood = MOOD_SAD;            /* a sad one wants comfort, not a joke */
    CHECK(talk_dm_expect(&s, 0, TA_COMFORT, TONE_PLAIN, TC_NONE) > talk_dm_expect(&s, 0, TA_JOKE, TONE_PLAIN, TC_NONE));
    town_res(ID_NANA)->mood = MOOD_ANGRY;          /* an angry one takes an apology, not a tease */
    CHECK(talk_dm_expect(&s, 0, TA_APOLOGIZE, TONE_PLAIN, TC_NONE) > talk_dm_expect(&s, 0, TA_TEASE, TONE_PLAIN, TC_NONE));
    town_res(ID_NANA)->mood = MOOD_OK;
    {
        int friend, rival;
        rel_get_or_add(ID_NANA, DOOMO_PLAYER_ID);
        rel_adjust(ID_NANA, DOOMO_PLAYER_ID, 80);
        friend = talk_dm_expect(&s, 0, TA_MOCK, TONE_PLAIN, TC_NONE);
        rel_adjust(ID_NANA, DOOMO_PLAYER_ID, -160);
        rival = talk_dm_expect(&s, 0, TA_MOCK, TONE_PLAIN, TC_NONE);
        CHECK(friend > rival);                     /* a friend's jab is banter, a rival's is not */
    }
    talk_db_close();
}

/* A number-only friendship meter is not enough: the resident remembers
 * whether the previous exchange was hurtful, kind, or left a promise open.
 * The game already serializes these bits. No new save field is required.
 * The SAME reaction estimator powers both reply suggestions and outcomes. */
TEST(talk_quick_past_experience_changes_reception)
{
    TalkState s, reopened;
    int apology, tease, greet, promise;
    premade_town();
    CHECK(open_db());
    set_persona(ID_NANA, 7, 7, 7, 7, 7);
    town_res(ID_NANA)->mood = MOOD_OK;
    begin2(&s, ID_NANA, -1, 311u);
    apology = talk_dm_expect(&s, 0, TA_APOLOGIZE, TONE_PLAIN, TC_NONE);
    tease = talk_dm_expect(&s, 0, TA_TEASE, TONE_PLAIN, TC_NONE);
    greet = talk_dm_expect(&s, 0, TA_GREET, TONE_PLAIN, TC_NONE);
    promise = talk_dm_expect(&s, 0, TA_PROMISE, TONE_PLAIN, TC_NONE);

    s.conv.mem_flags[0] = TMF_INSULTED;
    CHECK(talk_dm_expect(&s, 0, TA_APOLOGIZE, TONE_PLAIN, TC_NONE) > apology);
    CHECK(talk_dm_expect(&s, 0, TA_TEASE, TONE_PLAIN, TC_NONE) < tease);

    s.conv.mem_flags[0] = TMF_COMPLIMENTED;
    CHECK(talk_dm_expect(&s, 0, TA_GREET, TONE_PLAIN, TC_NONE) > greet);

    s.conv.mem_flags[0] = TMF_PROMISED;
    CHECK(talk_dm_expect(&s, 0, TA_PROMISE, TONE_PLAIN, TC_NONE) < promise);

    /* Save a *real* previous insult through the current 12-byte memory
     * table, then resume with the normal Talk memory-loading entrypoint. */
    talk_mem_set_flags(ID_NANA, TMF_INSULTED);
    talk_mem_end(&s.conv);
    begin2(&reopened, ID_NANA, -1, 312u);
    talk_mem_begin(&reopened.conv);
    CHECK(reopened.conv.mem_flags[0] & TMF_INSULTED);
    CHECK(talk_dm_expect(&reopened, 0, TA_APOLOGIZE, TONE_PLAIN, TC_NONE) > apology);
    CHECK(talk_dm_expect(&reopened, 0, TA_TEASE, TONE_PLAIN, TC_NONE) < tease);
    talk_db_close();
}

/* the same words said to two people leave different bonds behind */
TEST(talk_quick_persona_changes_the_bond)
{
    TalkState a, b;
    uint32_t h0 = talk_safe_lint_hits();
    int t;
    premade_town();
    CHECK(open_db());
    set_persona(ID_NANA, 14, 13, 13, 13, 12);
    set_persona(ID_RIVET, 2, 2, 2, 2, 2);
    town_res(ID_NANA)->mood = MOOD_OK;
    town_res(ID_RIVET)->mood = MOOD_OK;
    begin2(&a, ID_NANA, ID_RIVET, 77u);
    begin2(&b, ID_RIVET, ID_NANA, 77u);
    for (t = 0; t < 3; t++) {
        TalkSugg g;
        g.topic = TC_NONE;
        g.act = TA_TEASE;
        g.tone = TONE_SNARKY;
        g.text = "Oh, come on.";
        say(&a, &g, 0);                            /* teased Nana (the easygoing one) */
        say(&b, &g, 0);                            /* teased Rivet (the polite one)   */
    }
    CHECK_MSG(a.part[0].rel_d_player > b.part[0].rel_d_player, "easygoing %d vs polite %d", a.part[0].rel_d_player, b.part[0].rel_d_player);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- quick replies -------------------------------------------------------------------------- */
static int has_act(const TalkSugg *g, int n, int act)
{
    int i;
    for (i = 0; i < n; i++) if (g[i].act == act) return 1;
    return 0;
}

TEST(talk_quick_replies_follow_what_was_said)
{
    TalkState s;
    TalkSugg g[TALK_QUICK_MAX];
    int n, i, j;
    uint16_t pie;
    premade_town();
    CHECK(open_db());
    pie = concept("pie");
    begin2(&s, ID_NANA, -1, 21u);
    /* nothing said yet: no thread, but there are still ways to start; the talk over: nothing offered */
    s.nlines = 0;
    CHECK(talk_quick_thread(&s, NULL) == NULL);
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 0, g);
    CHECK(n >= 1 && n <= TALK_QUICK_MAX);
    for (i = 0; i < n; i++) CHECK(!TA_IS_ROMANCE(g[i].act));
    put_line(&s, 0, -1, TA_ASK_OPINION, pie, 0);
    s.flags |= TDF_OVER;
    CHECK_EQ(talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 0, g), 0);
    s.flags &= (uint8_t)~TDF_OVER;
    /* a question about a topic: answers about that topic */
    put_line(&s, 0, -1, TA_ASK_OPINION, pie, 0);
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 3, g);
    CHECK(n >= 2 && n <= TALK_QUICK_MAX);
    for (i = 0; i < n; i++) {
        CHECK(g[i].text && g[i].text[0]);
        CHECK(!TA_IS_ROMANCE(g[i].act) && !TA_IS_SAFETY(g[i].act));
        CHECK(talk_gen_act_class(g[i].act) != TAC_ROMANCE);
        for (j = i + 1; j < n; j++) CHECK(g[i].act != g[j].act);          /* an act is never offered twice */
        CHECK_EQ(g[i].topic, pie);                                          /* the talk stays on the topic */
    }
    CHECK(has_act(g, n, TA_OPINE_POS) || has_act(g, n, TA_OPINE_NEG) || has_act(g, n, TA_ANSWER) || has_act(g, n, TA_JOKE) ||
          has_act(g, n, TA_ASK_OPINION));
    /* a kind word: thanks */
    put_line(&s, 0, -1, TA_COMPLIMENT, TC_NONE, 0);
    for (i = 0; i < 6; i++) {
        n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, (uint32_t)i, g);
        CHECK(n >= 2);
        CHECK(has_act(g, n, TA_THANK) || has_act(g, n, TA_COMPLIMENT));
    }
    /* a sincere line (a refusal, grief, distress): comfort and kindness only, never a joke */
    put_line(&s, 0, -1, TA_TELL, TC_NONE, TLF_SINCERE);
    for (i = 0; i < 12; i++) {
        n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, (uint32_t)i, g);
        CHECK(n >= 2);
        for (j = 0; j < n; j++)
            CHECK_MSG(g[j].act == TA_SYMPATHIZE || g[j].act == TA_COMFORT || g[j].act == TA_OFFER || g[j].act == TA_ENCOURAGE,
                      "act %d after a sincere line", g[j].act);
    }
    /* deterministic: the same salt gives the same replies; a new salt varies them */
    {
        TalkSugg h[TALK_QUICK_MAX];
        const char *seen[24];
        int ns = 0, k;
        put_line(&s, 0, -1, TA_TELL, pie, 0);
        n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 5, g);
        CHECK_EQ(talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 5, h), n);
        for (i = 0; i < n; i++) CHECK(g[i].act == h[i].act && g[i].text == h[i].text);
        for (i = 0; i < 40; i++) {
            n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, (uint32_t)i, g);
            for (j = 0; j < n; j++) {
                int dup = 0;
                for (k = 0; k < ns; k++) if (seen[k] == g[j].text) dup = 1;
                if (!dup && ns < 24) seen[ns++] = g[j].text;
            }
        }
        CHECK_MSG(ns >= 5, "only %d different replies over 40 salts", ns);
    }
    talk_db_close();
}

/* the second reply is the one that suits the resident: persona decides what is offered */
TEST(talk_quick_replies_suit_the_resident)
{
    TalkState s;
    TalkSugg g[TALK_QUICK_MAX];
    int i, n;
    premade_town();
    CHECK(open_db());
    set_persona(ID_NANA, 8, 6, 14, 6, 6);          /* easygoing */
    set_persona(ID_RIVET, 8, 6, 1, 6, 6);          /* serious   */
    town_res(ID_NANA)->mood = MOOD_OK;
    town_res(ID_RIVET)->mood = MOOD_OK;
    begin2(&s, ID_NANA, ID_RIVET, 31u);
    put_line(&s, 0, -1, TA_ASK_FEELING, TC_NONE, 0);
    for (i = 0; i < 12; i++) {
        n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, (uint32_t)i, g);
        CHECK(n >= 2);
        CHECK_MSG(g[1].act == TA_JOKE, "easygoing: second reply act %d", g[1].act);
    }
    put_line(&s, 1, -1, TA_ASK_FEELING, TC_NONE, 0);
    for (i = 0; i < 12; i++) {
        n = talk_quick_replies(&s, 1, TC_NONE, TC_NONE, (uint32_t)i, g);
        CHECK(n >= 2);
        CHECK_MSG(g[1].act != TA_JOKE, "serious: second reply act %d", g[1].act);
    }
    /* a sad resident is not offered a joke in the second slot either */
    town_res(ID_NANA)->mood = MOOD_SAD;
    put_line(&s, 0, -1, TA_TELL, TC_NONE, 0);
    for (i = 0; i < 12; i++) {
        n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, (uint32_t)i, g);
        CHECK(n >= 2);
        CHECK_MSG(g[1].act == TA_SYMPATHIZE, "sad: second reply act %d", g[1].act);
    }
    talk_db_close();
}

/* An old insult should invite reconciliation even if the resident is
 * normally playful. Today's urgent medical/emotional needs remain above
 * yesterday's memories in the priority order. */
TEST(talk_quick_reconciliation_uses_actual_saved_social_context)
{
    TalkState s;
    TalkSugg g[TALK_QUICK_MAX];
    DoomoResident *r;
    int n;
    premade_town();
    CHECK(open_db());
    r = town_res(ID_NANA);
    set_persona(ID_NANA, 8, 8, 13, 12, 8);
    r->mood = MOOD_OK;
    begin2(&s, ID_NANA, -1, 371u);
    put_line(&s, 0, -1, TA_ASK_FEELING, TC_NONE, 0);
    s.conv.mem_flags[0] = TMF_INSULTED;
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 2u, g);
    CHECK(n >= 2);
    CHECK_EQ(g[1].act, TA_APOLOGIZE);
    s.conv.mem_flags[0] = 0;
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 2u, g);
    CHECK(n >= 2);
    CHECK_EQ(g[1].act, TA_JOKE);
    s.conv.mem_flags[0] = TMF_INSULTED;
    r->mood = MOOD_SICK;
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 2u, g);
    CHECK(n >= 2);
    CHECK_EQ(g[1].act, TA_OFFER);
    talk_db_close();
}

/* Two created citizens with identical persona sliders but distinct
 * explicitly chosen traits should invite visibly different responses.
 * The room's emotional state still overrides the trait choice. */
TEST(talk_quick_creator_traits_steer_the_followup_without_overriding_needs)
{
    TalkState s;
    TalkSugg g[TALK_QUICK_MAX];
    TalkTraitSet ts;
    int n;
    premade_town();
    CHECK(open_db());
    set_persona(ID_NANA, 8, 6, 6, 6, 6);
    set_persona(ID_RIVET, 8, 6, 6, 6, 6);
    town_res(ID_NANA)->mood = MOOD_OK;
    town_res(ID_RIVET)->mood = MOOD_OK;
    talk_traitset_clear(&ts);
    CHECK_EQ(talk_traitset_add(&ts, TT_GOOFBALL), TTE_OK);
    CHECK_EQ(talk_trait_set(ID_NANA, &ts), TTE_OK);
    talk_traitset_clear(&ts);
    CHECK_EQ(talk_traitset_add(&ts, TT_SHY), TTE_OK);
    CHECK_EQ(talk_trait_set(ID_RIVET, &ts), TTE_OK);
    begin2(&s, ID_NANA, ID_RIVET, 64u);
    put_line(&s, 0, -1, TA_ASK_FEELING, TC_NONE, 0);
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 1u, g);
    CHECK(n >= 2);
    CHECK_EQ(g[1].act, TA_JOKE);
    put_line(&s, 1, -1, TA_ASK_FEELING, TC_NONE, 0);
    n = talk_quick_replies(&s, 1, TC_NONE, TC_NONE, 1u, g);
    CHECK(n >= 2);
    CHECK_EQ(g[1].act, TA_THANK);
    /* An actual illness remains more important than a funny trait. */
    town_res(ID_NANA)->mood = MOOD_SICK;
    put_line(&s, 0, -1, TA_ASK_FEELING, TC_NONE, 0);
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 1u, g);
    CHECK(n >= 2);
    CHECK_EQ(g[1].act, TA_OFFER);
    talk_db_close();
}

TEST(talk_quick_replies_are_safe)
{
    TalkState s;
    TalkSugg g[TALK_QUICK_MAX];
    int i, j, n, la;
    static const int says[] = { TA_GREET, TA_INTRODUCE, TA_ASK_ABOUT, TA_ASK_OPINION, TA_ASK_FEELING, TA_ASK_FACT, TA_TELL,
                                TA_GOSSIP, TA_COMPLIMENT, TA_THANK, TA_COMPLAIN, TA_INSULT, TA_MOCK, TA_TEASE, TA_CHANGE_TOPIC,
                                TA_BACKCHANNEL, TA_FAREWELL, TA_JOKE };
    uint16_t funeral;
    premade_town();
    CHECK(open_db());
    funeral = concept("funeral");
    CHECK(funeral != TC_NONE);
    /* a kids' room: no snark and no dismissal, whatever the resident said */
    {
        int ids[2] = { ID_NANA, ID_PIP };
        talk_dm_begin(&s, DOOMO_PLAYER_ID, ids, 2, 41u);
        CHECK(s.conv.aud.any_minor);
        for (la = 0; la < (int)(sizeof says / sizeof says[0]); la++) {
            put_line(&s, 0, -1, says[la], concept("pie"), 0);
            for (i = 0; i < 20; i++) {
                n = talk_quick_replies(&s, 0, TC_NONE, concept("music"), (uint32_t)i, g);
                for (j = 0; j < n; j++) CHECK(g[j].act != TA_DISMISS && g[j].tone != TONE_SNARKY);
            }
        }
    }
    /* grief: no joke, tease, laugh or dismissal, in a thread or in the topic in play */
    begin2(&s, ID_NANA, ID_RIVET, 42u);
    for (la = 0; la < (int)(sizeof says / sizeof says[0]); la++) {
        put_line(&s, 0, -1, says[la], funeral, 0);
        for (i = 0; i < 20; i++) {
            n = talk_quick_replies(&s, 0, TC_NONE, concept("music"), (uint32_t)i, g);
            for (j = 0; j < n; j++) {
                CHECK(g[j].act != TA_JOKE && g[j].act != TA_TEASE && g[j].act != TA_LAUGH && g[j].act != TA_DISMISS);
                CHECK(g[j].topic != funeral || g[j].act != TA_CHANGE_TOPIC);
            }
        }
    }
    talk_db_close();
}

/* A prior, saved topic is a context cue, not a new allocation for each
 * resident. The recent topic always takes precedence over memory. */
TEST(talk_quick_reopens_last_subject_without_forgetting_the_thread)
{
    TalkState s;
    TalkSugg g[TALK_QUICK_MAX];
    uint16_t pie;
    int n, i;
    premade_town();
    CHECK(open_db());
    pie = concept("pie");
    CHECK(pie != TC_NONE);
    begin2(&s, ID_NANA, -1, 19u);
    s.conv.mem_topic[0] = pie;
    s.topic_stack[0] = TC_NONE;
    put_line(&s, 0, -1, TA_GREET, TC_NONE, 0);
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 2u, g);
    CHECK(n >= 2);
    for (i = 0; i < n; i++) CHECK_EQ(g[i].topic, pie);
    put_line(&s, 0, -1, TA_ASK_OPINION, concept("dancing"), 0);
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 2u, g);
    CHECK(n >= 2);
    for (i = 0; i < n; i++) CHECK_EQ(g[i].topic, concept("dancing"));
    talk_db_close();
}

/* A seasoned conversation should not keep proposing the player's last
 * sentence just because the seeded list happened to rotate to it. The
 * deliberative ranker uses the real DM reaction model, then applies a small
 * short-term novelty penalty; there is no heap or trained weights table. */
TEST(talk_quick_predictive_policy_discourages_repeat_acts)
{
    TalkState s;
    TalkSugg before[TALK_QUICK_MAX], after[TALK_QUICK_MAX], copy[TALK_QUICK_MAX];
    int changed = 0, i, n, m;
    uint16_t pie;
    premade_town();
    CHECK(open_db());
    pie = concept("pie");
    CHECK(pie != TC_NONE);
    begin2(&s, ID_NANA, -1, 118u);
    put_line(&s, 0, -1, TA_TELL, pie, 0);
    s.topic_stack[0] = pie;
    for (i = 0; i < 32; i++) {
        s.turn = 0;
        s.nhist = 0;
        s.last_act = TA_NONE;
        n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, (uint32_t)i, before);
        CHECK(n >= 2);
        memset(s.hist, 0, sizeof s.hist);
        s.hist[0].who = 0xffu;
        s.hist[0].flags = THF_PLAYER;
        s.hist[0].act = before[0].act;
        s.hist[0].topic = pie;
        s.nhist = 1;
        s.turn = 4;
        s.last_act = before[0].act;
        m = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, (uint32_t)i, after);
        CHECK(m >= 2);
        if (after[0].act != before[0].act) changed++;
        CHECK_EQ(talk_quick_replies(&s, 0, TC_NONE, TC_NONE, (uint32_t)i, copy), m);
        CHECK_EQ(after[0].act, copy[0].act);   /* same brain state: deterministic */
        CHECK(after[0].topic == pie && !TA_IS_ROMANCE(after[0].act));
    }
    CHECK_MSG(changed >= 23, "repeat was only discouraged in %d/32 seeded conversations", changed);
    talk_db_close();
}

/* The DM keeps an in-session bond delta and engagement score. The
 * quick planner should react to that recent feedback, not just the saved
 * relationship from before the talk began. The player can still manually
 * select an edgy act from its category page; this only improves suggestions. */
TEST(talk_quick_predictive_policy_can_repair_a_heated_conversation)
{
    TalkState s;
    TalkSugg g[TALK_QUICK_MAX];
    int i, n;
    premade_town();
    CHECK(open_db());
    begin2(&s, ID_NANA, -1, 119u);
    put_line(&s, 0, -1, TA_COMPLAIN, TC_NONE, 0);
    s.turn = 4;
    s.nhist = 1;
    s.hist[0].who = 0xffu;
    s.hist[0].act = TA_ASK_FEELING;
    s.hist[0].flags = THF_PLAYER;
    s.last_act = TA_ASK_FEELING;
    s.part[0].rel_d_player = -8;     /* bad exchange in THIS session */
    s.part[0].engage = 18;          /* at risk of walking away */
    s.tension = TALK_TENSION_ARGUE + 1;
    for (i = 0; i < 24; i++) {
        n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, (uint32_t)i, g);
        CHECK(n >= 2);
        CHECK_MSG(g[0].act == TA_SYMPATHIZE || g[0].act == TA_COMFORT,
                  "heated talk suggested act %d instead of a repair", g[0].act);
        CHECK(g[0].act != TA_JOKE && g[0].act != TA_TEASE);
    }
    talk_db_close();
}

/* A resident's *actual saved subject*, when explicitly associated with
 * what they are discussing now, makes a fourth-slot segue rather than an
 * unrelated forced topic change. It remains optional and respects grief. */
TEST(talk_quick_personal_semantic_bridge_is_earned_and_opt_in)
{
    TalkState s;
    TalkSugg g[TALK_QUICK_MAX];
    uint16_t band, song;
    uint16_t links[TALK_ASSOC_TOP];
    int n, i, found = 0, bridge = -1;
    premade_town();
    CHECK(open_db());
    band = concept("band");
    song = concept("song");
    CHECK(band != TC_NONE && song != TC_NONE);
    n = talk_db_assoc(band, links, NULL);
    for (i = 0; i < n; i++) if (links[i] == song) found = 1;
    CHECK(found);
    begin2(&s, ID_NANA, -1, 218u);
    put_line(&s, 0, -1, TA_TELL, band, 0);
    s.topic_stack[0] = band;
    s.conv.mem_topic[0] = song;
    s.turn = 4;
    s.last_act = TA_GREET;
    n = talk_quick_replies(&s, 0, band, TC_NONE, 33u, g);
    CHECK(n >= 3);
    for (i = 0; i < n; i++)
        if (g[i].act == TA_CHANGE_TOPIC && g[i].topic == song) bridge = i;
    CHECK_EQ(bridge, n - 1);
    /* Do not keep suggesting a change after the player just changed subject. */
    s.last_act = TA_CHANGE_TOPIC;
    n = talk_quick_replies(&s, 0, band, TC_NONE, 33u, g);
    CHECK(!has_act(g, n, TA_CHANGE_TOPIC));
    /* The context exists but no personal association was saved: no segue. */
    s.last_act = TA_GREET;
    s.conv.mem_topic[0] = TC_NONE;
    n = talk_quick_replies(&s, 0, band, TC_NONE, 33u, g);
    CHECK(!has_act(g, n, TA_CHANGE_TOPIC));
    /* Rehearsed topics supply the same optional bridge without a saved
     * last topic, and a bridge avoids circling back to the prior subject. */
    talk_link_note(ID_NANA, s.conv.avatar, band, song);
    n = talk_quick_replies(&s, 0, band, TC_NONE, 33u, g);
    CHECK(!has_act(g, n, TA_CHANGE_TOPIC));
    talk_link_note(ID_NANA, s.conv.avatar, band, song);
    n = talk_quick_replies(&s, 0, band, TC_NONE, 33u, g);
    CHECK(n >= 3);
    if (n >= 3) {
        CHECK_EQ(g[n - 1].act, TA_CHANGE_TOPIC);
        CHECK_EQ(g[n - 1].topic, song);
        CHECK_EQ(g[n - 1].to, 0);
    }
    s.topic_stack[1] = song;
    n = talk_quick_replies(&s, 0, band, TC_NONE, 33u, g);
    CHECK(!has_act(g, n, TA_CHANGE_TOPIC));
    s.topic_stack[1] = TC_NONE;
    s.conv.mem_topic[0] = song;
    put_line(&s, 0, -1, TA_TELL, band, TLF_SINCERE);
    n = talk_quick_replies(&s, 0, band, TC_NONE, 33u, g);
    for (i = 0; i < n; i++) {
        CHECK(g[i].act != TA_CHANGE_TOPIC);
        CHECK(g[i].act == TA_COMFORT || g[i].act == TA_SYMPATHIZE ||
              g[i].act == TA_OFFER || g[i].act == TA_ENCOURAGE);
    }
    talk_db_close();
}

/* Existing needs and mood, not an unrelated random word, change how the
 * player can engage a resident. Every choice still runs through the DM. */
TEST(talk_quick_reads_life_needs_as_conversation_context)
{
    TalkState s;
    TalkSugg g[TALK_QUICK_MAX];
    DoomoResident *r;
    int n;
    premade_town();
    CHECK(open_db());
    r = town_res(ID_NANA);
    set_persona(ID_NANA, 8, 6, 6, 6, 6);
    begin2(&s, ID_NANA, -1, 39u);
    put_line(&s, 0, -1, TA_GREET, TC_NONE, 0);
    r->mood = MOOD_HUNGRY;
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 3u, g);
    CHECK(n >= 2);
    CHECK_EQ(g[1].act, TA_OFFER);
    r->mood = MOOD_SICK;
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 3u, g);
    CHECK(n >= 2);
    CHECK_EQ(g[1].act, TA_OFFER);
    r->mood = MOOD_SCARED;
    n = talk_quick_replies(&s, 0, TC_NONE, TC_NONE, 3u, g);
    CHECK(n >= 2);
    CHECK_EQ(g[1].act, TA_COMFORT);
    talk_db_close();
}

/* every reply, in every situation, goes through the drama manager into a real answer */
TEST(talk_quick_replies_all_get_answers)
{
    TalkState s, t;
    TalkSugg g[TALK_QUICK_MAX];
    uint32_t h0 = talk_safe_lint_hits();
    int i, j, n, la, rooms, said = 0;
    static const int says[] = { TA_GREET, TA_INTRODUCE, TA_ASK_ABOUT, TA_ASK_OPINION, TA_ASK_FEELING, TA_ASK_FACT, TA_ASK_ADVICE,
                                TA_TELL, TA_OPINE_POS, TA_GOSSIP, TA_COMPLIMENT, TA_THANK, TA_COMPLAIN, TA_INSULT, TA_TEASE,
                                TA_CHANGE_TOPIC, TA_BACKCHANNEL, TA_JOKE };
    premade_town();
    CHECK(open_db());
    for (rooms = 0; rooms < 3; rooms++) {
        int ids[3] = { ID_NANA, rooms == 1 ? ID_PIP : ID_RIVET, 729 };
        talk_dm_begin(&s, DOOMO_PLAYER_ID, ids, rooms == 2 ? 3 : 2, 500u + (uint32_t)rooms);
        for (la = 0; la < (int)(sizeof says / sizeof says[0]); la++) {
            put_line(&s, 0, -1, says[la], la & 1 ? concept("pie") : TC_NONE, 0);
            for (i = 0; i < 12; i++) {
                n = talk_quick_replies(&s, 0, TC_NONE, concept("dancing"), (uint32_t)(i + la), g);
                for (j = 0; j < n; j++) {
                    int m;
                    t = s;
                    m = say(&t, &g[j], 0);
                    CHECK_MSG(m >= 1 || (t.flags & TDF_OVER), "no answer to \"%s\" after act %d", g[j].text, says[la]);
                    for (m = 0; m < t.nlines; m++) CHECK(t.line[m].text[0]);
                    said++;
                }
            }
        }
    }
    printf("      %d quick replies said and answered\n", said);
    CHECK(said > 500);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}
