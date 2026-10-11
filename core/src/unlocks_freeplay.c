/* unlocks_freeplay.c - the free play unlockables (ids 736..831, owner:
 * freeplay).
 *
 *   740..764  island achievements (UL_CAT_MISC, list 'F'), granted from
 *             code by the free play screens via fp_achieve()
 *   770..787  creator parts: most come bundled with an achievement (see
 *             bonus[] below), two follow PROG_* counters
 *   790..791  paints
 *   795..800  shop stock (UL_CAT_ITEM: the shop only sells them once owned)
 *   805       a window skin
 */
#include "fp_int.h"

#define FPL 'F'
#define ACH(id, idx, name, how) { id, UL_CAT_MISC, FPL, idx, name, how, 0, 0 }
#define PART(id, list, idx, name, how, prog, need) { id, UL_CAT_PART, list, idx, name, how, prog, need }
#define PAINT(id, idx, name, how, prog, need) { id, UL_CAT_PAINT, 0, idx, name, how, prog, need }
#define ITEM(id, idx, name, how, prog, need) { id, UL_CAT_ITEM, 0, idx, name, how, prog, need }
#define SKIN(id, opt, val, name, how, prog, need) { id, UL_CAT_SKIN, opt, val, name, how, prog, need }

enum {
    UP_BERET = 770, UP_BANDANA, UP_SCARF, UP_BACKPACK, UP_ELF_EARS, UP_PLAID, UP_PIGTAILS,
    UP_TONGUE, UP_LASHES, UP_CLOGS, UP_CARGO, UP_VEST, UP_MOCHI, UP_HOOK_NOSE, UP_GOATEE,
    UP_RAISED_BROWS, UP_CAP, UP_EARRINGS
};

const DoomoUnlock doomo_unlocks_freeplay[] = {
    /* ---- island achievements ---- */
    ACH(UL_FP_LANDLORD, 0, "Landlord", "Collect rent at Town Hall"),
    ACH(UL_FP_NAMER, 1, "Island Namer", "Give your island a name"),
    ACH(UL_FP_FIRST_CITIZEN, 2, "Hello, Neighbour!", "Create a citizen on a free play island"),
    ACH(UL_FP_PICNIC, 3, "Picnic Planner", "Throw a picnic in Hellvile Park"),
    ACH(UL_FP_SHOPAHOLIC, 4, "Shopaholic", "Spend 500 H$ in one go"),
    ACH(UL_FP_BLACK_MARKET, 5, "Shady Dealings", "Buy something at the black market"),
    ACH(UL_FP_GARDENER, 6, "Graveyard Gardener", "Leave flowers on a grave"),
    ACH(UL_FP_NIGHT_OWL, 7, "Midnight Stroll", "Check on the island after midnight"),
    ACH(UL_FP_BOOKWORM, 8, "Unlock Hunter", "Open the Unlocks gallery"),
    ACH(UL_FP_FAV_FOOD, 9, "Way to the Heart", "Feed a citizen their favourite food"),
    ACH(UL_FP_HEALER, 10, "House Call", "Pay for a check-up at the hospital"),
    ACH(UL_FP_MATCHMAKER, 11, "Matchmaker", "Cheer on a confession that works out"),
    ACH(UL_FP_FULL_HOUSE_HILL, 12, "No Vacancy: Towers", "Fill all 500 Hillvile homes on an empty island"),
    ACH(UL_FP_FULL_HOUSE_HELL, 13, "No Vacancy: Pits", "Fill all 500 Hellvile homes on an empty island"),
    ACH(UL_FP_GHOST_RITE, 14, "Seance", "Raise a ghost with the graveyard rite"),
    ACH(UL_FP_HELPER, 15, "Helping Paw", "Solve a citizen's problem"),
    ACH(UL_FP_HELPER_BIG, 16, "Soulmates", "Reach friendship 99 with a citizen"),
    ACH(UL_FP_COLLECTOR, 17, "Hoarder", "Hold 99 of a single item"),
    ACH(UL_FP_EMPTY_START, 18, "Blank Canvas", "Start an Empty Island"),
    ACH(UL_FP_PREMADE_START, 19, "Welcome to the Park", "Start a Hellvile Park Classic island"),
    ACH(UL_FP_RANDOM_START, 20, "Dice Roller", "Start a Random Island"),
    ACH(UL_FP_FAVOURITES, 21, "Favourites", "Mark 10 citizens as favourites"),
    ACH(UL_FP_SEARCHER, 22, "Detective", "Find a citizen with the name search"),
    ACH(UL_FP_HATED_FOOD, 23, "Oops, Gross", "Feed a citizen the food they hate"),
    ACH(UL_FP_HOUSEWARMING, 24, "Housewarming", "Peek into 25 different windows"),

    /* ---- creator parts that come with an achievement ---- */
    PART(UP_BERET, CLIST_HAT, HAT_BERET, "Artsy Beret", "Give your island a name", 0, 0),
    PART(UP_BANDANA, CLIST_HAT, HAT_BANDANA, "Picnic Bandana", "Throw a picnic in Hellvile Park", 0, 0),
    PART(UP_SCARF, CLIST_ACC, ACC_SCARF, "Cozy Scarf", "Collect rent at Town Hall", 0, 0),
    PART(UP_BACKPACK, CLIST_ACC, ACC_BACKPACK, "Smuggler Backpack", "Buy something at the black market", 0, 0),
    PART(UP_ELF_EARS, CLIST_EARS, EARS_ELF, "Elf Ears", "Check on the island after midnight", 0, 0),
    PART(UP_PLAID, CLIST_PATTERN, PATTERN_PLAID, "Plaid Print", "Pay for a check-up at the hospital", 0, 0),
    PART(UP_PIGTAILS, CLIST_HAIR, HAIR_PIGTAILS, "Pigtails", "Solve a citizen's problem", 0, 0),
    PART(UP_TONGUE, CLIST_MOUTH, MOUTH_TONGUE, "Bleh Tongue", "Feed a citizen the food they hate", 0, 0),
    PART(UP_LASHES, CLIST_EYE, EYE_LASHES, "Lash Eyes", "Feed a citizen their favourite food", 0, 0),
    PART(UP_CLOGS, CLIST_SHOES, SHOES_CLOGS, "Garden Clogs", "Leave flowers on a grave", 0, 0),
    PART(UP_CARGO, CLIST_BOTTOM, BOTTOM_CARGO, "Cargo Pants", "Spend 500 H$ in one go", 0, 0),
    PART(UP_VEST, CLIST_TOP, TOP_SWEATER_VEST, "Sweater Vest", "Mark 10 citizens as favourites", 0, 0),
    PART(UP_MOCHI, CLIST_HEAD, HEAD_MOCHI, "Mochi Head", "Reach friendship 99 with a citizen", 0, 0),
    PART(UP_HOOK_NOSE, CLIST_NOSE, NOSE_HOOK, "Snoop Nose", "Find a citizen with the name search", 0, 0),
    PART(UP_GOATEE, CLIST_FHAIR, FHAIR_GOATEE, "Seance Goatee", "Raise a ghost with the graveyard rite", 0, 0),
    PART(UP_RAISED_BROWS, CLIST_BROW, BROW_RAISED, "Ooh-La-La Brows", "Cheer on a confession that works out", 0, 0),
    /* ---- creator parts from island life ---- */
    PART(UP_CAP, CLIST_HAT, HAT_CAP, "Baseball Cap", "Live on your island for a day", PROG_DAYS_LIVED, 1),
    PART(UP_EARRINGS, CLIST_ACC, ACC_EARRINGS, "Coin Earrings", "Earn 1,000 H$", PROG_MONEY_EARNED, 1000),

    /* ---- paints ---- */
    PAINT(790, PAINT_CARAMEL, "Caramel Paint", "Serve 25 meals", PROG_MEALS_SERVED, 25),
    PAINT(791, PAINT_TEAL, "Teal Paint", "Make 30 friendships", PROG_FRIENDSHIPS, 30),

    /* ---- shop stock ---- */
    ITEM(795, ITEM_VIDEO_GAME, "Video Game", "Earn 1,500 H$", PROG_MONEY_EARNED, 1500),
    ITEM(796, ITEM_PERFUME, "Perfume", "Give 8 gifts", PROG_GIFTS_GIVEN, 8),
    ITEM(797, ITEM_BOARD_GAME, "Board Game", "Make 8 friendships", PROG_FRIENDSHIPS, 8),
    ITEM(798, ITEM_ENERGY_DRINK, "Energy Drink", "Serve 12 meals", PROG_MEALS_SERVED, 12),
    ITEM(799, ITEM_HELLSTEEL, "Hellsteel", "Defeat 25 demons", PROG_DEMONS_KILLED, 25),
    ITEM(800, ITEM_BLUEPRINT, "Blueprint", "Weld your first weapon", PROG_WEAPONS_WELDED, 1),

    /* ---- styles ---- */
    SKIN(805, OPT_WINDOW_SKIN, WSKIN_LAVENDER, "Lavender Windows", "Live on your island for 2 days",
         PROG_DAYS_LIVED, 2),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};

/* achievement -> the creator part that comes with it */
static const uint16_t bonus[][2] = {
    { UL_FP_NAMER, UP_BERET }, { UL_FP_PICNIC, UP_BANDANA }, { UL_FP_LANDLORD, UP_SCARF },
    { UL_FP_BLACK_MARKET, UP_BACKPACK }, { UL_FP_NIGHT_OWL, UP_ELF_EARS }, { UL_FP_HEALER, UP_PLAID },
    { UL_FP_HELPER, UP_PIGTAILS }, { UL_FP_HATED_FOOD, UP_TONGUE }, { UL_FP_FAV_FOOD, UP_LASHES },
    { UL_FP_GARDENER, UP_CLOGS }, { UL_FP_SHOPAHOLIC, UP_CARGO }, { UL_FP_FAVOURITES, UP_VEST },
    { UL_FP_HELPER_BIG, UP_MOCHI }, { UL_FP_SEARCHER, UP_HOOK_NOSE }, { UL_FP_GHOST_RITE, UP_GOATEE },
    { UL_FP_MATCHMAKER, UP_RAISED_BROWS }
};

void fp_achieve(int ul)
{
    int i;
    if (unlock_has(ul)) return;
    unlock_grant(ul);
    for (i = 0; i < DOOMO_COUNTOF(bonus); i++)
        if (bonus[i][0] == ul) unlock_grant(bonus[i][1]);
}

/* Achievements that follow the island's state: checked now and then by
 * the hub (cheap: one pass over the town). */
void fp_check_island_achievements(void)
{
    int i, fav = 0;
    /* a full district only counts on an island you populated yourself */
    if (doomo_town.mode == TOWN_FREEPLAY && doomo_town.population == TOWN_POP_EMPTY) {
        if (town_count_district(0, -1) >= DOOMO_DISTRICT_SIZE) fp_achieve(UL_FP_FULL_HOUSE_HILL);
        if (town_count_district(1, -1) >= DOOMO_DISTRICT_SIZE) fp_achieve(UL_FP_FULL_HOUSE_HELL);
    }
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        const DoomoResident *r = &doomo_town.res[i];
        if (r->status == RES_EMPTY) continue;
        if (r->flags & RF_FAVORITE) fav++;
        if (r->level >= 99) fp_achieve(UL_FP_HELPER_BIG);
    }
    if (fav >= 10) fp_achieve(UL_FP_FAVOURITES);
    for (i = 1; i < ITEM_COUNT; i++)
        if (inv_count(i) >= 99) { fp_achieve(UL_FP_COLLECTOR); break; }
}
