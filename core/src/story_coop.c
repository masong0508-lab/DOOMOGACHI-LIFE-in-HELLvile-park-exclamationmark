/* story_coop.c - MP1 content: the co-op heroes' abilities in play (docs/COOP_HEROES.md).
 * A guest who plays Ember, Rook or Wick gets one passive (an hourly pass while they walk the overworld) and one active (Y, once per
 * game day). The numbers are hero_coop_info()'s: small nudges on the existing 0..100 stats of the household and the town, never a
 * skip of the story, an exorcism or combat. Nothing here touches a story flag. A pool member has no stat record, so only members
 * that are town residents are changed; the rest still get family_care(). */
#include "doomo/doomo.h"
#include "doomo/playas.h"
#include "doomo/profiles.h"
#include "doomo/family.h"
#include "doomo/town.h"
#include "doomo/sim.h"
#include "doomo/ui.h"
#include "doomo/input.h"
#include "story_int.h"
#include "overworld_int.h"

#define NEAR_R (48 * OWQ)                 /* "a few steps" */
#define BIT_USED 12                       /* PROF flag: the active was used at least once (quest byte 1 holds the day) */

static int is_night(void) { int h = town_hour(); return h >= 21 || h < 6; }

static int near_player(int id)
{
    int i;
    if (!ow_scr) return 0;
    for (i = 0; i < ow_npc_count; i++) {
        int32_t dx = ow_npcs[i].x - ow.px, dy = ow_npcs[i].y - ow.py;
        if (ow_npcs[i].kind != NK_RESIDENT || ow_npcs[i].id != id) continue;
        return dx > -NEAR_R && dx < NEAR_R && dy > -NEAR_R && dy < NEAR_R;
    }
    return 0;
}

static DoomoResident *household_res(int id)
{
    DoomoResident *r;
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS || !family_find(id)) return NULL;
    r = &doomo_town.res[id];
    return r->status == RES_ALIVE ? r : NULL;
}

/* Ember: all housemates at night. Rook: villagers near him at night. Wick: housemates near them at night. */
int story_coop_passive(int kind, int night)
{
    const HeroCoopInfo *h = hero_coop_info(kind);
    int id, n = 0, me = playas_avatar();
    if (!h || !night) return 0;
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        DoomoResident *r;
        if (id == me) continue;
        if (kind == HERO_COOP_FIRST) r = household_res(id);
        else if (kind == HERO_COOP_FIRST + 1) { r = &doomo_town.res[id]; if (r->status != RES_ALIVE || !near_player(id)) r = NULL; }
        else { r = household_res(id); if (r && !near_player(id)) r = NULL; }
        if (!r) continue;
        sim_stat_add(r, h->stat[0], h->amount[0]);
        n++;
    }
    return n;
}

/* Ember: feed the whole household. Rook: the nearest housemate rests. Wick: a friend in reach calms down. */
int story_coop_active(int p, int kind)
{
    const HeroCoopInfo *h = hero_coop_info(kind);
    int id, n = 0, me = playas_avatar(), day = doomo_town.day & 0xFF;
    if (!h) return 0;
    if (prof_flag(p, BIT_USED) && prof_quest(p, 1) == day) return -1;
    for (id = 0; id < DOOMO_MAX_RESIDENTS + 0; id++) {
        DoomoResident *r;
        if (id == me) continue;
        if (kind == HERO_COOP_FIRST) r = household_res(id);
        else if (kind == HERO_COOP_FIRST + 1) { r = household_res(id); if (r && !near_player(id)) r = NULL; }
        else { r = &doomo_town.res[id]; if (r->status != RES_ALIVE || r->level < STORY_FRIEND_LEVEL || !near_player(id)) r = NULL; }
        if (!r) continue;
        sim_stat_add(r, h->stat[1], h->amount[1]);
        n++;
        if (kind != HERO_COOP_FIRST) break;          /* Rook and Wick: one person */
    }
    if (kind == HERO_COOP_FIRST) {                   /* the dish is for pool members too: they are cared for */
        for (id = FAMILY_POOL_FIRST; id <= FAMILY_POOL_LAST; id++) if (family_find(id)) { family_care(id); n++; }
    }
    if (n) { prof_flag_set(p, BIT_USED, 1); prof_quest_set(p, 1, day); }
    return n;
}

void story_coop_ability_tick(void)
{
    static int32_t last_hour = -1;
    int a = playas_avatar(), p, kind, n;
    int32_t hr;
    if (a < PLAYAS_ID_FAMILY_FIRST || story_talking()) return;
    p = prof_find_resident(a);
    if (p < 0 || !prof_is_coop(p)) return;
    kind = prof_hero(p);
    hr = (int32_t)doomo_town.day * 24 + town_hour();
    if (hr != last_hour) { last_hour = hr; story_coop_passive(kind, is_night()); }
    if (btn_pressed(BTN_Y)) {
        const HeroCoopInfo *h = hero_coop_info(kind);
        n = story_coop_active(p, kind);
        if (n < 0) ui_toast("Already used today.");
        else if (n == 0) ui_toast("Nobody in reach.");
        else if (h) ui_toast(h->ability[1]);
    }
}
