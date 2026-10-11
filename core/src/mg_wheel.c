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

enum { WH_INTRO, WH_READY, WH_CHARGE, WH_SPIN, WH_RESULT, WH_DONE };

typedef struct Wheel {
    int32_t ang16, vel16;          /* angle in 1/16 of DOOMO_ANG units */
    uint8_t st, power, wedge, last_peg, flap, autoplay, force, curse;
    uint16_t t;
    int16_t money_delta;
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
            char buf[NEWS_LEN];
            sim_corrupt(hit, 10);
            doomo_snprintf(buf, sizeof(buf), "%s got spooked by the Wheel of Fate!", town_res(hit)->name);
            sim_news_push(buf);
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

static int radius(void) { return mg_big() ? 72 : 50; }
static int wcx(void) { return mg_w() - radius() - (mg_big() ? 34 : 14); }
static int wcy(void) { return mg_h() / 2 + (mg_big() ? 10 : 8); }

static void wh_enter(int arg)
{
    memset(&mg, 0, sizeof(mg));
    if ((arg & MG_ARG_DEMO) && town_count(-1) == 0)
        town_new(doomo_town.mode, doomo_town.seed ? doomo_town.seed : 1234u, TOWN_POP_PREMADE);
    memset(mg_arena, 0, sizeof(mg_arena));
    mg_fx_clear();
    S->force = (arg & MG_ARG_AUTOSTART) != 0;
    S->autoplay = (arg & MG_ARG_AUTOPLAY) != 0;
    S->st = (mg_wheel_ready() || S->force) ? WH_INTRO : WH_DONE;
    S->ang16 = (int32_t)(doomo_hash32(doomo_frame_count()) & 0xFFFF);
    S->wedge = 0xFF;
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

static void pie(int cx, int cy, int R, int rot)
{
    int32_t cot[MG_WHEEL_WEDGES];
    int8_t up[MG_WHEEL_WEDGES];
    int k, y;
    for (k = 0; k < MG_WHEEL_WEDGES; k++) {
        int a = (rot + k * DOOMO_ANG_FULL / MG_WHEEL_WEDGES) & DOOMO_ANG_MASK;
        fix16 s = fx_sin(a), c = fx_cos(a);
        up[k] = (int8_t)(s < 0 ? -1 : s > 0 ? 1 : 0);
        if (s > -64 && s < 64) cot[k] = c > 0 ? (200 << 16) : -(200 << 16);
        else cot[k] = (int32_t)FIXDIV(c, s);
        if (cot[k] > (200 << 16)) cot[k] = 200 << 16;
        if (cot[k] < -(200 << 16)) cot[k] = -(200 << 16);
    }
    for (y = -R; y <= R; y++) {
        int hw = (int)fx_isqrt((uint32_t)(R * R - y * y)), xs[MG_WHEEL_WEDGES + 2], n = 0, i, j;
        int side = y < 0 ? -1 : 1;
        if (y == 0) side = 1;
        xs[n++] = -hw;
        for (k = 0; k < MG_WHEEL_WEDGES; k++) {
            if (up[k] != side) continue;
            {
                int x = (int)((cot[k] >> 8) * y >> 8);
                if (x > -hw && x < hw) xs[n++] = x;
            }
        }
        xs[n++] = hw + 1;
        for (i = 2; i < n - 1; i++)                 /* insertion sort */
            for (j = i; j > 1 && xs[j - 1] > xs[j]; j--) { int tmp = xs[j]; xs[j] = xs[j - 1]; xs[j - 1] = tmp; }
        for (i = 0; i < n - 1; i++) {
            int x0 = xs[i], x1 = xs[i + 1], mx = (x0 + x1) / 2, wdg, a;
            if (x1 <= x0) continue;
            a = (fx_atan2(y, mx) - rot) & DOOMO_ANG_MASK;
            wdg = a * MG_WHEEL_WEDGES / DOOMO_ANG_FULL;
            plat_rect(cx + x0, cy + y, x1 - x0, 1, wedge_col(wdg, ((y + R) * 3 / R) == 0));
        }
    }
}

static void wheel(int t)
{
    int cx = wcx() + mg.ox, cy = wcy() + mg.oy, R = radius(), k;
    int rot = (int)((S->ang16 >> 4) & DOOMO_ANG_MASK);
    /* stand */
    plat_rect(cx - 4, cy, 9, mg_h() - cy, PAL_RAMP(RAMP_WOOD, 2));
    plat_rect(cx - 3, cy, 3, mg_h() - cy, PAL_RAMP(RAMP_WOOD, 4));
    ui_round_rect(cx - 26, mg_h() - 12, 52, 12, 4, PAL_RAMP(RAMP_WOOD, 1));
    /* rim + bulbs */
    ui_fill_circle(cx + 2, cy + 3, R + 6, PAL_RAMP(RAMP_STONE, 0));
    ui_fill_circle(cx, cy, R + 6, PAL_RAMP(RAMP_CANDY, 2));
    ui_fill_circle(cx, cy, R + 4, PAL_UI_GOLD);
    ui_fill_circle(cx, cy, R + 1, PAL_RAMP(RAMP_WOOD, 1));
    pie(cx, cy, R, rot);
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

static void wh_draw(void)
{
    int W = mg_w(), H = mg_h(), t = S->t + (int)doomo_frame_count(), big = mg_big();
    int hx = big ? 12 : 6, hy = big ? 40 : 26, bx, bw;
    const char *line;
    char buf[64];
    if (plat_draw_skipped()) return;
    tent_bg(t);
    mg_textc(hx + 28 + (big ? 38 : 30), big ? 22 : 16, "WHEEL OF FATE", PAL_UI_GOLD);
    mg_host_frame(hx, hy, MG_WHEEL_HOST, S->st == WH_SPIN ? EXPR_TALK : EXPR_HAPPY, t);
    wheel(t);
    switch (S->st) {
    case WH_INTRO: line = "One spin a day, sugar. Prizes... or CURSES. Feeling lucky?"; break;
    case WH_READY: line = "Hold " STR_ICON_A " to wind it up, let go to spin!"; break;
    case WH_CHARGE: line = "Wind it up... wiiiind it..."; break;
    case WH_SPIN: line = "Round and round she goes..."; break;
    case WH_RESULT: line = wedge_lines[S->wedge]; break;
    default: line = mg_wheel_ready() ? "Spin again tomorrow? Or now. Whatever." : "That's your spin for today. Come back tomorrow, sugar!"; break;
    }
    bx = hx;
    bw = wcx() - radius() - 14 - bx;
    {
        int by = hy + (big ? 74 : 64), bh = big ? 60 : 46, i;
        ui_round_rect(bx + 2, by + 2, bw, bh, 6, PAL_RAMP(RAMP_STONE, 0));
        ui_round_rect(bx, by, bw, bh, 6, PAL_RAMP(RAMP_STONE, 2));
        ui_round_rect(bx + 1, by + 1, bw - 2, bh - 2, 5, PAL_WHITE);
        for (i = 0; i < 5; i++) plat_rect(bx + 20 + i, by - 5 + i, 6 - i, 1, PAL_WHITE);
        ui_text_wrap(bx + 5, by + 4, bw - 10, line, PAL_RAMP(RAMP_STONE, 0), 1);
    }
    if (S->st == WH_CHARGE) {
        int pw = big ? 90 : 60;
        ui_round_rect(bx, H - 26, pw + 4, 10, 4, PAL_RAMP(RAMP_STONE, 0));
        ui_bar(bx + 2, H - 24, pw, 6, S->power, 100, S->power > 80 ? PAL_UI_GOLD : PAL_PAINT(PAINT_MINT, 1));
        mg_text(bx + pw + 8, H - 26, "POWER", PAL_WHITE);
    }
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
}

static void wh_leave(void)
{
    mg_fx_clear();
}

const DoomoScene SCENE_MG_WHEEL = { "MG_WHEEL", wh_enter, wh_leave, wh_update, wh_draw, 0 };
