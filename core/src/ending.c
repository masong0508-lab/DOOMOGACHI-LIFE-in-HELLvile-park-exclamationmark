/* ending.c - E1 (core): the endings state, its 'ENDS' save chunk and the finale hook
 * (owner: E1; contract: doomo/ending.h). Strong definitions of the weak defaults in
 * weak_endings.c: ending_state / seen / mark / count_seen / resolve_finale / partner.
 *
 * What this is: the engine. An ending is reached with ending_reach(k); that records it
 * (seen bit, times, last, the unlock UL_STORY_END(k), quietly: K0's rule of no table entry),
 * queues its card, and the story overworld shows SCENE_ENDING_CARD when idle. The 'ENDS'
 * chunk keeps the 32 B state; an untouched state writes nothing, so a save that never
 * reached an ending stays byte-identical.
 *
 * The adult gate (R10): mature_adult_ok() is strong here (allow-list below). Every mature slot, sexy
 * ending, override-art lookup and romance route participant asks it; it is the one place that
 * says who may be in adult material.
 *
 * What this is not (yet): the rules that pick E02..E14 (E3 good + secret, E4 bad + sexy, the
 * route scripts). Today the story's credits reach the default good ending, E01; each content
 * package adds its own ending_reach() call.
 * State is 32 B of .bss; the allow-list is 18 B of const data. */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/ending.h"
#include "doomo/life.h"
#include "doomo/family.h"
#include "doomo/house_api.h"
#include "doomo/save_budget.h"
#include "doomo/save_ext.h"
#include "doomo/unlock_ids.h"
#include "doomo/unlocks.h"
#include "doomo/story.h"
#include "doomo/town.h"
#include "doomo/roster.h"
#include "doomo/resident.h"
#include "doomo/sim.h"

static DoomoEnds E;

static void ends_reset(void)
{
    memset(&E, 0, sizeof E);
    E.version = ENDS_VERSION;
    E.last = ENDING_NONE;
    E.pending = ENDING_NONE;
    E.route_partner = 0xFFFFu;
}

/* the state exists from the first call, even before any new-game reset() ran */
static DoomoEnds *ends(void)
{
    if (E.version == 0) ends_reset();
    return &E;
}

DoomoEnds *ending_state(void) { return ends(); }

int ending_seen(int k)
{
    if (k < 0 || k >= ENDING_COUNT) return 0;
    return (ends()->seen >> k) & 1;
}

void ending_mark(int k)
{
    DoomoEnds *e = ends();
    if (k < 0 || k >= ENDING_COUNT) return;
    e->seen = (uint16_t)(e->seen | (1u << k));
    if (e->times[k] < 255) e->times[k]++;
    e->last = (uint8_t)k;
    unlock_grant_quiet(UL_STORY_END(k));
}

int ending_count_seen(void)
{
    int k, n = 0;
    for (k = 0; k < ENDING_MAIN_COUNT; k++) if (!ENDING_IS_RETIRED(k)) n += ending_seen(k);    /* the bonus and the retired never count */
    return n;
}

/* E10 "Every Sparkle": every secret sparkle there is (HaHaland 0..7, Hellvile 16..23, the united
 * park 24..31: slots 8..15 hold none) and the dream D2 played. E11 "One More Time": a New Game+
 * run that has seen the bottle and opened the suitcase. */
static int sparkles_all_(void)
{
    int i;
    for (i = 0; i < 32; i++)
        if (!(i >= 8 && i < 16) && !story_flag(STORY_FLAG_SECRET_FIRST + i)) return 0;
    return 1;
}

static int e10_ok_(void) { return sparkles_all_() && story_episode_done(BEAT_D2); }
static int e11_ok_(void) { return story_flag(SF_NGPLUS) && story_flag(SF_BOTTLE_SEEN) && story_flag(SF_SUITCASE_OPEN); }

void ending_check_sparkles(void)
{
    if (e10_ok_() && !ending_seen(ENDING_E10)) ending_reach(ENDING_E10);
}

int ending_resolve_finale(void)
{
    /* secret outranks bad outranks good: E11, then E10, before the rules below */
    if (e11_ok_()) return ENDING_E11;
    if (e10_ok_()) return ENDING_E10;
    /* secret > bad > good (ending.h). E3's rule for the two finale good endings: E02, the better
     * one, needs a real household (3+ members, the player included), nobody lost to neglect, and
     * at least one close tie to show for it: a housemate with a closest housemate, a child, a
     * couple you matched, or a happy island (average happiness 60+). Anything less is the default
     * E01. E06 / E07 (the parade) and the other endings stay with their own packages. */
    LifeEndingFacts f;
    life_ending_facts(&f);
    if (f.household >= 3 && f.neglect_deaths == 0 &&
        (f.best_friends >= 1 || f.kids >= 1 || f.couples >= 1 || f.happiness >= 60))
        return ENDING_E02;
    return ENDING_E01;
}

int ending_partner(void)
{
    const DoomoEnds *e = ends();
    int p = life_player_partner(), r, best = -1, bs = RSTAGE_NONE;
    if (p >= 0) return p;
    for (r = 0; r < ROUTE_COUNT; r++) {
        int st = e->route_stage[r];
        if (st >= RSTAGE_COMMITTED && st <= RSTAGE_MARRIED && st > bs) { best = r; bs = st; }
    }
    return best >= 0 && e->route_partner != 0xFFFFu ? (int)e->route_partner : -1;
}

/* ---- reaching an ending ---- */
int ending_reach(int k)
{
    int fresh;
    if (k < 0 || k >= ENDING_COUNT || ENDING_IS_RETIRED(k)) return 0;   /* retired: never reached */
    fresh = !ending_seen(k);
    ending_mark(k);
    ends()->pending = (uint8_t)k;
    return fresh;
}

int ending_pending(void) { return ends()->pending; }

int ending_take_pending(void)
{
    DoomoEnds *e = ends();
    int k = e->pending;
    e->pending = ENDING_NONE;
    return k < ENDING_COUNT ? k : ENDING_NONE;
}

void ending_finale_reached(void)
{
    ends()->flags |= ENDF_FINALE_DONE;
    ending_reach(ending_resolve_finale());
    ending_check_family();
    ending_check_bonejangles();
    ending_check_sparkles();
}

/* E09 "Bonejangles Bows": the Feast is over (any outcome) and the machine-heart piece is found. */
void ending_check_bonejangles(void)
{
    if (story_flag(SF_FEAST_DONE) && story_flag(SF_BONE_HEART)) ending_reach(ENDING_E09);
}

/* E04 "Family Portrait": a partner, a household and an ETERNAL house room. */
void ending_check_family(void)
{
    int r;
    if (ending_partner() < 0 || family_count() < 1) return;
    for (r = 0; r < HOME_ROOM_COUNT; r++)
        if (home_grade(r) == HG_ETERNAL) { ending_reach(ENDING_E04); return; }
}

/* ---- the 'ENDS' chunk (little endian, 32 B, DoomoEnds' own layout) ---- */
static int is_default(void)
{
    const DoomoEnds *e = ends();
    int k;
    if (e->seen || e->flags || e->last != ENDING_NONE || e->pending != ENDING_NONE) return 0;
    if (e->route_partner != 0xFFFFu) return 0;
    for (k = 0; k < ENDING_COUNT; k++) if (e->times[k]) return 0;
    for (k = 0; k < ROUTE_COUNT; k++) if (e->route_meter[k] || e->route_stage[k]) return 0;
    return 1;
}

static uint32_t ends_size(void) { return is_default() ? 0u : (uint32_t)sizeof(DoomoEnds); }

static uint32_t ends_write(uint8_t *d, uint32_t cap)
{
    const DoomoEnds *e = ends();
    int k;
    if (is_default() || cap < sizeof(DoomoEnds)) return 0;
    memset(d, 0, sizeof(DoomoEnds));
    d[0] = ENDS_VERSION;
    d[1] = e->last;
    d[2] = (uint8_t)e->seen; d[3] = (uint8_t)(e->seen >> 8);
    for (k = 0; k < ENDING_COUNT; k++) d[4 + k] = e->times[k];            /* 4..18 */
    d[19] = e->flags;
    for (k = 0; k < ROUTE_COUNT; k++) { d[20 + k] = e->route_meter[k]; d[23 + k] = e->route_stage[k]; }
    d[26] = (uint8_t)e->route_partner; d[27] = (uint8_t)(e->route_partner >> 8);
    d[28] = e->pending;
    return (uint32_t)sizeof(DoomoEnds);
}

static int ends_read(const uint8_t *s, uint32_t len, uint16_t ver)
{
    DoomoEnds *e;
    int k;
    ends_reset();
    if (ver != 1 || len < sizeof(DoomoEnds)) return 0;
    e = &E;
    e->last = s[1] < ENDING_COUNT ? s[1] : ENDING_NONE;
    e->seen = (uint16_t)((s[2] | (s[3] << 8)) & ((1u << ENDING_COUNT) - 1u));
    for (k = 0; k < ENDING_COUNT; k++) e->times[k] = s[4 + k];
    e->flags = (uint8_t)(s[19] & (ENDF_FINALE_DONE | ENDF_NGPLUS_KEPT | ENDF_POSTGAME));
    for (k = 0; k < ROUTE_COUNT; k++) {
        e->route_meter[k] = s[20 + k];
        e->route_stage[k] = s[23 + k] < RSTAGE_COUNT ? s[23 + k] : RSTAGE_NONE;
    }
    e->route_partner = (uint16_t)(s[26] | (s[27] << 8));
    e->pending = s[28] < ENDING_COUNT ? s[28] : ENDING_NONE;
    return 1;
}

static const DoomoSaveExt ends_ext = {
    SAVE_EXT_TAG_ENDS, ENDS_VERSION, ends_size, ends_write, ends_read, ends_reset, SAVE_EXT_MAX_ENDS
};
const DoomoSaveExt *const doomo_save_ext_endings = &ends_ext;

/* ---- the adult gate (R10; E1) ---------------------------------------------
 * 1 only if resident `id` may appear in a mature or romance scene. All of these must hold:
 *   - id is a slot of a TOWN_POP_PREMADE island (not the player, a family pool id or a negative);
 *   - the slot still holds the premade citizen: its name equals roster_name(id), so a renamed
 *     or replaced slot (custom, sim-born, moved-in stranger) never qualifies;
 *   - not RF_CUSTOM (made or edited by the player);
 *   - life stage ADULT or ELDER (LS0: an elder counts as an adult), and the roster's own traits
 *     do not say KID or TEEN (belt and braces: a stage that was edited down never lets a kid in);
 *   - not on the kid-coded cast (core/data/story/kid_cast.txt, E5).
 * The kid cast is the same list as the file; test_ending.c reads the file and fails if the two
 * ever differ, so adding a kid to the file without this table is caught. */
static const uint16_t kid_cast_ids[] = { 42, 563, 72, 127, 530, 540, 582, 597, 820 };

int ending_kid_cast(int id)
{
    unsigned i;
    for (i = 0; i < sizeof kid_cast_ids / sizeof kid_cast_ids[0]; i++)
        if (kid_cast_ids[i] == id) return 1;
    return 0;
}

int mature_adult_ok(int id)
{
    const DoomoResident *r;
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return 0;
    if (doomo_town.population != TOWN_POP_PREMADE) return 0;
    r = town_res(id);
    if (!r || (r->flags & RF_CUSTOM)) return 0;
    if (strncmp(roster_name(id), r->name, DOOMO_NAME_LEN) != 0) return 0;
    if (!LIFE_IS_ADULT(sim_life_stage(id))) return 0;
    if (roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN)) return 0;
    return !ending_kid_cast(id);
}
