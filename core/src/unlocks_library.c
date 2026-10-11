/* unlocks_library.c - the Grand Library's unlockables (owner: library),
 * ids 384..408 (the unused tail of life's range 320..415; 409..415 are
 * raw "wing visited" bits that are not listed here, see library.h).
 *
 *   384..394  Bestiary sightings, one per demon (library_mark_seen())
 *   395, 396  the Jukebox and the Art Gallery wings (MODE_JUKEBOX /
 *             MODE_GALLERY): after a day in town / three days, or granted
 *             in the lobby (first demon spotted / a quarter of the library)
 *   397..408  library trophies; three of them come with a style reward
 *             (Peach Windows, the red crosshair colour, Paw Cursor)
 *
 * Only the two wings have automatic rules; everything else is granted by
 * code (lib_*.c) when the library notices it. */
#include "lib_int.h"

#define SEEN(t, name, how) \
    { (uint16_t)LIB_UL_SEEN(t), UL_CAT_MISC, LIB_UL_LIST, (uint16_t)((t) - THING_IMPLING), name, how, 0, 0 }
#define TROPHY(id, name, how) \
    { id, UL_CAT_MISC, LIB_UL_LIST, (uint16_t)(16 + (id) - LIB_UL_FIRST), name, how, 0, 0 }
#define SKIN(id, opt, val, name, how) { id, UL_CAT_SKIN, opt, val, name, how, 0, 0 }

const DoomoUnlock doomo_unlocks_library[] = {
    /* ---- Bestiary sightings ---- */
    SEEN(THING_IMPLING, "Bestiary: Impling", "Spot an Impling"),
    SEEN(THING_VAPE_WRAITH, "Bestiary: Vape Wraith", "Spot a Vape Wraith"),
    SEEN(THING_PUFF_DEMON, "Bestiary: Puff Demon", "Spot a Puff Demon"),
    SEEN(THING_HELLHOUND_PUP, "Bestiary: Hellhound Pup", "Spot a Hellhound Pup"),
    SEEN(THING_BONE_JESTER, "Bestiary: Bone Jester", "Spot a Bone Jester"),
    SEEN(THING_SLIME_BELCHER, "Bestiary: Slime Belcher", "Spot a Slime Belcher"),
    SEEN(THING_GARGOYLE, "Bestiary: Gargoyle", "Spot a Gargoyle"),
    SEEN(THING_CYBER_PIGGY, "Bestiary: Cyber Piggy", "Spot a Cyber Piggy"),
    SEEN(THING_BRUTE, "Bestiary: Brute", "Spot a Brute"),
    SEEN(THING_CLOUD_SPIDER, "Bestiary: Cloud Spider", "Spot a Cloud Spider"),
    SEEN(THING_LORD_VAPULA, "Bestiary: Lord Vapula", "Face Lord Vapula"),
    /* ---- the unlockable wings ---- */
    { LIB_UL_JUKEBOX, UL_CAT_MODE, 0, MODE_JUKEBOX, "Library Jukebox",
      "Live in town for a day, or spot your first demon", PROG_DAYS_LIVED, 1 },
    { LIB_UL_GALLERY, UL_CAT_MODE, 0, MODE_GALLERY, "Library Art Gallery",
      "Live in town for 3 days, or fill a quarter of the Library", PROG_DAYS_LIVED, 3 },
    /* ---- library trophies ---- */
    TROPHY(LIB_UL_CARD, "Library Card", "Visit Quillby's Grand Library"),
    SKIN(LIB_UL_BOOKWORM, OPT_WINDOW_SKIN, WSKIN_PEACH, "Bookworm: Peach Windows",
         "Open every book in the Grand Library"),
    SKIN(LIB_UL_DEMONOLOGIST, OPT_CROSSHAIR_COLOR, 4, "Demonologist: Red Crosshair",
         "Spot every kind of demon for the Bestiary"),
    TROPHY(LIB_UL_FIELD, "Field Researcher", "Defeat every kind of demon at least once"),
    SKIN(LIB_UL_HISTORIAN, OPT_CURSOR, 1, "Town Historian: Paw Cursor",
         "Get to know 100 citizens (Citizen Almanac)"),
    TROPHY(LIB_UL_KNOW_IT_ALL, "Know-It-All", "Get to know 500 citizens"),
    TROPHY(LIB_UL_GUN_NERD, "Gun Nerd", "Unlock every part in the Weapon Catalog"),
    TROPHY(LIB_UL_GOURMET, "Gourmet", "Have every kind of food in your bag at once"),
    TROPHY(LIB_UL_POLISHER, "Trophy Polisher", "Own 100 unlockables"),
    TROPHY(LIB_UL_NIGHT_OWL, "Night Owl", "Visit the library after dark"),
    TROPHY(LIB_UL_DJ, "DJ Doomo", "Play 10 different songs in one Jukebox visit"),
    TROPHY(LIB_UL_CRITIC, "Art Critic", "Visit every room of the Art Gallery in one go"),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};
