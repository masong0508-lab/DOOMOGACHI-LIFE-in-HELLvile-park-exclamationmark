/* talk_gen.c - Talk T5a: the generator (owner: TK; docs/TALK.md "The
 * generator"; design 3.6). The grammar VM, guards, fillers, sim-fact
 * callbacks, novelty, re-picks, the bubble fit, the per-conversation RNG,
 * the trace for the quality checks Q1-Q4, and a plain exchange (the gate,
 * who answers, reactions between residents) that T6's drama manager will
 * steer.
 *
 * It writes only from the database: template strings (TB_STRINGS) chosen
 * by the grammar (TB_GRAMMAR, core/tools/talkc_gen.py has the format) and
 * concept forms (TB_FORMS). The player's words are never copied.
 *
 * SAFETY (docs/TALK.md "Safety"):
 *   - talk_gen_exchange() runs talk_safe_gate() on the utterance before
 *     anything is written; a K-7 input ends the conversation;
 *   - every line goes through talk_safe_lint() (S3); a hit replaces it with
 *     a `safe` line and counts (a test fails on any count);
 *   - a concept that may not be said in this room is never filled in
 *     (talk_gen_concept_ok: never-generated, sexual / hate classes, and
 *     adults-only or register words with a child here);
 *   - a name the player typed (a custom resident) is checked with the
 *     lint's probe before it goes in a line;
 *   - the grammar's guards: romance lines need adults_only & !kid_present
 *     (talkc lints TK1-TK9 refuse a bank without them), an everyday swear
 *     needs a blunt adult and is said once a conversation at most (K-9).
 *
 * Deterministic: a local DoomoRng per line, seeded from the conversation
 * seed and the line count, so the text is the same on every platform.
 * No stdio, no malloc, no floats. RAM: nothing static; about 1.5 KB of
 * stack per line (the part buffers and one decoded template per nesting
 * level, at most 4).
 */
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/sim.h"
#include "doomo/roster.h"
#include "doomo/family.h"
#include "doomo/story.h"
#include "doomo/resident_acc.h"
#include "doomo/portrait.h"

#define GR_MAGIC   0x31524754u               /* "TGR1" */
#define GR_HDR     64u
#define NO_SYM     0xFFFFu
#define MAX_DEPTH  4
#define PART_MAX   100
#define TRIES      4

/* talkc_gen.py RF_* / AF_* / CC_* */
#define RF_KID 1u
#define RF_REFUSAL 2u
#define RF_ROMANCE 4u
#define RF_SINCERE 8u
#define RF_CAST 16u
#define RF_TIC 32u
#define RF_LOWER 64u                         /* a lower-case voice (Wisp, Vapula): the line as written */
#define AF_SWEAR 1u
#define AF_REGISTER 2u
#define AF_JMAT 4u
#define AF_CAST 8u
#define AF_TOPIC 16u
#define AF_LIKE 32u
#define AF_TIC 64u
#define AF_CATCH 128u
#define CC_REGISTER 1u
#define CC_SWEAR 2u
#define CC_FORBID 4u

typedef struct GHdr {
    uint16_t nrules, nalts, nterms, nsyms, nrefs, ncast, njobs, nacts;
    uint32_t rules, alts, terms, syms, refs, entry, acts, map, cast, jobs;
    uint16_t nfixed;
} GHdr;

/* the map section: u16 food[48], hobby[32], music[16], fear[24], u8 catch[64] */
#define MAP_FOOD  0
#define MAP_HOBBY 48
#define MAP_MUSIC 80
#define MAP_FEAR  96
#define MAP_CATCH 240                         /* byte offset of the catchphrase classes */

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int ghdr(GHdr *h)
{
    uint8_t b[GR_HDR];
    if (!talk_db_read(TB_GRAMMAR, 0, b, GR_HDR) || le32(b) != GR_MAGIC) return 0;
    h->nrules = le16(b + 4); h->nalts = le16(b + 6); h->nterms = le16(b + 8); h->nsyms = le16(b + 10);
    h->nrefs = le16(b + 12); h->ncast = le16(b + 14); h->njobs = le16(b + 16); h->nacts = le16(b + 18);
    h->rules = le32(b + 20); h->alts = le32(b + 24); h->terms = le32(b + 28); h->syms = le32(b + 32);
    h->refs = le32(b + 36); h->entry = le32(b + 40); h->acts = le32(b + 44); h->map = le32(b + 48);
    h->cast = le32(b + 52); h->jobs = le32(b + 56); h->nfixed = le16(b + 60);
    return h->nacts == TA_COUNT && h->nfixed == TGE_COUNT;
}

static int g_u16(uint32_t off, uint16_t *v)
{
    uint8_t b[2];
    if (!talk_db_read(TB_GRAMMAR, off, b, 2)) return 0;
    *v = le16(b);
    return 1;
}

int talk_gen_ready(void)
{
    GHdr h;
    uint32_t o, p, r;
    return talk_db_is_open() && ghdr(&h) && talk_db_bank(TB_STRINGS, &o, &p, &r);
}

/* ---- the acts table ---------------------------------------------------- */
static int act_row(int act, uint8_t row[16])
{
    GHdr h;
    if (act <= 0 || act >= TA_COUNT || !ghdr(&h)) return 0;
    return talk_db_read(TB_GRAMMAR, h.acts + 16u * (uint32_t)act, row, 16);
}

int talk_gen_adjacent(int act, int reply)
{
    uint8_t row[16];
    if (reply <= 0 || reply >= TA_COUNT || !act_row(act, row)) return 0;
    return (le32(row + 4 * (reply >> 5)) >> (reply & 31)) & 1u;
}

int talk_gen_act_class(int act)
{
    uint8_t row[16];
    return act_row(act, row) ? row[12] : TAC_FLOW;
}

int talk_gen_cast_id(int i)
{
    GHdr h;
    uint16_t v;
    if (!ghdr(&h) || i < 0 || i >= h.ncast || !g_u16(h.cast + 2u * (uint32_t)i, &v)) return -1;
    return v;
}

int talk_gen_nalts(void)
{
    GHdr h;
    return ghdr(&h) ? h.nalts : 0;
}

/* ---- who is who ---------------------------------------------------------- */
/* the resident record of an id: a town slot, the hero, or a pool member */
static const DoomoResident *res_of(int id, DoomoResident *buf)
{
    if (id >= 0 && id <= DOOMO_PLAYER_ID) {
        const DoomoResident *r = town_res(id);
        return r && r->status != RES_EMPTY ? r : NULL;
    }
    if (id > DOOMO_PLAYER_ID && id != TALK_PET_ID && family_resident(id, buf)) return buf;
    return NULL;
}

static const char *name_of(int id, DoomoResident *buf)
{
    const DoomoResident *r;
    if (id == TALK_PET_ID) return doomo_town.pet.name[0] ? doomo_town.pet.name : "Boreagotchi";
    if (id > DOOMO_PLAYER_ID) return family_name(id);
    r = res_of(id, buf);
    return r ? r->name : "";
}

uint16_t talk_gen_like(int id, int which)
{
    GHdr h;
    DoomoResident buf;
    const DoomoResident *r = res_of(id, &buf);
    uint16_t v = 0;
    uint32_t at;
    if (!r || !ghdr(&h)) return 0;
    switch (which) {
    case 0: at = MAP_FOOD + RES_LIKE_FOOD(r) % FOOD_COUNT; break;
    case 1: at = MAP_HOBBY + RES_LIKE_HOBBY(r) % HOBBY_COUNT; break;
    case 2: at = MAP_MUSIC + RES_LIKE_MUSIC(r) % MUSIC_COUNT; break;
    case 3: at = MAP_FEAR + RES_FEAR(r) % FEAR_COUNT; break;
    case 4: at = MAP_FOOD + RES_HATE_FOOD(r) % FOOD_COUNT; break;
    default: return 0;
    }
    if (!g_u16(h.map + 2u * at, &v)) return 0;
    return v;
}

int talk_gen_concept_ok(uint16_t concept, const TalkAudience *aud)
{
    TalkConcept c;
    uint8_t parent = 0, flags = 0;
    if (!concept || !talk_db_concept(concept, &c) || !aud) return 0;
    if (c.age == TAGE_NEVER || c.reg >= TR_SEXUAL) return 0;
    if (!talk_db_cat(c.cat, &parent, &flags) || (flags & (TCAT_SAFETY))) return 0;
    if (aud->any_minor || aud->avatar_minor || aud->target_minor) {
        if (c.age != TAGE_ANY || c.reg != TR_CLEAN || (flags & TCAT_GATED)) return 0;
    }
    return 1;
}

static int cat_is(int cat, const char *key)
{
    char n[24];
    return talk_db_cat_name(cat, n, (int)sizeof n) > 0 && strcmp(n, key) == 0;
}

int talk_gen_stance(int id, uint16_t topic)
{
    TalkConcept c;
    DoomoResident buf;
    const DoomoResident *r = res_of(id, &buf);
    int s, k;
    if (!topic || !talk_db_concept(topic, &c)) return TST_NEUTRAL;
    s = c.sent;
    for (k = 0; k < 3; k++) if (talk_gen_like(id, k) == topic) s = 3;
    if (talk_gen_like(id, 3) == topic || talk_gen_like(id, 4) == topic) s = -3;
    if (r && ((r->flags & RF_HELLBORN) || (id < DOOMO_MAX_RESIDENTS && RES_DISTRICT(id) == DISTRICT_HELLVILE))) {
        if (cat_is(c.cat, "lore.church")) s = -s;            /* holy things */
        else if (talk_db_cat_root(c.cat) == TCR_HELLVILE) s += 1;
    }
    s += (int)(doomo_hash32((uint32_t)id * 7919u + topic * 104729u) % 7u) - 3;   /* a fixed personal lean */
    return s >= 2 ? TST_PRO : s <= -2 ? TST_CON : TST_NEUTRAL;
}

static int days_to_bday(const DoomoResident *r)
{
    int m, d, bm = RES_BDAY_MONTH(r) % 12, bd = RES_BDAY_DAY(r);
    sim_date(&m, &d);
    return ((bm - m + 12) % 12) * 28 + (bd - d) < 0 ? ((bm - m + 12) % 12) * 28 + (bd - d) + 336
                                                    : ((bm - m + 12) % 12) * 28 + (bd - d);
}

/* is resident id named in the record (three 10-bit ids) of a template about people */
static int news_about(const DoomoNewsRec *n, int id)
{
    switch (n->tmpl) {
    case NT_BIRTHDAY_HELL: case NT_BIRTHDAY_HILL: case NT_POSSESSED: case NT_EXORCISM_OK: case NT_EXORCISM_FAIL:
    case NT_GHOST: case NT_DATING_GRUMBLE: case NT_DATING_CROSS: case NT_DATING: case NT_WED_GRUMBLE:
    case NT_WED_CROSS: case NT_WED: case NT_BIG_FIGHT: case NT_REKINDLED: case NT_NEMESES: case NT_RIVALS:
    case NT_BFF_CROSS: case NT_BFF: case NT_FRIENDS_CROSS: case NT_TALK_ROW: case NT_TALK_MADE_UP:
    case NT_TALK_GREAT_NIGHT:
        return (int)(n->v & 0x3FFu) == id || (int)((n->v >> 10) & 0x3FFu) == id || (int)((n->v >> 20) & 0x3FFu) == id;
    default:
        return 0;
    }
}

int talk_gen_fact(int id, int cb, uint16_t *val)
{
    DoomoResident buf;
    const DoomoResident *r = res_of(id, &buf);
    int minor = talk_safe_minor(id), i;
    uint16_t v = 0;
    GHdr h;
    if (!r) return 0;
    switch (cb) {
    case TCB_JOB:
        if (minor || id >= DOOMO_MAX_RESIDENTS || id >= ROSTER_COUNT || (r->flags & RF_CUSTOM) || !ghdr(&h) ||
            id >= h.njobs || !g_u16(h.jobs + 2u * (uint32_t)id, &v) || !v)
            return 0;
        break;
    case TCB_PARTNER:
        if (minor || id > DOOMO_PLAYER_ID || (i = sim_partner_of(id)) < 0) return 0;
        v = (uint16_t)i;
        break;
    case TCB_PET:
        if (!(r->flags & RF_HAS_PET)) return 0;
        v = 1;
        break;
    case TCB_NEWS:
        if (id >= DOOMO_MAX_RESIDENTS) return 0;
        for (i = 0; i < sim_news_count(); i++) {
            const DoomoNewsRec *n = sim_news_record(i);
            if (n && n->day + 7u >= (uint32_t)doomo_town.day && n->day <= doomo_town.day && news_about(n, id)) {
                v = n->tmpl;
                break;
            }
        }
        if (!v) return 0;
        break;
    case TCB_PROBLEM: {
        const DoomoProblem *p;
        if (id >= DOOMO_MAX_RESIDENTS || (i = sim_problem_for(id)) < 0 || !(p = sim_problem(i))) return 0;
        if (minor && (p->kind == PROB_CRUSH || p->kind == PROB_CONFESS_HELP || p->kind == PROB_BREAKUP ||
                      p->kind == PROB_PROPOSAL))
            return 0;
        v = p->kind;
        break;
    }
    case TCB_BDAY:
        i = days_to_bday(r);
        if (i > 7) return 0;
        v = (uint16_t)i;
        break;
    case TCB_GHOST:     if (r->status != RES_GHOST) return 0; v = 1; break;
    case TCB_POSSESSED: if (r->status != RES_POSSESSED) return 0; v = 1; break;
    case TCB_HELLBORN:
        if (!((r->flags & RF_HELLBORN) || (id < DOOMO_MAX_RESIDENTS && RES_DISTRICT(id) == DISTRICT_HELLVILE)))
            return 0;
        v = 1;
        break;
    case TCB_NIGHT:     if (!town_is_night()) return 0; v = 1; break;
    case TCB_FAMILY:
        if (family_role(id) == FAMROLE_NONE) return 0;
        v = (uint16_t)family_role(id);
        break;
    case TCB_CHAPTER:
        i = story_chapter();
        if (i < STORY_CH1 || i > STORY_CH4) return 0;
        v = (uint16_t)i;
        break;
    default:
        return 0;
    }
    if (val) *val = v;
    return 1;
}

/* ---- the per-line context --------------------------------------------- */
typedef struct Gen {
    GHdr h;
    TalkConv *c;
    DoomoRng rng;
    int who, to, act, sid, to_id, other;
    const DoomoResident *r;
    DoomoResident rbuf;
    uint8_t minor, stage, type, mood, rel, stance, cast, prob, mem_ok;
    uint8_t pers[PERS_COUNT];
    uint16_t topic, like_pick, job;
    uint16_t like[5];
    TalkConcept tc;
    int partner;
    int8_t pc[TGP_FIRST_VALUED];  /* boolean predicates already worked out, -1 not yet */
    /* the part being written */
    char *buf;
    int len, cap, fail, depth;
    TalkTrace *tr;
    TalkTrace part_tr;            /* fills of the part being written */
} Gen;

static void push_fill(Gen *g, int off, int len, int kind, int cat)
{
    TalkTrace *t = &g->part_tr;
    if (t->nfill < TALK_TRACE_FILLS && len > 0 && off < 255) {
        t->fill[t->nfill].off = (uint8_t)off;
        t->fill[t->nfill].len = (uint8_t)(len > 255 ? 255 : len);
        t->fill[t->nfill].kind = (uint8_t)kind;
        t->fill[t->nfill].cat = (uint8_t)cat;
        t->nfill++;
    }
}

static void push_said(Gen *g, uint16_t concept)
{
    TalkTrace *t = &g->part_tr;
    int i;
    if (!concept) return;
    for (i = 0; i < t->nsaid; i++) if (t->said[i] == concept) return;
    if (t->nsaid < 4) t->said[t->nsaid++] = concept;
}

static void push_alt(Gen *g, int alt)
{
    TalkTrace *t = &g->part_tr;
    if (t->nalt < TALK_TRACE_ALTS) t->alt[t->nalt++] = (uint16_t)alt;
}

static void emit(Gen *g, const char *s, int n)
{
    if (n < 0) n = (int)strlen(s);
    if (g->len + n >= g->cap) { g->fail = 1; return; }
    memcpy(g->buf + g->len, s, (size_t)n);
    g->len += n;
    g->buf[g->len] = 0;
}

static int band_mood(int mood)
{
    switch (mood) {
    case MOOD_HAPPY: case MOOD_ECSTATIC: return TMB_HAPPY;
    case MOOD_SAD: case MOOD_LONELY: case MOOD_LOVESICK: return TMB_SAD;
    case MOOD_ANGRY: return TMB_ANGRY;
    case MOOD_SCARED: return TMB_SCARED;
    case MOOD_HUNGRY: case MOOD_SLEEPY: case MOOD_SICK: case MOOD_FILTHY: return TMB_TIRED;
    case MOOD_CORRUPTED: case MOOD_POSSESSED: return TMB_WEIRD;
    default: return TMB_OK;
    }
}

static int band_rel(int a, int b)
{
    int k, v;
    if (a < 0 || b < 0) return TRB_STRANGER;
    if (b == DOOMO_PLAYER_ID && family_role(a) != FAMROLE_NONE) return TRB_FAMILY;
    k = rel_kind(a, b);
    v = rel_value(a, b);
    if (k == REL_DATING || k == REL_MARRIED) return TRB_PARTNER;
    if (k == REL_FAMILY) return TRB_FAMILY;
    if (k == REL_RIVAL || k == REL_NEMESIS || v < -30) return TRB_RIVAL;
    if (k == REL_BEST_FRIEND) return TRB_BEST;
    if (k == REL_FRIEND) return TRB_FRIEND;
    if (k >= 0 || v > 0) return TRB_ACQUAINTANCE;
    return TRB_STRANGER;
}

static int tod_band(void)
{
    int hr = (doomo_town.minute / 60) % 24;
    return hr < 6 || hr >= 21 ? TTOD_NIGHT : hr < 12 ? TTOD_MORNING : hr < 17 ? TTOD_DAY : TTOD_EVENING;
}

static int attr_pick(Gen *g, uint16_t cid, int want, uint16_t *out)
{
    uint16_t ids[TALK_ATTR_MAX];
    int n = talk_db_attrs(cid, ids), i, k = 0, pick = -1;
    for (i = 0; i < n; i++) {
        TalkConcept a;
        if (!talk_db_concept(ids[i], &a) || talk_db_cat_root(a.cat) != TCR_DESCRIBE) continue;
        if (!talk_gen_concept_ok(ids[i], &g->c->aud)) continue;
        if (want > 0 && a.sent <= 0) continue;
        if (want < 0 && a.sent >= 0) continue;
        k++;
        if (!out) return 1;
        if (rng_range(&g->rng, k) == 0) pick = i;
    }
    if (pick < 0) return 0;
    *out = ids[pick];
    return 1;
}

static int assoc_pick(Gen *g, uint16_t cid, uint16_t *out)
{
    uint16_t ids[TALK_ASSOC_TOP];
    uint8_t w[TALK_ASSOC_TOP];
    int n = talk_db_assoc(cid, ids, w), i, k = 0, pick = -1;
    for (i = 0; i < n; i++) {
        TalkConcept a;
        uint8_t fl = 0;
        if (ids[i] == cid || !talk_db_concept(ids[i], &a) || !talk_db_cat(a.cat, NULL, &fl) ||
            !(fl & TCAT_TALKABLE) || !talk_gen_concept_ok(ids[i], &g->c->aud))
            continue;
        k++;
        if (!out) return 1;
        if (rng_range(&g->rng, k) == 0) pick = i;
    }
    if (pick < 0) return 0;
    *out = ids[pick];
    return 1;
}

/* another participant for {other} and the hand-off: not the speaker, not the
 * one addressed, not the pet; one of them at random */
static int other_slot(Gen *g)
{
    int k, n = 0, pick = -1;
    for (k = 0; k < g->c->n; k++)
        if (k != g->who && k != g->to && g->c->id[k] != TALK_PET_ID && rng_range(&g->rng, ++n) == 0) pick = k;
    return pick;
}

static int is_fact_pred(int p);

/* ---- guards -------------------------------------------------------------- */
static int pred_raw(Gen *g, int p, int v);

static int pred(Gen *g, int p, int v)
{
    int r;
    if (p > 0 && p < TGP_FIRST_VALUED) {
        if (g->pc[p] >= 0) return g->pc[p];
        r = pred_raw(g, p, v);
        g->pc[p] = (int8_t)(r != 0);
        return r != 0;
    }
    return pred_raw(g, p, v);
}

static int pred_raw(Gen *g, int p, int v)
{
    TalkConv *c = g->c;
    switch (p) {
    case TGP_KID_PRESENT: return c->aud.any_minor || c->aud.avatar_minor || c->aud.target_minor;
    case TGP_PET: return c->aud.pet;
    case TGP_ADULTS_ONLY:
        return !c->aud.any_minor && !c->aud.avatar_minor && !g->minor &&
               (g->to_id < 0 || g->to_id == TALK_PET_ID || !talk_safe_minor(g->to_id));
    case TGP_JMAT: case TGP_MATURE_ADULT_OK: {
        int ids[TALK_PARTS_MAX + 1], n = 0, k;
        if (!c->aud.mature_ok) return 0;
        for (k = 0; k < c->n; k++) if (c->id[k] != TALK_PET_ID) ids[n++] = c->id[k];
        ids[n++] = c->avatar;
        return talk_safe_jmat_ok(ids, n);
    }
    case TGP_KID_SPEAKER: return g->minor;
    case TGP_TEEN: return g->stage == LIFE_TEEN;
    case TGP_ELDER: return g->stage == LIFE_ELDER;
    case TGP_POLITE: return g->pers[PERS_SPEECH] <= 5;
    case TGP_BLUNT: return g->pers[PERS_SPEECH] >= 10;
    case TGP_FLAT: return g->pers[PERS_EXPRESSION] <= 5;
    case TGP_LOUD: return g->pers[PERS_EXPRESSION] >= 10;
    case TGP_SERIOUS: return g->pers[PERS_ATTITUDE] <= 4;
    case TGP_EASYGOING: return g->pers[PERS_ATTITUDE] >= 11;
    case TGP_QUIRKY: return g->pers[PERS_QUIRK] >= 11;
    case TGP_QUICK: return g->pers[PERS_MOVEMENT] >= 11;
    case TGP_SLOW: return g->pers[PERS_MOVEMENT] <= 4;
    case TGP_HELLBORN: return talk_gen_fact(g->sid, TCB_HELLBORN, NULL);
    case TGP_GHOST: return g->r && g->r->status == RES_GHOST;
    case TGP_POSSESSED: return g->r && g->r->status == RES_POSSESSED;
    case TGP_CAST: return g->cast != 0xFF;
    case TGP_TOPIC: return g->topic != 0;
    case TGP_TO_PLAYER: return g->to == -1;
    case TGP_TO_RES: return g->to >= 0 && g->to < c->n;
    case TGP_OTHER: return g->other >= 0;
    case TGP_GROUP: {
        int k, n = 0;
        for (k = 0; k < c->n; k++) if (c->id[k] != TALK_PET_ID) n++;
        return n >= 2;
    }
    case TGP_CONSENT: return g->who >= 0 && ((c->consent >> g->who) & 1);
    case TGP_CATCH: {
        uint8_t cls = 0;
        if (!g->r || ((c->catch_used >> g->who) & 1)) return 0;
        if (!talk_db_read(TB_GRAMMAR, g->h.map + MAP_CATCH + RES_CATCHPHRASE(g->r) % CATCHPHRASE_COUNT, &cls, 1))
            return 0;
        if (cls & CC_FORBID) return 0;
        if ((cls & CC_REGISTER) && (pred(g, TGP_KID_PRESENT, 0) || g->minor)) return 0;
        if ((cls & CC_SWEAR) && (c->swears || !pred(g, TGP_BLUNT, 0) || !pred(g, TGP_ADULTS_ONLY, 0))) return 0;
        return 1;
    }
    case TGP_NIGHT: return town_is_night();
    case TGP_LIKES_TOPIC: return g->topic && (g->topic == g->like[0] || g->topic == g->like[1] || g->topic == g->like[2]);
    case TGP_FEARS_TOPIC: return g->topic && g->topic == g->like[3];
    case TGP_HATES_TOPIC: return g->topic && g->topic == g->like[4];
    case TGP_FIRST: return !((c->spoke >> g->who) & 1);
    case TGP_FACT_JOB: return g->job != 0;
    case TGP_FACT_PARTNER: return g->partner >= 0;
    case TGP_FACT_PET: return talk_gen_fact(g->sid, TCB_PET, NULL);
    case TGP_FACT_NEWS: return talk_gen_fact(g->sid, TCB_NEWS, NULL);
    case TGP_FACT_PROBLEM: return g->prob != PROB_NONE;
    case TGP_FACT_BDAY: return talk_gen_fact(g->sid, TCB_BDAY, NULL);
    case TGP_FACT_MEMORY: return g->mem_ok;
    case TGP_FACT_COMPLIMENTED: return (c->mem_flags[g->who] & TMF_COMPLIMENTED) != 0;
    case TGP_FACT_INSULTED: return (c->mem_flags[g->who] & TMF_INSULTED) != 0;
    case TGP_FACT_PROMISED: return (c->mem_flags[g->who] & TMF_PROMISED) != 0;
    case TGP_FACT_FAMILY: return talk_gen_fact(g->sid, TCB_FAMILY, NULL);
    case TGP_X_COUNTABLE: return g->topic && (g->tc.flags & TCF_COUNTABLE);
    case TGP_X_PROPER: return g->topic && (g->tc.flags & TCF_PROPER);
    case TGP_X_MASS: return g->topic && (g->tc.flags & TCF_MASS);
    case TGP_X_PLURAL: return g->topic && (g->tc.flags & TCF_PLURAL);
    case TGP_X_GERUND: return g->topic && (g->tc.flags & TCF_GERUND);
    case TGP_X_PERSON:
        return g->topic && (talk_db_cat_root(g->tc.cat) == TCR_PEOPLE || cat_is(g->tc.cat, "lore.cast"));
    case TGP_X_ADJ: return g->topic && attr_pick(g, g->topic, 0, NULL);
    case TGP_X_GOOD: return g->topic && attr_pick(g, g->topic, 1, NULL);
    case TGP_X_BAD: return g->topic && attr_pick(g, g->topic, -1, NULL);
    case TGP_X_ASSOC: return g->topic && assoc_pick(g, g->topic, NULL);
    case TGP_STANCE: return g->stance == v;
    case TGP_SPEAKER: return g->cast == v;
    case TGP_TYPE: return g->type == v;
    case TGP_MOOD: return g->mood == v;
    case TGP_REL: return g->rel == v;
    case TGP_ROOT: return g->topic && talk_db_cat_root(g->tc.cat) == v;
    case TGP_CHAPTER: return c->chapter == v;
    case TGP_TIME: return tod_band() == v;
    case TGP_BEAT: return c->beat == v;
    case TGP_TONE: return c->tone == v;
    case TGP_PACT: return talk_gen_act_class(g->act) == v;
    case TGP_ACT: return g->act == v;
    case TGP_PROB: return g->prob == v;
    default: return 0;
    }
}

static int terms_ok(Gen *g, uint16_t first, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        uint8_t t[2];
        int r;
        if (!talk_db_read(TB_GRAMMAR, g->h.terms + 2u * (uint32_t)(first + i), t, 2)) return 0;
        r = pred(g, t[0] & 0x7F, t[1]);
        if ((t[0] & 0x80) ? r : !r) return 0;
    }
    return 1;
}

typedef struct GRule { uint16_t alt_first; uint8_t nalt, act; uint16_t term_first; uint8_t nterm, flags; } GRule;
typedef struct GAlt { uint16_t str; uint8_t weight, face; uint16_t term_first; uint8_t nterm, flags; } GAlt;

static int rd_rule(const GHdr *h, int k, GRule *r)
{
    uint8_t b[8];
    if (k < 0 || k >= h->nrules || !talk_db_read(TB_GRAMMAR, h->rules + 8u * (uint32_t)k, b, 8)) return 0;
    r->alt_first = le16(b); r->nalt = b[2]; r->act = b[3]; r->term_first = le16(b + 4); r->nterm = b[6];
    r->flags = b[7];
    return 1;
}

static int rd_alt(const GHdr *h, int k, GAlt *a)
{
    uint8_t b[8];
    if (k < 0 || k >= h->nalts || !talk_db_read(TB_GRAMMAR, h->alts + 8u * (uint32_t)k, b, 8)) return 0;
    a->str = le16(b); a->weight = b[2]; a->face = b[3]; a->term_first = le16(b + 4); a->nterm = b[6];
    a->flags = b[7];
    return 1;
}

/* how recently an alternative was said: 0 not lately, 1 by someone in the
 * room, 2 by this speaker in their last TALK_NOVELTY lines (Q5) */
static int recent(const Gen *g, int alt)
{
    int i;
    for (i = 0; i < TALK_NOVELTY; i++) if (g->c->recent[g->who][i] == alt + 1) return 2;
    for (i = 0; i < TALK_ROOM_NOVELTY; i++) if (g->c->room_recent[i] == alt + 1) return 1;
    return 0;
}

/* pick an alternative of symbol sym (weighted, one pass); novelty: skip the
 * speaker's recently used ones while any other is possible. -1 none. */
#define AVOID_MAX 6
static int avoided(const int *avoid, int navoid, int a)
{
    int i;
    for (i = 0; i < navoid; i++) if (avoid[i] == a) return 1;
    return 0;
}

static int pick(Gen *g, int sym, int novelty, const int *avoid, int navoid)
{
    uint8_t s[4];
    int i, best[3] = { -1, -1, -1 }, lv;
    uint32_t tot[3] = { 0, 0, 0 };
    if (sym < 0 || sym >= g->h.nsyms || !talk_db_read(TB_GRAMMAR, g->h.syms + 4u * (uint32_t)sym, s, 4)) return -1;
    for (i = 0; i < le16(s + 2); i++) {
        uint16_t rk;
        GRule r;
        int a;
        if (!g_u16(g->h.refs + 2u * (uint32_t)(le16(s) + i), &rk) || !rd_rule(&g->h, rk, &r)) return -1;
        if (!terms_ok(g, r.term_first, r.nterm)) continue;
        for (a = r.alt_first; a < r.alt_first + r.nalt; a++) {
            GAlt al;
            uint32_t w;
            if (avoided(avoid, navoid, a) || !rd_alt(&g->h, a, &al)) continue;
            if ((al.flags & AF_SWEAR) && g->c->swears) continue;                    /* K-9 */
            if ((al.flags & AF_JMAT) && !pred(g, TGP_JMAT, 0)) continue;           /* K-6 */
            if (!terms_ok(g, al.term_first, al.nterm)) continue;
            w = al.weight * ((al.flags & AF_CAST) ? 8u : 1u);                      /* the cast's own lines win */
            lv = novelty ? recent(g, a) : 0;
            tot[lv] += w;
            if ((uint32_t)rng_range(&g->rng, (int)(tot[lv] > 65536u ? 65536u : tot[lv])) < w) best[lv] = a;
        }
    }
    /* the freshest that is possible: not said lately, else not by this speaker lately */
    return best[0] >= 0 ? best[0] : best[1] >= 0 ? best[1] : best[2];
}

static int entry_sym(const Gen *g, int table, int idx)
{
    uint16_t v;
    uint32_t at = g->h.entry + 2u * (uint32_t)(table * TA_COUNT + idx);
    if (!g_u16(at, &v) || v == NO_SYM) return -1;
    return v;
}
#define ENT_CORE  0
#define ENT_REACT 1
#define ENT_FIXED 2

/* ---- fillers -------------------------------------------------------------- */
static void apply_mods(char *s, const char *mods)
{
    int i;
    for (; *mods; mods++) {
        if (*mods == 'c') {
            for (i = 0; s[i]; i++) if (s[i] != ' ' && s[i] != '\'') break;
            if (s[i] >= 'a' && s[i] <= 'z') s[i] = (char)(s[i] - 'a' + 'A');
        } else if (*mods == 'u') {
            for (i = 0; s[i]; i++) if (s[i] >= 'a' && s[i] <= 'z') s[i] = (char)(s[i] - 'a' + 'A');
        } else if (*mods == 's') {
            int n = (int)strlen(s);
            if (n > 0 && n < 60) {
                if (s[n - 1] == 's') { s[n] = '\''; s[n + 1] = 0; }
                else { s[n] = '\''; s[n + 1] = 's'; s[n + 2] = 0; }
            }
        }
    }
}

static int has_mod(const char *mods, char m) { return strchr(mods, m) != NULL; }

/* one concept filler: cid, field (0 the form), mods; kind of record */
static void fill_concept(Gen *g, uint16_t cid, char field, const char *mods)
{
    char s[64];
    TalkConcept c;
    int which = TF_DEFAULT, start = g->len, rec = 1;
    uint16_t said = cid;
    if (!cid || !talk_db_concept(cid, &c) || !talk_gen_concept_ok(cid, &g->c->aud)) { g->fail = 1; return; }
    s[0] = 0;
    switch (field) {
    case 'a': case 'g': case 'b': {
        uint16_t a;
        TalkConcept ac;
        if (!attr_pick(g, cid, field == 'g' ? 1 : field == 'b' ? -1 : 0, &a) || !talk_db_concept(a, &ac)) {
            g->fail = 1;
            return;
        }
        if (has_mod(mods, 'n')) strcpy(s, (ac.flags & TCF_AN) ? "an " : "a ");
        talk_db_form(a, TF_LEMMA, s + strlen(s), (int)(sizeof s - strlen(s)));
        apply_mods(s, mods);
        if (talk_safe_lint_probe(s, &g->c->aud, 0) != TL_OK) { g->fail = 1; return; }
        emit(g, s, -1);
        push_fill(g, start, g->len - start, TFK_CONCEPT, ac.cat);
        return;
    }
    case 's': {
        uint16_t a;
        if (!assoc_pick(g, cid, &a) || !talk_db_concept(a, &c)) { g->fail = 1; return; }
        cid = a;
        said = a;
        break;
    }
    case 'i': case 'v': case 'h': case 'd': case 'w': {
        int pl = (c.flags & TCF_PLURAL) ||
                 ((c.flags & TCF_PROPER) && (talk_db_cat_root(c.cat) == TCR_PEOPLE || cat_is(c.cat, "lore.cast")));
        const char *w = field == 'i' ? (pl ? "they" : "it") : field == 'v' ? (pl ? "are" : "is") :
                        field == 'h' ? (pl ? "have" : "has") : field == 'd' ? (pl ? "do" : "does") :
                        (pl ? "were" : "was");
        strcpy(s, w);
        apply_mods(s, mods);
        emit(g, s, -1);
        return;
    }
    default:
        break;
    }
    if (has_mod(mods, 'p')) which = TF_PLURAL;
    else if (has_mod(mods, 'n')) which = TF_INDEF;
    else if (has_mod(mods, 't')) which = TF_DEF;
    else if (has_mod(mods, 'l')) which = TF_LEMMA;
    if (talk_db_form(cid, which, s, (int)sizeof s - 3) <= 0) { g->fail = 1; return; }
    apply_mods(s, mods);
    /* what the S3 lint would refuse is never filled in ("spice's" squeezes to a hate-list plural) */
    if (talk_safe_lint_probe(s, &g->c->aud, 0) != TL_OK) { g->fail = 1; return; }
    emit(g, s, -1);
    if (rec) push_fill(g, start, g->len - start, TFK_CONCEPT, c.cat);
    push_said(g, said);
}

static void fill_name(Gen *g, int id, const char *mods, const char *fallback)
{
    char s[24];
    DoomoResident buf;
    const char *n = id >= 0 ? name_of(id, &buf) : "";
    int start = g->len;
    if (!n || !n[0] || talk_safe_lint_probe(n, &g->c->aud, 1) != TL_OK) n = fallback;   /* a typed name that may not be said */
    if (!n) { g->fail = 1; return; }
    strncpy(s, n, sizeof s - 4);
    s[sizeof s - 4] = 0;
    apply_mods(s, mods);
    emit(g, s, -1);
    push_fill(g, start, g->len - start, TFK_NAME, 0);
}

static void expand_sym(Gen *g, int sym);

static void expand_str(Gen *g, int str)
{
    char t[128];
    int i, n;
    if (talk_db_string(TB_STRINGS, str, t, (int)sizeof t) < 0) { g->fail = 1; return; }
    for (i = 0; t[i] && !g->fail; ) {
        if (t[i] != '{') {
            int j = i;
            while (t[j] && t[j] != '{') j++;
            emit(g, t + i, j - i);
            i = j;
            continue;
        }
        n = 1;
        while (t[i + n] && t[i + n] != '}') n++;
        if (!t[i + n]) { g->fail = 1; return; }
        if (t[i + 1] == '#') {
            int sym = 0, k;
            for (k = i + 2; k < i + n; k++) sym = sym * 10 + (t[k] - '0');
            expand_sym(g, sym);
        } else if (t[i + 1] >= 'A' && t[i + 1] <= 'Z') {
            char slot = t[i + 1], field = 0, mods[8];
            int k = i + 2, m = 0;
            if (k < i + n && t[k] >= 'a' && t[k] <= 'z') field = t[k++];
            if (k < i + n && t[k] == ':') k++;
            while (k < i + n && m < 7) mods[m++] = t[k++];
            mods[m] = 0;
            switch (slot) {
            case 'X': fill_concept(g, g->topic, field, mods); break;
            case 'F': fill_concept(g, g->like[0], field, mods); break;
            case 'H': fill_concept(g, g->like[1], field, mods); break;
            case 'U': fill_concept(g, g->like[2], field, mods); break;
            case 'E': fill_concept(g, g->like[3], field, mods); break;
            case 'Z': fill_concept(g, g->like[4], field, mods); break;
            case 'L': fill_concept(g, g->like_pick, field, mods); break;
            case 'K': fill_concept(g, g->mem_ok ? g->c->mem_topic[g->who] : 0, field, mods); break;
            case 'J': fill_concept(g, g->job, field, mods); break;
            case 'Y': fill_name(g, g->c->avatar, mods, "pal"); break;
            case 'M': fill_name(g, g->sid, mods, "me"); break;
            case 'T': fill_name(g, g->to >= 0 && g->to < g->c->n ? g->c->id[g->to] : -1, mods, NULL); break;
            case 'O': fill_name(g, g->other >= 0 ? g->c->id[g->other] : -1, mods, NULL); break;
            case 'P': fill_name(g, g->partner, mods, NULL); break;
            case 'Q': fill_name(g, TALK_PET_ID, mods, "Boreagotchi"); break;
            case 'C': {
                char s[40];
                int start = g->len;
                if (!g->r || !pred(g, TGP_CATCH, 0)) { g->fail = 1; break; }
                strncpy(s, doomo_catchphrases[RES_CATCHPHRASE(g->r) % CATCHPHRASE_COUNT], sizeof s - 1);
                s[sizeof s - 1] = 0;
                apply_mods(s, mods);
                emit(g, s, -1);
                push_fill(g, start, g->len - start, TFK_TEXT, 0);
                break;
            }
            default: g->fail = 1; break;
            }
        } else {
            emit(g, t + i, n + 1);                      /* a story line code, kept */
        }
        i += n + 1;
    }
}

static void expand_alt(Gen *g, int alt)
{
    GAlt a;
    if (!rd_alt(&g->h, alt, &a)) { g->fail = 1; return; }
    push_alt(g, alt);
    expand_str(g, a.str);
}

/* a sub-rule: an alternative whose filling fails is set aside and another picked */
static void expand_sym(Gen *g, int sym)
{
    int avoid[AVOID_MAX], navoid = 0, len = g->len;
    uint8_t nalt = g->part_tr.nalt, nfill = g->part_tr.nfill, nsaid = g->part_tr.nsaid;
    if (++g->depth > MAX_DEPTH) { g->fail = 1; g->depth--; return; }
    for (;;) {
        int alt = pick(g, sym, 0, avoid, navoid);
        if (alt < 0) { g->fail = 1; break; }
        g->fail = 0;
        expand_alt(g, alt);
        if (!g->fail || navoid >= AVOID_MAX) break;
        avoid[navoid++] = alt;                               /* undo it and try another */
        g->len = len;
        g->buf[len] = 0;
        g->part_tr.nalt = nalt; g->part_tr.nfill = nfill; g->part_tr.nsaid = nsaid;
    }
    g->depth--;
}

/* ---- one part --------------------------------------------------------- */
typedef struct Part {
    char text[PART_MAX];
    int alt;                    /* its top alternative, -1 none */
    uint8_t face, flags;
    TalkTrace tr;
} Part;

/* one part from symbol sym: an alternative whose filling fails (a filler the
 * room may not hear, a name the lint refuses) is set aside and another picked */
static int make_part_avoid(Gen *g, int sym, int novelty, Part *p, const int *pre, int npre)
{
    GAlt a;
    int avoid[AVOID_MAX * 2], navoid = 0;
    while (navoid < npre && navoid < AVOID_MAX) { avoid[navoid] = pre[navoid]; navoid++; }
    p->text[0] = 0;
    p->alt = -1;
    p->face = 0;
    p->flags = 0;
    if (sym < 0) return 0;
    while (navoid < AVOID_MAX * 2) {
        p->alt = pick(g, sym, novelty, avoid, navoid);
        if (p->alt < 0 || !rd_alt(&g->h, p->alt, &a)) { p->alt = -1; return 0; }
        memset(&g->part_tr, 0, sizeof g->part_tr);
        g->buf = p->text;
        g->len = 0;
        g->cap = PART_MAX;
        g->fail = 0;
        g->depth = 0;
        p->text[0] = 0;
        expand_alt(g, p->alt);
        if (!g->fail && p->text[0]) {
            p->face = a.face;
            p->flags = a.flags;
            p->tr = g->part_tr;
            return 1;
        }
        avoid[navoid++] = p->alt;
    }
    p->alt = -1;
    p->text[0] = 0;
    return 0;
}

static int make_part(Gen *g, int sym, int novelty, Part *p) { return make_part_avoid(g, sym, novelty, p, NULL, 0); }

/* the visible text: line codes out */
static void visible(const char *s, char *v, int cap)
{
    int o = 0;
    for (; *s && o < cap - 1; s++) {
        if (*s == '{') { while (*s && *s != '}') s++; if (!*s) break; continue; }
        v[o++] = *s;
    }
    v[o] = 0;
}

/* "Dr." in "Dr. Bloom" ends no sentence (talkc_gen.py ABBREV is the same list) */
static int abbrev_at(const char *v, int i)
{
    static const char *const ab[] = { "Mr", "Mrs", "Ms", "Dr", "St", "Mme", "Jr", "Sr", "Mt", "Prof", "Rev" };
    int j = i, k;
    while (j > 0 && ((v[j - 1] >= 'a' && v[j - 1] <= 'z') || (v[j - 1] >= 'A' && v[j - 1] <= 'Z'))) j--;
    for (k = 0; k < (int)(sizeof ab / sizeof ab[0]); k++)
        if ((int)strlen(ab[k]) == i - j && strncmp(v + j, ab[k], (size_t)(i - j)) == 0) return 1;
    return 0;
}

/* sentences: an end mark followed by a space or the end ("...Right." is one) */
static int sentences(const char *v)
{
    int i, n = 0;
    for (i = 0; v[i]; i++)
        if ((v[i] == '.' || v[i] == '!' || v[i] == '?') && (v[i + 1] == 0 || v[i + 1] == ' ') &&
            !(v[i] == '.' && abbrev_at(v, i)))
            n++;
    return n;
}

/* the bubble (TALK_BUBBLE_LINES x TALK_BUBBLE_COLS) and 1-3 sentences (Q1) */
static int fits(const char *s)
{
    char v[TALK_LINE_MAX * 2];
    if ((int)strlen(s) >= TALK_LINE_MAX) return 0;
    visible(s, v, (int)sizeof v);
    return talk_safe_bubble_lines(v, TALK_BUBBLE_COLS) <= TALK_BUBBLE_LINES && sentences(v) <= TALK_SENTENCES_MAX;
}

static int ends_sentence(const char *s)
{
    int n = (int)strlen(s);
    while (n > 0) {
        if (s[n - 1] == '}') { while (n > 0 && s[n - 1] != '{') n--; n--; continue; }
        if (s[n - 1] == ' ' || s[n - 1] == '"' || s[n - 1] == ')') { n--; continue; }
        return s[n - 1] == '.' || s[n - 1] == '!' || s[n - 1] == '?';
    }
    return 1;
}

static void cap_first(char *s)
{
    int i;
    for (i = 0; s[i]; i++) {
        if (s[i] == '{') { while (s[i] && s[i] != '}') i++; if (!s[i]) return; continue; }
        if (s[i] >= 'a' && s[i] <= 'z') { s[i] = (char)(s[i] - 'a' + 'A'); return; }
        if ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9')) return;
    }
}

/* the exclamation budget of PERS_EXPRESSION (Q3) */
static void excl_budget(char *s, int expr)
{
    int budget = expr <= 4 ? 1 : expr <= 10 ? 2 : 3, n = 0, i, o = 0;
    for (i = 0; s[i]; i++) {
        if (s[i] == '{') { while (s[i] && s[i] != '}') s[o++] = s[i++]; if (!s[i]) break; s[o++] = s[i]; continue; }
        if (s[i] == '!' && ++n > budget) {
            if (o > 0 && (s[o - 1] == '!' || s[o - 1] == '?' || s[o - 1] == '.')) continue;
            s[o++] = '.';
            continue;
        }
        s[o++] = s[i];
    }
    s[o] = 0;
}

/* lower: a lower-case voice, no capital letters added */
static void join(char *out, int cap, const Part *parts, const int *use, int np, TalkTrace *tr, int lower)
{
    int k, len = 0;
    out[0] = 0;
    if (tr) memset(tr, 0, sizeof *tr);
    for (k = 0; k < np; k++) {
        int n, f, base;
        char tmp[PART_MAX];
        if (!use[k] || !parts[k].text[0]) continue;
        strcpy(tmp, parts[k].text);
        if (!lower && (len == 0 || ends_sentence(out))) cap_first(tmp);
        n = (int)strlen(tmp);
        if (len + (len ? 1 : 0) + n >= cap) continue;
        if (len) out[len++] = ' ';
        base = len;
        memcpy(out + len, tmp, (size_t)n + 1);
        len += n;
        if (!tr) continue;
        tr->parts |= (uint8_t)(1u << k);
        for (f = 0; f < parts[k].tr.nalt && tr->nalt < TALK_TRACE_ALTS; f++) tr->alt[tr->nalt++] = parts[k].tr.alt[f];
        for (f = 0; f < parts[k].tr.nfill && tr->nfill < TALK_TRACE_FILLS; f++) {
            tr->fill[tr->nfill] = parts[k].tr.fill[f];
            tr->fill[tr->nfill].off = (uint8_t)(tr->fill[tr->nfill].off + base);
            tr->nfill++;
        }
        for (f = 0; f < parts[k].tr.nsaid; f++) {
            int j, dup = 0;
            for (j = 0; j < tr->nsaid; j++) dup |= tr->said[j] == parts[k].tr.said[f];
            if (!dup && tr->nsaid < 4) tr->said[tr->nsaid++] = parts[k].tr.said[f];
        }
    }
}

static int is_fact_pred(int p)
{
    switch (p) {
    case TGP_FACT_JOB: return TCB_JOB;
    case TGP_FACT_PARTNER: return TCB_PARTNER;
    case TGP_FACT_PET: return TCB_PET;
    case TGP_FACT_NEWS: return TCB_NEWS;
    case TGP_FACT_PROBLEM: case TGP_PROB: return TCB_PROBLEM;
    case TGP_FACT_BDAY: return TCB_BDAY;
    case TGP_GHOST: return TCB_GHOST;
    case TGP_POSSESSED: return TCB_POSSESSED;
    case TGP_HELLBORN: return TCB_HELLBORN;
    case TGP_NIGHT: return TCB_NIGHT;
    case TGP_FACT_MEMORY: case TGP_FACT_COMPLIMENTED: case TGP_FACT_INSULTED: case TGP_FACT_PROMISED:
        return TCB_MEMORY;
    case TGP_LIKES_TOPIC: return TCB_LIKE;
    case TGP_FEARS_TOPIC: return TCB_FEAR;
    case TGP_CHAPTER: return TCB_CHAPTER;
    case TGP_FACT_FAMILY: return TCB_FAMILY;
    default: return TCB_NONE;
    }
}

/* the callback kind an alternative stands on (its first positive fact term) */
static int cb_kind(const GHdr *h, int alt)
{
    TalkAltInfo ai;
    int i;
    (void)h;
    if (!talk_gen_alt_info(alt, &ai)) return TCB_NONE;
    for (i = 0; i < ai.nterm; i++) {
        int k;
        if (ai.term[2 * i] & 0x80) continue;
        k = is_fact_pred(ai.term[2 * i] & 0x7F);
        if (k) return k;
    }
    return TCB_NONE;
}

int talk_gen_alt_info(int alt, TalkAltInfo *out)
{
    GHdr h;
    GRule r;
    GAlt a;
    int k, i, lo, hi;
    if (!out || !ghdr(&h) || !rd_alt(&h, alt, &a)) return 0;
    memset(out, 0, sizeof *out);
    /* the rule that owns it: rules are in alternative order */
    lo = 0;
    hi = h.nrules - 1;
    k = -1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (!rd_rule(&h, mid, &r)) return 0;
        if (alt < r.alt_first) hi = mid - 1;
        else if (alt >= r.alt_first + r.nalt) lo = mid + 1;
        else { k = mid; break; }
    }
    if (k < 0) return 0;
    out->rule = (uint16_t)k;
    out->act = r.act;
    out->rflags = r.flags;
    out->aflags = a.flags;
    out->weight = a.weight;
    for (i = 0; i < r.nterm + a.nterm && i < 16; i++) {
        uint32_t t = i < r.nterm ? (uint32_t)(r.term_first + i) : (uint32_t)(a.term_first + i - r.nterm);
        if (!talk_db_read(TB_GRAMMAR, h.terms + 2u * t, out->term + 2 * i, 2)) return 0;
    }
    out->nterm = (uint8_t)i;
    return 1;
}

/* ---- the line ---------------------------------------------------------- */
static int setup(Gen *g, TalkConv *c, int who, int to, int act, uint16_t topic)
{
    int k;
    uint16_t v;
    memset(g, 0, sizeof *g);
    if (!c || who < 0 || who >= c->n || !ghdr(&g->h)) return 0;
    g->c = c;
    g->who = who;
    g->to = to;
    g->act = act;
    g->sid = c->id[who];
    g->to_id = to >= 0 && to < c->n ? c->id[to] : to == -1 ? c->avatar : -1;
    g->r = res_of(g->sid, &g->rbuf);
    g->minor = (uint8_t)talk_safe_minor(g->sid);
    g->stage = (uint8_t)(g->sid <= DOOMO_PLAYER_ID ? sim_life_stage(g->sid) :
                         family_find(g->sid) ? family_find(g->sid)->stage : LIFE_ADULT);
    memset(g->pc, 0xFF, sizeof g->pc);
    g->cast = 0xFF;
    g->partner = -1;
    g->other = -1;
    rng_seed(&g->rng, c->seed ^ doomo_hash32(0x9E3779B9u * (uint32_t)(c->lines + 1u) + (uint32_t)who));
    c->lines++;
    if (g->r) {
        for (k = 0; k < PERS_COUNT; k++) g->pers[k] = RES_PERSONA(g->r, k);
        g->type = (uint8_t)persona_type(g->r);
        g->mood = (uint8_t)band_mood(g->r->mood);
        for (k = 0; k < 5; k++) g->like[k] = talk_gen_like(g->sid, k);
        if (g->sid < DOOMO_MAX_RESIDENTS && !(g->r->flags & RF_CUSTOM))
            for (k = 0; k < g->h.ncast; k++)
                if (g_u16(g->h.cast + 2u * (uint32_t)k, &v) && v == g->sid) g->cast = (uint8_t)k;
    } else {
        for (k = 0; k < PERS_COUNT; k++) g->pers[k] = 8;
    }
    g->rel = (uint8_t)band_rel(g->sid, g->to_id);
    if (g->rel == TRB_PARTNER && (g->minor || (g->to_id >= 0 && talk_safe_minor(g->to_id)))) g->rel = TRB_FRIEND;
    if (talk_gen_fact(g->sid, TCB_JOB, &v)) g->job = v;
    if (talk_gen_fact(g->sid, TCB_PARTNER, &v)) g->partner = v;
    if (talk_gen_fact(g->sid, TCB_PROBLEM, &v)) g->prob = (uint8_t)v;
    g->mem_ok = (uint8_t)(c->mem_topic[who] && talk_gen_concept_ok(c->mem_topic[who], &c->aud));
    /* the topic: only a concept that may be said here */
    if (topic && talk_gen_concept_ok(topic, &c->aud) && talk_db_concept(topic, &g->tc)) g->topic = topic;
    g->stance = (uint8_t)(g->topic ? talk_gen_stance(g->sid, g->topic) : TST_NEUTRAL);
    g->like_pick = g->like[rng_range(&g->rng, 3)];
    g->other = other_slot(g);
    return 1;
}

static void safe_line(Gen *g, TalkLine *out, TalkTrace *tr)
{
    Part p;
    int sym = entry_sym(g, ENT_FIXED, TGE_SAFE);
    out->text[0] = 0;
    if (make_part(g, sym, 0, &p)) {
        strncpy(out->text, p.text, TALK_LINE_MAX - 1);
        out->text[TALK_LINE_MAX - 1] = 0;
        if (tr) {
            memset(tr, 0, sizeof *tr);
            tr->alt[0] = (uint16_t)p.alt;
            tr->nalt = 1;
            tr->parts = TPART_CORE;
            tr->core_alt = 0xFFFF;
            tr->tic = 0xFF;
        }
    }
    if (!out->text[0]) strcpy(out->text, "...");
    out->face = 0;
    out->topic = TC_NONE;
}

/* write the line from entry symbol `core` (reply act fallback `act_out`) */
static int gen_line(Gen *g, int core, int optional, TalkLine *out, TalkTrace *tr)
{
    TalkConv *c = g->c;
    Part parts[5];               /* opener, core, callback, tag, hand-off */
    int use[5], try_, k, ok = 0, swears, lower = 0, too_long[AVOID_MAX], ntoo = 0;
    char line[TALK_LINE_MAX + PART_MAX];
    TalkAltInfo ai;
    memset(out, 0, sizeof *out);
    out->who = (int8_t)g->who;
    out->to = (int8_t)g->to;
    out->stance = g->stance;
    for (try_ = 0; try_ < TRIES && !ok; try_++) {
        int r = rng_range(&g->rng, 100), loud = g->pers[PERS_EXPRESSION];
        memset(use, 0, sizeof use);
        if (!make_part_avoid(g, core, 1, &parts[1], too_long, ntoo)) continue;
        use[1] = 1;
        lower = talk_gen_alt_info(parts[1].alt, &ai) && (ai.rflags & RF_LOWER);
        if (optional && !lower) {
            if (r < 30 + (loud >= 10 ? 15 : 0) - (loud <= 5 ? 15 : 0))
                use[0] = make_part(g, entry_sym(g, ENT_FIXED, TGE_OPEN), 0, &parts[0]);
            if (rng_range(&g->rng, 100) < 35) use[2] = make_part(g, entry_sym(g, ENT_FIXED, TGE_CB), 0, &parts[2]);
            if (rng_range(&g->rng, 100) < 40) use[3] = make_part(g, entry_sym(g, ENT_FIXED, TGE_TAG), 0, &parts[3]);
            if (g->other >= 0 && g->to == -1 && rng_range(&g->rng, 100) < 25)
                use[4] = make_part(g, entry_sym(g, ENT_FIXED, TGE_HANDOFF), 0, &parts[4]);
        }
        join(line, (int)sizeof line, parts, use, 5, NULL, lower);
        if (fits(line)) { ok = 1; break; }
        if (!fits(parts[1].text) && ntoo < AVOID_MAX) {     /* the core alone is too long: set it aside */
            too_long[ntoo++] = parts[1].alt;
            try_--;
            if (ntoo >= AVOID_MAX) try_ = TRIES - 2;
            continue;
        }
        if (try_ == TRIES - 1) {
            static const uint8_t drop[] = { 3, 2, 0, 4 };      /* tag, callback, opener, hand-off */
            for (k = 0; k < 4 && !ok; k++) {
                use[drop[k]] = 0;
                join(line, (int)sizeof line, parts, use, 5, NULL, lower);
                ok = fits(line);
            }
        }
    }
    if (!ok) {
        safe_line(g, out, tr);
        out->flags |= TLF_FALLBACK;
        return 0;
    }
    join(line, (int)sizeof line, parts, use, 5, tr, lower);
    excl_budget(line, g->pers[PERS_EXPRESSION]);
    if (!lower) cap_first(line);
    strncpy(out->text, line, TALK_LINE_MAX - 1);
    out->text[TALK_LINE_MAX - 1] = 0;
    /* what the line does and is about */
    if (talk_gen_alt_info(parts[1].alt, &ai)) {
        out->act = ai.act;
        if (ai.rflags & RF_SINCERE) out->flags |= TLF_SINCERE;
    }
    for (k = 0; k < 5; k++) if (use[k] && parts[k].face && !out->face) out->face = parts[k].face;
    if (parts[1].flags & AF_TOPIC) out->topic = g->topic;
    else if (parts[1].flags & AF_LIKE) out->topic = g->like_pick;
    if (use[4]) out->flags |= TLF_HANDOFF;
    if (tr) {
        tr->picks = (uint8_t)try_;
        tr->tic = 0xFF;
        tr->core_alt = (uint16_t)parts[1].alt;
        if (use[2]) {
            tr->cb = (uint8_t)cb_kind(&g->h, parts[2].alt);
            if (tr->cb == TCB_LIKE || tr->cb == TCB_FEAR) tr->cb_val = g->topic;
            else if (tr->cb == TCB_MEMORY) tr->cb_val = c->mem_topic[g->who];
            else if (tr->cb) talk_gen_fact(g->sid, tr->cb, &tr->cb_val);
        }
        if (use[3] && (parts[3].flags & AF_TIC)) tr->tic = g->type;
    }
    /* the conversation's memory of this line */
    for (k = TALK_NOVELTY - 1; k > 0; k--) c->recent[g->who][k] = c->recent[g->who][k - 1];
    c->recent[g->who][0] = (uint16_t)(parts[1].alt + 1);
    for (k = TALK_ROOM_NOVELTY - 1; k > 0; k--) c->room_recent[k] = c->room_recent[k - 1];
    c->room_recent[0] = (uint16_t)(parts[1].alt + 1);
    c->spoke |= (uint8_t)(1u << g->who);
    for (k = 0; k < 5; k++) if (use[k] && (parts[k].flags & AF_CATCH)) c->catch_used |= (uint8_t)(1u << g->who);
    /* S3: the output lint, every line */
    swears = c->swears;
    if ((k = talk_safe_lint(out->text, &c->aud, &swears)) != TL_OK) {
        char m[TALK_LINE_MAX + 24];
        doomo_snprintf(m, (int)sizeof m, "talk: lint %d: %s", k, out->text);   /* design 7.2: release builds log it */
        plat_log(m);
        safe_line(g, out, tr);
        out->flags |= TLF_SAFE;
        return 0;
    }
    c->swears = (uint8_t)swears;
    return 1;
}

int talk_gen_line(TalkConv *c, int who, int to, int act, uint16_t topic, TalkLine *out, TalkTrace *tr)
{
    Gen g;
    int sym, r;
    if (!out) return 0;
    memset(out, 0, sizeof *out);
    if (!setup(&g, c, who, to, act, topic)) { strcpy(out->text, "..."); return 0; }
    if (c->id[who] == TALK_PET_ID) {
        sym = entry_sym(&g, ENT_FIXED, TGE_PET);
        r = gen_line(&g, sym, 0, out, tr);
        out->act = TA_BACKCHANNEL;
        return r;
    }
    sym = act > 0 && act < TA_COUNT ? entry_sym(&g, ENT_CORE, act) : -1;
    if (sym < 0) sym = entry_sym(&g, ENT_CORE, TA_UNKNOWN);
    r = gen_line(&g, sym, !TA_IS_SAFETY(act) && !c->plain, out, tr);
    out->who = (int8_t)who;
    out->to = (int8_t)to;
    return r;
}

/* a reaction from `who` to the line `prev` (another resident's) */
static int react_line(TalkConv *c, int who, const TalkLine *prev, uint16_t topic, TalkLine *out, TalkTrace *tr)
{
    Gen g;
    static const uint8_t cand[] = { TA_AGREE, TA_DISAGREE, TA_LAUGH, TA_TEASE, TA_JOKE, TA_BACKCHANNEL };
    int k, act = 0, sym = -1, st, pst = prev->stance, w, best = -1;
    uint32_t tot = 0;
    if (!setup(&g, c, who, prev->who, prev->act, topic)) return 0;
    if (c->id[who] == TALK_PET_ID) {
        if (!talk_gen_adjacent(prev->act, TA_BACKCHANNEL)) return 0;
        k = gen_line(&g, entry_sym(&g, ENT_FIXED, TGE_PET), 0, out, tr);
        out->act = TA_BACKCHANNEL;
        out->flags |= TLF_REACTION;
        return k;
    }
    st = g.stance;
    for (k = 0; k < (int)sizeof cand; k++) {
        int a = cand[k];
        if (!talk_gen_adjacent(prev->act, a) || entry_sym(&g, ENT_REACT, a) < 0) continue;
        w = 2;
        if (a == TA_AGREE) w = (st == pst && st != TST_NEUTRAL) ? 8 : 1;
        if (a == TA_DISAGREE) w = (st != pst && st != TST_NEUTRAL && pst != TST_NEUTRAL) ? 8 : 1;
        if (a == TA_TEASE) w = (g.pers[PERS_SPEECH] >= 10 || g.rel == TRB_RIVAL) ? 5 : 1;
        if (a == TA_LAUGH) w = (prev->act == TA_JOKE || prev->act == TA_TEASE) ? 8 : 2;
        if (a == TA_JOKE) w = g.pers[PERS_QUIRK] >= 11 ? 4 : 2;
        tot += (uint32_t)w;
        if ((uint32_t)rng_range(&g.rng, (int)tot) < (uint32_t)w) best = a;
    }
    if (best < 0) return 0;
    act = best;
    sym = entry_sym(&g, ENT_REACT, act);
    g.act = prev->act;
    k = gen_line(&g, sym, 0, out, tr);
    out->flags |= TLF_REACTION;
    return k;
}

uint32_t talk_gen_seed(int avatar, const int *ids, int n)
{
    uint32_t h = doomo_hash32(doomo_town.seed ^ 0x54414C4Bu) ^ doomo_hash32((uint32_t)doomo_town.day * 2654435761u);
    int k;
    h ^= doomo_hash32((uint32_t)avatar + 0x1000u);
    for (k = 0; k < n; k++) h = doomo_hash32(h ^ (uint32_t)ids[k]);
    return h;
}

void talk_gen_begin(TalkConv *c, int avatar, const int *ids, int n, uint32_t seed)
{
    int k;
    memset(c, 0, sizeof *c);
    if (n > TALK_PARTS_MAX) n = TALK_PARTS_MAX;
    if (n < 0) n = 0;
    c->seed = seed;
    c->avatar = (int16_t)avatar;
    c->n = (uint8_t)n;
    for (k = 0; k < n; k++) c->id[k] = (int16_t)ids[k];
    c->beat = TBEAT_SMALLTALK;
    c->chapter = (uint8_t)story_chapter();
    talk_safe_ctx(avatar, ids, n, -1, &c->aud);
}

/* who answers when the player speaks to everyone: the most interested */
static int best_responder(TalkConv *c, uint16_t topic, DoomoRng *rng)
{
    int k, best = -1, bs = -100000;
    for (k = 0; k < c->n; k++) {
        int id = c->id[k], s, j;
        if (id == TALK_PET_ID) continue;
        s = rel_value(id, c->avatar) / 4 + rng_range(rng, 21) - 10;
        for (j = 0; j < 3; j++) if (topic && talk_gen_like(id, j) == topic) s += 60;
        if (topic && (talk_gen_like(id, 3) == topic || talk_gen_like(id, 4) == topic)) s -= 60;
        if (s > bs) { bs = s; best = k; }
    }
    return best;
}

static int exchange(TalkConv *c, TalkUtter *u, TalkExchange *x, TalkTrace *tr);

int talk_gen_exchange(TalkConv *c, TalkUtter *u, TalkExchange *x, TalkTrace *tr)
{
    int n = exchange(c, u, x, tr), k;
    for (k = 0; tr && k < n; k++) tr[k].gate = x->gate;
    return n;
}

static int exchange(TalkConv *c, TalkUtter *u, TalkExchange *x, TalkTrace *tr)
{
    TalkAudience aud;
    DoomoRng rng;
    int ids[TALK_PARTS_MAX], k, g, target, target_id, resp, nre = 0;
    if (!x) return 0;
    memset(x, 0, sizeof *x);
    x->first = -1;
    if (!c || !u || c->ended || c->n == 0) return 0;
    for (k = 0; k < c->n; k++) ids[k] = c->id[k];
    target = u->target >= 0 && u->target < c->n ? u->target : -1;
    target_id = target >= 0 ? c->id[target] : -1;
    talk_safe_ctx(c->avatar, ids, c->n, target, &aud);
    g = talk_safe_gate(u, &aud, c->avatar, target_id, target >= 0 && ((c->refused >> target) & 1));
    x->gate = (uint8_t)g;
    c->tone = u->tone;
    c->turn++;
    rng_seed(&rng, c->seed ^ doomo_hash32(0x7F4A7C15u + c->turn));
    if (g == TG_END_K7) {
        /* the adults step in, briefly and with no joke (a child alone says
         * a kid's line); then everyone leaves */
        int pass;
        for (pass = 0; pass < 2 && x->n == 0; pass++)
            for (k = 0; k < c->n && x->n < 2; k++) {
                Gen gg;
                if (c->id[k] == TALK_PET_ID || (pass == 0 && talk_safe_minor(c->id[k]))) continue;
                if (!setup(&gg, c, k, -1, TA_REFUSE_SEXUAL, TC_NONE)) continue;
                gen_line(&gg, entry_sym(&gg, ENT_FIXED, TGE_END_K7), 0, &x->line[x->n], tr ? &tr[x->n] : NULL);
                x->line[x->n].act = TA_REFUSE_SEXUAL;
                x->line[x->n].flags |= TLF_END | TLF_SINCERE;
                if (x->first < 0) x->first = (int8_t)k;
                x->n++;
                if (pass == 1) break;
            }
        c->ended = 1;
        return x->n;
    }
    /* who answers */
    resp = target >= 0 ? target : best_responder(c, u->topic[0], &rng);
    if (resp < 0) return 0;
    if (c->id[resp] == TALK_PET_ID && g != TG_PASS) {
        for (k = 0; k < c->n; k++) if (c->id[k] != TALK_PET_ID) { resp = k; break; }
        if (c->id[resp] == TALK_PET_ID) return 0;
    }
    x->first = (int8_t)resp;
    talk_gen_line(c, resp, -1, u->act[0], u->topic[0], &x->line[0], tr ? &tr[0] : NULL);
    x->n = 1;
    if (g == TG_DEFLECT && target >= 0) c->refused |= (uint8_t)(1u << target);      /* K-5: no more today */
    if (g == TG_PASS && TA_IS_ROMANCE(u->act[0]) && !((c->consent >> resp) & 1))
        c->refused |= (uint8_t)(1u << resp);                                         /* a no ends it (K-5) */
    if (g != TG_PASS || TA_IS_ROMANCE(u->act[0]) || c->id[resp] == TALK_PET_ID) return x->n;
    /* reactions between the residents: each other one may chime in, two at most */
    for (k = 0; k < c->n && x->n < TALK_EXCHANGE_MAX && nre < 2; k++) {
        const TalkLine *prev = &x->line[x->n - 1];
        DoomoResident buf;
        const DoomoResident *r;
        int p;
        if (k == prev->who || k == resp) continue;
        r = res_of(c->id[k], &buf);
        /* design 5.4: 0.25 + 0.04 EXPRESSION + 0.02 QUIRK (at most 75%) */
        p = c->id[k] == TALK_PET_ID ? 15 :
            r ? 25 + 4 * RES_PERSONA(r, PERS_EXPRESSION) + 2 * RES_PERSONA(r, PERS_QUIRK) : 25;
        if (p > 75) p = 75;
        if (rng_range(&rng, 100) >= p) continue;
        if (react_line(c, k, prev, u->topic[0], &x->line[x->n], tr ? &tr[x->n] : NULL) || x->line[x->n].text[0]) {
            x->n++;
            nre++;
        }
    }
    return x->n;
}
