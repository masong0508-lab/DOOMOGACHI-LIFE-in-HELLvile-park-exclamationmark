/* fish_data.c - the Lake of Fire's fish, rods and bait, and the rules
 * of what bites when (owner: fishcook). See fishcook_int.h.
 *
 * 30 species: 13 cute ones (Hillvile's side of the lake), 14 hellish
 * ones, two pieces of junk and Lord Vapula's Lost Sock. Rarity sets the
 * bite weight; time of day gates the night / twilight / day species;
 * bait (a food item used up per cast) tilts the odds. Sizes are rolled
 * per catch (a rare "whopper" overshoots the species' max) and the price
 * grows with size. */
#include "fishcook_int.h"

#define P(p)  PAL_PAINT(PAINT_##p, 1)
#define PL(p) PAL_PAINT(PAINT_##p, 2)
#define PD(p) PAL_PAINT(PAINT_##p, 0)
#define R(r, s) PAL_RAMP(RAMP_##r, s)

const char *const fish_rarity_names[FR_COUNT] = {
    "Junk", "Common", "Uncommon", "Rare", "Epic", "Legendary"
};
const char *const fish_time_names[4] = { "Any time", "Daytime", "Night", "Dawn & dusk" };

/*  name, desc,
 *  rarity, time, hell, shape, size min..max (cm), price, strength, style,
 *  body, belly, fin, pattern, eye, extras */
const FishInfo fish_info[FISH_COUNT] = {
    /* ---- cute ---- */
    { "Bubblegum Guppy", "Blows a bubble when nervous. Always nervous.",
      FR_COMMON, FT_ANY, 0, FSH_ROUND, 3, 8, 8, 1, FSTY_DARTER,
      P(BUBBLEGUM), PL(BUBBLEGUM), P(LAVENDER), FPAT_NONE, FEYE_CUTE, FF_BUBBLE | FF_BLUSH },
    { "Marshmallow Minnow", "Toast it lightly. It insists.",
      FR_COMMON, FT_DAY, 0, FSH_ROUND, 2, 6, 6, 1, FSTY_STEADY,
      PL(WHITE), P(CARAMEL), PL(PEACH), FPAT_NONE, FEYE_CUTE, FF_BLUSH },
    { "Sprinkle Trout", "Every sprinkle is a tiny wish. Mostly for snacks.",
      FR_COMMON, FT_DAY, 0, FSH_OVAL, 20, 45, 18, 3, FSTY_STEADY,
      PL(PEACH), PL(WHITE), P(BUBBLEGUM), FPAT_SPRINKLES, FEYE_CUTE, 0 },
    { "Honey Bass", "Smells like pancakes. Bees follow it home.",
      FR_COMMON, FT_DAY, 0, FSH_OVAL, 25, 50, 20, 4, FSTY_STEADY,
      P(HONEY), PL(LEMON), P(CARAMEL), FPAT_STRIPES, FEYE_CUTE, 0 },
    { "Teacup Tetra", "Fits in a teacup. Refuses to ever leave it.",
      FR_COMMON, FT_TWILIGHT, 0, FSH_OVAL, 2, 5, 10, 1, FSTY_DARTER,
      P(CYAN), PL(SKY), P(MINT), FPAT_NONE, FEYE_CUTE, FF_BLUSH },
    { "Plush Koi", "Hand-stitched by lake spirits. Machine washable.",
      FR_UNCOMMON, FT_DAY, 0, FSH_OVAL, 30, 70, 45, 4, FSTY_STEADY,
      PL(WHITE), PL(WHITE), P(TANGERINE), FPAT_PATCHES, FEYE_BUTTON, FF_STITCH },
    { "Cotton Candy Catfish", "Melts in the rain, so it only swims at night.",
      FR_UNCOMMON, FT_NIGHT, 0, FSH_OVAL, 25, 60, 40, 5, FSTY_THRASHER,
      PL(SKY), PL(BUBBLEGUM), P(LAVENDER), FPAT_NONE, FEYE_CUTE, FF_WHISKERS | FF_BLUSH },
    { "Lullaby Loach", "Hums you to sleep mid-fight. Rude. Adorable.",
      FR_UNCOMMON, FT_NIGHT, 0, FSH_LONG, 10, 25, 30, 2, FSTY_SLEEPY,
      P(LAVENDER), PL(LAVENDER), P(GRAPE), FPAT_SPOTS, FEYE_SLEEPY, FF_BLUSH },
    { "Mochi Puffer", "Squishy. Do not squeeze it. (Squeeze it.)",
      FR_RARE, FT_ANY, 0, FSH_ROUND, 8, 20, 80, 3, FSTY_JUMPER,
      PL(PEACH), PL(WHITE), P(ROSE), FPAT_NONE, FEYE_CUTE, FF_BLUSH },
    { "Starlight Sunfish", "Glows brighter whenever you make a wish.",
      FR_RARE, FT_NIGHT, 0, FSH_FLAT, 15, 35, 90, 4, FSTY_DARTER,
      P(NAVY), P(COBALT), P(LEMON), FPAT_STARS, FEYE_CUTE, FF_GLOW },
    { "Rainbow Ribbonfish", "Ties itself in a bow to look nice for you.",
      FR_RARE, FT_DAY, 0, FSH_LONG, 60, 140, 110, 5, FSTY_JUMPER,
      P(CHERRY), PL(WHITE), P(SKY), FPAT_RAINBOW, FEYE_CUTE, 0 },
    { "Angel Angelfish", "Forgives you for catching it. Immediately.",
      FR_EPIC, FT_DAY, 0, FSH_FLAT, 15, 30, 220, 6, FSTY_JUMPER,
      PL(WHITE), PL(LEMON), PL(SKY), FPAT_NONE, FEYE_CUTE, FF_HALO | FF_WINGS | FF_GLOW },
    { "Princess Puddingfin", "Royalty. Wobbles regally. Demands a spoon.",
      FR_LEGEND, FT_DAY, 0, FSH_ROUND, 40, 60, 600, 8, FSTY_THRASHER,
      PL(HONEY), PL(LEMON), P(CHOCOLATE), FPAT_NONE, FEYE_CUTE, FF_CROWN | FF_BLUSH | FF_GLOW },
    /* ---- hellish ---- */
    { "Brimstone Bream", "Warm to the touch. Hot to the bite.",
      FR_COMMON, FT_ANY, 1, FSH_OVAL, 15, 35, 16, 3, FSTY_STEADY,
      P(RUST), P(TANGERINE), P(CHERRY), FPAT_NONE, FEYE_ANGRY, 0 },
    { "Screaming Carp", "Has screamed since 1987. Nobody asks why.",
      FR_COMMON, FT_ANY, 1, FSH_OVAL, 30, 60, 22, 4, FSTY_THRASHER,
      R(STONE, 4), R(STONE, 6), P(GRAPE), FPAT_SPOTS, FEYE_CUTE, FF_SCREAM },
    { "Goth Guppy", "It's not a phase, Mom. It's a lifestyle.",
      FR_COMMON, FT_NIGHT, 1, FSH_ROUND, 3, 8, 10, 1, FSTY_DARTER,
      P(CHARCOAL), PL(CHARCOAL), P(MAGENTA), FPAT_NONE, FEYE_GOTH, 0 },
    { "Skull Sardine", "Ninety percent bones, ten percent attitude.",
      FR_COMMON, FT_NIGHT, 1, FSH_PIKE, 5, 15, 9, 2, FSTY_DARTER,
      P(CHARCOAL), P(BONE), PL(BONE), FPAT_BONES, FEYE_HOLLOW, 0 },
    { "Sulfur Salmon", "Swims up lava rapids. Smells of boiled eggs.",
      FR_COMMON, FT_TWILIGHT, 1, FSH_OVAL, 40, 80, 26, 5, FSTY_JUMPER,
      PD(LEMON), PL(LEMON), P(CHARTREUSE), FPAT_STRIPES, FEYE_SAD, 0 },
    { "Lava Eel", "Do not hug. (It really, really wants a hug.)",
      FR_UNCOMMON, FT_ANY, 1, FSH_LONG, 60, 180, 55, 6, FSTY_THRASHER,
      R(LAVA, 3), R(LAVA, 5), R(BLOOD, 4), FPAT_CRACKS, FEYE_CUTE, FF_GLOW },
    { "Vape Pike", "Blows Unholy Grape rings. A terrible influence.",
      FR_UNCOMMON, FT_NIGHT, 1, FSH_PIKE, 50, 110, 60, 6, FSTY_DARTER,
      P(VAPE_PURPLE), PL(LAVENDER), P(GRAPE), FPAT_STRIPES, FEYE_SLEEPY, FF_VAPE },
    { "Cinder Snapper", "Snaps. Crackles. Mostly snaps.",
      FR_UNCOMMON, FT_DAY, 1, FSH_OVAL, 25, 55, 50, 5, FSTY_JUMPER,
      P(CHERRY), P(TANGERINE), P(HELLFIRE), FPAT_NONE, FEYE_ANGRY, FF_FLAMES | FF_TEETH },
    { "Cursed Cod", "All cod are cursed. This one is honest about it.",
      FR_UNCOMMON, FT_NIGHT, 1, FSH_OVAL, 30, 70, 48, 5, FSTY_STEADY,
      R(SICK, 4), R(SICK, 6), P(GRAPE), FPAT_SPOTS, FEYE_SAD, FF_GLOW },
    { "Hellmouth Bass", "Eats bait, hooks, rods and the odd angler.",
      FR_RARE, FT_ANY, 1, FSH_BIG, 40, 90, 120, 7, FSTY_THRASHER,
      P(BLOOD), PL(ROSE), P(CHARCOAL), FPAT_NONE, FEYE_ANGRY, FF_TEETH },
    { "Bone Barracuda", "Lost all its meat in a bet. Still very fast.",
      FR_RARE, FT_NIGHT, 1, FSH_PIKE, 70, 150, 140, 7, FSTY_DARTER,
      P(CHARCOAL), P(BONE), PL(BONE), FPAT_BONES, FEYE_HOLLOW, FF_TEETH },
    { "Demon Discus", "Grants three wishes. All of them bad.",
      FR_RARE, FT_TWILIGHT, 1, FSH_FLAT, 20, 40, 130, 6, FSTY_JUMPER,
      P(MAGENTA), PL(ROSE), P(BLOOD), FPAT_STRIPES, FEYE_ANGRY, FF_HORNS },
    { "Gatekeeper Grouper", "Guards the sealed gate. Naps on duty.",
      FR_EPIC, FT_NIGHT, 1, FSH_BIG, 90, 200, 300, 8, FSTY_STEADY,
      R(STEEL, 3), R(STEEL, 5), P(RUST), FPAT_SPOTS, FEYE_SLEEPY, FF_HORNS | FF_CHAIN },
    { "Mama Magmaw", "The lake's oldest resident. Pays no rent.",
      FR_LEGEND, FT_ANY, 1, FSH_BIG, 220, 480, 800, 9, FSTY_THRASHER,
      R(LAVA, 2), R(LAVA, 5), R(BLOOD, 3), FPAT_CRACKS, FEYE_CUTE, FF_FLAMES | FF_GLOW | FF_BLUSH },
    /* ---- junk ---- */
    { "Soggy Boot", "Mackerel's other boot. He'll want it back.",
      FR_JUNK, FT_ANY, 0, FSH_BOOT, 25, 30, 2, 2, FSTY_STEADY,
      P(CHOCOLATE), PD(CHOCOLATE), PL(WHITE), FPAT_NONE, FEYE_NONE, 0 },
    { "Ghost Tin Can", "Rattles at midnight. Contains one ghost bean.",
      FR_JUNK, FT_NIGHT, 1, FSH_CAN, 8, 12, 3, 1, FSTY_SLEEPY,
      P(SILVER), P(ECTOPLASM), P(CHERRY), FPAT_NONE, FEYE_NONE, FF_GLOW },
    { "Lord Vapula's Lost Sock", "Smells of brimstone and regret. He looked EVERYWHERE.",
      FR_LEGEND, FT_NIGHT, 1, FSH_SOCK, 28, 31, 666, 9, FSTY_THRASHER,
      P(VAPE_PURPLE), PL(WHITE), P(HELLFIRE), FPAT_STRIPES, FEYE_NONE, FF_FLAMES | FF_VAPE }
};

/* ---- rods ---------------------------------------------------------------- */
const FishRod fish_rods[ROD_COUNT] = {
    { "Twig Rod", "A stick, a string, a dream.", ACT_ALWAYS, 0, 10,
      R(WOOD, 3), R(WOOD, 5) },
    { "Candy Cane Rod", "Bendy and minty. Harder to snap.", FC_UL_ROD_CANDY, 18, 11,
      P(CHERRY), PL(WHITE) },
    { "Hellsteel Rod", "Forged in the Ninth Cloud. Reels fast.", FC_UL_ROD_HELL, 10, 15,
      R(STEEL, 3), R(LAVA, 5) },
    { "Golden Paw Rod", "A lucky paw on a golden pole.", FC_UL_ROD_GOLD, 30, 14,
      PAL_UI_GOLD, PL(BUBBLEGUM) }
};

int fish_rod_ok(int rod)
{
    if (rod < 0 || rod >= ROD_COUNT) return 0;
    return fish_rods[rod].unlock == ACT_ALWAYS || unlock_has(fish_rods[rod].unlock);
}

/* ---- bait ---------------------------------------------------------------- */
const FishBait fish_baits[BAIT_COUNT] = {
    { ITEM_NONE, 2, 2, 0, 0, 0, "Just a bare hook. Brave." },
    { ITEM_CUPCAKE, 6, 1, 0, 0, 20, "Cute fish go wild for it" },
    { ITEM_HELL_PEPPERS, 1, 6, 0, 10, 20, "Hellish fish can't resist" },
    { ITEM_CANDY_EYEBALLS, 2, 2, 120, 0, 0, "Rare fish take a peek" },
    { ITEM_MEAT_SLAB, 2, 3, 30, 70, 0, "Big fish, big appetite" },
    { ITEM_PIXIE_SUGAR, 3, 3, 0, 0, 55, "Bites come twice as fast" }
};

int fish_bait_ok(int bait)
{
    if (bait <= BAIT_NONE || bait >= BAIT_COUNT) return bait == BAIT_NONE;
    if (opt_get(OPT_CHEAT_INFINITE_CANDY)) return 1;
    return inv_count(fish_baits[bait].item) > 0;
}

/* ---- when things bite ------------------------------------------------------ */
int fish_hour_kind(int hour)
{
    if (hour >= 21 || hour < 5) return FT_NIGHT;
    if (hour <= 6 || hour >= 18) return FT_TWILIGHT;
    return FT_DAY;
}

int fish_time_ok(int sp, int hour)
{
    int k = fish_hour_kind(hour);
    if (sp < 0 || sp >= FISH_COUNT) return 0;
    switch (fish_info[sp].time) {
    case FT_DAY: return k != FT_NIGHT;          /* day fish linger at dusk */
    case FT_NIGHT: return k == FT_NIGHT;
    case FT_TWILIGHT: return k == FT_TWILIGHT;
    default: return 1;
    }
}

static const uint8_t rarity_weight[FR_COUNT] = { 22, 100, 40, 14, 5, 2 };

int fish_weight(int sp, int hour, int bait)
{
    const FishInfo *f;
    const FishBait *b;
    int w;
    if (sp < 0 || sp >= FISH_COUNT || !fish_time_ok(sp, hour)) return 0;
    if (bait < 0 || bait >= BAIT_COUNT) bait = BAIT_NONE;
    f = &fish_info[sp];
    b = &fish_baits[bait];
    w = rarity_weight[f->rarity] * 8;
    /* the legendary sock only turns up once its owner is gone (story) */
    if (sp == FISH_VAPULA_SOCK) {
        w /= 2;
        if (doomo_town.mode == TOWN_STORY && doomo_town.story.chapter < 4) return 0;
    }
    if (f->rarity == FR_JUNK) return w;          /* bait doesn't matter    */
    w = w * (f->hell ? b->hell : b->cute) / 2;
    if (f->rarity >= FR_RARE) w = w * (100 + b->rare) / 100;
    return w;
}

int fish_roll_species(DoomoRng *r, int hour, int bait)
{
    int sp, total = 0, pick;
    for (sp = 0; sp < FISH_COUNT; sp++) total += fish_weight(sp, hour, bait);
    if (total <= 0) return FISH_BUBBLEGUM_GUPPY;
    pick = (int)(rng_next(r) % (uint32_t)total);
    for (sp = 0; sp < FISH_COUNT; sp++) {
        pick -= fish_weight(sp, hour, bait);
        if (pick < 0) return sp;
    }
    return FISH_BUBBLEGUM_GUPPY;
}

int fish_roll_size(DoomoRng *r, int sp, int bait)
{
    const FishInfo *f = &fish_info[DOOMO_CLAMP(sp, 0, FISH_COUNT - 1)];
    int span = f->size_max - f->size_min;
    int u = (rng_range(r, 1001) + rng_range(r, 1001)) / 2;       /* 0..1000 */
    int bias = (bait > 0 && bait < BAIT_COUNT) ? fish_baits[bait].size : 0;
    u += (1000 - u) * bias / 200;
    if (f->rarity != FR_JUNK && rng_range(r, 100) < 2)            /* whopper! */
        return f->size_max + 1 + span / 8 + rng_range(r, span / 6 + 1);
    return f->size_min + span * u / 1000;
}

int fish_value(int sp, int size)
{
    const FishInfo *f;
    int span, v;
    if (sp < 0 || sp >= FISH_COUNT) return 0;
    f = &fish_info[sp];
    span = f->size_max - f->size_min + 1;
    v = f->price * (60 + 80 * (size - f->size_min) / span) / 100;
    return DOOMO_MAX(v, 1);
}

/* ---- the Fishdex, the cooler ------------------------------------------------ */
int fish_dex_has(int sp)
{
    return sp >= 0 && sp < FISH_COUNT && unlock_has(FC_UL_DEX(sp));
}

int fish_dex_count(void)
{
    int sp, n = 0;
    for (sp = 0; sp < FISH_COUNT; sp++) n += fish_dex_has(sp);
    return n;
}

int fish_register_catch(int sp, int size)
{
    int flags = 0;
    if (sp < 0 || sp >= FISH_COUNT) return 0;
    if (!fish_dex_has(sp)) flags |= CATCH_NEW;
    if (size > fc.record[sp]) {
        if (fc.record[sp]) flags |= CATCH_RECORD;
        fc.record[sp] = (uint16_t)DOOMO_MIN(size, 65535);
    }
    if (fc.caught[sp] < 255) fc.caught[sp]++;
    unlock_grant(FC_UL_DEX(sp));
    progress_add(PROG_FISH_CAUGHT, 1);
    fc_check_unlocks();
    return flags;
}

void fish_keep(int sp)
{
    if (sp >= 0 && sp < FISH_COUNT && fc.cooler[sp] < 99) fc.cooler[sp]++;
}

static void earn(int h)
{
    if (h <= 0) return;
    doomo_town.money += (uint32_t)h;
    progress_add(PROG_MONEY_EARNED, (uint32_t)h);
}

int fish_sell(int sp, int size)
{
    int v = fish_value(sp, size);
    if (sp < 0 || sp >= FISH_COUNT) return 0;
    earn(v);
    if (fc.sold < 65535) fc.sold++;
    fc_check_unlocks();
    return v;
}

void fish_release(int sp)
{
    if (sp < 0 || sp >= FISH_COUNT) return;
    if (fc.released < 65535) fc.released++;
    fc_check_unlocks();
}

int fish_sell_cooler(int sp, int n)
{
    int v, k;
    const FishInfo *f;
    if (sp < 0 || sp >= FISH_COUNT || n <= 0) return 0;
    f = &fish_info[sp];
    n = DOOMO_MIN(n, fc.cooler[sp]);
    /* kept fish sell at the price of an average one */
    v = fish_value(sp, (f->size_min + f->size_max) / 2);
    for (k = 0; k < n; k++) {
        fc.cooler[sp]--;
        if (fc.sold < 65535) fc.sold++;
    }
    earn(v * n);
    fc_check_unlocks();
    return v * n;
}

int fish_cooler_total(void)
{
    int sp, n = 0;
    for (sp = 0; sp < FISH_COUNT; sp++) n += fc.cooler[sp];
    return n;
}
