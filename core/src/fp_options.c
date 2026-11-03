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
 */
#include "fp_int.h"
#include "doomo/options_store.h"
#include "doomo/ending.h"

#define ROWS_MAX 24

static const char *const opt_help[OPT_COUNT] = {
    /* GORE */ "Cute: confetti, hearts, plush stuffing. Brutal: full Doom. Cute + Gore (after the ending): both at once.",
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
    /* CONTROLS */ "Classic: L + D-pad looks. Modern: L/R strafe. Dual stick: left stick moves, right stick looks (DualShock).",
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
    /* LCD_FILTER */ "The Doomogachi's green LCD screen effect.",
    /* WINDOW_SKIN */ "The colour of every window. Earn more!",
    /* FONT_STYLE */ "Bold or shadowed letters, easier to read.",
    /* SUBTITLES */ "Show text for voices and sounds.",
    /* COLORBLIND */ "Shift colours that are hard to tell apart.",
    /* REDUCE_FLASH */ "Tone down bright flashes.",
    /* PET_NOTIFICATIONS */ "Your Doomogachi calls you when it needs you.",
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
    /* 3D_FAST */ "Faster: the 3D view at half resolution, scaled up, for a smoother shooter. Better: full resolution, up to 20 fps."
};

static struct {
    int page, sel, top, t;
    int rows[ROWS_MAX], nrows;
    char typed[24];
    uint8_t phase, pend;          /* E11: waiting on the code scene; the Mature level asked for */
} O;

enum { PH_NONE, PH_CODE };

static const DoomoOptionInfo *info_of(int opt)
{
    return opt >= OPT2_FIRST ? &doomo_option2_info[opt - OPT2_FIRST] : &doomo_option_info[opt];
}

static const char *help_of(int opt)
{
    if (opt == OPT_MATURE)
        return "Adult scenes. Off: cutaways. Censored: censor bars. On: implied, then a fade. Going up asks for the grown-ups' code (if set) and your age.";
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

static void build_rows(void)
{
    int i;
    O.nrows = 0;
    if (O.page == OPTPAGE_GAME) O.rows[O.nrows++] = OPT_LANGUAGE;      /* first row */
    for (i = 0; i < OPT_COUNT && O.nrows < ROWS_MAX; i++)
        if (doomo_option_info[i].page == O.page && i != OPT_LANGUAGE &&
            (i != OPT_DRAW_DIST || input_feel()) && (i != OPT_3D_FAST || plat_name()[0] == 'G'))
            O.rows[O.nrows++] = i;       /* draw distance: PS1 3D only; 3D quality: GBA only */
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
        if (opt == OPT_MONSTER_MODE || opt == OPT_LANGUAGE) continue;   /* never silently */
        opt_set(opt, info_of(opt)->def);
    }
    audio_sfx(SFX_SPARKLE);
    ui_toast("Page reset to defaults.");
}

static void opt_update(void)
{
    int vis = fp_big() ? 13 : 8;
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
    if (O.sel < O.nrows) {
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
    int sw = fp_sw(), big = fp_big(), vis = big ? 13 : 8, i;
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
        int hy = y0 + vis * 11 + 6;
        if (O.sel < O.nrows) {
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
        fp_pill(x - 4, hy, w + 8, big ? 22 : 12, st->fill_hi, st->outline, PAL_WHITE);
        if (big) ui_text_wrap(x + 2, hy + 2, w - 4, help, c, 1);
        else fp_marquee(x + 2, hy + 2, w - 4, help, c, O.t);
    }
    ui_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " Change  L/R Page  " STR_ICON_B " Done", style);
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

const DoomoScene SCENE_OPTIONS = { "OPTIONS", opt_enter, NULL, opt_update, opt_draw, 0 };
