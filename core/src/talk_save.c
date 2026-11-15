/* talk_save.c - talk memory and its save chunk 'TALK' (owner: TK, T6;
 * docs/TALK.md 6e; design 6.8, 5.5 "Talk memory", 3.6 "Memory").
 *
 * The table: at most TALK_MEM_MAX (24) entries of 12 B, the most recently
 * used first, so the last one is the one a 25th person evicts (LRU). With
 * its 4 B header that is the whole state: 292 B of .bss (TALK_MEM_WORST),
 * and the chunk is the same bytes written little endian:
 *   [u8 version = 1][u8 count][u16 day_of_last] then count x
 *   {u16 id; u16 day; u16 topic_a; u16 topic_b; u8 flags; u8 likes_known;
 *    u16 spare = 0}
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
 * drops it whole: 0 B on the GBA and on the PS1 today.
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

DOOMO_STATIC_ASSERT(sizeof(TalkMem) == TALK_MEM_ENTRY, talk_mem_entry_is_12_bytes);

#define TMF_LAST    (TMF_COMPLIMENTED | TMF_INSULTED | TMF_PROMISED)
#define TMF_SAVED   (TMF_LAST | TMF_REFUSED_TODAY)
#define PEND_SHIFT  4                       /* the running conversation's TMF_LAST bits */
#define LIKES_MASK  0x1Fu
#define TOPIC_MASK  0x0FFFu

static struct {
    uint8_t  count;
    uint8_t  pad;
    uint16_t day;                           /* day_of_last: the town day the flags are of */
    TalkMem  e[TALK_MEM_MAX];
} mem;                                      /* 292 B */

DOOMO_STATIC_ASSERT(sizeof(mem) == TALK_MEM_WORST, talk_mem_table_is_the_worst_case);

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
            int v = mem.count - 1;
            while (v > 0 && TQB_KIND(mem.e[v].spare) && TQB_STATE(mem.e[v].spare) != TQS_SETTLED) v--;
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

void talk_mem_reset(void) { memset(&mem, 0, sizeof mem); talk_trait_reset(); talk_quest_reset(); }   /* TQ: the pace too */

/* ---- the chunk (only where the build has Talk) ----------------------------- */
#ifdef DOOMO_HAS_TALK
static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static unsigned get16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }

/* nothing while the table is empty: like PLAY and PROF, an empty table is
 * a chunk of 0 bytes */
static uint32_t talk_size(void)
{
    uint32_t tr = talk_trait_save_size();
    return mem.count || tr ? (uint32_t)(TALK_MEM_HDR + mem.count * TALK_MEM_ENTRY) + tr : 0u;
}

static uint32_t talk_write(uint8_t *dst, uint32_t cap)
{
    uint32_t n = talk_size(), tr = talk_trait_save_size();
    int i;
    if (!n || cap < n) return 0;
    dst[0] = tr ? TALK_MEM_VERSION_TRAITS : TALK_MEM_VERSION;     /* 1: memory only (as before), 2: + traits tail */
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
    if (tr) talk_trait_save_write(dst + TALK_MEM_HDR + mem.count * TALK_MEM_ENTRY, tr);
    return n;
}

static int talk_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    int i, n;
    talk_mem_reset();
    if (version != TALK_MEM_VERSION) return 0;               /* a later format: ignored */
    if (len == 0) return 1;                                   /* the empty chunk */
    if (len < TALK_MEM_HDR || (src[0] != TALK_MEM_VERSION && src[0] != TALK_MEM_VERSION_TRAITS)) return 0;
    n = src[1];
    if (n > TALK_MEM_MAX || len < (uint32_t)(TALK_MEM_HDR + n * TALK_MEM_ENTRY)) return 0;
    if (src[0] == TALK_MEM_VERSION && len != (uint32_t)(TALK_MEM_HDR + n * TALK_MEM_ENTRY)) return 0;
    if (src[0] == TALK_MEM_VERSION_TRAITS &&
        len == (uint32_t)(TALK_MEM_HDR + n * TALK_MEM_ENTRY)) return 0;      /* a v2 chunk carries a tail */
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
        e->likes_known = (uint8_t)(p[9] & LIKES_MASK);
        e->spare = (uint16_t)(get16(p + 10) & TMEM_SPARE);
        if (TQB_KIND(e->spare) >= TQK_COUNT || (TQB_KIND(e->spare) == 0 && (e->spare & TMEM_QUEST)) ||
            (TQB_KIND(e->spare) && TQB_STATE(e->spare) == TQS_NONE))
            e->spare = (uint16_t)(e->spare & TMEM_DEPTH);                     /* a bad request field: none */
    }
    if (src[0] == TALK_MEM_VERSION_TRAITS) {                   /* TM1: the explicit trait sets */
        uint32_t at = (uint32_t)(TALK_MEM_HDR + n * TALK_MEM_ENTRY);
        if (!talk_trait_save_read(src + at, len - at)) { talk_mem_reset(); return 0; }
    }
    return 1;
}

static const DoomoSaveExt talk_ext = {
    SAVE_EXT_TAG_TALK, TALK_MEM_VERSION, talk_size, talk_write, talk_read, talk_mem_reset, TALK_SAVE_WORST
};
const DoomoSaveExt *const doomo_save_ext_talk = &talk_ext;
#endif /* DOOMO_HAS_TALK */
