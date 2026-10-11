/* fp_unlocks.c - SCENE_UNLOCKS + SCENE_JUKEBOX (owner: freeplay). */
#include "fp_int.h"

static void tmp_update(void)
{
    if (btn_pressed(BTN_B)) { if (scene_depth() > 1) scene_pop(); else scene_set(&SCENE_TITLE, 0); }
}
static void tmp_draw(void) { ui_backdrop(WIN_CUTE, doomo_frame_count()); }
const DoomoScene SCENE_UNLOCKS = { "UNLOCKS", NULL, NULL, tmp_update, tmp_draw, 0 };
const DoomoScene SCENE_JUKEBOX = { "JUKEBOX", NULL, NULL, tmp_update, tmp_draw, 0 };
