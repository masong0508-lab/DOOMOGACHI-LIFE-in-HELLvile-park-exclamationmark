/* activities.c - the activities registry (see activities.h). */
#include "doomo/activities.h"
#include "doomo/town.h"
#include "doomo/unlocks.h"
#include "doomo/scenes.h"

const char *const doomo_actloc_names[ACTLOC_COUNT] = {
    "Fun Fair", "Concert Hall", "Lake of Fire", "Brimstone Bites", "Library",
    "Apartments", "Arena", "Hellvile", "Town Hall"
};

/* Weak empty defaults; expansion modules provide strong definitions. */
__attribute__((weak)) const DoomoActivity *const doomo_activities_minigames = 0;
__attribute__((weak)) const DoomoActivity *const doomo_activities_concert = 0;
__attribute__((weak)) const DoomoActivity *const doomo_activities_collections = 0;
__attribute__((weak)) const DoomoActivity *const doomo_activities_combat = 0;
__attribute__((weak)) const DoomoActivity *const doomo_activities_extra = 0;

static const DoomoActivity *table(int t) {
    switch (t) {
    case 0: return doomo_activities_minigames;
    case 1: return doomo_activities_concert;
    case 2: return doomo_activities_collections;
    case 3: return doomo_activities_combat;
    case 4: return doomo_activities_extra;
    default: return 0;
    }
}
#define ACT_TABLES 5

int activity_count(void) {
    int n = 0, t;
    for (t = 0; t < ACT_TABLES; t++) {
        const DoomoActivity *a = table(t);
        while (a && a->name) { n++; a++; }
    }
    return n;
}

const DoomoActivity *activity_at(int i) {
    int t;
    if (i < 0) return 0;
    for (t = 0; t < ACT_TABLES; t++) {
        const DoomoActivity *a = table(t);
        while (a && a->name) {
            if (i-- == 0) return a;
            a++;
        }
    }
    return 0;
}

int activity_available(const DoomoActivity *a) {
    if (!a || !a->scene) return 0;
    if (doomo_town.mode == TOWN_STORY) {
        if (!(a->flags & ACTF_STORY)) return 0;
        if (doomo_town.story.chapter < a->min_chapter) return 0;
    } else if (!(a->flags & ACTF_FREEPLAY)) {
        return 0;
    }
    if (a->unlock_id != ACT_ALWAYS && !unlock_has(a->unlock_id)) return 0;
    if ((a->flags & ACTF_NIGHT_ONLY) && !town_is_night()) return 0;
    if ((a->flags & ACTF_DAY_ONLY) && town_is_night()) return 0;
    return 1;
}

int activities_at(int location, const DoomoActivity **out, int n) {
    int i, k = 0, total = activity_count();
    for (i = 0; i < total && k < n; i++) {
        const DoomoActivity *a = activity_at(i);
        if (a->location == location && activity_available(a)) out[k++] = a;
    }
    return k;
}

/* Hub scenes may override this (e.g. to open their citizen picker); the
 * default pushes the scene with arg, or with resident 0 for resident acts. */
__attribute__((weak)) void activity_launch(const DoomoActivity *a) {
    if (!a || !a->scene) return;
    scene_push(a->scene, (a->flags & ACTF_NEEDS_RESIDENT) ? 0 : a->arg);
}
