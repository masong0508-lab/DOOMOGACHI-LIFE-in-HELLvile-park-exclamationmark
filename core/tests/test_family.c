/* test_family.c - F1 part 1: the household table (family.h): the cap by tier,
 * who may join and in what role, the partner's life stage gate, the LIFE
 * chunk's family section and the strong life.h functions. Owner: life. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/roster.h"
#include "doomo/save_budget.h"

static void fresh(void)
{
    town_new(TOWN_FREEPLAY, 31, TOWN_POP_PREMADE);   /* resets the LIFE chunk: no household */
    sim_init();
}

/* an alive resident of this stage who is open to romance with anyone, so the
 * gate's answer depends on stages alone; resident ids from `from` up */
static int adult_open(int from)
{
    int id;
    for (id = from; id < DOOMO_MAX_RESIDENTS; id++) {
        DoomoResident *r = town_res(id);
        if (r && r->status == RES_ALIVE && roster_stage(id) == LIFE_ADULT) {
            resident_set_stage(r, LIFE_ADULT);
            resident_set_romance(r, ROMANCE_ANY);
            return id;
        }
    }
    return -1;
}

TEST(family_empty_household_is_silent)
{
    fresh();
    CHECK_EQ(family_count(), 0);
    CHECK_EQ(family_member(0), -1);
    CHECK_EQ(life_player_partner(), -1);
    CHECK_EQ(life_family_count(), 0);
    CHECK_EQ_U(family_save_size(), 0u);          /* nothing written: saves stay byte-identical */
    CHECK_EQ(family_role(5), FAMROLE_NONE);
    CHECK(family_find(5) == NULL);
}

TEST(family_cap_follows_the_tier_unlocks)
{
    int i, id;
    fresh();
    CHECK_EQ(family_cap(), HOUSEHOLD_CAP_FULL_T0);
    for (i = 0; i < 3; i++)
        CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST + i, FAMROLE_HOUSEMATE, LIFE_ADULT), i);
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST + 3, FAMROLE_HOUSEMATE, LIFE_ADULT), FAM_ERR_FULL);
    unlock_grant(UL_LIFE_HOUSEHOLD_T1);
    CHECK_EQ(family_cap(), HOUSEHOLD_CAP_FULL_T1);
    for (; i < HOUSEHOLD_CAP_FULL_T1; i++)
        CHECK(family_add(PLAYAS_ID_FAMILY_FIRST + i, FAMROLE_HOUSEMATE, LIFE_ADULT) >= 0);
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST + i, FAMROLE_HOUSEMATE, LIFE_ADULT), FAM_ERR_FULL);
    unlock_grant(UL_LIFE_HOUSEHOLD_T2);
    CHECK_EQ(family_cap(), HOUSEHOLD_CAP_FULL_T2);
    for (; i < HOUSEHOLD_CAP_FULL_T2; i++)
        CHECK(family_add(PLAYAS_ID_FAMILY_FIRST + i, FAMROLE_HOUSEMATE, LIFE_ADULT) >= 0);
    CHECK_EQ(family_count(), 20);
    CHECK_EQ(family_pool_alloc(), -1);           /* all 20 pool ids are used */
    CHECK_EQ(life_family_count(), 21);           /* the player counts here */
    id = family_member(19);
    CHECK_EQ(id, PLAYAS_ID_FAMILY_LAST);
    /* removing frees a slot and the pool id comes back */
    CHECK(family_remove(PLAYAS_ID_FAMILY_FIRST + 4));
    CHECK(!family_remove(PLAYAS_ID_FAMILY_FIRST + 4));
    CHECK_EQ(family_count(), 19);
    CHECK_EQ(family_pool_alloc(), PLAYAS_ID_FAMILY_FIRST + 4);
}

TEST(family_ids_duplicates_and_roles)
{
    int a;
    fresh();
    CHECK_EQ(family_add(-1, FAMROLE_CHILD, LIFE_CHILD), FAM_ERR_ID);
    CHECK_EQ(family_add(DOOMO_PLAYER_ID, FAMROLE_CHILD, LIFE_CHILD), FAM_ERR_ID);   /* the hero is not a member */
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_LAST + 1, FAMROLE_CHILD, LIFE_CHILD), FAM_ERR_ID);
    a = adult_open(0);
    CHECK(a >= 0);
    CHECK_EQ(family_add(a, FAMROLE_NONE, 0), FAM_ERR_ROLE);
    CHECK_EQ(family_add(a, FAMROLE_COUNT, 0), FAM_ERR_ROLE);
    CHECK_EQ(family_add(a, FAMROLE_PET, 0), FAM_ERR_ROLE);          /* pets are pool members */
    CHECK_EQ(family_add(a, FAMROLE_SIBLING, 0), 0);
    CHECK_EQ(family_add(a, FAMROLE_PARENT, 0), FAM_ERR_DUP);
    CHECK_EQ(family_role(a), FAMROLE_SIBLING);
    CHECK_EQ(family_set_role(a, FAMROLE_PARENT), 0);                 /* roles are the player's choice */
    CHECK_EQ(family_role(a), FAMROLE_PARENT);
    CHECK_EQ(family_set_role(a, FAMROLE_PET), FAM_ERR_ROLE);
    CHECK_EQ(family_set_role(999, FAMROLE_CHILD), FAM_ERR_ID);
    /* a resident's stage comes from the sim, not the argument */
    CHECK_EQ(family_find(a)->stage, sim_life_stage(a));
    /* a pet in the pool; it counts toward the cap */
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST, FAMROLE_PET, LIFE_ADULT), 1);
    CHECK_EQ(family_count_role(FAMROLE_PET), 1);
    /* a dead resident cannot join */
    {
        int b = adult_open(a + 1);
        CHECK(b >= 0);
        town_res(b)->status = RES_DEAD;
        CHECK_EQ(family_add(b, FAMROLE_HOUSEMATE, 0), FAM_ERR_ID);
    }
}

TEST(family_partner_passes_the_life_stage_gate)
{
    int a, b, kid;
    fresh();
    a = adult_open(0);
    b = adult_open(a + 1);
    kid = adult_open(b + 1);
    CHECK(a >= 0 && b >= 0 && kid >= 0);
    resident_set_stage(town_res(kid), LIFE_TEEN);
    CHECK_EQ(family_add(kid, FAMROLE_PARTNER, 0), FAM_ERR_GATE);     /* a teen is never a partner */
    resident_set_stage(town_res(kid), LIFE_CHILD);
    CHECK_EQ(family_add(kid, FAMROLE_PARTNER, 0), FAM_ERR_GATE);
    /* a minor in the pool, and an adult in the pool, by stage alone */
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST, FAMROLE_PARTNER, LIFE_TEEN), FAM_ERR_GATE);
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST, FAMROLE_PARTNER, LIFE_BABY), FAM_ERR_GATE);
    CHECK_EQ(family_count(), 0);
    /* a resident who is not open to the player is not a partner either */
    resident_set_romance(town_res(a), ROMANCE_NONE);
    CHECK_EQ(family_add(a, FAMROLE_PARTNER, 0), FAM_ERR_GATE);
    CHECK_EQ(family_add(a, FAMROLE_HOUSEMATE, 0), 0);                 /* but may live there */
    CHECK_EQ(family_set_role(a, FAMROLE_PARTNER), FAM_ERR_GATE);
    resident_set_romance(town_res(a), ROMANCE_ANY);
    CHECK_EQ(family_set_role(a, FAMROLE_PARTNER), 0);
    CHECK_EQ(life_player_partner(), a);
    /* only one partner */
    CHECK_EQ(family_add(b, FAMROLE_PARTNER, 0), FAM_ERR_ROLE);
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST, FAMROLE_PARTNER, LIFE_ADULT), FAM_ERR_ROLE);
    CHECK_EQ(family_set_role(a, FAMROLE_PARTNER), 0);                 /* itself again is fine */
    /* an elder counts as an adult */
    CHECK(family_remove(a));
    CHECK_EQ(life_player_partner(), -1);
    CHECK(family_add(PLAYAS_ID_FAMILY_FIRST, FAMROLE_PARTNER, LIFE_ELDER) >= 0);
    CHECK_EQ(life_player_partner(), PLAYAS_ID_FAMILY_FIRST);
}

TEST(family_section_round_trips_through_the_life_chunk)
{
    static uint8_t buf[SAVE_EXT_MAX_LIFE];
    const DoomoSaveExt *life = doomo_save_ext_life;
    uint32_t n;
    int a;
    fresh();
    n = life->size();                                  /* the news alone */
    a = adult_open(0);
    CHECK_EQ(family_add(a, FAMROLE_SIBLING, 0), 0);
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST + 2, FAMROLE_CHILD, LIFE_TODDLER), 1);
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST + 7, FAMROLE_PET, LIFE_ADULT), 2);
    CHECK_EQ_U(life->size(), n + LIFE_SEC_HDR + FAMILY_SEC_HEAD + 3u * 8u);
    CHECK(life->size() <= life->max_size);
    CHECK_EQ_U(life->write(buf, sizeof(buf)), life->size());
    n = life->size();
    life->reset();                                     /* a new town */
    CHECK_EQ(family_count(), 0);
    CHECK(life->read(buf, n, LIFE_CHUNK_VERSION));
    CHECK_EQ(family_count(), 3);
    CHECK_EQ(family_member(0), a);
    CHECK_EQ(family_role(a), FAMROLE_SIBLING);
    CHECK_EQ(family_role(PLAYAS_ID_FAMILY_FIRST + 2), FAMROLE_CHILD);
    CHECK_EQ(family_find(PLAYAS_ID_FAMILY_FIRST + 2)->stage, LIFE_TODDLER);
    CHECK_EQ(family_role(PLAYAS_ID_FAMILY_FIRST + 7), FAMROLE_PET);
    /* a save from before F1 (no family section) loads as no household */
    life->reset();
    CHECK_EQ(family_count(), 0);
    CHECK(life->read(buf, 0, LIFE_CHUNK_VERSION));
    CHECK_EQ(family_count(), 0);
}

TEST(family_load_rejects_bad_sections)
{
    uint8_t s[FAMILY_SZ_WORST + 8];
    uint32_t n;
    int a;
    fresh();
    a = adult_open(0);
    family_add(a, FAMROLE_SIBLING, 0);
    family_add(PLAYAS_ID_FAMILY_FIRST, FAMROLE_CHILD, LIFE_CHILD);
    n = family_save(s, sizeof(s));
    CHECK_EQ_U(n, FAMILY_SEC_HEAD + 16u);
    CHECK(family_load(s, n));                          /* the good one */
    CHECK_EQ(family_count(), 2);
    {   /* each corruption is refused and leaves the household as it was */
        uint8_t t[sizeof(s)];
        memcpy(t, s, n); t[0] = 3;                     CHECK(!family_load(t, n));           /* count vs length */
        memcpy(t, s, n); t[0] = 0;                     CHECK(!family_load(t, FAMILY_SEC_HEAD));
        memcpy(t, s, n); t[0] = FAMILY_MAX + 1;        CHECK(!family_load(t, n));
        memcpy(t, s, n);                               CHECK(!family_load(t, n - 1));       /* cut short */
        memcpy(t, s, n); t[2 + 8] = t[2]; t[2 + 9] = t[3]; CHECK(!family_load(t, n));      /* a duplicate id */
        memcpy(t, s, n); t[2 + 2] = FAMROLE_COUNT;     CHECK(!family_load(t, n));           /* bad role */
        memcpy(t, s, n); t[2 + 3] = LIFE_COUNT;        CHECK(!family_load(t, n));           /* bad stage */
        memcpy(t, s, n); t[2] = 0xFF; t[3] = 0xFF;     CHECK(!family_load(t, n));           /* bad id */
        memcpy(t, s, n); t[2 + 2] = FAMROLE_PET;       CHECK(!family_load(t, n));           /* a resident pet */
        memcpy(t, s, n); t[2 + 2] = t[2 + 10] = FAMROLE_PARTNER; CHECK(!family_load(t, n)); /* two partners */
    }
    CHECK_EQ(family_count(), 2);                       /* untouched by every refusal */
    CHECK_EQ(family_member(0), a);
}
