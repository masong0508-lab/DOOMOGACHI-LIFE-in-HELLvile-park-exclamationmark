/* test_talk_plan.c - TM2c: the plan (layer a) and the effective voice (layer c)
 * (core/src/talk_plan.c, talk_gen.c; docs/TALK.md 6m; design TALK_MULTILEVEL.md 2.2, 13 TM2c).
 * Owner: TALK. Each test fails without the code it covers.
 *
 *   plan_pure_*                     talk_plan_kind as a pure function: the hard rules, the reason rule, the order
 *   plan_kind_follows_stance        accept / agree with a lean of +1 +2, deny / refuse with -1 -2: 80% or more
 *   plan_trait_bias                 a kind resident accepts, a lazy one refuses, 20 points more than their opposite
 *   plan_relationship               a rival accepts under 15%, a best friend over 70%
 *   plan_sincere_bypass             grief, safety acts, unknown acts, the pet, a gate refusal: no plan
 *   plan_kid_room                   no cool reply act; the edgy traits change nothing (5 traits x 4 stages)
 *   plan_deterministic              same seed same plan and text; another turn another plan
 *   plan_guards_kind_and_trait      kind= and trait= in a grammar; the edgy trait guard is false in a kid room
 *   plan_soft_filter_counts_misses  an empty cell: the line is still made, the miss is counted
 *   plan_effective_voice            the voice guards read the sliders plus the trait deltas
 *   preview_four_lines              the creator's preview takes the plan: a refusal says no
 * The census checks (Q8a, Q8c) run in test_talk_census.c.
 */
#include <stdlib.h>
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/talk_traits.h"
#include "doomo/lang.h"
#include "doomo/sim.h"
#include "doomo/resident_acc.h"

static void premade_town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
}

static int open_en(void) { return talk_db_open(lang_find("en")) == TALK_DB_OK && talk_gen_ready(); }

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

static int adults[40], nadult, kids[8], nkid, teens[8], nteen;
static uint16_t pool[600];
static int npool;

static void people(void)
{
    int id;
    nadult = nkid = nteen = 0;
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        if (town_res(id)->status != RES_ALIVE || (town_res(id)->flags & RF_CUSTOM)) continue;
        if (sim_life_stage(id) == LIFE_TEEN) { if (nteen < 8) teens[nteen++] = id; }
        else if (talk_safe_minor(id)) { if (nkid < 8) kids[nkid++] = id; }
        else if (id < DOOMO_PLAYER_ID && nadult < 40) adults[nadult++] = id;
    }
}

/* talkable concepts that may be said in any room, outside grief */
static void topics(void)
{
    int n = talk_db_nconcepts(), i;
    TalkAudience a;
    memset(&a, 0, sizeof a);
    npool = 0;
    for (i = 1; i <= n && npool < 600; i += 5) {
        TalkConcept tc;
        uint8_t parent = 0, flags = 0;
        if (!talk_db_concept((uint16_t)i, &tc) || !talk_db_cat(tc.cat, &parent, &flags) || !(flags & TCAT_TALKABLE)) continue;
        if ((tc.flags & TCF_GRIEF) || !talk_gen_concept_ok((uint16_t)i, &a)) continue;
        a.any_minor = 1;
        if (!talk_gen_concept_ok((uint16_t)i, &a)) { a.any_minor = 0; continue; }
        a.any_minor = 0;
        pool[npool++] = (uint16_t)i;
    }
}

static uint32_t rs = 0x2545F491u;
static uint32_t rnd(uint32_t n) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return n ? rs % n : 0; }

static void set_rel(int id, int band)
{
    DoomoEdge *e;
    int kind = REL_ACQUAINTANCE, v = 10;
    switch (band) {
    case TRB_ACQUAINTANCE: kind = REL_ACQUAINTANCE; v = 15; break;
    case TRB_FRIEND: kind = REL_FRIEND; v = 50; break;
    case TRB_BEST: kind = REL_BEST_FRIEND; v = 85; break;
    case TRB_RIVAL: kind = REL_RIVAL; v = -50; break;
    default: break;
    }
    e = rel_get_or_add(DOOMO_PLAYER_ID, id);
    if (e) { e->kind = (uint8_t)kind; e->value = (int8_t)v; }
    e = rel_get_or_add(id, DOOMO_PLAYER_ID);
    if (e) { e->kind = (uint8_t)kind; e->value = (int8_t)v; }
}

/* one answer line of speaker ids[0] to the player; `other` (>= 0) joins the room */
static int one(int id, int other, int act, uint16_t topic, uint32_t seed, int turn, TalkLine *l, TalkTrace *tr)
{
    TalkConv c;
    int ids[2], n = 1;
    ids[0] = id;
    if (other >= 0) ids[n++] = other;
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, n, seed);
    c.turn = (uint8_t)turn;
    memset(tr, 0, sizeof *tr);
    return talk_gen_line(&c, 0, -1, act, topic, l, tr);
}

static int is_pos(int k) { return k == TK_ACCEPT || k == TK_AGREE; }
static int is_neg(int k) { return k == TK_DENY || k == TK_REFUSE; }

/* ---- the pure function ------------------------------------------------------------------ */
static void pin(TalkPlanIn *in, int act, int allowed, int dflt)
{
    memset(in, 0, sizeof *in);
    in->act = (uint8_t)act;
    in->allowed = (uint8_t)allowed;
    in->dflt = (uint8_t)dflt;
    in->rel = TRB_ACQUAINTANCE;
    in->mood = TMB_OK;
    in->stage = LIFE_ADULT;
    in->margin = 8;
    memset(in->eff, 8, sizeof in->eff);
}

TEST(plan_pure_hard_rules_and_reason)
{
    TalkPlanIn in;
    TalkPlan p;
    int i, k, seen[TK_COUNT];
    uint8_t all = (uint8_t)(TKM(TK_ACCEPT) | TKM(TK_AGREE) | TKM(TK_DENY) | TKM(TK_REFUSE) | TKM(TK_DEFLECT) |
                            TKM(TK_COUNTER) | TKM(TK_ASKBACK));
    /* nothing allowed: no plan */
    pin(&in, TA_INVITE, 0, 0);
    CHECK_EQ(talk_plan_kind(&in, &p), TK_NONE);
    CHECK_EQ(p.kind, TK_NONE);
    /* a romance act is accepted, deflected or refused: never a counter or an ask back, whatever the dice say */
    pin(&in, TA_FLIRT, all, TKM(TK_ACCEPT));
    for (i = 0; i < 400; i++) {
        in.seed = (uint32_t)i * 2654435761u;
        in.consent = (uint8_t)(i & 1);
        k = talk_plan_kind(&in, &p);
        CHECK_MSG(k == TK_ACCEPT || k == TK_DEFLECT || k == TK_REFUSE, "romance kind %d", k);
        if (in.consent) CHECK_EQ(k, TK_ACCEPT);                      /* the consent decides, as before the plan */
        else CHECK(k != TK_ACCEPT);
    }
    /* a hug among or toward children is accepted or refused, nothing else */
    pin(&in, TA_HUG, all, TKM(TK_ACCEPT));
    in.kid_room = 1;
    for (i = 0; i < 400; i++) {
        in.seed = (uint32_t)i * 40503u;
        in.mood = (uint8_t)(i % TMB_COUNT);
        k = talk_plan_kind(&in, &p);
        CHECK_MSG(k == TK_ACCEPT || k == TK_REFUSE, "hug kind %d", k);
    }
    /* the reason rule: a "no" to what they like needs a reason that is not the dice */
    pin(&in, TA_ASK_OPINION, (uint8_t)(TKM(TK_AGREE) | TKM(TK_DENY) | TKM(TK_COUNTER) | TKM(TK_DEFLECT)), TKM(TK_AGREE));
    in.lean = 2;
    for (i = 0; i < 400; i++) {
        in.seed = (uint32_t)i * 977u;
        k = talk_plan_kind(&in, &p);
        CHECK_MSG(!is_neg(k), "a lean of +2 and the kind %d", k);
    }
    in.lean = 1;
    in.rel = TRB_RIVAL; in.mood = TMB_ANGRY; in.eff[PERS_SPEECH] = 14;    /* a blunt rival in a temper: that is a reason */
    in.deny = 4;
    memset(seen, 0, sizeof seen);
    for (i = 0; i < 400; i++) {
        in.seed = (uint32_t)i * 977u;
        k = talk_plan_kind(&in, &p);
        seen[k]++;
        if (is_neg(k)) CHECK(p.flags & TPL_REASON);
    }
    CHECK_MSG(seen[TK_DENY] > 0, "a blunt angry rival never says no to what they like");
    /* the order: the best kind first, then the next, all allowed, no repeats */
    pin(&in, TA_INVITE, (uint8_t)(TKM(TK_ACCEPT) | TKM(TK_DENY) | TKM(TK_REFUSE) | TKM(TK_DEFLECT)), TKM(TK_ACCEPT));
    for (i = 0; i < 100; i++) {
        in.seed = (uint32_t)i;
        k = talk_plan_kind(&in, &p);
        CHECK(p.allowed & TKM(k));
        CHECK(p.next[0] == 0 || (p.next[0] & 7) != k);
        if (p.next[0]) {
            TalkPlan q = p;
            CHECK(talk_plan_next(&q));
            CHECK(q.kind == (p.next[0] & 7) && (q.allowed & TKM(q.kind)));
        }
    }
    {
        TalkPlan q;
        memset(&q, 0, sizeof q);
        CHECK(!talk_plan_next(&q));
    }
}

/* ---- 1: the stance ------------------------------------------------------------------------ */
TEST(plan_kind_follows_stance)
{
    int i, pos_n = 0, pos_ok = 0, neg_n = 0, neg_ok = 0, open = 0;
    TalkLine l;
    TalkTrace tr;
    premade_town();
    CHECK(open_en());
    people();
    topics();
    CHECK(nadult >= 10 && npool >= 100);
    for (i = 0; i < 5000; i++) {
        int id = adults[rnd((uint32_t)nadult)];
        uint16_t t = pool[rnd((uint32_t)npool)];
        one(id, -1, TA_ASK_OPINION, t, 1000u + (uint32_t)i, i & 15, &l, &tr);
        if (!tr.plan.kind) continue;
        if (tr.plan.flags & TPL_OPEN) open++;
        if (tr.plan.lean >= 1) { pos_n++; pos_ok += is_pos(tr.plan.kind); }
        if (tr.plan.lean <= -1) { neg_n++; neg_ok += is_neg(tr.plan.kind); }
    }
    CHECK_MSG(pos_n >= 150 && neg_n >= 150, "too few opinions with a lean: %d pro, %d con", pos_n, neg_n);
    CHECK_MSG(pos_ok * 100 >= pos_n * 80, "lean +: %d of %d accept or agree", pos_ok, pos_n);
    CHECK_MSG(neg_ok * 100 >= neg_n * 80, "lean -: %d of %d deny or refuse", neg_ok, neg_n);
    printf("      5000 opinions: lean + %d of %d accept/agree, lean - %d of %d deny/refuse, %d open\n", pos_ok, pos_n,
           neg_ok, neg_n, open);
    talk_db_close();
}

/* ---- 2: the traits ------------------------------------------------------------------------- */
/* the share (in %) of answers of kind `want` (a TKM mask) to `act`, speaker `id` with the explicit set `ts` */
static int share(int id, const TalkTraitSet *ts, int act, int want, int n)
{
    int i, hit = 0;
    TalkLine l;
    TalkTrace tr;
    if (talk_trait_set(id, ts) != TTE_OK) return -1;
    for (i = 0; i < n; i++) {
        one(id, -1, act, 0, 5000u + (uint32_t)i * 31u, i & 31, &l, &tr);
        if (!tr.plan.kind) { talk_trait_clear(id); return -1; }
        hit += (want & (int)TKM(tr.plan.kind)) != 0;
    }
    talk_trait_clear(id);
    return hit * 100 / n;
}

TEST(plan_trait_bias)
{
    TalkTraitSet kind, mean, lazy, driven;
    int id, k, m, lz, am;
    premade_town();
    CHECK(open_en());
    people();
    id = adults[0];
    ((DoomoResident *)town_res(id))->mood = MOOD_OK;
    talk_traitset_clear(&kind); talk_traitset_add(&kind, TT_KIND);
    talk_traitset_clear(&mean); talk_traitset_add(&mean, TT_MEAN);
    talk_traitset_clear(&lazy); talk_traitset_add(&lazy, TT_LAZY);
    talk_traitset_clear(&driven); talk_traitset_add(&driven, TT_AMBITIOUS);
    k = share(id, &kind, TA_INVITE, TKM(TK_ACCEPT), 600);
    m = share(id, &mean, TA_INVITE, TKM(TK_ACCEPT), 600);
    lz = share(id, &lazy, TA_INVITE, TKM(TK_DENY) | TKM(TK_REFUSE), 600);
    am = share(id, &driven, TA_INVITE, TKM(TK_DENY) | TKM(TK_REFUSE), 600);
    CHECK_MSG(k >= m + 20, "invitation: kind accepts %d%%, mean %d%%", k, m);
    CHECK_MSG(lz >= am + 20, "activity: lazy refuses %d%%, ambitious %d%%", lz, am);
    printf("      invitation accepted: kind %d%% vs mean %d%%; activity refused: lazy %d%% vs ambitious %d%%\n", k, m, lz, am);
    talk_db_close();
}

/* ---- 3: the relationship -------------------------------------------------------------------- */
TEST(plan_relationship)
{
    int id, rival, best, i;
    TalkTraitSet none;
    TalkLine l;
    TalkTrace tr;
    premade_town();
    CHECK(open_en());
    people();
    id = adults[1];
    ((DoomoResident *)town_res(id))->mood = MOOD_OK;
    talk_traitset_clear(&none);
    set_rel(id, TRB_RIVAL);
    rival = share(id, &none, TA_INVITE, TKM(TK_ACCEPT), 600);
    set_rel(id, TRB_BEST);
    best = share(id, &none, TA_INVITE, TKM(TK_ACCEPT), 600);
    CHECK_MSG(rival < 15, "a rival accepts %d%%", rival);
    CHECK_MSG(best > 70, "a best friend accepts %d%%", best);
    printf("      invitation accepted: rival %d%%, best friend %d%%\n", rival, best);
    /* the same on the produced lines: plan_relationship_line */
    set_rel(id, TRB_RIVAL);
    for (i = 0; i < 20; i++) one(id, -1, TA_INVITE, 0, 77u + (uint32_t)i, i, &l, &tr);
    CHECK(tr.plan.kind != 0);
    talk_db_close();
}

/* the share (in %) of PRODUCED lines (not the recorded plan) whose reply act serves a yes kind; the finding of the TM2c
 * review: a rival said yes to an invitation in 47% of the lines while the plan said deny */
static int line_yes(int id, int act, int n)
{
    int i, hit = 0;
    TalkLine l;
    TalkTrace tr;
    for (i = 0; i < n; i++) {
        one(id, -1, act, 0, 9000u + (uint32_t)i * 17u, i & 31, &l, &tr);
        hit += (talk_gen_kinds_of(l.act) & (int)(TKM(TK_ACCEPT) | TKM(TK_AGREE))) != 0;
    }
    return hit * 100 / n;
}

TEST(plan_relationship_line)
{
    int id, ract, bact, k;
    static const int acts[2] = { TA_INVITE, TA_ASK_FAVOR };
    TalkTraitSet none;
    premade_town();
    CHECK(open_en());
    people();
    id = adults[1];
    ((DoomoResident *)town_res(id))->mood = MOOD_OK;
    talk_traitset_clear(&none);
    talk_trait_set(id, &none);
    for (k = 0; k < 2; k++) {
        set_rel(id, TRB_RIVAL);
        ract = line_yes(id, acts[k], 600);
        set_rel(id, TRB_BEST);
        bact = line_yes(id, acts[k], 600);
        printf("      act %d, lines that say yes: rival %d%%, best friend %d%%\n", acts[k], ract, bact);
        CHECK_MSG(ract < 25, "act %d: a rival says yes in %d%% of the lines", acts[k], ract);
        CHECK_MSG(bact > 65, "act %d: a best friend says yes in %d%% of the lines", acts[k], bact);
    }
    talk_trait_clear(id);
    talk_db_close();
}

/* ---- 4: the sincere lines ------------------------------------------------------------------- */
TEST(plan_sincere_bypass)
{
    int i, id, n = 0;
    TalkLine l;
    TalkTrace tr;
    TalkConv c;
    TalkUtter u;
    TalkExchange x;
    TalkTrace xt[TALK_EXCHANGE_MAX];
    int ids[2];
    premade_town();
    CHECK(open_en());
    people();
    topics();
    id = adults[2];
    /* grief topics: whatever the act */
    for (i = 1; i <= talk_db_nconcepts() && n < 40; i++) {
        TalkConcept tc;
        int act;
        if (!talk_db_concept((uint16_t)i, &tc) || !(tc.flags & TCF_GRIEF)) continue;
        for (act = 1; act < TA_FIRST_SAFETY; act++) {
            one(id, -1, act, (uint16_t)i, 9000u + (uint32_t)act, 3, &l, &tr);
            CHECK_MSG(tr.plan.kind == 0, "a plan for a grief topic, act %d", act);
            CHECK_EQ(tr.plan.next[0], 0);
        }
        n++;
    }
    CHECK(n >= 3);
    /* the safety acts, the unknown act, the pet */
    for (i = TA_FIRST_SAFETY; i <= TA_LAST_SAFETY; i++) {
        one(id, -1, i, pool[0], 4u, 2, &l, &tr);
        CHECK_MSG(tr.plan.kind == 0, "a plan for the safety act %d", i);
    }
    one(id, -1, TA_UNKNOWN, 0, 4u, 2, &l, &tr);
    CHECK_EQ(tr.plan.kind, 0);
    ids[0] = TALK_PET_ID;
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 1, 3u);
    memset(&tr, 0, sizeof tr);
    talk_gen_line(&c, 0, -1, TA_GREET, 0, &l, &tr);
    CHECK_EQ(tr.plan.kind, 0);
    /* a gate refusal: a romance act toward a child is turned away, and no line of it has a plan */
    ids[0] = id; ids[1] = kids[0];
    for (i = 0; i < 40; i++) {
        int k;
        talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 600u + (uint32_t)i);
        memset(&u, 0, sizeof u);
        u.act[0] = (uint8_t)(i & 1 ? TA_FLIRT : TA_KISS);
        u.target = 1;
        u.conf = 15;
        memset(xt, 0, sizeof xt);
        talk_gen_exchange(&c, &u, &x, xt);
        for (k = 0; k < x.n; k++) CHECK_MSG(xt[k].plan.kind == 0, "a plan on a gate refusal, line %d", k);
    }
    talk_db_close();
}

/* ---- 5: a child in the room ----------------------------------------------------------------- */
TEST(plan_kid_room)
{
    static const uint8_t edgy[] = { TT_HOTHEADED, TT_ROMANTIC, TT_SNOB, TT_JEALOUS, TT_MEAN };
    static const uint8_t acts[] = { TA_INVITE, TA_ASK_FAVOR, TA_ASK_OPINION, TA_COMPLIMENT, TA_INSULT, TA_TEASE, TA_JOKE,
                                    TA_GREET, TA_DISMISS, TA_BLAME, TA_MOCK, TA_THREATEN, TA_HUG };
    static const uint8_t stages[] = { LIFE_CHILD, LIFE_TEEN, LIFE_ADULT, LIFE_ELDER };
    int i, a, e, st, adult, n = 0, cool = 0;
    TalkTraitSet ts, none;
    TalkLine l, l0;
    TalkTrace tr, tr0;
    premade_town();
    CHECK(open_en());
    people();
    topics();
    CHECK(nkid >= 1 && nadult >= 4);
    adult = adults[3];
    /* no plan picks tease, mock, insult, threaten with a child anywhere */
    for (i = 0; i < 1500; i++) {
        int id = i & 1 ? adult : kids[i % nkid];
        one(id, i & 1 ? kids[0] : -1, acts[i % sizeof acts], pool[i % npool], 20000u + (uint32_t)i, i & 15, &l, &tr);
        if (tr.plan.kind) { n++; cool += TALK_COOL_REPLY(l.act) != 0; CHECK(tr.plan.flags & TPL_KIDSAFE); }
    }
    CHECK(n > 500);
    CHECK_MSG(cool == 0, "%d cool reply acts in a kid room", cool);
    /* the edgy traits change nothing with a child in the room (a plan, a voice) and nothing for a minor speaker */
    talk_traitset_clear(&none);
    for (e = 0; e < (int)sizeof edgy; e++) {
        talk_traitset_clear(&ts);
        talk_traitset_add(&ts, edgy[e]);
        for (st = 0; st < (int)sizeof stages; st++) {
            int who = st == 0 ? kids[0] : st == 1 ? teens[0] : adult;      /* the speaker: a child, a teen, an adult */
            int other = st >= 2 ? kids[0] : -1;                             /* an adult speaker has a child with them */
            if (st == 3) other = teens[0];                                   /* ... or a teen */
            for (a = 0; a < (int)sizeof acts; a++) {
                for (i = 0; i < 8; i++) {
                    uint32_t seed = 31u * (uint32_t)i + (uint32_t)a;
                    talk_trait_set(who, &ts);
                    one(who, other, acts[a], pool[(a * 7 + i) % npool], seed, i, &l, &tr);
                    talk_trait_set(who, &none);
                    one(who, other, acts[a], pool[(a * 7 + i) % npool], seed, i, &l0, &tr0);
                    talk_trait_clear(who);
                    CHECK_MSG(tr.plan.kind == tr0.plan.kind && memcmp(tr.plan.eff, tr0.plan.eff, 5) == 0 &&
                              strcmp(l.text, l0.text) == 0,
                              "edgy trait %d, stage %d, act %d changed the plan or the line", edgy[e], stages[st], acts[a]);
                }
            }
        }
    }
    /* the sanity check of the check: an adult in an adult room does feel the trait */
    talk_traitset_clear(&ts);
    talk_traitset_add(&ts, TT_MEAN);
    {
        int diff = 0;
        for (i = 0; i < 200; i++) {
            talk_trait_set(adult, &ts);
            one(adult, -1, TA_INVITE, 0, 61u * (uint32_t)i, i, &l, &tr);
            talk_trait_set(adult, &none);
            one(adult, -1, TA_INVITE, 0, 61u * (uint32_t)i, i, &l0, &tr0);
            talk_trait_clear(adult);
            diff += tr.plan.kind != tr0.plan.kind || memcmp(tr.plan.eff, tr0.plan.eff, 5) != 0;
        }
        CHECK_MSG(diff > 20, "a mean adult in an adult room should differ from none (%d)", diff);
    }
    talk_db_close();
}

/* ---- 6: determinism ------------------------------------------------------------------------- */
TEST(plan_deterministic)
{
    int i, same_turn = 0, differ = 0, id;
    TalkLine a, b;
    TalkTrace ta, tb;
    premade_town();
    CHECK(open_en());
    people();
    topics();
    id = adults[4];
    {
        TalkTraitSet none;
        talk_traitset_clear(&none);
        talk_trait_set(id, &none);
    }
    ((DoomoResident *)town_res(id))->mood = MOOD_OK;
    set_rel(id, TRB_RIVAL);                                      /* a rival's answer to an invitation is not a foregone conclusion */
    /* (the sim fills some facts lazily on the first read: warm it up, then the same call must give the same line) */
    one(id, -1, TA_ASK_OPINION, pool[0], 1u, 1, &a, &ta);
    for (i = 0; i < 100; i++) {
        uint16_t t = pool[i % npool];
        one(id, -1, TA_ASK_OPINION, t, 123u + (uint32_t)i, 5, &a, &ta);
        one(id, -1, TA_ASK_OPINION, t, 123u + (uint32_t)i, 5, &b, &tb);
        same_turn += strcmp(a.text, b.text) == 0 && ta.plan.kind == tb.plan.kind && ta.plan.flags == tb.plan.flags &&
                     memcmp(ta.plan.eff, tb.plan.eff, 5) == 0 && ta.plan.lean == tb.plan.lean;
        one(id, -1, TA_INVITE, 0, 123u + (uint32_t)i, 6, &b, &tb);
        one(id, -1, TA_INVITE, 0, 123u + (uint32_t)i, 9, &a, &ta);
        differ += ta.plan.kind != tb.plan.kind;
    }
    CHECK_EQ(same_turn, 100);
    CHECK_MSG(differ >= 30, "another turn gave the same plan and line %d of 100 times", 100 - differ);
    talk_db_close();
}

/* ---- the guards kind= and trait= --------------------------------------------------------------- */
TEST(plan_guards_kind_and_trait)
{
    static const char *const name[TK_COUNT] = { "", "accept", "agree", "deny", "refuse", "deflect", "counter", "askback" };
    TalkTraitSet shy, none;
    TalkLine l;
    TalkTrace tr;
    int i, id, kinds_seen[TK_COUNT] = { 0 }, bye_shy = 0, bye_plain = 0;
    char want[40];
    premade_town();
    CHECK(open_gentest());
    people();
    id = adults[0];
    talk_traitset_clear(&none);
    talk_trait_set(id, &none);                                       /* (no trait of theirs pulls the kind) */
    CHECK_MSG(talk_gen_kinds_after(TA_INVITE) == (int)(TKM(TK_ACCEPT) | TKM(TK_DENY) | TKM(TK_REFUSE) | TKM(TK_DEFLECT)),
              "kinds after an invitation in the probe grammar: 0x%x", talk_gen_kinds_after(TA_INVITE));
    /* kind=: the line says the kind the plan chose, every time */
    for (i = 0; i < 300; i++) {
        set_rel(id, i % 3 == 0 ? TRB_RIVAL : i % 3 == 1 ? TRB_BEST : TRB_ACQUAINTANCE);
        ((DoomoResident *)town_res(id))->mood = (uint8_t)(i % 5 == 0 ? MOOD_ANGRY : MOOD_OK);
        one(id, -1, TA_INVITE, 0, 900u + (uint32_t)i, i & 15, &l, &tr);
        CHECK(tr.plan.kind >= TK_ACCEPT && tr.plan.kind < TK_COUNT);
        snprintf(want, sizeof want, "Probe %s.", name[tr.plan.kind]);
        CHECK_MSG(strstr(l.text, want) != NULL, "plan %s, line \"%s\"", name[tr.plan.kind], l.text);
        kinds_seen[tr.plan.kind]++;
    }
    CHECK_MSG(kinds_seen[TK_ACCEPT] > 0 && kinds_seen[TK_DENY] + kinds_seen[TK_REFUSE] > 0, "accept %d deny %d refuse %d deflect %d",
              kinds_seen[TK_ACCEPT], kinds_seen[TK_DENY], kinds_seen[TK_REFUSE], kinds_seen[TK_DEFLECT]);
    /* trait=: a shy speaker's line differs from a plain one's, in an adult room; trait= holds for a person who has it */
    talk_traitset_clear(&shy); talk_traitset_add(&shy, TT_SHY);
    talk_trait_set(id, &shy);
    for (i = 0; i < 20; i++) { one(id, -1, TA_FAREWELL, 0, 70u + (uint32_t)i, i, &l, &tr); bye_shy += strstr(l.text, "Bye shy.") != NULL; }
    talk_trait_set(id, &none);
    for (i = 0; i < 20; i++) { one(id, -1, TA_FAREWELL, 0, 70u + (uint32_t)i, i, &l, &tr); bye_plain += strstr(l.text, "Bye plain.") != NULL; }
    talk_trait_clear(id);
    CHECK_EQ(bye_shy, 20);
    CHECK_EQ(bye_plain, 20);
    talk_db_close();
}

/* ---- 7: the soft filter ----------------------------------------------------------------------- */
TEST(plan_soft_filter_counts_misses)
{
    TalkLine l;
    TalkTrace tr;
    int i, id, n_deny = 0, n_miss = 0, t, found = 0;
    premade_town();
    CHECK(open_gentest());
    people();
    topics();
    /* the test grammar's only "no" to an opinion is a rule a ghost speaks: with a lean of -2 the plan says deny and
       no rule of the kind passes the guards */
    for (t = 0; t < npool && t < 400; t++) {
        for (i = 0; i < 6; i++) {
            id = adults[(t + i) % nadult];
            one(id, -1, TA_ASK_OPINION, pool[t], 300u + (uint32_t)t, i, &l, &tr);
            if (tr.plan.kind == TK_DENY && !(tr.plan.flags & TPL_OPEN) && !(l.flags & TLF_FALLBACK)) {   /* (a long topic does not fit the probe) */
                n_deny++;
                n_miss += (tr.plan.flags & TPL_MISS) != 0;
                CHECK_MSG(l.text[0] && !(l.flags & TLF_FALLBACK), "the line was not made: \"%s\"", l.text);
                CHECK_MSG(strstr(l.text, "More:") != NULL, "the unfiltered pick is the bank's own line: \"%s\"", l.text);
                found = 1;
            }
        }
    }
    CHECK_MSG(found, "no decisive deny found to probe");
    CHECK_EQ(n_miss, n_deny);                                   /* every one is counted */
    /* and a plan whose kind has a rule is not counted */
    for (t = 0; t < 200; t++) {
        one(adults[t % nadult], -1, TA_INVITE, 0, 800u + (uint32_t)t, t & 7, &l, &tr);
        CHECK_MSG(!(tr.plan.flags & TPL_MISS), "a miss on the invitation probe");
    }
    talk_db_close();
}

/* ---- the effective voice ----------------------------------------------------------------------- */
TEST(plan_effective_voice)
{
    int id, k, kid_changed = 0;
    uint8_t eff[5];
    TalkTraitSet ts;
    premade_town();
    CHECK(open_en());
    people();
    id = adults[5];
    RES_SET_PERSONA((DoomoResident *)town_res(id), PERS_SPEECH, 12);
    RES_SET_PERSONA((DoomoResident *)town_res(id), PERS_ATTITUDE, 2);
    talk_traitset_clear(&ts);
    talk_traitset_add(&ts, TT_KIND);
    talk_traitset_add(&ts, TT_CHEERFUL);
    talk_trait_set(id, &ts);
    CHECK(talk_gen_eff_persona(id, 0, eff));
    CHECK_EQ(eff[PERS_SPEECH], 12 - 4 - 2);                     /* kind -2*2, cheerful -1*2: the stored sliders plus the deltas */
    CHECK(eff[PERS_ATTITUDE] > 2);
    CHECK_EQ(persona_type(town_res(id)) < PERSONA_TYPE_COUNT, 1);     /* the title still reads the stored ones */
    for (k = 0; k < PERS_COUNT; k++) CHECK(eff[k] <= 15);
    /* an edgy trait: counts for an adult, is dropped for a minor in the room */
    talk_traitset_clear(&ts);
    talk_traitset_add(&ts, TT_MEAN);
    talk_trait_set(id, &ts);
    CHECK(talk_gen_eff_persona(id, 0, eff));
    CHECK(eff[PERS_SPEECH] == 15 || eff[PERS_SPEECH] == 12 + 4);
    {
        uint8_t kid[5];
        CHECK(talk_gen_eff_persona(id, 1, kid));
        CHECK_EQ(kid[PERS_SPEECH], 12);
        CHECK_EQ(kid[PERS_ATTITUDE], 2);
        kid_changed = kid[PERS_SPEECH] != eff[PERS_SPEECH];
    }
    CHECK(kid_changed);
    talk_trait_clear(id);
    CHECK(!talk_gen_eff_persona(TALK_PET_ID, 0, eff));
    talk_db_close();
}

/* ---- 8: the creator's preview ------------------------------------------------------------------- */
static void sample(DoomoResident *r, int stage, int mov, int spe, int exp, int att, int quirk)
{
    resident_default(r);
    RES_SET_STAGE(r, stage);
    RES_SET_PERSONA(r, PERS_MOVEMENT, mov); RES_SET_PERSONA(r, PERS_SPEECH, spe);
    RES_SET_PERSONA(r, PERS_EXPRESSION, exp); RES_SET_PERSONA(r, PERS_ATTITUDE, att);
    RES_SET_PERSONA(r, PERS_QUIRK, quirk);
}

TEST(preview_four_lines)
{
    DoomoResident r, kid;
    TalkTraitSet ts, ts2;
    TalkLine l, l2;
    TalkAudience adults_only, kids_room;
    int t, kind, no_act = 0, refusals = 0, changed = 0, n, kchg[TP_KINDS] = { 0 };
    char a[TALK_LINE_MAX], b[TALK_LINE_MAX];
    uint32_t h0;
    premade_town();
    CHECK(open_en());
    sample(&r, LIFE_ADULT, 8, 8, 8, 8, 8);
    sample(&kid, LIFE_CHILD, 8, 8, 8, 8, 8);
    memset(&adults_only, 0, sizeof adults_only);
    memset(&kids_room, 0, sizeof kids_room);
    kids_room.any_minor = kids_room.avatar_minor = 1;
    h0 = talk_safe_lint_hits();
    for (t = 1; t < TT_COUNT; t++) {
        talk_traitset_clear(&ts);
        talk_traitset_add(&ts, t);
        for (kind = 0; kind < TP_KINDS; kind++) {
            int sw = 0, seed;
            for (seed = 1; seed <= 4; seed++) {
                CHECK(talk_preview_line(&r, &ts, kind, (uint32_t)seed, &l) > 0);
                CHECK_MSG(talk_safe_lint(l.text, &adults_only, &sw) == TL_OK, "trait %d kind %d: \"%s\"", t, kind, l.text);
                CHECK_EQ(talk_preview(&r, &ts, kind, (uint32_t)seed, a, sizeof a), (int)strlen(l.text));
                CHECK_STR(a, l.text);                                   /* the same call, the same line */
                if (kind == TP_REFUSAL) { refusals++; no_act += l.act == TA_NO; }
                /* a minor avatar gets a kid-safe line */
                sw = 0;
                CHECK(talk_preview_line(&kid, &ts, kind, (uint32_t)seed, &l2) > 0);
                CHECK_MSG(talk_safe_lint(l2.text, &kids_room, &sw) == TL_OK, "minor, trait %d kind %d: \"%s\"", t, kind, l2.text);
                CHECK_MSG(talk_safe_lint_probe(l2.text, &kids_room, 1) == TL_OK, "minor, trait %d: \"%s\"", t, l2.text);
            }
        }
    }
    CHECK_EQ(talk_safe_lint_hits(), h0);
    CHECK_MSG(no_act * 100 >= refusals * 90, "a refusal preview says no %d of %d times", no_act, refusals);
    /* unsaved traits change the line: the same character with two different sets */
    talk_traitset_clear(&ts); talk_traitset_add(&ts, TT_KIND); talk_traitset_like(&ts, TCR_FOOD);
    talk_traitset_clear(&ts2); talk_traitset_add(&ts2, TT_MEAN); talk_traitset_dislike(&ts2, TCR_FOOD);
    for (n = 0; n < 30; n++) {
        for (kind = 0; kind < TP_KINDS; kind++) {
            talk_preview(&r, &ts, kind, 50u + (uint32_t)n, a, sizeof a);
            talk_preview(&r, &ts2, kind, 50u + (uint32_t)n, b, sizeof b);
            changed += strcmp(a, b) != 0;
            kchg[kind] += strcmp(a, b) != 0;
        }
    }
    printf("      preview: %d of %d refusals are a no; kind vs mean differ in %d of 90 lines (greeting %d, answer %d, refusal %d of 30)\n",
           no_act, refusals, changed, kchg[0], kchg[1], kchg[2]);
    CHECK_MSG(changed >= 30, "two trait sets gave the same preview line %d of 90 times", 90 - changed);
    talk_db_close();
}

/* ---- acts.tsv KINDS reach the engine -------------------------------------------------------------- */
TEST(plan_acts_table_kinds)
{
    premade_town();
    CHECK(open_en());
    CHECK(talk_gen_kinds_of(TA_YES) & TKM(TK_ACCEPT));
    CHECK(talk_gen_kinds_of(TA_NO) & TKM(TK_DENY) && talk_gen_kinds_of(TA_NO) & TKM(TK_REFUSE));
    CHECK(talk_gen_kinds_of(TA_OPINE_POS) == TKM(TK_AGREE));
    CHECK(talk_gen_kinds_of(TA_JOKE) == TKM(TK_DEFLECT));
    CHECK(talk_gen_kinds_of(TA_ASK_ABOUT) == TKM(TK_ASKBACK));
    CHECK(talk_gen_kinds_of(TA_REFUSE_SEXUAL) == 0 && talk_gen_kinds_of(TA_DISTRESS) == 0);
    /* what is allowed after an act: the bank's own cells, a subset of the replies' kinds */
    CHECK(talk_gen_kinds_after(TA_INVITE) & TKM(TK_ACCEPT));
    CHECK(talk_gen_kinds_after(TA_INVITE) & (TKM(TK_DENY) | TKM(TK_REFUSE)));
    CHECK(talk_gen_kinds_after(TA_ASK_OPINION) & TKM(TK_AGREE) && talk_gen_kinds_after(TA_ASK_OPINION) & TKM(TK_DENY));
    CHECK_EQ(talk_gen_kinds_after(TA_REFUSE_MINOR), 0);
    CHECK_EQ(talk_gen_kinds_after(0), 0);
    talk_db_close();
}

/* ---- TM2F-Q8: the side of an opinion answer ---------------------------------------------------------- */
TEST(plan_kinds_reply_side)
{
    premade_town();
    CHECK(open_en());
    /* after a negative opinion an AGREE is a no to the topic and a DISAGREE is a yes; after a positive one the plain reading */
    CHECK_EQ(talk_gen_reply_side(TA_OPINE_NEG, TA_AGREE), -1);
    CHECK_EQ(talk_gen_reply_side(TA_OPINE_NEG, TA_DISAGREE), 1);
    CHECK_EQ(talk_gen_reply_side(TA_OPINE_POS, TA_AGREE), 1);
    CHECK_EQ(talk_gen_reply_side(TA_OPINE_POS, TA_DISAGREE), -1);
    CHECK_EQ(talk_gen_reply_side(TA_ASK_OPINION, TA_OPINE_POS), 1);
    CHECK_EQ(talk_gen_reply_side(TA_ASK_OPINION, TA_OPINE_NEG), -1);
    CHECK_EQ(talk_gen_reply_side(TA_OPINE_NEG, TA_JOKE), 0);
    CHECK(talk_gen_kinds_reply(TA_OPINE_NEG, TA_AGREE) & TKM(TK_DENY));
    CHECK(!(talk_gen_kinds_reply(TA_OPINE_NEG, TA_AGREE) & (TKM(TK_ACCEPT) | TKM(TK_AGREE))));
    CHECK(talk_gen_kinds_reply(TA_OPINE_NEG, TA_DISAGREE) & TKM(TK_AGREE));
    CHECK(!(talk_gen_kinds_reply(TA_OPINE_NEG, TA_DISAGREE) & TKM(TK_DENY)));
    CHECK_EQ(talk_gen_kinds_reply(TA_OPINE_POS, TA_AGREE), talk_gen_kinds_of(TA_AGREE));
    CHECK_EQ(talk_gen_kinds_reply(TA_OPINE_NEG, TA_JOKE), talk_gen_kinds_of(TA_JOKE));
    talk_db_close();
}

/* Q8a on produced lines: an answer to an opinion never takes the side the speaker's own stance is against, close call
 * or not, unless the plan went against the lean for a reason (design 7.2). Before TM2F-Q8 an open plan filtered nothing
 * and a "no" after a negative opinion was served by DISAGREE, which says yes to the topic. */
TEST(plan_stance_line)
{
    static const int acts[3] = { TA_OPINE_POS, TA_OPINE_NEG, TA_ASK_OPINION };
    int a, i, k, checked = 0, clash = 0;
    premade_town();
    CHECK(open_en());
    people();
    topics();
    CHECK(nadult >= 4 && npool >= 100);
    for (a = 0; a < 3; a++)
        for (k = 1; k < 5; k++)
            for (i = 0; i < 60; i++) {
                TalkLine l;
                TalkTrace tr;
                uint16_t topic = pool[rnd((uint32_t)npool)];
                int st;
                one(adults[k], -1, acts[a], topic, 4100u + (uint32_t)i * 31u, i & 31, &l, &tr);
                st = l.stance;
                if (!tr.plan.kind || st == TST_NEUTRAL || (l.flags & (TLF_SINCERE | TLF_FALLBACK)) || (tr.plan.flags & TPL_REASON)) continue;
                {
                    int side = talk_gen_reply_side(acts[a], l.act);
                    checked++;
                    if ((st == TST_PRO && side < 0) || (st == TST_CON && side > 0)) clash++;
                }
            }
    CHECK_MSG(checked >= 200, "only %d opinion answers had a stance", checked);
    CHECK_MSG(clash == 0, "%d of %d opinion answers take the side the speaker is against", clash, checked);
    talk_db_close();
}
