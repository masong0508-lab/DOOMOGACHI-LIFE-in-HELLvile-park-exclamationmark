/* library.h - the Grand Library of HaHaland: collections & encyclopedias
 * (owner: library; core/src/lib_*.c, core/src/unlocks_library.c).
 *
 * Quillby the owlkin bookseller runs it. The lobby (SCENE_LIBRARY) is a
 * shelf of seven books, each a browser with categories, filters, detail
 * pages and a completion %:
 *   BESTIARY        every demon: animated sprite, lore, threat, weaknesses
 *   CITIZEN ALMANAC all 1000 citizens by district, search, sort, profiles
 *   WEAPON CATALOG  every weld part with previews, stats and flavour text
 *   FOOD & ITEM DEX every item, its effects and who loves / hates each food
 *   TROPHY WALL     the whole unlocks registry with progress and hints
 *   JUKEBOX         every song and sound effect (unlock: MODE_JUKEBOX)
 *   ART GALLERY     textures, skies, sprites and the palette (MODE_GALLERY)
 *
 * The free play island and the story overworld list the library through
 * the activities registry (ACTLOC_LIBRARY): the lobby in both modes, plus
 * a direct shortcut to every wing in free play.
 *
 * Unlock ids 384..415 (the unused tail of life's range): 384..394 are the
 * Bestiary's "sighted" bits (one per demon), 395..408 the library's
 * unlockables (unlocks_library.c), 409..415 raw bits (wings visited).
 * No save fields of its own, no PROG_* counters.
 */
#ifndef DOOMO_LIBRARY_H
#define DOOMO_LIBRARY_H

#include "types.h"

/* ---- the Bestiary's discovery API (combat / story may call these) ------- */
/* A demon of THING_* type `thing_type` was encountered (seen on screen,
 * fought, read about...). Grants its Bestiary "sighted" unlock the first
 * time (toast "NEW UNLOCK! Bestiary: Impling"). Anything that is not a
 * demon type is ignored. Returns 1 if the entry was newly discovered.
 * Cheap: safe to call every tick for every visible demon. */
int  library_mark_seen(int thing_type);
/* 1 if the demon has a Bestiary entry: sighted, or ever defeated (the
 * action module's demon gallery unlocks count as sightings). */
int  library_seen(int thing_type);
/* 1 if a demon of that type was ever defeated (action's GAL_DEMONS bit). */
int  library_defeated(int thing_type);
/* Demons in the Bestiary (sighted or defeated) / in total. */
int  library_seen_count(void);
#define LIBRARY_DEMONS 11       /* THING_IMPLING .. THING_LORD_VAPULA      */

/* ---- completion ------------------------------------------------------------ */
enum { LIB_WING_BESTIARY, LIB_WING_ALMANAC, LIB_WING_WEAPONS, LIB_WING_ITEMS,
       LIB_WING_TROPHIES, LIB_WING_JUKEBOX, LIB_WING_GALLERY, LIB_WING_COUNT };
/* Completion of a wing in percent (0..100); -1 for a bad wing. The
 * Almanac's scan of 1000 citizens is the costliest (~1000 cheap checks):
 * the lobby asks once on entry, not per tick. */
int  library_wing_pct(int wing);
/* Average over all wings (0..100). */
int  library_total_pct(void);
/* Can the wing be opened right now (Jukebox / Gallery are unlockable)? */
int  library_wing_open(int wing);
/* Has the player ever opened the wing? (raw unlock bits 409..415) */
int  library_wing_visited(int wing);

/* ---- unlock ids --------------------------------------------------------------- */
#define LIB_UL_FIRST        384
#define LIB_UL_SEEN(t)      (LIB_UL_FIRST + (t) - 3)   /* t = THING_* demon   */
#define LIB_UL_JUKEBOX      395     /* MODE_JUKEBOX                           */
#define LIB_UL_GALLERY      396     /* MODE_GALLERY                           */
#define LIB_UL_CARD         397     /* Library Card: first visit              */
#define LIB_UL_BOOKWORM     398     /* every wing opened: Peach Windows        */
#define LIB_UL_DEMONOLOGIST 399     /* every demon sighted: red crosshair      */
#define LIB_UL_FIELD        400     /* every demon defeated                    */
#define LIB_UL_HISTORIAN    401     /* 100 citizens known: Paw Cursor          */
#define LIB_UL_KNOW_IT_ALL  402     /* 500 citizens known                      */
#define LIB_UL_GUN_NERD     403     /* every weapon part unlocked              */
#define LIB_UL_GOURMET      404     /* every food in the bag at once           */
#define LIB_UL_POLISHER     405     /* 100 unlocks owned                       */
#define LIB_UL_NIGHT_OWL    406     /* visited the library at night            */
#define LIB_UL_DJ           407     /* 10 songs in one jukebox session         */
#define LIB_UL_CRITIC       408     /* every room of the gallery in one visit  */
#define LIB_UL_VISITED(w)   (409 + (w))  /* raw bits, not in the table         */
#define LIB_UL_LAST         415

/* UL_CAT_MISC list id of the library's trophies and sightings. */
#define LIB_UL_LIST 0x4C        /* 'L' */

/* ---- scene args ---------------------------------------------------------------- */
/* Every library scene takes arg 0 from the hub. Debug / runner: OR in
 * LIB_ARG_DEMO to move the premade citizens in on an empty island and to
 * open locked wings for a look; the low byte picks the starting category
 * (tab) of a wing, or the selected book in the lobby. */
#define LIB_ARG_DEMO 0x100
#define LIB_ARG_TAB(a) ((a) & 0xFF)

#endif
