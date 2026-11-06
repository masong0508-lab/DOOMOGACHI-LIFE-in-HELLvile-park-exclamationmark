/* playas_scene.c - SCENE_PLAYAS: the character switcher (PA5; docs/PLAYAS.md).
 * A scrolling list of everyone you can play right now: the hero, the recent
 * ring, the family, then the neighbours (alive or ghost). A picks, B leaves.
 * State is 6 B: the rows are recomputed on demand (nth_id), not stored.
 * Strong definition of the weak placeholder in weak_scenes.c. */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/family.h"
#include "doomo/playas.h"
#include "doomo/profiles.h"
#include "doomo/ui.h"
#include "doomo/audio.h"
#include "doomo/input.h"
#include "doomo/platform.h"

/* nl: picking who the New life starts as; direct: opened on that list (the title);
 * pf: 0 the switcher, 1 the players list (MP0 profiles), 2 picking the villager a new player is;
 * pick: that villager while the confirm is up */
static struct { int16_t sel, top, count; uint8_t nl, direct, pf; int16_t rows, pick; } S;
static char nl_q[48];
enum { RK_ID, RK_NEWLIFE, RK_PLAYERS };

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

/* The rows of "Who joins?": the co-op heroes nobody plays yet (MP1), then the villagers a new player can be
 * (alive, not the hero, not already a player). A row is a villager id, or -(hero kind) for a co-op hero. */
static int coop_free(int want, int *count)
{
    int n = 0, k;
    for (k = HERO_COOP_FIRST; k < HERO_COOP_FIRST + HERO_COOP_FILLED; k++) {
        if (prof_hero_used(k)) continue;
        if (n == want) return -k;
        n++;
    }
    if (count) *count = n;
    return 0;
}

static int pick_id(int want, int *count)
{
    int n = 0, id, nc = 0;
    if (want >= 0) { int c = coop_free(want, NULL); if (c) return c; }
    coop_free(-1, &nc);
    n = nc;
    for (id = 0; id < DOOMO_PLAYER_ID; id++) {
        const DoomoResident *r = town_res(id);
        if (!r || r->status != RES_ALIVE || !playas_can_be(id) || prof_find_resident(id) >= 0) continue;
        if (n == want) return id;
        n++;
    }
    if (count) *count = n;
    return want >= 0 ? DOOMO_PLAYER_ID + 1000 : -1;   /* past the end */
}

static int can_add(void) { return prof_guest_count() < PROF_GUESTS && playas_available(); }

static int has_players_row(void) { return !S.nl && playas_available(); }

/* what row `sel` is in the switcher */
static int row_kind(int sel)
{
    if (sel < S.count) return RK_ID;
    return (sel == S.count && !S.nl && playas_new_life_ok()) ? RK_NEWLIFE : RK_PLAYERS;
}

static void recount(void)
{
    int c = 0;
    if (S.pf == 1) { S.count = (int16_t)prof_count(); S.rows = (int16_t)(S.count + (can_add() ? 1 : 0)); return; }
    if (S.pf == 2) { pick_id(-1, &c); S.count = S.rows = (int16_t)c; return; }
    nth_id(-1, &c);
    S.count = (int16_t)c;
    S.rows = (int16_t)(c + (!S.nl && playas_new_life_ok() ? 1 : 0) + (has_players_row() ? 1 : 0));
}

static void ps_enter(int arg)
{
    memset(&S, 0, sizeof S);
    if (arg == PLAYAS_ARG_PICK_NEWLIFE && playas_new_life_ok()) S.nl = S.direct = 1;
    if (arg == PLAYAS_ARG_PROFILES && playas_available()) S.pf = S.direct = 1;
    recount();
}

static void nl_done(int yes)
{
    int id = nth_id(S.sel, NULL);
    if (!yes) { S.nl = S.direct; recount(); return; }
    if (id >= 0 && playas_new_life(id)) {
        char nm[DOOMO_NAME_LEN], line[80];
        ui_toast(STR_ICON_STAR " A new life begins!");
        scene_pop();
        /* the first day: whose it is and what is in the pocket (PA10 part 2) */
        doomo_snprintf(line, sizeof line, "%s's first day. H$%u in the pocket, and a snack or two.",
                       name_of(playas_avatar(), nm), (unsigned)doomo_town.money);
        ui_say(-1, line);
    } else { S.nl = S.direct; recount(); }
}

static void nl_start(int yes)
{
    if (!yes) return;
    S.nl = 1;
    S.sel = S.top = 0;
    recount();
}

static char pf_name[PROF_NAME_LEN];

/* the name was typed (ok) or the entry cancelled: an empty name falls back to the villager's */
static void pf_join(int ok)
{
    char nm[DOOMO_NAME_LEN], msg[40];
    int r;
    if (!ok) return;
    if (!pf_name[0]) {
        if (S.pick < 0) { strncpy(pf_name, hero_preset(-S.pick)->name, PROF_NAME_LEN - 1); pf_name[PROF_NAME_LEN - 1] = 0; }
        else name_of(S.pick, pf_name);
    }
    r = S.pick < 0 ? prof_guest_add(pf_name, -1, -S.pick) : prof_guest_add(pf_name, S.pick, -1);
    if (r > 0) {
        doomo_snprintf(msg, sizeof msg, "%s joined!", prof_name(r));
        ui_toast(msg);
        S.pf = 1; recount(); S.sel = (int16_t)r; S.top = 0;
    } else {
        (void)nm;
        ui_toast(r == PROF_ERR_FULL ? "The island has its players." :
                 r == PROF_ERR_HOUSEHOLD ? "The household is full." :
                 r == PROF_ERR_NAME ? "Use letters and numbers." : "They can't join right now.");
        S.pf = 1; recount();
    }
}

static void pf_renamed(int ok)
{
    if (!ok) return;
    if (!pf_name[0] || prof_rename(S.sel, pf_name) != 1) ui_toast("Use letters and numbers.");
    else ui_toast("Renamed!");
    recount();
}

static void pf_leave(int yes)
{
    if (!yes) return;
    if (prof_guest_remove(S.sel) == 1) ui_toast("Moved out.");
    recount();
    if (S.sel >= S.rows) S.sel = (int16_t)(S.rows > 0 ? S.rows - 1 : 0);
}

/* the players list and the villager pick; 1 when it handled the frame */
static int pf_update(void)
{
    int n = S.rows;
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (S.pf == 2) { S.pf = 1; S.sel = S.top = 0; recount(); }
        else if (S.direct) scene_pop();
        else { S.pf = 0; S.sel = S.top = 0; recount(); }
        return 1;
    }
    if (n <= 0) return 1;
    if (btn_repeat(BTN_UP)) { S.sel = (S.sel + n - 1) % n; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { S.sel = (S.sel + 1) % n; audio_sfx(SFX_CURSOR); }
    if (S.sel < S.top) S.top = S.sel;
    if (S.sel >= S.top + vis()) S.top = S.sel - vis() + 1;
    if (S.pf == 1 && btn_pressed(BTN_SELECT) && S.sel >= 1 && S.sel < S.count) {
        audio_sfx(SFX_SELECT);
        doomo_snprintf(nl_q, sizeof nl_q, "%s moves out?", prof_name(S.sel));
        ui_confirm(nl_q, pf_leave);
        return 1;
    }
    if (S.pf == 1 && btn_pressed(BTN_START) && S.sel >= 1 && S.sel < S.count) {   /* rename a guest */
        audio_sfx(SFX_SELECT);
        strncpy(pf_name, prof_name(S.sel), PROF_NAME_LEN - 1);
        pf_name[PROF_NAME_LEN - 1] = 0;
        ui_text_entry("Rename the player", pf_name, PROF_NAME_LEN, pf_renamed);
        return 1;
    }
    if (btn_pressed(BTN_A)) {
        audio_sfx(SFX_SELECT);
        if (S.pf == 2) {
            S.pick = (int16_t)pick_id(S.sel, NULL);
            if (S.pick > DOOMO_PLAYER_ID) { recount(); return 1; }
            if (S.pick < 0) { strncpy(pf_name, hero_preset(-S.pick)->name, PROF_NAME_LEN - 1); pf_name[PROF_NAME_LEN - 1] = 0; }
            else name_of(S.pick, pf_name);       /* the villager's name to start from */
            ui_text_entry("Name the player", pf_name, PROF_NAME_LEN, pf_join);
        } else if (S.sel >= S.count) {
            S.pf = 2; S.sel = S.top = 0; recount();
            if (S.rows <= 0) { ui_toast("Nobody free to join."); S.pf = 1; recount(); }
        } else if (prof_switch(S.sel)) {
            ui_toast(STR_ICON_STAR " Now playing!");
            scene_pop();
        } else ui_toast("Not right now.");
    }
    return 1;
}

static void ps_update(void)
{
    int n;
    if (S.pf) { pf_update(); return; }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (S.nl && !S.direct) { S.nl = 0; S.sel = S.top = 0; recount(); } else scene_pop();
        return;
    }
    n = S.rows;
    if (n <= 0) return;
    if (btn_repeat(BTN_UP)) { S.sel = (S.sel + n - 1) % n; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { S.sel = (S.sel + 1) % n; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_L)) { S.sel = DOOMO_MAX(0, S.sel - vis()); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_R)) { S.sel = DOOMO_MIN(n - 1, S.sel + vis()); audio_sfx(SFX_CURSOR); }
    if (S.sel < S.top) S.top = S.sel;
    if (S.sel >= S.top + vis()) S.top = S.sel - vis() + 1;
    if (btn_pressed(BTN_A)) {
        int rk = row_kind(S.sel);
        int id = rk == RK_ID ? nth_id(S.sel, NULL) : -1;
        audio_sfx(SFX_SELECT);
        if (rk == RK_PLAYERS) {                  /* PA10 part 4: the players list */
            S.pf = 1; S.sel = S.top = 0; recount();
        } else if (rk == RK_NEWLIFE) {           /* PA6: the "Start a new life" row */
            ui_confirm("Start a new life? The story stays finished. You pick a villager to be.", nl_start);
        } else if (S.nl && id >= 0) {
            char nm[DOOMO_NAME_LEN];
            doomo_snprintf(nl_q, sizeof nl_q, "Start a new life as %s?", name_of(id, nm));
            ui_confirm(nl_q, nl_done);
        } else if (id >= 0 && playas_switch(id, PLAYAS_SWITCH_TAGIN)) {
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
    ui_text_center(sw / 2, 9, S.pf == 2 ? "Who joins?" : S.pf ? "Players" : S.nl ? "New life: who are you?" : "Play as...", st->text);
    ui_window(6, y0 - 2, w + 4, vis() * rh + 4, WIN_CUTE);
    if (S.rows <= 0) {
        ui_text_center(sw / 2, y0 + 16, playas_available() ? "Nobody else right now." :
                       "Unlocks after the story.", st->text_dim);
        return;
    }
    if (S.pf) {
        for (i = 0; i < vis() && S.top + i < S.rows; i++) {
            int row = S.top + i, y = y0 + i * rh, id;
            if (row == S.sel) ui_cursor(10, y + 4);
            if (S.pf == 2) {
                id = pick_id(row, NULL);
                if (id > DOOMO_PLAYER_ID) break;
                if (id < 0) {
                    ui_text(24, y, hero_preset(-id)->name, st->text);
                    ui_text(24, y + 9, "Co-op hero", st->text_dim);
                } else {
                    ui_text(24, y, name_of(id, nm), st->text);
                    ui_text(24, y + 9, "Neighbour", st->text_dim);
                }
            } else if (row >= S.count) {
                ui_text(24, y, "Add a player...", st->text);
                ui_text(24, y + 9, "A co-op hero or a villager", st->text_dim);
            } else {
                char sub[40];
                id = prof_resident(row);
                ui_text(24, y, prof_name(row), st->text);
                if (row == 0) doomo_snprintf(sub, sizeof sub, "The hero%s", prof_active() == 0 ? " - playing" : "");
                else if (prof_is_coop(row)) doomo_snprintf(sub, sizeof sub, "Co-op hero%s", prof_active() == row ? " - playing" : "");
                else doomo_snprintf(sub, sizeof sub, "Plays %s%s", name_of(id, nm), prof_active() == row ? " - playing" : "");
                ui_text(24, y + 9, sub, st->text_dim);
            }
        }
        if (S.rows > vis()) ui_scroll_arrows(sw - 10, y0 - 6, y0 + vis() * rh - 2, S.top > 0, S.top + vis() < S.rows);
        if (plat_screen_h() >= 200 && S.pf == 1) ui_text_center(sw / 2, y0 + vis() * rh + 8, "A play  START rename  SELECT out", st->text_dim);
        return;
    }
    for (i = 0; i < vis() && S.top + i < S.rows; i++) {
        int id = S.top + i < S.count ? nth_id(S.top + i, NULL) : -1, y = y0 + i * rh;
        if (S.top + i == S.sel) ui_cursor(10, y + 4);
        if (S.top + i >= S.count) {
            if (row_kind(S.top + i) == RK_PLAYERS) {
                ui_text(24, y, "Players...", st->text);
                ui_text(24, y + 9, "Share the island", st->text_dim);
            } else {
                ui_text(24, y, "Start a new life...", st->text);
                ui_text(24, y + 9, "Begin again as a villager", st->text_dim);
            }
            continue;
        }
        if (id < 0) break;
        ui_text(24, y, name_of(id, nm), st->text);
        ui_text(24, y + 9, kind_of(id), st->text_dim);
    }
    if (S.rows > vis()) ui_scroll_arrows(sw - 10, y0 - 6, y0 + vis() * rh - 2, S.top > 0, S.top + vis() < S.rows);
}

const DoomoScene SCENE_PLAYAS = { "PLAYAS", ps_enter, NULL, ps_update, ps_draw, 0 };
