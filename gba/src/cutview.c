/* cutview.c - GBA debug mode: cutscene viewer (owner: gba).
 *
 * Hold SELECT while powering on and a list of every story cutscene is shown
 * instead of the normal boot. Up/Down pick one, L/R jump 5, A plays it,
 * START inside a cutscene skips it and returns to the list. The list
 * includes "What an Ass" (the Feast of Rebuilding pop scene).
 * Compile out with `make CUTVIEW=0`.
 *
 * Only core API is used: the cutscene scene is looked up by name
 * (scene_by_name). Finishing a cutscene runs its normal end hook, so it
 * is marked as seen in the gallery.
 */
#include "gba_plat.h"
#include "doomo/doomo.h"

#ifdef DOOMO_CUTSCENE_VIEWER

/* same order as the CUT_* enum (story.h) */
static const char *const k_names[] = {
    "Chapter 1", "Chapter 2", "Chapter 3", "Chapter 4", "Arrival in HaHaland",
    "The Purple Fog", "The Gate Erupts", "Vapula Deflates", "The Wall Falls",
    "Free Play", "Home Sweet Home", "What an Ass (Feast: pop)"
};
#define NCUT ((int)(sizeof k_names / sizeof k_names[0]))
_Static_assert(CUT_COUNT == 12, "cutview.c: update k_names for the new CUT_* ids");

#define VIS   10        /* rows on screen (240x160 fits 10 at 11 px) */
#define ROW_H 11

static int s_sel;
static int s_top;
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
    if (btn_pressed(BTN_UP))    s_sel = (s_sel + NCUT - 1) % NCUT;
    if (btn_pressed(BTN_DOWN))  s_sel = (s_sel + 1) % NCUT;
    if (btn_pressed(BTN_L))     s_sel = (s_sel + NCUT - 5) % NCUT;
    if (btn_pressed(BTN_R))     s_sel = (s_sel + 5) % NCUT;
    if (s_sel < s_top) s_top = s_sel;
    if (s_sel >= s_top + VIS) s_top = s_sel - VIS + 1;
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
    int w = plat_screen_w(), h = plat_screen_h(), i, y = 20;
    plat_clear(PAL_UI_RED_D);
    plat_rect(0, 0, w, 14, PAL_UI_PURPLE);
    plat_text(4, 3, "DEBUG: CUTSCENE VIEWER", PAL_WHITE);
    for (i = s_top; i < NCUT && i < s_top + VIS; i++, y += ROW_H) {
        if (i == s_sel) {
            plat_rect(2, y - 2, w - 4, ROW_H, PAL_UI_PINK);
            plat_text(5, y, ">", PAL_UI_RED);
            plat_text(15, y, k_names[i], PAL_UI_RED_D);
        } else {
            plat_text(15, y, k_names[i], PAL_UI_CREAM);
        }
    }
    plat_text(4, h - 9, "A play  START skips  L/R jump", PAL_UI_CREAM);
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
