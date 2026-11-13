/* talk_outcome.c - T6-OUT: what a conversation does to the sim.
 * Owner: TK. Design 5.5; docs/TALK.md 6g.
 *
 * talk_dm_end() hands a TalkSummary and applies nothing. This file applies it:
 *   bond with the player  rel_adjust(avatar, id, d)  +-12 a resident a town day,
 *                         friendship level +3 a day, family_care for a family member
 *   resident-resident     rel_adjust(a, b, d)        +-8 a pair a day
 *   moods                 STAT_HAPPY +-12 a day, STAT_SOCIAL +4..+12 by engagement (12 a day)
 *   problems              sim_problem_resolve, one a resident a day
 *   mediation             sim_make_up(a, b), one a pair a day
 *   news                  NT_TALK_*, one headline a conversation, none for a row of minors
 *   romance               only ever ASKED: a yes to CONFESS / ASK_OUT calls sim_confess,
 *                         once a pair a day; the sim owns every crush bit and dating edge
 * A ledger of the day (keyed by town day and seed) keeps the caps across conversations,
 * so no number of talks farms more than the caps. It is not saved. Native builds only
 * reference this file (the console ports link no Talk): 0 B on GBA / PS1 / DC.
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/sim.h"
#include "doomo/town.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "sim_int.h"

#define LED_P 64
#define LED_Q 64
#define LPF_PROBLEM 0x01u          /* a problem was resolved today */
#define LPF_CARE    0x02u          /* family_care was given today  */
#define LQF_MADE_UP 0x01u
#define LQF_ASKED   0x02u          /* sim_confess was asked today  */

typedef struct { uint16_t id; int8_t rel, mood; uint8_t social, level, flags, pad; } LedP;   /* 8 B */
typedef struct { uint16_t a, b; int8_t d; uint8_t flags; } LedQ;                              /* 6 B */
static struct {
    uint32_t day, seed;
    uint8_t  live, np, nq, rp, rq, news;
    LedP p[LED_P];
    LedQ q[LED_Q];
} led;

void talk_outcome_reset(void) { memset(&led, 0, sizeof led); }

static void roll(void)
{
    if (led.live && led.day == doomo_town.day && led.seed == doomo_town.seed) return;
    memset(&led, 0, sizeof led);
    led.live = 1;
    led.day = doomo_town.day;
    led.seed = doomo_town.seed;
}

static LedP *led_p(int id)
{
    int i;
    for (i = 0; i < led.np; i++) if (led.p[i].id == (uint16_t)id) return &led.p[i];
    if (led.np < LED_P) i = led.np++;
    else { i = led.rp; led.rp = (uint8_t)((led.rp + 1) % LED_P); }
    memset(&led.p[i], 0, sizeof led.p[i]);
    led.p[i].id = (uint16_t)id;
    return &led.p[i];
}

static LedQ *led_q(int a, int b)
{
    int i;
    if (a > b) { int t = a; a = b; b = t; }
    for (i = 0; i < led.nq; i++) if (led.q[i].a == (uint16_t)a && led.q[i].b == (uint16_t)b) return &led.q[i];
    if (led.nq < LED_Q) i = led.nq++;
    else { i = led.rq; led.rq = (uint8_t)((led.rq + 1) % LED_Q); }
    memset(&led.q[i], 0, sizeof led.q[i]);
    led.q[i].a = (uint16_t)a;
    led.q[i].b = (uint16_t)b;
    return &led.q[i];
}

/* add `want` to a signed total that stays within +-cap; returns what was really added */
static int take_signed(int8_t *used, int want, int cap)
{
    int v = *used + want, d;
    if (v > cap) v = cap;
    if (v < -cap) v = -cap;
    d = v - *used;
    *used = (int8_t)v;
    return d;
}

/* add `want` (>= 0) to an unsigned total up to cap; returns what was really added */
static int take_up(uint8_t *used, int want, int cap)
{
    int d = want;
    if (*used + d > cap) d = cap - *used;
    if (d < 0) d = 0;
    *used = (uint8_t)(*used + d);
    return d;
}

static int is_pet_id(int id) { return id == TALK_PET_ID || talk_safe_is_pet(id); }
static int resident_id(int id) { return id >= 0 && id < DOOMO_MAX_RESIDENTS && !is_pet_id(id); }
static int pool_id(int id) { return id > DOOMO_PLAYER_ID && !is_pet_id(id); }

static int slot_ok(const TalkSummary *s, int k) { return k >= 0 && k < s->n && k < TALK_PARTS_MAX; }

/* the n-th pair (0,1) (0,2) (0,3) (1,2) (1,3) (2,3) of TalkSummary.pair_d */
static void pair_slots(int idx, int *i, int *j)
{
    static const uint8_t pi[6] = { 0, 0, 0, 1, 1, 2 }, pj[6] = { 1, 2, 3, 2, 3, 3 };
    *i = pi[idx];
    *j = pj[idx];
}

static int pair_index(int i, int j)
{
    int k, a, b;
    for (k = 0; k < 6; k++) { pair_slots(k, &a, &b); if ((a == i && b == j) || (a == j && b == i)) return k; }
    return -1;
}

static int minor_id(int id) { return talk_safe_minor(id); }

/* a headline, once per conversation, never about a pair of minors; the row and the
 * walk-out need adults (the ticker would otherwise name a child in a quarrel) */
static void post_news(TalkOutcome *out, int tmpl, int ida, int idb)
{
    if (out->news || led.news >= TALK_CAP_NEWS_DAY) return;      /* one a conversation, a few a day: the ticker is not Talk's */
    if (!resident_id(ida) || (idb >= 0 && !resident_id(idb))) return;
    if (tmpl == NT_TALK_ROW && (minor_id(ida) || (idb >= 0 && minor_id(idb)))) return;
    if (tmpl == NT_TALK_WALKOUT && minor_id(ida)) return;
    if (tmpl != NT_TALK_ROW && tmpl != NT_TALK_WALKOUT && idb >= 0 && minor_id(ida) && minor_id(idb)) return;
    sim_news_rec(tmpl, 0, NEWS_IDS(ida, idb >= 0 ? idb : (int)NEWS_NONE, (int)NEWS_NONE));
    out->news = (uint8_t)tmpl;
    led.news++;
}

static int alive_resident(int id)
{
    const DoomoResident *r = resident_id(id) ? town_res(id) : NULL;
    return r && (r->status == RES_ALIVE);
}

void talk_outcome_apply(const TalkSummary *sum, TalkOutcome *out)
{
    TalkOutcome local;
    int k, av, ev, great_a = -1, great_b = -1;
    int news_made = -1, news_row = -1, news_walk = -1;
    if (!out) out = &local;
    memset(out, 0, sizeof *out);
    if (!sum) return;
    roll();
    sim_ensure_init();
    av = playas_slot();

    /* ---- the bond, level, mood and needs of each resident ---- */
    for (k = 0; k < sum->n && k < TALK_PARTS_MAX; k++) {
        int id = sum->id[k], applied, want;
        LedP *lp;
        if (is_pet_id(id)) continue;
        if (pool_id(id)) {                                   /* a family pool member: no town slot, the bond nibble only */
            lp = led_p(id);
            if (sum->rel_d_player[k] > 0 && !(lp->flags & LPF_CARE)) { lp->flags |= LPF_CARE; family_care(id); out->rel[k] = 1; }
            if (sum->mood_d[k]) { int d = take_signed(&lp->mood, sum->mood_d[k], TALK_CAP_MOOD_DAY);
                                  if (d) { family_mood_add(id, d > 0 ? 1 : -1); out->mood[k] = (int8_t)d; } }
            continue;
        }
        if (!alive_resident(id)) continue;
        lp = led_p(id);
        want = sum->rel_d_player[k];
        applied = take_signed(&lp->rel, want, TALK_CAP_REL_DAY);
        if (applied) rel_adjust(id, av, applied);
        out->rel[k] = (int8_t)applied;
        if (applied > 0) {
            DoomoResident *r = town_res(id);
            int lv = take_up(&lp->level, (applied + 3) / 4, TALK_CAP_LEVEL_DAY);
            r->level = (uint8_t)DOOMO_MIN(99, r->level + lv);
            out->level = (uint8_t)(out->level + lv);
            if (!(lp->flags & LPF_CARE)) { lp->flags |= LPF_CARE; family_care(id); }   /* a no-op unless they are family */
        }
        {
            DoomoResident *r = town_res(id);
            int d = take_signed(&lp->mood, sum->mood_d[k], TALK_CAP_MOOD_DAY);
            if (d) sim_stat_add(r, STAT_HAPPY, d);
            out->mood[k] = (int8_t)d;
            if (!(sum->pflags[k] & TPF_LEFT)) {              /* STAT_SOCIAL +4..+12 by engagement */
                int soc = take_up(&lp->social, 4 + sum->engage[k] * 8 / 100, TALK_CAP_SOCIAL_DAY);
                sim_stat_add(r, STAT_SOCIAL, soc);
                out->social = (uint8_t)(out->social + soc);
            }
            sim_mood_of(id);
        }
    }

    /* ---- resident-resident bonds ---- */
    for (k = 0; k < 6; k++) {
        int i, j, d, ida, idb;
        pair_slots(k, &i, &j);
        if (!slot_ok(sum, i) || !slot_ok(sum, j) || !sum->pair_d[k]) continue;
        ida = sum->id[i]; idb = sum->id[j];
        if (!alive_resident(ida) || !alive_resident(idb)) continue;
        d = take_signed(&led_q(ida, idb)->d, sum->pair_d[k], TALK_CAP_PAIR_DAY);
        if (d) rel_adjust(ida, idb, d);
        out->pair[k] = (int8_t)d;
    }

    /* ---- the events ---- */
    for (ev = 0; ev < sum->nev && ev < TALK_EVENTS_MAX; ev++) {
        const TalkEvent *e = &sum->ev[ev];
        int ia = slot_ok(sum, e->a) ? sum->id[e->a] : -1, ib = slot_ok(sum, e->b) ? sum->id[e->b] : -1;
        switch (e->kind) {
        case TEV_ULTIMATUM:                                  /* a big row: the bond falls (rel_adjust demotes to rival by itself) */
            if (ia >= 0 && ib >= 0 && alive_resident(ia) && alive_resident(ib)) {
                int d = take_signed(&led_q(ia, ib)->d, -3, TALK_CAP_PAIR_DAY), pk = pair_index(e->a, e->b);
                if (d) rel_adjust(ia, ib, d);
                if (pk >= 0) out->pair[pk] = (int8_t)(out->pair[pk] + d);
                if (news_row < 0) news_row = ev;
            }
            break;
        case TEV_MEDIATED:                                   /* peace: the sim's own make-up, once a pair a day */
            if (ia >= 0 && ib >= 0 && alive_resident(ia) && alive_resident(ib)) {
                LedQ *q = led_q(ia, ib);
                if (!(q->flags & LQF_MADE_UP) && rel_kind(ia, ib) >= 0) {
                    sim_make_up(ia, ib);
                    q->flags |= LQF_MADE_UP;
                    out->made_up++;
                    if (news_made < 0) news_made = ev;
                }
            }
            break;
        case TEV_PROBLEM:                                    /* talked through: one a resident a day; the sim decides if it closes */
            if (ia >= 0 && resident_id(ia) && slot_ok(sum, e->a) && (sum->pflags[e->a] & TPF_PROBLEM)) {
                LedP *lp = led_p(ia);
                int pi = (lp->flags & LPF_PROBLEM) ? -1 : sim_problem_for(ia);
                if (pi >= 0) {
                    sim_problem_resolve(pi);
                    if (sim_problem_for(ia) < 0) { lp->flags |= LPF_PROBLEM; out->problems++; }   /* an open one (an item, a possession) stays open */
                }
            }
            break;
        case TEV_ROMANCE_YES:                                /* Talk only asks: the sim rolls and owns the result */
            if (ia >= 0 && (e->b == TA_CONFESS || e->b == TA_ASK_OUT) && resident_id(ia) && sum->avatar >= 0 &&
                sum->avatar < DOOMO_MAX_RESIDENTS && alive_resident(sum->avatar)) {
                LedQ *q = led_q(sum->avatar, ia);
                if (!(q->flags & LQF_ASKED)) {
                    q->flags |= LQF_ASKED;
                    out->confessed++;
                    if (sim_confess(sum->avatar, ia, 10)) out->dating++;
                }
            }
            break;
        case TEV_WALKOUT:
            if (ia >= 0 && news_walk < 0) news_walk = ev;
            break;
        case TEV_GREAT_NIGHT: {
            int j;
            for (j = 0; j < sum->n && j < TALK_PARTS_MAX; j++) {
                if (!resident_id(sum->id[j]) || (sum->pflags[j] & TPF_LEFT)) continue;
                if (great_a < 0) great_a = sum->id[j]; else if (great_b < 0) great_b = sum->id[j];
            }
            break;
        }
        default: break;
        }
    }

    /* ---- one headline: peace, then the row, the walk-out, the great night ---- */
    if (news_made >= 0) {
        const TalkEvent *e = &sum->ev[news_made];
        post_news(out, NT_TALK_MADE_UP, sum->id[e->a], sum->id[e->b]);
    }
    if (!out->news && news_row >= 0) {
        const TalkEvent *e = &sum->ev[news_row];
        post_news(out, NT_TALK_ROW, sum->id[e->a], sum->id[e->b]);
    }
    if (!out->news && news_walk >= 0) post_news(out, NT_TALK_WALKOUT, sum->id[sum->ev[news_walk].a], -1);
    if (!out->news && great_a >= 0 && great_b >= 0) post_news(out, NT_TALK_GREAT_NIGHT, great_a, great_b);
}

/* ---- the conversation's two ends ------------------------------------------------------ */
void talk_outcome_begin(TalkState *s, int avatar, const int *ids, int n, uint32_t seed)
{
    int k;
    if (!s) return;
    talk_dm_begin(s, avatar, ids, n, seed);
    talk_mem_begin(&s->conv);
    for (k = 0; k < s->nparts; k++)
        if ((s->conv.refused >> k) & 1) s->part[k].flags |= TPF_REFUSED;
}

void talk_outcome_end(TalkState *s, TalkOutcome *out, TalkSummary *sum)
{
    TalkSummary local;
    int k;
    if (!s) return;
    if (!sum) sum = &local;
    for (k = 0; k < s->nparts; k++) {                        /* what the player earned: those who stayed in the room */
        int id = s->part[k].id;
        if (is_pet_id(id) || (s->part[k].flags & TPF_LEFT)) continue;
        if (s->mem_earned) talk_mem_set_flags(id, s->mem_earned);
        if (s->topic_stack[0] && (s->part[k].flags & TPF_SPOKE)) talk_mem_note_topic(id, s->topic_stack[0]);
    }
    talk_mem_end(&s->conv);
    talk_dm_end(s, sum);
    talk_outcome_apply(sum, out);
}
