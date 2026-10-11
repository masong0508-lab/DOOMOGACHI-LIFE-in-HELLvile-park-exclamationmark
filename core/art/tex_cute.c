/* tex_cute.c - the CUTE texture set: HaHaland before the invasion.
 *
 * Pastel plush candy-land: soft two-tone grass with tiny flowers, pastel
 * bricks with glossy bevels, candy-cane pillars, scalloped awnings, heart
 * wallpaper, stained glass, puffy clouds. Every texture tiles seamlessly
 * (all drawing goes through wrapping ArtImg coordinates and periodic
 * noise) and is shaded from the top-left like the sprites.
 */
#include "art.h"

#define PX(x, y, c) img_put(im, (x), (y), (uint8_t)(c))
#define GX(x, y) img_get(im, (x), (y))

/* ---- grass family ---------------------------------------------------------- */
static void grass_base(ArtImg *im, uint32_t seed, int ticks)
{
    int x, y, i, n, xs[96], ys[96];
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int v = art_fbm(x, y, 16, 3, 64, seed);
            int w = art_noise(x, y, 8, 64, seed + 9);
            uint8_t c = RP(RAMP_GRASS, 4);
            if (v > 140) c = RP(RAMP_GRASS, 5);
            if (v > 140 && v < 146 && w > 128) c = RP(RAMP_GRASS, 4);
            if (v < 92) c = RP(RAMP_GRASS, 3);
            PX(x, y, c);
        }
    /* little grass ticks: dark 'v' with a light tip, in the local tone */
    n = tx_scatter(ticks, 64, 64, seed + 1, xs, ys);
    for (i = 0; i < n; i++) {
        int tx = xs[i], ty = ys[i];
        uint8_t b = GX(tx, ty), d = art_darker(b, 1), l = art_lighter(b, 1);
        PX(tx, ty, d);
        PX(tx - 1, ty - 1, d);
        PX(tx + 1, ty - 1, d);
        PX(tx - 1, ty - 2, l);
        if (i & 1) PX(tx + 1, ty - 2, l);
    }
}

static void t_grass(ArtImg *im, int f, int n)
{
    int i, k, xs[16], ys[16];
    (void)f; (void)n;
    grass_base(im, 0x6A55u, 40);
    /* dew sparkles and a couple of tiny daisies */
    k = tx_scatter(6, 64, 64, 0x6A56u, xs, ys);
    for (i = 0; i < k; i++) {
        if (i < 2) tx_flower(im, xs[i], ys[i], PAL_WHITE, PP(PAINT_WHITE, 0), RP(RAMP_CANDY, 5));
        else PX(xs[i], ys[i], RP(RAMP_GRASS, 7));
    }
}

static void t_flowers(ArtImg *im, int f, int n)
{
    static const int petals[6][2] = {
        {PAINT_BUBBLEGUM, PAINT_ROSE}, {PAINT_WHITE, PAINT_SILVER}, {PAINT_LEMON, PAINT_HONEY},
        {PAINT_LAVENDER, PAINT_GRAPE}, {PAINT_PEACH, PAINT_TANGERINE}, {PAINT_SKY, PAINT_COBALT}};
    int i, j, k, xs[32], ys[32];
    (void)f; (void)n;
    grass_base(im, 0xF10Bu, 24);
    k = tx_scatter(14, 64, 64, 0xF10Cu, xs, ys);
    for (i = 0; i < k; i++) {
        uint32_t h = art_hash((uint32_t)i, 3u, 0xF1u);
        int pi = (int)(h % 6u), cnt = 2 + (int)((h >> 4) % 3u);
        for (j = 0; j < cnt; j++) {
            int fx = xs[i] + (int)((h >> (8 + j * 4)) % 7u) - 3;
            int fy = ys[i] + (int)((h >> (10 + j * 4)) % 5u) - 2;
            uint8_t p = PP(petals[pi][0], j == 0 ? 2 : 1);
            uint8_t pd = PP(petals[pi][1], 1);
            /* leaf pair under the bloom */
            PX(fx - 1, fy + 2, RP(RAMP_GRASS, 3));
            PX(fx + 1, fy + 2, RP(RAMP_GRASS, 3));
            tx_flower(im, fx, fy, p, pd, (pi == 2) ? RP(RAMP_CANDY, 2) : RP(RAMP_CANDY, 5));
        }
    }
}

/* ---- ground ------------------------------------------------------------------ */
static void pebble(ArtImg *im, int x, int y, int rx, int ry, int ramp, int lo)
{
    /* soft shadow first, then the shaded pebble */
    int i;
    for (i = -rx; i <= rx; i++) img_shift(im, x + i + 1, y + ry + 1, -1);
    tx_ball(im, x, y, rx, ry, ramp, lo, lo + 3);
}

static void t_path(ArtImg *im, int f, int n)
{
    int x, y, i, k, xs[32], ys[32];
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int v = art_fbm(x, y, 16, 3, 64, 0xDA7u);
            uint8_t c = RP(RAMP_PEACH, 5);
            if (v > 152) c = RP(RAMP_PEACH, 6);
            else if (v < 96) c = RP(RAMP_PEACH, 4);
            PX(x, y, c);
        }
    /* faint tracks of darker speckle along the noise valleys */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            if ((art_hash((uint32_t)x, (uint32_t)y, 0xDA8u) & 63u) == 0) img_shift(im, x, y, -1);
    k = tx_scatter(16, 64, 64, 0xDA9u, xs, ys);
    for (i = 0; i < k; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0xDAAu);
        int rx = 1 + (int)(h % 2u), ry = 1;
        if ((h >> 3) % 3u == 0) pebble(im, xs[i], ys[i], rx, ry, RAMP_STONE, 3);
        else if ((h >> 3) % 3u == 1) pebble(im, xs[i], ys[i], rx, ry, RAMP_PEACH, 3);
        else { PX(xs[i], ys[i], RP(RAMP_PEACH, 7)); PX(xs[i] + 1, ys[i] + 1, RP(RAMP_PEACH, 4)); }
    }
}

static void t_cobble(ArtImg *im, int f, int n)
{
    static const int ramps[10] = {RAMP_STONE, RAMP_STONE, RAMP_STONE, RAMP_STONE, RAMP_STONE,
                                  RAMP_STONE, RAMP_PASTEL_PINK, RAMP_PEACH, RAMP_STONE, RAMP_VAPE};
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            ArtCell c;
            int edge, dx, dy, base, s, ramp;
            art_worley_j(x, y, 5, 64, 0xC0B1u, 10, &c);
            edge = c.d2 - c.d1;
            if (edge < 22) {
                /* soft mortar with a little moss */
                int m = art_noise(x, y, 16, 64, 0xC0B2u);
                PX(x, y, m > 150 ? RP(RAMP_MINT, 4) : RP(RAMP_STONE, 4));
                continue;
            }
            ramp = ramps[(c.id >> 3) % 10u];
            base = ramp == RAMP_STONE ? 5 + (int)((c.id >> 7) % 2u) : (ramp == RAMP_VAPE ? 6 : 5);
            dx = x - c.fx; dy = y - c.fy;
            s = base;
            if (edge < 40) s += (dx + dy < 0) ? 1 : -1;
            else if (dx + dy < -5 && dx + dy > -9 && art_abs(dx - dy) < 4) s = base + 1;
            if (edge >= 40 && dx + dy < -6 && dx + dy > -8 && art_abs(dx - dy) < 2) s = base + 2;
            PX(x, y, RP(ramp, s));
        }
}

static void t_sand(ArtImg *im, int f, int n)
{
    int x, y, i, k, xs[16], ys[16];
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int v = art_fbm(x, y, 32, 2, 64, 0x5A2Du);
            int ph = y * ART_TURN / 16 + art_sin(x * ART_TURN / 64 + v * 8) / 3 + v * 4;
            int r = art_sin(ph);
            uint8_t c = v > 140 ? RP(RAMP_CANDY, 7) : RP(RAMP_PEACH, 6);
            if (r > 3600) c = RP(RAMP_PEACH, 5);
            else if (r < -3700) c = RP(RAMP_PEACH, 7);
            PX(x, y, c);
        }
    k = tx_scatter(9, 64, 64, 0x5A2Eu, xs, ys);
    for (i = 0; i < k; i++) {
        int sx = xs[i], sy = ys[i];
        if (i % 3 == 0) {          /* tiny pink shell */
            PX(sx, sy, PP(PAINT_BUBBLEGUM, 2)); PX(sx + 1, sy, PP(PAINT_BUBBLEGUM, 1));
            PX(sx - 1, sy, PP(PAINT_BUBBLEGUM, 1));
            PX(sx, sy + 1, PP(PAINT_ROSE, 1)); PX(sx + 1, sy + 1, RP(RAMP_PEACH, 4));
        } else if (i % 3 == 1) {   /* sparkle */
            PX(sx, sy, PAL_WHITE);
        } else {
            PX(sx, sy, RP(RAMP_PEACH, 4)); PX(sx + 1, sy, RP(RAMP_PEACH, 4));
        }
    }
}

/* periodic animated caustics: moving feature points on circles */
static int caustic(int x, int y, int ph, uint32_t seed)
{
    int gx, gy, best1 = 1 << 30, best2 = 1 << 30, cx = x / 16, cy = y / 16;
    for (gy = cy - 1; gy <= cy + 1; gy++)
        for (gx = cx - 1; gx <= cx + 1; gx++) {
            uint32_t h = art_hash((uint32_t)art_wrap(gx, 4), (uint32_t)art_wrap(gy, 4), seed);
            int a = ph + (int)(h & 4095u), r = 4 + (int)((h >> 12) % 3u);
            int fx = gx * 16 + 8 + (int)((h >> 16) % 7u) - 3 + r * art_cos(a) / Q12;
            int fy = gy * 16 + 8 + (int)((h >> 20) % 7u) - 3 + r * art_sin(a) / Q12;
            int d = (x - fx) * (x - fx) + (y - fy) * (y - fy);
            if (d < best1) { best2 = best1; best1 = d; }
            else if (d < best2) best2 = d;
        }
    return (int)art_isqrt((uint32_t)best2) - (int)art_isqrt((uint32_t)best1);
}

static void t_water(ArtImg *im, int f, int n)
{
    int x, y, ph = tx_phase(f, n);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int deep = art_fbm(x, y, 16, 2, 64, 0x3A7Fu);
            int brk = art_noise(x, y, 8, 64, 0x3A81u);
            /* wavy horizontal ripples, displaced sideways over time */
            int a = y * ART_TURN / 8 + art_sin(x * ART_TURN / 32 + ph) / 3 +
                    art_sin(x * ART_TURN / 64 * 3 - y * ART_TURN / 32 + ph) / 5 + ph;
            int w = art_sin(a);
            int e = caustic(x, y, ph, 0x3A7Eu);
            uint8_t c = deep > 120 ? RP(RAMP_SKY, 5) : RP(RAMP_SKY, 4);
            if (e == 0 && brk > 120) c = deep > 120 ? RP(RAMP_SKY, 6) : RP(RAMP_SKY, 5);
            if (w > 3700 && brk > 90) c = RP(RAMP_SKY, 6);
            if (w > 4000 && brk > 170) c = RP(RAMP_SKY, 7);
            if (w < -3900 && deep < 150) c = RP(RAMP_SKY, 4);
            PX(x, y, c);
        }
    /* twinkling sparkles: each frame lights a different set */
    {
        int i;
        for (i = 0; i < 5; i++) {
            uint32_t h = art_hash((uint32_t)i, (uint32_t)f, 0x3A80u);
            int sx = (int)(h % 64u), sy = (int)((h >> 8) % 64u);
            PX(sx, sy, PAL_WHITE);
            if (i & 1) { PX(sx - 1, sy, RP(RAMP_SKY, 7)); PX(sx + 1, sy, RP(RAMP_SKY, 7)); }
        }
    }
}

/* ---- walls ---------------------------------------------------------------------- */
static void t_pastel_brick(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            TxCell c;
            int ramp, base, s;
            tx_brick(x, y, 16, 8, 8, 1, &c);
            if (c.mortar) {
                /* cream mortar, shaded right under each brick */
                PX(x, y, (c.ly == 0 && c.lx != 0) ? RP(RAMP_PEACH, 5) : RP(RAMP_PEACH, 7));
                continue;
            }
            switch (c.id % 7u) {
            case 0: case 1: case 2: ramp = RAMP_PASTEL_PINK; base = 5; break;
            case 3: ramp = RAMP_PASTEL_PINK; base = 4; break;
            case 4: case 5: ramp = RAMP_PEACH; base = 5; break;
            default: ramp = RAMP_VAPE; base = 6; break;
            }
            s = base + tx_bevel(c.lx - 1, c.ly - 1, c.w - 1, c.h - 1, 1);
            if (c.lx == 2 && c.ly == 2) s = base + 2;          /* glint */
            if ((art_hash((uint32_t)x, (uint32_t)y, 0xB1u) % 29u) == 0) s--;
            PX(x, y, RP(ramp, s));
        }
    /* one engraved heart */
    img_heart(im, 41, 28, 2, RP(RAMP_PASTEL_PINK, 3));
    PX(40, 27, RP(RAMP_PASTEL_PINK, 6));
}

static void t_candy_stripe(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int d = art_wrap(x + y, 16);
            int s = art_cos((x - 22) * ART_TURN / 64);      /* pillar light */
            int shade = s > 1800 ? 2 : (s > -2200 ? 1 : 0);
            uint8_t c;
            if (d < 7) c = PP(PAINT_CHERRY, shade);
            else if (d == 7 || d == 15) c = PP(PAINT_ROSE, shade);
            else c = PP(PAINT_WHITE, shade);
            /* glossy highlight streak */
            if (x >= 15 && x <= 17) c = d < 7 ? PP(PAINT_ROSE, 2) : PAL_WHITE;
            if (x == 18 && d >= 8) c = PAL_WHITE;
            PX(x, y, c);
        }
}

static void t_wood_floor(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int row = y / 8, ly = y % 8;
            int off = (int)(art_hash((uint32_t)row, 1u, 0x7D0u) % 32u);
            int lx = art_wrap(x + off, 32), plank = art_wrap(x + off, 64) / 32;
            uint32_t pid = art_hash((uint32_t)row, (uint32_t)plank, 0x7D1u);
            int base = 5 - (int)(pid % 3u == 0);
            int g = art_sin(ly * ART_TURN / 5 + art_noise(x, y, 16, 64, pid) * 10 + (int)(pid & 1023u));
            int s = base;
            if (g > 3300) s--;
            if (ly == 1) s++;
            if (ly == 7) s--;
            if (ly == 0 || lx == 0) s = 2;
            PX(x, y, RP(RAMP_WOOD, s));
        }
    /* a couple of knots */
    img_ellipse(im, 21, 12, 2, 1, RP(RAMP_WOOD, 3));
    PX(21, 12, RP(RAMP_WOOD, 2));
    img_ellipse(im, 52, 44, 1, 1, RP(RAMP_WOOD, 3));
}

static void t_wood_wall(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int lx = x % 8, b = x / 8;
            uint32_t h = art_hash((uint32_t)b, 0u, 0x77A1u);
            int base = 5 + (int)(h % 3u == 0) - (int)(h % 5u == 0);
            int g = art_sin(lx * ART_TURN / 4 + art_noise(x, y, 8, 64, h) * 12);
            int s = base;
            if (lx == 0) s = 2;
            else if (lx == 1) s = base + 1;
            else if (lx == 7) s = base - 1;
            else if (g > 3500) s = base - 1;
            PX(x, y, RP(RAMP_WOOD, s));
        }
    /* nails top and bottom of every board */
    for (x = 0; x < 64; x += 8) {
        PX(x + 4, 5, RP(RAMP_STEEL, 6)); PX(x + 5, 6, RP(RAMP_STEEL, 3));
        PX(x + 4, 57, RP(RAMP_STEEL, 6)); PX(x + 5, 58, RP(RAMP_STEEL, 3));
    }
}

static void t_roof_tiles(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            /* the topmost scale covering this pixel wins: upper rows overlap */
            int r, done = 0;
            for (r = y / 8 - 1; r <= y / 8 + 1 && !done; r++) {
                int cx = 0, dx, dy, d2, col;
                int off = (r & 1) ? 8 : 0;
                col = art_wrap(x - off, 64) / 16;
                cx = col * 16 + 8 + off;
                dx = art_wrap(x - cx + 32, 64) - 32;
                dy = y - (r * 8 + 4);
                d2 = (2 * dx + 1) * (2 * dx + 1) + 4 * dy * dy;
                if (dy < -2 || d2 > 16 * 16 * 4 / 4 * 4 / 4 + 0 || d2 > 4 * 64) continue;
                {
                    uint32_t h = art_hash((uint32_t)art_wrap(col, 4), (uint32_t)art_wrap(r, 8), 0x5CA1u);
                    int base = 4 - (int)(h % 4u == 0) + (int)(h % 5u == 0);
                    int s = base;
                    if (dy < 1) s = base - 1;                       /* shade under upper row */
                    if (d2 > 4 * 49) s = (dx < 0 && dy < 5) ? base + 1 : base - 2;
                    else if (d2 > 4 * 36 && dx < -2) s = base + 1;  /* lit inner rim */
                    if (dx == -4 && dy == 3) s = base + 2;           /* glint */
                    PX(x, y, RP(RAMP_PASTEL_PINK, s));
                    done = 1;
                }
            }
            if (!done) PX(x, y, RP(RAMP_PASTEL_PINK, 1));
        }
}

static void leaf_ball(ArtImg *im, int cx, int cy, int r, int lo, int hi, uint32_t seed)
{
    int k;
    ArtMat m = art_mat_ramp(RAMP_GRASS, lo, hi);
    img_ball(im, cx, cy, r, r * 9 / 10, &m, 0);
    /* leaf marks: little lighter chevrons on the lit half */
    for (k = 0; k < r; k++) {
        uint32_t h = art_hash((uint32_t)k, seed, 0x1EAFu);
        int lx = cx - r / 2 + (int)(h % (uint32_t)(r + 1)) - r / 3;
        int ly = cy - r / 2 + (int)((h >> 8) % (uint32_t)(r + 1)) - r / 3;
        uint8_t c = img_get(im, lx, ly);
        if (!c) continue;
        img_put(im, lx, ly, art_lighter(c, 1));
        img_put(im, lx + 1, ly + 1, art_darker(c, 1));
    }
}

static void t_hedge(ArtImg *im, int f, int n)
{
    int i, k, xs[48], ys[48], ord[48];
    (void)f; (void)n;
    img_fill(im, RP(RAMP_GRASS, 1));
    k = tx_scatter(30, 64, 64, 0x4ED6u, xs, ys);
    for (i = 0; i < k; i++) ord[i] = i;
    /* draw top to bottom so lower balls overlap upper ones */
    for (i = 1; i < k; i++) {
        int j = i, t = ord[i];
        while (j > 0 && ys[ord[j - 1]] > ys[t]) { ord[j] = ord[j - 1]; j--; }
        ord[j] = t;
    }
    for (i = 0; i < k; i++) {
        int q = ord[i];
        int r = 6 + (int)(art_hash((uint32_t)q, 1u, 0x4ED7u) % 3u);
        leaf_ball(im, xs[q], ys[q], r, 2, 6, (uint32_t)q);
    }
    /* tiny pink blossoms and berries */
    for (i = 0; i < 7; i++) {
        uint32_t h = art_hash((uint32_t)i, 2u, 0x4ED8u);
        int bx = (int)(h % 64u), by = (int)((h >> 8) % 64u);
        if (i & 1) tx_flower(im, bx, by, PP(PAINT_BUBBLEGUM, 2), PP(PAINT_BUBBLEGUM, 1), RP(RAMP_CANDY, 5));
        else { PX(bx, by, PP(PAINT_CHERRY, 1)); PX(bx, by - 1, PP(PAINT_CHERRY, 2)); PX(bx + 1, by, PP(PAINT_CHERRY, 0)); }
    }
}

static void t_picket_fence(ArtImg *im, int f, int n)
{
    int x, y, p;
    (void)f; (void)n;
    /* bushy backdrop */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int v = art_fbm(x, y, 16, 2, 64, 0xFE7Cu);
            PX(x, y, RP(RAMP_GRASS, v > 140 ? 3 : 2));
        }
    for (p = 0; p < 9; p++) {
        uint32_t h = art_hash((uint32_t)p, 0u, 0xFE7Du);
        leaf_ball(im, p * 8 + (int)(h % 5u), 4 + (int)((h >> 4) % 10u), 6, 2, 4, h);
    }
    /* rails behind the pickets */
    for (y = 0; y < 4; y++)
        for (x = 0; x < 64; x++) {
            int s = y == 0 ? 6 : (y == 3 ? 4 : 5);
            PX(x, 22 + y, RP(RAMP_BONE, s));
            PX(x, 46 + y, RP(RAMP_BONE, s));
        }
    /* pickets */
    for (p = 0; p < 4; p++) {
        int x0 = p * 16 + 3;
        for (y = 4; y < 64; y++)
            for (x = 0; x < 10; x++) {
                int top = y < 10 ? (10 - y) : 0;     /* pointed tip */
                int s;
                if (x < top / 2 + (top ? 0 : 0) || x > 9 - top / 2 || (y < 10 && art_abs(2 * x - 9) > 2 * (y - 4) + 1)) continue;
                s = 6;
                if (x <= 1) s = 7;
                else if (x >= 8) s = 4;
                else if (x >= 6) s = 5;
                if (y == 63) s = 3;
                PX(x0 + x, y, s == 7 ? PAL_WHITE : RP(RAMP_BONE, s + 1));
            }
        /* shadow of the picket on the rails */
        for (y = 0; y < 4; y++) {
            img_shift(im, x0 + 10, 22 + y, -2);
            img_shift(im, x0 + 10, 46 + y, -2);
        }
        /* nail heads */
        PX(x0 + 4, 23, RP(RAMP_STEEL, 4));
        PX(x0 + 4, 47, RP(RAMP_STEEL, 4));
    }
    /* grass tufts at the foot */
    for (x = 0; x < 64; x++) {
        int h = 2 + (int)(art_hash((uint32_t)x, 0u, 0xFE7Eu) % 4u);
        for (y = 0; y < h; y++)
            PX(x, 63 - y, RP(RAMP_GRASS, y == h - 1 ? 6 : (y == 0 ? 3 : 4 + (x & 1))));
    }
}

/* striped scalloped awning across the top of a shop front */
static void awning(ArtImg *im, int y0, int h, int paint_a, int paint_b)
{
    int x, y;
    for (y = 0; y < h + 3; y++)
        for (x = 0; x < 64; x++) {
            int stripe = (x / 8) & 1, lx = x % 8, shade;
            int paint = stripe ? paint_b : paint_a;
            int ly = y;
            if (ly >= h) {
                /* scallop: semicircle hanging under each stripe */
                int dx = 2 * lx - 7, dy = 2 * (ly - h);
                if (dx * dx + dy * dy * 3 > 49 + 12) continue;
            }
            shade = 1;
            if (ly < 2) shade = 0;                    /* fold at the top */
            else if (lx <= 1) shade = 2;              /* rounded ridge */
            else if (lx >= 6) shade = 0;
            if (ly >= h && ly == h + 2) shade = 0;
            PX(x, y0 + y, PP(paint, shade));
        }
    /* soft shadow under the awning */
    for (x = 0; x < 64; x++) {
        int lx = x % 8, dx = 2 * lx - 7;
        int sy = y0 + h + 3 + (dx * dx < 25 ? 1 : 0);
        img_shift(im, x, sy, -2);
        img_shift(im, x, sy + 1, -1);
    }
}

static void window_glass(ArtImg *im, int x0, int y0, int w, int h)
{
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int s = 6 - (y * 3) / h;
            int diag = x + y;
            uint8_t c = RP(RAMP_SKY, s);
            if (diag > w / 3 && diag < w / 3 + 3) c = RP(RAMP_SKY, 7);
            if (diag == w / 3 + 5) c = RP(RAMP_SKY, 7);
            PX(x0 + x, y0 + y, c);
        }
}

static void frame_box(ArtImg *im, int x0, int y0, int w, int h, int ramp, int base)
{
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int s = base + tx_bevel(x, y, w, h, 1);
            PX(x0 + x, y0 + y, RP(ramp, s));
        }
}

static void cupcake(ArtImg *im, int x, int y, int paint)
{
    /* wrapper */
    img_rect(im, x - 2, y, 5, 3, RP(RAMP_WOOD, 5));
    PX(x - 2, y, RP(RAMP_WOOD, 6)); PX(x + 2, y + 2, RP(RAMP_WOOD, 3));
    PX(x, y + 1, RP(RAMP_WOOD, 4));
    /* frosting */
    {
        ArtMat m = art_mat_paint(paint);
        img_ball(im, x, y - 1, 3, 2, &m, 0);
    }
    PX(x, y - 4, PP(PAINT_CHERRY, 1));
    PX(x - 1, y - 2, PAL_WHITE);
}

static void t_shop_front(ArtImg *im, int f, int n)
{
    int x, y, i;
    (void)f; (void)n;
    /* lavender wall */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) PX(x, y, RP(RAMP_VAPE, (y % 8 == 0) ? 5 : 6));
    /* window */
    frame_box(im, 5, 22, 54, 30, RAMP_BONE, 6);
    window_glass(im, 8, 25, 48, 24);
    /* shelves with goods */
    for (i = 0; i < 2; i++) {
        int sy = 35 + i * 12;
        for (x = 8; x < 56; x++) { PX(x, sy, RP(RAMP_WOOD, 5)); PX(x, sy + 1, RP(RAMP_WOOD, 3)); }
    }
    cupcake(im, 14, 32, PAINT_BUBBLEGUM);
    cupcake(im, 23, 32, PAINT_MINT);
    {   /* candy jar */
        ArtMat glass = art_mat_ramp(RAMP_SKY, 5, 7);
        img_ball(im, 34, 30, 4, 4, &glass, 0);
        PX(32, 30, PP(PAINT_CHERRY, 1)); PX(35, 31, PP(PAINT_LEMON, 1)); PX(34, 28, PP(PAINT_MINT, 1));
        PX(36, 29, PP(PAINT_GRAPE, 2)); PX(33, 32, PP(PAINT_SKY, 1));
        img_rect(im, 32, 25, 5, 1, RP(RAMP_WOOD, 4));
    }
    {   /* teddy */
        ArtMat fur = art_mat_ramp(RAMP_WOOD, 3, 6);
        img_ball(im, 46, 31, 4, 3, &fur, 0);
        img_ball(im, 43, 27, 1, 1, &fur, 0);
        img_ball(im, 49, 27, 1, 1, &fur, 0);
        PX(45, 31, PAL_BLACK); PX(47, 31, PAL_BLACK); PX(46, 32, RP(RAMP_WOOD, 2));
    }
    cupcake(im, 15, 44, PAINT_LEMON);
    for (i = 0; i < 4; i++) {           /* lollipops */
        int lx = 24 + i * 8;
        img_vline(im, lx, 42, 4, PAL_WHITE);
        img_disc(im, lx, 41, 2, PP(i & 1 ? PAINT_SKY : PAINT_ROSE, 1));
        PX(lx - 1, 40, PP(i & 1 ? PAINT_SKY : PAINT_ROSE, 2));
        PX(lx + 1, 42, PP(i & 1 ? PAINT_SKY : PAINT_ROSE, 0));
    }
    awning(im, 2, 12, PAINT_BUBBLEGUM, PAINT_WHITE);
    /* planter base */
    for (y = 54; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = y == 54 ? 6 : (y == 63 ? 3 : 5);
            if (x % 16 == 0 && y > 54) s = 3;
            PX(x, y, RP(RAMP_MINT, s));
        }
}

static void gothic_window(ArtImg *im, int cx, int y0, int w, int h, int glow)
{
    int x, y, hw = w / 2, spring = hw * 7 / 4;
    for (y = -2; y < h; y++)
        for (x = -hw - 2; x <= hw + 1; x++) {
            /* equilateral pointed arch: two circles of radius w centred on
             * the opposite springing points (doubled coordinates) */
            int ax = 2 * x + 1, ay = 2 * (y - spring);
            int in_body, in_frame;
            int64_t dl = (int64_t)(ax + 2 * hw) * (ax + 2 * hw) + (int64_t)ay * ay;
            int64_t dr = (int64_t)(ax - 2 * hw) * (ax - 2 * hw) + (int64_t)ay * ay;
            int64_t r = (int64_t)(4 * hw) * (4 * hw), rf = (int64_t)(4 * hw + 4) * (4 * hw + 4);
            if (y < spring) {
                in_body = dl <= r && dr <= r;
                in_frame = dl <= rf && dr <= rf;
            } else {
                in_body = art_abs(ax) < 2 * hw;
                in_frame = art_abs(ax) < 2 * hw + 4;
            }
            if (!in_frame) continue;
            if (!in_body) { PX(cx + x, y0 + y, RP(RAMP_BONE, x < 0 ? 4 : 3)); continue; }
            {
                static const int cols[6] = {PAINT_SKY, PAINT_ROSE, PAINT_LEMON, PAINT_LAVENDER, PAINT_MINT, PAINT_BUBBLEGUM};
                int pane = (x + hw) / 4 + ((y + 3) / 7) * 2;
                int lead = (y > spring - 2 && (y - spring + 2) % 8 == 0) || art_abs(ax) == 1 ||
                           (y < spring && art_abs(ax) == hw);
                uint8_t c = PP(cols[art_wrap(pane, 6)], glow ? ((x + y) % 5 == 0 ? 2 : 1) : 1);
                if (y < spring && dl > r - 6 * hw * 4 && dr > r - 6 * hw * 4) c = RP(RAMP_CANDY, 6);
                if (lead) c = PP(PAINT_CHARCOAL, 1);
                PX(cx + x, y0 + y, c);
            }
        }
}

static void t_church_wall(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            TxCell c;
            int s;
            tx_brick(x, y, 16, 8, 8, 1, &c);
            if (c.mortar) { PX(x, y, RP(RAMP_BONE, c.ly == 0 && c.lx ? 4 : 5)); continue; }
            s = 6 + tx_bevel(c.lx - 1, c.ly - 1, c.w - 1, c.h - 1, 1) - (int)(c.id % 6u == 0);
            if ((art_hash((uint32_t)x, (uint32_t)y, 0xC4u) % 31u) == 0) s--;
            PX(x, y, RP(RAMP_BONE, s));
        }
    gothic_window(im, 32, 8, 18, 45, 1);
    /* sill + golden heart keystone */
    for (x = 22; x < 42; x++) { PX(x, 53, RP(RAMP_BONE, 7)); PX(x, 54, RP(RAMP_BONE, 4)); }
    img_heart(im, 32, 4, 2, RP(RAMP_CANDY, 4));
    PX(31, 3, RP(RAMP_CANDY, 6));
}

static void t_stained_glass(ArtImg *im, int f, int n)
{
    static const int cols[7] = {PAINT_ROSE, PAINT_SKY, PAINT_LEMON, PAINT_MINT, PAINT_LAVENDER,
                                PAINT_BUBBLEGUM, PAINT_CYAN};
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int dx = 2 * x - 63, dy = 2 * y - 63;          /* centre (31.5,31.5) x2 */
            int d = art_dist(dx, dy), a = art_atan2(dy, dx);
            int lead = 0, pane, shade;
            uint8_t c;
            if (d < 14) {                                   /* golden heart core */
                pane = -1;
                lead = (d >= 12);
            } else if (d < 52) {                            /* petals */
                int seg = ((a + 256) & 4095) / 512;
                int rel = ((a + 256) & 511);
                pane = seg * 2 + (d > 34);
                lead = (d >= 33 && d <= 35) || rel < 24 || d >= 50;
            } else {                                        /* corner lattice */
                int u = art_wrap(x + y, 16), v = art_wrap(x - y, 16);
                pane = 20 + ((x + y) / 16 + (x - y + 64) / 16) % 4;
                lead = u == 0 || v == 0;
            }
            if (lead) { PX(x, y, PP(PAINT_CHARCOAL, (x + y) & 1 ? 0 : 1)); continue; }
            /* backlit: brighter towards the middle of each pane */
            shade = 1 + (art_noise(x, y, 8, 64, 0x9A55u) > 150) + (d < 40 && d > 24 ? 0 : 0);
            if (pane < 0) {
                c = RP(RAMP_CANDY, d < 8 ? 7 : (d < 11 ? 6 : 5));
            } else {
                int k = art_wrap(pane * 3 + 1, 7);
                if (pane >= 20) k = (pane - 20) * 2 % 7;
                c = PP(cols[k], shade > 2 ? 2 : shade);
            }
            PX(x, y, c);
        }
    img_heart(im, 32, 32, 3, PP(PAINT_CHERRY, 1));
    PX(30, 30, PP(PAINT_CHERRY, 2));
    /* glints */
    PX(20, 14, PAL_WHITE); PX(46, 44, PAL_WHITE); PX(9, 53, PAL_WHITE);
}

static void t_carpet(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int u = art_wrap(x + y, 32), v = art_wrap(x - y, 32);
            int pl = art_fbm(x, y, 8, 2, 64, 0xCA2Fu);
            uint8_t c = RP(RAMP_PASTEL_PINK, 4);
            if (pl > 168) c = RP(RAMP_PASTEL_PINK, 5);
            else if (pl < 78) c = RP(RAMP_PASTEL_PINK, 3);
            if (u == 0 || v == 0) c = RP(RAMP_CANDY, 5);
            else if (u == 1 || v == 1) c = RP(RAMP_CANDY, 3);
            PX(x, y, c);
        }
    /* little cream hearts in the diamond centres */
    img_heart(im, 32, 16, 2, RP(RAMP_PEACH, 7));
    img_heart(im, 0, 16, 2, RP(RAMP_PEACH, 7));
    img_heart(im, 16, 48, 2, RP(RAMP_PEACH, 7));
    img_heart(im, 48, 48, 2, RP(RAMP_PEACH, 7));
    img_heart(im, 16, 0, 2, RP(RAMP_CANDY, 6));
    img_heart(im, 48, 32, 2, RP(RAMP_CANDY, 6));
    img_heart(im, 16, 32, 2, RP(RAMP_CANDY, 6));
    img_heart(im, 48, 0, 2, RP(RAMP_CANDY, 6));
}

static void t_wallpaper_hearts(ArtImg *im, int f, int n)
{
    int x, y, i, j;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int lx = x % 16;
            uint8_t c = lx < 8 ? RP(RAMP_PASTEL_PINK, 6) : RP(RAMP_PASTEL_PINK, 7);
            if (lx == 0 || lx == 8) c = RP(RAMP_PASTEL_PINK, 5);
            if ((lx == 3 || lx == 4) && (y & 3) == 0) c = RP(RAMP_PASTEL_PINK, 5);
            PX(x, y, c);
        }
    for (j = 0; j < 4; j++)
        for (i = 0; i < 4; i++) {
            int hx = i * 16 + 12 - ((j & 1) ? 8 : 0), hy = j * 16 + 8;
            int pal = (i + j) & 1 ? PAINT_ROSE : PAINT_BUBBLEGUM;
            img_heart_px(im, hx + 1, hy + 1, 1, RP(RAMP_PASTEL_PINK, 5), RP(RAMP_PASTEL_PINK, 5), 0);
            img_heart_px(im, hx, hy, 1, PP(pal, 1), PP(pal, 0), PAL_WHITE);
        }
}

static void t_cloud_puff(ArtImg *im, int f, int n)
{
    int i, k, xs[40], ys[40], ord[40];
    ArtMat m = art_mat5(RP(RAMP_VAPE, 5), RP(RAMP_VAPE, 6), RP(RAMP_SKY, 6), RP(RAMP_SKY, 7), PAL_WHITE);
    (void)f; (void)n;
    img_fill(im, RP(RAMP_SKY, 5));
    k = tx_scatter(26, 64, 64, 0xC10Du, xs, ys);
    for (i = 0; i < k; i++) ord[i] = i;
    for (i = 1; i < k; i++) {
        int j = i, t = ord[i];
        while (j > 0 && ys[ord[j - 1]] > ys[t]) { ord[j] = ord[j - 1]; j--; }
        ord[j] = t;
    }
    for (i = 0; i < k; i++) {
        int q = ord[i], r = 7 + (int)(art_hash((uint32_t)q, 0u, 0xC10Eu) % 4u);
        img_ball(im, xs[q], ys[q], r, r * 8 / 10, &m, 0);
    }
    for (i = 0; i < 4; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0xC10Fu);
        PX((int)(h % 64u), (int)((h >> 8) % 64u), PAL_WHITE);
    }
}

static void t_tree(ArtImg *im, int f, int n)
{
    int i, k, x, y, xs[40], ys[40], ord[40];
    (void)f; (void)n;
    img_fill(im, RP(RAMP_GRASS, 0));
    /* trunks peeking out at the bottom */
    for (i = 0; i < 2; i++) {
        int tx0 = 10 + i * 32;
        for (y = 40; y < 64; y++)
            for (x = 0; x < 8; x++) {
                int s = x < 2 ? 4 : (x > 5 ? 2 : 3);
                if ((y + x * 3) % 7 == 0) s--;
                PX(tx0 + x, y, RP(RAMP_WOOD, s));
            }
    }
    k = tx_scatter(24, 64, 48, 0x72EEu, xs, ys);
    for (i = 0; i < k; i++) ord[i] = i;
    for (i = 1; i < k; i++) {
        int j = i, t = ord[i];
        while (j > 0 && ys[ord[j - 1]] > ys[t]) { ord[j] = ord[j - 1]; j--; }
        ord[j] = t;
    }
    for (i = 0; i < k; i++) {
        int q = ord[i], r = 7 + (int)(art_hash((uint32_t)q, 0u, 0x72EFu) % 4u);
        leaf_ball(im, xs[q], ys[q] - 4, r, 2, 6, (uint32_t)q * 7u);
    }
    /* blossoms and apples */
    for (i = 0; i < 9; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0x72F0u);
        int bx = (int)(h % 64u), by = (int)((h >> 8) % 44u);
        if (i % 3) tx_flower(im, bx, by, PP(PAINT_BUBBLEGUM, 2), PP(PAINT_BUBBLEGUM, 1), RP(RAMP_CANDY, 6));
        else {
            ArtMat ap = art_mat_paint(PAINT_CHERRY);
            img_ball(im, bx, by, 2, 2, &ap, 0);
            PX(bx, by - 3, RP(RAMP_GRASS, 3));
        }
    }
}

static void t_playground(ArtImg *im, int f, int n)
{
    static const int ramps[4] = {RAMP_PASTEL_PINK, RAMP_MINT, RAMP_CANDY, RAMP_SKY};
    static const int bases[4] = {5, 5, 5, 5};
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int tx = x / 16, ty = y / 16, lx = x % 16, ly = y % 16;
            int k = art_wrap(tx + ty * 3 + (tx * ty), 4);
            int s = bases[k] + tx_bevel(lx, ly, 16, 16, 1);
            uint32_t h = art_hash((uint32_t)x, (uint32_t)y, 0x9A7Du);
            if (lx == 0 || ly == 0) s = bases[k] - 2;
            else if ((h & 31u) == 0) s++;
            else if ((h & 31u) == 1) s--;
            PX(x, y, RP(ramps[k], s));
        }
}

static void t_vape_shop_front(ArtImg *im, int f, int n)
{
    int x, y, i;
    (void)f; (void)n;
    /* dark purple siding */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = (x % 8 == 0) ? 0 : ((x % 8 == 1) ? 2 : 1);
            PX(x, y, RP(RAMP_VAPE, s));
        }
    /* neon sign box */
    for (y = 1; y < 15; y++)
        for (x = 6; x < 58; x++) PX(x, y, RP(RAMP_VAPE, 0));
    for (x = 6; x < 58; x++) { PX(x, 1, PP(PAINT_CYAN, 2)); PX(x, 14, PP(PAINT_CYAN, 1)); }
    for (y = 1; y < 15; y++) { PX(6, y, PP(PAINT_CYAN, 2)); PX(57, y, PP(PAINT_CYAN, 1)); }
    /* glow behind the letters, then the tubes */
    img_text(im, 20, 5, "VAPE", 1, RP(RAMP_VAPE, 3));
    img_text(im, 21, 5, "VAPE", 1, RP(RAMP_VAPE, 3));
    img_text(im, 20, 4, "VAPE", 1, RP(RAMP_VAPE, 3));
    img_text(im, 20, 6, "VAPE", 1, RP(RAMP_VAPE, 3));
    img_text(im, 19, 5, "VAPE", 1, RP(RAMP_VAPE, 3));
    img_text(im, 20, 5, "VAPE", 1, PP(PAINT_MAGENTA, 2));
    /* cloud logo puffs either side */
    img_disc(im, 11, 8, 2, RP(RAMP_VAPE, 6)); img_disc(im, 14, 7, 2, RP(RAMP_VAPE, 7));
    img_disc(im, 50, 8, 2, RP(RAMP_VAPE, 6)); img_disc(im, 53, 7, 2, RP(RAMP_VAPE, 7));
    /* window */
    for (y = 18; y < 52; y++)
        for (x = 4; x < 60; x++) {
            uint8_t c = RP(RAMP_VAPE, 0);
            if (x == 4 || x == 59 || y == 18 || y == 51) c = RP(RAMP_STEEL, 3);
            else if (art_wrap(x + y, 24) == 0) c = RP(RAMP_VAPE, 1);
            PX(x, y, c);
        }
    /* glowing shelves of bottles */
    for (i = 0; i < 2; i++) {
        int sy = 33 + i * 14, bx;
        for (x = 6; x < 58; x++) { PX(x, sy, PP(PAINT_MAGENTA, 0)); PX(x, sy - 1, RP(RAMP_VAPE, 2)); }
        for (bx = 9; bx < 56; bx += 6) {
            static const int liq[4] = {PAINT_CYAN, PAINT_MAGENTA, PAINT_SLIME, PAINT_VAPE_PURPLE};
            int L = liq[art_wrap(bx / 6 + i, 4)];
            img_rect(im, bx, sy - 9, 3, 7, PP(L, 1));
            img_vline(im, bx, sy - 9, 7, PP(L, 2));
            img_vline(im, bx + 2, sy - 9, 7, PP(L, 0));
            img_rect(im, bx, sy - 11, 3, 2, RP(RAMP_STEEL, 5));
            PX(bx + 1, sy - 12, RP(RAMP_STEEL, 3));
            PX(bx, sy - 8, PAL_WHITE);
        }
    }
    /* lazy vapour curling out of the window top */
    img_disc(im, 52, 21, 2, RP(RAMP_VAPE, 4)); img_disc(im, 55, 20, 2, RP(RAMP_VAPE, 5));
    PX(54, 19, RP(RAMP_VAPE, 6));
    /* LED strip and dark tiles */
    for (x = 0; x < 64; x++) {
        PX(x, 54, PP(PAINT_MAGENTA, 2));
        PX(x, 55, RP(RAMP_VAPE, 3));
        for (y = 56; y < 64; y++) PX(x, y, RP(RAMP_STONE, (x % 8 == 0 || y == 63) ? 0 : 1));
    }
}

static void flower_box(ArtImg *im, int x0, int y0, int w)
{
    int x, i;
    for (x = 0; x < w; x++) {
        PX(x0 + x, y0, RP(RAMP_WOOD, 5));
        PX(x0 + x, y0 + 1, RP(RAMP_WOOD, 4));
        PX(x0 + x, y0 + 2, RP(RAMP_WOOD, 4));
        PX(x0 + x, y0 + 3, RP(RAMP_WOOD, 2));
    }
    for (i = 0; i < w; i += 2) {
        PX(x0 + i, y0 - 1, RP(RAMP_GRASS, 4));
        PX(x0 + i + 1, y0 - 1, RP(RAMP_GRASS, 3));
    }
    for (i = 2; i < w - 1; i += 4) {
        int p = (i / 4) % 3 == 0 ? PAINT_CHERRY : ((i / 4) % 3 == 1 ? PAINT_LEMON : PAINT_BUBBLEGUM);
        tx_flower(im, x0 + i, y0 - 3, PP(p, 2), PP(p, 1), RP(RAMP_CANDY, 6));
    }
}

static void t_apartment_front(ArtImg *im, int f, int n)
{
    int x, y, w;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = (y % 6 == 5) ? 5 : 6;
            PX(x, y, RP(RAMP_PEACH, s));
        }
    for (w = 0; w < 2; w++) {
        int x0 = 5 + w * 32;
        frame_box(im, x0, 8, 22, 30, RAMP_BONE, 6);
        window_glass(im, x0 + 2, 10, 18, 26);
        /* cross bar */
        for (y = 10; y < 36; y++) PX(x0 + 11, y, RP(RAMP_BONE, 6));
        for (x = x0 + 2; x < x0 + 20; x++) PX(x, 22, RP(RAMP_BONE, 6));
        /* curtains tied at the sides */
        for (y = 10; y < 36; y++) {
            int cw = y < 24 ? 3 + (24 - y) / 6 : 2 + (y - 24) / 4;
            for (x = 0; x < cw; x++) {
                int s = x == cw - 1 ? 4 : 5;
                if (((y + x) % 4) == 0) s = 6;
                PX(x0 + 2 + x, y, RP(RAMP_PASTEL_PINK, s));
                PX(x0 + 19 - x, y, RP(RAMP_PASTEL_PINK, s == 6 ? 5 : s));
            }
        }
        PX(x0 + 4, 24, RP(RAMP_CANDY, 5)); PX(x0 + 17, 24, RP(RAMP_CANDY, 5));
        flower_box(im, x0 + 1, 40, 20);
    }
    /* cornice band */
    for (x = 0; x < 64; x++) {
        PX(x, 56, RP(RAMP_MINT, 6));
        PX(x, 57, RP(RAMP_MINT, 5));
        PX(x, 58, RP(RAMP_MINT, 5));
        PX(x, 59, RP(RAMP_MINT, 3));
        if (x % 8 == 4) { PX(x, 57, RP(RAMP_MINT, 7)); PX(x, 58, RP(RAMP_MINT, 4)); }
    }
}

static void gear(ArtImg *im, int cx, int cy, int r)
{
    int x, y;
    for (y = -r - 2; y <= r + 2; y++)
        for (x = -r - 2; x <= r + 2; x++) {
            int d = art_dist(x * 16, y * 16), a = art_atan2(y, x);
            int tooth = art_wrap(a + 256, 512) < 256;
            int rr = (tooth ? r + 1 : r) * 16 + 8;
            if (d > rr || d < 16 + 8) continue;
            PX(cx + x, cy + y, RP(RAMP_CANDY, (x + y < 0) ? 6 : (x + y > 2 ? 3 : 5)));
        }
}

static void t_workshop_front(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    /* warm brick sides */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            TxCell c;
            tx_brick(x, y, 8, 4, 4, 1, &c);
            PX(x, y, c.mortar ? RP(RAMP_PEACH, 6) : RP(RAMP_WOOD, 4 + tx_bevel(c.lx - 1, c.ly - 1, 7, 3, 1) - (int)(c.id % 4u == 0)));
        }
    /* corrugated roller door */
    for (y = 14; y < 64; y++)
        for (x = 8; x < 56; x++) {
            int ly = (y - 14) % 4;
            int s = ly == 0 ? 6 : (ly == 1 ? 5 : (ly == 2 ? 5 : 3));
            if (x == 8) s = 3;
            if (x == 55) s = 2;
            PX(x, y, RP(RAMP_STEEL, s));
        }
    /* hazard stripe and handle */
    for (y = 57; y < 61; y++)
        for (x = 9; x < 55; x++)
            PX(x, y, art_wrap(x + y, 6) < 3 ? RP(RAMP_CANDY, 5) : RP(RAMP_STONE, 1));
    for (x = 28; x < 36; x++) { PX(x, 52, RP(RAMP_STEEL, 7)); PX(x, 53, RP(RAMP_STEEL, 2)); }
    /* little painted heart on the door */
    img_heart(im, 32, 34, 3, PP(PAINT_ROSE, 1));
    PX(30, 33, PP(PAINT_ROSE, 2));
    /* sign board with gear + WELD */
    frame_box(im, 6, 0, 52, 13, RAMP_WOOD, 5);
    gear(im, 13, 6, 3);
    img_text(im, 25, 4, "WELD", 1, RP(RAMP_WOOD, 2));
    img_text(im, 24, 3, "WELD", 1, RP(RAMP_CANDY, 6));
    /* awning bolt + lamp glow */
    PX(51, 6, RP(RAMP_CANDY, 7)); PX(50, 6, RP(RAMP_CANDY, 5)); PX(52, 6, RP(RAMP_CANDY, 5));
    PX(51, 5, RP(RAMP_CANDY, 5)); PX(51, 7, RP(RAMP_CANDY, 4));
}

static void t_cafe_front(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    /* cream wall */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) PX(x, y, RP(RAMP_PEACH, (y % 8 == 7) ? 6 : 7));
    /* sign */
    for (y = 0; y < 9; y++)
        for (x = 14; x < 50; x++) PX(x, y, RP(RAMP_WOOD, 3 + tx_bevel(x - 14, y, 36, 9, 1)));
    img_text(im, 21, 1, "CAFE", 1, RP(RAMP_CANDY, 6));
    awning(im, 10, 8, PAINT_MINT, PAINT_WHITE);
    frame_box(im, 6, 26, 52, 26, RAMP_WOOD, 5);
    window_glass(im, 9, 29, 46, 20);
    /* cake stand with a layered cake */
    for (x = 14; x < 30; x++) PX(x, 44, PAL_WHITE);
    PX(22, 45, PAL_WHITE); PX(22, 46, PAL_WHITE);
    for (y = 0; y < 9; y++)
        for (x = 0; x < 12; x++) {
            int s = x < 2 ? 2 : (x > 9 ? 0 : 1);
            int paint = (y < 3) ? PAINT_WHITE : ((y < 6) ? PAINT_BUBBLEGUM : PAINT_CHOCOLATE);
            if (y == 3 || y == 6) paint = PAINT_WHITE;
            PX(16 + x, 35 + y, PP(paint, s));
        }
    PX(21, 34, PP(PAINT_CHERRY, 1)); PX(22, 33, PP(PAINT_CHERRY, 2)); PX(21, 33, PP(PAINT_CHERRY, 1));
    cupcake(im, 37, 43, PAINT_LAVENDER);
    cupcake(im, 46, 43, PAINT_PEACH);
    {   /* steaming coffee cup */
        img_rect(im, 39, 33, 6, 5, PAL_WHITE);
        img_vline(im, 44, 33, 5, PP(PAINT_WHITE, 0));
        PX(45, 34, PAL_WHITE); PX(46, 35, PAL_WHITE); PX(45, 36, PAL_WHITE);
        img_hline(im, 39, 33, 5, PP(PAINT_CHOCOLATE, 1));
        PX(40, 31, RP(RAMP_SKY, 7)); PX(41, 30, RP(RAMP_SKY, 7)); PX(42, 31, RP(RAMP_SKY, 7));
        PX(42, 29, RP(RAMP_SKY, 7));
    }
    /* wainscot with heart */
    for (y = 54; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = y == 54 ? 6 : (x % 16 == 0 ? 3 : 5);
            PX(x, y, RP(RAMP_WOOD, s));
        }
    img_heart(im, 32, 59, 2, PP(PAINT_ROSE, 1));
}

static void t_tile_floor(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int tx = x / 16, ty = y / 16, lx = x % 16, ly = y % 16;
            int pink = (tx + ty) & 1, ramp = pink ? RAMP_PASTEL_PINK : RAMP_PEACH;
            int base = 6, s;
            if (lx == 0 || ly == 0) { PX(x, y, RP(RAMP_STONE, 5)); continue; }
            s = base + tx_bevel(lx - 1, ly - 1, 15, 15, 1);
            /* glossy reflection streak */
            if (lx + ly >= 7 && lx + ly <= 9 && lx < 9) s = 7;
            if (lx + ly == 11 && lx < 9) s = 7;
            PX(x, y, RP(ramp, s));
        }
}

const ArtTexDef art_tex_cute[] = {
    {TEX_GRASS, "grass", 1, 1, DOOMO_ANIM_LOOP, t_grass},
    {TEX_FLOWERS, "flowers", 1, 1, DOOMO_ANIM_LOOP, t_flowers},
    {TEX_PATH, "path", 1, 1, DOOMO_ANIM_LOOP, t_path},
    {TEX_COBBLE, "cobble", 1, 1, DOOMO_ANIM_LOOP, t_cobble},
    {TEX_SAND, "sand", 1, 1, DOOMO_ANIM_LOOP, t_sand},
    {TEX_WATER, "water", 4, 8, DOOMO_ANIM_LOOP, t_water},
    {TEX_PASTEL_BRICK, "pastel_brick", 1, 1, DOOMO_ANIM_LOOP, t_pastel_brick},
    {TEX_CANDY_STRIPE, "candy_stripe", 1, 1, DOOMO_ANIM_LOOP, t_candy_stripe},
    {TEX_WOOD_FLOOR, "wood_floor", 1, 1, DOOMO_ANIM_LOOP, t_wood_floor},
    {TEX_WOOD_WALL, "wood_wall", 1, 1, DOOMO_ANIM_LOOP, t_wood_wall},
    {TEX_ROOF_TILES, "roof_tiles", 1, 1, DOOMO_ANIM_LOOP, t_roof_tiles},
    {TEX_HEDGE, "hedge", 1, 1, DOOMO_ANIM_LOOP, t_hedge},
    {TEX_PICKET_FENCE, "picket_fence", 1, 1, DOOMO_ANIM_LOOP, t_picket_fence},
    {TEX_SHOP_FRONT, "shop_front", 1, 1, DOOMO_ANIM_LOOP, t_shop_front},
    {TEX_CHURCH_WALL, "church_wall", 1, 1, DOOMO_ANIM_LOOP, t_church_wall},
    {TEX_STAINED_GLASS, "stained_glass", 1, 1, DOOMO_ANIM_LOOP, t_stained_glass},
    {TEX_CARPET, "carpet", 1, 1, DOOMO_ANIM_LOOP, t_carpet},
    {TEX_WALLPAPER_HEARTS, "wallpaper_hearts", 1, 1, DOOMO_ANIM_LOOP, t_wallpaper_hearts},
    {TEX_CLOUD_PUFF, "cloud_puff", 1, 1, DOOMO_ANIM_LOOP, t_cloud_puff},
    {TEX_TREE, "tree", 1, 1, DOOMO_ANIM_LOOP, t_tree},
    {TEX_PLAYGROUND, "playground", 1, 1, DOOMO_ANIM_LOOP, t_playground},
    {TEX_VAPE_SHOP_FRONT, "vape_shop_front", 1, 1, DOOMO_ANIM_LOOP, t_vape_shop_front},
    {TEX_APARTMENT_FRONT, "apartment_front", 1, 1, DOOMO_ANIM_LOOP, t_apartment_front},
    {TEX_WORKSHOP_FRONT, "workshop_front", 1, 1, DOOMO_ANIM_LOOP, t_workshop_front},
    {TEX_CAFE_FRONT, "cafe_front", 1, 1, DOOMO_ANIM_LOOP, t_cafe_front},
    {TEX_TILE_FLOOR, "tile_floor", 1, 1, DOOMO_ANIM_LOOP, t_tile_floor},
};
const int art_tex_cute_n = (int)(sizeof art_tex_cute / sizeof art_tex_cute[0]);
