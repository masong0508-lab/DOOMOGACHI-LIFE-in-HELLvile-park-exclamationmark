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
 * What this is not (yet): the rules that pick E02..E14 (E3 good + secret, E4 bad + sexy,
 * the route scripts) and the strong mature_adult_ok() allow-list. mature_adult_ok() stays
 * the weak "nobody" answer in weak_endings.c on purpose: no mature slot or sexy ending can
 * play until the adult allow-list lands. Today the story's credits reach the default good
 * ending, E01; each content package adds its own ending_reach() call.
 * State is 32 B of .bss. */
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
    for (k = 0; k < ENDING_MAIN_COUNT; k++) n += ending_seen(k);    /* the bonus never counts */
    return n;
}

int ending_resolve_finale(void)
{
    /* secret > bad > good (ending.h). Provisional until E3 / E4 land their rules: the better
     * good ending E02 needs a real household (3+ members, the player included) and nobody
     * lost to neglect; otherwise the default E01. E06 / E07 (the parade) and the secret
     * finales stay with their content packages. */
    LifeEndingFacts f;
    life_ending_facts(&f);
    return (f.household >= 3 && f.neglect_deaths == 0) ? ENDING_E02 : ENDING_E01;
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
    if (k < 0 || k >= ENDING_COUNT) return 0;
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
