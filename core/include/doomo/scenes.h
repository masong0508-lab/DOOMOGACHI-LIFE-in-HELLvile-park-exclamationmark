/* scenes.h - registry of every scene and its owner. Each owner defines
 * the const DoomoScene in their own .c file. core/src/stubs.c gives
 * weak placeholder definitions so the game always links.
 *
 * Naming convention: DoomoScene.name is the identifier without the
 * SCENE_ prefix ("TITLE", "CREATOR", "UI_GALLERY"). The native runner's
 * `expect-scene NAME` and `scene NAME` script commands use it. */
#ifndef DOOMO_SCENES_H
#define DOOMO_SCENES_H

#include "scene.h"

/* foundation */
extern const DoomoScene SCENE_BOOT;         /* splash + init               */
extern const DoomoScene SCENE_CONFIRM;      /* ui_confirm modal            */
extern const DoomoScene SCENE_TEXT_ENTRY;   /* ui_text_entry modal         */
extern const DoomoScene SCENE_DIALOG;       /* ui_say / ui_choice modal    */
extern const DoomoScene SCENE_UI_GALLERY;   /* hidden debug: every widget  */
/* freeplay UI */
extern const DoomoScene SCENE_TITLE;        /* title + mode select         */
extern const DoomoScene SCENE_NEW_ISLAND;   /* empty / premade / random    */
extern const DoomoScene SCENE_ISLAND;       /* free play hub map           */
extern const DoomoScene SCENE_APARTMENTS;   /* browse homes in a district  */
extern const DoomoScene SCENE_ROSTER;       /* 1000-citizen browser        */
extern const DoomoScene SCENE_RESIDENT;     /* care + social + kill-off    */
extern const DoomoScene SCENE_SHOP;
extern const DoomoScene SCENE_GRAVEYARD;
extern const DoomoScene SCENE_NEWS;
extern const DoomoScene SCENE_OPTIONS;
extern const DoomoScene SCENE_SAVELOAD;
/* (freeplay, additive) the Unlocks gallery (every unlock, owned or locked
 * with its hint, and the progress counters) and the citizen picker (the
 * roster browser in picking mode, see core/src/fp_int.h fp_pick). */
extern const DoomoScene SCENE_UNLOCKS, SCENE_PICKER;
/* creator (arg = resident id to edit, or -1 new Hillvile, -2 new Hellvile,
 * -3 story protagonist) */
extern const DoomoScene SCENE_CREATOR;
/* (creator, additive) Resident id the last creator session wrote
 * (DOOMO_PLAYER_ID for the protagonist), -1 if it was cancelled; and a
 * citizen's nickname set in the creator ("" if none). */
int creator_last_result(void);
const char *creator_nickname(int id);
/* life */
extern const DoomoScene SCENE_EXORCISM;     /* arg = resident id           */
extern const DoomoScene SCENE_PET;          /* tamagotchi LCD screen       */
/* action */
extern const DoomoScene SCENE_COMBAT;       /* arg = map id                */
extern const DoomoScene SCENE_WORKSHOP;     /* weld crafting               */
extern const DoomoScene SCENE_WELD_GAME;    /* welding minigame            */
extern const DoomoScene SCENE_ARENA;        /* free play demon waves       */
/* story */
extern const DoomoScene SCENE_STORY;        /* overworld exploration       */
extern const DoomoScene SCENE_QUESTLOG;
extern const DoomoScene SCENE_CUTSCENE;     /* arg = cutscene id           */
extern const DoomoScene SCENE_CREDITS;
/* minigames (Fun Fair, mg_*.c; arg: see core/src/mg.h) */
extern const DoomoScene SCENE_MG_CATCH, SCENE_MG_WHACK, SCENE_MG_DODGE,
       SCENE_MG_GALLERY, SCENE_MG_DERBY, SCENE_MG_MEMORY, SCENE_MG_WHEEL;

/* concert hall + dreams (concert*.c, dream*.c; args: core/src/concert_int.h) */
extern const DoomoScene SCENE_CONCERT, SCENE_DREAM;

/* music studio (studio*.c; arg: song book page 0..5, -1 = resume the
 * working song, -2 = a new song; studio_edit() in song.h opens a copy) */
extern const DoomoScene SCENE_STUDIO;

/* the Grand Library (lib_*.c; args: doomo/library.h) */
extern const DoomoScene SCENE_LIBRARY, SCENE_LIB_BESTIARY, SCENE_LIB_ALMANAC,
       SCENE_LIB_WEAPONS, SCENE_LIB_ITEMS, SCENE_LIB_TROPHIES, SCENE_LIB_JUKEBOX,
       SCENE_LIB_GALLERY;

/* Lake of Fire fishing + Brimstone Bites kitchen (fish*.c, cook*.c; args:
 * core/src/fishcook_int.h) */
extern const DoomoScene SCENE_FISHING, SCENE_COOKING;

/* (levels, additive) Hell Breach 3..7 + arena modes (levels*.c,
 * arena_modes.c; args: core/src/levels_int.h): the level select (arg =
 * mode | (level + 1) << 4), one level (arg = level | mode << 8 | flags),
 * Boss Rush / Horde / Monster Mode Mayhem (arg = AM_*) */
extern const DoomoScene SCENE_LEVEL_SELECT, SCENE_BREACH, SCENE_ARENA_MODE;

/* --- S0: story + dream house scenes (docs/design/story/SPEC.md sec.4, C14;
 * maintainer after S0: I9, add-only) ---
 * SCENE_HOME        the house: Live / Build / Photo, visits, the haunting,
 *                   Hold the Door (home_draw.c, H4; arg encoding: home.h
 *                   HOME_ARG_*: -(1 + HOME_*) own home, 2 + id visit,
 *                   0x4000 | HSM_* story modes; 0 and 1 are refused)
 * SCENE_HOME_MART   HomeMart and the Black Market (home_mart.c, H4; arg 0
 *                   HomeMart, 1 Black Market)
 * SCENE_STORY_MG    a story side minigame (story_verbs.c shell, S2; arg = a
 *                   story_mg registry id, story_vm.h)
 * SCENE_STORY_TIMING the comic-timing press minigame (story_timing.c, S2;
 *                   arg = pattern | skin << 8)
 * The result of each is read by the story VM after the pop. */
extern const DoomoScene SCENE_HOME, SCENE_HOME_MART, SCENE_STORY_MG, SCENE_STORY_TIMING;
/* --- end S0 --- */

/* Registry (scene.c, foundation): every scene above, NULL-terminated. */
extern const DoomoScene *const doomo_all_scenes[];
/* Find a scene by its name ("TITLE"); also accepts "SCENE_TITLE". */
const DoomoScene *scene_by_name(const char *name);
/* (integration, stubs.c) 1 if the scene is still a weak placeholder. */
int scene_is_stub(const DoomoScene *s);

#endif
