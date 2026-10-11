/* mg_gallery.c - BUBBLE BLASTER GALLERY (owner: minigames).
 *
 * A classic shooting gallery with Rivet's cute weld weapons. Three rails
 * of cardboard cutouts slide behind painted wooden waves: cardboard
 * demons (pop them!), golden demons (pop them twice as hard) and
 * cardboard citizens (spare them - they are cardboard, but they have
 * feelings). Balloons drift up now and then for extra time. L/R switches
 * between the three weapons:
 *   Bubble Wand     fast, precise single bubbles
 *   Plush Launcher  slow lobbed plushie, big squishy splash (multi-pop)
 *   Confetti Cannon a three-way spray
 * A round without a single citizen hit earns the Pacifist trophy.
 */
#include "mg.h"

#define RAILS 3
#define PER_RAIL 5
#define SHOTS 6
enum { T_IMP, T_WRAITH, T_GOLD, T_CITIZEN, T_BALLOON };
enum { TS_UP, TS_FLIP, TS_DOWN, TS_RISE };
enum { W_WAND, W_PLUSH, W_CONFETTI, W_COUNT };

typedef struct GTarget {
    int16_t x16;
    uint8_t kind, st, t, who;
} GTarget;

typedef struct GShot {
    int16_t x, y, tx, ty;
    uint8_t t, kind;
} GShot;

typedef struct Gallery {
    GTarget tg[RAILS][PER_RAIL];
    GShot shot[SHOTS];
    int16_t cx16, cy16, cvx, cvy;
    int16_t bal_x, bal_y16;
    uint8_t weapon, cool, recoil, citizens_hit, bal_live, wswitch;
    int16_t cit[4];             /* resident ids for cardboard citizens */
    uint8_t ncit;
} Gallery;
MG_ARENA_FITS(Gallery);
#define S (MG_ARENA(Gallery))

static const char *const wnames[W_COUNT] = { "Bubble Wand", "Plush Launcher", "Confetti Cannon" };
static const uint8_t wcool[W_COUNT] = { 6, 22, 13 };

static int rail_top(void) { return mg_big() ? 52 : 34; }
static int row_h(void) { return (mg_h() - rail_top() - (mg_big() ? 44 : 30)) / RAILS; }
static int rail_y(int r) { return rail_top() + (r + 1) * row_h(); }  /* the wave line */
static int tsize(void) { return mg_big() ? 12 : 9; }
static int rail_speed(int r) { return (r & 1 ? -1 : 1) * (6 + mg.tier * 4 + r * 2 + mg.play_t / 200); }

static void pick_citizens(void)
{
    int i, n = 0, start = mg_rand(DOOMO_DISTRICT_SIZE);
    for (i = 0; i < DOOMO_DISTRICT_SIZE && n < 4; i++) {
        int id = (start + i * 7) % DOOMO_DISTRICT_SIZE;
        if (town_alive(id)) S->cit[n++] = (int16_t)id;
    }
    S->ncit = (uint8_t)n;
}

static int new_kind(void)
{
    int roll = mg_rand(100);
    if (roll < 22 + mg.tier * 4) return T_CITIZEN;
    if (roll < 30 + mg.tier * 4) return T_GOLD;
    if (roll < 52) return T_WRAITH;
    return T_IMP;
}

static void gl_start(void)
{
    int r, i, W = mg_w();
    pick_citizens();
    for (r = 0; r < RAILS; r++)
        for (i = 0; i < PER_RAIL; i++) {
            GTarget *g = &S->tg[r][i];
            g->x16 = (int16_t)((i * (W + 40) / PER_RAIL - 20 + r * 17) * 16);
            g->kind = (uint8_t)new_kind();
            g->who = (uint8_t)mg_rand(4);
            g->st = TS_UP;
        }
    S->cx16 = (int16_t)(W / 2 * 16);
    S->cy16 = (int16_t)(rail_y(1) * 16 - tsize() * 16);
}

static void pop_target(GTarget *g, int x, int y, int *pops)
{
    g->st = TS_FLIP;
    g->t = 0;
    switch (g->kind) {
    case T_CITIZEN:
        S->citizens_hit++;
        mg_combo_break();
        mg_add_raw(-60, x, y - 10);
        mg_popup(x, y - 22, "OUCH! I have feelings!", 0, PAL_PAINT(PAINT_CHERRY, 2));
        mg_shake_add(5);
        mg_flash(PAL_UI_RED, 3);
        audio_sfx(SFX_SAD);
        break;
    case T_GOLD:
        mg_combo_hit(x, y);
        mg_add_score(80, x, y - 10);
        mg_burst(MGFX_STAR, x, y, 10, 40, PAL_UI_GOLD);
        audio_sfx(SFX_SPARKLE);
        (*pops)++;
        break;
    default:
        mg_combo_hit(x, y);
        mg_add_score(g->kind == T_WRAITH ? 25 : 20, x, y - 10);
        mg_splat(x, y, 6);
        audio_sfx(SFX_BUBBLE);
        (*pops)++;
        break;
    }
}

static int target_y(int r) { return rail_y(r) - tsize() - 4; }

static void hit_at(int x, int y, int radius)
{
    int r, i, pops = 0;
    for (r = 0; r < RAILS; r++)
        for (i = 0; i < PER_RAIL; i++) {
            GTarget *g = &S->tg[r][i];
            int dx, dy, rr = radius + tsize();
            if (g->st != TS_UP) continue;
            dx = g->x16 / 16 - x;
            dy = target_y(r) - y;
            if (dx * dx + dy * dy <= rr * rr) pop_target(g, g->x16 / 16, target_y(r), &pops);
        }
    if (S->bal_live) {
        int dx = S->bal_x - x, dy = S->bal_y16 / 16 - y;
        if (dx * dx + dy * dy <= (radius + 8) * (radius + 8)) {
            S->bal_live = 0;
            mg.time_left = (uint16_t)DOOMO_MIN(mg.time_left + 3 * 30, mg.g->time);
            mg_popup(S->bal_x, S->bal_y16 / 16 - 10, "+3 SEC!", 0, PAL_PAINT(PAINT_MINT, 2));
            mg_burst(MGFX_CONFETTI, S->bal_x, S->bal_y16 / 16, 10, 40, 0);
            audio_sfx(SFX_CONFETTI);
        }
    }
    if (pops >= 2) {
        mg_add_raw(pops * 15, x, y - 24);
        mg_popup(x, y - 34, pops >= 3 ? "TRIPLE POP!" : "DOUBLE POP!", 0, PAL_UI_GOLD);
    }
}

static void fire(void)
{
    int x = S->cx16 / 16, y = S->cy16 / 16, k, n = S->weapon == W_CONFETTI ? 3 : 1;
    for (k = 0; k < n; k++) {
        int i, tx = x + (n > 1 ? (k - 1) * 13 : 0), ty = y + (n > 1 && k != 1 ? 4 : 0);
        for (i = 0; i < SHOTS; i++) if (!S->shot[i].t) break;
        if (i < SHOTS) {
            GShot *s = &S->shot[i];
            s->x = (int16_t)(mg_w() / 2);
            s->y = (int16_t)(mg_h() - 10);
            s->tx = (int16_t)tx;
            s->ty = (int16_t)ty;
            s->t = 1;
            s->kind = S->weapon;
        }
    }
    S->cool = wcool[S->weapon];
    S->recoil = 6;
    audio_sfx(S->weapon == W_WAND ? SFX_BUBBLE : S->weapon == W_PLUSH ? SFX_SQUEAK : SFX_CONFETTI);
}

static void land(const GShot *s)
{
    switch (s->kind) {
    case W_PLUSH:
        hit_at(s->tx, s->ty, 18);
        mg_burst(MGFX_PUFF, s->tx, s->ty, 6, 30, PAL_WHITE);
        mg_burst(MGFX_HEART, s->tx, s->ty, 4, 30, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        mg_shake_add(4);
        break;
    case W_CONFETTI:
        hit_at(s->tx, s->ty, 5);
        mg_burst(MGFX_CONFETTI, s->tx, s->ty, 5, 30, 0);
        break;
    default:
        hit_at(s->tx, s->ty, 4);
        mg_fx(MGFX_RING, s->tx, s->ty, 0, 0, PAL_PAINT(PAINT_SKY, 2), 8);
        break;
    }
}

static void gl_update(void)
{
    int r, i, W = mg_w(), acc = 10, max = mg_big() ? 64 : 52;
    /* crosshair with a little inertia */
    if (mg_down(BTN_LEFT)) S->cvx = (int16_t)(S->cvx - acc);
    else if (mg_down(BTN_RIGHT)) S->cvx = (int16_t)(S->cvx + acc);
    else S->cvx = (int16_t)(S->cvx / 2);
    if (mg_down(BTN_UP)) S->cvy = (int16_t)(S->cvy - acc);
    else if (mg_down(BTN_DOWN)) S->cvy = (int16_t)(S->cvy + acc);
    else S->cvy = (int16_t)(S->cvy / 2);
    S->cvx = (int16_t)DOOMO_CLAMP(S->cvx, -max, max);
    S->cvy = (int16_t)DOOMO_CLAMP(S->cvy, -max, max);
    S->cx16 = (int16_t)DOOMO_CLAMP(S->cx16 + S->cvx, 8 * 16, (W - 8) * 16);
    S->cy16 = (int16_t)DOOMO_CLAMP(S->cy16 + S->cvy, (rail_top() - 10) * 16, (mg_h() - 30) * 16);
    if (mg_pressed(BTN_L)) { S->weapon = (uint8_t)((S->weapon + W_COUNT - 1) % W_COUNT); S->wswitch = 30; audio_sfx(SFX_CURSOR); }
    if (mg_pressed(BTN_R)) { S->weapon = (uint8_t)((S->weapon + 1) % W_COUNT); S->wswitch = 30; audio_sfx(SFX_CURSOR); }
    if (S->wswitch) S->wswitch--;
    if (S->cool) S->cool--;
    if (S->recoil) S->recoil--;
    if (mg_down(BTN_A) && !S->cool) fire();
    /* shots in flight */
    for (i = 0; i < SHOTS; i++) {
        GShot *s = &S->shot[i];
        if (!s->t) continue;
        if (++s->t > (s->kind == W_PLUSH ? 9 : 5)) { land(s); s->t = 0; }
    }
    /* rails */
    for (r = 0; r < RAILS; r++) {
        int sp = rail_speed(r);
        for (i = 0; i < PER_RAIL; i++) {
            GTarget *g = &S->tg[r][i];
            g->x16 = (int16_t)(g->x16 + sp);
            if (g->x16 > (W + 24) * 16) g->x16 = (int16_t)(g->x16 - (W + 48) * 16);
            if (g->x16 < -24 * 16) g->x16 = (int16_t)(g->x16 + (W + 48) * 16);
            g->t++;
            if (g->st == TS_UP && g->t >= 150 + (i * 37 + r * 23) % 90) { g->st = TS_FLIP; g->t = 0; }
            else if (g->st == TS_FLIP && g->t >= 8) { g->st = TS_DOWN; g->t = 0; }
            else if (g->st == TS_DOWN && g->t >= 40 + mg_rand(30)) {
                g->st = TS_RISE; g->t = 0;
                g->kind = (uint8_t)new_kind();
                g->who = (uint8_t)mg_rand(4);
            } else if (g->st == TS_RISE && g->t >= 8) { g->st = TS_UP; g->t = 0; }
        }
    }
    /* a bonus balloon */
    if (!S->bal_live && mg_rand(260) == 0) {
        S->bal_live = 1;
        S->bal_x = (int16_t)mg_between(20, W - 20);
        S->bal_y16 = (int16_t)(mg_h() * 16);
    }
    if (S->bal_live) {
        S->bal_y16 = (int16_t)(S->bal_y16 - 10 - mg.tier * 3);
        if (S->bal_y16 < -20 * 16) S->bal_live = 0;
    }
    if (mg.time_left == 1 && !S->citizens_hit && mg.score >= mg.g->medal[0]) mg_trophy(MG_UL_PACIFIST);
}

/* ---- drawing ------------------------------------------------------------------ */
static void cutout(const GTarget *g, int x, int base, int t)
{
    int s = tsize(), h = 256, back = 0;
    if (g->st == TS_FLIP) { h = 256 - g->t * 32; back = g->t >= 4; }
    else if (g->st == TS_RISE) { h = g->t * 32; back = g->t < 4; }
    else if (g->st == TS_DOWN) return;
    if (h < 32) h = 32;
    {
        int ch = (s * 2 + 6) * h / 256, cy = base - ch, cw = s * 2 + 4;
        plat_rect(x - 1, base - 4, 3, 6, PAL_RAMP(RAMP_WOOD, 2));         /* the stick */
        ui_round_rect(x - cw / 2 - 1, cy - 1, cw + 2, ch + 2, 4, PAL_RAMP(RAMP_WOOD, 2));
        ui_round_rect(x - cw / 2, cy, cw, ch, 3, back ? PAL_PAINT(PAINT_CARAMEL, 1) : PAL_PAINT(PAINT_BONE, 1));
        if (back) {
            plat_rect(x - cw / 2 + 3, cy + ch / 2, cw - 6, 1, PAL_PAINT(PAINT_CARAMEL, 0));
            return;
        }
        if (h < 200) return;
        switch (g->kind) {
        case T_CITIZEN:
            if (S->ncit) {
                const DoomoImage *img = bodyspr_image(S->cit[g->who % S->ncit], 0, (t / 12) & 1);
                if (img) plat_image(x - img->w / 2, cy + ch - img->h - 1, img, 1, 0, 0);
            } else {
                mg_mascot(x, cy + ch / 2 + 3, s - 3, t);
            }
            plat_rect(x - cw / 2 + 2, cy + 2, cw - 4, 1, PAL_WHITE);
            break;
        case T_WRAITH:
            mg_wraith(x - 1, cy + ch / 2 - 2, s - 3, t, 0);
            break;
        default:
            mg_imp(x, cy + ch / 2 + 1, s - 2, s - 3, t, IMP_GRIN | (g->kind == T_GOLD ? IMP_GOLD : 0));
            break;
        }
    }
}

static void waves(int y, int r, int t)
{
    int W = mg_w(), x, wl = 16, off = ((r & 1 ? t : -t) / 2) % wl;
    uint8_t c = mg.hell ? PAL_RAMP(RAMP_LAVA, 3 + (r & 1)) : (r & 1 ? PAL_PAINT(PAINT_SKY, 1) : PAL_PAINT(PAINT_COBALT, 2));
    uint8_t d = mg.hell ? PAL_RAMP(RAMP_LAVA, 1) : PAL_PAINT(PAINT_COBALT, 0);
    plat_rect(0, y + 3, W, 8, d);
    plat_rect(0, y + 4, W, 7, c);
    for (x = -wl + off; x < W + wl; x += wl) {
        ui_fill_circle(x + wl / 2, y + 4, wl / 2, d);
        ui_fill_circle(x + wl / 2, y + 5, wl / 2 - 1, c);
        plat_rect(x + wl / 2 - 3, y, 4, 1, mg.hell ? PAL_RAMP(RAMP_LAVA, 6) : PAL_WHITE);
    }
}

static void weapon_draw(int x, int y, int w, int t, int recoil)
{
    int rc = recoil > 3 ? 3 : recoil;
    y += rc;
    switch (w) {
    case W_WAND:
        plat_rect(x - 1, y - 2, 3, 22, PAL_PAINT(PAINT_LAVENDER, 1));
        plat_rect(x, y - 2, 1, 22, PAL_PAINT(PAINT_LAVENDER, 2));
        ui_fill_circle(x, y - 9, 8, PAL_PAINT(PAINT_GRAPE, 1));
        ui_fill_circle(x, y - 9, 6, PAL_RAMP(RAMP_SKY, 6));
        ui_fill_circle(x, y - 9, 5, PAL_RAMP(RAMP_SKY, 7));
        plat_rect(x - 3, y - 13, 2, 2, PAL_WHITE);
        break;
    case W_PLUSH:
        ui_round_rect(x - 10, y - 14, 20, 30, 6, PAL_PAINT(PAINT_ROSE, 0));
        ui_round_rect(x - 9, y - 13, 18, 28, 5, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        plat_rect(x - 9, y - 4, 18, 3, PAL_PAINT(PAINT_LEMON, 1));
        ui_fill_circle(x, y - 15, 7, PAL_PAINT(PAINT_CHOCOLATE, 1));         /* the teddy */
        ui_fill_circle(x - 6, y - 21, 3, PAL_PAINT(PAINT_CHOCOLATE, 1));
        ui_fill_circle(x + 6, y - 21, 3, PAL_PAINT(PAINT_CHOCOLATE, 1));
        ui_fill_circle(x, y - 15, 6, PAL_PAINT(PAINT_CARAMEL, 1));
        plat_rect(x - 3, y - 17, 1, 2, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(x + 3, y - 17, 1, 2, PAL_RAMP(RAMP_STONE, 0));
        plat_rect(x - 1, y - 14, 3, 2, PAL_PAINT(PAINT_CHOCOLATE, 0));
        break;
    default: {
        int k;
        for (k = 0; k < 10; k++) {
            int hw = 4 + k;
            plat_rect(x - hw, y - 16 + k * 3, hw * 2, 3, k & 1 ? PAL_PAINT(PAINT_LEMON, 1) : PAL_PAINT(PAINT_MINT, 1));
        }
        ui_fill_ellipse(x, y - 17, 6, 2, PAL_RAMP(RAMP_STONE, 0));
        for (k = 0; k < 4; k++)
            plat_rect(x - 4 + k * 3, y - 20 - ((t / 3 + k) & 3), 2, 1, PAL_PAINT((k & 1) ? PAINT_CHERRY : PAINT_SKY, 1));
        break;
    }
    }
}

static void gl_draw(void)
{
    int W = mg_w(), H = mg_h(), t = mg.play_t + mg.t, r, i, ox = mg.ox, oy = mg.oy;
    int rt = rail_top();
    /* booth: back wall and a striped awning */
    plat_rect(0, rt - 4, W, H - rt, mg.hell ? PAL_RAMP(RAMP_BLOOD, 1) : PAL_PAINT(PAINT_NAVY, 2));
    for (i = 0; i < 16; i++) {
        uint32_t h = doomo_hash32((uint32_t)i * 9973u);
        int ph = (int)((t / 2 + (h >> 20)) & 31);
        ui_sparkle((int)(h % (uint32_t)W), rt + (int)((h >> 8) % (uint32_t)(H - rt)), ph < 16 ? ph : 0,
                   mg.hell ? PAL_RAMP(RAMP_LAVA, 5) : PAL_PAINT(PAINT_LEMON, 2));
    }
    mg_tent(0, rt - 4, mg.hell ? PAL_RAMP(RAMP_BLOOD, 3) : PAL_PAINT(PAINT_SKY, 1),
            mg.hell ? PAL_RAMP(RAMP_STONE, 1) : PAL_WHITE,
            mg.hell ? PAL_RAMP(RAMP_LAVA, 4) : PAL_PAINT(PAINT_COBALT, 1), t);
    mg_bulbs(2, rt - 6, W - 4, 4, t);
    for (r = 0; r < RAILS; r++) {
        int y = rail_y(r) + oy;
        plat_rect(0, y - 3, W, 3, PAL_RAMP(RAMP_WOOD, 1));
        for (i = 0; i < PER_RAIL; i++) cutout(&S->tg[r][i], S->tg[r][i].x16 / 16 + ox, y, t + r * 7 + i * 13);
        waves(y, r, t);
    }
    if (S->bal_live) {
        int bx = S->bal_x + ox + ui_wave(t, 50, 3), by = S->bal_y16 / 16 + oy;
        plat_line(bx, by + 8, bx + ui_wave(t + 10, 30, 2), by + 20, PAL_WHITE);
        ui_fill_ellipse(bx, by, 7, 8, PAL_PAINT(PAINT_CHERRY, 0));
        ui_fill_ellipse(bx, by, 6, 7, PAL_PAINT(PAINT_CHERRY, 1));
        plat_rect(bx - 3, by - 4, 2, 3, PAL_WHITE);
        mg_textc(bx, by - 3, "+3", PAL_WHITE);
    }
    /* counter + weapon */
    {
        int cy = H - (mg_big() ? 30 : 22);
        plat_rect(0, cy + oy, W, H - cy, PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(0, cy + oy, W, 3, PAL_RAMP(RAMP_WOOD, 5));
        plat_rect(0, cy + 3 + oy, W, 1, PAL_RAMP(RAMP_WOOD, 1));
        for (i = 0; i < W; i += 20) plat_rect(i, cy + 6 + oy, 10, H - cy - 8, PAL_RAMP(RAMP_WOOD, 4));
        weapon_draw(W / 2 + ox, H - 8 + oy, S->weapon, t, S->recoil);
        for (i = 0; i < W_COUNT; i++) {
            int dx = W / 2 + (i - 1) * 12 + (i == 0 ? -26 : i == 2 ? 26 : 0);
            if (i == 1) continue;
            ui_fill_circle(dx, H - 8, 3, i == S->weapon ? PAL_UI_GOLD : PAL_GRAY);
        }
        mg_text(6, cy + 7, wnames[S->weapon], S->wswitch ? PAL_UI_GOLD : PAL_WHITE);
        mg_text(W - 6 - ui_text_width("L/R swap"), cy + 7, "L/R swap", PAL_RAMP(RAMP_WOOD, 7));
    }
    /* shots in flight */
    for (i = 0; i < SHOTS; i++) {
        const GShot *s = &S->shot[i];
        int dur, x, y;
        if (!s->t) continue;
        dur = s->kind == W_PLUSH ? 9 : 5;
        x = s->x + (s->tx - s->x) * s->t / dur + ox;
        y = s->y + (s->ty - s->y) * s->t / dur + oy;
        if (s->kind == W_PLUSH) {
            y -= 24 * s->t * (dur - s->t) / (dur * dur / 4);
            ui_fill_circle(x, y, 5, PAL_PAINT(PAINT_CHOCOLATE, 1));
            ui_fill_circle(x, y, 4, PAL_PAINT(PAINT_CARAMEL, 1));
            ui_fill_circle(x - 4, y - 4, 2, PAL_PAINT(PAINT_CHOCOLATE, 1));
            ui_fill_circle(x + 4, y - 4, 2, PAL_PAINT(PAINT_CHOCOLATE, 1));
        } else if (s->kind == W_CONFETTI) {
            plat_rect(x - 1, y, 3, 2, PAL_PAINT(PAINT_LEMON, 2));
            plat_rect(x + 2, y + 2, 2, 2, PAL_PAINT(PAINT_BUBBLEGUM, 2));
        } else {
            ui_fill_circle(x, y, 4, PAL_PAINT(PAINT_SKY, 2));
            ui_fill_circle(x, y, 3, PAL_RAMP(RAMP_SKY, 7));
            plat_rect(x - 2, y - 2, 1, 1, PAL_WHITE);
        }
    }
    /* heart crosshair */
    {
        int cx = S->cx16 / 16 + ox, cy = S->cy16 / 16 + oy, k = S->cool ? 2 : 0, rad = S->weapon == W_PLUSH ? 12 : 8;
        uint8_t c = S->cool ? PAL_GRAY_L : PAL_PAINT(PAINT_CHERRY, 2);
        plat_rect(cx - rad - 3 + k, cy, 5, 1, c);
        plat_rect(cx + rad - 1 - k, cy, 5, 1, c);
        plat_rect(cx, cy - rad - 3 + k, 1, 5, c);
        plat_rect(cx, cy + rad - 1 - k, 1, 5, c);
        ui_heart_flat(cx, cy, 2, c);
        if (S->weapon == W_CONFETTI) { plat_rect(cx - 13, cy + 4, 2, 2, c); plat_rect(cx + 12, cy + 4, 2, 2, c); }
    }
}

static void gl_art(int cx, int cy, int t)
{
    int k = (t / 20) % 3;
    weapon_draw(cx, cy + 14, k, t, 0);
    ui_fill_circle(cx + 10, cy - 10 - (t % 40) / 3, 4, PAL_RAMP(RAMP_SKY, 7));
    plat_rect(cx + 8, cy - 12 - (t % 40) / 3, 1, 1, PAL_WHITE);
}

static uint32_t gl_bot(void)
{
    int r, i, best = -1, bd = 1 << 30, cx = S->cx16 / 16, cy = S->cy16 / 16, tx = cx, ty = cy;
    uint32_t b = 0;
    for (r = 0; r < RAILS; r++)
        for (i = 0; i < PER_RAIL; i++) {
            const GTarget *g = &S->tg[r][i];
            int x = (g->x16 + rail_speed(r) * 6) / 16, y = target_y(r), d;
            if (g->st != TS_UP || g->kind == T_CITIZEN || x < 10 || x > mg_w() - 10) continue;
            d = (x - cx) * (x - cx) + (y - cy) * (y - cy) - (g->kind == T_GOLD ? 2000 : 0);
            if (d < bd) { bd = d; best = r * PER_RAIL + i; tx = x; ty = y; }
        }
    if (best < 0) return 0;
    if (tx < cx - 2) b |= BTN_LEFT;
    if (tx > cx + 2) b |= BTN_RIGHT;
    if (ty < cy - 2) b |= BTN_UP;
    if (ty > cy + 2) b |= BTN_DOWN;
    if ((tx - cx) * (tx - cx) + (ty - cy) * (ty - cy) < 36 && mg_rand(100) < 35) b |= BTN_A;
    else if (mg_rand(100) < 3) b |= BTN_A;                      /* a jumpy shot */
    if (mg.play_t % 240 == 120) b |= BTN_R;
    return b;
}

const MgGame mg_game_gallery = {
    "BUBBLE BLASTER", "a Shooting Gallery",
    "Pop cardboard demons, spare cardboard citizens! L/R swaps weapons.",
    STR_ICON_ARROW_L STR_ICON_ARROW_R " aim  " STR_ICON_A " fire  L/R weapon",
    MG_GALLERY, PROG_HS_GALLERY, SONG_FESTIVAL, 0, ROSTER_ID_RIVET,
    { MG_GALLERY_B, MG_GALLERY_S, MG_GALLERY_G }, 32, 45 * 30,
    { PAL_PAINT(PAINT_SKY, 1), PAL_WHITE, PAL_PAINT(PAINT_COBALT, 1) },
    gl_start, gl_update, gl_draw, gl_art, gl_bot
};

MG_DEFINE_SCENE(SCENE_MG_GALLERY, "MG_GALLERY", mg_game_gallery);
