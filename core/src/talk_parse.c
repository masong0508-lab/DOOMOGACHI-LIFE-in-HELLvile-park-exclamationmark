/* talk_parse.c - Talk T3: the free-text parser's rules (owner: TK;
 * docs/TALK.md "The parser"; design section 3.2 stage 2).
 *
 * Dreamcast and native only (the PS1 composer builds TalkUtter directly;
 * --gc-sections drops this file on the PS1 and the GBA).
 *
 * talk_parse(): tag the line (talk_lex.c), classify its safety (S1,
 * talk_safe_scan: it runs FIRST and an explicit, hate or distress finding
 * wins over every rule), then run the rules of TB_PARSE in priority order
 * (core/data/talk/rules/<lang>.tsv, compiled by talkc_parse.py). The first
 * rule that matches gives act[0], the next matching rule with another act
 * act[1]. A capture names the topic (or the person) an act is about; the
 * rest of the topic comes from spreading activation. The target is a
 * participant named in the line, "everyone", or the facing ring.
 *
 * Deterministic: no RNG, no state between calls. Stack: about 6 KB with
 * the tagger's (a TalkParseInfo of 40 tokens when the caller passes none).
 */
#include "doomo/doomo.h"
#include "doomo/talk.h"

#define PRS_MAGIC 0x31535250u
#define RULE_HDR  8u
#define EL_SIZE   24u
#define EL_MAX    12

/* talkc_parse.py: the classes a rules file must declare first, in this order */
enum { TCL_EVERYONE = 0, TCL_HATEV, TCL_YEARS, TCL_YOU, TCL_I, TCL_IM, TCL_GREETW, TCL_HATEADJ, TCL_ATTRACT };
/* talkc_parse.py PREDS */
enum { P_ANY, P_CONCEPT, P_PERSON, P_ABSENT, P_ADJPOS, P_ADJNEG, P_QMARK, P_EXCL, P_NUM, P_UNK,
       P_SEXUAL, P_SUGGEST, P_HATE, P_DISTRESS, P_MINOR, P_REALWORLD, P_STRONG, P_CRUDE, P_FOOD,
       P_ACTIVITY, P_FEELING, P_PLACE, P_THING, P_ROMANCE, P_PEOPLE, P_ADJ, P_JOB, P_TIME,
       P_CREATURE, P_FEAR, P_NAME, P_LORE };
enum { OP_MATCH = 0, OP_GAP, OP_START, OP_END };
#define EF_OPT 1u
#define EF_NEG 2u
#define EF_STAR 4u
#define EF_OREND 8u

#define LIT_MAX   64
typedef struct El { uint32_t cls[4], preds; uint8_t op, flags, cap, nlit, lit0; } El;
typedef struct Rule { uint16_t prio; uint8_t act[2], cap[2], nel, nlit; El el[EL_MAX]; uint32_t lit[LIT_MAX]; } Rule;

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int has_class(const TalkTok *t, int bit) { return (t->tags[bit >> 5] >> (bit & 31)) & 1u; }

static int is_proper(int concept)
{
    TalkConcept c;
    return concept && talk_db_concept(concept, &c) && (c.flags & TCF_PROPER);
}

static int talkable(const TalkTok *t)
{
    TalkConcept c;
    uint8_t parent, fl;
    if (!t->concept || (t->wflags & TW_FUNC)) return 0;
    if (t->kind == TTK_PERSON && !(t->flags & TTF_ABSENT)) return 0;
    if (!talk_db_concept(t->concept, &c) || c.age == TAGE_NEVER || !talk_db_cat(c.cat, &parent, &fl)) return 0;
    return (fl & TCAT_TALKABLE) && !(fl & (TCAT_SAFETY | TCAT_DESCRIBE));
}

static int pred_ok(const TalkTok *t, int p)
{
    int r = t->root;
    switch (p) {
    case P_ANY:       return 1;
    case P_CONCEPT:   return talkable(t);
    case P_PERSON:    return t->kind == TTK_PERSON && !(t->flags & TTF_ABSENT);
    case P_ABSENT:    return (t->kind == TTK_PERSON && (t->flags & TTF_ABSENT)) ||
                             (t->kind != TTK_PERSON && (r == TCR_LORE || r == TCR_PEOPLE || r == TCR_HELLVILE) &&
                              is_proper(t->concept));
    case P_ADJPOS:    return r == TCR_DESCRIBE && t->sent > 0;
    case P_ADJNEG:    return r == TCR_DESCRIBE && t->sent < 0;
    case P_QMARK:     return t->kind == TTK_QMARK;
    case P_EXCL:      return t->kind == TTK_EXCL;
    case P_NUM:       return t->kind == TTK_NUM;
    case P_UNK:       return (t->flags & TTF_UNK) != 0;
    case P_SEXUAL:    return (t->wflags & TW_SEXUAL) != 0;
    case P_SUGGEST:   return (t->wflags & TW_SUGGEST) != 0;
    case P_HATE:      return (t->wflags & TW_HATE) != 0;
    case P_DISTRESS:  return (t->wflags & TW_DISTRESS) != 0;
    case P_MINOR:     return (t->wflags & TW_MINOR) != 0;
    case P_REALWORLD: return (t->wflags & TW_REALWORLD) != 0;
    case P_STRONG:    return (t->wflags & TW_STRONG) != 0;
    case P_CRUDE:     return (t->wflags & TW_CRUDE) != 0;
    case P_FOOD:      return r == TCR_FOOD;
    case P_ACTIVITY:  return r == TCR_HOBBIES || r == TCR_MUSIC;
    case P_FEELING:   return r == TCR_FEELINGS;
    case P_PLACE:     return r == TCR_PLACES;
    case P_THING:     return r == TCR_STUFF || r == TCR_HOUSE;
    case P_ROMANCE:   return r == TCR_ROMANCE;
    case P_PEOPLE:    return r == TCR_PEOPLE;
    case P_ADJ:       return r == TCR_DESCRIBE;
    case P_JOB:       return r == TCR_WORK;
    case P_TIME:      return r == TCR_TIME;
    case P_CREATURE:  return r == TCR_CREATURES;
    case P_FEAR:      return r == TCR_FEARS;
    case P_NAME:      return t->kind == TTK_PERSON || is_proper(t->concept);
    case P_LORE:      return r == TCR_LORE || r == TCR_HELLVILE;
    }
    return 0;
}

static int atom_ok(const Rule *r, const El *e, const TalkTok *t)
{
    int i;
    for (i = 0; i < 4; i++) if (e->cls[i] & t->tags[i]) return 1;
    for (i = 0; i < e->nlit; i++) if (r->lit[e->lit0 + i] == t->hash) return 1;
    for (i = 0; i < 32; i++) if (((e->preds >> i) & 1u) && pred_ok(t, i)) return 1;
    return 0;
}

typedef struct Mt { const Rule *r; const TalkTok *tok; int n; int cap[4]; } Mt;

static int m(Mt *M, int ei, int ti)
{
    const El *e;
    if (ei == M->r->nel) return 1;
    e = &M->r->el[ei];
    switch (e->op) {
    case OP_START: return ti == 0 && m(M, ei + 1, ti);
    case OP_END:   return ti == M->n && m(M, ei + 1, ti);
    case OP_GAP: {
        int k;
        for (k = ti; k <= M->n; k++) if (m(M, ei + 1, k)) return 1;
        return 0;
    }
    default:
        if (e->flags & EF_STAR) {                      /* lazy: as few as will do */
            int k = ti;
            for (;;) {
                if (m(M, ei + 1, k)) return 1;
                if (k >= M->n || atom_ok(M->r, e, &M->tok[k]) == ((e->flags & EF_NEG) != 0)) return 0;
                if (e->cap) M->cap[e->cap] = k;
                k++;
            }
        }
        if ((e->flags & EF_OREND) && ti == M->n) return m(M, ei + 1, ti);
        if (ti < M->n && atom_ok(M->r, e, &M->tok[ti]) != ((e->flags & EF_NEG) != 0)) {
            int old = e->cap ? M->cap[e->cap] : 0;
            if (e->cap) M->cap[e->cap] = ti;
            if (m(M, ei + 1, ti + 1)) return 1;
            if (e->cap) M->cap[e->cap] = old;
        }
        if (e->flags & EF_OPT) return m(M, ei + 1, ti);
        return 0;
    }
}

static int rule_match1(const Rule *r, const TalkTok *tok, int n, int cap[4])
{
    Mt M;
    int s, i;
    M.r = r; M.tok = tok; M.n = n;
    for (s = 0; s <= n; s++) {
        for (i = 0; i < 4; i++) M.cap[i] = -1;
        if (m(&M, 0, s)) {
            for (i = 0; i < 4; i++) cap[i] = M.cap[i];
            return 1;
        }
        if (r->nel && r->el[0].op == OP_START) break;
    }
    return 0;
}

static int vocative(const TalkTok *t)
{
    return (t->kind == TTK_PERSON && !(t->flags & (TTF_ABSENT | TTF_POSS))) || has_class(t, TCL_EVERYONE);
}

/* The line without the names it is said to: a leading greeting and names
 * ("hey Kevin, ..."), and trailing names ("..., Nana?"; the ? or ! stays).
 * *first = the first token kept; returns how many are kept. */
static int stripped_view(const TalkTok *tok, int n, int *first)
{
    int a = 0, b = n, tail = n;
    if (n > 1 && has_class(&tok[0], TCL_GREETW) && vocative(&tok[1])) a = 1;
    while (a < n && (vocative(&tok[a]) || (a > 0 && tok[a].kind == TTK_EXCL))) a++;
    while (tail > a && (tok[tail - 1].kind == TTK_QMARK || tok[tail - 1].kind == TTK_EXCL)) tail--;
    b = tail;
    while (b > a && vocative(&tok[b - 1])) b--;
    *first = a;
    if (a == 0 && b == tail) return n;          /* nothing to strip */
    if (b <= a) return n;
    return b - a + (n - tail);                  /* the kept middle + the final ? / ! */
}

/* a rule on the whole line, else on the line without the names said to */
static int rule_match(const Rule *r, const TalkTok *tok, int n, const TalkTok *sv, int svn, int svoff, int cap[4])
{
    int i;
    if (rule_match1(r, tok, n, cap)) return 1;
    if (!sv || svn == n) return 0;
    if (!rule_match1(r, sv, svn, cap)) return 0;
    for (i = 0; i < 4; i++) if (cap[i] >= 0) cap[i] = cap[i] < svn ? cap[i] + svoff : cap[i];
    return 1;
}

/* rule k at byte *at of the rules blob: 1 ok, *at moves on */
static int read_rule(uint32_t base, uint32_t len, uint32_t *at, Rule *r)
{
    uint8_t b[RULE_HDR + EL_SIZE * EL_MAX + 4 * LIT_MAX];
    uint32_t body;
    int i, k, lit = 0;
    if (*at + RULE_HDR > len || !talk_db_read(TB_PARSE, base + *at, b, RULE_HDR)) return 0;
    r->prio = (uint16_t)(b[0] | (b[1] << 8));
    r->act[0] = b[2]; r->act[1] = b[3]; r->cap[0] = b[4]; r->cap[1] = b[5]; r->nel = b[6]; r->nlit = b[7];
    body = EL_SIZE * r->nel + 4u * r->nlit;
    if (r->nel > EL_MAX || r->nlit > LIT_MAX || *at + RULE_HDR + body > len) return 0;
    if (!talk_db_read(TB_PARSE, base + *at + RULE_HDR, b, body)) return 0;
    for (i = 0; i < r->nel; i++) {
        const uint8_t *p = b + EL_SIZE * i;
        for (k = 0; k < 4; k++) r->el[i].cls[k] = le32(p + 4 * k);
        r->el[i].preds = le32(p + 16);
        r->el[i].op = p[20]; r->el[i].flags = p[21]; r->el[i].cap = p[22]; r->el[i].nlit = p[23];
        r->el[i].lit0 = (uint8_t)lit;
        lit += p[23];
    }
    if (lit != r->nlit) return 0;
    for (i = 0; i < r->nlit; i++) r->lit[i] = le32(b + EL_SIZE * r->nel + 4 * i);
    *at += RULE_HDR + body;
    return 1;
}

static int tone_of(int act)
{
    if (TA_IS_ROMANCE(act)) return TONE_FLIRTY;
    switch (act) {
    case TA_INSULT: case TA_CRITICIZE: case TA_MOCK: case TA_COMPLAIN: case TA_BLAME: case TA_DISMISS:
    case TA_THREATEN: case TA_ACCUSE: return TONE_SNARKY;
    case TA_TEASE: case TA_JOKE: case TA_GOSSIP: return TONE_CHEEKY;
    case TA_COMPLIMENT: case TA_PRAISE_OTHER: case TA_COMFORT: case TA_ENCOURAGE: case TA_SYMPATHIZE:
    case TA_THANK: case TA_APOLOGIZE: case TA_GREET: case TA_OFFER: case TA_INVITE: case TA_PROMISE:
    case TA_HUG: case TA_LAUGH: return TONE_WARM;
    }
    return TONE_PLAIN;
}

/* what a capture says about the act: topic / person */
static void use_capture(const TalkTok *tok, int ti, uint16_t *topic, TalkParseInfo *info)
{
    const TalkTok *t;
    if (ti < 0) return;
    t = &tok[ti];
    if (t->kind == TTK_PERSON && !(t->flags & TTF_ABSENT)) {
        if (info->other < 0) info->other = (int8_t)t->person;
        return;
    }
    if (t->kind == TTK_PERSON && (t->flags & TTF_ABSENT)) {
        if (info->subject < 0) info->subject = t->person;
        if (talkable(t) && *topic == TC_NONE) *topic = t->concept;
        return;
    }
    if (talkable(t) && *topic == TC_NONE) *topic = t->concept;
}

int talk_parse(const char *text, const TalkParseCtx *ctx, TalkUtter *out, TalkParseInfo *info_in)
{
    TalkParseInfo local, *info = info_in ? info_in : &local;
    uint8_t hb[64];
    uint32_t nrules, rules_off, rules_len, at = 0;
    int n, k, i, found = 0, fallback = 0;
    uint16_t cap_topic = TC_NONE, act_topic[2];
    TalkTok sv[TALK_TOK_MAX];
    int svn = 0, svoff = 0;
    Rule r;
    if (!out) return 0;
    memset(out, 0, sizeof *out);
    memset(info, 0, sizeof *info);
    info->subject = -1; info->other = -1;
    info->rule[0] = info->rule[1] = 0xFFFF;
    out->act[0] = TA_UNKNOWN;
    out->target = (int8_t)(ctx && ctx->nparts ? (ctx->ring >= 0 && ctx->ring < ctx->nparts ? ctx->ring : 0) : -1);
    if (text) {
        int l = (int)strlen(text);
        if (l > TALK_SHOWN_MAX - 1) l = TALK_SHOWN_MAX - 1;
        memcpy(out->shown, text, (size_t)l);
        out->shown[l] = 0;
    }
    if (!text || !talk_db_read(TB_PARSE, 0, hb, 64) || le32(hb) != PRS_MAGIC) return 0;
    nrules = le32(hb + 20); rules_off = le32(hb + 24); rules_len = le32(hb + 28);
    n = talk_lex(text, ctx, info->tok, TALK_TOK_MAX);
    info->ntok = (uint8_t)n;
    for (i = 0; i < n; i++) {
        if (info->tok[i].flags & TTF_FUZZY) info->fuzzy++;
        if (info->tok[i].flags & TTF_UNK) info->unknown++;
    }
    /* S1 first: the safety classes always win */
    out->safety = talk_safe_scan(info->tok, n);
    if (ctx && ctx->kid_room) {
        for (i = 0; i < n; i++)
            if (info->tok[i].wflags & (TW_CRUDE | TW_SUGGEST | TW_STRONG | TW_SEXUAL | TW_HATE))
                info->tok[i].flags |= TTF_MASK;
    }
    /* the bubble: masked words become #### */
    for (i = 0; i < n; i++) {
        const TalkTok *t = &info->tok[i];
        int p;
        if (!(t->flags & TTF_MASK)) continue;
        for (p = t->src0; p < t->src1 && p < TALK_SHOWN_MAX - 1 && out->shown[p]; p++)
            if (out->shown[p] != ' ' && out->shown[p] != '\t') out->shown[p] = '#';
        out->safety |= TS_MASKED;
    }
    if (out->safety & (TS_HATE | TS_DISTRESS) || talk_safe_explicit(info->tok, n)) {
        out->act[0] = (uint8_t)(talk_safe_explicit(info->tok, n) ? TA_REFUSE_SEXUAL :
                                (out->safety & TS_HATE) ? TA_REFUSE_HATE : TA_DISTRESS);
        out->act[1] = TA_NONE;
        out->tone = TONE_PLAIN;
        out->conf = 15;
        return 1;
    }
    /* the view without the names it is said to (the tokens are copied: the
     * final ? / ! follows the kept middle) */
    {
        int first = 0, keep = stripped_view(info->tok, n, &first), tail = n;
        while (tail > 0 && (info->tok[tail - 1].kind == TTK_QMARK || info->tok[tail - 1].kind == TTK_EXCL)) tail--;
        svn = keep;
        svoff = first;
        if (keep != n) {
            int mid = keep - (n - tail);
            memcpy(sv, info->tok + first, sizeof(TalkTok) * (size_t)mid);
            memcpy(sv + mid, info->tok + tail, sizeof(TalkTok) * (size_t)(n - tail));
        }
    }
    /* the rules, highest priority first */
    for (k = 0; k < (int)nrules; k++) {
        int cap[4];
        if (!read_rule(rules_off, rules_len, &at, &r)) break;
        if (!rule_match(&r, info->tok, n, svn != n ? sv : NULL, svn, svoff, cap)) continue;
        if (!found) {
            out->act[0] = r.act[0];
            info->rule[0] = (uint16_t)k;
            if (r.cap[0]) use_capture(info->tok, cap[r.cap[0]], &cap_topic, info);
            if (r.act[1]) {
                out->act[1] = r.act[1];
                info->rule[1] = (uint16_t)k;
                if (r.cap[1]) use_capture(info->tok, cap[r.cap[1]], &cap_topic, info);
            }
            found = 1;
            fallback = r.prio < 150;
            if (out->act[1] || fallback) break;
            continue;
        }
        if (r.prio < 200) break;
        if (r.act[0] != out->act[0]) {
            out->act[1] = r.act[0];
            info->rule[1] = (uint16_t)k;
            if (r.cap[0]) use_capture(info->tok, cap[r.cap[0]], &cap_topic, info);
            break;
        }
    }
    /* a flirting word makes the line a flirt (the gate decides what it may be) */
    if ((out->safety & TS_SEXUAL) && !TA_IS_ROMANCE(out->act[0])) {
        if (out->act[0] != TA_UNKNOWN) out->act[1] = out->act[0];
        out->act[0] = TA_FLIRT;
    }
    /* the topic: the capture first, then spreading activation */
    talk_lex_topics(info->tok, n, ctx, act_topic);
    if (cap_topic != TC_NONE) {
        out->topic[0] = cap_topic;
        out->topic[1] = act_topic[0] != cap_topic ? act_topic[0] : act_topic[1];
    } else {
        out->topic[0] = act_topic[0];
        out->topic[1] = act_topic[1];
    }
    /* the target: everyone, a participant named, or the ring */
    if (ctx && ctx->nparts) {
        int tgt = -2;
        for (i = 0; i < n; i++) if (has_class(&info->tok[i], TCL_EVERYONE)) tgt = -1;
        if (tgt == -2) {
            for (i = 0; i < n; i++) {
                const TalkTok *t = &info->tok[i];
                if (t->kind == TTK_PERSON && !(t->flags & TTF_ABSENT) && t->person != info->other) {
                    tgt = t->person;
                    break;
                }
            }
        }
        if (tgt == -2 && info->other >= 0) {
            /* "Vex is right" to the ring; "you're right, Vex" names Vex */
            int ring = ctx->ring >= 0 && ctx->ring < ctx->nparts ? ctx->ring : 0;
            if (out->act[0] == TA_TAKE_SIDE || out->act[0] == TA_DEFEND || out->act[0] == TA_ACCUSE ||
                out->act[0] == TA_PRAISE_OTHER || out->act[0] == TA_MEDIATE || out->act[0] == TA_INTRODUCE)
                tgt = ring == info->other && ctx->nparts > 1 ? (ring + 1) % ctx->nparts : ring;
            else
                tgt = info->other;
        }
        if (tgt != -2) out->target = (int8_t)tgt;
    }
    out->tone = (uint8_t)tone_of(out->act[0]);
    k = 15 - 3 * info->fuzzy - info->unknown;
    if (fallback) k = 6;
    if (out->act[0] == TA_UNKNOWN) k = 0;
    out->conf = (uint8_t)(k < 0 ? 0 : k > 15 ? 15 : k);
    return 1;
}
