/* studio_help.c - ST-EASY: say what a tab is for and which buttons do what (owner: studio).
 *
 *  - st_tab_label():  the tab strip's names (plan 2.2: Start, Edit, Arrange, Words, Choir, Show, File)
 *  - st_tab_help():   one line per tab (tab_help[]), at most 36 glyphs so it fits a 240 px bar. It replaces the
 *                     prompt bar when a tab opens, until the first button press on it (or 30 s), and not at all
 *                     once an edit was made in this session
 *  - the control card ("?" = SELECT + UP): a popup that lists this tab's buttons by the pad's own names (the
 *                     prompt bar's glyph builder, ui_prompts_text) and the buttons every tab shares
 *  - st_level():      the Studio level, Easy / Advanced (FILE). In st_easy() and in bit 15 of studio_stats.lines.
 *                     Advanced shows no extra tools yet (ST-ADV): the switch and its saved state are real.
 */
#include "studio_int.h"

static const char *const tab_label[STAB_COUNT] = { "Start", "Edit", "Arrange", "Words", "Choir", "Show", "File" };

const char *st_tab_label(int tab) { return tab_label[(tab + STAB_COUNT) % STAB_COUNT]; }

static const char *const tab_help[STAB_COUNT] = {
    "Name, tempo, key and sounds here.",
    STR_ICON_A " places a note, " STR_ICON_B " erases it.",
    "The order the patterns play in.",
    STR_ICON_A " writes the selected line.",
    "Choose who sings; " STR_ICON_A " edits one.",
    STR_ICON_A " starts the live show!",
    "Save, open or start a new song."
};

const char *st_tab_help(int tab)
{
    if (tab == STAB_SONG && studio.card) return "Pick style and mood, then Make it!";
    return tab_help[(tab + STAB_COUNT) % STAB_COUNT];
}

void st_help_arm(void)
{
    StEasy *ez = st_easy();
    if (ez && !ez->edited) ez->help_t = 900;
}

void st_bar_draw(void)
{
    StEasy *ez = st_easy();
    if (ez && ez->help_t && !ez->edited) { ui_hint(st_tab_help(studio.tab), WIN_CUTE); return; }
    {
        UiPrompt pr[UI_PROMPTS_MAX];
        ui_prompts(pr, tab_prompts(studio.tab, pr), WIN_CUTE);
    }
}

/* ---- the studio level ------------------------------------------------------------------------------ */
int st_level(void)
{
    StEasy *ez = st_easy();
    return ez ? ez->level : (studio_stats.lines & STUDIO_LINES_LEVEL) != 0;
}

void st_level_set(int level)
{
    StEasy *ez = st_easy();
    level = level != 0;
    if (ez) ez->level = (uint8_t)level;
    if (level) studio_stats.lines |= STUDIO_LINES_LEVEL;
    else studio_stats.lines &= (uint16_t)~STUDIO_LINES_LEVEL;
    st_msg(level ? "Advanced: more tools soon" : "Easy: the simple view");
    audio_sfx(SFX_SELECT);
}

/* ---- the control card ------------------------------------------------------------------------------ */
#define CARD_ROWS_MAX 9
static const UiPrompt card_song[] = {
    { BTN_UP | BTN_DOWN, 0, "Pick a setting" },
    { BTN_LEFT | BTN_RIGHT, 0, "Change its value" },
    { BTN_A, 0, "Rename (title row)" },
    { BTN_SELECT, 0, "Song tools" },
};
static const UiPrompt card_start[] = {
    { BTN_UP | BTN_DOWN, 0, "Pick a row" },
    { BTN_LEFT | BTN_RIGHT, 0, "Change style, mood..." },
    { BTN_A, 0, "Make it / Shuffle / Open" },
    { BTN_B, 0, "Skip: a blank song" },
};
static const UiPrompt card_notes[] = {
    { BTN_A, 0, "Place the last note" },
    { BTN_A, 0, "Hold, then Up / Down: pitch" },
    { BTN_B, 0, "Erase, or tie a note" },
    { BTN_L | BTN_R, 0, "Other pattern" },
    { BTN_SELECT, 0, "Copy, paste, clear..." },
};
static const UiPrompt card_order[] = {
    { BTN_UP | BTN_DOWN, 0, "Pick a pass" },
    { BTN_LEFT | BTN_RIGHT, 0, "Change its pattern" },
    { BTN_A, 0, "Add a copy below" },
    { BTN_SELECT, 0, "Order tools" },
};
static const UiPrompt card_lyrics[] = {
    { BTN_UP | BTN_DOWN, 0, "Pick a line" },
    { BTN_A, 0, "Write the line" },
    { BTN_LEFT | BTN_RIGHT, 0, "Who sings it" },
    { BTN_SELECT, 0, "Sing, fit, write for me" },
};
static const UiPrompt card_choir[] = {
    { BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT, 0, "Move over the choir" },
    { BTN_A, 0, "Pick or edit a singer" },
    { BTN_SELECT, 0, "Quick fill the choir" },
    { BTN_B, 0, "Back" },
};
static const UiPrompt card_stage[] = {
    { BTN_A, 0, "Start the live show" },
    { BTN_SELECT, 0, "Concert Hall" },
    { BTN_B, 0, "Back" },
};
static const UiPrompt card_file[] = {
    { BTN_UP | BTN_DOWN, 0, "Pick a row" },
    { BTN_A, 0, "Open the row" },
    { BTN_LEFT | BTN_RIGHT, 0, "Studio level (its row)" },
    { BTN_B, 0, "Back" },
};

/* this tab's rows, then the ones every tab has; returns the count (at most CARD_ROWS_MAX) */
static int card_rows(UiPrompt *out)
{
    const UiPrompt *t;
    int n, i, k = 0;
    switch (studio.tab) {
    case STAB_SONG: if (studio.card) { t = card_start; n = DOOMO_COUNTOF(card_start); }
                    else { t = card_song; n = DOOMO_COUNTOF(card_song); } break;
    case STAB_NOTES: t = card_notes; n = DOOMO_COUNTOF(card_notes); break;
    case STAB_ORDER: t = card_order; n = DOOMO_COUNTOF(card_order); break;
    case STAB_LYRICS: t = card_lyrics; n = DOOMO_COUNTOF(card_lyrics); break;
    case STAB_CHOIR: t = card_choir; n = DOOMO_COUNTOF(card_choir); break;
    case STAB_STAGE: t = card_stage; n = DOOMO_COUNTOF(card_stage); break;
    default: t = card_file; n = DOOMO_COUNTOF(card_file); break;
    }
    for (i = 0; i < n; i++) out[k++] = t[i];
    out[k].btn = BTN_START; out[k].prio = 0; out[k].verb = st_play_verb(); k++;
    out[k].btn = BTN_Y; out[k].prio = 0; out[k].verb = "Undo a step"; k++;
    out[k].btn = BTN_X; out[k].prio = 0; out[k].verb = "Redo a step"; k++;
    out[k].btn = BTN_SELECT | BTN_L | BTN_R; out[k].prio = 0; out[k].verb = "Switch tab"; k++;
    return DOOMO_MIN(k, CARD_ROWS_MAX);
}

void st_help_open(void)
{
    studio.help = 1;
    studio.pop_t = 0;
    audio_sfx(SFX_BLIP);
}

int st_help_is_open(void) { return studio.help; }

void st_help_update(void)
{
    studio.pop_t++;
    if (btn_pressed(BTN_A | BTN_B | BTN_START) || ((studio.sel_held || btn_down(BTN_SELECT)) && btn_pressed(BTN_UP))) {
        studio.help = 0;
        audio_sfx(SFX_CANCEL);
        input_eat();
    }
}

#define CARD_RH 11
int st_help_rect(int *x, int *y, int *w, int *h)
{
    UiPrompt rows[CARD_ROWS_MAX + 4];
    int n = card_rows(rows);
    *w = plat_screen_w() - 8;
    *h = 24 + n * CARD_RH + 4;
    *x = 4;
    *y = (plat_screen_h() - *h) / 2 + 3;
    return 1;
}

void st_help_draw(void)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    UiPrompt rows[CARD_ROWS_MAX + 4];
    char buf[96], title[24];
    int n = card_rows(rows), i, x, y, w, h, yy;
    st_help_rect(&x, &y, &w, &h);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, studio.pop_t)) return;
    doomo_snprintf(title, sizeof(title), "%s: buttons", st_tab_label(studio.tab));
    ui_title(plat_screen_w() / 2, y - 5, title, WIN_CUTE);
    ui_text(x + 8, y + 11, st_tab_help(studio.tab), st->text_dim);
    yy = y + 24;
    for (i = 0; i < n; i++) {
        buf[0] = 0;
        (void)ui_prompts_text(buf, (int)sizeof(buf), &rows[i], 1, 9999);
        if (!buf[0]) continue;                  /* a button this pad does not have */
        ui_text(x + 8, yy, buf, st->text);
        yy += CARD_RH;
    }
}
