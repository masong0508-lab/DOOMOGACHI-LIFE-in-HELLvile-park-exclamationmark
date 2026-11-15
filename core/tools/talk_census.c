/* talk_census.c - Talk T7: the census (owner: TK; docs/TALK.md "The census";
 * design 3.7). A host program that links the core: it generates exchanges
 * over a fixed grid of contexts (core/data/talk/census_en.tsv) with the real
 * generator, the real gate and the real S3 lint, counts the distinct
 * replies on three tiers (Surface, Skeleton, Meaning) among the exchanges
 * that pass the quality checks Q1-Q6, estimates the total reachable (Chao1),
 * prints the failure histogram, and runs the conversation tier (bot
 * conversations of 6 player turns; two are distinct when their Surface
 * hashes differ in at least 2 of the 6).
 *
 *   make -C core talk-census          the full run (design: 2,000,000 contexts
 *                                     and 2,000,000 conversations); exit 1 if
 *                                     a target is missed or quality fails > 1%
 *   build/talk_census --smoke         the unit tests' size (no targets)
 *   build/talk_census --pick          print 32 residents and 400 topics for a
 *                                     new grid (the committed grid was made so)
 *   build/talk_census --sample F --every N   write every Nth exchange (context
 *                                     and lines) for core/tools/talk_sample.py
 *
 * Compiled with TALK_CENSUS_NO_MAIN it is a library for
 * core/tests/test_talk_census.c (the smoke census of `make -C core test`).
 * Host only: stdio, malloc and doubles are fine here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <time.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/talk_traits.h"
#include "doomo/lang.h"
#include "doomo/roster.h"
#include "doomo/sim.h"
#include "doomo/story.h"
#include "doomo/family.h"
#include "doomo/ending.h"
#include "doomo/portrait.h"
#include "doomo/resident_acc.h"
#include "doomo/talk_traits.h"
#include "talk_census.h"

const char *const talk_census_qf_names[QF_COUNT] = {
    "ok", "Q1 fallback (nothing fitted)", "Q1 line code", "Q1 spacing", "Q1 capital", "Q1 end punctuation",
    "Q1 article", "Q1 bubble", "Q1 sentences", "Q2 reply act", "Q2 topic", "Q3 voice guard",
    "Q3 exclamations", "Q3 tic", "Q4 callback fact", "Q5 repeat", "Q6 lint", "Q6 kid room", "Q6 mature",
    "K-10 grief punchline",
    "Q8a reaction (report)", "Q8b content (report)", "Q8c colour (report)", "Q8d hook (report)",
    "Q8e filler (report)", "Q8f unknown word (report)"
};

/* talk.h's TA_* names, in order (the grid names acts) */
static const char *const act_names[TA_COUNT] = {
    "NONE", "GREET", "FAREWELL", "THANK", "APOLOGIZE", "INTRODUCE", "ASK_ABOUT", "ASK_OPINION", "ASK_FEELING",
    "ASK_FACT", "ASK_WHO", "ASK_ADVICE", "ASK_FAVOR", "ASK_REPEAT", "TELL", "OPINE_POS", "OPINE_NEG", "AGREE",
    "DISAGREE", "ANSWER", "YES", "NO", "COMPLIMENT", "PRAISE_OTHER", "COMFORT", "ENCOURAGE", "SYMPATHIZE", "JOKE",
    "LAUGH", "OFFER", "INVITE", "PROMISE", "INSULT", "CRITICIZE", "TEASE", "MOCK", "COMPLAIN", "BLAME", "DISMISS",
    "THREATEN", "GOSSIP", "DEFEND", "ACCUSE", "TAKE_SIDE", "MEDIATE", "CHANGE_TOPIC", "CONTINUE", "BACKCHANNEL",
    "UNKNOWN", "FLIRT", "CONFESS", "ASK_OUT", "KISS", "WAVE", "NOD", "SHAKE_HEAD", "HUG", "GIVE", "WALK_AWAY",
    "HIGH_FIVE", "REFUSE_SEXUAL", "REFUSE_MINOR", "REFUSE_HATE", "DISTRESS", "DEFLECT_ROMANCE"
};

#define MAX_RES    64
#define MAX_TOPICS 1024
#define MAX_ACTS   TA_COUNT

typedef struct Grid {
    uint32_t town, seed, contexts, conversations, smoke_contexts, smoke_conversations;
    int res[MAX_RES], nres;
    uint16_t topic[MAX_TOPICS];
    int ntopics;
    uint8_t act[MAX_ACTS];
    int nacts;
} Grid;

/* ---- hash sets (open addressing, u64 keys, u8 counts) -------------------- */
typedef struct HSet { uint64_t *key; uint8_t *cnt; uint64_t cap, n; } HSet;

static int hs_init(HSet *h, uint64_t want)
{
    uint64_t cap = 1024;
    while (cap < want * 2) cap <<= 1;
    h->key = (uint64_t *)calloc(cap, sizeof(uint64_t));
    h->cnt = (uint8_t *)calloc(cap, 1);
    h->cap = cap;
    h->n = 0;
    return h->key && h->cnt;
}

static void hs_free(HSet *h) { free(h->key); free(h->cnt); memset(h, 0, sizeof *h); }

/* insert: returns the count before (0 new) */
static int hs_add(HSet *h, uint64_t k)
{
    uint64_t i;
    if (!k) k = 1;
    if (h->n * 10 >= h->cap * 7) {                       /* grow at 70% */
        HSet g;
        uint64_t j;
        if (!hs_init(&g, h->cap)) { fprintf(stderr, "talk_census: out of memory\n"); exit(2); }
        for (j = 0; j < h->cap; j++)
            if (h->key[j]) {
                uint64_t p = h->key[j] & (g.cap - 1);
                while (g.key[p]) p = (p + 1) & (g.cap - 1);
                g.key[p] = h->key[j];
                g.cnt[p] = h->cnt[j];
                g.n++;
            }
        hs_free(h);
        *h = g;
    }
    i = k & (h->cap - 1);
    while (h->key[i] && h->key[i] != k) i = (i + 1) & (h->cap - 1);
    if (!h->key[i]) { h->key[i] = k; h->cnt[i] = 1; h->n++; return 0; }
    if (h->cnt[i] < 255) h->cnt[i]++;
    return h->cnt[i] - 1;
}

static int hs_has(const HSet *h, uint64_t k)
{
    uint64_t i;
    if (!k) k = 1;
    i = k & (h->cap - 1);
    while (h->key[i]) {
        if (h->key[i] == k) return 1;
        i = (i + 1) & (h->cap - 1);
    }
    return 0;
}

static void hs_tier(const HSet *h, CensusTier *t)
{
    uint64_t j;
    t->distinct = h->n;
    t->f1 = t->f2 = 0;
    for (j = 0; j < h->cap; j++) {
        if (!h->key[j]) continue;
        if (h->cnt[j] == 1) t->f1++;
        else if (h->cnt[j] == 2) t->f2++;
    }
    /* Chao1: S_obs + f1^2 / (2 f2) (the bias-corrected form when f2 = 0) */
    t->chao1 = (double)t->distinct + (t->f2 ? (double)t->f1 * (double)t->f1 / (2.0 * (double)t->f2)
                                            : (double)t->f1 * ((double)t->f1 - 1.0) / 2.0);
}

static uint64_t fnv64(uint64_t h, const char *s, int n)
{
    int i;
    for (i = 0; i < n; i++) { h ^= (uint8_t)s[i]; h *= 0x100000001B3ull; }
    return h;
}
#define FNV0 0xCBF29CE484222325ull

/* ---- the grid -------------------------------------------------------------- */
static int act_by_name(const char *n)
{
    int a;
    for (a = 1; a < TA_COUNT; a++) if (strcmp(act_names[a], n) == 0) return a;
    return -1;
}

static uint16_t concept_by_key(const char *key)
{
    uint16_t c = 0, wf = 0;
    uint32_t tags[4];
    char k[64];
    int i;
    char canon[64];
    for (i = 0; key[i] && i < 63; i++) k[i] = key[i] == '_' ? ' ' : key[i];
    k[i] = 0;
    talk_lex_canon(k, canon, (int)sizeof canon);
    if (talk_lex_find(canon, &c, &wf, tags) < 0) return 0;
    return c;
}

static int load_grid(const char *path, Grid *g, char *err, int errcap)
{
    if (!act_names[TA_COUNT - 1]) { snprintf(err, (size_t)errcap, "act_names is behind talk.h"); return -1; }
    {
    FILE *f = fopen(path, "r");
    char line[256];
    int n = 0;
    memset(g, 0, sizeof *g);
    if (!f) { snprintf(err, (size_t)errcap, "%s: missing", path); return -1; }
    while (fgets(line, sizeof line, f)) {
        char a[64], b[96];
        n++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (sscanf(line, "%63s %95s", a, b) != 2) continue;
        if (strcmp(a, "@town") == 0) g->town = (uint32_t)strtoul(b, NULL, 10);
        else if (strcmp(a, "@seed") == 0) g->seed = (uint32_t)strtoul(b, NULL, 10);
        else if (strcmp(a, "@contexts") == 0) g->contexts = (uint32_t)strtoul(b, NULL, 10);
        else if (strcmp(a, "@conversations") == 0) g->conversations = (uint32_t)strtoul(b, NULL, 10);
        else if (strcmp(a, "@smoke_contexts") == 0) g->smoke_contexts = (uint32_t)strtoul(b, NULL, 10);
        else if (strcmp(a, "@smoke_conversations") == 0) g->smoke_conversations = (uint32_t)strtoul(b, NULL, 10);
        else if (strcmp(a, "resident") == 0 && g->nres < MAX_RES) g->res[g->nres++] = atoi(b);
        else if (strcmp(a, "act") == 0 && g->nacts < MAX_ACTS) {
            int act = act_by_name(b);
            if (act < 0) { snprintf(err, (size_t)errcap, "%s:%d: act %s", path, n, b); fclose(f); return -1; }
            g->act[g->nacts++] = (uint8_t)act;
        } else if (strcmp(a, "topic") == 0 && g->ntopics < MAX_TOPICS) {
            uint16_t c = concept_by_key(b);
            if (!c) { snprintf(err, (size_t)errcap, "%s:%d: topic %s is no concept", path, n, b); fclose(f); return -1; }
            g->topic[g->ntopics++] = c;
        }
    }
    fclose(f);
    if (!g->nres || !g->ntopics || !g->nacts) {
        snprintf(err, (size_t)errcap, "%s: needs resident, topic and act rows", path);
        return -1;
    }
    return 0;
    }
}

/* ---- text -------------------------------------------------------------------- */
static void visible(const char *s, char *v, int cap)
{
    int o = 0;
    for (; *s && o < cap - 1; s++) {
        if (*s == '{') { while (*s && *s != '}') s++; if (!*s) break; continue; }
        v[o++] = *s;
    }
    v[o] = 0;
}

static int abbrev_at(const char *v, int i)
{
    static const char *const ab[] = { "Mr", "Mrs", "Ms", "Dr", "St", "Mme", "Jr", "Sr", "Mt", "Prof", "Rev" };
    int j = i;
    unsigned k;
    while (j > 0 && isalpha((unsigned char)v[j - 1])) j--;
    for (k = 0; k < sizeof ab / sizeof ab[0]; k++)
        if ((int)strlen(ab[k]) == i - j && strncmp(v + j, ab[k], (size_t)(i - j)) == 0) return 1;
    return 0;
}

static int sentence_count(const char *v)
{
    int i, n = 0;
    /* an end mark followed by a space or the end ("...Right." is one; "?!" is one; "Dr." none) */
    for (i = 0; v[i]; i++)
        if ((v[i] == '.' || v[i] == '!' || v[i] == '?') && (v[i + 1] == 0 || v[i + 1] == ' ') &&
            !(v[i] == '.' && abbrev_at(v, i)))
            n++;
    return n;
}

static int a_word_ok(const char *w, int an)
{
    static const char *const a_vowel_ok[] = { "uni", "use", "usu", "uti", "ure", "eu", "one", "once", "ewe", "ufo",
                                              "u-", "uk", "ura" };
    static const char *const an_cons_ok[] = { "hour", "honest", "honor", "honour", "heir", "herb" };
    char c = (char)tolower((unsigned char)w[0]);
    unsigned k;
    int vowel = c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
    if (!isalpha((unsigned char)c)) return 1;
    if (!an) {
        if (!vowel) return 1;
        for (k = 0; k < sizeof a_vowel_ok / sizeof *a_vowel_ok; k++)
            if (strncasecmp(w, a_vowel_ok[k], strlen(a_vowel_ok[k])) == 0) return 1;
        return 0;
    }
    if (vowel) {
        if (strncasecmp(w, "uni", 3) == 0 || strncasecmp(w, "use", 3) == 0 || strncasecmp(w, "one", 3) == 0) return 0;
        return 1;
    }
    for (k = 0; k < sizeof an_cons_ok / sizeof *an_cons_ok; k++)
        if (strncasecmp(w, an_cons_ok[k], strlen(an_cons_ok[k])) == 0) return 1;
    return 0;
}

/* Q1 on one line's text: QF_NONE or the problem */
static int q1_text(const char *text, int lower)
{
    char v[TALK_LINE_MAX * 2];
    int i, n;
    for (i = 0; text[i]; i++) {
        if (text[i] == '}') return QF_Q1_CODE;
        if (text[i] == '{') {
            int j = i + 1;
            static const char *const codes[] = { ".", "..", "...", "d", "!", "big", "/", "w", "r", "punch" };
            unsigned k;
            int ok = 0;
            while (text[j] && text[j] != '}') j++;
            if (!text[j]) return QF_Q1_CODE;
            for (k = 0; k < sizeof codes / sizeof *codes; k++)
                if ((int)strlen(codes[k]) == j - i - 1 && strncmp(text + i + 1, codes[k], (size_t)(j - i - 1)) == 0) ok = 1;
            if (!ok && !(strncmp(text + i + 1, "p ", 2) == 0 || strncmp(text + i + 1, "cut ", 4) == 0)) return QF_Q1_CODE;
            i = j;
        }
    }
    visible(text, v, (int)sizeof v);
    n = (int)strlen(v);
    if (!n) return QF_Q1_CODE;
    for (i = 0; v[i]; i++) {
        /* two spaces, a space before , . ? ! (an ellipsis "...Right." may follow a space) */
        if (v[i] == ' ' && (v[i + 1] == ' ' || (v[i + 1] == '.' && strncmp(v + i + 1, "...", 3) != 0) ||
                            v[i + 1] == ',' || v[i + 1] == '?' || v[i + 1] == '!' || v[i + 1] == 0))
            return QF_Q1_SPACE;
    }
    if (v[0] == ' ') return QF_Q1_SPACE;
    for (i = 0; v[i] && !isalnum((unsigned char)v[i]); i++) { }
    if (v[i] && islower((unsigned char)v[i]) && !lower) return QF_Q1_CASE;
    {
        char e = v[n - 1];
        /* end punctuation, an action (*waves*), or a sign-off ("- V", Vex) */
        if (!(e == '.' || e == '!' || e == '?' || e == '*' || e == '"' || e == ')' || e == '-' || e == '~') &&
            !(n >= 3 && v[n - 2] == ' ' && v[n - 3] == '-'))
            return QF_Q1_END;
    }
    for (i = 0; v[i]; i++) {
        int start = i == 0 || v[i - 1] == ' ';
        if (!start) continue;
        if ((v[i] == 'a' || v[i] == 'A') && v[i + 1] == ' ' && !a_word_ok(v + i + 2, 0)) return QF_Q1_ARTICLE;
        if ((v[i] == 'a' || v[i] == 'A') && v[i + 1] == 'n' && v[i + 2] == ' ' && !a_word_ok(v + i + 3, 1))
            return QF_Q1_ARTICLE;
    }
    if (talk_safe_bubble_lines(v, TALK_BUBBLE_COLS) > TALK_BUBBLE_LINES) return QF_Q1_BUBBLE;
    if (sentence_count(v) > 3) return QF_Q1_SENTENCES;
    return QF_NONE;
}

/* the speaker's data, read independently of the generator (Q3); the sliders are the EFFECTIVE ones (TM2c) */
static int voice_term_ok(int id, int pred, int val, const uint8_t eff[5])
{
    DoomoResident buf;
    const DoomoResident *r = NULL;
    if (id >= 0 && id <= DOOMO_PLAYER_ID) r = town_res(id);
    else if (id > DOOMO_PLAYER_ID && id != TALK_PET_ID && family_resident(id, &buf)) r = &buf;
    if (!r) return 1;
    switch (pred) {
    case TGP_POLITE: return eff[PERS_SPEECH] <= 5;
    case TGP_BLUNT: return eff[PERS_SPEECH] >= 10;
    case TGP_FLAT: return eff[PERS_EXPRESSION] <= 5;
    case TGP_LOUD: return eff[PERS_EXPRESSION] >= 10;
    case TGP_SERIOUS: return eff[PERS_ATTITUDE] <= 4;
    case TGP_EASYGOING: return eff[PERS_ATTITUDE] >= 11;
    case TGP_QUIRKY: return eff[PERS_QUIRK] >= 11;
    case TGP_QUICK: return eff[PERS_MOVEMENT] >= 11;
    case TGP_SLOW: return eff[PERS_MOVEMENT] <= 4;
    case TGP_TYPE: return persona_type(r) == val;
    case TGP_SPEAKER: return talk_gen_cast_id(val) == id && !(r->flags & RF_CUSTOM);
    case TGP_KID_SPEAKER: return talk_safe_minor(id);
    case TGP_TEEN: return id <= DOOMO_PLAYER_ID && sim_life_stage(id) == LIFE_TEEN;
    case TGP_ELDER: return id <= DOOMO_PLAYER_ID && sim_life_stage(id) == LIFE_ELDER;
    default: return -1;                                  /* not a voice term */
    }
}

static int days_bday(const DoomoResident *r)
{
    int m, d, bm = RES_BDAY_MONTH(r) % 12, bd = RES_BDAY_DAY(r), x;
    sim_date(&m, &d);
    x = ((bm - m + 12) % 12) * 28 + (bd - d);
    return x < 0 ? x + 336 : x;
}

/* Q4: the callback's fact, re-read from the sim's accessors */
static int q4_fact(const TalkConv *c, int who, const TalkTrace *t)
{
    int id = c->id[who];
    const DoomoResident *r = id <= DOOMO_PLAYER_ID ? town_res(id) : NULL;
    switch (t->cb) {
    case TCB_NONE: return 1;
    case TCB_JOB: {
        char f[48], j[48];
        int i;
        if (!r || id >= ROSTER_COUNT || talk_safe_minor(id)) return 0;
        if (talk_db_form(t->cb_val, TF_LEMMA, f, (int)sizeof f) <= 0) return 0;
        snprintf(j, sizeof j, "%s", roster_job(id));
        for (i = 0; j[i]; i++) j[i] = (char)tolower((unsigned char)j[i]);
        for (i = 0; f[i]; i++) f[i] = (char)tolower((unsigned char)f[i]);
        return strcmp(f, j) == 0;
    }
    case TCB_PARTNER: return sim_partner_of(id) == (int)t->cb_val && !talk_safe_minor(id);
    case TCB_PROBLEM: {
        int k = sim_problem_for(id);
        return k >= 0 && sim_problem(k) && sim_problem(k)->kind == t->cb_val;
    }
    case TCB_BDAY: return r && days_bday(r) == (int)t->cb_val && t->cb_val <= 7;
    case TCB_NIGHT: return town_is_night();
    case TCB_GHOST: return r && r->status == RES_GHOST;
    case TCB_POSSESSED: return r && r->status == RES_POSSESSED;
    case TCB_HELLBORN:
        return r && ((r->flags & RF_HELLBORN) || (id < DOOMO_MAX_RESIDENTS && RES_DISTRICT(id) == DISTRICT_HELLVILE));
    case TCB_MEMORY: return c->mem_flags[who] != 0 || c->mem_topic[who] == t->cb_val;
    case TCB_LIKE: return t->cb_val && (talk_gen_like(id, 0) == t->cb_val || talk_gen_like(id, 1) == t->cb_val ||
                                        talk_gen_like(id, 2) == t->cb_val);
    case TCB_FEAR: return t->cb_val && talk_gen_like(id, 3) == t->cb_val;
    case TCB_CHAPTER: return story_chapter() == (int)t->cb_val;
    case TCB_FAMILY: return family_role(id) == (int)t->cb_val && t->cb_val != FAMROLE_NONE;
    case TCB_PET: return r && (r->flags & RF_HAS_PET);
    case TCB_NEWS: { uint16_t v; return talk_gen_fact(id, TCB_NEWS, &v) && v == t->cb_val; }
    default: return 0;
    }
}

/* K-10: a grief or loss concept (the lexicon's grief flag) */
static int is_grief(uint16_t concept)
{
    TalkConcept tc;
    return concept && talk_db_concept(concept, &tc) && (tc.flags & TCF_GRIEF);
}

/* K-10 on one resident line: a grief topic gets a sincere core line and nothing else (no
 * joke act), and a grief word is said only in a sincere line */
static int k10_ok(const TalkUtter *u, const TalkLine *l, const TalkTrace *t)
{
    int i;
    if (is_grief(u->topic[0]) && !TA_IS_SAFETY(u->act[0])) {
        if (!(l->flags & TLF_SINCERE) || (t->parts & ~TPART_CORE)) return 0;
        if (l->act == TA_JOKE || l->act == TA_LAUGH || l->act == TA_TEASE || l->act == TA_MOCK) return 0;
    }
    for (i = 0; i < t->nsaid; i++) if (is_grief(t->said[i]) && !(l->flags & TLF_SINCERE)) return 0;
    if (is_grief(l->topic) && !(l->flags & TLF_SINCERE)) return 0;
    return 1;
}

static int topic_ok(const TalkConv *c, const TalkUtter *u, const TalkLine *l)
{
    uint16_t ids[TALK_ASSOC_TOP];
    int n, i, k;
    if (l->flags & TLF_MORE) return 1;                      /* TM2d: a story or a secret is about the speaker, a like of theirs or the thread */
    if (!l->topic || l->topic == u->topic[0] || l->topic == u->topic[1]) return 1;
    for (k = 0; k < 2; k++) {
        if (!u->topic[k]) continue;
        n = talk_db_assoc(u->topic[k], ids, NULL);
        for (i = 0; i < n; i++) if (ids[i] == l->topic) return 1;
    }
    if (l->act == TA_CHANGE_TOPIC && l->who >= 0)
        for (k = 0; k < 3; k++) if (talk_gen_like(c->id[l->who], k) == l->topic) return 1;
    return 0;
}

/* Q8c: does the speaker have trait `tr` in this room (the edgy traits dropped for a minor or in a kid room) */
static int has_trait(int id, int kid, int tr)
{
    TalkTraitSet ts;
    DoomoResident buf;
    const DoomoResident *r = id <= DOOMO_PLAYER_ID ? town_res(id) : (family_resident(id, &buf) ? &buf : NULL);
    if (!r || !talk_trait_get(id, &ts)) return 0;
    talk_trait_room(&ts, RES_STAGE(r), kid || talk_safe_minor(id));
    return talk_traitset_has(&ts, tr);
}

/* Q8a (TM2c): the line is what the plan chose. The reply act serves the kind (unless the soft filter recorded a
 * miss), the kind is allowed after the player's act, a kind that goes against the lean has a reason, and the hard
 * rules of layer a hold: a romance act is accepted, deflected or refused; a hug in a kid room is accepted or
 * refused; a kid room gets no tease, mock, insult or threaten from the plan. */
static int q8a_ok(const TalkConv *c, const TalkUtter *u, const TalkLine *l, const TalkTrace *t)
{
    int k = t->plan.kind, kid = c->aud.any_minor || c->aud.avatar_minor || c->aud.target_minor;
    int sgn = (k == TK_ACCEPT || k == TK_AGREE) ? 1 : (k == TK_DENY || k == TK_REFUSE) ? -1 : 0;
    if (!(t->plan.allowed & TKM(k)) || (t->plan.allowed & ~talk_gen_kinds_after(u->act[0]))) return 0;
    if (!(t->plan.flags & (TPL_MISS | TPL_OPEN)) && !(talk_gen_kinds_reply(u->act[0], l->act) & (int)TKM(k))) return 0;
    if (sgn * t->plan.lean < 0 && !(t->plan.flags & TPL_REASON)) return 0;
    if (TA_IS_ROMANCE(u->act[0]) && k != TK_ACCEPT && k != TK_DEFLECT && k != TK_REFUSE) return 0;
    if (u->act[0] == TA_HUG && (kid || talk_safe_minor(c->id[l->who])) && k != TK_ACCEPT && k != TK_REFUSE) return 0;
    if (kid && TALK_COOL_REPLY(l->act)) return 0;
    return 1;
}

int talk_census_check(const TalkConv *c, const TalkUtter *u, const TalkExchange *x, const TalkTrace *tr)
{
    int k, i, kid = c->aud.any_minor || c->aud.avatar_minor || c->aud.target_minor;
    for (k = 0; k < x->n; k++) {
        const TalkLine *l = &x->line[k];
        const TalkTrace *t = &tr[k];
        int id = l->who >= 0 && l->who < c->n ? c->id[l->who] : -1, lower = 0, q, prev_act, nex = 0, budget;
        TalkAltInfo ai;
        uint8_t eff[5];
        if (l->flags & TLF_SAFE) return QF_Q6_LINT;
        if (l->flags & TLF_FALLBACK) return QF_Q1_FALLBACK;
        if (talk_gen_alt_info(t->core_alt, &ai) && (ai.rflags & 64u)) lower = 1;
        q = q1_text(l->text, lower);
        if (q) return q;
        if (id == TALK_PET_ID) continue;
        if (!k10_ok(u, l, t)) return QF_K10_GRIEF;
        /* Q2: the act, then the topic */
        prev_act = k == 0 || (l->flags & TLF_MORE) ? u->act[0] : x->line[k - 1].act;     /* TM2d: a story or a secret goes on from the answer's own act */
        if (!(l->flags & TLF_END) && !talk_gen_adjacent(prev_act, l->act)) return QF_Q2_ACT;
        if (!topic_ok(c, u, l)) return QF_Q2_TOPIC;
        /* Q8c / Q3: the effective sliders, re-derived from the room, and the plan's own record of them */
        if (!talk_gen_eff_persona(id, kid, eff)) memset(eff, 8, sizeof eff);
        if (memcmp(eff, t->plan.eff, sizeof eff) != 0) return QF_Q8_VOICE;
        /* Q8a: the plan's kind (answer lines only) */
        if (t->plan.kind && !q8a_ok(c, u, l, t)) return QF_Q8_KIND;
        /* Q3: every voice term of every alternative used holds for the speaker */
        for (i = 0; i < t->nalt; i++) {
            int j;
            if (!talk_gen_alt_info(t->alt[i], &ai)) return QF_Q3_VOICE;
            for (j = 0; j < ai.nterm; j++) {
                int p = ai.term[2 * j] & 0x7F, neg = ai.term[2 * j] & 0x80, ok = voice_term_ok(id, p, ai.term[2 * j + 1], eff);
                if (ok < 0) continue;
                if (neg ? ok : !ok) return p >= TGP_POLITE && p <= TGP_SLOW ? QF_Q8_VOICE : QF_Q3_VOICE;
                if (p == TGP_TRAIT && (neg ? has_trait(id, kid, ai.term[2 * j + 1]) : !has_trait(id, kid, ai.term[2 * j + 1])))
                    return QF_Q8_VOICE;
            }
            /* Q6: no romance rule, no Mature variant where they may not be */
            if (kid && (ai.rflags & 4u)) return QF_Q6_KIDROOM;
            if ((ai.aflags & 4u) && !c->aud.mature_ok) return QF_Q6_MATURE;
        }
        {
            DoomoResident buf;
            const DoomoResident *r = id <= DOOMO_PLAYER_ID ? town_res(id) : (family_resident(id, &buf) ? &buf : NULL);
            budget = r ? (eff[PERS_EXPRESSION] <= 4 ? 1 : eff[PERS_EXPRESSION] <= 10 ? 2 : 3) : 3;
            if (t->tic != 0xFF && r && t->tic != persona_type(r)) return QF_Q3_TIC;
        }
        for (i = 0; l->text[i]; i++) {
            if (l->text[i] == '{') { while (l->text[i] && l->text[i] != '}') i++; if (!l->text[i]) break; continue; }
            nex += l->text[i] == '!';
        }
        if (nex > budget) return QF_Q3_EXCL;
        /* Q4 */
        if (!q4_fact(c, l->who, t)) return QF_Q4_FACT;
        /* Q6: re-lint with the room (the generator's lint counted nothing, or the line is safe) */
        {
            int sw = 0;
            TalkAudience aud = c->aud;
            if (talk_safe_lint(l->text, &aud, &sw) != TL_OK) return QF_Q6_LINT;
            if (kid && (talk_safe_lint_probe(l->text, &aud, 1) != TL_OK || l->face == EXPR_LOVE + 1)) return QF_Q6_KIDROOM;
        }
    }
    return QF_NONE;
}

/* ---- Q8: the coherence checks, report mode (TM2b; TALK_MULTILEVEL.md 7.2) ----------------------
 * The old engine has no plan, so every check reads what the generator leaves in the trace and the
 * lines: the speaker's stance, the core alternative's guard terms, the concepts said, the acts. When
 * the plan lands (TM2c) the same checks read the plan's kind, lean, depth and hook as well. */
static int q8_topic_act(int a)
{
    switch (a) {
    case TA_ASK_ABOUT: case TA_ASK_OPINION: case TA_TELL: case TA_OPINE_POS: case TA_OPINE_NEG:
    case TA_CHANGE_TOPIC: case TA_CONTINUE: case TA_AGREE: case TA_DISAGREE: case TA_ANSWER: case TA_GOSSIP:
        return 1;
    }
    return 0;
}

/* which side of the topic a reply takes: +1 for it, -1 against, 0 none (an agree reads the player's side) */
static int q8_valence(int reply, int player)
{
    return talk_gen_reply_side(player, reply);
}

/* the visible text of a line: no line codes, no *stage directions*, one space between words */
static void q8_visible(const char *s, char *v, int cap)
{
    int o = 0, sp = 1;
    for (; *s && o < cap - 1; s++) {
        if (*s == '{') { while (*s && *s != '}') s++; if (!*s) break; continue; }
        if (*s == '*') { s++; while (*s && *s != '*') s++; if (!*s) break; continue; }
        if (*s == ' ') { if (!sp) v[o++] = ' '; sp = 1; continue; }
        v[o++] = *s;
        sp = 0;
    }
    while (o > 0 && v[o - 1] == ' ') o--;
    v[o] = 0;
}

/* a filler word or phrase alone (the lint's L6 list): "Hm.", "Sure.", "I see.", "Go on." */
static int q8_filler_word(const char *vis)
{
    static const char *const w[] = { "hm", "hmm", "mm", "mmm", "mhm", "uh", "um", "oh", "ah", "huh", "eh", "sure", "ok", "okay",
        "fine", "yeah", "yep", "yes", "no", "nope", "nah", "right", "well", "wow", "cool", "nice", "whatever", "indeed",
        "i see", "if you say so", "go on", "interesting", "really", "true", "maybe", "perhaps", "hmph", "meh", "so",
        "anyway", "noted", "sigh", "ha", "haha", "heh", "lol" };
    char b[64];
    int i, o = 0;
    unsigned k;
    for (i = 0; vis[i] && o < 62; i++) {
        if (isalpha((unsigned char)vis[i])) b[o++] = (char)tolower((unsigned char)vis[i]);
        else if (vis[i] == ' ' && o && b[o - 1] != ' ') b[o++] = ' ';
        else if (vis[i] == '\'') continue;
    }
    while (o && b[o - 1] == ' ') o--;
    b[o] = 0;
    if (vis[i]) return 0;                                    /* longer than any filler */
    for (k = 0; k < sizeof w / sizeof w[0]; k++)
        if (strcmp(b, w[k]) == 0) return 1;
    return 0;
}

static int q8_has_q(const TalkExchange *x)
{
    int k;
    for (k = 0; k < x->n; k++) if (strchr(x->line[k].text, '?')) return 1;
    return 0;
}

/* the core alternative's guard needs the topic (a slot, a `topic` / likes / fears / hates / stance / root guard) */
static int q8_alt_needs_topic(int alt)
{
    TalkAltInfo ai;
    int j;
    if (alt == 0xFFFF || !talk_gen_alt_info(alt, &ai)) return 0;
    for (j = 0; j < ai.nterm; j++) {
        int p = ai.term[2 * j], v = ai.term[2 * j + 1];
        if (p & 0x80) continue;
        if (p == TGP_TOPIC || p == TGP_LIKES_TOPIC || p == TGP_FEARS_TOPIC || p == TGP_HATES_TOPIC ||
            (p >= TGP_X_COUNTABLE && p <= TGP_X_GRIEF) || p == TGP_ROOT || (p == TGP_STANCE && v != TST_NEUTRAL))
            return 1;
    }
    return 0;
}

static int q8_said_topic(const TalkTrace *t, uint16_t topic)
{
    uint16_t ids[TALK_ASSOC_TOP];
    int i, n, k;
    for (k = 0; k < t->nsaid; k++) {
        if (t->said[k] == topic) return 1;
        n = talk_db_assoc(topic, ids, NULL);
        for (i = 0; i < n; i++) if (ids[i] == t->said[k]) return 1;
    }
    return 0;
}

unsigned talk_census_q8(const TalkConv *c, const TalkUtter *u, const TalkExchange *x, const TalkTrace *tr,
                        unsigned want, unsigned *applied)
{
    unsigned bad = 0, app = 0;
    int pa = u->act[0], gated = x->gate != TG_PASS || TA_IS_SAFETY(pa), kid, k;
    const TalkLine *l0 = x->n ? &x->line[0] : NULL;
    int pet0 = l0 && l0->who >= 0 && l0->who < c->n && c->id[l0->who] == TALK_PET_ID;
    kid = c->aud.any_minor || c->aud.avatar_minor || c->aud.target_minor;
    if (!l0 || gated || pet0 || (l0->flags & (TLF_SAFE | TLF_FALLBACK | TLF_END | TLF_REACTION))) goto done;
    /* Q8a: a yes to what the speaker dislikes, a no to what they like (the stance is the engine's own) */
    if ((want & (1u << Q8_KIND)) && (pa == TA_ASK_OPINION || pa == TA_ASK_ABOUT || pa == TA_OPINE_POS || pa == TA_OPINE_NEG) &&
        u->topic[0] && !(l0->flags & TLF_SINCERE) && !is_grief(u->topic[0])) {
        int v = q8_valence(l0->act, pa);
        if (v && l0->stance != TST_NEUTRAL) {
            app |= 1u << Q8_KIND;
            /* design 7.2: against the stance is a failure unless the plan recorded a reason (the relationship, the mood, the voice, a trait) */
            if (((l0->stance == TST_PRO && v < 0) || (l0->stance == TST_CON && v > 0)) &&
                !(tr[0].plan.kind && (tr[0].plan.flags & TPL_REASON)))
                bad |= 1u << Q8_KIND;
        }
    }
    /* Q8b: the player gave a topic, the reply is a topic act reply: it names the topic (or one of its
     * associations), or its core alternative's guard needs the topic (a pronoun line) */
    if ((want & (1u << Q8_CONTENT)) && q8_topic_act(pa) && u->topic[0] && !(l0->flags & TLF_SINCERE) &&
        !is_grief(u->topic[0]) && (tr[0].parts & TPART_CORE)) {
        app |= 1u << Q8_CONTENT;
        if (!q8_said_topic(&tr[0], u->topic[0]) && !q8_alt_needs_topic(tr[0].core_alt)) bad |= 1u << Q8_CONTENT;
    }
    /* Q8c: the voice guards against the sliders as the speaker has them now (the effective ones, TM2c); an edgy trait never stings when a minor is in the room */
    if (want & (1u << Q8_VOICE)) {
        for (k = 0; k < x->n; k++) {
            const TalkLine *l = &x->line[k];
            int id = l->who >= 0 && l->who < c->n ? c->id[l->who] : -1, i, root = 0;
            uint8_t eff[5];
            if (id < 0 || id == TALK_PET_ID) continue;
            if (!talk_gen_eff_persona(id, kid, eff)) memset(eff, 8, sizeof eff);
            app |= 1u << Q8_VOICE;
            for (i = 0; i < tr[k].nalt; i++) {
                TalkAltInfo ai;
                int j;
                if (!talk_gen_alt_info(tr[k].alt[i], &ai)) { bad |= 1u << Q8_VOICE; continue; }
                for (j = 0; j < ai.nterm; j++) {
                    int p = ai.term[2 * j] & 0x7F, neg = ai.term[2 * j] & 0x80, ok = voice_term_ok(id, p, ai.term[2 * j + 1], eff);
                    if (ok >= 0 && (neg ? ok : !ok)) bad |= 1u << Q8_VOICE;
                }
            }
            if (kid) {
                TalkConcept tc;
                TalkTraitBias b;
                if (l->topic && talk_db_concept(l->topic, &tc)) root = talk_db_cat_root(tc.cat);
                b = talk_trait_bias_id(id, l->act, root, 1);
                if (b.deny != 0 || b.mood < 0) bad |= 1u << Q8_VOICE;
            }
        }
    }
    /* Q8d: a no or a deflection of a request leaves a hook (a question or a hand-off); the player's own
     * question is not handed straight back */
    if (want & (1u << Q8_HOOK)) {
        int refused = l0->act == TA_NO || l0->act == TA_DISMISS || l0->act == TA_CHANGE_TOPIC;
        if ((pa == TA_ASK_FAVOR || pa == TA_INVITE) && refused) {
            int hook = q8_has_q(x);
            for (k = 0; k < x->n; k++) if (tr[k].parts & TPART_HANDOFF) hook = 1;
            app |= 1u << Q8_HOOK;
            if (!hook) bad |= 1u << Q8_HOOK;
        }
        if (talk_gen_act_class(pa) == TAC_ASK && u->topic[0]) {
            app |= 1u << Q8_HOOK;
            for (k = 0; k < x->n; k++)
                if (x->line[k].act == pa && x->line[k].topic == u->topic[0] && !(x->line[k].flags & TLF_REACTION))
                    bad |= 1u << Q8_HOOK;
        }
    }
    /* Q8e: the speaker's whole reply is at least 20 visible characters (unless the player's act is a
     * gesture, greeting, farewell, thank you or laugh) and never a filler word alone */
    if (want & (1u << Q8_FILLER)) {
        char all[TALK_LINE_MAX * 2], v[TALK_LINE_MAX];
        int total = 0, exempt = talk_gen_act_class(pa) == TAC_GESTURE || pa == TA_GREET || pa == TA_FAREWELL ||
                                pa == TA_THANK || pa == TA_LAUGH || (l0->flags & TLF_SINCERE);    /* a short sincere line (grief, care) is never padded */
        all[0] = 0;
        for (k = 0; k < x->n; k++) {
            if (x->line[k].who != l0->who || (x->line[k].flags & TLF_REACTION)) continue;
            q8_visible(x->line[k].text, v, (int)sizeof v);
            total += (int)strlen(v);
            if (strlen(all) + strlen(v) + 2 < sizeof all) { if (all[0]) strcat(all, " "); strcat(all, v); }
        }
        app |= 1u << Q8_FILLER;
        if ((total < 20 && !exempt) || q8_filler_word(all)) bad |= 1u << Q8_FILLER;
    }
    /* Q8f: a line the parser did not understand is named, guessed at or asked about; a joke, a backchannel
     * or a change of topic with no question is a failure */
    if ((want & (1u << Q8_UNKNOWN)) && pa == TA_UNKNOWN && !(l0->flags & TLF_SINCERE)) {    /* K-10: a grief topic is answered sincerely, never asked about */
        app |= 1u << Q8_UNKNOWN;
        if ((l0->act == TA_JOKE || l0->act == TA_BACKCHANNEL || l0->act == TA_CHANGE_TOPIC) && !q8_has_q(x))
            bad |= 1u << Q8_UNKNOWN;
    }
done:
    if (applied) *applied = app;
    return bad;
}

/* ---- normalised hashes ------------------------------------------------------- */
typedef struct Hashes { uint64_t surface, skeleton, meaning; } Hashes;

/* one line into the Surface (names <N>) and Skeleton (every filler its category) texts */
static void norm_line(const TalkLine *l, const TalkTrace *t, int role, char *surf, int scap, char *skel, int kcap)
{
    int i, s = 0, k = 0, f;
    s += snprintf(surf + s, (size_t)(scap - s), "R%d:", role);
    k += snprintf(skel + k, (size_t)(kcap - k), "R%d:", role);
    for (i = 0; l->text[i] && s < scap - 24 && k < kcap - 24; ) {
        const TalkFill *fill = NULL;
        char ch;
        for (f = 0; f < t->nfill; f++) if (t->fill[f].off == i && t->fill[f].len) fill = &t->fill[f];
        if (fill) {
            if (fill->kind == TFK_NAME) {
                s += snprintf(surf + s, (size_t)(scap - s), "<n>");
                k += snprintf(skel + k, (size_t)(kcap - k), "<n>");
            } else {
                char cat[24] = "x";
                int j;
                for (j = 0; j < fill->len; j++) {
                    ch = (char)tolower((unsigned char)l->text[i + j]);
                    if (isalnum((unsigned char)ch)) surf[s++] = ch;
                    else if (ch == ' ' && s && surf[s - 1] != ' ') surf[s++] = ' ';
                }
                if (fill->kind == TFK_CONCEPT) talk_db_cat_name(talk_db_cat_root(fill->cat), cat, (int)sizeof cat);
                else snprintf(cat, sizeof cat, "text");
                k += snprintf(skel + k, (size_t)(kcap - k), "<%s>", cat);
            }
            i += fill->len;
            continue;
        }
        ch = l->text[i];
        if (ch == '{') { while (l->text[i] && l->text[i] != '}') i++; if (l->text[i]) i++; continue; }
        ch = (char)tolower((unsigned char)ch);
        if (isdigit((unsigned char)ch)) {
            while (isdigit((unsigned char)l->text[i])) i++;
            s += snprintf(surf + s, (size_t)(scap - s), "<#>");
            k += snprintf(skel + k, (size_t)(kcap - k), "<#>");
            continue;
        }
        if (isalpha((unsigned char)ch)) { surf[s++] = ch; skel[k++] = ch; }
        else if (ch == ' ' || ch == '-' || ch == '/') {
            if (s && surf[s - 1] != ' ') surf[s++] = ' ';
            if (k && skel[k - 1] != ' ') skel[k++] = ' ';
        }
        i++;
    }
    surf[s] = 0;
    skel[k] = 0;
}

static Hashes exchange_hashes(const TalkConv *c, const TalkExchange *x, const TalkTrace *tr)
{
    Hashes h;
    int k, roles[TALK_PARTS_MAX], nroles = 0, responders = 0;
    char surf[256], skel[256];
    h.surface = h.skeleton = h.meaning = FNV0;
    for (k = 0; k < TALK_PARTS_MAX; k++) roles[k] = -1;
    for (k = 0; k < x->n; k++) {
        int w = x->line[k].who;
        if (w >= 0 && w < TALK_PARTS_MAX && roles[w] < 0) { roles[w] = ++nroles; responders++; }
    }
    for (k = 0; k < x->n; k++) {
        const TalkLine *l = &x->line[k];
        uint8_t m[8];
        int role = l->who >= 0 && l->who < TALK_PARTS_MAX ? roles[l->who] : 0;
        TalkConcept tc;
        norm_line(l, &tr[k], role, surf, (int)sizeof surf, skel, (int)sizeof skel);
        h.surface = fnv64(h.surface, surf, (int)strlen(surf) + 1);
        h.skeleton = fnv64(h.skeleton, skel, (int)strlen(skel) + 1);
        m[0] = l->act; m[1] = l->stance;
        m[2] = (uint8_t)(l->topic && talk_db_concept(l->topic, &tc) ? talk_db_cat_root(tc.cat) : 0);
        m[3] = tr[k].cb; m[4] = c->beat; m[5] = (uint8_t)responders; m[6] = (uint8_t)role; m[7] = 0;
        h.meaning = fnv64(h.meaning, (const char *)m, 8);
    }
    return h;
}

/* ---- contexts ---------------------------------------------------------------- */
static uint32_t rnd(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }

static const uint8_t mood_of_band[TMB_COUNT] = { MOOD_OK, MOOD_HAPPY, MOOD_SAD, MOOD_ANGRY, MOOD_SCARED,
                                                 MOOD_SLEEPY, MOOD_CORRUPTED };

static void set_rel(int id, int band)
{
    DoomoEdge *e;
    int kind = REL_ACQUAINTANCE, v = 10;
    if (band == TRB_PARTNER && !(sim_romance_ok(DOOMO_PLAYER_ID, id) && sim_romance_ok(id, DOOMO_PLAYER_ID)))
        band = TRB_FRIEND;
    switch (band) {
    case TRB_STRANGER: e = rel_find(DOOMO_PLAYER_ID, id); if (e) { e->value = 0; e->kind = REL_ACQUAINTANCE; } return;
    case TRB_ACQUAINTANCE: kind = REL_ACQUAINTANCE; v = 15; break;
    case TRB_FRIEND: kind = REL_FRIEND; v = 50; break;
    case TRB_BEST: kind = REL_BEST_FRIEND; v = 85; break;
    case TRB_PARTNER: kind = REL_DATING; v = 80; break;
    case TRB_RIVAL: kind = REL_RIVAL; v = -50; break;
    default: break;
    }
    e = rel_get_or_add(DOOMO_PLAYER_ID, id);
    if (e) { e->kind = (uint8_t)kind; e->value = (int8_t)v; }
}

typedef struct Ctx { int ids[3], n, target; uint8_t act; uint16_t topic; int mem; } Ctx;

static void draw_context(const Grid *g, uint32_t *s, Ctx *x)
{
    int i, j;
    static const uint16_t minutes[3] = { 9 * 60, 14 * 60, 22 * 60 + 30 };
    static const uint8_t chapters[3] = { STORY_CH1, STORY_CH2, STORY_CH3 };
    x->n = 1 + (int)(rnd(s) % 3u);
    for (i = 0; i < x->n; i++) {
        int id;
        do {
            id = g->res[rnd(s) % (uint32_t)g->nres];
            for (j = 0; j < i && x->ids[j] != id; j++) { }
        } while (j < i);
        x->ids[i] = id;
        town_res(id)->mood = mood_of_band[rnd(s) % TMB_COUNT];
        set_rel(id, (int)(rnd(s) % 6u));
    }
    x->act = g->act[rnd(s) % (uint32_t)g->nacts];
    x->topic = g->topic[rnd(s) % (uint32_t)g->ntopics];
    x->target = (rnd(s) % 10u) < 7 ? (int)(rnd(s) % (uint32_t)x->n) : -1;
    x->mem = (int)(rnd(s) % 4u);
    doomo_town.minute = minutes[rnd(s) % 3u];
    doomo_town.story.chapter = chapters[rnd(s) % 3u];
}

static void begin(TalkConv *c, const Grid *g, const Ctx *x, uint32_t seed)
{
    int k;
    talk_gen_begin(c, DOOMO_PLAYER_ID, x->ids, x->n, seed);
    for (k = 0; k < x->n; k++) {
        static const uint8_t flags[4] = { 0, TMF_COMPLIMENTED, TMF_INSULTED, TMF_PROMISED };
        c->mem_flags[k] = flags[x->mem];
        c->mem_topic[k] = x->mem ? g->topic[(seed + (uint32_t)k * 7u) % (uint32_t)g->ntopics] : TC_NONE;
    }
}

static void write_sample(FILE *f, uint32_t i, const TalkConv *c, const TalkUtter *u, const TalkExchange *x,
                         const TalkTrace *tr)
{
    char topic[48] = "-";
    int k;
    if (u->topic[0]) talk_db_form(u->topic[0], TF_DEFAULT, topic, (int)sizeof topic);
    fprintf(f, "%u\t%s\t%s\t", i, act_names[u->act[0] < TA_COUNT ? u->act[0] : 0], topic);
    for (k = 0; k < c->n; k++) {
        const DoomoResident *r = town_res(c->id[k]);
        fprintf(f, "%s%s (%s, %s%s)", k ? "; " : "", r ? r->name : "?",
                r ? doomo_persona_type_names[persona_type(r)] : "?",
                talk_safe_minor(c->id[k]) ? "kid" : "adult", c->aud.any_minor ? ", kid room" : "");
    }
    for (k = 0; k < x->n; k++) {
        const DoomoResident *r = town_res(c->id[x->line[k].who]);
        fprintf(f, "\t%s: %s", r ? r->name : "?", x->line[k].text);
    }
    /* the core alternatives (talk_sample.py maps them to file:line for the bank's writer) */
    fprintf(f, "\t#alts");
    for (k = 0; tr && k < x->n; k++) fprintf(f, " %d", tr[k].core_alt == 0xFFFF ? -1 : (int)tr[k].core_alt);
    if (tr && getenv("CENSUS_PLAN")) {                                       /* the plan of each line: kind, flags, lean, depth, content, hook, parts */
        fprintf(f, "\t#plan");
        for (k = 0; k < x->n; k++)
            fprintf(f, " k%d/f%d/l%d/d%d/c%d/h%d/p%x", tr[k].plan.kind, tr[k].plan.flags, tr[k].plan.lean, tr[k].plan.depth,
                    tr[k].plan.content, tr[k].plan.hook, tr[k].parts);
    }
    fputc('\n', f);
}

/* ---- the run ------------------------------------------------------------------ */
static void census_town(const Grid *g)
{
    memset(&doomo_town, 0, sizeof doomo_town);
    opt_defaults();
    town_new(TOWN_FREEPLAY, g->town ? g->town : 7u, TOWN_POP_PREMADE);
}

/* Q8 on one exchange: the counts (report mode: nothing here fails the exchange) and the --q8-fails line */
static void q8_count(CensusResult *res, const CensusCfg *cfg, uint32_t i, const TalkConv *c, const TalkUtter *u,
                     const TalkExchange *x, const TalkTrace *tr, unsigned want)
{
    unsigned app = 0, bad = talk_census_q8(c, u, x, tr, want, &app);
    int n;
    for (n = 0; n < Q8_COUNT; n++) {
        res->q8_n[n] += (app >> n) & 1u;
        res->q8[n] += (bad >> n) & 1u;
    }
    if (bad) {
        res->q8_any++;
        if (cfg->q8fails)
            for (n = 0; n < Q8_COUNT; n++)
                if ((bad >> n) & 1u) {
                    fprintf(cfg->q8fails, "%s [plan kind %d flags %d lean %d; line act %d stance %d]\t", talk_census_qf_names[QF_Q8_KIND + n],
                            x->n ? tr[0].plan.kind : 0, x->n ? tr[0].plan.flags : 0, x->n ? tr[0].plan.lean : 0, x->n ? x->line[0].act : 0,
                            x->n ? x->line[0].stance : 0);
                    write_sample(cfg->q8fails, i, c, u, x, tr);
                }
    }
}

int talk_census_run(const CensusCfg *cfg, CensusResult *res, char *err, int errcap)
{
    Grid g;
    HSet hs, hk, hm, hc;
    uint32_t i, s, contexts, convs;
    uint32_t h0;
    clock_t t0;
    memset(res, 0, sizeof *res);
    if (talk_db_open(lang_find("en")) != TALK_DB_OK || !talk_gen_ready()) {
        snprintf(err, (size_t)errcap, "no TALK_en.BIN with the generator banks (make -C core talk)");
        return -1;
    }
    if (load_grid(cfg->grid, &g, err, errcap) < 0) return -1;
    census_town(&g);
    contexts = cfg->contexts ? cfg->contexts : g.contexts;
    convs = cfg->conversations;
    s = cfg->seed ? cfg->seed : (g.seed ? g.seed : 1u);
    res->nres = (uint32_t)g.nres; res->ntopics = (uint32_t)g.ntopics; res->nacts = (uint32_t)g.nacts;
    if (!hs_init(&hs, contexts) || !hs_init(&hk, contexts) || !hs_init(&hm, contexts)) {
        snprintf(err, (size_t)errcap, "out of memory");
        return -1;
    }
    h0 = talk_safe_lint_hits();
    t0 = clock();
    for (i = 0; i < contexts; i++) {
        Ctx x;
        TalkConv c;
        TalkUtter u;
        TalkExchange ex;
        TalkTrace tr[TALK_EXCHANGE_MAX];
        int q;
        draw_context(&g, &s, &x);
        begin(&c, &g, &x, s ^ (i * 2654435761u));
        memset(&u, 0, sizeof u);
        u.act[0] = x.act;
        u.topic[0] = x.topic;
        u.target = (int8_t)x.target;
        u.conf = 15;
        talk_gen_exchange(&c, &u, &ex, tr);
        res->contexts++;
        res->lines += ex.n;
        if (c.aud.any_minor) res->kid_rooms++;
        if (cfg->sample && cfg->sample_every && i % cfg->sample_every == 0) write_sample(cfg->sample, i, &c, &u, &ex, tr);
        q = ex.n ? talk_census_check(&c, &u, &ex, tr) : QF_Q1_FALLBACK;
        res->fail[q]++;
        if (ex.n) q8_count(res, cfg, i, &c, &u, &ex, tr, Q8_ALL);
        {
            int k;
            for (k = 0; k < ex.n; k++) if (tr[k].plan.kind) {
                res->plan_lines++;
                res->plan_kind[tr[k].plan.kind]++;
                if (tr[k].plan.flags & TPL_OPEN) res->plan_open++;
                if (tr[k].plan.flags & TPL_MISS) res->plan_miss++;
            }
        }
        if (q && cfg->fails) {
            fprintf(cfg->fails, "%s\t", talk_census_qf_names[q]);
            write_sample(cfg->fails, i, &c, &u, &ex, tr);
        }
        if (q == QF_NONE) {
            Hashes hh = exchange_hashes(&c, &ex, tr);
            res->exchanges_ok++;
            hs_add(&hs, hh.surface);
            hs_add(&hk, hh.skeleton);
            hs_add(&hm, hh.meaning);
        }
        if (!cfg->quiet && contexts >= 200000 && i % 200000 == 199999)
            fprintf(stderr, "talk_census: %u / %u contexts\n", i + 1, contexts);
    }
    res->secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    hs_tier(&hs, &res->surface);
    hs_tier(&hk, &res->skeleton);
    hs_tier(&hm, &res->meaning);
    hs_free(&hs); hs_free(&hk); hs_free(&hm);
    /* the conversation tier: 6 player turns a conversation */
    if (convs) {
        t0 = clock();
        if (!hs_init(&hc, (uint64_t)convs * 6u)) { snprintf(err, (size_t)errcap, "out of memory"); return -1; }
        for (i = 0; i < convs; i++) {
            Ctx x;
            TalkConv c;
            uint64_t seq[6], masked[6];
            int t, k, j, q5 = 0, distinct = 1;
            char seen[6 * TALK_EXCHANGE_MAX][TALK_LINE_MAX];
            int nseen = 0;
            uint8_t last_act = TA_GREET;
            uint16_t topic;
            draw_context(&g, &s, &x);
            begin(&c, &g, &x, s ^ (i * 0x9E3779B9u));
            topic = x.topic;
            for (t = 0; t < 6; t++) {
                TalkUtter u;
                TalkExchange ex;
                TalkTrace tr[TALK_EXCHANGE_MAX];
                Hashes hh;
                memset(&u, 0, sizeof u);
                /* the bot: a reply the adjacency table allows to the last line, a topic from the chips */
                if (t == 0) u.act[0] = TA_GREET;
                else {
                    int tries;
                    u.act[0] = g.act[rnd(&s) % (uint32_t)g.nacts];
                    for (tries = 0; tries < 8 && !talk_gen_adjacent(last_act, u.act[0]); tries++)
                        u.act[0] = g.act[rnd(&s) % (uint32_t)g.nacts];
                }
                if (rnd(&s) % 3u == 0) {
                    uint16_t a[TALK_ASSOC_TOP];
                    int na = talk_db_assoc(topic, a, NULL);
                    if (na) topic = a[rnd(&s) % (uint32_t)na];
                } else if (rnd(&s) % 4u == 0) topic = g.topic[rnd(&s) % (uint32_t)g.ntopics];
                u.topic[0] = topic;
                u.target = (int8_t)((rnd(&s) % 10u) < 7 ? (int)(rnd(&s) % (uint32_t)x.n) : -1);
                u.conf = 15;
                talk_gen_exchange(&c, &u, &ex, tr);
                hh = exchange_hashes(&c, &ex, tr);
                seq[t] = hh.surface;
                if (ex.n) last_act = ex.line[ex.n - 1].act;
                /* Q5: no line twice in a conversation; a core template once a speaker in 6 turns */
                for (k = 0; k < ex.n; k++) {
                    for (j = 0; j < nseen; j++)
                        if (strcmp(seen[j], ex.line[k].text) == 0) {
                            if (!q5 && cfg->fails) fprintf(cfg->fails, "Q5 repeat\t%u\t%s\n", i, ex.line[k].text);
                            q5 = 1;
                        }
                    if (nseen < 6 * TALK_EXCHANGE_MAX) strcpy(seen[nseen++], ex.line[k].text);
                }
            }
            /* two conversations are distinct when their hash sequences differ in 2 of the 6 or more:
             * keep it if no kept conversation shares any of its 6 leave-one-out keys */
            for (t = 0; t < 6; t++) {
                uint64_t h = FNV0;
                for (k = 0; k < 6; k++) {
                    uint64_t v = k == t ? 0x5A5A5A5Aull : seq[k];
                    h = fnv64(h, (const char *)&v, 8);
                }
                masked[t] = h;
                if (hs_has(&hc, h)) distinct = 0;
            }
            if (distinct) {
                for (t = 0; t < 6; t++) hs_add(&hc, masked[t]);
                res->conv_distinct++;
            }
            res->conv_q5 += (uint64_t)q5;
            res->conversations++;
            if (!cfg->quiet && convs >= 200000 && i % 200000 == 199999)
                fprintf(stderr, "talk_census: %u / %u conversations\n", i + 1, convs);
        }
        res->conv_secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
        hs_free(&hc);
    }
    res->lint_hits = talk_safe_lint_hits() - h0;
    talk_db_close();
    return 0;
}

/* The cool acts in kid rooms: does a resident still answer? (T5B-S3FIX; docs/TALK.md) */
const uint8_t talk_census_cool_acts[CENSUS_COOL_N] = { TA_INSULT, TA_MOCK, TA_TEASE, TA_BLAME, TA_THREATEN, TA_DISMISS };

int talk_census_cool_kid(const CensusCfg *cfg, uint32_t rooms_per_act, CensusCoolKid *res, char *err, int errcap)
{
    Grid g;
    uint32_t s, a, n;
    memset(res, 0, sizeof *res);
    if (talk_db_open(lang_find("en")) != TALK_DB_OK || !talk_gen_ready()) {
        snprintf(err, (size_t)errcap, "no TALK_en.BIN with the generator banks (make -C core talk)");
        return -1;
    }
    if (load_grid(cfg->grid, &g, err, errcap) < 0) { talk_db_close(); return -1; }
    census_town(&g);
    s = cfg->seed ? cfg->seed : (g.seed ? g.seed : 1u);
    for (a = 0; a < CENSUS_COOL_N; a++) {
        for (n = 0; n < rooms_per_act; ) {
            Ctx x;
            TalkConv c;
            TalkUtter u;
            TalkExchange ex;
            TalkTrace tr[TALK_EXCHANGE_MAX];
            draw_context(&g, &s, &x);
            x.act = talk_census_cool_acts[a];
            begin(&c, &g, &x, s ^ (n * 2654435761u));
            if (!c.aud.any_minor) continue;   /* only rooms with a child */
            memset(&u, 0, sizeof u);
            u.act[0] = x.act;
            u.topic[0] = x.topic;
            u.target = (int8_t)x.target;
            u.conf = 15;
            talk_gen_exchange(&c, &u, &ex, tr);
            n++;
            res->rooms[a]++;
            if (ex.n && !(ex.line[0].flags & (TLF_FALLBACK | TLF_SAFE))) res->replied[a]++;
            else if (res->shown < 5 && cfg->fails) { /* the first few misses, for the log */
                fprintf(cfg->fails, "%s\t", act_names[x.act]);
                write_sample(cfg->fails, n, &c, &u, &ex, tr);
                res->shown++;
            }
        }
    }
    talk_db_close();
    return 0;
}

static void tier_line(FILE *f, const char *name, const CensusTier *t, uint64_t n, uint64_t target)
{
    fprintf(f, "  %-9s %10llu distinct of %llu (%.1f%% unique), Chao1 %.0f, target %llu%s\n", name,
            (unsigned long long)t->distinct, (unsigned long long)n, n ? 100.0 * (double)t->distinct / (double)n : 0.0,
            t->chao1, (unsigned long long)target, t->distinct >= target ? "" : "  (MISSED)");
}

/* the Q8 table (report mode: counted, not failing; TM2e makes the budget a test) */
void talk_census_q8_print(const CensusResult *r, FILE *f)
{
    static const char *const nm[Q8_COUNT] = { "Q8a reaction (yes to a dislike, no to a like)", "Q8b content (topic unused)",
                                              "Q8c colour (voice, trait around a child)", "Q8d hook (dead end, question handed back)",
                                              "Q8e filler (under 20 characters, a filler word)", "Q8f unknown word (joke, backchannel, topic change)" };
    int n;
    fprintf(f, "  Q8 coherence (report mode, not failing): %llu of %llu exchanges with a failure (%.2f%%)\n", (unsigned long long)r->q8_any, (unsigned long long)r->contexts,
            r->contexts ? 100.0 * (double)r->q8_any / (double)r->contexts : 0.0);
    for (n = 0; n < Q8_COUNT; n++)
        fprintf(f, "    %-52s %7llu of %7llu applied (%.2f%%)\n", nm[n], (unsigned long long)r->q8[n],
                (unsigned long long)r->q8_n[n], r->q8_n[n] ? 100.0 * (double)r->q8[n] / (double)r->q8_n[n] : 0.0);
}

void talk_census_print(const CensusResult *r, FILE *f)
{
    int q;
    uint64_t failed = r->contexts - r->exchanges_ok;
    fprintf(f, "talk_census: %llu contexts (%u residents, %u topics, %u acts; %llu kid rooms), %llu lines, %.1f s\n",
            (unsigned long long)r->contexts, r->nres, r->ntopics, r->nacts, (unsigned long long)r->kid_rooms,
            (unsigned long long)r->lines, r->secs);
    fprintf(f, "  quality: %llu passed, %llu failed (%.2f%%; the bar is 1%%)\n", (unsigned long long)r->exchanges_ok,
            (unsigned long long)failed, r->contexts ? 100.0 * (double)failed / (double)r->contexts : 0.0);
    for (q = 1; q < QF_COUNT; q++)
        if (r->fail[q]) fprintf(f, "    %-30s %llu\n", talk_census_qf_names[q], (unsigned long long)r->fail[q]);
    tier_line(f, "Surface", &r->surface, r->exchanges_ok, 1000000);
    tier_line(f, "Skeleton", &r->skeleton, r->exchanges_ok, 100000);
    tier_line(f, "Meaning", &r->meaning, r->exchanges_ok, 5000);
    if (r->conversations)
        fprintf(f, "  conversations: %llu distinct of %llu (differ in 2 or more of 6 exchanges), target 1000000%s; "
                   "Q5 repeats in %llu; %.1f s\n",
                (unsigned long long)r->conv_distinct, (unsigned long long)r->conversations,
                r->conv_distinct >= 1000000 ? "" : "  (MISSED)", (unsigned long long)r->conv_q5, r->conv_secs);
    if (r->plan_lines) {
        static const char *const kn[TK_COUNT] = { "", "accept", "agree", "deny", "refuse", "deflect", "counter", "askback" };
        int k;
        fprintf(f, "  plan: %llu answer lines, %llu open (no kind by a margin), soft-filter misses %llu (%.2f%%);",
                (unsigned long long)r->plan_lines, (unsigned long long)r->plan_open, (unsigned long long)r->plan_miss,
                100.0 * (double)r->plan_miss / (double)r->plan_lines);
        for (k = 1; k < TK_COUNT; k++) fprintf(f, " %s %llu", kn[k], (unsigned long long)r->plan_kind[k]);
        fputc('\n', f);
    }
    fprintf(f, "  S3 lint hits: %llu\n", (unsigned long long)r->lint_hits);
    talk_census_q8_print(r, f);
}

int talk_census_targets_met(const CensusResult *r)
{
    uint64_t failed = r->contexts - r->exchanges_ok;
    return r->surface.distinct >= 1000000 && r->skeleton.distinct >= 100000 && r->meaning.distinct >= 5000 &&
           r->conv_distinct >= 1000000 && failed * 100 <= r->contexts && r->lint_hits == 0 &&
           r->fail[QF_Q6_LINT] == 0 && r->fail[QF_Q6_KIDROOM] == 0 && r->fail[QF_Q6_MATURE] == 0;
}

#ifndef TALK_CENSUS_NO_MAIN
#include "native.h"

/* --pick: 32 residents covering the 16 persona types in both districts, 4 of
 * the cast, 4 elders, 4 teens, 4 children; 400 topics over the categories */
static int picked(const int *c, int n, int id)
{
    int j;
    for (j = 0; j < n; j++) if (c[j] == id) return 1;
    return 0;
}

static void pick(void)
{
    static const int cast[] = { 1, 5, 666, 729 };          /* Nana Puddin, Bonejangles, Vex, Grumbleguts */
    int chosen[64], n = 0, i, id, t, kids = 0, teens = 0, elders = 0;
    for (i = 0; i < 4; i++) {
        chosen[n++] = cast[i];
        if (sim_life_stage(cast[i]) == LIFE_ELDER) elders++;
    }
    /* one adult of each of the 16 persona types, the districts taking turns */
    for (t = 0; t < PERSONA_TYPE_COUNT; t++) {
        int d, found = 0;
        for (d = 0; d < 2 && !found; d++) {
            int dist = (t + d) & 1;
            for (id = dist * DOOMO_DISTRICT_SIZE; id < (dist + 1) * DOOMO_DISTRICT_SIZE && !found; id++) {
                const DoomoResident *r = town_res(id);
                if (!r || r->status != RES_ALIVE || persona_type(r) != t || talk_safe_minor(id) ||
                    sim_life_stage(id) == LIFE_ELDER || picked(chosen, n, id))
                    continue;
                chosen[n++] = id;
                found = 1;
            }
        }
    }
    /* to 20 adults (the cast's adults included) */
    for (id = DOOMO_MAX_RESIDENTS - 1; id >= 0 && n - elders < 20; id--) {
        const DoomoResident *r = town_res(id);
        if (!r || r->status != RES_ALIVE || talk_safe_minor(id) || sim_life_stage(id) == LIFE_ELDER ||
            picked(chosen, n, id))
            continue;
        chosen[n++] = id;
    }
    /* elders, teens and children (Pip among them) to 4 each */
    if (!picked(chosen, n, 42)) { chosen[n++] = 42; kids++; }
    for (id = 0; id < DOOMO_MAX_RESIDENTS && (kids < 4 || teens < 4 || elders < 4); id++) {
        int st = sim_life_stage(id);
        const DoomoResident *r = town_res(id);
        if (!r || r->status != RES_ALIVE || picked(chosen, n, id)) continue;
        if (st == LIFE_CHILD && kids < 4) { chosen[n++] = id; kids++; }
        else if (st == LIFE_TEEN && teens < 4) { chosen[n++] = id; teens++; }
        else if (st == LIFE_ELDER && elders < 4) { chosen[n++] = id; elders++; }
    }
    for (i = 0; i < n; i++) {
        const DoomoResident *r = town_res(chosen[i]);
        printf("resident %d\t# %s, %s, %s, %s\n", chosen[i], r->name, doomo_persona_type_names[persona_type(r)],
               doomo_life_stage_names[sim_life_stage(chosen[i])], RES_DISTRICT(chosen[i]) ? "Hellvile" : "Hillvile");
    }
    {
        int ncat = talk_db_ncats(), cat, round, got = 0, nc = talk_db_nconcepts();
        for (round = 0; got < 400 && round < 64; round++)
            for (cat = 1; cat <= ncat && got < 400; cat++) {
                uint8_t parent, flags;
                int k = 0, c;
                char key[48];
                if (!talk_db_cat(cat, &parent, &flags) || !parent || !(flags & TCAT_TALKABLE) ||
                    (flags & (TCAT_SAFETY | TCAT_GATED | TCAT_DESCRIBE)))
                    continue;
                for (c = 1; c <= nc; c++) {
                    TalkConcept tc;
                    if (!talk_db_concept(c, &tc) || tc.cat != cat || tc.age == TAGE_NEVER || tc.reg >= TR_SEXUAL)
                        continue;
                    if (k++ != round * 7) continue;
                    talk_db_form(c, TF_LEMMA, key, (int)sizeof key);
                    for (k = 0; key[k]; k++) key[k] = key[k] == ' ' ? '_' : (char)tolower((unsigned char)key[k]);
                    printf("topic %s\n", key);
                    got++;
                    break;
                }
            }
    }
}

int main(int argc, char **argv)
{
    CensusCfg cfg;
    CensusResult r;
    NatOptions o;
    char err[256];
    int i, smoke = 0, do_pick = 0, full = 0, cool_kid = 0;
    const char *sample = NULL;
    memset(&cfg, 0, sizeof cfg);
    cfg.grid = "core/data/talk/census_en.tsv";
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--grid") == 0 && i + 1 < argc) cfg.grid = argv[++i];
        else if (strcmp(argv[i], "--contexts") == 0 && i + 1 < argc) cfg.contexts = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--conversations") == 0 && i + 1 < argc)
            cfg.conversations = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) cfg.seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--sample") == 0 && i + 1 < argc) sample = argv[++i];
        else if (strcmp(argv[i], "--every") == 0 && i + 1 < argc) cfg.sample_every = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--fails") == 0 && i + 1 < argc) {
            cfg.fails = fopen(argv[++i], "w");
            if (!cfg.fails) { perror(argv[i]); return 2; }
        }
        else if (strcmp(argv[i], "--q8-fails") == 0 && i + 1 < argc) {
            cfg.q8fails = fopen(argv[++i], "w");
            if (!cfg.q8fails) { perror(argv[i]); return 2; }
        }
        else if (strcmp(argv[i], "--smoke") == 0) smoke = 1;
        else if (strcmp(argv[i], "--full") == 0) full = 1;
        else if (strcmp(argv[i], "--pick") == 0) do_pick = 1;
        else if (strcmp(argv[i], "--quiet") == 0) cfg.quiet = 1;
        else if (strcmp(argv[i], "--cool-kid") == 0) cool_kid = 1;
        else { fprintf(stderr, "usage: talk_census [--grid F] [--full|--smoke] [--contexts N] [--conversations N] "
                               "[--seed N] [--sample F --every N] [--fails F] [--q8-fails F] [--pick] [--cool-kid [--contexts N-per-act]]\n"); return 2; }
    }
    memset(&o, 0, sizeof o);
    o.w = 320; o.h = 240; o.seed = 1; o.quiet = getenv("DOOMO_TALK_LOG") ? 0 : 1; o.save_dir = "/tmp";
    nat_init(&o);
    doomo_init();
    if (do_pick) {
        memset(&doomo_town, 0, sizeof doomo_town);
        opt_defaults();
        town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
        if (talk_db_open(lang_find("en")) != TALK_DB_OK) { fprintf(stderr, "talk_census: no TALK_en.BIN\n"); return 2; }
        pick();
        return 0;
    }
    if (sample) {
        cfg.sample = fopen(sample, "w");
        if (!cfg.sample) { perror(sample); return 2; }
        if (!cfg.sample_every) cfg.sample_every = 1000;
    }
    if (smoke) {
        Grid g;
        /* the grid's smoke sizes (read it once for them) */
        if (talk_db_open(lang_find("en")) == TALK_DB_OK && load_grid(cfg.grid, &g, err, (int)sizeof err) == 0) {
            if (!cfg.contexts) cfg.contexts = g.smoke_contexts;
            if (!cfg.conversations) cfg.conversations = g.smoke_conversations;
        }
        talk_db_close();
    } else if (full) {
        Grid g;
        if (talk_db_open(lang_find("en")) == TALK_DB_OK && load_grid(cfg.grid, &g, err, (int)sizeof err) == 0)
            if (!cfg.conversations) cfg.conversations = g.conversations;
        talk_db_close();
    }
    if (cool_kid) {
        CensusCoolKid ck;
        uint32_t a, per = cfg.contexts ? cfg.contexts : 20000u;
        if (talk_census_cool_kid(&cfg, per, &ck, err, (int)sizeof err) < 0) { fprintf(stderr, "talk_census: %s\n", err); return 2; }
        for (a = 0; a < CENSUS_COOL_N; a++)
            printf("cool-kid %-9s %u kid rooms, %u answered (%.2f%%)\n", act_names[talk_census_cool_acts[a]], ck.rooms[a],
                   ck.replied[a], ck.rooms[a] ? 100.0 * ck.replied[a] / ck.rooms[a] : 0.0);
        return 0;
    }
    if (talk_census_run(&cfg, &r, err, (int)sizeof err) < 0) {
        fprintf(stderr, "talk_census: %s\n", err);
        return 2;
    }
    if (cfg.sample) fclose(cfg.sample);
    if (cfg.fails) fclose(cfg.fails);
    if (cfg.q8fails) fclose(cfg.q8fails);
    talk_census_print(&r, stdout);
    if (full) {
        int ok = talk_census_targets_met(&r);
        printf("talk_census: %s\n", ok ? "every target met" : "a target was missed");
        return ok ? 0 : 1;
    }
    return 0;
}
#endif
