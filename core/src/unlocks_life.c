/* unlocks_life.c - the life module's unlockables (ids 320..415, owner:
 * life): the 16 Doomogachi forms (granted by pet.c when your pet first
 * evolves into one), creator parts, paints and shop items earned through
 * friendships, weddings, babies, exorcisms and days lived, and a few
 * life achievements. Automatic rules use the PROG_* counters; need == 0
 * entries are granted from code (pet evolutions, a first possession,
 * laying a soul to rest, a Monster Mode kill). */
#include "doomo/unlocks.h"
#include "doomo/catalog.h"
#include "doomo/items.h"
#include "doomo/palette.h"

#define PET(id, form, name, how) { id, UL_CAT_PET, 0, form, name, how, 0, 0 }
#define P(id, list, idx, name, how, prog, need) { id, UL_CAT_PART, list, idx, name, how, prog, need }
#define PAINT(id, idx, name, how, prog, need) { id, UL_CAT_PAINT, 0, idx, name, how, prog, need }
#define ITEM(id, idx, name, how, prog, need) { id, UL_CAT_ITEM, 0, idx, name, how, prog, need }
#define MODE(id, idx, name, how, prog, need) { id, UL_CAT_MODE, 0, idx, name, how, prog, need }
#define MISC(id, idx, name, how, prog, need) { id, UL_CAT_MISC, 0, idx, name, how, prog, need }

const DoomoUnlock doomo_unlocks_life[] = {
    /* ---- Doomogachi forms (index = form, see pet.h) ---- */
    PET(320, 0, "Mochiblob", "Hatch a Doomogachi egg"),
    PET(321, 1, "Puffling", "Raise a well-loved child"),
    PET(322, 2, "Sprinkle", "Raise a well-loved teen"),
    PET(323, 3, "Cupcakeling", "Raise a well-loved adult"),
    PET(324, 4, "Halopup", "Raise a very holy baby"),
    PET(325, 5, "Seraphuff", "Raise a very holy child"),
    PET(326, 6, "Choirbun", "Raise a very holy teen"),
    PET(327, 7, "Archangelo", "Raise a very holy adult"),
    PET(328, 8, "Scruffle", "Forget your pet a little..."),
    PET(329, 9, "Grumblin", "Let a child grow up wild"),
    PET(330, 10, "Feralfloof", "Let a teen grow up wild"),
    PET(331, 11, "Wildbeastie", "Let an adult grow up wild"),
    PET(332, 12, "Impkin", "Spoil a baby rotten"),
    PET(333, 13, "Hornlet", "Spoil a child rotten"),
    PET(334, 14, "Hellmuffin", "Spoil a teen rotten"),
    PET(335, 15, "Demogorgoo", "Raise a truly devilish adult"),

    /* ---- exorcisms ---- */
    P(336, CLIST_HAT, HAT_NUN_VEIL, "Nun Veil", "Win your first exorcism", PROG_EXORCISMS_WON, 1),
    P(337, CLIST_ACC, ACC_CROSS_NECKLACE, "Cross Necklace", "Win 2 exorcisms", PROG_EXORCISMS_WON, 2),
    P(338, CLIST_ACC, ACC_ROSARY, "Rosary Necklace", "Win 4 exorcisms", PROG_EXORCISMS_WON, 4),
    P(339, CLIST_EYE, EYE_CROSS, "Cross Eyes", "Win 6 exorcisms", PROG_EXORCISMS_WON, 6),
    P(340, CLIST_HORNS, HORNS_BROKEN, "Broken Horns", "Win 7 exorcisms", PROG_EXORCISMS_WON, 7),

    /* ---- love & weddings ---- */
    P(341, CLIST_EYE, EYE_HEART, "Heart Eyes", "Celebrate a wedding", PROG_MARRIAGES, 1),
    P(342, CLIST_AURA, AURA_HEARTS, "Heart Aura", "Celebrate 2 weddings", PROG_MARRIAGES, 2),
    P(343, CLIST_MOUTH, MOUTH_LIPSTICK, "Lipstick", "Celebrate 4 weddings", PROG_MARRIAGES, 4),
    P(344, CLIST_HAT, HAT_TIARA, "Tiara", "Celebrate 8 weddings", PROG_MARRIAGES, 8),

    /* ---- island life ---- */
    P(345, CLIST_HAT, HAT_PARTY, "Party Hat", "Live in town for 2 days", PROG_DAYS_LIVED, 2),
    P(346, CLIST_TOP, TOP_PAJAMAS, "Pajamas", "Live in town for 4 days", PROG_DAYS_LIVED, 4),
    P(347, CLIST_SHOES, SHOES_SLIPPERS, "Bunny Slippers", "Live in town for 6 days", PROG_DAYS_LIVED, 6),
    P(348, CLIST_HAT, HAT_FLOWER_CROWN, "Flower Crown", "Make 25 friendships", PROG_FRIENDSHIPS, 25),
    P(349, CLIST_PATTERN, PATTERN_HEARTS, "Heart Pattern", "Make 40 friendships", PROG_FRIENDSHIPS, 40),
    P(350, CLIST_MARK, MARK_CHEEK_HEARTS, "Cheek Hearts", "Make 60 friendships", PROG_FRIENDSHIPS, 60),
    P(351, CLIST_ACC, ACC_TEDDY, "Teddy", "Welcome 2 babies to town", PROG_BABIES, 2),
    P(352, CLIST_EYE, EYE_STAR, "Star Eyes", "Welcome 4 babies to town", PROG_BABIES, 4),

    /* ---- darker moments (granted from code) ---- */
    MISC(353, 0, "Something Wicked", "Witness a possession", 0, 0),
    P(354, CLIST_MARK, MARK_TEARS, "Tear Marks", "Lay a lost soul to rest", 0, 0),
    MISC(355, 1, "I Am A Monster", "Kill off a citizen in Monster Mode", 0, 0),

    /* ---- caring ---- */
    P(356, CLIST_TOP, TOP_NURSE, "Nurse Outfit", "Serve 20 meals", PROG_MEALS_SERVED, 20),
    P(357, CLIST_HAT, HAT_CHEF, "Chef Hat", "Serve 40 meals", PROG_MEALS_SERVED, 40),
    P(358, CLIST_ACC, ACC_BOWTIE, "Bowtie", "Give 5 gifts", PROG_GIFTS_GIVEN, 5),
    P(359, CLIST_AURA, AURA_BUBBLES, "Bubble Aura", "Give 20 gifts", PROG_GIFTS_GIVEN, 20),
    P(360, CLIST_TOP, TOP_KIMONO, "Kimono", "Befriend 10 folks across the wall", PROG_CROSS_BORDER_FRIENDS, 10),
    P(361, CLIST_HAIR, HAIR_HIME, "Hime Cut", "Live in town for 45 days", PROG_DAYS_LIVED, 45),

    /* ---- paints ---- */
    PAINT(362, PAINT_MAGENTA, "Magenta Paint", "Celebrate 3 weddings", PROG_MARRIAGES, 3),
    PAINT(363, PAINT_SLIME, "Slime Paint", "Evolve your Doomogachi 3 times", PROG_PET_EVOLUTIONS, 3),

    /* ---- shop stock ---- */
    ITEM(364, ITEM_PLUSH_CROSS, "Plush Cross", "Win your first exorcism", PROG_EXORCISMS_WON, 1),
    ITEM(365, ITEM_ROSARY, "Rosary", "Win 2 exorcisms", PROG_EXORCISMS_WON, 2),
    ITEM(366, ITEM_HYMN_BOOK, "Hymn Book", "Win 3 exorcisms", PROG_EXORCISMS_WON, 3),
    ITEM(367, ITEM_RING, "Ring", "Celebrate a wedding", PROG_MARRIAGES, 1),
    ITEM(368, ITEM_ANGEL_CAKE, "Angel Cake", "Make 15 friendships", PROG_FRIENDSHIPS, 15),
    ITEM(369, ITEM_CHILL_PILL, "Chill Pill", "Fail an exorcism", PROG_EXORCISMS_FAILED, 1),

    /* ---- modes ---- */
    MODE(370, MODE_TAMA_ONLY, "Tamagotchi Only", "Raise a Doomogachi to adulthood", PROG_PET_EVOLUTIONS, 4),

    /* ---- achievements ---- */
    MISC(371, 2, "Cupid", "Celebrate 10 weddings", PROG_MARRIAGES, 10),
    MISC(372, 3, "Exorcist", "Win 10 exorcisms", PROG_EXORCISMS_WON, 10),
    MISC(373, 4, "Tamagotchi Master", "Evolve your Doomogachi 12 times", PROG_PET_EVOLUTIONS, 12),
    MISC(374, 5, "Old Timer", "Live in town for 100 days", PROG_DAYS_LIVED, 100),
    MISC(375, 6, "Social Butterfly", "Make 100 friendships", PROG_FRIENDSHIPS, 100),
    MISC(376, 7, "Baby Boom", "Welcome 10 babies to town", PROG_BABIES, 10),
    MISC(377, 8, "Ghost Whisperer", "Raise 3 ghosts", PROG_GHOSTS_RAISED, 3),
    MISC(378, 9, "Bridge Builder", "Befriend 25 folks across the wall", PROG_CROSS_BORDER_FRIENDS, 25),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};
