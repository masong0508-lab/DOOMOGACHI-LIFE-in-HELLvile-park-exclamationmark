/* cutview.c - PS1 debug mode: cutscene viewer.
 *
 * Hold SELECT while the game boots (through the splash, until the menu
 * appears) and a list of every story cutscene is shown instead of the
 * title screen. Up/Down pick one, Cross (A) plays it, START inside a
 * cutscene skips it and returns to the list. Compile out with
 * -DDOOMO_CUTSCENE_VIEWER=OFF (cmake option).
 *
 * Only resident core API is used here: the cutscene scene itself is looked
 * up by name (scene_by_name) so this file adds no reference that would pin
 * the story overlay's code or data into RAM. Note: finishing a cutscene
 * runs its normal end hook, which marks it as seen in the gallery.
 */
#include "ps1.h"

#ifdef DOOMO_CUTSCENE_VIEWER

/* same order as the CUT_* enum (story.h); the overlay's own name table
 * (story_cutscene_name) lives in overlay code, so it is not called here */
static const char *const k_names[] = {
    "Chapter 1", "Chapter 2", "Chapter 3", "Chapter 4", "Arrival in HaHaland",
    "The Purple Fog", "The Gate Erupts", "Vapula Deflates", "The Wall Falls",
    "Free Play", "Home Sweet Home"
};
#define NCUT ((int)(sizeof k_names / sizeof k_names[0]))
_Static_assert(CUT_COUNT == 11, "cutview.c: update k_names for the new CUT_* ids");

static int s_sel;
static int s_played;

static void view_enter(int arg)
{
    (void)arg;
    s_played = 0;
    audio_music(SONG_NONE);
}

static void view_update(void)
{
    const DoomoScene *cs;
    if (s_played) {                 /* back from a cutscene: stop its music */
        audio_music(SONG_NONE);
        s_played = 0;
    }
    if (btn_pressed(BTN_UP))   s_sel = (s_sel + NCUT - 1) % NCUT;
    if (btn_pressed(BTN_DOWN)) s_sel = (s_sel + 1) % NCUT;
    if (btn_pressed(BTN_LEFT | BTN_L))  s_sel = (s_sel + NCUT - 5) % NCUT;
    if (btn_pressed(BTN_RIGHT | BTN_R)) s_sel = (s_sel + 5) % NCUT;
    if (btn_pressed(BTN_A)) {
        cs = scene_by_name("CUTSCENE");
        if (cs) {
            s_played = 1;
            scene_push(cs, s_sel);  /* depth > 1: it pops back to us */
        }
    }
}

static void view_draw(void)
{
    int w = plat_screen_w(), i, y = 40;
    plat_clear(PAL_UI_RED_D);
    plat_rect(0, 0, w, 24, PAL_UI_PURPLE);
    plat_text(8, 8, "DEBUG: CUTSCENE VIEWER", PAL_WHITE);
    for (i = 0; i < NCUT; i++, y += 14) {
        if (i == s_sel) {
            plat_rect(8, y - 3, w - 16, 13, PAL_UI_PINK);
            plat_text(14, y, ">", PAL_UI_RED);
            plat_text(26, y, k_names[i], PAL_UI_RED_D);
        } else {
            plat_text(26, y, k_names[i], PAL_UI_CREAM);
        }
    }
    plat_text(8, plat_screen_h() - 20, "UP/DOWN pick   L/R jump 5", PAL_UI_CREAM);
    plat_text(8, plat_screen_h() - 10, "X play   START (in scene) skips", PAL_UI_CREAM);
}

static const DoomoScene SCENE_CUTVIEW = {
    "CUTVIEW", view_enter, 0, view_update, view_draw, 0
};

void cutview_enter(void)
{
    scene_set(&SCENE_CUTVIEW, 0);
}

#else
void cutview_enter(void) {}
#endif
