/* cook_scene.c - SCENE_COOKING: Granny Hopscotch's kitchen at Brimstone
 * Bites (owner: fishcook).
 *
 *   BOOK     the recipe book, one recipe per spread (LEFT/RIGHT flips the
 *            page, L/R jumps). A cooks (Granny sells you what's missing),
 *            START goes to the counter, B leaves.
 *   CHOP     the cleaver slides over the board: press A on each dotted line
 *   STIR     spin the D-pad clockwise (or rock LEFT/RIGHT) to keep the
 *            spoon's speed in the green zone
 *   HEAT     BAKE: take it out while it's golden; FRY: flip three times
 *            when the underside is golden; SIMMER: hold A for heat and
 *            keep the pot in the band; SCORCH: wait... for... black.
 *   RESULT   the plate, the grade stamp, portions into your bag
 *   COUNTER  today's customers (cook_customers): pick one, pick a dish
 *            from your bag, watch them react (sim_feed + flavour), tips!
 *
 * Args (fishcook_int.h): COOK_ARG_COUNTER opens at the counter,
 * COOK_ARG_RECIPE(r) starts cooking recipe r, COOK_ARG_AUTOPLAY lets a
 * bot play, COOK_ARG_STOCK fills the pantry, COOK_ARG_DEMO moves the
 * premade cast onto an empty island (tests / screenshots). */
#include "fishcook_int.h"

enum { CS_BOOK, CS_BUY, CS_INTRO, CS_CHOP, CS_STIR, CS_HEAT, CS_STEP_DONE,
       CS_RESULT, CS_COUNTER, CS_PICK, CS_REACT };

#define MAX_CUTS 8
#define STIR_TICKS 150
#define SIMMER_TICKS 150
#define GRACE 15

static struct {
    uint8_t  st, autoplay, opened, recipe, step, grade, sel, cust_sel, pick_sel, pick_top;
    uint8_t  ncuts, whiffs, next_dir, flips, flipping, react_kind, npicks, react_cust;
    int8_t   flip_dir;
    int16_t  t, flip_t, score[3], portions, tip;
    int16_t  kx16, kspeed16, slam_t, cut_x[MAX_CUTS];
    uint8_t  cut_state[MAX_CUTS], cut_score[MAX_CUTS];
    int16_t  speed16, angle, in_zone, done, brown, flip_sum, temp16, gust_t;
    uint8_t  picks[ITEM_COUNT];
    char     line[96];
    uint32_t bot_now, bot_prev;
    uint8_t  music;
} K;

/* ---- input ---------------------------------------------------------------------- */
static int down(uint32_t b) { return K.autoplay ? (K.bot_now & b) != 0 : btn_down(b); }
static int pressed(uint32_t b)
{
    if (K.autoplay) return (K.bot_now & b) && !(K.bot_prev & b);
    return btn_pressed(b);
}
static int repeat(uint32_t b) { return K.autoplay ? pressed(b) : btn_repeat(b); }

static void go(int st) { K.st = (uint8_t)st; K.t = 0; }

static const Recipe *RC(void) { return &cook_recipes[K.recipe < RECIPE_COUNT ? K.recipe : 0]; }
static int heat(void) { return RC()->heat; }
static int diff(void) { return DOOMO_CLAMP(RC()->diff, 1, 5); }

/* ---- layout ---------------------------------------------------------------------- */
static int W(void) { return fc_w(); }
static int H(void) { return fc_h(); }
static int counter_y(void) { return H() * 64 / 100; }
static int gran_x(void) { return fc_big() ? 52 : 38; }
static int stage_x(void) { return fc_big() ? W() * 60 / 100 : W() * 62 / 100; }

static void chop_geom(int *bx, int *by, int *bw)
{
    *bw = fc_big() ? 170 : 120;
    *bx = stage_x() - *bw / 2;
    *by = counter_y() - 6;
}

/* ---- Granny's lines -------------------------------------------------------------- */
static const char *const step_tips[6] = {
    "Chop when the knife kisses the line!",
    "Spin the pad round and round. Gently!",
    "Pull it out when it's golden. GOLDEN!",
    "Flip it when the bottom's golden-brown!",
    "Hold A for heat. Keep it purring!",
    "Burn it, dearie! Wait till it's BLACK!"
};
static const char *const grade_lines[5] = {
    "", "Well... it's crunchy. Hellvile folk love that!",
    "Edible! Mostly. Probably. Good job, dear!",
    "Lovely! Nana Puddin would weep with joy.",
    "Oh my whiskers! Better than mine! Shh!"
};

static const char *step_tip(void)
{
    if (K.step == 0) return step_tips[0];
    if (K.step == 1) return step_tips[1];
    switch (heat()) {
    case HEAT_BAKE: return step_tips[2];
    case HEAT_FRY: return step_tips[3];
    case HEAT_SIMMER: return step_tips[4];
    default: return step_tips[5];
    }
}

static const char *step_name(int step)
{
    static const char *const heat_words[4] = { "BAKE!", "FRY!", "SIMMER!", "SCORCH!" };
    if (step == 0) return "CHOP!";
    if (step == 1) return "STIR!";
    return heat_words[heat() & 3];
}

/* ---- the bot ---------------------------------------------------------------------- */
static uint32_t bot(void)
{
    static const uint32_t dirs[4] = { BTN_UP, BTN_RIGHT, BTN_DOWN, BTN_LEFT };
    switch (K.st) {
    case CS_CHOP: {
        int i;
        for (i = 0; i < K.ncuts; i++)
            if (K.cut_state[i] == 0) {
                int d = K.kx16 / 16 - K.cut_x[i];
                return (d >= -1 && d <= 1 && !(K.bot_prev & BTN_A)) ? BTN_A : 0;
            }
        return 0;
    }
    case CS_STIR: {
        int lo = 30 + diff() * 4, hi = 80 - diff() * 3, mid = (lo + hi) / 2;
        if (K.speed16 / 16 < mid && (K.t & 3) == 0) return dirs[K.next_dir];
        return 0;
    }
    case CS_HEAT:
        switch (heat()) {
        case HEAT_BAKE: return K.done >= 735 ? BTN_A : 0;
        case HEAT_FRY: {
            int lo = 60 + diff(), hi = 86 - diff() * 2;
            return (!K.flipping && K.brown / 16 >= (lo + hi) / 2 && !(K.bot_prev & BTN_A)) ? BTN_A : 0;
        }
        case HEAT_SIMMER: {
            int lo = 45 + diff() * 2, hi = 72 - diff() * 2;
            return K.temp16 / 16 < (lo + hi) / 2 ? BTN_A : 0;
        }
        default: return 0;                                 /* SCORCH: wait */
        }
    default:
        return 0;
    }
}

/* ---- steps ------------------------------------------------------------------------ */
static void start_step(int step)
{
    K.step = (uint8_t)step;
    go(CS_INTRO);
    audio_sfx(SFX_BELL);
}

static void begin_chop(void)
{
    int bx, by, bw, i, d = diff();
    chop_geom(&bx, &by, &bw);
    K.ncuts = (uint8_t)DOOMO_MIN(MAX_CUTS, 3 + d);
    for (i = 0; i < K.ncuts; i++) {
        K.cut_x[i] = (int16_t)(bx + 14 + (bw - 28) * (i + 1) / (K.ncuts + 1) + fc_rand(7) - 3);
        K.cut_state[i] = 0;
        K.cut_score[i] = 0;
    }
    K.whiffs = 0;
    K.slam_t = 0;
    K.kx16 = (int16_t)((bx - 8) * 16);
    K.kspeed16 = (int16_t)((bw + 16) * 16 / (100 - d * 10));
    go(CS_CHOP);
}

static void chop_tick(void)
{
    int bx, by, bw, i, kx;
    chop_geom(&bx, &by, &bw);
    K.kx16 = (int16_t)(K.kx16 + K.kspeed16);
    kx = K.kx16 / 16;
    if (K.slam_t) K.slam_t--;
    for (i = 0; i < K.ncuts; i++)
        if (K.cut_state[i] == 0 && K.cut_x[i] < kx - 12) {
            K.cut_state[i] = 2;
            fc_popup(K.cut_x[i], by - 22, "MISS", PAL_RAMP(RAMP_STONE, 4));
        }
    if (pressed(BTN_A)) {
        int best = -1, bd = 99;
        K.slam_t = 6;
        for (i = 0; i < K.ncuts; i++) {
            int d = kx - K.cut_x[i];
            if (d < 0) d = -d;
            if (K.cut_state[i] == 0 && d <= 12 && d < bd) { bd = d; best = i; }
        }
        if (best >= 0) {
            int s = bd <= 1 ? 100 : bd <= 3 ? 85 : bd <= 6 ? 60 : bd <= 9 ? 35 : 15;
            K.cut_state[best] = 1;
            K.cut_score[best] = (uint8_t)s;
            fc_popup(K.cut_x[best], by - 22, s >= 100 ? "PERFECT" : s >= 85 ? "NICE" : s >= 60 ? "OK" : "WOBBLY",
                     s >= 85 ? PAL_UI_GOLD : PAL_WHITE);
            fc_burst(FCX_CRUMB, K.cut_x[best], by - 4, 5, 20, RC()->c1);
            audio_sfx(SFX_SQUEAK);
        } else {
            K.whiffs++;
            fc_popup(kx, by - 22, "whoosh", PAL_RAMP(RAMP_SKY, 6));
            audio_sfx(SFX_BLIP);
        }
    }
    if (kx > bx + bw + 14) {
        int sum = 0;
        for (i = 0; i < K.ncuts; i++) sum += K.cut_score[i];
        K.score[0] = (int16_t)DOOMO_CLAMP(sum / DOOMO_MAX(1, K.ncuts) - K.whiffs * 8, 0, 100);
        go(CS_STEP_DONE);
    }
}

static void begin_stir(void)
{
    K.speed16 = 0;
    K.angle = 0;
    K.in_zone = 0;
    K.next_dir = 0;
    go(CS_STIR);
}

static void stir_tick(void)
{
    static const uint32_t dirs[4] = { BTN_UP, BTN_RIGHT, BTN_DOWN, BTN_LEFT };
    int lo = 30 + diff() * 4, hi = 80 - diff() * 3, sp, k;
    K.speed16 = (int16_t)DOOMO_MAX(0, K.speed16 - 16);
    for (k = 0; k < 4; k++) {
        if (!pressed(dirs[k])) continue;
        if (k == K.next_dir) { K.speed16 = (int16_t)(K.speed16 + 190); K.next_dir = (uint8_t)((k + 1) & 3); }
        else if (k == ((K.next_dir + 1) & 3)) { K.speed16 = (int16_t)(K.speed16 + 130); K.next_dir = (uint8_t)((k + 1) & 3); }
        else K.speed16 = (int16_t)DOOMO_MAX(0, K.speed16 - 60);
    }
    K.speed16 = (int16_t)DOOMO_MIN(K.speed16, 1600);
    sp = K.speed16 / 16;
    K.angle = (int16_t)((K.angle + K.speed16 / 6) & DOOMO_ANG_MASK);
    if (K.t > GRACE && sp >= lo && sp <= hi) K.in_zone++;
    if (sp > hi + 8 && (K.t & 3) == 0) {
        fc_fx(FCX_DROP, stage_x() + fc_rand(30) - 15, counter_y() - 22, fc_rand(41) - 20, -30 - fc_rand(20), RC()->c1, 20);
        if ((K.t & 15) == 0) audio_sfx(SFX_SPLAT);
    }
    if (sp >= lo && sp <= hi && (K.t % 10) == 0) audio_sfx(SFX_BUBBLE);
    if (K.t >= STIR_TICKS) {
        K.score[1] = (int16_t)DOOMO_CLAMP(K.in_zone * 100 / (STIR_TICKS - GRACE), 0, 100);
        go(CS_STEP_DONE);
    }
}

static void begin_heat(void)
{
    K.done = 0;
    K.brown = 0;
    K.flips = 0;
    K.flipping = 0;
    K.flip_sum = 0;
    K.temp16 = 20 * 16;
    K.in_zone = 0;
    K.gust_t = (int16_t)(20 + fc_rand(20));
    go(CS_HEAT);
}

static void finish_heat(int score)
{
    K.score[2] = (int16_t)DOOMO_CLAMP(score, 0, 100);
    go(CS_STEP_DONE);
}

static void heat_tick(void)
{
    int d = diff(), h = heat();
    if (h == HEAT_BAKE || h == HEAT_SCORCH) {
        int c = h == HEAT_SCORCH ? 950 : 740, hw = h == HEAT_SCORCH ? 50 : 130 - d * 18;
        K.done = (int16_t)(K.done + 1000 / (110 - d * 6));
        if ((K.t % 12) == 0) fc_fx(FCX_STEAM, stage_x() + fc_rand(20) - 10, counter_y() - 50, 0, -8, PAL_WHITE, 20);
        if (pressed(BTN_A)) {
            int s;
            if (K.done >= c - hw && K.done <= c + hw) s = 100 - (DOOMO_MAX(K.done - c, c - K.done) * 40) / hw;
            else if (K.done < c - hw) s = K.done * 55 / (c - hw);
            else s = 40 - (K.done - c - hw) / 4;
            audio_sfx(SFX_BELL);
            finish_heat(s);
            return;
        }
        if (K.done >= 1000) {
            K.done = 1000;
            fc_burst(FCX_SMOKE, stage_x(), counter_y() - 40, 10, 20, PAL_RAMP(RAMP_STONE, 2));
            if (h == HEAT_SCORCH) { audio_sfx(SFX_EXPLODE); finish_heat(100); }
            else { audio_sfx(SFX_EXPLODE); finish_heat(0); }
        }
    } else if (h == HEAT_FRY) {
        int lo = 60 + d, hi = 86 - d * 2, b;
        if (K.flipping) {
            if (--K.flipping == 0 && K.flips >= 3) { finish_heat(K.flip_sum / 3); return; }
            return;
        }
        K.brown = (int16_t)(K.brown + 18 + d * 4);
        b = K.brown / 16;
        if ((K.t & 3) == 0) fc_fx(FCX_DROP, stage_x() + fc_rand(30) - 15, counter_y() - 16, fc_rand(31) - 15, -20 - fc_rand(15), PAL_UI_GOLD, 12);
        if ((K.t % 8) == 0) audio_sfx(SFX_WELD);
        if (pressed(BTN_A) || b >= 100) {
            int s = b >= 100 ? 5 : (b >= lo && b <= hi) ? 100 - DOOMO_MAX(b - (lo + hi) / 2, (lo + hi) / 2 - b) * 30 / DOOMO_MAX(1, (hi - lo) / 2)
                  : b < lo ? b * 50 / lo : DOOMO_MAX(0, 40 - (b - hi) * 3);
            K.flip_sum = (int16_t)(K.flip_sum + DOOMO_CLAMP(s, 0, 100));
            K.flips++;
            K.flipping = 20;
            K.brown = 0;
            fc_popup(stage_x(), counter_y() - 44, s >= 85 ? "FLIP!" : s >= 50 ? "flip" : b >= 100 ? "BURNT!" : "early!",
                     s >= 85 ? PAL_UI_GOLD : PAL_WHITE);
            audio_sfx(b >= 100 ? SFX_SAD : SFX_SQUEAK);
        }
    } else {                                                    /* SIMMER */
        int lo = 45 + d * 2, hi = 72 - d * 2, tp;
        if (down(BTN_A)) K.temp16 = (int16_t)(K.temp16 + 22);
        else K.temp16 = (int16_t)(K.temp16 - 15);
        if (--K.gust_t <= 0) {
            K.temp16 = (int16_t)(K.temp16 + (fc_rand(3) == 0 ? -(60 + fc_rand(80)) : 80 + fc_rand(100)));
            K.gust_t = (int16_t)(20 + fc_rand(25));
        }
        K.temp16 = (int16_t)DOOMO_CLAMP(K.temp16, 0, 1600);
        tp = K.temp16 / 16;
        if (K.t > GRACE && tp >= lo && tp <= hi) K.in_zone++;
        if (tp > 85 && (K.t & 3) == 0) fc_fx(FCX_BUBBLE, stage_x() + fc_rand(30) - 15, counter_y() - 34, fc_rand(21) - 10, -16, PAL_WHITE, 14);
        if ((K.t % 9) == 0 && tp > 30) audio_sfx(SFX_BUBBLE);
        if (K.t >= SIMMER_TICKS) finish_heat(K.in_zone * 100 / (SIMMER_TICKS - GRACE));
    }
}

static void start_cooking(void)
{
    int i;
    cook_take(K.recipe);
    for (i = 0; i < 3; i++) K.score[i] = 0;
    rng_seed(&fc_rng, doomo_town.seed ^ (uint32_t)K.recipe * 7919u ^ (uint32_t)fc.cooked * 104729u);
    fc_fx_clear();
    start_step(0);
}

static void to_result(void)
{
    K.grade = (uint8_t)cook_grade(K.recipe, K.score[0], K.score[1], K.score[2]);
    K.portions = (int16_t)cook_finish(K.recipe, K.grade);
    K.sel = 0;
    go(CS_RESULT);
    if (K.grade == GRADE_BURNT) {
        audio_sfx(SFX_SAD);
        fc_burst(FCX_SMOKE, W() / 2, H() / 2, 10, 24, PAL_RAMP(RAMP_STONE, 3));
    } else {
        audio_sfx(K.grade == GRADE_PERFECT ? SFX_EVOLVE : SFX_HAPPY);
        fc_burst(K.grade == GRADE_PERFECT ? FCX_CONFETTI : FCX_STAR, W() / 2, H() / 3, 16, 36,
                 K.grade == GRADE_PERFECT ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_UI_GOLD);
    }
}

/* ---- the counter ---------------------------------------------------------------- */
static int seat_x(int i) { return W() * (1 + 2 * i) / (2 * FC_CUSTOMERS); }

static void build_picks(int cust)
{
    const DoomoResident *r = cook_customer(cust) >= 0 ? town_res(cook_customer(cust)) : NULL;
    int it, n = 0, pass;
    for (pass = 0; pass < 2; pass++)            /* their favourite flavours first */
        for (it = 1; it < ITEM_COUNT; it++) {
            int fav;
            if (doomo_items[it].category != ICAT_FOOD || inv_count(it) <= 0) continue;
            fav = r && cook_item_food(it) >= 0 && cook_item_food(it) == r->like_food;
            if (fav == (pass == 0)) K.picks[n++] = (uint8_t)it;
        }
    K.npicks = (uint8_t)n;
    K.pick_sel = 0;
    K.pick_top = 0;
}

static void first_free_customer(void)
{
    int i;
    for (i = 0; i < FC_CUSTOMERS; i++)
        if (cook_customer(i) >= 0 && !cook_customer_served(i)) { K.cust_sel = (uint8_t)i; return; }
}

static void to_counter(void)
{
    cook_customers();
    K.cust_sel = 0;
    first_free_customer();
    go(CS_COUNTER);
}

/* ---- scene ------------------------------------------------------------------------ */
static void stock_pantry(void)
{
    static const uint8_t items[] = {
        ITEM_CLOUD_FLOUR, ITEM_PIXIE_SUGAR, ITEM_SPECKLED_EGG, ITEM_MILK, ITEM_HELL_PEPPERS,
        ITEM_SPROUTS, ITEM_PICKLES, ITEM_BLOOD_ORANGE, ITEM_ONIGIRI, ITEM_COFFEE, ITEM_SALAD,
        ITEM_CANDY_EYEBALLS, ITEM_BONE_BROTH
    };
    int i;
    for (i = 0; i < DOOMO_COUNTOF(items); i++) inv_add(items[i], 6);
    fc.cooler[FISH_BRIMSTONE_BREAM] = 4;
    fc.cooler[FISH_SPRINKLE_TROUT] = 4;
    fc.cooler[FISH_SCREAMING_CARP] = 2;
    fc.cooler[FISH_LAVA_EEL] = 1;
    fc.cooler[FISH_CURSED_COD] = 1;
}

static void cook_enter(int arg)
{
    int r = (arg >> 8) & 0xFF;
    memset(&K, 0, sizeof(K));
    if ((arg & COOK_ARG_DEMO) && town_count(-1) == 0)
        town_new(doomo_town.mode, doomo_town.seed ? doomo_town.seed : 1234u, TOWN_POP_PREMADE);
    if (arg & COOK_ARG_STOCK) stock_pantry();
    K.autoplay = (arg & COOK_ARG_AUTOPLAY) != 0;
    K.opened = (uint8_t)(arg & COOK_ARG_COUNTER);
    rng_seed(&fc_rng, doomo_town.seed ^ doomo_town.day ^ 0xC00Cu);
    fc_fx_clear();
    K.music = (uint8_t)audio_music_current();
    audio_music(SONG_SHOP);
    if (r > 0 && r <= RECIPE_COUNT) {
        K.recipe = (uint8_t)(r - 1);
        if (cook_recipe_ok(K.recipe) && cook_can_make(K.recipe)) start_cooking();
        else go(CS_BOOK);
    } else if (K.opened) {
        to_counter();
    } else {
        go(CS_BOOK);
    }
}

static void cook_leave(void) { audio_music(K.music); }

static void flip_page(int d)
{
    K.recipe = (uint8_t)((K.recipe + RECIPE_COUNT + d) % RECIPE_COUNT);
    K.flip_t = 8;
    K.flip_dir = (int8_t)(d > 0 ? 1 : -1);
    audio_sfx(SFX_CURSOR);
}

static void serve_pick(void)
{
    int item = K.picks[K.pick_sel], id = cook_customer(K.cust_sel), tip = 0;
    int kind = cook_serve(K.cust_sel, item, K.line, sizeof(K.line), &tip);
    K.react_cust = K.cust_sel;
    K.react_kind = (uint8_t)kind;
    K.tip = (int16_t)tip;
    if (kind == SERVE_FAIL) {
        audio_sfx(SFX_ERROR);
        go(CS_COUNTER);
        return;
    }
    go(CS_REACT);
    audio_sfx(SFX_EAT);
    {
        int x = seat_x(K.cust_sel), y = counter_y() - 30;
        if (kind == SERVE_FAV) { fc_burst(FCX_HEART, x, y, 10, 30, PAL_PAINT(PAINT_ROSE, 1)); audio_sfx(SFX_LOVE); }
        else if (kind == SERVE_HATE) { fc_burst(FCX_SMOKE, x, y, 6, 16, PAL_PAINT(PAINT_SLIME, 1)); audio_sfx(SFX_SAD); }
        else fc_burst(FCX_STAR, x, y, 6, 20, PAL_UI_GOLD);
        if (tip > 0) {
            char buf[16];
            doomo_snprintf(buf, sizeof(buf), "+%d H$", tip);
            fc_popup(x, y - 20, buf, PAL_UI_GOLD);
            fc_burst(FCX_COIN, x, y, 5, 24, PAL_UI_GOLD);
        }
    }
    (void)id;
}

static void cook_update(void)
{
    K.bot_prev = K.bot_now;
    K.bot_now = K.autoplay ? bot() : 0;
    K.t++;
    if (K.flip_t) K.flip_t--;
    fc_fx_update();
    switch (K.st) {
    case CS_BOOK:
        if (repeat(BTN_LEFT)) flip_page(-1);
        else if (repeat(BTN_RIGHT)) flip_page(1);
        else if (repeat(BTN_L)) flip_page(-5);
        else if (repeat(BTN_R)) flip_page(5);
        else if (pressed(BTN_START)) { audio_sfx(SFX_SELECT); to_counter(); }
        else if (pressed(BTN_B)) { audio_sfx(SFX_CANCEL); scene_pop(); return; }
        else if (pressed(BTN_A)) {
            if (!cook_recipe_ok(K.recipe)) audio_sfx(SFX_ERROR);
            else if (cook_can_make(K.recipe)) { audio_sfx(SFX_SELECT); start_cooking(); }
            else if (cook_missing_cost(K.recipe) >= 0) { audio_sfx(SFX_SELECT); go(CS_BUY); }
            else audio_sfx(SFX_ERROR);
        }
        break;
    case CS_BUY:
        if (pressed(BTN_A)) {
            if (cook_buy_missing(K.recipe)) { audio_sfx(SFX_PICKUP); start_cooking(); }
            else audio_sfx(SFX_ERROR);
        } else if (pressed(BTN_B)) { audio_sfx(SFX_CANCEL); go(CS_BOOK); }
        break;
    case CS_INTRO:
        if (K.t >= 40 || (K.t > 12 && pressed(BTN_A))) {
            if (K.step == 0) begin_chop();
            else if (K.step == 1) begin_stir();
            else begin_heat();
        }
        break;
    case CS_CHOP: chop_tick(); break;
    case CS_STIR: stir_tick(); break;
    case CS_HEAT: heat_tick(); break;
    case CS_STEP_DONE:
        if (K.t == 1) {
            int s = K.score[K.step];
            fc_popup(stage_x(), counter_y() - 60, s >= 88 ? "PERFECT!" : s >= 65 ? "GREAT!" : s >= 40 ? "OK!" : "Oops!",
                     s >= 88 ? PAL_UI_GOLD : s >= 65 ? PAL_PAINT(PAINT_MINT, 2) : PAL_WHITE);
            audio_sfx(s >= 65 ? SFX_SPARKLE : SFX_BLIP);
        }
        if (K.t >= 30) {
            if (K.step < 2) start_step(K.step + 1);
            else to_result();
        }
        break;
    case CS_RESULT:
        if (K.t < 20 || K.autoplay) break;
        if (repeat(BTN_LEFT)) { K.sel = (uint8_t)((K.sel + 2) % 3); audio_sfx(SFX_CURSOR); }
        if (repeat(BTN_RIGHT)) { K.sel = (uint8_t)((K.sel + 1) % 3); audio_sfx(SFX_CURSOR); }
        if (pressed(BTN_A)) {
            audio_sfx(SFX_SELECT);
            if (K.sel == 0) {
                if (cook_recipe_ok(K.recipe) && cook_can_make(K.recipe)) start_cooking();
                else if (cook_missing_cost(K.recipe) >= 0) go(CS_BUY);
                else go(CS_BOOK);
            } else if (K.sel == 1) to_counter();
            else go(CS_BOOK);
        } else if (pressed(BTN_B)) { audio_sfx(SFX_CANCEL); go(CS_BOOK); }
        break;
    case CS_COUNTER: {
        int i, n = 0;
        for (i = 0; i < FC_CUSTOMERS; i++) n += cook_customer(i) >= 0;
        if (n) {
            int d = repeat(BTN_RIGHT) ? 1 : repeat(BTN_LEFT) ? -1 : 0, k;
            for (k = 0; d && k < FC_CUSTOMERS; k++) {
                K.cust_sel = (uint8_t)((K.cust_sel + FC_CUSTOMERS + d) % FC_CUSTOMERS);
                if (cook_customer(K.cust_sel) >= 0) { audio_sfx(SFX_CURSOR); break; }
            }
            if (K.autoplay && K.t == 30) K.bot_now = BTN_A;
            if (pressed(BTN_A) && cook_customer(K.cust_sel) >= 0) {
                if (cook_customer_served(K.cust_sel)) audio_sfx(SFX_ERROR);
                else { build_picks(K.cust_sel); go(CS_PICK); audio_sfx(SFX_SELECT); }
            }
        }
        if (pressed(BTN_B) || pressed(BTN_START)) {
            audio_sfx(SFX_CANCEL);
            if (K.opened) { scene_pop(); return; }
            go(CS_BOOK);
        }
        break; }
    case CS_PICK: {
        int vis = fc_big() ? 8 : 5;
        if (K.autoplay && K.t == 20) K.bot_now = BTN_A;
        if (repeat(BTN_UP) && K.pick_sel > 0) { K.pick_sel--; audio_sfx(SFX_CURSOR); }
        if (repeat(BTN_DOWN) && K.pick_sel + 1 < K.npicks) { K.pick_sel++; audio_sfx(SFX_CURSOR); }
        if (K.pick_sel < K.pick_top) K.pick_top = K.pick_sel;
        if (K.pick_sel >= K.pick_top + vis) K.pick_top = (uint8_t)(K.pick_sel - vis + 1);
        if (pressed(BTN_A) && K.npicks) serve_pick();
        else if (pressed(BTN_B)) { audio_sfx(SFX_CANCEL); go(CS_COUNTER); }
        break; }
    case CS_REACT:
        if (K.t > 90 || (K.t > 15 && pressed(BTN_A))) { first_free_customer(); go(CS_COUNTER); }
        break;
    default: break;
    }
}

/* ---- drawing ------------------------------------------------------------------------ */
static void title_bar(const char *s)
{
    ui_title(W() / 2, 3, s, WIN_CUTE);
    fc_money_pill(W() - 3, 3);
}

static void grade_badge(int x, int y, int g)
{
    static const uint8_t col[5] = { 0, PAL_RAMP(RAMP_STONE, 3), PAL_PAINT(PAINT_RUST, 2),
                                    PAL_PAINT(PAINT_SILVER, 2), PAL_UI_GOLD };
    if (g <= GRADE_NONE) { ui_frame(x - 3, y - 3, 7, 7, PAL_RAMP(RAMP_STONE, 4)); return; }
    if (g == GRADE_PERFECT) { ui_star(x, y, 4, PAL_UI_GOLD); return; }
    ui_fill_circle(x, y, 3, ui_darker(col[g]));
    ui_fill_circle(x, y, 2, col[g]);
    plat_rect(x - 1, y - 1, 1, 1, PAL_WHITE);
}

static void draw_book(void)
{
    const Recipe *rc = RC();
    int w = W(), h = H(), t = K.t, big = fc_big(), ok = cook_recipe_ok(K.recipe);
    int bx = 6, by = big ? 24 : 18, bw = w - 12, bh = h - by - (big ? 58 : 46);
    int lp = bx + 6, lw = bw / 2 - 10, rp = bx + bw / 2 + 6, rw = bw / 2 - 12;
    int i, y, fl = K.flip_t;
    char buf[64];
    kitchen_draw_back((uint32_t)t, 0);
    kitchen_draw_counter(counter_y() + 10, (uint32_t)t);
    title_bar("Granny's Recipe Book");
    /* the open book: cover, two cream pages, a spine, page curl when flipping */
    ui_round_rect(bx - 2, by - 2, bw + 4, bh + 6, 6, PAL_PAINT(PAINT_BLOOD, 0));
    ui_round_rect(bx - 1, by - 1, bw + 2, bh + 4, 5, PAL_PAINT(PAINT_CHERRY, 1));
    ui_round_rect(bx + 2, by + 1, bw / 2 - 2, bh - 2, 3, PAL_RAMP(RAMP_CANDY, 7));
    ui_round_rect(bx + bw / 2, by + 1, bw / 2 - 2, bh - 2, 3, PAL_RAMP(RAMP_CANDY, 7));
    plat_rect(bx + bw / 2 - 1, by + 1, 2, bh - 2, PAL_RAMP(RAMP_PEACH, 4));
    plat_rect(bx + bw / 2 - 3, by + 1, 1, bh - 2, PAL_RAMP(RAMP_PEACH, 6));
    for (y = by + 14; y < by + bh - 6; y += 12)                                     /* ruled lines */
        plat_rect(rp, y + 9, rw, 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
    ui_heart(bx + bw - 8, by - 1, 3, PAL_PAINT(PAINT_ROSE, 2));                    /* bookmark */
    plat_rect(bx + bw - 9, by - 1, 3, 6 + ui_wave(t, 60, 1), PAL_PAINT(PAINT_ROSE, 1));
    if (fl) {
        int px = K.flip_dir > 0 ? bx + bw / 2 + (bw / 2) * fl / 8 : bx + bw / 2 - (bw / 2) * fl / 8;
        int a = px < bx + bw / 2 ? px : bx + bw / 2, b = px < bx + bw / 2 ? bx + bw / 2 : px;
        plat_rect(a, by + 2, b - a, bh - 4, PAL_WHITE);
        plat_rect(px, by + 2, 1, bh - 4, PAL_RAMP(RAMP_PEACH, 4));
    } else {
        doomo_snprintf(buf, sizeof(buf), "%d/%d", K.recipe + 1, RECIPE_COUNT);
        ui_text(rp + rw - ui_text_width(buf), by + 4, buf, PAL_RAMP(RAMP_PEACH, 3));
        /* left page: the dish on a doily, its name and blurb */
        {
            int cx = lp + lw / 2, cy = by + (big ? 44 : 28), r = big ? 28 : 17;
            ui_fill_circle(cx, cy, r + 5, PAL_RAMP(RAMP_PASTEL_PINK, 7));
            for (i = 0; i < 6; i++) {
                int a = (t * 8 + i * 683) & DOOMO_ANG_MASK;
                ui_sparkle(cx + FIX2INT(fx_cos(a) * (r + 8)), cy + FIX2INT(fx_sin(a) * (r / 2 + 6)), (t / 2 + i * 3) & 15,
                           ok ? PAL_UI_GOLD : PAL_RAMP(RAMP_STONE, 4));
            }
            if (ok) dish_draw(K.recipe, cx, cy + ui_wave(t, 60, 1), r, t, 0);
            else {
                ui_fill_ellipse(cx, cy + 4, r, r / 3, PAL_RAMP(RAMP_STONE, 5));
                ui_text_big(cx - 6, cy - 12, "?", 3, PAL_RAMP(RAMP_STONE, 4), PAL_WHITE);
            }
            y = cy + r / 2 + (big ? 12 : 8);
            y += FONT_LINE * ui_text_wrap(lp + 2, y, lw - 2, ok ? rc->name : "Secret recipe", PAL_PAINT(PAINT_CHERRY, 0), 1) + 1;
            ui_text_wrap(lp + 2, y, lw - 2, ok ? rc->desc : (unlock_by_id(rc->unlock) ? unlock_by_id(rc->unlock)->how : ""),
                         PAL_RAMP(RAMP_WOOD, 2), 1);
        }
        /* right page: ingredients, steps, flavour, difficulty, best grade */
        y = by + 4;
        ui_text(rp, y, "Ingredients", PAL_PAINT(PAINT_CHERRY, 0));
        y += big ? 13 : 11;
        for (i = 0; i < 3; i++) {
            const CookIng *g = &rc->ing[i];
            int have, need, maxc, k;
            const char *nm;
            char nb[24];
            if (g->kind == ING_NONE) continue;
            have = cook_have(g);
            need = g->qty;
            switch (g->kind) {
            case ING_ITEM: nm = doomo_items[g->id].name; break;
            case ING_FISH: nm = ok || fish_dex_has(g->id) ? fish_info[g->id].name : "mystery fish"; break;
            case ING_CUTE_FISH: nm = "cute fish"; break;
            case ING_HELL_FISH: nm = "hellish fish"; break;
            default: nm = "any fish"; break;
            }
            if (ok) ingredient_draw(g, rp + 5, y + 3, 5, t + i * 9);
            maxc = (rw - 36) / FONT_ADVANCE;
            for (k = 0; k < maxc && k < 23 && nm[k]; k++) nb[k] = nm[k];
            if (nm[k] && k > 1) nb[k - 1] = '.';
            nb[k] = 0;
            ui_text(rp + 13, y, nb, PAL_RAMP(RAMP_WOOD, 1));
            doomo_snprintf(buf, sizeof(buf), "%d/%d", DOOMO_MIN(have, 99), need);
            ui_text(rp + rw - ui_text_width(buf), y, buf, have >= need ? PAL_PAINT(PAINT_FOREST, 1) : PAL_PAINT(PAINT_CHERRY, 1));
            y += big ? 13 : 11;
        }
        doomo_snprintf(buf, sizeof(buf), "Chop" STR_ICON_ARROW_R "Stir" STR_ICON_ARROW_R "%s", cook_heat_names[rc->heat & 3]);
        ui_text(rp, y + 1, buf, PAL_PAINT(PAINT_GRAPE, 1));
        y += FONT_LINE + 3;
        if (rc->food >= 0) {
            doomo_snprintf(buf, sizeof(buf), "Tastes like %s!", doomo_food_names[rc->food]);
            y += FONT_LINE * ui_text_wrap(rp, y, rw, buf, PAL_PAINT(PAINT_TEAL, 1), 1) + 2;
        }
        y = by + bh - 12;
        for (i = 0; i < diff(); i++) ui_heart(rp + 4 + i * 8, y + 4, 3, PAL_PAINT(PAINT_ROSE, 1));
        grade_badge(rp + rw - 4, y + 4, fc.grade[K.recipe]);
    }
    /* Granny, below the book, with advice */
    {
        const char *say;
        int gx = big ? 24 : 17, gy = h - (big ? 12 : 8);
        int cost = cook_missing_cost(K.recipe);
        if (!ok) say = "That page is stuck together. Earn it first!";
        else if (cook_can_make(K.recipe)) say = "Ooh, this one! Press A to cook, dearie!";
        else if (cost >= 0) {
            doomo_snprintf(K.line, sizeof(K.line), "Short a bit? I'll sell you the rest for %d H$.", cost);
            say = K.line;
        } else say = "This one needs fish, dear. Off to the Lake of Fire!";
        granny_draw(gx, gy + 6, big ? 12 : 9, t, GRAN_TALK);
        fc_bubble(gx + (big ? 22 : 17), by + bh + 5, w - gx - (big ? 28 : 21), say, gx + 6, gy - 12, 999);
    }
    ui_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R "Page " STR_ICON_A "Cook START Serve " STR_ICON_B "Exit", WIN_CUTE);
}

static void draw_buy(void)
{
    int w = fc_big() ? 220 : 196, h = 58, x = (W() - w) / 2, y = (H() - h) / 2;
    int cost = cook_missing_cost(K.recipe);
    char buf[64];
    plat_shade_rect(0, 0, W(), H(), 10);
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, K.t)) return;
    granny_draw(x + 20, y + h - 6, 8, K.t, GRAN_TALK);
    doomo_snprintf(buf, sizeof(buf), "Need a hand? The missing bits are %d H$.", cost);
    ui_text_wrap(x + 40, y + 8, w - 48, buf, PAL_RAMP(RAMP_WOOD, 1), 1);
    ui_text(x + 40, y + h - 14, (uint32_t)cost <= doomo_town.money ? STR_ICON_A " Buy & cook  " STR_ICON_B " No"
                                                                   : "Not enough H$!  " STR_ICON_B " Back",
            (uint32_t)cost <= doomo_town.money ? PAL_PAINT(PAINT_FOREST, 1) : PAL_PAINT(PAINT_CHERRY, 1));
}

static void step_banner(void)
{
    char buf[24];
    int s = ui_pop_scale(K.t), sc = fc_big() ? 3 : 2;
    doomo_snprintf(buf, sizeof(buf), "STEP %d", K.step + 1);
    fc_textc(stage_x(), counter_y() - (fc_big() ? 84 : 62), buf, PAL_WHITE);
    if (s > 100) {
        int tw = ui_text_big_width(step_name(K.step), sc) * DOOMO_MIN(s, 256) / 256;
        ui_text_big(stage_x() - tw / 2, counter_y() - (fc_big() ? 70 : 50) - (s > 256 ? 2 : 0), step_name(K.step),
                    s < 200 ? sc - 1 : sc, PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE);
    }
}

/* the apparatus of each step, sitting on the counter at stage_x() */
static void draw_board(int t)
{
    const Recipe *rc = RC();
    int bx, by, bw, i, kx = K.kx16 / 16, n = K.ncuts;
    uint8_t c = rc->c1;
    chop_geom(&bx, &by, &bw);
    ui_round_rect(bx - 2, by - 2, bw + 4, 18, 5, PAL_RAMP(RAMP_WOOD, 2));
    ui_round_rect(bx, by, bw, 14, 4, PAL_RAMP(RAMP_WOOD, 5));
    plat_rect(bx + 3, by + 1, bw - 6, 1, PAL_RAMP(RAMP_WOOD, 6));
    /* the ingredient as a log of pieces; cut pieces slide apart */
    {
        int x0 = bx + 10, x1 = bx + bw - 10, prev = x0, shift = 0;
        for (i = 0; i <= n; i++) {
            int xe = i < n ? K.cut_x[i] : x1, cut = i < n && K.cut_state[i] == 1;
            int px = prev + shift, pw = xe - prev - 1;
            if (pw > 2) {
                ui_round_rect(px - 1, by - 9, pw + 2, 13, 4, ui_darker(ui_darker(c)));
                ui_round_rect(px, by - 8, pw, 11, 3, c);
                plat_rect(px + 2, by - 7, DOOMO_MAX(1, pw - 4), 2, ui_lighter(c));
                plat_rect(px + 1, by + 1, pw - 2, 1, ui_darker(c));
            }
            if (cut) shift += 2;
            prev = xe;
        }
        for (i = 0; i < n; i++)
            if (K.cut_state[i] == 0) {
                int d;
                for (d = 0; d < 4; d++) plat_rect(K.cut_x[i], by - 12 + d * 4, 1, 2, PAL_WHITE);
            }
    }
    /* the cleaver */
    {
        int up = K.slam_t ? (6 - K.slam_t) * 2 : 14 + ui_wave(t * 3, 20, 2);
        int ky = by - 10 - up;
        plat_rect(kx - 1, ky - 18, 3, 9, PAL_RAMP(RAMP_WOOD, 3));               /* handle */
        plat_rect(kx, ky - 18, 1, 9, PAL_RAMP(RAMP_WOOD, 5));
        ui_round_rect(kx - 7, ky - 10, 15, 11, 2, PAL_RAMP(RAMP_STEEL, 2));
        ui_round_rect(kx - 6, ky - 9, 13, 9, 2, PAL_RAMP(RAMP_STEEL, 5));
        plat_rect(kx - 6, ky - 1, 13, 1, PAL_WHITE);                                /* edge */
        ui_fill_circle(kx + 3, ky - 6, 1, PAL_RAMP(RAMP_STEEL, 2));
        plat_rect(kx, by - 12, 1, 1, PAL_PAINT(PAINT_CHERRY, 1));
    }
}

static void draw_bowl(int t)
{
    const Recipe *rc = RC();
    int cx = stage_x(), cy = counter_y() - 12, rx = fc_big() ? 40 : 30, ry = rx / 3, k;
    int lo = 30 + diff() * 4, hi = 80 - diff() * 3, sp = K.speed16 / 16;
    static const char *const arrows[4] = { STR_ICON_ARROW_U, STR_ICON_ARROW_R, STR_ICON_ARROW_D, STR_ICON_ARROW_L };
    /* bowl */
    ui_fill_ellipse(cx, cy + ry, rx + 1, ry + 6, PAL_RAMP(RAMP_SKY, 3));
    ui_fill_ellipse(cx, cy + ry, rx, ry + 5, PAL_RAMP(RAMP_SKY, 6));
    plat_rect(cx - rx, cy, rx * 2 + 1, ry, PAL_RAMP(RAMP_SKY, 6));
    ui_fill_ellipse(cx, cy, rx + 1, ry + 1, PAL_RAMP(RAMP_SKY, 3));
    ui_fill_ellipse(cx, cy, rx - 1, ry, rc->c1);
    /* the swirl */
    for (k = 0; k < 10; k++) {
        int a = (K.angle + k * 300) & DOOMO_ANG_MASK, rr = rx * (10 - k) / 12;
        int x = cx + FIX2INT(fx_cos(a) * rr), y = cy + FIX2INT(fx_sin(a) * (rr / 3));
        plat_rect(x, y, 2, 1, ui_lighter(rc->c1));
    }
    /* the spoon */
    {
        int a = K.angle, x = cx + FIX2INT(fx_cos(a) * (rx * 2 / 3)), y = cy + FIX2INT(fx_sin(a) * (ry * 2 / 3));
        plat_line(x, y, x + 6, y - 26, PAL_RAMP(RAMP_WOOD, 3));
        plat_line(x + 1, y, x + 7, y - 26, PAL_RAMP(RAMP_WOOD, 5));
        ui_fill_ellipse(x, y, 3, 2, PAL_RAMP(RAMP_WOOD, 4));
    }
    /* the speed gauge */
    {
        int gx = cx - 50, gy = fc_big() ? 34 : 28, gw = 100, mk = gw * DOOMO_MIN(sp, 100) / 100;
        ui_window(gx - 8, gy - 6, gw + 16, 32, WIN_CREAM);
        plat_rect(gx, gy, gw, 5, PAL_RAMP(RAMP_STONE, 3));
        plat_rect(gx + gw * lo / 100, gy, gw * (hi - lo) / 100, 5, PAL_PAINT(PAINT_MINT, 1));
        plat_rect(gx + gw * hi / 100, gy, gw - gw * hi / 100, 5, PAL_PAINT(PAINT_CHERRY, 2));
        ui_heart(gx + mk, gy + 2 + ui_wave(t * 3, 20, 1), 3, sp >= lo && sp <= hi ? PAL_PAINT(PAINT_MINT, 0) : PAL_PAINT(PAINT_CHERRY, 1));
        fc_textc(cx, gy + 14, sp > hi ? "Too fast!" : sp < lo ? "Faster!" : "Yum! Just right!",
                 sp > hi ? PAL_PAINT(PAINT_CHERRY, 2) : sp < lo ? PAL_WHITE : PAL_UI_GOLD);
        /* the next direction, bouncing */
        ui_text_big(cx + rx + 8, cy - 18 - ui_wave(t * 4, 20, 2), arrows[K.next_dir], 2, PAL_UI_GOLD, PAL_RAMP(RAMP_WOOD, 1));
        /* time left */
        ui_bar(gx, gy + 7, gw, 4, STIR_TICKS - K.t, STIR_TICKS, PAL_PAINT(PAINT_SKY, 1));
    }
}

static void draw_oven(int t)
{
    const Recipe *rc = RC();
    int cx = stage_x(), w = fc_big() ? 96 : 76, h = fc_big() ? 70 : 54, x = cx - w / 2, y = counter_y() - h + 6;
    int sc = heat() == HEAT_SCORCH, c = sc ? 950 : 740, hw = sc ? 50 : 130 - diff() * 18;
    int burnt = !sc && K.done > c + hw;
    ui_round_rect(x - 1, y - 1, w + 2, h + 2, 6, PAL_PAINT(PAINT_CHERRY, 0));
    ui_round_rect(x, y, w, h, 6, PAL_PAINT(PAINT_PEACH, 1));
    plat_rect(x + 4, y + 2, w - 8, 1, PAL_PAINT(PAINT_PEACH, 2));
    /* dials */
    ui_fill_circle(x + 9, y + 8, 3, PAL_RAMP(RAMP_STEEL, 3));
    ui_fill_circle(x + 19, y + 8, 3, PAL_RAMP(RAMP_STEEL, 3));
    plat_rect(x + 9, y + 6, 1, 3, PAL_WHITE);
    ui_heart(x + w - 10, y + 8, 3, (t / 10) & 1 ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(PAINT_CHERRY, 2));
    /* the window glows; the dish rises and browns inside */
    {
        int wx = x + 8, wy = y + 16, ww = w - 16, wh = h - 24, glow = (t / 6) & 1;
        ui_round_rect(wx - 1, wy - 1, ww + 2, wh + 2, 4, PAL_RAMP(RAMP_STONE, 1));
        ui_round_rect(wx, wy, ww, wh, 3, glow ? PAL_RAMP(RAMP_LAVA, 3) : PAL_RAMP(RAMP_LAVA, 2));
        plat_rect(wx + 2, wy + wh - 3, ww - 4, 1, PAL_RAMP(RAMP_LAVA, 6));
        dish_draw(K.recipe, wx + ww / 2, wy + wh / 2 + 2, DOOMO_MAX(6, (ww / 4) * (600 + K.done / 2) / 1000), t,
                  burnt || (sc && K.done > 600));
        plat_rect(wx + 3, wy + 2, 2, wh - 6, PAL_WHITE);
    }
    /* the doneness bar: raw ... golden ... burnt */
    {
        int bx = cx - 50, by = fc_big() ? 36 : 30, bw = 100, k;
        ui_window(bx - 8, by - 8, bw + 16, 22, WIN_CREAM);
        for (k = 0; k < bw; k += 4) {
            int v = k * 1000 / bw;
            uint8_t col = v < c - hw ? PAL_RAMP(RAMP_CANDY, 7 - DOOMO_MIN(3, v / 200))
                        : v <= c + hw ? PAL_UI_GOLD : PAL_RAMP(RAMP_STONE, 1);
            plat_rect(bx + k, by, 4, 5, col);
        }
        ui_frame(bx + bw * (c - hw) / 1000, by - 1, bw * 2 * hw / 1000 + 1, 7, PAL_WHITE);
        plat_rect(bx + bw * K.done / 1000, by - 2, 1, 9, PAL_PAINT(PAINT_CHERRY, 1));
        ui_star(bx + bw * K.done / 1000, by - 4, 3, PAL_UI_GOLD);
    }
    (void)rc;
}

static void draw_pan(int t)
{
    const Recipe *rc = RC();
    int cx = stage_x(), cy = counter_y() - 6, rx = fc_big() ? 34 : 26, k;
    int lo = 60 + diff(), hi = 86 - diff() * 2, b = K.brown / 16;
    /* burner flames */
    for (k = -2; k <= 2; k++) {
        int fh = 3 + ((t / 3 + k * 2) & 3);
        plat_rect(cx + k * rx / 3, cy + 6 - fh, 2, fh, (k & 1) ? PAL_RAMP(RAMP_LAVA, 6) : PAL_PAINT(PAINT_SKY, 1));
    }
    /* the pan */
    plat_rect(cx + rx, cy - 2, rx, 4, PAL_RAMP(RAMP_STONE, 1));
    plat_rect(cx + rx, cy - 2, rx, 1, PAL_RAMP(RAMP_STONE, 3));
    ui_fill_ellipse(cx, cy, rx + 1, rx / 4 + 2, PAL_RAMP(RAMP_STONE, 0));
    ui_fill_ellipse(cx, cy - 1, rx, rx / 4, PAL_RAMP(RAMP_STONE, 2));
    /* the food: flies up and turns over when flipped */
    {
        int fy = cy - 3, fry = rx / 5 + 1, frx = rx * 2 / 3;
        uint8_t top = rc->c1, bot = rc->c1;
        int k2;
        for (k2 = 0; k2 < DOOMO_MIN(3, b / 30); k2++) bot = ui_darker(bot);
        if (K.flipping) {
            int j = 20 - K.flipping;
            fy -= j * (20 - j) / 4;
            fry = DOOMO_MAX(1, fry * DOOMO_MAX(1, (j < 10 ? 10 - j : j - 10)) / 10);
            top = (j < 10) ? top : ui_darker(rc->c1);
        }
        ui_fill_ellipse(cx, fy, frx + 1, fry + 1, ui_darker(ui_darker(rc->c1)));
        ui_fill_ellipse(cx, fy, frx, fry, top);
        if (!K.flipping) plat_rect(cx - frx + 2, fy + fry - 1, frx * 2 - 4, 1, bot);
        plat_rect(cx - frx / 2, fy - fry + 1, frx / 2, 1, ui_lighter(top));
    }
    /* the browning meter */
    {
        int gx = cx - 50, gy = fc_big() ? 34 : 28, gw = 100;
        char buf[16];
        ui_window(gx - 8, gy - 6, gw + 16, 28, WIN_CREAM);
        plat_rect(gx, gy, gw, 5, PAL_RAMP(RAMP_CANDY, 6));
        plat_rect(gx + gw * lo / 100, gy, gw * (hi - lo) / 100, 5, PAL_UI_GOLD);
        plat_rect(gx + gw * hi / 100, gy, gw - gw * hi / 100, 5, PAL_RAMP(RAMP_STONE, 1));
        plat_rect(gx + gw * DOOMO_MIN(b, 100) / 100, gy - 2, 1, 9, PAL_PAINT(PAINT_CHERRY, 1));
        ui_heart(gx + gw * DOOMO_MIN(b, 100) / 100, gy - 4, 2, PAL_PAINT(PAINT_CHERRY, 1));
        doomo_snprintf(buf, sizeof(buf), "Flip %d of 3", DOOMO_MIN(K.flips + 1, 3));
        fc_textc(cx, gy + 9, buf, PAL_UI_GOLD);
    }
}

static void draw_pot(int t)
{
    const Recipe *rc = RC();
    int cx = stage_x(), w = fc_big() ? 64 : 50, h = fc_big() ? 40 : 30, x = cx - w / 2, y = counter_y() - h;
    int lo = 45 + diff() * 2, hi = 72 - diff() * 2, tp = K.temp16 / 16, k;
    /* flame grows with the heat */
    for (k = -2; k <= 2; k++) {
        int fh = 2 + tp / 20 + ((t / 3 + k) & 1) + (down(BTN_A) ? 3 : 0);
        plat_rect(cx + k * w / 6, y + h + 6 - fh, 3, fh, (k & 1) ? PAL_RAMP(RAMP_LAVA, 5) : PAL_RAMP(RAMP_LAVA, 7));
    }
    /* the pot, contents bubbling, lid rattling when hot */
    ui_round_rect(x - 1, y - 1, w + 2, h + 2, 5, PAL_RAMP(RAMP_STEEL, 1));
    ui_round_rect(x, y, w, h, 5, PAL_RAMP(RAMP_STEEL, 4));
    plat_rect(x + 3, y + 3, 2, h - 6, PAL_RAMP(RAMP_STEEL, 6));
    plat_rect(x - 5, y + 5, 5, 3, PAL_RAMP(RAMP_STEEL, 2));
    plat_rect(x + w, y + 5, 5, 3, PAL_RAMP(RAMP_STEEL, 2));
    ui_fill_ellipse(cx, y + 1, w / 2 - 2, 3, rc->c1);
    for (k = 0; k < 3; k++) {
        int p = (t * (1 + tp / 30) + k * 11) % 24, bx = cx - w / 3 + k * w / 3;
        if (tp > 20) ui_fill_circle(bx, y - p / 6, 1 + (tp > 60) + p / 12, ui_lighter(rc->c1));
    }
    {
        int rattle = tp > 80 ? ((t & 2) ? -2 : 0) : 0;
        ui_fill_ellipse(cx + (tp > 80 ? ((t & 4) ? 2 : -2) : 0), y - 5 + rattle, w / 2, 3, PAL_RAMP(RAMP_STEEL, 3));
        ui_fill_circle(cx, y - 9 + rattle, 2, PAL_PAINT(PAINT_CHERRY, 1));
    }
    if (tp > 85) kitchen_steam(cx, y - 10, t, 4);
    /* thermometer */
    {
        int gx = cx + w / 2 + 16, gy = counter_y() - 66, gh = 56, lvl = gh * DOOMO_MIN(tp, 100) / 100;
        ui_round_rect(gx - 4, gy - 3, 11, gh + 6, 4, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(gx - 2, gy, 7, gh, PAL_RAMP(RAMP_STONE, 5));
        plat_rect(gx - 2, gy + gh - gh * hi / 100, 7, gh * (hi - lo) / 100, PAL_PAINT(PAINT_MINT, 1));
        plat_rect(gx, gy + gh - lvl, 3, lvl, PAL_PAINT(PAINT_CHERRY, 1));
        ui_fill_circle(gx + 1, gy + gh + 3, 4, PAL_PAINT(PAINT_CHERRY, 1));
        fc_text(gx + 9, gy + gh - lvl - 4, tp > hi ? "HOT!" : tp < lo ? "cold" : "purr", tp > hi ? PAL_PAINT(PAINT_CHERRY, 2) : PAL_WHITE);
        ui_bar(cx - 40, counter_y() + 12, 80, 4, SIMMER_TICKS - K.t, SIMMER_TICKS, PAL_PAINT(PAINT_SKY, 1));
    }
}

static void draw_cooking(void)
{
    int t = K.t, mood = GRAN_TALK;
    char buf[48];
    kitchen_draw_back((uint32_t)t, 1);
    if (K.st == CS_STEP_DONE) mood = K.score[K.step] >= 65 ? GRAN_CHEER : K.score[K.step] < 40 ? GRAN_DISMAY : GRAN_CALM;
    else if (K.st != CS_INTRO) mood = GRAN_CALM;
    granny_draw(gran_x(), counter_y() + 4, fc_big() ? 18 : 14, t, mood);
    kitchen_draw_counter(counter_y(), (uint32_t)t);
    doomo_snprintf(buf, sizeof(buf), "%s", RC()->name);
    title_bar(buf);
    switch (K.step) {
    case 0: if (K.st != CS_INTRO) draw_board(t); break;
    case 1: if (K.st != CS_INTRO) draw_bowl(t); break;
    default:
        if (K.st != CS_INTRO) {
            if (heat() == HEAT_FRY) draw_pan(t);
            else if (heat() == HEAT_SIMMER) draw_pot(t);
            else draw_oven(t);
        }
        break;
    }
    /* three step pips */
    {
        int k;
        for (k = 0; k < 3; k++) {
            int x = W() / 2 - 16 + k * 16, y = 22;
            uint8_t c = k < K.step || (k == K.step && K.st == CS_STEP_DONE) ? PAL_UI_GOLD
                      : k == K.step ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_RAMP(RAMP_STONE, 4);
            ui_fill_circle(x, y, 4, ui_darker(c));
            ui_fill_circle(x, y, 3, c);
        }
    }
    if (K.st == CS_INTRO) {
        step_banner();
        fc_bubble(4, fc_big() ? 30 : 24, stage_x() - (fc_big() ? 70 : 62), step_tip(), gran_x() + 8,
                  counter_y() - (fc_big() ? 44 : 34), K.t * 2);
    }
    fc_fx_draw(0, 0);
    switch (K.st) {
    case CS_CHOP: ui_hint(STR_ICON_A " Chop on the lines!", WIN_CUTE); break;
    case CS_STIR: ui_hint("Spin the D-pad clockwise!", WIN_CUTE); break;
    case CS_HEAT:
        ui_hint(heat() == HEAT_SIMMER ? "Hold " STR_ICON_A " for heat" : heat() == HEAT_FRY ? STR_ICON_A " Flip!" :
                heat() == HEAT_SCORCH ? "Wait for it... " STR_ICON_A " (if you dare)" : STR_ICON_A " Take it out!", WIN_CUTE);
        break;
    default: break;
    }
}

static void draw_result(void)
{
    int t = K.t, w = W(), h = H(), big = fc_big(), cx = w * 64 / 100, cy = h * 47 / 100;
    int it = cook_result_item(K.recipe, K.grade);
    char buf[48];
    static const char *const opts[3] = { "Again", "Serve", "Book" };
    int k;
    kitchen_draw_back((uint32_t)t, 1);
    granny_draw(gran_x(), counter_y() + 4, big ? 18 : 14, t,
                K.grade == GRADE_BURNT ? GRAN_DISMAY : K.grade >= GRADE_GREAT ? GRAN_CHEER : GRAN_TALK);
    kitchen_draw_counter(counter_y(), (uint32_t)t);
    title_bar(K.grade == GRADE_BURNT ? "Uh oh..." : "Order up!");
    /* a spotlight + rays behind the plate */
    for (k = 0; k < 8; k++) {
        int a = (t * 6 + k * 512) & DOOMO_ANG_MASK, r = big ? 70 : 52;
        plat_line(cx, cy, cx + FIX2INT(fx_cos(a) * r), cy + FIX2INT(fx_sin(a) * r / 2), PAL_RAMP(RAMP_CANDY, 6));
    }
    ui_fill_ellipse(cx, cy + 6, big ? 52 : 40, big ? 22 : 16, PAL_RAMP(RAMP_CANDY, 7));
    {
        int s = ui_pop_scale(t), r = (big ? 30 : 22) * s / 256;
        if (r > 4) dish_draw(K.recipe, cx, cy + ui_wave(t, 50, 1), r, t, K.grade == GRADE_BURNT);
    }
    if (t > 14) fc_grade_stamp(cx, cy - (big ? 54 : 40), K.grade, t - 14);
    doomo_snprintf(buf, sizeof(buf), "+%d %s", K.portions, doomo_items[it].name);
    fc_textc(cx, cy + (big ? 30 : 22), buf, PAL_WHITE);
    fc_bubble(4, big ? 28 : 22, cx - (big ? 80 : 62), grade_lines[K.grade <= GRADE_PERFECT ? K.grade : 0],
              gran_x() + 6, counter_y() - (big ? 44 : 34), t * 2);
    fc_fx_draw(0, 0);
    if (t >= 20) {
        int bw = big ? 70 : 56, bx = (w - (bw * 3 + 12)) / 2 + (big ? 20 : 14), by = counter_y() + (big ? 22 : 16);
        for (k = 0; k < 3; k++) ui_button(bx + k * (bw + 6), by, bw, opts[k], K.sel == k, WIN_CUTE);
    }
    ui_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " Choose  " STR_ICON_A " OK  " STR_ICON_B " Book", WIN_CUTE);
}

static void draw_counter(void)
{
    int t = K.t, w = W(), cy = counter_y(), i, big = fc_big(), n = 0;
    char buf[64];
    kitchen_draw_back((uint32_t)t, 1);
    for (i = 0; i < FC_CUSTOMERS; i++) {
        int id = cook_customer(i), x = seat_x(i), sel = i == K.cust_sel && K.st != CS_REACT;
        const DoomoResident *r;
        int expr = EXPR_HAPPY, served = cook_customer_served(i);
        if (id < 0) continue;
        n++;
        r = town_res(id);
        if (K.st == CS_REACT && K.react_cust == i)
            expr = K.react_kind == SERVE_FAV ? EXPR_LOVE : K.react_kind == SERVE_HATE ? EXPR_SICK : EXPR_HAPPY;
        else if (!served) expr = (t / 50 + i) % 3 == 0 ? EXPR_TALK : EXPR_NEUTRAL;
        /* stool */
        plat_rect(x - 10, cy - 8, 20, 4, PAL_PAINT(PAINT_CHERRY, 1));
        {
            const DoomoImage *img = portrait_image(id, expr);
            int bob = sel ? ui_wave(t * 2, 30, 1) : 0;
            plat_image(x - PORTRAIT_W / 2, cy - PORTRAIT_H + 8 - bob, img, 1, 0, 0);
        }
        if (!served) {
            /* the order bubble: what they're craving */
            const char *food = doomo_food_names[r->like_food % FOOD_COUNT];
            int bw = w / FC_CUSTOMERS - 2, lines = ui_text_wrap(0, 0, bw - 4, food, 0, 0);
            int bh = 6 + DOOMO_MAX(1, lines) * FONT_LINE;
            int by = cy - PORTRAIT_H + 2 - bh - ui_wave(t + i * 20, 60, 1);
            ui_round_rect(x - bw / 2, by, bw, bh, 5, sel ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_RAMP(RAMP_PASTEL_PINK, 4));
            ui_round_rect(x - bw / 2 + 1, by + 1, bw - 2, bh - 2, 4, PAL_WHITE);
            plat_rect(x - 2, by + bh - 1, 5, 2, PAL_WHITE);
            plat_rect(x - 1, by + bh + 1, 3, 1, PAL_WHITE);
            ui_heart(x - bw / 2 + 1, by + 1, 2, PAL_PAINT(PAINT_ROSE, 1));
            ui_text_wrap(x - bw / 2 + 3, by + 4, bw - 4, food, PAL_RAMP(RAMP_WOOD, 1), 1);
        } else {
            ui_heart(x + 14, cy - PORTRAIT_H + 6 - ((t / 6 + i) % 8), 3, PAL_PAINT(PAINT_ROSE, 1));
        }
        if (sel) ui_cursor(x - 6, cy + 14 + ui_wave(t * 3, 20, 1));
    }
    kitchen_draw_counter(cy, (uint32_t)t);
    for (i = 0; i < FC_CUSTOMERS; i++) {                 /* name plates on the counter */
        int id = cook_customer(i), x = seat_x(i);
        if (id < 0) continue;
        {
            char nm[DOOMO_NAME_LEN];
            int k, maxc = (w / FC_CUSTOMERS - 4) / FONT_ADVANCE;
            for (k = 0; k < maxc && k < DOOMO_NAME_LEN - 1 && doomo_town.res[id].name[k]; k++) nm[k] = doomo_town.res[id].name[k];
            nm[k] = 0;
            fc_textc(x, cy + 2, nm, cook_customer_served(i) ? PAL_RAMP(RAMP_STONE, 5) : PAL_WHITE);
        }
    }
    doomo_snprintf(buf, sizeof(buf), "The Counter  %d served", fc.served);
    title_bar("The Counter");
    if (!n) {
        fc_bubble(w / 2 - 70, cy - 60, 140, "No customers today... the island's a bit empty, dearie.", w / 2, cy, 999);
    }
    fc_fx_draw(0, 0);
    if (K.st == CS_PICK) {
        int vis = big ? 8 : 5, pw = big ? 200 : 168, ph = vis * 13 + 30, px = (w - pw) / 2, py = big ? 30 : 22;
        const DoomoResident *r = town_res(cook_customer(K.cust_sel));
        plat_shade_rect(0, 0, w, fc_h(), 10);
        if (ui_window_pop(px, py, pw, ph, WIN_CREAM, t)) {
            ui_text(px + 8, py + 5, "Serve what?", PAL_PAINT(PAINT_CHERRY, 0));
            if (!K.npicks) ui_text_wrap(px + 8, py + 20, pw - 16, "Your bag has no food! Cook something first.", PAL_RAMP(RAMP_WOOD, 2), 1);
            for (i = 0; i < vis && K.pick_top + i < K.npicks; i++) {
                int it = K.picks[K.pick_top + i], ry = py + 18 + i * 13, s = K.pick_top + i == K.pick_sel;
                int fi = cook_item_food(it);
                if (s) { fc_pill(px + 10, ry - 1, pw - 16, 12, PAL_RAMP(RAMP_PASTEL_PINK, 6)); ui_cursor(px + 2, ry + 1); }
                ui_text(px + 16, ry + 1, doomo_items[it].name, PAL_RAMP(RAMP_WOOD, 1));
                doomo_snprintf(buf, sizeof(buf), "x%d", inv_count(it));
                ui_text(px + pw - 12 - ui_text_width(buf), ry + 1, buf, PAL_RAMP(RAMP_WOOD, 2));
                if (r && fi >= 0 && fi == r->like_food) ui_heart(px + pw - 40, ry + 5, 3, PAL_PAINT(PAINT_ROSE, 1));
                if (r && fi >= 0 && fi == r->hate_food) ui_text(px + pw - 44, ry + 1, STR_ICON_SKULL, PAL_RAMP(RAMP_STONE, 3));
            }
            if (K.npicks > vis)
                ui_scroll_arrows(px + pw - 6, py + 16, py + 16 + vis * 13, K.pick_top > 0, K.pick_top + vis < K.npicks);
        }
        ui_hint(STR_ICON_A " Serve  " STR_ICON_B " Back", WIN_CREAM);
    } else if (K.st == CS_REACT) {
        int x = seat_x(K.react_cust), bw = big ? 170 : 140;
        int bx = DOOMO_CLAMP(x - bw / 2, 4, w - bw - 4);
        fc_bubble(bx, big ? 26 : 18, bw, K.line, x, cy - PORTRAIT_H + 8, t * 2);
        ui_hint(STR_ICON_A " OK", WIN_CUTE);
    } else {
        ui_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " Customer  " STR_ICON_A " Serve  " STR_ICON_B " Back", WIN_CUTE);
    }
}

static void cook_draw(void)
{
    if (plat_draw_skipped()) return;
    switch (K.st) {
    case CS_BOOK: draw_book(); fc_fx_draw(0, 0); break;
    case CS_BUY: draw_book(); draw_buy(); break;
    case CS_RESULT: draw_result(); break;
    case CS_COUNTER: case CS_PICK: case CS_REACT: draw_counter(); break;
    default: draw_cooking(); break;
    }
}

const DoomoScene SCENE_COOKING = {
    "COOKING", cook_enter, cook_leave, cook_update, cook_draw, 0
};

/* ---- test hooks ------------------------------------------------------------------- */
int cook_scene_state(void) { return K.st; }
int cook_scene_grade(void) { return K.grade; }
int cook_scene_score(int step) { return step >= 0 && step < 3 ? K.score[step] : 0; }
int cook_scene_recipe(void) { return K.recipe; }
