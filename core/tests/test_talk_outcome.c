/* test_talk_outcome.c - T6-OUT: a conversation's outcomes into the sim
 * (core/src/talk_outcome.c). Owner: TK. docs/TALK.md 6g; design 5.5.
 *
 * Hand-made summaries for the single rules (the daily caps of the bond, the
 * level, the mood, the pair; one problem a resident, one make-up a pair, one
 * headline; no news for minors; romance only ever asked), then real sessions:
 * 100 sessions by a greedy player stay under the caps, the talk memory is
 * written back (topics, last-time flags, refused today, and nobody is locked
 * out by an ordinary end), and random sessions with every act (romance
 * included) leave nothing the romance sweep would remove (P7).
 */
#include "test.h"
#include <stdlib.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/roster.h"
#include "doomo/sim.h"
#include "doomo/town.h"
#include "doomo/rng.h"

#define ID_NANA  1
#define ID_RIVET 13
#define ID_PIP   42           /* kid cast */
#define ID_TEEN  11
#define ID_GRUMB 729
#define HERO     DOOMO_PLAYER_ID

static int open_db(void) { return talk_db_open(lang_find("en")) == TALK_DB_OK && talk_dm_ready(); }

static void premade_town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    talk_outcome_reset();
    talk_mem_reset();
    sim_problem_for(-1);                                             /* the sim is up before a test raises a problem */
}

static void summary(TalkSummary *s, int a, int b, int c)
{
    memset(s, 0, sizeof *s);
    s->avatar = HERO;
    s->id[s->n++] = (int16_t)a;
    if (b >= 0) s->id[s->n++] = (int16_t)b;
    if (c >= 0) s->id[s->n++] = (int16_t)c;
    s->turns = 12;
}

static void add_ev(TalkSummary *s, int kind, int a, int b)
{
    TalkEvent *e = &s->ev[s->nev++];
    e->kind = (uint8_t)kind;
    e->a = (uint8_t)a;
    e->b = (uint8_t)b;
}

void sim_problem_remove(int index);                                  /* sim_int.h: the test clears the new town's own problems */
/* raise a problem for id, first clearing whatever the new town gave them (one problem a resident) */
static int raise_fresh(int id, int kind, int item)
{
    int i;
    memset(doomo_town.items, 0, sizeof doomo_town.items);
    while ((i = sim_problem_for(id)) >= 0) sim_problem_remove(i);
    return sim_problem_raise(id, kind, 0, item);
}

static void next_day(void) { doomo_town.day++; }

static void make_rivals(int a, int b)
{
    rel_get_or_add(a, b);
    rel_adjust(a, b, -80);
    rel_set_kind(a, b, REL_RIVAL);
    rel_adjust(a, b, 0);
}

static int last_news_tmpl(void)
{
    const DoomoNewsRec *r = sim_news_record(0);
    return r ? r->tmpl : -1;
}

/* ---- the bond, the level and the mood: the daily caps ---------------------------------------- */
TEST(talk_outcome_bond_caps)
{
    TalkSummary s;
    TalkOutcome o;
    int r0, lv0, i;
    premade_town();
    r0 = rel_value(ID_NANA, HERO);
    lv0 = town_res(ID_NANA)->level;
    summary(&s, ID_NANA, -1, -1);
    s.rel_d_player[0] = 9;
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.rel[0], 9);
    CHECK_EQ(rel_value(ID_NANA, HERO), r0 + 9);
    CHECK_EQ(town_res(ID_NANA)->level, lv0 + 3);                     /* level +3 a day at most */
    talk_outcome_apply(&s, &o);                                      /* the same again: only 3 are left of the 12 */
    CHECK_EQ(o.rel[0], 3);
    CHECK_EQ(rel_value(ID_NANA, HERO), r0 + TALK_CAP_REL_DAY);
    CHECK_EQ(o.level, 0);
    for (i = 0; i < 20; i++) talk_outcome_apply(&s, &o);
    CHECK_EQ(rel_value(ID_NANA, HERO), r0 + TALK_CAP_REL_DAY);
    CHECK_EQ(town_res(ID_NANA)->level, lv0 + TALK_CAP_LEVEL_DAY);
    s.rel_d_player[0] = -12;                                         /* a bad day: the net of the day stays within +-12 */
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.rel[0], -12);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.rel[0], -12);
    CHECK_EQ(rel_value(ID_NANA, HERO), r0 - TALK_CAP_REL_DAY);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.rel[0], 0);
    next_day();                                                      /* a new town day: the caps are new */
    s.rel_d_player[0] = 12;
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.rel[0], 12);
    CHECK_EQ(rel_value(ID_NANA, HERO), r0);
    CHECK_EQ(town_res(ID_NANA)->level, lv0 + 2 * TALK_CAP_LEVEL_DAY);
}

TEST(talk_outcome_mood_and_social)
{
    TalkSummary s;
    TalkOutcome o;
    int h0, so0, so1;
    premade_town();
    h0 = town_res(ID_NANA)->stat[STAT_HAPPY] = 40;
    so0 = town_res(ID_NANA)->stat[STAT_SOCIAL] = 10;
    summary(&s, ID_NANA, ID_RIVET, -1);
    s.mood_d[0] = 8;
    s.engage[0] = 100;                                               /* +4 +8 = 12, the daily cap */
    s.engage[1] = 0;                                                 /* +4 */
    s.pflags[1] = TPF_LEFT;                                          /* walked out: no social gain */
    talk_outcome_apply(&s, &o);
    CHECK_EQ(town_res(ID_NANA)->stat[STAT_HAPPY], h0 + 8);
    so1 = town_res(ID_NANA)->stat[STAT_SOCIAL];
    CHECK_EQ(so1, so0 + 12);
    talk_outcome_apply(&s, &o);                                      /* again: social is spent, the mood has 4 left */
    CHECK_EQ(town_res(ID_NANA)->stat[STAT_SOCIAL], so1);
    CHECK_EQ(town_res(ID_NANA)->stat[STAT_HAPPY], h0 + TALK_CAP_MOOD_DAY);
    CHECK_EQ(o.social, 0);
}

TEST(talk_outcome_leaver_social)
{
    TalkSummary s;
    TalkOutcome o;
    int so0;
    premade_town();
    so0 = town_res(ID_RIVET)->stat[STAT_SOCIAL] = 20;
    summary(&s, ID_RIVET, -1, -1);
    s.pflags[0] = TPF_LEFT;
    s.engage[0] = 90;
    talk_outcome_apply(&s, &o);
    CHECK_EQ(town_res(ID_RIVET)->stat[STAT_SOCIAL], so0);
    CHECK_EQ(o.social, 0);
}

/* ---- resident-resident bonds ---------------------------------------------------------------- */
TEST(talk_outcome_pair_cap)
{
    TalkSummary s;
    TalkOutcome o;
    int v0;
    premade_town();
    rel_get_or_add(ID_NANA, ID_RIVET);
    v0 = rel_value(ID_NANA, ID_RIVET);
    summary(&s, ID_NANA, ID_RIVET, -1);
    s.pair_d[0] = 6;
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.pair[0], 6);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.pair[0], 2);                                          /* 6 + 2 = the 8 of the day */
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.pair[0], 0);
    CHECK_EQ(rel_value(ID_NANA, ID_RIVET), v0 + TALK_CAP_PAIR_DAY);
    next_day();
    talk_outcome_apply(&s, &o);
    CHECK_EQ(rel_value(ID_NANA, ID_RIVET), v0 + TALK_CAP_PAIR_DAY + 6);
}

/* ---- mediation: sim_make_up once a pair a day, one headline ---------------------------------- */
TEST(talk_outcome_mediation_once)
{
    TalkSummary s;
    TalkOutcome o;
    int v0, v1;
    premade_town();
    make_rivals(ID_NANA, ID_RIVET);
    v0 = rel_value(ID_NANA, ID_RIVET);
    summary(&s, ID_NANA, ID_RIVET, -1);
    add_ev(&s, TEV_MEDIATED, 0, 1);
    add_ev(&s, TEV_WALKOUT, 1, -1);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.made_up, 1);
    v1 = rel_value(ID_NANA, ID_RIVET);
    CHECK(v1 > v0 + 10);                                             /* the sim's own make-up */
    CHECK_EQ(o.news, NT_TALK_MADE_UP);                               /* peace wins the one headline */
    CHECK_EQ(last_news_tmpl(), NT_TALK_MADE_UP);
    rel_adjust(ID_NANA, ID_RIVET, -30);
    v0 = rel_value(ID_NANA, ID_RIVET);
    talk_outcome_apply(&s, &o);                                      /* again the same day: no second make-up */
    CHECK_EQ(o.made_up, 0);
    CHECK_EQ(rel_value(ID_NANA, ID_RIVET), v0);
    next_day();
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.made_up, 1);
    CHECK(rel_value(ID_NANA, ID_RIVET) > v0);
}

TEST(talk_outcome_unknown_pair_not_marked)
{
    TalkSummary s;
    TalkOutcome o;
    premade_town();
    summary(&s, ID_NANA, ID_GRUMB, -1);                              /* strangers: sim_make_up says "we don't even know each other" */
    add_ev(&s, TEV_MEDIATED, 0, 1);
    if (rel_kind(ID_NANA, ID_GRUMB) >= 0) return;
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.made_up, 0);
    CHECK_EQ(o.news, 0);
}

/* ---- the news: one a conversation, kid-safe ---------------------------------------------------- */
TEST(talk_outcome_news_rules)
{
    TalkSummary s;
    TalkOutcome o;
    int n0;
    premade_town();
    n0 = sim_news_count();
    summary(&s, ID_NANA, ID_RIVET, -1);
    add_ev(&s, TEV_ULTIMATUM, 0, 1);
    add_ev(&s, TEV_WALKOUT, 1, -1);
    add_ev(&s, TEV_GREAT_NIGHT, -1, -1);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.news, NT_TALK_ROW);                                   /* the row first */
    CHECK_EQ(sim_news_count(), n0 + 1);                              /* and one headline in all */
    CHECK_EQ(last_news_tmpl(), NT_TALK_ROW);
    {   /* the row also cost the pair some bond */
        CHECK(o.pair[0] < 0);
    }
    /* a walk-out alone, a great night alone */
    summary(&s, ID_NANA, ID_RIVET, -1);
    add_ev(&s, TEV_WALKOUT, 1, -1);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.news, NT_TALK_WALKOUT);
    summary(&s, ID_NANA, ID_RIVET, -1);
    add_ev(&s, TEV_GREAT_NIGHT, -1, -1);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.news, NT_TALK_GREAT_NIGHT);
    /* a quiet one: nothing */
    summary(&s, ID_NANA, ID_RIVET, -1);
    n0 = sim_news_count();
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.news, 0);
    CHECK_EQ(sim_news_count(), n0);
}

TEST(talk_outcome_no_news_for_minors)
{
    TalkSummary s;
    TalkOutcome o;
    int n0, kids[2] = { ID_PIP, ID_TEEN };
    premade_town();
    CHECK(talk_safe_minor(ID_PIP) && talk_safe_minor(ID_TEEN));
    n0 = sim_news_count();
    summary(&s, kids[0], kids[1], -1);                               /* a row and peace between two minors */
    add_ev(&s, TEV_ULTIMATUM, 0, 1);
    add_ev(&s, TEV_MEDIATED, 0, 1);
    add_ev(&s, TEV_GREAT_NIGHT, -1, -1);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.news, 0);
    CHECK_EQ(sim_news_count(), n0);
    summary(&s, ID_NANA, ID_PIP, -1);                                /* a row with a child in it: no headline either */
    add_ev(&s, TEV_ULTIMATUM, 0, 1);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.news, 0);
    summary(&s, ID_PIP, -1, -1);                                     /* a child walking out */
    add_ev(&s, TEV_WALKOUT, 0, -1);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.news, 0);
    CHECK_EQ(sim_news_count(), n0);
}

/* ---- problems ------------------------------------------------------------------------------ */
TEST(talk_outcome_problem_once_a_day)
{
    TalkSummary s;
    TalkOutcome o;
    premade_town();
    CHECK(raise_fresh(ID_NANA, PROB_BORED, 0) >= 0);
    summary(&s, ID_NANA, -1, -1);
    s.pflags[0] = TPF_PROBLEM;
    add_ev(&s, TEV_PROBLEM, 0, -1);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.problems, 1);
    CHECK_EQ(sim_problem_for(ID_NANA), -1);
    CHECK(raise_fresh(ID_NANA, PROB_BORED, 0) >= 0);       /* a new one the same day: Talk has done its one */
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.problems, 0);
    CHECK(sim_problem_for(ID_NANA) >= 0);
    next_day();
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.problems, 1);
    CHECK_EQ(sim_problem_for(ID_NANA), -1);
    talk_outcome_apply(&s, &o);                                      /* and nothing to resolve: a no-op */
    CHECK_EQ(o.problems, 0);
}

TEST(talk_outcome_problem_needing_an_item_stays_open)
{
    TalkSummary s;
    TalkOutcome o;
    premade_town();
    CHECK(raise_fresh(ID_NANA, PROB_WANTS_ITEM, 1) >= 0);              /* the player has none of it */
    summary(&s, ID_NANA, -1, -1);
    s.pflags[0] = TPF_PROBLEM;
    add_ev(&s, TEV_PROBLEM, 0, -1);
    talk_outcome_apply(&s, &o);                                      /* the sim keeps it open without the item */
    CHECK_EQ(o.problems, 0);
    CHECK(sim_problem_for(ID_NANA) >= 0);
}

/* ---- romance: only ever asked, never forced -------------------------------------------------- */
TEST(talk_outcome_romance_asked_not_forced)
{
    TalkSummary s;
    TalkOutcome o;
    int k0;
    premade_town();
    /* the hero as avatar: the sim has no romance edge for the hero, so nothing is asked and nothing is set */
    summary(&s, ID_NANA, -1, -1);
    add_ev(&s, TEV_ROMANCE_YES, 0, TA_CONFESS);
    k0 = rel_kind(ID_NANA, HERO);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.confessed, 0);
    CHECK_EQ(rel_kind(ID_NANA, HERO), k0);
    CHECK_EQ(rel_has_crush(ID_NANA, HERO), 0);
    CHECK_EQ(rel_has_crush(HERO, ID_NANA), 0);
    /* a flirt is not a confession */
    s.avatar = ID_RIVET;
    s.nev = 0;
    add_ev(&s, TEV_ROMANCE_YES, 0, TA_FLIRT);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.confessed, 0);
    /* a resident avatar confessing: sim_confess is asked, once a pair a day, and the sweep finds nothing to undo */
    s.nev = 0;
    add_ev(&s, TEV_ROMANCE_YES, 0, TA_CONFESS);
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.confessed, 1);
    CHECK(o.dating == 0 || rel_kind(ID_RIVET, ID_NANA) == REL_DATING);   /* only the sim decides, and it is consistent */
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.confessed, 0);                                        /* not again today */
    CHECK_EQ(sim_romance_sweep(-1), 0);
    /* a child: the sim's gate says no, Talk cannot get around it */
    summary(&s, ID_PIP, -1, -1);
    s.avatar = ID_RIVET;
    add_ev(&s, TEV_ROMANCE_YES, 0, TA_ASK_OUT);                      /* (the gate would never let this event happen; the sim is the second wall) */
    talk_outcome_apply(&s, &o);
    CHECK_EQ(o.dating, 0);
    CHECK(rel_kind(ID_RIVET, ID_PIP) != REL_DATING && rel_kind(ID_RIVET, ID_PIP) != REL_MARRIED);
    CHECK_EQ(sim_romance_sweep(-1), 0);
}

/* ---- real sessions ----------------------------------------------------------------------------- */
static void utter(TalkUtter *u, int act, uint16_t topic, int target)
{
    memset(u, 0, sizeof *u);
    u->act[0] = (uint8_t)act;
    u->topic[0] = topic;
    u->target = (int8_t)target;
    strcpy(u->shown, "zorpy blorp");
}

static uint16_t concept(const char *key)
{
    uint16_t c = 0, wf = 0;
    uint32_t tags[4];
    if (talk_lex_find(key, &c, &wf, tags) < 0) return 0;
    return c;
}

static void play(TalkState *s, int act, uint16_t topic, int target)
{
    TalkUtter u;
    utter(&u, act, topic, target);
    talk_dm_say(s, &u);
    while (talk_dm_step(s, NULL)) {}
}

/* the greedy player: every warm act on a liked topic, 14 turns, and a mediation / praise on top */
TEST(talk_outcome_no_farming_100_sessions)
{
    static const uint8_t greedy[] = { TA_COMPLIMENT, TA_PRAISE_OTHER, TA_COMFORT, TA_ENCOURAGE, TA_OFFER, TA_JOKE, TA_MEDIATE, TA_TAKE_SIDE };
    static const int ids[3] = { ID_NANA, ID_RIVET, ID_GRUMB };
    TalkState s;
    TalkOutcome o;
    int base_rel[3], base_lv[3], base_soc[3], base_hap[3], base_pair[3];
    int i, t, k, sum_rel_d = 0, made = 0, news = 0, probs = 0;
    uint16_t pie;
    premade_town();
    CHECK(open_db());
    pie = concept("pie");
    make_rivals(ID_NANA, ID_RIVET);
    for (k = 0; k < 3; k++) {
        DoomoResident *r = town_res(ids[k]);
        r->stat[STAT_SOCIAL] = 0;
        r->stat[STAT_HAPPY] = 50;
        base_rel[k] = rel_value(ids[k], HERO);
        base_lv[k] = r->level;
        base_soc[k] = r->stat[STAT_SOCIAL];
        base_hap[k] = r->stat[STAT_HAPPY];
    }
    base_pair[0] = rel_value(ID_NANA, ID_RIVET);
    base_pair[1] = rel_value(ID_NANA, ID_GRUMB);
    base_pair[2] = rel_value(ID_RIVET, ID_GRUMB);
    for (i = 0; i < 100; i++) {
        TalkSummary sum;
        talk_outcome_begin(&s, HERO, ids, 3, 9000u + (uint32_t)i);
        for (t = 0; t < 14 && !talk_dm_over(&s); t++)
            play(&s, greedy[(t + i) % (int)sizeof greedy], t % 2 ? pie : TC_NONE, (t + i) % 3);
        talk_outcome_end(&s, &o, &sum);
        CHECK(talk_dm_over(&s));
        for (k = 0; k < 3; k++) sum_rel_d += sum.rel_d_player[k];
        made += o.made_up;
        if (o.news) news++;
        probs += o.problems;
        CHECK(o.news == 0 || (o.news >= NT_TALK_ROW && o.news <= NT_TALK_WALKOUT));
    }
    printf("      farming: 100 greedy sessions: summed bond change asked %d, make-ups %d, headlines %d (cap %d a day)\n", sum_rel_d, made, news, TALK_CAP_NEWS_DAY);
    CHECK(sum_rel_d > 3 * TALK_CAP_REL_DAY);                         /* the test is meaningful: the sessions asked for much more than the cap */
    for (k = 0; k < 3; k++) {
        DoomoResident *r = town_res(ids[k]);
        int dr = rel_value(ids[k], HERO) - base_rel[k];
        CHECK_MSG(dr <= TALK_CAP_REL_DAY && dr >= -TALK_CAP_REL_DAY, "bond of %d moved %d", ids[k], dr);
        CHECK(r->level - base_lv[k] <= TALK_CAP_LEVEL_DAY + made);   /* Talk's +3; sim_make_up adds its own +1 to one of a pair */
        CHECK(r->stat[STAT_SOCIAL] - base_soc[k] <= TALK_CAP_SOCIAL_DAY);
        CHECK(r->stat[STAT_HAPPY] - base_hap[k] <= TALK_CAP_MOOD_DAY + 16);   /* sim_make_up gives each of a pair +8, once a pair a day */
        CHECK(r->stat[STAT_HAPPY] - base_hap[k] >= -TALK_CAP_MOOD_DAY);
    }
    {   /* a pair: the +-8 cap, plus one make-up (the sim's own 28 + compat * 3, at most 46) */
        const int MAKEUP_MAX = 46;
        int dp[3];
        dp[0] = rel_value(ID_NANA, ID_RIVET) - base_pair[0];
        dp[1] = rel_value(ID_NANA, ID_GRUMB) - base_pair[1];
        dp[2] = rel_value(ID_RIVET, ID_GRUMB) - base_pair[2];
        CHECK(dp[1] <= TALK_CAP_PAIR_DAY + MAKEUP_MAX && dp[1] >= -TALK_CAP_PAIR_DAY);
        CHECK(dp[2] <= TALK_CAP_PAIR_DAY + MAKEUP_MAX && dp[2] >= -TALK_CAP_PAIR_DAY);
        CHECK(dp[0] <= TALK_CAP_PAIR_DAY + MAKEUP_MAX && dp[0] >= -TALK_CAP_PAIR_DAY);
        CHECK(made <= 3);                                            /* one make-up a pair a day: 3 pairs */
    }
    CHECK(probs <= 3);
    CHECK(news <= TALK_CAP_NEWS_DAY);
    CHECK_EQ(sim_romance_sweep(-1), 0);
    /* the next day the caps are new: the same player can gain again */
    next_day();
    {
        int before = rel_value(ID_GRUMB, HERO), gained = 0;
        for (i = 0; i < 20 && gained <= 0; i++) {
            talk_outcome_begin(&s, HERO, ids, 3, 777u + (uint32_t)i);
            for (t = 0; t < 10 && !talk_dm_over(&s); t++) play(&s, TA_COMPLIMENT, pie, 2);
            talk_outcome_end(&s, &o, NULL);
            gained = rel_value(ID_GRUMB, HERO) - before;
        }
        CHECK(gained > 0 && gained <= TALK_CAP_REL_DAY);
    }
    talk_db_close();
}

/* the talk memory is written back, and an ordinary end locks nobody out */
TEST(talk_outcome_memory_written_back)
{
    static const int ids[2] = { ID_NANA, ID_RIVET };
    TalkState s;
    TalkOutcome o;
    const TalkMem *m;
    uint16_t pie;
    int t;
    premade_town();
    CHECK(open_db());
    pie = concept("pie");
    talk_outcome_begin(&s, HERO, ids, 2, 4242u);
    for (t = 0; t < 6 && !talk_dm_over(&s); t++) play(&s, t % 2 ? TA_COMPLIMENT : TA_ASK_ABOUT, pie, 0);
    CHECK(s.mem_earned & TMF_COMPLIMENTED);
    talk_outcome_end(&s, &o, NULL);
    m = talk_mem_find(ID_NANA);
    CHECK(m != NULL);
    if (m) {
        CHECK_EQ(m->topic_a, pie);
        CHECK(m->flags & TMF_COMPLIMENTED);
        CHECK(!(m->flags & TMF_REFUSED_TODAY));                      /* talk_dm_end no longer marks every talk as a K-7 */
    }
    CHECK(!talk_mem_refused_today(ID_NANA));
    CHECK(!talk_mem_refused_today(ID_RIVET));
    /* the next talk sees it ("last time you were nice") and begins with the remembered topic */
    talk_outcome_begin(&s, HERO, ids, 2, 4243u);
    CHECK(s.conv.mem_flags[0] & TMF_COMPLIMENTED);
    CHECK_EQ(s.conv.mem_topic[0], pie);
    CHECK(!(s.part[0].flags & TPF_REFUSED));
    talk_db_close();
}

TEST(talk_outcome_refused_today_is_remembered)
{
    static const int ids[1] = { ID_NANA };
    TalkState s;
    TalkOutcome o;
    int t, refused = 0;
    premade_town();
    CHECK(open_db());
    for (t = 0; t < 200 && !refused; t++) {                          /* find a seed where the target says no */
        talk_outcome_begin(&s, ID_RIVET, ids, 1, 100u + (uint32_t)t);
        play(&s, TA_ASK_OUT, TC_NONE, 0);
        refused = (s.conv.refused & 1) != 0;
        talk_outcome_end(&s, &o, NULL);
        if (!refused) talk_mem_reset();
    }
    if (refused) {
        CHECK(talk_mem_refused_today(ID_NANA));
        talk_outcome_begin(&s, ID_RIVET, ids, 1, 5u);                /* a new talk the same day: still refused (K-5) */
        CHECK(s.part[0].flags & TPF_REFUSED);
        talk_outcome_end(&s, &o, NULL);
        next_day();
        talk_outcome_begin(&s, ID_RIVET, ids, 1, 6u);                /* the next day: forgotten */
        CHECK(!(s.part[0].flags & TPF_REFUSED));
    }
    talk_db_close();
}

/* P7: random sessions of every act (romance included), a resident or the hero as avatar, days passing:
 * the sim holds nothing the romance sweep would remove, and no cap is ever broken */
TEST(talk_outcome_p7_random_sessions)
{
    static const int pool[] = { ID_NANA, ID_RIVET, ID_PIP, ID_TEEN, ID_GRUMB, 2, 3, 4, 5, 6, 7, 8 };
    DoomoRng r;
    TalkState s;
    TalkOutcome o;
    int i, n_pool = (int)(sizeof pool / sizeof pool[0]), yes = 0, asked = 0, dating = 0;
    premade_town();
    CHECK(open_db());
    rng_seed(&r, 31337u);
    for (i = 0; i < 1500; i++) {
        int ids[3], n = 1 + rng_range(&r, 3), k, t, avatar = rng_range(&r, 3) ? HERO : pool[rng_range(&r, n_pool)];
        if (i % 50 == 49) next_day();
        for (k = 0; k < n; k++) {
            int c, tries = 0, dup;
            do { c = pool[rng_range(&r, n_pool)]; dup = 0; for (t = 0; t < k; t++) if (ids[t] == c) dup = 1; if (c == avatar) dup = 1; } while (dup && ++tries < 30);
            ids[k] = c;
        }
        for (k = 0; k < n; k++) for (t = 0; t < k; t++) if (ids[t] == ids[k]) n = k;   /* no duplicate (a failed draw) */
        if (n < 1) continue;
        talk_outcome_begin(&s, avatar, ids, n, 20000u + (uint32_t)i);
        for (t = 0; t < 10 && !talk_dm_over(&s); t++) {
            int act = 1 + rng_range(&r, TA_COUNT - 1);
            if (rng_range(&r, 4) == 0) act = TA_FLIRT + rng_range(&r, 4);   /* the romance acts often */
            play(&s, act, TC_NONE, rng_range(&r, n + 1) - 1);
        }
        talk_outcome_end(&s, &o, NULL);
        for (k = 0; k < s.nev; k++) if (s.ev[k].kind == TEV_ROMANCE_YES) yes++;
        asked += o.confessed;
        dating += o.dating;
        CHECK(o.news == 0 || o.news >= NT_TALK_ROW);
    }
    printf("      P7: 1500 sessions, %d romance yes, %d asked of the sim, %d now dating (the sim decided)\n", yes, asked, dating);
    CHECK_EQ(sim_romance_sweep(-1), 0);
    talk_db_close();
}
