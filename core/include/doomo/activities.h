/* activities.h - registry of optional game modes / minigames / collections
 * that expansion modules add (owner: expansion). Self-registering, like the
 * unlocks registry: each expansion module defines its own NULL-terminated
 * table (core/src/activities.c has weak empty defaults) and the free play
 * island (fp_*.c) + story overworld list whatever is registered, by
 * location. Adding a new activity never requires editing the hub scenes.
 */
#ifndef DOOMO_ACTIVITIES_H
#define DOOMO_ACTIVITIES_H

#include "types.h"
#include "scene.h"

/* Where an activity lives on the island / overworld. */
enum {
    ACTLOC_FUN_FAIR,      /* new: minigame fair on the Hillvile side       */
    ACTLOC_CONCERT_HALL,  /* Tomodachi-style concerts                      */
    ACTLOC_LAKE,          /* Lake of Fire (beach): fishing etc.            */
    ACTLOC_CAFE,          /* Brimstone Bites: cooking                      */
    ACTLOC_LIBRARY,       /* new: collections, bestiary, almanac, album    */
    ACTLOC_APARTMENTS,    /* things you do at a citizen's home (dreams)    */
    ACTLOC_ARENA,         /* extra combat modes                            */
    ACTLOC_HELLVILE,      /* activities on the bad side of the park        */
    ACTLOC_TOWN_HALL,
    ACTLOC_COUNT
};
extern const char *const doomo_actloc_names[ACTLOC_COUNT];

enum { ACTF_NEEDS_RESIDENT = 1,   /* scene arg = a resident id the hub picks */
       ACTF_FREEPLAY = 2,         /* offered in free play                     */
       ACTF_STORY = 4,            /* offered in story mode (after chapter req) */
       ACTF_NIGHT_ONLY = 8,
       ACTF_DAY_ONLY = 16 };

typedef struct DoomoActivity {
    const char       *name;       /* "Cupcake Catch"                       */
    const char       *desc;       /* one line, <= 40 chars                 */
    uint8_t           location;   /* ACTLOC_*                              */
    uint8_t           flags;      /* ACTF_*                                */
    uint8_t           icon;       /* ICON_* glyph                          */
    uint8_t           min_chapter;/* story chapter needed (0 = always)     */
    uint16_t          unlock_id;  /* unlocks.h id, 0xFFFF = always open    */
    const DoomoScene *scene;
    int               arg;        /* passed to scene enter (unless NEEDS_RESIDENT) */
} DoomoActivity;
#define ACT_ALWAYS 0xFFFF

/* Registry (activities.c). */
int  activity_count(void);
const DoomoActivity *activity_at(int i);
/* Is it available right now (mode, chapter, unlock, time of day)? */
int  activity_available(const DoomoActivity *a);
/* Collect available activities at a location into out[] (max n). */
int  activities_at(int location, const DoomoActivity **out, int n);
/* Launch it (handles ACTF_NEEDS_RESIDENT by opening a citizen picker). */
void activity_launch(const DoomoActivity *a);

/* Expansion tables (weak NULL defaults in activities.c). */
extern const DoomoActivity *const doomo_activities_minigames;
extern const DoomoActivity *const doomo_activities_concert;
extern const DoomoActivity *const doomo_activities_collections;
extern const DoomoActivity *const doomo_activities_combat;
extern const DoomoActivity *const doomo_activities_extra;
extern const DoomoActivity *const doomo_activities_studio;   /* Music Studio */

#endif
