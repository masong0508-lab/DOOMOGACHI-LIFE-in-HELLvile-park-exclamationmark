/* rng.h - deterministic xorshift32 PRNG. Identical on every target so a
 * seed always generates the same resident / map / event. */
#ifndef DOOMO_RNG_H
#define DOOMO_RNG_H

#include "types.h"

typedef struct DoomoRng { uint32_t s; } DoomoRng;

/* Seeds are mixed with doomo_hash32 so 0 and small seeds are fine. */
void     rng_seed(DoomoRng *r, uint32_t seed);
uint32_t rng_next(DoomoRng *r);                 /* xorshift32 13/17/5 */
int      rng_range(DoomoRng *r, int n);         /* 0..n-1, n>0 (n<=65536) */
int      rng_between(DoomoRng *r, int lo, int hi); /* lo..hi inclusive  */
int      rng_chance(DoomoRng *r, int percent);  /* 1 with percent% odds */
uint32_t doomo_hash32(uint32_t x);              /* lowbias32 integer hash */

/* Global gameplay RNG (seeded from plat_entropy() at boot). Never use it
 * for anything that must be reproducible; use a local DoomoRng instead. */
extern DoomoRng doomo_rng;

#endif
