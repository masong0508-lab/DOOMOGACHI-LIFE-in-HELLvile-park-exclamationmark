/* story_mg_a.c - S4's story minigames, the doomo_story_mg_a registry rows
 * (owner: S4; docs/design/story/SPEC.md sec.7 S4, C14; story_final.md
 * sec.14). SCENE_STORY_MG (story_mg.c) frames each one with its title and
 * result cards; `push minigame SMG_A_*` (story_int.h) starts it and the VM
 * reads the result (var 0) after the pop.
 *
 * Four small engines, six games:
 *   HOP   S150 "Lava Hopscotch": ten stone slabs over lava, hop with A on the
 *         beat (the pet goes first and sizzles; it's fine). Three sizzles
 *         and you lose (properly).
 *   ROLL  S143 the town sign: D-pad the roller, hold A to paint; the
 *         population counter rolls 500 -> 1000 as v = 500 + 500 (a/22)^2.2.
 *   GRID  S146 Whack-an-Imp (on: bonk twelve; Father Fuzz pops up too, do
 *         NOT bonk him: result 2 if you do), Whack-an-Imp switched off (they
 *         come anyway, slower, looking at you; the last one says "beep."),
 *         S130 Twisty's balloons (pop the grey ones, keep twenty pink ones;
 *         a grey one left alone pops one of yours; pop three pinks and lose).
 *   BOB   S144 the float: pull (A) when it dips, not when it twitches.
 *         Three pulls land the fish; five spooks and he gets away.
 *
 * Staging (cartoon-modest, GRUMBLEGUTS_AND_DIRECTOR_NOTES): a lit cabinet
 * with a marquee for the machine (its bulbs die when it is switched off and
 * the imps' eyes glow instead), imps that pop out of their holes, a striped
 * stall for the balloons with your bundle growing in the corner, stone
 * slabs with chalk numbers over bubbling lava, a sky and a pier for the
 * float. Only ui_* / plat_rect spans: no art of its own.
 *
 * State lives in the shared scene scratch (0 B of EWRAM), deterministic
 * per game (story_rng), 30 Hz. Story-minigame group code on the PS1 (the
 * "storymg" overlay: the name matches story_mg(_\w+)?).
 */
#include "story_int.h"
#include "doomo/save_ext.h"

/* Size over speed for this file only (SPEC S4: story_mg_a.c <= 6 KB of
 * code): a handful of ui_* spans per frame at 30 Hz, nothing hot - every
 * toolchain here is GCC (gcc, arm-none-eabi-gcc, mipsel). */
#pragma GCC optimize ("Os")

#define MGA_TAG SAVE_EXT_TAG('S', 'M', 'G', 'A')
enum { E_HOP, E_ROLL, E_GRID, E_BOB };
enum { H_NONE, H_IMP, H_FUZZ, H_PINK, H_GREY, H_BEEP };

typedef struct MgA {
    uint16_t id, t, next;
    uint8_t  eng, done, result, end_t;
    int8_t   cur;               /* hole / roller column / square reached (-1 start) */
    uint8_t  n, bad, flag;      /* hops, painted columns, kept pinks, pulls | misses | Fuzz bonked */
    uint8_t  hop_t, fx_t, spawned, bite, twitch, shake;
    uint8_t  hit_k, hit_kind;   /* GRID: the last bonk, for its burst (fx_t frames) */
    uint8_t  kind[6], life[6];  /* GRID holes */
    uint8_t  paint[22];         /* ROLL columns, 0..3 */
} MgA;
#define G ((MgA *)scene_scratch_block())

static const struct {
    uint8_t beat, win, demo, hop;           /* HOP: frames per beat, +- window, demo hop, hop arc */
    uint8_t gap_on, gap_off, gap_pink, up_on, up_off, up_pink;   /* GRID spawn gaps, lifetimes */
    uint8_t bonks, pinks, offs, pink_bad;   /* GRID goals */
    uint8_t dip, twitch, pulls, spooks;     /* BOB */
    uint8_t end, burst;                     /* frames of the closing flourish / a bonk's burst */
} VA = { 16, 4, 8, 8, 18, 40, 16, 36, 60, 50, 12, 20, 8, 3, 14, 6, 3, 5, 24, 8 };
#define GRID_SECONDS_ON 30
#define GRID_SECONDS_PINK 75

/* (a / 22)^2.2 x 500: the s12 counter's ease (S143) */
static const uint16_t roll_ease[23] = { 0, 1, 3, 6, 12, 19, 29, 40, 54, 70, 88, 109,
                                        132, 157, 185, 215, 248, 284, 322, 362, 405, 451, 500 };
/* the hopscotch: columns 0..7, rows 0 top / 1 middle / 2 bottom (4-5 and 7-8 doubles) */
static const uint8_t hop_col[10] = { 0, 1, 2, 3, 3, 4, 5, 5, 6, 7 };
static const uint8_t hop_row[10] = { 1, 1, 1, 0, 2, 1, 0, 2, 1, 1 };

static uint32_t rnd(uint32_t k) { return story_rng(((uint32_t)G->id << 16) ^ ((uint32_t)G->t << 3) ^ k); }

static void finish(int result)
{
    if (G->done) return;
    G->done = 1;
    G->result = (uint8_t)result;
    G->end_t = 0;
}

static void enter(int id)
{
    MgA *g = (MgA *)scene_scratch(MGA_TAG, (uint32_t)sizeof(MgA));
    if (!g) return;
    memset(g, 0, sizeof(*g));
    g->id = (uint16_t)id;
    g->eng = id == SMG_A_HOPSCOTCH ? E_HOP : id == SMG_A_ROLLER ? E_ROLL : id == SMG_A_BOBBER ? E_BOB : E_GRID;
    g->cur = g->eng == E_HOP ? -1 : g->eng == E_ROLL ? 0 : 1;
    g->next = 20;
}

/* ---- shared bits ---------------------------------------------------------- */
static void eyes(int x, int y, int gap, int r, int look, uint8_t white)
{
    ui_fill_circle(x - gap, y, r, white);
    ui_fill_circle(x + gap, y, r, white);
    if (white != PAL_UI_GOLD) {                           /* glowing eyes have no pupils */
        ui_fill_circle(x - gap + look, y, r / 2, PAL_BLACK);
        ui_fill_circle(x + gap + look, y, r / 2, PAL_BLACK);
    }
}

static void lcd(int x, int y, const char *s)
{
    ui_round_rect(x, y, ui_text_width(s) + 10, 14, 4, PAL_BLACK);
    ui_text(x + 5, y + 3, s, PAL_UI_LCD);
}

/* a scalloped awning of two colours */
static void awning(int x, int y, int w, int h, uint8_t a, uint8_t b)
{
    int k;
    for (k = 0; k * 12 < w; k++) {
        int cw = DOOMO_MIN(12, w - k * 12);
        plat_rect(x + k * 12, y, cw, h, (k & 1) ? a : b);
        ui_fill_circle(x + k * 12 + cw / 2, y + h, cw / 2, (k & 1) ? a : b);
    }
}

/* hearts: `left` of three still lit */
static void lives(int x, int y, int left, uint8_t c)
{
    int k;
    for (k = 0; k < 3; k++) ui_heart(x + k * 14, y, 4, k < left ? c : PAL_GRAY_D);
}

/* ---- HOP ------------------------------------------------------------------ */
static void hop_update(void)
{
    MgA *g = G;
    int demo = 10 * VA.demo + 12, t, off;
    if (g->hop_t) g->hop_t--;
    if (g->shake) g->shake--;
    if (g->t < demo) {                                   /* the pet goes first */
        if (g->t % VA.demo == 0 && g->t / VA.demo < 10) {
            g->hop_t = VA.hop;
            audio_sfx(g->t / VA.demo == 2 ? SFX_SPLAT : SFX_BOING);
        }
        return;
    }
    t = g->t - demo;
    off = t % VA.beat;
    if (off == 0) audio_sfx(SFX_TIK);
    if (btn_pressed(BTN_A)) {
        if (off <= VA.win || VA.beat - off <= VA.win) {
            g->cur++;
            g->hop_t = VA.hop;
            audio_sfx(g->cur >= 9 ? SFX_HAPPY : SFX_BOING);
            if (g->cur >= 9) finish(1);
        } else {
            g->bad++;
            g->shake = 12;
            audio_sfx(SFX_SPLAT);
            if (g->bad >= 3) finish(0);
        }
    }
}

static void square_xy(int k, int sw, int sh, int *x, int *y, int *s)
{
    int cw = (sw - 32) / 8;
    *s = cw - 4;
    *x = 16 + hop_col[k] * cw + 2;
    *y = sh / 2 - *s / 2 + 4 + (hop_row[k] - 1) * (*s * 3 / 5);
}

static void hopper(int x, int y, int r, uint8_t c, int mitts, int arc)
{
    ui_fill_ellipse(x, y + r, r, r / 3 + 1, PAL_RAMP(RAMP_LAVA, 0));       /* its shadow on the slab */
    y -= arc;
    ui_fill_circle(x, y, r, c);
    ui_fill_circle(x - r / 3, y - r / 3, r / 4 + 1, ui_lighter(c));
    eyes(x, y - r / 5, r / 3, 1 + r / 6, 0, PAL_WHITE);
    if (mitts) {
        ui_fill_circle(x - r - 1, y + 2, r / 3 + 2, PAL_PAINT(PAINT_CHERRY, 1));
        ui_fill_circle(x + r + 1, y + 2, r / 3 + 2, PAL_PAINT(PAINT_CHERRY, 1));
    } else {                                              /* the pet's little ears */
        ui_fill_circle(x - r / 2, y - r, r / 3, c);
        ui_fill_circle(x + r / 2, y - r, r / 3, c);
    }
}

static void puff(int x, int y, int t)
{
    int k;
    for (k = 0; k < 3; k++)
        ui_fill_circle(x - 6 + k * 6, y - t - k * 3, 3 + (t + k) % 3, PAL_GRAY_L);
}

static void hop_draw(int sw, int sh, int big)
{
    MgA *g = G;
    int k, x, y, s, demo = 10 * VA.demo + 12, arc = g->hop_t ? (int)g->hop_t * (VA.hop - g->hop_t) / 2 : 0;
    int beat = g->t >= demo ? (int)((g->t - demo) % VA.beat) : 99;
    plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_LAVA, 2));
    for (y = 6; y < sh; y += 14)                          /* slow crust bands */
        plat_rect(0, y + ui_wave((int)g->t + y, 90, 2), sw, 3, PAL_RAMP(RAMP_LAVA, 3));
    for (k = 0; k < 11; k++) {                            /* bubbles swell and pop */
        int ph = ((int)g->t * 2 + k * 29) % 60, r = ph < 40 ? ph / 10 : 4 - (ph - 40) / 5;
        if (r > 0) ui_fill_circle((k * 53 + 11) % sw, (k * 41 + 23) % sh, r, PAL_RAMP(RAMP_LAVA, 5 + (k & 1)));
    }
    for (k = 0; k < 10; k++) {
        int next = k == g->cur + 1 && beat < 6;
        char num[6];
        square_xy(k, sw, sh, &x, &y, &s);
        plat_rect(x + 2, y + 3, s, s, PAL_RAMP(RAMP_LAVA, 0));               /* the slab's shadow */
        ui_round_rect(x, y, s, s, 3, PAL_RAMP(RAMP_STONE, next ? 6 : 4));
        if (next) ui_frame(x - 1, y - 1, s + 2, s + 2, PAL_UI_GOLD);
        doomo_snprintf(num, sizeof(num), k == 9 ? "HOME" : "%d", k + 1);
        ui_text(x + (k == 9 ? 1 : 3), y + 3, num, PAL_WHITE);
    }
    if (g->t < demo) {                                    /* the pet's run, one sizzle */
        k = DOOMO_MIN((int)g->t / VA.demo, 9);
        square_xy(k, sw, sh, &x, &y, &s);
        hopper(x + s / 2, y + s / 2, s / 3, PAL_PAINT(PAINT_BUBBLEGUM, 1), 0, arc);
        if (k == 2) puff(x + s / 2, y, (int)g->t % 12);
    } else {
        if (g->cur < 0) { x = 2; y = sh / 2 - 8; s = 18; }
        else square_xy(g->cur, sw, sh, &x, &y, &s);
        hopper(x + s / 2 + (g->shake & 2) - 1, y + s / 2, s / 3, PAL_SKIN(1, 1), 1, arc);
        if (g->shake) {
            puff(x + s / 2, y, 12 - g->shake);
            ui_text_outline(x, y - 14, "TSSS!", PAL_WHITE, PAL_RAMP(RAMP_LAVA, 0));
        }
        y = (int)(((g->t - demo) / VA.beat) & 3);
        for (k = 0; k < 4; k++)                           /* the beat: four dots, the one now big */
            ui_fill_circle(sw / 2 - 18 + k * 12, big ? 32 : 24, k == y && beat < 6 ? 4 : 2, k == y ? PAL_UI_GOLD : PAL_RAMP(RAMP_LAVA, 5));
    }
    lives(sw - 40, sh - (big ? 34 : 26), 3 - g->bad, PAL_PAINT(PAINT_CHERRY, 1));
}

/* ---- ROLL ----------------------------------------------------------------- */
static void roll_update(void)
{
    MgA *g = G;
    if ((btn_pressed(BTN_LEFT) || (btn_down(BTN_LEFT) && (g->t & 3) == 0)) && g->cur > 0) g->cur--;
    if ((btn_pressed(BTN_RIGHT) || (btn_down(BTN_RIGHT) && (g->t & 3) == 0)) && g->cur < 21) g->cur++;
    if (btn_down(BTN_A) && (g->t & 1) && g->paint[g->cur] < 3) {
        if (++g->paint[g->cur] == 3) {
            g->n++;
            audio_sfx(g->n == 11 || g->n == 22 ? SFX_TINK : SFX_SPLAT);
        }
    }
    if (g->n >= 22) finish(1);
}

static void roll_draw(int sw, int sh, int big)
{
    MgA *g = G;
    int bw = sw - 40, bh = big ? 96 : 70, bx = 20, by = big ? 48 : 34, k, cw = bw / 22, x0 = bx + (bw - 22 * cw) / 2, tw;
    int scale = big ? 4 : 3;
    char buf[8];
    ui_backdrop(WIN_MINT, g->t);
    plat_rect(0, sh - 22, sw, 22, PAL_RAMP(RAMP_GRASS, 4));                  /* the verge */
    plat_rect(bx + 16, by + bh, 6, sh - by - bh, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(bx + bw - 22, by + bh, 6, sh - by - bh, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(bx + 3, by + 4, bw, bh, PAL_RAMP(RAMP_WOOD, 1));              /* the board's shadow */
    ui_round_rect(bx, by, bw, bh, 4, PAL_RAMP(RAMP_WOOD, 4));
    ui_frame(bx, by, bw, bh, PAL_RAMP(RAMP_WOOD, 1));
    ui_text_center(sw / 2, by + 5, "POPULATION:", PAL_RAMP(RAMP_WOOD, 1));
    for (k = 0; k < 22; k++)                              /* the fresh coat, pink and red */
        if (g->paint[k])
            plat_rect(x0 + k * cw, by + 18, cw, bh - 24 + (g->paint[k] == 3 && k % 3 == 0) * (2 + k % 4),   /* a drip or two */
                      g->paint[k] < 3 ? PAL_PAINT(PAINT_ROSE, 2) : PAL_PAINT(k & 1 ? PAINT_BUBBLEGUM : PAINT_CHERRY, 2));
    doomo_snprintf(buf, sizeof(buf), "%d", 500 + roll_ease[g->n]);
    tw = ui_text_big_width(buf, scale);
    ui_text_big(sw / 2 - tw / 2, by + bh / 2 - 4 * scale + 6, buf, scale, PAL_WHITE, PAL_RAMP(RAMP_WOOD, 1));
    k = x0 + g->cur * cw + cw / 2;                        /* the roller on its pole */
    plat_rect(k - 1, by + bh - 4, 2, big ? 34 : 26, PAL_RAMP(RAMP_WOOD, 1));
    ui_round_rect(k - 9, by + bh - 11 - (btn_down(BTN_A) ? 2 : 0), 18, 9, 4, PAL_PAINT(PAINT_BUBBLEGUM, 1));
}

/* ---- GRID ----------------------------------------------------------------- */
static int grid_spawn_kind(void)
{
    MgA *g = G;
    if (g->id == SMG_A_BALLOONS) return (rnd(7) % 100) < 55 ? H_PINK : H_GREY;
    if (g->id == SMG_A_WHACK_OFF) return g->spawned + 1 >= VA.offs ? H_BEEP : H_IMP;
    return g->spawned > 2 && (rnd(9) % 10) == 0 ? H_FUZZ : H_IMP;
}

static int up_of(int k)
{
    MgA *g = G;
    return g->id == SMG_A_WHACK_OFF ? VA.up_off + (g->kind[k] == H_BEEP ? 20 : 0) :
           g->id == SMG_A_BALLOONS ? VA.up_pink : VA.up_on;
}

static void grid_update(void)
{
    MgA *g = G;
    int k, off = g->id == SMG_A_WHACK_OFF, pink = g->id == SMG_A_BALLOONS, busy = 0;
    if (btn_pressed(BTN_LEFT) && g->cur % 3 > 0) g->cur--;
    if (btn_pressed(BTN_RIGHT) && g->cur % 3 < 2) g->cur++;
    if (btn_pressed(BTN_UP) && g->cur >= 3) g->cur -= 3;
    if (btn_pressed(BTN_DOWN) && g->cur < 3) g->cur += 3;
    if (g->fx_t) g->fx_t--;
    for (k = 0; k < 6; k++) {
        if (!g->kind[k]) continue;
        busy++;
        if (g->life[k] && --g->life[k]) continue;
        if (g->kind[k] == H_PINK) { g->n++; audio_sfx(SFX_BOING); }               /* floats up: yours */
        if (g->kind[k] == H_GREY && g->n) { g->n--; audio_sfx(SFX_POP2); }        /* ...pops one of yours */
        if (g->kind[k] == H_BEEP) finish(1);
        g->kind[k] = H_NONE;
    }
    if (g->t >= g->next && !(off && g->spawned >= VA.offs)) {
        k = (int)(rnd(3) % 6);
        while (busy < 6 && g->kind[k]) k = (k + 1) % 6;
        if (busy < 6) {
            g->kind[k] = (uint8_t)grid_spawn_kind();
            g->life[k] = (uint8_t)up_of(k);
            g->spawned++;
            if (g->kind[k] == H_BEEP) audio_sfx(SFX_BEEP);
        }
        g->next = (uint16_t)(g->t + (off ? VA.gap_off : pink ? VA.gap_pink : VA.gap_on) + rnd(5) % 8);
    }
    if (btn_pressed(BTN_A)) {
        int h = g->kind[g->cur];
        g->fx_t = VA.burst;
        g->hit_k = (uint8_t)g->cur;
        g->hit_kind = (uint8_t)h;
        if (h == H_FUZZ) { g->flag = 1; g->shake = 8; audio_sfx(SFX_THUD); }
        else if (h == H_PINK) { g->bad++; audio_sfx(SFX_POP2); }
        else if (h) { if (h == H_IMP && !off) g->n++; audio_sfx(SFX_POP2); }   /* an imp bonked, a grey popped */
        else audio_sfx(SFX_TIK);
        if (h == H_BEEP) finish(1);
        if (h) g->kind[g->cur] = H_NONE;
    }
    if (g->shake) g->shake--;
    if (pink) {
        if (g->n >= VA.pinks) finish(1);
        else if (g->bad >= VA.pink_bad || g->t >= GRID_SECONDS_PINK * 30) finish(0);
    } else if (!off && (g->n >= VA.bonks || g->t >= GRID_SECONDS_ON * 30)) {
        finish(g->flag ? 2 : 1);
    }
}

/* an imp (or Father Fuzz, who is not in the machine) rising out of hole k */
static void popper(int k, int cx, int cy, int r, int off)
{
    MgA *g = G;
    int h = g->kind[k], age = up_of(k) - g->life[k], rise = DOOMO_MIN(age * 3, r + 4);
    int y = cy + r + 2 - rise;
    if (g->life[k] < 6) y += (6 - g->life[k]) * 2;        /* ducks back down */
    if (h == H_FUZZ) {
        ui_fill_ellipse(cx, y - r - 3, r - 2, 2, PAL_UI_GOLD);                 /* the halo */
        ui_fill_circle(cx, y, r, PAL_PAINT(PAINT_LAVENDER, 1));
        plat_rect(cx - 3, y + r - 4, 6, 3, PAL_WHITE);                         /* the collar */
        eyes(cx, y - 2, r / 3, 2, 0, PAL_WHITE);
    } else {
        int look = off ? ((g->cur % 3) - (k % 3)) * 2 : 0;
        uint8_t skin = off ? PAL_PAINT(PAINT_BLOOD, 0) : PAL_PAINT(PAINT_CHERRY, 1);
        ui_fill_circle(cx - r + 3, y - r + 2, r / 4 + 1, PAL_PAINT(PAINT_BONE, 1));   /* horns */
        ui_fill_circle(cx + r - 3, y - r + 2, r / 4 + 1, PAL_PAINT(PAINT_BONE, 1));
        ui_fill_circle(cx, y, r, skin);
        eyes(cx, y - 2, r / 3, 2, look, off ? PAL_UI_GOLD : PAL_WHITE);
        if (!off) ui_fill_ellipse(cx, y + r / 3 + 1, r / 3, 2, PAL_BLACK);     /* the grin */
        if (h == H_BEEP) {
            ui_round_rect(cx + 6, y - r - 16, 36, 13, 5, PAL_WHITE);
            ui_fill_circle(cx + 8, y - r - 3, 2, PAL_WHITE);
            ui_text(cx + 10, y - r - 13, "beep.", PAL_BLACK);
        }
    }
}

static void grid_draw(int sw, int sh, int big)
{
    MgA *g = G;
    int off = g->id == SMG_A_WHACK_OFF, pink = g->id == SMG_A_BALLOONS, k;
    int top = big ? 78 : 58, rh = big ? 64 : 42, cw = (sw - 60) / 3, r = big ? 13 : 9;
    uint8_t deck = pink ? PAL_PAINT(PAINT_SKY, 2) : off ? PAL_PAINT(PAINT_JET, 1) : PAL_PAINT(PAINT_MINT, 2);
    char buf[24];
    if (off) plat_rect(0, 0, sw, sh, PAL_PAINT(PAINT_JET, 0));
    else ui_backdrop(pink ? WIN_MINT : WIN_CUTE, g->t);
    /* the cabinet / the stall */
    ui_round_rect(16, top - 30, sw - 32, 2 * rh + 36, 8, pink ? PAL_PAINT(PAINT_CARAMEL, 2) : off ? PAL_PAINT(PAINT_GRAPE, 0) : PAL_PAINT(PAINT_GRAPE, 1));
    ui_round_rect(24, top - 14, sw - 48, 2 * rh + 12, 6, deck);
    if (pink) {
        awning(16, top - 48, sw - 32, 12, PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_WHITE);
        ui_text_center(sw / 2, top - 27, "TWISTY'S BALLOONS", PAL_PAINT(PAINT_CHOCOLATE, 0));
    } else {
        ui_round_rect(sw / 2 - 64, top - 50, 128, 20, 8, off ? PAL_GRAY_D : PAL_PAINT(PAINT_CHERRY, 1));
        ui_text_center(sw / 2, top - 44, "WHACK-AN-IMP", off ? PAL_GRAY : PAL_WHITE);
        for (k = 0; k < 11; k++) {                        /* the marquee's bulbs */
            uint8_t c = off ? PAL_GRAY_D : ((g->t / 6 + k) & 1) ? PAL_UI_GOLD : PAL_WHITE;
            ui_fill_circle(sw / 2 - 60 + k * 12, top - 52 + (k & 1) * 24, 2, c);
        }
    }
    for (k = 0; k < 6; k++) {
        int cx = 30 + (k % 3) * cw + cw / 2 + ((g->shake & 2) ? 2 : 0), cy = top + (k / 3) * rh + rh / 2 - 6, h = g->kind[k];
        if (pink) {
            ui_fill_circle(cx, cy + r + 4, 2, PAL_PAINT(PAINT_CHOCOLATE, 1));   /* the peg */
            if (h == H_PINK || h == H_GREY) {
                int up = (VA.up_pink - g->life[k]) / 3;
                uint8_t c = h == H_PINK ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_GRAY;
                plat_rect(cx, cy + r - up, 1, 6 + up, PAL_GRAY_D);
                ui_fill_ellipse(cx, cy - up, r - 2, r, c);
                ui_fill_circle(cx - 3, cy - 4 - up, 2, PAL_WHITE);
                ui_fill_circle(cx, cy + r - up, 2, c);
            }
        } else {
            ui_fill_ellipse(cx, cy + r + 2, r + 5, r / 2 + 1, off ? PAL_BLACK : PAL_GRAY_D);   /* the hole */
            if (h) popper(k, cx, cy, r, off);
            ui_fill_ellipse(cx, cy + r + r / 2 + 4, r + 6, r / 2, deck);                     /* its front lip */
        }
        if (k == g->cur) {                                /* the mallet / the pin */
            int sq = g->fx_t > VA.burst - 4 ? 4 : 0;
            plat_rect(cx + r + 2, cy - r - 4 + sq, 3, r + 8, PAL_RAMP(RAMP_WOOD, 2));
            ui_round_rect(cx + r - 6, cy - r - 12 + sq, 18, 9 - sq / 2, 4, pink ? PAL_GRAY_L : PAL_PAINT(PAINT_SKY, 1));
        }
        if (g->fx_t && k == g->hit_k && g->hit_kind) {    /* the burst */
            ui_star(cx, cy - r, 6 + (VA.burst - g->fx_t), PAL_UI_GOLD);
            ui_text_outline(cx - 14, cy - r - 22, g->hit_kind == H_FUZZ ? "tsk." : pink ? "POP!" : "BONK!", PAL_WHITE, PAL_BLACK);
        }
    }
    if (pink) {
        for (k = 0; k < g->n && k < 6; k++)               /* your bundle, by the counter */
            ui_fill_ellipse(sw - 58 - k * 5, top - 22 + (k & 1) * 3, 3, 4, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        lives(26, top - 21, VA.pink_bad - g->bad, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        doomo_snprintf(buf, sizeof(buf), "%d/%d", g->n, VA.pinks);
        lcd(sw - 50, top - 28, buf);
    } else if (!off) {
        doomo_snprintf(buf, sizeof(buf), "%02d", g->n);
        lcd(sw - 44, top - 50, buf);
    }
}

/* ---- BOB ------------------------------------------------------------------ */
static void bob_update(void)
{
    MgA *g = G;
    if (g->bite) g->bite--;
    if (g->twitch) g->twitch--;
    if (g->t >= g->next && !g->bite && !g->twitch) {
        if (rnd(11) % 10 < 4) g->twitch = VA.twitch;
        else { g->bite = VA.dip; audio_sfx(SFX_SPLASH); }
        g->next = (uint16_t)(g->t + VA.dip + 30 + rnd(13) % 50);
    }
    if (btn_pressed(BTN_A)) {
        if (g->bite) {
            g->n++;
            g->bite = 0;
            g->fx_t = 10;
            audio_sfx(g->n >= VA.pulls ? SFX_HAPPY : SFX_BOING);
            if (g->n >= VA.pulls) finish(1);
        } else {
            g->bad++;
            audio_sfx(SFX_CANCEL);
            if (g->bad >= VA.spooks) finish(0);
        }
    }
    if (g->fx_t) g->fx_t--;
}

static void bob_draw(int sw, int sh, int big)
{
    MgA *g = G;
    int wy = sh * 11 / 20, fx = sw * 5 / 8, k;
    int fy = wy - 3 + ui_wave(g->t, 40, 2) + (g->twitch ? 2 : 0) + (g->bite ? 7 : 0) - (g->fx_t ? 6 : 0);
    plat_rect(0, 0, sw, wy, PAL_RAMP(RAMP_SKY, 6));
    ui_fill_circle(sw - 34, big ? 46 : 36, big ? 12 : 9, PAL_PAINT(PAINT_LEMON, 2));          /* the sun */
    for (k = 0; k < 3; k++) {                                                              /* clouds */
        int cx = ((int)g->t / (3 + k) + k * 97) % (sw + 60) - 30, cy = (big ? 52 : 40) + k * 9;
        ui_fill_ellipse(cx, cy, 16, 5, PAL_WHITE);
    }
    plat_rect(0, wy, sw, sh - wy, PAL_RAMP(RAMP_SKY, 2));
    for (k = 0; k < 6; k++)
        plat_rect((k * 47 + (int)g->t / 2) % sw, wy + 6 + k * 9, 14, 1, PAL_RAMP(RAMP_SKY, 4));
    plat_rect(0, sh - 26, sw / 3, 26, PAL_RAMP(RAMP_WOOD, 3));            /* the pier */
    for (k = 0; k < 4; k++) plat_rect(k * (sw / 12), sh - 26, 1, 26, PAL_RAMP(RAMP_WOOD, 1));
    for (k = 0; k < 16; k++)                                              /* the line */
        plat_rect(sw / 6 + (fx - sw / 6) * k / 16, sh / 4 + (fy - sh / 4) * k / 16, 2, 1, PAL_GRAY_L);
    if (g->bite || g->twitch) ui_fill_ellipse(fx, wy + 2, 6 + (g->bite ? VA.dip - g->bite : 2) * 2, 3, PAL_RAMP(RAMP_SKY, 5));
    ui_fill_circle(fx, fy, big ? 6 : 5, PAL_WHITE);
    ui_fill_ellipse(fx, fy + 2, big ? 6 : 5, 3, PAL_PAINT(PAINT_CHERRY, 1));
    plat_rect(fx, fy - (big ? 10 : 8), 1, 4, PAL_BLACK);
    for (k = 0; k < VA.pulls; k++) ui_star(sw - 14 - k * 16, sh - (big ? 40 : 32), 6, k < g->n ? PAL_UI_GOLD : PAL_GRAY_D);
    for (k = 0; k < VA.spooks; k++) ui_fill_circle(10 + k * 9, big ? 36 : 26, 3, k < g->bad ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_GRAY_L);
}

/* ---- the registry rows ------------------------------------------------------ */
static int update(void)
{
    MgA *g = G;
    if (!scene_scratch_mine(MGA_TAG)) return 1 + 1;       /* the scratch is gone: count it as played */
    g->t++;
    if (g->done) return ++g->end_t >= VA.end ? 1 + g->result : 0;
    switch (g->eng) {
    case E_HOP: hop_update(); break;
    case E_ROLL: roll_update(); break;
    case E_BOB: bob_update(); break;
    default: grid_update(); break;
    }
    return 0;
}

static void draw(void)
{
    /* each engine's title (GRID's is its marquee / awning) and hint */
    static const char *const title[4] = { "Lava Hopscotch", "Repaint the sign", NULL, "Watch the float" };
    static const char *const hint[4] = { STR_ICON_A " Hop on the beat!", STR_ICON_A " Paint  <> Roll",
                                         STR_ICON_A " Bonk! (not Father Fuzz)", STR_ICON_A " Pull when it dips!" };
    static const uint8_t style[4] = { WIN_HELL, WIN_MINT, WIN_CUTE, WIN_MINT };
    MgA *g = G;
    int sw = plat_screen_w(), sh = plat_screen_h(), big = sh >= 200, e = g->eng;
    const char *h = hint[e];
    if (!scene_scratch_mine(MGA_TAG)) { ui_backdrop(WIN_CUTE, 0); return; }
    switch (e) {
    case E_HOP: hop_draw(sw, sh, big); break;
    case E_ROLL: roll_draw(sw, sh, big); break;
    case E_BOB: bob_draw(sw, sh, big); break;
    default: grid_draw(sw, sh, big); break;
    }
    if (title[e])
        ui_title(sw / 2, big ? 8 : 4, e == E_HOP && g->t < 10 * VA.demo + 12 ? "The pet goes first!" :
                 e == E_BOB && g->bite ? "BITE!" : title[e], style[e]);
    if (g->id == SMG_A_BALLOONS) h = STR_ICON_A " Pop the grey ones";
    else if (e == E_GRID && (g->spawned > 3 || g->id == SMG_A_WHACK_OFF)) h = STR_ICON_A " Bonk!";
    ui_hint(h, g->id == SMG_A_WHACK_OFF ? WIN_DARK : style[e]);
    if (g->done && g->result)
        ui_sparkle(sw / 2 + ui_wave(g->end_t * 4, 40, 30), sh / 2 - 20, g->end_t & 15, PAL_UI_GOLD);
}

static const StoryMgEntry rows[] = {
    { SMG_A_HOPSCOTCH, "Lava Hopscotch", enter, update, draw },
    { SMG_A_ROLLER, "Paint the Sign", enter, update, draw },
    { SMG_A_WHACK, "Whack-an-Imp", enter, update, draw },
    { SMG_A_WHACK_OFF, "Whack-an-Imp (off)", enter, update, draw },
    { SMG_A_BALLOONS, "Balloon Stall", enter, update, draw },
    { SMG_A_BOBBER, "Gone Fishing", enter, update, draw },
    { 0, NULL, NULL, NULL, NULL },
};
const StoryMgEntry *const doomo_story_mg_a = rows;
