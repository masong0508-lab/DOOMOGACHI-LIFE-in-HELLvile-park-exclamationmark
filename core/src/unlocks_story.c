/* unlocks_story.c - the story module's unlockables (owner: story), ids
 * 608..735: modes the ending opens, every song for the jukebox, every
 * cutscene for the gallery, chapter / collection achievements, the
 * keepsake badges side quests reward and Mr. Flops' bunny-ears hat. All
 * are granted by code (unlock_grant) as the story is played. (Side quest
 * rewards are badges, not creator parts: the creator's species presets
 * use almost every part, and those must stay unlocked.)
 */
#include "story_int.h"

#define MODE(id, m, name, how) { id, UL_CAT_MODE, 0, m, name, how, 0, 0 }
#define SONGU(s, name) { (uint16_t)(USTORY_SONG_FIRST + (s) - 1), UL_CAT_GALLERY, GAL_JUKEBOX, s, name, "Hear it in Story Mode", 0, 0 }
#define CUTU(c, name, how) { (uint16_t)(USTORY_CUT_FIRST + (c)), UL_CAT_GALLERY, GAL_CUTSCENES, c, name, how, 0, 0 }
#define MISC(id, k, name, how) { id, UL_CAT_MISC, 83, k, name, how, 0, 0 }
#define PART(k, list, idx, name, how) { (uint16_t)(USTORY_REWARD_FIRST + (k)), UL_CAT_PART, list, idx, name, how, 0, 0 }
#define BADGE(k, name, how) { (uint16_t)(USTORY_REWARD_FIRST + (k)), UL_CAT_MISC, 83, (uint16_t)(20 + (k)), name, how, 0, 0 }

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

    CUTU(CUT_ARRIVAL, "Arrival in HaHaland", "Start Story Mode"),
    CUTU(CUT_FOG, "The Purple Fog", "Investigate the Wall"),
    CUTU(CUT_ERUPTION, "The Gate Erupts", "Clear the Vape Cellar"),
    CUTU(CUT_VAPULA_DOWN, "Vapula Deflates", "Defeat Lord Vapula"),
    CUTU(CUT_WALL_FALLS, "The Wall Falls", "Bring both towns together"),
    CUTU(CUT_FREEPLAY, "Happily Ever After", "Finish Story Mode"),
    CUTU(CUT_HOME, "Home Sweet Home", "Move into your cottage"),
    CUTU(CUT_FEAST_POP, "What an Ass", "Let the Feast of Rebuilding go too far"),

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

    /* side quest keepsakes (badges), and one hat */
    BADGE(0, "Cupid's Arrow Pin", "Side quest: Knightly Hearts"),
    BADGE(1, "Lovebird Stamp", "Side quest: Mail Mix-Up"),
    BADGE(2, "Tulip Pin", "Side quest: Coffee for Bluebell"),
    BADGE(3, "Oma's Cake Locket", "Side quest: Grandma's Glasses"),
    BADGE(4, "Bookworm Bookmark", "Side quest: Bedtime Stories"),
    BADGE(5, "Honorary Firefighter", "Side quest: Fire Drill"),
    BADGE(6, "Spider Swatter Medal", "Side quest: Graveyard Shift"),
    BADGE(7, "Golden Hug Token", "Side quest: Nervous Nellie"),
    BADGE(8, "Storm Chaser", "Side quest: Smoke Signals"),
    BADGE(9, "Bat Buddy", "Side quest: Squeak the Bat"),
    BADGE(10, "Flower Friend Ribbon", "Side quest: The Florist's Apprentice"),
    BADGE(11, "Sky Scout Wings", "Side quest: Spire Scout"),
    BADGE(12, "Champion's Buckle", "Side quest: Echo of the Duke"),
    BADGE(13, "Wall Artist Ribbon", "Side quest: Wall Art"),
    BADGE(14, "Surf Club Member", "Side quest: Sandcastle Contest"),
    PART(15, CLIST_HAT, HAT_BUNNY_EARS, "Mr. Flops Ears", "Side quest: Pip's Bravery Badge"),
    BADGE(16, "Wedding Planner Pin", "Side quest: Frog Wedding"),

    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};
