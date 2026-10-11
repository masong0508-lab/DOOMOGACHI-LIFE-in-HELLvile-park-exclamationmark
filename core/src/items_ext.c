/* items_ext.c - rows of doomo_items[] appended by the expansions (owner:
 * fishcook): the Brimstone Bites ingredients and the dishes cooked in
 * Granny Hopscotch's kitchen (cook*.c). Ids: items.h, after ITEM_GATE_SIGIL.
 *
 * A C array cannot be split across files, so items.c #includes this file
 * INSIDE its doomo_items[] initializer (one line, right after the last
 * row). Compiled on its own (every .c file in core/src is a core source) it is an
 * empty unit: the rows are only seen when doomo/items.h has already been
 * included, i.e. from inside items.c.
 *
 * The first four rows line up with doomo_food_names[44..47] (sim_feed and
 * the UIs map an item to a favourite food as item - ITEM_CUPCAKE), so the
 * citizens who love or hate Blood Oranges, Pickles, Brussels Sprouts or
 * Bone Broth react to them. Dishes are food (sim_feed accepts them); the
 * cafe sells a dish once its recipe is unlocked (unlocks_fishcook.c). */
#ifdef DOOMO_ITEMS_H
/*  name, desc, price, category,
 *  hunger, happy, hygiene, energy, health, sanity, corruption, holy, icon */
    /* ---- ingredients that are also snacks (food 44..47) ---- */
    , { "Blood Orange", "Juicy, a bit dramatic. Bleeds.", 16, ICAT_FOOD,
        12, 6, 0, 6, 4, 0, 1, 0, ICON_FOOD }
    , { "Pickle Jar", "Crunchy, sour, judging you.", 14, ICAT_FOOD,
        10, 3, 0, 2, 2, -1, 0, 0, ICON_FOOD }
    , { "Brussels Sprouts", "Tiny cabbages of pure suspicion.", 10, ICAT_FOOD,
        14, -6, 0, 4, 12, 0, 0, 0, ICON_FOOD }
    , { "Bone Broth", "Simmered for hours. Hums softly.", 40, ICAT_FOOD,
        34, 6, 0, 10, 12, 6, 2, 0, ICON_DROP }
    /* ---- baking basics (Granny's pantry) ---- */
    , { "Cloud Flour", "Floaty flour. For baking, dear!", 8, ICAT_FOOD,
        2, -3, -2, 0, 0, 0, 0, 0, ICON_CLOUD }
    , { "Pixie Sugar", "Sparkles. Makes spoons giggle.", 8, ICAT_FOOD,
        3, 8, 0, 12, -3, 0, 0, 0, ICON_SPARK }
    , { "Speckled Egg", "Laid by a very proud hen.", 6, ICAT_FOOD,
        6, -2, 0, 2, 2, 0, 0, 0, ICON_EGG }
    , { "Charred Mystery", "Was dinner. Now a lifestyle.", 3, ICAT_FOOD,
        4, -8, -2, 0, -4, -2, 3, 0, ICON_FIRE }
    /* ---- dishes ---- */
    , { "Fish Ramen", "Slurpy noodles in lake broth.", 70, ICAT_FOOD,
        50, 14, 0, 12, 6, 6, 0, 0, ICON_FOOD }
    , { "Tempura Roll", "Crispy fish roll. Rolls itself.", 65, ICAT_FOOD,
        38, 14, 0, 8, 6, 4, 0, 0, ICON_FOOD }
    , { "Takoyaki", "Octopus-ish balls. Don't ask.", 55, ICAT_FOOD,
        32, 14, 0, 8, 2, 4, 1, 0, ICON_FOOD }
    , { "Fluffy Pancakes", "A wobbly stack, butter crown.", 50, ICAT_FOOD,
        36, 16, 0, 10, 2, 6, 0, 0, ICON_FOOD }
    , { "Molten Lava Cake", "Erupts when poked. Worth it.", 85, ICAT_FOOD,
        30, 22, 0, 10, 0, 4, 4, 0, ICON_FIRE }
    , { "Hellfire Curry", "Spicy enough to see ancestors.", 75, ICAT_FOOD,
        50, 8, 0, 20, 6, -4, 5, 0, ICON_FIRE }
    , { "Pickle Pizza", "Controversial. Delicious. Both.", 70, ICAT_FOOD,
        46, 16, 0, 8, 0, 4, 1, 0, ICON_FOOD }
    , { "Grave Dirt Cake", "Cocoa soil, gummy worms, RIP.", 65, ICAT_FOOD,
        28, 20, 0, 6, 0, 2, 3, 0, ICON_SKULL }
    , { "Eyeball Jelly", "Wobbles. Watches. Grape-y.", 45, ICAT_FOOD,
        20, 16, 0, 6, 2, -2, 3, 0, ICON_FOOD }
    , { "Sock Stew", "A demon lord's sock, slow-cooked.", 250, ICAT_FOOD,
        60, 30, -10, 30, 20, 20, -10, 10, ICON_STAR }
#else
/* Compiled on its own: nothing here (see above). */
typedef int doomo_items_ext_unit;
#endif
