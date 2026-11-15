/* test_res_live.c - GBA-500: the 500 residents of the GBA (res_live.h, town.h's slots, the
 * old-save rule in save.c). Every test runs in both builds: `make -C core test` (all 1000
 * live: the slot is the id) and `make -C core unit LIVE=500 BUILD=build-500` (the GBA's 500).
 * res_live_lists_the_500 writes the list to TMP/gba500_live.txt. */
#include <stdlib.h>
#include "test.h"
#include "doomo/res_live.h"
#include "doomo/family.h"
#include "doomo/story_vm.h"
#include "doomo/exorcism.h"

int home_visited(int id);              /* home_int.h */

static int count_bits(const uint8_t *set, int from, int to)
{
    int i, n = 0;
    for (i = from; i < to; i++) n += (set[i >> 3] >> (i & 7)) & 1;
    return n;
}

TEST(res_live_default_is_250_per_district_and_holds_the_must_set)
{
    int i;
    CHECK_EQ(count_bits(doomo_live_default, 0, DOOMO_DISTRICT_SIZE), 250);
    CHECK_EQ(count_bits(doomo_live_default, DOOMO_DISTRICT_SIZE, DOOMO_MAX_RESIDENTS), 250);
    CHECK_EQ(count_bits(doomo_live_must, 0, DOOMO_MAX_RESIDENTS), doomo_live_must_count);
    CHECK(doomo_live_must_count > 0 && doomo_live_must_count <= 500);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (res_live_must(i)) CHECK_MSG(res_live_default(i), "must citizen %d is not in the default 500", i);
    CHECK(!res_live_must(-1) && !res_live_must(DOOMO_MAX_RESIDENTS));
    CHECK(!res_live_default(-1) && !res_live_default(DOOMO_MAX_RESIDENTS));
}

/* the rule (tools/gba_live.py) against the compiled tables it must cover */
TEST(res_live_must_set_covers_the_cast_the_kids_and_the_givers)
{
    int i;
    for (i = 0; i < NPC_COUNT; i++) CHECK_MSG(res_live_must(doomo_story_npc[i]), "story npc %d", i);
    CHECK(res_live_must(ROSTER_ID_GRUMBLEGUTS));
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        const DoomoResident *p = roster_premade(i);
        if (roster_traits(i) & (ROSTER_T_KID | ROSTER_T_TEEN))
            CHECK_MSG(res_live_must(i), "kid / teen %d %s", i, roster_name(i));
        if (p && (p->flags & RF_STORY_NPC)) CHECK_MSG(res_live_must(i), "story citizen %d %s", i, roster_name(i));
    }
    for (i = 0; i < STORY_SIDE2_ROWS; i++)
        if (doomo_side_rows[i].kind && doomo_side_rows[i].giver < DOOMO_MAX_RESIDENTS)
            CHECK_MSG(res_live_must(doomo_side_rows[i].giver), "the giver of S%d", 100 + i);
    for (i = 0; i < doomo_possess_count; i++) {
        const DoomoPossessor *p = &doomo_possess_table[i];
        if (p->host < DOOMO_MAX_RESIDENTS) CHECK_MSG(res_live_must(p->host), "possessor %d's host", i);
        if (p->name_res < DOOMO_MAX_RESIDENTS) CHECK_MSG(res_live_must(p->name_res), "possessor %d's name", i);
    }
    /* the shop keepers fp_shop.c looks up by name */
    CHECK(res_live_must(roster_find("Swipey")) && res_live_must(roster_find("Cardamom")) &&
          res_live_must(roster_find("Bargain Bun")));
}

TEST(res_live_lists_the_500)
{
    char path[512];
    FILE *f;
    int i, n = 0;
    snprintf(path, sizeof path, "%s/gba500_live.txt", test_tmp_dir);
    f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(f, "# the GBA's 500 residents (tools/gba_live.py): id name [must]\n");
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (res_live_default(i)) {
            fprintf(f, "%4d %-11s%s\n", i, roster_name(i), res_live_must(i) ? " must" : "");
            n++;
        }
    fclose(f);
    CHECK_EQ(n, 500);
}

TEST(town_slots_a_classic_island_moves_in_the_live_set)
{
    int i, n = 0;
    town_new(TOWN_FREEPLAY, 1, TOWN_POP_PREMADE);
    CHECK_EQ(town_count(-1), DOOMO_LIVE_RESIDENTS);
    CHECK_EQ(town_count_district(0, -1), DOOMO_LIVE_PER_DISTRICT);
    CHECK_EQ(town_count_district(1, -1), DOOMO_LIVE_PER_DISTRICT);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        int in = TOWN_RES(i)->status != RES_EMPTY;
        CHECK_MSG(in == (DOOMO_RES_SLOTS ? res_live_default(i) : 1), "id %d", i);
        if (in) {
            n++;
            CHECK(strncmp(TOWN_RES(i)->name, roster_name(i), DOOMO_NAME_LEN) == 0);
            CHECK(town_res(i) == TOWN_RES(i));
            CHECK(town_slot(i) >= 0 && town_slot(i) < DOOMO_LIVE_RESIDENTS && town_slot_id(town_slot(i)) == i);
        } else {
            CHECK(town_slot(i) < 0);
        }
    }
    CHECK_EQ(n, DOOMO_LIVE_RESIDENTS);
    for (i = 0; i < NPC_COUNT; i++) CHECK(town_alive(doomo_story_npc[i]));
    /* every edge joins two residents who live here (or the player) */
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        CHECK(e->a < DOOMO_MAX_RESIDENTS && TOWN_RES(e->a)->status != RES_EMPTY);
        CHECK(e->b == DOOMO_PLAYER_ID || TOWN_RES(e->b)->status != RES_EMPTY);
    }
    CHECK(doomo_town.edge_count > 0);
    /* a Random Island takes the same ids */
    town_new(TOWN_FREEPLAY, 99, TOWN_POP_RANDOM);
    CHECK_EQ(town_count(-1), DOOMO_LIVE_RESIDENTS);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        CHECK((TOWN_RES(i)->status != RES_EMPTY) == (DOOMO_RES_SLOTS ? res_live_default(i) : 1));
    CHECK(doomo_town.edge_count > 0);
}

TEST(town_slots_alloc_fills_a_district_and_free_gives_the_slot_back)
{
    int i, id, first;
    town_new(TOWN_FREEPLAY, 3, TOWN_POP_EMPTY);
    CHECK_EQ(town_count(-1), 0);
    CHECK_EQ(town_district_free(1), DOOMO_LIVE_PER_DISTRICT);
    first = town_alloc(1);
    CHECK_EQ(first, DOOMO_DISTRICT_SIZE);              /* the lowest free id */
    for (i = 0; i < DOOMO_LIVE_PER_DISTRICT; i++) {
        DoomoResident *r;
        id = town_alloc(1);
        CHECK_EQ(id, DOOMO_DISTRICT_SIZE + i);
        r = town_res(id);
        resident_generate_hellborn(r, 50u + (uint32_t)i);
        CHECK(r->status == RES_ALIVE);
    }
    CHECK_EQ(town_alloc(1), -1);                        /* full */
    CHECK_EQ(town_district_free(1), 0);
    CHECK_EQ(town_count_district(1, -1), DOOMO_LIVE_PER_DISTRICT);
    CHECK_EQ(town_count_district(0, -1), 0);
    CHECK(town_alloc(0) == 0);                          /* the other district has room */
    /* freeing one gives its slot (and its id) back */
    id = DOOMO_DISTRICT_SIZE + 7;
    rel_adjust(id, DOOMO_DISTRICT_SIZE + 8, 30);
    CHECK(rel_find(id, DOOMO_DISTRICT_SIZE + 8) != NULL);
    town_free(id);
    CHECK(TOWN_RES(id)->status == RES_EMPTY);
    CHECK(rel_find(id, DOOMO_DISTRICT_SIZE + 8) == NULL);
    CHECK_EQ(town_district_free(1), 1);
    CHECK_EQ(town_alloc(1), id);
    resident_generate_hellborn(town_res(id), 7);
    CHECK_EQ(town_alloc(1), -1);
    if (DOOMO_RES_SLOTS) {
        /* the ids past the slots are not in town: they read as empty, take no edges */
        id = DOOMO_DISTRICT_SIZE + DOOMO_LIVE_PER_DISTRICT + 3;
        CHECK(town_slot(id) < 0);
        CHECK(TOWN_RES(id)->status == RES_EMPTY && !town_alive(id));
        CHECK(rel_get_or_add(id, DOOMO_PLAYER_ID) == NULL);
        CHECK(town_bind(id) < 0);                       /* no free slot in Hellvile */
        town_free(DOOMO_DISTRICT_SIZE + 1);
        CHECK(town_bind(id) >= 0 && town_slot(id) >= 0);   /* the freed slot */
        CHECK(town_slot(DOOMO_DISTRICT_SIZE + 1) < 0);
        town_unbind(id);
        CHECK(town_slot(id) < 0);
    }
}

/* ---- an old save with all 1000 (fixtures/gba500_old_save.c made it with the build before
 * GBA-500) ----------------------------------------------------------------------------- */
static uint8_t *read_fixture(uint32_t *size)
{
    char path[512];
    FILE *f;
    uint8_t *buf;
    long n;
    snprintf(path, sizeof path, "%s/../fixtures/gba500_old_1000.sav", test_golden_dir);
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (uint8_t *)malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    *size = (uint32_t)n;
    return buf;
}

/* the ids the fixture made close to the player (all outside the default 500) */
static const int16_t close_ids[] = { 304, 810, 305, 306, 307, 317, 807, 808, 308, 309, 809, 310, 312,
                                     313, 314, 316 };

TEST(save_old_1000_island_keeps_the_500_closest)
{
    uint32_t size = 0;
    uint8_t *buf = read_fixture(&size);
    int i, found = 0;
    CHECK(buf != NULL);
    for (i = 0; i < DOOMO_COUNTOF(close_ids); i++) CHECK_MSG(!res_live_default(close_ids[i]), "%d", close_ids[i]);
    CHECK(save_deserialize(buf, size));
    free(buf);
    CHECK_EQ(town_count_district(0, -1), DOOMO_LIVE_PER_DISTRICT);
    CHECK_EQ(town_count_district(1, -1), DOOMO_LIVE_PER_DISTRICT);
    CHECK_EQ(save_moved_away(), DOOMO_MAX_RESIDENTS - DOOMO_LIVE_RESIDENTS);
    /* never a story character, the kid cast or anyone else the game names */
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        if (res_live_must(i)) CHECK_MSG(TOWN_RES(i)->status != RES_EMPTY, "must citizen %d moved away", i);
    /* the household, the partner, friends, a rival, friendship levels, a made-over citizen,
     * a favourite, visited houses and a close friend's grave stay */
    for (i = 0; i < DOOMO_COUNTOF(close_ids); i++)
        CHECK_MSG(TOWN_RES(close_ids[i])->status != RES_EMPTY, "close citizen %d moved away", close_ids[i]);
    CHECK(family_find(304) != NULL && family_find(810) != NULL);
    CHECK_EQ(family_role(810), FAMROLE_PARTNER);
    CHECK_EQ(rel_kind(810, DOOMO_PLAYER_ID), REL_MARRIED);
    CHECK_EQ(rel_kind(305, DOOMO_PLAYER_ID), REL_BEST_FRIEND);
    CHECK(TOWN_RES(310)->flags & RF_CUSTOM);
    CHECK(strcmp(TOWN_RES(310)->name, "Zed Custom") == 0);
    CHECK(TOWN_RES(312)->flags & RF_FAVORITE);
    CHECK(home_visited(313) && home_visited(314));
    CHECK(TOWN_RES(316)->status == RES_DEAD);
    for (i = 0; i < doomo_town.grave_count; i++) {
        CHECK(TOWN_RES(doomo_town.graves[i].resident)->status != RES_EMPTY);
        if (doomo_town.graves[i].resident == 316) found |= 1;
        if (doomo_town.graves[i].resident == 960) found |= 2;
    }
    CHECK_EQ(found, DOOMO_RES_SLOTS ? 1 : 3);             /* the stranger's grave goes with them */
    CHECK((TOWN_RES(960)->status == RES_EMPTY) == DOOMO_RES_SLOTS);
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        CHECK(TOWN_RES(e->a)->status != RES_EMPTY);
        CHECK(e->b == DOOMO_PLAYER_ID || TOWN_RES(e->b)->status != RES_EMPTY);
    }
    if (DOOMO_RES_SLOTS) {
        /* the news says it when the sim starts (the newest line) */
        sim_init();
        CHECK(sim_news_count() > 0);
        CHECK(strstr(sim_news(0), "moved away") != NULL);
        /* who left: the ones nobody was close to, the highest ids of the default set first */
        CHECK(TOWN_RES(0)->status != RES_EMPTY);
    }
    /* the island saves and loads again as it is now, nobody else leaves */
    {
        static uint8_t img[DOOMO_SAVE_SLOT1_MAX];
        uint32_t n = save_serialize(img, sizeof img);
        int before = town_count(-1);
        CHECK(n > 0);
        CHECK(save_deserialize(img, n));
        CHECK_EQ(save_moved_away(), 0);
        CHECK_EQ(town_count(-1), before);
        for (i = 0; i < DOOMO_COUNTOF(close_ids); i++) CHECK(TOWN_RES(close_ids[i])->status != RES_EMPTY);
    }
}

TEST(save_a_live_island_round_trips_by_id)
{
    static uint8_t img[DOOMO_SAVE_SLOT1_MAX];
    uint32_t n;
    int i, id;
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_PREMADE);
    /* a birth-like change of the slot order: free a resident, a new citizen takes the id */
    id = DOOMO_RES_SLOTS ? 0 : 3;
    town_free(3);
    CHECK_EQ(town_alloc(0), 3);
    resident_generate(town_res(3), 1234);
    memcpy(town_res(3)->name, "Newbie", 7);
    town_res(id)->level = 42;
    n = save_serialize(img, sizeof img);
    CHECK(n > 0);
    town_new(TOWN_FREEPLAY, 6, TOWN_POP_EMPTY);
    CHECK(save_deserialize(img, n));
    CHECK_EQ(save_moved_away(), 0);
    CHECK_EQ(town_count(-1), DOOMO_LIVE_RESIDENTS);
    CHECK(strcmp(town_res(3)->name, "Newbie") == 0);
    CHECK_EQ(town_res(id)->level, 42);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
        CHECK((TOWN_RES(i)->status != RES_EMPTY) == (DOOMO_RES_SLOTS ? res_live_default(i) : 1));
}
