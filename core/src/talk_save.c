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
    if (mem.day == today()) return;
    for (i = 0; i < mem.count; i++) mem.e[i].flags = (uint8_t)(mem.e[i].flags & ~TMF_REFUSED_TODAY);
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
        if (mem.count == TALK_MEM_MAX) mem.count--;          /* LRU: the last one goes */
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
    }
}

void talk_mem_forget(int id)
{
    int i = index_of(id);
    if (i >= 0) drop(i);
}

void talk_mem_reset(void) { memset(&mem, 0, sizeof mem); }

/* ---- the chunk (only where the build has Talk) ----------------------------- */
#ifdef DOOMO_HAS_TALK
static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static unsigned get16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }

/* nothing while the table is empty: like PLAY and PROF, an empty table is
 * a chunk of 0 bytes */
static uint32_t talk_size(void)
{
    return mem.count ? (uint32_t)(TALK_MEM_HDR + mem.count * TALK_MEM_ENTRY) : 0u;
}

static uint32_t talk_write(uint8_t *dst, uint32_t cap)
{
    uint32_t n = talk_size();
    int i;
    if (!n || cap < n) return 0;
    dst[0] = TALK_MEM_VERSION;
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
        put16(p + 10, 0);
    }
    return n;
}

static int talk_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    int i, n;
    talk_mem_reset();
    if (version != TALK_MEM_VERSION) return 0;               /* a later format: ignored */
    if (len == 0) return 1;                                   /* the empty chunk */
    if (len < TALK_MEM_HDR || src[0] != TALK_MEM_VERSION) return 0;
    n = src[1];
    if (n > TALK_MEM_MAX || len != (uint32_t)(TALK_MEM_HDR + n * TALK_MEM_ENTRY)) return 0;
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
    }
    return 1;
}

static const DoomoSaveExt talk_ext = {
    SAVE_EXT_TAG_TALK, TALK_MEM_VERSION, talk_size, talk_write, talk_read, talk_mem_reset, TALK_MEM_WORST
};
const DoomoSaveExt *const doomo_save_ext_talk = &talk_ext;
#endif /* DOOMO_HAS_TALK */
