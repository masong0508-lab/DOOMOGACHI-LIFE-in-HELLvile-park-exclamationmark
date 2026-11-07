/* test_math.c - rng and fixmath (owner: foundation). Golden values were
 * computed with an independent Python model of xorshift32 + lowbias32. */
#include "test.h"

TEST(rng_golden_values)
{
    static const struct { uint32_t seed, v[5]; } g[] = {
        { 0, { 0x378B94D9u, 0xE7025C71u, 0x3D096CD7u, 0x02E3175Eu, 0x61140C1Au } },
        { 1, { 0x2745031Fu, 0x63F2B10Cu, 0x8FB65745u, 0xEE8468AAu, 0x11B23522u } },
        { 42, { 0x14353FE3u, 0xCAE9C6E7u, 0xB49EA05Du, 0x723332F7u, 0x99D6C0E1u } },
        { 0xDEADBEEFu, { 0xA6C4033Fu, 0xF2DDFDAEu, 0xE06B785Au, 0x636F82AAu, 0xF46BEDD7u } }
    };
    size_t i, k;
    for (i = 0; i < sizeof(g) / sizeof(g[0]); i++) {
        DoomoRng r;
        rng_seed(&r, g[i].seed);
        for (k = 0; k < 5; k++) CHECK_EQ_U(rng_next(&r), g[i].v[k]);
    }
}

TEST(rng_hash_golden)
{
    CHECK_EQ_U(doomo_hash32(0), 0x00000000u);
    CHECK_EQ_U(doomo_hash32(1), 0x688990C0u);
    CHECK_EQ_U(doomo_hash32(2), 0xD1132181u);
    CHECK_EQ_U(doomo_hash32(12345), 0x912EFCF7u);
    CHECK_EQ_U(doomo_hash32(0xFFFFFFFFu), 0x6768824Au);
}

TEST(rng_range_golden_and_bounds)
{
    static const int want[8] = { 37, 61, 85, 3, 4, 87, 88, 48 };
    DoomoRng r;
    int i, hist[10];
    rng_seed(&r, 7);
    for (i = 0; i < 8; i++) CHECK_EQ(rng_range(&r, 100), want[i]);
    memset(hist, 0, sizeof(hist));
    for (i = 0; i < 100000; i++) {
        int v = rng_range(&r, 10), b = rng_between(&r, -3, 3);
        CHECK(v >= 0 && v < 10);
        CHECK(b >= -3 && b <= 3);
        hist[v]++;
    }
    for (i = 0; i < 10; i++) CHECK_MSG(hist[i] > 9000 && hist[i] < 11000, "bucket %d = %d", i, hist[i]);
    CHECK_EQ(rng_range(&r, 1), 0);
    CHECK(rng_range(&r, 65536) < 65536);
    for (i = 0; i < 1000; i++) CHECK(rng_chance(&r, 0) == 0 && rng_chance(&r, 100) == 1);
}

TEST(rng_state_never_zero)
{
    DoomoRng r;
    uint32_t s;
    for (s = 0; s < 100000; s++) {
        rng_seed(&r, s);
        CHECK(r.s != 0);
    }
    r.s = 0;                         /* poked to zero: must recover */
    CHECK(rng_next(&r) != 0);
}

TEST(fixmath_sin_cos_accuracy)
{
    int a;
    CHECK_EQ(fx_sin(0), 0);
    CHECK_EQ(fx_sin(1024), 65536);
    CHECK_EQ(fx_sin(2048), 0);
    CHECK_EQ(fx_sin(3072), -65536);
    CHECK_EQ(fx_cos(0), 65536);
    CHECK_EQ(fx_sin(-1024), -65536);
    CHECK_EQ(fx_sin(4096 + 1024), 65536);
    for (a = 0; a < 4096; a++) {
        int64_t s = fx_sin(a), c = fx_cos(a);
        int64_t one = (s * s + c * c) >> 16;
        CHECK_MSG(one >= 65536 - 8 && one <= 65536 + 8, "sin^2+cos^2 at %d = %lld", a, (long long)one);
        CHECK_EQ(fx_sin(a), -fx_sin(4096 - a));        /* odd */
        CHECK_EQ(fx_sin(a), fx_cos(a - 1024));
    }
    /* sin 45 deg = 0.70711 */
    CHECK(fx_sin(512) >= 46340 && fx_sin(512) <= 46342);
}

TEST(fixmath_atan2)
{
    int a;
    CHECK_EQ(fx_atan2(0, 1), 0);
    CHECK_EQ(fx_atan2(1, 0), 1024);
    CHECK_EQ(fx_atan2(0, -1), 2048);
    CHECK_EQ(fx_atan2(-1, 0), 3072);
    CHECK_EQ(fx_atan2(0, 0), 0);
    for (a = 0; a < 4096; a += 3) {
        int32_t r[3] = { 1000, 65536, 30000000 };
        int k;
        for (k = 0; k < 3; k++) {
            int32_t x = (int32_t)(((int64_t)fx_cos(a) * r[k]) >> 16);
            int32_t y = (int32_t)(((int64_t)fx_sin(a) * r[k]) >> 16);
            int got = fx_atan2(y, x), d = (got - a) & 4095;
            if (d > 2048) d = 4096 - d;
            CHECK_MSG(d <= (k == 0 ? 4 : 2), "atan2 angle %d radius %d -> %d", a, (int)r[k], got);
        }
    }
}

TEST(fixmath_isqrt_dist_recip)
{
    uint32_t v;
    for (v = 0; v < 70000; v++) {
        uint32_t s = fx_isqrt(v);
        CHECK(s * s <= v && (s + 1) * (s + 1) > v);
    }
    CHECK_EQ_U(fx_isqrt(0xFFFFFFFFu), 65535u);
    CHECK_EQ(fx_dist(INT2FIX(3), INT2FIX(4)), INT2FIX(5));
    CHECK_EQ(fx_dist(INT2FIX(-30000), INT2FIX(0)), INT2FIX(30000));
    {
        fix16 d = fx_dist(INT2FIX(20000), INT2FIX(-20000));
        /* 28284.27 */
        CHECK(d >= INT2FIX(28284) && d <= INT2FIX(28285));
        /* results beyond 16.16 range clamp instead of wrapping */
        CHECK_EQ(fx_dist(INT2FIX(32000), INT2FIX(32000)), 0x7FFFFFFF);
    }
    CHECK_EQ(fx_recip(FIX_ONE), FIX_ONE);
    CHECK_EQ(fx_recip(INT2FIX(2)), FIX_HALF);
    CHECK_EQ(fx_recip(0), 0x7FFFFFFF);
    CHECK_EQ(fx_recip(1), 0x7FFFFFFF);
}

/* the plain bit-by-bit 64-bit root fx_dist used before the split root (the reference the fast one must equal) */
static fix16 ref_dist(fix16 dx, fix16 dy)
{
    int64_t x = dx, y = dy;
    uint64_t v = (uint64_t)(x * x) + (uint64_t)(y * y), res = 0, bit = (uint64_t)1 << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= res + bit) { v -= res + bit; res = (res >> 1) + bit; } else res >>= 1;
        bit >>= 2;
    }
    if (res > 0x7FFFFFFFu) res = 0x7FFFFFFFu;
    return (fix16)res;
}

TEST(fixmath_dist_split_root_is_exact)
{
    uint64_t st = 0x9E3779B97F4A7C15ull;
    int i, k;
    for (i = 0; i < 400000; i++) {
        fix16 dx, dy, r;
        st = st * 6364136223846793005ull + 1442695040888963407ull;
        k = (int)(st >> 60);                                   /* magnitude class 0..15: tiny .. +-2^31 */
        dx = (fix16)((int32_t)(st >> 8) >> (k * 2 > 31 ? 31 : k * 2));
        st = st * 6364136223846793005ull + 1442695040888963407ull;
        dy = (fix16)((int32_t)(st >> 8) >> (((st >> 60) * 2) > 31 ? 31 : (int)((st >> 60) * 2)));
        if (i < 4000) { dx = (fix16)(i * 1000003u); dy = (fix16)(i * 7919u); }
        if (fx_dist(dx, dy) != ref_dist(dx, dy)) { CHECK_MSG(0, "fx_dist(%d,%d) = %d, reference %d", (int)dx, (int)dy, (int)fx_dist(dx, dy), (int)ref_dist(dx, dy)); break; }
        /* compare helpers equal the root compare, on radii around the true distance and some far ones */
        {
            fix16 d = ref_dist(dx, dy);
            fix16 rs[7];
            int j;
            st = st * 6364136223846793005ull + 1442695040888963407ull;
            rs[0] = d; rs[1] = d - 1; rs[2] = d + 1; rs[3] = 0; rs[4] = -5; rs[5] = (fix16)(st >> 33); rs[6] = 0x7FFFFFFF;
            for (j = 0; j < 7; j++) {
                r = rs[j];
                if (fx_dist_lt(dx, dy, r) != (fx_dist(dx, dy) < r)) { CHECK_MSG(0, "lt(%d,%d,%d)", (int)dx, (int)dy, (int)r); break; }
                if (fx_dist_gt(dx, dy, r) != (fx_dist(dx, dy) > r)) { CHECK_MSG(0, "gt(%d,%d,%d)", (int)dx, (int)dy, (int)r); break; }
            }
        }
    }
    CHECK_EQ(fx_dist(0, 0), 0);
    CHECK_EQ(fx_dist(INT2FIX(1), 0), INT2FIX(1));
    CHECK_EQ(fx_dist(INT2FIX(30000), INT2FIX(30000)), ref_dist(INT2FIX(30000), INT2FIX(30000)));
}
