/* weak_playas.c - weak defaults for playas.h (owner: playas, LIFESIM_PLAN
 * R4: the area's own weak file, so nobody edits stubs.c). PA1 (playas.c),
 * PA2 (hero.c), PA9 and MP0 define the strong functions in their own files;
 * nobody edits this file to implement one.
 *
 * The defaults are "you are the hero, nothing is locked, play-as is not
 * available yet", so the game and its save files are exactly what they
 * were before PA0: playas_profile_flags() is 0, the value byte 47 of the
 * save header always had. The getters are quiet (callers ask on normal
 * paths); the mutators log "STUB: playas_<name>" and change nothing.
 * No .bss.
 */
#include "doomo/playas.h"
#include "doomo/catalog.h"
#include "doomo/sim.h"
#include "doomo/platform.h"

#define WEAK __attribute__((weak))

WEAK int playas_avatar(void) { return PLAYAS_ID_HERO; }

WEAK int playas_is_hero(void) { return playas_avatar() == PLAYAS_ID_HERO; }

WEAK int playas_story_locked(void) { return 0; }

WEAK int playas_new_life_ok(void) { return 0; }

WEAK int playas_new_life(int id)
{
    (void)id;
    return 0;
}

WEAK int playas_available(void) { return 0; }

WEAK int playas_can_be(int id)
{
    (void)id;
    return 0;
}

WEAK int playas_gender(int id)
{
    const DoomoResident *r = town_res(id);
    if (!r || r->status == RES_EMPTY) return GENDER_N;
    return RES_PRONOUN(r) == PRONOUN_SHE ? GENDER_F : RES_PRONOUN(r) == PRONOUN_HE ? GENDER_M : GENDER_N;
}

WEAK int playas_you_gender(void) { return playas_gender(playas_avatar()); }

WEAK int playas_hero_kind(void) { return HERO_CUSTOM; }

WEAK int playas_recent(int i)
{
    (void)i;
    return PLAYAS_ID_NONE;
}

WEAK int playas_hero_pos(PlayasPos *out)
{
    (void)out;
    return 0;
}

WEAK uint8_t playas_profile_flags(void) { return 0; }

WEAK const DoomoResident *hero_preset(int kind)
{
    (void)kind;
    return NULL;
}

WEAK int playas_romance_ok(int from, int to) { return sim_romance_ok(from, to); }

WEAK int playas_profile(void) { return 0; }

WEAK int playas_profile_avatar(int profile) { return profile == 0 ? playas_avatar() : PLAYAS_ID_NONE; }

WEAK int playas_profile_hero(int profile) { return profile == 0 ? playas_hero_kind() : -1; }

WEAK int playas_switch(int id, int how)
{
    (void)id;
    (void)how;
    plat_log("STUB: playas_switch");
    return 0;
}

WEAK void playas_set_hero_kind(int kind)
{
    (void)kind;
    plat_log("STUB: playas_set_hero_kind");
}
