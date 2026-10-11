/* skies.c - 256x96 panoramic skies, seamless horizontally (one full turn).
 *
 *   pastel_day   candy-blue sky, fluffy shaded cumulus, a pastel rainbow,
 *                a soft sun and rolling mint hills with lollipop trees
 *   sunset       purple-to-gold bands, a striped setting sun, gilded
 *                cloud streaks over a cottage skyline
 *   night_stars  deep navy, twinkling stars, a milky way, crescent moon and
 *                cosy lit windows on the hills
 *   hell         blood-red smoke lit from below by lava, an eclipsed sun,
 *                jagged black mountains with glowing lava falls, embers
 *   vape_storm   churning violet vapour over the town skyline, with
 *                distant lightning forks
 */
#include "art.h"

#define PX(x, y, c) img_put(im, (x), (y), (uint8_t)(c))
#define GX(x, y) img_get(im, (x), (y))

/* vertical gradient through a list of colours with a short ordered-dither
 * blend at each band edge (looks smooth, never noisy) */
static void gradient(ArtImg *im, const uint8_t *cols, const int *ys, int n)
{
    int x, y, k;
    for (y = 0; y < SKY_H; y++) {
        for (k = 0; k + 1 < n && y >= ys[k + 1]; k++) {}
        for (x = 0; x < SKY_W; x++) {
            uint8_t c = cols[k];
            if (k + 1 < n) {
                int to = ys[k + 1] - y;          /* rows until the next band */
                if (to <= 3) {
                    int th = (4 - to) * 4;       /* 4..16 of 16 */
                    if (art_bayer(x, y) < th - 2) c = cols[k + 1];
                }
            }
            PX(x, y, c);
        }
    }
}

/* fluffy cumulus: a cluster of shaded balls with a flattened base */
static void cumulus(ArtImg *im, int cx, int cy, int w, int h, const ArtMat *m, uint32_t seed, int flat)
{
    static uint8_t buf[SKY_W * SKY_H];
    ArtImg tmp;
    int i, n = 4 + w / 8, x, y, base = cy + h / 5;
    ArtRng r;
    img_init(&tmp, buf, SKY_W, SKY_H, 1, 0);
    art_seed(&r, seed);
    for (i = 0; i < n; i++) {
        int t = i * 2048 / (n - 1) - 1024;           /* -1024..1024 across */
        int ox = t * w / 2048 + art_range(&r, -2, 2);
        int hump = (art_cos(t) * h) / Q12;           /* taller in the middle */
        int rr = h / 3 + hump / 3 + art_range(&r, 0, h / 6 + 1);
        img_ball(&tmp, cx + ox, cy - hump / 2, rr + 2, rr, m, 0);
    }
    /* copy, flattening the base: the bottom row gets the shadow tone */
    for (y = 0; y < SKY_H; y++)
        for (x = 0; x < SKY_W; x++) {
            uint8_t c = buf[y * SKY_W + x];
            if (!c) continue;
            if (flat && y > base) continue;
            if (flat && (y == base || (y + 1 < SKY_H && !buf[(y + 1) * SKY_W + x]))) c = m->c[0];
            PX(x, y, c);
        }
}

static void hills(ArtImg *im, int base, int amp, int period_cnt, uint32_t seed, uint8_t fill, uint8_t rim,
                  uint8_t shade)
{
    int x, y;
    for (x = 0; x < SKY_W; x++) {
        int h = base - (amp * (art_sin(x * ART_TURN * period_cnt / SKY_W + (int)(seed & 4095u)) + Q12)) / (2 * Q12) -
                (art_noise2(x, 0, 32, SKY_W, 256, seed) * amp) / 512;
        for (y = h; y < SKY_H; y++) {
            uint8_t c = fill;
            if (y == h) c = rim;
            else if (y > h + 3 && ((x + y) & 7) == 0 && shade) c = shade;
            PX(x, y, c);
        }
    }
}

static int hill_top(const ArtImg *im, int x, uint8_t fill)
{
    /* walk up from the bottom through the hill body */
    int y = SKY_H - 1;
    while (y > 0 && img_get(im, x, y - 1) == fill) y--;
    return y - 1;
}

/* ---- pastel day ------------------------------------------------------------ */
static void s_pastel_day(ArtImg *im, int f, int n)
{
    static const uint8_t g[6] = {0, 0, 0, 0, 0, 0};
    uint8_t cols[6];
    int ys[6] = {0, 22, 44, 62, 74, 84};
    ArtMat cloud = art_mat5(RP(RAMP_VAPE, 6), RP(RAMP_SKY, 6), RP(RAMP_SKY, 7), PAL_WHITE, PAL_WHITE);
    ArtMat far_cloud = art_mat3(RP(RAMP_SKY, 6), RP(RAMP_SKY, 7), RP(RAMP_SKY, 7));
    int x, y, i;
    (void)f; (void)n; (void)g;
    cols[0] = RP(RAMP_SKY, 5); cols[1] = RP(RAMP_SKY, 6); cols[2] = RP(RAMP_SKY, 7);
    cols[3] = RP(RAMP_PASTEL_PINK, 7); cols[4] = RP(RAMP_PEACH, 7); cols[5] = RP(RAMP_CANDY, 7);
    gradient(im, cols, ys, 6);
    /* soft sun with a glow ring and little rays */
    for (y = -14; y <= 14; y++)
        for (x = -14; x <= 14; x++) {
            int d = art_dist(x * 16, y * 16) / 16;
            if (d <= 5) PX(48 + x, 20 + y, d <= 3 ? PAL_WHITE : RP(RAMP_CANDY, 7));
            else if (d <= 7) PX(48 + x, 20 + y, RP(RAMP_CANDY, 6));
            else if (d <= 10 && ((x + y) & 1)) PX(48 + x, 20 + y, RP(RAMP_SKY, 7));
        }
    for (i = 0; i < 8; i++) {
        int a = i * 512 + 256, k;
        for (k = 9; k <= 12; k++) PX(48 + k * art_cos(a) / Q12, 20 + k * art_sin(a) / Q12, RP(RAMP_CANDY, 6));
    }
    /* pastel rainbow arc (centre below the horizon) */
    {
        static const int bands[6] = {PAINT_ROSE, PAINT_PEACH, PAINT_LEMON, PAINT_MINT, PAINT_SKY, PAINT_LAVENDER};
        for (y = 0; y < SKY_H; y++)
            for (x = 120; x < 250; x++) {
                int d = art_dist((x - 184) * 16, (y - 112) * 16) / 16;
                int b = (78 - d) / 2;
                if (d > 78 || b >= 6 || b < 0) continue;
                if (((x + y) & 1) && b == 5) continue;
                PX(x, y, PP(bands[b], 2));
            }
    }
    /* far wisps, then big cumulus */
    for (i = 0; i < 6; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0xDA71u);
        cumulus(im, (int)(h % 256u), 30 + (int)((h >> 8) % 20u), 16 + (int)((h >> 16) % 14u), 6, &far_cloud, h, 1);
    }
    for (i = 0; i < 5; i++) {
        uint32_t h = art_hash((uint32_t)i, 1u, 0xDA72u);
        cumulus(im, i * 51 + (int)(h % 20u), 22 + (int)((h >> 8) % 26u), 26 + (int)((h >> 16) % 18u), 14, &cloud, h, 1);
    }
    /* rolling hills: far mint, near grass, lollipop trees */
    hills(im, 82, 10, 3, 0x111u, RP(RAMP_MINT, 5), RP(RAMP_MINT, 6), 0);
    hills(im, 90, 8, 5, 0x222u, RP(RAMP_GRASS, 5), RP(RAMP_GRASS, 6), RP(RAMP_GRASS, 4));
    for (i = 0; i < 14; i++) {
        uint32_t h = art_hash((uint32_t)i, 2u, 0xDA73u);
        int tx = (int)(h % 256u), ty = hill_top(im, tx, RP(RAMP_GRASS, 5));
        ArtMat leaf = art_mat_ramp(RAMP_GRASS, 3, 6);
        ArtMat pink = art_mat_ramp(RAMP_PASTEL_PINK, 4, 7);
        if (ty >= SKY_H - 2) continue;
        PX(tx, ty, RP(RAMP_WOOD, 3)); PX(tx, ty - 1, RP(RAMP_WOOD, 4));
        img_ball(im, tx, ty - 4, 3, 3, (i % 3 == 0) ? &pink : &leaf, 0);
    }
}

/* ---- sunset ---------------------------------------------------------------------- */
static void s_sunset(ArtImg *im, int f, int n)
{
    uint8_t cols[7];
    int ys[7] = {0, 16, 32, 46, 58, 68, 78};
    int x, y, i;
    (void)f; (void)n;
    cols[0] = RP(RAMP_VAPE, 2); cols[1] = RP(RAMP_VAPE, 3); cols[2] = RP(RAMP_VAPE, 4);
    cols[3] = RP(RAMP_PASTEL_PINK, 4); cols[4] = RP(RAMP_PEACH, 4); cols[5] = RP(RAMP_PEACH, 5);
    cols[6] = RP(RAMP_CANDY, 5);
    gradient(im, cols, ys, 7);
    /* first stars in the purple */
    for (i = 0; i < 18; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0x5E7Au);
        PX((int)(h % 256u), (int)((h >> 8) % 20u), (i & 3) ? RP(RAMP_VAPE, 6) : PAL_WHITE);
    }
    /* big striped sun sinking behind the hills */
    for (y = -24; y <= 24; y++)
        for (x = -24; x <= 24; x++) {
            int d = art_dist(x * 16, y * 16) / 16, yy = 82 + y;
            uint8_t c;
            if (d > 24) {
                if (d <= 28 && ((x + y) & 1) && yy < 90) PX(160 + x, yy, RP(RAMP_CANDY, 6));
                continue;
            }
            if (y > 0 && (y % 6) >= 4 - y / 8) continue;            /* retro stripes */
            c = y < -12 ? RP(RAMP_CANDY, 7) : (y < 0 ? RP(RAMP_CANDY, 6) : RP(RAMP_LAVA, 6));
            PX(160 + x, yy, c);
        }
    /* gilded streak clouds: dark tops, glowing undersides */
    for (i = 0; i < 9; i++) {
        uint32_t h = art_hash((uint32_t)i, 1u, 0x5E7Bu);
        int cx = (int)(h % 256u), cy = 26 + (int)((h >> 8) % 40u), w = 24 + (int)((h >> 16) % 40u);
        int th = 2 + (int)((h >> 24) % 3u);
        for (y = 0; y < th + 2; y++)
            for (x = -w / 2; x < w / 2; x++) {
                int edge = (w / 2 - art_abs(x)) / 4;
                uint8_t c;
                if (y > edge + th) continue;
                if (y < th - edge && y < 1) continue;
                c = y == 0 ? RP(RAMP_VAPE, 3) : (y >= th ? RP(RAMP_CANDY, 6) : RP(RAMP_PASTEL_PINK, 3));
                if (cy > 56) c = y >= th ? RP(RAMP_CANDY, 7) : RP(RAMP_PASTEL_PINK, 4);
                PX(cx + x, cy + y, c);
            }
    }
    /* silhouette hills with a cottage skyline and lit windows */
    hills(im, 86, 8, 4, 0x333u, RP(RAMP_VAPE, 2), RP(RAMP_VAPE, 3), 0);
    for (i = 0; i < 10; i++) {
        uint32_t h = art_hash((uint32_t)i, 2u, 0x5E7Cu);
        int hx = i * 25 + (int)(h % 12u), ty = hill_top(im, hx, RP(RAMP_VAPE, 2)) + 2;
        int hw = 6 + (int)((h >> 8) % 4u), hh = 5 + (int)((h >> 12) % 4u), k;
        for (y = 0; y < hh; y++)
            for (x = 0; x < hw; x++) PX(hx + x, ty - y, RP(RAMP_VAPE, 1));
        for (k = 0; k <= hw / 2 + 1; k++)
            for (x = k - 1; x <= hw - k; x++) PX(hx + x, ty - hh - k, RP(RAMP_VAPE, 1));
        PX(hx + 2, ty - 2, RP(RAMP_CANDY, 6));
        if (hw > 7) PX(hx + hw - 3, ty - 3, RP(RAMP_CANDY, 5));
    }
}

/* ---- night ------------------------------------------------------------------------ */
static void s_night(ArtImg *im, int f, int n)
{
    uint8_t cols[5];
    int ys[5] = {0, 26, 50, 68, 80};
    int x, y, i;
    (void)f; (void)n;
    cols[0] = RP(RAMP_SKY, 0); cols[1] = RP(RAMP_SKY, 1); cols[2] = RP(RAMP_SKY, 2);
    cols[3] = RP(RAMP_VAPE, 2); cols[4] = RP(RAMP_VAPE, 3);
    gradient(im, cols, ys, 5);
    /* milky way: a soft wavy band of faint dust across the whole sky */
    for (y = 0; y < 72; y++)
        for (x = 0; x < SKY_W; x++) {
            int cyb = 30 + 14 * art_sin(x * ART_TURN / SKY_W + 600) / Q12;
            int band = art_abs(y - cyb);
            int dust = art_fbm2(x, y, 16, 3, SKY_W, 256, 0x9172u);
            if (band < 12 && dust > 128 + band * 5) PX(x, y, dust > 170 ? RP(RAMP_SKY, 3) : RP(RAMP_SKY, 2));
            else if (band < 16 && dust > 175 + band * 3 && ((x + y) & 1)) PX(x, y, RP(RAMP_VAPE, 2));
        }
    /* stars */
    for (i = 0; i < 110; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0x9173u);
        int sx = (int)(h % 256u), sy = (int)((h >> 8) % 72u), k = (int)((h >> 16) % 16u);
        if (k == 0) {
            PX(sx, sy, PAL_WHITE);
            PX(sx - 1, sy, RP(RAMP_SKY, 6)); PX(sx + 1, sy, RP(RAMP_SKY, 6));
            PX(sx, sy - 1, RP(RAMP_SKY, 6)); PX(sx, sy + 1, RP(RAMP_SKY, 6));
        } else if (k < 3) PX(sx, sy, RP(RAMP_CANDY, 7));
        else if (k < 8) PX(sx, sy, RP(RAMP_SKY, 6));
        else PX(sx, sy, RP(RAMP_SKY, 4));
    }
    /* crescent moon with a halo */
    for (y = -16; y <= 16; y++)
        for (x = -16; x <= 16; x++) {
            int d = art_dist(x * 16, y * 16) / 16;
            int d2 = art_dist((x - 5) * 16, (y + 3) * 16) / 16;
            if (d <= 9 && d2 > 8) PX(200 + x, 24 + y, (x + y < -6) ? PAL_WHITE : RP(RAMP_CANDY, 7));
            else if (d <= 9 && d2 <= 8) continue;
            else if (d <= 13 && ((x + y) & 1)) PX(200 + x, 24 + y, RP(RAMP_SKY, 2));
        }
    PX(196, 22, RP(RAMP_CANDY, 6)); PX(195, 27, RP(RAMP_CANDY, 6));
    /* shooting star */
    for (i = 0; i < 9; i++) PX(80 + i, 12 + i / 2, i < 6 ? RP(RAMP_SKY, 4 + i / 2) : PAL_WHITE);
    /* dark hills with cosy windows */
    hills(im, 84, 10, 3, 0x444u, RP(RAMP_SKY, 1), RP(RAMP_SKY, 2), 0);
    hills(im, 91, 6, 6, 0x555u, RP(RAMP_SKY, 0), RP(RAMP_SKY, 1), 0);
    for (i = 0; i < 12; i++) {
        uint32_t h = art_hash((uint32_t)i, 1u, 0x9174u);
        int hx = (int)(h % 256u), ty = hill_top(im, hx, RP(RAMP_SKY, 0)) + 2;
        if (ty > 92) continue;
        img_rect(im, hx, ty - 4, 6, 5, RP(RAMP_SKY, 0));
        PX(hx + 1, ty - 5, RP(RAMP_SKY, 0)); PX(hx + 4, ty - 5, RP(RAMP_SKY, 0));
        PX(hx + 2, ty - 6, RP(RAMP_SKY, 0)); PX(hx + 3, ty - 6, RP(RAMP_SKY, 0));
        PX(hx + 2, ty - 3, RP(RAMP_CANDY, 5));
        if (h & 16u) PX(hx + 4, ty - 2, RP(RAMP_CANDY, 6));
    }
}

/* ---- hell --------------------------------------------------------------------------- */
static void s_hell(ArtImg *im, int f, int n)
{
    uint8_t cols[6];
    int ys[6] = {0, 18, 38, 56, 68, 78};
    int x, y, i;
    (void)f; (void)n;
    cols[0] = RP(RAMP_BLOOD, 0); cols[1] = RP(RAMP_BLOOD, 1); cols[2] = RP(RAMP_BLOOD, 2);
    cols[3] = RP(RAMP_BLOOD, 3); cols[4] = RP(RAMP_LAVA, 2); cols[5] = RP(RAMP_LAVA, 3);
    gradient(im, cols, ys, 6);
    /* eclipsed sun: black disc with a burning corona */
    for (y = -18; y <= 18; y++)
        for (x = -18; x <= 18; x++) {
            int d = art_dist(x * 16, y * 16) / 16;
            int a = art_atan2(y, x);
            int flare = (art_sin(a * 7) + Q12) / 1365;          /* 0..6 */
            if (d <= 9) PX(96 + x, 26 + y, RP(RAMP_BLOOD, 0));
            else if (d == 10) PX(96 + x, 26 + y, RP(RAMP_LAVA, 7));
            else if (d <= 11 + flare / 2) PX(96 + x, 26 + y, RP(RAMP_LAVA, 5));
            else if (d <= 13 + flare && ((x + y) & 1)) PX(96 + x, 26 + y, RP(RAMP_LAVA, 3));
        }
    /* smoke banks lit from below */
    for (y = 0; y < 70; y++)
        for (x = 0; x < SKY_W; x++) {
            int v = art_fbm2(x, y * 2, 32, 4, SKY_W, 512, 0x4E11u);
            int band = art_sin(y * ART_TURN / 40 + art_sin(x * ART_TURN / SKY_W * 2) / 4);
            int sm = v + band / 80;
            if (sm > 150) {
                int below = art_fbm2(x, (y + 2) * 2, 32, 4, SKY_W, 512, 0x4E11u) +
                            art_sin((y + 2) * ART_TURN / 40 + art_sin(x * ART_TURN / SKY_W * 2) / 4) / 80;
                uint8_t c = RP(RAMP_BLOOD, 1);
                if (below <= 150) c = y > 40 ? RP(RAMP_LAVA, 4) : RP(RAMP_LAVA, 3);
                else if (sm > 175) c = RP(RAMP_STONE, 0);
                if (c == RP(RAMP_BLOOD, 1) && GX(x, y) == RP(RAMP_BLOOD, 0)) c = RP(RAMP_STONE, 0);
                PX(x, y, c);
            }
        }
    /* jagged mountains with lava falls */
    for (x = 0; x < SKY_W; x++) {
        int h = 66 + (art_abs(art_wrap(x * 3, 64) - 32) * 14) / 32 - (art_noise2(x, 0, 16, SKY_W, 256, 0x4E12u) * 20) / 256;
        int h2 = 78 + (art_abs(art_wrap(x * 5 + 17, 48) - 24) * 8) / 24;
        for (y = h; y < SKY_H; y++) PX(x, y, y == h ? RP(RAMP_BLOOD, 2) : RP(RAMP_STONE, 0));
        for (y = h2; y < SKY_H; y++) PX(x, y, y == h2 ? RP(RAMP_LAVA, 2) : RP(RAMP_BLOOD, 0));
    }
    for (i = 0; i < 7; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0x4E13u);
        int lx = (int)(h % 256u), top = 70 + (int)((h >> 8) % 8u);
        for (y = top; y < SKY_H; y++) {
            uint8_t cur = GX(lx, y);
            if (cur != RP(RAMP_STONE, 0) && cur != RP(RAMP_BLOOD, 0)) continue;
            PX(lx, y, RP(RAMP_LAVA, 6));
            PX(lx + 1, y, RP(RAMP_LAVA, 4));
            if (y % 3 == 0) PX(lx - 1, y, RP(RAMP_LAVA, 3));
        }
    }
    /* embers */
    for (i = 0; i < 40; i++) {
        uint32_t h = art_hash((uint32_t)i, 1u, 0x4E14u);
        PX((int)(h % 256u), 30 + (int)((h >> 8) % 50u), (i & 1) ? RP(RAMP_LAVA, 6) : RP(RAMP_LAVA, 4));
    }
}

/* ---- vape storm ------------------------------------------------------------------------- */
static void bolt(ArtImg *im, int x, int y, int len, uint32_t seed)
{
    ArtRng r;
    int k;
    art_seed(&r, seed);
    for (k = 0; k < len; k++) {
        PX(x, y, PAL_WHITE);
        PX(x + 1, y, PP(PAINT_CYAN, 2));
        PX(x - 1, y, PP(PAINT_MAGENTA, 1));
        y++;
        x += art_range(&r, -1, 1) + ((k % 7 == 3) ? art_range(&r, -2, 2) : 0);
        if (k == len / 2) {
            int bx = x, by = y, j;
            for (j = 0; j < len / 3; j++) {
                PX(bx, by, PP(PAINT_CYAN, 2));
                by++;
                bx += art_range(&r, 0, 2);
            }
        }
    }
}

static void s_vape_storm(ArtImg *im, int f, int n)
{
    uint8_t cols[5];
    int ys[5] = {0, 20, 44, 64, 78};
    int x, y, i;
    (void)n;
    cols[0] = RP(RAMP_VAPE, 0); cols[1] = RP(RAMP_VAPE, 1); cols[2] = RP(RAMP_VAPE, 2);
    cols[3] = RP(RAMP_VAPE, 3); cols[4] = PP(PAINT_MAGENTA, 0);
    gradient(im, cols, ys, 5);
    /* churning swirls */
    for (y = 0; y < 80; y++)
        for (x = 0; x < SKY_W; x++) {
            int v = art_fbm2(x, y, 32, 4, SKY_W, 256, 0x7A90u);
            int sw = art_sin(x * ART_TURN / 64 + y * ART_TURN / 24 + v * 20);
            int s = -1;
            if (v > 150) s = (sw > 2000) ? 4 : 3;
            else if (v > 128 && sw > 3000) s = 3;
            if (v > 175) s = (sw > 0) ? 5 : 4;
            if (s >= 0) {
                if (f == 1) s++;
                PX(x, y, RP(RAMP_VAPE, s));
            }
        }
    /* magenta rims on the cloud undersides */
    for (y = 1; y < 80; y++)
        for (x = 0; x < SKY_W; x++) {
            uint8_t c = GX(x, y), up = GX(x, y - 1);
            if (up >= RP(RAMP_VAPE, 3) && up <= RP(RAMP_VAPE, 6) && c < RP(RAMP_VAPE, 3) && c >= PAL_RAMP(RAMP_VAPE, 0))
                PX(x, y, f == 1 ? PP(PAINT_MAGENTA, 2) : PP(PAINT_MAGENTA, 0));
        }
    /* the town skyline under the storm */
    for (x = 0; x < SKY_W; x++) {
        int bx = x % 32;
        int h = 84 - ((bx < 14) ? 8 + (x / 32 % 3) * 2 : (bx < 24 ? 4 : 0));
        if (bx < 14) h -= 7 - art_abs(bx - 7);          /* pointy roofs */
        for (y = h; y < SKY_H; y++) PX(x, y, RP(RAMP_VAPE, 0));
        if (bx > 3 && bx < 11 && (bx & 1) && x / 32 % 2 == 0) PX(x, 86, f == 1 ? RP(RAMP_CANDY, 7) : RP(RAMP_CANDY, 4));
    }
    for (x = 0; x < SKY_W; x++) PX(x, 88, RP(RAMP_VAPE, 1));
    /* far-off forks of lightning frozen in the clouds (a full-screen flash
     * is left to the renderer: TINT_PURPLE / TINT_NONE pulses) */
    bolt(im, 70, 6, 14, 0xB017u);
    bolt(im, 196, 10, 10, 0xB018u);
    /* glitter in the vapour */
    for (i = 0; i < 24; i++) {
        uint32_t h = art_hash((uint32_t)i, (uint32_t)f, 0x7A91u);
        PX((int)(h % 256u), (int)((h >> 8) % 70u), (i & 1) ? PP(PAINT_MAGENTA, 2) : PP(PAINT_CYAN, 1));
    }
}

const ArtSkyDef art_skies[] = {
    {SKY_PASTEL_DAY, "pastel_day", 1, 1, DOOMO_ANIM_LOOP, s_pastel_day},
    {SKY_SUNSET, "sunset", 1, 1, DOOMO_ANIM_LOOP, s_sunset},
    {SKY_NIGHT_STARS, "night_stars", 1, 1, DOOMO_ANIM_LOOP, s_night},
    {SKY_HELL, "hell", 1, 1, DOOMO_ANIM_LOOP, s_hell},
    {SKY_VAPE_STORM, "vape_storm", 1, 1, DOOMO_ANIM_LOOP, s_vape_storm},
};
const int art_skies_n = (int)(sizeof art_skies / sizeof art_skies[0]);
