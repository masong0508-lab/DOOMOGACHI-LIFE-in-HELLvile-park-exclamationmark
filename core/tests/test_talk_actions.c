/* test_talk_actions.c - TM2e: sim actions a reply can carry
 * (core/src/talk_act.c, talk_dm.c, talk_gen.c, talk_outcome.c, sim_nudge.c; docs/TALK.md 6p;
 * design docs/design/talk/TALK_MULTILEVEL.md 6 and 13 TM2e, tests 1 to 4 and 7).
 * Owner: TALK. Each test fails without the code it covers.
 *
 *   action_says_only_if_done     3,000 sessions: every line that carries an action has its event, every action event
 *                                has its line, talk_outcome_apply applies every one (skipped 0), walk-offs match TEV_WALKOUT
 *   action_caps                  invitation 1 a resident a day and 3 a town day, gift 1 and 3, ask 1; after a cap the plan
 *                                refuses with a reason (TAW_*), says no, and the sim state is unchanged
 *   action_minor_rules           the exhaustive stage x venue x item x partner tables, and the same rules through the
 *                                drama manager (a child's room: no vape shop, no night beach, no partner called in)
 *   action_sim_effects           an accepted invitation runs sim_nudge_activity once and the activity becomes the venue's;
 *                                a gift moves one item; an ask raises one PROB_WANTS_ITEM, only without an open problem
 *   action_sim_nudge             the sim hook refuses the asleep, the possessed, the ghost, the night park, the early vape shop
 *   action_p2_p3_p7_random       1,500 random sessions with traits and actions on: no romance line near a minor, no couple
 *                                made, no minor invited anywhere unsafe, nothing applied that was not said
 */
#include "test.h"
#include <stdio.h>
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/talk_traits.h"
#include "doomo/lang.h"
#include "doomo/sim.h"
#include "doomo/items.h"
#include "doomo/resident_acc.h"
#include "doomo/playas.h"
#include "../src/fp_int.h"
#include "../src/fp_island.h"

/* Free Play island menu's read-only inspection hooks. */
int fp_island_select_kind(int kind);
int fp_island_menu_open(void);
int fp_island_menu_count(void);
const char *fp_island_menu_label(int index);
int fp_island_menu_select(const char *label);

static void premade_town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    doomo_town.minute = 12 * 60;                      /* noon: every venue is open */
    sim_init();
    talk_outcome_reset();
}

static int open_en(void) { return talk_db_open(lang_find("en")) == TALK_DB_OK && talk_gen_ready(); }

static uint16_t concept(const char *key)
{
    uint16_t c = 0, wf = 0;
    uint32_t tags[4];
    if (talk_lex_find(key, &c, &wf, tags) < 0) return 0;
    return c;
}

static uint32_t rs = 0x2545F491u;
static uint32_t rnd(uint32_t n) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return n ? rs % n : 0; }

static int adults[60], nadult, kids[8], nkid, teens[8], nteen;
static void people(void)
{
    int id;
    nadult = nkid = nteen = 0;
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        const DoomoResident *r = town_res(id);
        if (r->status != RES_ALIVE || (r->flags & RF_CUSTOM) || !sim_nudge_ok(id, ACT_PARK)) continue;
        if (sim_life_stage(id) == LIFE_TEEN) { if (nteen < 8) teens[nteen++] = id; }
        else if (talk_safe_minor(id)) { if (nkid < 8) kids[nkid++] = id; }
        else if (id < DOOMO_PLAYER_ID && nadult < 60) adults[nadult++] = id;
    }
}

static void set_rel(int id, int kind, int value)
{
    DoomoEdge *e;
    e = rel_get_or_add(DOOMO_PLAYER_ID, id);
    if (e) { e->kind = (uint8_t)kind; e->value = (int8_t)value; }
    e = rel_get_or_add(id, DOOMO_PLAYER_ID);
    if (e) { e->kind = (uint8_t)kind; e->value = (int8_t)value; }
}

static void set_trait(int id, int t)
{
    TalkTraitSet ts;
    talk_traitset_clear(&ts);
    talk_traitset_add(&ts, t);
    talk_trait_set(id, &ts);
}

static int has_ev(const TalkSummary *s, int kind)
{
    int k, n = 0;
    for (k = 0; k < s->nev; k++) if (s->ev[k].kind == kind) n++;
    return n;
}

static int st_ev(const TalkState *s, int kind)
{
    int k, n = 0;
    for (k = 0; k < s->nev; k++) if (s->ev[k].kind == kind) n++;
    return n;
}

/* ---- one session: random acts, the lines of every exchange tallied, then the end applies it ------------------ */
typedef struct Tally { int line[TACT_COUNT]; int start_bad_venue; } Tally;

static const int speak_acts[] = { TA_INVITE, TA_INVITE, TA_THANK, TA_COMPLIMENT, TA_GREET, TA_ASK_ABOUT, TA_TELL, TA_ENCOURAGE, TA_JOKE, TA_ASK_OPINION };

static const int cool_acts[] = { TA_INSULT, TA_THREATEN, TA_COMPLAIN, TA_BLAME, TA_COMFORT, TA_SYMPATHIZE, TA_DISMISS, TA_MOCK };
static int g_cool;                                        /* 1: the player only speaks hostile or vulnerable acts */

static void run_session(uint32_t seed, int avatar, const int *ids, int n, int turns, const uint16_t *topics, int ntopics,
                        Tally *t, TalkOutcome *out, TalkSummary *sum, TalkState *keep)
{
    TalkState local, *s = keep ? keep : &local;
    int k, tn;
    talk_outcome_begin(s, avatar, ids, n, seed);
    for (tn = 0; tn < turns && !talk_dm_over(s); tn++) {
        TalkUtter u;
        memset(&u, 0, sizeof u);
        u.act[0] = g_cool ? (uint8_t)cool_acts[rnd((uint32_t)(sizeof cool_acts / sizeof cool_acts[0]))]
                          : (uint8_t)speak_acts[rnd((uint32_t)(sizeof speak_acts / sizeof speak_acts[0]))];
        u.topic[0] = ntopics ? topics[rnd((uint32_t)ntopics)] : 0;
        u.target = (int8_t)(n == 1 ? 0 : (int)rnd((uint32_t)n + 1) - 1);
        u.conf = 15;
        s->part[0].engage = 80;
        talk_dm_say(s, &u);
        while (talk_dm_step(s, NULL)) {}
        for (k = 0; k < s->nlines; k++) if (s->line[k].action) { t->line[s->line[k].action]++; }
    }
    memset(out, 0, sizeof *out);
    talk_outcome_end(s, out, sum);
}

/* make the speakers likely to act: best friends, outgoing or glutton, happy */
static void prime(void)
{
    int k;
    talk_trait_reset();                                   /* the table keeps explicit sets for 24 people: the first 20 adults get one */
    for (k = 0; k < nadult; k++) {
        set_rel(adults[k], REL_BEST_FRIEND, 85);
        town_res(adults[k])->mood = MOOD_HAPPY;
        if (k < 20) set_trait(adults[k], k % 3 == 0 ? TT_GLUTTON : k % 3 == 1 ? TT_OUTGOING : TT_CURIOUS);
    }
    for (k = 0; k < nkid; k++) set_rel(kids[k], REL_FRIEND, 50);
}

TEST(action_says_only_if_done)
{
    static const char *const keys[] = { "park", "beach", "shop", "workshop", "brimstone bites", "st cuddles", "concert hall", "cloud nine vapor", "pie" };
    uint16_t topics[9];
    int nt = 0, k, it, tot[TACT_COUNT], walk_lines = 0, walk_ev = 0, applied[4] = { 0, 0, 0, 0 };
    premade_town();
    CHECK(open_en());
    people();
    CHECK(nadult >= 20 && nkid >= 1);
    prime();
    for (k = 0; k < 9; k++) { uint16_t c = concept(keys[k]); if (c) topics[nt++] = c; }
    CHECK(nt >= 6);
    memset(tot, 0, sizeof tot);
    for (it = 0; it < 3000; it++) {
        int ids[3], n = 1 + it % 3, j;
        Tally t;
        TalkOutcome out;
        TalkSummary sum;
        memset(&t, 0, sizeof t);
        ids[0] = adults[(it * 7) % nadult];
        ids[1] = (it % 4 == 0 && nkid) ? kids[it % nkid] : adults[(it * 7 + 3) % nadult];
        ids[2] = adults[(it * 7 + 11) % nadult];
        for (j = 1; j < n; j++) if (ids[j] == ids[0]) ids[j] = adults[(it * 7 + 17 + j) % nadult];
        if (n == 3 && ids[1] == ids[2]) n = 2;
        talk_outcome_reset();                              /* the caps are the next test's: every session starts a fresh day */
        run_session(0x7000u + (uint32_t)it, DOOMO_PLAYER_ID, ids, n, 5, topics, nt, &t, &out, &sum, NULL);
        /* every line that carries an action has its event, every event has its line (a walk-off is the existing TEV_WALKOUT) */
        CHECK_EQ(t.line[TACT_START], has_ev(&sum, TEV_ACT_INVITE));
        CHECK_EQ(t.line[TACT_GIFT], has_ev(&sum, TEV_ACT_GIFT));
        CHECK_EQ(t.line[TACT_ASKITEM], has_ev(&sum, TEV_ACT_ASK));
        CHECK_EQ(t.line[TACT_CALL], has_ev(&sum, TEV_ACT_CALL));
        walk_lines += t.line[TACT_WALKOFF];
        walk_ev += has_ev(&sum, TEV_WALKOUT);
        /* and the apply did every one of them */
        CHECK_EQ((int)out.skipped, 0);
        CHECK_EQ((int)out.invited, t.line[TACT_START]);
        CHECK_EQ((int)out.gifted, t.line[TACT_GIFT]);
        CHECK_EQ((int)out.asked, t.line[TACT_ASKITEM]);
        CHECK_EQ((int)out.called, t.line[TACT_CALL]);
        for (j = TACT_START; j <= TACT_CALL; j++) tot[j] += t.line[j];
        applied[0] += out.invited; applied[1] += out.gifted; applied[2] += out.asked; applied[3] += out.called;
    }
    CHECK_EQ(walk_lines, walk_ev);
    printf("      3,000 sessions: %d starts, %d gifts, %d asks, %d calls, every one applied (%d / %d / %d / %d); %d walk-offs\n",
           tot[TACT_START], tot[TACT_GIFT], tot[TACT_ASKITEM], tot[TACT_CALL], applied[0], applied[1], applied[2], applied[3], walk_lines);
    CHECK(tot[TACT_START] >= 30 && tot[TACT_GIFT] >= 10 && tot[TACT_ASKITEM] >= 10 && tot[TACT_CALL] >= 10);
    talk_db_close();
}

/* ---- caps ------------------------------------------------------------------------------------------------------- */
/* one invitation to the park in a talk with `id` alone; seeds tried until the plan accepts; returns the session's outcome */
static int try_invite(int id, uint32_t seed0, int tries, TalkState *s, TalkOutcome *out, TalkSummary *sum, uint16_t topic)
{
    int tr;
    for (tr = 0; tr < tries; tr++) {
        TalkUtter u;
        int ids[1], got;
        ids[0] = id;
        talk_outcome_begin(s, DOOMO_PLAYER_ID, ids, 1, seed0 + (uint32_t)tr);
        memset(&u, 0, sizeof u);
        u.act[0] = TA_INVITE;
        u.topic[0] = topic;
        u.target = 0;
        u.conf = 15;
        s->part[0].engage = 85;
        talk_dm_say(s, &u);
        while (talk_dm_step(s, NULL)) {}
        got = st_ev(s, TEV_ACT_INVITE);
        memset(out, 0, sizeof *out);
        talk_outcome_end(s, out, sum);
        if (got) return 1;
    }
    return 0;
}

/* does the first line say a plain yes (every kind it serves is accept or agree)? */
static int says_yes(const TalkState *s)
{
    int kinds;
    if (s->nlines < 1) return 0;
    kinds = talk_gen_kinds_of(s->line[0].act);
    return kinds && !(kinds & ~(int)(TKM(TK_ACCEPT) | TKM(TK_AGREE)));
}

TEST(action_caps)
{
    TalkState s;
    TalkOutcome out;
    TalkSummary sum;
    uint16_t park = 0;
    int a, b, c, d, e, k, yes = 0, ref = 0, act0;
    premade_town();
    CHECK(open_en());
    people();
    prime();
    park = concept("park");
    CHECK(park);
    a = adults[0]; b = adults[1]; c = adults[2]; d = adults[3]; e = adults[4];
    /* once a resident a day: the first is taken, the second is refused with a reason and changes nothing */
    CHECK(try_invite(a, 100u, 60, &s, &out, &sum, park));
    CHECK_EQ((int)out.invited, 1);
    CHECK_EQ((int)town_res(a)->activity, (int)ACT_PARK);
    CHECK(!talk_outcome_can(TEV_ACT_INVITE, a));
    town_res(a)->activity = ACT_CAFE;                         /* anything else: if the sim were touched it would move */
    for (k = 0; k < 30; k++) {
        int ids[1] = { a };
        TalkUtter u;
        talk_outcome_begin(&s, DOOMO_PLAYER_ID, ids, 1, 900u + (uint32_t)k);
        memset(&u, 0, sizeof u);
        u.act[0] = TA_INVITE; u.topic[0] = park; u.target = 0; u.conf = 15;
        s.part[0].engage = 85;
        talk_dm_say(&s, &u);
        while (talk_dm_step(&s, NULL)) {}
        CHECK_EQ((int)s.part[0].actp, (int)TACT_REFUSE);
        CHECK_EQ((int)s.part[0].actw, (int)TAW_CAP_DAY);
        CHECK_EQ(st_ev(&s, TEV_ACT_INVITE), 0);
        yes += says_yes(&s);
        ref++;
        memset(&out, 0, sizeof out);
        talk_outcome_end(&s, &out, &sum);
        CHECK_EQ((int)out.invited, 0);
        CHECK_EQ((int)out.skipped, 0);
    }
    CHECK_EQ((int)town_res(a)->activity, (int)ACT_CAFE);     /* the sim state is unchanged */
    printf("      second invitation, 30 seeds: refused with a reason, %d of %d first lines still a plain yes\n", yes, ref);
    CHECK_EQ(yes, 0);
    /* 3 a town day: b and c are taken, d is the fourth and is refused */
    act0 = town_res(d)->activity;
    CHECK(try_invite(b, 200u, 60, &s, &out, &sum, park));
    CHECK(try_invite(c, 300u, 60, &s, &out, &sum, park));
    CHECK(!try_invite(d, 400u, 20, &s, &out, &sum, park));
    CHECK_EQ((int)s.part[0].actw, (int)TAW_CAP_TOWN);
    CHECK_EQ((int)town_res(d)->activity, act0);
    CHECK(!talk_outcome_can(TEV_ACT_INVITE, e));
    /* the next town day starts clean */
    doomo_town.day++;
    CHECK(talk_outcome_can(TEV_ACT_INVITE, d));
    CHECK(talk_outcome_can(TEV_ACT_INVITE, a));
    /* gifts: 1 a resident, 3 a town day (the events applied by hand: the apply is the rule) */
    {
        TalkSummary g;
        TalkOutcome o;
        int item = ITEM_CUPCAKE, before, who[5], j;
        for (j = 0; j < 5; j++) who[j] = adults[10 + j];
        for (j = 0; j < 5; j++) {
            memset(&g, 0, sizeof g);
            g.avatar = DOOMO_PLAYER_ID; g.n = 1; g.id[0] = (int16_t)who[j];
            g.nev = 1; g.ev[0].kind = TEV_ACT_GIFT; g.ev[0].a = 0; g.ev[0].b = (uint8_t)item;
            before = inv_count(item);
            memset(&o, 0, sizeof o);
            talk_outcome_apply(&g, &o);
            if (j < 3) { CHECK_EQ((int)o.gifted, 1); CHECK_EQ(inv_count(item), before + 1); }
            else { CHECK_EQ((int)o.gifted, 0); CHECK_EQ((int)o.skipped, 1); CHECK_EQ(inv_count(item), before); }
        }
        memset(&g, 0, sizeof g);                              /* the same resident again: refused, nothing moves */
        g.avatar = DOOMO_PLAYER_ID; g.n = 1; g.id[0] = (int16_t)who[0];
        g.nev = 1; g.ev[0].kind = TEV_ACT_GIFT; g.ev[0].b = (uint8_t)item;
        before = inv_count(item);
        memset(&o, 0, sizeof o);
        talk_outcome_apply(&g, &o);
        CHECK_EQ((int)o.skipped, 1);
        CHECK_EQ(inv_count(item), before);
        CHECK(!talk_outcome_can(TEV_ACT_GIFT, who[0]) && !talk_outcome_can(TEV_ACT_GIFT, who[4]));
        /* ask: 1 a resident (and nothing without room in the queue: the sim's own rule) */
        memset(&g, 0, sizeof g);
        g.avatar = DOOMO_PLAYER_ID; g.n = 1; g.id[0] = (int16_t)who[0];
        g.nev = 1; g.ev[0].kind = TEV_ACT_ASK; g.ev[0].b = (uint8_t)item;
        CHECK(talk_outcome_can(TEV_ACT_ASK, who[0]));
        memset(&o, 0, sizeof o);
        talk_outcome_apply(&g, &o);
        CHECK_EQ((int)o.asked + (int)o.skipped, 1);
        if (o.asked) CHECK(!talk_outcome_can(TEV_ACT_ASK, who[0]));
    }
    talk_db_close();
}

/* ---- the minor rules, as tables and through the drama manager ----------------------------------------------------- */
TEST(action_minor_rules)
{
    int venue, minor, hour, act, item, g, t, sp, pa, al;
    static const int acts[] = { TA_INVITE, TA_ASK_OUT, TA_GIVE, TA_OFFER, TA_ASK_FAVOR, TA_NONE };
    static const int adult_items[] = { ITEM_LOVE_LETTER, ITEM_PERFUME, ITEM_RING, ITEM_VAPE_PEN, ITEM_CHILL_PILL, ITEM_ENERGY_DRINK, ITEM_COFFEE };
    for (act = 0; act < (int)(sizeof acts / sizeof acts[0]); act++)
        for (venue = 0; venue < ACT_COUNT; venue++)
            for (minor = 0; minor < 2; minor++)
                for (hour = 0; hour < 24; hour++) {
                    int want = 0, got = talk_act_invite_ok(acts[act], venue, minor, hour);
                    if (acts[act] == TA_INVITE) {
                        if (venue == ACT_PARK || venue == ACT_CAFE || venue == ACT_CHURCH || venue == ACT_BEACH) want = !minor || (hour >= 8 && hour < 18);
                        else if (venue == ACT_SHOP || venue == ACT_WORKSHOP || venue == ACT_CONCERT || venue == ACT_VAPE_SHOP) want = !minor;
                    }
                    CHECK_MSG(got == want, "act %d venue %d minor %d hour %d: %d, want %d", acts[act], venue, minor, hour, got, want);
                    if (minor) CHECK(!(venue == ACT_VAPE_SHOP && got));
                    if (acts[act] == TA_ASK_OUT) CHECK(!got);            /* a date is never an invitation action */
                }
    for (item = 0; item <= ITEM_COUNT; item++)
        for (g = 0; g < 2; g++)
            for (t = 0; t < 2; t++) {
                int got = talk_act_gift_ok(item, g, t), adult = 0, i, want;
                for (i = 0; i < (int)(sizeof adult_items / sizeof adult_items[0]); i++) if (item == adult_items[i]) adult = 1;
                CHECK_EQ(talk_act_item_adult(item), adult);
                if (item <= ITEM_NONE || item >= ITEM_COUNT || doomo_items[item].category == ICAT_KEY) want = 0;
                else if (g || t) want = !adult && (doomo_items[item].category == ICAT_FOOD || item == ITEM_PLUSHIE || item == ITEM_BOARD_GAME || item == ITEM_VIDEO_GAME);
                else want = 1;
                CHECK_MSG(got == want, "item %d giver minor %d taker minor %d: %d, want %d", item, g, t, got, want);
            }
    for (sp = 0; sp < 2; sp++) for (pa = 0; pa < 2; pa++) for (al = 0; al < 2; al++)
        CHECK_EQ(talk_act_call_ok(sp, pa, al), al && !(sp && pa));
    /* through the drama manager */
    {
        TalkState s;
        TalkOutcome out;
        TalkSummary sum;
        uint16_t beach, vape;
        int k, kid_beach_night = 0, partner_called = 0, adult_partner_called = 0, sess;
        premade_town();
        CHECK(open_en());
        people();
        prime();
        CHECK(nkid >= 1);
        beach = concept("beach");
        vape = concept("cloud nine vapor");
        CHECK(beach && vape);
        /* a child, at night, asked to the beach: refused for the rule (the sim would say no too, the rule is first) */
        doomo_town.minute = 22 * 60;
        town_res(kids[0])->activity = ACT_HOME;
        {
            int ids[1] = { kids[0] };
            for (k = 0; k < 20; k++) {
                TalkUtter u;
                talk_outcome_begin(&s, DOOMO_PLAYER_ID, ids, 1, 5000u + (uint32_t)k);
                memset(&u, 0, sizeof u);
                u.act[0] = TA_INVITE; u.topic[0] = beach; u.target = 0; u.conf = 15;
                s.part[0].engage = 90;
                talk_dm_say(&s, &u);
                while (talk_dm_step(&s, NULL)) {}
                CHECK_EQ((int)s.part[0].actw, (int)TAW_RULE);
                CHECK_EQ(st_ev(&s, TEV_ACT_INVITE), 0);
                kid_beach_night++;
                talk_outcome_end(&s, &out, &sum);
                CHECK_EQ((int)town_res(kids[0])->activity, (int)ACT_HOME);
            }
        }
        /* the vape shop: an adult, alone with the player, is taken (non-vacuous); with a child in the room never */
        doomo_town.minute = 12 * 60;
        {
            int adult = adults[5], taken = 0, ids2[2];
            for (k = 0; k < 40 && !taken; k++) {
                int ids[1] = { adult };
                TalkUtter u;
                talk_outcome_reset();
                talk_outcome_begin(&s, DOOMO_PLAYER_ID, ids, 1, 7000u + (uint32_t)k);
                memset(&u, 0, sizeof u);
                u.act[0] = TA_INVITE; u.topic[0] = vape; u.target = 0; u.conf = 15;
                s.part[0].engage = 90;
                talk_dm_say(&s, &u);
                while (talk_dm_step(&s, NULL)) {}
                if (s.part[0].actp == TACT_ACCEPT) taken = 1;
                memset(&out, 0, sizeof out);
                talk_outcome_end(&s, &out, &sum);
                if (out.invited) CHECK_EQ((int)town_res(adult)->activity, (int)ACT_VAPE_SHOP);
            }
            printf("      vape shop: an adult alone with the player is %s; ", taken ? "taken" : "NOT taken");
            ids2[0] = adults[6]; ids2[1] = kids[0];
            for (k = 0; k < 40; k++) {
                TalkUtter u;
                talk_outcome_reset();
                talk_outcome_begin(&s, DOOMO_PLAYER_ID, ids2, 2, 7500u + (uint32_t)k);
                memset(&u, 0, sizeof u);
                u.act[0] = TA_INVITE; u.topic[0] = vape; u.target = 0; u.conf = 15;
                s.part[0].engage = 90;
                talk_dm_say(&s, &u);
                while (talk_dm_step(&s, NULL)) {}
                CHECK(s.part[0].actp != TACT_ACCEPT);
                CHECK_EQ(st_ev(&s, TEV_ACT_INVITE), 0);
                talk_outcome_end(&s, &out, &sum);
            }
            printf("with a child in the room 0 of 40\n");
            CHECK(taken);
        }
        /* no call-over puts a partner with a child: the speaker's partner is the best candidate (a bond, the same venue) */
        {
            int speaker = adults[7], partner = adults[8], kid = kids[0];
            set_rel(partner, REL_BEST_FRIEND, 20);
            rel_get_or_add(speaker, partner)->kind = REL_DATING;
            rel_get_or_add(speaker, partner)->value = 90;
            rel_get_or_add(partner, speaker)->kind = REL_DATING;
            rel_get_or_add(partner, speaker)->value = 90;
            town_res(partner)->activity = town_res(speaker)->activity;
            town_res(partner)->status = RES_ALIVE;
            for (sess = 0; sess < 400; sess++) {
                int ids[2], n = sess % 2 ? 2 : 1;
                Tally tl;
                ids[0] = speaker; ids[1] = kid;
                memset(&tl, 0, sizeof tl);
                talk_outcome_reset();
                run_session(9000u + (uint32_t)sess, DOOMO_PLAYER_ID, ids, n, 6, NULL, 0, &tl, &out, &sum, &s);
                if (sum.call_to) {
                    if (sum.call_to - 1 == partner) { if (n == 2) partner_called++; else adult_partner_called++; }
                }
            }
            printf("      call-over: the partner was called %d times with a child in the room and %d times in an adult room\n", partner_called, adult_partner_called);
            CHECK_EQ(partner_called, 0);
            CHECK(adult_partner_called >= 1);                         /* the rule is what stopped it, not luck */
        }
        /* a minor who likes coffee gives and asks for nothing adult */
        {
            int fi, found = 0;
            for (fi = 0; fi < 12; fi++) {
                int id = kids[0];
                res_gset(town_res(id), GF_LIKE_FOOD, (uint32_t)(ITEM_COFFEE - ITEM_CUPCAKE));
                res_gset(town_res(id), GF_HATE_FOOD, (uint32_t)(ITEM_PUDDING - ITEM_CUPCAKE));
                CHECK_EQ(talk_act_food_item(id, 1), 0);
                CHECK(talk_act_food_item(id, 0) == ITEM_COFFEE);
                found = 1;
            }
            CHECK(found);
        }
        (void)kid_beach_night; (void)adult_partner_called;
        talk_db_close();
    }
}

/* ---- the sim effects ------------------------------------------------------------------------------------------ */
static int items_total(void)
{
    int i, n = 0;
    for (i = 1; i < ITEM_COUNT; i++) n += inv_count(i);
    return n;
}

/* A real invitation can be consulted without giving out a free visit.
 * A temporarily changed schedule hides the available arrival point, but
 * the Talk journal may still tell the player where the plan was made. */
TEST(action_meetup_plan_is_read_only_and_day_scoped)
{
    TalkState s;
    TalkOutcome o;
    TalkSummary sum;
    uint16_t cafe;
    int a;
    premade_town();
    CHECK(open_en());
    people();
    prime();
    CHECK(nadult > 0);
    a = adults[0];
    cafe = concept("brimstone bites");
    CHECK(cafe && talk_act_venue_of(cafe) == ACT_CAFE);
    CHECK_EQ(talk_outcome_meet_plan(a), -1);
    CHECK(try_invite(a, 12000u, 150, &s, &o, &sum, cafe));
    CHECK_EQ(o.invited, 1);
    CHECK_EQ(talk_outcome_meet_plan(a), ACT_CAFE);
    CHECK_EQ(talk_outcome_meet_plan(a), ACT_CAFE); /* inspection never consumes */
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), a);
    town_res(a)->activity = ACT_PARK;
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), -1);
    CHECK_EQ(talk_outcome_meet_plan(a), ACT_CAFE); /* honest missed-plan cue */
    town_res(a)->activity = ACT_CAFE;
    CHECK_EQ(talk_outcome_meet(ACT_CAFE), a);
    CHECK_EQ(talk_outcome_meet_plan(a), -1);
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), -1);
    doomo_town.day++;
    CHECK_EQ(talk_outcome_meet_plan(a), -1);
    talk_db_close();
}

TEST(action_meetup_requires_a_real_matching_place)
{
    TalkState s;
    TalkOutcome o;
    TalkSummary sum;
    uint16_t cafe;
    int a, happy, news_before, news_after;
    premade_town();
    CHECK(open_en());
    people();
    prime();
    CHECK(nadult > 0);
    a = adults[0];
    cafe = concept("brimstone bites");
    CHECK(cafe && talk_act_venue_of(cafe) == ACT_CAFE);
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), -1);
    CHECK(try_invite(a, 12000u, 150, &s, &o, &sum, cafe));
    CHECK_EQ(o.invited, 1);
    CHECK_EQ(talk_outcome_meet_ready(ACT_PARK), -1);
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), a);
    happy = town_res(a)->stat[STAT_HAPPY];
    news_before = sim_news_count();
    CHECK_EQ(talk_outcome_meet(ACT_PARK), -1);
    CHECK_EQ(town_res(a)->stat[STAT_HAPPY], happy);
    CHECK_EQ(sim_news_count(), news_before); /* a false arrival made no news */
    CHECK_EQ(talk_outcome_meet(ACT_CAFE), a);
    CHECK(town_res(a)->stat[STAT_HAPPY] >= happy);
    CHECK(talk_mem_shared(a, TALK_SHARED_PLAYED));
    news_after = sim_news_count();
    CHECK(news_after > news_before);
    CHECK(sim_news_record(0) &&
          sim_news_record(0)->tmpl ==
          (sim_favourite_venue(a) == ACT_CAFE ? NT_TALK_MEET_FAV : NT_TALK_MEET));
    CHECK(strstr(sim_news(0), sim_favourite_venue(a) == ACT_CAFE ?
                 "favorite" : "met") != NULL);
    CHECK_EQ(talk_outcome_meet(ACT_CAFE), -1); /* one visit, not farmable */
    CHECK_EQ(sim_news_count(), news_after); /* cannot invent repeated headlines */
    doomo_town.day++;                         /* day rollover drops pending plans */
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), -1);
    talk_db_close();
}

/* The actual Free Play City Map, not only the Story-mode door, must let
 * the player meet a friend at the accepted destination. A real A press
 * selects the hub's guest, then a real A press chooses free chat. */
static void meetup_frames(uint32_t buttons, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        plat_begin_frame();
        doomo_frame(buttons);
        plat_end_frame();
    }
}
static void meetup_press(uint32_t b)
{
    meetup_frames(b, 1);
    meetup_frames(0, 1);
}

TEST(action_meetup_is_reachable_and_one_shot_from_freeplay_hub)
{
    TalkState s;
    TalkOutcome o;
    TalkSummary sum;
    int a, i, found = 0, tries;
    uint16_t cafe;
    char label[64];
    premade_town();
    CHECK(open_en());
    people();
    prime();
    CHECK(nadult > 0);
    a = adults[0];
    cafe = concept("brimstone bites");
    CHECK(cafe != 0);
    CHECK(try_invite(a, 12000u, 150, &s, &o, &sum, cafe));
    CHECK_EQ(o.invited, 1);
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), a);
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    meetup_frames(0, 5);
    fp_island_select_kind(LOC_CAFE);
    for (tries = 0; tries < 3 && !fp_island_menu_open(); tries++) {
        meetup_press(BTN_A);
        meetup_frames(0, 3);
    }
    CHECK(fp_island_menu_open());
    label[0] = 0;
    for (i = 0; i < fp_island_menu_count(); i++) {
        const char *txt = fp_island_menu_label(i);
        if (strncmp(txt, "Meet ", 5) == 0) {
            doomo_snprintf(label, sizeof(label), "%s", txt);
            found++;
        }
    }
    CHECK_EQ(found, 1);
    CHECK(fp_island_menu_select(label));
    meetup_press(BTN_A); /* opens the real modal choice */
    for (tries = 0; tries < 12 && talk_outcome_meet_ready(ACT_CAFE) == a; tries++) {
        meetup_frames(0, 15);
        meetup_press(BTN_A); /* first choice = free catch-up */
    }
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), -1);
    CHECK_EQ(talk_outcome_meet_plan(a), -1);
    CHECK(talk_mem_shared(a, TALK_SHARED_PLAYED));
    talk_db_close();
}

/* Favorites, moods and identity affect the *same* reaction in the
 * Story place menu and the Free Play City Map. Reading is non-mutating. */
TEST(action_meetup_reactions_reflect_real_guest_traits)
{
    int a, venue;
    const char *reply;
    premade_town();
    people();
    CHECK(nadult > 0);
    a = adults[0];
    venue = sim_favourite_venue(a);
    town_res(a)->mood = MOOD_HAPPY;
    reply = talk_outcome_meet_reply(a, venue, TALK_MEET_ACTIVITY);
    CHECK(strstr(reply, "favorite place") != NULL);
    town_res(a)->mood = MOOD_SAD;
    reply = talk_outcome_meet_reply(a, venue, TALK_MEET_CHAT);
    CHECK(strstr(reply, "staying with me") != NULL);
    CHECK_EQ(talk_outcome_meet_plan(a), -1);
}

TEST(action_meetup_activity_is_real_and_cannot_be_farmed)
{
    TalkState s;
    TalkOutcome o;
    TalkSummary sum;
    uint16_t cafe;
    int a, initial, before, av;
    premade_town();
    CHECK(open_en());
    people();
    prime();
    CHECK(nadult > 0);
    a = adults[0];
    cafe = concept("brimstone bites");
    CHECK(cafe && talk_act_venue_of(cafe) == ACT_CAFE);
    CHECK(try_invite(a, 12000u, 150, &s, &o, &sum, cafe));
    CHECK_EQ(o.invited, 1);
    av = playas_slot();
    town_res(a)->stat[STAT_HUNGER] = 24;
    if (town_res(av)) town_res(av)->stat[STAT_HUNGER] = 40;
    doomo_town.money = 0;
    CHECK_EQ(talk_outcome_meet_kind(ACT_CAFE, TALK_MEET_ACTIVITY), -1);
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), a); /* no money spent, still invited */
    doomo_town.money = 20;
    initial = doomo_town.money;
    before = town_res(a)->stat[STAT_HUNGER];
    CHECK_EQ(talk_outcome_meet_kind(ACT_CAFE, TALK_MEET_ACTIVITY), a);
    CHECK_EQ(doomo_town.money, initial - 6);
    CHECK(town_res(a)->stat[STAT_HUNGER] > before);
    CHECK(talk_mem_shared(a, TALK_SHARED_PLAYED));
    CHECK_EQ(talk_outcome_meet_kind(ACT_CAFE, TALK_MEET_ACTIVITY), -1);
    CHECK_EQ(doomo_town.money, initial - 6);
    CHECK_EQ(talk_outcome_meet_ready(ACT_CAFE), -1);
    CHECK_EQ(talk_outcome_meet_kind(ACT_CAFE, 99), -1);
    talk_db_close();
}

TEST(action_sim_effects)
{
    TalkState s;
    TalkOutcome out;
    TalkSummary sum;
    int a, gifted = 0, asked = 0, k, ids[1];
    uint16_t beach, workshop;
    premade_town();
    CHECK(open_en());
    people();
    prime();
    beach = concept("beach");
    workshop = concept("workshop");
    CHECK(beach && workshop);
    /* an accepted invitation: the hook runs once and the activity is the venue's */
    a = adults[0];
    town_res(a)->activity = ACT_HOME;
    CHECK(try_invite(a, 100u, 60, &s, &out, &sum, workshop));
    CHECK_EQ((int)out.invited, 1);
    CHECK_EQ((int)out.skipped, 0);
    CHECK_EQ((int)town_res(a)->activity, (int)ACT_WORKSHOP);
    for (k = 0; k < sum.nev; k++) if (sum.ev[k].kind == TEV_ACT_INVITE) { CHECK_EQ((int)sum.ev[k].b, (int)ACT_WORKSHOP); CHECK_EQ((int)sum.ev[k].a, 0); }
    /* the same hook on its own: a refusal changes nothing, an accepted nudge sets exactly the activity */
    CHECK(!sim_nudge_activity(a, ACT_PARK, 0));
    CHECK_EQ((int)town_res(a)->activity, (int)ACT_WORKSHOP);
    CHECK(sim_nudge_activity(a, ACT_PARK, 60));
    CHECK_EQ((int)town_res(a)->activity, (int)ACT_PARK);
    /* a gift moves exactly one item into the inventory, from a best friend in a good mood */
    for (k = 0; k < 400 && !gifted; k++) {
        int before = items_total();
        Tally t;
        ids[0] = adults[1 + 3 * (k % 6)];
        memset(&t, 0, sizeof t);
        talk_outcome_reset();
        run_session(30000u + (uint32_t)k, DOOMO_PLAYER_ID, ids, 1, 6, NULL, 0, &t, &out, &sum, NULL);
        if (t.line[TACT_GIFT]) {
            CHECK_EQ((int)out.gifted, 1);
            CHECK_EQ(items_total(), before + 1);
            gifted = 1;
        } else {
            CHECK_EQ(items_total(), before);
        }
    }
    CHECK(gifted);
    /* an ask raises one PROB_WANTS_ITEM, and only for a resident with no open problem */
    for (k = 0; k < 600 && !asked; k++) {
        int before = sim_problem_count(), id = adults[3 * (k % 6)], pi;
        Tally t;
        ids[0] = id;
        while (sim_problem_for(id) >= 0) sim_problem_resolve(sim_problem_for(id));
        res_gset(town_res(id), GF_LIKE_FOOD, 0);                    /* a cupcake: a food that is an item */
        res_gset(town_res(id), GF_HATE_FOOD, 5);
        memset(&t, 0, sizeof t);
        talk_outcome_reset();
        before = sim_problem_count();
        run_session(40000u + (uint32_t)k, DOOMO_PLAYER_ID, ids, 1, 6, NULL, 0, &t, &out, &sum, NULL);
        if (t.line[TACT_ASKITEM]) {
            pi = sim_problem_for(id);
            CHECK_EQ((int)out.asked, 1);
            CHECK_EQ(sim_problem_count(), before + 1);
            CHECK(pi >= 0 && sim_problem(pi)->kind == PROB_WANTS_ITEM);
            asked = 1;
        }
    }
    CHECK(asked);
    {   /* with an open problem: never asked, and an event applied by hand is refused */
        int id = adults[3], n0, applied = 0;
        TalkSummary g;
        TalkOutcome o;
        ids[0] = id;
        sim_problem_raise(id, PROB_BORED, -1, 0);
        res_gset(town_res(id), GF_LIKE_FOOD, 0);
        res_gset(town_res(id), GF_HATE_FOOD, 5);
        n0 = sim_problem_count();
        for (k = 0; k < 150; k++) {
            Tally t;
            memset(&t, 0, sizeof t);
            talk_outcome_reset();
            if (sim_problem_for(id) < 0) sim_problem_raise(id, PROB_BORED, -1, 0);   /* talked through earlier: open again */
            n0 = sim_problem_count();
            run_session(50000u + (uint32_t)k, DOOMO_PLAYER_ID, ids, 1, 6, NULL, 0, &t, &out, &sum, NULL);
            applied += t.line[TACT_ASKITEM];
            CHECK(sim_problem_count() <= n0);
        }
        CHECK_EQ(applied, 0);
        if (sim_problem_for(id) < 0) sim_problem_raise(id, PROB_BORED, -1, 0);
        n0 = sim_problem_count();
        memset(&g, 0, sizeof g);
        g.avatar = DOOMO_PLAYER_ID; g.n = 1; g.id[0] = (int16_t)id;
        g.nev = 1; g.ev[0].kind = TEV_ACT_ASK; g.ev[0].b = ITEM_CUPCAKE;
        talk_outcome_reset();
        memset(&o, 0, sizeof o);
        talk_outcome_apply(&g, &o);
        CHECK_EQ((int)o.asked, 0);
        CHECK_EQ((int)o.skipped, 1);
        CHECK_EQ(sim_problem_count(), n0);
    }
    {   /* a call-over between strangers: the apply introduces them (sim_introduce) */
        int x = adults[56], y = adults[57];
        TalkSummary g;
        TalkOutcome o;
        for (k = 0; k < 40 && rel_kind(x, y) >= 0; k++) { x = adults[(56 + k) % nadult]; y = adults[(57 + 2 * k) % nadult]; if (x == y) y = adults[(58 + k) % nadult]; }
        CHECK(x != y && rel_kind(x, y) < 0);
        memset(&g, 0, sizeof g);
        g.avatar = DOOMO_PLAYER_ID; g.n = 1; g.id[0] = (int16_t)x; g.call_to = (uint16_t)(y + 1);
        g.nev = 1; g.ev[0].kind = TEV_ACT_CALL; g.ev[0].a = 0;
        memset(&o, 0, sizeof o);
        talk_outcome_apply(&g, &o);
        CHECK_EQ((int)o.called, 1);
        CHECK(rel_kind(x, y) >= 0);
    }
    talk_db_close();
}

TEST(action_sim_nudge)
{
    int id = -1, k, hell = -1;
    premade_town();
    people();
    CHECK(nadult >= 5);
    id = adults[0];
    CHECK(sim_nudge_ok(id, ACT_PARK));
    CHECK(!sim_nudge_ok(id, ACT_HOME) && !sim_nudge_ok(id, ACT_SLEEPING) && !sim_nudge_ok(id, ACT_HOSPITAL) && !sim_nudge_ok(id, ACT_COUNT));
    CHECK(!sim_nudge_ok(-1, ACT_PARK) && !sim_nudge_ok(DOOMO_MAX_RESIDENTS, ACT_PARK));
    town_res(id)->activity = ACT_SLEEPING;
    CHECK(!sim_nudge_ok(id, ACT_PARK) && !sim_nudge_activity(id, ACT_PARK, 60));
    CHECK_EQ((int)town_res(id)->activity, (int)ACT_SLEEPING);
    town_res(id)->activity = ACT_HOME;
    town_res(id)->status = RES_GHOST;
    CHECK(!sim_nudge_ok(id, ACT_PARK));
    town_res(id)->status = RES_POSSESSED;
    CHECK(!sim_nudge_ok(id, ACT_PARK));
    town_res(id)->status = RES_ALIVE;
    CHECK(sim_nudge_ok(id, ACT_CAFE));
    doomo_town.minute = 23 * 60;                                   /* at night only the cafe, the concert hall, the church, the vape shop */
    CHECK(!sim_nudge_ok(id, ACT_PARK) && !sim_nudge_ok(id, ACT_BEACH) && !sim_nudge_ok(id, ACT_SHOP) && sim_nudge_ok(id, ACT_CAFE));
    doomo_town.minute = 12 * 60;
    for (k = 0; k < DOOMO_MAX_RESIDENTS && hell < 0; k++) if (!RES_DISTRICT(k) && !(town_res(k)->flags & RF_HELLBORN) && town_res(k)->status == RES_ALIVE) hell = k;
    if (hell >= 0 && doomo_town.mode == TOWN_STORY) {
        doomo_town.story.chapter = 0;
        town_res(hell)->activity = ACT_HOME;
        CHECK(!sim_nudge_ok(hell, ACT_VAPE_SHOP));
        doomo_town.story.chapter = 3;
        CHECK(sim_nudge_ok(hell, ACT_VAPE_SHOP));
    }
    CHECK(sim_can_raise_problem(id));
    sim_problem_raise(id, PROB_BORED, -1, 0);
    CHECK(!sim_can_raise_problem(id));
    CHECK(sim_favourite_venue(id) >= 0 && sim_favourite_venue(id) < ACT_COUNT);
}

/* ---- P2 / P3 / P7 with traits and actions on ----------------------------------------------------------------- */
static int romance_edges(void)
{
    int a, n = 0;
    for (a = 0; a < DOOMO_MAX_RESIDENTS; a++) {
        int b;
        for (b = a + 1; b < DOOMO_MAX_RESIDENTS; b++) {
            int k = rel_kind(a, b);
            if (k == REL_DATING || k == REL_MARRIED) n++;
        }
    }
    return n;
}

/* review: no gift, ask or call-over after a hostile or a vulnerable player act */
TEST(action_not_after_hostile)
{
    int it, total = 0;
    premade_town();
    CHECK(open_en());
    people();
    prime();
    g_cool = 1;
    for (it = 0; it < 400; it++) {
        int ids[2];
        TalkState s;
        TalkOutcome out;
        TalkSummary sum;
        Tally t;
        ids[0] = adults[(it * 5) % nadult];
        ids[1] = adults[(it * 5 + 2) % nadult];
        memset(&t, 0, sizeof t);
        talk_outcome_reset();
        run_session(70000u + (uint32_t)it, DOOMO_PLAYER_ID, ids, 1 + it % 2, 6, NULL, 0, &t, &out, &sum, &s);
        total++;
        CHECK_EQ(t.line[TACT_GIFT] + t.line[TACT_ASKITEM] + t.line[TACT_CALL], 0);
    }
    g_cool = 0;
    CHECK(total > 0);
    talk_db_close();
}

TEST(action_p2_p3_p7_random)
{
    static const char *const keys[] = { "park", "beach", "shop", "workshop", "brimstone bites", "st cuddles", "concert hall", "cloud nine vapor", "pie" };
    uint16_t topics[9];
    int nt = 0, k, it, edges, nact = 0, kid_sessions = 0;
    premade_town();
    CHECK(open_en());
    people();
    prime();
    CHECK(nkid >= 1 && nteen >= 1);
    for (k = 0; k < 9; k++) { uint16_t c = concept(keys[k]); if (c) topics[nt++] = c; }
    edges = romance_edges();
    for (it = 0; it < 1500; it++) {
        int ids[3], n = 1 + it % 3, minor_room = 0, j, avatar = DOOMO_PLAYER_ID;
        TalkState s;
        TalkOutcome out;
        TalkSummary sum;
        Tally t;
        ids[0] = adults[(it * 5) % nadult];
        ids[1] = it % 3 == 1 ? kids[it % nkid] : it % 3 == 2 ? teens[it % nteen] : adults[(it * 5 + 2) % nadult];
        ids[2] = adults[(it * 5 + 9) % nadult];
        if (it % 11 == 0) avatar = teens[(it / 11) % nteen];
        for (j = 0; j < n; j++) {
            if (ids[j] == avatar) ids[j] = adults[(it * 5 + 21 + j) % nadult];
            if (talk_safe_minor(ids[j])) minor_room = 1;
        }
        if (talk_safe_minor(avatar)) minor_room = 1;
        if (n == 3 && ids[1] == ids[2]) n = 2;
        memset(&t, 0, sizeof t);
        talk_outcome_reset();
        run_session(60000u + (uint32_t)it, avatar, ids, n, 6, topics, nt, &t, &out, &sum, &s);
        kid_sessions += minor_room;
        CHECK_EQ((int)out.skipped, 0);
        for (j = 0; j < sum.nev; j++) {
            const TalkEvent *e = &sum.ev[j];
            if (minor_room) CHECK(e->kind != TEV_ROMANCE_YES);
            if (e->kind == TEV_ACT_INVITE) {
                nact++;
                CHECK(talk_act_invite_ok(TA_INVITE, e->b, minor_room, 12));
                CHECK(!minor_room || e->b == ACT_PARK || e->b == ACT_CAFE || e->b == ACT_CHURCH || e->b == ACT_BEACH);
            }
            if (e->kind == TEV_ACT_GIFT || e->kind == TEV_ACT_ASK) {
                nact++;
                CHECK(talk_act_gift_ok(e->b, 0, minor_room));
                CHECK(!minor_room || !talk_act_item_adult(e->b));
            }
            if (e->kind == TEV_ACT_CALL) nact++;
        }
        for (j = 0; j < s.nlines; j++) (void)j;
    }
    CHECK_EQ(romance_edges(), edges);                           /* P7: no couple made */
    CHECK_EQ(sim_romance_sweep(-1), 0);
    printf("      1,500 random sessions (%d with a minor in the room): %d actions, no couple made\n", kid_sessions, nact);
    CHECK(nact >= 100);
    talk_db_close();
}
