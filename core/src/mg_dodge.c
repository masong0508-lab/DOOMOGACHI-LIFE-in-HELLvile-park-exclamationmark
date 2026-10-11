/* mg_dodge.c - VAPE CLOUD DODGE (owner: minigames).
 *
 * A cute bullet hell. Lord Vapula lounges in his cloud at the top of the
 * Vape Lounge tent and blows vape rings at you, a tiny cherub: radial
 * bursts, spirals, aimed fans, drizzle, expanding smoke rings and side
 * sweeps, faster and denser as the round goes on. Hold B to focus (slow,
 * shows your tiny hitbox). Rings that only brush past you GRAZE for
 * points and combo; catch the falling halo stars for a bonus. Survive the
 * whole minute without a scratch for the UNTOUCHABLE trophy.
 */
#include "mg.h"

#define DG_BUL 64
enum { P_RADIAL, P_SPIRAL, P_AIMED, P_RAIN, P_WAVE, P_SWEEP, P_COUNT };

typedef struct DBul {
    int16_t x16, y16;
    int8_t  vx, vy;              /* 1/16 px per tick */
    uint8_t r, col, flags, age;  /* flags: 1 grazed, 2 accelerate, 4 star */
} DBul;

typedef struct Dodge {
    int16_t px16, py16;
    uint8_t inv, focus, hits, flap;
    uint8_t pat, pat_t, pat_n, taunt, taunt_t, pose;
    uint16_t next_pat, grazes, star_t;
    int16_t spin;
    DBul b[DG_BUL];
} Dodge;
MG_ARENA_FITS(Dodge);
#define S (MG_ARENA(Dodge))

static const char *const taunts[] = {
    "Inhale my excellence!", "Ring around the cherub!", "Unholy Grape, darling.",
    "Puff puff... PASS!", "You can't dodge FLAVOR.", "Brimstone Blast!",
    "*cough* I meant that.", "Cloud nine, baby!", "Smoke 'em if you got 'em!",
    "My rings are ARTISANAL."
};
#define TAUNTS ((int)(sizeof(taunts) / sizeof(taunts[0])))

static int top_y(void) { return mg_big() ? 18 : 16; }
static int mouth_x(void) { return mg_w() / 2 - 2; }
static int mouth_y(void) { return top_y() + (mg_big() ? 30 : 28); }
static int cloud_y(void) { return top_y() + (mg_big() ? 52 : 44); }
static int ring_r(void) { return mg_big() ? 4 : 3; }

static void dg_start(void)
{
    S->px16 = (int16_t)(mg_w() / 2 * 16);
    S->py16 = (int16_t)((mg_h() - 24) * 16);
    S->next_pat = 50;
    S->taunt = (uint8_t)mg_rand(TAUNTS);
    S->taunt_t = 70;
}

static DBul *fire(int x, int y, int ang, int speed16, int col, int flags)
{
    int i;
    for (i = 0; i < DG_BUL; i++) {
        DBul *b = &S->b[i];
        if (b->r) continue;
        b->x16 = (int16_t)(x * 16);
        b->y16 = (int16_t)(y * 16);
        b->vx = (int8_t)DOOMO_CLAMP(FIX2INT(fx_cos(ang) * speed16), -127, 127);
        b->vy = (int8_t)DOOMO_CLAMP(FIX2INT(fx_sin(ang) * speed16), -127, 127);
        b->r = (uint8_t)ring_r();
        b->col = (uint8_t)col;
        b->flags = (uint8_t)flags;
        b->age = 0;
        return b;
    }
    return NULL;
}

static int aim_at_player(int x, int y)
{
    return fx_atan2(S->py16 / 16 - y, S->px16 / 16 - x);
}

static void begin_pattern(void)
{
    static const uint8_t gap[MG_TIERS] = { 70, 56, 42 };
    int p = mg_rand(mg.play_t < 300 ? 3 : P_COUNT);
    if (p == S->pat && mg_rand(2)) p = (p + 1) % P_COUNT;
    S->pat = (uint8_t)p;
    S->pat_t = 0;
    S->pat_n = 0;
    S->next_pat = (uint16_t)DOOMO_MAX(gap[mg.tier] - mg.play_t / 60, 28) + 40;
    S->pose = 20;
    if (mg_rand(3) == 0) { S->taunt = (uint8_t)mg_rand(TAUNTS); S->taunt_t = 60; }
    audio_sfx(SFX_BUBBLE);
}

static void run_pattern(void)
{
    int mx = mouth_x(), my = mouth_y(), t = S->pat_t, k;
    int lvl = mg.tier + (mg.play_t > 900) + (mg.play_t > 1500);
    int sp = 14 + lvl * 3;
    switch (S->pat) {
    case P_RADIAL:
        if (t % (24 - lvl * 3) == 0 && S->pat_n < 3) {
            int n = 10 + lvl * 3;
            for (k = 0; k < n; k++)
                fire(mx, my, (k * DOOMO_ANG_FULL / n + S->pat_n * 90 + S->spin) & DOOMO_ANG_MASK, sp, S->pat_n, 0);
            S->pat_n++;
        }
        break;
    case P_SPIRAL:
        if (t < 60 && t % (4 - (lvl > 2)) == 0) {
            S->spin = (int16_t)(S->spin + 170);
            fire(mx, my, S->spin & DOOMO_ANG_MASK, sp, 1, 0);
            fire(mx, my, (S->spin + 2048) & DOOMO_ANG_MASK, sp, 2, 0);
            if (lvl >= 3) fire(mx, my, (S->spin + 1024) & DOOMO_ANG_MASK, sp, 0, 0);
        }
        break;
    case P_AIMED:
        if (t % 16 == 0 && S->pat_n < 3 + lvl / 2) {
            int a = aim_at_player(mx, my), spread = 150 - lvl * 15, n = 3 + (lvl >= 2) * 2;
            for (k = 0; k < n; k++) fire(mx, my, (a + (k - n / 2) * spread) & DOOMO_ANG_MASK, sp + 6, 2, 0);
            S->pat_n++;
        }
        break;
    case P_RAIN:
        if (t < 70 && t % (6 - DOOMO_MIN(lvl, 3)) == 0)
            fire(mg_between(10, mg_w() - 10), cloud_y(), 1024 + mg_between(-120, 120), 10 + lvl * 2, 0, 0);
        break;
    case P_WAVE:
        if (t % 30 == 0 && S->pat_n < 2) {
            int n = 18 + lvl * 2, skip = mg_rand(n);
            for (k = 0; k < n; k++) {
                DBul *b;
                if (k == skip || k == (skip + 1) % n) continue;      /* a gap */
                b = fire(mx, my, (k * DOOMO_ANG_FULL / n) & DOOMO_ANG_MASK, 6, 1, 2);
                if (b) b->r = (uint8_t)(ring_r() + 1);
            }
            S->pat_n++;
        }
        break;
    case P_SWEEP:
        if (t % 12 == 0 && S->pat_n < 4 + lvl) {
            int y = mg_between(cloud_y() + 10, mg_h() - 12), left = S->pat_n & 1;
            for (k = 0; k < 3; k++)
                fire(left ? -4 - k * 12 : mg_w() + 4 + k * 12, y, left ? 0 : 2048, 16 + lvl * 3, 2, 0);
            S->pat_n++;
        }
        break;
    default:
        break;
    }
    S->pat_t++;
}

static void dg_update(void)
{
    int i, W = mg_w(), H = mg_h(), px, py, spd;
    int hit_r2, graze_r2, star_r2;
    if (S->inv) S->inv--;
    if (S->taunt_t) S->taunt_t--;
    if (S->pose) S->pose--;
    S->flap++;
    S->focus = (uint8_t)(mg_down(BTN_B) != 0);
    spd = S->focus ? 14 : 30;
    if (mg_down(BTN_LEFT)) S->px16 = (int16_t)(S->px16 - spd);
    if (mg_down(BTN_RIGHT)) S->px16 = (int16_t)(S->px16 + spd);
    if (mg_down(BTN_UP)) S->py16 = (int16_t)(S->py16 - spd);
    if (mg_down(BTN_DOWN)) S->py16 = (int16_t)(S->py16 + spd);
    S->px16 = (int16_t)DOOMO_CLAMP(S->px16, 8 * 16, (W - 8) * 16);
    S->py16 = (int16_t)DOOMO_CLAMP(S->py16, (cloud_y() + 14) * 16, (H - 8) * 16);
    px = S->px16 / 16;
    py = S->py16 / 16;
    /* patterns */
    if (S->next_pat) S->next_pat--;
    else begin_pattern();
    run_pattern();
    /* halo stars drift down now and then */
    if (++S->star_t > 150 - mg.tier * 20) {
        DBul *b = fire(mg_between(16, W - 16), cloud_y(), 1024, 10, 3, 4);
        if (b) b->r = 5;
        S->star_t = 0;
    }
    /* move, graze, collide */
    hit_r2 = 3 * 3;
    graze_r2 = 13 * 13;
    star_r2 = 11 * 11;
    for (i = 0; i < DG_BUL; i++) {
        DBul *b = &S->b[i];
        int bx, by, dx, dy, d2;
        if (!b->r) continue;
        b->age++;
        if ((b->flags & 2) && b->age > 20 && (b->age & 3) == 0) {   /* smoke rings speed up */
            b->vx = (int8_t)DOOMO_CLAMP(b->vx * 5 / 4 + (b->vx > 0) - (b->vx < 0), -100, 100);
            b->vy = (int8_t)DOOMO_CLAMP(b->vy * 5 / 4 + (b->vy > 0) - (b->vy < 0), -100, 100);
        }
        if (b->flags & 4) b->x16 = (int16_t)(b->x16 + ui_wave(b->age, 40, 8));
        b->x16 = (int16_t)(b->x16 + b->vx);
        b->y16 = (int16_t)(b->y16 + b->vy);
        bx = b->x16 / 16;
        by = b->y16 / 16;
        if (bx < -40 || bx > W + 40 || by < -20 || by > H + 10) { b->r = 0; continue; }
        dx = bx - px;
        dy = by - py;
        d2 = dx * dx + dy * dy;
        if (b->flags & 4) {
            if (d2 < star_r2) {
                b->r = 0;
                mg_combo_hit(px, py - 10);
                mg_add_score(50, px, py - 14);
                mg_burst(MGFX_STAR, px, py, 8, 34, PAL_UI_GOLD);
                audio_sfx(SFX_SPARKLE);
            }
            continue;
        }
        if (d2 < hit_r2 + b->r * b->r && !S->inv) {
            b->r = 0;
            S->inv = 60;
            S->hits++;
            S->pose = 0;
            mg_burst(MGFX_HEART, px, py, 6, 30, PAL_PAINT(PAINT_ROSE, 2));
            mg_burst(MGFX_PUFF, px, py, 4, 20, PAL_RAMP(RAMP_VAPE, 6));
            mg_lose_life(px, py);
            {   /* a little mercy: nearby rings pop into hearts */
                int k;
                for (k = 0; k < DG_BUL; k++) {
                    DBul *o = &S->b[k];
                    int ex, ey;
                    if (!o->r || (o->flags & 4)) continue;
                    ex = o->x16 / 16 - px;
                    ey = o->y16 / 16 - py;
                    if (ex * ex + ey * ey < 40 * 40) { mg_fx(MGFX_HEART, o->x16 / 16, o->y16 / 16, 0, -6, PAL_PAINT(PAINT_BUBBLEGUM, 2), 14); o->r = 0; }
                }
            }
            continue;
        }
        if (d2 < graze_r2 && !(b->flags & 1) && !S->inv) {
            b->flags |= 1;
            S->grazes++;
            mg_combo_hit(px, py - 12);
            mg.score += 5 * mg_combo_mult();
            mg.score_bump = 4;
            mg_fx(MGFX_SPARK, (bx + px) / 2, (by + py) / 2, -dx, -dy, PAL_WHITE, 8);
            if ((S->grazes & 7) == 0) mg_popup(px, py - 16, "GRAZE!", 0, PAL_PAINT(PAINT_MINT, 2));
            if ((S->grazes & 3) == 0) audio_sfx(SFX_BLIP);
        }
    }
    /* survival points */
    if ((mg.play_t % 15) == 0) mg.score += 2 + mg.tier;
    if (mg.time_left == 1) {
        if (!S->hits) {
            mg_add_raw(500, mg_w() / 2, mg_h() / 2);
            mg_popup(mg_w() / 2, mg_h() / 2 - 12, "UNTOUCHABLE!", 0, PAL_UI_GOLD);
            mg_trophy(MG_UL_UNTOUCHABLE);
        } else {
            mg_add_raw(mg.lives * 100, mg_w() / 2, mg_h() / 2);
        }
    }
}

/* ---- drawing ------------------------------------------------------------------ */
static void lounge_bg(int t)
{
    int W = mg_w(), H = mg_h(), i, b = H / 5;
    static const uint8_t cute[5] = {
        PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_VAPE, 4),
        PAL_RAMP(RAMP_VAPE, 5), PAL_RAMP(RAMP_PASTEL_PINK, 5)
    };
    static const uint8_t hell[5] = {
        PAL_RAMP(RAMP_BLOOD, 0), PAL_RAMP(RAMP_BLOOD, 1), PAL_RAMP(RAMP_VAPE, 1),
        PAL_RAMP(RAMP_BLOOD, 2), PAL_RAMP(RAMP_LAVA, 2)
    };
    const uint8_t *c = mg.hell ? hell : cute;
    for (i = 0; i < 5; i++) {
        plat_rect(0, i * b, W, i == 4 ? H - 4 * b : b, c[i]);
        if (i) { plat_checker(0, i * b - 1, W, 1, c[i]); plat_checker(1, i * b, W, 1, c[i - 1]); }
    }
    for (i = 0; i < 12; i++) {                          /* twinkles */
        uint32_t h = doomo_hash32((uint32_t)i * 40503u + 11u);
        int ph = (int)((t / 2 + (h >> 16)) & 31);
        if (ph < 16) ui_sparkle((int)(h % (uint32_t)W), (int)((h >> 8) % (uint32_t)H), ph,
                                mg.hell ? PAL_RAMP(RAMP_LAVA, 5) : PAL_WHITE);
    }
    for (i = 0; i < 4; i++) {                           /* drifting vape puffs */
        int s = 8 + i * 3, span = W + 80, y = H / 3 + i * (H / 6);
        int x = (int)((uint32_t)(t * (1 + (i & 1)) / 2 + i * 120) % (uint32_t)span) - 40;
        uint8_t cc = mg.hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_RAMP(RAMP_VAPE, 5 + (i & 1));
        ui_fill_ellipse(x, y, s * 2, s / 2 + 1, cc);
        ui_fill_circle(x - s / 2, y - 2, s / 2, cc);
        ui_fill_circle(x + s / 2, y - 3, s / 2 + 1, cc);
    }
}

static void vapula(int t)
{
    int W = mg_w(), frame = S->pose > 4 ? DFRAME_ATTACK : (t / 16) & 1, rows;
    const DoomoImage *img = sprite_image(SPR_LORD_VAPULA, frame);
    int bob = ui_wave(t, 60, 2), cy = cloud_y(), k;
    if (S->inv > 50) img = sprite_image(SPR_LORD_VAPULA, DFRAME_WALK0);
    if (img) {
        DoomoImage c;
        int y = top_y() - (frame == DFRAME_ATTACK ? 12 : 0) + bob;
        rows = cy + 8 - y;
        c = mg_crop(img, 0, DOOMO_MIN(rows, img->h));
        plat_image(W / 2 - img->w / 2 + mg.ox / 2, y, &c, 1, 0, 0);
    }
    /* his cloud couch */
    for (k = -5; k <= 5; k++) {
        int x = W / 2 + k * (mg_big() ? 16 : 12), r = (mg_big() ? 13 : 10) - (k < 0 ? -k : k) / 2;
        int y = cy + ui_wave(t + k * 9, 50, 1);
        ui_fill_circle(x, y + 2, r + 1, mg.hell ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_RAMP(RAMP_VAPE, 4));
    }
    for (k = -5; k <= 5; k++) {
        int x = W / 2 + k * (mg_big() ? 16 : 12), r = (mg_big() ? 13 : 10) - (k < 0 ? -k : k) / 2;
        int y = cy + ui_wave(t + k * 9, 50, 1);
        ui_fill_circle(x, y, r, mg.hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_RAMP(RAMP_VAPE, 6));
        plat_rect(x - r / 2, y - r + 2, r / 2, 1, mg.hell ? PAL_RAMP(RAMP_BLOOD, 4) : PAL_RAMP(RAMP_VAPE, 7));
    }
}

static void cherub(int x, int y, int t, int flap, int focus, int hide)
{
    int f = (flap / 3) & 1, k, r = mg_big() ? 6 : 5;
    uint8_t skin = PAL_PAINT(PAINT_PEACH, 2), skin_d = PAL_PAINT(PAINT_PEACH, 0);
    if (hide) return;
    /* fluffy wings */
    for (k = -1; k <= 1; k += 2) {
        int wx = x + k * (r + 2), wy = y - 2 - (f ? 2 : 0);
        ui_fill_ellipse(wx, wy, 4, f ? 3 : 4, PAL_RAMP(RAMP_SKY, 6));
        ui_fill_ellipse(wx, wy - 1, 3, f ? 2 : 3, PAL_WHITE);
    }
    ui_fill_circle(x, y, r + 1, skin_d);
    ui_fill_circle(x, y, r, skin);
    plat_rect(x - r / 2, y - r + 1, 2, 1, PAL_WHITE);
    plat_rect(x - 2, y - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
    plat_rect(x + 2, y - 1, 1, 2, PAL_RAMP(RAMP_STONE, 0));
    plat_rect(x - 4, y + 1, 2, 1, PAL_PAINT(PAINT_ROSE, 2));
    plat_rect(x + 3, y + 1, 2, 1, PAL_PAINT(PAINT_ROSE, 2));
    plat_rect(x, y + 2, 1, 1, PAL_PAINT(PAINT_ROSE, 0));
    /* golden curl + halo */
    plat_rect(x - 1, y - r - 1, 2, 1, PAL_PAINT(PAINT_HONEY, 2));
    ui_fill_ellipse(x, y - r - 4, 4, 1, PAL_UI_GOLD);
    plat_rect(x - 2, y - r - 4, 5, 1, mg.hell ? PAL_RAMP(RAMP_LAVA, 3) : PAL_RAMP(RAMP_VAPE, 4));
    if (focus) {
        plat_rect(x - 1, y - 1, 3, 3, PAL_WHITE);
        plat_rect(x, y, 1, 1, PAL_UI_RED);
        for (k = 0; k < 12; k++) {
            int a = k * 341 + t * 24;
            plat_rect(x + FIX2INT(fx_cos(a) * 13), y + FIX2INT(fx_sin(a) * 13), 1, 1, PAL_WHITE);
        }
    }
    (void)t;
}

static void dg_draw(void)
{
    int i, t = mg.play_t + mg.t, ox = mg.ox, oy = mg.oy;
    static const uint8_t cute_cols[3][2] = {
        { PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_PAINT(PAINT_MAGENTA, 0) },
        { PAL_PAINT(PAINT_LAVENDER, 2), PAL_PAINT(PAINT_GRAPE, 1) },
        { PAL_PAINT(PAINT_MINT, 1), PAL_PAINT(PAINT_TEAL, 0) }
    };
    static const uint8_t hell_cols[3][2] = {
        { PAL_RAMP(RAMP_LAVA, 5), PAL_RAMP(RAMP_BLOOD, 2) },
        { PAL_RAMP(RAMP_LAVA, 6), PAL_RAMP(RAMP_LAVA, 2) },
        { PAL_RAMP(RAMP_VAPE, 6), PAL_RAMP(RAMP_VAPE, 1) }
    };
    lounge_bg(t);
    vapula(t);
    cherub(S->px16 / 16 + ox, S->py16 / 16 + oy, t, S->flap, S->focus, S->inv && (S->inv & 4));
    for (i = 0; i < DG_BUL; i++) {
        const DBul *b = &S->b[i];
        int x, y;
        if (!b->r) continue;
        x = b->x16 / 16 + ox;
        y = b->y16 / 16 + oy;
        if (b->flags & 4) {
            ui_star(x, y, 5, PAL_UI_GOLD);
            ui_sparkle(x + 5, y - 5, (t / 2 + i) & 15, PAL_WHITE);
            continue;
        }
        {
            const uint8_t *c = mg.hell ? hell_cols[b->col % 3] : cute_cols[b->col % 3];
            ui_fill_circle(x, y, b->r + 1, c[1]);
            ui_fill_circle(x, y, b->r, c[0]);
            if (b->r >= 3) ui_fill_circle(x, y, b->r - 2, c[1]);
            plat_rect(x - b->r + 1, y - b->r + 1, 1, 1, PAL_WHITE);
        }
    }
    if (S->taunt_t) {                            /* a speech bubble right of his crown */
        const char *s = taunts[S->taunt];
        int x = mg_w() / 2 + (mg_big() ? 40 : 32), y = top_y() + (mg_big() ? 16 : 12);
        int w = DOOMO_MIN(mg_w() - 4 - x, ui_text_width(s) + 10);
        int lines = ui_text_wrap(x + 5, y + 3, w - 10, s, PAL_RAMP(RAMP_VAPE, 2), 0), k;
        int h = lines * FONT_LINE + 5, pop = ui_pop_scale(60 - S->taunt_t);
        if (pop < 200) h = h * pop / 256;
        ui_round_rect(x, y, w, h, 5, PAL_RAMP(RAMP_VAPE, 1));
        ui_round_rect(x + 1, y + 1, w - 2, h - 2, 4, PAL_WHITE);
        for (k = 0; k < 4; k++) plat_rect(x - 4 + k, y + 6 + k, 5 - k, 1, k ? PAL_WHITE : PAL_RAMP(RAMP_VAPE, 1));
        if (pop >= 200) ui_text_wrap(x + 5, y + 3, w - 10, s, PAL_RAMP(RAMP_VAPE, 2), 1);
    }
}

static void dg_art(int cx, int cy, int t)
{
    int k;
    for (k = 0; k < 3; k++) {
        int r = 4 + ((t / 2 + k * 10) % 30) / 3, y = cy - 6 - ((t / 2 + k * 10) % 30) / 2;
        ui_fill_circle(cx + 8, y, r, PAL_PAINT(PAINT_GRAPE, 1));
        ui_fill_circle(cx + 8, y, r - 2 > 0 ? r - 2 : 1, PAL_RAMP(RAMP_VAPE, 6));
    }
    cherub(cx - 8, cy + 8 + ui_wave(t, 40, 3), t, t, 0, 0);
}

static uint32_t dg_bot(void)
{
    static const int8_t dirs[9][2] = { {0,0}, {-1,0}, {1,0}, {0,-1}, {0,1}, {-1,-1}, {1,-1}, {-1,1}, {1,1} };
    int d, best = 0, bestv = 0x7fffffff, i, px = S->px16, py = S->py16;
    int H = mg_h(), W = mg_w();
    for (d = 0; d < 9; d++) {
        int nx = px + dirs[d][0] * 30 * 4, ny = py + dirs[d][1] * 30 * 4, v = 0;
        if (nx < 10 * 16 || nx > (W - 10) * 16 || ny < (cloud_y() + 16) * 16 || ny > (H - 10) * 16) continue;
        for (i = 0; i < DG_BUL; i++) {
            const DBul *b = &S->b[i];
            int bx, by, dx, dy, dd;
            if (!b->r) continue;
            bx = b->x16 + b->vx * 4;
            by = b->y16 + b->vy * 4;
            dx = (bx - nx) / 16;
            dy = (by - ny) / 16;
            dd = dx * dx + dy * dy;
            if (b->flags & 4) { if (dd < 900) v -= 2000; continue; }
            if (dd < 400) v += 4000 / (dd + 4);
        }
        v += (ny / 16 < H - 40 ? (H - 40 - ny / 16) : 0) * 2;          /* stay low */
        v += ((nx / 16 - W / 2) < 0 ? -(nx / 16 - W / 2) : (nx / 16 - W / 2)) / 4;
        if (v < bestv) { bestv = v; best = d; }
    }
    return (dirs[best][0] < 0 ? BTN_LEFT : 0) | (dirs[best][0] > 0 ? BTN_RIGHT : 0) |
           (dirs[best][1] < 0 ? BTN_UP : 0) | (dirs[best][1] > 0 ? BTN_DOWN : 0);
}

const MgGame mg_game_dodge = {
    "VAPE CLOUD DODGE", "Lord Vapula's lounge",
    "Dodge my rings, little cherub! Brush past them to GRAZE. Hold B to focus.",
    STR_ICON_ARROW_U STR_ICON_ARROW_D STR_ICON_ARROW_L STR_ICON_ARROW_R " fly  " STR_ICON_B " focus",
    MG_DODGE, PROG_HS_DODGE, SONG_VAPE_LOUNGE, 3, ROSTER_ID_LORD_VAPULA,
    { MG_DODGE_B, MG_DODGE_S, MG_DODGE_G }, 10, 60 * 30,
    { PAL_PAINT(PAINT_VAPE_PURPLE, 1), PAL_PAINT(PAINT_LAVENDER, 2), PAL_PAINT(PAINT_GRAPE, 1) },
    dg_start, dg_update, dg_draw, dg_art, dg_bot
};

MG_DEFINE_SCENE(SCENE_MG_DODGE, "MG_DODGE", mg_game_dodge);
