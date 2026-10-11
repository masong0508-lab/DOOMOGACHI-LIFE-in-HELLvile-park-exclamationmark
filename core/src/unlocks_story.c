/* unlocks_story.c - the story module's unlockables (owner: story), ids
 * 608..735: modes the ending opens, every song for the jukebox, every
 * cutscene for the gallery, chapter / collection achievements and the
 * creator parts and window skin that side quests reward. All are granted
 * by code (unlock_grant) as the story is played.
 */
#include "story_int.h"

#define MODE(id, m, name, how) { id, UL_CAT_MODE, 0, m, name, how, 0, 0 }
#define SONGU(s, name) { (uint16_t)(USTORY_SONG_FIRST + (s) - 1), UL_CAT_GALLERY, GAL_JUKEBOX, s, name, "Hear it in Story Mode", 0, 0 }
#define CUTU(c, name, how) { (uint16_t)(USTORY_CUT_FIRST + (c)), UL_CAT_GALLERY, GAL_CUTSCENES, c, name, how, 0, 0 }
#define MISC(id, k, name, how) { id, UL_CAT_MISC, 83, k, name, how, 0, 0 }
#define PART(k, list, idx, name, how) { (uint16_t)(USTORY_REWARD_FIRST + (k)), UL_CAT_PART, list, idx, name, how, 0, 0 }

const DoomoUnlock doomo_unlocks_story[] = {
    MODE(USTORY_UNITED_PARK, MODE_UNITED_PARK, "United Park", "Finish Story Mode"),
    MODE(USTORY_HELLVILE_VISITS, MODE_HELLVILE_VISITS, "Hellvile Visits", "Reach Chapter 4 of Story Mode"),
    MODE(USTORY_NEW_GAME_PLUS, MODE_NEW_GAME_PLUS, "New Game+", "Finish Story Mode"),

    SONGU(SONG_TITLE, "Doomogachi Life!"),
    SONGU(SONG_HAHALAND_DAY, "HaHaland Day"),
    SONGU(SONG_HAHALAND_NIGHT, "HaHaland Night"),
    SONGU(SONG_CREATOR, "Make a Friend!"),
    SONGU(SONG_SHOP, "Shop Hop"),
    SONGU(SONG_CHURCH, "St. Cuddles Hymn"),
    SONGU(SONG_PET, "Doomogachi Beeps"),
    SONGU(SONG_HELLVILE, "Hellvile After Dark"),
    SONGU(SONG_EXORCISM, "The Ritual"),
    SONGU(SONG_WORKSHOP, "Rivet's Workshop"),
    SONGU(SONG_INVASION, "Hell Invades"),
    SONGU(SONG_HELL_BREACH, "Hell Breach"),
    SONGU(SONG_BOSS, "Lord Vapula"),
    SONGU(SONG_VICTORY, "Victory!"),
    SONGU(SONG_SAD, "Goodbye, Friend"),
    SONGU(SONG_FESTIVAL, "Hellvile Park Festival"),
    SONGU(SONG_CREDITS, "Everyone Together"),
    SONGU(SONG_VAPE_LOUNGE, "Cloud Nine Lounge"),
    SONGU(SONG_OMINOUS, "The Wall"),

    CUTU(CUT_CHAPTER1, "Chapter 1 Card", "Start Story Mode"),
    CUTU(CUT_CHAPTER2, "Chapter 2 Card", "Reach Chapter 2"),
    CUTU(CUT_CHAPTER3, "Chapter 3 Card", "Reach Chapter 3"),
    CUTU(CUT_CHAPTER4, "Chapter 4 Card", "Reach Chapter 4"),
    CUTU(CUT_ARRIVAL, "Arrival in HaHaland", "Start Story Mode"),
    CUTU(CUT_FOG, "The Purple Fog", "Investigate the Wall"),
    CUTU(CUT_ERUPTION, "The Gate Erupts", "Clear the Vape Cellar"),
    CUTU(CUT_VAPULA_DOWN, "Vapula Deflates", "Defeat Lord Vapula"),
    CUTU(CUT_WALL_FALLS, "The Wall Falls", "Bring both towns together"),
    CUTU(CUT_FREEPLAY, "Happily Ever After", "Finish Story Mode"),
    CUTU(CUT_HOME, "Home Sweet Home", "Move into your cottage"),

    MISC(USTORY_CH1_DONE, 0, "Welcome Home", "Finish Chapter 1"),
    MISC(USTORY_CH2_DONE, 1, "Vapor Buster", "Finish Chapter 2"),
    MISC(USTORY_CH3_DONE, 2, "Hell Survivor", "Finish Chapter 3"),
    MISC(USTORY_CH4_DONE, 3, "Bridge Builder", "Finish Chapter 4"),
    MISC(USTORY_ALL_SIDE, 4, "Good Neighbour", "Finish every side quest"),
    MISC(USTORY_EXPLORER, 5, "Explorer", "Visit almost every building in Story Mode"),
    MISC(USTORY_SECRETS, 6, "Sparkle Hunter", "Find 15 secret sparkles"),
    MISC(USTORY_PACIFIST, 7, "Plush Hero", "Clear a Hell Breach with only cute weapons"),
    MISC(USTORY_FRIEND_OF_ALL, 8, "Friend of All", "Make 25 friends in Story Mode"),
    MISC(USTORY_BRUTAL_START, 9, "Boomstick", "Choose the brutal starter weapon"),
    MISC(USTORY_CUTE_START, 10, "Bubble Trouble", "Choose the cute starter weapon"),

    PART(0, CLIST_HAT, HAT_TOP_HAT, "Mayor's Top Hat", "Side quest: Knightly Hearts"),
    PART(1, CLIST_ACC, ACC_SCARF, "Lovebird Scarf", "Side quest: Mail Mix-Up"),
    PART(2, CLIST_WINGS, WINGS_BUTTERFLY, "Knitted Butterfly Wings", "Side quest: Coffee for Bluebell"),
    PART(3, CLIST_ACC, ACC_EARRINGS, "Oma's Pearl Earrings", "Side quest: Grandma's Glasses"),
    PART(4, CLIST_PATTERN, PATTERN_PLAID, "Cosy Plaid", "Side quest: Bedtime Stories"),
    PART(5, CLIST_HAIR, HAIR_WILD, "Hero Hair", "Side quest: Fire Drill"),
    PART(6, CLIST_TOP, TOP_CLOAK, "Hero's Cloak", "Side quest: Graveyard Shift"),
    PART(7, CLIST_MARK, MARK_STITCHES, "Patched-Up Stitches", "Side quest: Nervous Nellie"),
    PART(8, CLIST_ACC, ACC_SUNGLASSES, "Cloud Shades", "Side quest: Smoke Signals"),
    PART(9, CLIST_WINGS, WINGS_TINY_BAT, "Squeak's Bat Wings", "Side quest: Squeak the Bat"),
    PART(10, CLIST_MARK, MARK_EYELINER, "Goth Eyeliner", "Side quest: The Florist's Apprentice"),
    PART(11, CLIST_TAIL, TAIL_FEATHER_FAN, "Lucky Feather Tail", "Side quest: Spire Scout"),
    PART(12, CLIST_SHOES, SHOES_COMBAT, "Champ Boots", "Side quest: Echo of the Duke"),
    PART(13, CLIST_HAT, HAT_BERET, "Artist's Beret", "Side quest: Wall Art"),
    PART(14, CLIST_ACC, ACC_BACKPACK, "Sandy Backpack", "Side quest: Sandcastle Contest"),
    PART(15, CLIST_HAT, HAT_BUNNY_EARS, "Mr. Flops Ears", "Side quest: Pip's Bravery Badge or Frog Wedding"),
    { USTORY_REWARD_FIRST + 16, UL_CAT_SKIN, OPT_WINDOW_SKIN, WSKIN_LAVENDER, "Lavender Windows",
      "Finish Chapter 4 of Story Mode", 0, 0 },

    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};
