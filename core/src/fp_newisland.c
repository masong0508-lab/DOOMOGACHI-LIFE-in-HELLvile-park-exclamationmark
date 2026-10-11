/* fp_newisland.c - SCENE_NEW_ISLAND: how does your island start?
 * (owner: freeplay)
 *
 *   Empty Island          like the original games: nobody lives here yet,
 *                         you create every citizen (straight into the creator)
 *   Hellvile Park Classic all 1000 premade citizens move in
 *   Random Island         1000 seeded random citizens (type a seed with
 *                         SELECT, reroll with LEFT/RIGHT on the card)
 *
 * Then: name your island? (optional) -> town_new + sim_init -> the island.
 */
#include "fp_int.h"

enum { C_EMPTY, C_CLASSIC, C_RANDOM, C_COUNT };

static const char *const card_names[C_COUNT] = { "Empty Island", "Classic", "Random" };
static const char *const card_titles[C_COUNT] = {
    "Empty Island", "Hellvile Park Classic", "Random Island"
};
static const char *const card_desc[C_COUNT] = {
    "Nobody lives here yet. Create every citizen yourself, up to 500 on each side - just like the original games!",
    "All 1000 premade citizens move in: 500 sweethearts in Hillvile and 500 rascals in Hellvile.",
    "1000 brand-new random citizens, cooked up from a seed. Same seed, same island!"
};

static struct {
    int sel, t, phase, phase_t;
    uint32_t seed;
    char seedbuf[12];
    char namebuf[FP_ISLAND_NAME_LEN];
    int roll_t;
} N;

static void ni_enter(int arg)
{
    (void)arg;
    memset(&N, 0, sizeof(N));
    N.sel = C_CLASSIC;
    N.seed = plat_entropy() % 100000u;
    audio_music(SONG_CREATOR);
    fp_fx_clear();
}

static void build(void)
{
    int pop = N.sel == C_EMPTY ? TOWN_POP_EMPTY : N.sel == C_CLASSIC ? TOWN_POP_PREMADE : TOWN_POP_RANDOM;
    uint32_t seed = N.sel == C_RANDOM ? N.seed : plat_entropy();
    fp_new_island(pop, seed);
    fp_set_island_name(N.namebuf);
    fp_achieve(N.sel == C_EMPTY ? UL_FP_EMPTY_START : N.sel == C_CLASSIC ? UL_FP_PREMADE_START : UL_FP_RANDOM_START);
    if (N.namebuf[0]) fp_achieve(UL_FP_NAMER);
    audio_sfx(SFX_HAPPY);
    fp_goto_island(FP_ISLAND_ARG_WELCOME);
}

static void cb_name(int ok)
{
    if (!ok) N.namebuf[0] = 0;
    build();
}

static void cb_ask_name(int c)
{
    if (c == 0) {
        N.namebuf[0] = 0;
        ui_text_entry("Name your island", N.namebuf, FP_ISLAND_NAME_LEN, cb_name);
    } else {
        N.namebuf[0] = 0;
        build();
    }
}

static void cb_seed(int ok)
{
    uint32_t v = 0;
    int i, any = 0;
    if (!ok) return;
    for (i = 0; N.seedbuf[i]; i++) {
        char c = N.seedbuf[i];
        if (c >= '0' && c <= '9') { v = v * 10u + (uint32_t)(c - '0'); any = 1; }
        else if (c != ' ') v = v * 31u + (uint32_t)(unsigned char)c, any = 1;   /* words work too */
    }
    if (any) N.seed = v;
    audio_sfx(SFX_SPARKLE);
    N.roll_t = 16;
}

static void ni_update(void)
{
    fp_fx_tick();
    N.t++;
    if (N.roll_t > 0) N.roll_t--;
    if (btn_repeat(BTN_LEFT)) { N.sel = (N.sel + C_COUNT - 1) % C_COUNT; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_RIGHT)) { N.sel = (N.sel + 1) % C_COUNT; audio_sfx(SFX_CURSOR); }
    if (N.sel == C_RANDOM && btn_pressed(BTN_UP | BTN_DOWN)) {
        N.seed = (plat_entropy() ^ doomo_hash32(N.seed + (uint32_t)N.t)) % 100000u;
        N.roll_t = 16;
        audio_sfx(SFX_BLIP);
    }
    if (N.sel == C_RANDOM && btn_pressed(BTN_SELECT)) {
        doomo_snprintf(N.seedbuf, sizeof(N.seedbuf), "%u", (unsigned)N.seed);
        ui_text_entry("Island seed (number or word)", N.seedbuf, sizeof(N.seedbuf), cb_seed);
        return;
    }
    if (btn_pressed(BTN_A)) {
        static const char *const opts[2] = { "Yes, name it!", "Maybe later" };
        audio_sfx(SFX_SELECT);
        fp_fx_burst(fp_sw() / 2, fp_sh() / 2, FX_CONFETTI, 16);
        ui_choice(-1, "Would you like to give your island a name?", opts, 2, cb_ask_name);
        return;
    }
    if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); scene_set(&SCENE_TITLE, 0); }
}

/* ---- card art ---------------------------------------------------------- */
static void mini_island(int cx, int cy, int w, int h, int kind, int t)
{
    int i;
    ui_fill_ellipse(cx, cy + 2, w / 2 + 2, h / 2 + 1, PAL_RAMP(RAMP_PEACH, 3));
    ui_fill_ellipse(cx, cy, w / 2 + 1, h / 2 + 1, PAL_RAMP(RAMP_CANDY, 5));
    ui_fill_ellipse(cx, cy - 1, w / 2 - 2, h / 2 - 2, PAL_RAMP(RAMP_GRASS, 6));
    if (kind == C_EMPTY) {
        /* one tiny sapling and a "?" sign */
        int sw = ui_wave(t, 60, 1);
        plat_rect(cx - 1, cy - 6, 2, 6, PAL_RAMP(RAMP_WOOD, 3));
        ui_fill_circle(cx + sw, cy - 8, 3, PAL_RAMP(RAMP_GRASS, 3));
        ui_fill_circle(cx - 1 + sw, cy - 9, 1, PAL_RAMP(RAMP_GRASS, 7));
        plat_rect(cx + 7, cy - 8, 1, 8, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(cx + 4, cy - 12, 8, 6, PAL_RAMP(RAMP_WOOD, 5));
        plat_text(cx + 6, cy - 12, "?", PAL_RAMP(RAMP_WOOD, 1));
        return;
    }
    /* the river, the wall, and lots of little homes and folks */
    plat_rect(cx, cy - h / 2 + 2, 3, h - 4, PAL_RAMP(RAMP_SKY, 5));
    plat_rect(cx + 3, cy - h / 2 + 2, 1, h - 4, PAL_RAMP(RAMP_STONE, 3));
    ui_fill_ellipse(cx + w / 4, cy, w / 4 - 1, h / 2 - 3, PAL_RAMP(RAMP_VAPE, 4));
    for (i = 0; i < 6; i++) {
        int hx = cx - w / 2 + 6 + (i % 3) * 7, hy = cy - 5 + (i / 3) * 7;
        plat_rect(hx, hy, 5, 4, PAL_PAINT(i & 1 ? PAINT_PEACH : PAINT_BUBBLEGUM, 2));
        plat_rect(hx - 1, hy - 2, 7, 2, PAL_PAINT(i & 1 ? PAINT_ROSE : PAINT_SKY, 1));
        hx = cx + 6 + (i % 3) * 6;
        plat_rect(hx, hy, 4, 4, PAL_RAMP(RAMP_VAPE, 2));
        plat_rect(hx + 1, hy - 3, 2, 3, PAL_RAMP(RAMP_VAPE, 1));
    }
    for (i = 0; i < (kind == C_CLASSIC ? 10 : 6); i++) {
        uint32_t hh = doomo_hash32((uint32_t)i * 97u + (uint32_t)kind);
        int px = cx - w / 2 + 5 + (int)(hh % (uint32_t)(w - 10)) + ui_wave(t + i * 13, 70, 2);
        int py = cy - h / 2 + 5 + (int)((hh >> 8) % (uint32_t)(h - 10));
        plat_rect(px, py, 2, 2, (hh & 1) ? PAL_RAMP(RAMP_PEACH, 7) : PAL_RAMP(RAMP_VAPE, 6));
        plat_rect(px, py + 2, 2, 1, PAL_PAINT((int)(hh >> 20) % 32, 1));
    }
}

static void die(int cx, int cy, int s, uint32_t seed, int roll)
{
    static const int8_t pips[7][6][2] = {
        { { 0, 0 } }, { { 0, 0 } },
        { { -1, -1 }, { 1, 1 } },
        { { -1, -1 }, { 0, 0 }, { 1, 1 } },
        { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } },
        { { -1, -1 }, { 1, -1 }, { 0, 0 }, { -1, 1 }, { 1, 1 } },
        { { -1, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 } }
    };
    int face = 1 + (int)((seed + (uint32_t)(roll ? fp_t() / 3 : 0)) % 6u), i, n = face;
    int jx = roll ? ui_wave(fp_t() * 7, 10, 2) : 0;
    cx += jx;
    ui_round_rect(cx - s, cy - s + 2, s * 2, s * 2, 4, PAL_RAMP(RAMP_STONE, 3));
    ui_round_rect(cx - s, cy - s, s * 2, s * 2, 4, PAL_RAMP(RAMP_STONE, 2));
    ui_round_rect(cx - s + 1, cy - s + 1, s * 2 - 2, s * 2 - 2, 3, PAL_WHITE);
    plat_rect(cx - s + 3, cy - s + 2, s, 1, PAL_RAMP(RAMP_SKY, 7));
    for (i = 0; i < n && i < 6; i++)
        ui_fill_circle(cx + pips[n][i][0] * s / 2, cy + pips[n][i][1] * s / 2, s >= 10 ? 2 : 1,
                       PAL_PAINT(PAINT_ROSE, 1));
}

static void ni_draw(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int sw = fp_sw(), sh = fp_sh(), big = fp_big(), i, t = N.t;
    int cw = big ? 96 : 72, ch = big ? 108 : 78, gap = big ? 8 : 4;
    int x0 = (sw - (cw * 3 + gap * 2)) / 2, y0 = big ? 34 : 24;
    ui_backdrop(WIN_MINT, (uint32_t)fp_t());
    ui_title(sw / 2, 4, "A brand-new island!", WIN_CUTE);
    for (i = 0; i < C_COUNT; i++) {
        int sel = i == N.sel, b = sel ? (ui_wave(t, 24, 2) > 0) * 2 : 0;
        int x = x0 + i * (cw + gap), y = y0 - b + (sel ? -2 : 2);
        int style = i == C_EMPTY ? WIN_CREAM : i == C_CLASSIC ? WIN_CUTE : WIN_MINT;
        const UiStyle *cs = ui_style(style);
        if (!ui_window_pop(x, y, cw, ch, style, t - i * 3)) continue;
        if (sel) {
            ui_round_rect(x - 2, y - 2, cw + 4, 2, 1, PAL_UI_GOLD);
            ui_sparkle(x + cw - 4, y + 4, (t / 2) & 15, PAL_UI_GOLD);
            ui_sparkle(x + 5, y + ch - 6, (t / 2 + 8) & 15, PAL_UI_GOLD);
        }
        if (i == C_RANDOM) die(x + cw / 2, y + ch / 2 - 6, big ? 14 : 10, N.seed, N.roll_t > 0);
        else mini_island(x + cw / 2, y + ch / 2 - 6, cw - 16, big ? 52 : 38, i, t);
        ui_text_center(x + cw / 2, y + ch - (big ? 26 : 22), card_names[i], sel ? cs->accent : cs->text);
        if (i == C_EMPTY) ui_text_center(x + cw / 2, y + ch - 12, "0 citizens", cs->text_dim);
        else if (i == C_CLASSIC) ui_text_center(x + cw / 2, y + ch - 12, "1000 premade", cs->text_dim);
        else {
            char sb[20];
            doomo_snprintf(sb, sizeof(sb), "Seed %u", (unsigned)N.seed);
            ui_text_center(x + cw / 2, y + ch - 12, sb, cs->text_dim);
        }
        if (sel) ui_cursor(x + cw / 2 - 4, y + ch + 3);
    }
    {
        int by = y0 + ch + (big ? 14 : 8), bh = sh - by - 16;
        ui_window(6, by, sw - 12, bh, WIN_CREAM);
        ui_text(14, by + 5, card_titles[N.sel], st->accent);
        ui_text_wrap(14, by + 5 + FONT_LINE + 1, sw - 28, card_desc[N.sel], ui_style(WIN_CREAM)->text, 1);
    }
    ui_hint(N.sel == C_RANDOM ? "\x88\x87 Pick  \x89 Reroll  SEL Seed  " STR_ICON_A " Go!"
                              : "\x88\x87 Pick  " STR_ICON_A " Let's go!  " STR_ICON_B " Back", WIN_CUTE);
    fp_fx_draw();
}

const DoomoScene SCENE_NEW_ISLAND = { "NEW_ISLAND", ni_enter, NULL, ni_update, ni_draw, 0 };
