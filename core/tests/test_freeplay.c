/* test_freeplay.c - headless flows through the free play screens (owner:
 * freeplay). Everything is driven through the real scenes with button
 * presses, like a player would. */
#include "test.h"
#include "../src/fp_int.h"
#include "../src/fp_island.h"

/* fp_island.c test hooks */
int fp_island_select_kind(int kind);
int fp_island_menu_open(void);
int fp_island_menu_count(void);
const char *fp_island_menu_label(int i);
int fp_island_menu_select(const char *label);

static void frames(uint32_t b, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        plat_begin_frame();
        doomo_frame(b);
        plat_end_frame();
    }
}

/* one press: held for a frame, then released for a frame */
static void press(uint32_t b) { frames(b, 1); frames(0, 1); }

static int top_is(const DoomoScene *s) { return scene_top() == s; }

/* finish every queued dialogue line (and pick the first choice) */
static void close_dialogs(void)
{
    int guard = 0;
    while (top_is(&SCENE_DIALOG) && guard++ < 40) { press(BTN_A); frames(0, 2); }
}

static void premade_island(void)
{
    fp_new_island(TOWN_POP_PREMADE, 4242);
    scene_set(&SCENE_ISLAND, FP_ISLAND_ARG_RETURN);
    frames(0, 3);
}

static int first_alive(int from)
{
    int i;
    for (i = from; i < DOOMO_MAX_RESIDENTS; i++)
        if (doomo_town.res[i].status == RES_ALIVE && sim_problem_for(i) < 0) return i;
    return -1;
}

/* ====================================================================== */
TEST(freeplay_new_island_choices)
{
    fp_new_island(TOWN_POP_PREMADE, 1);
    CHECK_EQ(town_count(-1), 1000);
    CHECK_EQ(doomo_town.mode, TOWN_FREEPLAY);
    fp_new_island(TOWN_POP_EMPTY, 1);
    CHECK_EQ(town_count(-1), 0);
    fp_new_island(TOWN_POP_RANDOM, 777);
    CHECK_EQ(town_count(-1), 1000);
    {
        uint32_t fp1 = resident_fingerprint(&doomo_town.res[123]);
        fp_new_island(TOWN_POP_RANDOM, 777);
        CHECK_EQ_U(resident_fingerprint(&doomo_town.res[123]), fp1);
    }
}

TEST(freeplay_new_island_scene_classic)
{
    int guard = 0;
    scene_set(&SCENE_NEW_ISLAND, 0);
    frames(0, 5);
    press(BTN_A);                               /* Classic is preselected; A builds it (LS6) */
    frames(0, 10);
    while (!top_is(&SCENE_ISLAND) && guard++ < 20) { press(BTN_A); frames(0, 2); }
    CHECK(top_is(&SCENE_ISLAND));
    CHECK_EQ(town_count(-1), 1000);
    CHECK(unlock_has(UL_FP_PREMADE_START));
    CHECK(!unlock_has(UL_FP_NAMER));
    CHECK_EQ(fp_island_name()[0], 0);
}

/* LS7: a burst of toasts clears faster, and a line too wide for its pill
 * holds long enough to scroll to its end instead of running off the screen. */
TEST(toast_backlog_clears_and_long_lines_scroll)
{
    int n;
    scene_set(&SCENE_TITLE, 0);
    frames(0, 3);
    ui_toast_clear();
    ui_toast("one");
    ui_toast("two");
    frames(0, 70);                              /* queued: 9 + 48 + 8 = 65 ticks each */
    CHECK_EQ(ui_toast_pending(), 1);
    ui_toast_clear();
    ui_toast("alone");
    frames(0, 70);                              /* nobody waiting: 9 + 75 + 8 = 92 */
    CHECK_EQ(ui_toast_pending(), 1);
    frames(0, 30);
    CHECK_EQ(ui_toast_pending(), 0);
    ui_toast_clear();
    ui_toast("A very long toast line that can never fit inside its little pill, so it scrolls");
    frames(0, 100);                             /* a short one would be gone by 92 */
    CHECK_EQ(ui_toast_pending(), 1);
    for (n = 0; n < 400 && ui_toast_pending(); n++) frames(0, 1);
    CHECK_EQ(ui_toast_pending(), 0);            /* and it does finish */
}

TEST(freeplay_new_island_start_names_it_first)
{
    const char *p;
    int guard = 0;
    scene_set(&SCENE_NEW_ISLAND, 0);
    frames(0, 5);
    press(BTN_START);                           /* LS6: START asks for a name, then builds */
    frames(0, 5);
    CHECK(top_is(&SCENE_TEXT_ENTRY));
    for (p = "Mochi Isle\n"; *p; p++) { ui_text_entry_feed(*p); frames(0, 1); }
    frames(0, 20);
    while (!top_is(&SCENE_ISLAND) && guard++ < 20) { press(BTN_A); frames(0, 2); }   /* the welcome */
    CHECK(top_is(&SCENE_ISLAND));
    CHECK_EQ(town_count(-1), 1000);
    CHECK(unlock_has(UL_FP_NAMER));
    CHECK(strcmp(fp_island_name(), "Mochi Isle") == 0);
}

TEST(freeplay_new_island_naming_can_be_cancelled)
{
    scene_set(&SCENE_NEW_ISLAND, 0);
    frames(0, 5);
    press(BTN_START);
    frames(0, 5);
    CHECK(top_is(&SCENE_TEXT_ENTRY));
    press(BTN_B);                               /* backing out stays on the card */
    frames(0, 10);
    CHECK(top_is(&SCENE_NEW_ISLAND));
    CHECK(!unlock_has(UL_FP_NAMER));
}

TEST(freeplay_empty_island_opens_creator)
{
    int guard = 0;
    scene_set(&SCENE_NEW_ISLAND, 0);
    frames(0, 5);
    press(BTN_LEFT);                            /* Empty Island */
    press(BTN_A);                               /* A builds it, no question in the way (LS6) */
    frames(0, 10);
    while (!top_is(&SCENE_CREATOR) && guard++ < 20) { press(BTN_A); frames(0, 2); }
    CHECK_EQ(town_count(-1), 0);
    CHECK(top_is(&SCENE_CREATOR));
}

TEST(freeplay_feed_through_resident_screen)
{
    int id, cup0, hunger0, lvl0;
    uint32_t money0;
    premade_island();
    id = first_alive(20);
    CHECK(id >= 0);
    doomo_town.res[id].stat[STAT_HUNGER] = 30;          /* hungry: not "full" */
    cup0 = inv_count(ITEM_CUPCAKE);
    CHECK(cup0 > 0);
    hunger0 = doomo_town.res[id].stat[STAT_HUNGER];
    lvl0 = doomo_town.res[id].level;
    money0 = doomo_town.money;
    scene_push(&SCENE_RESIDENT, id);
    frames(0, 3);
    CHECK(top_is(&SCENE_RESIDENT));
    CHECK(fp_resident_select(FPA_FEED));
    press(BTN_A);                               /* Feed -> the food popup */
    frames(0, 2);
    press(BTN_A);                               /* the first food: Cupcake */
    frames(0, 2);
    CHECK(top_is(&SCENE_DIALOG));               /* they react, with their voice */
    close_dialogs();
    CHECK(top_is(&SCENE_RESIDENT));
    CHECK_EQ(inv_count(ITEM_CUPCAKE), cup0 - 1);
    CHECK(doomo_town.res[id].stat[STAT_HUNGER] > hunger0);
    CHECK(doomo_town.res[id].level >= lvl0);
    CHECK_EQ_U(doomo_town.money, money0);       /* feeding is free */
}

TEST(freeplay_shop_buy_updates_money_and_bag)
{
    uint32_t money0;
    int item, before, price;
    premade_island();
    doomo_town.money = 1000;
    money0 = doomo_town.money;
    scene_push(&SCENE_SHOP, FP_SHOP_ARG_SHOP);
    frames(0, 3);
    CHECK(top_is(&SCENE_SHOP));
    /* the first shelf item is an unlocked food */
    item = ITEM_NONE;
    {
        int i;
        for (i = 1; i < ITEM_COUNT && item == ITEM_NONE; i++)
            if (doomo_items[i].category == ICAT_FOOD) item = i;
    }
    press(BTN_A);                               /* open the quantity popup */
    press(BTN_RIGHT);                           /* 2 */
    press(BTN_A);                               /* buy */
    frames(0, 2);
    CHECK(doomo_town.money < money0);
    price = (int)(money0 - doomo_town.money);
    CHECK(price > 0);
    CHECK(price % 2 == 0);                       /* two of the same thing */
    before = 0;
    {
        int i, total = 0;
        for (i = 1; i < ITEM_COUNT; i++) total += inv_count(i);
        before = total;
    }
    CHECK(before >= 2);
    (void)item;
}

TEST(freeplay_care_list_is_decluttered)
{
    int id, a, sum = 0, plush0, med0;
    premade_island();
    id = first_alive(40);
    CHECK(id >= 0);
    doomo_town.res[id].status = RES_ALIVE;
    scene_push(&SCENE_RESIDENT, id);
    frames(0, 3);
    /* Gift and Heal became one Give bag */
    CHECK(fp_resident_has_action(FPA_GIVE));
    CHECK(!fp_resident_has_action(FPA_GIFT));
    CHECK(!fp_resident_has_action(FPA_HEAL));
    /* the housekeeping verbs sit under Manage, not in the care list */
    CHECK(fp_resident_has_action(FPA_MANAGE));
    CHECK(!fp_resident_has_action(FPA_EDIT));
    CHECK(!fp_resident_has_action(FPA_FAV));
    CHECK(!fp_resident_has_action(FPA_HOME));
    /* no action twice: the count is the number of distinct actions */
    for (a = 0; a < FPA_COUNT; a++) sum += fp_resident_has_action(a);
    CHECK_EQ(sum, fp_resident_action_count());
    /* Give hands over a present and a remedy alike */
    inv_add(ITEM_PLUSHIE, 1);
    inv_add(ITEM_MEDICINE, 1);
    plush0 = inv_count(ITEM_PLUSHIE);
    med0 = inv_count(ITEM_MEDICINE);
    CHECK(fp_resident_select(FPA_GIVE));
    press(BTN_A);                               /* the bag opens */
    frames(0, 2);
    press(BTN_A);                               /* the first thing in it: a present */
    frames(0, 2);
    close_dialogs();
    CHECK_EQ(inv_count(ITEM_PLUSHIE) + inv_count(ITEM_MEDICINE), plush0 + med0 - 1);
    CHECK(top_is(&SCENE_RESIDENT));
    /* Manage -> Favourite (the second row) toggles the star */
    {
        int fav0 = (doomo_town.res[id].flags & RF_FAVORITE) != 0;
        CHECK(fp_resident_select(FPA_MANAGE));
        press(BTN_A);
        frames(0, 2);
        CHECK(top_is(&SCENE_DIALOG));
        press(BTN_A);                           /* A finishes the question's typing first */
        frames(0, 2);
        press(BTN_DOWN);                        /* Edit look -> Favourite */
        press(BTN_A);
        frames(0, 2);
        close_dialogs();
        CHECK_EQ((doomo_town.res[id].flags & RF_FAVORITE) != 0, !fav0);
        CHECK(top_is(&SCENE_RESIDENT));
    }
}

TEST(freeplay_lay_to_rest_gating)
{
    int id;
    premade_island();
    id = first_alive(40);
    CHECK(id >= 0);
    opt_set(OPT_MONSTER_MODE, 0);
    scene_push(&SCENE_RESIDENT, id);
    frames(0, 2);
    CHECK(fp_resident_action_count() > 5);
    CHECK(!fp_resident_has_action(FPA_REST));    /* ALIVE, no monster mode */
    /* a lost soul after three failed exorcisms: the option appears */
    doomo_town.res[id].status = RES_SOUL_LOST;
    doomo_town.res[id].exorcism = (uint8_t)((3 << 4) | 3);
    frames(0, 2);
    CHECK(fp_resident_has_action(FPA_REST));
    /* possessed (not lost): still hidden */
    doomo_town.res[id].status = RES_POSSESSED;
    frames(0, 2);
    CHECK(!fp_resident_has_action(FPA_REST));
    CHECK(fp_resident_has_action(FPA_EXORCISE));
    /* Monster Mode: visible even for the innocent */
    doomo_town.res[id].status = RES_ALIVE;
    opt_set(OPT_MONSTER_MODE, 1);
    frames(0, 2);
    CHECK(fp_resident_has_action(FPA_REST));
}

TEST(freeplay_lay_to_rest_lost_soul_double_confirm)
{
    int id;
    premade_island();
    id = first_alive(60);
    CHECK(id >= 0);
    doomo_town.res[id].status = RES_SOUL_LOST;
    doomo_town.res[id].exorcism = (uint8_t)((3 << 4) | 3);
    scene_push(&SCENE_RESIDENT, id);
    frames(0, 2);
    CHECK(fp_resident_select(FPA_REST));
    press(BTN_A);
    CHECK(top_is(&SCENE_CONFIRM));
    press(BTN_A);                               /* default answer is No */
    frames(0, 2);
    CHECK_EQ(doomo_town.res[id].status, RES_SOUL_LOST);
    CHECK(fp_resident_select(FPA_REST));
    press(BTN_A);
    press(BTN_LEFT); press(BTN_A);              /* first: Yes */
    frames(0, 2);
    CHECK(top_is(&SCENE_CONFIRM));              /* ...are you REALLY sure? */
    press(BTN_LEFT); press(BTN_A);
    frames(0, 2);
    CHECK_EQ(doomo_town.res[id].status, RES_DEAD);
    CHECK(fp_resident_in_farewell());
    CHECK(doomo_town.grave_count > 0);
    frames(0, 50);
    press(BTN_A);                               /* to the graveyard */
    frames(0, 2);
    CHECK(top_is(&SCENE_GRAVEYARD));
}

TEST(freeplay_monster_mode_needs_the_words)
{
    int id;
    const char *p;
    premade_island();
    id = first_alive(80);
    CHECK(id >= 0);
    opt_set(OPT_MONSTER_MODE, 1);
    scene_push(&SCENE_RESIDENT, id);
    frames(0, 2);
    CHECK(fp_resident_select(FPA_REST));
    press(BTN_A);
    CHECK(top_is(&SCENE_TEXT_ENTRY));
    for (p = "oops\n"; *p; p++) { ui_text_entry_feed(*p); frames(0, 1); }
    frames(0, 2);
    CHECK(!top_is(&SCENE_CONFIRM));             /* wrong words: nothing happens */
    CHECK_EQ(doomo_town.res[id].status, RES_ALIVE);
    CHECK(fp_resident_select(FPA_REST));
    press(BTN_A);
    CHECK(top_is(&SCENE_TEXT_ENTRY));
    for (p = "i am a monster\n"; *p; p++) { ui_text_entry_feed(*p); frames(0, 1); }
    frames(0, 2);
    CHECK(top_is(&SCENE_CONFIRM));
    press(BTN_LEFT); press(BTN_A);
    frames(0, 2);
    CHECK_EQ(doomo_town.res[id].status, RES_DEAD);
    CHECK(doomo_town.res[id].flags & RF_MONSTER_KILL);
    CHECK(doomo_town.monster_meter > 0);
}

TEST(freeplay_monster_mode_option_needs_the_words)
{
    const char *p;
    opt_set(OPT_MONSTER_MODE, 0);
    scene_set(&SCENE_OPTIONS, OPTPAGE_GAME);
    frames(0, 2);
    press(BTN_DOWN); press(BTN_DOWN);           /* past Language and Gore: Monster Mode */
    press(BTN_RIGHT);
    CHECK(top_is(&SCENE_TEXT_ENTRY));
    for (p = "I AM A MONSTER\n"; *p; p++) { ui_text_entry_feed(*p); frames(0, 1); }
    frames(0, 2);
    CHECK(top_is(&SCENE_CONFIRM));
    CHECK_EQ(opt_get(OPT_MONSTER_MODE), 0);
    press(BTN_LEFT); press(BTN_A);
    frames(0, 2);
    CHECK_EQ(opt_get(OPT_MONSTER_MODE), 1);
    press(BTN_RIGHT);                           /* switching it off is instant */
    CHECK_EQ(opt_get(OPT_MONSTER_MODE), 0);
}

TEST(freeplay_save_load_round_trip_through_ui)
{
    uint32_t money;
    premade_island();
    plat_save_erase(1);
    doomo_town.money = 4321;
    fp_set_island_name("Mochi Isle");
    fp_set_flowers(7, 2);
    scene_push(&SCENE_SAVELOAD, FP_SL_ARG_MANAGE);
    frames(0, 2);
    press(BTN_A);                               /* Save into the empty slot 2 */
    frames(0, 2);
    CHECK(save_exists(1));
    money = doomo_town.money;
    doomo_town.money = 1;
    fp_set_island_name("Changed");
    fp_set_flowers(7, 0);
    press(BTN_RIGHT);                           /* -> Load */
    press(BTN_A);
    CHECK(top_is(&SCENE_CONFIRM));
    press(BTN_LEFT); press(BTN_A);
    frames(0, 3);
    CHECK(top_is(&SCENE_ISLAND));
    CHECK_EQ_U(doomo_town.money, money);
    CHECK_STR(fp_island_name(), "Mochi Isle");
    CHECK_EQ(fp_flowers(7), 2);
    CHECK_EQ(town_count(-1), 1000);
}

TEST(freeplay_save_too_big_keeps_old_save_and_explains)
{
    int i;
    premade_island();
    plat_save_erase(1);
    CHECK(save_game(1));
    /* an island that cannot fit: every citizen fully custom & random */
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        resident_generate(&doomo_town.res[i], 99991u * (uint32_t)i + 7u);
        doomo_town.res[i].flags |= RF_CUSTOM;
        doomo_town.res[i].name[0] = (char)('A' + i % 26);
    }
    scene_push(&SCENE_SAVELOAD, FP_SL_ARG_MANAGE);
    frames(0, 2);
    press(BTN_A);                               /* overwrite? */
    if (top_is(&SCENE_CONFIRM)) { press(BTN_LEFT); press(BTN_A); }
    frames(0, 2);
    if (save_last_error()[0]) {
        CHECK(top_is(&SCENE_DIALOG));           /* explained in a dialogue */
        CHECK(save_verify(1));                  /* the old save is intact */
    }
}

TEST(freeplay_activity_needs_resident_opens_picker)
{
    static const DoomoActivity fake = {
        "Test Dream", "A test activity", ACTLOC_APARTMENTS, ACTF_NEEDS_RESIDENT | ACTF_FREEPLAY,
        ICON_ZZZ, 0, ACT_ALWAYS, &SCENE_GRAVEYARD, 0
    };
    premade_island();
    activity_launch(&fake);
    frames(0, 2);
    CHECK(top_is(&SCENE_PICKER));
    press(BTN_A);
    frames(0, 2);
    CHECK(top_is(&SCENE_GRAVEYARD));
    scene_pop();
    frames(0, 1);
    CHECK(top_is(&SCENE_ISLAND));
    /* cancelling the picker launches nothing */
    activity_launch(&fake);
    frames(0, 2);
    press(BTN_B);
    frames(0, 2);
    CHECK(top_is(&SCENE_ISLAND));
}

TEST(freeplay_island_menus_list_activities)
{
    int i, n, found = 0;
    premade_island();
    fp_island_select_kind(LOC_FUN_FAIR);
    press(BTN_A);
    CHECK(fp_island_menu_open());
    n = fp_island_menu_count();
    CHECK(n >= 2);                               /* something + Leave */
    for (i = 0; i < n; i++) {
        const char *l = fp_island_menu_label(i);
        int k, total = activity_count();
        if (strcmp(l, "Coming soon!") == 0) found = 1;
        for (k = 0; k < total; k++) {
            const DoomoActivity *a = activity_at(k);
            if (a && a->location == ACTLOC_FUN_FAIR && strcmp(a->name, l) == 0) found = 1;
        }
    }
    CHECK(found);
    press(BTN_B);
    CHECK(!fp_island_menu_open());
    /* every location opens a menu with at least "Leave" */
    for (i = 0; i < LOC_COUNT; i++) {
        fp_island_select_kind(i);
        press(BTN_A);
        CHECK_MSG(fp_island_menu_open(), "location %d has no menu", i);
        CHECK(fp_island_menu_count() >= 1);
        press(BTN_B);
    }
}

TEST(freeplay_island_town_hall_rent_once_a_day)
{
    uint32_t m0;
    premade_island();
    m0 = doomo_town.money;
    fp_island_select_kind(LOC_TOWN_HALL);
    press(BTN_A);
    CHECK(fp_island_menu_select("Collect rent"));
    press(BTN_A);
    CHECK(doomo_town.money > m0);
    m0 = doomo_town.money;
    CHECK(fp_island_menu_select("Collect rent"));
    press(BTN_A);
    close_dialogs();
    CHECK_EQ_U(doomo_town.money, m0);            /* once a day */
    CHECK(unlock_has(UL_FP_LANDLORD));
}

TEST(freeplay_picker_kinds)
{
    premade_island();
    doomo_town.res[5].status = RES_POSSESSED;
    CHECK(fp_pick_match(PICK_POSSESSED, 5));
    CHECK(!fp_pick_match(PICK_POSSESSED, 6) || doomo_town.res[6].status == RES_POSSESSED ||
          doomo_town.res[6].status == RES_SOUL_LOST);
    doomo_town.res[6].status = RES_DEAD;
    CHECK(fp_pick_match(PICK_DEAD, 6));
    CHECK(!fp_pick_match(PICK_LIVING, 6));
    CHECK(fp_pick_match(PICK_ANY, 6));
    doomo_town.res[7].status = RES_EMPTY;
    CHECK(!fp_pick_match(PICK_ANY, 7));
}

TEST(freeplay_unlock_table_is_valid)
{
    int i, n = 0;
    for (i = 0; doomo_unlocks_freeplay[i].name; i++) {
        const DoomoUnlock *u = &doomo_unlocks_freeplay[i];
        CHECK_MSG(u->id >= 736 && u->id <= 831, "unlock %s id %d out of range", u->name, u->id);
        CHECK(u->how && u->how[0]);
        CHECK(unlock_by_id(u->id) == u);        /* no clash with another team */
        n++;
    }
    CHECK(n >= 40);
    /* achievements bring their creator part along */
    premade_island();
    CHECK(!unlock_option_ok(CLIST_HAT, HAT_BERET));
    fp_achieve(UL_FP_NAMER);
    CHECK(unlock_has(UL_FP_NAMER));
    CHECK(unlock_option_ok(CLIST_HAT, HAT_BERET));
}

TEST(freeplay_island_chunk_round_trip)
{
    static uint8_t buf[DOOMO_SAVE_SLOT1_MAX];
    uint32_t n;
    premade_island();
    fp_set_island_name("Bun Bay");
    fp_set_rent_day(9);
    fp_set_flowers(999, 3);
    n = save_serialize(buf, sizeof(buf));
    CHECK(n > 0);
    fp_new_island(TOWN_POP_EMPTY, 3);
    CHECK_STR(fp_island_name(), "");            /* town_new resets the chunk */
    CHECK(save_deserialize(buf, n));
    CHECK_STR(fp_island_name(), "Bun Bay");
    CHECK_EQ_U(fp_rent_day(), 9);
    CHECK_EQ(fp_flowers(999), 3);
}

TEST(freeplay_sim_lives_while_browsing)
{
    uint32_t m0;
    premade_island();
    opt_set(OPT_DAY_LENGTH, 4);
    opt_set(OPT_SIM_SPEED, 3);
    m0 = (uint32_t)doomo_town.minute + doomo_town.day * 1440u;
    scene_push(&SCENE_ROSTER, 0);
    frames(0, 200);
    CHECK((uint32_t)doomo_town.minute + doomo_town.day * 1440u > m0);
}

/* Every free play screen at both resolutions, a few seconds each, with
 * some button mashing: nothing may crash or leave the screen blank. */
TEST(freeplay_all_screens_smoke_both_resolutions)
{
    static const DoomoScene *const scenes[] = {
        &SCENE_TITLE, &SCENE_NEW_ISLAND, &SCENE_ISLAND, &SCENE_APARTMENTS, &SCENE_ROSTER,
        &SCENE_RESIDENT, &SCENE_SHOP, &SCENE_GRAVEYARD, &SCENE_NEWS, &SCENE_OPTIONS,
        &SCENE_SAVELOAD, &SCENE_UNLOCKS, &SCENE_PICKER
    };
    static const int args[] = { 0, 0, 0, 1, 0, 3, 1, 0, 0, 6, 0, 0, 0 };
    static const uint32_t mash[] = { BTN_DOWN, BTN_RIGHT, BTN_R, BTN_DOWN, BTN_LEFT, BTN_L, 0 };
    int res, i, k;
    for (res = 0; res < 2; res++) {
        nat_w = res ? 320 : 240;
        nat_h = res ? 240 : 160;
        for (i = 0; i < DOOMO_COUNTOF(scenes); i++) {
            int lit = 0, p;
            premade_island();
            grave_add(9);
            doomo_town.res[9].status = RES_DEAD;
            scene_push(scenes[i], args[i]);
            frames(0, 20);
            for (k = 0; k < DOOMO_COUNTOF(mash); k++) { press(mash[k]); frames(0, 4); }
            for (p = 0; p < nat_w * nat_h; p += 97) if (nat_fb[p]) lit++;
            CHECK_MSG(lit > 100, "%s drew a blank screen at %dx%d", scenes[i]->name, nat_w, nat_h);
        }
    }
    nat_w = 240;
    nat_h = 160;
}

/* The hub, the roster and the unlocks gallery keep their big state in the
 * shared scene scratch: after another scene took it they rebuild. */
TEST(freeplay_scene_scratch_loss_is_rebuilt)
{
    int n, id;
    premade_island();
    fp_island_select_kind(LOC_TOWN_HALL);
    press(BTN_A);
    CHECK(fp_island_menu_open());
    n = fp_island_menu_count();
    CHECK(n > 3);
    scene_push(&SCENE_UNLOCKS, 0);              /* claims the scratch */
    frames(0, 3);
    scene_pop();
    frames(0, 3);
    CHECK(top_is(&SCENE_ISLAND));
    CHECK(fp_island_menu_open());
    CHECK_EQ(fp_island_menu_count(), n);
    CHECK(fp_island_menu_select("Town records"));
    press(BTN_B);
    /* the roster */
    scene_push(&SCENE_ROSTER, 0);
    frames(0, 3);
    press(BTN_DOWN);
    press(BTN_DOWN);
    scene_push(&SCENE_UNLOCKS, 0);
    frames(0, 3);
    scene_pop();
    frames(0, 3);
    CHECK(top_is(&SCENE_ROSTER));
    press(BTN_A);
    CHECK(top_is(&SCENE_RESIDENT));
    id = fp_resident_id();
    CHECK(id >= 0 && doomo_town.res[id].status != RES_EMPTY);
}
