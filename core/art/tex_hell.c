/* tex_hell.c - the HELL texture set: what the invasion brings.
 *
 * Grotesque Doom-style surfaces: veiny flesh, ossuary bone walls, bloody
 * bricks, molten lava, rusted tech, skull mosaics, staring demon eyes,
 * chains, billowing vape-cloud walls, gore floors, pentagrams, infected
 * grass and toxic slime - plus the doors, exit sign and switch used by
 * the Hell Breach levels. Animated ones (lava, slime, vape cloud, demon
 * eyes, tech lights, exit sign) loop seamlessly in time.
 */
#include "art.h"

#define PX(x, y, c) img_put(im, (x), (y), (uint8_t)(c))
#define GX(x, y) img_get(im, (x), (y))

/* ---- flesh -------------------------------------------------------------------- */
static void flesh_base(ArtImg *im, uint32_t seed, int lo)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            ArtCell lump, vein;
            int v = art_fbm(x, y, 16, 2, 64, seed);
            int fib = art_sin(y * ART_TURN / 5 + x * ART_TURN / 32 + v * 24);
            int s, dx, dy, r2, e;
            /* soft rounded muscle lumps, each shaded like a wet ball */
            art_worley_j(x, y, 4, 64, seed + 7, 11, &lump);
            dx = x - lump.fx; dy = y - lump.fy;
            r2 = lump.d1 / 16;
            s = lo + 2;
            if (dx + dy < -3 && r2 < 7) s = lo + 3;
            if (dx + dy > 4) s = lo + 1;
            if (lump.d2 - lump.d1 < 40) s = lo;                   /* creases */
            if (fib > 3600 && s > lo) s--;                       /* striation */
            if (v > 170) s++;
            PX(x, y, RP(RAMP_FLESH, s));
            /* raised veins: purple tubes with a wet top highlight */
            art_worley_j(x, y, 3, 64, seed + 19, 14, &vein);
            e = vein.d2 - vein.d1;
            if (e < 13) PX(x, y, RP(RAMP_VAPE, 3));
            else if (e < 22) PX(x, y, RP(RAMP_VAPE, 2));
            else if (e < 28) PX(x, y, RP(RAMP_FLESH, lo - 1));
        }
    /* wet glints along vein tops and lump highlights */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            uint8_t c = GX(x, y), below = GX(x, y + 1);
            if (c == RP(RAMP_VAPE, 3) && GX(x, y - 1) != RP(RAMP_VAPE, 3) && (art_hash((uint32_t)x, (uint32_t)y, seed) % 3u) == 0)
                PX(x, y, RP(RAMP_VAPE, 6));
            else if (c == RP(RAMP_FLESH, lo + 3) && below == RP(RAMP_FLESH, lo + 3) &&
                     (art_hash((uint32_t)x, (uint32_t)y, seed + 1) % 23u) == 0)
                PX(x, y, RP(RAMP_FLESH, 7));
        }
}

static void pustule(ArtImg *im, int x, int y, int r)
{
    ArtMat m = art_mat5(RP(RAMP_FLESH, 3), RP(RAMP_FLESH, 5), RP(RAMP_FLESH, 6), RP(RAMP_SICK, 6),
                        RP(RAMP_SICK, 7));
    int i;
    for (i = -r; i <= r; i++) img_shift(im, x + i + 1, y + r + 1, -2);
    img_ball(im, x, y, r, r, &m, 0);
    img_put(im, x - r / 2, y - r / 2, PAL_WHITE);
}

static void t_flesh_wall(ArtImg *im, int f, int n)
{
    (void)f; (void)n;
    flesh_base(im, 0xF1E5u, 3);
    pustule(im, 14, 20, 3);
    pustule(im, 46, 41, 2);
    pustule(im, 36, 10, 2);
    pustule(im, 8, 52, 2);
    img_put(im, 25, 33, PAL_WHITE);
    img_put(im, 55, 25, PAL_WHITE);
}

/* ---- bone -------------------------------------------------------------------- */
static void skull(ArtImg *im, int cx, int cy, uint32_t h)
{
    ArtMat bone = art_mat_ramp(RAMP_BONE, 3, 7);
    int x;
    /* cranium + jaw */
    img_ball(im, cx, cy - 1, 6, 5, &bone, 0);
    img_ball(im, cx, cy + 3, 4, 3, &bone, 0);
    /* eye sockets */
    img_ellipse(im, cx - 3, cy + 1, 1, 1, RP(RAMP_BONE, 0));
    img_ellipse(im, cx + 3, cy + 1, 1, 1, RP(RAMP_BONE, 0));
    PX(cx - 3, cy - 1, RP(RAMP_BONE, 2)); PX(cx + 3, cy - 1, RP(RAMP_BONE, 2));
    PX(cx - 2, cy - 1, RP(RAMP_BONE, 2)); PX(cx + 2, cy - 1, RP(RAMP_BONE, 2));
    if (h % 4u == 0) { PX(cx - 3, cy + 1, RP(RAMP_LAVA, 5)); PX(cx + 3, cy + 1, RP(RAMP_LAVA, 5)); }
    /* nose */
    PX(cx, cy + 3, RP(RAMP_BONE, 1)); PX(cx - 1, cy + 3, RP(RAMP_BONE, 2));
    /* teeth */
    for (x = -3; x <= 2; x++) {
        PX(cx + x, cy + 5, (x & 1) ? RP(RAMP_BONE, 2) : RP(RAMP_BONE, 7));
        PX(cx + x, cy + 6, RP(RAMP_BONE, 3));
    }
}

static void femur(ArtImg *im, int x0, int y, int len)
{
    int x;
    ArtMat bone = art_mat_ramp(RAMP_BONE, 3, 7);
    for (x = 2; x < len - 2; x++) {
        PX(x0 + x, y - 1, RP(RAMP_BONE, 6));
        PX(x0 + x, y, RP(RAMP_BONE, 5));
        PX(x0 + x, y + 1, RP(RAMP_BONE, 3));
    }
    img_ball(im, x0 + 1, y - 1, 2, 1, &bone, 0);
    img_ball(im, x0 + 1, y + 1, 2, 1, &bone, 0);
    img_ball(im, x0 + len - 2, y - 1, 2, 1, &bone, 0);
    img_ball(im, x0 + len - 2, y + 1, 2, 1, &bone, 0);
}

static void t_bone_wall(ArtImg *im, int f, int n)
{
    int x, y, i;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            PX(x, y, RP(RAMP_WOOD, (art_noise(x, y, 4, 64, 0xB0E1u) > 150) ? 1 : 0));
    /* bone rows */
    for (i = 0; i < 4; i++) {
        femur(im, i * 16 + 1, 18, 15);
        femur(im, i * 16 + 9, 21, 15);
        femur(im, i * 16 + 5, 50, 15);
        femur(im, i * 16 + 13, 53, 15);
    }
    /* skull rows */
    for (i = 0; i < 4; i++) {
        skull(im, i * 16 + 8, 7, art_hash((uint32_t)i, 0u, 0xB0E2u));
        skull(im, i * 16 + 16, 37, art_hash((uint32_t)i, 1u, 0xB0E2u));
    }
}

/* ---- bricks ------------------------------------------------------------------- */
static void drip(ArtImg *im, int x, int y, int len, int glossy)
{
    int k;
    for (k = 0; k < len; k++) {
        PX(x, y + k, RP(RAMP_BLOOD, glossy ? 5 : 4));
        PX(x + 1, y + k, RP(RAMP_BLOOD, 3));
        if (glossy && k > 0 && k < len - 1 && (k % 3) == 1) PX(x, y + k, RP(RAMP_BLOOD, 7));
    }
    PX(x, y + len, RP(RAMP_BLOOD, 5));
    PX(x + 1, y + len, RP(RAMP_BLOOD, 4));
    PX(x, y + len + 1, RP(RAMP_BLOOD, 3));
    PX(x + 1, y + len + 1, RP(RAMP_BLOOD, 2));
    if (glossy) PX(x, y + len, RP(RAMP_BLOOD, 7));
}

static void t_blood_brick(ArtImg *im, int f, int n)
{
    int x, y, i;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            TxCell c;
            int s, base;
            tx_brick(x, y, 16, 8, 8, 1, &c);
            if (c.mortar || c.ly == 7) { PX(x, y, RP(RAMP_BLOOD, c.mortar ? 0 : 1)); continue; }
            base = 3 - (int)(c.id % 3u == 0);
            s = base + tx_bevel(c.lx - 1, c.ly - 1, c.w - 1, c.h - 2, 1);
            if (art_noise(x, y, 4, 64, 0xB1D0u) > 190) s--;
            if ((art_hash((uint32_t)x, (uint32_t)y, 0xB1D1u) % 17u) == 0) s = base + 1;
            PX(x, y, RP(s < 2 ? RAMP_BLOOD : RAMP_BLOOD, s));
        }
    /* stains */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            if (art_fbm(x, y, 16, 2, 64, 0xB1D2u) > 168) img_shift(im, x, y, -1);
    /* drips running from the mortar lines */
    for (i = 0; i < 9; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0xB1D3u);
        int dx = (int)(h % 64u), row = (int)((h >> 8) % 8u);
        drip(im, dx, row * 8 + 1, 2 + (int)((h >> 12) % 9u), (int)((h >> 20) & 1u));
    }
}

/* ---- lava -------------------------------------------------------------------- */
static void t_lava(ArtImg *im, int f, int n)
{
    int x, y, ph = tx_phase(f, n);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            ArtCell c;
            int v = art_fbm(x, y, 32, 3, 64, 0x1A7Au);
            int sw = art_sin(x * ART_TURN / 32 + art_sin(y * ART_TURN / 64 + ph) / 2 + v * 24 + ph);
            int edge, crack;
            uint8_t col;
            art_worley_j(x, y, 5, 64, 0x1A7Bu, 13, &c);
            edge = c.d2 - c.d1;
            /* cracks between crust plates breathe with the loop */
            crack = 20 + (art_sin(ph + (int)(c.id & 4095u)) * 8) / Q12;
            if ((c.id % 5u) < 2 || edge < crack) {
                /* molten: open pools and the glowing cracks */
                col = RP(RAMP_LAVA, 4);
                if (sw > 2600) col = RP(RAMP_LAVA, 5);
                if (sw > 3800) col = RP(RAMP_LAVA, 6);
                if (sw > 4040 && v > 120) col = RP(RAMP_LAVA, 7);
                if (sw < -3000) col = RP(RAMP_LAVA, 3);
                if ((c.id % 5u) >= 2 && edge < crack / 2) col = RP(RAMP_LAVA, 6);
            } else if (edge < crack + 14) {
                col = RP(RAMP_LAVA, 2);                         /* hot rim */
            } else {
                /* cooled crust plate, lit from the top-left */
                int dx = x - c.fx, dy = y - c.fy;
                int s = (dx + dy < -3) ? 2 : ((dx + dy > 4) ? 0 : 1);
                col = RP(RAMP_BLOOD, s);
                if (s == 2 && edge > crack + 40 && dx + dy < -6) col = RP(RAMP_WOOD, 2);
                if ((art_hash((uint32_t)x, (uint32_t)y, 0x1A7Cu) % 13u) == 0) col = RP(RAMP_LAVA, 3);
            }
            PX(x, y, col);
        }
    /* bubbles in the molten pools: grow over the loop, pop on the last frame */
    {
        int i;
        for (i = 0; i < 6; i++) {
            uint32_t h = art_hash((uint32_t)i, 0u, 0x1A7Du);
            int bx = (int)(h % 64u), by = (int)((h >> 8) % 64u);
            int st = art_wrap(f + (int)((h >> 16) % 4u), 4);
            uint8_t cur = GX(bx, by);
            if (cur < PAL_RAMP(RAMP_LAVA, 3)) continue;
            if (st == 1) { PX(bx, by, RP(RAMP_LAVA, 7)); PX(bx + 1, by, RP(RAMP_LAVA, 5)); }
            else if (st == 2) {
                img_ring(im, bx, by, 1, 2, RP(RAMP_LAVA, 6));
                PX(bx - 1, by - 1, RP(RAMP_LAVA, 7));
            } else if (st == 3) {
                PX(bx - 2, by - 1, RP(RAMP_LAVA, 7)); PX(bx + 2, by - 2, RP(RAMP_LAVA, 7));
                PX(bx, by - 3, RP(RAMP_LAVA, 6)); PX(bx, by, RP(RAMP_LAVA, 3));
            }
        }
    }
}

/* ---- metal --------------------------------------------------------------------- */
static void rivet(ArtImg *im, int x, int y)
{
    PX(x, y, RP(RAMP_STEEL, 6));
    PX(x + 1, y, RP(RAMP_STEEL, 4));
    PX(x, y + 1, RP(RAMP_STEEL, 4));
    PX(x + 1, y + 1, RP(RAMP_STEEL, 2));
    PX(x + 2, y + 2, RP(RAMP_STEEL, 1));
}

static void t_rust_metal(ArtImg *im, int f, int n)
{
    int x, y, i;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int lx = x % 32, ly = y % 32;
            int brush = art_noise(x, y / 4, 4, 64, 0x2057u);
            int rust = art_fbm(x, y, 16, 3, 64, 0x2058u) + (art_noise(x, y, 8, 64, 0x2059u) - 128) / 4;
            uint8_t c = RP(RAMP_STEEL, brush > 150 ? 4 : 3);
            if (rust > 150) c = RP(RAMP_WOOD, 3);
            if (rust > 168) c = RP(RAMP_WOOD, 2);
            if (rust > 185) c = PP(PAINT_RUST, 1);
            if (rust > 200) c = PP(PAINT_RUST, 2);
            if (rust > 140 && rust <= 150) c = RP(RAMP_STEEL, 2);
            if (lx == 0 || ly == 0) c = RP(RAMP_STEEL, 0);
            else if (lx == 1 || ly == 1) c = RP(RAMP_STEEL, 5);
            else if (lx == 31 || ly == 31) c = RP(RAMP_STEEL, 1);
            PX(x, y, c);
        }
    for (i = 0; i < 4; i++) {
        int px = (i & 1) * 32, py = (i >> 1) * 32;
        rivet(im, px + 4, py + 4);
        rivet(im, px + 26, py + 4);
        rivet(im, px + 4, py + 26);
        rivet(im, px + 26, py + 26);
        /* rust tears below the lower rivets */
        for (y = 0; y < 5; y++) {
            PX(px + 5, py + 29 + y, PP(PAINT_RUST, y < 3 ? 1 : 0));
            PX(px + 27, py + 29 + y, PP(PAINT_RUST, y < 2 ? 1 : 0));
        }
    }
    /* scratches */
    img_line(im, 38, 12, 50, 16, RP(RAMP_STEEL, 6));
    img_line(im, 10, 44, 18, 41, RP(RAMP_STEEL, 6));
}

static void light_dot(ArtImg *im, int x, int y, int paint, int on)
{
    if (on) {
        PX(x - 1, y, PP(paint, 0)); PX(x + 2, y, PP(paint, 0));
        PX(x, y - 1, PP(paint, 0)); PX(x + 1, y - 1, PP(paint, 0));
        PX(x, y + 2, PP(paint, 0)); PX(x + 1, y + 2, PP(paint, 0));
        PX(x, y, PAL_WHITE); PX(x + 1, y, PP(paint, 2));
        PX(x, y + 1, PP(paint, 2)); PX(x + 1, y + 1, PP(paint, 1));
    } else {
        PX(x, y, RP(RAMP_STEEL, 1)); PX(x + 1, y, RP(RAMP_STEEL, 1));
        PX(x, y + 1, RP(RAMP_STEEL, 0)); PX(x + 1, y + 1, RP(RAMP_STEEL, 0));
        PX(x, y, PP(paint, 0));
    }
}

static void t_tech_panel(ArtImg *im, int f, int n)
{
    int x, y, i;
    (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = 3 + tx_bevel(x, y, 64, 64, 2);
            if ((x == 32 || y == 32) && 0) s = 1;
            PX(x, y, RP(RAMP_STEEL, s));
        }
    /* vent grille */
    for (y = 5; y < 13; y++)
        for (x = 6; x < 58; x++)
            PX(x, y, (y & 1) ? RP(RAMP_STEEL, 0) : RP(RAMP_STEEL, (x == 6) ? 5 : 4));
    /* screen */
    for (y = 16; y < 41; y++)
        for (x = 6; x < 41; x++) {
            uint8_t c = RP(RAMP_MINT, 0);
            if (x == 6 || y == 16) c = RP(RAMP_STEEL, 1);
            else if (x == 40 || y == 40) c = RP(RAMP_STEEL, 5);
            else if ((y & 1) == 0) c = RP(RAMP_MINT, 1);
            PX(x, y, c);
        }
    /* text lines that change per frame + a blinking cursor */
    for (i = 0; i < 6; i++) {
        int len = 6 + (int)(art_hash((uint32_t)i, (uint32_t)f, 0x7EC0u) % 24u);
        int ly = 19 + i * 3 + (i > 2);
        for (x = 0; x < len; x++)
            if ((art_hash((uint32_t)x, (uint32_t)i, 0x7EC1u + (uint32_t)f) % 5u) != 0)
                PX(9 + x, ly, i == 0 ? PP(PAINT_LIME, 2) : RP(RAMP_MINT, 5));
    }
    if (f & 1) { PX(30, 37, RP(RAMP_MINT, 7)); PX(31, 37, RP(RAMP_MINT, 7)); PX(30, 36, RP(RAMP_MINT, 7)); PX(31, 36, RP(RAMP_MINT, 7)); }
    /* lights column */
    {
        static const int lp[4] = {PAINT_CHERRY, PAINT_LIME, PAINT_LEMON, PAINT_CYAN};
        for (i = 0; i < 4; i++) {
            int on = ((art_hash((uint32_t)i, (uint32_t)f, 0x7EC2u) & 3u) != 0) || i == f % 4;
            for (y = 0; y < 6; y++)
                for (x = 0; x < 10; x++) PX(46 + x, 16 + i * 7 + y, RP(RAMP_STEEL, (x == 0 || y == 0) ? 1 : 2));
            light_dot(im, 48, 18 + i * 7, lp[i], on);
            img_hline(im, 52, 18 + i * 7, 3, RP(RAMP_STEEL, 4));
            img_hline(im, 52, 20 + i * 7, 2, RP(RAMP_STEEL, 4));
        }
    }
    /* pipes */
    for (y = 0; y < 4; y++)
        for (x = 0; x < 64; x++) {
            static const int sh[4] = {5, 6, 4, 2};
            PX(x, 46 + y, RP(RAMP_STEEL, sh[y]));
            PX(x, 52 + y, RP(RAMP_BLOOD, sh[y]));
        }
    for (x = 4; x < 64; x += 16) {
        PX(x, 45, RP(RAMP_STEEL, 1)); PX(x, 50, RP(RAMP_STEEL, 1));
        PX(x + 1, 51, RP(RAMP_BLOOD, 1)); PX(x + 1, 56, RP(RAMP_BLOOD, 1));
    }
    for (y = 58; y < 62; y++)
        for (x = 4; x < 60; x++)
            PX(x, y, art_wrap(x - y, 8) < 4 ? RP(RAMP_CANDY, 4) : RP(RAMP_STONE, 0));
}

/* ---- skull mosaic ---------------------------------------------------------------- */
static void t_skull_mosaic(ArtImg *im, int f, int n)
{
    static const char *const bm[16] = {
        "g..............g",
        "....XXXXXXXX....",
        "...XXXXXXXXXX...",
        "..XXXXXXXXXXXX..",
        "..XXXXXXXXXXXX..",
        "..XEEEXXXXEEEX..",
        "..XERRXXXXRREX..",
        "..XEEEXXXXEEEX..",
        "..XXXXXNNXXXXX..",
        "...XXXXNNXXXX...",
        "....XXXXXXXX....",
        "....TDTDTDTD....",
        "....XTXTXTXX....",
        ".....XXXXXX.....",
        "g..............g",
        "gggggggggggggggg"};
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int tx = x / 4, ty = y / 4, lx = x % 4, ly = y % 4, s;
            char k = bm[ty][tx];
            int ramp, base;
            uint32_t h = art_hash((uint32_t)tx, (uint32_t)ty, 0x5C11u);
            if (lx == 3 || ly == 3) { PX(x, y, RP(RAMP_STONE, 0)); continue; }
            switch (k) {
            case 'X': ramp = RAMP_BONE; base = 5 + (int)(h % 4u == 0) - (int)(tx >= 10); break;
            case 'T': ramp = RAMP_BONE; base = 7; break;
            case 'D': ramp = RAMP_BONE; base = 1; break;
            case 'E': ramp = RAMP_BLOOD; base = 1; break;
            case 'R': ramp = RAMP_LAVA; base = 5; break;
            case 'N': ramp = RAMP_BONE; base = 1; break;
            case 'g': ramp = RAMP_CANDY; base = 3; break;
            default: ramp = RAMP_BLOOD; base = 2 + (int)(h % 3u == 0); break;
            }
            s = base;
            if (lx == 0 && ly == 0) s = base + 1;
            else if (lx == 2 && ly == 2) s = base - 1;
            PX(x, y, RP(ramp, s));
        }
}

/* ---- demon eyes ------------------------------------------------------------------- */
static void demon_eye(ArtImg *im, int cx, int cy, int rx, int ry, int gaze, int open, int iris_ramp)
{
    int x, y;
    int ory = ry * open / 4;
    /* swollen lid around the eye */
    for (y = -ry - 2; y <= ry + 2; y++)
        for (x = -rx - 2; x <= rx + 2; x++) {
            int64_t lim = (int64_t)(ry + 2) * (Q12 - (int64_t)x * x * Q12 / ((rx + 2) * (rx + 2)));
            if ((int64_t)art_abs(y) * Q12 > lim) continue;
            PX(cx + x, cy + y, RP(RAMP_FLESH, y < 0 ? 5 : 2));
        }
    for (y = -ry - 1; y <= ry + 1; y++)
        for (x = -rx - 1; x <= rx + 1; x++) {
            int64_t lim = (int64_t)(ry + 1) * (Q12 - (int64_t)x * x * Q12 / ((rx + 1) * (rx + 1)));
            if ((int64_t)art_abs(y) * Q12 > lim) continue;
            PX(cx + x, cy + y, RP(RAMP_FLESH, 1));
        }
    if (ory <= 0) {
        /* closed: lash line */
        for (x = -rx; x <= rx; x++) PX(cx + x, cy, RP(RAMP_FLESH, 0));
        for (x = -rx + 1; x < rx; x += 2) PX(cx + x, cy + 1, RP(RAMP_FLESH, 0));
        return;
    }
    for (y = -ory; y <= ory; y++)
        for (x = -rx; x <= rx; x++) {
            int64_t lim = (int64_t)ory * (Q12 - (int64_t)x * x * Q12 / (rx * rx));
            int ix = x - gaze, ir = ry - 1 > 1 ? ry - 1 : 1;
            int di = ix * ix + y * y;
            uint8_t c;
            if ((int64_t)art_abs(y) * Q12 > lim) continue;
            c = RP(RAMP_BONE, y < 0 ? 6 : 5);
            if (art_abs(x) > rx * 2 / 3 && ((x + y) & 1)) c = RP(RAMP_BLOOD, 5);   /* veins */
            if (di <= ir * ir) {
                c = RP(iris_ramp, di * 2 <= ir * ir ? 6 : 4);
                if (ix == 0 || (ir > 2 && art_abs(ix) == 0)) c = RP(RAMP_BLOOD, 0);
                if (ix == 0 && art_abs(y) <= ir) c = PAL_BLACK;
            }
            if (y == -ory) c = art_darker(c, 2);                  /* lid shadow */
            PX(cx + x, cy + y, c);
        }
    PX(cx + gaze - 1, cy - ory / 2 - (ory > 1), PAL_WHITE);
}

static void t_demon_eyes(ArtImg *im, int f, int n)
{
    static const int eyes[7][4] = {{16, 14, 8, 4}, {46, 10, 5, 3}, {38, 34, 9, 5}, {10, 42, 5, 3},
                                   {56, 52, 6, 3}, {22, 56, 4, 2}, {58, 28, 4, 2}};
    int i;
    (void)n;
    flesh_base(im, 0xE7E5u, 2);
    for (i = 0; i < 7; i++) {
        /* frame 0: all staring at you, gazes slightly askew; frame 1 (the
         * flash): half of them blink while the rest dart sideways */
        int gz = (i % 3) - 1;
        int open = 4;
        if (f == 1) {
            gz = (i & 1) ? -2 : 2;
            open = (i & 1) ? 0 : ((i % 3 == 0) ? 1 : 4);
        }
        if (eyes[i][2] < 6) gz = gz / 2;
        demon_eye(im, eyes[i][0], eyes[i][1], eyes[i][2], eyes[i][3], gz, open,
                  (i % 3 == 0) ? RAMP_LAVA : ((i % 3 == 1) ? RAMP_CANDY : RAMP_SICK));
    }
}

/* ---- chains ---------------------------------------------------------------------- */
static void t_chains(ArtImg *im, int f, int n)
{
    static const int cx[3] = {10, 33, 53};
    int x, y, c;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            ArtCell w;
            int v = art_fbm(x, y, 16, 3, 64, 0xC4A1u), s = v > 140 ? 2 : 1;
            art_worley_j(x, y, 4, 64, 0xC4A2u, 12, &w);
            if (w.d2 - w.d1 < 16) s = 0;
            else if (x - w.fx + y - w.fy < -6) s++;
            PX(x, y, RP(RAMP_STONE, s));
        }
    for (c = 0; c < 3; c++) {
        int off = c * 5;
        /* wall shadow */
        for (y = 0; y < 64; y++) {
            img_shift(im, cx[c] + 5, y + 2, -1);
            img_shift(im, cx[c] + 4, y + 2, -1);
        }
        for (y = 0; y < 64; y++) {
            int ly = art_wrap(y + off, 12);
            if (ly < 10) {
                /* front-facing oval link (ring, 2 px thick) */
                int dy2 = 2 * ly - 9;
                for (x = -4; x <= 3; x++) {
                    int dx2 = 2 * x + 1;
                    int o = dx2 * dx2 * 100 + dy2 * dy2 * 64;   /* ellipse 8x10 */
                    int s;
                    if (o > 64 * 100 + 300) continue;
                    if (dx2 * dx2 * 100 / 4 + dy2 * dy2 * 64 * 100 / 36 / 100 * 1 < 0) continue;
                    if (art_abs(dx2) <= 3 && art_abs(dy2) <= 5) continue;    /* hole */
                    s = x < -1 ? 6 : (x < 1 ? 5 : (x < 3 ? 3 : 2));
                    if (dy2 > 5) s--;
                    PX(cx[c] + x, y, RP(RAMP_STEEL, s));
                }
            }
            if (ly >= 7 || ly < 3) {
                /* side-on link threading through */
                if (ly >= 7) {
                    PX(cx[c] - 1, y, RP(RAMP_STEEL, 4));
                    PX(cx[c], y, RP(RAMP_STEEL, 3));
                }
            }
            if ((art_hash((uint32_t)c, (uint32_t)y, 0xC4A3u) % 7u) == 0 && GX(cx[c] + 2, y) >= PAL_RAMP(RAMP_STEEL, 0))
                PX(cx[c] + 2, y, PP(PAINT_RUST, 1));
        }
    }
}

/* ---- vape cloud wall ------------------------------------------------------------------- */
static void t_vape_cloud_wall(ArtImg *im, int f, int n)
{
    int i, k, ph = tx_phase(f, n);
    int px[24], py[24], pr[24], ord[24];
    ArtMat m = art_mat5(RP(RAMP_VAPE, 1), RP(RAMP_VAPE, 2), RP(RAMP_VAPE, 3), RP(RAMP_VAPE, 4), RP(RAMP_VAPE, 5));
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int sw = art_sin(x * ART_TURN / 32 + y * ART_TURN / 64 + art_noise(x, y, 16, 64, 0x7A9Eu) * 16 - ph);
            PX(x, y, RP(RAMP_VAPE, sw > 3000 ? 1 : 0));
        }
    k = tx_scatter(20, 64, 64, 0x7A9Fu, px, py);
    for (i = 0; i < k; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0x7AA0u);
        int a = ph + (int)(h & 4095u), r = 2 + (int)((h >> 12) % 3u);
        px[i] += r * art_cos(a) / Q12;
        py[i] += r * art_sin(a) / Q12;
        pr[i] = 6 + (int)((h >> 16) % 5u);
        ord[i] = i;
    }
    for (i = 1; i < k; i++) {
        int j = i, t = ord[i];
        while (j > 0 && py[ord[j - 1]] > py[t]) { ord[j] = ord[j - 1]; j--; }
        ord[j] = t;
    }
    for (i = 0; i < k; i++) {
        int q = ord[i];
        img_ball(im, px[q], py[q], pr[q], pr[q] * 8 / 10, &m, 0);
        /* magenta glow on the lower rim of some puffs */
        if (q % 3 == 0)
            for (x = -pr[q] / 2; x <= pr[q] / 2; x++) PX(px[q] + x, py[q] + pr[q] * 8 / 10, PP(PAINT_MAGENTA, 0));
    }
    /* two sly faces drift in the vapour (glowing slit eyes + grin) */
    for (i = 0; i < 2; i++) {
        int fx = 16 + i * 32 + (art_cos(ph + i * 2048) * 2) / Q12, fy = 20 + i * 26 + (art_sin(ph + i * 2048) * 2) / Q12;
        uint8_t eye = (f + i) % 4 == 3 ? RP(RAMP_VAPE, 3) : PP(PAINT_MAGENTA, 2);
        PX(fx - 4, fy, eye); PX(fx - 3, fy, eye); PX(fx - 3, fy + 1, PP(PAINT_MAGENTA, 0));
        PX(fx + 3, fy, eye); PX(fx + 4, fy, eye); PX(fx + 3, fy + 1, PP(PAINT_MAGENTA, 0));
        for (x = -3; x <= 3; x++) PX(fx + x, fy + 4 + (art_abs(x) == 3 ? -1 : 0), RP(RAMP_VAPE, 0));
        PX(fx - 1, fy + 5, RP(RAMP_BONE, 6)); PX(fx + 1, fy + 5, RP(RAMP_BONE, 6));
    }
    /* glitter */
    for (i = 0; i < 6; i++) {
        uint32_t h = art_hash((uint32_t)i, (uint32_t)f, 0x7AA1u);
        int sx = (int)(h % 64u), sy = (int)((h >> 8) % 64u);
        PX(sx, sy, (i & 1) ? PP(PAINT_MAGENTA, 2) : PP(PAINT_CYAN, 2));
    }
}

/* ---- rock ------------------------------------------------------------------------- */
static void t_hell_rock(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            ArtCell c;
            int edge, dx, dy, s, glow, grain;
            art_worley_j(x, y, 8, 64, 0x4E11u, 14, &c);
            edge = c.d2 - c.d1;
            dx = x - c.fx; dy = y - c.fy;
            grain = art_noise(x, y, 4, 64, 0x4E12u);
            /* each knobbly rock lit from the top-left */
            s = 2;
            if (dx + dy < -2) s = 3;
            if (dx + dy < -5 && c.d1 < 4 * 16) s = 4;
            if (dx + dy > 3) s = 1;
            if (grain > 200) s--;
            glow = art_fbm(x, y, 32, 2, 64, 0x4E13u) > 148;
            if (edge < 14) {
                PX(x, y, glow ? RP(RAMP_LAVA, edge < 6 ? 6 : 4) : RP(RAMP_BLOOD, 0));
                continue;
            }
            if (edge < 24) {
                if (glow) { PX(x, y, RP(RAMP_LAVA, 2)); continue; }
                s--;
            }
            PX(x, y, (c.id >> 20) % 4u == 0 ? RP(RAMP_BLOOD, s + 1) : RP(RAMP_WOOD, s));
        }
}

/* ---- gore floor ---------------------------------------------------------------------- */
static void gut(ArtImg *im, int x0, int y0, int len, uint32_t seed)
{
    ArtMat m = art_mat5(RP(RAMP_FLESH, 3), RP(RAMP_FLESH, 4), RP(RAMP_FLESH, 5), RP(RAMP_FLESH, 6),
                        RP(RAMP_FLESH, 7));
    int i, a = (int)(seed & 4095u), x = x0 * 16, y = y0 * 16;
    for (i = 0; i < len; i++) {
        a += art_sin(i * 600 + (int)seed) / 6;
        x += art_cos(a) * 2 * 16 / Q12;
        y += art_sin(a) * 2 * 16 / Q12;
        img_ball(im, x / 16, y / 16, 2, 2, &m, 0);
    }
}

static void t_gore_floor(ArtImg *im, int f, int n)
{
    int x, y, i;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            ArtCell c;
            int v = art_fbm(x, y, 16, 3, 64, 0x60E1u);
            int pool = art_fbm(x, y, 32, 2, 64, 0x60E2u);
            int s;
            /* meaty lumps */
            art_worley_j(x, y, 6, 64, 0x60E6u, 14, &c);
            s = (x - c.fx + y - c.fy < -1) ? 3 : 2;
            if (c.d2 - c.d1 < 18) s = 1;
            if (v > 160) s++;
            PX(x, y, RP(RAMP_FLESH, s));
            if (pool > 146) {
                int ps = pool > 172 ? 2 : 3;
                if (pool < 152) ps = 4;
                PX(x, y, RP(RAMP_BLOOD, ps));
            }
        }
    /* gloss streaks on the pools */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            uint8_t c = GX(x, y), up = GX(x, y - 1);
            if (c == RP(RAMP_BLOOD, 2) && up == RP(RAMP_BLOOD, 3) && (art_hash((uint32_t)x, (uint32_t)y, 0x60E3u) % 3u) == 0)
                PX(x, y, RP(RAMP_BLOOD, 6));
        }
    gut(im, 12, 40, 9, 0x111u);
    gut(im, 44, 14, 7, 0x777u);
    for (i = 0; i < 5; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0x60E4u);
        int bx = (int)(h % 64u), by = (int)((h >> 8) % 64u);
        /* bone shard with knobs */
        PX(bx, by, RP(RAMP_BONE, 7)); PX(bx + 1, by, RP(RAMP_BONE, 6)); PX(bx + 2, by + 1, RP(RAMP_BONE, 5));
        PX(bx + 3, by + 1, RP(RAMP_BONE, 6)); PX(bx + 4, by + 2, RP(RAMP_BONE, 7));
        PX(bx + 1, by + 1, RP(RAMP_BONE, 3)); PX(bx + 3, by + 2, RP(RAMP_BONE, 3));
        PX(bx + 2, by + 2, RP(RAMP_BLOOD, 1)); PX(bx + 4, by + 3, RP(RAMP_BLOOD, 1));
    }
    for (i = 0; i < 7; i++) {
        uint32_t h = art_hash((uint32_t)i, 1u, 0x60E5u);
        int tx = (int)(h % 64u), ty = (int)((h >> 8) % 64u);
        PX(tx, ty, RP(RAMP_BONE, 7));               /* teeth */
        PX(tx, ty + 1, RP(RAMP_BONE, 4));
    }
}

static void t_metal_floor(ArtImg *im, int f, int n)
{
    int x, y, i;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int lx = x % 32, ly = y % 32;
            int gx = x / 8, gy = y / 8, ux = x % 8, uy = y % 8;
            int dir = (gx + gy) & 1;
            int s = 4;
            /* raised lozenge along a diagonal in each 8x8 cell */
            int a = dir ? (ux + uy) : (ux - uy + 7);
            if (ux >= 1 && ux <= 6 && uy >= 1 && uy <= 6) {
                if (a == 7) s = 6;
                else if (a == 8) s = 2;
                else if (a == 6 && ux > 1 && ux < 6) s = 5;
            }
            if (art_noise(x, y, 8, 64, 0x3E7Au) > 200) s--;
            if (lx == 0 || ly == 0) s = 1;
            else if (lx == 1 || ly == 1) s = 6;
            PX(x, y, RP(RAMP_STEEL, s));
        }
    for (i = 0; i < 4; i++) {
        rivet(im, (i & 1) * 32 + 3, (i >> 1) * 32 + 3);
        rivet(im, (i & 1) * 32 + 28, (i >> 1) * 32 + 28);
    }
}

static void t_grate(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int lx = x % 8, ly = y % 8, bar = 0, s;
            int glow = art_fbm(x, y, 16, 2, 64, 0x62A7u);
            if (ly < 2) bar = 1;
            if (lx < 2) bar = 2;
            if (!bar) {
                /* hot pit below the grate */
                s = glow > 170 ? 3 : (glow > 130 ? 2 : (glow > 100 ? 1 : 0));
                if (ly == 2 || lx == 2) s = s > 1 ? s - 2 : 0;          /* bars' shadow */
                PX(x, y, s ? RP(RAMP_LAVA, s) : RP(RAMP_STONE, 0));
                continue;
            }
            if (bar == 1) s = ly == 0 ? 6 : 3;
            else s = lx == 0 ? 5 : 3;
            if (bar == 1 && lx < 2) s = 4;
            if ((art_hash((uint32_t)x, (uint32_t)y, 0x62A8u) % 11u) == 0) { PX(x, y, PP(PAINT_RUST, 1)); continue; }
            PX(x, y, RP(RAMP_STEEL, s));
        }
    /* heavier frame beams every 32 px */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            if (x % 32 < 3 || y % 32 < 3) {
                int s = (x % 32 == 0 || y % 32 == 0) ? 6 : ((x % 32 == 2 || y % 32 == 2) ? 2 : 4);
                PX(x, y, RP(RAMP_STEEL, s));
            }
}

/* distance in 1/16 px from p to segment ab */
static int seg_dist(int px, int py, int ax, int ay, int bx, int by)
{
    int64_t dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, t;
    int qx, qy;
    t = l2 ? ((int64_t)(px - ax) * dx + (int64_t)(py - ay) * dy) * 4096 / l2 : 0;
    if (t < 0) t = 0;
    if (t > 4096) t = 4096;
    qx = ax + (int)(dx * t / 4096);
    qy = ay + (int)(dy * t / 4096);
    return art_dist(px - qx, py - qy);
}

static void t_pentagram_floor(ArtImg *im, int f, int n)
{
    int x, y, k, ptx[5], pty[5];
    (void)f; (void)n;
    for (k = 0; k < 5; k++) {
        int a = -1024 + k * ART_TURN / 5;
        ptx[k] = 32 * 16 + 25 * art_cos(a) * 16 / Q12;
        pty[k] = 32 * 16 + 25 * art_sin(a) * 16 / Q12;
    }
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int lx = x % 16, ly = y % 16, s, d, dc, rr;
            int X = x * 16 + 8, Y = y * 16 + 8;
            uint8_t c;
            s = 2 + tx_bevel(lx, ly, 16, 16, 1);
            if (art_noise(x, y, 4, 64, 0x9E7Au) > 190) s--;
            c = (lx == 0 || ly == 0) ? RP(RAMP_STONE, 0) : RP(RAMP_STONE, s);
            /* distance to the star and the circle */
            d = 1 << 30;
            for (k = 0; k < 5; k++) {
                int e = seg_dist(X, Y, ptx[k], pty[k], ptx[(k + 2) % 5], pty[(k + 2) % 5]);
                if (e < d) d = e;
            }
            rr = art_dist(X - 32 * 16, Y - 32 * 16);
            dc = art_abs(rr - 27 * 16);
            if (dc < d) d = dc;
            if (art_abs(rr - 30 * 16) < 10) d = 0;               /* outer thin ring */
            if (d < 10) c = RP(RAMP_LAVA, 6);
            else if (d < 18) c = RP(RAMP_LAVA, 4);
            else if (d < 30) c = RP(RAMP_BLOOD, 5);
            else if (d < 44) c = (c >= RP(RAMP_STONE, 2)) ? RP(RAMP_BLOOD, 2) : RP(RAMP_BLOOD, 1);
            PX(x, y, c);
        }
    /* runes between the rings */
    for (k = 0; k < 10; k++) {
        int a = k * ART_TURN / 10 + 200;
        int rx = 32 + 28 * art_cos(a) / Q12 + (art_cos(a) > 0 ? 0 : -1);
        int ry = 32 + 28 * art_sin(a) / Q12;
        PX(rx, ry, RP(RAMP_LAVA, 5));
        PX(rx + ((k & 1) ? 1 : -1), ry - 1, RP(RAMP_BLOOD, 6));
    }
    PX(32, 32, RP(RAMP_LAVA, 7));
}

static void t_infected_grass(ArtImg *im, int f, int n)
{
    int x, y, i, k, xs[64], ys[64];
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int v = art_fbm(x, y, 32, 3, 64, 0x1F6Au);
            int dead = art_fbm(x, y, 16, 2, 64, 0x1F6Bu);
            ArtCell c;
            uint8_t col = RP(RAMP_SICK, v > 136 ? 4 : 3);
            if (dead > 165) col = RP(RAMP_WOOD, dead > 180 ? 2 : 3);
            art_worley_j(x, y, 3, 64, 0x1F6Cu, 14, &c);
            if (c.d2 - c.d1 < 14) col = RP(RAMP_VAPE, 5);
            else if (c.d2 - c.d1 < 26) col = RP(RAMP_VAPE, 2);
            PX(x, y, col);
        }
    k = tx_scatter(34, 64, 64, 0x1F6Du, xs, ys);
    for (i = 0; i < k; i++) {
        uint8_t b = GX(xs[i], ys[i]);
        if (b < PAL_RAMP(RAMP_SICK, 0)) continue;
        PX(xs[i], ys[i], art_darker(b, 1));
        PX(xs[i] - 1, ys[i] - 1, art_darker(b, 1));
        PX(xs[i] + 1, ys[i] - 1, art_darker(b, 1));
        PX(xs[i] + 1, ys[i] - 2, art_lighter(b, 1));
    }
    /* pustules and purple shrooms */
    for (i = 0; i < 6; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0x1F6Eu);
        int bx = (int)(h % 64u), by = (int)((h >> 8) % 64u);
        if (i & 1) {
            ArtMat m = art_mat4(PP(PAINT_SLIME, 0), PP(PAINT_SLIME, 1), PP(PAINT_SLIME, 2), RP(RAMP_SICK, 7));
            img_ball(im, bx, by, 2, 2, &m, 0);
            PX(bx - 1, by - 1, PAL_WHITE);
        } else {
            PX(bx, by + 1, RP(RAMP_BONE, 6)); PX(bx, by + 2, RP(RAMP_BONE, 4));
            PX(bx - 1, by, PP(PAINT_VAPE_PURPLE, 1)); PX(bx, by, PP(PAINT_VAPE_PURPLE, 2));
            PX(bx + 1, by, PP(PAINT_VAPE_PURPLE, 0)); PX(bx, by - 1, PP(PAINT_VAPE_PURPLE, 1));
        }
    }
}

static void t_slime(ArtImg *im, int f, int n)
{
    int x, y, i, ph = tx_phase(f, n);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int v = art_fbm(x, y, 32, 2, 64, 0x511Eu);
            int sw = art_sin(x * ART_TURN / 64 * 2 + art_sin(y * ART_TURN / 32 - ph) / 3 + v * 16 + ph);
            int sw2 = art_sin(y * ART_TURN / 64 * 3 - art_sin(x * ART_TURN / 64 + ph) / 4 - ph);
            uint8_t c = RP(RAMP_SICK, 4);
            if (v < 110) c = RP(RAMP_SICK, 3);
            if (sw > 3300) c = PP(PAINT_SLIME, 1);
            if (sw > 3950 && sw2 > 0) c = PP(PAINT_SLIME, 2);
            if (sw < -3600) c = RP(RAMP_SICK, 2);
            PX(x, y, c);
        }
    for (i = 0; i < 8; i++) {
        uint32_t h = art_hash((uint32_t)i, 0u, 0x511Fu);
        int bx = (int)(h % 64u), by = (int)((h >> 8) % 64u);
        int st = art_wrap(f + (int)((h >> 16) % 4u), 4), big = (int)((h >> 20) & 1u);
        if (st == 0) { PX(bx, by, RP(RAMP_SICK, 6)); continue; }
        if (st == 1) {
            PX(bx, by, PP(PAINT_SLIME, 2)); PX(bx + 1, by, RP(RAMP_SICK, 6));
            PX(bx, by + 1, RP(RAMP_SICK, 5)); PX(bx + 1, by + 1, RP(RAMP_SICK, 3));
        } else if (st == 2) {
            img_ring(im, bx, by, 1 + big, 2 + big, RP(RAMP_SICK, 6));
            PX(bx - 1, by - 1 - big, PAL_WHITE);
            PX(bx + 1 + big, by + 1, RP(RAMP_SICK, 3));
        } else {
            PX(bx - 3, by, RP(RAMP_SICK, 7)); PX(bx + 3, by - 1, RP(RAMP_SICK, 7));
            PX(bx, by - 3, RP(RAMP_SICK, 6)); PX(bx - 2, by + 2, RP(RAMP_SICK, 6));
            PX(bx + 2, by + 2, RP(RAMP_SICK, 6));
        }
    }
}

/* ---- doors & signs ----------------------------------------------------------------- */
static void t_door_cute(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s;
            uint8_t c;
            if (x < 4 || x > 59 || y < 3) {
                /* wooden frame */
                s = 4 + ((x < 2 || y < 1) ? 1 : 0) - (x > 61 ? 1 : 0);
                c = RP(RAMP_WOOD, s);
            } else {
                int lx = (x - 4) % 14;
                s = 5;
                if (lx == 0) s = 3;
                else if (lx == 1) s = 6;
                else if (lx == 13) s = 4;
                c = RP(RAMP_PASTEL_PINK, s);
            }
            PX(x, y, c);
        }
    /* inset panels */
    {
        int p;
        for (p = 0; p < 2; p++) {
            int x0 = 10 + p * 24, y0 = 34, w = 20, h = 22;
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    int e = tx_bevel(x, y, w, h, 1);
                    if (x > 0 && y > 0 && x < w - 1 && y < h - 1 && (x == 1 || y == 1)) e = -1;
                    if (e) PX(x0 + x, y0 + y, RP(RAMP_PASTEL_PINK, 5 + e));
                }
        }
    }
    /* heart window: the 9x8 heart bitmap at 2x with a wooden frame */
    {
        ArtImg glass;
        uint8_t buf[12 * 12];
        int gx, gy;
        img_init(&glass, buf, 12, 12, 0, 0);
        img_heart_px(&glass, 6, 6, 2, 1, 1, 0);
        for (gy = -1; gy < 11; gy++)
            for (gx = -1; gx < 12; gx++) {
                int in = img_get(&glass, gx, gy) != 0, near = 0, a, b;
                for (b = -1; b <= 1; b++)
                    for (a = -1; a <= 1; a++) near |= img_get(&glass, gx + a, gy + b) != 0;
                for (b = 0; b < 2; b++)
                    for (a = 0; a < 2; a++) {
                        int X = 22 + gx * 2 + a, Y = 6 + gy * 2 + b;
                        if (in) {
                            int d = (gx * 2 + a) + (gy * 2 + b);
                            uint8_t c = RP(RAMP_SKY, gy < 3 ? 6 : 5);
                            if (d >= 7 && d <= 9) c = RP(RAMP_SKY, 7);
                            if (d == 12) c = RP(RAMP_SKY, 7);
                            PX(X, Y, c);
                        } else if (near) PX(X, Y, RP(RAMP_WOOD, (gx + gy < 8) ? 6 : 4));
                    }
            }
    }
    /* golden knob */
    {
        ArtMat gold = art_mat_ramp(RAMP_CANDY, 3, 7);
        img_ball(im, 50, 30, 3, 3, &gold, 0);
        PX(51, 34, RP(RAMP_PASTEL_PINK, 3)); PX(52, 33, RP(RAMP_PASTEL_PINK, 3));
    }
    /* hinges */
    for (y = 8; y < 60; y += 40) {
        img_rect(im, 4, y, 5, 3, RP(RAMP_CANDY, 4));
        img_hline(im, 4, y, 5, RP(RAMP_CANDY, 6));
        img_hline(im, 4, y + 2, 5, RP(RAMP_CANDY, 2));
    }
    /* little flower sprig under the window */
    tx_flower(im, 27, 30, PAL_WHITE, PP(PAINT_WHITE, 0), RP(RAMP_CANDY, 5));
    tx_flower(im, 37, 30, PP(PAINT_LEMON, 2), PP(PAINT_LEMON, 1), RP(RAMP_CANDY, 3));
    PX(32, 30, RP(RAMP_GRASS, 4)); PX(31, 31, RP(RAMP_GRASS, 3)); PX(33, 31, RP(RAMP_GRASS, 3));
}

static void t_door_hell(ArtImg *im, int f, int n)
{
    int x, y, i;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = 3, lx = x % 32;
            int rust = art_fbm(x, y, 16, 3, 64, 0xD00Eu);
            if (lx == 0) s = 0;
            else if (lx == 1) s = 5;
            else if (lx == 31) s = 1;
            else if (art_noise(x, y / 3, 4, 64, 0xD00Fu) > 170) s = 4;
            if (y < 3) s = y == 0 ? 6 : 4;
            if (y > 60) s = 1;
            PX(x, y, (rust > 175 && s >= 3) ? PP(PAINT_RUST, rust > 190 ? 0 : 1) : RP(RAMP_STEEL, s));
        }
    /* hazard stripes */
    for (y = 50; y < 59; y++)
        for (x = 2; x < 62; x++) {
            uint8_t c = art_wrap(x + y, 10) < 5 ? RP(RAMP_CANDY, 4) : RP(RAMP_STONE, 0);
            if (y == 50) c = RP(RAMP_STEEL, 5);
            if (y == 58) c = RP(RAMP_STEEL, 1);
            PX(x, y, c);
        }
    /* rivets */
    for (i = 0; i < 6; i++) {
        rivet(im, 4, 6 + i * 8);
        rivet(im, 58, 6 + i * 8);
    }
    /* horned skull emblem */
    {
        ArtMat horn = art_mat_ramp(RAMP_BONE, 2, 6);
        int k;
        for (k = 0; k < 7; k++) {
            img_ball(im, 23 - k, 17 - k * 3 / 2 + (k * k) / 6, 2 - k / 4, 2 - k / 4, &horn, 0);
            img_ball(im, 41 + k, 17 - k * 3 / 2 + (k * k) / 6, 2 - k / 4, 2 - k / 4, &horn, 0);
        }
        skull(im, 32, 24, 0);
        img_ellipse(im, 29, 25, 1, 1, RP(RAMP_BLOOD, 6));
        img_ellipse(im, 35, 25, 1, 1, RP(RAMP_BLOOD, 6));
        PX(29, 25, RP(RAMP_LAVA, 7)); PX(35, 25, RP(RAMP_LAVA, 7));
    }
    /* blood smear */
    for (y = 4; y < 46; y++) {
        int w = 2 + (art_noise(0, y, 4, 64, 0xD010u) > 128);
        for (x = 0; x < w; x++) PX(48 + x + y / 12, y, RP(RAMP_BLOOD, x == 0 ? 4 : 3));
    }
    drip(im, 50, 46, 4, 1);
}

static void glow_text(ArtImg *im, int x, int y, const char *t, int scale, uint8_t glow, uint8_t core)
{
    int dx, dy;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (dx || dy) img_text(im, x + dx, y + dy, t, scale, glow);
    img_text(im, x, y, t, scale, core);
}

static void t_exit_sign(ArtImg *im, int f, int n)
{
    int x, y;
    (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = 2 + tx_bevel(x % 32, y, 32, 64, 1);
            PX(x, y, RP(RAMP_STEEL, s));
        }
    /* sign box */
    for (y = 16; y < 46; y++)
        for (x = 6; x < 58; x++) {
            uint8_t c = RP(RAMP_BLOOD, 0);
            if (y == 16 || x == 6) c = RP(RAMP_STEEL, 6);
            else if (y == 45 || x == 57) c = RP(RAMP_STEEL, 1);
            else if (y == 17 || x == 7) c = RP(RAMP_STEEL, 4);
            PX(x, y, c);
        }
    if (f == 0) glow_text(im, 9, 20, "EXIT", 2, RP(RAMP_BLOOD, 3), PP(PAINT_CHERRY, 2));
    else glow_text(im, 9, 20, "EXIT", 2, RP(RAMP_BLOOD, 1), RP(RAMP_BLOOD, 4));
    /* inner glare on the letters */
    if (f == 0)
        for (x = 9; x < 56; x++)
            if (GX(x, 20) == PP(PAINT_CHERRY, 2)) PX(x, 20, PP(PAINT_ROSE, 2));
    /* arrow */
    for (x = 0; x < 9; x++) PX(28 + x, 39, f == 0 ? PP(PAINT_CHERRY, 1) : RP(RAMP_BLOOD, 3));
    PX(35, 38, f == 0 ? PP(PAINT_CHERRY, 1) : RP(RAMP_BLOOD, 3));
    PX(35, 40, f == 0 ? PP(PAINT_CHERRY, 1) : RP(RAMP_BLOOD, 3));
    PX(34, 37, f == 0 ? PP(PAINT_CHERRY, 0) : RP(RAMP_BLOOD, 2));
    PX(34, 41, f == 0 ? PP(PAINT_CHERRY, 0) : RP(RAMP_BLOOD, 2));
    /* light spill on the wall under the sign */
    if (f == 0)
        for (x = 10; x < 54; x++) { img_shift(im, x, 46, 1); if (x & 1) img_shift(im, x, 47, 1); }
}

static void t_switch(ArtImg *im, int f, int n)
{
    int x, y;
    (void)f; (void)n;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int s = 3 + tx_bevel(x, y, 64, 64, 2);
            if (art_noise(x, y / 3, 4, 64, 0x5017u) > 190) s++;
            PX(x, y, RP(RAMP_STEEL, s));
        }
    /* hazard frame around the housing */
    for (y = 12; y < 52; y++)
        for (x = 16; x < 48; x++)
            PX(x, y, art_wrap(x + y, 8) < 4 ? RP(RAMP_CANDY, 4) : RP(RAMP_STONE, 0));
    for (y = 15; y < 49; y++)
        for (x = 19; x < 45; x++) {
            uint8_t c = RP(RAMP_STEEL, 1);
            if (y == 15 || x == 19) c = RP(RAMP_STEEL, 0);
            else if (y == 48 || x == 44) c = RP(RAMP_STEEL, 5);
            PX(x, y, c);
        }
    /* big red button in a gold ring */
    {
        ArtMat ring = art_mat_ramp(RAMP_CANDY, 2, 6);
        ArtMat btn = art_mat5(RP(RAMP_BLOOD, 3), RP(RAMP_BLOOD, 4), PP(PAINT_CHERRY, 1), PP(PAINT_CHERRY, 2),
                              PP(PAINT_ROSE, 2));
        img_ball(im, 32, 34, 9, 9, &ring, 0);
        img_ball(im, 32, 33, 7, 7, &btn, 0);
        PX(29, 29, PAL_WHITE); PX(30, 29, PAL_WHITE); PX(29, 30, PAL_WHITE);
    }
    /* status lamp + label */
    light_dot(im, 31, 18, PAINT_LIME, 1);
    img_text(im, 21, 54, "PUSH", 1, RP(RAMP_STEEL, 1));
    img_text(im, 20, 53, "PUSH", 1, RP(RAMP_STEEL, 6));
    rivet(im, 5, 5); rivet(im, 56, 5); rivet(im, 5, 56); rivet(im, 56, 56);
}

const ArtTexDef art_tex_hell[] = {
    {TEX_FLESH_WALL, "flesh_wall", 1, 1, DOOMO_ANIM_LOOP, t_flesh_wall},
    {TEX_BONE_WALL, "bone_wall", 1, 1, DOOMO_ANIM_LOOP, t_bone_wall},
    {TEX_BLOOD_BRICK, "blood_brick", 1, 1, DOOMO_ANIM_LOOP, t_blood_brick},
    {TEX_LAVA, "lava", 4, 7, DOOMO_ANIM_LOOP, t_lava},
    {TEX_RUST_METAL, "rust_metal", 1, 1, DOOMO_ANIM_LOOP, t_rust_metal},
    {TEX_TECH_PANEL, "tech_panel", 1, 1, DOOMO_ANIM_LOOP, t_tech_panel},
    {TEX_SKULL_MOSAIC, "skull_mosaic", 1, 1, DOOMO_ANIM_LOOP, t_skull_mosaic},
    {TEX_DEMON_EYES, "demon_eyes", 2, 5, DOOMO_ANIM_FLASH, t_demon_eyes},
    {TEX_CHAINS, "chains", 1, 1, DOOMO_ANIM_LOOP, t_chains},
    {TEX_VAPE_CLOUD_WALL, "vape_cloud_wall", 4, 8, DOOMO_ANIM_LOOP, t_vape_cloud_wall},
    {TEX_HELL_ROCK, "hell_rock", 1, 1, DOOMO_ANIM_LOOP, t_hell_rock},
    {TEX_GORE_FLOOR, "gore_floor", 1, 1, DOOMO_ANIM_LOOP, t_gore_floor},
    {TEX_METAL_FLOOR, "metal_floor", 1, 1, DOOMO_ANIM_LOOP, t_metal_floor},
    {TEX_GRATE, "grate", 1, 1, DOOMO_ANIM_LOOP, t_grate},
    {TEX_PENTAGRAM_FLOOR, "pentagram_floor", 1, 1, DOOMO_ANIM_LOOP, t_pentagram_floor},
    {TEX_INFECTED_GRASS, "infected_grass", 1, 1, DOOMO_ANIM_LOOP, t_infected_grass},
    {TEX_SLIME, "slime", 4, 8, DOOMO_ANIM_LOOP, t_slime},
    {TEX_DOOR_CUTE, "door_cute", 1, 1, DOOMO_ANIM_LOOP, t_door_cute},
    {TEX_DOOR_HELL, "door_hell", 1, 1, DOOMO_ANIM_LOOP, t_door_hell},
    {TEX_EXIT_SIGN, "exit_sign", 1, 1, DOOMO_ANIM_LOOP, t_exit_sign},
    {TEX_SWITCH, "switch", 1, 1, DOOMO_ANIM_LOOP, t_switch},
};
const int art_tex_hell_n = (int)(sizeof art_tex_hell / sizeof art_tex_hell[0]);
