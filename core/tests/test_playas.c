/* test_playas.c - PA1: play-as state, the story barrier and the PLAY chunk
 * (core/src/playas.c). The PA0 contract checks are in test_contracts_pa0.c. */
#include <string.h>
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/home.h"
#include "doomo/family.h"
#include "doomo/ending.h"
#include "doomo/life_events.h"
#include "doomo/save.h"
#include "doomo/playas.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/story.h"
#include "doomo/town.h"
#include "doomo/sim.h"
#include "doomo/story_vm.h"
#include "../src/story_int.h"
#include "../src/home_int.h"
#include "../src/lib_int.h"
#include "../src/story_int.h"

static void fresh(void)
{
    town_new(TOWN_FREEPLAY, 31, TOWN_POP_PREMADE);
    sim_init();
    doomo_save_ext_playas->reset();
}

static void cleared(void)
{
    doomo_town.mode = TOWN_STORY;
    doomo_town.story.chapter = STORY_CH_DONE;
}

TEST(playas_default_is_the_hero_and_the_chunk_is_empty)
{
    fresh();
    CHECK_EQ(playas_avatar(), PLAYAS_ID_HERO);
    CHECK(playas_is_hero());
    CHECK(!playas_story_locked());
    CHECK_EQ(playas_recent(0), PLAYAS_ID_NONE);
    CHECK_EQ(doomo_save_ext_playas->size(), 0);
    CHECK_EQ(playas_profile_flags() & PLAYAS_HF_PLAYING_OTHER, 0);
}

TEST(playas_locked_until_the_story_is_cleared)
{
    int a;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    doomo_town.mode = TOWN_STORY;
    doomo_town.story.chapter = STORY_CH3;
    CHECK(!playas_available());
    CHECK(!playas_can_be(a));
    CHECK(!playas_switch(a, PLAYAS_SWITCH_INSTANT));
    CHECK_EQ(playas_avatar(), PLAYAS_ID_HERO);
    cleared();
    CHECK(playas_available());
    CHECK(playas_can_be(a));
    CHECK(!playas_can_be(PLAYAS_ID_HERO));           /* already the hero */
}

TEST(playas_switch_there_and_back)
{
    int a, k;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    k = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    cleared();
    CHECK(playas_switch(a, PLAYAS_SWITCH_TAGIN));
    CHECK_EQ(playas_avatar(), a);
    CHECK(!playas_is_hero() && playas_story_locked());
    CHECK_EQ(playas_recent(0), PLAYAS_ID_HERO);
    CHECK_EQ(playas_you_gender(), GENDER_F);
    CHECK(playas_profile_flags() & PLAYAS_HF_PLAYING_OTHER);
    CHECK(!playas_can_be(a));
    CHECK(playas_switch(k, PLAYAS_SWITCH_FADE));
    CHECK_EQ(playas_recent(0), a);
    CHECK_EQ(playas_recent(1), PLAYAS_ID_HERO);
    CHECK_EQ(playas_gender(k), GENDER_M);
    CHECK(playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_TAGIN));
    CHECK(playas_is_hero() && !playas_story_locked());
    CHECK_EQ(playas_recent(0), k);
    CHECK(!playas_switch(-1, PLAYAS_SWITCH_INSTANT));
    CHECK(!playas_switch(PLAYAS_ID_FAMILY_LAST, PLAYAS_SWITCH_INSTANT));
}

TEST(playas_barrier_refuses_story_writes)
{
    int a, flag = 5;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    cleared();
    story_set_flag(flag, 0);
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    story_set_flag(flag, 1);
    CHECK(!story_flag(flag));
    story_set_beat(0);
    CHECK_EQ(story_chapter(), STORY_CH_DONE);
    CHECK(playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_INSTANT));
    story_set_flag(flag, 1);
    CHECK(story_flag(flag));
    story_set_flag(flag, 0);
}

TEST(playas_pets_and_the_lost_cannot_be_played)
{
    int p, a;
    fresh();
    p = family_pool_add(FAMROLE_PET, LIFE_CHILD, 9u, "Rex", PRONOUN_IT);
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    cleared();
    if (p >= 0) CHECK(!playas_can_be(p));
    CHECK(playas_can_be(a));
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    family_remove(a);
    CHECK_EQ(playas_check(), 1);
    CHECK(playas_is_hero());
    CHECK_EQ(playas_recent(0), PLAYAS_ID_NONE);
    CHECK_EQ(playas_check(), 0);
}

TEST(playas_chunk_round_trips_and_clamps)
{
    uint8_t buf[800];
    int a, k;
    PlayasPos pos;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    k = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    cleared();
    playas_set_hero_kind(HERO_MARVIN);
    playas_set_hero_pos(120, 64, 3, 2);
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    CHECK(playas_switch(k, PLAYAS_SWITCH_INSTANT));
    CHECK_EQ(doomo_save_ext_playas->size(), 32 + 96 + 48);   /* header + the hero's stash + Ann's pocket */
    CHECK_EQ(doomo_save_ext_playas->write(buf, sizeof(buf)), 32 + 96 + 48);
    doomo_save_ext_playas->reset();
    CHECK(playas_is_hero());
    CHECK_EQ(playas_hero_kind(), HERO_CUSTOM);
    CHECK(doomo_save_ext_playas->read(buf, 32 + 96 + 48, 1));
    CHECK_EQ(playas_avatar(), k);
    CHECK_EQ(playas_hero_kind(), HERO_MARVIN);
    CHECK_EQ(playas_recent(0), a);
    CHECK_EQ(playas_recent(1), PLAYAS_ID_HERO);
    CHECK(playas_hero_pos(&pos));
    CHECK(pos.x == 120 && pos.y == 64 && pos.map == 3 && pos.dir == 2);
    CHECK((playas_profile_flags() & PLAYAS_HF_HERO_MASK) == PLAYAS_HF_MAKE_HERO(HERO_MARVIN));
    family_remove(k);                                 /* gone by load time: clamped */
    CHECK(doomo_save_ext_playas->read(buf, 32 + 96 + 48, 1));
    CHECK(playas_is_hero());
    CHECK(!doomo_save_ext_playas->read(buf, 10, 1));  /* short chunk resets */
    CHECK(playas_is_hero());
    CHECK_EQ(playas_hero_kind(), HERO_CUSTOM);
}

TEST(playas_gender_codes_and_avatar_name)
{
    char o[64];
    int a;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    cleared();
    vm_fmt(o, sizeof o, "He said @( he | she | they ) won. @P!");
    CHECK(strstr(o, "won.") != NULL);                         /* hero default */
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    vm_fmt(o, sizeof o, "@(he|she|they) won, @P.");
    CHECK(strcmp(o, "she won, Ann.") == 0);
    vm_fmt(o, sizeof o, "@(a|b) @(x");
    CHECK(strcmp(o, "b @(x") == 0);
    vm_fmt(o, sizeof o, "@(a) and @@");
    CHECK(strcmp(o, "a and @") == 0);
}

TEST(playas_scene_lists_and_switches)
{
    int a;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    cleared();
    CHECK(!scene_is_reserved_stub(&SCENE_PLAYAS));
    CHECK(playas_can_be(a));
    scene_set(&SCENE_UI_GALLERY, 0);
    scene_push(&SCENE_PLAYAS, 0);
    CHECK(scene_top() == &SCENE_PLAYAS);
    plat_begin_frame(); scene_update(); scene_draw();
    CHECK(scene_top() == &SCENE_PLAYAS);              /* it stays (not a stub) */
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));   /* the list now offers the hero first */
    plat_begin_frame(); scene_draw();
    scene_pop();
}

/* (PA8) your own house draws whoever you play: the Live-mode frame with a
 * family member as the avatar differs from the hero's. */
static uint32_t own_house_live_frame(void)
{
    static const int seq[] = { BTN_START, 0, BTN_DOWN, 0, BTN_A, 0, 0, 0, 0, 0 };
    uint32_t h = 2166136261u;
    int i;
    home_reset();
    scene_set(&SCENE_HOME, HOME_ARG_OWN(HOME_COTTAGE));
    for (i = 0; i < 10; i++) { plat_begin_frame(); doomo_frame(seq[i]); plat_end_frame(); }
    for (i = 0; i < 10; i++) doomo_frame(0);
    for (i = 0; i < nat_w * nat_h; i++) { h ^= nat_fb[i]; h *= 16777619u; }
    return h;
}

TEST(playas_house_draws_the_avatar_body)
{
    int a;
    uint32_t hero, other;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(a >= 0);
    cleared();
    hero = own_house_live_frame();
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    other = own_house_live_frame();
    CHECK(hero != other);
}

/* (PA8) the almanac's "known" test follows the avatar: a villager you play
 * knows the people they have ties with, not the hero's. */
TEST(playas_almanac_follows_the_avatar)
{
    int v = -1, o = -1, i;
    fresh();
    cleared();
    for (i = 0; i < DOOMO_PLAYER_ID && v < 0; i++)
        if (playas_can_be(i)) v = i;
    CHECK(v >= 0);
    for (i = 0; i < DOOMO_PLAYER_ID; i++) {
        const DoomoResident *r = town_res(i);
        if (i != v && r && r->status != RES_EMPTY && r->level == 0 && !(r->flags & (RF_FAVORITE | RF_PLAYER)) &&
            !rel_find(i, DOOMO_PLAYER_ID) && !rel_find(i, v)) { o = i; break; }
    }
    CHECK(o >= 0);
    CHECK(!lib_known(o));
    rel_adjust(o, v, 10);
    CHECK(!lib_known(o));                   /* still the hero's almanac */
    CHECK(playas_switch(v, PLAYAS_SWITCH_INSTANT));
    CHECK(lib_known(o));
}

/* (PA8) care bonds go to whoever you play, not always the hero. */
TEST(playas_care_bonds_with_the_avatar)
{
    int v = -1, o = -1, i;
    fresh();
    cleared();
    CHECK_EQ(playas_slot(), DOOMO_PLAYER_ID);
    for (i = 0; i < DOOMO_PLAYER_ID && v < 0; i++)
        if (playas_can_be(i)) v = i;
    CHECK(v >= 0);
    for (i = 0; i < DOOMO_PLAYER_ID; i++) {
        const DoomoResident *r = town_res(i);
        if (i != v && r && r->status == RES_ALIVE && !rel_find(i, v) && !rel_find(i, DOOMO_PLAYER_ID)) { o = i; break; }
    }
    CHECK(o >= 0);
    CHECK(playas_switch(v, PLAYAS_SWITCH_INSTANT));
    CHECK_EQ(playas_slot(), v);
    sim_play(o);
    CHECK(rel_find(o, v) != NULL);
    CHECK(rel_find(o, DOOMO_PLAYER_ID) == NULL);
}

/* (PA6) New life: only from a finished story, starts as a villager, sets the
 * header flag, and keeps play-as open and the hero switchable. */
TEST(playas_new_life_starts_as_a_villager)
{
    int v = -1, i;
    fresh();
    doomo_town.mode = TOWN_STORY;
    doomo_town.story.chapter = STORY_CH3;
    CHECK(!playas_new_life_ok());
    CHECK(!playas_new_life(0));                      /* story not finished */
    cleared();
    CHECK(playas_new_life_ok());
    CHECK(!playas_new_life(PLAYAS_ID_HERO));         /* never the hero */
    CHECK_EQ(playas_profile_flags() & PLAYAS_HF_NEW_LIFE, 0);
    for (i = 0; i < DOOMO_PLAYER_ID && v < 0; i++) if (playas_can_be(i)) v = i;
    CHECK(v >= 0);
    CHECK(playas_new_life(v));
    CHECK_EQ(playas_avatar(), v);
    CHECK(playas_profile_flags() & PLAYAS_HF_NEW_LIFE);
    CHECK(!playas_new_life_ok());                    /* only once */
    CHECK(playas_story_locked());
    CHECK(playas_can_be(PLAYAS_ID_HERO));            /* the hero is a resident now */
    CHECK(playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_INSTANT));
    CHECK(playas_available());
}

/* (LS5 / PA7) the ending facts read the household; the story's life hooks
 * answer; a side effect on you hits the avatar, not the hero. */
TEST(ls5_life_hooks_and_facts)
{
    LifeEndingFacts f;
    int a;
    fresh();
    a = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 7u, "Kit", PRONOUN_THEY);
    CHECK(a >= 0);
    life_ending_facts(&f);
    CHECK_EQ(f.kids, 1);
    CHECK(f.household >= 2);
    CHECK(f.happiness <= 100);
    CHECK_EQ(f.partner_adult, 0);                    /* no partner */
    side_callback(CB_LIFE_KIDS, 0);
    CHECK_EQ(vm_var(0), 1);
    side_callback(CB_LIFE_HOUSEHOLD, 0);
    CHECK_EQ(vm_var(0), f.household);
    side_callback(CB_LIFE_PARTNER, 0);
    CHECK_EQ(vm_var(0), -1);
    side_callback(CB_LIFE_FAMILY_ADD, 99999);        /* nobody there */
    CHECK(vm_var(0) < 0);
}

/* ---- PA10 part 2: per-avatar pockets ---------------------------------------------------------
 * Money, the bag and health belong to whoever you play (user answer 13). */
static void hero_things(uint32_t money, int hp)
{
    memset(doomo_town.items, 0, sizeof doomo_town.items);
    doomo_town.money = money;
    doomo_town.story.hp = (uint8_t)hp;
    doomo_town.story.armor = 30;
    doomo_town.items[ITEM_CUPCAKE] = 5;
    doomo_town.items[ITEM_ROSARY] = 2;
    doomo_town.items[ITEM_HELLSTEEL] = 9;         /* a materials stack past the 16 biggest of a villager is still the hero's */
}

TEST(pa10_pockets_follow_the_avatar)
{
    int a, k;
    uint32_t m = 0;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    k = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 6u, "Kit", PRONOUN_HE);
    cleared();
    hero_things(500, 80);
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    CHECK(doomo_town.money >= 40 && doomo_town.money < 200);        /* the sim's small purse, not the hero's 500 */
    CHECK_EQ(doomo_town.story.hp, 100);
    CHECK_EQ(doomo_town.items[ITEM_ROSARY], 0);
    CHECK_EQ(doomo_town.items[ITEM_HELLSTEEL], 0);
    CHECK(playas_pocket_money(PLAYAS_ID_HERO, &m) && m == 500);
    CHECK_EQ(playas_pocket_count(), 2);                              /* the hero's stash */
    doomo_town.money = 7;                                            /* Ann spends and shops */
    doomo_town.items[ITEM_PLUSHIE] = 3;
    doomo_town.story.hp = 55;
    CHECK(playas_switch(k, PLAYAS_SWITCH_INSTANT));
    CHECK(playas_pocket_money(a, &m) && m == 7);
    CHECK_EQ(playas_pocket_count(), 3);
    doomo_town.money += 1000;                                        /* Kit's own purse */
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));                  /* Ann has everything back */
    CHECK_EQ(doomo_town.money, 7);
    CHECK_EQ(doomo_town.items[ITEM_PLUSHIE], 3);
    CHECK_EQ(doomo_town.story.hp, 55);
    CHECK(playas_pocket_money(k, &m) && m >= 1000);
    CHECK(playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_INSTANT));     /* and the hero's whole bag, armour too */
    CHECK_EQ(doomo_town.money, 500);
    CHECK_EQ(doomo_town.story.hp, 80);
    CHECK_EQ(doomo_town.story.armor, 30);
    CHECK_EQ(doomo_town.items[ITEM_CUPCAKE], 5);
    CHECK_EQ(doomo_town.items[ITEM_ROSARY], 2);
    CHECK_EQ(doomo_town.items[ITEM_HELLSTEEL], 9);
    CHECK_EQ(doomo_town.items[ITEM_PLUSHIE], 0);
    CHECK_EQ(playas_pocket_count(), 2);                              /* Ann's and Kit's; the stash is spent */
    CHECK(!playas_pocket_money(PLAYAS_ID_HERO, &m));
}

TEST(pa10_default_pocket_is_stable_and_never_random)
{
    int a;
    uint32_t m1, m2;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    cleared();
    hero_things(100, 100);
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    m1 = doomo_town.money;
    CHECK(playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_INSTANT));
    fresh(); a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE); cleared();
    hero_things(100, 100);
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    m2 = doomo_town.money;
    CHECK_EQ(m1, m2);                                                /* same seed, same id, same purse */
    CHECK(doomo_town.items[ITEM_CUPCAKE] + doomo_town.items[ITEM_ONIGIRI] + doomo_town.items[ITEM_SOUL_SOUP] +
          doomo_town.items[ITEM_BRIMSTONE_BAGEL] + doomo_town.items[ITEM_CANDY_EYEBALLS] + doomo_town.items[ITEM_PUDDING] +
          doomo_town.items[ITEM_HELL_PEPPERS] + doomo_town.items[ITEM_ANGEL_CAKE] + doomo_town.items[ITEM_MEAT_SLAB] +
          doomo_town.items[ITEM_SALAD] + doomo_town.items[ITEM_MILK] + doomo_town.items[ITEM_COFFEE] >= 2);   /* two snacks */
}

TEST(pa10_pockets_round_trip_and_the_chunk_stays_under_800)
{
    uint8_t buf[1024];
    int a, k, i, tried = 0;
    uint32_t sz, m = 0;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    k = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 6u, "Kit", PRONOUN_HE);
    cleared();
    hero_things(500, 80);
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    doomo_town.money = 7; doomo_town.items[ITEM_PLUSHIE] = 3;
    CHECK(playas_switch(k, PLAYAS_SWITCH_INSTANT));
    doomo_town.money = 1234;
    sz = doomo_save_ext_playas->write(buf, sizeof(buf));
    CHECK_EQ(sz, 32 + 96 + 48);
    doomo_save_ext_playas->reset();
    CHECK(playas_is_hero() && playas_pocket_count() == 0);
    CHECK(doomo_save_ext_playas->read(buf, sz, 1));
    CHECK_EQ(playas_avatar(), k);
    CHECK(playas_pocket_money(PLAYAS_ID_HERO, &m) && m == 500);
    CHECK(playas_pocket_money(a, &m) && m == 7);
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    CHECK_EQ(doomo_town.items[ITEM_PLUSHIE], 3);
    CHECK(playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_INSTANT));
    CHECK_EQ(doomo_town.money, 500);
    CHECK_EQ(doomo_town.items[ITEM_HELLSTEEL], 9);
    /* play as many villagers: only the 14 newest pockets stay, and the chunk never passes 800 B */
    for (i = 0; i < DOOMO_PLAYER_ID && tried < 20; i++)
        if (playas_can_be(i)) { CHECK(playas_switch(i, PLAYAS_SWITCH_INSTANT)); doomo_town.money = 1000u + (uint32_t)i; tried++; }
    CHECK(tried == 20);
    CHECK(playas_pocket_count() <= PLAY_POCKETS);
    sz = doomo_save_ext_playas->size();
    CHECK(sz <= SAVE_EXT_MAX_PLAY);
    CHECK_EQ(doomo_save_ext_playas->write(buf, sizeof(buf)), sz);
    CHECK(sz + 0u <= 800u);
}

TEST(pa10_hostile_chunk_is_clamped)
{
    uint8_t buf[800];
    int a, i;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    cleared();
    hero_things(500, 80);
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    CHECK(doomo_save_ext_playas->write(buf, sizeof(buf)) == 32 + 96);
    buf[32 + 2] = 250; buf[32 + 3] = 0;                 /* hero stash health out of range */
    buf[32 + 8 + ITEM_CUPCAKE] = 200;                    /* a stack over 99 */
    CHECK(doomo_save_ext_playas->read(buf, 32 + 96, 1));
    CHECK(playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_INSTANT));
    CHECK_EQ(doomo_town.story.hp, 100);
    CHECK_EQ(doomo_town.items[ITEM_CUPCAKE], 99);
    /* a count that claims more slots than the bytes hold, and garbage ids */
    memset(buf, 0, sizeof buf);
    buf[0] = 1; buf[2] = (uint8_t)(PLAYAS_ID_HERO & 255); buf[3] = (uint8_t)(PLAYAS_ID_HERO >> 8); buf[22] = 16;
    for (i = 32; i < 800; i++) buf[i] = (uint8_t)(i * 37);
    CHECK(doomo_save_ext_playas->read(buf, 100, 1));
    CHECK(playas_is_hero());
    CHECK(playas_pocket_count() <= PLAY_POCKETS);
}

TEST(pa10_an_old_save_mid_play_keeps_what_is_carried)
{
    uint8_t buf[800];
    int a;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    cleared();
    hero_things(321, 90);
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    CHECK(doomo_save_ext_playas->write(buf, sizeof(buf)) >= 32);
    memset(buf + 22, 0, 3);                              /* the 32 B header of a save from before PA10 */
    doomo_save_ext_playas->reset();
    CHECK(doomo_save_ext_playas->read(buf, 32, 1));
    CHECK_EQ(playas_avatar(), a);
    doomo_town.money = 77;
    CHECK(playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_INSTANT));
    CHECK_EQ(doomo_town.money, 77);                      /* no stash: the shared behaviour, nothing lost */
}

TEST(pa10_a_lost_avatar_sends_the_hero_home_with_the_hero_bag)
{
    int a;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    cleared();
    hero_things(500, 80);
    CHECK(playas_switch(a, PLAYAS_SWITCH_INSTANT));
    doomo_town.money = 9;
    family_remove(a);
    CHECK(playas_check());
    CHECK(playas_is_hero());
    CHECK_EQ(doomo_town.money, 500);
    CHECK_EQ(doomo_town.items[ITEM_ROSARY], 2);
}

/* ---- PA10 part 3: New life from the title, and in a fresh world ---------------------------------- */
TEST(pa10_a_fresh_free_play_world_can_start_a_new_life)
{
    int a;
    uint8_t buf[800];
    fresh();                                              /* a brand-new free play island */
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    CHECK(!playas_new_life_ok());                         /* not armed: free play alone is not a New life */
    playas_new_life_arm();
    CHECK(playas_new_life_ok());
    CHECK(playas_new_life(a));
    CHECK_EQ(playas_avatar(), a);
    CHECK(playas_profile_flags() & PLAYAS_HF_NEW_LIFE);
    CHECK(!playas_new_life_ok());                         /* one New life per world */
    CHECK(doomo_save_ext_playas->write(buf, sizeof(buf)) >= 32);
    doomo_save_ext_playas->reset();
    CHECK(doomo_save_ext_playas->read(buf, 32, 1));
    CHECK(playas_profile_flags() & PLAYAS_HF_NEW_LIFE);   /* the flag survives the save */
}

TEST(pa10_arming_is_for_free_play_and_ends_with_the_world)
{
    int a;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    doomo_town.mode = TOWN_STORY;                         /* an unfinished story world refuses */
    doomo_town.story.chapter = 2;
    playas_new_life_arm();
    CHECK(!playas_new_life_ok());
    CHECK(!playas_new_life(a));
    fresh();                                              /* a new world: armed state is gone */
    playas_new_life_arm();
    CHECK(playas_new_life_ok());
    town_new(TOWN_FREEPLAY, 77, TOWN_POP_PREMADE);
    CHECK(!playas_new_life_ok());
    CHECK(playas_is_hero());
}

TEST(pa10_scene_opens_on_the_new_life_list_from_the_title)
{
    int a, b;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    cleared();
    scene_set(&SCENE_UI_GALLERY, 0);
    scene_push(&SCENE_PLAYAS, PLAYAS_ARG_PICK_NEWLIFE);
    plat_begin_frame(); scene_update(); scene_draw();
    CHECK(scene_top() == &SCENE_PLAYAS);                  /* the list is open, no switcher row first */
    CHECK(playas_is_hero());
    scene_pop();
    CHECK(playas_new_life(a));                            /* once it is a New life the arg opens the plain switcher */
    b = family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 6u, "Kit", PRONOUN_HE);
    scene_push(&SCENE_PLAYAS, PLAYAS_ARG_PICK_NEWLIFE);
    plat_begin_frame(); scene_update(); scene_draw();
    CHECK(scene_top() == &SCENE_PLAYAS);
    CHECK(playas_can_be(b));
    scene_pop();
}

/* ---- PA10 part 1: the play-as stress save ----------------------------------------------------
 * A finished story, a full household (pets and a grown-up child among them), a New life played
 * as a family member, life events fired, a memorial: save it, check it stays under PA10's
 * 32,768 B gate (36 KB cap minus a 4 KB margin), load it back, and see the same person. */
static uint8_t pa10_image[DOOMO_SAVE_SLOT1_MAX + 4096];

TEST(pa10_stress_save_round_trips_under_the_gate)
{
    int i, n = 0, who = -1, lost = -1, pets = 0;
    uint32_t sz;
    DoomoSaveInfo info;
    fresh();
    cleared();
    family_tier_story_done();                            /* tier 1, then tier 2: the biggest household */
    family_tier_story_done();
    for (i = 0; i < FAMILY_MAX; i++) {
        int role = (i % 5 == 4) ? FAMROLE_PET : (i % 3 == 0 ? FAMROLE_CHILD : FAMROLE_HOUSEMATE);
        int stage = role == FAMROLE_PET ? LIFE_ADULT : (i % 4 == 1 ? LIFE_TEEN : LIFE_ADULT);
        int id = family_pool_add(role, stage, 100u + (uint32_t)i, "Pa Ten", PRONOUN_THEY);
        if (id < 0) break;
        n++;
        if (role == FAMROLE_PET) pets++;
        else if (who < 0 && playas_can_be(id)) who = id;
        else if (lost < 0 && role != FAMROLE_PET) lost = id;
    }
    CHECK(n >= 10 && who >= 0 && pets >= 1);
    CHECK(playas_new_life_ok());
    CHECK(playas_new_life(who));
    CHECK_EQ(playas_avatar(), who);
    if (lost >= 0) family_lose(lost, 0);                 /* a memorial in the save */
    sz = save_serialize(pa10_image, sizeof(pa10_image));
    CHECK(sz > 0);
    printf("      PA10 stress save: %u B for %d members (%d pets), gate 32768 B\n", (unsigned)sz, n, pets);
    CHECK(sz + 0u <= 32768u);
    CHECK(save_info_from(pa10_image, sz, &info));
    CHECK(info.flags & PLAYAS_HF_NEW_LIFE);
    CHECK(info.flags & PLAYAS_HF_PLAYING_OTHER);
    memset(&doomo_town, 0x5A, sizeof(doomo_town));
    doomo_save_ext_playas->reset();
    CHECK(save_deserialize(pa10_image, sz));
    CHECK_EQ(playas_avatar(), who);                      /* the same person after the load */
    CHECK(playas_profile_flags() & PLAYAS_HF_NEW_LIFE);
    CHECK(playas_story_locked());                        /* and the story still belongs to the hero */
    CHECK_EQ(family_count_role(FAMROLE_PET), pets);
}

/* ---- Z1: the joint gate across play-as, family, life events and endings --------------------
 * One save holds all four at once. Playing as every household member must leave the story
 * exactly as it was (the PA4/Z1 tour: fingerprint + flags), and the whole lot must survive a
 * save and load together under the PA10 size gate. */
TEST(z1_joint_gate_across_the_four_areas)
{
    int i, ids[FAMILY_MAX], n = 0, switched = 0, flag = 7, pre_flag;
    uint32_t fp0, sz, ev_total;
    int seen1, seen2, cnt;
    fresh();
    cleared();
    family_tier_story_done();
    family_tier_story_done();
    for (i = 0; i < FAMILY_MAX; i++) {
        int role = (i % 4 == 3) ? FAMROLE_PET : (i % 2 ? FAMROLE_CHILD : FAMROLE_HOUSEMATE);
        int stage = role == FAMROLE_PET ? LIFE_ADULT : (i % 5 == 1 ? LIFE_ELDER : LIFE_ADULT);
        int id = family_pool_add(role, stage, 300u + (uint32_t)i, "Zed", PRONOUN_THEY);
        if (id < 0) break;
        ids[n++] = id;
    }
    CHECK(n >= 10);
    /* endings and life events share the save */
    ending_mark(ENDING_E01);
    ending_mark(ENDING_E02);
    life_event_fire(LEV_LUCKY_FIND);
    life_event_fire(LEV_PICNIC);
    life_event_fire(LEV_MOVIE_NIGHT);
    ev_total = (uint32_t)life_event_total();
    CHECK(ev_total >= 2);
    pre_flag = story_flag(flag);
    fp0 = story_fingerprint();
    /* the tour: be everyone who can be played, try to write the story, come back */
    for (i = 0; i < n; i++) {
        if (!playas_can_be(ids[i])) continue;
        CHECK(playas_switch(ids[i], PLAYAS_SWITCH_INSTANT));
        switched++;
        story_set_flag(flag, !pre_flag);                  /* refused while someone else is played */
        story_set_beat(0);
        CHECK_EQ(story_flag(flag), pre_flag);
        CHECK_EQ((int)story_fingerprint() == (int)fp0, 1);
        CHECK(playas_switch(PLAYAS_ID_HERO, PLAYAS_SWITCH_INSTANT));
    }
    CHECK(switched >= 5);
    CHECK_EQ((int)story_fingerprint() == (int)fp0, 1);
    /* one more avatar for the save, then the whole thing round-trips together */
    for (i = 0; i < n; i++) if (playas_can_be(ids[i])) { CHECK(playas_switch(ids[i], PLAYAS_SWITCH_INSTANT)); break; }
    seen1 = ending_seen(ENDING_E01); seen2 = ending_seen(ENDING_E02);
    cnt = family_count();
    sz = save_serialize(pa10_image, sizeof(pa10_image));
    CHECK(sz > 0 && sz <= 32768u);
    printf("      Z1 joint save: %u B (%d members, %d endings seen, %u events)\n",
           (unsigned)sz, cnt, ending_count_seen(), (unsigned)ev_total);
    {
        int who = playas_avatar();
        memset(&doomo_town, 0x5A, sizeof(doomo_town));
        doomo_save_ext_playas->reset();
        CHECK(save_deserialize(pa10_image, sz));
        CHECK_EQ(playas_avatar(), who);
        CHECK_EQ(family_count(), cnt);
        CHECK_EQ(ending_seen(ENDING_E01), seen1);
        CHECK_EQ(ending_seen(ENDING_E02), seen2);
        CHECK_EQ(life_event_total(), (int)ev_total);
        CHECK_EQ((int)story_fingerprint() == (int)fp0, 1);
        CHECK(playas_story_locked());
    }
}
