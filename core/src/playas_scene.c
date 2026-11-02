/* playas_scene.c - SCENE_PLAYAS: the character switcher (PA5; docs/PLAYAS.md).
 * A scrolling list of everyone you can play right now: the hero, the recent
 * ring, the family, then the neighbours (alive or ghost). A picks, B leaves.
 * State is 6 B: the rows are recomputed on demand (nth_id), not stored.
 * Strong definition of the weak placeholder in weak_scenes.c. */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/ui.h"
#include "doomo/audio.h"
#include "doomo/input.h"
#include "doomo/platform.h"

static struct { int16_t sel, top, count; } S;

static int in_recent(int id)
{
    int i;
    for (i = 0; i < 4; i++) if (playas_recent(i) == id) return 1;
    return 0;
}

/* the i-th playable id (-1 past the end); `count` = how many there are */
static int nth_id(int want, int *count)
{
    int n = 0, i, id;
#define TAKE(x) do { id = (x); if (playas_can_be(id)) { if (n == want) return id; n++; } } while (0)
    TAKE(PLAYAS_ID_HERO);
    for (i = 0; i < 4; i++) { id = playas_recent(i); if (id != PLAYAS_ID_NONE && id != PLAYAS_ID_HERO) TAKE(id); }
    for (i = 0; i < family_count(); i++) { id = family_member(i); if (!in_recent(id)) TAKE(id); }
    for (id = 0; id < DOOMO_PLAYER_ID; id++) if (!in_recent(id)) TAKE(id);
#undef TAKE
    if (count) *count = n;
    return -1;
}

static const char *name_of(int id, char *buf)
{
    const FamilyIdentity *fi = family_identity(id);
    const DoomoResident *r = town_res(id);
    int i;
    const char *s = fi ? fi->name : (r && r->name[0]) ? r->name : "?";
    for (i = 0; i < DOOMO_NAME_LEN - 1 && s[i]; i++) buf[i] = s[i];
    buf[i] = 0;
    return buf;
}

static const char *kind_of(int id)
{
    if (id == PLAYAS_ID_HERO) return "The hero";
    if (in_recent(id)) return "Played recently";
    if (id >= PLAYAS_ID_FAMILY_FIRST || family_find(id)) return "Family";
    return "Neighbour";
}

static int vis(void) { return plat_screen_h() >= 200 ? 7 : 5; }

static void recount(void)
{
    int c = 0;
    nth_id(-1, &c);
    S.count = (int16_t)c;
}

static void ps_enter(int arg)
{
    (void)arg;
    memset(&S, 0, sizeof S);
    recount();
}

static void ps_update(void)
{
    int n;
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); scene_pop(); return; }
    n = S.count;
    if (n <= 0) return;
    if (btn_repeat(BTN_UP)) { S.sel = (S.sel + n - 1) % n; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { S.sel = (S.sel + 1) % n; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_L)) { S.sel = DOOMO_MAX(0, S.sel - vis()); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_R)) { S.sel = DOOMO_MIN(n - 1, S.sel + vis()); audio_sfx(SFX_CURSOR); }
    if (S.sel < S.top) S.top = S.sel;
    if (S.sel >= S.top + vis()) S.top = S.sel - vis() + 1;
    if (btn_pressed(BTN_A)) {
        int id = nth_id(S.sel, NULL);
        audio_sfx(SFX_SELECT);
        if (id >= 0 && playas_switch(id, PLAYAS_SWITCH_TAGIN)) {
            ui_toast(STR_ICON_STAR " Now playing!");
            scene_pop();
        } else recount();                    /* someone changed under us */
    }
}

static void ps_draw(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int sw = plat_screen_w(), rh = 20, y0 = 34, i, w = sw - 16;
    char nm[DOOMO_NAME_LEN];
    ui_backdrop(WIN_CUTE, (uint32_t)doomo_frame_count());
    ui_window(4, 3, sw - 8, 24, WIN_CUTE);
    ui_text_center(sw / 2, 9, "Play as...", st->text);
    ui_window(6, y0 - 2, w + 4, vis() * rh + 4, WIN_CUTE);
    if (S.count <= 0) {
        ui_text_center(sw / 2, y0 + 16, playas_available() ? "Nobody else right now." :
                       "Unlocks after the story.", st->text_dim);
        return;
    }
    for (i = 0; i < vis() && S.top + i < S.count; i++) {
        int id = nth_id(S.top + i, NULL), y = y0 + i * rh;
        if (id < 0) break;
        if (S.top + i == S.sel) ui_cursor(10, y + 4);
        ui_text(24, y, name_of(id, nm), st->text);
        ui_text(24, y + 9, kind_of(id), st->text_dim);
    }
    if (S.count > vis()) ui_scroll_arrows(sw - 10, y0 - 6, y0 + vis() * rh - 2, S.top > 0, S.top + vis() < S.count);
}

const DoomoScene SCENE_PLAYAS = { "PLAYAS", ps_enter, NULL, ps_update, ps_draw, 0 };
