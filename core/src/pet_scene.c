/* pet_scene.c - SCENE_PET: the Doomogachi's LCD-green screen (owner: life).
 *
 * A plush egg-shaped keychain device (its shell colour follows the pet's
 * line) with a real dot-matrix LCD: 64x44 chunky dots drawn 2x (GBA) or
 * 3x with dot gaps (PS1), a faint LCD shadow under every lit dot, a row of
 * icons on top (food, lights, play, medicine) and bottom (clean, health,
 * discipline, attention). LEFT/RIGHT pick an icon, A uses it, B backs out,
 * SELECT opens the Doomodex, START leaves. The world keeps ticking while
 * you watch (sim_tick), so the pet lives on game time.
 */
#include "doomo/doomo.h"

#define LW 64                       /* LCD size in dots                  */
#define LH 44
#define ICONS 8

enum { IC_FOOD, IC_LIGHTS, IC_PLAY, IC_MED, IC_CLEAN, IC_HEALTH, IC_DISC, IC_ATTN };
enum { M_MAIN, M_FOOD, M_EAT, M_LIGHTS, M_PLAY, M_MED, M_CLEAN, M_HEALTH, M_DISC,
       M_EVOLVE, M_DEAD, M_NOPET, M_DEX };

/* 10x6 icons, bit 9 = leftmost dot */
static const uint16_t icon_bits[ICONS][6] = {
    { 0x030, 0x078, 0x0FC, 0x1CE, 0x186, 0x3FF },          /* onigiri     */
    { 0x0F0, 0x108, 0x108, 0x090, 0x0F0, 0x060 },          /* bulb        */
    { 0x0F0, 0x1B8, 0x3DC, 0x3EC, 0x1F8, 0x0F0 },          /* ball        */
    { 0x030, 0x030, 0x1FE, 0x1FE, 0x030, 0x030 },          /* plus        */
    { 0x0C0, 0x1E0, 0x1D8, 0x2F8, 0x1F0, 0x0E0 },          /* duck        */
    { 0x18C, 0x3DE, 0x3FE, 0x1FC, 0x0F8, 0x070 },          /* heart       */
    { 0x00C, 0x03C, 0x37C, 0x37C, 0x03C, 0x00C },          /* megaphone   */
    { 0x0C4, 0x1E4, 0x0C4, 0x1E0, 0x0C4, 0x120 }           /* attention   */
};
static const uint8_t poop_bits[7] = { 0x08, 0x18, 0x3C, 0x66, 0x7E, 0xDB, 0xFF }; /* 8 wide swirl */
static const uint8_t skull_bits[6] = { 0x38, 0x7C, 0x54, 0x7C, 0x38, 0x28 };      /* 7 wide */
static const uint8_t heart_bits[6] = { 0x6C, 0xFE, 0xFE, 0x7C, 0x38, 0x10 };      /* 7 wide */
static const uint8_t heart_empty[6] = { 0x6C, 0x92, 0x82, 0x44, 0x28, 0x10 };
static const uint8_t meal_bits[6] = { 0x00, 0x3C, 0x7E, 0xFF, 0x7E, 0x3C };       /* rice bowl */
static const uint8_t snack_bits[6] = { 0x18, 0x3C, 0x7E, 0x24, 0x3C, 0x18 };      /* cupcake */
static const uint8_t duck_big[7] = { 0x30, 0x78, 0x76, 0xBE, 0x7E, 0x3C, 0x00 };
static const uint8_t pill_bits[6] = { 0x18, 0x24, 0x24, 0x3C, 0x3C, 0x18 };

static struct {
    int mode, mt, sel, sub;
    int x, dir, step_t;                 /* wandering position in dots      */
    int last_stage, last_form;
    int evolve_from;
    int play_round, play_score, play_guess, play_way, play_t;
    int page;
    int btn_flash[3];
    int dex_sel;
    char name_buf[8];
    uint8_t ink, mid, bg, shadow, grid, dark, seam;
} S;

static int spr_key = -9999;             /* what spr (below) holds         */

/* ---------------------------------------------------------------------- */
/* geometry & the LCD blitter                                               */
static int big(void) { return plat_screen_h() >= 200; }
static int dsc(void) { return big() ? 3 : 2; }
static int lcd_x(void) { return plat_screen_w() / 2 - LW * dsc() / 2; }
static int egg_cy(void) { return big() ? 118 : 84; }
static int lcd_y(void) { return egg_cy() - LH * dsc() / 2 - (big() ? 4 : 2); }

static void lcd_rect(int x, int y, int w, int h, int v)
{
    int s = dsc(), gap = s >= 3, i, j;
    uint8_t c = v == 1 ? S.ink : v == 2 ? S.mid : v == 4 ? S.shadow : S.bg;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LW) w = LW - x;
    if (y + h > LH) h = LH - y;
    if (w <= 0 || h <= 0) return;
    plat_rect(lcd_x() + x * s, lcd_y() + y * s, w * s, h * s, c);
    if (!gap || v != 1) return;
    /* hint of the dot matrix inside lit areas: a darker seam per dot */
    for (j = 0; j < h; j++)
        plat_rect(lcd_x() + x * s, lcd_y() + (y + j) * s + s - 1, w * s, 1, S.seam);
    for (i = 0; i < w; i++)
        plat_rect(lcd_x() + (x + i) * s + s - 1, lcd_y() + y * s, 1, h * s, S.seam);
}

static void lcd_dot(int x, int y, int v) { lcd_rect(x, y, 1, 1, v); }

/* a 2-bit canvas image (pet_render) with its LCD shadow; flip mirrors */
static void lcd_sprite(int x, int y, const uint8_t *pix, int flip, int ghost_only)
{
    int i, j;
    for (j = 0; j < PET_SPR; j++)
        for (i = 0; i < PET_SPR; i++) {
            int v = pix[j * PET_SPR + (flip ? PET_SPR - 1 - i : i)];
            if (v == 1 || v == 2) lcd_dot(x + i + 1, y + j + 1, 4);
        }
    if (ghost_only) return;
    for (j = 0; j < PET_SPR; j++) {
        int run = -1, rv = 0;
        for (i = 0; i <= PET_SPR; i++) {
            int v = i < PET_SPR ? pix[j * PET_SPR + (flip ? PET_SPR - 1 - i : i)] : 0;
            if (v != rv || i == PET_SPR) {
                if (rv && run >= 0) lcd_rect(x + run, y + j, i - run, 1, rv);
                run = i;
                rv = v;
            }
        }
    }
}

static void lcd_bits(int x, int y, const uint8_t *rows, int h, int w, int v)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (rows[j] & (1 << (w - 1 - i))) lcd_dot(x + i, y + j, v);
}

static void lcd_text(int x, int y, const char *s, int v)
{
    for (; *s; s++, x += 6) {
        int c = (unsigned char)*s, col, row;
        if (c < FONT_FIRST || c > FONT_LAST) continue;
        for (col = 0; col < 5; col++) {
            uint8_t bits = doomo_font[c - FONT_FIRST][col];
            for (row = 0; row < 8; row++)
                if (bits & (1 << row)) lcd_dot(x + col, y + row, v);
        }
    }
}

static void lcd_text_center(int y, const char *s, int v)
{
    lcd_text(LW / 2 - (int)strlen(s) * 3, y, s, v);
}

static void colours(void)
{
    S.ink = palette_nearest(44, 56, 40);
    S.mid = palette_nearest(104, 124, 76);
    S.bg = PAL_UI_LCD;
    S.shadow = palette_nearest(150, 174, 102);
    S.grid = palette_nearest(164, 190, 112);
    S.dark = palette_nearest(56, 70, 46);
    S.seam = palette_nearest(70, 88, 56);
}

/* ---------------------------------------------------------------------- */
/* state                                                                    */
static const DoomoPet *pet(void) { return &doomo_town.pet; }

static void set_mode(int m)
{
    S.mode = m;
    S.mt = 0;
    S.sub = 0;
}

static void on_named(int ok)
{
    if (!ok || !S.name_buf[0]) memcpy(S.name_buf, "Doomo", 6);
    pet_new(S.name_buf);
    S.last_stage = PET_STAGE_EGG;
    S.last_form = 0;
    set_mode(M_MAIN);
    audio_sfx(SFX_EGG_CRACK);
}

static void pet_enter(int arg)
{
    (void)arg;
    memset(&S, 0, sizeof(S));
    spr_key = -9999;
    colours();
    S.x = LW / 2 - PET_SPR / 2;
    S.dir = 1;
    S.last_stage = pet()->stage;
    S.last_form = pet()->form;
    audio_music(SONG_PET);
    set_mode(pet_exists() ? (pet()->stage == PET_STAGE_DEAD ? M_DEAD : M_MAIN) : M_NOPET);
}

static void press_fx(int b)
{
    S.btn_flash[b] = 6;
    audio_sfx(b == 0 ? SFX_CURSOR : b == 1 ? SFX_SELECT : SFX_CANCEL);
}

static int asleep(void) { return pet_is_asleep(); }

static void use_icon(int ic)
{
    const DoomoPet *p = pet();
    if (p->stage == PET_STAGE_EGG && ic != IC_HEALTH) { audio_sfx(SFX_ERROR); return; }
    switch (ic) {
    case IC_FOOD:
        if (asleep()) { audio_sfx(SFX_ERROR); return; }
        set_mode(M_FOOD); break;
    case IC_LIGHTS: set_mode(M_LIGHTS); S.sel = pet_lights_off() ? 1 : 0; break;
    case IC_PLAY:
        if (asleep()) { audio_sfx(SFX_ERROR); return; }
        set_mode(M_PLAY); S.play_round = 0; S.play_score = 0; S.play_guess = -1; break;
    case IC_MED: set_mode(M_MED); S.sub = pet_is_sick(); pet_medicine(); break;
    case IC_CLEAN: set_mode(M_CLEAN); break;
    case IC_HEALTH: set_mode(M_HEALTH); S.page = 0; break;
    case IC_DISC:
        set_mode(M_DISC);
        S.sub = pet_call_is_tantrum();
        pet_discipline();
        audio_sfx(S.sub ? SFX_HAPPY : SFX_SAD);
        break;
    default: break;
    }
}

/* ---------------------------------------------------------------------- */
/* update                                                                   */
static void pet_update(void)
{
    const DoomoPet *p;
    int i;
    sim_tick();                                   /* the world goes on */
    p = pet();
    S.mt++;
    for (i = 0; i < 3; i++) if (S.btn_flash[i] > 0) S.btn_flash[i]--;

    if (btn_pressed(BTN_START) && S.mode != M_PLAY) {
        audio_sfx(SFX_CANCEL);
        if (scene_depth() > 1) scene_pop(); else scene_set(&SCENE_TITLE, 0);
        return;
    }
    /* evolution / hatching / passing, whatever screen we were on */
    if (pet_exists() && S.mode != M_EVOLVE && S.mode != M_NOPET &&
        (p->stage != S.last_stage || p->form != S.last_form)) {
        S.evolve_from = S.last_stage == PET_STAGE_EGG ? -1 : S.last_form;
        if (p->stage == PET_STAGE_DEAD) {
            set_mode(M_DEAD);
            audio_sfx(SFX_SAD);
        } else {
            set_mode(M_EVOLVE);
            audio_sfx(S.last_stage == PET_STAGE_EGG ? SFX_EGG_CRACK : SFX_EVOLVE);
        }
        S.last_stage = p->stage;
        S.last_form = p->form;
    }
    /* beep beep when it wants you */
    if (pet_calling() && opt_get(OPT_PET_NOTIFICATIONS) && S.mode == M_MAIN && S.mt % 75 == 0)
        audio_sfx(SFX_BLIP);

    switch (S.mode) {
    case M_NOPET:
        if (btn_pressed(BTN_A)) {
            press_fx(1);
            S.name_buf[0] = 0;
            ui_text_entry("Name your Doomogachi!", S.name_buf, (int)sizeof(S.name_buf), on_named);
        }
        break;
    case M_MAIN:
        if (btn_pressed(BTN_SELECT)) { press_fx(2); set_mode(M_DEX); break; }
        if (btn_repeat(BTN_RIGHT)) { S.sel = (S.sel + 1) % (ICONS - 1); press_fx(0); }
        if (btn_repeat(BTN_LEFT)) { S.sel = (S.sel + ICONS - 2) % (ICONS - 1); press_fx(0); }
        if (btn_pressed(BTN_A)) { press_fx(1); use_icon(S.sel); }
        if (btn_pressed(BTN_B)) { press_fx(2); }
        /* wander */
        if (!asleep() && p->stage != PET_STAGE_EGG && ++S.step_t >= 18) {
            S.step_t = 0;
            if (rng_range(&doomo_rng, 5) == 0) S.dir = -S.dir;
            S.x += S.dir * 2;
            if (S.x < 2) { S.x = 2; S.dir = 1; }
            if (S.x > LW - PET_SPR - 12) { S.x = LW - PET_SPR - 12; S.dir = -1; }
        }
        break;
    case M_FOOD:
        if (btn_repeat(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) { S.sel ^= 1; press_fx(0); }
        if (btn_pressed(BTN_B)) { press_fx(2); set_mode(M_MAIN); S.sel = IC_FOOD; }
        if (btn_pressed(BTN_A)) {
            press_fx(1);
            S.sub = S.sel;                       /* 0 meal, 1 snack */
            pet_feed(S.sub);
            set_mode(M_EAT);
            S.sub = (S.sel & 1) | (pet_last_fed() ? 2 : 0);
            audio_sfx(pet_last_fed() ? SFX_EAT : SFX_ERROR);
        }
        break;
    case M_EAT:
        if (S.mt == 20 && (S.sub & 2)) audio_sfx(SFX_EAT);
        if (S.mt > 60 || btn_pressed(BTN_B)) { set_mode(M_MAIN); S.sel = IC_FOOD; }
        break;
    case M_LIGHTS:
        if (btn_repeat(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) { S.sel ^= 1; press_fx(0); }
        if (btn_pressed(BTN_A)) { press_fx(1); pet_lights(S.sel); set_mode(M_MAIN); S.sel = IC_LIGHTS; }
        if (btn_pressed(BTN_B)) { press_fx(2); set_mode(M_MAIN); S.sel = IC_LIGHTS; }
        break;
    case M_PLAY:
        if (S.play_guess < 0) {
            if (btn_pressed(BTN_LEFT)) { S.play_guess = 0; press_fx(0); }
            if (btn_pressed(BTN_RIGHT)) { S.play_guess = 1; press_fx(0); }
            if (btn_pressed(BTN_B)) { press_fx(2); set_mode(M_MAIN); S.sel = IC_PLAY; break; }
            if (S.play_guess >= 0) {
                S.play_way = rng_range(&doomo_rng, 2);
                S.play_t = 0;
                if (S.play_way == S.play_guess) { S.play_score++; audio_sfx(SFX_HAPPY); }
                else audio_sfx(SFX_BLIP);
            }
        } else if (++S.play_t > 40) {
            S.play_guess = -1;
            if (++S.play_round >= 5) {
                int won = S.play_score >= 3;
                pet_play_result(won);
                S.sub = won;
                S.play_t = 0;
                S.play_round = 5;
                S.play_guess = -2;
                audio_sfx(won ? SFX_HAPPY : SFX_SAD);
            }
        }
        if (S.play_guess == -2) {
            S.play_t++;
            if (S.play_t > 45 || btn_pressed(BTN_A | BTN_B)) { set_mode(M_MAIN); S.sel = IC_PLAY; }
        }
        break;
    case M_MED: case M_DISC:
        if (S.mt > 55 || btn_pressed(BTN_B)) { set_mode(M_MAIN); S.sel = S.mode == M_MED ? IC_MED : IC_DISC; }
        break;
    case M_CLEAN:
        if (S.mt == 1) audio_sfx(SFX_SPLASH);
        if (S.mt == 40) pet_clean();
        if (S.mt > 50) { set_mode(M_MAIN); S.sel = IC_CLEAN; }
        break;
    case M_HEALTH:
        if (btn_repeat(BTN_RIGHT) || btn_pressed(BTN_A)) { S.page = (S.page + 1) % 6; press_fx(1); }
        if (btn_repeat(BTN_LEFT)) { S.page = (S.page + 5) % 6; press_fx(0); }
        if (btn_pressed(BTN_B)) { press_fx(2); set_mode(M_MAIN); S.sel = IC_HEALTH; }
        break;
    case M_EVOLVE:
        if (S.mt == 70) audio_sfx(SFX_HAPPY);
        if (S.mt > 110 || (S.mt > 30 && btn_pressed(BTN_A | BTN_B))) set_mode(M_MAIN);
        break;
    case M_DEAD:
        if (S.mt > 40 && btn_pressed(BTN_A)) {
            press_fx(1);
            memcpy(S.name_buf, pet()->name, 8);
            S.name_buf[7] = 0;
            ui_text_entry("A new egg! Name it:", S.name_buf, (int)sizeof(S.name_buf), on_named);
        }
        break;
    case M_DEX:
        if (btn_repeat(BTN_RIGHT)) { S.dex_sel = (S.dex_sel + 1) % PET_FORM_COUNT; press_fx(0); }
        if (btn_repeat(BTN_LEFT)) { S.dex_sel = (S.dex_sel + PET_FORM_COUNT - 1) % PET_FORM_COUNT; press_fx(0); }
        if (btn_repeat(BTN_DOWN)) { S.dex_sel = (S.dex_sel + 4) % PET_FORM_COUNT; press_fx(0); }
        if (btn_repeat(BTN_UP)) { S.dex_sel = (S.dex_sel + PET_FORM_COUNT - 4) % PET_FORM_COUNT; press_fx(0); }
        if (btn_pressed(BTN_B | BTN_SELECT)) { press_fx(2); set_mode(M_MAIN); }
        break;
    default: break;
    }
}

/* ---------------------------------------------------------------------- */
/* drawing: the device                                                      */
static void shell_colours(uint8_t *hi, uint8_t *md, uint8_t *lo, uint8_t *rim)
{
    int line = pet_exists() && pet()->stage != PET_STAGE_EGG ? pet_line() : -1;
    int ramp = line == PET_LINE_ANGEL ? RAMP_CANDY : line == PET_LINE_FERAL ? RAMP_MINT :
               line == PET_LINE_DEMON ? RAMP_VAPE : line == PET_LINE_CUTE ? RAMP_PASTEL_PINK : -1;
    if (ramp < 0) {                                 /* egg / none: lavender */
        *hi = PAL_PAINT(PAINT_LAVENDER, 2); *md = PAL_PAINT(PAINT_LAVENDER, 1);
        *lo = PAL_PAINT(PAINT_LAVENDER, 0); *rim = PAL_PAINT(PAINT_GRAPE, 0);
        return;
    }
    if (line == PET_LINE_DEMON) {
        *hi = PAL_RAMP(ramp, 4); *md = PAL_RAMP(ramp, 3); *lo = PAL_RAMP(ramp, 2); *rim = PAL_RAMP(ramp, 0);
    } else {
        *hi = PAL_RAMP(ramp, 7); *md = PAL_RAMP(ramp, 5); *lo = PAL_RAMP(ramp, 4); *rim = PAL_RAMP(ramp, 2);
    }
}

/* an egg: the top half narrower and a little taller than the bottom */
static void egg_fill(int cx, int cy, int rx, int ry, int rxt, int ryt, uint8_t c)
{
    int y;
    for (y = cy - ryt; y <= cy + ry; y++) {
        int top = y < cy, dy = top ? cy - y : y - cy, r = top ? ryt : ry, w = top ? rxt : rx, hw;
        uint32_t k;
        if (y < 0 || y >= plat_screen_h() || dy > r) continue;
        k = (uint32_t)(r * r - dy * dy) * 256u / (uint32_t)(r * r);
        hw = (int)((uint32_t)w * fx_isqrt(k * 256u) / 256u);
        plat_rect(cx - hw, y, hw * 2 + 1, 1, c);
    }
}

static void draw_device(uint32_t t)
{
    int W = plat_screen_w(), b = big();
    int cx = W / 2, cy = egg_cy(), rx = b ? 134 : 102, ry = b ? 118 : 84, rxt = b ? 122 : 94, ryt = b ? 128 : 92;
    int lx = lcd_x(), ly = lcd_y(), lw = LW * dsc(), lh = LH * dsc(), i;
    uint8_t hi, md, lo, rim;
    shell_colours(&hi, &md, &lo, &rim);
    /* soft shadow under the device */
    for (i = 0; i < 4; i++) plat_shade_rect(cx - rx + 14 + i * 6, cy + ry - 4 + i, rx * 2 - 28 - i * 12, 1, 11);
    /* keychain loop + chain on top (when it fits) */
    if (cy - ryt - 6 > 0) {
        ui_fill_circle(cx, cy - ryt - 2, 7, PAL_RAMP(RAMP_STEEL, 3));
        ui_fill_circle(cx, cy - ryt - 2, 5, PAL_RAMP(RAMP_STEEL, 6));
        ui_fill_circle(cx, cy - ryt - 2, 3, 0);
    }
    /* the egg-shaped shell: coloured rim, body, inner glow, highlight */
    egg_fill(cx, cy, rx + 2, ry + 2, rxt + 2, ryt + 2, rim);
    egg_fill(cx, cy, rx, ry, rxt, ryt, lo);
    egg_fill(cx - 2, cy - 2, rx - 6, ry - 7, rxt - 6, ryt - 5, md);
    ui_fill_ellipse(cx - rxt / 2, cy - ryt * 2 / 3, rxt / 4, ryt / 8 + 1, hi);
    plat_rect(cx - rxt / 2 - rxt / 8, cy - ryt * 2 / 3 - 1, rxt / 6, 2, PAL_WHITE);
    ui_sparkle(cx - rxt / 2 + rxt / 6, cy - ryt * 2 / 3 - 3, (int)((t / 3) & 15), PAL_WHITE);
    /* printed hearts and stars around the screen */
    for (i = 0; i < 8; i++) {
        int a = i * (DOOMO_ANG_FULL / 8) + 256;
        int sx = cx + ((fx_cos(a) * (rx - 14)) >> 16), sy = cy + ((fx_sin(a) * (ry - 12)) >> 16);
        if (sy > ly - 10 && sy < ly + lh + 10 && sx > lx - 10 && sx < lx + lw + 10) continue;
        if (sy < 4 || sy > plat_screen_h() - 4) continue;
        if (i & 1) ui_star(sx, sy, 3, hi); else ui_heart(sx, sy, 3, hi);
    }
    /* brand above the screen */
    {
        const char *brand = "DOOMOGACHI";
        int bw = ui_text_width(brand);
        ui_text_outline(cx - bw / 2, ly - (b ? 20 : 17), brand, PAL_WHITE, rim);
    }
    /* bezel */
    ui_round_rect(lx - 7, ly - 7, lw + 14, lh + 14, 8, rim);
    ui_round_rect(lx - 6, ly - 6, lw + 12, lh + 12, 7, PAL_RAMP(RAMP_STONE, 2));
    ui_round_rect(lx - 3, ly - 3, lw + 6, lh + 6, 4, PAL_RAMP(RAMP_STONE, 1));
    plat_rect(lx - 2, ly - 5, lw + 4, 1, PAL_RAMP(RAMP_STONE, 4));
    /* three buttons */
    for (i = 0; i < 3; i++) {
        int bx = cx + (i - 1) * (b ? 36 : 28), by = ly + lh + (b ? 22 : 17) + (i == 1 ? 3 : 0), r = b ? 9 : 7;
        int down = S.btn_flash[i] > 0;
        static const char *const lab[3] = { STR_ICON_ARROW_L STR_ICON_ARROW_R, "A", "B" };
        ui_fill_circle(bx, by + 2, r, rim);
        ui_fill_circle(bx, by + down, r, i == 1 ? PAL_PAINT(PAINT_BUBBLEGUM, 0) : PAL_PAINT(PAINT_LEMON, 0));
        ui_fill_circle(bx, by - 1 + down, r - 1, i == 1 ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_PAINT(PAINT_LEMON, 1));
        plat_rect(bx - r / 2, by - r / 2 - 1 + down, r / 2, 2, PAL_WHITE);
        if (b) ui_text_center(bx, by + r + 3, lab[i], rim);
    }
}

static void lcd_background(int dark)
{
    int s = dsc(), x, y;
    uint8_t c = dark ? S.dark : S.bg;
    plat_rect(lcd_x(), lcd_y(), LW * s, LH * s, c);
    if (s >= 3 && !dark) {
        for (x = 1; x <= LW; x++) plat_rect(lcd_x() + x * s - 1, lcd_y(), 1, LH * s, S.grid);
        for (y = 1; y <= LH; y++) plat_rect(lcd_x(), lcd_y() + y * s - 1, LW * s, 1, S.grid);
    }
}

static void draw_icons(void)
{
    int i;
    for (i = 0; i < ICONS; i++) {
        int row = i / 4, col = i % 4, x = 3 + col * 16, y = row ? LH - 7 : 1;
        int v = 2, j, k;
        if (i == IC_ATTN) v = pet_calling() ? 1 : 2;
        if (i == S.sel && S.mode == M_MAIN) v = 1;
        for (j = 0; j < 6; j++)
            for (k = 0; k < 10; k++)
                if (icon_bits[i][j] & (1 << (9 - k))) lcd_dot(x + k, y + j, v);
        if (i == S.sel && S.mode == M_MAIN) {
            int bl = (doomo_frame_count() / 8) & 1;
            lcd_dot(x - 2 + bl, y + 2, 1);
            lcd_dot(x - 2 + bl, y + 3, 1);
        }
    }
}

/* ---------------------------------------------------------------------- */
/* drawing: LCD screens                                                     */
/* the 32x32 canvas lives in the scene scratch (pet_gfx.c); valid after
 * render_cached() */
static uint8_t *spr;
uint8_t *pet_scene_canvas(int *fresh);

static int pet_frame(uint32_t t)
{
    const DoomoPet *p = pet();
    if (p->stage == PET_STAGE_EGG) return (int)((t / 15) & 1);
    if (p->stage == PET_STAGE_DEAD) return (int)((t / 20) & 1);
    if (asleep()) return PET_ANIM_SLEEP;
    if (pet_is_sick()) return (t / 20) & 1 ? PET_ANIM_SICK : PET_ANIM_IDLE0;
    if (pet_call_is_tantrum()) return (t / 10) & 1 ? PET_ANIM_ANGRY : PET_ANIM_IDLE0;
    if (p->hunger == 0 || p->joy == 0) return (t / 24) & 1 ? PET_ANIM_SAD : PET_ANIM_IDLE0;
    if ((t % 120) < 8) return PET_ANIM_IDLE1;                     /* blink */
    return (t / 16) & 1 ? PET_ANIM_IDLE0 : (p->joy >= 3 && (t / 16) % 6 == 1 ? PET_ANIM_HAPPY : PET_ANIM_IDLE0);
}

static int pet_form_now(void)
{
    const DoomoPet *p = pet();
    return p->stage == PET_STAGE_EGG ? -1 : p->stage == PET_STAGE_DEAD ? -2 : p->form;
}

/* render a form into spr only when it changes (frames change every few
 * ticks; rendering every frame would waste the GBA's time) */
static void render_cached(int form, int frame)
{
    int key = (form + 2) * 16 + frame, fresh;
    spr = pet_scene_canvas(&fresh);
    if (fresh) spr_key = -9999;
    if (key == spr_key) return;
    pet_render(form, frame, spr);
    spr_key = key;
}

static void draw_pet_at(int x, int frame, int flip)
{
    render_cached(pet_form_now(), frame);
    lcd_sprite(x, 7 + (LH - 14 - PET_SPR) / 2, spr, flip, 0);
}

static void draw_main(uint32_t t)
{
    const DoomoPet *p = pet();
    int i;
    if (pet_lights_off()) {
        lcd_background(1);
        draw_icons();
        if (asleep()) {
            int z = (int)((t / 20) % 3);
            lcd_text(LW / 2 + 6 + z * 2, 10 - z * 2, "z", 2);
            lcd_text(LW / 2 + 12, 6, "Z", 2);
        }
        return;
    }
    lcd_background(0);
    draw_icons();
    draw_pet_at(S.x, pet_frame(t), S.dir < 0);
    /* poop piles at the right edge, steaming */
    for (i = 0; i < p->poop && i < 4; i++) {
        int px = LW - 10 - (i & 1) * 9, py = LH - 15 - (i >> 1) * 8;
        lcd_bits(px, py - 1, poop_bits, 7, 8, 1);
        if ((t / 12 + (uint32_t)i) & 1) { lcd_dot(px + 2, py - 2, 2); lcd_dot(px + 5, py - 3, 2); }
        else { lcd_dot(px + 3, py - 3, 2); lcd_dot(px + 6, py - 2, 2); }
    }
    if (pet_is_sick()) lcd_bits(LW - 9, 9, skull_bits, 6, 7, 1);
    if (asleep()) {
        int z = (int)((t / 20) % 3);
        lcd_text(S.x + PET_SPR - 4 + z * 2, 10 - z * 2, "z", 1);
    }
}

static void draw_food(void)
{
    lcd_background(0);
    draw_icons();
    lcd_text(14, 11, "MEAL", 1);
    lcd_text(14, 22, "SNACK", 1);
    lcd_bits(46, 12, meal_bits, 6, 8, 1);
    lcd_bits(46, 23, snack_bits, 6, 8, 1);
    lcd_text(6, S.sel ? 22 : 11, STR_ICON_ARROW_R, 1);
}

static void draw_eat(uint32_t t)
{
    int ate = (S.sub & 2) != 0, snack = S.sub & 1, bites = S.mt / 15;
    lcd_background(0);
    draw_icons();
    if (!ate) {
        /* shakes its head: "no thanks" */
        draw_pet_at(LW / 2 - PET_SPR / 2 + ((t / 5) & 1 ? -2 : 2), PET_ANIM_SAD, 0);
        lcd_text(LW - 14, 10, "NO", 1);
        return;
    }
    draw_pet_at(8, (S.mt / 8) & 1 ? PET_ANIM_EAT : PET_ANIM_IDLE0, 0);
    if (bites < 3) {
        const uint8_t *b = snack ? snack_bits : meal_bits;
        int rows = 6 - bites * 2;
        lcd_bits(44, 22 + bites * 2, b + bites * 2, rows, 8, 1);
    } else {
        lcd_bits(44, 12, heart_bits, 6, 7, 1);
    }
}

static void draw_lights(void)
{
    lcd_background(0);
    draw_icons();
    lcd_text(20, 11, "ON", 1);
    lcd_text(20, 22, "OFF", 1);
    lcd_text(10, S.sel ? 22 : 11, STR_ICON_ARROW_R, 1);
}

static void draw_play(uint32_t t)
{
    char b[16];
    lcd_background(0);
    draw_icons();
    if (S.play_guess == -2) {
        draw_pet_at(LW / 2 - PET_SPR / 2, S.sub ? PET_ANIM_HAPPY : PET_ANIM_SAD, 0);
        lcd_text(2, 8, S.sub ? "WIN!" : "LOSE", 1);
        doomo_snprintf(b, sizeof(b), "%d/5", S.play_score);
        lcd_text(LW - 20, 8, b, 1);
        return;
    }
    if (S.play_guess < 0) {
        draw_pet_at(LW / 2 - PET_SPR / 2, (t / 10) & 1 ? PET_ANIM_IDLE0 : PET_ANIM_IDLE1, 0);
        lcd_text(4, 18, STR_ICON_ARROW_L, (t / 8) & 1 ? 1 : 2);
        lcd_text(LW - 9, 18, STR_ICON_ARROW_R, (t / 8) & 1 ? 1 : 2);
        lcd_text(LW / 2 - 3, 8, "?", 1);
    } else {
        int hop = S.play_t < 8 ? S.play_t : 8, right = S.play_way;
        draw_pet_at(LW / 2 - PET_SPR / 2 + (right ? hop * 2 : -hop * 2),
                    S.play_way == S.play_guess ? PET_ANIM_HAPPY : PET_ANIM_IDLE0, !right);
        lcd_text(LW / 2 - 3, 8, S.play_way == S.play_guess ? "O" : "X", 1);
    }
    doomo_snprintf(b, sizeof(b), "%d", S.play_round + 1);
    lcd_text(2, 8, b, 2);
}

static void draw_med(uint32_t t)
{
    lcd_background(0);
    draw_icons();
    if (S.sub) {
        int px = LW - 12 - (S.mt < 20 ? 0 : (S.mt - 20) / 2);
        draw_pet_at(6, S.mt < 30 ? PET_ANIM_SICK : PET_ANIM_HAPPY, 0);
        if (S.mt < 34) lcd_bits(px, 20, pill_bits, 6, 6, 1);
        else lcd_bits(44, 12, heart_bits, 6, 7, 1);
    } else {
        draw_pet_at(LW / 2 - PET_SPR / 2 + ((t / 5) & 1 ? -2 : 2), PET_ANIM_SAD, 0);
        lcd_text(LW - 14, 10, "NO", 1);
    }
}

static void draw_clean(uint32_t t)
{
    int dx = S.mt * (LW + 12) / 40 - 12, i;
    const DoomoPet *p = pet();
    lcd_background(0);
    draw_icons();
    draw_pet_at(S.x, PET_ANIM_IDLE0, S.dir < 0);
    for (i = 0; i < p->poop && i < 4 && S.mt < 40; i++) {
        int px = LW - 10 - (i & 1) * 9, py = LH - 15 - (i >> 1) * 8;
        if (px > dx + 6) lcd_bits(px, py - 1, poop_bits, 7, 8, 1);
    }
    /* the rubber duck surfs across on a wave */
    lcd_bits(dx, LH - 17, duck_big, 7, 8, 1);
    for (i = 0; i < 9; i++) lcd_dot(dx - 2 + i, LH - 10 + (((uint32_t)i + t / 3) & 1), 2);
}

static void draw_gauge(int y, int value)
{
    int w = 44, f = w * DOOMO_CLAMP(value, 0, 100) / 100;
    lcd_rect(10, y, w + 2, 6, 1);
    lcd_rect(11, y + 1, w, 4, 3);
    lcd_rect(11, y + 1, f, 4, 1);
}

static void draw_hearts(int y, int n)
{
    int i;
    for (i = 0; i < PET_HEARTS; i++)
        lcd_bits(10 + i * 11, y, i < n ? heart_bits : heart_empty, 6, 7, 1);
}

static void draw_health(void)
{
    const DoomoPet *p = pet();
    char b[24];
    lcd_background(0);
    draw_icons();
    switch (S.page) {
    case 0:
        lcd_text(2, 8, p->name, 1);
        doomo_snprintf(b, sizeof(b), "AGE %dd", p->age_days);
        lcd_text(2, 18, b, 1);
        doomo_snprintf(b, sizeof(b), "%dg", p->weight);
        lcd_text(44, 18, b, 1);
        lcd_text(2, 27, doomo_pet_stage_names[DOOMO_CLAMP(p->stage, 0, PET_STAGE_DEAD)], 2);
        break;
    case 1: lcd_text(2, 8, "HUNGRY", 1); draw_hearts(19, p->hunger); break;
    case 2: lcd_text(2, 8, "HAPPY", 1); draw_hearts(19, p->joy); break;
    case 3: lcd_text(2, 8, "DISCIPL", 1); draw_gauge(19, p->discipline); break;
    case 4:
        lcd_text(2, 8, "HOLY", 1);
        lcd_text(40, 8, p->holiness >= 75 ? STR_ICON_HEART : p->holiness <= 25 ? STR_ICON_PENTA : "-", 1);
        draw_gauge(18, p->holiness);
        lcd_text(2, 27, doomo_pet_line_names[pet_line_forecast()], 2);
        break;
    default:
        if (p->stage == PET_STAGE_EGG) lcd_text_center(14, "EGG", 1);
        else lcd_text_center(10, p->stage == PET_STAGE_DEAD ? "BYE" : doomo_pet_form_names[p->form], 1);
        if (p->stage != PET_STAGE_EGG && p->stage != PET_STAGE_DEAD) {
            lcd_text_center(18, doomo_pet_line_names[pet_line()], 2);
            doomo_snprintf(b, sizeof(b), "ERR %d", p->mistakes);
            lcd_text(2, 27, b, 2);
        }
        break;
    }
    doomo_snprintf(b, sizeof(b), "%d/6", S.page + 1);
    lcd_text(LW - 20, 27, b, 2);
}

static void draw_evolve(uint32_t t)
{
    int flip = S.mt < 70 ? ((S.mt / (S.mt < 40 ? 6 : 3)) & 1) : 0, i;
    lcd_background(0);
    draw_icons();
    if (S.mt < 70 && flip) render_cached(S.evolve_from, S.evolve_from == -1 ? 2 + (S.mt > 40) : PET_ANIM_IDLE0);
    else render_cached(pet_form_now(), S.mt < 70 ? PET_ANIM_IDLE0 : PET_ANIM_HAPPY);
    lcd_sprite(LW / 2 - PET_SPR / 2, 7 + (LH - 14 - PET_SPR) / 2, spr, 0, 0);
    for (i = 0; i < 6; i++) {
        int a = (int)t * 40 + i * (DOOMO_ANG_FULL / 6);
        int sx = LW / 2 + ((fx_cos(a) * 22) >> 16), sy = LH / 2 + ((fx_sin(a) * 14) >> 16);
        lcd_dot(sx, sy, 1);
        if ((t / 4 + (uint32_t)i) & 1) { lcd_dot(sx - 1, sy, 2); lcd_dot(sx + 1, sy, 2); lcd_dot(sx, sy - 1, 2); lcd_dot(sx, sy + 1, 2); }
    }
    if (S.mt >= 70) lcd_text_center(8, pet()->stage == PET_STAGE_BABY && S.evolve_from == -1 ? "HATCHED!" : "EVOLVED!", 1);
}

static void draw_dead(uint32_t t)
{
    lcd_background(0);
    draw_icons();
    render_cached(-2, (int)((t / 20) & 1));
    lcd_sprite(LW / 2 - PET_SPR / 2, 4 - (int)((t / 30) % 3), spr, 0, 0);
    lcd_text_center(LH - 16, S.mt > 40 && ((t / 15) & 1) ? "A:NEW EGG" : "BYE BYE..", 1);
}

static void draw_nopet(uint32_t t)
{
    lcd_background(0);
    draw_icons();
    render_cached(-1, (int)((t / 15) & 1));
    lcd_sprite(-4, 6, spr, 0, 0);
    lcd_text(28, 12, "PRESS", 1);
    if ((t / 15) & 1) lcd_text(40, 22, "A", 1);
}

/* The Doomodex: one evolution line per page (baby -> adult), so the four
 * images fit the art cache. UP/DOWN change the line, LEFT/RIGHT the form. */
static void draw_dex(uint32_t t)
{
    int W = plat_screen_w(), H = plat_screen_h(), b = big(), i, seen = 0;
    int line = S.dex_sel / 4, cell = b ? 64 : 50, ww = cell * 4 + 20, wh = b ? 150 : 112;
    int gx = W / 2 - ww / 2, gy = H / 2 - wh / 2;
    char buf[48];
    for (i = 0; i < PET_FORM_COUNT; i++) seen += pet_form_seen(i);
    if (!ui_window_pop(gx, gy, ww, wh, WIN_LCD, S.mt)) return;
    doomo_snprintf(buf, sizeof(buf), "Doomodex %d/16", seen);
    ui_text(gx + 8, gy + 5, buf, ui_style(WIN_LCD)->title);
    doomo_snprintf(buf, sizeof(buf), STR_ICON_ARROW_U " %s line " STR_ICON_ARROW_D, doomo_pet_line_names[line]);
    ui_text(gx + ww - 8 - ui_text_width(buf), gy + 5, buf, ui_style(WIN_LCD)->text);
    for (i = 0; i < 4; i++) {
        int f = line * 4 + i, x = gx + 10 + i * cell, y = gy + (b ? 30 : 22), sel = f == S.dex_sel;
        int bob = sel ? ui_wave((int)t, 30, 2) : 0;
        const DoomoImage *img = pet_form_image(f, PET_ANIM_IDLE0);
        if (sel) {
            ui_round_rect(x - 1, y - 1, cell - 4, cell - 4, 5, ui_style(WIN_LCD)->accent);
            ui_round_rect(x + 1, y + 1, cell - 8, cell - 8, 4, PAL_UI_LCD);
        }
        if (pet_form_seen(f))
            plat_image(x + (cell - 4 - PET_SPR) / 2, y + (cell - 4 - PET_SPR) / 2 - bob, img, 1, 0, 0);
        else
            plat_image(x + (cell - 4 - PET_SPR) / 2, y + (cell - 4 - PET_SPR) / 2 - bob, img, 1,
                       IMG_SILHOUETTE, ui_style(WIN_LCD)->text_dim);
        if (i < 3) ui_text(x + cell - 8, y + cell / 2 - 6, STR_ICON_ARROW_R, ui_style(WIN_LCD)->text_dim);
        ui_text_center(x + cell / 2 - 2, y + cell - 2, doomo_pet_stage_names[i + 1], ui_style(WIN_LCD)->text_dim);
    }
    {
        const DoomoUnlock *u = unlock_find(UL_CAT_PET, 0, S.dex_sel);
        int ty = gy + wh - (b ? 34 : 24);
        ui_text(gx + 10, ty, pet_form_seen(S.dex_sel) ? doomo_pet_form_names[S.dex_sel] : "???",
                ui_style(WIN_LCD)->title);
        if (u) ui_text(gx + 10, ty + 11, pet_form_seen(S.dex_sel) ? "Raised!" : u->how,
                       ui_style(WIN_LCD)->text);
    }
}

static void pet_draw(void)
{
    uint32_t t = doomo_frame_count();
    const DoomoPet *p = pet();
    char b[48];
    ui_backdrop(WIN_CUTE, t);
    draw_device(t);
    switch (S.mode) {
    case M_NOPET: draw_nopet(t); break;
    case M_FOOD: draw_food(); break;
    case M_EAT: draw_eat(t); break;
    case M_LIGHTS: draw_lights(); break;
    case M_PLAY: draw_play(t); break;
    case M_MED: case M_DISC:
        if (S.mode == M_MED) draw_med(t);
        else {
            lcd_background(0);
            draw_icons();
            draw_pet_at(LW / 2 - PET_SPR / 2, S.mt < 25 ? PET_ANIM_ANGRY : (S.sub ? PET_ANIM_HAPPY : PET_ANIM_SAD), 0);
            if (S.mt < 25) { lcd_text(4, 10, "!", 1); lcd_text(LW - 8, 10, "!", 1); }
        }
        break;
    case M_CLEAN: draw_clean(t); break;
    case M_HEALTH: draw_health(); break;
    case M_EVOLVE: draw_evolve(t); break;
    case M_DEAD: draw_dead(t); break;
    default: draw_main(t); break;
    }
    if (S.mode == M_DEX) { draw_main(t); draw_dex(t); }
    /* caption under the device */
    if (pet_exists() && S.mode != M_DEX) {
        if (p->stage == PET_STAGE_EGG) doomo_snprintf(b, sizeof(b), "%s (egg)", p->name);
        else if (p->stage == PET_STAGE_DEAD) doomo_snprintf(b, sizeof(b), "%s is watching over you", p->name);
        else doomo_snprintf(b, sizeof(b), "%s the %s", p->name, doomo_pet_form_names[p->form]);
        if (big()) ui_title(plat_screen_w() / 2, 2, b, WIN_CUTE);
    }
    if (big()) ui_hint(S.mode == M_DEX ? STR_ICON_B " Back" :
                       STR_ICON_ARROW_L STR_ICON_ARROW_R " Pick  " STR_ICON_A " Use  SELECT Doomodex  START Exit",
                       WIN_CUTE);
}

const DoomoScene SCENE_PET = { "PET", pet_enter, NULL, pet_update, pet_draw, 0 };
