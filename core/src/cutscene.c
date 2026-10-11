/* cutscene.c - SCENE_CUTSCENE (arg = CUT_*): chapter title cards and the
 * story's animated vignettes (owner: story). Each cutscene is a few
 * captioned panels over a little animated picture drawn with the UI
 * primitives, textures, sprites and the citizens' own portraits. A
 * advances (after a moment), START skips. When it ends it pops itself and
 * SCENE_STORY continues the plot.
 */
#include "story_int.h"

typedef struct { int16_t who; uint16_t min_t; const char *text; } Panel;

/* ---- the scripts --------------------------------------------------------------- */
static const Panel P_ch1[] = { { -1, 60, NULL }, { 0, 0, NULL } };
static const Panel P_arrival[] = {
    { -1, 50, "A sleepy pink bus rolls over the candy-coloured hills..." },
    { -1, 50, "Welcome to HaHaland: the happiest place on Earth!" },
    { -1, 50, "Population: 500 lovely citizens. And now, @P!" },
    { 0, 0, NULL }
};
static const Panel P_home[] = {
    { -1, 40, "Home sweet home! Your very own cottage on Hillvile Lane." },
    { -1, 50, "The Doomogachi egg is wiggling... it's warm... it's HATCHING!" },
    { 0, 0, NULL }
};
static const Panel P_fog[] = {
    { -1, 50, "Purple fog pours over the Wall like spilled grape soda." },
    { -1, 50, "Two little lights blink at you from inside it." },
    { -1, 60, "\"Hee hee hee... Hillvile... come and PLAY with us...\"" },
    { 0, 0, NULL }
};
static const Panel P_eruption[] = {
    { -1, 60, "Somewhere beyond the Wall, an enormous vape cloud starts to swell..." },
    { -1, 50, "It grows. And grows. It blots out the pink sky..." },
    { -1, 50, "...and with a sound like the world's biggest cough, it tears the gate WIDE OPEN." },
    { -1, 70, "THE DEMONS OF HELL ARE HERE." },
    { 0, 0, NULL }
};
static const Panel P_vapula[] = {
    { -1, 50, "Lord Vapula, Duke of the Ninth Cloud, wheezes his last mighty wheeze..." },
    { -1, 50, "...and deflates. Pfffffffff. With a sad little squeak." },
    { -1, 50, "The vape cloud dissolves. The sky blushes pink again." },
    { 0, 0, NULL }
};
static const Panel P_wall[] = {
    { -1, 50, "The Wall creaks... groans... and comes tumbling down." },
    { -1, 50, "For the first time, Hillvile and Hellvile see each other." },
    { -1, 60, "And then, all at once, everybody runs to say hello." },
    { 0, 0, NULL }
};
static const Panel P_freeplay[] = {
    { -1, 60, "FREE PLAY UNLOCKED! Visit the united park, Hellvile and HaHaland any time." },
    { -1, 40, "Thank you for playing, @P. Hellvile Park is better with you in it." },
    { 0, 0, NULL }
};

static const Panel *const scripts[CUT_COUNT] = {
    P_ch1, P_ch1, P_ch1, P_ch1, P_arrival, P_fog, P_eruption, P_vapula, P_wall, P_freeplay, P_home
};

static const char *const cut_names[CUT_COUNT] = {
    "Chapter 1", "Chapter 2", "Chapter 3", "Chapter 4", "Arrival in HaHaland",
    "The Purple Fog", "The Gate Erupts", "Vapula Deflates", "The Wall Falls",
    "Free Play", "Home Sweet Home"
};

const char *story_cutscene_name(int c) { return (c >= 0 && c < CUT_COUNT) ? cut_names[c] : ""; }

static int cut, panel;
static uint32_t ct, pt;            /* ticks in cutscene / in panel */


static int panel_count(void)
{
    int n = 0;
    while (scripts[cut][n].min_t) n++;
    return n;
}

static void cs_end(void)
{
    if (cut >= 0 && cut < CUT_COUNT) unlock_grant(USTORY_CUT_FIRST + cut);
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
}

static void cs_enter(int arg)
{
    cut = (arg >= 0 && arg < CUT_COUNT) ? arg : CUT_CHAPTER1;
    panel = 0;
    ct = pt = 0;

    switch (cut) {
    case CUT_CHAPTER1: audio_music(SONG_TITLE); break;
    case CUT_CHAPTER2: audio_music(SONG_VAPE_LOUNGE); break;
    case CUT_CHAPTER3: audio_music(SONG_INVASION); break;
    case CUT_CHAPTER4: audio_music(SONG_HELLVILE); break;
    case CUT_ARRIVAL: audio_music(SONG_HAHALAND_DAY); break;
    case CUT_HOME: audio_music(SONG_PET); break;
    case CUT_FOG: audio_music(SONG_OMINOUS); break;
    case CUT_ERUPTION: audio_music(SONG_OMINOUS); break;
    case CUT_VAPULA_DOWN: audio_music(SONG_VICTORY); break;
    case CUT_WALL_FALLS: audio_music(SONG_FESTIVAL); break;
    case CUT_FREEPLAY: audio_music(SONG_CREDITS); break;
    default: break;
    }
}

static void cs_update(void)
{
    int n = panel_count();
    ct++;
    pt++;
    if (cut == CUT_ERUPTION && panel == 2 && pt == 20) { audio_sfx(SFX_EXPLODE); audio_music(SONG_INVASION); }
    if (cut == CUT_ERUPTION && panel == 3 && pt == 2) audio_sfx(SFX_DEMON_ROAR);
    if (cut == CUT_WALL_FALLS && panel == 0 && (pt % 16) == 8) audio_sfx(SFX_EXPLODE);
    if (cut == CUT_HOME && panel == 1 && (pt % 30) == 10) audio_sfx(SFX_EGG_CRACK);
    if (cut == CUT_FOG && panel == 2 && pt == 4) audio_sfx(SFX_POSSESS);
    if (cut == CUT_VAPULA_DOWN && panel == 1 && pt == 10) audio_sfx(SFX_DEMON_DIE);
    if (btn_pressed(BTN_START)) { audio_sfx(SFX_CANCEL); cs_end(); return; }
    if ((btn_pressed(BTN_A) || btn_pressed(BTN_B)) && pt >= 12) {
        pt = scripts[cut][panel].min_t + 200;     /* fall through below */
    }
    if (pt >= (uint32_t)scripts[cut][panel].min_t + 150 + 50) {
        panel++;
        pt = 0;
        audio_sfx(SFX_BLIP);
        if (panel >= n) { cs_end(); return; }
    }
}

/* ---- drawing helpers ------------------------------------------------------------- */
static void sky_bands(uint8_t top, uint8_t mid, uint8_t bottom, int y1)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), h3 = y1 / 3;
    plat_rect(0, 0, sw, h3, top);
    plat_checker(0, h3, sw, 2, mid);
    plat_rect(0, h3 + 1, sw, h3, mid);
    plat_checker(0, 2 * h3, sw, 2, bottom);
    plat_rect(0, 2 * h3 + 1, sw, y1 - 2 * h3, bottom);
    (void)sh;
}

static void tile_tex(int tex, int x0, int y0, int w, int h, int scale)
{
    const DoomoImage *im = texture_image(tex, (uint16_t)ct);
    int x, y, step = 64 * scale;
    if (!im) return;
    for (y = y0; y < y0 + h; y += step)
        for (x = x0; x < x0 + w; x += step) plat_image(x, y, im, scale, 0, 0);
}

static void cloud(int cx, int cy, int r, uint8_t c, int checker)
{
    if (checker) {
        plat_checker(cx - r, cy - r / 2, r * 2, r, c);
    } else {
        ui_fill_circle(cx, cy, r, c);
        ui_fill_circle(cx - r, cy + r / 3, r * 2 / 3, c);
        ui_fill_circle(cx + r, cy + r / 3, r * 2 / 3, c);
    }
}

static void caption(void)
{
    const Panel *p = &scripts[cut][panel];
    char buf[200];
    int sw = plat_screen_w(), sh = plat_screen_h(), lines, h, y, style;
    uint32_t chars;
    if (!p->text) return;
    style = (cut == CUT_ERUPTION && panel >= 2) || cut == CUT_FOG ? WIN_DARK : WIN_CREAM;
    story_fmt(buf, sizeof(buf), p->text);
    lines = ui_text_wrap(0, 0, sw - 32, buf, 0, 0);
    h = lines * FONT_LINE + 12;
    y = sh - h - 6;
    if (!ui_window_pop(8, y, sw - 16, h, style, (int)pt)) return;
    chars = pt * 2;
    ui_text_wrap_n(16, y + 6, sw - 32, buf, ui_style_text(style), (int)chars);
    if (chars > strlen(buf) && ((ct / 10) & 1))
        plat_text(sw - 22, y + h - 10, STR_ICON_ARROW_D, ui_style(style)->accent);
}

/* ---- chapter cards ------------------------------------------------------------------ */
static void draw_card(int ch)
{
    static const uint8_t styles[4] = { WIN_CUTE, WIN_DARK, WIN_HELL, WIN_MINT };
    int sw = plat_screen_w(), sh = plat_screen_h(), s, y, i;
    char num[16];
    const char *title = story_chapter_title(ch);
    int style = styles[(ch - 1) & 3];
    ui_backdrop(style, ct);
    if (ch == 2) {
        for (i = 0; i < 6; i++)
            cloud((int)((i * 53 + ct / 2) % (uint32_t)(sw + 60)) - 30, sh - 20 - (i % 3) * 18,
                  14 + (i % 3) * 4, PAL_RAMP(RAMP_VAPE, 4 + (i & 1)), 1);
    }
    if (ch == 3) {
        for (i = 0; i < 18; i++) {
            int x = (int)((i * 71 + ct * (2 + i % 3)) % (uint32_t)sw);
            int yy = sh - (int)((ct * (3 + i % 4) + (uint32_t)i * 29) % (uint32_t)sh);
            plat_rect(x, yy, 1, 2, PAL_RAMP(RAMP_LAVA, 4 + i % 4));
        }
        if ((ct % 60) < 3) plat_checker(0, 0, sw, sh, PAL_UI_RED);
    }
    if (ch == 4) {
        plat_checker(sw / 2, 0, sw / 2, sh, PAL_RAMP(RAMP_VAPE, 2));
        ui_heart(sw / 2, sh / 2 + 34, 10 + ui_wave((int)ct, 30, 2), PAL_UI_PINK);
    }
    doomo_snprintf(num, sizeof(num), "Chapter %d", ch);
    s = ui_pop_scale((int)ct);
    y = sh / 2 - 30;
    if (ch == 3 && ct < 40) y += ((ct * 7) % 5) - 2;
    if (s > 0) {
        int sc = sh >= 200 ? 3 : 2;
        ui_text_big((sw - ui_text_big_width(num, sc)) / 2, y, num, sc, PAL_WHITE,
                    ch == 3 ? PAL_UI_RED_D : ch == 2 ? PAL_RAMP(RAMP_VAPE, 1) : PAL_UI_PINK_D);
    }
    if (ct > 18) {
        int sc = (ui_text_big_width(title, 2) < sw - 16) ? 2 : 1;
        int ty = y + (sh >= 200 ? 34 : 26);
        if (sc == 2) ui_text_big((sw - ui_text_big_width(title, 2)) / 2, ty, title, 2, PAL_UI_CREAM, PAL_BLACK);
        else ui_title(sw / 2, ty, title, style);
    }
    for (i = 0; i < 6; i++)
        ui_sparkle((int)((i * 41 + 17) % (uint32_t)sw), (int)((i * 29 + 11) % (uint32_t)(sh / 3)) + 6,
                   (int)((ct / 2 + (uint32_t)i * 5) & 15), PAL_WHITE);
    if (ct > 60 && ((ct / 16) & 1)) ui_text_center(sw / 2, sh - 14, STR_ICON_A " Continue", PAL_WHITE);
}

/* ---- vignettes ------------------------------------------------------------------------- */
static void draw_hills(int sw, int sh, uint8_t far, uint8_t near)
{
    ui_fill_ellipse(sw / 4, sh * 3 / 4, sw / 2, sh / 4, far);
    ui_fill_ellipse(sw * 3 / 4, sh * 3 / 4 + 6, sw / 2, sh / 4, far);
    ui_fill_ellipse(sw / 2, sh + 10, sw * 3 / 4, sh / 3, near);
}

static void draw_player_figure(int x, int y, int expr)
{
    const DoomoResident *p = town_res(DOOMO_PLAYER_ID);
    const DoomoImage *f;
    if (!p || p->status == RES_EMPTY) return;
    f = figure_image(&p->look, p->status, expr);
    if (f) plat_image(x, y, f, 1, IMG_SHADOW, 0);
}

static void draw_arrival(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), bx, by, i;
    sky_bands(PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_CANDY, 7), sh * 2 / 3);
    ui_fill_circle(sw - 40, 30, 14, PAL_RAMP(RAMP_CANDY, 6));
    ui_fill_circle(sw - 43, 27, 4, PAL_WHITE);
    for (i = 0; i < 3; i++) cloud((int)((i * 97 + ct / 3) % (uint32_t)(sw + 80)) - 40, 22 + i * 14, 9, PAL_WHITE, 0);
    draw_hills(sw, sh, PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 5));
    plat_rect(0, sh * 3 / 4 + 4, sw, 10, PAL_RAMP(RAMP_PEACH, 5));
    plat_rect(0, sh * 3 / 4 + 4, sw, 1, PAL_RAMP(RAMP_PEACH, 3));
    for (i = 0; i < sw; i += 16) plat_rect(i + (int)((ct) % 16), sh * 3 / 4 + 8, 6, 1, PAL_WHITE);
    /* the bus */
    bx = (int)DOOMO_MIN((int)ct * 2 - 80, sw / 2 - 30);
    by = sh * 3 / 4 - 22 + (((ct / 4) & 1) && bx < sw / 2 - 30 ? 1 : 0);
    ui_round_rect(bx, by, 60, 26, 6, PAL_RAMP(RAMP_PASTEL_PINK, 2));
    ui_round_rect(bx + 1, by + 1, 58, 23, 5, PAL_RAMP(RAMP_PASTEL_PINK, 5));
    plat_rect(bx + 3, by + 3, 54, 1, PAL_RAMP(RAMP_PASTEL_PINK, 7));
    for (i = 0; i < 4; i++) ui_round_rect(bx + 6 + i * 13, by + 6, 10, 8, 2, PAL_RAMP(RAMP_SKY, 6));
    ui_fill_circle(bx + 14, by + 26, 5, PAL_RAMP(RAMP_STONE, 1));
    ui_fill_circle(bx + 46, by + 26, 5, PAL_RAMP(RAMP_STONE, 1));
    ui_fill_circle(bx + 14, by + 26, 2, PAL_GRAY_L);
    ui_fill_circle(bx + 46, by + 26, 2, PAL_GRAY_L);
    ui_heart(bx + 52, by + 18, 3, PAL_UI_RED);
    if (bx >= sw / 2 - 30 && panel >= 1) {
        int hop = ui_wave((int)pt, 20, 3);
        draw_player_figure(bx + 64, sh * 3 / 4 - FIGURE_H + 10 - (hop < 0 ? -hop : hop), EXPR_HAPPY);
        for (i = 0; i < 5; i++) ui_sparkle(bx + 64 + (i * 13) % 50, sh / 2 - 30 + (i * 7) % 30, (int)((pt / 2 + i * 3) & 15), PAL_WHITE);
    }
}

static void draw_home(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, cx = sw / 2, ey = sh / 2 + 6;
    const DoomoImage *egg;
    tile_tex(TEX_WALLPAPER_HEARTS, 0, 0, sw, sh * 2 / 3, 1);
    tile_tex(TEX_WOOD_FLOOR, 0, sh * 2 / 3, sw, sh / 3 + 64, 1);
    plat_rect(0, sh * 2 / 3 - 2, sw, 3, PAL_RAMP(RAMP_WOOD, 2));
    /* window */
    ui_round_rect(sw / 6, 16, 50, 40, 3, PAL_RAMP(RAMP_WOOD, 3));
    plat_rect(sw / 6 + 4, 20, 42, 32, PAL_RAMP(RAMP_SKY, 6));
    cloud(sw / 6 + 24, 34, 6, PAL_WHITE, 0);
    plat_rect(sw / 6 + 24, 20, 2, 32, PAL_RAMP(RAMP_WOOD, 3));
    /* bed */
    ui_round_rect(sw - 90, sh * 2 / 3 - 20, 80, 26, 5, PAL_RAMP(RAMP_PASTEL_PINK, 4));
    ui_round_rect(sw - 88, sh * 2 / 3 - 26, 24, 12, 5, PAL_WHITE);
    /* cushion + egg */
    ui_fill_ellipse(cx, ey + 26, 26, 8, PAL_RAMP(RAMP_PASTEL_PINK, 5));
    egg = sprite_image(SPR_EGG, panel >= 1 ? (int)((pt / 4) & 1) : 0);
    if (egg) {
        int wob = panel >= 1 ? ui_wave((int)pt, 8, 2) : 0;
        plat_image(cx - egg->w * 3 / 2 + wob, ey + 24 - egg->h * 3, egg, 3, 0, 0);
    }
    if (panel >= 1) {
        for (i = 0; i < 8; i++) {
            int a = (int)(i * 512 + pt * 20);
            ui_sparkle(cx + FIX2INT(fx_cos(a) * 34), ey + FIX2INT(fx_sin(a) * 22), (int)((pt / 2 + i * 2) & 15), PAL_UI_GOLD);
        }
    }
    draw_player_figure(sw / 6 - 10, sh * 2 / 3 - FIGURE_H + 14, EXPR_LOVE);
}

static void draw_wall_row(int y, int h, int sw)
{
    tile_tex(TEX_RUST_METAL, 0, y, sw, h, 1);
    plat_rect(0, y, sw, 1, PAL_RAMP(RAMP_STEEL, 1));
}

static void draw_fog(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i;
    sky_bands(PAL_RAMP(RAMP_VAPE, 0), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), sh);
    draw_wall_row(sh / 3, sh / 2, sw);
    for (i = 0; i < 10; i++) {
        int x = (int)((i * 61 + ct * (1 + i % 2)) % (uint32_t)(sw + 60)) - 30;
        cloud(x, sh / 3 - 6 + (i % 4) * 14 + ui_wave((int)ct + i * 9, 60, 3), 16 + (i % 3) * 5,
              PAL_RAMP(RAMP_VAPE, 4 + (i % 3)), 1);
    }
    if (panel >= 1) {
        int blink = ((ct / 50) % 4) == 3 && (ct % 50) < 5;
        int ex = sw / 2, ey = sh / 3 + 18;
        if (!blink) {
            ui_fill_circle(ex - 12, ey, 3, PAL_RAMP(RAMP_LAVA, 6));
            ui_fill_circle(ex + 12, ey, 3, PAL_RAMP(RAMP_LAVA, 6));
            plat_rect(ex - 13, ey - 1, 1, 1, PAL_WHITE);
            plat_rect(ex + 11, ey - 1, 1, 1, PAL_WHITE);
        }
    }
    if (panel >= 2) {
        static const char *const hee[3] = { "hee", "hee hee", "hee hee hee" };
        ui_text_outline(sw / 2 - 30 + ui_wave((int)ct, 20, 4), sh / 3 - 18, hee[(ct / 15) % 3],
                        PAL_RAMP(RAMP_VAPE, 7), PAL_RAMP(RAMP_VAPE, 0));
    }
}

static void draw_eruption(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, r, shake = 0;
    int phase = panel;
    if (phase >= 2) shake = ((ct * 7) % 7) - 3;
    if (phase == 0) sky_bands(PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6), sh);
    else if (phase == 1) sky_bands(PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_PASTEL_PINK, 4), sh);
    else sky_bands(PAL_RAMP(RAMP_BLOOD, 1), PAL_RAMP(RAMP_BLOOD, 3), PAL_RAMP(RAMP_LAVA, 4), sh);
    /* the swelling cloud */
    r = 12 + (int)(phase * 18) + (int)DOOMO_MIN(pt, 60u) / 3;
    for (i = 0; i < 5; i++)
        ui_fill_circle(sw / 2 + shake + (i - 2) * r / 2, sh / 3 + ui_wave((int)ct + i * 13, 30, 2) - (i & 1) * r / 3,
                       r - (i == 2 ? 0 : r / 4), PAL_RAMP(RAMP_VAPE, phase >= 2 ? 2 + (i & 1) : 4 + (i & 1)));
    if (phase >= 2) {
        for (i = 0; i < 4; i++) {
            int lx = (int)((i * 83 + ct * 5) % (uint32_t)sw);
            if (((ct + (uint32_t)i * 11) % 23) < 3) plat_line(lx, 0, lx + 10, sh / 3, PAL_WHITE);
        }
        if (pt < 6) plat_rect(0, 0, sw, sh, PAL_WHITE);
    }
    draw_wall_row(sh / 2 + shake, sh / 4, sw);
    plat_rect(0, sh * 3 / 4, sw, sh / 4, phase >= 2 ? PAL_RAMP(RAMP_FLESH, 2) : PAL_RAMP(RAMP_GRASS, 5));
    if (phase >= 3) {
        static const uint8_t dem[5] = { SPR_IMPLING, SPR_PUFF_DEMON, SPR_VAPE_WRAITH, SPR_GARGOYLE, SPR_HELLHOUND_PUP };
        for (i = 0; i < 5; i++) {
            const DoomoImage *d = sprite_image(dem[i], (int)((ct / 6 + (uint32_t)i) & 1));
            int x = sw / 2 - 90 + i * 40 + ui_wave((int)ct + i * 20, 40, 6);
            int y = sh / 3 - 10 + (int)((pt * (2 + (uint32_t)i % 3)) % 60u) - 30;
            if (d) plat_image(x - d->w / 2, y, d, 1, IMG_SILHOUETTE, PAL_BLACK);
        }
        ui_text_big((sw - ui_text_big_width("!!!", 3)) / 2 + shake, 18, "!!!", 3, PAL_UI_RED, PAL_BLACK);
    }
}

static void draw_vapula(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, scale, frame;
    const DoomoImage *v;
    if (panel < 2) sky_bands(PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 4), sh);
    else { sky_bands(PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_CANDY, 7), sh); }
    scale = panel == 0 ? 2 : 1;
    frame = panel == 0 ? DFRAME_PAIN : panel == 1 ? DFRAME_DEATH0 + (int)DOOMO_MIN(pt / 12, 3u) : DFRAME_DEATH3;
    v = sprite_image(SPR_LORD_VAPULA, frame);
    if (v && panel < 2) {
        int sx = panel == 0 ? ((int)(ct * 3) % 5) - 2 : 0;
        plat_image(sw / 2 - v->w * scale / 2 + sx, sh / 2 - v->h * scale / 2 - 10, v, scale, 0, 0);
    }
    if (panel == 1) {
        for (i = 0; i < 8; i++) {
            int a = i * 512 + (int)pt * 30, rr = (int)pt;
            cloud(sw / 2 + FIX2INT(fx_cos(a) * rr), sh / 2 - 10 + FIX2INT(fx_sin(a) * rr) / 2, 6, PAL_RAMP(RAMP_VAPE, 6), 1);
        }
    }
    if (panel == 2) {
        const DoomoImage *small = sprite_image(SPR_LORD_VAPULA, DFRAME_WALK0);
        if (small) plat_image(sw / 2 - small->w / 2, sh / 2 - small->h / 2, small, 1, 0, 0);
        for (i = 0; i < 24; i++) {
            int x = (int)((i * 37 + pt * (1 + i % 3)) % (uint32_t)sw);
            int y = (int)((pt * (2 + (uint32_t)i % 3) + (uint32_t)i * 13) % (uint32_t)sh);
            plat_rect(x, y, 2, 2, PAL_PAINT(i & 31, 2));
        }
    }
}

static void draw_wall_falls(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, j, gy = sh * 3 / 4;
    plat_rect(0, 0, sw / 2, gy, PAL_RAMP(RAMP_SKY, 6));
    plat_rect(sw / 2, 0, sw - sw / 2, gy, PAL_RAMP(RAMP_VAPE, 3));
    plat_rect(0, gy, sw / 2, sh - gy, PAL_RAMP(RAMP_GRASS, 5));
    plat_rect(sw / 2, gy, sw - sw / 2, sh - gy, PAL_RAMP(RAMP_GRASS, 3));
    /* bricks of the Wall falling */
    for (j = 0; j < 6; j++)
        for (i = 0; i < 3; i++) {
            int bx = sw / 2 - 18 + i * 12, by0 = gy - 12 * (j + 1);
            int delay = (5 - j) * 10 + i * 4, ft = (int)ct - delay;
            int by = by0, dx = 0;
            if (ft > 0) { by = by0 + ft * ft / 10; dx = (i - 1) * ft; }
            if (by > gy - 4) by = gy - 4 - (i + j) % 3 * 2;
            ui_round_rect(bx + dx, by, 11, 11, 2, PAL_RAMP(RAMP_STEEL, 2 + (i + j) % 3));
            plat_rect(bx + dx + 1, by + 1, 9, 1, PAL_RAMP(RAMP_STEEL, 6));
        }
    if (ct > 30 && ct < 90)
        for (i = 0; i < 6; i++) cloud(sw / 2 - 30 + i * 12, gy - 6, 6 + (int)(ct % 5), PAL_RAMP(RAMP_STONE, 6), 1);
    /* both sides run together */
    if (panel >= 1) {
        static const int16_t left[4] = { ID_MAYOR, ID_PIP, ID_NANA, ID_FUZZ };
        static const int16_t right[4] = { ID_MOTHER_NOX, ID_SOOTKIN, ID_ANVIL, ID_TALLYWORN };
        int run = panel >= 2 ? (int)DOOMO_MIN(pt, 80u) : 0;
        for (i = 0; i < 4; i++) {
            const DoomoImage *a = bodyspr_image(left[i], 2, run ? (int)((ct / 4 + (uint32_t)i) & 3) : 0);
            const DoomoImage *b = bodyspr_image(right[i], 1, run ? (int)((ct / 4 + (uint32_t)i) & 3) : 0);
            int y = gy - 24 + (i & 1) * 10;
            if (a) plat_image(18 + i * 14 + run, y, a, 1, 0, 0);
            if (b) plat_image(sw - 34 - i * 14 - run, y, b, 1, 0, 0);
        }
        if (panel >= 2 && pt > 50)
            for (i = 0; i < 6; i++) ui_heart(sw / 2 - 40 + i * 16, gy - 40 - (int)((pt * 2 + (uint32_t)i * 9) % 30u), 4, PAL_UI_PINK);
    }
}

static void draw_freeplay(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, s;
    ui_backdrop(WIN_CUTE, ct);
    plat_checker(sw / 2, 0, sw - sw / 2, sh, PAL_RAMP(RAMP_VAPE, 3));
    for (i = 0; i < 4; i++) {
        int fx = (int)((i * 67 + 30) % (uint32_t)sw), fy = 26 + (i * 19) % 40, t2 = (int)((ct + (uint32_t)i * 17) % 50u);
        int k;
        for (k = 0; k < 12; k++) {
            int a = k * 341;
            plat_rect(fx + FIX2INT(fx_cos(a) * t2), fy + FIX2INT(fx_sin(a) * t2), 2, 2, PAL_PAINT((i * 7 + k) & 31, 2));
        }
    }
    s = sh >= 200 ? 3 : 2;
    ui_text_big((sw - ui_text_big_width("THE END", s)) / 2, sh / 2 - 34, "THE END", s, PAL_WHITE, PAL_UI_PINK_D);
    if (ct > 30) ui_title(sw / 2, sh / 2 - 6, "...of the beginning!", WIN_CUTE);
}

static void cs_draw(void)
{
    if (plat_draw_skipped()) return;
    switch (cut) {
    case CUT_CHAPTER1: case CUT_CHAPTER2: case CUT_CHAPTER3: case CUT_CHAPTER4:
        draw_card(cut - CUT_CHAPTER1 + 1);
        return;                                  /* cards have no caption */
    case CUT_ARRIVAL: draw_arrival(); break;
    case CUT_HOME: draw_home(); break;
    case CUT_FOG: draw_fog(); break;
    case CUT_ERUPTION: draw_eruption(); break;
    case CUT_VAPULA_DOWN: draw_vapula(); break;
    case CUT_WALL_FALLS: draw_wall_falls(); break;
    case CUT_FREEPLAY: draw_freeplay(); break;
    default: plat_clear(PAL_BLACK); break;
    }
    caption();
}

const DoomoScene SCENE_CUTSCENE = {
    "CUTSCENE", cs_enter, NULL, cs_update, cs_draw, 0
};
