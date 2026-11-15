/* part_lru.c - the PS1's LZ part-script blocks: a 2-slot block LRU and a per-render part arena
 * (customisation v2, P1.M1; SPEC 4.4; CR2).
 *
 * The part scripts are kept LZ-compressed in 2 KB blocks (parts.h PartLz, tools/parts_build.py). A
 * block is decoded into one of two 2 KB slots when an entry in it is first needed (least recently
 * used replaced), and the ENTRY (its blocks up to PH_END) is copied into a 4 KB arena that lives for
 * one render: part_arena_begin() (pctx_init) empties it, so each worn entry is decoded once per
 * render however many layer groups and hooks run it. 8 KB in all, as the old 4-slot LRU.
 *
 * The compressed bytes come through part_lz_src(): straight from the linked array.
 *
 * Only DOOMO_PARTS_LZ builds read through it; host unit builds compile it with
 * DOOMO_PARTS_LZ_TEST to check it against the raw arrays.
 *
 * CR10: the PS1 no longer uses the LZ form. Its scripts are RAW bytes in SPU RAM and an entry is read
 * straight into the per-render arena (part_store_ops, below): no LZ decoder, no 4 KB of block slots, no
 * staging buffer. */
#include "doomo/doomo.h"
#include "doomo/parts.h"
#include "doomo/part_icons.h"

#if defined(DOOMO_PARTS_LZ) || defined(DOOMO_PARTS_LZ_TEST)

#define ARENA_ENTRIES 32

static uint8_t  buf[PART_SLOTS][PART_BLK];
static const PartLz *slot_lz[PART_SLOTS];
static uint16_t slot_blk[PART_SLOTS];
static uint32_t slot_use[PART_SLOTS];
static uint32_t use_clock, loads, fills;

static uint8_t arena[PART_ARENA];
#define STAGE NULL
#define STAGE_CAP 0u
static struct { const PartLz *z; uint16_t ofs, at; } ae[ARENA_ENTRIES];
static uint16_t arena_used, arena_n;

const uint8_t *part_lz_src(const PartLz *z, unsigned blk, uint8_t *stage, unsigned cap)
{
    (void)stage; (void)cap;
    return z->z + z->zofs[blk];
}

static void unlz(const uint8_t *s, uint8_t *d, unsigned n)
{
    unsigned o = 0;
    while (o < n) {
        unsigned c = *s++, k;
        if (c < 0x80) {
            k = c + 1;
            if (k > n - o) k = n - o;
            while (k--) d[o++] = *s++;
        } else {
            unsigned dist = s[0] | ((unsigned)s[1] << 8);
            s += 2;
            k = (c & 0x7Fu) + 3;
            if (dist == 0 || dist > o) return;              /* corrupt: stop, never read outside */
            if (k > n - o) k = n - o;
            while (k--) { d[o] = d[o - dist]; o++; }
        }
    }
}

const uint8_t *part_lz_ops(const PartLz *z, unsigned ofs)
{
    unsigned lo = 0, hi = z->nblk, b, n;
    int i, v = 0;
    while (hi - lo > 1) {                                   /* the last block starting at or before ofs */
        unsigned mid = (lo + hi) / 2;
        if (z->rofs[mid] <= ofs) lo = mid; else hi = mid;
    }
    b = lo;
    use_clock++;
    for (i = 0; i < PART_SLOTS; i++)
        if (slot_lz[i] == z && slot_blk[i] == b) {
            slot_use[i] = use_clock;
            return buf[i] + (ofs - z->rofs[b]);
        }
    for (i = 1; i < PART_SLOTS; i++)
        if (slot_use[i] < slot_use[v]) v = i;
    n = (unsigned)(z->rofs[b + 1] - z->rofs[b]);
    if (n > PART_BLK) n = PART_BLK;
    unlz(part_lz_src(z, b, STAGE, STAGE_CAP), buf[v], n);
    slot_lz[v] = z;
    slot_blk[v] = (uint16_t)b;
    slot_use[v] = use_clock;
    loads++;
    return buf[v] + (ofs - z->rofs[b]);
}

static void lz_arena_begin(void)
{
    arena_used = 0;
    arena_n = 0;
}

const uint8_t *part_arena_ops(const PartLz *z, unsigned ofs)
{
    const uint8_t *p, *q;
    unsigned len, i;
    for (i = 0; i < arena_n; i++)
        if (ae[i].z == z && ae[i].ofs == ofs) return arena + ae[i].at;
    p = part_lz_ops(z, ofs);
    for (q = p; q[0] != PH_END; q += 4 + q[3]) {}           /* an entry never straddles blocks */
    len = (unsigned)(q - p) + 4;
    if (arena_n >= ARENA_ENTRIES || arena_used + len > PART_ARENA) return p;   /* full: the slot */
    memcpy(arena + arena_used, p, len);
    ae[arena_n].z = z;
    ae[arena_n].ofs = (uint16_t)ofs;
    ae[arena_n].at = arena_used;
    arena_n++;
    arena_used = (uint16_t)(arena_used + len);
    fills++;
    return arena + ae[arena_n - 1].at;
}

uint32_t part_lru_loads(void) { return loads; }
uint32_t part_arena_fills(void) { return fills; }

void part_lru_reset(void)
{
    int i;
    for (i = 0; i < PART_SLOTS; i++) { slot_lz[i] = NULL; slot_use[i] = 0; }
    loads = 0;
    fills = 0;
    lz_arena_begin();
}

#endif

/* ---- CR10: the raw part store (SPU RAM on the PS1) ----------------------------------------------- */
#if defined(DOOMO_PARTS_SPU) || defined(DOOMO_PARTS_SPU_TEST)

typedef struct { const PartSet *set; uint16_t ofs, at, len; } StoreEnt;
static uint8_t  st_arena[PART_STORE_ARENA];
static StoreEnt st_ent[PART_STORE_SLOTS];
static uint8_t  st_win[PART_ENTRY_WIN] __attribute__((aligned(4)));   /* the entry being read / the spill */
static uint16_t st_used, st_n;
static uint32_t st_fills, st_spills;

#if defined(DOOMO_PARTS_SPU_TEST)
/* host: the store is the linked raw array; `part_store_test_fail` makes it read nothing (store not loaded) */
int part_store_test_fail;
int plat_part_read(unsigned set, unsigned ofs, uint8_t *dst, unsigned n)
{
    const PartSet *ps;
    unsigned i, total;
    if (part_store_test_fail || set >= doomo_part_script_count) return 0;
    ps = doomo_part_scripts[set];
    if (!ps->ops) return 0;
    total = (unsigned)ps->ref[ps->n - 1].ofs;
    {   /* the raw size is the last entry's end: walk it */
        const uint8_t *q = ps->ops + total;
        while (q[0] != PH_END) q += 4 + q[3];
        total = (unsigned)(q - ps->ops) + 4;
    }
    for (i = 0; i < n; i++) dst[i] = ofs + i < total ? ps->ops[ofs + i] : 0;
    return 1;
}

/* the packed part icons' bytes: the linked array here, SPU RAM on the PS1 (ps1/src/partstore.c) */
int plat_part_icon_read(uint32_t ofs, uint8_t *dst, unsigned n)
{
    uint32_t total = doomo_part_icon_rle_ofs[doomo_part_icon_rle_count];
    unsigned i;
    if (part_store_test_fail || n > PART_ICON_PACK_MAX) return 0;
    for (i = 0; i < n; i++) dst[i] = ofs + i < total ? doomo_part_icon_rle[ofs + i] : 0;
    return 1;
}
#endif

void part_store_reset(void)
{
    st_used = 0;
    st_n = 0;
    st_fills = 0;
    st_spills = 0;
}

const uint8_t *part_store_ops(const PartSet *set, unsigned ofs)
{
    static const uint8_t none[4] = { PH_END, 0, 0, 0 };
    const uint8_t *q;
    unsigned i, len, si;
    for (i = 0; i < st_n; i++)
        if (st_ent[i].set == set && st_ent[i].ofs == ofs) return st_arena + st_ent[i].at;
    for (si = 0; si < doomo_part_script_count; si++)
        if (doomo_part_scripts[si] == set) break;
    if (si >= doomo_part_script_count || !plat_part_read(si, ofs, st_win, PART_ENTRY_WIN)) return none;
    for (q = st_win; q < st_win + PART_ENTRY_WIN - 3 && q[0] != PH_END; q += 4 + q[3]) {}
    if (q >= st_win + PART_ENTRY_WIN - 3) return none;      /* no end inside the window: a damaged store */
    len = (unsigned)(q - st_win) + 4;
    st_fills++;
    if (st_n >= PART_STORE_SLOTS || st_used + len > PART_STORE_ARENA) {   /* full: the window itself */
        st_spills++;
        return st_win;
    }
    memcpy(st_arena + st_used, st_win, len);
    st_ent[st_n].set = set;
    st_ent[st_n].ofs = (uint16_t)ofs;
    st_ent[st_n].at = st_used;
    st_ent[st_n].len = (uint16_t)len;
    st_n++;
    st_used = (uint16_t)(st_used + len);
    return st_arena + st_ent[st_n - 1].at;
}

uint32_t part_store_fills(void) { return st_fills; }
uint32_t part_store_spills(void) { return st_spills; }

#endif

/* a render starts: every per-render arena is emptied (pctx_init) */
#if defined(DOOMO_PARTS_LZ) || defined(DOOMO_PARTS_LZ_TEST) || defined(DOOMO_PARTS_SPU) || defined(DOOMO_PARTS_SPU_TEST)
void part_arena_begin(void)
{
#if defined(DOOMO_PARTS_LZ) || defined(DOOMO_PARTS_LZ_TEST)
    lz_arena_begin();
#endif
#if defined(DOOMO_PARTS_SPU) || defined(DOOMO_PARTS_SPU_TEST)
    st_used = 0;
    st_n = 0;
#endif
}
#endif
