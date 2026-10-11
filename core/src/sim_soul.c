/* sim_soul.c - corruption, night possession, the possessor's true name,
 * laying citizens to rest and raising ghosts (owner: life).
 *
 * Corruption creeps up at night: vape exposure (Cloud Nine Vapor), the
 * Hellvile air for its own citizens, the purple fog over the wall for
 * Hillvile night owls, frayed sanity, nightmares, nemeses, and possessed
 * friends haunting you. Daylight, St. Cuddles and holy species push it
 * back. At 100 a resident becomes RES_POSSESSED by a named Hellvile
 * citizen (sim_possessor_of). OPT_POSSESSION_RATE scales the creep and
 * caps how many can be possessed at once, so the town never collapses
 * into an all-demon apocalypse unless you ask for Frequent.
 *
 * Killing off is gated: only a RES_SOUL_LOST resident (3 failed
 * exorcisms) may be laid to rest, unless OPT_MONSTER_MODE is on - then
 * anyone can, it is marked RF_MONSTER_KILL, raises the monster meter and
 * the victim's friends remember who did it.
 */
#include "sim_int.h"

/* corruption per game hour by venue (before scaling) */
static const int8_t venue_corr[ACT_COUNT] = {
    /* HOME SLEEP PARK CAFE SHOP CHURCH WORK BEACH CONCERT VAPE HOSP WANDER HAUNT */
       0,   0,   -1,   0,   0,   -6,    0,  -1,   1,      4,   0,   0,     2
};

static int rate_factor(void)
{
    static const uint8_t f[4] = { 0, 50, 100, 200 };
    static const uint8_t d[4] = { 60, 100, 140, 180 };
    int o = DOOMO_CLAMP(opt_get(OPT_POSSESSION_RATE), 0, 3);
    int k = DOOMO_CLAMP(opt_get(OPT_DIFFICULTY), 0, 3);
    return f[o] * d[k] / 100;
}

int sim_possessed_count(void)
{
    sim_ensure_init();
    return sim_s.possessed;
}

int sim_possession_cap(void)
{
    int pop = town_count(-1), o = opt_get(OPT_POSSESSION_RATE);
    if (o <= 0) return 0;
    if (doomo_town.mode == TOWN_STORY && doomo_town.story.chapter < 2) return 0;
    if (o == 1) return 1 + pop / 250;
    if (o == 2) return 2 + pop / 100;
    return 4 + pop / 25;
}

void sim_recount_possessed(void)
{
    sim_s.possessed = (int16_t)town_count(RES_POSSESSED);
}

/* ---------------------------------------------------------------------- */
/* who is in there?                                                         */
int sim_possessor_of(int id)
{
    const DoomoResident *r = town_res(id);
    uint32_t h;
    int start, i;
    if (!r || id >= DOOMO_MAX_RESIDENTS) return -1;
    if (r->status != RES_POSSESSED && r->status != RES_SOUL_LOST) return -1;
    /* attempts - fails stays the same through one possession (every
     * failure bumps both) and changes after a successful exorcism */
    h = doomo_hash32(doomo_town.seed * 0x2545F491u ^ (uint32_t)id * 0x9E3779B1u ^
                     (uint32_t)(RES_EXO_ATTEMPTS(r) - RES_EXO_FAILS(r)) * 0x85EBCA6Bu ^ 0xDE40u);
    start = (int)(h % DOOMO_DISTRICT_SIZE);
    for (i = 0; i < DOOMO_DISTRICT_SIZE; i++) {
        int c = DOOMO_DISTRICT_SIZE + (start + i * 7) % DOOMO_DISTRICT_SIZE;
        int s = doomo_town.res[c].status;
        if (c == id) continue;
        if (s == RES_ALIVE || s == RES_POSSESSED || s == RES_GHOST) return c;
    }
    /* nobody lives in Hellvile: name a premade Hellvile citizen */
    start = DOOMO_DISTRICT_SIZE + start;
    return start == id ? (start + 1 < DOOMO_MAX_RESIDENTS ? start + 1 : DOOMO_DISTRICT_SIZE) : start;
}

const char *sim_possessor_name(int id)
{
    int p = sim_possessor_of(id);
    const DoomoResident *r;
    if (p < 0) return "";
    r = town_res(p);
    if (r && r->status != RES_EMPTY && r->name[0]) return r->name;
    if (roster_name(p)[0]) return roster_name(p);
    return "Nameless One";
}

/* ---------------------------------------------------------------------- */
/* corruption & possession                                                  */
void sim_possess(int id)
{
    DoomoResident *r = town_res(id);
    if (!r || id >= DOOMO_MAX_RESIDENTS || r->status != RES_ALIVE) return;
    r->status = RES_POSSESSED;
    r->stat[STAT_CORRUPTION] = 100;
    r->stat[STAT_SANITY] = (uint8_t)DOOMO_MIN(r->stat[STAT_SANITY], 30);
    r->activity = (uint8_t)(sim_s.night ? ACT_HAUNTING : ACT_WANDERING);
    sim_s.possessed++;
    sim_s.stats.possessions++;
    sim_set_transient(id, TR_NONE, 0);
    sim_problem_raise(id, PROB_POSSESSED, sim_possessor_of(id), 0);
    sim_newsf("Uh oh! %s is acting strange... POSSESSED by %s!", r->name,
              sim_possessor_name(id));
    portrait_invalidate(id);
    sim_mood_of(id);
    unlock_grant(353);                         /* "Something wicked" */
}

static int possession_allowed(void)
{
    return sim_s.possessed < sim_possession_cap();
}

void sim_corrupt_natural(int id, int amount)
{
    DoomoResident *r = town_res(id);
    int v, holy;
    if (!r || id >= DOOMO_MAX_RESIDENTS || r->status != RES_ALIVE || !amount) return;
    if (amount > 0) {
        int q, rem;
        holy = doomo_species_info[r->look.species % SPECIES_COUNT].holy_affinity;
        if (r->look.halo != HALO_NONE && r->look.halo != HALO_BROKEN) holy += 10;
        if (r->look.accessory == ACC_ROSARY || r->look.accessory == ACC_CROSS_NECKLACE) holy += 15;
        holy = DOOMO_CLAMP(holy, 0, 90);
        q = amount * rate_factor() * (100 - holy);
        rem = q % 10000;
        amount = q / 10000 + (rem && sim_rand(10000) < rem ? 1 : 0);
        if (!amount) return;
    }
    v = r->stat[STAT_CORRUPTION] + amount;
    v = DOOMO_CLAMP(v, 0, 100);
    if (v >= 100) {
        if (possession_allowed()) {
            r->stat[STAT_CORRUPTION] = 100;
            sim_possess(id);
            return;
        }
        v = 99;
    }
    r->stat[STAT_CORRUPTION] = (uint8_t)v;
}

void sim_corrupt(int id, int amount)
{
    DoomoResident *r = town_res(id);
    int v;
    sim_ensure_init();
    if (!r || id >= DOOMO_MAX_RESIDENTS || r->status != RES_ALIVE) return;
    v = r->stat[STAT_CORRUPTION] + amount;
    v = DOOMO_CLAMP(v, 0, 100);
    r->stat[STAT_CORRUPTION] = (uint8_t)v;
    if (v >= 100) sim_possess(id);
}

void sim_soul_update(int id, DoomoResident *r, int m)
{
    int act = r->activity < ACT_COUNT ? r->activity : 0;
    int base = doomo_species_info[r->look.species % SPECIES_COUNT].base_corruption;
    int hell = RES_DISTRICT(id) || (r->flags & RF_HELLBORN);
    int up = 0, down = 0, c = r->stat[STAT_CORRUPTION];
    if (r->status == RES_GHOST) { r->stat[STAT_CORRUPTION] = 0; return; }
    if (r->status != RES_ALIVE) return;
    if (venue_corr[act] > 0) up += venue_corr[act];
    else down -= venue_corr[act];
    if (sim_s.night) {
        if (hell && c < base + 10) up += 1;
        if (!hell && (act == ACT_CONCERT || act == ACT_WANDERING)) up += 1;
        if (r->stat[STAT_SANITY] < 30) up += 1;
        /* nightmares (chance per game hour in 1/10000) */
        if (act == ACT_SLEEPING) {
            int pct = 1 + (100 - r->stat[STAT_SANITY]) / 8 +
                      ((r->fear == 1 || r->fear == 4) ? 6 : 0) +
                      (c >= 50 ? 8 : 0) + (rate_factor() >= 150 ? 8 : 0);
            if (sim_roll(600000u) < (uint32_t)(pct * (m > 120 ? 120 : m))) {
                sim_s.stats.nightmares++;
                sim_stat_add(r, STAT_SANITY, -8);
                sim_stat_add(r, STAT_HAPPY, -4);
                sim_set_transient(id, TR_SCARED, 4);
                sim_corrupt_natural(id, 6);
                if (r->status != RES_ALIVE) return;
                if (sim_chance(50)) sim_problem_raise(id, PROB_NIGHTMARE, -1, 0);
            }
        }
    } else if (c > base) {
        down += 1;                                     /* sunshine helps */
    }
    if (up) sim_corrupt_natural(id, sim_scaled(up, m));
    if (down && r->status == RES_ALIVE) sim_stat_add(r, STAT_CORRUPTION, -sim_scaled(down, m));
}

void sim_soul_daily(void)
{
    int i, fuzz = -1;
    const DoomoResident *f;
    sim_recount_possessed();
    if (doomo_town.mode == TOWN_STORY || sim_s.possessed <= 0) return;
    /* Father Fuzz makes house calls when nobody else will */
    f = town_res(ROSTER_ID_FATHER_FUZZ);
    if (f && f->status == RES_ALIVE && strncmp(f->name, roster_name(ROSTER_ID_FATHER_FUZZ),
                                               DOOMO_NAME_LEN) == 0)
        fuzz = ROSTER_ID_FATHER_FUZZ;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        DoomoResident *r = &doomo_town.res[i];
        if (r->status != RES_POSSESSED) continue;
        if (!sim_chance(fuzz >= 0 ? 7 : 3)) continue;
        sim_newsf("%s chased %s out of %s! Phew!", fuzz >= 0 ? "Father Fuzz" : "A kind stranger",
                  sim_possessor_name(i), r->name);
        r->status = RES_ALIVE;
        r->stat[STAT_CORRUPTION] = 35;
        sim_s.possessed--;
        sim_s.stats.exorcisms_auto++;
        sim_problem_drop_for(i);
        portrait_invalidate(i);
        sim_mood_of(i);
    }
}

void sim_on_exorcism(int id, int success)
{
    DoomoResident *r = town_res(id);
    int pi;
    if (!r) return;
    sim_ensure_init();
    sim_recount_possessed();
    if ((pi = sim_problem_find(id, PROB_POSSESSED)) >= 0 && r->status != RES_POSSESSED)
        sim_problem_remove(pi);
    if (success) {
        sim_set_transient(id, TR_ECSTATIC, 6);
        sim_newsf("St. Cuddles rejoices! %s is free again!", r->name);
    } else if (r->status == RES_SOUL_LOST) {
        sim_newsf("The exorcisms have failed... %s's soul is lost.", r->name);
    } else {
        sim_newsf("Exorcism failed! %s still has a demon inside.", r->name);
    }
    portrait_invalidate(id);
    sim_mood_of(id);
}

/* ---------------------------------------------------------------------- */
/* death, grief, ghosts                                                     */
int sim_can_lay_to_rest(int id)
{
    const DoomoResident *r = town_res(id);
    if (!r || id < 0 || id >= DOOMO_MAX_RESIDENTS) return 0;
    if (doomo_town.mode == TOWN_STORY && (r->flags & RF_STORY_NPC)) return 0;
    if (r->status == RES_SOUL_LOST) return 1;
    if (opt_get(OPT_MONSTER_MODE) && (r->status == RES_ALIVE || r->status == RES_POSSESSED))
        return 1;
    return 0;
}

static void grieve(int id, int monster)
{
    int i;
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        int other, v = e->value, k = e->kind & REL_KIND_MASK;
        DoomoResident *o;
        if (e->a == id) other = e->b;
        else if (e->b == id) other = e->a;
        else continue;
        if (other >= DOOMO_MAX_RESIDENTS) continue;
        o = &doomo_town.res[other];
        if (o->status != RES_ALIVE && o->status != RES_GHOST) continue;
        if (k == REL_FAMILY || k == REL_MARRIED || k == REL_DATING) v += 40;
        if (v < 15) continue;
        sim_stat_add(o, STAT_HAPPY, -(10 + v / 5));
        sim_stat_add(o, STAT_SANITY, -(5 + v / 10));
        sim_set_transient(other, TR_GRIEF, 6 + v / 12);
        if (monster) rel_adjust(other, DOOMO_PLAYER_ID, -(10 + v / 4));
        if (v >= 70 && o->status == RES_ALIVE) sim_problem_raise(other, PROB_LONELY, id, 0);
        if (k == REL_DATING || k == REL_MARRIED) bit_set(sim_s.taken, other, 0);
    }
}

void sim_lay_to_rest(int id, int cause)
{
    DoomoResident *r = town_res(id);
    int monster, was;
    sim_ensure_init();
    if (!r || id < 0 || id >= DOOMO_MAX_RESIDENTS) return;
    was = r->status;
    if (cause == DEATH_LAID_TO_REST || cause == DEATH_MONSTER || cause == DEATH_NONE) {
        if (!sim_can_lay_to_rest(id)) return;
        monster = was != RES_SOUL_LOST;
        cause = monster ? DEATH_MONSTER : DEATH_LAID_TO_REST;
    } else {
        /* acts of fate (demon attacks in the story, old age ...) */
        if (was != RES_ALIVE && was != RES_POSSESSED && was != RES_SOUL_LOST) return;
        if (doomo_town.mode == TOWN_STORY && (r->flags & RF_STORY_NPC)) return;
        monster = 0;
    }
    grieve(id, monster);
    r->status = RES_DEAD;
    r->death_cause = (uint8_t)DOOMO_CLAMP(cause, 0, DEATH_COUNT - 1);
    r->activity = ACT_HOME;
    r->mood = MOOD_OK;
    bit_set(sim_s.taken, id, 0);
    bit_set(sim_s.crush, id, 0);
    sim_s.transient[id] = 0;
    if (was == RES_POSSESSED) sim_s.possessed--;
    grave_add(id);
    sim_problem_drop_for(id);
    sim_s.stats.deaths++;
    portrait_invalidate(id);
    if (monster) {
        r->flags |= RF_MONSTER_KILL;
        if (doomo_town.monster_meter < 0xFFFF) doomo_town.monster_meter++;
        sim_newsf("%s has... vanished. Folks whisper about a MONSTER.", r->name);
        unlock_grant(355);
    } else if (cause == DEATH_LAID_TO_REST) {
        sim_newsf("Farewell, %s. Laid to rest under the willow.", r->name);
        unlock_grant(354);
    } else {
        sim_newsf("Tragedy: %s is gone (%s). Flowers at the cemetery.", r->name,
                  doomo_death_names[r->death_cause]);
    }
}

int sim_raise_ghost(int id)
{
    DoomoResident *r = town_res(id);
    sim_ensure_init();
    if (!r || id < 0 || id >= DOOMO_MAX_RESIDENTS || r->status != RES_DEAD) return 0;
    r->status = RES_GHOST;
    r->stat[STAT_HUNGER] = 100;
    r->stat[STAT_HAPPY] = 60;
    r->stat[STAT_HYGIENE] = 100;
    r->stat[STAT_ENERGY] = 70;
    r->stat[STAT_HEALTH] = 100;
    r->stat[STAT_SANITY] = 50;
    r->stat[STAT_CORRUPTION] = 0;
    r->stat[STAT_SOCIAL] = 40;
    r->activity = ACT_HAUNTING;
    r->exorcism = (uint8_t)(r->exorcism & 0xF0);
    sim_s.stamp[id] = (uint16_t)sim_now();
    progress_add(PROG_GHOSTS_RAISED, 1);
    sim_newsf("Boo! %s is back... as a friendly GHOST!", r->name);
    portrait_invalidate(id);
    sim_mood_of(id);
    return 1;
}
