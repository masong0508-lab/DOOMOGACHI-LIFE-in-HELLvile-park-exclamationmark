/* lib_help.c - SCENE_LIB_HELP: the Field Guide, the Grand Library's help and
 * glossary book (CL4 / clarity C12; owner: library).
 *
 * One page per rule or word a new player meets without an explanation: the
 * buttons, H$ and rent, the eight needs, moods, Dark and possession, bonds,
 * the two sides, exorcism, welding stats, the house and its score, Hold the
 * Door, the Boreagotchi, family, play-as, the Music Studio's words and the
 * tips themselves. The first-time tip cards (tips.c) are the short version;
 * this book is the long one.
 *
 * It is the eighth book on the lobby's shelf (not a wing: no completion, no
 * visited bit, so the library's unlock ids are untouched), a shortcut in the
 * free play Library menu, and the "Help" row of the island's START menu and
 * the story's pause panel (library.h HELP_ARG_PAGE opens one page at once).
 *
 * The list is the browser framework of every book (lib_nav, lib_list); A
 * opens a page, LEFT / RIGHT turn pages, UP / DOWN scroll. A page is up to
 * four paragraphs, each its own English literal under 250 bytes (one
 * translation row each, docs/LANGUAGES.md); they are translated and joined
 * into the scene scratch once per page (lib_w.help), then scrolled from
 * there. Text and code live in this file only (a scene overlay on the PS1).
 */
#include "lib_int.h"

#define G (lib_w.help)
#define PARAS 4

/* A page: its emblem and colour; its title and paragraphs come from title_of() / para_text(). On the PS1
 * every literal of this file loads with the Library's overlay group: nothing resident points into the
 * file's string section (the scene's name is a named array, help_scene_name). */
typedef struct {
    uint8_t icon, paint;
} HelpPage;

enum { HP_BUTTONS, HP_MONEY, HP_NEEDS, HP_MOODS, HP_DARK, HP_BONDS, HP_SIDES, HP_EXORCISM,
       HP_WELDING, HP_HOUSE, HP_SCORE, HP_DEFEND, HP_PET, HP_FAMILY, HP_PLAYAS, HP_STUDIO,
       HP_TIPS, HP_COUNT };
DOOMO_STATIC_ASSERT(HP_COUNT <= 255, help_pages_fit_a_byte);

static const HelpPage pages[HP_COUNT] = {
    { ICON_BTN_A, PAINT_ROSE },
    { ICON_COIN, PAINT_HONEY },
    { ICON_FOOD, PAINT_TANGERINE },
    { ICON_SMILE, PAINT_BUBBLEGUM },
    { ICON_SKULL, PAINT_BLOOD },
    { ICON_HEART, PAINT_CHERRY },
    { ICON_PENTAGRAM, PAINT_GRAPE },
    { ICON_CROSS, PAINT_SKY },
    { ICON_WRENCH, PAINT_COBALT },
    { ICON_HOUSE, PAINT_PEACH },
    { ICON_STAR, PAINT_HONEY },
    { ICON_BOMB, PAINT_RUST },
    { ICON_EGG, PAINT_LIME },
    { ICON_HOUSE, PAINT_ROSE },
    { ICON_SMILE, PAINT_TEAL },
    { ICON_NOTE, PAINT_GRAPE },
    { ICON_CHECK, PAINT_FOREST },
};

static const char *title_of(int page)
{
    switch (page) {
    case HP_BUTTONS: return "Buttons";
    case HP_MONEY: return "H$ and rent";
    case HP_NEEDS: return "Needs";
    case HP_MOODS: return "Moods";
    case HP_DARK: return "Dark";
    case HP_BONDS: return "Bonds";
    case HP_SIDES: return "Two sides";
    case HP_EXORCISM: return "Exorcism";
    case HP_WELDING: return "Weld stats";
    case HP_HOUSE: return "The house";
    case HP_SCORE: return "House score";
    case HP_DEFEND: return "Hold the Door";
    case HP_PET: return "Boreagotchi";
    case HP_FAMILY: return "Family";
    case HP_PLAYAS: return "Play as";
    case HP_STUDIO: return "Studio words";
    case HP_TIPS: return "Tips";
    default: return "";
    }
}

/* paragraph k (0..PARAS-1) of a page, NULL past its last one */
static const char *para_text(int page, int k)
{
    switch (page * PARAS + k) {
    case HP_BUTTONS * PARAS + 0:
        return "A: choose, talk, use. B: back; hold it to run in the shooter. START: the menu. In the story START pauses, "
               "and START again saves. SELECT: the map in the story, your Family on the island.";
    case HP_BUTTONS * PARAS + 1:
        return "L and R: switch tabs and pages. On the island L opens the news and R the citizen list. In the shooter "
               "(Modern controls) L and R turn you around.";
    case HP_MONEY * PARAS + 0:
        return "H$ is HaHaland money. You earn it from rent, the Arena, minigames and jobs in the story, and spend it in "
               "the shops, at the hospital and on picnics.";
    case HP_MONEY * PARAS + 1:
        return "Rent: once a day every citizen chips in. On the island it collects itself a moment after you stop "
               "pressing buttons on the map: 50 H$ plus 2 H$ for every citizen (ghosts pay too).";
    case HP_NEEDS * PARAS + 0:
        return "Every citizen has eight needs, each 0 to 100: Food, Joy, Clean, Energy, Health, Mind, Dark and Social. "
               "Higher is better for all of them except Dark.";
    case HP_NEEDS * PARAS + 1:
        return "Care actions fill them: Feed fills Food, Gift and Play fill Joy, Clean fills Clean, Talk fills Social, "
               "Comfort fills Mind (and lowers Dark). Energy comes back with sleep; Dr. Bloom fixes Health.";
    case HP_NEEDS * PARAS + 2:
        return "A need near zero shows as a red bar and an icon in the citizen's window. Low needs mean bad moods, and "
               "starving or neglected citizens can get sick, leave or worse.";
    case HP_MOODS * PARAS + 0:
        return "A citizen's mood is the face they pull: OK, Happy, Ecstatic, Sad, Angry, Lovesick, Hungry, Sleepy, Sick, "
               "Filthy, Lonely, Scared, Corrupted or Possessed.";
    case HP_MOODS * PARAS + 1:
        return "Moods come from needs: a hungry citizen is Hungry, a lonely one Lonely. Fix the need and the mood follows. "
               "Happy citizens make friends faster; Corrupted ones are close to possession.";
    case HP_DARK * PARAS + 0:
        return "Dark is the one need you want LOW. It creeps up with Hellvile snacks, black market goods, scares and "
               "neglect. Black market tags like \"Dark+2\" mean the item adds Dark.";
    case HP_DARK * PARAS + 1:
        return "When Dark fills up, a demon can move in at night: the citizen is Possessed. Comfort, holy items and a "
               "happy life keep Dark down. Options > Life Sim sets how often possessions happen.";
    case HP_DARK * PARAS + 2:
        return "A possessed citizen needs an exorcism at the church (see Exorcism).";
    case HP_BONDS * PARAS + 0:
        return "Every pair of citizens has a bond from -100 (hate) to 100 (adore). Talking, playing, gifts and being "
               "introduced raise it; fights and bad moods lower it.";
    case HP_BONDS * PARAS + 1:
        return "Bonds become friends, best friends, dating, married, family, rivals, nemeses or exes. A heart over a "
               "citizen means a crush. A citizen's Friends tab lists their bonds.";
    case HP_BONDS * PARAS + 2:
        return "Romance only happens between citizens of the same life stage, never with children, and each citizen has "
               "their own taste.";
    case HP_SIDES * PARAS + 0:
        return "The island has two sides with 500 homes each. Hillvile is the pastel good side; Hellvile, over the Wall, "
               "is the spooky bad side. HaHaland is the town everyone shares.";
    case HP_SIDES * PARAS + 1:
        return "Make citizens for either side at Town Hall. Hellvile folk are not evil, mostly. Some are very nice and "
               "just dress badly.";
    case HP_EXORCISM * PARAS + 0:
        return "Father Fuzz's ritual at the church frees a possessed citizen. First pick tools (holy water, sage, plush "
               "cross, rosary, salt, hymn book): each one raises the odds shown on screen. START begins.";
    case HP_EXORCISM * PARAS + 1:
        return "Then three phases: 1. Prayer: press the button on each prompt as it reaches the heart. 2. Holy water: "
               "aim and throw at the demon. 3. True name: pick the demon's name from the clues.";
    case HP_EXORCISM * PARAS + 2:
        return "Doing well in each phase raises the final chance. A failed exorcism can be tried again, but after three "
               "failures the soul is lost. You can practice on a plush dummy for free.";
    case HP_WELDING * PARAS + 0:
        return "Rivet welds weapons from parts: a frame, barrel, ammo core, mod, sight, stock, magazine and more. The "
               "welding game sets the Quality (0 to 100), which scales every stat.";
    case HP_WELDING * PARAS + 1:
        return "DMG: damage per shot. RATE: shots per second. RNG: range. ACC: accuracy. AMMO: magazine size. RLD: "
               "reload speed. CTRL: how little it kicks. MOB: how fast you move with it.";
    case HP_HOUSE * PARAS + 0:
        return "Your house has three modes. Live: walk around, A uses things (sit, sleep, lamps, doors). Photo: take "
               "pictures, L/R turn the view.";
    case HP_HOUSE * PARAS + 1:
        return "Build: move the cursor, A picks a piece up and puts it down, L/R turn it (or the view), SELECT undoes.";
    case HP_HOUSE * PARAS + 2:
        return "START opens the menu: Catalog (buy and place furniture), the Build / Live switch, Score, walls, themes, "
               "blueprints, photos and rooms. Grey pieces in a blueprint are bought at HomeMart.";
    case HP_SCORE * PARAS + 0:
        return "Each room is scored on four dots: Comfort, Spook, Style (Harmony later in the story) and Light. "
               "Barricade joins them when the demons come: it is how well the door holds.";
    case HP_SCORE * PARAS + 1:
        return "The Score page explains each rule it found, like \"The bed is not by the door\". Fill a room well and it "
               "earns a stamp: COSY, LIT, HELD, HOME and, rarely, ETERNAL.";
    case HP_DEFEND * PARAS + 0:
        return "Some nights demons attack the house. Walk to things and press A: the door to repair it, the toaster "
               "turret to wind it, a guest to calm them, a window to weld a plate on.";
    case HP_DEFEND * PARAS + 1:
        return "Imps hit the door; brutes push on the windows. Keep your guests safe until dawn. The door's health is "
               "the room's Barricade.";
    case HP_PET * PARAS + 0:
        return "Your pocket pet. Its screen has icons: Food, Lights, Play, Medicine, Clean, Health, Discipline and "
               "Attention. LEFT/RIGHT picks one, A uses it.";
    case HP_PET * PARAS + 1:
        return "Health shows its meters; Attention lights up when it calls you. Care well and it grows into a better "
               "form; the Boredex (SELECT) lists every form you found.";
    case HP_FAMILY * PARAS + 0:
        return "Your household is you plus the members you add: partners, children, roommates and pets. Adding someone, "
               "you pick what they are to you. Care for everyone does a round of care in one go.";
    case HP_FAMILY * PARAS + 1:
        return "The household has room for 3 at first, more after you finish the story, and the most after New Game+ "
               "(20; 10 on the GBA). Pets count too. Options > Life Sim > Loss decides if members can be lost.";
    case HP_PLAYAS * PARAS + 0:
        return "After you finish the story you can switch who you play: the hero, a villager, a family member or one of "
               "the main cast. In the story R opens the switcher and L goes back to who you were.";
    case HP_PLAYAS * PARAS + 1:
        return "Each person keeps their own money, items and health; your Boreagotchi follows you. The story itself "
               "stays the hero's.";
    case HP_STUDIO * PARAS + 0:
        return "Song: the title, genre, tempo (the number by the heart, in beats per minute), key, scale and swing "
               "(how bouncy the beat is). Notes: the piano roll of one pattern.";
    case HP_STUDIO * PARAS + 1:
        return "Pattern: a short loop of notes. Order: the patterns in the order they play. Lyrics: the words and who "
               "sings them. Choir: who sings which section. Stage: perform it.";
    case HP_TIPS * PARAS + 0:
        return "The first time you reach a new place, a card explains it. Any button closes it. Options > Game > Tips "
               "turns the cards off; Show all tips again brings them back.";
    case HP_TIPS * PARAS + 1:
        return "This Field Guide lives in the Grand Library. Open it any time from the island's START menu or the "
               "story's pause menu (Help).";
    default: return NULL;
    }
}

/* the X / Y line differs per machine */
static const char *buttons_extra(void)
{
    if (plat_name()[0] == 'G') return "No X or Y on the Game Boy Advance: SELECT+A works as X and SELECT+B as Y.";
    return "X and Y are extras: in the shooter X rolls and Y swaps weapons.";
}

static const char *para_of(int page, int k)
{
    if (page == HP_BUTTONS && k == 2) return buttons_extra();
    return para_text(page, k);
}

/* the open page's paragraphs, translated, joined with a blank line */
static void compose(int page)
{
    int k, n = 0, lang = lang_get();
    if (G.page_for == page + 1 && G.lang_for == lang) return;
    G.page_for = (int16_t)(page + 1);
    G.lang_for = (int8_t)lang;
    for (k = 0; k < PARAS; k++) {
        const char *p = para_of(page, k);
        int i;
        if (!p) continue;
        p = lang_tr(p);
        if (n && n + 2 < (int)sizeof(G.text) - 1) { G.text[n++] = '\n'; G.text[n++] = '\n'; }
        for (i = 0; p[i] && n < (int)sizeof(G.text) - 1; i++) G.text[n++] = p[i];
    }
    G.text[n] = 0;
}

/* ---- scene ------------------------------------------------------------------ */
static void help_enter(int arg)
{
    int page = HELP_ARG_PAGE_OF(arg);
    lib_wing_enter(-1, arg & LIB_ARG_DEMO);
    G.page_for = 0;
    lib_b.count = HP_COUNT;
    if (page > 0 && page <= HP_COUNT) {                 /* straight to one page */
        lib_b.sel = (int16_t)(page - 1);
        lib_b.detail = 1;
        lib_b.dt = 0;
    }
    lib_clamp();
}

static void help_update(void)
{
    int ev;
    if (lib_w_lost(1)) return;
    lib_b.count = HP_COUNT;
    ev = lib_nav(1, 1);
    if (ev == LIBE_BACK) lib_wing_leave();
}

static void row(int index, int x, int y, int w, int sel)
{
    const HelpPage *p = &pages[index];
    const UiStyle *st = ui_style(WIN_MINT);
    char g[2], buf[48];
    g[0] = (char)p->icon;
    g[1] = 0;
    ui_text(x, y + 1, g, PAL_PAINT(p->paint, 1));
    ui_text(x + 9, y + 1, lib_fit(title_of(index), w - 10, buf, sizeof(buf)), sel ? st->title : st->text);
}

/* a text block from the composed page, `lines` tall; the open page (scroll = 1) is scrolled to
 * lib_b.scroll and sets lib_b.scroll_max, the preview shows the top */
static void page_text(int x, int y, int w, int lines, uint8_t color, int scroll)
{
    int total = ui_text_wrap(0, 0, w, G.text, 0, 0), skip, n, top = 0, max = DOOMO_MAX(0, total - lines);
    char keep;
    if (scroll) {
        lib_b.scroll_max = (int16_t)max;
        if (lib_b.scroll > lib_b.scroll_max) lib_b.scroll = lib_b.scroll_max;
        top = lib_b.scroll;
    }
    skip = ui_wrap_skip(G.text, w, top);
    n = skip + ui_wrap_skip(G.text + skip, w, lines);
    keep = G.text[n];
    G.text[n] = 0;
    ui_text_wrap(x, y, w, G.text + skip, color, 1);
    G.text[n] = keep;
    if (max > 0)
        ui_scroll_arrows(x + w + 1, y - 5, y + lines * FONT_LINE - 2, top > 0, top < max);
}

static void preview(int x, int y, int w, int h)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    const HelpPage *p = &pages[lib_b.sel];
    ui_window(x, y, w, h, WIN_CREAM);
    if (plat_draw_skipped() || lib_b.detail) return;      /* under an open page: nothing to read */
    lib_icon_chip(x + 14, y + 14, 9, p->icon, PAL_PAINT(p->paint, 1));
    ui_text_shadow(x + 28, y + 6, title_of(lib_b.sel), st->accent, st->fill_lo);
    ui_text(x + 28, y + 16, "A: read", st->text_dim);
    compose(lib_b.sel);
    page_text(x + 6, y + 30, w - 14, (h - 36) / FONT_LINE, st->text, 0);
}

static void detail(int sw, int sh)
{
    const UiStyle *st = ui_style(WIN_CREAM);
    const HelpPage *p = &pages[lib_b.sel];
    int x = 4, y = 18, w = sw - 8, h = sh - 18 - 15;
    char num[16];
    if (!lib_detail_window(x, y, w, h, WIN_CREAM) || plat_draw_skipped()) return;
    lib_icon_chip(x + 14, y + 13, 9, p->icon, PAL_PAINT(p->paint, 1));
    ui_text_shadow(x + 28, y + 5, title_of(lib_b.sel), st->accent, st->fill_lo);
    doomo_snprintf(num, sizeof(num), "%d/%d", lib_b.sel + 1, HP_COUNT);
    ui_text(x + w - 8 - ui_text_width(num), y + 5, num, st->text_dim);
    plat_rect(x + 8, y + 25, w - 16, 1, st->fill_lo);
    compose(lib_b.sel);
    page_text(x + 8, y + 30, w - 22, (h - 36) / FONT_LINE, st->text, 1);
    lib_detail_end(x, y, w, h, WIN_CREAM);
}

static void help_draw(void)
{
    int sw = lib_sw(), sh = lib_sh(), y = 18, lw;
    if (lib_w_lost(0)) return;
    ui_backdrop(WIN_MINT, doomo_frame_count());
    if (plat_draw_skipped()) return;
    ui_title(sw / 2, 2, STR_ICON_CHECK " FIELD GUIDE", WIN_MINT);
    lw = lib_big() ? 132 : 104;
    lib_list(4, y + 1, lw, sh - y - 16, 12, WIN_MINT, row);
    preview(lw + 8, y + 1, sw - lw - 12, sh - y - 16);
    if (lib_b.detail) {
        lib_dim_for_detail();
        detail(sw, sh);
        lib_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " page  " STR_ICON_ARROW_U STR_ICON_ARROW_D " scroll  "
                 STR_ICON_B " close", WIN_CREAM);
    } else {
        lib_hint(STR_ICON_A " read  " STR_ICON_ARROW_U STR_ICON_ARROW_D " pick  " STR_ICON_B " back", WIN_MINT);
    }
}

int lib_help_pages(void) { return HP_COUNT; }

/* The scene's name is a named array, not a literal: the resident scene struct would otherwise pull this
 * file's whole string section (every page) into resident RAM on the PS1. */
static const char help_scene_name[] = "LIB_HELP";
const DoomoScene SCENE_LIB_HELP = { help_scene_name, help_enter, NULL, help_update, help_draw, 0 };
