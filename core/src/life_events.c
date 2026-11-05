/* life_events.c - LS3, the life event engine (owner: life; doomo/life_events.h). */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/life.h"
#include "doomo/life_events.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/sim.h"
#include "doomo/items.h"
#include "doomo/town.h"
#include "doomo/story.h"
#include "sim_int.h"

typedef struct LevRow {
    uint8_t weight;      /* relative chance among the events that can run today */
    uint8_t cooldown;    /* town days before it can run again                   */
} LevRow;

static const LevRow rows_[LEV_COUNT] = {
    /* POTLUCK        */ { 10, 6 },
    /* PICNIC         */ { 10, 5 },
    /* SURPRISE_PARTY */ {  6, 8 },
    /* ANNIVERSARY    */ {  6, 12 },
    /* LUCKY_FIND     */ { 12, 4 },
    /* LETTER         */ {  8, 7 },
    /* RAINY_DAY      */ {  8, 6 },
    /* BAD_DAY        */ {  6, 7 },
    /* TALENT_SHOW    */ {  5, 9 },
    /* STARGAZING     */ {  6, 9 },
    /* BEDTIME_STORY  */ {  9, 4 },
    /* HOMEWORK_HELP  */ {  8, 5 },
    /* OLD_STORIES    */ {  7, 8 },
    /* PET_TRICK      */ {  8, 6 },
    /* MOVIE_NIGHT    */ {  7, 7 },
    /* BAKE_OFF       */ {  5, 10 },
    /* HELPING_HAND   */ {  6,  5 },
    /* AWAY_TRIP      */ {  5,  6 }
};

static uint8_t cool_[LEV_COUNT];
static uint16_t total_;
static int8_t last_ = -1;

/* ---- who an event can touch --------------------------------------------------- */
/* Housemates an event may pick: not pets, not the avatar you are playing. */
static int people_(int *ids, int max, int kids_only)
{
    int i, n = 0, cnt = family_count();
    for (i = 0; i < cnt && n < max; i++) {
        int id = family_member(i);
        const FamilyMember *m = id >= 0 ? family_find(id) : NULL;
        if (!m || m->role == FAMROLE_PET || id == playas_avatar()) continue;
        if (kids_only && m->stage != LIFE_CHILD && m->stage != LIFE_TEEN) continue;
        ids[n++] = id;
    }
    return n;
}

static int pick_(int kids_only)
{
    int ids[FAMILY_MAX], n = people_(ids, FAMILY_MAX, kids_only);
    return n ? ids[sim_rand(n)] : -1;
}

/* A pet to pick (LS4): family members whose role is PET. */
static int pets_(int *ids, int max)
{
    int i, n = 0, cnt = family_count();
    for (i = 0; i < cnt && n < max; i++) {
        int id = family_member(i);
        const FamilyMember *m = id >= 0 ? family_find(id) : NULL;
        if (m && m->role == FAMROLE_PET) ids[n++] = id;
    }
    return n;
}

static int pick_pet_(void)
{
    int ids[FAMILY_MAX], n = pets_(ids, FAMILY_MAX);
    return n ? ids[sim_rand(n)] : -1;
}

/* An elder housemate (LS4): never a pet, never the avatar. */
static int pick_elder_(void)
{
    int ids[FAMILY_MAX], n = people_(ids, FAMILY_MAX, 0), i, e[FAMILY_MAX], ne = 0;
    for (i = 0; i < n; i++) {
        const FamilyMember *m = family_find(ids[i]);
        if (m && m->stage == LIFE_ELDER) e[ne++] = ids[i];
    }
    return ne ? e[sim_rand(ne)] : -1;
}

static int partner_member_(void)
{
    int p = life_player_partner();
    return p >= 0 && family_find(p) ? p : -1;
}

/* ---- the table's conditions ------------------------------------------------------ */
static int ok_(int ev)
{
    int ids[FAMILY_MAX];
    switch (ev) {
    case LEV_POTLUCK: case LEV_RAINY_DAY: return people_(ids, FAMILY_MAX, 0) >= 2;
    case LEV_PICNIC: case LEV_BAD_DAY: case LEV_TALENT_SHOW: case LEV_STARGAZING:
        return people_(ids, FAMILY_MAX, 0) >= 1;
    case LEV_SURPRISE_PARTY: return people_(ids, FAMILY_MAX, 1) >= 1;
    case LEV_ANNIVERSARY: return partner_member_() >= 0;
    case LEV_LUCKY_FIND: case LEV_LETTER: return family_count() >= 1;
    case LEV_BEDTIME_STORY: return people_(ids, FAMILY_MAX, 1) >= 1 && people_(ids, FAMILY_MAX, 0) >= 2;
    case LEV_HOMEWORK_HELP: return people_(ids, FAMILY_MAX, 1) >= 1;
    case LEV_OLD_STORIES: return pick_elder_() >= 0;
    case LEV_PET_TRICK: return pets_(ids, FAMILY_MAX) >= 1;
    case LEV_MOVIE_NIGHT: case LEV_BAKE_OFF: return people_(ids, FAMILY_MAX, 0) >= 2;
    case LEV_HELPING_HAND: return family_helper_count(0) >= 1;
    case LEV_AWAY_TRIP: return family_away_id() < 0 && family_helper_count(1) >= 1;
    default: return 0;
    }
}

int life_event_can(int ev)
{
    return ev >= 0 && ev < LEV_COUNT && cool_[ev] == 0 && ok_(ev);
}

/* ---- the effects --------------------------------------------------------------- */
static void news_(const char *fmt, const char *name)
{
    char msg[NEWS_LEN];
    if (family_freewill_level() == FAMILY_WILL_SUBCONSCIOUS) return;
    if (!name) { sim_news_push(fmt); return; }     /* a line with no name in it */
    doomo_snprintf(msg, sizeof(msg), fmt, name);
    sim_news_push(msg);
}

static void everyone_mood_(int d)
{
    int ids[FAMILY_MAX], n = people_(ids, FAMILY_MAX, 0), i;
    for (i = 0; i < n; i++) family_mood_add(ids[i], d);
}

static void run_(int ev)
{
    int id;
    switch (ev) {
    case LEV_POTLUCK:
        everyone_mood_(2);
        news_("The neighbours threw a potluck. Everyone left full and happy!", NULL);
        break;
    case LEV_PICNIC:
        id = pick_(0);
        family_bond_add(id, 1); family_mood_add(id, 2);
        news_("%s asked for a picnic by the pond. Sandwiches for all!", family_name(id));
        break;
    case LEV_SURPRISE_PARTY:
        id = pick_(1);
        family_bond_add(id, 1); family_mood_add(id, 5);
        inv_add(ITEM_CUPCAKE, 1);
        news_("%s got a surprise party! There are cupcakes.", family_name(id));
        break;
    case LEV_ANNIVERSARY:
        id = partner_member_();
        family_bond_add(id, 1); family_mood_add(id, 3);
        news_("You and %s remembered your anniversary. Aww.", family_name(id));
        break;
    case LEV_LUCKY_FIND:
        doomo_town.money += 20u + (uint32_t)sim_rand(41);
        news_("You found some Hell-Bucks down the back of the sofa.", NULL);
        break;
    case LEV_LETTER:
        inv_add(ITEM_FLOWERS, 1);
        news_("A fan letter arrived with a pressed flower inside.", NULL);
        break;
    case LEV_RAINY_DAY:
        everyone_mood_(1);
        news_("It rained all day, so it was board games indoors.", NULL);
        break;
    case LEV_BAD_DAY:
        id = pick_(0);
        family_mood_add(id, -3);
        news_("%s had a rotten day. A hug would help.", family_name(id));
        break;
    case LEV_TALENT_SHOW:
        id = pick_(0);
        family_mood_add(id, 3); family_bond_add(id, 1);
        news_("%s put on a talent show in the living room!", family_name(id));
        break;
    case LEV_STARGAZING:
        id = pick_(0);
        family_mood_add(id, 2); family_bond_add(id, 1);
        news_("%s and you watched the embers fall from the sky.", family_name(id));
        break;
    case LEV_BEDTIME_STORY:
        id = pick_(1);
        family_bond_add(id, 1); family_mood_add(id, 3);
        news_("%s got a bedtime story and was out before the dragon.", family_name(id));
        break;
    case LEV_HOMEWORK_HELP:
        id = pick_(1);
        family_bond_add(id, 2);
        news_("You helped %s with homework. Mostly brimstone.", family_name(id));
        break;
    case LEV_OLD_STORIES:
        id = pick_elder_();
        family_mood_add(id, 3); family_bond_add(id, 1);
        everyone_mood_(1);
        news_("%s told the old stories again. Nobody minded.", family_name(id));
        break;
    case LEV_PET_TRICK:
        id = pick_pet_();
        family_mood_add(id, 3);
        news_("%s learned a new trick! Nobody knows why.", family_name(id));
        break;
    case LEV_MOVIE_NIGHT:
        everyone_mood_(2);
        news_("Movie night! The popcorn died during the titles.", NULL);
        break;
    case LEV_BAKE_OFF:
        id = pick_(0);
        family_mood_add(id, 2); family_bond_add(id, 1);
        inv_add(ITEM_CUPCAKE, 1);
        news_("%s won the bake-off. The judges were bribed.", family_name(id));
        break;
    case LEV_HELPING_HAND:                      /* family_help_do shows its own news line */
        family_help_do(family_helper_pick(0));
        break;
    case LEV_AWAY_TRIP:
        family_away_start(family_helper_pick(1));
        break;
    default: break;
    }
}

int life_event_fire(int ev)
{
    if (!life_event_can(ev)) return 0;
    run_(ev);
    cool_[ev] = rows_[ev].cooldown;
    if (total_ < 0xFFFFu) total_++;
    last_ = (int8_t)ev;
    return 1;
}

int life_events_daily(void)
{
    int ev, w, tot = 0, r, wts[LEV_COUNT];
    for (ev = 0; ev < LEV_COUNT; ev++) if (cool_[ev]) cool_[ev]--;
    if (family_count() < 1) return -1;                     /* no household: no RNG draw */
    if (doomo_town.mode == TOWN_STORY && story_chapter() != STORY_CH_DONE) return -1;
    if (!sim_chance(LEV_DAILY_PCT)) return -1;
    for (ev = 0; ev < LEV_COUNT; ev++) {
        wts[ev] = life_event_can(ev) ? rows_[ev].weight : 0;
        tot += wts[ev];
    }
    if (!tot) return -1;
    r = sim_rand(tot);
    for (ev = 0; ev < LEV_COUNT; ev++) {
        w = wts[ev];
        if (r < w) { life_event_fire(ev); return ev; }
        r -= w;
    }
    return -1;
}

int life_event_cooldown(int ev) { return ev >= 0 && ev < LEV_COUNT ? cool_[ev] : 0; }
int life_event_total(void) { return total_; }
int life_event_last(void) { return last_; }

/* ---- the LIFE chunk's section: [total u16 LE][cooldown x LEV_COUNT] ---------------- */
#define LEV_SEC_SIZE (2u + LEV_COUNT)

uint32_t life_events_save_size(void)
{
    int i;
    if (total_) return LEV_SEC_SIZE;
    for (i = 0; i < LEV_COUNT; i++) if (cool_[i]) return LEV_SEC_SIZE;
    return 0;
}

uint32_t life_events_save(uint8_t *dst, uint32_t cap)
{
    if (cap < LEV_SEC_SIZE) return 0;
    dst[0] = (uint8_t)total_;
    dst[1] = (uint8_t)(total_ >> 8);
    memcpy(dst + 2, cool_, LEV_COUNT);
    return LEV_SEC_SIZE;
}

int life_events_load(const uint8_t *src, uint32_t len)
{
    int i;
    if (len < 2) return 0;
    life_events_clear();
    total_ = (uint16_t)(src[0] | (src[1] << 8));
    for (i = 0; i < LEV_COUNT && (uint32_t)(2 + i) < len; i++) {
        uint8_t c = src[2 + i];
        cool_[i] = c > rows_[i].cooldown ? rows_[i].cooldown : c;   /* never longer than the table says */
    }
    return 1;
}

void life_events_clear(void)
{
    memset(cool_, 0, sizeof(cool_));
    total_ = 0;
    last_ = -1;
}
