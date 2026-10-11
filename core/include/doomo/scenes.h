/* scenes.h - registry of every scene and its owner. Each owner defines
 * the const DoomoScene in their own .c file. core/src/scene_stubs.c gives
 * weak placeholder definitions so the game always links. */
#ifndef DOOMO_SCENES_H
#define DOOMO_SCENES_H

#include "scene.h"

/* foundation */
extern const DoomoScene SCENE_BOOT;         /* splash + init               */
extern const DoomoScene SCENE_CONFIRM;      /* ui_confirm modal            */
extern const DoomoScene SCENE_TEXT_ENTRY;   /* ui_text_entry modal         */
extern const DoomoScene SCENE_DIALOG;       /* ui_say / ui_choice modal    */
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
/* creator (arg = resident id to edit, or -1 new Hillvile, -2 new Hellvile,
 * -3 story protagonist) */
extern const DoomoScene SCENE_CREATOR;
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

#endif
