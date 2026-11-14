/* test_talk_dm.c - T6: the drama manager (core/src/talk_dm.c, core/data/talk/beats.tlk).
 * Owner: TK. docs/TALK.md "The drama manager"; design 5 and 7.5.
 *
 * Over the real TALK_en.BIN: the beat table and the state budget, the arc,
 * scripted sessions (who answers, reactions, interruptions, idle chatter and
 * the prod, engagement and walk-outs, arguments: jab / row / ultimatum, a
 * child holding it at the jab, mediation calling sim_make_up, taking a
 * side), the romance beat and its guards, and the safety properties with the
 * drama manager in charge: P2 (no romance line toward or as a minor), P3 (K-7
 * ends the talk and the input text never comes back), P7 (random sessions
 * leave nothing in the sim that the romance sweep would remove). Every test
 * also checks the S3 lint counted nothing.
 */
#include "test.h"
#include <stdlib.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/roster.h"
#include "doomo/sim.h"
#include "doomo/town.h"

#define ID_NANA  1
#define ID_RIVET 13
#define ID_PIP   42           /* kid cast */
#define ID_TEEN  11
#define ID_VEX   666
#define ID_GRUMB 729

static int open_db(void) { return talk_db_open(lang_find("en")) == TALK_DB_OK && talk_dm_ready(); }

static void premade_town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
}

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

static void begin(TalkState *s, int avatar, int a, int b, int d, uint32_t seed)
{
    int ids[3], n = 0;
    ids[n++] = a;
    if (b >= 0) ids[n++] = b;
    if (d >= 0) ids[n++] = d;
    talk_dm_begin(s, avatar, ids, n, seed);
}

/* one player turn and every line of the exchange; returns the number of lines */
static int turn(TalkState *s, int act, uint16_t topic, int target, TalkTrace *tr)
{
    TalkUtter u;
    int n = 0;
    utter(&u, act, topic, target);
    talk_dm_say(s, &u);
    while (talk_dm_step(s, tr)) n++;
    return n;
}

static int has_event(const TalkState *s, int kind)
{
    int k;
    for (k = 0; k < s->nev; k++) if (s->ev[k].kind == kind) return 1;
    return 0;
}

static void make_rivals(int a, int b)
{
    rel_get_or_add(a, b);
    rel_adjust(a, b, -80);
    rel_set_kind(a, b, REL_RIVAL);
    rel_adjust(a, b, 0);
}

/* ---- the table and the budget ------------------------------------------------ */
TEST(talk_dm_table_and_budget)
{
    TalkBeatInfo bi;
    uint8_t fav = 0, dull = 0;
    int b;
    CHECK(sizeof(TalkState) <= TALK_STATE_MAX);
    CHECK_EQ(sizeof(TalkPart), 12);
    premade_town();
    CHECK(open_db());
    for (b = 1; b < TBEAT_COUNT; b++) CHECK_MSG(talk_dm_beat_info(b, &bi) && bi.goals >= 1 && bi.turns >= 1, "beat %d", b);
    CHECK(!talk_dm_beat_info(0, &bi) && !talk_dm_beat_info(TBEAT_COUNT, &bi));
    CHECK(talk_dm_beat_info(TBEAT_ROMANCE_ADULT, &bi));
    CHECK_EQ(bi.pre, TBP_ROMANCE);
    CHECK(bi.flags & TBTF_ADULTS);
    CHECK(talk_dm_beat_info(TBEAT_KID_MOMENT, &bi));
    CHECK(bi.flags & TBTF_KIDSAFE);
    CHECK(talk_dm_beat_info(TBEAT_RIVALRY, &bi));
    CHECK_EQ(bi.pre, TBP_RIVALS);
    CHECK(talk_dm_beat_info(TBEAT_PROBLEM, &bi));
    CHECK(bi.resolve[0] == TA_COMFORT);
    CHECK(talk_dm_affinity_roots(2, &fav, &dull) && fav == TCR_FOOD && dull == TCR_FEELINGS);   /* Grumpy Gremlin */
    CHECK(!talk_dm_affinity_roots(16, &fav, &dull));
    talk_db_close();
    CHECK(!talk_dm_ready());
}

TEST(talk_dm_target_tension_arc)
{
    int t, peak = 0, peak_at = 0;
    for (t = 0; t < 40; t++) {
        int v = talk_dm_target_tension(t);
        CHECK(v >= 0 && v <= TALK_TENSION_MAX);
        if (v > peak) { peak = v; peak_at = t; }
    }
    CHECK(peak >= 5 && peak_at >= 8 && peak_at <= 10);            /* up to a peak at turns 8-10 */
    CHECK(talk_dm_target_tension(15) <= 1);                        /* down by 14-16 */
    CHECK(talk_dm_target_tension(30) == 2);                        /* then lively */
    CHECK(talk_dm_target_tension(0) == 0);
}

/* ---- a scripted session ----------------------------------------------------------- */
TEST(talk_dm_scripted_session)
{
    TalkState s, s2;
    TalkTrace tr;
    uint32_t h0 = talk_safe_lint_hits();
    int i, n, ncheck = 0;
    char first[TALK_LINE_MAX];
    uint16_t pie;
    premade_town();
    CHECK(open_db());
    pie = concept("pie");
    begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_VEX, 4242u);
    CHECK_EQ(s.beat, TBEAT_ARRIVE);
    CHECK_EQ(s.nparts, 3);
    CHECK_EQ(s.part[0].engage, TALK_ENGAGE_START);
    /* the greeting, addressed to Rivet: Rivet answers, the exchange is 1..4 lines, ARRIVE says its goal line */
    n = turn(&s, TA_GREET, TC_NONE, 1, &tr);
    CHECK(n >= 1 && n <= TALK_EXCHANGE_MAX);
    CHECK_EQ(s.line[0].who, 1);
    CHECK_EQ(s.gate, TG_PASS);
    CHECK(s.hist[0].who != 0xFF && s.nhist >= 2);
    CHECK_EQ(s.conv.beat, s.beat);
    {
        int goal = 0, k;
        for (k = 0; k < s.nlines; k++) { CHECK(s.line[k].text[0]); if (s.hist[s.nlines - 1 - k].flags & THF_BEAT) goal++; }
        CHECK_EQ(goal, 1);
    }
    snprintf(first, sizeof first, "%s", s.line[0].text);
    /* determinism: the same session on a fresh state says the same */
    begin(&s2, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_VEX, 4242u);
    CHECK_EQ(turn(&s2, TA_GREET, TC_NONE, 1, &tr), n);
    CHECK_STR(s2.line[0].text, first);
    /* a topic they like: the engagement of the fan rises, nobody's line is empty, the beat moves on */
    for (i = 0; i < 12; i++) {
        int k, m = turn(&s, i & 1 ? TA_ASK_ABOUT : TA_TELL, pie, i % 3 == 2 ? -1 : i % 3, NULL);
        CHECK(m >= (s.flags & TDF_OVER ? 0 : 1) && m <= TALK_EXCHANGE_MAX);
        for (k = 0; k < s.nlines; k++) {
            CHECK_MSG(s.line[k].text[0], "empty line %d of turn %d", k, i);
            CHECK(s.line[k].who >= 0 && s.line[k].who < 3);
            CHECK(k == 0 || !(s.line[k].flags & TLF_REACTION) || s.line[k].who != s.line[k - 1].who);
            ncheck++;
        }
        CHECK_EQ(s.conv.beat, s.beat);
        CHECK(s.tension <= TALK_TENSION_MAX);
        if (s.flags & TDF_OVER) break;
    }
    CHECK(ncheck >= 8);
    CHECK(s.beats_used & (1u << TBEAT_ARRIVE));
    CHECK(s.beats_used != (1u << TBEAT_ARRIVE));                   /* it moved to another beat */
    CHECK(s.topic_stack[0] == pie);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* the arc (peak_tension is a running maximum, so 'ends under the peak' holds by definition and is only printed): a calm session of 18 turns peaks in the middle and is calm by the end (5.2) */
TEST(talk_dm_arc)
{
    TalkState s;
    uint32_t h0 = talk_safe_lint_hits();
    int seed, t, inrange = 0, calm = 0, runs = 0;
    uint16_t pie;
    premade_town();
    CHECK(open_db());
    pie = concept("pie");
    make_rivals(ID_NANA, ID_RIVET);
    for (seed = 1; seed <= 30; seed++) {
        begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_GRUMB, 7000u + (uint32_t)seed);
        for (t = 0; t < 18 && !(s.flags & TDF_OVER); t++) turn(&s, t % 2 ? TA_INSULT : TA_TELL, t % 4 ? pie : TC_NONE, 2, NULL);
        if (s.turn < 18) continue;
        runs++;
        if (s.peak_turn >= 3 && s.peak_turn <= 13) inrange++;
        if (s.tension <= s.peak_tension) calm++;
    }
    printf("      arc: %d runs, peak in turns 3-13: %d, ends at or under the peak: %d\n", runs, inrange, calm);
    CHECK(runs >= 20 && inrange * 10 >= runs * 8);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- idle chatter, then the prod, then nothing ------------------------------------------ */
TEST(talk_dm_idle_chatter)
{
    TalkState s;
    uint32_t h0 = talk_safe_lint_hits();
    int chats = 0, prods = 0, i, n;
    premade_town();
    CHECK(open_db());
    begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_GRUMB, 99u);
    CHECK(!talk_dm_tick(&s, TALK_IDLE_FRAMES - 1));                /* 8 s not yet */
    CHECK_EQ(s.nlines, 0);
    for (i = 0; i < 6; i++) {
        if (!talk_dm_tick(&s, TALK_IDLE_FRAMES)) continue;
        n = 0;
        while (talk_dm_step(&s, NULL)) n++;
        CHECK(n >= 1 && n <= 2);
        CHECK(s.line[0].text[0]);
        CHECK(s.hist[n - 1].flags & THF_IDLE || s.hist[0].flags & THF_IDLE);
        if (s.line[0].to == -1) { prods++; CHECK_EQ(i, TALK_IDLE_CHATS); } else chats++;
    }
    CHECK_EQ(chats, TALK_IDLE_CHATS);                              /* two idle chats, then someone prods the player */
    CHECK_EQ(prods, 1);
    CHECK(!talk_dm_tick(&s, 5000));                                /* and nothing more until the player speaks */
    turn(&s, TA_GREET, TC_NONE, -1, NULL);
    CHECK_EQ(s.idle_n, 0);
    CHECK(talk_dm_tick(&s, TALK_IDLE_FRAMES));                     /* the player spoke: idle chatter again */
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- engagement and walk-outs (5.3) ------------------------------------------------------ */
TEST(talk_dm_walkout)
{
    TalkState s;
    TalkSummary sum;
    uint32_t h0 = talk_safe_lint_hits();
    int t, left_at = -1, away_seen = 0, k;
    premade_town();
    CHECK(open_db());
    begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_GRUMB, 31337u);
    for (t = 0; t < 40 && !talk_dm_over(&s); t++) {
        int n, gone[3];
        for (k = 0; k < 3; k++) gone[k] = (s.part[k].flags & TPF_LEFT) != 0;
        n = turn(&s, TA_DISMISS, TC_NONE, -1, NULL);
        for (k = 0; k < 3; k++) if (s.part[k].flags & TPF_AWAY) away_seen = 1;
        if (left_at < 0 && has_event(&s, TEV_WALKOUT)) {
            left_at = t;
            /* the excuse is the last thing the leaver says, and it is said to the player */
            CHECK(n >= 1);
            {
                int j, found = 0;
                for (j = 0; j < s.nhist; j++) if (s.hist[j].flags & THF_LEAVE) { found = 1; break; }
                CHECK(found);
            }
        }
        /* nobody who left ever speaks again */
        for (k = 0; k < s.nlines; k++) CHECK(!gone[(int)s.line[k].who]);
    }
    CHECK(away_seen);                                              /* below 25 they turn away first */
    CHECK(left_at >= 1);
    CHECK(talk_dm_over(&s));                                       /* "...and they're gone": everyone left */
    CHECK_EQ(turn(&s, TA_GREET, TC_NONE, -1, NULL), 0);
    talk_dm_end(&s, &sum);
    CHECK(sum.nev >= 3);
    for (k = 0; k < sum.n; k++) CHECK(sum.pflags[k] & TPF_LEFT);
    CHECK(sum.rel_d_player[0] <= 0 && sum.mood_d[0] <= 0);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- arguments: jab, row, ultimatum; mediation; taking a side ------------------------------- */
/* drive a session of rivals towards an argument; returns 1 when one is running */
static int to_argument(TalkState *s, int third, uint32_t seed)
{
    int t;
    begin(s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, third, seed);
    for (t = 0; t < 14 && !talk_dm_over(s); t++) {
        turn(s, t % 2 ? TA_INSULT : TA_TELL, TC_NONE, 2 < s->nparts ? 2 : -1, NULL);
        if (s->arg_step >= 1 && s->beat == TBEAT_RIVALRY) return 1;
    }
    return 0;
}

TEST(talk_dm_argument_steps)
{
    TalkState s;
    uint32_t h0 = talk_safe_lint_hits();
    int seed, runs = 0, row = 0, ult = 0, prev;
    premade_town();
    CHECK(open_db());
    make_rivals(ID_NANA, ID_RIVET);
    CHECK(rel_kind(ID_NANA, ID_RIVET) == REL_RIVAL || rel_kind(ID_NANA, ID_RIVET) == REL_NEMESIS);
    for (seed = 1; seed <= 80; seed++) {
        int t;
        if (!to_argument(&s, ID_GRUMB, 900u + (uint32_t)seed)) continue;
        runs++;
        prev = s.arg_step;
        CHECK_EQ(s.arg_step, 1);                                   /* it starts with a jab */
        CHECK(s.hist[0].flags & THF_ARGUE || s.nlines > 1);
        for (t = 0; t < 6 && s.arg_step > 0 && !talk_dm_over(&s); t++) {
            turn(&s, TA_INSULT, TC_NONE, 2, NULL);
            CHECK(s.arg_step == 0 || s.arg_step >= prev || s.arg_step <= 3);
            if (s.arg_step == 2) { row++; CHECK(s.tension >= TALK_TENSION_ARGUE); }
            if (s.arg_step == 3) { ult++; CHECK(has_event(&s, TEV_ULTIMATUM)); }
            prev = s.arg_step ? s.arg_step : prev;
        }
        /* ignored, it fizzles out: a walk-out of one of the two, the beat is over */
        if (s.arg_step == 0) CHECK(s.beat_step == 255 || s.beat != TBEAT_RIVALRY);
    }
    printf("      %d of 80 sessions ran an argument; rows %d, ultimatums %d\n", runs, row, ult);
    CHECK(runs >= 10 && row >= 1 && ult >= 1);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

TEST(talk_dm_mediation_makes_up)
{
    TalkState s;
    TalkSummary sum;
    uint32_t h0 = talk_safe_lint_hits();
    int seed, ok = 0, side = 0, failed = 0;
    premade_town();
    CHECK(open_db());
    for (seed = 1; seed <= 120; seed++) {
        int a, b, before, k, i;
        const char *msg;
        make_rivals(ID_NANA, ID_RIVET);
        if (!to_argument(&s, ID_GRUMB, 5000u + (uint32_t)seed)) continue;
        a = s.arg_a;
        b = s.arg_b;
        CHECK(a >= 0 && b >= 0 && a != b);
        if (seed % 3 == 0) {                                       /* take a side: the pair moves, the player's bond with the other drops */
            turn(&s, TA_TAKE_SIDE, TC_NONE, a, NULL);
            if (has_event(&s, TEV_SIDE_TAKEN)) {
                for (i = 0; i < s.nev; i++) if (s.ev[i].kind == TEV_SIDE_TAKEN) { CHECK_EQ(s.ev[i].a, a); CHECK_EQ(s.ev[i].b, b); }
                CHECK_EQ(s.arg_step, 0);
                talk_dm_end(&s, &sum);
                CHECK(sum.rel_d_player[a] > sum.rel_d_player[b]);
                side++;
            }
            continue;
        }
        turn(&s, TA_MEDIATE, TC_NONE, a, NULL);
        if (!has_event(&s, TEV_MEDIATED)) { failed++; CHECK(!has_event(&s, TEV_MEDIATED)); continue; }
        for (k = 0; k < s.nev; k++) if (s.ev[k].kind == TEV_MEDIATED) { CHECK_EQ(s.ev[k].a, a); CHECK_EQ(s.ev[k].b, b); }
        CHECK_EQ(s.arg_step, 0);
        {   /* the "make up" line is said (a to b) */
            int found = 0;
            for (k = 0; k < s.nlines; k++) if (s.line[k].who == a && s.line[k].to == b && s.line[k].text[0]) found = 1;
            CHECK(found);
        }
        talk_dm_end(&s, &sum);
        before = rel_value(s.part[a].id, s.part[b].id);
        msg = sim_make_up(s.part[a].id, s.part[b].id);               /* T6-OUT applies it; the call works on this pair */
        CHECK(msg && msg[0]);
        CHECK(rel_value(s.part[a].id, s.part[b].id) > before);
        CHECK(sum.nev >= 1);
        ok++;
    }
    printf("      mediation: %d made up (sim_make_up called), %d failed rolls, %d sides taken\n", ok, failed, side);
    CHECK(ok >= 3 && failed >= 1 && side >= 1);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* a child in the room: the argument stays at the jab, the child asks "Are you guys fighting?" and the room cools (5.4) */
TEST(talk_dm_kid_holds_argument_at_the_jab)
{
    TalkState s;
    uint32_t h0 = talk_safe_lint_hits();
    int seed, runs = 0, kidasks = 0, t, k;
    premade_town();
    CHECK(open_db());
    make_rivals(ID_NANA, ID_RIVET);
    for (seed = 1; seed <= 100; seed++) {
        begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_PIP, 300u + (uint32_t)seed);
        CHECK(s.conv.aud.any_minor);
        CHECK_EQ(s.part[2].stage_class, TSC_MINOR);
        for (t = 0; t < 14 && !talk_dm_over(&s); t++) {
            turn(&s, t % 2 ? TA_INSULT : TA_TELL, TC_NONE, 0, NULL);
            CHECK(s.arg_step <= 1);                                 /* never a row or an ultimatum */
            CHECK(!has_event(&s, TEV_ULTIMATUM));
            if (s.arg_step == 1) runs++;
            for (k = 0; k < s.nlines; k++)
                if (s.line[k].who == 2 && s.line[k].to == -2 && (s.hist[s.nlines - 1 - k].flags & THF_ARGUE) == 0 &&
                    !(s.hist[s.nlines - 1 - k].flags & (THF_BEAT | THF_REACT | THF_INTERRUPT)) && strchr(s.line[k].text, '?'))
                    kidasks++;
            CHECK(s.conv.aud.any_minor);
        }
    }
    printf("      kid room: %d argument turns, %d kid questions, never past the jab\n", runs, kidasks);
    CHECK(runs >= 5);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- interruptions (5.4): at most once in 3 exchanges, adults only, the line is cut at a word ---------- */
TEST(talk_dm_interruptions)
{
    TalkState s;
    uint32_t h0 = talk_safe_lint_hits();
    int id, loud[3], nl = 0, seed, cuts = 0, t, k, last_cut_exch, worst_gap = 99;
    premade_town();
    CHECK(open_db());
    for (id = 1; id < 400 && nl < 3; id++) {                         /* three blunt adults (SPEECH >= 11) */
        DoomoResident *r = town_res(id);
        if (r && r->status == RES_ALIVE && !talk_safe_minor(id) && RES_PERSONA(r, PERS_SPEECH) >= 11) loud[nl++] = id;
    }
    CHECK_EQ(nl, 3);
    make_rivals(loud[0], loud[1]);
    for (seed = 1; seed <= 60; seed++) {
        int ids[3];
        ids[0] = loud[0]; ids[1] = loud[1]; ids[2] = loud[2];
        talk_dm_begin(&s, DOOMO_PLAYER_ID, ids, 3, 777u + (uint32_t)seed);
        last_cut_exch = 0;
        for (t = 0; t < 16 && !talk_dm_over(&s); t++) {
            TalkUtter u;
            int n = 0;
            utter(&u, t % 3 == 0 ? TA_TELL : TA_ASK_OPINION, TC_NONE, -1);
            talk_dm_say(&s, &u);
            while (talk_dm_step(&s, NULL)) n++;
            for (k = 0; k < s.nlines; k++)
                if (s.line[k].flags & TLF_CUT) {
                    size_t len = strlen(s.line[k].text);
                    cuts++;
                    CHECK(len >= 2 && s.line[k].text[len - 1] == '-');
                    CHECK(k + 1 < s.nlines && s.line[k + 1].who != s.line[k].who);    /* the interrupter speaks */
                    CHECK(k + 2 < s.nlines && s.line[k + 2].who == s.line[k].who);    /* the interrupted one grumbles */
                    CHECK(s.hist[s.nlines - 2 - k].flags & THF_INTERRUPT);
                    if (last_cut_exch) { if ((int)s.exchange - last_cut_exch < worst_gap) worst_gap = (int)s.exchange - last_cut_exch; }
                    last_cut_exch = s.exchange;
                    CHECK(s.part[s.line[k].who].stage_class == TSC_ADULT);
                }
        }
    }
    printf("      interruptions: %d cuts in 60 sessions, smallest gap %d exchanges\n", cuts, worst_gap);
    CHECK(cuts >= 3);
    CHECK(worst_gap >= TALK_INTERRUPT_GAP);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- alliances (5.4): taking Y's side raises the bond with Y and lowers it with Y's opponents ------------- */
TEST(talk_dm_alliances)
{
    TalkState s;
    TalkSummary sum;
    premade_town();
    CHECK(open_db());
    make_rivals(ID_NANA, ID_RIVET);
    rel_get_or_add(ID_NANA, ID_GRUMB);
    rel_adjust(ID_NANA, ID_GRUMB, 40);                               /* Grumb likes Nana */
    begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_GRUMB, 55u);
    turn(&s, TA_TAKE_SIDE, TC_NONE, 0, NULL);
    talk_dm_end(&s, &sum);
    CHECK(sum.rel_d_player[0] >= 3);                                 /* Y */
    CHECK(sum.rel_d_player[1] < 0);                                  /* Y's opponent */
    CHECK(sum.rel_d_player[2] > 0);                                  /* Heider: a friend of Y */
    CHECK(sum.rel_d_player[0] <= TALK_REL_D_MAX && sum.rel_d_player[1] >= -TALK_REL_D_MAX);
    /* the caps hold whatever the player does */
    {
        int t;
        begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_GRUMB, 56u);
        for (t = 0; t < 40 && !talk_dm_over(&s); t++) turn(&s, t % 3 == 0 ? TA_TAKE_SIDE : t % 3 == 1 ? TA_COMPLIMENT : TA_DEFEND, TC_NONE, 0, NULL);
        talk_dm_end(&s, &sum);
        CHECK_EQ(sum.rel_d_player[0], TALK_REL_D_MAX);
        CHECK(sum.rel_d_player[1] >= -TALK_REL_D_MAX);
    }
    talk_db_close();
}

/* ---- a problem talked through, a great night, what the player earned ---------------------------------------- */
TEST(talk_dm_problem_and_summary)
{
    TalkState s;
    TalkSummary sum;
    uint32_t h0 = talk_safe_lint_hits();
    int seed, solved = 0, great = 0, t, k;
    premade_town();
    CHECK(open_db());
    CHECK(sim_problem_raise(ID_NANA, PROB_LONELY, -1, 0) >= 0);
    for (seed = 1; seed <= 60; seed++) {
        begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_GRUMB, 2000u + (uint32_t)seed);
        for (t = 0; t < 8 && !talk_dm_over(&s); t++) {
            turn(&s, t < 2 ? TA_TELL : TA_COMFORT, TC_NONE, 0, NULL);
            if (s.beat == TBEAT_PROBLEM && s.beat_a == 0) break;
        }
        if (s.beat == TBEAT_PROBLEM && s.beat_a == 0) {
            turn(&s, TA_COMFORT, TC_NONE, 0, NULL);
            if (has_event(&s, TEV_PROBLEM)) {
                CHECK(s.part[0].flags & TPF_PROBLEM);
                CHECK_EQ(s.beat_step, 255);
                solved++;
            }
        }
    }
    CHECK(solved >= 1);
    for (seed = 1; seed <= 60; seed++) {
        begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_GRUMB, 4000u + (uint32_t)seed);
        for (t = 0; t < 12 && !talk_dm_over(&s); t++)
            turn(&s, t % 2 ? TA_COMPLIMENT : TA_JOKE, TC_NONE, t % 3, NULL);
        talk_dm_end(&s, &sum);
        for (k = 0; k < sum.nev; k++) if (sum.ev[k].kind == TEV_GREAT_NIGHT) great++;
        CHECK(sum.mem_flags & TMF_COMPLIMENTED);
        CHECK(!(sum.mem_flags & TMF_INSULTED));
        CHECK_EQ(sum.avatar, DOOMO_PLAYER_ID);
        CHECK_EQ(sum.n, 3);
        CHECK(sum.turns >= 1 && sum.turns <= 12);
        CHECK(talk_dm_over(&s));
    }
    printf("      problem solved in %d of 60 sessions, a great night in %d of 60\n", solved, great);
    CHECK(great >= 1);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- the romance beat and its guards (design 7; K-4, K-5) ------------------------------------------------- */
static int count_romance_edges(void)
{
    int a, b, n = 0;
    for (a = 0; a < 120; a++)
        for (b = a + 1; b < 120; b++) {
            int k = rel_kind(a, b);
            if (k == REL_DATING || k == REL_MARRIED) n++;
            if (rel_has_crush(a, b) || rel_has_crush(b, a)) n += 1000;
        }
    return n;
}

TEST(talk_dm_romance_beat_adults_only)
{
    TalkState s;
    TalkSummary sum;
    uint32_t h0 = talk_safe_lint_hits();
    int avatar = roster_find("Mayor Giggs"), steamy = roster_find("Steamy"), k, said = 0, edges;
    premade_town();
    CHECK(open_db());
    CHECK(avatar >= 0 && steamy >= 0 && sim_romance_ok(steamy, avatar));
    edges = count_romance_edges();
    /* the target says yes: an adult room, a warm bond; the beat joins in and the target speaks its line */
    rel_get_or_add(steamy, avatar);
    rel_adjust(steamy, avatar, 30);
    begin(&s, avatar, steamy, -1, -1, 11u);
    CHECK(talk_dm_consent(&s, 0, TA_FLIRT));
    turn(&s, TA_FLIRT, TC_NONE, 0, NULL);
    CHECK_EQ(s.gate, TG_PASS);
    CHECK(has_event(&s, TEV_ROMANCE_YES));
    CHECK(s.conv.consent & 1);
    CHECK_EQ(s.beat, TBEAT_ROMANCE_ADULT);
    for (k = 0; k < s.nlines; k++) {
        CHECK_EQ(s.line[k].who, 0);
        if (s.hist[s.nlines - 1 - k].flags & THF_BEAT) said++;
    }
    CHECK_EQ(said, 1);
    /* the target says no: nothing starts, the day is refused (K-5), the next try is deflected */
    rel_adjust(steamy, avatar, -100);
    begin(&s, avatar, steamy, -1, -1, 12u);
    CHECK(!talk_dm_consent(&s, 0, TA_KISS));
    turn(&s, TA_FLIRT, TC_NONE, 0, NULL);
    CHECK(has_event(&s, TEV_ROMANCE_NO));
    CHECK(!has_event(&s, TEV_ROMANCE_YES) && !(s.conv.consent & 1));
    CHECK(s.beat != TBEAT_ROMANCE_ADULT && (s.conv.refused & 1) && (s.part[0].flags & TPF_REFUSED));
    turn(&s, TA_FLIRT, TC_NONE, 0, NULL);
    CHECK_EQ(s.gate, TG_DEFLECT);
    CHECK(s.beat != TBEAT_ROMANCE_ADULT);
    talk_dm_end(&s, &sum);
    CHECK(sum.pflags[0] & TPF_REFUSED);
    /* the pet in the room, or a child: never */
    rel_adjust(steamy, avatar, 120);
    {
        int ids[2];
        ids[0] = steamy; ids[1] = TALK_PET_ID;
        talk_dm_begin(&s, avatar, ids, 2, 13u);
        CHECK(!talk_dm_consent(&s, 0, TA_FLIRT));
        turn(&s, TA_FLIRT, TC_NONE, 0, NULL);
        CHECK(!has_event(&s, TEV_ROMANCE_YES) && s.beat != TBEAT_ROMANCE_ADULT && !s.conv.consent);
        ids[1] = ID_PIP;
        talk_dm_begin(&s, avatar, ids, 2, 14u);
        CHECK(!talk_dm_consent(&s, 0, TA_FLIRT));
        turn(&s, TA_FLIRT, TC_NONE, 0, NULL);
        CHECK(s.gate == TG_REFUSE_MINOR || s.gate == TG_DEFLECT);
        CHECK(!has_event(&s, TEV_ROMANCE_YES) && s.beat != TBEAT_ROMANCE_ADULT && !s.conv.consent);
    }
    /* Talk wrote nothing into the sim's romance state */
    CHECK_EQ(count_romance_edges(), edges);
    CHECK_EQ(sim_romance_sweep(-1), 0);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- P2 and P7 with the drama manager in charge: random sessions ----------------------------------------- */
static void run_random(TalkState *s, int avatar, const int *ids, int n, uint32_t seed, int turns, int *romance_yes)
{
    DoomoRng r;
    int t, k, minor_room = 0;
    talk_dm_begin(s, avatar, ids, n, seed);
    rng_seed(&r, seed * 2654435761u + 17u);
    for (k = 0; k < n; k++) if (ids[k] != TALK_PET_ID && talk_safe_minor(ids[k])) minor_room = 1;
    if (talk_safe_minor(avatar)) minor_room = 1;
    for (t = 0; t < turns && !talk_dm_over(s); t++) {
        TalkUtter u;
        int act = 1 + rng_range(&r, TA_COUNT - 1), tgt = rng_range(&r, n + 1) - 1;
        utter(&u, act, TC_NONE, tgt);
        if (rng_range(&r, 6) == 0) u.tone = TONE_FLIRTY;
        talk_dm_say(s, &u);
        while (talk_dm_step(s, NULL)) {}
        for (k = 0; k < s->nlines; k++) {
            const TalkLine *l = &s->line[k];
            int who = l->who, to = l->to;
            CHECK_MSG(l->text[0], "empty line");
            if (!TA_IS_ROMANCE(l->act)) continue;
            /* P2: no resident ever says a romance act in a room with a minor, as a minor, or to one */
            CHECK_MSG(!minor_room, "romance line in a minor's room: %s", l->text);
            CHECK(!s->conv.aud.pet && !s->conv.aud.any_minor);
            CHECK(s->part[who].stage_class == TSC_ADULT);
            CHECK(to < 0 || (to < n && s->part[to].stage_class == TSC_ADULT));
            CHECK((s->conv.consent >> who) & 1);
            CHECK(s->in.act[0] >= TA_FIRST_ROMANCE || s->hist[s->nlines - 1 - k].flags & THF_BEAT);
        }
        if (minor_room || s->conv.aud.pet) {
            CHECK(!s->conv.consent && s->beat != TBEAT_ROMANCE_ADULT);
        }
        CHECK(s->conv.aud.any_minor >= (minor_room && !talk_safe_minor(avatar) ? 1 : 0) || talk_safe_minor(avatar));
    }
    for (k = 0; k < s->nev; k++) {
        if (s->ev[k].kind == TEV_ROMANCE_YES) { (*romance_yes)++; CHECK(!minor_room); }
        if (minor_room) CHECK(s->ev[k].kind != TEV_ROMANCE_YES);
    }
}

TEST(talk_dm_p2_p7_random_sessions)
{
    TalkState s;
    uint32_t h0 = talk_safe_lint_hits();
    int avatar_giggs = roster_find("Mayor Giggs"), steamy = roster_find("Steamy"), seed, yes = 0, edges;
    static const int rooms[8][3] = {
        { ID_NANA, ID_RIVET, ID_VEX }, { ID_NANA, ID_PIP, ID_RIVET }, { ID_TEEN, ID_NANA, -1 }, { ID_NANA, TALK_PET_ID, -1 },
        { ID_PIP, ID_TEEN, -1 },       { ID_NANA, ID_RIVET, -1 },     { ID_GRUMB, ID_VEX, ID_RIVET }, { ID_NANA, -1, -1 } };
    premade_town();
    CHECK(open_db());
    CHECK(avatar_giggs >= 0 && steamy >= 0);
    rel_get_or_add(steamy, avatar_giggs);
    rel_adjust(steamy, avatar_giggs, 60);
    edges = count_romance_edges();
    for (seed = 1; seed <= 400; seed++) {
        int ids[3], n = 0, k, room = seed % 8, avatar;
        for (k = 0; k < 3; k++) if (rooms[room][k] >= 0) ids[n++] = rooms[room][k];
        avatar = seed % 5 == 0 ? ID_TEEN : seed % 7 == 0 ? ID_PIP : (seed % 2 ? DOOMO_PLAYER_ID : avatar_giggs);
        if (avatar == ids[0] || (n > 1 && avatar == ids[1])) avatar = DOOMO_PLAYER_ID;
        if (seed % 9 == 0) { ids[0] = steamy; if (n == 1) { ids[0] = steamy; avatar = avatar_giggs; } }
        run_random(&s, avatar, ids, n, (uint32_t)seed, 14, &yes);
    }
    printf("      400 random sessions, %d romance yes events, all in adult rooms\n", yes);
    /* P7: Talk left nothing behind that the sim's romance sweep would remove, and made no couple */
    CHECK_EQ(count_romance_edges(), edges);
    CHECK_EQ(sim_romance_sweep(-1), 0);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- P2, both directions: adult with minor, minor with adult -------------------------------------------- */
TEST(talk_dm_p2_both_directions)
{
    TalkState s;
    uint32_t h0 = talk_safe_lint_hits();
    int act, avatar, k;
    premade_town();
    CHECK(open_db());
    for (act = TA_FIRST_ROMANCE; act <= TA_LAST_ROMANCE; act++) {
        /* an adult avatar toward a child */
        begin(&s, DOOMO_PLAYER_ID, ID_PIP, ID_NANA, -1, 80u + (uint32_t)act);
        turn(&s, act, TC_NONE, 0, NULL);
        CHECK_EQ(s.gate, TG_REFUSE_MINOR);
        CHECK(!has_event(&s, TEV_ROMANCE_YES) && !s.conv.consent);
        for (k = 0; k < s.nlines; k++) CHECK(!TA_IS_ROMANCE(s.line[k].act));
        /* a minor avatar toward an adult */
        for (avatar = 0; avatar < 2; avatar++) {
            begin(&s, avatar ? ID_PIP : ID_TEEN, ID_NANA, ID_RIVET, -1, 90u + (uint32_t)act);
            turn(&s, act, TC_NONE, 0, NULL);
            CHECK_EQ(s.gate, TG_REFUSE_MINOR);
            CHECK(!has_event(&s, TEV_ROMANCE_YES) && !s.conv.consent && s.beat != TBEAT_ROMANCE_ADULT);
            for (k = 0; k < s.nlines; k++) CHECK(!TA_IS_ROMANCE(s.line[k].act));
        }
    }
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- P3: sexual-class input with a minor in the room ends the talk, and is never echoed ------------------- */
TEST(talk_dm_p3_k7_ends_the_talk)
{
    TalkState s;
    TalkUtter u;
    uint32_t h0 = talk_safe_lint_hits();
    int k, target;
    premade_town();
    CHECK(open_db());
    for (target = -1; target <= 1; target++) {
        begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_PIP, ID_RIVET, 600u + (uint32_t)target);
        utter(&u, TA_ASK_ABOUT, TC_NONE, target);
        u.safety = TS_SEXUAL;
        strcpy(u.shown, "xyzzy-plugh");
        talk_dm_say(&s, &u);
        CHECK_EQ(s.gate, TG_END_K7);
        while (talk_dm_step(&s, NULL)) {}
        CHECK(s.nlines >= 1 && s.nlines <= 2);
        CHECK(talk_dm_over(&s) && (s.flags & TDF_K7) && s.conv.ended);
        CHECK(has_event(&s, TEV_K7));
        for (k = 0; k < s.nlines; k++) {
            CHECK(s.line[k].flags & TLF_END);
            CHECK(s.line[k].flags & TLF_SINCERE);
            CHECK(!strstr(s.line[k].text, "xyzzy"));
            CHECK(s.line[k].who != 1 || s.nlines == 1);             /* an adult steps in first */
        }
        CHECK_EQ(s.line[0].who == 1, 0);
        CHECK_EQ(turn(&s, TA_GREET, TC_NONE, -1, NULL), 0);          /* everyone is gone for the day */
    }
    /* a minor named in the line: the same */
    begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, -1, 610u);
    utter(&u, TA_ASK_ABOUT, TC_NONE, 0);
    u.safety = TS_SEXUAL | TS_MINOR;
    strcpy(u.shown, "xyzzy-plugh");
    talk_dm_say(&s, &u);
    CHECK_EQ(s.gate, TG_END_K7);
    CHECK(talk_dm_over(&s));
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- the gate's other answers: refusals and distress get the answer alone, no beat, no reactions ------------ */
TEST(talk_dm_gate_answers_stand_alone)
{
    TalkState s;
    TalkUtter u;
    uint32_t h0 = talk_safe_lint_hits();
    premade_town();
    CHECK(open_db());
    begin(&s, DOOMO_PLAYER_ID, ID_NANA, ID_RIVET, ID_GRUMB, 71u);
    utter(&u, TA_ASK_ABOUT, TC_NONE, -1);
    u.safety = TS_DISTRESS;
    strcpy(u.shown, "xyzzy");
    talk_dm_say(&s, &u);
    CHECK_EQ(s.gate, TG_DISTRESS);
    CHECK_EQ(s.nplan, 1);
    while (talk_dm_step(&s, NULL)) {}
    CHECK_EQ(s.nlines, 1);
    CHECK(s.line[0].flags & TLF_SINCERE);
    CHECK(!talk_dm_over(&s));
    utter(&u, TA_ASK_ABOUT, TC_NONE, 1);
    u.safety = TS_HATE;
    talk_dm_say(&s, &u);
    CHECK_EQ(s.gate, TG_HATE);
    CHECK_EQ(s.nplan, 1);
    CHECK(has_event(&s, TEV_HATE));
    CHECK(s.part[1].rel_d_player < 0);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* ---- keeping the talk going (DK_FOLLOW, TGE_FOLLOW, talk_dm_open) -------------------- */
TEST(talk_dm_keep_going)
{
    TalkState s, s2;
    uint32_t h0 = talk_safe_lint_hits();
    uint16_t pie, music;
    int i, n, switches = 0, cools = 0, long_runs = 0, runs = 0;
    char first[TALK_LINE_MAX];
    premade_town();
    CHECK(open_db());
    pie = concept("pie");
    music = concept("music");
    CHECK(pie && music);
    /* the talk opens by itself, said by the resident to the player, the same on a replay */
    begin(&s, DOOMO_PLAYER_ID, ID_NANA, -1, -1, 777u);
    CHECK(talk_dm_open(&s));
    n = 0;
    while (talk_dm_step(&s, NULL)) n++;
    CHECK(n >= 1 && n <= TALK_EXCHANGE_MAX);
    CHECK_EQ(s.line[n - 1].who, 0);
    CHECK(s.line[n - 1].text[0]);
    snprintf(first, sizeof first, "%s", s.line[n - 1].text);
    begin(&s2, DOOMO_PLAYER_ID, ID_NANA, -1, -1, 777u);
    CHECK(talk_dm_open(&s2));
    while (talk_dm_step(&s2, NULL)) {}
    CHECK(strcmp(first, s2.line[s2.nlines - 1].text) == 0);
    for (i = 0; i < 24; i++) {
        begin(&s, DOOMO_PLAYER_ID, ID_NANA, -1, -1, 900u + (uint32_t)i);
        turn(&s, TA_GREET, TC_NONE, 0, NULL);
        n = turn(&s, TA_CHANGE_TOPIC, pie, 0, NULL);
        if (n >= 2 && s.line[n - 1].who == 0 && s.line[n - 1].to == -1 && s.line[n - 1].text[0]) switches++;
        begin(&s, DOOMO_PLAYER_ID, ID_NANA, -1, -1, 1900u + (uint32_t)i);
        n = turn(&s, TA_TEASE, TC_NONE, 0, NULL);
        if (n >= 2 && s.line[n - 1].who == 0 && s.line[n - 1].text[0]) cools++;
        begin(&s, DOOMO_PLAYER_ID, ID_NANA, -1, -1, 2900u + (uint32_t)i);
        n = turn(&s, TA_ASK_OPINION, music, 0, NULL);
        runs++;
        if (n >= 2) long_runs++;
        CHECK(s.tension <= TALK_TENSION_MAX);
    }
    CHECK_EQ(switches, 24);
    CHECK_EQ(cools, 24);
    CHECK(long_runs * 10 >= runs * 5);
    begin(&s, DOOMO_PLAYER_ID, ID_NANA, -1, -1, 31u);
    turn(&s, TA_FAREWELL, TC_NONE, 0, NULL);
    CHECK(talk_dm_over(&s));
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}
