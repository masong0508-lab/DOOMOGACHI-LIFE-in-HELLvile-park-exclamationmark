/* story_mg.c - SCENE_STORY_MG, the story's side-minigame shell (owner: S2;
 * docs/design/story/SPEC.md sec.7 S2, C12, C14; docs/STORY_ENGINE.md).
 *
 * arg = a story_mg registry id (story_vm.h): the frame around every side
 * minigame of S4 / S5 (story_mg_a.c, story_mg_b.c, read through the weak
 * registry pointers): a title card, the game's enter / update / draw
 * (update returns 0 while running, else 1 + the result), a result card;
 * the VM reads the result (var 0) after the pop. Built in:
 * STORY_MG_TEXT_ENTRY types the stashed answer (txt_stash 1) to the stashed
 * prompt (txt_stash 0), case and spaces forgiven (Sprinklepop's EXORCISM
 * UNFAILED).
 *
 * On the PS1 the shell, the timing game and S4 / S5's minigames are an
 * overlay group of their own ("storymg", ps1/tools/ovl_plan.py): they are
 * scene edges from the story, which keeps the story group in its slot. The
 * only address that escapes is the text-entry callback (a few resident
 * bytes, it only sets a flag).
 */
#include "story_int.h"

static const struct { uint8_t card_frames, result_frames; } VT = { 26, 40 };

enum { MGS_CARD, MGS_PLAY, MGS_RESULT };
static struct {
    const StoryMgEntry *e;
    uint16_t id;
    uint8_t state, builtin, te_done, te_ok;
    int16_t result;
    uint32_t t, st_t;
    char typed[24];                  /* a typed name (the answers are names) */
} M;
static int16_t mg_last = 0;

int story_mg_result(void) { return mg_last; }

/* --- S4: the registry reader lives here, in the storymg group. stubs.c's
 * weak reader is resident code, and on the PS1 everything resident code
 * reaches stays resident: through it the registry tables, and with them
 * every S4 / S5 game, would be pinned in resident RAM instead of loading
 * with this scene. Reads the tables directly (stubs.c's story_mg_at is
 * resident too). The tables are extern here, so nothing folds them. --- */
const StoryMgEntry *story_mg_find(int id)
{
    const StoryMgEntry *e = doomo_story_mg_a;
    int t;
    for (t = 0; t < 2; t++, e = doomo_story_mg_b)
        for (; e && e->name; e++)
            if (e->id == id) return e;
    return NULL;
}
/* --- end S4 --- */

/* the only escaping address (ui_text_entry's callback): it only sets flags */
static void te_cb(int ok) { M.te_done = 1; M.te_ok = (uint8_t)ok; }

static int same_word(const char *a, const char *b)
{
    for (;;) {
        while (*a == ' ') a++;
        while (*b == ' ') b++;
        if (!*a || !*b) return *a == *b;
        if ((*a | 0x20) != (*b | 0x20)) return 0;
        a++;
        b++;
    }
}

static void mg_enter(int arg)
{
    memset(&M, 0, sizeof(M));
    M.id = (uint16_t)arg;
    M.builtin = M.id == STORY_MG_TEXT_ENTRY;
    if (!M.builtin) M.e = story_mg_find(M.id);
    if (!M.builtin && !M.e) {
        plat_log("story_mg: no such minigame");
        M.state = MGS_RESULT;
        M.result = 0;
    }
    mg_last = 0;
}

static void mg_finish(int result)
{
    M.result = (int16_t)result;
    mg_last = (int16_t)result;
    M.state = MGS_RESULT;
    M.st_t = 0;
    audio_sfx(result > 0 ? SFX_HAPPY : SFX_CANCEL);
}

static void mg_update(void)
{
    M.t++;
    M.st_t++;
    switch (M.state) {
    case MGS_CARD:
        if ((btn_pressed(BTN_A) && M.st_t > 8) || M.st_t >= VT.card_frames * 4) {
            audio_sfx(SFX_SELECT);
            M.state = MGS_PLAY;
            M.st_t = 0;
            if (M.builtin) ui_text_entry(txt_stash_get(0)[0] ? txt_stash_get(0) : "Type it!", M.typed,
                                         (int)sizeof(M.typed), te_cb);
            else if (M.e->enter) M.e->enter(M.id);
        }
        break;
    case MGS_PLAY:
        if (M.builtin) {
            if (M.te_done) mg_finish(M.te_ok && same_word(M.typed, txt_stash_get(1)) ? 1 : 0);
        } else {
            int r = M.e->update ? M.e->update() : 1;
            if (r) mg_finish(r - 1);
        }
        break;
    default:
        if ((btn_pressed(BTN_A) || btn_pressed(BTN_B)) && M.st_t > VT.result_frames / 2) {
            if (scene_depth() > 1) scene_pop();
            else scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
        }
        break;
    }
}

static void mg_card(const char *title, const char *line, int style, int t)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), w = sw - 40, h = sh / 2;
    int x = (sw - w) / 2, y = (sh - h) / 2 - 6, s = ui_pop_scale(t);
    ui_backdrop(style, M.t);
    if (!ui_window_pop(x, y, w, h, style, t)) return;
    {
        int scale = sh >= 200 ? 3 : 2, tw = ui_text_big_width(title, scale);
        if (tw > w - 12) { scale = 2; tw = ui_text_big_width(title, scale); }
        ui_text_big(sw / 2 - tw / 2, y + 10 - (s > 256 ? (s - 256) / 16 : 0), title, scale,
                    ui_style(style)->title, ui_style(style)->outline);
    }
    ui_text_center(sw / 2, y + h - 22, line, ui_style(style)->text);
}

static void mg_draw(void)
{
    char buf[48];
    int sw = plat_screen_w(), sh = plat_screen_h(), k;
    if (plat_draw_skipped()) return;
    switch (M.state) {
    case MGS_CARD:
        mg_card(M.builtin ? "Say It!" : M.e->name,
                M.builtin ? (txt_stash_get(0)[0] ? txt_stash_get(0) : "Type it!") : "A side game!", WIN_CUTE,
                (int)M.st_t);
        ui_hint(STR_ICON_A " Start!", WIN_CUTE);
        break;
    case MGS_PLAY:
        if (M.builtin) ui_backdrop(WIN_DARK, M.t);
        else if (M.e->draw) M.e->draw();
        break;
    default:
        mg_card(M.result > 0 ? "Nice!" : "Aww...", M.result > 0 ? "You did it!" : "Not this time.",
                M.result > 0 ? WIN_MINT : WIN_DARK, (int)M.st_t);
        if (M.result > 0)
            for (k = 0; k < 5; k++)
                ui_sparkle(sw / 2 - 48 + k * 24, sh / 2 + 14 + ui_wave((int)M.t + k * 9, 30, 3),
                           (int)(M.t + (uint32_t)k * 3) & 15, PAL_PAINT(PAINT_LEMON, 2));
        doomo_snprintf(buf, sizeof(buf), STR_ICON_A " Done");
        ui_hint(buf, WIN_CUTE);
        break;
    }
}

const DoomoScene SCENE_STORY_MG = { "STORY_MG", mg_enter, NULL, mg_update, mg_draw, 0 };
