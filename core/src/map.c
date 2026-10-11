/* map.c - the live map: loading, doors, lifts, events (owner: action).
 *
 * doomo_map is the mutable copy of the current map that every renderer
 * reads. Doors and lifts are "movers": a small table of animated heights
 * (world units). map_floor_z()/map_ceil_z() return the animated height of
 * mover cells of doomo_map, and the live cells' floor_h / ceil_h are kept
 * in step (rounded to height steps) for code that reads cells directly.
 *
 * Cell semantics (also documented in map.h and core/data/maps/README.md):
 *   DOOR  def floor_h = sill, def ceil_h = open height; starts closed.
 *         tag 0 bump, 1/2 red/blue key, 3..15 bump + auto-close,
 *         >= 16 remote (map_event only).
 *   LIFT  def floor_h = rest height, (int8_t)tag = the other height.
 */
#include "doomo/map.h"
#include "doomo/textures.h"
#include "doomo/audio.h"

DoomoMap doomo_map;

#define MOVER_MAX     48
#define DOOR_SPEED    5      /* world units per tick */
#define LIFT_SPEED    4
#define DOOR_WAIT     120    /* auto-close doors (tag 3..15) */
#define LIFT_WAIT     90

enum { MV_DOOR, MV_LIFT };
enum { MS_IDLE, MS_TO_HI, MS_WAIT, MS_TO_LO };

typedef struct Mover {
    uint16_t cell;           /* index into cells[] */
    uint8_t  kind;           /* MV_* */
    uint8_t  state;          /* MS_* */
    int16_t  cur;            /* current height (world units) */
    int16_t  lo, hi;         /* door: closed / open; lift: rest / other */
    uint16_t timer;
    uint8_t  tag;
    uint8_t  moved;          /* changed height this tick */
} Mover;

static Mover movers[MOVER_MAX];
static int   nmovers;
static int   cur_map_id = -1;
static int   last_hit;       /* lookup cache */

static int step_floor(int units)       /* world units -> height steps (floor) */
{
    return units >= 0 ? units / DOOMO_HEIGHT_STEP
                      : -((-units + DOOMO_HEIGHT_STEP - 1) / DOOMO_HEIGHT_STEP);
}

static Mover *find_mover(const DoomoMap *m, int cx, int cy)
{
    int idx, i;
    if (m != &doomo_map || cx < 0 || cy < 0 || cx >= m->w || cy >= m->h) return NULL;
    idx = cy * m->w + cx;
    if (last_hit < nmovers && movers[last_hit].cell == idx) return &movers[last_hit];
    for (i = 0; i < nmovers; i++)
        if (movers[i].cell == idx) { last_hit = i; return &movers[i]; }
    return NULL;
}

const DoomoCell *map_cell_safe(const DoomoMap *m, int cx, int cy)
{
    static const DoomoCell solid = { 1, 1, 1, 0, 0, 8, 0, 0 };
    if (!m || cx < 0 || cy < 0 || cx >= m->w || cy >= m->h) return &solid;
    return &m->cells[cy * m->w + cx];
}

int map_floor_z(const DoomoMap *m, int cx, int cy)
{
    const DoomoCell *c = map_cell_safe(m, cx, cy);
    if (c->flags & CELL_LIFT) {
        const Mover *mv = find_mover(m, cx, cy);
        if (mv) return mv->cur;
    }
    return c->floor_h * DOOMO_HEIGHT_STEP;
}

int map_ceil_z(const DoomoMap *m, int cx, int cy)
{
    const DoomoCell *c = map_cell_safe(m, cx, cy);
    if (c->flags & CELL_DOOR) {
        const Mover *mv = find_mover(m, cx, cy);
        if (mv) return mv->cur;
    }
    return c->ceil_h * DOOMO_HEIGHT_STEP;
}

static void sync_cell(const Mover *mv)
{
    DoomoCell *c = &doomo_map.cells[mv->cell];
    if (mv->kind == MV_DOOR) c->ceil_h = (int8_t)step_floor(mv->cur);
    else c->floor_h = (int8_t)step_floor(mv->cur);
}

/* weak: the PS1 keeps the cells packed and overrides it (map.h) */
__attribute__((weak)) void map_def_cells(const DoomoMapDef *d, DoomoCell *dst, int w, int h)
{
    if (d->w == w) {
        memcpy(dst, d->cells, (size_t)(w * h) * sizeof(DoomoCell));
    } else {
        int y;
        for (y = 0; y < h; y++)
            memcpy(&dst[y * w], &d->cells[y * d->w], (size_t)w * sizeof(DoomoCell));
    }
}

void map_load_def(const DoomoMapDef *d)
{
    int i, n;
    memset(&doomo_map, 0, sizeof(doomo_map));
    nmovers = 0;
    last_hit = 0;
    if (!d) return;
    doomo_map.def = d;
    doomo_map.w = d->w > DOOMO_MAP_MAX_W ? DOOMO_MAP_MAX_W : d->w;
    doomo_map.h = d->h > DOOMO_MAP_MAX_H ? DOOMO_MAP_MAX_H : d->h;
    doomo_map.kind = d->kind;
    doomo_map.sky = d->sky;
    doomo_map.ambient = d->ambient;
    n = doomo_map.w * doomo_map.h;
    map_def_cells(d, doomo_map.cells, doomo_map.w, doomo_map.h);
    for (i = 0; i < n && nmovers < MOVER_MAX; i++) {
        DoomoCell *c = &doomo_map.cells[i];
        Mover *mv;
        if (c->wall || !(c->flags & (CELL_DOOR | CELL_LIFT))) continue;
        mv = &movers[nmovers++];
        memset(mv, 0, sizeof(*mv));
        mv->cell = (uint16_t)i;
        mv->tag = c->tag;
        if (c->flags & CELL_DOOR) {
            mv->kind = MV_DOOR;
            mv->lo = (int16_t)(c->floor_h * DOOMO_HEIGHT_STEP);
            mv->hi = (int16_t)(c->ceil_h * DOOMO_HEIGHT_STEP);
            if (mv->hi < mv->lo) mv->hi = mv->lo;
        } else {
            mv->kind = MV_LIFT;
            mv->lo = (int16_t)(c->floor_h * DOOMO_HEIGHT_STEP);
            mv->hi = (int16_t)((int8_t)c->tag * DOOMO_HEIGHT_STEP);
        }
        mv->cur = mv->lo;
        sync_cell(mv);
    }
}

/* A tiny pastel room used when a map is not available yet. */
static const DoomoCell fb_wall = { TEX_PASTEL_BRICK, TEX_PASTEL_BRICK, 0, 0, 16, 0, 0, 0 };
static const DoomoCell fb_open = { 0, TEX_WOOD_FLOOR, TEX_CLOUD_PUFF, 0, 14, 13, 0, 0 };
static const DoomoThing fb_things[1] = { { 3, 3, 1 /* THING_PLAYER_START */, 0, 0, 0 } };

static void load_fallback(void)
{
    int x, y;
    static const DoomoMapDef fb_def = { "Pastel Room", 8, 8, MAP_DUNGEON, SKY_PASTEL_DAY,
                                        SONG_HAHALAND_DAY, 12, NULL, fb_things, 1 };
    memset(&doomo_map, 0, sizeof(doomo_map));
    nmovers = 0;
    doomo_map.def = &fb_def;
    doomo_map.w = 8;
    doomo_map.h = 8;
    doomo_map.kind = MAP_DUNGEON;
    doomo_map.sky = SKY_PASTEL_DAY;
    doomo_map.ambient = 12;
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            doomo_map.cells[y * 8 + x] =
                (x == 0 || y == 0 || x == 7 || y == 7) ? fb_wall : fb_open;
}

void map_load(int map_id)
{
    const DoomoMapDef *d = (map_id >= 0 && map_id < MAPID_COUNT) ? doomo_maps[map_id] : NULL;
    if (d && d->cells) {
        map_load_def(d);
        cur_map_id = map_id;
    } else {
        load_fallback();
        cur_map_id = map_id >= 0 && map_id < MAPID_COUNT ? map_id : -1;
    }
}

int map_current_id(void) { return doomo_map.def ? cur_map_id : -1; }

const char *map_title(int map_id)
{
    static const char *const names[MAPID_COUNT] = {
        "HaHaland", "HaHaland (Invaded)", "Hellvile", "The Vape Cellar",
        "Cloud Nine Spire", "The Demon Arena", "Hellvile Park United",
        /* (levels expansion, additive) */
        "Sugar Rush Sewers", "Plushie Factory", "Graveyard Shift", "Bone Cathedral",
        "Hellvile Underbelly"
    };
    const DoomoMapDef *d;
    if (map_id < 0 || map_id >= MAPID_COUNT) return "";
    d = doomo_maps[map_id];
    return d && d->name ? d->name : names[map_id];
}

static void mover_go(Mover *mv, int state)
{
    if (mv->state == state) return;
    mv->state = (uint8_t)state;
    mv->timer = 0;
    if (state == MS_TO_HI || state == MS_TO_LO) audio_sfx(SFX_DOOR);
}

void map_tick(DoomoMap *m)
{
    int i;
    if (!m) return;
    m->time++;
    if (m != &doomo_map) return;
    for (i = 0; i < nmovers; i++) {
        Mover *mv = &movers[i];
        int sp = mv->kind == MV_DOOR ? DOOR_SPEED : LIFT_SPEED;
        int before = mv->cur;
        switch (mv->state) {
        case MS_TO_HI:
            if (mv->cur < mv->hi) mv->cur = (int16_t)DOOMO_MIN(mv->cur + sp, mv->hi);
            else if (mv->cur > mv->hi) mv->cur = (int16_t)DOOMO_MAX(mv->cur - sp, mv->hi);
            if (mv->cur == mv->hi) {
                int stays = mv->kind == MV_DOOR && !(mv->tag >= 3 && mv->tag < DOOMO_DOOR_TAG_REMOTE);
                mv->state = stays ? MS_IDLE : MS_WAIT;
                mv->timer = 0;
            }
            break;
        case MS_WAIT:
            if (++mv->timer >= (mv->kind == MV_DOOR ? DOOR_WAIT : LIFT_WAIT))
                mover_go(mv, MS_TO_LO);
            break;
        case MS_TO_LO:
            if (mv->cur < mv->lo) mv->cur = (int16_t)DOOMO_MIN(mv->cur + sp, mv->lo);
            else if (mv->cur > mv->lo) mv->cur = (int16_t)DOOMO_MAX(mv->cur - sp, mv->lo);
            if (mv->cur == mv->lo) { mv->state = MS_IDLE; mv->timer = 0; }
            break;
        default:
            break;
        }
        mv->moved = (uint8_t)(mv->cur != before);
        if (mv->moved) sync_cell(mv);
    }
}

/* Activate a mover: doors open, lifts go to their other height (or wait
 * longer there). */
static int activate(Mover *mv)
{
    if (mv->kind == MV_DOOR) {
        if (mv->state == MS_TO_LO || (mv->state == MS_IDLE && mv->cur != mv->hi)) {
            mover_go(mv, MS_TO_HI);
        } else if (mv->state == MS_WAIT) {
            mv->timer = 0;
        }
        return 1;
    }
    if (mv->state == MS_IDLE && mv->cur == mv->lo) mover_go(mv, MS_TO_HI);
    else if (mv->state == MS_WAIT) mv->timer = 0;
    return 1;
}

void map_use(DoomoMap *m, int cx, int cy)
{
    Mover *mv = find_mover(m, cx, cy);
    if (mv) activate(mv);
}

int map_event(DoomoMap *m, int tag)
{
    int i, n = 0;
    if (m != &doomo_map || tag <= 0) return 0;
    for (i = 0; i < nmovers; i++) {
        Mover *mv = &movers[i];
        if (mv->kind != MV_DOOR || mv->tag != tag) continue;
        activate(mv);
        n++;
    }
    return n;
}

int map_door_open(const DoomoMap *m, int cx, int cy)
{
    const Mover *mv = find_mover(m, cx, cy);
    int span;
    if (!mv || mv->kind != MV_DOOR) return -1;
    span = mv->hi - mv->lo;
    if (span <= 0) return 256;
    return (mv->cur - mv->lo) * 256 / span;
}

int map_door_top(const DoomoMap *m, int cx, int cy)
{
    const Mover *mv = find_mover(m, cx, cy);
    return mv && mv->kind == MV_DOOR ? mv->hi : 0;
}

int map_door_bump(DoomoMap *m, int cx, int cy, int keys)
{
    Mover *mv = find_mover(m, cx, cy);
    if (!mv || mv->kind != MV_DOOR) return -1;
    if (mv->tag >= DOOMO_DOOR_TAG_REMOTE) return mv->cur == mv->hi ? 1 : 0;
    if (mv->tag == DOOMO_DOOR_TAG_RED && !(keys & 1)) return 0;
    if (mv->tag == DOOMO_DOOR_TAG_BLUE && !(keys & 2)) return 0;
    activate(mv);
    return 1;
}

int map_lift_busy(const DoomoMap *m, int cx, int cy)
{
    const Mover *mv = find_mover(m, cx, cy);
    if (!mv || mv->kind != MV_LIFT) return 0;
    return mv->state != MS_IDLE || mv->cur != mv->lo;
}

int map_lift_range(const DoomoMap *m, int cx, int cy, int *lo, int *hi)
{
    const DoomoCell *c = map_cell_safe(m, cx, cy);
    int a, b;
    if (!(c->flags & CELL_LIFT) || c->wall) return 0;
    {
        const Mover *mv = find_mover(m, cx, cy);
        if (mv) { a = mv->lo; b = mv->hi; }
        else { a = c->floor_h * DOOMO_HEIGHT_STEP; b = (int8_t)c->tag * DOOMO_HEIGHT_STEP; }
    }
    if (lo) *lo = DOOMO_MIN(a, b);
    if (hi) *hi = DOOMO_MAX(a, b);
    return 1;
}

int map_mover_moving(const DoomoMap *m, int cx, int cy)
{
    const Mover *mv = find_mover(m, cx, cy);
    return mv ? mv->moved : 0;
}

int map_mover_count(void) { return nmovers; }
