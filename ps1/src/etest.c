/* etest.c - ENGINE TEST mode (build with -DDOOMO_ENGINE_TEST=ON).
 *
 * Boots straight into an automatic, deterministic walk-through of a
 * showcase map that exercises everything the GTE engine does: stairs,
 * ledges, a balcony, pillars, low ceilings, a door, a lift, a lava pit with
 * stepping stones, a water pond in an open-sky courtyard, look up/down,
 * billboards with shadows, first person, third person (3D player model) and
 * the 3D overworld view. The core's own maps are used when the action /
 * story modules provide them (MAPID_VAPE_CELLAR, MAPID_CLOUD_NINE_SPIRE,
 * MAPID_HAHALAND), otherwise a built-in map. A HUD shows fps, CPU time,
 * polygon and cell counts; the same numbers go to the TTY every second
 * ("perf: ...") for the harness.
 *
 * Pad: any button switches to manual control. D-pad move/turn, L1/R1
 * strafe, L2/R2 look down/up, SELECT first/third person, START next
 * scene (FP map -> overworld -> next map), TRIANGLE toggles the HUD.
 */
#include <string.h>
#include <stdio.h>
#include <psxgpu.h>
#include <psxgte.h>
#include "r3d.h"
#include "world.h"

#define C64 DOOMO_CELL_UNITS
#define FX(v) ((int32_t)((v) * 65536))

static DoomoMap s_et;                 /* the built-in showcase map */

/* ---- the built-in showcase map --------------------------------------------- */
static void fill(DoomoMap *m, int x0, int y0, int x1, int y1, int wall, int ftex,
                 int ctex, int fh, int ch, int light, int flags)
{
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            DoomoCell *c = &m->cells[y * m->w + x];
            c->wall = (uint8_t)wall;
            c->floor_tex = (uint8_t)ftex;
            c->ceil_tex = (uint8_t)ctex;
            c->floor_h = (int8_t)fh;
            c->ceil_h = (int8_t)ch;
            c->light = (uint8_t)light;
            c->flags = (uint8_t)flags;
            c->tag = 0;
        }
}

static void paint_walls(DoomoMap *m, int x0, int y0, int x1, int y1, int tex)
{
    int x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            DoomoCell *c = &m->cells[y * m->w + x];
            if (c->wall)
                c->wall = (uint8_t)tex;
        }
}

void etest_build_map(DoomoMap *m)
{
    int i;
    memset(m, 0, sizeof(*m));
    m->w = 32;
    m->h = 24;
    m->kind = MAP_DUNGEON;
    m->sky = SKY_PASTEL_DAY;
    m->ambient = 10;
    fill(m, 0, 0, 31, 23, TEX_PASTEL_BRICK, TEX_WOOD_FLOOR, TEX_TILE_FLOOR, 0, 16, 12, 0);
    /* A: the cute hall with pillars, stairs and a balcony */
    fill(m, 1, 1, 10, 14, 0, TEX_WOOD_FLOOR, TEX_CANDY_STRIPE, 0, 16, 13, 0);
    fill(m, 1, 1, 10, 3, 0, TEX_CARPET, TEX_CANDY_STRIPE, 0, 16, 14, 0);
    m->cells[3 * 32 + 3].wall = TEX_CANDY_STRIPE;
    m->cells[3 * 32 + 8].wall = TEX_CANDY_STRIPE;
    m->cells[6 * 32 + 3].wall = TEX_CANDY_STRIPE;
    m->cells[6 * 32 + 8].wall = TEX_CANDY_STRIPE;
    for (i = 0; i < 5; i++)
        fill(m, 2 + i, 10, 2 + i, 13, 0, TEX_WOOD_FLOOR, TEX_CANDY_STRIPE, 1 + i, 16, 13, 0);
    fill(m, 7, 10, 10, 13, 0, TEX_CARPET, TEX_CANDY_STRIPE, 6, 16, 15, 0);
    /* doorway with a lintel into the courtyard */
    fill(m, 11, 5, 11, 6, 0, TEX_COBBLE, TEX_WOOD_WALL, 0, 11, 13, 0);
    /* B: open-sky courtyard with a pond and a flower terrace */
    fill(m, 12, 1, 25, 9, 0, TEX_GRASS, DOOMO_TEX_SKY, 0, 30, 15, 0);
    fill(m, 15, 3, 20, 5, 0, TEX_WATER, DOOMO_TEX_SKY, -2, 30, 15, CELL_WATER);
    fill(m, 14, 7, 23, 8, 0, TEX_FLOWERS, DOOMO_TEX_SKY, 3, 30, 15, 0);
    fill(m, 13, 1, 15, 1, 0, TEX_PATH, DOOMO_TEX_SKY, 0, 30, 15, 0);
    /* C: low corridor south */
    fill(m, 17, 9, 18, 12, 0, TEX_COBBLE, TEX_WOOD_WALL, 0, 8, 11, 0);
    /* E: hell closet behind a door */
    fill(m, 26, 4, 26, 5, 0, TEX_METAL_FLOOR, TEX_DOOR_HELL, 0, 0, 10, CELL_DOOR);
    fill(m, 27, 1, 30, 11, 0, TEX_GORE_FLOOR, TEX_SKULL_MOSAIC, 0, 14, 8, 0);
    /* D: hell cavern with a lava pit, stepping stones, a lift and a ledge */
    fill(m, 12, 13, 30, 22, 0, TEX_HELL_ROCK, TEX_RUST_METAL, 0, 20, 9, 0);
    fill(m, 16, 16, 26, 20, 0, TEX_LAVA, TEX_RUST_METAL, -4, 20, 12, CELL_LAVA);
    fill(m, 18, 18, 18, 18, 0, TEX_METAL_FLOOR, TEX_RUST_METAL, 0, 20, 10, 0);
    fill(m, 21, 17, 21, 17, 0, TEX_METAL_FLOOR, TEX_RUST_METAL, 0, 20, 10, 0);
    fill(m, 24, 19, 24, 19, 0, TEX_METAL_FLOOR, TEX_RUST_METAL, 0, 20, 10, 0);
    fill(m, 27, 18, 30, 22, 0, TEX_GRATE, TEX_RUST_METAL, 6, 20, 10, 0);
    fill(m, 28, 14, 28, 14, 0, TEX_PENTAGRAM_FLOOR, TEX_RUST_METAL, 0, 20, 11, CELL_LIFT);
    fill(m, 12, 19, 14, 22, 0, TEX_METAL_FLOOR, TEX_TECH_PANEL, 0, 9, 8, 0);
    m->cells[15 * 32 + 14].wall = TEX_TECH_PANEL;
    m->cells[15 * 32 + 23].wall = TEX_TECH_PANEL;
    m->cells[21 * 32 + 20].wall = TEX_CHAINS;
    /* corridor from the hall into the cavern */
    fill(m, 9, 15, 11, 16, 0, TEX_COBBLE, TEX_WOOD_WALL, 0, 10, 10, 0);
    fill(m, 9, 14, 9, 14, 0, TEX_COBBLE, TEX_WOOD_WALL, 0, 10, 11, 0);
    /* wall paint per area */
    paint_walls(m, 0, 0, 11, 15, TEX_WALLPAPER_HEARTS);
    paint_walls(m, 11, 0, 26, 10, TEX_HEDGE);
    paint_walls(m, 26, 0, 31, 12, TEX_BLOOD_BRICK);
    paint_walls(m, 11, 12, 31, 23, TEX_FLESH_WALL);
    paint_walls(m, 0, 15, 11, 23, TEX_TECH_PANEL);
    m->cells[15 * 32 + 14].wall = TEX_TECH_PANEL;
    m->cells[15 * 32 + 23].wall = TEX_TECH_PANEL;
    m->cells[21 * 32 + 20].wall = TEX_SKULL_MOSAIC;
    m->cells[0 * 32 + 19].wall = TEX_SHOP_FRONT;
}

/* ---- the built-in cutesy town (overworld test) ------------------------------ */
static DoomoMap s_town;

static void house(DoomoMap *m, int x0, int y0, int x1, int y1, int wall, int roof, int h)
{
    fill(m, x0, y0, x1, y1, wall, roof, DOOMO_TEX_SKY, 0, h, 12, 0);
}

static void build_town(DoomoMap *m)
{
    int x, y;
    memset(m, 0, sizeof(*m));
    m->w = 40;
    m->h = 32;
    m->kind = MAP_OVERWORLD;
    m->sky = SKY_PASTEL_DAY;
    m->ambient = 13;
    fill(m, 0, 0, 39, 31, 0, TEX_GRASS, DOOMO_TEX_SKY, 0, 0, 13, 0);
    /* flower meadows */
    for (y = 0; y < 32; y++)
        for (x = 0; x < 40; x++)
            if (((x * 7 + y * 13) % 11) == 0)
                m->cells[y * 40 + x].floor_tex = TEX_FLOWERS;
    /* paths and the plaza */
    fill(m, 0, 15, 39, 16, 0, TEX_PATH, DOOMO_TEX_SKY, 0, 0, 13, 0);
    fill(m, 19, 0, 20, 31, 0, TEX_PATH, DOOMO_TEX_SKY, 0, 0, 13, 0);
    fill(m, 16, 12, 23, 19, 0, TEX_COBBLE, DOOMO_TEX_SKY, 0, 0, 14, 0);
    /* cottages, shop, cafe, church (blocks with roofs) */
    house(m, 10, 9, 13, 11, TEX_PASTEL_BRICK, TEX_ROOF_TILES, 7);
    house(m, 25, 9, 28, 11, TEX_WOOD_WALL, TEX_ROOF_TILES, 6);
    house(m, 11, 19, 14, 21, TEX_SHOP_FRONT, TEX_ROOF_TILES, 8);
    house(m, 25, 19, 29, 21, TEX_CAFE_FRONT, TEX_CANDY_STRIPE, 7);
    house(m, 17, 3, 22, 6, TEX_CHURCH_WALL, TEX_ROOF_TILES, 12);
    house(m, 31, 4, 35, 7, TEX_APARTMENT_FRONT, TEX_ROOF_TILES, 14);
    /* hedges and fences */
    fill(m, 8, 13, 14, 13, TEX_HEDGE, TEX_HEDGE, DOOMO_TEX_SKY, 0, 3, 13, 0);
    fill(m, 25, 13, 31, 13, TEX_PICKET_FENCE, 0, DOOMO_TEX_SKY, 0, 2, 13, 0);
    fill(m, 8, 18, 8, 23, TEX_HEDGE, TEX_HEDGE, DOOMO_TEX_SKY, 0, 3, 13, 0);
    /* a pond and a stepped flower hill */
    fill(m, 30, 23, 35, 27, 0, TEX_WATER, DOOMO_TEX_SKY, -2, 0, 13, CELL_WATER);
    fill(m, 2, 2, 8, 7, 0, TEX_GRASS, DOOMO_TEX_SKY, 2, 0, 13, 0);
    fill(m, 3, 3, 7, 6, 0, TEX_FLOWERS, DOOMO_TEX_SKY, 4, 0, 13, 0);
    fill(m, 4, 4, 6, 5, 0, TEX_GRASS, DOOMO_TEX_SKY, 6, 0, 13, 0);
    /* street lamps glow at night */
    m->cells[14 * 40 + 18].light = 15;
    m->cells[17 * 40 + 21].light = 15;
    m->cells[17 * 40 + 15].light = 15;
}

/* ---- scene setup -------------------------------------------------------------- */
typedef struct Key {
    int16_t t;              /* frame */
    int16_t x, y;           /* world units */
    int16_t ang;            /* 0..4095 */
    int16_t pitch;          /* -256..256 */
    uint8_t cam;            /* 0 FP, 1 TP */
} Key;

/* walk-through of the built-in map (30 frames = 1 s) */
static const Key k_tour[] = {
    {   0,  2 * 64 + 32, 1 * 64 + 40,  700,    0, 0 },
    {  60,  4 * 64 + 10, 4 * 64 + 40,  900,  150, 0 },   /* look up at the ceiling */
    { 110,  5 * 64 + 10, 7 * 64 + 40, 1100, -160, 0 },   /* look down */
    { 170,  3 * 64 + 10, 11 * 64 + 32,   20,    0, 0 },  /* to the stairs */
    { 240,  6 * 64 + 40, 11 * 64 + 32,    0,  -40, 0 },  /* climb */
    { 300,  9 * 64 + 10, 11 * 64 + 32, 3400,  -60, 0 },  /* balcony view */
    { 360, 10 * 64 + 20, 6 * 64 + 0,     0,    0, 0 },
    { 420, 14 * 64 + 0,  5 * 64 + 60,  3800,  200, 0 },  /* courtyard, sky */
    { 480, 17 * 64 + 0,  2 * 64 + 20,  1024,  -60, 1 },  /* third person */
    { 560, 22 * 64 + 0,  6 * 64 + 0,   1600,  -40, 1 },
    { 620, 18 * 64 + 0,  9 * 64 + 40,  1024,    0, 1 },
    { 680, 17 * 64 + 40, 13 * 64 + 40, 1024,  -90, 0 },  /* into the cavern */
    { 760, 15 * 64 + 0,  15 * 64 + 20,  600, -170, 0 },  /* lava pit */
    { 840, 25 * 64 + 0,  14 * 64 + 20, 1300, -100, 1 },
    { 920, 28 * 64 + 32, 16 * 64 + 0,  2600,  -30, 1 },
    { 990, 28 * 64 + 32, 16 * 64 + 0,  3000,  -30, 1 },
};
#define TOUR_LEN ((int)(sizeof(k_tour) / sizeof(k_tour[0])))
#define TOUR_END 990
#define OW_FRAMES 450

static int lerpi(int a, int b, int t, int n) { return a + (b - a) * t / n; }
static int lerpa(int a, int b, int t, int n)
{
    int d = ((b - a + 2048) & 4095) - 2048;
    return (a + d * t / n) & 4095;
}

static void tour_at(int f, Key *o)
{
    int i;
    if (f <= 0) { *o = k_tour[0]; return; }
    for (i = 0; i + 1 < TOUR_LEN; i++) {
        const Key *a = &k_tour[i], *b = &k_tour[i + 1];
        if (f >= a->t && f < b->t) {
            int t = f - a->t, n = b->t - a->t;
            /* smoothstep */
            int s = (t * t * (3 * n - 2 * t)) / (n * n);
            o->x = (int16_t)lerpi(a->x, b->x, s, n);
            o->y = (int16_t)lerpi(a->y, b->y, s, n);
            o->ang = (int16_t)lerpa(a->ang, b->ang, s, n);
            o->pitch = (int16_t)lerpi(a->pitch, b->pitch, s, n);
            o->cam = a->cam;
            return;
        }
    }
    *o = k_tour[TOUR_LEN - 1];
}

/* billboards placed around the map */
#define MAX_BB 48
static DoomoBillboard s_bb[MAX_BB];
static int s_nbb;

static const DoomoImage *spr(int id, int frame, int fallback)
{
    const DoomoImage *img = core_sprite_image(id, frame);
    return img ? img : etest_sprite(fallback, frame);
}

static void add_bb(int x, int y, int z, const DoomoImage *img, int scale, int flags, int light)
{
    DoomoBillboard *b;
    if (s_nbb >= MAX_BB || !img)
        return;
    b = &s_bb[s_nbb++];
    b->x = FX(x); b->y = FX(y); b->z = FX(z);
    b->img = img;
    b->scale = (uint16_t)scale;
    b->flags = (uint8_t)flags;
    b->light = (uint8_t)light;
}

static int floor_at(const DoomoMap *m, int x, int y)
{
    int cx = x >> 6, cy = y >> 6;
    if (cx < 0 || cy < 0 || cx >= m->w || cy >= m->h)
        return 0;
    return core_floor_z(m, cx, cy);
}

static void place_things(const DoomoMap *m, int t)
{
    int i;
    s_nbb = 0;
    if (m == &s_et) {
        static const int16_t crit[][3] = {
            { 5 * 64 + 32, 2 * 64 + 32, 0 }, { 6 * 64 + 40, 5 * 64 + 20, 1 },
            { 9 * 64 + 10, 12 * 64 + 0, 2 }, { 20 * 64 + 0, 7 * 64 + 40, 0 },
            { 23 * 64 + 0, 2 * 64 + 40, 1 }, { 19 * 64 + 32, 14 * 64 + 40, 2 },
            { 29 * 64 + 0, 20 * 64 + 0, 2 }, { 28 * 64 + 32, 6 * 64 + 0, 1 },
        };
        static const uint8_t spr_ids[3] = { SPR_PET, SPR_PUFF_DEMON, SPR_IMPLING };
        for (i = 0; i < (int)(sizeof(crit) / sizeof(crit[0])); i++) {
            int x = crit[i][0] + (isin((t * 24 + i * 700) & 4095) * 12 >> 12);
            int y = crit[i][1];
            add_bb(x, y, floor_at(m, x, y), spr(spr_ids[crit[i][2]], (t >> 3) & 1, crit[i][2]),
                   256 + 64, BB_SHADOW | ((t + i * 13) % 40 < 20 ? BB_FLIPX : 0), 0);
        }
        /* trees and flowers in the courtyard */
        add_bb(13 * 64 + 20, 3 * 64 + 0, 0, spr(SPR_TREE, 0, 0), 320, BB_SHADOW, 0);
        add_bb(24 * 64 + 40, 4 * 64 + 20, 0, spr(SPR_TREE, 0, 0), 320, BB_SHADOW, 0);
        add_bb(21 * 64 + 40, 1 * 64 + 40, 0, spr(SPR_FLOWER_BED, 0, 0), 256, 0, 0);
        add_bb(16 * 64 + 0, 7 * 64 + 32, 24, spr(SPR_FLOWER_BED, 0, 0), 256, 0, 0);
        /* torches in hell (fullbright) and a portal */
        add_bb(13 * 64 + 10, 14 * 64 + 10, 0, spr(SPR_TORCH, (t >> 2) & 3, 2), 256, BB_FULLBRIGHT, 8);
        add_bb(29 * 64 + 50, 13 * 64 + 10, 0, spr(SPR_TORCH, (t >> 2) & 3, 2), 256, BB_FULLBRIGHT, 8);
        add_bb(29 * 64 + 0, 2 * 64 + 0, 0, spr(SPR_VAPE_PORTAL, (t >> 2) & 3, 1), 320,
               BB_FULLBRIGHT | BB_TRANSLUCENT, 8);
        add_bb(21 * 64 + 32, 17 * 64 + 32, 0, spr(SPR_BARREL, 0, 2), 256, BB_SHADOW, 0);
        return;
    }
    /* core map: billboards from its things */
    if (m->def && m->def->things) {
        for (i = 0; i < m->def->thing_count && s_nbb < MAX_BB; i++) {
            const DoomoThing *th = &m->def->things[i];
            int x = th->x * 64 + 32, y = th->y * 64 + 32, sid;
            switch (th->type) {
            case THING_IMPLING: sid = SPR_IMPLING; break;
            case THING_VAPE_WRAITH: sid = SPR_VAPE_WRAITH; break;
            case THING_PUFF_DEMON: sid = SPR_PUFF_DEMON; break;
            case THING_HELLHOUND_PUP: sid = SPR_HELLHOUND_PUP; break;
            case THING_BONE_JESTER: sid = SPR_BONE_JESTER; break;
            case THING_SLIME_BELCHER: sid = SPR_SLIME_BELCHER; break;
            case THING_GARGOYLE: sid = SPR_GARGOYLE; break;
            case THING_CYBER_PIGGY: sid = SPR_CYBER_PIGGY; break;
            case THING_BRUTE: sid = SPR_BRUTE; break;
            case THING_CLOUD_SPIDER: sid = SPR_CLOUD_SPIDER; break;
            case THING_LORD_VAPULA: sid = SPR_LORD_VAPULA; break;
            case THING_HEALTH_SMALL: sid = SPR_HEALTH_SMALL; break;
            case THING_HEALTH_BIG: sid = SPR_HEALTH_BIG; break;
            case THING_ARMOR: sid = SPR_ARMOR; break;
            case THING_AMMO_BULLETS: sid = SPR_AMMO_BULLETS; break;
            case THING_AMMO_SHELLS: sid = SPR_AMMO_SHELLS; break;
            case THING_PROP_TREE: sid = SPR_TREE; break;
            case THING_PROP_LAMP: sid = SPR_LAMP; break;
            case THING_PROP_BARREL: sid = SPR_BARREL; break;
            case THING_PROP_TORCH: sid = SPR_TORCH; break;
            case THING_PROP_SKULLS: sid = SPR_SKULL_PILE; break;
            case THING_PROP_FLOWERS: sid = SPR_FLOWER_BED; break;
            case THING_VAPE_PORTAL: sid = SPR_VAPE_PORTAL; break;
            case THING_KEY_RED: sid = SPR_KEY_RED; break;
            case THING_KEY_BLUE: sid = SPR_KEY_BLUE; break;
            default: sid = 0; break;
            }
            if (!sid)
                continue;
            add_bb(x, y, floor_at(m, x, y), spr(sid, (t >> 3) & 1, i), 256,
                   sid == SPR_TORCH || sid == SPR_VAPE_PORTAL ? BB_FULLBRIGHT : BB_SHADOW, 0);
        }
    }
}

static void place_town(int t)
{
    static const int16_t trees[][2] = {
        {6, 10}, {3, 12}, {5, 20}, {2, 26}, {9, 28}, {24, 26}, {36, 12}, {37, 18},
        {33, 15}, {15, 25}, {28, 2}, {12, 4}, {38, 28}, {22, 9}, {16, 9} };
    static const int16_t lamps[][2] = { {18, 14}, {21, 17}, {15, 17} };
    int i;
    s_nbb = 0;
    for (i = 0; i < (int)(sizeof(trees) / sizeof(trees[0])); i++)
        add_bb(trees[i][0] * 64 + 32, trees[i][1] * 64 + 32, 0, spr(SPR_TREE, 0, 0), 300, BB_SHADOW, 0);
    for (i = 0; i < 3; i++)
        add_bb(lamps[i][0] * 64 + 32, lamps[i][1] * 64 + 32, 0, spr(SPR_LAMP, 0, 1), 256, 0, 6);
    add_bb(19 * 64 + 64, 15 * 64 + 64, 0, spr(SPR_FOUNTAIN, (t >> 3) & 3, 1), 300, BB_SHADOW, 0);
    add_bb(13 * 64 + 20, 15 * 64 + 10, 0, spr(SPR_BENCH, 0, 2), 256, BB_SHADOW, 0);
    add_bb(24 * 64 + 40, 12 * 64 + 40, 0, spr(SPR_MAILBOX, 0, 2), 256, BB_SHADOW, 0);
    add_bb(5 * 64, 4 * 64 + 32, 48, spr(SPR_FLOWER_BED, 0, 0), 256, 0, 0);
    /* residents strolling along the paths */
    for (i = 0; i < 6; i++) {
        int ph = (t * 3 + i * 300) % 1600;
        int x, y;
        if (i & 1) { x = ph + 64; y = 15 * 64 + 40 + (i * 9) % 30; }
        else { x = 19 * 64 + 40 + (i * 7) % 30; y = (ph * 2) % 2000 + 32; }
        add_bb(x, y, 0, spr(i % 3 == 0 ? SPR_PET : i % 3 == 1 ? SPR_EGG : SPR_CANDY, (t >> 3) & 1, i),
               256 + 64, BB_SHADOW | ((i & 1) ? BB_FLIPX : 0), 0);
    }
}

/* ---- the run loop ------------------------------------------------------------- */
enum { SC_TOUR, SC_OVERWORLD, SC_MODEL, SC_COUNT };

static const DoomoMap *s_cur_map, *s_ow_map = &s_town;
static int s_scene, s_manual, s_hud = 1;
static int32_t s_mx, s_my;
static int s_ma, s_mp, s_mcam;
static int s_mapsel;            /* 0 built-in, 1 vape cellar, 2 cloud nine */

static const DoomoMap *pick_map(int sel)
{
    static const int ids[3] = { -1, MAPID_VAPE_CELLAR, MAPID_CLOUD_NINE_SPIRE };
    if (sel > 0 && core_map_load(ids[sel % 3]))
        return core_map();
    return &s_et;
}

static void hud(const char *mode, int t)
{
    char buf[96];
    if (!s_hud)
        return;
    plat_shade_rect(0, 0, SCR_W, 20, 5);
    snprintf(buf, sizeof(buf), "PS1 GTE ENGINE  %s  %d.%dfps", mode,
             (int)(g_perf.fps_x10 / 10), (int)(g_perf.fps_x10 % 10));
    plat_text(4, 2, buf, PAL_UI_GOLD);
    snprintf(buf, sizeof(buf), "cpu %2d.%dms 3d %2d.%dms poly %4d cell %3d spr %2d",
             (int)(g_perf.cpu_us / 1000), (int)(g_perf.cpu_us / 100 % 10),
             (int)(g_r3stats.cpu_us / 1000), (int)(g_r3stats.cpu_us / 100 % 10),
             g_r3stats.polys, g_r3stats.cells, g_r3stats.sprites);
    plat_text(4, 11, buf, PAL_WHITE);
    if ((t % 60) == 59)
        dbg("perf: t=%d mode=%s fps=%d.%d cpu=%uus r3d=%uus gpuwait=%uus polys=%d cells=%d "
            "subdiv=%d clipped=%d sprites=%d prims=%uB upload=%uB faces=%d culled=%d "
            "t_setup=%u t_vis=%u t_cells=%u\n",
            t, mode, (int)(g_perf.fps_x10 / 10), (int)(g_perf.fps_x10 % 10),
            (unsigned)g_perf.cpu_us, (unsigned)g_r3stats.cpu_us, (unsigned)g_perf.sync_us,
            g_r3stats.polys, g_r3stats.cells, g_r3stats.subdiv, g_r3stats.clipped,
            g_r3stats.sprites, (unsigned)g_perf.prim_bytes, (unsigned)vram_uploads(),
            g_r3stats.faces, g_r3stats.culled, (unsigned)g_r3stats.t_setup,
            (unsigned)g_r3stats.t_vis, (unsigned)g_r3stats.t_cells);
}

static void view_fp_tp(const DoomoMap *m, int32_t x, int32_t y, int ang, int pitch, int cam,
                       int t)
{
    DoomoView3D v;
    int fz = floor_at(m, x >> 16, y >> 16);
    memset(&v, 0, sizeof(v));
    v.map = m;
    v.angle = (int16_t)ang;
    v.pitch = (int16_t)pitch;
    v.vx = 0; v.vy = 0; v.vw = SCR_W; v.vh = SCR_H;
    v.time = (uint16_t)t;
    v.camera = (uint8_t)cam;
    v.player_sprite = -1;
    place_things(m, t);
    if (cam == CAM_THIRD_PERSON && s_nbb < MAX_BB) {
        /* the player stands at (x,y); the camera sits behind and above */
        DoomoBillboard *p = &s_bb[s_nbb];
        const DoomoImage *img = spr(SPR_PET, (t >> 3) & 1, 0);
        int back = 80;
        p->x = x; p->y = y; p->z = FX(fz);
        p->img = img; p->scale = 320; p->flags = BB_SHADOW; p->light = 0;
        v.player_sprite = (int8_t)s_nbb;
        s_nbb++;
        v.x = x - ((icos(ang) * back) << 4);
        v.y = y - ((isin(ang) * back) << 4);
        v.z = FX(fz + DOOMO_EYE_HEIGHT + 20);
        if (v.pitch > -30) v.pitch = -30;
    } else {
        v.x = x; v.y = y;
        v.z = FX(fz + DOOMO_EYE_HEIGHT);
        v.bob = (int16_t)(isin((t * 180) & 4095) * 2 >> 12);
    }
    v.sprites = s_bb;
    v.sprite_count = s_nbb;
    plat_view3d(&v);
    /* crosshair */
    if (cam == CAM_FIRST_PERSON) {
        plat_rect(SCR_W / 2 - 4, SCR_H / 2, 3, 1, PAL_WHITE);
        plat_rect(SCR_W / 2 + 2, SCR_H / 2, 3, 1, PAL_WHITE);
        plat_rect(SCR_W / 2, SCR_H / 2 - 4, 1, 3, PAL_WHITE);
        plat_rect(SCR_W / 2, SCR_H / 2 + 2, 1, 3, PAL_WHITE);
    }
}

/* overworld: a stroll around the town; daylight runs noon -> dusk ->
 * night -> dawn over the scene */
static void view_overworld(const DoomoMap *m, int t, int scene_t)
{
    DoomoOverworldView ov;
    int a = (scene_t * 6) & 4095;
    memset(&ov, 0, sizeof(ov));
    ov.map = m;
    ov.cx = FX(m->w * 32 + (icos(a) * (m->w * 14)) / 4096);
    ov.cy = FX(m->h * 32 + (isin(a) * (m->h * 12)) / 4096);
    ov.vx = 0; ov.vy = 0; ov.vw = SCR_W; ov.vh = SCR_H;
    if (m == &s_town)
        place_town(t);
    else
        place_things(m, t);
    ov.sprites = s_bb;
    ov.sprite_count = s_nbb;
    {
        int d = scene_t % OW_FRAMES;          /* 15 .. 0 .. 15 */
        d = d < OW_FRAMES / 2 ? 15 - (d * 30) / OW_FRAMES : ((d - OW_FRAMES / 2) * 30) / OW_FRAMES;
        if (d < 0) d = 0;
        if (d > 15) d = 15;
        ov.daylight = (uint8_t)d;
    }
    ov.time = (uint16_t)t;
    plat_overworld(&ov);
}

/* model showcase: orbit the third-person 3D citizen in the courtyard */
static void view_model(int t, int scene_t)
{
    DoomoView3D v;
    DoomoBillboard *p;
    int orbit = (scene_t * 10) & 4095, run = (scene_t / 90) & 1;
    int32_t px = FX(17 * 64), py = FX(2 * 64 + 40);
    int dist = 64 + ((isin((scene_t * 7) & 4095) * 16) >> 12);
    memset(&v, 0, sizeof(v));
    v.map = &s_et;
    s_nbb = 0;
    if (run) {
        /* jog on a small circle so the run cycle plays */
        px += (icos((scene_t * 40) & 4095) * 24) << 4;
        py += (isin((scene_t * 40) & 4095) * 24) << 4;
    }
    p = &s_bb[s_nbb++];
    p->x = px; p->y = py; p->z = 0;
    p->img = spr(SPR_PET, 0, 0); p->scale = 256; p->flags = BB_SHADOW; p->light = 0;
    v.player_sprite = 0;
    v.sprites = s_bb;
    v.sprite_count = s_nbb;
    v.camera = CAM_THIRD_PERSON;
    v.angle = (int16_t)((orbit + 2048) & 4095);           /* look at the model */
    v.pitch = -40;
    v.x = px + ((icos(orbit) * dist) << 4);
    v.y = py + ((isin(orbit) * dist) << 4);
    v.z = FX(34);
    v.vw = SCR_W; v.vh = SCR_H;
    v.time = (uint16_t)t;
    plat_view3d(&v);
}

/* Memory card self test (SELECT + START in the engine test, or at boot of
 * the engine test when the harness asks for it): save, read back, grow a
 * slot (second file, the old one erased), erase. Prints PASS/FAIL. With
 * the core linked the buffers live in the core's save scratch. */
#define MC_TEST_MAX (36 * 1024)
#ifdef DOOMO_HAVE_CORE
static uint8_t *s_mc_buf, *s_mc_rd;
#else
static uint8_t s_mc_buf[MC_TEST_MAX];
static uint8_t s_mc_rd[MC_TEST_MAX];
#endif

static int mc_fill_check(uint32_t seed, uint32_t n, int check)
{
    uint32_t i, x = seed;
    for (i = 0; i < n; i++) {
        x = x * 1664525u + 1013904223u;
        if (check) {
            if (s_mc_rd[i] != (uint8_t)(x >> 24))
                return 0;
        } else {
            s_mc_buf[i] = (uint8_t)(x >> 24);
        }
    }
    return 1;
}

static void memcard_selftest(void)
{
    uint32_t sz = 0;
    int ok = 1, r;
#ifdef DOOMO_HAVE_CORE
    {
        uint32_t cap = 0;
        s_mc_buf = plat_save_scratch(&cap);
        s_mc_rd = s_mc_buf + MC_TEST_MAX;
        if (!s_mc_buf || cap < 2 * MC_TEST_MAX) {
            dbg("memcard selftest: FAIL (scratch %u bytes)\n", (unsigned)cap);
            return;
        }
    }
#endif
    dbg("memcard selftest: start\n");
    plat_save_erase(0);
    plat_save_erase(1);
    mc_fill_check(1, 5000, 0);
    r = plat_save_write(0, s_mc_buf, 5000);
    dbg("memcard selftest: write slot0 5000B -> %d\n", r);
    ok &= r;
    memset(s_mc_rd, 0, MC_TEST_MAX);
    r = plat_save_read(0, s_mc_rd, MC_TEST_MAX, &sz);
    dbg("memcard selftest: read slot0 -> %d size %u\n", r, (unsigned)sz);
    ok &= r && sz == 5000 && mc_fill_check(1, 5000, 1);
    /* grow slot 0 past its block count: temp file + rename */
    mc_fill_check(2, 20000, 0);
    r = plat_save_write(0, s_mc_buf, 20000);
    dbg("memcard selftest: grow slot0 20000B -> %d\n", r);
    ok &= r;
    memset(s_mc_rd, 0, MC_TEST_MAX);
    r = plat_save_read(0, s_mc_rd, MC_TEST_MAX, &sz);
    ok &= r && sz == 20000 && mc_fill_check(2, 20000, 1);
    dbg("memcard selftest: read grown slot0 -> %d size %u\n", r, (unsigned)sz);
    /* slot 1, an odd size */
    mc_fill_check(3, 33333, 0);
    r = plat_save_write(1, s_mc_buf, 33333);
    ok &= r;
    memset(s_mc_rd, 0, MC_TEST_MAX);
    r = plat_save_read(1, s_mc_rd, MC_TEST_MAX, &sz);
    ok &= r && sz == 33333 && mc_fill_check(3, 33333, 1);
    dbg("memcard selftest: slot1 33333B -> %d size %u\n", r, (unsigned)sz);
    /* erase slot 1, reading it must fail now */
    r = plat_save_erase(1);
    dbg("memcard selftest: erase slot1 -> %d\n", r);
    ok &= !plat_save_read(1, s_mc_rd, MC_TEST_MAX, &sz);
    /* leave a save in slot 0 for the emulator's card manager */
    dbg("memcard selftest: %s\n", ok ? "PASS" : "FAIL");
}

/* quick CPU benchmark of the pipeline pieces, printed to the TTY */
static void bench(void)
{
    void r3_bench(void);
    plat_begin_frame();
    r3_camera(FX(100), FX(100), FX(40), 0, 0, 0, 0, 320, 240, 160, 1600, 300, 0);
    r3_begin();
    r3_bench();
    r3_end();
    plat_end_frame();
}

void etest_run(void)
{
    uint32_t t = 0, prev = 0;
    int scene_t = 0;
    dbg("etest: engine test, core maps %s\n", core_has_maps() ? "available" : "missing");
    core_palette_init();
    bench();
    etest_build_map(&s_et);
    build_town(&s_town);
    s_mapsel = core_has_maps() ? 1 : 0;
    s_cur_map = pick_map(s_mapsel);
    for (;;) {
        uint32_t b, pressed;
        hcmd_poll();
        b = pad_read();
        pressed = b & ~prev;
        prev = b;
        if (pressed)
            s_manual = 1;
        if ((b & BTN_SELECT) && (pressed & BTN_START)) {
            memcard_selftest();
            pressed = 0;
            prev = b;
        }
        if (pressed & BTN_START) {
            s_scene = (s_scene + 1) % SC_COUNT;
            if (s_scene == SC_TOUR) {
                s_mapsel = (s_mapsel + 1) % 3;
                s_cur_map = pick_map(s_mapsel);
                world_invalidate();
            }
            scene_t = 0;
        }
        if (pressed & BTN_Y)
            s_hud ^= 1;
        if (pressed & BTN_SELECT)
            s_mcam ^= 1;
        plat_begin_frame();
        plat_clear(PAL_BLACK);
        if (s_scene == SC_TOUR) {
            Key k;
            if (!s_manual) {
                int ft = scene_t % (TOUR_END + 1);
                if (s_cur_map == &s_et)
                    tour_at(ft, &k);
                else {
                    /* core map: slow orbit from its player start */
                    k.x = (int16_t)(s_cur_map->w * 32); k.y = (int16_t)(s_cur_map->h * 32);
                    k.ang = (int16_t)((ft * 6) & 4095); k.pitch = (int16_t)((isin(ft * 20 & 4095) * 120) >> 12);
                    k.cam = (uint8_t)((ft / 300) & 1);
                }
                s_mx = FX(k.x); s_my = FX(k.y); s_ma = k.ang; s_mp = k.pitch; s_mcam = k.cam;
                if (s_cur_map == &s_et && scene_t >= TOUR_END) {
                    s_scene = SC_OVERWORLD;
                    scene_t = -1;
                }
                if (s_cur_map != &s_et && scene_t >= 900) {
                    s_scene = SC_OVERWORLD;
                    scene_t = -1;
                }
            } else {
                int sp = (b & BTN_B) ? 6 : 3;
                if (b & BTN_LEFT)  s_ma = (s_ma - 48) & 4095;
                if (b & BTN_RIGHT) s_ma = (s_ma + 48) & 4095;
                if (b & BTN_UP)    { s_mx += icos(s_ma) * sp * 16; s_my += isin(s_ma) * sp * 16; }
                if (b & BTN_DOWN)  { s_mx -= icos(s_ma) * sp * 16; s_my -= isin(s_ma) * sp * 16; }
                if (b & BTN_L)     { s_mx += isin(s_ma) * sp * 16; s_my -= icos(s_ma) * sp * 16; }
                if (b & BTN_R)     { s_mx -= isin(s_ma) * sp * 16; s_my += icos(s_ma) * sp * 16; }
                if (b & BTN_L2)    s_mp = s_mp > -256 ? s_mp - 8 : -256;
                if (b & BTN_R2)    s_mp = s_mp < 256 ? s_mp + 8 : 256;
            }
            view_fp_tp(s_cur_map, s_mx, s_my, s_ma, s_mp, s_mcam ? CAM_THIRD_PERSON : CAM_FIRST_PERSON,
                       (int)t);
            hud(s_mcam ? "3RD PERSON" : "1ST PERSON", (int)t);
        } else if (s_scene == SC_MODEL) {
            view_model((int)t, scene_t);
            hud("3D CITIZEN", (int)t);
            if (!s_manual && scene_t >= 360) {
                s_scene = SC_TOUR;
                scene_t = -1;
                s_cur_map = pick_map(s_mapsel);
                world_invalidate();
            }
        } else {
            if (scene_t == 0) {
                s_ow_map = &s_town;
                if (core_map_load(MAPID_HAHALAND))
                    s_ow_map = core_map();
                world_invalidate();
            }
            view_overworld(s_ow_map, (int)t, scene_t);
            hud("OVERWORLD", (int)t);
            if (!s_manual && scene_t >= OW_FRAMES) {
                s_scene = SC_MODEL;
                scene_t = -1;
                world_invalidate();
            }
        }
        plat_end_frame();
        g_loop_frames++;
        t++;
        scene_t++;
    }
}
