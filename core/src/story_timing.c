/* story_timing.c - SCENE_STORY_TIMING, the comic-timing press minigame
 * (owner: S2; docs/design/story/SPEC.md sec.7 S2, C14; story_final.md
 * sec.15 "timing"; comic_timing.md's beat ladder).
 *
 * arg = TPAT_* | (TSKIN_* | TIMING_DRIFT) << 8 (story_vm.h TIMING_ARG).
 * A metronome clicks; a ball hops from dot to dot along the round's
 * pattern (gaps in beats, e.g. the rule of three 3-1, 3-1-2, 3-1-2-1) and
 * the last dot is the punchline: press A exactly when the ball lands on
 * it. Three rounds; the result (var 0 after the pop) is the rounds landed,
 * 0..3. Perfect = within 2 frames, good = within 5.
 *
 * Skins change the sound and the crowd: thump (a drum), laugh (a laugh
 * track that roars when you land it), bell, heart (a heartbeat, for the
 * sincere ones). TIMING_DRIFT (S155) makes the metronome drift a frame or
 * two per beat, deterministically (story_rng), so the player has to
 * listen instead of counting.
 *
 * Everything advances on the 30 Hz tick; every frame count is in the
 * table below. Story group code on the PS1 (the name matches the regex).
 */
#include "story_int.h"
#include "doomo/camfx.h"

/* gaps in beats; [0] = the number of gaps (the last one ends on the punch) */
static const uint8_t patterns[TPAT_COUNT][3][5] = {
    { { 2, 3, 1 }, { 3, 3, 1, 2 }, { 4, 3, 1, 2, 1 } },          /* THREE   */
    { { 3, 2, 2, 1 }, { 3, 2, 1, 1 }, { 3, 1, 1, 2 } },          /* KNOCK   */
    { { 2, 4, 4 }, { 3, 4, 2, 2 }, { 4, 2, 2, 2, 2 } },          /* PRAYER  */
    { { 2, 2, 1 }, { 3, 1, 1, 1 }, { 3, 3, 2, 1 } },             /* RIMSHOT */
};
static const char *const pattern_names[TPAT_COUNT] = { "Rule of Three", "Knock Knock", "The Prayer", "Ba-dum Tss" };

static const struct {
    uint8_t beat;            /* frames per beat (R13 + 2: the trailer's 15-16 f rhythm) */
    uint8_t lead;            /* count-in beats before a round                     */
    uint8_t perfect, good;   /* |error| in frames                                 */
    uint8_t late;            /* frames after the punch before it is a miss         */
    uint8_t react;           /* frames the reaction stays                          */
    uint8_t card;            /* frames before A starts / ends                      */
    uint8_t drift_max;       /* frames a drifting beat may stray                   */
} TT = { 15, 2, 2, 5, 8, 30, 8, 2 };

/* skin: the click, the punch and the crowd */
static const struct { uint8_t click, punch, crowd, style; } skins[TSKIN_COUNT] = {
    { SFX_THUD, SFX_POP2, 0, WIN_CUTE },
    { SFX_TIK, SFX_KAZOO, 1, WIN_CUTE },
    { SFX_TINK, SFX_FANFARE, 0, WIN_MINT },
    { SFX_THUD, SFX_PLUCK, 0, WIN_DARK },
};
static const char *const verdicts[4] = { "MISS", "GOOD!", "PERFECT!", "TOO EARLY!" };

int story_timing_judge(int err)
{
    if (err < 0) err = -err;
    if (err <= TT.perfect) return 2;
    if (err <= TT.good) return 1;
    return 0;
}

enum { TS_CARD, TS_ROUND, TS_REACT, TS_RESULT };
#define MAX_CLICKS 8
static struct {
    uint8_t pat, skin, drift, state, round, landed, verdict, pressed;
    uint8_t got[3];
    int16_t clicks[MAX_CLICKS];  /* frame of each click in the round (count-in included) */
    uint8_t nclicks, first;      /* first = index of the first pattern dot            */
    uint16_t t, st_t;
    uint32_t clock;
} T;
static int16_t timing_last;

int story_timing_result(void) { return timing_last; }

/* tests / tools: where the game is (TS_* 0 card .. 3 result), the round and
 * the frames until the punch (-1 outside a round) */
int story_timing_debug(int *round, int *to_punch)
{
    if (round) *round = T.round;
    if (to_punch) *to_punch = T.state == TS_ROUND ? T.clicks[T.nclicks - 1] - (int)T.t : -1;
    return T.state;
}

/* the round's click times: the count-in, then the pattern's gaps */
static void plan_round(void)
{
    const uint8_t *g = patterns[T.pat][T.round];
    int i, at = 0, n = 0;
    for (i = 0; i < TT.lead && n < MAX_CLICKS; i++) { T.clicks[n++] = (int16_t)at; at += TT.beat; }
    T.first = (uint8_t)n;
    T.clicks[n++] = (int16_t)at;
    for (i = 1; i <= g[0] && n < MAX_CLICKS; i++) {
        int k, beat = 0;
        for (k = 0; k < g[i]; k++) {
            int b = TT.beat;
            if (T.drift) {
                uint32_t r = story_rng(0x54494D00u | (uint32_t)(T.round << 4) | (uint32_t)(n * 3 + k));
                b += (int)(r % (2u * TT.drift_max + 1u)) - TT.drift_max;
            }
            beat += b;
        }
        at += beat;
        T.clicks[n++] = (int16_t)at;
    }
    T.nclicks = (uint8_t)n;
    T.t = 0;
    T.pressed = 0;
}

static void timing_enter(int arg)
{
    memset(&T, 0, sizeof(T));
    T.pat = (uint8_t)((arg & 0xFF) % TPAT_COUNT);
    T.skin = (uint8_t)(((arg >> 8) & 0x7F) % TSKIN_COUNT);
    T.drift = (uint8_t)(((arg >> 8) & TIMING_DRIFT) != 0);
    T.state = TS_CARD;
    timing_last = 0;
}

static void judge(int verdict)
{
    T.verdict = (uint8_t)verdict;
    T.got[T.round] = (uint8_t)(verdict == 1 || verdict == 2 ? verdict : 0);
    if (verdict == 1 || verdict == 2) {
        T.landed++;
        audio_sfx_synth(skins[T.skin].punch);
        if (verdict == 2) camfx_punch(0, -3);
    } else {
        audio_sfx(SFX_CANCEL);
    }
    T.state = TS_REACT;
    T.st_t = 0;
}

static void timing_update(void)
{
    T.clock++;
    T.st_t++;
    switch (T.state) {
    case TS_CARD:
        if ((btn_pressed(BTN_A) && T.st_t > TT.card) || T.st_t >= TT.card * 12) {
            audio_sfx(SFX_SELECT);
            T.round = 0;
            plan_round();
            T.state = TS_ROUND;
        }
        break;
    case TS_ROUND: {
        int i, punch = T.clicks[T.nclicks - 1];
        for (i = 0; i < T.nclicks - 1; i++)
            if (T.clicks[i] == (int)T.t) audio_sfx_synth(skins[T.skin].click);
        if (btn_pressed(BTN_A) && !T.pressed) {
            int err = (int)T.t - punch;
            T.pressed = 1;
            judge(err < -TT.good ? 3 : story_timing_judge(err));
            break;
        }
        if ((int)T.t > punch + TT.late) { judge(0); break; }
        T.t++;
        break; }
    case TS_REACT:
        if (T.st_t >= TT.react) {
            if (++T.round >= 3) {
                timing_last = T.landed;
                T.state = TS_RESULT;
                T.st_t = 0;
                audio_sfx(T.landed >= 2 ? SFX_HAPPY : SFX_CANCEL);
            } else {
                plan_round();
                T.state = TS_ROUND;
            }
        }
        break;
    default:
        if ((btn_pressed(BTN_A) || btn_pressed(BTN_B)) && T.st_t > TT.card) {
            if (scene_depth() > 1) scene_pop();
            else scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
        }
        break;
    }
}

/* ---- drawing -------------------------------------------------------------------------- */
static void draw_crowd(int sw, int sh, int cheer)
{
    int k, n = 7, y = sh - 30;
    for (k = 0; k < n; k++) {
        int x = sw / 2 - (n - 1) * 14 + k * 28, hop = cheer ? ui_wave((int)T.clock * 3 + k * 11, 12, 4) : 0;
        if (hop < 0) hop = -hop;
        ui_fill_circle(x, y - hop, 8, PAL_PAINT(k & 1 ? PAINT_BUBBLEGUM : PAINT_LEMON, 2));
        ui_fill_circle(x - 3, y - 2 - hop, 1, PAL_BLACK);
        ui_fill_circle(x + 3, y - 2 - hop, 1, PAL_BLACK);
        if (cheer) ui_fill_ellipse(x, y + 3 - hop, 3, 2, PAL_RAMP(RAMP_BLOOD, 1));
        else plat_rect(x - 2, y + 3, 5, 1, PAL_BLACK);
    }
}

static void draw_round(int sw, int sh)
{
    const UiStyle *st = ui_style(skins[T.skin].style);
    int n = T.nclicks, first = T.first, last = T.clicks[n - 1], span = last - T.clicks[first];
    int x0 = 24, x1 = sw - 24, y = sh / 2 - 4, i, bx, by, seg;
    if (span <= 0) span = 1;
    /* the dots: the pattern's beats, the punch is a star */
    plat_rect(x0, y + 6, x1 - x0, 2, st->outline);
    for (i = first; i < n; i++) {
        int x = x0 + (T.clicks[i] - T.clicks[first]) * (x1 - x0) / span;
        if (i == n - 1) ui_star(x, y, 9 + (T.state == TS_REACT && T.got[T.round] == 2 ? 3 : 0), PAL_UI_GOLD);
        else {
            ui_fill_circle(x, y + 2, 5, st->outline);
            ui_fill_circle(x, y + 2, 3, (int)T.t >= T.clicks[i] ? st->accent : PAL_WHITE);
        }
    }
    /* the ball: hops dot to dot (count-in: bounces on the first dot) */
    seg = first;
    while (seg < n - 1 && (int)T.t >= T.clicks[seg + 1]) seg++;
    if ((int)T.t < T.clicks[first]) {
        int k = (int)T.t % TT.beat;
        bx = x0;
        by = y - 6 - (k * (TT.beat - k) * 4) / TT.beat;
    } else if (seg >= n - 1) {
        bx = x1;
        by = y - 6;
    } else {
        int a = T.clicks[seg], b = T.clicks[seg + 1], u = (int)T.t - a, d = b - a > 0 ? b - a : 1;
        int xa = x0 + (a - T.clicks[first]) * (x1 - x0) / span, xb = x0 + (b - T.clicks[first]) * (x1 - x0) / span;
        bx = xa + (xb - xa) * u / d;
        by = y - 6 - (u * (d - u) * 64) / (d * d);
    }
    ui_fill_ellipse(bx, y + 7, 5 - (y - 6 - by) / 8, 2, st->outline);     /* its shadow on the line */
    ui_fill_circle(bx, by, 7, st->outline);
    ui_fill_circle(bx, by, 6, PAL_UI_GOLD);
    ui_fill_circle(bx - 2, by - 2, 2, PAL_WHITE);
}

static void timing_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), big = sh >= 200, k;
    int style = skins[T.skin].style;
    char buf[48];
    if (plat_draw_skipped()) return;
    ui_backdrop(style, T.clock);
    if (T.state == TS_CARD || T.state == TS_RESULT) {
        int w = sw - 40, h = sh / 2, x = (sw - w) / 2, y = (sh - h) / 2 - 8;
        if (!ui_window_pop(x, y, w, h, style, (int)T.st_t)) return;
        if (T.state == TS_CARD) {
            int tw = ui_text_big_width("Timing!", 2);
            ui_text_big(sw / 2 - tw / 2, y + 8, "Timing!", 2, ui_style(style)->title, ui_style(style)->outline);
            ui_text_center(sw / 2, y + 34, pattern_names[T.pat], ui_style(style)->text);
            ui_text_center(sw / 2, y + h - 18, T.drift ? "Listen! The beat drifts." : "Press A on the star!",
                           ui_style(style)->text_dim);
            ui_hint(STR_ICON_A " Start!", style);
        } else {
            int tw = ui_text_big_width(T.landed >= 2 ? "Nailed it!" : "Tough crowd", 2);
            ui_text_big(sw / 2 - tw / 2, y + 8, T.landed >= 2 ? "Nailed it!" : "Tough crowd", 2,
                        ui_style(style)->title, ui_style(style)->outline);
            for (k = 0; k < 3; k++)
                ui_star(sw / 2 - 28 + k * 28, y + h - 24, 9, k < T.landed ? PAL_UI_GOLD : ui_style(style)->fill_lo);
            ui_hint(STR_ICON_A " Done", style);
        }
        return;
    }
    ui_title(sw / 2, big ? 10 : 6, pattern_names[T.pat], style);
    for (k = 0; k < 3; k++)
        ui_star(sw - 14 - (2 - k) * 14, big ? 34 : 26, 5,
                k < T.round || (k == T.round && T.state == TS_REACT) ?
                (T.got[k] ? PAL_UI_GOLD : PAL_GRAY_D) : ui_style(style)->fill_lo);
    doomo_snprintf(buf, sizeof(buf), "Round %d", T.round + 1);
    ui_text(10, big ? 30 : 22, buf, ui_style(style)->text);
    draw_round(sw, sh);
    if (T.state == TS_REACT) {
        const char *v = verdicts[T.verdict];
        int s = ui_pop_scale((int)T.st_t), scale = s > 280 ? 3 : 2, tw = ui_text_big_width(v, scale);
        ui_text_big(sw / 2 - tw / 2, sh / 2 + 16, v, scale, T.got[T.round] ? PAL_UI_GOLD : PAL_WHITE,
                    ui_style(style)->outline);
    } else if (T.drift) {
        ui_text_center(sw / 2, sh / 2 + 20, "(listen...)", ui_style(style)->text_dim);
    }
    if (skins[T.skin].crowd) draw_crowd(sw, sh, T.state == TS_REACT && T.got[T.round]);
    ui_hint(STR_ICON_A " on the star", style);
}

const DoomoScene SCENE_STORY_TIMING = { "STORY_TIMING", timing_enter, NULL, timing_update, timing_draw, 0 };
