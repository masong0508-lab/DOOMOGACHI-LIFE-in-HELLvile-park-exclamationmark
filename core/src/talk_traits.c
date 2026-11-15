/* talk_traits.c - the trait model of Talk and the personality API (TM1-TRAITS).
 * Owner: TALK. Header: doomo/talk_traits.h. docs/TALK.md "Traits and the personality API".
 *
 * 24 Sims-like traits, up to three a person, plus liked and disliked topic roots, as a
 * TalkTraitSet. Nothing here adds a resident byte: the effective set is the creator's explicit
 * choice (a small saved table, native only) or a pure function of the record (see the header).
 * The safety rules are in code, not in data:
 *   - a romance trait (romantic, jealous) is inert for a minor: talk_trait_bias() gives the empty
 *     set's answer, and a minor's default set never has one;
 *   - when a minor is involved no trait turns a request down harder, stings or makes the speaker
 *     cool (deny 0, mood never below 0): a mean adult is gruff to adults only;
 *   - a romance topic is never liked or disliked by a minor.
 * No stdio, no malloc, no floats. The tables are const and go with --gc-sections where nobody calls
 * them (the GBA and PS1 builds do not).
 */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/talk_traits.h"
#include "doomo/sim.h"
#include "doomo/family.h"
#include "doomo/town.h"
#include "doomo/rng.h"

DOOMO_STATIC_ASSERT(TALK_TRAIT_CUSTOM_MAX == 24, talk_save_worst_counts_24_trait_sets);

/* ---- the table ---------------------------------------------------------------- */
typedef struct TraitInfo {
    const char *name, *adj, *desc;
    int8_t  w[PERS_COUNT];          /* default pull of the sliders (movement, speech, expression, attitude, quirk) */
    uint8_t fav, dull;              /* topic roots this trait likes / avoids (TCR_*) */
    uint8_t romance;                /* a romance trait: inert for a minor */
} TraitInfo;

static const TraitInfo ti[TT_COUNT] = {
    { "", "", "", {0, 0, 0, 0, 0}, 0, 0, 0 },
    { "Hot-headed", "hot-headed", "Flares up fast at digs and pushy asks; cools down just as fast.",
      {1, 2, 1, -2, 0}, TCR_HELLVILE, TCR_TIME, 0 },
    { "Romantic", "romantic", "Melts at sweet words and loves a love story (grown-ups only).",
      {0, -1, 2, 0, 0}, TCR_ROMANCE, TCR_WORK, 1 },
    { "Goofball", "goofy", "Turns everything into a bit and loves a joke back.",
      {0, 0, 1, 2, 2}, TCR_HOBBIES, TCR_SCHOOL, 0 },
    { "Snob", "snobby", "Picky about invites and gifts; likes being told they have taste.",
      {-1, 1, -1, -1, 0}, TCR_PLACES, TCR_CREATURES, 0 },
    { "Neat", "neat", "Likes things tidy and plans; happy to help set things right.",
      {0, -1, 0, -1, -2}, TCR_HOUSE, TCR_HELLVILE, 0 },
    { "Slob", "messy", "Easygoing about everything except effort; will say yes to snacks and sofa.",
      {-1, 0, 0, 1, 1}, TCR_FOOD, TCR_HOUSE, 0 },
    { "Bookworm", "bookish", "Loves facts, questions and quiet; would rather stay in than go out.",
      {-2, -1, -1, -1, 0}, TCR_SCHOOL, TCR_TIME, 0 },
    { "Cheerful", "cheerful", "Sees the bright side; takes kind words and jokes gladly.",
      {1, -1, 2, 2, 0}, TCR_FEELINGS, TCR_FEARS, 0 },
    { "Gloomy", "gloomy", "Expects the worst; warms to comfort, groans at jokes.",
      {-1, 0, -1, -2, 0}, TCR_LORE, TCR_HOBBIES, 0 },
    { "Jealous", "jealous", "Bristles when someone else gets the praise (grown-ups only).",
      {0, 0, 1, -1, 1}, TCR_PEOPLE, TCR_ROMANCE, 1 },
    { "Loyal", "loyal", "Sticks up for their people and keeps promises.",
      {0, -1, 0, -1, -1}, TCR_PEOPLE, TCR_WORK, 0 },
    { "Mean", "mean", "Gruff and sharp with grown-ups; never to a kid.",
      {0, 2, -1, -1, 1}, 0, TCR_FEELINGS, 0 },
    { "Kind", "kind", "Gentle, helpful, and hard to offend.",
      {0, -2, 1, 1, -1}, TCR_PEOPLE, 0, 0 },
    { "Lazy", "lazy", "Would rather not: invites and favours get a no, offers a yes.",
      {-2, 0, -1, 2, 0}, TCR_FOOD, TCR_WORK, 0 },
    { "Ambitious", "driven", "Always working on something; likes advice and a push.",
      {2, 1, 0, -1, 0}, TCR_WORK, TCR_HOUSE, 0 },
    { "Paranoid", "paranoid", "Suspicious of questions, gifts and strangers; listens to gossip.",
      {1, 0, -1, -1, 2}, TCR_FEARS, TCR_PEOPLE, 0 },
    { "Self-assured", "self-assured", "Takes praise as their due and shrugs off digs.",
      {1, 1, 1, 1, -1}, TCR_PEOPLE, TCR_FEARS, 0 },
    { "Shy", "shy", "Hides from invites and hugs; opens up slowly.",
      {-1, -1, -2, 0, 0}, TCR_HOUSE, TCR_PEOPLE, 0 },
    { "Outgoing", "outgoing", "Says yes to any invite and loves a chat.",
      {1, 1, 2, 1, 0}, TCR_PLACES, TCR_HOUSE, 0 },
    { "Creative", "creative", "Full of ideas; asks for opinions and says yes to a project.",
      {0, 0, 1, 0, 2}, TCR_MUSIC, TCR_WORK, 0 },
    { "Glutton", "food-loving", "Lives for food; an offer of a bite is a good day.",
      {-1, 0, 0, 1, 0}, TCR_FOOD, TCR_TIME, 0 },
    { "Morbid", "morbid", "Fascinated by the dark side of Hellvile lore (never grief; never a joke about loss).",
      {0, 0, -1, -1, 1}, TCR_LORE, TCR_FEELINGS, 0 },
    { "Curious", "curious", "Asks about everything and loves a new fact.",
      {1, 0, 1, 0, 1}, TCR_CREATURES, TCR_TIME, 0 },
    { "Honest", "blunt", "Says what they think, kindly or not; values a straight answer.",
      {0, 2, 0, 0, -1}, 0, 0, 0 },
};

static const char *const root_names[TALK_TRAIT_ROOT_MAX + 1] = {
    "", "people", "places", "food", "music", "hobbies", "fears", "creatures", "the house",
    "lore", "Hellvile", "feelings", "work", "time", "school", "romance"
};

const char *talk_trait_name(int t) { return t > 0 && t < TT_COUNT ? ti[t].name : ""; }
const char *talk_trait_desc(int t) { return t > 0 && t < TT_COUNT ? ti[t].desc : ""; }
const char *talk_trait_adj(int t) { return t > 0 && t < TT_COUNT ? ti[t].adj : ""; }
const char *talk_trait_topic_name(int root)
{
    return root > 0 && root <= TALK_TRAIT_ROOT_MAX ? root_names[root] : "";
}
int talk_trait_is_romance(int t) { return t > 0 && t < TT_COUNT && ti[t].romance; }

int talk_trait_conflict(int a, int b)
{
    static const uint8_t pair[][2] = {
        { TT_KIND, TT_MEAN }, { TT_CHEERFUL, TT_GLOOMY }, { TT_NEAT, TT_SLOB }, { TT_SHY, TT_OUTGOING },
        { TT_LAZY, TT_AMBITIOUS }, { TT_SELF_ASSURED, TT_PARANOID }, { TT_SHY, TT_SELF_ASSURED },
        { TT_KIND, TT_HOTHEADED }, { TT_LOYAL, TT_JEALOUS }, { TT_GOOFBALL, TT_SNOB }
    };
    unsigned i;
    if (a == b) return 0;
    for (i = 0; i < sizeof pair / sizeof pair[0]; i++)
        if ((pair[i][0] == a && pair[i][1] == b) || (pair[i][0] == b && pair[i][1] == a)) return 1;
    return 0;
}

static int stage_ok(int stage) { return stage >= 0 && stage < LIFE_COUNT ? stage : LIFE_ADULT; }
static int stage_minor(int stage) { return LIFE_IS_MINOR(stage_ok(stage)); }

int talk_trait_inert(int t, int stage) { return talk_trait_is_romance(t) && stage_minor(stage); }
int talk_trait_topic_ok(int root, int stage)
{
    if (root < 1 || root > TALK_TRAIT_ROOT_MAX) return 0;
    return root != TCR_ROMANCE || !stage_minor(stage);
}

/* ---- editing ----------------------------------------------------------------- */
void talk_traitset_clear(TalkTraitSet *ts) { memset(ts, 0, sizeof *ts); }

int talk_traitset_count(const TalkTraitSet *ts)
{
    int i, n = 0;
    for (i = 0; i < TALK_TRAIT_MAX; i++) n += ts->trait[i] != 0;
    return n;
}

int talk_traitset_has(const TalkTraitSet *ts, int t)
{
    int i;
    if (t <= 0) return 0;
    for (i = 0; i < TALK_TRAIT_MAX; i++) if (ts->trait[i] == t) return 1;
    return 0;
}

int talk_traitset_add(TalkTraitSet *ts, int t)
{
    int i, n;
    if (t <= TT_NONE || t >= TT_COUNT) return TTE_RANGE;
    if (talk_traitset_has(ts, t)) return TTE_DUP;
    n = talk_traitset_count(ts);
    if (n >= TALK_TRAIT_MAX) return TTE_FULL;
    for (i = 0; i < n; i++) if (talk_trait_conflict(ts->trait[i], t)) return TTE_CONFLICT;
    ts->trait[n] = (uint8_t)t;
    return TTE_OK;
}

void talk_traitset_remove(TalkTraitSet *ts, int t)
{
    int i, j = 0;
    for (i = 0; i < TALK_TRAIT_MAX; i++)
        if (ts->trait[i] != t) ts->trait[j++] = ts->trait[i];
    while (j < TALK_TRAIT_MAX) ts->trait[j++] = 0;
}

static int topic_in(const uint8_t *a, int root)
{
    int i;
    for (i = 0; i < TALK_TRAIT_TOPICS; i++) if (a[i] == root) return 1;
    return 0;
}

static int topic_add(uint8_t *a, const uint8_t *other, int root)
{
    int i;
    if (root < 1 || root > TALK_TRAIT_ROOT_MAX) return TTE_RANGE;
    if (topic_in(a, root)) return TTE_DUP;
    if (topic_in(other, root)) return TTE_TOPIC_BOTH;
    for (i = 0; i < TALK_TRAIT_TOPICS; i++) if (!a[i]) { a[i] = (uint8_t)root; return TTE_OK; }
    return TTE_FULL;
}

int talk_traitset_like(TalkTraitSet *ts, int root) { return topic_add(ts->like, ts->dislike, root); }
int talk_traitset_dislike(TalkTraitSet *ts, int root) { return topic_add(ts->dislike, ts->like, root); }

static void topic_drop(uint8_t *a, int root)
{
    int i, j = 0;
    for (i = 0; i < TALK_TRAIT_TOPICS; i++) if (a[i] != root) a[j++] = a[i];
    while (j < TALK_TRAIT_TOPICS) a[j++] = 0;
}

void talk_traitset_forget_topic(TalkTraitSet *ts, int root)
{
    topic_drop(ts->like, root);
    topic_drop(ts->dislike, root);
}

static int topics_check(const uint8_t *a, const uint8_t *other, int stage)
{
    int i, j;
    for (i = 0; i < TALK_TRAIT_TOPICS; i++) {
        if (!a[i]) {
            for (j = i + 1; j < TALK_TRAIT_TOPICS; j++) if (a[j]) return TTE_GAP;
            break;
        }
        if (!talk_trait_topic_ok(a[i], stage)) return TTE_RANGE;
        for (j = i + 1; j < TALK_TRAIT_TOPICS; j++) if (a[j] == a[i]) return TTE_DUP;
        if (topic_in(other, a[i])) return TTE_TOPIC_BOTH;
    }
    return TTE_OK;
}

int talk_trait_validate(const TalkTraitSet *ts, int stage)
{
    int i, j, e;
    for (i = 0; i < TALK_TRAIT_MAX; i++) {
        if (ts->trait[i] >= TT_COUNT) return TTE_RANGE;
        if (!ts->trait[i]) {
            for (j = i + 1; j < TALK_TRAIT_MAX; j++) if (ts->trait[j]) return TTE_GAP;
            break;
        }
        for (j = i + 1; j < TALK_TRAIT_MAX; j++) {
            if (ts->trait[j] == ts->trait[i]) return TTE_DUP;
            if (ts->trait[j] && talk_trait_conflict(ts->trait[i], ts->trait[j])) return TTE_CONFLICT;
        }
    }
    e = topics_check(ts->like, ts->dislike, stage);
    return e ? e : topics_check(ts->dislike, ts->like, stage);
}

int talk_trait_normalize(TalkTraitSet *ts, int stage)
{
    TalkTraitSet o;
    int i, fixes = 0;
    talk_traitset_clear(&o);
    for (i = 0; i < TALK_TRAIT_MAX; i++) {
        if (!ts->trait[i]) continue;
        if (talk_traitset_add(&o, ts->trait[i]) != TTE_OK) fixes++;
    }
    for (i = 0; i < TALK_TRAIT_TOPICS; i++) {
        if (ts->like[i] && (!talk_trait_topic_ok(ts->like[i], stage) || talk_traitset_like(&o, ts->like[i]) != TTE_OK)) fixes++;
    }
    for (i = 0; i < TALK_TRAIT_TOPICS; i++) {
        if (ts->dislike[i] && (!talk_trait_topic_ok(ts->dislike[i], stage) || talk_traitset_dislike(&o, ts->dislike[i]) != TTE_OK)) fixes++;
    }
    if (memcmp(&o, ts, sizeof o) != 0 && fixes == 0) fixes = 1;      /* only a gap closed */
    *ts = o;
    return fixes;
}

uint32_t talk_traitset_pack(const TalkTraitSet *ts)
{
    uint32_t v = 0;
    int i, sh = 0;
    for (i = 0; i < TALK_TRAIT_MAX; i++, sh += 5) v |= (uint32_t)(ts->trait[i] & 31u) << sh;
    for (i = 0; i < TALK_TRAIT_TOPICS; i++, sh += 4) v |= (uint32_t)(ts->like[i] & 15u) << sh;
    for (i = 0; i < TALK_TRAIT_TOPICS; i++, sh += 4) v |= (uint32_t)(ts->dislike[i] & 15u) << sh;
    return v;
}

void talk_traitset_unpack(uint32_t v, TalkTraitSet *ts)
{
    int i, sh = 0;
    for (i = 0; i < TALK_TRAIT_MAX; i++, sh += 5) ts->trait[i] = (uint8_t)((v >> sh) & 31u);
    for (i = 0; i < TALK_TRAIT_TOPICS; i++, sh += 4) ts->like[i] = (uint8_t)((v >> sh) & 15u);
    for (i = 0; i < TALK_TRAIT_TOPICS; i++, sh += 4) ts->dislike[i] = (uint8_t)((v >> sh) & 15u);
    talk_trait_normalize(ts, LIFE_ADULT);       /* the stage is checked again when the set is used */
}

/* ---- the default: a pure function of the record -------------------------------- */
void talk_trait_default(const DoomoResident *r, TalkTraitSet *out)
{
    int stage = stage_ok(RES_STAGE(r)), minor = stage_minor(stage);
    int p[PERS_COUNT], t, k, best[2], score[2];
    uint32_t seed = gene_get(r->gene, GF_LOOK_SEED);
    TalkTraitSet o;
    for (k = 0; k < PERS_COUNT; k++) p[k] = RES_PERSONA(r, k) - 7;
    talk_traitset_clear(&o);
    best[0] = best[1] = 0;
    score[0] = score[1] = -1000;
    for (t = 1; t < TT_COUNT; t++) {
        int s = 0;
        if (minor && (ti[t].romance || t == TT_MEAN)) continue;
        for (k = 0; k < PERS_COUNT; k++) s += ti[t].w[k] * p[k];
        s += (int)(doomo_hash32(seed * 977u + (uint32_t)t * 7919u + (uint32_t)p[1] * 31u + (uint32_t)p[4]) % 17u) - 8;
        if (s > score[0]) { score[1] = score[0]; best[1] = best[0]; score[0] = s; best[0] = t; }
        else if (s > score[1]) { score[1] = s; best[1] = t; }
    }
    /* the best trait, then the best that does not clash with it (the loop keeps only two, so a clash
     * falls through to a scan) */
    talk_traitset_add(&o, best[0]);
    if (talk_traitset_add(&o, best[1]) != TTE_OK) {
        int bs = -1000, bt = 0;
        for (t = 1; t < TT_COUNT; t++) {
            int s = 0;
            if (t == best[0] || talk_trait_conflict(t, best[0]) || (minor && (ti[t].romance || t == TT_MEAN))) continue;
            for (k = 0; k < PERS_COUNT; k++) s += ti[t].w[k] * p[k];
            s += (int)(doomo_hash32(seed * 977u + (uint32_t)t * 7919u + (uint32_t)p[1] * 31u + (uint32_t)p[4]) % 17u) - 8;
            if (s > bs) { bs = s; bt = t; }
        }
        talk_traitset_add(&o, bt);
    }
    /* one liked and one disliked topic: what the first trait leans to, else a seeded root */
    {
        int like = ti[o.trait[0]].fav, dis = ti[o.trait[0]].dull;
        if (!like || !talk_trait_topic_ok(like, stage))
            like = 1 + (int)(doomo_hash32(seed + 11u) % 12u);              /* people .. lore */
        if (!dis || !talk_trait_topic_ok(dis, stage) || dis == like)
            dis = 1 + (int)(doomo_hash32(seed + 29u) % 12u);
        if (dis == like) dis = like % 12 + 1;
        talk_traitset_like(&o, like);
        talk_traitset_dislike(&o, dis);
    }
    *out = o;
}

/* ---- who is who ------------------------------------------------------------------ */
static const DoomoResident *res_of(int id, DoomoResident *buf)
{
    if (id == TALK_PREVIEW_ID) return talk_preview_resident();
    if (id >= 0 && id <= DOOMO_PLAYER_ID) {
        const DoomoResident *r = town_res(id);
        return r && r->status != RES_EMPTY ? r : NULL;
    }
    if (id > DOOMO_PLAYER_ID && id != TALK_PET_ID && family_resident(id, buf)) return buf;
    return NULL;
}

static int stage_of(int id, const DoomoResident *r)
{
    if (id == TALK_PREVIEW_ID) return talk_preview_stage();
    if (id >= 0 && id <= DOOMO_PLAYER_ID) return sim_life_stage(id);
    if (id > DOOMO_PLAYER_ID && id != TALK_PET_ID && family_find(id)) return family_find(id)->stage;
    return stage_ok(RES_STAGE(r));
}

/* ---- the explicit sets (the creator's choice; saved in the 'TALK' chunk) -------------- */
static struct {
    uint8_t  n, pad;
    uint16_t id[TALK_TRAIT_CUSTOM_MAX];
    uint32_t pk[TALK_TRAIT_CUSTOM_MAX];
} ovr;                                       /* 146 B, native only */

static TalkTraitSet pv_ts;                   /* the set of the character being previewed */
static const DoomoResident *pv_r;
static int pv_stage;

const DoomoResident *talk_preview_resident(void) { return pv_r; }
int talk_preview_stage(void) { return pv_stage; }

static int ovr_find(int id)
{
    int i;
    for (i = 0; i < ovr.n; i++) if (ovr.id[i] == id) return i;
    return -1;
}

int talk_trait_get(int id, TalkTraitSet *out)
{
    DoomoResident buf;
    const DoomoResident *r;
    int i;
    talk_traitset_clear(out);
    if (id == TALK_PREVIEW_ID) {
        if (!pv_r) return 0;
        *out = pv_ts;
        return 1;
    }
    r = res_of(id, &buf);
    if (!r) return 0;
    i = ovr_find(id);
    if (i >= 0) {
        talk_traitset_unpack(ovr.pk[i], out);
        talk_trait_normalize(out, stage_of(id, r));
        return 1;
    }
    talk_trait_default(r, out);
    return 1;
}

int talk_trait_set(int id, const TalkTraitSet *ts)
{
    DoomoResident buf;
    const DoomoResident *r = id == TALK_PREVIEW_ID ? NULL : res_of(id, &buf);
    TalkTraitSet o;
    int i;
    if (!r) return TTE_NOBODY;
    o = *ts;
    talk_trait_normalize(&o, stage_of(id, r));
    i = ovr_find(id);
    if (i < 0) {
        if (ovr.n >= TALK_TRAIT_CUSTOM_MAX) return TTE_FULL;
        i = ovr.n++;
        ovr.id[i] = (uint16_t)id;
    }
    ovr.pk[i] = talk_traitset_pack(&o);
    return TTE_OK;
}

int talk_trait_is_custom(int id) { return ovr_find(id) >= 0; }

void talk_trait_forget(int id)
{
    int i = ovr_find(id);
    if (i < 0) return;
    for (; i + 1 < ovr.n; i++) { ovr.id[i] = ovr.id[i + 1]; ovr.pk[i] = ovr.pk[i + 1]; }
    ovr.n--;
    ovr.id[ovr.n] = 0;
    ovr.pk[ovr.n] = 0;
}
void talk_trait_clear(int id) { talk_trait_forget(id); }
int talk_trait_custom_count(void) { return ovr.n; }
void talk_trait_reset(void) { memset(&ovr, 0, sizeof ovr); }

/* the save chunk's tail (talk_save.c): [u8 n] then n x {u16 id, u32 packed}; 6 B an entry */
uint32_t talk_trait_save_size(void) { return ovr.n ? 1u + 6u * ovr.n : 0u; }

uint32_t talk_trait_save_write(uint8_t *dst, uint32_t cap)
{
    uint32_t need = talk_trait_save_size();
    int i;
    if (!need || cap < need) return 0;
    dst[0] = ovr.n;
    for (i = 0; i < ovr.n; i++) {
        uint8_t *p = dst + 1 + 6 * i;
        p[0] = (uint8_t)ovr.id[i]; p[1] = (uint8_t)(ovr.id[i] >> 8);
        p[2] = (uint8_t)ovr.pk[i]; p[3] = (uint8_t)(ovr.pk[i] >> 8);
        p[4] = (uint8_t)(ovr.pk[i] >> 16); p[5] = (uint8_t)(ovr.pk[i] >> 24);
    }
    return need;
}

/* 1 if the tail is well formed (len exactly 1 + 6n, n <= max); entries of people who are gone,
 * duplicates and invalid sets are dropped, so a bad entry never reaches talk_trait_get */
int talk_trait_save_read(const uint8_t *src, uint32_t len)
{
    int i, n;
    talk_trait_reset();
    if (len == 0) return 1;
    n = src[0];
    if (n > TALK_TRAIT_CUSTOM_MAX || len != 1u + 6u * (uint32_t)n) return 0;
    for (i = 0; i < n; i++) {
        const uint8_t *p = src + 1 + 6 * i;
        int id = (int)p[0] | ((int)p[1] << 8);
        uint32_t pk = (uint32_t)p[2] | ((uint32_t)p[3] << 8) | ((uint32_t)p[4] << 16) | ((uint32_t)p[5] << 24);
        DoomoResident buf;
        const DoomoResident *r;
        TalkTraitSet ts;
        if (id == TALK_PET_ID || id == TALK_PREVIEW_ID || ovr_find(id) >= 0) continue;
        r = res_of(id, &buf);
        if (!r) continue;
        talk_traitset_unpack(pk, &ts);
        talk_trait_normalize(&ts, stage_of(id, r));
        ovr.id[ovr.n] = (uint16_t)id;
        ovr.pk[ovr.n++] = talk_traitset_pack(&ts);
    }
    return 1;
}

/* ---- the one-line read ------------------------------------------------------------------ */
const char *talk_trait_title(const DoomoResident *r)
{
    return doomo_persona_type_names[persona_type(r) % PERSONA_TYPE_COUNT];
}

typedef struct { char *p; int n, cap; } Buf;
static void put(Buf *b, const char *s)
{
    while (*s && b->n < b->cap - 1) b->p[b->n++] = *s++;
    b->p[b->n] = 0;
}

int talk_trait_read(const DoomoResident *r, const TalkTraitSet *tsin, char *out, int cap)
{
    TalkTraitSet ts;
    Buf b;
    int i, n, first;
    if (!out || cap < 1) return 0;
    out[0] = 0;
    if (!r) return 0;
    if (tsin) { ts = *tsin; talk_trait_normalize(&ts, RES_STAGE(r)); }
    else talk_trait_default(r, &ts);
    b.p = out; b.n = 0; b.cap = cap;
    put(&b, talk_trait_title(r));
    n = talk_traitset_count(&ts);
    if (n) {
        put(&b, ": ");
        for (i = 0; i < n; i++) {
            put(&b, i == 0 ? "" : i == n - 1 ? " and " : ", ");
            put(&b, talk_trait_adj(ts.trait[i]));
        }
    }
    first = 1;
    for (i = 0; i < TALK_TRAIT_TOPICS; i++) if (ts.like[i]) {
        put(&b, first ? ". Loves " : ", ");
        put(&b, talk_trait_topic_name(ts.like[i]));
        first = 0;
    }
    first = 1;
    for (i = 0; i < TALK_TRAIT_TOPICS; i++) if (ts.dislike[i]) {
        put(&b, first ? (b.n && strstr(b.p, "Loves") ? "; avoids " : ". Avoids ") : ", ");
        put(&b, talk_trait_topic_name(ts.dislike[i]));
        first = 0;
    }
    if (n || ts.like[0] || ts.dislike[0]) put(&b, ".");
    return b.n;
}

/* ---- what the traits do ------------------------------------------------------------------- */
static int warm(int a)
{
    switch (a) {
    case TA_THANK: case TA_APOLOGIZE: case TA_COMPLIMENT: case TA_PRAISE_OTHER: case TA_COMFORT: case TA_ENCOURAGE:
    case TA_SYMPATHIZE: case TA_OFFER: case TA_INVITE: case TA_PROMISE: case TA_GIVE: case TA_HUG: case TA_HIGH_FIVE:
        return 1;
    default: return 0;
    }
}
static int cool(int a)
{
    switch (a) {
    case TA_INSULT: case TA_CRITICIZE: case TA_TEASE: case TA_MOCK: case TA_COMPLAIN: case TA_BLAME: case TA_DISMISS:
    case TA_THREATEN: case TA_ACCUSE:
        return 1;
    default: return 0;
    }
}
static int joke(int a) { return a == TA_JOKE || a == TA_LAUGH || a == TA_TEASE; }
static int asking(int a)
{
    return a == TA_ASK_ABOUT || a == TA_ASK_OPINION || a == TA_ASK_FEELING || a == TA_ASK_FACT || a == TA_ASK_WHO ||
           a == TA_ASK_ADVICE;
}
static int pushy(int a) { return a == TA_INVITE || a == TA_ASK_FAVOR; }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int talk_trait_topic_lean(const TalkTraitSet *ts, int stage, int root)
{
    int i, v = 0;
    if (root < 1 || root > TALK_TRAIT_ROOT_MAX || !talk_trait_topic_ok(root, stage)) return 0;
    for (i = 0; i < TALK_TRAIT_MAX; i++) {
        int t = ts->trait[i];
        if (!t || talk_trait_inert(t, stage)) continue;
        if (ti[t].fav == root) v += 1;
        if (ti[t].dull == root) v -= 1;
    }
    v = clampi(v, -2, 2);
    for (i = 0; i < TALK_TRAIT_TOPICS; i++) {
        if (ts->like[i] == root) v += 2;
        if (ts->dislike[i] == root) v -= 2;
    }
    return clampi(v, -3, 3);
}

/* one trait's act lean, added into a, d (accept, deny) and m (mood) */
static void trait_act(int t, int act, int *a, int *d, int *m)
{
    switch (t) {
    case TT_HOTHEADED:
        if (cool(act)) { *d += 2; *m -= 1; } else if (act == TA_ASK_FAVOR) *d += 1;
        break;
    case TT_ROMANTIC:
        if (TA_IS_ROMANCE(act)) *a += 2; else if (act == TA_COMPLIMENT) *a += 1;
        break;
    case TT_GOOFBALL:
        if (joke(act)) { *a += 2; *m += 1; }
        break;
    case TT_SNOB:
        if (pushy(act) || act == TA_GIVE) *d += 1; else if (act == TA_COMPLIMENT) *a += 1; else if (cool(act)) *d += 1;
        break;
    case TT_NEAT:
        if (act == TA_ASK_FAVOR || act == TA_PROMISE) *a += 1;
        break;
    case TT_SLOB:
        if (act == TA_INVITE || act == TA_OFFER) *a += 1; else if (act == TA_ASK_FAVOR) *d += 1;
        break;
    case TT_BOOKWORM:
        if (act == TA_ASK_FACT || act == TA_ASK_ADVICE || act == TA_ASK_OPINION) *a += 2; else if (act == TA_INVITE) *d += 1;
        break;
    case TT_CHEERFUL:
        if (warm(act)) { *a += 1; *m += 1; } else if (joke(act)) *a += 1;
        break;
    case TT_GLOOMY:
        if (act == TA_COMFORT || act == TA_SYMPATHIZE) *a += 1; else if (act == TA_JOKE) { *d += 1; *m -= 1; }
        break;
    case TT_JEALOUS:
        if (act == TA_PRAISE_OTHER || act == TA_GOSSIP) { *d += 1; *m -= 1; }
        break;
    case TT_LOYAL:
        if (act == TA_DEFEND || act == TA_TAKE_SIDE) *a += 2; else if (act == TA_PROMISE) *a += 1; else if (act == TA_GOSSIP) *d += 1;
        break;
    case TT_MEAN:
        if (act == TA_GOSSIP) *a += 2; else if (warm(act) && act != TA_GIVE) *d += 1; else if (cool(act)) *a += 1;
        break;
    case TT_KIND:
        if (warm(act)) { *a += 2; *m += 1; } else if (act == TA_ASK_FAVOR) *a += 2;
        break;
    case TT_LAZY:
        if (pushy(act)) *d += 2; else if (act == TA_OFFER) *a += 1;
        break;
    case TT_AMBITIOUS:
        if (act == TA_ENCOURAGE) *a += 2; else if (act == TA_ASK_ADVICE) *a += 1; else if (act == TA_INVITE) *d += 1;
        break;
    case TT_PARANOID:
        if (act == TA_ASK_ABOUT || act == TA_ASK_FACT || act == TA_INVITE || act == TA_GIVE) *d += 1;
        else if (act == TA_GOSSIP) *a += 1;
        break;
    case TT_SELF_ASSURED:
        if (act == TA_COMPLIMENT) { *a += 1; *m += 1; } else if (cool(act)) *a += 1;
        break;
    case TT_SHY:
        if (act == TA_INVITE) *d += 2; else if (act == TA_HUG) *d += 1;
        break;
    case TT_OUTGOING:
        if (act == TA_INVITE) *a += 2; else if (act == TA_GREET || act == TA_GOSSIP || act == TA_HUG) *a += 1;
        break;
    case TT_CREATIVE:
        if (act == TA_ASK_OPINION || act == TA_INVITE) *a += 1;
        break;
    case TT_GLUTTON:
        if (act == TA_OFFER || act == TA_GIVE) { *a += 2; *m += 1; }
        break;
    case TT_MORBID:
        if (act == TA_TELL || act == TA_ASK_ABOUT) *a += 1;
        break;
    case TT_CURIOUS:
        if (asking(act) || act == TA_TELL) *a += 1;
        break;
    case TT_HONEST:
        if (act == TA_DISAGREE || act == TA_ASK_OPINION) *a += 1;
        break;
    default: break;
    }
}

TalkTraitBias talk_trait_bias(const TalkTraitSet *ts, int stage, int act, int topic_root, int minor_room)
{
    TalkTraitBias b;
    int i, a = 0, d = 0, m = 0;
    memset(&b, 0, sizeof b);
    if (!ts || act < TA_NONE || act >= TA_COUNT || TA_IS_SAFETY(act)) return b;
    stage = stage_ok(stage);
    for (i = 0; i < TALK_TRAIT_MAX; i++) {
        int t = ts->trait[i];
        if (t <= TT_NONE || t >= TT_COUNT || talk_trait_inert(t, stage)) continue;
        trait_act(t, act, &a, &d, &m);
    }
    if (minor_room || stage_minor(stage)) {          /* a child is involved: nobody gets sharper */
        d = 0;
        if (m < 0) m = 0;
        if (cool(act)) a = a > 0 ? a : 0;
    }
    if (minor_room && TA_IS_ROMANCE(act)) a = d = m = 0;   /* (the gate never lets one through; belt and braces) */
    b.accept = (int8_t)clampi(a, 0, 4);
    b.deny = (int8_t)clampi(d, 0, 4);
    b.mood = (int8_t)clampi(m, -3, 3);
    b.stance = (int8_t)talk_trait_topic_lean(ts, stage, topic_root);
    return b;
}

TalkTraitBias talk_trait_bias_id(int id, int act, int topic_root, int minor_room)
{
    DoomoResident buf;
    const DoomoResident *r = res_of(id, &buf);
    TalkTraitSet ts;
    TalkTraitBias none;
    memset(&none, 0, sizeof none);
    if (!r || !talk_trait_get(id, &ts)) return none;
    return talk_trait_bias(&ts, stage_of(id, r), act, topic_root, minor_room);
}

int talk_trait_take(const TalkTraitBias *b) { return b->accept - b->deny; }

/* ---- the live preview --------------------------------------------------------------------- */
#define PV_SCAN 512

/* a concept of root `root` (0: any talkable root) that may be said in this room, scanned from a seeded start */
static uint16_t pv_topic(const TalkConv *c, int root, int stage, uint32_t seed)
{
    int n = talk_db_nconcepts(), i;
    uint32_t start;
    if (n <= 0) return 0;
    start = doomo_hash32(seed ^ 0x51ED270Bu) % (uint32_t)n;
    for (i = 0; i < PV_SCAN && i < n; i++) {
        uint16_t id = (uint16_t)(1u + (start + (uint32_t)i) % (uint32_t)n);
        TalkConcept tc;
        uint8_t parent = 0, flags = 0;
        int rt;
        if (!talk_db_concept(id, &tc) || !talk_db_cat(tc.cat, &parent, &flags) || !(flags & TCAT_TALKABLE)) continue;
        rt = talk_db_cat_root(tc.cat);
        if (rt < 1 || rt > TALK_TRAIT_ROOT_MAX || !talk_trait_topic_ok(rt, stage)) continue;
        if (root && rt != root) continue;
        if (tc.flags & TCF_GRIEF) continue;
        if (talk_gen_concept_ok(id, &c->aud)) return id;
    }
    return 0;
}

int talk_preview(const DoomoResident *r, const TalkTraitSet *tsin, int kind, uint32_t seed, char *out, int cap)
{
    static const uint8_t asks[] = { TA_INVITE, TA_ASK_FAVOR, TA_GIVE };
    TalkConv c;
    TalkLine line;
    TalkTraitSet ts;
    int ids[1], stage, act = TA_GREET, k, bestd = -100, root = 0, to;
    uint16_t topic = 0;
    if (!out || cap < 1) return 0;
    out[0] = 0;
    if (!r || kind < 0 || kind >= TP_KINDS || !talk_gen_ready()) return 0;
    stage = stage_ok(RES_STAGE(r));
    if (tsin) { ts = *tsin; talk_trait_normalize(&ts, stage); }
    else talk_trait_default(r, &ts);
    pv_r = r; pv_ts = ts; pv_stage = stage;
    ids[0] = TALK_PREVIEW_ID;
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 1, doomo_hash32(seed + (uint32_t)kind * 0x9E3779B9u));
    c.plain = 1;
    to = -1;
    if (kind == TP_ANSWER) {
        act = TA_ASK_OPINION;
        root = ts.like[0] ? ts.like[0] : 0;
        topic = pv_topic(&c, root, stage, seed);
        if (!topic) topic = pv_topic(&c, 0, stage, seed);
    } else if (kind == TP_REFUSAL) {
        act = TA_INVITE;
        for (k = 0; k < (int)sizeof asks; k++) {                     /* the ask this person turns down hardest */
            TalkTraitBias b = talk_trait_bias(&ts, stage, asks[k], 0, 0);
            int d = b.deny - b.accept;
            if (d > bestd) { bestd = d; act = asks[k]; }
        }
        root = ts.dislike[0] ? ts.dislike[0] : 0;
        if (root) topic = pv_topic(&c, root, stage, seed);
        if (act == TA_GIVE) act = TA_INVITE;
    }
    if (talk_gen_line(&c, 0, to, act, topic, &line, NULL) || line.text[0]) {
        const char *s = line.text;
        int n = 0;
        while (*s && n < cap - 1) out[n++] = *s++;
        out[n] = 0;
    }
    pv_r = NULL;
    return (int)strlen(out);
}
