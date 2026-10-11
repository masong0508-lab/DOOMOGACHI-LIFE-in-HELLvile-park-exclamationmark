/* et_divtest.c - checks the IWRAM division helpers (src/div.s) against a
 * plain shift-subtract reference at engine-test boot (owner: gba).
 * Logs "ET divtest ok (N cases)" or "ET divtest FAIL ...". */
#include "gba_plat.h"
#include "enginetest.h"

static uint32_t rng = 0x12345678u;
static uint32_t next(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

static uint64_t ref_udiv64(uint64_t n, uint64_t d, uint64_t *r)
{
    uint64_t q = 0, rem = 0;
    int i;
    for (i = 63; i >= 0; i--) {
        rem = (rem << 1) | ((n >> i) & 1);
        if (rem >= d) { rem -= d; q |= (uint64_t)1 << i; }
    }
    *r = rem;
    return q;
}

static int64_t ref_sdiv64(int64_t n, int64_t d, int64_t *r)
{
    uint64_t un = n < 0 ? (uint64_t)0 - (uint64_t)n : (uint64_t)n;
    uint64_t ud = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d, ur;
    uint64_t q = ref_udiv64(un, ud, &ur);
    *r = n < 0 ? -(int64_t)ur : (int64_t)ur;
    return (n < 0) != (d < 0) ? -(int64_t)q : (int64_t)q;
}

/* operands of assorted magnitudes */
static uint32_t pick(void)
{
    uint32_t v = next();
    switch (next() & 7) {
    case 0: return v & 0xF;
    case 1: return v & 0xFF;
    case 2: return v & 0xFFF;
    case 3: return v & 0xFFFF;
    case 4: return v >> (next() & 31);
    case 5: return (uint32_t)-(int32_t)(v & 0xFFF);
    case 6: return 0x80000000u | (v & 7);
    default: return v;
    }
}

void et_div_test(void)
{
    volatile uint32_t a, b;
    volatile int64_t la, lb;
    int i, n = 0;
    for (i = 0; i < 3000; i++) {
        uint32_t x = pick(), y = pick();
        uint64_t ur;
        int64_t sr;
        if (!y) y = 1 + (next() & 3);
        a = x; b = y;
        /* unsigned 32 */
        {
            uint32_t q = ref_udiv64(x, y, &ur);
            if (a / b != q || a % b != (uint32_t)ur) goto fail_u;
        }
        /* signed 32 (skip INT_MIN / -1) */
        if (!(x == 0x80000000u && y == 0xFFFFFFFFu)) {
            int32_t q = (int32_t)ref_sdiv64((int32_t)x, (int32_t)y, &sr);
            if ((int32_t)a / (int32_t)b != q || (int32_t)a % (int32_t)b != (int32_t)sr)
                goto fail_s;
        }
        /* signed 64: 32-bit range and wide values */
        {
            int64_t nx = (int32_t)x, ny = (int32_t)y, q;
            if (i & 1) nx = (int64_t)(((uint64_t)next() << 32) | x);
            if ((i & 3) == 3) ny = (int64_t)(((uint64_t)(next() & 0xFF) << 32) | y);
            if (!ny) ny = 3;
            la = nx; lb = ny;
            q = ref_sdiv64(nx, ny, &sr);
            if (la / lb != q || la % lb != sr) goto fail_l;
        }
        n++;
    }
    /* division by zero must not hang or crash */
    a = 1234; b = 0;
    (void)(a / b);
    (void)((int32_t)a / (int32_t)b);
    gba_logf("ET divtest ok (%d cases)", n);
    return;
fail_u:
    gba_logf("ET divtest FAIL u32 %u / %u", (unsigned)a, (unsigned)b);
    return;
fail_s:
    gba_logf("ET divtest FAIL s32 %d / %d", (int)(int32_t)a, (int)(int32_t)b);
    return;
fail_l:
    gba_logf("ET divtest FAIL s64 %08x%08x / %08x%08x",
             (unsigned)((uint64_t)la >> 32), (unsigned)la,
             (unsigned)((uint64_t)lb >> 32), (unsigned)lb);
}
