/* part_lru.c - the PS1's LZ part-script blocks behind an 8 KB LRU (customisation v2, P1.M1; SPEC 4.4).
 *
 * The part scripts are kept LZ-compressed in 2 KB blocks (parts.h PartLz, tools/parts_build.py) and a
 * block is decoded into one of four slots when an entry in it is first run, the least recently used
 * slot being replaced. Only DOOMO_PARTS_LZ builds (PS1) read through it; host unit builds compile it
 * with DOOMO_PARTS_LZ_TEST to check it against the raw arrays. */
#include "doomo/doomo.h"
#include "doomo/parts.h"

#if defined(DOOMO_PARTS_LZ) || defined(DOOMO_PARTS_LZ_TEST)

#define PL_SLOTS 4

static uint8_t  buf[PL_SLOTS][PART_BLK];
static const PartLz *slot_lz[PL_SLOTS];
static uint16_t slot_blk[PL_SLOTS];
static uint32_t slot_use[PL_SLOTS];
static uint32_t use_clock, loads;

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
    for (i = 0; i < PL_SLOTS; i++)
        if (slot_lz[i] == z && slot_blk[i] == b) {
            slot_use[i] = use_clock;
            return buf[i] + (ofs - z->rofs[b]);
        }
    for (i = 1; i < PL_SLOTS; i++)
        if (slot_use[i] < slot_use[v]) v = i;
    n = (unsigned)(z->rofs[b + 1] - z->rofs[b]);
    if (n > PART_BLK) n = PART_BLK;
    unlz(z->z + z->zofs[b], buf[v], n);
    slot_lz[v] = z;
    slot_blk[v] = (uint16_t)b;
    slot_use[v] = use_clock;
    loads++;
    return buf[v] + (ofs - z->rofs[b]);
}

uint32_t part_lru_loads(void) { return loads; }

void part_lru_reset(void)
{
    int i;
    for (i = 0; i < PL_SLOTS; i++) { slot_lz[i] = NULL; slot_use[i] = 0; }
    loads = 0;
}

#endif
