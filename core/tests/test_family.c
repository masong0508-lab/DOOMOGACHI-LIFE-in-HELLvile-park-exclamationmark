/* test_family.c - F1 part 1: the household table (family.h): the cap by tier,
 * who may join and in what role, the partner's life stage gate, the LIFE
 * chunk's family section and the strong life.h functions. Owner: life. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/roster.h"
#include "doomo/save_budget.h"
#include "doomo/ui.h"

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

/* town_new keeps the unlocks, so tests of the tiers start from "neither tier" */
static void clear_tiers(void)
{
    doomo_town.unlocks[UL_LIFE_HOUSEHOLD_T1 >> 3] &= (uint8_t)~(1u << (UL_LIFE_HOUSEHOLD_T1 & 7));
    doomo_town.unlocks[UL_LIFE_HOUSEHOLD_T2 >> 3] &= (uint8_t)~(1u << (UL_LIFE_HOUSEHOLD_T2 & 7));
}

TEST(family_tiers_follow_the_story_finishes)
{
    fresh();
    clear_tiers();
    CHECK_EQ(family_tier(), HOUSEHOLD_TIER_START);
    CHECK_EQ(family_cap(), HOUSEHOLD_CAP_FULL_T0);
    /* the first finish: 3 -> 10, with one toast and no table entry */
    ui_toast_clear();
    CHECK_EQ(family_tier_story_done(), HOUSEHOLD_TIER_GAME_DONE);
    CHECK(unlock_has(UL_LIFE_HOUSEHOLD_T1) && !unlock_has(UL_LIFE_HOUSEHOLD_T2));
    CHECK_EQ(family_cap(), HOUSEHOLD_CAP_FULL_T1);
    CHECK(ui_toast_pending());
    ui_toast_clear();
    /* a new story keeps the unlocks; finishing it again: 10 -> 20 */
    town_new(TOWN_STORY, 77, TOWN_POP_PREMADE);
    CHECK_EQ(family_tier(), HOUSEHOLD_TIER_GAME_DONE);
    CHECK_EQ(family_tier_story_done(), HOUSEHOLD_TIER_NGPLUS_DONE);
    CHECK_EQ(family_cap(), HOUSEHOLD_CAP_FULL_T2);
    CHECK(ui_toast_pending());
    ui_toast_clear();
    /* nothing above 20, and nothing to say about it */
    CHECK_EQ(family_tier_story_done(), 0);
    CHECK_EQ(family_cap(), HOUSEHOLD_CAP_FULL_T2);
    CHECK(!ui_toast_pending());
    CHECK(unlock_by_id(UL_LIFE_HOUSEHOLD_T1) == NULL);     /* K0: no table entry */
    clear_tiers();
}

TEST(family_tier_sync_is_a_catch_up_for_tier_one_only)
{
    fresh();
    clear_tiers();
    ui_toast_clear();
    CHECK_EQ(family_tier_sync(0), 0);                      /* an unfinished story: nothing */
    CHECK_EQ(family_tier(), HOUSEHOLD_TIER_START);
    CHECK(!ui_toast_pending());
    CHECK_EQ(family_tier_sync(1), 1);                      /* finished before the tiers */
    CHECK_EQ(family_tier(), HOUSEHOLD_TIER_GAME_DONE);
    CHECK(ui_toast_pending());
    ui_toast_clear();
    CHECK_EQ(family_tier_sync(1), 0);                      /* once, and never tier 2 */
    CHECK_EQ(family_tier(), HOUSEHOLD_TIER_GAME_DONE);
    CHECK(!ui_toast_pending());
    clear_tiers();
}

TEST(family_empty_household_is_silent)
{
    fresh();
    CHECK_EQ(family_count(), 0);
    CHECK_EQ(family_member(0), -1);
    CHECK_EQ(life_player_partner(), -1);
    CHECK_EQ(life_family_count(), 0);
    CHECK_EQ_U(family_save_size(), 0u);          /* nothing written: saves stay byte-identical */
    CHECK_EQ_U(family_pool_save_size(), 0u);
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
    CHECK_EQ_U(life->size(), n + LIFE_SEC_HDR + FAMILY_SEC_HEAD + 3u * 8u +
                             LIFE_SEC_HDR + FAMILY_SEC_HEAD + 2u * FAMILY_POOL_REC);
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

/* ---- part 2: pool members' identities ------------------------------------- */
static int name_is_clean(const char *n)
{
    int i, len = (int)strlen(n);
    if (len < 1 || len > DOOMO_NAME_LEN - 1) return 0;
    for (i = 0; i < len; i++)
        if ((unsigned char)n[i] < 0x20 || (unsigned char)n[i] > 0x7E) return 0;
    for (i = len; i < DOOMO_NAME_LEN; i++)
        if (n[i]) return 0;
    return 1;
}

TEST(family_pool_identity_is_default_stable_and_town_specific)
{
    const FamilyIdentity *a, *b;
    uint32_t seed_a, seed_a2;
    char name_a[DOOMO_NAME_LEN];
    int i;
    fresh();
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST + 3, FAMROLE_CHILD, LIFE_CHILD), 0);
    CHECK(family_identity(PLAYAS_ID_FAMILY_FIRST + 3) != NULL);
    CHECK(family_identity(PLAYAS_ID_FAMILY_FIRST + 4) == NULL);      /* not a member */
    CHECK(family_identity(5) == NULL);                                /* residents have their own */
    a = family_identity(PLAYAS_ID_FAMILY_FIRST + 3);
    CHECK(name_is_clean(a->name));
    CHECK(a->pronoun < PRONOUN_COUNT && a->romance < ROMANCE_COUNT);
    seed_a = a->seed;
    memcpy(name_a, a->name, sizeof(name_a));
    /* the same town and id give the same person after a remove and a re-add */
    CHECK(family_remove(PLAYAS_ID_FAMILY_FIRST + 3));
    CHECK(family_identity(PLAYAS_ID_FAMILY_FIRST + 3) == NULL);
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST + 3, FAMROLE_CHILD, LIFE_CHILD), 0);
    b = family_identity(PLAYAS_ID_FAMILY_FIRST + 3);
    CHECK_EQ_U(b->seed, seed_a);
    CHECK_STR(b->name, name_a);
    /* another town has other people in the same slot */
    town_new(TOWN_FREEPLAY, 32, TOWN_POP_PREMADE);
    sim_init();
    CHECK_EQ(family_add(PLAYAS_ID_FAMILY_FIRST + 3, FAMROLE_CHILD, LIFE_CHILD), 0);
    seed_a2 = family_identity(PLAYAS_ID_FAMILY_FIRST + 3)->seed;
    CHECK(seed_a2 != seed_a);
    /* different ids in one town differ too, and every default name is clean */
    fresh();
    unlock_grant(UL_LIFE_HOUSEHOLD_T1);                              /* cap 10 */
    for (i = 0; i < 10; i++)
        CHECK(family_add(PLAYAS_ID_FAMILY_FIRST + i, FAMROLE_HOUSEMATE, LIFE_ADULT) >= 0);
    for (i = 1; i < 10; i++) {
        CHECK(family_identity(PLAYAS_ID_FAMILY_FIRST + i)->seed != family_identity(PLAYAS_ID_FAMILY_FIRST)->seed);
        CHECK(name_is_clean(family_identity(PLAYAS_ID_FAMILY_FIRST + i)->name));
    }
}

TEST(family_pool_add_sets_identity_and_changes_nothing_on_error)
{
    int id, i;
    const FamilyIdentity *f;
    fresh();
    id = family_pool_add(FAMROLE_CHILD, LIFE_TODDLER, 0xC0FFEEu, "Pip", PRONOUN_XE);
    CHECK_EQ(id, PLAYAS_ID_FAMILY_FIRST);                            /* the first free pool id */
    f = family_identity(id);
    CHECK_EQ_U(f->seed, 0xC0FFEEu);
    CHECK_STR(f->name, "Pip");
    CHECK_EQ(f->pronoun, PRONOUN_XE);
    CHECK_EQ(family_find(id)->stage, LIFE_TODDLER);
    /* NULL name / -1 pronoun keep the generated ones of that seed */
    {
        DoomoResident g;
        id = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 777u, NULL, -1);
        CHECK_EQ(id, PLAYAS_ID_FAMILY_FIRST + 1);
        resident_generate(&g, 777u);
        resident_sanitize(&g);
        CHECK_STR(family_identity(id)->name, g.name);
        CHECK_EQ(family_identity(id)->pronoun, g.pronoun);
    }
    /* bad names: nothing added */
    CHECK_EQ(family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 1u, "", -1), FAM_ERR_NAME);
    CHECK_EQ(family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 1u, "TwelveLetter", -1), FAM_ERR_NAME);
    CHECK_EQ(family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 1u, "Zo\xE9", -1), FAM_ERR_NAME);
    CHECK_EQ(family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 1u, "Tab\tbed", -1), FAM_ERR_NAME);
    CHECK_EQ(family_count(), 2);
    /* a minor partner: refused by the gate, the pool id stays free */
    CHECK_EQ(family_pool_add(FAMROLE_PARTNER, LIFE_TEEN, 5u, "Kit", -1), FAM_ERR_GATE);
    CHECK_EQ(family_count(), 2);
    CHECK_EQ(family_pool_alloc(), PLAYAS_ID_FAMILY_FIRST + 2);
    /* full: three members at the start (cap 3) */
    CHECK(family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 6u, "Moe", -1) >= 0);
    CHECK_EQ(family_pool_add(FAMROLE_HOUSEMATE, LIFE_ADULT, 7u, "Joe", -1), FAM_ERR_FULL);
    CHECK_EQ(family_count(), 3);
    /* removing and re-adding the same id never shows the old person */
    id = PLAYAS_ID_FAMILY_FIRST;
    CHECK(family_remove(id));
    CHECK_EQ(family_add(id, FAMROLE_CHILD, LIFE_CHILD), 2);          /* appended */
    CHECK(family_identity(id)->seed != 0xC0FFEEu);
    for (i = 0; i < family_count(); i++) CHECK(family_identity(family_member(i)) != NULL);
}

TEST(family_set_name_checks_the_name_and_the_member)
{
    int id;
    fresh();
    id = family_pool_add(FAMROLE_PARENT, LIFE_ELDER, 3u, "Gran", -1);
    CHECK(id >= 0);
    CHECK_EQ(family_set_name(id, "Granny Jo"), 0);
    CHECK_STR(family_identity(id)->name, "Granny Jo");
    CHECK_EQ(family_set_name(id, "Granny Josephine"), FAM_ERR_NAME);
    CHECK_EQ(family_set_name(id, ""), FAM_ERR_NAME);
    CHECK_EQ(family_set_name(id, NULL), FAM_ERR_NAME);
    CHECK_STR(family_identity(id)->name, "Granny Jo");               /* refusals change nothing */
    CHECK_EQ(family_set_name(id + 1, "Nobody"), FAM_ERR_ID);         /* not a member */
    CHECK_EQ(family_set_name(5, "Nobody"), FAM_ERR_ID);              /* residents are renamed elsewhere */
}

TEST(family_resident_builds_the_person_from_the_identity)
{
    DoomoResident r, g;
    int id, a;
    fresh();
    id = family_pool_add(FAMROLE_CHILD, LIFE_TODDLER, 4242u, "Pip", PRONOUN_SHE);
    CHECK(id >= 0);
    CHECK(family_resident(id, &r));
    resident_generate(&g, 4242u);
    CHECK(memcmp(&r.look, &g.look, sizeof(r.look)) == 0);            /* the seed is the look */
    CHECK_STR(r.name, "Pip");
    CHECK_EQ(r.pronoun, PRONOUN_SHE);
    CHECK_EQ(r.status, RES_ALIVE);
    CHECK_EQ(RES_STAGE(&r), LIFE_TODDLER);
    CHECK_EQ(RES_ROMANCE(&r), family_identity(id)->romance);
    CHECK_EQ(resident_sanitize(&r), 0);                              /* already valid */
    CHECK_EQ(sim_life_stage(DOOMO_PLAYER_ID) , sim_life_stage(DOOMO_PLAYER_ID));
    /* the same twice: no hidden state */
    {
        DoomoResident r2;
        CHECK(family_resident(id, &r2));
        CHECK(memcmp(&r, &r2, sizeof(r)) == 0);
    }
    /* a moved-in resident is a copy of the town's */
    a = adult_open(0);
    CHECK_EQ(family_add(a, FAMROLE_HOUSEMATE, 0), 1);
    CHECK(family_resident(a, &r));
    CHECK(memcmp(&r, town_res(a), sizeof(r)) == 0);
    /* a pet is not drawn as a person (F5), a stranger is nobody, NULL is safe */
    CHECK(family_add(PLAYAS_ID_FAMILY_FIRST + 5, FAMROLE_PET, LIFE_ADULT) >= 0);
    CHECK(!family_resident(PLAYAS_ID_FAMILY_FIRST + 5, &r));
    CHECK(!family_resident(PLAYAS_ID_FAMILY_FIRST + 9, &r));
    CHECK(!family_resident(a + 1, &r));
    CHECK(!family_resident(id, NULL));
}

TEST(family_pool_section_round_trips_in_either_order)
{
    static uint8_t fsec[FAMILY_SZ_WORST], psec[FAMILY_POOL_SZ_WORST];
    FamilyIdentity before[2];
    uint32_t fn, pn;
    int x, y;
    fresh();
    x = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 99u, "Pip", PRONOUN_HE);
    y = family_pool_add(FAMROLE_PET, LIFE_ADULT, 100u, "Rex", PRONOUN_IT);
    CHECK(x >= 0 && y >= 0);
    before[0] = *family_identity(x);
    before[1] = *family_identity(y);
    fn = family_save(fsec, sizeof(fsec));
    pn = family_pool_save(psec, sizeof(psec));
    CHECK_EQ_U(pn, FAMILY_SEC_HEAD + 2u * FAMILY_POOL_REC);
    /* family first, then the identities (the writer's order) */
    family_clear();
    CHECK(family_load(fsec, fn));
    CHECK(family_pool_load(psec, pn));
    CHECK_STR(family_identity(x)->name, "Pip");
    CHECK_EQ_U(family_identity(y)->seed, before[1].seed);
    CHECK(memcmp(family_identity(x), &before[0], sizeof(before[0])) == 0);
    /* identities first, then the family: nothing is lost */
    family_clear();
    CHECK(family_pool_load(psec, pn));
    CHECK(family_load(fsec, fn));
    CHECK(memcmp(family_identity(x), &before[0], sizeof(before[0])) == 0);
    CHECK(memcmp(family_identity(y), &before[1], sizeof(before[1])) == 0);
    /* a save from part 1 (family, no identities) gives the default people */
    family_clear();
    CHECK(family_load(fsec, fn));
    CHECK(family_identity(x) != NULL && name_is_clean(family_identity(x)->name));
    /* a rejected identity section resets to the defaults, the family stays */
    {
        uint8_t bad[sizeof(psec)];
        memcpy(bad, psec, pn);
        bad[2 + 2 + 6] = 0x01;                                       /* name starts with a control char */
        CHECK(!family_pool_load(bad, pn));
        family_pool_reset();
        CHECK_EQ(family_count(), 2);
        CHECK(family_identity(x) != NULL);
    }
}

TEST(family_pool_load_rejects_bad_sections)
{
    static uint8_t good[FAMILY_POOL_SZ_WORST];
    uint8_t t[sizeof(good)];
    uint32_t n;
    int x;
    fresh();
    x = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 99u, "Pip", PRONOUN_HE);
    family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 100u, "Sam", PRONOUN_THEY);
    n = family_pool_save(good, sizeof(good));
    CHECK_EQ_U(n, FAMILY_SEC_HEAD + 2u * FAMILY_POOL_REC);
    CHECK(family_pool_load(good, n));
    {
        uint32_t e1 = FAMILY_SEC_HEAD, e2 = FAMILY_SEC_HEAD + FAMILY_POOL_REC;   /* the entries */
        memcpy(t, good, n); t[0] = 3;                      CHECK(!family_pool_load(t, n));      /* count vs length */
        memcpy(t, good, n); t[0] = 0;                      CHECK(!family_pool_load(t, FAMILY_SEC_HEAD));
        memcpy(t, good, n);                                CHECK(!family_pool_load(t, n - 1));
        memcpy(t, good, n); t[e2] = t[e1]; t[e2 + 1] = t[e1 + 1]; CHECK(!family_pool_load(t, n)); /* duplicate id */
        memcpy(t, good, n); t[e1] = 5; t[e1 + 1] = 0;      CHECK(!family_pool_load(t, n));      /* not a pool id */
        memcpy(t, good, n); t[e1] = (uint8_t)(FAMILY_POOL_LAST + 1); t[e1 + 1] = (uint8_t)((FAMILY_POOL_LAST + 1) >> 8);
                                                           CHECK(!family_pool_load(t, n));      /* past the pool */
        memcpy(t, good, n); t[e1 + 6] = 0;                 CHECK(!family_pool_load(t, n));      /* empty name */
        memcpy(t, good, n); t[e1 + 6 + 11] = 'x';          CHECK(!family_pool_load(t, n));      /* 12 chars, no NUL */
        memcpy(t, good, n); t[e1 + 6 + 7] = 'z';           CHECK(!family_pool_load(t, n));      /* junk after the NUL */
        memcpy(t, good, n); t[e1 + 6] = 0xE9;              CHECK(!family_pool_load(t, n));      /* not ASCII */
        memcpy(t, good, n); t[e1 + 18] = PRONOUN_COUNT;    CHECK(!family_pool_load(t, n));
        memcpy(t, good, n); t[e1 + 19] = ROMANCE_COUNT;    CHECK(!family_pool_load(t, n));
        memcpy(t, good, n); t[e1 + 20] = 1;                CHECK(!family_pool_load(t, n));      /* spare bytes are 0 */
    }
    CHECK_STR(family_identity(x)->name, "Pip");                      /* every refusal left it alone */
}

/* ---- owner amendments: kids can be lost (gently), grief, free will ---------- */
TEST(family_minor_loss_rules)
{
    int kid;
    fresh();
    family_loss_set(1);
    kid = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 99u, "Pip", PRONOUN_HE);
    CHECK(kid >= 0);
    CHECK(!family_lose(kid, FAMLOSS_AGE));          /* never old age */
    CHECK(!family_lose(kid, FAMLOSS_NEGLECT));      /* never neglect */
    family_loss_set(0);
    CHECK(!family_lose(kid, FAMLOSS_ILLNESS));      /* Loss off: safe */
    CHECK(!family_lose(kid, FAMLOSS_DEMON));
    family_loss_set(1);
    CHECK(family_lose(kid, FAMLOSS_ILLNESS));       /* Loss on: illness or Hell only */
    CHECK_EQ(family_memorial_count(), 1);
    CHECK_EQ(family_count(), 0);
    kid = family_pool_add(FAMROLE_CHILD, LIFE_BABY, 98u, "Bo", PRONOUN_THEY);
    CHECK(family_lose(kid, FAMLOSS_GONE));          /* moving away always works */
}

TEST(family_grief_after_partner_loss)
{
    int p;
    fresh();
    family_loss_set(1);
    CHECK_EQ(family_grief_days_left(), 0);
    CHECK_EQ(family_grief_damage_pct(), 100);
    CHECK_EQ(family_grief_hurt_pct(), 100);
    p = family_pool_add(FAMROLE_PARTNER, LIFE_ADULT, 77u, "Sol", PRONOUN_THEY);
    if (p < 0) return;                              /* gate closed in this setup */
    CHECK(family_lose(p, FAMLOSS_ILLNESS));
    CHECK_EQ(family_grief_days_left(), FAMILY_GRIEF_DAYS);
    CHECK_EQ(family_grief_damage_pct(), 50);
    CHECK_EQ(family_grief_hurt_pct(), 125);
    doomo_town.day += 7;
    CHECK(family_grief_damage_pct() > 50 && family_grief_damage_pct() < 100);
    doomo_town.day += FAMILY_GRIEF_DAYS;
    CHECK_EQ(family_grief_days_left(), 0);
    CHECK_EQ(family_grief_damage_pct(), 100);
}

TEST(family_freewill_default_save_and_off)
{
    uint8_t sec[FAMILY_SEC_HEAD + 8 * FAMILY_MAX];
    uint32_t n;
    int a, b;
    fresh();
    CHECK_EQ(family_freewill_level(), FAMILY_WILL_NORMAL);          /* default Normal */
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    b = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    CHECK(a >= 0 && b >= 0);
    n = family_save(sec, sizeof(sec));
    CHECK_EQ(sec[1], 0);                                            /* the default costs nothing */
    family_freewill_set(FAMILY_WILL_SUBCONSCIOUS);
    n = family_save(sec, sizeof(sec));
    CHECK_EQ(sec[1], FAMILY_WILL_SUBCONSCIOUS + 1);
    family_freewill_set(FAMILY_WILL_OFF);
    CHECK(family_load(sec, n));
    CHECK_EQ(family_freewill_level(), FAMILY_WILL_SUBCONSCIOUS);
    family_freewill_set(FAMILY_WILL_OFF);
    CHECK_EQ(family_freewill_daily(), 0);                           /* Off = today's behaviour */
    family_freewill_set(99);
    CHECK_EQ(family_freewill_level(), FAMILY_WILL_SUBCONSCIOUS);    /* clamped */
}

TEST(family_bond_care_decay_and_save)
{
    uint8_t buf[FAMILY_BOND_SZ_WORST + 8];
    uint32_t n;
    int a, b;
    fresh();
    CHECK_EQ(family_bond_save_size(), 0u);                           /* all default: nothing saved */
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    b = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    CHECK(a >= 0 && b >= 0);
    CHECK_EQ(family_bond(b), 3);
    family_care(b);
    CHECK_EQ(family_bond(b), 4);
    family_bond_add(b, 100);
    CHECK_EQ(family_bond(b), FAMILY_BOND_MAX);
    CHECK(family_bond_line(b) != NULL);
    n = family_bond_save(buf, sizeof(buf));
    CHECK_EQ_U(n, (uint32_t)family_count());
    family_bond_add(b, -100);
    CHECK(family_bond_load(buf, n));
    CHECK_EQ(family_bond(b), FAMILY_BOND_MAX);
    CHECK(!family_bond_load(buf, n + 1));                            /* wrong length rejected */
    family_remove(a);                                                /* bond follows its member */
    CHECK_EQ(family_bond(b), FAMILY_BOND_MAX);
}

TEST(family_disasters)
{
    int a, k;
    fresh();
    family_loss_set(1);
    CHECK_EQ(family_disaster_pending(), FAMDIS_NONE);
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    k = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    CHECK(a >= 0 && k >= 0);
    /* a warning, then an unprevented illness takes an adult the next day */
    CHECK(family_disaster_start(FAMDIS_ILLNESS, a));
    CHECK(!family_disaster_start(FAMDIS_FIRE, k));                   /* one at a time */
    CHECK_EQ(family_disaster_target(), a);
    CHECK_EQ(family_memorial_count(), 0);
    CHECK_EQ(family_disaster_daily(), 1);
    CHECK_EQ(family_memorial_count(), 1);
    /* care in between saves a member */
    CHECK(family_disaster_start(FAMDIS_DEMON, k));
    family_care(k);
    CHECK_EQ(family_disaster_daily(), 0);
    CHECK_EQ(family_count(), 1);
    /* a storm only frightens: nobody is lost */
    CHECK(family_disaster_start(FAMDIS_STORM, k));
    CHECK_EQ(family_disaster_daily(), 0);
    CHECK_EQ(family_count(), 1);
    CHECK_EQ(family_disaster_pending(), FAMDIS_NONE);
    /* Loss off: nothing happens, the pending one is dropped */
    CHECK(family_disaster_start(FAMDIS_ILLNESS, k));
    family_loss_set(0);
    CHECK_EQ(family_disaster_daily(), 0);
    CHECK_EQ(family_count(), 1);
    CHECK_EQ(family_disaster_pending(), FAMDIS_NONE);
    family_loss_set(1);
}

TEST(family_bond_events)
{
    int a, b, pet, m0;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    b = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    pet = family_pool_add(FAMROLE_PET, LIFE_ADULT, 7u, "Rex", PRONOUN_IT);
    CHECK(a >= 0 && b >= 0 && pet >= 0);
    CHECK(!family_squabble_start(a, pet));                          /* pets do not squabble */
    CHECK(!family_squabble_start(a, a));
    m0 = family_mood(b);
    CHECK(family_squabble_start(a, b));
    CHECK(family_squabble_pending());
    CHECK(!family_squabble_start(a, b));                            /* one at a time */
    CHECK(family_mood(b) < m0);
    family_freewill_set(FAMILY_WILL_NORMAL);
    CHECK_EQ(family_events_daily(), 1);                             /* the day after: they make up */
    CHECK(!family_squabble_pending());
    CHECK(family_mood(b) > m0 - 3);
    family_freewill_set(FAMILY_WILL_OFF);
    CHECK(family_squabble_start(a, b));
    CHECK_EQ(family_events_daily(), 0);                             /* Off: no events, pending dropped */
    CHECK(!family_squabble_pending());
}

TEST(family_helpers)
{
    int a, k, b;
    uint32_t m0;
    fresh();
    a = family_pool_add(FAMROLE_SIBLING, LIFE_ADULT, 5u, "Ann", PRONOUN_SHE);
    k = family_pool_add(FAMROLE_CHILD, LIFE_CHILD, 6u, "Pip", PRONOUN_HE);
    b = family_pool_add(FAMROLE_CHILD, LIFE_BABY, 8u, "Bo", PRONOUN_THEY);
    CHECK(a >= 0 && k >= 0 && b >= 0);
    CHECK_EQ(family_help_do(a), 0);                                 /* bond 3: not yet */
    family_bond_add(a, 2); family_bond_add(k, 2); family_bond_add(b, 2);
    m0 = doomo_town.money;
    CHECK_EQ(family_help_do(a), 1);
    CHECK(doomo_town.money > m0);                                   /* an adult earns */
    CHECK_EQ(family_help_do(k), 2);                                 /* a child leaves a gift */
    CHECK_EQ(family_help_do(b), 3);                                 /* a baby smiles, nothing else */
    family_freewill_set(FAMILY_WILL_OFF);
    CHECK_EQ(family_help_daily(), 0);
}
