/* combat_card.c - the shooter's control card (clarity C10; owner: action).
 *
 * Every Hell Breach level, the Demon Arena, the arena modes and the story's
 * first fight after power-on open on a card that says how to play, on every
 * difficulty and in every mode (Brutal Only included): the controls of the scheme picked in Options, with the
 * names of the pad in the player's hands, the HUD counters' legend and the
 * mode's rule. A starts (the card is the only thing between the player and
 * the fight, so it is skipped with the first press); nothing is saved.
 *
 * ccard_lines() flows the scheme's (key, action) pairs into lines that fit
 * a width, so the same table fills two short lines at 320x240 and three at
 * 240x160. ccard_draw() is the panel; the scenes place it.
 */
#include "combat_int.h"

/* The pad's own names for the buttons with no cap glyph (A and B are drawn
 * as caps, the D-pad as arrows). The GBA has no X / Y: the core makes them
 * from SELECT+A / SELECT+B (input.h); the PS1 says L1 / R1 and the face
 * buttons' shapes; the Dreamcast's SELECT is START+Y (dc/src/main.c). */
#if defined(DOOMO_PLATFORM_GBA)
#define CC_L "L"
#define CC_R "R"
#define CC_X "SEL+A"
#define CC_Y "SEL+B"
#define CC_SEL "SEL"
#elif defined(DOOMO_PLATFORM_PS1)
#define CC_L "L1"
#define CC_R "R1"
#define CC_X "Square"
#define CC_Y "Triangle"
#define CC_SEL "SEL"
#elif defined(DOOMO_PLATFORM_DC)
#define CC_L "L"
#define CC_R "R"
#define CC_X "X"
#define CC_Y "Y"
#define CC_SEL "START+Y"
#else
#define CC_L "L"
#define CC_R "R"
#define CC_X "X"
#define CC_Y "Y"
#define CC_SEL "SEL"
#endif

typedef struct { const char *key, *what; } CcPair;

static const CcPair cc_modern[] = {
    { STR_ICON_A, "Shoot" }, { STR_ICON_B, "Jump/run" }, { STR_ICON_ARROW_U STR_ICON_ARROW_D, "Walk" },
    { STR_ICON_ARROW_L STR_ICON_ARROW_R, "Step aside" }, { CC_L "/" CC_R, "Turn" },
    { CC_Y, "Next gun" }, { CC_SEL, "Camera" }
};
static const CcPair cc_classic[] = {
    { STR_ICON_A, "Shoot" }, { STR_ICON_B, "Jump/run" }, { "D-pad", "Walk/turn" },
    { "Hold " CC_L, "Strafe, look" }, { CC_R, "Lock on" },
    { CC_Y, "Next gun" }, { CC_SEL, "Camera" }
};
static const CcPair cc_dual[] = {
    { STR_ICON_A "/" CC_R, "Shoot" }, { STR_ICON_B, "Jump/run" }, { "L stick", "Walk" },
    { "R stick", "Look" }, { "L2", "Lock on" }, { CC_L, "Roll" },
    { CC_Y, "Next gun" }, { CC_SEL, "Camera" }
};

#if DOOMO_TRIGGERS
/* (ST1) one stick + analog triggers (Dreamcast) */
static const CcPair cc_stick1[] = {
    { STR_ICON_A, "Shoot" }, { STR_ICON_B, "Jump/run" }, { "Stick", "Walk/turn" },
    { CC_L "/" CC_R, "Strafe" }, { CC_L "+" CC_R, "Lock on" }, { CC_X, "Roll" },
    { CC_Y, "Next gun" }, { CC_SEL, "Camera" }
};
#endif

static const CcPair *scheme_pairs(int *n)
{
    int s = controls_scheme();          /* (ST1) Auto resolved against the pad in hand */
    if (s == CONTROLS_CLASSIC) { *n = (int)DOOMO_COUNTOF(cc_classic); return cc_classic; }
#if DOOMO_TRIGGERS
    if (s == CONTROLS_DUAL_STICK && !(input_caps() & INPUT_CAP_RSTICK)) { *n = (int)DOOMO_COUNTOF(cc_stick1); return cc_stick1; }
#endif
    if (s == CONTROLS_DUAL_STICK) { *n = (int)DOOMO_COUNTOF(cc_dual); return cc_dual; }
    *n = (int)DOOMO_COUNTOF(cc_modern);
    return cc_modern;
}

int ccard_lines(char lines[][CCARD_LINE_CAP], int max, int width)
{
    int n, i, k = 0;
    const CcPair *p = scheme_pairs(&n);
    if (max <= 0) return 0;
    lines[0][0] = 0;
    for (i = 0; i < n; i++) {
        char item[CCARD_LINE_CAP], cand[CCARD_LINE_CAP * 2];
        doomo_snprintf(item, sizeof(item), "%s %s", p[i].key, p[i].what);
        doomo_snprintf(cand, sizeof(cand), lines[k][0] ? "%s  %s" : "%s%s", lines[k], item);
        if (lines[k][0] && (ui_text_width(cand) > width || (int)strlen(cand) >= CCARD_LINE_CAP)) {
            if (k + 1 >= max) return k + 1;            /* the rest does not fit: the table is ordered by need */
            k++;
            doomo_snprintf(lines[k], CCARD_LINE_CAP, "%s", item);
        } else {
            doomo_snprintf(lines[k], CCARD_LINE_CAP, "%s", cand);
        }
    }
    return k + 1;
}

const char *ccard_mode_rule(int mode)
{
    switch (mode) {
    case CCARD_RULE_CUTE: return STR_ICON_HEART " Cute weapons only. No exceptions.";
    case CCARD_RULE_BRUTAL: return STR_ICON_SKULL " Brutal weapons only. No bubbles.";
    case CCARD_RULE_TIME: return STR_ICON_BOLT " Beat the gold time on the clock.";
    default: return NULL;
    }
}

const char *ccard_goal(int cgoal)
{
    return cgoal == CGOAL_KILL_ALL ? STR_ICON_STAR " Goal: beat every demon" :
           cgoal == CGOAL_EXIT ? STR_ICON_STAR " Goal: find the exit" : STR_ICON_STAR " Goal: survive";
}

int ccard_height(int w, const char *goal, const char *rule)
{
    char l[CCARD_MAX_LINES][CCARD_LINE_CAP];
    int n = ccard_lines(l, CCARD_MAX_LINES, w - 12);
    return 6 + (n + 1 + (goal && goal[0]) + (rule && rule[0])) * FONT_LINE + 2;
}

static void ccard_draw_lines(int x, int y, int w, int h, int style, const char *goal, const char *rule,
                             char l[][CCARD_LINE_CAP], int n)
{
    const UiStyle *st = ui_style(style);
    int i, ty;
    ui_window(x, y, w, h, style);
    ty = y + 4;
    if (goal && goal[0]) { ui_text(x + 6, ty, goal, st->text); ty += FONT_LINE; }
    if (rule && rule[0]) { ui_text(x + 6, ty, rule, style == WIN_HELL ? PAL_UI_GOLD : PAL_UI_PINK_D); ty += FONT_LINE; }
    for (i = 0; i < n; i++, ty += FONT_LINE) ui_text(x + 6, ty, l[i], st->text);
    combat_hud_legend(x + 6, ty, w - 12, st->text_dim);
}

void ccard_draw(int x, int y, int w, int style, const char *goal, const char *rule)
{
    char l[CCARD_MAX_LINES][CCARD_LINE_CAP];
    int n;
    if (plat_draw_skipped()) return;
    n = ccard_lines(l, CCARD_MAX_LINES, w - 12);
    ccard_draw_lines(x, y, w, 6 + (n + 1 + (goal && goal[0]) + (rule && rule[0])) * FONT_LINE + 2, style, goal, rule,
                     l, n);
}

/* (S2-2) ccard_height + ccard_draw at (cx - w / 2, bottom - height) with the lines flowed once (the cards drew
 * them twice a frame: their snprintf and text widths were ~3 ms of a held GBA card) */
void ccard_draw_above(int cx, int bottom, int w, int style, const char *goal, const char *rule)
{
    char l[CCARD_MAX_LINES][CCARD_LINE_CAP];
    int n, h;
    if (plat_draw_skipped()) return;
    n = ccard_lines(l, CCARD_MAX_LINES, w - 12);
    h = 6 + (n + 1 + (goal && goal[0]) + (rule && rule[0])) * FONT_LINE + 2;
    ccard_draw_lines(cx - w / 2, bottom - h, w, h, style, goal, rule, l, n);
}
