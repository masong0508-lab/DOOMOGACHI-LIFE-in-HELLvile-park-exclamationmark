/* ending_scene.c - SCENE_ENDING_CARD: an ending's card (E1; ending.h).
 * arg = ENDING_*. Shows which ending, its colour category, how many of the 14 are found and
 * how often this one was reached; A or B closes it. The titles and the epilogue text are the
 * content packages' (E3 / E4): until they land the card names the ending by its number.
 * Strong definition of the weak placeholder in weak_scenes.c. State is 4 B. */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/ending.h"
#include "doomo/ui.h"
#include "doomo/audio.h"
#include "doomo/input.h"
#include "doomo/platform.h"

static struct { int16_t k, t; } C;

static const char *const cat_names[ENDCAT_COUNT] = {
    "A good ending", "A bad ending", "A secret ending", "A grown-ups' ending", "A very secret ending"
};

/* The written endings so far (E1 stand-in text; E3 / E4 replace these with the content
 * packages' titles and epilogues). Short lines: the card is 240 px wide. */
static const struct { const char *title, *l1, *l2; } story[ENDING_COUNT] = {
    /* E01 */ { "Home, Sweet Hell", "The park is quiet again.", "Your door is open. Come in." },
    /* E02 */ { "A Full House", "Every chair has someone in it.", "Hell has never felt so warm." },
    /* E03 */ { 0, 0, 0 }, /* E04 */ { "Family Portrait", "One frame. Everyone you love.", "Nobody is looking away." },
};

static void ec_enter(int arg)
{
    memset(&C, 0, sizeof C);
    C.k = (int16_t)(arg >= 0 && arg < ENDING_COUNT ? arg : 0);
    audio_sfx(SFX_SPARKLE);
}

static void ec_update(void)
{
    C.t++;
    if (C.t > 20 && (btn_pressed(BTN_A) || btn_pressed(BTN_B) || btn_pressed(BTN_START))) {
        audio_sfx(SFX_SELECT);
        scene_pop();
    }
}

static void ec_draw(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int sw = plat_screen_w(), sh = plat_screen_h(), cat = ending_category(C.k), y;
    char buf[48];
    ui_backdrop(WIN_CUTE, (uint32_t)doomo_frame_count());
    ui_window(8, sh / 2 - 50, sw - 16, 108, WIN_CUTE);
    y = sh / 2 - 44;
    if (C.k == ENDING_NOPE) doomo_snprintf(buf, sizeof buf, "Nope.");
    else if (C.k < 4 && story[C.k].title) doomo_snprintf(buf, sizeof buf, "%s", story[C.k].title);
    else doomo_snprintf(buf, sizeof buf, "ENDING %d", C.k + 1);
    ui_text_center(sw / 2, y, buf, st->text);
    ui_text_center(sw / 2, y + 14, cat >= 0 ? cat_names[cat] : "?", st->text_dim);
    doomo_snprintf(buf, sizeof buf, "Endings found: %d / %d", ending_count_seen(), ENDING_MAIN_COUNT);
    ui_text_center(sw / 2, y + 36, buf, st->text);
    {
        const DoomoEnds *e = ending_state();
        int n = e ? e->times[C.k] : 0;
        doomo_snprintf(buf, sizeof buf, n > 1 ? "Reached %d times" : "First time!", n);
        ui_text_center(sw / 2, y + 50, buf, st->text_dim);
    }
    if (C.k < 4 && story[C.k].l1) {
        ui_text_center(sw / 2, y + 62, story[C.k].l1, st->text);
        ui_text_center(sw / 2, y + 74, story[C.k].l2, st->text);
    }
    if (C.t > 20 && ((C.t >> 4) & 1)) ui_text_center(sw / 2, y + 88, STR_ICON_A " Continue", st->text_dim);
}

const DoomoScene SCENE_ENDING_CARD = { "ENDING_CARD", ec_enter, NULL, ec_update, ec_draw, 0 };
