/* fp_graveyard.c - SCENE_GRAVEYARD (owner: freeplay).
 *
 * Rows of little tombstones under a big moon: each plot shows the name,
 * how they passed, the day, the flowers you left (up to three posies) and,
 * for Monster Mode kills, a red scratch everyone can see. A citizen raised
 * by the rite leaves an empty plot with a happy ghost hovering over it.
 * A on a plot: leave flowers, raise as a ghost (night, Hymn Book + Sage
 * Bundle), remember them (their resident screen).
 */
#include "fp_int.h"

static struct {
    int sel, top, t, choice_open;
    int ghost_x;
} G;

static int cols(void) { return fp_big() ? 6 : 5; }
static int rows_vis(void) { return fp_big() ? 3 : 2; }

static int grave_id(int i)
{
    if (i < 0 || i >= doomo_town.grave_count) return -1;
    return doomo_town.graves[i].resident;
}

static void gy_enter(int arg)
{
    (void)arg;
    memset(&G, 0, sizeof(G));
    G.sel = doomo_town.grave_count ? doomo_town.grave_count - 1 : 0;   /* the newest */
    audio_music(SONG_SAD);
}

static void gy_leave(void)
{
    if (doomo_town.mode == TOWN_FREEPLAY) audio_music(fp_island_song());
}

static int pending_id = -1;

static void cb_grave(int c)
{
    int id = pending_id;
    const DoomoResident *r = fp_res(id);
    if (!r) return;
    switch (c) {
    case 0: {
        int f = fp_flowers(id);
        if (f >= 3) { ui_toast("Their grave is covered in flowers already."); return; }
        if (inv_count(ITEM_FLOWERS) <= 0 && !opt_get(OPT_CHEAT_INFINITE_CANDY)) {
            audio_sfx(SFX_ERROR);
            ui_toast("You need Flowers (the shop sells them).");
            return;
        }
        if (!opt_get(OPT_CHEAT_INFINITE_CANDY)) inv_take(ITEM_FLOWERS, 1);
        fp_set_flowers(id, f + 1);
        audio_sfx(SFX_SPARKLE);
        fp_fx_burst(fp_sw() / 2, fp_sh() / 2, FX_PETAL, 14);
        fp_achieve(UL_FP_GARDENER);
        ui_toast(STR_ICON_HEART " You leave a little posy.");
        break; }
    case 1:
        if (r->status == RES_DEAD) fp_ghost_rite(id);
        else ui_say(-1, "This one's already up and floating about. Boo!");
        break;
    case 2:
        fp_open_resident(id);
        break;
    default: break;
    }
}

static void gy_update(void)
{
    int n = doomo_town.grave_count, c = cols(), vis = rows_vis();
    fp_live();
    G.t++;
    if (G.t % 40 == 0) fp_fx_burst(rng_range(&doomo_rng, fp_sw()), fp_sh() - 30, FX_SPARK, 1);
    if (n) {
        if (btn_repeat(BTN_LEFT)) { G.sel = (G.sel + n - 1) % n; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_RIGHT)) { G.sel = (G.sel + 1) % n; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_UP) && G.sel - c >= 0) { G.sel -= c; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN) && G.sel + c < n) { G.sel += c; audio_sfx(SFX_CURSOR); }
        if (G.sel / c < G.top) G.top = G.sel / c;
        if (G.sel / c >= G.top + vis) G.top = G.sel / c - vis + 1;
        if (btn_pressed(BTN_A)) {
            static const char *const opts[4] = { "Leave flowers", "Raise as a ghost", "Remember them", "Never mind" };
            const DoomoResident *r;
            pending_id = grave_id(G.sel);
            r = fp_res(pending_id);
            if (r) {
                char q[64];
                doomo_snprintf(q, sizeof(q), "%s's grave. What will you do?", r->name);
                audio_sfx(SFX_SELECT);
                ui_choice(-1, q, opts, 4, cb_grave);
            }
            return;
        }
    }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop();
        else fp_goto_island(FP_ISLAND_ARG_RETURN);
    }
}

/* ====================================================================== */
static void tombstone(int x, int y, int w, int h, int id, int sel)
{
    const DoomoResident *r = fp_res(id);
    int risen = r && r->status == RES_GHOST, monster = r && (r->flags & RF_MONSTER_KILL);
    int f = fp_flowers(id), k, t = G.t;
    uint8_t s0 = PAL_RAMP(RAMP_STONE, 3), s1 = PAL_RAMP(RAMP_STONE, 5), s2 = PAL_RAMP(RAMP_STONE, 6);
    char nm[DOOMO_NAME_LEN + 1];
    /* mound */
    ui_fill_ellipse(x + w / 2, y + h - 3, w / 2 - 2, 4, PAL_RAMP(RAMP_GRASS, 2));
    ui_fill_ellipse(x + w / 2, y + h - 4, w / 2 - 4, 3, PAL_RAMP(RAMP_GRASS, 3));
    if (risen) {
        /* an empty, cracked plot and a happy ghost bobbing above */
        int gx = x + w / 2, gy = y + 8 + ui_wave(t + id * 13, 50, 2);
        plat_rect(x + w / 2 - 6, y + h - 8, 12, 3, PAL_RAMP(RAMP_WOOD, 1));
        ui_fill_circle(gx, gy, 5, PAL_RAMP(RAMP_MINT, 6));
        plat_rect(gx - 5, gy, 11, 5, PAL_RAMP(RAMP_MINT, 6));
        plat_rect(gx - 5, gy + 5, 2, 1, PAL_RAMP(RAMP_MINT, 6));
        plat_rect(gx - 1, gy + 5, 2, 1, PAL_RAMP(RAMP_MINT, 6));
        plat_rect(gx + 3, gy + 5, 2, 1, PAL_RAMP(RAMP_MINT, 6));
        plat_rect(gx - 2, gy - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(gx + 2, gy - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(gx - 1, gy + 2, 3, 1, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(gx - 3, gy - 3, 1, 1, PAL_WHITE);
    } else {
        int tw = w - 12, th = h - 16, tx = x + 6, ty = y + 6;
        ui_round_rect(tx - 1, ty - 1, tw + 2, th + 2, 5, PAL_RAMP(RAMP_STONE, 1));
        ui_round_rect(tx, ty, tw, th, 4, s1);
        plat_rect(tx + 1, ty + 3, 2, th - 4, s2);
        plat_rect(tx + tw - 3, ty + 3, 2, th - 4, s0);
        plat_rect(tx + 4, ty + 1, tw - 8, 1, PAL_WHITE);
        /* R.I.P. + the cause icon */
        plat_text(tx + tw / 2 - 2, ty + 4, monster ? STR_ICON_SKULL : STR_ICON_HEART,
                  monster ? PAL_UI_RED : PAL_RAMP(RAMP_STONE, 2));
        if (monster) {
            /* the scratch of shame */
            plat_line(tx + 2, ty + 3, tx + tw - 3, ty + th - 4, PAL_UI_RED_D);
            plat_line(tx + 3, ty + 3, tx + tw - 2, ty + th - 4, PAL_UI_RED);
        }
    }
    for (k = 0; k < f; k++) {
        int fx = x + 8 + k * 7, fy = y + h - 6;
        plat_rect(fx, fy, 1, 3, PAL_RAMP(RAMP_GRASS, 4));
        plat_rect(fx - 1, fy - 1, 3, 1, k == 1 ? PAL_PAINT(PAINT_LEMON, 1) : PAL_PAINT(PAINT_BUBBLEGUM, 1));
        plat_rect(fx, fy - 2, 1, 3, k == 1 ? PAL_PAINT(PAINT_LEMON, 1) : PAL_PAINT(PAINT_BUBBLEGUM, 1));
        plat_rect(fx, fy - 1, 1, 1, PAL_UI_GOLD);
    }
    fp_strcpy(nm, r ? r->name : "?", sizeof(nm));
    if ((int)strlen(nm) * FONT_ADVANCE > w) nm[w / FONT_ADVANCE] = 0;
    ui_text_outline(x + w / 2 - ui_text_width(nm) / 2, y + h + 1, nm,
                    sel ? PAL_UI_GOLD : PAL_RAMP(RAMP_BONE, 7), PAL_RAMP(RAMP_STONE, 0));
    if (sel) {
        ui_frame(x, y, w, h + 10, (t / 6) & 1 ? PAL_UI_GOLD : PAL_WHITE);
        ui_cursor(x + 1, y + 1);
    }
}

static void gy_draw(void)
{
    int sw = fp_sw(), sh = fp_sh(), big = fp_big(), n = doomo_town.grave_count;
    int c = cols(), vis = rows_vis(), i, t = G.t;
    int cw = (sw - 16) / c, chh = big ? 56 : 44, gx0 = 8, gy0 = big ? 52 : 36;
    const UiStyle *st = ui_style(WIN_DARK);
    char buf[80];
    /* night sky, big moon, mist */
    plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_SKY, 1));
    plat_rect(0, sh / 4, sw, sh / 4, PAL_RAMP(RAMP_VAPE, 1));
    plat_checker(0, sh / 4, sw, 1, PAL_RAMP(RAMP_SKY, 1));
    ui_fill_circle(sw - 40, 26, 16, PAL_RAMP(RAMP_BONE, 6));
    ui_fill_circle(sw - 40, 26, 14, PAL_RAMP(RAMP_BONE, 7));
    ui_fill_circle(sw - 45, 22, 3, PAL_RAMP(RAMP_BONE, 5));
    ui_fill_circle(sw - 35, 31, 2, PAL_RAMP(RAMP_BONE, 5));
    for (i = 0; i < 12; i++) {
        uint32_t h = doomo_hash32((uint32_t)i * 7919u + 1u);
        ui_sparkle((int)(h % (uint32_t)sw), (int)((h >> 9) % (uint32_t)(sh / 3)), (t / 4 + i * 3) & 15, PAL_WHITE);
    }
    plat_rect(0, sh / 2 - 8, sw, sh / 2 + 8, PAL_RAMP(RAMP_GRASS, 2));
    plat_rect(0, sh / 2 - 8, sw, 2, PAL_RAMP(RAMP_GRASS, 3));
    /* the iron fence */
    for (i = 0; i < sw; i += 8) {
        plat_rect(i, sh / 2 - 22, 2, 16, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(i - 1, sh / 2 - 23, 4, 2, PAL_RAMP(RAMP_STONE, 0));
    }
    plat_rect(0, sh / 2 - 16, sw, 2, PAL_RAMP(RAMP_STONE, 0));
    /* the wandering friendly ghost */
    {
        int gx = (t / 2) % (sw + 40) - 20, gyy = sh / 2 - 34 + ui_wave(t, 60, 3);
        ui_fill_circle(gx, gyy, 5, PAL_RAMP(RAMP_MINT, 6));
        plat_rect(gx - 5, gyy, 11, 5, PAL_RAMP(RAMP_MINT, 6));
        plat_rect(gx - 2, gyy - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(gx + 2, gyy - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(gx - 5 + ((t / 6) & 1), gyy + 5, 2, 1, PAL_RAMP(RAMP_MINT, 6));
        plat_rect(gx + 2 - ((t / 6) & 1), gyy + 5, 2, 1, PAL_RAMP(RAMP_MINT, 6));
    }
    ui_title(sw / 2, 3, "Graveyard", WIN_DARK);
    if (!n) {
        int bx = sw / 2 - 60, by = sh / 2 - 6;
        ui_window(bx, by, 120, 46, WIN_DARK);
        ui_text_center(sw / 2, by + 8, "No graves yet!", st->accent);
        ui_text_center(sw / 2, by + 20, "Everyone's alive", st->text);
        ui_text_center(sw / 2, by + 30, "and fluffy.", st->text);
    }
    for (i = 0; i < vis * c; i++) {
        int gi = G.top * c + i;
        int x = gx0 + (i % c) * cw, y = gy0 + (i / c) * (chh + 14);
        if (gi >= n) break;
        tombstone(x + 2, y, cw - 4, chh, grave_id(gi), gi == G.sel);
    }
    if (n > vis * c)
        ui_scroll_arrows(sw - 8, gy0 - 6, gy0 + vis * (chh + 14) - 6, G.top > 0, (G.top + vis) * c < n);
    /* info card */
    if (n) {
        int id = grave_id(G.sel), iy = sh - (big ? 44 : 34);
        const DoomoResident *r = fp_res(id);
        ui_window(4, iy, sw - 8, big ? 28 : 20, WIN_DARK);
        if (r) {
            const char *cause = doomo_death_names[r->death_cause % DEATH_COUNT];
            if (r->status == RES_GHOST)
                doomo_snprintf(buf, sizeof(buf), "%s - risen as a ghost! Boo!", r->name);
            else if (r->flags & RF_MONSTER_KILL)
                doomo_snprintf(buf, sizeof(buf), "%s - day %u - killed by a MONSTER", r->name,
                               (unsigned)doomo_town.graves[G.sel].day + 1);
            else
                doomo_snprintf(buf, sizeof(buf), "%s - day %u - %s", r->name,
                               (unsigned)doomo_town.graves[G.sel].day + 1, cause);
            fp_marquee(10, iy + 5, sw - 20, buf, (r->flags & RF_MONSTER_KILL) ? PAL_UI_RED : st->text, t);
            if (big) {
                const char *why = fp_ghost_rite_blocker(id);
                ui_text(10, iy + 15, why[0] ? why : "The veil is thin tonight... a rite may work.",
                        why[0] ? st->text_dim : PAL_UI_GOLD);
            }
        }
    }
    ui_hint(n ? STR_ICON_A " Pay respects  " STR_ICON_B " Back" : STR_ICON_B " Back", WIN_DARK);
    fp_fx_draw();
}

const DoomoScene SCENE_GRAVEYARD = { "GRAVEYARD", gy_enter, gy_leave, gy_update, gy_draw, 0 };
