/* fish_scene.c - SCENE_FISHING: the Lake of Fire (owner: fishcook).
 *
 *   IDLE   on the pier (L/R bait, START menu, B leave)
 *   CHARGE hold A: the power meter swings; let go to cast (a PERFECT
 *          cast at the top of the swing lures rarer fish)
 *   CAST   the bobber arcs out and plops in
 *   WAIT   it bobs; a shadow circles; nibbles are fake-outs (pressing A
 *          now spooks the fish)
 *   BITE   SPLOOSH! "!" - press A in time to hook it
 *   REEL   hold A to reel, let go when it runs or jumps. Keep the tension
 *          in the sweet zone (double speed) and below the snapping point.
 *          Strong fish tire out.
 *   LANDED / CARD  the catch card: name, size, rarity, NEW / RECORD; keep
 *          it in the cooler (for cooking), sell it, or let it go
 *   MENU / DEX / COOLER  the Fishdex, the cooler (sell), rods
 *
 * Args (fishcook_int.h): FISH_ARG_DEX / COOLER open those pages,
 * FISH_ARG_AUTOPLAY lets a bot fish, FISH_ARG_NIGHT sets the clock to
 * 23:00, FISH_ARG_FORCE(sp) makes every bite species sp (screenshots).
 * State: FishState in the scene scratch (with the shared particle pool),
 * so the lake costs no .bss while you're elsewhere. */
#include "fishcook_int.h"

enum { FS_IDLE, FS_CHARGE, FS_CAST, FS_WAIT, FS_BITE, FS_REEL, FS_LANDED,
       FS_CARD, FS_LOST, FS_MENU, FS_DEX, FS_COOLER };
enum { FISH_REST, FISH_RUN };

#define DIST_START 1600           /* 1/16 units: 100 = the cast point      */
#define DIST_MAX   2600           /* line runs out                         */
#define T_ZONE_LO  350
#define T_ZONE_HI  760
#define T_BREAK    1000

typedef struct {
    uint8_t  st, prev, autoplay, opened, force, perfect, menu_sel, card_sel;
    uint8_t  sp, fstate, jump, exhausted, flags, nibbles, shake, landed_flags;
    int16_t  t, power, pdir, wait, nib_t, fish_t, size, value;
    int16_t  land_x, land_y, bob_x, bob_y, tip_x, tip_y, shadow_off;
    int16_t  dist, dist0, tension, stamina, zone_ticks, dex_sel, dex_top, cool_sel;
    int16_t  catches;                 /* this visit                         */
    uint32_t bot_now, bot_prev;
    const char *msg;                  /* lost / info banner                 */
    LakeView v;
} FishState;
DOOMO_STATIC_ASSERT(sizeof(FishState) <= FC_STATE_MAX, fish_state_fits_scratch);

/* The state lives in the scene scratch (fc_scratch_claim); fs points at
 * it while the scene runs. Only the music to restore stays in .bss. */
static FishState *fs;
static uint8_t fish_music;
#define F (*fs)
#define V (fs->v)

/* ---- input (the pad, or the bot) ------------------------------------------- */
static int down(uint32_t b) { return F.autoplay ? (F.bot_now & b) != 0 : btn_down(b); }
static int pressed(uint32_t b)
{
    if (F.autoplay) return (F.bot_now & b) && !(F.bot_prev & b);
    return btn_pressed(b);
}
static int repeat(uint32_t b) { return F.autoplay ? pressed(b) : btn_repeat(b); }

static void go(int st)
{
    F.prev = F.st;
    F.st = (uint8_t)st;
    F.t = 0;
}

static void say_lost(const char *msg)
{
    F.msg = msg;
    go(FS_LOST);
    audio_sfx(SFX_SAD);
}

/* ---- the bot ------------------------------------------------------------------ */
static uint32_t bot(void)
{
    switch (F.st) {
    case FS_IDLE:
        return F.t > 20 ? BTN_A : 0;
    case FS_CHARGE:
        /* let go near the top of the swing */
        return (F.power >= 90 && F.pdir < 0) ? 0 : BTN_A;
    case FS_BITE:
        return F.t >= 4 ? BTN_A : 0;
    case FS_REEL: {
        int run = F.fstate == FISH_RUN && !F.exhausted;
        if (F.jump) return 0;
        if (run) return F.tension < 240 ? BTN_A : 0;
        return F.tension < 690 ? BTN_A : 0;
    }
    case FS_CARD:
        return F.t > 40 && !(F.bot_prev & BTN_A) ? BTN_A : 0;   /* keep it */
    default:
        return 0;
    }
}

/* ---- casting ------------------------------------------------------------------- */
static void land_point(void)
{
    int p = DOOMO_CLAMP(F.power, 0, 100);
    F.land_x = (int16_t)(V.pier_x1 + 28 + p * (V.w - V.pier_x1 - 48) / 100);
    F.land_y = (int16_t)((V.h - 22) - p * (V.h - 22 - (V.horizon + 10)) / 100);
}

static int rarer(int a, int b)
{
    int ra = fish_info[a].rarity, rb = fish_info[b].rarity;
    if (ra == FR_JUNK) ra = 0;
    if (rb == FR_JUNK) rb = 0;
    return rb > ra ? b : a;
}

static void roll_bite(void)
{
    int hour = town_hour(), bait = fc.bait, sp;
    if (!fish_bait_ok(bait)) bait = BAIT_NONE;
    sp = fish_roll_species(&fc_rng, hour, bait);
    if (F.perfect || F.power >= 80) sp = rarer(sp, fish_roll_species(&fc_rng, hour, bait));
    if (F.force) sp = F.force - 1;
    F.sp = (uint8_t)sp;
    F.size = (int16_t)fish_roll_size(&fc_rng, sp, bait);
    if (bait != BAIT_NONE && !opt_get(OPT_CHEAT_INFINITE_CANDY)) inv_take(fish_baits[bait].item, 1);
    F.wait = (int16_t)((45 + fc_rand(90)) * (100 - fish_baits[bait].speed) / 100);
    if (F.autoplay) F.wait = (int16_t)DOOMO_MIN(F.wait, 50);
    F.nibbles = (uint8_t)fc_rand(3);
    F.nib_t = (int16_t)(F.wait / (F.nibbles + 1));
    F.shadow_off = (int16_t)(40 + fc_rand(30));
}

static void start_reel(void)
{
    const FishInfo *f = &fish_info[F.sp];
    /* small fry start closer: the fight lasts about 3 s for a guppy and
     * 10 s for a legend (bot timings, test_fishcook.c) */
    F.dist = F.dist0 = (int16_t)(DIST_START * (66 + f->strength * 4) / 100);
    F.tension = 120;
    F.stamina = (int16_t)(60 + f->strength * 25);
    F.fstate = FISH_RUN;
    F.fish_t = 12;
    F.jump = 0;
    F.exhausted = 0;
    F.zone_ticks = 0;
    go(FS_REEL);
    audio_sfx(SFX_SPLASH);
    F.shake = 6;
    fc_burst(FCX_DROP, F.bob_x, F.bob_y, 10, 28, PAL_WHITE);
}

static void reel_tick(void)
{
    const FishInfo *f = &fish_info[F.sp];
    const FishRod *rod = &fish_rods[fc.rod < ROD_COUNT ? fc.rod : 0];
    int s = f->strength, pull, hold = down(BTN_A), zone, brk = T_BREAK + rod->tension * 10;
    /* the fish */
    if (--F.fish_t <= 0) {
        if (F.fstate == FISH_REST && !F.exhausted) {
            F.fstate = FISH_RUN;
            F.fish_t = (int16_t)(f->style == FSTY_DARTER ? 10 + fc_rand(14) :
                                 f->style == FSTY_SLEEPY ? 12 + fc_rand(10) : 18 + fc_rand(26));
            audio_sfx(SFX_SPLASH);
        } else {
            F.fstate = FISH_REST;
            F.fish_t = (int16_t)(f->style == FSTY_SLEEPY ? 60 + fc_rand(40) :
                                 f->style == FSTY_DARTER ? 22 + fc_rand(20) : 30 + fc_rand(40));
        }
    }
    if (F.fstate == FISH_RUN && f->style == FSTY_JUMPER && !F.jump && fc_rand(40) == 0) {
        F.jump = 26;
        audio_sfx(SFX_SPLASH);
    }
    if (F.jump) {
        F.jump--;
        if (F.jump == 24 || F.jump == 2) fc_burst(FCX_DROP, F.bob_x, F.bob_y, 5, 22, PAL_WHITE);
    }
    pull = (F.fstate == FISH_RUN) ? s * 3 + 4 : s;
    if (F.exhausted) pull /= 2;
    zone = F.tension >= T_ZONE_LO && F.tension <= T_ZONE_HI;
    if (hold) {
        int add = 4 + pull * 2 / 3;
        if (F.jump) add += 14;
        if (f->style == FSTY_THRASHER && F.fstate == FISH_RUN) add += fc_rand(8);
        F.tension = (int16_t)(F.tension + add);
        F.dist = (int16_t)(F.dist - rod->reel * (zone ? 2 : 1) + (F.fstate == FISH_RUN ? pull / 3 : 0));
        F.stamina = (int16_t)(F.stamina - (zone ? 2 : 1));
        if (zone) F.zone_ticks++;
        if ((F.t & 3) == 0) audio_sfx(SFX_BLIP);
    } else {
        F.tension = (int16_t)DOOMO_MAX(0, F.tension - 9);
        if (F.fstate == FISH_RUN) F.dist = (int16_t)(F.dist + pull / 2);
    }
    if (F.stamina <= 0 && !F.exhausted) {
        F.exhausted = 1;
        F.fstate = FISH_REST;
        F.fish_t = 999;
        fc_popup(F.bob_x, F.bob_y - 12, "Tired!", PAL_PAINT(PAINT_SKY, 2));
    }
    /* juice */
    if (F.fstate == FISH_RUN && (F.t & 1)) fc_fx(FCX_DROP, F.bob_x - 3 + fc_rand(7), F.bob_y, fc_rand(17) - 8, -10 - fc_rand(10), PAL_WHITE, 12);
    if (zone && hold && (F.t % 6) == 0) fc_fx(FCX_SPARK, V.w - 14, 30 + fc_rand(10), 0, -6, PAL_UI_GOLD, 14);
    if (F.tension >= brk) {
        F.shake = 10;
        fc_burst(FCX_STAR, F.tip_x, F.tip_y, 8, 24, PAL_UI_GOLD);
        say_lost("SNAP! The line broke...");
        return;
    }
    if (F.dist >= DIST_MAX) { say_lost("It swam off with your bait!"); return; }
    if (F.dist <= 0) {
        F.landed_flags = (uint8_t)fish_register_catch(F.sp, F.size);
        F.value = (int16_t)fish_value(F.sp, F.size);
        F.catches++;
        go(FS_LANDED);
        audio_sfx(SFX_SPLASH);
        fc_burst(FCX_DROP, F.bob_x, F.bob_y, 14, 34, PAL_WHITE);
    }
}

/* ---- scene ---------------------------------------------------------------------- */
/* (Re)claim the scratch: a fresh state on the pier. */
static int fish_claim(void)
{
    fs = (FishState *)fc_scratch_claim(sizeof(FishState));
    return fs != NULL;
}

static void fish_enter(int arg)
{
    fish_music = (uint8_t)audio_music_current();
    if (!fish_claim()) return;
    F.autoplay = (arg & FISH_ARG_AUTOPLAY) != 0;
    F.force = (uint8_t)((arg >> 8) & 0xFF);
    if (F.force > FISH_COUNT) F.force = 0;
    if (arg & FISH_ARG_NIGHT) doomo_town.minute = 23 * 60;
    if (arg & FISH_ARG_DEMO) {                  /* debug: a well-used tackle box */
        int sp;
        for (sp = 0; sp < FISH_COUNT; sp += 1 + (sp % 3 == 2)) {
            fc.caught[sp] = (uint8_t)(1 + sp % 4);
            fc.record[sp] = (uint16_t)fish_info[sp].size_max;
            fc.cooler[sp] = (uint8_t)(sp % 5);
            unlock_grant(FC_UL_DEX(sp));
        }
        ui_toast_clear();
    }
    rng_seed(&fc_rng, doomo_town.seed ^ (doomo_town.day * 977u) ^ doomo_town.minute ^ 0xF1500Du);
    fc_fx_clear();
    if (!fish_rod_ok(fc.rod)) fc.rod = ROD_TWIG;
    if (fc.bait >= BAIT_COUNT) fc.bait = BAIT_NONE;
    F.opened = (uint8_t)(arg & FISH_ARG_MODE_MASK);
    if (F.opened == FISH_ARG_DEX) go(FS_DEX);
    else if (F.opened == FISH_ARG_COOLER) go(FS_COOLER);
    else go(FS_IDLE);
    lake_layout(&V);
    audio_music(V.night ? SONG_HAHALAND_NIGHT : SONG_HAHALAND_DAY);
}

static void fish_leave(void)
{
    audio_music(fish_music);
}

/* Another scene used the scratch while this one was underneath: start
 * over on the pier (fish on the line are lost, the cooler is saved). */
static int fish_resume(void)
{
    if (fs && fc_scratch_mine()) return 1;
    if (!fish_claim()) return 0;
    go(FS_IDLE);
    lake_layout(&V);
    return 1;
}

static void leave_scene(void)
{
    audio_sfx(SFX_CANCEL);
    scene_pop();
}

static void card_choice(int c)
{
    const FishInfo *f = &fish_info[F.sp];
    char buf[24];
    int cx = V.w / 2, cy = V.h / 2;
    switch (c) {
    case 0:
        fish_keep(F.sp);
        doomo_snprintf(buf, sizeof(buf), "Cooler x%d", fc.cooler[F.sp]);
        fc_popup(cx, cy, buf, PAL_PAINT(PAINT_SKY, 2));
        audio_sfx(SFX_PICKUP);
        break;
    case 1:
        doomo_snprintf(buf, sizeof(buf), "+%d H$", fish_sell(F.sp, F.size));
        fc_popup(cx, cy, buf, PAL_UI_GOLD);
        fc_burst(FCX_COIN, cx, cy, 8, 30, PAL_UI_GOLD);
        audio_sfx(SFX_PICKUP);
        break;
    default:
        fish_release(F.sp);
        fc_popup(cx, cy, f->rarity == FR_JUNK ? "Yeet!" : "Bye bye!", PAL_PAINT(PAINT_BUBBLEGUM, 2));
        fc_burst(FCX_HEART, cx, cy, 6, 20, PAL_PAINT(PAINT_ROSE, 1));
        audio_sfx(SFX_SPLASH);
        break;
    }
    go(FS_IDLE);
}

/* rods you own, cycled on the menu */
static void next_rod(int d)
{
    int i, r = fc.rod;
    for (i = 0; i < ROD_COUNT; i++) {
        r = (r + d + ROD_COUNT) % ROD_COUNT;
        if (fish_rod_ok(r)) { fc.rod = (uint8_t)r; return; }
    }
}

static void next_bait(int d)
{
    int i, b = fc.bait;
    for (i = 0; i < BAIT_COUNT; i++) {
        b = (b + d + BAIT_COUNT) % BAIT_COUNT;
        if (fish_bait_ok(b)) { fc.bait = (uint8_t)b; audio_sfx(SFX_CURSOR); return; }
    }
}

/* species kept in the cooler, in Fishdex order */
static int cooler_list(uint8_t *out)
{
    int sp, n = 0;
    for (sp = 0; sp < FISH_COUNT; sp++) if (fc.cooler[sp]) out[n++] = (uint8_t)sp;
    return n;
}

static void fish_update(void)
{
    if (!fish_resume()) return;
    F.bot_prev = F.bot_now;
    F.bot_now = F.autoplay ? bot() : 0;
    F.t++;
    if (F.shake) F.shake--;
    lake_layout(&V);
    fc_fx_update();
    if (F.autoplay && btn_pressed(BTN_B)) F.autoplay = 0;      /* take over */
    switch (F.st) {
    case FS_IDLE:
        if (pressed(BTN_A)) { go(FS_CHARGE); F.power = 0; F.pdir = 1; audio_sfx(SFX_SELECT); }
        else if (repeat(BTN_L)) next_bait(-1);
        else if (repeat(BTN_R)) next_bait(1);
        else if (pressed(BTN_START)) { go(FS_MENU); F.menu_sel = 0; audio_sfx(SFX_SELECT); }
        else if (pressed(BTN_B)) leave_scene();
        if (!fish_bait_ok(fc.bait)) fc.bait = BAIT_NONE;
        break;
    case FS_CHARGE:
        F.power = (int16_t)(F.power + F.pdir * 3);
        if (F.power >= 100) { F.power = 100; F.pdir = -1; }
        if (F.power <= 0) { F.power = 0; F.pdir = 1; }
        if (!down(BTN_A)) {
            F.perfect = F.power >= 92;
            land_point();
            go(FS_CAST);
            audio_sfx(SFX_CURSOR);
            if (F.perfect) {
                fc_popup(V.pier_x1, V.pier_y - 60, "PERFECT CAST!", PAL_UI_GOLD);
                audio_sfx(SFX_SPARKLE);
            }
        }
        break;
    case FS_CAST:
        if (F.t >= 18) {
            F.bob_x = F.land_x;
            F.bob_y = F.land_y;
            roll_bite();
            go(FS_WAIT);
            audio_sfx(SFX_SPLASH);
            fc_burst(FCX_DROP, F.bob_x, F.bob_y, 8, 22, PAL_WHITE);
            fc_fx(FCX_RING, F.bob_x, F.bob_y + 1, 0, 0, PAL_WHITE, 20);
        }
        break;
    case FS_WAIT:
        if (F.nibbles && F.t > 0 && (F.t % F.nib_t) == 0 && F.t < F.wait) {
            F.nibbles--;
            audio_sfx(SFX_BUBBLE);
            fc_fx(FCX_RING, F.bob_x, F.bob_y + 1, 0, 0, PAL_WHITE, 14);
        }
        if (pressed(BTN_A)) { say_lost("Too eager! It swam off."); break; }
        if (pressed(BTN_B)) { go(FS_IDLE); audio_sfx(SFX_CANCEL); break; }
        if (F.t >= F.wait) {
            go(FS_BITE);
            audio_sfx(SFX_SPLASH);
            fc_burst(FCX_DROP, F.bob_x, F.bob_y, 10, 26, PAL_WHITE);
        }
        break;
    case FS_BITE: {
        int window = DOOMO_MAX(9, 20 - fish_info[F.sp].rarity * 2);
        if (pressed(BTN_A)) start_reel();
        else if (F.t > window) say_lost("It stole the bait! Sneaky...");
        break; }
    case FS_REEL:
        reel_tick();
        break;
    case FS_LANDED:
        if (F.t >= 26) {
            go(FS_CARD);
            F.card_sel = 0;
            audio_sfx(F.landed_flags ? SFX_EVOLVE : SFX_HAPPY);
            if (F.landed_flags) fc_burst(FCX_CONFETTI, V.w / 2, V.h / 3, 18, 40, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        }
        break;
    case FS_CARD:
        if (F.t < 12) break;
        if (repeat(BTN_LEFT)) { F.card_sel = (uint8_t)((F.card_sel + 2) % 3); audio_sfx(SFX_CURSOR); }
        if (repeat(BTN_RIGHT)) { F.card_sel = (uint8_t)((F.card_sel + 1) % 3); audio_sfx(SFX_CURSOR); }
        if (pressed(BTN_A)) card_choice(F.card_sel);
        else if (pressed(BTN_B)) card_choice(0);
        break;
    case FS_LOST:
        if (F.t > 50 || (F.t > 10 && pressed(BTN_A))) go(FS_IDLE);
        break;
    case FS_MENU:
        if (repeat(BTN_UP)) { F.menu_sel = (uint8_t)((F.menu_sel + 3) % 4); audio_sfx(SFX_CURSOR); }
        if (repeat(BTN_DOWN)) { F.menu_sel = (uint8_t)((F.menu_sel + 1) % 4); audio_sfx(SFX_CURSOR); }
        if (F.menu_sel == 2 && (repeat(BTN_LEFT) || repeat(BTN_RIGHT))) {
            next_rod(btn_down(BTN_LEFT) ? -1 : 1);
            audio_sfx(SFX_CURSOR);
        }
        if (pressed(BTN_A)) {
            audio_sfx(SFX_SELECT);
            switch (F.menu_sel) {
            case 0: go(FS_DEX); break;
            case 1: go(FS_COOLER); F.cool_sel = 0; break;
            case 2: next_rod(1); break;
            default: scene_pop(); return;
            }
        } else if (pressed(BTN_B) || pressed(BTN_START)) { go(FS_IDLE); audio_sfx(SFX_CANCEL); }
        break;
    case FS_DEX: {
        int cols = 6, old = F.dex_sel;
        if (repeat(BTN_LEFT)) F.dex_sel--;
        if (repeat(BTN_RIGHT)) F.dex_sel++;
        if (repeat(BTN_UP)) F.dex_sel = (int16_t)(F.dex_sel - cols);
        if (repeat(BTN_DOWN)) F.dex_sel = (int16_t)(F.dex_sel + cols);
        F.dex_sel = (int16_t)DOOMO_CLAMP(F.dex_sel, 0, FISH_COUNT - 1);
        if (F.dex_sel != old) audio_sfx(SFX_CURSOR);
        if (pressed(BTN_B)) {
            audio_sfx(SFX_CANCEL);
            if (F.opened == FISH_ARG_DEX) { scene_pop(); return; }
            go(FS_MENU);
        }
        break; }
    case FS_COOLER: {
        uint8_t list[FISH_COUNT];
        int n = cooler_list(list);
        if (repeat(BTN_UP) && F.cool_sel > 0) { F.cool_sel--; audio_sfx(SFX_CURSOR); }
        if (repeat(BTN_DOWN) && F.cool_sel < n - 1) { F.cool_sel++; audio_sfx(SFX_CURSOR); }
        F.cool_sel = (int16_t)DOOMO_CLAMP(F.cool_sel, 0, DOOMO_MAX(0, n - 1));
        if (n && (pressed(BTN_A) || pressed(BTN_R))) {
            char buf[16];
            int h = fish_sell_cooler(list[F.cool_sel], pressed(BTN_R) ? 99 : 1);
            doomo_snprintf(buf, sizeof(buf), "+%d H$", h);
            fc_popup(V.w / 2, V.h / 2, buf, PAL_UI_GOLD);
            fc_burst(FCX_COIN, V.w / 2, V.h / 2, 6, 26, PAL_UI_GOLD);
            audio_sfx(SFX_PICKUP);
        }
        if (pressed(BTN_B)) {
            audio_sfx(SFX_CANCEL);
            if (F.opened == FISH_ARG_COOLER) { scene_pop(); return; }
            go(FS_MENU);
        }
        break; }
    default: break;
    }
    /* the bobber's position follows the fight */
    if (F.st == FS_REEL || F.st == FS_LANDED) {
        int ex = V.pier_x1 + 10, ey = V.pier_y + 12, d = DOOMO_MAX(0, (int)F.dist);
        int wob = F.fstate == FISH_RUN ? ui_wave(F.t * 5, 30, fish_info[F.sp].style == FSTY_DARTER ? 6 : 3) : 0;
        F.bob_x = (int16_t)DOOMO_CLAMP(ex + (F.land_x - ex) * d / DOOMO_MAX(1, F.dist0) + wob, ex, V.w - 8);
        F.bob_y = (int16_t)DOOMO_CLAMP(ey + (F.land_y - ey) * d / DOOMO_MAX(1, F.dist0), V.horizon + 6, V.h - 6);
    }
}

/* ---- drawing ---------------------------------------------------------------------- */
static void draw_line(int x0, int y0, int x1, int y1, int sag, uint8_t c)
{
    int k, px = x0, py = y0;
    for (k = 1; k <= 10; k++) {
        int x = x0 + (x1 - x0) * k / 10, y = y0 + (y1 - y0) * k / 10 + sag * k * (10 - k) / 25;
        plat_line(px, py, x, y, c);
        px = x;
        py = y;
    }
}

static void hud(void)
{
    char buf[32], tm[12];
    int w = V.w;
    town_time_str(tm, sizeof(tm));
    doomo_snprintf(buf, sizeof(buf), "Lake of Fire  %s", tm);
    fc_pill(3, 3, ui_text_width(buf) + 12, 12, V.night ? PAL_RAMP(RAMP_VAPE, 4) : PAL_RAMP(RAMP_PASTEL_PINK, 6));
    ui_text(9, 5, buf, V.night ? PAL_WHITE : PAL_RAMP(RAMP_WOOD, 1));
    fc_money_pill(w - 3, 3);
    if (F.st == FS_IDLE || F.st == FS_CHARGE) {
        const FishBait *b = &fish_baits[fc.bait];
        if (fc.bait == BAIT_NONE) doomo_snprintf(buf, sizeof(buf), STR_ICON_ARROW_L "Bait: none" STR_ICON_ARROW_R);
        else doomo_snprintf(buf, sizeof(buf), STR_ICON_ARROW_L "%s x%d" STR_ICON_ARROW_R,
                            doomo_items[b->item].name, inv_count(b->item));
        fc_pill(3, 17, ui_text_width(buf) + 12, 12, PAL_RAMP(RAMP_MINT, 6));
        ui_text(9, 19, buf, PAL_RAMP(RAMP_MINT, 1));
        if (progress_get(PROG_FISH_CAUGHT) > 0 || F.catches > 0)       /* not under the tutorial */
            ui_text(9, 31, b->effect, V.night ? PAL_RAMP(RAMP_SKY, 6) : PAL_RAMP(RAMP_WOOD, 2));
    }
}

static void power_meter(void)
{
    int x = V.pier_x1 + 6, y = V.pier_y - 62, h = 46, fillh = h * F.power / 100, k;
    ui_round_rect(x - 1, y - 1, 9, h + 2, 3, PAL_RAMP(RAMP_STONE, 0));
    ui_round_rect(x, y, 7, h, 2, PAL_RAMP(RAMP_STONE, 2));
    for (k = 0; k < fillh; k++) {
        int lvl = k * 100 / h;
        uint8_t c = lvl >= 92 ? PAL_UI_GOLD : lvl > 60 ? PAL_PAINT(PAINT_TANGERINE, 1) : PAL_PAINT(PAINT_MINT, 1);
        plat_rect(x + 1, y + h - 1 - k, 5, 1, c);
    }
    plat_rect(x - 2, y + h - h * 92 / 100, 11, 1, PAL_WHITE);
    ui_star(x + 3, y - 5, 3, F.power >= 92 ? PAL_UI_GOLD : PAL_RAMP(RAMP_STONE, 4));
    fc_text(x + 11, y + h - fillh - 4, F.power >= 92 ? "MAX!" : "", PAL_UI_GOLD);
}

static void tension_meter(void)
{
    const FishRod *rod = &fish_rods[fc.rod < ROD_COUNT ? fc.rod : 0];
    int brk = T_BREAK + rod->tension * 10;
    int x = V.w - 18, y = 22, h = V.h - y - 28, k;
    int lvl = h * DOOMO_MIN((int)F.tension, brk) / brk;
    int zlo = h * T_ZONE_LO / brk, zhi = h * T_ZONE_HI / brk;
    int danger = F.tension > T_ZONE_HI;
    int jx = danger ? ((F.t & 1) ? 1 : -1) : 0;
    ui_round_rect(x - 2 + jx, y - 2, 14, h + 4, 5, PAL_RAMP(RAMP_STONE, 0));
    ui_round_rect(x - 1 + jx, y - 1, 12, h + 2, 4, PAL_RAMP(RAMP_STONE, 2));
    plat_rect(x + 1 + jx, y + h - zhi, 8, zhi - zlo, PAL_RAMP(RAMP_MINT, 5));      /* zone */
    for (k = 0; k < lvl; k++) {
        int r = k * brk / DOOMO_MAX(1, h);
        uint8_t c = r > T_ZONE_HI ? PAL_PAINT(PAINT_CHERRY, (k & 2) ? 1 : 2)
                  : r >= T_ZONE_LO ? PAL_PAINT(PAINT_MINT, 1) : PAL_PAINT(PAINT_SKY, 1);
        plat_rect(x + 2 + jx, y + h - 1 - k, 6, 1, c);
    }
    plat_rect(x + 2 + jx, y + h - lvl, 6, 1, PAL_WHITE);
    ui_heart(x + 5 + jx, y + h - lvl, 3, danger ? PAL_PAINT(PAINT_CHERRY, 1) : PAL_PAINT(PAINT_BUBBLEGUM, 1));
    fc_text(x - 8, y + h + 4, danger ? "EASY!" : (F.tension >= T_ZONE_LO ? "ZONE" : "REEL"),
            danger ? PAL_PAINT(PAINT_CHERRY, 2) : F.tension >= T_ZONE_LO ? PAL_UI_GOLD : PAL_WHITE);
    /* distance: fish icon swimming toward the pier icon */
    {
        int bx = V.w / 2 - 50, bw = 100, by = 22, p = DOOMO_CLAMP((int)F.dist, 0, DIST_MAX);
        int fxp = bx + bw * p / DIST_MAX;
        fc_pill(bx - 6, by - 3, bw + 12, 9, PAL_RAMP(RAMP_STONE, 5));
        plat_rect(bx, by + 1, bw * F.dist0 / DIST_MAX, 1, PAL_RAMP(RAMP_STONE, 3));
        plat_rect(bx + bw * F.dist0 / DIST_MAX, by - 2, 1, 7, PAL_PAINT(PAINT_CHERRY, 1));
        plat_rect(bx - 3, by - 1, 4, 5, PAL_RAMP(RAMP_WOOD, 3));               /* pier */
        fish_draw(F.sp, fxp, by + 1, 6, F.t, FDRAW_FLIP | FDRAW_SILHOUETTE, PAL_RAMP(RAMP_VAPE, 1));
        if (F.exhausted) fc_text(fxp + 6, by - 2, "zz", PAL_PAINT(PAINT_SKY, 2));
    }
}

static void draw_card(void)
{
    const FishInfo *f = &fish_info[F.sp];
    int w = DOOMO_MIN(V.w - 12, fc_big() ? 280 : 228), h = fc_big() ? 176 : 132;
    int x = (V.w - w) / 2, y = fc_big() ? 22 : 6, t = F.t;
    int pw = fc_big() ? 120 : 92, ph = fc_big() ? 96 : 62, px = x + 8, py = y + 22;
    int rx = px + pw + 8, ry = py, r, k;
    char buf[40];
    static const char *const opts[3] = { "Keep", "Sell", "Release" };
    plat_shade_rect(0, 0, V.w, V.h, 9);
    if (!ui_window_pop(x, y, w, h, f->hell ? WIN_DARK : WIN_CREAM, t)) return;
    ui_title(x + w / 2, y - 4, f->name, f->hell ? WIN_HELL : WIN_CUTE);
    /* the photo: lake gradient, the fish wiggling, sparkles */
    ui_round_rect(px - 1, py - 1, pw + 2, ph + 2, 4, PAL_RAMP(RAMP_STONE, 1));
    for (k = 0; k < 4; k++) {
        uint8_t c = f->hell ? PAL_RAMP(RAMP_LAVA, 6 - k) : PAL_RAMP(RAMP_SKY, 7 - k);
        plat_rect(px, py + ph * k / 4, pw, ph / 4 + 1, c);
    }
    for (k = 0; k < 5; k++) {
        int bx = px + 6 + k * pw / 5, by = py + ph - 4 - ((t + k * 13) % 40) * ph / 50;
        ui_fill_circle(bx, by, 1, PAL_WHITE);
    }
    r = fish_draw_radius(F.sp, F.size, fc_big() ? 50 : 38);
    if (f->shape == FSH_LONG) r = r * 4 / 5;
    fish_draw(F.sp, px + pw / 2, py + ph / 2 + ui_wave(t, 40, 2), r, t, FDRAW_HAPPY * ((t / 40) & 1), 0);
    if (F.landed_flags & CATCH_NEW) {
        fc_pill(px - 4, py - 4, 30, 11, PAL_PAINT(PAINT_CHERRY, 1));
        ui_text(px + 1, py - 2, "NEW!", PAL_WHITE);
    }
    /* stats */
    fc_stars(rx + 3, ry + 4, f->rarity, 5, t);
    ui_text(rx, ry + 10, fish_rarity_names[f->rarity], f->hell ? PAL_RAMP(RAMP_LAVA, 6) : PAL_PAINT(PAINT_GRAPE, 1));
    if (F.size >= 100) doomo_snprintf(buf, sizeof(buf), "%d.%02d m", F.size / 100, F.size % 100);
    else doomo_snprintf(buf, sizeof(buf), "%d cm", F.size);
    ui_text_big(rx, ry + 21, buf, 2, f->hell ? PAL_WHITE : PAL_RAMP(RAMP_WOOD, 1), f->hell ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_WHITE);
    if (F.landed_flags & CATCH_RECORD) {
        int b = ui_wave(t * 2, 30, 2);
        fc_pill(rx, ry + 40 - b, 50, 11, PAL_UI_GOLD);
        ui_text(rx + 4, ry + 42 - b, "RECORD!", PAL_RAMP(RAMP_WOOD, 1));
    } else {
        doomo_snprintf(buf, sizeof(buf), "Best %d cm", fc.record[F.sp]);
        ui_text(rx, ry + 42, buf, f->hell ? PAL_RAMP(RAMP_STONE, 5) : PAL_RAMP(RAMP_STONE, 2));
    }
    doomo_snprintf(buf, sizeof(buf), STR_ICON_COIN "%d  " STR_ICON_STAR "%s", F.value, fish_time_names[f->time]);
    ui_text(rx, ry + 54, buf, f->hell ? PAL_RAMP(RAMP_CANDY, 6) : PAL_RAMP(RAMP_WOOD, 2));
    ui_text_wrap(x + 8, py + ph + 5, w - 16, f->desc, f->hell ? PAL_RAMP(RAMP_STONE, 6) : PAL_RAMP(RAMP_STONE, 1), 1);
    /* buttons */
    {
        int bw = (w - 40) / 3, by = y + h - 19;
        for (k = 0; k < 3; k++) {
            const char *lab = opts[k];
            if (k == 1) { doomo_snprintf(buf, sizeof(buf), "Sell %d", F.value); lab = buf; }
            if (k == 2 && f->rarity == FR_JUNK) lab = "Toss";
            ui_button(x + 14 + k * (bw + 6), by, bw, lab, F.card_sel == k, f->hell ? WIN_HELL : WIN_CUTE);
        }
    }
}

static void draw_menu(void)
{
    static const char *const items[4] = { "Fishdex", "Cooler", "Rod", "Leave the lake" };
    int w = fc_big() ? 200 : 168, h = 4 * 16 + 30, x = (V.w - w) / 2, y = (V.h - h) / 2, i;
    char buf[40];
    plat_shade_rect(0, 0, V.w, V.h, 10);
    if (!ui_window_pop(x, y, w, h, WIN_MINT, F.t)) return;
    ui_title(x + w / 2, y - 5, "Tackle Box", WIN_MINT);
    for (i = 0; i < 4; i++) {
        const char *val = "";
        if (i == 0) { doomo_snprintf(buf, sizeof(buf), "%d/%d", fish_dex_count(), FISH_COUNT); val = buf; }
        if (i == 1) { doomo_snprintf(buf, sizeof(buf), "%d fish", fish_cooler_total()); val = buf; }
        if (i == 2) val = fish_rods[fc.rod].name;
        ui_value_row(x + 18, y + 14 + i * 16, w - 30, items[i], val, F.menu_sel == i);
    }
    ui_text_wrap(x + 10, y + h - 14, w - 20, F.menu_sel == 2 ? fish_rods[fc.rod].desc : "", PAL_RAMP(RAMP_MINT, 1), 1);
}

/* An aquarium backdrop for the Fishdex / cooler: three bands of water and
 * a few bubbles rising (cheap: ~25 spans). */
static void aquarium(int warm)
{
    int w = V.w, h = V.h, k, t = F.t;
    uint8_t a = warm ? PAL_RAMP(RAMP_PEACH, 7) : PAL_RAMP(RAMP_MINT, 7);
    uint8_t b = warm ? PAL_RAMP(RAMP_PASTEL_PINK, 7) : PAL_RAMP(RAMP_SKY, 7);
    uint8_t c = warm ? PAL_RAMP(RAMP_PEACH, 6) : PAL_RAMP(RAMP_MINT, 6);
    plat_rect(0, 0, w, h / 3, a);
    plat_rect(0, h / 3, w, h / 3, b);
    plat_rect(0, h * 2 / 3, w, h - h * 2 / 3, c);
    plat_checker(0, h / 3, w, 1, a);
    plat_checker(0, h * 2 / 3, w, 1, b);
    for (k = 0; k < 6; k++) {
        int x = (k * 53 + 17) % w, y = h - ((t * (1 + (k & 1)) + k * 41) % (h + 10));
        lake_draw_ripple(x + ui_wave(t + k * 30, 50, 2), y, 2 + (k % 3), warm ? PAL_RAMP(RAMP_PEACH, 5) : PAL_RAMP(RAMP_SKY, 5));
    }
}

static void draw_dex(void)
{
    const UiStyle *st = ui_style(WIN_MINT);
    int big = fc_big(), cols = 6, cw = (V.w - 16) / cols, chh = big ? 36 : 24;
    int rows = big ? 4 : 3, gy = big ? 20 : 18, top, i, sel = F.dex_sel, t = F.t;
    int py = gy + rows * chh + 2, ph = V.h - py - 16;
    char buf[48];
    const FishInfo *f = &fish_info[sel];
    int have = fish_dex_has(sel);
    aquarium(0);
    doomo_snprintf(buf, sizeof(buf), "Fishdex  %d/%d", fish_dex_count(), FISH_COUNT);
    ui_title(V.w / 2, 3, buf, WIN_MINT);
    top = sel / cols - rows + 1;
    if (top < 0) top = 0;
    if (F.dex_top > sel / cols) F.dex_top = (int16_t)(sel / cols);
    if (F.dex_top < top) F.dex_top = (int16_t)top;
    top = F.dex_top;
    for (i = top * cols; i < FISH_COUNT && i < (top + rows) * cols; i++) {
        int cx = 8 + (i % cols) * cw + cw / 2, cy = gy + (i / cols - top) * chh + chh / 2;
        int s = i == sel, r = big ? 14 : 11;
        uint8_t bg = s ? st->accent_d : (fish_dex_has(i) ? PAL_WHITE : PAL_RAMP(RAMP_MINT, 6));
        if (s) {
            ui_round_rect(cx - cw / 2 + 1, cy - chh / 2 + 1, cw - 2, chh - 2, 4, st->accent);
            ui_round_rect(cx - cw / 2 + 2, cy - chh / 2 + 2, cw - 4, chh - 4, 3, bg);
        } else {
            plat_rect(cx - cw / 2 + 2, cy - chh / 2 + 1, cw - 4, chh - 2, bg);
            plat_rect(cx - cw / 2 + 2, cy + chh / 2 - 2, cw - 4, 1, st->outline);
        }
        if (fish_dex_has(i)) fish_draw(i, cx, cy + 1 - (s ? ui_wave(t * 2, 30, 1) : 0), r, t + i * 7, 0, 0);
        else fish_draw(i, cx, cy + 1, r, 0, FDRAW_SILHOUETTE, PAL_RAMP(RAMP_MINT, 3));
        if (s) ui_cursor(cx - cw / 2 - 2, cy - 3);
    }
    ui_scroll_arrows(V.w - 7, gy, gy + rows * chh - 4, top > 0, (top + rows) * cols < FISH_COUNT);
    /* info panel */
    ui_window(4, py, V.w - 8, ph, WIN_CREAM);
    if (have) {
        int ty = py + 4;
        ui_text_shadow(10, ty, f->name, PAL_RAMP(RAMP_WOOD, 1), PAL_WHITE);
        fc_stars(V.w - 52, ty + 3, f->rarity, 5, t);
        ty += FONT_LINE + 1;
        if (fc.record[sel] >= 100)
            doomo_snprintf(buf, sizeof(buf), "%s  Best %d.%02d m  x%d", fish_time_names[f->time],
                           fc.record[sel] / 100, fc.record[sel] % 100, fc.caught[sel]);
        else
            doomo_snprintf(buf, sizeof(buf), "%s  Best %d cm  x%d", fish_time_names[f->time],
                           fc.record[sel], fc.caught[sel]);
        ui_text(10, ty, buf, PAL_PAINT(PAINT_TEAL, 1));
        ty += FONT_LINE + 1;
        ui_text_wrap(10, ty, V.w - 20, f->desc, PAL_RAMP(RAMP_STONE, 1), 1);
    } else {
        const DoomoUnlock *u = unlock_by_id(FC_UL_DEX(sel));
        ui_text_shadow(10, py + 4, "? ? ?", PAL_RAMP(RAMP_STONE, 3), PAL_WHITE);
        doomo_snprintf(buf, sizeof(buf), "#%02d  %s", sel + 1, fish_rarity_names[f->rarity]);
        ui_text(10, py + 4 + FONT_LINE + 1, buf, PAL_RAMP(RAMP_STONE, 2));
        ui_text_wrap(10, py + 6 + 2 * FONT_LINE, V.w - 20, u ? u->how : "", PAL_RAMP(RAMP_STONE, 2), 1);
    }
    ui_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R STR_ICON_ARROW_U STR_ICON_ARROW_D " Browse  " STR_ICON_B " Back", WIN_MINT);
}

static void draw_cooler(void)
{
    uint8_t list[FISH_COUNT];
    int big = fc_big(), n = cooler_list(list), w = V.w - 16, x = 8, y = 20, rowh = big ? 18 : 15;
    int ir = big ? 9 : 7, tx = x + (big ? 50 : 44);
    int vis = (V.h - y - 34) / rowh, top = 0, i, t = F.t;
    char buf[48];
    aquarium(1);
    doomo_snprintf(buf, sizeof(buf), "Cooler  %d fish", fish_cooler_total());
    ui_title(V.w / 2, 3, buf, WIN_CREAM);
    fc_money_pill(V.w - 3, 3);
    ui_window(x - 2, y - 2, w + 4, vis * rowh + 6, WIN_CREAM);
    if (!n) {
        ui_text_center(V.w / 2, y + 20, "Empty! Keep a catch to cook it.", PAL_RAMP(RAMP_STONE, 2));
        fish_draw(FISH_BUBBLEGUM_GUPPY, V.w / 2, y + 44, 12, t, FDRAW_SILHOUETTE, PAL_RAMP(RAMP_PEACH, 5));
    }
    if (F.cool_sel >= vis) top = F.cool_sel - vis + 1;
    for (i = top; i < n && i < top + vis; i++) {
        int sp = list[i], ry = y + (i - top) * rowh, s = i == F.cool_sel;
        const FishInfo *f = &fish_info[sp];
        if (s) {
            fc_pill(x + 8, ry, w - 12, rowh - 1, PAL_RAMP(RAMP_PASTEL_PINK, 6));
            ui_cursor(x, ry + rowh / 2 - 4);
        }
        fish_draw(sp, x + (big ? 32 : 28), ry + rowh / 2, ir, s ? t : 0, FDRAW_ICON, 0);
        ui_text(tx, ry + rowh / 2 - 4, f->name, PAL_RAMP(RAMP_WOOD, 1));
        doomo_snprintf(buf, sizeof(buf), "x%d  " STR_ICON_COIN "%d", fc.cooler[sp],
                       fish_value(sp, (f->size_min + f->size_max) / 2));
        ui_text(x + w - 8 - ui_text_width(buf), ry + rowh / 2 - 4, buf, PAL_RAMP(RAMP_WOOD, 2));
    }
    ui_hint(STR_ICON_A " Sell 1  R Sell all  " STR_ICON_B " Back", WIN_CREAM);
}

static void fish_draw_scene(void)
{
    int t = F.t, ox = 0, oy = 0, pose = 0, bend = 0, k;
    int ax, ay;
    if (plat_draw_skipped() || !fs || !fc_scratch_mine()) return;
    ax = V.pier_x1 - 40;
    ay = V.pier_y - 47;
    if (F.st == FS_DEX) { draw_dex(); fc_fx_draw(0, 0); return; }
    if (F.st == FS_COOLER) { draw_cooler(); fc_fx_draw(0, 0); return; }
    if (F.shake) { ox = ui_wave(t * 9, 6, 2); oy = ui_wave(t * 7, 5, 1); }
    (void)ox; (void)oy;
    lake_draw_sky(&V);
    lake_draw_water(&V);
    /* the fish shadow circling under the surface */
    if (F.st == FS_WAIT) {
        int off = F.shadow_off * (F.wait - F.t) / DOOMO_MAX(1, F.wait);
        int r = 4 + fish_info[F.sp].rarity * 2 + (fish_info[F.sp].shape == FSH_BIG) * 4;
        lake_draw_shadow(F.bob_x + off * ((F.sp & 1) ? 1 : -1), F.bob_y + 5 + ui_wave(t, 50, 2), r);
    }
    lake_draw_pier(&V);
    switch (F.st) {
    case FS_CHARGE: pose = 1; break;
    case FS_CAST: pose = F.t < 6 ? 2 : 0; break;
    case FS_REEL: pose = down(BTN_A) ? 3 : 0; bend = DOOMO_MIN(14, F.tension / 60); break;
    case FS_LANDED: case FS_CARD: pose = 4; break;
    default: pose = 0; break;
    }
    {
        int tx, ty;
        lake_draw_angler(&V, ax, ay, pose, bend, fc.rod, &tx, &ty);
        F.tip_x = (int16_t)tx;
        F.tip_y = (int16_t)ty;
    }
    /* line + bobber */
    switch (F.st) {
    case FS_CAST: {
        int p = F.t * 256 / 18, bx = F.tip_x + (F.land_x - F.tip_x) * p / 256;
        int by = F.tip_y + (F.land_y - F.tip_y) * p / 256 - (p * (256 - p)) / 900;
        draw_line(F.tip_x, F.tip_y, bx, by, 2, PAL_WHITE);
        lake_draw_bobber(bx, by, t, 0);
        break; }
    case FS_WAIT: case FS_BITE: {
        int dip = 0, under = F.st == FS_BITE;
        if (F.st == FS_WAIT && F.nibbles < 3 && (F.t % DOOMO_MAX(1, F.nib_t)) < 4 && F.t > 4) dip = 2;
        draw_line(F.tip_x, F.tip_y, F.bob_x, F.bob_y + dip, 6, PAL_RAMP(RAMP_STONE, 6));
        lake_draw_bobber(F.bob_x, F.bob_y + dip + ui_wave(t, 45, 1), t, under);
        if (under) {
            int s = ui_pop_scale(F.t * 2);
            fc_text(F.bob_x - 2, F.bob_y - 16 - (s > 256 ? 2 : 0), "!", PAL_PAINT(PAINT_CHERRY, 2));
            ui_text_big(F.bob_x - 6, F.bob_y - 28 - ui_wave(t * 4, 20, 2), "!", 3, PAL_PAINT(PAINT_CHERRY, 1), PAL_WHITE);
        }
        break; }
    case FS_REEL: {
        uint8_t lc = F.tension > T_ZONE_HI ? ((t & 2) ? PAL_PAINT(PAINT_CHERRY, 2) : PAL_WHITE) : PAL_WHITE;
        draw_line(F.tip_x, F.tip_y, F.bob_x, F.bob_y, F.tension > 500 ? 0 : 3, lc);
        if (F.jump) {
            int j = 26 - F.jump, hgt = j * (26 - j) / 6;
            fish_draw(F.sp, F.bob_x + j / 2, F.bob_y - hgt, 8 + fish_info[F.sp].rarity * 2, t,
                      FDRAW_FIGHT | FDRAW_FLIP, 0);
        } else {
            lake_draw_ripple(F.bob_x, F.bob_y + 2, 4 + (t / 2) % 5, PAL_WHITE);
            lake_draw_shadow(F.bob_x + 4, F.bob_y + 4, 8 + fish_info[F.sp].rarity * 2);
            plat_rect(F.bob_x - 1, F.bob_y - 1, 3, 2, PAL_PAINT(PAINT_CHERRY, 1));
        }
        break; }
    case FS_LANDED: {
        int j = F.t, ex = ax + 40, ey = ay + 6;
        int x = F.bob_x + (ex - F.bob_x) * j / 26, y = F.bob_y + (ey - F.bob_y) * j / 26 - j * (26 - j) / 5;
        draw_line(F.tip_x, F.tip_y, x, y, 0, PAL_WHITE);
        fish_draw(F.sp, x, y, 10 + fish_info[F.sp].rarity * 2, t * 2, FDRAW_FIGHT | FDRAW_FLIP, 0);
        break; }
    default: break;
    }
    /* a bucket by your feet with a tail poking out when the cooler has fish */
    {
        int bx = ax - 4, by = V.pier_y - 9;
        if (fish_cooler_total() > 0) {
            for (k = 0; k < 3; k++) plat_rect(bx + 2 - k, by - 3 + k, 1 + 2 * k, 1, PAL_PAINT(PAINT_TANGERINE, 1));
        }
        ui_round_rect(bx - 4, by - 1, 12, 10, 2, PAL_RAMP(RAMP_STEEL, 2));
        plat_rect(bx - 3, by, 10, 7, PAL_RAMP(RAMP_STEEL, 4));
        plat_rect(bx - 3, by, 10, 1, PAL_RAMP(RAMP_STEEL, 6));
        plat_rect(bx - 3, by + 3, 10, 1, PAL_PAINT(PAINT_SKY, 1));
    }
    fc_fx_draw(0, 0);
    hud();
    switch (F.st) {
    case FS_IDLE:
        ui_hint(STR_ICON_A " Cast  L/R Bait  START Menu  " STR_ICON_B " Leave", WIN_CUTE);
        if (progress_get(PROG_FISH_CAUGHT) == 0 && F.catches == 0)
            fc_bubble(V.w - (fc_big() ? 196 : 150), V.horizon - (fc_big() ? 30 : 20), fc_big() ? 180 : 140,
                      "Hold " STR_ICON_A " to wind up, let go to cast! Top of the swing = PERFECT.",
                      ax + 16, ay, 999);
        break;
    case FS_CHARGE:
        power_meter();
        ui_hint("Let go of " STR_ICON_A " to cast!", WIN_CUTE);
        break;
    case FS_WAIT:
        ui_hint("Wait for the big splash...  " STR_ICON_B " Reel in", WIN_CUTE);
        break;
    case FS_BITE:
        ui_hint(STR_ICON_A " NOW! Hook it!", WIN_HELL);
        break;
    case FS_REEL:
        tension_meter();
        ui_hint("Hold " STR_ICON_A " to reel - let go when it pulls!", WIN_CUTE);
        break;
    case FS_LOST:
        if (F.msg) {
            int s = ui_pop_scale(F.t), w = ui_text_width(F.msg) + 20;
            if (s > 128) {
                ui_window(V.w / 2 - w / 2, V.h / 2 - 14, w, 22, WIN_DARK);
                ui_text_center(V.w / 2, V.h / 2 - 7, F.msg, PAL_WHITE);
            }
        }
        break;
    case FS_CARD:
        draw_card();
        fc_fx_draw(0, 0);
        ui_hint(STR_ICON_ARROW_L STR_ICON_ARROW_R " Choose  " STR_ICON_A " OK", WIN_CREAM);
        break;
    case FS_MENU:
        draw_menu();
        ui_hint(STR_ICON_A " Open  " STR_ICON_ARROW_L STR_ICON_ARROW_R " Rod  " STR_ICON_B " Back", WIN_MINT);
        break;
    default: break;
    }
}

const DoomoScene SCENE_FISHING = {
    "FISHING", fish_enter, fish_leave, fish_update, fish_draw_scene, 0
};

/* ---- test hooks ------------------------------------------------------------------ */
static int live(void) { return fs && fc_scratch_mine(); }
int fish_scene_state(void) { return live() ? F.st : -1; }
int fish_scene_species(void) { return live() ? F.sp : -1; }
int fish_scene_catches(void) { return live() ? F.catches : 0; }
void fish_scene_reel_info(int *dist, int *tension, int *stamina)
{
    if (dist) *dist = live() ? F.dist : 0;
    if (tension) *tension = live() ? F.tension : 0;
    if (stamina) *stamina = live() ? F.stamina : 0;
}
