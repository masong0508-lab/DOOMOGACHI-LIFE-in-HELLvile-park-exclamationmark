/* levels_draw.c - 2D pieces shared by the level select, the level play
 * scene and the arena modes (owner: levels): rank stamps that slam in,
 * title banners on a ribbon, bobbing demon portraits, the mini-boss intro
 * card and the variant boss bar. Everything is ui_* / plat_* primitives
 * (no new art), adapts to 240x160 and 320x240, and returns early on the
 * GBA's skipped catch-up frames.
 */
#include "levels_int.h"
#include "grumbleguts_gfx.h"

int lvd_big(void) { return plat_screen_h() >= 200; }

void lvd_time(char *buf, int cap, int seconds)
{
    if (seconds < 0) seconds = 0;
    doomo_snprintf(buf, cap, "%d:%02d", seconds / 60, seconds % 60);
}

void lvd_rank_stamp(int cx, int cy, int rank, int t, int big)
{
    static const char *const letters[5] = { "-", "C", "B", "A", "S" };
    int pop = ui_pop_scale(t), r = (big ? 21 : 11) * pop / 256, sc = big ? 4 : 2;
    uint8_t c = lv_rank_color(rank);
    const char *s;
    if (plat_draw_skipped() || r < 2) return;
    if (rank < 0 || rank > RANK_S) rank = RANK_NONE;
    s = letters[rank];
    ui_fill_circle(cx + 1, cy + 2, r + 1, PAL_BLACK);                 /* drop shadow  */
    ui_fill_circle(cx, cy, r + 1, ui_darker(ui_darker(c)));           /* outline      */
    ui_fill_circle(cx, cy, r, c);
    ui_fill_circle(cx - r / 4, cy - r / 4, r * 3 / 5, ui_lighter(c)); /* soft highlight */
    ui_fill_circle(cx, cy, r * 3 / 4, c);
    if (pop >= 200) {
        if (big) ui_text_big(cx - ui_text_big_width(s, sc) / 2, cy - sc * 4 + 1, s, sc, PAL_WHITE, ui_darker(ui_darker(c)));
        else ui_text_outline(cx - ui_text_width(s) / 2, cy - 3, s, PAL_WHITE, ui_darker(ui_darker(c)));
    }
    if (rank == RANK_S) {
        int k;
        for (k = 0; k < (big ? 5 : 3); k++) {
            int a = (int)((t * 24 + k * 4096 / (big ? 5 : 3)) & 4095);
            ui_sparkle(cx + FIX2INT(fx_cos(a) * (r + 5)), cy + FIX2INT(fx_sin(a) * (r + 5)),
                       (t / 2 + k * 5) & 15, PAL_WHITE);
        }
    }
}

void lvd_banner(int cy, const char *kicker, const char *title, int t, int hell)
{
    int sw = plat_screen_w(), sc = lvd_big() ? 3 : 2;
    int tw = ui_text_big_width(title, sc), slide = t < 10 ? (10 - t) * (10 - t) * 4 : 0;
    int bh = sc * 8 + 14, by = cy - bh / 2;
    uint8_t band = hell ? PAL_UI_RED_D : PAL_UI_PINK_D, band_hi = hell ? PAL_UI_RED : PAL_UI_PINK;
    if (plat_draw_skipped()) return;
    if (tw > sw - 32) { sc = 2; tw = ui_text_big_width(title, sc); }     /* room for the sparkles */
    plat_shade_rect(0, by - 2, sw, bh + 4, 4);
    plat_rect(-slide, by, sw, bh, band);
    plat_rect(-slide, by + 2, sw, 2, band_hi);
    plat_rect(-slide, by + bh - 4, sw, 2, band_hi);
    plat_checker(-slide, by + 5, sw, bh - 10, ui_darker(band));
    if (kicker && kicker[0]) {
        int kw = ui_text_width(kicker) + 12, kx = sw / 2 - kw / 2 + slide;
        ui_round_rect(kx, by - 7, kw, 11, 5, hell ? PAL_UI_GOLD : PAL_WHITE);
        ui_text(kx + 6, by - 5, kicker, hell ? PAL_UI_RED_D : PAL_UI_PINK_D);
    }
    ui_text_big(sw / 2 - tw / 2 + slide, cy - sc * 4 + 2 + ui_wave(t, 36, 1), title, sc,
                hell ? PAL_RAMP(RAMP_LAVA, 7) : PAL_WHITE, hell ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_UI_PINK_D);
    if (t > 8) {
        ui_sparkle(sw / 2 - tw / 2 - 8, cy - 6, (t / 2) & 15, PAL_UI_GOLD);
        ui_sparkle(sw / 2 + tw / 2 + 8, cy + 6, (t / 2 + 8) & 15, PAL_UI_GOLD);
    }
}

void lvd_demon(int x, int y, int spr, int t, int scale, int flip)
{
    const DoomoImage *img;
    int frame = ((t / 10) & 1) ? DFRAME_WALK1 : DFRAME_WALK0, bob = ui_wave(t, 30, 1);
    if (plat_draw_skipped()) return;
    img = sprite_image(spr, frame);
    if (!img) return;
    if (scale < 1) scale = 1;
    ui_fill_ellipse(x, y + 1, img->w * scale / 3, 2 + scale, PAL_BLACK);   /* contact shadow */
    plat_image(x - img->w * scale / 2, y - img->h * scale + bob, img, scale, IMG_SHADOW | (flip ? IMG_FLIPX : 0), 0);
}

void lvd_padlock(int cx, int cy, int k, int t)
{
    /* a chunky toy padlock: rounded gold body, steel shackle, heart keyhole;
     * it wobbles now and then ("nope!") */
    int wob = ((t / 4) % 24) < 3 ? (((t / 4) & 1) ? k : -k) : 0, x = cx + wob, i;
    uint8_t steel = PAL_GRAY_L, steel_d = PAL_GRAY_D, body = PAL_UI_GOLD, body_d = ui_darker(PAL_UI_GOLD);
    if (plat_draw_skipped()) return;
    if (k < 1) k = 1;
    ui_fill_ellipse(cx, cy + 10 * k, 8 * k, 2 * k, PAL_BLACK);            /* contact shadow */
    for (i = 0; i < 2; i++) {                                              /* outline pass, then fill */
        int o = i ? 0 : 1;
        uint8_t c = i ? steel : steel_d;
        plat_rect(x - 6 * k - o, cy - 9 * k - o, 2 * k + 2 * o, 7 * k + o, c);
        plat_rect(x + 4 * k - o, cy - 9 * k - o, 2 * k + 2 * o, 7 * k + o, c);
        ui_round_rect(x - 6 * k - o, cy - 12 * k - o, 12 * k + 2 * o, 4 * k + 2 * o, 2 * k, c);
    }
    plat_rect(x - 5 * k, cy - 11 * k, 1 * k, 8 * k, PAL_WHITE);               /* shine on the shackle */
    ui_round_rect(x - 8 * k - 1, cy - 3 * k - 1, 16 * k + 2, 12 * k + 2, 3 * k, body_d);
    ui_round_rect(x - 8 * k, cy - 3 * k, 16 * k, 12 * k, 3 * k, body);
    plat_rect(x - 7 * k, cy - 2 * k, 14 * k, k, ui_lighter(body));           /* top highlight */
    plat_rect(x - 7 * k, cy + 7 * k, 14 * k, k, body_d);                     /* bottom shade */
    ui_heart_flat(x, cy + 2 * k, DOOMO_MAX(2, 1 + k), PAL_UI_RED_D);          /* heart keyhole */
    plat_rect(x - (k + 1) / 2, cy + 3 * k, k, 3 * k, PAL_UI_RED_D);
}

/* the mini-boss waiting at the end: Gatekeeper Grumbleguts is drawn as
 * himself (the concept-sheet figure, grumbleguts_gfx.c), the others as
 * their sprite */
void lvd_boss_figure(int boss, int x, int y, int t, int scale, int flip)
{
    GgParams p;
    if (boss < 0 || boss >= LB_COUNT) return;
#if defined(DOOMO_PLATFORM_PS1)
    /* the PS1 keeps the renderer in the cutscene overlay (a reference from
     * the combat group would make its ~20 KB resident, docs/PS1.md 3) */
    if (1) { lvd_demon(x, y, lv_boss[boss].spr, t, scale, flip); return; }
#endif
    if (boss != LB_GRUMBLEGUTS) { lvd_demon(x, y, lv_boss[boss].spr, t, scale, flip); return; }
    gg_params_init(&p);
    p.dir = flip ? -1 : 1;
    p.bob = ui_wave(t * 3, 24, 1);
    p.flap = ui_wave(t * 4, 14, 3 * scale);
    p.tail = ui_wave(t * 5, 16, 2 * scale);
    p.belly_y = ui_wave(t * 3, 24, scale);
    p.blink = (t % 97) < 4;
    p.seams = lvd_big();
    gg_set_quality(lvd_big());
    gg_ellipse(x, y + 1, 22 * scale, 2 + scale, PAL_BLACK);                   /* contact shadow */
    gg_draw(x, y, scale, GGV_FRONT, GGP_STAND, ((t / 40) & 3) == 1 ? GGE_WINK : GGE_SMUG, &p);
}

void lvd_boss_intro(int boss, int t)
{
    const LevelBoss *b;
    int sw = plat_screen_w(), sh = plat_screen_h(), bar, cy, sc;
    if (plat_draw_skipped() || boss < 0 || boss >= LB_COUNT) return;
    b = &lv_boss[boss];
    /* letterbox bars */
    bar = DOOMO_MIN(t, 8) * (lvd_big() ? 3 : 2);
    if (t > 70) bar = (80 - t) * (lvd_big() ? 3 : 2) * 8 / 10;
    plat_rect(0, 0, sw, bar, PAL_BLACK);
    plat_rect(0, sh - bar, sw, bar, PAL_BLACK);
    if (t > 72 && (t & 1)) return;
    cy = sh / 3;
    sc = lvd_big() ? 3 : 2;
    if (ui_text_big_width(b->name, sc) > sw - 24) sc = 2;
    if (ui_text_big_width(b->name, sc) > sw - 16) sc = 1;
    lvd_banner(cy, "MINI-BOSS!", "", t, 1);
    if (sc > 1) ui_text_big(sw / 2 - ui_text_big_width(b->name, sc) / 2, cy - sc * 4 + 2, b->name, sc,
                            PAL_RAMP(RAMP_LAVA, 7), PAL_RAMP(RAMP_BLOOD, 0));
    else ui_text_outline(sw / 2 - ui_text_width(b->name) / 2, cy - 3, b->name, PAL_RAMP(RAMP_LAVA, 7), PAL_BLACK);
    if (t > 12) ui_text_outline(sw / 2 - ui_text_width(b->title) / 2, cy + sc * 4 + 8, b->title, PAL_UI_GOLD, PAL_BLACK);
    if (t > 22) {
        int w = ui_text_width(b->taunt) + 14, x = sw / 2 - w / 2, y = cy + sc * 4 + 22;
        ui_round_rect(x + 1, y + 2, w, 13, 6, PAL_BLACK);
        ui_round_rect(x, y, w, 13, 6, PAL_WHITE);
        ui_text(x + 7, y + 3, b->taunt, PAL_UI_RED_D);
    }
#if !defined(DOOMO_PLATFORM_PS1)
    if (boss == LB_GRUMBLEGUTS && t > 6) {
        /* and the man himself leans into the card from the right: head and
         * shoulders, a slow wink (cheap: the head alone, three tones) */
        int s = lvd_big() ? 2 : 1, slide = t < 16 ? (16 - t) * (16 - t) * s / 2 : 0;
        GgParams p;
        gg_params_init(&p);
        p.layers = GGL_HEAD | GGL_FACE;
        p.bob = ui_wave(t * 3, 30, s);
        p.blink = (t % 50) < 3;
        gg_set_quality(lvd_big());
        gg_draw(sw - 26 * s + slide, sh - bar + 44 * s + 2 * s, s, GGV_FRONT, GGP_STAND,
                t < 40 ? GGE_SULTRY : t < 56 ? GGE_WINK : GGE_GRIN, &p);
    }
#endif
}

int lvd_bar_y(void)
{
    /* below the map title + objective while the stock HUD shows them */
    return combat_stats()->ticks < 150 ? 32 : 13;
}

void lvd_boss_bar(int boss, int hp, int hp_max, int t)
{
    const LevelBoss *b;
    int sw = plat_screen_w(), w = sw - (lvd_big() ? 96 : 64), x = (sw - w) / 2, y = lvd_bar_y(), fill;
    if (plat_draw_skipped() || boss < 0 || boss >= LB_COUNT) return;
    b = &lv_boss[boss];
    if (hp < 0) hp = 0;
    if (hp_max < 1) hp_max = 1;
    fill = (w - 4) * hp / hp_max;
    ui_text_outline(sw / 2 - ui_text_width(b->name) / 2, y - 10, b->name, PAL_RAMP(RAMP_BONE, 7), PAL_RAMP(RAMP_BLOOD, 0));
    ui_round_rect(x - 1, y - 1, w + 2, 9, 3, PAL_BLACK);
    ui_round_rect(x, y, w, 7, 3, PAL_RAMP(RAMP_BLOOD, 1));
    if (fill > 0) {
        plat_rect(x + 2, y + 1, fill, 5, b->bar);
        plat_rect(x + 2, y + 1, fill, 1, ui_lighter(b->bar));
        if (hp * 4 < hp_max && (t & 4)) plat_rect(x + 2, y + 1, fill, 5, PAL_WHITE);     /* nearly done! */
    }
    ui_text_outline(x - 9, y, STR_ICON_SKULL, PAL_RAMP(RAMP_BONE, 7), PAL_BLACK);
    ui_text_outline(x + w + 3, y, STR_ICON_SKULL, PAL_RAMP(RAMP_BONE, 7), PAL_BLACK);
}
