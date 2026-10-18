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
    while (lo < hi) {
        int mid = (lo + hi + 1) >> 1;
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

static uint32_t isqrt64(uint64_t v)
{
    uint64_t res = 0, bit = (uint64_t)1 << 62;
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
    return (uint32_t)res;
}

fix16 fx_dist(fix16 dx, fix16 dy)
{
    int64_t x = dx, y = dy;
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
    s = (uint64_t)(x * x) + (uint64_t)(y * y);
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
