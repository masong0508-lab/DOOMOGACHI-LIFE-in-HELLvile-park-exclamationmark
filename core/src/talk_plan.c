/* talk_plan.c - TM2c: the plan of an answer line, layer a (the reaction kind).
 * Owner: TALK. docs/TALK.md 6j; design docs/design/talk/TALK_MULTILEVEL.md 2.1 and 2.2.
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
 * generator does not filter by it. Measured on the smoke census (docs/TALK.md 6j): a filter on every
 * line costs 4 points of Surface uniqueness (85.7% to 81.7%); binding only where something decides
 * (a stance, a relationship, a trait, a romance consent) keeps the bank's variety (85.5%).
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
#define TRAIT_W 2           /* the weight of the trait bias (talk_trait_bias: 0..4 each way) */

static int sign_of(int k)
{
    return (k == TK_ACCEPT || k == TK_AGREE) ? 1 : (k == TK_DENY || k == TK_REFUSE) ? -1 : 0;
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
        s = ((in->dflt & TKM(k)) ? 3 : 1) + LEAN_W * in->lean * sign_of(k) + ctx + tt + rng_range(&rng, 3);
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
    if (n > 1 && cs[0] - cs[1] < in->margin) out->flags |= TPL_OPEN;
    out->kind = ck[0] & 7;
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
