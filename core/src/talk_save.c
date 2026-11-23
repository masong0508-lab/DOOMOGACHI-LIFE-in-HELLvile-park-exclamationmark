/* talk_save.c - talk memory and its save chunk 'TALK' (owner: TK, T6;
 * docs/TALK.md 6e; design 6.8, 5.5 "Talk memory", 3.6 "Memory").
 *
 * The table: at most TALK_MEM_MAX platform-sized entries of 12 B, the most
 * recently used first. Older completed entries are evicted before open
 * commitments. Its 4-byte header and entries occupy TALK_MEM_WORST bytes,
 * and the chunk is the same bytes written little endian:
 *   [u8 payload version][u8 count][u16 day_of_last] then count x
 *   {u16 id; u16 day; u16 topic_a; u16 topic_b; u8 flags; u8 likes_known;
 *    u16 spare}
 * Optional traits follow the table. Payload v5 ends with the fixed-size
 * same-day invitation ledger after any traits.
 *
 * "Refused today" (K-5) is cleared when the town day is no longer
 * day_of_last: every public call checks first, so a save loaded on a later
 * day forgets it too. The chunk callbacks never roll the day (save.c may
 * produce a chunk twice and the bytes must not change in between).
 *
 * The flags of a running conversation wait in the high nibble (never
 * saved) until talk_mem_end(): a "you were nice to me last time" callback
 * can then never speak of the conversation it is said in.
 *
 * WHERE IT IS REGISTERED. The save registry is link-time (save_ext.h), so
 * the runtime bit PLATF_TALK cannot choose: the descriptor is compiled
 * only with -DDOOMO_HAS_TALK (core/Makefile: the native host; a console
 * port adds it when it gets Talk). Everywhere else doomo_save_ext_talk
 * stays the weak NULL, nothing references this file and --gc-sections
 * drops it whole on a port built without Talk.
 *
 * Loading: no chunk (an old save) or a chunk read() refuses (unknown
 * version, truncated, too long, a count over 24) leaves the table empty.
 * The registry reads LIFE (the family pool) before TALK, so "who is still
 * here" is asked of the loaded town: an entry of someone gone, of the pet,
 * or a second entry of the same id is dropped. Topic ids are kept as they
 * are (12 bits): a retired id is skipped when a conversation begins,
 * because only the open database knows it.
 *
 * No stdio, no malloc, no floats.
 */
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/talk_traits.h"
#include "doomo/family.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/sim.h"

DOOMO_STATIC_ASSERT(sizeof(TalkMem) == TALK_MEM_ENTRY, talk_mem_entry_is_12_bytes);

#define TMF_LAST    (TMF_COMPLIMENTED | TMF_INSULTED | TMF_PROMISED)
#define TMF_SAVED   (TMF_LAST | TMF_REFUSED_TODAY)
#define PEND_SHIFT  4                       /* the running conversation's TMF_LAST bits */
#define LIKES_MASK  0x1Fu
#define MEMORY_MASK (LIKES_MASK | TALK_SHARED_ALL)
#define TOPIC_MASK  0x0FFFu

static struct {
    uint8_t  count;
    uint8_t  pad;
    uint16_t day;                           /* day_of_last: the town day the flags are of */
    TalkMem  e[TALK_MEM_MAX];
} mem;                                      /* 292 B */

DOOMO_STATIC_ASSERT(sizeof(mem) == TALK_MEM_WORST, talk_mem_table_is_the_worst_case);
DOOMO_STATIC_ASSERT(TALK_SAVE_WORST <= SAVE_EXT_MAX_TALK, talk_chunk_fits_budget);

#define TMEM_SECRETS 0x3Cu                  /* TalkMem.spare bits 2-5: the secrets told today (TDEP_MEM_SECRET) */
#define TMEM_DEPTH   0x3Fu                  /* bits 0-1 the level reached in the last talk, 2-5 the secrets told today */
/* TQ-QUESTS: the request this person has put to the player, in the 10 high bits of `spare` (no byte more in the chunk):
 * bits 6-9 the kind (TQK_*), 10-11 the state (TQS_*), 12 the bonus (a haggle that won), 13-15 the days left (an open
 * request: TQ_DAYS at most; a settled one: the cooldown, 0 = ran out unanswered). An old chunk has 0 there: none. */
#define TMEM_QUEST   0xFFC0u
#define TMEM_SPARE   0xFFFFu
#define TQB_KIND(sp)  (((sp) >> 6) & 15u)
#define TQB_STATE(sp) (((sp) >> 10) & 3u)
#define TQB_BONUS(sp) (((sp) >> 12) & 1u)
#define TQB_DAYS(sp)  (((sp) >> 13) & 7u)
#define TQB_PACK(k, st, bo, d) ((uint16_t)((((unsigned)(k) & 15u) << 6) | (((unsigned)(st) & 3u) << 10) | \
                                           (((unsigned)(bo) & 1u) << 12) | (((unsigned)(d) & 7u) << 13)))

static uint16_t today(void) { return (uint16_t)doomo_town.day; }

/* somebody talk memory may be about: a resident or the hero who is on the
 * island, or a member of the family pool */
static int person_ok(int id)
{
    if (id >= 0 && id <= DOOMO_PLAYER_ID) {
        const DoomoResident *r = town_res(id);
        return r && r->status != RES_EMPTY;
    }
    return id > DOOMO_PLAYER_ID && id < TALK_PET_ID && family_find(id) != NULL;
}

/* the town day changed: nobody has "refused today" any more */
static void roll(void)
{
    int i;
    unsigned gone;
    if (mem.day == today()) return;
    gone = (uint16_t)(today() - mem.day);
    if (gone > 7u || gone == 0u) gone = 7u;                                      /* a save from long ago: every wait has run out */
    for (i = 0; i < mem.count; i++) {
        unsigned sp = mem.e[i].spare, k = TQB_KIND(sp), st = TQB_STATE(sp), d = TQB_DAYS(sp), bo = TQB_BONUS(sp);
        mem.e[i].flags = (uint8_t)(mem.e[i].flags & ~TMF_REFUSED_TODAY);
        sp &= ~(unsigned)TMEM_SECRETS;                                              /* TM2d: a new day, a new secret may be told */
        if (k && st == TQS_SETTLED && d > 0)                                        /* TQ: the cooldown after a hand-in or a no */
            sp = d > gone ? (sp & ~(unsigned)TMEM_QUEST) | TQB_PACK(k, st, bo, d - gone) : (sp & ~(unsigned)TMEM_QUEST);
        else if (k && (st == TQS_ACTIVE || st == TQS_READY))                        /* TQ: an open request waits TQ_DAYS days */
            sp = (sp & ~(unsigned)TMEM_QUEST) | (d > gone ? TQB_PACK(k, st, bo, d - gone) : TQB_PACK(k, TQS_SETTLED, 0, 0));
        mem.e[i].spare = (uint16_t)sp;                                              /* ran out: settled with 0 days = "never mind" */
    }
    mem.day = today();
}

static int index_of(int id)
{
    int i;
    for (i = 0; i < mem.count; i++) if (mem.e[i].id == id) return i;
    return -1;
}

static void drop(int i)
{
    memmove(&mem.e[i], &mem.e[i + 1], (size_t)(mem.count - 1 - i) * sizeof(TalkMem));
    mem.count--;
    memset(&mem.e[mem.count], 0, sizeof(TalkMem));
}

/* the entry of id if its person is still here (a stale one is dropped) */
static TalkMem *lookup(int id)
{
    int i = index_of(id);
    if (i < 0) return NULL;
    if (!person_ok(id)) { drop(i); return NULL; }
    return &mem.e[i];
}

/* find or make, move to the front */
static TalkMem *touch(int id)
{
    TalkMem t;
    int i;
    if (!person_ok(id)) { if ((i = index_of(id)) >= 0) drop(i); return NULL; }
    roll();
    i = index_of(id);
    if (i < 0) {
        if (mem.count == TALK_MEM_MAX) {                     /* LRU: the last one goes, unless it holds a request (TQ) */
            int v, fallback = -1;
            /* Keep commitments, then favor memories of actual shared
             * experiences over routine greetings. Search from oldest to
             * newest, so the oldest casual record goes first. */
            for (v = mem.count - 1; v >= 0; v--) {
                unsigned sp = mem.e[v].spare;
                if (TQB_KIND(sp) &&
                    (TQB_STATE(sp) == TQS_ACTIVE || TQB_STATE(sp) == TQS_READY))
                    continue;
                if (fallback < 0) fallback = v;
                if (!(mem.e[v].likes_known & TALK_SHARED_ALL) &&
                    !(mem.e[v].flags & TMF_LAST)) break;
            }
            if (v < 0) v = fallback;
            /* Every slot may hold an open commitment. Never silently
             * discard one just to remember another casual chat. */
            if (v < 0) return NULL;
            drop(v);
        }
        i = mem.count++;
        memset(&mem.e[i], 0, sizeof(TalkMem));
        mem.e[i].id = (uint16_t)id;
    }
    t = mem.e[i];
    memmove(&mem.e[1], &mem.e[0], (size_t)i * sizeof(TalkMem));
    t.day = today();
    mem.e[0] = t;
    return &mem.e[0];
}

int talk_mem_count(void) { return mem.count; }
int talk_mem_capacity(void) { return TALK_MEM_MAX; }
int talk_mem_pinned_count(void)
{
    int i, count = 0;
    roll();
    for (i = 0; i < mem.count; i++) {
        unsigned sp = mem.e[i].spare;
        if (TQB_KIND(sp) &&
            (TQB_STATE(sp) == TQS_ACTIVE || TQB_STATE(sp) == TQS_READY))
            count++;
    }
    return count;
}

const TalkMem *talk_mem_at(int i) { return i >= 0 && i < mem.count ? &mem.e[i] : NULL; }

const TalkMem *talk_mem_find(int id)
{
    roll();
    return lookup(id);
}

int talk_mem_touch(int id) { return touch(id) != NULL; }

void talk_mem_note_topic(int id, uint16_t topic)
{
    TalkMem *e;
    if (topic == TC_NONE || topic > TOPIC_MASK || !(e = touch(id)) || e->topic_a == topic) return;
    e->topic_b = e->topic_a;
    e->topic_a = topic;
}

void talk_mem_set_flags(int id, unsigned flags)
{
    TalkMem *e = touch(id);
    if (!e) return;
    e->flags = (uint8_t)(e->flags | (flags & TMF_REFUSED_TODAY) | ((flags & TMF_LAST) << PEND_SHIFT));
}

void talk_mem_clear_flags(int id, unsigned flags)
{
    TalkMem *e;
    roll();
    if (!(e = lookup(id))) return;
    flags &= TMF_SAVED;
    e->flags = (uint8_t)(e->flags & ~(flags | ((flags & TMF_LAST) << PEND_SHIFT)));
}

void talk_mem_note_shared(int id, unsigned event_flags)
{
    TalkMem *e;
    /* Only concrete completed player actions call this. Zero or unknown
     * flags do not waste an LRU entry or disturb existing likes. */
    event_flags &= TALK_SHARED_ALL;
    if (!event_flags || !(e = touch(id))) return;
    e->likes_known = (uint8_t)(e->likes_known | event_flags);
}

int talk_mem_shared(int id, unsigned event_flags)
{
    const TalkMem *e;
    if (!event_flags || (event_flags & ~TALK_SHARED_ALL)) return 0;
    e = talk_mem_find(id);
    return e && (e->likes_known & event_flags) == event_flags;
}

void talk_mem_know_like(int id, int which)
{
    TalkMem *e;
    if (which < 0 || which > 4 || !(e = touch(id))) return;
    e->likes_known = (uint8_t)(e->likes_known | (1u << which));
}

int talk_mem_like_known(int id, int which)
{
    const TalkMem *e = talk_mem_find(id);
    return e && which >= 0 && which <= 4 && ((e->likes_known >> which) & 1);
}

/* TQ-QUESTS: the request id put to the player. get: 1 when there is one (kind, TQS_*, days, bonus; any pointer may be
 * NULL); set: touches id and writes it (kind 0 clears it); 0 for nobody. */
int talk_mem_quest_get(int id, int *kind, int *state, int *days, int *bonus)
{
    const TalkMem *e = talk_mem_find(id);
    unsigned sp;
    if (!e || !TQB_KIND(e->spare)) return 0;
    sp = e->spare;
    if (kind) *kind = (int)TQB_KIND(sp);
    if (state) *state = (int)TQB_STATE(sp);
    if (days) *days = (int)TQB_DAYS(sp);
    if (bonus) *bonus = (int)TQB_BONUS(sp);
    return 1;
}

int talk_mem_quest_set(int id, int kind, int state, int days, int bonus)
{
    TalkMem *e;
    if (kind < 0 || kind >= TQK_COUNT || state < 0 || state > TQS_SETTLED || days < 0 || days > 7) return 0;
    if (!(e = touch(id))) return 0;
    e->spare = (uint16_t)((e->spare & TMEM_DEPTH) | (kind ? TQB_PACK(kind, state, bonus, days) : 0u));
    return 1;
}

int talk_mem_refused_today(int id)
{
    const TalkMem *e = talk_mem_find(id);
    return e && (e->flags & TMF_REFUSED_TODAY) != 0;
}

/* a remembered topic the open database still has (0: a retired id, or no database) */
static uint16_t live_topic(uint16_t t)
{
    TalkConcept tc;
    return t && talk_db_concept(t, &tc) ? t : TC_NONE;
}

void talk_mem_begin(TalkConv *c)
{
    int i, k;
    if (!c) return;
    roll();
    /* flags left pending by a conversation that never ended are not "last time" */
    for (i = 0; i < mem.count; i++) mem.e[i].flags = (uint8_t)(mem.e[i].flags & TMF_SAVED);
    for (k = 0; k < c->n && k < TALK_PARTS_MAX; k++) {
        const TalkMem *e = lookup(c->id[k]);
        c->mem_topic[k] = TC_NONE;
        c->mem_flags[k] = 0;
        if (!e) continue;
        c->mem_topic[k] = live_topic(e->topic_a);
        if (!c->mem_topic[k]) c->mem_topic[k] = live_topic(e->topic_b);
        c->mem_flags[k] = (uint8_t)(e->flags & TMF_LAST);
        c->mem_dep[k] = (uint8_t)(e->spare & TMEM_SECRETS);                /* TM2d: the secrets told today; the level is this talk's own */
        if (TDEP_MEM_LEVEL(e->spare) > 1u) {                                 /* the next talk starts one level lower on the last topic */
            c->dep[k] = (uint8_t)(TDEP_MEM_LEVEL(e->spare) - 1u);
            c->dep_tw[k] = c->mem_topic[k];
        }
        if (e->flags & TMF_REFUSED_TODAY) c->refused = (uint8_t)(c->refused | (1u << k));
    }
}

void talk_mem_end(const TalkConv *c)
{
    int k;
    if (!c) return;
    roll();
    for (k = 0; k < c->n && k < TALK_PARTS_MAX; k++) {
        int id = c->id[k];
        TalkMem *e = lookup(id);
        unsigned pend = e ? (unsigned)(e->flags >> PEND_SHIFT) & TMF_LAST : 0u;
        unsigned no = c->ended || ((c->refused >> k) & 1);     /* K-5; K-7 locks the room for the day */
        if (!pend && !no && !((c->spoke >> k) & 1)) continue;   /* took no part */
        if (!(e = touch(id))) continue;
        e->flags = (uint8_t)((e->flags & (TMF_PROMISED | TMF_REFUSED_TODAY)) | pend | (no ? TMF_REFUSED_TODAY : 0u));
        e->spare = (uint16_t)((e->spare & TMEM_QUEST) | TDEP_MEM_LEVEL(c->mem_dep[k]) | (c->mem_dep[k] & TMEM_SECRETS) |
                              (e->spare & TMEM_SECRETS));   /* TM2d; the request bits stay (TQ) */
    }
}

void talk_mem_forget(int id)
{
    int i = index_of(id);
    if (i >= 0) drop(i);
    talk_trait_forget(id);                    /* TM1: an explicit trait set goes with the person */
}

/* TQ: the pace of requests lives in the spare byte of the table's header (no .bss of its own); not saved */
unsigned talk_mem_pace(void) { return mem.pad; }
void talk_mem_set_pace(unsigned v) { mem.pad = (uint8_t)v; }

void talk_mem_reset(void) { memset(&mem, 0, sizeof mem); talk_trait_reset(); talk_quest_reset(); }   /* TQ: the pace too */

/* ---- the chunk (only where the build has Talk) ----------------------------- */
#ifdef DOOMO_HAS_TALK
static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static unsigned get16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }

static void talk_save_reset(void)
{
    talk_mem_reset();
    talk_outcome_reset();
}

/* nothing while the table is empty: like PLAY and PROF, an empty table is
 * a chunk of 0 bytes */
static uint32_t talk_size(void)
{
    uint32_t tails = talk_trait_save_size() + talk_outcome_meet_save(NULL);
    return mem.count || tails ? (uint32_t)(TALK_MEM_HDR + mem.count * TALK_MEM_ENTRY) + tails : 0u;
}

static uint32_t talk_write(uint8_t *dst, uint32_t cap)
{
    uint32_t n = talk_size(), tr = talk_trait_save_size(), at;
    int i;
    if (!n || cap < n) return 0;
    /* New payload versions distinguish actual event flags from stray bits
     * in legacy v1/v2 likes_known. Outer save chunk version stays 1. */
    dst[0] = talk_outcome_meet_save(NULL) ? TALK_MEM_VERSION_MEETUPS :
             tr ? TALK_MEM_VERSION_EVENTS_TRAITS : TALK_MEM_VERSION_EVENTS;
    dst[1] = mem.count;
    put16(dst + 2, mem.day);
    for (i = 0; i < mem.count; i++) {
        const TalkMem *e = &mem.e[i];
        uint8_t *p = dst + TALK_MEM_HDR + i * TALK_MEM_ENTRY;
        put16(p, e->id);
        put16(p + 2, e->day);
        put16(p + 4, e->topic_a);
        put16(p + 6, e->topic_b);
        p[8] = (uint8_t)(e->flags & TMF_SAVED);
        p[9] = e->likes_known;
        put16(p + 10, e->spare & TMEM_SPARE);                  /* TM2d: depth reached, secrets told today (an old chunk read 0) */
    }
    at = (uint32_t)(TALK_MEM_HDR + mem.count * TALK_MEM_ENTRY);
    if (tr) talk_trait_save_write(dst + at, tr);
    if (dst[0] == TALK_MEM_VERSION_MEETUPS) talk_outcome_meet_save(dst + at + tr);
    return n;
}

static int talk_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    uint32_t at, meets, traits;
    int i, n, payload;
    talk_save_reset();
    if (version != TALK_MEM_VERSION) return 0;               /* a later format: ignored */
    if (len == 0) return 1;                                   /* the empty chunk */
    if (len < TALK_MEM_HDR || len > TALK_SAVE_WORST) return 0;
    payload = src[0];
    if (payload < TALK_MEM_VERSION || payload > TALK_MEM_VERSION_MEETUPS) return 0;
    n = src[1];
    at = (uint32_t)(TALK_MEM_HDR + n * TALK_MEM_ENTRY);
    meets = payload == TALK_MEM_VERSION_MEETUPS ? TALK_MEET_SAVE_BYTES : 0u;
    if (n > TALK_MEM_MAX || len < at + meets) return 0;
    traits = len - at - meets;
    if ((payload == TALK_MEM_VERSION || payload == TALK_MEM_VERSION_EVENTS) && traits) return 0;
    if ((payload == TALK_MEM_VERSION_TRAITS || payload == TALK_MEM_VERSION_EVENTS_TRAITS) && !traits) return 0;
    mem.day = (uint16_t)get16(src + 2);
    for (i = 0; i < n; i++) {
        const uint8_t *p = src + TALK_MEM_HDR + i * TALK_MEM_ENTRY;
        int id = (int)get16(p);
        TalkMem *e;
        if (!person_ok(id) || index_of(id) >= 0) continue;   /* gone, the pet, nobody, or twice */
        e = &mem.e[mem.count++];
        e->id = (uint16_t)id;
        e->day = (uint16_t)get16(p + 2);
        e->topic_a = (uint16_t)(get16(p + 4) & TOPIC_MASK);
        e->topic_b = (uint16_t)(get16(p + 6) & TOPIC_MASK);
        e->flags = (uint8_t)(p[8] & TMF_SAVED);
        /* Older payloads never wrote event bits; strip high noise rather
         * than inventing memories of invitations/playing/gifts. */
        e->likes_known = (uint8_t)(p[9] &
            (payload >= TALK_MEM_VERSION_EVENTS ? MEMORY_MASK : LIKES_MASK));
        e->spare = (uint16_t)(get16(p + 10) & TMEM_SPARE);
        if (TQB_KIND(e->spare) >= TQK_COUNT || (TQB_KIND(e->spare) == 0 && (e->spare & TMEM_QUEST)) ||
            (TQB_KIND(e->spare) && TQB_STATE(e->spare) == TQS_NONE))
            e->spare = (uint16_t)(e->spare & TMEM_DEPTH);                     /* a bad request field: none */
    }
    if ((traits && !talk_trait_save_read(src + at, traits)) ||
        (meets && !talk_outcome_meet_load(src + at + traits))) {
        talk_save_reset();
        return 0;
    }
    return 1;
}

static const DoomoSaveExt talk_ext = {
    SAVE_EXT_TAG_TALK, TALK_MEM_VERSION, talk_size, talk_write, talk_read, talk_save_reset, TALK_SAVE_WORST
};
const DoomoSaveExt *const doomo_save_ext_talk = &talk_ext;

/* TVIS v2, newest first: [u16 visits][u8 elders], followed by five bytes
 * per outing and four per elder. Each outing holds two 10-bit IDs, a
 * four-bit venue and a 16-bit date. V1 visits had no recorded date. */
#define TVIS_HDR 3u
#define TVIS_REC 5u
#define TVIS_ELDER_REC 4u
#define TVIS_V1_REC 3u
#define TVIS_V1_MAX 576u
#define TVIS_PEOPLE 0xFFFFFu
static struct {
    uint16_t count;
    uint8_t elders;
    uint8_t rows[SAVE_TVIS_VISITS][TVIS_REC];
    uint32_t elder[SAVE_TVIS_ELDERS];
} visits;

DOOMO_STATIC_ASSERT(SAVE_EXT_MAX_TVIS <= SAVE_TVIS_WIRE_MAX && SAVE_TVIS_WIRE_MAX <= DOOMO_SAVE_EXT_MAX,
                    visit_archive_fits);
DOOMO_STATIC_ASSERT(ACT_COUNT <= 16 && DOOMO_PLAYER_ID + HOUSEHOLD_CAP_MAX < NEWS_NONE, visit_fields_fit);

static uint32_t visit_key(const uint8_t *p)
{
    return get16(p) | ((uint32_t)p[2] << 16);
}

static uint32_t visit_day(const uint8_t *p)
{
    return get16(p + 3);
}

static uint32_t visit_today(void)
{
    return DOOMO_MIN(doomo_town.day, TALK_VISIT_UNDATED - 1);
}

static int visit_guest_ok(int guest)
{
    return guest >= 0 && guest < DOOMO_MAX_RESIDENTS;
}

static int visit_valid(uint32_t key)
{
    int guest = (int)(key & 1023u), avatar = (int)((key >> 10) & 1023u);
    return visit_guest_ok(guest) && guest != avatar && person_ok(guest) && person_ok(avatar) &&
           talk_act_invite_ok(TA_INVITE, (int)(key >> 20), 0, 12);
}

static int elder_eligible(int guest)
{
    uint32_t seed;
    if (!visit_guest_ok(guest) || town_res(guest)->status != RES_ALIVE || sim_life_stage(guest) != LIFE_ELDER)
        return 0;
    seed = doomo_town.seed ^ ((uint32_t)guest * 0x9E3779B1u);
    return doomo_hash32(seed ^ 0xEF34E10Du) % 6u == 0;
}

/* Track a small subset of elders, starting with their first shared outing
 * or memory conversation. No town-wide daily aging pass is needed. */
static int elder_index(int guest)
{
    int i;
    for (i = 0; i < visits.elders; i++) if ((visits.elder[i] & 1023u) == (unsigned)guest) return i;
    return -1;
}

static int elder_observe(int guest)
{
    int i;
    if (!opt_get(OPT_MEMORY_LAPSES) || !elder_eligible(guest)) return -1;
    i = elder_index(guest);
    if (i >= 0 || visits.elders == SAVE_TVIS_ELDERS) return i;
    i = visits.elders++;
    visits.elder[i] = (visit_today() << 10) | (uint32_t)guest;
    return i;
}

int talk_visit_recall(int guest, int cue)
{
    uint32_t elapsed, chance, roll, day, onset;
    int i = elder_observe(guest);
    if (i < 0) return TALK_RECALL_CLEAR;
    day = visit_today();
    onset = visits.elder[i] >> 10;
    elapsed = day >= onset ? day - onset : 0;
    chance = elapsed < 28u ? 0u : elapsed < 84u ? 32u : elapsed < 168u ? 80u : 136u;
    if (cue) chance /= 2u;
    /* One consistent recall day per elder, including after loading. A new
     * day can be easier or harder. The journal is never modified by recall. */
    roll = doomo_hash32(doomo_town.seed ^ (day * 0x85EBCA6Bu) ^
                        ((uint32_t)guest * 0xC2B2AE35u)) & 255u;
    return roll < chance / 3u ? TALK_RECALL_LAPSE : roll < chance ? TALK_RECALL_HAZY : TALK_RECALL_CLEAR;
}

static int visit_duplicate(uint32_t key, uint32_t day)
{
    int i;
    for (i = visits.count - 1; i >= 0 && visit_day(visits.rows[i]) == day; i--)
        if (visit_key(visits.rows[i]) == key) return 1;
    return 0;
}

static void visit_pack(uint8_t *p, uint32_t key, uint32_t day)
{
    put16(p, key & 0xFFFFu);
    p[2] = (uint8_t)(key >> 16);
    put16(p + 3, day);
}

static int visit_evict(void)
{
    int i, oldest = visits.count - 1, crowded = -1;
    /* Keep recent entries in full. In the oldest quarter, thin dates
     * that already have a monthly anchor, preferring repeat outings. */
    for (i = oldest - 1; i >= (int)(SAVE_TVIS_VISITS * 3u / 4u); i--) {
        uint32_t day = visit_day(visits.rows[i]), older = visit_day(visits.rows[i + 1]);
        if (day == TALK_VISIT_UNDATED || older == TALK_VISIT_UNDATED ||
            day / SIM_MONTH_DAYS != older / SIM_MONTH_DAYS) continue;
        if (visit_key(visits.rows[i]) == visit_key(visits.rows[i + 1])) return i;
        if (crowded < 0) crowded = i;
    }
    return crowded >= 0 ? crowded : oldest;
}

static void visit_push(uint32_t key, uint32_t day)
{
    int i;
    /* Same-day trips to the same venue use one record. Most days stop
     * after a handful of rows, with no scan of the older history. */
    for (i = 0; i < visits.count && visit_day(visits.rows[i]) == day; ++i)
        if (visit_key(visits.rows[i]) == key) break;
    if (i == visits.count || visit_day(visits.rows[i]) != day) {
        if (visits.count < SAVE_TVIS_VISITS) i = visits.count++;
        else i = visit_evict();
    }
    memmove(visits.rows + 1, visits.rows, TVIS_REC * (unsigned)i);
    visit_pack(visits.rows[0], key, day);
}

void talk_visit_note(int guest, int avatar, int venue)
{
    uint32_t key;
    if (!visit_guest_ok(guest) || avatar < 0 || avatar >= (int)NEWS_NONE ||
        venue < 0 || venue >= ACT_COUNT) return;
    key = NEWS_IDS(guest, avatar, venue);
    if (!visit_valid(key)) return;
    visit_push(key, visit_today());
    elder_observe(guest);
}

void talk_visit_import_news(void)
{
    int i;
    if (visits.count) return;
    for (i = sim_news_count() - 1; i >= 0; i--) {
        const DoomoNewsRec *news = sim_news_record(i);
        uint32_t key, day;
        if (!news || (news->tmpl != NT_TALK_MEET && news->tmpl != NT_TALK_MEET_FAV) ||
            news->v >> 20 != NEWS_NONE || news->arg8 >= ACT_COUNT) continue;
        key = (news->v & TVIS_PEOPLE) | ((uint32_t)news->arg8 << 20);
        /* The news stores the low 16 bits of the day. */
        day = doomo_town.day - (uint16_t)(doomo_town.day - news->day);
        if (visit_valid(key) && day <= visit_today()) visit_push(key, day);
    }
}

static int visit_matches(const uint8_t *row, uint32_t people, int minor)
{
    return (visit_key(row) & TVIS_PEOPLE) == people &&
           talk_act_invite_ok(TA_INVITE, (int)(row[2] >> 4), minor, 12);
}

int talk_visit_venues(int guest, int avatar, int minor, int *venues, int capacity)
{
    uint32_t people, seen = 0;
    int i, count = 0;
    if (!visit_guest_ok(guest) || avatar < 0 || avatar >= (int)NEWS_NONE) return 0;
    people = NEWS_IDS(guest, avatar, 0);
    for (i = 0; i < visits.count && count < capacity; i++) {
        const uint8_t *row = visits.rows[i];
        int venue = (int)(visit_key(row) >> 20);
        uint32_t bit = 1u << venue;
        if ((seen & bit) || !visit_matches(row, people, minor)) continue;
        venues[count++] = venue;
        seen |= bit;
    }
    return count;
}

int talk_visit_page(int guest, int avatar, int minor, int offset, TalkVisit *page, int capacity)
{
    uint32_t people;
    int i, count = 0;
    if (!visit_guest_ok(guest) || avatar < 0 || avatar >= (int)NEWS_NONE) return 0;
    people = NEWS_IDS(guest, avatar, 0);
    for (i = 0; i < visits.count && count < capacity; i++) {
        const uint8_t *row = visits.rows[i];
        if (!visit_matches(row, people, minor)) continue;
        if (offset > 0) { offset--; continue; }
        if (page) {
            page[count].venue = (int)(row[2] >> 4);
            page[count].day = visit_day(row);
        }
        count++;
    }
    return count;
}

int talk_visit_has(int guest, int avatar, int minor, const TalkVisit *visit)
{
    uint32_t key;
    int i;
    if (!visit_guest_ok(guest) || avatar < 0 || avatar >= (int)NEWS_NONE ||
        !talk_act_invite_ok(TA_INVITE, visit->venue, minor, 12)) return 0;
    key = NEWS_IDS(guest, avatar, visit->venue);
    for (i = 0; i < visits.count; ++i)
        if (visit_key(visits.rows[i]) == key && visit_day(visits.rows[i]) == visit->day) return 1;
    return 0;
}

uint32_t talk_visit_latest_date(int guest, int avatar, int minor, int venue)
{
    uint32_t key;
    int i;
    if (!visit_guest_ok(guest) || avatar < 0 || avatar >= (int)NEWS_NONE ||
        !talk_act_invite_ok(TA_INVITE, venue, minor, 12)) return TALK_VISIT_UNDATED;
    key = NEWS_IDS(guest, avatar, venue);
    for (i = 0; i < visits.count; ++i)
        if (visit_key(visits.rows[i]) == key) return visit_day(visits.rows[i]);
    return TALK_VISIT_UNDATED;
}

static void visit_reset(void)
{
    visits.count = 0;
    visits.elders = 0;
}

static uint32_t visit_size(void)
{
    if (!visits.count && !visits.elders) return 0;
    return TVIS_HDR + visits.count * TVIS_REC + visits.elders * TVIS_ELDER_REC;
}

static uint32_t visit_write(uint8_t *dst, uint32_t cap)
{
    uint32_t i, size = visit_size();
    if (!size || cap < size) return 0;
    put16(dst, visits.count);
    dst[2] = visits.elders;
    memcpy(dst + TVIS_HDR, visits.rows, visits.count * TVIS_REC);
    dst += TVIS_HDR + visits.count * TVIS_REC;
    for (i = 0; i < visits.elders; i++, dst += TVIS_ELDER_REC) {
        put16(dst, visits.elder[i] & 0xFFFFu);
        put16(dst + 2, visits.elder[i] >> 16);
    }
    return size;
}

static int visit_read_v1(const uint8_t *src, uint32_t len)
{
    uint32_t i;
    if (len > TVIS_V1_MAX || len % TVIS_V1_REC) return 0;
    for (i = 0; i < len && visits.count < SAVE_TVIS_VISITS; i += TVIS_V1_REC) {
        uint32_t key = get16(src + i) | ((uint32_t)src[i + 2] << 16);
        if (visit_valid(key) && !visit_duplicate(key, TALK_VISIT_UNDATED))
            visit_pack(visits.rows[visits.count++], key, TALK_VISIT_UNDATED);
    }
    return 1;
}

static int visit_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    uint32_t count, elders, i, last_day = visit_today();
    visit_reset();
    if (version == 1) return visit_read_v1(src, len);
    if (version != 2 || len < TVIS_HDR || len > SAVE_TVIS_WIRE_MAX) return 0;
    count = get16(src);
    elders = src[2];
    if (count > SAVE_TVIS_WIRE_VISITS || elders > SAVE_TVIS_WIRE_ELDERS ||
        len != TVIS_HDR + count * TVIS_REC + elders * TVIS_ELDER_REC) return 0;
    src += TVIS_HDR;
    for (i = 0; i < count && visits.count < SAVE_TVIS_VISITS; i++) {
        const uint8_t *row = src + i * TVIS_REC;
        uint32_t key = visit_key(row), day = visit_day(row);
        if ((day != TALK_VISIT_UNDATED && (last_day == TALK_VISIT_UNDATED || day > last_day)) ||
            !visit_valid(key) || visit_duplicate(key, day)) continue;
        last_day = day;
        memcpy(visits.rows[visits.count++], row, TVIS_REC);
    }
    src += count * TVIS_REC;
    for (i = 0; i < elders && visits.elders < SAVE_TVIS_ELDERS; i++, src += TVIS_ELDER_REC) {
        uint32_t item = get16(src) | ((uint32_t)get16(src + 2) << 16);
        int guest = (int)(item & 1023u);
        if (item >> 10 <= visit_today() && elder_eligible(guest) && elder_index(guest) < 0)
            visits.elder[visits.elders++] = item;
    }
    return 1;
}

static const DoomoSaveExt visit_ext = {
    SAVE_EXT_TAG_TVIS, 2, visit_size, visit_write, visit_read, visit_reset, SAVE_EXT_MAX_TVIS
};
const DoomoSaveExt *const doomo_save_ext_tvis = &visit_ext;

/* TEVT v1 is a count followed by six-byte rows, newest first. The key
 * holds two 10-bit IDs, a five-bit place and a four-bit event kind. */
#define TEVT_REC 6u
static uint8_t event_rows[SAVE_TEVT_EVENTS][TEVT_REC];
static uint8_t event_count;

DOOMO_STATIC_ASSERT(SAVE_TEVT_WIRE_EVENTS <= 255 && SAVE_TEVT_EVENTS <= SAVE_TEVT_WIRE_EVENTS,
                    event_count_fits);
DOOMO_STATIC_ASSERT((int)ACT_COUNT < TALK_PLACE_NEST_PLAZA && TALK_PLACE_COUNT <= 32 && TALK_EV_COUNT <= 16,
                    event_fields_fit);

static uint32_t event_key(const uint8_t *row)
{
    return get16(row) | ((uint32_t)get16(row + 2) << 16);
}

static int event_kind(uint32_t key) { return (int)(key >> 25); }
static int event_place(uint32_t key) { return (key >> 20) & 31u; }
static int event_is_private(int kind) { return kind >= TALK_EV_PRAISE && kind <= TALK_EV_APOLOGY; }
static int event_is_world(int kind) { return kind >= TALK_EV_BATTLE_LOST; }

static int event_valid(uint32_t key)
{
    int a = key & NEWS_NONE, b = (key >> 10) & NEWS_NONE;
    int kind = event_kind(key), place = event_place(key);
    if (kind <= 0 || kind >= TALK_EV_COUNT) return 0;
    if (event_is_world(kind))
        return a == NEWS_NONE && b == NEWS_NONE && place >= TALK_PLACE_NEST_PLAZA &&
               place < TALK_PLACE_COUNT && (kind != TALK_EV_RESCUED || place < TALK_PLACE_UNDERBELLY);
    if (place > ACT_COUNT || a == b || !person_ok(a) || !person_ok(b)) return 0;
    return event_is_private(kind) ? visit_guest_ok(a) : a < b && visit_guest_ok(a) && visit_guest_ok(b);
}

static int event_priority(int kind)
{
    if (kind >= TALK_EV_BATTLE_WON) return 3;
    if (kind == TALK_EV_BATTLE_LOST || kind == TALK_EV_RECONCILED || kind == TALK_EV_FRIENDSHIP) return 2;
    return event_is_private(kind) || kind == TALK_EV_FIGHT ? 1 : 0;
}

static int event_index(uint32_t key, uint32_t day)
{
    int i, one_off = event_kind(key) >= TALK_EV_BATTLE_WON;
    for (i = 0; i < event_count; i++) {
        uint32_t saved_day = get16(event_rows[i] + 4);
        if (!one_off && saved_day < day) break;
        if ((one_off || saved_day == day) && key == event_key(event_rows[i])) return i;
    }
    return -1;
}

/* Prefer older events of the lowest priority when the journal is full.
 * When loading a larger console's journal, equal-priority rows keep the
 * newer date. The four chapter rescues cannot be crowded out by spats. */
static int event_room(int kind, int importing)
{
    int i, victim = -1, priority = event_priority(kind) + !importing;
    if (event_count < SAVE_TEVT_EVENTS) return event_count++;
    for (i = event_count - 1; i >= 0; i--) {
        int old = event_priority(event_kind(event_key(event_rows[i])));
        if (old < priority) { priority = old; victim = i; }
        if (priority == 0) break;
    }
    return victim;
}

void talk_event_note(int kind, int person, int other, int place)
{
    uint32_t key, day = visit_today();
    int at, pos = 0;
    if (kind <= 0 || kind >= TALK_EV_COUNT || place < 0 || place >= TALK_PLACE_COUNT) return;
    if (event_is_world(kind)) {
        if (person != -1 || other != -1) return;
        person = other = NEWS_NONE;
    } else {
        if (person < 0 || person >= (int)NEWS_NONE || other < 0 || other >= (int)NEWS_NONE) return;
        if (!event_is_private(kind) && person > other) { int swap = person; person = other; other = swap; }
    }
    key = NEWS_IDS(person, other, place) | ((uint32_t)kind << 25);
    if (!event_valid(key)) return;
    at = event_index(key, day);
    if (at >= 0 && kind >= TALK_EV_BATTLE_WON) return;
    if (at < 0) at = event_room(kind, 0);
    if (at < 0) return;
    if (event_count > 1 && day < get16(event_rows[0] + 4)) {
        memmove(event_rows + at, event_rows + at + 1, TEVT_REC * (unsigned)(event_count - at - 1));
        at = event_count - 1;
        while (pos < at && day < get16(event_rows[pos] + 4)) pos++;
    }
    memmove(event_rows + pos + 1, event_rows + pos, TEVT_REC * (unsigned)(at - pos));
    put16(event_rows[pos], key & 0xFFFFu);
    put16(event_rows[pos] + 2, key >> 16);
    put16(event_rows[pos] + 4, day);
}

/* Return the other participant, or NEWS_NONE for a public record. */
static int event_other(uint32_t key, int guest, int avatar)
{
    int a = key & NEWS_NONE, b = (key >> 10) & NEWS_NONE;
    int kind = event_kind(key);
    if (event_is_world(kind)) return guest == -1 ? (int)NEWS_NONE : -1;
    if (event_is_private(kind)) return guest == a && avatar == b ? b : -1;
    return guest == a ? b : guest == b ? a : -1;
}

int talk_event_page(int guest, int avatar, int offset, TalkMemoryEvent *page, int capacity)
{
    int i, count = 0;
    if (guest != -1 && (!visit_guest_ok(guest) || !town_alive(guest))) return 0;
    for (i = 0; i < event_count && count < capacity; i++) {
        uint32_t key = event_key(event_rows[i]);
        int other = event_other(key, guest, avatar);
        if (other < 0) continue;
        if (offset > 0) { offset--; continue; }
        if (page) {
            page[count].day = (uint16_t)get16(event_rows[i] + 4);
            page[count].other = (uint16_t)other;
            page[count].kind = (uint8_t)event_kind(key);
            page[count].place = (uint8_t)event_place(key);
        }
        count++;
    }
    return count;
}

int talk_event_has(int guest, int avatar, const TalkMemoryEvent *event)
{
    int i;
    if (!event || (guest != -1 && (!visit_guest_ok(guest) || !town_alive(guest)))) return 0;
    for (i = 0; i < event_count; i++) {
        uint32_t key = event_key(event_rows[i]);
        unsigned day = get16(event_rows[i] + 4);
        if (day < event->day) break;
        if (day == event->day && event_kind(key) == event->kind && event_place(key) == event->place &&
            event_other(key, guest, avatar) == event->other) return 1;
    }
    return 0;
}

int talk_event_next_day(int guest, int avatar, const TalkMemoryEvent *event, int kind)
{
    int i, next = -1;
    if (!event || (guest != -1 && (!visit_guest_ok(guest) || !town_alive(guest)))) return -1;
    /* Newest first, including insertion order within a day. Return the
     * closest later match only once we reach the actual source record. */
    for (i = 0; i < event_count; i++) {
        uint32_t key = event_key(event_rows[i]);
        int row_kind = event_kind(key), day = get16(event_rows[i] + 4);
        if (day < event->day) break;
        if (event_other(key, guest, avatar) != event->other) continue;
        if (row_kind == event->kind && event_place(key) == event->place && day == event->day)
            return next;
        if (row_kind == kind) next = day;
    }
    return -1;
}

int talk_event_after(int guest, int avatar, const TalkMemoryEvent *event, int kind)
{
    return talk_event_next_day(guest, avatar, event, kind) >= 0;
}

int talk_event_pending_hurt(int guest, int avatar)
{
    int i;
    if (avatar < 0 || avatar >= (int)NEWS_NONE) return 0;
    for (i = 0; i < event_count; i++) {
        uint32_t key = event_key(event_rows[i]);
        int kind = event_kind(key);
        if ((kind == TALK_EV_HURT || kind == TALK_EV_APOLOGY) &&
            event_other(key, guest, avatar) == avatar) return kind == TALK_EV_HURT;
    }
    return 0;
}

static void event_reset(void) { event_count = 0; }
static uint32_t event_size(void) { return event_count ? 1u + event_count * TEVT_REC : 0; }

static uint32_t event_write(uint8_t *dst, uint32_t cap)
{
    uint32_t size = event_size();
    if (!size || cap < size) return 0;
    dst[0] = event_count;
    memcpy(dst + 1, event_rows, size - 1);
    return size;
}

static int event_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    uint32_t i, last = visit_today();
    event_reset();
    if (version != 1 || !len || src[0] > SAVE_TEVT_WIRE_EVENTS || len != 1u + src[0] * TEVT_REC) return 0;
    for (i = 0; i < src[0]; ++i) {
        const uint8_t *row = src + 1 + i * TEVT_REC;
        uint32_t key = event_key(row), day = get16(row + 4);
        int at;
        if (day > last || !event_valid(key) || event_index(key, day) >= 0) continue;
        last = day;
        at = event_room(event_kind(key), 1);
        if (at < 0) continue;
        memmove(event_rows + at, event_rows + at + 1, TEVT_REC * (unsigned)(event_count - at - 1));
        memcpy(event_rows[event_count - 1], row, TEVT_REC);
    }
    return 1;
}

static const DoomoSaveExt event_ext = {
    SAVE_EXT_TAG_TEVT, 1, event_size, event_write, event_read, event_reset, SAVE_EXT_MAX_TEVT
};
const DoomoSaveExt *const doomo_save_ext_tevt = &event_ext;

static int history_has_person(uint32_t key, unsigned id)
{
    return (key & NEWS_NONE) == id || ((key >> 10) & NEWS_NONE) == id;
}

void talk_forget_departed(int id)
{
    unsigned i, kept;
    int elder;
    if (id < 0 || id >= (int)NEWS_NONE) return;
    talk_mem_forget(id);
    talk_link_forget(id);
    talk_outcome_meet_forget(id);
    /* Older saves can reconstruct an empty outing archive from these headlines. */
    sim_news_forget_meetups(id);

    for (i = 0, kept = 0; i < visits.count; i++) {
        if (history_has_person(visit_key(visits.rows[i]), (unsigned)id)) continue;
        if (kept != i) memcpy(visits.rows[kept], visits.rows[i], TVIS_REC);
        kept++;
    }
    visits.count = (uint16_t)kept;
    elder = elder_index(id);
    if (elder >= 0) {
        visits.elders--;
        memmove(visits.elder + elder, visits.elder + elder + 1,
                (visits.elders - (unsigned)elder) * sizeof(visits.elder[0]));
    }

    for (i = 0, kept = 0; i < event_count; i++) {
        if (history_has_person(event_key(event_rows[i]), (unsigned)id)) continue;
        if (kept != i) memcpy(event_rows[kept], event_rows[i], TEVT_REC);
        kept++;
    }
    event_count = (uint8_t)kept;
}

/* TLNK v1: [u8 count], then six bytes per link. The first three hold
 * resident, avatar (10 bits each) and weight (4 bits). The remaining
 * three hold two sorted concept IDs (12 bits each). Newest use first. */
#define TLNK_REC 6u
static uint8_t link_rows[SAVE_TLNK_FREE_LINKS][TLNK_REC];
static uint8_t link_count;

DOOMO_STATIC_ASSERT(SAVE_TLNK_STORY_LINKS <= SAVE_TLNK_FREE_LINKS &&
                    SAVE_TLNK_FREE_LINKS <= SAVE_TLNK_WIRE_LINKS && SAVE_TLNK_WIRE_LINKS <= 255 &&
                    SAVE_EXT_MAX_TLNK <= DOOMO_SAVE_EXT_MAX, topic_links_fit_save);

static unsigned link_capacity(void)
{
    return doomo_town.mode == TOWN_STORY ? SAVE_TLNK_STORY_LINKS : SAVE_TLNK_FREE_LINKS;
}

static uint32_t link_owner(const uint8_t *row)
{
    return get16(row) | ((uint32_t)(row[2] & 15u) << 16);
}

static uint32_t link_pair(const uint8_t *row)
{
    return get16(row + 3) | ((uint32_t)row[5] << 16);
}

static uint32_t link_key(uint16_t a, uint16_t b)
{
    if (!a || !b || a == b || a > TALK_MAX_CONCEPTS || b > TALK_MAX_CONCEPTS) return 0;
    return a < b ? a | ((uint32_t)b << 12) : b | ((uint32_t)a << 12);
}

static int link_people_ok(int resident, int avatar)
{
    return visit_guest_ok(resident) && resident != avatar && avatar >= 0 && avatar < (int)NEWS_NONE &&
           person_ok(resident) && person_ok(avatar);
}

static int link_find(uint32_t owner, uint32_t pair)
{
    int i;
    for (i = 0; i < link_count; i++)
        if (link_owner(link_rows[i]) == owner && link_pair(link_rows[i]) == pair) return i;
    return -1;
}

static int link_oldest_weak(void)
{
    int i, oldest = link_count - 1;
    unsigned best = 16;
    for (i = oldest; i >= 0; i--) {
        unsigned weight = link_rows[i][2] >> 4;
        if (weight < best) { best = weight; oldest = i; }
        if (best == 1) break;
    }
    return oldest;
}

int talk_link_topic_safe(uint16_t topic)
{
    TalkConcept c;
    uint8_t flags;
    return topic && talk_db_concept(topic, &c) && c.age == TAGE_ANY && c.reg == TR_CLEAN &&
           !(c.flags & TCF_GRIEF) && talk_db_cat(c.cat, NULL, &flags) &&
           (flags & TCAT_TALKABLE) && !(flags & (TCAT_GATED | TCAT_SAFETY));
}

void talk_link_note(int resident, int avatar, uint16_t a, uint16_t b)
{
    uint32_t owner = NEWS_IDS(resident, avatar, 0), pair = link_key(a, b);
    unsigned weight = 1, cap = link_capacity();
    int i;
    if (!pair || !link_people_ok(resident, avatar)) return;
    if (link_count > cap) link_count = (uint8_t)cap;
    i = link_find(owner, pair);
    if (i < 0) i = link_count < cap ? link_count++ : link_oldest_weak();
    else weight = DOOMO_MIN(15u, (link_rows[i][2] >> 4) + 1u);
    memmove(link_rows + 1, link_rows, TLNK_REC * (unsigned)i);
    put16(link_rows[0], owner & 0xFFFFu);
    link_rows[0][2] = (uint8_t)((owner >> 16) | (weight << 4));
    put16(link_rows[0] + 3, pair & 0xFFFFu);
    link_rows[0][5] = (uint8_t)(pair >> 16);
}

int talk_link_weight(int resident, int avatar, uint16_t a, uint16_t b)
{
    uint32_t pair = link_key(a, b);
    int i;
    if (!pair || !link_count || !link_people_ok(resident, avatar)) return 0;
    i = link_find(NEWS_IDS(resident, avatar, 0), pair);
    return i < 0 ? 0 : link_rows[i][2] >> 4;
}

uint16_t talk_link_pick(int resident, int avatar, uint16_t topic, uint16_t avoid)
{
    uint32_t owner = NEWS_IDS(resident, avatar, 0);
    uint16_t best = TC_NONE;
    int i, strength = TALK_LINK_REHEARSED - 1;
    if (!topic || !link_count || !link_people_ok(resident, avatar)) return TC_NONE;
    for (i = 0; i < link_count; i++) {
        const uint8_t *row = link_rows[i];
        int weight = row[2] >> 4;
        uint32_t pair;
        uint16_t a, b, other;
        if (weight <= strength || link_owner(row) != owner) continue;
        pair = link_pair(row);
        a = (uint16_t)(pair & TALK_MAX_CONCEPTS);
        b = (uint16_t)(pair >> 12);
        other = a == topic ? b : b == topic ? a : TC_NONE;
        if (!other || other == avoid || !talk_link_topic_safe(other)) continue;
        best = other;
        strength = weight;
        if (strength == 15) break;
    }
    return best && talk_link_topic_safe(topic) ? best : TC_NONE;
}

int talk_link_page(int resident, int avatar, int offset, TalkTopicPair *page, int capacity)
{
    uint32_t owner = NEWS_IDS(resident, avatar, 0);
    int i, count = 0;
    if (!link_count || !link_people_ok(resident, avatar)) return 0;
    for (i = 0; i < link_count && count < capacity; i++) {
        const uint8_t *row = link_rows[i];
        uint32_t pair;
        uint16_t a, b;
        if ((row[2] >> 4) < TALK_LINK_REHEARSED || link_owner(row) != owner) continue;
        pair = link_pair(row);
        a = (uint16_t)(pair & TALK_MAX_CONCEPTS);
        b = (uint16_t)(pair >> 12);
        if (!talk_link_topic_safe(a) || !talk_link_topic_safe(b)) continue;
        if (offset > 0) { offset--; continue; }
        if (page) page[count] = (TalkTopicPair){ a, b };
        count++;
    }
    return count;
}

void talk_link_forget(int id)
{
    unsigned i, kept = 0;
    for (i = 0; i < link_count; i++) {
        uint32_t owner = link_owner(link_rows[i]);
        if ((owner & NEWS_NONE) == (unsigned)id || (owner >> 10) == (unsigned)id) continue;
        if (kept != i) memcpy(link_rows[kept], link_rows[i], TLNK_REC);
        kept++;
    }
    link_count = (uint8_t)kept;
}

static void link_reset(void) { link_count = 0; }

static uint32_t link_size(void)
{
    unsigned count = DOOMO_MIN(link_count, link_capacity());
    return count ? 1u + count * TLNK_REC : 0;
}

static uint32_t link_write(uint8_t *dst, uint32_t cap)
{
    uint32_t size = link_size();
    if (!size || cap < size) return 0;
    dst[0] = (uint8_t)((size - 1u) / TLNK_REC);
    memcpy(dst + 1, link_rows, size - 1u);
    return size;
}

static int link_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    unsigned i, cap = link_capacity();
    link_reset();
    if (version != 1 || !len || src[0] > SAVE_TLNK_WIRE_LINKS || len != 1u + src[0] * TLNK_REC) return 0;
    for (i = 0; i < src[0]; i++) {
        const uint8_t *row = src + 1u + i * TLNK_REC;
        uint32_t owner = link_owner(row), pair = link_pair(row);
        unsigned a = pair & TALK_MAX_CONCEPTS, b = pair >> 12;
        if (!a || a >= b || !link_people_ok(owner & NEWS_NONE, owner >> 10) ||
            !(row[2] >> 4) || link_find(owner, pair) >= 0) continue;
        if (link_count == cap) {
            int drop = link_oldest_weak();
            if ((row[2] >> 4) <= (link_rows[drop][2] >> 4)) continue;
            link_count--;
            memmove(link_rows + drop, link_rows + drop + 1, TLNK_REC * (unsigned)(link_count - drop));
        }
        memcpy(link_rows[link_count++], row, TLNK_REC);
    }
    return 1;
}

static const DoomoSaveExt link_ext = {
    SAVE_EXT_TAG_TLNK, 1, link_size, link_write, link_read, link_reset, SAVE_EXT_MAX_TLNK
};
const DoomoSaveExt *const doomo_save_ext_tlnk = &link_ext;
#endif /* DOOMO_HAS_TALK */
