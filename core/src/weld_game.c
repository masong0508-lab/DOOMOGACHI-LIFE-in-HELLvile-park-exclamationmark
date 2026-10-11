/* weld_game.c - SCENE_WELD_GAME: the welding minigame (owner: action).
 *
 * Two steel plates, a wobbly seam between them, and Rivet's torch moving
 * along it by itself. Steer the bead onto the seam with UP/DOWN and keep
 * the heat in the green zone by feathering A (hot) - too cold leaves gaps,
 * too hot burns through and spits sparks. Two passes; the accuracy and
 * heat control of every step become the weapon's quality (0..100), with a
 * streak bonus and a grade from D to S. The bead trail glows white-hot at
 * the torch and cools through orange and red to a rippled grey weld.
 * arg 1 = practice (nothing is welded).
 */
#include "weld_int.h"

#define TRAIL_MAX 320
#define SPARKS 24

enum { WG_INTRO, WG_PASS, WG_BETWEEN, WG_RESULT };

static int wg_state, wg_t, wg_pass, wg_practice;
static int torch_x, torch_y16, torch_vy16;
static int heat;                       /* 0..1000 */
static int zone_lo, zone_hi;
static int seam_x0, seam_x1, seam_cy;
static int amp1, amp2, fr1, fr2, ph1, ph2;
static int32_t score_sum, score_n;
static int streak, best_streak, quality;
typedef struct Spark { int16_t x, y, vx, vy; uint8_t life, kind; } Spark;
/* (integration, memory budget) the bead trail and the sparks (2.1 KB) live
 * in the shared scene scratch (scene.h, tag WGAM) while the minigame runs:
 * it only opens modals over itself, and the workshop below it just
 * composes its weapon sprites again afterwards */
#define WG_SCRATCH_TAG SAVE_EXT_TAG('W', 'G', 'A', 'M')
typedef struct WgScratch {
    int8_t  dy[2][TRAIL_MAX];
    uint8_t heat[2][TRAIL_MAX];
    uint8_t t[2][TRAIL_MAX];
    Spark   sparks[SPARKS];
} WgScratch;
static WgScratch *wgs;
#define trail_dy (wgs->dy)
#define trail_heat (wgs->heat)
#define trail_t (wgs->t)
#define sparks (wgs->sparks)

static int seam_y(int x, int pass)
{
    int a = (x * fr1 + ph1 + pass * 700) & DOOMO_ANG_MASK;
    int b = (x * fr2 + ph2 + pass * 1300) & DOOMO_ANG_MASK;
    return seam_cy + FIX2INT(fx_sin(a) * amp1) + FIX2INT(fx_sin(b) * amp2);
}

static void spark(int x, int y, int hot)
{
    int i;
    for (i = 0; i < SPARKS; i++)
        if (!sparks[i].life) {
            sparks[i].x = (int16_t)(x * 16);
            sparks[i].y = (int16_t)(y * 16);
            sparks[i].vx = (int16_t)rng_between(&doomo_rng, -40, 40 + (hot ? 20 : 0));
            sparks[i].vy = (int16_t)rng_between(&doomo_rng, -70, -10);
            sparks[i].life = (uint8_t)(10 + rng_range(&doomo_rng, 14));
            sparks[i].kind = (uint8_t)(weld_pending.style == WSTYLE_CUTE ? 1 + (i & 1) : 0);
            return;
        }
}

static void wg_enter(int arg)
{
    DoomoWeaponStats st;
    uint32_t h = doomo_hash32((uint32_t)weld_pending.frame * 977u + weld_pending.core * 31u +
                              weld_pending.barrel * 7u + doomo_frame_count());
    int sw = plat_screen_w(), sh = plat_screen_h(), hard;
    wgs = (WgScratch *)scene_scratch(WG_SCRATCH_TAG, (uint32_t)sizeof(WgScratch));
    wg_practice = arg == 1;
    wg_state = WG_INTRO;
    wg_t = 0;
    wg_pass = 0;
    score_sum = score_n = 0;
    streak = best_streak = 0;
    quality = 0;
    heat = 400;
    memset(trail_dy, 0, sizeof(trail_dy));
    memset(trail_heat, 0, sizeof(trail_heat));
    memset(trail_t, 0, sizeof(trail_t));
    memset(sparks, 0, sizeof(sparks));
    weld_stats(&weld_pending, &st);
    hard = st.wclass == WCLASS_HEAVY ? 2 : st.wclass == WCLASS_PRECISION ? 1 :
           st.wclass == WCLASS_CUTE || st.wclass == WCLASS_SIDEARM ? -1 : 0;
    seam_x0 = 24;
    seam_x1 = sw - 44;
    if (seam_x1 - seam_x0 > TRAIL_MAX - 1) seam_x1 = seam_x0 + TRAIL_MAX - 1;
    seam_cy = sh / 2 + 2;
    amp1 = 7 + hard * 2 + (sh > 200 ? 6 : 0);
    amp2 = 3 + (hard > 0 ? hard : 0);
    fr1 = 18 + (int)(h % 10) + hard * 3;
    fr2 = 50 + (int)((h >> 8) % 30);
    ph1 = (int)((h >> 12) & DOOMO_ANG_MASK);
    ph2 = (int)((h >> 20) & DOOMO_ANG_MASK);
    zone_lo = 420 + hard * 20;
    zone_hi = 720 - hard * 30;
    torch_x = seam_x0;
    torch_y16 = seam_y(seam_x0, 0) * 16;
    torch_vy16 = 0;
    weld_game_result = -1;
    audio_music(SONG_WORKSHOP);
}

static int step_score(int dy, int ht)
{
    int on, hq;
    if (dy < 0) dy = -dy;
    on = dy <= 2 ? 100 : dy <= 4 ? 70 : dy <= 7 ? 30 : 0;
    if (ht >= zone_lo && ht <= zone_hi) hq = 100;
    else if (ht >= zone_lo - 120 && ht <= zone_hi + 120) hq = 55;
    else hq = 15;
    return on * hq / 100;
}

static void wg_update(void)
{
    int i;
    if (!scene_scratch_mine(WG_SCRATCH_TAG)) {
        /* cannot happen (only modals open over it): nothing welded */
        plat_log("weld game: scene scratch lost, cancelled");
        weld_game_result = -1;
        scene_pop();
        return;
    }
    wg_t++;
    for (i = 0; i < SPARKS; i++)
        if (sparks[i].life) {
            sparks[i].life--;
            sparks[i].x = (int16_t)(sparks[i].x + sparks[i].vx);
            sparks[i].y = (int16_t)(sparks[i].y + sparks[i].vy);
            sparks[i].vy = (int16_t)(sparks[i].vy + 6);
        }
    switch (wg_state) {
    case WG_INTRO:
        if (wg_t > 20 && (btn_pressed(BTN_A) || wg_t > 150)) { wg_state = WG_PASS; wg_t = 0; }
        if (btn_pressed(BTN_B)) { weld_game_result = -1; scene_pop(); }
        return;
    case WG_BETWEEN:
        if (wg_t > 45) {
            wg_state = WG_PASS;
            wg_t = 0;
            torch_x = seam_x0;
            torch_y16 = (seam_y(seam_x0, wg_pass) + 6) * 16;
            torch_vy16 = 0;
        }
        return;
    case WG_RESULT:
        if (wg_t > 40 && btn_pressed(BTN_A)) {
            audio_sfx(SFX_SELECT);
            weld_game_result = wg_practice ? -1 : quality;
            scene_pop();
        }
        return;
    default:
        break;
    }
    if (btn_pressed(BTN_B)) { weld_game_result = -1; audio_sfx(SFX_CANCEL); scene_pop(); return; }
    /* steering with a little inertia and a nervous hand */
    if (btn_down(BTN_UP)) torch_vy16 -= 9;
    if (btn_down(BTN_DOWN)) torch_vy16 += 9;
    torch_vy16 = torch_vy16 * 3 / 4 + (int)(fx_sin(wg_t * 97) >> 15);
    torch_y16 += torch_vy16;
    torch_y16 = DOOMO_CLAMP(torch_y16, (seam_cy - 30) * 16, (seam_cy + 30) * 16);
    /* heat */
    if (btn_down(BTN_A)) heat += 22; else heat -= 15;
    heat = DOOMO_CLAMP(heat, 0, 1000);
    /* advance and score */
    if ((wg_t & 1) == 0 || torch_x < seam_x0 + 4) {
        int idx = torch_x - seam_x0, dy = torch_y16 / 16 - seam_y(torch_x, wg_pass);
        int sc = step_score(dy, heat);
        if (idx >= 0 && idx < TRAIL_MAX) {
            trail_dy[wg_pass][idx] = (int8_t)DOOMO_CLAMP(torch_y16 / 16 - seam_cy, -100, 100);
            trail_heat[wg_pass][idx] = (uint8_t)(heat / 4);
            trail_t[wg_pass][idx] = 1;
        }
        score_sum += sc;
        score_n++;
        if (sc >= 70) { if (++streak > best_streak) best_streak = streak; }
        else streak = 0;
        torch_x++;
        if (heat > zone_hi + 100 || rng_chance(&doomo_rng, 40)) spark(torch_x, torch_y16 / 16, heat > zone_hi);
        if ((wg_t & 7) == 0) audio_sfx(SFX_WELD);
    }
    if (torch_x >= seam_x1) {
        wg_pass++;
        wg_t = 0;
        if (wg_pass >= 2) {
            int q = score_n ? (int)(score_sum / score_n) : 0;
            q += DOOMO_MIN(best_streak / 12, 8);
            quality = DOOMO_CLAMP(q, 0, 100);
            wg_state = WG_RESULT;
            audio_sfx(quality >= 80 ? SFX_EVOLVE : SFX_SELECT);
        } else {
            wg_state = WG_BETWEEN;
        }
    }
}

/* ---- drawing ---------------------------------------------------------------------- */
static uint8_t bead_color(int age, int ht)
{
    /* white hot at the torch, then orange, red, and cooled grey ripples */
    if (age < 3) return PAL_WHITE;
    if (age < 8) return PAL_RAMP(RAMP_LAVA, 7);
    if (age < 16) return PAL_RAMP(RAMP_LAVA, 5);
    if (age < 30) return PAL_RAMP(RAMP_LAVA, 3);
    if (ht > 220) return PAL_RAMP(RAMP_STONE, 1);                /* scorched */
    if (ht < 90) return PAL_RAMP(RAMP_STEEL, 3);                 /* cold, lumpy */
    return PAL_RAMP(RAMP_STEEL, 5);
}

static void plates(int sw, int sh)
{
    int y0 = seam_cy - 34, y1 = seam_cy + 34, x, k;
    /* the table */
    plat_rect(0, y1 + 6, sw, sh - y1 - 6, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(0, y1 + 6, sw, 2, PAL_RAMP(RAMP_WOOD, 4));
    for (k = 0; k < sw; k += 24) plat_rect(k, y1 + 10, 1, sh - y1 - 10, PAL_RAMP(RAMP_WOOD, 1));
    /* two steel plates, bevelled, with rivets */
    ui_round_rect(seam_x0 - 12, y0, seam_x1 - seam_x0 + 24, y1 - y0, 3, PAL_RAMP(RAMP_STEEL, 1));
    ui_round_rect(seam_x0 - 11, y0 + 1, seam_x1 - seam_x0 + 22, y1 - y0 - 2, 3, PAL_RAMP(RAMP_STEEL, 4));
    plat_rect(seam_x0 - 9, y0 + 2, seam_x1 - seam_x0 + 18, 1, PAL_RAMP(RAMP_STEEL, 6));
    for (k = 0; k < 4; k++) {
        int rx = seam_x0 - 6 + k * (seam_x1 - seam_x0 + 12) / 3;
        plat_rect(rx, y0 + 5, 2, 2, PAL_RAMP(RAMP_STEEL, 6));
        plat_rect(rx + 1, y0 + 6, 1, 1, PAL_RAMP(RAMP_STEEL, 2));
        plat_rect(rx, y1 - 7, 2, 2, PAL_RAMP(RAMP_STEEL, 6));
        plat_rect(rx + 1, y1 - 6, 1, 1, PAL_RAMP(RAMP_STEEL, 2));
    }
    /* the seam: a dark groove */
    for (x = seam_x0; x < seam_x1; x++) {
        int y = seam_y(x, wg_pass > 1 ? 1 : wg_pass);
        plat_rect(x, y - 1, 1, 1, PAL_RAMP(RAMP_STEEL, 2));
        plat_rect(x, y, 1, 1, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(x, y + 1, 1, 1, PAL_RAMP(RAMP_STEEL, 3));
    }
}

static void bead(void)
{
    int p, i, pass = wg_pass > 1 ? 1 : wg_pass;
    for (p = 0; p <= pass; p++) {
        int head = p == wg_pass ? torch_x - seam_x0 : TRAIL_MAX;
        for (i = 0; i < TRAIL_MAX && seam_x0 + i < seam_x1; i++) {
            int age, y;
            uint8_t c;
            if (!trail_t[p][i]) continue;
            age = head - i;
            y = seam_cy + trail_dy[p][i];
            c = bead_color(age, trail_heat[p][i]);
            plat_rect(seam_x0 + i, y - 1, 1, 3, c);
            if ((i % 3) == 0 && age >= 30) plat_rect(seam_x0 + i, y - 1, 1, 1, PAL_RAMP(RAMP_STEEL, 6));
            if (age < 8) plat_rect(seam_x0 + i, y - 2, 1, 5, ui_lighter(c));
        }
    }
}

static void torch(void)
{
    int x = torch_x, y = torch_y16 / 16, k;
    int glow = 3 + (wg_t & 1) + heat / 300;
    /* light halo */
    ui_fill_circle(x, y, glow + 4, PAL_RAMP(RAMP_LAVA, 4));
    ui_fill_circle(x, y, glow + 1, PAL_RAMP(RAMP_LAVA, 7));
    ui_fill_circle(x, y, glow - 1, PAL_WHITE);
    /* the torch nozzle coming in from the upper right */
    for (k = 0; k < 18; k++) {
        plat_rect(x + 3 + k, y - 5 - k, 3, 2, k < 4 ? PAL_RAMP(RAMP_CANDY, 6) : PAL_RAMP(RAMP_STEEL, 4));
        plat_rect(x + 3 + k, y - 4 - k, 1, 1, PAL_RAMP(RAMP_STEEL, 2));
    }
    ui_round_rect(x + 18, y - 34, 10, 14, 3, weld_pending.style == WSTYLE_CUTE ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_RAMP(RAMP_BLOOD, 3));
    for (k = 0; k < SPARKS; k++) {
        const Spark *s = &sparks[k];
        int sx = s->x / 16, sy = s->y / 16;
        if (!s->life) continue;
        if (s->kind == 1 && s->life > 6) ui_heart(sx, sy, 2, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        else if (s->kind == 2 && s->life > 6) ui_sparkle(sx, sy, s->life, PAL_UI_GOLD);
        else {
            plat_rect(sx, sy, 1, 1, s->life > 8 ? PAL_RAMP(RAMP_LAVA, 7) : PAL_RAMP(RAMP_LAVA, 5));
            plat_rect(sx - (s->vx > 0 ? 1 : -1), sy - 1, 1, 1, PAL_RAMP(RAMP_LAVA, 4));
        }
    }
}

static void heat_gauge(int sw, int sh)
{
    int x = sw - 26, y = sh / 2 - 44, h = 88, w = 12;
    int ylo = y + h - zone_lo * h / 1000, yhi = y + h - zone_hi * h / 1000;
    int hy = y + h - heat * h / 1000;
    ui_round_rect(x - 2, y - 2, w + 4, h + 4, 4, PAL_RAMP(RAMP_STONE, 1));
    ui_round_rect(x, y, w, h, 3, PAL_RAMP(RAMP_STONE, 3));
    plat_rect(x + 2, yhi, w - 4, ylo - yhi, PAL_RAMP(RAMP_GRASS, 4));
    plat_rect(x + 2, yhi, w - 4, 1, PAL_RAMP(RAMP_GRASS, 6));
    plat_rect(x + 2, hy, w - 4, y + h - hy, heat > zone_hi ? PAL_RAMP(RAMP_LAVA, 5) :
                                           heat < zone_lo ? PAL_RAMP(RAMP_SKY, 5) : PAL_RAMP(RAMP_LAVA, 6));
    ui_fill_circle(x + w / 2, y + h + 6, 7, PAL_RAMP(RAMP_STONE, 1));
    ui_fill_circle(x + w / 2, y + h + 6, 6, heat > zone_hi ? PAL_UI_RED : PAL_RAMP(RAMP_LAVA, 5));
    plat_rect(x - 6, hy - 1, 5, 3, PAL_WHITE);
    plat_text(x - 4, y - 12, "HEAT", PAL_UI_CREAM);
}

static void wg_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    char buf[40];
    const DoomoImage *icon;
    if (!scene_scratch_mine(WG_SCRATCH_TAG)) { plat_clear(PAL_BLACK); return; }
    icon = weld_icon_image(&weld_pending);
    ui_backdrop(WIN_DARK, (uint32_t)wg_t);
    plates(sw, sh);
    bead();
    if (wg_state == WG_PASS || wg_state == WG_BETWEEN) torch();
    heat_gauge(sw, sh);
    /* what we are welding */
    plat_image(6, 4, icon, 2, IMG_SHADOW, 0);
    weld_name(&weld_pending, buf, sizeof(buf));
    ui_text_outline(58, 6, buf, PAL_UI_CREAM, PAL_RAMP(RAMP_STONE, 0));
    doomo_snprintf(buf, sizeof(buf), "Pass %d/2  Streak %d", DOOMO_MIN(wg_pass + 1, 2), streak);
    ui_text_outline(58, 16, buf, PAL_UI_GOLD, PAL_RAMP(RAMP_STONE, 0));
    if (score_n) {
        int q = (int)(score_sum / score_n);
        ui_bar(58, 26, 80, 6, q, 100, q >= 80 ? PAL_UI_MINT : q >= 50 ? PAL_UI_GOLD : PAL_UI_RED);
    }
    switch (wg_state) {
    case WG_INTRO:
        if (ui_window_pop(sw / 2 - 100, sh - 70, 200, 52, WIN_CUTE, wg_t)) {
            ui_text_wrap(sw / 2 - 94, sh - 64, 188,
                         "Rivet: Keep the bead on the seam with UP/DOWN. Feather " STR_ICON_A
                         " to keep the heat in the green!", PAL_UI_PINK_D, 1);
        }
        ui_hint(STR_ICON_A " Start  " STR_ICON_B " Cancel", WIN_CUTE);
        break;
    case WG_BETWEEN:
        ui_text_big(sw / 2 - ui_text_big_width("NICE!", 2) / 2, sh / 4, "NICE!", 2, PAL_UI_GOLD, PAL_RAMP(RAMP_STONE, 0));
        break;
    case WG_RESULT: {
        static const char *const grades[5] = { "D", "C", "B", "A", "S" };
        int g = quality >= 95 ? 4 : quality >= 80 ? 3 : quality >= 60 ? 2 : quality >= 40 ? 1 : 0;
        int s = ui_pop_scale(wg_t), k;
        int sc = s > 280 ? 5 : 4;
        plat_shade_rect(0, 0, sw, sh, 9);
        if (ui_window_pop(sw / 2 - 90, sh / 2 - 44, 180, 88, WIN_CUTE, wg_t)) {
            ui_text_big(sw / 2 - 50, sh / 2 - 36, grades[g], sc,
                        g >= 3 ? PAL_UI_GOLD : PAL_UI_PINK, PAL_UI_PINK_D);
            doomo_snprintf(buf, sizeof(buf), "Quality %d", quality);
            ui_text(sw / 2 - 10, sh / 2 - 30, buf, PAL_UI_PINK_D);
            doomo_snprintf(buf, sizeof(buf), "Best streak %d", best_streak);
            ui_text(sw / 2 - 10, sh / 2 - 20, buf, PAL_UI_PINK_D);
            for (k = 0; k < 5; k++)
                ui_star(sw / 2 - 40 + k * 20, sh / 2 + 22 + (k == (wg_t / 6) % 5 ? -2 : 0), 6,
                        k <= g ? PAL_UI_GOLD : PAL_RAMP(RAMP_STONE, 3));
            ui_text_center(sw / 2, sh / 2 + 32, wg_practice ? "(practice)" :
                           quality >= 100 ? "LEGENDARY WELD!" : "Rivet: \"Not bad!\"", PAL_UI_PINK_D);
        }
        if (wg_t > 40) ui_hint(STR_ICON_A " Done", WIN_CUTE);
        break;
    }
    default:
        ui_hint(STR_ICON_ARROW_U STR_ICON_ARROW_D " Steer  hold " STR_ICON_A " Heat  " STR_ICON_B " Stop", WIN_DARK);
        break;
    }
}

const DoomoScene SCENE_WELD_GAME = { "WELD_GAME", wg_enter, NULL, wg_update, wg_draw, 0 };
