/* fp_title.c - SCENE_TITLE (owner: freeplay).
 *
 * "DOOMOGACHI LIFE in HELLvile Park!": a candy logo that drops in letter by
 * letter and keeps bouncing, two plush citizens waving beside the menu
 * (they take turns: a parade of premade favourites), sparkles and hearts
 * in a themed sky. Every few seconds the "HELL" glitches and drips red -
 * foreshadowing. Six title themes (OPT_TITLE_THEME, some unlockable) can
 * be flipped right here with LEFT / RIGHT on the theme row.
 *
 * Menu: Story Mode (New story / Continue), Free Play (New island /
 * Continue), Options, Unlocks, Credits.
 */
#include "fp_int.h"

enum { M_MAIN, M_STORY, M_FREE };
enum { I_STORY, I_FREE, I_OPTIONS, I_UNLOCKS, I_CREDITS, I_THEME, I_MAIN_N };
enum { THEME_DAWN, THEME_NIGHT, THEME_CANDY, THEME_SUNSET, THEME_VAPE, THEME_UNITED, THEME_N };

static const char *const main_items[I_MAIN_N] = {
    "Story Mode", "Free Play", "Options", "Unlocks", "Credits", "Theme"
};

/* premade favourites who take turns waving on the title (roster names) */
static const char *const parade[] = {
    "Pip", "Rivet", "Nana Puddin", "Vex", "Father Fuzz", "Mayor Giggs", "Dr. Bloom",
    "Keysie", "Gutterworth", "Cardamom", "Fangela", "Shushana", "Maestro Mew", "Swipey"
};
#define NPARADE DOOMO_COUNTOF(parade)

static struct {
    uint8_t menu, started, sel, sub_sel;
    int t, menu_t;
    int glitch_t, next_glitch;
    int drip_x[8], drip_y[8], drip_v[8];
    int parade_k;
    uint8_t has_story, has_free;
} T;

static int theme(void) { return opt_get(OPT_TITLE_THEME) % THEME_N; }

static int theme_ok(int v) { return unlock_ok(UL_CAT_SKIN, OPT_TITLE_THEME, v); }

static void refresh_saves(void)
{
    DoomoSaveInfo si;
    T.has_story = (uint8_t)(save_peek(0, &si) && si.mode == TOWN_STORY);
    T.has_free = (uint8_t)(save_peek(1, &si) && si.mode == TOWN_FREEPLAY);
}

static void title_enter(int arg)
{
    (void)arg;
    memset(&T, 0, sizeof(T));
    T.next_glitch = 70;
    T.parade_k = (int)(plat_entropy() % NPARADE) & ~1;
    refresh_saves();
    audio_music(SONG_TITLE);
    fp_fx_clear();
}

/* ====================================================================== */
/* actions                                                                  */
static void start_story(void)
{
    audio_sfx(SFX_SELECT);
    story_begin();
    if (scene_top() == &SCENE_TITLE) {
        /* story module not available: set it up by the contract ourselves */
        town_new(TOWN_STORY, plat_entropy(), TOWN_POP_PREMADE);
        sim_init();
        scene_set(&SCENE_STORY, STORY_ARG_NEW);
    }
}

static void continue_story(void)
{
    if (!load_game(0)) {
        audio_sfx(SFX_ERROR);
        doomo_snprintf(fp_tmp, sizeof(fp_tmp), "Couldn't load: %s", save_last_error()[0] ? save_last_error() : "no save");
        ui_toast(fp_tmp);
        return;
    }
    audio_sfx(SFX_SELECT);
    sim_init();
    scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
}

static void continue_free(void)
{
    if (!load_game(1) || doomo_town.mode != TOWN_FREEPLAY) {
        audio_sfx(SFX_ERROR);
        doomo_snprintf(fp_tmp, sizeof(fp_tmp), "Couldn't load: %s", save_last_error()[0] ? save_last_error() : "no save");
        ui_toast(fp_tmp);
        return;
    }
    audio_sfx(SFX_HAPPY);
    sim_init();
    fp_autosave_bind(1);
    fp_goto_island(FP_ISLAND_ARG_RETURN);
    ui_toast(STR_ICON_HEART " Welcome back!");
}

static void cycle_theme(int d)
{
    int v = theme(), i, old = v;
    for (i = 0; i < THEME_N; i++) {
        v = (v + THEME_N + d) % THEME_N;
        if (theme_ok(v)) break;
    }
    if (v == old) {
        const DoomoUnlock *u = unlock_find(UL_CAT_SKIN, OPT_TITLE_THEME, (old + 1) % THEME_N);
        audio_sfx(SFX_ERROR);
        doomo_snprintf(fp_tmp, sizeof(fp_tmp), "More themes: %s", u && u->how ? u->how : "keep playing!");
        ui_toast(fp_tmp);
        return;
    }
    opt_set(OPT_TITLE_THEME, v);
    audio_sfx(SFX_SPARKLE);
    fp_fx_burst(fp_sw() / 2, 40, FX_SPARK, 10);
}

static int sub_count(void) { return 3; }

static void title_update(void)
{
    int sw = fp_sw();
    fp_fx_tick();
    T.t++;
    /* HELL glitch timer */
    if (T.glitch_t > 0) T.glitch_t--;
    else if (--T.next_glitch <= 0) {
        int i;
        T.glitch_t = 14;
        T.next_glitch = (theme() == THEME_VAPE ? 50 : 110) + rng_range(&doomo_rng, 90);
        for (i = 0; i < 8; i++) {
            if (T.drip_v[i]) continue;
            T.drip_x[i] = rng_range(&doomo_rng, 4 * 12);
            T.drip_y[i] = 0;
            T.drip_v[i] = 1 + rng_range(&doomo_rng, 2);
            if (rng_chance(&doomo_rng, 50)) break;
        }
        if (theme() >= THEME_SUNSET) audio_sfx(SFX_POSSESS);
    }
    {
        int i;
        for (i = 0; i < 8; i++) {
            if (!T.drip_v[i]) continue;
            if ((T.t & 1) == 0) T.drip_y[i] += T.drip_v[i];
            if (T.drip_y[i] > 26) T.drip_v[i] = 0;
        }
    }
    /* the waving pair takes turns every 8 seconds */
    if (T.t % 240 == 239) {
        T.parade_k = (T.parade_k + 2) % (NPARADE & ~1);
        fp_fx_burst(30, fp_sh() - 50, FX_HEART, 6);
        fp_fx_burst(sw - 30, fp_sh() - 50, FX_STAR, 6);
    }
    if (T.t % 50 == 0) fp_fx_burst(rng_range(&doomo_rng, sw), fp_sh() - 4, FX_HEART, 1);
    if (!T.started) {
        if (T.t > 20 && btn_pressed(BTN_A | BTN_START)) {
            T.started = 1;
            T.menu_t = 0;
            audio_sfx(SFX_SELECT);
            fp_fx_burst(sw / 2, fp_sh() / 2, FX_CONFETTI, 20);
        }
        return;
    }
    T.menu_t++;
    if (T.menu == M_MAIN) {
        if (btn_repeat(BTN_UP)) { T.sel = (uint8_t)((T.sel + I_MAIN_N - 1) % I_MAIN_N); audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { T.sel = (uint8_t)((T.sel + 1) % I_MAIN_N); audio_sfx(SFX_CURSOR); }
        if (T.sel == I_THEME && btn_repeat(BTN_LEFT)) cycle_theme(-1);
        if (T.sel == I_THEME && btn_repeat(BTN_RIGHT)) cycle_theme(1);
        if (btn_pressed(BTN_B)) { T.started = 0; audio_sfx(SFX_CANCEL); return; }
        if (!btn_pressed(BTN_A)) return;
        switch (T.sel) {
        case I_STORY: T.menu = M_STORY; T.sub_sel = (uint8_t)(T.has_story ? 1 : 0); T.menu_t = 0; audio_sfx(SFX_SELECT); break;
        case I_FREE: T.menu = M_FREE; T.sub_sel = (uint8_t)(T.has_free ? 1 : 0); T.menu_t = 0; audio_sfx(SFX_SELECT); break;
        case I_OPTIONS: audio_sfx(SFX_SELECT); scene_push(&SCENE_OPTIONS, 0); break;
        case I_UNLOCKS: audio_sfx(SFX_SELECT); scene_push(&SCENE_UNLOCKS, 0); break;
        case I_CREDITS: audio_sfx(SFX_SELECT); scene_push(&SCENE_CREDITS, 0); break;
        default: cycle_theme(1); break;
        }
        return;
    }
    if (btn_repeat(BTN_UP)) { T.sub_sel = (uint8_t)((T.sub_sel + sub_count() - 1) % sub_count()); audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { T.sub_sel = (uint8_t)((T.sub_sel + 1) % sub_count()); audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_B) || (btn_pressed(BTN_A) && T.sub_sel == 2)) {
        T.menu = M_MAIN;
        T.menu_t = 0;
        audio_sfx(SFX_CANCEL);
        return;
    }
    if (!btn_pressed(BTN_A)) return;
    if (T.menu == M_STORY) {
        if (T.sub_sel == 0) start_story();
        else if (T.has_story) continue_story();
        else { audio_sfx(SFX_ERROR); ui_toast("No story save yet!"); }
    } else {
        if (T.sub_sel == 0) { audio_sfx(SFX_SELECT); scene_set(&SCENE_NEW_ISLAND, 0); }
        else if (T.has_free) continue_free();
        else { audio_sfx(SFX_ERROR); ui_toast("No island saved yet!"); }
    }
}

/* ====================================================================== */
/* drawing: themed sky                                                      */
static void draw_sky(void)
{
    static const uint8_t skies[THEME_N][4] = {
        { PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6), PAL_RAMP(RAMP_PEACH, 7), PAL_RAMP(RAMP_CANDY, 7) },
        { PAL_RAMP(RAMP_SKY, 0), PAL_RAMP(RAMP_SKY, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3) },
        { PAL_RAMP(RAMP_MINT, 7), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_CANDY, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6) },
        { PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_BLOOD, 4), PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_LAVA, 6) },
        { PAL_RAMP(RAMP_STONE, 0), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_SICK, 2) },
        { PAL_RAMP(RAMP_SKY, 5), PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_SKY, 7), PAL_RAMP(RAMP_MINT, 7) }
    };
    int sw = fp_sw(), sh = fp_sh(), th = theme(), i, t = T.t, bh = sh * 2 / 3 / 4;
    for (i = 0; i < 4; i++) {
        plat_rect(0, i * bh, sw, bh + 1, skies[th][i]);
        if (i) plat_checker(0, i * bh, sw, 1, skies[th][i - 1]);
    }
    /* sun / moon */
    switch (th) {
    case THEME_DAWN: case THEME_UNITED: case THEME_CANDY: {
        int cx = sw - 34, cy = 24 + ui_wave(t, 200, 2);
        ui_fill_circle(cx, cy, 15, PAL_RAMP(RAMP_CANDY, 6));
        ui_fill_circle(cx, cy, 12, PAL_RAMP(RAMP_CANDY, 7));
        ui_fill_circle(cx - 4, cy - 4, 4, PAL_WHITE);
        break; }
    case THEME_NIGHT: {
        int cx = sw - 34, cy = 24;
        ui_fill_circle(cx, cy, 11, PAL_RAMP(RAMP_CANDY, 7));
        ui_fill_circle(cx + 5, cy - 3, 9, skies[th][0]);
        for (i = 0; i < 24; i++) {
            uint32_t h = doomo_hash32((uint32_t)i * 7919u);
            ui_sparkle((int)(h % (uint32_t)sw), (int)((h >> 9) % (uint32_t)(sh * 2 / 3)), (t / 3 + i * 3) & 15,
                       i & 1 ? PAL_WHITE : PAL_RAMP(RAMP_CANDY, 7));
        }
        break; }
    case THEME_SUNSET: {
        int cx = sw / 2, cy = sh * 2 / 3 - 6;
        ui_fill_circle(cx, cy, 26, PAL_RAMP(RAMP_LAVA, 6));
        ui_fill_circle(cx, cy, 22, PAL_RAMP(RAMP_LAVA, 7));
        for (i = 0; i < 3; i++) {
            int bx = (t * (2 + i) / 3 + i * 90) % (sw + 40) - 20, by = 30 + i * 14 + ui_wave(t * 3 + i * 40, 30, 3);
            plat_rect(bx - 3, by, 7, 1, PAL_RAMP(RAMP_STONE, 0));
            plat_rect(bx - 1, by + 1, 3, 1, PAL_RAMP(RAMP_STONE, 0));
            plat_rect(bx - 4 - ((t / 4) & 1), by - 1 + ((t / 4) & 1), 2, 1, PAL_RAMP(RAMP_STONE, 0));
            plat_rect(bx + 3 + ((t / 4) & 1), by - 1 + ((t / 4) & 1), 2, 1, PAL_RAMP(RAMP_STONE, 0));
        }
        break; }
    case THEME_VAPE:
        for (i = 0; i < 6; i++) {
            int cx = (i * 61 + t / (2 + (i & 1))) % (sw + 60) - 30, cy = 20 + (i * 37) % (sh / 2);
            ui_fill_ellipse(cx, cy, 22, 8, PAL_RAMP(RAMP_VAPE, 3 + (i & 1)));
            ui_fill_ellipse(cx + 6, cy - 3, 12, 5, PAL_RAMP(RAMP_VAPE, 5));
        }
        for (i = 0; i < 12; i++) {
            uint32_t h = doomo_hash32((uint32_t)i * 131u + 7u);
            int x = (int)(h % (uint32_t)sw), y = sh - ((t * (1 + (int)(h >> 30)) + (int)(h >> 8)) % sh);
            plat_rect(x, y, 1, 2, PAL_RAMP(RAMP_LAVA, 6));
        }
        break;
    default: break;
    }
    /* drifting clouds (not at night / vape) */
    if (th != THEME_NIGHT && th != THEME_VAPE) {
        for (i = 0; i < 3; i++) {
            int cx = (i * 97 + t / (3 + i)) % (sw + 80) - 40, cy = 14 + i * 22;
            uint8_t c = th == THEME_SUNSET ? PAL_RAMP(RAMP_PASTEL_PINK, 4) : PAL_WHITE;
            ui_fill_ellipse(cx, cy, 16, 5, c);
            ui_fill_ellipse(cx - 6, cy - 3, 8, 5, c);
            ui_fill_ellipse(cx + 5, cy - 4, 9, 6, c);
        }
    }
    /* candy storm: falling sprinkles */
    if (th == THEME_CANDY) {
        static const uint8_t sc[5] = { PAL_PAINT(PAINT_ROSE, 1), PAL_PAINT(PAINT_SKY, 1), PAL_PAINT(PAINT_LEMON, 1),
                                       PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_LAVENDER, 1) };
        for (i = 0; i < 30; i++) {
            uint32_t h = doomo_hash32((uint32_t)i * 2654435761u);
            int x = (int)(h % (uint32_t)sw) + ui_wave(t + i * 9, 80, 4), y = (int)((h >> 8) + (uint32_t)t) % sh;
            plat_rect(x, y, (i & 1) ? 1 : 3, (i & 1) ? 3 : 1, sc[i % 5]);
        }
    }
    /* united park: fireworks */
    if (th == THEME_UNITED) {
        for (i = 0; i < 2; i++) {
            int ph = (t + i * 45) % 90, cx = 40 + ((t / 90 + i * 3) * 67) % (sw - 80), cy = 30 + i * 20, k;
            if (ph < 30) continue;
            for (k = 0; k < 8; k++) {
                int a = k * 512, r = (ph - 30) / 2;
                plat_rect(cx + FIX2INT(fx_cos(a) * r), cy + FIX2INT(fx_sin(a) * r), 2, 2,
                          (k & 1) ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(PAINT_LEMON, 1));
            }
        }
    }
}

static void draw_ground(void)
{
    int sw = fp_sw(), sh = fp_sh(), th = theme(), y0 = sh * 2 / 3, i, t = T.t;
    int hell = th == THEME_SUNSET || th == THEME_VAPE;
    uint8_t g0 = hell ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_GRASS, 6);
    uint8_t g1 = hell ? PAL_RAMP(RAMP_VAPE, 1) : PAL_RAMP(RAMP_GRASS, 5);
    uint8_t g2 = hell ? PAL_RAMP(RAMP_STONE, 0) : PAL_RAMP(RAMP_GRASS, 4);
    /* rolling hills: Hillvile houses on the left, Hellvile spires on the right */
    for (i = 0; i < sw; i += 4) {
        int h = 10 + ui_wave(i * 6 + 30, 400, 6) + ui_wave(i * 13, 300, 2);
        plat_rect(i, y0 - h, 4, h + 2, g1);
    }
    for (i = 0; i < 7; i++) {
        int x = 6 + i * 18, hh = 10 + (i * 7) % 8, y = y0 - 10 - (i % 3) * 2;
        uint8_t wall = hell ? PAL_RAMP(RAMP_VAPE, 3) : PAL_PAINT(i & 1 ? PAINT_PEACH : PAINT_BUBBLEGUM, 2);
        plat_rect(x, y - hh, 10, hh, wall);
        plat_rect(x - 1, y - hh - 4, 12, 4, hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_PAINT(i & 1 ? PAINT_ROSE : PAINT_SKY, 1));
        plat_rect(x + 3, y - hh + 3, 3, 3, hell || th == THEME_NIGHT ? PAL_RAMP(RAMP_CANDY, 6) : PAL_RAMP(RAMP_SKY, 6));
    }
    for (i = 0; i < 5; i++) {
        int x = sw - 14 - i * 20, hh = 16 + (i * 11) % 10, y = y0 - 8 - (i & 1) * 3;
        plat_rect(x, y - hh, 8, hh, PAL_RAMP(RAMP_VAPE, 1));
        plat_rect(x + 2, y - hh - 6, 4, 6, PAL_RAMP(RAMP_VAPE, 1));
        plat_rect(x + 3, y - hh - 9, 2, 3, PAL_RAMP(RAMP_VAPE, 1));
        if ((t / 20 + i) % 3) plat_rect(x + 3, y - hh + 4, 2, 2, PAL_RAMP(RAMP_LAVA, 6));
    }
    plat_rect(0, y0, sw, sh - y0, g0);
    plat_checker(0, y0, sw, 1, g1);
    plat_rect(0, y0 + (sh - y0) / 2, sw, sh - y0 - (sh - y0) / 2, g2);
    plat_checker(0, y0 + (sh - y0) / 2, sw, 1, g0);
    /* flowers / embers on the meadow */
    for (i = 0; i < 16; i++) {
        uint32_t h = doomo_hash32((uint32_t)i * 4099u + 3u);
        int x = (int)(h % (uint32_t)sw), y = y0 + 3 + (int)((h >> 10) % (uint32_t)DOOMO_MAX(sh - y0 - 6, 1));
        uint8_t c = hell ? PAL_RAMP(RAMP_LAVA, 5) : ((i & 1) ? PAL_PAINT(PAINT_BUBBLEGUM, 2) : PAL_WHITE);
        plat_rect(x - 1, y, 3, 1, c);
        plat_rect(x, y - 1, 1, 3, c);
        plat_rect(x, y, 1, 1, hell ? PAL_RAMP(RAMP_LAVA, 7) : PAL_UI_GOLD);
    }
}

/* ====================================================================== */
/* the logo                                                                 */
static const uint8_t logo_cols[10] = {
    PAINT_ROSE, PAINT_TANGERINE, PAINT_LEMON, PAINT_MINT, PAINT_SKY,
    PAINT_LAVENDER, PAINT_BUBBLEGUM, PAINT_ROSE, PAINT_LEMON, PAINT_MINT
};

static void draw_logo(void)
{
    static const char word[] = "DOOMOGACHI";
    int sw = fp_sw(), big = fp_big(), sc = big ? 4 : 3, adv = FONT_ADVANCE * sc;
    int lw = 10 * adv, x0 = (sw - lw) / 2, y0 = big ? 16 : 10, i, t = T.t;
    char g[2] = { 0, 0 };
    for (i = 0; i < 10; i++) {
        int drop = t - i * 3, dy;
        if (drop < 0) continue;
        if (drop < 12) dy = -(12 - drop) * (12 - drop) / 3;
        else if (drop < 18) dy = (drop - 12) < 3 ? (drop - 12) : 6 - (drop - 12);
        else dy = ui_wave(t * 2 + i * 22, 120, 2);
        g[0] = word[i];
        /* never draw big text partly off-screen: the UI kit caches the
         * clipped glyph and would replay it clipped later */
        if (y0 + dy < 2) dy = 2 - y0;
        ui_text_big(x0 + i * adv, y0 + dy, g, sc, PAL_PAINT(logo_cols[i], 2),
                    PAL_PAINT(logo_cols[i], 0));
    }
    /* "LIFE" in a pill under the right end */
    if (t > 34) {
        int lx = x0 + lw - (big ? 92 : 70), ly = y0 + 8 * sc + (big ? 2 : 1);
        int pop = ui_pop_scale(t - 34), pw = (big ? 88 : 66) * pop / 256;
        fp_pill(lx + ((big ? 88 : 66) - pw) / 2, ly, pw, big ? 22 : 18, PAL_PAINT(PAINT_ROSE, 1),
                PAL_PAINT(PAINT_ROSE, 0), PAL_PAINT(PAINT_ROSE, 2));
        if (pop >= 220) {
            ui_text_big(lx + (big ? 18 : 12), ly + (big ? 3 : 2), "LIFE", 2, PAL_WHITE, PAL_PAINT(PAINT_ROSE, 0));
            ui_heart(lx + (big ? 80 : 60), ly + 4, 3, PAL_UI_GOLD);
        }
    }
    /* "in HELLvile Park!" - the HELL glitches */
    if (t > 44) {
        int s2 = big ? 2 : 1, ly = y0 + 8 * sc + (big ? 28 : 22);
        int w_in = ui_text_big_width("in ", s2), w_hell = ui_text_big_width("HELL", s2);
        int w_rest = ui_text_big_width("vile Park!", s2);
        int tw = w_in + w_hell + w_rest - 4, lx = (sw - tw) / 2 - (big ? 0 : 20);
        int gl = T.glitch_t > 0, k;
        uint8_t hc = gl ? ((T.t / 2) & 1 ? PAL_UI_RED : PAL_RAMP(RAMP_BLOOD, 2)) : PAL_PAINT(PAINT_CHERRY, 1);
        if (s2 == 1) {
            ui_text_outline(lx, ly, "in", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0));
            ui_text_outline(lx + w_in + w_hell - 2, ly, "vile Park!", PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0));
        } else {
            ui_text_big(lx, ly, "in", s2, PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0));
            ui_text_big(lx + w_in + w_hell - 2, ly, "vile Park!", s2, PAL_WHITE, PAL_PAINT(PAINT_GRAPE, 0));
        }
        for (k = 0; k < 4; k++) {
            int jx = gl ? rng_between(&doomo_rng, -2, 2) : 0, jy = gl ? rng_between(&doomo_rng, -1, 1) : 0;
            char c[2] = { "HELL"[k], 0 };
            if (gl && k == 1 && (T.t & 4)) c[0] = '3';
            if (s2 == 1) ui_text_outline(lx + w_in + k * FONT_ADVANCE + jx, ly + jy, c, hc, PAL_RAMP(RAMP_BLOOD, 0));
            else ui_text_big(lx + w_in + k * FONT_ADVANCE * s2 + jx, ly + jy, c, s2, hc, PAL_RAMP(RAMP_BLOOD, 0));
        }
        if (gl && T.glitch_t > 10) plat_shade_rect(lx + w_in - 2, ly + 2 + rng_range(&doomo_rng, 6 * s2), w_hell + 4, 2, 4);
        /* drips */
        for (k = 0; k < 8; k++) {
            int dx, dy;
            if (!T.drip_v[k]) continue;
            dx = lx + w_in + (T.drip_x[k] * s2) % DOOMO_MAX(w_hell - 2, 1);
            dy = ly + 7 * s2 + T.drip_y[k];
            plat_rect(dx, ly + 7 * s2, 1, T.drip_y[k], PAL_RAMP(RAMP_BLOOD, 3));
            plat_rect(dx - (T.drip_y[k] > 3), dy, 2, 2, PAL_UI_RED);
            plat_rect(dx, dy, 1, 1, PAL_RAMP(RAMP_BLOOD, 6));
        }
    }
    /* sparkles around the logo */
    for (i = 0; i < 5; i++) {
        uint32_t h = doomo_hash32((uint32_t)(i + (t / 48) * 5));
        int sx = x0 + (int)(h % (uint32_t)lw), sy = y0 - 4 + (int)((h >> 12) % (uint32_t)(8 * sc + 8));
        ui_sparkle(sx, sy, (t / 2 + i * 3) & 15, PAL_WHITE);
    }
}

/* ====================================================================== */
static void draw_figures(void)
{
    int sw = fp_sw(), sh = fp_sh(), k, t = T.t, big = fp_big();
    if (!T.started && t < 50) return;
    for (k = 0; k < 2; k++) {
        int id = roster_find(parade[(T.parade_k + k) % NPARADE]);
        const DoomoResident *p;
        int x = k ? sw - FIGURE_W - (big ? 18 : 2) : (big ? 18 : 2);
        int y = sh - FIGURE_H - (big ? 14 : 4) + (ui_wave(t + k * 30, 50, 2) > 0);
        int hop = ((t + k * 40) % 120) < 10 ? -((t + k * 40) % 120) * (10 - (t + k * 40) % 120) / 6 : 0;
        int expr = ((t / 90 + k) % 3) == 0 ? EXPR_HAPPY : ((t / 90 + k) % 3) == 1 ? EXPR_LOVE : EXPR_NEUTRAL;
        if (id < 0) continue;
        p = roster_premade(id);
        if (!p) continue;
        ui_fill_ellipse(x + FIGURE_W / 2, sh - (big ? 15 : 5), 18, 3, PAL_RAMP(RAMP_STONE, 3));
        fp_figure(x, y + hop, &p->look, RES_ALIVE, expr);
    }
}

static void draw_menu(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int sw = fp_sw(), sh = fp_sh(), big = fp_big(), i, t = T.t;
    int bw = big ? 120 : 96, bh = big ? 17 : 15, n, y0;
    if (!T.started) {
        if ((t / 20) % 3 != 2 && t > 50) {
            const char *msg = "PRESS START";
            int w = ui_text_width(msg) + 20, x = (sw - w) / 2, y = sh - (big ? 62 : 40);
            fp_pill(x, y, w, 14, PAL_WHITE, st->outline, PAL_WHITE);
            ui_text(x + 10, y + 3, msg, st->accent);
            ui_heart(x + 4, y + 7, 2, st->accent);
            ui_heart(x + w - 5, y + 7, 2, st->accent);
        }
        return;
    }
    if (T.menu == M_MAIN) {
        n = I_MAIN_N;
        y0 = sh - n * bh - (big ? 22 : 6);
        for (i = 0; i < n; i++) {
            int pop = ui_pop_scale(T.menu_t - i * 2), w = bw * pop / 256;
            int x = (sw - w) / 2, y = y0 + i * bh;
            char label[40];
            if (pop <= 0) continue;
            if (i == I_THEME) {
                const DoomoOptionInfo *oi = &doomo_option_info[OPT_TITLE_THEME];
                doomo_snprintf(label, sizeof(label), STR_ICON_ARROW_L " %s " STR_ICON_ARROW_R,
                               oi->values ? oi->values[theme()] : "?");
            } else {
                doomo_snprintf(label, sizeof(label), "%s", main_items[i]);
            }
            if (pop < 250) {
                fp_pill(x, y, w, bh - 2, st->fill_hi, st->outline, PAL_WHITE);
                continue;
            }
            ui_button(x, y, bw, label, i == T.sel, i == I_THEME ? WIN_MINT : WIN_CUTE);
        }
        return;
    }
    {
        const char *items[3];
        const char *title = T.menu == M_STORY ? "Story Mode" : "Free Play";
        int has = T.menu == M_STORY ? T.has_story : T.has_free;
        items[0] = T.menu == M_STORY ? "New story" : "New island";
        items[1] = has ? "Continue" : "Continue (no save)";
        items[2] = "Back";
        y0 = sh - 3 * bh - (big ? 40 : 18);
        ui_title(sw / 2, y0 - 16, title, WIN_CUTE);
        for (i = 0; i < 3; i++) {
            int pop = ui_pop_scale(T.menu_t - i * 2), w = (bw + 20) * pop / 256;
            if (pop < 250) { fp_pill((sw - w) / 2, y0 + i * bh, w, bh - 2, st->fill_hi, st->outline, PAL_WHITE); continue; }
            ui_button((sw - bw - 20) / 2, y0 + i * bh, bw + 20, items[i], i == T.sub_sel,
                      (i == 1 && !has) ? WIN_CREAM : WIN_CUTE);
        }
        if (T.sub_sel == 1 && has) {
            DoomoSaveInfo si;
            if (save_peek(T.menu == M_STORY ? 0 : 1, &si)) {
                char buf[64];
                if (T.menu == M_STORY)
                    doomo_snprintf(buf, sizeof(buf), "%s - Chapter %d - Day %u", si.player, si.chapter, (unsigned)si.day + 1);
                else
                    doomo_snprintf(buf, sizeof(buf), "%d citizens - Day %u - H$ %u", si.residents,
                                   (unsigned)si.day + 1, (unsigned)si.money);
                ui_text_outline(sw / 2 - ui_text_width(buf) / 2, y0 + 3 * bh + 2, buf, PAL_WHITE, st->outline);
            }
        }
    }
    (void)t;
}

static void title_draw(void)
{
    int sw = fp_sw(), sh = fp_sh();
    char buf[48];
    draw_sky();
    draw_ground();
    draw_figures();
    draw_logo();
    draw_menu();
    doomo_snprintf(buf, sizeof(buf), "%s", plat_name());
    ui_text_outline(sw - ui_text_width(buf) - 4, sh - 10, buf, PAL_WHITE, PAL_RAMP(RAMP_STONE, 2));
    ui_text_outline(3, sh - 10, "(c) HaHaland", PAL_WHITE, PAL_RAMP(RAMP_STONE, 2));
    fp_fx_draw();
}

const DoomoScene SCENE_TITLE = { "TITLE", title_enter, NULL, title_update, title_draw, 0 };
