/* mg_wheel.c - the WHEEL OF FATE (owner: minigames).
 *
 * Wagerine the cacoblob bookie runs a big pastel prize wheel in a
 * star-spangled fortune tent. One spin per in-game day (the day of the
 * last spin + 1 is kept in PROG_FAIR_WHEEL_DAY): hold A to wind up, let
 * go to spin. The flapper clacks over the pegs, the wheel slows, and you
 * win H$, treats, a plushie or holy water... or a CURSE (the Tax Imp, a
 * rain of brimstone bagels, a spooked neighbour). The jackpot and being
 * cursed both earn trophies. The hub of the wheel is Wagerine's eyeball,
 * and it watches the pointer.
 *
 * The wheel is a real pie: per scanline the ten wedge boundaries are
 * intersected with the row (one 16.16 cotangent per ray per frame) and
 * the row is emitted as a handful of coloured spans.
 *
 * Scene arg: 16 = spin even if already spun today, 32 = the bot spins
 * (runner scripts), 64 = premade cast on an empty island.
 */
#include "mg.h"
#include "doomo/plat_ext.h"                 /* plat_strip (S2-3: the POWER bar rows of a held frame) */

enum { WH_INTRO, WH_READY, WH_CHARGE, WH_SPIN, WH_RESULT, WH_DONE };

typedef struct Wheel {
    int32_t ang16, vel16;          /* angle in 1/16 of DOOMO_ANG units */
    uint8_t st, power, wedge, last_peg, flap, autoplay, force, curse, prev_song;
    uint16_t t;
    int16_t money_delta;
    /* S2-3 (the held frame, wh_draw_body): what the last full frame drew, so a held tick repaints only what
     * changed since: its tick count (t), the speech line and host face, and whether the POWER bar is up */
    int32_t full_t;
    const char *held_line;
    uint8_t held_expr, bar_up, bar_saved;
    uint8_t bub_n, face_n, bar_n;  /* held ticks still to repaint it: 2 (both pages) after a change */
} Wheel;
MG_ARENA_FITS(Wheel);
#define S (MG_ARENA(Wheel))

const char *const mg_wheel_names[MG_WHEEL_WEDGES] = {
    "50 H$", "3 Cupcakes", "CURSE: Tax Imp", "Plushie", "200 H$",
    "CURSE: Bagel Rain", "2 Energy Drinks", "JACKPOT! 777 H$", "CURSE: Spooked", "Holy Water"
};
static const char *const wedge_lines[MG_WHEEL_WEDGES] = {
    "Fifty Hell-Bucks. Don't spend it all on vapes.",
    "Three cupcakes! Nana Pudding approves.",
    "A Tax Imp nibbles 10% of your wallet. Rules are rules.",
    "A squishy bunny plushie. Hug responsibly.",
    "Two hundred H$! Wagerine looks personally offended.",
    "Three brimstone bagels. Enjoy. You have to.",
    "Two Hellraiser Extremes. Fizzy!",
    "JACKPOT!!! The wheel is NOT rigged. Probably.",
    "A spooky chill goes looking for a neighbour...",
    "A bottle of Holy Water, blessed by Father Fuzz."
};
static const uint8_t wedge_icon[MG_WHEEL_WEDGES] = {
    ICON_COIN, ICON_FOOD, ICON_SKULL, ICON_HEART, ICON_COIN,
    ICON_SKULL, ICON_BOLT, ICON_STAR, ICON_SKULL, ICON_CROSS
};
static const uint8_t wedge_paint[MG_WHEEL_WEDGES] = {
    PAINT_LEMON, PAINT_BUBBLEGUM, PAINT_JET, PAINT_MINT, PAINT_SKY,
    PAINT_JET, PAINT_TANGERINE, PAINT_HONEY, PAINT_JET, PAINT_LAVENDER
};

int mg_wheel_is_curse(int w)
{
    return w == 2 || w == 5 || w == 8;
}

int mg_wheel_ready(void)
{
    return progress_get(PROG_FAIR_WHEEL_DAY) != doomo_town.day + 1;
}

int mg_wheel_apply(int w)
{
    int before = (int)doomo_town.money, delta;
    switch (w) {
    case 0: doomo_town.money += 50; progress_add(PROG_MONEY_EARNED, 50); break;
    case 1: inv_add(ITEM_CUPCAKE, 3); break;
    case 2: {
        int tax = (int)(doomo_town.money / 10);
        if (tax > 100) tax = 100;
        doomo_town.money -= (uint32_t)tax;
        break;
    }
    case 3: inv_add(ITEM_PLUSHIE, 1); break;
    case 4: doomo_town.money += 200; progress_add(PROG_MONEY_EARNED, 200); break;
    case 5: inv_add(ITEM_BRIMSTONE_BAGEL, 3); break;
    case 6: inv_add(ITEM_ENERGY_DRINK, 2); break;
    case 7:
        doomo_town.money += 777;
        progress_add(PROG_MONEY_EARNED, 777);
        mg_trophy(MG_UL_JACKPOT);
        break;
    case 8: {
        int i, start = (int)(doomo_hash32(doomo_town.day * 31u + doomo_town.seed) % DOOMO_DISTRICT_SIZE), hit = -1;
        for (i = 0; i < DOOMO_DISTRICT_SIZE; i++) {
            int id = (start + i) % DOOMO_DISTRICT_SIZE;
            if (town_alive(id) && town_res(id)->status == RES_ALIVE) { hit = id; break; }
        }
        if (hit >= 0) {
            sim_corrupt(hit, 10);
            sim_news_rec(NT_WHEEL_SPOOKED, 0, NEWS_IDS(hit, NEWS_NONE, NEWS_NONE));
        } else {
            int fine = doomo_town.money >= 25 ? 25 : (int)doomo_town.money;
            doomo_town.money -= (uint32_t)fine;
        }
        break;
    }
    case 9: inv_add(ITEM_HOLY_WATER, 1); break;
    default: return 0;
    }
    if (mg_wheel_is_curse(w)) mg_trophy(MG_UL_CURSED);
    delta = (int)doomo_town.money - before;
    return delta;
}

static int radius(void) { return mg_big() ? MG_WHEEL_R_BIG : MG_WHEEL_R_SMALL; }
static int wcx(void) { return mg_w() - radius() - (mg_big() ? 34 : 14); }
static int wcy(void) { return mg_h() / 2 + (mg_big() ? 10 : 8); }

static void wh_enter(int arg)
{
    memset(&mg, 0, sizeof(mg));
    if ((arg & MG_ARG_DEMO) && town_count(-1) == 0)
        town_new(doomo_town.mode, doomo_town.seed ? doomo_town.seed : 1234u, TOWN_POP_PREMADE);
    mg_scratch_claim();
    memset(mg_state(), 0, MG_ARENA_BYTES);
    mg_fx_clear();
    S->force = (arg & MG_ARG_AUTOSTART) != 0;
    S->autoplay = (arg & MG_ARG_AUTOPLAY) != 0;
    S->st = (mg_wheel_ready() || S->force) ? WH_INTRO : WH_DONE;
    S->ang16 = (int32_t)(doomo_hash32(doomo_frame_count()) & 0xFFFF);
    S->wedge = 0xFF;
    S->prev_song = (uint8_t)audio_music_current();
    audio_music(SONG_FESTIVAL);
}

static int pointer_wedge(void)
{
    /* the pointer sits at the top (angle 3072); wedge k spans
     * [rot + k*W, rot + (k+1)*W) */
    int rot = (int)((S->ang16 >> 4) & DOOMO_ANG_MASK);
    int a = (3072 - rot) & DOOMO_ANG_MASK;
    return a * MG_WHEEL_WEDGES / DOOMO_ANG_FULL;
}

static void wh_update(void)
{
    if (!scene_scratch_mine(MG_SCRATCH_TAG)) {      /* cannot happen (mg.h) */
        plat_log("wheel: scene scratch lost, closing");
        if (scene_depth() > 1) scene_pop(); else scene_set(&SCENE_TITLE, 0);
        return;
    }
    S->t++;
    if (mg.shake) { mg.ox = (int8_t)rng_between(&doomo_rng, -2, 2); mg.oy = (int8_t)rng_between(&doomo_rng, -2, 2); mg.shake--; }
    else mg.ox = mg.oy = 0;
    mg_fx_update();
    if (S->flap) S->flap--;
    switch (S->st) {
    case WH_INTRO:
        S->ang16 += 24;
        if (S->t > 20 && (btn_pressed(BTN_A) || (S->autoplay && S->t > 30))) { S->st = WH_READY; S->t = 0; }
        if (btn_pressed(BTN_B)) scene_pop();
        break;
    case WH_READY:
        S->ang16 += 24;
        if (btn_down(BTN_A) || S->autoplay) { S->st = WH_CHARGE; S->power = 0; S->t = 0; }
        if (btn_pressed(BTN_B)) scene_pop();
        break;
    case WH_CHARGE:
        S->ang16 -= S->power / 4;           /* wind back */
        if (S->power < 100) S->power = (uint8_t)(S->power + 3);
        if ((S->autoplay && S->t > 24) || (!S->autoplay && !btn_down(BTN_A))) {
            S->vel16 = 900 + S->power * 22 + rng_range(&doomo_rng, 400);
            S->st = WH_SPIN;
            S->t = 0;
            progress_set(PROG_FAIR_WHEEL_DAY, doomo_town.day + 1);   /* the day's spin is used */
            audio_sfx(SFX_SELECT);
        }
        break;
    case WH_SPIN: {
        int peg;
        S->ang16 += S->vel16;
        S->vel16 = S->vel16 * 252 / 256 - 1;
        peg = pointer_wedge();
        if (peg != S->last_peg) { S->last_peg = (uint8_t)peg; S->flap = 4; audio_sfx(SFX_BLIP); }
        if (S->vel16 <= 0) {
            S->vel16 = 0;
            S->wedge = (uint8_t)pointer_wedge();
            S->money_delta = (int16_t)mg_wheel_apply(S->wedge);
            S->curse = (uint8_t)mg_wheel_is_curse(S->wedge);
            S->st = WH_RESULT;
            S->t = 0;
            if (S->curse) {
                mg_burst(MGFX_PUFF, wcx(), wcy(), 10, 40, PAL_RAMP(RAMP_VAPE, 3));
                mg_shake_add(8);
                mg_flash(PAL_UI_PURPLE, 4);
                audio_sfx(SFX_POSSESS);
            } else {
                mg_confetti_rain(S->wedge == 7 ? 40 : 20);
                mg_burst(MGFX_STAR, wcx(), wcy() - radius(), 10, 40, PAL_UI_GOLD);
                audio_sfx(S->wedge == 7 ? SFX_EVOLVE : SFX_HAPPY);
            }
        }
        break;
    }
    case WH_RESULT:
        if (S->t > 30 && (btn_pressed(BTN_A | BTN_B))) { S->st = WH_DONE; S->t = 0; }
        break;
    case WH_DONE:
        S->ang16 += 8;
        if (S->t > 10 && btn_pressed(BTN_A | BTN_B)) { audio_sfx(SFX_CANCEL); scene_pop(); }
        break;
    default:
        break;
    }
}

/* ---- drawing ------------------------------------------------------------------ */
static uint8_t wedge_col(int k, int shade)
{
    if (wedge_paint[k] == PAINT_JET) return shade ? PAL_RAMP(RAMP_VAPE, 2) : PAL_RAMP(RAMP_VAPE, 1);
    return PAL_PAINT(wedge_paint[k], shade ? 2 : 1);
}

/* S2-3: fx_atan2 from the generated octant table (the same angle for every (x, y): it folds the point into
 * the first octant and places it back exactly as fx_atan2 does; test_minigames checks the whole disc). Points
 * outside the table (none today: the pie's radius is at most MG_WHEEL_ATAN_MAX) take fx_atan2. */
int mg_wheel_atan2(int y, int x)
{
    int ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, a;
    if (ax > MG_WHEEL_ATAN_MAX || ay > MG_WHEEL_ATAN_MAX) return fx_atan2(y, x);
    if (ay <= ax) a = mg_wheel_atan_oct[ax * (ax + 1) / 2 + ay];
    else          a = 1024 - mg_wheel_atan_oct[ay * (ay + 1) / 2 + ax];
    if (x >= 0 && y >= 0) return a & DOOMO_ANG_MASK;
    if (x < 0 && y >= 0)  return (2048 - a) & DOOMO_ANG_MASK;
    if (x < 0)            return (2048 + a) & DOOMO_ANG_MASK;
    return (4096 - a) & DOOMO_ANG_MASK;
}

/* The rim: ui_fill_circle(cx + 2, cy + 3, R + 6, shadow), (cx, cy, R + 6, candy), (cx, cy, R + 4, gold), all but
 * a ring of each hidden under the next and the pie. S2-3: the same pixels from the generated rect list of the
 * rings (tools/gen_wheel.py), instead of three discs and their row loops */
static void rim(int cx, int cy, int R, int held)
{
    const MgRimRect *r = R == MG_WHEEL_R_BIG ? mg_wheel_rim_big : mg_wheel_rim_small;
    uint8_t col[3];
    col[0] = PAL_RAMP(RAMP_STONE, 0); col[1] = PAL_RAMP(RAMP_CANDY, 2); col[2] = PAL_UI_GOLD;
    for (; r->w; r++)                               /* (held: the shadow, outside the rest, is there already) */
        if (r->hl >= 64 || !held) plat_rect(cx + r->x, cy + r->y, r->w, r->hl & 63, col[r->hl >> 6]);
}

static void pie(int cx, int cy, int R, int rot)
{
    const uint8_t *hws = R == MG_WHEEL_R_BIG + 1 ? mg_wheel_pie_hw_big : mg_wheel_pie_hw_small;
    int32_t cot[MG_WHEEL_WEDGES];
    int8_t up[MG_WHEEL_WEDGES];
    uint8_t ord[2][MG_WHEEL_WEDGES], nord[2] = { 0, 0 };
    int k, y, shade_rows = 0;
    for (k = 0; k < MG_WHEEL_WEDGES; k++) {
        int a = (rot + k * DOOMO_ANG_FULL / MG_WHEEL_WEDGES) & DOOMO_ANG_MASK;
        fix16 s = fx_sin(a), c = fx_cos(a);
        up[k] = (int8_t)(s < 0 ? -1 : s > 0 ? 1 : 0);
        if (s > -64 && s < 64) cot[k] = c > 0 ? (200 << 16) : -(200 << 16);
        else cot[k] = (int32_t)FIXDIV(c, s);
        if (cot[k] > (200 << 16)) cot[k] = 200 << 16;
        if (cot[k] < -(200 << 16)) cot[k] = -(200 << 16);
    }
    /* S2-3: a ray's crossing x = (cot >> 8) * y >> 8 is monotonic in cot for a fixed y (non-decreasing for y > 0,
     * non-increasing for y < 0), so each half's crossings come out sorted from one order of its rays made here
     * (ascending cot for the lower half, descending for the upper) instead of an insertion sort every row */
    for (k = 0; k < MG_WHEEL_WEDGES; k++) {
        int h = up[k] == 1 ? 1 : up[k] == -1 ? 0 : -1, j;
        if (h < 0) continue;
        for (j = nord[h]; j > 0; j--) {
            int32_t o = cot[ord[h][j - 1]];
            if (h ? o <= cot[k] : o >= cot[k]) break;
            ord[h][j] = ord[h][j - 1];
        }
        ord[h][j] = (uint8_t)k;
        nord[h]++;
    }
    while ((shade_rows * 3) < R) shade_rows++;      /* rows y + R < shade_rows: ((y + R) * 3 / R) == 0 */
    for (y = -R; y <= R; y++) {
        int hw = hws[y < 0 ? -y : y], xs[MG_WHEEL_WEDGES + 2], n = 0, i;
        int h = y < 0 ? 0 : 1, shade = (y + R) < shade_rows;
        xs[n++] = -hw;
        for (i = 0; i < nord[h]; i++) {
            int x = (int)((cot[ord[h][i]] >> 8) * y >> 8);
            if (x > -hw && x < hw) xs[n++] = x;
        }
        xs[n++] = hw + 1;
        for (i = 0; i < n - 1; i++) {
            int x0 = xs[i], x1 = xs[i + 1], mx = (x0 + x1) / 2, wdg, a;
            if (x1 <= x0) continue;
            a = (mg_wheel_atan2(y, mx) - rot) & DOOMO_ANG_MASK;
            wdg = a * MG_WHEEL_WEDGES / DOOMO_ANG_FULL;
            plat_rect(cx + x0, cy + y, x1 - x0, 1, wedge_col(wdg, shade));
        }
    }
}

static void wheel(int t, int held)
{
    int cx = wcx() + mg.ox, cy = wcy() + mg.oy, R = radius(), k;
    int rot = (int)((S->ang16 >> 4) & DOOMO_ANG_MASK);
    /* stand (S2-3: on a held frame it is there already, and the rim and pie cover all of it they cover) */
    if (!held) {
        plat_rect(cx - 4, cy, 9, mg_h() - cy, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(cx - 3, cy, 3, mg_h() - cy, PAL_RAMP(RAMP_WOOD, 4));
        ui_round_rect(cx - 26, mg_h() - 12, 52, 12, 4, PAL_RAMP(RAMP_WOOD, 1));
    }
    /* rim + bulbs */
    rim(cx, cy, R, held);
    pie(cx, cy, R + 1, rot);
    for (k = 0; k < 20; k++) {
        int a = (rot / 2 + k * 4096 / 20) & DOOMO_ANG_MASK;
        int lit = ((k + t / 4) % 4) == 0 || S->st == WH_RESULT;
        plat_rect(cx + FIX2INT(fx_cos(a) * (R + 3)) - 1, cy + FIX2INT(fx_sin(a) * (R + 3)) - 1, 2, 2,
                  lit && ((t / 3) & 1 || S->st != WH_RESULT) ? PAL_WHITE : PAL_RAMP(RAMP_CANDY, 4));
    }
    /* pegs, icons */
    for (k = 0; k < MG_WHEEL_WEDGES; k++) {
        int a = (rot + k * DOOMO_ANG_FULL / MG_WHEEL_WEDGES) & DOOMO_ANG_MASK;
        int ac = (a + DOOMO_ANG_FULL / MG_WHEEL_WEDGES / 2) & DOOMO_ANG_MASK;
        char ic[2] = { (char)wedge_icon[k], 0 };
        int ix = cx + FIX2INT(fx_cos(ac) * (R * 2 / 3)), iy = cy + FIX2INT(fx_sin(ac) * (R * 2 / 3));
        plat_rect(cx + FIX2INT(fx_cos(a) * (R - 1)) - 1, cy + FIX2INT(fx_sin(a) * (R - 1)) - 1, 3, 3, PAL_WHITE);
        plat_line(cx + FIX2INT(fx_cos(a) * 8), cy + FIX2INT(fx_sin(a) * 8),
                  cx + FIX2INT(fx_cos(a) * (R - 1)), cy + FIX2INT(fx_sin(a) * (R - 1)), PAL_RAMP(RAMP_STONE, 0));
        if (S->st == WH_RESULT && k == S->wedge && (t / 4) & 1)
            ui_fill_circle(ix, iy, mg_big() ? 9 : 7, PAL_WHITE);
        ui_text_outline(ix - 2, iy - 3, ic, mg_wheel_is_curse(k) ? PAL_PAINT(PAINT_LIME, 2) : PAL_WHITE,
                        PAL_RAMP(RAMP_STONE, 0));
    }
    /* the hub: Wagerine's watchful eyeball */
    ui_fill_circle(cx, cy, mg_big() ? 12 : 9, PAL_RAMP(RAMP_STONE, 0));
    ui_fill_circle(cx, cy, mg_big() ? 11 : 8, PAL_WHITE);
    {
        int er = mg_big() ? 5 : 4, look = S->st == WH_SPIN ? ui_wave(t * 4, 20, 2) : 0;
        ui_fill_circle(cx + look, cy - 2, er, PAL_PAINT(PAINT_LIME, 1));
        ui_fill_circle(cx + look, cy - 2, er / 2, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(cx + look - 1, cy - 4, 1, 1, PAL_WHITE);
        if ((t % 90) < 4) ui_fill_circle(cx, cy, mg_big() ? 10 : 7, PAL_PAINT(PAINT_LIME, 0));
    }
    /* the pointer + flapper at the top */
    {
        int px = cx, py = cy - R - 8, bend = S->flap ? (S->flap > 2 ? 3 : 1) : 0;
        ui_heart(px, py, 6, PAL_PAINT(PAINT_CHERRY, 1));
        plat_line(px, py + 4, px - bend, py + 13, PAL_RAMP(RAMP_STONE, 0));
        plat_line(px + 1, py + 4, px + 1 - bend, py + 13, PAL_PAINT(PAINT_CHERRY, 0));
    }
}

static void tent_bg(int t)
{
    int W = mg_w(), H = mg_h(), i;
    plat_rect(0, 0, W, H, PAL_PAINT(PAINT_NAVY, 1));
    for (i = 0; i < W; i += 24) {
        plat_rect(i, 0, 12, H, PAL_PAINT(PAINT_NAVY, 0));
        plat_rect(i + 12, 0, 1, H, PAL_PAINT(PAINT_NAVY, 2));
    }
    for (i = 0; i < 18; i++) {
        uint32_t h = doomo_hash32((uint32_t)i * 7919u + 77u);
        int ph = (int)((t / 2 + (h >> 20)) & 31);
        if (ph < 16) ui_sparkle((int)(h % (uint32_t)W), (int)((h >> 8) % (uint32_t)H), ph, PAL_UI_GOLD);
        else plat_rect((int)(h % (uint32_t)W), (int)((h >> 8) % (uint32_t)H), 1, 1, PAL_PAINT(PAINT_LEMON, 2));
    }
    mg_tent(0, 14, PAL_PAINT(PAINT_GRAPE, 1), PAL_PAINT(PAINT_VAPE_PURPLE, 0), PAL_UI_GOLD, t);
}

static const char *wh_line(void)
{
    switch (S->st) {
    case WH_INTRO: return "One spin a day, sugar. Prizes... or CURSES. Feeling lucky?";
    case WH_READY: return "Hold " STR_ICON_A " to wind it up, let go to spin!";
    case WH_CHARGE: return "Wind it up... wiiiind it...";
    case WH_SPIN: return "Round and round she goes...";
    case WH_RESULT: return wedge_lines[S->wedge];
    default: return mg_wheel_ready() ? "Spin again tomorrow? Or now. Whatever." : "That's your spin for today. Come back tomorrow, sugar!";
    }
}

/* the speech bubble left of the wheel (opaque over the same rect every frame) */
static void wh_bubble(const char *line)
{
    int big = mg_big(), bx = big ? 12 : 6, hy = big ? 40 : 26;
    int bw = wcx() - radius() - 14 - bx, by = hy + (big ? 74 : 64), bh = big ? 60 : 46, i;
    ui_round_rect(bx + 2, by + 2, bw, bh, 6, PAL_RAMP(RAMP_STONE, 0));
    ui_round_rect(bx, by, bw, bh, 6, PAL_RAMP(RAMP_STONE, 2));
    ui_round_rect(bx + 1, by + 1, bw - 2, bh - 2, 5, PAL_WHITE);
    for (i = 0; i < 5; i++) plat_rect(bx + 20 + i, by - 5 + i, 6 - i, 1, PAL_WHITE);
    ui_text_wrap(bx + 5, by + 4, bw - 10, line, PAL_RAMP(RAMP_STONE, 0), 1);
}

/* the POWER bar while winding up: rows [H - 26, H - 16) */
#define WH_BAR_Y(H) ((H) - 26)
#define WH_BAR_H 10
static void wh_power_bar(void)
{
    int big = mg_big(), bx = big ? 12 : 6, pw = big ? 90 : 60, H = mg_h();
    ui_round_rect(bx, WH_BAR_Y(H), pw + 4, 10, 4, PAL_RAMP(RAMP_STONE, 0));
    ui_bar(bx + 2, H - 24, pw, 6, S->power, 100, S->power > 80 ? PAL_UI_GOLD : PAL_PAINT(PAINT_MINT, 1));
    mg_text(bx + pw + 8, WH_BAR_Y(H), "POWER", PAL_WHITE);
}

static int wh_face(void) { return S->st == WH_SPIN ? EXPR_TALK : EXPR_HAPPY; }

/* S2-3: the held frame (ui_modal_hold, GBA; docs/GBA.md "Speed wave 2: S2-3"). Outside the result card (its
 * confetti, window, flash and shake), what changes from tick to tick is the wheel: everything it draws lies inside
 * its rim's disc (the bulbs, pegs, icons, the eyeball, the flapper), which the rim and the pie repaint opaquely.
 * So after one full frame the platform keeps it in both pages and a tick draws the wheel only (~27 ms instead of
 * ~55), plus what changed with the state: the speech line, Wagerine's face (drawn at the held frame's tick, so her
 * frame sits where it is) and the POWER bar (a clean copy of its rows is kept, plat_strip, to take it away).
 * Pixel-changing while held: the tent's sway, the sparkles' twinkle and the host frame's bob stand still. */
static const uint8_t wh_hold_key;

static int wh_hold_ok(void)
{
    if (S->st == WH_RESULT || mg.shake || mg.ox || mg.oy || mg.flash_t || mg_fx_live()) return 0;
#if !defined(DOOMO_PLATFORM_PS1)
    if (portrait_job_on()) return 0;                /* a stand-in face may still change */
#endif
    return 1;
}

static int wh_draw_held(int t)
{
    int R = radius(), H = mg_h();
    const char *line = wh_line();
    if (!ui_modal_hold(&wh_hold_key, wh_hold_ok(), wcx() - R - 8, wcy() - R - 14, 2 * R + 17, 2 * R + 24))
        return 0;
    /* a change is painted on two held ticks: the two pages flip, each needs it */
    if (S->bar_up && S->st != WH_CHARGE) { S->bar_up = 0; S->bar_n = 2; }
    if (line != S->held_line) { S->held_line = line; S->bub_n = 2; }
    if (wh_face() != S->held_expr) { S->held_expr = (uint8_t)wh_face(); S->face_n = 2; }
    if (S->bar_n) {                                 /* the POWER bar goes: its rows' clean copy back */
        if (!plat_strip(PLAT_STRIP_RESTORE, WH_BAR_Y(H), WH_BAR_H)) {
            ui_modal_hold_end();                    /* the copy is gone: a full frame */
            ui_modal_drop();
            return 0;
        }
        S->bar_n--;
    }
    /* a new line first, then the new face (one of them a tick, so the tick stays under 33 ms) */
    if (S->bub_n) {
        wh_bubble(line);
        S->bub_n--;
    } else if (S->face_n) {
        mg_host_frame(mg_big() ? 12 : 6, mg_big() ? 40 : 26, MG_WHEEL_HOST, wh_face(), S->full_t);
        S->face_n--;
    }
    wheel(t, 1);
    if (S->st == WH_CHARGE) {
        if (!S->bar_saved) {                        /* no clean copy of its rows: draw it, hold no longer */
            ui_modal_hold_end();
            ui_modal_drop();
            return 0;
        }
        wh_power_bar();
        S->bar_up = 1;
    }
    ui_modal_hold_end();
    return 1;
}

static void wh_draw_body(void)
{
    int W = mg_w(), H = mg_h(), t = S->t + (int)doomo_frame_count(), big = mg_big();
    int hx = big ? 12 : 6, hy = big ? 40 : 26;
    const char *line = wh_line();
    char buf[64];
    if (plat_draw_skipped()) return;
    /* S2-3: Wagerine talks while the wheel spins: her talking face is drawn ahead, in the idle time of the frames
     * before (a 45 ms render in the first spin tick otherwise; the same picture) */
    if (S->st < WH_SPIN && mg_host_alive(MG_WHEEL_HOST)) portrait_prefetch(MG_WHEEL_HOST, EXPR_TALK);
    if (wh_draw_held(t)) return;
    tent_bg(t);
    mg_textc(hx + 28 + (big ? 38 : 30), big ? 22 : 16, "WHEEL OF FATE", PAL_UI_GOLD);
    mg_host_frame(hx, hy, MG_WHEEL_HOST, wh_face(), t);
    wheel(t, 0);
    wh_bubble(line);
    /* (S2-3) what a held tick needs from this frame */
    S->full_t = t;
    S->held_line = line;
    S->held_expr = (uint8_t)wh_face();
    S->bar_saved = (uint8_t)plat_strip(PLAT_STRIP_SAVE, WH_BAR_Y(H), WH_BAR_H);
    S->bar_up = S->st == WH_CHARGE;
    S->bub_n = S->face_n = S->bar_n = 0;
    if (S->st == WH_CHARGE) wh_power_bar();
    mg_fx_draw();
    if (S->st == WH_RESULT && S->t > 8) {
        int w = big ? 200 : 160, h = 44, x = (W - w) / 2, y = H - h - (big ? 16 : 6);
        if (ui_window_pop(x, y, w, h, S->curse ? WIN_HELL : WIN_CREAM, S->t - 8)) {
            ui_title(W / 2, y - 6, S->curse ? "CURSED!" : "PRIZE!", S->curse ? WIN_HELL : WIN_CUTE);
            mg_textc(W / 2, y + 12, mg_wheel_names[S->wedge], S->curse ? PAL_PAINT(PAINT_LIME, 2) : PAL_UI_GOLD);
            if (S->money_delta) {
                doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN " %s%d H$", S->money_delta > 0 ? "+" : "", S->money_delta);
                ui_text_center(W / 2, y + 26, buf, ui_style_text(S->curse ? WIN_HELL : WIN_CREAM));
            } else {
                ui_text_center(W / 2, y + 26, "Added to your bag", ui_style_text(S->curse ? WIN_HELL : WIN_CREAM));
            }
        }
    }
    if (!opt_get(OPT_REDUCE_FLASH) && mg.flash_t) { plat_checker(0, 0, W, H, mg.flash_c); mg.flash_t--; }
    ui_hint(S->st == WH_DONE || S->st == WH_RESULT ? STR_ICON_A " OK" :
            STR_ICON_A " Spin  " STR_ICON_B " Leave", WIN_CUTE);
    ui_modal_no_band();                             /* the band copy is the POWER bar's (plat_strip has one) */
    ui_modal_full(&wh_hold_key, wh_hold_ok());
}

static void wh_draw(void)
{
    if (!scene_scratch_mine(MG_SCRATCH_TAG)) { ui_modal_hold(&wh_hold_key, 0, 0, 0, 0, 0); plat_clear(PAL_UI_PINK); return; }
    wh_draw_body();
}

static void wh_leave(void)
{
    if (!scene_scratch_mine(MG_SCRATCH_TAG)) return;
    mg_fx_clear();
    audio_music(S->prev_song);
}

const DoomoScene SCENE_MG_WHEEL = { "MG_WHEEL", wh_enter, wh_leave, wh_update, wh_draw, 0 };
