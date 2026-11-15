/* talk_quest.c - TQ-QUESTS: residents raise requests while you talk (owner: TALK; docs/TALK.md 6q).
 *
 * Owner decision 2026-11-15, "TALK QUESTS": now and then a resident raises a request or a problem in a conversation,
 * paced by the drama manager and never twice in a row. This file is the rules and the table; talk_dm.c raises the
 * request (a DK_QUEST line), answers the player's accept / decline / haggle / ask more, and adds an event for each;
 * talk_outcome.c applies the events (the table, the reward, the bond, the headline).
 *
 *   sources   TQSRC_PROBLEM the sim's problems queue (sim_problem_for: hungry, wants an item, bored, wants a friend, lonely,
 *             a fight), TQSRC_FAVOUR a small favour (a snack, a toy, a message, company), TQSRC_TALK a Talk-born one (cheer
 *             someone up, keep a secret).
 *   memory    the request is in the talk memory entry of the giver, in the 10 high bits of TalkMem.spare: kind, state,
 *             a haggle bonus and the days left. No byte more in the chunk. What a request is about (the item, the person)
 *             is not stored: talk_quest_derive recomputes it the same way every time (a food from the giver's likes, the
 *             friend with the smallest hash), so the log, the line and the hand-in always agree.
 *   quest log story_questlog.c reads talk_quest_log_count / _row (weak defaults in weak_talk.c: no rows where Talk is not
 *             linked). There is no add function in the log: it shows the rows that exist.
 *   pacing    not saved. A raise needs one conversation since the last raise (never twice in a row, also across a load:
 *             the counter starts at 0), no raise yet this town day, and fewer than TALK_QUEST_OPEN_MAX open requests.
 *
 * KID RULES (tables below, exhaustive tests in test_talk_quests.c): a minor giver, or a minor in the room or at the
 * controls, gets only a snack, a toy, a message, company or cheering up, and a message or a cheer-up is always about
 * someone of the giver's own stage. Secrets and fights are for adults in adult rooms only. No kind is a place, a
 * romance, a date, an adult item or a danger. The care window, grief and a hostile or sad exchange never raise one
 * (talk_dm.c asks before it plans the line).
 *
 * No stdio, no malloc, no floats.
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/talk_traits.h"
#include "doomo/sim.h"
#include "doomo/town.h"
#include "doomo/items.h"
#include "doomo/playas.h"
#include "doomo/resident_acc.h"

#define KIND_WEIGHT_BASE 2
#ifdef DOOMO_TALK_DEBUG
static struct { uint8_t armed, kind; } dbg;                  /* the native runner and the tests: raise the next one at once */
#endif

/* ---- the rules: tables --------------------------------------------------------------------- */
int talk_quest_kind_ok(int kind, int minor_giver, int minor_room, int minor_target)
{
    int kid = minor_giver || minor_room;
    switch (kind) {
    case TQK_FETCH: case TQK_COMPANY:
        return 1;
    case TQK_TOY:
        return minor_giver != 0;                                   /* "my toy": a child's request */
    case TQK_MESSAGE: case TQK_CHEER:
        return minor_target >= 0 && !!minor_target == !!minor_giver;   /* someone of the giver's own stage, never across */
    case TQK_PEACE:
        return !kid && minor_target == 0;                          /* adults, adult room: a fight between two adults */
    case TQK_SECRET:
        return !kid && minor_target <= 0;
    default:
        return 0;
    }
}

int talk_quest_item_ok(int item, int minor_giver, int minor_room)
{
    if (item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    if (!talk_act_gift_ok(item, minor_giver, minor_room)) return 0;
    if (talk_act_item_adult(item)) return 0;                       /* never an adult item, in any room */
    return doomo_items[item].category == ICAT_FOOD || item == ITEM_PLUSHIE || item == ITEM_BOARD_GAME;
}

int talk_quest_reply_class(int act)
{
    switch (act) {
    case TA_YES: case TA_AGREE: case TA_PROMISE: case TA_OFFER: case TA_NOD:
        return TQR_ACCEPT;
    case TA_NO: case TA_DISAGREE: case TA_SHAKE_HEAD: case TA_DISMISS:
        return TQR_DECLINE;
    case TA_ASK_FAVOR:
        return TQR_HAGGLE;
    case TA_ASK_ABOUT: case TA_ASK_FACT: case TA_ASK_WHO: case TA_ASK_OPINION: case TA_ASK_FEELING: case TA_ASK_REPEAT:
        return TQR_MORE;
    default:
        return TQR_NONE;
    }
}

int talk_quest_reward(int kind, int bonus)
{
    static const uint8_t base[TQK_COUNT] = { 0, 20, 12, 12, 10, 15, 30, 10 };
    int v = kind > 0 && kind < TQK_COUNT ? base[kind] : 0;
    return bonus ? v + v / 2 : v;
}

const char *talk_quest_kind_name(int kind)
{
    static const char *const n[TQK_COUNT] = { "none", "fetch", "toy", "message", "company", "cheer", "peace", "secret" };
    return kind >= 0 && kind < TQK_COUNT ? n[kind] : "?";
}

/* ---- who and what ---------------------------------------------------------------------------- */
static int minor_of(int id) { return talk_safe_minor(id) != 0; }

static int alive_slot(int id)
{
    const DoomoResident *r = id >= 0 && id < DOOMO_MAX_RESIDENTS ? town_res(id) : NULL;
    return r && r->status == RES_ALIVE;
}

static uint32_t hk(int a, int b, int c)
{
    return doomo_hash32(doomo_town.seed ^ doomo_hash32((uint32_t)a * 0x9E3779B1u + (uint32_t)b * 0x85EBCA6Bu + (uint32_t)c * 0xC2B2AE35u + 0x7154u));
}

static int in_room(const int *room, int n, int id)
{
    int k;
    for (k = 0; k < n; k++) if (room[k] == id) return 1;
    return 0;
}

static int has_trait(int id, int t)
{
    TalkTraitSet ts;
    return talk_trait_get(id, &ts) && talk_traitset_has(&ts, t);
}

/* the problem the giver has open, if its kind is one this kind of request answers */
static const DoomoProblem *problem_of(int giver, int *index)
{
    int pi = sim_problem_for(giver);
    if (index) *index = pi;
    return pi >= 0 ? sim_problem(pi) : NULL;
}

/* the friend (or, with rival, the rival) with the smallest hash, of the giver's stage, alive, not the player; -1 none */
static int pick_person(int giver, int kind, int rival)
{
    uint16_t ids[24];
    int n = rel_list(giver, ids, 24), k, best = -1, want_minor = minor_of(giver), me = playas_slot();
    uint32_t bh = 0;
    for (k = 0; k < n; k++) {
        int id = ids[k], v;
        uint32_t h;
        if (id == giver || id == me || !alive_slot(id) || minor_of(id) != want_minor) continue;
        v = rel_value(giver, id);
        if (rival ? v >= 0 : v <= 0) continue;
        h = hk(giver, kind, id);
        if (best < 0 || h < bh) { best = id; bh = h; }
    }
    return best;
}

static int friendly_to_player(int giver)
{
    int b = talk_gen_rel_band(giver, playas_slot());
    return b == TRB_FRIEND || b == TRB_BEST || b == TRB_PARTNER || b == TRB_FAMILY;
}

/* the item a request of this kind is about; the same for any room (the strictest rule: a kid-safe snack or toy) */
static int item_for(int giver, int kind, int *src)
{
    const DoomoProblem *p;
    int item = 0;
    *src = TQSRC_FAVOUR;
    if (kind == TQK_TOY) return hk(giver, kind, 1) & 1 ? ITEM_PLUSHIE : ITEM_BOARD_GAME;
    p = problem_of(giver, NULL);
    if (p && p->kind == PROB_WANTS_ITEM && talk_quest_item_ok(p->item, 1, 1)) { *src = TQSRC_PROBLEM; return p->item; }
    if (p && p->kind == PROB_HUNGRY) *src = TQSRC_PROBLEM;
    item = talk_act_food_item(giver, 1);
    if (!talk_quest_item_ok(item, 1, 1)) item = ITEM_CUPCAKE;
    return item;
}

int talk_quest_derive(int giver, int kind, const int *room, int n, int minor_room, TalkQuest *out)
{
    TalkQuest q;
    int mg, src = TQSRC_FAVOUR, target = -1, item = 0, tm = -1;
    const DoomoProblem *p;
    if (!alive_slot(giver) || kind <= TQK_NONE || kind >= TQK_COUNT) return 0;
    mg = minor_of(giver);
    p = problem_of(giver, NULL);
    switch (kind) {
    case TQK_FETCH: case TQK_TOY:
        item = item_for(giver, kind, &src);
        if (!talk_quest_item_ok(item, mg, minor_room)) return 0;
        break;
    case TQK_MESSAGE:
        target = pick_person(giver, kind, 0);
        if (target < 0) return 0;
        break;
    case TQK_COMPANY:
        if (p && (p->kind == PROB_BORED || p->kind == PROB_WANTS_FRIEND || p->kind == PROB_LONELY)) src = TQSRC_PROBLEM;
        break;
    case TQK_CHEER:
        target = pick_person(giver, kind, 0);
        if (target < 0) return 0;
        src = TQSRC_TALK;
        break;
    case TQK_PEACE:
        if (p && p->kind == PROB_FIGHT && p->other != 0xFFFF) { target = p->other; src = TQSRC_PROBLEM; }
        else { target = pick_person(giver, kind, 1); src = TQSRC_TALK; }
        if (target < 0 || !alive_slot(target)) return 0;
        break;
    case TQK_SECRET:
        if (!friendly_to_player(giver)) return 0;
        src = TQSRC_TALK;
        break;
    default:
        return 0;
    }
    if (target >= 0) tm = minor_of(target);
    if (!talk_quest_kind_ok(kind, mg, minor_room, tm)) return 0;
    if (room && target >= 0 && in_room(room, n, target)) return 0;      /* "go and tell the one who is standing here": no */
    memset(&q, 0, sizeof q);
    q.kind = (uint8_t)kind;
    q.src = (uint8_t)src;
    q.item = (uint8_t)item;
    q.target = (int16_t)target;
    q.reward = (uint8_t)talk_quest_reward(kind, 0);
    if (out) *out = q;
    return 1;
}

/* is the one this cheer-up is about low enough to need it */
static int needs_cheer(int id)
{
    const DoomoResident *r = alive_slot(id) ? town_res(id) : NULL;
    return r && (r->mood == MOOD_SAD || r->mood == MOOD_ANGRY);
}

int talk_quest_pick(int giver, const int *room, int n, int minor_room, uint32_t salt, TalkQuest *out)
{
    TalkQuest cand[TQK_COUNT];
    uint8_t w[TQK_COUNT];
    int k, total = 0, nc = 0, pick;
    uint32_t h;
    if (!alive_slot(giver) || talk_mem_quest_get(giver, NULL, NULL, NULL, NULL)) return 0;   /* one a resident: open, cooling down or "never mind" pending */
    for (k = TQK_FETCH; k < TQK_COUNT; k++) {
        TalkQuest q;
        int wt;
        if (!talk_quest_derive(giver, k, room, n, minor_room, &q)) continue;
        if (k == TQK_CHEER && !needs_cheer(q.target)) continue;
        wt = KIND_WEIGHT_BASE;
        if (q.src == TQSRC_PROBLEM) wt += 4;                                   /* what the sim already says is wrong comes first */
        if (k == TQK_FETCH && (has_trait(giver, TT_LAZY) || has_trait(giver, TT_GLUTTON))) wt += 2;
        if ((k == TQK_COMPANY || k == TQK_MESSAGE) && has_trait(giver, TT_OUTGOING)) wt += 2;
        if (k == TQK_MESSAGE && has_trait(giver, TT_SHY)) wt += 2;
        if (k == TQK_CHEER && (has_trait(giver, TT_KIND) || has_trait(giver, TT_CHEERFUL))) wt += 2;
        if (k == TQK_SECRET && (has_trait(giver, TT_PARANOID) || has_trait(giver, TT_SHY))) wt += 1;
        cand[nc] = q;
        w[nc++] = (uint8_t)wt;
        total += wt;
    }
    if (!nc) return 0;
#ifdef DOOMO_TALK_DEBUG
    if (dbg.armed && dbg.kind) {                              /* armed for one kind: that one or none */
        int j, m = 0;
        for (j = 0; j < nc; j++) if (cand[j].kind == dbg.kind) cand[m++] = cand[j];
        total = 0;
        for (j = 0; j < m; j++) { w[j] = 1; total++; }
        nc = m;
        if (!nc) return 0;
    }
#endif
    h = doomo_hash32(salt ^ hk(giver, 77, 0));
    pick = (int)(h % (uint32_t)total);
    for (k = 0; k < nc; k++) { if (pick < w[k]) break; pick -= w[k]; }
    if (k >= nc) k = nc - 1;
    if (out) *out = cand[k];
    return 1;
}

int talk_quest_haggle(int giver, uint32_t salt)
{
    int score = 0, band;
    if (!alive_slot(giver)) return 0;
    band = talk_gen_rel_band(giver, playas_slot());
    if (band == TRB_FRIEND) score += 1;
    else if (band == TRB_BEST || band == TRB_PARTNER || band == TRB_FAMILY) score += 2;
    else if (band == TRB_STRANGER || band == TRB_RIVAL) score -= 2;
    if (has_trait(giver, TT_KIND) || has_trait(giver, TT_LOYAL) || has_trait(giver, TT_CHEERFUL)) score += 1;
    if (has_trait(giver, TT_MEAN) || has_trait(giver, TT_SNOB) || has_trait(giver, TT_GLOOMY)) score -= 2;
    if (has_trait(giver, TT_AMBITIOUS)) score -= 1;
    if (score >= 2) return 1;
    if (score == 1) return (int)(doomo_hash32(salt ^ hk(giver, 91, 0)) & 1u);
    return 0;
}

/* ---- pacing ------------------------------------------------------------------------------------ */
static struct { uint8_t since, raised; uint32_t day; uint8_t has_day; } pace;

void talk_quest_reset(void)
{
    memset(&pace, 0, sizeof pace);
#ifdef DOOMO_TALK_DEBUG
    memset(&dbg, 0, sizeof dbg);
#endif
}

int talk_quest_raise_chance(void)
{
#ifdef DOOMO_TALK_DEBUG
    if (dbg.armed) return 100;
#endif
    return 14;
}

#ifdef DOOMO_TALK_DEBUG
void talk_quest_debug_arm(int kind)
{
    pace.since = 1;
    pace.raised = 0;
    pace.has_day = 0;
    dbg.armed = 1;
    dbg.kind = (uint8_t)kind;
}
#endif

int talk_quest_pace_ok(void)
{
    if (pace.since < 1 || pace.raised) return 0;
    if (pace.has_day && pace.day == doomo_town.day) return 0;
    return talk_quest_open_count() < TALK_QUEST_OPEN_MAX;
}

void talk_quest_note_raise(void)
{
#ifdef DOOMO_TALK_DEBUG
    dbg.armed = 0;
#endif
    pace.since = 0;
    pace.raised = 1;
    pace.day = doomo_town.day;
    pace.has_day = 1;
}

void talk_quest_note_end(void)
{
    if (!pace.raised && pace.since < 255) pace.since++;
    pace.raised = 0;
}

/* ---- the table ------------------------------------------------------------------------------------ */
int talk_quest_state(int giver, int *kind, int *state, int *days, int *bonus)
{
    return alive_slot(giver) && talk_mem_quest_get(giver, kind, state, days, bonus);
}

int talk_quest_open_count(void)
{
    int i, n = 0;
    for (i = 0; i < talk_mem_count(); i++) {
        const TalkMem *e = talk_mem_at(i);
        int st = 0;
        if (e && talk_mem_quest_get(e->id, NULL, &st, NULL, NULL) && (st == TQS_ACTIVE || st == TQS_READY)) n++;
    }
    return n;
}

int talk_quest_add(int giver, int kind, int bonus)
{
    int k, st;
    if (!alive_slot(giver) || kind <= TQK_NONE || kind >= TQK_COUNT) return 0;
    if (talk_mem_quest_get(giver, &k, &st, NULL, NULL) && (st == TQS_ACTIVE || st == TQS_READY)) return 0;   /* one a resident */
    if (talk_quest_open_count() >= TALK_QUEST_OPEN_MAX) return 0;
    if (!talk_quest_derive(giver, kind, NULL, 0, 0, NULL)) return 0;
    return talk_mem_quest_set(giver, kind, TQS_ACTIVE, TQ_DAYS, bonus ? 1 : 0);
}

int talk_quest_ready(int giver)
{
    TalkQuest q;
    int kind, st;
    if (!talk_quest_state(giver, &kind, &st, NULL, NULL)) return 0;
    if (st == TQS_READY) return 1;
    if (st != TQS_ACTIVE) return 0;
    if (kind != TQK_FETCH && kind != TQK_TOY) return 0;
    return talk_quest_derive(giver, kind, NULL, 0, 0, &q) && inv_count(q.item) > 0;
}

int talk_quest_complete(int giver)
{
    TalkQuest q;
    int kind, st, bonus, pi, reward;
    const DoomoProblem *p;
    if (!talk_quest_state(giver, &kind, &st, NULL, &bonus) || !talk_quest_ready(giver)) return -1;
    if (!talk_quest_derive(giver, kind, NULL, 0, 0, &q) && kind != TQK_COMPANY) return -1;
    p = problem_of(giver, &pi);
    if (kind == TQK_FETCH || kind == TQK_TOY) {
        if (kind == TQK_FETCH && p && p->kind == PROB_WANTS_ITEM && p->item == q.item) {
            sim_problem_resolve(pi);                                    /* the sim takes the item and cheers them up */
            if (sim_problem_for(giver) == pi) return -1;
        } else if (!inv_take(q.item, 1)) {
            return -1;
        } else if (kind == TQK_FETCH && p && p->kind == PROB_HUNGRY) {
            sim_problem_resolve(pi);
        }
    } else if (p && ((kind == TQK_COMPANY && (p->kind == PROB_BORED || p->kind == PROB_WANTS_FRIEND || p->kind == PROB_LONELY)) ||
                     (kind == TQK_PEACE && p->kind == PROB_FIGHT))) {
        sim_problem_resolve(pi);
    }
    reward = talk_quest_reward(kind, bonus);
    doomo_town.money += (uint32_t)reward;
    talk_mem_quest_set(giver, kind, TQS_SETTLED, TQ_COOLDOWN, 0);
    return reward;
}

void talk_quest_decline(int giver)
{
    int kind = TQK_FETCH, st = 0;
    if (!alive_slot(giver)) return;
    if (talk_mem_quest_get(giver, &kind, &st, NULL, NULL) && (st == TQS_ACTIVE || st == TQS_READY)) return;   /* one is open: keep it */
    talk_mem_quest_set(giver, TQK_FETCH, TQS_SETTLED, TQ_COOLDOWN, 0);
}

void talk_quest_lapse_ack(int giver)
{
    int kind, st, days;
    if (!talk_mem_quest_get(giver, &kind, &st, &days, NULL) || st != TQS_SETTLED || days != 0) return;
    talk_mem_quest_set(giver, kind, TQS_SETTLED, 1, 0);          /* said so: a day's quiet, then it is gone */
}

/* a conversation is over: what its summary meets of the open requests */
void talk_quest_end(const TalkSummary *sum)
{
    int i, k, nopen = 0;
    uint16_t open_ids[TALK_MEM_MAX];
    if (sum) {
        for (i = 0; i < talk_mem_count(); i++) {                 /* the setters below move entries to the front: list the givers first */
            const TalkMem *e = talk_mem_at(i);
            int st = 0;
            if (e && talk_mem_quest_get(e->id, NULL, &st, NULL, NULL) && st == TQS_ACTIVE) open_ids[nopen++] = e->id;
        }
        for (i = 0; i < nopen; i++) {
            int giver = open_ids[i], kind, st, g = -1, t = -1;
            TalkQuest q;
            if (!talk_mem_quest_get(giver, &kind, &st, NULL, NULL) || st != TQS_ACTIVE) continue;
            for (k = 0; k < sum->n && k < TALK_PARTS_MAX; k++) {
                if (sum->pflags[k] & TPF_LEFT) continue;
                if (sum->id[k] == giver) g = k;
            }
            for (k = 0; k < sum->nev; k++)                       /* accepted in this very conversation: it starts after it */
                if (sum->ev[k].kind == TEV_QUEST_ACCEPT && sum->ev[k].a < sum->n && sum->id[sum->ev[k].a] == giver) g = -2;
            if (g == -2) continue;
            if (kind == TQK_FETCH || kind == TQK_TOY) continue;      /* ready by what is in the bag */
            if (!talk_quest_derive(giver, kind, NULL, 0, 0, &q)) q.target = -1;
            if (q.target >= 0)
                for (k = 0; k < sum->n && k < TALK_PARTS_MAX; k++)
                    if (sum->id[k] == q.target && !(sum->pflags[k] & TPF_LEFT)) t = k;
            switch (kind) {
            case TQK_MESSAGE:
                if (t >= 0) talk_mem_quest_set(giver, kind, TQS_READY, TQ_DAYS, 0);
                break;
            case TQK_CHEER:
                if (t >= 0 && sum->mood_d[t] > 0) talk_mem_quest_set(giver, kind, TQS_READY, TQ_DAYS, 0);
                break;
            case TQK_COMPANY:
                if (g >= 0 && sum->turns >= 3) talk_mem_quest_set(giver, kind, TQS_READY, TQ_DAYS, 0);
                break;
            case TQK_PEACE: {
                int made = 0;
                for (k = 0; k < sum->nev; k++)
                    if (sum->ev[k].kind == TEV_MEDIATED) made = 1;
                if ((made || (q.target >= 0 && rel_value(giver, q.target) >= 0)) && g >= 0)
                    talk_mem_quest_set(giver, kind, TQS_READY, TQ_DAYS, 0);
                break;
            }
            case TQK_SECRET: {
                int d = 0;
                talk_mem_quest_get(giver, NULL, NULL, &d, NULL);
                if (g >= 0 && d < TQ_DAYS) talk_mem_quest_set(giver, kind, TQS_READY, d, 0);   /* a day has passed: still kept */
                break;
            }
            default: break;
            }
        }
    }
    talk_quest_note_end();
}

/* ---- the quest log --------------------------------------------------------------------------------- */
static void goal_text(int giver, int kind, int state, char *buf, int cap)
{
    TalkQuest q;
    const DoomoResident *g = town_res(giver), *t;
    char gn[DOOMO_NAME_LEN + 1], tn[DOOMO_NAME_LEN + 1];
    gn[0] = tn[0] = 0;
    if (g) { strncpy(gn, g->name, DOOMO_NAME_LEN); gn[DOOMO_NAME_LEN] = 0; }
    if (!talk_quest_derive(giver, kind, NULL, 0, 0, &q)) memset(&q, 0, sizeof q), q.target = -1;
    t = q.target >= 0 ? town_res(q.target) : NULL;
    if (t) { strncpy(tn, t->name, DOOMO_NAME_LEN); tn[DOOMO_NAME_LEN] = 0; }
    if (state == TQS_READY || (state == TQS_ACTIVE && (kind == TQK_FETCH || kind == TQK_TOY) && q.item && inv_count(q.item) > 0)) {
        doomo_snprintf(buf, (size_t)cap, "Go back to %s.", gn);
        return;
    }
    switch (kind) {
    case TQK_FETCH: case TQK_TOY:
        doomo_snprintf(buf, (size_t)cap, "Bring %s a %s.", gn, q.item ? doomo_items[q.item].name : "snack");
        break;
    case TQK_MESSAGE: doomo_snprintf(buf, (size_t)cap, "Say hi to %s for %s.", tn, gn); break;
    case TQK_COMPANY: doomo_snprintf(buf, (size_t)cap, "Keep %s company.", gn); break;
    case TQK_CHEER:   doomo_snprintf(buf, (size_t)cap, "Cheer %s up for %s.", tn, gn); break;
    case TQK_PEACE:   doomo_snprintf(buf, (size_t)cap, "Help %s and %s make up.", gn, tn); break;
    case TQK_SECRET:  doomo_snprintf(buf, (size_t)cap, "Keep %s's secret.", gn); break;
    default:          doomo_snprintf(buf, (size_t)cap, "A favour for %s.", gn); break;
    }
}

int talk_quest_log_count(void)
{
    int i, n = 0;
    for (i = 0; i < talk_mem_count() && n < TALK_QUEST_LOG_MAX; i++) {
        const TalkMem *e = talk_mem_at(i);
        int st = 0;
        if (e && talk_mem_quest_get(e->id, NULL, &st, NULL, NULL) && (st == TQS_ACTIVE || st == TQS_READY)) n++;
    }
    return n;
}

int talk_quest_log_row(int i, TalkQuestRow *out)
{
    int k, n = 0;
    if (!out || i < 0) return 0;
    for (k = 0; k < talk_mem_count(); k++) {
        const TalkMem *e = talk_mem_at(k);
        int kind, st, days, bonus;
        if (!e || !talk_mem_quest_get(e->id, &kind, &st, &days, &bonus) || (st != TQS_ACTIVE && st != TQS_READY)) continue;
        if (n++ != i) continue;
        memset(out, 0, sizeof *out);
        out->giver = (int16_t)e->id;
        out->kind = (uint8_t)kind;
        out->state = (uint8_t)(talk_quest_ready(e->id) ? TQS_READY : st);
        out->days = (uint8_t)days;
        out->reward = (uint8_t)talk_quest_reward(kind, bonus);
        goal_text(e->id, kind, st, out->goal, (int)sizeof out->goal);
        return 1;
    }
    return 0;
}

/* ---- the four quick replies --------------------------------------------------------------------- */
int talk_quest_quick(const TalkState *s, TalkSugg *out)
{
    static const TalkSugg q[4] = {
        { TA_YES,       TONE_WARM,  0, -1, "Sure, I'll do it." },
        { TA_NO,        TONE_PLAIN, 0, -1, "Sorry, I can't." },
        { TA_ASK_FAVOR, TONE_CHEEKY, 0, -1, "What's in it for me?" },
        { TA_ASK_ABOUT, TONE_PLAIN, 0, -1, "Tell me more." },
    };
    int k, n = 0;
    if (!s || !out || !s->q_kind || (s->q_st & TQ_ST_CLOSED) || !(s->q_st & TQ_ST_ASKED)) return 0;
    for (k = 0; k < 4; k++) {
        if (k == 2 && (s->q_st & TQ_ST_HAGGLED)) continue;           /* one haggle a request */
        out[n] = q[k];
        out[n].to = (int8_t)(s->q_st & TQ_ST_SLOT);
        n++;
    }
    return n;
}
