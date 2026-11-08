/* part_lru.c - the PS1's LZ part-script blocks: a 2-slot block LRU and a per-render part arena
 * (customisation v2, P1.M1; SPEC 4.4; CR2).
 *
 * The part scripts are kept LZ-compressed in 2 KB blocks (parts.h PartLz, tools/parts_build.py). A
 * block is decoded into one of two 2 KB slots when an entry in it is first needed (least recently
 * used replaced), and the ENTRY (its blocks up to PH_END) is copied into a 4 KB arena that lives for
 * one render: part_arena_begin() (pctx_init) empties it, so each worn entry is decoded once per
 * render however many layer groups and hooks run it. 8 KB in all, as the old 4-slot LRU.
 *
 * The compressed bytes come through part_lz_src(): straight from the linked array by default. A
 * build that keeps the blocks in SPU RAM (DOOMO_PARTS_SPU, the PS1's CR10) defines its own, which
 * copies block `blk` into `stage` (DMA) and returns it.
 *
 * Only DOOMO_PARTS_LZ builds (PS1) read through it; host unit builds compile it with
 * DOOMO_PARTS_LZ_TEST to check it against the raw arrays. */
#include "doomo/doomo.h"
#include "doomo/parts.h"

#if defined(DOOMO_PARTS_LZ) || defined(DOOMO_PARTS_LZ_TEST)

#define ARENA_ENTRIES 32

static uint8_t  buf[PART_SLOTS][PART_BLK];
static const PartLz *slot_lz[PART_SLOTS];
static uint16_t slot_blk[PART_SLOTS];
static uint32_t slot_use[PART_SLOTS];
static uint32_t use_clock, loads, fills;

static uint8_t arena[PART_ARENA];
#if defined(DOOMO_PARTS_SPU)
static uint8_t stage[PART_BLK + PART_BLK / 128 + 8];     /* a compressed block fetched from SPU RAM */
#define STAGE stage
#define STAGE_CAP ((unsigned)sizeof(stage))
#else
#define STAGE NULL
#define STAGE_CAP 0u
#endif
static struct { const PartLz *z; uint16_t ofs, at; } ae[ARENA_ENTRIES];
static uint16_t arena_used, arena_n;

#if !defined(DOOMO_PARTS_SPU)
const uint8_t *part_lz_src(const PartLz *z, unsigned blk, uint8_t *stage, unsigned cap)
{
    (void)stage; (void)cap;
    return z->z + z->zofs[blk];
}
#endif

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

void part_arena_begin(void)
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
    part_arena_begin();
}

#endif
