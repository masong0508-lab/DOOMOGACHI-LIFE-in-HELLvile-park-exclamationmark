/* cook_data.c - Granny Hopscotch's recipe book, ingredients, grading and
 * the customers at the Brimstone Bites counter (owner: fishcook). See
 * fishcook_int.h.
 *
 * Every recipe is CHOP -> STIR -> a heat step (BAKE / FRY / SIMMER, or
 * SCORCH for the Charred Mystery: burn it on purpose). The step scores
 * make a grade; the grade sets the portions (OK = yield, GREAT + 1,
 * PERFECT + 2) and a BURNT dish comes out as one Charred Mystery.
 * Ingredients are food items (the old menu and Granny's pantry items) and
 * fish from the cooler (a species, or any cute / hellish / fish at all).
 * A dish keeps its recipe's flavour: serving a Fish Ramen to a citizen
 * who loves Ramen counts as their favourite (cook_serve). */
#include "fishcook_int.h"

#define P(p)  PAL_PAINT(PAINT_##p, 1)
#define PL(p) PAL_PAINT(PAINT_##p, 2)
#define PD(p) PAL_PAINT(PAINT_##p, 0)
#define R(r, s) PAL_RAMP(RAMP_##r, s)
#define I(it, n) { ING_ITEM, ITEM_##it, n }
#define F(sp) { ING_FISH, FISH_##sp, 1 }
#define CUTE { ING_CUTE_FISH, 0, 1 }
#define HELL { ING_HELL_FISH, 0, 1 }
#define ANYF { ING_ANY_FISH, 0, 1 }
#define NONE { ING_NONE, 0, 0 }
#define LOCK(k) (FC_UL_RECIPE0 + (k))

const char *const cook_grade_names[5] = { "", "BURNT", "OK", "GREAT", "PERFECT!" };
const char *const cook_heat_names[4] = { "Bake", "Fry", "Simmer", "Scorch" };

/*  name, desc, item, flavour (doomo_food_names), heat, diff, unlock, yield,
 *  art, colours, ingredients */
const Recipe cook_recipes[RECIPE_COUNT] = {
    { "Pink Cupcakes", "Granny's classic. Sprinkles are the law.",
      ITEM_CUPCAKE, 0, HEAT_BAKE, 1, ACT_ALWAYS, 2,
      DISH_CUPCAKE, PL(BUBBLEGUM), P(CARAMEL), P(CHERRY), { I(CLOUD_FLOUR, 1), I(PIXIE_SUGAR, 1), I(SPECKLED_EGG, 1) } },
    { "Granny's Pudding", "Wobbly custard in a caramel hat.",
      ITEM_PUDDING, 5, HEAT_SIMMER, 1, ACT_ALWAYS, 2,
      DISH_PUDDING, PL(LEMON), P(CARAMEL), PL(WHITE), { I(MILK, 1), I(PIXIE_SUGAR, 1), I(SPECKLED_EGG, 1) } },
    { "Fluffy Pancakes", "Flip them high. Catch them higher.",
      ITEM_PANCAKES, 12, HEAT_FRY, 1, ACT_ALWAYS, 2,
      DISH_STACK, R(CANDY, 4), PL(LEMON), P(CARAMEL), { I(CLOUD_FLOUR, 1), I(SPECKLED_EGG, 1), I(MILK, 1) } },
    { "Sprout Salad", "Warm, crunchy and secretly healthy.",
      ITEM_SALAD, 9, HEAT_FRY, 1, ACT_ALWAYS, 2,
      DISH_SALAD, P(LIME), P(FOREST), P(TANGERINE), { I(SPROUTS, 1), I(PICKLES, 1), I(BLOOD_ORANGE, 1) } },
    { "Bone Broth", "Fish bones plus patience equals magic.",
      ITEM_BONE_BROTH, 47, HEAT_SIMMER, 2, ACT_ALWAYS, 2,
      DISH_BOWL, R(BONE, 5), P(LIME), P(SKY), { HELL, I(SPROUTS, 1), NONE } },
    { "Brimstone Bagels", "Toasted over real hellfire. Crunch!",
      ITEM_BRIMSTONE_BAGEL, 3, HEAT_BAKE, 2, ACT_ALWAYS, 2,
      DISH_BAGEL, P(CARAMEL), P(HELLFIRE), PL(WHITE), { I(CLOUD_FLOUR, 1), I(SPECKLED_EGG, 1), I(HELL_PEPPERS, 1) } },
    { "Candy Eyeballs", "Boiled sugar that stares right back.",
      ITEM_CANDY_EYEBALLS, 4, HEAT_SIMMER, 2, LOCK(0), 3,
      DISH_BALLS, PL(WHITE), P(COBALT), P(CHERRY), { I(PIXIE_SUGAR, 2), I(BLOOD_ORANGE, 1), NONE } },
    { "Tempura Roll", "Crispy fried fish, rolled with love.",
      ITEM_TEMPURA_ROLL, 19, HEAT_FRY, 2, LOCK(1), 2,
      DISH_ROLL, PL(WHITE), P(JET), P(TANGERINE), { I(ONIGIRI, 1), CUTE, NONE } },
    { "Takoyaki", "Batter balls with a fishy secret.",
      ITEM_TAKOYAKI, 27, HEAT_FRY, 3, LOCK(2), 3,
      DISH_BALLS, P(CARAMEL), P(CHOCOLATE), P(MINT), { I(CLOUD_FLOUR, 1), I(SPECKLED_EGG, 1), ANYF } },
    { "Fish Ramen", "Slurp! Noodles in lake-fish broth.",
      ITEM_FISH_RAMEN, 14, HEAT_SIMMER, 3, LOCK(3), 2,
      DISH_BOWL, R(CANDY, 5), PL(LEMON), P(ROSE), { I(BONE_BROTH, 1), I(CLOUD_FLOUR, 1), I(SPECKLED_EGG, 1) } },
    { "Cursed Cod Chowder", "Creamy, soothing, faintly wailing.",
      ITEM_SOUL_SOUP, 2, HEAT_SIMMER, 3, LOCK(4), 3,
      DISH_BOWL, PL(WHITE), R(SICK, 5), P(GRAPE), { F(CURSED_COD), I(MILK, 1), I(SALAD, 1) } },
    { "Lava Eel Steak", "Seared on its own tail. Sizzling!",
      ITEM_MEAT_SLAB, 8, HEAT_FRY, 3, LOCK(5), 2,
      DISH_STEAK, P(RUST), R(LAVA, 6), P(LIME), { F(LAVA_EEL), I(HELL_PEPPERS, 1), NONE } },
    { "Hellfire Curry", "Spicy enough to see your ancestors.",
      ITEM_CURRY, 20, HEAT_SIMMER, 3, LOCK(6), 2,
      DISH_PLATE, P(TANGERINE), PL(WHITE), P(HELLFIRE), { HELL, I(HELL_PEPPERS, 1), I(ONIGIRI, 1) } },
    { "Pickle Pizza", "Controversial. Delicious. Both.",
      ITEM_PICKLE_PIZZA, 17, HEAT_BAKE, 3, LOCK(7), 2,
      DISH_PIZZA, P(CARAMEL), P(BLOOD), P(LIME), { I(CLOUD_FLOUR, 1), I(PICKLES, 1), I(BLOOD_ORANGE, 1) } },
    { "Grave Dirt Cake", "Cocoa soil, gummy worms, RIP candle.",
      ITEM_GRAVE_CAKE, 43, HEAT_BAKE, 3, LOCK(8), 2,
      DISH_CAKE, PD(CHOCOLATE), P(BUBBLEGUM), R(STONE, 4), { I(CLOUD_FLOUR, 1), I(COFFEE, 1), I(CANDY_EYEBALLS, 1) } },
    { "Eyeball Jelly", "Wobbles. Watches. Tastes of grape.",
      ITEM_EYEBALL_JELLY, 42, HEAT_SIMMER, 3, LOCK(9), 3,
      DISH_JELLY, P(GRAPE), PL(WHITE), P(COBALT), { F(SCREAMING_CARP), I(PIXIE_SUGAR, 1), I(BLOOD_ORANGE, 1) } },
    { "Molten Lava Cake", "Poke it and it erupts. Worth it.",
      ITEM_LAVA_CAKE, 36, HEAT_BAKE, 4, LOCK(10), 2,
      DISH_CAKE, P(CHOCOLATE), R(LAVA, 5), P(HELLFIRE), { I(CLOUD_FLOUR, 1), I(PIXIE_SUGAR, 2), I(HELL_PEPPERS, 1) } },
    { "Angel Cake", "So fluffy it floats off the plate.",
      ITEM_ANGEL_CAKE, 7, HEAT_BAKE, 4, LOCK(11), 2,
      DISH_CAKE, PL(WHITE), PL(LEMON), PL(SKY), { I(CLOUD_FLOUR, 2), I(PIXIE_SUGAR, 2), I(SPECKLED_EGG, 2) } },
    { "Charred Mystery", "Burn it ON PURPOSE. Hellvile loves it.",
      ITEM_CHARRED, 40, HEAT_SCORCH, 1, LOCK(12), 2,
      DISH_LUMP, P(JET), P(HELLFIRE), P(CHARCOAL), { I(CLOUD_FLOUR, 1), I(HELL_PEPPERS, 1), NONE } },
    { "Marshmallow Cupcakes", "Fishy marshmallow cupcakes. Trust Gran.",
      ITEM_CUPCAKE, 0, HEAT_BAKE, 2, LOCK(13), 4,
      DISH_CUPCAKE, PL(WHITE), PL(CARAMEL), PL(PEACH), { F(MARSHMALLOW_MINNOW), I(CLOUD_FLOUR, 1), I(PIXIE_SUGAR, 1) } },
    { "Starlight Pudding", "Twinkles on the spoon. Night magic.",
      ITEM_PUDDING, 5, HEAT_SIMMER, 3, LOCK(14), 4,
      DISH_PUDDING, P(NAVY), PL(LEMON), PL(WHITE), { F(STARLIGHT_SUNFISH), I(MILK, 1), I(PIXIE_SUGAR, 1) } },
    { "Sock Stew", "A demon lord's sock, slow-cooked. Wow.",
      ITEM_SOCK_STEW, -1, HEAT_SIMMER, 5, LOCK(15), 1,
      DISH_BOWL, P(VAPE_PURPLE), P(HELLFIRE), P(CHARCOAL), { F(VAPULA_SOCK), I(BONE_BROTH, 1), I(HELL_PEPPERS, 1) } }
};

int cook_recipe_ok(int r)
{
    if (r < 0 || r >= RECIPE_COUNT) return 0;
    return cook_recipes[r].unlock == ACT_ALWAYS || unlock_has(cook_recipes[r].unlock);
}

/* ---- ingredients ------------------------------------------------------------ */
static int fish_class_ok(int kind, int sp)
{
    const FishInfo *f = &fish_info[sp];
    if (f->shape == FSH_BOOT || f->shape == FSH_CAN || f->shape == FSH_SOCK) return 0;
    switch (kind) {
    case ING_CUTE_FISH: return !f->hell;
    case ING_HELL_FISH: return f->hell;
    case ING_ANY_FISH: return 1;
    default: return 0;
    }
}

static int cheat(void) { return opt_get(OPT_CHEAT_INFINITE_CANDY) != 0; }

int cook_have(const CookIng *g)
{
    int sp, n = 0;
    switch (g->kind) {
    case ING_ITEM: return inv_count(g->id);
    case ING_FISH: return g->id < FISH_COUNT ? fc.cooler[g->id] : 0;
    case ING_CUTE_FISH: case ING_HELL_FISH: case ING_ANY_FISH:
        for (sp = 0; sp < FISH_COUNT; sp++) if (fish_class_ok(g->kind, sp)) n += fc.cooler[sp];
        return n;
    default: return 0;
    }
}

int cook_can_make(int r)
{
    int i;
    if (r < 0 || r >= RECIPE_COUNT) return 0;
    if (cheat()) return 1;
    for (i = 0; i < 3; i++) {
        const CookIng *g = &cook_recipes[r].ing[i];
        if (g->kind != ING_NONE && cook_have(g) < g->qty) return 0;
    }
    return 1;
}

int cook_missing_cost(int r)
{
    int i, cost = 0;
    if (r < 0 || r >= RECIPE_COUNT) return -1;
    if (cheat()) return 0;
    for (i = 0; i < 3; i++) {
        const CookIng *g = &cook_recipes[r].ing[i];
        int miss = g->qty - cook_have(g);
        if (g->kind == ING_NONE || miss <= 0) continue;
        if (g->kind != ING_ITEM) return -1;            /* go fishing!       */
        if (doomo_items[g->id].price == 0 || !unlock_ok(UL_CAT_ITEM, 0, g->id)) return -1;
        cost += doomo_items[g->id].price * miss;
    }
    return cost;
}

int cook_buy_missing(int r)
{
    int i, cost = cook_missing_cost(r);
    if (cost < 0 || (uint32_t)cost > doomo_town.money) return 0;
    doomo_town.money -= (uint32_t)cost;
    for (i = 0; i < 3 && !cheat(); i++) {
        const CookIng *g = &cook_recipes[r].ing[i];
        int miss = g->qty - cook_have(g);
        if (g->kind == ING_ITEM && miss > 0) inv_add(g->id, miss);
    }
    return 1;
}

/* Uses up the cheapest fish of a class first (keep the rare ones!). */
static void take_fish(int kind, int n)
{
    while (n-- > 0) {
        int sp, best = -1;
        for (sp = 0; sp < FISH_COUNT; sp++)
            if (fc.cooler[sp] && fish_class_ok(kind, sp) &&
                (best < 0 || fish_info[sp].price < fish_info[best].price)) best = sp;
        if (best < 0) return;
        fc.cooler[best]--;
    }
}

void cook_take(int r)
{
    int i;
    if (r < 0 || r >= RECIPE_COUNT || cheat()) return;
    for (i = 0; i < 3; i++) {
        const CookIng *g = &cook_recipes[r].ing[i];
        switch (g->kind) {
        case ING_ITEM: inv_take(g->id, DOOMO_MIN(g->qty, inv_count(g->id))); break;
        case ING_FISH:
            if (g->id < FISH_COUNT) fc.cooler[g->id] = (uint8_t)DOOMO_MAX(0, fc.cooler[g->id] - g->qty);
            break;
        case ING_CUTE_FISH: case ING_HELL_FISH: case ING_ANY_FISH: take_fish(g->kind, g->qty); break;
        default: break;
        }
    }
}

/* ---- grading ------------------------------------------------------------------ */
int cook_grade(int r, int chop, int stir, int heat)
{
    int q = (DOOMO_CLAMP(chop, 0, 100) + DOOMO_CLAMP(stir, 0, 100) + 2 * DOOMO_CLAMP(heat, 0, 100)) / 4;
    int g = q < 40 ? GRADE_BURNT : q < 65 ? GRADE_OK : q < 88 ? GRADE_GREAT : GRADE_PERFECT;
    /* the Charred Mystery cannot be burnt: that's the point */
    if (r >= 0 && r < RECIPE_COUNT && cook_recipes[r].heat == HEAT_SCORCH && g == GRADE_BURNT)
        g = GRADE_OK;
    return g;
}

int cook_yield(int r, int grade)
{
    int y;
    if (r < 0 || r >= RECIPE_COUNT || grade <= GRADE_NONE) return 0;
    if (grade == GRADE_BURNT) return 1;
    y = cook_recipes[r].yield + (grade - GRADE_OK);
    return DOOMO_CLAMP(y, 1, 6);
}

int cook_result_item(int r, int grade)
{
    if (r < 0 || r >= RECIPE_COUNT) return ITEM_NONE;
    return grade == GRADE_BURNT ? ITEM_CHARRED : cook_recipes[r].item;
}

int cook_finish(int r, int grade)
{
    int n = cook_yield(r, grade), it = cook_result_item(r, grade);
    if (n <= 0) return 0;
    inv_add(it, n);
    if (fc.cooked < 65535) fc.cooked++;
    if (grade > fc.grade[r]) fc.grade[r] = (uint8_t)grade;
    if (grade == GRADE_PERFECT && fc.perfects < 65535) fc.perfects++;
    if (grade == GRADE_BURNT && fc.burnt < 65535) fc.burnt++;
    fc_check_unlocks();
    return n;
}

int cook_item_food(int item)
{
    int r;
    if (item <= ITEM_NONE || item >= ITEM_COUNT || doomo_items[item].category != ICAT_FOOD) return -1;
    if (item <= ITEM_COFFEE || (item >= ITEM_BLOOD_ORANGE && item <= ITEM_BONE_BROTH))
        return item - ITEM_CUPCAKE;                    /* the sim's own rule */
    for (r = 0; r < RECIPE_COUNT; r++)
        if (cook_recipes[r].item == item && cook_recipes[r].food >= 0) return cook_recipes[r].food;
    return -1;
}

/* ---- customers ------------------------------------------------------------------ */
static int seat_taken(int id, int n)
{
    int i;
    for (i = 0; i < n; i++) if (fc.cust[i] == id) return 1;
    return 0;
}

static int can_dine(int id)
{
    const DoomoResident *r = town_res(id);
    return id >= 0 && id < DOOMO_MAX_RESIDENTS && r && r->status == RES_ALIVE;
}

int cook_customers(void)
{
    int i, n = 0;
    if (fc.cust_day != doomo_town.day + 1) {
        DoomoRng r;
        int start, k;
        rng_seed(&r, doomo_town.seed ^ (doomo_town.day * 2654435761u) ^ 0xC00CB175u);
        for (i = 0; i < FC_CUSTOMERS; i++) fc.cust[i] = -1;
        /* regulars who are hanging out at the cafe right now come first */
        start = rng_range(&r, DOOMO_MAX_RESIDENTS);
        for (k = 0; k < DOOMO_MAX_RESIDENTS && n < FC_CUSTOMERS; k++) {
            int id = (start + k * 7) % DOOMO_MAX_RESIDENTS;
            if (can_dine(id) && doomo_town.res[id].activity == ACT_CAFE && rng_chance(&r, 50))
                fc.cust[n++] = (int16_t)id;
        }
        for (k = 0; k < 400 && n < FC_CUSTOMERS; k++) {
            int id = rng_range(&r, DOOMO_MAX_RESIDENTS);
            if (can_dine(id) && !seat_taken(id, n)) fc.cust[n++] = (int16_t)id;
        }
        fc.cust_day = doomo_town.day + 1;
        fc.cust_served = 0;
    }
    for (i = 0, n = 0; i < FC_CUSTOMERS; i++) if (fc.cust[i] >= 0) n++;
    return n;
}

int cook_customer(int i)
{
    if (i < 0 || i >= FC_CUSTOMERS) return -1;
    return fc.cust[i] >= 0 && town_res(fc.cust[i]) && town_res(fc.cust[i])->status != RES_EMPTY
           ? fc.cust[i] : -1;
}

int cook_customer_served(int i)
{
    return i >= 0 && i < FC_CUSTOMERS && ((fc.cust_served >> i) & 1);
}

static void copy_line(char *buf, int cap, const char *s)
{
    int i;
    if (!buf || cap <= 0) return;
    for (i = 0; i < cap - 1 && s && s[i]; i++) buf[i] = s[i];
    buf[i] = 0;
}

int cook_serve(int i, int item, char *buf, int cap, int *tip)
{
    static const char *const love[4] = {
        "%s?! You cooked this for ME?!",
        "Is this... %s?! My heart is doing flips!",
        "%s! Exactly how I like it! Marry me! (Kidding.)",
        "*happy wiggle* %s! You remembered!"
    };
    static const char *const ugh[3] = {
        "Oh. %s. *forces a smile* ...yum.",
        "%s?! I literally told you I hate %s!",
        "*pushes the %s away very politely*"
    };
    int id = cook_customer(i), fi, kind = SERVE_OK, before, money = 0, aligned, k;
    DoomoResident *r;
    const char *line;
    if (tip) *tip = 0;
    if (id < 0 || cook_customer_served(i) || item <= ITEM_NONE || item >= ITEM_COUNT) return SERVE_FAIL;
    r = town_res(id);
    if (!r || r->status != RES_ALIVE) return SERVE_FAIL;
    before = inv_count(item);
    if (before <= 0 && !cheat()) return SERVE_FAIL;
    fi = cook_item_food(item);
    aligned = (fi >= 0 && fi == item - ITEM_CUPCAKE);
    line = sim_feed(id, item);
    if (!cheat() && inv_count(item) >= before) {        /* it refused        */
        copy_line(buf, cap, line);
        return SERVE_FAIL;
    }
    copy_line(buf, cap, line);
    if (fi >= 0 && fi == r->like_food) kind = SERVE_FAV;
    else if (fi >= 0 && fi == r->hate_food) kind = SERVE_HATE;
    /* sim_feed already knows the old menu's flavours; a dish's flavour
     * (a Fish Ramen is Ramen) is this kitchen's own bonus */
    if (!aligned && kind != SERVE_OK && buf && cap > 0) {
        const char *food = doomo_food_names[fi];
        k = (int)((doomo_town.day + (uint32_t)id) % 4u);
        if (kind == SERVE_FAV) {
            r->stat[STAT_HAPPY] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_HAPPY] + 25);
            r->level = (uint8_t)DOOMO_MIN(99, r->level + 3);
            r->mood = MOOD_ECSTATIC;
            rel_adjust(id, DOOMO_PLAYER_ID, 5);
            doomo_snprintf(buf, cap, love[k], food);
        } else {
            r->stat[STAT_HAPPY] = (uint8_t)DOOMO_MAX(0, r->stat[STAT_HAPPY] - 12);
            k %= 3;
            if (k == 1) doomo_snprintf(buf, cap, ugh[1], food, food);
            else doomo_snprintf(buf, cap, ugh[k], food);
        }
    }
    switch (kind) {
    case SERVE_FAV: money = 30 + doomo_items[item].price / 2; break;
    case SERVE_HATE: money = 0; break;
    default: money = 8 + doomo_items[item].price / 6; break;
    }
    if (money > 0) {
        doomo_town.money += (uint32_t)money;
        progress_add(PROG_MONEY_EARNED, (uint32_t)money);
    }
    if (tip) *tip = money;
    fc.cust_served = (uint8_t)(fc.cust_served | (1u << i));
    if (fc.served < 65535) fc.served++;
    if (kind == SERVE_FAV) fc.feats |= FEAT_FAV_SERVED;
    {
        int all = 1;
        for (k = 0; k < FC_CUSTOMERS; k++)
            if (cook_customer(k) >= 0 && !cook_customer_served(k)) all = 0;
        if (all) fc.feats |= FEAT_FULL_HOUSE;
    }
    fc_check_unlocks();
    return kind;
}
