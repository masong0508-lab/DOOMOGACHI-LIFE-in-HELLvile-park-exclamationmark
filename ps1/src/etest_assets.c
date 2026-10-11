/* etest_assets.c - self-contained fallback assets for the engine test.
 *
 * The real game uses the core's generated art (core/gen/art_data.c). These
 * procedural stand-ins are only used when a core asset is missing (during
 * bring-up, or if the art module returns NULL): a 5x8 font, 64x64 textures
 * per TEX_* family, a sky and a few cute critter sprites. Generated once
 * into RAM on first use.
 */
#include <string.h>
#include "ps1.h"

#define abs(v) ((v) < 0 ? -(v) : (v))

/* 5x8 column-major font, bit 0 = top row, 0x20..0x9F (icons are simple). */
const uint8_t etest_font[128][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x56,0x20,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
    {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00},{0x10,0x08,0x08,0x10,0x08},{0x00,0x00,0x00,0x00,0x00},
    /* 0x80.. icons: heart skull star coin cross penta note arrows ... */
    {0x0C,0x1E,0x3C,0x1E,0x0C},{0x1E,0x35,0x1F,0x35,0x1E},{0x14,0x1C,0x7F,0x1C,0x14},
    {0x1C,0x3E,0x2A,0x3E,0x1C},{0x08,0x08,0x7F,0x08,0x08},{0x26,0x1C,0x63,0x1C,0x26},
    {0x20,0x70,0x3F,0x02,0x0C},{0x08,0x08,0x3E,0x1C,0x08},{0x08,0x1C,0x3E,0x08,0x08},
    {0x08,0x0C,0x7E,0x0C,0x08},{0x10,0x30,0x7E,0x30,0x10},{0x3E,0x55,0x55,0x5D,0x3E},
    {0x3E,0x41,0x7F,0x6B,0x3E},{0x3C,0x52,0x48,0x52,0x3C},{0x3C,0x48,0x52,0x48,0x3C},
    {0x30,0x5C,0x7E,0x1C,0x30},{0x18,0x3C,0x3E,0x3C,0x18},{0x08,0x5C,0x3E,0x1D,0x08},
    {0x1C,0x3E,0x3E,0x22,0x1C},{0x22,0x32,0x2A,0x26,0x22},{0x08,0x2A,0x1C,0x2A,0x08},
    {0x38,0x7C,0x7D,0x7E,0x38},{0x40,0x3E,0x0D,0x0D,0x02},{0x60,0x70,0x38,0x1D,0x0F},
    {0x7C,0x2A,0x7E,0x2A,0x7C},{0x3C,0x7E,0x7F,0x7E,0x3C},{0x7E,0x43,0x45,0x43,0x7E},
    {0x30,0x7C,0x7E,0x7C,0x30},{0x10,0x20,0x10,0x08,0x04},{0x22,0x14,0x08,0x14,0x22},
    {0x30,0x48,0x4A,0x36,0x0E},{0x06,0x09,0x79,0x09,0x06},
};

/* ---- fallback palette (only when the art module's palette is absent) ----- */
static uint16_t s_pal[256];

static uint16_t rgb555(int r, int g, int b)
{
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
}

const uint16_t *etest_palette(void)
{
    /* UI colours, then paints / skins / ramps as dark->light blends */
    static const uint8_t ui[16][3] = {
        {0,0,0},{0,0,0},{255,255,255},{204,200,216},{140,136,156},{76,72,92},
        {255,184,208},{208,80,136},{168,240,208},{48,160,128},{255,248,228},
        {232,40,48},{120,8,24},{255,208,64},{168,88,224},{176,200,120} };
    static const uint8_t paint[32][3] = {
        {238,240,250},{164,168,188},{64,64,84},{24,22,32},{224,40,72},{136,8,24},
        {232,96,140},{255,144,200},{255,168,144},{248,128,40},{152,68,32},{232,160,40},
        {255,224,64},{176,216,40},{88,208,72},{120,224,184},{32,104,64},{24,152,160},
        {72,216,248},{120,176,248},{48,88,208},{32,40,112},{184,160,240},{120,56,168},
        {168,88,224},{232,56,200},{112,64,40},{200,136,72},{232,224,200},{120,208,40},
        {255,96,24},{96,248,200} };
    static const uint8_t skin[8][3] = {
        {255,232,224},{248,212,188},{240,184,148},{216,152,108},{184,132,92},
        {148,96,64},{108,68,44},{72,44,32} };
    static const uint8_t ramp[15][2][3] = {
        {{40,36,60},{232,228,244}},{{56,24,24},{240,180,120}},{{16,56,56},{232,248,160}},
        {{96,24,56},{255,228,236}},{{16,20,56},{200,232,255}},{{40,16,64},{232,200,255}},
        {{96,56,16},{255,248,200}},{{8,56,56},{216,255,240}},{{64,32,24},{255,232,208}},
        {{32,0,4},{255,72,64}},{{64,8,0},{255,255,200}},{{56,16,40},{255,184,184}},
        {{64,44,24},{255,248,232}},{{24,40,8},{216,240,120}},{{24,28,40},{200,212,232}} };
    int i, k;
    if (s_pal[2])
        return s_pal;
    for (i = 0; i < 16; i++)
        s_pal[i] = rgb555(ui[i][0], ui[i][1], ui[i][2]);
    for (i = 0; i < 32; i++)
        for (k = 0; k < 3; k++) {
            int f = k == 0 ? 160 : k == 1 ? 256 : 330;
            int r = paint[i][0] * f / 256, g = paint[i][1] * f / 256, b = paint[i][2] * f / 256;
            if (k == 2) { r = (r + 255) / 2; g = (g + 255) / 2; b = (b + 255) / 2; }
            s_pal[PAL_PAINT(i, k)] = rgb555(r, g, b);
        }
    for (i = 0; i < 8; i++)
        for (k = 0; k < 3; k++) {
            int f = k == 0 ? 200 : k == 1 ? 256 : 290;
            s_pal[PAL_SKIN(i, k)] = rgb555(skin[i][0] * f / 256, skin[i][1] * f / 256,
                                           skin[i][2] * f / 256);
        }
    for (i = 0; i < 15; i++)
        for (k = 0; k < 8; k++) {
            const uint8_t *a = ramp[i][0], *b = ramp[i][1];
            s_pal[PAL_RAMP(i, k)] = rgb555(a[0] + (b[0] - a[0]) * k / 7,
                                           a[1] + (b[1] - a[1]) * k / 7,
                                           a[2] + (b[2] - a[2]) * k / 7);
        }
    return s_pal;
}

/* ---- tiny deterministic noise ------------------------------------------- */
static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
static int noise(int x, int y, int seed) { return (int)(hash32((uint32_t)(x * 73 + y * 9187 + seed * 100003)) & 255); }

static int smooth(int x, int y, int cell, int seed)
{
    /* value noise, wraps at 64 */
    int x0 = x / cell, y0 = y / cell, fx = x % cell, fy = y % cell;
    int n = 64 / cell;
    int a = noise(x0 % n, y0 % n, seed), b = noise((x0 + 1) % n, y0 % n, seed);
    int c = noise(x0 % n, (y0 + 1) % n, seed), d = noise((x0 + 1) % n, (y0 + 1) % n, seed);
    int ab = a + (b - a) * fx / cell, cd = c + (d - c) * fx / cell;
    return ab + (cd - ab) * fy / cell;
}

static uint8_t ramp(int r, int shade)
{
    if (shade < 0) shade = 0;
    if (shade > 7) shade = 7;
    return PAL_RAMP(r, shade);
}

/* ---- textures ------------------------------------------------------------- */
enum { FAM_BRICK, FAM_STONE, FAM_WOOD, FAM_GRASS, FAM_TILE, FAM_TECH, FAM_LIQUID,
       FAM_FLESH, FAM_DOOR, FAM_CLOUD, FAM_CANDY, FAM_SIGN };

static void tex_family(int id, int *fam, int *rmp, int *rmp2)
{
    *rmp2 = RAMP_STONE;
    switch (id) {
    case TEX_PASTEL_BRICK: *fam = FAM_BRICK; *rmp = RAMP_PASTEL_PINK; *rmp2 = RAMP_PEACH; break;
    case TEX_BLOOD_BRICK:  *fam = FAM_BRICK; *rmp = RAMP_BLOOD; *rmp2 = RAMP_BONE; break;
    case TEX_CHURCH_WALL:  *fam = FAM_BRICK; *rmp = RAMP_BONE; *rmp2 = RAMP_STONE; break;
    case TEX_COBBLE: case TEX_PATH: *fam = FAM_STONE; *rmp = RAMP_STONE; break;
    case TEX_HELL_ROCK: case TEX_SKULL_MOSAIC: *fam = FAM_STONE; *rmp = RAMP_WOOD; break;
    case TEX_SAND:         *fam = FAM_STONE; *rmp = RAMP_PEACH; break;
    case TEX_WOOD_FLOOR: case TEX_WOOD_WALL: case TEX_PICKET_FENCE:
                           *fam = FAM_WOOD; *rmp = RAMP_WOOD; break;
    case TEX_GRASS: case TEX_HEDGE: case TEX_TREE: *fam = FAM_GRASS; *rmp = RAMP_GRASS; break;
    case TEX_FLOWERS:      *fam = FAM_GRASS; *rmp = RAMP_GRASS; *rmp2 = RAMP_PASTEL_PINK; break;
    case TEX_INFECTED_GRASS: *fam = FAM_GRASS; *rmp = RAMP_SICK; *rmp2 = RAMP_VAPE; break;
    case TEX_TILE_FLOOR: case TEX_CARPET: case TEX_PLAYGROUND:
                           *fam = FAM_TILE; *rmp = RAMP_MINT; *rmp2 = RAMP_PASTEL_PINK; break;
    case TEX_METAL_FLOOR: case TEX_GRATE: case TEX_PENTAGRAM_FLOOR:
                           *fam = FAM_TILE; *rmp = RAMP_STEEL; *rmp2 = RAMP_BLOOD; break;
    case TEX_TECH_PANEL: case TEX_SWITCH: case TEX_CHAINS:
                           *fam = FAM_TECH; *rmp = RAMP_STEEL; *rmp2 = RAMP_LAVA; break;
    case TEX_RUST_METAL:   *fam = FAM_TECH; *rmp = RAMP_WOOD; *rmp2 = RAMP_CANDY; break;
    case TEX_WATER:        *fam = FAM_LIQUID; *rmp = RAMP_SKY; break;
    case TEX_LAVA:         *fam = FAM_LIQUID; *rmp = RAMP_LAVA; break;
    case TEX_SLIME:        *fam = FAM_LIQUID; *rmp = RAMP_SICK; break;
    case TEX_FLESH_WALL: case TEX_GORE_FLOOR: case TEX_DEMON_EYES:
                           *fam = FAM_FLESH; *rmp = RAMP_FLESH; *rmp2 = RAMP_BLOOD; break;
    case TEX_BONE_WALL:    *fam = FAM_FLESH; *rmp = RAMP_BONE; *rmp2 = RAMP_BONE; break;
    case TEX_DOOR_CUTE:    *fam = FAM_DOOR; *rmp = RAMP_CANDY; *rmp2 = RAMP_PASTEL_PINK; break;
    case TEX_DOOR_HELL:    *fam = FAM_DOOR; *rmp = RAMP_STEEL; *rmp2 = RAMP_BLOOD; break;
    case TEX_CLOUD_PUFF: case TEX_VAPE_CLOUD_WALL:
                           *fam = FAM_CLOUD; *rmp = id == TEX_CLOUD_PUFF ? RAMP_SKY : RAMP_VAPE; break;
    case TEX_CANDY_STRIPE: case TEX_WALLPAPER_HEARTS:
                           *fam = FAM_CANDY; *rmp = RAMP_PASTEL_PINK; *rmp2 = RAMP_MINT; break;
    case TEX_EXIT_SIGN:    *fam = FAM_SIGN; *rmp = RAMP_STEEL; *rmp2 = RAMP_SICK; break;
    default:               *fam = FAM_BRICK; *rmp = RAMP_PEACH; *rmp2 = RAMP_STONE; break;
    }
}

static void gen_texture(uint8_t *t, int id, int frame)
{
    int fam, r, r2, x, y;
    tex_family(id, &fam, &r, &r2);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            int n = smooth(x, y, 8, id) / 64 + (noise(x, y, id) >> 6);
            int s = 3;
            uint8_t c;
            switch (fam) {
            case FAM_BRICK: {
                int row = y / 16, off = (row & 1) * 16;
                int bx = (x + off) % 32, by = y % 16;
                if (by == 0 || bx == 0) c = ramp(r2, 1);
                else {
                    s = 2 + (noise((x + off) / 32, row, id) & 3) + (by == 1 || bx == 1 ? 1 : 0)
                        - (by == 15 ? 1 : 0) + (noise(x, y, id) > 230);
                    c = ramp(r, s);
                }
                break; }
            case FAM_STONE: {
                int v = smooth(x, y, 16, id + 7);
                int cr = (v / 32) & 1;
                s = 2 + n / 2 + cr;
                if (((x ^ y) & 15) == 0 && noise(x, y, 3) > 200) s = 1;
                c = ramp(r, s);
                break; }
            case FAM_WOOD: {
                int plank = x / 16, gx = x % 16;
                int grain = (smooth(gx * 4, y, 16, plank + id) >> 5);
                s = 2 + grain / 2 + (plank & 1);
                if (gx == 0) s = 1;
                if ((y + plank * 23) % 64 == 0) s = 1;
                c = ramp(r, s);
                break; }
            case FAM_GRASS:
                s = 2 + (smooth(x, y, 8, id) >> 6) + (noise(x, y, id) > 220);
                c = ramp(r, s);
                if (r2 != RAMP_STONE && noise(x / 2, y / 2, id + 5) > 246) c = ramp(r2, 6);
                break;
            case FAM_TILE: {
                int tx = x / 16, ty = y / 16, gx = x % 16, gy = y % 16;
                if (gx == 0 || gy == 0) c = ramp(r, 1);
                else {
                    s = ((tx + ty) & 1) ? 5 : 3;
                    if (gx == 1 || gy == 1) s++;
                    s += noise(x, y, id) > 240;
                    c = ramp(((tx + ty) & 1) && r2 != RAMP_BLOOD ? r2 : r, s);
                }
                break; }
            case FAM_TECH: {
                int px = x % 32, py = y % 32;
                s = 3 + (n > 2);
                if (px == 0 || py == 0) s = 1;
                else if (px == 1 || py == 1) s = 5;
                if ((px == 4 || px == 27) && (py == 4 || py == 27)) s = 6;
                c = ramp(r, s);
                if (py > 12 && py < 18 && px > 8 && px < 24) c = ramp(r2, 4 + ((x + frame) & 1));
                break; }
            case FAM_LIQUID: {
                int ph = frame * 16;
                int w = smooth((x + ph) & 63, (y + (ph >> 1)) & 63, 16, id) +
                        smooth((x - ph / 2) & 63, (y + ph) & 63, 8, id + 1);
                s = 2 + w / 100;
                c = ramp(r, s);
                break; }
            case FAM_FLESH: {
                int v = smooth(x, y, 8, id) + smooth(x, y, 16, id + 3);
                int vein = (v > 250 && v < 270) || (v > 380 && v < 392);
                s = 2 + v / 120;
                c = vein ? ramp(r2, 3) : ramp(r, s);
                break; }
            case FAM_DOOR: {
                int gx = x % 32;
                s = 3 + (gx == 1) * 2 - (gx == 0 || gx == 31) * 2;
                if (y < 4 || y > 59) s = 1;
                c = ramp(r, s + (n > 2));
                if (y > 28 && y < 36 && x > 20 && x < 44) c = ramp(r2, 5);
                break; }
            case FAM_CLOUD:
                s = 3 + (smooth(x, y, 16, id) >> 6) + (smooth(x, y, 8, id + 1) >> 7);
                c = ramp(r, s + 1);
                break;
            case FAM_CANDY:
                s = ((x + y) / 8) & 1;
                c = s ? ramp(r, 5) : ramp(r2, 6);
                if (noise(x / 4, y / 4, id) > 245) c = PAL_WHITE;
                break;
            default: /* FAM_SIGN */
                c = ramp(r, 1);
                if (y > 16 && y < 48 && x > 4 && x < 60) c = ramp(r2, 5 + ((x / 4 + y / 4) & 1));
                break;
            }
            t[y * 64 + x] = c;
        }
}

/* Generated fallback textures live in a small LRU cache (the core's art
 * module normally provides every texture, so this is only exercised in
 * bring-up builds without the core, or for an id the art module lacks). */
#define ANIM_FRAMES 4
#ifdef DOOMO_HAVE_CORE
#define FB_SLOTS 4
#else
#define FB_SLOTS 60      /* bring-up build: every texture + animation frames */
#endif
static uint8_t  s_fbtex[FB_SLOTS][4096];
static uint16_t s_fbkey[FB_SLOTS];          /* id * 8 + frame + 1, 0 = free */
static uint32_t s_fbuse[FB_SLOTS], s_fbclock;

static int anim_index(int id)
{
    switch (id) {
    case TEX_WATER: case TEX_LAVA: case TEX_SLIME: case TEX_TECH_PANEL: return 1;
    default: return 0;
    }
}

const uint8_t *etest_texture(int tex_id, uint16_t time)
{
    int frame = 0, i, best = 0;
    uint16_t key;
    if (tex_id <= 0 || tex_id >= TEX_COUNT)
        tex_id = TEX_PASTEL_BRICK;
    if (anim_index(tex_id))
        frame = (time >> 3) % ANIM_FRAMES;
    key = (uint16_t)(tex_id * 8 + frame + 1);
    s_fbclock++;
    for (i = 0; i < FB_SLOTS; i++) {
        if (s_fbkey[i] == key) {
            s_fbuse[i] = s_fbclock;
            return s_fbtex[i];
        }
        if (s_fbuse[i] < s_fbuse[best])
            best = i;
    }
    gen_texture(s_fbtex[best], tex_id, frame);
    s_fbkey[best] = key;
    s_fbuse[best] = s_fbclock;
    return s_fbtex[best];
}

/* ---- sky ---------------------------------------------------------------------- */
#ifndef DOOMO_HAVE_CORE
static uint8_t s_sky[SKY_W * SKY_H];
static int s_sky_done;
#endif

const uint8_t *etest_sky(uint16_t time)
{
#ifdef DOOMO_HAVE_CORE
    (void)time;
    return NULL;            /* the art module provides the skies */
#else
    int x, y;
    (void)time;
    if (s_sky_done)
        return s_sky;
    for (y = 0; y < SKY_H; y++)
        for (x = 0; x < SKY_W; x++) {
            /* pastel gradient with fluffy clouds and a far hill line */
            int s = 7 - (SKY_H - 1 - y) * 4 / SKY_H;
            uint8_t c = ramp(RAMP_SKY, s);
            int cl = smooth(x & 63, (y * 2) & 63, 16, 11 + x / 64) + (smooth(x & 63, y & 63, 8, 3) >> 1);
            int hill = SKY_H - 10 - ((smooth(x & 63, 0, 32, 5 + (x >> 6)) * 14) >> 8);
            if (y < SKY_H - 30 && cl > 300)
                c = cl > 340 ? PAL_WHITE : ramp(RAMP_PASTEL_PINK, 7);
            if (y >= hill)
                c = ramp(RAMP_GRASS, 3 + ((y - hill) < 2));
            s_sky[y * SKY_W + x] = c;
        }
    s_sky_done = 1;
    return s_sky;
#endif
}

/* ---- critter sprites -------------------------------------------------------- */
#define CR_W 32
#define CR_H 40
static uint8_t s_cr_pix[3][2][CR_W * CR_H];
static DoomoImage s_cr_img[3][2];
static int s_cr_done;

static void gen_critter(uint8_t *p, int kind, int frame)
{
    int x, y;
    int body = kind == 0 ? RAMP_PASTEL_PINK : kind == 1 ? RAMP_VAPE : RAMP_LAVA;
    memset(p, 0, CR_W * CR_H);
    for (y = 0; y < CR_H; y++)
        for (x = 0; x < CR_W; x++) {
            int dx = x * 2 - 31, dy = y * 2 - 44 + (frame ? 2 : 0);
            int d2 = dx * dx * 100 / 256 + dy * dy * 100 / 300;
            uint8_t c = 0;
            if (d2 < 100) {
                int sh = 6 - (dx + dy + 40) / 22;
                c = ramp(body, sh < 2 ? 2 : sh);
                if (d2 > 82) c = ramp(body, 1);
            }
            /* eyes */
            if ((abs(dx + 9) < 4 || abs(dx - 9) < 4) && abs(dy + 8) < 6)
                c = PAL_BLACK;
            if ((abs(dx + 10) < 2 || abs(dx - 8) < 2) && abs(dy + 10) < 2)
                c = PAL_WHITE;
            /* blush + mouth */
            if ((abs(dx + 17) < 4 || abs(dx - 17) < 4) && abs(dy - 2) < 3 && c)
                c = PAL_UI_PINK;
            if (abs(dx) < 4 && dy > 2 && dy < 6)
                c = kind == 2 ? PAL_WHITE : PAL_UI_PINK_D;
            /* feet / horns */
            if (y >= CR_H - 4 && (abs(x - 10) < 4 || abs(x - 22) < 4) && (frame || y < CR_H - 1))
                c = ramp(body, 1);
            if (kind && y < 8 && (abs(x - 9 + y / 3) < 2 || abs(x - 22 - y / 3) < 2))
                c = kind == 2 ? PAL_BLACK : PAL_UI_PURPLE;
            p[y * CR_W + x] = c;
        }
}

const DoomoImage *etest_sprite(int which, int frame)
{
    int k;
    if (!s_cr_done) {
        int f;
        for (k = 0; k < 3; k++)
            for (f = 0; f < 2; f++) {
                gen_critter(s_cr_pix[k][f], k, f);
                s_cr_img[k][f].w = CR_W;
                s_cr_img[k][f].h = CR_H;
                s_cr_img[k][f].pix = s_cr_pix[k][f];
                s_cr_img[k][f].uid = 0xE7000000u + (uint32_t)(k * 2 + f);
            }
        s_cr_done = 1;
    }
    k = which % 3;
    if (k < 0) k = -k;
    return &s_cr_img[k][frame & 1];
}
