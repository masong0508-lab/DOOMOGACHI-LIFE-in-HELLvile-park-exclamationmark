/* lang_pick.c - SCENE_LANG_PICK: the first-boot language picker (U5; scenes.h).
 * Shown once, from the boot scene, on a machine that keeps a config record (PLATF_CONFIG) and has
 * none yet (opt_config_first_boot). UP / DOWN choose among the languages offered to players (the
 * registry's `ready` ones) and the choice is applied at once, so the prompt and the names switch as
 * you move; A or START keeps it, writes the machine record (so it never asks again) and goes to the
 * title. Skipped by test harnesses (PLAT_TEST_NO_PICKER, plat_ext.h).
 * The second screen is the Mature question (U5 amendment, user answer 20): Off / Censored / On,
 * starting on Off. Off goes straight on; Censored or On first asks a grown-up to set the security
 * code (SCENE_SECURITY_CODE, seccode_scene.c). Skipping the code leaves Mature Off: a choice nobody
 * confirmed does not stick. B goes back to the language. The 18+ confirm and the code check when
 * Mature is turned on later belong to E11.
 * State: 8 B. */
#include "doomo/doomo.h"
#include "doomo/lang.h"
#include "doomo/ui.h"
#include "doomo/audio.h"
#include "doomo/input.h"
#include "doomo/ending.h"
#include "doomo/options_store.h"
#include "doomo/platform.h"

enum { PH_LANG, PH_MATURE, PH_CODE };
static struct { int16_t t; uint8_t phase, row, note; } P;

static int ready_count(void)
{
    int i, n = 0;
    for (i = 0; i < LANG_COUNT; i++) n += doomo_lang_info[i].ready != 0;
    return n;
}

static int ready_nth(int n)
{
    int i;
    for (i = 0; i < LANG_COUNT; i++)
        if (doomo_lang_info[i].ready && n-- == 0) return i;
    return LANG_EN;
}

static int ready_index(int lang)
{
    int i, n = 0;
    for (i = 0; i < LANG_COUNT; i++) {
        if (i == lang) return n;
        n += doomo_lang_info[i].ready != 0;
    }
    return 0;
}

static void lp_enter(int arg)
{
    (void)arg;
    P.t = 0;
    P.phase = PH_LANG;
    P.row = MATURE_OFF;
    P.note = 0;
    audio_music(SONG_NONE);
}

static void finish(void)
{
    opt_config_save();                           /* the record exists from now on: never asked again */
    opt_config_first_boot_done();
    audio_sfx(SFX_SELECT);
    scene_set(&SCENE_TITLE, 0);
}

static void lp_update(void)
{
    int cur = lang_get();
    P.t++;
    if (P.phase == PH_CODE) {                    /* back from SCENE_SECURITY_CODE */
        if (seccode_scene_result()) { finish(); return; }
        opt_set(OPT_MATURE, MATURE_OFF);         /* no code, no Mature: ask again */
        P.phase = PH_MATURE;
        P.row = MATURE_OFF;
        P.note = 1;
        P.t = 0;
        return;
    }
    if (P.phase == PH_MATURE) {
        if (btn_pressed(BTN_UP) && P.row > MATURE_OFF) { P.row--; P.note = 0; audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_DOWN) && P.row < MATURE_ON) { P.row++; P.note = 0; audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_B)) { P.phase = PH_LANG; P.t = 0; audio_sfx(SFX_CANCEL); return; }
        if (P.t > 8 && (btn_pressed(BTN_A) || btn_pressed(BTN_START))) {
            opt_set(OPT_MATURE, P.row);
            if (P.row == MATURE_OFF) { finish(); return; }
            P.phase = PH_CODE;
            audio_sfx(SFX_SELECT);
            scene_push(&SCENE_SECURITY_CODE, SECCODE_SET);
        }
        return;
    }
    if (btn_pressed(BTN_UP) || btn_pressed(BTN_DOWN)) {
        opt_set(OPT_LANGUAGE, lang_next(cur, btn_pressed(BTN_DOWN) ? 1 : -1));
        audio_sfx(SFX_CURSOR);
    }
    if (P.t > 8 && (btn_pressed(BTN_A) || btn_pressed(BTN_START))) {
        P.phase = PH_MATURE;
        P.row = (uint8_t)(opt_mature() <= MATURE_ON ? opt_mature() : MATURE_OFF);
        P.t = 0;
        audio_sfx(SFX_SELECT);
    }
}

static void mature_draw(void)
{
    static const char *const names[3] = { "Off", "Censored", "On" };
    static const char *const hints[3] = { "Cutaways. Nothing spicy.", "Spicy bits get censor bars.", "Implied scenes, then a fade." };
    const UiStyle *st = ui_style(WIN_CUTE);
    int sw = plat_screen_w(), sh = plat_screen_h(), i, h = 98, y;
    ui_backdrop(WIN_CUTE, (uint32_t)doomo_frame_count());
    ui_window(16, sh / 2 - h / 2, sw - 32, h, WIN_CUTE);
    y = sh / 2 - h / 2 + 8;
    ui_text_center(sw / 2, y, "Grown-up content?", st->text);
    ui_text_center(sw / 2, y + 12, "Ask a grown-up first.", st->text_dim);
    y += 28;
    for (i = 0; i < 3; i++, y += 14) {
        ui_text_center(sw / 2, y, names[i], i == P.row ? st->text : st->text_dim);
        if (i == P.row) ui_cursor(sw / 2 - 56, y);
    }
    ui_text_center(sw / 2, y + 2, P.note ? "Without a code it stays Off." : hints[P.row], st->text_dim);
    if (P.t > 8 && ((P.t >> 4) & 1)) ui_text_center(sw / 2, sh / 2 + h / 2 - 12, STR_ICON_A " OK", st->text_dim);
}

static void lp_draw(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int sw = plat_screen_w(), sh = plat_screen_h(), n = ready_count(), cur = ready_index(lang_get());
    int i, h = 40 + n * 14, y;
    if (P.phase != PH_LANG) { mature_draw(); return; }
    ui_backdrop(WIN_CUTE, (uint32_t)doomo_frame_count());
    ui_window(16, sh / 2 - h / 2, sw - 32, h, WIN_CUTE);
    y = sh / 2 - h / 2 + 8;
    ui_text_center(sw / 2, y, "Choose your language", st->text);
    y += 20;
    for (i = 0; i < n; i++, y += 14) {
        ui_text_center(sw / 2, y, doomo_lang_info[ready_nth(i)].name, i == cur ? st->text : st->text_dim);
        if (i == cur) ui_cursor(sw / 2 - 56, y);
    }
    if (P.t > 8 && ((P.t >> 4) & 1)) ui_text_center(sw / 2, sh / 2 + h / 2 - 12, STR_ICON_A " OK", st->text_dim);
}

const DoomoScene SCENE_LANG_PICK = { "LANG_PICK", lp_enter, NULL, lp_update, lp_draw, 0 };
