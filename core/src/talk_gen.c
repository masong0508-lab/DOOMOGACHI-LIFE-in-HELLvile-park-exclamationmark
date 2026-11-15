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
 *     needs a blunt adult and is said once a conversation at most (K-9);
 *   - K-10, grief is not a punchline (SCRIPT_STYLE R6): a grief or loss
 *     topic (TCF_GRIEF) takes the sincere `grief` bank whatever the act, the
 *     core part alone; a reaction to it is a sincere `comfort` line or none;
 *     a TCF_GRIEF concept is never filled into any other line.
 *
 * Deterministic: a local DoomoRng per line, seeded from the conversation
 * seed and the line count, so the text is the same on every platform.
 * No stdio, no malloc, no floats. RAM: nothing static; about 1.5 KB of
 * stack per line (the part buffers and one decoded template per nesting
 * level, at most 4).
 */
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/talk_traits.h"
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
/* copies at most cap - 1 bytes and always ends the string (no strncpy: the
   sanitizer build's -Wstringop-truncation rejects the bounded copies) */
static void copy_cap(char *d, const char *s, int cap)
{
    int i = 0;
    if (cap <= 0) return;
    while (i < cap - 1 && s[i]) { d[i] = s[i]; i++; }
    d[i] = 0;
}
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

/* TM2c: byte 14 of an act's row: the kinds it serves as a reply; byte 15: the kinds allowed after it */
int talk_gen_kinds_of(int reply_act)
{
    uint8_t row[16];
    return act_row(reply_act, row) ? row[14] : 0;
}

int talk_gen_kinds_after(int act)
{
    uint8_t row[16];
    return act_row(act, row) ? row[15] : 0;
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
    if (id == TALK_PREVIEW_ID) return talk_preview_resident();      /* the creator's live preview (TM1) */
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
    if (id == TALK_PREVIEW_ID) return talk_preview_resident() ? talk_preview_resident()->name : "";
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

/* the stance score of a speaker on a concept (TM2c: the plan reads the number, the bands read the same one):
 * the concept's own sentiment, their likes +3, their fear and hated food -3, Hellvile-born flips holy things,
 * what their traits and liked topics lean to, and a fixed personal lean. ts NULL: no trait term. */
static int stance_score(int id, const DoomoResident *r, const TalkConcept *c, uint16_t topic, const uint16_t like[5],
                        const TalkTraitSet *ts, int stage)
{
    int s = c->sent, k;
    for (k = 0; k < 3; k++) if (like[k] == topic) s = 3;
    if (like[3] == topic || like[4] == topic) s = -3;
    if (r && ((r->flags & RF_HELLBORN) || (id < DOOMO_MAX_RESIDENTS && RES_DISTRICT(id) == DISTRICT_HELLVILE))) {
        if (cat_is(c->cat, "lore.church")) s = -s;            /* holy things */
        else if (talk_db_cat_root(c->cat) == TCR_HELLVILE) s += 1;
    }
    if (ts) s += talk_trait_topic_lean(ts, stage, talk_db_cat_root(c->cat));     /* TM1: traits and liked topics */
    s += (int)(doomo_hash32((uint32_t)id * 7919u + topic * 104729u) % 7u) - 3;   /* a fixed personal lean */
    return s;
}

static int stance_band(int s) { return s >= 2 ? TST_PRO : s <= -2 ? TST_CON : TST_NEUTRAL; }

int talk_gen_stance(int id, uint16_t topic)
{
    TalkConcept c;
    DoomoResident buf;
    const DoomoResident *r = res_of(id, &buf);
    uint16_t like[5];
    TalkTraitSet ts;
    int k, has = 0;
    if (!topic || !talk_db_concept(topic, &c)) return TST_NEUTRAL;
    for (k = 0; k < 5; k++) like[k] = talk_gen_like(id, k);
    if (id != TALK_PET_ID && r && talk_trait_get(id, &ts)) has = 1;
    return stance_band(stance_score(id, r, &c, topic, like, has ? &ts : NULL,
                                    !has ? 0 : id == TALK_PREVIEW_ID ? talk_preview_stage() : RES_STAGE(r)));
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

/* K-10: the memorial (index, newest first) of the household member the
 * speaker lost in the last TALK_GRIEF_DAYS town days; -1 none. Only a member
 * of the household grieves here (the memorials are the household's), and only
 * a death: one who moved away (FAMLOSS_GONE) is not mourned as one. */
static int loss_of(int id)
{
    int n, cnt;
    uint32_t today = doomo_town.day > 0xFFFFu ? 0xFFFFu : doomo_town.day;   /* the memorial's day saturates */
    if (id < 0 || id == TALK_PET_ID || family_role(id) == FAMROLE_NONE) return -1;
    cnt = family_memorial_count();
    for (n = 0; n < cnt; n++) {
        const FamilyMemorial *m = family_memorial(n);
        if (!m || (uint32_t)m->day + TALK_GRIEF_DAYS < today) return -1;     /* newest first: the rest are older */
        if ((int)m->id != id && m->cause != FAMLOSS_GONE) return n;
    }
    return -1;
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
    case TCB_LOSS:
        if ((i = loss_of(id)) < 0) return 0;
        v = family_memorial(i)->id;
        break;
    default:
        return 0;
    }
    if (val) *val = v;
    return 1;
}

/* ---- the per-line context --------------------------------------------- */
/* TM2c: a kind binds (filters the core) when it wins by this many points. Questions and requests (the ASK acts: an
 * opinion, a favour, a fact) bind from 2: it costs nothing measurable; every other act binds from 8, so only a
 * stance, a relationship or a trait that really decides binds. The request acts (invite, favour, ask out, give) bind
 * whenever the plan overrides the natural reply or something pushes it (talk_plan.c). A filter on every line costs
 * 3 to 4 points of Surface uniqueness in the smoke census (docs/TALK.md 6m has the table). */
#define PLAN_MARGIN_ASK 2
#define PLAN_MARGIN 8
#define KF_KIND 1
#define KF_COOL 2
typedef struct Gen {
    GHdr h;
    TalkConv *c;
    DoomoRng rng;
    int who, to, act, sid, to_id, other;
    const DoomoResident *r;
    DoomoResident rbuf;
    uint8_t minor, stage, type, mood, rel, stance, cast, prob, mem_ok;
    uint8_t grief;                /* K-10: a sincere grief or comfort line: a TCF_GRIEF concept may be said */
    uint8_t adj;                  /* the core's rules must answer g->act (acts.tsv): the grief and comfort banks */
    uint8_t pers[PERS_COUNT];     /* the EFFECTIVE sliders (TM2c: stored + trait deltas); the voice guards read these */
    uint8_t kf;                   /* TM2c: while the core is picked, KF_KIND filters by the plan's kind, KF_COOL drops the cool acts */
    uint8_t has_ts, kid_room;
    uint8_t dlvl;                 /* TM2d: the depth guard (depth=): the level of the line being made */
    uint8_t hook_now;             /* TM2d: the hook guard (hook=), TH_* */
    uint8_t secret;               /* TM2d: the secret guard (secret=), TSEC_* + 1, 0 none */
    uint8_t unk;                  /* TM2d: the unknown-line guard (unk=), TU_* */
    uint8_t unk_fail;             /* TM2d: the named line failed the S3 probe: ask without the echo */
    int8_t  lean;                 /* the stance as the plan sees it, -2..2 */
    TalkPlan plan;
    TalkTraitSet ts;              /* the speaker's traits as they count in this room (edgy ones dropped for a minor / in a kid room) */
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
            !(fl & TCAT_TALKABLE) || !talk_gen_concept_ok(ids[i], &g->c->aud) || ((a.flags & TCF_GRIEF) && !g->grief))
            continue;                                         /* K-10: no grief word in a joke */
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
    case TGP_FACT_LOSS: return loss_of(g->sid) >= 0;
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
    case TGP_X_GRIEF: return g->topic && (g->tc.flags & TCF_GRIEF);
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
    case TGP_KIND: return g->plan.kind == v;
    case TGP_TRAIT: return g->has_ts && talk_traitset_has(&g->ts, v);
    case TGP_DEPTH: return g->dlvl == v;
    case TGP_HOOK: return g->hook_now == v;
    case TGP_SECRET: return g->secret == v + 1;
    case TGP_UNK: return g->unk == v;
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
#define AVOID_MAX 10
static int avoided(const int *avoid, int navoid, int a)
{
    int i;
    for (i = 0; i < navoid; i++) if (avoid[i] == a) return 1;
    return 0;
}

/* TM2c: may a core rule with reply act `act` be picked: a kid room never gets the cool acts from the plan,
 * and the rule's reply act must serve the plan's kind (acts.tsv KINDS: byte 14 of the act's row) */
static int kind_serves(const Gen *g, int act)
{
    uint8_t b = 0;
    if (act <= 0 || act >= TA_COUNT || !talk_db_read(TB_GRAMMAR, g->h.acts + 16u * (uint32_t)act + 14u, &b, 1))
        return 0;
    return (b & TKM(g->plan.kind)) != 0;
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
        if (g->adj && g->depth == 0 && !talk_gen_adjacent(g->act, r.act)) continue;      /* K-10 banks */
        if (!terms_ok(g, r.term_first, r.nterm)) continue;
        if (g->kf && g->depth == 0) {                                                     /* TM2c: the plan's kind */
            if ((g->kf & KF_COOL) && TALK_COOL_REPLY(r.act)) continue;                    /* a kid room: never */
            if ((g->kf & KF_KIND) && !kind_serves(g, r.act)) continue;
        }
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
    if ((c.flags & TCF_GRIEF) && !g->grief) { g->fail = 1; return; }              /* K-10 */
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

static void fill_text_name(Gen *g, const char *n, const char *mods, const char *fallback)
{
    char s[24];
    int start = g->len;
    if (!n || !n[0] || talk_safe_lint_probe(n, &g->c->aud, 1) != TL_OK) n = fallback;   /* a typed name that may not be said */
    if (!n) { g->fail = 1; return; }
    copy_cap(s, n, (int)sizeof s - 3);
    apply_mods(s, mods);
    emit(g, s, -1);
    push_fill(g, start, g->len - start, TFK_NAME, 0);
}

static void fill_name(Gen *g, int id, const char *mods, const char *fallback)
{
    DoomoResident buf;
    fill_text_name(g, id >= 0 ? name_of(id, &buf) : "", mods, fallback);
}

/* K-10: {lost}, the one the speaker lost lately (a name the town or the pool
 * still knows; none: the line is set aside) */
static void fill_lost(Gen *g, const char *mods)
{
    char n[DOOMO_NAME_LEN + 1];
    int k = loss_of(g->sid);
    n[0] = 0;
    if (k < 0 || !family_memorial_name(k, n)) { g->fail = 1; return; }
    n[DOOMO_NAME_LEN] = 0;
    fill_text_name(g, n, mods, NULL);
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
            case 'G': fill_lost(g, mods); break;
#ifndef TALK_NO_UNK
            case 'W': {                                        /* TM2d: {u}, the unknown word, quoted (E7: unk.name rules only) */
                int start = g->len, n = (int)strlen(g->c->unk);
                if (g->unk != TU_NAME || n < 3 || n > 16 || !g->c->unk[0]) { g->fail = 1; break; }
                emit(g, "\"", 1);
                emit(g, g->c->unk, n);
                emit(g, "\"", 1);
                push_fill(g, start, g->len - start, TFK_TEXT, 0);
                break;
            }
#endif
            case 'C': {
                char s[40];
                int start = g->len;
                if (!g->r || !pred(g, TGP_CATCH, 0)) { g->fail = 1; break; }
                copy_cap(s, doomo_catchphrases[RES_CATCHPHRASE(g->r) % CATCHPHRASE_COUNT], (int)sizeof s);
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
        if ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '"') return;   /* a quoted word stays as typed: lower case (design 4, E7) */
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

/* the order the parts are said in: opener, core, reason, callback, tag, hook, hand-off */
static const uint8_t PART_ORDER[7] = { 0, 1, 5, 2, 3, 6, 4 };

/* lower: a lower-case voice, no capital letters added */
static void join(char *out, int cap, const Part *parts, const int *use, int np, TalkTrace *tr, int lower)
{
    int k, len = 0;
    out[0] = 0;
    if (tr) memset(tr, 0, sizeof *tr);
    for (k = 0; k < np; k++) {
        int n, f, base, pi = np == 7 ? PART_ORDER[k] : k;
        char tmp[PART_MAX];
        if (!use[pi] || !parts[pi].text[0]) continue;
        strcpy(tmp, parts[pi].text);
        if (!lower && (len == 0 || ends_sentence(out))) cap_first(tmp);
        n = (int)strlen(tmp);
        if (len + (len ? 1 : 0) + n >= cap) continue;
        if (len) out[len++] = ' ';
        base = len;
        memcpy(out + len, tmp, (size_t)n + 1);
        len += n;
        if (!tr) continue;
        tr->parts |= (uint8_t)(1u << pi);
        for (f = 0; f < parts[pi].tr.nalt && tr->nalt < TALK_TRACE_ALTS; f++) tr->alt[tr->nalt++] = parts[pi].tr.alt[f];
        for (f = 0; f < parts[pi].tr.nfill && tr->nfill < TALK_TRACE_FILLS; f++) {
            tr->fill[tr->nfill] = parts[pi].tr.fill[f];
            tr->fill[tr->nfill].off = (uint8_t)(tr->fill[tr->nfill].off + base);
            tr->nfill++;
        }
        for (f = 0; f < parts[pi].tr.nsaid; f++) {
            int j, dup = 0;
            for (j = 0; j < tr->nsaid; j++) dup |= tr->said[j] == parts[pi].tr.said[f];
            if (!dup && tr->nsaid < 4) tr->said[tr->nsaid++] = parts[pi].tr.said[f];
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
    case TGP_FACT_LOSS: return TCB_LOSS;
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
static int stage_for(int id)
{
    return id == TALK_PREVIEW_ID ? talk_preview_stage() :
           id <= DOOMO_PLAYER_ID ? sim_life_stage(id) :
           family_find(id) ? family_find(id)->stage : LIFE_ADULT;
}

int talk_gen_eff_persona(int id, int minor_room, uint8_t out[5])
{
    DoomoResident buf;
    const DoomoResident *r = res_of(id, &buf);
    TalkTraitSet ts;
    int k;
    if (!r) return 0;
    for (k = 0; k < PERS_COUNT; k++) out[k] = (uint8_t)RES_PERSONA(r, k);
    if (id != TALK_PET_ID && talk_trait_get(id, &ts)) {
        int8_t d[PERS_COUNT];
        talk_trait_room(&ts, stage_for(id), minor_room || talk_safe_minor(id));
        talk_trait_slider_delta(&ts, d);
        for (k = 0; k < PERS_COUNT; k++) {
            int v = out[k] + d[k];
            out[k] = (uint8_t)(v < 0 ? 0 : v > 15 ? 15 : v);
        }
    }
    return 1;
}

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
    g->stage = (uint8_t)stage_for(g->sid);
    g->kid_room = (uint8_t)(c->aud.any_minor || c->aud.avatar_minor || c->aud.target_minor || g->minor);
    memset(g->pc, 0xFF, sizeof g->pc);
    g->cast = 0xFF;
    g->partner = -1;
    g->other = -1;
    rng_seed(&g->rng, c->seed ^ doomo_hash32(0x9E3779B9u * (uint32_t)(c->lines + 1u) + (uint32_t)who));
    c->lines++;
    if (g->r) {
        for (k = 0; k < PERS_COUNT; k++) g->pers[k] = RES_PERSONA(g->r, k);
        g->type = (uint8_t)persona_type(g->r);                /* the stored sliders: the creator's title does not shift */
        if (g->sid != TALK_PET_ID && talk_trait_get(g->sid, &g->ts)) {      /* TM2c: the effective voice */
            int8_t d[PERS_COUNT];
            g->has_ts = 1;
            talk_trait_room(&g->ts, g->stage, g->kid_room);
            talk_trait_slider_delta(&g->ts, d);
            for (k = 0; k < PERS_COUNT; k++) {
                int v = g->pers[k] + d[k];
                g->pers[k] = (uint8_t)(v < 0 ? 0 : v > 15 ? 15 : v);
            }
        }
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
    if (g->mem_ok) {                                         /* K-10: a remembered grief topic is no callback */
        TalkConcept mc;
        if (!talk_db_concept(c->mem_topic[who], &mc) || (mc.flags & TCF_GRIEF)) g->mem_ok = 0;
    }
    /* the topic: only a concept that may be said here */
    if (topic && talk_gen_concept_ok(topic, &c->aud) && talk_db_concept(topic, &g->tc)) g->topic = topic;
    if (g->topic) {
        int sc = stance_score(g->sid, g->r, &g->tc, g->topic, g->like, g->has_ts ? &g->ts : NULL, g->stage);
        g->stance = (uint8_t)stance_band(sc);
        g->lean = (int8_t)(sc >= 3 ? 2 : sc >= 2 ? 1 : sc <= -3 ? -2 : sc <= -2 ? -1 : 0);
    }
    memcpy(g->plan.eff, g->pers, sizeof g->plan.eff);
    g->like_pick = g->like[rng_range(&g->rng, 3)];
    g->other = other_slot(g);
    return 1;
}

static void safe_line(Gen *g, TalkLine *out, TalkTrace *tr)
{
    Part p;
    int sym = entry_sym(g, ENT_FIXED, TGE_SAFE), ok;
    uint8_t adj = g->adj;
    out->text[0] = 0;
    g->adj = 0;                                  /* the safe bank answers anything */
    ok = make_part(g, sym, 0, &p);
    g->adj = adj;
    if (ok) {
        copy_cap(out->text, p.text, TALK_LINE_MAX);
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
static void trace_plan(const Gen *g, TalkTrace *tr)
{
    if (tr) tr->plan = g->plan;
}

/* TM2d: the visible length of a line (codes and *stage directions* out) */
static int vis_len(const char *s)
{
    char v[TALK_LINE_MAX * 2];
    int n = 0, i;
    visible(s, v, (int)sizeof v);
    for (i = 0; v[i]; i++) {
        if (v[i] == '*') { i++; while (v[i] && v[i] != '*') i++; if (!v[i]) break; continue; }
        n++;
    }
    return n;
}

/* TM2d: a core reply act that gives a stance (a reason can follow it) */
static int stance_act(int a)
{
    return a == TA_AGREE || a == TA_DISAGREE || a == TA_OPINE_POS || a == TA_OPINE_NEG || a == TA_YES || a == TA_NO ||
           a == TA_ANSWER;
}

/* a lower-case voice (Wisp, Vapula) says every part in lower case; the line codes stay */
static void lower_codes(char *s)
{
    int i;
    for (i = 0; s[i]; i++) {
        if (s[i] == '{') { while (s[i] && s[i] != '}') i++; if (!s[i]) return; continue; }
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
    }
}

static int same_thread(uint16_t a, uint16_t b);

/* TM2d: the player's act is a topic act: a question or a statement about the topic (the census' Q8b set) */
static int topic_act(int a)
{
    switch (a) {
    case TA_ASK_ABOUT: case TA_ASK_OPINION: case TA_TELL: case TA_OPINE_POS: case TA_OPINE_NEG: case TA_CHANGE_TOPIC:
    case TA_CONTINUE: case TA_AGREE: case TA_DISAGREE: case TA_ANSWER: case TA_GOSSIP:
        return 1;
    }
    return 0;
}

/* the core alternative stands on the topic: a guard that needs it (a pronoun line: "it", "that") */
static int alt_topicy(const TalkAltInfo *ai)
{
    int j;
    for (j = 0; j < ai->nterm; j++) {
        int p = ai->term[2 * j], v = ai->term[2 * j + 1];
        if (p & 0x80) continue;
        if (p == TGP_TOPIC || p == TGP_LIKES_TOPIC || p == TGP_FEARS_TOPIC || p == TGP_HATES_TOPIC ||
            (p >= TGP_X_COUNTABLE && p <= TGP_X_GRIEF) || p == TGP_ROOT || (p == TGP_STANCE && v != TST_NEUTRAL))
            return 1;
    }
    return 0;
}

static int gen_line(Gen *g, int core, int optional, TalkLine *out, TalkTrace *tr)
{
    TalkConv *c = g->c;
    Part parts[7];               /* opener, core, callback, tag, hand-off, reason, hook */
    int use[7], try_, k, ok = 0, swears, lower = 0, too_long[AVOID_MAX], ntoo = 0;
    char line[TALK_LINE_MAX + PART_MAX];
    TalkAltInfo ai;
    int relaxed = 0;             /* every core was set aside for a hook or a bridge: they are no longer required */
    memset(out, 0, sizeof *out);
    out->who = (int8_t)g->who;
    out->to = (int8_t)g->to;
    out->stance = g->stance;
    for (try_ = 0; try_ < TRIES && !ok; try_++) {
        int r = rng_range(&g->rng, 100), loud = g->pers[PERS_EXPRESSION], got, hook_req = 0, bridge_req = 0;
        memset(use, 0, sizeof use);
        g->kf = g->plan.kind ? (uint8_t)((g->plan.flags & (TPL_MISS | TPL_OPEN) ? 0 : KF_KIND) | (g->kid_room ? KF_COOL : 0)) : 0;
        got = make_part_avoid(g, core, 1, &parts[1], too_long, ntoo);
        while (!got && (g->kf & KF_KIND)) {                 /* TM2c: the soft filter: no rule of the kind passes the guards */
            if (!talk_plan_next(&g->plan)) {                /* the next best kind, else drop the filter and count the miss */
                g->plan.flags |= TPL_MISS;
                g->kf &= (uint8_t)~KF_KIND;
            }
            got = make_part_avoid(g, core, 1, &parts[1], too_long, ntoo);
        }
        g->kf = 0;                                        /* the filter is for the core part only */
        if (!got && ntoo && !relaxed) { ntoo = 0; relaxed = 1; try_--; continue; }    /* the shorter cores ran out: any core, nothing required */
        if (!got) continue;
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
        /* TM2d: a topic act answered by a line that names nothing: the line names the topic (Q8b; reason.name) */
        if (optional && !lower && g->topic && !g->grief && topic_act(g->act) && !(ai.rflags & (RF_SINCERE | RF_ROMANCE | RF_REFUSAL))) {
            int named = (parts[1].flags & AF_TOPIC) != 0 || alt_topicy(&ai), q;
            for (q = 0; q < parts[1].tr.nsaid && !named; q++) named = same_thread(g->topic, parts[1].tr.said[q]);
            if (!named && !(g->plan.kind && g->plan.depth >= 1 && stance_act(ai.act) && !strchr(parts[1].text, '?'))) {
                uint8_t d = g->dlvl;
                g->dlvl = 0;
                use[5] = make_part(g, entry_sym(g, ENT_FIXED, TGE_REASON), 0, &parts[5]);
                g->dlvl = d;
                bridge_req = use[5];
            }
        }
        /* TM2d: layer b depth 1 (the reason) and layer d (the hook) */
        if (optional && !lower && g->plan.kind && !(ai.rflags & (RF_SINCERE | RF_ROMANCE | RF_REFUSAL))) {
            int hk = talk_hook_wanted(g->act, g->plan.kind, TA_IS_ROMANCE(g->act), g->kid_room, 0);
            if (!hk && (g->act == TA_INVITE || g->act == TA_ASK_FAVOR) && (ai.act == TA_NO || ai.act == TA_DISMISS || ai.act == TA_CHANGE_TOPIC))
                hk = TH_OFFER;                                /* whatever the plan said, a line that turns a request down leaves something to do */
            if (hk == TH_ASK && g->other >= 0 && g->to == -1 && rng_range(&g->rng, 100) < 30) hk = TH_CALL;
            if (g->plan.depth >= 1 && g->topic && stance_act(ai.act) && !strchr(parts[1].text, '?')) {
                uint8_t d = g->dlvl;
                g->dlvl = 1;
                use[5] = make_part(g, entry_sym(g, ENT_FIXED, TGE_REASON), 0, &parts[5]);
                g->dlvl = d;
            }
            if (hk && !strchr(parts[1].text, '?')) {
                g->hook_now = (uint8_t)hk;
                use[6] = make_part(g, entry_sym(g, ENT_FIXED, TGE_HOOK), 0, &parts[6]);
                if (!use[6] && hk == TH_OFFER) {              /* no offer fits (no topic, no other): a question will do */
                    g->hook_now = TH_ASK;
                    use[6] = make_part(g, entry_sym(g, ENT_FIXED, TGE_HOOK), 0, &parts[6]);
                }
                g->hook_now = 0;
                hook_req = use[6];
                if (use[6]) g->plan.hook = (uint8_t)hk;
            }
        }
        join(line, (int)sizeof line, parts, use, 7, NULL, lower);
        if (fits(line)) { ok = 1; break; }
        if ((use[5] || use[6]) && fits(parts[1].text)) {        /* the reason, the bridge or the hook does not fit with the rest */
            static const uint8_t shed[] = { 3, 4, 0, 2 };         /* tag, hand-off, opener, callback: the optional parts go first */
            for (k = 0; k < 4 && !ok; k++) {
                if (!use[shed[k]]) continue;
                use[shed[k]] = 0;
                join(line, (int)sizeof line, parts, use, 7, NULL, lower);
                ok = fits(line);
            }
            if (!ok && use[6] && hook_req) {                     /* a shorter hook of the same kind before another core */
                int av[AVOID_MAX], nav = 0, i;
                for (i = 0; i < AVOID_MAX && !ok; i++) {
                    av[nav++] = parts[6].alt;
                    g->hook_now = g->plan.hook;
                    use[6] = make_part_avoid(g, entry_sym(g, ENT_FIXED, TGE_HOOK), 0, &parts[6], av, nav);
                    g->hook_now = 0;
                    if (!use[6]) break;
                    join(line, (int)sizeof line, parts, use, 7, NULL, lower);
                    ok = fits(line);
                }
                if (!ok && !use[6]) {                            /* no hook of that kind is short enough: the line without it */
                    join(line, (int)sizeof line, parts, use, 7, NULL, lower);
                    ok = fits(line);
                    if (ok) g->plan.hook = 0;
                }
                if (ok) break;
            }
            if (!ok && ((use[6] && hook_req) || (use[5] && bridge_req)) && ntoo < 6 && !relaxed) {
                too_long[ntoo++] = parts[1].alt;                 /* a required hook or bridge does not fit this core: try a shorter one */
                try_--;
                continue;
            }
            if (!ok && use[5]) {                                 /* the reason, then the hook, last */
                use[5] = 0;
                join(line, (int)sizeof line, parts, use, 7, NULL, lower);
                ok = fits(line);
            }
            if (!ok && use[6]) {
                use[6] = 0;
                join(line, (int)sizeof line, parts, use, 7, NULL, lower);
                ok = fits(line);
                if (ok && hook_req) g->plan.hook = 0;
            }
            if (ok) break;
        }
        if (!fits(parts[1].text) && ntoo < AVOID_MAX) {     /* the core alone is too long: set it aside */
            too_long[ntoo++] = parts[1].alt;
            try_--;
            if (ntoo >= AVOID_MAX) try_ = TRIES - 2;
            continue;
        }
        if (try_ == TRIES - 1) {
            static const uint8_t drop[] = { 3, 2, 0, 4, 5, 6 };      /* tag, callback, opener, hand-off, reason, hook */
            for (k = 0; k < 6 && !ok; k++) {
                use[drop[k]] = 0;
                join(line, (int)sizeof line, parts, use, 7, NULL, lower);
                ok = fits(line);
            }
        }
    }
    if (!ok) {
        safe_line(g, out, tr);
        trace_plan(g, tr);
        out->flags |= TLF_FALLBACK;
        return 0;
    }
    /* a reply that stays under 20 visible characters (a bare "Hm.") or answers a thanks, an apology, a compliment or a
     * feeling gets a small acknowledgement that asks something back (Q8e; hook.ack): decided on the line as it fits */
    if (optional && !use[6] && !g->grief && !(ai.rflags & (RF_SINCERE | RF_ROMANCE | RF_REFUSAL)) &&
        !TA_IS_ROMANCE(g->act) && !TA_IS_SAFETY(g->act) && g->act != TA_UNKNOWN) {
        int a = g->act, n, want;
        int calm = talk_gen_act_class(a) != TAC_GESTURE && a != TA_GREET && a != TA_FAREWELL && a != TA_LAUGH;
        int kindly = a == TA_THANK || a == TA_APOLOGIZE || a == TA_COMPLIMENT || a == TA_ASK_FEELING;
        join(line, (int)sizeof line, parts, use, 7, NULL, lower);
        n = vis_len(line);
        want = n < 8 || (calm && n < 20) || (calm && kindly && !strchr(line, '?') && rng_range(&g->rng, 100) < 30);
        if (want) {
            g->hook_now = TH_ACK;
            use[6] = make_part(g, entry_sym(g, ENT_FIXED, TGE_HOOK), 0, &parts[6]);
            g->hook_now = 0;
            if (use[6]) {
                static const uint8_t shed2[] = { 3, 2, 0, 4, 5 };
                if (lower) lower_codes(parts[6].text);
                join(line, (int)sizeof line, parts, use, 7, NULL, lower);
                for (k = 0; k < 5 && !fits(line); k++) {
                    if (!use[shed2[k]]) continue;
                    use[shed2[k]] = 0;
                    join(line, (int)sizeof line, parts, use, 7, NULL, lower);
                }
                if (fits(line)) g->plan.hook = TH_ACK; else use[6] = 0;
            }
        }
    }
    join(line, (int)sizeof line, parts, use, 7, tr, lower);
    excl_budget(line, g->pers[PERS_EXPRESSION]);
    if (!lower) cap_first(line);
    copy_cap(out->text, line, TALK_LINE_MAX);
    /* what the line does and is about */
    if (talk_gen_alt_info(parts[1].alt, &ai)) {
        out->act = ai.act;
        if (ai.rflags & RF_SINCERE) out->flags |= TLF_SINCERE;
    }
    for (k = 0; k < 7; k++) if (use[k] && parts[k].face && !out->face) out->face = parts[k].face;
    if (parts[1].flags & AF_TOPIC) out->topic = g->topic;
    else if (parts[1].flags & AF_LIKE) out->topic = g->like_pick;
    if (use[4]) out->flags |= TLF_HANDOFF;
    if (!use[6]) g->plan.hook = 0;
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
        trace_plan(g, tr);
    }
    /* the conversation's memory of this line */
    for (k = TALK_NOVELTY - 1; k > 0; k--) c->recent[g->who][k] = c->recent[g->who][k - 1];
    c->recent[g->who][0] = (uint16_t)(parts[1].alt + 1);
    for (k = TALK_ROOM_NOVELTY - 1; k > 0; k--) c->room_recent[k] = c->room_recent[k - 1];
    c->room_recent[0] = (uint16_t)(parts[1].alt + 1);
    c->spoke |= (uint8_t)(1u << g->who);
    for (k = 0; k < 7; k++) if (use[k] && (parts[k].flags & AF_CATCH)) c->catch_used |= (uint8_t)(1u << g->who);
    if (g->unk == TU_NAME && talk_safe_lint_probe(out->text, &c->aud, 1) != TL_OK) {   /* E6: the named line must pass S3 */
        g->unk_fail = 1;
        return 0;
    }
    /* S3: the output lint, every line */
    swears = c->swears;
    if ((k = talk_safe_lint(out->text, &c->aud, &swears)) != TL_OK) {
        char m[TALK_LINE_MAX + 24];
        doomo_snprintf(m, (int)sizeof m, "talk: lint %d: %s", k, out->text);   /* design 7.2: release builds log it */
        plat_log(m);
        safe_line(g, out, tr);
        trace_plan(g, tr);
        out->flags |= TLF_SAFE;
        return 0;
    }
    c->swears = (uint8_t)swears;
    return 1;
}

/* K-10: a grief or loss topic */
static int grief_topic(const Gen *g) { return g->topic && (g->tc.flags & TCF_GRIEF); }

/* K-10: the sincere line from entry `which` (TGE_GRIEF, TGE_COMFORT): the core
 * part alone, a reply act that may answer g->act, no punchline */
static int grief_line(Gen *g, int which, TalkLine *out, TalkTrace *tr)
{
    int sym = entry_sym(g, ENT_FIXED, which), r = 0;
    g->grief = 1;
    g->adj = 1;
    if (sym >= 0) r = gen_line(g, sym, 0, out, tr);
    g->adj = 0;
    if (sym < 0 || !r) {                     /* no bank (a test grammar) or nothing fitted: a safe line */
        if (sym < 0) safe_line(g, out, tr);
        out->flags |= TLF_FALLBACK;
        r = 0;
    }
    out->flags |= TLF_SINCERE;
    return r;
}

/* TM2c: the kinds a kid room still has after `act`: the cool reply acts (tease, mock, insult, threaten) are out,
 * so a kind only they served is out too (the bank's own cells are in row[15], this narrows them) */
static int kinds_kid(int act, const uint8_t row[16])
{
    int r, m = 0;
    for (r = 1; r < TA_COUNT; r++)
        if (((le32(row + 4 * (r >> 5)) >> (r & 31)) & 1u) && !TALK_COOL_REPLY(r)) m |= talk_gen_kinds_of(r);
    (void)act;
    return m & row[15];
}

/* TM2c: the plan of an answer line (layer a). Never for a safety act, an unknown act, grief or the pet: those
 * keep the old path (a sincere core, no plan, no filter). The gate has already said pass. */
static void make_plan(Gen *g, int act)
{
    TalkPlanIn in;
    TalkConv *c = g->c;
    uint8_t row[16], drow[16];
    TalkTraitBias b;
    int root = g->topic ? talk_db_cat_root(g->tc.cat) : 0, forced = (c->plain >> 1) & 7, i;
    if (!g->r || c->id[g->who] == TALK_PET_ID || act <= 0 || act >= TA_COUNT || TA_IS_SAFETY(act) || act == TA_UNKNOWN ||
        grief_topic(g) || !act_row(act, row) || !row[15] || !act_row(row[13], drow))
        return;
    if (g->kid_room && !kinds_kid(act, row)) return;
    memset(&in, 0, sizeof in);
    in.act = (uint8_t)act;
    in.allowed = g->kid_room ? (uint8_t)kinds_kid(act, row) : row[15];
    in.dflt = drow[14];
    in.lean = g->lean;
    in.rel = g->rel;
    in.mood = g->mood;
    in.stage = g->stage;
    in.kid_room = g->kid_room;
    in.consent = (uint8_t)((c->consent >> g->who) & 1);
    in.to_minor = (uint8_t)(g->to == -1 ? (c->aud.avatar_minor != 0) :
                            g->to_id >= 0 && g->to_id != TALK_PET_ID && talk_safe_minor(g->to_id));
    memcpy(in.eff, g->pers, sizeof in.eff);
    if (g->has_ts) {
        b = talk_trait_bias(&g->ts, g->stage, act, root, g->kid_room);
        in.accept = b.accept;
        in.deny = b.deny;
        for (i = 0; i < TALK_TRAIT_TOPICS; i++) {
            if (root && g->ts.like[i] == root) in.topic_pref = 1;
            if (root && g->ts.dislike[i] == root) in.topic_pref = -1;
        }
    }
    in.margin = (uint8_t)(talk_gen_act_class(act) == TAC_ASK ? PLAN_MARGIN_ASK : PLAN_MARGIN);
    in.seed = doomo_hash32(c->seed ^ 0x504C414Eu ^ ((uint32_t)c->turn << 8) ^ ((uint32_t)g->who * 0x9E3779B1u) ^
                           ((uint32_t)c->lines * 0x85EBCA6Bu));
    talk_plan_kind(&in, &g->plan);
    if (forced && g->plan.kind && (g->plan.allowed & TKM(forced)) && !TA_IS_ROMANCE(act)) {      /* the creator's preview */
        if (forced == TK_AGREE && g->lean < 0 && (g->plan.allowed & TKM(TK_DENY))) forced = TK_DENY;
        g->plan.kind = (uint8_t)forced;
        g->plan.flags = (uint8_t)((g->plan.flags | TPL_FORCED) & ~TPL_OPEN);
    }
}

/* ---- TM2d: topic depth (design 3.1), the second line, the unknown word (design 4) ----------------------- */
static int root_of_topic(uint16_t topic)
{
    TalkConcept tc;
    return topic && talk_db_concept(topic, &tc) ? talk_db_cat_root(tc.cat) : 0;
}

/* the same topic, or one hop from it (the word network is symmetric enough: either side counts) */
static int same_thread(uint16_t a, uint16_t b)
{
    uint16_t ids[TALK_ASSOC_TOP];
    int n, i;
    if (!a || !b) return 0;
    if (a == b) return 1;
    n = talk_db_assoc(a, ids, NULL);
    for (i = 0; i < n; i++) if (ids[i] == b) return 1;
    n = talk_db_assoc(b, ids, NULL);
    for (i = 0; i < n; i++) if (ids[i] == a) return 1;
    return 0;
}

/* The depth of this answer line: the state of the speaker's thread moves by one step (rise, hold, fall, reset), the
 * plan gets the level and the content, and a story or a secret asks for a second line (TalkConv.more). Never for a
 * line without a plan kind (a safety act, the pet, grief, an unknown act). */
static void depth_plan(Gen *g, int act)
{
    TalkConv *c = g->c;
    TalkDepthIn in;
    uint16_t th = (uint16_t)TDEP_TOPIC(c->dep_tw[g->who]);
    int lvl, who = g->who, rv;
    c->more = 0;
    if (!g->r || c->id[who] == TALK_PET_ID || act <= 0 || act >= TA_COUNT || TA_IS_SAFETY(act) || TA_IS_ROMANCE(act) ||
        act == TA_UNKNOWN)
        return;
    memset(&in, 0, sizeof in);
    in.act = (uint8_t)act;
    in.rel = g->rel;
    in.stage = g->stage;
    in.kid_room = g->kid_room;
    in.engage = c->engage ? c->engage : TALK_ENGAGE_START;
    rv = g->to_id >= 0 ? rel_value(g->sid, g->to_id) : 0;
    in.level = (uint8_t)(rv < 0 ? 0 : rv > 100 ? 100 : rv);
    in.root = (uint8_t)(g->topic ? talk_db_cat_root(g->tc.cat) : 0);
    in.thread_root = (uint8_t)root_of_topic(th);
    in.same = (uint8_t)(g->topic && same_thread(g->topic, th));
    in.grief = (uint8_t)grief_topic(g);
    if (g->has_ts) {
        in.shy = (uint8_t)talk_traitset_has(&g->ts, TT_SHY);
        in.paranoid = (uint8_t)talk_traitset_has(&g->ts, TT_PARANOID);
        in.outgoing = (uint8_t)talk_traitset_has(&g->ts, TT_OUTGOING);
        in.loyal = (uint8_t)talk_traitset_has(&g->ts, TT_LOYAL);
        in.gloomy = (uint8_t)talk_traitset_has(&g->ts, TT_GLOOMY);
    }
    lvl = talk_depth_step(&in, &c->dep[who], &c->dep_tw[who], g->topic);
    if ((unsigned)lvl > TDEP_MEM_LEVEL(c->mem_dep[who])) c->mem_dep[who] = (uint8_t)((c->mem_dep[who] & ~3u) | (unsigned)lvl);
    if (!g->plan.kind) return;
    c->dep_tw[who] = (uint16_t)(TDEP_TOPIC(c->dep_tw[who]) | ((unsigned)g->plan.kind << 12));
    g->plan.depth = (uint8_t)lvl;
    g->plan.content = (uint8_t)(!g->topic ? TCN_NONE : lvl == 0 ? TCN_STANCE : lvl == 1 ? TCN_REASON : lvl == 2 ? TCN_STORY : TCN_SECRET);
    if (lvl >= 2 && TDEP_TOPIC(c->dep_tw[who]))
        c->more = (uint16_t)((lvl == 2 ? TMORE_STORY : TMORE_SECRET) | ((unsigned)who << 2) | ((unsigned)act << 8));
}

/* The reply act a second line carries: the first of a few that may answer the player's act (acts.tsv), so Q2 holds
 * for the line as it did for the first. 0 when none (a compliment, a thanks: nothing to tell). */
static int more_act(int act)
{
    static const uint8_t cand[] = { TA_ANSWER, TA_TELL, TA_OPINE_POS, TA_AGREE, TA_BACKCHANNEL };
    int k;
    for (k = 0; k < (int)sizeof cand; k++) if (talk_gen_adjacent(act, cand[k])) return cand[k];
    return 0;
}

/* the secrets a speaker may tell: a like, the fear or the hated food the player has not learned (and none told this
 * talk or today), or a small problem from the safe set. Kid-safe by construction (design 3.3): never a crush, a
 * break-up, a proposal, possession, corruption, a nightmare or a fight. Returns TSEC_* + 1 and sets *which (the
 * like index, 5 for a problem), 0 when there is nothing to tell. */
static int pick_secret(Gen *g, int *which)
{
    int cand[6], kind[6], n = 0, w;
    TalkConv *c = g->c;
    if (c->mem_dep[g->who] & (TDEP_MEM_SECRET(0) | TDEP_MEM_SECRET(1) | TDEP_MEM_SECRET(2) | TDEP_MEM_SECRET(3))) return 0;
    for (w = 0; w < 5; w++) {
        if (!g->like[w] || talk_mem_like_known(g->sid, w) || !talk_gen_concept_ok(g->like[w], &c->aud)) continue;
        cand[n] = w;
        kind[n++] = w < 3 ? TSEC_LIKE : w == 3 ? TSEC_FEAR : TSEC_HATE;
    }
    if (g->prob == PROB_HUNGRY || g->prob == PROB_DIRTY || g->prob == PROB_BORED || g->prob == PROB_SICK ||
        g->prob == PROB_WANTS_ITEM || g->prob == PROB_WANTS_FRIEND) {
        cand[n] = 5;
        kind[n++] = TSEC_PROBLEM;
    }
    if (!n) return 0;
    w = rng_range(&g->rng, n);
    *which = cand[w];
    return kind[w] + 1;
}

int talk_gen_more(TalkConv *c, TalkLine *out, TalkTrace *tr)
{
    Gen g;
    unsigned m;
    int who, kind, act, sym, r, which = 0, rv;
    if (!out) return 0;
    memset(out, 0, sizeof *out);
    if (!c || !c->more) return 0;
    m = c->more;
    c->more = 0;
    kind = (int)(m & 3u);
    who = (int)((m >> 2) & 3u);
    act = (int)(m >> 8);
    if (who >= c->n || c->id[who] == TALK_PET_ID || c->care || c->ended || !(act = more_act(act))) return 0;
    if (!setup(&g, c, who, -1, TA_TELL, TDEP_TOPIC(c->dep_tw[who]))) return 0;
    if (!g.r || grief_topic(&g)) return 0;
    g.dlvl = (uint8_t)(kind == TMORE_SECRET ? 3 : 2);
    if (kind == TMORE_SECRET) {
        /* a friend whose bond is 40 or more, or a loyal one, counts as a best friend for the secret rules (the gate said so) */
        if (g.rel == TRB_FRIEND) {
            rv = g.to_id >= 0 ? rel_value(g.sid, g.to_id) : 0;
            if (rv >= 40 || (g.has_ts && talk_traitset_has(&g.ts, TT_LOYAL))) g.rel = TRB_BEST;
        }
        g.secret = (uint8_t)pick_secret(&g, &which);
        if (!g.secret) return 0;
        if (which < 3) g.like_pick = g.like[which];
        sym = entry_sym(&g, ENT_FIXED, TGE_SECRET);
    } else {
        sym = entry_sym(&g, ENT_FIXED, TGE_STORY);
    }
    if (sym < 0) return 0;
    g.plan.kind = 0;
    r = gen_line(&g, sym, 0, out, tr);
    if (!r || (out->flags & (TLF_SAFE | TLF_FALLBACK))) {
        if (!(out->flags & TLF_SAFE)) memset(out, 0, sizeof *out);
        return 0;
    }
    out->who = (int8_t)who;
    out->to = -1;
    out->act = (uint8_t)act;
    out->flags |= TLF_MORE;
    if (kind == TMORE_SECRET) {
        c->mem_dep[who] |= (uint8_t)TDEP_MEM_SECRET(g.secret - 1);
        if (tr) tr->plan.depth = 3;
    }
    if (tr) { tr->plan.content = (uint8_t)(kind == TMORE_SECRET ? TCN_SECRET : TCN_STORY); tr->plan.depth = g.dlvl; }
    return 1;
}

#ifndef TALK_NO_UNK
/* ---- the unknown word (design 4) -------------------------------------------------------------------------- */
/* E1 to E6 on the word the parser pointed at, from the bubble text only (E2); the kid-room length rule. 0: do not echo */
int talk_gen_unk_word(const TalkConv *c, const TalkUtter *u, char out[18])
{
    int off = u->unk_off, len = u->unk_len, i, n = 0, sep = 0, unsafe = 0;
    char w[20];
    uint16_t cid;
    int kid = c->aud.any_minor || c->aud.avatar_minor || c->aud.target_minor;
    out[0] = 0;
    if (len < 3 || len > 16 || off + len > (int)strlen(u->shown)) return 0;
    if (u->safety) return 0;                                              /* E5: the player's safety bits are 0 (a masked word is a bit too) */
    if (c->ended || c->care) return 0;                                    /* E5: not a K-7 room, not in the care window */
    for (i = 0; i < len; i++) {                                           /* E3: ASCII letters and one apostrophe or hyphen, no digits, no shout */
        char ch = u->shown[off + i];
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        if (ch >= 'a' && ch <= 'z') { w[n++] = ch; continue; }
        if ((ch == '\'' || ch == '-') && !sep && i > 0 && i < len - 1) { sep = 1; w[n++] = ch; continue; }
        return 0;
    }
    {
        int upper = 0;
        for (i = 0; i < len; i++) if (u->shown[off + i] >= 'A' && u->shown[off + i] <= 'Z') upper++;
        if (upper == len && len >= 3) return 0;                           /* a shout */
    }
    w[n] = 0;
    if (kid && n > 12) return 0;                                          /* a kid room echoes nothing long */
    if (talk_safe_hate_word(w, 1)) return 0;                              /* E4 */
    if (talk_lex_known(w, NULL)) return 0;                                /* E1: it is not a word the lexicon knows */
    (void)talk_lex_near(w, &cid, &unsafe);
    if (unsafe) return 0;                                                 /* E4: within one edit of a register, minor, hate word */
    {                                                                      /* ... and no such word inside it ("hitlerish"): every part of 3 letters or more */
        int a, l;
        for (a = 0; a < n; a++)
            for (l = 3; a + l <= n; l++) {
                char sub[20];
                uint16_t wf = 0;
                memcpy(sub, w + a, (size_t)l);
                sub[l] = 0;
                if (talk_lex_known(sub, &wf) && (wf & (TW_SUGGEST | TW_SEXUAL | TW_HATE | TW_DISTRESS | TW_MINOR | TW_REALWORLD |
                                                       TW_STRONG | TW_CRUDE | TW_CAPPED | TW_GROUP | TW_MINORX)))
                    return 0;
                if (l >= 4 && talk_safe_hate_word(sub, 0)) return 0;
            }
    }
    if (talk_safe_lint_probe(w, &c->aud, 1) != TL_OK) return 0;           /* E6 (the word alone; the line is probed again) */
    memcpy(out, w, (size_t)n + 1);
    return n;
}

void talk_gen_unk_prepare(TalkConv *c, const TalkUtter *u)
{
    char w[18], raw[20];
    int unsafe = 0, k, n = 0, valid = 1;
    uint16_t cid = 0;
    if (!c) return;
    TC_SET_UNK_MODE(c, 0);
    c->unk[0] = 0;
    c->unk_guess = 0;
    if (!u || u->act[0] != TA_UNKNOWN) return;
    TC_SET_UNK_MODE(c, TU_ASK);                                                 /* no word to speak of: a plain ask */
    if (u->safety) { TC_SET_UNK_MODE(c, TU_NOECHO); return; }                    /* a masked or flagged line: nothing is echoed or guessed */
    if (u->unk_len < 3) return;
    if (u->unk_off + u->unk_len > (int)strlen(u->shown) || u->unk_len > 16) { TC_SET_UNK_MODE(c, TU_NOECHO); return; }
    for (k = 0; k < u->unk_len; k++) {
        char ch = u->shown[u->unk_off + k];
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        if (!((ch >= 'a' && ch <= 'z') || ch == '\'' || ch == '-')) valid = 0;
        raw[n++] = ch;
    }
    raw[n] = 0;
    TC_SET_UNK_MODE(c, TU_NOECHO);
    /* step 1: a guess is a real concept, so only for a word that is itself harmless (E4) in a room that may say it */
    if (valid && !talk_safe_hate_word(raw, 1) && talk_lex_near(raw, &cid, &unsafe) && !unsafe && talk_gen_concept_ok(cid, &c->aud)) {
        TalkConcept tc;
        if (talk_db_concept(cid, &tc) && !(tc.flags & TCF_GRIEF)) { c->unk_guess = cid; TC_SET_UNK_MODE(c, TU_GUESS); }
    }
    /* step 2: the word named back, only when every fence holds, once a talk */
    if (!TC_UNK_SAID(c) && talk_gen_unk_word(c, u, w)) {
        memcpy(c->unk, w, strlen(w) + 1);
        if (TC_UNK_MODE(c) != TU_GUESS) TC_SET_UNK_MODE(c, TU_NAME);               /* with a guess too, the line draws one of the two */
    }
}

/* The answer to an unknown line: unk.guess, unk.name, unk.askmeaning or unk.noecho, all of which ask what kind of
 * thing it is. 0 when nothing fits (the old core.unknown bank answers then). */
static int unk_line(Gen *g, TalkLine *out, TalkTrace *tr)
{
    TalkConv *c = g->c;
    int sym = entry_sym(g, ENT_FIXED, TGE_UNK), mode = (int)TC_UNK_MODE(c), r = 0, i;
    if (sym < 0 || !g->r) return 0;
    if (mode == TU_GUESS && c->unk[0] && !TC_UNK_SAID(c) && rng_range(&g->rng, 2)) mode = TU_NAME;
    if (mode == TU_NAME && (TC_UNK_SAID(c) || !c->unk[0])) mode = TU_ASK;
    for (i = 0; i < 3; i++) {
        g->unk = (uint8_t)(mode ? mode : TU_ASK);
        g->unk_fail = 0;
        if (g->unk == TU_GUESS) {
            g->topic = c->unk_guess;
            if (!g->topic || !talk_db_concept(g->topic, &g->tc)) { mode = c->unk[0] && !TC_UNK_SAID(c) ? TU_NAME : TU_ASK; continue; }
        } else {
            g->topic = 0;
        }
        g->plan.kind = 0;
        g->plan.content = TCN_NAME;
        g->plan.hook = TH_ASK;
        r = gen_line(g, sym, g->unk != TU_NAME, out, tr);
        if (r && !(out->flags & (TLF_SAFE | TLF_FALLBACK))) {
            if (g->unk == TU_NAME) TC_SET_UNK_SAID(c);
            if (g->unk == TU_NAME || g->unk == TU_ASK || g->unk == TU_NOECHO) {      /* the player may say what kind of thing it is */
                int h = 0, k, at = -1;
                for (k = 0; c->unk[k]; k++) h = (h * 31 + c->unk[k]) & 0xFFFF;
                if (h) for (k = 0; k < 4; k++) if (c->taught_hash[k] == h) at = k;
                if (at < 0) {                                                       /* a new word: the front of the ring of 4 */
                    for (k = 3; k > 0; k--) { c->taught_hash[k] = c->taught_hash[k - 1]; c->taught_root[k] = c->taught_root[k - 1]; }
                    c->taught_hash[0] = (uint16_t)h;
                    c->taught_root[0] = 0;
                    at = 0;
                }
                TC_SET_TAUGHT(c, at + 1);
            }
            out->topic = TC_NONE;                                                   /* a guess is no topic of the player's (Q2) */
            return 1;
        }
        if (g->unk_fail || g->unk == TU_NAME) { mode = TU_NOECHO; continue; }
        if (g->unk == TU_GUESS) { mode = TU_ASK; continue; }
        break;
    }
    memset(out, 0, sizeof *out);
    return 0;
}
#else
/* the GBA has no typed line: nothing to name, guess or ask about */
int talk_gen_unk_word(const TalkConv *c, const TalkUtter *u, char out[18])
{
    (void)c; (void)u;
    out[0] = 0;
    return 0;
}

void talk_gen_unk_prepare(TalkConv *c, const TalkUtter *u) { (void)c; (void)u; }

static int unk_line(Gen *g, TalkLine *out, TalkTrace *tr) { (void)g; (void)out; (void)tr; return 0; }
#endif

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
    if ((grief_topic(&g) || c->care) && !TA_IS_SAFETY(act)) { /* K-10: grief is not a punchline; K-11: nor is the turn after distress */
        r = grief_line(&g, TGE_GRIEF, out, tr);
        out->who = (int8_t)who;
        out->to = (int8_t)to;
        return r;
    }
    c->more = 0;
    if (act == TA_UNKNOWN && TC_UNK_MODE(c) && !(c->plain & 1)) {                /* TM2d: guess, name or ask (design 4) */
        r = unk_line(&g, out, tr);
        if (r) { out->who = (int8_t)who; out->to = (int8_t)to; return r; }
        setup(&g, c, who, to, act, topic);                                      /* nothing fitted: the old bank answers */
        c->lines--;
    }
#ifndef TALK_NO_UNK
    if (TC_TAUGHT(c) && act != TA_UNKNOWN && !TA_IS_SAFETY(act) && g.topic && !(c->plain & 1)) {   /* "it's a food": the kind it was asked */
        int root = root_of_topic(g.topic), at = (int)TC_TAUGHT(c) - 1;
        TC_SET_TAUGHT(c, 0);
        if (root && at < 4 && entry_sym(&g, ENT_FIXED, TGE_UNK) >= 0) {
            c->taught_root[at] = (uint8_t)root;
            g.unk = TU_TAUGHT;
            g.plan.kind = 0;
            g.plan.content = TCN_NAME;
            r = gen_line(&g, entry_sym(&g, ENT_FIXED, TGE_UNK), 0, out, tr);
            if (r && !(out->flags & (TLF_SAFE | TLF_FALLBACK))) { out->who = (int8_t)who; out->to = (int8_t)to; return r; }
            setup(&g, c, who, to, act, topic);
            c->lines--;
        }
    }
#endif
    TC_SET_TAUGHT(c, 0);
    sym = act > 0 && act < TA_COUNT ? entry_sym(&g, ENT_CORE, act) : -1;
    if (sym < 0) sym = entry_sym(&g, ENT_CORE, TA_UNKNOWN);
    make_plan(&g, act);
    if (!(c->plain & 1)) depth_plan(&g, act);
    r = gen_line(&g, sym, !TA_IS_SAFETY(act) && !(c->plain & 1), out, tr);
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
    if (grief_topic(&g) || (prev->flags & TLF_SINCERE) || c->care) {
        /* K-10: after a sincere line, a sincere comfort line or nothing (the pet stays quiet) */
        if (c->id[who] == TALK_PET_ID || !grief_topic(&g) ||
            !(talk_gen_adjacent(prev->act, TA_AGREE) || talk_gen_adjacent(prev->act, TA_BACKCHANNEL)))
            return 0;
        k = grief_line(&g, TGE_COMFORT, out, tr);
        out->flags |= TLF_REACTION;
        if (!k) { memset(out, 0, sizeof *out); return 0; }  /* no comfort fitted: no reaction at all */
        return k;
    }
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

int talk_gen_react(TalkConv *c, int who, const TalkLine *prev, uint16_t topic, TalkLine *out, TalkTrace *tr)
{
    int r;
    if (!out) return 0;
    memset(out, 0, sizeof *out);
    if (!c || !prev || who < 0 || who >= c->n || who == prev->who) return 0;
    r = react_line(c, who, prev, topic, out, tr);
    return r || out->text[0];
}

/* (T6) a fixed entry's line: the drama manager's own lines */
int talk_gen_entry(TalkConv *c, int who, int to, int entry, int act, uint16_t topic, TalkLine *out, TalkTrace *tr)
{
    Gen g;
    int sym, r;
    if (!out) return 0;
    memset(out, 0, sizeof *out);
    if (entry < 0 || entry >= TGE_COUNT || !setup(&g, c, who, to, act, topic)) return 0;
    if (c->id[who] == TALK_PET_ID) return 0;
    if (grief_topic(&g)) {                                   /* K-10: grief is nobody's beat line */
        c->lines--;
        if (!setup(&g, c, who, to, act, TC_NONE)) return 0;
    }
    sym = entry_sym(&g, ENT_FIXED, entry);
    if (sym < 0) return 0;
    r = gen_line(&g, sym, 0, out, tr);
    out->who = (int8_t)who;
    out->to = (int8_t)to;
    return r;
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
    int ids[TALK_PARTS_MAX], k, g, target, target_id, resp, nre = 0, maxre;
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
    if (g == TG_PASS) talk_gen_unk_prepare(c, u);                                    /* TM2d: the unknown word */
    talk_gen_line(c, resp, -1, u->act[0], u->topic[0], &x->line[0], tr ? &tr[0] : NULL);
    x->n = 1;
    if (g == TG_DEFLECT && target >= 0) c->refused |= (uint8_t)(1u << target);      /* K-5: no more today */
    if (g == TG_PASS && TA_IS_ROMANCE(u->act[0]) && !((c->consent >> resp) & 1))
        c->refused |= (uint8_t)(1u << resp);                                         /* a no ends it (K-5) */
    if (g != TG_PASS || TA_IS_ROMANCE(u->act[0]) || c->id[resp] == TALK_PET_ID) { c->more = 0; return x->n; }
    if (c->more && !(x->line[0].flags & (TLF_SAFE | TLF_FALLBACK | TLF_SINCERE)) &&      /* TM2d: the story or the secret, right after the answer */
        talk_gen_more(c, &x->line[1], tr ? &tr[1] : NULL))
        x->n = 2;
    c->more = 0;
    /* reactions between the residents: each other one may chime in, two at most
     * (one after a sincere grief line, K-10: a comfort line, no pile-on) */
    maxre = (x->line[0].flags & TLF_SINCERE) ? 1 : 2;
    for (k = 0; k < c->n && x->n < TALK_EXCHANGE_MAX && nre < maxre; k++) {
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
