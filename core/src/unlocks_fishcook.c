/* unlocks_fishcook.c - the Lake of Fire + Brimstone Bites unlockables
 * (ids 960..1023, owner: fishcook; the roster team's unused range).
 *
 *   960..989   Fishdex: one bit per species, granted on the first catch
 *   990..992   rods (two by fish caught, one for landing a legend)
 *   993..1008  recipes. Recipes that make a NEW dish are UL_CAT_ITEM
 *              entries: the cafe and shops stock that dish once you can
 *              cook it. Recipes for existing foods are UL_CAT_MISC.
 *   1009..1020 trophies
 * Rules on PROG_FISH_CAUGHT are automatic; the rest are granted from code
 * (fc_check_unlocks in fishcook.c, need == 0 here). */
#include "fishcook_int.h"

#define FL_DEX    0xF1      /* UL_CAT_MISC list ids of this module        */
#define FL_ROD    0xF2
#define FL_RECIPE 0xC0
#define FL_TROPHY 0xC1

#define DEX(sp, name, how) { FC_UL_DEX(sp), UL_CAT_MISC, FL_DEX, sp, "Fishdex: " name, how, 0, 0 }
#define ROD(id, k, name, how, prog, need) { id, UL_CAT_MISC, FL_ROD, k, name, how, prog, need }
#define DISH(k, item, name, how, prog, need) \
    { FC_UL_RECIPE0 + (k), UL_CAT_ITEM, 0, item, "Recipe: " name, how, prog, need }
#define RCP(k, name, how) { FC_UL_RECIPE0 + (k), UL_CAT_MISC, FL_RECIPE, k, "Recipe: " name, how, 0, 0 }
#define TROPHY(id, name, how, prog, need) \
    { id, UL_CAT_MISC, FL_TROPHY, (id) - FC_UL_FIRST_BITE, name, how, prog, need }

const DoomoUnlock doomo_unlocks_fishcook[] = {
    /* ---- Fishdex ---- */
    DEX(FISH_BUBBLEGUM_GUPPY, "Bubblegum Guppy", "Catch one at the lake, any time"),
    DEX(FISH_MARSHMALLOW_MINNOW, "Marshmallow Minnow", "Catch one in the daytime"),
    DEX(FISH_SPRINKLE_TROUT, "Sprinkle Trout", "Catch one in the daytime"),
    DEX(FISH_HONEY_BASS, "Honey Bass", "Catch one in the daytime"),
    DEX(FISH_TEACUP_TETRA, "Teacup Tetra", "Catch one at dawn or dusk"),
    DEX(FISH_PLUSH_KOI, "Plush Koi", "Uncommon, daytime"),
    DEX(FISH_COTTON_CATFISH, "Cotton Candy Catfish", "Uncommon, at night"),
    DEX(FISH_LULLABY_LOACH, "Lullaby Loach", "Uncommon, at night"),
    DEX(FISH_MOCHI_PUFFER, "Mochi Puffer", "Rare, any time"),
    DEX(FISH_STARLIGHT_SUNFISH, "Starlight Sunfish", "Rare, at night"),
    DEX(FISH_RAINBOW_RIBBON, "Rainbow Ribbonfish", "Rare, daytime"),
    DEX(FISH_ANGEL_ANGELFISH, "Angel Angelfish", "Epic, daytime"),
    DEX(FISH_PUDDINGFIN, "Princess Puddingfin", "Legendary, daytime. Try cupcake bait"),
    DEX(FISH_BRIMSTONE_BREAM, "Brimstone Bream", "Catch one at the lake, any time"),
    DEX(FISH_SCREAMING_CARP, "Screaming Carp", "Catch one at the lake, any time"),
    DEX(FISH_GOTH_GUPPY, "Goth Guppy", "Catch one at night"),
    DEX(FISH_SKULL_SARDINE, "Skull Sardine", "Catch one at night"),
    DEX(FISH_SULFUR_SALMON, "Sulfur Salmon", "Catch one at dawn or dusk"),
    DEX(FISH_LAVA_EEL, "Lava Eel", "Uncommon, any time"),
    DEX(FISH_VAPE_PIKE, "Vape Pike", "Uncommon, at night"),
    DEX(FISH_CINDER_SNAPPER, "Cinder Snapper", "Uncommon, daytime"),
    DEX(FISH_CURSED_COD, "Cursed Cod", "Uncommon, at night"),
    DEX(FISH_HELLMOUTH_BASS, "Hellmouth Bass", "Rare, any time"),
    DEX(FISH_BONE_BARRACUDA, "Bone Barracuda", "Rare, at night"),
    DEX(FISH_DEMON_DISCUS, "Demon Discus", "Rare, at dawn or dusk"),
    DEX(FISH_GATE_GROUPER, "Gatekeeper Grouper", "Epic, at night"),
    DEX(FISH_MAMA_MAGMAW, "Mama Magmaw", "Legendary. Try hell pepper bait"),
    DEX(FISH_SOGGY_BOOT, "Soggy Boot", "It's junk. Somebody lost it"),
    DEX(FISH_GHOST_CAN, "Ghost Tin Can", "Junk that rattles at night"),
    DEX(FISH_VAPULA_SOCK, "Lord Vapula's Lost Sock", "Legendary junk, at night"),
    /* ---- rods ---- */
    ROD(FC_UL_ROD_CANDY, ROD_CANDY, "Candy Cane Rod", "Catch 15 fish", PROG_FISH_CAUGHT, 15),
    ROD(FC_UL_ROD_HELL, ROD_HELLSTEEL, "Hellsteel Rod", "Catch 50 fish", PROG_FISH_CAUGHT, 50),
    ROD(FC_UL_ROD_GOLD, ROD_GOLDEN, "Golden Paw Rod", "Land a legendary catch", 0, 0),
    /* ---- recipes ---- */
    RCP(0, "Candy Eyeballs", "Cook 3 dishes at Brimstone Bites"),
    DISH(1, ITEM_TEMPURA_ROLL, "Tempura Roll", "Catch 3 fish", PROG_FISH_CAUGHT, 3),
    DISH(2, ITEM_TAKOYAKI, "Takoyaki", "Catch 10 fish", PROG_FISH_CAUGHT, 10),
    DISH(3, ITEM_FISH_RAMEN, "Fish Ramen", "Cook Bone Broth", 0, 0),
    RCP(4, "Cursed Cod Chowder", "Catch a Cursed Cod"),
    RCP(5, "Lava Eel Steak", "Catch a Lava Eel"),
    DISH(6, ITEM_CURRY, "Hellfire Curry", "Cook 5 dishes", 0, 0),
    DISH(7, ITEM_PICKLE_PIZZA, "Pickle Pizza", "Cook 8 dishes", 0, 0),
    DISH(8, ITEM_GRAVE_CAKE, "Grave Dirt Cake", "Cook 12 dishes", 0, 0),
    DISH(9, ITEM_EYEBALL_JELLY, "Eyeball Jelly", "Catch a Screaming Carp", 0, 0),
    DISH(10, ITEM_LAVA_CAKE, "Molten Lava Cake", "Cook 2 PERFECT dishes", 0, 0),
    RCP(11, "Angel Cake", "Cook 5 PERFECT dishes"),
    DISH(12, ITEM_CHARRED, "Charred Mystery", "Burn a dish (oops)", 0, 0),
    RCP(13, "Marshmallow Cupcakes", "Catch a Marshmallow Minnow"),
    RCP(14, "Starlight Pudding", "Catch a Starlight Sunfish"),
    DISH(15, ITEM_SOCK_STEW, "Sock Stew", "Fish up Lord Vapula's Lost Sock", 0, 0),
    /* ---- trophies ---- */
    TROPHY(FC_UL_FIRST_BITE, "First Bite", "Catch your first fish", PROG_FISH_CAUGHT, 1),
    TROPHY(FC_UL_MASTER, "Master Angler", "Complete the Fishdex", 0, 0),
    TROPHY(FC_UL_WHOPPER, "Whopper!", "Land a catch of 3 m or more", 0, 0),
    TROPHY(FC_UL_RELEASE, "Catch & Release", "Let 10 fish go", 0, 0),
    TROPHY(FC_UL_MONGER, "Fishmonger", "Sell 30 fish", 0, 0),
    TROPHY(FC_UL_NIGHT_OWL, "Night Owl Angler", "Catch every night-only fish", 0, 0),
    TROPHY(FC_UL_LEGEND, "Lake Legend", "Catch all three legends", 0, 0),
    TROPHY(FC_UL_GOLD_STAR, "Gold Star Chef", "Cook a PERFECT dish", 0, 0),
    TROPHY(FC_UL_HEAD_CHEF, "Head Chef", "PERFECT every recipe", 0, 0),
    TROPHY(FC_UL_DISASTER, "Kitchen Disaster", "Burn 3 dishes", 0, 0),
    TROPHY(FC_UL_REGULAR, "Regular's Favourite", "Serve a customer their favourite", 0, 0),
    TROPHY(FC_UL_FULL_HOUSE, "Full House", "Serve every customer in one day", 0, 0),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};
