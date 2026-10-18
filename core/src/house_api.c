/* house_api.c - the resident door into the dream house (owner: H1; SPEC
 * sec.4, C6, C9). Story code calls these; they only read and write the
 * HOME .bss and the resident grant table (furn_grant.c). Anything that
 * needs the catalog, the score or a layout runs in the home group
 * (SCENE_HOME) and leaves its result in HOME.cache, which is served here.
 * NOT named home_*.c, so the PS1 `home` overlay regex leaves it resident
 * (SPEC R4).
 */
#include "home_int.h"

static uint8_t remove_only;          /* Interlude III's editor mode (not saved) */

static int cap_one(int furn)
{
    return (doomo_furn_cap1[furn >> 3] >> (furn & 7)) & 1;
}

int home_has(int furn)
{
    HomeState *s = home_state();
    if (!s || furn < 0 || furn >= FURN_MAX) return 0;
    return (s->owned[furn >> 1] >> ((furn & 1) * 4)) & 15;
}

int home_grant(int furn)
{
    HomeState *s = home_state();
    int n, cap;
    if (!s || furn < 0 || furn >= FURN_MAX) return 0;
    n = home_has(furn);
    cap = cap_one(furn) ? 1 : 15;
    if (n >= cap) return 0;
    n++;
    s->owned[furn >> 1] = (uint8_t)((s->owned[furn >> 1] & (0xF0 >> ((furn & 1) * 4))) |
                                    (n << ((furn & 1) * 4)));
    s->seen_new[furn >> 3] |= (uint8_t)(1 << (furn & 7));
    s->ver_flags |= HVF_TOUCHED;
    return 1;
}

/* A gift lands in storage with its "NEW!" bit; the next time the house
 * opens, SCENE_HOME places NEW gift pieces with a DROP (C6: no placement
 * code here). 0 = it could not even be stored (bad id, full, no save room). */
int home_place_gift(int furn)
{
    if (!save_budget_fits(doomo_town.mode == TOWN_STORY ? 0 : 1, HOME_SZ_OBJ)) return 0;
    return home_grant(furn);
}

void home_remove_only(int on) { remove_only = (uint8_t)(on != 0); }
int  home_remove_only_on(void) { return remove_only; }

int home_grade(int room)
{
    HomeState *s = home_state();
    if (!s || room < 0 || room >= HOME_ROOM_COUNT) return HG_NONE;
    return home_stamps_grade(s->cache[HC_STAMPS + room]);
}

int home_stamps_grade(unsigned stamps)
{
    int g;
    for (g = HG_COUNT - 1; g > HG_NONE; g--)
        if (stamps & HOME_STAMP(g)) return g;
    return HG_NONE;
}

int home_stat(int stat)
{
    HomeState *s = home_state();
    if (!s || stat < 0 || stat >= HS_COUNT) return 0;
    return s->cache[HC_STAT + stat];
}

int home_goal_check(int goal)
{
    HomeState *s = home_state();
    int r;
    if (!s || goal <= HG_NONE || goal >= HG_COUNT) return 0;
    for (r = 0; r < HOME_ROOM_COUNT; r++)
        if (s->cache[HC_STAMPS + r] & HOME_STAMP(goal)) return 1;
    return 0;
}

int home_decorate_for(int layout, int tol)
{
    HomeState *s = home_state();
    (void)tol;
    if (!s) return 0;
    /* the home group stores (layout << 4 | progress) when it checks */
    return (s->decorate_for >> 4) == (layout & 15) ? (s->decorate_for & 15) : 0;
}

void home_haunt_step(int night)
{
    HomeState *s = home_state();
    if (!s) return;
    if (night > s->haunt[0] && night <= 3) s->haunt[0] = (uint8_t)night;
    s->ver_flags |= HVF_TOUCHED;
}

int home_defend_result(void)
{
    HomeState *s = home_state();
    return s ? s->cache[HC_DEFEND] : 0;
}

int home_rivet_visits(void)
{
    HomeState *s = home_state();
    int was;
    if (!s) return 0;
    was = s->days_since_fire;
    s->days_since_fire = 0;
    s->ver_flags |= HVF_TOUCHED;
    return was;
}

int home_days_since_fire(void)
{
    HomeState *s = home_state();
    return s ? s->days_since_fire : 0;
}

void home_set_phase(int phase)
{
    if (phase >= 0 && phase < HP_COUNT) story_stry()->home_phase = (uint8_t)phase;
}

int home_phase(void)
{
    /* free play has no story phase: the house is simply yours */
    if (doomo_town.mode != TOWN_STORY) return HP_OPEN;
    return story_stry()->home_phase < HP_COUNT ? story_stry()->home_phase : HP_COSY;
}

int home_prop_gag(int gag)
{
    HomeState *s = home_state();
    int i;
    if (!s || gag <= 0) return 0;
    for (i = 0; i < HOME_PENDING_GAGS; i++)
        if (s->gag[i] == 0) { s->gag[i] = (uint16_t)gag; s->ver_flags |= HVF_TOUCHED; return 1; }
    return 0;
}

int home_moved_in(void)
{
    HomeState *s = home_state();
    return s ? (s->ver_flags & HVF_MOVED_IN) != 0 : 0;
}
