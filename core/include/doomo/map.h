/* map.h - the shared 2.5D world format.
 *
 * One format serves every view:
 *   - the cutesy top-down overworld (GBA: 2D tiles, PS1: tilted 3D camera)
 *   - first-person Hell Breach levels (GBA: height raycaster with textured
 *     floors/ceilings, PS1: true 3D polygons on the GTE)
 *
 * The world is a grid of cells, each DOOMO_CELL_UNITS world units square.
 * Every cell has its own floor and ceiling height, so maps have stairs,
 * ledges, pits, raised platforms, low ceilings, lifts and doors - the
 * renderers draw the vertical faces between neighbouring cells of
 * different heights ("step faces"), not just full-height walls.
 *
 * Coordinates: x grows east, y grows south. World units are integers in
 * map data and 16.16 fixed point at runtime (actor positions, cameras).
 * Heights: floor_h / ceil_h are in units of DOOMO_HEIGHT_STEP world units.
 */
#ifndef DOOMO_MAP_H
#define DOOMO_MAP_H

#include "types.h"

#define DOOMO_CELL_UNITS   64
#define DOOMO_HEIGHT_STEP  8          /* floor_h 1 == 8 world units        */
#define DOOMO_MAP_MAX_W    64
#define DOOMO_MAP_MAX_H    64
#define DOOMO_EYE_HEIGHT   40         /* player eye above floor            */
#define DOOMO_STEP_UP      24         /* max climbable step                */
#define DOOMO_TEX_SIZE     64         /* textures are 64x64 8bpp           */
#define DOOMO_TEX_SKY      255        /* ceil_tex value: open sky          */

enum {
    CELL_DOOR    = 0x01,  /* ceiling slides up when used (door state anim) */
    CELL_LIFT    = 0x02,  /* floor moves between floor_h and tag-height    */
    CELL_LAVA    = 0x04,  /* hurts (BRUTAL: lava, CUTE: hot cocoa)          */
    CELL_WATER   = 0x08,  /* slows, animated                               */
    CELL_SECRET  = 0x10,
    CELL_EXIT    = 0x20,  /* finishing the level                           */
    CELL_PORTAL  = 0x40,  /* vape-cloud hell portal (animated, spawns)     */
    CELL_TRIGGER = 0x80   /* fires story/script tag on enter               */
};

typedef struct DoomoCell {
    uint8_t wall;       /* 0 = open cell; else texture id of a solid block */
    uint8_t floor_tex;  /* floor texture id (also the overworld tile)      */
    uint8_t ceil_tex;   /* ceiling texture id or DOOMO_TEX_SKY             */
    int8_t  floor_h;    /* floor height  (x DOOMO_HEIGHT_STEP)              */
    int8_t  ceil_h;     /* ceiling height (x DOOMO_HEIGHT_STEP)             */
    uint8_t light;      /* 0..15 sector light                              */
    uint8_t flags;      /* CELL_*                                          */
    uint8_t tag;        /* script / trigger / lift target tag              */
} DoomoCell;            /* 8 bytes */

/* Things placed in a map (spawns). Types are THING_* in actor.h. */
typedef struct DoomoThing {
    uint8_t x, y;       /* cell */
    uint8_t type;       /* THING_* */
    uint8_t angle;      /* 0..255 -> 0..4095 */
    uint8_t arg;        /* type-specific (npc resident id low byte, etc.) */
    uint8_t flags;
} DoomoThing;

enum { MAP_OVERWORLD, MAP_DUNGEON, MAP_ARENA };

typedef struct DoomoMapDef {
    const char       *name;
    uint8_t           w, h;
    uint8_t           kind;      /* MAP_* */
    uint8_t           sky;       /* sky id (textures.h SKY_*) */
    uint8_t           music;     /* SONG_* */
    uint8_t           ambient;   /* base light 0..15 */
    const DoomoCell  *cells;     /* w*h, row-major */
    const DoomoThing *things;
    uint16_t          thing_count;
} DoomoMapDef;

/* The live, mutable copy of the current map (doors open, lifts move,
 * walls get blown up). Renderers read ONLY this. */
typedef struct DoomoMap {
    const DoomoMapDef *def;
    uint8_t   w, h;
    uint8_t   kind, sky, ambient;
    uint16_t  time;              /* ticks since load (texture animation) */
    DoomoCell cells[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H];
} DoomoMap;

extern DoomoMap doomo_map;

/* Map registry (owner: action for dungeons/arenas, story for overworld). */
enum { MAPID_HAHALAND,          /* cutesy HaHaland + Hillvile side of park  */
       MAPID_HAHALAND_INVADED,  /* same, after the vape gate erupts         */
       MAPID_HELLVILE,          /* bad side of the park (free play visits)  */
       MAPID_VAPE_CELLAR,       /* Hell Breach 1 (first-person)            */
       MAPID_CLOUD_NINE_SPIRE,  /* Hell Breach 2 + Lord Vapula             */
       MAPID_ARENA,             /* free play demon arena                   */
       MAPID_PARK_UNITED,       /* ending: wall down, both sides comingle  */
       /* (levels expansion, additive) Hell Breach 3..7 */
       MAPID_SUGAR_RUSH_SEWERS, MAPID_PLUSHIE_FACTORY, MAPID_GRAVEYARD_SHIFT,
       MAPID_BONE_CATHEDRAL, MAPID_HELLVILE_UNDERBELLY,
       MAPID_COUNT };
extern const DoomoMapDef *const doomo_maps[MAPID_COUNT];

void map_load(int map_id);
static inline const DoomoCell *map_cell(const DoomoMap *m, int cx, int cy) {
    return &m->cells[cy * m->w + cx];
}
/* Out-of-range cells are treated as solid wall texture 1. */
const DoomoCell *map_cell_safe(const DoomoMap *m, int cx, int cy);
/* World-unit floor / ceiling at a cell (includes door/lift animation). */
int  map_floor_z(const DoomoMap *m, int cx, int cy);
int  map_ceil_z(const DoomoMap *m, int cx, int cy);
void map_tick(DoomoMap *m);          /* animate doors & lifts */
void map_use(DoomoMap *m, int cx, int cy);  /* open door / call lift */

/* ======================================================================
 * action additions (owner: action; additive)
 * ==================================================================== */

/* Cell semantics used by the runtime (map.c) and the map compiler
 * (tools/mapc.py, legend in core/data/maps/README.md):
 *
 *  DOOR  floor_h = sill, ceil_h = the OPEN height, ceil_tex = the door
 *        texture. Doors start closed: the live copy in doomo_map has
 *        ceil_h == floor_h and map_ceil_z() returns the animated height.
 *        tag 0        opens when bumped, stays open
 *        tag 1 / 2    needs the red / blue key (then like tag 0)
 *        tag 3..15    opens when bumped, closes again after ~4 s
 *        tag >= 16    remote: opened only by map_event(tag) (triggers,
 *                     cleared demon groups, bosses)
 *  LIFT  floor_h = rest height, (int8_t)tag = the other height. Standing
 *        on it (or map_use) sends it to the other height; it waits ~3 s
 *        and returns. map_floor_z() returns the animated height.
 *  TRIGGER  entering the cell fires map_event(tag) once (and story_trigger).
 *  SECRET   entering counts a secret (once per cell).
 *  EXIT     entering finishes the level.
 *  LAVA / WATER / PORTAL  hurt / slow / animated hell portal.
 */
#define DOOMO_DOOR_TAG_RED     1
#define DOOMO_DOOR_TAG_BLUE    2
#define DOOMO_DOOR_TAG_REMOTE  16     /* tags >= this are remote-only     */
#define DOOMO_PLAYER_HEIGHT    56     /* standing; crouched: 32           */
#define DOOMO_CROUCH_HEIGHT    32

/* DoomoThing.flags */
enum {
    THF_AMBUSH     = 0x01,  /* deaf: waits until it sees the player       */
    THF_NOT_EASY   = 0x02,  /* absent on DIFF_PLUSHIE                      */
    THF_HARD       = 0x04,  /* only on DIFF_NIGHTMARE and above            */
    THF_DORMANT    = 0x08,  /* appears (vape puff) when event `arg` fires  */
    THF_BOSS       = 0x10,  /* boss / mini-boss: more HP, boss bar         */
    THF_KILL_EVENT = 0x20,  /* when every demon of this group is dead, fire
                               event `arg` (`arg + 1` if also DORMANT)     */
    THF_SECRET     = 0x40   /* pickup counts as a secret item              */
};

/* Display title of a map ("The Vape Cellar"), "" if unknown. */
const char *map_title(int map_id);
/* The map id currently in doomo_map (-1 if none / custom). */
int  map_current_id(void);
/* Load an arbitrary definition (tests, tools). */
void map_load_def(const DoomoMapDef *d);
/* Fire a map event: open (or toggle) every remote door whose tag == tag,
 * start lifts with that tag. Returns how many movers reacted. */
int  map_event(DoomoMap *m, int tag);
/* Door state 0 closed .. 256 fully open (-1 if not a door). */
int  map_door_open(const DoomoMap *m, int cx, int cy);
/* (additive) A door's open ceiling height in world units, 0 if (cx, cy)
 * has no door mover. Use it instead of reading the definition's cells. */
int  map_door_top(const DoomoMap *m, int cx, int cy);
/* (ps1, additive) Copy the w x h top-left cells of a definition into dst
 * (row-major, w per row). map_load_def() is the ONLY reader of
 * DoomoMapDef.cells: the PS1 keeps the grids LZ-packed and overrides this
 * (ps1/tools/packmaps.py), so nothing else may read through d->cells. */
void map_def_cells(const DoomoMapDef *d, DoomoCell *dst, int w, int h);
/* Bump a door (keys = bitmask 1 red, 2 blue). Returns 1 if it opens or
 * is open, 0 if it is locked / remote, -1 if not a door. */
int  map_door_bump(DoomoMap *m, int cx, int cy, int keys);
/* 1 while a lift is moving or waiting away from its rest height. */
int  map_lift_busy(const DoomoMap *m, int cx, int cy);
/* Both heights of a lift cell in world units (lo <= hi); 0 if no lift. */
int  map_lift_range(const DoomoMap *m, int cx, int cy, int *lo, int *hi);
/* 1 if a mover (door / lift) changed height this tick at that cell. */
int  map_mover_moving(const DoomoMap *m, int cx, int cy);
/* Number of active movers (doors + lifts) in the live map. */
int  map_mover_count(void);

#endif
