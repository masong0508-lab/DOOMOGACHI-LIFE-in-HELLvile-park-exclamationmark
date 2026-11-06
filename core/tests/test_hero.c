/* test_hero.c - PA2: hero presets and the new-game default player. */
#include <string.h>
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/playas.h"
#include "doomo/story.h"
#include "doomo/town.h"
#include "doomo/sim.h"
#include "doomo/resident.h"
#include "doomo/catalog.h"
#include "doomo/save.h"

TEST(hero_presets_exist_for_sunny_and_marvin_only)
{
    const DoomoResident *s = hero_preset(HERO_SUNNY), *m = hero_preset(HERO_MARVIN);
    CHECK(hero_preset(HERO_CUSTOM) == NULL);
    CHECK(hero_preset(HERO_COOP_FIRST) != NULL);               /* MP1: the filled co-op heroes have presets */
    CHECK(hero_preset(HERO_COOP_LAST) == NULL);                /* the rest stay reserved */
    CHECK(s && m);
    if (!s || !m) return;
    CHECK(strcmp(s->name, "Sunny") == 0 && strcmp(m->name, "Marvin") == 0);
    CHECK_EQ(RES_PRONOUN(s), PRONOUN_SHE);
    CHECK_EQ(RES_PRONOUN(m), PRONOUN_HE);
    CHECK_EQ(RES_SPECIES(s), SPECIES_BUNNYKIN);
    CHECK_EQ(RES_SPECIES(m), SPECIES_FOXKIN);
    CHECK(s->flags & RF_PLAYER);
    CHECK(!(m->flags & RF_HELLBORN));
}

TEST(hero_new_story_game_uses_the_chosen_preset)
{
    town_new(TOWN_STORY, 77, TOWN_POP_PREMADE);
    sim_init();
    playas_set_hero_kind(HERO_MARVIN);
    story_new_game();
    CHECK(strcmp(doomo_town.player.name, "Marvin") == 0);
    CHECK_EQ(RES_PRONOUN(&doomo_town.player), PRONOUN_HE);
    CHECK_EQ(playas_you_gender(), GENDER_M);
    town_new(TOWN_STORY, 77, TOWN_POP_PREMADE);
    playas_set_hero_kind(HERO_SUNNY);
    story_new_game();
    CHECK(strcmp(doomo_town.player.name, "Sunny") == 0);
    CHECK_EQ(playas_you_gender(), GENDER_F);
    playas_set_hero_kind(HERO_CUSTOM);
}

TEST(hero_created_name_is_never_overwritten)
{
    town_new(TOWN_STORY, 78, TOWN_POP_PREMADE);
    memcpy(doomo_town.player.name, "Zed", 4);
    playas_set_hero_kind(HERO_MARVIN);
    story_new_game();
    CHECK(strcmp(doomo_town.player.name, "Zed") == 0);
    playas_set_hero_kind(HERO_CUSTOM);
}

/* the hero kind rides in header byte 47 so menus can say Sunny / Marvin
 * without loading the save; an old (custom) hero reads 0 there */
TEST(hero_kind_is_in_the_save_header_byte)
{
    DoomoSaveInfo info;
    int k;
    static const int kinds[3] = { HERO_SUNNY, HERO_MARVIN, HERO_CUSTOM };
    for (k = 0; k < 3; k++) {
        town_new(TOWN_STORY, 80 + (uint32_t)k, TOWN_POP_PREMADE);
        sim_init();
        playas_set_hero_kind(kinds[k]);
        story_new_game();
        CHECK(save_game(0));
        CHECK(save_peek(0, &info));
        CHECK_EQ(PLAYAS_HF_HERO(info.flags), kinds[k]);
        CHECK_STR(info.player, kinds[k] == HERO_SUNNY ? "Sunny" : kinds[k] == HERO_MARVIN ? "Marvin" : info.player);
    }
    playas_set_hero_kind(HERO_CUSTOM);
}

/* MP1 content: the co-op heroes' final names, looks, side quests and abilities (docs/COOP_HEROES.md). */
TEST(hero_coop_heroes_have_final_names_looks_quests_and_abilities)
{
    static const char *const names[HERO_COOP_FILLED] = { "Ember", "Rook", "Wick" };
    int k, q, a;
    CHECK(hero_coop_info(HERO_SUNNY) == NULL && hero_coop_info(HERO_CUSTOM) == NULL);
    CHECK(hero_coop_info(HERO_COOP_LAST) == NULL);
    for (k = 0; k < HERO_COOP_FILLED; k++) {
        const HeroCoopInfo *i = hero_coop_info(HERO_COOP_FIRST + k);
        DoomoResident r;
        CHECK(i != NULL);
        if (!i) continue;
        CHECK_STR(i->name, names[k]);
        CHECK_STR(hero_preset(HERO_COOP_FIRST + k)->name, names[k]);
        CHECK(strcmp(i->name, "Pip") != 0);                         /* Pip is the show's child */
        CHECK(i->title && i->line && i->quest[0]);
        for (q = 0; q < HERO_COOP_QUESTS; q++) CHECK(i->quest[q] && i->quest[q][0]);
        for (a = 0; a < HERO_COOP_ABILITIES; a++) CHECK(i->ability[a] && i->amount[a] != 0 && i->stat[a] < STAT_COUNT);
        resident_generate(&r, hero_coop_seed(HERO_COOP_FIRST + k)); /* the seed makes the final look */
        CHECK_EQ(RES_SPECIES(&r), i->species);
        CHECK_EQ(RES_LOOK(&r, GF_EARS), i->ears);
        CHECK_EQ(RES_LOOK(&r, GF_TAIL), i->tail);
        CHECK(RES_STAGE(hero_preset(HERO_COOP_FIRST + k)) == LIFE_ADULT);
    }
    CHECK(hero_coop_seed(HERO_COOP_FIRST) != hero_coop_seed(HERO_COOP_FIRST + 1));
    CHECK(hero_coop_seed(HERO_COOP_FIRST + 1) != hero_coop_seed(HERO_COOP_FIRST + 2));
}
