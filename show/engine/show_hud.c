/* show_hud.c - the first-person HUD of the VIEW3D stage kind (owner: show,
 * ENGINE lane 3; spec 4.1, 8.3; PILOT_BEATSHEET 3-03, 3-05).
 *
 * A 32-px strip along the bottom of the picture, drawn with the game's 2D
 * layer and its welded weapon art:
 *   hearts (health) and shells (ammo) | the pet's face in the centre slot,
 *   glancing left and right, grinning at a kill, wincing at a hit, the
 *   horn nub and the glare | the weapon plate (cute name, flipped to the
 *   brutal one) and the tiny plate under it.
 * Above it: the welded weapon in the paws (weld_view_image(), the game's
 * own first-person sprite, CUTE or BRUTAL by the filter), its kick and the
 * local muzzle flash; the GORE: label with the decency sticker slapped
 * over it and blown off; and the lens (splats that slide off on twos).
 * Everything is a pure function of the shot frame t and the record's
 * events, so a seek draws the same HUD.
 */
#include <string.h>
#include "show_host_int.h"
#include "ps1.h"
#include "doomo/weld.h"

typedef struct HudState {
    int on, slam, hearts, hearts_max, shells, shells_max, face, weapon, plate;
    int sticker, sticker_f, mode, horn, horn_f;
    int hurt_f, kill_f, fire_f, swap_f, prev_weapon, heart_pop_f;
} HudState;

static ShowHostHud s_hud;
static int wframe_of(int weapon);

void sh_hud_prep(void)
{
    memset(&s_hud, 0, sizeof(s_hud));
    if (g_sh.hud) s_hud = *g_sh.hud;
    else {
        s_hud.hearts = s_hud.hearts_max = 5;
        s_hud.shells = s_hud.shells_max = 8;
        s_hud.weapon = SWPN_PUMP;
    }
}

/* The weapon's view frames, both filter styles, composed once at prep.
 * weld_gfx keeps only 2 slots (idle + fire of ONE style), so a mid-shot
 * filter flip (Act Three's cute/brutal switch) would compose a new 96x64
 * frame in the flip frame: measured 65+ ms, double the budget. The show
 * keeps its own copies (4 x 6 KB) with a stable uid each, so the flip only
 * changes which image is drawn. Another weapon (a swap) falls back to the
 * game's cache. */
#define HW_N 4
static uint8_t s_hw_pix[HW_N][WELD_VIEW_W * WELD_VIEW_H];
static DoomoImage s_hw_img[HW_N];
static int s_hw_weapon = -1;

static const DoomoImage *hud_weapon_image(const DoomoWeapon *w, int weapon, int brutal, int frame)
{
    if (weapon == s_hw_weapon && frame >= 0 && frame < 2 && s_hw_img[brutal * 2 + frame].pix)
        return &s_hw_img[brutal * 2 + frame];
    return weld_view_image(w, frame);
}

void sh_hud_warm(void)
{
    DoomoWeapon w;
    int st, f;
    const DoomoImage *img;
    s_hw_weapon = -1;
    memset(s_hw_img, 0, sizeof(s_hw_img));
    if (s_hud.weapon == SWPN_NONE) return;
    for (st = 0; st < 2; st++)
        for (f = 0; f < 2; f++) {
            weld_default(&w, wframe_of(s_hud.weapon), st ? WSTYLE_BRUTAL : WSTYLE_CUTE);
            img = weld_view_image(&w, f);
            if (!img || img->w != WELD_VIEW_W || img->h != WELD_VIEW_H) continue;
            memcpy(s_hw_pix[st * 2 + f], img->pix, sizeof(s_hw_pix[0]));
            s_hw_img[st * 2 + f] = *img;
            s_hw_img[st * 2 + f].pix = s_hw_pix[st * 2 + f];
            s_hw_img[st * 2 + f].uid = doomo_image_new_uid();
        }
    s_hw_weapon = s_hud.weapon;
}

/* at prep, inside warm_draw() (uploads synchronous, list discarded): put all
 * four weapon frames in VRAM, and draw the HUD's raster-cached shapes (the
 * hearts, the lens splats and hearts) in BOTH filter styles, so a filter
 * flip's frame renders no new shape (measured: 13 ms of first-use hearts) */
void sh_hud_warm_vram(void)
{
    static const uint8_t blood[3] = { 2, 3, 4 };
    int i, r, saved = g_sh.gore;
    if (s_hw_weapon >= 0)
        for (i = 0; i < HW_N; i++)
            if (s_hw_img[i].pix) plat_image(0, 30, &s_hw_img[i], 1, 0, 0);
    for (r = 0; r <= 9; r++) {
        ui_heart(40, 80, r + 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        ui_heart(40, 80, r + 1, PAL_PAINT(PAINT_CHERRY, 2));
        for (i = 0; i < 3; i++) ui_fill_circle(40, 80, r, PAL_RAMP(RAMP_BLOOD, blood[i]));
    }
    for (i = 0; i < 2; i++) {
        g_sh.gore = i ? SG_BRUTAL : SG_CUTE;
        show_host_draw_hud(0);
    }
    g_sh.gore = saved;
}

/* the HUD's state at t: the record's start, then every event up to t */
static void hud_state(int t, HudState *h)
{
    int i;
    memset(h, 0, sizeof(*h));
    h->on = s_hud.on;
    h->hearts = s_hud.hearts;
    h->hearts_max = s_hud.hearts_max ? s_hud.hearts_max : 5;
    h->shells = s_hud.shells;
    h->shells_max = s_hud.shells_max ? s_hud.shells_max : 8;
    h->face = s_hud.face;
    h->weapon = s_hud.weapon;
    h->prev_weapon = s_hud.weapon;
    h->plate = s_hud.plate;
    h->sticker = s_hud.sticker;
    h->mode = s_hud.mode;
    h->horn = s_hud.horn;
    h->slam = h->hurt_f = h->kill_f = h->fire_f = h->swap_f = h->sticker_f = h->horn_f = -1000;
    h->heart_pop_f = -1000;
    for (i = 0; i < g_sh.nev && g_sh.ev[i].f <= t; i++) {
        const ShowHostEvent *e = &g_sh.ev[i];
        switch (e->op) {
        case SEV_HUD:
            switch (e->a) {
            case SHUD_ON:      h->on = e->b != 0; if (e->b == 2) h->slam = e->f; break;
            case SHUD_HEARTS:  h->hearts = e->b; break;
            case SHUD_SHELLS:  h->shells = e->b; break;
            case SHUD_FACE:    h->face = e->b; break;
            case SHUD_PLATE:   h->plate = e->b; break;
            case SHUD_STICKER: h->sticker = e->b; h->sticker_f = e->f; break;
            case SHUD_MODE:    h->mode = e->b; break;
            case SHUD_HORN:    h->horn = e->b; h->horn_f = e->f; break;
            default: break;
            }
            break;
        case SEV_FIRE:
            h->shells -= e->b > 0 ? e->b : 1;
            if (h->shells < 0) h->shells = 0;
            h->fire_f = e->f;
            break;
        case SEV_HURT:
            if (h->hearts > 0) { h->hearts--; h->heart_pop_f = e->f; }
            h->hurt_f = e->f;
            break;
        case SEV_KILL:
            h->kill_f = e->f;
            break;
        case SEV_WEAPON:
            h->prev_weapon = h->weapon;
            h->weapon = e->a;
            h->swap_f = e->f;
            break;
        default:
            break;
        }
    }
}

/* ---- palettes: the cute pastel HUD, or blood and bone ---------------------------- */
typedef struct HudPal { uint8_t fill, hi, lo, out, text, text_out, accent, dim; } HudPal;

static void hud_pal(int brutal, HudPal *p)
{
    if (brutal) {
        p->fill = PAL_RAMP(RAMP_BLOOD, 1); p->hi = PAL_RAMP(RAMP_BLOOD, 3); p->lo = PAL_RAMP(RAMP_BLOOD, 0);
        p->out = PAL_RAMP(RAMP_BONE, 3); p->text = PAL_RAMP(RAMP_BONE, 7); p->text_out = PAL_RAMP(RAMP_BLOOD, 0);
        p->accent = PAL_RAMP(RAMP_LAVA, 6); p->dim = PAL_RAMP(RAMP_BONE, 4);
    } else {
        p->fill = PAL_UI_PINK; p->hi = PAL_RAMP(RAMP_PASTEL_PINK, 7); p->lo = PAL_RAMP(RAMP_PASTEL_PINK, 4);
        p->out = PAL_UI_PINK_D; p->text = PAL_WHITE; p->text_out = PAL_UI_PINK_D;
        p->accent = PAL_UI_MINT; p->dim = PAL_RAMP(RAMP_PASTEL_PINK, 5);
    }
}

static void panel(int x, int y, int w, int h, const HudPal *p)
{
    ui_round_rect(x, y, w, h, 3, p->out);
    ui_round_rect(x + 1, y + 1, w - 2, h - 2, 2, p->fill);
    plat_rect(x + 3, y + 2, w - 6, 1, p->hi);
    plat_rect(x + 3, y + h - 3, w - 6, 1, p->lo);
}

static void shell_icon(int x, int y, int full, int brutal)
{
    uint8_t body = full ? (brutal ? PAL_RAMP(RAMP_BLOOD, 4) : PAL_PAINT(PAINT_CHERRY, 1)) : PAL_GRAY_D;
    uint8_t brass = full ? PAL_UI_GOLD : PAL_GRAY;
    plat_rect(x, y, 5, 8, PAL_BLACK);
    plat_rect(x + 1, y + 1, 3, 5, body);
    plat_rect(x + 1, y + 6, 3, 2, brass);
    if (full) plat_rect(x + 1, y + 1, 1, 4, ui_lighter(body));
}

/* ---- the pet's face (SPR_PET) ------------------------------------------------------ */
static void face_draw(int cx, int cy, const HudState *h, int t, const HudPal *p)
{
    const DoomoImage *img = sprite_image(SPR_PET, 0);
    int face = h->face, x, y, flip = 0, k;
    /* reactions win for a moment, then the scripted face; idle glances */
    if (t - h->hurt_f < 14) face = SFACE_WINCE;
    else if (t - h->kill_f < 16) face = SFACE_GRIN;
    else if (face == SFACE_IDLE) {
        int ph = (t / 22) & 3;
        face = ph == 1 ? SFACE_LEFT : ph == 3 ? SFACE_RIGHT : SFACE_IDLE;
    }
    ui_round_rect(cx - 17, cy - 14, 34, 29, 4, p->out);
    ui_round_rect(cx - 16, cy - 13, 32, 27, 3, p->lo);
    if (!img) return;
    x = cx - img->w;
    y = cy - img->h + (face == SFACE_WINCE ? 1 : 0);
    if (face == SFACE_LEFT) { x -= 2; flip = 1; }
    if (face == SFACE_RIGHT) x += 2;
    if (h->horn) {
        int age = t - h->horn_f, s = age < 4 && age >= 0 ? age + 1 : 4;
        /* the horn nub pops in over 4 frames */
        for (k = 0; k < s; k++) {
            plat_rect(cx - 6 - k / 2, cy - 12 - k, 2 + (s - k) / 2, 1, PAL_RAMP(RAMP_BLOOD, 3 + (k & 1)));
            plat_rect(cx + 5 - (s - k) / 2 + k / 2, cy - 12 - k, 2 + (s - k) / 2, 1, PAL_RAMP(RAMP_BLOOD, 3 + (k & 1)));
        }
    }
    plat_image(x, y, img, 2, flip ? IMG_FLIPX : 0, 0);
    /* the expression marks over the sprite */
    switch (face) {
    case SFACE_GRIN:
        plat_rect(cx - 5, cy + 5, 11, 1, PAL_BLACK);
        plat_rect(cx - 4, cy + 6, 9, 2, PAL_WHITE);
        plat_rect(cx - 4, cy + 8, 9, 1, PAL_BLACK);
        break;
    case SFACE_WINCE:
        plat_line(cx - 7, cy - 3, cx - 3, cy - 1, PAL_BLACK);
        plat_line(cx - 7, cy + 1, cx - 3, cy - 1, PAL_BLACK);
        plat_line(cx + 7, cy - 3, cx + 3, cy - 1, PAL_BLACK);
        plat_line(cx + 7, cy + 1, cx + 3, cy - 1, PAL_BLACK);
        break;
    case SFACE_GLARE:
        plat_line(cx - 8, cy - 6, cx - 2, cy - 3, PAL_BLACK);
        plat_line(cx + 8, cy - 6, cx + 2, cy - 3, PAL_BLACK);
        break;
    default:
        break;
    }
}

/* ---- the weapon ------------------------------------------------------------------ */
static int wframe_of(int weapon)
{
    switch (weapon) {
    case SWPN_SAW:  return WFRAME_CHAINSAW;
    case SWPN_WAND: return WFRAME_BUBBLE_WAND;
    default:        return WFRAME_PUMP_SHOTGUN;
    }
}

static void muzzle_flash(int mx, int my, int left, int brutal, int t)
{
    int k, r = 4 + left * 2;
    uint8_t c1 = brutal ? PAL_RAMP(RAMP_LAVA, 5) : PAL_PAINT(PAINT_BUBBLEGUM, 1);
    uint8_t c2 = brutal ? PAL_RAMP(RAMP_LAVA, 7) : PAL_PAINT(PAINT_LEMON, 2);
    if (opt_get(OPT_REDUCE_FLASH)) r = r * 2 / 3;
    if (!brutal) {
        ui_star(mx, my, r, c1);
        ui_star(mx, my, r / 2 + 1, c2);
        for (k = 0; k < 3; k++)
            ui_sparkle(mx + ((k * 37 + t * 5) % 21) - 10, my - 4 - ((k * 23 + t * 3) % 12), (t + k * 5) & 15, c2);
        return;
    }
    for (k = 0; k < 6; k++) {
        int a = k * 682 + t * 97;
        ui_fill_circle(mx + FIX2INT(fx_cos(a) * (r + 2)), my + FIX2INT(fx_sin(a) * (r - 1)), 2, c1);
    }
    ui_fill_circle(mx, my, r - 1, c1);
    ui_fill_circle(mx, my, r / 2, c2);
    plat_rect(mx - 1, my - 1, 3, 3, PAL_WHITE);
}

static void weapon_draw(int vx, int vy, int vw, int strip_y, const HudState *h, int t, int brutal)
{
    DoomoWeapon w;
    const DoomoImage *img;
    int flash = sh_v3d_flash(t), kick = sh_v3d_kick(t), frame = 0, x, y, mx, my, gx, gy, weapon = h->weapon;
    int dip = 0, age = t - h->swap_f;
    (void)vy;
    if (weapon == SWPN_NONE) return;
    if (age >= 0 && age < 16) {
        /* the slot spins: down with the old one, up with the new one */
        if (age < 8) { weapon = h->prev_weapon; dip = age * 9; }
        else dip = (16 - age) * 9;
        if (weapon == SWPN_NONE) return;
    }
    weld_default(&w, wframe_of(weapon), brutal ? WSTYLE_BRUTAL : WSTYLE_CUTE);
    if (flash > 1) frame = 1;
    img = hud_weapon_image(&w, weapon, brutal, frame);
    if (!img) return;
    weld_view_points(&w, &mx, &my, &gx, &gy);
    x = vx + vw / 2 - WELD_VIEW_W / 2 + 22;
    y = strip_y - WELD_VIEW_H + 6 + kick / 24 + dip;
    /* idle sway, on the shot frame so a seek sways the same */
    x += FIX2INT(fx_sin(t * 61) * 2);
    y += FIX2INT(fx_cos(t * 122) < 0 ? -fx_cos(t * 122) : fx_cos(t * 122)) * 0;
    plat_image(x, y, img, 1, 0, 0);
    if (flash > 0 && weapon != SWPN_SAW)
        muzzle_flash(x + mx, y + my, flash, brutal, t);
}

/* ---- the lens: splats that slide off on twos --------------------------------------- */
static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static void lens_draw(int vx, int vy, int vw, int vh, int t, int brutal)
{
    int i, k;
    for (i = 0; i < g_sh.nev && g_sh.ev[i].f <= t; i++) {
        const ShowHostEvent *e = &g_sh.ev[i];
        int age = t - e->f, n;
        uint32_t seed;
        if (!(e->op == SEV_HURT || (e->op == SEV_KILL && e->b)))
            continue;                       /* b on a kill: it was close: it hits the lens */
        if (age > 70) continue;
        seed = hash32(g_sh.hdr->seed + (uint32_t)i * 7777u);
        n = e->op == SEV_HURT ? 5 : 7;
        for (k = 0; k < n; k++) {
            uint32_t r = hash32(seed + (uint32_t)k * 131u);
            int x = vx + 20 + (int)(r % (uint32_t)(vw - 40));
            int y = vy + 10 + (int)((r >> 9) % (uint32_t)(vh / 2));
            int rad = 3 + (int)((r >> 17) % 5u);
            int slide = (age / 2) * (1 + (int)((r >> 21) & 1));   /* on twos */
            int life = 50 + (int)((r >> 23) % 20u);
            if (age > life) continue;
            if (age > life - 10) rad = rad * (life - age) / 10;
            if (rad <= 0) continue;
            y += slide;
            if (y > vy + vh - 34) continue;
            if (brutal) {
                ui_fill_circle(x, y, rad, PAL_RAMP(RAMP_BLOOD, 2));
                ui_fill_circle(x - 1, y - 1, rad / 2, PAL_RAMP(RAMP_BLOOD, 4));
                plat_rect(x, y + rad, 1, 2 + slide / 3, PAL_RAMP(RAMP_BLOOD, 2));   /* the drip */
            } else {
                ui_heart(x, y, rad + 1, PAL_PAINT(PAINT_BUBBLEGUM, 1));
            }
        }
    }
}

/* ---- the sticker over the GORE: label ---------------------------------------------- */
static void label_and_sticker(int x, int y, const HudState *h, int t, const HudPal *p, int brutal)
{
    char buf[24];
    int age = t - h->sticker_f;
    if (h->mode) {
        doomo_snprintf(buf, sizeof(buf), "GORE: %s", brutal ? "BRUTAL" : "CUTE");
        ui_text_outline(x + 4, y + 3, buf, brutal ? PAL_RAMP(RAMP_LAVA, 7) : PAL_WHITE, p->text_out);
    }
    if (h->sticker == 1 || (h->sticker == 2 && age < 40)) {
        const char *l1 = sh_str(s_hud.str_sticker), *l2 = sh_str(s_hud.str_sticker2);
        int w = 116, hh = 22, sx = x, sy = y - 4, k;
        if (!l1[0]) l1 = "HELLVILE PARK BOARD";
        if (!l2[0]) l2 = "OF DECENCY (approved)";
        if (h->sticker == 1 && age >= 0 && age < 4) {
            /* SLAM: drops in big over 4 frames */
            int s = 4 - age;
            sx -= s * 3; sy -= s * 3; w += s * 6; hh += s * 4;
        }
        if (h->sticker == 2) {
            /* blown off: torn in three, a spray of red, fluttering down the lens */
            for (k = 0; k < 3; k++) {
                int px = sx + k * (w / 3) + (k - 1) * age * 2 + FIX2INT(fx_sin(age * 300 + k * 900) * 3);
                int py = sy + age * (2 + k) + age * age / 30;
                ui_round_rect(px, py, w / 3 - 2, hh - k * 3, 2, PAL_UI_CREAM);
                plat_rect(px + 2, py + 3, w / 3 - 6, 1, PAL_GRAY);
            }
            if (age < 10)
                for (k = 0; k < 8; k++) {
                    uint32_t r = hash32((uint32_t)k * 77u + 5u);
                    ui_fill_circle(sx + (int)(r % (uint32_t)w), sy + hh / 2 + (int)((r >> 8) % 12u) - 6 + age,
                                   2 - age / 5, PAL_RAMP(RAMP_BLOOD, 3));
                }
            return;
        }
        ui_round_rect(sx, sy, w, hh, 3, PAL_GRAY_D);
        ui_round_rect(sx + 1, sy + 1, w - 2, hh - 2, 3, PAL_UI_CREAM);
        ui_text_center(sx + w / 2, sy + 3, l1, PAL_BLACK);
        ui_text_center(sx + w / 2, sy + 12, l2, PAL_UI_RED_D);
    }
}

/* ---- the frame --------------------------------------------------------------------- */
void sh_hud_draw(int t)
{
    const ShowHostWorld *w = g_sh.world;
    HudState h;
    HudPal p;
    int vx = w->vx, vy = w->vy, vw = w->vw, vh = w->vh, brutal = g_sh.gore == SG_BRUTAL;
    int sy, k, slam_off = 0;
    char buf[40];
    if (!vw || !vh) { vx = 0; vy = 30; vw = 320; vh = 180; }
    hud_state(t, &h);
    lens_draw(vx, vy, vw, vh, t, brutal);
    if (!h.on) return;
    hud_pal(brutal, &p);
    sy = vy + vh - 32;
    if (t - h.slam >= 0 && t - h.slam < 4) slam_off = (4 - (t - h.slam)) * 9;   /* slams up in 4 f */
    weapon_draw(vx, vy, vw, sy + slam_off, &h, t, brutal);
    sy += slam_off;
    /* the strip */
    panel(vx, sy, vw, 32, &p);
    /* hearts and shells, left */
    for (k = 0; k < h.hearts_max && k < 8; k++) {
        int full = k < h.hearts;
        ui_heart(vx + 12 + k * 13, sy + 9, 5, full ? (brutal ? PAL_RAMP(RAMP_BLOOD, 4) : PAL_PAINT(PAINT_CHERRY, 1))
                                                    : p.dim);
    }
    if (h.hearts < h.hearts_max && t - h.heart_pop_f >= 0 && t - h.heart_pop_f < 14) {
        int a = t - h.heart_pop_f;
        ui_heart(vx + 12 + h.hearts * 13, sy + 9 - a * 2, 5 + a / 3, PAL_PAINT(PAINT_CHERRY, 2));
    }
    for (k = 0; k < h.shells_max && k < 16; k++)
        shell_icon(vx + 8 + k * 7, sy + 19, k < h.shells, brutal);
    /* the pet in the centre slot */
    face_draw(vx + vw / 2, sy + 16, &h, t, &p);
    /* the weapon plate, right */
    {
        int px = vx + vw / 2 + 22, pw = vw - (px - vx) - 6, age = t - h.swap_f;
        const char *name;
        int wi = h.weapon;
        if (age >= 0 && age < 8) wi = h.prev_weapon;
        name = sh_str(h.plate ? s_hud.str_brutal[wi < SWPN_COUNT ? wi : 0] : s_hud.str_cute[wi < SWPN_COUNT ? wi : 0]);
        if (!name[0]) name = h.plate ? "SLAUGHTER" : "PARTY PUMP";
        ui_round_rect(px, sy + 4, pw, 13, 2, p.out);
        ui_round_rect(px + 1, sy + 5, pw - 2, 11, 2, brutal ? PAL_RAMP(RAMP_STEEL, 2) : PAL_UI_CREAM);
        if (age >= 0 && age < 16) {
            /* the slot spins: the name rolls */
            int off = (age < 8 ? age : 16 - age) * 2;
            ui_text_center(px + pw / 2, sy + 7 - off, name, brutal ? PAL_RAMP(RAMP_BONE, 7) : PAL_UI_PINK_D);
        } else {
            ui_text_center(px + pw / 2, sy + 7, name, brutal ? PAL_RAMP(RAMP_BONE, 7) : PAL_UI_PINK_D);
        }
        name = sh_str(s_hud.str_tiny);
        if (name[0]) ui_text_center(px + pw / 2, sy + 20, name, p.text);
    }
    /* the GORE: label (+ sticker), top left of the picture */
    label_and_sticker(vx + 8, vy + 8, &h, t, &p, brutal);
    (void)buf;
}
