/* cook_scene.c - SCENE_COOKING (owner: fishcook). Work in progress. */
#include "fishcook_int.h"
static void cs_update(void) { if (btn_pressed(BTN_B)) scene_pop(); }
static void cs_draw(void) { ui_backdrop(WIN_CUTE, scene_ticks()); }
const DoomoScene SCENE_COOKING = { "COOKING", NULL, NULL, cs_update, cs_draw, 0 };
