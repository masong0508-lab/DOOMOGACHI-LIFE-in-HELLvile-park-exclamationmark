/* fixmath.c - integer trig and helpers (owner: foundation).
 * Pure integer code: identical results on GBA, PS1 and native. */
#include "doomo/fixmath.h"

fix16 fx_sin(int angle)
{
    int a = angle & DOOMO_ANG_MASK;
    if (a < 1024) return doomo_sin_q[a];
    if (a < 2048) return doomo_sin_q[2048 - a];
    if (a < 3072) return -doomo_sin_q[a - 2048];
    return -doomo_sin_q[4096 - a];
}

fix16 fx_cos(int angle)
{
    return fx_sin(angle + 1024);
}

/* Angle 0..512 (0..45 deg) whose tangent is num/den, 0 <= num <= den.
 * Binary search on sin(t)*den <= cos(t)*num, done in 64-bit so any input
 * scale works without division. */
static int atan_oct(uint32_t num, uint32_t den)
{
    int lo = 0, hi = 512;
    if (den == 0) return 0;
#if defined(DOOMO_PLATFORM_GBA)
    /* ARM7 THUMB has a cheap 32-bit MUL, but its 64-bit products are
     * library calls. Shift BOTH arguments down until <=65535 (the
     * quarter-wave sine factors never exceed 65536), then most
     * binary-search decisions use exact conservative 32-bit bounds.
     * A 64-bit comparison remains for near-ties, preserving EVERY angle. */
    uint32_t nn = num, dd = den;
    uint32_t high = num > den ? num : den;
    int truncated = 0;
    while (high > 65535u) {
        high >>= 1;
        nn >>= 1;
        dd >>= 1;
        truncated = 1;
    }
#endif
    while (lo < hi) {
        int mid = (lo + hi + 1) >> 1;
#if defined(DOOMO_PLATFORM_GBA)
        uint32_t sinv = (uint32_t)doomo_sin_q[mid];
        uint32_t cosv = (uint32_t)doomo_sin_q[1024 - mid];
        uint32_t sn = sinv * dd, cs = cosv * nn;
        /* The discarded remainder of either argument is <2^shift.
         * sn < cs by at least sinv proves the original sn<=cs;
         * sn > cs by more than cosv proves the opposite. With no
         * discarded bits the 32-bit comparison is EXACT. */
        if (!truncated || (sn < cs && cs - sn >= sinv) ||
            (sn > cs && sn - cs > cosv)) {
            if (sn <= cs) lo = mid; else hi = mid - 1;
            continue;
        }
#endif
        int64_t s = (int64_t)doomo_sin_q[mid] * den;
        int64_t c = (int64_t)doomo_sin_q[1024 - mid] * num;
        if (s <= c) lo = mid; else hi = mid - 1;
    }
    /* round: pick lo or lo+1, whichever tangent is closer */
    if (lo < 512) {
        int64_t e0 = (int64_t)doomo_sin_q[1024 - lo] * num -
                     (int64_t)doomo_sin_q[lo] * den;
        int64_t e1 = (int64_t)doomo_sin_q[lo + 1] * den -
                     (int64_t)doomo_sin_q[1023 - lo] * num;
        if (e1 < e0) lo++;
    }
    return lo;
}

int fx_atan2(int32_t y, int32_t x)
{
    uint32_t ax = x < 0 ? (uint32_t)0 - (uint32_t)x : (uint32_t)x;
    uint32_t ay = y < 0 ? (uint32_t)0 - (uint32_t)y : (uint32_t)y;
    int a;
    if (ax == 0 && ay == 0) return 0;
    if (ay <= ax) a = atan_oct(ay, ax);            /* 0..512    */
    else          a = 1024 - atan_oct(ax, ay);     /* 512..1024 */
    /* a is the angle in the first quadrant; place it */
    if (x >= 0 && y >= 0) return a & DOOMO_ANG_MASK;
    if (x < 0 && y >= 0)  return (2048 - a) & DOOMO_ANG_MASK;
    if (x < 0 && y < 0)   return (2048 + a) & DOOMO_ANG_MASK;
    return (4096 - a) & DOOMO_ANG_MASK;
}

uint32_t fx_isqrt(uint32_t v)
{
    uint32_t res = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= res + bit) {
            v -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

/* floor(sqrt(v)), exactly. (perf) floor(sqrt(v >> 2m)) == floor(sqrt(v) / 2^m), so the top half of the root comes from
 * the 32-bit root of the top 32 bits of v; only the last m bits (m <= 16) are then found one bit pair at a time
 * (instead of 32 bit pairs in 64-bit arithmetic, which is slow on the GBA's 32-bit CPU). tests/test_fixmath checks it
 * against the plain bit-by-bit root. */
static uint32_t isqrt64(uint64_t v)
{
    uint32_t hi = (uint32_t)(v >> 32), q;
    uint64_t rem;
    int sh = 0, n = 0, m;
    if (!hi) return fx_isqrt((uint32_t)v);
    while ((hi >> n) != 0 && n < 32) n++;      /* bit length of v is 32 + n */
    sh = n & 1 ? n + 1 : n;                    /* even shift: v >> sh fits 32 bits (32 + n - sh <= 32) */
    m = sh >> 1;
    q = fx_isqrt((uint32_t)(v >> sh));
    rem = (v >> sh) - (uint64_t)q * q;         /* 0 <= rem <= 2q */
    while (m--) {
        sh -= 2;
        rem = (rem << 2) | ((v >> sh) & 3u);
        if (rem >= ((uint64_t)q << 2) + 1u) { rem -= ((uint64_t)q << 2) + 1u; q = (q << 1) | 1u; }
        else q <<= 1;
    }
    return q;
}

/* ARM7 THUMB: build the exact 64-bit signed-square value from three
 * 32-bit multiplications instead of a software 64-bit multiply. The
 * 16-bit halves are unsigned, so even INT32_MIN is handled exactly.
 * No lookup table, extra RAM, heap or change to other ports. */
#if defined(DOOMO_PLATFORM_GBA)
static uint64_t fx_sq_gba(int32_t v)
{
    uint32_t a = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
    uint32_t hi = a >> 16, lo = a & 65535u;
    return ((uint64_t)(hi * hi) << 32) +
           ((uint64_t)(hi * lo) << 17) + (uint64_t)(lo * lo);
}
#endif

/* fx_dist(dx, dy) < r / > r without the root (exact: floor(sqrt(s)) < r <=> s < r*r; > r <=> s >= (r+1)^2) */
int fx_dist_lt(fix16 dx, fix16 dy, fix16 r)
{
    uint64_t s;
    if (r <= 0) return 0;
#if defined(DOOMO_PLATFORM_GBA)
    /* For the common integer-world radii (collisions, demon melee,
     * projectile hitboxes), first compare 32-bit integer-unit bounds.
     * With u=floor(|dx|/65536), the true |dx| is in [u,u+1), so:
     *   u_x^2 + u_y^2 >= R^2       => definitely OUTSIDE,
     *   (u_x+1)^2+(u_y+1)^2 <= R^2 => definitely INSIDE.
     * Near the boundary, use the exact original 64-bit square test.
     * The bounds use unsigned 32-bit math only when overflow is
     * impossible. They preserve every result and do not change AI,
     * actor ordering, RNG calls, pixels or other ports.
     * 1.3M random and adversarial tests reproduced original booleans. */
    if (((uint32_t)r & 65535u) == 0) {
        uint32_t ax = dx < 0 ? 0u - (uint32_t)dx : (uint32_t)dx;
        uint32_t ay = dy < 0 ? 0u - (uint32_t)dy : (uint32_t)dy;
        uint32_t ux = ax >> 16, uy = ay >> 16, rad = (uint32_t)r >> 16;
        if (ux <= 32766u && uy <= 32766u && rad <= 32767u) {
            uint32_t radius_sq = rad * rad;
            uint32_t low = ux * ux + uy * uy;
            if (low >= radius_sq) return 0;
            if ((ux + 1u) * (ux + 1u) +
                (uy + 1u) * (uy + 1u) <= radius_sq) return 1;
        }
    }
    s = fx_sq_gba(dx) + fx_sq_gba(dy);
    return s < fx_sq_gba(r);
#else
    s = (uint64_t)((int64_t)dx * dx) + (uint64_t)((int64_t)dy * dy);
    return s < (uint64_t)(int64_t)r * (uint64_t)(int64_t)r;
#endif
}

int fx_dist_gt(fix16 dx, fix16 dy, fix16 r)
{
    uint64_t s, t;
    if (r < 0) return 1;
    if (r >= 0x7FFFFFFF) return 0;
#if defined(DOOMO_PLATFORM_GBA)
    s = fx_sq_gba(dx) + fx_sq_gba(dy);
    t = fx_sq_gba(r + 1);
#else
    s = (uint64_t)((int64_t)dx * dx) + (uint64_t)((int64_t)dy * dy);
    t = (uint64_t)(int64_t)(r + 1) * (uint64_t)(int64_t)(r + 1);
#endif
    return s >= t;
}

fix16 fx_dist(fix16 dx, fix16 dy)
{
    uint64_t s;
    uint32_t r;
    /* (perf) both under 46341: the sum fits 32 bits, so the 32-bit root
     * gives the same result in half the steps (and the GBA's THUMB code
     * avoids the 64-bit multiplies and shifts: the raycaster's actors call
     * this for every sprite every frame) */
    if ((uint32_t)(dx + 46340) <= 92680u && (uint32_t)(dy + 46340) <= 92680u) {
        uint32_t ux = (uint32_t)(dx < 0 ? -dx : dx), uy = (uint32_t)(dy < 0 ? -dy : dy);
        return (fix16)fx_isqrt(ux * ux + uy * uy);
    }
#if defined(DOOMO_PLATFORM_GBA)
    s = fx_sq_gba(dx) + fx_sq_gba(dy);
#else
    s = (uint64_t)((int64_t)dx * dx) + (uint64_t)((int64_t)dy * dy);
#endif
    r = isqrt64(s);
    if (r > 0x7FFFFFFFu) r = 0x7FFFFFFFu;
    return (fix16)r;
}

fix16 fx_recip(fix16 x)
{
    int64_t r;
    if (x <= 0) return (fix16)0x7FFFFFFF;
    r = ((int64_t)1 << 32) / x;
    if (r > 0x7FFFFFFF) r = 0x7FFFFFFF;
    return (fix16)r;
}
