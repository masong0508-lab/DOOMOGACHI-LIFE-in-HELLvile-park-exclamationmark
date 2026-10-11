/* libgcc_r3000.c - R3000-safe replacements for the libgcc helpers the core
 * needs (owner: ps1).
 *
 * The libgcc.a shipped with Debian/Ubuntu's mipsel-linux-gnu-gcc is built
 * for MIPS32R2 and its 64-bit division uses CLZ, which the PlayStation's
 * R3000A does not have (Reserved Instruction exception). The core's
 * FIXDIV() divides int64_t values, so we provide our own __divdi3 & co.
 * Objects are linked before libgcc.a, so these definitions win; the
 * post-build ISA check (tools/check_isa.py) guarantees no MIPS32-only
 * instruction slipped in from anywhere else.
 *
 * Everything is written with explicit 32-bit halves so the compiler never
 * calls back into a 64-bit helper.
 */
#include <stdint.h>

typedef union {
    uint64_t u;
    int64_t  s;
    struct { uint32_t lo, hi; } w;     /* little endian */
} DW;

static inline int clz32(uint32_t x)
{
    int n = 0;
    if (!x) return 32;
    if (!(x & 0xffff0000u)) { n += 16; x <<= 16; }
    if (!(x & 0xff000000u)) { n += 8;  x <<= 8; }
    if (!(x & 0xf0000000u)) { n += 4;  x <<= 4; }
    if (!(x & 0xc0000000u)) { n += 2;  x <<= 2; }
    if (!(x & 0x80000000u)) { n += 1; }
    return n;
}

static inline int clz64(DW v)
{
    return v.w.hi ? clz32(v.w.hi) : 32 + clz32(v.w.lo);
}

static inline DW shl(DW v, int s)
{
    if (s >= 32) { v.w.hi = v.w.lo << (s - 32); v.w.lo = 0; }
    else if (s > 0) { v.w.hi = (v.w.hi << s) | (v.w.lo >> (32 - s)); v.w.lo <<= s; }
    return v;
}

static inline int geu(DW a, DW b)
{
    return a.w.hi > b.w.hi || (a.w.hi == b.w.hi && a.w.lo >= b.w.lo);
}

static inline DW sub(DW a, DW b)
{
    DW r;
    r.w.lo = a.w.lo - b.w.lo;
    r.w.hi = a.w.hi - b.w.hi - (a.w.lo < b.w.lo);
    return r;
}

static inline DW neg(DW a)
{
    DW z;
    z.w.lo = 0;
    z.w.hi = 0;
    return sub(z, a);
}

/* unsigned 64-bit division: shift-subtract over the significant bits only */
static DW udivmod(DW n, DW d, DW *rem)
{
    DW q;
    int s, i;
    q.w.lo = q.w.hi = 0;
    if (!d.w.hi && !n.w.hi) {
        if (d.w.lo) {
            q.w.lo = n.w.lo / d.w.lo;
            if (rem) { rem->w.lo = n.w.lo % d.w.lo; rem->w.hi = 0; }
        } else if (rem) {
            *rem = n;
        }
        return q;
    }
    if (!d.w.hi && !d.w.lo) {           /* division by zero: like hardware */
        if (rem) *rem = n;
        q.w.lo = q.w.hi = 0xffffffffu;
        return q;
    }
    if (!geu(n, d)) {
        if (rem) *rem = n;
        return q;
    }
    s = clz64(d) - clz64(n);
    d = shl(d, s);
    for (i = 0; i <= s; i++) {
        q = shl(q, 1);
        if (geu(n, d)) {
            n = sub(n, d);
            q.w.lo |= 1;
        }
        /* d >>= 1 */
        d.w.lo = (d.w.lo >> 1) | (d.w.hi << 31);
        d.w.hi >>= 1;
    }
    if (rem) *rem = n;
    return q;
}

uint64_t __udivdi3(uint64_t a, uint64_t b);
uint64_t __umoddi3(uint64_t a, uint64_t b);
int64_t  __divdi3(int64_t a, int64_t b);
int64_t  __moddi3(int64_t a, int64_t b);
uint64_t __udivmoddi4(uint64_t a, uint64_t b, uint64_t *c);
int64_t  __divmoddi4(int64_t a, int64_t b, int64_t *c);
int64_t  __muldi3(int64_t a, int64_t b);
int64_t  __ashldi3(int64_t a, int b);
int64_t  __ashrdi3(int64_t a, int b);
int64_t  __lshrdi3(int64_t a, int b);
int64_t  __negdi2(int64_t a);
int      __cmpdi2(int64_t a, int64_t b);
int      __ucmpdi2(uint64_t a, uint64_t b);
int      __clzsi2(uint32_t a);
int      __ctzsi2(uint32_t a);

uint64_t __udivmoddi4(uint64_t a, uint64_t b, uint64_t *c)
{
    DW n, d, r, q;
    n.u = a; d.u = b;
    q = udivmod(n, d, &r);
    if (c) *c = r.u;
    return q.u;
}

uint64_t __udivdi3(uint64_t a, uint64_t b)
{
    DW n, d, q;
    n.u = a; d.u = b;
    q = udivmod(n, d, 0);
    return q.u;
}

uint64_t __umoddi3(uint64_t a, uint64_t b)
{
    DW n, d, r;
    n.u = a; d.u = b;
    udivmod(n, d, &r);
    return r.u;
}

int64_t __divdi3(int64_t a, int64_t b)
{
    DW n, d, q;
    int negq = 0;
    n.s = a; d.s = b;
    if ((int32_t)n.w.hi < 0) { n = neg(n); negq ^= 1; }
    if ((int32_t)d.w.hi < 0) { d = neg(d); negq ^= 1; }
    q = udivmod(n, d, 0);
    if (negq) q = neg(q);
    return q.s;
}

int64_t __moddi3(int64_t a, int64_t b)
{
    DW n, d, r;
    int negr = 0;
    n.s = a; d.s = b;
    if ((int32_t)n.w.hi < 0) { n = neg(n); negr = 1; }
    if ((int32_t)d.w.hi < 0) d = neg(d);
    udivmod(n, d, &r);
    if (negr) r = neg(r);
    return r.s;
}

int64_t __divmoddi4(int64_t a, int64_t b, int64_t *c)
{
    int64_t q = __divdi3(a, b);
    if (c) *c = __moddi3(a, b);
    return q;
}

/* 64x64 -> 64 multiply from 32x32 -> 64 multiplies (multu) */
int64_t __muldi3(int64_t a, int64_t b)
{
    DW x, y, r;
    uint32_t al, ah, bl, bh;
    x.s = a; y.s = b;
    al = x.w.lo; ah = x.w.hi; bl = y.w.lo; bh = y.w.hi;
    {
        /* low x low: full 64-bit product via 16-bit pieces (no multu
         * intrinsic in C99 without 64-bit arithmetic) */
        uint32_t a0 = al & 0xffff, a1 = al >> 16, b0 = bl & 0xffff, b1 = bl >> 16;
        uint32_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
        uint32_t mid = (p00 >> 16) + (p01 & 0xffff) + (p10 & 0xffff);
        r.w.lo = (p00 & 0xffff) | (mid << 16);
        r.w.hi = p11 + (p01 >> 16) + (p10 >> 16) + (mid >> 16);
    }
    r.w.hi += al * bh + ah * bl;
    return r.s;
}

int64_t __ashldi3(int64_t a, int b)
{
    DW v;
    v.s = a;
    v = shl(v, b & 63);
    return v.s;
}

int64_t __lshrdi3(int64_t a, int b)
{
    DW v;
    v.s = a;
    b &= 63;
    if (b >= 32) { v.w.lo = v.w.hi >> (b - 32); v.w.hi = 0; }
    else if (b > 0) { v.w.lo = (v.w.lo >> b) | (v.w.hi << (32 - b)); v.w.hi >>= b; }
    return v.s;
}

int64_t __ashrdi3(int64_t a, int b)
{
    DW v;
    v.s = a;
    b &= 63;
    if (b >= 32) {
        v.w.lo = (uint32_t)((int32_t)v.w.hi >> (b - 32));
        v.w.hi = (uint32_t)((int32_t)v.w.hi >> 31);
    } else if (b > 0) {
        v.w.lo = (v.w.lo >> b) | (v.w.hi << (32 - b));
        v.w.hi = (uint32_t)((int32_t)v.w.hi >> b);
    }
    return v.s;
}

int64_t __negdi2(int64_t a)
{
    DW v;
    v.s = a;
    v = neg(v);
    return v.s;
}

int __cmpdi2(int64_t a, int64_t b)
{
    DW x, y;
    x.s = a; y.s = b;
    if ((int32_t)x.w.hi < (int32_t)y.w.hi) return 0;
    if ((int32_t)x.w.hi > (int32_t)y.w.hi) return 2;
    if (x.w.lo < y.w.lo) return 0;
    if (x.w.lo > y.w.lo) return 2;
    return 1;
}

int __ucmpdi2(uint64_t a, uint64_t b)
{
    DW x, y;
    x.u = a; y.u = b;
    if (x.w.hi < y.w.hi) return 0;
    if (x.w.hi > y.w.hi) return 2;
    if (x.w.lo < y.w.lo) return 0;
    if (x.w.lo > y.w.lo) return 2;
    return 1;
}

int __clzsi2(uint32_t a) { return clz32(a); }

int __ctzsi2(uint32_t a)
{
    int n = 0;
    if (!a) return 32;
    while (!(a & 1)) { a >>= 1; n++; }
    return n;
}
