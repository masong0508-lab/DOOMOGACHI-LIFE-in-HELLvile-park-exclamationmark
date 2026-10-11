/* mg.c - the FUN FAIR arcade flow shared by every minigame (owner:
 * minigames): title card, READY/GO, pause, end banner, results with
 * H$ rewards, best scores (PROG_HS_*), medal unlocks and trophies, plus
 * the activities table that puts the booths on the island. See mg.h. */
#include "mg.h"

MgState mg;
uint32_t mg_arena[MG_ARENA_BYTES / 4];
DoomoRng mg_rng;
void *mg_state(void) { return mg_arena; }
static uint8_t last_tier[MG_GAME_COUNT];
static uint8_t prev_song;

const char *const mg_tier_names[MG_TIERS] = { "Sweet", "Spicy", "Hellfire" };
static const uint8_t tier_pct[MG_TIERS] = { 100, 150, 200 };

const MgGame *const mg_games[MG_GAME_COUNT] = {
    &mg_game_catch, &mg_game_whack, &mg_game_dodge,
    &mg_game_gallery, &mg_game_derby, &mg_game_memory
};

uint8_t mg_tier_color(int tier)
{
    return tier == 0 ? PAL_PAINT(PAINT_MINT, 1) :
           tier == 1 ? PAL_PAINT(PAINT_TANGERINE, 1) : PAL_PAINT(PAINT_CHERRY, 1);
}

/* ---- input --------------------------------------------------------------- */
int mg_down(uint32_t b)
{
    return mg.autoplay ? (mg.bot_now & b) != 0 : btn_down(b);
}

int mg_pressed(uint32_t b)
{
    return mg.autoplay ? (mg.bot_now & ~mg.bot_prev & b) != 0 : btn_pressed(b);
}

int mg_rand(int n) { return n > 1 ? rng_range(&mg_rng, n) : 0; }
int mg_between(int lo, int hi) { return hi > lo ? rng_between(&mg_rng, lo, hi) : lo; }

/* ---- scoring -------------------------------------------------------------- */
int mg_combo_mult(void)
{
    return 1 + DOOMO_MIN(mg.combo / 8, 4);
}

int mg_add_score(int base, int x, int y)
{
    int p = base * mg_combo_mult() * tier_pct[mg.tier] / 100;
    mg.score += p;
    mg.score_bump = 6;
    mg_popup(x, y, NULL, p, mg_combo_mult() >= 3 ? PAL_UI_GOLD : PAL_WHITE);
    return p;
}

void mg_add_raw(int pts, int x, int y)
{
    mg.score += pts;
    if (mg.score < 0) mg.score = 0;
    mg.score_bump = 6;
    mg_popup(x, y, NULL, pts, pts < 0 ? PAL_PAINT(PAINT_CHERRY, 2) : PAL_WHITE);
}

void mg_combo_hit(int x, int y)
{
    int before = mg_combo_mult();
    mg.combo++;
    mg.combo_bump = 6;
    if (mg.combo > mg.best_combo) mg.best_combo = mg.combo;
    if (mg_combo_mult() > before) {
        static const char *const words[] = { "", "", "COMBO x2!", "COMBO x3!",
                                             "COMBO x4!", "MAX COMBO x5!" };
        mg_popup(x, y - 10, words[mg_combo_mult()], 0, PAL_UI_GOLD);
        mg_burst(MGFX_STAR, x, y, 6, 30, PAL_UI_GOLD);
        audio_sfx(SFX_SPARKLE);
    }
}

void mg_combo_break(void)
{
    if (mg.combo >= 8) mg_popup(mg_w() / 2, 30, "combo dropped", 0, PAL_GRAY_L);
    mg.combo = 0;
}

void mg_lose_life(int x, int y)
{
    if (!mg.g->lives || mg.phase != MGP_PLAY) return;
    if (mg.lives) mg.lives--;
    mg_combo_break();
    mg_shake_add(8);
    mg_flash(PAL_UI_RED, 4);
    mg_hitstop(4);
    mg_burst(MGFX_HEART, x, y, 5, 28, PAL_GRAY_L);
    audio_sfx(SFX_PLAYER_HURT);
    if (!mg.lives) mg_end("OUCH!");
}

void mg_gain_life(int x, int y)
{
    if (mg.lives < mg.g->lives) {
        mg.lives++;
        mg_popup(x, y, "+" STR_ICON_HEART, 0, PAL_PAINT(PAINT_ROSE, 2));
    }
}

void mg_end(const char *text)
{
    if (mg.phase != MGP_PLAY) return;
    mg.phase = MGP_END;
    mg.t = 0;
    mg.end_text = text;
    mg_shake_add(6);
    audio_sfx(SFX_BELL);
}

int mg_medal_for(const MgGame *g, int32_t score)
{
    int m = 0;
    while (m < 3 && score >= g->medal[m]) m++;
    return m;
}

int mg_reward_for(const MgGame *g, int32_t score)
{
    static const uint8_t bonus[4] = { 0, 10, 25, 50 };
    int32_t base;
    if (score <= 0) return 0;
    base = score / g->pay_div;
    if (base > MG_REWARD_CAP) base = MG_REWARD_CAP;
    return (int)base + bonus[mg_medal_for(g, score)];
}

uint32_t mg_best(const MgGame *g)
{
    return progress_get(g->prog);
}

void mg_trophy(int id)
{
    unlock_grant(id);
}

int mg_finish_round(const MgGame *g, int32_t score)
{
    uint32_t best = mg_best(g);
    int i, all_played = 1, all_gold = 1, isnew = 0, reward;
    if (score < 0) score = 0;
    mg.prev_medal = (uint8_t)mg_medal_for(g, (int32_t)best);
    mg.medal = (uint8_t)mg_medal_for(g, score);
    if ((uint32_t)score > best) {
        isnew = best > 0 || score > 0;
        progress_set(g->prog, (uint32_t)score);     /* medal rules fire here */
        if (best > 0) {
            char buf[NEWS_LEN];
            doomo_snprintf(buf, sizeof(buf), "Fun Fair: new %s record, %d pts!",
                           g->title, (int)score);
            sim_news_push(buf);
        }
    }
    reward = mg_reward_for(g, score);
    if (reward > 0) {
        doomo_town.money += (uint32_t)reward;
        progress_add(PROG_MONEY_EARNED, reward);
    }
    mg.reward = reward;
    for (i = 0; i < MG_GAME_COUNT; i++) {
        uint32_t b = progress_get(mg_games[i]->prog);
        if (!b) all_played = 0;
        if (b < mg_games[i]->medal[2]) all_gold = 0;
    }
    if (all_played) mg_trophy(MG_UL_REGULAR);
    if (all_gold) mg_trophy(MG_UL_GOLD_ALL);
    if (mg.tier == MG_TIER_HELLFIRE && mg.medal == MG_MEDAL_GOLD) mg_trophy(MG_UL_HELLFIRE);
    return isnew;
}

/* ---- flow ----------------------------------------------------------------- */
int mg_w(void) { return plat_screen_w(); }
int mg_h(void) { return plat_screen_h(); }
int mg_big(void) { return plat_screen_w() >= 300 && plat_screen_h() >= 200; }

static void start_round(void)
{
    rng_seed(&mg_rng, doomo_hash32(plat_entropy() ^ doomo_frame_count() ^
                                   ((uint32_t)mg.g->id << 24)));
    memset(mg_arena, 0, sizeof(mg_arena));
    mg_fx_clear();
    mg.phase = MGP_READY;
    mg.t = 0;
    mg.play_t = 0;
    mg.score = mg.shown = 0;
    mg.reward = mg.reward_shown = 0;
    mg.combo = mg.best_combo = 0;
    mg.lives = mg.g->lives;
    mg.time_left = mg.g->time;
    mg.new_best = mg.medal = 0;
    mg.freeze = mg.shake = mg.flash_t = 0;
    mg.stat_a = mg.stat_b = 0;
    mg.bot_now = mg.bot_prev = 0;
    mg.result_sel = 0;
    mg.end_text = "TIME UP!";
    mg.hell = mg.tier == MG_TIER_HELLFIRE;
    last_tier[mg.g->id] = mg.tier;
    mg.g->start();
    audio_music(mg.hell ? SONG_HELL_BREACH : mg.g->song);
    input_eat();
}

void mg_scene_enter(const MgGame *g, int arg)
{
    int tier = arg & MG_ARG_TIER_MASK;
    memset(&mg, 0, sizeof(mg));
    if ((arg & MG_ARG_DEMO) && town_count(-1) == 0)
        town_new(doomo_town.mode, doomo_town.seed ? doomo_town.seed : 1234u, TOWN_POP_PREMADE);
    mg.g = g;
    mg.tier = (uint8_t)(tier ? tier - 1 : last_tier[g->id]);
    if (mg.tier >= MG_TIERS) mg.tier = 0;
    mg.autoplay = (arg & MG_ARG_AUTOPLAY) != 0;
    mg.autostart = (arg & MG_ARG_AUTOSTART) != 0;
    mg.hell = mg.tier == MG_TIER_HELLFIRE;
    prev_song = (uint8_t)audio_music_current();
    mg_fx_clear();
    if (mg.autostart) {
        start_round();
    } else {
        mg.phase = MGP_TITLE;
        audio_music(SONG_FESTIVAL);
    }
}

void mg_scene_leave(void)
{
    mg_fx_clear();
    audio_music(prev_song);
}

static void leave(void)
{
    audio_sfx(SFX_CANCEL);
    scene_pop();
}

void mg_scene_update(void)
{
    const MgGame *g = mg.g;
    mg.t++;
    if (mg.score_bump) mg.score_bump--;
    if (mg.combo_bump) mg.combo_bump--;
    if (mg.flash_t) mg.flash_t--;
    switch (mg.phase) {
    case MGP_TITLE:
        if (btn_repeat(BTN_LEFT) && mg.tier > 0) { mg.tier--; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_RIGHT) && mg.tier < MG_TIERS - 1) { mg.tier++; audio_sfx(SFX_CURSOR); }
        mg.hell = mg.tier == MG_TIER_HELLFIRE;
        if (btn_pressed(BTN_A | BTN_START) && mg.t > 6) { audio_sfx(SFX_SELECT); start_round(); }
        else if (btn_pressed(BTN_B)) leave();
        break;
    case MGP_READY:
        if (mg.t == 32) audio_sfx(SFX_BELL);
        if (mg.t >= 44) { mg.phase = MGP_PLAY; mg.t = 0; }
        mg_fx_update();
        break;
    case MGP_PLAY:
        if (btn_pressed(BTN_START)) {
            mg.phase = MGP_PAUSE;
            mg.pause_sel = 0;
            mg.t = 0;
            audio_sfx(SFX_SELECT);
            break;
        }
        if (mg.autoplay) { mg.bot_prev = mg.bot_now; mg.bot_now = g->bot(); }
        if (mg.freeze) {
            mg.freeze--;
        } else {
            mg.play_t++;
            g->update();
            if (g->time && mg.phase == MGP_PLAY) {
                if (mg.time_left) mg.time_left--;
                if (mg.time_left == 5 * 30 || mg.time_left == 3 * 30 ||
                    mg.time_left == 2 * 30 || mg.time_left == 30) audio_sfx(SFX_BLIP);
                if (!mg.time_left) mg_end("TIME UP!");
            }
        }
        mg_fx_update();
        break;
    case MGP_PAUSE:
        if (btn_pressed(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) {
            mg.pause_sel ^= 1;
            audio_sfx(SFX_CURSOR);
        }
        if (btn_pressed(BTN_START) || btn_pressed(BTN_B) ||
            (btn_pressed(BTN_A) && mg.pause_sel == 0)) {
            mg.phase = MGP_PLAY;
            input_eat();
        } else if (btn_pressed(BTN_A)) {
            leave();
        }
        break;
    case MGP_END:
        mg_fx_update();
        if (mg.t >= 56) {
            mg.new_best = (uint8_t)mg_finish_round(g, mg.score);
            mg.phase = MGP_RESULTS;
            mg.t = 0;
            if (mg.medal > mg.prev_medal || (mg.new_best && mg.medal)) {
                mg_confetti_rain(24);
                audio_music_once(SONG_VICTORY);
            } else if (mg.new_best) {
                audio_sfx(SFX_HAPPY);
            }
            input_eat();
        }
        break;
    case MGP_RESULTS:
        mg_fx_update();
        if (mg.shown < mg.score) {
            int32_t d = (mg.score - mg.shown) / 8 + 1;
            mg.shown += d;
            if (mg.shown > mg.score) mg.shown = mg.score;
            if ((mg.t & 3) == 0) audio_sfx(SFX_BLIP);
        } else if (mg.reward_shown < mg.reward) {
            mg.reward_shown += (mg.reward - mg.reward_shown) / 6 + 1;
            if (mg.reward_shown > mg.reward) mg.reward_shown = mg.reward;
            if ((mg.t & 3) == 0) audio_sfx(SFX_PICKUP);
        }
        if (mg.t > 20) {
            if (btn_pressed(BTN_LEFT | BTN_RIGHT)) { mg.result_sel ^= 1; audio_sfx(SFX_CURSOR); }
            if (btn_pressed(BTN_A)) {
                if (mg.shown < mg.score || mg.reward_shown < mg.reward) {
                    mg.shown = mg.score;
                    mg.reward_shown = mg.reward;
                } else if (mg.result_sel == 0) {
                    audio_sfx(SFX_SELECT);
                    start_round();
                } else {
                    leave();
                }
            } else if (btn_pressed(BTN_B)) {
                leave();
            }
        }
        break;
    default:
        break;
    }
    /* screen shake offset for this frame */
    if (mg.shake) {
        int amt = mg.shake, lvl = opt_get(OPT_SCREEN_SHAKE);
        if (lvl == 0) amt = 0;
        else if (lvl == 1) amt = (amt + 1) / 2;
        mg.ox = (int8_t)(amt ? rng_between(&doomo_rng, -amt / 2, amt / 2) : 0);
        mg.oy = (int8_t)(amt ? rng_between(&doomo_rng, -amt / 2, amt / 2) : 0);
        mg.shake--;
    } else {
        mg.ox = mg.oy = 0;
    }
}

/* ---- drawing: title card -------------------------------------------------- */
static void draw_tier_pill(int x, int y, int w, int tier, int sel, int t)
{
    uint8_t c = mg_tier_color(tier);
    int bob = sel ? ui_wave(t, 24, 2) - 2 : 0, i;
    y += bob;
    ui_round_rect(x + 1, y + 2, w, 15, 7, PAL_RAMP(RAMP_STONE, 1));
    ui_round_rect(x, y, w, 15, 7, sel ? ui_darker(c) : PAL_GRAY);
    ui_round_rect(x + 1, y + 1, w - 2, 13, 6, sel ? c : PAL_GRAY_L);
    plat_rect(x + 5, y + 2, w - 10, 1, PAL_WHITE);
    ui_text_outline(x + 5, y + 4, mg_tier_names[tier], PAL_WHITE,
                    sel ? ui_darker(ui_darker(c)) : PAL_GRAY_D);
    for (i = 0; i <= tier; i++) {
        char ic[2] = { (char)ICON_FIRE, 0 };
        ui_text(x + w - 9 - i * 5, y + 4, ic, sel ? PAL_UI_GOLD : PAL_GRAY);
    }
    if (sel) ui_cursor(x - 8, y + 4);
}

static void draw_title(void)
{
    const MgGame *g = mg.g;
    int W = mg_w(), H = mg_h(), big = mg_big(), t = mg.t;
    int sw = DOOMO_MIN(W - 10, big ? 300 : 230), sh = big ? 46 : 33;
    int sx = (W - sw) / 2, sy = big ? 6 : 2;
    int hx, hy, bx, by, bw, bh, pw, gap, i;
    uint8_t a = g->pal[0], b = g->pal[1], trim = g->pal[2];
    char buf[48];
    if (mg.hell) { a = PAL_RAMP(RAMP_BLOOD, 3); b = PAL_RAMP(RAMP_BLOOD, 1); trim = PAL_RAMP(RAMP_LAVA, 5); }
    mg_tent(0, H, a, b, trim, t);
    /* floor boards */
    {
        int fh = big ? 30 : 20, fy = H - fh;
        plat_rect(0, fy, W, fh, mg.hell ? PAL_RAMP(RAMP_WOOD, 0) : PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(0, fy, W, 2, mg.hell ? PAL_RAMP(RAMP_LAVA, 3) : PAL_RAMP(RAMP_WOOD, 5));
        for (i = 0; i < W; i += 28) plat_rect(i, fy + 2, 1, fh - 2, PAL_RAMP(RAMP_WOOD, mg.hell ? 0 : 2));
    }
    if (mg.hell)
        for (i = 0; i < 6; i++) {
            uint32_t s = doomo_hash32((uint32_t)i * 977u);
            int ex = (int)(s % (uint32_t)W), ey = H - (int)(((uint32_t)t * (2 + i % 3) + (s >> 8)) % (uint32_t)H);
            plat_rect(ex + ui_wave(t + i * 20, 40, 3), ey, 2, 2, PAL_RAMP(RAMP_LAVA, 6 - (i & 1)));
        }
    /* marquee sign */
    ui_round_rect(sx + 2, sy + 3, sw, sh, 6, PAL_RAMP(RAMP_STONE, 0));
    ui_round_rect(sx, sy, sw, sh, 6, mg.hell ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_WOOD, 1));
    ui_round_rect(sx + 3, sy + 3, sw - 6, sh - 6, 4, mg.hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_UI_CREAM);
    plat_rect(sx + 8, sy + 4, sw - 16, 1, PAL_WHITE);
    mg_bulbs(sx + 1, sy + 1, sw - 2, sh - 2, t);
    {
        int sc = big && ui_text_big_width(g->title, 3) <= sw - 24 ? 3 : 2;
        mg_big_text(W / 2, sy + (big ? 7 : 4), g->title, sc,
                    mg.hell ? PAL_RAMP(RAMP_LAVA, 6) : trim,
                    mg.hell ? PAL_RAMP(RAMP_BLOOD, 0) : ui_darker(ui_darker(trim)), t);
        ui_text_center(W / 2, sy + (big ? 7 : 4) + 8 * sc + (big ? 3 : 1), g->sub,
                       mg.hell ? PAL_RAMP(RAMP_LAVA, 5) : PAL_RAMP(RAMP_WOOD, 2));
    }
    mg_bunting(0, W, sy + sh + 1, big ? 8 : 4, t);
    /* host booth photo + speech bubble */
    hx = big ? 14 : 5;
    hy = sy + sh + (big ? 12 : 5);
    mg_host_frame(hx, hy, g->host, (t / 40) % 3 == 1 ? EXPR_TALK : EXPR_HAPPY, t);
    bx = hx + 64;
    by = hy + 2;
    bw = W - bx - (big ? 78 : 5);
    bh = big ? 56 : 38;
    ui_round_rect(bx + 2, by + 2, bw, bh, 6, PAL_RAMP(RAMP_STONE, 1));
    ui_round_rect(bx, by, bw, bh, 6, PAL_RAMP(RAMP_STONE, 2));
    ui_round_rect(bx + 1, by + 1, bw - 2, bh - 2, 5, PAL_WHITE);
    for (i = 0; i < 5; i++) plat_rect(bx - 5 + i, by + 12 + i, 6 - i, 1, PAL_WHITE);
    plat_line(bx - 6, by + 11, bx, by + 17, PAL_RAMP(RAMP_STONE, 2));
    ui_text_wrap(bx + 5, by + 4, bw - 10, g->how, PAL_RAMP(RAMP_STONE, 0), 1);
    mg_text(bx + 3, by + bh + 4, g->keys, PAL_WHITE);
    if (big) g->art(W - 38, hy + 30, t);
    else g->art(W - 18, H - 20, t);
    /* tiers */
    pw = big ? 90 : 76;
    gap = big ? 10 : 3;
    {
        int y = H - (big ? 74 : 56), x0 = (W - (pw * 3 + gap * 2)) / 2;
        for (i = 0; i < MG_TIERS; i++) draw_tier_pill(x0 + i * (pw + gap), y, pw, i, i == mg.tier, t);
    }
    /* best score + medals */
    {
        uint32_t best = mg_best(g);
        int m = mg_medal_for(g, (int32_t)best), y = H - (big ? 50 : 35), x, tw;
        doomo_snprintf(buf, sizeof(buf), "BEST %u", (unsigned)best);
        tw = ui_text_width(buf);
        x = W / 2 - (tw + 46) / 2;
        ui_round_rect(x - 6, y - 3, tw + 56, 14, 6, PAL_RAMP(RAMP_STONE, 0));
        mg_text(x, y, buf, PAL_UI_GOLD);
        x += tw + 8;
        for (i = 1; i <= 3; i++) mg_medal_icon(x + (i - 1) * 13 + 4, y + 3, m >= i ? i : 0, t + i * 9);
        if (big) {
            if (m < 3) doomo_snprintf(buf, sizeof(buf), "Next medal at %u", (unsigned)g->medal[m]);
            else doomo_snprintf(buf, sizeof(buf), "All medals won!");
            mg_textc(W / 2, y + 15, buf, PAL_WHITE);
        }
    }
    ui_hint(STR_ICON_A " Play  " STR_ICON_ARROW_L STR_ICON_ARROW_R " Tier  " STR_ICON_B " Leave",
            mg.hell ? WIN_HELL : WIN_CUTE);
}

/* ---- drawing: overlays ---------------------------------------------------- */
static void draw_ready(void)
{
    int W = mg_w(), H = mg_h(), t = mg.t;
    if (t < 30) {
        int y = H / 2 - 16, drop = t < 8 ? (8 - t) * 10 : 0;
        int bounce = t >= 8 && t < 16 ? -ui_wave(t - 8, 16, 6) : 0;
        mg_big_text(W / 2, y - drop + bounce, "READY?", 3, PAL_WHITE, PAL_RAMP(RAMP_PASTEL_PINK, 1), t);
        ui_text_center(W / 2, y + 30, mg_tier_names[mg.tier], mg_tier_color(mg.tier));
    } else {
        int sc = t < 33 ? 3 : 4;
        mg_big_text(W / 2, H / 2 - 4 * sc, "GO!", sc, PAL_UI_GOLD, PAL_RAMP(RAMP_WOOD, 0), t);
        if (t == 30) mg_burst(MGFX_STAR, W / 2, H / 2, 10, 48, PAL_UI_GOLD);
    }
}

static void draw_pause(void)
{
    int W = mg_w(), H = mg_h(), w = 120, h = 66, x = (W - w) / 2, y = (H - h) / 2;
    plat_shade_rect(0, 0, W, H, 8);
    if (ui_window_pop(x, y, w, h, WIN_CUTE, mg.t)) {
        ui_title(W / 2, y - 6, "PAUSED", WIN_CUTE);
        ui_button(x + 14, y + 16, w - 28, "Resume", mg.pause_sel == 0, WIN_CUTE);
        ui_button(x + 14, y + 38, w - 28, "Quit", mg.pause_sel == 1, WIN_CUTE);
    }
}

static void draw_end(void)
{
    int W = mg_w(), H = mg_h(), t = mg.t, y = H / 2 - 14;
    int slide = t < 10 ? (10 - t) * (W / 10) : 0, sc = 3;
    if (t < 10 && t == 9) mg_shake_add(4);
    ui_round_rect(slide - 4, y - 6, W + 8, 36, 6, PAL_RAMP(RAMP_STONE, 0));
    ui_round_rect(slide - 4, y - 8, W + 8, 36, 6, mg.lives || !mg.g->lives ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_UI_RED);
    plat_rect(slide, y - 6, W, 1, PAL_WHITE);
    mg_big_text(W / 2 + slide, y, mg.end_text, sc, PAL_WHITE, PAL_RAMP(RAMP_PASTEL_PINK, 0), t);
    if (t > 10 && t < 40 && (t & 3) == 0) mg_fx(MGFX_STAR, mg_between(10, W - 10), y + mg_between(-8, 26), 0, -8, PAL_WHITE, 10);
}

static void draw_results(void)
{
    const MgGame *g = mg.g;
    int W = mg_w(), H = mg_h(), big = mg_big(), t = mg.t;
    int w = big ? 240 : 200, h = big ? 180 : 140, x = (W - w) / 2, y = (H - h) / 2 + 5;
    int st = mg.hell ? WIN_HELL : WIN_CREAM, ly, m;
    uint8_t tc = ui_style_text(st);
    char buf[40];
    static const char *const mname[4] = { "No medal yet", "BRONZE MEDAL!", "SILVER MEDAL!", "GOLD MEDAL!" };
    plat_shade_rect(0, 0, W, H, 9);
    if (!ui_window_pop(x, y, w, h, st, t)) return;
    ui_title(W / 2, y - 7, "RESULTS", mg.hell ? WIN_HELL : WIN_CUTE);
    ui_text_center(W / 2, y + 9, g->title, tc);
    doomo_snprintf(buf, sizeof(buf), "%d", (int)mg.shown);
    {
        int sc = big ? 4 : 3;
        ui_text_big(W / 2 - ui_text_big_width(buf, sc) / 2, y + 20, buf, sc, PAL_UI_GOLD,
                    PAL_RAMP(RAMP_WOOD, 0));
    }
    if (mg.new_best && mg.shown >= mg.score) {
        int bx = x + w - 56, by = y + 22 + ui_wave(t, 20, 2);
        uint8_t c = (t / 4) & 1 ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(PAINT_MAGENTA, 1);
        ui_round_rect(bx, by, 50, 13, 6, ui_darker(c));
        ui_round_rect(bx + 1, by + 1, 48, 11, 5, c);
        ui_text_center(bx + 25, by + 3, "NEW BEST", PAL_WHITE);
        ui_sparkle(bx + 2, by, (t / 2) & 15, PAL_WHITE);
        ui_sparkle(bx + 48, by + 12, (t / 2 + 8) & 15, PAL_WHITE);
    }
    ly = y + (big ? 58 : 48);
    m = mg.shown >= mg.score ? mg.medal : 0;
    mg_medal_icon(x + 22, ly + 4, m, t);
    ui_text(x + 34, ly, mname[m], m ? PAL_RAMP(RAMP_CANDY, 2) : tc);
    if (m < 3) doomo_snprintf(buf, sizeof(buf), "%s at %u", m == 0 ? "Bronze" : m == 1 ? "Silver" : "Gold",
                              (unsigned)g->medal[m]);
    else doomo_snprintf(buf, sizeof(buf), "Fun Fair champion!");
    ui_text(x + 34, ly + 9, buf, PAL_GRAY);
    ly += big ? 26 : 22;
    doomo_snprintf(buf, sizeof(buf), "Combo %d", mg.best_combo);
    ui_text(x + 16, ly, buf, tc);
    doomo_snprintf(buf, sizeof(buf), "Record %u", (unsigned)mg_best(g));
    ui_text(x + w - 16 - ui_text_width(buf), ly, buf, tc);
    ly += big ? 16 : 12;
    ui_round_rect(x + 16, ly, w - 32, 18, 8, PAL_RAMP(RAMP_CANDY, 2));
    ui_round_rect(x + 17, ly + 1, w - 34, 16, 7, PAL_RAMP(RAMP_CANDY, 5));
    plat_rect(x + 24, ly + 2, w - 48, 1, PAL_RAMP(RAMP_CANDY, 7));
    doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN " +%d H$", (int)mg.reward_shown);
    ui_text_outline(W / 2 - ui_text_width(buf) / 2, ly + 5, buf, PAL_WHITE, PAL_RAMP(RAMP_CANDY, 1));
    if (mg.reward_shown > 0 && mg.reward_shown < mg.reward)
        ui_sparkle(W / 2 + ui_text_width(buf) / 2 + 4, ly + 4, (t / 2) & 15, PAL_UI_GOLD);
    ui_button(x + 14, y + h - 22, w / 2 - 20, "Retry", mg.result_sel == 0, WIN_CUTE);
    ui_button(x + w / 2 + 6, y + h - 22, w / 2 - 20, "Leave", mg.result_sel == 1, WIN_CUTE);
}

static void draw_flash(void)
{
    if (!mg.flash_t || opt_get(OPT_REDUCE_FLASH)) return;
    if (mg.flash_t > 2) plat_checker(0, 0, mg_w(), mg_h(), mg.flash_c);
    else {
        plat_rect(0, 0, mg_w(), 3, mg.flash_c);
        plat_rect(0, mg_h() - 3, mg_w(), 3, mg.flash_c);
    }
}

void mg_scene_draw(void)
{
    if (plat_draw_skipped()) return;
    if (mg.phase == MGP_TITLE) {
        draw_title();
        mg_fx_draw();
        return;
    }
    mg.g->draw();
    mg_fx_draw();
    if (mg.phase != MGP_RESULTS) mg_hud();
    draw_flash();
    switch (mg.phase) {
    case MGP_READY: draw_ready(); break;
    case MGP_PAUSE: draw_pause(); break;
    case MGP_END: draw_end(); break;
    case MGP_RESULTS: draw_results(); mg_fx_draw(); break;
    default: break;
    }
}

/* ---- the booths on the island ------------------------------------------------ */
extern const DoomoScene SCENE_MG_CATCH, SCENE_MG_WHACK, SCENE_MG_DODGE,
       SCENE_MG_GALLERY, SCENE_MG_DERBY, SCENE_MG_MEMORY, SCENE_MG_WHEEL;
#define FAIR (ACTF_FREEPLAY | ACTF_STORY)
static const DoomoActivity mg_activities[] = {
    { "Cupcake Catch", "Catch treats, dodge brimstone bagels", ACTLOC_FUN_FAIR, FAIR,
      ICON_FOOD, 1, ACT_ALWAYS, &SCENE_MG_CATCH, 0 },
    { "Whack-an-Imp", "Bonk imps. Do NOT bonk Father Fuzz", ACTLOC_FUN_FAIR, FAIR,
      ICON_SKULL, 1, ACT_ALWAYS, &SCENE_MG_WHACK, 0 },
    { "Memory Match", "Pair up citizens. Hellvile cards bite", ACTLOC_FUN_FAIR, FAIR,
      ICON_HEART, 1, ACT_ALWAYS, &SCENE_MG_MEMORY, 0 },
    { "Doomogachi Derby", "Race your pet against five rivals", ACTLOC_FUN_FAIR, FAIR,
      ICON_EGG, 1, ACT_ALWAYS, &SCENE_MG_DERBY, 0 },
    { "Bubble Blaster Gallery", "Pop cardboard demons, spare citizens", ACTLOC_FUN_FAIR, FAIR,
      ICON_GUN, 2, ACT_ALWAYS, &SCENE_MG_GALLERY, 0 },
    { "Vape Cloud Dodge", "Dodge Lord Vapula's vape rings", ACTLOC_FUN_FAIR, FAIR,
      ICON_CLOUD, 3, ACT_ALWAYS, &SCENE_MG_DODGE, 0 },
    { "Wheel of Fate", "One spin a day: prize or curse?", ACTLOC_FUN_FAIR, FAIR,
      ICON_STAR, 1, ACT_ALWAYS, &SCENE_MG_WHEEL, 0 },
    { NULL, NULL, 0, 0, 0, 0, 0, NULL, 0 }
};
const DoomoActivity *const doomo_activities_minigames = mg_activities;
