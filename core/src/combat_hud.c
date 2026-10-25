/* combat_hud.c - the shooter's HUD and first-person weapon (owner: action).
 *
 * Three layouts (OPT_HUD_LAYOUT): CUTE (floating pastel badges: your own
 * citizen's face, heart + health, shield + armour, candy ammo pill),
 * CLASSIC (a Doom-style status bar along the bottom, textured, with the
 * face in the middle) and MINIMAL. Both badge layouts morph from the cute
 * pastel look into a hellish blood-and-bone look as you walk onto hell
 * floors (combat_s.hell). Plus: crosshair per option + colour, hit
 * markers, floating damage numbers, directional damage chevrons, lock-on
 * reticle, boss bar, keys, objective, combo counter, pet buff, scope
 * overlay, low-health heartbeat, and the welded weapon in your paws with
 * bob, sway, kick, reload dip, muzzle flash.
 */
#include "combat_int.h"
#include "doomo/txt.h"      /* S3: the stashed story captions */

static DoomoView3D hud_view;      /* camera of the last built view */
static int hud_t;
static int obj_t;
static DoomoImage face_img, bar_img;

void combat_hud_reset(void)
{
    hud_t = 0;
    obj_t = 150;
}

int combat_hud_style(void) { return combat_s.hell; }

/* ---- projection (same maths as the reference raycaster) ------------------ */
static int focal_px(const DoomoView3D *v)
{
    int fov = opt_get(OPT_FOV);
    int plane = fov == 0 ? 36000 : fov == 2 ? 52000 : 43250;
    return (int)(((int64_t)v->vw / 2 * 65536) / plane);
}

void combat_hud_project(fix16 x, fix16 y, fix16 z, int *sx, int *sy, int *visible)
{
    const DoomoView3D *v = &hud_view;
    int64_t rx = (int64_t)x - v->x, ry = (int64_t)y - v->y;
    int64_t dirx = fx_cos(v->angle), diry = fx_sin(v->angle);
    int64_t depth = (rx * dirx + ry * diry) >> 16;
    int64_t lat = (rx * -diry + ry * dirx) >> 16;
    int focal = focal_px(v);
    int horizon = v->vy + v->vh / 2 + (int)((int64_t)v->pitch * focal * 84 / (256 * 100));
    int64_t eye = (int64_t)v->z + ((int64_t)v->bob * 65536);   /* (integration) bob may be < 0 */
    *visible = 0;
    if (depth < ((int64_t)8 << 16)) return;
    *sx = v->vx + v->vw / 2 + (int)(lat * focal / depth);
    *sy = horizon - (int)(((int64_t)z - eye) * focal / depth);
    *visible = *sx >= -16 && *sx < v->vx + v->vw + 16 && *sy >= -16 && *sy < v->vy + v->vh + 16;
}

/* ---- palette helpers ----------------------------------------------------- */
typedef struct HudPal {
    uint8_t fill, fill_hi, fill_lo, outline, text, text_out, accent, accent2, dim;
} HudPal;

static void hud_pal(HudPal *p)
{
    if (combat_s.hell >= 128) {
        p->fill = PAL_RAMP(RAMP_BLOOD, 1);
        p->fill_hi = PAL_RAMP(RAMP_BLOOD, 3);
        p->fill_lo = PAL_RAMP(RAMP_BLOOD, 0);
        p->outline = PAL_RAMP(RAMP_BONE, 3);
        p->text = PAL_RAMP(RAMP_BONE, 7);
        p->text_out = PAL_RAMP(RAMP_BLOOD, 0);
        p->accent = PAL_RAMP(RAMP_LAVA, 6);
        p->accent2 = PAL_RAMP(RAMP_LAVA, 4);
        p->dim = PAL_RAMP(RAMP_BONE, 4);
    } else {
        p->fill = PAL_UI_PINK;
        p->fill_hi = PAL_RAMP(RAMP_PASTEL_PINK, 7);
        p->fill_lo = PAL_RAMP(RAMP_PASTEL_PINK, 4);
        p->outline = PAL_UI_PINK_D;
        p->text = PAL_WHITE;
        p->text_out = PAL_UI_PINK_D;
        p->accent = PAL_PAINT(PAINT_CHERRY, 1);
        p->accent2 = PAL_UI_MINT;
        p->dim = PAL_RAMP(RAMP_PASTEL_PINK, 2);
    }
}

static void panel(int x, int y, int w, int h, const HudPal *p)
{
    ui_round_rect(x, y, w, h, 4, p->outline);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 3, p->fill);
    plat_rect(x + 3, y + 2, w - 6, 1, p->fill_hi);
    plat_rect(x + 3, y + h - 3, w - 6, 1, p->fill_lo);
}

static void num_big(int x, int y, int value, uint8_t col, uint8_t out)
{
    char buf[8];
    doomo_snprintf(buf, sizeof(buf), "%d", value);
    ui_text_big(x, y, buf, 2, col, out);
}

/* ---- icons drawn with primitives ----------------------------------------- */
static void icon_shield(int cx, int cy, uint8_t fill, uint8_t out)
{
    ui_round_rect(cx - 5, cy - 5, 11, 8, 2, out);
    ui_fill_ellipse(cx, cy + 2, 5, 5, out);
    ui_round_rect(cx - 4, cy - 4, 9, 6, 2, fill);
    ui_fill_ellipse(cx, cy + 2, 4, 4, fill);
    plat_rect(cx - 3, cy - 3, 2, 3, ui_lighter(fill));
}

static void icon_skull(int cx, int cy, uint8_t fill, uint8_t out)
{
    ui_fill_circle(cx, cy - 1, 5, out);
    ui_round_rect(cx - 3, cy + 1, 7, 5, 1, out);
    ui_fill_circle(cx, cy - 1, 4, fill);
    ui_round_rect(cx - 2, cy + 1, 5, 4, 1, fill);
    plat_rect(cx - 3, cy - 1, 2, 2, out);
    plat_rect(cx + 2, cy - 1, 2, 2, out);
    plat_rect(cx - 1, cy + 3, 1, 2, out);
    plat_rect(cx + 1, cy + 3, 1, 2, out);
}

static void icon_key(int x, int y, uint8_t col)
{
    ui_heart(x + 4, y + 4, 3, col);
    plat_rect(x + 3, y + 7, 2, 7, PAL_UI_GOLD);
    plat_rect(x + 5, y + 10, 2, 1, PAL_UI_GOLD);
    plat_rect(x + 5, y + 12, 2, 1, PAL_UI_GOLD);
}

static void icon_paw(int cx, int cy, uint8_t fill, uint8_t out)
{
    ui_fill_ellipse(cx, cy + 2, 4, 3, out);
    ui_fill_ellipse(cx, cy + 2, 3, 2, fill);
    ui_fill_circle(cx - 4, cy - 2, 2, out); ui_fill_circle(cx - 4, cy - 2, 1, fill);
    ui_fill_circle(cx - 1, cy - 4, 2, out); ui_fill_circle(cx - 1, cy - 4, 1, fill);
    ui_fill_circle(cx + 2, cy - 4, 2, out); ui_fill_circle(cx + 2, cy - 4, 1, fill);
    ui_fill_circle(cx + 5, cy - 2, 2, out); ui_fill_circle(cx + 5, cy - 2, 1, fill);
}

/* ---- your citizen's face ----------------------------------------------- */
static int face_expr(void)
{
    if (doomo_player.hp <= 0) return EXPR_DEAD;
    if (combat_px.face_t > 0) return combat_px.face_expr;
    if (doomo_player.hp < 30) return EXPR_SAD;
    if (combat_px.combo >= 3 && combat_px.combo_t > 0) return EXPR_HAPPY;
    return EXPR_NEUTRAL;
}

static const DoomoImage *face_crop(int top, int rows)
{
    const DoomoImage *p = portrait_image(doomo_player.resident_id, face_expr());
    if (!p || !p->pix || p->h < top + rows) return p;
    face_img.w = p->w;
    face_img.h = (uint16_t)rows;
    face_img.pix = p->pix + top * p->w;
    face_img.uid = doomo_hash32(p->uid ^ 0x0FACE000u ^ (uint32_t)(top << 8) ^ (uint32_t)rows);
    return &face_img;
}

static void face_badge(int x, int y, const HudPal *p)
{
    const DoomoImage *f = face_crop(6, 36);
    int bounce = doomo_player.hurt_flash > 10 ? ui_wave(hud_t, 4, 2) : 0;
    ui_round_rect(x, y, 52, 40, 8, p->outline);
    ui_round_rect(x + 1, y + 1, 50, 38, 7, combat_s.hell >= 128 ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_UI_CREAM);
    ui_round_rect(x + 3, y + 3, 46, 34, 6, combat_s.hell >= 128 ? PAL_RAMP(RAMP_STONE, 1) : PAL_RAMP(RAMP_SKY, 6));
    if (f) plat_image(x + 2 + bounce, y + 3, f, 1, 0, 0);
    /* glossy corner + sparkle */
    plat_rect(x + 6, y + 2, 10, 1, PAL_WHITE);
    if (combat_s.hell < 128) ui_sparkle(x + 47, y + 5, hud_t / 3, PAL_UI_GOLD);
    else icon_skull(x + 46, y + 6, PAL_RAMP(RAMP_BONE, 6), PAL_RAMP(RAMP_BLOOD, 0));
}

/* ---- crosshair --------------------------------------------------------------- */
static uint8_t crosshair_color(void)
{
    static const uint8_t cols[6] = { PAL_WHITE, PAL_PAINT(PAINT_BUBBLEGUM, 2), PAL_UI_MINT,
                                     PAL_UI_GOLD, PAL_UI_RED, PAL_PAINT(PAINT_CYAN, 2) };
    int c = opt_get(OPT_CROSSHAIR_COLOR);
    return cols[c >= 0 && c < 6 ? c : 0];
}

static void crosshair(int cx, int cy)
{
    int kind = opt_get(OPT_CROSSHAIR);
    uint8_t col = crosshair_color(), dark = PAL_BLACK;
    int gap = 2 + combat_ws.spread / 12 + doomo_player.recoil / 6 + (doomo_player.sprint ? 2 : 0);
    if (doomo_player.crouch) gap = gap * 2 / 3;
    gap = DOOMO_MIN(gap, 12);
    switch (kind) {
    case 0:   /* dot */
        plat_rect(cx - 1, cy - 1, 3, 3, dark);
        plat_rect(cx, cy, 1, 1, col);
        break;
    case 1:   /* cross */
        plat_rect(cx - gap - 4, cy - 1, 4, 3, dark); plat_rect(cx + gap + 1, cy - 1, 4, 3, dark);
        plat_rect(cx - 1, cy - gap - 4, 3, 4, dark); plat_rect(cx - 1, cy + gap + 1, 3, 4, dark);
        plat_rect(cx - gap - 3, cy, 3, 1, col); plat_rect(cx + gap + 1, cy, 3, 1, col);
        plat_rect(cx, cy - gap - 3, 1, 3, col); plat_rect(cx, cy + gap + 1, 1, 3, col);
        break;
    case 2: { /* circle */
        int r = gap + 3, a;
        for (a = 0; a < 4096; a += 256) {
            int px = cx + FIX2INT(fx_cos(a) * r), py = cy + FIX2INT(fx_sin(a) * r);
            plat_rect(px, py, 1, 1, col);
        }
        plat_rect(cx, cy, 1, 1, col);
        break;
    }
    case 3:   /* heart */
        ui_heart(cx, cy, 3 + gap / 4, col);
        break;
    case 4:   /* star */
        ui_star(cx, cy, 4 + gap / 4, col);
        break;
    case 5:   /* skull */
        icon_skull(cx, cy, col, dark);
        break;
    case 6:   /* paw */
        icon_paw(cx, cy, col, dark);
        break;
    default:
        break;
    }
    /* charge / spin-up meter */
    if (combat_ws.charge && combat_px.charge_t > 0) {
        int w = 20 * combat_px.charge_t / combat_ws.charge;
        plat_rect(cx - 11, cy + 9, 22, 4, dark);
        plat_rect(cx - 10, cy + 10, w, 2, combat_px.charge_t >= combat_ws.charge ? PAL_UI_GOLD : col);
    }
    if (combat_ws.spin_up && combat_px.spin > 0 && combat_px.spin < 256) {
        plat_rect(cx - 11, cy + 9, 22, 4, dark);
        plat_rect(cx - 10, cy + 10, 20 * combat_px.spin / 256, 2, PAL_RAMP(RAMP_LAVA, 6));
    }
    /* hit marker */
    if (combat_px.hit_marker && opt_get(OPT_HIT_MARKERS)) {
        uint8_t hc = combat_px.hit_kill ? PAL_UI_RED : combat_px.hit_head ? PAL_UI_GOLD : PAL_WHITE;
        int d = 4 + (6 - DOOMO_MIN(combat_px.hit_marker, 6)) / 2 + (combat_px.hit_kill ? 2 : 0);
        int k;
        for (k = 0; k < 3; k++) {
            plat_rect(cx - d - k, cy - d - k, 1, 1, hc);
            plat_rect(cx + d + k, cy - d - k, 1, 1, hc);
            plat_rect(cx - d - k, cy + d + k, 1, 1, hc);
            plat_rect(cx + d + k, cy + d + k, 1, 1, hc);
        }
    }
}

/* ---- lock-on reticle ------------------------------------------------------- */
static void lock_reticle(void)
{
    const DoomoActor *t;
    int sx, sy, vis, r, k;
    if (doomo_player.lock_target < 0) return;
    t = &doomo_actors[doomo_player.lock_target];
    combat_hud_project(t->x, t->y, t->z + INT2FIX(t->height / 2), &sx, &sy, &vis);
    if (!vis) return;
    r = 10 + DOOMO_MAX(0, 8 - combat_px.lock_ui) + ui_wave(hud_t, 20, 1);
    for (k = 0; k < 4; k++) {
        int a = k * 1024 + hud_t * 24 + 512;
        int px = sx + FIX2INT(fx_cos(a) * r), py = sy + FIX2INT(fx_sin(a) * r);
        if (combat_s.hell >= 128) icon_skull(px, py, PAL_RAMP(RAMP_LAVA, 6), PAL_BLACK);
        else ui_heart(px, py, 3, PAL_PAINT(PAINT_BUBBLEGUM, 2));
    }
}

/* ---- damage numbers --------------------------------------------------------- */
static void damage_numbers(void)
{
    int i;
    for (i = 0; i < DMGNUM_MAX; i++) {
        const DmgNum *d = &combat_dmgnums[i];
        int sx, sy, vis;
        char buf[8];
        if (!d->t) continue;
        combat_hud_project(d->x, d->y, d->z, &sx, &sy, &vis);
        if (!vis) continue;
        doomo_snprintf(buf, sizeof(buf), "%d", d->value);
        sx += (i & 1) ? d->t / 3 : -d->t / 3;
        if (d->crit) ui_text_outline(sx - ui_text_width(buf) / 2, sy - 4, buf, PAL_UI_GOLD, PAL_UI_RED_D);
        else ui_text_outline(sx - ui_text_width(buf) / 2, sy - 4, buf,
                             combat_s.hell >= 128 ? PAL_RAMP(RAMP_BONE, 7) : PAL_WHITE,
                             combat_s.hell >= 128 ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_UI_PINK_D);
    }
}

/* ---- directional damage -------------------------------------------------------- */
static void damage_direction(void)
{
    int rel, sw = plat_screen_w(), sh = plat_screen_h(), cx, cy, k;
    if (!combat_px.dmg_dir_t) return;
    rel = ((combat_px.dmg_dir - doomo_player.angle + 2048) & DOOMO_ANG_MASK) - 2048;
    cx = sw / 2 + FIX2INT(fx_sin(rel) * (sw / 2 - 16));
    cy = sh / 2 - FIX2INT(fx_cos(rel) * (sh / 2 - 16));
    if ((combat_px.dmg_dir_t & 2) || combat_px.dmg_dir_t > 20) {
        for (k = 0; k < 3; k++) {
            int ix = cx - FIX2INT(fx_sin(rel) * (k * 4)), iy = cy + FIX2INT(fx_cos(rel) * (k * 4));
            ui_fill_circle(ix, iy, 3 - k / 2, k == 0 ? PAL_UI_RED : PAL_UI_RED_D);
        }
    }
}

/* ---- the welded weapon in your paws -------------------------------------------- */
static void draw_paw(int cx, int cy, int r, const DoomoLook *l)
{
    uint8_t base = look_skin(l, 1), dark = look_skin(l, 0), light = look_skin(l, 2);
    uint8_t out = ui_darker(dark);
    ui_fill_ellipse(cx, cy, r + 1, r, out);
    ui_fill_ellipse(cx, cy, r, r - 1, base);
    ui_fill_ellipse(cx - 1, cy - 1, r - 2, r - 3, light);
    /* little finger bumps + a fluffy tuft */
    ui_fill_circle(cx - r / 2 - 1, cy - r + 1, 2, out);
    ui_fill_circle(cx - r / 2 - 1, cy - r + 1, 1, base);
    ui_fill_circle(cx + 1, cy - r, 2, out);
    ui_fill_circle(cx + 1, cy - r, 1, base);
    ui_fill_circle(cx + r / 2 + 2, cy - r + 1, 2, out);
    ui_fill_circle(cx + r / 2 + 2, cy - r + 1, 1, base);
    plat_rect(cx - 2, cy + r - 2, 1, 2, dark);
    plat_rect(cx + 2, cy + r - 2, 1, 2, dark);
}

static void muzzle_flash(int mx, int my, const DoomoWeapon *w)
{
    int k, r = 6 + combat_px.flash * 2;
    int pk = combat_ws.pkind;
    uint8_t c1, c2;
    if (opt_get(OPT_REDUCE_FLASH)) r = r * 2 / 3;
    if (pk == PK_PLASMA || pk == PK_BFG || pk == PK_ICE || combat_ws.behaviour == WB_RAIL) {
        c1 = PAL_PAINT(PAINT_CYAN, 1); c2 = PAL_PAINT(PAINT_CYAN, 2);
    } else if (pk == PK_BUBBLE || pk == PK_WATER || pk == PK_HOLY) {
        c1 = PAL_PAINT(PAINT_SKY, 2); c2 = PAL_WHITE;
    } else if (pk == PK_HEART || pk == PK_GLITTER || pk == PK_CANDY || pk == PK_PLUSH ||
               w->style == WSTYLE_CUTE) {
        c1 = PAL_PAINT(PAINT_BUBBLEGUM, 1); c2 = PAL_PAINT(PAINT_LEMON, 2);
    } else {
        c1 = PAL_RAMP(RAMP_LAVA, 5); c2 = PAL_RAMP(RAMP_LAVA, 7);
    }
    if (w->style == WSTYLE_CUTE && combat_ws.behaviour != WB_RAIL) {
        ui_star(mx, my, r, c1);
        ui_star(mx, my, r / 2 + 1, c2);
        for (k = 0; k < 3; k++)
            ui_sparkle(mx + ((k * 37 + hud_t * 5) % 21) - 10, my - 4 - ((k * 23 + hud_t * 3) % 12), (hud_t + k * 5) & 15, c2);
    } else {
        for (k = 0; k < 6; k++) {
            int a = k * 682 + hud_t * 97;
            int px = mx + FIX2INT(fx_cos(a) * (r + 2)), py = my + FIX2INT(fx_sin(a) * (r - 1));
            ui_fill_circle(px, py, 2, c1);
        }
        ui_fill_circle(mx, my, r - 1, c1);
        ui_fill_circle(mx, my, r / 2, c2);
        plat_rect(mx - 1, my - 1, 3, 3, PAL_WHITE);
    }
}

static void weapon_view(void)
{
    const DoomoWeapon *w = combat_weapon();
    const DoomoImage *img;
    const DoomoResident *me = town_res(doomo_player.resident_id);
    int sw = plat_screen_w(), sh = plat_screen_h();
    int x, y, frame = 0, mx, my, gx, gy;
    int bob = combat_px.bob_amp;
    if (doomo_player.hp <= 0 && combat_px.dead_t > 10) return;
    if (combat_px.zoom_t > 160 && combat_ws.zoom >= 2) return;           /* behind the scope */
    /* idle + fire stay in the 2-slot sprite cache while auto-firing (the
     * kick offsets below animate the recovery), so the GBA never
     * recomposes a sprite per shot */
    if (combat_px.flash > 1) frame = 1;
    if (combat_px.reload_t && combat_ws.reload) {
        int prog = combat_ws.reload - combat_px.reload_t;
        if (prog > 4 && combat_px.reload_t > 4) frame = 3;
    }
    if (combat_ws.behaviour == WB_MELEE && combat_px.kick > 32) frame = 1;
    img = weld_view_image(w, frame);
    weld_view_points(w, &mx, &my, &gx, &gy);
    x = sw / 2 - WELD_VIEW_W / 2 + 14;
    y = sh - WELD_VIEW_H + 2;
    x += FIX2INT(fx_sin(combat_px.bob_phase) * bob * 2) + combat_px.sway_x / 16;
    y += FIX2INT((fx_cos(combat_px.bob_phase * 2) < 0 ? -fx_cos(combat_px.bob_phase * 2) : fx_cos(combat_px.bob_phase * 2)) * bob);
    y += combat_px.kick / 16;
    if (combat_ws.behaviour == WB_MELEE && combat_px.kick > 0) x -= combat_px.kick / 8;
    y += combat_px.raise * 72 / 256;
    if (combat_px.reload_t && combat_ws.reload) {
        int prog = combat_ws.reload - combat_px.reload_t;
        int dip = DOOMO_MIN(prog, combat_px.reload_t) * 3;
        y += DOOMO_MIN(dip, 26);
        x -= DOOMO_MIN(dip, 26) / 3;
    }
    if (doomo_player.sprint) { y += 8 + ui_wave(hud_t, 8, 2); x -= 10; }
    if (doomo_player.roll_timer) y += 28;
    if (combat_px.land_t) y += combat_px.land_t / 2;
    if (combat_px.charge_t && combat_ws.charge) {
        y += combat_px.charge_t * 6 / combat_ws.charge;
        if (combat_px.charge_t >= combat_ws.charge) x += (hud_t & 1);
    }
    if (combat_px.zoom_t > 0) {
        /* sights up: slide to the centre */
        int tx = sw / 2 - mx, ty = sh / 2 - my + 18;
        x += (tx - x) * combat_px.zoom_t / 400;
        y += (ty - y) * combat_px.zoom_t / 400;
    }
    /* the off-paw behind, the gun, the trigger paw in front */
    if (me && w->frame != WFRAME_FISTS)
        draw_paw(x + gx - 22, y + gy + 8, 7, &me->look);
    if (img) plat_image(x, y, img, 1, 0, 0);
    if (me && w->frame != WFRAME_FISTS && w->frame != WFRAME_DUAL_PISTOLS)
        draw_paw(x + gx, y + gy + 2, 8, &me->look);
    if (combat_px.flash && combat_ws.behaviour != WB_MELEE)
        muzzle_flash(x + mx, y + my, w);
    if (w->frame == WFRAME_DUAL_PISTOLS && combat_px.flash)
        muzzle_flash(x + mx - 40, y + my + 4, w);
}

static void scope_overlay(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2;
    int r = sh * 2 / 5 + (256 - combat_px.zoom_t) / 8, y;
    uint8_t ring = combat_s.hell >= 128 ? PAL_RAMP(RAMP_STEEL, 2) : PAL_UI_PINK_D;
    if (combat_px.zoom_t <= 160 || combat_ws.zoom < 2) return;
    for (y = 0; y < sh; y++) {
        int dy = y - cy, half;
        if (dy * dy >= r * r) { plat_rect(0, y, sw, 1, PAL_BLACK); continue; }
        half = (int)fx_isqrt((uint32_t)(r * r - dy * dy));
        plat_rect(0, y, cx - half, 1, PAL_BLACK);
        plat_rect(cx + half, y, sw - cx - half, 1, PAL_BLACK);
        plat_rect(cx - half, y, 2, 1, ring);
        plat_rect(cx + half - 2, y, 2, 1, ring);
    }
    plat_rect(cx - r + 6, cy, r - 10, 1, PAL_BLACK);
    plat_rect(cx + 5, cy, r - 10, 1, PAL_BLACK);
    plat_rect(cx, cy - r + 6, 1, r - 10, PAL_BLACK);
    plat_rect(cx, cy + 5, 1, r - 10, PAL_BLACK);
    if (combat_weapon()->sight == WSIGHT_HEART_DOT) ui_heart(cx, cy, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    else plat_rect(cx - 1, cy - 1, 3, 3, PAL_UI_RED);
}

/* ---- panels ---------------------------------------------------------------------- */
static void ammo_panel(int x, int y, const HudPal *p)
{
    const DoomoWeapon *w = combat_weapon();
    const DoomoImage *icon = weld_icon_image(w);
    char buf[16];
    int slot = doomo_player.weapon_slot;
    panel(x, y, 70, 26, p);
    if (icon) plat_image(x + 4, y + 5, icon, 1, 0, 0);
    if (combat_ws.ammo_type == 0xFF) {
        ui_text_outline(x + 32, y + 9, "MELEE", p->text, p->text_out);
        return;
    }
    if (combat_ws.mag_size) {
        int mag = combat_px.mag[slot];
        uint8_t col = mag * 4 < combat_ws.mag_size ? p->accent : p->text;
        if (combat_px.reload_t) {
            ui_text_outline(x + 30, y + 4, (hud_t & 8) ? "RELOAD" : "", p->accent2, p->text_out);
        } else {
            doomo_snprintf(buf, sizeof(buf), "%d", mag);
            ui_text_big(x + 30, y + 3, buf, 2, col, p->text_out);
        }
    } else {
        doomo_snprintf(buf, sizeof(buf), "%d", combat_ammo(combat_ws.ammo_type));
        ui_text_big(x + 30, y + 3, buf, 2, p->text, p->text_out);
    }
    doomo_snprintf(buf, sizeof(buf), "%s %d", doomo_ammo_names[combat_ws.ammo_type],
                   combat_ammo(combat_ws.ammo_type));
    if (combat_ws.mag_size) plat_text(x + 30, y + 17, buf, p->dim);
    else plat_text(x + 30, y + 17, doomo_ammo_names[combat_ws.ammo_type], p->dim);
}

static void health_panel(int x, int y, const HudPal *p)
{
    int hp = doomo_player.hp, low = hp > 0 && hp <= 30;
    int beat = low ? (combat_px.heartbeat % (hp <= 15 ? 18 : 26)) : 99;
    int hs = 5 + (beat < 3 ? 2 : beat < 6 ? 1 : 0);
    face_badge(x, y - 14, p);
    panel(x + 50, y - 2, 52, 30, p);
    if (combat_s.hell >= 128) icon_skull(x + 59, y + 6, hp > 30 ? PAL_RAMP(RAMP_BONE, 6) : PAL_RAMP(RAMP_LAVA, 6), PAL_BLACK);
    else ui_heart(x + 59, y + 6, hs, low ? PAL_UI_RED : PAL_PAINT(PAINT_CHERRY, 1));
    num_big(x + 67, y, DOOMO_MAX(hp, 0), low ? (hud_t & 4 ? p->accent : p->text) : p->text, p->text_out);
    icon_shield(x + 59, y + 19, doomo_player.armor ? PAL_UI_MINT : p->dim, p->outline);
    {
        char buf[8];
        doomo_snprintf(buf, sizeof(buf), "%d", doomo_player.armor);
        ui_text_outline(x + 67, y + 16, buf, doomo_player.armor ? p->text : p->dim, p->text_out);
    }
}

static void boss_bar(void)
{
    const DoomoActor *b;
    int sw = plat_screen_w(), w = sw - 60, x = 30, y = 14, max;
    const char *name;
    if (combat_s.boss < 0) return;
    b = &doomo_actors[combat_s.boss];
    if (!(b->flags & AF_BOSS) || b->state == AST_IDLE || b->state == AST_DORMANT) return;
    max = combat_s.boss_max > 0 ? combat_s.boss_max : 1;
    name = combat_demon_name(b->type);
    ui_text_outline(sw / 2 - ui_text_width(name) / 2, y - 10, name, PAL_RAMP(RAMP_BONE, 7), PAL_RAMP(RAMP_BLOOD, 0));
    ui_bar(x, y, w, 7, DOOMO_MAX(b->hp, 0), max, b->type == THING_LORD_VAPULA ? PAL_UI_PURPLE : PAL_UI_RED);
    if (b->type == THING_LORD_VAPULA) {
        plat_rect(x + w / 3, y, 1, 7, PAL_RAMP(RAMP_BONE, 7));
        plat_rect(x + w * 2 / 3, y, 1, 7, PAL_RAMP(RAMP_BONE, 7));
    }
}

static int boss_visible(void)
{
    const DoomoActor *b;
    if (combat_s.boss < 0) return 0;
    b = &doomo_actors[combat_s.boss];
    return (b->flags & AF_BOSS) && b->state != AST_IDLE && b->state != AST_DORMANT;
}

static void top_left(const HudPal *p)
{
    int x = 4, y = 4;
    if (doomo_player.keys & 1) { icon_key(x, y, PAL_UI_RED); x += 11; }
    if (doomo_player.keys & 2) { icon_key(x, y, PAL_PAINT(PAINT_COBALT, 1)); x += 11; }
    if (combat_st.ticks < 150 && !boss_visible()) {
        const char *o = story_objective();
        const char *title = map_title(combat_s.map_id);
        int ox = x + 2;
        if (!o || !o[0]) o = combat_s.goal == CGOAL_EXIT ? "Find the exit!" :
                             combat_s.goal == CGOAL_KILL_ALL ? "Defeat every demon!" : "Survive!";
        if (combat_st.ticks < 130 || (combat_st.ticks & 2)) {
            ui_text_outline(ox, y, title, p->text, p->text_out);
            ui_text_outline(ox, y + 9, o, combat_s.hell >= 128 ? PAL_RAMP(RAMP_LAVA, 6) : PAL_UI_GOLD, p->text_out);
        }
    }
}

/* --- S3: the HUD caption slot (SPEC sec.12 C10): in the story's climb of
 * Cloud Nine Spire the captions stashed before the push (SPEC C5,
 * story_ch34.c C34A_SPIRE_CAPS) come up one per quarter of the demons
 * cleared, three seconds each: Vapula talks down the stairs. --- */
static void story_caption(const HudPal *p)
{
    static int8_t shown = -1;
    static uint8_t left;
    int q;
    const char *c;
    if (doomo_town.mode != TOWN_STORY || combat_s.map_id != MAPID_CLOUD_NINE_SPIRE ||
        combat_st.kills_total <= 0) { shown = -1; return; }
    q = DOOMO_MIN(3, combat_st.kills * 4 / combat_st.kills_total);
    if (q != shown) { shown = (int8_t)q; left = 90; }
    if (!left) return;
    left--;
    c = txt_stash_get(q);
    if (c && c[0])
        ui_text_outline(plat_screen_w() / 2 - ui_text_width(c) / 2, plat_screen_h() / 3, c,
                        PAL_RAMP(RAMP_VAPE, 6), p->text_out);
}
/* --- end S3 --- */

static void top_right(const HudPal *p)
{
    char buf[24];
    int sw = plat_screen_w(), y = 4, w;
    doomo_snprintf(buf, sizeof(buf), STR_ICON_SKULL "%d/%d", combat_st.kills, combat_st.kills_total);
    if (combat_s.goal == CGOAL_ENDLESS) doomo_snprintf(buf, sizeof(buf), STR_ICON_STAR "%d", (int)combat_st.score);
    w = ui_text_width(buf);
    ui_text_outline(sw - w - 4, y, buf, p->text, p->text_out);
    y += 10;
    if (pet_combat_buff() > 0) {
        doomo_snprintf(buf, sizeof(buf), "+%d%%", DOOMO_MIN(pet_combat_buff(), 50));
        w = ui_text_width(buf);
        icon_paw(sw - w - 14, y + 4, PAL_UI_LCD, PAL_RAMP(RAMP_GRASS, 1));
        ui_text_outline(sw - w - 4, y, buf, PAL_UI_LCD, PAL_RAMP(RAMP_GRASS, 0));
        y += 11;
    }
    if (combat_px.combo >= 3 && combat_px.combo_t > 0) {
        int s = ui_pop_scale(90 - combat_px.combo_t);
        doomo_snprintf(buf, sizeof(buf), "x%d COMBO!", combat_px.combo);
        w = ui_text_width(buf);
        ui_text_outline(sw - w - 4 - (s > 256 ? 2 : 0), y + ui_wave(hud_t, 10, 1), buf,
                        combat_s.hell >= 128 ? PAL_RAMP(RAMP_LAVA, 7) : PAL_PAINT(PAINT_BUBBLEGUM, 2), p->text_out);
    }
}

static void toast(const HudPal *p)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), w, x, y;
    if (!combat_px.msg_t || !combat_px.msg) return;
    w = ui_text_width(combat_px.msg) + 12;
    x = sw / 2 - w / 2;
    y = sh / 3 - 6;
    if (combat_px.msg_t > 70) y -= (combat_px.msg_t - 70) * 2;
    panel(x, y, w, 13, p);
    ui_text(x + 6, y + 3, combat_px.msg, p->text);
}

/* ---- classic status bar ------------------------------------------------------------------ */
static void classic_bar(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), y = sh - 28, x;
    int tex = combat_s.hell >= 128 ? TEX_RUST_METAL : TEX_CANDY_STRIPE;
    const DoomoImage *t = texture_image(tex, 0);
    const DoomoImage *f = face_crop(8, 26);
    char buf[16];
    uint8_t tc = PAL_UI_RED, out = PAL_BLACK;
    if (t && t->pix) {
        bar_img.w = t->w;
        bar_img.h = 28;
        bar_img.pix = t->pix;
        bar_img.uid = doomo_hash32(t->uid ^ 0xBA770000u);
        for (x = 0; x < sw; x += 64) plat_image(x, y, &bar_img, 1, 0, 0);
    } else {
        plat_rect(0, y, sw, 28, PAL_GRAY_D);
    }
    plat_shade_rect(0, y, sw, 28, 9);
    plat_rect(0, y, sw, 1, PAL_GRAY_L);
    /* ammo | health | arms | face | armor | keys */
    if (combat_ws.ammo_type != 0xFF) {
        doomo_snprintf(buf, sizeof(buf), "%d", combat_ws.mag_size ? combat_px.mag[doomo_player.weapon_slot]
                                                                 : combat_ammo(combat_ws.ammo_type));
        ui_text_big(6, y + 4, buf, 2, tc, out);
    }
    plat_text(6, y + 19, "AMMO", PAL_GRAY_L);
    doomo_snprintf(buf, sizeof(buf), "%d%%", DOOMO_MAX(doomo_player.hp, 0));
    ui_text_big(sw / 2 - 86 + (sw > 240 ? 0 : 6), y + 4, buf, 2, tc, out);
    {
        const DoomoImage *icon = weld_icon_image(combat_weapon());
        if (icon) plat_image(sw / 2 - 40, y + 6, icon, 1, 0, 0);
    }
    if (f) plat_image(sw / 2 - 12, y + 1, f, 1, 0, 0);
    doomo_snprintf(buf, sizeof(buf), "%d%%", doomo_player.armor);
    ui_text_big(sw / 2 + 42, y + 4, buf, 2, tc, out);
    if (doomo_player.keys & 1) icon_key(sw - 24, y + 6, PAL_UI_RED);
    if (doomo_player.keys & 2) icon_key(sw - 13, y + 6, PAL_PAINT(PAINT_COBALT, 1));
}

/* Options > Performance > Show FPS: frames actually drawn per second of
 * game ticks (DOOMO_TICK_HZ), top-centre. */
static void fps_counter(int sw)
{
    static uint32_t last_tick;
    static uint16_t frames, shown;
    uint32_t now = combat_stats()->ticks;
    char buf[12];
    if (!plat_draw_skipped()) frames++;
    if (now < last_tick || now - last_tick >= DOOMO_TICK_HZ) {
        shown = now >= last_tick && now - last_tick < DOOMO_TICK_HZ * 2
              ? (uint16_t)(frames * DOOMO_TICK_HZ / (now - last_tick)) : 0;
        frames = 0;
        last_tick = now;
    }
    doomo_snprintf(buf, sizeof(buf), "%u FPS", (unsigned)shown);
    ui_text_outline(sw / 2 - ui_text_width(buf) / 2, 2, buf, PAL_UI_MINT, PAL_BLACK);
}

/* ---- main entry ----------------------------------------------------------------------- */
void combat_draw_hud(void)
{
    HudPal p;
    int sw = plat_screen_w(), sh = plat_screen_h();
    int layout = opt_get(OPT_HUD_LAYOUT);
    hud_t++;
    if (obj_t > 0) obj_t--;
    hud_pal(&p);
    if (doomo_player.camera == CAM_FIRST_PERSON) weapon_view();
    scope_overlay();
    damage_numbers();
    lock_reticle();
    if (doomo_player.hp > 0) crosshair(sw / 2, sh / 2);
    damage_direction();
    boss_bar();
    if (layout == 1) {
        classic_bar();
        top_left(&p);
    } else if (layout == 2) {
        char buf[24];
        doomo_snprintf(buf, sizeof(buf), STR_ICON_HEART "%d " STR_ICON_CROSS "%d", DOOMO_MAX(doomo_player.hp, 0),
                       doomo_player.armor);
        ui_text_outline(4, sh - 12, buf, p.text, p.text_out);
        if (combat_ws.ammo_type != 0xFF) {
            doomo_snprintf(buf, sizeof(buf), STR_ICON_GUN "%d", combat_ws.mag_size ? combat_px.mag[doomo_player.weapon_slot]
                                                                                 : combat_ammo(combat_ws.ammo_type));
            ui_text_outline(sw - ui_text_width(buf) - 4, sh - 12, buf, p.text, p.text_out);
        }
    } else if (layout == 0) {
        health_panel(3, sh - 30, &p);
        ammo_panel(sw - 73, sh - 29, &p);
        top_left(&p);
        top_right(&p);
    }
    story_caption(&p);                           /* S3 */
    toast(&p);
    if (opt_get(OPT_SHOW_FPS)) fps_counter(sw);
}

/* called by combat_build_view() so the HUD projects with the same camera */
void combat_hud_set_view(const DoomoView3D *v) { hud_view = *v; }
