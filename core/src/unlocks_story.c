/* unlocks_story.c - the story module's unlockables (owner: story), ids
 * 608..735 (677..710: the story SPEC's additions, S2): modes the ending opens, every song for the jukebox, every
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
/* --- S2: the story SPEC's ids 677..710 (R7, C14; unlock_ids.h) --- */
#define CUTU2(c, name, how) { (uint16_t)UL_STORY_CUT2(c), UL_CAT_GALLERY, GAL_CUTSCENES, c, name, how, 0, 0 }
#define SONGU2(s, name) { (uint16_t)UL_STORY_SONG2(s), UL_CAT_GALLERY, GAL_JUKEBOX, s, name, "Hear it in Story Mode", 0, 0 }
/* misc k: title variants, Breach and Arena variants, creator part sets
 * (UL_MISC_STORY index 40 + k; S3 / S5 grant them, the creator's owner may
 * re-point the two part sets at real CLIST_* parts) */
#define MISC2(k, name, how) { (uint16_t)(UL_STORY_MISC2_FIRST + (k)), UL_CAT_MISC, UL_MISC_STORY, (uint16_t)(40 + (k)), name, how, 0, 0 }
/* --- end S2 --- */

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
    /* --- S2: the 9 new songs (689..697) --- */
    SONGU2(SONG_MINGLE, "Mingle"),
    SONGU2(SONG_WALL_LULLABY, "Lullaby for the Wall"),
    SONGU2(SONG_TWO_CHOIRS, "Two Choirs"),
    SONGU2(SONG_BOAT_FLOATS, "The Boat Floats"),
    SONGU2(SONG_THIRTEEN_BELLS, "Thirteen Bells"),
    SONGU2(SONG_HELL_LULLABY, "Hellvile Lullaby"),
    SONGU2(SONG_DOUBLE_ACT, "Double Act"),
    SONGU2(SONG_HEEHEE_JINGLE, "Hee. Hee."),
    SONGU2(SONG_NUMBER_TEN, "Number Ten"),
    /* --- end S2 --- */

    CUTU(CUT_ARRIVAL, "Arrival in HaHaland", "Start Story Mode"),
    CUTU(CUT_FOG, "The Purple Fog", "Investigate the Wall"),
    CUTU(CUT_ERUPTION, "The Gate Erupts", "Clear the Vape Cellar"),
    CUTU(CUT_VAPULA_DOWN, "Vapula Deflates", "Defeat Lord Vapula"),
    CUTU(CUT_WALL_FALLS, "The Wall Falls", "Bring both towns together"),
    CUTU(CUT_FREEPLAY, "Happily Ever After", "Finish Story Mode"),
    CUTU(CUT_HOME, "Home Sweet Home", "Move into your cottage"),
    CUTU(CUT_FEAST_POP, "What an Ass", "Let the Feast of Rebuilding go too far"),
    /* --- S2: the 12 new cutscenes (677..688) --- */
    CUTU2(CUT_P0, "The Crayon Prologue", "Start Story Mode"),
    CUTU2(CUT_BOTTLE, "The Bottle", "Chapter 2: the Bottle"),
    CUTU2(CUT_F1, "Flashback: The Fountain", "Remember the first flashback"),
    CUTU2(CUT_F2, "Flashback: The Pump", "Remember the second flashback"),
    CUTU2(CUT_F3, "Flashback: Re-plumbed", "Remember the third flashback"),
    CUTU2(CUT_F4, "Cupcakes and Bone Soup", "Remember the fourth flashback"),
    CUTU2(CUT_INT3_KNOCKS, "Knock Knock", "Answer every knock"),
    CUTU2(CUT_SUMMIT, "The Summit", "Bring both towns to one table"),
    CUTU2(CUT_SAYIT, "Say It", "Say it"),
    CUTU2(CUT_FEAST_SAVE, "Saved by the Bell", "Save the Feast of Rebuilding"),
    CUTU2(CUT_E1, "Days Since: 1", "See the epilogue"),
    CUTU2(CUT_BALLOON, "The Ninth Cloud", "Take the balloon up"),
    /* --- end S2 --- */

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
    /* --- S2: misc 698..710 (R7: 3 title variants, 6 Breach variants, 2
     * arena modes, 2 creator part sets; granted by S3 / S5 content) --- */
    MISC2(0, "Title: On Fire", "Finish Story Mode"),
    MISC2(1, "Title: Population 1000", "Side mission: S143"),
    MISC2(2, "Title: Mingle Night", "Side mission: S195"),
    MISC2(3, "Breach: The Bakery", "Hell Breach 3, after the story"),
    MISC2(4, "Breach: Soft", "Hell Breach 4, after the story"),
    MISC2(5, "Breach: Dry", "Hell Breach 5, after the story"),
    MISC2(6, "Breach: The Choir", "Hell Breach 6, after the story"),
    MISC2(7, "Breach: The Ninth Cloud", "Side mission: S171"),
    MISC2(8, "Breach 3-7 Post-Story", "Finish Story Mode"),
    MISC2(9, "Arena: The Exes", "Side mission: S174"),
    MISC2(10, "Arena: Hold the Door", "Hold the Door (Ep 3.7)"),
    MISC2(11, "Creator: Stitched Parts", "Side mission: the Florist Monster"),
    MISC2(12, "Creator: Hellvile Palette", "Side mission: Phantasma's portrait"),
    /* --- end S2 --- */

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
