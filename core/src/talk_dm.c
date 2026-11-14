/* talk_dm.c - Talk T6: the drama manager (owner: TK; docs/TALK.md "The drama
 * manager"; design 5.1-5.4). It is the POLICY over the generator: which beat
 * the room is in (core/data/talk/beats.tlk, bank TB_BEATS), who answers, who
 * reacts, who cuts in, who argues, who walks out. Every line is still made by
 * talk_gen_line / talk_gen_react / talk_gen_entry; the T4 gate runs first on
 * every player utterance and the S3 lint on every line (inside the generator).
 *
 * No static RAM: everything between calls is in TalkState (the house's scene
 * scratch). One talk_dm_step() makes one line. The random stream of a call is
 * seeded from the conversation seed, the turn and the exchange, so the same
 * session gives the same text on every platform.
 *
 * SAFETY lives here in C whatever beats.tlk says: see talk.h. The short list:
 *   - the beat ROMANCE_ADULT needs an adult room (no minor, no pet, no kid cast,
 *     an adult avatar), talk_safe_romance_ok(), and the target's yes to the
 *     player's own romance act this turn; its goal line is said by that adult;
 *   - no other line the drama manager starts is a romance act (one filter);
 *   - an argument stops at the jab when anyone here is not an adult; rivals
 *     who are not both adults never argue;
 *   - the room's audience only gets stricter during a talk;
 *   - romance state is never written here; the sim is only asked.
 */
#include <string.h>

#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/sim.h"
#include "doomo/town.h"
#include "doomo/rng.h"
#include "doomo/family.h"
#include "doomo/story.h"
#include "doomo/resident_acc.h"

/* ---- the plan of an exchange (TalkState.plan[].kind) ----------------------- */
enum { DK_NONE = 0, DK_ANSWER, DK_REACT, DK_BEAT, DK_IDLE, DK_PROD, DK_JAB, DK_ROW, DK_ULT, DK_EXCUSE,
       DK_KIDASK, DK_MAKEUP, DK_GRUMBLE, DK_CUT, DK_K7, DK_FOLLOW };

#define BEATS_MAGIC 0x31544254u          /* "TBT1" (talkc_beats.py) */
#define BEATS_HDR   12
#define BEATS_ROW   24
#define TO_ZERO     127

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int absi(int v) { return v < 0 ? -v : v; }

/* ---- the beat table ------------------------------------------------------- */
static int bank_hdr(uint16_t *nbeats)
{
    uint8_t b[BEATS_HDR];
    if (!talk_db_is_open() || !talk_db_read(TB_BEATS, 0, b, BEATS_HDR) || le32(b) != BEATS_MAGIC) return 0;
    if (le16(b + 6) != BEATS_ROW || le16(b + 4) != TBEAT_COUNT) return 0;
    *nbeats = le16(b + 4);
    return 1;
}

int talk_dm_ready(void)
{
    uint16_t n;
    return talk_gen_ready() && bank_hdr(&n);
}

int talk_dm_beat_info(int beat, TalkBeatInfo *out)
{
    uint8_t r[BEATS_ROW];
    uint16_t n;
    if (!out || beat <= 0 || beat >= TBEAT_COUNT || !bank_hdr(&n)) return 0;
    if (!talk_db_read(TB_BEATS, BEATS_HDR + (uint32_t)beat * BEATS_ROW, r, BEATS_ROW)) return 0;
    out->pre = r[0];
    out->tension = (int8_t)r[1];
    out->after = (int8_t)r[2];
    out->goals = r[3];
    out->turns = r[4];
    out->bonus = r[5];
    out->involves = r[6];
    out->lean_bonus = r[7];
    out->flags = r[8];
    out->handles = le16(r + 10);
    out->hurt = le16(r + 12);
    out->lean = le16(r + 14);
    memcpy(out->resolve, r + 16, 4);
    memcpy(out->lean_act, r + 20, 2);
    return 1;
}

int talk_dm_affinity_roots(int type, uint8_t *favours, uint8_t *dull)
{
    uint8_t b[2];
    uint16_t n;
    if (type < 0 || type >= 16 || !bank_hdr(&n) ||
        !talk_db_read(TB_BEATS, BEATS_HDR + (uint32_t)n * BEATS_ROW + 2u * (uint32_t)type, b, 2))
        return 0;
    if (favours) *favours = b[0];
    if (dull) *dull = b[1];
    return 1;
}

int talk_dm_target_tension(int turn)
{
    if (turn < 0) turn = 0;
    if (turn <= 9) return (turn * 6 + 4) / 9;           /* up to the peak at turns 8-10      */
    if (turn <= 15) return 6 - (turn - 9);              /* down to calm by turn 15            */
    return 2;                                           /* then "lively"                      */
}

/* ---- who is who ----------------------------------------------------------- */
static const DoomoResident *res_of(int id, DoomoResident *buf)
{
    if (id >= 0 && id <= DOOMO_PLAYER_ID) {
        const DoomoResident *r = town_res(id);
        return r && r->status != RES_EMPTY ? r : NULL;
    }
    if (id > DOOMO_PLAYER_ID && id != TALK_PET_ID && family_resident(id, buf)) return buf;
    return NULL;
}

static int pers(int id, int p)
{
    DoomoResident buf;
    const DoomoResident *r = res_of(id, &buf);
    return r ? RES_PERSONA(r, p) : 6;
}

static int type_of(int id)
{
    DoomoResident buf;
    const DoomoResident *r = res_of(id, &buf);
    return r ? persona_type(r) : -1;
}

static int relv(int a, int b)
{
    if (a < 0 || a > DOOMO_PLAYER_ID || b < 0 || b > DOOMO_PLAYER_ID) return 0;
    return rel_value(a, b);
}

static int relk(int a, int b)
{
    if (a < 0 || a > DOOMO_PLAYER_ID || b < 0 || b > DOOMO_PLAYER_ID) return -1;
    return rel_kind(a, b);
}

static int is_pet(const TalkState *s, int k) { return (s->part[k].flags & TPF_PET) != 0; }
static int here(const TalkState *s, int k)
{
    return k >= 0 && k < s->nparts && !(s->part[k].flags & TPF_LEFT);
}
static int talker(const TalkState *s, int k) { return here(s, k) && !is_pet(s, k); }
static int adult(const TalkState *s, int k) { return s->part[k].stage_class == TSC_ADULT; }

static int active_residents(const TalkState *s)
{
    int k, n = 0;
    for (k = 0; k < s->nparts; k++) if (talker(s, k)) n++;
    return n;
}

static void rng_for(const TalkState *s, DoomoRng *r, uint32_t salt)
{
    rng_seed(r, s->conv.seed ^ doomo_hash32(0x6D2B79F5u + (uint32_t)s->turn * 977u +
                                            (uint32_t)s->exchange * 131u + salt));
}

static void add_event(TalkState *s, int kind, int a, int b)
{
    if (s->nev >= TALK_EVENTS_MAX) return;
    s->ev[s->nev].kind = (uint8_t)kind;
    s->ev[s->nev].a = (uint8_t)(a < 0 ? 0xFF : a);
    s->ev[s->nev].b = (uint8_t)(b < 0 ? 0xFF : b);
    s->ev[s->nev].turn = s->turn;
    s->nev++;
}

static int pair_index(int a, int b)
{
    static const int8_t ix[4][4] = { { -1, 0, 1, 2 }, { 0, -1, 3, 4 }, { 1, 3, -1, 5 }, { 2, 4, 5, -1 } };
    if (a < 0 || b < 0 || a > 3 || b > 3) return -1;
    return ix[a][b];
}

static void bump_pair(TalkState *s, int a, int b, int d)
{
    int i = pair_index(a, b);
    if (i >= 0) s->pair_d[i] = (int8_t)clampi(s->pair_d[i] + d, -TALK_PAIR_D_MAX, TALK_PAIR_D_MAX);
}

static void bump_rel(TalkState *s, int k, int d)
{
    if (k >= 0 && k < s->nparts && !is_pet(s, k))
        s->part[k].rel_d_player = (int8_t)clampi(s->part[k].rel_d_player + d, -TALK_REL_D_MAX, TALK_REL_D_MAX);
}

static void bump_mood(TalkState *s, int k, int d)
{
    if (k >= 0 && k < s->nparts && !is_pet(s, k))
        s->part[k].mood_d = (int8_t)clampi(s->part[k].mood_d + d, -10, 10);
}

/* ---- the audience: only ever stricter during a talk --------------------------- */
static void room_audience(TalkState *s, int target, TalkAudience *aud)
{
    int ids[TALK_PARTS_MAX], n = 0, k;
    for (k = 0; k < s->nparts; k++) ids[n++] = s->part[k].id;
    talk_safe_ctx(s->conv.avatar, ids, n, target, aud);
    /* what the room was at the start stays: a child who walks out leaves a kids' room behind */
    aud->any_minor |= s->conv.aud.any_minor;
    aud->avatar_minor |= s->conv.aud.avatar_minor;
    aud->pet |= s->conv.aud.pet;
    aud->cast_kid |= s->conv.aud.cast_kid;
    aud->mature_ok &= s->conv.aud.mature_ok;
}

/* may the drama manager play romance in this room (the player's act is not what is asked: the room is) */
static int adult_room(const TalkState *s)
{
    const TalkAudience *a = &s->conv.aud;
    int k;
    if (a->any_minor || a->avatar_minor || a->pet || a->cast_kid) return 0;
    for (k = 0; k < s->nparts; k++)
        if (s->part[k].stage_class != TSC_ADULT) return 0;
    return 1;
}

/* ---- begin ------------------------------------------------------------------- */
void talk_dm_begin(TalkState *s, int avatar, const int *ids, int n, uint32_t seed)
{
    int k;
    memset(s, 0, sizeof *s);
    talk_gen_begin(&s->conv, avatar, ids, n, seed);
    s->nparts = s->conv.n;
    for (k = 0; k < s->nparts; k++) {
        TalkPart *p = &s->part[k];
        p->id = s->conv.id[k];
        p->engage = TALK_ENGAGE_START;
        if (talk_safe_is_pet(p->id) || p->id == TALK_PET_ID) {
            p->stage_class = TSC_PET;
            p->flags |= TPF_PET;
            p->engage = 0;
        } else {
            p->stage_class = talk_safe_minor(p->id) ? TSC_MINOR : TSC_ADULT;
        }
    }
    if (s->conv.aud.avatar_minor) s->conv.aud.any_minor = 1;
    s->beat = TBEAT_ARRIVE;
    s->beat_a = s->beat_b = s->arg_a = s->arg_b = -1;
    s->conv.beat = TBEAT_ARRIVE;
    s->beats_used = (uint16_t)(1u << TBEAT_ARRIVE);
    if (!talk_dm_ready()) {
        s->flags |= TDF_NO_BEATS;
        s->beat = s->conv.beat = TBEAT_SMALLTALK;
    }
    if (s->conv.aud.any_minor) s->flags |= TDF_KIDSAFE;
}

int talk_dm_over(const TalkState *s) { return s && (s->flags & TDF_OVER) != 0; }

/* ---- consent (K-5) ------------------------------------------------------------- */
int talk_dm_consent(const TalkState *s, int who, int act)
{
    static const uint8_t need[4] = { 5, 25, 30, 45 };      /* FLIRT, CONFESS, ASK_OUT, KISS */
    int id, rel, want;
    if (!s || !TA_IS_ROMANCE(act) || !talker(s, who) || !adult(s, who)) return 0;
    if (((s->conv.refused >> who) & 1) || (s->part[who].flags & TPF_REFUSED)) return 0;
    if (!adult_room(s)) return 0;
    id = s->part[who].id;
    if (!talk_safe_romance_ok(&s->conv.aud, s->conv.avatar, id)) return 0;
    if (id < 0 || id > DOOMO_PLAYER_ID || s->conv.avatar < 0 || s->conv.avatar > DOOMO_PLAYER_ID) return 0;
    if (!sim_romance_ok(id, s->conv.avatar)) return 0;     /* their preference, both stages, not kin */
    rel = relv(id, s->conv.avatar);
    want = need[act - TA_FIRST_ROMANCE];
    if (rel_has_crush(id, s->conv.avatar)) want -= 10;
    return rel >= want;
}

/* ---- stance, affinity --------------------------------------------------------- */
static int topic_stance(int id, uint16_t topic)
{
    int v;
    if (!topic || id == TALK_PET_ID) return TST_NEUTRAL;
    v = talk_gen_stance(id, topic);
    return v >= 2 ? TST_PRO : v <= -2 ? TST_CON : TST_NEUTRAL;
}

/* design 5.3: +3 a like, +1 a root the persona type favours, -3 a hate or fear, -1 a dull root */
static int affinity(int id, uint16_t topic)
{
    DoomoResident buf;
    const DoomoResident *r;
    TalkConcept tc;
    uint8_t fav = 0, dull = 0;
    int j, root;
    if (!topic || id == TALK_PET_ID) return 0;
    for (j = 0; j < 3; j++) if (talk_gen_like(id, j) == topic) return 3;
    if (talk_gen_like(id, 3) == topic || talk_gen_like(id, 4) == topic) return -3;
    r = res_of(id, &buf);
    if (!r || !talk_db_concept(topic, &tc)) return 0;
    root = talk_db_cat_root(tc.cat);
    if (!talk_dm_affinity_roots(persona_type(r), &fav, &dull)) return 0;
    return root && root == fav ? 1 : root && root == dull ? -1 : 0;
}

static int topic_root(uint16_t topic)
{
    TalkConcept tc;
    return topic && talk_db_concept(topic, &tc) ? talk_db_cat_root(tc.cat) : 0;
}

/* the reward of an act for a persona (5.3 examples) */
static int reward(int id, int act)
{
    switch (act) {
    case TA_JOKE:       return pers(id, PERS_ATTITUDE) >= 10 ? 3 : -1;
    case TA_COMPLIMENT: case TA_PRAISE_OTHER: return pers(id, PERS_EXPRESSION) >= 10 ? 3 : 1;
    case TA_INSULT: case TA_MOCK:
        return (pers(id, PERS_QUIRK) >= 12 || pers(id, PERS_SPEECH) >= 12) ? 2 : -2;
    case TA_GOSSIP:     return type_of(id) == 7 ? 2 : 1;
    case TA_COMFORT: case TA_ENCOURAGE: case TA_SYMPATHIZE: return 1;
    case TA_THREATEN: case TA_DISMISS: return -3;
    default: return 0;
    }
}

/* ---- engagement (5.3) ---------------------------------------------------------- */
static const TalkBeatInfo *beat_now(const TalkState *s, TalkBeatInfo *bi)
{
    return s->beat && talk_dm_beat_info(s->beat, bi) ? bi : NULL;
}

static void update_engage(TalkState *s, const TalkUtter *u, int target)
{
    TalkBeatInfo bi;
    const TalkBeatInfo *b = beat_now(s, &bi);
    int k, act = u->act[0], rep = act && act == s->last_act;
    for (k = 0; k < s->nparts; k++) {
        TalkPart *p = &s->part[k];
        int d, involved = 0;
        if (is_pet(s, k) || (p->flags & TPF_LEFT)) continue;
        if (b) involved = b->involves == TBI_ALL || (b->involves == TBI_A && k == s->beat_a) ||
                          (b->involves == TBI_PAIR && (k == s->beat_a || k == s->beat_b));
        d = (target == k ? 6 : 0) + affinity(p->id, u->topic[0]) + reward(p->id, act) + (involved ? 2 : 0) - 2 -
            (rep ? 4 : 0) - (p->turns_ignored >= 3 ? 3 : 0);
        if (target == k || (p->flags & TPF_SPOKE)) p->turns_ignored = 0;
        else if (p->turns_ignored < 255) p->turns_ignored++;
        p->flags &= (uint8_t)~TPF_SPOKE;
        p->engage = (uint8_t)clampi(p->engage + d, 0, 100);
        if (p->engage < TALK_ENGAGE_AWAY) p->flags |= TPF_AWAY; else p->flags &= (uint8_t)~TPF_AWAY;
        if (p->engage > TALK_ENGAGE_LEAN) p->flags |= TPF_LEAN; else p->flags &= (uint8_t)~TPF_LEAN;
        if (p->engage < TALK_ENGAGE_LEAVE) { if (p->low_turns < 255) p->low_turns++; } else p->low_turns = 0;
    }
}

/* ---- who answers (5.4) ---------------------------------------------------------- */
static int pick_responder(const TalkState *s, uint16_t topic, DoomoRng *rng, int prefer_adult)
{
    int k, best = -1, bs = -100000, pass;
    for (pass = prefer_adult ? 0 : 1; pass < 2 && best < 0; pass++)
        for (k = 0; k < s->nparts; k++) {
            int sc;
            if (!talker(s, k) || (pass == 0 && !adult(s, k))) continue;
            sc = s->part[k].engage + 20 * affinity(s->part[k].id, topic) + relv(s->part[k].id, s->conv.avatar) +
                 rng_range(rng, 21) - 10;
            if (sc > bs) { bs = sc; best = k; }
        }
    if (best < 0)
        for (k = 0; k < s->nparts; k++) if (here(s, k)) return k;           /* only the pet is left */
    return best;
}

/* ---- the beat ------------------------------------------------------------------------ */
static int any_minor_here(const TalkState *s)
{
    int k;
    for (k = 0; k < s->nparts; k++) if (talker(s, k) && !adult(s, k)) return k;
    return -1;
}

/* does precondition `pre` hold; *a / *b the slots it is about (-1 none) */
static int pre_ok(const TalkState *s, int pre, const TalkUtter *u, int *a, int *b)
{
    int k, j;
    uint16_t topic = s->topic_stack[0], v;
    *a = *b = -1;
    switch (pre) {
    case TBP_DEFAULT: return 1;
    case TBP_START: return 0;                              /* ARRIVE is entered by talk_dm_begin only */
    case TBP_LIKE:
        for (k = 0; topic && k < s->nparts; k++) {
            if (!talker(s, k)) continue;
            for (j = 0; j < 3; j++) if (talk_gen_like(s->part[k].id, j) == topic) { *a = k; return 1; }
        }
        return 0;
    case TBP_HOUSE: return topic_root(topic) == TCR_HOUSE;
    case TBP_NEWS:
        for (k = 0; k < s->nparts; k++)
            if (talker(s, k) && talk_gen_fact(s->part[k].id, TCB_NEWS, &v)) { *a = k; return 1; }
        return 0;
    case TBP_PROBLEM:
        for (k = 0; k < s->nparts; k++)
            if (talker(s, k) && !(s->part[k].flags & TPF_PROBLEM) && talk_gen_fact(s->part[k].id, TCB_PROBLEM, &v)) {
                *a = k;
                return 1;
            }
        return 0;
    case TBP_RIVALS:
        for (k = 0; k < s->nparts; k++)
            for (j = k + 1; j < s->nparts; j++) {
                int rk, rv;
                if (!talker(s, k) || !talker(s, j) || !adult(s, k) || !adult(s, j)) continue;   /* never with a minor in it */
                rk = relk(s->part[k].id, s->part[j].id);
                rv = relv(s->part[k].id, s->part[j].id);
                if (rk == REL_RIVAL || rk == REL_NEMESIS || rv < -30) { *a = k; *b = j; return 1; }
            }
        return 0;
    case TBP_CLASH:
        for (k = 0; topic && k < s->nparts; k++)
            for (j = k + 1; j < s->nparts; j++)
                if (talker(s, k) && talker(s, j) && s->part[k].stance != TST_NEUTRAL &&
                    s->part[j].stance != TST_NEUTRAL && s->part[k].stance != s->part[j].stance) {
                    *a = k; *b = j; return 1;
                }
        return 0;
    case TBP_STORY:
        return s->conv.chapter >= STORY_CH1 && s->conv.chapter <= STORY_CH4 &&
               (topic_root(topic) == TCR_LORE || topic_root(topic) == TCR_HELLVILE);
    case TBP_LULL: return s->lull >= 3;
    case TBP_KID:
        k = any_minor_here(s);
        *a = k;
        return k >= 0;
    case TBP_ROMANCE:                                      /* the player's own act, the target's yes, an adult room */
        if (!TA_IS_ROMANCE(u->act[0]) || s->gate != TG_PASS || !adult_room(s)) return 0;
        for (k = 0; k < s->nparts; k++)
            if (talker(s, k) && ((s->conv.consent >> k) & 1) &&
                talk_safe_romance_ok(&s->conv.aud, s->conv.avatar, s->part[k].id)) { *a = k; return 1; }
        return 0;
    case TBP_LATE: return s->turn >= 14;
    }
    return 0;
}

static void start_beat(TalkState *s, int beat, int a, int b)
{
    TalkBeatInfo bi;
    s->beat = (uint8_t)beat;
    s->conv.beat = (uint8_t)beat;
    s->beat_step = 0;
    s->beat_turns = 0;
    s->beat_a = (int8_t)a;
    s->beat_b = (int8_t)b;
    s->beats_used |= (uint16_t)(1u << beat);
    if (talk_dm_beat_info(beat, &bi)) {
        s->tension = (uint8_t)(bi.tension == TO_ZERO ? 0 : clampi((int)s->tension + bi.tension, 0, TALK_TENSION_MAX));
        if (bi.flags & TBTF_KIDSAFE) { s->flags |= TDF_KIDSAFE; s->conv.aud.any_minor = 1; }
    }
    if (beat == TBEAT_RIVALRY) { s->arg_a = (int8_t)a; s->arg_b = (int8_t)b; s->arg_step = 0; s->arg_wait = 0; }
}

static void finish_beat(TalkState *s)
{
    TalkBeatInfo bi;
    if (s->beat_step == 255) return;
    if (s->beat && talk_dm_beat_info(s->beat, &bi))
        s->tension = (uint8_t)clampi((int)s->tension + bi.after, 0, TALK_TENSION_MAX);
    s->beat_step = 255;
    if (s->beat == TBEAT_RIVALRY) s->arg_step = 0;
}

static int in_mask(uint16_t mask, int act) { return act > 0 && (mask >> talk_gen_act_class(act)) & 1u; }

static void select_beat(TalkState *s, const TalkUtter *u, DoomoRng *rng)
{
    int b, best = -1, bs = -1000, ba = -1, bb = -1, target = talk_dm_target_tension(s->turn);
    for (b = TBEAT_SMALLTALK; b < TBEAT_COUNT; b++) {
        TalkBeatInfo bi;
        int a, c, sc, res, j;
        if (b == s->beat || !talk_dm_beat_info(b, &bi)) continue;
        if (!pre_ok(s, bi.pre, u, &a, &c)) continue;
        if ((bi.flags & TBTF_ADULTS) && !adult_room(s)) continue;                 /* in C whatever the table says */
        if (b == TBEAT_ROMANCE_ADULT && bi.pre != TBP_ROMANCE) continue;
        if (b == TBEAT_RIVALRY && s->conv.aud.pet) continue;
        res = bi.tension == TO_ZERO ? 0 : clampi((int)s->tension + bi.tension, 0, TALK_TENSION_MAX);
        sc = 20 - 4 * absi(res - target) + bi.bonus + rng_range(rng, 3);
        if (s->beats_recent & (1u << b)) sc -= 3;
        if (s->beats_used & (1u << b)) sc -= 5;
        for (j = 0; j < 2; j++)
            if (in_mask(bi.lean, u->act[j]) || (u->act[j] && (u->act[j] == bi.lean_act[0] || u->act[j] == bi.lean_act[1])))
                sc += bi.lean_bonus;
        if (b == TBEAT_ROMANCE_ADULT) sc += 100;              /* the player's romance act, the yes and the room are all there */
        if (b == TBEAT_WIND_DOWN) sc += 20;
        if (sc > bs) { bs = sc; best = b; ba = a; bb = c; }
    }
    if (best < 0) best = TBEAT_SMALLTALK;
    start_beat(s, best, ba, bb);
}

/* ---- the plan ----------------------------------------------------------------------- */
static void plan_add(TalkState *s, int kind, int who, int to, int arg)
{
    if (s->nplan >= 6 || s->nplan >= TALK_EXCHANGE_MAX) return;
    s->plan[s->nplan].kind = (uint8_t)kind;
    s->plan[s->nplan].who = (int8_t)who;
    s->plan[s->nplan].to = (int8_t)to;
    s->plan[s->nplan].arg = (uint8_t)arg;
    s->nplan++;
}

static void alliance(TalkState *s, int y, int d)
{
    int k;
    if (!talker(s, y)) return;
    bump_rel(s, y, d);
    bump_mood(s, y, 1);
    for (k = 0; k < s->nparts; k++) {
        int rv, rk;
        if (k == y || !talker(s, k)) continue;
        rv = relv(s->part[k].id, s->part[y].id);
        rk = relk(s->part[k].id, s->part[y].id);
        if (rk == REL_RIVAL || rk == REL_NEMESIS || rv <= -10) bump_rel(s, k, -d);   /* y's opponent: the player lost a little */
        else if (rv >= 10) bump_rel(s, k, 1);                                          /* Heider: a friend of y */
    }
}

static int stance_clash(const TalkState *s, int a, int b)
{
    return s->part[a].stance != TST_NEUTRAL && s->part[b].stance != TST_NEUTRAL && s->part[a].stance != s->part[b].stance;
}

/* an argument may go past the jab: everyone here (and the player) an adult, tension high enough */
static int may_escalate(const TalkState *s)
{
    int k;
    if (s->tension < TALK_TENSION_ARGUE || !adult_room(s)) return 0;
    for (k = 0; k < s->nparts; k++) if (here(s, k) && !adult(s, k)) return 0;
    return 1;
}

static void end_argument(TalkState *s)
{
    s->arg_step = 0;
    s->arg_wait = 0;
    finish_beat(s);
}

/* the argument's step of this exchange (5.4): jab, row, ultimatum; the player's reply ends it */
static void plan_argument(TalkState *s, const TalkUtter *u, int *budget, DoomoRng *rng)
{
    int a = s->arg_a, b = s->arg_b, act = u->act[0];
    if (s->beat != TBEAT_RIVALRY || !talker(s, a) || !talker(s, b)) {
        if (s->arg_step) end_argument(s);
        return;
    }
    if (s->arg_step > 0 && s->gate == TG_PASS) {
        if (act == TA_MEDIATE) {
            int att = pers(s->part[a].id, PERS_ATTITUDE) + pers(s->part[b].id, PERS_ATTITUDE);
            int chance = clampi(35 + 3 * att + relv(s->part[a].id, s->part[b].id) / 4, 10, 95);
            if (rng_range(rng, 100) < chance) {
                if (*budget > 0) { plan_add(s, DK_MAKEUP, a, b, 0); (*budget)--; }
                add_event(s, TEV_MEDIATED, a, b);
                bump_pair(s, a, b, 3);
                bump_rel(s, a, 2);
                bump_rel(s, b, 2);
                bump_mood(s, a, 1);
                bump_mood(s, b, 1);
                s->tension = (uint8_t)clampi((int)s->tension - 3, 0, TALK_TENSION_MAX);
                end_argument(s);
                return;
            }
        } else if (act == TA_TAKE_SIDE || act == TA_DEFEND) {
            int y = s->in.target;
            if (y == a || y == b) {
                int o = y == a ? b : a;
                add_event(s, TEV_SIDE_TAKEN, y, o);
                bump_pair(s, a, b, -2);
                bump_rel(s, o, -2);
                bump_mood(s, o, -1);
                s->tension = (uint8_t)clampi((int)s->tension - 2, 0, TALK_TENSION_MAX);
                end_argument(s);
                return;
            }
        }
        s->arg_wait++;
        if (s->arg_step >= 3 || !may_escalate(s)) {
            if (s->arg_wait >= (s->arg_step >= 3 ? 2 : 3)) {  /* ignored, and it cannot grow: it fizzles out, with a walk-out */
                if (*budget > 0) { plan_add(s, DK_EXCUSE, rng_range(rng, 2) ? a : b, -1, 1); (*budget)--; }
                s->tension = (uint8_t)clampi((int)s->tension - 3, 0, TALK_TENSION_MAX);
                end_argument(s);
                return;
            }
        }
    }
    if (*budget <= 0) return;
    if (s->arg_step == 0) {
        s->arg_step = 1;
        plan_add(s, DK_JAB, a, b, 0);
        (*budget)--;
        s->tension = (uint8_t)clampi((int)s->tension + 1, 0, TALK_TENSION_MAX);
        {   /* a child asks "Are you guys fighting?" and the room cools */
            int kid = any_minor_here(s);
            if (kid >= 0 && kid != a && kid != b && *budget > 0) {
                plan_add(s, DK_KIDASK, kid, -2, 0);
                (*budget)--;
                s->tension = (uint8_t)clampi((int)s->tension - 2, 0, TALK_TENSION_MAX);
            }
        }
    } else if (s->arg_step < 3 && may_escalate(s)) {
        s->arg_step++;
        s->arg_wait = 0;
        s->tension = (uint8_t)clampi((int)s->tension + 1, 0, TALK_TENSION_MAX);
        if (s->arg_step == 2) plan_add(s, DK_ROW, b, a, 0);
        else { plan_add(s, DK_ULT, a, b, 0); add_event(s, TEV_ULTIMATUM, a, b); }
        (*budget)--;
    }
}

/* the goal line of the beat (not for a rivalry: its lines are the argument's) */
static void plan_beat_line(TalkState *s, int resp, int *budget)
{
    TalkBeatInfo bi;
    int who = -1, k;
    if (*budget <= 0 || (s->flags & TDF_NO_BEATS) || s->beat_step == 255 || s->beat == TBEAT_RIVALRY) return;
    if (!talk_dm_beat_info(s->beat, &bi) || s->beat_step >= bi.goals) return;
    if (bi.involves == TBI_A && talker(s, s->beat_a)) who = s->beat_a;
    else if (bi.involves == TBI_PAIR && talker(s, s->beat_a) && talker(s, s->beat_b))
        who = (s->beat_step & 1) ? s->beat_b : s->beat_a;
    else {
        int be = -1;
        for (k = 0; k < s->nparts; k++)                    /* everyone: the most engaged who did not just answer */
            if (talker(s, k) && k != resp && (be < 0 || s->part[k].engage > s->part[be].engage)) be = k;
        who = be >= 0 ? be : resp;
    }
    if (!talker(s, who)) return;
    plan_add(s, DK_BEAT, who, -1, 0);
    s->beat_step++;
    (*budget)--;
}

static int reaction_chance(const TalkState *s, int k, int resp)
{
    int p;
    if (is_pet(s, k)) return 15;
    p = 25 + 4 * pers(s->part[k].id, PERS_EXPRESSION) + 2 * pers(s->part[k].id, PERS_QUIRK);
    if (stance_clash(s, k, resp)) p += 15;
    return p > 75 ? 75 : p;
}

/* reactions between the residents, and one interruption now and then (5.4) */
static void plan_reactions(TalkState *s, int resp, int budget, int next_who, DoomoRng *rng)
{
    int k, last = resp, nre = 0, cut = -1;
    if (budget <= 0) return;
    /* an interruption: SPEECH >= 11, a clash or a rival speaking, engagement >= 50, both adults, once in 3 exchanges */
    if (budget >= 2 && adult(s, resp) && (!s->last_cut || (int)s->exchange - (int)s->last_cut >= TALK_INTERRUPT_GAP)) {
        for (k = 0; k < s->nparts; k++) {
            int rk;
            if (k == resp || !talker(s, k) || !adult(s, k) || s->part[k].engage < 50 || pers(s->part[k].id, PERS_SPEECH) < 11)
                continue;
            rk = relk(s->part[k].id, s->part[resp].id);
            if (stance_clash(s, k, resp) || rk == REL_RIVAL || rk == REL_NEMESIS) { cut = k; break; }
        }
        if (cut >= 0 && rng_range(rng, 100) < 60) {
            plan_add(s, DK_CUT, cut, resp, 0);
            plan_add(s, DK_GRUMBLE, resp, cut, 0);
            s->last_cut = s->exchange;
            return;
        }
    }
    for (k = 0; k < s->nparts && budget > 0 && nre < 2; k++) {
        if (k == last || k == resp || k == next_who || !here(s, k)) continue;
        if (rng_range(rng, 100) >= reaction_chance(s, k, resp) / (nre + 1)) continue;   /* the second one is rarer */
        plan_add(s, DK_REACT, k, last, nre);
        last = k;
        nre++;
        budget--;
    }
}

/* Keeping the talk going: the one who answered hands it back to the player with a follow-up (a
 * question about the topic, a remark, a way out of a spat). Always after a change of subject or a
 * snarky line (so the player can push on or let it go); else most of the time while they are
 * engaged. Never in an argument, a walk-out, a farewell or a tense room (the drama speaks then). */
static int follow_wanted(const TalkState *s, int resp, int switched, DoomoRng *rng)
{
    int i, cls, chance = 70;
    if ((s->flags & (TDF_FAREWELL | TDF_OVER)) || !talker(s, resp) || (s->part[resp].flags & TPF_LEFT)) return 0;
    if (s->tension >= TALK_TENSION_ARGUE || s->part[resp].engage < 20) return 0;
    for (i = 0; i < s->nplan; i++)
        if (s->plan[i].kind == DK_JAB || s->plan[i].kind == DK_ROW || s->plan[i].kind == DK_ULT ||
            s->plan[i].kind == DK_EXCUSE || s->plan[i].kind == DK_CUT || s->plan[i].kind == DK_GRUMBLE) return 0;
    cls = s->in.act[0] ? talk_gen_act_class(s->in.act[0]) : TAC_FLOW;
    if (switched || cls == TAC_COOL) chance = 100;
    return rng_range(rng, 100) < chance;
}

/* the one who ran out of patience walks out (5.3: under 15 for 2 turns) */
static void plan_walkout(TalkState *s, int *budget)
{
    int k, w = -1;
    if (*budget <= 0) return;
    for (k = 0; k < s->nparts; k++)
        if (talker(s, k) && s->part[k].low_turns >= 2 && (w < 0 || s->part[k].engage < s->part[w].engage)) w = k;
    if (w >= 0) { plan_add(s, DK_EXCUSE, w, -1, 0); (*budget)--; }
}

static void push_topic(TalkState *s, uint16_t t)
{
    int k;
    if (!t) return;
    for (k = 0; k < TALK_STACK_MAX - 1 && s->topic_stack[k] != t; k++) {}
    for (; k > 0; k--) s->topic_stack[k] = s->topic_stack[k - 1];
    s->topic_stack[0] = t;
}

static void hist_push(TalkState *s, int who, int act, int flags, int to, uint16_t topic, uint16_t tmpl)
{
    memmove(&s->hist[1], &s->hist[0], sizeof(s->hist[0]) * (TALK_HIST - 1));
    s->hist[0].who = (uint8_t)who;
    s->hist[0].act = (uint8_t)act;
    s->hist[0].flags = (uint8_t)flags;
    s->hist[0].to = (int8_t)to;
    s->hist[0].topic = topic;
    s->hist[0].tmpl = tmpl;
    if (s->nhist < TALK_HIST) s->nhist++;
}

/* ---- the player's turn --------------------------------------------------------------- */
static void mem_acts(TalkState *s, int act)
{
    if (act == TA_COMPLIMENT || act == TA_PRAISE_OTHER) s->mem_earned |= TMF_COMPLIMENTED;
    else if (act == TA_INSULT || act == TA_MOCK) s->mem_earned |= TMF_INSULTED;
    else if (act == TA_PROMISE) s->mem_earned |= TMF_PROMISED;
}

int talk_dm_say(TalkState *s, const TalkUtter *u)
{
    TalkAudience aud;
    DoomoRng rng;
    int k, g, target, target_id, resp, budget, act, cls, romance, switched, want = 0;
    if (!s || !u) return TG_PASS;
    s->nlines = s->nplan = s->plan_at = 0;
    s->gate = TG_PASS;
    if (s->flags & TDF_OVER) return TG_PASS;
    s->in = *u;
    for (k = 0; k < s->nparts; k++)
        if (s->part[k].flags & TPF_REFUSED) s->conv.refused |= (uint8_t)(1u << k);
    target = s->in.target >= 0 && s->in.target < s->nparts && here(s, s->in.target) ? s->in.target : -1;
    target_id = target >= 0 ? s->part[target].id : -1;
    room_audience(s, target, &aud);
    g = talk_safe_gate(&s->in, &aud, s->conv.avatar, target_id, target >= 0 && ((s->conv.refused >> target) & 1));
    s->gate = (uint8_t)g;
    s->conv.tone = s->in.tone;
    s->conv.turn++;
    s->turn++;
    s->exchange++;
    s->idle_n = 0;
    s->idle_frames = 0;
    rng_for(s, &rng, 0);
    hist_push(s, 0xFF, s->in.act[0], THF_PLAYER, target, s->in.topic[0], 0);
    act = s->in.act[0];
    mem_acts(s, act);
    if (aud.any_minor) { s->conv.aud.any_minor = 1; s->flags |= TDF_KIDSAFE; }

    if (g == TG_END_K7) {                                   /* the adults step in, briefly and with no joke; then everyone leaves */
        int pass, n = 0;
        for (pass = 0; pass < 2 && n == 0; pass++)
            for (k = 0; k < s->nparts && n < 2; k++) {
                if (!talker(s, k) || (pass == 0 && !adult(s, k))) continue;
                plan_add(s, DK_K7, k, -1, 0);
                n++;
                if (pass == 1) break;
            }
        s->flags |= TDF_OVER | TDF_K7;
        s->conv.ended = 1;
        add_event(s, TEV_K7, n ? s->plan[0].who : -1, -1);
        return g;
    }

    /* who answers */
    resp = target >= 0 ? target : pick_responder(s, s->in.topic[0], &rng, g != TG_PASS && g != TG_DEFLECT);
    if (resp >= 0 && is_pet(s, resp) && g != TG_PASS) resp = pick_responder(s, s->in.topic[0], &rng, 1);
    if (resp < 0) { s->flags |= TDF_OVER; return g; }
    plan_add(s, DK_ANSWER, resp, -1, 0);

    if (g == TG_DEFLECT && target >= 0) { s->conv.refused |= (uint8_t)(1u << target); s->part[target].flags |= TPF_REFUSED; }
    if (g == TG_HATE) {                                     /* a bond penalty (K-8) */
        add_event(s, TEV_HATE, resp, -1);
        bump_rel(s, resp, -4);
        bump_mood(s, resp, -2);
        if (resp >= 0 && s->part[resp].engage > 10) s->part[resp].engage = (uint8_t)(s->part[resp].engage - 10);
    }
    if (g == TG_DISTRESS) s->tension = (uint8_t)(s->tension > 1 ? s->tension - 1 : 0);
    if (g != TG_PASS) { s->last_act = (uint8_t)act; return g; }  /* a refusal, a deflection, distress, hate: the answer only */

    /* ---- the utterance stood ---- */
    cls = act ? talk_gen_act_class(act) : TAC_FLOW;
    switched = s->in.act[0] == TA_CHANGE_TOPIC || (s->in.topic[0] != TC_NONE && s->in.topic[0] != s->topic_stack[0]);
    push_topic(s, s->in.topic[0]);
    for (k = 0; k < s->nparts; k++) s->part[k].stance = (uint8_t)topic_stance(s->part[k].id, s->in.topic[0]);
    if (act == TA_FAREWELL) s->flags |= TDF_FAREWELL;
    romance = TA_IS_ROMANCE(act);
    if (romance) {                                         /* the target's yes or no (K-5); the sim is only asked */
        if (talk_dm_consent(s, resp, act)) {
            s->conv.consent |= (uint8_t)(1u << resp);
            add_event(s, TEV_ROMANCE_YES, resp, act);
            bump_mood(s, resp, 2);
        } else {
            s->conv.refused |= (uint8_t)(1u << resp);
            s->part[resp].flags |= TPF_REFUSED;
            add_event(s, TEV_ROMANCE_NO, resp, act);
        }
    }
    /* what the act does to the people (alliances, mood, bond) */
    if (target >= 0 && (act == TA_TAKE_SIDE || act == TA_DEFEND)) alliance(s, target, 3);
    else if (target >= 0 && act == TA_AGREE) bump_rel(s, target, 1);
    if (target >= 0 && (act == TA_COMPLIMENT || act == TA_PRAISE_OTHER || cls == TAC_WARM)) { bump_rel(s, target, 1); bump_mood(s, target, 1); }
    if (act == TA_INSULT || act == TA_MOCK || act == TA_THREATEN) {
        if (target >= 0) { bump_rel(s, target, -2); bump_mood(s, target, -2); }
        s->tension = (uint8_t)clampi((int)s->tension + 1, 0, TALK_TENSION_MAX);
    }
    update_engage(s, &s->in, target);

    /* the beat: finish it when its goals are done, pick the next by the arc */
    if (!(s->flags & TDF_NO_BEATS)) {
        TalkBeatInfo bi;
        if (s->beat_step != 255 && s->beat && talk_dm_beat_info(s->beat, &bi) && (s->beat_step >= bi.goals || s->beat_turns >= bi.turns))
            finish_beat(s);
        if (s->flags & TDF_FAREWELL) { if (s->beat != TBEAT_WIND_DOWN) start_beat(s, TBEAT_WIND_DOWN, -1, -1); }
        else if (romance && ((s->conv.consent >> resp) & 1) && adult_room(s) && s->beat != TBEAT_ROMANCE_ADULT) {
            finish_beat(s);
            start_beat(s, TBEAT_ROMANCE_ADULT, resp, -1);
        } else if (s->beat_step == 255 || !s->beat) select_beat(s, &s->in, &rng);
        /* progress of the beat this turn */
        s->beat_turns++;
        if (talk_dm_beat_info(s->beat, &bi) && s->beat_step != 255) {
            int j;
            /* a handled act counts as a goal, but one goal line is always said */
            for (j = 0; j < 2; j++)
                if (s->in.act[j] && in_mask(bi.handles, s->in.act[j])) { if (s->beat_step + 1 < bi.goals) s->beat_step++; break; }
            if (s->beat == TBEAT_PROBLEM && talker(s, s->beat_a)) {
                if (in_mask(bi.hurt, act)) {                  /* an insult to the troubled one: they are hurt, the others turn on the player */
                    s->part[s->beat_a].engage = (uint8_t)(s->part[s->beat_a].engage > 8 ? s->part[s->beat_a].engage - 8 : 0);
                    bump_rel(s, s->beat_a, -2);
                    for (k = 0; k < s->nparts; k++) if (k != s->beat_a && talker(s, k)) bump_rel(s, k, -1);
                }
                for (j = 0; j < 4; j++)
                    if (bi.resolve[j] && bi.resolve[j] == act) {   /* talked through (T6-OUT: sim_problem_resolve) */
                        s->part[s->beat_a].flags |= TPF_PROBLEM;
                        add_event(s, TEV_PROBLEM, s->beat_a, -1);
                        bump_mood(s, s->beat_a, 3);
                        bump_rel(s, s->beat_a, 2);
                        finish_beat(s);
                        break;
                    }
            }
        }
    }
    /* the tension drifts down a little; a lull is tension of 1 or less */
    if (s->turn % 3 == 0 && s->tension > 0 && s->beat != TBEAT_RIVALRY) s->tension--;
    if (cls == TAC_WARM && s->tension > 0 && s->beat != TBEAT_RIVALRY) s->tension--;
    s->lull = (uint8_t)(s->tension <= 1 ? (s->lull < 255 ? s->lull + 1 : 255) : 0);
    if (s->tension > s->peak_tension) { s->peak_tension = s->tension; s->peak_turn = s->turn; }

    /* the lines of the exchange: the answer, then the mandatory ones (the argument, the goal line, the
     * walk-out), then what room is left goes to reactions, which are said right after the answer */
    budget = TALK_EXCHANGE_MAX - s->nplan;
    if (romance) {
        if ((s->conv.consent >> resp) & 1) plan_beat_line(s, resp, &budget);   /* an adult said yes to an adult: the beat may joke */
    } else {
        int n0 = s->nplan, n1, j;
        struct { uint8_t kind; int8_t who, to; uint8_t arg; } tmp[6];
        if (!(s->flags & TDF_NO_BEATS)) plan_argument(s, &s->in, &budget, &rng);
        plan_beat_line(s, resp, &budget);
        plan_walkout(s, &budget);
        n1 = s->nplan;
        want = follow_wanted(s, resp, switched, &rng);
        plan_reactions(s, resp, TALK_EXCHANGE_MAX - s->nplan - want, n0 < n1 ? s->plan[n0].who : -1, &rng);
        if (s->nplan > n1) {                               /* move the reactions to just after the answer */
            int nr = s->nplan - n1;
            memcpy(tmp, &s->plan[n1], sizeof tmp[0] * (size_t)nr);
            for (j = n1 - 1; j >= n0; j--) s->plan[j + nr] = s->plan[j];
            memcpy(&s->plan[n0], tmp, sizeof tmp[0] * (size_t)nr);
        }
        if (want) plan_add(s, DK_FOLLOW, resp, -1, switched);       /* the talk goes back to the player */
    }
    s->last_act = (uint8_t)act;
    if (s->flags & TDF_FAREWELL) s->flags |= TDF_OVER;
    return g;
}

/* The talk opens: the beat's opening line, then a follow-up that hands it to the player. */
int talk_dm_open(TalkState *s)
{
    int k, who = -1;
    if (!s || (s->flags & TDF_OVER)) return 0;
    s->nlines = s->nplan = s->plan_at = 0;
    for (k = 0; k < s->nparts && who < 0; k++) if (talker(s, k)) who = k;
    if (who < 0) return 0;
    s->exchange++;
    s->idle_n = 0;
    s->idle_frames = 0;
    if (!(s->flags & TDF_NO_BEATS) && s->beat == TBEAT_ARRIVE && s->beat_step == 0) {
        plan_add(s, DK_BEAT, who, -1, 0);
        s->beat_step++;
    }
    plan_add(s, DK_FOLLOW, who, -1, 0);
    return 1;
}

/* ---- idle chatter (5.4) ------------------------------------------------------------- */
int talk_dm_tick(TalkState *s, int frames)
{
    DoomoRng rng;
    int k, a = -1, b = -1;
    if (!s || (s->flags & TDF_OVER) || frames <= 0) return 0;
    if (s->nplan > s->plan_at && s->nlines < TALK_EXCHANGE_MAX) return 0;      /* lines are still playing */
    if ((int)s->idle_frames + frames > 65000) s->idle_frames = 65000; else s->idle_frames = (uint16_t)(s->idle_frames + frames);
    if (s->idle_frames < TALK_IDLE_FRAMES) return 0;
    s->idle_frames = 0;
    if (s->idle_n > TALK_IDLE_CHATS) return 0;                  /* prodded once: nothing more until the player speaks */
    s->nlines = s->nplan = s->plan_at = 0;
    s->exchange++;
    rng_for(s, &rng, 0x1D1Eu + s->idle_n);
    {
        int best = -1000;
        for (k = 0; k < s->nparts; k++)                         /* the most engaged talks */
            if (talker(s, k)) {
                int sc = s->part[k].engage + rng_range(&rng, 21);
                if (sc > best) { best = sc; a = k; }
            }
    }
    if (a < 0) return 0;
    if (s->idle_n >= TALK_IDLE_CHATS) {                          /* "You're very quiet, {you}." */
        plan_add(s, DK_PROD, a, -1, 0);
        s->idle_n++;
        return 1;
    }
    /* to another resident; a rival is teased, but never when a child is one of the two */
    {
        int start = rng_range(&rng, s->nparts);
        for (k = 0; k < s->nparts; k++) {
            int c = (start + k) % s->nparts;
            if (c != a && talker(s, c)) { b = c; break; }
        }
    }
    if (b >= 0 && (!adult(s, a) || !adult(s, b))) {
        int rk = relk(s->part[a].id, s->part[b].id);
        if (rk == REL_RIVAL || rk == REL_NEMESIS) b = -1;
    }
    plan_add(s, DK_IDLE, a, b >= 0 ? b : -2, rng_range(&rng, 4));
    if (b >= 0 && s->nparts > 1 && rng_range(&rng, 100) < reaction_chance(s, b, a)) plan_add(s, DK_REACT, b, a, 0);
    s->idle_n++;
    return 1;
}

/* ---- the lines, one at a time -------------------------------------------------------- */
/* cut the line before at a word boundary and put a dash on it (5.4); 0 when it cannot be cut cleanly */
static int cut_line(TalkLine *l)
{
    int len = (int)strlen(l->text), p, j;
    if (len < 16) return 0;
    for (p = len * 2 / 3; p >= 8 && l->text[p] != ' '; p--) {}
    if (p < 8) return 0;
    for (j = 0; j < p; j++) if ((unsigned char)l->text[j] < 0x20) return 0;       /* line codes: leave it whole */
    while (p > 0 && (l->text[p - 1] == ' ' || l->text[p - 1] == ',')) p--;
    l->text[p] = '-';
    l->text[p + 1] = 0;
    l->flags |= TLF_CUT;
    return 1;
}

static int romance_line_ok(const TalkState *s, int who)
{
    return talker(s, who) && adult(s, who) && adult_room(s) && ((s->conv.consent >> who) & 1) &&
           talk_safe_romance_ok(&s->conv.aud, s->conv.avatar, s->part[who].id);
}

static int make_line(TalkState *s, int kind, int who, int to, int arg, TalkLine *L, TalkTrace *tr, int *hflags)
{
    TalkConv *c = &s->conv;
    uint16_t topic = s->in.topic[0] ? s->in.topic[0] : s->topic_stack[0];
    int ok = 0;
    if (!talker(s, who) && !(kind == DK_ANSWER && here(s, who))) return 0;
    switch (kind) {
    case DK_ANSWER:
        *hflags = 0;
        ok = talk_gen_line(c, who, to, s->in.act[0], s->in.topic[0], L, tr);
        if (TA_IS_ROMANCE(L->act) && !(TA_IS_ROMANCE(s->in.act[0]) && s->gate == TG_PASS && ((c->consent >> who) & 1))) ok = 0;
        break;
    case DK_REACT:
    case DK_CUT: {
        TalkLine *prev;
        if (s->nlines == 0) return 0;
        prev = &s->line[s->nlines - 1];
        if (prev->who == who) return 0;
        if (kind == DK_REACT && arg > 0 && (s->line[0].flags & TLF_SINCERE)) return 0;     /* K-10: one comfort line, no pile-on */
        if (kind == DK_CUT) {
            TalkLine keep = *prev;
            if (!cut_line(prev)) { if (s->plan_at < s->nplan) s->plan[s->plan_at].kind = DK_NONE; return 0; }
            ok = talk_gen_react(c, who, prev, topic, L, tr);
            if (!ok && !L->text[0]) { *prev = keep; if (s->plan_at < s->nplan) s->plan[s->plan_at].kind = DK_NONE; return 0; }
            *hflags = THF_INTERRUPT | THF_REACT;
        } else {
            ok = talk_gen_react(c, who, prev, topic, L, tr);
            *hflags = THF_REACT;
        }
        break;
    }
    case DK_BEAT:
        if (s->beat == TBEAT_ROMANCE_ADULT && !romance_line_ok(s, who)) return 0;
        ok = talk_gen_entry(c, who, to, TGE_BEAT, TA_NONE, topic, L, tr);
        *hflags = THF_BEAT;
        break;
    case DK_IDLE: {
        DoomoRng r;
        uint16_t t = 0;
        rng_for(s, &r, 0x7C0Fu + (uint32_t)who);
        if (arg < 3) t = talk_gen_like(s->part[who].id, arg);
        if (!t) t = s->topic_stack[0];
        ok = talk_gen_entry(c, who, to, TGE_IDLE, TA_NONE, t, L, tr);
        *hflags = THF_IDLE;
        break;
    }
    case DK_FOLLOW:
        ok = talk_gen_entry(c, who, -1, TGE_FOLLOW, arg ? TA_CHANGE_TOPIC : s->in.act[0], topic, L, tr);
        if (!ok || (L->flags & TLF_FALLBACK)) return 0;
        *hflags = 0;
        break;
    case DK_PROD:   ok = talk_gen_entry(c, who, -1, TGE_PROD, TA_NONE, TC_NONE, L, tr); *hflags = THF_IDLE; break;
    case DK_JAB:    ok = talk_gen_entry(c, who, to, TGE_JAB, TA_NONE, topic, L, tr); *hflags = THF_ARGUE; break;
    case DK_ROW:    ok = talk_gen_entry(c, who, to, TGE_ROW, TA_NONE, topic, L, tr); *hflags = THF_ARGUE; break;
    case DK_ULT:    ok = talk_gen_entry(c, who, to, TGE_ULTIMATUM, TA_NONE, topic, L, tr); *hflags = THF_ARGUE; break;
    case DK_MAKEUP: ok = talk_gen_entry(c, who, to, TGE_MAKEUP, TA_NONE, topic, L, tr); *hflags = 0; break;
    case DK_GRUMBLE: ok = talk_gen_entry(c, who, to, TGE_GRUMBLE, TA_NONE, topic, L, tr); *hflags = 0; break;
    case DK_KIDASK: ok = talk_gen_entry(c, who, -2, TGE_KIDASK, TA_NONE, TC_NONE, L, tr); *hflags = 0; break;
    case DK_EXCUSE: ok = talk_gen_entry(c, who, -1, TGE_EXCUSE, TA_FAREWELL, TC_NONE, L, tr); *hflags = THF_LEAVE; break;
    case DK_K7:
        ok = talk_gen_entry(c, who, -1, TGE_END_K7, TA_REFUSE_SEXUAL, TC_NONE, L, tr);
        L->act = TA_REFUSE_SEXUAL;
        L->flags |= TLF_END | TLF_SINCERE;
        *hflags = THF_SINCERE;
        break;
    default: return 0;
    }
    if (!ok && !L->text[0]) return 0;
    if (kind != DK_ANSWER && kind != DK_BEAT && TA_IS_ROMANCE(L->act)) return 0;         /* one filter: no romance line is ever started here */
    if (kind == DK_BEAT && TA_IS_ROMANCE(L->act) && !romance_line_ok(s, who)) return 0;
    L->who = (int8_t)who;
    L->to = (int8_t)to;
    if (L->flags & TLF_SINCERE) *hflags |= THF_SINCERE;
    return 1;
}

int talk_dm_step(TalkState *s, TalkTrace *tr)
{
    if (!s) return 0;
    while (s->plan_at < s->nplan && s->nlines < TALK_EXCHANGE_MAX) {
        int kind = s->plan[s->plan_at].kind, who = s->plan[s->plan_at].who, to = s->plan[s->plan_at].to,
            arg = s->plan[s->plan_at].arg, hf = 0;
        TalkLine *L = &s->line[s->nlines];
        s->plan_at++;
        memset(L, 0, sizeof *L);
        if (kind == DK_NONE || !make_line(s, kind, who, to, arg, L, tr, &hf)) continue;
        if (tr && kind != DK_ANSWER) { /* the trace is the caller's one slot: only the line just made */ }
        s->nlines++;
        if (who >= 0 && who < s->nparts) { s->part[who].flags |= TPF_SPOKE; s->conv.spoke |= (uint8_t)(1u << who); }
        hist_push(s, who, L->act, hf, to, L->topic, 0);
        if (kind == DK_EXCUSE) {                            /* walks out */
            s->part[who].flags |= TPF_LEFT;
            add_event(s, TEV_WALKOUT, who, -1);
            if (s->arg_step && (who == s->arg_a || who == s->arg_b)) end_argument(s);
            if (active_residents(s) == 0) s->flags |= TDF_OVER;
        }
        return 1;
    }
    return 0;
}

/* ---- the end ------------------------------------------------------------------------- */
void talk_dm_end(TalkState *s, TalkSummary *out)
{
    int k, n = 0, left = 0, sum = 0, great;
    if (!s || !out) return;
    memset(out, 0, sizeof *out);
    for (k = 0; k < s->nparts; k++)
        if (talker(s, k)) { n++; sum += s->part[k].engage; }
        else if (!is_pet(s, k)) left++;
    great = n > 0 && !left && s->turn >= 8 && sum / n >= 60 && s->peak_tension <= TALK_TENSION_ARGUE + 2 &&
            !(s->flags & TDF_K7);
    for (k = 0; great && k < s->nev; k++)
        if (s->ev[k].kind == TEV_GREAT_NIGHT || s->ev[k].kind == TEV_HATE || s->ev[k].kind == TEV_ULTIMATUM) great = 0;
    if (great) add_event(s, TEV_GREAT_NIGHT, -1, -1);
    s->flags |= TDF_OVER;                                   /* conv.ended stays K-7's: talk_mem_end locks the room for the day on it */
    out->avatar = s->conv.avatar;
    out->n = s->nparts;
    out->turns = s->turn;
    out->peak_tension = s->peak_tension;
    out->flags = s->flags;
    for (k = 0; k < s->nparts; k++) {
        out->id[k] = s->part[k].id;
        out->engage[k] = s->part[k].engage;
        out->pflags[k] = s->part[k].flags;
        out->mood_d[k] = (int8_t)clampi(s->part[k].mood_d, -10, 10);
        out->rel_d_player[k] = (int8_t)clampi(s->part[k].rel_d_player, -TALK_REL_D_MAX, TALK_REL_D_MAX);
    }
    for (k = 0; k < 6; k++) out->pair_d[k] = (int8_t)clampi(s->pair_d[k], -TALK_PAIR_D_MAX, TALK_PAIR_D_MAX);
    out->beats_used = s->beats_used;
    out->topic[0] = s->topic_stack[0];
    out->topic[1] = s->topic_stack[1];
    out->mem_flags = s->mem_earned;
    out->nev = s->nev;
    memcpy(out->ev, s->ev, sizeof out->ev);
}
