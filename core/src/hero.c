/* hero.c - PA2: the hero presets (SUNNY, MARVIN) behind hero_preset().
 *
 * The story's protagonist is town_res(DOOMO_PLAYER_ID). A new story game
 * copies the preset of playas_hero_kind() into it (story.c default_player);
 * HERO_CUSTOM has no preset (the creator, arg -3, makes the hero), and the
 * co-op kinds (HERO_COOP_FIRST.. +2: Ember, Rook, Pip) are the guests' heroes (MP1); the rest
 * stay NULL. The presets are built once from fixed seeds with resident_generate(), so they
 * are identical on every platform. 5 x 96 B of statics. */
#include <string.h>
#include "doomo/playas.h"
#include "doomo/resident.h"
#include "doomo/catalog.h"

#define HERO_SEED_SUNNY  0x5A44u
#define HERO_SEED_MARVIN 0x4D41u

static DoomoResident preset[2 + HERO_COOP_FILLED];
static uint8_t preset_ready;

static void build(DoomoResident *r, uint32_t seed, const char *name,
                  int species, int ears, int tail, int pronoun)
{
    resident_generate(r, seed);
    memset(r->name, 0, sizeof(r->name));
    strncpy(r->name, name, sizeof(r->name) - 1);
    RES_SET_SPECIES(r, (uint8_t)species);
    RES_SET_LOOK(r, GF_EARS, (uint8_t)ears);
    RES_SET_LOOK(r, GF_TAIL, (uint8_t)tail);
    RES_SET_LOOK(r, GF_HORNS, HORNS_NONE);
    RES_SET_LOOK(r, GF_WINGS, WINGS_NONE);
    r->status       = RES_ALIVE;
    resident_sanitize(r);
    RES_SET_PRONOUN(r, (uint8_t)pronoun);               /* after sanitize: the contract */
    r->flags   = (uint8_t)((r->flags | RF_PLAYER) & ~RF_HELLBORN);
    r->level   = 0;
}

/* MP1: the co-op heroes of the guest profiles. Each is the look of a fixed seed (what a family pool member made
 * from that seed looks like), a name and a pronoun; a guest who picks one plays a housemate made from it. */
static const struct { uint32_t seed; const char *name; uint8_t pronoun; } coop[HERO_COOP_FILLED] = {
    { 0x00C0E001u, "Ember", PRONOUN_SHE },
    { 0x00C0E002u, "Rook",  PRONOUN_HE },
    { 0x00C0E003u, "Pip",   PRONOUN_THEY },
};

static int coop_slot(int kind) { return kind >= HERO_COOP_FIRST && kind < HERO_COOP_FIRST + HERO_COOP_FILLED ? kind - HERO_COOP_FIRST : -1; }

uint32_t hero_coop_seed(int kind) { int c = coop_slot(kind); return c < 0 ? 0u : coop[c].seed; }

const DoomoResident *hero_preset(int kind)
{
    int c = coop_slot(kind);
    if (kind != HERO_SUNNY && kind != HERO_MARVIN && c < 0) return NULL;
    if (!preset_ready) {
        int i;
        build(&preset[0], HERO_SEED_SUNNY, "Sunny", SPECIES_BUNNYKIN, EARS_BUNNY, TAIL_BUNNY, PRONOUN_SHE);
        build(&preset[1], HERO_SEED_MARVIN, "Marvin", SPECIES_FOXKIN, EARS_FOX, TAIL_FOX, PRONOUN_HE);
        for (i = 0; i < HERO_COOP_FILLED; i++) {
            DoomoResident *r = &preset[2 + i];
            resident_generate(r, coop[i].seed);
            memset(r->name, 0, sizeof(r->name));
            strncpy(r->name, coop[i].name, sizeof(r->name) - 1);
            resident_sanitize(r);
            RES_SET_PRONOUN(r, coop[i].pronoun);
            r->status = RES_ALIVE;
        }
        preset_ready = 1;
    }
    return c >= 0 ? &preset[2 + c] : &preset[kind == HERO_MARVIN];
}
