/* unlocks_creator.c - the creator's own unlockables (ids 256..319) and the life stage names.
 *
 * Split from creator_data.c (CR3): resident code reads these two tables (unlocks.c, family.c), and a
 * table that resident code reads keeps its whole object's string pool resident on the PS1 (the
 * overlay planner cannot split a merged string section). Here the pool is small; creator_data.c's
 * (about 6 KB of row labels, help lines and names) moves into the creator overlay.
 */
#include "creator_int.h"

/* resident.h (LS0); here, with its only UI, to keep it out of resident RAM
 * on the PS1 */
const char *const doomo_life_stage_names[LIFE_COUNT] = {
    "Adult", "Baby", "Toddler", "Child", "Teen", "Elder"
};

/* ====================================================================== */
/* creator unlockables (ids 256..319)                                       */
#define CP(id, list, idx, name, how, prog, need) \
    { id, UL_CAT_PART, list, idx, name, how, prog, need }

const DoomoUnlock doomo_unlocks_creator[] = {
    /* rewards for filling the park with citizens */
    CP(256, CLIST_EYE, EYE_SPARKLE, "Sparkle Eyes", "Create your first citizen", PROG_CITIZENS_CREATED, 1),
    CP(257, CLIST_HAT, HAT_WITCH, "Witch Hat", "Create 3 citizens", PROG_CITIZENS_CREATED, 3),
    CP(258, CLIST_MARK, MARK_STAR_FRECKLES, "Star Freckles", "Create 5 citizens", PROG_CITIZENS_CREATED, 5),
    CP(259, CLIST_ACC, ACC_MONOCLE, "Monocle", "Create 8 citizens", PROG_CITIZENS_CREATED, 8),
    CP(260, CLIST_HORNS, HORNS_UNICORN, "Unicorn Horn", "Create 12 citizens", PROG_CITIZENS_CREATED, 12),
    CP(261, CLIST_SHOES, SHOES_PLATFORM, "Platform Shoes", "Create 15 citizens", PROG_CITIZENS_CREATED, 15),
    CP(262, CLIST_HAT, HAT_FEZ, "Fez", "Create 18 citizens", PROG_CITIZENS_CREATED, 18),
    CP(263, CLIST_EYE, EYE_GOAT, "Goat Eyes", "Create 22 citizens", PROG_CITIZENS_CREATED, 22),
    CP(264, CLIST_HAT, HAT_SKULL_CAP, "Skull Cap", "Create 30 citizens", PROG_CITIZENS_CREATED, 30),
    CP(265, CLIST_TOP, TOP_LAB_COAT, "Lab Coat", "Create 40 citizens", PROG_CITIZENS_CREATED, 40),
    CP(266, CLIST_HEAD, HEAD_HEART, "Heart Head", "Create 50 citizens", PROG_CITIZENS_CREATED, 50),
    CP(267, CLIST_WINGS, WINGS_FLY, "Fly Wings", "Create 75 citizens", PROG_CITIZENS_CREATED, 75),
    CP(268, CLIST_HAIR, HAIR_POMPADOUR, "Pompadour", "Create 100 citizens", PROG_CITIZENS_CREATED, 100),
    /* creator secrets (granted by creator.c) */
    CP(CRU_MOVE_TO_HELLVILE, CLIST_HORNS, HORNS_SPIRAL, "Spiral Horns",
       "Move a new citizen into Hellvile", 0, 0),
    CP(CRU_MAKEOVER, CLIST_FHAIR, FHAIR_CHOPS, "Mutton Chops",
       "Give a citizen a makeover", 0, 0),
    CP(CRU_SELF_PORTRAIT, CLIST_HAT, HAT_COWBOY, "Cowboy Hat",
       "Create yourself for the story", 0, 0),
    CP(CRU_UNDO_LOTS, CLIST_BROW, BROW_BUSHY, "Bushy Brows",
       "Undo 30 times in one sitting", 0, 0),
    CP(CRU_VOICE_TESTS, CLIST_ACC, ACC_NOSE_RING, "Nose Ring",
       "Test 10 voices in one sitting", 0, 0),
    CP(CRU_RANDOMISE_LOTS, CLIST_MARK, MARK_SCALES, "Scales",
       "Randomise 25 times in one sitting", 0, 0),
    CP(CRU_COPY_PREMADE, CLIST_HAIR, HAIR_BOWL, "Bowl Cut",
       "Copy the look of a premade citizen", 0, 0),
    CP(CRU_POSSESSED_PEEK, CLIST_TAIL, TAIL_LIZARD, "Lizard Tail",
       "Peek at a possessed preview", 0, 0),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};

