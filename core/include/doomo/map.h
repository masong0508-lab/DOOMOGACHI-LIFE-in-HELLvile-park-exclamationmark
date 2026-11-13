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
    THF_SECRET     = 0x40,  /* pickup counts as a secret item              */
    THF_MACHINE_HEART = 0x80 /* a THING_SCRAP that is Bonejangles' machine heart:
                               picking it up sets SF_BONE_HEART (E09)       */
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

/* ======================================================================
 * levels additions (owner: levels, LV1; additive): map format v2
 * ==================================================================== */

/* Map format v2 (tools/mapc.py, core/data/maps/README.md "Format v2").
 * DoomoCell, the cell flags and the thing flags are full, so everything
 * new is a ROM side table in a DoomoMapExt. A v1 map has no DoomoMapExt:
 * its generated file and its DoomoMapDef are unchanged. A v2 map's
 * generated file defines `map_ext_<name>` next to `map_def_<name>`; the
 * ext points back at its def. Nothing here is mutable and nothing is
 * resident RAM: the accessors live in core/src/levels_mapx.c and take the
 * caller's state. */

#define DOOMO_ZONE_MAX        32
#define DOOMO_PATH_MAX        8
#define DOOMO_PATH_POINTS     32     /* per path */
#define DOOMO_SWAP_MAX        16     /* named sets per map */
#define DOOMO_SWAP_CELLS_MAX  256    /* cell overrides, all sets together */
#define DOOMO_TEXBANK_MAX     8
#define DOOMO_TEXBANK_FIRST   56     /* level-local texture ids 56..63 */
#define DOOMO_TEXBANK_NAME    12     /* bytes, with the NUL */

/* DoomoZone.kind. Append only: the values are in generated maps. */
enum { ZONE_NONE,
       ZONE_FOG,      /* arg = fog distance in cells                        */
       ZONE_TIDE,     /* arg = high-water floor height (steps)              */
       ZONE_GRAVITY,  /* arg = percent of normal gravity (1..250)           */
       ZONE_DARK,     /* arg = light level the zone is held at              */
       ZONE_NOISE,    /* arg = noise the zone adds / allows                 */
       ZONE_FACE_A,   /* the Wall's face A                                  */
       ZONE_FACE_B,   /* the Wall's face B                                  */
       ZONE_WARD,     /* a naming ward; arg = ward number                   */
       ZONE_CHASE,    /* a chase lane                                       */
       ZONE_SAFE,     /* a safe room                                        */
       ZONE_BOUNCE,   /* bounce pad; arg = rise in height steps             */
       ZONE_WEB,      /* slowing floor                                      */
       ZONE_COUNT };

typedef struct DoomoZone {
    uint8_t x0, y0, x1, y1;   /* cells, inclusive, x0 <= x1, y0 <= y1 */
    uint8_t kind;             /* ZONE_* */
    uint8_t arg;
    uint8_t tag;              /* event / script tag, 0 = none */
} DoomoZone;

/* DoomoPath.kind: how the player may travel it (core/tools/map_bfs.h has
 * the same rules). Append only. */
enum { MPATH_ROUTE,   /* nobody rides it: an escort's or a patrol's route    */
       MPATH_ZIP,     /* zipline: one way, first point to last point         */
       MPATH_RIDE,    /* ride cycle: board and leave at any point            */
       MPATH_RAFT,    /* current: one way, from a point to any later point   */
       MPATH_TUBE,    /* tube link: both ways between first and last point   */
       MPATH_ARC,     /* bounce arc: one way, first point to last point      */
       MPATH_CHASE,   /* the pursuer's lane: nobody rides it                 */
       MPATH_COUNT };

typedef struct DoomoPath {
    uint8_t  id;              /* the number in `path ID ...`, unique per map */
    uint8_t  kind;            /* MPATH_* */
    uint8_t  count;           /* points, 2..DOOMO_PATH_POINTS */
    uint8_t  reserved;
    uint16_t first;           /* index of the first point (x, y byte pairs)  */
} DoomoPath;

/* DoomoSwapSet.mode: who applies the set. */
enum { MSWAP_SCRIPT,  /* only the level script (LV2)                         */
       MSWAP_ONCE,    /* applied when map event `event` fires; it stays      */
       MSWAP_TOGGLE,  /* flipped each time map event `event` fires (levers)  */
       MSWAP_COUNT };

/* One cell override: `x` is (base cell) XOR (swapped cell), byte by byte,
 * so applying a set and reverting it are the same XOR and exact inverses
 * whatever else changed the live cell meanwhile. */
typedef struct DoomoSwapCell {
    uint16_t cell;            /* index into cells[], y * w + x */
    uint8_t  x[8];            /* sizeof(DoomoCell) */
} DoomoSwapCell;

typedef struct DoomoSwapSet {
    uint16_t first;           /* index into DoomoMapExt.swap_cells */
    uint16_t count;
    uint8_t  mode;            /* MSWAP_* */
    uint8_t  event;           /* map event tag (MSWAP_ONCE / MSWAP_TOGGLE)   */
} DoomoSwapSet;

/* The parallel byte per thing (`tflags2=` in a map file). */
enum {
    THF2_ESCORT   = 0x01,  /* the level's escort                           */
    THF2_COLLECT  = 0x02,  /* one of the level's collectible set           */
    THF2_SLEEPER  = 0x04,  /* asleep until noise wakes it                  */
    THF2_SPLITTER = 0x08,  /* splits when it dies                          */
    THF2_FROZEN   = 0x10,  /* frozen at the start                          */
    THF2_USABLE   = 0x20   /* a usable; `arg` is its kind                  */
};

/* DoomoMapExt.flags */
enum {
    MAPX_DARK = 0x01,      /* `dark yes`: a dark level (needs `note`)      */
    MAPX_GBA  = 0x02,      /* `gba yes`: built to the GBA budgets          */
    MAPX_HERO = 0x04       /* `hero LOOK LOADOUT`: the hero override       */
};

/* `detail a,b,c`: what the frame-rate governor drops, first to last.
 * Append only (LV9 owns the meaning). */
enum { MDET_NONE, MDET_FLOORS, MDET_SKY, MDET_PROPS, MDET_FX, MDET_FAR, MDET_LIGHT, MDET_COUNT };

typedef struct DoomoMapExt {
    const DoomoMapDef  *def;
    uint8_t  flags;           /* MAPX_* */
    uint8_t  fog;             /* fog distance in cells, 0 = none             */
    uint8_t  view;            /* view distance in cells, 0 = the default     */
    uint8_t  gravity;         /* percent of normal, 0 = the default (100)    */
    uint16_t note;            /* content note text id, 0 = none              */
    uint8_t  detail[3];       /* MDET_*, the governor's drop order           */
    uint8_t  hero_look, hero_loadout;   /* valid with MAPX_HERO              */
    uint8_t  zone_count, path_count, swap_count, texbank_count;
    const DoomoZone     *zones;
    const DoomoPath     *paths;
    const uint8_t       *path_pts;      /* x, y pairs */
    const DoomoSwapSet  *swaps;
    const DoomoSwapCell *swap_cells;
    const char (*texbank)[DOOMO_TEXBANK_NAME];   /* names of ids 56.. */
    const uint8_t       *tflags2;       /* def->thing_count bytes, or NULL */
} DoomoMapExt;

/* The registry of v2 maps (levels_mapx.c): NULL-terminated. Empty until
 * the first new level lands. */
extern const DoomoMapExt *const doomo_map_exts[];
/* The ext of a definition, NULL for a v1 map (all 12 maps of today). */
const DoomoMapExt *map_ext_of(const DoomoMapDef *d);
/* The first zone of `kind` (ZONE_NONE: any kind) that holds the cell, at
 * or after index `from`; its index, or -1. Walk all with from = i + 1. */
int  map_zone_at(const DoomoMapExt *x, int cx, int cy, int kind, int from);
/* The next zone of `kind` at or after index `from`; its index, or -1. */
int  map_zone_next(const DoomoMapExt *x, int kind, int from);
/* The path with that id, NULL if none. */
const DoomoPath *map_path_by_id(const DoomoMapExt *x, int id);
/* Point `i` of a path as a cell; 0 if i is out of range. */
int  map_path_point(const DoomoMapExt *x, const DoomoPath *p, int i, int *cx, int *cy);
/* The thing's tflags2 byte (0 without a table or out of range). */
int  map_thing_flags2(const DoomoMapExt *x, int thing);
/* Swap sets on the live map. `applied` is the caller's state, one bit per
 * set (it will live in LvRun). Apply does nothing if the set's bit is
 * set, revert nothing if it is clear; both return the number of cells
 * changed (at most DOOMO_SWAP_CELLS_MAX). Revert undoes apply exactly. */
int  map_swap_apply(DoomoMap *m, const DoomoMapExt *x, int set, uint16_t *applied);
int  map_swap_revert(DoomoMap *m, const DoomoMapExt *x, int set, uint16_t *applied);
/* Map event `tag` for the swap sets: applies every MSWAP_ONCE set and
 * flips every MSWAP_TOGGLE set of that event. Returns sets changed. */
int  map_swap_event(DoomoMap *m, const DoomoMapExt *x, int tag, uint16_t *applied);

#endif
