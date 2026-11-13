/* tips.c - first-time tip cards (CL4 / clarity C4; contract: doomo/tips.h).
 *
 * One card at a time, drawn over the scene that asked for it (tip_once from
 * its update). The card is the minigame title card's speech bubble turned
 * into a strip: a speaker chip and name, one line in the speaker's voice and
 * one plain instruction line on a highlighted band, "A OK" in the corner.
 * It boings in (ui_window_pop), then holds still as a retained panel (G7) and
 * leaves on the first press after TIP_LOCK ticks; every press while it is up
 * is eaten. A scene that keeps its frames (the island hub) keeps the steady
 * card on its kept page (tip_steady / tip_hold). The card stays clear of the
 * bottom 14 rows (hint pills, the island's news strip).
 *
 * Owner scene: the card belongs to the scene that was on top when it showed.
 * While another scene is pushed over it (a confirm, the options) it is not
 * drawn and takes no input; when its scene is gone (popped, scene_set) it is
 * dropped. A tip counts as seen once it was closed by a press or was on
 * screen for TIP_SEEN_TICKS.
 *
 * Resident code and text (PS1: called from many scene overlays, drawn from
 * doomo_frame): the text lives here, never in a scene's overlay. RAM: the
 * state below (15 B). The words go through lang_tr (ui_text_wrap, ui_text),
 * so a language without a row simply shows the English.
 */
#include "doomo/doomo.h"
#include "doomo/tips.h"
#include "doomo/plat_ext.h"
#include "doomo/options_store.h"

#define TIP_LOCK        14      /* presses ignored (still eaten) while it boings in      */
#define TIP_SEEN_TICKS  60      /* on screen this long: seen even if its scene goes away */
#define TIP_SETTLED     13      /* the pop-in is over: the card holds still (tip_steady, the panel)   */
#define TIP_BOTTOM      17      /* card + its drop shadow clear of the bottom 14 rows (hint pill, the island's news strip, which
                                   repaints itself on the hub's kept frames)                          */

enum { SPK_MAYOR, SPK_RIVET, SPK_FUZZ, SPK_COUNT };
static const char *const spk_name[SPK_COUNT] = { "Mayor Giggs", "Rivet", "Father Fuzz" };

typedef struct {
    uint8_t     spk, icon;
    const char *voice;          /* in character: one or two lines           */
    const char *plain;          /* the one plain instruction                 */
} TipText;

/* Voice lines follow docs/design/story/SCRIPT_STYLE.md (abrupt indie-parody comedy, the odd
 * training-tape joke); the plain line is what to press, with no joke in it. */
static const TipText tip_text[TIP_COUNT] = {
    /* TIP_ISLAND */
    { SPK_MAYOR, ICON_STAR,
      "Gobble! The island is yours now. It came with a tape marked DO NOT WATCH. I watched it. Needs salt.",
      "D-pad: pick a place. A: go in. B: help the next citizen in need. START: menu. %L: news. %R: citizens. %S: family." },
    /* TIP_TOWN_HALL */
    { SPK_MAYOR, ICON_HOUSE,
      "My office! Hillvile is the pastel side. Hellvile is over the Wall. We don't discuss the Wall.",
      "Create citizens for either side here. Rent comes in once a day. Save, Options and Family are here too." },
    /* TIP_APARTMENTS */
    { SPK_MAYOR, ICON_HOUSE,
      "Every window is somebody's home. Peeking is legal. I checked. I wrote the law this morning.",
      "Icons in a window show what that citizen needs. A: visit. START: pick a floor. %S: what the icons mean." },
    /* TIP_CARE */
    { SPK_MAYOR, ICON_HEART,
      "Citizens have eight needs. Like houseplants, but they scream. Let one hit zero and things get weird.",
      "Pick an action, A does it. Feed fills Food, Clean fills Clean, Talk fills Social, Comfort fills Mind. %L/%R: tabs." },
    /* TIP_HOUSE */
    { SPK_RIVET, ICON_WRENCH,
      "Your house! I welded the plumbing myself. Do not lick the plumbing.",
      "START: menu (Catalog, Build or Live mode, Photo, Score). Build: A picks up and puts down. Live: walk, A uses things." },
    /* TIP_DEFEND */
    { SPK_RIVET, ICON_BOMB,
      "Demons at the door! Classic Tuesday. Hold the door and nobody gets eaten. Probably.",
      "Walk up to something and press A: the door to repair it, the toaster turret to wind it, a guest to calm them, a window to plate it." },
    /* TIP_SHOOTER (the Modern scheme; tip_plain() swaps in the others) */
    { SPK_RIVET, ICON_GUN,
      "Shooty time! Aim at the demons, not the furniture. The furniture has rights now.",
      "UP/DOWN: walk. LEFT/RIGHT: step aside. %L/%R: turn. A: shoot. B: jump, hold to run. START: pause." },
    /* TIP_EXORCISM */
    { SPK_FUZZ, ICON_CROSS,
      "Bless you! A demon moved into this poor soul without paying rent. We evict it. Gently. With water.",
      "A picks tools (better odds), START begins. Then: 1. press the buttons on the beat, 2. throw holy water at the demon, 3. pick its true name." },
    /* TIP_PET */
    { SPK_MAYOR, ICON_EGG,
      "Your Boreagotchi! A pet, a toy and a tiny landlord. Feed it, or it files a complaint. With me.",
      "LEFT/RIGHT: pick an icon. A: use it. %S: the Boredex. START: leave." },
    /* TIP_STUDIO */
    { SPK_MAYOR, ICON_NOTE,
      "The Music Studio! I wrote the town anthem here. It is one note. It is a very good note.",
      "%L/%R: switch tabs (Song, Notes, Order, Lyrics, Choir, Stage, File). A: edit the row. START: play. B: back." },
    /* TIP_WORKSHOP */
    { SPK_RIVET, ICON_WRENCH,
      "My workshop! Pick parts, weld them, get a gun. Keep all your fingers. Most people do.",
      "LEFT/RIGHT: pick a part, A: weld it. DMG damage, RATE fire rate, RNG range, ACC aim, RLD reload, MOB how fast you move." },
    /* TIP_FAMILY */
    { SPK_FUZZ, ICON_HOUSE,
      "Family! The people you would hide in a cupboard during a demon attack. With snacks.",
      "Your household is you and the members you add. Pick what each one is to you, then care for everyone here." },
    /* TIP_LIBRARY */
    { SPK_FUZZ, ICON_STAR,
      "The Grand Library! Quillby runs it. Whisper. The Bestiary is a light sleeper.",
      "LEFT/RIGHT: pick a book, A: read it. The Field Guide explains every rule and word in the game." }
};

/* the plain line, by control scheme where that matters */
static const char *tip_plain(int id)
{
    if (id == TIP_SHOOTER) {
        int s = controls_scheme();
        if (s == CONTROLS_CLASSIC)
            return "D-pad: walk and turn. Hold %L: step aside and look. A: shoot. B: jump, hold to run. START: pause.";
        if (DOOMO_TRIGGERS && s == CONTROLS_DUAL_STICK && !(input_caps() & INPUT_CAP_RSTICK))
            return opt_get(OPT_STICK_MODE) ? "Stick: walk and step aside. L/R triggers: turn. A: shoot. B: jump, hold to run. START: pause." :
                   "Stick: walk and turn. L/R triggers: step aside. A: shoot. B: jump, hold to run. START: pause.";
        if (s == CONTROLS_DUAL_STICK)
            return "Left stick: walk. Right stick: look. A: shoot. B: jump, hold to run. START: pause.";
    }
    return tip_text[id].plain;
}

/* %L %R %S in a plain line become the button's name on the pad in the player's hands (C3, input.h
 * btn_name): L1 / R1 on a PS1, START+Y for SELECT on a Dreamcast, SELECT spelled out elsewhere. The
 * English line is looked up first (lang_tr keys on it), then the names go in. */
static const char *tip_expand(const char *s, char *out, int n)
{
    int o = 0;
    s = lang_tr(s);
    for (; *s && o < n - 1; s++) {
        const char *w = 0;
        if (s[0] == '%') {
            uint32_t b = s[1] == 'L' ? BTN_L : s[1] == 'R' ? BTN_R : s[1] == 'S' ? BTN_SELECT : 0;
            if (b) {
                w = b == BTN_SELECT && input_pad() != PAD_DC ? "SELECT" : btn_name(b);
                if (!w) w = "";
                s++;
            }
        }
        if (w) { while (*w && o < n - 1) out[o++] = *w++; }
        else out[o++] = *s;
    }
    out[o] = 0;
    return out;
}

/* ---- state (15 B) ------------------------------------------------------------- */
static uint32_t s_seen;                /* bit per TIP_*                              */
static const DoomoScene *s_owner;      /* the scene the card belongs to              */
static int8_t   s_cur = -1;            /* the card, -1 none                          */
static uint8_t  s_depth;               /* scene_depth() when it showed               */
static uint8_t  s_t;                   /* ticks on screen (saturates)                */
static uint8_t  s_gen;                 /* tip_steady's name                          */
static uint8_t  s_held;                /* tip_hold() this frame                      */
static uint8_t  s_dirty;               /* seen flags not yet in the config record    */
static uint8_t  s_force;               /* "tips on" under a test harness             */

static const char *const tip_names[TIP_COUNT] = {
    "island", "town_hall", "apartments", "care", "house", "defend", "shooter", "exorcism", "pet",
    "studio", "workshop", "family", "library"
};
const char *tip_name(int id) { return id >= 0 && id < TIP_COUNT ? tip_names[id] : "none"; }

int tip_seen(int id) { return id >= 0 && id < TIP_COUNT && ((s_seen >> id) & 1); }
uint32_t tip_seen_bits(void) { return s_seen; }
int tip_current(void) { return s_cur; }
int tips_dirty(void) { return s_dirty; }
void tips_clean(void) { s_dirty = 0; }

void tip_seen_merge(uint32_t bits)
{
    bits &= (TIP_COUNT >= 32) ? 0xFFFFFFFFu : ((1u << TIP_COUNT) - 1u);
    s_seen |= bits;
}

static void mark_seen(int id)
{
    if (tip_seen(id)) return;
    s_seen |= 1u << id;
    s_dirty = 1;
}

void tips_reset(void)
{
    s_seen = 0;
    s_dirty = 1;                       /* the record follows when the Options are left (opt_config_flush) */
}

void tips_test_force(int on) { s_force = (uint8_t)(on != 0); }

const char *tip_voice_text(int id) { return id >= 0 && id < TIP_COUNT ? tip_text[id].voice : 0; }
const char *tip_plain_text(int id, char *out, int n)
{
    return id >= 0 && id < TIP_COUNT && out && n > 0 ? tip_expand(tip_plain(id), out, n) : 0;
}
const char *tip_speaker(int id) { return id >= 0 && id < TIP_COUNT ? spk_name[tip_text[id].spk] : 0; }

void tips_session_reset(void)
{
    s_seen = 0;
    s_cur = -1;
    s_owner = 0;
    s_t = 0;
    s_held = 0;
    s_dirty = 0;
    s_force = 0;
    s_gen++;
}

int tips_enabled(void)
{
    if ((plat_test_flags() & PLAT_TEST_NO_TIPS) && !s_force) return 0;
    return opt_get(OPT_TIPS) != 0;
}

int tip_active(void)
{
    return s_cur >= 0 && scene_top() == s_owner && scene_depth() == s_depth;
}

static void drop(void)
{
    if (s_cur >= 0 && s_t >= TIP_SEEN_TICKS) mark_seen(s_cur);
    s_cur = -1;
    s_owner = 0;
    s_gen++;
    plat_hold_frame(0);                /* a kept frame never outlives the card that was on it */
}

int tip_once(int id)
{
    if (id < 0 || id >= TIP_COUNT || tip_seen(id) || s_cur >= 0) return 0;
    if (!tips_enabled() || ui_dialog_active() || !scene_top()) return 0;
    s_cur = (int8_t)id;
    s_owner = scene_top();
    s_depth = (uint8_t)scene_depth();
    s_t = 0;
    s_gen++;
    plat_hold_frame(0);                /* the scene's kept frame (if any) has no card on it */
    audio_sfx(SFX_SPARKLE);
    return 1;
}

void tips_input(void)
{
    if (s_cur < 0) return;
    if (scene_depth() < s_depth || (scene_depth() == s_depth && scene_top() != s_owner)) {
        drop();                        /* its scene is gone */
        return;
    }
    if (!tip_active()) return;         /* something is pushed over it: wait */
    if (s_t < 255) s_t++;
    if (btn_pressed(0xFFFFFFFFu) && s_t > TIP_LOCK) {
        mark_seen(s_cur);
        audio_sfx(SFX_SELECT);
        drop();
    }
    input_eat();                       /* the card has the pad: nothing reaches the scene */
}

int tip_steady(void)
{
    if (!tip_active() || s_t < TIP_SETTLED) return 0;
    return 1 + s_gen;
}

void tip_hold(void) { s_held = 1; }

/* ---- drawing ------------------------------------------------------------------ */
void tips_draw(void)
{
    const TipText *tt;
    const UiStyle *st;
    int sw, sh, big, w, x, y, h, tw, vl, pl, panel = 0, kept = s_held;
    const char *plain;
    char pbuf[TIP_PLAIN_MAX];
    s_held = 0;
    if (!tip_active() || kept || plat_draw_skipped()) return;
    tt = &tip_text[s_cur];
    plain = tip_expand(tip_plain(s_cur), pbuf, (int)sizeof pbuf);
    st = ui_style(WIN_CREAM);
    sw = plat_screen_w();
    sh = plat_screen_h();
    big = sh >= 200;
    w = DOOMO_MIN(sw - 8, big ? 300 : 236);
    x = (sw - w) / 2;
    tw = w - 14;
    vl = ui_text_wrap(0, 0, tw, tt->voice, 0, 0);
    pl = ui_text_wrap(0, 0, tw - 6, plain, 0, 0);
    h = 17 + vl * FONT_LINE + 4 + pl * FONT_LINE + 6;
    y = sh - h - TIP_BOTTOM;
    /* once it has boinged in the card holds still: a retained panel (G7, platform.h), so a platform that
     * keeps panels copies it instead of drawing the text again. It borrows the toast's id: no toast is
     * drawn while a card is up (game.c). */
    if (s_t >= TIP_SETTLED) {
        if (UI_PANELS) {
            uint32_t key = ui_key_int(ui_panel_key(), s_cur | controls_scheme() << 8 | (s_t > TIP_LOCK) << 12 | input_pad() << 13);
            key = ui_key_int(key, x | y << 9 | w << 18);
            key = ui_key_int(key, ui_window_glint(x, y, w, h));
            if (!plat_panel_begin(PANEL_TOAST, x, y, w + 2, h + 3, key)) {   /* + the drop shadow */
                ui_window_shadow(x, y, w, h);          /* kept: only its shading */
                plat_panel_end();
                return;
            }
            panel = 1;
        }
        ui_window(x, y, w, h, WIN_CREAM);
    } else if (!ui_window_pop(x, y, w, h, WIN_CREAM, s_t)) {
        return;
    }
    /* the speaker: a chip with their emblem, the name, and the legend */
    {
        char g[2];
        int cx = x + 11, cy = y + 9;
        uint8_t chip = tt->spk == SPK_RIVET ? PAL_PAINT(PAINT_TANGERINE, 1) :
                       tt->spk == SPK_FUZZ ? PAL_PAINT(PAINT_SKY, 1) : PAL_PAINT(PAINT_ROSE, 1);
        g[0] = (char)tt->icon;
        g[1] = 0;
        ui_fill_circle(cx, cy + 1, 7, ui_darker(chip));
        ui_fill_circle(cx, cy, 7, chip);
        ui_fill_circle(cx - 2, cy - 3, 2, PAL_WHITE);
        ui_text_outline(cx - 2, cy - 4, g, PAL_WHITE, ui_darker(chip));
        ui_text_shadow(x + 22, y + 5, spk_name[tt->spk], st->accent, st->fill_lo);
        {
            const char *ok = STR_ICON_A " OK";
            int ox = x + w - 8 - ui_text_width(ok);
            if (s_t > TIP_LOCK) ui_text(ox, y + 5, ok, st->text);
            if (s_t < TIP_SETTLED) ui_sparkle(x + w - 5, y + 3, (s_t / 2) & 15, PAL_UI_GOLD);
        }
    }
    ui_text_wrap(x + 7, y + 17, tw, tt->voice, st->text, 1);
    /* the plain instruction on its own band */
    {
        int by = y + 17 + vl * FONT_LINE + 2, bh = pl * FONT_LINE + 3;
        ui_round_rect(x + 4, by, w - 8, bh, 3, st->fill_lo);
        plat_rect(x + 5, by + 1, 2, bh - 2, st->accent);
        ui_text_wrap(x + 10, by + 2, tw - 6, plain, st->text, 1);
    }
    if (panel) plat_panel_end();
}
