/* art_math.c - integer math, hashing, periodic noise (artgen only).
 * Integer-only and seeded so every machine produces identical art. */
#include "art.h"

/* ---- trig ---------------------------------------------------------------- */
static int16_t sin_tab[1025];       /* quarter wave, Q12, 1024 = 90 deg     */
static int sin_ready;

static void sin_build(void)
{
    /* Taylor series in Q30 for each quarter-wave sample (exact enough and
     * fully deterministic - no libm). */
    int i;
    const int64_t PI_HALF_Q30 = 1686629713LL;     /* pi/2 * 2^30          */
    for (i = 0; i <= 1024; i++) {
        int64_t x = PI_HALF_Q30 * i / 1024;
        int64_t x2 = (x * x) >> 30;
        int64_t term = x, sum = x;
        int k;
        for (k = 1; k < 10; k++) {
            term = -((term * x2) >> 30) / ((2 * k) * (2 * k + 1));
            sum += term;
        }
        sin_tab[i] = (int16_t)((sum + (1 << 17)) >> 18);  /* Q30 -> Q12 */
    }
    sin_ready = 1;
}

int art_sin(int a)
{
    int q, i;
    if (!sin_ready) sin_build();
    a &= ART_TURN - 1;
    q = a >> 10;
    i = a & 1023;
    switch (q) {
    case 0: return sin_tab[i];
    case 1: return sin_tab[1024 - i];
    case 2: return -sin_tab[i];
    default: return -sin_tab[1024 - i];
    }
}

int art_cos(int a) { return art_sin(a + 1024); }

int art_atan2(int y, int x)
{
    /* CORDIC vectoring, angle in 1/256 units of the 4096 turn */
    static const int32_t at[16] = {131072, 77376, 40884, 20753, 10417, 5213,
                                   2607, 1304, 652, 326, 163, 81, 41, 20, 10, 5};
    int64_t X = x, Y = y;
    int32_t ang = 0;
    int i;
    if (x == 0 && y == 0) return 0;
    if (X < 0) {                     /* rotate by 180 deg into right half */
        X = -X; Y = -Y; ang = 2048 * 256;
    }
    X <<= 8; Y <<= 8;
    for (i = 0; i < 16; i++) {
        int64_t nx;
        if (Y > 0) {
            nx = X + (Y >> i); Y = Y - (X >> i); ang += at[i];
        } else {
            nx = X - (Y >> i); Y = Y + (X >> i); ang -= at[i];
        }
        X = nx;
    }
    return ((ang + 128) >> 8) & (ART_TURN - 1);
}

uint32_t art_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}

int art_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
int art_wrap(int v, int m) { int r = v % m; return r < 0 ? r + m : r; }
int art_abs(int v) { return v < 0 ? -v : v; }

int art_smooth(int t)
{
    t = art_clamp(t, 0, Q12);
    return (int)(((int64_t)t * t * (3 * Q12 - 2 * t)) >> 24);
}

int art_lerp(int a, int b, int t) { return a + (int)(((int64_t)(b - a) * t) >> 12); }

int art_dist(int dx, int dy)
{
    return (int)art_isqrt((uint32_t)(dx * dx + dy * dy));
}

/* ---- hashing --------------------------------------------------------------- */
uint32_t art_hash(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^
                 (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du;
    h ^= h >> 12; h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

void art_seed(ArtRng *r, uint32_t seed) { r->s = seed * 2654435761u + 0x6D2B79F5u; }

uint32_t art_next(ArtRng *r)
{
    /* splitmix32-ish */
    uint32_t z = (r->s += 0x9E3779B9u);
    z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
    z = (z ^ (z >> 13)) * 0xC2B2AE35u;
    return z ^ (z >> 16);
}

int art_rnd(ArtRng *r, int n) { return n <= 1 ? 0 : (int)(art_next(r) % (uint32_t)n); }
int art_range(ArtRng *r, int lo, int hi) { return lo + art_rnd(r, hi - lo + 1); }

/* ---- value noise ------------------------------------------------------------- */
static int lattice(int ix, int iy, int nx, int ny, uint32_t seed)
{
    ix = art_wrap(ix, nx);
    iy = art_wrap(iy, ny);
    return (int)(art_hash((uint32_t)ix, (uint32_t)iy, seed) & 255u);
}

int art_noise2(int x, int y, int cell, int px, int py, uint32_t seed)
{
    int nx = px / cell, ny = py / cell;
    int ix = x >= 0 ? x / cell : -((-x + cell - 1) / cell);
    int iy = y >= 0 ? y / cell : -((-y + cell - 1) / cell);
    int fx = ((x - ix * cell) << 12) / cell, fy = ((y - iy * cell) << 12) / cell;
    int a = lattice(ix, iy, nx, ny, seed), b = lattice(ix + 1, iy, nx, ny, seed);
    int c = lattice(ix, iy + 1, nx, ny, seed), d = lattice(ix + 1, iy + 1, nx, ny, seed);
    int sx = art_smooth(fx), sy = art_smooth(fy);
    int top = art_lerp(a, b, sx), bot = art_lerp(c, d, sx);
    if (nx < 1 || ny < 1) return 128;
    return art_lerp(top, bot, sy);
}

int art_noise(int x, int y, int cell, int period, uint32_t seed)
{
    return art_noise2(x, y, cell, period, period, seed);
}

int art_fbm2(int x, int y, int cell, int oct, int px, int py, uint32_t seed)
{
    int sum = 0, w = 128, tw = 0, o;
    for (o = 0; o < oct && cell >= 1; o++) {
        sum += art_noise2(x, y, cell, px, py, seed + (uint32_t)o * 7919u) * w;
        tw += w;
        w >>= 1;
        if (w < 8) w = 8;
        cell >>= 1;
    }
    return tw ? sum / tw : 128;
}

int art_fbm(int x, int y, int cell, int oct, int period, uint32_t seed)
{
    return art_fbm2(x, y, cell, oct, period, period, seed);
}

/* ---- cellular -------------------------------------------------------------------- */
void art_worley_j(int x, int y, int grid, int period, uint32_t seed, int jitter,
                  ArtCell *out)
{
    int cs = period / grid;              /* cell size in px */
    int cx = x >= 0 ? x / cs : -((-x + cs - 1) / cs);
    int cy = y >= 0 ? y / cs : -((-y + cs - 1) / cs);
    int best1 = 0x7FFFFFFF, best2 = 0x7FFFFFFF;
    int i, j;
    out->id = 0; out->fx = x; out->fy = y;
    for (j = -1; j <= 1; j++) {
        for (i = -1; i <= 1; i++) {
            int gx = cx + i, gy = cy + j;
            uint32_t h = art_hash((uint32_t)art_wrap(gx, grid), (uint32_t)art_wrap(gy, grid), seed);
            /* feature point in 1/16 px */
            int ox = (int)(h & 255u), oy = (int)((h >> 8) & 255u);
            int fx16 = gx * cs * 16 + (cs * 8) + ((ox - 128) * cs * jitter) / 256;
            int fy16 = gy * cs * 16 + (cs * 8) + ((oy - 128) * cs * jitter) / 256;
            int dx = fx16 - (x * 16 + 8), dy = fy16 - (y * 16 + 8);
            int d = art_dist(dx, dy);
            if (d < best1) {
                best2 = best1; best1 = d;
                out->id = h;
                out->fx = fx16 / 16; out->fy = fy16 / 16;
            } else if (d < best2) {
                best2 = d;
            }
        }
    }
    out->d1 = best1;
    out->d2 = best2;
}

void art_worley(int x, int y, int grid, int period, uint32_t seed, ArtCell *out)
{
    art_worley_j(x, y, grid, period, seed, 12, out);
}

int art_bayer(int x, int y)
{
    static const uint8_t b[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    return b[y & 3][x & 3];
}
