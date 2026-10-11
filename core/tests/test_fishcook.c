/* test_fishcook.c - Lake of Fire fishing + Brimstone Bites cooking (owner:
 * fishcook): data tables, bite rules, catches, the save chunk, recipes,
 * grading, ingredients, customers, unlocks, and both scenes played by
 * their bots from start to finish. */
#include "test.h"
#include "../src/fishcook_int.h"

static uint8_t fc_image[160 * 1024];

static void tick_scene(int draw)
{
    input_update(0);
    scene_update();
    if (draw) scene_draw();
}

static void unlock_all_recipes(void)
{
    int r;
    for (r = 0; r < RECIPE_COUNT; r++)
        if (cook_recipes[r].unlock != ACT_ALWAYS) unlock_grant(cook_recipes[r].unlock);
    ui_toast_clear();
}

/* ====================================================================== */
TEST(fishcook_items_appended)
{
    int it;
    CHECK(ITEM_COUNT <= 64);
    CHECK(ITEM_SOCK_STEW == ITEM_COUNT - 1);
    for (it = ITEM_BLOOD_ORANGE; it < ITEM_COUNT; it++) {
        const DoomoItemInfo *d = &doomo_items[it];
        CHECK_MSG(d->name && d->name[0] && strlen(d->name) <= 16, "item %d name", it);
        CHECK_MSG(d->desc && strlen(d->desc) > 4 && strlen(d->desc) <= 36, "item %d desc", it);
        CHECK_MSG(d->category == ICAT_FOOD && d->price > 0 && d->hunger > 0, "item %d stats", it);
    }
    /* the first four line up with the favourite-food names (sim_feed rule) */
    CHECK_STR(doomo_food_names[ITEM_BLOOD_ORANGE - ITEM_CUPCAKE], "Blood Oranges");
    CHECK_STR(doomo_food_names[ITEM_PICKLES - ITEM_CUPCAKE], "Pickles");
    CHECK_STR(doomo_food_names[ITEM_SPROUTS - ITEM_CUPCAKE], "Brussels Sprouts");
    CHECK_STR(doomo_food_names[ITEM_BONE_BROTH - ITEM_CUPCAKE], "Bone Broth");
    /* dishes carry their recipe's flavour; ingredients without one have none */
    CHECK_EQ(cook_item_food(ITEM_FISH_RAMEN), 14);           /* Ramen     */
    CHECK_EQ(cook_item_food(ITEM_TEMPURA_ROLL), 19);         /* Sushi     */
    CHECK_EQ(cook_item_food(ITEM_CHARRED), 40);              /* Burnt Toast */
    CHECK_EQ(cook_item_food(ITEM_PICKLES), 45);
    CHECK_EQ(cook_item_food(ITEM_CUPCAKE), 0);
    CHECK_EQ(cook_item_food(ITEM_CLOUD_FLOUR), -1);
    CHECK_EQ(cook_item_food(ITEM_SOAP), -1);
}

TEST(fishcook_species_table)
{
    int sp, j, tiers[FR_COUNT] = { 0 }, times[4] = { 0 }, cute = 0, hell = 0;
    CHECK(FISH_COUNT >= 24);
    for (sp = 0; sp < FISH_COUNT; sp++) {
        const FishInfo *f = &fish_info[sp];
        const DoomoUnlock *u = unlock_by_id(FC_UL_DEX(sp));
        CHECK_MSG(f->name && f->desc && strlen(f->desc) < 60, "fish %d text", sp);
        CHECK_MSG(f->size_min > 0 && f->size_min < f->size_max, "fish %d sizes", sp);
        CHECK_MSG(f->strength >= 1 && f->strength <= 9 && f->price > 0, "fish %d stats", sp);
        CHECK(f->rarity < FR_COUNT && f->time < 4);
        CHECK_MSG(u && strstr(u->name, f->name), "fish %d has no Fishdex unlock", sp);
        tiers[f->rarity]++;
        times[f->time]++;
        if (f->hell) hell++; else cute++;
        for (j = 0; j < sp; j++) CHECK(strcmp(fish_info[j].name, f->name) != 0);
    }
    for (j = 0; j < FR_COUNT; j++) CHECK_MSG(tiers[j] > 0, "no fish of rarity %d", j);
    for (j = 0; j < 4; j++) CHECK_MSG(times[j] > 0, "no fish for time %d", j);
    CHECK(cute >= 10 && hell >= 10);
    CHECK_STR(fish_info[FISH_VAPULA_SOCK].name, "Lord Vapula's Lost Sock");
}

TEST(fishcook_unlock_table)
{
    const DoomoUnlock *u;
    int n = 0, i, r;
    static uint8_t seen[64];
    memset(seen, 0, sizeof(seen));
    for (u = doomo_unlocks_fishcook; u->name; u++, n++) {
        CHECK_MSG(u->id >= UL_IDS_FISHCOOK_FIRST && u->id <= UL_IDS_FISHCOOK_LAST, "id %d", u->id);
        CHECK_MSG(!seen[u->id - 960], "duplicate id %d", u->id);
        seen[u->id - 960] = 1;
        CHECK(u->how && u->how[0]);
        if (u->cat == UL_CAT_ITEM) CHECK(u->index >= ITEM_BLOOD_ORANGE && u->index < ITEM_COUNT);
        else CHECK(u->cat == UL_CAT_MISC);
        if (u->need) CHECK(u->prog == PROG_FISH_CAUGHT);
    }
    CHECK(n >= FISH_COUNT + 20);
    for (r = 0; r < RECIPE_COUNT; r++)
        if (cook_recipes[r].unlock != ACT_ALWAYS) {
            u = unlock_by_id(cook_recipes[r].unlock);
            CHECK_MSG(u && strstr(u->name, cook_recipes[r].name), "recipe %d unlock", r);
        }
    for (i = FC_UL_FIRST_BITE; i <= FC_UL_LAST; i++) CHECK(unlock_by_id(i) != NULL);
}

TEST(fishcook_activities_registered)
{
    const DoomoActivity *out[16];
    int n, i, fish = 0, cook = 0;
    const DoomoActivity *a = doomo_activities_extra;
    CHECK(a != NULL);
    for (n = 0; a[n].name; n++) {
        CHECK(a[n].location == ACTLOC_LAKE || a[n].location == ACTLOC_CAFE);
        CHECK(strlen(a[n].desc) <= 40);
        CHECK((a[n].flags & (ACTF_FREEPLAY | ACTF_STORY)) == (ACTF_FREEPLAY | ACTF_STORY));
    }
    CHECK_EQ(n, 4);
    n = activities_at(ACTLOC_LAKE, out, 16);
    for (i = 0; i < n; i++) fish += out[i]->scene == &SCENE_FISHING;
    n = activities_at(ACTLOC_CAFE, out, 16);
    for (i = 0; i < n; i++) cook += out[i]->scene == &SCENE_COOKING;
    CHECK_EQ(fish, 2);
    CHECK_EQ(cook, 2);
    doomo_town.mode = TOWN_STORY;
    doomo_town.story.chapter = 0;
    n = activities_at(ACTLOC_LAKE, out, 16);
    for (i = 0, fish = 0; i < n; i++) fish += out[i]->scene == &SCENE_FISHING;
    CHECK_EQ(fish, 0);
    doomo_town.story.chapter = 1;
    n = activities_at(ACTLOC_CAFE, out, 16);
    for (i = 0, cook = 0; i < n; i++) cook += out[i]->scene == &SCENE_COOKING;
    CHECK_EQ(cook, 2);
    CHECK(scene_by_name("FISHING") == &SCENE_FISHING);
    CHECK(scene_by_name("COOKING") == &SCENE_COOKING);
}

/* ====================================================================== */
TEST(fish_time_of_day_gates)
{
    int sp;
    for (sp = 0; sp < FISH_COUNT; sp++) {
        int t = fish_info[sp].time;
        int noon = fish_weight(sp, 12, BAIT_NONE), night = fish_weight(sp, 23, BAIT_NONE);
        int dusk = fish_weight(sp, 19, BAIT_NONE);
        if (t == FT_NIGHT) CHECK_MSG(noon == 0 && night > 0, "night fish %d", sp);
        if (t == FT_DAY) CHECK_MSG(noon > 0 && night == 0 && dusk > 0, "day fish %d", sp);
        if (t == FT_TWILIGHT) CHECK_MSG(noon == 0 && night == 0 && dusk > 0, "dusk fish %d", sp);
        if (t == FT_ANY) CHECK_MSG(noon > 0 && night > 0 && dusk > 0, "any-time fish %d", sp);
    }
    CHECK_EQ(fish_hour_kind(3), FT_NIGHT);
    CHECK_EQ(fish_hour_kind(6), FT_TWILIGHT);
    CHECK_EQ(fish_hour_kind(12), FT_DAY);
    CHECK_EQ(fish_hour_kind(19), FT_TWILIGHT);
    /* story: the lost sock only bites once its owner is gone */
    doomo_town.mode = TOWN_STORY;
    doomo_town.story.chapter = 3;
    CHECK_EQ(fish_weight(FISH_VAPULA_SOCK, 23, BAIT_NONE), 0);
    doomo_town.story.chapter = 4;
    CHECK(fish_weight(FISH_VAPULA_SOCK, 23, BAIT_NONE) > 0);
}

static void count_rolls(int hour, int bait, int *cute, int *hell, int *rare, int *common, uint32_t seed)
{
    DoomoRng r;
    int i;
    rng_seed(&r, seed);
    *cute = *hell = *rare = *common = 0;
    for (i = 0; i < 3000; i++) {
        int sp = fish_roll_species(&r, hour, bait);
        const FishInfo *f = &fish_info[sp];
        CHECK(fish_time_ok(sp, hour));
        if (f->rarity == FR_JUNK) continue;
        if (f->hell) (*hell)++; else (*cute)++;
        if (f->rarity >= FR_RARE) (*rare)++;
        if (f->rarity == FR_COMMON) (*common)++;
    }
}

TEST(fish_rolls_follow_rarity_and_bait)
{
    int c0, h0, r0, k0, c1, h1, r1, k1;
    DoomoRng a, b;
    int i;
    count_rolls(12, BAIT_NONE, &c0, &h0, &r0, &k0, 5);
    CHECK(k0 > r0 * 3);                                     /* commons dominate */
    CHECK(r0 > 0);
    count_rolls(12, BAIT_CUPCAKE, &c1, &h1, &r1, &k1, 5);
    CHECK(c1 * 100 / (c1 + h1) > c0 * 100 / (c0 + h0) + 10);   /* cute fish love cupcakes */
    count_rolls(12, BAIT_PEPPERS, &c1, &h1, &r1, &k1, 5);
    CHECK(h1 * 100 / (c1 + h1) > h0 * 100 / (c0 + h0) + 10);   /* hellish ones love peppers */
    count_rolls(12, BAIT_EYEBALLS, &c1, &h1, &r1, &k1, 5);
    CHECK(r1 > r0);                                         /* rare fish peek */
    /* deterministic */
    rng_seed(&a, 77);
    rng_seed(&b, 77);
    for (i = 0; i < 50; i++) CHECK_EQ(fish_roll_species(&a, 23, BAIT_NONE), fish_roll_species(&b, 23, BAIT_NONE));
}

TEST(fish_sizes_and_values)
{
    DoomoRng r;
    int sp, i;
    rng_seed(&r, 9);
    for (sp = 0; sp < FISH_COUNT; sp++) {
        const FishInfo *f = &fish_info[sp];
        int span = f->size_max - f->size_min;
        for (i = 0; i < 60; i++) {
            int s = fish_roll_size(&r, sp, i & 1 ? BAIT_MEAT : BAIT_NONE);
            CHECK_MSG(s >= f->size_min && s <= f->size_max + 1 + span / 8 + span / 6 + 1, "fish %d size %d", sp, s);
        }
        CHECK(fish_value(sp, f->size_max) >= fish_value(sp, f->size_min));
        CHECK(fish_value(sp, f->size_min) >= 1);
    }
    CHECK(fish_value(FISH_VAPULA_SOCK, 30) > fish_value(FISH_BUBBLEGUM_GUPPY, 8) * 20);
}

TEST(fish_catch_dex_record_cooler)
{
    uint32_t money = doomo_town.money, prog = progress_get(PROG_FISH_CAUGHT);
    int f;
    CHECK(!fish_dex_has(FISH_PLUSH_KOI));
    f = fish_register_catch(FISH_PLUSH_KOI, 40);
    CHECK(f & CATCH_NEW);
    CHECK(!(f & CATCH_RECORD));
    CHECK(fish_dex_has(FISH_PLUSH_KOI));
    CHECK_EQ(fc.record[FISH_PLUSH_KOI], 40);
    CHECK_EQ(progress_get(PROG_FISH_CAUGHT), prog + 1);
    CHECK(unlock_has(FC_UL_FIRST_BITE));                       /* automatic rule */
    f = fish_register_catch(FISH_PLUSH_KOI, 55);
    CHECK_EQ(f, CATCH_RECORD);
    f = fish_register_catch(FISH_PLUSH_KOI, 31);
    CHECK_EQ(f, 0);
    CHECK_EQ(fc.caught[FISH_PLUSH_KOI], 3);
    fish_keep(FISH_PLUSH_KOI);
    fish_keep(FISH_PLUSH_KOI);
    CHECK_EQ(fish_cooler_total(), 2);
    CHECK(fish_sell(FISH_PLUSH_KOI, 55) > 0);
    CHECK(doomo_town.money > money);
    money = doomo_town.money;
    CHECK(fish_sell_cooler(FISH_PLUSH_KOI, 99) > 0);
    CHECK_EQ(fc.cooler[FISH_PLUSH_KOI], 0);
    CHECK(doomo_town.money > money);
    CHECK_EQ(fc.sold, 3);
    fish_release(FISH_PLUSH_KOI);
    CHECK_EQ(fc.released, 1);
    /* recipes and trophies that hang on catches */
    CHECK(!cook_recipe_ok(15));
    fish_register_catch(FISH_SCREAMING_CARP, 40);
    CHECK(cook_recipe_ok(15));                                  /* Eyeball Jelly */
    fish_register_catch(FISH_MAMA_MAGMAW, 320);
    CHECK(unlock_has(FC_UL_ROD_GOLD));
    CHECK(unlock_has(FC_UL_WHOPPER));
    CHECK(fish_rod_ok(ROD_GOLDEN));
    CHECK(!fish_rod_ok(ROD_HELLSTEEL));
}

TEST(fish_save_chunk_round_trip)
{
    uint32_t n;
    int i;
    fish_register_catch(FISH_LAVA_EEL, 150);
    fish_keep(FISH_LAVA_EEL);
    fc.cooler[FISH_GOTH_GUPPY] = 7;
    fc.grade[3] = GRADE_PERFECT;
    fc.rod = ROD_TWIG;
    fc.bait = BAIT_PEPPERS;
    fc.cooked = 12;
    fc.perfects = 3;
    fc.cust_day = 5;
    fc.cust[2] = 321;
    fc.cust_served = 5;
    {
        FcData before = fc;
        n = save_serialize(fc_image, sizeof(fc_image));
        CHECK(n > 0);
        town_new(TOWN_FREEPLAY, 1, TOWN_POP_EMPTY);             /* resets the chunk */
        CHECK_EQ(fc.cooked, 0);
        CHECK_EQ(fc.cust[2], -1);
        CHECK(save_deserialize(fc_image, n));
        CHECK_EQ(fc.record[FISH_LAVA_EEL], 150);
        CHECK_EQ(fc.cooler[FISH_LAVA_EEL], 1);
        CHECK_EQ(fc.cooler[FISH_GOTH_GUPPY], 7);
        CHECK_EQ(fc.grade[3], GRADE_PERFECT);
        CHECK_EQ(fc.bait, BAIT_PEPPERS);
        CHECK_EQ(fc.cooked, 12);
        CHECK_EQ(fc.cust[2], 321);
        CHECK_EQ(fc.cust_served, 5);
        for (i = 0; i < FISH_COUNT; i++) CHECK(fc.caught[i] == before.caught[i]);
        CHECK(fish_dex_has(FISH_LAVA_EEL));
    }
}

/* Every species can be landed by the bot (forced bites), at the right
 * hour, drawing as it goes, and the fight has a sane length. */
TEST(fish_scene_bot_lands_every_species)
{
    int sp;
    for (sp = 0; sp < FISH_COUNT; sp++) {
        int i, start = -1, landed = 0;
        const FishInfo *f = &fish_info[sp];
        doomo_town.minute = (uint16_t)((f->time == FT_NIGHT ? 23 : f->time == FT_TWILIGHT ? 19 : 12) * 60);
        scene_set(&SCENE_FISHING, FISH_ARG_AUTOPLAY | FISH_ARG_FORCE(sp));
        for (i = 0; i < 2400; i++) {
            int st;
            tick_scene((i & 7) == 0);
            st = fish_scene_state();
            if (st == 5 && start < 0) start = i;               /* FS_REEL   */
            if (st == 6) { landed = 1; break; }                 /* FS_LANDED */
            CHECK_MSG(st != 8, "lost %s", f->name);              /* FS_LOST   */
        }
        CHECK_MSG(landed, "bot never landed %s", f->name);
        CHECK_EQ(fish_scene_species(), sp);
        CHECK_MSG(i - start >= 45 && i - start <= 600, "%s fight took %d ticks", f->name, i - start);
        /* the card: the bot keeps it */
        for (i = 0; i < 200 && fish_scene_state() != 0; i++) tick_scene(i == 50);
        CHECK_EQ(fish_scene_state(), 0);                         /* FS_IDLE   */
        CHECK(fish_dex_has(sp));
        CHECK(fc.cooler[sp] >= 1);
    }
    CHECK(unlock_has(FC_UL_MASTER));
    CHECK(unlock_has(FC_UL_NIGHT_OWL));
    CHECK(unlock_has(FC_UL_LEGEND));
}

TEST(fish_scene_menus_dex_cooler)
{
    int i;
    fc.cooler[FISH_HONEY_BASS] = 3;
    scene_set(&SCENE_FISHING, FISH_ARG_DEMO);
    for (i = 0; i < 5; i++) tick_scene(1);
    CHECK_EQ(fish_scene_state(), 0);
    input_update(BTN_START);
    scene_update();
    scene_draw();
    CHECK_EQ(fish_scene_state(), 9);                             /* FS_MENU */
    input_update(BTN_A);
    scene_update();
    scene_draw();
    CHECK_EQ(fish_scene_state(), 10);                            /* FS_DEX  */
    for (i = 0; i < 40; i++) {
        input_update(i & 1 ? 0 : (i < 20 ? BTN_RIGHT : BTN_DOWN));
        scene_update();
        scene_draw();
    }
    input_update(BTN_B);
    scene_update();
    CHECK_EQ(fish_scene_state(), 9);
    input_update(0); scene_update();
    input_update(BTN_DOWN); scene_update();
    input_update(0); scene_update();
    input_update(BTN_A); scene_update(); scene_draw();
    CHECK_EQ(fish_scene_state(), 11);                            /* FS_COOLER */
    {
        uint32_t m = doomo_town.money;
        input_update(0); scene_update();
        input_update(BTN_R); scene_update(); scene_draw();
        CHECK(doomo_town.money > m);
    }
    /* B twice leaves the lake */
    input_update(0); scene_update();
    input_update(BTN_B); scene_update();
    input_update(0); scene_update();
    input_update(BTN_DOWN); scene_update(); input_update(0); scene_update();
    input_update(BTN_DOWN); scene_update(); input_update(0); scene_update();
    input_update(BTN_A); scene_update();
    CHECK(scene_top() != &SCENE_FISHING);
}

TEST(fish_scene_pressing_early_spooks)
{
    int i;
    doomo_town.minute = 12 * 60;
    scene_set(&SCENE_FISHING, 0);
    tick_scene(0);
    input_update(BTN_A); scene_update();                 /* wind up */
    for (i = 0; i < 20; i++) { input_update(BTN_A); scene_update(); }
    input_update(0); scene_update();                     /* cast    */
    for (i = 0; i < 25; i++) tick_scene(0);
    CHECK_EQ(fish_scene_state(), 3);                     /* FS_WAIT */
    input_update(BTN_A); scene_update();
    CHECK_EQ(fish_scene_state(), 8);                     /* FS_LOST */
    for (i = 0; i < 60; i++) tick_scene(0);
    CHECK_EQ(fish_scene_state(), 0);
}

/* ====================================================================== */
TEST(cook_recipe_table)
{
    int r, i, start = 0, fishy = 0;
    CHECK(RECIPE_COUNT >= 20);
    for (r = 0; r < RECIPE_COUNT; r++) {
        const Recipe *rc = &cook_recipes[r];
        int n = 0;
        CHECK_MSG(rc->name && rc->desc && strlen(rc->desc) <= 40, "recipe %d text", r);
        CHECK(doomo_items[rc->item].category == ICAT_FOOD);
        CHECK(rc->food >= -1 && rc->food < FOOD_COUNT);
        CHECK(rc->diff >= 1 && rc->diff <= 5 && rc->yield >= 1);
        CHECK(rc->heat <= HEAT_SCORCH);
        for (i = 0; i < 3; i++) {
            const CookIng *g = &rc->ing[i];
            if (g->kind == ING_NONE) continue;
            n++;
            CHECK(g->qty >= 1);
            if (g->kind == ING_ITEM) CHECK(doomo_items[g->id].category == ICAT_FOOD);
            if (g->kind == ING_FISH) CHECK(g->id < FISH_COUNT);
            if (g->kind >= ING_FISH) fishy++;
        }
        CHECK_MSG(n >= 2 || rc->ing[0].kind == ING_FISH, "recipe %d needs ingredients", r);
        if (cook_recipe_ok(r)) start++;
        for (i = 0; i < r; i++) CHECK(strcmp(cook_recipes[i].name, rc->name) != 0);
    }
    CHECK_EQ(start, 6);
    CHECK(fishy >= 8);
    /* every new dish item has a recipe */
    for (i = ITEM_FISH_RAMEN; i < ITEM_COUNT; i++) {
        int found = 0;
        for (r = 0; r < RECIPE_COUNT; r++) found |= cook_recipes[r].item == i;
        CHECK_MSG(found, "item %d has no recipe", i);
    }
}

TEST(cook_grades_and_yields)
{
    CHECK_EQ(cook_grade(0, 100, 100, 100), GRADE_PERFECT);
    CHECK_EQ(cook_grade(0, 80, 80, 80), GRADE_GREAT);
    CHECK_EQ(cook_grade(0, 50, 50, 50), GRADE_OK);
    CHECK_EQ(cook_grade(0, 10, 10, 10), GRADE_BURNT);
    CHECK_EQ(cook_grade(0, 100, 100, 0), GRADE_OK);             /* heat counts double */
    CHECK_EQ(cook_grade(18, 0, 0, 0), GRADE_OK);                /* can't burn a Charred Mystery */
    CHECK_EQ(cook_yield(0, GRADE_OK), cook_recipes[0].yield);
    CHECK_EQ(cook_yield(0, GRADE_PERFECT), cook_recipes[0].yield + 2);
    CHECK_EQ(cook_yield(0, GRADE_BURNT), 1);
    CHECK_EQ(cook_result_item(0, GRADE_BURNT), ITEM_CHARRED);
    CHECK_EQ(cook_result_item(0, GRADE_GREAT), ITEM_CUPCAKE);
}

TEST(cook_ingredients_buy_and_take)
{
    int r = 0;                                                  /* Pink Cupcakes */
    doomo_town.money = 1000;
    CHECK(!cook_can_make(r));
    CHECK_EQ(cook_missing_cost(r), doomo_items[ITEM_CLOUD_FLOUR].price + doomo_items[ITEM_PIXIE_SUGAR].price +
                                  doomo_items[ITEM_SPECKLED_EGG].price);
    CHECK(cook_buy_missing(r));
    CHECK(cook_can_make(r));
    CHECK(doomo_town.money < 1000);
    cook_take(r);
    CHECK_EQ(inv_count(ITEM_CLOUD_FLOUR), 0);
    CHECK(!cook_can_make(r));
    /* fish can't be bought: go fishing */
    CHECK_EQ(cook_missing_cost(4), -1);                          /* Bone Broth */
    fc.cooler[FISH_GOTH_GUPPY] = 1;
    fc.cooler[FISH_HELLMOUTH_BASS] = 1;
    fc.cooler[FISH_BUBBLEGUM_GUPPY] = 2;                         /* cute: no use */
    CHECK_EQ(cook_have(&cook_recipes[4].ing[0]), 2);
    inv_add(ITEM_SPROUTS, 1);
    CHECK(cook_can_make(4));
    cook_take(4);
    CHECK_EQ(fc.cooler[FISH_GOTH_GUPPY], 0);                     /* the cheap one went first */
    CHECK_EQ(fc.cooler[FISH_HELLMOUTH_BASS], 1);
    CHECK_EQ(fc.cooler[FISH_BUBBLEGUM_GUPPY], 2);
    /* junk is never an ingredient */
    fc.cooler[FISH_SOGGY_BOOT] = 5;
    CHECK_EQ(cook_have(&cook_recipes[8].ing[2]), 3);             /* any fish: 1 + 2 */
}

TEST(cook_finish_unlocks)
{
    int i;
    CHECK(!cook_recipe_ok(6));
    for (i = 0; i < 3; i++) CHECK(cook_finish(0, GRADE_OK) == cook_recipes[0].yield);
    CHECK(cook_recipe_ok(6));                                   /* Candy Eyeballs */
    CHECK_EQ(inv_count(ITEM_CUPCAKE), 3 + 3 * cook_recipes[0].yield);
    CHECK(!cook_recipe_ok(18));
    CHECK_EQ(cook_finish(2, GRADE_BURNT), 1);
    CHECK_EQ(inv_count(ITEM_CHARRED), 1);
    CHECK(cook_recipe_ok(18));                                  /* Charred Mystery */
    CHECK(!unlock_ok(UL_CAT_ITEM, 0, ITEM_FISH_RAMEN));         /* not in shops yet */
    cook_finish(4, GRADE_GREAT);
    CHECK(cook_recipe_ok(9));                                   /* Fish Ramen     */
    CHECK(unlock_ok(UL_CAT_ITEM, 0, ITEM_FISH_RAMEN));
    CHECK(!unlock_has(FC_UL_GOLD_STAR));
    cook_finish(1, GRADE_PERFECT);
    CHECK(unlock_has(FC_UL_GOLD_STAR));
    CHECK_EQ(fc.grade[1], GRADE_PERFECT);
    cook_finish(1, GRADE_OK);
    CHECK_EQ(fc.grade[1], GRADE_PERFECT);                       /* best grade kept */
    CHECK_EQ(fc.cooked, 7);
}

/* The bot cooks every recipe from the book to the plate. */
TEST(cook_scene_bot_cooks_every_recipe)
{
    int r, good = 0;
    unlock_all_recipes();
    for (r = 0; r < RECIPE_COUNT; r++) {
        int i, it = cook_recipes[r].item, before, g;
        fc.cooler[FISH_VAPULA_SOCK] = 1;
        fc.cooler[FISH_MARSHMALLOW_MINNOW] = 1;
        fc.cooler[FISH_STARLIGHT_SUNFISH] = 1;
        before = inv_count(it);
        scene_set(&SCENE_COOKING, COOK_ARG_AUTOPLAY | COOK_ARG_STOCK | COOK_ARG_RECIPE(r));
        for (i = 0; i < 1500 && cook_scene_state() != 7; i++) tick_scene((i & 7) == 0);
        CHECK_MSG(cook_scene_state() == 7, "recipe %d never finished", r);   /* CS_RESULT */
        for (i = 0; i < 30; i++) tick_scene(i == 20);
        g = cook_scene_grade();
        CHECK_MSG(g >= GRADE_OK, "recipe %d grade %d (%d/%d/%d)", r, g,
                  cook_scene_score(0), cook_scene_score(1), cook_scene_score(2));
        if (g >= GRADE_GREAT) good++;
        CHECK_MSG(inv_count(it) > before || inv_count(it) == 99, "recipe %d made nothing", r);
    }
    CHECK_MSG(good >= RECIPE_COUNT - 2, "the bot cooked only %d good dishes", good);
}

TEST(cook_scene_book_buy_and_leave)
{
    int i;
    doomo_town.money = 500;
    scene_push(&SCENE_COOKING, 0);
    for (i = 0; i < 4; i++) tick_scene(1);
    CHECK_EQ(cook_scene_state(), 0);                              /* CS_BOOK */
    input_update(BTN_RIGHT); scene_update(); scene_draw();
    CHECK_EQ(cook_scene_recipe(), 1);
    input_update(0); scene_update();
    input_update(BTN_LEFT); scene_update();
    input_update(0); scene_update();
    input_update(BTN_A); scene_update(); scene_draw();
    CHECK_EQ(cook_scene_state(), 1);                              /* CS_BUY  */
    input_update(0); scene_update();
    input_update(BTN_A); scene_update();
    CHECK_EQ(cook_scene_state(), 2);                              /* cooking! */
    CHECK(doomo_town.money < 500);
    /* a locked page refuses */
    scene_set(&SCENE_COOKING, 0);
    for (i = 0; i < 6; i++) { input_update(i & 1 ? 0 : BTN_RIGHT); scene_update(); }
    CHECK_EQ(cook_scene_recipe(), 3);
    for (i = 0; i < 6; i++) { input_update(i & 1 ? 0 : BTN_RIGHT); scene_update(); }
    CHECK(!cook_recipe_ok(cook_scene_recipe()));
    input_update(BTN_A); scene_update(); scene_draw();
    CHECK_EQ(cook_scene_state(), 0);
    input_update(0); scene_update();
    input_update(BTN_B); scene_update();
    CHECK(scene_top() != &SCENE_COOKING);
}

/* ====================================================================== */
TEST(cook_customers_and_serving)
{
    int n, i, j, id, kind, tip, served_fav = 0;
    char line[96];
    uint32_t meals;
    town_new(TOWN_FREEPLAY, 4242, TOWN_POP_PREMADE);
    n = cook_customers();
    CHECK_EQ(n, FC_CUSTOMERS);
    for (i = 0; i < FC_CUSTOMERS; i++) {
        id = cook_customer(i);
        CHECK(id >= 0 && town_res(id)->status == RES_ALIVE);
        for (j = 0; j < i; j++) CHECK(cook_customer(j) != id);
    }
    /* the same customers all day */
    {
        int keep = cook_customer(0);
        CHECK_EQ(cook_customers(), FC_CUSTOMERS);
        CHECK_EQ(cook_customer(0), keep);
        doomo_town.day++;
        cook_customers();
        CHECK_EQ(fc.cust_served, 0);
    }
    /* make customer 0 crave Ramen and serve them a Fish Ramen */
    id = cook_customer(0);
    town_res(id)->like_food = 14;
    town_res(id)->hate_food = 46;
    inv_add(ITEM_FISH_RAMEN, 1);
    meals = progress_get(PROG_MEALS_SERVED);
    {
        uint32_t m = doomo_town.money;
        int lvl = town_res(id)->level;
        kind = cook_serve(0, ITEM_FISH_RAMEN, line, sizeof(line), &tip);
        CHECK_EQ(kind, SERVE_FAV);
        CHECK(strstr(line, "Ramen") != NULL);
        CHECK(tip > 0 && doomo_town.money == m + (uint32_t)tip);
        CHECK(town_res(id)->level > lvl);
        CHECK_EQ(inv_count(ITEM_FISH_RAMEN), 0);
        CHECK_EQ(progress_get(PROG_MEALS_SERVED), meals + 1);      /* via sim_feed */
        served_fav = 1;
    }
    CHECK(unlock_has(FC_UL_REGULAR));
    CHECK(cook_customer_served(0));
    inv_add(ITEM_FISH_RAMEN, 1);
    CHECK_EQ(cook_serve(0, ITEM_FISH_RAMEN, line, sizeof(line), &tip), SERVE_FAIL);   /* already fed */
    /* hated flavour */
    id = cook_customer(1);
    town_res(id)->like_food = 0;
    town_res(id)->hate_food = 17;                                  /* Pizza */
    inv_add(ITEM_PICKLE_PIZZA, 1);
    CHECK_EQ(cook_serve(1, ITEM_PICKLE_PIZZA, line, sizeof(line), &tip), SERVE_HATE);
    CHECK_EQ(tip, 0);
    /* nothing in the bag: refused */
    CHECK_EQ(inv_count(ITEM_TAKOYAKI), 0);
    CHECK_EQ(cook_serve(2, ITEM_TAKOYAKI, line, sizeof(line), &tip), SERVE_FAIL);
    /* aligned items use the sim's own favourite reaction */
    id = cook_customer(2);
    town_res(id)->like_food = ITEM_BLOOD_ORANGE - ITEM_CUPCAKE;
    inv_add(ITEM_BLOOD_ORANGE, 1);
    CHECK_EQ(cook_serve(2, ITEM_BLOOD_ORANGE, line, sizeof(line), &tip), SERVE_FAV);
    CHECK_EQ(town_res(id)->mood, MOOD_ECSTATIC);
    inv_add(ITEM_CUPCAKE, 1);
    town_res(cook_customer(3))->like_food = 1;
    town_res(cook_customer(3))->hate_food = 2;
    CHECK_EQ(cook_serve(3, ITEM_CUPCAKE, line, sizeof(line), &tip), SERVE_OK);
    CHECK(unlock_has(FC_UL_FULL_HOUSE));
    CHECK(served_fav);
    CHECK_EQ(fc.served, 4);
}

TEST(cook_scene_counter_bot_serves)
{
    int i;
    scene_set(&SCENE_COOKING, COOK_ARG_COUNTER | COOK_ARG_AUTOPLAY | COOK_ARG_DEMO | COOK_ARG_STOCK);
    CHECK_EQ(cook_scene_state(), 8);                              /* CS_COUNTER */
    for (i = 0; i < 80; i++) tick_scene((i & 3) == 0);
    CHECK_EQ(fc.served, 1);
    CHECK(cook_customer_served(0) || cook_customer_served(1) || cook_customer_served(2) || cook_customer_served(3));
    /* empty island: no customers, still draws */
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_EMPTY);
    scene_set(&SCENE_COOKING, COOK_ARG_COUNTER);
    for (i = 0; i < 10; i++) tick_scene(1);
    CHECK_EQ(cook_customers(), 0);
}

/* Drawing never strays outside the screen, at both layouts. */
TEST(fishcook_draw_both_resolutions)
{
    int res, i;
    static const int dims[2][2] = { { 240, 160 }, { 320, 240 } };
    for (res = 0; res < 2; res++) {
        NatOptions o;
        memset(&o, 0, sizeof(o));
        o.w = dims[res][0]; o.h = dims[res][1]; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
        nat_init(&o);
        scene_set(&SCENE_FISHING, FISH_ARG_DEMO | FISH_ARG_DEX);
        for (i = 0; i < 10; i++) tick_scene(1);
        scene_set(&SCENE_FISHING, FISH_ARG_AUTOPLAY | FISH_ARG_NIGHT | FISH_ARG_FORCE(FISH_VAPULA_SOCK));
        for (i = 0; i < 300; i++) tick_scene(1);
        scene_set(&SCENE_COOKING, COOK_ARG_STOCK);
        for (i = 0; i < 10; i++) tick_scene(1);
        unlock_all_recipes();
        scene_set(&SCENE_COOKING, COOK_ARG_AUTOPLAY | COOK_ARG_STOCK | COOK_ARG_RECIPE(9));
        for (i = 0; i < 700; i++) tick_scene(1);
        CHECK(fc_fx_live() >= 0);
    }
    {
        NatOptions o;
        memset(&o, 0, sizeof(o));
        o.w = 240; o.h = 160; o.seed = 1; o.quiet = 1; o.save_dir = test_tmp_dir;
        nat_init(&o);
    }
}
