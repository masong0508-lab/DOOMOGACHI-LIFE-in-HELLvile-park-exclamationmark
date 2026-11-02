/* hero.c - PA2: the hero presets (SUNNY, MARVIN) behind hero_preset().
 *
 * The story's protagonist is town_res(DOOMO_PLAYER_ID). A new story game
 * copies the preset of playas_hero_kind() into it (story.c default_player);
 * HERO_CUSTOM has no preset (the creator, arg -3, makes the hero), and the
 * co-op kinds stay NULL until MP1 fills them. The presets are built once
 * from fixed seeds with resident_generate(), so they are identical on every
 * platform. 2 x 96 B of statics, no tables. */
#include <string.h>
#include "doomo/playas.h"
#include "doomo/resident.h"
#include "doomo/catalog.h"

#define HERO_SEED_SUNNY  0x5A44u
#define HERO_SEED_MARVIN 0x4D41u

static DoomoResident preset[2];
static uint8_t preset_ready;

static void build(DoomoResident *r, uint32_t seed, const char *name,
                  int species, int ears, int tail, int pronoun)
{
    resident_generate(r, seed);
    memset(r->name, 0, sizeof(r->name));
    strncpy(r->name, name, sizeof(r->name) - 1);
    r->look.species = (uint8_t)species;
    r->look.ears    = (uint8_t)ears;
    r->look.tail    = (uint8_t)tail;
    r->look.horns   = HORNS_NONE;
    r->look.wings   = WINGS_NONE;
    r->status       = RES_ALIVE;
    resident_sanitize(r);
    r->pronoun = (uint8_t)pronoun;               /* after sanitize: the contract */
    r->flags   = (uint8_t)((r->flags | RF_PLAYER) & ~RF_HELLBORN);
    r->level   = 0;
}

const DoomoResident *hero_preset(int kind)
{
    if (kind != HERO_SUNNY && kind != HERO_MARVIN) return NULL;
    if (!preset_ready) {
        build(&preset[0], HERO_SEED_SUNNY, "Sunny", SPECIES_BUNNYKIN, EARS_BUNNY, TAIL_BUNNY, PRONOUN_SHE);
        build(&preset[1], HERO_SEED_MARVIN, "Marvin", SPECIES_FOXKIN, EARS_FOX, TAIL_FOX, PRONOUN_HE);
        preset_ready = 1;
    }
    return &preset[kind == HERO_MARVIN];
}
