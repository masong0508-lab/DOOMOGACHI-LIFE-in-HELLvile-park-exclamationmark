/* talk_plan.c - TM2c: the plan of an answer line, layer a (the reaction kind).
 * Owner: TALK. docs/TALK.md 6m; design docs/design/talk/TALK_MULTILEVEL.md 2.1 and 2.2.
 *
 * talk_plan_kind() is a pure function: it reads a TalkPlanIn (the player's act, the kinds acts.tsv
 * allows after it, the stance as the speaker sees it, the relationship band, the mood band, the
 * effective sliders, the trait bias, the liked / disliked root) and picks one of seven reply kinds:
 * accept, agree, deny, refuse, deflect, counter, ask back. talk_gen.c then picks the core line only
 * among the rules whose reply act serves that kind. Deterministic: the only noise is a small RNG
 * seeded from the conversation seed and the turn, so the same talk gives the same plan everywhere.
 *
 *   score(K) = base + 4*lean*sign(K) + rel + mood + voice + 2*trait + topic + noise(0..2)
 *
 * base is 3 for the kinds the default reply act of acts.tsv serves, 1 for the others. A kind that
 * goes against the lean (a "no" to what they like) needs a reason that is not the dice (rel + mood +
 * voice + trait + topic adding to 2 or more), else it only wins when nothing else is allowed. The
 * best kind and the next two are kept (TalkPlan.next): when no rule of the best kind passes the
 * guards, talk_gen.c tries the next before it drops the filter and counts a miss.
 * A kind that wins by less than TalkPlanIn.margin is "open" (TPL_OPEN): it is named and recorded but the
 * generator does not filter by it. Exception, the REQUEST acts (invite, favour, ask out, give): there a yes or
 * a no is the whole answer, so the plan binds whenever it overrides the natural reply (a rival who says no) or
 * the lean, the relationship or a trait pushes the kind by PUSH_BIND points or more. Measured on the smoke census
 * (docs/TALK.md 6m): a filter on every line costs 3 to 4 points of Surface uniqueness (85.7% to about 82%);
 * binding the requests and the decided cases keeps the bank's variety (85.5%). Line level, requests: a rival
 * says yes in 8% of the lines, a best friend in 100%.
 *
 * The hard rules are code, not data (design 2.2): a romance act is accepted, deflected or refused
 * (consent decides), never countered or asked back; a hug toward or among children is accepted or
 * refused; a kid room never lets the plan pick the cool reply acts (talk_gen.c filters them out).
 * No stdio, no malloc, no floats; 8 ints of stack.
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/rng.h"

#define LEAN_W 4            /* the weight of one step of lean: a stance decides unless the rest adds up */
      /* a kind that wins by less than this binds nothing: the filter keeps the bank's variety (see below) */
#define PUSH_BIND 3         /* a default kind that the lean, a relationship or a trait pushes by this much binds, margin or not */
#define TRAIT_W 2           /* the weight of the trait bias (talk_trait_bias: 0..4 each way) */

static int sign_of(int k)
{
    return (k == TK_ACCEPT || k == TK_AGREE) ? 1 : (k == TK_DENY || k == TK_REFUSE) ? -1 : 0;
}

/* the winner is what the act's default reply would say: the kind itself, or its twin (accept and agree, deny and refuse) */
static int natural(int dflt, int k)
{
    int twin = k == TK_ACCEPT ? TK_AGREE : k == TK_AGREE ? TK_ACCEPT : k == TK_DENY ? TK_REFUSE : k == TK_REFUSE ? TK_DENY : 0;
    return (dflt & (int)TKM(k)) || (twin && (dflt & (int)TKM(twin)));
}

static int is_request(int a)
{
    return a == TA_INVITE || a == TA_ASK_FAVOR || a == TA_ASK_OUT || a == TA_GIVE;
}

static int is_personal(int a)
{
    return is_request(a) || a == TA_ASK_FEELING || a == TA_ASK_ADVICE || a == TA_HUG || a == TA_ASK_ABOUT;
}

/* everything that is not the base, the lean, the traits or the noise; the "reason" a plan may go against the lean */
static int context_term(const TalkPlanIn *in, int k)
{
    int s = 0, rel = in->rel, mood = in->mood, a = in->act;
    const uint8_t *e = in->eff;
    switch (rel) {
    case TRB_STRANGER:
        if (is_personal(a) && k == TK_REFUSE) s += 1;
        if (k == TK_DEFLECT) s += 1;
        break;
    case TRB_FRIEND:
        if (k == TK_ACCEPT) s += 1;
        break;
    case TRB_BEST: case TRB_PARTNER: case TRB_FAMILY:
        if (k == TK_ACCEPT) s += 2;
        if (k == TK_AGREE) s += 1;
        break;
    case TRB_RIVAL:
        if (k == TK_DENY) s += 2;
        if (k == TK_DEFLECT) s += 1;
        if (k == TK_ACCEPT) s -= 3;
        if (k == TK_AGREE) s -= 1;
        break;
    default: break;
    }
    switch (mood) {
    case TMB_HAPPY: if (k == TK_ACCEPT) s += 1; break;
    case TMB_SAD: if (k == TK_COUNTER) s += 1; break;
    case TMB_ANGRY:
        if (k == TK_DENY) s += 2;
        if (k == TK_REFUSE) s += 1;
        break;
    case TMB_TIRED: if ((a == TA_INVITE || a == TA_ASK_FAVOR) && k == TK_REFUSE) s += 1; break;
    case TMB_SCARED:
        if (k == TK_REFUSE || k == TK_DEFLECT) s += 1;
        break;
    case TMB_WEIRD: if (k == TK_COUNTER) s += 1; break;
    default: break;
    }
    if (e[1] >= 10) {                                    /* blunt */
        if (k == TK_DENY) s += 2;
        if (k == TK_REFUSE) s += 1;
    }
    if (e[1] <= 5 && k == TK_AGREE) s += 1;              /* polite */
    if (e[3] >= 11 && (k == TK_ACCEPT || k == TK_DEFLECT)) s += 1;   /* easygoing */
    if (e[3] <= 4 && k == TK_DEFLECT) s -= 1;            /* serious */
    if (e[4] >= 11 && (k == TK_COUNTER || k == TK_DEFLECT)) s += 1;  /* quirky */
    if (in->topic_pref > 0 && (k == TK_ACCEPT || k == TK_AGREE)) s += 2;
    if (in->topic_pref < 0 && (k == TK_REFUSE || k == TK_DEFLECT)) s += 3;
    return s;
}

static int trait_term(const TalkPlanIn *in, int k)
{
    int s = 0;
    if (k == TK_ACCEPT || k == TK_AGREE) s = in->accept;
    else if (k == TK_DENY || k == TK_REFUSE) s = in->deny;
    return TRAIT_W * s;
}

int talk_plan_kind(const TalkPlanIn *in, TalkPlan *out)
{
    DoomoRng rng;
    int k, allowed = in->allowed, romance = TA_IS_ROMANCE(in->act), s, n = 0, j2 = 0;
    int cs[TK_COUNT];                                    /* the kinds, best first: score, and the kind with its flags */
    uint8_t ck[TK_COUNT];
    int push[TK_COUNT];                                  /* per kind: what the lean, relationship, mood, voice, traits and topic added */
    memset(out, 0, sizeof *out);
    out->allowed = in->allowed;
    out->lean = in->lean;
    memcpy(out->eff, in->eff, sizeof out->eff);
    if (in->kid_room) out->flags |= TPL_KIDSAFE;
    if (romance) allowed &= (int)(TKM(TK_ACCEPT) | TKM(TK_DEFLECT) | TKM(TK_REFUSE));   /* never a counter or an ask back */
    if (in->act == TA_HUG && (in->kid_room || in->to_minor)) allowed &= (int)(TKM(TK_ACCEPT) | TKM(TK_REFUSE));
    if (!allowed) return TK_NONE;
    rng_seed(&rng, in->seed);
    for (k = TK_ACCEPT; k < TK_COUNT; k++) {
        int ctx, tt, against, why, j;
        if (!(allowed & (int)TKM(k))) continue;
        ctx = context_term(in, k);
        tt = trait_term(in, k);
        push[k] = LEAN_W * in->lean * sign_of(k) + ctx + tt;
        s = ((in->dflt & TKM(k)) ? 3 : 1) + push[k] + rng_range(&rng, 3);
        if (romance) {                                   /* the consent decides, as before the plan */
            if (k == TK_ACCEPT) s += in->consent ? 8 : -8;
            else if (in->consent) s -= 4;
        }
        /* a kind against the lean (a "no" to what they like, a "yes" to what they hate) needs a reason that is not
         * the dice: the relationship, the mood, the voice, a trait or a liked / disliked topic adds up to 2 or more.
         * Without one it only wins when nothing else is left (and then it counts as reasoned). */
        against = sign_of(k) * in->lean < 0;
        why = ctx + tt >= 2;
        if (against && !why) s -= 100;
        for (j = n; j > 0 && cs[j - 1] < s; j--) { cs[j] = cs[j - 1]; ck[j] = ck[j - 1]; }
        cs[j] = s;
        ck[j] = (uint8_t)(k | (against ? 0x80 : 0) | ((against && !why) ? 0x40 : 0));
        n++;
    }
    if (!n) return TK_NONE;
    out->kind = ck[0] & 7;
    /* open = the bank's own choice stands: a close call, and the winner is the kind the act's default reply serves.
     * A plan that overrides the natural reply (a rival who would say no to an invitation, a lean against a like)
     * always binds: that is where the line would otherwise be wrong (review of TM2c: a rival said yes in 47%). */
    if (n > 1 && cs[0] - cs[1] < in->margin && !(is_request(in->act) && (!natural(in->dflt, out->kind) || push[out->kind] >= PUSH_BIND)))
        out->flags |= TPL_OPEN;
    if (ck[0] & 0x80) out->flags |= TPL_REASON;           /* against the lean: for a reason, or nothing else was left */
    for (k = 1; k < n && j2 < 2; k++)
        if (!(ck[k] & 0x40)) out->next[j2++] = (uint8_t)((ck[k] & 7) | ((ck[k] & 0x80) ? 0x80 : 0));
    return out->kind;
}

int talk_plan_next(TalkPlan *p)
{
    int v = p->next[0];
    if (!v) return 0;
    p->kind = (uint8_t)(v & 7);
    if (v & 0x80) p->flags |= TPL_REASON; else p->flags &= (uint8_t)~TPL_REASON;
    p->next[0] = p->next[1];
    p->next[1] = 0;
    return 1;
}

/* ---- TM2d: topic depth (design 3.1) and the hook rule (design 2.5) -------------------------------------
 * Pure functions: the generator reads the room and the sim, fills a TalkDepthIn and calls these; the tests drive
 * them directly. Hard rules in code: a stranger never reaches 2, a rival never goes past 1, a secret needs a
 * close bond (best friend, partner, family, or a friend whose bond is 40 or more), 3 thread turns and
 * engagement 70, and a grief topic never goes deeper than the surface (grief is sincere, never mined). */
int talk_depth_thread_act(int act)
{
    switch (act) {
    case TA_ASK_ABOUT: case TA_ASK_OPINION: case TA_ASK_FEELING: case TA_TELL: case TA_OPINE_POS: case TA_OPINE_NEG:
    case TA_CONTINUE: case TA_AGREE: case TA_SYMPATHIZE: case TA_COMPLIMENT: case TA_COMFORT:
        return 1;
    }
    return 0;
}

static int cool_act(int a)
{
    return a == TA_INSULT || a == TA_MOCK || a == TA_DISMISS || a == TA_THREATEN;
}

int talk_depth_gate(const TalkDepthIn *in, int turns)
{
    int rank, close, need2, max = 0;
    if (in->grief) return 0;
    rank = in->rel == TRB_STRANGER ? 0 : in->rel == TRB_ACQUAINTANCE ? 1 : in->rel == TRB_FRIEND ? 2 :
           in->rel == TRB_RIVAL ? -1 : 3;                                   /* best, partner, family */
    /* 1: a stance and its reason. A shy or paranoid speaker wants to know you a little first */
    if (!((in->shy || in->paranoid) && rank < 1)) max = 1;
    else return 0;
    if (rank < 0) return 1;                                                  /* a rival: a jab, never deeper */
    /* 2: a story. Acquaintance or better (a stranger never), a shy, paranoid or gloomy speaker one band more,
     * an outgoing one one band less (but never below acquaintance); 2 turns on the thread; engagement 50 */
    need2 = 1 + ((in->shy || in->paranoid || in->gloomy) ? 1 : 0) - (in->outgoing ? 1 : 0);
    if (need2 < 1) need2 = 1;
    if (rank >= need2 && turns >= 2 && in->engage >= 50) max = 2;
    else return max;
    /* 3: a secret. Best friend, partner or family; a friend with a bond of 40 (a loyal one needs no bond);
     * 3 turns; engagement 70. No trait lowers this. */
    close = rank >= 3 || (rank == 2 && (in->level >= 40 || in->loyal));
    if (close && turns >= 3 && in->engage >= 70) max = 3;
    return max;
}

void talk_depth_reset(uint8_t *b)
{
    *b = (uint8_t)(*b & ~3u & ~TDEP_HOOKED);
}

int talk_depth_step(const TalkDepthIn *in, uint8_t *b, uint16_t *tw, uint16_t topic)
{
    int lvl = (int)TDEP_LEVEL(*b), turns = (int)TDEP_TURNS(*b), act = in->act, gate;
    if (cool_act(act)) {                                                      /* down one, the thread stays */
        if (lvl > 0) lvl--;
        *b = (uint8_t)((*b & ~3u) | (unsigned)lvl);
        return 0;
    }
    if (in->grief) { talk_depth_reset(b); return 0; }
    if (!talk_depth_thread_act(act) && act != TA_CHANGE_TOPIC) return 0;        /* a greeting, a thanks: nothing moves */
    if (topic && !in->same && !(act == TA_CHANGE_TOPIC && in->root && in->root == in->thread_root)) {
        /* another topic: another root starts a new thread; the same root with a thread act holds */
        if (!in->thread_root || in->root != in->thread_root) {
            *b = (uint8_t)(1u << 2);                                          /* level 0, 1 turn, hook unused */
            *tw = (uint16_t)((*tw & 0x7000u) | (topic & 0xFFFu));
            return 0;
        }
        *tw = (uint16_t)((*tw & 0x7000u) | (topic & 0xFFFu));                 /* the same root: the new topic, the level held */
        return 0;
    }
    if (act == TA_CHANGE_TOPIC) {                                             /* within the root: hold, the reply bridges */
        if (topic) *tw = (uint16_t)((*tw & 0x7000u) | (topic & 0xFFFu));
        return 0;
    }
    if (!topic && !TDEP_TOPIC(*tw)) return 0;                                  /* nothing to dig into */
    if (!in->same && TDEP_TOPIC(*tw) && topic) return 0;
    gate = talk_depth_gate(in, turns);
    if (lvl < gate) lvl++;
    else if (lvl > gate) lvl = gate;
    if (turns < 7) turns++;
    *b = (uint8_t)((*b & TDEP_HOOKED) | ((unsigned)turns << 2) | (unsigned)lvl);
    if (topic && !TDEP_TOPIC(*tw)) *tw = (uint16_t)((*tw & 0x7000u) | (topic & 0xFFFu));
    return lvl;
}

int talk_hook_wanted(int act, int kind, int romance, int kid_room, int sincere)
{
    (void)kid_room;
    if (romance || sincere || TA_IS_SAFETY(act)) return TH_NONE;              /* a romance answer is the target's alone; a sincere line takes no hook */
    if (kind == TK_COUNTER) return TH_ASK;
    if ((kind == TK_DENY || kind == TK_REFUSE) && (act == TA_INVITE || act == TA_ASK_FAVOR)) return TH_OFFER;
    return TH_NONE;
}
