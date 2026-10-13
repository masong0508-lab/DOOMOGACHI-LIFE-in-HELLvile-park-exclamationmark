/* unlocks.c - the unlock registry and progress counters (owner:
 * foundation), plus foundation's own unlockables (ids 0..255): the most
 * outrageous creator parts, a few paints, cheats, window skins, title
 * themes, cursors, crosshairs and camera filters.
 *
 * Every team adds a NULL-terminated table (weak empty defaults live in
 * stubs.c); the registry concatenates them. Lookups use a small RAM index
 * (id -> entry, plus a per-list bitmap of which creator options have an
 * entry at all) built on first use: ~2.3 KB.
 */
#include "doomo/unlocks.h"
#include "doomo/font.h"
#include "doomo/town.h"
#include "doomo/catalog.h"
#include "doomo/weld.h"
#include "doomo/palette.h"
#include "doomo/ui.h"
#include "doomo/audio.h"
#include "doomo/platform.h"

const char *const doomo_progress_names[PROG_COUNT] = {
    "Citizens created", "Exorcisms won", "Exorcisms failed", "Demons defeated",
    "Demons defeated (cute)", "Demons defeated (brutal)", "Weapons welded",
    "Days lived", "Friendships", "Marriages", "Babies", "Ghosts raised",
    "Pet evolutions", "Gifts given", "Meals served", "Secrets found",
    "Headshots", "Cross-border friends", "Money earned", "Arena waves",
    "Best: Cupcake Catch", "Best: Whack-an-Imp", "Best: Vape Cloud Dodge",
    "Best: Bubble Blaster", "Best: Doomogachi Derby", "Best: Memory Match",
    "Wheel of Fate: last spin day",
    "Concerts", "Hit Parade (packed)", "Hit Parade 2 (packed)",
    "Fish caught",
    "Hell Breach ranks (packed)"
};

const char *const doomo_unlock_cat_names[10] = {
    "Creator Part", "Paint", "Weapon Part", "Shop Item", "Game Mode", "Cheat",
    "Gallery", "Style", "Pet Form", "Secret"
};

const char *const doomo_mode_names[MODE_COUNT] = {
    "Demon Arena", "Boss Rush", "Hellvile Visits", "Tamagotchi Only",
    "Monster Mode", "United Park", "New Game+", "Jukebox", "Gallery"
};

/* ---------------------------------------------------------------------- */
/* foundation's unlockables                                                 */
#define P(id, list, idx, name, how, prog, need) \
    { id, UL_CAT_PART, list, idx, name, how, prog, need }
#define PAINT(id, idx, name, how, prog, need) \
    { id, UL_CAT_PAINT, 0, idx, name, how, prog, need }
#define CHEAT(id, opt, name, how, prog, need) \
    { id, UL_CAT_CHEAT, 0, opt, name, how, prog, need }
#define SKIN(id, opt, val, name, how, prog, need) \
    { id, UL_CAT_SKIN, opt, val, name, how, prog, need }

const DoomoUnlock doomo_unlocks_foundation[] = {
    /* species */
    P(0, CLIST_SPECIES, SPECIES_CACOBLOB, "Cacoblob", "Win 3 exorcisms", PROG_EXORCISMS_WON, 3),
    P(1, CLIST_SPECIES, SPECIES_MUMMY, "Mummy", "Live in town for 7 days", PROG_DAYS_LIVED, 7),
    P(2, CLIST_SPECIES, SPECIES_GHOST, "Ghost", "Raise a ghost at the graveyard", PROG_GHOSTS_RAISED, 1),
    /* body & head */
    P(3, CLIST_BODY, BODY_NOODLE, "Noodle Body", "Serve 50 meals", PROG_MEALS_SERVED, 50),
    P(4, CLIST_BODY, BODY_TOP_HEAVY, "Top-Heavy Body", "Weld 5 weapons", PROG_WEAPONS_WELDED, 5),
    P(5, CLIST_BODY, BODY_BUFF, "Buff Body", "Defeat 100 demons", PROG_DEMONS_KILLED, 100),
    P(6, CLIST_HEAD, HEAD_SKULL, "Skull Head", "Defeat 50 demons", PROG_DEMONS_KILLED, 50),
    P(7, CLIST_HEAD, HEAD_STAR, "Star Head", "Find 3 secrets", PROG_SECRETS_FOUND, 3),
    P(8, CLIST_HEAD, HEAD_DIAMOND, "Diamond Head", "Earn 5,000 H$", PROG_MONEY_EARNED, 5000),
    /* eyes */
    P(9, CLIST_EYE, EYE_SPIRAL, "Spiral Eyes", "Fail an exorcism (oops)", PROG_EXORCISMS_FAILED, 1),
    P(10, CLIST_EYE, EYE_VOID, "Void Eyes", "Win 10 exorcisms", PROG_EXORCISMS_WON, 10),
    P(11, CLIST_EYE, EYE_HOLLOW, "Hollow Eyes", "Fail 3 exorcisms", PROG_EXORCISMS_FAILED, 3),
    P(12, CLIST_EYE, EYE_COMPOUND, "Compound Eyes", "Create 20 citizens", PROG_CITIZENS_CREATED, 20),
    P(13, CLIST_EYE, EYE_X, "X Eyes", "Defeat 200 demons", PROG_DEMONS_KILLED, 200),
    P(14, CLIST_EYE, EYE_DEMON_SLIT, "Demon Slit Eyes", "Defeat 25 demons", PROG_DEMONS_KILLED, 25),
    P(15, CLIST_EYE, EYE_GLOW, "Glowing Eyes", "Land 10 headshots", PROG_HEADSHOTS, 10),
    /* brows, nose, mouth, ears */
    P(16, CLIST_BROW, BROW_SCARRED, "Scarred Brows", "Defeat 150 demons", PROG_DEMONS_KILLED, 150),
    P(17, CLIST_BROW, BROW_UNIBROW, "Unibrow", "Create 10 citizens", PROG_CITIZENS_CREATED, 10),
    P(18, CLIST_NOSE, NOSE_CLOWN, "Clown Nose", "Give 30 gifts", PROG_GIFTS_GIVEN, 30),
    P(19, CLIST_NOSE, NOSE_SKULL, "Skull Nose", "Defeat 75 demons", PROG_DEMONS_KILLED, 75),
    P(20, CLIST_NOSE, NOSE_PIG, "Piggy Nose", "Serve 100 meals", PROG_MEALS_SERVED, 100),
    P(21, CLIST_MOUTH, MOUTH_VOID, "Void Mouth", "Win 20 exorcisms", PROG_EXORCISMS_WON, 20),
    P(22, CLIST_MOUTH, MOUTH_DROOL, "Drooly Mouth", "Serve 30 meals", PROG_MEALS_SERVED, 30),
    P(23, CLIST_MOUTH, MOUTH_SHARK, "Shark Mouth", "Defeat 40 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 40),
    P(24, CLIST_MOUTH, MOUTH_FANGY, "Fangy Mouth", "Befriend 3 folks across the wall", PROG_CROSS_BORDER_FRIENDS, 3),
    P(25, CLIST_EARS, EARS_FIN, "Fin Ears", "Live in town for 14 days", PROG_DAYS_LIVED, 14),
    P(26, CLIST_EARS, EARS_BAT, "Bat Ears", "Befriend someone from Hellvile", PROG_CROSS_BORDER_FRIENDS, 1),
    /* hair */
    P(27, CLIST_HAIR, HAIR_FLAME, "Flame Hair", "Defeat 60 demons", PROG_DEMONS_KILLED, 60),
    P(28, CLIST_HAIR, HAIR_TENTACLES, "Tentacle Hair", "Fail 5 exorcisms", PROG_EXORCISMS_FAILED, 5),
    P(29, CLIST_HAIR, HAIR_SLIME_DRIP, "Slime Drip Hair", "Defeat 30 demons cutely", PROG_DEMONS_KILLED_CUTE, 30),
    P(30, CLIST_HAIR, HAIR_DRILLS, "Drill Curls", "Earn 10,000 H$", PROG_MONEY_EARNED, 10000),
    P(31, CLIST_HAIR, HAIR_LIBERTY_SPIKES, "Liberty Spikes", "Weld 10 weapons", PROG_WEAPONS_WELDED, 10),
    P(32, CLIST_HAIR, HAIR_CROWN_BRAID, "Crown Braid", "Celebrate 3 weddings", PROG_MARRIAGES, 3),
    P(33, CLIST_HAIR, HAIR_ODANGO, "Odango Buns", "Make 10 friendships", PROG_FRIENDSHIPS, 10),
    P(34, CLIST_FHAIR, FHAIR_WIZARD, "Wizard Beard", "Live in town for 30 days", PROG_DAYS_LIVED, 30),
    P(35, CLIST_FHAIR, FHAIR_HANDLEBAR, "Handlebar 'Stache", "Earn 3,000 H$", PROG_MONEY_EARNED, 3000),
    /* horns, tails, wings, halos */
    P(36, CLIST_HORNS, HORNS_MANY, "So Many Horns", "Defeat 300 demons", PROG_DEMONS_KILLED, 300),
    P(37, CLIST_HORNS, HORNS_HALO_HORNS, "Halo Horns", "Win 15 exorcisms", PROG_EXORCISMS_WON, 15),
    P(38, CLIST_HORNS, HORNS_CROWN_SPIKES, "Crown Spikes", "Survive 10 arena waves", PROG_ARENA_WAVES, 10),
    P(39, CLIST_HORNS, HORNS_DEVIL_TALL, "Tall Devil Horns", "Defeat 120 demons", PROG_DEMONS_KILLED, 120),
    P(40, CLIST_TAIL, TAIL_FLAME, "Flame Tail", "Defeat 80 demons", PROG_DEMONS_KILLED, 80),
    P(41, CLIST_TAIL, TAIL_TENTACLE, "Tentacle Tail", "Find 5 secrets", PROG_SECRETS_FOUND, 5),
    P(42, CLIST_TAIL, TAIL_SKELETAL, "Skeletal Tail", "Raise 2 ghosts", PROG_GHOSTS_RAISED, 2),
    P(43, CLIST_TAIL, TAIL_SPIKED, "Spiked Tail", "Defeat 50 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 50),
    P(44, CLIST_WINGS, WINGS_MECH, "Mech Wings", "Weld 15 weapons", PROG_WEAPONS_WELDED, 15),
    P(45, CLIST_WINGS, WINGS_FLAME, "Flame Wings", "Survive 20 arena waves", PROG_ARENA_WAVES, 20),
    P(46, CLIST_WINGS, WINGS_BONE, "Bone Wings", "Raise 3 ghosts", PROG_GHOSTS_RAISED, 3),
    P(47, CLIST_WINGS, WINGS_DRAGON, "Dragon Wings", "Defeat 400 demons", PROG_DEMONS_KILLED, 400),
    P(48, CLIST_HALO, HALO_FLAME_RING, "Flame Ring", "Win 25 exorcisms", PROG_EXORCISMS_WON, 25),
    P(49, CLIST_HALO, HALO_THORNS, "Thorn Halo", "Fail 8 exorcisms", PROG_EXORCISMS_FAILED, 8),
    P(50, CLIST_HALO, HALO_BROKEN, "Broken Halo", "Fail 2 exorcisms", PROG_EXORCISMS_FAILED, 2),
    /* markings */
    P(51, CLIST_MARK, MARK_THIRD_EYE, "Third Eye", "Find 7 secrets", PROG_SECRETS_FOUND, 7),
    P(52, CLIST_MARK, MARK_PENTAGRAM, "Pentagram Mark", "Defeat 250 demons", PROG_DEMONS_KILLED, 250),
    P(53, CLIST_MARK, MARK_SKULL_PAINT, "Skull Paint", "Defeat 20 demons", PROG_DEMONS_KILLED, 20),
    P(54, CLIST_MARK, MARK_CRACKS, "Cracks", "Land 50 headshots", PROG_HEADSHOTS, 50),
    P(55, CLIST_MARK, MARK_CLOWN, "Clown Paint", "Give 50 gifts", PROG_GIFTS_GIVEN, 50),
    /* outfits */
    P(56, CLIST_TOP, TOP_PLATE_ARMOR, "Plate Armor", "Survive 15 arena waves", PROG_ARENA_WAVES, 15),
    P(57, CLIST_TOP, TOP_SPACESUIT, "Spacesuit", "Find 10 secrets", PROG_SECRETS_FOUND, 10),
    P(58, CLIST_TOP, TOP_HAZMAT, "Hazmat Suit", "Defeat 100 demons cutely", PROG_DEMONS_KILLED_CUTE, 100),
    P(59, CLIST_TOP, TOP_PRIEST, "Priest Robes", "Win 5 exorcisms", PROG_EXORCISMS_WON, 5),
    P(60, CLIST_TOP, TOP_ARMOR, "Armor", "Defeat 70 demons", PROG_DEMONS_KILLED, 70),
    P(61, CLIST_TOP, TOP_MAID, "Maid Outfit", "Serve 75 meals", PROG_MEALS_SERVED, 75),
    P(62, CLIST_BOTTOM, BOTTOM_TUTU, "Tutu", "Make 20 friendships", PROG_FRIENDSHIPS, 20),
    P(63, CLIST_BOTTOM, BOTTOM_KILT, "Kilt", "Live in town for 21 days", PROG_DAYS_LIVED, 21),
    P(64, CLIST_BOTTOM, BOTTOM_ROBE, "Robe Skirt", "Win 8 exorcisms", PROG_EXORCISMS_WON, 8),
    P(65, CLIST_SHOES, SHOES_HOOVES, "Hooves", "Defeat 90 demons", PROG_DEMONS_KILLED, 90),
    P(66, CLIST_SHOES, SHOES_ROLLERSKATES, "Rollerskates", "Live in town for 10 days", PROG_DAYS_LIVED, 10),
    P(67, CLIST_SHOES, SHOES_CLAWS, "Claws", "Defeat 60 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 60),
    P(68, CLIST_HAT, HAT_TRAFFIC_CONE, "Traffic Cone", "Find 2 secrets", PROG_SECRETS_FOUND, 2),
    P(69, CLIST_HAT, HAT_HORNED_HELMET, "Horned Helmet", "Survive 5 arena waves", PROG_ARENA_WAVES, 5),
    P(70, CLIST_HAT, HAT_MARINE_HELMET, "Marine Helmet", "Defeat 150 demons", PROG_DEMONS_KILLED, 150),
    P(71, CLIST_HAT, HAT_CROWN, "Crown", "Earn 25,000 H$", PROG_MONEY_EARNED, 25000),
    P(72, CLIST_HAT, HAT_PROPELLER, "Propeller Cap", "Evolve your Doomogachi twice", PROG_PET_EVOLUTIONS, 2),
    P(73, CLIST_HAT, HAT_BUCKET, "Bucket Hat", "Give 10 gifts", PROG_GIFTS_GIVEN, 10),
    P(74, CLIST_ACC, ACC_GAS_MASK, "Gas Mask", "Defeat 35 demons", PROG_DEMONS_KILLED, 35),
    P(75, CLIST_ACC, ACC_CHAINS, "Chains", "Defeat 45 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 45),
    P(76, CLIST_ACC, ACC_PACIFIER, "Pacifier", "Welcome a baby to town", PROG_BABIES, 1),
    P(77, CLIST_ACC, ACC_CAPE, "Cape", "Weld 3 weapons", PROG_WEAPONS_WELDED, 3),
    P(78, CLIST_ACC, ACC_SKULL_NECKLACE, "Skull Necklace", "Defeat 10 demons", PROG_DEMONS_KILLED, 10),
    P(79, CLIST_AURA, AURA_GLITCH, "Glitch Aura", "Find 12 secrets", PROG_SECRETS_FOUND, 12),
    P(80, CLIST_AURA, AURA_FLAMES, "Flame Aura", "Defeat 180 demons", PROG_DEMONS_KILLED, 180),
    P(81, CLIST_AURA, AURA_FLIES, "Fly Aura", "Serve 150 meals (gross)", PROG_MEALS_SERVED, 150),
    P(82, CLIST_AURA, AURA_SMOKE, "Smoke Aura", "Defeat 15 demons", PROG_DEMONS_KILLED, 15),
    P(83, CLIST_PATTERN, PATTERN_SKULLS, "Skull Print", "Defeat 30 demons", PROG_DEMONS_KILLED, 30),
    P(84, CLIST_PATTERN, PATTERN_FLAMES, "Flame Print", "Defeat 55 demons", PROG_DEMONS_KILLED, 55),
    P(85, CLIST_PATTERN, PATTERN_CAMO, "Camo Print", "Survive 3 arena waves", PROG_ARENA_WAVES, 3),
    /* paints */
    PAINT(96, PAINT_HELLFIRE, "Hellfire Paint", "Defeat 100 demons", PROG_DEMONS_KILLED, 100),
    PAINT(97, PAINT_ECTOPLASM, "Ectoplasm Paint", "Raise a ghost at the graveyard", PROG_GHOSTS_RAISED, 1),
    PAINT(98, PAINT_VAPE_PURPLE, "Vape Purple Paint", "Win your first exorcism", PROG_EXORCISMS_WON, 1),
    PAINT(99, PAINT_BLOOD, "Blood Paint", "Defeat 60 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 60),
    /* cheats */
    CHEAT(128, OPT_CHEAT_BIG_HEAD, "Big-Head Mode", "Create 25 citizens", PROG_CITIZENS_CREATED, 25),
    CHEAT(129, OPT_CHEAT_CONFETTI, "Confetti Everything", "Defeat 150 demons cutely", PROG_DEMONS_KILLED_CUTE, 150),
    CHEAT(130, OPT_CHEAT_INFINITE_CANDY, "Infinite Candy", "Earn 20,000 H$", PROG_MONEY_EARNED, 20000),
    CHEAT(131, OPT_CHEAT_CUTE_DEMONS, "Cute Demons", "Win 30 exorcisms", PROG_EXORCISMS_WON, 30),
    CHEAT(132, OPT_CHEAT_BRUTAL_RESIDENTS, "Brutal Residents", "Defeat 500 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 500),
    CHEAT(133, OPT_CHEAT_LOW_GRAV, "Low Gravity", "Find 15 secrets", PROG_SECRETS_FOUND, 15),
    CHEAT(134, OPT_CHEAT_PAPER_MODE, "Paper Mode", "Live in town for 60 days", PROG_DAYS_LIVED, 60),
    CHEAT(135, OPT_CHEAT_GOD_MODE, "Plush Armor", "Survive 30 arena waves", PROG_ARENA_WAVES, 30),
    CHEAT(136, OPT_CHEAT_RICH, "Bottomless Wallet", "Earn 50,000 H$", PROG_MONEY_EARNED, 50000),
    CHEAT(137, OPT_CHEAT_DISCO, "Disco Lights", "Celebrate 5 weddings", PROG_MARRIAGES, 5),
    CHEAT(138, OPT_CHEAT_TINY_DEMONS, "Tiny Demons", "Land 200 headshots", PROG_HEADSHOTS, 200),
    CHEAT(139, OPT_CHEAT_ALL_GHOSTS, "Everyone's a Ghost", "Raise 5 ghosts", PROG_GHOSTS_RAISED, 5),
    /* window skins */
    SKIN(160, OPT_WINDOW_SKIN, WSKIN_HELLFIRE, "Hellfire Windows", "Defeat 50 demons", PROG_DEMONS_KILLED, 50),
    SKIN(161, OPT_WINDOW_SKIN, WSKIN_MIDNIGHT, "Midnight Windows", "Live in town for 5 days", PROG_DAYS_LIVED, 5),
    SKIN(162, OPT_WINDOW_SKIN, WSKIN_TAMAGOTCHI, "Tamagotchi Windows", "Evolve your Doomogachi 4 times", PROG_PET_EVOLUTIONS, 4),
    SKIN(163, OPT_WINDOW_SKIN, WSKIN_GOTH, "Goth Windows", "Befriend 5 folks across the wall", PROG_CROSS_BORDER_FRIENDS, 5),
    /* title themes */
    SKIN(170, OPT_TITLE_THEME, 2, "Candy Storm Title", "Make 15 friendships", PROG_FRIENDSHIPS, 15),
    SKIN(171, OPT_TITLE_THEME, 3, "Hellvile Sunset Title", "Defeat 25 demons", PROG_DEMONS_KILLED, 25),
    SKIN(172, OPT_TITLE_THEME, 4, "Vape Apocalypse Title", "Defeat 250 demons", PROG_DEMONS_KILLED, 250),
    SKIN(173, OPT_TITLE_THEME, 5, "United Park Title", "See the story's ending", 0, 0),
    SKIN(174, OPT_GORE, GORE_MIXED, "Cute + Gore Mix", "See the story's ending", 0, 0),
    /* cursors */
    SKIN(180, OPT_CURSOR, 4, "Skull Cursor", "Defeat 40 demons", PROG_DEMONS_KILLED, 40),
    SKIN(181, OPT_CURSOR, 5, "Fang Cursor", "Win 12 exorcisms", PROG_EXORCISMS_WON, 12),
    /* camera filters */
    SKIN(190, OPT_CAMERA_FILTER, 1, "Storybook Filter", "Make 5 friendships", PROG_FRIENDSHIPS, 5),
    SKIN(191, OPT_CAMERA_FILTER, 2, "VHS Filter", "Weld 8 weapons", PROG_WEAPONS_WELDED, 8),
    SKIN(192, OPT_CAMERA_FILTER, 3, "Game Pal Filter", "Evolve your Doomogachi", PROG_PET_EVOLUTIONS, 1),
    SKIN(193, OPT_CAMERA_FILTER, 4, "Sepia Filter", "Live in town for 3 days", PROG_DAYS_LIVED, 3),
    SKIN(194, OPT_CAMERA_FILTER, 5, "Nightvision Filter", "Defeat 130 demons", PROG_DEMONS_KILLED, 130),
    /* crosshairs */
    SKIN(200, OPT_CROSSHAIR, 5, "Skull Crosshair", "Land 25 headshots", PROG_HEADSHOTS, 25),
    SKIN(201, OPT_CROSSHAIR, 6, "Paw Crosshair", "Defeat 20 demons cutely", PROG_DEMONS_KILLED_CUTE, 20),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};

/* (integration) one slot per team of doomo/unlock_ids.h; every table has a
 * weak empty default in stubs.c (never in this file: GCC would fold a weak
 * constant defined here). Keep the order: test_integration.c maps each
 * slot to its owner's id range. */
const DoomoUnlock *const doomo_unlock_tables[] = {
    doomo_unlocks_foundation, doomo_unlocks_creator, doomo_unlocks_life,
    doomo_unlocks_action, doomo_unlocks_story, doomo_unlocks_freeplay,
    doomo_unlocks_art, doomo_unlocks_portrait, doomo_unlocks_roster,
    doomo_unlocks_minigames, doomo_unlocks_concert, doomo_unlocks_studio,
    doomo_unlocks_library, doomo_unlocks_fishcook, doomo_unlocks_levels, NULL
};
#define UL_TABLES 15
/* Registry slots are read through a volatile lvalue, like activities.c /
 * save_ext.c (ARCHITECTURE.md "Weak defaults of registries"). */
#define UL_TABLE(t) (((const DoomoUnlock *const volatile *)doomo_unlock_tables)[t])

DOOMO_STATIC_ASSERT(PROG_COUNT <= UL_PROG_SLOTS &&
                    UL_PROG_SLOTS == sizeof(doomo_town.progress) / sizeof(doomo_town.progress[0]),
                    progress_counters_fit_the_town);

/* ---------------------------------------------------------------------- */
/* index                                                                    */
static uint8_t  ul_ready;
static uint16_t ul_len[UL_TABLES];
static uint16_t ul_total;
static int16_t  ul_by_id[DOOMO_UNLOCK_BITS];      /* global index or -1 */
static uint8_t  ul_part_bits[CLIST_COUNT][8];    /* has an entry (index < 64) */
static uint8_t  ul_filter[64];                   /* hash of (cat, list, index) */
/* (integration) part keys gated by more than one entry ("alternate
 * routes", unlock_ids.h): owning any of them unlocks the part. */
static uint8_t  ul_alias[UL_ALIAS_MAX][2];       /* (list, index) */
static uint8_t  ul_nalias;
#define UL_HASH(c, l, i) ((((unsigned)(c) * 37u + (unsigned)(l)) * 61u + (unsigned)(i)) & 511u)

static void ul_init(void)
{
    int t, i, g = 0;
    if (ul_ready) return;
    ul_ready = 1;
    for (i = 0; i < DOOMO_UNLOCK_BITS; i++) ul_by_id[i] = -1;
    memset(ul_part_bits, 0, sizeof(ul_part_bits));
    ul_nalias = 0;
    for (t = 0; t < UL_TABLES && UL_TABLE(t); t++) {
        const DoomoUnlock *u = UL_TABLE(t);
        for (i = 0; u[i].name; i++, g++) {
            if (u[i].id < DOOMO_UNLOCK_BITS) {
                if (ul_by_id[u[i].id] < 0) ul_by_id[u[i].id] = (int16_t)g;
                else plat_log("unlocks: duplicate unlock id");
            }
            {
                unsigned hb = UL_HASH(u[i].cat, u[i].list, u[i].index);
                ul_filter[hb >> 3] |= (uint8_t)(1u << (hb & 7));
            }
            if ((u[i].cat == UL_CAT_PART || u[i].cat == UL_CAT_WEAPON_PART) &&
                u[i].list < CLIST_COUNT && u[i].index < 64) {
                uint8_t *pb = &ul_part_bits[u[i].list][u[i].index >> 3];
                uint8_t m = (uint8_t)(1 << (u[i].index & 7));
                if ((*pb & m) && ul_nalias < UL_ALIAS_MAX) {   /* 2nd route */
                    ul_alias[ul_nalias][0] = u[i].list;
                    ul_alias[ul_nalias][1] = (uint8_t)u[i].index;
                    ul_nalias++;
                }
                *pb |= m;
            }
        }
        ul_len[t] = (uint16_t)i;
    }
    ul_total = (uint16_t)g;
}

int unlock_count(void)
{
    ul_init();
    return ul_total;
}

const DoomoUnlock *unlock_at(int i)
{
    int t;
    ul_init();
    if (i < 0) return NULL;
    for (t = 0; t < UL_TABLES && UL_TABLE(t); t++) {
        if (i < ul_len[t]) return &UL_TABLE(t)[i];
        i -= ul_len[t];
    }
    return NULL;
}

const DoomoUnlock *unlock_by_id(int id)
{
    ul_init();
    if (id < 0 || id >= DOOMO_UNLOCK_BITS || ul_by_id[id] < 0) return NULL;
    return unlock_at(ul_by_id[id]);
}

static int ul_is_alias(int list, int index)
{
    int k;
    for (k = 0; k < ul_nalias; k++)
        if (ul_alias[k][0] == list && ul_alias[k][1] == index) return 1;
    return 0;
}

const DoomoUnlock *unlock_find(int cat, int list, int index)
{
    int i, t, alias;
    const DoomoUnlock *first = NULL;
    ul_init();
    if ((cat == UL_CAT_PART || cat == UL_CAT_WEAPON_PART) && list >= 0 &&
        list < CLIST_COUNT && index >= 0 && index < 64 &&
        !(ul_part_bits[list][index >> 3] & (1 << (index & 7))))
        return NULL;                       /* fast path: never locked */
    {
        /* no unlock hashes here: never locked (menus ask this per item and
         * per frame, e.g. the 32 paint swatches) */
        unsigned hb = UL_HASH(cat, list, index);
        if (!(ul_filter[hb >> 3] & (1u << (hb & 7)))) return NULL;
    }
    /* (integration) with alternate routes, prefer an owned entry so every
     * caller's `u && !unlock_has(u->id)` honours all of them */
    alias = (cat == UL_CAT_PART || cat == UL_CAT_WEAPON_PART) && ul_is_alias(list, index);
    for (t = 0; t < UL_TABLES && UL_TABLE(t); t++) {
        const DoomoUnlock *u = UL_TABLE(t);
        for (i = 0; i < ul_len[t]; i++)
            if (u[i].cat == cat && u[i].list == list && u[i].index == index) {
                if (!alias || unlock_has(u[i].id)) return &u[i];
                if (!first) first = &u[i];
            }
    }
    return first;
}

int unlock_has(int id)
{
    if (id < 0 || id >= DOOMO_UNLOCK_BITS) return 0;
    return (doomo_town.unlocks[id >> 3] >> (id & 7)) & 1;
}

static int grant_bit(int id)
{
    if (id < 0 || id >= DOOMO_UNLOCK_BITS || unlock_has(id)) return 0;
    doomo_town.unlocks[id >> 3] |= (uint8_t)(1 << (id & 7));
    return 1;
}

int unlock_grant(int id)
{
    const DoomoUnlock *u;
    char msg[64];
    if (!grant_bit(id)) return 0;
    u = unlock_by_id(id);
    doomo_snprintf(msg, sizeof(msg), STR_ICON_STAR " NEW UNLOCK! %s", u ? u->name : "???");
    ui_toast(msg);
    audio_sfx(SFX_SPARKLE);
    return 1;
}

int unlock_grant_match(int cat, int list, int index)
{
    const DoomoUnlock *u = unlock_find(cat, list, index);
    return u ? unlock_grant(u->id) : 0;
}

int unlock_ok(int cat, int list, int index)
{
    const DoomoUnlock *u = unlock_find(cat, list, index);
    return !u || unlock_has(u->id);
}

int unlock_option_ok(int list, int index)
{
    return unlock_ok(list >= CLIST_WFRAME ? UL_CAT_WEAPON_PART : UL_CAT_PART, list, index);
}

int unlock_paint_ok(int paint) { return unlock_ok(UL_CAT_PAINT, 0, paint); }
int unlock_mode_ok(int mode) { return unlock_ok(UL_CAT_MODE, 0, mode); }

static int check_rules(int counter)
{
    int i, n, granted = 0;
    ul_init();
    n = ul_total;
    for (i = 0; i < n; i++) {
        const DoomoUnlock *u = unlock_at(i);
        if (u->need == 0 || u->prog >= PROG_COUNT) continue;
        if (counter >= 0 && u->prog != counter) continue;
        if (doomo_town.progress[u->prog] >= u->need && !unlock_has(u->id))
            granted += unlock_grant(u->id);
    }
    return granted;
}

int unlock_check_all(void) { return check_rules(-1); }

int unlock_owned_count(void)
{
    int i, n, c = 0;
    ul_init();
    n = ul_total;
    for (i = 0; i < n; i++)
        if (unlock_has(unlock_at(i)->id)) c++;
    return c;
}

void unlock_grant_all(void)
{
    int i, n;
    ul_init();
    n = ul_total;
    for (i = 0; i < n; i++) grant_bit(unlock_at(i)->id);
}

void progress_add(int counter, int amount)
{
    uint32_t v;
    if (counter < 0 || counter >= PROG_COUNT) return;
    v = doomo_town.progress[counter];
    if (amount >= 0) v = (v + (uint32_t)amount < v) ? 0xFFFFFFFFu : v + (uint32_t)amount;
    else v = ((uint32_t)-amount > v) ? 0 : v - (uint32_t)-amount;
    doomo_town.progress[counter] = v;
    check_rules(counter);
}

void progress_set(int counter, uint32_t value)
{
    if (counter < 0 || counter >= PROG_COUNT) return;
    doomo_town.progress[counter] = value;
    check_rules(counter);
}

uint32_t progress_get(int counter)
{
    if (counter < 0 || counter >= PROG_COUNT) return 0;
    return doomo_town.progress[counter];
}
