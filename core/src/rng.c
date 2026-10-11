/* rng.c - deterministic xorshift32 (13/17/5) seeded through the lowbias32
 * integer hash (owner: foundation). The state is never 0. */
#include "doomo/rng.h"

DoomoRng doomo_rng = { 0x2545F491u };

uint32_t doomo_hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

void rng_seed(DoomoRng *r, uint32_t seed)
{
    uint32_t s = doomo_hash32(seed + 0x9E3779B9u);
    r->s = s ? s : 0x6D2B79F5u;
}

uint32_t rng_next(DoomoRng *r)
{
    uint32_t x = r->s;
    if (x == 0) x = 0x6D2B79F5u;        /* never stuck, even if poked */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

int rng_range(DoomoRng *r, int n)
{
    if (n <= 1) { rng_next(r); return 0; }
    if (n > 65536) n = 65536;
    return (int)(((rng_next(r) >> 16) * (uint32_t)n) >> 16);
}

int rng_between(DoomoRng *r, int lo, int hi)
{
    if (hi < lo) { int t = lo; lo = hi; hi = t; }
    return lo + rng_range(r, hi - lo + 1);
}

int rng_chance(DoomoRng *r, int percent)
{
    return rng_range(r, 100) < percent;
}
