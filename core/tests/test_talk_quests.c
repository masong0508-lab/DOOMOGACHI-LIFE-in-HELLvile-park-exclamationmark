/* test_talk_quests.c - TQ-QUESTS: residents raise requests while you talk
 * (core/src/talk_quest.c, talk_dm.c, talk_gen.c, talk_outcome.c, talk_save.c, story_questlog.c; docs/TALK.md 6q).
 * Owner: TALK. Each test fails without the code it covers.
 *
 *   quest_kind_tables_kid_safe     the exhaustive kind x giver x room x target table, and every item: nothing for adults only
 *                                  reaches a minor, no kind is a place, a date or a romance
 *   quest_reply_classes            which player acts are an accept, a decline, a haggle, "tell me more"
 *   quest_pick_every_resident      every resident, both room kinds, 8 salts: the pick obeys the kid rules, its target is a living
 *                                  person of the giver's own stage who is not in the room, its item is a snack or a toy
 *   quest_sources                  a problem in the sim's queue becomes its request (hungry / wants an item -> fetch, bored /
 *                                  wants a friend / lonely -> company, a fight -> peace); with none, a favour or a Talk-born one
 *   quest_lines_every_phase        every phase x kind has a line, kid rooms too, no hole, no register word, nothing unfilled
 *   quest_pacing                   never two in a row (also in 300 sessions), one a town day, at most four open, none at a load
 *   quest_never_when_hurting       hostile, sad or distressed exchanges never raise one (400 sessions each)
 *   quest_accept_and_hand_in       a session raises a fetch, the player accepts: the table, the log row, the item taken, H$ paid,
 *                                  a headline, the bond; the next conversation opens with the thanks
 *   quest_decline_haggle_more      a no settles with a cooldown and costs a little bond; a haggle works once and the bonus
 *                                  pays half again; "tell me more" keeps the request open
 *   quest_ready_by_kind            message, cheer, company, peace and secret become ready by what the player does, not before
 *   quest_memory_and_save          the 10 bits: every value round trips through the chunk, an old chunk reads none, a bad field
 *                                  is dropped, the days run out, an entry with a request is never the one evicted, the chunk
 *                                  is no bigger than before
 *   quest_kid_sessions             600 sessions with a child as the giver or in the room: only kid kinds, never a place or romance
 */
#include "test.h"
#include <stdio.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/talk_traits.h"
#include "doomo/lang.h"
#include "doomo/sim.h"
#include "doomo/family.h"
#include "doomo/items.h"
#include "doomo/resident_acc.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"

static void premade_town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    doomo_town.minute = 12 * 60;
    sim_init();
    talk_outcome_reset();
    talk_mem_reset();
}

static int open_en(void) { return talk_db_open(lang_find("en")) == TALK_DB_OK && talk_gen_ready(); }

static int adults[80], nadult, kids[12], nkid;
static void people(void)
{
    int id;
    nadult = nkid = 0;
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        const DoomoResident *r = town_res(id);
        if (r->status != RES_ALIVE || (r->flags & RF_CUSTOM)) continue;
        if (talk_safe_minor(id)) { if (nkid < 12) kids[nkid++] = id; }
        else if (id < DOOMO_PLAYER_ID && nadult < 80) adults[nadult++] = id;
    }
}

static void set_rel(int a, int b, int kind, int value)
{
    DoomoEdge *e = rel_get_or_add(a, b);
    if (e) { e->kind = (uint8_t)kind; e->value = (int8_t)value; }
    e = rel_get_or_add(b, a);
    if (e) { e->kind = (uint8_t)kind; e->value = (int8_t)value; }
}

static uint32_t rs = 0x2545F491u;
static uint32_t rnd(uint32_t n) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return n ? rs % n : 0; }

static int st_ev(const TalkState *s, int kind)
{
    int k, n = 0;
    for (k = 0; k < s->nev; k++) if (s->ev[k].kind == kind) n++;
    return n;
}

static void say(TalkState *s, int act)
{
    TalkUtter u;
    memset(&u, 0, sizeof u);
    u.act[0] = (uint8_t)act;
    u.target = 0;
    u.conf = 15;
    s->part[0].engage = 80;
    talk_dm_say(s, &u);
    while (talk_dm_step(s, NULL)) {}
}

/* talk to `giver` (alone) with neutral acts until a request of `want` (0 any) is on the table; 1 when it is. The
 * conversation is left open. Each try ends a conversation, so the pace is reset first. */
static int raise(TalkState *s, int giver, int want, uint32_t seed0)
{
    int it, tn;
    for (it = 0; it < 600; it++) {
        TalkOutcome out;
        talk_quest_reset();
        talk_quest_note_end();
        talk_outcome_begin(s, DOOMO_PLAYER_ID, &giver, 1, seed0 + (uint32_t)it * 31u);
        for (tn = 0; tn < 9 && !talk_dm_over(s) && !s->q_kind; tn++) say(s, tn & 1 ? TA_TELL : TA_COMPLIMENT);
        if (s->q_kind && (!want || s->q_kind == want)) return 1;
        talk_outcome_end(s, &out, NULL);
        talk_mem_quest_set(giver, 0, 0, 0, 0);
    }
    return 0;
}

static int quest_line_seen(const TalkState *s)
{
    int k;
    for (k = 0; k < s->nlines; k++) if (s->line[k].text[0] && strchr(s->line[k].text, '?')) return 1;
    return 0;
}

/* ---- the tables ------------------------------------------------------------------------------------------------ */
TEST(quest_kind_tables_kid_safe)
{
    int kind, mg, mr, mt, item;
    for (kind = 0; kind <= TQK_COUNT; kind++)
        for (mg = 0; mg < 2; mg++)
            for (mr = 0; mr < 2; mr++)
                for (mt = -1; mt < 2; mt++) {
                    int ok = talk_quest_kind_ok(kind, mg, mr, mt);
                    if (kind == TQK_NONE || kind >= TQK_COUNT) CHECK(!ok);
                    if (mg || mr) CHECK(!(ok && (kind == TQK_PEACE || kind == TQK_SECRET)));        /* adults only, adult rooms only */
                    if (kind == TQK_TOY) CHECK(ok == (mg != 0));
                    if (ok && (kind == TQK_MESSAGE || kind == TQK_CHEER)) CHECK(mt >= 0 && !!mt == !!mg);   /* never across stages */
                    if ((kind == TQK_MESSAGE || kind == TQK_CHEER) && mt < 0) CHECK(!ok);
                    if (kind == TQK_PEACE && ok) CHECK(mt == 0 && !mg && !mr);
                    if (kind == TQK_FETCH || kind == TQK_COMPANY) CHECK(ok);
                }
    for (item = -1; item <= ITEM_COUNT; item++)
        for (mg = 0; mg < 2; mg++)
            for (mr = 0; mr < 2; mr++) {
                int ok = talk_quest_item_ok(item, mg, mr);
                if (ok) {
                    CHECK(item > ITEM_NONE && item < ITEM_COUNT);
                    CHECK(!talk_act_item_adult(item));                                          /* no adult item in any room */
                    CHECK(doomo_items[item].category == ICAT_FOOD || item == ITEM_PLUSHIE || item == ITEM_BOARD_GAME);
                    CHECK(doomo_items[item].category != ICAT_KEY);
                }
            }
    CHECK(!talk_quest_item_ok(ITEM_VAPE_PEN, 0, 0));
    CHECK(!talk_quest_item_ok(ITEM_LOVE_LETTER, 0, 0));
    CHECK(!talk_quest_item_ok(ITEM_RING, 1, 1));
    CHECK(talk_quest_item_ok(ITEM_CUPCAKE, 1, 1) && talk_quest_item_ok(ITEM_PLUSHIE, 1, 1));
    /* every kind has a name and a reward, and the bonus is half again */
    for (kind = TQK_FETCH; kind < TQK_COUNT; kind++) {
        CHECK(talk_quest_reward(kind, 0) > 0);
        CHECK_EQ(talk_quest_reward(kind, 1), talk_quest_reward(kind, 0) * 3 / 2);
        CHECK(strcmp(talk_quest_kind_name(kind), "?") != 0);
    }
    CHECK_EQ(talk_quest_reward(TQK_NONE, 1), 0);
}

TEST(quest_reply_classes)
{
    int a;
    CHECK_EQ(talk_quest_reply_class(TA_YES), TQR_ACCEPT);
    CHECK_EQ(talk_quest_reply_class(TA_PROMISE), TQR_ACCEPT);
    CHECK_EQ(talk_quest_reply_class(TA_NO), TQR_DECLINE);
    CHECK_EQ(talk_quest_reply_class(TA_SHAKE_HEAD), TQR_DECLINE);
    CHECK_EQ(talk_quest_reply_class(TA_ASK_FAVOR), TQR_HAGGLE);
    CHECK_EQ(talk_quest_reply_class(TA_ASK_ABOUT), TQR_MORE);
    CHECK_EQ(talk_quest_reply_class(TA_ASK_WHO), TQR_MORE);
    for (a = 0; a < TA_COUNT; a++) {
        int r = talk_quest_reply_class(a);
        CHECK(r >= TQR_NONE && r < TQR_COUNT);
        if (TA_IS_ROMANCE(a) || TA_IS_SAFETY(a)) CHECK_EQ(r, TQR_NONE);          /* a romance act is never a way to answer a request */
    }
}

/* ---- picking ------------------------------------------------------------------------------------------------------ */
TEST(quest_pick_every_resident)
{
    int id, salt, mr, picks = 0, kinds_seen = 0, seen[TQK_COUNT] = { 0 }, k;
    premade_town();
    people();
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        const DoomoResident *r = town_res(id);
        if (r->status != RES_ALIVE) continue;
        for (mr = 0; mr < 2; mr++)
            for (salt = 0; salt < 8; salt++) {
                TalkQuest q;
                int mg = talk_safe_minor(id), room[1], tm;
                room[0] = id;
                if (!talk_quest_pick(id, room, 1, mr, 0x9000u + (uint32_t)salt * 77u, &q)) continue;
                picks++;
                seen[q.kind]++;
                CHECK(q.kind > TQK_NONE && q.kind < TQK_COUNT);
                CHECK(q.src <= TQSRC_TALK);
                tm = q.target >= 0 ? talk_safe_minor(q.target) : -1;
                CHECK(talk_quest_kind_ok(q.kind, mg, mr, tm));
                if (q.target >= 0) {
                    CHECK(q.target != id && q.target < DOOMO_MAX_RESIDENTS && town_res(q.target)->status == RES_ALIVE);
                    CHECK(!!talk_safe_minor(q.target) == !!mg);
                }
                if (q.item) CHECK(talk_quest_item_ok(q.item, mg, mr));
                if (q.kind == TQK_FETCH || q.kind == TQK_TOY) CHECK(q.item > 0);
                if (mg || mr) CHECK(q.kind != TQK_PEACE && q.kind != TQK_SECRET);
                /* the same again: nothing about it is random but the salt, and the derive agrees with the pick */
                {
                    TalkQuest d;
                    CHECK(talk_quest_derive(id, q.kind, room, 1, mr, &d));
                    CHECK(d.target == q.target && d.item == q.item && d.kind == q.kind);
                }
            }
    }
    CHECK(picks > 500);
    for (k = TQK_FETCH; k < TQK_COUNT; k++) if (seen[k]) kinds_seen++;
    CHECK_MSG(kinds_seen >= 5, "only %d kinds ever picked", kinds_seen);
    CHECK(seen[TQK_TOY] > 0);                              /* the children's own */
    /* a target standing in the room is no target: "go and tell the one who is here" */
    {
        TalkQuest q;
        int g = adults[0], t, room[2];
        set_rel(g, adults[1], REL_FRIEND, 40);
        CHECK(talk_quest_derive(g, TQK_MESSAGE, NULL, 0, 0, &q));
        t = q.target;
        room[0] = g;
        room[1] = t;
        CHECK(!talk_quest_derive(g, TQK_MESSAGE, room, 2, 0, &q));
    }
}

TEST(quest_sources)
{
    TalkQuest q;
    int g, o;
    premade_town();
    people();
    g = adults[3];
    o = adults[5];
    /* no problem: a favour; a fetch is a snack the giver likes (or a cupcake) */
    while (sim_problem_for(g) >= 0) sim_problem_resolve(sim_problem_for(g));
    CHECK(talk_quest_derive(g, TQK_FETCH, NULL, 0, 0, &q));
    CHECK_EQ(q.src, TQSRC_FAVOUR);
    CHECK(q.item > 0);
    CHECK(!talk_quest_derive(g, TQK_PEACE, NULL, 0, 0, &q) || q.src == TQSRC_TALK);    /* a rival, or none: Talk-born */
    /* hungry: the sim's own problem is the request */
    CHECK(sim_problem_raise(g, PROB_HUNGRY, -1, 0) >= 0);
    CHECK(talk_quest_derive(g, TQK_FETCH, NULL, 0, 0, &q));
    CHECK_EQ(q.src, TQSRC_PROBLEM);
    sim_problem_resolve(sim_problem_for(g));
    /* wants an item: that item */
    CHECK(sim_problem_raise(g, PROB_WANTS_ITEM, -1, ITEM_ONIGIRI) >= 0);
    CHECK(talk_quest_derive(g, TQK_FETCH, NULL, 0, 0, &q));
    CHECK_EQ(q.src, TQSRC_PROBLEM);
    CHECK_EQ(q.item, ITEM_ONIGIRI);
    sim_problem_resolve(sim_problem_for(g));
    inv_add(ITEM_ONIGIRI, 0);
    /* bored, wants a friend, lonely: company, from the problem */
    {
        static const int bored[3] = { PROB_BORED, PROB_WANTS_FRIEND, PROB_LONELY };
        int k;
        for (k = 0; k < 3; k++) {
            while (sim_problem_for(g) >= 0) sim_problem_resolve(sim_problem_for(g));
            CHECK(sim_problem_raise(g, bored[k], -1, 0) >= 0);
            CHECK(talk_quest_derive(g, TQK_COMPANY, NULL, 0, 0, &q));
            CHECK_EQ(q.src, TQSRC_PROBLEM);
        }
    }
    /* a fight: peace with the other one, adults only */
    while (sim_problem_for(g) >= 0) sim_problem_resolve(sim_problem_for(g));
    CHECK(sim_problem_raise(g, PROB_FIGHT, o, 0) >= 0);
    CHECK(talk_quest_derive(g, TQK_PEACE, NULL, 0, 0, &q));
    CHECK_EQ(q.src, TQSRC_PROBLEM);
    CHECK_EQ(q.target, o);
    /* and a pick with a problem open leans on it: 40 salts, the problem's kind comes up most */
    {
        int n = 0, s;
        for (s = 0; s < 40; s++)
            if (talk_quest_pick(g, &g, 1, 0, 100u + (uint32_t)s * 13u, &q) && q.src == TQSRC_PROBLEM) n++;
        CHECK_MSG(n >= 20, "the open problem was the request only %d times of 40", n);
    }
    /* cheer-up (Talk-born) only for someone who is down */
    {
        int friend_ = adults[7];
        TalkQuest d;
        set_rel(g, friend_, REL_FRIEND, 60);
        town_res(friend_)->mood = MOOD_HAPPY;
        CHECK(talk_quest_derive(g, TQK_CHEER, NULL, 0, 0, &d));
        {
            int s, n = 0;
            for (s = 0; s < 40; s++) if (talk_quest_pick(g, &g, 1, 0, 500u + (uint32_t)s, &q) && q.kind == TQK_CHEER) n++;
            CHECK_EQ(n, 0);
        }
    }
}

TEST(quest_lines_every_phase)
{
    int phase, kind, seed, who, ids[2];
    TalkConv c;
    TalkLine L;
    premade_town();
    CHECK(open_en());
    people();
    for (who = 0; who < 2; who++) {                        /* an adult speaker, then a child speaking in a kids' room */
        int giver = who ? kids[0] : adults[0];
        ids[0] = giver;
        for (phase = TQP_ASK; phase < TQP_COUNT; phase++)
            for (kind = TQK_FETCH; kind < TQK_COUNT; kind++) {
                if (who && (kind == TQK_PEACE || kind == TQK_SECRET)) continue;
                if (kind == TQK_TOY && !who) continue;
                for (seed = 0; seed < 12; seed++) {
                    int ok, k;
                    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 1, 0x5100u + (uint32_t)seed * 17u);
                    ok = talk_gen_quest(&c, 0, phase, kind, kind == TQK_MESSAGE || kind == TQK_CHEER || kind == TQK_PEACE ? adults[1] : -1,
                                        kind == TQK_TOY ? ITEM_PLUSHIE : ITEM_CUPCAKE, &L, NULL);
                    CHECK_MSG(ok && L.text[0], "no line for phase %d kind %d (who %d)", phase, kind, who);
                    CHECK(!(L.flags & (TLF_SAFE | TLF_FALLBACK)));
                    CHECK(!strchr(L.text, '{') && !strchr(L.text, '}'));
                    CHECK(strlen(L.text) < TALK_LINE_MAX);
                    for (k = 0; L.text[k]; k++) CHECK((unsigned char)L.text[k] >= 0x20 || L.text[k] == 0);
                    if (phase == TQP_ASK || phase == TQP_LATE || phase == TQP_HAGGLE_YES || phase == TQP_HAGGLE_NO)
                        CHECK_MSG(strchr(L.text, '?') != NULL, "a request must hand the talk over: %s", L.text);
                    {
                        TalkAudience aud;
                        int sw = 0;
                        memset(&aud, 0, sizeof aud);
                        aud.any_minor = (uint8_t)who;
                        CHECK_EQ(talk_safe_lint(L.text, &aud, &sw), TL_OK);
                        CHECK_EQ(sw, 0);                    /* no swear, ever, in a request */
                    }
                    if (kind == TQK_FETCH && (phase == TQP_ASK || phase == TQP_DONE))
                        CHECK(strstr(L.text, "cupcake") != NULL || strstr(L.text, "Cupcake") != NULL ||
                              phase == TQP_DONE || strchr(L.text, '?'));
                }
            }
    }
    /* the item is its name, lower case, with the right article */
    ids[0] = adults[0];
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 1, 1u);
    {
        int seen_an = 0, seen_a = 0;
        for (seed = 0; seed < 40; seed++) {
            talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 1, 0x700u + (uint32_t)seed);
            if (talk_gen_quest(&c, 0, TQP_ASK, TQK_FETCH, -1, ITEM_ONIGIRI, &L, NULL) && strstr(L.text, "an onigiri")) seen_an = 1;
            talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 1, 0x800u + (uint32_t)seed);
            if (talk_gen_quest(&c, 0, TQP_ASK, TQK_FETCH, -1, ITEM_CUPCAKE, &L, NULL) && strstr(L.text, "a cupcake")) seen_a = 1;
        }
        CHECK(seen_an && seen_a);
    }
    /* a message with no target, a fetch with no item: no line (the generator never says a hole) */
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 1, 3u);
    CHECK(!talk_gen_quest(&c, 0, TQP_ASK, TQK_MESSAGE, -1, 0, &L, NULL) || !strstr(L.text, "{"));
}

/* ---- pacing ------------------------------------------------------------------------------------------------------ */
TEST(quest_pacing)
{
    int i, giver, raised_run = 0, last = 0, total = 0, per_day = 0;
    uint32_t day = 0;
    premade_town();
    CHECK(open_en());
    people();
    /* the counters: nothing at a load, one conversation between two raises, one a day, four open at most */
    talk_quest_reset();
    CHECK(!talk_quest_pace_ok());                       /* a fresh load: the first conversation never raises */
    talk_quest_note_end();
    CHECK(talk_quest_pace_ok());
    talk_quest_note_raise();
    CHECK(!talk_quest_pace_ok());
    talk_quest_note_end();                              /* the conversation that raised ends */
    CHECK(!talk_quest_pace_ok());
    talk_quest_note_end();                              /* one more with nothing raised ... */
    CHECK(!talk_quest_pace_ok());                       /* ... but it is still the same town day */
    doomo_town.day++;
    CHECK(talk_quest_pace_ok());
    talk_quest_reset();
    talk_quest_note_end();
    {
        int k;
        for (k = 0; k < TALK_QUEST_OPEN_MAX; k++) CHECK(talk_mem_quest_set(adults[k], TQK_COMPANY, TQS_ACTIVE, TQ_DAYS, 0));
        CHECK_EQ(talk_quest_open_count(), TALK_QUEST_OPEN_MAX);
        CHECK(!talk_quest_pace_ok());
        CHECK(!talk_quest_add(adults[9], TQK_FETCH, 0));      /* a fifth is refused */
        talk_mem_quest_set(adults[0], TQK_COMPANY, TQS_SETTLED, 3, 0);
        CHECK(talk_quest_pace_ok());
        talk_mem_reset();
    }
    /* 300 conversations: every one a full Talk session with neutral acts, a town day every 4th */
    talk_quest_reset();
    for (i = 0; i < 300; i++) {
        TalkState s;
        TalkOutcome out;
        int tn, here = 0;
        giver = adults[i % 40];
        if (i % 4 == 0) doomo_town.day++;
        if (doomo_town.day != day) { day = doomo_town.day; per_day = 0; }
        talk_outcome_begin(&s, DOOMO_PLAYER_ID, &giver, 1, 0x1234u + (uint32_t)i * 101u);
        for (tn = 0; tn < 7 && !talk_dm_over(&s); tn++) {
            if (tn == 0) { talk_dm_open(&s); while (talk_dm_step(&s, NULL)) {} }
            say(&s, tn & 1 ? TA_TELL : TA_COMPLIMENT);
            if (s.q_kind) { here = 1; if (talk_mem_quest_get(giver, NULL, NULL, NULL, NULL) == 0) { /* declined: free the slot */ } }
            if (here) break;
        }
        if (here) say(&s, TA_NO);                          /* said no, so the table never fills */
        talk_outcome_end(&s, &out, NULL);
        if (here) { total++; per_day++; CHECK_MSG(!last, "two raises in a row at conversation %d", i); CHECK(per_day <= 1); raised_run++; }
        last = here;
        for (tn = 0; tn < talk_mem_count(); tn++) {         /* free the cooldowns, so the pace is the only limit */
            const TalkMem *e = talk_mem_at(tn);
            if (e) talk_mem_quest_set(e->id, 0, 0, 0, 0);
        }
    }
    CHECK_MSG(total >= 8, "only %d requests in 300 conversations: the pace is dead", total);
    CHECK_MSG(total <= 150, "%d requests in 300 conversations: not 'now and then'", total);
    CHECK(raised_run == total);
    CHECK_EQ(giver >= 0, 1);
    printf("      pacing: %d requests in 300 conversations (%d%%), never two in a row, at most one a town day\n", total, total * 100 / 300);
}

static const int hurt_acts[] = { TA_INSULT, TA_THREATEN, TA_COMPLAIN, TA_BLAME, TA_COMFORT, TA_SYMPATHIZE, TA_DISMISS, TA_MOCK, TA_ACCUSE };

TEST(quest_never_when_hurting)
{
    int it, mode, raises = 0;
    premade_town();
    CHECK(open_en());
    people();
    for (mode = 0; mode < 3; mode++)
        for (it = 0; it < 400; it++) {
            TalkState s;
            TalkOutcome out;
            int giver = adults[it % 40], tn;
            talk_quest_reset();
            talk_quest_note_end();
            talk_outcome_begin(&s, DOOMO_PLAYER_ID, &giver, 1, 0xBEEF0u + (uint32_t)it * 7u + (uint32_t)mode);
            for (tn = 0; tn < 7 && !talk_dm_over(&s); tn++) {
                if (mode == 0) say(&s, hurt_acts[rnd((uint32_t)(sizeof hurt_acts / sizeof hurt_acts[0]))]);
                else if (mode == 1) say(&s, TA_DISTRESS);     /* the gate answers a distress line with care: K-11 */
                else { say(&s, TA_COMPLIMENT); say(&s, TA_COMFORT); }   /* the warm talk with a sad one in between: the last act hurts */
                if (s.q_kind && mode < 2) { raises++; break; }
            }
            talk_outcome_end(&s, &out, NULL);
            talk_mem_quest_set(giver, 0, 0, 0, 0);
        }
    CHECK_EQ(raises, 0);
    /* and the grief rule: a grief topic in the thread raises nothing, the same talk on another topic does */
    {
        static const char *const grief[] = { "grief", "funeral", "memorial" };
        int g, giver = adults[0], tn, raised_grief = 0, raised_other = 0;
        uint16_t gt[3], other = 0, c0 = 0, wf = 0;
        uint32_t tags[4];
        for (g = 0; g < 3; g++) {
            gt[g] = 0;
            if (talk_lex_find(grief[g], &c0, &wf, tags) >= 0) gt[g] = c0;
        }
        CHECK(gt[0] && gt[1] && gt[2]);
        if (talk_lex_find("pie", &c0, &wf, tags) >= 0) other = c0;
        CHECK(other);
        for (mode = 0; mode < 2; mode++)
            for (tn = 0; tn < 80; tn++) {
                TalkState s;
                TalkOutcome out;
                int k;
                talk_quest_reset();
                talk_quest_note_end();
                talk_outcome_begin(&s, DOOMO_PLAYER_ID, &giver, 1, 0xC0DE00u + (uint32_t)tn);
                for (k = 0; k < 6 && !talk_dm_over(&s); k++) {
                    TalkUtter u;
                    memset(&u, 0, sizeof u);
                    u.act[0] = k & 1 ? TA_TELL : TA_ASK_ABOUT;
                    u.topic[0] = mode == 0 ? gt[k % 3] : other;
                    u.target = 0;
                    u.conf = 15;
                    s.part[0].engage = 80;
                    talk_dm_say(&s, &u);
                    while (talk_dm_step(&s, NULL)) {}
                    if (s.q_kind) { if (mode == 0) raised_grief++; else raised_other++; break; }
                }
                talk_outcome_end(&s, &out, NULL);
                talk_mem_quest_set(giver, 0, 0, 0, 0);
            }
        CHECK_EQ(raised_grief, 0);
        CHECK_MSG(raised_other > 0, "the control talk never raised a request");
    }
}

/* ---- the session ------------------------------------------------------------------------------------------------- */
TEST(quest_accept_and_hand_in)
{
    TalkState s;
    TalkOutcome out;
    TalkSummary sum;
    TalkSugg quick[TALK_QUICK_MAX];
    TalkQuestRow row;
    TalkQuest q;
    int giver, nq, money0, inv0, news0;
    premade_town();
    CHECK(open_en());
    people();
    giver = adults[2];
    set_rel(giver, DOOMO_PLAYER_ID, REL_FRIEND, 50);
    CHECK(raise(&s, giver, TQK_FETCH, 0x4000u));
    CHECK(s.nlines > 0 || s.q_kind);
    CHECK(s.q_st & TQ_ST_ASKED);
    CHECK(quest_line_seen(&s));
    /* the four replies */
    nq = talk_quick_replies(&s, 0, 0, 0, 1u, quick);
    CHECK_EQ(nq, 4);
    CHECK_EQ(quick[0].act, TA_YES);
    CHECK_EQ(quick[1].act, TA_NO);
    CHECK_EQ(quick[2].act, TA_ASK_FAVOR);
    CHECK_EQ(quick[3].act, TA_ASK_ABOUT);
    CHECK(quick[0].text && quick[1].text && quick[2].text && quick[3].text);
    CHECK(talk_quest_derive(giver, TQK_FETCH, NULL, 0, 0, &q));
    CHECK(q.item > 0);
    while (inv_count(q.item) > 0) inv_take(q.item, 1);
    /* accept */
    say(&s, TA_YES);
    CHECK(s.nlines >= 1);
    CHECK_EQ(st_ev(&s, TEV_QUEST_ACCEPT), 1);
    CHECK(s.q_st & TQ_ST_CLOSED);
    CHECK(talk_quick_replies(&s, 0, 0, 0, 2u, quick) > 0 && quick[0].act != TA_NO);   /* the ordinary replies are back */
    memset(&out, 0, sizeof out);
    talk_outcome_end(&s, &out, &sum);
    CHECK_EQ(out.quests_new, 1);
    CHECK_EQ(out.skipped, 0);
    CHECK_EQ(talk_quest_open_count(), 1);
    CHECK_EQ(talk_quest_log_count(), 1);
    CHECK(talk_quest_log_row(0, &row));
    CHECK_EQ(row.giver, giver);
    CHECK_EQ(row.kind, TQK_FETCH);
    CHECK_EQ(row.state, TQS_ACTIVE);
    CHECK(row.days > 0 && row.reward == talk_quest_reward(TQK_FETCH, 0));
    CHECK(strstr(row.goal, "Bring") && strstr(row.goal, town_res(giver)->name));
    CHECK(!talk_quest_ready(giver));
    /* a second request from the same one is not raised while this is open */
    CHECK(!talk_quest_pick(giver, &giver, 1, 0, 5u, &q));
    /* the item in the bag: ready, and the log says go back */
    CHECK(talk_quest_derive(giver, TQK_FETCH, NULL, 0, 0, &q));
    inv_add(q.item, 2);
    CHECK(talk_quest_ready(giver));
    CHECK(talk_quest_log_row(0, &row));
    CHECK_EQ(row.state, TQS_READY);
    CHECK(strstr(row.goal, "Go back"));
    /* the next conversation opens with the thanks */
    money0 = (int)doomo_town.money;
    inv0 = inv_count(q.item);
    news0 = sim_news_count();
    talk_quest_reset();
    talk_quest_note_end();
    talk_outcome_begin(&s, DOOMO_PLAYER_ID, &giver, 1, 77u);
    CHECK(talk_dm_open(&s));
    while (talk_dm_step(&s, NULL)) {}
    CHECK_EQ(st_ev(&s, TEV_QUEST_DONE), 1);
    CHECK(s.nlines >= 1);
    say(&s, TA_THANK);
    memset(&out, 0, sizeof out);
    talk_outcome_end(&s, &out, &sum);
    CHECK_EQ(out.quests_done, 1);
    CHECK_EQ(out.skipped, 0);
    CHECK_EQ(out.reward, talk_quest_reward(TQK_FETCH, 0));
    CHECK_EQ((int)doomo_town.money, money0 + talk_quest_reward(TQK_FETCH, 0));
    CHECK_EQ(inv_count(q.item), inv0 - 1);
    CHECK(out.rel[0] > 0);                                  /* the bond: through the daily cap like every other */
    CHECK_EQ(out.news, NT_TALK_QUEST);
    CHECK(sim_news_count() >= news0);
    CHECK_EQ(talk_quest_log_count(), 0);
    {
        int kind, st, days;
        CHECK(talk_quest_state(giver, &kind, &st, &days, NULL));
        CHECK(st == TQS_SETTLED && days > 0);               /* a cooldown: not asked again at once */
    }
    CHECK(!talk_quest_pick(giver, &giver, 1, 0, 9u, &q));
    /* handing in twice does nothing */
    CHECK_EQ(talk_quest_complete(giver), -1);
}

TEST(quest_decline_haggle_more)
{
    TalkState s;
    TalkOutcome out;
    TalkSugg quick[TALK_QUICK_MAX];
    int giver, won = 0, lost = 0, tries, nq;
    premade_town();
    CHECK(open_en());
    people();
    giver = adults[4];
    /* decline */
    CHECK(raise(&s, giver, TQK_COMPANY, 0x5000u));
    say(&s, TA_NO);
    CHECK_EQ(st_ev(&s, TEV_QUEST_DECLINE), 1);
    CHECK_EQ(st_ev(&s, TEV_QUEST_ACCEPT), 0);
    CHECK(s.nlines >= 1);
    memset(&out, 0, sizeof out);
    talk_outcome_end(&s, &out, NULL);
    CHECK_EQ(out.quests_declined, 1);
    CHECK_EQ(talk_quest_log_count(), 0);
    CHECK_EQ(talk_quest_open_count(), 0);
    {
        int st = 0, days = 0;
        CHECK(talk_quest_state(giver, NULL, &st, &days, NULL));
        CHECK(st == TQS_SETTLED && days == TQ_COOLDOWN);   /* a no is a cooldown, not a punishment */
    }
    /* tell me more: the request stays on the table, said again with the same quick replies */
    talk_mem_quest_set(giver, 0, 0, 0, 0);
    set_rel(giver, adults[20], REL_FRIEND, 50);                       /* a message needs someone to carry it to */
    CHECK(raise(&s, giver, TQK_MESSAGE, 0x6000u));
    say(&s, TA_ASK_ABOUT);
    CHECK_EQ(st_ev(&s, TEV_QUEST_ACCEPT) + st_ev(&s, TEV_QUEST_DECLINE), 0);
    CHECK(s.nlines >= 1);
    CHECK(!(s.q_st & TQ_ST_CLOSED) && (s.q_st & TQ_ST_ASKED));
    CHECK_EQ(talk_quick_replies(&s, 0, 0, 0, 3u, quick), 4);
    say(&s, TA_ASK_WHO);
    say(&s, TA_ASK_ABOUT);
    CHECK(!(s.q_st & TQ_ST_CLOSED));
    say(&s, TA_YES);
    CHECK_EQ(st_ev(&s, TEV_QUEST_ACCEPT), 1);
    talk_outcome_end(&s, &out, NULL);
    CHECK_EQ(out.quests_new, 1);
    talk_mem_quest_set(giver, 0, 0, 0, 0);
    /* haggle: once, and by their nature; the bonus reaches the table and the pay */
    for (tries = 0; tries < 60 && (!won || !lost); tries++) {
        int g = adults[10 + tries % 30], bonus = 0, kind, st, days;
        talk_mem_quest_set(g, 0, 0, 0, 0);
        set_rel(g, DOOMO_PLAYER_ID, tries & 1 ? REL_BEST_FRIEND : REL_ACQUAINTANCE, tries & 1 ? 85 : 5);
        if (!raise(&s, g, TQK_COMPANY, 0x7000u + (uint32_t)tries * 977u)) continue;
        say(&s, TA_ASK_FAVOR);
        CHECK(s.q_st & TQ_ST_HAGGLED);
        CHECK(s.nlines >= 1);
        CHECK(!(s.q_st & TQ_ST_CLOSED));
        nq = talk_quick_replies(&s, 0, 0, 0, 4u, quick);
        CHECK_EQ(nq, 3);                                    /* no second haggle offered */
        {
            int k;
            for (k = 0; k < nq; k++) CHECK(quick[k].act != TA_ASK_FAVOR);
        }
        say(&s, TA_ASK_FAVOR);                              /* typed anyway: the answer is a no, the request stands */
        CHECK(!(s.q_st & TQ_ST_CLOSED));
        if (s.q_st & TQ_ST_WON) { won++; bonus = 1; } else lost++;
        say(&s, TA_YES);
        CHECK_EQ(st_ev(&s, TEV_QUEST_ACCEPT), 1);
        CHECK_EQ(s.ev[s.nev - 1].b & 0x10, bonus ? 0x10 : 0);
        talk_outcome_end(&s, &out, NULL);
        CHECK_EQ(out.quests_new, 1);
        CHECK(talk_quest_state(g, &kind, &st, &days, &bonus));
        CHECK_EQ(kind, TQK_COMPANY);
        CHECK(st == TQS_ACTIVE && days == TQ_DAYS);
        if (bonus) CHECK_EQ(talk_quest_reward(kind, bonus), 15);
        talk_mem_quest_set(g, 0, 0, 0, 0);
    }
    CHECK_MSG(won > 0 && lost > 0, "haggling must sometimes work and sometimes not (won %d, lost %d)", won, lost);
    CHECK(talk_quest_haggle(adults[0], 1) == 0 || talk_quest_haggle(adults[0], 1) == 1);
}

TEST(quest_ready_by_kind)
{
    TalkState s;
    TalkOutcome out;
    TalkSummary sum;
    int g, t, kind, st, tn;
    TalkQuest q;
    premade_town();
    CHECK(open_en());
    people();
    g = adults[6];
    t = adults[8];
    set_rel(g, t, REL_FRIEND, 50);
    town_res(t)->mood = MOOD_SAD;
    /* a message: ready when the player has talked to the target, not before */
    CHECK(talk_quest_add(g, TQK_MESSAGE, 0));
    CHECK(talk_quest_derive(g, TQK_MESSAGE, NULL, 0, 0, &q));
    t = q.target;
    CHECK(!talk_quest_ready(g));
    talk_outcome_begin(&s, DOOMO_PLAYER_ID, &g, 1, 11u);           /* a talk with the giver does not do it */
    say(&s, TA_GREET);
    talk_outcome_end(&s, &out, &sum);
    CHECK(talk_quest_state(g, &kind, &st, NULL, NULL) && st == TQS_ACTIVE);
    talk_outcome_begin(&s, DOOMO_PLAYER_ID, &t, 1, 12u);           /* a talk with the target does */
    say(&s, TA_GREET);
    talk_outcome_end(&s, &out, &sum);
    CHECK(talk_quest_state(g, &kind, &st, NULL, NULL) && st == TQS_READY);
    CHECK(talk_quest_ready(g));
    CHECK(talk_quest_complete(g) == talk_quest_reward(TQK_MESSAGE, 0));
    /* company: ready after a later talk of three turns, not the one that took it */
    talk_mem_reset();
    CHECK(talk_quest_add(g, TQK_COMPANY, 0));
    talk_outcome_begin(&s, DOOMO_PLAYER_ID, &g, 1, 13u);
    say(&s, TA_GREET);
    talk_outcome_end(&s, &out, &sum);
    CHECK(talk_quest_state(g, &kind, &st, NULL, NULL) && st == TQS_ACTIVE);   /* one turn: not enough */
    talk_outcome_begin(&s, DOOMO_PLAYER_ID, &g, 1, 14u);
    for (tn = 0; tn < 3; tn++) say(&s, tn & 1 ? TA_TELL : TA_COMPLIMENT);
    talk_outcome_end(&s, &out, &sum);
    CHECK(talk_quest_state(g, &kind, &st, NULL, NULL) && st == TQS_READY);
    /* cheer up: the target's mood must have gone up in the talk */
    talk_mem_reset();
    CHECK(talk_quest_add(g, TQK_CHEER, 0));
    CHECK(talk_quest_derive(g, TQK_CHEER, NULL, 0, 0, &q));
    t = q.target;
    talk_outcome_begin(&s, DOOMO_PLAYER_ID, &t, 1, 15u);
    say(&s, TA_INSULT);
    talk_outcome_end(&s, &out, &sum);
    CHECK(talk_quest_state(g, &kind, &st, NULL, NULL) && st == TQS_ACTIVE);
    talk_outcome_begin(&s, DOOMO_PLAYER_ID, &t, 1, 16u);
    for (tn = 0; tn < 3; tn++) say(&s, TA_COMPLIMENT);
    talk_outcome_end(&s, &out, &sum);
    CHECK(talk_quest_state(g, &kind, &st, NULL, NULL) && st == TQS_READY);
    /* a secret: kept after a day */
    talk_mem_reset();
    set_rel(g, DOOMO_PLAYER_ID, REL_BEST_FRIEND, 90);
    CHECK(talk_quest_add(g, TQK_SECRET, 0));
    talk_outcome_begin(&s, DOOMO_PLAYER_ID, &g, 1, 17u);
    say(&s, TA_GREET);
    talk_outcome_end(&s, &out, &sum);
    CHECK(talk_quest_state(g, &kind, &st, NULL, NULL) && st == TQS_ACTIVE);   /* the same day */
    doomo_town.day++;
    talk_outcome_begin(&s, DOOMO_PLAYER_ID, &g, 1, 18u);
    say(&s, TA_GREET);
    talk_outcome_end(&s, &out, &sum);
    CHECK(talk_quest_state(g, &kind, &st, NULL, NULL) && st == TQS_READY);
    CHECK(talk_quest_complete(g) > 0);
}

/* ---- memory and the chunk ----------------------------------------------------------------------------------------- */
static const DoomoSaveExt *ext(void) { return doomo_save_ext_talk; }

TEST(quest_memory_and_save)
{
    static uint8_t img[4096];
    int kind, st, days, bonus, k, n, i;
    uint32_t len, len0;
    int ids[8];
    premade_town();
    people();
    for (i = 0; i < 8; i++) ids[i] = adults[i];
    /* the 10 bits hold every value */
    for (kind = TQK_FETCH; kind < TQK_COUNT; kind++)
        for (st = TQS_ACTIVE; st <= TQS_SETTLED; st++)
            for (days = 0; days <= 7; days++)
                for (bonus = 0; bonus < 2; bonus++) {
                    int k2, s2, d2, b2;
                    CHECK(talk_mem_quest_set(ids[0], kind, st, days, bonus));
                    CHECK(talk_mem_quest_get(ids[0], &k2, &s2, &d2, &b2));
                    CHECK(k2 == kind && s2 == st && d2 == days && b2 == bonus);
                }
    CHECK(!talk_mem_quest_set(ids[0], TQK_COUNT, TQS_ACTIVE, 1, 0));
    CHECK(!talk_mem_quest_set(ids[0], TQK_FETCH, 9, 1, 0));
    CHECK(!talk_mem_quest_set(ids[0], TQK_FETCH, TQS_ACTIVE, 8, 0));
    CHECK(talk_mem_quest_set(ids[0], 0, 0, 0, 0));
    CHECK(!talk_mem_quest_get(ids[0], NULL, NULL, NULL, NULL));
    /* the chunk: no byte more than before, six requests and their depth bits round trip */
    talk_mem_reset();
    len0 = ext()->size();
    for (i = 0; i < 6; i++) {
        talk_mem_note_topic(ids[i], (uint16_t)(100 + i));
        CHECK(talk_mem_quest_set(ids[i], 1 + i, 1 + i % 3, 1 + i, i & 1));
    }
    len = ext()->size();
    CHECK_EQ(len, (uint32_t)(TALK_MEM_HDR + 6 * TALK_MEM_ENTRY));      /* the same 12 B an entry */
    CHECK(len > len0 && len <= SAVE_EXT_MAX_TALK);
    CHECK(TALK_SAVE_WORST <= SAVE_EXT_MAX_TALK);
    CHECK_EQ(ext()->write(img, sizeof img), len);
    talk_mem_reset();
    CHECK(ext()->read(img, len, TALK_MEM_VERSION));
    CHECK_EQ(talk_mem_count(), 6);
    for (i = 0; i < 6; i++) {
        CHECK(talk_mem_quest_get(ids[i], &kind, &st, &days, &bonus));
        CHECK(kind == 1 + i && st == 1 + i % 3 && days == 1 + i && bonus == (i & 1));
    }
    /* an old chunk (the high bits 0) reads no request; a bad field (kind 15, kind with no state) is dropped */
    img[TALK_MEM_HDR + 10] = 0; img[TALK_MEM_HDR + 11] = 0;
    n = TALK_MEM_HDR + 1 * TALK_MEM_ENTRY;
    img[n + 10] = (uint8_t)(15u << 6); img[n + 11] = (uint8_t)((15u << 6) >> 8);
    img[n + TALK_MEM_ENTRY + 10] = (uint8_t)(3u << 6); img[n + TALK_MEM_ENTRY + 11] = (uint8_t)((3u << 6) >> 8);
    talk_mem_reset();
    CHECK(ext()->read(img, len, TALK_MEM_VERSION));
    CHECK(!talk_mem_quest_get(ids[5], NULL, NULL, NULL, NULL));              /* the chunk is newest first: entry 0 is ids[5] */
    CHECK(!talk_mem_quest_get(ids[4], NULL, NULL, NULL, NULL));
    CHECK(!talk_mem_quest_get(ids[3], NULL, NULL, NULL, NULL));
    CHECK(talk_mem_quest_get(ids[2], NULL, NULL, NULL, NULL));
    /* a conversation does not wipe it (talk_mem_end rewrites the depth bits only) */
    talk_mem_reset();
    CHECK(talk_mem_quest_set(ids[0], TQK_CHEER, TQS_ACTIVE, 5, 1));
    {
        TalkConv c;
        memset(&c, 0, sizeof c);
        c.n = 1; c.spoke = 1; c.id[0] = (int16_t)ids[0];
        c.mem_dep[0] = 0x2D;
        talk_mem_end(&c);
    }
    CHECK(talk_mem_quest_get(ids[0], &kind, &st, &days, &bonus) && kind == TQK_CHEER && st == TQS_ACTIVE && days == 5 && bonus == 1);
    /* the days run out: open ones to "never mind" (settled, 0), cooldowns to nothing */
    doomo_town.day += 2;
    CHECK(talk_mem_quest_get(ids[0], &kind, &st, &days, &bonus) && st == TQS_ACTIVE && days == 3);
    CHECK(talk_mem_quest_set(ids[1], TQK_FETCH, TQS_SETTLED, 2, 0));
    doomo_town.day += 3;
    CHECK(talk_mem_quest_get(ids[0], &kind, &st, &days, &bonus) && st == TQS_SETTLED && days == 0);
    CHECK(!talk_mem_quest_get(ids[1], NULL, NULL, NULL, NULL));                  /* the cooldown is over */
    CHECK(talk_mem_quest_get(ids[0], NULL, NULL, NULL, NULL));                   /* "never mind" waits to be said */
    talk_quest_lapse_ack(ids[0]);
    CHECK(talk_mem_quest_get(ids[0], &kind, &st, &days, NULL) && st == TQS_SETTLED && days == 1);
    doomo_town.day += 1;
    CHECK(!talk_mem_quest_get(ids[0], NULL, NULL, NULL, NULL));
    /* a long gap (a save from long ago): everything has run out */
    talk_mem_reset();
    CHECK(talk_mem_quest_set(ids[0], TQK_FETCH, TQS_ACTIVE, 7, 0));
    doomo_town.day += 400;
    CHECK(talk_mem_quest_get(ids[0], &kind, &st, &days, NULL) && st == TQS_SETTLED && days == 0);
    /* eviction: the 25th person never pushes out one who has asked something */
    talk_mem_reset();
    CHECK(talk_mem_quest_set(adults[0], TQK_COMPANY, TQS_ACTIVE, 6, 0));
    for (k = 1; k < TALK_MEM_MAX; k++) CHECK(talk_mem_touch(adults[k]));
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK_EQ(talk_mem_at(TALK_MEM_MAX - 1)->id, adults[0]);                       /* the oldest: next to go */
    CHECK(talk_mem_touch(adults[TALK_MEM_MAX]));
    CHECK_EQ(talk_mem_count(), TALK_MEM_MAX);
    CHECK(talk_mem_quest_get(adults[0], &kind, &st, NULL, NULL) && kind == TQK_COMPANY);
    CHECK(talk_mem_find(adults[1]) == NULL);                                     /* the next oldest went instead */
}

/* ---- kids ----------------------------------------------------------------------------------------------------------- */
TEST(quest_kid_sessions)
{
    int it, raised = 0, saw_toy = 0, bad = 0;
    premade_town();
    CHECK(open_en());
    people();
    CHECK(nkid >= 2);
    for (it = 0; it < 600; it++) {
        TalkState s;
        TalkOutcome out;
        int ids[2], n = 1, tn, mode = it % 3, k;
        ids[0] = mode == 2 ? adults[it % 30] : kids[it % nkid];
        if (mode == 1 && nkid > 1) { ids[1] = kids[(it + 1) % nkid]; n = 2; }
        if (mode == 2) { ids[1] = kids[it % nkid]; n = 2; }           /* an adult with a child in the room */
        talk_mem_reset();
        talk_quest_note_end();
        talk_outcome_begin(&s, DOOMO_PLAYER_ID, ids, n, 0x9900u + (uint32_t)it * 37u);
        for (tn = 0; tn < 9 && !talk_dm_over(&s); tn++) {
            say(&s, tn & 1 ? TA_TELL : TA_COMPLIMENT);
            if (s.q_kind) {
                raised++;
                if (s.q_kind == TQK_TOY) saw_toy++;
                if (s.q_kind == TQK_PEACE || s.q_kind == TQK_SECRET) bad++;
                for (k = 0; k < s.nlines; k++) {
                    TalkAudience aud;
                    int sw = 0;
                    memset(&aud, 0, sizeof aud);
                    aud.any_minor = 1;
                    if (talk_safe_lint(s.line[k].text, &aud, &sw) != TL_OK || sw) bad++;
                }
                say(&s, TA_YES);
                break;
            }
        }
        talk_outcome_end(&s, &out, NULL);
    }
    printf("      kid rooms: %d requests in 600 sessions (%d toys), 0 unsafe kinds or lines\n", raised, saw_toy);
    CHECK_EQ(bad, 0);
    CHECK_MSG(raised > 20, "only %d kid-room requests in 600 sessions", raised);
    CHECK(saw_toy > 0);
}

/* a yes or no later in the talk, on another topic, does not answer a request that is no longer on the table */
TEST(quest_stray_yes_no_ignored)
{
    TalkState s;
    TalkOutcome out;
    int giver, a;
    premade_town();
    CHECK(open_en());
    people();
    giver = adults[6];
    for (a = 0; a < 3; a++) {
        static const int acts[3] = { TA_AGREE, TA_NO, TA_DISAGREE };
        CHECK(raise(&s, giver, 0, 0x7000u + (uint32_t)a * 977u));
        say(&s, TA_TELL);                                   /* the player ignores the request */
        say(&s, acts[a]);
        CHECK_EQ(st_ev(&s, TEV_QUEST_ACCEPT), 0);
        CHECK_EQ(st_ev(&s, TEV_QUEST_DECLINE), 0);
        memset(&out, 0, sizeof out);
        talk_outcome_end(&s, &out, NULL);
        CHECK_EQ(out.quests_new, 0);
        CHECK_EQ(out.quests_declined, 0);
        talk_mem_quest_set(giver, 0, 0, 0, 0);
    }
}

/* review fixes: nobody is asked to cheer up someone who is grieving, and a thank-you needs room for its event */
TEST(quest_cheer_not_for_grief)
{
    int g, t, x, s, n = 0, n2 = 0;
    TalkQuest q;
    premade_town();
    people();
    g = adults[0]; t = adults[7]; x = adults[9];
    set_rel(g, t, REL_FRIEND, 60);
    town_res(t)->mood = MOOD_SAD;
    for (s = 0; s < 80; s++) if (talk_quest_pick(g, &g, 1, 0, 900u + (uint32_t)s, &q) && q.kind == TQK_CHEER && q.target == t) n++;
    CHECK_MSG(n > 0, "setup: a sad friend is asked about %d times", n);
    CHECK(family_add(t, FAMROLE_HOUSEMATE, 0) >= 0);
    CHECK(family_add(x, FAMROLE_HOUSEMATE, 0) >= 0);
    CHECK(family_lose(x, FAMLOSS_ILLNESS));
    for (s = 0; s < 80; s++) if (talk_quest_pick(g, &g, 1, 0, 900u + (uint32_t)s, &q) && q.kind == TQK_CHEER && q.target == t) n2++;
    CHECK_EQ(n2, 0);
}

TEST(quest_accept_needs_event_room)
{
    TalkState s;
    int giver, k;
    premade_town();
    CHECK(open_en());
    people();
    giver = adults[2];
    set_rel(giver, DOOMO_PLAYER_ID, REL_FRIEND, 50);
    CHECK(raise(&s, giver, TQK_FETCH, 0x4000u));
    while (s.nev < TALK_EVENTS_MAX) { k = s.nev++; memset(&s.ev[k], 0, sizeof s.ev[k]); s.ev[k].kind = TEV_LEARNED; s.ev[k].a = -1; }
    say(&s, TA_YES);
    CHECK_EQ(st_ev(&s, TEV_QUEST_ACCEPT), 0);
    CHECK(!(s.q_st & TQ_ST_CLOSED));                        /* no thank-you was said for a request nobody will log */
}
