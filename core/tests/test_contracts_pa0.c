/* test_contracts_pa0.c - the play-as contract (playas.h) and the save
 * header's byte 47 (LIFESIM_PLAN PA0, R9, R13 + section 6). Owner: playas.
 *
 * Like test_contracts_k0.c, every check holds before and after the owners
 * land (PA1 playas.c, PA2 hero.c, PA9, MP0): they test a fresh town, where
 * the hero is the avatar and play-as is not unlocked, ranges and layouts,
 * and the header byte against playas_profile_flags() rather than against 0.
 * PA1's own behaviour (switching, the barrier, the PLAY chunk) is
 * test_playas.c's.
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/playas.h"

static uint8_t image[256 * 1024];

static void put_crc(uint8_t *buf, uint32_t n)
{
    uint32_t c = save_crc32(buf + 16, n - 16);
    buf[12] = (uint8_t)c;
    buf[13] = (uint8_t)(c >> 8);
    buf[14] = (uint8_t)(c >> 16);
    buf[15] = (uint8_t)(c >> 24);
}

TEST(pa0_ids_and_hero_kinds)
{
    int k;
    CHECK_EQ(PLAYAS_ID_HERO, DOOMO_PLAYER_ID);
    CHECK_EQ(PLAYAS_ID_FAMILY_FIRST, 1001);                  /* R13 */
    CHECK_EQ(PLAYAS_ID_FAMILY_LAST, 1020);
    CHECK_EQ(PLAYAS_ID_FAMILY_LAST - PLAYAS_ID_FAMILY_FIRST + 1, HOUSEHOLD_CAP_MAX);
    CHECK(PLAYAS_ID_FAMILY_LAST <= 0xFFFF);                  /* the PLAY chunk's u16 */
    CHECK(PLAYAS_ID_NONE < 0);
    /* SUNNY, MARVIN, make your own, and the reserved co-op block */
    CHECK_EQ(HERO_CUSTOM, 0);
    CHECK(HERO_SUNNY != HERO_MARVIN && HERO_SUNNY != HERO_CUSTOM);
    CHECK(HERO_IS_MAIN(HERO_CUSTOM) && HERO_IS_MAIN(HERO_SUNNY) && HERO_IS_MAIN(HERO_MARVIN));
    CHECK_EQ(HERO_COOP_FIRST, HERO_MARVIN + 1);
    CHECK(HERO_COOP_LAST - HERO_COOP_FIRST + 1 >= PROF_GUESTS);   /* one per guest at least */
    CHECK_EQ(HERO_KIND_COUNT, HERO_COOP_LAST + 1);
    for (k = 0; k < HERO_KIND_COUNT; k++) {
        CHECK(HERO_IS_MAIN(k) != HERO_IS_COOP(k));
        /* every kind fits the header's 3 bits and comes back out */
        CHECK_EQ(PLAYAS_HF_HERO(PLAYAS_HF_MAKE_HERO(k)), k);
        CHECK_EQ(PLAYAS_HF_MAKE_HERO(k) & ~PLAYAS_HF_HERO_MASK, 0);
    }
    CHECK(!HERO_IS_COOP(HERO_KIND_COUNT) && !HERO_IS_MAIN(-1));
    /* the flag bits are distinct and fill the byte exactly */
    CHECK_EQ(PLAYAS_HF_STORY_CLEARED | PLAYAS_HF_PLAYING_OTHER | PLAYAS_HF_NEW_LIFE |
             PLAYAS_HF_PROFILES | PLAYAS_HF_HERO_MASK | PLAYAS_HF_RESERVED, 0xFF);
    CHECK_EQ(PLAYAS_HF_STORY_CLEARED + PLAYAS_HF_PLAYING_OTHER + PLAYAS_HF_NEW_LIFE +
             PLAYAS_HF_PROFILES + PLAYAS_HF_HERO_MASK + PLAYAS_HF_RESERVED, 0xFF);
    CHECK_EQ(PLAYAS_PROFILES_MAX, PROF_PROFILES_MAX);
    CHECK(GENDER_N != GENDER_F && GENDER_F != GENDER_M && GENDER_M != GENDER_N);
}

TEST(pa0_fresh_town_plays_the_hero)
{
    PlayasPos pos;
    int p;
    town_new(TOWN_STORY, 7, TOWN_POP_PREMADE);
    CHECK_EQ(playas_avatar(), PLAYAS_ID_HERO);
    CHECK(playas_is_hero());
    CHECK(!playas_story_locked());
    CHECK(!playas_available());               /* the story is not cleared */
    CHECK(!playas_hero_pos(&pos));            /* the hero is the avatar */
    CHECK(HERO_IS_MAIN(playas_hero_kind()));
    /* invalid ids are never playable */
    CHECK(!playas_can_be(-1));
    CHECK(!playas_can_be(PLAYAS_ID_FAMILY_LAST + 1));
    CHECK(!playas_can_be(0xFFFF));
    /* a switch while play-as is locked away changes nothing */
    CHECK(!playas_switch(0, PLAYAS_SWITCH_INSTANT));
    CHECK_EQ(playas_avatar(), PLAYAS_ID_HERO);
    /* the recent ring holds only play-as ids */
    for (p = 0; p < 8; p++) {
        int id = playas_recent(p);
        CHECK(id == PLAYAS_ID_NONE || (id >= 0 && id <= PLAYAS_ID_FAMILY_LAST));
    }
    /* profile 0 is the hero; nobody else has moved in */
    CHECK_EQ(playas_profile(), 0);
    CHECK_EQ(playas_profile_avatar(0), playas_avatar());
    CHECK_EQ(playas_profile_hero(0), playas_hero_kind());
    for (p = 1; p < PLAYAS_PROFILES_MAX; p++) {
        CHECK_EQ(playas_profile_avatar(p), PLAYAS_ID_NONE);
        CHECK_EQ(playas_profile_hero(p), -1);
    }
    CHECK_EQ(playas_profile_avatar(PLAYAS_PROFILES_MAX), PLAYAS_ID_NONE);
    CHECK(hero_preset(HERO_CUSTOM) == NULL);
    CHECK(hero_preset(HERO_COOP_LAST) == NULL);       /* reserved, unfilled */
    CHECK(hero_preset(HERO_KIND_COUNT) == NULL);
}

TEST(pa0_gender_from_pronoun)
{
    town_new(TOWN_STORY, 7, TOWN_POP_PREMADE);
    RES_SET_PRONOUN(&doomo_town.res[5], PRONOUN_SHE);
    CHECK_EQ(playas_gender(5), GENDER_F);
    RES_SET_PRONOUN(&doomo_town.res[5], PRONOUN_HE);
    CHECK_EQ(playas_gender(5), GENDER_M);
    RES_SET_PRONOUN(&doomo_town.res[5], PRONOUN_THEY);
    CHECK_EQ(playas_gender(5), GENDER_N);
    RES_SET_PRONOUN(&doomo_town.res[5], PRONOUN_IT);
    CHECK_EQ(playas_gender(5), GENDER_N);
    RES_SET_PRONOUN(&doomo_town.res[5], PRONOUN_XE);
    CHECK_EQ(playas_gender(5), GENDER_N);
    doomo_town.res[6].status = RES_EMPTY;
    CHECK_EQ(playas_gender(6), GENDER_N);
    CHECK_EQ(playas_gender(-1), GENDER_N);
    CHECK_EQ(playas_gender(5000), GENDER_N);
    /* "you" is the avatar: the hero on a fresh town */
    doomo_town.player.status = RES_ALIVE;
    RES_SET_PRONOUN(&doomo_town.player, PRONOUN_HE);
    CHECK_EQ(playas_you_gender(), GENDER_M);
    RES_SET_PRONOUN(&doomo_town.player, PRONOUN_SHE);
    CHECK_EQ(playas_you_gender(), GENDER_F);
}

TEST(pa0_romance_gate_is_the_one_chain)
{
    /* away from the hero and the avatar, the play-as gate is LS0's gate */
    int a, b, n = 0;
    town_new(TOWN_STORY, 3, TOWN_POP_PREMADE);
    for (a = 0; a < 60; a++)
        for (b = 0; b < 60; b += 7) {
            CHECK_EQ(playas_romance_ok(a, b), sim_romance_ok(a, b));
            n += sim_romance_ok(a, b);
        }
    CHECK(n > 0);                             /* the sample has some yeses */
}

TEST(pa0_header_byte_47)
{
    DoomoSaveInfo info;
    uint32_t n;
    town_new(TOWN_STORY, 11, TOWN_POP_PREMADE);
    memcpy(doomo_town.player.name, "Abcdefghijk", 12);      /* 11 chars, the most */
    n = save_serialize(image, sizeof(image));
    CHECK(n > 48);
    /* the writer puts the play-as summary there; the reader hands it back */
    CHECK_EQ(image[47], playas_profile_flags());
    CHECK(save_info_from(image, n, &info));
    CHECK_EQ(info.flags, image[47]);
    CHECK_STR(info.player, "Abcdefghijk");
    CHECK_EQ(image[47] & PLAYAS_HF_RESERVED, 0);
    CHECK_EQ(PLAYAS_HF_HERO(image[47]), playas_hero_kind());
    /* the name stops before byte 47 */
    CHECK_EQ(image[36 + 10], 'k');

    /* an old save: byte 47 is 0; it reads and loads as before */
    image[47] = 0;
    put_crc(image, n);
    CHECK(save_info_from(image, n, &info));
    CHECK_EQ(info.flags, 0);
    CHECK_STR(info.player, "Abcdefghijk");
    CHECK(save_deserialize(image, n));
    CHECK_STR(doomo_town.player.name, "Abcdefghijk");

    /* every bit set: still an 11-char name, and the load does not trust it */
    image[47] = 0xFF;
    put_crc(image, n);
    CHECK(save_info_from(image, n, &info));
    CHECK_EQ(info.flags, 0xFF);
    CHECK_STR(info.player, "Abcdefghijk");
    CHECK_EQ(PLAYAS_HF_HERO(info.flags), HERO_COOP_LAST);
    CHECK(save_deserialize(image, n));
    CHECK_STR(doomo_town.player.name, "Abcdefghijk");
    CHECK_EQ(playas_avatar(), PLAYAS_ID_HERO);    /* no PLAY chunk: the hero */
    CHECK(!playas_story_locked());
}

TEST(pa0_header_byte_47_through_the_slot)
{
    DoomoSaveInfo info;
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_RANDOM);
    memcpy(doomo_town.player.name, "Mochi", 6);
    CHECK(save_game(1));
    CHECK(save_peek(1, &info));
    CHECK_EQ(info.flags, playas_profile_flags());
    CHECK_STR(info.player, "Mochi");
    CHECK(save_verify(1));
    CHECK(load_game(1));
    CHECK_EQ(playas_avatar(), PLAYAS_ID_HERO);
    plat_save_erase(1);
}
