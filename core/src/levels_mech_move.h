/* levels_mech_move.h - mechanic kit A, movement (owner: levels, LV3; levels plan 8.3).
 *
 * Ride cycles (HB10), ziplines (HB14), bounce pads and low gravity (HB16), two-handed carry (N1),
 * pneumatic tubes (N2) and the raft (N5). Pure rules on a DoomoMapExt (zones and paths of map format
 * v2, map.h) and one small state block, `MmMove`: no statics, no allocation, nothing drawn. A level
 * without these zones never calls any of it, so it pays nothing per frame; a function that finds
 * nothing to do returns at once.
 *
 * Units: a distance along a path is in 1/16 cell (`dist`), a position is returned in 1/256 cell, a
 * height in world units (DOOMO_HEIGHT_STEP is 8). One tick is one 30 fps frame.
 *
 * The rules match core/tools/map_bfs.h edge for edge (that file is what proves a level can be
 * finished): `mm_edges` lists the same edges as the search, so the autopilot (combat_bot.c, a
 * follow-up: see docs/design/held/status/LEVELS.md) and the BFS cannot disagree. The player's own
 * ride state is an `MmMove` kept by the levels runtime (LvRun mechanic union, 16 bytes).
 *
 * Not part of the contract. On the PS1 nothing links until a level calls it (LV_HAS_NEW is 0 there).
 */
#ifndef DOOMO_LEVELS_MECH_MOVE_H
#define DOOMO_LEVELS_MECH_MOVE_H

#include "doomo/map.h"

/* MmMove.mode */
enum { MM_ON_FOOT, MM_RIDE, MM_ZIP, MM_BOUNCE, MM_TUBE, MM_RAFT };

#define MM_U            16      /* path distance units per cell */
#define MM_TUBE_TICKS   12      /* the whoosh (plan 8.3: a 12-tick fade) */
#define MM_ZIP_SPEED    3       /* 1/16 cell per tick: 5.6 cells a second */
#define MM_RIDE_SPEED   1       /* the default speed of a ride cycle (1/16 cell per tick) */
#define MM_RAFT_SPEED   2       /* the current's drift */
#define MM_RAFT_SIDE    12      /* sideways limit, 1/16 cell: past it the raft capsizes */
#define MM_BOUNCE_AIR   18      /* air time of a pad at normal gravity, ticks */
#define MM_CARRY_SLOW   3       /* a carrying player walks at MM_CARRY_SLOW / 4 speed */

typedef struct MmMove {
    uint8_t  mode;        /* MM_*                                                   */
    uint8_t  path;        /* index into ext->paths of the zip / ride / tube / raft / arc in use */
    uint16_t dist;        /* 1/16 cell along that path (raft: along its current)    */
    uint8_t  t;           /* ticks in the mode (tube whoosh, bounce air time)        */
    uint8_t  air;         /* bounce: total air time, ticks                          */
    int8_t   side;        /* raft: sideways offset from the current, 1/16 cell       */
    uint8_t  buoy;        /* raft: the last point passed (capsizing puts you there)  */
    uint8_t  carry;       /* two-handed carry: the item id held, 0 = hands free      */
    uint8_t  lever;       /* tube junction levers, one bit each                      */
    uint8_t  stopped;     /* the ride cycles are stopped by a booth (1)              */
    uint8_t  rise;        /* bounce: peak height in height steps                     */
    uint16_t clock;       /* ride cycles: the ticks they have run                    */
    uint8_t  cx, cy;      /* tube: the station the whoosh ends at                    */
} MmMove;                 /* 16 bytes */

/* A way the player can get from one cell to another by one of the mechanics. */
typedef struct MmEdge {
    uint8_t to_x, to_y;
    uint8_t kind;         /* MPATH_* (a zone bounce is MPATH_ARC) */
    uint8_t path;         /* path index */
    uint16_t ticks;       /* how long the trip takes */
} MmEdge;

void mm_reset(MmMove *m);

/* ---- paths ---- */
/* Length of a path in 1/16 cell (a diagonal step counts max + min / 2). */
int  mm_path_len(const DoomoMapExt *x, const DoomoPath *p);
/* The position `dist` (1/16 cell) along the path, in 1/256 cell. A dist past the end is the last point. */
void mm_path_pos(const DoomoMapExt *x, const DoomoPath *p, int dist, int *px256, int *py256);
/* Path index of `p` in x->paths. */
int  mm_path_index(const DoomoMapExt *x, const DoomoPath *p);

/* ---- gravity ---- */
/* Percent of normal gravity at a cell: a ZONE_GRAVITY zone, else the map's `gravity`, else 100. */
int  mm_gravity_at(const DoomoMapExt *x, int cx, int cy);
/* The highest step the player gets onto at that gravity, world units (the BFS's `rise`). */
int  mm_step_rise(const DoomoMapExt *x, int cx, int cy);
/* Height above the floor, world units, of a hop of `rise` height steps at `t` of `air` ticks (a parabola). */
int  mm_arc_height(int rise, int air, int t);

/* ---- ride cycles ---- */
/* The platform of a ride path (MPATH_RIDE) after the machine's clock, in 1/256 cell. It runs the path
 * out and back. `speed` is 1/16 cell per tick, 0 = MM_RIDE_SPEED. */
void mm_ride_platform(const DoomoMapExt *x, const DoomoPath *p, int clock, int speed, int *px256, int *py256);
/* Advance the cycle clock by one tick (the levels runtime calls it only for a map with a ride path).
 * One clock runs every ride; a booth stops all of them at once. */
void mm_ride_tick(MmMove *m);
/* A booth (A) stops or starts the rides. */
void mm_ride_stop(MmMove *m, int stopped);
/* Step onto the ride whose platform is in cell (cx, cy) now: the path index, or -1. */
int  mm_ride_board(const DoomoMapExt *x, MmMove *m, int cx, int cy);

/* ---- ziplines ---- */
/* Hang on the zipline whose first point is the cell (cx, cy): the path index, or -1. */
int  mm_zip_board(const DoomoMapExt *x, MmMove *m, int cx, int cy);

/* ---- bounce pads ---- */
/* Step onto a ZONE_BOUNCE cell: starts the hop (along the zone's arc path, if one starts in the zone). Returns 1. */
int  mm_bounce_start(const DoomoMapExt *x, MmMove *m, int cx, int cy);

/* ---- tubes ---- */
/* The station a tube sends (cx, cy) to: writes it and returns the path index, or -1 if (cx, cy) is no
 * station. A station on several tubes takes the one its lever picks (bit 0 of the lever count). */
int  mm_tube_dest(const DoomoMapExt *x, const MmMove *m, int cx, int cy, int *to_x, int *to_y);
/* Climb into the tube at (cx, cy). Returns the path index or -1. */
int  mm_tube_enter(const DoomoMapExt *x, MmMove *m, int cx, int cy);
/* 0..255: how black the screen is `t` ticks into a whoosh (up, then down). */
int  mm_tube_fade(int t);
/* A junction lever (script or A on a lever): flips bit `n` (0..7). */
void mm_tube_lever(MmMove *m, int n);

/* ---- the raft ---- */
/* Step onto a raft current at one of its points (the BFS's rule: from there to any later point). Returns the path index or -1. */
int  mm_raft_board(const DoomoMapExt *x, MmMove *m, int cx, int cy);

/* ---- carry ---- */
int  mm_carry_take(MmMove *m, int item);          /* 1 if the hands were free */
int  mm_carry_drop(MmMove *m);                    /* the item that was held (0: none) */
/* Held in both hands: no weapon, slow walk. */
int  mm_carry_blocks_fire(const MmMove *m);
/* The tag of the first tagged zone over the cell: where a dropped item is laid (0 = no zone: it is dropped on the floor). */
int  mm_carry_lay_tag(const DoomoMapExt *x, int cx, int cy);

/* ---- the tick ---- */
/* One tick of whatever the player is doing (the hop, the zipline, the tube, the raft, a ride). `steer` is
 * the stick's sideways push (-1, 0, +1; raft) and `off` asks to get off (zipline, ride, raft). Writes
 * the position in 1/256 cell (-1: leave the player where they are: the tube's first half, a hop with no
 * arc) and the height above the floor in world units. Returns 1 while the move
 * goes on, 0 on the tick it ends (the position is where the player lands) or when the player is on foot. */
int  mm_tick(const DoomoMapExt *x, MmMove *m, int steer, int off, int *px256, int *py256, int *height);

/* ---- the autopilot's edges ---- */
/* Every mechanic edge out of cell (cx, cy), the same ones map_bfs.h follows. Returns how many were
 * written (at most max). A level without a table returns 0 at once. */
int  mm_edges(const DoomoMapExt *x, int cx, int cy, MmEdge *out, int max);

#endif
