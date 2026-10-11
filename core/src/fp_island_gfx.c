/* fp_island_gfx.c - the illustrated free play island (owner: freeplay).
 *
 * HaHaland is a squircle island in a pastel sea. A river runs down its
 * middle with the Wall on the east bank: Hillvile (west) is a candy
 * meadow of pink apartment towers, cafes and flower beds; Hellvile (east)
 * is spooky-cute plum ground with horned towers, lava pools and a
 * graveyard with a friendly ghost. Hellvile Park sits in the middle with
 * a fountain on the bridge.
 *
 * Everything is drawn every frame from row spans (no stored bitmap: the
 * island must fit 240x160 and 320x240 and costs no RAM), about 8-10 spans
 * a row plus the buildings. Day / dusk / night re-colour every span
 * through the tint maps; lit windows and lamps are drawn untinted so they
 * glow at night. Nothing is ever still: waves, foam, a turning ferris
 * wheel, smoking chimneys, bubbling lava, a floating ghost, tiny citizens
 * strolling the roads.
 */
#include "fp_island.h"

/* ====================================================================== */
/* geometry                                                                 */
static FpIslandGeo *G;                       /* in the island's scene scratch */
#define M       (G->map)
#define coast_l (G->coast_l)
#define coast_r (G->coast_r)
#define river_c (G->river_c)
static int g_tint, g_t;

static void build_geometry(void);
static void use_geo(void)
{
    G = fp_island_geo();
    if (G->geo_w != fp_sw() || G->geo_h != fp_sh() || M.h == 0) build_geometry();
}

const FpMap *fp_map(void) { use_geo(); return &M; }

int fp_mx(int u) { use_geo(); return M.x + u * M.w / 256; }
int fp_my(int u) { use_geo(); return M.y + u * M.h / 256; }

static int isqrt_i(int v) { return v <= 0 ? 0 : (int)fx_isqrt((uint32_t)v); }

static void build_geometry(void)
{
    int row, sw = fp_sw(), sh = fp_sh();
    M.x = 0;
    M.y = 16;
    M.w = sw;
    M.h = sh - 16 - 14;
    if (M.h > FP_GEO_ROWS - 2) M.h = FP_GEO_ROWS - 2;
    G->geo_w = (int16_t)sw;
    G->geo_h = (int16_t)sh;
    for (row = 0; row < M.h; row++) {
        /* superellipse |x/a|^4 + |y/b|^4 <= 1 with a little coastal wobble */
        int cy = M.h / 2, b = M.h / 2 - 3, a = M.w / 2 - 5;
        int dy = row - cy, v, hw, wob_l, wob_r;
        int q = dy * 256 / b;                       /* -256..256 */
        q = q < 0 ? -q : q;
        if (q > 256) { coast_l[row] = coast_r[row] = -1; river_c[row] = -1; continue; }
        v = (q * q / 256) * (q * q / 256) / 256;    /* (y/b)^4 in 1/256 */
        hw = a * isqrt_i(isqrt_i((256 - v) * 256) * 256) / 256;
        wob_l = ui_wave(row * 2 + 11, 140, 5) + ui_wave(row * 7, 97, 2) + ui_wave(row * 3, 41, 1);
        wob_r = ui_wave(row * 2 + 70, 150, 5) + ui_wave(row * 5 + 40, 83, 2) + ui_wave(row * 9 + 5, 37, 1);
        coast_l[row] = (int16_t)(M.x + M.w / 2 - hw + wob_l);
        coast_r[row] = (int16_t)(M.x + M.w / 2 + hw + wob_r);
        if (coast_r[row] - coast_l[row] < 4) { coast_l[row] = coast_r[row] = -1; }
        river_c[row] = (int16_t)(M.x + M.w / 2 + ui_wave(row * 3 + 20, 140, 4) +
                                 ui_wave(row, 53, 1));
    }
}

void fp_island_geometry(void) { use_geo(); }

/* Is the screen point on land, and on which side (0 hill, 1 hell)? */
int fp_island_side_at(int x, int y)
{
    int row;
    use_geo();
    row = y - M.y;
    if (row < 0 || row >= M.h || coast_l[row] < 0) return -1;
    if (x < coast_l[row] || x > coast_r[row]) return -1;
    return x > river_c[row] + 5 ? 1 : 0;
}

/* ====================================================================== */
/* colour + primitives                                                      */
static uint8_t tc(uint8_t c) { return g_tint ? doomo_tintmap[g_tint][c] : c; }
static void R(int x, int y, int w, int h, uint8_t c) { if (w > 0 && h > 0) plat_rect(x, y, w, h, tc(c)); }
static void RG(int x, int y, int w, int h, uint8_t c) { if (w > 0 && h > 0) plat_rect(x, y, w, h, c); }
static void C(int cx, int cy, int r, uint8_t c) { ui_fill_circle(cx, cy, r, tc(c)); }
static void E(int cx, int cy, int rx, int ry, uint8_t c) { ui_fill_ellipse(cx, cy, rx, ry, tc(c)); }

/* a shaded box with a coloured outline (dark shade of its own colour) */
static void box(int x, int y, int w, int h, uint8_t dark, uint8_t base, uint8_t light)
{
    R(x, y, w, h, ui_darker(dark));
    R(x + 1, y + 1, w - 2, h - 2, base);
    R(x + 1, y + 1, w - 2, 1, light);
    R(x + 1, y + 2, 1, h - 3, light);
    R(x + w - 3, y + 2, 2, h - 3, dark);
}

/* triangular roof: apex at (cx, y), h rows tall, w wide at the base */
static void roof(int cx, int y, int w, int h, uint8_t dark, uint8_t base, uint8_t light)
{
    int i;
    for (i = 0; i < h; i++) {
        int rw = 1 + (w - 1) * (i + 1) / h, x0 = cx - rw / 2;
        R(x0 - 1, y + i, rw + 2, 1, ui_darker(dark));
        R(x0, y + i, rw, 1, base);
        if (rw > 4) {
            R(x0 + 1, y + i, rw / 3, 1, light);
            R(x0 + rw - rw / 4, y + i, rw / 4, 1, dark);
        }
    }
}

/* window: lit ones glow untinted at night */
static void win(int x, int y, int w, int h, int lit, uint8_t frame)
{
    R(x - 1, y - 1, w + 2, h + 2, frame);
    if (lit) {
        RG(x, y, w, h, PAL_RAMP(RAMP_CANDY, 6));
        RG(x, y + h - 1, w, 1, PAL_RAMP(RAMP_CANDY, 4));
    } else {
        R(x, y, w, h, PAL_RAMP(RAMP_SKY, 6));
        R(x, y, 1, 1, PAL_WHITE);
        R(x + w - 1, y + h - 1, 1, 1, PAL_RAMP(RAMP_SKY, 4));
    }
}

static void shadow(int cx, int y, int rx)
{
    int i;
    for (i = 0; i < 3; i++) {
        int w = rx * 2 - (i == 1 ? 0 : 4) - (i == 2 ? 4 : 0);
        plat_shade_rect(cx - w / 2, y - 1 + i, w, 1, i == 1 ? 10 : 12);
    }
}

/* golden hour (dawn / dusk): the land keeps its candy colours, the sea
 * blushes peach and pink */
static int golden_hour(void)
{
    int h = town_hour();
    return !town_is_night() && (h >= 18 || h < 7);
}

static int night_lit(int salt)
{
    if (!g_tint) return 0;
    return (doomo_hash32((uint32_t)salt * 2654435761u + (uint32_t)(doomo_town.day)) & 3) != 0;
}

/* ====================================================================== */
/* terrain                                                                  */
#define SAND_HI   PAL_RAMP(RAMP_CANDY, 6)
#define SAND      PAL_RAMP(RAMP_CANDY, 5)
#define SAND_D    PAL_RAMP(RAMP_PEACH, 5)
#define ASH_HI    PAL_RAMP(RAMP_STONE, 4)
#define ASH       PAL_RAMP(RAMP_STONE, 3)
#define CLIFF     PAL_RAMP(RAMP_PEACH, 3)
#define CLIFF_H   PAL_RAMP(RAMP_VAPE, 1)

static uint8_t hill_grass(int row)
{
    int k = row * 6 / DOOMO_MAX(M.h, 1);
    static const uint8_t g[6] = {
        PAL_RAMP(RAMP_GRASS, 7), PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 6),
        PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 5)
    };
    return g[DOOMO_CLAMP(k, 0, 5)];
}

static uint8_t hell_ground(int row)
{
    int k = row * 6 / DOOMO_MAX(M.h, 1);
    static const uint8_t g[6] = {
        PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 4),
        PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 3)
    };
    return g[DOOMO_CLAMP(k, 0, 5)];
}

static void draw_sea(void)
{
    int i, t = g_t, night = g_tint == TINT_NIGHT;
    int bands = 4, bh = (M.h + bands - 1) / bands;
    static const uint8_t day[4] = { PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_MINT, 6),
                                    PAL_RAMP(RAMP_MINT, 5), PAL_RAMP(RAMP_SKY, 5) };
    static const uint8_t dusk[4] = { PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PASTEL_PINK, 6),
                                     PAL_RAMP(RAMP_PASTEL_PINK, 5), PAL_RAMP(RAMP_VAPE, 5) };
    const uint8_t *band = golden_hour() ? dusk : day;
    for (i = 0; i < bands; i++) {
        int y = M.y + i * bh;
        R(M.x, y, M.w, DOOMO_MIN(bh, M.y + M.h - y), band[i]);
        if (i) plat_checker(M.x, y, M.w, 1, tc(band[i - 1]));
    }
    /* wave glints drifting along */
    for (i = 0; i < 18; i++) {
        uint32_t h = doomo_hash32((uint32_t)i * 977u + 5u);
        int x = M.x + (int)((h & 0xFFF) % (uint32_t)M.w);
        int y = M.y + 3 + (int)((h >> 12) % (uint32_t)DOOMO_MAX(M.h - 6, 1));
        int ph = (t / 3 + (int)(h >> 20)) % 64;
        x = M.x + (x - M.x + t / 8) % M.w;
        if (fp_island_side_at(x, y) >= 0 || fp_island_side_at(x + 6, y) >= 0) continue;
        if (ph < 40) {
            int w = ph < 20 ? 2 + ph / 5 : 6 - (ph - 20) / 5;
            RG(x, y, w, 1, night ? PAL_RAMP(RAMP_SKY, 4) : golden_hour() ? PAL_RAMP(RAMP_CANDY, 7) : PAL_WHITE);
            if (w > 3) R(x + 1, y + 1, w - 2, 1, golden_hour() ? PAL_RAMP(RAMP_LAVA, 7) : PAL_RAMP(RAMP_SKY, 7));
        }
    }
    if (night) {
        for (i = 0; i < 14; i++) {
            uint32_t h = doomo_hash32((uint32_t)i * 7919u + 3u);
            int x = M.x + (int)(h % (uint32_t)M.w), y = M.y + (int)((h >> 10) % (uint32_t)M.h);
            if (fp_island_side_at(x, y) >= 0) continue;
            ui_sparkle(x, y, (t / 4 + i * 5) & 15, PAL_RAMP(RAMP_CANDY, 7));
        }
    }
}

static void draw_land(void)
{
    int row, t = g_t;
    /* the cliff under the south rim: the land drawn 3 rows lower first */
    for (row = 0; row < M.h; row++) {
        int src = row - 3, l, r;
        if (src < 0 || coast_l[src] < 0) continue;
        l = coast_l[src];
        r = coast_r[src];
        if (row < M.h && coast_l[row] >= 0 && coast_l[row] <= l && coast_r[row] >= r) continue;
        R(l, M.y + row, river_c[src] - l, 1, CLIFF);
        R(river_c[src], M.y + row, r - river_c[src], 1, CLIFF_H);
    }
    for (row = 0; row < M.h; row++) {
        int y = M.y + row, l = coast_l[row], r = coast_r[row], rc = river_c[row];
        int foam = ((row + t / 6) % 9) < 2;
        int rl, rr, wl;
        if (l < 0) continue;
        rl = rc - 5;               /* river left bank */
        rr = rc + 4;               /* river right edge */
        wl = rr + 1;               /* the wall starts (5 px + shadow) */
        /* west sand + meadow */
        RG(l - 1, y, 1, 1, tc(foam ? PAL_WHITE : PAL_RAMP(RAMP_SKY, 7)));
        R(l, y, 3, 1, row < M.h / 2 ? SAND_HI : SAND);
        R(l + 3, y, rl - l - 3, 1, hill_grass(row));
        /* river (with a bright shimmer line) */
        R(rl, y, 1, 1, SAND_D);
        R(rl + 1, y, rr - rl - 1, 1, PAL_RAMP(RAMP_SKY, 5));
        if (((row + t / 4) % 7) == 0) R(rl + 2 + (row & 1), y, 2, 1, PAL_RAMP(RAMP_SKY, 7));
        /* the Wall seen from above: outline, lit west face, brick courses
         * every 3 rows (offset mortar), shadow cast onto Hellvile */
        R(wl, y, 1, 1, PAL_RAMP(RAMP_STONE, 1));
        R(wl + 1, y, 1, 1, PAL_RAMP(RAMP_STONE, 6));
        R(wl + 2, y, 3, 1, (row % 3) == 0 ? PAL_RAMP(RAMP_STONE, 3) : PAL_RAMP(RAMP_STONE, 5));
        R(wl + 2 + ((row / 3) & 1) * 2, y, 1, 1, PAL_RAMP(RAMP_STONE, 3));
        R(wl + 5, y, 1, 1, PAL_RAMP(RAMP_STONE, 1));
        R(wl + 6, y, 2, 1, ui_darker(hell_ground(row)));
        /* east: Hellvile plum ground + ash shore */
        R(wl + 8, y, r - 3 - (wl + 8), 1, hell_ground(row));
        R(r - 3, y, 3, 1, ASH);
        RG(r, y, 1, 1, tc(foam ? PAL_RAMP(RAMP_SKY, 7) : PAL_WHITE));
    }
    /* soft seams between the meadow bands */
    for (row = 1; row < 6; row++) {
        int rr0 = row * M.h / 6, l, r, rc;
        if (rr0 >= M.h || coast_l[rr0] < 0) continue;
        l = coast_l[rr0];
        r = coast_r[rr0];
        rc = river_c[rr0];
        plat_checker(l + 3, M.y + rr0, rc - 4 - l - 3, 1, tc(hill_grass(rr0 - 1)));
        plat_checker(rc + 13, M.y + rr0, r - 3 - rc - 13, 1, tc(hell_ground(rr0 - 1)));
    }
}

/* ---- ground patches: clover on the meadow, plum blotches and embers in
 * Hellvile (so neither side is ever a flat single-colour blob) --------- */
typedef struct { uint8_t x, y, rx, ry; } Patch;
static const Patch hill_patches[] = {
    { 40, 90, 14, 8 }, { 84, 140, 12, 7 }, { 20, 172, 10, 6 }, { 70, 40, 12, 6 },
    { 108, 104, 8, 10 }, { 54, 192, 12, 6 }, { 100, 222, 10, 5 }
};
static const Patch hell_patches[] = {
    { 176, 92, 16, 9 }, { 220, 100, 12, 8 }, { 196, 146, 18, 9 }, { 166, 184, 12, 7 },
    { 232, 168, 10, 10 }, { 200, 48, 14, 6 }, { 168, 226, 14, 5 }, { 234, 206, 8, 8 }
};

static void draw_patches(void)
{
    int i, t = g_t;
    for (i = 0; i < DOOMO_COUNTOF(hill_patches); i++) {
        const Patch *p = &hill_patches[i];
        int x = fp_mx(p->x), y = fp_my(p->y), rx = p->rx * M.w / 256 + 1, ry = p->ry * M.h / 256 + 1;
        E(x, y, rx, ry, PAL_RAMP(RAMP_GRASS, 7));
        E(x - rx / 4, y - ry / 4, rx / 2, ry / 2, PAL_RAMP(RAMP_MINT, 7));
        R(x - rx / 2, y, 1, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
        R(x + rx / 3, y - 1, 1, 1, PAL_WHITE);
    }
    for (i = 0; i < DOOMO_COUNTOF(hell_patches); i++) {
        const Patch *p = &hell_patches[i];
        int x = fp_mx(p->x), y = fp_my(p->y), rx = p->rx * M.w / 256 + 1, ry = p->ry * M.h / 256 + 1;
        int glow = ((t / 8) + i * 3) % 12;
        E(x, y, rx * 3 / 4, ry * 3 / 4, PAL_RAMP(RAMP_VAPE, 3));
        E(x + rx / 5, y + ry / 5, rx / 2, ry / 2, PAL_RAMP(RAMP_VAPE, 2));
        /* a little ember crack */
        R(x - rx / 2, y, rx / 3, 1, PAL_RAMP(RAMP_LAVA, 3));
        R(x - rx / 2 + rx / 3, y + 1, rx / 4, 1, PAL_RAMP(RAMP_LAVA, 3));
        if (glow < 6) RG(x - rx / 3, y, 2, 1, PAL_RAMP(RAMP_LAVA, 5 + glow / 3));
        /* purple grass tufts */
        R(x + rx - 1, y - ry, 1, 2, PAL_RAMP(RAMP_VAPE, 6));
        R(x + rx + 1, y - ry - 1, 1, 3, PAL_RAMP(RAMP_VAPE, 6));
    }
}

/* ---- roads --------------------------------------------------------------- */
typedef struct { uint8_t x0, y0, x1, y1; } Seg;     /* map units, axis aligned */
static const Seg roads[] = {
    /* Hillvile main street (east-west) and the avenues */
    { 24, 98, 112, 98 }, { 30, 160, 112, 160 }, { 40, 60, 104, 60 },
    { 56, 60, 56, 206 }, { 94, 60, 94, 200 }, { 24, 98, 24, 160 },
    /* to the park bridge */
    { 104, 128, 152, 128 },
    /* Hellvile lanes */
    { 150, 76, 226, 76 }, { 150, 76, 150, 200 }, { 150, 124, 210, 124 },
    { 150, 168, 196, 168 },
    /* the wall road */
    { 108, 40, 108, 128 }
};

static void draw_roads(void)
{
    int i, rw = 3;
    for (i = 0; i < DOOMO_COUNTOF(roads); i++) {
        const Seg *s = &roads[i];
        int x0 = fp_mx(s->x0), y0 = fp_my(s->y0), x1 = fp_mx(s->x1), y1 = fp_my(s->y1);
        int hell = s->x0 >= 140;
        uint8_t top = hell ? PAL_RAMP(RAMP_VAPE, 6) : PAL_RAMP(RAMP_PEACH, 7);
        uint8_t mid = hell ? PAL_RAMP(RAMP_VAPE, 5) : PAL_RAMP(RAMP_PEACH, 6);
        uint8_t edge = hell ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_PEACH, 4);
        if (y0 == y1) {
            R(x0, y0 - 1, x1 - x0 + rw, rw + 1, edge);
            R(x0, y0 - 1, x1 - x0 + rw, 1, top);
            R(x0, y0, x1 - x0 + rw, rw - 1, mid);
            if (!hell) {
                int x;
                for (x = x0 + 4; x < x1; x += 9) R(x, y0 + 1, 3, 1, PAL_RAMP(RAMP_PEACH, 7));
            } else {
                int x;
                for (x = x0 + 2; x < x1; x += 4) R(x, y0 + (x & 1), 2, 1, PAL_RAMP(RAMP_VAPE, 4));
            }
        } else {
            R(x0 - 1, y0, rw + 2, y1 - y0 + rw, edge);
            R(x0, y0, rw - 1, y1 - y0 + rw - 1, mid);
            R(x0, y0, 1, y1 - y0 + rw - 1, top);
            if (hell) {
                int y;
                for (y = y0 + 2; y < y1; y += 4) R(x0 + 1, y + (y & 1), 1, 2, PAL_RAMP(RAMP_VAPE, 4));
            }
        }
    }
}

/* ---- decorations ----------------------------------------------------------- */
typedef struct { uint8_t x, y, kind; } Deco;
enum { D_TREE, D_TREE_PINK, D_FLOWERS, D_DEAD_TREE, D_MUSHROOM, D_LAMP, D_PUMPKIN,
       D_CANDY, D_SKULL_LAMP, D_BUSH };
static const Deco decos[] = {
    { 18, 84, D_TREE }, { 14, 130, D_TREE_PINK }, { 66, 80, D_FLOWERS }, { 46, 138, D_BUSH },
    { 82, 130, D_TREE }, { 30, 186, D_TREE_PINK }, { 72, 228, D_FLOWERS }, { 112, 184, D_TREE },
    { 46, 40, D_TREE_PINK }, { 88, 86, D_LAMP }, { 40, 108, D_CANDY }, { 116, 92, D_FLOWERS },
    { 68, 178, D_LAMP }, { 20, 108, D_FLOWERS },
    { 168, 96, D_DEAD_TREE }, { 238, 104, D_MUSHROOM }, { 176, 150, D_PUMPKIN }, { 230, 150, D_DEAD_TREE },
    { 214, 216, D_MUSHROOM }, { 140, 166, D_SKULL_LAMP }, { 178, 44, D_PUMPKIN }, { 240, 186, D_DEAD_TREE },
    { 200, 100, D_SKULL_LAMP }, { 134, 222, D_MUSHROOM }, { 236, 128, D_PUMPKIN }, { 166, 228, D_DEAD_TREE }
};

static void deco_tree(int x, int y, uint8_t leaf_d, uint8_t leaf, uint8_t leaf_l, int sway)
{
    R(x - 1, y - 5, 3, 5, PAL_RAMP(RAMP_WOOD, 3));
    R(x - 1, y - 5, 1, 5, PAL_RAMP(RAMP_WOOD, 5));
    C(x + sway, y - 9, 5, ui_darker(leaf_d));
    C(x + sway, y - 9, 4, leaf_d);
    C(x - 1 + sway, y - 10, 3, leaf);
    C(x - 2 + sway, y - 11, 1, leaf_l);
    R(x - 3 + sway, y - 12, 1, 1, PAL_WHITE);
}

static void draw_deco(const Deco *d)
{
    int x = fp_mx(d->x), y = fp_my(d->y), t = g_t;
    int sway = ui_wave(t + d->x * 9, 90, 1);
    switch (d->kind) {
    case D_TREE:
        deco_tree(x, y, PAL_RAMP(RAMP_GRASS, 3), PAL_RAMP(RAMP_GRASS, 5), PAL_RAMP(RAMP_GRASS, 7), sway);
        break;
    case D_TREE_PINK:
        deco_tree(x, y, PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 6),
                  PAL_RAMP(RAMP_PASTEL_PINK, 7), sway);
        break;
    case D_BUSH:
        C(x - 2, y - 2, 3, PAL_RAMP(RAMP_GRASS, 3));
        C(x + 2, y - 2, 3, PAL_RAMP(RAMP_GRASS, 4));
        R(x - 1, y - 4, 1, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
        R(x + 2, y - 3, 1, 1, PAL_RAMP(RAMP_CANDY, 7));
        break;
    case D_FLOWERS: {
        int i;
        for (i = 0; i < 5; i++) {
            int fx = x - 6 + i * 3, fy = y - 2 - (i & 1) * 2 + (ui_wave(t + i * 10, 40, 1) > 0);
            static const uint8_t fc[5] = { PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_CANDY, 7),
                                           PAL_WHITE, PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_PASTEL_PINK, 5) };
            R(fx, fy + 1, 1, 2, PAL_RAMP(RAMP_GRASS, 3));
            R(fx - 1, fy, 3, 1, fc[i]);
            R(fx, fy - 1, 1, 3, fc[i]);
            R(fx, fy, 1, 1, PAL_RAMP(RAMP_CANDY, 4));
        }
        break; }
    case D_DEAD_TREE:
        R(x, y - 10, 2, 10, PAL_RAMP(RAMP_STONE, 1));
        R(x - 3, y - 9, 3, 1, PAL_RAMP(RAMP_STONE, 1));
        R(x - 4, y - 11, 1, 2, PAL_RAMP(RAMP_STONE, 1));
        R(x + 2, y - 7, 3, 1, PAL_RAMP(RAMP_STONE, 1));
        R(x + 4, y - 9, 1, 2, PAL_RAMP(RAMP_STONE, 1));
        R(x, y - 12, 1, 2, PAL_RAMP(RAMP_STONE, 2));
        R(x + 1, y - 10, 1, 9, PAL_RAMP(RAMP_STONE, 3));
        /* a sleepy bat hanging upside down */
        if ((t / 120 + d->x) % 3 == 0) {
            R(x - 4, y - 8, 3, 2, PAL_RAMP(RAMP_VAPE, 1));
            R(x - 5, y - 8, 1, 1, PAL_RAMP(RAMP_VAPE, 1));
            R(x - 1, y - 8, 1, 1, PAL_RAMP(RAMP_VAPE, 1));
        }
        break;
    case D_MUSHROOM: {
        int glow = g_tint == TINT_NIGHT;
        R(x - 1, y - 3, 2, 3, PAL_RAMP(RAMP_BONE, 6));
        E(x, y - 4, 3, 2, PAL_RAMP(RAMP_VAPE, 5));
        if (glow) { RG(x - 1, y - 5, 1, 1, PAL_RAMP(RAMP_MINT, 7)); RG(x + 2, y - 4, 1, 1, PAL_RAMP(RAMP_MINT, 7)); }
        else { R(x - 1, y - 5, 1, 1, PAL_WHITE); R(x + 2, y - 4, 1, 1, PAL_WHITE); }
        break; }
    case D_PUMPKIN:
        E(x, y - 3, 4, 3, PAL_PAINT(PAINT_TANGERINE, 0));
        E(x, y - 3, 3, 3, PAL_PAINT(PAINT_TANGERINE, 1));
        R(x, y - 7, 1, 2, PAL_RAMP(RAMP_GRASS, 2));
        if (g_tint == TINT_NIGHT) {
            RG(x - 2, y - 4, 1, 1, PAL_RAMP(RAMP_CANDY, 7));
            RG(x + 1, y - 4, 1, 1, PAL_RAMP(RAMP_CANDY, 7));
            RG(x - 1, y - 2, 3, 1, PAL_RAMP(RAMP_CANDY, 6));
        } else {
            R(x - 2, y - 4, 1, 1, PAL_PAINT(PAINT_TANGERINE, 2));
        }
        break;
    case D_CANDY:
        R(x, y - 8, 1, 8, PAL_WHITE);
        C(x, y - 10, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        R(x - 1, y - 11, 2, 1, PAL_WHITE);
        R(x + 1, y - 9, 1, 1, PAL_PAINT(PAINT_MINT, 1));
        break;
    case D_LAMP: case D_SKULL_LAMP: {
        int lit = g_tint != 0;
        R(x, y - 9, 1, 9, PAL_RAMP(RAMP_STONE, 1));
        if (d->kind == D_LAMP) {
            R(x - 1, y - 11, 3, 3, PAL_RAMP(RAMP_STONE, 2));
            if (lit) { RG(x, y - 10, 1, 1, PAL_WHITE); RG(x - 2, y - 12, 5, 5, PAL_RAMP(RAMP_CANDY, 6)); RG(x - 1, y - 11, 3, 3, PAL_WHITE); }
            else R(x, y - 10, 1, 1, PAL_RAMP(RAMP_CANDY, 6));
        } else {
            R(x - 1, y - 12, 3, 3, PAL_RAMP(RAMP_BONE, 6));
            R(x - 1, y - 11, 1, 1, lit ? PAL_RAMP(RAMP_MINT, 7) : PAL_RAMP(RAMP_STONE, 1));
            R(x + 1, y - 11, 1, 1, lit ? PAL_RAMP(RAMP_MINT, 7) : PAL_RAMP(RAMP_STONE, 1));
            if (lit) RG(x - 2, y - 13, 1, 1, PAL_RAMP(RAMP_MINT, 6));
        }
        break; }
    default: break;
    }
}

/* ====================================================================== */
/* buildings                                                                */
static void flag(int x, int y, uint8_t c)
{
    int w = ui_wave(g_t + x, 20, 1);
    R(x, y, 1, 7, PAL_RAMP(RAMP_STONE, 2));
    R(x + 1, y, 4, 2, c);
    R(x + 1, y + 2, 3 + w, 1, c);
    R(x + 1, y, 1, 1, ui_lighter(c));
}

static void chimney_smoke(int x, int y, uint8_t c)
{
    int i;
    for (i = 0; i < 3; i++) {
        int ph = (g_t / 2 + i * 16) % 48;
        int r = 1 + ph / 16;
        C(x + ph / 8 + ui_wave(ph * 4, 40, 1), y - ph / 3, r, c);
    }
}

static void b_towers(int x, int y, int hell)
{
    int i, j, w = 18, h = 26, bx = x - w / 2, by = y - h;
    uint8_t d = hell ? PAL_RAMP(RAMP_VAPE, 2) : PAL_PAINT(PAINT_BUBBLEGUM, 0);
    uint8_t b = hell ? PAL_RAMP(RAMP_VAPE, 3) : PAL_PAINT(PAINT_BUBBLEGUM, 1);
    uint8_t l = hell ? PAL_RAMP(RAMP_VAPE, 5) : PAL_PAINT(PAINT_BUBBLEGUM, 2);
    box(bx, by, w, h, d, b, l);
    /* roof trim */
    R(bx - 1, by - 2, w + 2, 3, hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_PAINT(PAINT_ROSE, 0));
    R(bx, by - 2, w, 1, hell ? PAL_RAMP(RAMP_STONE, 3) : PAL_PAINT(PAINT_ROSE, 2));
    for (j = 0; j < 4; j++)
        for (i = 0; i < 3; i++)
            win(bx + 3 + i * 5, by + 3 + j * 5, 3, 3, night_lit(x * 31 + i * 7 + j * 13 + hell),
                hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_PAINT(PAINT_ROSE, 0));
    /* door */
    R(x - 2, y - 4, 4, 4, hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_WOOD, 3));
    R(x - 2, y - 4, 4, 1, hell ? PAL_RAMP(RAMP_BLOOD, 4) : PAL_RAMP(RAMP_WOOD, 5));
    if (hell) {
        /* little horns and bat wings */
        R(bx + 1, by - 5, 2, 3, PAL_RAMP(RAMP_BONE, 6));
        R(bx, by - 6, 1, 2, PAL_RAMP(RAMP_BONE, 7));
        R(bx + w - 3, by - 5, 2, 3, PAL_RAMP(RAMP_BONE, 6));
        R(bx + w - 1, by - 6, 1, 2, PAL_RAMP(RAMP_BONE, 7));
        {
            int f = ui_wave(g_t, 24, 1);
            R(bx - 4, by + 4 + f, 4, 2, PAL_RAMP(RAMP_VAPE, 1));
            R(bx - 5, by + 6 + f, 2, 1, PAL_RAMP(RAMP_VAPE, 1));
            R(bx + w, by + 4 - f, 4, 2, PAL_RAMP(RAMP_VAPE, 1));
            R(bx + w + 3, by + 6 - f, 2, 1, PAL_RAMP(RAMP_VAPE, 1));
        }
    } else {
        /* a heart sign on the roof + flower boxes */
        ui_heart(x, by - 5 + (ui_wave(g_t, 40, 1) > 0), 3, tc(PAL_PAINT(PAINT_ROSE, 1)));
        R(bx + 2, by + 7, 5, 1, PAL_RAMP(RAMP_GRASS, 4));
        R(bx + 12, by + 12, 5, 1, PAL_RAMP(RAMP_GRASS, 4));
        R(bx + 3, by + 6, 1, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
        R(bx + 13, by + 11, 1, 1, PAL_RAMP(RAMP_CANDY, 7));
    }
}

static void b_town_hall(int x, int y)
{
    int w = 26, h = 14, bx = x - w / 2, by = y - h, tw = 10;
    int hr = town_hour() % 12, mn = doomo_town.minute % 60;
    box(bx, by, w, h, PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_RAMP(RAMP_PEACH, 7));
    /* columns */
    R(bx + 3, by + 4, 2, h - 5, PAL_WHITE);
    R(bx + w - 6, by + 4, 2, h - 5, PAL_WHITE);
    R(x - 2, y - 6, 4, 6, PAL_RAMP(RAMP_WOOD, 3));
    /* clock tower */
    box(x - tw / 2, by - 10, tw, 11, PAL_RAMP(RAMP_PEACH, 5), PAL_RAMP(RAMP_PEACH, 6), PAL_WHITE);
    roof(x, by - 17, tw + 4, 7, PAL_PAINT(PAINT_SKY, 0), PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_SKY, 2));
    C(x, by - 5, 3, PAL_RAMP(RAMP_STONE, 2));
    C(x, by - 5, 2, PAL_WHITE);
    /* the hands follow the island clock */
    {
        int ha = hr * 4096 / 12 + mn * 4096 / 720, ma = mn * 4096 / 60;
        int hx = FIX2INT(fx_sin(ha) * 2), hy = -FIX2INT(fx_cos(ha) * 2);
        int mx = FIX2INT(fx_sin(ma) * 2 + FIX_HALF), my = -FIX2INT(fx_cos(ma) * 2 + FIX_HALF);
        plat_line(x, by - 5, x + hx, by - 5 + hy, tc(PAL_RAMP(RAMP_STONE, 0)));
        plat_line(x, by - 5, x + mx, by - 5 + my, tc(PAL_PAINT(PAINT_CHERRY, 0)));
    }
    flag(x, by - 24, PAL_PAINT(PAINT_ROSE, 1));
    win(bx + 7, by + 5, 2, 3, night_lit(77), PAL_RAMP(RAMP_PEACH, 4));
    win(bx + w - 9, by + 5, 2, 3, night_lit(78), PAL_RAMP(RAMP_PEACH, 4));
}

static void awning(int x, int y, int w, uint8_t a, uint8_t b)
{
    int i;
    for (i = 0; i < w; i += 3) R(x + i, y, DOOMO_MIN(3, w - i), 3, (i / 3) & 1 ? b : a);
    for (i = 0; i < w; i += 3) R(x + i + 1, y + 3, 1, 1, (i / 3) & 1 ? b : a);
    R(x, y, w, 1, ui_lighter(a));
}

static void b_shop(int x, int y, int black)
{
    int w = 22, h = 14, bx = x - w / 2, by = y - h;
    if (black) {
        /* the black market: a patched purple tent with a skull lantern */
        int i;
        for (i = 0; i < 14; i++) {
            int rw = 2 + i * 22 / 14;
            R(x - rw / 2 - 1, by - 2 + i, rw + 2, 1, PAL_RAMP(RAMP_VAPE, 0));
            R(x - rw / 2, by - 2 + i, rw, 1, (i / 3) & 1 ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_VAPE, 3));
        }
        R(x - 4, by + 4, 8, 8, PAL_RAMP(RAMP_STONE, 0));
        R(x - 3, by + 5, 6, 7, PAL_RAMP(RAMP_BLOOD, 1));
        R(x - 7, by + 6, 3, 3, PAL_PAINT(PAINT_SLIME, 0));
        {
            int fl = (g_t / 5) % 3;
            int lit = g_tint != 0;
            R(x + 6, by + 1, 1, 3, PAL_RAMP(RAMP_STONE, 1));
            R(x + 5, by + 4, 3, 3, PAL_RAMP(RAMP_BONE, 6));
            RG(x + 5, by + 5, 1, 1, lit ? PAL_RAMP(RAMP_MINT, 7 - fl) : PAL_RAMP(RAMP_STONE, 0));
            RG(x + 7, by + 5, 1, 1, lit ? PAL_RAMP(RAMP_MINT, 7 - fl) : PAL_RAMP(RAMP_STONE, 0));
        }
        R(x - 1, by - 5, 2, 3, PAL_RAMP(RAMP_STONE, 1));
        return;
    }
    box(bx, by, w, h, PAL_RAMP(RAMP_PEACH, 5), PAL_UI_CREAM, PAL_WHITE);
    awning(bx - 1, by + 1, w + 2, PAL_PAINT(PAINT_ROSE, 1), PAL_WHITE);
    win(bx + 3, by + 7, 6, 4, night_lit(301), PAL_RAMP(RAMP_PEACH, 4));
    R(x + 3, y - 7, 5, 7, PAL_RAMP(RAMP_WOOD, 3));
    R(x + 3, y - 7, 5, 1, PAL_RAMP(RAMP_WOOD, 5));
    /* coin sign */
    C(x, by - 3, 3, PAL_RAMP(RAMP_CANDY, 3));
    C(x, by - 3, 2, PAL_UI_GOLD);
    R(x - 1, by - 4, 1, 1, PAL_WHITE);
}

static void b_cafe(int x, int y)
{
    int w = 20, h = 13, bx = x - w / 2, by = y - h;
    box(bx, by, w, h, PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5), PAL_RAMP(RAMP_WOOD, 6));
    awning(bx - 1, by + 1, w + 2, PAL_PAINT(PAINT_MINT, 1), PAL_WHITE);
    win(bx + 3, by + 6, 5, 4, night_lit(402), PAL_RAMP(RAMP_WOOD, 3));
    R(x + 2, y - 7, 5, 7, PAL_PAINT(PAINT_CHOCOLATE, 1));
    /* the giant cupcake on the roof */
    R(x - 5, by - 5, 10, 5, PAL_PAINT(PAINT_TANGERINE, 1));
    R(x - 5, by - 5, 10, 1, PAL_PAINT(PAINT_TANGERINE, 2));
    R(x - 4, by - 4, 1, 4, PAL_PAINT(PAINT_TANGERINE, 0));
    R(x, by - 4, 1, 4, PAL_PAINT(PAINT_TANGERINE, 0));
    C(x, by - 8, 5, PAL_PAINT(PAINT_BUBBLEGUM, 0));
    C(x, by - 8, 4, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    C(x - 1, by - 10, 2, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    C(x, by - 13, 1, PAL_PAINT(PAINT_CHERRY, 1));
    R(x - 3, by - 8, 1, 1, PAL_PAINT(PAINT_LEMON, 1));
    R(x + 2, by - 9, 1, 1, PAL_PAINT(PAINT_SKY, 1));
    R(x + 1, by - 6, 1, 1, PAL_PAINT(PAINT_MINT, 1));
    chimney_smoke(bx + w - 3, by - 1, PAL_RAMP(RAMP_STONE, 6));
}

static void b_church(int x, int y)
{
    int w = 16, h = 14, bx = x - w / 2, by = y - h, sw = ui_wave(g_t, 50, 1);
    box(bx, by, w, h, PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6), PAL_WHITE);
    roof(x, by - 7, w + 4, 8, PAL_PAINT(PAINT_LAVENDER, 0), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2));
    /* steeple with a swinging bell and a plush cross */
    box(x - 3, by - 14, 7, 8, PAL_RAMP(RAMP_STONE, 5), PAL_RAMP(RAMP_STONE, 6), PAL_WHITE);
    roof(x, by - 20, 9, 6, PAL_PAINT(PAINT_LAVENDER, 0), PAL_PAINT(PAINT_LAVENDER, 1), PAL_PAINT(PAINT_LAVENDER, 2));
    R(x - 1 + sw, by - 11, 3, 3, PAL_UI_GOLD);
    R(x, by - 26, 1, 6, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    R(x - 2, by - 24, 5, 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    /* heart rose window + door */
    ui_heart(x, by + 4, 2, g_tint ? PAL_RAMP(RAMP_CANDY, 6) : tc(PAL_PAINT(PAINT_ROSE, 1)));
    R(x - 2, y - 6, 4, 6, PAL_RAMP(RAMP_WOOD, 3));
    R(x - 1, y - 7, 2, 1, PAL_RAMP(RAMP_WOOD, 3));
}

static void b_hospital(int x, int y)
{
    int w = 22, h = 15, bx = x - w / 2, by = y - h;
    int pulse = (g_t / 15) % 2;
    box(bx, by, w, h, PAL_RAMP(RAMP_MINT, 4), PAL_RAMP(RAMP_MINT, 6), PAL_RAMP(RAMP_MINT, 7));
    R(bx - 1, by - 2, w + 2, 3, PAL_RAMP(RAMP_MINT, 3));
    win(bx + 3, by + 4, 3, 3, night_lit(501), PAL_RAMP(RAMP_MINT, 3));
    win(bx + w - 7, by + 4, 3, 3, night_lit(502), PAL_RAMP(RAMP_MINT, 3));
    win(bx + 3, by + 9, 3, 3, night_lit(503), PAL_RAMP(RAMP_MINT, 3));
    R(x - 2, y - 6, 4, 6, PAL_RAMP(RAMP_SKY, 4));
    /* the red cross sign, pulsing softly */
    R(x - 4, by - 8, 9, 7, PAL_WHITE);
    R(x - 1, by - 7, 3, 5, pulse ? PAL_UI_RED : PAL_PAINT(PAINT_CHERRY, 1));
    R(x - 3, by - 6, 7, 3, pulse ? PAL_UI_RED : PAL_PAINT(PAINT_CHERRY, 1));
}

static void b_fair(int x, int y)
{
    int r = 9, cx = x - 3, cy = y - 13, i, a0 = g_t * 6;
    /* ferris wheel */
    R(cx - 1, cy, 2, 13, PAL_RAMP(RAMP_STONE, 4));
    R(cx - 4, y - 2, 8, 2, PAL_RAMP(RAMP_STONE, 3));
    for (i = 0; i < 8; i++) {
        int a = a0 + i * 512;
        int ex = cx + FIX2INT(fx_cos(a) * r), ey = cy + FIX2INT(fx_sin(a) * r);
        plat_line(cx, cy, ex, ey, tc(PAL_RAMP(RAMP_STONE, 6)));
    }
    for (i = 0; i < 16; i++) {
        int a = i * 256;
        R(cx + FIX2INT(fx_cos(a) * r), cy + FIX2INT(fx_sin(a) * r), 1, 1, PAL_PAINT(PAINT_ROSE, 1));
    }
    for (i = 0; i < 6; i++) {
        static const uint8_t gc[6] = { PAINT_LEMON, PAINT_MINT, PAINT_SKY, PAINT_BUBBLEGUM, PAINT_LAVENDER, PAINT_TANGERINE };
        int a = a0 + i * 683;
        int gx = cx + FIX2INT(fx_cos(a) * r), gy = cy + FIX2INT(fx_sin(a) * r);
        R(gx - 1, gy, 3, 3, PAL_PAINT(gc[i], 1));
        R(gx - 1, gy, 3, 1, PAL_PAINT(gc[i], 2));
    }
    C(cx, cy, 1, PAL_UI_GOLD);
    /* striped tent */
    for (i = 0; i < 8; i++) {
        int rw = 2 + i * 12 / 8;
        R(x + 8 - rw / 2, y - 8 + i, rw, 1, (i & 1) ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_WHITE);
    }
    R(x + 8, y - 11, 1, 3, PAL_RAMP(RAMP_STONE, 2));
    R(x + 9, y - 11, 2, 1, PAL_UI_GOLD);
}

static void b_library(int x, int y)
{
    int w = 22, h = 14, bx = x - w / 2, by = y - h;
    box(bx, by, w, h, PAL_RAMP(RAMP_WOOD, 2), PAL_RAMP(RAMP_WOOD, 4), PAL_RAMP(RAMP_WOOD, 5));
    roof(x, by - 7, w + 4, 8, PAL_PAINT(PAINT_FOREST, 0), PAL_PAINT(PAINT_FOREST, 1), PAL_PAINT(PAINT_FOREST, 2));
    win(bx + 3, by + 4, 4, 5, night_lit(601), PAL_RAMP(RAMP_WOOD, 1));
    win(bx + w - 7, by + 4, 4, 5, night_lit(602), PAL_RAMP(RAMP_WOOD, 1));
    R(x - 2, y - 7, 4, 7, PAL_RAMP(RAMP_WOOD, 1));
    /* an open book sign */
    R(x - 5, by - 3, 5, 4, PAL_WHITE);
    R(x + 1, by - 3, 5, 4, PAL_WHITE);
    R(x, by - 3, 1, 5, PAL_PAINT(PAINT_CHERRY, 0));
    R(x - 4, by - 2, 3, 1, PAL_RAMP(RAMP_STONE, 4));
    R(x + 2, by - 2, 3, 1, PAL_RAMP(RAMP_STONE, 4));
}

static void b_concert(int x, int y)
{
    int i, by = y - 10;
    R(x - 13, by, 26, 10, ui_darker(PAL_PAINT(PAINT_LAVENDER, 0)));
    R(x - 12, by + 1, 24, 9, PAL_PAINT(PAINT_LAVENDER, 1));
    for (i = 0; i < 4; i++) R(x - 10 + i * 6, by + 3, 3, 7, PAL_PAINT(PAINT_GRAPE, 0));
    /* dome */
    E(x, by, 13, 9, PAL_PAINT(PAINT_GRAPE, 0));
    E(x, by, 12, 8, PAL_PAINT(PAINT_GRAPE, 1));
    E(x - 3, by - 3, 6, 4, PAL_PAINT(PAINT_GRAPE, 2));
    R(x - 13, by, 26, 2, PAL_PAINT(PAINT_GRAPE, 0));
    R(x - 1, by - 12, 2, 4, PAL_UI_GOLD);
    /* notes floating up */
    for (i = 0; i < 2; i++) {
        int ph = (g_t + i * 30) % 60, nx = x + 6 + i * 4 + ui_wave(ph * 4, 60, 2);
        if (ph < 50) {
            char g[2] = { (char)ICON_NOTE, 0 };
            plat_text(nx, by - 12 - ph / 3, g, tc(i ? PAL_PAINT(PAINT_ROSE, 1) : PAL_PAINT(PAINT_SKY, 0)));
        }
    }
}

static void gear(int cx, int cy, int r, int a0, uint8_t c)
{
    int i;
    C(cx, cy, r, c);
    for (i = 0; i < 6; i++) {
        int a = a0 + i * 683;
        R(cx + FIX2INT(fx_cos(a) * (r + 1)) - 1, cy + FIX2INT(fx_sin(a) * (r + 1)) - 1, 2, 2, c);
    }
    C(cx, cy, r / 2 > 0 ? r / 2 : 1, ui_lighter(c));
}

static void b_workshop(int x, int y)
{
    int w = 22, h = 13, bx = x - w / 2, by = y - h;
    box(bx, by, w, h, PAL_RAMP(RAMP_STEEL, 3), PAL_RAMP(RAMP_STEEL, 4), PAL_RAMP(RAMP_STEEL, 6));
    R(bx - 1, by - 2, w + 2, 3, PAL_RAMP(RAMP_WOOD, 2));
    /* garage door stripes */
    R(x - 6, y - 8, 12, 8, PAL_RAMP(RAMP_STEEL, 2));
    R(x - 6, y - 6, 12, 1, PAL_RAMP(RAMP_STEEL, 5));
    R(x - 6, y - 3, 12, 1, PAL_RAMP(RAMP_STEEL, 5));
    if ((g_t / 7) % 5 == 0) RG(x - 2 + (g_t % 5), y - 5, 2, 2, PAL_RAMP(RAMP_LAVA, 7));   /* weld sparks */
    gear(bx + 4, by - 5, 3, g_t * 12, PAL_UI_GOLD);
    R(bx + w - 5, by - 7, 3, 6, PAL_RAMP(RAMP_STONE, 2));
    chimney_smoke(bx + w - 4, by - 8, PAL_RAMP(RAMP_STONE, 4));
}

static void b_beach(int x, int y)
{
    int i;
    /* sandcastle */
    R(x + 3, y - 5, 7, 5, PAL_RAMP(RAMP_CANDY, 4));
    R(x + 3, y - 7, 2, 2, PAL_RAMP(RAMP_CANDY, 4));
    R(x + 8, y - 7, 2, 2, PAL_RAMP(RAMP_CANDY, 4));
    R(x + 4, y - 4, 2, 1, PAL_RAMP(RAMP_CANDY, 6));
    R(x + 6, y - 2, 1, 2, PAL_RAMP(RAMP_PEACH, 4));
    flag(x + 9, y - 12, PAL_PAINT(PAINT_SKY, 1));
    /* umbrella */
    R(x - 5, y - 12, 1, 12, PAL_WHITE);
    for (i = 0; i < 5; i++) {
        int rw = 3 + i * 3;
        R(x - 5 - rw / 2, y - 16 + i, rw, 1, (i & 1) ? PAL_WHITE : PAL_PAINT(PAINT_CHERRY, 1));
    }
    /* towel */
    R(x - 11, y - 3, 7, 3, PAL_PAINT(PAINT_SKY, 1));
    R(x - 11, y - 2, 7, 1, PAL_WHITE);
    /* a beach ball bouncing */
    {
        int b = ui_wave(g_t, 30, 3);
        int by = y - 4 - (b > 0 ? b : -b);
        C(x - 13, by, 2, PAL_PAINT(PAINT_LEMON, 1));
        R(x - 14, by - 1, 1, 1, PAL_WHITE);
        R(x - 13, by, 2, 1, PAL_PAINT(PAINT_CHERRY, 1));
    }
}

static void b_park(int x, int y)
{
    int i, t = g_t;
    /* lawn circle */
    E(x, y - 4, 14, 6, PAL_RAMP(RAMP_GRASS, 4));
    E(x, y - 5, 13, 5, PAL_RAMP(RAMP_GRASS, 6));
    /* the bridge planks over the river */
    R(x - 6, y - 7, 12, 4, PAL_RAMP(RAMP_WOOD, 4));
    for (i = 0; i < 12; i += 2) R(x - 6 + i, y - 7, 1, 4, PAL_RAMP(RAMP_WOOD, 2));
    R(x - 6, y - 7, 12, 1, PAL_RAMP(RAMP_WOOD, 6));
    /* fountain */
    E(x, y - 8, 6, 3, PAL_RAMP(RAMP_STONE, 4));
    E(x, y - 9, 5, 2, PAL_RAMP(RAMP_SKY, 5));
    R(x - 1, y - 15, 2, 6, PAL_RAMP(RAMP_STONE, 5));
    for (i = 0; i < 6; i++) {
        int ph = (t + i * 5) % 30;
        int dx = (i - 3) * ph / 10, dy = -15 - ph / 2 + ph * ph / 60;
        R(x + dx, y + dy, 1, 1, PAL_RAMP(RAMP_SKY, 7));
    }
    R(x - 1, y - 17, 2, 2, PAL_WHITE);
    /* two trees, one pastel one spooky: the two halves of the park */
    deco_tree(x - 11, y - 3, PAL_RAMP(RAMP_PASTEL_PINK, 4), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7),
              ui_wave(t, 90, 1));
    deco_tree(x + 11, y - 3, PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 6),
              ui_wave(t + 40, 90, 1));
}

static void b_wall_gate(int x, int y)
{
    int i, t = g_t;
    /* a tall chunk of wall with an arched gate */
    box(x - 12, y - 16, 24, 16, PAL_RAMP(RAMP_STONE, 2), PAL_RAMP(RAMP_STONE, 4), PAL_RAMP(RAMP_STONE, 5));
    for (i = 0; i < 6; i++) R(x - 12 + i * 4, y - 19, 3, 3, PAL_RAMP(RAMP_STONE, 4));
    for (i = 0; i < 6; i++) R(x - 12 + i * 4, y - 19, 3, 1, PAL_RAMP(RAMP_STONE, 6));
    R(x - 5, y - 11, 10, 11, PAL_RAMP(RAMP_STONE, 0));
    E(x, y - 11, 5, 3, PAL_RAMP(RAMP_STONE, 0));
    /* the bars, the chains and the big padlock */
    for (i = 0; i < 4; i++) R(x - 4 + i * 3, y - 12, 1, 12, PAL_RAMP(RAMP_STEEL, 4));
    plat_line(x - 5, y - 9, x + 5, y - 3, tc(PAL_RAMP(RAMP_STEEL, 5)));
    plat_line(x + 5, y - 9, x - 5, y - 3, tc(PAL_RAMP(RAMP_STEEL, 5)));
    R(x - 2, y - 7, 5, 4, PAL_UI_GOLD);
    R(x - 1, y - 9, 3, 2, PAL_RAMP(RAMP_CANDY, 3));
    R(x, y - 6, 1, 2, PAL_RAMP(RAMP_CANDY, 2));
    /* purple fog leaking over the top */
    for (i = 0; i < 3; i++) {
        int ph = (t / 2 + i * 21) % 64;
        if (ph < 48) C(x - 8 + i * 8 + ph / 12, y - 20 - ph / 8, 1 + ph / 20, PAL_RAMP(RAMP_VAPE, 5));
    }
}

static void b_arena(int x, int y, int locked)
{
    int i;
    E(x, y - 6, 13, 7, PAL_RAMP(RAMP_BLOOD, 1));
    E(x, y - 7, 12, 6, PAL_RAMP(RAMP_BONE, 4));
    E(x, y - 8, 9, 4, PAL_RAMP(RAMP_PEACH, 4));
    for (i = 0; i < 7; i++) R(x - 11 + i * 4, y - 5, 2, 3, PAL_RAMP(RAMP_BLOOD, 1));
    flag(x - 11, y - 18, PAL_PAINT(PAINT_HELLFIRE, 1));
    flag(x + 10, y - 18, PAL_PAINT(PAINT_HELLFIRE, 1));
    if (locked) {
        plat_line(x - 12, y - 12, x + 12, y - 2, tc(PAL_RAMP(RAMP_STEEL, 3)));
        plat_line(x + 12, y - 12, x - 12, y - 2, tc(PAL_RAMP(RAMP_STEEL, 3)));
        R(x - 3, y - 10, 6, 5, PAL_RAMP(RAMP_STEEL, 4));
        R(x - 2, y - 12, 4, 2, PAL_RAMP(RAMP_STEEL, 5));
        R(x - 1, y - 8, 1, 2, PAL_RAMP(RAMP_STEEL, 1));
    } else {
        int ph = (g_t / 4) % 8;
        RG(x - 4 + ph, y - 9, 2, 2, PAL_RAMP(RAMP_LAVA, 6));
    }
}

static void b_square(int x, int y)
{
    int i, t = g_t;
    /* goth fountain with purple glowing water and green flames */
    E(x, y - 4, 10, 4, PAL_RAMP(RAMP_STONE, 1));
    E(x, y - 5, 9, 3, PAL_RAMP(RAMP_STONE, 3));
    E(x, y - 5, 7, 2, PAL_RAMP(RAMP_VAPE, 5));
    R(x - 1, y - 13, 3, 8, PAL_RAMP(RAMP_STONE, 2));
    R(x - 2, y - 15, 5, 3, PAL_RAMP(RAMP_BONE, 6));
    R(x - 1, y - 14, 1, 1, PAL_RAMP(RAMP_STONE, 0));
    R(x + 1, y - 14, 1, 1, PAL_RAMP(RAMP_STONE, 0));
    for (i = -1; i <= 1; i += 2) {
        int fx = x + i * 11, fl = ui_wave(t + i * 13, 12, 1);
        R(fx, y - 9, 1, 9, PAL_RAMP(RAMP_STONE, 1));
        RG(fx - 1, y - 13 + fl, 3, 3, PAL_PAINT(PAINT_SLIME, 1));
        RG(fx, y - 15 + fl, 1, 2, PAL_PAINT(PAINT_SLIME, 2));
    }
    for (i = 0; i < 3; i++) {
        int ph = (t + i * 20) % 60;
        if (ph < 40) C(x - 4 + i * 4 + ph / 10, y - 16 - ph / 4, 1 + ph / 20, PAL_RAMP(RAMP_VAPE, 6));
    }
}

static void b_lake(int x, int y)
{
    int i, t = g_t;
    E(x, y - 5, 16, 6, PAL_RAMP(RAMP_STONE, 1));
    E(x, y - 5, 15, 5, PAL_RAMP(RAMP_LAVA, 3));
    E(x - 2, y - 6, 11, 3, PAL_RAMP(RAMP_LAVA, 5));
    for (i = 0; i < 4; i++) {
        int ph = (t / 2 + i * 13) % 26;
        int bx = x - 10 + i * 6, by = y - 5 + (i & 1);
        if (ph < 8) RG(bx, by - ph / 3, 2, 2, PAL_RAMP(RAMP_LAVA, 7));
        else if (ph < 11) RG(bx - 1, by - 3, 4, 1, PAL_RAMP(RAMP_LAVA, 6));
    }
    /* the pier */
    R(x + 9, y - 9, 9, 3, PAL_RAMP(RAMP_WOOD, 2));
    R(x + 9, y - 9, 9, 1, PAL_RAMP(RAMP_WOOD, 4));
    R(x + 10, y - 6, 1, 3, PAL_RAMP(RAMP_WOOD, 1));
    R(x + 16, y - 6, 1, 3, PAL_RAMP(RAMP_WOOD, 1));
    /* a fishing line */
    plat_line(x + 15, y - 15, x + 6, y - 6, tc(PAL_RAMP(RAMP_STONE, 6)));
    R(x + 15, y - 15, 1, 7, PAL_RAMP(RAMP_WOOD, 3));
}

static void tomb(int x, int y, uint8_t c)
{
    R(x - 2, y - 6, 5, 6, ui_darker(c));
    R(x - 1, y - 7, 3, 1, ui_darker(c));
    R(x - 1, y - 6, 3, 6, c);
    R(x - 1, y - 6, 1, 6, ui_lighter(c));
    R(x, y - 5, 1, 3, ui_darker(c));
    R(x - 1, y - 4, 3, 1, ui_darker(c));
}

static void b_graveyard(int x, int y)
{
    int i, t = g_t;
    /* fence */
    for (i = 0; i < 8; i++) R(x - 13 + i * 4, y - 6, 1, 6, PAL_RAMP(RAMP_STONE, 1));
    R(x - 13, y - 4, 29, 1, PAL_RAMP(RAMP_STONE, 1));
    tomb(x - 8, y - 2, PAL_RAMP(RAMP_STONE, 5));
    tomb(x, y - 4, PAL_RAMP(RAMP_STONE, 6));
    tomb(x + 8, y - 1, PAL_RAMP(RAMP_STONE, 5));
    R(x - 5, y - 2, 2, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
    R(x + 4, y - 1, 2, 1, PAL_RAMP(RAMP_CANDY, 7));
    /* the friendly ghost */
    {
        int gx = x + 6 + ui_wave(t, 80, 4), gy = y - 15 + ui_wave(t + 20, 50, 2);
        C(gx, gy, 3, PAL_RAMP(RAMP_MINT, 6));
        R(gx - 3, gy, 7, 3, PAL_RAMP(RAMP_MINT, 6));
        R(gx - 3, gy + 3, 1, 1, PAL_RAMP(RAMP_MINT, 6));
        R(gx, gy + 3, 1, 1, PAL_RAMP(RAMP_MINT, 6));
        R(gx + 3, gy + 3, 1, 1, PAL_RAMP(RAMP_MINT, 6));
        R(gx - 1, gy - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
        R(gx + 1, gy - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
        R(gx - 2, gy - 2, 1, 1, PAL_WHITE);
    }
}

void fp_island_building(int kind, int x, int y, int locked)
{
    switch (kind) {
    case LOC_TOWERS: b_towers(x, y, 0); break;
    case LOC_PITS: b_towers(x, y, 1); break;
    case LOC_TOWN_HALL: b_town_hall(x, y); break;
    case LOC_SHOP: b_shop(x, y, 0); break;
    case LOC_BLACK_MARKET: b_shop(x, y, 1); break;
    case LOC_CAFE: b_cafe(x, y); break;
    case LOC_CHURCH: b_church(x, y); break;
    case LOC_HOSPITAL: b_hospital(x, y); break;
    case LOC_FUN_FAIR: b_fair(x, y); break;
    case LOC_LIBRARY: b_library(x, y); break;
    case LOC_CONCERT: b_concert(x, y); break;
    case LOC_WORKSHOP: b_workshop(x, y); break;
    case LOC_BEACH: b_beach(x, y); break;
    case LOC_PARK: b_park(x, y); break;
    case LOC_WALL: b_wall_gate(x, y); break;
    case LOC_ARENA: b_arena(x, y, locked); break;
    case LOC_SQUARE: b_square(x, y); break;
    case LOC_LAKE: b_lake(x, y); break;
    case LOC_GRAVEYARD: b_graveyard(x, y); break;
    default: break;
    }
}

/* ---- walkers: tiny citizens strolling the roads ---------------------------- */
static void draw_walkers(void)
{
    int i, t = g_t, n = 14;
    int pop = town_count(-1);
    if (pop <= 0) return;
    if (pop < 40) n = 3 + pop / 6;
    for (i = 0; i < n; i++) {
        const Seg *s = &roads[i % DOOMO_COUNTOF(roads)];
        int len = (s->x1 - s->x0) + (s->y1 - s->y0), p, x, y, hell, step;
        uint32_t h = doomo_hash32((uint32_t)i * 4099u + 17u);
        static const uint8_t shirts[6] = { PAINT_ROSE, PAINT_SKY, PAINT_LEMON, PAINT_MINT,
                                           PAINT_LAVENDER, PAINT_TANGERINE };
        if (len <= 0) continue;
        p = (int)((h + (uint32_t)(t * (1 + (h >> 28) % 2) / 4)) % (uint32_t)(len * 2));
        if (p > len) p = len * 2 - p;                /* walk back and forth */
        x = fp_mx(s->x0 + (s->y0 == s->y1 ? p : 0)) + 1;
        y = fp_my(s->y0 + (s->y0 == s->y1 ? 0 : p)) + 1;
        hell = s->x0 >= 140;
        step = (t / 6 + i) & 1;
        if (hell && town_is_night() == 0 && (i & 1)) continue;
        R(x, y - 5, 3, 3, hell ? PAL_RAMP(RAMP_VAPE, 6) : PAL_RAMP(RAMP_PEACH, 7));
        R(x, y - 2, 3, 2, PAL_PAINT(hell ? PAINT_GRAPE : shirts[h % 6], 1));
        R(x + step * 2, y, 1, 1, PAL_RAMP(RAMP_STONE, 1));
        R(x + 1, y - 4, 1, 1, PAL_RAMP(RAMP_STONE, 0));
    }
}

/* ---- weather --------------------------------------------------------------- */
int fp_island_weather(void)
{
    int w = opt_get(OPT_WEATHER), m, d;
    uint32_t h;
    if (!w) return WX_CLEAR;
    sim_date(&m, &d);
    h = doomo_hash32(doomo_town.seed ^ (doomo_town.day * 2246822519u));
    if ((h & 15) < (unsigned)(w == 2 ? 6 : 3)) {
        /* months 0..11 from April: Dec, Jan, Feb are 8, 9, 10 */
        if (m >= 8 && m <= 10) return WX_SNOW;
        return WX_RAIN;
    }
    if (((h >> 4) & 15) < 3) return WX_PETALS;
    return WX_CLEAR;
}

static void draw_weather(int wx)
{
    int i, t = g_t;
    if (wx == WX_CLEAR) return;
    for (i = 0; i < 40; i++) {
        uint32_t h = doomo_hash32((uint32_t)i * 2654435761u + 99u);
        int x0 = (int)(h % (uint32_t)M.w), sp = wx == WX_RAIN ? 5 : wx == WX_SNOW ? 1 : 1;
        int y = M.y + (int)(((h >> 8) % (uint32_t)M.h + (uint32_t)(t * sp)) % (uint32_t)M.h);
        int x = M.x + (x0 + (wx == WX_RAIN ? -(t * 2) : ui_wave(t + i * 7, 60, 3))) % M.w;
        if (x < M.x) x += M.w;
        if (wx == WX_RAIN) {
            RG(x, y, 1, 3, PAL_RAMP(RAMP_SKY, 7));
        } else if (wx == WX_SNOW) {
            RG(x, y, 2, 2, PAL_WHITE);
        } else if (i < 16) {
            RG(x, y, 2, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
            RG(x + 1, y + 1, 1, 1, PAL_RAMP(RAMP_PASTEL_PINK, 5));
        }
    }
}

/* ====================================================================== */
/* entry point                                                              */
int fp_island_tint(void)
{
    return town_is_night() ? TINT_NIGHT : TINT_NONE;
}


void fp_island_draw_map(const FpLocDraw *locs, int n, int sel)
{
    int i, order[FP_LOC_MAX];
    fp_island_geometry();
    g_t = fp_t();
    g_tint = fp_island_tint();
    draw_sea();
    draw_land();
    draw_patches();
    draw_roads();
    for (i = 0; i < DOOMO_COUNTOF(decos); i++) draw_deco(&decos[i]);
    draw_walkers();
    /* buildings back to front */
    for (i = 0; i < n; i++) order[i] = i;
    for (i = 1; i < n; i++) {
        int j = i, v = order[i];
        while (j > 0 && locs[order[j - 1]].y > locs[v].y) { order[j] = order[j - 1]; j--; }
        order[j] = v;
    }
    for (i = 0; i < n; i++) {
        const FpLocDraw *l = &locs[order[i]];
        int b = 0;
        if (l->kind >= LOC_COUNT) continue;
        if (order[i] == sel) {
            int ph = g_t % 24;
            b = ph < 6 ? ph / 2 : ph < 12 ? (12 - ph) / 2 : 0;
        }
        shadow(l->x, l->y, 11);
        fp_island_building(l->kind, l->x, l->y - b, l->locked);
    }
    draw_weather(fp_island_weather());
}
