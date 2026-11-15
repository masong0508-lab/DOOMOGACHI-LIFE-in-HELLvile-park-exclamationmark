/* sim_nudge.c - TM2e: the sim hook Talk uses to send a resident somewhere (design TALK_MULTILEVEL 6.1).
 * Owner: SIM (added by TALK, small on purpose). Only talk_outcome.c calls it, so a console build that links no
 * Talk drops it with --gc-sections; it keeps no state of its own (the activity is the resident's own field and
 * decide_activity() chooses again at the next hour change). */
#include "doomo/doomo.h"
#include "doomo/sim.h"
#include "doomo/town.h"
#include "doomo/story.h"
#include "doomo/resident_acc.h"
#include "sim_int.h"

int sim_nudge_ok(int id, int act)
{
    const DoomoResident *r = id >= 0 && id < DOOMO_MAX_RESIDENTS ? town_res(id) : NULL;
    int night = town_is_night(), hell;
    if (!r || r->status != RES_ALIVE) return 0;           /* not a ghost, a possessed or a lost soul either */
    switch (r->activity) {                                 /* what they must not be pulled out of */
    case ACT_SLEEPING: case ACT_HOSPITAL: case ACT_HAUNTING: return 0;
    default: break;
    }
    if (r->stat[STAT_ENERGY] < 12 || r->stat[STAT_HEALTH] < 25) return 0;    /* decide_activity would send them back to bed or the hospital */
    hell = RES_DISTRICT(id) || (r->flags & RF_HELLBORN);
    switch (act) {
    case ACT_PARK: case ACT_SHOP: case ACT_WORKSHOP: case ACT_BEACH: if (night) return 0; break;
    case ACT_CAFE: case ACT_CHURCH: case ACT_CONCERT: break;
    case ACT_VAPE_SHOP:                                    /* the same story gate as decide_activity() */
        if (doomo_town.mode == TOWN_STORY && doomo_town.story.chapter < 2 && !hell) return 0;
        break;
    default: return 0;
    }
    return 1;
}

int sim_nudge_activity(int id, int act, int minutes)
{
    if (minutes <= 0 || !sim_nudge_ok(id, act)) return 0;
    sim_ensure_init();
    town_res(id)->activity = (uint8_t)act;
    return 1;
}

int sim_favourite_venue(int id)
{
    const DoomoResident *r = id >= 0 && id < DOOMO_MAX_RESIDENTS ? town_res(id) : NULL;
    return r ? doomo_hobby_venue[RES_LIKE_HOBBY(r) % HOBBY_COUNT] : ACT_HOME;
}

int sim_can_raise_problem(int id)
{
    const DoomoResident *r = id >= 0 && id < DOOMO_MAX_RESIDENTS ? town_res(id) : NULL;
    return r && r->status != RES_EMPTY && r->status != RES_DEAD && sim_problem_find(id, -1) < 0 &&
           sim_s.nprob < SIM_MAX_PROBLEMS;
}
