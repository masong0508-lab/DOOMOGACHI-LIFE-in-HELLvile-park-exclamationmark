/* fp_options.c - SCENE_OPTIONS (owner: freeplay).
 *
 * Generated from doomo_option_info[]: one tab per page (Game, Shooter,
 * Life Sim, Audio, Display, Accessibility, Cheats), a value row per
 * option, a one-line explanation of the selected one. Values that are
 * still locked (window skins, title themes, cursors, filters, crosshairs:
 * UL_CAT_SKIN; cheats: UL_CAT_CHEAT) show as "???" with their unlock hint
 * and are skipped. Monster Mode can only be switched ON after typing
 * "I AM A MONSTER" and confirming. Mature (E11, last row of the Game page) can be
 * lowered freely; raising it asks for the grown-ups' code when one is set, then
 * for an 18+ confirm. arg = the page to open on.
 *
 * U2: the first page is "Quick" (language, music, sound, text speed, difficulty, gore and a
 * Preset row), so the common changes are one press away; Loss (Life Sim) and Rumble (Controls,
 * only where there is a motor) have rows; a preset sets many options at once.
 */
#include "fp_int.h"
#include "doomo/options_store.h"
#include "doomo/ending.h"
#include "doomo/plat_ext.h"
#include "doomo/tips.h"

#define ROWS_MAX 24
#define ROW_PRESET 100          /* U2: the Quick page's preset row (not an option id) */
#define ROW_TIPS_RESET 101      /* CL4: "Show all tips again" (Game page, under Tips) */
/* CL4: the row's words are named arrays: as literals of opt_draw() they would land in a string section
 * that the resident pause_menu_draw() shares, so on the PS1 they would stay in resident RAM */
static const char tips_reset_label[] = "Show all tips again";
static const char tips_reset_help[] = "Every first-time tip shows again, once, the next time you reach its place.";
static const char tips_reset_hint[] = STR_ICON_A " Reset  L/R Page  " STR_ICON_B " Done";

static const char *const opt_help[OPT_COUNT] = {
    /* GORE */ "Cute: confetti, hearts, plush. Brutal: full Doom. Cute + Gore (after the ending): both at once.",
    /* MONSTER */ "Lets you kill off ANY citizen. Everyone will remember.",
    /* DIFFICULTY */ "How mean the demons are. Plushie is extra gentle.",
    /* MUSIC_VOL */ "Volume of the chiptune soundtrack.",
    /* SFX_VOL */ "Volume of sound effects.",
    /* VOICE_VOL */ "Volume of the citizens' babbling voices.",
    /* VOICE_BABBLE */ "Citizens babble Tomodachi-style while they talk.",
    /* TEXT_SPEED */ "How fast dialogue types out.",
    /* SIM_SPEED */ "How fast time flows on the island.",
    /* DAY_LENGTH */ "How long one island day lasts in real time.",
    /* AUTOSAVE */ "Save the free play island automatically.",
    /* CAMERA */ "Default camera in the shooter (SELECT toggles).",
    /* CONTROLS */ "Classic: L + D-pad looks. Modern: L/R strafe. Dual stick: left moves, right looks.",
    /* INVERT */ "Invert looking up and down.",
    /* LOOK_SPEED */ "How fast you turn and look around.",
    /* AIM_ASSIST */ "Gentle help keeping demons in your sights.",
    /* LOCK_ON */ "Hold or toggle the soft lock-on.",
    /* CROSSHAIR */ "The shape of your crosshair.",
    /* CROSSHAIR_COLOR */ "The colour of your crosshair.",
    /* FOV */ "How wide the 3D view is.",
    /* HEAD_BOB */ "How much the view bobs as you run.",
    /* SCREEN_SHAKE */ "Shake on hits and explosions.",
    /* DAMAGE_NUMBERS */ "Pop-up numbers when you hit demons.",
    /* HIT_MARKERS */ "A little X when a shot connects.",
    /* HUD_LAYOUT */ "How the shooter's health and ammo look. Hidden: crosshair only.",
    /* SHOULDER */ "Which shoulder the third-person camera sits on.",
    /* BLOOD_COLOR */ "Red, green, rainbow... or just confetti.",
    /* CONFETTI */ "How much confetti cute kills make.",
    /* DEMON_DENSITY */ "How many demons crowd each fight.",
    /* POSSESSION_RATE */ "How often citizens get possessed at night.",
    /* EVENT_RATE */ "How much drama the island gets up to.",
    /* WEATHER */ "Rain, snow and petals on the island map.",
    /* LCD_FILTER */ "The Boreagotchi's green LCD screen effect.",
    /* WINDOW_SKIN */ "The colour of every window. Earn more!",
    /* FONT_STYLE */ "Bold or shadowed letters, easier to read.",
    /* SUBTITLES */ "Show text for voices and sounds.",
    /* COLORBLIND */ "Shift colours that are hard to tell apart.",
    /* REDUCE_FLASH */ "Tone down bright flashes.",
    /* PET_NOTIFICATIONS */ "Your Boreagotchi calls you when it needs you.",
    /* NEWS_TICKER */ "Scrolling news on the island map.",
    /* CLOCK_24H */ "12-hour or 24-hour clock.",
    /* TITLE_THEME */ "The title screen's look. Earn more themes!",
    /* CURSOR */ "The bouncy menu cursor.",
    /* CAMERA_FILTER */ "A filter over the shooter's 3D view.",
    /* UI_BOUNCE */ "How boingy windows and menus are.",
    /* BIG_HEAD */ "Everyone's heads are huge. Huger.",
    /* CONFETTI_ALL */ "Everything explodes into confetti.",
    /* INFINITE_CANDY */ "Feeding and gifts never use up items.",
    /* CUTE_DEMONS */ "Demons stay adorable even in the invasion.",
    /* BRUTAL_RESIDENTS */ "Citizens fight back. Brutally.",
    /* LOW_GRAV */ "Floaty jumps and floaty gibs.",
    /* PAPER_MODE */ "Everything is a flat paper cut-out.",
    /* GOD_MODE */ "Plush armor: you can't be hurt.",
    /* RICH */ "Spending never empties your wallet.",
    /* DISCO */ "Disco lights everywhere.",
    /* TINY_DEMONS */ "Demons shrink to pocket size.",
    /* ALL_GHOSTS */ "Every citizen looks like a ghost.",
    /* COMBAT_FEEL */ "PS1: Punchy = heavy recoil, shake, snappy moves. Brutal adds gore, gibs and hit-stop.",
    /* TURN_SWAP */ "Modern controls: which way R1 turns (R2/L1 turn the other way).",
    /* EFFECTS */ "Low: fewer, shorter-lived gibs and confetti. Smoother in big fights.",
    /* DRAW_DIST */ "How far the 3D view reaches. Near is faster in big open maps.",
    /* SHOW_FPS */ "Show how many frames per second the shooter draws.",
    /* LANGUAGE */ "The language of menus and story text. Cambia el idioma del juego.",
    /* 3D_FAST */ "Faster: half-resolution 3D, scaled up, smoother. Better: full resolution, up to 20 fps."
};

/* U2: presets. Applying one first puts every ordinary option back to its default, then lays the
 * list over it. Language, volumes, Mature, Monster Mode, cheats, skins and themes, and the
 * Performance page are never touched. */
typedef struct { uint8_t opt, val; } PresetSet;
static const PresetSet pre_comfort[] = {
    { OPT_DIFFICULTY, 0 }, { OPT_GORE, GORE_CUTE }, { OPT_AIM_ASSIST, 3 }, { OPT_LOCK_ON, 1 },
    { OPT_SCREEN_SHAKE, 1 }, { OPT_HEAD_BOB, 1 }, { OPT_REDUCE_FLASH, 1 }, { OPT_UI_BOUNCE, 1 },
    { OPT_POSSESSION_RATE, 0 }, { OPT_EVENT_RATE, 0 }, { OPT_SUBTITLES, 1 }
};
static const PresetSet pre_classic[] = {
    { OPT_GORE, GORE_BRUTAL }, { OPT_DIFFICULTY, 2 }, { OPT_CONTROL_SCHEME, CONTROLS_CLASSIC },
    { OPT_COMBAT_FEEL, 2 }, { OPT_CAMERA_DEFAULT, 0 }, { OPT_HUD_LAYOUT, 1 }, { OPT_CROSSHAIR, 1 },
    { OPT_AIM_ASSIST, 0 }, { OPT_LOCK_ON, 0 }, { OPT_DAMAGE_NUMBERS, 0 }, { OPT_HIT_MARKERS, 0 },
    { OPT_HEAD_BOB, 2 }
};
static const PresetSet pre_cozy[] = {
    { OPT_GORE, GORE_CUTE }, { OPT_DIFFICULTY, 0 }, { OPT_SIM_SPEED, 0 }, { OPT_AUTOSAVE, 2 },
    { OPT_POSSESSION_RATE, 1 }, { OPT_EVENT_RATE, 0 }, { OPT_WEATHER, 2 }, { OPT_LOSS, 0 },
    { OPT_PET_NOTIFICATIONS, 1 }, { OPT_NEWS_TICKER, 1 }, { OPT_CONFETTI_AMOUNT, 1 }
};
static const PresetSet pre_hard[] = {
    { OPT_DIFFICULTY, 3 }, { OPT_GORE, GORE_BRUTAL }, { OPT_AIM_ASSIST, 0 }, { OPT_LOCK_ON, 0 },
    { OPT_DAMAGE_NUMBERS, 0 }, { OPT_HIT_MARKERS, 0 }, { OPT_HUD_LAYOUT, 2 }, { OPT_DEMON_DENSITY, 2 },
    { OPT_POSSESSION_RATE, 3 }, { OPT_EVENT_RATE, 2 }, { OPT_AUTOSAVE, 0 }, { OPT_LOSS, 1 },
    { OPT_COMBAT_FEEL, 2 }, { OPT_CROSSHAIR, 0 }, { OPT_SIM_SPEED, 2 }
};
static const PresetSet pre_access[] = {
    { OPT_SUBTITLES, 1 }, { OPT_REDUCE_FLASH, 1 }, { OPT_SCREEN_SHAKE, 0 }, { OPT_HEAD_BOB, 0 },
    { OPT_FONT_STYLE, 1 }, { OPT_UI_BOUNCE, 0 }, { OPT_AIM_ASSIST, 3 }, { OPT_LOCK_ON, 1 },
    { OPT_LCD_FILTER, 0 }
};
#define PRESET_N 6
static const char *const preset_name[PRESET_N] = {
    "Default", "Comfort", "Classic DOOM", "Cozy life", "Hardcore", "Max accessibility"
};
static const char *const preset_help[PRESET_N] = {
    "Every ordinary setting back to how the game ships. A confirm asks first.",
    "Gentle: Plushie, cute gore, strong aim help, less shake and bobbing, calm island.",
    "Brutal gore, Nightmare, Classic controls, no aim help, classic HUD and crosshair.",
    "Chill island: plushie, slow sim, rare possessions, weather, no family loss.",
    "Ultra-Hellvile, brutal, no aim help, minimal HUD, swarms, chaos, loss on.",
    "Subtitles, no flashing, no shake, bold text, strong aim help, still menus."
};
static const PresetSet *const preset_set[PRESET_N] = { 0, pre_comfort, pre_classic, pre_cozy, pre_hard, pre_access };
static const uint8_t preset_len[PRESET_N] = {
    0, DOOMO_COUNTOF(pre_comfort), DOOMO_COUNTOF(pre_classic), DOOMO_COUNTOF(pre_cozy),
    DOOMO_COUNTOF(pre_hard), DOOMO_COUNTOF(pre_access)
};

static struct {
    int page, sel, top, t, preset;
    int rows[ROWS_MAX], nrows;
    char typed[24];
    uint8_t phase, pend;          /* E11: waiting on the code scene; the Mature level asked for */
} O;

enum { PH_NONE, PH_CODE };

static const DoomoOptionInfo *info_of(int opt)
{
    return opt >= OPT2_FIRST ? &doomo_option2_info[opt - OPT2_FIRST] : &doomo_option_info[opt];
}

/* C3: the rows that name buttons say the buttons of the pad in your hands (input.h input_pad) */
static const char *const help_controls[PAD_COUNT] = {
    "Classic: hold L to strafe and look. Modern: D-pad strafes, R / L turn. Dual stick: two sticks.",
    "Classic: hold L to strafe and look. Modern: D-pad strafes, R / L turn.",
    "Classic: hold L1 to strafe/look. Modern: D-pad strafes, R1/R2 turn. Dual stick: two sticks.",
    "Classic: hold L to strafe and look. Modern: D-pad strafes, R / L turn."
};
static const char *const help_turn[PAD_COUNT] = {
    "Modern controls: Normal = R turns right, R2 / L left. Swapped turns the other way.",
    "Modern controls: Normal = R turns right, L left. Swapped turns the other way.",
    "Modern controls: Normal = R1 turns right, R2 / L1 left. Swapped turns the other way.",
    "Modern controls: Normal = R turns right, L left. Swapped turns the other way."
};
static const char *const help_camera[PAD_COUNT] = {
    "Default camera in the shooter (SEL toggles).", "Default camera in the shooter (SEL toggles).",
    "Default camera in the shooter (SELECT toggles).", "Default camera in the shooter (START+Y toggles)."
};

static const char *help_of(int opt)
{
    if (opt == OPT_CONTROL_SCHEME) return help_controls[input_pad()];
    if (opt == OPT_TURN_SWAP) return help_turn[input_pad()];
    if (opt == OPT_CAMERA_DEFAULT) return help_camera[input_pad()];
    if (opt == OPT_LOSS) return "Family members can be lost over a life. Off: nobody is ever lost.";
    if (opt == OPT_RUMBLE) return "Controller rumble, apart from Screen shake.";
    if (opt == OPT_TIPS) return "A short card the first time you reach a new place: what it is and which buttons do what.";
    if (opt == OPT_MATURE)
        /* (CL5 land) three lines at 240x160 (it took five, and the help pill shows three) */
        return "Off: cutaways. Censored: bars. On: implied, then a fade. Going up asks your age (and the code, if set).";
    return opt < OPT_COUNT && opt_help[opt] ? opt_help[opt] : "";
}

static int value_ok(int opt, int v)
{
    if (opt == OPT_MATURE) return v <= MATURE_ON;               /* the pack level comes with a pack */
    if (opt == OPT_LANGUAGE) return doomo_lang_info[v].ready;    /* lang.h: offered languages */
    if (opt >= OPT_CHEAT_BIG_HEAD && opt <= OPT_CHEAT_ALL_GHOSTS && v > 0)
        return unlock_ok(UL_CAT_CHEAT, 0, opt);
    return unlock_ok(UL_CAT_SKIN, opt, v);
}

/* U2: the Quick page lists these (the same options as on their own pages), then the preset row. */
static const uint8_t quick_rows[] = { OPT_LANGUAGE, OPT_MUSIC_VOL, OPT_SFX_VOL, OPT_TEXT_SPEED,
                                      OPT_DIFFICULTY, OPT_GORE };

static void build_rows(void)
{
    int i;
    O.nrows = 0;
    if (O.page == OPTPAGE_QUICK) {
        for (i = 0; i < (int)DOOMO_COUNTOF(quick_rows); i++) O.rows[O.nrows++] = quick_rows[i];
        O.rows[O.nrows++] = ROW_PRESET;
        O.sel = DOOMO_MIN(O.sel, O.nrows);
        return;
    }
    if (O.page == OPTPAGE_GAME) O.rows[O.nrows++] = OPT_LANGUAGE;      /* first row */
    for (i = 0; i < OPT_COUNT && O.nrows < ROWS_MAX; i++)
        if (doomo_option_info[i].page == O.page && i != OPT_LANGUAGE &&
            (i != OPT_DRAW_DIST || input_feel()) && (i != OPT_3D_FAST || plat_name()[0] == 'G'))
            O.rows[O.nrows++] = i;       /* draw distance: PS1 3D only; 3D quality: GBA only */
    for (i = OPT_LOSS; i < OPT2_END && O.nrows < ROWS_MAX; i++)          /* U2: Loss (Life Sim), Rumble */
        if (doomo_option2_info[i - OPT2_FIRST].page == O.page &&
            (i != OPT_RUMBLE || (plat_features() & PLATF_RUMBLE)))
            O.rows[O.nrows++] = i;
    if (O.page == OPTPAGE_GAME && O.nrows < ROWS_MAX) O.rows[O.nrows++] = ROW_TIPS_RESET;  /* CL4 */
    if (O.page == OPTPAGE_GAME && O.nrows < ROWS_MAX) O.rows[O.nrows++] = OPT_MATURE;   /* E11: last row */
    O.sel = DOOMO_MIN(O.sel, O.nrows);       /* last row = "Defaults" */
}

static void opt_enter(int arg)
{
    memset(&O, 0, sizeof(O));
    O.page = DOOMO_CLAMP(arg, 0, OPTPAGE_COUNT - 1);
    build_rows();
}

static void cb_monster_final(int yes)
{
    if (!yes) return;
    opt_set(OPT_MONSTER_MODE, 1);
    audio_sfx(SFX_DEMON_ROAR);
    ui_toast(STR_ICON_SKULL " Monster Mode is ON.");
}

static int monster_ok(const char *s)
{
    static const char want[] = "IAMAMONSTER";
    int i = 0;
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c == ' ') continue;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (!want[i] || c != want[i]) return 0;
        i++;
    }
    return want[i] == 0;
}

static void cb_monster_typed(int ok)
{
    if (!ok) return;
    if (!monster_ok(O.typed)) {
        audio_sfx(SFX_ERROR);
        ui_toast("Phew. Monster Mode stays off.");
        return;
    }
    ui_confirm("Last chance! With Monster Mode on you can kill off innocent citizens. Turn it on?",
               cb_monster_final);
}

/* E11: lowering Mature is free; raising it asks for the code (when one is set), then for 18+. */
static void cb_adult(int yes)
{
    if (yes) {
        opt_set(OPT_MATURE, O.pend);
        audio_sfx(SFX_SELECT);
        ui_toast("Mature scenes are on.");
    } else {
        audio_sfx(SFX_CANCEL);
        ui_toast("Mature stays as it was.");
    }
}

static void change_mature(int d)
{
    int v = opt_get(OPT_MATURE), nv;
    if (v > MATURE_ON) v = MATURE_ON;
    nv = (v + 3 + d) % 3;
    if (nv <= v) {                                   /* down (or the wrap to Off): nothing to ask */
        opt_set(OPT_MATURE, nv);
        audio_sfx(SFX_CURSOR);
        return;
    }
    O.pend = (uint8_t)nv;
    audio_sfx(SFX_CURSOR);
    if (seccode_is_set()) {
        O.phase = PH_CODE;
        scene_push(&SCENE_SECURITY_CODE, SECCODE_ASK);
    } else {
        ui_confirm("Mature scenes are for adults. Are you 18 or older?", cb_adult);
    }
}

static void change(int opt, int d)
{
    const DoomoOptionInfo *oi = info_of(opt);
    if (opt == OPT_MATURE) { change_mature(d); return; }
    int v = opt_get(opt), k;
    if (opt == OPT_MONSTER_MODE) {
        if (v) { opt_set(opt, 0); audio_sfx(SFX_HAPPY); ui_toast(STR_ICON_HEART " Monster Mode is off. Phew!"); }
        else {
            O.typed[0] = 0;
            audio_sfx(SFX_POSSESS);
            ui_text_entry("Type I AM A MONSTER to enable", O.typed, sizeof(O.typed), cb_monster_typed);
        }
        return;
    }
    for (k = 0; k < oi->count; k++) {
        v = (v + oi->count + d) % oi->count;
        if (value_ok(opt, v)) break;
    }
    opt_set(opt, v);
    if (opt == OPT_SFX_VOL || opt == OPT_VOICE_VOL) audio_sfx(SFX_BLIP);
    else audio_sfx(SFX_CURSOR);
}

static void cb_defaults(int yes)
{
    int i;
    if (!yes) return;
    for (i = 0; i < O.nrows; i++) {
        int opt = O.rows[i];
        if (opt == OPT_MONSTER_MODE || opt == OPT_LANGUAGE || opt == ROW_PRESET || opt == ROW_TIPS_RESET)
            continue;                                                                       /* never silently */
        opt_set(opt, info_of(opt)->def);
    }
    audio_sfx(SFX_SPARKLE);
    ui_toast("Page reset to defaults.");
}

/* U2: the options a preset may change: not language, volumes, Mature, Monster Mode, cheats, skins and
 * themes, cursor, camera filter, nor anything on the Performance page. */
static int preset_may(int opt)
{
    const DoomoOptionInfo *oi = info_of(opt);
    if (oi->page == OPTPAGE_CHEATS || oi->page == OPTPAGE_PERF) return 0;
    switch (opt) {
    case OPT_MONSTER_MODE: case OPT_LANGUAGE: case OPT_MUSIC_VOL: case OPT_SFX_VOL: case OPT_VOICE_VOL:
    case OPT_MATURE: case OPT_WINDOW_SKIN: case OPT_TITLE_THEME: case OPT_CURSOR: case OPT_CAMERA_FILTER:
    case OPT_TIPS:
        return 0;
    }
    return 1;
}

static void preset_apply(int n)
{
    int i;
    if (n < 0 || n >= PRESET_N) return;
    for (i = 0; i < OPT_COUNT; i++)
        if (preset_may(i)) opt_set(i, info_of(i)->def);
    for (i = OPT2_FIRST; i < OPT2_END; i++)
        if (preset_may(i)) opt_set(i, info_of(i)->def);
    for (i = 0; i < preset_len[n]; i++) {
        const PresetSet *s = &preset_set[n][i];
        if (preset_may(s->opt) && value_ok(s->opt, s->val)) opt_set(s->opt, s->val);
    }
}

static void cb_preset(int yes)
{
    if (!yes) return;
    preset_apply(O.preset);
    audio_sfx(SFX_SPARKLE);
    ui_toast("Preset applied.");
}

/* CL4 */
static void cb_tips_reset(int yes)
{
    if (!yes) return;
    tips_reset();
    audio_sfx(SFX_SPARKLE);
    ui_toast("Every tip will show again.");
}

/* (CL5, C13 1.14a) rows on screen: 7 at 240x160 (was 8), so the help under them has up to three whole lines
 * instead of a one-line marquee whose start ("Classic: L + D-pad looks. Modern: L/") was all a reader caught */
static int opt_vis(void) { return fp_big() ? 13 : 7; }
#define OPT_HELP_LINES 3

static void opt_update(void)
{
    int vis = opt_vis();
    O.t++;
    if (fp_is_freeplay()) fp_live(); else fp_fx_tick();
    if (O.phase == PH_CODE) {                           /* back from SCENE_SECURITY_CODE */
        O.phase = PH_NONE;
        if (seccode_scene_result()) ui_confirm("Mature scenes are for adults. Are you 18 or older?", cb_adult);
        else { audio_sfx(SFX_ERROR); ui_toast("Mature stays as it was."); }
        return;
    }
    if (btn_pressed(BTN_L | BTN_R)) {
        O.page = (O.page + (btn_pressed(BTN_L) ? OPTPAGE_COUNT - 1 : 1)) % OPTPAGE_COUNT;
        O.sel = O.top = 0;
        build_rows();
        audio_sfx(SFX_BLIP);
        return;
    }
    if (btn_repeat(BTN_UP)) { O.sel = (O.sel + O.nrows) % (O.nrows + 1); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { O.sel = (O.sel + 1) % (O.nrows + 1); audio_sfx(SFX_CURSOR); }
    if (O.sel < O.top) O.top = O.sel;
    if (O.sel >= O.top + vis) O.top = O.sel - vis + 1;
    if (O.sel < O.nrows && O.rows[O.sel] == ROW_PRESET) {        /* U2: pick with LEFT/RIGHT, A applies */
        if (btn_repeat(BTN_LEFT)) { O.preset = (O.preset + PRESET_N - 1) % PRESET_N; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_RIGHT)) { O.preset = (O.preset + 1) % PRESET_N; audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_A))
            ui_confirm("Apply this preset? Language, volumes, Mature and Monster Mode stay as they are.", cb_preset);
    } else if (O.sel < O.nrows && O.rows[O.sel] == ROW_TIPS_RESET) {     /* CL4: A asks, then resets */
        if (btn_pressed(BTN_A)) ui_confirm("Show every first-time tip again?", cb_tips_reset);
    } else if (O.sel < O.nrows) {
        if (btn_repeat(BTN_LEFT)) change(O.rows[O.sel], -1);
        if (btn_repeat(BTN_RIGHT) || btn_pressed(BTN_A)) change(O.rows[O.sel], 1);
    } else if (btn_pressed(BTN_A)) {
        ui_confirm("Reset this page to its default settings?", cb_defaults);
    }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop();
        else scene_set(&SCENE_TITLE, 0);
    }
}

static void opt_draw(void)
{
    int sw = fp_sw(), big = fp_big(), vis = opt_vis(), i;
    int style = O.page == OPTPAGE_CHEATS ? WIN_HELL : WIN_CUTE;
    const UiStyle *st = ui_style(style);
    int x = 10, w = sw - 20, y0 = 36;
    char val[40];
    ui_backdrop(style, (uint32_t)fp_t());
    ui_title(sw / 2, 3, "Options", style);
    {
        /* page tabs: show a window of them around the current page */
        int first = O.page > 2 ? O.page - 2 : 0, count = big ? 5 : 3, k;
        const char *names[OPTPAGE_COUNT];
        if (first + count > OPTPAGE_COUNT) first = OPTPAGE_COUNT - count;
        for (k = 0; k < count; k++) names[k] = doomo_option_page_names[first + k];
        ui_tabs(x, 21, names, count, O.page - first, style);
        if (first > 0) plat_text(2, 22, STR_ICON_ARROW_L, st->accent);
        if (first + count < OPTPAGE_COUNT) plat_text(sw - 8, 22, STR_ICON_ARROW_R, st->accent);
    }
    ui_window(x - 4, y0 - 4, w + 8, vis * 11 + 6, style);
    for (i = 0; i < vis && O.top + i <= O.nrows; i++) {
        int row = O.top + i, y = y0 + i * 11;
        if (row == O.nrows) {
            ui_value_row(x + 12, y, w - 18, STR_ICON_STAR " Reset page to defaults", "", row == O.sel);
            continue;
        }
        if (O.rows[row] == ROW_PRESET) {
            ui_value_row(x + 12, y, w - 18, "Preset", preset_name[O.preset], row == O.sel);
            continue;
        }
        if (O.rows[row] == ROW_TIPS_RESET) {
            ui_value_row(x + 12, y, w - 18, tips_reset_label, "", row == O.sel);
            continue;
        }
        {
            int opt = O.rows[row], v = opt_get(opt);
            const DoomoOptionInfo *oi = info_of(opt);
            /* a value name is passed as it is (constant text: lang_tr()
             * remembers its translation, L1); numbers are built in val */
            const char *shown = oi->values ? oi->values[v] : val;
            if (!oi->values) doomo_snprintf(val, sizeof(val), "%d", v);
            if (opt >= OPT_MUSIC_VOL && opt <= OPT_VOICE_VOL) {
                int k;
                val[0] = 0;
                for (k = 0; k < 10; k++) val[k] = k < v ? '|' : '.';
                val[10] = 0;
                shown = val;
            }
            ui_value_row(x + 12, y, w - 18, oi->name, shown, row == O.sel);
            if (opt == OPT_MONSTER_MODE && v) fp_icon(x + w - 10, y, ICON_SKULL, PAL_UI_RED);
        }
    }
    if (O.nrows + 1 > vis) ui_scroll_arrows(x + w, y0 - 6, y0 + vis * 11 - 2, O.top > 0, O.top + vis <= O.nrows);
    /* help line (or what the next locked value needs) */
    {
        const char *help = "Put every setting on this page back.";
        uint8_t c = st->text_dim;
        int hy = y0 + vis * 11 + (big ? 6 : 3);     /* (CL5) 240x160: three lines end above the bottom bar */
        if (O.sel < O.nrows && O.rows[O.sel] == ROW_PRESET) {
            help = preset_help[O.preset];
        } else if (O.sel < O.nrows && O.rows[O.sel] == ROW_TIPS_RESET) {
            help = tips_reset_help;
        } else if (O.sel < O.nrows) {
            int opt = O.rows[O.sel], v = opt_get(opt), k;
            const DoomoOptionInfo *oi = info_of(opt);
            help = help_of(opt);
            for (k = 1; k < oi->count; k++) {
                int nv = (v + k) % oi->count;
                if (!value_ok(opt, nv)) {
                    const DoomoUnlock *u = opt >= OPT_CHEAT_BIG_HEAD ? unlock_find(UL_CAT_CHEAT, 0, opt)
                                                                    : unlock_find(UL_CAT_SKIN, opt, nv);
                    if (u && u->how && k == 1) {
                        doomo_snprintf(fp_tmp, sizeof(fp_tmp), "Next: ??? (%s)", u->how);
                        help = fp_tmp;
                        c = PAL_UI_GOLD;
                    }
                    break;
                }
            }
        }
        {
            /* wrapped on up to OPT_HELP_LINES lines, the pill as tall as the text (the 320x240 pill was 22 px for
             * any text, and the Controls help ran out under it) */
            int lines = DOOMO_CLAMP(ui_text_wrap(0, 0, w - 4, help, 0, 0), 1, OPT_HELP_LINES);
            fp_pill(x - 4, hy, w + 8, 4 + lines * 9, st->fill_hi, st->outline, PAL_WHITE);
            fp_wrap_lines(x + 2, hy + 2, w - 4, help, c, lines);
        }
    }
    ui_hint((O.sel < O.nrows && O.rows[O.sel] == ROW_PRESET) ? STR_ICON_ARROW_L STR_ICON_ARROW_R " Pick  " STR_ICON_A " Apply  L/R Page  " STR_ICON_B " Done"
            : (O.sel < O.nrows && O.rows[O.sel] == ROW_TIPS_RESET) ? tips_reset_hint
                                                         : STR_ICON_ARROW_L STR_ICON_ARROW_R " Change  L/R Page  " STR_ICON_B " Done", style);
    fp_fx_draw();
}

/* ---- the shared pause menu -------------------------------------------------- */
int pause_menu_nav(int *sel, int n)
{
    int d = btn_repeat(BTN_UP) ? -1 : btn_repeat(BTN_DOWN) ? 1 : 0;
    if (!d || n <= 0) return 0;
    *sel = (*sel + n + d) % n;
    audio_sfx(SFX_CURSOR);
    return 1;
}

const char *pause_music_label(char *buf, int n)
{
    int v = opt_get(OPT_MUSIC_VOL);
    if (v <= 0) doomo_snprintf(buf, (size_t)n, "Music: off");
    else doomo_snprintf(buf, (size_t)n, "Music: %d", v);
    return buf;
}

int pause_music_nudge(int dir)
{
    int v = opt_get(OPT_MUSIC_VOL), nv = v + (dir < 0 ? -1 : 1);
    if (nv < 0) nv = 0;
    if (nv > doomo_option_info[OPT_MUSIC_VOL].count - 1) nv = doomo_option_info[OPT_MUSIC_VOL].count - 1;
    if (nv == v) return 0;
    opt_set(OPT_MUSIC_VOL, nv);
    audio_sfx(SFX_CURSOR);
    return 1;
}

void pause_menu_draw(const char *const *labels, int n, int sel, unsigned lr_mask, int t, int style)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), big = sh >= 200;
    int w = big ? 200 : 176, row = big ? 18 : 16, h = n * row + 24;
    int x = sw / 2 - w / 2, y = sh / 2 - h / 2 + 2, i;
    char buf[40];
    plat_shade_rect(0, 0, sw, sh, 7);
    if (!ui_window_pop(x, y, w, h, style, t)) return;
    ui_title(sw / 2, y - 6, "PAUSED", style);
    for (i = 0; i < n; i++) {
        int by = y + 12 + i * row, k;
        doomo_snprintf(buf, sizeof(buf), "%s", labels[i]);
        for (k = (int)strlen(buf); k > 3 && ui_text_width(buf) > w - 44; k--) {
            buf[k - 3] = '.'; buf[k - 2] = '.'; buf[k - 1] = 0;
        }
        ui_button(x + 16, by, w - 32, buf, i == sel, style);
        if (i == sel && (lr_mask & (1u << i))) {
            ui_text(x + 6 + ui_wave(t, 16, 1), by + 3, STR_ICON_ARROW_L, PAL_UI_GOLD);
            ui_text(x + w - 12 - ui_wave(t, 16, 1), by + 3, STR_ICON_ARROW_R, PAL_UI_GOLD);
        }
    }
    ui_hint((lr_mask & (1u << sel)) ? STR_ICON_A " Select  " STR_ICON_ARROW_L STR_ICON_ARROW_R " Change  " STR_ICON_B " Resume"
                                     : STR_ICON_A " Select  " STR_ICON_B " Resume", style);
}

void options_open(int page)
{
    audio_sfx(SFX_SELECT);
    scene_push(&SCENE_OPTIONS, page);
}

/* (CL4, PS1 RAM) The scene's name is a named array, not a literal: the resident scene struct would
 * otherwise pull this file's whole string section (the help lines and value names, 3.4 KB) into resident RAM on the PS1
 * (ovl_plan.py; lib_help.c does the same). */
static const char options_scene_name[] = "OPTIONS";
const DoomoScene SCENE_OPTIONS = { options_scene_name, opt_enter, NULL, opt_update, opt_draw, 0 };

/* (CL5, clarity C13) the text fit of the Options help lines: how many option help texts (every option, the
 * machine-wide ones from OPT2_FIRST too, every preset, the tips reset row) need more lines than the help pill has
 * at the current size; the first is copied to why */
int fp_options_help_overflow(char *why, int cap)
{
    int i, bad = 0, w = fp_sw() - 20 - 4, n2 = OPT2_END - OPT2_FIRST;
    if (why && cap) why[0] = 0;
    for (i = 0; i < OPT_COUNT + n2 + PRESET_N + 1; i++) {
        const char *h = i < OPT_COUNT ? help_of(i) : i < OPT_COUNT + n2 ? help_of(OPT2_FIRST + i - OPT_COUNT)
                      : i < OPT_COUNT + n2 + PRESET_N ? preset_help[i - OPT_COUNT - n2] : tips_reset_help;
        int n = h ? ui_text_wrap(0, 0, w, h, 0, 0) : 0;
        if (n > OPT_HELP_LINES) {
            int l = why ? (int)strlen(why) : 0;
            bad++;
            if (why && l < cap - 8) doomo_snprintf(why + l, cap - l, "%s%d lines: %s", l ? " | " : "", n, h);
        }
    }
    return bad;
}
