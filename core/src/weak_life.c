/* weak_life.c - weak defaults for life.h's family / endings functions
 * (owner: life, LIFESIM_PLAN R4). F1 and LS5 define the strong ones in
 * their own files; nobody edits stubs.c or this file to do it.
 *
 * Every default means "no household yet" and is silent (no STUB log): the
 * callers (play-as, endings) ask on normal paths where "none" is the right
 * answer. No .bss.
 */
#include <string.h>
#include "doomo/life.h"

#define WEAK __attribute__((weak))

WEAK int family_count(void) { return 0; }

WEAK int family_member(int i)
{
    (void)i;
    return -1;
}

WEAK int life_player_partner(void) { return -1; }

WEAK int life_family_count(void) { return 0; }

WEAK void life_ending_facts(LifeEndingFacts *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->partner = -1;
}
