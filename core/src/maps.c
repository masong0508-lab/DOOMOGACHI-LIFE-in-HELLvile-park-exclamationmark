/* maps.c - the map registry (owner: action).
 *
 * Every map is compiled from core/data/maps/<name>.txt by tools/mapc.py
 * into core/gen/map_<name>.c, which defines `map_def_<name>`. The registry
 * references them WEAKLY: a map that has not been authored yet (e.g. a
 * story overworld map while that module is landing) is simply NULL here,
 * and map_load() falls back to a small built-in room.
 */
#include "doomo/map.h"

#define WEAKREF __attribute__((weak))

extern const DoomoMapDef map_def_hahaland WEAKREF;
extern const DoomoMapDef map_def_hahaland_invaded WEAKREF;
extern const DoomoMapDef map_def_hellvile WEAKREF;
extern const DoomoMapDef map_def_vape_cellar WEAKREF;
extern const DoomoMapDef map_def_cloud_nine_spire WEAKREF;
extern const DoomoMapDef map_def_arena WEAKREF;
extern const DoomoMapDef map_def_park_united WEAKREF;
/* (levels expansion, additive) */
extern const DoomoMapDef map_def_sugar_rush_sewers WEAKREF;
extern const DoomoMapDef map_def_plushie_factory WEAKREF;
extern const DoomoMapDef map_def_graveyard_shift WEAKREF;
extern const DoomoMapDef map_def_bone_cathedral WEAKREF;
extern const DoomoMapDef map_def_hellvile_underbelly WEAKREF;

const DoomoMapDef *const doomo_maps[MAPID_COUNT] = {
    &map_def_hahaland,          /* MAPID_HAHALAND          (story)  */
    &map_def_hahaland_invaded,  /* MAPID_HAHALAND_INVADED  (story)  */
    &map_def_hellvile,          /* MAPID_HELLVILE          (story)  */
    &map_def_vape_cellar,       /* MAPID_VAPE_CELLAR       (action) */
    &map_def_cloud_nine_spire,  /* MAPID_CLOUD_NINE_SPIRE  (action) */
    &map_def_arena,             /* MAPID_ARENA             (action) */
    &map_def_park_united,       /* MAPID_PARK_UNITED       (story)  */
    &map_def_sugar_rush_sewers,     /* MAPID_SUGAR_RUSH_SEWERS     (levels) */
    &map_def_plushie_factory,       /* MAPID_PLUSHIE_FACTORY       (levels) */
    &map_def_graveyard_shift,       /* MAPID_GRAVEYARD_SHIFT       (levels) */
    &map_def_bone_cathedral,        /* MAPID_BONE_CATHEDRAL        (levels) */
    &map_def_hellvile_underbelly    /* MAPID_HELLVILE_UNDERBELLY   (levels) */
};
