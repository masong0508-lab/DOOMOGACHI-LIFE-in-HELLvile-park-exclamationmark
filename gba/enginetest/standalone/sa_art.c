/* sa_art.c - NOCORE=1 stand-ins for the core's art (owner: gba).
 *
 * Only linked into gba/build/doomogachi_et_nocore.gba, the engine test
 * built against nothing but the stable core basics (palette, font,
 * fixmath). Procedural textures, sky and sprites are generated into EWRAM
 * at boot, so the renderer can be developed and judged while the art
 * module is still landing. The real game always uses core/gen/art_data.c.
 */
#include "doomo/doomo.h"

#define EWRAM_BSS_SA   /* plain .bss is EWRAM on the GBA */

/* generated tables the art module will provide; empty here (the engine
 * test detects the missing identity row and builds a fallback) */
const uint8_t doomo_art_colormap[DOOMO_LIGHT_LEVELS][256];
const uint8_t doomo_art_tintmap[TINT_COUNT][256];
const uint8_t doomo_art_darker[256];
const uint8_t doomo_art_lighter[256];

void doomo_palette_fallback(void);
static uint8_t nearest_fast(const uint8_t *pr, const uint8_t *pg, const uint8_t *pb,
                            int r, int g, int b)
{
    int i, best = 1;
    uint32_t bd = 0xFFFFFFFFu;
    for (i = 1; i < 256; i++) {
        int dr = r - pr[i], dg = g - pg[i], db = b - pb[i];
        uint32_t d = (uint32_t)(2 * dr * dr + 4 * dg * dg + 3 * db * db);
        if (d < bd) { bd = d; best = i; }
    }
    return (uint8_t)best;
}

void doomo_palette_fallback(void)
{
    static const uint8_t tint_rgb[TINT_COUNT][3] = {
        { 0, 0, 0 }, { 255, 40, 40 }, { 255, 220, 80 }, { 80, 255, 80 },
        { 180, 80, 255 }, { 40, 50, 140 }
    };
    static uint8_t pr[256], pg[256], pb[256];
    int lv, i;
    for (i = 0; i < 256; i++) palette_rgb((uint8_t)i, &pr[i], &pg[i], &pb[i]);
    for (i = 0; i < 256; i++) {
        int r = pr[i], g = pg[i], b = pb[i];
        for (lv = 0; lv < DOOMO_LIGHT_LEVELS; lv++) {
            if (i == 0) { doomo_colormap[lv][i] = 0; continue; }
            if (lv == DOOMO_LIGHT_LEVELS - 1) { doomo_colormap[lv][i] = (uint8_t)i; continue; }
            if (lv == 0) { doomo_colormap[lv][i] = PAL_BLACK; continue; }
            doomo_colormap[lv][i] = nearest_fast(pr, pg, pb, r * (lv + 1) / 16,
                                                 g * (lv + 1) / 16, b * (lv + 1) / 16);
        }
        for (lv = 0; lv < TINT_COUNT; lv++) {
            if (i == 0 || lv == TINT_NONE) { doomo_tintmap[lv][i] = (uint8_t)i; continue; }
            doomo_tintmap[lv][i] = nearest_fast(pr, pg, pb, (r + tint_rgb[lv][0]) / 2,
                                                (g + tint_rgb[lv][1]) / 2,
                                                (b + tint_rgb[lv][2]) / 2);
        }
    }
}

/* ------------------------------------------------------------------------ */
static uint32_t hash2(int x, int y, uint32_t seed)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* value noise 0..255, period 64 (tileable), cell size s */
static int vnoise(int x, int y, int s, uint32_t seed)
{
    int x0 = x / s, y0 = y / s, fx = x % s, fy = y % s, n = 64 / s;
    int a = (int)(hash2(x0 % n, y0 % n, seed) & 255);
    int b = (int)(hash2((x0 + 1) % n, y0 % n, seed) & 255);
    int c = (int)(hash2(x0 % n, (y0 + 1) % n, seed) & 255);
    int d = (int)(hash2((x0 + 1) % n, (y0 + 1) % n, seed) & 255);
    int ab = a + (b - a) * fx / s, cd = c + (d - c) * fx / s;
    return ab + (cd - ab) * fy / s;
}

static int fbm(int x, int y, uint32_t seed)
{
    return (vnoise(x, y, 16, seed) * 4 + vnoise(x, y, 8, seed + 1) * 2 +
            vnoise(x, y, 4, seed + 2)) / 7;
}

static uint8_t ramp(int r, int shade)
{
    if (shade < 0) shade = 0;
    if (shade > 7) shade = 7;
    return PAL_RAMP(r, shade);
}

#define NTEX 64
static EWRAM_BSS_SA uint8_t texbuf[24][64 * 64];
static int8_t tex_slot[NTEX];
static uint8_t tex_avg[NTEX];
static int ntex_used;
static EWRAM_BSS_SA uint8_t skybuf[SKY_W * SKY_H];

static void gen_bricks(uint8_t *p, int rm, int rmort, int bw, int bh, uint32_t seed)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int row = y / bh, off = (row & 1) ? bw / 2 : 0;
            int bx = (x + off) % bw, by = y % bh;
            int id = (x + off) / bw;
            int sh = 4 + (int)(hash2(id & 7, row, seed) % 3) - 1;
            int n = fbm(x, y, seed) - 128;
            if (bx == 0 || by == 0) { p[y * 64 + x] = ramp(rmort, 2 + (n > 40)); continue; }
            if (by == 1 || bx == 1) sh++;
            if (by == bh - 1) sh--;
            p[y * 64 + x] = ramp(rm, sh + n / 48);
        }
}

static void gen_planks(uint8_t *p, int rm, int vertical, uint32_t seed)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int u = vertical ? x : y, v = vertical ? y : x;
            int plank = u / 16, pu = u % 16;
            int grain = (vnoise(v, u * 4, 8, seed + (uint32_t)plank) - 128) / 40 +
                        ((int)((hash2(plank, v / 21, seed) & 3)) == 0 ? -1 : 0);
            int sh = 4 + grain;
            if (pu == 0) sh = 1;
            else if (pu == 1) sh += 1;
            else if (pu == 15) sh -= 1;
            if (((v + plank * 23) % 32) == 0 && pu > 1) sh = 2;
            p[y * 64 + x] = ramp(rm, sh);
        }
}

static void gen_tiles(uint8_t *p, int r1, int r2, int ts, uint32_t seed)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int tx = x % ts, ty = y % ts;
            int alt = ((x / ts) + (y / ts)) & 1;
            int n = (fbm(x, y, seed) - 128) / 50;
            int sh = 5 + n;
            if (tx == 0 || ty == 0) { p[y * 64 + x] = ramp(r1, 2); continue; }
            if (tx == 1 || ty == 1) sh = 7;
            if (tx == ts - 1 || ty == ts - 1) sh = 3;
            p[y * 64 + x] = ramp(alt ? r2 : r1, sh);
        }
}

static void gen_organic(uint8_t *p, int rm, int base, int contrast, uint32_t seed)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int n = fbm(x, y, seed) - 128;
            p[y * 64 + x] = ramp(rm, base + n * contrast / 128);
        }
}

static void gen_grass(uint8_t *p, uint32_t seed)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int n = fbm(x, y, seed) - 128;
            uint32_t h = hash2(x, y, seed + 9) & 63;
            int sh = 4 + n / 40;
            if (h < 5) sh += 2;
            else if (h < 9) sh -= 1;
            p[y * 64 + x] = ramp(RAMP_GRASS, sh);
            if ((hash2(x, y, seed + 77) & 255) == 3) p[y * 64 + x] = PAL_PAINT(PAINT_BUBBLEGUM, 2);
            if ((hash2(x, y, seed + 78) & 255) == 5) p[y * 64 + x] = PAL_PAINT(PAINT_LEMON, 2);
        }
}

static void gen_cobble(uint8_t *p, int rm, uint32_t seed)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            /* jittered grid voronoi, cells of 16 */
            int best = 1 << 30, second = 1 << 30, gx, gy, bid = 0;
            for (gy = -1; gy <= 1; gy++)
                for (gx = -1; gx <= 1; gx++) {
                    int cx = (x / 16 + gx), cy = (y / 16 + gy);
                    uint32_t h = hash2((cx + 4) & 3, (cy + 4) & 3, seed);
                    int px = cx * 16 + 3 + (int)(h % 10), py = cy * 16 + 3 + (int)((h >> 8) % 10);
                    int d = (x - px) * (x - px) + (y - py) * (y - py);
                    if (d < best) { second = best; best = d; bid = (int)(h >> 16); }
                    else if (d < second) second = d;
                }
            {
                int edge = second - best;
                int sh = 4 + (bid % 3) - 1 + (fbm(x, y, seed) - 128) / 64;
                if (edge < 12) sh = 1;
                else if (edge < 30) sh -= 1;
                else if (best < 20) sh += 1;
                p[y * 64 + x] = ramp(rm, sh);
            }
        }
}

static void gen_stripes(uint8_t *p, uint32_t seed)
{
    int x, y;
    (void)seed;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int d = (x + y) & 15;
            p[y * 64 + x] = d < 8 ? ramp(RAMP_PASTEL_PINK, d == 0 ? 3 : 5)
                                  : PAL_PAINT(PAINT_WHITE, d == 8 ? 0 : 1);
        }
}

static void gen_clouds(uint8_t *p, uint32_t seed)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int n = fbm(x, y, seed);
            p[y * 64 + x] = n > 150 ? PAL_PAINT(PAINT_WHITE, n > 175 ? 1 : 0)
                                    : ramp(RAMP_SKY, 5 + (n > 120));
        }
}

static void gen_roof(uint8_t *p, uint32_t seed)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int row = y / 8, off = (row & 1) * 4;
            int tx = (x + off) % 8, ty = y % 8;
            int dx = tx - 4, rr = dx * dx + (ty - 2) * (ty - 2) * 2;
            int sh = 5 - ty / 3 + (fbm(x, y, seed) - 128) / 64;
            if (rr > 24 || ty == 7) sh = 1;
            p[y * 64 + x] = ramp(RAMP_PASTEL_PINK, sh);
        }
}

static void gen_metal(uint8_t *p, int rm, int tech, uint32_t seed)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int tx = x % 32, ty = y % 32;
            int sh = 4 + (fbm(x, y, seed) - 128) / 80;
            if (tx == 0 || ty == 0) sh = 1;
            else if (tx == 1 || ty == 1) sh = 6;
            else if ((tx == 4 || tx == 28) && (ty == 4 || ty == 28)) sh = 7;
            if (tech && ty > 12 && ty < 20 && tx > 6 && tx < 26) {
                sh = 2;
                if (ty == 15 && (tx & 3) == 0) {
                    p[y * 64 + x] = (x / 32 + y / 32) & 1 ? PAL_PAINT(PAINT_MINT, 2) : PAL_UI_RED;
                    continue;
                }
            }
            p[y * 64 + x] = ramp(rm, sh);
        }
}

static void gen_pentagram(uint8_t *p, uint32_t seed)
{
    int x, y;
    gen_organic(p, RAMP_BLOOD, 2, 2, seed);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int dx = x - 32, dy = y - 32, r2 = dx * dx + dy * dy;
            if (r2 > 26 * 26 && r2 < 29 * 29) p[y * 64 + x] = ramp(RAMP_LAVA, 5);
        }
    {
        static const int8_t pts[5][2] = { { 0, -25 }, { 24, -8 }, { 15, 20 }, { -15, 20 }, { -24, -8 } };
        int i, t;
        for (i = 0; i < 5; i++) {
            const int8_t *a = pts[i], *b = pts[(i + 2) % 5];
            for (t = 0; t <= 64; t++) {
                int xx = 32 + a[0] + (b[0] - a[0]) * t / 64, yy = 32 + a[1] + (b[1] - a[1]) * t / 64;
                p[(yy & 63) * 64 + (xx & 63)] = ramp(RAMP_LAVA, 6);
            }
        }
    }
}

static void gen_door(uint8_t *p, uint32_t seed)
{
    int x, y;
    gen_planks(p, RAMP_WOOD, 1, seed);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            if (x < 3 || x > 60 || y < 3) p[y * 64 + x] = ramp(RAMP_WOOD, 1);
            {
                int dx = x - 32, dy = y - 26;
                int hx = dx < 0 ? -dx : dx;
                /* a little heart */
                if ((hx - 5) * (hx - 5) + (dy + 3) * (dy + 3) < 26 || (dy >= -2 && dy < 9 - hx && hx < 10))
                    p[y * 64 + x] = PAL_PAINT(PAINT_ROSE, dy < -3 ? 2 : 1);
            }
            if (x == 50 && y > 34 && y < 40) p[y * 64 + x] = PAL_UI_GOLD;
        }
}

static void make_tex(int id, uint8_t *p)
{
    uint32_t s = (uint32_t)id * 7919u;
    switch (id) {
    case TEX_PASTEL_BRICK: gen_bricks(p, RAMP_PASTEL_PINK, RAMP_STONE, 16, 8, s); break;
    case TEX_BLOOD_BRICK: gen_bricks(p, RAMP_BLOOD, RAMP_STONE, 16, 8, s); break;
    case TEX_CHURCH_WALL: gen_bricks(p, RAMP_STONE, RAMP_STONE, 32, 16, s); break;
    case TEX_WOOD_WALL: gen_planks(p, RAMP_WOOD, 1, s); break;
    case TEX_WOOD_FLOOR: gen_planks(p, RAMP_PEACH, 0, s); break;
    case TEX_TILE_FLOOR: gen_tiles(p, RAMP_MINT, RAMP_STONE, 16, s); break;
    case TEX_CARPET: gen_organic(p, RAMP_PASTEL_PINK, 4, 2, s); break;
    case TEX_GRASS: case TEX_FLOWERS: gen_grass(p, s); break;
    case TEX_PATH: case TEX_SAND: gen_organic(p, RAMP_PEACH, 5, 2, s); break;
    case TEX_COBBLE: gen_cobble(p, RAMP_STONE, s); break;
    case TEX_HEDGE: case TEX_TREE: gen_organic(p, RAMP_GRASS, 3, 4, s); break;
    case TEX_ROOF_TILES: gen_roof(p, s); break;
    case TEX_CANDY_STRIPE: gen_stripes(p, s); break;
    case TEX_CLOUD_PUFF: gen_clouds(p, s); break;
    case TEX_WATER: gen_organic(p, RAMP_SKY, 4, 3, s); break;
    case TEX_LAVA: gen_organic(p, RAMP_LAVA, 5, 4, s); break;
    case TEX_FLESH_WALL: gen_organic(p, RAMP_FLESH, 4, 5, s); break;
    case TEX_GORE_FLOOR: gen_organic(p, RAMP_BLOOD, 3, 4, s); break;
    case TEX_HELL_ROCK: gen_cobble(p, RAMP_BLOOD, s); break;
    case TEX_METAL_FLOOR: case TEX_GRATE: gen_metal(p, RAMP_STEEL, 0, s); break;
    case TEX_RUST_METAL: gen_metal(p, RAMP_WOOD, 0, s); break;
    case TEX_TECH_PANEL: gen_metal(p, RAMP_STEEL, 1, s); break;
    case TEX_PENTAGRAM_FLOOR: gen_pentagram(p, s); break;
    case TEX_DOOR_CUTE: case TEX_DOOR_HELL: gen_door(p, s); break;
    default: gen_tiles(p, RAMP_CANDY, RAMP_PASTEL_PINK, 32, s); break;
    }
}

const uint8_t *texture_pixels(int tex_id, uint16_t time)
{
    int slot;
    (void)time;
    if (tex_id < 0 || tex_id >= NTEX) tex_id = 0;
    slot = tex_slot[tex_id];
    if (slot <= 0) {
        if (ntex_used >= 24) return texbuf[0];
        slot = ++ntex_used;
        make_tex(tex_id, texbuf[slot - 1]);
        tex_slot[tex_id] = (int8_t)slot;
        {
            uint32_t r = 0, g = 0, b = 0;
            int i;
            for (i = 0; i < 4096; i += 7) {
                uint8_t cr, cg, cb;
                palette_rgb(texbuf[slot - 1][i], &cr, &cg, &cb);
                r += cr; g += cg; b += cb;
            }
            tex_avg[tex_id] = palette_nearest((int)(r / 586), (int)(g / 586), (int)(b / 586));
        }
    }
    {
        const uint8_t *p = texbuf[slot - 1];
        /* animated: scroll water and lava */
        if (tex_id == TEX_WATER || tex_id == TEX_LAVA) {
            (void)p;
        }
        return p;
    }
}

const DoomoImage *texture_image(int tex_id, uint16_t time)
{
    static DoomoImage im;
    im.w = 64; im.h = 64;
    im.pix = texture_pixels(tex_id, time);
    im.uid = 0x7E000000u + (uint32_t)tex_id;
    return &im;
}

int texture_is_animated(int tex_id) { (void)tex_id; return 0; }

uint8_t texture_avg(int tex_id)
{
    if (tex_id < 0 || tex_id >= NTEX) return PAL_GRAY;
    if (!tex_avg[tex_id]) texture_pixels(tex_id, 0);
    return tex_avg[tex_id];
}

const uint8_t *sky_pixels(int sky_id, uint16_t time)
{
    (void)sky_id; (void)time;
    return skybuf;
}

void textures_init(void)
{
    int x, y;
    for (y = 0; y < SKY_H; y++)
        for (x = 0; x < SKY_W; x++) {
            /* pastel day: blue at the top, pink at the horizon, clouds */
            int t = y * 8 / SKY_H;
            uint8_t c = y < 70 ? ramp(RAMP_SKY, 4 + t / 2) : ramp(RAMP_PASTEL_PINK, 6 + (y > 84));
            int n = (vnoise(x & 63, y & 63, 16, 5u + (uint32_t)(x >> 6)) +
                     vnoise(x & 63, y & 63, 8, 9u + (uint32_t)(x >> 6))) / 2;
            int band = y > 30 && y < 80;
            if (band && n > 150) c = PAL_PAINT(PAINT_WHITE, n > 168 ? 1 : 0);
            skybuf[y * SKY_W + x] = c;
        }
}

/* ------------------------------------------------------------------------ */
/* sprites: simple procedural shapes */
#define SW 32
#define SH 48
static EWRAM_BSS_SA uint8_t sprbuf[16][SW * SH];
static DoomoImage sprimg[16];
static int8_t spr_slot[SPR_COUNT];
static int nspr;

static void disc(uint8_t *p, int cx, int cy, int rx, int ry, uint8_t c, uint8_t hi)
{
    int x, y;
    for (y = cy - ry; y <= cy + ry; y++)
        for (x = cx - rx; x <= cx + rx; x++) {
            int dx = x - cx, dy = y - cy;
            if (x < 0 || y < 0 || x >= SW || y >= SH) continue;
            if (dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry) {
                int h = (dx * dx + (dy + ry / 2) * (dy + ry / 2)) < (rx * rx) / 4;
                p[y * SW + x] = h ? hi : c;
            }
        }
}

static void box(uint8_t *p, int x0, int y0, int x1, int y1, uint8_t c)
{
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (x >= 0 && y >= 0 && x < SW && y < SH) p[y * SW + x] = c;
}

static void make_spr(int id, uint8_t *p)
{
    memset(p, 0, SW * SH);
    switch (id) {
    case SPR_TREE:
        box(p, 13, 30, 19, 48, ramp(RAMP_WOOD, 3));
        disc(p, 16, 18, 15, 16, ramp(RAMP_GRASS, 4), ramp(RAMP_GRASS, 6));
        disc(p, 9, 22, 6, 6, ramp(RAMP_GRASS, 3), ramp(RAMP_GRASS, 5));
        break;
    case SPR_LAMP:
        box(p, 15, 10, 17, 48, ramp(RAMP_STEEL, 2));
        disc(p, 16, 7, 6, 6, PAL_UI_GOLD, PAL_PAINT(PAINT_LEMON, 2));
        break;
    case SPR_TORCH:
        box(p, 14, 26, 18, 48, ramp(RAMP_WOOD, 2));
        disc(p, 16, 16, 7, 12, ramp(RAMP_LAVA, 5), ramp(RAMP_LAVA, 7));
        break;
    case SPR_FOUNTAIN:
        disc(p, 16, 40, 15, 7, ramp(RAMP_STONE, 5), ramp(RAMP_STONE, 6));
        box(p, 14, 18, 18, 38, ramp(RAMP_STONE, 4));
        disc(p, 16, 14, 9, 6, ramp(RAMP_SKY, 6), PAL_WHITE);
        break;
    case SPR_FLOWER_BED: {
        int i;
        for (i = 0; i < 12; i++)
            disc(p, 3 + (i * 7) % 27, 38 + (i * 5) % 9, 2, 2,
                 i & 1 ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_PAINT(PAINT_LEMON, 1), PAL_WHITE);
        box(p, 0, 45, 32, 48, ramp(RAMP_GRASS, 3));
        break; }
    case SPR_BARREL:
        box(p, 6, 8, 26, 48, ramp(RAMP_STEEL, 3));
        box(p, 6, 14, 26, 16, ramp(RAMP_STEEL, 1));
        box(p, 6, 38, 26, 40, ramp(RAMP_STEEL, 1));
        box(p, 10, 8, 13, 48, ramp(RAMP_STEEL, 5));
        disc(p, 16, 8, 10, 3, ramp(RAMP_SICK, 5), ramp(RAMP_SICK, 7));
        break;
    case SPR_IMPLING: case SPR_HELLHOUND_PUP:
        disc(p, 16, 32, 12, 15, ramp(RAMP_BLOOD, 4), ramp(RAMP_BLOOD, 6));
        box(p, 6, 10, 9, 20, ramp(RAMP_BONE, 6));
        box(p, 23, 10, 26, 20, ramp(RAMP_BONE, 6));
        disc(p, 11, 28, 3, 3, PAL_UI_GOLD, PAL_WHITE);
        disc(p, 21, 28, 3, 3, PAL_UI_GOLD, PAL_WHITE);
        box(p, 12, 37, 20, 39, PAL_BLACK);
        break;
    case SPR_BRUTE:
        disc(p, 16, 26, 15, 21, ramp(RAMP_FLESH, 3), ramp(RAMP_FLESH, 5));
        disc(p, 10, 18, 3, 3, PAL_UI_RED, PAL_PAINT(PAINT_LEMON, 2));
        disc(p, 22, 18, 3, 3, PAL_UI_RED, PAL_PAINT(PAINT_LEMON, 2));
        box(p, 9, 30, 23, 33, ramp(RAMP_BONE, 7));
        break;
    case SPR_PUFF_DEMON: case SPR_VAPE_WRAITH:
        disc(p, 16, 24, 14, 18, ramp(RAMP_VAPE, 4), ramp(RAMP_VAPE, 6));
        disc(p, 11, 20, 3, 4, PAL_BLACK, PAL_UI_PURPLE);
        disc(p, 21, 20, 3, 4, PAL_BLACK, PAL_UI_PURPLE);
        break;
    case SPR_SKULL_PILE:
        disc(p, 10, 42, 6, 5, ramp(RAMP_BONE, 6), ramp(RAMP_BONE, 7));
        disc(p, 22, 42, 6, 5, ramp(RAMP_BONE, 5), ramp(RAMP_BONE, 7));
        disc(p, 16, 35, 6, 5, ramp(RAMP_BONE, 6), ramp(RAMP_BONE, 7));
        break;
    case SPR_PET:
        disc(p, 16, 30, 13, 16, PAL_UI_PINK, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        disc(p, 8, 12, 4, 6, PAL_UI_PINK, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        disc(p, 24, 12, 4, 6, PAL_UI_PINK, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        disc(p, 11, 27, 2, 3, PAL_BLACK, PAL_BLACK);
        disc(p, 21, 27, 2, 3, PAL_BLACK, PAL_BLACK);
        disc(p, 16, 34, 2, 1, PAL_UI_PINK_D, PAL_UI_PINK_D);
        break;
    default:
        disc(p, 16, 38, 8, 8, ramp(RAMP_CANDY, 5), ramp(RAMP_CANDY, 7));
        break;
    }
}

const DoomoImage *sprite_image(int spr_id, int frame)
{
    int s;
    (void)frame;
    if (spr_id < 0 || spr_id >= SPR_COUNT) spr_id = 0;
    s = spr_slot[spr_id];
    if (s <= 0) {
        if (nspr >= 16) return &sprimg[0];
        s = ++nspr;
        make_spr(spr_id, sprbuf[s - 1]);
        sprimg[s - 1].w = SW;
        sprimg[s - 1].h = SH;
        sprimg[s - 1].pix = sprbuf[s - 1];
        sprimg[s - 1].uid = 0x7F000000u + (uint32_t)spr_id;
        spr_slot[spr_id] = (int8_t)s;
    }
    return &sprimg[s - 1];
}

int sprite_frames(int spr_id) { (void)spr_id; return 1; }
void sprites_init(void) { }
