/* mg_catch.c - CUPCAKE CATCH (owner: minigames).
 *
 * Mochi, a plush bunny with a wicker basket over her head, catches treats
 * raining over the fair: cupcakes, donuts, cookies, candy and rare star
 * cakes (hearts give a life back). Brimstone bagels burn (lose a heart)
 * and falling imps bonk her dizzy and steal points. Every catch fills the
 * SUGAR RUSH meter; when it is full the fever starts: a rainbow fair, only
 * treats, double points. Three fevers in one round earn a trophy.
 */
#include "mg.h"

#define CT_ITEMS 14
enum { K_CUPCAKE, K_DONUT, K_COOKIE, K_CANDY, K_STAR, K_HEART, K_BAGEL, K_IMP };

typedef struct CItem {
    int16_t x, y16, vy16;
    uint8_t kind, live, wob, spin;
} CItem;

typedef struct Catch {
    int16_t px16, vx16;
    int8_t  face;
    uint8_t squash, stun, burn, step;
    uint16_t spawn_t, fever;
    uint8_t meter, fevers, carried, last_kind[3];
    CItem it[CT_ITEMS];
} Catch;
MG_ARENA_FITS(Catch);
#define S (MG_ARENA(Catch))

static const uint8_t points[8] = { 10, 15, 10, 20, 50, 0, 0, 0 };
static const uint8_t treat_of[6] = { TREAT_CUPCAKE, TREAT_DONUT, TREAT_COOKIE, TREAT_CANDY,
                                     TREAT_STAR, TREAT_HEART };

static int ground_y(void) { return mg_h() - (mg_big() ? 26 : 18); }
static int item_r(void) { return mg_big() ? 8 : 6; }
static int basket_w(void) { return mg_big() ? 34 : 26; }
/* the basket sits on her head: ground - 2 * body ry - 2 * head r - 4 */
static int basket_y(void) { return ground_y() - (mg_big() ? 50 : 37); }

static void ct_start(void)
{
    S->px16 = (int16_t)(mg_w() / 2 * 16);
    S->face = 1;
    S->spawn_t = 20;
}

static void spawn(void)
{
    CItem *c = NULL;
    int i, bad, roll, W = mg_w();
    static const uint8_t bad_pct[MG_TIERS] = { 14, 22, 32 };
    for (i = 0; i < CT_ITEMS; i++) if (!S->it[i].live) { c = &S->it[i]; break; }
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->live = 1;
    c->x = (int16_t)mg_between(14, W - 14);
    c->y16 = (int16_t)(-12 * 16);
    bad = !S->fever && mg_rand(100) < bad_pct[mg.tier] + mg.play_t / 300;
    roll = mg_rand(100);
    if (bad) c->kind = (uint8_t)(roll < 60 ? K_BAGEL : K_IMP);
    else if (roll < 4 && mg.lives < 3 && !S->fever) c->kind = K_HEART;
    else if (roll < (S->fever ? 16 : 8)) c->kind = K_STAR;
    else c->kind = (uint8_t)(roll % 4);
    {
        static const uint8_t base[MG_TIERS] = { 14, 18, 22 };
        int sp = base[mg.tier] + mg.play_t / 90;
        if (c->kind == K_IMP) sp += 6;
        if (c->kind == K_STAR) sp += 4;
        c->vy16 = (int16_t)(sp + mg_rand(6));
    }
    c->wob = (uint8_t)mg_rand(256);
}

static void catch_item(CItem *c)
{
    int bx = S->px16 / 16, by = basket_y();
    c->live = 0;
    S->squash = 8;
    if (c->kind <= K_STAR) {
        int base = points[c->kind] * (S->fever ? 2 : 1);
        mg_combo_hit(bx, by - 10);
        mg_add_score(base, bx, by - 16);
        mg_burst(c->kind == K_STAR ? MGFX_STAR : MGFX_SPARK, bx, by - 4, c->kind == K_STAR ? 8 : 5, 30,
                 c->kind == K_STAR ? PAL_UI_GOLD : PAL_PAINT(PAINT_BUBBLEGUM, 2));
        audio_sfx(c->kind == K_STAR ? SFX_SPARKLE : SFX_EAT);
        S->last_kind[S->carried % 3] = (uint8_t)c->kind;
        if (S->carried < 250) S->carried++;
        if (!S->fever) {
            S->meter = (uint8_t)DOOMO_MIN(100, S->meter + (c->kind == K_STAR ? 30 : 8));
            if (S->meter >= 100) {
                S->fever = 8 * 30;
                S->meter = 100;
                S->fevers++;
                mg_popup(mg_w() / 2, mg_h() / 3, "SUGAR RUSH!", 0, PAL_UI_GOLD);
                mg_confetti_rain(16);
                mg_shake_add(4);
                audio_sfx(SFX_EVOLVE);
                if (S->fevers >= 3) mg_trophy(MG_UL_SUGAR);
            }
        }
    } else if (c->kind == K_HEART) {
        if (mg.lives < mg.g->lives) mg_gain_life(bx, by - 16);
        else mg_add_score(25, bx, by - 16);
        mg_burst(MGFX_HEART, bx, by, 5, 24, PAL_PAINT(PAINT_ROSE, 2));
        audio_sfx(SFX_LOVE);
    } else if (c->kind == K_BAGEL) {
        S->burn = 20;
        mg_burst(MGFX_EMBER, bx, by, 8, 30, PAL_RAMP(RAMP_LAVA, 5));
        mg_burst(MGFX_PUFF, bx, by - 4, 3, 10, PAL_RAMP(RAMP_STONE, 3));
        mg_popup(bx, by - 18, "HOT HOT HOT!", 0, PAL_RAMP(RAMP_LAVA, 6));
        mg_lose_life(bx, by);
    } else {                                  /* an imp lands on her */
        S->stun = 30;
        mg_combo_break();
        mg_add_raw(-30, bx, by - 18);
        mg_shake_add(6);
        mg_hitstop(3);
        mg_splat(bx, by, 6);
        audio_sfx(SFX_SQUEAK);
    }
}

static void ct_update(void)
{
    int i, W = mg_w(), gy = ground_y(), by = basket_y(), bw = basket_w();
    int px, max = mg_big() ? 52 : 44, interval;
    static const uint8_t iv[MG_TIERS] = { 30, 22, 16 };
    /* player */
    if (S->stun) S->stun--;
    if (S->burn) S->burn--;
    if (S->squash) S->squash--;
    if (!S->stun) {
        if (mg_down(BTN_LEFT)) { S->vx16 = (int16_t)(S->vx16 - 9); S->face = -1; }
        if (mg_down(BTN_RIGHT)) { S->vx16 = (int16_t)(S->vx16 + 9); S->face = 1; }
    }
    S->vx16 = (int16_t)(S->vx16 * 13 / 16);
    S->vx16 = (int16_t)DOOMO_CLAMP(S->vx16, -max, max);
    S->px16 = (int16_t)DOOMO_CLAMP(S->px16 + S->vx16, 16 * 16, (W - 16) * 16);
    if (S->vx16 > 8 || S->vx16 < -8) S->step++;
    px = S->px16 / 16;
    /* fever */
    if (S->fever) {
        S->fever--;
        S->meter = (uint8_t)(S->fever * 100 / 240);
        if ((mg.play_t & 3) == 0) mg_fx(MGFX_STAR, mg_rand(W), mg_between(20, gy), 0, 6, PAL_WHITE, 12);
        if (!S->fever) { S->meter = 0; mg_popup(W / 2, mg_h() / 3, "phew...", 0, PAL_WHITE); }
    }
    /* spawning */
    interval = iv[mg.tier] - mg.play_t / 200;
    if (S->fever) interval /= 2;
    if (interval < 6) interval = 6;
    if (S->spawn_t) S->spawn_t--;
    else { spawn(); S->spawn_t = (uint16_t)(interval + mg_rand(interval / 2 + 1)); }
    /* falling */
    for (i = 0; i < CT_ITEMS; i++) {
        CItem *c = &S->it[i];
        int y0 = c->y16 / 16, y1;
        if (!c->live) continue;
        c->y16 = (int16_t)(c->y16 + c->vy16);
        if (c->vy16 < 60) c->vy16 = (int16_t)(c->vy16 + (c->kind == K_IMP ? 1 : 0));
        c->wob++;
        c->spin++;
        if (c->kind == K_CUPCAKE || c->kind == K_HEART) c->x = (int16_t)(c->x + ui_wave(c->wob, 40, 1) / 1);
        c->x = (int16_t)DOOMO_CLAMP(c->x, 8, W - 8);
        y1 = c->y16 / 16;
        if (y0 < by && y1 >= by && c->x > px - bw / 2 - 2 && c->x < px + bw / 2 + 2) {
            catch_item(c);
            continue;
        }
        if (y1 >= gy) {
            c->live = 0;
            if (c->kind <= K_STAR) {
                mg_burst(MGFX_CRUMB, c->x, gy, 6, 26, PAL_PAINT(PAINT_CARAMEL, 1));
                mg_burst(MGFX_CRUMB, c->x, gy, 3, 20, PAL_PAINT(PAINT_BUBBLEGUM, 1));
                mg_combo_break();
                audio_sfx(SFX_SPLAT);
            } else {
                mg_burst(MGFX_PUFF, c->x, gy - 2, 3, 10, c->kind == K_BAGEL ? PAL_RAMP(RAMP_STONE, 3) : PAL_PAINT(PAINT_CHERRY, 2));
            }
        }
    }
}

/* ---- drawing ------------------------------------------------------------------ */
static void basket(int cx, int y, int w, int t)
{
    int k, h = w / 2 - 2;
    /* treats peeking out of the basket */
    for (k = 0; k < 3 && k < S->carried; k++) {
        int kind = S->last_kind[k];
        mg_treat(kind <= K_STAR ? treat_of[kind] : TREAT_CUPCAKE, cx - w / 3 + k * w / 3, y + 1,
                 mg_big() ? 5 : 4, 256, 256, t);
    }
    ui_round_rect(cx - w / 2 - 1, y - 1, w + 2, h + 2, 4, PAL_RAMP(RAMP_WOOD, 1));
    ui_round_rect(cx - w / 2, y, w, h, 3, PAL_RAMP(RAMP_WOOD, 4));
    for (k = 2; k < h - 1; k += 3) plat_checker(cx - w / 2 + 1 + (k & 1), y + k, w - 2, 1, PAL_RAMP(RAMP_WOOD, 3));
    for (k = -w / 2 + 4; k < w / 2 - 2; k += 5) plat_rect(cx + k, y + 2, 1, h - 3, PAL_RAMP(RAMP_WOOD, 3));
    plat_rect(cx - w / 2, y, w, 2, PAL_RAMP(RAMP_WOOD, 6));
    plat_rect(cx - w / 2 + 2, y, w - 4, 1, PAL_RAMP(RAMP_WOOD, 7));
    /* a ribbon bow */
    ui_fill_circle(cx + w / 2 - 3, y + 3, 2, PAL_PAINT(PAINT_ROSE, 1));
    ui_fill_circle(cx + w / 2 + 1, y + 3, 2, PAL_PAINT(PAINT_ROSE, 1));
    plat_rect(cx + w / 2 - 1, y + 2, 2, 3, PAL_PAINT(PAINT_ROSE, 0));
}

static void bunny(int cx, int gy, int t)
{
    int big = mg_big(), sq = S->squash, k;
    int bx = (big ? 11 : 8), by = (big ? 9 : 7), hr = big ? 11 : 8;
    int stretch = sq > 4 ? -(sq - 4) : (sq ? sq / 2 : 0);
    int breathe = ui_wave(t, 50, 1);
    int lean = S->vx16 / 12;
    int bodyy = gy - by - 3, heady;
    uint8_t fur = PAL_PAINT(PAINT_WHITE, 1), fur_d = PAL_PAINT(PAINT_LAVENDER, 0), fur_l = PAL_WHITE;
    int run = (S->step / 3) & 1;
    if (S->burn & 2) { fur = PAL_RAMP(RAMP_WOOD, 3); fur_l = PAL_RAMP(RAMP_WOOD, 5); }
    mg_shadow(cx, gy, bx + 3);
    /* feet */
    ui_fill_ellipse(cx - bx / 2 + (run ? 1 : -1), gy - 2, 4, 2, fur_d);
    ui_fill_ellipse(cx + bx / 2 - (run ? 1 : -1), gy - 2, 4, 2, fur_d);
    ui_fill_ellipse(cx - bx / 2 + (run ? 1 : -1), gy - 3, 3, 2, fur);
    ui_fill_ellipse(cx + bx / 2 - (run ? 1 : -1), gy - 3, 3, 2, fur);
    /* fluffy tail */
    ui_fill_circle(cx - S->face * (bx + 1), bodyy + 2, 3, fur_d);
    ui_fill_circle(cx - S->face * (bx + 1), bodyy + 1, 2, fur_l);
    /* body with a striped tee */
    ui_fill_ellipse(cx + lean / 2, bodyy, bx + 1 - stretch / 2, by + 1 + stretch + breathe, fur_d);
    ui_fill_ellipse(cx + lean / 2, bodyy, bx - stretch / 2, by + stretch + breathe, PAL_PAINT(PAINT_MINT, 1));
    for (k = -by + 2; k < by; k += 3) plat_rect(cx + lean / 2 - bx + 2, bodyy + k, bx * 2 - 4, 1, PAL_PAINT(PAINT_MINT, 2));
    ui_fill_ellipse(cx + lean / 2, bodyy + by / 2, bx / 2, by / 3, fur_l);
    /* head */
    heady = bodyy - by - hr + 3 + stretch / 2 + breathe;
    ui_fill_circle(cx + lean, heady, hr + 1, fur_d);
    ui_fill_circle(cx + lean, heady, hr, fur);
    ui_fill_ellipse(cx + lean - hr / 3, heady - hr / 2, hr / 2, hr / 3, fur_l);
    /* floppy lop ears hanging beside her cheeks (a twitch now and then) */
    for (k = -1; k <= 1; k += 2) {
        int ex = cx + lean + k * (hr + 1), tw = (k > 0 && (t % 100) < 5) ? -2 : 0;
        int ey = heady + 1 + tw + ui_wave(t + k * 9, 50, 1), eh = big ? 8 : 6;
        ui_fill_ellipse(ex, ey, 3 + big, eh + 1, fur_d);
        ui_fill_ellipse(ex, ey, 2 + big, eh, fur);
        ui_fill_ellipse(ex - k, ey + 1, 1, eh - 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    }
    if (S->stun) {
        for (k = -1; k <= 1; k += 2) {
            int ex = cx + lean + k * hr / 2, ey = heady;
            plat_line(ex - 2, ey - 2, ex + 2, ey + 2, PAL_RAMP(RAMP_STONE, 1));
            plat_line(ex - 2, ey + 2, ex + 2, ey - 2, PAL_RAMP(RAMP_STONE, 1));
        }
        for (k = 0; k < 3; k++) {
            int a = (t * 150 + k * 1365) & DOOMO_ANG_MASK;
            ui_sparkle(cx + FIX2INT(fx_cos(a) * (hr + 4)), heady - hr - 2 + FIX2INT(fx_sin(a) * 3), 6, PAL_UI_GOLD);
        }
    } else {
        int blink = (t % 90) < 3, er = big ? 3 : 2;
        for (k = -1; k <= 1; k += 2) {
            int ex = cx + lean + k * hr / 2 + S->face, ey = heady;
            if (blink || (sq > 4)) { plat_rect(ex - er, ey, er * 2 + 1, 1, PAL_RAMP(RAMP_STONE, 0)); continue; }
            ui_fill_ellipse(ex, ey, er, er + 1, PAL_RAMP(RAMP_STONE, 0));
            plat_rect(ex - 1, ey - er + 1, 1, 1, PAL_WHITE);
            plat_rect(ex + 1, ey + 1, 1, 1, PAL_PAINT(PAINT_SKY, 2));
        }
    }
    plat_rect(cx + lean - hr / 2 - 3, heady + 3, 3, 1, PAL_PAINT(PAINT_ROSE, 2));
    plat_rect(cx + lean + hr / 2 + 1, heady + 3, 3, 1, PAL_PAINT(PAINT_ROSE, 2));
    plat_rect(cx + lean, heady + 2, 1, 1, PAL_PAINT(PAINT_ROSE, 0));
    plat_rect(cx + lean - 1, heady + 4, 1, 1, PAL_RAMP(RAMP_STONE, 1));
    plat_rect(cx + lean + 1, heady + 4, 1, 1, PAL_RAMP(RAMP_STONE, 1));
    /* the basket balanced on her head, little paws on the rim */
    {
        int bw = basket_w(), byy = basket_y() + (sq ? sq / 3 : 0) + stretch / 2 + breathe;
        basket(cx + lean, byy, bw, t);
        ui_fill_circle(cx + lean - bw / 2 - 1, byy + bw / 4, 3, fur_d);
        ui_fill_circle(cx + lean + bw / 2, byy + bw / 4, 3, fur_d);
        ui_fill_circle(cx + lean - bw / 2 - 1, byy + bw / 4 - 1, 2, fur);
        ui_fill_circle(cx + lean + bw / 2, byy + bw / 4 - 1, 2, fur);
    }
    if (S->fever && (t & 2)) ui_sparkle(cx + ui_wave(t, 20, 14), heady - 4 + ui_wave(t + 5, 26, 10), (t / 2) & 15, PAL_UI_GOLD);
}

static void falling(const CItem *c, int t)
{
    int r = item_r(), x = c->x + mg.ox, y = c->y16 / 16 + mg.oy;
    int sy = 256 + ui_wave(c->wob * 3, 40, 24), sx = 512 - sy;
    switch (c->kind) {
    case K_IMP:
        mg_imp(x, y, r + 2, r + 1, t + c->wob, IMP_GRIN);
        break;
    case K_BAGEL:
        mg_treat(TREAT_BAGEL, x, y, r + 1, 256, 256, t + c->wob);
        break;
    default:
        if (c->kind == K_STAR) ui_sparkle(x - r - 2, y - r, (t + c->wob) & 15, PAL_WHITE);
        mg_treat(treat_of[c->kind], x, y, r, sx, sy, t + c->wob);
        break;
    }
}

static void ct_draw(void)
{
    int W = mg_w(), H = mg_h(), gy = ground_y(), i, t = mg.play_t + mg.t;
    int ox = mg.ox, oy = mg.oy;
    if (S->fever) {
        static const uint8_t rb[6] = { PAINT_BUBBLEGUM, PAINT_TANGERINE, PAINT_LEMON, PAINT_MINT, PAINT_SKY, PAINT_LAVENDER };
        int k, sw = mg_big() ? 24 : 18, off = (t * 2) % (sw * 6);
        for (k = -6; k * sw - off < W; k++) {
            int x = k * sw + off - sw * 6;
            if (x + sw < 0) continue;
            plat_rect(x, 0, sw, gy + oy, PAL_PAINT(rb[(k + 60) % 6], 2));
            plat_rect(x, 0, 2, gy + oy, PAL_WHITE);
        }
        for (k = 0; k < 6; k++)
            ui_sparkle((k * 53 + t) % W, 24 + (k * 37) % (gy - 30), (t / 2 + k * 3) & 15, PAL_WHITE);
    } else {
        mg_fair_sky(0, gy + oy, t);
    }
    /* candy-striped path and grass */
    plat_rect(0, gy + oy, W, H - gy, mg.hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_RAMP(RAMP_GRASS, 5));
    plat_rect(0, gy + oy, W, 2, mg.hell ? PAL_RAMP(RAMP_LAVA, 3) : PAL_RAMP(RAMP_GRASS, 6));
    for (i = 0; i < W; i += 12) {
        plat_rect(i + ox, gy + 5 + oy, 6, H - gy - 8, mg.hell ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_PASTEL_PINK, 6));
        plat_rect(i + 6 + ox, gy + 5 + oy, 6, H - gy - 8, mg.hell ? PAL_RAMP(RAMP_STONE, 2) : PAL_WHITE);
    }
    for (i = 3; i < W; i += 17) {
        plat_rect(i + ox, gy - 2 + oy, 1, 2, mg.hell ? PAL_RAMP(RAMP_STONE, 2) : PAL_RAMP(RAMP_GRASS, 4));
        plat_rect(i + 2 + ox, gy - 3 + oy, 1, 3, mg.hell ? PAL_RAMP(RAMP_STONE, 2) : PAL_RAMP(RAMP_GRASS, 4));
    }
    bunny(S->px16 / 16 + ox, gy + oy, t);
    for (i = 0; i < CT_ITEMS; i++) if (S->it[i].live) falling(&S->it[i], t);
    /* the sugar rush meter (left edge) */
    {
        int mh = mg_big() ? 90 : 64, mx = 4, my = 24;
        ui_round_rect(mx - 1, my - 1, 9, mh + 2, 4, PAL_RAMP(RAMP_STONE, 0));
        ui_round_rect(mx, my, 7, mh, 3, PAL_RAMP(RAMP_STONE, 3));
        if (S->meter) {
            int fh = mh * S->meter / 100;
            uint8_t c = S->fever ? PAL_PAINT((t / 3) % 2 ? PAINT_LEMON : PAINT_MAGENTA, 2) : PAL_PAINT(PAINT_BUBBLEGUM, 1);
            plat_rect(mx + 1, my + mh - fh, 5, fh, c);
            plat_rect(mx + 1, my + mh - fh, 1, fh, PAL_WHITE);
        }
        ui_heart(mx + 3, my + mh + 6, 3, S->fever ? PAL_UI_GOLD : PAL_PAINT(PAINT_BUBBLEGUM, 1));
    }
}

static void ct_art(int cx, int cy, int t)
{
    int sq = ui_wave(t * 2, 30, 40);
    mg_shadow(cx, cy + 14, 10);
    mg_treat(TREAT_CUPCAKE, cx, cy + 2 - (sq > 0 ? sq / 10 : 0), 9, 256 + sq / 2, 256 - sq / 2, t);
    ui_sparkle(cx - 12, cy - 12, (t / 2) & 15, PAL_WHITE);
    ui_sparkle(cx + 13, cy - 6, (t / 2 + 7) & 15, PAL_UI_GOLD);
}

static uint32_t ct_bot(void)
{
    /* plan by arrival time: the reachable treat that lands first (stars
     * first), then steer clear of bagels and imps about to land */
    int i, px = S->px16 / 16, by = basket_y(), bw = basket_w(), target = px, best_t = 9999;
    int speed = mg_big() ? 3 : 2;
    for (i = 0; i < CT_ITEMS; i++) {
        const CItem *c = &S->it[i];
        int y = c->y16 / 16, ticks, dist;
        if (!c->live || y > by || c->kind >= K_BAGEL) continue;
        ticks = (by - y) * 16 / (c->vy16 > 0 ? c->vy16 : 1);
        dist = c->x > px ? c->x - px : px - c->x;
        if (dist - bw / 2 > ticks * speed) continue;            /* out of reach */
        if (c->kind == K_STAR) ticks -= 20;
        if (ticks < best_t) { best_t = ticks; target = c->x; }
    }
    for (i = 0; i < CT_ITEMS; i++) {
        const CItem *c = &S->it[i];
        int y = c->y16 / 16, ticks;
        if (!c->live || c->kind < K_BAGEL || y > by) continue;
        ticks = (by - y) * 16 / (c->vy16 > 0 ? c->vy16 : 1);
        if (ticks < 28 && c->x > target - bw / 2 - 10 && c->x < target + bw / 2 + 10)
            target = c->x < px ? c->x + bw / 2 + 14 : c->x - bw / 2 - 14;
    }
    if (target < px - 2) return BTN_LEFT;
    if (target > px + 2) return BTN_RIGHT;
    return 0;
}

const MgGame mg_game_catch = {
    "CUPCAKE CATCH", "a Sugar Rush booth",
    "Catch the treats! Bagels burn, imps bonk. Fill the heart for a SUGAR RUSH!",
    STR_ICON_ARROW_L STR_ICON_ARROW_R " move the basket",
    MG_CATCH, PROG_HS_CATCH, SONG_FESTIVAL, 3, ROSTER_ID_NANA_PUDDING,
    { MG_CATCH_B, MG_CATCH_S, MG_CATCH_G }, 14, 60 * 30,
    { PAL_PAINT(PAINT_BUBBLEGUM, 1), PAL_WHITE, PAL_PAINT(PAINT_CHERRY, 1) },
    ct_start, ct_update, ct_draw, ct_art, ct_bot
};

MG_DEFINE_SCENE(SCENE_MG_CATCH, "MG_CATCH", mg_game_catch);
