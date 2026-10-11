/* fp_saveload.c - SCENE_SAVELOAD (owner: freeplay).
 *
 * Two slots: 0 = Story, 1 = Free Play (platform.h). Each card shows the
 * header summary (save_peek: who, chapter or population, day, money,
 * size) without reading the whole save. Save writes the live town into
 * the slot of its own mode; Load restores either; Erase asks twice. A
 * failing save (an island too custom to fit, "Island too big: 93 KB >
 * 88 KB") never touches the old save and is explained in a cute error
 * box. arg FP_SL_ARG_LOAD hides saving (loading from the title).
 */
#include "fp_int.h"

enum { ACT_SAVE, ACT_LOAD, ACT_ERASE, ACT_N };
static const char *const act_names[ACT_N] = { "Save", "Load", "Erase" };

static struct {
    int slot, act, t, load_only, busy_t;
    DoomoSaveInfo info[2];
    uint8_t has[2];
    char msg[96];
    int msg_t, msg_bad;
} L;

static void refresh(void)
{
    int s;
    for (s = 0; s < 2; s++) L.has[s] = (uint8_t)save_peek(s, &L.info[s]);
}

static void sl_enter(int arg)
{
    memset(&L, 0, sizeof(L));
    L.load_only = arg == FP_SL_ARG_LOAD;
    L.slot = doomo_town.mode == TOWN_STORY ? 0 : 1;
    L.act = L.load_only ? ACT_LOAD : ACT_SAVE;
    refresh();
}

static int act_ok(int slot, int act)
{
    switch (act) {
    case ACT_SAVE:
        if (L.load_only) return 0;
        return slot == (doomo_town.mode == TOWN_STORY ? 0 : 1);
    case ACT_LOAD: case ACT_ERASE: return L.has[slot];
    default: return 0;
    }
}

static void say(const char *m, int bad)
{
    fp_strcpy(L.msg, m, sizeof(L.msg));
    L.msg_t = 150;
    L.msg_bad = bad;
}

static void do_save(void)
{
    char buf[96];
    if (save_game(L.slot)) {
        if (L.slot == 1) fp_autosave_bind(1);
        doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART " Saved! (%u KB)", (unsigned)((save_last_size() + 1023) / 1024));
        say(buf, 0);
        audio_sfx(SFX_SPARKLE);
        fp_fx_burst(fp_sw() / 2, fp_sh() / 2, FX_HEART, 12);
    } else {
        doomo_snprintf(buf, sizeof(buf), "Couldn't save: %s. Your old save is safe.",
                       save_last_error()[0] ? save_last_error() : "unknown error");
        say(buf, 1);
        audio_sfx(SFX_ERROR);
        ui_say(-1, buf);
        if (fp_strcasestr(save_last_error(), "too big"))
            ui_say(-1, "Your island is too custom to fit! Try making fewer brand-new citizens, or let some premade ones stay as they are.");
    }
    refresh();
}

static void cb_overwrite(int yes) { if (yes) do_save(); }

static void cb_load(int yes)
{
    char buf[96];
    if (!yes) return;
    if (!load_game(L.slot)) {
        doomo_snprintf(buf, sizeof(buf), "Couldn't load: %s", save_last_error()[0] ? save_last_error() : "no save");
        say(buf, 1);
        audio_sfx(SFX_ERROR);
        return;
    }
    sim_init();
    audio_sfx(SFX_HAPPY);
    if (doomo_town.mode == TOWN_STORY) {
        scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
    } else {
        fp_autosave_bind(1);
        fp_goto_island(FP_ISLAND_ARG_RETURN);
        ui_toast(STR_ICON_HEART " Loaded! Welcome back.");
    }
}

static void cb_erase2(int yes)
{
    if (!yes) return;
    if (plat_save_erase(L.slot)) { say("Slot erased.", 0); audio_sfx(SFX_SPLASH); }
    else { say("Couldn't erase that slot.", 1); audio_sfx(SFX_ERROR); }
    if (L.slot == 1) fp_autosave_bind(0);
    refresh();
}

static void cb_erase1(int yes)
{
    if (yes) ui_confirm("Really REALLY erase it? It can't be undone!", cb_erase2);
}

static void sl_update(void)
{
    L.t++;
    if (L.msg_t) L.msg_t--;
    if (fp_is_freeplay()) fp_live(); else fp_fx_tick();
    if (btn_repeat(BTN_UP | BTN_DOWN)) { L.slot ^= 1; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_LEFT)) { L.act = (L.act + ACT_N - 1) % ACT_N; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_RIGHT)) { L.act = (L.act + 1) % ACT_N; audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_A)) {
        if (!act_ok(L.slot, L.act)) {
            audio_sfx(SFX_ERROR);
            if (L.act == ACT_SAVE)
                say(L.slot == 0 ? "Story saves go in slot 1 (Story)." : "Free play islands go in slot 2.", 1);
            else say("That slot is empty.", 1);
            return;
        }
        audio_sfx(SFX_SELECT);
        switch (L.act) {
        case ACT_SAVE:
            if (L.has[L.slot]) ui_confirm("Overwrite the save in this slot?", cb_overwrite);
            else do_save();
            break;
        case ACT_LOAD:
            ui_confirm(L.load_only ? "Load this save?" : "Load this save? Unsaved progress will be lost!", cb_load);
            break;
        default:
            ui_confirm("Erase this save?", cb_erase1);
            break;
        }
        return;
    }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop();
        else scene_set(&SCENE_TITLE, 0);
    }
}

static void slot_card(int x, int y, int w, int h, int s, int sel)
{
    int style = s == 0 ? WIN_MINT : WIN_CUTE, t = L.t;
    const UiStyle *st = ui_style(style);
    const DoomoSaveInfo *in = &L.info[s];
    char buf[64];
    int b = sel ? (ui_wave(t, 24, 1) > 0) : 0;
    y -= b;
    ui_window(x, y, w, h, style);
    if (sel) {
        ui_frame(x - 2, y - 2, w + 4, h + 4, (t / 8) & 1 ? PAL_UI_GOLD : PAL_WHITE);
        ui_cursor(x - 10, y + h / 2 - 3);
    }
    /* slot badge */
    ui_fill_circle(x + 14, y + 14, 9, st->outline);
    ui_fill_circle(x + 14, y + 14, 8, s == 0 ? PP(PAINT_LAVENDER, 1) : PP(PAINT_ROSE, 1));
    doomo_snprintf(buf, sizeof(buf), "%d", s + 1);
    ui_text_big(x + 10, y + 7, buf, 2, PAL_WHITE, st->outline);
    ui_text(x + 28, y + 5, s == 0 ? "Story" : "Free Play", st->accent);
    if (!L.has[s]) {
        ui_text(x + 28, y + 16, "- empty -", st->text_dim);
        ui_heart(x + w - 14, y + h / 2, 4, st->fill_lo);
        return;
    }
    if (in->mode == TOWN_STORY)
        doomo_snprintf(buf, sizeof(buf), "%s - Ch.%d", in->player[0] ? in->player : "You", in->chapter);
    else
        doomo_snprintf(buf, sizeof(buf), "%s island", in->population == TOWN_POP_EMPTY ? "Homemade" :
                       in->population == TOWN_POP_RANDOM ? "Random" : "Classic");
    ui_text(x + 28, y + 16, buf, st->text);
    doomo_snprintf(buf, sizeof(buf), "Day %u  %d citizens  H$%u", (unsigned)in->day + 1, in->residents,
                   (unsigned)in->money);
    fp_marquee(x + 8, y + 28, w - 16, buf, st->text_dim, t);
    if (h > 44) {
        doomo_snprintf(buf, sizeof(buf), "%u KB of %u KB", (unsigned)((in->size + 1023) / 1024),
                       (unsigned)(save_slot_max(s) / 1024));
        ui_text(x + 8, y + 39, buf, st->text_dim);
        ui_bar(x + 8 + ui_text_width(buf) + 6, y + 40, w - 24 - ui_text_width(buf), 6,
               (int)(in->size / 256), (int)(save_slot_max(s) / 256), s == 0 ? PP(PAINT_LAVENDER, 1) : PP(PAINT_ROSE, 1));
    }
}

static void sl_draw(void)
{
    int sw = fp_sw(), sh = fp_sh(), big = fp_big(), s, i;
    int w = sw - 40, h = big ? 54 : 40, x = 24, y0 = big ? 36 : 26, gap = big ? 14 : 8;
    const UiStyle *st = ui_style(WIN_CUTE);
    ui_backdrop(WIN_CUTE, (uint32_t)fp_t());
    ui_title(sw / 2, 3, L.load_only ? "Load" : "Save & Load", WIN_CUTE);
    for (s = 0; s < 2; s++) slot_card(x, y0 + s * (h + gap), w, h, s, s == L.slot);
    /* action buttons */
    {
        int bw = big ? 70 : 56, by = y0 + 2 * (h + gap) + 2, bx0 = (sw - (ACT_N * (bw + 12) - 12)) / 2;
        for (i = 0; i < ACT_N; i++)
            ui_button(bx0 + i * (bw + 12), by, bw, act_names[i], i == L.act,
                      act_ok(L.slot, i) ? (i == ACT_ERASE ? WIN_HELL : WIN_CUTE) : WIN_CREAM);
    }
    if (L.msg_t) {
        int my = sh - 30;
        fp_pill(8, my, sw - 16, 13, L.msg_bad ? PAL_UI_RED : st->fill_hi, st->outline, PAL_WHITE);
        fp_marquee(14, my + 3, sw - 28, L.msg, L.msg_bad ? PAL_WHITE : st->text, 150 - L.msg_t);
    }
    ui_hint(STR_ICON_ARROW_U STR_ICON_ARROW_D " Slot  " STR_ICON_ARROW_L STR_ICON_ARROW_R " Action  " STR_ICON_A " Go", WIN_CUTE);
    fp_fx_draw();
}

const DoomoScene SCENE_SAVELOAD = { "SAVELOAD", sl_enter, NULL, sl_update, sl_draw, 0 };
