/* lv_env.h - mechanic kit B, environment and light (owner: levels, LV4; plan 8.3).
 *
 * Fog zones cleared by valves, the glum tide, the lantern and candles, the Wall's two faces, the
 * showtime light swap, collapsing floors and dissolving rooms. Every piece is a rule over the
 * LIVE map and the level's ROM zones (map.h: ZONE_*): no clock, no rng, no allocation, no statics.
 * The caller owns one LvEnv (the mechanic union of the level runtime; 84 B with the pointers) and
 * calls lv_env_tick() once per game tick with the player's cell.
 *
 * What it writes in the live cells, and nothing else: `light` (fog, candles, lantern, showtime),
 * `floor_h` and CELL_WATER (the tide, collapses), `wall` (dissolved rooms). Each of those fields is
 * recomputed from the DEFINITION's cell plus the state ("a cell is rebuilt, never patched"), so
 * a restore is exact whatever happened in between. Doors, lifts and the rest of a cell are left
 * to map.c. Swap sets (the two faces) XOR whole cells: a swap set of an env level may not
 * touch `light` / `floor_h` / `wall` of a cell that one of these zones covers
 * (lv_env_check() refuses it).
 *
 * The code is for the host, GBA and DC builds. The PS1 compiles it out (LV_HAS_NEW is 0 there,
 * no new resident byte before PS1-RAM): see docs/LEVELS.md.
 */
#ifndef DOOMO_LV_ENV_H
#define DOOMO_LV_ENV_H

#include "map.h"

/* tunables (ticks are game ticks: 30 a second) */
#define LV_ENV_VIEW_MIN      8      /* shortest view distance a renderer takes (cells)       */
#define LV_ENV_TIDE_N        4      /* tide zones with state                                   */
#define LV_ENV_TIDE_TICKS    180    /* the sludge rises one step every 6 s while you are in it */
#define LV_ENV_DECAY_N       8      /* collapse / ward zones with a timer                      */
#define LV_ENV_DECAY_DIV     4      /* the timer counts in ticks / 4 (a byte)                  */
#define LV_ENV_DISSOLVE_TICKS 240   /* a room you left unnamed dissolves after 8 s             */
#define LV_ENV_COLLAPSE_TICKS 45    /* a floor you left falls after 1.5 s                      */
#define LV_ENV_OIL_MAX       1800   /* the lantern: 60 s of oil                                */
#define LV_ENV_LAMP_R        2      /* lights the cells within this many (5 x 5 = 25)          */
#define LV_ENV_LAMP_LIGHT    10     /* its light at the centre, 2 less per cell away           */
#define LV_ENV_CANDLE_LIGHT  9      /* a lit candle's cells when the zone's arg is 0           */
#define LV_ENV_FOG_DROP      5      /* light a fog zone takes off                              */
#define LV_ENV_SHOW_TICKS    300    /* showtime cycle: every 10 s ...                          */
#define LV_ENV_SHOW_ON       120    /* ... the lights blaze for 4 s                             */
#define LV_ENV_SHOW_LIGHT    15     /* ... at this light                                       */
#define LV_ENV_FOGWALL       TEX_CLOUD_PUFF   /* the wall a dissolved room turns into          */
#define LV_ENV_USE_RANGE     1      /* a candle is used from inside its zone or this close     */

typedef struct LvEnv {
    const DoomoMapExt *x;
    DoomoMap          *map;
    uint32_t fog_off;          /* bit per zone: this fog zone is cleared                  */
    uint32_t lit;              /* bit per zone: this candle is lit                        */
    uint32_t seen;             /* bit per zone: the player has been in this ward / collapse zone */
    uint32_t gone;             /* bit per zone: this ward dissolved / floor fell          */
    uint32_t named;            /* bit per zone: this ward is named (never dissolves)      */
    uint32_t drained;          /* bit per zone: this tide zone is drained for good        */
    uint16_t tick;
    uint16_t oil;              /* lantern oil left, ticks                                 */
    uint16_t show_t;           /* showtime clock                                          */
    uint16_t applied;          /* swap sets applied by lv_env_face_flip                   */
    uint8_t  lamp_x, lamp_y;   /* where the lantern last lit, 255 = off                   */
    uint8_t  tide_t;           /* ticks spent in a tide zone since the last step          */
    uint8_t  face;             /* 0 = face A, 1 = face B                                  */
    uint8_t  show_on;          /* the lights are up                                       */
    int8_t   tide[LV_ENV_TIDE_N];       /* surface height of tide zone k (steps)          */
    uint8_t  decay[LV_ENV_DECAY_N];     /* ward / collapse zone k: ticks / 4 left, 255 = not counting */
} LvEnv;

/* lv_env_check results (0 = fine) */
enum { LVENV_OK, LVENV_TIDE_MANY, LVENV_DECAY_MANY, LVENV_SWAP_OWNED, LVENV_TIDE_ARG };

/* ---- the level header ---- */
/* The view distance a level asks of the renderers, for DoomoView3D.view_dist: 0 = the renderer's own.
 * `view N` of the header, clamped to LV_ENV_VIEW_MIN.. . NULL (a v1 map): 0. */
int  lv_env_view(const DoomoMapExt *x);

/* ---- the run ---- */
int  lv_env_check(const DoomoMapExt *x);
/* Start: clears the state, fills the lantern, reads the tide basins' bottoms. 0 = fine, else LVENV_*. */
int  lv_env_begin(LvEnv *e, DoomoMap *map, const DoomoMapExt *x);
/* One game tick. (cx, cy) = the player's cell (cx < 0: nobody). Returns the number of live cells
 * rewritten this tick. */
int  lv_env_tick(LvEnv *e, int cx, int cy);
/* The view distance for this frame: the level's, cut by a fog zone the player stands in
 * (its arg), never under LV_ENV_VIEW_MIN. 0 = the renderer's own. */
int  lv_env_view_at(const LvEnv *e, int cx, int cy);
/* A hash of the whole state: two runs with the same inputs must agree. */
uint32_t lv_env_hash(const LvEnv *e);

/* ---- fog and valves ---- */
/* Clear (or bring back) every fog zone with that tag. Returns zones changed. */
int  lv_env_fog_set(LvEnv *e, int tag, int on);
/* 1 if an uncleared fog zone holds the cell. */
int  lv_env_fog_at(const LvEnv *e, int cx, int cy);

/* ---- the tide ---- */
/* The valve with that tag drains every tide zone with it (the sludge goes down and stays down).
 * Returns zones drained. */
int  lv_env_drain(LvEnv *e, int tag);
/* The sludge surface of the first tide zone with that tag, in height steps; -128 if none. */
int  lv_env_tide_level(const LvEnv *e, int tag);

/* ---- lantern and candles ---- */
/* A on a candle: lights the candle zone the cell is in (or next to). Returns its zone number + 1,
 * 0 if there is none or it was lit already. */
int  lv_env_candle_use(LvEnv *e, int cx, int cy);
int  lv_env_candles_lit(const LvEnv *e);
int  lv_env_candles_total(const LvEnv *e);
/* An oil jar: fills the lantern. */
void lv_env_refill(LvEnv *e);
/* 1 while the lantern burns (oil left). */
int  lv_env_lantern_on(const LvEnv *e);

/* ---- the two faces ---- */
/* Flip the Wall: apply (or revert) swap set `set` and switch the face. Returns cells changed. */
int  lv_env_face_flip(LvEnv *e, int set);
/* Which face a cell belongs to: 0 = both, 1 = A, 2 = B. */
int  lv_env_face_of(const LvEnv *e, int cx, int cy);
/* 1 if the cell belongs to the face that is not showing (its actors go translucent and cannot be hurt). */
int  lv_env_out_of_phase(const LvEnv *e, int cx, int cy);

/* ---- showtime ---- */
/* 1 while the stage lights are up: animatronics freeze and cannot be hurt. */
int  lv_env_showtime(const LvEnv *e);

/* ---- collapse and dissolving rooms ---- */
/* Name a ward: zone of kind ZONE_WARD with arg == ward. It never dissolves, and a dissolved
 * one comes back. Returns 1 if something changed. */
int  lv_env_name(LvEnv *e, int ward);
/* 1 if the ward / collapse zone holding the cell is gone. */
int  lv_env_gone_at(const LvEnv *e, int cx, int cy);

#endif
