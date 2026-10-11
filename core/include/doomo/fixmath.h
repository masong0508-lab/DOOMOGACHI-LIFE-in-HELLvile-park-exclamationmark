/* fixmath.h - integer trig and helpers shared by every target. */
#ifndef DOOMO_FIXMATH_H
#define DOOMO_FIXMATH_H

#include "types.h"

/* sin/cos for angles 0..4095 (masked), result is 16.16 in [-1, 1]. */
fix16 fx_sin(int angle);
fix16 fx_cos(int angle);
/* atan2 returning a 0..4095 angle. Arguments may be any scale. */
int   fx_atan2(int32_t y, int32_t x);
/* Integer square root of an unsigned 32-bit value. */
uint32_t fx_isqrt(uint32_t v);
/* Approximate distance |(dx,dy)| for 16.16 inputs (result 16.16).
 * Must not overflow for |dx|,|dy| < 32768.0 */
fix16 fx_dist(fix16 dx, fix16 dy);
/* 1/x reciprocal table helper for renderers: returns 65536*65536/x
 * clamped, for x in 16.16 > 0. */
fix16 fx_recip(fix16 x);

/* The raw 1024-entry quarter-wave table is exposed for renderers that
 * want to inline lookups: doomo_sin_q[i] = sin(i * 90deg / 1024) in 16.16,
 * i in 0..1024 inclusive. */
extern const int32_t doomo_sin_q[1025];

#endif
