/* fish_scene.c - SCENE_FISHING (owner: fishcook). Work in progress. */
#include "fishcook_int.h"
static void fs_update(void) { if (btn_pressed(BTN_B)) scene_pop(); }
static void fs_draw(void) { ui_backdrop(WIN_CUTE, scene_ticks()); }
const DoomoScene SCENE_FISHING = { "FISHING", NULL, NULL, fs_update, fs_draw, 0 };
